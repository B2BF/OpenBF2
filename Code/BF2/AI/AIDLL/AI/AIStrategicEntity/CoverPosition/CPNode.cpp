// CPNode. Evidence: libaidll.so 1.5.3153 (ia-32), see CPNode.h.

#include "CPNode.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x35C440, __tcf_0 0x35C4A0, _GLOBAL__I 0x35C4D0).
#include <iostream>

#include "../../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x35BAC0 (C2; C1 0x35BB00): six zero words.
CPNode::CPNode()
    : m_parent(0)
    , m_next(0)
    , m_prev(0)
    , m_key(0)
    , m_g(0.0f)
    , m_f(0.0f)
{
}

// ELF 0x35BB40 (C2; C1 0x35BCC0). The cover position goes through checkPointer.
CPNode::CPNode(CPNode* parent, int coverPosition)
    : m_parent(parent)
    , m_next(0)
    , m_prev(0)
    , m_key(checkPointer(CoverPositionManager::m_instance->m_coverPositions[coverPosition]))
    , m_g(0.0f)
    , m_f(0.0f)
{
}

// ELF 0x35BE40 (C2; C1 0x35BE70): every word copied.
CPNode::CPNode(const CPNode& other)
    : m_parent(other.m_parent)
    , m_next(other.m_next)
    , m_prev(other.m_prev)
    , m_key(other.m_key)
    , m_g(other.m_g)
    , m_f(other.m_f)
{
}

// ELF 0x35BEA0 (D2; D1 0x35BEB0).
CPNode::~CPNode()
{
}

// ELF 0x35BEC0.
void CPNode::setCoverPosition(int index)
{
    m_key = checkPointer(CoverPositionManager::m_instance->m_coverPositions[index]);
}

// ELF 0x35C010.
void CPNode::setCoverPosition(CoverPosition* coverPosition)
{
    m_key = coverPosition;
}

// ELF 0x35C020. An empty neighbour set is tested by its size; the iterator then starts at begin.
CPNode* CPNode::expandFirst(CPNodeManager* manager, nodeStatus::Status& status, NeighbourIterator& it)
{
    if (m_key->m_moveNeighbours.empty())
    {
        status = nodeStatus::kInvalid;
        return 0;
    }
    it = m_key->m_moveNeighbours.begin();
    const int neighbour = *it;
    ++it;
    getStatus(manager, status, neighbour);
    return manager->getNewNode(this, neighbour);
}

// ELF 0x35C1E0.
CPNode* CPNode::expand(CPNodeManager* manager, nodeStatus::Status& status, NeighbourIterator& it)
{
    if (it == m_key->m_moveNeighbours.end())
    {
        status = nodeStatus::kInvalid;
        return 0;
    }
    const int neighbour = *it;
    ++it;
    getStatus(manager, status, neighbour);
    return manager->getNewNode(this, neighbour);
}

// ELF 0x35C3A0. The goal test comes first (0x35C3B8). The manager's +0x14 filter, when non-null,
// is then asked through its virtual +8 with the position (0x35C420..0x35C42C), and a false byte
// gives kInvalid (0x35C3C9 / 0x35C413). The filter is always null in retail (see CPNodeFilter),
// so that arm is never taken; it is kept as shipped.
void CPNode::getStatus(CPNodeManager* manager, nodeStatus::Status& status, int coverPosition)
{
    if (manager->m_goal == coverPosition)
    {
        status = nodeStatus::kGoal;
        return;
    }
    bool mayEnter = true;
    if (manager->m_filter != 0)
        mayEnter = manager->m_filter->mayEnter(coverPosition);
    if (!mayEnter)
    {
        status = nodeStatus::kInvalid;
        return;
    }
    const std::pair<bool, bool>& flags = manager->m_status[coverPosition];
    if (flags.first)
        status = nodeStatus::kOpen;
    else
        status = flags.second ? nodeStatus::kClosed : nodeStatus::kUnvisited;
}

} // namespace ai
} // namespace hfe
} // namespace dice
