// Strategic-area waypoints. Evidence: libaidll.so (ia-32) 0x34E980..0x34EDAF.
// [INFERRED] Source placement.

#include "SAWayPoint.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x34ED00, __tcf_0 0x34ED60, _GLOBAL__I 0x34ED90).
#include <iostream>

#include "../DrawPrimitives/DPManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x34E980 (C2; C1 0x34E9C0). Five stores and no allocation.
SAWayPoint::SAWayPoint(const Vec3& position, int map)
    : m_position(position)
    , m_island(0xFFFFFFFFu)          // 1, written as the dword the constructor stores
    , m_polygon(0xFFFFFFFFu)
    , m_map(map)
    , m_debugPrimitive(0)
{
}

// ELF 0x34EA00 (D2; D1 0x34EA30). Removes the waypoint's debug primitive; the waypoint owns no other heap resources.
SAWayPoint::~SAWayPoint()
{
    undraw();
}

// ELF 0x34EA60. Resolves the waypoint polygon with search mode 3 on the trace map and 0 otherwise.
// Requires the pathfinder singleton and preserves its separate reads for query and search.
void SAWayPoint::init()
{
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;

    const int traceMap = pathfinding->getTraceMapIndex();

    pathfinding->getPolygon(m_position, m_map, m_polygon, m_island,
                            3 * (traceMap == m_map));
}

// ELF 0x34ECB0. Deletes the debug circle when present and clears its handle unconditionally.
// Safe to repeat after an earlier undraw call.
void SAWayPoint::undraw() const
{
    if (m_debugPrimitive)
    {
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)
            ->deleteCircle(m_debugPrimitive);
    }

    m_debugPrimitive = 0;
}

// ELF 0x34EAD0. A debug circle at the waypoint: red while it has no polygon, green once resolved;
// draw type 6 on the trace map, 4 otherwise. The first call creates the circle, later calls
// update it in place.
void SAWayPoint::draw() const
{
    const int type = (IAIPathfinding::instance->getTraceMapIndex() == m_map) ? 6 : 4;
    Vec3 normal;
    if (!m_debugPrimitive)
    {
        const unsigned int colour = (m_polygon == 0xFFFFFFFFu) ? 0xFFBB0000u : 0xFF00BB00u;
        normal.x = 0.0f;
        normal.y = 1.0f;
        normal.z = 0.0f;
        // DPStaticCircle derives singly from DrawPrimitive (its typeinfo is a
        // __si_class_type_info on DrawPrimitive) and the image stores the pointer unadjusted;
        // neither class is defined in this tree, hence the cast. [CHOSEN, not recovered]
        m_debugPrimitive = reinterpret_cast<DrawPrimitive*>(
            static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
                ->createStaticCircle(1.0e9f, colour, m_position, 1.0f, normal, false, type));
    }
    else
    {
        const unsigned int colour = (m_polygon == 0xFFFFFFFFu) ? 0xFFBB0000u : 0xFF00BB00u;
        normal.x = 0.0f;
        normal.y = 1.0f;
        normal.z = 0.0f;
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)
            ->setCircleAttributes(m_debugPrimitive, 1.0e9f, colour, m_position, 1.0f, normal,
                                  false, type);
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
