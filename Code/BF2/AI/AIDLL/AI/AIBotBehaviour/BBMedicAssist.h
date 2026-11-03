// Medic/support assist behaviour (infantry): a medic (kit 5) or engineer (kit 4) picks the most
// urgent friendly unit near it to heal or repair; a support soldier (kit 3) instead looks for a
// friendly unit that needs ammunition (calculateGiveAmmoUrgency). Recovered from libaidll.so
// (Linux 1.5.3153, ia-32); F-urgency package, see docs/macos-ai-behaviour-plan.md.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBMEDICASSIST_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBMEDICASSIST_H

#include "BotBehaviour.h"

#include <vector>

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class CommunicationAgent;
class Information;
class PlanGenerator;
class UrgeCurve;

// ELF: vtable 0x4BAF20 (13 words), operator new 0x3C, package F-infantry.
class BBMedicAssist : public BotBehaviour
{
public:
    BBMedicAssist(unsigned char, unsigned char, PlanGenerator*, UrgeCurve*, float*);   // 0x216220
    virtual ~BBMedicAssist();   // 0x216810

    virtual float calculateUrgency(Bot*, bool, float);   // 0x216B40, slot +0x28; from the base declaration
    virtual bool isAplicable(Information*, Bot*);   // 0x217E70, slot +0x30; bool: 0/1 in %edx

    // ELF bool (movzbl of a 0/1 local); true when a human (non-AI) player occupies the
    // information's primary object or one of its secondary objects for that side.
    bool isHumanInTarget(Information*, int);   // 0x216980
    // ELF returns 0/1 in %eax (movl $0x1, %edx); every caller tests the low byte, so bool.
    bool vehicleRequestsEngineer(Information*, int, CommunicationAgent*) const;   // 0x217F10
    // ELF float: both returns leave the urgency on the x87 stack (flds).
    float calculateGiveAmmoUrgency(Bot*, float);   // 0x218020

private:
    // The friendly units of the last calculation, filled by IAIInformationEnvironment
    // getFriendlyAndNeutralUnits / getFriendlyUnits (75 m around the bot).
    /* +0x24 */ std::vector<unsigned int> m_friendlies;
    // Three per-battle-strength-type arrays (new[IAISettings::getNBattleStrengths]); for each
    // strength type the bot's own healing/repair (weapon type 1) or ammo (type 2) weapon with the
    // highest strength against it: its index in IPIArmament::getWeapons, its max range, its strength.
    /* +0x30 */ int*   m_bestWeaponIndex;      // -1 = none
    /* +0x34 */ float* m_bestWeaponRange;      // WeaponInfo::getMaxRange
    /* +0x38 */ int*   m_bestWeaponStrength;   // WeaponInfo::getStrength(type)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBMEDICASSIST_H
