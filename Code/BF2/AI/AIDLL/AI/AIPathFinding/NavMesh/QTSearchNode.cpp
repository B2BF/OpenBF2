// The local (nav-mesh) A* node's out-of-line members. [INFERRED] file name: the image keeps them
// in their own translation unit (own static initialisation) between QTPath's and
// QTSearchNodeManager's.

#include "QTSearchNodeManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2FED20, __tcf_0 0x2FED80, _GLOBAL__I 0x2FEDB0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2FEAA0 (C2; C1 0x2FEB00).
QTSearchNode::QTSearchNode(unsigned int polygon, unsigned int depth)
    : m_next(0)
    , m_prev(0)
    , m_parent(0)
    , m_expandedFrom(0)
    , m_key(polygon)
    , m_g(0.0f)
    , m_f(0.0f)
    , m_depth(static_cast<int>(depth))
{
    m_position.x = 0.0f;
    m_position.y = 0.0f;
    m_position.z = 0.0f;
}

// ELF 0x2FEB60 (C2; C1 0x2FEBE0).
QTSearchNode::QTSearchNode(const QTSearchNode& other)
    : m_next(0)
    , m_prev(0)
    , m_parent(0)
    , m_expandedFrom(0)
    , m_key(other.m_key)
    , m_g(0.0f)
    , m_f(0.0f)
    , m_depth(0)
    , m_position(other.m_position)
{
}

// ELF 0x2FEC60 (D2; D1 0x2FEC70): empty.
QTSearchNode::~QTSearchNode()
{
}

// ELF 0x2FEC80.
QTSearchNode* QTSearchNode::expandFirst(QTSearchNodeManager* manager, nodeStatus::Status& status,
                                        unsigned short& edge)
{
    edge = 0;
    QTSearchNode* const neighbour = manager->getNeighbourOfNode(this, edge, status);
    ++edge;
    return neighbour;
}

// ELF 0x2FECD0.
QTSearchNode* QTSearchNode::expand(QTSearchNodeManager* manager, nodeStatus::Status& status,
                                   unsigned short& edge)
{
    QTSearchNode* const neighbour = manager->getNeighbourOfNode(this, edge, status);
    ++edge;
    return neighbour;
}

} // namespace ai
} // namespace hfe
} // namespace dice
