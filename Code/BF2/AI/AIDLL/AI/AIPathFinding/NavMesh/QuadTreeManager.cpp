// Navigation island ownership, GTS import orchestration and navigation debug meshes.

#include "QuadTreeManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x310FA0, __tcf_0 0x311000, _GLOBAL__I 0x311030).
#include <iostream>

#include <cstdlib>                                       // std::abort - the ELF's island trap
#include <algorithm>
#include <map>
#include <set>
#include <sstream>

#include "QuadTree.h"
#include "../Cluster/ClusterGraph.h"
#include "../../AIUtils/checkPointer.h"
#include "../../Main/AIRandom.h"
#include "../../Main/AISettingsInterfaces.h"
#include "../../Main/CommonSingletonServer.h"
#include "../../DrawPrimitives/DPManager.h"             // IDPManager, IDPStaticCreator
#include "../../../../AIInterface/BFKnowledge/BFGame.h"  // IAIGame::getLevelPath
#include "../../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../../../IO/File/File.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x30CACB, 0x30E669, 0x30F9AC, 0x310375: inlined constructor.
QuadTreeManager::DebugMesh::DebugMesh()
    : m_numberOfVertices(0), m_numberOfPolygons(0), m_numberOfIndices(0)
{
    // shipped defect: the pointers, including the drawing handle, remain uninitialized.
}

// ELF 0x30D126..0x30D1FF (inlined); amd-64 ELF 0x3580F0 attests lockSize's name.
void QuadTreeManager::DebugMesh::lockSize()
{
    m_firstIndices = checkPointer(new unsigned int[m_numberOfPolygons]);
    m_vertexCounts = checkPointer(new unsigned int[m_numberOfPolygons]);
    m_vertices = checkPointer(new Vec3[m_numberOfVertices]());
    m_indices = checkPointer(new unsigned int[m_numberOfIndices]);
    m_materials = checkPointer(new unsigned char[m_numberOfPolygons]);
    m_colours = checkPointer(new unsigned int[m_numberOfPolygons]);
}

// ELF 0x30A4A0 (C2; C1 0x30A5F0). Initializes an empty quad-tree collection and its island
// metadata.
QuadTreeManager::QuadTreeManager()
{
    // m_numberOfIslands deliberately not initialised - the binary steps over it.
}

// ELF 0x30DD40 (D2; D1 0x30DE30; first recovered on the client track,
// docs/macos-c-singleplayer.md): destroyQuadTrees, unloadDebugMeshes, unloadDebugClusterMeshes, then the four lists and the
// island vector go as members.
QuadTreeManager::~QuadTreeManager()
{
    destroyQuadTrees();
    unloadDebugMeshes();
    unloadDebugClusterMeshes();
}

// ELF 0x30E530 / 0x30E550. Returns the selected island or cluster graph. The caller must supply
// a valid island index.
QuadTree* QuadTreeManager::getIsland(unsigned int island) const
{
    return m_islands[island].first;
}

ClusterGraph* QuadTreeManager::getClusterGraph(unsigned int island) const
{
    return m_islands[island].second;
}

// ELF 0x310F80.
unsigned int QuadTreeManager::getNumberOfIslands() const
{
    return static_cast<unsigned int>(m_islands.size());
}

// ELF 0x30E240. Deletes both halves of every island's pair (each `delete` null-tests for itself), then
// `resize(0)` - the shape in both images: g++ 3.x's size test with erase / zero-length
// _M_fill_insert, and MSVC's size test with an erase that only moves `end` back. Not clear: the
// vector keeps its capacity in both. Called first by loadQuadTrees, so a reload frees the old
// islands before the new files are even opened - a reload that then fails to open leaves the map
// with no islands at all.
void QuadTreeManager::destroyQuadTrees()
{
    for (std::vector<std::pair<QuadTree*, ClusterGraph*> >::iterator it = m_islands.begin();
         it != m_islands.end(); ++it)
    {
        delete it->first;
        delete it->second;
    }
    m_islands.resize(0);
}

// ELF 0x30A690. Deletes the matching quad tree while retaining its island entry and nulling its pointer.
void QuadTreeManager::destroyQuadTree(QuadTree* quadTree)
{
    std::vector<std::pair<QuadTree*, ClusterGraph*> >::iterator it = m_islands.begin();
    for (; it != m_islands.end(); ++it)
    {
        if (it->first == quadTree)
            break;
    }

    if (it != m_islands.end())
    {
        delete it->first;
        it->first = 0;
    }
}

