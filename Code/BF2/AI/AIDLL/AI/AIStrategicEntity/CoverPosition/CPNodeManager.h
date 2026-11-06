// The node pool and per-position open/closed flags of one cover-position search.
// Evidence: libaidll.so 1.5.3153 (ia-32), ELF 0x35C4F0..0x35C890 (constructors, destructors, init,
// reset, the TU's static initialisation and the vector<pair<bool, bool> >::erase instantiation).
// [INFERRED] Placement AIStrategicEntity/CoverPosition/CPNodeManager.cpp.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_CPNODEMANAGER_H     // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_CPNODEMANAGER_H

#include <utility>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class CPNode;

// [CHOSEN, not recovered] Interface of CPNodeManager::m_filter, declared under AGENTS.md's
// null-only member pointer clause. libaidll.so writes that member only as null (constructors
// ELF 0x35C51B / 0x35C55B; amd-64 0x3962A6 / 0x3962D6), and no setter, copy, engine producer or
// vtable exists in any inspected image. So the single call through it (CPNode::getStatus,
// 0x35C429 `call [eax+8]`; amd-64 0x3958FE `call [rax+0x10]`) is never taken in retail.
// The call site proves only: receiver, one int position argument, a tested byte result.
// [UNRESOLVED] Original class, method names and leading slots. The two leading slots are not
// claimed to be destructors. Never give this interface an implementation, setter or factory.
class CPNodeFilter
{
public:
    virtual void chosenSlot0() = 0;                       // +0x00 [CHOSEN]
    virtual void chosenSlot1() = 0;                       // +0x04 [CHOSEN]
    virtual bool mayEnter(int coverPosition) = 0;         // +0x08 [CHOSEN] name; bool for the tested byte

protected:
    ~CPNodeFilter() {}
};

class CPNodeManager
{
public:
    CPNodeManager();
    ~CPNodeManager();           // not virtual

    // Sizes the flags to the cover-position count, new entries (false, false).
    void init();
    // Clears every flag, then sizes them as init does. The node pool is kept.
    void reset();
    // [INFERRED] inline and empty, added by ai-nav: ~PathfindingEngine (ELF 0x3593F0, the CPNode
    // instantiation) has no call between `+0xC = -1` and `+0x1C = -1` where the QTSearchNode
    // (0x3AE784) and ClusterNode (0x3AE994) instantiations call returnReset, and the image has no
    // CPNodeManager::returnReset symbol.
    void returnReset() {}

    // Inline in retail; defined in CPNode.h.
    void setNodeOpen(CPNode* node);
    void setNodeClosed(CPNode* node);
    void recycleNode(CPNode* node);
    bool updateNode(CPNode* from, CPNode* existing, int maxSearchDepth);
    CPNode* getNewNode(CPNode* parent, int coverPosition);

    // Layout from the 54-byte constructor; names [INFERRED].
    // Per cover position: (open, closed). setNodeOpen sets first; setNodeClosed clears first and
    // sets second (the vector<pair<bool, bool> > instantiations in this TU and CPNode::getStatus).
    /*  +0 */ std::vector<std::pair<bool, bool> > m_status;
    // The free list of recycled nodes, threaded through CPNode::m_next; the destructor deletes it.
    /* +12 */ CPNode* m_nodes;
    /* +16 */ int m_goal;           // = -1; the goal's cover-position index (CoverPositionManager::search)
    // = 0 and never written elsewhere in any inspected image. CPNode::getStatus asks a non-null one
    // through its virtual +8 whether a position may be entered (see CPNodeFilter).
    /* +20 */ CPNodeFilter* m_filter;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_COVERPOSITION_CPNODEMANAGER_H
