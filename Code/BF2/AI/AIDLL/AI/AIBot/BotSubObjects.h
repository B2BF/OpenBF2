// Aim-correction state used by BotMain.

#ifndef AI_AIDLL_AI_AIBOT_BOTSUBOBJECTS_H
#define AI_AIDLL_AI_AIBOT_BOTSUBOBJECTS_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class BotMain;

// Aim-correction state; construction leaves the first four bytes uninitialized.
class FireCorrectionData
{
public:
    FireCorrectionData();                       // for the class
    // ELF 0x1A2170: +4/+5/+6 set, +72 = -1.0, +44 zeroed, +80 = the target's world object.
    void newTarget(unsigned int handle);

    // BotMain::planExecution (ELF 0x188B90) writes these inline once the collision handler
    // reports that the bot's projectile passed its point: the pre/post/target points and the
    // maximum height from IBFCollisionHandler, then sets +5, +84 and +85.
    /*  0 */ int  m_weapon;                     // not initialised by the ctor; event_firing's int (the weapon) [INFERRED]
    /*  4 */ bool m_flag4;                      // = true
    /*  5 */ bool m_flag5;                      // = true; planExecution: true
    /*  6 */ bool m_flag6;                      // not initialised by the ctor; event_firing: a correction shot is out
    /*  7 */ char m_pad7[1];
    /*  8 */ float m_firedTarget[3];            // zeroed; event_firing's target (a Vec3)
    // +20..+64 zeroed by the ctor. event_shotMissed (ELF 0x18B230) stores the miss at +0x14;
    // newTarget (0x1A2170) zeroes +0x2C; EntryMouseTurretAimAt (0x2B9A5C) writes +0x20 (the aim
    // direction) and +0x38 (the predicted target position). [INFERRED] names.
    /* 20 */ Vec3 m_missedPosition;
    /* 32 */ Vec3 m_aimDirection;
    /* 44 */ Vec3 m_vec44;                      // [UNRESOLVED] name
    /* 56 */ Vec3 m_predictedTarget;
    /* 68 */ char m_gap68[4];                   // [UNRESOLVED] - the ctor skips +68..+83
    /* 72 */ double m_double72;                 // event_firing / newTarget: -1.0; event_shotMissed: the miss time
    /* 80 */ const void* m_shotTarget;          // event_shotHit: a hit on this object confirms the shot [INFERRED]
    /* 84 */ bool m_flag84;                     // = false; planExecution: true
    /* 85 */ bool m_flag85;                     // planExecution: true
    /* 86 */ char m_pad86[2];
    /* 88 */ Vec3 m_projectilePrePoint;         // = 0
    /* 100 */ Vec3 m_projectilePostPoint;       // = 0
    /* 112 */ Vec3 m_projectileTargetPoint;     // = 0
    /* 124 */ float m_projectileMaxHeight;      // not initialised by the ctor
};


} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTSUBOBJECTS_H
