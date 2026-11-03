#include "ClusterGraph.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x31DAE0, __tcf_0 0x31DB40, _GLOBAL__I 0x31DB70).
#include <iostream>
#include <cmath>
#include <sstream>                                      // drawCluster
#include <vector>
#include <cfloat>

#include "../NavMesh/QuadTree.h"
#include "../NavMesh/GTSDataOptimiser.h"
#include "../Material/MaterialManager.h"
#include "../../Main/AISettingsInterfaces.h"
#include "../../AIUtils/checkPointer.h"
#include "../../Main/CommonSingletonServer.h"
#include "../../DrawPrimitives/DPManager.h"             // IDPStaticCreator
#include "../../Main/AIPathfinding.h"   // getPortalCost, isValidClusterType
#include "../../Main/IAIPathfinding.h"
#include "../../../../../IO/File/File.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x314380.
void ClusterGraph::readGTSDataFromFile(QuadTree* tree, io::File& file)
{
    clean();
    file.read(m_numberOfClusters);
    m_clusters = checkPointer(new Cluster[m_numberOfClusters]);
    m_clusterPolygons = checkPointer(new unsigned int[tree->m_numberOfPolygons]);
    for (unsigned short i = 0; i < m_numberOfClusters; ++i)
    {
        Cluster& cluster = m_clusters[i];
        unsigned int count;
        cluster.readGTSDataFromFile(m_clusterPolygons + m_numberOfClusterPolygons,
                                    count, m_numberOfClusterPolygons, file);
        const unsigned int first = cluster.m_firstPolygon;
        const unsigned int size = cluster.m_numberOfPolygons;
        for (unsigned int j = 0; j < size; ++j)
        {
            const unsigned int id = m_clusterPolygons[first + j];
            Polygon* polygon = tree->getPolygon(id);
            DoublePolygon* doublePolygon = tree->getPolygonDouble(id);
            polygon->m_cluster = i;
            doublePolygon->m_cluster = i;
        }
        m_numberOfClusterPolygons += count;
    }
}

// ELF 0x315160. The radius argument is unused in the shipped body.
void ClusterGraph::createPortals(int map, QuadTree* tree, float)
{
    std::list<std::pair<unsigned short, int> > costTypes;
    AIPathfinding* const pathfinding = static_cast<AIPathfinding*>(IAIPathfinding::instance);
    for (unsigned short type = 0; pathfinding->isValidClusterType(map, type); ++type)
        costTypes.push_back(std::make_pair(type,
            pathfinding->getEquipmentTypeFromClusterCostType(map, type)));
    PortalBuildMap portals;
    for (unsigned short i = 0; i < m_numberOfClusters; ++i)
    {
        const Cluster& cluster = m_clusters[i];
        ClusterEdges edges;
        VertexClusterEdges vertices;
        buildClusterEdges(cluster, i, tree, vertices, edges);
        buildPortalsFromClusterEdges(i, tree, vertices, edges, portals);
    }
    if (portals.empty())
    {
        m_numberOfPortals = 0;
        for (unsigned short i = 0; i < m_numberOfClusters; ++i)
        {
            m_clusters[i].m_firstPortal = 0;
            m_clusters[i].m_numberOfPortals = 0;
        }
        return;
    }
    for (PortalBuildMap::const_iterator i = portals.begin(); i != portals.end(); ++i)
        m_numberOfPortals += static_cast<unsigned int>(i->second.size());
    m_numberOfPortals *= static_cast<unsigned int>(costTypes.size());
    m_portals = checkPointer(new ClusterPortal[m_numberOfPortals]);
    unsigned short portalIndex = 0;
    for (unsigned short i = 0; i < m_numberOfClusters; ++i)
    {
        Cluster& cluster = m_clusters[i];
        cluster.m_firstPortal = portalIndex;
        PortalBuildMap::const_iterator found = portals.find(i);
        if (found == portals.end())
        {
            cluster.m_numberOfPortals = 0;
            Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp", 468, "")
                << static_cast<unsigned long>(i) << " is a lost cluster!";
            continue;
        }
        for (std::list<std::pair<unsigned short, int> >::const_iterator type = costTypes.begin();
             type != costTypes.end(); ++type)
        {
            MaterialManager* materials = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
                ->getVehicleMaterialManager(type->second);
            cluster.m_numberOfPortals = static_cast<unsigned short>(found->second.size());
            for (std::list<ClusterPortalBuildInfo>::const_iterator p = found->second.begin();
                 p != found->second.end(); ++p, ++portalIndex)
            {
                unsigned int polygon;
                ClusterPortal& portal = m_portals[portalIndex];
                if (!calculateMiddleVertex(i, *p, tree, portalIndex, materials, portal.m_position, polygon))
                {
                    float bestDistance = FLT_MAX;
                    bool success = false;
                    bool checkMaterial = true;
                    const Cluster& neighbour = m_clusters[p->neighbour];
                    for (int pass = 0; pass < 2; ++pass)
                    {
                        for (int side = 0; side < 2; ++side)
                        {
                            const Cluster& candidateCluster = side == 0 ? cluster : neighbour;
                            const unsigned int first = candidateCluster.m_firstPolygon;
                            const unsigned int count = candidateCluster.m_numberOfPolygons;
                            for (unsigned int j = 0; j < count; ++j)
                            {
                                const unsigned int id = m_clusterPolygons[first + j];
                                const Polygon* candidate = tree->getPolygon(id);
                                if (!candidate->m_material ||
                                    (checkMaterial && !materials->isValidMaterial(candidate->m_material)))
                                    continue;
                                const Vec3& a = tree->m_vertices[p->firstVertex];
                                const Vec3& b = tree->m_vertices[p->secondVertex];
                                Vec3 midpoint;
                                midpoint.x = (b.x + a.x) * 0.5f;
                                midpoint.y = (b.y + a.y) * 0.5f;
                                midpoint.z = (b.z + a.z) * 0.5f;
                                Vec3 centre = {};
                                tree->getCentreOfPolygon(centre, id);
                                // shipped defect: compares against the changing output position, not centre.
                                const float dx = midpoint.x - portal.m_position.x;
                                const float dy = midpoint.y - portal.m_position.y;
                                const float dz = midpoint.z - portal.m_position.z;
                                const float distance = dx * dx + dy * dy + dz * dz;
                                if (bestDistance > distance)
                                {
                                    portal.m_position = centre;
                                    bestDistance = distance;
                                    success = true;
                                    polygon = id;
                                }
                            }
                        }
                        checkMaterial = false;
                        if (success) break;
                    }
                    if (!success)
                    {
                        portal.m_neighbourCluster = 0xFFFF;
                        portal.m_position.x = portal.m_position.y = portal.m_position.z = 0.0f;
                        portal.m_polygon = 0xFFFFFFFF;
                        continue;
                    }
                }
                portal.m_neighbourCluster = p->neighbour;
                portal.m_polygon = polygon;
            }
        }
    }
}

