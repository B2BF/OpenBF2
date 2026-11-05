// [TEMP] Focused strict-x86 validation for the recovered destroyable navmesh map-part lifecycle.
// The production QuadTree, QuadTreeManager and Mat4Inverse translation units are compiled and
// linked separately. Private layout is exposed in this fixture only so exact mesh/cluster inputs
// can be installed without adding production test seams.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <crtdbg.h>
#include <windows.h>
#endif

#include "../../BF2/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTree.h"
#include "../../BF2/AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTreeManager.h"
#include "../../BF2/AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../BF2/IO/File/File.h"
#include "../../BF2/Math/Mat4.h"

using dice::hfe::Mat4;
using dice::hfe::Vec3;
using dice::hfe::ai::Cluster;
using dice::hfe::ai::ClusterGraph;
using dice::hfe::ai::DestroyableMapPart;
using dice::hfe::ai::DoubleVec3;
using dice::hfe::ai::QuadTree;
using dice::hfe::ai::QuadTreeManager;
typedef dice::hfe::ai::Polygon NavPolygon;

namespace {

template<class Tag, typename Tag::type Member>
struct PrivateMember
{
    friend typename Tag::type access(Tag) { return Member; }
};

struct TreePolygonCountTag
{
    typedef unsigned int QuadTree::* type;
    friend type access(TreePolygonCountTag);
};
template struct PrivateMember<TreePolygonCountTag, &QuadTree::m_numberOfPolygons>;

struct TreeVertexCountTag
{
    typedef unsigned int QuadTree::* type;
    friend type access(TreeVertexCountTag);
};
template struct PrivateMember<TreeVertexCountTag, &QuadTree::m_vertexCount>;

struct TreeVertexIndexCountTag
{
    typedef unsigned int QuadTree::* type;
    friend type access(TreeVertexIndexCountTag);
};
template struct PrivateMember<TreeVertexIndexCountTag, &QuadTree::m_vertexIndexCount>;

struct TreePolygonsTag
{
    typedef NavPolygon* QuadTree::* type;
    friend type access(TreePolygonsTag);
};
template struct PrivateMember<TreePolygonsTag, &QuadTree::m_polygons>;

struct TreeVerticesTag
{
    typedef Vec3* QuadTree::* type;
    friend type access(TreeVerticesTag);
};
template struct PrivateMember<TreeVerticesTag, &QuadTree::m_vertices>;

struct TreeVertexIndicesTag
{
    typedef unsigned int* QuadTree::* type;
    friend type access(TreeVertexIndicesTag);
};
template struct PrivateMember<TreeVertexIndicesTag, &QuadTree::m_vertexIndices>;

struct TreeDoubleVerticesTag
{
    typedef DoubleVec3* QuadTree::* type;
    friend type access(TreeDoubleVerticesTag);
};
template struct PrivateMember<TreeDoubleVerticesTag, &QuadTree::m_array84>;

struct TreePartsTag
{
    typedef std::vector<DestroyableMapPart*> QuadTree::* type;
    friend type access(TreePartsTag);
};
template struct PrivateMember<TreePartsTag, &QuadTree::m_destroyableMapParts>;

struct GraphClustersTag
{
    typedef Cluster* ClusterGraph::* type;
    friend type access(GraphClustersTag);
};
template struct PrivateMember<GraphClustersTag, &ClusterGraph::m_clusters>;

struct GraphClusterCountTag
{
    typedef unsigned int ClusterGraph::* type;
    friend type access(GraphClusterCountTag);
};
template struct PrivateMember<GraphClusterCountTag, &ClusterGraph::m_numberOfClusters>;

std::vector<DestroyableMapPart*>& parts(QuadTree& tree)
{
    return tree.*access(TreePartsTag());
}

unsigned int g_checks = 0;
unsigned int g_cases = 0;

[[noreturn]] void unexpected(const char* message)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", message);
    std::fflush(stderr);
    std::exit(91);
}

#define CHECK(expression)                                                                  \
    do                                                                                     \
    {                                                                                      \
        ++g_checks;                                                                        \
        if (!(expression))                                                                 \
        {                                                                                  \
            std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expression);     \
            std::fflush(stderr);                                                           \
            std::exit(90);                                                                 \
        }                                                                                  \
    } while (0)

