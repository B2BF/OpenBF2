// Condition on the status of one strategic area for the asking side.
// Evidence: libaidll.so (ia-32) 0x3322B0..0x332870, vtable 0x4BF458.

#ifndef AI_AIDLL_AI_AISTRATEGIC_SCAREA_H
#define AI_AIDLL_AI_AISTRATEGIC_SCAREA_H

#include "StatusMap.h"
#include "StrategyCondition.h"

namespace dice {
namespace hfe {
namespace ai {

class SCArea : public StrategyCondition
{
public:
    // ELF 0x3322B0: base (name, 0.0f, Compare 9, Friendly, Crisp, Conditional 44) - both
    // out of range, so the base comparisons are never used.
    SCArea(const std::string& name, const std::string& areaName, int object,
           StatusMap::Status status);
    virtual ~SCArea();
    virtual int getType() const { return 3; }                          // ELF 0x332870 (weak)
    virtual float compare(float* friendly, float* enemy, int side) const;
    int updateArea();                                                  // ELF 0x332630

    /* +0x24 */ int m_object;                    // strategic-object index, -1 when unresolved
    /* +0x28 */ std::string m_areaName;
    /* +0x2C */ StatusMap::Status m_status;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_SCAREA_H
