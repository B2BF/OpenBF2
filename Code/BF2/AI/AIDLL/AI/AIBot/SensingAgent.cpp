// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining SensingAgent operations.

#include "SensingAgent.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1BF770, __tcf_0 0x1BF7D0, _GLOBAL__I 0x1BF800).
#include <iostream>

#include <cstring>

#include "BotSenses.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "SensingAgentPatterns.h"           // ISensingAgentPattern::reset - the shared patterns
#include "../Main/AIRandom.h"               // the per-life reaction time
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"   // IBFTimer::getGameTime
#include <map>
#include "../AIUtils/checkPointer.h"
#include "BotMain.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../../Math/Mat4.h"
#include <cmath>
#include "../Main/AIDllFrontier.h"
#include "../AIKnowledge/FireObject.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Core/Debug.h"
#include "../AIPlanInterpreter/BotScript.h"
#include "SensedObject.h"
#include "../DrawPrimitives/DPManager.h"
#include <list>

namespace dice {
namespace hfe {
namespace ai {

// Creates sensing state and an eight-sector attention vector for the bot.
SensingAgent::SensingAgent(BotMain* owner, unsigned char /*behaviourCount*/)
    : m_flag134(false)
    , m_plan(0)
{
    // +0..+20: the two focus vectors, empty.
    m_focusPosition.x = 0.0f;            // +32, +36, +40
    m_focusPosition.y = 0.0f;
    m_focusPosition.z = 0.0f;
    m_planDirection.x = 0.0f;            // +64, +68, +72
    m_planDirection.y = 0.0f;
    m_planDirection.z = 0.0f;
    m_moveDirection.x = 0.0f;            // +80..+96 zeroed
    m_moveDirection.y = 0.0f;
    m_sprintFocus.x = 0.0f;
    m_sprintFocus.y = 0.0f;
    m_sprintFocus.z = 0.0f;

    // the mechanism this agent steers. , and it takes the bot rather than the agent
    // so `BotSenses` answers to the bot directly and this object is a policy layer over it.
    m_senses = checkPointer(new BotSenses(owner));
    m_bot    = owner;

    m_bapContext = checkPointer(new BAPContext());

    // eight quadrants, zeroed. A live resize; see the block above.
    m_quadrantWeights.resize(8, 0.0f);

    // the last statement, and it is of its own. A sensing agent is only half built
    // until `reset` has run - the same constructor/reset split `BotMain` uses.
    reset();
}

// ELF 0x1BBB50: the senses (+0x68) and the BAP context (+0x90) are deleted; the plan is not.
// The vectors at +0, +0xC and +0x30 are destroyed as members.
SensingAgent::~SensingAgent()
{
    delete m_senses;
    delete m_bapContext;
}

// Deletes the owned plan and marks planning state for refresh.
void SensingAgent::deletePlan()
{
    if (m_plan)
    {
        // `delete m_plan;` - vtable byte-slot +8, the deleting destructor.
        delete m_plan;
        m_plan = 0;
    }
    m_flag140 = true;
}

// Clears the current plan's target information.
void SensingAgent::clearPlanTargetInformation()
{
    m_planTarget60 = 0;
    m_planWeight = 0.0f;
}

// Resets sensing state and draws a new reaction delay in [0.5, 1.0).
void SensingAgent::reset()
{
    m_flag120 = false;
    m_flag121 = false;
    m_double112 = 0.0;
    m_flag122 = false;
    m_flag132 = false;
    m_flag140 = true;
    m_flag133 = false;

    // the shared patterns, RE-indexed. See the block above.
    IAISettings* const settings =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    const std::map<int, ISensingAgentPattern*>& patterns = settings->getSensingAgentPatterns();

    int index = 0;
    for (std::map<int, ISensingAgentPattern*>::const_iterator it = patterns.begin();
         it != patterns.end(); ++it)
    {
        it->second->reset(index++);
    }

    deletePlan();

    m_int24  = -1;
    m_focusWeight = 0.0f;
    m_flag44 = false;
    m_flag148 = true;
    m_moveDirection.x = 0.0f;              // +80
    m_moveDirection.y = 0.0f;              // +84

    // through the buffer, and the size is not touched. See the block above.
    for (unsigned q = 0; q <= 7; ++q)
        m_quadrantWeights[q] = 0.0f;

    clearPlanTargetInformation();

    // game time, not wall-clock.
    m_double112 =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_double124 = 0.0;

    // Chooses a reaction time in [0.5, 1.0), preserving the arithmetic order.
    m_currentPeriod = 0.5f + AIRandom::m_instance->nextFloat() * (1.0f - 0.5f);

    m_senses->reset();
}

// Ignores the override while m_flag132 is set. Otherwise stores the requested value
// and the current game time. reset clears the refusal flag.
void SensingAgent::setOverride(bool value)
{
    // The refusal. See the block above.
    if (!m_flag132)
    {
        m_flag122 = value;
        m_double124 =
            static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    }
}

namespace {

// The inline Vec2 normalisation every caller below spells out: left alone within FLT_EPSILON of
// unit length, otherwise scaled by 1 / sqrt(length^2); `zeroIsFalse` reports a degenerate vector
// (computeQuadrantsFromPosition returns false there; quadInFocus zeroes it).
bool normaliseXZ(float& x, float& y, bool zeroIsFalse)
{
    const float lengthSq = y * y + x * x;
    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
    {
        if (std::fabs(lengthSq) < 1.4210855e-14f)
        {
            if (zeroIsFalse)
                return false;
            x = 0.0f;
            y = 0.0f;
            return true;
        }
        const float inverse = 1.0f / std::sqrt(lengthSq);
        x = x * inverse;
        y = inverse * y;
    }
    return true;
}

} // namespace

// ELF 0x1BE110. The point is the camera's row 2 (forward) plus row 3 (position).
void SensingAgent::setQuadSeen()
{
    const Mat4& camera = *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
    Vec3 ahead;
    ahead.x = camera.m[8] + camera.m[12];
    ahead.y = camera.m[9] + camera.m[13];
    ahead.z = camera.m[10] + camera.m[14];
    int quadrants[2];
    if (!computeQuadrantsFromPosition(ahead, quadrants))
        return;
    for (int i = 0; i < 2; ++i)
    {
        if (quadInFocus(quadrants[i]))
        {
            m_quadrantWeights[quadrants[i]] = 0.0f;
            if (m_plan && m_flag148 && quadrants[i] == m_int24)
                m_flag44 = true;
        }
    }
}

// ELF 0x1BD860. The XZ offset from the camera position (row 3), normalised, is dotted with the
// diagonal axes (+-0.70710677) for the first quadrant and with the world axes for the second.
bool SensingAgent::computeQuadrantsFromPosition(const Vec3& position, int* quadrants) const
{
    const Mat4& camera = *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
    float x = position.x - camera.m[12];
    float y = position.z - camera.m[14];
    if (!normaliseXZ(x, y, true))
        return false;
    const float s = 0.70710677f;
    const float b = y * -s + x * s;
    if (0.0f <= x * s + y * s)
        quadrants[0] = 0.0f <= b ? 1 : 0;
    else
        quadrants[0] = (b < 0.0f) + 2;
    const float d = y * 0.0f + x * 1.0f;
    if (0.0f <= x * 0.0f + y * 1.0f)
        quadrants[1] = (d < 0.0f) * 3 + 4;
    else
        quadrants[1] = (d < 0.0f) + 5;
    return true;
}

// ELF 0x1BDA60. The camera is read through the bot's player field (+0x9C) directly.
bool SensingAgent::quadInFocus(int quadrant) const
{
    if (quadrant < 0)
        return false;
    const Mat4& camera = *static_cast<const Mat4*>(m_bot->m_player->getCameraTransformation());
    float x = camera.m[8];
    float y = camera.m[10];
    normaliseXZ(x, y, false);
    Vec2 direction;
    direction.x = 0.0f;
    direction.y = 0.0f;
    getQuadPosition(quadrant, direction);
    return 0.9848f < x * direction.x + y * direction.y;
}

// ELF 0x1BDBC0: an eight-way jump table; the default reports through Debug (type 6, line 770).
void SensingAgent::getQuadPosition(int quadrant, Vec2& position) const
{
    const float s = 0.70710677f;
    switch (quadrant)
    {
    case 0: position.x = 0.0f;  position.y = 1.0f;  break;
    case 1: position.x = 1.0f;  position.y = 0.0f;  break;
    case 2: position.x = 0.0f;  position.y = -1.0f; break;
    case 3: position.x = -1.0f; position.y = 0.0f;  break;
    case 4: position.x = s;     position.y = s;     break;
    case 5: position.x = s;     position.y = -s;    break;
    case 6: position.x = -s;    position.y = -s;    break;
    case 7: position.x = -s;    position.y = s;     break;
    default:
        Debug(DebugTypeError, __FILE__, __LINE__, "") << "Bad quadrant value " << quadrant;
        break;
    }
}

// ELF 0x1BE0A0: also drops the stored override value.
void SensingAgent::disallowOverride(bool b)
{
    m_flag122 = false;
    m_flag132 = b;
}

// ELF 0x1BF230 (130 bytes). Both result flags start false here (BotMain's `ok` starts true);
// a failed or unfinished plan sets +140 and clears the plan targets.
void SensingAgent::executePlan()
{
    bool ok = false;
    bool failed = false;
    // ELF 0x1BF258: m_bot->getPlanInterpreter() vtable +8 execute(m_plan, m_bot, &ok, &failed).
    m_bot->getPlanInterpreter()->execute(m_plan, m_bot, ok, failed);
    m_flag140 = failed || !ok;
    if (m_flag140)
        clearPlanTargetInformation();
}

namespace {

// Bot +4 is the bound PlayerInput; an unmapped input reads as zero (`(mappingFlags >> i) & 1`,
// e.g. checkForPlanInvalidation 0x1BD0F7 `shrdl $0x3` then `flds 0xc(%ecx)`).
float boundInput(const io::PlayerInput* input, io::PlayerInputMap index)
{
    return ((input->mappingFlags >> static_cast<unsigned int>(index)) & 1ULL) ? input->inputs[index]
                                                                             : 0.0f;
}

// The inline Vec3 normalisation of createPlan: left alone within FLT_EPSILON of unit length,
// zeroed within FLT_EPSILON of zero (not FLT_EPSILON^2 as the Vec2 copies above).
void normalise3(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
    {
        if (std::fabs(lengthSq) >= 1.1920929e-07f)
        {
            const float inverse = 1.0f / std::sqrt(lengthSq);
            v.x = v.x * inverse;
            v.y = v.y * inverse;
            v.z = inverse * v.z;
        }
        else
        {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 0.0f;
        }
    }
}

// The vehicle's forward (transformation row 2), turned about Y by the control info's camera DOF
// offset when it is positive (withinCamView 0x1BD5DA, createPlan): row vector times the rotation.
Vec3 cameraForward(BotMain* bot, Information* equipment)
{
    const float offset = equipment->getControlInfo()->getCameraRelativeDofRotationOffset();
    const Mat4& transformation = bot->getPlayer()->getCurrentVehicle()->getTransformation();
    Vec3 forward;
    forward.x = transformation.m[8];
    forward.y = transformation.m[9];
    forward.z = transformation.m[10];
    if (0.0f < offset)
    {
        Mat4 rotation;
        for (int i = 0; i < 16; ++i)
            rotation.m[i] = 0.0f;
        rotation.m[0] = 1.0f;
        rotation.m[5] = 1.0f;
        rotation.m[10] = 1.0f;
        rotation.m[15] = 1.0f;
        setRotateYDeg(rotation, offset * 57.295776f);
        const Vec3 f = forward;
        forward.x = f.z * rotation.m[8] + f.x * rotation.m[0] + f.y * rotation.m[4];
        forward.y = f.x * rotation.m[1] + f.y * rotation.m[5] + f.z * rotation.m[9];
        forward.z = f.x * rotation.m[2] + f.y * rotation.m[6] + f.z * rotation.m[10];
    }
    return forward;
}

// The angle between `direction` and the quadrant's unit direction (dot clamped to [-1, 1]);
// isForward / isBehind share it.
float angleToQuadrant(const SensingAgent* agent, int quadrant, const Vec2& direction)
{
    Vec2 q;
    q.x = 0.0f;
    q.y = 0.0f;
    agent->getQuadPosition(quadrant, q);
    float dot = direction.x * q.x + direction.y * q.y;
    if (dot < -1.0f)
        dot = -1.0f;
    else if (dot > 1.0f)
        dot = 1.0f;
    return std::acos(dot);
}

// AIRandom::nextFloat inline with a range: (hi - lo) * r + lo (calculateCurrentPeriod 0x1BE31F).
float randomBetween(float lo, float hi)
{
    return (hi - lo) * AIRandom::m_instance->nextFloat() + lo;
}

// computeTargetQuadrant's random quadrant: floor(r * 2^-32 * 7 + 0.5) in x87 precision, through a
// float, truncated (0x1BCE7D..0x1BCEB1).
int randomQuadrant()
{
    const double r = static_cast<double>(AIRandom::m_instance->next());
    const float rounded = static_cast<float>(
        std::floor(r * static_cast<double>(2.3283064e-10f) * 7.0 + 0.5));
    return static_cast<int>(rounded);
}

} // namespace

// ELF 0x1BBD30 (1130 bytes), static double waitTime = 2.0 (0x4B7448).
void SensingAgent::update()
{
    static double waitTime = 2.0;

    Information* const equipment = informationManager->getInformation(m_bot->m_currentEquipment);
    if (!equipment)
        return;
    if (m_bot->getManDown())
        return;
    ISensingAgentPattern* const pattern =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
            ->getSensingAgentPattern(equipment->getUnit()->getEquipmentType());
    if (!pattern)
    {
        deletePlan();
        return;
    }
    if (m_flag122)
        deletePlan();
    const double now =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    // An override (a plan aiming by itself) holds the sensing agent off for waitTime seconds.
    if (m_flag122)
    {
        if (now - m_double124 > waitTime)
            m_flag122 = false;
        return;
    }
    if (now < m_double112)
        m_double112 = now;
    const double elapsed = now - m_double112;

    Information* const current = informationManager->getInformation(m_bot->getCurrentEquipment());
    bool turnedAway = false;
    if ((current->getObjectTypeFlags() & 0x00200000) != 0 && m_flag133
        && !isForward(m_int24, m_moveDirection))
        turnedAway = true;
    if (!(elapsed >= m_currentPeriod || m_flag120 || m_flag140 == true) && !turnedAway)
        return;
    if (turnedAway)
        m_flag133 = false;

    const int pose = equipment->getSoldierPose();
    bool lookAlongMove = false;
    if ((pose == 3 && !m_flag120) || pose == 4)
        lookAlongMove = true;
    if (!m_bot->isFireAllowed() || lookAlongMove)
    {
        Vec3 direction;
        direction.x = m_moveDirection.x;
        direction.y = current->getTransformation().m[9];
        direction.z = m_moveDirection.y;
        createDirPlan(direction, pattern);
        return;
    }

    Vec2 direction;
    direction.x = 0.0f;
    direction.y = 0.0f;
    if (!m_flag120)
        updateQuadTimeUrgencies(direction, turnedAway);
    QuadrantInformation quadrant;
    quadrant.position.x = 0.0f;
    quadrant.position.y = 0.0f;
    quadrant.position.z = 0.0f;
    Vec2 opposite;
    opposite.x = -direction.x;
    opposite.y = -direction.y;
    calcTargetQuadFromObjects(&quadrant, opposite);
    calculateCurrentPeriod(quadrant, current, direction);
    if (m_flag120)
        deletePlan();
    else
        m_flag121 = false;
    m_flag120 = false;
    m_double112 = now;
    checkForPlanInvalidation(quadrant, turnedAway);
    if (m_flag140 == true)
        createPlan(pattern, quadrant);
    clearInformation();
}

// ELF 0x1BC1A0. A soldier looks away from its move direction (+0x50); in a vehicle the vehicle's
// backward XZ (transformation row 2, negated) is used. Quadrants the bot is not facing gain
// urgency over time (0.0125 per update; 0.05 ahead, 0.25 ahead when turned away); a primary
// mobile vehicle clears the quadrants behind it.
void SensingAgent::updateQuadTimeUrgencies(Vec2& direction, bool turnedAway)
{
    Information* const current = informationManager->getInformation(m_bot->getCurrentEquipment());
    if ((current->getObjectTypeFlags() & 0x00200000) == 0)
    {
        Information* const primary =
            informationManager->getInformation(current->getPrimaryObject(m_bot->getSide()));
        const Mat4& transformation = primary->getTransformation();
        direction.x = -transformation.m[8];
        direction.y = -transformation.m[10];
    }
    else
    {
        Vec2 move = m_moveDirection;
        if (move.x * move.x + move.y * move.y < 0.01f)
            return;
        direction.x = -move.x;
        direction.y = -move.y;
    }
    bool clearBehind = false;
    if ((current->getObjectTypeFlags() & 0x00200000) == 0 && current->hasMobile() && current->isPrimary())
        clearBehind = true;
    for (int q = 0; q < 8; ++q)
    {
        if (clearBehind && isBehind(q, direction))
        {
            m_quadrantWeights[q] = 0.0f;
            continue;
        }
        Vec2 ahead;
        ahead.x = -direction.x;
        ahead.y = -direction.y;
        if (!isForward(q, ahead))
            m_quadrantWeights[q] = m_quadrantWeights[q] + 0.0125f;
        else if (turnedAway)
            m_quadrantWeights[q] = m_quadrantWeights[q] + 0.25f;
        else
            m_quadrantWeights[q] = m_quadrantWeights[q] + 0.05f;
    }
}

// ELF 0x1BC410. The direction argument is not read.
void SensingAgent::calcTargetQuadFromObjects(QuadrantInformation* info, const Vec2& /*direction*/)
{
    evaluateSensedObjects();
    evaluateFireObjects();
    std::pair<float, std::pair<Vec3, unsigned int> > zero;
    zero.first = 0.0f;
    zero.second.first.x = 0.0f;
    zero.second.first.y = 0.0f;
    zero.second.first.z = 0.0f;
    zero.second.second = 0;
    QuadrantWeights weights(8, zero);      // operator new(0xA0): 8 x 20 bytes
    getQuadrantWeights(weights);
    computeTargetQuadrant(weights, info);
}

// ELF 0x1BC5B0. Every living sensed object is a candidate: seen directly 0.5; seen indirectly
// 0.25 when heard or a fire target (flag 0x20, BBPFireInfantery), else 0; not seen 0.3 when
// heard, + 0.25 for a fire target. The weight is scaled by the information's getSecurity().
void SensingAgent::evaluateSensedObjects()
{
    FocusInformation empty;
    empty.weight = 0.0f;
    empty.position.x = 0.0f;
    empty.position.y = 0.0f;
    empty.position.z = 0.0f;
    m_sensedFocus.resize(0, empty);        // the image's resize: _M_fill_insert / erase
    const std::map<unsigned int, SensedObject*>& sensed = m_senses->getSensedObjects();
    for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin();
         it != sensed.end(); ++it)
    {
        SensedObject* const object = it->second;
        Information* const info = informationManager->getInformation(object->m_handle);
        if (!info)
            continue;
        IPIPhysical* const physical = info->getPhysical();
        if (physical && !(0.0f < physical->getHealth()))
            continue;
        float weight = 0.0f;
        const unsigned int flags = object->m_flags;
        if ((flags & SensedObject::SENSE_SEEN) == 0)
        {
            if (flags & SensedObject::SENSE_HEARD)
                weight = 0.3f;
            if (flags & 0x20)
                weight = weight + 0.25f;
        }
        else if (flags & SensedObject::SENSE_INDIRECT)
        {
            if ((flags & 0x20) || (flags & SensedObject::SENSE_HEARD))
                weight = weight + 0.25f;
        }
        else
        {
            weight = 0.5f;
        }
        FocusInformation focus;
        focus.weight = weight * info->getSecurity();
        focus.position = info->getPosition();
        m_sensedFocus.push_back(focus);
    }
}

// ELF 0x1BC7E0. A friendly shot draws the eye to where it lands (FireObject +0x14), weight 0.5;
// an enemy shot to the shooter, 0.7 for 5 s, then 0.5, and books a re-plan once (+0x78 / +0x79).
void SensingAgent::evaluateFireObjects()
{
    FocusInformation empty;
    empty.weight = 0.0f;
    empty.position.x = 0.0f;
    empty.position.y = 0.0f;
    empty.position.z = 0.0f;
    m_fireFocus.resize(0, empty);
    const double now =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    std::list<BotSenses::FireEntry>* const fire = m_senses->getIncommingFire();
    for (std::list<BotSenses::FireEntry>::iterator it = fire->begin(); it != fire->end(); ++it)
    {
        Information* const info = informationManager->getInformation(it->second);
        if (!info)
            continue;
        FireObject* const shot = it->first;
        FocusInformation focus;
        float weight;
        if (info->getSide() == m_bot->getSide())
        {
            focus.position = shot->getTarget();
            weight = 0.5f;
        }
        else
        {
            if (!m_flag121)
            {
                m_flag120 = true;
                m_flag121 = true;
            }
            weight = 0.5f;
            if (now - shot->getTime() < 5.0)
                weight = 0.7f;
            focus.position = info->getPosition();
        }
        focus.weight = info->getSecurity() * weight;
        m_fireFocus.push_back(focus);
    }
}

// ELF 0x1BCA90. Each candidate adds its weight and position to both of its quadrants; then the
// time urgencies (+0x30) are added and the positions averaged.
void SensingAgent::getQuadrantWeights(QuadrantWeights& weights)
{
    const std::vector<FocusInformation>* const lists[2] = { &m_sensedFocus, &m_fireFocus };
    for (int l = 0; l < 2; ++l)
    {
        for (std::vector<FocusInformation>::const_iterator it = lists[l]->begin();
             it != lists[l]->end(); ++it)
        {
            int quadrants[2];
            if (!computeQuadrantsFromPosition(it->position, quadrants))
                continue;
            for (int k = 0; k < 2; ++k)
            {
                std::pair<float, std::pair<Vec3, unsigned int> >& w = weights[quadrants[k]];
                w.first = it->weight + w.first;
                w.second.first.x = it->position.x + w.second.first.x;
                w.second.first.y = it->position.y + w.second.first.y;
                w.second.first.z = it->position.z + w.second.first.z;
                w.second.second = w.second.second + 1;
            }
        }
    }
    for (unsigned int q = 0; q < 8; ++q)
    {
        std::pair<float, std::pair<Vec3, unsigned int> >& w = weights[q];
        w.first = m_quadrantWeights[q] + w.first;
        if (w.second.second != 0)
        {
            const float inverse = 1.0f / static_cast<float>(w.second.second);
            w.second.first.x = w.second.first.x * inverse;
            w.second.first.y = w.second.first.y * inverse;
            w.second.first.z = inverse * w.second.first.z;
        }
    }
}

// ELF 0x1BCD50. The heaviest feasible quadrant wins; with none, up to seven random tries for a
// feasible one, at weight 0.1 and no position. The getQuadPosition calls' results are unused.
void SensingAgent::computeTargetQuadrant(const QuadrantWeights& weights, QuadrantInformation* info)
{
    info->quadrant = -1;
    info->weight = 0.0f;
    float best = 0.0f;
    int q = 0;
    for (QuadrantWeights::const_iterator it = weights.begin(); it != weights.end(); ++it, ++q)
    {
        Vec2 unused;
        unused.x = 0.0f;
        unused.y = 0.0f;
        getQuadPosition(q, unused);
        if (best < it->first && targetFeasible(q))
        {
            best = it->first;
            info->weight = best;
            info->quadrant = q;
            info->position = it->second.first;
        }
    }
    if (info->quadrant < 0)
    {
        unsigned int tries = 0;
        info->quadrant = randomQuadrant();
        while (!targetFeasible(info->quadrant) && tries <= 5)
        {
            ++tries;
            info->quadrant = randomQuadrant();
            Vec2 unused;
            unused.x = 0.0f;
            unused.y = 0.0f;
            getQuadPosition(info->quadrant, unused);
        }
        info->weight = 0.1f;
        info->position.x = 0.0f;
        info->position.y = 0.0f;
        info->position.z = 0.0f;
    }
}

// ELF 0x1BCFF0. Vehicles and borrowed armament never keep a sensing plan. Otherwise the plan goes
// when its focus is out of the camera's reach, a quadrant plan's quadrant changed, a target plan's
// target is still sensed (sic: `test %eax,%eax; je` keeps the plan only when the lookup is null),
// a pose-3 soldier throttles backwards, or `force`.
void SensingAgent::checkForPlanInvalidation(const QuadrantInformation& info, bool force)
{
    Information* const equipment = informationManager->getInformation(m_bot->m_currentEquipment);
    bool vehicle = false;
    if ((equipment->getObjectTypeFlags() & 0x20) || (equipment->getObjectTypeFlags() & 0x10))
        vehicle = true;
    if (equipment->getUnit()->mustBorrowArmament() || vehicle)
    {
        deletePlan();
        return;
    }
    bool invalid = false;
    if (!targetFeasible(m_int24))
        invalid = true;
    if (m_flag148)
    {
        if (info.quadrant != m_int24)
        {
            m_flag44 = false;
            invalid = true;
        }
    }
    else if (m_bot->getSensedObjectFromHandle(m_planTarget60))
    {
        invalid = true;
    }
    if (equipment->getSoldierPose() == 3 && boundInput(m_bot->getInput(), io::PIThrottle) < 0.0f)
        invalid = true;
    if (force)
        invalid = true;
    if (m_plan && invalid)
        deletePlan();
}

// ELF 0x1BD170. A quadrant is tested by its unit direction; quadrant < 0 means the plan target,
// whose world position's XZ is passed as is (not relative to the bot).
bool SensingAgent::targetFeasible(int quadrant) const
{
    Vec2 direction;
    if (quadrant < 0)
    {
        Information* const target = informationManager->getInformation(m_planTarget60);
        if (!target)
            return false;
        const Vec3& position = target->getPosition();
        direction.x = position.x;
        direction.y = position.z;
    }
    else
    {
        direction.x = 0.0f;
        direction.y = 0.0f;
        getQuadPosition(quadrant, direction);
    }
    return withinCamView(direction);
}

// ELF 0x1BD250: within 20 degrees (0.34906584 rad) of the quadrant.
bool SensingAgent::isForward(int quadrant, const Vec2& direction) const
{
    if (direction.y * direction.y + direction.x * direction.x < 0.01f || quadrant < 0)
        return false;
    return angleToQuadrant(this, quadrant, direction) < 0.34906584f;
}

// ELF 0x1BD340: within 90 degrees of the quadrant.
bool SensingAgent::isBehind(int quadrant, const Vec2& direction) const
{
    if (direction.y * direction.y + direction.x * direction.x < 0.01f || quadrant < 0)
        return false;
    return angleToQuadrant(this, quadrant, direction) < 1.5707964f;
}

// ELF 0x1BD430. The camera's XZ forward against `direction`: inside the control info's relative
// rotation limits (max on the left of the forward, min on the right); without limits always.
bool SensingAgent::withinCamView(const Vec2& direction) const
{
    Information* const equipment = informationManager->getInformation(m_bot->m_currentEquipment);
    IPIControlInfo* const control = equipment->getControlInfo();
    const Vec3 forward3 = cameraForward(m_bot, equipment);
    float x = forward3.x;
    float y = forward3.z;
    normaliseXZ(x, y, false);
    float dot = y * direction.y + x * direction.x;
    if (dot < -1.0f)
        dot = -1.0f;
    else if (dot > 1.0f)
        dot = 1.0f;
    const float angle = std::acos(dot);
    const Vec3& maxRotation = control->getCameraRelativeMaxRotation();
    const Vec3& minRotation = control->getCameraRelativeMinRotation();
    if (maxRotation.x == 0.0f && minRotation.x == 0.0f)
        return true;
    if (y * direction.x - x * direction.y <= 0.0f)
        return minRotation.x <= -angle;
    return angle <= maxRotation.x;
}

// ELF 0x1BE020: the plan target and the candidates' weights (positions are kept).
void SensingAgent::clearInformation()
{
    clearPlanTargetInformation();
    for (std::vector<FocusInformation>::iterator it = m_sensedFocus.begin(); it != m_sensedFocus.end(); ++it)
        it->weight = 0.0f;
    for (std::vector<FocusInformation>::iterator it = m_fireFocus.begin(); it != m_fireFocus.end(); ++it)
        it->weight = 0.0f;
}

// ELF 0x1BE250. The re-plan period (+0x98), drawn from a range: vehicle or borrowed armament
// [1, 1.5); no look direction [1.5, 2); standing still a soldier [3, 4), else [7, 10); moving
// with the focus ahead [7, 10), else a soldier [1.5, 2), a vehicle [5, 7).
void SensingAgent::calculateCurrentPeriod(const QuadrantInformation& info, Information* equipment,
                                          const Vec2& direction)
{
    const io::PlayerInput* const input = m_bot->getInput();
    if (!equipment->getUnit())
        return;
    const unsigned int flags = equipment->getObjectTypeFlags();
    if (equipment->getUnit()->mustBorrowArmament() || (flags & 0x20) || (flags & 0x10))
    {
        m_currentPeriod = randomBetween(1.0f, 1.5f);
        return;
    }
    Vec2 look = direction;
    if (!(0.01f <= look.y * look.y + look.x * look.x))
    {
        m_currentPeriod = randomBetween(1.5f, 2.0f);
        return;
    }
    const bool soldier = (flags & 0x00200000) != 0;
    if (boundInput(input, io::PIThrottle) < 0.01f && boundInput(input, io::PIYaw) < 0.01f)
    {
        m_currentPeriod = soldier ? randomBetween(3.0f, 4.0f) : randomBetween(7.0f, 10.0f);
        return;
    }
    look.x = -look.x;
    look.y = -look.y;
    const bool forward = isForward(info.quadrant, look);
    if (forward)
        m_currentPeriod = randomBetween(7.0f, 10.0f);
    else if (soldier)
        m_currentPeriod = randomBetween(1.5f, 2.0f);
    else
        m_currentPeriod = randomBetween(5.0f, 7.0f);
}

// ELF 0x1BE850. The previous plan is handed to the pattern first (+4 = pattern), which may reuse
// it. Vehicles and borrowed armament look along the (camera-turned) vehicle forward, a vehicle
// slightly down (-0.4 for flag 0x10, else -0.7). Otherwise the stronger of the plan direction
// (+0x4C weight, or any move) and the focus quadrant wins: a quadrant plan looks at the quadrant's
// mean position (or its direction at eye height), a direction / target plan at the plan's.
void SensingAgent::createPlan(ISensingAgentPattern* pattern, const QuadrantInformation& info)
{
    if (m_plan)
        m_plan->m_owner = pattern;
    Information* const equipment = informationManager->getInformation(m_bot->m_currentEquipment);
    bool vehicle = false;
    if ((equipment->getObjectTypeFlags() & 0x20) || (equipment->getObjectTypeFlags() & 0x10))
        vehicle = true;
    if (equipment->getUnit()->mustBorrowArmament() || vehicle)
    {
        const Vec3 forward = cameraForward(m_bot, equipment);
        if (vehicle)
        {
            Vec3 down;
            down.x = forward.x;
            down.y = (equipment->getObjectTypeFlags() & 0x10) ? -0.4f : -0.7f;
            down.z = forward.z;
            createDirPlan(down, pattern);
        }
        else
        {
            createDirPlan(forward, pattern);
        }
        m_flag133 = false;
        return;
    }

    if (0.01f <= m_moveDirection.y * m_moveDirection.y + m_moveDirection.x * m_moveDirection.x
        || m_planWeight <= info.weight)
    {
        if (info.quadrant < 0)
        {
            Debug(DebugTypeError, __FILE__, __LINE__, "") << "Sensing agent plan has nothing to look at!";
        }
        else
        {
            Vec3 direction;
            const Mat4& transformation = equipment->getTransformation();
            if (info.position.y != 0.0f)
            {
                direction.x = info.position.x - transformation.m[12];
                direction.y = info.position.y - transformation.m[13];
                direction.z = info.position.z - transformation.m[14];
            }
            else
            {
                Vec2 q;
                q.x = 0.0f;
                q.y = 0.0f;
                getQuadPosition(info.quadrant, q);
                direction.x = q.x;
                direction.y = transformation.m[9];
                direction.z = q.y;
            }
            normalise3(direction);
            m_int24 = info.quadrant;
            m_focusWeight = info.weight;
            m_plan = pattern->createPlan(this, m_bot, direction);
            m_flag148 = true;
            m_int24 = info.quadrant;
            m_focusWeight = info.weight;
            m_focusPosition = info.position;
            m_flag133 = isForward(m_int24, m_moveDirection);
        }
    }
    else
    {
        if (m_planTarget60 == m_bot->getCurrentEquipment())
        {
            Vec3 direction = m_planDirection;
            normalise3(direction);
            m_plan = pattern->createPlan(this, m_bot, direction);
            m_flag148 = false;
        }
        else
        {
            m_plan = pattern->createPlan(this, m_bot, m_planTarget60);
            m_flag148 = false;
            m_int24 = -1;
            m_focusWeight = 0.0f;
        }
        m_flag133 = false;
    }
    if (m_plan)
        m_plan->m_owner = this;
}

// ELF 0x1BF120: the previous plan's owner becomes the pattern, the new plan's this agent.
void SensingAgent::createDirPlan(const Vec3& direction, ISensingAgentPattern* pattern)
{
    if (m_plan)
        m_plan->m_owner = pattern;
    m_plan = pattern->createPlan(this, m_bot, direction);
    if (m_plan)
        m_plan->m_owner = this;
}

// ELF 0x1BF180: IAISettings +0x8C getSensingAgentNullPattern, target handle 0.
void SensingAgent::createNullPlan()
{
    ISensingAgentPattern* const pattern =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getSensingAgentNullPattern();
    if (m_plan)
        m_plan->m_owner = pattern;
    m_plan = pattern->createPlan(this, m_bot, 0u);
    if (m_plan)
        m_plan->m_owner = this;
}

void SensingAgent::insertPlanDirectionInformation(float weight, const Vec3& direction, bool flag)
{
    m_planDirection = direction;
    m_planTarget60 = m_bot->getCurrentEquipment();
    m_planWeight = weight;
    m_flag120 = flag;
}

// ELF 0x1BF2C0. Draws the selected quadrant/target, camera direction and soldier movement.
// The current-equipment and positive-weight target lookups are deliberately unchecked.
void SensingAgent::drawSensingAgentDebugStuff()
{
    Information* const equipment = informationManager->getInformation(m_bot->m_currentEquipment);
    const Vec3& position = equipment->getPosition();
    if (m_int24 >= 0)
    {
        const float axial = 1.0f * 10.0f;
        const float zero = 0.0f * 10.0f;
        const float diagonal = 0.7071067690849304f * 10.0f;
        const float negativeDiagonal = -0.7071067690849304f * 10.0f;
        const Vec3 directions[8] = {
            {zero, 3.0f, axial}, {axial, 3.0f, zero},
            {-zero, 3.0f, -axial}, {-axial, 3.0f, -zero},
            {diagonal, 3.0f, diagonal}, {diagonal, 3.0f, negativeDiagonal},
            {-diagonal, 3.0f, -diagonal}, {-diagonal, 3.0f, -negativeDiagonal}
        };
        const Vec3 end = { position.x + directions[m_int24].x,
                           position.y + directions[m_int24].y,
                           position.z + directions[m_int24].z };
        static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStaticLine(0.1f, 0xFF00, position, end, 0.1f);
    }
    else if (m_planWeight > 0.0f)
    {
        Information* const target = informationManager->getInformation(m_planTarget60);
        const Vec3& end = target->getPosition();
        static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStaticLine(0.1f, 0xF00F00, position, end, 0.1f);
    }

    Information* const target = informationManager->getInformation(m_planTarget60);
    if (target)
    {
        target->getPosition(); // The image discards this virtual result.
        const Mat4& camera = *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
        const Vec3 direction = { camera.m[8] * 3.0f, camera.m[9] * 3.0f, camera.m[10] * 3.0f };
        const Vec3 start = { camera.m[12], camera.m[13], camera.m[14] };
        const Vec3 end = { start.x + direction.x, start.y + direction.y, start.z + direction.z };
        static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStaticLine(0.1f, 0xFFFF00, start, end, 0.1f);
    }
    if (equipment->getObjectTypeFlags() & 0x200000)
    {
        const Vec3 direction = { m_moveDirection.x * 10.0f, 0.0f * 10.0f, m_moveDirection.y * 10.0f };
        const Vec3 end = { position.x + direction.x, position.y + direction.y, position.z + direction.z };
        static_cast<IDPStaticCreator*>(CommonSingletonServer::m_iDPStaticCreator)
            ->createStaticLine(0.1f, 0x550000, position, end, 0.1f);
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