void beginCase()
{
    ++g_cases;
}

std::vector<unsigned int> ids(std::initializer_list<unsigned int> values)
{
    return std::vector<unsigned int>(values);
}

Mat4 identity()
{
    Mat4 value = {};
    value.m[0] = 1.0f;
    value.m[5] = 1.0f;
    value.m[10] = 1.0f;
    value.m[15] = 1.0f;
    return value;
}

Mat4 translation(float x, float y, float z)
{
    Mat4 value = identity();
    value.m[12] = x;
    value.m[13] = y;
    value.m[14] = z;
    return value;
}

Mat4 rotateZ90()
{
    Mat4 value = identity();
    value.m[0] = 0.0f;
    value.m[1] = -1.0f;
    value.m[4] = 1.0f;
    value.m[5] = 0.0f;
    return value;
}

struct TreeFixture
{
    explicit TreeFixture(unsigned int island = 0)
        : tree(new QuadTree(island))
    {
    }

    void addPolygon(unsigned char material, unsigned short cluster,
                    std::initializer_list<Vec3> floatVertices,
                    std::initializer_list<DoubleVec3> doubleVertices = {})
    {
        if (floatVertices.size() > 255)
            unexpected("fixture polygon exceeds unsigned-byte corner count");
        if (doubleVertices.size() != 0 && doubleVertices.size() != floatVertices.size())
            unexpected("fixture float/double vertex counts differ");

        NavPolygon polygon;
        polygon.vertexCount = static_cast<unsigned char>(floatVertices.size());
        polygon.m_material = material;
        polygon.cluster = cluster;
        polygon.vertexOffset = static_cast<unsigned int>(indices.size());
        polygon.m_normal.x = 0.0f;
        polygon.m_normal.y = 1.0f;
        polygon.m_normal.z = 0.0f;
        polygons.push_back(polygon);

        std::initializer_list<DoubleVec3>::const_iterator doubleIt = doubleVertices.begin();
        for (std::initializer_list<Vec3>::const_iterator it = floatVertices.begin();
             it != floatVertices.end(); ++it)
        {
            indices.push_back(static_cast<unsigned int>(vertices.size()));
            vertices.push_back(*it);
            if (doubleVertices.size())
            {
                doubles.push_back(*doubleIt);
                ++doubleIt;
            }
            else
            {
                DoubleVec3 value = {it->x, it->y, it->z};
                doubles.push_back(value);
            }
        }
    }

    void publish()
    {
        tree->*access(TreePolygonCountTag()) = static_cast<unsigned int>(polygons.size());
        tree->*access(TreeVertexCountTag()) = static_cast<unsigned int>(vertices.size());
        tree->*access(TreeVertexIndexCountTag()) = static_cast<unsigned int>(indices.size());
        tree->*access(TreePolygonsTag()) = polygons.empty() ? 0 : &polygons[0];
        tree->*access(TreeVerticesTag()) = vertices.empty() ? 0 : &vertices[0];
        tree->*access(TreeVertexIndicesTag()) = indices.empty() ? 0 : &indices[0];
        tree->*access(TreeDoubleVerticesTag()) = doubles.empty() ? 0 : &doubles[0];
    }

    QuadTree* tree;
    std::vector<NavPolygon> polygons;
    std::vector<Vec3> vertices;
    std::vector<DoubleVec3> doubles;
    std::vector<unsigned int> indices;
};

struct GraphFixture
{
    explicit GraphFixture(unsigned int count)
        : graph(new ClusterGraph)
        , clusters(count)
    {
        for (unsigned int i = 0; i < count; ++i)
        {
            for (unsigned int byte = 0; byte < sizeof(clusters[i].m_unread00); ++byte)
                clusters[i].m_unread00[byte] = 0;
            clusters[i].m_firstPortal = 0;
            clusters[i].m_numberOfPortals = 0;
            for (unsigned int byte = 0; byte < sizeof(clusters[i].m_unread0C); ++byte)
                clusters[i].m_unread0C[byte] = 0;
        }
        graph->*access(GraphClustersTag()) = clusters.empty() ? 0 : &clusters[0];
        graph->*access(GraphClusterCountTag()) = static_cast<unsigned int>(clusters.size());
    }