// ELF 0x315DE0.
void ClusterGraph::buildClusterEdges(const Cluster& cluster, unsigned short clusterId,
    QuadTree* tree, VertexClusterEdges& vertices, ClusterEdges& edges)
{
    const unsigned int first = cluster.m_firstPolygon;
    const unsigned int count = cluster.m_numberOfPolygons;
    for (unsigned int j = 0; j < count; ++j)
    {
        const unsigned int id = m_clusterPolygons[first + j];
        const Polygon* polygon = tree->getPolygon(id);
        for (unsigned char edge = 0; edge < polygon->m_numberOfVertices; ++edge)
        {
            const unsigned int neighbourId = tree->getNeighbourOfPolygon(id, edge);
            const Polygon* neighbour = tree->getPolygon(neighbourId);
            if (!neighbour || neighbour->m_cluster == polygon->m_cluster) continue;
            const unsigned int a = tree->getVertexIndexForPolygon(id, edge);
            const unsigned int b = tree->getVertexIndexForPolygon(id,
                static_cast<unsigned char>((edge + 1) % polygon->m_numberOfVertices));
            for (int endpoint = 0; endpoint < 2; ++endpoint)
            {
                const unsigned int vertex = endpoint == 0 ? a : b;
                VertexClusterEdges::iterator found = vertices.find(vertex);
                if (found == vertices.end())
                {
                    std::map<unsigned short, unsigned int> clusters;
                    clusters.insert(std::make_pair(neighbour->m_cluster, 1U));
                    vertices.insert(std::make_pair(vertex, clusters));
                }
                else
                {
                    std::map<unsigned short, unsigned int>::iterator entry = found->second.find(neighbour->m_cluster);
                    if (entry == found->second.end())
                        found->second.insert(std::make_pair(neighbour->m_cluster, 1U));
                    else if (++entry->second > 2)
                    {
                        printClusterInfo(clusterId, tree, false);
                        Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp",
                              endpoint == 0 ? 510 : 539, "") << "Bailing out!";
                    }
                }
            }
            ClusterEdges::iterator found = edges.find(neighbour->m_cluster);
            const std::pair<unsigned int, unsigned int> edgePair(a, b);
            if (found == edges.end())
            {
                std::list<std::pair<unsigned int, unsigned int> > list;
                list.push_back(edgePair);
                edges.insert(std::make_pair(neighbour->m_cluster, list));
            }
            else
                found->second.push_back(edgePair);
        }
    }
}

// ELF 0x316EC0.
void ClusterGraph::buildPortalsFromClusterEdges(unsigned short cluster, QuadTree* tree,
    const VertexClusterEdges& vertices, const ClusterEdges& edges, PortalBuildMap& output)
{
    typedef std::pair<unsigned int, unsigned int> Edge;
    std::list<ClusterPortalBuildInfo> portals;
    for (ClusterEdges::const_iterator group = edges.begin(); group != edges.end(); ++group)
    {
        std::list<Edge> starts;
        for (std::list<Edge>::const_iterator e = group->second.begin(); e != group->second.end(); ++e)
        {
            VertexClusterEdges::const_iterator a = vertices.find(e->first);
            VertexClusterEdges::const_iterator b = vertices.find(e->second);
            std::map<unsigned short, unsigned int>::const_iterator ac = a->second.find(group->first);
            std::map<unsigned short, unsigned int>::const_iterator bc = b->second.find(group->first);
            if (ac->second == 1 || bc->second == 1) starts.push_back(*e);
        }
        std::set<Edge> visitedEdges;
        if (starts.empty())
        {
            // shipped defect: takes the first group, even when processing a later group.
            const ClusterEdges::const_iterator first = edges.begin();
            ClusterPortalBuildInfo portal;
            portal.firstVertex = first->second.begin()->first;
            portal.secondVertex = first->second.begin()->first;
            portal.neighbour = first->first;
            portals.push_back(portal);
            output.insert(std::make_pair(cluster, portals));
            continue;
        }
        for (std::list<Edge>::const_iterator start = starts.begin(); start != starts.end(); ++start)
        {
            if (visitedEdges.find(*start) != visitedEdges.end()) continue;
            if (visitedEdges.find(Edge(start->second, start->first)) != visitedEdges.end()) continue;
            VertexClusterEdges::const_iterator a = vertices.find(start->first);
            VertexClusterEdges::const_iterator b = vertices.find(start->second);
            std::map<unsigned short, unsigned int>::const_iterator ac = a->second.find(group->first);
            std::map<unsigned short, unsigned int>::const_iterator bc = b->second.find(group->first);
            unsigned int first;
            unsigned int current;
            unsigned int last = 0xFFFFFFFF;
            bool complete = false;
            if (ac->second == 1)
            {
                first = start->first;
                current = start->second;
                if (bc->second == 1) { last = start->second; complete = true; }
            }
            else { first = start->second; current = start->first; }
            std::set<unsigned int> visited;
            visited.insert(first);
            visited.insert(current);
            while (!complete)
            {
                bool progressed = false;
                for (std::list<Edge>::const_iterator e = group->second.begin(); e != group->second.end(); ++e)
                {
                    VertexClusterEdges::const_iterator va = vertices.find(e->first);
                    VertexClusterEdges::const_iterator vb = vertices.find(e->second);
                    std::map<unsigned short, unsigned int>::const_iterator ea = va->second.find(group->first);
                    std::map<unsigned short, unsigned int>::const_iterator eb = vb->second.find(group->first);
                    if (visited.find(e->first) != visited.end() && visited.find(e->second) != visited.end()) continue;
                    unsigned int next;
                    unsigned int degree;
                    if (e->first == current) { next = e->second; degree = eb->second; }
                    else if (e->second == current) { next = e->first; degree = ea->second; }
                    else continue;
                    if (degree == 1)
                    {
                        last = next;
                        visitedEdges.insert(*start);
                        visitedEdges.insert(Edge(current, last));
                        complete = true;
                    }
                    else
                    {
                        current = next;
                        visited.insert(current);
                        progressed = true;
                    }
                    break;
                }
                if (complete) break;
                if (progressed) continue;
                for (VertexClusterEdges::const_iterator v = vertices.begin(); v != vertices.end(); ++v)
                    for (std::map<unsigned short, unsigned int>::const_iterator c = v->second.begin(); c != v->second.end(); ++c) {}
                printClusterInfo(cluster, tree, false);
                printClusterInfo(group->first, tree, false);
                Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp", 756, "") << "Bailing out!";
                break;
            }
            if (complete)
            {
                ClusterPortalBuildInfo portal;
                portal.firstVertex = first < last ? first : last;
                portal.secondVertex = first < last ? last : first;
                portal.neighbour = group->first;
                portals.push_back(portal);
            }
        }
    }
    if (!portals.empty() && portals.size()) output.insert(std::make_pair(cluster, portals));
    else
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp", 782, "")
            << "Could not find any portals for cluster: " << static_cast<unsigned long>(cluster);
}

