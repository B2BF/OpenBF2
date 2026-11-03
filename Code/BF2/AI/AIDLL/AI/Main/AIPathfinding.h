// Pathfinding queries and map lifecycle through separate query and setup interfaces.
// Use typed interface conversions for the secondary base subobject.

#ifndef AI_AIDLL_AI_MAIN_AIPATHFINDING_H
#define AI_AIDLL_AI_MAIN_AIPATHFINDING_H

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../../../IO/File/IFileSystem.h"     // hfe::NoCaseStringCompare - the map registry's comparator
#include "../AIPathFinding/NavMesh/MapProperties.h"      // MapContainer owns one
#include "../AIPathFinding/NavMesh/QuadTreeManager.h"    // and one of these
#include "AIPathfindingSearchTypes.h"            // QTPath, ClusterNodePath, PathfindingEngine
#include "IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"

namespace dice {
namespace hfe {
namespace ai {

// Types this class only ever holds or forwards pointers to. All owned by other chunks.
class ClusterGraph;
class ClusterNode;
class ClusterNodeManager;
class IAIObject;
class IAITerrain;
class MaterialManager;
class QTSearchNode;
class QTSearchNodeManager;

class AIPathfinding : public IAIPathfinding, public IAIPathfindingSetup
{
public:
    // Owns one navigation mesh, its properties and island manager.
    class MapContainer : public IAIPathfindingSetup::IMapContainer
    {
    public:
        MapContainer(const std::string& name, int id);
        virtual ~MapContainer();

        // IMapContainer, slots 2..29, in vtable order
        virtual const std::string& getName() const;
        virtual int getMinPolygonsInLeaf() const;
        virtual void setMinPolygonsInLeaf(int);
        virtual int getMaxDepthInTree() const;
        virtual void setMaxDepthInTree(int);
        virtual float getMaxSlope() const;
        virtual void setMaxSlope(float);
        virtual void setHeadClearance(float);
        virtual float getHeadClearance() const;
        virtual void setRadius(float);
        virtual float getRadius() const;
        virtual void setAllowedHeightDiff(float);
        virtual float getAllowedHeightDiff() const;
        virtual void draw() const;
        virtual void undraw() const;
        virtual void drawCluster() const;
        virtual void undrawCluster() const;
        virtual void drawClusterNeighbourIndices(const Vec3&) const;
        virtual void loadGTSDebugMeshes();
        virtual void buildDebugMeshes();
        virtual void unloadDebugMeshes();
        virtual void buildDebugClusterMeshes();
        virtual void unloadDebugClusterMeshes();
        virtual void saveQuadTrees();
        virtual void loadQuadTrees();
        virtual void loadGTSData(int, float);
        virtual void addVehicleForClusterCost(const std::string&);
        virtual void recalculatePortalCosts();

        // the non-virtual query surface - what AIPathfinding's forwarders actually call
        // none of these is in the vtable. That is the split: the console drives the virtuals
        // above, the pathfinder calls these directly on a `MapContainer*`.
        void init();
        bool isValidPosition(const Vec3&, MaterialManager *, bool) const;
        bool isValidPosition(const Vec3&, MaterialManager *, unsigned int, bool) const;
        bool isValidPosition(const Vec3&, MaterialManager *, unsigned int, unsigned int, bool) const;
        bool getPolygon(std::vector<const Vec3*>&, const Vec3&, unsigned int&, unsigned int&, MaterialManager *) const;
        bool getPolygon(std::vector<const Vec3*>&, unsigned int, unsigned int) const;
        bool getPolygon(const Vec3&, unsigned int&, unsigned int&, MaterialManager *) const;
        bool getPolygon(const Vec3&, unsigned int&, unsigned int&, unsigned int, unsigned int, MaterialManager *) const;
        void getPolygonsInRadius(std::list<unsigned int>&, unsigned int, unsigned int, const Vec3&, float) const;
        void getPolygonsInTriangle(std::list<unsigned int>&, unsigned int, unsigned int, const Vec3&, const Vec3&, const Vec3&) const;
        bool getPolygonFromIsland(const Vec3&, unsigned int&, unsigned int&, MaterialManager *) const;
        bool getMaterial(const Vec3&, MaterialManager *, unsigned char&) const;
        bool getMaterial(unsigned int, unsigned int, unsigned char&) const;
        bool setMaterial(unsigned int, unsigned int, unsigned char);
        void getPolygonCentre(Vec3&, unsigned int, unsigned int) const;
        bool getEdge(Vec3&, Vec3&, unsigned int, unsigned int, unsigned int) const;
        bool trace(unsigned int&, unsigned int&, const Vec3&, const Vec3&, Vec3&, unsigned int&, int, float, MaterialManager *, unsigned char, unsigned char *) const;
        bool traceKnown(unsigned int, unsigned int, const Vec3&, const Vec3&, Vec3&, unsigned int&, int, float, MaterialManager *, unsigned char, unsigned char *) const;
        bool getLineIntersectionWithPolygon(Vec3&, unsigned int, unsigned int, const Vec3&, const Vec3&) const;
        void drawPolygon(unsigned int, unsigned int, float *, float) const;
        bool findClosestValidPolygon(unsigned int&, unsigned int&, Vec3&, const Vec3&, const Vec3&, unsigned int, MaterialManager *) const;
        bool updateClusterPortalCosts(unsigned int&, triple<unsigned short, unsigned short, unsigned short>&, unsigned int, unsigned short);