    unsigned char& state(unsigned int cluster)
    {
        return clusters[cluster].m_unread0C[2];
    }

    void fill(unsigned char value)
    {
        for (unsigned int i = 0; i < clusters.size(); ++i)
            state(i) = value;
    }

    ClusterGraph* graph;
    std::vector<Cluster> clusters;
};

DestroyableMapPart* part(QuadTree* tree, unsigned int id)
{
    return parts(*tree)[id];
}

void testOrdinaryBoundarySelection()
{
    beginCase();
    const Vec3 minimum = {-1.0f, -1.0f, -1.0f};
    const Vec3 maximum = {1.0f, 1.0f, 1.0f};
    const Mat4 transform = identity();

    TreeFixture empty;
    empty.publish();
    empty.tree->initializeDestroyableMapPart(37, true, minimum, maximum, transform, 0, false);
    CHECK(parts(*empty.tree).size() == 1);
    CHECK(part(empty.tree, 0)->m_polygons.empty());

    TreeFixture fixture;
    fixture.addPolygon(1, 0, {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}});
    fixture.addPolygon(2, 0, {{2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}});
    fixture.addPolygon(3, 0, {});
    fixture.addPolygon(4, 0, {{1.0f, 1.0f, 1.0f}});
    fixture.addPolygon(5, 0, {{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}});
    // The segment crosses the box, but neither endpoint is inside. Retail performs no edge/AABB test.
    fixture.addPolygon(6, 0, {{-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}});
    fixture.publish();

    fixture.tree->initializeDestroyableMapPart(0, true, minimum, maximum, transform, 0, false);
    fixture.tree->initializeDestroyableMapPart(1, true, minimum, maximum, transform, 0, false);
    CHECK(parts(*fixture.tree).size() == 2);
    CHECK(part(fixture.tree, 0)->m_polygons == ids({0, 3}));
    CHECK(part(fixture.tree, 1)->m_polygons == ids({0, 3}));
}

void testInverseTransformSelection()
{
    beginCase();
    TreeFixture translated;
    translated.addPolygon(1, 0, {{10.0f, 2.0f, -3.0f}});
    translated.addPolygon(2, 0, {{11.0f, 2.0f, -3.0f}});
    translated.publish();
    const Vec3 origin = {0.0f, 0.0f, 0.0f};
    translated.tree->initializeDestroyableMapPart(
        0, true, origin, origin, translation(10.0f, 2.0f, -3.0f), 0, false);
    CHECK(part(translated.tree, 0)->m_polygons == ids({0}));

    TreeFixture rotated;
    rotated.addPolygon(1, 0, {{0.0f, -0.5f, 0.0f}});
    rotated.addPolygon(2, 0, {{0.5f, 0.0f, 0.0f}});
    rotated.publish();
    const Vec3 local = {0.5f, 0.0f, 0.0f};
    rotated.tree->initializeDestroyableMapPart(0, true, local, local, rotateZ90(), 0, false);
    CHECK(part(rotated.tree, 0)->m_polygons == ids({0}));
}

void testDoubleBoundarySelection()
{
    beginCase();
    const Vec3 minimum = {-1.0f, -1.0f, -1.0f};
    const Vec3 maximum = {1.0f, 1.0f, 1.0f};

    TreeFixture same;
    same.addPolygon(1, 0, {{-1.0f, -1.0f, -1.0f}});
    same.addPolygon(2, 0, {{1.0f, 1.0f, 1.0f}});
    same.addPolygon(3, 0, {{2.0f, 0.0f, 0.0f}});
    same.publish();
    same.tree->initializeDestroyableMapPart(0, true, minimum, maximum, identity(), 0, false);
    same.tree->initializeDestroyableMapPart(1, true, minimum, maximum, identity(), 0, true);
    CHECK(part(same.tree, 0)->m_polygons == ids({0, 1}));
    CHECK(part(same.tree, 1)->m_polygons == part(same.tree, 0)->m_polygons);

    TreeFixture distinct;
    distinct.addPolygon(1, 0, {{0.0f, 0.0f, 0.0f}}, {{9.0, 0.0, 0.0}});
    distinct.addPolygon(2, 0, {{9.0f, 0.0f, 0.0f}}, {{0.0, 0.0, 0.0}});
    distinct.publish();
    distinct.tree->initializeDestroyableMapPart(0, true, minimum, maximum, identity(), 0, false);
    distinct.tree->initializeDestroyableMapPart(1, true, minimum, maximum, identity(), 0, true);
    CHECK(part(distinct.tree, 0)->m_polygons == ids({0}));
    CHECK(part(distinct.tree, 1)->m_polygons == ids({1}));

    TreeFixture rounded;
    rounded.addPolygon(1, 0, {{9.0f, 0.0f, 0.0f}}, {{1.00000001, 0.0, 0.0}});
    rounded.publish();
    rounded.tree->initializeDestroyableMapPart(0, true, minimum, maximum, identity(), 0, true);
    CHECK(part(rounded.tree, 0)->m_polygons == ids({0}));
}

