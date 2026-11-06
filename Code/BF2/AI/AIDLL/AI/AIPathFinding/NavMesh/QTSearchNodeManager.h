// Navigation-mesh A* search nodes and their manager: node pool, per-polygon open/closed status,
// neighbour expansion, costs and path building.
// Placement of QTSearchNode in this file is [INFERRED]: its functions sit beside the manager's in
// the ELF and the anonymous `checkNodeHeritage` helper is between them.
#ifndef AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QTSEARCHNODEMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QTSEARCHNODEMANAGER_H

#include <set>
#include <vector>

#include "../../Main/AIPathfindingSearchTypes.h"   // QTPath, nodeStatus

namespace dice {
namespace hfe {
namespace ai {

class MaterialManager;
class QuadTree;
class QTSearchNodeManager;

// One A* node: a polygon reached through a polygon edge. 44 bytes on the ELF (operator new(0x2C)).
class QTSearchNode
{
public:
    QTSearchNode(unsigned int polygon, unsigned int depth);
    // Copies only the key and the position; links, costs and depth start cleared.
    QTSearchNode(const QTSearchNode& other);
    ~QTSearchNode();

    // Neighbour iteration for AStarResource::update: expandFirst starts at edge 0. Both advance
    // `edge` past the edge they report.
    QTSearchNode* expandFirst(QTSearchNodeManager* manager, nodeStatus::Status& status,
                              unsigned short& edge);
    QTSearchNode* expand(QTSearchNodeManager* manager, nodeStatus::Status& status,
                         unsigned short& edge);

    /* +0x00 */ QTSearchNode* m_next;          // hash chain / free-list link
    /* +0x04 */ QTSearchNode* m_prev;          // hash chain back link
    /* +0x08 */ QTSearchNode* m_parent;        // the node the path continues from
    /* +0x0C */ QTSearchNode* m_expandedFrom;  // [INFERRED] name: the node whose edge gave m_position
    /* +0x10 */ unsigned int  m_key;           // the polygon
    /* +0x14 */ float         m_g;             // cost from the start
    /* +0x18 */ float         m_f;             // g plus the heuristic
    /* +0x1C */ int           m_depth;         // steps from the start
    /* +0x20 */ Vec3          m_position;      // entry point: the midpoint of the crossed edge
};

class QTSearchNodeManager
{
public:
    // One per-polygon status record, chained per bucket. 12 bytes (operator new(0xC)). Names
    // [INFERRED].
    struct StatusEntry
    {
        bool          m_open;
        bool          m_closed;
        QTSearchNode* m_node;
        StatusEntry*  m_next;
    };

    // The eight-byte record of the table at +24: a link in the used-bucket list and the bucket's
    // entry chain. `operator new[]` is called without an array cookie; the constructor zeroes
    // both words. Names [INFERRED].
    class CacheEntry
    {
    public:
        CacheEntry()
            : m_nextUsed(0)
            , m_head(0)
        {
        }

        CacheEntry*  m_nextUsed;
        StatusEntry* m_head;
    };

    // the same prime the search resource'S two hashes use. Written out as a literal in the binary.
    enum { kCacheEntries = 1031 };

    QTSearchNodeManager();
    explicit QTSearchNodeManager(QuadTree* quadTree);
    ~QTSearchNodeManager();

    void setQuadTree(QuadTree* quadTree);
    void setGoal(QTSearchNode* node);
    void setNodeOpen(QTSearchNode* node);
    void setNodeClosed(QTSearchNode* node);
    QTSearchNode* getNeighbourOfNode(QTSearchNode* node, unsigned short& edge,
                                     nodeStatus::Status& status);
    QTSearchNode* getNewNode(unsigned int polygon, QTSearchNode* parent, unsigned int depth);
    QTSearchNode* getNewNode(QTSearchNode* source);
    void reset();
    // Clears the per-search additional goals and the smoothing flag.
    void returnReset();
    bool updateNode(QTSearchNode* parent, QTSearchNode* existing, unsigned int maxDepth);
    float calculateCost(QTSearchNode* from, QTSearchNode* to, const Vec3& entry) const;
    float calculateCostBetweenPolygons(unsigned int fromPolygon, const Vec3& from,
                                       unsigned int toPolygon, const Vec3& to) const;
    float calculateHeuristicCost(float cost, const Vec3& position) const;
    void buildPath(QTPath& path, QTSearchNode* goal);
    float calculateCostOfPath(const QTPath& path) const;
    void calculateEntryPoint(Vec3& entry, unsigned short edge, QTSearchNode* node);
    int getSizeOfNodeCache() const;
    bool checkIfNodeHasAdequateSize(QTSearchNode* node, unsigned char edge) const;

    // [INFERRED] Inlined in every retail user: returns a node to the free list.
    void recycleNode(QTSearchNode* node)
    {
        node->m_next = m_nodeCache;
        m_nodeCache = node;
    }

    // layout, on the ELF, read store by store out of the constructor
    /*  +0 */ QuadTree* m_quadTree;      // the one argument the second constructor takes
    /*  +4 */ QTSearchNode* m_goal;      // = 0; setGoal's copy of the goal node
    /*  +8 */ QTSearchNode* m_nodeCache; // = 0 the intrusive free list `getSizeOfNodeCache` walks
    /* +12 */ int m_numberOfNodes;       // = 0; nodes allocated and not yet deleted
    /* +16 */ StatusEntry* m_freeEntries;// = 0; recycled status records
    /* +20 */ CacheEntry* m_usedBuckets; // = 0; buckets holding status records
    /* +24 */ CacheEntry* m_table;       // new CacheEntry[kCacheEntries]
    /* +28 */ std::vector<QTSearchNode*> m_pathNodes;  // buildPath's goal-to-start scratch list
    // Per-search options set through AIPathfinding; the two sets are borrowed, not owned.
    /* +40 */ const std::set<unsigned int>* m_additionalGoals;       // = 0; returnReset clears it
    /* +44 */ const std::set<unsigned short>* m_allowedClusters;     // = 0; reset clears it
    /* +48 */ bool m_usePathTopologySmoothing;                       // = false; returnReset clears it
    /* +52 */ MaterialManager* m_materials;  // = 0; the searching vehicle's material costs
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QTSEARCHNODEMANAGER_H
