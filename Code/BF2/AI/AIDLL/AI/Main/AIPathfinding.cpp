// Navigation searches, map lifecycle and offline GTS export.

// static init: ELF 0x3AE2B0 (__tcf_0 0x3AE310, _GLOBAL__I 0x3AE340): <iostream>'s Init object only.
#include <iostream>

#include "AIPathfinding.h"

#include <algorithm>     // std::remove - deleteMap uses the erase-remove idiom
#include <cmath>       // traceKnown's line length
#include <cstddef>
#include <sstream>
#include "../../../../Core/Debug.h"
#include "../../../../IO/File/File.h"
#include "../../../../IO/File/FileManager.h"

#include "../../../AIInterface/BFKnowledge/BFGame.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../AIUtils/DebugRendering/AIRenderUtil.h"
#include "../DrawPrimitives/DPManager.h"
#include "../AIPathFinding/NavMesh/MeshImporterExporter.h"
#include "../AIBot/BotNavigation.h"                       // trace(BotNavigation*): getMapIndex
#include "../AIPathFinding/Cluster/ClusterGraph.h"          // areNeighbours, updatePortalCosts
#include "../AIPathFinding/Cluster/ClusterNodeManager.h"   // init builds one per cluster search
#include "../AIPathFinding/Material/MaterialManager.h"     // traceKnown: isValidMaterial
#include "../AIPathFinding/NavMesh/QuadTree.h"             // MapContainer polygon vertex query
#include "../AIPathFinding/NavMesh/QTSearchNodeManager.h"  // init builds one per local search
#include "../AIStrategicEntity/AIStrategicObjectManager.h" // loadQuadTrees' tail calls initAreas
#include "../AIUtils/checkPointer.h"
#include "AIMineManager.h"               // loadQuadTrees / loadGTSData call IAIMineManager::init
#include "AISettingsInterfaces.h"        // IAISettings::getVehicle / getVehicleMaterialManager
#include "CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

typedef PathfindingEngine<QTSearchNode, QTSearchNodeManager, QTSearchNode,
                          unsigned short, QTPath> LocalEngine;
typedef PathfindingEngine<ClusterNode, ClusterNodeManager, ClusterNode,
                          unsigned short, ClusterNodePath> ClusterEngine;

// The pool's records. Both element types are attested in full by the `_M_insert_aux` symbols the
// two `push_back`s in `init` call.
typedef AStarResource<QTSearchNode, QTSearchNodeManager, QTSearchNode,
                      unsigned short, QTPath> LocalResource;
typedef AStarResource<ClusterNode, ClusterNodeManager, ClusterNode,
                      unsigned short, ClusterNodePath> ClusterResource;


// Allocates the pathfinding engines, search resources and debug paths.
// ELF 0x3A1DC0, 0x3A2630 (compiler-emitted variants).
AIPathfinding::AIPathfinding()
    : m_activatePathfindingDebugTool(false)
    , m_pathfindingDebugToolVehicleId(0)
    , m_searchPointToSet(0)
    , m_debugPath(0)
    , m_debugClusterPath(0)
    , m_d36(-1000.0)
    , m_clusterSearch(false)
    , m_engine(0)
    , m_clusterEngine(0)
    , m_activeMap(0)
    , m_traceMapIndex(-666)
    , m_nextDestroyableMapPartId(0)
    , m_maxWaterDepth(0.5f)
    , m_locking(false)
{
    m_debugPath = new QTPath();
    checkPointer(m_debugPath);

    m_debugClusterPath = new ClusterNodePath();
    checkPointer(m_debugClusterPath);

    m_engine = new LocalEngine();
    checkPointer(m_engine);

    m_clusterEngine = new ClusterEngine();
    checkPointer(m_clusterEngine);

    // `new Vec3` with the parentheses. `Vec3` is a POD in this tree and the binary zeroes all
    // three floats after the allocation, which is value-initialisation, not default.
    m_searchPoint[0] = new Vec3();
    checkPointer(m_searchPoint[0]);

    m_searchPoint[1] = new Vec3();
    checkPointer(m_searchPoint[1]);
}

// Resets debug paths before deleting engines, paths, maps, and search points.
// Preserves the explicit null check before each map deletion.
// ELF 0x3A2EA0, 0x3A3060, 0x3A3220 (compiler-emitted variants).
AIPathfinding::~AIPathfinding()
{
    reset();

    delete m_engine;
    delete m_clusterEngine;
    delete m_debugPath;
    delete m_debugClusterPath;
    delete m_searchPoint[0];
    delete m_searchPoint[1];

    for (std::vector<MapContainer*>::iterator i = m_maps.begin(); i != m_maps.end(); ++i)
    {
        if (*i)
            delete *i;
    }
}

// Resets pathfinding debug state. Loaded maps and in-flight searches remain active.
// ELF 0x3A4120.
int AIPathfinding::reset()
{
    m_activatePathfindingDebugTool = false;
    m_d36 = -1000.0;
    m_searchPointToSet = 0;

    m_debugPath->m_nodes.resize(0);
    m_debugPath->m_i0 = 0;
    m_debugPath->m_i20 = 0;

    m_debugClusterPath->m_nodes.resize(0);
    m_debugClusterPath->m_i0 = 0;

    m_locking = false;
    return 1;
}

// Requests a cooperative pause. Poll isLockComplete() until both search pools are idle.
// This is not a thread synchronization primitive.
// ELF 0x3A4260.
void AIPathfinding::lock()
{
    m_locking = true;
}

// ELF 0x3A4270.
void AIPathfinding::unlock()
{
    m_locking = false;
}

// ELF 0x3A4280.
bool AIPathfinding::isLocking() const
{
    return m_locking;
}

// ELF 0x3A4290.
bool AIPathfinding::isLockComplete() const
{
    if (m_engine->m_inFlight.size() != 0)
        return false;
    if (m_clusterEngine->m_inFlight.size() != 0)
        return false;
    return true;
}

// Map-name lookup is case-insensitive. Negative stored IDs are treated as missing.
// ELF 0x3A7350.
int AIPathfinding::getNumberOfMaps() const
{
    return static_cast<int>(m_maps.size());
}

// ELF 0x3A7370.
bool AIPathfinding::isMapUsed(int id) const
{
    if (id < 0)
        return false;
    return static_cast<unsigned int>(m_maps.size()) > static_cast<unsigned int>(id);
}

// Returns the stored map name directly for a nonnegative id; the caller must ensure
// it is below the map count. Negative ids use the fallback name.
// ELF 0x3A7280.
const std::string& AIPathfinding::getMapNameFromId(int id) const
{
    if (id >= 0)
        return m_maps[id]->m_name;

    m_returnedName.assign("No Map");
    return m_returnedName;
}

// ELF 0x3A72F0.
int AIPathfinding::getMapIdFromName(const std::string& name) const
{
    const std::map<std::string, int, NoCaseStringCompare>::const_iterator i = m_mapIds.find(name);
    if (i == m_mapIds.end())
        return -1;
    return i->second;
}

// ELF 0x3A6CD0.
void AIPathfinding::setActiveMap(const std::string& name)
{
    const std::map<std::string, int, NoCaseStringCompare>::const_iterator i = m_mapIds.find(name);
    if (i == m_mapIds.end() || i->second < 0)
        m_activeMap = 0;
    else
        m_activeMap = m_maps[i->second];
}

// ELF 0x3A6D40.
const std::string& AIPathfinding::getActiveMap() const
{
    if (m_activeMap)
        return m_activeMap->getName();

    m_returnedName.assign("* NO ACTIVE MAPPROPERTIES *");
    return m_returnedName;
}

// 666 is the "NO TRACE MAP" sentinel, not -1, and it is written here on every miss.
// ELF 0x3A6DB0.
void AIPathfinding::setTraceMap(const std::string& name)
{
    const std::map<std::string, int, NoCaseStringCompare>::const_iterator i = m_mapIds.find(name);
    if (i == m_mapIds.end() || i->second < 0)
        m_traceMapIndex = -666;
    else
        m_traceMapIndex = i->second;
}

// the literal has no space before its closing star - "* NO ACTIVE TRACE MAP*" - unlike its
// neighbour above. Copied exactly; it is a shipped string a console user can see.
// ELF 0x3A6E10.
const std::string& AIPathfinding::getTraceMap() const
{
    if (m_traceMapIndex == -666)
    {
        m_returnedName.assign("* NO ACTIVE TRACE MAP*");
        return m_returnedName;
    }
    return m_maps[m_traceMapIndex]->getName();
}

// ELF 0x3B1CB0.
int AIPathfinding::getTraceMapIndex() const
{
    return m_traceMapIndex;
}

// ELF 0x3A6E80.
IAIPathfindingSetup::IMapContainer* AIPathfinding::getActiveMapContainer() const
{
    return m_activeMap;
}

// no bounds check and no guard - it walks whatever is in the list and calls the non-virtual
// `MapContainer::init` on each.
// ELF 0x3A40D0.
void AIPathfinding::initMaps()
{
    for (std::vector<MapContainer*>::iterator i = m_maps.begin(); i != m_maps.end(); ++i)
        (*i)->init();
}

// Debug vehicle selection uses the vehicle IDs and names in AI settings.
// ELF 0x3A6810.
bool AIPathfinding::getActivatePathfindingDebugTool() const
{
    return m_activatePathfindingDebugTool;
}

// ELF 0x3A53A0.
void AIPathfinding::setPathfindingDebugToolVehicle(const std::string& name)
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    m_pathfindingDebugToolVehicleId = settings->getVehicle(name);
}

// ELF 0x3A53E0.
const std::string& AIPathfinding::getPathfindingDebugToolVehicle() const
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    return settings->getVehicle(m_pathfindingDebugToolVehicleId);
}

// ELF 0x3A5420.
void AIPathfinding::setPathfindingDebugToolVehicleId(int id)
{
    m_pathfindingDebugToolVehicleId = id;
}

// ELF 0x3A5380.
void AIPathfinding::setClusterSearch(bool on)
{
    m_clusterSearch = on;
}

// ELF 0x3A5390.
bool AIPathfinding::getClusterSearch() const
{
    return m_clusterSearch;
}

// ELF 0x3A6710.
int AIPathfinding::getSearchPointToSet() const
{
    return m_searchPointToSet;
}

// ELF 0x3A6720.
QTPath* AIPathfinding::getDebugPath() const
{
    return m_debugPath;
}

// ELF 0x3A6730.
ClusterNodePath* AIPathfinding::getDebugClusterPath() const
{
    return m_debugClusterPath;
}

// unchecked, and the array has exactly two elements. `searchPointSelected` subscripts it with
// `m_searchPointToSet`, which that function is careful to keep at 0 or 1; nothing here enforces it.
// ELF 0x3A6740.
Vec3* AIPathfinding::getSearchPoint(int index) const
{
    return m_searchPoint[index];
}

// ELF 0x3A09B0.
void AIPathfinding::setMaxWaterDepth(float depth)
{
    m_maxWaterDepth = depth;
}

// ELF 0x3A09A0.
float AIPathfinding::getMaxWaterDepth() const
{
    return m_maxWaterDepth;
}

// Queries navigation using the selected map and vehicle-specific material rules.
// ELF 0x3A79D0.
bool AIPathfinding::getPolygon(const Vec3& position, int map, unsigned int& polygon,
                               unsigned int& quadTree, int vehicleId) const
{
    if (map < 0 || getNumberOfMaps() <= map)
        return false;

    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);

    return m_maps[map]->getPolygon(position, polygon, quadTree, materials);
}

// ELF 0x3A7B00.
void AIPathfinding::getPolygonsInRadius(std::list<unsigned int>& polygons, int map,
                                        unsigned int quadTree, unsigned int polygon,
                                        const Vec3& centre, float radius) const
{
    if (map < 0 || getNumberOfMaps() <= map)
        return;

    m_maps[map]->getPolygonsInRadius(polygons, quadTree, polygon, centre, radius);
}

