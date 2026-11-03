// The sensing agent's plan patterns (libaidll.so 0x1C0490..0x1C3250). [CHOSEN, not recovered]: the
// four classes stay grouped in one header and one source file.

#ifndef AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNS_H
#define AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNS_H

#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

class BAPALook;
class Bot;
class BotScript;
class IPIControlInfo;
class SensingAgent;

class ISensingAgentPattern
{
public:
    ISensingAgentPattern() {}
    virtual ~ISensingAgentPattern();

    // Preserve virtual order: reset precedes the other pattern operations.
    virtual void reset(int) = 0;                                        // +0x08
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&) = 0;    // +0x0C
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int) = 0;   // +0x10
    virtual int getId() const = 0;                                      // +0x14

    // Uninitialised by construction - see the block above.
    /* +4 */ int m_id;
};

// The turret aimer. createPlan 821 / .
class SensingAgentTurretPattern : public ISensingAgentPattern
{
public:
    SensingAgentTurretPattern();
    virtual ~SensingAgentTurretPattern();

    // Preserve virtual order: reset occupies the first slot.
    virtual void reset(int);
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&);
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int);
    virtual int getId() const;
};

// The mouse-look aimer, and the heaviest of the four: createPlan 2,293 / .
class SensingAgentMouseFreeLookPattern : public ISensingAgentPattern
{
public:
    SensingAgentMouseFreeLookPattern();
    virtual ~SensingAgentMouseFreeLookPattern();

    // Preserve virtual order: reset occupies the first slot.
    virtual void reset(int);
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&);
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int);
    virtual int getId() const;
};

// The on-foot aimer - the only one of the four with helpers of its own.
class SensingAgentSoldierPattern : public ISensingAgentPattern
{
public:
    SensingAgentSoldierPattern();
    virtual ~SensingAgentSoldierPattern();

    // Preserve virtual order: reset occupies the first slot.
    virtual void reset(int);
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&);
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int);
    virtual int getId() const;

    // Unique to this pattern and virtual (vtable 0x4B7640 slots +0x18 / +0x1C): they bracket the
    // plan it builds. initPlan (ELF 0x1C1F60) finds the control info; endPlan (0x1C2050) wraps the
    // look in a parallel combiner with BAPAMergeSoldierInputs.
    virtual bool initPlan(SensingAgent *, Bot *, IPIControlInfo *&, BotScript *&);
    virtual BotScript* endPlan(BAPALook *, Bot *);
};

// The null object: both createPlan overloads build (or keep) a BAPAResetControls.
class SensingAgentNullPattern : public ISensingAgentPattern
{
public:
    SensingAgentNullPattern();
    virtual ~SensingAgentNullPattern();

    // Preserve virtual order: reset occupies the first slot.
    virtual void reset(int);
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&);
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int);
    virtual int getId() const;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNS_H
