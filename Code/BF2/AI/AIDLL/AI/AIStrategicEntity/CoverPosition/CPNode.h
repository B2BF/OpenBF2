// The cover-position A* node: a cover position reached from its parent. Its neighbours are the
// cover position's move neighbours (CoverPosition +0x2C). The CPNodeManager inlines the search
// templates call are defined here, after both classes are complete.
// Evidence: libaidll.so 1.5.3153 (ia-32), ELF 0x35BAC0..0x35C4D0 (CPNode's own functions and the
// TU's static initialisation; its first symbol names CPNode::CPNode()). [INFERRED] Placement
// AIStrategicEntity/CoverPosition/CPNode.cpp; no __FILE__ string attests it.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_CPNODE_H     // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_CPNODE_H

#include <set>

#include "../../AIUtils/checkPointer.h"
#include "../../Main/AIPathfindingSearchTypes.h"    // nodeStatus
#include "CPNodeManager.h"
#include "CoverPosition.h"
#include "CoverPositionManager.h"

namespace dice {
namespace hfe {
namespace ai {

// 24 bytes (operator new(0x18) in expandFirst, expand and CoverPositionManager::search).
class CPNode
{
public:
    // The iterator over a cover position's move neighbours that expandFirst / expand advance.
    typedef std::set<int>::const_iterator NeighbourIterator;

    CPNode();
    // A node on cover position `coverPosition`, reached from `parent`; costs and links cleared.
    CPNode(CPNode* parent, int coverPosition);
    CPNode(const CPNode& other);
    ~CPNode();

    // Points the node at cover position `index` of CoverPositionManager::m_instance.
    void setCoverPosition(int index);
    void setCoverPosition(CoverPosition* coverPosition);

    // Neighbour iteration for AStarResource::update: the next move neighbour as a node (from the
    // manager's pool) and its status, or null with kInvalid when there is none.
    CPNode* expandFirst(CPNodeManager* manager, nodeStatus::Status& status, NeighbourIterator& it);
    CPNode* expand(CPNodeManager* manager, nodeStatus::Status& status, NeighbourIterator& it);
    // kGoal for the manager's goal, kInvalid when the manager's filter refuses it, else kOpen,
    // kClosed or kUnvisited from the manager's per-position flags.
    void getStatus(CPNodeManager* manager, nodeStatus::Status& status, int coverPosition);

    // [INFERRED] The node has no depth word: AStarResource<CPNode>::update tests the search depth
    // limit against the constant 0.
    static const int m_depth = 0;

    // [INFERRED] names; offsets from the constructors and the search templates.
    /*  +0 */ CPNode* m_parent;
    /*  +4 */ CPNode* m_next;            // hash chain and the manager's free-list link
    /*  +8 */ CPNode* m_prev;            // hash chain back link
    /* +12 */ CoverPosition* m_key;      // the cover position; the search compares nodes by it
    /* +16 */ float m_g;                 // cost from the start
    /* +20 */ float m_f;                 // the open tree's order
};

// The Hash hooks (AIPathfindingSearchTypes.h): a CPNode hashes to its cover position's index with
// no modulus (Hash<CPNode>::insert 0x359B20, get 0x35B8C0, remove 0x35B960) and compares by cover
// position pointer.
inline int hashIndexOf(const CPNode* node, int)
{
    return node->m_key->m_id;
}

inline bool hashKeyEquals(const CPNode* a, const CPNode* b)
{
    return a->m_key == b->m_key;
}

// The manager's inline operations, as AStarResource<CPNode> (0x359BC0, 0x35B7D0) and
// CoverPositionManager::search inline them. [INFERRED] names (shared with QTSearchNodeManager).

inline void CPNodeManager::setNodeOpen(CPNode* node)
{
    m_status[node->m_key->m_id].first = true;
}

inline void CPNodeManager::setNodeClosed(CPNode* node)
{
    m_status[node->m_key->m_id].first = false;
    m_status[node->m_key->m_id].second = true;
}

inline void CPNodeManager::recycleNode(CPNode* node)
{
    if (node != 0)
    {
        node->m_next = m_nodes;
        m_nodes = node;
    }
}

// A cheaper route through `from`: the cost is the distance-table entry plus from's g. Shipped
// defect, preserved: only f changes, by -(g + cost); g and the parent keep their old values.
inline bool CPNodeManager::updateNode(CPNode* from, CPNode* existing, int)
{
    const float cost = CoverPositionManager::m_instance->getDistance(from->m_key->m_id,
                                                                     existing->m_key->m_id) +
                       from->m_g;
    if (cost < existing->m_g)
    {
        existing->m_f -= existing->m_g + cost;
        return true;
    }
    return false;
}

// A node on `coverPosition` reached from `parent`: the head of the free list, re-pointed, or a new
// one. A reused node keeps its old costs.
inline CPNode* CPNodeManager::getNewNode(CPNode* parent, int coverPosition)
{
    CPNode* node = m_nodes;
    if (node == 0)
        return checkPointer(new CPNode(parent, coverPosition));
    m_nodes = node->m_next;
    node->setCoverPosition(coverPosition);
    node->m_parent = parent;
    return node;
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_CPNODE_H
