// Cover-plan construction recovered from libaidll.so.

// static init: ELF 0x29FC30 (__tcf_0 0x29FC90, _GLOBAL__I 0x29FCC0): <iostream>'s Init object only.
#include <iostream>

#include "BBPTakeCover.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPALookInDir.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConEndOfPath.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPIWTakeCover.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x29D910 (C1; C2 0x29D2F0)
BBPTakeCover::BBPTakeCover(int patternId)
{
    const unsigned int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_patternId = patternId;
    m_coverObjects = checkPointer(new unsigned int[count]);
    m_coverPositions = checkPointer(new Vec3[count]());
    m_threatPositions = checkPointer(new Vec3[count]());
    m_threatObjects = checkPointer(new unsigned int[count]);
    m_useObjects = checkPointer(new bool[count]);
}

// ELF 0x29DFD0 (D1; D2 0x29DF30, D0 0x29E070)
BBPTakeCover::~BBPTakeCover()
{
    if (m_coverObjects)
        delete[] m_coverObjects;
    if (m_coverPositions)
        delete[] m_coverPositions;
    if (m_threatPositions)
        delete[] m_threatPositions;
    // shipped defect: m_threatObjects is leaked, and m_useObjects is deleted twice.
    if (m_useObjects)
        delete[] m_useObjects;
    if (m_useObjects)
        delete[] m_useObjects;
}

