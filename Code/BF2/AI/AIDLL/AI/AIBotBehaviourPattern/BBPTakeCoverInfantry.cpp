// Infantry cover-plan construction recovered from libaidll.so.

// static init: ELF 0x2A68C0 (__tcf_0 0x2A6920, _GLOBAL__I 0x2A6950): <iostream>'s Init object only.
#include <iostream>

#include "BBPTakeCoverInfantry.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPActionInline.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPASetSensingAgentFocus.h"
#include "../AIBotActionPlan/BAPASoldierPose.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConEndOfPath.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConLineOfFire.h"
#include "../AIBotActionPlan/BAPConObjectLineOfFire.h"
#include "../AIBotActionPlan/BAPConOr.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPIWTakeCover.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPICover.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../../Math/Mat4.h"
#include <cfloat>
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// [CHOSEN, not recovered] Packaging of the inline transforms in ELF 0x2A1B50.
Vec3 transformPoint(const Mat4& m, const Vec3& point)
{
    const Vec3 result = {point.x * m.m[0] + point.y * m.m[4] + point.z * m.m[8] + m.m[12],
                         point.x * m.m[1] + point.y * m.m[5] + point.z * m.m[9] + m.m[13],
                         point.x * m.m[2] + point.y * m.m[6] + point.z * m.m[10] + m.m[14]};
    return result;
}

Vec3 transformDirection(const Mat4& m, const Vec3& direction)
{
    const Vec3 result = {direction.x * m.m[0] + direction.y * m.m[4] + direction.z * m.m[8],
                         direction.x * m.m[1] + direction.y * m.m[5] + direction.z * m.m[9],
                         direction.x * m.m[2] + direction.y * m.m[6] + direction.z * m.m[10]};
    return result;
}

} // namespace

// ELF 0x2A1110 (C1; C2 0x2A0AF0)
BBPTakeCoverInfantry::BBPTakeCoverInfantry(int patternId)
{
    const unsigned int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_patternId = patternId;
    m_coverObjects = checkPointer(new unsigned int[count]);
    m_coverPositions = checkPointer(new Vec3[count]());
    m_threatPositions = checkPointer(new Vec3[count]());
    m_threatObjects = checkPointer(new unsigned int[count]);
    m_useObjects = checkPointer(new bool[count]);
}

// ELF 0x2A17E0 (D1; D2 0x2A1730, D0 0x2A1890)
BBPTakeCoverInfantry::~BBPTakeCoverInfantry()
{
    if (m_coverObjects)
        delete[] m_coverObjects;
    if (m_coverPositions)
        delete[] m_coverPositions;
    if (m_threatPositions)
        delete[] m_threatPositions;
    if (m_threatObjects)
        delete[] m_threatObjects;
    if (m_useObjects)
        delete[] m_useObjects;
}