// ELF 0x318270. The portal index argument is unused.
bool ClusterGraph::calculateMiddleVertex(unsigned short cluster, const ClusterPortalBuildInfo& info,
    QuadTree* tree, unsigned short, MaterialManager* materials, Vec3& point, unsigned int& polygon) const
{
    const Vec3& a = tree->m_vertices[info.firstVertex];
    const Vec3& b = tree->m_vertices[info.secondVertex];
    point.x = (b.x + a.x) * 0.5f;
    point.y = (b.y + a.y) * 0.5f;
    point.z = (b.z + a.z) * 0.5f;
    const bool found = tree->findPolygon(point, polygon, materials, 100.0f);
    const Polygon* result = tree->getPolygon(polygon);
    if (found)
    {
        const bool valid = materials->isValidMaterial(result->m_material);
        const bool rightCluster = result->m_cluster == cluster || result->m_cluster == info.neighbour;
        if (valid && rightCluster) return true;
    }
    Vec3 direction;
    direction.x = -(b.z - a.z);
    direction.y = b.y - a.y;
    direction.z = b.x - a.x;
    const float lengthSquared = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (!(FLT_EPSILON > std::fabs(1.0f - lengthSquared)))
    {
        if (!(FLT_EPSILON > std::fabs(lengthSquared)))
        {
            const float scale = 1.0f / std::sqrt(lengthSquared);
            direction.x *= scale; direction.y *= scale; direction.z *= scale;
        }
        else direction.x = direction.y = direction.z = 0.0f;
    }
    const Vec3 origin = point;
    for (unsigned int side = 0; side <= 1; ++side)
    {
        float distance = 0.1f;
        for (int step = 0; step <= 999; ++step)
        {
            point.x = direction.x * distance + origin.x;
            point.y = direction.y * distance + origin.y;
            point.z = direction.z * distance + origin.z;
            if (tree->findPolygon(point, polygon, materials, 100.0f))
            {
                result = tree->getPolygon(polygon);
                const bool valid = materials->isValidMaterial(result->m_material);
                const bool rightCluster = result->m_cluster == cluster || result->m_cluster == info.neighbour;
                if (valid && rightCluster) return true;
            }
            distance += 0.1f;
        }
        direction.x = -direction.x; direction.y = -direction.y; direction.z = -direction.z;
    }
    return false;
}

// ELF 0x318610. Only polygon ranges are rebuilt; other Cluster fields remain uninitialised.
void ClusterGraph::adjustPolygonsInClusters()
{
    Cluster* adjusted = checkPointer(new Cluster[m_numberOfClusters]);
    unsigned int first = 0;
    for (unsigned int i = 0; i < m_numberOfClusters; ++i)
    {
        const Cluster& old = m_clusters[i];
        const unsigned int count = old.m_numberOfPolygons;
        const unsigned int oldFirst = old.m_firstPolygon;
        adjusted[i].m_numberOfPolygons = count;
        for (unsigned int j = 0; j < count; ++j)
            if (m_set18.find(m_clusterPolygons[oldFirst + j]) != m_set18.end())
                --adjusted[i].m_numberOfPolygons;
        adjusted[i].m_firstPolygon = first;
        first += adjusted[i].m_numberOfPolygons;
    }
    delete[] m_clusters;
    m_clusters = adjusted;
}

// ELF 0x318910.
void ClusterGraph::movePolygonBetweenClusters(unsigned short source, unsigned short destination,
                                              unsigned int polygon)
{
    Cluster* adjusted = checkPointer(new Cluster[m_numberOfClusters]);
    unsigned int* polygons = checkPointer(new unsigned int[m_numberOfClusterPolygons]);
    for (unsigned int i = 0; i < m_numberOfClusters; ++i)
    {
        if (i == source) adjusted[i].m_numberOfPolygons = m_clusters[i].m_numberOfPolygons - 1;
        else if (i == destination) adjusted[i].m_numberOfPolygons = m_clusters[destination].m_numberOfPolygons + 1;
        else adjusted[i].m_numberOfPolygons = m_clusters[i].m_numberOfPolygons;
    }
    for (unsigned int i = 0; i < m_numberOfClusterPolygons; ++i)
    {
        // shipped defect: skipped entries leave holes; insertion also skips the old destination entry.
        if (m_clusterPolygons[i] == polygon) continue;
        if (m_clusterPolygons[i] == m_clusterPolygons[m_clusters[destination].m_firstPolygon])
            polygons[i++] = polygon;
        polygons[i] = m_clusterPolygons[i];
    }
    unsigned int first = 0;
    for (unsigned int i = 0; i < m_numberOfClusters; ++i)
    {
        adjusted[i].m_firstPolygon = first;
        first += adjusted[i].m_numberOfPolygons;
    }
    delete[] m_clusters;
    m_clusters = adjusted;
    delete[] m_clusterPolygons;
    m_clusterPolygons = polygons;
}

// ELF 0x318D30.
void ClusterGraph::addPolygonsToNewCluster(const std::set<unsigned int>& selected,
                                          std::list<OptPolygon*>& mesh)
{
    Cluster* adjusted = checkPointer(new Cluster[m_numberOfClusters + 1]);
    unsigned int* polygons = checkPointer(new unsigned int[m_numberOfClusterPolygons]);
    for (unsigned int i = 0; i < m_numberOfClusters; ++i)
        adjusted[i].m_numberOfPolygons = m_clusters[i].m_numberOfPolygons;
    unsigned int write = 0;
    for (unsigned int i = 0; i < m_numberOfClusters; ++i)
    {
        const unsigned int first = m_clusters[i].m_firstPolygon;
        const unsigned int count = m_clusters[i].m_numberOfPolygons;
        for (unsigned int j = 0; j < count; ++j)
        {
            const unsigned int id = m_clusterPolygons[first + j];
            if (selected.find(id) == selected.end()) polygons[write++] = id;
            else --adjusted[i].m_numberOfPolygons;
        }
    }
    unsigned int first = 0;
    for (unsigned int i = 0; i < m_numberOfClusters; ++i)
    {
        adjusted[i].m_firstPolygon = first;
        first += adjusted[i].m_numberOfPolygons;
    }
    adjusted[m_numberOfClusters].m_numberOfPolygons = static_cast<unsigned int>(selected.size());
    // shipped defect: the empty-graph case reads the preceding record.
    adjusted[m_numberOfClusters].m_firstPolygon =
        adjusted[m_numberOfClusters - 1].m_firstPolygon + adjusted[m_numberOfClusters - 1].m_numberOfPolygons;
    first = adjusted[m_numberOfClusters].m_firstPolygon;
    const unsigned int count = adjusted[m_numberOfClusters].m_numberOfPolygons;
    std::set<unsigned int>::const_iterator id = selected.begin();
    for (unsigned int j = 0; j < count; ++j, ++id) polygons[first + j] = *id;
    for (std::list<OptPolygon*>::iterator p = mesh.begin(); p != mesh.end(); ++p)
        if (selected.find((*p)->m_index) != selected.end())
            (*p)->m_cluster = static_cast<unsigned short>(m_numberOfClusters);
    ++m_numberOfClusters;
    delete[] m_clusters;
    m_clusters = adjusted;
    delete[] m_clusterPolygons;
    m_clusterPolygons = polygons;
}

