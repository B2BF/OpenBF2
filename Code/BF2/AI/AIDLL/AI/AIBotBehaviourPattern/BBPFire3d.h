// Fire pattern of aircraft (3d navigation): attack runs against a target object or a laser-designated
// point, built by createPlan from the target and the chosen weapon.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE3D_H

#include "../../../../Math/Vec3.h"
#include "BBPFire.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

class BAPStatement;

// ELF: vtable 0x4BBE00 (9 words), operator new 0x28 (nothing beyond BBPFire), package G-vehicles.
class BBPFire3d : public BBPFire
{
public:
    // The attack parameters createPlan hands to the plan builders; a stack object of createPlan
    // (no constructor in the image). Names are [INFERRED] from createPlan (0x2514E0).
    struct FireData
    {
        /* +0x00 */ int   m_unused00;    // [UNRESOLVED] neither written nor read by BBPFire3d
        // 0: target without object type flag 0x40; 1: flag 0x40, direct weapon; 2: flag 0x40,
        // indirect weapon, or a laser-designated point; 3: the own vehicle has no IPIMobile.
        /* +0x04 */ int   m_mode;
        /* +0x08 */ float m_approachRadius;  // twice the own (plus the moving target's) physical radius
        /* +0x0C */ float m_precision;   // weapon deviation, or the target's mean box size (at least 10)
        /* +0x10 */ Vec3  m_position;    // zero, or the laser-designated point
    };

    explicit BBPFire3d(int);   // 0x2512C0
    virtual ~BBPFire3d();   // 0x251340

    virtual BotScript* createPlan(Bot*);   // 0x2514E0, slot +0x08
    virtual void init(Bot*, unsigned int, int, bool, bool);   // 0x2513C0, slot +0x18
    virtual void init(Bot*, unsigned int, const Vec3&, int);   // 0x251440, slot +0x1C
    virtual int getImplementationType();   // 0x25BA20, slot +0x20: 4

    // The three builders return the statement createPlan puts into its parallel block
    // (`movl %eax,-0x18c(%ebp)` after each call, 0x2517CC).
    BAPStatement* createPlanInternal(Bot*, BBPFire3d::FireData&);   // 0x252260
    BAPStatement* createMobileLessAttackPlan(Bot*, BBPFire3d::FireData&);   // 0x2577D0
    BAPStatement* createPlanLaserGuided(Bot*, BBPFire3d::FireData&);   // 0x25A260
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE3D_H