// ELF 0x2A1B50
BotScript* BBPTakeCoverInfantry::createPlan(Bot* bot)
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
    IAIPathfinding* pathfinding = IAIPathfinding::instance;
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
                            const Vec3 difference = bapinline::sub(wrapper->m_position, oldTarget->getPosition());
                            if (bapinline::lengthSqr(difference) < 1.0f)
                                return wrapper;
                        }
                    }
                }
                else if (!wrapper->m_useObject)
                {
                    const Vec3 difference = bapinline::sub(wrapper->m_position, m_threatPositions[bot->getBotId()]);
                    if (bapinline::lengthSqr(difference) < 9.0f)
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
    bot->getSquadMember()->clearRequestAction();
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
    IPICover* coverInfo = 0;
    if (cover)
    {
        if (!cover->getPhysical())
            return 0;
        coverInfo = cover->getCover();
        if (!coverInfo)
            return 0;
    }
    if (useObject)
    {
        Information* target = informationManager->getInformation(m_threatObjects[bot->getBotId()]);
        if (!target)
            return 0;
        threatPosition = target->getPosition();
    }
    else
        threatPosition = m_threatPositions[bot->getBotId()];

    BAPCombinerSerial* root;
    if (!cover)
    {
        Vec3 direction = bapinline::sub(threatPosition, self->getPosition());
        const Vec3 destination = coverPosition;
        root = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        BAPASetSensingAgentFocus* focus;
        if (!m_useObjects[bot->getBotId()])
        {
            bapinline::normalize(direction);
            focus = checkPointer(new BAPASetSensingAgentFocus(0.5f, direction, m_patternId, false));
        }
        else
        {
            const unsigned int target = m_threatObjects[bot->getBotId()];
            focus = checkPointer(new BAPASetSensingAgentFocus(0.5f, target, m_patternId, false));
        }
        const unsigned long long driveControls = controls->getDriveControls();
        BAPConPosition* atPosition = checkPointer(new BAPConPosition(destination, 4.0f));
        const float arriveSpeed = mobile->getMaxSpeed();
        const float speed = mobile->getMaxSpeed() + 1.0f;
        BAPAMoveToFinding* move = checkPointer(new BAPAMoveToFinding(bot, destination, speed, arriveSpeed,
                                                                   atPosition, driveControls, m_patternId, 0.0f, 0.0f));
        BAPAResetControls* reset = checkPointer(new BAPAResetControls(true, false, false, m_patternId));
        BAPASoldierPose* lie = checkPointer(new BAPASoldierPose(SPLying, checkPointer(new BAPConTrue()), m_patternId, false));
        root->addStatement(move);
        root->addStatement(reset);
        root->addStatement(lie);
        root->addStatement(focus);
    }
    else
    {
        Vec2 goal = {};
        const int equipmentType = self->getUnit()->getEquipmentType();
        if (coverInfo->getPortalCount() == 0)
        {
            Vec3 sensePosition = {};
            Vec3 senseNormal = {};
            Vec3 hint = {};
            static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
                ->pickRandomSensePosition(sensePosition, senseNormal, hint, cover->getAIObject());
            Vec3 tracePosition = {};
            unsigned int polygon;
            const int vehicleNumber = mobile->getVehicleNumber();
            pathfinding->trace(vehicleNumber, coverPosition, sensePosition, tracePosition, polygon,
                                equipmentType, -1, 0.0f, 0, 0);
            vec3ToVec2XZ(tracePosition, goal);
        }
        else
        {
            const Mat4& world = cover->getTransformation();
            Mat4 inverse;
            inverse.setInverse(world);
            const Vec3 localThreat = transformPoint(inverse, threatPosition);
            unsigned int selected = 0;
            float bestDot = -FLT_MAX;
            unsigned int portal;
            for (portal = 0; portal < coverInfo->getPortalCount(); ++portal)
            {
                Vec3 direction = bapinline::sub(localThreat, coverInfo->getPortalCenter(portal));
                bapinline::normalize(direction);
                const float dot = bapinline::dot(direction, coverInfo->getPortalNormal(portal));
                if (dot > bestDot)
                {
                    selected = portal;
                    bestDot = dot;
                }
            }
            const Vec3& centre = coverInfo->getPortalCenter(selected);
            const Vec3& normal = coverInfo->getPortalNormal(selected);
            const Vec3 worldCentre = transformPoint(world, centre);
            const Vec3 worldNormal = transformDirection(world, normal);
            const Vec3 offset = {worldNormal.x * -2.0f, worldNormal.y * -2.0f, worldNormal.z * -2.0f};
            Vec3 closest = {};
            unsigned int polygon;
            unsigned int island;
            const int vehicleNumber = mobile->getVehicleNumber();
            if (pathfinding->findClosestValidPolygon(polygon, island, closest, vehicleNumber,
                                                     worldCentre, offset, 10, equipmentType))
            {
                const Vec3 tangent = {worldNormal.z, 0.0f, -worldNormal.x};
                const float side = bapinline::dot(tangent, threatPosition) - bapinline::dot(tangent, worldCentre);
                const Vec3 step = {tangent.x * 2.0f, tangent.y * 2.0f, tangent.z * 2.0f};
                // shipped defect: the extent query uses the exhausted loop index, not `selected`.
                const float extent = side > 0.0f ? coverInfo->getPortalXZExtentR(portal)
                                                 : coverInfo->getPortalXZExtentL(portal);
                const Vec3 from = {step.x * extent + worldCentre.x, step.y * extent + worldCentre.y,
                                    step.z * extent + worldCentre.z};
                Vec3 tracePosition = {};
                unsigned int tracePolygon;
                const int traceVehicle = mobile->getVehicleNumber();
                pathfinding->trace(traceVehicle, from, closest, tracePosition, tracePolygon,
                                    equipmentType, -1, 0.0f, 0, 0);
                vec3ToVec2XZ(tracePosition, goal);
            }
        }
        const float maximumSpeed = mobile->getMaxSpeed();
        const float radius = maximumSpeed < 4.0f ? 4.0f : maximumSpeed * 0.5f;
        (void)std::sqrt(bapinline::lengthSqr(bapinline::sub(threatPosition, self->getPosition())));
        root = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        BAPASetSensingAgentFocus* focus;
        if (!m_useObjects[bot->getBotId()])
        {
            Vec3 direction = bapinline::sub(threatPosition, self->getPosition());
            bapinline::normalize(direction);
            focus = checkPointer(new BAPASetSensingAgentFocus(0.5f, direction, m_patternId, false));
        }
        else
        {
            const unsigned int target = m_threatObjects[bot->getBotId()];
            focus = checkPointer(new BAPASetSensingAgentFocus(0.5f, target, m_patternId, false));
        }
        root->addStatement(focus);
        BAPAResetControls* reset = checkPointer(new BAPAResetControls(true, false, true, m_patternId));
        BAPCombinerParallel* moving = checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));
        BAPConFalse* poseBreak = checkPointer(new BAPConFalse());
        BAPConTrue* poseRun = checkPointer(new BAPConTrue());
        BAPASoldierPose* pose = checkPointer(new BAPASoldierPose(SPStanding, SPLying, 5, SPCrouching,
                                                               0.0f, poseRun, poseBreak, m_patternId, false));
        BAPConOr* endCondition = checkPointer(new BAPConOr());
        BAPConFalse* never = new BAPConFalse();
        BAPConAnd* arrived = checkPointer(new BAPConAnd());
        BAPConEndOfPath* endOfPath = checkPointer(new BAPConEndOfPath());
        BAPConPosition* atPosition = checkPointer(new BAPConPosition(goal, radius));
        arrived->addCondition(endOfPath);
        arrived->addCondition(atPosition);
        endCondition->addCondition(never);
        endCondition->addCondition(arrived);
        const unsigned long long driveControls = controls->getDriveControls();
        const float arriveSpeed = mobile->getMaxSpeed();
        const float speed = mobile->getMaxSpeed();
        const Vec3 destination = {goal.x, 0.0f, goal.y};
        BAPAMoveToFinding* move = checkPointer(new BAPAMoveToFinding(bot, destination, speed, arriveSpeed,
                                                                   endCondition, driveControls, m_patternId, 0.0f, 0.0f));
        moving->addStatement(pose);
        moving->addStatement(move);
        BAPCombinerParallel* posing = checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));
        unsigned int target = 0;
        BAPCondition* standingLine;
        if (useObject)
        {
            target = m_threatObjects[bot->getBotId()];
            standingLine = checkPointer(new BAPConObjectLineOfFire(target, false, SPStanding));
        }
        else
            standingLine = checkPointer(new BAPConLineOfFire(threatPosition, false, SPStanding));
        BAPASoldierPose* stand = checkPointer(new BAPASoldierPose(SPStanding, checkPointer(new BAPConTrue()), m_patternId, false));
        BAPCondition* crouchingLine;
        if (useObject)
            crouchingLine = checkPointer(new BAPConObjectLineOfFire(target, false, SPCrouching));
        else
            crouchingLine = checkPointer(new BAPConLineOfFire(threatPosition, false, SPCrouching));
        BAPASoldierPose* crouch = checkPointer(new BAPASoldierPose(SPCrouching, checkPointer(new BAPConTrue()), m_patternId, false));
        BAPASoldierPose* lie = checkPointer(new BAPASoldierPose(SPLying, checkPointer(new BAPConTrue()), m_patternId, false));
        BAPFlowCIf* lowerPose = checkPointer(new BAPFlowCIf(crouchingLine, lie, crouch, m_patternId, false, false));
        BAPFlowCIf* selectPose = checkPointer(new BAPFlowCIf(standingLine, lowerPose, stand, m_patternId, false, false));
        posing->addStatement(selectPose);
        root->addStatement(reset);
        root->addStatement(moving);
        root->addStatement(posing);
    }
    BAPIWTakeCover* wrapper;
    if (useObject)
    {
        const unsigned int target = m_threatObjects[bot->getBotId()];
        wrapper = checkPointer(new BAPIWTakeCover(target, root, m_patternId));
    }
    else
        wrapper = checkPointer(new BAPIWTakeCover(threatPosition, root, m_patternId));
    wrapper->m_owner = this;
    return wrapper;
}

// ELF 0x2A1940
void BBPTakeCoverInfantry::init(Bot* bot, const Vec3& threat, const Vec3& position, unsigned int cover)
{
    m_coverObjects[bot->getBotId()] = cover;
    m_coverPositions[bot->getBotId()] = position;
    m_threatPositions[bot->getBotId()] = threat;
    m_useObjects[bot->getBotId()] = false;
    m_initiated[bot->getBotId()] = true;
}

// ELF 0x2A1A00
void BBPTakeCoverInfantry::init(Bot* bot, unsigned int threat, const Vec3& position, unsigned int cover)
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
