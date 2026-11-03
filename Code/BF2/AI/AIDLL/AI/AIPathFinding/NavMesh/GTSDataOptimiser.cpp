// ELF FILE symbol GTSDataOptimiser.cpp; OptPolygon methods occur in this translation unit.
#include "GTSDataOptimiser.h"
#include "QuadTree.h"
#include "../Cluster/ClusterGraph.h"
#include "../../AIUtils/checkPointer.h"
#include "../../../../../IO/File/File.h"
#include <cmath>
#include <cfloat>
#include <sstream>
// ELF static init 0x2FC5A0, __tcf_0 0x2FC600, _GLOBAL__I 0x2FC630: ios_base::Init lifetime.
#include <iostream>

namespace dice { namespace hfe { namespace ai {

namespace {
// Inlined vector operations in the optimiser bodies; the helper names are inferred.
DoubleVec3 difference(const DoubleVec3& a, const DoubleVec3& b)
{
    DoubleVec3 result = { a.x - b.x, a.y - b.y, a.z - b.z };
    return result;
}
double scalarProduct(const DoubleVec3& a, const DoubleVec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
DoubleVec3 vectorProduct(const DoubleVec3& a, const DoubleVec3& b)
{
    DoubleVec3 result = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                         a.x * b.y - a.y * b.x };
    return result;
}
void normaliseDouble(DoubleVec3& vector)
{
    const double lengthSquared = scalarProduct(vector, vector);
    if (static_cast<double>(FLT_EPSILON) > std::fabs(1.0 - lengthSquared)) return;
    if (!(static_cast<double>(FLT_EPSILON) > std::fabs(lengthSquared)))
    {
        const double scale = 1.0 / std::sqrt(lengthSquared);
        vector.x *= scale; vector.y *= scale; vector.z *= scale;
    }
    else vector.x = vector.y = vector.z = 0.0;
}
int wrappedIndex(int index, int count)
{
    // shipped defect: a negative multiple produces count rather than zero.
    return index < 0 ? count + index % count : index % count;
}
float normalAngle(const DoubleVec3& a, const DoubleVec3& b)
{
    float dot = static_cast<float>(scalarProduct(a, b));
    if (dot < -1.0f) dot = -1.0f;
    else if (dot > 1.0f) dot = 1.0f;
    return std::acos(dot);
}
}

// ELF 0x2F2390 (C2), 0x2F2490 (C1).
GTSDataOptimiser::GTSDataOptimiser(ClusterGraph* graph) : m_current(), m_graph(graph) {}

// ELF 0x2F24E0 (D2), 0x2F2550 (D1).
GTSDataOptimiser::~GTSDataOptimiser()
{
    for (std::list<OptPolygon*>::iterator i = m_polygons.begin(); i != m_polygons.end(); ++i)
        destroyPolygon(*i);
}

// ELF 0x2F3EB0.
void GTSDataOptimiser::destroyPolygon(OptPolygon* polygon) { delete polygon; }

// ELF 0x2F62F0. Shipped code dereferences begin without checking for an empty list.
OptPolygon* GTSDataOptimiser::initPolygonIteration()
{
    m_current = m_polygons.begin();
    return *m_current;
}

// ELF 0x2F6310.
OptPolygon* GTSDataOptimiser::getNextPolygon()
{
    ++m_current;
    return m_current == m_polygons.end() ? 0 : *m_current;
}

// ELF 0x2F6340.
unsigned int GTSDataOptimiser::getNumberOfVertices() const
{
    unsigned int count = 0;
    for (std::list<OptPolygon*>::const_iterator i = m_polygons.begin(); i != m_polygons.end(); ++i)
        count += (*i)->m_numberOfVertices;
    return count;
}

// ELF 0x2F25C0 (C2), 0x2F25F0 (C1). m_index is left uninitialized.
OptPolygon::OptPolygon() : m_numberOfVertices(0), m_vertices(0), m_neighbours(0),
    m_material(0), m_originalMaterial(0), m_normal(0), m_cluster(0) {}

// ELF 0x2F2620 (D2), 0x2F2650 (D1).
OptPolygon::~OptPolygon() { deleteMembers(); }

// ELF 0x2F2680. Only the cached normal pointer is nulled after freeing.
void OptPolygon::deleteMembers()
{
    delete m_normal;
    m_normal = 0;
    delete[] m_vertices;
    delete[] m_neighbours;
}

// ELF 0x2F2F30.
const DoubleVec3* OptPolygon::getNormal() const
{
    if (!m_normal)
        calculateNormal();
    return m_normal;
}

// ELF 0x2F26E0. Calculates and caches a polygon normal from its vertex fan.
void OptPolygon::calculateNormal() const
{
    const DoubleVec3 origin = m_vertices[0];
    const DoubleVec3 firstEdge = difference(m_vertices[1], origin);
    DoubleVec3 secondEdge = difference(m_vertices[2], origin);
    m_normal = checkPointer(new DoubleVec3(vectorProduct(firstEdge, secondEdge)));
    DoubleVec3 firstUnit = firstEdge;
    normaliseDouble(firstUnit);
    DoubleVec3 secondUnit = secondEdge;
    normaliseDouble(secondUnit);
    float dot = static_cast<float>(scalarProduct(secondUnit, firstUnit));
    float angle = std::fabs(std::fabs(dot) - 1.0f);
    int corner = 2;
    while (static_cast<double>(0.01f) > scalarProduct(*m_normal, *m_normal) || 0.01f > angle)
    {
        if (corner >= m_numberOfVertices - 1) break;
        ++corner;
        secondEdge = difference(m_vertices[corner], origin);
        *m_normal = vectorProduct(firstEdge, secondEdge);
        secondUnit = secondEdge;
        normaliseDouble(secondUnit);
        dot = static_cast<float>(scalarProduct(secondUnit, firstUnit));
        // shipped defect: subsequent fan triangles omit the outer absolute value.
        angle = std::fabs(dot) - 1.0f;
    }
    normaliseDouble(*m_normal);
}

// ELF 0x2F2F70.
unsigned char OptPolygon::findFirstNeighbourIndex(OptPolygon* neighbour) const
{
    for (int edge = 0; edge < m_numberOfVertices; ++edge)
        if (m_neighbours[edge] == neighbour) return static_cast<unsigned char>(edge);
    Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/GTSDataOptimiser.cpp", 103, "")
        << "Couldn't find first neighbour index in " << static_cast<const void*>(this)
        << " of " << static_cast<const void*>(neighbour);
    return 0xFF;
}

// ELF 0x2F30F0.
void OptPolygon::changeVertex(const DoubleVec3& oldVertex, const DoubleVec3& newVertex)
{
    for (int corner = 0; corner < m_numberOfVertices; ++corner)
    {
        const DoubleVec3 delta = difference(oldVertex, m_vertices[corner]);
        if (!(scalarProduct(delta, delta) > DBL_EPSILON))
        {
            delete m_normal;
            m_normal = 0;
            m_vertices[corner] = newVertex;
            return;
        }
    }
}

// ELF 0x2F3210.
void OptPolygon::changeNeighbour(const DoubleVec3& vertex, OptPolygon* oldNeighbour,
                                 OptPolygon* newNeighbour)
{
    for (int corner = 0; corner < m_numberOfVertices; ++corner)
    {
        const DoubleVec3 delta = difference(vertex, m_vertices[corner]);
        if (!(scalarProduct(delta, delta) > DBL_EPSILON))
        {
            const int previous = corner - 1;
            const int edge = previous < 0 ? m_numberOfVertices + previous % m_numberOfVertices
                                          : previous % m_numberOfVertices;
            if (m_neighbours[edge] == oldNeighbour)
            {
                m_neighbours[edge] = newNeighbour;
                return;
            }
        }
    }
    Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/GTSDataOptimiser.cpp", 138, "")
        << "Couldn't change neighbour in " << static_cast<const void*>(this)
        << " from " << static_cast<const void*>(oldNeighbour)
        << " to " << static_cast<const void*>(newNeighbour);
}

// ELF 0x2F36E0. Appends the polygon's XZ coordinates and diagnostic pointer to the file.
void OptPolygon::printPolygon(io::File& file, int id) const
{
    file.seek(static_cast<io::SeekOrigin>(2), 0);
    std::stringstream text;
    const unsigned int count = m_numberOfVertices;
    text << static_cast<unsigned long>(count) << " " << id << std::endl;
    text << static_cast<const void*>(this) << std::endl;
    for (unsigned int corner = 0; corner < count; ++corner)
        text << m_vertices[corner].x << " " << m_vertices[corner].z << std::endl;
    file.write(text.str());
}

// ELF 0x2F3C40.
bool OptPolygon::checkForDuplicateVertices() const
{
    const unsigned int count = m_numberOfVertices;
    for (unsigned int first = 0; first < count; ++first)
        for (unsigned int second = first + 1; second < count; ++second)
        {
            const DoubleVec3 delta = difference(m_vertices[second], m_vertices[first]);
            if (DBL_EPSILON > scalarProduct(delta, delta)) return true;
        }
    return false;
}

// ELF 0x2F3450.
void GTSDataOptimiser::removeVerticesAndRebuildPolygon(const VertexRemovalMap& removals)
{
    for (VertexRemovalMap::const_iterator i = removals.begin(); i != removals.end(); ++i)
    {
        OptPolygon* polygon = i->first;
        const unsigned char oldCount = polygon->m_numberOfVertices;
        const unsigned char count = static_cast<unsigned char>(oldCount - i->second.size());
        DoubleVec3* vertices = new DoubleVec3[count]();
        OptPolygon** neighbours = new OptPolygon*[count];
        unsigned char output = 0;
        for (unsigned char corner = 0; corner < oldCount; ++corner)
        {
            bool remove = false;
            for (std::vector<const DoubleVec3*>::const_iterator v = i->second.begin();
                 v != i->second.end(); ++v)
            {
                const DoubleVec3 delta = difference(polygon->m_vertices[corner], **v);
                if (DBL_EPSILON > scalarProduct(delta, delta)) { remove = true; break; }
            }
            if (!remove)
            {
                vertices[output] = polygon->m_vertices[corner];
                neighbours[output++] = polygon->m_neighbours[corner];
            }
        }
        polygon->deleteMembers();
        polygon->m_numberOfVertices = count;
        polygon->m_vertices = vertices;
        delete polygon->m_normal;
        polygon->m_normal = 0;
        polygon->m_neighbours = neighbours;
    }
}

// ELF 0x2F3D60.
OptPolygon* GTSDataOptimiser::createPolygon() const { return checkPointer(new OptPolygon); }

// ELF 0x2F3EF0. Only the two-to-one pass is enabled in the shipped optimiser.
void GTSDataOptimiser::optimise(const std::vector<DestroyableMapPart*>& parts)
{
    m_graph->resetTempMemberSizes();
    std::set<OptPolygon*> removed;
    while (twoToOneMerge(removed)) {}
    delete[] m_graph->m_tempMemberSizes;
    m_graph->m_tempMemberSizes = 0;
    setPolygonIndices(parts);
}

// ELF 0x2F5340.
void GTSDataOptimiser::setPolygonIndices(const std::vector<DestroyableMapPart*>& parts)
{
    ClusterGraph::ClusterPolygonMap clusters;
    const std::set<unsigned int>& removed = m_graph->m_set18;
    std::map<unsigned int, unsigned int> oldParts;
    unsigned int part = 0;
    for (std::vector<DestroyableMapPart*>::const_iterator p = parts.begin(); p != parts.end(); ++p, ++part)
        for (std::vector<unsigned int>::const_iterator i = (*p)->m_polygons.begin();
             i != (*p)->m_polygons.end(); ++i)
            oldParts.insert(std::make_pair(*i, part));
    ClusterGraph::DynamicPolygonMap newParts;
    unsigned int index = 0;
    for (std::list<OptPolygon*>::iterator i = m_polygons.begin(); i != m_polygons.end(); ++i, ++index)
    {
        OptPolygon* polygon = *i;
        if (removed.find(polygon->m_index) == removed.end())
        {
            for (unsigned short cluster = 0; cluster < m_graph->m_numberOfClusters; ++cluster)
                if (polygon->m_cluster == cluster)
                {
                    ClusterGraph::ClusterPolygonMap::iterator entry = clusters.find(cluster);
                    if (entry == clusters.end())
                    {
                        std::list<unsigned int> indices;
                        indices.insert(indices.end(), index);
                        clusters.insert(std::make_pair(cluster, indices));
                    }
                    else entry->second.insert(entry->second.end(), index);
                }
        }
        else
            Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/GTSDataOptimiser.cpp", 623, "")
                << "We shouldn't come here!";
        std::map<unsigned int, unsigned int>::iterator entry = oldParts.find(polygon->m_index);
        if (entry != oldParts.end())
            newParts.insert(std::make_pair(index,
                std::make_pair(entry->second, parts[entry->second]->m_initiallyWalkable)));
        polygon->m_index = index;
    }
    m_graph->updateClustersAfterOptimisation(newParts, m_polygons, clusters);
}

// ELF 0x2F5D20. Dropping a previous list does not destroy its polygons, as shipped.
void GTSDataOptimiser::buildOptMeshFromTriangles(DoublePolygon* triangles, DoubleVec3* vertices,
    unsigned int* vertexIndices, unsigned int* neighbourIndices, unsigned int count, QuadTree* tree)
{
    m_polygons.resize(0, static_cast<OptPolygon*>(0));
    std::vector<OptPolygon*> polygons;
    std::set<unsigned int> dynamic;
    for (std::vector<DestroyableMapPart*>::const_iterator p = tree->m_destroyableMapParts.begin();
         p != tree->m_destroyableMapParts.end(); ++p)
    {
        const std::vector<unsigned int> indices = (*p)->m_polygons;
        for (std::vector<unsigned int>::const_iterator i = indices.begin(); i != indices.end(); ++i)
            dynamic.insert(*i);
    }
    for (unsigned int i = 0; i < count; ++i)
    {
        OptPolygon* polygon = createPolygon();
        polygon->m_numberOfVertices = triangles[i].m_numberOfVertices;
        polygon->m_vertices = new DoubleVec3[polygon->m_numberOfVertices]();
        delete polygon->m_normal;
        polygon->m_normal = 0;
        polygon->m_neighbours = new OptPolygon*[polygon->m_numberOfVertices];
        polygon->m_originalMaterial = triangles[i].m_material;
        polygon->m_material = dynamic.find(i) == dynamic.end() ? triangles[i].m_material : 8;
        if (polygon->m_normal) *polygon->m_normal = triangles[i].m_normal;
        else polygon->m_normal = new DoubleVec3(triangles[i].m_normal);
        polygon->m_index = i;
        polygon->m_cluster = triangles[i].m_cluster;
        for (unsigned int corner = 0; corner < triangles[i].m_numberOfVertices; ++corner)
        {
            polygon->m_vertices[corner] = vertices[vertexIndices[triangles[i].m_firstIndex + corner]];
            delete polygon->m_normal;
            polygon->m_normal = 0;
        }
        m_polygons.insert(m_polygons.end(), polygon);
        polygons.push_back(polygon);
    }
    unsigned int i = 0;
    for (std::list<OptPolygon*>::iterator p = m_polygons.begin(); p != m_polygons.end(); ++p, ++i)
        for (unsigned int corner = 0; corner < triangles[i].m_numberOfVertices; ++corner)
        {
            const unsigned int neighbour = neighbourIndices[triangles[i].m_firstIndex + corner];
            (*p)->m_neighbours[corner] = neighbour == ~0u ? 0 : polygons[neighbour];
        }
}

// ELF 0x2FB170. The two counters are diagnostic inputs unused by the shipped body.
void GTSDataOptimiser::cleanPolygon(OptPolygon* polygon, unsigned int, unsigned int)
{
    std::list<DoubleVec3> vertices;
    std::list<OptPolygon*> neighbours;
    VertexRemovalMap removals;
    const unsigned char count = polygon->m_numberOfVertices;
    unsigned char next = 1;
    unsigned char previous = static_cast<unsigned char>(count - 1);
    for (unsigned char corner = 0; corner < count; ++corner)
    {
        OptPolygon* neighbour = polygon->m_neighbours[corner];
        bool remove = false;
        if (neighbour == polygon->m_neighbours[previous] &&
            (!neighbour || neighbour->m_numberOfVertices > 3))
        {
            DoubleVec3 forward = difference(polygon->m_vertices[next], polygon->m_vertices[corner]);
            normaliseDouble(forward);
            DoubleVec3 backward = difference(polygon->m_vertices[previous], polygon->m_vertices[corner]);
            normaliseDouble(backward);
            remove = static_cast<double>(0.01f) > scalarProduct(backward, forward) + 1.0;
        }
        if (remove)
        {
            if (neighbour)
            {
                VertexRemovalMap::iterator entry = removals.find(neighbour);
                if (entry == removals.end())
                {
                    std::vector<const DoubleVec3*> indices;
                    indices.push_back(&polygon->m_vertices[corner]);
                    removals.insert(std::make_pair(neighbour, indices));
                }
                else entry->second.push_back(&polygon->m_vertices[corner]);
            }
            // shipped defect: previous advances only when a vertex is retained.
        }
        else
        {
            vertices.insert(vertices.end(), polygon->m_vertices[corner]);
            neighbours.insert(neighbours.end(), polygon->m_neighbours[corner]);
            previous = static_cast<unsigned char>(previous + 1) % count;
        }
        next = static_cast<unsigned char>(next + 1) % count;
    }
    removeVerticesAndRebuildPolygon(removals);
    polygon->deleteMembers();
    polygon->m_numberOfVertices = static_cast<unsigned char>(vertices.size());
    polygon->m_neighbours = new OptPolygon*[polygon->m_numberOfVertices];
    polygon->m_vertices = new DoubleVec3[polygon->m_numberOfVertices]();
    delete polygon->m_normal;
    polygon->m_normal = 0;
    unsigned int corner = 0;
    std::list<OptPolygon*>::const_iterator n = neighbours.begin();
    for (std::list<DoubleVec3>::const_iterator v = vertices.begin(); v != vertices.end(); ++v, ++n, ++corner)
    {
        polygon->m_vertices[corner] = *v;
        delete polygon->m_normal;
        polygon->m_normal = 0;
        polygon->m_neighbours[corner] = *n;
    }
}

// ELF 0x2F6380.
bool GTSDataOptimiser::twoToOneMerge(std::set<OptPolygon*>& removed)
{
    std::set<OptPolygon*> created;
    static unsigned int callTimes = 0;
    ++callTimes;
    bool merged = false;
    unsigned int ordinal = 0;
    for (std::list<OptPolygon*>::iterator p = m_polygons.begin(); p != m_polygons.end(); ++p)
    {
        ++ordinal;
        OptPolygon* polygon = *p;
        OptPolygon* output = 0;
        if (removed.find(polygon) != removed.end()) continue;
        const int count = polygon->m_numberOfVertices;
        for (int edge = 0; edge < count; ++edge)
        {
            OptPolygon* neighbour = polygon->m_neighbours[edge];
            if (!neighbour || neighbour->m_material != polygon->m_material ||
                neighbour->m_material == 8 || polygon->m_material == 8) continue;
            const DoubleVec3* neighbourNormal = neighbour->getNormal();
            const DoubleVec3* polygonNormal = polygon->getNormal();
            if (normalAngle(*polygonNormal, *neighbourNormal) > 0.08726646f) continue;
            if (polygon->m_neighbours[wrappedIndex(edge - 1, count)] == neighbour) continue;
            if (created.find(neighbour) != created.end()) continue;
            if (removed.find(neighbour) != removed.end())
                Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/GTSDataOptimiser.cpp", 955, "")
                    << "Neighbour " << static_cast<const void*>(neighbour) << " has previously been removed!";
            const int otherCount = neighbour->m_numberOfVertices;
            DoubleVec2 backward = {};
            getVectorToXZDouble(polygon->m_vertices[edge],
                polygon->m_vertices[wrappedIndex(edge - 1, count)], backward);
            int firstOther = otherCount - 1;
            for (; firstOther >= 0; --firstOther)
                if (neighbour->m_neighbours[firstOther] != polygon &&
                    neighbour->m_neighbours[wrappedIndex(firstOther - 1, otherCount)] == polygon)
                    break;
            DoubleVec2 normal = {};
            DoubleVec3 vertex = neighbour->m_vertices[firstOther];
            getVectorToXZDouble(vertex, neighbour->m_vertices[wrappedIndex(firstOther + 1, otherCount)], normal);
            getNormalDouble(normal, normal);
            if (normal.x * backward.x + normal.y * backward.y < 0.0) continue;
            DoubleVec2 forward = {};
            int last = edge;
            int lastOther = firstOther;
            do
            {
                last = wrappedIndex(last + 1, count);
                lastOther = wrappedIndex(lastOther - 1, otherCount);
            } while (polygon->m_neighbours[last] == neighbour);
            getVectorToXZDouble(polygon->m_vertices[last],
                polygon->m_vertices[wrappedIndex(last + 1, count)], forward);
            normal = DoubleVec2{};
            vertex = neighbour->m_vertices[lastOther];
            getVectorToXZDouble(neighbour->m_vertices[wrappedIndex(lastOther - 1, otherCount)], vertex, normal);
            getNormalDouble(normal, normal);
            if (normal.x * forward.x + normal.y * forward.y < 0.0) continue;

            OptPolygon* retained = neighbour;
            OptPolygon* discarded = polygon;
            unsigned short retainedCluster = neighbour->m_cluster;
            unsigned short discardedCluster = polygon->m_cluster;
            unsigned int remaining = m_graph->m_tempMemberSizes[discardedCluster];
            const unsigned int otherSize = m_graph->m_tempMemberSizes[retainedCluster];
            if (remaining <= otherSize)
            {
                retained = polygon;
                discarded = neighbour;
                retainedCluster = polygon->m_cluster;
                discardedCluster = neighbour->m_cluster;
                remaining = otherSize;
            }
            --remaining;
            if (retainedCluster != discardedCluster && remaining > 1)
            {
                std::set<unsigned int> visited;
                if (!m_graph->validateClusterConnectivityFromPolygon(discardedCluster, discarded,
                        remaining, visited, false))
                {
                    visited.clear();
                    continue;
                }
            }
            output = createPolygon();
            output->m_index = retained->m_index;
            output->m_cluster = retainedCluster;
            m_graph->m_set18.insert(discarded->m_index);
            m_graph->m_tempMemberSizes[discardedCluster] = remaining;
            std::list<const DoubleVec3*> vertices;
            std::list<OptPolygon*> neighbours;
            for (int corner = firstOther; corner != lastOther; corner = wrappedIndex(corner + 1, otherCount))
            {
                vertices.insert(vertices.end(), &neighbour->m_vertices[corner]);
                OptPolygon* next = neighbour->m_neighbours[corner];
                if (next)
                {
                    next->changeNeighbour(neighbour->m_vertices[corner], neighbour, output);
                    neighbour->changeNeighbour(neighbour->m_vertices[(corner + 1) % neighbour->m_numberOfVertices], next, 0);
                }
                neighbours.insert(neighbours.end(), next);
            }
            for (int corner = 0; corner < neighbour->m_numberOfVertices; ++corner)
                if (neighbour->m_neighbours[corner])
                    neighbour->m_neighbours[corner]->changeNeighbour(neighbour->m_vertices[corner], neighbour, 0);
            for (int corner = last; corner != edge; corner = wrappedIndex(corner + 1, count))
            {
                vertices.insert(vertices.end(), &polygon->m_vertices[corner]);
                OptPolygon* next = polygon->m_neighbours[corner];
                if (next)
                {
                    next->changeNeighbour(polygon->m_vertices[corner], polygon, output);
                    polygon->changeNeighbour(polygon->m_vertices[(corner + 1) % polygon->m_numberOfVertices], next, 0);
                }
                neighbours.insert(neighbours.end(), next);
            }
            for (int corner = 0; corner < polygon->m_numberOfVertices; ++corner)
                if (polygon->m_neighbours[corner])
                    polygon->m_neighbours[corner]->changeNeighbour(polygon->m_vertices[corner], polygon, 0);
            output->m_numberOfVertices = static_cast<unsigned char>(vertices.size());
            output->m_neighbours = new OptPolygon*[output->m_numberOfVertices];
            output->m_vertices = new DoubleVec3[output->m_numberOfVertices]();
            delete output->m_normal;
            output->m_normal = 0;
            int corner = 0;
            std::list<OptPolygon*>::const_iterator n = neighbours.begin();
            for (std::list<const DoubleVec3*>::const_iterator v = vertices.begin(); v != vertices.end(); ++v, ++n, ++corner)
            {
                output->m_vertices[corner] = **v;
                delete output->m_normal;
                output->m_normal = 0;
                output->m_neighbours[corner] = *n;
            }
            output->m_material = polygon->m_material;
            output->m_originalMaterial = polygon->m_originalMaterial;
            cleanPolygon(output, ordinal, callTimes);
            removed.insert(neighbour);
            removed.insert(polygon);
            created.insert(output);
            merged = true;
            break;
        }
    }
    for (std::list<OptPolygon*>::iterator p = m_polygons.begin(); p != m_polygons.end();)
    {
        if (removed.find(*p) == removed.end()) { ++p; continue; }
        OptPolygon* polygon = *p;
        m_polygons.erase(p++);
        destroyPolygon(polygon);
    }
    removed.clear();
    for (std::set<OptPolygon*>::const_iterator p = created.begin(); p != created.end(); ++p)
        m_polygons.insert(m_polygons.end(), *p);
    return merged;
}

// ELF 0x2F4000. Retail dead code: every polygon is treated as a triangle without checking its size.
void GTSDataOptimiser::removeTooThinTriangles()
{
    std::set<OptPolygon*> removed;
    const double minimumSquared = 0.0025000000000000005;
    for (std::list<OptPolygon*>::iterator p = m_polygons.begin(); p != m_polygons.end(); ++p)
    {
        OptPolygon* polygon = *p;
        if (removed.find(polygon) != removed.end()) continue;
        double shortest = DBL_MAX;
        bool shortEdge = false;
        unsigned char edge = 0;
        OptPolygon* neighbour = 0;
        for (unsigned char i = 0; i <= 2; ++i)
        {
            const DoubleVec3 delta = difference(polygon->m_vertices[i], polygon->m_vertices[(i + 1) % 3]);
            const double lengthSquared = scalarProduct(delta, delta);
            if (minimumSquared > lengthSquared && shortest > lengthSquared)
            {
                shortEdge = true;
                neighbour = polygon->m_neighbours[i];
                edge = i;
                shortest = lengthSquared;
            }
        }
        if (shortEdge)
        {
            for (unsigned char i = 0; i <= 2; ++i)
            {
                if (i == edge)
                {
                    const unsigned char first = (edge + 1) % 3;
                    const unsigned char second = (edge + 2) % 3;
                    OptPolygon* a = polygon->m_neighbours[first];
                    OptPolygon* b = polygon->m_neighbours[second];
                    if (a) a->changeNeighbour(polygon->m_vertices[first], polygon, b);
                    if (b) b->changeNeighbour(polygon->m_vertices[second], polygon, a);
                }
                if (neighbour && neighbour->m_neighbours[i] == polygon)
                {
                    const unsigned char first = (i + 1) % 3;
                    const unsigned char second = (i + 2) % 3;
                    OptPolygon* a = neighbour->m_neighbours[first];
                    OptPolygon* b = neighbour->m_neighbours[second];
                    if (a) a->changeNeighbour(neighbour->m_vertices[first], neighbour, b);
                    if (b) b->changeNeighbour(neighbour->m_vertices[second], neighbour, a);
                }
            }
            const DoubleVec3 oldVertex = polygon->m_vertices[edge];
            const DoubleVec3 newVertex = polygon->m_vertices[(edge + 1) % 3];
            for (std::list<OptPolygon*>::iterator i = m_polygons.begin(); i != m_polygons.end(); ++i)
                if (removed.find(*i) == removed.end()) (*i)->changeVertex(oldVertex, newVertex);
            removed.insert(polygon);
            if (neighbour) removed.insert(neighbour);
        }
        else
        {
            unsigned char longestEdge = 0;
            double longest = 0.0;
            for (unsigned char i = 0; i <= 2; ++i)
            {
                const DoubleVec3 delta = difference(polygon->m_vertices[(i + 1) % 3], polygon->m_vertices[i % 3]);
                const double lengthSquared = scalarProduct(delta, delta);
                if (lengthSquared > longest) { longest = lengthSquared; longestEdge = i; }
            }
            const unsigned char end = (longestEdge + 1) % 3;
            const unsigned char opposite = (longestEdge + 2) % 3;
            const DoubleVec3* a = &polygon->m_vertices[longestEdge];
            const DoubleVec3* b = &polygon->m_vertices[end];
            DoubleVec3 direction = difference(*b, *a);
            normaliseDouble(direction);
            const DoubleVec3 toOpposite = difference(polygon->m_vertices[opposite], *a);
            const double projection = scalarProduct(direction, toOpposite);
            if (!(minimumSquared > scalarProduct(toOpposite, toOpposite) - projection * projection)) continue;
            OptPolygon* before = polygon->m_neighbours[opposite];
            OptPolygon* across = polygon->m_neighbours[longestEdge];
            OptPolygon* after = polygon->m_neighbours[end];
            OptPolygon* split[2] = {};
            const DoubleVec3 midpoint = { (b->x + a->x) * 0.5, (b->y + a->y) * 0.5, (b->z + a->z) * 0.5 };
            if (across)
            {
                split[0] = createPolygon();
                split[1] = createPolygon();
                for (unsigned char half = 0; half <= 1; ++half)
                {
                    OptPolygon* output = split[half];
                    output->m_numberOfVertices = 3;
                    output->m_vertices = new DoubleVec3[output->m_numberOfVertices]();
                    delete output->m_normal;
                    output->m_normal = 0;
                    output->m_neighbours = new OptPolygon*[output->m_numberOfVertices];
                    // shipped defect: material stays at its constructor value of zero.
                    output->m_originalMaterial = across->m_material;
                    const DoubleVec3* normal = across->getNormal();
                    if (output->m_normal) *output->m_normal = *normal;
                    else output->m_normal = new DoubleVec3(*normal);
                    output->m_index = static_cast<unsigned int>(m_polygons.size());
                    output->m_cluster = across->m_cluster;
                    for (unsigned char i = 0; i <= 2; ++i)
                    {
                        const DoubleVec3 delta = difference(across->m_vertices[i], half == 0 ? *b : *a);
                        output->m_vertices[i] = DBL_EPSILON > scalarProduct(delta, delta) ? midpoint : across->m_vertices[i];
                        delete output->m_normal;
                        output->m_normal = 0;
                    }
                    for (unsigned char i = 0; i <= 2; ++i)
                    {
                        const DoubleVec3 current = difference(output->m_vertices[i], midpoint);
                        const DoubleVec3 next = difference(output->m_vertices[(i + 1) % 3], midpoint);
                        if (half == 0)
                        {
                            if (DBL_EPSILON > scalarProduct(next, next)) output->m_neighbours[i] = split[1];
                            else if (DBL_EPSILON > scalarProduct(current, current)) output->m_neighbours[i] = before;
                            else
                            {
                                output->m_neighbours[i] = across->m_neighbours[i];
                                if (output->m_neighbours[i])
                                    output->m_neighbours[i]->changeNeighbour(across->m_vertices[i], across, output);
                            }
                        }
                        else
                        {
                            if (DBL_EPSILON > scalarProduct(current, current)) output->m_neighbours[i] = split[0];
                            else if (DBL_EPSILON > scalarProduct(next, next)) output->m_neighbours[i] = after;
                            else
                            {
                                output->m_neighbours[i] = across->m_neighbours[i];
                                if (output->m_neighbours[i])
                                    output->m_neighbours[i]->changeNeighbour(across->m_vertices[i], across, output);
                            }
                        }
                    }
                    m_polygons.insert(m_polygons.end(), split[half]);
                }
                ++m_graph->m_tempMemberSizes[across->m_cluster];
                removed.insert(across);
            }
            if (before) before->changeNeighbour(polygon->m_vertices[opposite], polygon, split[0]);
            if (after) after->changeNeighbour(polygon->m_vertices[end], polygon, split[1]);
            const DoubleVec3 oldVertex = polygon->m_vertices[opposite];
            for (std::list<OptPolygon*>::iterator i = m_polygons.begin(); i != m_polygons.end(); ++i)
                if (removed.find(*i) == removed.end()) (*i)->changeVertex(oldVertex, midpoint);
            removed.insert(polygon);
        }
    }
    for (std::list<OptPolygon*>::iterator i = m_polygons.begin(); i != m_polygons.end();)
    {
        if (removed.find(*i) == removed.end()) { ++i; continue; }
        OptPolygon* polygon = *i;
        m_graph->m_set18.insert(polygon->m_index);
        --m_graph->m_tempMemberSizes[polygon->m_cluster];
        m_polygons.erase(i++);
        destroyPolygon(polygon);
    }
}

// ELF 0x2F7650. Retail dead code; cluster membership and original material are not repaired here.
bool GTSDataOptimiser::threeToTwoMerge()
{
    // Names of these inlined list/array operations are inferred.
    const auto append = [](OptPolygon* source, int corner, OptPolygon* output,
                           std::list<DoubleVec3>& vertices, std::list<OptPolygon*>& neighbours)
    {
        vertices.insert(vertices.end(), source->m_vertices[corner]);
        OptPolygon* neighbour = source->m_neighbours[corner];
        if (neighbour) neighbour->changeNeighbour(source->m_vertices[corner], source, output);
        neighbours.insert(neighbours.end(), neighbour);
    };
    const auto range = [&append](OptPolygon* source, int start, int end, OptPolygon* output,
                                std::list<DoubleVec3>& vertices, std::list<OptPolygon*>& neighbours)
    {
        for (int i = start; i != end; i = wrappedIndex(i + 1, source->m_numberOfVertices))
            append(source, i, output, vertices, neighbours);
    };
    const auto install = [](OptPolygon* output, const std::list<DoubleVec3>& vertices,
                            const std::list<OptPolygon*>& neighbours, unsigned char material)
    {
        const int count = static_cast<int>(vertices.size());
        DoubleVec3* v = new DoubleVec3[count]();
        OptPolygon** n = new OptPolygon*[count];
        std::list<DoubleVec3>::const_iterator vi = vertices.begin();
        std::list<OptPolygon*>::const_iterator ni = neighbours.begin();
        for (int i = 0; i < count; ++i, ++vi, ++ni) { v[i] = *vi; n[i] = *ni; }
        output->m_numberOfVertices = static_cast<unsigned char>(count);
        output->m_vertices = v;
        delete output->m_normal;
        output->m_normal = 0;
        output->m_neighbours = n;
        output->m_material = material;
    };
    const auto splitExternal = [](OptPolygon* polygon, OptPolygon* target, OptPolygon* first,
                                  OptPolygon* second, const DoubleVec3& intersection, bool wrap)
    {
        const int count = polygon->m_numberOfVertices;
        DoubleVec3* vertices = new DoubleVec3[count + 1]();
        OptPolygon** neighbours = new OptPolygon*[count + 1];
        int corner = 0;
        if (wrap && polygon->m_neighbours[count - 1] == target && polygon->m_neighbours[0] == target)
        {
            for (; corner < count - 1; ++corner)
            {
                vertices[corner] = polygon->m_vertices[corner];
                neighbours[corner] = polygon->m_neighbours[corner];
            }
        }
        else
        {
            for (; polygon->m_neighbours[corner] != target; ++corner)
            {
                vertices[corner] = polygon->m_vertices[corner];
                neighbours[corner] = polygon->m_neighbours[corner];
            }
        }
        vertices[corner] = polygon->m_vertices[corner];
        neighbours[corner] = first;
        ++corner;
        vertices[corner] = intersection;
        neighbours[corner] = second;
        for (; corner < count; ++corner)
        {
            vertices[corner + 1] = polygon->m_vertices[corner];
            neighbours[corner + 1] = polygon->m_neighbours[corner];
        }
        polygon->deleteMembers();
        if (wrap)
        {
            polygon->m_vertices = vertices;
            polygon->m_numberOfVertices = static_cast<unsigned char>(count + 1);
        }
        else
        {
            polygon->m_numberOfVertices = static_cast<unsigned char>(count + 1);
            polygon->m_vertices = vertices;
        }
        delete polygon->m_normal;
        polygon->m_normal = 0;
        polygon->m_neighbours = neighbours;
    };
    const auto replaceAll = [](OptPolygon* polygon, OptPolygon* old, OptPolygon* replacement)
    {
        if (polygon)
            for (int i = 0; i < polygon->m_numberOfVertices; ++i)
                if (polygon->m_neighbours[i] == old) polygon->m_neighbours[i] = replacement;
    };
    const auto eraseInputs = [this](OptPolygon* a, OptPolygon* b, OptPolygon* c, bool stopAtThree)
    {
        int count = 0;
        for (std::list<OptPolygon*>::iterator i = m_polygons.begin();
             i != m_polygons.end() && (!stopAtThree || count < 3);)
        {
            if (*i == a || *i == b || *i == c)
            {
                ++count;
                destroyPolygon(*i);
                m_polygons.erase(i++);
            }
            else ++i;
        }
    };
    const auto sign = [](double value) { return value > 0.0 ? 1.0 : value < 0.0 ? -1.0 : 0.0; };
    for (std::list<OptPolygon*>::iterator p = m_polygons.begin(); p != m_polygons.end(); ++p)
    {
        OptPolygon* polygon = *p;
        const unsigned char material = polygon->m_material;
        const int count = polygon->m_numberOfVertices;
        if (count <= 3) continue;
        for (int edge = 0; edge < count; ++edge)
        {
            OptPolygon* neighbour = polygon->m_neighbours[edge];
            if (!neighbour) continue;
            OptPolygon* third = polygon->m_neighbours[wrappedIndex(edge + 1, count)];
            if (neighbour == third) continue;
            OptPolygon* previous = polygon->m_neighbours[wrappedIndex(edge - 1, count)];
            if (neighbour == previous || neighbour->m_numberOfVertices <= 3) continue;
            const int otherCount = neighbour->m_numberOfVertices;
            int otherEdge = 0;
            for (; otherEdge < otherCount; ++otherEdge)
                if (neighbour->m_neighbours[otherEdge] == polygon) break;
            if (!third || neighbour->m_neighbours[wrappedIndex(otherEdge - 1, otherCount)] != third)
            {
                third = previous;
                if (!third || neighbour->m_neighbours[wrappedIndex(otherEdge + 1, otherCount)] != third) continue;
            }
            if (neighbour->m_material != polygon->m_material || third->m_material != polygon->m_material) continue;
            const DoubleVec3* normal = polygon->getNormal();
            const DoubleVec3* otherNormal = neighbour->getNormal();
            const DoubleVec3* thirdNormal = third->getNormal();
            if (normalAngle(*otherNormal, *normal) > 0.08726646f) continue;
            // shipped defect: the second angle compares the neighbour's normal with itself.
            if (normalAngle(*otherNormal, *otherNormal) > 0.08726646f) continue;
            if (normalAngle(*otherNormal, *thirdNormal) > 0.08726646f) continue;
            const int thirdCount = third->m_numberOfVertices;
            int thirdEdge = 0;
            bool swap = false;
            for (; thirdEdge < thirdCount; ++thirdEdge)
            {
                if (third->m_neighbours[thirdEdge] == polygon)
                {
                    if (thirdEdge <= 0 && third->m_neighbours[thirdCount - 1] == neighbour)
                    { thirdEdge = thirdCount - 1; swap = true; }
                    break;
                }
                if (third->m_neighbours[thirdEdge] == neighbour)
                {
                    swap = true;
                    if (thirdEdge <= 0 && third->m_neighbours[thirdCount - 1] == polygon)
                    { thirdEdge = thirdCount - 1; swap = false; }
                    break;
                }
            }
            OptPolygon* a = swap ? neighbour : polygon;
            OptPolygon* b = swap ? polygon : neighbour;
            const int ac = swap ? otherCount : count;
            const int bc = swap ? count : otherCount;
            const int ai = wrappedIndex((swap ? otherEdge : edge) + 1, ac);
            const int bi = swap ? edge : otherEdge;
            const int aBegin = wrappedIndex(ai + 1, ac);
            const int aEnd = wrappedIndex(ai - 1, ac);
            const int bBegin = wrappedIndex(bi + 1, bc);
            const int bEnd = wrappedIndex(bi - 1, bc);
            const int cBegin = wrappedIndex(thirdEdge + 2, thirdCount);
            DoubleVec2 vector = {}, perpendicular = {};
            getVectorToXZDouble(a->m_vertices[aBegin], a->m_vertices[wrappedIndex(ai + 2, ac)], vector);
            getVectorToXZDouble(third->m_vertices[wrappedIndex(thirdEdge - 1, thirdCount)],
                                third->m_vertices[thirdEdge], perpendicular);
            getNormalDouble(perpendicular, perpendicular);
            if (perpendicular.x * vector.x + perpendicular.y * vector.y > 0.0) continue;
            vector = DoubleVec2{};
            getVectorToXZDouble(b->m_vertices[wrappedIndex(bi - 2, bc)], b->m_vertices[bEnd], vector);
            perpendicular = DoubleVec2{};
            getVectorToXZDouble(third->m_vertices[cBegin],
                                third->m_vertices[wrappedIndex(thirdEdge + 3, thirdCount)], perpendicular);
            getNormalDouble(perpendicular, perpendicular);
            if (perpendicular.x * vector.x + perpendicular.y * vector.y < 0.0) continue;
            vector = DoubleVec2{};
            getVectorToXZDouble(b->m_vertices[bBegin], b->m_vertices[wrappedIndex(bi + 2, bc)], vector);
            perpendicular = DoubleVec2{};
            getVectorToXZDouble(a->m_vertices[aEnd], a->m_vertices[wrappedIndex(ai - 2, ac)], perpendicular);
            getNormalDouble(perpendicular, perpendicular);
            if (perpendicular.x * vector.x + perpendicular.y * vector.y <= 0.0)
            {
                OptPolygon* output = createPolygon();
                std::list<DoubleVec3> vertices;
                std::list<OptPolygon*> neighbours;
                range(a, aBegin, aEnd, output, vertices, neighbours);
                range(b, bBegin, bEnd, output, vertices, neighbours);
                range(third, cBegin, thirdEdge, output, vertices, neighbours);
                eraseInputs(a, b, third, false);
                install(output, vertices, neighbours, material);
                m_polygons.insert(m_polygons.end(), output);
                return true;
            }
            DoubleVec3 intersection = {};
            const DoubleVec3* lineEnd = &a->m_vertices[aEnd];
            const DoubleVec3* lineStart = &a->m_vertices[wrappedIndex(ai - 2, ac)];
            perpendicular = DoubleVec2{};
            getVectorToXZDouble(*lineStart, *lineEnd, perpendicular);
            getNormalDouble(perpendicular, perpendicular);
            vector = DoubleVec2{};
            getVectorToXZDouble(a->m_vertices[ai], *lineEnd, vector);
            if (perpendicular.x * vector.x + perpendicular.y * vector.y != 0.0)
                for (int cut = wrappedIndex(bi + 2, bc); cut != bEnd; cut = wrappedIndex(cut + 1, bc))
                {
                    const int next = wrappedIndex(cut + 1, bc);
                    DoubleVec2 currentSide = {}, nextSide = {};
                    getVectorToXZDouble(*lineEnd, b->m_vertices[cut], currentSide);
                    const double firstSide = currentSide.x * perpendicular.x + currentSide.y * perpendicular.y;
                    getVectorToXZDouble(*lineEnd, b->m_vertices[next], nextSide);
                    const double secondSide = nextSide.x * perpendicular.x + nextSide.y * perpendicular.y;
                    if (sign(firstSide) == sign(secondSide) || secondSide == 0.0) continue;
                    lineIntersect(b->m_vertices[cut], b->m_vertices[next], *lineStart, *lineEnd, intersection);
                    const DoubleVec3 delta = difference(b->m_vertices[cut], intersection);
                    const bool split = !(static_cast<double>(0.1f) > scalarProduct(delta, delta));
                    if (!split) intersection = b->m_vertices[cut];
                    OptPolygon* first = createPolygon();
                    OptPolygon* second = createPolygon();
                    {
                        std::list<DoubleVec3> vertices;
                        std::list<OptPolygon*> neighbours;
                        range(a, aBegin, aEnd, first, vertices, neighbours);
                        vertices.insert(vertices.end(), a->m_vertices[aEnd]);
                        neighbours.insert(neighbours.end(), second);
                        if (split)
                        {
                            vertices.insert(vertices.end(), intersection);
                            neighbours.insert(neighbours.end(), b->m_neighbours[cut]);
                        }
                        else append(b, cut, first, vertices, neighbours);
                        range(b, next, bEnd, first, vertices, neighbours);
                        range(third, cBegin, thirdEdge, first, vertices, neighbours);
                        install(first, vertices, neighbours, material);
                    }
                    {
                        std::list<DoubleVec3> vertices;
                        std::list<OptPolygon*> neighbours;
                        range(b, bBegin, cut, second, vertices, neighbours);
                        if (split)
                        {
                            append(b, cut, second, vertices, neighbours);
                            vertices.insert(vertices.end(), intersection);
                            neighbours.insert(neighbours.end(), first);
                        }
                        else
                        {
                            vertices.insert(vertices.end(), b->m_vertices[cut]);
                            neighbours.insert(neighbours.end(), first);
                        }
                        install(second, vertices, neighbours, material);
                    }
                    if (split && b->m_neighbours[cut])
                        splitExternal(b->m_neighbours[cut], second, first, second, intersection, false);
                    eraseInputs(a, b, third, false);
                    m_polygons.insert(m_polygons.end(), first);
                    m_polygons.insert(m_polygons.end(), second);
                    return true;
                }
            lineEnd = &b->m_vertices[bBegin];
            lineStart = &b->m_vertices[wrappedIndex(bi + 2, bc)];
            perpendicular = DoubleVec2{};
            getVectorToXZDouble(*lineStart, *lineEnd, perpendicular);
            getNormalDouble(perpendicular, perpendicular);
            vector = DoubleVec2{};
            getVectorToXZDouble(b->m_vertices[bi], *lineEnd, vector);
            if (perpendicular.x * vector.x + perpendicular.y * vector.y == 0.0) continue;
            for (int cut = aBegin; cut != wrappedIndex(ai - 2, ac); cut = wrappedIndex(cut + 1, ac))
            {
                const int next = wrappedIndex(cut + 1, ac);
                DoubleVec2 currentSide = {}, nextSide = {};
                getVectorToXZDouble(*lineEnd, a->m_vertices[cut], currentSide);
                const double firstSide = currentSide.x * perpendicular.x + currentSide.y * perpendicular.y;
                getVectorToXZDouble(*lineEnd, a->m_vertices[next], nextSide);
                const double secondSide = nextSide.x * perpendicular.x + nextSide.y * perpendicular.y;
                if (sign(firstSide) == sign(secondSide) || secondSide == 0.0) continue;
                lineIntersect(a->m_vertices[cut], a->m_vertices[next], *lineStart, *lineEnd, intersection);
                const DoubleVec3 delta = difference(a->m_vertices[cut], intersection);
                const bool split = !(static_cast<double>(0.1f) > scalarProduct(delta, delta));
                if (!split) intersection = a->m_vertices[cut];
                OptPolygon* first = createPolygon();
                OptPolygon* second = createPolygon();
                {
                    std::list<DoubleVec3> vertices;
                    std::list<OptPolygon*> neighbours;
                    range(a, aBegin, next, first, vertices, neighbours);
                    if (split)
                    {
                        vertices.insert(vertices.end(), intersection);
                        neighbours.insert(neighbours.end(), second);
                    }
                    else
                    {
                        replaceAll(a->m_neighbours[wrappedIndex(next - 1, ac)], a, first);
                        neighbours.back() = second;
                    }
                    range(b, bBegin, bEnd, first, vertices, neighbours);
                    range(third, cBegin, wrappedIndex(thirdEdge, thirdCount), first, vertices, neighbours);
                    install(first, vertices, neighbours, material);
                }
                {
                    std::list<DoubleVec3> vertices;
                    std::list<OptPolygon*> neighbours;
                    range(a, next, aEnd, second, vertices, neighbours);
                    vertices.insert(vertices.end(), a->m_vertices[aEnd]);
                    neighbours.insert(neighbours.end(), first);
                    OptPolygon* neighbour = a->m_neighbours[cut];
                    if (split)
                    {
                        vertices.insert(vertices.end(), intersection);
                        replaceAll(neighbour, a, second);
                    }
                    else
                    {
                        vertices.insert(vertices.end(), a->m_vertices[cut]);
                        if (neighbour) neighbour->changeNeighbour(a->m_vertices[cut], a, second);
                    }
                    neighbours.insert(neighbours.end(), neighbour);
                    install(second, vertices, neighbours, material);
                }
                if (split && a->m_neighbours[cut])
                    splitExternal(a->m_neighbours[cut], first, second, first, intersection, true);
                eraseInputs(a, b, third, true);
                m_polygons.insert(m_polygons.end(), first);
                m_polygons.insert(m_polygons.end(), second);
                return true;
            }
        }
    }
    return false;
}

// ELF 0x2FBBA0.
DoubleVec2& GTSDataOptimiser::getVectorToXZDouble(const DoubleVec3& from, const DoubleVec3& to,
                                               DoubleVec2& result)
{
    result.x = to.x - from.x;
    result.y = to.z - from.z;
    return result;
}

// ELF 0x2FBBC0. Preserve the read before either write for aliased input/output.
DoubleVec2& GTSDataOptimiser::getNormalDouble(const DoubleVec2& vector, DoubleVec2& result)
{
    const double negativeX = -vector.x;
    result.x = vector.y;
    result.y = negativeX;
    return result;
}

// ELF 0x2FBBE0.
bool GTSDataOptimiser::lineIntersect(const DoubleVec3& a, const DoubleVec3& b,
    const DoubleVec3& c, const DoubleVec3& d, DoubleVec3& result)
{
    const DoubleVec3 first = difference(b, a);
    DoubleVec3 firstUnit = first;
    normaliseDouble(firstUnit);
    const DoubleVec3 second = difference(d, c);
    DoubleVec3 secondUnit = second;
    normaliseDouble(secondUnit);
    if (static_cast<double>(0.001f) > std::fabs(std::fabs(scalarProduct(firstUnit, secondUnit)) - 1.0))
        return false;
    const DoubleVec3 origin = a;
    const DoubleVec3 separation = difference(c, a);
    const DoubleVec3 cross = vectorProduct(first, second);
    const DoubleVec3 numerator = vectorProduct(separation, second);
    const double denominator = scalarProduct(cross, cross);
    if (denominator != 0.0)
    {
        const double scale = scalarProduct(cross, numerator) / denominator;
        const DoubleVec3 displacement = { first.x * scale, first.y * scale, first.z * scale };
        result = displacement;
        // shipped defect: output aliasing a adds the displacement to itself.
        result.x = displacement.x + a.x;
        result.y = displacement.y + a.y;
        result.z = displacement.z + a.z;
        return true;
    }
    if (scalarProduct(numerator, numerator) == 0.0) { result = origin; return true; }
    return false;
}

// ELF 0x2FC080. Tests whether first occurs in second's neighbour array.
bool GTSDataOptimiser::isNeighbourOfPolygon(OptPolygon* first, OptPolygon* second) const
{
    const unsigned char count = second->m_numberOfVertices;
    for (unsigned char edge = 0; edge < count; ++edge)
        if (second->m_neighbours[edge] == first) return true;
    return false;
}

// ELF 0x2FC0C0.
void GTSDataOptimiser::validateNeighbours(OptPolygon* polygon) const
{
    unsigned char count = polygon->m_numberOfVertices;
    for (unsigned int edge = 0; edge < count; ++edge)
    {
        OptPolygon* neighbour = polygon->m_neighbours[edge];
        if (!neighbour) continue;
        bool found = false;
        const DoubleVec3& vertex = polygon->m_vertices[edge];
        for (unsigned int corner = 0; corner < neighbour->m_numberOfVertices; ++corner)
        {
            const DoubleVec3 delta = difference(vertex, neighbour->m_vertices[corner]);
            if (DBL_EPSILON > scalarProduct(delta, delta)) { found = true; break; }
        }
        if (!found)
        {
            Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/GTSDataOptimiser.cpp", 2429, "")
                << "Couldn't find common neighbour vertex for polygon " << static_cast<const void*>(polygon);
            count = polygon->m_numberOfVertices;
        }
    }
}

// ELF 0x2FC320.
void GTSDataOptimiser::validateNeighbourRelations(const std::set<OptPolygon*>& removed)
{
    for (OptPolygon* polygon = initPolygonIteration(); polygon; polygon = getNextPolygon())
    {
        if (removed.find(polygon) != removed.end()) continue;
        validateNeighbours(polygon);
        const unsigned char count = polygon->m_numberOfVertices;
        for (unsigned char edge = 0; edge < count; ++edge)
            if (removed.find(polygon->m_neighbours[edge]) != removed.end())
                Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/GTSDataOptimiser.cpp", 2455, "")
                    << "Reference to removed polygon!";
    }
}

} } }
