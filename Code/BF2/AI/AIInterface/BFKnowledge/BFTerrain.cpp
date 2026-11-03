// [INFERRED] Source placement.

#include "BFTerrain.h"

#include "../../../Core/Debug.h"
#include "../../AIDLL/AI/Main/IAIPathfinding.h"
#include <cfloat>
#include <cmath>
#include "AIPlayer.h"
#include "../../../Physics/Heightmap.h"
#include "../../../Game/GameLogic/CombatAreaManager.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods
#include "../../../Physics/HeightmapCluster.h"   // world::IHeightmapCluster - the +0x30 slot, typed

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// First wins in, exact match out.
IAITerrain::IAITerrain()
{
    if (!CommonSingletonServer::m_iaiTerrain)
    {
// Refresh cached interface pointers after singleton registration changes.
        classManager->registerSimpleSingleton(std::string("IAITerrain"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

IAITerrain::~IAITerrain()
{
    if (CommonSingletonServer::m_iaiTerrain == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IAITerrain"));
        BFSingletonServer::updateStaticMethods();
    }
}

// Initializes terrain dimensions from the engine terrain and registers the AI terrain adapter.
BFTerrain::BFTerrain()
{
    m_heightmapCluster = classManager->getSingleton(std::string("HeightmapCluster"));

    // No guard - see the block above.
    world::IHeightmapCluster* const cluster =
        static_cast<world::IHeightmapCluster*>(m_heightmapCluster);

    // one number, asked for twice. See the block above.
    m_worldSizeX = cluster->getHeightmapSize();
    m_worldSizeZ = cluster->getHeightmapSize();

    m_worldMinX = static_cast<int>(0.0f - static_cast<float>(m_worldSizeX) * 0.5f);
    m_worldMaxX = m_worldSizeX + m_worldMinX;
    m_worldMinZ = static_cast<int>(0.0f - 0.5f * static_cast<float>(m_worldSizeZ));
    m_worldMaxZ = m_worldSizeZ + m_worldMinZ;
}

BFTerrain::~BFTerrain()
{
}

// Terrain altitude queries forward to the engine terrain at the supplied position.

// slot 2 - one forward, no arithmetic.
float BFTerrain::getHeight(float x, float z) const
{
    return static_cast<world::IHeightmapCluster*>(m_heightmapCluster)
               ->getHeightInWorldCoords(x, z);
}

// slot 3. height above the surface - see the block above. The subtraction is
// an `fsubr` against the Vec3's second word, i.e. `position.y - surface`, in that order.
float BFTerrain::getAltitude(const Vec3& position) const
{
    return position.y - getTerrainAltitude(position);
}

// slot 4 - X and Z, and Y is dropped. A Vec3 goes in and a two-dimensional
// question comes out, which is the same XZ-plane assumption `vec3ToVec2XZ` encodes elsewhere.
float BFTerrain::getTerrainAltitude(const Vec3& position) const
{
    return getTerrainAltitude(position.x, position.z);
}

// slot 5. the ground or the water, whichever is higher.
float BFTerrain::getTerrainAltitude(float x, float z) const
{
    const float height = getHeight(x, z);
    const float water  = getWaterLevel(x, z);

    // `fcmovbe`: the water losing a tie - or an unordered compare - yields the height.
    if (water <= height)
        return height;

    return water;
}

// ELF 0x840C000. The shipped method reports its own unimplemented operation.
unsigned char BFTerrain::getMaterial(float /*x*/, float /*z*/) const
{
    Debug(DebugTypeError, "AI/AIInterface/BFKnowledge/BFTerrain.cpp", 111) << "Not implemented";
    return 0;
}

// ELF, - a constant.
// bf2 0x840C0F0: `mov eax, -1`.
int BFTerrain::getMaterialToGiveDamage() const
{
    return -1;
}

// bf2 0x840DEB0: the combat area manager (unchecked) tests the wrapper's player.
bool BFTerrain::isInsideCombatArea(IAIPlayer* player) const
{
    return CombatAreaManager::getInstance()->isPlayerInside(player->getPlayer());
}

// bf2 0x840C100: IHeightmapCluster +0x7C getNormalInWorldCoords.
void BFTerrain::getNormal(float x, float z, Vec3& normal) const
{
    static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getNormalInWorldCoords(x, z, normal);
}

// bf2 0x840C130: IHeightmapCluster +0x84 getHeightAndNormalInWorldCoords, its float passed on
// (the caller pops it from st0, hence the float return the port's declaration lacked).
float BFTerrain::getHeightAndNormal(float x, float z, Vec3& normal, unsigned char& material) const
{
    return static_cast<world::IHeightmapCluster*>(m_heightmapCluster)
        ->getHeightAndNormalInWorldCoords(x, z, normal, material);
}

// bf2 0x840C170: acos of the terrain normal's y (IHeightmapCluster +0x7C into a zeroed Vec3),
// clamped to [-1, 1].
float BFTerrain::getSlope(float x, float z) const
{
    Vec3 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    normal.z = 0.0f;
    static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getNormalInWorldCoords(x, z, normal);
    float y = normal.y;
    if (-1.0f > y)
        y = -1.0f;
    else if (y > 1.0f)
        y = 1.0f;
    return std::acos(y);
}

// slot 12 - the second half of `getTerrainAltitude`, and the same one-line
// forward shape as `getHeight`. `IHeightmapCluster` slot 38.
float BFTerrain::getWaterLevel(float x, float z) const
{
    return static_cast<world::IHeightmapCluster*>(m_heightmapCluster)
               ->getWaterLevelInWorldCoords(x, z);
}

// bf2 0x840C210: the primary heightmap's sea level (IHeightmap +0x68).
float BFTerrain::getSeaWaterLevel() const
{
    return static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getPrimaryHeightmap()->getSeaWaterLevel();
}

// ELF 0x840C230. The second vector is a direction, normalized before scaling by step.
bool BFTerrain::findTerrainPoint(const Vec3& from, const Vec3& direction, float step, Vec3& hit) const
{
    Vec3 unit = direction;
    const float lengthSq = unit.x * unit.x + unit.y * unit.y + unit.z * unit.z;
    if (!(FLT_EPSILON > std::fabs(lengthSq - 1.0f)))
    {
        if (!(FLT_EPSILON > std::fabs(lengthSq)))
        {
            const float scale = 1.0f / std::sqrt(lengthSq);
            unit.x *= scale;
            unit.y *= scale;
            unit.z *= scale;
        }
        else
            unit.x = unit.y = unit.z = 0.0f;
    }
    Vec3 to;
    to.x = step * unit.x + from.x;
    to.y = step * unit.y + from.y;
    to.z = step * unit.z + from.z;
    Vec3 normal = {0.0f, 0.0f, 0.0f};
    float distance;
    unsigned char material;
    return static_cast<world::IHeightmapCluster*>(m_heightmapCluster)
        ->intersectRayInWorldCoords(from, to, hit, distance, normal, material, true);
}

// ELF, - a bare member load. All ten of these are.
// The six world-bounds getters - eleven bytes each.
// They return exactly what the constructor computed and nothing else: no clamping, no
// recomputation, no null check on the heightmap cluster. The whole of the AI's world extent is
// six member reads. The four `getPrimaryWorld*` getters below are not these - they go back
// through the cluster to the primary heightmap every call, and floor the mins while ceiling the
// maxes. Different question, different answer.
int BFTerrain::getWorldSizeX() const
{
    return m_worldSizeX;
}

int BFTerrain::getWorldSizeZ() const
{
    return m_worldSizeZ;
}

int BFTerrain::getWorldMinX() const
{
    return m_worldMinX;
}

int BFTerrain::getWorldMinZ() const
{
    return m_worldMinZ;
}

int BFTerrain::getWorldMaxX() const
{
    return m_worldMaxX;
}

int BFTerrain::getWorldMaxZ() const
{
    return m_worldMaxZ;
}

// bf2 0x840C420: floor of the primary heightmap's getLowBoundaryX (as double), stored to float, then truncated.
int BFTerrain::getPrimaryWorldMinX() const
{
    return static_cast<int>(static_cast<float>(
        std::floor(static_cast<double>(static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getPrimaryHeightmap()->getLowBoundaryX()))));
}

// bf2 0x840C460: floor of the primary heightmap's getLowBoundaryZ (as double), stored to float, then truncated.
int BFTerrain::getPrimaryWorldMinZ() const
{
    return static_cast<int>(static_cast<float>(
        std::floor(static_cast<double>(static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getPrimaryHeightmap()->getLowBoundaryZ()))));
}

// bf2 0x840C4A0: ceil of the primary heightmap's getHighBoundaryX (as double), stored to float, then truncated.
int BFTerrain::getPrimaryWorldMaxX() const
{
    return static_cast<int>(static_cast<float>(
        std::ceil(static_cast<double>(static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getPrimaryHeightmap()->getHighBoundaryX()))));
}

// bf2 0x840C4E0: ceil of the primary heightmap's getHighBoundaryZ (as double), stored to float, then truncated.
int BFTerrain::getPrimaryWorldMaxZ() const
{
    return static_cast<int>(static_cast<float>(
        std::ceil(static_cast<double>(static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getPrimaryHeightmap()->getHighBoundaryZ()))));
}

// bf2 0x840C520: the heightmap cluster's intersectRayInWorldCoords (+0x90, verified in the ELF
// primary vtable), two-sided, into a discarded material byte.
bool BFTerrain::intersectTerrain(const Vec3& from, const Vec3& to, Vec3& hit, float& distance, Vec3& normal) const
{
    unsigned char material;
    return static_cast<world::IHeightmapCluster*>(m_heightmapCluster)
        ->intersectRayInWorldCoords(from, to, hit, distance, normal, material, true);
}

// ELF 0x840C580. Output references are forwarded in their original order.
bool BFTerrain::getTriangle(const Vec3& position, Vec3& a, Vec3& b, Vec3& c, Vec3& normal, unsigned char& material) const
{
    return static_cast<world::IHeightmapCluster*>(m_heightmapCluster)
        ->getTriangleAndNormalAndMaterialInWorldCoords(position.x, position.z, a, b, c, normal, material);
}

// ELF 0x840C5E0. Sample just beyond the selected edge, away from the opposite vertex.
bool BFTerrain::getTriangleNeighbour(const Vec3& a, const Vec3& b, const Vec3& c, unsigned int edge, Vec3& na, Vec3& nb, Vec3& nc, Vec3& normal, unsigned char& material) const
{
    const Vec3 *first, *second, *opposite;
    switch (edge)
    {
    case 0: first = &b; second = &a; opposite = &c; break;
    case 1: first = &c; second = &b; opposite = &a; break;
    case 2: first = &a; second = &c; opposite = &b; break;
    default: return false;
    }
    Vec3 midpoint;
    midpoint.x = (first->x + second->x) * 0.5f;
    midpoint.y = (first->y + second->y) * 0.5f;
    midpoint.z = (first->z + second->z) * 0.5f;
    Vec3 position;
    position.x = (midpoint.x - opposite->x) * 0.2f + midpoint.x;
    position.y = (midpoint.y - opposite->y) * 0.2f + midpoint.y;
    position.z = (midpoint.z - opposite->z) * 0.2f + midpoint.z;
    return getTriangle(position, na, nb, nc, normal, material);
}

// ELF 0x840C7B0.
void BFTerrain::getAllTerrainPolygons(std::pair<Vec2, Vec2> bounds, std::vector<Vec3>& vertices, std::vector<std::pair<int, int> >& edges, std::list<triple<int, int, int> >& triangles) const
{
    world::IHeightmap* const heightmap =
        static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getPrimaryHeightmap();
    Vec2 max;
    max.x = heightmap->getHighBoundaryX();
    max.y = heightmap->getHighBoundaryZ();
    Vec2 min;
    min.x = heightmap->getLowBoundaryX();
    min.y = heightmap->getLowBoundaryZ();
    getAllTerrainPolygonsWithinBoundary(bounds, min, max, vertices, edges, triangles);
}

// ELF 0x840C880.
void BFTerrain::getAllTerrainPolygonsWithinBoundary(std::pair<Vec2, Vec2> bounds, const Vec2& min, const Vec2& max, std::vector<Vec3>& vertices, std::vector<std::pair<int, int> >& edges, std::list<triple<int, int, int> >& triangles) const
{
    world::IHeightmap* const heightmap =
        static_cast<world::IHeightmapCluster*>(m_heightmapCluster)->getPrimaryHeightmap();
    const Vec3& scale = heightmap->getScale();
    const float maxX = std::ceil(max.x / scale.x) * scale.x;
    const float maxZ = std::ceil(max.y / scale.z) * scale.z;
    const float minX = std::floor(min.x / scale.x) * scale.x;
    const float minZ = std::floor(min.y / scale.z) * scale.z;
    const int countX = static_cast<int>((maxX - minX) / scale.x) + 1;
    const int countZ = static_cast<int>((maxZ - minZ) / scale.z) + 1;
    const unsigned int count = static_cast<unsigned int>(countX * countZ);

    // [CHOSEN, not recovered] The shared CRT heap policy has no per-allocation cap:
    // https://github.com/B2BF/OpenBF2/issues/7 (docs/architecture.md, Images and boundaries).
    // Retail Memory.cpp set_max_alloc_size/reset_max_alloc_size calls are omitted here and
    // in checkUnconnectedMeshData; no ineffectual replacement setters are published.
    vertices.reserve(count);
    std::vector<int> vertexMap;
    vertexMap.reserve(count);
    int nextVertex = 0;
    for (float x = minX; maxX >= x; x += scale.x)
    {
        for (float z = minZ; maxZ >= z; z += scale.z)
        {
            Vec3 position;
            position.x = x;
            position.y = heightmap->getHeightInWorldCoords(x, z);
            position.z = z;
            if (pointInsideBbox(position, bounds))
            {
                vertices.push_back(position);
                vertexMap.push_back(nextVertex++);
            }
            else
                vertexMap.push_back(-1);
        }
    }

    // ELF 0x840C860: edgesSize(countX, countZ).
    const int edgeCount = (countZ - 1) * (countX - 1) + (countZ - 1) * countX + countZ * (countX - 1);
    edges.reserve(static_cast<unsigned int>(edgeCount));
    std::vector<int> edgeMap;
    edgeMap.reserve(static_cast<unsigned int>(3 * countZ * countX));
    int nextEdge = 0;
    for (int x = 0; x < countX; ++x)
    {
        for (int z = 0; z < countZ - 1; ++z)
        {
            const int a = vertexMap[x * countZ + z];
            const int b = vertexMap[x * countZ + z + 1];
            if (a >= 0 && b >= 0)
            {
                edges.push_back(std::make_pair(a, b));
                edgeMap.push_back(nextEdge++);
            }
            else
                edgeMap.push_back(-1);
        }
    }
    for (int x = 0; x < countX - 1; ++x)
    {
        for (int z = 0; z < countZ; ++z)
        {
            const int a = vertexMap[x * countZ + z];
            const int b = vertexMap[(x + 1) * countZ + z];
            if (a >= 0 && b >= 0)
            {
                edges.push_back(std::make_pair(a, b));
                edgeMap.push_back(nextEdge++);
            }
            else
                edgeMap.push_back(-1);
        }
    }
    for (int x = 0; x < countX - 1; ++x)
    {
        for (int z = 1; z < countZ; ++z)
        {
            const int a = vertexMap[x * countZ + z];
            const int b = vertexMap[(x + 1) * countZ + z - 1];
            if (a >= 0 && b >= 0)
            {
                edges.push_back(std::make_pair(a, b));
                edgeMap.push_back(nextEdge++);
            }
            else
                edgeMap.push_back(-1);
        }
    }
    const int horizontalCount = (countZ - 1) * countX;
    const int diagonalStart = horizontalCount + countZ * (countX - 1);
    for (int x = 0; x < countX - 1; ++x)
    {
        for (int z = 0; z < countZ - 1; ++z)
        {
            const int horizontal = x * (countZ - 1) + z;
            const int vertical = horizontalCount + countZ * x + z;
            const int diagonal = diagonalStart + x * (countZ - 1) + z;
            if (edgeMap[horizontal] >= 0 && edgeMap[diagonal] >= 0 && edgeMap[vertical] >= 0)
                triangles.push_back(make_triple(edgeMap[horizontal], edgeMap[diagonal], edgeMap[vertical]));
            if (edgeMap[vertical + 1] >= 0 && edgeMap[(x + 1) * (countZ - 1) + z] >= 0 && edgeMap[diagonal] >= 0)
                triangles.push_back(make_triple(edgeMap[vertical + 1], edgeMap[(x + 1) * (countZ - 1) + z], edgeMap[diagonal]));
        }
    }
    // shipped defect: output is appended, but generated vertex and edge IDs start at zero.
    checkUnconnectedMeshData(vertices, edges, triangles);
}

// ELF 0x840D470. The retail trailing scans do not remove or remap any mesh data.
void BFTerrain::checkUnconnectedMeshData(std::vector<Vec3>& vertices,
                                       std::vector<std::pair<int, int>>& edges,
                                       std::list<triple<int, int, int>>& triangles) const
{
    std::vector<bool> usedVertices(vertices.size(), false);
    std::vector<bool> usedEdges(edges.size(), false);
    for (std::list<triple<int, int, int>>::const_iterator it = triangles.begin(); it != triangles.end(); ++it)
    {
        usedVertices[edges[it->first].first] = true;
        usedVertices[edges[it->first].second] = true;
        usedVertices[edges[it->second].first] = true;
        usedVertices[edges[it->second].second] = true;
        usedVertices[edges[it->third].first] = true;
        usedVertices[edges[it->third].second] = true;
        usedEdges[it->first] = true;
        usedEdges[it->second] = true;
        usedEdges[it->third] = true;
    }
    for (unsigned int i = 0; i < usedVertices.size(); ++i) {}
    for (unsigned int i = 0; i < usedEdges.size(); ++i) {}
}

// ELF 0x840DE60. Ordered comparisons reject NaN in either coordinate or boundary.
bool BFTerrain::pointInsideBbox(const Vec3& position, const std::pair<Vec2, Vec2>& bounds) const
{
    return position.x >= bounds.first.x && position.z >= bounds.first.y
        && bounds.second.x >= position.x && bounds.second.y >= position.z;
}

} // namespace ai
} // namespace hfe
} // namespace dice