// ELF 0x3A7B70.
void AIPathfinding::getPolygonsInTriangle(std::list<unsigned int>& polygons, int map,
                                          unsigned int quadTree, unsigned int polygon,
                                          const Vec3& corner0, const Vec3& corner1,
                                          const Vec3& corner2) const
{
    if (map < 0 || getNumberOfMaps() <= map)
        return;

    m_maps[map]->getPolygonsInTriangle(polygons, quadTree, polygon, corner0, corner1, corner2);
}

// no bounds check - see the block above. ELF 0x3A7D50 returns the container's result.
bool AIPathfinding::setMaterial(int map, unsigned int quadTree, unsigned int polygon,
                                unsigned char material) const
{
    return m_maps[map]->setMaterial(quadTree, polygon, material);
}

// no bounds check.
// ELF 0x3A7D00.
bool AIPathfinding::getMaterial(int map, unsigned int quadTree, unsigned int polygon,
                                unsigned char& material) const
{
    return m_maps[map]->getMaterial(quadTree, polygon, material);
}

// no bounds check, and it draws unconditionally in the shipped server build.
// ELF 0x3A7F30.
void AIPathfinding::drawPolygon(int map, unsigned int quadTree, unsigned int polygon,
                                float* colour, float duration) const
{
    m_maps[map]->drawPolygon(quadTree, polygon, colour, duration);
}

// Requires a valid map index. Validation checks the vehicle vector while cost lookup
// indexes the other vector; the caller must also ensure that lookup index is valid.
// ELF 0x3AE200.
bool AIPathfinding::isValidClusterType(int map, unsigned int type) const
{
    return type < m_maps[map]->m_clusterCostVehicles.size();
}

// ELF 0x3A5E10. Acquires a search resource (owner 2000), empties the shared debug path and runs
// the portal-to-portal search with the vehicle of cluster cost type `costType`, updating the
// engine (10 ms budget) until it stops searching. Appends -1 when a polygon is missing or its
// material is invalid for that vehicle, or when the search fails. Retail quirk kept: a search
// that finds its path on the first call appends the straight distance times the first polygon's
// material cost, not the path cost; only a search that needed updates appends the path cost.
// Topology smoothing is switched on for every map but map 0. The resource is released last.
void AIPathfinding::getPortalCost(std::vector<float>& costs, unsigned short costType, int map,
                                  unsigned int island, const Vec3& from,
                                  unsigned int fromPolygon, const Vec3& to,
                                  unsigned int toPolygon,
                                  const std::set<unsigned short>& allowedClusters)
{
    unsigned int resource = 0xFFFFFFFFu;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    QuadTree* const quadTree = m_maps[map]->m_quadTreeManager->getIsland(island);
    acquireResource(resource, 2000);

    const int vehicleId = settings->getVehicle(m_maps[map]->m_clusterCostVehicles[costType]);
    QTPath* const path = m_debugPath;
    path->m_nodes.resize(0);
    path->m_i0 = 0;
    path->m_i20 = 0.0f;
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);

    if (fromPolygon == 0xFFFFFFFFu || toPolygon == 0xFFFFFFFFu)
    {
        costs.push_back(-1.0f);
    }
    else
    {
        const unsigned char fromMaterial = quadTree->getMaterialOfPolygon(fromPolygon);
        if (!materials->isValidMaterial(fromMaterial)
            || !materials->isValidMaterial(quadTree->getMaterialOfPolygon(toPolygon)))
        {
            costs.push_back(-1.0f);
        }
        else
        {
            QTSearchNodeManager* const manager = m_engine->m_searches[resource]->m_nodeManager;
            manager->m_allowedClusters = &allowedClusters;
            if (map != 0)
                manager->m_usePathTopologySmoothing = true;

            if (localSearch(resource, map, island, fromPolygon, from, toPolygon, to, vehicleId,
                            m_debugPath) == astarStatus::kFound)
            {
                const float dx = to.x - from.x;
                const float dy = to.y - from.y;
                const float dz = to.z - from.z;
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                costs.push_back(materials->getMaterialCost(fromMaterial) * distance);
            }
            else
            {
                while (m_engine->m_searches[resource]->m_status == astarStatus::kSearching)
                    m_engine->update(10.0f);
                if (localSearch(resource, map, island, fromPolygon, from, toPolygon, to,
                                vehicleId, m_debugPath) != astarStatus::kFound)
                    m_debugPath->m_i20 = -1.0f;
                costs.push_back(m_debugPath->m_i20);
            }
        }
    }

    releaseResource(resource, 2000);
}

// ELF 0x3A63B0. Runs the portal-to-portal search on the shared debug path (m_debugPath is
// emptied first: resize(0), step and cost zeroed) with the vehicle of cluster cost type
// `costType`. Both polygons must exist and carry a material valid for that vehicle, else the cost
// is -1 (true). The search is limited to `allowedClusters`; topology smoothing is switched on
// for every map but map 0 (retail tests the map index, nothing else). Search status 1 (still
// running) answers false; 2 (found) takes the path cost, or for a zero-cost path (both portals
// on one polygon) the straight distance times the first polygon's material cost; anything else
// is -1. All non-running outcomes answer true.
bool AIPathfinding::getPortalCost(float& cost, unsigned int resource, int map, unsigned int island,
                                  unsigned short costType, const Vec3& from,
                                  unsigned int fromPolygon, const Vec3& to,
                                  unsigned int toPolygon,
                                  const std::set<unsigned short>& allowedClusters)
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    QuadTree* const quadTree = m_maps[map]->m_quadTreeManager->getIsland(island);

    QTPath* const path = m_debugPath;
    path->m_nodes.resize(0);
    path->m_i0 = 0;
    path->m_i20 = 0.0f;

    const int vehicleId = settings->getVehicle(m_maps[map]->m_clusterCostVehicles[costType]);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);

    if (fromPolygon == 0xFFFFFFFFu || toPolygon == 0xFFFFFFFFu)
    {
        cost = -1.0f;
        return true;
    }
    const unsigned char fromMaterial = quadTree->getMaterialOfPolygon(fromPolygon);
    if (!materials->isValidMaterial(fromMaterial)
        || !materials->isValidMaterial(quadTree->getMaterialOfPolygon(toPolygon)))
    {
        cost = -1.0f;
        return true;
    }

    QTSearchNodeManager* const manager = m_engine->m_searches[resource]->m_nodeManager;
    manager->m_allowedClusters = &allowedClusters;
    if (map != 0)
        manager->m_usePathTopologySmoothing = true;

    const int status = localSearch(resource, map, island, fromPolygon, from, toPolygon, to,
                                   vehicleId, m_debugPath);
    if (status == astarStatus::kSearching)
        return false;
    if (status != astarStatus::kFound)
    {
        cost = -1.0f;
        return true;
    }
    if (m_debugPath->m_i20 == 0.0f)
    {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        cost = std::sqrt(dx * dx + dy * dy + dz * dz);
        cost = materials->getMaterialCost(fromMaterial) * cost;
        return true;
    }
    cost = m_debugPath->m_i20;
    return true;
}

// ELF 0x3AE280.
int AIPathfinding::getClusterCostType(int map, unsigned int type) const
{
    return m_maps[map]->m_clusterCostTypes[type];
}

// AIPathfinding::MapContainer - the trivial accessors.
// ELF 0x3B1CC0.
const std::string& AIPathfinding::MapContainer::getName() const
{
    return m_name;
}

// ELF 0x3A08F0.
int AIPathfinding::MapContainer::getMinPolygonsInLeaf() const
{
    return m_minPolygonsInLeaf;
}

// ELF 0x3A0900.
void AIPathfinding::MapContainer::setMinPolygonsInLeaf(int n)
{
    m_minPolygonsInLeaf = n;
}

// ELF 0x3A0910.
int AIPathfinding::MapContainer::getMaxDepthInTree() const
{
    return m_maxDepthInTree;
}

// ELF 0x3A0920.
void AIPathfinding::MapContainer::setMaxDepthInTree(int n)
{
    m_maxDepthInTree = n;
}

// Owns a navigation map with defaults of four polygons per leaf and tree depth eight.
// ELF 0x39FAD0, 0x39FDA0 (compiler-emitted variants).
AIPathfinding::MapContainer::MapContainer(const std::string& name, int id)
    : m_id(id)
    , m_name(name)
    , m_properties(0)
    , m_minPolygonsInLeaf(4)
    , m_maxDepthInTree(8)
    , m_quadTreeManager(0)
{
    m_properties = new MapProperties();
    checkPointer(m_properties);

    m_quadTreeManager = new QuadTreeManager();
    checkPointer(m_quadTreeManager);
}

// Two deletes; the two vectors and the name string are the compiler's own work. The quad-tree
// manager goes first and the properties second, which is the reverse of the order the constructor
// built them - and both are `delete` with a null test the binary spells out even though `delete`
// already handles null.
// ELF 0x3A0070, 0x3A01C0, 0x3A0310 (compiler-emitted variants).
AIPathfinding::MapContainer::~MapContainer()
{
    delete m_quadTreeManager;
    delete m_properties;
}

// Slope setters accept degrees; getters return radians.
// ELF 0x3A0930.
float AIPathfinding::MapContainer::getMaxSlope() const
{
    return m_properties->getMaxSlope();
}

// Converts the supplied slope from degrees to radians. The corresponding getter
// returns radians, so console input and output use different units.
// ELF 0x3A0960.
void AIPathfinding::MapContainer::setMaxSlope(float degrees)
{
    m_properties->setMaxSlope(0.017453292f * degrees);
}

// ELF 0x3A0C50.
void AIPathfinding::MapContainer::setHeadClearance(float clearance)
{
    m_properties->setHeadClearance(clearance);
}

// ELF 0x3A0C80.
float AIPathfinding::MapContainer::getHeadClearance() const
{
    return m_properties->getHeadClearance();
}

// ELF 0x3A0BE0.
void AIPathfinding::MapContainer::setRadius(float radius)
{
    m_properties->setRadius(radius);
}

// ELF 0x3A0BB0.
float AIPathfinding::MapContainer::getRadius() const
{
    return m_properties->getRadius();
}

// ELF 0x3A0C10.
void AIPathfinding::MapContainer::setAllowedHeightDiff(float diff)
{
    m_properties->setAllowedHeightDiff(diff);
}

// A direct member read - see the block above.
// ELF 0x3A0C40.
float AIPathfinding::MapContainer::getAllowedHeightDiff() const
{
    return m_properties->m_allowedHeightDiff;
}

// Forwards map operations to QuadTreeManager. Build/load operations use m_name as the
// file stem for quad-tree and GTS mesh data.
// ELF 0x3A0CB0.
void AIPathfinding::MapContainer::draw() const
{
    m_quadTreeManager->draw();
}

// ELF 0x3A0CE0.
void AIPathfinding::MapContainer::undraw() const
{
    m_quadTreeManager->undraw();
}

// ELF 0x3A0D10.
void AIPathfinding::MapContainer::drawCluster() const
{
    m_quadTreeManager->drawCluster();
}

// ELF 0x3A0D40.
void AIPathfinding::MapContainer::undrawCluster() const
{
    m_quadTreeManager->undrawCluster();
}

// ELF 0x3A0D70.
void AIPathfinding::MapContainer::drawClusterNeighbourIndices(const Vec3& position) const
{
    m_quadTreeManager->drawClusterNeighbourIndices(position);
}

// ELF 0x3A0DA0.
void AIPathfinding::MapContainer::loadGTSDebugMeshes()
{
    m_quadTreeManager->loadGTSDebugMeshes(m_name);
}