// ELF 0x29E330
BotScript* BBPTakeCover::createPlan(Bot* bot)
{
    if (!isInitiated(bot))
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

    const unsigned int coverObject = m_coverObjects[bot->getBotId()];
    const bool useObject = m_useObjects[bot->getBotId()];
    const Vec3 coverPosition = m_coverPositions[bot->getBotId()];
    Vec3 threatPosition = {};
    if (bot->hasPlan())
    {
        BAPStatement* previous = static_cast<BAPStatement*>(bot->getPlan());
        if (previous->getPatternId() == m_patternId && previous->getId() == BAPSTInfoWrapper)
        {
            BAPIWTakeCover* wrapper = static_cast<BAPIWTakeCover*>(previous);
            if (wrapper->getInfoType() == 7)
            {
                if (useObject)
                {
                    if (wrapper->m_useObject)
                    {
                        Information* target = informationManager->getInformation(m_threatObjects[bot->getBotId()]);
                        Information* oldTarget = informationManager->getInformation(wrapper->m_object);
                        if (target && oldTarget && target->getAIObject() == oldTarget->getAIObject())
                        {
                            const Vec3& position = oldTarget->getPosition();
                            const Vec3 difference = {wrapper->m_position.x - position.x,
                                                     wrapper->m_position.y - position.y,
                                                     wrapper->m_position.z - position.z};
                            if (difference.x * difference.x + difference.y * difference.y
                                + difference.z * difference.z < 1.0f)
                                return wrapper;
                        }
                    }
                }
                else if (!wrapper->m_useObject)
                {
                    const Vec3& position = m_threatPositions[bot->getBotId()];
                    const Vec3 difference = {wrapper->m_position.x - position.x,
                                             wrapper->m_position.y - position.y,
                                             wrapper->m_position.z - position.z};
                    if (difference.x * difference.x + difference.y * difference.y
                        + difference.z * difference.z < 9.0f)
                        return wrapper;
                }
            }
        }
    }
    if (bot->getPlan())
    {
        BotScript* previous = bot->getPlan();
        if (previous)
            delete previous;
        bot->setPlan(0);
    }
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    Information* self = informationManager->getInformation(bot->getCurrentEquipment());
    if (!self)
        return 0;
    IPIControlInfo* controls = self->getControlInfo();
    if (!controls)
        return 0;
    IPIMobile* mobile = self->getMobile();
    if (!mobile)
        return 0;
    Information* cover = informationManager->getInformation(coverObject);
    if (!cover || !cover->getPhysical())
        return 0;
    if (useObject)
    {
        Information* target = informationManager->getInformation(m_threatObjects[bot->getBotId()]);
        if (!target)
            return 0;
        threatPosition = target->getPosition();
    }
    else
        threatPosition = m_threatPositions[bot->getBotId()];

    bot->getSquadMember()->clearRequestAction();
    const Vec2 goal = {};
    Vec3 sensePosition = {};
    Vec3 senseNormal = {};
    Vec3 hint = {};
    static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->pickRandomSensePosition(sensePosition, senseNormal, hint, cover->getAIObject());
    Vec3 tracePosition = {};
    unsigned int polygon;
    const int equipmentType = self->getUnit()->getEquipmentType();
    const int vehicleNumber = mobile->getVehicleNumber();
    IAIPathfinding::instance->trace(vehicleNumber, sensePosition, coverPosition, tracePosition, polygon,
                                   equipmentType, -1, 0.0f, 0, 0);
    const float maximumSpeed = mobile->getMaxSpeed();
    const float radius = maximumSpeed < 4.0f ? 4.0f : maximumSpeed * 0.5f;
    BAPCombinerSerial* root = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    BAPAResetControls* reset = checkPointer(new BAPAResetControls(true, false, false, m_patternId));
    BAPCombinerParallel* parallel = checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));
    BAPConAnd* endCondition = checkPointer(new BAPConAnd());
    BAPConEndOfPath* endOfPath = checkPointer(new BAPConEndOfPath());
    BAPConPosition* atPosition = checkPointer(new BAPConPosition(goal, radius));
    endCondition->addCondition(endOfPath);
    endCondition->addCondition(atPosition);
    const unsigned long long driveControls = controls->getDriveControls();
    const float arriveSpeed = mobile->getMaxSpeed();
    const float speed = mobile->getMaxSpeed();
    // shipped defect: the trace output is discarded; the separate XZ goal remains (0, 0).
    const Vec3 destination = {goal.x, 0.0f, goal.y};
    BAPAMoveToFinding* move = checkPointer(new BAPAMoveToFinding(bot, destination, speed, arriveSpeed,
                                                               endCondition, driveControls, m_patternId, 0.0f, 0.0f));
    BAPStatement* look;
    if (useObject)
    {
        const unsigned long long lookControls = controls->getLookControls();
        BAPConFalse* condition = checkPointer(new BAPConFalse());
        const unsigned int target = m_threatObjects[bot->getBotId()];
        look = checkPointer(new BAPALookAtObject(bot, target, 0.314f, condition, lookControls, m_patternId, 0, 0.0f));
    }
    else
    {
        const unsigned long long lookControls = controls->getLookControls();
        BAPConFalse* condition = checkPointer(new BAPConFalse());
        const Vec3& position = self->getPosition();
        const Vec3 direction = {threatPosition.x - position.x, threatPosition.y - position.y,
                                threatPosition.z - position.z};
        look = checkPointer(new BAPALookInDir(direction, 0.314f, condition, lookControls, m_patternId));
    }
    parallel->addStatement(move);
    parallel->addStatement(look);
    root->addStatement(reset);
    root->addStatement(parallel);
    BAPIWTakeCover* wrapper;
    if (useObject)
    {
        const unsigned int target = m_threatObjects[bot->getBotId()];
        wrapper = checkPointer(new BAPIWTakeCover(target, root, m_patternId));
    }
    else
        wrapper = checkPointer(new BAPIWTakeCover(threatPosition, root, m_patternId));
    wrapper->mpOwner = this;
    return wrapper;
}

// ELF 0x29E120
void BBPTakeCover::init(Bot* bot, const Vec3& threat, const Vec3& position, unsigned int cover)
{
    m_coverObjects[bot->getBotId()] = cover;
    m_coverPositions[bot->getBotId()] = position;
    m_threatPositions[bot->getBotId()] = threat;
    m_useObjects[bot->getBotId()] = false;
    m_initiated[bot->getBotId()] = true;
}

// ELF 0x29E1E0
void BBPTakeCover::init(Bot* bot, unsigned int threat, const Vec3& position, unsigned int cover)
{
    m_coverObjects[bot->getBotId()] = cover;
    m_coverPositions[bot->getBotId()] = position;
    m_threatObjects[bot->getBotId()] = threat;
    m_useObjects[bot->getBotId()] = true;
    Information* target = informationManager->getInformation(m_threatObjects[bot->getBotId()]);
    if (target)
    {
        Vec3& destination = m_threatPositions[bot->getBotId()];
        destination = target->getPosition();
        m_initiated[bot->getBotId()] = true;
    }
    else
        m_initiated[bot->getBotId()] = false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
