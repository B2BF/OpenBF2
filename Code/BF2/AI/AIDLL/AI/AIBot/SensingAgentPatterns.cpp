
#include "SensingAgentPatterns.h"
#include <iostream>
// Four original pattern TUs are combined here; each only initialized iostreams.
// static init: ELF 0x1C1780, 0x1C17E0, 0x1C1810 (MouseFreeLook)
// static init: ELF 0x1C1DD0, 0x1C1E30, 0x1C1E60 (Null)
// static init: ELF 0x1C2980, 0x1C29E0, 0x1C2A10 (Soldier)
// static init: ELF 0x1C31A0, 0x1C3200, 0x1C3230 (Turret)

#include "SensingAgent.h"
#include "BotMain.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPALookInDir.h"
#include "../AIBotActionPlan/BAPAMergeSoldierInputs.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

// The base owns the shared id field used by every pattern's getId/reset pair.
// Constructors and destructors perform no additional member initialization or cleanup.
ISensingAgentPattern::~ISensingAgentPattern()
{
}

SensingAgentTurretPattern::SensingAgentTurretPattern()
{
}

SensingAgentTurretPattern::~SensingAgentTurretPattern()
{
}

int SensingAgentTurretPattern::getId() const
{
    return m_id;
}

void SensingAgentTurretPattern::reset(int id)
{
    m_id = id;
}

SensingAgentMouseFreeLookPattern::SensingAgentMouseFreeLookPattern()
{
}

SensingAgentMouseFreeLookPattern::~SensingAgentMouseFreeLookPattern()
{
}

int SensingAgentMouseFreeLookPattern::getId() const
{
    return m_id;
}

void SensingAgentMouseFreeLookPattern::reset(int id)
{
    m_id = id;
}

SensingAgentSoldierPattern::SensingAgentSoldierPattern()
{
}

SensingAgentSoldierPattern::~SensingAgentSoldierPattern()
{
}

int SensingAgentSoldierPattern::getId() const
{
    return m_id;
}

void SensingAgentSoldierPattern::reset(int id)
{
    m_id = id;
}

SensingAgentNullPattern::SensingAgentNullPattern()
{
}

SensingAgentNullPattern::~SensingAgentNullPattern()
{
}

int SensingAgentNullPattern::getId() const
{
    return m_id;
}

void SensingAgentNullPattern::reset(int id)
{
    m_id = id;
}

// The patterns' shared prologue (inline in every createPlan but the soldier's, e.g. 0x1C2B10): the
// agent's plan is kept when this pattern built it (BAPStatement +0x10 == getId()); otherwise it is
// deleted and the agent's BAP context cleared. The function returns the kept plan or null.
#define SENSING_PATTERN_KEEP_OR_DROP(agent)                                                       \
    if (BotScript* const previous = (agent)->m_plan)                                               \
    {                                                                                              \
        if (static_cast<BAPStatement*>(previous)->getPatternId() == getId())                        \
            return previous;                                                                       \
        delete previous;                                                                           \
        (agent)->m_plan = 0;                                                                       \
    }                                                                                              \
    (agent)->m_bapContext->clear()

namespace {

// The bot's current equipment's control info, or null (Bot +0xD8, Information +0x30).
IPIControlInfo* currentControlInfo(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    return equipment ? equipment->getControlInfo() : 0;
}

// BAPATriggerContinously(PIMouseLook, true, false, false, mask 0x800, id, 0.0): the mouse-look
// button held for the plan's life (0x1C06F9..0x1C0727; 0x30 bytes).
BAPATriggerContinously* holdMouseLook(int id)
{
    BAPCondition* const third = checkPointer(new BAPConFalse());
    BAPCondition* const second = checkPointer(new BAPConFalse());
    BAPCondition* const first = checkPointer(new BAPConTrue());
    return checkPointer(new BAPATriggerContinously(io::PIMouseLook, first, second, third, 0x800ULL, id, 0.0));
}

} // namespace

// ELF 0x1C2B10: look in the direction, 5 degrees (0.08726646 rad) tolerance, never ending.
BotScript* SensingAgentTurretPattern::createPlan(SensingAgent* agent, Bot* bot, const Vec3& direction)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BotScript* plan = 0;
    if (IPIControlInfo* const control = currentControlInfo(bot))
    {
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookInDir* const look = checkPointer(
            new BAPALookInDir(direction, 0.08726646f, checkPointer(new BAPConFalse()), controls, id));
        look->m_owner = this;
        plan = look;
    }
    return plan;
}

// ELF 0x1C2E50: look at the object, as above.
BotScript* SensingAgentTurretPattern::createPlan(SensingAgent* agent, Bot* bot, unsigned int target)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BotScript* plan = 0;
    if (IPIControlInfo* const control = currentControlInfo(bot))
    {
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
            bot, target, 0.08726646f, checkPointer(new BAPConFalse()), controls, id, 0, 0.0f));
        look->m_owner = this;
        plan = look;
    }
    return plan;
}