// ELF 0x3A0DD0.
void AIPathfinding::MapContainer::buildDebugMeshes()
{
    m_quadTreeManager->buildDebugMeshes(m_name);
}

// ELF 0x3A0E00.
void AIPathfinding::MapContainer::unloadDebugMeshes()
{
    m_quadTreeManager->unloadDebugMeshes();
}

// ELF 0x3A0E30.
void AIPathfinding::MapContainer::buildDebugClusterMeshes()
{
    m_quadTreeManager->buildDebugClusterMeshes(m_name);
}

// ELF 0x3A0E60.
void AIPathfinding::MapContainer::unloadDebugClusterMeshes()
{
    m_quadTreeManager->unloadDebugClusterMeshes();
}

// ELF 0x3A0740.
void AIPathfinding::MapContainer::saveQuadTrees()
{
    m_quadTreeManager->saveQuadTrees(m_name);
}

// Loads navigation data and supplies its dimensions to the mine manager.
// ELF 0x3A0770.
void AIPathfinding::MapContainer::loadQuadTrees()
{
    m_quadTreeManager->loadQuadTrees(m_name);

    const unsigned int islands = m_quadTreeManager->getNumberOfIslands();
    IAIMineManager* const mines =
        static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager);
    mines->init(static_cast<IAIMineManager::MineType>(m_name.compare("Infantry") != 0 ? 1 : 0),
                islands);
}

// ELF 0x3A07F0.
void AIPathfinding::MapContainer::loadGTSData(int a, float b)
{
    // the tree parameters GO in depth-then-leaf order, which is the reverse of the order they are
    // declared and initialised in. From the binary; do not tidy.
    m_quadTreeManager->loadGTSData(m_name, m_maxDepthInTree, m_minPolygonsInLeaf, a, b);

    const unsigned int islands = m_quadTreeManager->getNumberOfIslands();
    IAIMineManager* const mines =
        static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager);
    mines->init(static_cast<IAIMineManager::MineType>(m_name.compare("Infantry") != 0 ? 1 : 0),
                islands);
}

// Appends a vehicle name only. init populates the parallel cost-type vector later.
// ELF 0x3A0880.
void AIPathfinding::MapContainer::addVehicleForClusterCost(const std::string& vehicle)
{
    m_clusterCostVehicles.push_back(vehicle);
}

// ELF 0x3A0E90.
void AIPathfinding::MapContainer::recalculatePortalCosts()
{
    m_quadTreeManager->recalculatePortalCosts(m_id);
}

// ELF 0x3A0470.
void AIPathfinding::MapContainer::init()
{
    // Build the map-local vehicle
    // numbering used by cluster costs. The settings pointer is deliberately unguarded.
    IAISettings* const settings =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);

    std::map<int, int> vehicleIndices;
    int vehicleIndex = 0;
    for (std::vector<std::string>::iterator i = m_clusterCostVehicles.begin();
         i != m_clusterCostVehicles.end(); ++i)
    {
        // insert makes duplicate global ids first-wins; the sequential index still advances.
        vehicleIndices.insert(std::make_pair(settings->getVehicle(*i), vehicleIndex));
        ++vehicleIndex;
    }

    const std::vector<int>& settingsClusterCostTypes = settings->getClusterCostTypes();
    m_clusterCostTypes.resize(settingsClusterCostTypes.size());
    for (unsigned int i = 0; i < m_clusterCostTypes.size(); ++i)
    {
        // Retail dereferences find without comparing it to end. Preserve that missing guard.
        m_clusterCostTypes[i] = vehicleIndices.find(settingsClusterCostTypes[i])->second;
    }
}

// ELF 0x3A0F30. A position is valid when it lies on a polygon; unless `ignoreMaterial` is set, the
// polygon's material must also be readable and not 5. [INFERRED] name of the flag.
bool AIPathfinding::MapContainer::isValidPosition(const Vec3& position, MaterialManager* materials,
                                                  bool ignoreMaterial) const
{
    unsigned int polygon;
    const unsigned int island = m_quadTreeManager->findPolygon(
        position, polygon, materials, m_properties->m_allowedHeightDiff);
    bool valid = false;
    if (island != 0xFFFFFFFFu && polygon != 0xFFFFFFFFu)
    {
        valid = true;
        if (!ignoreMaterial)
        {
            unsigned char material;
            if (!getMaterial(island, polygon, material))
                return false;
            if (material == 5)
                return false;
        }
    }
    return valid;
}

// ELF 0x3A0FD0. The island is given; only that island's quad tree is searched. The return value of
// QuadTree::findPolygon and the polygon output are both tested.
bool AIPathfinding::MapContainer::isValidPosition(const Vec3& position, MaterialManager* materials,
                                                  unsigned int island, bool ignoreMaterial) const
{
    unsigned int polygon;
    const bool found = m_quadTreeManager->getIsland(island)->findPolygon(
        position, polygon, materials, m_properties->m_allowedHeightDiff);
    bool valid = false;
    if (found && polygon != 0xFFFFFFFFu)
        valid = true;
    if (valid && !ignoreMaterial)
    {
        unsigned char material;
        if (!getMaterial(island, polygon, material))
            return false;
        if (material == 5)
            return false;
    }
    return valid;
}

// ELF 0x3A1090. The known island and polygon seed the manager's search; note the order: the
// first unsigned argument is the island (0x3A10B7..0x3A10C8 pass the second as findPolygon's
// known polygon), the reverse of the getPolygon overload at 0x3A12A0.
bool AIPathfinding::MapContainer::isValidPosition(const Vec3& position, MaterialManager* materials,
                                                  unsigned int knownIsland,
                                                  unsigned int knownPolygon,
                                                  bool ignoreMaterial) const
{
    unsigned int polygon;
    const unsigned int island = m_quadTreeManager->findPolygon(
        position, polygon, knownPolygon, knownIsland, materials,
        m_properties->m_allowedHeightDiff);
    bool valid = false;
    if (island != 0xFFFFFFFFu && polygon != 0xFFFFFFFFu)
    {
        valid = true;
        if (!ignoreMaterial)
        {
            unsigned char material;
            if (!getMaterial(island, polygon, material))
                return false;
            if (material == 5)
                return false;
        }
    }
    return valid;
}

// Resolves polygon then island outputs. Always writes the full-width island result;
// on failure it is UINT_MAX and the polygon output is unchanged. The vertex overload
// appends without clearing, and known ids are validated before reuse.
// ELF 0x3A1140.
bool AIPathfinding::MapContainer::getPolygon(std::vector<const Vec3*>& vertices, const Vec3& position,
                                             unsigned int& polygon, unsigned int& island,
                                             MaterialManager* materials) const
{
    island = m_quadTreeManager->findPolygon(
        position, polygon, materials, m_properties->m_allowedHeightDiff);
    return getPolygon(vertices, polygon, island);
}

// ELF 0x3A11C0.
bool AIPathfinding::MapContainer::getPolygon(std::vector<const Vec3*>& vertices,
                                             unsigned int polygon, unsigned int island) const
{
    if (island == 0xFFFFFFFFu || polygon == 0xFFFFFFFFu)
        return false;

    m_quadTreeManager->getIsland(island)->getVerticiesOfPolygon(vertices, polygon);
    return true;
}

// ELF 0x3A1230.
bool AIPathfinding::MapContainer::getPolygon(const Vec3& position, unsigned int& polygon,
                                             unsigned int& island,
                                             MaterialManager* materials) const
{
    island = m_quadTreeManager->findPolygon(
        position, polygon, materials, m_properties->m_allowedHeightDiff);
    return island != 0xFFFFFFFFu && polygon != 0xFFFFFFFFu;
}

// ELF 0x3A12A0.
bool AIPathfinding::MapContainer::getPolygon(const Vec3& position, unsigned int& polygon,
                                             unsigned int& island, unsigned int knownPolygon,
                                             unsigned int knownIsland,
                                             MaterialManager* materials) const
{
    island = m_quadTreeManager->findPolygon(position, polygon, knownPolygon, knownIsland,
                                            materials, m_properties->m_allowedHeightDiff);
    return island != 0xFFFFFFFFu && polygon != 0xFFFFFFFFu;
}

// ELF 0x3A1320: forwarded unchanged to the quad-tree manager.
void AIPathfinding::MapContainer::getPolygonsInRadius(std::list<unsigned int>& polygons,
                                                      unsigned int quadTree, unsigned int polygon,
                                                      const Vec3& centre, float radius) const
{
    m_quadTreeManager->getPolygonsInRadius(polygons, quadTree, polygon, centre, radius);
}

// ELF 0x3A1370: forwarded unchanged to the quad-tree manager.
void AIPathfinding::MapContainer::getPolygonsInTriangle(std::list<unsigned int>& polygons,
                                                        unsigned int quadTree,
                                                        unsigned int polygon, const Vec3& a,
                                                        const Vec3& b, const Vec3& c) const
{
    m_quadTreeManager->getPolygonsInTriangle(polygons, quadTree, polygon, a, b, c);
}

// ELF 0x3A13D0. Searches only the given island; QuadTree::findPolygon's own result is discarded and
// the polygon output decides. The island reference is read, never written.
bool AIPathfinding::MapContainer::getPolygonFromIsland(const Vec3& position, unsigned int& polygon,
                                                       unsigned int& island,
                                                       MaterialManager* materials) const
{
    const float allowedHeightDiff = m_properties->m_allowedHeightDiff;
    m_quadTreeManager->getIsland(island)->findPolygon(position, polygon, materials,
                                                      allowedHeightDiff);
    return polygon != 0xFFFFFFFFu;
}

// ELF 0x3A1440. The material is zeroed when no polygon is found.
bool AIPathfinding::MapContainer::getMaterial(const Vec3& position, MaterialManager* materials,
                                              unsigned char& material) const
{
    unsigned int polygon;
    const unsigned int island = m_quadTreeManager->findPolygon(
        position, polygon, materials, m_properties->m_allowedHeightDiff);
    if (island == 0xFFFFFFFFu || polygon == 0xFFFFFFFFu)
    {
        material = 0;
        return false;
    }
    material = m_quadTreeManager->getIsland(island)->getMaterialOfPolygon(polygon);
    return true;
}

// UINT_MAX in either index is the only rejected input. Other
// out-of-range values retain retail's unchecked island and polygon indexing.
// ELF 0x3A14E0.
bool AIPathfinding::MapContainer::getMaterial(unsigned int quadTree, unsigned int polygon,
                                               unsigned char& material) const
{
    if (quadTree == 0xFFFFFFFFu || polygon == 0xFFFFFFFFu)
    {
        material = 0;
        return false;
    }

    material = m_quadTreeManager->getIsland(quadTree)->getMaterialOfPolygon(polygon);
    return true;
}

// ELF 0x3A1550. UINT_MAX in either index is rejected; other values index unchecked.
bool AIPathfinding::MapContainer::setMaterial(unsigned int quadTree, unsigned int polygon,
                                              unsigned char material)
{
    if (quadTree == 0xFFFFFFFFu || polygon == 0xFFFFFFFFu)
        return false;
    m_quadTreeManager->getIsland(quadTree)->setMaterialOfPolygon(polygon, material);
    return true;
}

// ELF 0x3A15C0.
void AIPathfinding::MapContainer::getPolygonCentre(Vec3& centre, unsigned int polygon,
                                                   unsigned int island) const
{
    m_quadTreeManager->getIsland(island)->getCentreOfPolygon(centre, polygon);
}

