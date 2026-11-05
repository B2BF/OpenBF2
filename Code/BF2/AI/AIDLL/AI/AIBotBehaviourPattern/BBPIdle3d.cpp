// Idle pattern of aircraft (AI/AIDLL/AI/AIBotBehaviourPattern/BBPIdle3d.cpp in the image).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x28F420 (__tcf_0 0x28F480, _GLOBAL__I 0x28F4B0): <iostream>'s Init object only.
#include <iostream>

#include "BBPIdle3d.h"

#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/WaypointList.h"
#include "../AIBotActionPlan/BAPActionInline.h"
#include "../AIBotActionPlan/BAPAMoveTo3d.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPASendRadioMessage.h"
#include "../AIBotActionPlan/BAPAUseFlareOnMissileLock.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPIWIdle.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIPathFinding/AerialHeightMap/AerialHeightMap.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAISettings* settings()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
}

} // namespace

// ELF 0x28DCF0
BBPIdle3d::BBPIdle3d(int id)
    : BBPIdle(id)
{
    m_pickupPlayerId = checkPointer(new int[settings()->getMaxNBots()]);
    const int bots = settings()->getMaxNBots();
    m_altitude = checkPointer(new float[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_altitude[i] = 100.0f;
        m_pickupPlayerId[i] = -1;
    }
}

// ELF 0x28E020. Retail quirk: m_altitude is not released.
BBPIdle3d::~BBPIdle3d()
{
    if (m_pickupPlayerId)
        delete[] m_pickupPlayerId;
}

// ELF 0x28E120 (listing). The plan is a BAPIWIdle at the equipment's position around
// `serial(reset controls (no aim reset), [pickup radio message], parallel(reset controls, flares on
// missile lock, [move]))`. The bot counts as airborne when the bot reports it, or when the primary
// object has type flag 0x10 and the equipment moves faster than 3 m/s; an airborne bot gets the
// move: 500 m ahead along the flattened heading plus the right axis when the primary object lacks
// type flag 0x20 or has an armament, otherwise to the waypoint goal (or stays) at the aerial
// height map altitude plus 45 m. The pickup message (6/6/2, 1.0) is queued as in BBPIdle2d, but
// only when not airborne. An existing idle plan of this pattern is kept, re-enqueueing the radio
// message (6/6/3, 1.0) for a confirmed request when not airborne. The per-bot altitude that init
// stores (+0x0C) is not read here. The equipment and primary informations are not null-checked,
// as compiled.
// [PLATFORM] precision: the x87 build keeps the heading-plus-right sum and its product with 500 in
// extended precision before adding the position; the float expressions below round each step.
BotScript* BBPIdle3d::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
        {
            delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = equipment->getMobile();
    Information* const primary = informationManager->getInformation(equipment->getPrimaryObject(bot->getSide()));
    bool airborne = false;
    if (bot->isAirBorn() || (primary->isOfType(ITAir) && mobile->getSpeedMagnitude() > 3.0f))
        airborne = true;

    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 4)
    {
        const int pickupPlayerId = m_pickupPlayerId[bot->getBotId()];
        if (pickupPlayerId >= 0 && !airborne
            && bot->getCommunicationAgent()->confirmReceivedMessage(static_cast<AIRadioMessageType>(2),
                                                                    pickupPlayerId))
        {
            bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(6),
                                                              static_cast<AIRadioChannelType>(6), 3, 1.0f, -1);
        }
        return bot->getPlan();
    }

    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    bot->getSquadMember()->clearRequestAction();

    const unsigned long long controls = equipment->getControlInfo()->getDriveControls();
    const float maxSpeed = mobile->getMaxSpeed();
    BAPAResetControls* const reset = checkPointer(new BAPAResetControls(false, true, true, m_patternId));
    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    serial->addStatement(reset);

    const int pickupPlayerId = m_pickupPlayerId[bot->getBotId()];
    const bool anyPosition = pickupPlayerId == 0xFF;
    if (pickupPlayerId >= 0 && !primary->isOfType(ITSoldier))
    {
        IAIPlayer* const passenger =
            static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(pickupPlayerId);
        IAIObject* const passengerVehicle = passenger ? passenger->getCurrentVehicle() : 0;   // +0x54
        if (passengerVehicle)
        {
            const unsigned int equipmentHandle = bot->getCurrentEquipment();
            Information* const passengerInfo =
                informationManager->getInformation(passengerVehicle->getHandle(bot->getSide()));   // +0xD0
            if (passengerInfo)
            {
                const Mat4& own = primary->getTransformation();                                   // +0x3C
                const Mat4& other = passengerInfo->getTransformation();
                const float dx = own.m[12] - other.m[12];
                const float dy = own.m[13] - other.m[13];
                const float dz = own.m[14] - other.m[14];
                const float distanceSqr = dx * dx + dy * dy + dz * dz;
                if ((anyPosition || freePosition(equipmentHandle, bot->getSide()))
                    && 1600.0f > distanceSqr
                    && equipment->isPrimary()
                    && !airborne
                    && bot->getCommunicationAgent()->confirmReceivedMessage(static_cast<AIRadioMessageType>(2),
                                                                            pickupPlayerId))
                {
                    BAPASendRadioMessage* const message = checkPointer(new BAPASendRadioMessage(
                        static_cast<AIRadioMessageType>(6), static_cast<AIRadioChannelType>(6), 2, 1.0f,
                        m_patternId, -1));
                    serial->addStatement(message);
                }
            }
        }
    }

    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_patternId, true, -1, false));
    parallel->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_patternId)));
    // No checkPointer on this allocation in the image.
    parallel->addStatement(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, m_patternId));
    if (airborne)
    {
        Vec3 destination = equipment->getPosition();   // +0x30
        if (!primary->isOfType(ITHelicopter) || primary->getArmament())
        {
            const Mat4& heading = equipment->getTransformation();   // +0x3C
            Vec3 direction;
            direction.x = heading.m[8];
            direction.y = 0.0f;
            direction.z = heading.m[10];
            bapinline::normalize(direction);
            const Mat4& axes = equipment->getTransformation();
            direction.x = axes.m[0] + direction.x;
            direction.y = axes.m[1] + direction.y;
            direction.z = axes.m[2] + direction.z;
            direction.y = 0.0f;
            destination.x = direction.x * 500.0f + destination.x;
            destination.y = direction.y * 500.0f + destination.y;
            destination.z = direction.z * 500.0f + destination.z;
        }
        else
        {
            Vec3 goal;
            goal.x = goal.y = goal.z = 0.0f;
            if (bot->getWaypoints()->getGoalPoint(goal))
                destination = goal;
            destination.y = AerialHeightMap::m_instance->getAltitude(equipment->getPosition()) + 45.0f;
        }
        BAPAMoveTo3d* const move = checkPointer(new BAPAMoveTo3d(bot, destination, maxSpeed, maxSpeed,
                                                                 checkPointer(new BAPConFalse()), controls, 25.0f,
                                                                 m_patternId));
        parallel->addStatement(move);
    }
    serial->addStatement(parallel);

    BAPIWIdle* const idle = checkPointer(new BAPIWIdle(equipment->getPosition(), serial, m_patternId, bot));
    idle->setParent(this);   // +0x04 (BotScript::mpOwner)
    bot->getBotNavigation()->clearProgress();
    return idle;
}

// ELF 0x28F4D0
int BBPIdle3d::getType()
{
    return 2;
}

// ELF 0x28E0C0
void BBPIdle3d::init(float altitude, Bot* bot, int pickupPlayerId)
{
    m_altitude[bot->getBotId()] = altitude;
    m_initiated[bot->getBotId()] = true;
    m_pickupPlayerId[bot->getBotId()] = pickupPlayerId;
}

} // namespace ai
} // namespace hfe
} // namespace dice