void testInitializerBranchesAndMaterials()
{
    beginCase();
    const Vec3 minimum = {-1.0f, -1.0f, -1.0f};
    const Vec3 maximum = {1.0f, 1.0f, 1.0f};

    TreeFixture zeroTranslation;
    zeroTranslation.addPolygon(7, 0, {{0.0f, 0.0f, 0.0f}});
    zeroTranslation.publish();
    GraphFixture zeroGraph(1);
    zeroGraph.state(0) = 0x55;
    zeroTranslation.tree->initializeDestroyableMapPart(
        0, false, minimum, maximum, rotateZ90(), zeroGraph.graph, false);
    CHECK(parts(*zeroTranslation.tree).size() == 1);
    CHECK(part(zeroTranslation.tree, 0)->m_polygons == ids({0}));
    CHECK(part(zeroTranslation.tree, 0)->m_originalMaterial == 0);
    CHECK(!part(zeroTranslation.tree, 0)->m_initiallyWalkable);
    CHECK(!part(zeroTranslation.tree, 0)->m_destroyed);
    CHECK(zeroTranslation.polygons[0].m_material == 7);
    CHECK(zeroGraph.state(0) == 0x55);

    TreeFixture materialScan;
    materialScan.addPolygon(0, 1, {{10.0f, 0.0f, 0.0f}});
    materialScan.addPolygon(6, 2, {{10.2f, 0.0f, 0.0f}});
    materialScan.addPolygon(9, 0, {{10.4f, 0.0f, 0.0f}});
    materialScan.publish();
    GraphFixture scanGraph(3);
    scanGraph.fill(0x66);
    materialScan.tree->initializeDestroyableMapPart(
        0, false, minimum, maximum, translation(10.0f, 0.0f, 0.0f), scanGraph.graph, false);
    CHECK(part(materialScan.tree, 0)->m_polygons == ids({0, 1, 2}));
    CHECK(part(materialScan.tree, 0)->m_originalMaterial == 6);
    CHECK(materialScan.polygons[0].m_material == 0);
    CHECK(materialScan.polygons[1].m_material == 0);
    CHECK(materialScan.polygons[2].m_material == 0);
    CHECK(scanGraph.state(1) == 0);
    CHECK(scanGraph.state(0) == 0x66 && scanGraph.state(2) == 0x66);

    TreeFixture firstMaterial;
    firstMaterial.addPolygon(5, 0, {{10.0f, 0.0f, 0.0f}});
    firstMaterial.addPolygon(8, 0, {{10.5f, 0.0f, 0.0f}});
    firstMaterial.publish();
    firstMaterial.tree->initializeDestroyableMapPart(
        0, true, minimum, maximum, translation(10.0f, 0.0f, 0.0f), 0, false);
    CHECK(part(firstMaterial.tree, 0)->m_originalMaterial == 5);

    TreeFixture allZero;
    allZero.addPolygon(0, 0, {{10.0f, 0.0f, 0.0f}});
    allZero.addPolygon(0, 0, {{10.5f, 0.0f, 0.0f}});
    allZero.publish();
    allZero.tree->initializeDestroyableMapPart(
        0, true, minimum, maximum, translation(10.0f, 0.0f, 0.0f), 0, false);
    CHECK(part(allZero.tree, 0)->m_originalMaterial == 0);

    TreeFixture empty;
    empty.publish();
    GraphFixture emptyGraph(1);
    emptyGraph.state(0) = 0x72;
    empty.tree->initializeDestroyableMapPart(
        0, false, minimum, maximum, translation(10.0f, 0.0f, 0.0f), emptyGraph.graph, false);
    CHECK(parts(*empty.tree).size() == 1);
    CHECK(part(empty.tree, 0)->m_polygons.empty());
    CHECK(part(empty.tree, 0)->m_originalMaterial == 0);
    CHECK(emptyGraph.state(0) == 0x72);

    TreeFixture doubleInit;
    doubleInit.addPolygon(5, 0, {{100.0f, 0.0f, 0.0f}}, {{10.0, 0.0, 0.0}});
    doubleInit.publish();
    GraphFixture doubleGraph(1);
    doubleGraph.state(0) = 0x73;
    doubleInit.tree->initializeDestroyableMapPart(
        0, false, minimum, maximum, translation(10.0f, 0.0f, 0.0f), doubleGraph.graph, true);
    CHECK(part(doubleInit.tree, 0)->m_polygons == ids({0}));
    CHECK(part(doubleInit.tree, 0)->m_originalMaterial == 5);
    CHECK(doubleInit.polygons[0].m_material == 5);
    CHECK(doubleGraph.state(0) == 0x73);
}

