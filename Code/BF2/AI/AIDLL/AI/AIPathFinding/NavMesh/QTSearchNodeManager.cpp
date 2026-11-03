// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining QTSearchNodeManager operations.

#include "QTSearchNodeManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x300590, __tcf_0 0x3005F0, _GLOBAL__I 0x300620).
#include <iostream>

#include <cmath>

#include "QuadTree.h"
#include "../Material/MaterialManager.h"
#include "../../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF 0x2FEDD0. False when the node is its own m_expandedFrom ancestor within ten generations.
// Nothing in the image calls it.
bool checkNodeHeritage(QTSearchNode* node)
{
    int generation = 0;
    for (QTSearchNode* ancestor = node->m_expandedFrom; ancestor != 0 && generation < 10;
         ancestor = ancestor->m_expandedFrom, ++generation)
    {
        if (ancestor == node)
            return false;
    }
    return true;
}

} // namespace

// QTSearchNodeManager

// ELF 0x2FEE00 (C2; C1 0x2FEEC0) and 0x2FEF80 (C2; C1 0x2FF040). Constructors share
// initialization except for the optional quad-tree pointer.
// Keep both overloads; observed stores do not establish member declaration order.
QTSearchNodeManager::QTSearchNodeManager()
    : m_quadTree(0)
    , m_goal(0)
    , m_nodeCache(0)
    , m_numberOfNodes(0)
    , m_freeEntries(0)
    , m_usedBuckets(0)
    , m_table(new CacheEntry[kCacheEntries])
    , m_additionalGoals(0)
    , m_allowedClusters(0)
    , m_usePathTopologySmoothing(false)
    , m_materials(0)
{
}

QTSearchNodeManager::QTSearchNodeManager(QuadTree* quadTree)
    : m_quadTree(quadTree)
    , m_goal(0)
    , m_nodeCache(0)
    , m_numberOfNodes(0)
    , m_freeEntries(0)
    , m_usedBuckets(0)
    , m_table(new CacheEntry[kCacheEntries])
    , m_additionalGoals(0)
    , m_allowedClusters(0)
    , m_usePathTopologySmoothing(false)
    , m_materials(0)
{
}

// ELF 0x2FF100 (D2; D1 0x2FF290). Frees the node pool, every bucket's status records and the nodes they hold, the
// recycled records, the table and the goal copy.
QTSearchNodeManager::~QTSearchNodeManager()
{
    while (m_nodeCache != 0)
    {
        QTSearchNode* const node = m_nodeCache;
        m_nodeCache = node->m_next;
        delete node;
        --m_numberOfNodes;
    }

    for (unsigned int i = 0; i < kCacheEntries; ++i)
    {
        StatusEntry* entry = m_table[i].m_head;
        while (entry != 0)
        {
            StatusEntry* const next = entry->m_next;
            if (entry->m_node != 0)
                delete entry->m_node;
            delete entry;
            entry = next;
        }
    }

    while (m_freeEntries != 0)
    {
        StatusEntry* const entry = m_freeEntries;
        m_freeEntries = entry->m_next;
        delete entry;
    }

    if (m_table != 0)
        delete[] m_table;

    if (m_goal != 0)
    {
        delete m_goal;
        --m_numberOfNodes;
    }
}

// ELF 0x2FF420.
void QTSearchNodeManager::setQuadTree(QuadTree* quadTree)
{
    m_quadTree = quadTree;
}

// ELF 0x2FF430. Keeps a private copy of the goal's polygon and position and recycles `node`.
void QTSearchNodeManager::setGoal(QTSearchNode* node)
{
    if (m_goal == 0)
        m_goal = getNewNode(node);
    else
        m_goal->m_key = node->m_key;
    m_goal->m_position = node->m_position;
    recycleNode(node);
}

// ELF 0x2FF4C0. Finds or creates the polygon's status record and marks it open. A recycled record
// keeps its old closed flag; a new one starts not closed.
void QTSearchNodeManager::setNodeOpen(QTSearchNode* node)
{
    const int key = static_cast<int>(node->m_key);
    int index = key % kCacheEntries;
    if (key < 0)
        index += kCacheEntries;
    CacheEntry& bucket = m_table[index];

    for (StatusEntry* entry = bucket.m_head; entry != 0; entry = entry->m_next)
    {
        if (entry->m_node->m_key == node->m_key)
        {
            entry->m_open = true;
            return;
        }
    }

    StatusEntry* entry = m_freeEntries;
    if (entry != 0)
    {
        m_freeEntries = entry->m_next;
        entry->m_open = true;
        entry->m_node = node;
    }
    else
    {
        entry = new StatusEntry;
        entry->m_open = true;
        entry->m_closed = false;
        entry->m_node = node;
    }

    entry->m_next = bucket.m_head;
    if (bucket.m_head == 0)
    {
        bucket.m_nextUsed = m_usedBuckets;
        m_usedBuckets = &bucket;
    }
    bucket.m_head = entry;
}