// ELF 0x319330.
void ClusterGraph::updateClustersAfterOptimisation(const DynamicPolygonMap& dynamicPolygons,
    std::list<OptPolygon*>& mesh, ClusterPolygonMap& clusters)
{
    adjustPolygonsInClusters();
    setPolygonIndices(clusters);
    moveDynamicObjectPolygonsToSeparateClusters(mesh, dynamicPolygons, clusters);
    clearEmptyClusters(mesh);
    setPolygonIndices(clusters);
    fixClusterGraphConnectivity(mesh, false);
    m_set18.clear();
}

// ELF 0x319400.
void ClusterGraph::clearEmptyClusters(std::list<OptPolygon*>& mesh)
{
    std::list<unsigned short> empty;
    unsigned int count = m_numberOfClusters;
    for (unsigned short i = 0; i < m_numberOfClusters; ++i)
        if (!m_clusters[i].m_numberOfPolygons) { empty.push_back(i); --count; }
    if (empty.empty() || !empty.size()) return;
    while (!empty.empty() && empty.size())
    {
        const unsigned short removed = *empty.begin();
        for (std::list<OptPolygon*>::iterator p = mesh.begin(); p != mesh.end(); ++p)
            if ((*p)->m_cluster > removed) --(*p)->m_cluster;
        empty.erase(empty.begin());
        for (std::list<unsigned short>::iterator i = empty.begin(); i != empty.end(); ++i) --*i;
    }
    Cluster* adjusted = checkPointer(new Cluster[count]);
    unsigned int write = 0;
    for (unsigned short i = 0; i < m_numberOfClusters; ++i)
        if (m_clusters[i].m_numberOfPolygons)
        {
            adjusted[write].m_numberOfPolygons = m_clusters[i].m_numberOfPolygons;
            adjusted[write].m_firstPolygon = m_clusters[i].m_firstPolygon;
            ++write;
        }
    m_numberOfClusters = count;
    delete[] m_clusters;
    m_clusters = adjusted;
}

// ELF 0x319870.
void ClusterGraph::splitClustersCutByDeepWater(const std::list<OptPolygon*>& mesh)
{
    bool split;
    do
    {
        std::set<OptPolygon*> checked;
        split = false;
        for (std::list<OptPolygon*>::const_iterator p = mesh.begin(); p != mesh.end(); ++p)
        {
            OptPolygon* seed = *p;
            if (checked.find(seed) != checked.end()) continue;
            std::set<OptPolygon*> connected;
            floodFillPolygonsInCluster(seed, connected, true);
            if (connected.empty()) continue;
            std::set<OptPolygon*> invalid;
            getAllDeepWaterAndInvalidPolygonsInCluster(seed->m_cluster, mesh, invalid);
            if (connected.size() + invalid.size() == m_clusters[seed->m_cluster].m_numberOfPolygons)
            {
                checked.insert(connected.begin(), connected.end());
                continue;
            }
            split = true;
            std::set<OptPolygon*> other;
            for (std::list<OptPolygon*>::const_iterator q = mesh.begin(); q != mesh.end(); ++q)
                if ((*q)->m_cluster == seed->m_cluster && connected.find(*q) == connected.end())
                    other.insert(*q);
            Cluster* adjusted = checkPointer(new Cluster[m_numberOfClusters + 1]);
            unsigned int first = 0;
            for (unsigned short i = 0; i < m_numberOfClusters; ++i)
            {
                if (i == seed->m_cluster)
                {
                    // shipped defect: decrements once regardless of the size of other; leaves the range unset.
                    adjusted[i].m_numberOfPolygons = m_clusters[i].m_numberOfPolygons - 1;
                    for (std::set<OptPolygon*>::iterator q = other.begin(); q != other.end(); ++q)
                        (*q)->m_cluster = static_cast<unsigned short>(m_numberOfClusters);
                }
                else adjusted[i].m_firstPolygon = first;
                // shipped defect: other counts and the new record were never initialised.
                first += adjusted[i].m_numberOfPolygons;
            }
            delete[] m_clusters;
            ++m_numberOfClusters;
            m_clusters = adjusted;
            break;
        }
    } while (split);
}

// ELF 0x31A020.
void ClusterGraph::moveDynamicObjectPolygonsToSeparateClusters(const std::list<OptPolygon*>& mesh,
    const DynamicPolygonMap& dynamicPolygons, ClusterPolygonMap& clusters)
{
    typedef std::map<unsigned int, std::vector<unsigned int> > ObjectPolygons;
    ObjectPolygons objects;
    for (DynamicPolygonMap::const_iterator p = dynamicPolygons.begin(); p != dynamicPolygons.end(); ++p)
    {
        ObjectPolygons::iterator object = objects.find(p->second.first);
        if (object == objects.end())
        {
            std::vector<unsigned int> polygons;
            polygons.push_back(p->first);
            objects.insert(std::make_pair(p->second.first, polygons));
        }
        else object->second.push_back(p->first);
    }
    const int objectCount = static_cast<int>(objects.size());
    if (!objectCount) return;
    Cluster* adjusted = checkPointer(new Cluster[m_numberOfClusters + objectCount]);
    for (unsigned int i = 0; i < m_numberOfClusters; ++i)
    {
        adjusted[i].m_walkable = 1;
        adjusted[i].m_numberOfPolygons = m_clusters[i].m_numberOfPolygons;
    }
    unsigned int newCluster = m_numberOfClusters;
    for (ObjectPolygons::const_iterator object = objects.begin(); object != objects.end(); ++object)
    {
        const DynamicPolygonMap::const_iterator first = dynamicPolygons.find(object->second[0]);
        adjusted[newCluster].m_numberOfPolygons = static_cast<unsigned int>(object->second.size());
        adjusted[newCluster].m_walkable = first->second.second;
        std::list<unsigned int> empty;
        clusters.insert(std::make_pair(static_cast<unsigned short>(newCluster), empty));
        for (std::list<OptPolygon*>::const_iterator p = mesh.begin(); p != mesh.end(); ++p)
        {
            OptPolygon* polygon = *p;
            for (std::vector<unsigned int>::const_iterator id = object->second.begin(); id != object->second.end(); ++id)
            {
                if (*id != polygon->m_index) continue;
                --adjusted[polygon->m_cluster].m_numberOfPolygons;
                ClusterPolygonMap::iterator old = clusters.find(polygon->m_cluster);
                std::list<unsigned int>::iterator entry = old->second.begin();
                while (entry != old->second.end())
                {
                    if (*entry == *id)
                    {
                        std::list<unsigned int>::iterator removed = entry++;
                        old->second.erase(removed);
                        clusters.find(static_cast<unsigned short>(newCluster))->second.push_back(*id);
                    }
                    else ++entry;
                }
                polygon->m_cluster = static_cast<unsigned short>(newCluster);
            }
        }
        ++newCluster;
    }
    m_numberOfClusters += objectCount;
    unsigned int first = 0;
    for (unsigned int i = 0; i < m_numberOfClusters; ++i)
    {
        adjusted[i].m_firstPolygon = first;
        first += adjusted[i].m_numberOfPolygons;
    }
    delete[] m_clusters;
    m_clusters = adjusted;
}