// ELF 0x1C0560: hold the mouse-look button and look in the direction, in parallel.
BotScript* SensingAgentMouseFreeLookPattern::createPlan(SensingAgent* agent, Bot* bot, const Vec3& direction)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BotScript* plan = 0;
    if (IPIControlInfo* const control = currentControlInfo(bot))
    {
        BAPATriggerContinously* const trigger = holdMouseLook(getId());
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookInDir* const look = checkPointer(
            new BAPALookInDir(direction, 0.08726646f, checkPointer(new BAPConFalse()), controls, id));
        BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(getId(), -1, false));
        parallel->addStatement(trigger);
        parallel->addStatement(look);
        parallel->m_owner = this;
        plan = parallel;
    }
    return plan;
}

// ELF 0x1C0E60: as above, looking at the object.
BotScript* SensingAgentMouseFreeLookPattern::createPlan(SensingAgent* agent, Bot* bot, unsigned int target)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BotScript* plan = 0;
    if (IPIControlInfo* const control = currentControlInfo(bot))
    {
        BAPATriggerContinously* const trigger = holdMouseLook(getId());
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
            bot, target, 0.08726646f, checkPointer(new BAPConFalse()), controls, id, 0, 0.0f));
        BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(getId(), -1, false));
        parallel->addStatement(trigger);
        parallel->addStatement(look);
        parallel->m_owner = this;
        plan = parallel;
    }
    return plan;
}

// ELF 0x1C1F60. Both createPlan overloads pass a null `plan`, so the reuse arm (the agent's plan
// kept when it is this pattern's: false; otherwise `plan` deleted and the agent's cleared) never
// runs from them. True with the current equipment's control info; otherwise `plan` = 0.
bool SensingAgentSoldierPattern::initPlan(SensingAgent* agent, Bot* bot, IPIControlInfo*& control,
                                          BotScript*& plan)
{
    if (plan)
    {
        if (static_cast<BAPStatement*>(agent->m_plan)->getPatternId() == getId())
            return false;
        delete plan;
        agent->m_plan = 0;
    }
    agent->m_bapContext->clear();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (equipment)
    {
        control = equipment->getControlInfo();
        if (control)
            return true;
    }
    plan = 0;
    return false;
}

// ELF 0x1C2050: parallel(look, BAPAMergeSoldierInputs), owned by the pattern.
BotScript* SensingAgentSoldierPattern::endPlan(BAPALook* look, Bot* /*bot*/)
{
    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(getId(), -1, false));
    BAPAMergeSoldierInputs* const merge = checkPointer(new BAPAMergeSoldierInputs(getId()));
    parallel->addStatement(look);
    parallel->addStatement(merge);
    parallel->m_owner = this;
    return parallel;
}

// ELF 0x1C2310: look in the direction exactly (tolerance 0), never ending.
BotScript* SensingAgentSoldierPattern::createPlan(SensingAgent* agent, Bot* bot, const Vec3& direction)
{
    IPIControlInfo* control = 0;
    BotScript* plan = 0;
    if (initPlan(agent, bot, control, plan))
    {
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookInDir* const look = checkPointer(
            new BAPALookInDir(direction, 0.0f, checkPointer(new BAPConFalse()), controls, id));
        plan = endPlan(look, bot);
    }
    return plan;
}

// ELF 0x1C2640: look at the object (tolerance 0).
BotScript* SensingAgentSoldierPattern::createPlan(SensingAgent* agent, Bot* bot, unsigned int target)
{
    IPIControlInfo* control = 0;
    BotScript* plan = 0;
    if (initPlan(agent, bot, control, plan))
    {
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
            bot, target, 0.0f, checkPointer(new BAPConFalse()), controls, id, 0, 0.0f));
        plan = endPlan(look, bot);
    }
    return plan;
}

// ELF 0x1C1A70: BAPAResetControls(false, true, false, id) (0x1C bytes), owned by the pattern.
BotScript* SensingAgentNullPattern::createPlan(SensingAgent* agent, Bot* /*bot*/, const Vec3& /*direction*/)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BAPAResetControls* const reset = checkPointer(new BAPAResetControls(false, true, false, getId()));
    reset->m_owner = this;
    return reset;
}

// ELF 0x1C1C20: identical to the direction overload.
BotScript* SensingAgentNullPattern::createPlan(SensingAgent* agent, Bot* /*bot*/, unsigned int /*target*/)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BAPAResetControls* const reset = checkPointer(new BAPAResetControls(false, true, false, getId()));
    reset->m_owner = this;
    return reset;
}

#undef SENSING_PATTERN_KEEP_OR_DROP

} // namespace ai
} // namespace hfe
} // namespace dice