// ELF 0x30ABA0. Loads navigation meshes for a map into the managed quad trees.
void QuadTreeManager::loadQuadTrees(const std::string& name)
{
    destroyQuadTrees();

    const std::string levelPath =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getLevelPath();

    io::File quadTreeFile;
    if (!quadTreeFile.open(levelPath + "/AIPathFinding/" + name + ".qtr", io::FILEMODE_READ,
                           static_cast<io::FileAccess>(0), 0))
        return;

    io::File clusterFile;
    if (!clusterFile.open(levelPath + "/AIPathFinding/" + name + ".clb", io::FILEMODE_READ,
                          static_cast<io::FileAccess>(0), 0))
        return;

    quadTreeFile.read(m_numberOfIslands);
    if (m_numberOfIslands > 1)
    {
        // [PLATFORM] ELF: signed `cmp edi, 1 / jg` at 0x30AF0A, `call abort` at 0x30B385. PE: signed
        // `cmp [ebx], 2 / jl` at , `int 3` at - which falls through into the
        // island loop if a debugger resumes it.
#if defined(_MSC_VER)
        __debugbreak();
#else
        std::abort();
#endif
    }

    for (unsigned int island = 0; island < static_cast<unsigned int>(m_numberOfIslands); ++island)
    {
        QuadTree* const quadTree = checkPointer(new QuadTree(island));
        quadTree->readFromFile(quadTreeFile);

        ClusterGraph* const clusterGraph = checkPointer(new ClusterGraph);
        clusterGraph->readFromFile(clusterFile);

        m_islands.push_back(std::make_pair(quadTree, clusterGraph));
    }

    for (unsigned int island = 0; island < static_cast<unsigned int>(m_numberOfIslands); ++island)
    {
        for (unsigned short cluster = 0;
             cluster < m_islands[island].second->getNumberOfClusters();
             ++cluster)
        {
        }
    }

    clusterFile.close();
    quadTreeFile.close();
}

// ELF 0x30A700. Writes <level>/AIPathFinding/<name>.qtr and .clb: the island count (also stored in
// m_numberOfIslands) to the tree file, then each island's tree and graph. Nothing is written
// unless both files open.
void QuadTreeManager::saveQuadTrees(const std::string& name)
{
    const std::string levelPath =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getLevelPath();

    io::File quadTreeFile;
    if (!quadTreeFile.open(levelPath + "/AIPathFinding/" + name + ".qtr",
                           static_cast<io::FileMode>(0), io::FILEACCESS_1, 0))
        return;

    io::File clusterFile;
    if (!clusterFile.open(levelPath + "/AIPathFinding/" + name + ".clb",
                          static_cast<io::FileMode>(0), io::FILEACCESS_1, 0))
        return;

    m_numberOfIslands = static_cast<int>(m_islands.size());
    quadTreeFile.write(m_numberOfIslands);
    for (std::vector<std::pair<QuadTree*, ClusterGraph*> >::iterator it = m_islands.begin();
         it != m_islands.end(); ++it)
    {
        it->first->saveToFile(quadTreeFile);
        it->second->saveToFile(clusterFile);
    }

    quadTreeFile.close();
    clusterFile.close();
}

// ELF 0x30B390. Imports and optimises each island before publishing it and computing its costs.
bool QuadTreeManager::loadGTSData(const std::string& name, int maxDepth, int minPolygons,
                                 int map, float radius)
{
    destroyQuadTrees();
    const std::string levelPath =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getLevelPath();
    io::File treeFile;
    if (!treeFile.open(levelPath + "/GTSData/Output/" + name + ".qti",
                       io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0))
        return false;
    io::File clusterFile;
    if (!clusterFile.open(levelPath + "/GTSData/Output/" + name + ".cls",
                          io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0))
        return false;
    unsigned int count;
    treeFile.read(count);
    if (count == 0)
        return false;

    std::vector<DestroyableObject> objects;
    static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->getAllDestroyableObjects(objects);
    unsigned int island = 0;
    do
    {
        QuadTree* tree = checkPointer(new QuadTree(island));
        tree->m_gtsBuildValue = 1;
        tree->m_maxDepthInTree = maxDepth;
        tree->m_minPolygonsInLeaf = minPolygons;
        tree->readGTSDataFromFileDoubles(treeFile);
        for (std::vector<DestroyableObject>::const_iterator i = objects.begin();
             i != objects.end(); ++i)
        {
            // shipped defect: every object is assigned the same map-part id.
            tree->initializeDestroyableMapPart(map, i->initiallyWalkable, i->min, i->max,
                                               i->transform, 0, true);
        }
        ClusterGraph* graph = checkPointer(new ClusterGraph);
        graph->readGTSDataFromFile(tree, clusterFile);
        graph->validateClusterGraphConnectivity(tree, true);
        tree->optimiseGTSData(graph);
        graph->validateClusterGraphConnectivity(tree, false);
        graph->createPortals(map, tree, radius);
        m_islands.push_back(std::make_pair(tree, graph));
        graph->calculatePortalCosts(map, island);
        ++island;
    } while (--count != 0);
    treeFile.close();
    clusterFile.close();
    return island != 0;
}