// ELF 0x2FF5A0. The record must exist: retail writes through the search result unchecked.
void QTSearchNodeManager::setNodeClosed(QTSearchNode* node)
{
    const int key = static_cast<int>(node->m_key);
    int index = key % kCacheEntries;
    if (key < 0)
        index += kCacheEntries;

    StatusEntry* entry = m_table[index].m_head;
    while (entry != 0 && entry->m_node->m_key != node->m_key)
        entry = entry->m_next;
    entry->m_open = false;
    entry->m_closed = true;
}

// ELF 0x2FF610. The neighbour across the first bordered edge at or after `edge` (which is left on
// that edge), as a new node one step deeper, with what is known about it in `status`. Impassable
// material, a cluster outside the allowed set and an exhausted polygon report kInvalid; the last
// returns null, the others the node. Unseen neighbours are costed: the entry point is the crossed
// edge's midpoint and, for the goal or an additional goal, f is the exact remaining cost.
QTSearchNode* QTSearchNodeManager::getNeighbourOfNode(QTSearchNode* node, unsigned short& edge,
                                                      nodeStatus::Status& status)
{
    unsigned int neighbourPolygon =
        m_quadTree->getNeighbourOfPolygon(node->m_key, static_cast<unsigned char>(edge));
    while (neighbourPolygon == 0xFFFFFFFFu)
    {
        ++edge;
        if (edge >= m_quadTree->getPolygon(node->m_key)->m_numberOfVertices)
        {
            status = nodeStatus::kInvalid;
            return 0;
        }
        neighbourPolygon =
            m_quadTree->getNeighbourOfPolygon(node->m_key, static_cast<unsigned char>(edge));
    }

    QTSearchNode* const neighbour = getNewNode(neighbourPolygon, node, node->m_depth + 1);

    const int key = static_cast<int>(neighbour->m_key);
    int index = key % kCacheEntries;
    if (key < 0)
        index += kCacheEntries;
    StatusEntry* entry = m_table[index].m_head;
    while (entry != 0 && entry->m_node->m_key != neighbour->m_key)
        entry = entry->m_next;

    const Polygon* const polygon = m_quadTree->getPolygon(neighbourPolygon);
    if (0.0f > m_materials->getMaterialCost(polygon->m_material))
    {
        status = nodeStatus::kInvalid;
        return neighbour;
    }

    if (m_allowedClusters != 0 && m_allowedClusters->find(polygon->m_cluster) == m_allowedClusters->end())
    {
        status = nodeStatus::kInvalid;
        return neighbour;
    }

    if (entry != 0)
    {
        if (entry->m_open)
        {
            status = nodeStatus::kOpen;
            return neighbour;
        }
        if (entry->m_closed)
        {
            status = nodeStatus::kClosed;
            return neighbour;
        }
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/QTSearchNodeManager.cpp", 289, "")
            << "This case should not exists!";
        status = nodeStatus::kInvalid;
        return neighbour;
    }

    Vec3 entryPoint;
    entryPoint.x = 0.0f;
    entryPoint.y = 0.0f;
    entryPoint.z = 0.0f;
    calculateEntryPoint(entryPoint, edge, node);
    neighbour->m_position = entryPoint;
    neighbour->m_g = calculateCost(node, neighbour, neighbour->m_position);

    if (neighbour->m_key == m_goal->m_key ||
        (m_additionalGoals != 0 &&
         m_additionalGoals->find(neighbourPolygon) != m_additionalGoals->end()))
    {
        neighbour->m_f = calculateCostBetweenPolygons(node->m_key, entryPoint, neighbourPolygon,
                                                      m_goal->m_position) +
                         neighbour->m_g;
        status = nodeStatus::kGoal;
        return neighbour;
    }

    neighbour->m_f = calculateHeuristicCost(neighbour->m_g, entryPoint);
    status = nodeStatus::kUnvisited;
    return neighbour;
}

// ELF 0x2FFA70. A recycled node keeps its costs, position and links except the two parents.
QTSearchNode* QTSearchNodeManager::getNewNode(unsigned int polygon, QTSearchNode* parent,
                                              unsigned int depth)
{
    QTSearchNode* node = m_nodeCache;
    if (node != 0)
    {
        node->m_key = polygon;
        node->m_depth = static_cast<int>(depth);
        m_nodeCache = m_nodeCache->m_next;
    }
    else
    {
        node = new QTSearchNode(polygon, depth);
        ++m_numberOfNodes;
    }
    node->m_parent = parent;
    node->m_expandedFrom = parent;
    return node;
}

