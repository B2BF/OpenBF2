// Builds approach and firing plans for ground-vehicle weapons.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE2D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE2D_H

#include "../../../../Math/Vec3.h"
#include "BBPFire.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BBC80 (9 words), operator new 0x2C, package G-vehicles.
class BBPFire2d : public BBPFire
{
public:
    explicit BBPFire2d(int);   // 0x245140
    virtual ~BBPFire2d();   // 0x245300

    virtual BotScript* createPlan(Bot*);   // 0x245650, slot +0x08; from the base declaration
    virtual void init(Bot*, unsigned int, int, bool, bool);   // 0x2453A0, slot +0x18; from the base declaration
    virtual void init(Bot*, unsigned int, const Vec3&, int);   // 0x2454A0, slot +0x1C; from the base declaration
    virtual int getImplementationType();   // 0x24B6D0, slot +0x20; from the base declaration

    void init(Bot*, unsigned int, int, bool, bool, bool);   // 0x245400; [INFERRED] ELF void

    /* +0x28 */ bool* m_perBotFlag;   // per bot: the last bool of init 0x245400 [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE2D_H
