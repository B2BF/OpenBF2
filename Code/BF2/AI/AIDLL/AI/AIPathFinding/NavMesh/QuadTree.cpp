// Navigation quad-tree queries, GTS import, construction and destroyable-map-part state.

#include "QuadTree.h"
#include "GTSDataOptimiser.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x309940, __tcf_0 0x3099A0, _GLOBAL__I 0x3099D0).
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>                                      // drawPolygonIds

#include "../Cluster/ClusterGraph.h"
#include "../Material/MaterialManager.h"
#include "../../AIUtils/checkPointer.h"                 // and, through it, Core/Debug.h
#include "../../DrawPrimitives/DPManager.h"             // IDPManager::deleteMesh
#include "../../Main/CommonSingletonServer.h"
#include "../../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../../../IO/File/File.h"
#include "../../../../../Math/Vec3.h"                   // vec3ToVec2XZ, lineIntersect, pointWithinPolygon2d
#include "../../../../../Math/Mat4.h"
#include "../../../../../Math/Plane3.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF 0x300ED0.
void setAllMaterialsToGround(unsigned int count, Polygon* polygons)
{
    for (unsigned int i = 0; i < count; ++i)
        polygons[i].m_material = 1;
}

// [INFERRED] Helper name/grouping for the inlined normalisation at ELF 0x30255B,
// 0x3037BC and 0x303869. Both precisions use the single-precision epsilon.
template<class Vector, class Scalar>
void normalizeGTSNormal(Vector& normal, Scalar)
{
    const Scalar squared = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
    const Scalar epsilon = 1.1920928955078125e-7f;
    if (std::fabs(squared - Scalar(1)) < epsilon)
        return;
    if (std::fabs(squared) < epsilon)
    {
        normal.x = 0;
        normal.y = 0;
        normal.z = 0;
        return;
    }
    const Scalar inverse = Scalar(1) / std::sqrt(squared);
    normal.y *= inverse;
    normal.z *= inverse;
    normal.x *= inverse;
}

} // namespace

// ELF 0x3031C0. Retains double vertices and normals for the subsequent optimiser.
void QuadTree::readGTSDataFromFileDoubles(io::File& file)
{
    clean();
    file.read(m_numberOfVertices);
    file.read(m_numberOfPolygons);
    m_numberOfVertexIndices = m_numberOfPolygons * 3;
    m_numberOfNeighbours = m_numberOfPolygons * 3;
    file.read(m_double8C);
    file.read(m_double94);
    file.read(m_double9C);
    file.read(m_doubleA4);
    const double xSize = m_double9C - m_double8C;
    const double zSize = m_doubleA4 - m_double94;
    m_doubleAC = zSize > xSize ? zSize : xSize;
    m_polygons = checkPointer(new Polygon[m_numberOfPolygons]);
    m_array88 = checkPointer(new DoublePolygon[m_numberOfPolygons]);
    m_array84 = checkPointer(new DoubleVec3[m_numberOfVertices]());
    m_vertexIndices = checkPointer(new unsigned int[m_numberOfVertexIndices]);
    m_neighbours = checkPointer(new unsigned int[m_numberOfNeighbours]);
    file.read(m_array84, m_numberOfVertices * 24);
    file.read(m_vertexIndices, m_numberOfVertexIndices * 4);
    std::vector<unsigned int> polygons;
    DoubleVec3* normals = checkPointer(new DoubleVec3[m_numberOfPolygons]());
    file.read(normals, m_numberOfPolygons * 24);
    unsigned char* materials = checkPointer(new unsigned char[m_numberOfPolygons]);
    file.read(materials, m_numberOfPolygons);
    IAIObjectEnvironment* environment = static_cast<IAIObjectEnvironment*>(
        CommonSingletonServer::m_iaiObjectEnvironment);
    environment->initAiCombatAreas();
    for (unsigned int p = 0; p < m_numberOfPolygons; ++p)
    {
        polygons.push_back(p);
        m_array88[p].m_numberOfVertices = 3;
        m_array88[p].m_firstIndex = p * 3;
        m_polygons[p].m_numberOfVertices = 3;
        m_polygons[p].m_firstIndex = p * 3;
        const DoubleVec3& av = m_array84[m_vertexIndices[p * 3]];
        const DoubleVec3& bv = m_array84[m_vertexIndices[p * 3 + 1]];
        const DoubleVec3& cv = m_array84[m_vertexIndices[p * 3 + 2]];
        const Vec3 a = {static_cast<float>(av.x), static_cast<float>(av.y), static_cast<float>(av.z)};
        const Vec3 b = {static_cast<float>(bv.x), static_cast<float>(bv.y), static_cast<float>(bv.z)};
        const Vec3 c = {static_cast<float>(cv.x), static_cast<float>(cv.y), static_cast<float>(cv.z)};
        if (environment->isAiPositionInsideCombatArea(a) &&
            environment->isAiPositionInsideCombatArea(b) &&
            environment->isAiPositionInsideCombatArea(c))
        {
            m_array88[p].m_material = static_cast<unsigned char>(materials[p] + 1);
            m_polygons[p].m_material = static_cast<unsigned char>(materials[p] + 1);
        }
        else
        {
            m_array88[p].m_material = 0;
            m_polygons[p].m_material = 0;
        }
        m_array88[p].m_normal = normals[p];
        normalizeGTSNormal(m_array88[p].m_normal, double());
        m_polygons[p].m_normal = Vec3{static_cast<float>(normals[p].x),
            static_cast<float>(normals[p].y), static_cast<float>(normals[p].z)};
        normalizeGTSNormal(m_polygons[p].m_normal, float());
    }
    delete[] normals;
    delete[] materials;
    file.read(m_neighbours, m_numberOfNeighbours * 4);
}

// ELF 0x302130. The single-precision reader constructs the tree immediately.
void QuadTree::readGTSDataFromFile(io::File& file)
{
    clean();
    file.read(m_numberOfVertices);
    file.read(m_numberOfPolygons);
    m_numberOfVertexIndices = m_numberOfPolygons * 3;
    m_numberOfNeighbours = m_numberOfPolygons * 3;
    file.read(m_min);
    file.read(m_max);
    const float xSize = m_max.x - m_min.x;
    const float zSize = m_max.y - m_min.y;
    m_size = zSize > xSize ? zSize : xSize;
    m_polygons = checkPointer(new Polygon[m_numberOfPolygons]);
    m_vertices = checkPointer(new Vec3[m_numberOfVertices]());
    m_vertexIndices = checkPointer(new unsigned int[m_numberOfVertexIndices]);
    m_neighbours = checkPointer(new unsigned int[m_numberOfNeighbours]);
    file.read(m_vertices, m_numberOfVertices * 12);
    file.read(m_vertexIndices, m_numberOfVertexIndices * 4);
    std::vector<unsigned int> polygons;
    Vec3* normals = checkPointer(new Vec3[m_numberOfPolygons]());
    file.read(normals, m_numberOfPolygons * 12);
    unsigned char* materials = checkPointer(new unsigned char[m_numberOfPolygons]);
    file.read(materials, m_numberOfPolygons);
    IAIObjectEnvironment* environment = static_cast<IAIObjectEnvironment*>(
        CommonSingletonServer::m_iaiObjectEnvironment);
    environment->initAiCombatAreas();
    for (unsigned int p = 0; p < m_numberOfPolygons; ++p)
    {
        polygons.push_back(p);
        m_polygons[p].m_numberOfVertices = 3;
        m_polygons[p].m_firstIndex = p * 3;
        if (environment->isAiPositionInsideCombatArea(m_vertices[m_vertexIndices[p * 3]]) &&
            environment->isAiPositionInsideCombatArea(m_vertices[m_vertexIndices[p * 3 + 1]]) &&
            environment->isAiPositionInsideCombatArea(m_vertices[m_vertexIndices[p * 3 + 2]]))
            m_polygons[p].m_material = static_cast<unsigned char>(materials[p] + 1);
        else
            m_polygons[p].m_material = 0;
        m_polygons[p].m_normal = normals[p];
        normalizeGTSNormal(m_polygons[p].m_normal, float());
    }
    delete[] normals;
    delete[] materials;
    file.read(m_neighbours, m_numberOfNeighbours * 4);
    buildQuadTree(polygons);
}

