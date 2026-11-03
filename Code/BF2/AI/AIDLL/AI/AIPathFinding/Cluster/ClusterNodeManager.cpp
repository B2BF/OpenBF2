// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining ClusterNodeManager operations.

#include "ClusterNodeManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x3215F0, __tcf_0 0x321650, _GLOBAL__I 0x321680).
#include <iostream>

#include <cmath>

#include "ClusterGraph.h"
#include "../NavMesh/QuadTree.h"
#include "../../AIUtils/checkPointer.h"
#include "../../Main/IAIPathfinding.h"
#include "../../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF 0x31FC60. Reports a node that is its own ancestor within ten generations. The result is
// discarded by both retail callers.
bool checkHeritage(ClusterNode* node)
{
    int generation = 0;
    for (ClusterNode* ancestor = node->m_parent; ancestor != 0; ancestor = ancestor->m_parent)
    {
        if (ancestor == node)
        {
            Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterNodeManager.cpp", 38, "")
                << "Circular heritage for " << static_cast<unsigned long>(ancestor->m_key);
            return false;
        }
        ++generation;
        if (generation > 9)
            break;
    }
    return true;
}

} // namespace

// ClusterNodeManager

// ELF 0x31FDD0 (C2; C1 0x320010). Positions zeroed, empty pools, a 313-bucket status hash linked to this manager.
ClusterNodeManager::ClusterNodeManager()
    : m_quadTree(0)
    , m_clusterGraph(0)
    , m_goal(0)
    , m_freeNodes(0)
    , m_numberOfNodes(0)
    , m_freeOCData(0)
    , m_numberOfOCData(0)
    , m_ocDataHash(checkPointer(new Hash<OCData, ClusterNodeManager, OCData>(kOCDataBuckets)))
    , m_costType(0)
{
    m_start.x = 0.0f;
    m_start.y = 0.0f;
    m_start.z = 0.0f;
    m_goalPosition.x = 0.0f;
    m_goalPosition.y = 0.0f;
    m_goalPosition.z = 0.0f;
    m_ocDataHash->m_nodeManager = this;
}

// ELF 0x3201E0 (D2; D1 0x320370). Frees the node pool, the record pool, every hashed record and its node, the hash
// and the goal copy. Retail reads each hashed record's chain link after deleting it; the link is
// read first here ([CHOSEN], the same walk without the use after free).
ClusterNodeManager::~ClusterNodeManager()
{
    while (m_freeNodes != 0)
    {
        ClusterNode* const node = m_freeNodes;
        m_freeNodes = node->m_next;
        delete node;
        --m_numberOfNodes;
    }

    while (m_freeOCData != 0)
    {
        OCData* const data = m_freeOCData;
        m_freeOCData = data->m_next;
        delete data;
        --m_numberOfOCData;
    }

    Hash<OCData, ClusterNodeManager, OCData>* const hash = m_ocDataHash;
    Hash<OCData, ClusterNodeManager, OCData>::iterator it = hash->begin();
    while (it != hash->end())
    {
        OCData* const data = it.m_node;
        OCData* const next = data->m_next;
        if (data->m_node != 0)
            delete data->m_node;
        delete data;
        --m_numberOfOCData;
        --m_numberOfNodes;
        it.m_node = next;
        if (it.m_node == 0)
        {
            it.m_bucket = it.m_bucket->m_nextUsed;
            if (it.m_bucket != 0)
                it.m_node = it.m_bucket->m_head;
        }
    }
    if (hash != 0)
    {
        if (hash->m_buckets != 0)
            delete[] hash->m_buckets;
        delete hash;
    }

    if (m_goal != 0)
    {
        delete m_goal;
        --m_numberOfNodes;
    }
}

// ELF 0x320500.
void ClusterNodeManager::setQuadTree(QuadTree* quadTree)
{
    m_quadTree = quadTree;
}

// ELF 0x320510.
void ClusterNodeManager::setClusterGraph(ClusterGraph* clusterGraph)
{
    m_clusterGraph = clusterGraph;
}