        // the layout, from the 720-byte constructor
        /*  +4 */ int m_id;                          // = the caller's index into AIPathfinding::m_maps
        /*  +8 */ std::string m_name;
        /* +12 */ MapProperties* m_properties;       // = new MapProperties
        /* +16 */ int m_minPolygonsInLeaf;           // = 4
        /* +20 */ int m_maxDepthInTree;              // = 8
        /* +24 */ QuadTreeManager* m_quadTreeManager;// = new QuadTreeManager
        // Parallel vehicle-name and cluster-cost-type arrays.
        /* +28 */ std::vector<std::string> m_clusterCostVehicles;   // isValidClusterType's bound
        /* +40 */ std::vector<int> m_clusterCostTypes;              // paired init/read access proves int
    };

    AIPathfinding();
    virtual ~AIPathfinding();

    // IAIPathfinding, slots 2..70 - the query face. Order and constness are the binary's; do not
    // reorder. See IAIPathfinding.h for what const means here.
    virtual int getTraceMapIndex() const;
    virtual int reset();
    virtual void resetDestroyableMapParts();
    virtual void lock();
    virtual void unlock();
    virtual bool isLocking() const;
    virtual bool isLockComplete() const;
    virtual int localSearch(int, int, unsigned int, unsigned int, const Vec3&, unsigned int, const Vec3&, int, QTPath *);
    virtual int clusterSearch(int, int, unsigned int, unsigned int, const Vec3&, unsigned int, const Vec3&, int, ClusterNodePath *);
    virtual bool isValidPosition(int, int, const Vec3&, bool) const;
    virtual bool isValidPosition(int, int, const Vec3&, unsigned int, bool) const;
    virtual bool isValidPosition(int, int, const Vec3&, unsigned int, unsigned int, bool) const;
    virtual bool getPolygon(std::vector<const Vec3*>&, int, const Vec3&, int) const;
    virtual bool getPolygon(std::vector<const Vec3*>&, int, const Vec3&, unsigned int&, unsigned int&, int) const;
    virtual bool getPolygon(std::vector<const Vec3*>&, int, unsigned int, unsigned int) const;
    virtual bool getPolygon(const Vec3&, int, unsigned int&, unsigned int&, int) const;
    virtual bool getPolygon(const Vec3&, int, unsigned int&, unsigned int&, unsigned int, unsigned int, int) const;
    virtual void getPolygonsInRadius(std::list<unsigned int>&, int, unsigned int, unsigned int, const Vec3&, float) const;
    virtual void getPolygonsInTriangle(std::list<unsigned int>&, int, unsigned int, unsigned int, const Vec3&, const Vec3&, const Vec3&) const;
    virtual bool getPolygonFromIsland(const Vec3&, int, unsigned int&, unsigned int, int) const;
    virtual bool getMaterial(const Vec3&, int, int, unsigned char&) const;
    virtual bool getMaterial(int, unsigned int, unsigned int, unsigned char&) const;
    virtual bool setMaterial(int, unsigned int, unsigned int, unsigned char) const;
    virtual void* getQuadTree(int, unsigned int) const;
    virtual void* getClusterGraph(int, unsigned int) const;
    virtual void getPolygonCentre(Vec3&, int, unsigned int, unsigned int) const;
    virtual bool getEdge(Vec3&, Vec3&, int, unsigned int, unsigned int, unsigned int) const;
    virtual bool getLineIntersectionWithPolygon(Vec3&, int, unsigned int, unsigned int, const Vec3&, const Vec3&) const;
    virtual void drawPolygon(int, unsigned int, unsigned int, float *, float) const;
    virtual int getClusterOfPolygon(int, unsigned int, unsigned int) const;
    virtual bool areNeighbouringClusters(int, unsigned int, unsigned short, unsigned short) const;
    virtual bool updateClusterPortalCosts(unsigned int&, triple<unsigned short, unsigned short, unsigned short>&, int, unsigned int, unsigned short);
    virtual bool trace(int, const Vec3&, const Vec3&, Vec3&, unsigned int&, int, int, float, unsigned char, unsigned char *) const;
    virtual bool trace(unsigned int, unsigned int, int, const Vec3&, const Vec3&, Vec3&, unsigned int&, int, int, float, unsigned char, unsigned char *) const;
    virtual bool trace(BotNavigation *, bool, const Vec3&, Vec3&, unsigned int&, int, int, float, unsigned char, unsigned char *) const;
    virtual bool findClosestValidPolygon(unsigned int&, unsigned int&, Vec3&, int, const Vec3&, const Vec3&, unsigned int, int) const;
    virtual bool acquireResource(unsigned int&, int);
    virtual void releaseResource(unsigned int&, int);
    virtual bool acquireClusterResource(unsigned int&, int);
    virtual void releaseClusterResource(unsigned int&, int);
    virtual void setMaxIterationsForResource(unsigned int&, unsigned int);
    virtual void setMaxSearchDepthForResource(unsigned int&, unsigned int);
    virtual void setAdditionalGoals(unsigned int, const std::set<unsigned int>*);
    virtual void setAllowedClusters(unsigned int, const std::set<unsigned short>*);
    virtual void setUsePathTopologySmoothing(unsigned int, bool);
    virtual int getStatus(unsigned int) const;
    virtual void setStatus(unsigned int, astar::Status);
    virtual void updateQuadTree(unsigned int, int, unsigned int);
    virtual float calculateCostOfPath(unsigned int, int, unsigned int, const QTPath&) const;
    virtual const std::string& getMapNameFromId(int) const;
    virtual int getMapIdFromName(const std::string&) const;
    virtual int getNumberOfMaps() const;
    virtual bool isMapUsed(int) const;
    virtual void updatePathfinding(double);
    virtual void initializeDestroyableMapPart(int&, bool, const Vec3&, const Vec3&, const Mat4&);
    virtual void destroyMapPart(int);
    virtual void restoreMapPart(int);
    virtual void resetOriginalMaterialOfDestroyableMapPart(int);
    virtual void searchPointSelected(bool);
    virtual void tryPath(const Vec3&, const Vec3&);
    virtual void tryClusterPath(const Vec3&, const Vec3&);
    virtual void setClusterSearch(bool);
    virtual bool getClusterSearch() const;
    virtual void setActivatePathfindingDebugTool(bool);
    virtual bool getActivatePathfindingDebugTool() const;
    virtual void setPathfindingDebugToolVehicle(const std::string&);
    virtual const std::string& getPathfindingDebugToolVehicle() const;
    virtual void setPathfindingDebugToolVehicleId(int);
    virtual int getClusterCostType(int, unsigned int) const;