// ELF 0x30BDA0. Rebuilds every island's portal cost table (island index in vector order).
void QuadTreeManager::recalculatePortalCosts(int map)
{
    unsigned int island = 0;
    for (std::vector<std::pair<QuadTree*, ClusterGraph*> >::iterator it = m_islands.begin();
         it != m_islands.end(); ++it, ++island)
        it->second->calculatePortalCosts(map, island);
}

// ELF 0x30BE00. Re-creates each debug mesh's static mesh (1e9 s); neither manager is null-tested.
// DPStaticMesh is not defined in this tree; the image stores the returned pointer unadjusted as
// the DrawPrimitive handle. [CHOSEN, not recovered] cast.
void QuadTreeManager::draw() const
{
    for (std::list<DebugMesh*>::const_iterator it = m_debugMeshes.begin();
         it != m_debugMeshes.end(); ++it)
    {
        DebugMesh* const mesh = *it;
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(mesh->m_mesh);
        mesh->m_mesh = 0;
        mesh->m_mesh = reinterpret_cast<DrawPrimitive*>(
            static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
                ->createStaticMesh(1.0e9f, mesh->m_colours, 0, mesh->m_vertices,
                                   mesh->m_indices, mesh->m_numberOfPolygons, true));
    }
}

// ELF 0x30BEB0.
void QuadTreeManager::undraw() const
{
    for (std::list<DebugMesh*>::const_iterator it = m_debugMeshes.begin();
         it != m_debugMeshes.end(); ++it)
    {
        DebugMesh* const mesh = *it;
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(mesh->m_mesh);
        mesh->m_mesh = 0;
    }
}

// ELF 0x30BF20. draw on the cluster meshes.
void QuadTreeManager::drawCluster()
{
    for (std::list<DebugMesh*>::iterator it = m_debugClusterMeshes.begin();
         it != m_debugClusterMeshes.end(); ++it)
    {
        DebugMesh* const mesh = *it;
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(mesh->m_mesh);
        mesh->m_mesh = 0;
        mesh->m_mesh = reinterpret_cast<DrawPrimitive*>(
            static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
                ->createStaticMesh(1.0e9f, mesh->m_colours, 0, mesh->m_vertices,
                                   mesh->m_indices, mesh->m_numberOfPolygons, true));
    }
}

// ELF 0x30BFE0.
void QuadTreeManager::undrawCluster() const
{
    for (std::list<DebugMesh*>::const_iterator it = m_debugClusterMeshes.begin();
         it != m_debugClusterMeshes.end(); ++it)
    {
        DebugMesh* const mesh = *it;
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(mesh->m_mesh);
        mesh->m_mesh = 0;
    }
}

// ELF 0x30C050. The first drawing handle is a circle; the remaining handles are portal labels.
void QuadTreeManager::drawClusterNeighbourIndices(const Vec3& point)
{
    std::list<DrawPrimitive*>::iterator i = m_drawn.begin();
    if (i != m_drawn.end())
    {
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteCircle(*i);
        i = m_drawn.erase(i);
    }
    while (i != m_drawn.end())
    {
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->delete3dText(*i);
        i = m_drawn.erase(i);
    }
    const Vec3 up = {0.0f, 1.0f, 0.0f};
    const Vec3 raised = {point.x + 0.0f, point.y + 1.0f, point.z + 0.0f};
    m_drawn.push_back(reinterpret_cast<DrawPrimitive*>(
        static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStaticCircle(60.0f, 0xFFFFFFu, raised, 0.5f, up, false, 255)));

    const unsigned int islands = static_cast<unsigned int>(m_islands.size());
    for (unsigned int island = 0; island < islands; ++island)
    {
        QuadTree* tree = m_islands[island].first;
        ClusterGraph* graph = m_islands[island].second;
        MaterialManager* materials =
            static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
                ->getVehicleMaterialManager(1);
        unsigned int polygon = 0xFFFFFFFFu;
        if (!tree->findPolygon(point, polygon, materials, 100.0f))
            continue;
        const unsigned short cluster = graph->getClusterId(tree, polygon);
        if (cluster == 0xFFFFu)
            return;
        std::list<ClusterPortal*> portals;
        graph->getClusterPortals(cluster, 0, portals);
        for (std::list<ClusterPortal*>::const_iterator p = portals.begin();
             p != portals.end(); ++p)
        {
            std::stringstream text;
            if ((*p)->m_neighbourCluster == 0xFFFFu)
                text << "Inv";
            else
                text << static_cast<unsigned long>((*p)->m_neighbourCluster);
            Vec3 position = (*p)->m_position;
            // Preserve the x87 expression's precision until the final float store.
            const long double random = static_cast<long double>(AIRandom::m_instance->next()) *
                                       2.3283064365386962890625e-10f;
            position.y = static_cast<float>((3.0L - 1.0L) * random + 1.0L + position.y);
            m_drawn.push_back(
                static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
                    ->createStatic3dText(60.0f, 0xFFFFFFu, position, text.str(), 0,
                                         static_cast<AITextAlignmentX>(0),
                                         static_cast<AITextAlignmentY>(0)));
        }
    }
}