// ELF 0x304160.
void QuadTree::optimiseGTSData(ClusterGraph* graph)
{
    validateNeighbourRelations();
    GTSDataOptimiser* optimiser = checkPointer(new GTSDataOptimiser(graph));
    optimiser->buildOptMeshFromTriangles(m_array88, m_array84, m_vertexIndices, m_neighbours,
                                        m_numberOfPolygons, this);
    optimiser->optimise(m_destroyableMapParts);
    clean();
    buildQuadTreeMeshFromOptMesh(optimiser);
    delete optimiser;
}

// ELF 0x304B80; the same construction is inlined into ELF 0x302661..0x302869.
void QuadTree::buildQuadTree(const std::vector<unsigned int>& polygons)
{
    m_nodes = checkPointer(new QuadTreeNode[0x1555A]);
    m_numberOfNodes = 0;
    m_nodes->create(this, m_numberOfNodes, m_min, m_size, 0, polygons, m_numberOfPolygons);
    QuadTreeNode* nodes = checkPointer(new QuadTreeNode[m_numberOfNodes]);
    for (unsigned int i = 0; i < m_numberOfNodes; ++i)
        nodes[i] = m_nodes[i];
    delete[] m_nodes;
    m_nodes = nodes;
    m_numberOfNodePolygons = static_cast<unsigned int>(m_vector18.size());
    m_nodePolygons = checkPointer(new unsigned int[m_numberOfNodePolygons]);
    for (unsigned int i = 0; i < m_numberOfNodePolygons; ++i)
        m_nodePolygons[i] = m_vector18[i];
    m_vector18.erase(m_vector18.begin(), m_vector18.end());
}

// ELF 0x3069C0.
DoublePolygon* QuadTree::getPolygonDouble(unsigned int polygon) const
{
    return polygon < m_numberOfPolygons ? m_array88 + polygon : 0;
}

// ELF 0x304350. The optimiser owns the source polygons; this tree receives independent arrays.
void QuadTree::buildQuadTreeMeshFromOptMesh(GTSDataOptimiser* optimiser)
{
    m_numberOfPolygons = static_cast<unsigned int>(optimiser->m_polygons.size());
    m_numberOfVertices = optimiser->getNumberOfVertices();
    m_numberOfVertexIndices = m_numberOfVertices;
    m_numberOfNeighbours = m_numberOfVertices;
    m_min.x = static_cast<float>(m_double8C);
    m_min.y = static_cast<float>(m_double94);
    m_max.x = static_cast<float>(m_double9C);
    m_max.y = static_cast<float>(m_doubleA4);
    const float xSize = m_max.x - m_min.x;
    const float zSize = m_max.y - m_min.y;
    m_size = zSize > xSize ? zSize : xSize;
    delete[] m_polygons;
    m_polygons = checkPointer(new Polygon[m_numberOfPolygons]);
    delete[] m_vertices;
    m_vertices = checkPointer(new Vec3[m_numberOfVertices]());
    delete[] m_vertexIndices;
    m_vertexIndices = checkPointer(new unsigned int[m_numberOfVertexIndices]);
    delete[] m_neighbours;
    m_neighbours = checkPointer(new unsigned int[m_numberOfNeighbours]);
    std::vector<unsigned int> polygons;
    unsigned int polygon = 0;
    unsigned int first = 0;
    for (OptPolygon* p = optimiser->initPolygonIteration(); p; p = optimiser->getNextPolygon())
    {
        if (p->m_material != 0)
            p->m_material = p->m_originalMaterial;
        polygons.push_back(polygon);
        m_polygons[polygon].m_numberOfVertices = p->m_numberOfVertices;
        m_polygons[polygon].m_firstIndex = first;
        m_polygons[polygon].m_material = p->m_material;
        const DoubleVec3* normal = p->getNormal();
        m_polygons[polygon].m_normal = Vec3{static_cast<float>(normal->x),
            static_cast<float>(normal->y), static_cast<float>(normal->z)};
        m_polygons[polygon].m_cluster = p->m_cluster;
        const unsigned char count = p->m_numberOfVertices;
        for (unsigned char corner = 0; corner < count; ++corner)
        {
            const unsigned int index = first + corner;
            const DoubleVec3& vertex = p->m_vertices[corner];
            m_vertices[index] = Vec3{static_cast<float>(vertex.x),
                static_cast<float>(vertex.y), static_cast<float>(vertex.z)};
            m_vertexIndices[index] = index;
            m_neighbours[index] = p->m_neighbours[corner] ?
                p->m_neighbours[corner]->m_index : 0xFFFFFFFFu;
        }
        ++polygon;
        first += count;
    }
    removeDuplicateVerticesFromOptMesh();
    buildQuadTree(polygons);
}

// ELF 0x305060. A duplicate points forward to another original vertex; follow the whole chain.
void QuadTree::removeDuplicateVerticesFromOptMesh()
{
    std::vector<std::pair<bool, unsigned int> > mapping(
        m_numberOfVertexIndices, std::make_pair(false, 0u));
    unsigned int count = 0;
    buildQuadTreesForDuplicateVertices(mapping, count);
    Vec3* vertices = checkPointer(new Vec3[count]());
    for (unsigned int i = 0; i < m_numberOfVertexIndices; ++i)
    {
        if (mapping[i].first)
        {
            vertices[mapping[i].second] = m_vertices[i];
            m_vertexIndices[i] = mapping[i].second;
        }
        else
        {
            unsigned int source = i;
            while (!mapping[source].first)
                source = mapping[source].second;
            m_vertexIndices[i] = mapping[source].second;
        }
    }
    delete[] m_vertices;
    m_vertices = vertices;
    m_numberOfVertices = count;
}

// ELF 0x305360.
void QuadTree::buildQuadTreesForDuplicateVertices(
    std::vector<std::pair<bool, unsigned int> >& mapping, unsigned int& nextVertex)
{
    std::vector<Vec3*> vertices[4];
    std::vector<unsigned int> indices[4];
    std::vector<std::pair<Vec2, Vec2> > bounds;
    initialDivideQuadsAndTestForDuplicateVertices(vertices, indices, bounds);
    for (unsigned int i = 0; i < 4; ++i)
        recurseQuadTreeForDuplicateVertices(vertices[i], indices[i], bounds[i], 0,
                                            mapping, nextVertex);
}