// ELF 0x3A1610. The shared edge of `polygon` towards `neighbour`: its two corners, the vertex at the
// neighbour's index and the next one (wrapping). False when the polygon id is out of range or the
// neighbour is not adjacent.
bool AIPathfinding::MapContainer::getEdge(Vec3& first, Vec3& second, unsigned int polygon,
                                          unsigned int neighbour, unsigned int island) const
{
    const QuadTree* const quadTree = m_quadTreeManager->getIsland(island);
    const Polygon* const record = quadTree->getPolygon(polygon);
    if (record == 0)
        return false;

    unsigned char index = 0;
    if (!quadTree->getNeighbourIndexForPolygon(index, polygon, neighbour))
        return false;

    first = *quadTree->getVertexForPolygon(polygon, index);
    second = *quadTree->getVertexForPolygon(
        polygon, static_cast<unsigned char>((index + 1) % record->m_numberOfVertices));
    return true;
}

// ELF 0x3A1720. Resolves the start polygon from the caller's hints (both written back), then traces
// from that known polygon.
bool AIPathfinding::MapContainer::trace(unsigned int& polygon, unsigned int& island,
                                        const Vec3& from, const Vec3& to, Vec3& end,
                                        unsigned int& endPolygon, int maxSteps, float width,
                                        MaterialManager* materials, unsigned char flags,
                                        unsigned char* materialOut) const
{
    island = m_quadTreeManager->findPolygon(from, polygon, polygon, island, materials,
                                            m_properties->m_allowedHeightDiff);
    if (polygon == 0xFFFFFFFFu || island == 0xFFFFFFFFu)
        return false;
    return traceKnown(polygon, island, from, to, end, endPolygon, maxSteps, width, materials,
                      flags, materialOut);
}

// ELF 0x3A1800. Walks polygon to polygon along the XZ line from `from` towards `to`, starting in
// the known polygon. True when `to` is reached (within 0.05 along the line or 0.025 squared in
// XZ; `end` is then `to`), or when the very first polygon has no exit at all. False on a border,
// after `maxSteps` polygons (-1: no limit) or 30,001, on a material the vehicle cannot use, on
// `stopMaterial` (5 and 6 also stop on 7), or where a non-zero `width` does not fit through an
// edge. `endPolygon` tracks the last polygon entered; `end` the last edge crossing.
// [INFERRED] parameter names.
bool AIPathfinding::MapContainer::traceKnown(unsigned int polygon, unsigned int island,
                                             const Vec3& from, const Vec3& to, Vec3& end,
                                             unsigned int& endPolygon, int maxSteps, float width,
                                             MaterialManager* materials,
                                             unsigned char stopMaterial,
                                             unsigned char* materialOut) const
{
    const QuadTree* const quadTree = m_quadTreeManager->getIsland(island);

    Vec3 direction;
    direction.x = to.x - from.x;
    direction.y = to.y - from.y;
    direction.z = to.z - from.z;
    const float length =
        std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (!(length > 1e-6f))
    {
        endPolygon = polygon;
        return true;
    }
    const float inverse = 1.0f / length;
    direction.x *= inverse;
    direction.y *= inverse;
    direction.z *= inverse;

    end = from;
    unsigned int previousPolygon = 0xFFFFFFFFu;
    endPolygon = polygon;
    int steps = 0;
    for (;;)
    {
        if (polygon == 0xFFFFFFFFu)
            return false;
        if (maxSteps != -1 && steps >= maxSteps)
            return false;
        if (steps > 30000)
            return false;
        endPolygon = polygon;
        ++steps;

        float clearance;
        // [CHOSEN, not recovered] The callee never writes its second float output, so retail
        // compares an uninitialised stack value below. Zero keeps that width test inert.
        float unused = 0.0f;
        unsigned char edge;
        const bool crossed = quadTree->getLineIntersectionOnEdgeInPolygon(
            end, polygon, edge, previousPolygon, from, direction,
            width == 0.0f ? 0 : &clearance, width == 0.0f ? 0 : &unused);
        if (!crossed)
        {
            if (previousPolygon != 0xFFFFFFFFu)
                return false;
            end = to;
            return true;
        }
        previousPolygon = polygon;

        const float dx = end.x - from.x;
        const float dy = end.y - from.y;
        const float dz = end.z - from.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) + 0.05f >= length ||
            0.025f >= distXZSqr(to, end))
        {
            end = to;
            return true;
        }

        if (width != 0.0f && unused > clearance + clearance && width > clearance)
            return false;

        polygon = quadTree->getNeighbourOfPolygon(polygon, edge);
        if (polygon == 0xFFFFFFFFu)
            return false;
        const unsigned char material = quadTree->getMaterialOfPolygon(polygon);
        if (materialOut != 0)
            *materialOut = material;
        if (!materials->isValidMaterial(material))
            return false;
        if (stopMaterial != 0)
        {
            if (material == stopMaterial)
                return false;
            if (static_cast<unsigned char>(stopMaterial - 5) <= 1 && material == 7)
                return false;
        }
    }
}

// ELF 0x3A1B90. Any edge of the polygon (no previous edge, UINT_MAX) and no parameter outputs.
bool AIPathfinding::MapContainer::getLineIntersectionWithPolygon(Vec3& intersection,
                                                                 unsigned int polygon,
                                                                 unsigned int island,
                                                                 const Vec3& from,
                                                                 const Vec3& to) const
{
    unsigned char edge;
    return m_quadTreeManager->getIsland(island)->getLineIntersectionOnEdgeInPolygon(
        intersection, polygon, edge, 0xFFFFFFFFu, from, to, 0, 0);
}

// ELF 0x3A1C10.
void AIPathfinding::MapContainer::drawPolygon(unsigned int island, unsigned int polygon,
                                              float* height, float duration) const
{
    m_quadTreeManager->getIsland(island)->drawPolygon(polygon, height, duration);
}

// Sample `limit` equally spaced points from
// `from` through `to`, returning the first point accepted by the ordinary polygon lookup. Retail
// advances `closest` once more after the final failed sample, so failure can leave it beyond `to`;
// preserve that observable quirk. The output ids are cleared before the zero-limit return, polygon
// first and island second, and receive the successful lookup in that same order.
// ELF 0x3A1C60.
bool AIPathfinding::MapContainer::findClosestValidPolygon(unsigned int& polygon,
                                                          unsigned int& island, Vec3& closest,
                                                          const Vec3& from, const Vec3& to,
                                                          unsigned int limit,
                                                          MaterialManager* materials) const
{
    Vec3 step;
    step.x = to.x - from.x;
    step.y = to.y - from.y;
    step.z = to.z - from.z;

    if (limit > 1)
    {
        const float inverse = 1.0f / static_cast<float>(limit - 1);
        step.x *= inverse;
        step.y *= inverse;
        step.z *= inverse;
    }

    closest = from;
    island = 0xFFFFFFFFu;                    // stores 0x3A1D19 (second) then 0x3A1D1F (first)
    polygon = 0xFFFFFFFFu;

    if (limit == 0)
        return false;

    unsigned int sample = 0;
    unsigned int foundPolygon;
    unsigned int foundIsland;
    while (!getPolygon(closest, foundPolygon, foundIsland, materials))
    {
        ++sample;
        closest.x += step.x;
        closest.y += step.y;
        closest.z += step.z;
        if (sample >= limit)
            return false;
    }

    // 0x3A1D91..0x3A1D9F: the first reference receives getPolygon's polygon, the second its island.
    island = foundIsland;
    polygon = foundPolygon;
    return true;
}

// ELF 0x3A0EC0. The map id is this container's own.
bool AIPathfinding::MapContainer::updateClusterPortalCosts(
    unsigned int& resource, triple<unsigned short, unsigned short, unsigned short>& portal,
    unsigned int island, unsigned short costType)
{
    return m_quadTreeManager->getClusterGraph(island)->updatePortalCosts(resource, portal, m_id,
                                                                        island, costType);
}

// AIPathfinding, the unwritten half

// Resets destroyable map parts in every map's quad-tree manager.
// ELF 0x3A8680.
void AIPathfinding::resetDestroyableMapParts()
{
    for (unsigned int i = 0; i < m_maps.size(); ++i)
        m_maps[i]->m_quadTreeManager->resetDestroyableMapParts();
}

// ELF 0x3A5430. Polls or starts the resource's navigation-mesh search. A failed (3) search reports
// that until the resource is released; a found (2) one hands its path over through
// internalLocalSearch with the polygon hints cleared. A new search needs all three polygon ids;
// otherwise the answer is 4 and nothing starts. Retail fetches the vehicle's material table here
// and discards it.
int AIPathfinding::localSearch(int resource, int map, unsigned int island,
                               unsigned int startPolygon, const Vec3& start,
                               unsigned int goalPolygon, const Vec3& goal, int vehicleId,
                               QTPath* path)
{
    const int status = m_engine->m_searches[resource]->m_status;
    if (status == astarStatus::kFailed || status == astarStatus::kInvalid)
        return status;
    if (status == astarStatus::kFound)
        return internalLocalSearch(resource, map, 0xFFFFFFFFu, 0xFFFFFFFFu, start, 0xFFFFFFFFu,
                                   goal, vehicleId, path);
    if (status == astarStatus::kSearching)
        return status;

    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    settings->getVehicleMaterialManager(vehicleId);
    if (island == 0xFFFFFFFFu || startPolygon == 0xFFFFFFFFu || goalPolygon == 0xFFFFFFFFu)
        return astarStatus::kInvalid;
    return internalLocalSearch(resource, map, island, startPolygon, start, goalPolygon, goal,
                               vehicleId, path);
}

// ELF 0x3A5990. Polls or starts the resource's cluster-graph search. Failed (3) and invalid (4)
// report as such; a found one appends its path when a goal node exists. A new search runs on
// `island`'s cluster graph from the start polygon's cluster to the goal polygon's, with the
// vehicle's cluster cost type, the two positions and emptied open, overflow and closed sets.
// Unlike localSearch the polygon ids are not checked.
int AIPathfinding::clusterSearch(int resource, int map, unsigned int island,
                                 unsigned int startPolygon, const Vec3& start,
                                 unsigned int goalPolygon, const Vec3& goal, int vehicleId,
                                 ClusterNodePath* path)
{
    ClusterResource* search = m_clusterEngine->m_searches[resource];
    const int status = search->m_status;
    if (status == astarStatus::kFailed || status == astarStatus::kInvalid)
        return status;
    if (status == astarStatus::kFound)
    {
        if (search->m_goal != 0)
            search->m_nodeManager->buildPath(*path, search->m_goal);
        return status;
    }
    if (status == astarStatus::kSearching)
        return status;

    ClusterNodeManager* const manager = search->m_nodeManager;
    path->m_island = static_cast<int>(island);
    path->setVehicle(map, vehicleId);

    QuadTree* const quadTree = m_maps[map]->m_quadTreeManager->getIsland(island);
    manager->setQuadTree(quadTree);
    manager->setClusterGraph(m_maps[map]->m_quadTreeManager->getClusterGraph(island));

    ClusterNode* const startNode =
        manager->getNewNode(quadTree->getPolygon(startPolygon)->m_cluster, 0, 0);
    ClusterNode* const goalNode =
        manager->getNewNode(quadTree->getPolygon(goalPolygon)->m_cluster, 0, 0);
    manager->m_start = start;
    manager->m_goalPosition = goal;
    manager->m_costType = m_maps[map]->m_clusterCostTypes[vehicleId];

    search = m_clusterEngine->m_searches[resource];
    search->m_iterations = 0;
    search->m_status = astarStatus::kIdle;
    search->m_worstOpen = 0;
    search->m_open->clear();
    search->m_openOverflow->clear();
    search->m_closed->clear();
    search->m_nodeManager->reset();

    search = m_clusterEngine->m_searches[resource];
    if (startNode->m_key == goalNode->m_key)
    {
        search->m_nodeManager->recycleNode(goalNode);
        search->m_status = astarStatus::kFound;
        search->m_goal = startNode;
        return search->m_status;
    }

    search->m_nodeManager->setGoal(goalNode);
    search->pushOpenNode(startNode);
    search->m_status = astarStatus::kSearching;
    return search->m_status;
}