// ELF 0x30E570. Deduplicates vertices within each island and splits at 0x5555 polygons.
void QuadTreeManager::buildDebugMeshes(const std::string&)
{
    unloadDebugMeshes();
    std::list<unsigned int> indices;
    std::map<unsigned int, unsigned int> vertexMap;
    std::list<Vec3> vertices;
    std::list<unsigned char> materials;
    std::vector<unsigned int> counts;
    const unsigned int islands = static_cast<unsigned int>(m_islands.size());
    DebugMesh* mesh = checkPointer(new DebugMesh);
    int polygons = 0;
    unsigned int nextVertex = 0;
    for (unsigned int island = 0; island < islands; ++island)
    {
        QuadTree* tree = m_islands[island].first;
        const unsigned int polygonCount = tree->m_numberOfPolygons;
        vertexMap.clear();
        for (unsigned int polygon = 0; polygon < polygonCount; ++polygon)
        {
            const Polygon* p = tree->getPolygon(polygon);
            counts.push_back(p->m_numberOfVertices);
            materials.push_back(p->m_material);
            for (unsigned char corner = 0; corner < p->m_numberOfVertices; ++corner)
            {
                const unsigned int index = tree->m_vertexIndices[p->m_firstIndex + corner];
                std::map<unsigned int, unsigned int>::iterator v = vertexMap.find(index);
                if (v == vertexMap.end())
                {
                    vertexMap.insert(std::make_pair(index, nextVertex));
                    indices.push_back(nextVertex);
                    vertices.push_back(tree->m_vertices[index]);
                    ++nextVertex;
                }
                else
                    indices.push_back(v->second);
            }
            ++polygons;
            if ((island == islands - 1 && polygon == polygonCount - 1) || polygons == 0x5555)
            {
                mesh->m_numberOfPolygons = polygons;
                mesh->m_numberOfVertices = static_cast<int>(vertices.size());
                mesh->m_numberOfIndices = static_cast<int>(indices.size());
                mesh->lockSize();
                std::copy(vertices.begin(), vertices.end(), mesh->m_vertices);
                std::copy(indices.begin(), indices.end(), mesh->m_indices);
                std::copy(materials.begin(), materials.end(), mesh->m_materials);
                IAIObjectEnvironment* environment = static_cast<IAIObjectEnvironment*>(
                    CommonSingletonServer::m_iaiObjectEnvironment);
                environment->initAiCombatAreas();
                for (int p = 0; p < mesh->m_numberOfPolygons; ++p)
                {
                    mesh->m_vertexCounts[p] = counts[p];
                    mesh->m_firstIndices[p] = p > 0 ?
                        mesh->m_firstIndices[p - 1] + counts[p - 1] : 0;
                    bool outside = false;
                    for (unsigned int corner = 0; corner < counts[p]; ++corner)
                    {
                        if (!environment->isAiPositionInsideCombatArea(mesh->m_vertices[
                                mesh->m_indices[mesh->m_firstIndices[p] + corner]]))
                        {
                            outside = true;
                            break;
                        }
                    }
                    if (outside)
                        mesh->m_colours[p] = 0;
                    else
                    {
                        unsigned int base;
                        switch (mesh->m_materials[p])
                        {
                        case 1: base = 0x80C000u; break;
                        case 2: base = 0xE00000u; break;
                        case 3: base = 0xE0E000u; break;
                        case 4: base = 0x6020E0u; break;
                        default: base = 0xE00000u; break;
                        }
                        mesh->m_colours[p] = (AIRandom::m_instance->next() & 0x1F1F1F1Fu) + base;
                    }
                }
                m_debugMeshes.push_back(mesh);
                if (polygons == 0x5555)
                {
                    mesh = checkPointer(new DebugMesh);
                    polygons = 0;
                    nextVertex = 0;
                    vertices.resize(0, Vec3());
                    indices.resize(0, 0);
                    vertexMap.clear();
                    materials.resize(0, 0);
                    counts.resize(0);
                }
            }
        }
    }
    // shipped defect: an unused mesh leaks for empty input or an exact final chunk.
    triangulateDebugMeshes(m_debugMeshes);
}

