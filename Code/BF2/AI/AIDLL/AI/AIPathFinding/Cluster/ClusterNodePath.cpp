// The cluster path result's out-of-line members. [INFERRED] file name: the image keeps them in
// their own translation unit (own static initialisation) after ClusterNodeManager's.

#include "ClusterNodeManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x322170, __tcf_0 0x3221D0, _GLOBAL__I 0x322200).
#include <iostream>

#include "ClusterGraph.h"
#include "../../Main/IAIPathfinding.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x321E90 (C2; C1 0x321EC0). Only the step index and the node vector are set; map, cost
// type and island are left to setVehicle and clusterSearch.
ClusterNodePath::ClusterNodePath()
    : m_i0(0)
{
}

// ELF 0x321EF0 (D2; D1 0x321F50).
ClusterNodePath::~ClusterNodePath()
{
}

// ELF 0x321FB0.
void ClusterNodePath::setVehicle(int map, int vehicleId)
{
    m_map = map;
    m_costType = IAIPathfinding::instance->getClusterCostType(m_map, vehicleId);
}

// ELF 0x322000.
bool ClusterNodePath::getPoint(Vec3& position, unsigned int& polygon, unsigned int index) const
{
    if (index >= m_nodes.size() || m_nodes[index].second == 0xFFFF)
        return false;
    ClusterGraph* const graph =
        static_cast<ClusterGraph*>(IAIPathfinding::instance->getClusterGraph(m_map, m_island));
    const ClusterPortal* const portal = graph->getClusterPortal(
        m_nodes[index].first, static_cast<unsigned short>(m_costType), m_nodes[index].second);
    position = portal->m_position;
    polygon = portal->m_polygon;
    return true;
}

// ELF 0x3220D0.
const Vec3& ClusterNodePath::getPoint(unsigned int index) const
{
    const unsigned int step = m_i0 + index;
    if (step >= m_nodes.size() || m_nodes[step].second == 0xFFFF)
        return Vec3::getZero();
    ClusterGraph* const graph =
        static_cast<ClusterGraph*>(IAIPathfinding::instance->getClusterGraph(m_map, m_island));
    return graph
        ->getClusterPortal(m_nodes[step].first, static_cast<unsigned short>(m_costType),
                           m_nodes[step].second)
        ->m_position;
}

} // namespace ai
} // namespace hfe
} // namespace dice