void testWalkableTransitionsAndClusterSelection()
{
    beginCase();
    const Vec3 minimum = {-1.0f, -1.0f, -1.0f};
    const Vec3 maximum = {1.0f, 1.0f, 1.0f};
    const Mat4 transform = translation(10.0f, 0.0f, 0.0f);

    TreeFixture walkable;
    walkable.addPolygon(9, 1, {{10.0f, 0.0f, 0.0f}});
    walkable.addPolygon(4, 0, {{10.5f, 0.0f, 0.0f}});
    walkable.publish();
    GraphFixture walkableGraph(2);
    walkableGraph.fill(0x30);
    walkable.tree->initializeDestroyableMapPart(
        0, true, minimum, maximum, transform, walkableGraph.graph, false);
    CHECK(part(walkable.tree, 0)->m_originalMaterial == 9);
    CHECK(walkable.polygons[0].m_material == 9 && walkable.polygons[1].m_material == 4);

    walkable.tree->destroyMapPart(0, walkableGraph.graph);
    CHECK(walkable.polygons[0].m_material == 0 && walkable.polygons[1].m_material == 0);
    CHECK(walkableGraph.state(1) == 0 && walkableGraph.state(0) == 0x30);
    CHECK(part(walkable.tree, 0)->m_destroyed);

    walkable.polygons[0].m_material = 3;
    walkable.polygons[1].m_material = 4;
    walkableGraph.state(1) = 0x31;
    walkable.tree->destroyMapPart(0, walkableGraph.graph);
    CHECK(walkable.polygons[0].m_material == 3 && walkable.polygons[1].m_material == 4);
    CHECK(walkableGraph.state(1) == 0x31);

    walkable.tree->restoreMapPart(0, walkableGraph.graph);
    CHECK(walkable.polygons[0].m_material == 9 && walkable.polygons[1].m_material == 9);
    CHECK(walkableGraph.state(1) == 1 && walkableGraph.state(0) == 0x30);
    CHECK(!part(walkable.tree, 0)->m_destroyed);

    walkable.polygons[0].m_material = 2;
    walkableGraph.state(1) = 0x32;
    walkable.tree->restoreMapPart(0, walkableGraph.graph);
    CHECK(walkable.polygons[0].m_material == 9 && walkableGraph.state(1) == 1);

    walkable.tree->destroyMapPart(0, walkableGraph.graph);
    walkable.tree->resetOriginalMaterialOfDestroyableMapPart(0, walkableGraph.graph);
    CHECK(walkable.polygons[0].m_material == 9 && walkable.polygons[1].m_material == 9);
    CHECK(walkableGraph.state(1) == 1 && !part(walkable.tree, 0)->m_destroyed);

    walkable.polygons[0].m_material = 2;
    walkableGraph.state(1) = 0x33;
    part(walkable.tree, 0)->m_destroyed = true;
    walkable.tree->restoreMapPart(-1, walkableGraph.graph);
    walkable.tree->resetOriginalMaterialOfDestroyableMapPart(-1, walkableGraph.graph);
    CHECK(walkable.polygons[0].m_material == 2 && walkableGraph.state(1) == 0x33);
    CHECK(part(walkable.tree, 0)->m_destroyed);

    part(walkable.tree, 0)->m_destroyed = false;
    walkable.tree->destroyMapPart(0, 0);
    CHECK(walkable.polygons[0].m_material == 0 && part(walkable.tree, 0)->m_destroyed);
    CHECK(walkableGraph.state(1) == 0x33);
    walkable.tree->resetOriginalMaterialOfDestroyableMapPart(0, 0);
    CHECK(walkable.polygons[0].m_material == 9 && !part(walkable.tree, 0)->m_destroyed);

    TreeFixture blocked;
    blocked.addPolygon(8, 0, {{10.0f, 0.0f, 0.0f}});
    blocked.publish();
    GraphFixture blockedGraph(1);
    blocked.tree->initializeDestroyableMapPart(
        0, false, minimum, maximum, transform, blockedGraph.graph, false);
    CHECK(blocked.polygons[0].m_material == 0 && blockedGraph.state(0) == 0);
    blocked.tree->destroyMapPart(0, blockedGraph.graph);
    CHECK(blocked.polygons[0].m_material == 8 && blockedGraph.state(0) == 1);
    blocked.tree->restoreMapPart(0, blockedGraph.graph);
    CHECK(blocked.polygons[0].m_material == 0 && blockedGraph.state(0) == 0);
    blocked.tree->destroyMapPart(0, blockedGraph.graph);
    blocked.tree->resetOriginalMaterialOfDestroyableMapPart(0, blockedGraph.graph);
    CHECK(blocked.polygons[0].m_material == 8 && blockedGraph.state(0) == 0);
    CHECK(!part(blocked.tree, 0)->m_destroyed);
}