// ELF 0x30C9B0. Clusters share a base colour; each polygon consumes another random value.
void QuadTreeManager::buildDebugClusterMeshes(const std::string&)
{
    unloadDebugClusterMeshes();
    const unsigned int islands = static_cast<unsigned int>(m_islands.size());
    int polygons = 0;
    std::list<unsigned int> indices;
    std::list<Vec3> vertices;
    std::map<unsigned int, unsigned int> vertexMap;
    std::vector<unsigned int> counts;
    std::list<unsigned char> materials;
    std::list<unsigned int> colours;
    unsigned int clusterTag = 1;
    DebugMesh* mesh = checkPointer(new DebugMesh);
    unsigned int nextVertex = 0;
    for (unsigned int island = 0; island < islands; ++island)
    {
        QuadTree* tree = m_islands[island].first;
        ClusterGraph* graph = m_islands[island].second;
        const unsigned int clusterCount = graph->m_numberOfClusters;
        vertexMap.clear();
        for (unsigned short cluster = 0; cluster < clusterCount; ++cluster)
        {
            const unsigned int first = graph->m_clusters[cluster].m_firstPolygon;
            const unsigned int count = graph->m_clusters[cluster].m_numberOfPolygons;
            unsigned int base = graph->m_clusters[cluster].m_walkable ?
                AIRandom::m_instance->next() & 0xFFFFFFu : 0xFF0000u;
            for (unsigned int polygon = 0; polygon < count; ++polygon)
            {
                const Polygon* p = tree->getPolygon(graph->m_clusterPolygons[first + polygon]);
                ++polygons;
                counts.push_back(p->m_numberOfVertices);
                materials.push_back(static_cast<unsigned char>(clusterTag));
                base &= 0xE0E0E0E0u;
                colours.push_back((AIRandom::m_instance->next() & 0x1F1F1F1Fu) + base);
                for (unsigned char corner = 0; corner < p->m_numberOfVertices; ++corner)
                {
                    const unsigned int index = tree->m_vertexIndices[p->m_firstIndex + corner];
                    std::map<unsigned int, unsigned int>::iterator v = vertexMap.find(index);
                    if (v == vertexMap.end())
                    {
                        vertexMap.insert(std::make_pair(index, nextVertex));
                        indices.push_back(nextVertex);
                        vertices.push_back(tree->m_vertices[index]);
                        ++nextVertex;
                    }
                    else
                        indices.push_back(v->second);
                }
                if ((island == islands - 1 && cluster == clusterCount - 1 &&
                     polygon == count - 1) || polygons == 0x5555)
                {
                    mesh->m_numberOfPolygons = polygons;
                    mesh->m_numberOfVertices = static_cast<int>(vertices.size());
                    mesh->m_numberOfIndices = static_cast<int>(indices.size());
                    mesh->lockSize();
                    std::copy(vertices.begin(), vertices.end(), mesh->m_vertices);
                    std::copy(indices.begin(), indices.end(), mesh->m_indices);
                    std::copy(materials.begin(), materials.end(), mesh->m_materials);
                    std::list<unsigned int>::const_iterator colour = colours.begin();
                    for (int p = 0; p < mesh->m_numberOfPolygons; ++p, ++colour)
                    {
                        mesh->m_vertexCounts[p] = counts[p];
                        mesh->m_firstIndices[p] = p > 0 ?
                            mesh->m_firstIndices[p - 1] + counts[p - 1] : 0;
                        mesh->m_colours[p] = *colour;
                    }
                    m_debugClusterMeshes.push_back(mesh);
                    if (polygons == 0x5555)
                    {
                        mesh = checkPointer(new DebugMesh);
                        nextVertex = 0;
                        polygons = 0;
                        vertices.resize(0, Vec3());
                        indices.resize(0, 0);
                        vertexMap.clear();
                        materials.resize(0, 0);
                        counts.resize(0);
                        colours.resize(0, 0);
                    }
                }
            }
            ++clusterTag;
        }
    }
    // shipped defect: an unused mesh leaks for empty input or an exact final chunk.
    triangulateDebugMeshes(m_debugClusterMeshes);
}