// ELF 0x2FFAF0. A recycled node is assigned every field of `source`; a new one is copy
// constructed, which keeps only the key and the position.
QTSearchNode* QTSearchNodeManager::getNewNode(QTSearchNode* source)
{
    QTSearchNode* node = m_nodeCache;
    if (node != 0)
    {
        m_nodeCache = node->m_next;
        *node = *source;
    }
    else
    {
        node = new QTSearchNode(*source);
        ++m_numberOfNodes;
    }
    return node;
}

// ELF 0x2FFBA0. Returns every status record's node to the pool and the records to their free list,
// one bucket chain at a time, then the goal copy, and drops the allowed-cluster set.
void QTSearchNodeManager::reset()
{
    while (m_usedBuckets != 0)
    {
        StatusEntry* const first = m_usedBuckets->m_head;
        StatusEntry* entry = first;
        while (entry->m_next != 0)
        {
            recycleNode(entry->m_node);
            entry = entry->m_next;
        }
        recycleNode(entry->m_node);
        entry->m_next = m_freeEntries;
        m_freeEntries = first;

        m_usedBuckets->m_head = 0;
        m_usedBuckets = m_usedBuckets->m_nextUsed;
    }

    if (m_goal != 0)
    {
        recycleNode(m_goal);
        m_goal = 0;
    }
    m_allowedClusters = 0;
}

// ELF 0x2FFC20.
void QTSearchNodeManager::returnReset()
{
    m_usePathTopologySmoothing = false;
    m_additionalGoals = 0;
}

// ELF 0x2FFC40. Re-costs `existing` as reached from `parent` (through the edge between them). The
// route is taken only when it is cheaper by more than half a unit; it then becomes the node's
// parent chain. Refused at the depth limit, for the parent's own predecessor and for a node
// without a parent (the start).
bool QTSearchNodeManager::updateNode(QTSearchNode* parent, QTSearchNode* existing,
                                     unsigned int maxDepth)
{
    const int depth = parent->m_depth + 1;
    if (maxDepth == static_cast<unsigned int>(depth) || parent->m_expandedFrom == existing ||
        existing->m_parent == 0)
        return false;

    Vec3 entryPoint;
    entryPoint.x = 0.0f;
    entryPoint.y = 0.0f;
    entryPoint.z = 0.0f;
    unsigned char edge;
    m_quadTree->getNeighbourIndexForPolygon(edge, parent->m_key, existing->m_key);

    QTSearchNode* from = parent;
    if (!checkIfNodeHasAdequateSize(existing, edge))
    {
        from = parent->m_parent;
        if (from == 0)
            return false;
    }

    calculateEntryPoint(entryPoint, edge, parent);
    const float g = calculateCost(from, existing, entryPoint);
    const float f = calculateHeuristicCost(g, entryPoint);
    if (f + 0.5f >= existing->m_f)
        return false;

    existing->m_expandedFrom = parent;
    existing->m_position = entryPoint;
    existing->m_f = f;
    existing->m_g = g;
    existing->m_parent = from;
    existing->m_depth = depth;
    return true;
}

// ELF 0x2FFDA0. Accumulated cost: the step cost plus `from`'s g when the step cost is positive
// (an impassable -1 is returned as is). No predecessor costs nothing.
float QTSearchNodeManager::calculateCost(QTSearchNode* from, QTSearchNode* to,
                                         const Vec3& entry) const
{
    if (from == 0)
        return 0.0f;
    float cost = calculateCostBetweenPolygons(from->m_key, from->m_position, to->m_key, entry);
    if (cost > 0.0f)
        cost += from->m_g;
    return cost;
}

