// Medic/support assist pattern: BBMedicAssist hands it the chosen target, the weapon index to use
// and the target's player per bot (init); createPlan builds the plan from them. Recovered from
// libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see docs/macos-ai-behaviour-plan.md.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPMEDICASSIST_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPMEDICASSIST_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;
class Information;

// ELF: vtable 0x4BC3C0 (6 words), operator new 0x18, package F-infantry.
class BBPMedicAssist : public BBPPattern
{
public:
    explicit BBPMedicAssist(int);   // 0x290DE0
    virtual ~BBPMedicAssist();   // 0x291220

    virtual BotScript* createPlan(Bot*);   // 0x291550, slot +0x08; from the base declaration

    // ELF void: no result register is set (0x291320..0x291389).
    void init(unsigned int, int, int, Bot*);   // 0x291320
    // ELF bool (movzbl of a 0/1 local); the same body as BBMedicAssist::isHumanInTarget.
    bool humanPlayerInTarget(Information*, int);   // 0x291390

private:
    /* +0x08 */ int m_id;                    // pattern id: createPlan tags every statement with it
    // Per bot (new[IAISettings::getMaxNBots], each -1), written by init.
    /* +0x0C */ unsigned int* m_target;      // information handle of the unit to treat
    /* +0x10 */ int* m_weaponIndex;          // index into the bot's IPIArmament::getWeapons
    /* +0x14 */ int* m_targetPlayerId;       // occupying player of the target, -1 if none
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPMEDICASSIST_H