namespace {

// [INFERRED] Name/grouping for the identical inlined subdivision in ELF 0x30556D..0x3057FC
// and 0x305A6A..0x305CF2. The four pairs are appended, without clearing the output.
void appendDuplicateVertexQuadrants(const Vec2& minimum, const Vec2& maximum,
                                    std::vector<std::pair<Vec2, Vec2> >& bounds)
{
    Vec2 a = minimum;
    Vec2 b = {(minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f};
    bounds.push_back(std::make_pair(a, b));
    a = Vec2{minimum.x, (minimum.y + maximum.y) * 0.5f};
    b = Vec2{(minimum.x + maximum.x) * 0.5f, maximum.y};
    bounds.push_back(std::make_pair(a, b));
    a = Vec2{(minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f};
    b = maximum;
    bounds.push_back(std::make_pair(a, b));
    a = Vec2{(minimum.x + maximum.x) * 0.5f, minimum.y};
    b = Vec2{maximum.x, (minimum.y + maximum.y) * 0.5f};
    bounds.push_back(std::make_pair(a, b));
}

} // namespace

// ELF 0x305550. Inclusive edges choose the first matching quadrant, with no overlap margin.
void QuadTree::initialDivideQuadsAndTestForDuplicateVertices(std::vector<Vec3*>* vertices,
    std::vector<unsigned int>* indices, std::vector<std::pair<Vec2, Vec2> >& bounds)
{
    appendDuplicateVertexQuadrants(m_min, m_max, bounds);
    for (unsigned int i = 0; i < m_numberOfVertices; ++i)
    {
        Vec3* vertex = m_vertices + i;
        for (unsigned int q = 0; q < 4; ++q)
        {
            if (bounds[q].first.x <= vertex->x && bounds[q].first.y <= vertex->z &&
                vertex->x <= bounds[q].second.x && vertex->z <= bounds[q].second.y)
            {
                vertices[q].push_back(vertex);
                indices[q].push_back(m_vertexIndices[i]);
                break;
            }
        }
    }
}

// ELF 0x305A50.
void QuadTree::divideQuadsAndTestForDuplicateVertices(const Vec2& minimum, const Vec2& maximum,
    const std::vector<Vec3*>& inputVertices, const std::vector<unsigned int>& inputIndices,
    std::vector<Vec3*>* vertices, std::vector<unsigned int>* indices,
    std::vector<std::pair<Vec2, Vec2> >& bounds)
{
    appendDuplicateVertexQuadrants(minimum, maximum, bounds);
    for (unsigned int i = 0; i < inputVertices.size(); ++i)
    {
        for (unsigned int q = 0; q < 4; ++q)
        {
            Vec3* vertex = inputVertices[i];
            if (bounds[q].first.x <= vertex->x && bounds[q].first.y <= vertex->z &&
                vertex->x <= bounds[q].second.x && vertex->z <= bounds[q].second.y)
            {
                vertices[q].push_back(vertex);
                indices[q].push_back(inputIndices[i]);
                break;
            }
        }
    }
}

// ELF 0x305FA0. Subdivision stops at depth four or nine vertices, then compares forward pairs.
void QuadTree::recurseQuadTreeForDuplicateVertices(const std::vector<Vec3*>& vertices,
    const std::vector<unsigned int>& indices, const std::pair<Vec2, Vec2>& boundary,
    unsigned int depth, std::vector<std::pair<bool, unsigned int> >& mapping,
    unsigned int& nextVertex)
{
    const unsigned int count = static_cast<unsigned int>(vertices.size());
    if (depth != 4 && count > 9)
    {
        std::vector<Vec3*> children[4];
        std::vector<unsigned int> childIndices[4];
        std::vector<std::pair<Vec2, Vec2> > bounds;
        divideQuadsAndTestForDuplicateVertices(boundary.first, boundary.second, vertices,
                                               indices, children, childIndices, bounds);
        for (unsigned int q = 0; q < 4; ++q)
            recurseQuadTreeForDuplicateVertices(children[q], childIndices[q], bounds[q],
                                                depth + 1, mapping, nextVertex);
        return;
    }
    for (unsigned int i = 0; i < count; ++i)
    {
        bool duplicate = false;
        for (unsigned int j = i + 1; j < count; ++j)
        {
            const Vec3 difference = {vertices[i]->x - vertices[j]->x,
                vertices[i]->y - vertices[j]->y, vertices[i]->z - vertices[j]->z};
            if (difference.x * difference.x + difference.y * difference.y +
                difference.z * difference.z < 1.1920928955078125e-7f)
            {
                mapping[indices[i]] = std::make_pair(false, indices[j]);
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            mapping[indices[i]] = std::make_pair(true, nextVertex);
            ++nextVertex;
        }
    }
}

// ELF 0x300EF0 (C2; C1 0x301010). Initializes the island, bounds, array pointers, selected
// counts, timing values, and debug mesh. Size and node/vertex-index/neighbor counts remain unset until clean,
// which readFromFile calls before loading.
QuadTree::QuadTree(unsigned int island)
    : m_island(island)
    , m_numberOfPolygons(0)
    , m_numberOfNodePolygons(0)
    , m_numberOfVertices(0)
    , m_nodes(0)
    , m_polygons(0)
    , m_nodePolygons(0)
    , m_vertices(0)
    , m_vertexIndices(0)
    , m_neighbours(0)
    , m_array84(0)
    , m_array88(0)
    , m_double8C(0.0)
    , m_double94(0.0)
    , m_double9C(0.0)
    , m_doubleA4(0.0)
    , m_debugMesh(0)
{
    m_min.x = 0.0f;
    m_min.y = 0.0f;
    m_max.x = 0.0f;
    m_max.y = 0.0f;

    // m_size, m_numberOfNodes, m_numberOfVertexIndices and m_numberOfNeighbours deliberately not
    // initialised - both binaries step over them.
}

// ELF 0x301130 (D2; D1 0x301230). clean, then the five vectors in reverse declaration order.
QuadTree::~QuadTree()
{
    clean();
}

// ELF 0x301330. Releases mesh arrays, resets their counts and pointers, and removes debug drawing.
void QuadTree::clean()
{
    delete[] m_nodes;
    delete[] m_polygons;
    delete[] m_nodePolygons;
    delete[] m_vertices;
    delete[] m_vertexIndices;
    delete[] m_neighbours;

    m_numberOfNodes = 0;
    m_numberOfPolygons = 0;
    m_numberOfNodePolygons = 0;
    m_numberOfVertices = 0;
    m_numberOfVertexIndices = 0;
    m_numberOfNeighbours = 0;
    m_nodes = 0;
    m_polygons = 0;
    m_nodePolygons = 0;
    m_vertices = 0;
    m_vertexIndices = 0;
    m_neighbours = 0;

    delete[] m_array84;
    delete[] m_array88;
    m_array84 = 0;
    m_array88 = 0;

    for (unsigned int i = 0; i < m_destroyableMapParts.size(); ++i)
        delete m_destroyableMapParts[i];
    m_destroyableMapParts.clear();

    undraw();
}

// ELF 0x3016C0. Reads a version-3 navigation tree. Unsupported versions leave the existing tree unchanged.
void QuadTree::readFromFile(io::File& file)
{
    int version = 0;
    file.read(version);

    if (version < 3)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTree.cpp", 132, "")
            << "Quad-tree file is not up to date. Reload GTS-data end save new quadtree-files for this level.";
        return;
    }
    if (version > 3)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTree.cpp", 137, "")
            << "Quad-tree file is newer than your build. Update to latest build.";
        return;
    }

    clean();

    file.read(&m_min, sizeof(m_min));
    file.read(&m_max, sizeof(m_max));
    file.read(m_size);

    file.read(m_numberOfNodes);
    m_nodes = checkPointer(new QuadTreeNode[m_numberOfNodes]);
    file.read(m_nodes, sizeof(QuadTreeNode) * m_numberOfNodes);

    file.read(m_numberOfPolygons);
    m_polygons = checkPointer(new Polygon[m_numberOfPolygons]);
    file.read(m_polygons, sizeof(Polygon) * m_numberOfPolygons);

    file.read(m_numberOfNodePolygons);
    m_nodePolygons = checkPointer(new unsigned int[m_numberOfNodePolygons]);
    file.read(m_nodePolygons, sizeof(unsigned int) * m_numberOfNodePolygons);

    file.read(m_numberOfVertices);
    m_vertices = checkPointer(new Vec3[m_numberOfVertices]());
    file.read(m_vertices, sizeof(Vec3) * m_numberOfVertices);

    file.read(m_numberOfVertexIndices);
    m_vertexIndices = checkPointer(new unsigned int[m_numberOfVertexIndices]);
    file.read(m_vertexIndices, sizeof(unsigned int) * m_numberOfVertexIndices);

    file.read(m_numberOfNeighbours);
    m_neighbours = checkPointer(new unsigned int[m_numberOfNeighbours]);
    file.read(m_neighbours, sizeof(unsigned int) * m_numberOfNeighbours);
}

// ELF 0x306350. Finds a polygon for a position. Empty trees and points outside the island bounds return false.
bool QuadTree::findPolygon(const Vec3& position, unsigned int& polygon,
                           MaterialManager* materials, float allowedHeightDiff) const
{
    if (!m_nodes)
        return false;

    if (position.x < m_min.x || position.x > m_max.x
        || position.z < m_min.y || position.z > m_max.y)
        return false;

    return m_nodes[0].findPolygon(this, m_min, m_size, position, polygon, materials,
                                  allowedHeightDiff);
}

// ELF 0x306630.
bool QuadTree::pointWithinPolygonEdges(Polygon* polygon, const Vec3& point) const
{
    Vec2 point2;
    point2.x = 0.0f;
    point2.y = 0.0f;
    vec3ToVec2XZ(point, point2);

    unsigned char edge = 0;
    if (polygon->m_numberOfVertices == 0)
        return true;

    do
    {
        Vec2 first;
        first.x = 0.0f;
        first.y = 0.0f;
        vec3ToVec2XZ(m_vertices[m_vertexIndices[polygon->m_firstIndex + edge]], first);

        Vec2 second;
        second.x = 0.0f;
        second.y = 0.0f;
        const unsigned char next = static_cast<unsigned char>(
            (edge + 1) % polygon->m_numberOfVertices);
        vec3ToVec2XZ(m_vertices[m_vertexIndices[polygon->m_firstIndex + next]], second);

        if ((second.y - first.y) * (point2.x - first.x)
            + (point2.y - first.y) * -(second.x - first.x) < -0.001f)
            return false;

        ++edge;
    }
    while (edge < polygon->m_numberOfVertices);

    return true;
}

// ELF 0x3064B0.
bool QuadTree::withinPolygon(Polygon* polygon, const Vec3& point, float& distanceSquared,
                             float allowedHeightDiff) const
{
    const Vec3& first = m_vertices[m_vertexIndices[polygon->m_firstIndex]];

    Plane3 plane;
    plane.normal = polygon->m_normal;
    plane.d = -(first.y * polygon->m_normal.y
              + first.x * polygon->m_normal.x
              + first.z * polygon->m_normal.z);

    Vec3 lineEnd;
    lineEnd.x = point.x;
    lineEnd.y = point.y - 1.0f;
    lineEnd.z = point.z;

    Vec3 intersection;
    intersection.x = 0.0f;
    intersection.y = 0.0f;
    intersection.z = 0.0f;
    bool insideSegment;

    if (!plane.intersectLine(point, lineEnd, intersection, insideSegment)
        || !pointWithinPolygonEdges(polygon, intersection))
        return false;

    const float dx = point.x - intersection.x;
    const float dy = point.y - intersection.y;
    const float dz = point.z - intersection.z;
    distanceSquared = dy * dy + dx * dx + dz * dz;     // the listing's summation order

    static const float maxDistanceAbove = 1.0f;
    const float limit = point.y > intersection.y ? allowedHeightDiff : maxDistanceAbove;
    return !(distanceSquared > limit * limit);         // setbe 0x306619: unordered counts as inside
}

// ELF 0x308100.
bool QuadTree::isPointInPolygon(const Vec3& point, unsigned int polygon, float allowedHeightDiff,
                                MaterialManager* materials) const
{
    Polygon* const candidate = &m_polygons[polygon];
    // The shipped known-polygon gate is a positive `cost >= 0` test. Keep its unordered case:
    // unlike the leaf scan's rejection-form comparison, a NaN cost rejects this fast path.
    if (materials && !(materials->getMaterialCost(candidate->m_material) >= 0.0f))
        return false;

    float distanceSquared = 0.0f;
    return withinPolygon(candidate, point, distanceSquared, allowedHeightDiff);
}

// ELF 0x307780.
void QuadTree::getVerticiesOfPolygon(std::vector<const Vec3*>& vertices,
                                     unsigned int polygon) const
{
    Polygon* const candidate = &m_polygons[polygon];
    unsigned char corner = 0;
    if (candidate->m_numberOfVertices == 0)
        return;

    do
    {
        vertices.push_back(&m_vertices[m_vertexIndices[candidate->m_firstIndex + corner]]);
        ++corner;
    }
    while (corner < candidate->m_numberOfVertices);
}

// ELF 0x3081A0. Retail directly indexes the polygon array; there is no bounds or
// null guard between the caller-supplied polygon id and the material byte.
unsigned char QuadTree::getMaterialOfPolygon(unsigned int polygon) const
{
    return m_polygons[polygon].m_material;
}

// ELF 0x3081C0. Unchecked, like the getter.
void QuadTree::setMaterialOfPolygon(unsigned int polygon, unsigned char material)
{
    m_polygons[polygon].m_material = material;
}

// Polygon topology accessors. Each polygon's corners and neighbours share one run of indices
// starting at m_firstIndex: corner i is m_vertices[m_vertexIndices[first + i]], and the polygon
// across the edge from corner i to corner i + 1 is m_neighbours[first + i] (UINT_MAX on a border).
// None of these checks the polygon id; the "safe" variants only wrap the corner index.

// ELF 0x3069A0. The only bounds-checked lookup: null past the last polygon.
Polygon* QuadTree::getPolygon(unsigned int polygon) const
{
    if (polygon >= m_numberOfPolygons)
        return 0;
    return &m_polygons[polygon];
}

// ELF 0x3068B0. UINT_MAX when the edge index is not below the polygon's corner count.
unsigned int QuadTree::getNeighbourOfPolygon(unsigned int polygon, unsigned char edge) const
{
    const Polygon* const record = &m_polygons[polygon];
    if (edge >= record->m_numberOfVertices)
        return 0xFFFFFFFFu;
    return m_neighbours[record->m_firstIndex + edge];
}

// ELF 0x306F80. The start polygon is appended unconditionally (no sentinel or duplicate test,
// unlike the internal walk), then each neighbour is walked in edge order.
void QuadTree::getPolygonsInRadius(std::list<unsigned int>& polygons, unsigned int polygon,
                                   const Vec3& centre, float radius) const
{
    polygons.insert(polygons.end(), polygon);
    const Polygon* const record = &m_polygons[polygon];
    for (unsigned char edge = 0; edge < record->m_numberOfVertices; ++edge)
        getPolygonsInRadiusInternal(polygons, m_neighbours[record->m_firstIndex + edge], centre,
                                    radius);
}

// ELF 0x307030. A polygon is taken when corner 0, or the corner `previous` of any pass, is
// strictly closer than the radius, or when the centre projects onto the edge previous -> i
// (0 <= t <= length) at a perpendicular distance within the radius. Retail quirks kept:
// `previous` only advances when the projection lands on the edge (a degenerate edge, length
// <= 1e-8, or an off-edge projection keeps the old corner), and corner 0 is read before the
// corner count is tested. A taken polygon is appended and its neighbours walked in edge order.
void QuadTree::getPolygonsInRadiusInternal(std::list<unsigned int>& polygons,
                                           unsigned int polygon, const Vec3& centre,
                                           float radius) const
{
    if (polygon == 0xFFFFFFFFu)
        return;
    if (std::find(polygons.begin(), polygons.end(), polygon) != polygons.end())
        return;

    const float radiusSqr = radius * radius;
    const Polygon* const record = &m_polygons[polygon];
    unsigned char previous = static_cast<unsigned char>(record->m_numberOfVertices - 1);

    const Vec3& first = m_vertices[m_vertexIndices[record->m_firstIndex]];
    float dx = centre.x - first.x;
    float dy = centre.y - first.y;
    float dz = centre.z - first.z;
    bool touches = radiusSqr > dx * dx + dy * dy + dz * dz;

    if (!touches)
    {
        for (unsigned char i = 0; i < record->m_numberOfVertices; ++i)
        {
            const Vec3& a = m_vertices[m_vertexIndices[record->m_firstIndex + i]];
            const Vec3& b = m_vertices[m_vertexIndices[record->m_firstIndex + previous]];

            dx = centre.x - b.x;
            dy = centre.y - b.y;
            dz = centre.z - b.z;
            if (radiusSqr > dx * dx + dy * dy + dz * dz)
            {
                touches = true;
                break;
            }

            float ex = b.x - a.x;
            float ey = b.y - a.y;
            float ez = b.z - a.z;
            const float length = std::sqrt(ex * ex + ey * ey + ez * ez);
            if (length > 1e-08f)
            {
                const float inverse = 1.0f / length;
                ex *= inverse;
                ey *= inverse;
                ez *= inverse;
                const float wx = centre.x - a.x;
                const float wy = centre.y - a.y;
                const float wz = centre.z - a.z;
                const float t = ex * wx + ey * wy + ez * wz;
                if (!(0.0f > t) && !(t > length))
                {
                    if (radiusSqr >= (wx * wx + wy * wy + wz * wz) - t * t)
                    {
                        touches = true;
                        break;
                    }
                    previous = i;
                }
            }
        }
        if (!touches)
            return;
    }

    polygons.insert(polygons.end(), polygon);
    for (unsigned char edge = 0; edge < record->m_numberOfVertices; ++edge)
        getPolygonsInRadiusInternal(polygons, m_neighbours[record->m_firstIndex + edge], centre,
                                    radius);
}

// ELF 0x3069E0. Same shape as getPolygonsInRadius.
void QuadTree::getPolygonsInTriangle(std::list<unsigned int>& polygons, unsigned int polygon,
                                     const Vec3& a, const Vec3& b, const Vec3& c) const
{
    polygons.insert(polygons.end(), polygon);
    const Polygon* const record = &m_polygons[polygon];
    for (unsigned char edge = 0; edge < record->m_numberOfVertices; ++edge)
        getPolygonsInTriangleInternal(polygons, m_neighbours[record->m_firstIndex + edge], a, b, c);
}

// ELF 0x306AA0. A polygon is taken when corner 0, or the corner `previous` of a pass, lies in the
// triangle (pointWithinPolygon2d, XZ), or when the edge current -> previous crosses a triangle
// edge (lineIntersect with both parameters in [0, 1]). Any tested corner more than 2 m above
// or below the triangle's first vertex rejects the whole polygon (corner 0 after its inside
// test, the others before theirs). `previous` advances on every pass here.
void QuadTree::getPolygonsInTriangleInternal(std::list<unsigned int>& polygons,
                                             unsigned int polygon, const Vec3& a, const Vec3& b,
                                             const Vec3& c) const
{
    Vec3 triangle[3];
    triangle[0] = a;
    triangle[1] = b;
    triangle[2] = c;
    if (polygon == 0xFFFFFFFFu)
        return;
    if (std::find(polygons.begin(), polygons.end(), polygon) != polygons.end())
        return;

    const Polygon* const record = &m_polygons[polygon];
    unsigned char previous = static_cast<unsigned char>(record->m_numberOfVertices - 1);
    const Vec3& first = m_vertices[m_vertexIndices[record->m_firstIndex]];
    bool touches = pointWithinPolygon2d(first, triangle, 3);
    if (std::fabs(first.y - a.y) > 2.0f)
        return;

    Vec2 triangle2d[3];
    for (int i = 0; i < 3; ++i)
    {
        triangle2d[i].x = 0.0f;   // retail Vec2 constructors
        triangle2d[i].y = 0.0f;
    }
    vec3ToVec2XZ(a, triangle2d[0]);
    vec3ToVec2XZ(b, triangle2d[1]);
    vec3ToVec2XZ(c, triangle2d[2]);

    if (!touches)
    {
        for (unsigned char i = 0; i < record->m_numberOfVertices; ++i)
        {
            const Vec3& current = m_vertices[m_vertexIndices[record->m_firstIndex + i]];
            const Vec3& corner = m_vertices[m_vertexIndices[record->m_firstIndex + previous]];
            if (std::fabs(corner.y - a.y) > 2.0f)
                return;
            if (pointWithinPolygon2d(corner, triangle, 3))
            {
                touches = true;
                break;
            }

            Vec2 from = { 0.0f, 0.0f };
            Vec2 to = { 0.0f, 0.0f };
            vec3ToVec2XZ(current, from);
            vec3ToVec2XZ(corner, to);
            for (int j = 0; j < 3; ++j)
            {
                float t;
                float s;
                if (lineIntersect(from, to, triangle2d[j], triangle2d[(j + 1) % 3], t, s)
                    && t >= 0.0f && t <= 1.0f && s >= 0.0f && s <= 1.0f)
                {
                    touches = true;
                    break;
                }
            }
            if (touches)
                break;
            previous = i;
        }
        if (!touches)
            return;
    }

    polygons.insert(polygons.end(), polygon);
    for (unsigned char edge = 0; edge < record->m_numberOfVertices; ++edge)
        getPolygonsInTriangleInternal(polygons, m_neighbours[record->m_firstIndex + edge], a, b, c);
}

// ELF 0x3068F0.
bool QuadTree::isNeighbourOfPolygon(unsigned int polygon, unsigned int neighbour) const
{
    const Polygon* const record = &m_polygons[polygon];
    for (unsigned char edge = 0; edge < record->m_numberOfVertices; ++edge)
    {
        if (m_neighbours[record->m_firstIndex + edge] == neighbour)
            return true;
    }
    return false;
}

// ELF 0x306950. The edge index wraps modulo the corner count.
unsigned int QuadTree::getSafeNeighbourOfPolygon(unsigned int polygon, unsigned char edge) const
{
    const Polygon* const record = &m_polygons[polygon];
    return m_neighbours[record->m_firstIndex + edge % record->m_numberOfVertices];
}

// ELF 0x307530.
const Vec3* QuadTree::getVertexForPolygon(unsigned int polygon, unsigned char corner) const
{
    return &m_vertices[m_vertexIndices[m_polygons[polygon].m_firstIndex + corner]];
}

// ELF 0x307560.
bool QuadTree::isVertexOfPolygon(unsigned int polygon, unsigned int vertex) const
{
    const Polygon* const record = &m_polygons[polygon];
    for (unsigned char corner = 0; corner < record->m_numberOfVertices; ++corner)
    {
        if (m_vertexIndices[record->m_firstIndex + corner] == vertex)
            return true;
    }
    return false;
}

// ELF 0x3075C0. The corner index wraps modulo the corner count.
const Vec3* QuadTree::getSafeVertexForPolygon(unsigned int polygon, unsigned char corner) const
{
    const Polygon* const record = &m_polygons[polygon];
    return &m_vertices[m_vertexIndices[record->m_firstIndex + corner % record->m_numberOfVertices]];
}

// ELF 0x307610.
unsigned int QuadTree::getVertexIndexForPolygon(unsigned int polygon, unsigned char corner) const
{
    return m_vertexIndices[m_polygons[polygon].m_firstIndex + corner];
}

// ELF 0x307640. The corner index wraps modulo the corner count.
unsigned int QuadTree::getSafeVertexIndexForPolygon(unsigned int polygon,
                                                    unsigned char corner) const
{
    const Polygon* const record = &m_polygons[polygon];
    return m_vertexIndices[record->m_firstIndex + corner % record->m_numberOfVertices];
}

// ELF 0x307680. The corner average. A polygon without corners divides by zero, as in retail.
void QuadTree::getCentreOfPolygon(Vec3& centre, unsigned int polygon) const
{
    const Polygon* const record = &m_polygons[polygon];
    centre.x = 0.0f;
    centre.y = 0.0f;
    centre.z = 0.0f;
    for (unsigned char corner = 0; corner < record->m_numberOfVertices; ++corner)
    {
        const Vec3& vertex = m_vertices[m_vertexIndices[record->m_firstIndex + corner]];
        centre.x += vertex.x;
        centre.y += vertex.y;
        centre.z += vertex.z;
    }
    const float inverse = 1.0f / static_cast<float>(record->m_numberOfVertices);
    centre.x *= inverse;
    centre.y *= inverse;
    centre.z *= inverse;
}

// ELF 0x307760.
const Vec3* QuadTree::getNormalOfPolygon(unsigned int polygon) const
{
    return &m_polygons[polygon].m_normal;
}

// ELF 0x307F90. Finds the edge of `polygon` that borders `neighbour`. Retail tests edge 0 before
// comparing against the corner count, so a polygon without corners still reads its first entry.
bool QuadTree::getNeighbourIndexForPolygon(unsigned char& edge, unsigned int polygon,
                                           unsigned int neighbour) const
{
    const Polygon* const record = &m_polygons[polygon];
    edge = 0;
    if (m_neighbours[record->m_firstIndex] == neighbour)
        return true;
    for (;;)
    {
        ++edge;
        if (edge >= record->m_numberOfVertices)
            return false;
        if (m_neighbours[record->m_firstIndex + edge] == neighbour)
            return true;
    }
}

// ELF 0x307830. Walks the polygon's edges (skipping the one shared with `previousPolygon`) and
// intersects the XZ ray origin + t * (direction.x, 0, direction.z) with each. The first edge hit
// with t >= 0 inside the edge wins; within 0.001 of a corner the edge only counts when the ray
// leaves through it (its 2D normal faces against the ray). Otherwise the smallest near miss under
// `minInvalid` is used. The result lies on the ray in XZ; its height follows `direction.y` over
// the XZ distance. `edge` is left on the hit edge (or past the last one). For a direct hit
// `edgeClearance` receives the distance to the nearer edge end.
bool QuadTree::getLineIntersectionOnEdgeInPolygon(Vec3& intersection, unsigned int polygon,
                                                  unsigned char& edge,
                                                  unsigned int previousPolygon,
                                                  const Vec3& origin, const Vec3& direction,
                                                  float* edgeClearance, float*) const
{
    static float minInvalid = 0.001f;

    Vec2 start;
    start.x = 0.0f;
    start.y = 0.0f;
    Vec2 corner;
    corner.x = 0.0f;
    corner.y = 0.0f;
    Vec2 nextCorner;
    nextCorner.x = 0.0f;
    nextCorner.y = 0.0f;
    Vec2 end;
    end.x = 0.0f;
    end.y = 0.0f;
    Vec2 hit;
    hit.x = 0.0f;
    hit.y = 0.0f;
    vec3ToVec2XZ(origin, start);

    float bestMiss = 99999.0f;
    float bestT = 0.0f;
    Vec2 bestEnd;
    bestEnd.x = 0.0f;
    bestEnd.y = 0.0f;

    const Polygon* const record = &m_polygons[polygon];
    edge = 0;
    if (record->m_numberOfVertices != 0)
    {
        do
        {
            if (previousPolygon == 0xFFFFFFFFu ||
                m_neighbours[record->m_firstIndex + edge] != previousPolygon)
            {
                vec3ToVec2XZ(m_vertices[m_vertexIndices[record->m_firstIndex + edge]], corner);
                vec3ToVec2XZ(m_vertices[m_vertexIndices[record->m_firstIndex +
                                                        (edge + 1) % record->m_numberOfVertices]],
                             nextCorner);

                const float scale = 1.0f;
                Vec3 flat;
                flat.x = direction.x * scale;
                flat.y = 0.0f * scale;
                flat.z = direction.z * scale;
                Vec3 ahead;
                ahead.x = origin.x + flat.x;
                ahead.y = origin.y + flat.y;
                ahead.z = origin.z + flat.z;
                vec3ToVec2XZ(ahead, end);

                float t;
                float s;
                if (lineIntersect(start, end, corner, nextCorner, t, s))
                {
                    bool onEdge = false;
                    if (t >= 0.0f && s >= 0.0f && 1.0f >= s)
                    {
                        if (0.001f >= s || !(s < 0.999f))
                        {
                            Vec2 heading;
                            heading.x = 0.0f;
                            heading.y = 0.0f;
                            vec3ToVec2XZ(direction, heading);
                            Vec2 side;
                            side.x = nextCorner.x - corner.x;
                            side.y = nextCorner.y - corner.y;
                            Vec2 normal;
                            normal.x = 0.0f;
                            normal.y = 0.0f;
                            getNormal(side, normal);
                            onEdge = !(normal.x * heading.x + normal.y * heading.y >= 0.0f);
                        }
                        else
                            onEdge = true;

                        if (onEdge)
                        {
                            Vec2 point;
                            point.x = start.x * (1.0f - t) + end.x * t;
                            point.y = start.y * (1.0f - t) + end.y * t;
                            vec2ToVec3XZ(point, intersection);
                            vec3ToVec2XZ(intersection, hit);
                            const float hx = hit.x - start.x;
                            const float hy = hit.y - start.y;
                            intersection.y =
                                std::sqrt(hx * hx + hy * hy) * direction.y + origin.y;
                            if (edgeClearance != 0)
                            {
                                const float ex = corner.x - nextCorner.x;
                                const float ey = corner.y - nextCorner.y;
                                const float length = std::sqrt(ex * ex + ey * ey);
                                const float nearer = (1.0f - s) > s ? s : 1.0f - s;
                                *edgeClearance = nearer * length;
                            }
                            return true;
                        }
                    }
                    else
                    {
                        // How far outside the ray (t < 0) or the edge (s outside [0, 1]).
                        const float behind = 0.0f > t ? std::fabs(t) : 9999.0f;
                        float miss;
                        if (0.0f > s && behind > -s)
                            miss = -s;
                        else if (s > 1.0f)
                            miss = behind > s - 1.0f ? s - 1.0f : behind;
                        else
                            miss = behind;

                        if (minInvalid > miss && bestMiss > miss)
                        {
                            bestMiss = miss;
                            bestT = t;
                            bestEnd = end;
                        }
                    }
                }
            }
            ++edge;
        }
        while (record->m_numberOfVertices > edge);
    }

    if (!(minInvalid > bestMiss))
        return false;

    Vec2 point;
    point.x = start.x * (1.0f - bestT) + bestEnd.x * bestT;
    point.y = start.y * (1.0f - bestT) + bestEnd.y * bestT;
    vec2ToVec3XZ(point, intersection);
    vec3ToVec2XZ(intersection, hit);
    const float hx = hit.x - start.x;
    const float hy = hit.y - start.y;
    intersection.y = std::sqrt(hx * hx + hy * hy) * direction.y + origin.y;
    return true;
}

// Tracks polygons affected by destroyable map parts and restores their original materials.

// ELF 0x308EE0.
void QuadTree::getPolygonsWithinBoundary(const Vec3& minimum, const Vec3& maximum,
                                         const Mat4& transformation,
                                         std::vector<unsigned int>& polygons) const
{
    Mat4 inverse;
    fullInvert(transformation, inverse);

    for (unsigned int polygonId = 0; polygonId < m_numberOfPolygons; ++polygonId)
    {
        const Polygon& polygon = m_polygons[polygonId];
        for (unsigned int corner = 0; corner < polygon.m_numberOfVertices; ++corner)
        {
            const Vec3& vertex = m_vertices[m_vertexIndices[polygon.m_firstIndex + corner]];

            Vec3 local;
            local.x = inverse.m[0] * vertex.x + inverse.m[4] * vertex.y
                    + inverse.m[8] * vertex.z + inverse.m[12];
            local.y = inverse.m[1] * vertex.x + inverse.m[5] * vertex.y
                    + inverse.m[9] * vertex.z + inverse.m[13];
            local.z = inverse.m[2] * vertex.x + inverse.m[6] * vertex.y
                    + inverse.m[10] * vertex.z + inverse.m[14];

            if (local.x >= minimum.x && local.x <= maximum.x
                && local.y >= minimum.y && local.y <= maximum.y
                && local.z >= minimum.z && local.z <= maximum.z)
            {
                polygons.push_back(polygonId);
                break;
            }
        }
    }
}

// ELF 0x3093B0.
void QuadTree::getPolygonsWithinBoundaryDoubles(const Vec3& minimum, const Vec3& maximum,
                                                const Mat4& transformation,
                                                std::vector<unsigned int>& polygons) const
{
    Mat4 inverse;
    fullInvert(transformation, inverse);

    for (unsigned int polygonId = 0; polygonId < m_numberOfPolygons; ++polygonId)
    {
        const Polygon& polygon = m_polygons[polygonId];
        for (unsigned int corner = 0; corner < polygon.m_numberOfVertices; ++corner)
        {
            const DoubleVec3& vertex = m_array84[m_vertexIndices[polygon.m_firstIndex + corner]];

            Vec3 local;
            local.x = static_cast<float>(inverse.m[0] * vertex.x + inverse.m[4] * vertex.y
                                       + inverse.m[8] * vertex.z + inverse.m[12]);
            local.y = static_cast<float>(inverse.m[1] * vertex.x + inverse.m[5] * vertex.y
                                       + inverse.m[9] * vertex.z + inverse.m[13]);
            local.z = static_cast<float>(inverse.m[2] * vertex.x + inverse.m[6] * vertex.y
                                       + inverse.m[10] * vertex.z + inverse.m[14]);

            if (local.x >= minimum.x && local.x <= maximum.x
                && local.y >= minimum.y && local.y <= maximum.y
                && local.z >= minimum.z && local.z <= maximum.z)
            {
                polygons.push_back(polygonId);
                break;
            }
        }
    }
}

// ELF 0x308860.
void QuadTree::initializeDestroyableMapPart(int mapPartId, bool initiallyWalkable,
                                            const Vec3& minimum, const Vec3& maximum,
                                            const Mat4& transformation, ClusterGraph* graph,
                                            bool useDoubles)
{
    const bool translationIsZero = transformation.m[12] == 0.0f
                                && transformation.m[13] == 0.0f
                                && transformation.m[14] == 0.0f;

    DestroyableMapPart* const part = checkPointer(new DestroyableMapPart);
    if (useDoubles)
        getPolygonsWithinBoundaryDoubles(minimum, maximum, transformation, part->m_polygons);
    else
        getPolygonsWithinBoundary(minimum, maximum, transformation, part->m_polygons);

    if (translationIsZero || part->m_polygons.empty())
    {
        part->m_originalMaterial = 0;
        m_destroyableMapParts.push_back(part);
        part->m_initiallyWalkable = initiallyWalkable;
        part->m_destroyed = false;
        return;
    }

    part->m_originalMaterial = m_polygons[part->m_polygons[0]].m_material;
    if (!part->m_originalMaterial)
    {
        for (unsigned int i = 0; i < part->m_polygons.size(); ++i)
        {
            const unsigned char material = m_polygons[part->m_polygons[i]].m_material;
            if (material)
            {
                part->m_originalMaterial = material;
                break;
            }
        }
    }

    m_destroyableMapParts.push_back(part);
    part->m_initiallyWalkable = initiallyWalkable;
    if (!initiallyWalkable && !useDoubles)
        restoreMapPart(mapPartId, graph);
    part->m_destroyed = false;
}

// ELF 0x308C60.
void QuadTree::destroyMapPart(int mapPartId, ClusterGraph* graph)
{
    if (!m_destroyableMapParts.empty())
    {
        DestroyableMapPart* const part = m_destroyableMapParts[mapPartId];
        if (part && !part->m_destroyed)
        {
            for (unsigned int i = 0; i < part->m_polygons.size(); ++i)
            {
                Polygon& polygon = m_polygons[part->m_polygons[i]];
                polygon.m_material = part->m_initiallyWalkable ? 0 : part->m_originalMaterial;
            }

            if (graph && !part->m_polygons.empty())
            {
                const unsigned int first = part->m_polygons[0];
                graph->m_clusters[m_polygons[first].m_cluster].m_walkable
                    = static_cast<unsigned char>(!part->m_initiallyWalkable);
            }
            part->m_destroyed = true;
        }
    }
}

// ELF 0x308D20.
void QuadTree::restoreMapPart(int mapPartId, ClusterGraph* graph)
{
    if (mapPartId != -1)
    {
        DestroyableMapPart* const part = m_destroyableMapParts[mapPartId];
        if (part)
        {
            for (unsigned int i = 0; i < part->m_polygons.size(); ++i)
            {
                Polygon& polygon = m_polygons[part->m_polygons[i]];
                polygon.m_material = part->m_initiallyWalkable ? part->m_originalMaterial : 0;
            }

            if (graph && !part->m_polygons.empty())
            {
                const unsigned int first = part->m_polygons[0];
                graph->m_clusters[m_polygons[first].m_cluster].m_walkable
                    = static_cast<unsigned char>(part->m_initiallyWalkable);
            }
            part->m_destroyed = false;
        }
    }
}

// ELF 0x308E30.
void QuadTree::resetOriginalMaterialOfDestroyableMapPart(int mapPartId, ClusterGraph* graph)
{
    if (mapPartId != -1)
    {
        DestroyableMapPart* const part = m_destroyableMapParts[mapPartId];
        if (part)
        {
            for (unsigned int i = 0; i < part->m_polygons.size(); ++i)
                m_polygons[part->m_polygons[i]].m_material = part->m_originalMaterial;

            if (graph && !part->m_polygons.empty())
            {
                const unsigned int first = part->m_polygons[0];
                graph->m_clusters[m_polygons[first].m_cluster].m_walkable
                    = static_cast<unsigned char>(part->m_initiallyWalkable);
            }
            part->m_destroyed = false;
        }
    }
}

// ELF 0x308DC0.
void QuadTree::resetDestroyableMapParts(ClusterGraph* graph)
{
    for (unsigned int mapPartId = 0; mapPartId < m_destroyableMapParts.size(); ++mapPartId)
        restoreMapPart(mapPartId, graph);
}

// ELF 0x301550. Writes the version-3 record readFromFile reads: bounds, size, then each array
// behind its count.
void QuadTree::saveToFile(io::File& file)
{
    file.write(3);
    file.write(&m_min, sizeof(m_min));
    file.write(&m_max, sizeof(m_max));
    file.write(m_size);

    file.write(m_numberOfNodes);
    file.write(m_nodes, sizeof(QuadTreeNode) * m_numberOfNodes);
    file.write(m_numberOfPolygons);
    file.write(m_polygons, sizeof(Polygon) * m_numberOfPolygons);
    file.write(m_numberOfNodePolygons);
    file.write(m_nodePolygons, sizeof(unsigned int) * m_numberOfNodePolygons);
    file.write(m_numberOfVertices);
    file.write(m_vertices, sizeof(Vec3) * m_numberOfVertices);
    file.write(m_numberOfVertexIndices);
    file.write(m_vertexIndices, sizeof(unsigned int) * m_numberOfVertexIndices);
    file.write(m_numberOfNeighbours);
    file.write(m_neighbours, sizeof(unsigned int) * m_numberOfNeighbours);
}

// ELF 0x306400. The debug walk of findPolygon: the same empty-tree and bounds gates (written as
// inclusive acceptance tests here), then the root node's printPolygonsInNode.
void QuadTree::printPolygonsInNode(const Vec3& position, unsigned int& polygon,
                                   MaterialManager* materials, float allowedHeightDiff) const
{
    if (!m_nodes)
        return;

    if (m_min.x <= position.x && position.x <= m_max.x
        && m_min.y <= position.z && position.z <= m_max.y)
        m_nodes[0].printPolygonsInNode(this, m_min, m_size, position, polygon, materials,
                                       allowedHeightDiff);
}

// ELF 0x306790. One static line per edge (red, 0.3 wide). With `height` both ends are drawn at
// that height instead of the vertices' own.
void QuadTree::drawPolygon(unsigned int polygon, float* height, float time)
{
    const Polygon* const candidate = &m_polygons[polygon];
    unsigned char corner = 0;
    if (candidate->m_numberOfVertices == 0)
        return;

    do
    {
        Vec3 start = m_vertices[m_vertexIndices[candidate->m_firstIndex + corner]];
        const unsigned char next = static_cast<unsigned char>(
            (corner + 1) % candidate->m_numberOfVertices);
        Vec3 end = m_vertices[m_vertexIndices[candidate->m_firstIndex + next]];
        if (height)
        {
            end.y = *height;
            start.y = end.y;
        }
        static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStaticLine(time, 0xFF0000u, start, end, 0.3f);
        ++corner;
    }
    while (corner < candidate->m_numberOfVertices);
}

// ELF 0x3068A0. Empty in the shipped image.
void QuadTree::drawPolygonEdge(unsigned int, unsigned char) const
{
}

// ELF 0x308010. The corner average, accumulated through getVertexForPolygon. A polygon without
// corners divides by zero, as in retail.
Vec3 QuadTree::getPolygonMidPoint(unsigned int polygon)
{
    Vec3 midPoint;
    midPoint.x = 0.0f;
    midPoint.y = 0.0f;
    midPoint.z = 0.0f;

    const Polygon* const candidate = &m_polygons[polygon];
    for (unsigned char corner = 0; corner < candidate->m_numberOfVertices; ++corner)
    {
        const Vec3* const vertex = getVertexForPolygon(polygon, corner);
        midPoint.x += vertex->x;
        midPoint.y += vertex->y;
        midPoint.z += vertex->z;
    }

    const float scale = 1.0f / static_cast<float>(candidate->m_numberOfVertices);
    midPoint.x *= scale;
    midPoint.y *= scale;
    midPoint.z *= scale;
    return midPoint;
}

// ELF 0x3081E0. Replaces the debug mesh with one static mesh of every polygon (60 s). Unlike
// undraw, neither manager is null-tested.
void QuadTree::draw() const
{
    IDPStaticCreator* const creator
        = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator);
    static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(m_debugMesh);
    // DPStaticMesh is not defined in this tree; the image stores the returned pointer unadjusted
    // as the DrawPrimitive handle. [CHOSEN, not recovered] cast.
    m_debugMesh = reinterpret_cast<DrawPrimitive*>(
        creator->createStaticMesh(60.0f, 0, m_polygons, m_vertices, m_vertexIndices,
                                  static_cast<int>(m_numberOfPolygons), true));
}