// ELF 0x31AC20.
void ClusterGraph::fixClusterGraphConnectivity(std::list<OptPolygon*>& mesh, bool reportOnly)
{
    std::set<unsigned short> checked;
    if (m_numberOfClusters == 1 && m_clusters[0].m_numberOfPolygons == 1) return;
    for (;;)
    {
        bool fixed = true;
        for (std::list<OptPolygon*>::iterator p = mesh.begin(); p != mesh.end(); ++p)
        {
            OptPolygon* polygon = *p;
            if (checked.find(polygon->m_cluster) != checked.end()) continue;
            std::set<unsigned int> connected;
            if (validateClusterConnectivityFromPolygon(polygon->m_cluster, polygon,
                    m_clusters[polygon->m_cluster].m_numberOfPolygons, connected, true))
            {
                checked.insert(polygon->m_cluster);
                continue;
            }
            if (reportOnly)
            {
                Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp", 1294, "")
                    << "Non connected cluster " << static_cast<unsigned long>(polygon->m_cluster)
                    << ". Found " << static_cast<int>(connected.size())
                    << " polygons, should be " << static_cast<int>(m_clusters[polygon->m_cluster].m_numberOfPolygons)
                    << " polygons";
                return;
            }
            unsigned int unconnected = 0xFFFFFFFF;
            const Cluster& cluster = m_clusters[polygon->m_cluster];
            const unsigned int first = cluster.m_firstPolygon;
            const unsigned int count = cluster.m_numberOfPolygons;
            for (unsigned int j = 0; j < count; ++j)
                if (connected.find(m_clusterPolygons[first + j]) == connected.end())
                {
                    unconnected = m_clusterPolygons[first + j];
                    break;
                }
            OptPolygon* seed = 0;
            for (std::list<OptPolygon*>::iterator q = mesh.begin(); q != mesh.end(); ++q)
                if ((*q)->m_index == unconnected) { seed = *q; break; }
            std::set<unsigned int> selected;
            selected.insert(unconnected);
            for (unsigned int edge = 0; edge < seed->m_numberOfVertices; ++edge)
            {
                OptPolygon* neighbour = seed->m_neighbours[edge];
                if (neighbour && neighbour->m_cluster == seed->m_cluster)
                    recurseFloodFill(neighbour, seed, selected);
            }
            addPolygonsToNewCluster(selected, mesh);
            fixed = false;
            break;
        }
        if (fixed) return;
    }
}

// ELF 0x31B3F0.
void ClusterGraph::floodFillPolygonsInCluster(OptPolygon* polygon, std::set<OptPolygon*>& filled,
                                             bool filter) const
{
    if (filled.find(polygon) != filled.end()) return;
    // shipped defect: with filter true the material must simultaneously equal zero and four.
    if (filter && polygon->m_material != 0) return;
    if (polygon->m_material != 4) return;
    filled.insert(polygon);
    for (unsigned int edge = 0; edge < polygon->m_numberOfVertices; ++edge)
    {
        OptPolygon* neighbour = polygon->m_neighbours[edge];
        if (neighbour && neighbour->m_cluster == polygon->m_cluster)
            floodFillPolygonsInCluster(neighbour, filled, filter);
    }
}

// ELF 0x31B510.
void ClusterGraph::getAllDeepWaterAndInvalidPolygonsInCluster(unsigned short cluster,
    const std::list<OptPolygon*>& mesh, std::set<OptPolygon*>& polygons) const
{
    for (std::list<OptPolygon*>::const_iterator p = mesh.begin(); p != mesh.end(); ++p)
        if ((*p)->m_cluster == cluster && ((*p)->m_material == 0 || (*p)->m_material == 4))
            polygons.insert(*p);
}

// ELF 0x31B5B0.
void ClusterGraph::setPolygonIndices(const ClusterPolygonMap& clusters)
{
    m_numberOfClusterPolygons = 0;
    for (ClusterPolygonMap::const_iterator i = clusters.begin(); i != clusters.end(); ++i)
        m_numberOfClusterPolygons += static_cast<unsigned int>(i->second.size());
    unsigned int* polygons = checkPointer(new unsigned int[m_numberOfClusterPolygons]);
    unsigned int write = 0;
    for (ClusterPolygonMap::const_iterator i = clusters.begin(); i != clusters.end(); ++i)
        for (std::list<unsigned int>::const_iterator p = i->second.begin(); p != i->second.end(); ++p)
            polygons[write++] = *p;
    delete[] m_clusterPolygons;
    m_clusterPolygons = polygons;
}

// ELF 0x31BC80.
void ClusterGraph::recurseFloodFill(OptPolygon* polygon, OptPolygon* origin,
                                   std::set<unsigned int>& filled) const
{
    if (polygon == origin) return;
    if (filled.find(polygon->m_index) != filled.end()) return;
    filled.insert(polygon->m_index);
    for (unsigned int edge = 0; edge < polygon->m_numberOfVertices; ++edge)
    {
        OptPolygon* neighbour = polygon->m_neighbours[edge];
        if (neighbour && neighbour->m_cluster == origin->m_cluster)
            recurseFloodFill(neighbour, origin, filled);
    }
}

// ELF 0x31BD70. The cluster argument is unused.
bool ClusterGraph::validateClusterConnectivityFromPolygon(unsigned short, OptPolygon* polygon,
    unsigned int count, std::set<unsigned int>& filled, bool all) const
{
    if (all) filled.insert(polygon->m_index);
    for (unsigned int edge = 0; edge < polygon->m_numberOfVertices; ++edge)
    {
        OptPolygon* neighbour = polygon->m_neighbours[edge];
        if (neighbour && neighbour->m_cluster == polygon->m_cluster)
        {
            recurseFloodFill(neighbour, polygon, filled);
            if (!all) break;
        }
    }
    return filled.size() == count;
}

// ELF 0x313E70 (C2; C1 0x314010). Zeroes the six fields at +0x00..+0x14 and the three at +0x24..+0x2C; the two sets
// construct empty (their sentinel allocations are the only calls). is bigger only because the
// SGI node allocator is inlined into it - same source, two inlining decisions.
ClusterGraph::ClusterGraph()
    : m_clusterPolygons(0)
    , m_numberOfClusterPolygons(0)
    , m_clusters(0)
    , m_numberOfClusters(0)
    , m_numberOfPortals(0)
    , m_portals(0)
    , m_tempMemberSizes(0)
    , m_numberOfPortalCosts(0)
    , m_portalCosts(0)
{
}

// ELF 0x3140F0 (D2; D1 0x3141D0). clean, then the two sets in reverse declaration order.
ClusterGraph::~ClusterGraph()
{
    clean();
}

// ELF 0x3142B0. Releases arrays and clears counts, but leaves four freed pointers unchanged.
// Do not clean twice without repopulating those pointers; readFromFile assigns them
// immediately after cleanup. Only the pointer at +0x24 is cleared.
void ClusterGraph::clean()
{
    delete[] m_clusterPolygons;
    m_numberOfClusterPolygons = 0;

    delete[] m_clusters;
    m_numberOfClusters = 0;

    delete[] m_portals;
    m_numberOfPortals = 0;

    delete[] m_tempMemberSizes;
    m_tempMemberSizes = 0;

    delete[] m_portalCosts;
    m_numberOfPortalCosts = 0;
}

