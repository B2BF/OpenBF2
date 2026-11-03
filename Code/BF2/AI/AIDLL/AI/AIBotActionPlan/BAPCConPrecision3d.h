// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCCONPRECISION3D_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCCONPRECISION3D_H

#include "BAPConnectedCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPAAimAt3d;
class Bot;

// ELF: vtable 0x4B95D0 (5 words), operator new 0x24, package G-vehicles.
// True when the aim's trajectory passes within `precision` of the target's predicted position and
// the aim deviates from the weapon's forward axis by at most atan(precision / 80). With
// waitForClosest it holds fire while the miss distance still shrinks and answers once it grows.
// Each hit stamps BotMain::setHadPrecisionTimeStamp.
class BAPCConPrecision3d : public BAPConnectedCondition
{
public:
    BAPCConPrecision3d(unsigned int, BAPAAimAt3d*, float, bool);   // 0x1EC650
    virtual ~BAPCConPrecision3d();   // 0x1EC6F0

    virtual bool evaluate(Bot*);   // 0x1EC750, slot +0x00
    virtual BAPConditionType getConditionType() const;   // 0x1ECA20, slot +0x04

private:
    /* +0x08 */ unsigned int m_object;           // checked for information only
    /* +0x0C */ float        m_precisionSq;
    /* +0x10 */ float        m_precision;
    /* +0x14 */ BAPAAimAt3d* m_aim;
    /* +0x18 */ bool         m_waitForClosest;   // [INFERRED] name; callers pass !weapon->getBurst()
    /* +0x1C */ float        m_bestDistanceSq;   // FLT_MAX: no approach yet
    /* +0x20 */ float        m_maxAngle;         // atanf(precision / 80)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCCONPRECISION3D_H
