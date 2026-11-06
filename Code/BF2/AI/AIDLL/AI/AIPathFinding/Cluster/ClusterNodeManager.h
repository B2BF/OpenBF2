// Cluster-graph A* search nodes and their manager: node and open/closed-record pools, portal
// expansion, costs and path building. Placement of ClusterNode and ClusterNodePath's methods in
// this file is [INFERRED]: their functions sit beside the manager's in the ELF.

#ifndef AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERNODEMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERNODEMANAGER_H

#include <vector>

#include "../../Main/AIPathfindingSearchTypes.h"      // Hash, nodeStatus

namespace dice {
namespace hfe {
namespace ai {

class ClusterGraph;
class ClusterNodeManager;
class QuadTree;

// One A* node: a cluster entered through one of its portals. 28 bytes (operator new(0x1C)).
class ClusterNode
{
public:
    ClusterNode(unsigned short cluster, unsigned int depth);
    // Copies the cluster and the entry portal; links, costs and depth start cleared.
    ClusterNode(const ClusterNode& other);
    ~ClusterNode();

    // Portal iteration for AStarResource::update: expandFirst starts at portal 0. Both advance
    // `portal` past the one they report.
    ClusterNode* expandFirst(ClusterNodeManager* manager, nodeStatus::Status& status,
                             unsigned short& portal);
    ClusterNode* expand(ClusterNodeManager* manager, nodeStatus::Status& status,
                        unsigned short& portal);

    /* +0x00 */ ClusterNode*   m_next;      // hash chain / free-list link
    /* +0x04 */ ClusterNode*   m_prev;
    /* +0x08 */ ClusterNode*   m_parent;
    /* +0x0C */ float          m_g;
    /* +0x10 */ float          m_f;
    /* +0x14 */ unsigned short m_key;       // the cluster
    /* +0x16 */ unsigned short m_portal;    // entry portal index in this cluster; 0xFFFF for the start
    /* +0x18 */ int            m_depth;
};

class ClusterNodeManager
{
public:
    // The open/closed record of one cluster node, kept in a hash keyed by the node's cluster.
    // 16 bytes; +12 is not written on allocation. Names [INFERRED]; the nested type is attested by
    // the mangled names of getNewOCData, recycleOCData and HashIterator<ClusterNodeManager::OCData>.
    class OCData
    {
    public:
        /*  +0 */ bool         m_open;
        /*  +1 */ bool         m_closed;
        /*  +4 */ ClusterNode* m_node;
        /*  +8 */ OCData*      m_next;      // hash chain / free-list link
        /* +12 */ OCData*      m_prev;
    };

    // the same prime the cluster search resource'S two hashes use. A literal in the binary.
    enum { kOCDataBuckets = 313 };

    ClusterNodeManager();
    ~ClusterNodeManager();

    void setQuadTree(QuadTree* quadTree);
    void setClusterGraph(ClusterGraph* clusterGraph);
    void setGoal(ClusterNode* node);
    void setNodeOpen(ClusterNode* node);
    void setNodeClosed(ClusterNode* node);
    ClusterNode* getNeighbourOfNode(ClusterNode* node, unsigned short& portal,
                                    nodeStatus::Status& status);
    ClusterNode* getNewNode(unsigned short cluster, ClusterNode* parent, unsigned int depth);
    ClusterNode* getNewNode(ClusterNode* source);
    void reset();
    void returnReset();
    bool updateNode(ClusterNode* parent, ClusterNode* existing, unsigned int maxDepth);
    float calculateCost(ClusterNode* node, unsigned short portal) const;
    float calculateHeuristicCost(float cost, unsigned short portal, ClusterNode* node) const;
    void buildPath(ClusterNodePath& path, ClusterNode* goal);
    OCData* getNewOCData();
    void recycleOCData(OCData* data);
    const Vec3* getEntryPoint(unsigned short portal, ClusterNode* node) const;
    float findBestPortalAndCost(unsigned short& bestPortal, unsigned short& neighbourPortal,
                                ClusterNode* node, unsigned short entryPortal,
                                unsigned short portal, unsigned short cluster) const;

    // [INFERRED] Inlined in every retail user: returns a node to the free list.
    void recycleNode(ClusterNode* node)
    {
        node->m_next = m_freeNodes;
        m_freeNodes = node;
    }

    // layout, on the ELF
    /*  +0 */ QuadTree* m_quadTree;          // = 0; `setQuadTree` writes it
    /*  +4 */ ClusterGraph* m_clusterGraph;  // = 0; `setClusterGraph` writes it
    /*  +8 */ Vec3 m_start;                  // the search's start position
    /* +20 */ Vec3 m_goalPosition;           // the search's goal position
    /* +32 */ ClusterNode* m_goal;           // = 0; setGoal's copy of the goal node
    /* +36 */ ClusterNode* m_freeNodes;      // = 0
    /* +40 */ int m_nodeCacheSize;           // = 0  // name: AIDLL_r assert 0x1018FDD0 (PE +0x28)
    /* +44 */ OCData* m_freeOCData;          // = 0 the intrusive free list `recycleOCData` pushes to
    /* +48 */ int m_ocCacheSize;          // = 0  // name: AIDLL_r assert 0x1018FDD0 (PE +0x30)
    /* +52 */ Hash<OCData, ClusterNodeManager, OCData>* m_ocDataHash;   // new Hash(313), then ->this
    /* +56 */ std::vector<ClusterNode*> m_pathNodes;   // buildPath's goal-to-start scratch list
    /* +68 */ int m_costType;                // the vehicle's cluster cost type; read as 16 bits
};

// The status records hash by their node's cluster.
inline int hashKeyOf(const ClusterNodeManager::OCData* data)
{
    return static_cast<int>(data->m_node->m_key);
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERNODEMANAGER_H
