// [INFERRED] Source placement: see AreaDistanceInfo.h.

#include "AreaDistanceInfo.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x34E8A0: `movl $0x0,(%eax); movl $0x0,0x4(%eax); movl <0.0f>,0x8(%eax)`.
AreaDistanceInfo::AreaDistanceInfo()
    : m_vehicleGroups(0)
    , m_area(0)
    , m_distance(0.0f)
{
}

// ELF 0x34E900: three word copies.
AreaDistanceInfo::AreaDistanceInfo(const AreaDistanceInfo& other)
    : m_vehicleGroups(other.m_vehicleGroups)
    , m_area(other.m_area)
    , m_distance(other.m_distance)
{
}

// ELF 0x34E940: the arguments in field order.
AreaDistanceInfo::AreaDistanceInfo(int vehicleGroups, AIStrategicArea* area, float distance)
    : m_vehicleGroups(vehicleGroups)
    , m_area(area)
    , m_distance(distance)
{
}

} // namespace ai
} // namespace hfe
} // namespace dice