// ELF 0x320520. Keeps a private copy of the goal's cluster and portal and recycles `node`.
void ClusterNodeManager::setGoal(ClusterNode* node)
{
    if (m_goal == 0)
        m_goal = getNewNode(node);
    else
    {
        m_goal->m_key = node->m_key;
        m_goal->m_portal = node->m_portal;
    }
    recycleNode(node);
}

// ELF 0x320590. A probe record is built first; an existing record is marked open and the probe
// recycled, otherwise the probe itself is hashed as the node's open record.
void ClusterNodeManager::setNodeOpen(ClusterNode* node)
{
    OCData* const probe = getNewOCData();
    probe->m_open = true;
    probe->m_node = node;
    probe->m_closed = false;

    OCData* const existing = m_ocDataHash->get(probe);
    if (existing != 0)
    {
        recycleOCData(probe);
        existing->m_open = true;
        return;
    }
    m_ocDataHash->insert(probe);
}

// ELF 0x3206B0. The record must exist: retail writes through the lookup unchecked.
void ClusterNodeManager::setNodeClosed(ClusterNode* node)
{
    OCData* const probe = getNewOCData();
    probe->m_open = true;
    probe->m_closed = false;
    probe->m_node = node;
    OCData* const existing = m_ocDataHash->get(probe);
    recycleOCData(probe);
    existing->m_open = false;
    existing->m_closed = true;
}

// ELF 0x320730. The neighbour cluster behind the first usable portal at or after `portal`. A portal
// is usable when the best route to its neighbour cluster costs something non-negative, does not
// start from the portal this node was entered through, and is not an earlier portal (so each
// neighbour is reached once, through its cheapest portal). The new node enters the neighbour
// through the matching portal there; unseen neighbours are costed (the goal only flagged).
ClusterNode* ClusterNodeManager::getNeighbourOfNode(ClusterNode* node, unsigned short& portal,
                                                    nodeStatus::Status& status)
{
    const Cluster& cluster = m_clusterGraph->m_clusters[node->m_key];
    unsigned short neighbourPortal = 0xFFFF;
    unsigned short bestPortal;
    if (portal >= cluster.m_numberOfPortals)
    {
        status = nodeStatus::kInvalid;
        return 0;
    }

    float cost = findBestPortalAndCost(bestPortal, neighbourPortal, node, node->m_portal, portal,
                                       node->m_key);
    for (;;)
    {
        if (cluster.m_numberOfPortals <= portal)
        {
            status = nodeStatus::kInvalid;
            return 0;
        }
        if (cost >= 0.0f && neighbourPortal != 0xFFFF && node->m_portal != portal &&
            bestPortal >= portal)
            break;
        ++portal;
        cost = findBestPortalAndCost(bestPortal, neighbourPortal, node, node->m_portal, portal,
                                     node->m_key);
    }

    const ClusterPortal* const exit = m_clusterGraph->getClusterPortal(
        node->m_key, static_cast<unsigned short>(m_costType), bestPortal);
    ClusterNode* const neighbour = getNewNode(exit->m_neighbourCluster, node, node->m_depth + 1);

    OCData* const probe = getNewOCData();
    probe->m_open = true;
    probe->m_closed = false;
    probe->m_node = neighbour;
    OCData* const existing = m_ocDataHash->get(probe);
    if (existing != 0)
    {
        if (existing->m_open)
            status = nodeStatus::kOpen;
        else if (existing->m_closed)
            status = nodeStatus::kClosed;
        else
        {
            Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/Cluster/ClusterNodeManager.cpp", 233, "")
                << "This case should not exists!";
            status = nodeStatus::kInvalid;
        }
        recycleOCData(probe);
        return neighbour;
    }

    checkHeritage(neighbour);
    neighbour->m_portal = neighbourPortal;
    if (neighbour->m_key == m_goal->m_key)
        status = nodeStatus::kGoal;
    else
    {
        status = nodeStatus::kUnvisited;
        neighbour->m_g = cost;
        neighbour->m_f = calculateHeuristicCost(cost, neighbourPortal, neighbour);
    }
    recycleOCData(probe);
    return neighbour;
}