    // IAIPathfindingSetup, slots 2..20 - the owner's face. These are the twenty that also occupy
    // primary slots 71..90, because a class that overrides a non-primary base's virtual declares it
    // itself and so gets an entry in both tables. `reset` above is the twenty-first and is shared.
    virtual void init();
    virtual bool createMap(const std::string&);
    virtual bool deleteMap(const std::string&);
    virtual void setActiveMap(const std::string&);
    virtual const std::string& getActiveMap() const;
    virtual void setTraceMap(const std::string&);
    virtual const std::string& getTraceMap() const;
    virtual void saveQuadTrees();
    virtual void loadQuadTrees();
    virtual void loadGTSData();
    virtual bool exportGTSMeshes() const;
    virtual bool exportGTSMeshesWithinBoundary(const Vec2&, const Vec2&) const;
    // Exports level meshes, water, ladders and destroyable add-ons for navigation generation.
    virtual bool createGTSFiles(std::list<IAIObjectEnvironment::MeshAndBoundingBox>&,
                               std::list<IAIObjectEnvironment::WaterMesh>&,
                               std::map<const IAIObject*, IAIObjectEnvironment::DestroyableAddOnMesh>&,
                               std::list<IAIObjectEnvironment::MeshAndBoundingBox>&,
                               IAITerrain*, unsigned char) const;
    virtual IAIPathfindingSetup::IMapContainer* getActiveMapContainer() const;
    virtual std::string getMapList();
    virtual void setMaxWaterDepth(float);
    virtual float getMaxWaterDepth() const;
    virtual void drawDynamicAIObjects() const;
    virtual void initMaps();

