// CPNodeManager. Evidence: libaidll.so 1.5.3153 (ia-32), see CPNodeManager.h.

#include "CPNodeManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x35C800, __tcf_0 0x35C860, _GLOBAL__I 0x35C890).
#include <iostream>

#include "CPNode.h"
#include "CoverPositionManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x35C4F0 (C2; C1 0x35C530): the empty vector, a null pool, goal -1, no filter.
CPNodeManager::CPNodeManager()
    : m_nodes(0)
    , m_goal(-1)
    , m_filter(0)
{
}

// ELF 0x35C5F0 (D1; D2 0x35C570): every pooled node is unlinked and deleted (CPNode D1 0x35BEB0,
// then operator delete); the vector goes as a member.
CPNodeManager::~CPNodeManager()
{
    while (m_nodes != 0)
    {
        CPNode* const node = m_nodes;
        m_nodes = node->m_next;
        delete node;
    }
}

// ELF 0x35C670.
void CPNodeManager::init()
{
    m_status.resize(CoverPositionManager::m_instance->m_coverPositions.size(),
                    std::pair<bool, bool>(false, false));
}

// ELF 0x35C710: resize(0) (the empty-vector arm calls _M_fill_insert with a count of 0, so this
// is resize rather than clear), then the init sizing.
void CPNodeManager::reset()
{
    m_status.resize(0, std::pair<bool, bool>(false, false));
    m_status.resize(CoverPositionManager::m_instance->m_coverPositions.size(),
                    std::pair<bool, bool>(false, false));
}

} // namespace ai
} // namespace hfe
} // namespace dice