// ELF 0x320B10. A recycled node keeps its costs, links and entry portal except the parent.
ClusterNode* ClusterNodeManager::getNewNode(unsigned short cluster, ClusterNode* parent,
                                            unsigned int depth)
{
    ClusterNode* node = m_freeNodes;
    if (node != 0)
    {
        node->m_key = cluster;
        node->m_depth = static_cast<int>(depth);
        m_freeNodes = m_freeNodes->m_next;
    }
    else
    {
        node = new ClusterNode(cluster, depth);
        ++m_numberOfNodes;
    }
    node->m_parent = parent;
    return node;
}

// ELF 0x320B90. A recycled node is assigned every field of `source`; a new one is copy
// constructed, which keeps only the cluster and the entry portal.
ClusterNode* ClusterNodeManager::getNewNode(ClusterNode* source)
{
    ClusterNode* node = m_freeNodes;
    if (node != 0)
    {
        m_freeNodes = node->m_next;
        *node = *source;
    }
    else
    {
        node = new ClusterNode(*source);
        ++m_numberOfNodes;
    }
    return node;
}

// ELF 0x320C30. Returns every hashed record's node to the pool, unhashes and recycles the records,
// then the goal copy.
void ClusterNodeManager::reset()
{
    Hash<OCData, ClusterNodeManager, OCData>::iterator it = m_ocDataHash->begin();
    while (it != m_ocDataHash->end())
    {
        OCData* const data = it.m_node;
        recycleNode(data->m_node);
        ++it;
        m_ocDataHash->remove(data);
        recycleOCData(data);
    }

    if (m_goal != 0)
    {
        recycleNode(m_goal);
        m_goal = 0;
    }
}

// ELF 0x320DE0: empty.
void ClusterNodeManager::returnReset()
{
}

// ELF 0x320DF0. Re-costs `existing` as reached from `parent` through the portal between them;
// taken only when strictly cheaper. Refused at the depth limit and for the start node.
bool ClusterNodeManager::updateNode(ClusterNode* parent, ClusterNode* existing,
                                    unsigned int maxDepth)
{
    const int depth = parent->m_depth + 1;
    if (maxDepth == static_cast<unsigned int>(depth) || existing->m_parent == 0)
        return false;

    const unsigned short costType = static_cast<unsigned short>(m_costType);
    const unsigned short portal =
        m_clusterGraph->findClusterPortalWithNeighbour(parent->m_key, costType, existing->m_key);
    unsigned short bestPortal;
    unsigned short neighbourPortal;
    const float cost = findBestPortalAndCost(bestPortal, neighbourPortal, parent,
                                             parent->m_portal, portal, parent->m_key);
    if (0.0f > cost)
        return false;

    const float f = calculateHeuristicCost(cost, neighbourPortal, existing);
    if (f >= existing->m_f)
        return false;

    existing->m_portal =
        m_clusterGraph->getPortalForNeighbour(existing->m_key, costType, parent->m_key);
    existing->m_parent = parent;
    checkHeritage(existing);
    existing->m_g = cost;
    existing->m_f = f;
    existing->m_depth = depth;
    existing->m_portal = neighbourPortal;
    return true;
}

// ELF 0x320F30. The accumulated cost of leaving `node` through `portal`: from the start position
// for the start node, otherwise the cluster's portal-to-portal table (-1 when negative).
float ClusterNodeManager::calculateCost(ClusterNode* node, unsigned short portal) const
{
    if (node == 0)
        return 0.0f;

    const unsigned short costType = static_cast<unsigned short>(m_costType);
    if (node->m_portal != 0xFFFF)
    {
        const float step = m_clusterGraph->getCostBetweenClustersPortals(node->m_key,
                                                                         node->m_portal, portal,
                                                                         costType);
        if (0.0f > step)
            return -1.0f;
        return step + node->m_g;
    }

    const ClusterPortal* const exit = m_clusterGraph->getClusterPortal(node->m_key, costType, portal);
    const float dx = exit->m_position.x - m_start.x;
    const float dy = exit->m_position.y - m_start.y;
    const float dz = exit->m_position.z - m_start.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz) + node->m_g;
}