// ELF 0x3082B0. Each polygon's id as 3D text (white, 30 s) 2 m above its centre.
void QuadTree::drawPolygonIds() const
{
    IDPStaticCreator* const creator
        = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator);
    for (unsigned long polygon = 0; polygon < m_numberOfPolygons; ++polygon)
    {
        Vec3 centre;
        centre.x = 0.0f;
        centre.y = 0.0f;
        centre.z = 0.0f;
        getCentreOfPolygon(centre, polygon);
        centre.y += 2.0f;

        std::stringstream text;
        text << polygon;
        creator->createStatic3dText(30.0f, 0xFFFFFFFFu, centre, text.str(), 0,
                                    static_cast<AITextAlignmentX>(0),
                                    static_cast<AITextAlignmentY>(0));
    }
}

// ELF 0x3098F0. Walks every polygon's edges and checks nothing: the shipped body is the empty
// loop nest.
void QuadTree::validateNeighbourRelations() const
{
    for (unsigned int polygon = 0; polygon < m_numberOfPolygons; ++polygon)
    {
        const Polygon& candidate = m_polygons[polygon];
        for (unsigned char edge = 0; edge < candidate.m_numberOfVertices; ++edge)
        {
        }
    }
}

// ELF 0x308270. Inlined into the PE's clean.
// Hands the debug mesh to IDPManager::deleteMesh (ELF slot +0x50, PE +0x4C) when the draw-primitive
// manager exists; deleteMesh takes the handle by reference. The bool it answers is discarded.
void QuadTree::undraw() const
{
    if (CommonSingletonServer::m_iDPManager)
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(m_debugMesh);
}

} // namespace ai
} // namespace hfe
} // namespace dice