// ELF 0x3A5550. While the resource is searching it only reports; once found it appends the path
// (when a goal node exists) and reports. Otherwise it starts a new search: the quad tree of
// `island`, a start and a goal node, the vehicle's material table, and emptied open, overflow and
// closed sets. A start already on the goal polygon is found at once; else the goal is stored and
// the start opened.
int AIPathfinding::internalLocalSearch(int resource, int map, unsigned int island,
                                       unsigned int startPolygon, const Vec3& start,
                                       unsigned int goalPolygon, const Vec3& goal, int vehicleId,
                                       QTPath* path)
{
    LocalResource* search = m_engine->m_searches[resource];
    if (search->m_status == astarStatus::kSearching || search->m_status == astarStatus::kFound)
    {
        search = m_engine->m_searches[resource];
        if (search->m_status == astarStatus::kFound && search->m_goal != 0)
            search->m_nodeManager->buildPath(*path, search->m_goal);
        return m_engine->m_searches[resource]->m_status;
    }

    QTSearchNodeManager* const manager = search->m_nodeManager;
    path->m_island = static_cast<int>(island);
    manager->setQuadTree(m_maps[map]->m_quadTreeManager->getIsland(island));

    QTSearchNode* const startNode = manager->getNewNode(startPolygon, 0, 0);
    startNode->m_position = start;
    startNode->m_g = 0.0f;
    QTSearchNode* const goalNode = manager->getNewNode(goalPolygon, 0, 0);
    goalNode->m_position = goal;

    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    manager->m_materials = settings->getVehicleMaterialManager(vehicleId);

    search = m_engine->m_searches[resource];
    search->m_iterations = 0;
    search->m_status = astarStatus::kIdle;
    search->m_worstOpen = 0;
    search->m_open->clear();
    search->m_openOverflow->clear();
    search->m_closed->clear();
    search->m_nodeManager->reset();

    search = m_engine->m_searches[resource];
    if (startNode->m_key == goalNode->m_key)
    {
        search->m_nodeManager->recycleNode(goalNode);
        search->m_status = astarStatus::kFound;
        search->m_goal = startNode;
        return search->m_status;
    }

    search->m_nodeManager->setGoal(goalNode);
    search->pushOpenNode(startNode);
    search->m_status = astarStatus::kSearching;
    return search->m_status;
}

// ELF 0x3A70E0. A negative map index means "no map" and every position is valid; the upper bound
// is not checked. The next two overloads share the shape.
bool AIPathfinding::isValidPosition(int map, int vehicleId, const Vec3& position,
                                    bool ignoreMaterial) const
{
    if (map < 0)
        return true;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);
    return m_maps[map]->isValidPosition(position, materials, ignoreMaterial);
}

// ELF 0x3A7160.
bool AIPathfinding::isValidPosition(int map, int vehicleId, const Vec3& position,
                                    unsigned int island, bool ignoreMaterial) const
{
    if (map < 0)
        return true;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);
    return m_maps[map]->isValidPosition(position, materials, island, ignoreMaterial);
}

// ELF 0x3A71F0.
bool AIPathfinding::isValidPosition(int map, int vehicleId, const Vec3& position,
                                    unsigned int knownIsland, unsigned int knownPolygon,
                                    bool ignoreMaterial) const
{
    if (map < 0)
        return true;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);
    return m_maps[map]->isValidPosition(position, materials, knownIsland, knownPolygon,
                                        ignoreMaterial);
}

// ELF 0x3A7910. Unlike the Vec3-only overloads, the vertex overloads do not check the map index.
bool AIPathfinding::getPolygon(std::vector<const Vec3*>& vertices, int map,
                               const Vec3& position, int vehicleId) const
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);
    unsigned int polygon;
    unsigned int island;
    return m_maps[map]->getPolygon(vertices, position, polygon, island, materials);
}

// ELF 0x3A78A0.
bool AIPathfinding::getPolygon(std::vector<const Vec3*>& vertices, int map,
                               const Vec3& position, unsigned int& polygon,
                               unsigned int& island, int vehicleId) const
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);
    return m_maps[map]->getPolygon(vertices, position, polygon, island, materials);
}

// ELF 0x3A7980.
bool AIPathfinding::getPolygon(std::vector<const Vec3*>& vertices, int map,
                               unsigned int polygon, unsigned int island) const
{
    return m_maps[map]->getPolygon(vertices, polygon, island);
}

// This is the saved-position variant used by the information
// courier. The two hint values are forwarded unchanged; the lower seeded search decides whether
// they can be reused or whether it must fall back to a normal spatial search. As in the adjacent
// overload, retail leaves both output references untouched when the map index is invalid and does
// not guard the settings singleton.
// ELF 0x3A7A60.
bool AIPathfinding::getPolygon(const Vec3& position, int map, unsigned int& polygon,
                               unsigned int& quadTree, unsigned int knownPolygon,
                               unsigned int knownQuadTree, int vehicleId) const
{
    if (map < 0 || getNumberOfMaps() <= map)
        return false;

    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);

    return m_maps[map]->getPolygon(position, polygon, quadTree, knownPolygon, knownQuadTree,
                                   materials);
}

// ELF 0x3A7BE0. The island is passed by value and handed down by reference to a local copy.
bool AIPathfinding::getPolygonFromIsland(const Vec3& position, int map, unsigned int& polygon,
                                         unsigned int island, int vehicleId) const
{
    if (map < 0 || getNumberOfMaps() <= map)
        return false;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);
    return m_maps[map]->getPolygonFromIsland(position, polygon, island, materials);
}

// ELF 0x3A7C70. An invalid map index returns false and leaves the material untouched.
bool AIPathfinding::getMaterial(const Vec3& position, int map, int vehicleId,
                                unsigned char& material) const
{
    if (map < 0 || getNumberOfMaps() <= map)
        return false;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);
    return m_maps[map]->getMaterial(position, materials, material);
}

// ELF 0x3A7DF0.
void* AIPathfinding::getQuadTree(int map, unsigned int island) const
{
    return m_maps[map]->m_quadTreeManager->getIsland(island);
}

// ELF 0x3A7E30.
void* AIPathfinding::getClusterGraph(int map, unsigned int island) const
{
    return m_maps[map]->m_quadTreeManager->getClusterGraph(island);
}

// ELF 0x3A7DA0.
void AIPathfinding::getPolygonCentre(Vec3& centre, int map, unsigned int polygon,
                                     unsigned int island) const
{
    m_maps[map]->getPolygonCentre(centre, polygon, island);
}

// ELF 0x3A7E70.
bool AIPathfinding::getEdge(Vec3& first, Vec3& second, int map, unsigned int polygon,
                            unsigned int neighbour, unsigned int island) const
{
    return m_maps[map]->getEdge(first, second, polygon, neighbour, island);
}

// ELF 0x3A7ED0.
bool AIPathfinding::getLineIntersectionWithPolygon(Vec3& intersection, int map,
                                                   unsigned int polygon, unsigned int island,
                                                   const Vec3& from, const Vec3& to) const
{
    return m_maps[map]->getLineIntersectionWithPolygon(intersection, polygon, island, from, to);
}

// ELF 0x3A7F80. The polygon's 16-bit cluster id; an out-of-range polygon dereferences null, as in
// retail.
int AIPathfinding::getClusterOfPolygon(int map, unsigned int island, unsigned int polygon) const
{
    return m_maps[map]->m_quadTreeManager->getIsland(island)->getPolygon(polygon)->m_cluster;
}

// ELF 0x3A7FD0.
bool AIPathfinding::areNeighbouringClusters(int map, unsigned int island, unsigned short first,
                                            unsigned short second) const
{
    return m_maps[map]->m_quadTreeManager->getClusterGraph(island)->areNeighbours(first, second);
}

// ELF 0x3A8040.
bool AIPathfinding::updateClusterPortalCosts(unsigned int& resource,
                                             triple<unsigned short, unsigned short,
                                                    unsigned short>& portal, int map,
                                             unsigned int island, unsigned short costType)
{
    return m_maps[map]->updateClusterPortalCosts(resource, portal, island, costType);
}

// ELF 0x3A80A0. No polygon or island hint.
bool AIPathfinding::trace(int map, const Vec3& from, const Vec3& to, Vec3& end,
                          unsigned int& endPolygon, int vehicleId, int maxSteps, float width,
                          unsigned char stopMaterial, unsigned char* materialOut) const
{
    return trace(0xFFFFFFFFu, 0xFFFFFFFFu, map, from, to, end, endPolygon, vehicleId, maxSteps,
                 width, stopMaterial, materialOut);
}

// ELF 0x3A8120. The hints are local copies: the container's resolved ids are not handed back.
// No map-index check.
bool AIPathfinding::trace(unsigned int polygon, unsigned int island, int map, const Vec3& from,
                          const Vec3& to, Vec3& end, unsigned int& endPolygon, int vehicleId,
                          int maxSteps, float width, unsigned char stopMaterial,
                          unsigned char* materialOut) const
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);
    return m_maps[map]->trace(polygon, island, from, to, end, endPolygon, maxSteps, width,
                              materials, stopMaterial, materialOut);
}

namespace {

// [TEMP] BotNavigation::getTraceData (ELF 0x194F10, ai-bot's AIBot/BotNavigation) is declared but
// not recovered yet; request on coord/ai-requests.md. Replace with the direct call once it lands.
void traceDataOf(BotNavigation* navigation, int map, unsigned int& polygon, unsigned int& island,
                 Vec3& from)
{
    navigation->getTraceData(map, polygon, island, from);
}

} // anonymous namespace

// ELF 0x3A81C0. Traces from the bot's current trace start: on the trace map when
// `useTraceMap`, else on the bot's own map (BotNavigation +0x50). The start position, polygon
// and island come from BotNavigation::getTraceData; without a polygon or island the answer is
// false. The trace itself is MapContainer::traceKnown, with the vehicle's material table.
bool AIPathfinding::trace(BotNavigation* navigation, bool useTraceMap, const Vec3& to, Vec3& end,
                          unsigned int& endPolygon, int vehicleId, int maxSteps, float width,
                          unsigned char stopMaterial, unsigned char* materialOut) const
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);   // +0xB0
    const int map = useTraceMap ? m_traceMapIndex : navigation->getMapIndex();
    Vec3 from;
    from.x = 0.0f;
    from.y = 0.0f;
    from.z = 0.0f;
    unsigned int polygon;
    unsigned int island;
    traceDataOf(navigation, map, polygon, island, from);
    if (polygon == 0xFFFFFFFFu || island == 0xFFFFFFFFu)
        return false;
    return m_maps[map]->traceKnown(polygon, island, from, to, end, endPolygon, maxSteps, width,
                                   materials, stopMaterial, materialOut);
}

// The
// vehicle's material table (IAISettings slot 44, +0xB0 in both images), then the non-virtual
// MapContainer::findClosestValidPolygon on `m_maps[map]`.
// unguarded - no sign or bounds test on the map index (getPolygon above has one; this has none)
// so the -666 "no trace map" sentinel would index before the vector. Both images. Retail always has
// a trace map: `aiPathfinding.traceMap Infantry` (AIPathFinding.ai), bound at the foot of this file.
// ELF 0x3A8310.
bool AIPathfinding::findClosestValidPolygon(unsigned int& polygon, unsigned int& island,
                                            Vec3& closest, int map, const Vec3& from,
                                            const Vec3& to, unsigned int limit,
                                            int vehicleId) const
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    MaterialManager* const materials = settings->getVehicleMaterialManager(vehicleId);   // +0xB0
    return m_maps[map]->findClosestValidPolygon(polygon, island, closest, from, to, limit,
                                                materials);
}