// ELF 0x2FFE20. The straight distance weighted by the material cost of the polygon left; -1 for
// impassable material. With topology smoothing, steps of at least 0.25 also weigh the slope of the
// polygon left and the change of normal between the two polygons.
float QTSearchNodeManager::calculateCostBetweenPolygons(unsigned int fromPolygon,
                                                        const Vec3& from,
                                                        unsigned int toPolygon,
                                                        const Vec3& to) const
{
    float cost = m_materials->getMaterialCost(m_quadTree->getMaterialOfPolygon(fromPolygon));
    if (cost < 0.0f)
        return -1.0f;

    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!m_usePathTopologySmoothing)
        return distance * cost;

    float extra = 0.0f;
    if (distance >= 0.25f)
    {
        const Vec3* const fromNormal = m_quadTree->getNormalOfPolygon(fromPolygon);
        const Vec3* const toNormal = m_quadTree->getNormalOfPolygon(toPolygon);
        const float turn =
            toNormal->x * fromNormal->x + toNormal->y * fromNormal->y + toNormal->z * fromNormal->z;
        const float upright = m_quadTree->getNormalOfPolygon(fromPolygon)->y;

        float slope = upright * 5.0f - 4.0f;
        if (!(slope > 0.0f))
            slope = 0.0f;
        float bend = turn * 5.0f - 4.0f;
        if (!(bend > 0.0f))
            bend = 0.0f;
        const float slopeFactor = 4.0f - slope * slope * 3.0f;
        const float bendTerm = 2.0f - bend * bend;
        extra = bendTerm * bendTerm;
        cost = slopeFactor * slopeFactor * cost;
    }
    return cost * distance + extra;
}

// ELF 0x300060. g plus the straight distance to the goal's position.
float QTSearchNodeManager::calculateHeuristicCost(float cost, const Vec3& position) const
{
    const float dx = m_goal->m_position.x - position.x;
    const float dy = m_goal->m_position.y - position.y;
    const float dz = m_goal->m_position.z - position.z;
    return cost + std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ELF 0x300100. Appends the chain from the start to `goal` (at most 201 nodes walked back from the
// goal) and the goal's f cost. Each step records its polygon, the polygon it was entered from
// (UINT_MAX for the start) and its entry point. The path's nodes are not cleared first.
void QTSearchNodeManager::buildPath(QTPath& path, QTSearchNode* goal)
{
    m_pathNodes.resize(0);

    QTSearchNode* node = goal;
    int count = 0;
    while (node != 0)
    {
        ++count;
        m_pathNodes.push_back(node);
        node = node->m_parent;
        if (count > 200)
            break;
    }

    for (int i = static_cast<int>(m_pathNodes.size()) - 1; i >= 0; --i)
    {
        const unsigned int polygon = m_pathNodes[i]->m_key;
        const unsigned int enteredFrom = m_pathNodes[i]->m_expandedFrom != 0
                                             ? m_pathNodes[i]->m_expandedFrom->m_key
                                             : 0xFFFFFFFFu;
        path.m_nodes.push_back(make_triple(polygon, enteredFrom, m_pathNodes[i]->m_position));
    }
    path.m_i20 = goal->m_f;
}

// ELF 0x300310. Sums the step costs from the path's current index. The loop runs size - 1 steps
// from there regardless of that start, so a non-zero start reads past the end (a null position);
// retail behaviour.
float QTSearchNodeManager::calculateCostOfPath(const QTPath& path) const
{
    float total = 0.0f;
    const unsigned int steps = static_cast<unsigned int>(path.m_nodes.size());
    unsigned int polygon = path.getPolygon(path.m_i0);
    const Vec3* position = path.getPosition(path.m_i0);
    for (unsigned int i = 1; i < steps; ++i)
    {
        const Vec3* const nextPosition = path.getPosition(path.m_i0 + i);
        const unsigned int nextPolygon = path.getPolygon(path.m_i0 + i);
        total += calculateCostBetweenPolygons(polygon, *position, nextPolygon, *nextPosition);
        polygon = path.getPolygon(path.m_i0 + i);
        position = path.getPosition(path.m_i0 + i);
    }
    return total;
}

// ELF 0x3004B0. The midpoint of the polygon edge from corner `edge` to the next (wrapping).
void QTSearchNodeManager::calculateEntryPoint(Vec3& entry, unsigned short edge, QTSearchNode* node)
{
    const Vec3* const first =
        m_quadTree->getVertexForPolygon(node->m_key, static_cast<unsigned char>(edge));
    const Vec3* const second = m_quadTree->getSafeVertexForPolygon(
        node->m_key, static_cast<unsigned char>(static_cast<unsigned char>(edge) + 1));
    entry.x = (second->x + first->x) * 0.5f;
    entry.y = (second->y + first->y) * 0.5f;
    entry.z = (second->z + first->z) * 0.5f;
}

// ELF 0x300560.
int QTSearchNodeManager::getSizeOfNodeCache() const
{
    int size = 0;
    for (QTSearchNode* node = m_nodeCache; node != 0; node = node->m_next)
        ++size;
    return size;
}

// ELF 0x300580: always true in the shipped build.
bool QTSearchNodeManager::checkIfNodeHasAdequateSize(QTSearchNode*, unsigned char) const
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