    // primary slot 90, introduced by this class and in neither interface
    virtual int internalLocalSearch(int, int, unsigned int, unsigned int, const Vec3&, unsigned int, const Vec3&, int, QTPath *);

    // non-virtual, and the debug tool's own accessors
    int getSearchPointToSet() const;
    QTPath* getDebugPath() const;
    ClusterNodePath* getDebugClusterPath() const;
    Vec3* getSearchPoint(int) const;
    bool isValidClusterType(int, unsigned int) const;
    // ELF 0x3A5E10. The blocking variant for ClusterGraph::calculatePortalCosts: acquires its own
    // resource, runs the search to completion and appends one cost to `costs`; see the definition.
    void getPortalCost(std::vector<float>& costs, unsigned short costType, int map,
                       unsigned int island, const Vec3& from, unsigned int fromPolygon,
                       const Vec3& to, unsigned int toPolygon,
                       const std::set<unsigned short>& allowedClusters);
    // ELF 0x3A63B0. One portal-to-portal local search for ClusterGraph::updatePortalCosts; see
    // the definition. False while the search is still running.
    bool getPortalCost(float& cost, unsigned int resource, int map, unsigned int island,
                       unsigned short costType, const Vec3& from, unsigned int fromPolygon,
                       const Vec3& to, unsigned int toPolygon,
                       const std::set<unsigned short>& allowedClusters);
    int getEquipmentTypeFromClusterCostType(int, unsigned short) const;

private:
    // The original layout has base-interface vptrs at +0 and +4; secondary-base calls
    // adjust this by four bytes.
    /*   +8 */ bool m_activatePathfindingDebugTool;   // = false; reset clears it
    /*  +12 */ int m_pathfindingDebugToolVehicleId;   // = 0; an IAISettings vehicle id, not an index
    /*  +16 */ int m_searchPointToSet;                // = 0; which of the two points the next click fills
    // an array, not two members - `searchPointSelected` subscripts it by `m_searchPointToSet`.
    // Both elements are separately heap-allocated zeroed `Vec3`s; `tryPath` writes the caller's two
    // positions straight into them.
    /*  +20 */ Vec3* m_searchPoint[2];
    /*  +28 */ QTPath* m_debugPath;                   // = new QTPath
    /*  +32 */ ClusterNodePath* m_debugClusterPath;   // = new ClusterNodePath
    /*  +36 */ double m_d36;                          // Last debug endpoint selection time; reset sentinel -1000.0.
    /*  +44 */ bool m_clusterSearch;                  // = false; the debug tool's cluster/local toggle
    /*  +48 */ PathfindingEngine<QTSearchNode, QTSearchNodeManager, QTSearchNode,
                                 unsigned short, QTPath>* m_engine;
    /*  +52 */ PathfindingEngine<ClusterNode, ClusterNodeManager, ClusterNode,
                                 unsigned short, ClusterNodePath>* m_clusterEngine;
    // case-insensitive. The comparator is `dice::hfe::NoCaseStringCompare`, attested in the
    // mangled name of every `find` call on it - so "Infantry" and "infantry" are one map.
    /*  +56 */ std::map<std::string, int, NoCaseStringCompare> m_mapIds;
    /*  +68 */ std::vector<MapContainer*> m_maps;
    /*  +80 */ MapContainer* m_activeMap;             // = 0
    /*  +84 */ int m_traceMapIndex;                   // = -666 the sentinel, not -1
    // Shared scratch name for const map-name getters. A later getter call may overwrite
    // its contents. [UNRESOLVED] The original mutable spelling is not established.
    /*  +88 */ mutable std::string m_returnedName;
    // [INFERRED name] the destroyable map-part id counter: `initializeDestroyableMapPart` hands out
    // its value through the int& and increments it (ELF +92, PE +0x7C). Nothing resets it.
    /*  +92 */ int m_nextDestroyableMapPartId;        // = 0
    /*  +96 */ float m_maxWaterDepth;                 // = 0.5f
    /* +100 */ bool m_locking;                        // = false; lock/unlock/isLocking
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIPATHFINDING_H
