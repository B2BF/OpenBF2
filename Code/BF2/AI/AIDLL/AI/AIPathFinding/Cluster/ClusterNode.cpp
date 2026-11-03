// The cluster A* node's out-of-line members. [INFERRED] file name: the image keeps them in their
// own translation unit (own static initialisation) between ClusterGraph's and
// ClusterNodeManager's.

#include "ClusterNodeManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x31FBB0, __tcf_0 0x31FC10, _GLOBAL__I 0x31FC40).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x31F9B0 (C2; C1 0x31FA00).
ClusterNode::ClusterNode(unsigned short cluster, unsigned int depth)
    : m_next(0)
    , m_prev(0)
    , m_parent(0)
    , m_g(0.0f)
    , m_f(0.0f)
    , m_key(cluster)
    , m_portal(0xFFFF)
    , m_depth(static_cast<int>(depth))
{
}

// ELF 0x31FA50 (C2; C1 0x31FAA0).
ClusterNode::ClusterNode(const ClusterNode& other)
    : m_next(0)
    , m_prev(0)
    , m_parent(0)
    , m_g(0.0f)
    , m_f(0.0f)
    , m_key(other.m_key)
    , m_portal(other.m_portal)
    , m_depth(0)
{
}

// ELF 0x31FAF0 (D2; D1 0x31FB00): empty.
ClusterNode::~ClusterNode()
{
}

// ELF 0x31FB10.
ClusterNode* ClusterNode::expandFirst(ClusterNodeManager* manager, nodeStatus::Status& status,
                                      unsigned short& portal)
{
    portal = 0;
    ClusterNode* const neighbour = manager->getNeighbourOfNode(this, portal, status);
    ++portal;
    return neighbour;
}

// ELF 0x31FB60.
ClusterNode* ClusterNode::expand(ClusterNodeManager* manager, nodeStatus::Status& status,
                                 unsigned short& portal)
{
    ClusterNode* const neighbour = manager->getNeighbourOfNode(this, portal, status);
    ++portal;
    return neighbour;
}

} // namespace ai
} // namespace hfe
} // namespace dice
