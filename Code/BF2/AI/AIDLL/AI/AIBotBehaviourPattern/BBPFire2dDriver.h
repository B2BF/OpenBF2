// Fire pattern of artillery drivers and other ground vehicles aiming with their own turret.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE2DDRIVER_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE2DDRIVER_H

#include "../../../../Math/Vec3.h"
#include "BBPFire.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BBD60 (9 words), operator new 0x2C, package H-deferred.
class BBPFire2dDriver : public BBPFire
{
public:
    explicit BBPFire2dDriver(int);   // 0x24BBF0
    virtual ~BBPFire2dDriver();   // 0x24BDB0

    virtual BotScript* createPlan(Bot*);   // 0x24C100, slot +0x08; from the base declaration
    virtual void init(Bot*, unsigned int, int, bool, bool);   // 0x24BE50, slot +0x18; from the base declaration
    virtual void init(Bot*, unsigned int, const Vec3&, int);   // 0x24BF50, slot +0x1C; from the base declaration
    virtual int getImplementationType();   // 0x251110, slot +0x20; from the base declaration

    void init(Bot*, unsigned int, int, bool, bool, bool);   // 0x24BEB0; [INFERRED] ELF void

    // One flag per bot slot, allocated (not initialised) by the constructor; init's sixth
    // argument. [UNRESOLVED] meaning: createPlan does not read it.
    /* +0x28 */ bool* m_flags28;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE2DDRIVER_H
