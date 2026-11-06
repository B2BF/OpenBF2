// One thing a bot has seen or heard: BotSenses keeps one record per handle (the map at +112) and
// threads it through the seen list (+56) and the heard list (+76).
// Evidence: libaidll.so 1.5.3153 (ia-32). Attested file AIBot/SensedObject.cpp (FILE symbol
// beside 0x1A29C0); constructors 0x1A2860 / 0x1A28F0, destructors 0x1A2980 / 0x1A2990 (empty,
// not virtual: every owner deletes through `~SensedObject(); operator delete`), initStrengths
// 0x1A29A0. Size 0x68 (`operator new(0x68)` in BotSenses::event_seenObject / event_heardObject).

#ifndef AI_AIDLL_AI_AIBOT_SENSEDOBJECT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SENSEDOBJECT_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class SensedObject
{
public:
    // The flag word (+0x4C) and the "last sense" word (+0x50) share these values: every writer
    // that sets a bit also stores the same value to +0x50. [INFERRED] names; the bits are attested.
    enum SenseFlag
    {
        SENSE_HEARD       = 0x01,   // in the heard list; updateHearing / event_heardObject
        SENSE_SEEN        = 0x02,   // in the seen list; event_seenObject sets, forgetObject clears
        SENSE_SIDED       = 0x04,   // the constructor's third argument (the object's side != 0)
        SENSE_INDIRECT    = 0x08,   // seen, but not directly yet (event_seenObject's `direct`)
        SENSE_HEARD_NOW   = 0x10,   // heard during the last hearing pass
        SENSE_FLAG40      = 0x40    // event_seenObject with its last argument false; meaning [UNRESOLVED]
    };

    // ELF 0x1A2860. The flag word is built as (seen ? SEEN : HEARD) | (sided ? SIDED : 0) and, for
    // a heard record, INDIRECT | HEARD_NOW. +0x10, +0x14, +0x34 and +0x44 are not written here.
    SensedObject(unsigned int handle, bool seen, bool sided);
    ~SensedObject();                    // ELF 0x1A2980: empty

    // ELF 0x1A29A0: a counted loop (8 passes) whose body the compiler removed; no caller in the image.
    void initStrengths();

    /* +0x00 */ unsigned int m_handle;
    /* +0x04 */ Vec3   m_seenPosition;        // event_seenObject's position argument
    /* +0x10 */ unsigned int m_seenPart;      // event_seenObject's third argument (-1 from event_projectile)
    /* +0x14 */ double m_firstSeenTime;       // event_seenObject, new record
    /* +0x1C */ Vec3   m_lastPosition;        // the information's position at the last sense
    /* +0x28 */ Vec3   m_lastSeenPosition;    // the information's position when last seen
    /* +0x34 */ double m_lastSensedTime;      // seen or heard
    /* +0x3C */ double m_lastAttackTime;      // ctor -1000.0; event_projectile: the game time
    // +0x44/+0x48: the fire patterns' per-target state. Writers: BBPFireInfantery::createPlan sets
    // both to 0 together with flag 0x20 (0x25CC43/0x25CC4D) and +0x48 = -100 when no weapon fits
    // (0x25F36E); BBPFire2d::createPlan sets flag 0x20 and +0x48 = -100 (0x24AF3A). Readers:
    // BBFire::calculateUrgency (0x208B58) and BBFireLargeBore::calculateUrgency (0x2118A8) skip the
    // target when flag 0x20 is set and +0x48 < 0 (signed: js). BAPANotifyBot::execute (ELF 0x1DD800)
    // adds its integer plan variable to +0x44 when its notification type is zero.
    // AICollisionHandler::shooterUpdate (0x382B70) increments +0x48 for every confirmed hit.
    // [CHOSEN, not recovered] names (ai-w1-e-fire, merged with D's m_unresolved44 use).
    /* +0x44 */ int    m_unresolved44;        // [UNRESOLVED] notification accumulator meaning
    /* +0x48 */ int    m_fireRating;          // < 0: no weapon can engage this target
    /* +0x4C */ unsigned int m_flags;         // SenseFlag bits
    /* +0x50 */ int    m_lastSense;           // ctor 2; the SenseFlag value last set
    /* +0x54 */ double m_lastSeenTime;        // ctor 0
    /* +0x5C */ double m_lastPositionTime;    // ctor 0; when +0x1C was written
    /* +0x64 */ bool   m_hasContact;          // Information::addContact was called for this record
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SENSEDOBJECT_H