// ELF 0x321060. g plus the straight distance from the node's entry point to the goal position.
float ClusterNodeManager::calculateHeuristicCost(float cost, unsigned short portal,
                                                 ClusterNode* node) const
{
    const Vec3* const entry = getEntryPoint(portal, node);
    const float dx = m_goalPosition.x - entry->x;
    const float dy = m_goalPosition.y - entry->y;
    const float dz = m_goalPosition.z - entry->z;
    return cost + std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ELF 0x321110. Appends (cluster, entry portal) from the start to `goal`. The path's nodes are not
// cleared first; no cost is stored.
void ClusterNodeManager::buildPath(ClusterNodePath& path, ClusterNode* goal)
{
    m_pathNodes.resize(0);
    for (ClusterNode* node = goal; node != 0; node = node->m_parent)
        m_pathNodes.push_back(node);

    for (int i = static_cast<int>(m_pathNodes.size()) - 1; i >= 0; --i)
        path.m_nodes.push_back(std::make_pair(m_pathNodes[i]->m_key, m_pathNodes[i]->m_portal));
}

// ELF 0x321280. checkPointer guards the allocation (checkPointer.h line 46).
ClusterNodeManager::OCData* ClusterNodeManager::getNewOCData()
{
    if (m_freeOCData != 0)
    {
        OCData* const data = m_freeOCData;
        m_freeOCData = data->m_next;
        return data;
    }

    ++m_numberOfOCData;
    OCData* const data = new OCData;
    data->m_open = true;
    data->m_closed = false;
    data->m_node = 0;
    data->m_next = 0;
    return checkPointer(data);
}

// ELF 0x3213F0.
void ClusterNodeManager::recycleOCData(OCData* data)
{
    data->m_next = m_freeOCData;
    m_freeOCData = data;
}

// ELF 0x321410. The start position for a node without a parent, else the portal's position.
const Vec3* ClusterNodeManager::getEntryPoint(unsigned short portal, ClusterNode* node) const
{
    if (node->m_parent == 0)
        return &m_start;
    return &m_clusterGraph
                ->getClusterPortal(node->m_key, static_cast<unsigned short>(m_costType), portal)
                ->m_position;
}

// ELF 0x321470. Among the cluster's portals into the same neighbour as `portal`, the cheapest
// non-negative calculateCost; its index goes to `bestPortal` and the neighbour's matching portal
// to `neighbourPortal` (0xFFFF when none). -1 when the neighbour is not a valid, walkable
// cluster or nothing qualifies. `entryPortal` is not read.
float ClusterNodeManager::findBestPortalAndCost(unsigned short& bestPortal,
                                                unsigned short& neighbourPortal,
                                                ClusterNode* node, unsigned short,
                                                unsigned short portal,
                                                unsigned short cluster) const
{
    const Cluster& record = m_clusterGraph->m_clusters[cluster];
    const unsigned short costType = static_cast<unsigned short>(m_costType);
    const unsigned int neighbour =
        m_clusterGraph->getClusterPortal(cluster, costType, portal)->m_neighbourCluster;
    if (neighbour >= m_clusterGraph->m_numberOfClusters ||
        m_clusterGraph->m_clusters[neighbour].m_walkable == 0)
        return -1.0f;

    float best = 1e9f;
    neighbourPortal = 0xFFFF;
    for (unsigned short i = 0; i < record.m_numberOfPortals; ++i)
    {
        ClusterPortal* const candidate = m_clusterGraph->getClusterPortal(cluster, costType, i);
        if (candidate->m_neighbourCluster != neighbour)
            continue;
        const float cost = calculateCost(node, i);
        if (best > cost && cost >= 0.0f)
        {
            bestPortal = i;
            neighbourPortal = m_clusterGraph->getCorrespongingPortalInNeighbour(candidate, costType);
            best = cost;
        }
    }
    return neighbourPortal == 0xFFFF ? -1.0f : best;
}

} // namespace ai
} // namespace hfe
} // namespace dice
