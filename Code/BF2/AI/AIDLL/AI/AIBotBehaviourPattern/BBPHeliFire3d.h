// Fire pattern of helicopters: hover attacks against a target object, built by createPlan from the
// target and a weapon chosen independently of the distance.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPHELIFIRE3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPHELIFIRE3D_H

#include "../../../../Math/Vec3.h"
#include "BBPFire.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

class BAPStatement;

// ELF: vtable 0x4BC140 (9 words), operator new 0x28 (nothing beyond BBPFire), package G-vehicles.
class BBPHeliFire3d : public BBPFire
{
public:
    // The attack parameters createPlan hands to the plan builders; a stack object of createPlan
    // (0x3C bytes at -0x158, no constructor in the image). Names are [INFERRED] from createPlan (0x287830).
    struct FireData
    {
        /* +0x00 */ int   m_weapon;          // armament index from choseWeaponDistanceIndependent
        // 0: target without object type flag 0x40; 1: flag 0x40, direct weapon; 2: flag 0x40,
        // indirect weapon (createPlanInternal builds nothing for it); 3: no IPIMobile of its own.
        /* +0x04 */ int   m_mode;
        /* +0x08 */ float m_approachRadius;  // twice the own (plus the moving target's) physical radius
        /* +0x0C */ float m_precision;       // from the target's bounding box; not set in mode 3
        // Written by createPlan, read by neither builder.
        /* +0x10 */ Vec3  m_weaponDirection; // third row of the weapon's absolute transformation
        /* +0x1C */ float m_unused1C;        // [UNRESOLVED] never accessed
        /* +0x20 */ Vec3  m_position;        // fourth row of the own transformation
        /* +0x2C */ float m_unused2C;        // [UNRESOLVED] never accessed
        /* +0x30 */ Vec3  m_direction;       // third row of the own transformation
    };

    explicit BBPHeliFire3d(int);   // 0x287500
    virtual ~BBPHeliFire3d();   // 0x287580

    virtual BotScript* createPlan(Bot*);   // 0x287830, slot +0x08
    virtual void init(Bot*, unsigned int, int, bool, bool);   // 0x287600, slot +0x18
    virtual void init(Bot*, unsigned int, const Vec3&, int);   // 0x287680, slot +0x1C: refuses (Debug Error)
    virtual int getImplementationType();   // 0x28C190, slot +0x20: 4

    // createPlan hands createPlanInternal's statement to the BAPIWFire constructor; createPlanInternal
    // returns createMobileLessAttackPlan's root for mode 3 and puts it into a parallel otherwise.
    BAPStatement* createPlanInternal(Bot*, BBPHeliFire3d::FireData&);   // 0x288130
    BAPStatement* createMobileLessAttackPlan(Bot*, BBPHeliFire3d::FireData&);   // 0x28A7B0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPHELIFIRE3D_H