// ELF 0x30F630. GTS debug triangles use different material colours from the quad-tree meshes.
void QuadTreeManager::loadGTSDebugMeshes(const std::string& name)
{
    unloadDebugMeshes();
    const std::string levelPath =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getLevelPath();
    io::File file;
    if (!file.open(levelPath + "/GTSData/Output/" + name + ".vbf",
                   io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0))
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTreeManager.cpp", 938, "")
            << "Could not find file " << levelPath << "/GTSData/Output/" << name << ".vbf";
        return;
    }
    int count;
    file.read(count);
    IAIObjectEnvironment* environment = static_cast<IAIObjectEnvironment*>(
        CommonSingletonServer::m_iaiObjectEnvironment);
    environment->initAiCombatAreas();
    for (int island = 0; island < count; ++island)
    {
        DebugMesh* mesh = checkPointer(new DebugMesh);
        file.read(mesh->m_numberOfVertices);
        file.read(mesh->m_numberOfPolygons);
        mesh->m_numberOfIndices = mesh->m_numberOfPolygons * 3;
        mesh->lockSize();
        file.read(mesh->m_vertices, mesh->m_numberOfVertices * 12);
        file.read(mesh->m_indices, mesh->m_numberOfPolygons * 12);
        file.read(mesh->m_materials, mesh->m_numberOfPolygons);
        for (int polygon = 0; polygon < mesh->m_numberOfPolygons; ++polygon)
        {
            mesh->m_firstIndices[polygon] = polygon * 3;
            mesh->m_vertexCounts[polygon] = 3;
            if (!environment->isAiPositionInsideCombatArea(
                    mesh->m_vertices[mesh->m_indices[polygon * 3]]) ||
                !environment->isAiPositionInsideCombatArea(
                    mesh->m_vertices[mesh->m_indices[polygon * 3 + 1]]) ||
                !environment->isAiPositionInsideCombatArea(
                    mesh->m_vertices[mesh->m_indices[polygon * 3 + 2]]))
                mesh->m_colours[polygon] = 0;
            else
            {
                unsigned int base;
                switch (static_cast<unsigned int>(mesh->m_materials[polygon]) + 1)
                {
                case 1: base = 0x80C000u; break;
                case 3: base = 0x80C0u; break;
                case 4: base = 0x40A0u; break;
                default: base = 0xE00000u; break;
                }
                mesh->m_colours[polygon] = (AIRandom::m_instance->next() & 0x1F1F1F1Fu) + base;
            }
        }
        m_debugMeshes.push_back(mesh);
    }
    file.close();
}

// ELF 0x310150. Erases every mesh node first, then releases the mesh's static mesh (IDPManager
// +0x50), frees its six arrays and the mesh; finally resize(0).
void QuadTreeManager::unloadDebugMeshes()
{
    std::list<DebugMesh*>::iterator it = m_debugMeshes.begin();
    while (it != m_debugMeshes.end())
    {
        DebugMesh* const mesh = *it;
        it = m_debugMeshes.erase(it);
        if (mesh)
        {
            static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(mesh->m_mesh);
            mesh->m_mesh = 0;
            delete[] mesh->m_firstIndices;
            delete[] mesh->m_vertexCounts;
            delete[] mesh->m_vertices;
            delete[] mesh->m_indices;
            delete[] mesh->m_materials;
            delete[] mesh->m_colours;
            delete mesh;
        }
    }
    m_debugMeshes.resize(0, 0);
}

// ELF 0x30DBD0: the same sequence on the cluster-mesh list.
void QuadTreeManager::unloadDebugClusterMeshes()
{
    std::list<DebugMesh*>::iterator it = m_debugClusterMeshes.begin();
    while (it != m_debugClusterMeshes.end())
    {
        DebugMesh* const mesh = *it;
        it = m_debugClusterMeshes.erase(it);
        if (mesh)
        {
            static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(mesh->m_mesh);
            mesh->m_mesh = 0;
            delete[] mesh->m_firstIndices;
            delete[] mesh->m_vertexCounts;
            delete[] mesh->m_vertices;
            delete[] mesh->m_indices;
            delete[] mesh->m_materials;
            delete[] mesh->m_colours;
            delete mesh;
        }
    }
    m_debugClusterMeshes.resize(0, 0);
}

