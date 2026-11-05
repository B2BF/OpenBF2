// Airborne roaming recovered from libaidll.so.

// static init: ELF 0x29C090 (__tcf_0 0x29C0F0, _GLOBAL__I 0x29C120): <iostream>'s Init object only.
#include <iostream>

#include "BBPRoam3d.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAMoveTo3dObject.h"
#include "../AIBotActionPlan/BAPConObjectVisible.h"
#include "../AIBotActionPlan/BAPIWRoam.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x29BAC0 (C1; C2 0x29BA60)
BBPRoam3d::BBPRoam3d(int patternId)
{
    m_patternId = patternId;
    m_objects = new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()];
}

// ELF 0x29BB70 (D1; D2 0x29BB20, D0 0x29BBC0)
BBPRoam3d::~BBPRoam3d()
{
    if (m_objects)
        delete[] m_objects;
}

// ELF 0x29BC60
BotScript* BBPRoam3d::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
        {
            BotScript* previous = bot->getPlan();
            if (previous)
                delete previous;
            bot->setPlan(0);
        }
        return 0;
    }
    Information* self = informationManager->getInformation(bot->getCurrentEquipment());
    if (bot->hasPlan()
        && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId
        && static_cast<BAPStatement*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 5)
    {
        const unsigned int previousObject = static_cast<BAPIWRoam*>(bot->getPlan())->m_object;
        if (previousObject == m_objects[bot->getBotId()])
            return bot->getPlan();
    }
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    // shipped defect: a replaced plan is not deleted or cleared here.
    bot->getScriptContext()->clear();
    Information* target = informationManager->getInformation(m_objects[bot->getBotId()]);
    if (!target)
        return 0;
    bot->getSquadMember()->clearRequestAction();
    IPIMobile* mobile = self->getMobile();
    const float ownRadius = self->getPhysical()->getRadius();
    float radius = 0.0f;
    // shipped defect: the Boolean mobile flag is added to our radius only for this zero test.
    if (ownRadius + static_cast<float>(target->getMobile() != 0) != 0.0f)
        radius = target->getPhysical()->getRadius();
    radius += radius;
    BAPConObjectVisible* visible = new BAPConObjectVisible(m_objects[bot->getBotId()]);
    const unsigned long long driveControls = self->getControlInfo()->getDriveControls();
    const float arriveSpeed = mobile->getMaxSpeed() * 0.5f;
    const float speed = mobile->getMaxSpeed();
    const unsigned int object = m_objects[bot->getBotId()];
    BAPAMoveTo3dObject* move = new BAPAMoveTo3dObject(object, radius, speed, arriveSpeed, 50.0f,
                                                    visible, driveControls, m_patternId);
    BAPIWRoam* wrapper = new BAPIWRoam(m_objects[bot->getBotId()], move, m_patternId);
    wrapper->mpOwner = this;
    return wrapper;
}

// ELF 0x29C140
int BBPRoam3d::getRoamType()
{
    return 2;
}

// ELF 0x29BC10
void BBPRoam3d::init(unsigned int object, Bot* bot)
{
    m_objects[bot->getBotId()] = object;
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
