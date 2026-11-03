// Aiming condition restricted to the camera's yaw range.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVALIDYAWAIMING_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVALIDYAWAIMING_H

#include "../../../../Math/Vec3.h"
#include "BAPConObjectValidAiming.h"
#include "BAPTypes.h"
#include "BAPVariable.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9A08 (5 words), operator new 0x88, package H-deferred.
// No members of its own.
class BAPConObjectValidYawAiming : public BAPConObjectValidAiming
{
public:
    BAPConObjectValidYawAiming(unsigned int, BAPVariable<bool>*, BAPVariable<Vec3>*);   // 0x1F1640
    BAPConObjectValidYawAiming(unsigned int, unsigned int, BAPVariable<bool>*, BAPVariable<Vec3>*);   // 0x1F1700
    virtual ~BAPConObjectValidYawAiming();   // 0x1F18E0

    virtual bool evaluate(Bot*);   // 0x1F1760, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F1820, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVALIDYAWAIMING_H