// ELF 0x3102C0. Replaces each polygon mesh with triangle fans, retaining sorted vertex order.
void QuadTreeManager::triangulateDebugMeshes(std::list<DebugMesh*>& meshes)
{
    std::vector<DebugMesh*> triangles;
    for (std::list<DebugMesh*>::const_iterator i = meshes.begin(); i != meshes.end(); ++i)
    {
        DebugMesh* source = *i;
        int first = 0;
        while (first < source->m_numberOfPolygons)
        {
            DebugMesh* mesh = checkPointer(new DebugMesh);
            std::set<unsigned int> vertices;
            int polygonCount = 0;
            int indexCount = 0;
            int end = first;
            int prospectiveCount = static_cast<int>(source->m_vertexCounts[end] - 2);
            while (prospectiveCount <= 0x5555 && end < source->m_numberOfPolygons)
            {
                polygonCount = prospectiveCount;
                indexCount += source->m_vertexCounts[end] * 3 - 6;
                ++end;
                // shipped defect: the lookahead reads past the final polygon's count.
                prospectiveCount = static_cast<int>(source->m_vertexCounts[end] - 2) + polygonCount;
            }
            for (int polygon = first; polygon < end; ++polygon)
                for (unsigned int corner = 0; corner < source->m_vertexCounts[polygon]; ++corner)
                    vertices.insert(source->m_indices[source->m_firstIndices[polygon] + corner]);

            mesh->m_numberOfVertices = static_cast<int>(vertices.size());
            mesh->m_numberOfPolygons = polygonCount;
            mesh->m_numberOfIndices = indexCount;
            mesh->lockSize();
            std::map<unsigned int, unsigned int> vertexMap;
            unsigned int nextVertex = 0;
            for (std::set<unsigned int>::const_iterator v = vertices.begin(); v != vertices.end(); ++v)
            {
                mesh->m_vertices[nextVertex] = source->m_vertices[*v];
                vertexMap.insert(std::make_pair(*v, nextVertex));
                ++nextVertex;
            }
            unsigned int triangle = 0;
            unsigned int index = 0;
            unsigned int firstIndex = 0;
            for (int polygon = first; polygon < end; ++polygon)
            {
                const std::map<unsigned int, unsigned int>::const_iterator v = vertexMap.find(
                    source->m_indices[source->m_firstIndices[polygon]]);
                for (unsigned int corner = 1; corner < source->m_vertexCounts[polygon] - 1; ++corner)
                {
                    mesh->m_indices[index++] = v->second;
                    mesh->m_indices[index++] = vertexMap.find(
                        source->m_indices[source->m_firstIndices[polygon] + corner])->second;
                    mesh->m_indices[index++] = vertexMap.find(
                        source->m_indices[source->m_firstIndices[polygon] + corner + 1])->second;
                    mesh->m_firstIndices[triangle] = firstIndex;
                    mesh->m_vertexCounts[triangle] = 3;
                    mesh->m_materials[triangle] = source->m_materials[polygon];
                    mesh->m_colours[triangle] = source->m_colours[polygon];
                    ++triangle;
                    firstIndex += 3;
                }
            }
            first = end;
            triangles.push_back(mesh);
            // shipped defect: a polygon exceeding the chunk limit makes no forward progress.
        }
    }

    std::list<DebugMesh*>::iterator i = meshes.begin();
    while (i != meshes.end())
    {
        DebugMesh* mesh = *i;
        i = meshes.erase(i);
        if (mesh)
        {
            static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(mesh->m_mesh);
            mesh->m_mesh = 0;
            delete[] mesh->m_firstIndices;
            delete[] mesh->m_vertexCounts;
            delete[] mesh->m_vertices;
            delete[] mesh->m_indices;
            delete[] mesh->m_materials;
            delete[] mesh->m_colours;
            delete mesh;
        }
    }
    meshes.resize(0, 0);
    for (std::vector<DebugMesh*>::const_iterator mesh = triangles.begin();
         mesh != triangles.end(); ++mesh)
        meshes.push_back(*mesh);
}

// ELF 0x30DF20. Visit islands in vector order and return the first
// whose tree finds a polygon. The polygon output is whatever that tree wrote; a total miss leaves it
// untouched and returns UINT_MAX.
unsigned int QuadTreeManager::findPolygon(const Vec3& point, unsigned int& polygon,
                                          MaterialManager* materials, float allowedHeightDiff)
{
    for (unsigned int island = 0; island < m_islands.size(); ++island)
    {
        if (m_islands[island].first->findPolygon(point, polygon, materials, allowedHeightDiff))
            return island;
    }

    return 0xFFFFFFFFu;
}

