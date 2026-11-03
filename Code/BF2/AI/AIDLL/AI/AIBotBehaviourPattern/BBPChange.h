// Change pattern: per-bot target vehicle/seat, entry position and get-in flags that BBChange's urgency
// fills (init / setGetInConfirm) and createPlan consumes.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md. Virtual functions are in ELF vtable slot order (vtable check).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGE_H

#include "../../../../Math/Vec3.h"
#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;
class Information;

// ELF: vtable 0x4BBA00 (6 words), operator new 0x1C, package F-infantry.
class BBPChange : public BBPPattern
{
public:
    explicit BBPChange(int);   // 0x235CB0
    virtual ~BBPChange();   // 0x236210

    virtual BotScript* createPlan(Bot*);   // 0x236A20, slot +0x08; from the base declaration

    void init(unsigned int, const Vec3&, bool, bool, Bot*);   // 0x236340; ELF void (no result register)
    void init(unsigned int, bool, bool, Bot*);   // 0x2363F0; ELF void
    void setGetInConfirm(bool, Bot*);   // 0x236470; ELF void
    bool findValidNavPosToAir(int, int, Vec3, Vec3, Vec2&);   // 0x2364A0; [INFERRED] ELF bool, callers {'bool': 2}
    bool getPosAroundAircraft(Information*, int, int, Vec3&);   // 0x236540; [INFERRED] ELF int, callers {'bool': 1}

private:
    // Every array is new[]'d with IAISettings::getMaxNBots (slot 0) entries and indexed by Bot::getBotId.
    /* +0x08 */ int m_patternId;                // the constructor argument: the pattern id the plan nodes carry
    /* +0x0C */ unsigned int* m_targets;        // init: the vehicle/seat handle
    /* +0x10 */ Vec3* m_entryPositions;         // value-initialised (0,0,0); init(handle, pos, ...) only
    /* +0x14 */ bool* m_sameVehicle;            // [INFERRED] name: init's first bool, BBChange passes "same primary object"
    /* +0x18 */ bool* m_getInConfirm;           // init's second bool and setGetInConfirm (named after the setter)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGE_H