// ELF 0x3A73A0. Takes the first free local-search resource for `owner`. Fails (and writes
// UINT_MAX) while map parts are locking, when none is free, or when the first free one is still
// owned. The id moves from the free list to the back of the in-flight list.
bool AIPathfinding::acquireResource(unsigned int& resource, int owner)
{
    if (m_locking)
    {
        resource = 0xFFFFFFFFu;
        return false;
    }

    std::list<unsigned int>& available = m_engine->m_available;
    if (available.empty())
    {
        resource = 0xFFFFFFFFu;
        return false;
    }

    std::list<unsigned int>::iterator first = available.begin();
    resource = *first;
    LocalResource* const search = m_engine->m_searches[resource];
    if (search->m_owner >= 0)
    {
        resource = 0xFFFFFFFFu;
        return false;
    }

    search->m_owner = owner;
    available.erase(first);
    m_engine->m_inFlight.insert(m_engine->m_inFlight.end(), resource);
    return true;
}

// ELF 0x3A7480. Only the owner can release. Clears the search limits, the node manager's per-search
// options and the status, returns the id to the back of the free list and invalidates the
// caller's handle.
void AIPathfinding::releaseResource(unsigned int& resource, int owner)
{
    if (m_engine->m_searches[resource]->m_owner != owner)
        return;

    m_engine->m_searches[resource]->m_owner = -1;
    LocalResource* const search = m_engine->m_searches[resource];
    search->m_maxIterations = 0;
    search->m_maxSearchDepth = -1;
    search->m_nodeManager->returnReset();
    search->m_status = 0;
    search->m_owner = -1;
    m_engine->m_available.insert(m_engine->m_available.end(), resource);
    m_engine->m_inFlight.remove(resource);
    resource = 0xFFFFFFFFu;
}

// ELF 0x3A7560 (the local body on the cluster engine). Takes the first free cluster-search resource for `owner`. Fails (and writes
// UINT_MAX) while map parts are locking, when none is free, or when the first free one is still
// owned. The id moves from the free list to the back of the in-flight list.
bool AIPathfinding::acquireClusterResource(unsigned int& resource, int owner)
{
    if (m_locking)
    {
        resource = 0xFFFFFFFFu;
        return false;
    }

    std::list<unsigned int>& available = m_clusterEngine->m_available;
    if (available.empty())
    {
        resource = 0xFFFFFFFFu;
        return false;
    }

    std::list<unsigned int>::iterator first = available.begin();
    resource = *first;
    ClusterResource* const search = m_clusterEngine->m_searches[resource];
    if (search->m_owner >= 0)
    {
        resource = 0xFFFFFFFFu;
        return false;
    }

    search->m_owner = owner;
    available.erase(first);
    m_clusterEngine->m_inFlight.insert(m_clusterEngine->m_inFlight.end(), resource);
    return true;
}

// ELF 0x3A7640 (the local body on the cluster engine). Only the owner can release. Clears the search limits, the node manager's per-search
// options and the status, returns the id to the back of the free list and invalidates the
// caller's handle.
void AIPathfinding::releaseClusterResource(unsigned int& resource, int owner)
{
    if (m_clusterEngine->m_searches[resource]->m_owner != owner)
        return;

    m_clusterEngine->m_searches[resource]->m_owner = -1;
    ClusterResource* const search = m_clusterEngine->m_searches[resource];
    search->m_maxIterations = 0;
    search->m_maxSearchDepth = -1;
    search->m_nodeManager->returnReset();
    search->m_status = 0;
    search->m_owner = -1;
    m_clusterEngine->m_available.insert(m_clusterEngine->m_available.end(), resource);
    m_clusterEngine->m_inFlight.remove(resource);
    resource = 0xFFFFFFFFu;
}

// ELF 0x3A7720.
void AIPathfinding::setMaxIterationsForResource(unsigned int& resource, unsigned int iterations)
{
    m_engine->m_searches[resource]->m_maxIterations = iterations;
}

// ELF 0x3A7740.
void AIPathfinding::setMaxSearchDepthForResource(unsigned int& resource, unsigned int depth)
{
    m_engine->m_searches[resource]->m_maxSearchDepth = depth;
}

// ELF 0x3A8460. The set is borrowed, not copied; releaseResource clears the pointer.
void AIPathfinding::setAdditionalGoals(unsigned int resource, const std::set<unsigned int>* goals)
{
    m_engine->m_searches[resource]->m_nodeManager->m_additionalGoals = goals;
}

// ELF 0x3A8480. Borrowed as well; releaseResource leaves it set.
void AIPathfinding::setAllowedClusters(unsigned int resource,
                                       const std::set<unsigned short>* clusters)
{
    m_engine->m_searches[resource]->m_nodeManager->m_allowedClusters = clusters;
}

// ELF 0x3A84A0.
void AIPathfinding::setUsePathTopologySmoothing(unsigned int resource, bool use)
{
    m_engine->m_searches[resource]->m_nodeManager->m_usePathTopologySmoothing = use;
}

// ELF 0x3A7760. The local engine's status word; cluster searches have their own.
int AIPathfinding::getStatus(unsigned int resource) const
{
    return m_engine->m_searches[resource]->m_status;
}

// ELF 0x3A7780.
void AIPathfinding::setStatus(unsigned int resource, astar::Status status)
{
    m_engine->m_searches[resource]->m_status = status;
}

// ELF 0x3A77A0. Points the resource's node manager at another quad tree.
void AIPathfinding::updateQuadTree(unsigned int resource, int map, unsigned int island)
{
    QuadTree* const quadTree = m_maps[map]->m_quadTreeManager->getIsland(island);
    m_engine->m_searches[resource]->m_nodeManager->setQuadTree(quadTree);
}

// ELF 0x3A7810. Costs `path` with the resource's node manager, which keeps the material table of
// its last search.
float AIPathfinding::calculateCostOfPath(unsigned int resource, int map, unsigned int island,
                                         const QTPath& path) const
{
    QuadTree* const quadTree = m_maps[map]->m_quadTreeManager->getIsland(island);
    m_engine->m_searches[resource]->m_nodeManager->setQuadTree(quadTree);
    return m_engine->m_searches[resource]->m_nodeManager->calculateCostOfPath(path);
}

// Splits the time budget between cluster and polygon searches by queue length.
// With both queues empty, the cluster share is NaN and no cluster work runs.
// ELF 0x3A8390.
void AIPathfinding::updatePathfinding(double budget)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double deadline = timer->getUpdateTime() + budget;
    const unsigned int clusters = static_cast<unsigned int>(m_clusterEngine->m_inFlight.size());
    const unsigned int polygons = static_cast<unsigned int>(m_engine->m_inFlight.size());
    m_clusterEngine->update(
        budget * (static_cast<double>(clusters) / (static_cast<double>(polygons) + clusters)));
    m_engine->update(deadline - timer->getUpdateTime());
}

// Assigns an ID to a destroyable map part and propagates its lifecycle to all navigation meshes.
// ELF 0x3A84C0.
void AIPathfinding::initializeDestroyableMapPart(int& mapPartId, bool walkable, const Vec3& minCorner,
                                                 const Vec3& maxCorner, const Mat4& transformation)
{
    mapPartId = m_nextDestroyableMapPartId++;
    for (unsigned int i = 0; i < m_maps.size(); ++i)
        m_maps[i]->m_quadTreeManager->initializeDestroyableMapPart(mapPartId, walkable, minCorner,
                                                                  maxCorner, transformation);
}

// ELF 0x3A8560.
void AIPathfinding::destroyMapPart(int mapPartId)
{
    for (unsigned int i = 0; i < m_maps.size(); ++i)
        m_maps[i]->m_quadTreeManager->destroyMapPart(mapPartId);
}

// ELF 0x3A85C0.
void AIPathfinding::restoreMapPart(int mapPartId)
{
    for (unsigned int i = 0; i < m_maps.size(); ++i)
        m_maps[i]->m_quadTreeManager->restoreMapPart(mapPartId);
}

// ELF 0x3A8620.
void AIPathfinding::resetOriginalMaterialOfDestroyableMapPart(int mapPartId)
{
    for (unsigned int i = 0; i < m_maps.size(); ++i)
        m_maps[i]->m_quadTreeManager->resetOriginalMaterialOfDestroyableMapPart(mapPartId);
}

// ELF 0x3A42F0. Selects the two debug endpoints with a two-second click interval.
void AIPathfinding::searchPointSelected(bool resetPoints)
{
    if (!m_activatePathfindingDebugTool)
        return;
    const double nextSelection = m_d36 + 2.0;
    if (nextSelection >=
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime())
        return;
    if (resetPoints || m_searchPointToSet == 2)
    {
        m_d36 = -1000.0;
        m_searchPointToSet = 0;
        m_debugPath->m_nodes.resize(0);
        m_debugPath->m_i0 = 0;
        m_debugPath->m_i20 = 0.0f;
        if (resetPoints)
            return;
    }
    m_d36 = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime();
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    Mat4 camera = {{1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f}};
    environment->getLocalPlayerCameraTransformation(camera);
    const Vec3 origin = {camera.m[12], camera.m[13], camera.m[14]};
    const Vec3 end = {camera.m[8] * 250.0f + origin.x,
                      camera.m[9] * 250.0f + origin.y,
                      camera.m[10] * 250.0f + origin.z};
    if (!environment->collideLineWithWorld(origin, end, 0, m_searchPoint[m_searchPointToSet]))
        return;
    if (!isValidPosition(m_pathfindingDebugToolVehicleId != 3,
                         m_pathfindingDebugToolVehicleId,
                         *m_searchPoint[m_searchPointToSet], true))
        return;
    unsigned int resource = 0xFFFFFFFFu;
    unsigned int fromPolygon, island;
    getPolygon(*m_searchPoint[0], m_pathfindingDebugToolVehicleId != 3,
               fromPolygon, island, m_pathfindingDebugToolVehicleId);
    if (island == 0xFFFFFFFFu || fromPolygon == 0xFFFFFFFFu)
        return;
    if (m_searchPointToSet == 0 && m_clusterSearch)
    {
        QuadTree* const tree = m_maps[m_pathfindingDebugToolVehicleId != 3]
                                  ->m_quadTreeManager->getIsland(island);
        const Polygon* const polygon = tree->getPolygon(fromPolygon);
        const int type = m_maps[m_pathfindingDebugToolVehicleId != 3]
                             ->m_clusterCostTypes[m_pathfindingDebugToolVehicleId];
        m_maps[m_pathfindingDebugToolVehicleId != 3]->m_quadTreeManager
            ->getClusterGraph(island)->drawCluster(polygon->m_cluster, type);
    }
    unsigned int toPolygon;
    // shipped defect: this also queries the second endpoint before the first click completes.
    getPolygonFromIsland(*m_searchPoint[1], m_pathfindingDebugToolVehicleId != 3,
                         toPolygon, island, m_pathfindingDebugToolVehicleId);
    if (toPolygon == 0xFFFFFFFFu)
        return;
    if (m_searchPointToSet == 1)
    {
        const bool acquired = !m_clusterSearch ? acquireResource(resource, 2000)
                                               : acquireClusterResource(resource, 2000);
        if (!acquired)
        {
            m_searchPointToSet = 1;
            return;
        }
    }
    if (++m_searchPointToSet != 2)
        return;
    if (!m_clusterSearch)
    {
        m_debugPath->m_nodes.resize(0);
        m_debugPath->m_i0 = 0;
        m_debugPath->m_i20 = 0.0f;
        if (localSearch(resource, m_pathfindingDebugToolVehicleId != 3, island, fromPolygon,
                        *m_searchPoint[0], toPolygon, *m_searchPoint[1],
                        m_pathfindingDebugToolVehicleId, m_debugPath) != 2)
        {
            while (m_engine->m_searches[resource]->m_status == 1)
                m_engine->update(10.0);
            localSearch(resource, m_pathfindingDebugToolVehicleId != 3, island, fromPolygon,
                        *m_searchPoint[0], toPolygon, *m_searchPoint[1],
                        m_pathfindingDebugToolVehicleId, m_debugPath);
        }
    }
    else
    {
        m_debugClusterPath->m_nodes.resize(0);
        m_debugClusterPath->m_i0 = 0;
        if (clusterSearch(resource, m_pathfindingDebugToolVehicleId != 3, island, fromPolygon,
                          *m_searchPoint[0], toPolygon, *m_searchPoint[1],
                          m_pathfindingDebugToolVehicleId, m_debugClusterPath) != 2)
        {
            while (m_clusterEngine->m_searches[resource]->m_status == 1)
                m_clusterEngine->update(10.0);
            clusterSearch(resource, m_pathfindingDebugToolVehicleId != 3, island, fromPolygon,
                          *m_searchPoint[0], toPolygon, *m_searchPoint[1],
                          m_pathfindingDebugToolVehicleId, m_debugClusterPath);
        }
    }
    if (!m_clusterSearch)
        releaseResource(resource, 2000);
    else
        releaseClusterResource(resource, 2000);
}

