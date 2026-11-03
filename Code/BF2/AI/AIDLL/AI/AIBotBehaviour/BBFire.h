// Fire behaviour base: urgency of engaging the bot's chosen target, weapon choice, and the target
// handed to the fire pattern (BBPFire) through initializeGenerator.
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRE_H

#include "../../../../Math/Vec3.h"
#include "BotBehaviour.h"

#include <map>

namespace dice {
namespace hfe {

namespace ai {

class BBPFire;
class Bot;
class BotMain;
class CustomCurve;
class UrgeCurve;

// ELF: vtable 0x4BAB00 (15 words), operator new 0x40, package D-behaviours.
class BBFire : public BotBehaviour
{
public:
    // Per-target feedback from the fire pattern, kept by target handle (new(8) in getBBPFeedback).
    struct BBPFeedback
    {
        BBPFeedback() : m_time(-10000.0) {}
        /* +0 */ double m_time;   // [UNRESOLVED] meaning; -10000 = never
    };

    BBFire(unsigned char index, unsigned char queue, BBPFire* pattern, UrgeCurve* curve, float* modifiers);   // 0x2079B0
    virtual ~BBFire();   // 0x207E80

    virtual float calculateUrgency(Bot*, bool, float);   // 0x208040, slot +0x28
    // Hands the bot's current target (BotBehaviour +0x08 [botId]) to the pattern and to the bot.
    virtual void initializeGenerator(BotMain*, int, bool, bool);   // 0x20A090, slot +0x34
    virtual void initializeGenerator(unsigned int target, const Vec3& position, BotMain*, int);   // 0x20A120, slot +0x38

    BBPFeedback* getBBPFeedback(unsigned int target);   // 0x20A180: find, or insert a new one
    int chooseWeapon(Bot*, unsigned int, float);   // 0x20A350; [INFERRED] ELF int, callers {'int': 1}

protected:
    /* +0x24 */ unsigned int* m_unknown24;     // per bot slot, constructed -1 [UNRESOLVED] meaning
    /* +0x28 */ double m_double28;             // constructed 0.0 [UNRESOLVED] meaning
    /* +0x30 */ std::map<unsigned int, BBPFeedback*> m_feedback;   // never freed (retail leak)
    /* +0x3C */ CustomCurve* m_curve;          // CustomCurve(modifiedSoftStepFunc, 0, 1, 0.03)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRE_H
