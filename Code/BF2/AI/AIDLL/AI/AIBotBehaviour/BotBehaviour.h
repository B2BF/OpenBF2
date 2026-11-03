// Behaviour base: per-bot urgency bookkeeping and plan generation through the behaviour's pattern.
// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOUR_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOUR_H

#include "UrgencyMerger.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;
class PlanGenerator;
class Information;
class UrgeCurve;
class UrgencyGenerator;

// Behaviour base. Vtable 0x4BB580 (13 words). One object per vehicle type and behaviour (built at
// map load by BotBehaviourManager), shared by every bot of that vehicle: the per-bot state is in
// arrays indexed by Bot::getBotId() (+0xE8).
// ELF: vtable 0x4BB580 (13 words), package A-core.
class BotBehaviour : public UrgencyGenerator
{
public:
    BotBehaviour(float*, UrgeCurve*, int);   // 0x225610
    BotBehaviour(float*, UrgeCurve*);   // 0x225A20
    virtual ~BotBehaviour();   // 0x225B00

    virtual float getUrgency(Bot*);   // 0x225D70, slot +0x08; from the base declaration
    virtual float getActiveUrgency(Bot*);   // 0x225DA0, slot +0x0C; from the base declaration
    virtual float updateActiveUrgency(Bot*);   // 0x225DD0, slot +0x10; from the base declaration
    virtual bool hasChangedTarget(Bot*);   // 0x225E20, slot +0x14; from the base declaration
    virtual bool quotientChanged(Bot*, float, float);   // 0x225E50, slot +0x18; from the base declaration
    virtual bool hasChanged(Bot*, float, float);   // 0x225F30, slot +0x1C; from the base declaration
    virtual UrgencyGenerator* getChosenGenerator();   // 0x1FD160, slot +0x20; from the base declaration
    virtual void reset(Bot*);   // 0x225F90, slot +0x24; from the base declaration
    virtual float calculateUrgency(Bot*, bool, float) = 0;   // no body, slot +0x28; from the base declaration
    virtual BotScript* generatePlan(Bot*);   // 0x225C30, slot +0x2C; from the base declaration
    virtual bool isAplicable(Information*, Bot*);   // 0x1FD170, slot +0x30; from the base declaration

    // BotMain::decisionMaking (0x1893D0) reads +4, +5 and +0x14 inline; the image has no accessor.
    // [CHOSEN, not recovered] inline getters so AIBot needs no friend access.
    unsigned char getIndex() const { return m_index; }
    unsigned char getQueueGroup() const { return m_queue; }
    float* getModifiers() const { return m_modifiers; }
    // [INFERRED] inline reader of +0x0C for BotMain::updateBot (ELF 0x18BF50 reads it inline).
    PlanGenerator* getPattern() const { return m_pattern; }
    // [INFERRED] inline reader of +0x20 for BotMemory::modifyBehaviourUrgency (ELF 0x18F2F8 reads it inline).
    UrgeCurve* getUrgeCurve() const { return m_urgeCurve; }

protected:
    // [TEMP] [CHOSEN, not recovered] S0 only: lets the generated derived constructor stubs compile;
    // it stops by name. [RETIRE-WHEN: ai-behaviour-skeleton-ctors] every derived constructor is recovered.
    BotBehaviour();

protected:
    // BotBehaviour(float*, UrgeCurve*, int count) 0x225250: +8 = +0xC = 0, the three arrays of
    // `count` (checkPointer each), activeUrgency[i] = 1e-5f, targetChanged[i] = true; urgency[] is
    // left uninitialised. BotBehaviour(float*, UrgeCurve*) 0x2259D0: all three arrays 0.
    // +0x04/+0x05 are bytes the BB* constructors store (behaviour index, queue group) [INFERRED].
    /* +0x04 */ unsigned char m_index;
    /* +0x05 */ unsigned char m_queue;
    // +0x08: per-bot handle array that only some behaviours allocate (BBTriggerable); reset 0x22601E
    // writes 0xFFFFFFFF when present. [INFERRED] name and element type.
    /* +0x08 */ unsigned int* m_targets;
    // +0x0C: the behaviour's pattern (BBCreateFunction<X> passes `new BBP<Y>(id)`), owned: the
    // destructor deletes it (0x225AC6). generatePlan calls isInitiated (+0xC) and createPlan (+8),
    // reset calls unInit (+0x10).
    /* +0x0C */ PlanGenerator* m_pattern;
    /* +0x10 */ float* m_urgency;               // [bot]: getUrgency 0x225D70
    /* +0x14 */ float* m_modifiers;             // constructor argument 1 (the behaviour-modifier row), not owned
    /* +0x18 */ bool* m_targetChanged;          // [bot]: hasChangedTarget 0x225E20 returns it [INFERRED] name
    /* +0x1C */ float* m_activeUrgency;         // [bot]: updateActiveUrgency 0x225DD0 copies urgency into it
    /* +0x20 */ UrgeCurve* m_urgeCurve;         // constructor argument 2, not owned
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOUR_H