// ELF 0x3A4D30. Failed endpoint lookup leaves the acquired debug resource owned, as shipped.
void AIPathfinding::tryPath(const Vec3& from, const Vec3& to)
{
    unsigned int resource = 0xFFFFFFFFu;
    if (!acquireResource(resource, 2000))
        return;
    AIRenderUtil::m_instance->unrendenderPathfindingDebug();
    m_searchPointToSet = 2;
    m_d36 = -1000.0;
    *m_searchPoint[0] = from;
    *m_searchPoint[1] = to;
    m_debugPath->m_nodes.resize(0);
    m_debugPath->m_i0 = 0;
    m_debugPath->m_i20 = 0.0f;
    unsigned int fromPolygon, island, toPolygon;
    getPolygon(*m_searchPoint[0], m_pathfindingDebugToolVehicleId != 3,
               fromPolygon, island, m_pathfindingDebugToolVehicleId);
    if (island == 0xFFFFFFFFu || fromPolygon == 0xFFFFFFFFu)
        return;
    getPolygonFromIsland(*m_searchPoint[1], m_pathfindingDebugToolVehicleId != 3,
                         toPolygon, island, m_pathfindingDebugToolVehicleId);
    if (toPolygon == 0xFFFFFFFFu)
        return;
    m_debugPath->m_nodes.resize(0);
    m_debugPath->m_i0 = 0;
    m_debugPath->m_i20 = 0.0f;
    if (localSearch(resource, m_pathfindingDebugToolVehicleId != 3, island, fromPolygon,
                    *m_searchPoint[0], toPolygon, *m_searchPoint[1],
                    m_pathfindingDebugToolVehicleId, m_debugPath) != 2)
    {
        while (m_engine->m_searches[resource]->m_status == 1)
            m_engine->update(10.0);
        localSearch(resource, m_pathfindingDebugToolVehicleId != 3, island, fromPolygon,
                    *m_searchPoint[0], toPolygon, *m_searchPoint[1],
                    m_pathfindingDebugToolVehicleId, m_debugPath);
    }
    releaseResource(resource, 2000);
}

// ELF 0x3A5060. Clears the local path first even for a cluster search.
void AIPathfinding::tryClusterPath(const Vec3& from, const Vec3& to)
{
    unsigned int resource = 0xFFFFFFFFu;
    if (!acquireClusterResource(resource, 2000))
        return;
    AIRenderUtil::m_instance->unrendenderPathfindingDebug();
    m_searchPointToSet = 2;
    m_d36 = -1000.0;
    *m_searchPoint[0] = from;
    *m_searchPoint[1] = to;
    m_debugPath->m_nodes.resize(0);
    m_debugPath->m_i0 = 0;
    m_debugPath->m_i20 = 0.0f;
    unsigned int fromPolygon, island, toPolygon;
    getPolygon(*m_searchPoint[0], m_pathfindingDebugToolVehicleId != 3,
               fromPolygon, island, m_pathfindingDebugToolVehicleId);
    if (island == 0xFFFFFFFFu || fromPolygon == 0xFFFFFFFFu)
        return;
    getPolygonFromIsland(*m_searchPoint[1], m_pathfindingDebugToolVehicleId != 3,
                         toPolygon, island, m_pathfindingDebugToolVehicleId);
    if (toPolygon == 0xFFFFFFFFu)
        return;
    m_debugClusterPath->m_nodes.resize(0);
    m_debugClusterPath->m_i0 = 0;
    if (clusterSearch(resource, m_pathfindingDebugToolVehicleId != 3, island, fromPolygon,
                      *m_searchPoint[0], toPolygon, *m_searchPoint[1],
                      m_pathfindingDebugToolVehicleId, m_debugClusterPath) != 2)
    {
        while (m_clusterEngine->m_searches[resource]->m_status == 1)
            m_clusterEngine->update(10.0);
        clusterSearch(resource, m_pathfindingDebugToolVehicleId != 3, island, fromPolygon,
                      *m_searchPoint[0], toPolygon, *m_searchPoint[1],
                      m_pathfindingDebugToolVehicleId, m_debugClusterPath);
    }
    releaseClusterResource(resource, 2000);
}

// ELF 0x3A6750. Disabling clears only the polygon path and the endpoint selection.
void AIPathfinding::setActivatePathfindingDebugTool(bool active)
{
    m_activatePathfindingDebugTool = active;
    if (!active)
    {
        m_debugPath->m_nodes.resize(0);
        m_debugPath->m_i0 = 0;
        m_debugPath->m_i20 = 0.0f;
        m_searchPointToSet = 0;
    }
}

// ELF 0x3AE230. Returns the settings vehicle ID for the map's cluster-cost vehicle name.
int AIPathfinding::getEquipmentTypeFromClusterCostType(int map, unsigned short type) const
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
        ->getVehicle(m_maps[map]->m_clusterCostVehicles[type]);
}

// Allocates the search-resource pool: one resource per four bots, with a minimum of two.
// ELF 0x3A33F0.
void AIPathfinding::init()
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);

    for (int i = 0; i < settings->getMaxNBots() || i < 8; i += 4)
    {
        // the navigation-mesh search
        LocalResource* const resource =
            checkPointer(new LocalResource(checkPointer(new QTSearchNodeManager())));
        resource->m_maxIterations = 30000;
        m_engine->m_searches.push_back(resource);

        resource->m_iterations      = 0;
        resource->m_openSize = 21;
        resource->m_hashSize = 1031;
        resource->m_open   = checkPointer(new BinarySearchTree<QTSearchNode, QTSearchNode>(21));
        resource->m_openOverflow =
            checkPointer(new Hash<QTSearchNode, QTSearchNodeManager, QTSearchNode>(1031));
        resource->m_closed  =
            checkPointer(new Hash<QTSearchNode, QTSearchNodeManager, QTSearchNode>(1031));
        resource->m_openOverflow->m_nodeManager = resource->m_nodeManager;
        resource->m_closed->m_nodeManager  = resource->m_nodeManager;

        m_engine->m_available.push_back(
            static_cast<unsigned int>(m_engine->m_searches.size() - 1));

        // the cluster-graph search
        ClusterResource* const clusterResource =
            checkPointer(new ClusterResource(checkPointer(new ClusterNodeManager())));
        clusterResource->m_maxIterations = 5000;
        m_clusterEngine->m_searches.push_back(clusterResource);

        clusterResource->m_iterations      = 0;
        clusterResource->m_openSize = 13;
        clusterResource->m_hashSize = 313;
        clusterResource->m_open   = checkPointer(new BinarySearchTree<ClusterNode, ClusterNode>(13));
        clusterResource->m_openOverflow =
            checkPointer(new Hash<ClusterNode, ClusterNodeManager, ClusterNode>(313));
        clusterResource->m_closed  =
            checkPointer(new Hash<ClusterNode, ClusterNodeManager, ClusterNode>(313));
        clusterResource->m_openOverflow->m_nodeManager = clusterResource->m_nodeManager;
        clusterResource->m_closed->m_nodeManager  = clusterResource->m_nodeManager;

        m_clusterEngine->m_available.push_back(
            static_cast<unsigned int>(m_clusterEngine->m_searches.size() - 1));
    }
}

// Creates and selects a named map. Duplicate names fail and clear the active selection.
// ELF 0x3A6820.
bool AIPathfinding::createMap(const std::string& name)
{
    if (m_mapIds.find(name) != m_mapIds.end())
    {
        m_activeMap = 0;
        return false;
    }

    const int id = static_cast<int>(m_maps.size());

    m_activeMap = new MapContainer(name, id);
    checkPointer(m_activeMap);

    m_mapIds.insert(std::make_pair(name, id));
    m_maps.push_back(m_activeMap);
    return true;
}

// Deletes the active map. The supplied map argument does not select the map to remove.
// ELF 0x3A6BA0.
bool AIPathfinding::deleteMap(const std::string& name)
{
    const std::map<std::string, int, NoCaseStringCompare>::iterator i = m_mapIds.find(name);
    if (i == m_mapIds.end())
        return false;

    m_mapIds.erase(i);

    // `m_activeMap` - see above. Not `m_maps[i->second]`.
    m_maps.erase(std::remove(m_maps.begin(), m_maps.end(), m_activeMap), m_maps.end());

    m_activeMap = 0;
    return true;
}

// ELF 0x3A6FE0.
void AIPathfinding::saveQuadTrees()
{
    for (std::vector<MapContainer*>::iterator i = m_maps.begin(); i != m_maps.end(); ++i)
        (*i)->saveQuadTrees();
}

// Loads quad trees and notifies the strategic layer after loading.
// ELF 0x3A7020.
void AIPathfinding::loadQuadTrees()
{
    for (std::vector<MapContainer*>::iterator i = m_maps.begin(); i != m_maps.end(); ++i)
        (*i)->loadQuadTrees();

    AIStrategicObjectManager::getInstance()->initAreas();
}

// ELF 0x3A7070.
void AIPathfinding::loadGTSData()
{
    int map = 0;
    for (std::vector<MapContainer*>::iterator i = m_maps.begin(); i != m_maps.end(); ++i)
    {
        const float radius = (*i)->getRadius();
        (*i)->loadGTSData(map++, radius);
    }
    AIStrategicObjectManager::getInstance()->initAreas();
}

// ELF 0x3AD260.
bool AIPathfinding::exportGTSMeshes() const
{
    std::list<IAIObjectEnvironment::MeshAndBoundingBox> meshes;
    std::list<IAIObjectEnvironment::WaterMesh> water;
    std::map<const IAIObject*, IAIObjectEnvironment::DestroyableAddOnMesh> addOns;
    std::list<IAIObjectEnvironment::MeshAndBoundingBox> ladders;
    std::pair<Vec2, Vec2> bounds = {};
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    if (!environment->initAiCombatAreas())
        return false;
    const std::string overgrowth = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
                                      ->getLevelPath() + "/overgrowth/OvergrowthCollision.con";
    if (!environment->loadOvergrowthCollision(overgrowth))
        return false;
    const unsigned char layer = static_cast<unsigned char>(environment->getAiCombatAreaLayer());
    if (!environment->getAllStaticCollisionMeshes(meshes, water, addOns, ladders, bounds, layer))
        return false;
    std::vector<Vec3> vertices;
    std::vector<std::pair<int, int>> edges;
    std::list<triple<int, int, int>> faces;
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    terrain->getAllTerrainPolygons(bounds, vertices, edges, faces);
    if (!MeshImporterExporter::exportMesh(vertices, edges, faces, "Ground"))
        return false;
    return createGTSFiles(meshes, water, addOns, ladders, terrain, layer);
}