// ELF 0x30DFA0. Reuse a known polygon/island only when neither is the
// sentinel and the point still belongs to it; otherwise run the full scan. The island is deliberately
// not bounds-checked before indexing the vector.
unsigned int QuadTreeManager::findPolygon(const Vec3& point, unsigned int& polygon,
                                          unsigned int knownPolygon, unsigned int knownIsland,
                                          MaterialManager* materials, float allowedHeightDiff)
{
    if (knownIsland == 0xFFFFFFFFu || knownPolygon == 0xFFFFFFFFu
        || !m_islands[knownIsland].first->isPointInPolygon(
            point, knownPolygon, allowedHeightDiff, materials))
        return findPolygon(point, polygon, materials, allowedHeightDiff);

    polygon = knownPolygon;
    return knownIsland;
}

// ELF 0x30E080. Every island's debug walk, re-reading the vector size each pass.
void QuadTreeManager::printPolygonsInNode(const Vec3& point, unsigned int& polygon,
                                          MaterialManager* materials, float allowedHeightDiff)
{
    for (unsigned int island = 0; island < m_islands.size(); ++island)
        m_islands[island].first->printPolygonsInNode(point, polygon, materials,
                                                     allowedHeightDiff);
}

// ELF 0x30E0F0.
unsigned short QuadTreeManager::getCluster(unsigned int island, unsigned int polygon) const
{
    return m_islands[island].second->getClusterId(m_islands[island].first, polygon);
}

// ELF 0x30E140.
void QuadTreeManager::getClusterPortals(unsigned int island, unsigned short cluster,
                                        unsigned short costType,
                                        std::list<ClusterPortal*>& portals) const
{
    m_islands[island].second->getClusterPortals(cluster, costType, portals);
}

// ELF 0x30E190. The island's tree only; no bounds check.
void QuadTreeManager::getPolygonsInRadius(std::list<unsigned int>& polygons, unsigned int island,
                                          unsigned int polygon, const Vec3& centre,
                                          float radius) const
{
    m_islands[island].first->getPolygonsInRadius(polygons, polygon, centre, radius);
}

// ELF 0x30E1E0.
void QuadTreeManager::getPolygonsInTriangle(std::list<unsigned int>& polygons, unsigned int island,
                                            unsigned int polygon, const Vec3& a, const Vec3& b,
                                            const Vec3& c) const
{
    m_islands[island].first->getPolygonsInTriangle(polygons, polygon, a, b, c);
}

// ELF 0x30E320. Forward to every island in ascending order, re-reading the vector
// size on each pass. Runtime map parts use the ordinary float-vertex path (`useDoubles=false`).
void QuadTreeManager::initializeDestroyableMapPart(int mapPartId, bool initiallyWalkable,
                                                   const Vec3& minimum, const Vec3& maximum,
                                                   const Mat4& transformation)
{
    for (unsigned int island = 0; island < m_islands.size(); ++island)
        m_islands[island].first->initializeDestroyableMapPart(
            mapPartId, initiallyWalkable, minimum, maximum, transformation,
            m_islands[island].second, false);
}

// ELF 0x30E3C0.
void QuadTreeManager::destroyMapPart(int mapPartId)
{
    for (unsigned int island = 0; island < m_islands.size(); ++island)
        m_islands[island].first->destroyMapPart(mapPartId, m_islands[island].second);
}

// ELF 0x30E420.
void QuadTreeManager::restoreMapPart(int mapPartId)
{
    for (unsigned int island = 0; island < m_islands.size(); ++island)
        m_islands[island].first->restoreMapPart(mapPartId, m_islands[island].second);
}

// ELF 0x30E480.
void QuadTreeManager::resetOriginalMaterialOfDestroyableMapPart(int mapPartId)
{
    for (unsigned int island = 0; island < m_islands.size(); ++island)
        m_islands[island].first->resetOriginalMaterialOfDestroyableMapPart(
            mapPartId, m_islands[island].second);
}

// ELF 0x30E4E0.
void QuadTreeManager::resetDestroyableMapParts()
{
    for (unsigned int island = 0; island < m_islands.size(); ++island)
        m_islands[island].first->resetDestroyableMapParts(m_islands[island].second);
}

// ELF 0x310F30.
void QuadTreeManager::validateNeighbourRelations() const
{
    for (unsigned int island = 0; island < m_islands.size(); ++island)
        m_islands[island].first->validateNeighbourRelations();
}

} // namespace ai
} // namespace hfe
} // namespace dice