// ELF 0x314830. Loads a version-5 cluster graph. Unsupported versions leave the existing graph unchanged.
void ClusterGraph::readFromFile(io::File& file)
{
    int version = 0;
    file.read(version);

    if (version == 5)
    {
        clean();

        file.read(m_numberOfClusters);
        m_clusters = checkPointer(new Cluster[m_numberOfClusters]);
        file.read(m_clusters, sizeof(Cluster) * m_numberOfClusters);

        file.read(m_numberOfClusterPolygons);
        m_clusterPolygons = checkPointer(new unsigned int[m_numberOfClusterPolygons]);
        file.read(m_clusterPolygons, sizeof(unsigned int) * m_numberOfClusterPolygons);

        file.read(m_numberOfPortals);
        if (m_numberOfPortals)
        {
            m_portals = checkPointer(new ClusterPortal[m_numberOfPortals]);
            file.read(m_portals, sizeof(ClusterPortal) * m_numberOfPortals);
        }
        else
        {
            m_portals = 0;
        }

        file.read(m_numberOfPortalCosts);
        if (m_numberOfPortalCosts)
        {
            m_portalCosts = checkPointer(new float[m_numberOfPortalCosts]);
            file.read(m_portalCosts, sizeof(float) * m_numberOfPortalCosts);
        }
        else
        {
            m_portalCosts = 0;
        }
    }
}

// ELF 0x314E10. An out-of-range polygon dereferences null, as in retail.
unsigned short ClusterGraph::getClusterId(QuadTree* quadTree, unsigned int polygon) const
{
    return quadTree->getPolygon(polygon)->m_cluster;
}

// ELF 0x31DA70. True when one of the cluster's portals leads to `neighbour`.
bool ClusterGraph::areNeighbours(unsigned short cluster, unsigned short neighbour) const
{
    const Cluster& record = m_clusters[cluster];
    for (unsigned short i = 0; i < record.m_numberOfPortals; ++i)
    {
        if (m_portals[record.m_firstPortal + i].m_neighbourCluster == neighbour)
            return true;
    }
    return false;
}

// ELF 0x314E50.
ClusterPortal* ClusterGraph::getClusterPortal(unsigned short cluster, unsigned short costType,
                                              unsigned short index) const
{
    const Cluster& record = m_clusters[cluster];
    return &m_portals[record.m_firstPortal + costType * record.m_numberOfPortals + index];
}

// ELF 0x314EA0. The index of the cluster's first portal into `neighbour`, or 0xFFFF. The
// cost-type offset plus index is truncated to 16 bits before the base is added, as in retail.
unsigned short ClusterGraph::getPortalForNeighbour(unsigned short cluster,
                                                   unsigned short costType,
                                                   unsigned short neighbour) const
{
    if (cluster == 0xFFFF)
        return 0xFFFF;
    const Cluster& record = m_clusters[cluster];
    for (unsigned short i = 0; i < record.m_numberOfPortals; ++i)
    {
        const unsigned short offset =
            static_cast<unsigned short>(costType * record.m_numberOfPortals + i);
        if (m_portals[record.m_firstPortal + offset].m_neighbourCluster == neighbour)
            return i;
    }
    return 0xFFFF;
}

// ELF 0x314F30. The neighbour cluster's index of the same portal: the same polygon key and a
// position within 0.1 (squared distance under 0.01).
unsigned short ClusterGraph::getCorrespongingPortalInNeighbour(ClusterPortal* portal,
                                                               unsigned short costType) const
{
    const unsigned short neighbour = portal->m_neighbourCluster;
    if (neighbour == 0xFFFF)
        return 0xFFFF;
    const Cluster& record = m_clusters[neighbour];
    for (unsigned short i = 0; i < record.m_numberOfPortals; ++i)
    {
        const ClusterPortal& candidate =
            m_portals[record.m_firstPortal + costType * record.m_numberOfPortals + i];
        if (candidate.m_polygon != portal->m_polygon)
            continue;
        const float dx = candidate.m_position.x - portal->m_position.x;
        const float dy = candidate.m_position.y - portal->m_position.y;
        const float dz = candidate.m_position.z - portal->m_position.z;
        if (0.01f > dx * dx + dy * dy + dz * dz)
            return i;
    }
    return 0xFFFF;
}

// ELF 0x31CEE0. Portal-to-portal costs are a triangular table per cluster and cost type: the pair
// (lo, hi) is entry T(n) - T(n - lo) + (hi - lo) after the cluster's base, T(k) = k(k - 1) / 2,
// read one before that position. Every intermediate is truncated to 16 bits as in retail.
float ClusterGraph::getCostBetweenClustersPortals(unsigned short cluster, unsigned short first,
                                                  unsigned short second,
                                                  unsigned short costType) const
{
    const Cluster& record = m_clusters[cluster];
    const unsigned short lo = first < second ? first : second;
    const unsigned short hi = first < second ? second : first;
    const int count = record.m_numberOfPortals;
    const unsigned short pairs = static_cast<unsigned short>(count * (count - 1) / 2);
    const int remaining = static_cast<unsigned short>(count - lo);
    const unsigned short tail = static_cast<unsigned short>(remaining * (remaining - 1) / 2);
    const unsigned short within = static_cast<unsigned short>(
        pairs - tail + static_cast<unsigned short>(hi - lo));
    const int index = record.m_firstPortalCost + pairs * costType + within;
    return m_portalCosts[index - 1];
}

// ELF 0x315050. Appends the cluster's portals of one cost type; an unknown cluster adds none.
void ClusterGraph::getClusterPortals(unsigned short cluster, unsigned short costType,
                                     std::list<ClusterPortal*>& portals) const
{
    if (cluster >= m_numberOfClusters)
        return;
    const Cluster& record = m_clusters[cluster];
    for (unsigned short i = 0; i < record.m_numberOfPortals; ++i)
        portals.push_back(
            &m_portals[record.m_firstPortal + costType * record.m_numberOfPortals + i]);
}

// ELF 0x31CF90. Like getPortalForNeighbour, without the 0xFFFF guard or the 16-bit truncation.
unsigned short ClusterGraph::findClusterPortalWithNeighbour(unsigned short cluster,
                                                            unsigned short costType,
                                                            unsigned short neighbour) const
{
    const Cluster& record = m_clusters[cluster];
    for (unsigned short i = 0; i < record.m_numberOfPortals; ++i)
    {
        if (m_portals[record.m_firstPortal + costType * record.m_numberOfPortals + i]
                .m_neighbourCluster == neighbour)
            return i;
    }
    return 0xFFFF;
}

// ELF 0x31CE30. Mirrors getCostBetweenClustersPortals: the smaller portal index first, the
// same 16-bit truncations, and the store at index - 1.
void ClusterGraph::setCostBetweenClustersPortals(unsigned short cluster, unsigned short first,
                                                 unsigned short second, unsigned short costType,
                                                 float cost)
{
    const Cluster& record = m_clusters[cluster];
    const unsigned short lo = first < second ? first : second;
    const unsigned short hi = first < second ? second : first;
    const int count = record.m_numberOfPortals;
    const unsigned short pairs = static_cast<unsigned short>(count * (count - 1) / 2);
    const int remaining = static_cast<unsigned short>(count - lo);
    const unsigned short tail = static_cast<unsigned short>(remaining * (remaining - 1) / 2);
    const unsigned short within = static_cast<unsigned short>(
        pairs - tail + static_cast<unsigned short>(hi - lo));
    const int index = record.m_firstPortalCost + pairs * costType + within;
    m_portalCosts[index - 1] = cost;
}