void testEmptyNullAndResetOrder()
{
    beginCase();
    GraphFixture graph(1);
    graph.state(0) = 0x41;

    TreeFixture emptyVector;
    emptyVector.publish();
    emptyVector.tree->destroyMapPart(0, graph.graph);
    CHECK(graph.state(0) == 0x41);

    TreeFixture nullRecord;
    nullRecord.publish();
    parts(*nullRecord.tree).push_back(0);
    nullRecord.tree->destroyMapPart(0, graph.graph);
    nullRecord.tree->restoreMapPart(0, graph.graph);
    nullRecord.tree->resetOriginalMaterialOfDestroyableMapPart(0, graph.graph);
    nullRecord.tree->resetDestroyableMapParts(graph.graph);
    CHECK(graph.state(0) == 0x41);

    const Vec3 minimum = {-1.0f, -1.0f, -1.0f};
    const Vec3 maximum = {1.0f, 1.0f, 1.0f};
    const Mat4 transform = translation(10.0f, 0.0f, 0.0f);
    TreeFixture order;
    order.addPolygon(9, 0, {{10.0f, 0.0f, 0.0f}});
    order.publish();
    order.tree->initializeDestroyableMapPart(0, true, minimum, maximum, transform, graph.graph, false);
    order.tree->initializeDestroyableMapPart(1, false, minimum, maximum, transform, graph.graph, false);
    CHECK(parts(*order.tree).size() == 2);
    CHECK(part(order.tree, 0)->m_originalMaterial == 9);
    CHECK(part(order.tree, 1)->m_originalMaterial == 9);
    order.polygons[0].m_material = 4;
    graph.state(0) = 0x42;
    part(order.tree, 0)->m_destroyed = true;
    part(order.tree, 1)->m_destroyed = true;
    order.tree->resetDestroyableMapParts(graph.graph);
    // Ascending order is observable: id 0 restores 9, then blocked id 1 restores 0.
    CHECK(order.polygons[0].m_material == 0);
    CHECK(graph.state(0) == 0);
    CHECK(!part(order.tree, 0)->m_destroyed && !part(order.tree, 1)->m_destroyed);

    TreeFixture emptyPart;
    emptyPart.publish();
    emptyPart.tree->initializeDestroyableMapPart(
        0, true, minimum, maximum, transform, graph.graph, false);
    graph.state(0) = 0x43;
    emptyPart.tree->destroyMapPart(0, graph.graph);
    CHECK(part(emptyPart.tree, 0)->m_destroyed && graph.state(0) == 0x43);
    emptyPart.tree->restoreMapPart(0, graph.graph);
    CHECK(!part(emptyPart.tree, 0)->m_destroyed && graph.state(0) == 0x43);
}