// ELF 0x3ADA70.
bool AIPathfinding::exportGTSMeshesWithinBoundary(const Vec2& min, const Vec2& max) const
{
    std::list<IAIObjectEnvironment::MeshAndBoundingBox> meshes;
    std::list<IAIObjectEnvironment::WaterMesh> water;
    std::map<const IAIObject*, IAIObjectEnvironment::DestroyableAddOnMesh> addOns;
    std::list<IAIObjectEnvironment::MeshAndBoundingBox> ladders;
    std::pair<Vec2, Vec2> bounds = {};
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    if (!environment->initAiCombatAreas())
        return false;
    const std::string overgrowth = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
                                      ->getLevelPath() + "/overgrowth/OvergrowthCollision.con";
    if (!environment->loadOvergrowthCollision(overgrowth))
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/Main/AIPathfinding.cpp", 2653, "")
            << "Couldn't load OvergrowthCollision.con\nABORTING EXPORT!";
        // shipped defect: if the diagnostic is filtered, export continues.
    }
    const unsigned char layer = static_cast<unsigned char>(environment->getAiCombatAreaLayer());
    if (!environment->getAllStaticCollisionMeshesWithinBoundary(
            min, max, meshes, water, addOns, ladders, bounds, layer))
        return false;
    std::vector<Vec3> vertices;
    std::vector<std::pair<int, int>> edges;
    std::list<triple<int, int, int>> faces;
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    terrain->getAllTerrainPolygonsWithinBoundary(bounds, min, max, vertices, edges, faces);
    if (!MeshImporterExporter::exportMesh(vertices, edges, faces, "Ground"))
        return false;
    return createGTSFiles(meshes, water, addOns, ladders, terrain, layer);
}

// ELF 0x3A86D0. Resets the stream contents and appends a prefix and five-digit decimal index.
void getFileName(std::stringstream& stream, const std::string& prefix, int index)
{
    stream.str("");
    stream << prefix;
    if (index <= 9)
        stream << "0000" << index;
    else if (index <= 99)
        stream << "000" << index;
    else if (index <= 999)
        stream << "00" << index;
    else if (index <= 9999)
        stream << "0" << index;
    else
        stream << index;
}

// ELF 0x3A8860. Exports navigation input meshes and the generator configuration.
bool AIPathfinding::createGTSFiles(
    std::list<IAIObjectEnvironment::MeshAndBoundingBox>& meshes,
    std::list<IAIObjectEnvironment::WaterMesh>& water,
    std::map<const IAIObject*, IAIObjectEnvironment::DestroyableAddOnMesh>& addOns,
    std::list<IAIObjectEnvironment::MeshAndBoundingBox>& ladders,
    IAITerrain* terrain, unsigned char layer) const
{
    bool success = false;
    MapContainer* const infantry = m_maps[0];
    MapContainer* const vehicle = m_maps[1];
    std::stringstream config;
    config << "MaxSlopeInfantry: " << infantry->getMaxSlope() << std::endl;
    config << "MaxSlopeVehicle: " << vehicle->getMaxSlope() << std::endl;
    config << "RadiusInfantry: " << infantry->getRadius() << std::endl;
    config << "RadiusVehicle: " << vehicle->getRadius() << std::endl;
    config << "HeadClearanceInfantry: " << infantry->getHeadClearance() << std::endl;
    config << "HeadClearanceVehicle: " << vehicle->getHeadClearance() << std::endl;
    config << "MaxWaterDepth: " << getMaxWaterDepth() << std::endl;
    config << "WaterLevel: " << terrain->getSeaWaterLevel() << std::endl;
    std::vector<Vec3> infantryPoints;
    static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
        ->getSpawnPointPositions(infantryPoints, layer);
    config << "Infantry_NumberOfKeyPoints: " << infantryPoints.size() << std::endl;
    for (unsigned int i = 0; i < infantryPoints.size(); ++i)
        config << infantryPoints[i].x << " " << infantryPoints[i].y << " "
               << infantryPoints[i].z << std::endl;
    std::vector<Vec3> vehiclePoints;
    static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->getVehicleSpawnerPositions(vehiclePoints, layer);
    config << "Vehicle_NumberOfKeyPoints: " << vehiclePoints.size() << std::endl;
    for (unsigned int i = 0; i < vehiclePoints.size(); ++i)
        config << vehiclePoints[i].x << " " << vehiclePoints[i].y << " "
               << vehiclePoints[i].z << std::endl;
    std::stringstream meshName, boxName, addOnName;
    config << "NumberOfMeshes: " << meshes.size() << std::endl;
    int meshIndex = 0;
    for (std::list<IAIObjectEnvironment::MeshAndBoundingBox>::iterator i = meshes.begin();
         i != meshes.end(); ++i)
    {
        getFileName(meshName, "Object", meshIndex);
        getFileName(boxName, "ObjectBB", meshIndex);
        // shipped defect: each ordinary mesh overwrites previous export failures.
        success = MeshImporterExporter::exportMesh(i->mesh.vertices, i->mesh.edges,
                                                   i->mesh.faces, meshName.str()) &&
                  MeshImporterExporter::exportMesh(i->boundingBox.vertices, i->boundingBox.edges,
                                                   i->boundingBox.faces, boxName.str());
        config << meshName.str() << " " << i->name << std::endl;
        meshName.str("");
        boxName.str("");
        ++meshIndex;
    }
    config << "NumberOfDestructableMeshes: " << addOns.size() << std::endl;
    int addOnIndex = 0;
    for (std::map<const IAIObject*, IAIObjectEnvironment::DestroyableAddOnMesh>::iterator
             i = addOns.begin(); i != addOns.end(); ++i)
    {
        const std::string walkable = i->second.initiallyWalkable ? "IW" : "INW";
        getFileName(addOnName, "ObjectDestr", addOnIndex);
        config << addOnName.str() << " " << walkable << std::endl;
        success = success && MeshImporterExporter::exportMesh(
                                 i->second.mesh.vertices, i->second.mesh.edges,
                                 i->second.mesh.faces, addOnName.str());
        ++addOnIndex;
    }
    std::stringstream waterName;
    config << "NumberOfWaterMeshes: " << water.size() << std::endl;
    int waterIndex = 0;
    for (std::list<IAIObjectEnvironment::WaterMesh>::iterator i = water.begin();
         i != water.end(); ++i)
    {
        getFileName(waterName, "ObjectWaterMesh", waterIndex);
        success = success && MeshImporterExporter::exportMesh(
                                 i->mesh.vertices, i->mesh.edges, i->mesh.faces, waterName.str());
        config << waterName.str() << std::endl;
        ++waterIndex;
        waterName.str("");
    }
    config << "NumberOfLadders: " << ladders.size() << std::endl;
    int ladderIndex = 0;
    for (std::list<IAIObjectEnvironment::MeshAndBoundingBox>::iterator i = ladders.begin();
         i != ladders.end(); ++i)
    {
        getFileName(meshName, "Ladder", ladderIndex);
        getFileName(boxName, "LaddertBB", ladderIndex);
        // shipped defect: ladders also overwrite the accumulated failure status.
        success = MeshImporterExporter::exportMesh(i->mesh.vertices, i->mesh.edges,
                                                   i->mesh.faces, meshName.str()) &&
                  MeshImporterExporter::exportMesh(i->boundingBox.vertices, i->boundingBox.edges,
                                                   i->boundingBox.faces, boxName.str());
        config << meshName.str() << " " << i->name << std::endl;
        meshName.str("");
        boxName.str("");
        ++ladderIndex;
    }
    const std::string levelPath = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
                                      ->getLevelPath();
    bool deleted;
    do
    {
        deleted = false;
        getFileName(meshName, "Object", meshIndex);
        getFileName(boxName, "ObjectBB", meshIndex);
        getFileName(addOnName, "ObjectDestr", addOnIndex);
        if (io::fileManager->fileExists(levelPath + "/GTSData/Meshes/" + meshName.str() + ".gts"))
        {
            io::fileManager->deleteFile(levelPath + "/GTSData/Meshes/" + meshName.str() + ".gts");
            deleted = true;
        }
        if (io::fileManager->fileExists(levelPath + "/GTSData/Meshes/" + boxName.str() + ".gts"))
        {
            io::fileManager->deleteFile(levelPath + "/GTSData/Meshes/" + boxName.str() + ".gts");
            deleted = true;
        }
        if (io::fileManager->fileExists(levelPath + "/GTSData/Meshes/" + addOnName.str() + ".gts"))
        {
            io::fileManager->deleteFile(levelPath + "/GTSData/Meshes/" + addOnName.str() + ".gts");
            deleted = true;
        }
        ++meshIndex;
        ++addOnIndex;
        meshName.str("");
        boxName.str("");
        addOnName.str("");
        getFileName(meshName, "Ladder", ladderIndex);
        getFileName(boxName, "LaddertBB", ladderIndex);
        if (io::fileManager->fileExists(levelPath + "/GTSData/Meshes/" + meshName.str() + ".gts"))
        {
            io::fileManager->deleteFile(levelPath + "/GTSData/Meshes/" + meshName.str() + ".gts");
            deleted = true;
        }
        if (io::fileManager->fileExists(levelPath + "/GTSData/Meshes/" + boxName.str() + ".gts"))
        {
            io::fileManager->deleteFile(levelPath + "/GTSData/Meshes/" + boxName.str() + ".gts");
            deleted = true;
        }
        ++ladderIndex;
        meshName.str("");
        boxName.str("");
    } while (deleted);
    {
        io::File file;
        if (!file.open(levelPath + "/GTSData/Meshes/config.dat",
                       static_cast<io::FileMode>(0), static_cast<io::FileAccess>(1), 0))
            return false;
        file.write(config.str());
        file.close();
    }
    {
        io::File file;
        if (!file.open(levelPath + "/GTSData/Meshes/CombatArea.dat",
                       static_cast<io::FileMode>(0), static_cast<io::FileAccess>(1), 0))
            return false;
        std::stringstream combatArea;
        std::list<Vec2> points;
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
            ->getAICombatArea(points);
        combatArea << points.size() << std::endl;
        for (std::list<Vec2>::iterator i = points.begin(); i != points.end(); ++i)
            combatArea << i->x << " " << i->y << std::endl;
        file.write(combatArea.str());
        file.close();
    }
    return success;
}

// Returns virtual getName results joined by newlines, with no trailing newline.
// ELF 0x3A6E90.
std::string AIPathfinding::getMapList()
{
    std::string list;

    for (std::vector<MapContainer*>::iterator i = m_maps.begin(); i != m_maps.end(); ++i)
    {
        if (!list.empty())
            list.append("\n" + (*i)->getName());
        else
            list.append((*i)->getName());
    }

    return list;
}

// ELF 0x3A09C0. Draws a line between each object's two transformed bounds corners.
void AIPathfinding::drawDynamicAIObjects() const
{
    std::vector<DestroyableObject> objects;
    static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->getAllDestroyableObjects(objects);
    for (std::vector<DestroyableObject>::iterator i = objects.begin(); i != objects.end(); ++i)
    {
        const float* const m = i->transform.m;
        const Vec3 from = {i->min.x * m[0] + i->min.y * m[4] + i->min.z * m[8] + m[12],
                            i->min.x * m[1] + i->min.y * m[5] + i->min.z * m[9] + m[13],
                            i->min.x * m[2] + i->min.y * m[6] + i->min.z * m[10] + m[14]};
        const Vec3 to = {i->max.x * m[0] + i->max.y * m[4] + i->max.z * m[8] + m[12],
                          i->max.x * m[1] + i->max.y * m[5] + i->max.z * m[9] + m[13],
                          i->max.x * m[2] + i->max.y * m[6] + i->max.z * m[10] + m[14]};
        static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStaticLine(60.0f, 0, from, to, 0.05f);
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