// ELF 0x31CA80. One step of the incremental portal-cost update of `cluster`: `state` is
// (first portal, second portal, cost type), starting at (0, 1, 0) when the caller has no search
// resource yet (one is acquired for owner 1234; none free -> false). The step restricts the
// search to the cluster and both portals' neighbour clusters (m_set30) and asks
// AIPathfinding::getPortalCost; while that search runs the answer is false. A finished pair is
// stored, the resource is released and re-acquired, and the state advances: next cost type,
// then next second portal (cost type 0), then next first portal (second = first + 1). True once
// the first portal passes the last one. Retail also constructs and frees an unused local
// std::vector (4-byte elements; -0x3C..-0x34) and has an unused static `clusterUpdate`.
bool ClusterGraph::updatePortalCosts(unsigned int& resource,
                                     triple<unsigned short, unsigned short, unsigned short>& state,
                                     int map, unsigned int island, unsigned short cluster)
{
    AIPathfinding* const pathfinding = static_cast<AIPathfinding*>(IAIPathfinding::instance);
    const Cluster& record = m_clusters[cluster];
    const unsigned short numberOfPortals = record.m_numberOfPortals;
    const unsigned int firstPortal = record.m_firstPortal;

    if (resource == 0xFFFFFFFFu)
    {
        state.first = 0;
        state.second = 1;
        state.third = 0;
        pathfinding->acquireResource(resource, 1234);
        if (resource == 0xFFFFFFFFu)
            return false;
    }

    const ClusterPortal* const from = &m_portals[state.first + firstPortal];
    const ClusterPortal* const to = &m_portals[state.second + firstPortal];
    m_set30.clear();
    m_set30.insert(cluster);
    m_set30.insert(from->m_neighbourCluster);
    m_set30.insert(to->m_neighbourCluster);

    float cost;
    if (!pathfinding->getPortalCost(cost, resource, map, island, state.third, from->m_position,
                                    from->m_polygon, to->m_position, to->m_polygon, m_set30))
        return false;

    pathfinding->releaseResource(resource, 1234);
    setCostBetweenClustersPortals(cluster, state.first, state.second, state.third, cost);
    ++state.third;
    if (!pathfinding->isValidClusterType(map, state.third))
    {
        state.third = 0;
        ++state.second;
        if (state.second >= numberOfPortals)
        {
            const unsigned short previous = state.first;
            ++state.first;
            if (state.first >= numberOfPortals)
                return true;
            state.second = static_cast<unsigned short>(previous + 2);
        }
    }
    pathfinding->acquireResource(resource, 1234);
    return false;
}

// ELF 0x314740. Writes the version-5 record readFromFile reads; empty portal and cost arrays are
// written as their zero count alone.
void ClusterGraph::saveToFile(io::File& file) const
{
    file.write(5);
    file.write(m_numberOfClusters);
    file.write(m_clusters, sizeof(Cluster) * m_numberOfClusters);
    file.write(m_numberOfClusterPolygons);
    file.write(m_clusterPolygons, sizeof(unsigned int) * m_numberOfClusterPolygons);
    file.write(m_numberOfPortals);
    if (m_numberOfPortals)
        file.write(m_portals, sizeof(ClusterPortal) * m_numberOfPortals);
    file.write(m_numberOfPortalCosts);
    if (m_numberOfPortalCosts)
        file.write(m_portalCosts, sizeof(float) * m_numberOfPortalCosts);
}

// ELF 0x31C2E0. Rebuilds the whole portal-to-portal cost table: for every cluster (its base is
// the running table size) and every valid cluster cost type, one blocking search per portal pair
// (i < j), restricted to the cluster and both portals' neighbour clusters. The old table is freed
// first; its count is only rewritten at the end. Retail also keeps a per-cluster
// std::list<std::list<float> > whose iterator is stepped once per appended cost and never read,
// and a final pass that recomputes each pair's distance and stored cost without using either.
void ClusterGraph::calculatePortalCosts(int map, unsigned int island)
{
    AIPathfinding* const pathfinding = static_cast<AIPathfinding*>(IAIPathfinding::instance);
    delete[] m_portalCosts;

    std::vector<float> portalCosts;
    std::vector<float> costs;
    for (unsigned int cluster = 0; cluster < m_numberOfClusters; ++cluster)
    {
        std::list<std::list<float> > unusedCosts;
        Cluster& record = m_clusters[cluster];
        const unsigned short count = record.m_numberOfPortals;
        record.m_firstPortalCost = static_cast<unsigned short>(portalCosts.size());
        unsigned int first = record.m_firstPortal;
        for (unsigned int costType = 0; pathfinding->isValidClusterType(map, costType);
             ++costType, first += count)
        {
            for (unsigned short i = 0; i < count; ++i)
            {
                const ClusterPortal* const from = &m_portals[first + i];
                for (unsigned short j = static_cast<unsigned short>(i + 1); j < count; ++j)
                {
                    const ClusterPortal* const to = &m_portals[first + j];
                    std::set<unsigned short> allowedClusters;
                    allowedClusters.insert(static_cast<unsigned short>(cluster));
                    allowedClusters.insert(from->m_neighbourCluster);
                    allowedClusters.insert(to->m_neighbourCluster);
                    costs.resize(0);
                    pathfinding->getPortalCost(costs, static_cast<unsigned short>(costType), map,
                                               island, to->m_position, to->m_polygon,
                                               from->m_position, from->m_polygon,
                                               allowedClusters);
                    std::list<std::list<float> >::iterator unused = unusedCosts.begin();
                    for (std::vector<float>::iterator cost = costs.begin(); cost != costs.end();
                         ++cost, ++unused)
                        portalCosts.push_back(*cost);
                }
            }
        }
    }

    m_portalCosts = new float[portalCosts.size()];
    m_numberOfPortalCosts = portalCosts.size();
    for (unsigned int i = 0; i < portalCosts.size(); ++i)
        m_portalCosts[i] = portalCosts[i];

    for (unsigned int cluster = 0; cluster < m_numberOfClusters; ++cluster)
    {
        const Cluster& record = m_clusters[cluster];
        const unsigned short count = record.m_numberOfPortals;
        const unsigned int first = record.m_firstPortal;
        for (unsigned short i = 0; i < count; ++i)
        {
            for (unsigned short j = static_cast<unsigned short>(i + 1); j < count; ++j)
            {
                for (unsigned int costType = 0; pathfinding->isValidClusterType(map, costType);
                     ++costType)
                {
                    const Vec3 a = m_portals[first + i + costType * count].m_position;
                    const Vec3 b = m_portals[first + j + costType * count].m_position;
                    const float dx = a.x - b.x;
                    const float dy = a.y - b.y;
                    const float dz = a.z - b.z;
                    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    const float cost = getCostBetweenClustersPortals(
                        static_cast<unsigned short>(cluster), i, j,
                        static_cast<unsigned short>(costType));
                    (void)distance;
                    (void)cost;
                }
            }
        }
    }
}