void testManagerPairingAndForwarders()
{
    beginCase();
    const Vec3 minimum = {-1.0f, -1.0f, -1.0f};
    const Vec3 maximum = {1.0f, 1.0f, 1.0f};
    const Mat4 transform = translation(10.0f, 0.0f, 0.0f);

    TreeFixture first(0);
    first.addPolygon(4, 0, {{10.0f, 0.0f, 0.0f}}, {{100.0, 0.0, 0.0}});
    first.publish();
    TreeFixture second(1);
    second.addPolygon(8, 1, {{10.0f, 0.0f, 0.0f}}, {{100.0, 0.0, 0.0}});
    second.publish();
    GraphFixture firstGraph(2);
    GraphFixture secondGraph(2);
    firstGraph.fill(0x51);
    secondGraph.fill(0x52);

    QuadTreeManager* manager = new QuadTreeManager;
    manager->m_quadTrees.push_back(std::make_pair(first.tree, firstGraph.graph));
    manager->m_quadTrees.push_back(std::make_pair(second.tree, secondGraph.graph));
    manager->initializeDestroyableMapPart(0, false, minimum, maximum, transform);
    CHECK(parts(*first.tree).size() == 1);
    CHECK(parts(*second.tree).size() == 1);
    CHECK(part(first.tree, 0)->m_polygons == ids({0}));
    CHECK(part(second.tree, 0)->m_polygons == ids({0}));
    CHECK(first.polygons[0].m_material == 0 && second.polygons[0].m_material == 0);
    CHECK(firstGraph.state(0) == 0 && firstGraph.state(1) == 0x51);
    CHECK(secondGraph.state(0) == 0x52 && secondGraph.state(1) == 0);

    firstGraph.fill(0x53);
    secondGraph.fill(0x54);
    manager->destroyMapPart(0);
    CHECK(first.polygons[0].m_material == 4 && second.polygons[0].m_material == 8);
    CHECK(firstGraph.state(0) == 1 && firstGraph.state(1) == 0x53);
    CHECK(secondGraph.state(0) == 0x54 && secondGraph.state(1) == 1);

    firstGraph.fill(0x55);
    secondGraph.fill(0x56);
    manager->destroyMapPart(0);
    CHECK(firstGraph.state(0) == 0x55 && secondGraph.state(1) == 0x56);

    manager->restoreMapPart(0);
    CHECK(first.polygons[0].m_material == 0 && second.polygons[0].m_material == 0);
    CHECK(firstGraph.state(0) == 0 && firstGraph.state(1) == 0x55);
    CHECK(secondGraph.state(0) == 0x56 && secondGraph.state(1) == 0);

    first.polygons[0].m_material = 2;
    second.polygons[0].m_material = 3;
    firstGraph.fill(0x57);
    secondGraph.fill(0x58);
    manager->resetOriginalMaterialOfDestroyableMapPart(0);
    CHECK(first.polygons[0].m_material == 4 && second.polygons[0].m_material == 8);
    CHECK(firstGraph.state(0) == 0 && firstGraph.state(1) == 0x57);
    CHECK(secondGraph.state(0) == 0x58 && secondGraph.state(1) == 0);

    manager->destroyMapPart(0);
    firstGraph.fill(0x59);
    secondGraph.fill(0x5A);
    manager->resetDestroyableMapParts();
    CHECK(first.polygons[0].m_material == 0 && second.polygons[0].m_material == 0);
    CHECK(firstGraph.state(0) == 0 && firstGraph.state(1) == 0x59);
    CHECK(secondGraph.state(0) == 0x5A && secondGraph.state(1) == 0);

    QuadTreeManager* empty = new QuadTreeManager;
    empty->initializeDestroyableMapPart(0, true, minimum, maximum, identity());
    empty->destroyMapPart(0);
    empty->restoreMapPart(0);
    empty->resetOriginalMaterialOfDestroyableMapPart(0);
    empty->resetDestroyableMapParts();
    CHECK(empty->m_quadTrees.empty());

    TreeFixture noPolygonsA(0);
    TreeFixture noPolygonsB(1);
    noPolygonsA.publish();
    noPolygonsB.publish();
    QuadTreeManager* emptyTrees = new QuadTreeManager;
    emptyTrees->m_quadTrees.push_back(std::make_pair(noPolygonsA.tree, firstGraph.graph));
    emptyTrees->m_quadTrees.push_back(std::make_pair(noPolygonsB.tree, secondGraph.graph));
    emptyTrees->initializeDestroyableMapPart(0, true, minimum, maximum, transform);
    CHECK(parts(*noPolygonsA.tree).size() == 1);
    CHECK(parts(*noPolygonsB.tree).size() == 1);
    CHECK(part(noPolygonsA.tree, 0)->m_polygons.empty());
    CHECK(part(noPolygonsB.tree, 0)->m_polygons.empty());
}

