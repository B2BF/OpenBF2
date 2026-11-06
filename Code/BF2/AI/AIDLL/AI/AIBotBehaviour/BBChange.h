// Change behaviour (the "Change" slot of aibehaviours.ai): picks the vehicle or seat a bot should get
// into (or back out of) and hands it to the BBPChange pattern.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md. Virtual functions are in ELF vtable slot order (vtable check).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGE_H

#include <list>
#include <vector>
#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

struct AIRadioMessageInfo;
class Bot;
class CommunicationAgent;
class IAITerrain;
class Information;
class PlanGenerator;
class UrgeCurve;

// ELF: vtable 0x4BA860 (13 words), operator new 0x44, package F-infantry.
class BBChange : public BotBehaviour
{
public:
    BBChange(unsigned char, unsigned char, PlanGenerator*, UrgeCurve*, float*);   // 0x200240
    virtual ~BBChange();   // 0x200530

    virtual float calculateUrgency(Bot*, bool, float);   // 0x200770, slot +0x28; from the base declaration

    float checkBookings(Bot*);   // 0x2006A0; float in st0 (flds of the urgency slot)
    float modifyForDriver(Information*, bool);   // 0x202E20; float in st0 (1.0f / 0.77f / 0.5f)
    bool runwayClear(Information*, Information*, int) const;   // 0x202F00; bool: sete + movzbl
    float getRadioStrength(Information*, Bot*) const;   // 0x203210; float in st0
    bool isMannedByEnemy(unsigned int&, Information*, int&) const;   // 0x203240; bool byte
    bool isUpsideDown(Information*, IAITerrain*, float) const;   // 0x2032A0; bool: seta + movzbl
    bool isBailAllowed(Bot*) const;   // 0x2032E0; 0/1 in eax, both callers test %al
    bool isCallingVehicle(unsigned int, const std::list<AIRadioMessageInfo>&, int) const;   // 0x203460; 0/1, callers test %al
    void denyGetIn(Information*, unsigned int, CommunicationAgent*) const;   // 0x203610; ELF void
    bool botChoiceInVehicle(unsigned int, unsigned int, int) const;   // 0x203740; 0/1, the caller tests %al

private:
    // The constructors zero +0x24..+0x40 and the destructors free them (ELF 0x200480 / 0x200530 / 0x2005E0).
    // The base +0x08 (m_targets) is the per-bot chosen vehicle or seat (unsigned int[bot], -1 = none).
    /* +0x24 */ std::vector<unsigned int> m_vehicles;            // calculateUrgency: the getHardware list
    // runwayClear (a const member) resizes and refills it: [INFERRED] mutable.
    /* +0x30 */ mutable std::vector<unsigned int> m_runwayObstacles;
    // [UNRESOLVED] Nothing in this TU writes these except the constructors' 0; every destructor deletes
    // them through vtable slot +4 (the deleting destructor), +0x3C first. The pointee type is
    // [CHOSEN, not recovered]: UrgeCurve, a polymorphic type whose delete compiles to the same call.
    /* +0x3C */ UrgeCurve* m_unknown3C;
    /* +0x40 */ UrgeCurve* m_unknown40;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGE_H