// ELF 0x31B7F0. Snapshots every cluster's polygon count into a new array (the previous one is
// not freed).
void ClusterGraph::resetTempMemberSizes()
{
    m_tempMemberSizes = checkPointer(new unsigned int[m_numberOfClusters]);
    for (unsigned int cluster = 0; cluster < m_numberOfClusters; ++cluster)
        m_tempMemberSizes[cluster] = m_clusters[cluster].m_numberOfPolygons;
}

// ELF 0x31B970. False as soon as the cluster's polygons border more than one other (assigned)
// cluster.
bool ClusterGraph::allNeighboursOfClusterInSameCluster(QuadTree* quadTree,
                                                       unsigned short cluster) const
{
    const Cluster& record = m_clusters[cluster];
    const unsigned int first = record.m_firstPolygon;
    const unsigned int count = record.m_numberOfPolygons;
    std::set<unsigned short> neighbours;
    for (unsigned int i = 0; i < count; ++i)
    {
        const unsigned int polygon = m_clusterPolygons[first + i];
        const Polygon* const candidate = quadTree->getPolygon(polygon);
        for (unsigned char edge = 0; edge < candidate->m_numberOfVertices; ++edge)
        {
            const Polygon* const neighbour
                = quadTree->getPolygon(quadTree->getNeighbourOfPolygon(polygon, edge));
            if (neighbour && neighbour->m_cluster != cluster && neighbour->m_cluster != 0xFFFF)
            {
                neighbours.insert(neighbour->m_cluster);
                if (neighbours.size() > 1)
                    return false;
            }
        }
    }
    return true;
}

// ELF 0x31BB90. A cluster of more than one polygon holds when every polygon has a neighbour in
// the same cluster.
bool ClusterGraph::checkClusterCondition(QuadTree* quadTree, unsigned short cluster) const
{
    const Cluster& record = m_clusters[cluster];
    const unsigned int first = record.m_firstPolygon;
    const unsigned int count = record.m_numberOfPolygons;
    if (count == 1)
        return true;

    for (unsigned int i = 0; i < count; ++i)
    {
        const unsigned int polygon = m_clusterPolygons[first + i];
        const Polygon* const candidate = quadTree->getPolygon(polygon);
        unsigned char edge = 0;
        for (; edge < candidate->m_numberOfVertices; ++edge)
        {
            const Polygon* const neighbour
                = quadTree->getPolygon(quadTree->getNeighbourOfPolygon(polygon, edge));
            if (neighbour && neighbour->m_cluster == cluster)
                break;
        }
        if (edge >= candidate->m_numberOfVertices)
            return false;
    }
    return true;
}

// ELF 0x31BE50. Reports (printClusterInfo, then an error) every cluster whose member list holds a
// polygon assigned elsewhere; always true.
bool ClusterGraph::validateClusterGraphConnectivity(QuadTree* quadTree, bool verbose)
{
    for (unsigned short cluster = 0; cluster < m_numberOfClusters; ++cluster)
    {
        const Cluster& record = m_clusters[cluster];
        const unsigned int first = record.m_firstPolygon;
        const unsigned int count = record.m_numberOfPolygons;
        unsigned int members = 0;
        for (unsigned int i = 0; i < count; ++i)
        {
            if (quadTree->getPolygon(m_clusterPolygons[first + i])->m_cluster == cluster)
                ++members;
        }
        if (members != count)
        {
            printClusterInfo(cluster, quadTree, verbose);
            Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp", 1578, "")
                << "Bailing out in validateClusterGraphConnectivity()!";
        }
    }
    return true;
}

// ELF 0x31C080. Reports the cluster id; the walk over its polygons reads each corner index and
// neighbour without printing them.
void ClusterGraph::printClusterInfo(unsigned short cluster, QuadTree* quadTree, bool) const
{
    if (cluster >= m_numberOfClusters)
        return;

    const Cluster& record = m_clusters[cluster];
    const unsigned int first = record.m_firstPolygon;
    const unsigned int count = record.m_numberOfPolygons;
    // operator<<(int): the listing carries its inlined oct/hex test.
    Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterGraph.cpp", 1596, "")
        << "Cluster info for cluster " << static_cast<int>(cluster);

    for (unsigned int i = 0; i < count; ++i)
    {
        const unsigned int polygon = m_clusterPolygons[first + i];
        const Polygon* const candidate = quadTree->getPolygon(polygon);
        for (unsigned char edge = 0; edge < candidate->m_numberOfVertices; ++edge)
        {
            quadTree->getVertexIndexForPolygon(polygon, edge);
            quadTree->getPolygon(quadTree->getNeighbourOfPolygon(polygon, edge));
        }
    }
}

// ELF 0x31D000. Debug view of one cluster and cost type (30 s): its id above the first portal,
// a red circle per portal, and per portal pair a line and the stored cost at its midpoint. The
// first portal is read even for a cluster without portals.
void ClusterGraph::drawCluster(unsigned short cluster, int costType) const
{
    Vec3 up;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    const Cluster& record = m_clusters[cluster];
    IDPStaticCreator* const creator
        = static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator);
    const unsigned short count = record.m_numberOfPortals;
    const unsigned int first = record.m_firstPortal + costType * count;
    const ClusterPortal* const firstPortal = &m_portals[first];

    {
        std::stringstream text;
        text << cluster;
        Vec3 position;
        position.x = up.x + firstPortal->m_position.x;
        position.y = up.y + firstPortal->m_position.y;
        position.z = up.z + firstPortal->m_position.z;
        creator->createStatic3dText(30.0f, 0x5588u, position, text.str(), 0,
                                    static_cast<AITextAlignmentX>(0),
                                    static_cast<AITextAlignmentY>(0));
    }

    for (unsigned short i = 0; i < count; ++i)
    {
        const ClusterPortal* const portal = &m_portals[first + i];
        creator->createStaticCircle(30.0f, 0xFFFF0000u, portal->m_position, 0.5f, up, false, 10);
        for (unsigned short j = static_cast<unsigned short>(i + 1); j < count; ++j)
        {
            const ClusterPortal* const other = &m_portals[first + j];
            creator->createStaticLine(30.0f, 0xAAAA0000u, portal->m_position, other->m_position,
                                      0.25f);
            Vec3 middle;
            middle.x = (portal->m_position.x + other->m_position.x) * 0.5f;
            middle.y = (portal->m_position.y + other->m_position.y) * 0.5f;
            middle.z = (portal->m_position.z + other->m_position.z) * 0.5f;
            std::stringstream text;
            text << getCostBetweenClustersPortals(cluster, i, j,
                                                  static_cast<unsigned short>(costType));
            creator->createStatic3dText(30.0f, 0x88u, middle, text.str(), 0,
                                        static_cast<AITextAlignmentX>(0),
                                        static_cast<AITextAlignmentY>(0));
        }
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