void testManagerAscendingOrder()
{
    beginCase();
    const Vec3 minimum = {-1.0f, -1.0f, -1.0f};
    const Vec3 maximum = {1.0f, 1.0f, 1.0f};
    TreeFixture fixture;
    fixture.addPolygon(7, 0, {{10.0f, 0.0f, 0.0f}});
    fixture.publish();
    fixture.tree->initializeDestroyableMapPart(
        0, true, minimum, maximum, translation(10.0f, 0.0f, 0.0f), 0, false);

    GraphFixture firstGraph(1);
    GraphFixture secondGraph(1);
    firstGraph.state(0) = 0x61;
    secondGraph.state(0) = 0x62;
    QuadTreeManager* manager = new QuadTreeManager;
    manager->m_quadTrees.push_back(std::make_pair(fixture.tree, firstGraph.graph));
    manager->m_quadTrees.push_back(std::make_pair(fixture.tree, secondGraph.graph));
    manager->destroyMapPart(0);
    // The first island destroys the shared record; the second sees destroyed=true and does nothing.
    CHECK(firstGraph.state(0) == 0);
    CHECK(secondGraph.state(0) == 0x62);
    CHECK(part(fixture.tree, 0)->m_destroyed);
}

} // namespace

// [TEMP] Whole-TU strict-link seams. The fixture never enters loader or draw paths; every operation
// that would make one observable stops visibly. Constructors merely establish their documented
// empty value so exception cleanup remains valid if a compiler references them.
namespace dice {
namespace hfe {
namespace io {

File::File() : m_file(0) {}
File::~File() {}

bool File::open(const std::string&, IFileSystem::FileMode, IFileSystem::FileAccess, unsigned int)
{
    unexpected("unused File::open reached");
}

void File::close()
{
    unexpected("unused File::close reached");
}

unsigned int File::read(void*, unsigned int)
{
    unexpected("unused File::read(void*, unsigned int) reached");
}

bool File::read(unsigned int&)
{
    unexpected("unused File::read(unsigned int&) reached");
}

bool File::read(int&)
{
    unexpected("unused File::read(int&) reached");
}

bool File::read(float&)
{
    unexpected("unused File::read(float&) reached");
}

} // namespace io

namespace ai {
void* CommonSingletonServer::m_iDPManager = 0;
void* CommonSingletonServer::m_iaiGame = 0;
} // namespace ai
} // namespace hfe
} // namespace dice

int main()
{
#if defined(_WIN32)
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
#endif

    testOrdinaryBoundarySelection();
    testInverseTransformSelection();
    testDoubleBoundarySelection();
    testInitializerBranchesAndMaterials();
    testWalkableTransitionsAndClusterSelection();
    testEmptyNullAndResetOrder();
    testManagerPairingAndForwarders();
    testManagerAscendingOrder();

    std::printf("PASS destroyable_map_part_runtime %u checks across %u cases\n", g_checks, g_cases);
    return 0;
}
