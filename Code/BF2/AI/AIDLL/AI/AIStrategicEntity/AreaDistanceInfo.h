// One entry of an area's per-side distance list (AIStrategicArea::m_distances): a strategic
// area, its route distance from the owner, and the vehicle groups that reach it at that distance.
// [INFERRED] Source placement and field names: the ctors sit between AreaAssignment.cpp and
// SAWayPoint.cpp in libaidll.so (0x34E8A0..0x34E960) without a FILE symbol of their own.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_AREADISTANCEINFO_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_AREADISTANCEINFO_H

namespace dice {
namespace hfe {
namespace ai {

class AIStrategicArea;

// dice::hfe::ai::AreaDistanceInfo (namespace scope in the ELF mangling), twelve bytes.
struct AreaDistanceInfo
{
    AreaDistanceInfo();                                               // ELF 0x34E8A0
    AreaDistanceInfo(const AreaDistanceInfo& other);                  // ELF 0x34E900
    AreaDistanceInfo(int vehicleGroups, AIStrategicArea* area, float distance);  // ELF 0x34E940

    /* +0 */ int m_vehicleGroups;        // bit mask, 1 << group (insertUnitDistances)
    /* +4 */ AIStrategicArea* m_area;
    /* +8 */ float m_distance;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_AREADISTANCEINFO_H
