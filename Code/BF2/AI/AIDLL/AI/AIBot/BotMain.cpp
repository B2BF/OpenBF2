// Bot implementation: owns its behavior, movement, sensing and communication state.
// The manager schedules work through the Bot interface; unimplemented operations stop by name.

#include <cstdlib>
#include <algorithm>
#include <set>
#include <iostream>
// Bot.cpp and BotMain.cpp are combined here; both original TUs only initialized iostreams.
// static init: ELF 0x185D60, 0x185DC0, 0x185DF0, 0x18DA90, 0x18DAF0, 0x18DB20
#include "BotMain.h"
#include "BotOrder.h"
#include "BotCallBack.h"

#include "../Main/AIDllFrontier.h"
#include "../Main/AIMain.h"
#include "../AIStrategic/SAI.h"
#include "BotMemory.h"
#include "BotNavigation.h"
#include "BotSquad.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIBotActionPlan/BotScript.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "SensingAgent.h"
#include "CommunicationAgent.h"
#include "BotSubObjects.h"
#include "../AIBotBehaviour/BotBehaviour.h"
#include "UrgencyMerger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/Information.h"      // updatePlayerAction: the equipment's mobile plug-in
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "BotSenses.h"
#include "WaypointList.h"
#include "WPEmpty.h"
#include "WPAltitudeMoveTo.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/BFCollisionHandler.h"
#include "../../../AIInterface/BFKnowledge/SoldierPose.h"
#include "../../../../Core/Debug.h"
#include <cfloat>       // DBL_MAX - the two 'never happened' timestamps

namespace dice {
namespace hfe {
namespace ai {

// The base destructor leaves member lifetime to the derived class.
Bot::~Bot()
{
}

// Returns base bot type 1; BotMain overrides this with type 0.
int Bot::getBotType() const
{
    return 1;
}

// Returns the context stored in Bot. BotMain inherits this accessor.
BAPContext* Bot::getScriptContext()
{
    return m_bapContext;
}

// Constructs the per-bot state and helper objects. The supplied player is borrowed.
BotMain::BotMain(IAIPlayer* player,
                 unsigned int id,
                 float* weights,
                 float f,
                 int i,
                 BotOrder* order,
                 BotMemory* memory,
                 PlanInterpreter* interpreter,
                 int n,
                 UrgencyGenerator* merger,
                 UrgencyGenerator** generators)
{
    m_debugger  = 0;
    m_lodLevel  = BotLodFull;           // = 0
    m_planInvalid = true;
    m_hasDefaultPlan = false;
    m_botId        = id;
    m_basicWeights = weights;
    m_float32      = f;
    m_obedience      = 1.0f;
    m_int40        = i;
    m_inverseAttention = 0.0f;
    m_spawnToStrategicAreaId        = -1;
    m_side        = n;
    m_ptr72        = 0;
    m_float48      = 0.01f;
    m_badVehicle   = false;             // +89
    m_memory       = memory;
    m_vehicleChanged = false;           // +90
    m_inTransitionPhase = false;        // +91
    m_pad92        = 0;                 // +92
    m_deactivated  = false;             // +93
    // +100..+108: the empty behaviour-queue vector.
    m_activePlan = 0;
    m_planLock = false;                 // +120

    // List members construct themselves. [UNRESOLVED] Some element types remain unknown.

    m_currentEquipment            = -1;
    m_planInterpreter   = interpreter;
    m_urgencyMerger     = merger;
    m_urgencyGenerators = generators;
    m_lookDir = Vec3::getZero();

    // sub-object 1 of 6
    m_fireCorrection = checkPointer(new FireCorrectionData());

    for (int k = 0; k < 5; ++k)
        (&m_changeTarget)[k] = static_cast<unsigned int>(-1);   // +204..+220
    m_mineTarget = -1;
    m_moveInclination = 0;
    m_fireInclination = 0;
    m_moveInclinationDelta = 1.0f;
    m_fireInclinationDelta = 1.0f;
    m_ptr232 = 0;
    m_ptr256 = 0;
    m_ptr260 = 0;
    m_firingTarget = -1;
    m_requestedSoldierPose = 1;
    m_poseTestTarget = -1;
    m_unresolvedCollision = -1;
    m_ptr252 = 0;

    // Use -DBL_MAX for uninitialized timestamps; magnitude sentinels use separate values.
    m_time276 = -DBL_MAX;
    m_soldierPoseAlteredTime = -DBL_MAX;
    m_botStuck = false;
    m_poseInputActive = false;
    for (int k = 0; k < 4; ++k)
        m_poseTestResults[k] = false;           // +300..+303
    m_airBorn = false;                  // +312
    for (int k = 0; k < 3; ++k)
        m_spawnCoord[k] = 0;            // +316..+324
    m_deathCallBack = 0;                // +328
    m_spawnCallBack = 0;                // +332
    m_squadMember = 0;
    m_assignedAreas[0] = -1;
    m_assignedAreas[3] = -1;                   // +396
    m_assignedAreas[2] = -1;                   // +392
    m_currentWaypoint[0] = 0.0f;
    m_assignedAreas[1] = -1;                   // +388
    m_manDown = false;                  // +360
    m_isFiring = false;                 // +372
    m_fireAllowed = true;
    m_currentWaypoint[1] = 0.0f;
    m_currentWaypoint[2] = 0.0f;
    m_int412 = -1;
    m_stalledPlanTicks = 0;
    m_c4Explosives.position.x = 0.0f;             // +428..+436 (three zero word stores)
    m_c4Explosives.position.y = 0.0f;
    m_c4Explosives.position.z = 0.0f;
    m_moveToStatus = 0;
    m_timeOfLastFailedStaticAttack = 0.0;
    m_timeOfLastFailedStaticSkyAttack = 0.0;
    m_avoidSide = 0;
    for (int k = 0; k < 3; ++k)
        (&m_wantsToSimulateArtillery)[k] = false;   // +500..+502
    m_avoidSideChanged = false;                   // +512
    m_collidedObject = -1;
    m_timeOfLastStateChange = 0.0;
    for (int k = 0; k < 3; ++k)
        m_collidedObjectPosition[k] = 0;                // +528..+536

    // A different sentinel from the two above - -9999.0 is a magnitude, not a time.
    m_lastCheckMoveToFinishedTime = -9999.0;
    m_timeOfLastFailedSplashDamageAttack = -9999.0;
    m_slowDownToTarget    = -1;
    m_friendsInLineOfFireTime = 0.0;
    m_ignoreObstacles = false;          // +540
    m_isGoingForFlag = false;
    m_useStaticAttack = false;

    // copied component by component out of Vec3::getZero, which is what the binary does.
    const Vec3& zero = Vec3::getZero();
    m_takeOffPosition[0] = zero.x;
    m_takeOffPosition[1] = zero.y;
    m_takeOffPosition[2] = zero.z;

    m_grenadeThrownTime[0] = 0.0;
    m_grenadeThrownTime[1] = 0.0;
    m_grenadeThrownTime[2] = 0.0;

    // sub-objects 2 to 5
    m_navigation = checkPointer(new BotNavigation(this));
    m_squad      = checkPointer(new BotSquad(this));
    m_sensingAgent = checkPointer(new SensingAgent(this, mu8NBehaviours));

    // this byte is set in the middle of the allocations, between SensingAgent and
    // CommunicationAgent, and it is -1 rather than 0. Reproduced in position.
    m_mode = static_cast<unsigned char>(-1);
    m_communication = checkPointer(new CommunicationAgent(this));

    // Allocates matching urgency and behavior arrays, but clears only behavior pointers.
    // Urgency values remain uninitialized until written.
    m_behaviourUrgencies = checkPointer(new float[mu8NBehaviours]);
    m_behaviours = checkPointer(new BotBehaviour*[mu8NBehaviours]);
    for (unsigned k = 0; k < mu8NBehaviours; ++k)
        m_behaviours[k] = 0;

    // finishing someone else'S object - see the block above.
    *reinterpret_cast<int*>(m_memory) = -1;
    *reinterpret_cast<double*>(reinterpret_cast<char*>(m_memory) + 16) = 0.0;

    if (order)
        recieveOrder(order);            // dice's spelling

    m_waypoints = 0;
    m_lastScout = 0.0f;
    m_player = player;
    if (player)
        setPlayerInput();

    // zeroed A second time - see the block above.
    m_lookDir = Vec3::getZero();

    // sub-object 6 of 6, and it is the smallest thing here at four bytes
    m_bapContext = checkPointer(new BAPContext());

    // 256 is BF2'S player cap - the same number BotManager's player-id table uses. Every bot can
    // sense every player, and the storage for that is committed at construction.
    m_sensedObjects.reserve(256);

    m_c4Explosives.isPlaced = false;                 // the constructor's last field store
    reset();
}


// ELF .bss 0x4CDFE8: zero until configured by the module.
unsigned char BotMain::mu8NBehaviours = 0;
// ELF .bss 0x4CDFEC. Set by prepareBehaviourPool, not per bot.
float* BotMain::maBasicWeights = 0;

// [FRONTIER] Remaining bot lifecycle operations stop by name.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding BotMain operation.
// ELF 0x187A50 (D2). In order: the waypoint list (+124, deleting dtor), the behaviour and urgency
// arrays (delete[]), the memory (+76; the constructor's argument, deleted here), the death and
// spawn callbacks (deleting dtors), the BAP context, the player (IAIPlayer +0x78
// removePlayerFromGame, then IAIPlayerFactory +0x18 destroy), navigation, squad, sensing and
// communication agents, +376 cleared, the fire correction (plain operator delete); then the
// members (+340 vector, +252 vector, the three lists, the behaviour queues) and Bot.
BotMain::~BotMain()
{
    if (m_waypoints)
        delete m_waypoints;
    if (m_behaviours)
        delete[] m_behaviours;
    if (m_behaviourUrgencies)
        delete[] m_behaviourUrgencies;
    if (m_memory)
        delete m_memory;
    if (m_deathCallBack)
        delete m_deathCallBack;
    if (m_spawnCallBack)
        delete m_spawnCallBack;
    if (m_bapContext)
        delete m_bapContext;
    if (m_player)
    {
        m_player->removePlayerFromGame();
        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->destroy(m_player);
    }
    if (m_navigation)
        delete m_navigation;
    if (m_squad)
        delete m_squad;
    if (m_sensingAgent)
        delete m_sensingAgent;
    if (m_communication)
        delete m_communication;
    m_squadMember = 0;
    operator delete(m_fireCorrection);
}

// Releases each behavior's initialized state without deleting the behavior itself.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding BotMain operation.
void BotMain::uninitAllBehaviours()
{
    for (int i = 0; i < mu8NBehaviours; ++i)
    {
        // ELF 0x18A740: behaviour +0xC (the pattern), when set, vtable +0x10 unInit(this).
        if (m_behaviours[i] && m_behaviours[i]->getPattern())
            m_behaviours[i]->getPattern()->unInit(this);
    }
}

// Resets per-life bot state for reuse. Dependent operations may reach named frontier stops.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
void BotMain::reset()
{
    m_double172 = 0.0;

    // ELF 0x18B6A5 / 0x18B880: IAIPlayer +0x08 reset().
    if (m_player)
        m_player->reset();

    // ELF 0x18B850: the plan's deleting destructor (BotScript slot +0x08).
    if (m_activePlan)
    {
        delete m_activePlan;
        m_activePlan = 0;
        m_planInvalid = true;
        m_sensingAgent->setOverride(false);
    }

    m_list128.clear();
    m_orderedBehaviour = -1;

    // the five. See the block above.
    uninitAllBehaviours();
    m_squad->reset();
    m_navigation->resetAll();
    m_sensingAgent->reset();
    m_communication->reset();

    m_stalledPlanTicks = 0;
    m_int200 = -1;
    m_poseTestTarget = -1;
    m_avoidSide = 0;
    m_moveToStatus = 0;
    m_collidedObject = -1;
    m_rearmTimer = 0.0;
    m_unresolvedCollision = -1;
    m_grenadeThrownTime[0] = 0.0;
    m_grenadeThrownTime[1] = 0.0;
    m_grenadeThrownTime[2] = 0.0;
    m_timeOfLastFailedStaticAttack = 0.0;
    m_timeOfLastFailedStaticSkyAttack = 0.0;
    m_timeOfLastStateChange = 0.0;
    m_wantsToSimulateArtillery = false;
    m_isSimulatingArtillery = false;
    m_forcePose = false;
    m_avoidSideChanged = false;
    m_collidedObjectPosition[0] = 0;
    m_collidedObjectPosition[1] = 0;
    m_collidedObjectPosition[2] = 0;
    m_friendsInLineOfFireTime = 0.0;
    // three of the five, and not the first three: the change and revive targets survive a
    // reset while the three repair targets do not.
    m_wantedRepairTarget = static_cast<unsigned int>(-1);
    m_repairTarget       = static_cast<unsigned int>(-1);
    m_bridgeRepairTarget = static_cast<unsigned int>(-1);
    m_ignoreObstacles = false;
    m_slowDownToTarget = -1;
    m_lastCheckMoveToFinishedTime = -9999.0;
    m_isRepaired           = false;
    m_isEngineerTarget     = false;
    m_shouldWaitForRepairs = false;
    m_isGoingForFlag = false;
    m_useStaticAttack = false;
    // 9999.0, not the ctor's -DBL_MAX. See the block above.
    m_soldierPoseAlteredTime = -9999.0;
    m_timeOfLastFailedSplashDamageAttack = -9999.0;
    m_takeOffPosition[0] = 0.0f;
    m_takeOffPosition[1] = 0.0f;
    m_takeOffPosition[2] = 0.0f;
}

// Both materialise a Boolean result. The pointer at +68 is the
// order's weights, not the BotOrder object. There is no copy, deletion or null-order guard.
bool BotMain::recieveOrder(BotOrder* order)
{
    if (!m_takingOrders)
        return false;
    m_currentOrders = order->m_weights;
    m_orderedBehaviour = static_cast<signed char>(order->m_behaviour);
    m_list128.clear();
    if (m_ptr72)
        calculateMods();
    return true;
}

// Binding a player immediately prepares its input; no null guard.
void BotMain::setPlayer(IAIPlayer* player)
{
    m_player = player;
    setPlayerInput();
}

// Clear only the 64 values; existing mapping/state bits survive.
// The engine's current getPlayerInput declaration is const void*, but both images write through
// the returned pointer. This cast preserves that existing ABI pending its wider type recovery.
void BotMain::setPlayerInput()
{
    m_playerInput = const_cast<io::PlayerInput*>(
        static_cast<const io::PlayerInput*>(m_player->getPlayerInput()));
    for (unsigned int i = 0; i != 64; ++i)
        m_playerInput->inputs[i] = 0.0f;
    m_playerInput->mappingFlags |= 1ULL;
    m_playerInput->setInputIsMapped(io::PIPitch, true);
    m_playerInput->setInputIsMapped(io::PIRoll, true);
    m_playerInput->setInputIsMapped(io::PIThrottle, true);
    m_playerInput->setInputIsMapped(io::PISprint, true);
    m_playerInput->setInputIsMapped(io::PIUse, true);
    m_playerInput->setInputIsMapped(io::PIMouseLook, true);
    m_playerInput->setInputIsMapped(io::PIMouseLookX, true);
    m_playerInput->setInputIsMapped(io::PIMouseLookY, true);
    m_playerInput->setInputIsMapped(io::PIFire, true);
    m_playerInput->setInputIsMapped(io::PIAltFire, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect1, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect2, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect3, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect4, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect5, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect6, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect7, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect8, true);
    m_playerInput->setInputIsMapped(io::PIWeaponSelect9, true);
    m_playerInput->setInputIsMapped(io::PIPositionSelect1, true);
    m_playerInput->setInputIsMapped(io::PIPositionSelect2, true);
    m_playerInput->setInputIsMapped(io::PIPositionSelect3, true);
    m_playerInput->setInputIsMapped(io::PIPositionSelect4, true);
    m_playerInput->setInputIsMapped(io::PIPositionSelect5, true);
    m_playerInput->setInputIsMapped(io::PIPositionSelect6, true);
    m_playerInput->setInputIsMapped(io::PIPositionSelect7, true);
    m_playerInput->setInputIsMapped(io::PIPositionSelect8, true);
    m_playerInput->setInputIsMapped(io::PILie, true);
    m_playerInput->setInputIsMapped(io::PICrouch, true);
    m_playerInput->setInputIsMapped(io::PIUse, true); // repeated in both images
    m_playerInput->setInputIsMapped(io::PIReload, true);
    m_playerInput->setInputIsMapped(io::PIFlareFire, true);
    m_playerInput->setInputIsMapped(io::PIAction, true);
}

// ELF 0x18A2C0 / PE 0x1017EB20. [PLATFORM] Both products use the same factors, but the
// instructions multiply in different orders. Keep each order instead of reassociating it.
void BotMain::calculateMods()
{
    for (int i = 0; i < mu8NBehaviours; ++i)
    {
#if defined(_WIN32)
        if (m_takingOrders)
            m_behaviourUrgencies[i] = m_currentOrders[i] * m_ptr72[i] * maBasicWeights[i] * m_basicWeights[i];
        else
            m_behaviourUrgencies[i] = m_ptr72[i] * maBasicWeights[i] * m_basicWeights[i];
#else
        if (m_takingOrders)
            m_behaviourUrgencies[i] = m_currentOrders[i] * m_basicWeights[i] * maBasicWeights[i] * m_ptr72[i];
        else
            m_behaviourUrgencies[i] = maBasicWeights[i] * m_basicWeights[i] * m_ptr72[i];
#endif
    }
}

// 30/50/70 / /50/60/70: class statics, no this argument.
void BotMain::setNBehaviours(unsigned char n) { mu8NBehaviours = n; }
unsigned char BotMain::getNBehaviours() { return mu8NBehaviours; }
void BotMain::setBasicWeights(float* weights) { maBasicWeights = weights; }
float* BotMain::getBasicWeights() { return maBasicWeights; }

// per-bot accessors, unlike getBasicWeights above.
float* BotMain::getBasicBehaviourMods() { return m_basicWeights; }
float* BotMain::getCurrentBehaviourMods() { return m_behaviourUrgencies; }
void BotMain::setTakingOrders(bool taking) { m_takingOrders = taking; }

// Decision making and plan execution below remain frontiers; player binding, input preparation
// and initial order receipt above are recovered without claiming that the bot can execute them.


// The two plan-execution brackets. not vtable slots - `planExecution` at +0x010 is, but these
// two are ordinary members, so whatever calls them does so on a `BotMain*` and not through `Bot`.

// ELF 0x1891E0: only the movement axes are released before the plan runs.
void BotMain::prePlanExecution()
{
    resetAllControls(true, false, false);
}

// ELF 0x189220 (413 bytes). Soldiers only (equipment type flag 0x200000; no null guard on the
// information). One second after friends were last in the line of fire the avoid side is
// cleared; otherwise the bot strafes (PIYaw +-1) towards the avoid side while the navigation
// reports more than 0.5 of space there (+292 / +296), or switches the side when only the other
// side has space.
void BotMain::postPlanExecution()
{
    Information* const equipment = informationManager->getInformation(getCurrentEquipment());
    if (!equipment->isOfType(ITSoldier))
        return;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const double sinceFriendsInLine = now - getFriendsInLineOfFireTime();
    if (sinceFriendsInLine >= 1.0)
    {
        setAvoidSide(0);
        return;
    }
    if (m_avoidSide == 1)
    {
        if (getBotNavigation()->getSpaceLeft() > 0.5f)
            m_playerInput->setInput(io::PIYaw, -1.0f);
        else if (getBotNavigation()->getSpaceRight() > 0.5f)
            setAvoidSide(2);
    }
    else
    {
        if (getBotNavigation()->getSpaceRight() > 0.5f)
            m_playerInput->setInput(io::PIYaw, 1.0f);
        else if (getBotNavigation()->getSpaceLeft() > 0.5f)
            setAvoidSide(1);
    }
}

// Bot interface implementations, in virtual declaration order.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding BotMain operation.

// +0x008
void BotMain::setLodLevel(BotLodLevel level)
{
    m_lodLevel = level;
}

// +0x00C
BotLodLevel BotMain::getLodLevel() const
{
    return m_lodLevel;
}

// +0x010. ELF 0x188B90 (1611 bytes). Returns !m_planInvalid in %al; BotManager discards it.
// 1. Clears the change target, then copies a finished bot projectile's flight data from the
//    collision handler into the fire correction data (slots +0x44 .. +0x58, +0x40 removes it).
// 2. An invalid plan is dropped (BotScript deleting destructor, vtable +8) and the sensing
//    override released. Without a plan, the equipment unit's default behaviour (IAISettings
//    +0x1C, or the man-down equipment type +0xA0) generates one (BotBehaviour +0x2C).
// 3. With a valid plan it runs between pre/postPlanExecution; a failure invalidates it.
//    The sensing agent updates and runs its own plan with the override disallowed.
// 4. Non-soldiers release PIWeaponSelect9; a soldier falling faster than 1 m/s (speed.y <= -1)
//    presses it (inlined Input::setInput: mapped bit 22 -> inputs[22] = 1).
void BotMain::planExecution(const double& slice)
{
    bool ok = true;
    bool failed = false;
    resetChangeTarget();

    IBFCollisionHandler* const collision =
        checkPointer(static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler));
    if (collision->botProjectilePassedPoint(m_botId))
    {
        m_fireCorrection->m_projectilePrePoint = collision->botProjectilePrePoint(m_botId);
        m_fireCorrection->m_projectilePostPoint = collision->botProjectilePostPoint(m_botId);
        m_fireCorrection->m_projectileTargetPoint = collision->botProjectileTargetPoint(m_botId);
        m_fireCorrection->m_projectileMaxHeight = collision->botProjectileMaxHeight(m_botId);
        m_fireCorrection->m_flag5 = true;
        m_fireCorrection->m_flag84 = true;
        m_fireCorrection->m_flag85 = true;
        collision->removeBotProjectile(m_botId);
    }

    Information* const equipment = informationManager->getInformation(getCurrentEquipment());
    bool invalid;
    bool havePlan = false;
    if (m_planInvalid)
    {
        // ELF 0x188F80: the invalid plan is deleted (BotScript +8, deleting destructor).
        if (m_activePlan)
        {
            delete m_activePlan;
            m_activePlan = 0;
            m_sensingAgent->setOverride(false);
        }
    }
    else if (m_activePlan)
    {
        havePlan = true;
    }
    if (havePlan)
    {
        invalid = false;
    }
    else
    {
        IPIUnit* const unit = equipment ? equipment->getUnit() : 0;
        BotBehaviour* behaviour = 0;
        if (unit)
        {
            int type = unit->getEquipmentType();
            if (!m_manDown)
                unit->setTime();
            IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
            if (getManDown())
                type = settings->getManDownEquipmentType();
            behaviour = settings->getDefaultBehaviour(type);
        }
        // ELF 0x188EFE: the default behaviour's pattern (+0xC) is forced initiated (PlanGenerator
        // +0x14), then generatePlan (+0x2C) becomes the active plan; a plan is owned by the bot
        // and marks the default plan.
        if (behaviour)
        {
            behaviour->getPattern()->forceInitiated(this);
            m_activePlan = behaviour->generatePlan(this);
            if (m_activePlan)
            {
                m_activePlan->mpOwner = this;
                m_hasDefaultPlan = true;
                m_planInvalid = false;
            }
        }
        invalid = m_planInvalid;
    }

    if (!invalid)
    {
        prePlanExecution();
        // ELF 0x188D82: m_planInterpreter (+0x90) vtable +8 execute(plan, this, &ok, &failed);
        // the result is ignored. The flag pair: -0x15A first, -0x159 second; the plan stays
        // valid only when the first is set and the second clear.
        m_planInterpreter->execute(m_activePlan, this, ok, failed);
        postPlanExecution();
        m_planInvalid = failed || !ok;
        if (m_planInvalid)
        {
            setPlanLock(false);
            resetAllControls(true, true, true);
            m_sensingAgent->clearPlanTargetInformation();
        }
        m_sensingAgent->update();
        if (m_sensingAgent->getPlan())
        {
            m_sensingAgent->disallowOverride(true);
            m_sensingAgent->executePlan();
            m_sensingAgent->disallowOverride(false);
        }
    }

    // No null guard: the ELF reads the flag word of whatever the handle resolves to.
    Information* const current = informationManager->getInformation(m_currentEquipment);
    if (!current->isOfType(ITSoldier))
    {
        m_playerInput->setInput(io::PIWeaponSelect9, 0.0f);
    }
    else if (IPIMobile* const mobile = equipment->getMobile())
    {
        if (mobile->getSpeed().y <= -1.0f)
            m_playerInput->setInput(io::PIWeaponSelect9, 1.0f);
    }
    pollRequestedSoldierPose();
    (void)slice;
}

// +0x014. Returns the count BotManager::actionDecisionMaking banks in BotInfo +32 (see BotMain.h).
// ELF 0x1893D0 (3134 bytes). Returns the number of behaviours whose urgency was refreshed.
// 1. A deactivated bot drops its plan and does nothing.
// 2. The inclinations decay; a changed vehicle runs updateBotVehicle and notifies the SAI
//    (ISAI +0x40 setBotHasChangedEquipment); a vehicle the bot cannot use is reported.
// 3. Behaviour urgencies are refreshed queue by queue within the slice; once the slice is used
//    up the remaining queues only age (each moves into the one before it).
// 4. The urgency merger picks the behaviour (hasChanged(bot, 0.87f, 1.15f) unless the plan is
//    invalid), the memory records it, and its generated plan replaces the active one.
int BotMain::decisionMaking(const double& slice)
{
    std::list<int> refreshed;
    int count = 0;
    if (m_deactivated)
    {
        // ELF 0x189401: the plan is deleted (BotScript +8, the deleting destructor), the plan
        // marked invalid and the sensing override released.
        if (m_activePlan)
        {
            delete m_activePlan;
            m_activePlan = 0;
            m_planInvalid = true;
            m_sensingAgent->setOverride(false);
        }
        return 0;
    }

    const int lastBehaviour = m_memory->m_int0;
    stepMoveInclination();
    stepFireInclination();
    if (!m_checkVehicle)
    {
        IAIObject* const vehicle = m_player->getCurrentVehicle();
        if (!vehicle)
        {
            m_currentEquipment = static_cast<unsigned int>(-1);
            m_checkVehicle = true;
        }
        else
        {
            m_checkVehicle = vehicle->getInformation(m_side) != m_currentEquipment;
        }
    }
    if (m_checkVehicle)
    {
        bool usable = false;
        if (updateBotVehicle(&usable))
        {
            if (m_activePlan)
            {
                delete m_activePlan;
                m_activePlan = 0;
                m_planInvalid = true;
                m_sensingAgent->setOverride(false);
            }
            if (!usable || !informationManager->getInformation(m_currentEquipment))
            {
                Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotMain.cpp", 749, "") << "Bot in bad vehicle!";
                m_badVehicle = true;
                m_currentEquipment = static_cast<unsigned int>(-1);
                return 0;
            }
            updateBot();
            IAIMain::m_instance->getSAI(m_side)->setBotHasChangedEquipment(m_botId);
        }
        else if (m_badVehicle)
        {
            updateBot();
            m_navigation->reset();
            m_badVehicle = false;
            IAIMain::m_instance->getSAI(m_side)->setBotHasChangedEquipment(m_botId);
        }
        m_checkVehicle = false;
    }

    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    if (m_vehicleChanged)
    {
        m_planInvalid = true;
        m_vehicleChanged = false;
    }
    if (m_planInvalid)
    {
        m_memory->m_int0 = -1;
        m_memory->m_double16 = timer->getGameTime();
    }

    // Step 3 (ELF 0x189700..0x189B5E). While the slice lasts, queue i is consumed from the front:
    // null behaviours are dropped, the others get their urgency refreshed (vtable +0x28, with
    // `true`) when positive and move to `refreshed`. When start + slice <= now (getUpdateTime)
    // the rest only age: from the current queue on (at least queue 1) every behaviour still gets
    // calculateUrgency(this, false, urgency) when positive, then the queue's entries are
    // appended to the queue before it and the queue is emptied. The urgency is the stored one,
    // scaled (unless the plan is invalid or no behaviour was remembered) by
    // BotMemory::modifyBehaviourUrgency(behaviour, game time) and the last behaviour's modifier
    // row entry; the two loops multiply in different orders (kept).
    unsigned int i = 0;
    bool exhausted = false;
    for (; i < m_behaviourQueues.size() && !exhausted; ++i)
    {
        std::list<unsigned short>& queue = m_behaviourQueues[i];
        while (!queue.empty())
        {
            const unsigned short index = queue.front();
            BotBehaviour* const behaviour = m_behaviours[index];
            if (!behaviour)
            {
                queue.erase(queue.begin());
            }
            else
            {
                const float stored = m_behaviourUrgencies[index];
                float urgency = stored;
                if (!m_planInvalid && lastBehaviour != -1)
                {
                    const float modifier = m_memory->modifyBehaviourUrgency(behaviour, timer->getGameTime());
                    urgency = modifier * m_behaviours[lastBehaviour]->getModifiers()[index] * stored;
                }
                if (0.0f < urgency)
                {
                    ++count;
                    behaviour->calculateUrgency(this, true, urgency);
                }
                queue.erase(queue.begin());
                refreshed.push_back(index);
            }
            if (start + slice <= timer->getUpdateTime())
            {
                if (i == 0)
                    i = 1;
                exhausted = true;
                break;
            }
        }
        if (exhausted)
            break;
    }

    if (exhausted)
    {
        for (; i < m_behaviourQueues.size(); ++i)
        {
            std::list<unsigned short>& queue = m_behaviourQueues[i];
            for (std::list<unsigned short>::iterator it = queue.begin(); it != queue.end(); )
            {
                const unsigned short index = *it;
                BotBehaviour* const behaviour = m_behaviours[index];
                if (!behaviour)
                {
                    it = queue.erase(it);
                    continue;
                }
                float urgency;
                if (!m_planInvalid && lastBehaviour != -1)
                {
                    const float modifier = m_memory->modifyBehaviourUrgency(behaviour, timer->getGameTime());
                    urgency = modifier * m_behaviourUrgencies[index] * m_behaviours[lastBehaviour]->getModifiers()[index];
                }
                else
                {
                    urgency = m_behaviourUrgencies[index];
                }
                if (0.0f < urgency)
                    behaviour->calculateUrgency(this, false, urgency);
                ++it;
            }
            std::list<unsigned short>& previous = m_behaviourQueues[i - 1];
            previous.insert(previous.end(), queue.begin(), queue.end());
            queue.erase(queue.begin(), queue.end());
        }
    }

    // Step 4 (ELF 0x189BE0..). The merger picks the behaviour; the memory records it and the
    // game time. An existing plan is handed to the chosen behaviour (+4), or deleted when the
    // plan is invalid. A different behaviour clears the plan targets. The generated plan
    // replaces the active one (the old one is owned back by the bot, deleted, and the controls
    // reset); no plan marks the plan invalid. The refreshed behaviours go to the back of their
    // queue group (BotBehaviour +5).
    UrgencyGenerator* const merger = m_urgencyMerger;
    if (m_planInvalid || merger->hasChanged(this, 0.87f, 1.15f))
        merger->updateActiveUrgency(this);
    BotBehaviour* const chosen = static_cast<BotBehaviour*>(merger->getChosenGenerator());
    // Shipped defect: the selected behaviour is used without a null check.
    m_memory->m_int0 = chosen->getIndex();
    m_memory->m_double16 = timer->getGameTime();
    if (m_activePlan)
    {
        if (!m_planInvalid)
        {
            m_activePlan->mpOwner = chosen;
        }
        else
        {
            delete m_activePlan;
            m_activePlan = 0;
            m_sensingAgent->setOverride(false);
        }
    }
    if (lastBehaviour != static_cast<int>(chosen->getIndex()))
        m_sensingAgent->clearPlanTargetInformation();
    BotScript* const plan = chosen->generatePlan(this);
    if (m_activePlan != plan)
    {
        if (m_activePlan)
        {
            m_activePlan->mpOwner = this;
            delete m_activePlan;
            m_activePlan = 0;
            m_sensingAgent->setOverride(false);
        }
        resetAllControls(true, true, true);
    }
    m_activePlan = plan;
    if (!plan)
    {
        m_planInvalid = true;
    }
    else
    {
        m_planInvalid = false;
        m_hasDefaultPlan = false;
        plan->mpOwner = this;
    }
    for (std::list<int>::iterator it = refreshed.begin(); it != refreshed.end(); ++it)
    {
        std::list<unsigned short>& queue = m_behaviourQueues[m_behaviours[*it]->getQueueGroup()];
        queue.insert(queue.end(), static_cast<unsigned short>(*it));
    }
    return count;
}

// ELF 0x18C850: decays below 0.01 to zero, else by 0.8; the accumulated delta enters at 0.2.
void BotMain::stepMoveInclination()
{
    if (m_moveInclination <= 0.01f)
        m_moveInclination = 0.0f;
    else
        m_moveInclination = m_moveInclination * 0.8f;
    const float delta = m_moveInclinationDelta;
    m_moveInclinationDelta = 0.0f;
    m_moveInclination = delta * 0.2f + m_moveInclination;
}

// ELF 0x18C920: the same on the fire pair.
void BotMain::stepFireInclination()
{
    if (m_fireInclination <= 0.01f)
        m_fireInclination = 0.0f;
    else
        m_fireInclination = m_fireInclination * 0.8f;
    const float delta = m_fireInclinationDelta;
    m_fireInclinationDelta = 0.0f;
    m_fireInclination = delta * 0.2f + m_fireInclination;
}

// +0x018
void BotMain::cheatExecution()
{
    // five bytes - A bare `ret`. The hook exists and does nothing. Reproduced.
}



// +0x01C
// ELF 0x18A1D0: forwards to the sensing agent's BotSenses (+192 -> +104).
void BotMain::see(const double& dt)
{
    m_sensingAgent->getSenses()->see(dt);
}

// +0x020
// ELF 0x18A210: forwards like see.
void BotMain::updateSensing(const double& dt)
{
    m_sensingAgent->getSenses()->updateSensing(dt);
}

// +0x024
// ELF 0x188AA0: tail of BotNavigation::update (+188). Its bool result is returned in %al
// and BotManager discards it, so the slot keeps its void declaration.
void BotMain::updatePathfinding(bool& a, bool* b)
{
    m_navigation->update(a, b);
}

// +0x02C
// ELF 0x18BCC0 (655 bytes). Returns false only when the bot already uses the vehicle's
// information for its side and is not man-down. Otherwise the manager takes the vehicle
// (IBotManager +0x2C), the old equipment's strength drops to 3/4 and the new one's rises by 4/3,
// and the bot restarts on it. *out tells whether there is a vehicle at all.
bool BotMain::updateBotVehicle(bool* out)
{
    IAIObject* const vehicle = m_player->getCurrentVehicle();
    getPlan();
    m_vehicleChanged = true;
    m_planInvalid = true;
    m_hasDefaultPlan = false;
    if (!vehicle)
    {
        m_navigation->reset();
        setCurrentEquipment(static_cast<unsigned int>(-1));
        if (out)
            *out = false;
        return true;
    }
    if (out)
        *out = true;
    const unsigned int handle = vehicle->getInformation(m_side);
    if (handle != static_cast<unsigned int>(-1) && getCurrentEquipment() == handle && !m_manDown)
        return false;
    IBotManager::instance->updateBotVehicle(this, vehicle);
    m_airBorn = false;
    Information* const previous = informationManager->getInformation(getCurrentEquipment());
    m_lastChangeTime = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const unsigned int next = vehicle->getInformation(getSide());
    Information* const current = informationManager->getInformation(next);
    if (previous)
        previous->setStrength(previous->getStrength() * 0.75f);
    if (!current)
    {
        setCurrentEquipment(static_cast<unsigned int>(-1));
        return true;
    }
    m_sensingAgent->deletePlan();
    current->setStrength(current->getStrength() * 1.3333334f);
    setCurrentEquipment(next);
    setPlanLock(false);
    m_navigation->reset();
    m_takeOffPosition[0] = 0.0f;
    m_takeOffPosition[1] = 0.0f;
    m_takeOffPosition[2] = 0.0f;
    resetAllControls(true, true, true);
    return true;
}

// +0x030
// ELF 0x18BF50 (1488 bytes). (Re)builds the bot for its current equipment: the plan
// interpreter and the applicable behaviours for the equipment type (the man-down type while man
// down), the vehicle modifiers, then a fresh memory and a waypoint list - an empty one at the
// bot's position, or for flags & 0x10 (byte +4 of the information) a climb to 100 m above it.
// False without equipment or without its unit plug-in.
bool BotMain::updateBot()
{
    Information* const equipment = informationManager->getInformation(getCurrentEquipment());
    if (!equipment || !equipment->getUnit())
        return false;
    UrgencyGenerator** const generators = getBehaviourUrgencyGenerators();
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    const int type = m_manDown ? settings->getManDownEquipmentType()          // slot +0xA0
                               : equipment->getUnit()->getEquipmentType();
    setPlanInterpreter(static_cast<PlanInterpreter*>(settings->getPlanInterpreter(type)));   // +0x20
    const int count = settings->getNumberOfBehaviours();                     // +0x0C
    for (int i = 0; i < count; ++i)
    {
        BotBehaviour* behaviour = settings->getBehaviour(type, i);           // +0x18
        if (behaviour)
        {
            if (!behaviour->isAplicable(equipment, this))                    // BotBehaviour +0x30
                behaviour = 0;
            if (behaviour)
                behaviour->reset(this);                                      // +0x24
        }
        addBehaviour(behaviour, i);
        generators[i] = behaviour;
        if (behaviour && behaviour->getPattern())
            behaviour->getPattern()->unInit(this);                           // PlanGenerator +0x10
    }
    m_ptr72 = settings->getVehicleMods(type);                                // +0x14
    calculateMods();
    getSelector()->updateActiveUrgency(this);                                // UrgencyGenerator +0x10
    getMemory()->reset();
    if (!equipment->isOfType(ITAir))
    {
        // The image re-reads +0x8C (m_currentEquipment) inline here; no null guard.
        Information* const self = informationManager->getInformation(m_currentEquipment);
        setWaypoints(checkPointer(new WPEmpty(self->getPosition())));
    }
    else
    {
        std::list<Vec3> path;
        const Vec3& position = equipment->getPosition();
        Vec3 above;
        above.x = position.x + 0.0f;
        above.y = position.y + 100.0f;
        above.z = position.z + 0.0f;
        path.push_back(above);
        setWaypoints(checkPointer(new WPAltitudeMoveTo(1.0f, 1.0f, 25.0f, 1, path)));
    }
    return true;
}

// ELF 0x18AAD0.
UrgencyGenerator** BotMain::getBehaviourUrgencyGenerators()
{
    return m_urgencyGenerators;
}

// ELF 0x18AAC0.
UrgencyGenerator* BotMain::getSelector()
{
    return m_urgencyMerger;
}

// ELF 0x18A540 (496 bytes). Grows the queue table to the behaviour's group (+5), queues its
// index (+4) when that slot was empty, and stores it in both the behaviour and generator tables.
void BotMain::addBehaviour(BotBehaviour* b, int i)
{
    if (b)
    {
        if (m_behaviourQueues.size() <= b->getQueueGroup())
            m_behaviourQueues.resize(static_cast<unsigned int>(b->getQueueGroup()) + 1, std::list<unsigned short>());
        if (m_behaviours[b->getIndex()] == 0)
        {
            std::list<unsigned short>& queue = m_behaviourQueues[b->getQueueGroup()];
            // `insert(header, idx)`: the list's header node is end() in this STL - a push_back.
            queue.insert(queue.end(), static_cast<unsigned short>(b->getIndex()));
        }
    }
    m_behaviours[i] = b;
    m_urgencyGenerators[i] = b;
}

// +0x034
BotSquad* BotMain::getBotSquad() const
{
    return m_squad;
}

// +0x038
float BotMain::getObedience()
{
    return m_obedience;
}

// ELF 0x18ABA0: the int at +0x28 (the ctor's `i`). BBFireUnarmed::calculateUrgency converts the
// answer with cvtsi2ss, hence int.
int BotMain::getHarmlessThrsh()
{
    return m_int40;
}

// +0x03C
float* BotMain::getCurrentOrders()
{
    return m_currentOrders;
}

// +0x040
WaypointList* BotMain::getWaypoints()
{
    return m_waypoints;
}

// +0x044
// ELF 0x18A3D0: the previous list is destroyed through its deleting destructor (vtable +4)
// before the replacement is stored.
void BotMain::setWaypoints(WaypointList* w)
{
    if (m_waypoints)
        delete m_waypoints;
    m_waypoints = w;
}

// +0x04C
// ELF 0x18A080 (272 bytes). As the one-argument order, with the target list copied in order and
// the modifiers always recalculated.
bool BotMain::recieveOrder(BotOrder* order, std::list<unsigned int>& targets)
{
    if (!m_takingOrders)
        return false;
    m_currentOrders = order->m_weights;
    m_orderedBehaviour = static_cast<signed char>(order->m_behaviour);
    m_list128.clear();
    for (std::list<unsigned int>::const_iterator it = targets.begin(); it != targets.end(); ++it)
        m_list128.push_back(*it);
    calculateMods();
    return true;
}

// +0x050
std::list<unsigned int>* BotMain::getOrderList(unsigned char behaviour)
{
    return (behaviour == m_orderedBehaviour) ? &m_list128 : &m_list132;
}

// +0x054
bool BotMain::hasPlan()
{
    return !m_planInvalid;
}

// +0x058
bool BotMain::hasDefaultPlan() const
{
    return m_hasDefaultPlan;
}

// +0x05C
void BotMain::invalidatePlan()
{
    m_planInvalid = true;
}

// +0x060
BotScript* BotMain::getPlan()
{
    return m_activePlan;
}

// +0x064
void BotMain::setPlan(BotScript* plan)
{
    m_activePlan = plan;
}

// +0x068
bool BotMain::getPlanLock() const
{
    return m_planLock;
}

// +0x06C
void BotMain::setPlanLock(bool b)
{
    m_planLock = b;
}

// +0x070
bool BotMain::isTakingOrders()
{
    return m_takingOrders;
}

// ELF 0x18A9F0: +0x58 = true.
void BotMain::setUpdateCurrentEquipment()
{
    m_checkVehicle = true;
}

// ELF 0x18AA00: +0x58.
bool BotMain::getUpdateCurrentEquipment()
{
    return m_checkVehicle;
}

// +0x074
bool BotMain::isOrderedBehaviour(unsigned char behaviour)
{
    return behaviour == m_orderedBehaviour;
}

// +0x078
// ELF 0x18A450 (208 bytes). Only while taking orders, with ordered targets, for the ordered
// behaviour (BotBehaviour's index byte +4): whether `handle` is one of the targets.
bool BotMain::isOrderedObject(unsigned int handle, BotBehaviour* b)
{
    if (!m_takingOrders || m_list128.empty())
        return false;
    if (b->getIndex() != static_cast<unsigned char>(m_orderedBehaviour))
        return false;
    return std::find(m_list128.begin(), m_list128.end(), handle) != m_list128.end();
}

// ELF 0x18A780: +144.
PlanInterpreter* BotMain::getPlanInterpreter()
{
    return m_planInterpreter;
}

// ELF 0x18A790: +144.
void BotMain::setPlanInterpreter(PlanInterpreter* p)
{
    m_planInterpreter = p;
}

// ELF 0x18C7D0: +276.
void BotMain::setHadPrecisionTimeStamp(double time)
{
    m_time276 = time;
}

// +0x07C
// ELF 0x18C5E0 (432 bytes). A new target restarts the fire correction and drops the bot's
// tracked projectile.
void BotMain::setFiringTarget(unsigned int handle)
{
    if (m_firingTarget == handle)
        return;
    m_firingTarget = handle;
    m_firingTargetTime = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_fireCorrection->newTarget(handle);
    m_fireCorrection->m_flag84 = false;
    checkPointer(static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler))
        ->removeBotProjectile(m_botId);                               // slot +0x40
}

// +0x080
unsigned int BotMain::getFiringTarget() const
{
    return m_firingTarget;
}

// +0x084
double BotMain::getFiringTargetTime() const
{
    return m_firingTargetTime;
}

// +0x088
SensingAgent* BotMain::getSensingAgent()
{
    return m_sensingAgent;
}

// +0x08C
// ELF 0x18AA10: forwarded to the sensing agent.
void BotMain::insertBehaviourInformation(float f, const Vec3& v, unsigned char c)
{
    m_sensingAgent->insertBehaviourInformation(f, v, c);
}

// +0x090
// ELF 0x18AA60: BotSenses::getSensedObjects, the handle -> SensedObject* map at senses +112.
const std::map<unsigned int, SensedObject*>& BotMain::getSensedObjects() const
{
    return m_sensingAgent->getSenses()->getSensedObjects();
}

// +0x094. ELF 0x18A280: BotSenses::getSensedObjectFromHandle.
SensedObject* BotMain::getSensedObjectFromHandle(unsigned int handle) const
{
    return m_sensingAgent->getSenses()->getSensedObjectFromHandle(handle);
}

// +0x098
void BotMain::setUnresolvedCollisionObject(unsigned int handle)
{
    m_unresolvedCollision = handle;
}

// +0x09C
unsigned int BotMain::getUnresolvedCollisionHandle()
{
    return m_unresolvedCollision;
}

// +0x0A0
// ELF 0x18B0C0: `lea 0x54(senses)` - the fire-object list.
std::list<std::pair<FireObject*, unsigned int> >* BotMain::getIncommingFire()
{
    return m_sensingAgent->getSenses()->getIncommingFire();
}

// +0x0A4. ELF 0x18B0E0: `flds 0x58(senses)`.
float BotMain::getIncommingFireTotalStrength()
{
    return m_sensingAgent->getSenses()->getIncommingFireTotalStrength();
}

// +0x0A8. ELF 0x18B100: `flds 0x5c(senses)`.
float BotMain::getIncommingFireTotalStrengthAgainstSelf()
{
    return m_sensingAgent->getSenses()->getIncommingFireTotalStrengthAgainstSelf();
}

// +0x0AC. ELF 0x18B120: `flds 0x60(senses)`.
float BotMain::getAttackerStrength() const
{
    return m_sensingAgent->getSenses()->getAttackerStrength();
}

// +0x0B0
CommunicationAgent* BotMain::getCommunicationAgent()
{
    return m_communication;
}

// forward to the bound player's getId, without a null guard.
int BotMain::getPlayerId() const
{
    return m_player->getId();
}

// +0x0B8
// ELF 0x18A7D0: IAIPlayer::getInputId (+0x20), no null guard.
int BotMain::getInputId()
{
    return m_player->getInputId();
}

// +0x0BC
IAIPlayer* BotMain::getPlayer() const
{
    return m_player;
}

// The squared speed both images test, grouped (x*x + y*y) + z*z as the PE's x87 sequence is and
// summed in double, which is exactly the PE's 53-bit precision control (products of floats are
// exact at 53 bits). [PLATFORM] the ELF keeps the sum at 64-bit precision. A NaN sum is never
// "at most", which matches both images' unordered branch.
static bool speedSquaredAtMost(const Vec3& speed, float limit)
{
    const double x = speed.x, y = speed.y, z = speed.z;
    return (x * x + y * y) + z * z <= limit;
}

// Updates player actions, retaining stalled movement when a plan update was skipped.
void BotMain::updatePlayerAction(bool stalled)
{
    if (!stalled)
    {
        m_stalledPlanTicks = 0;
        return;
    }
    ++m_stalledPlanTicks;
    Information* const equipment = informationManager->getInformation(m_currentEquipment);
    if (m_stalledPlanTicks <= 15)
    {
        IPIMobile* const mobile = equipment->getMobile();
        if (mobile && !speedSquaredAtMost(mobile->getSpeed(), 0.01f))
        {
            m_playerInput->setInput(io::PILie, 0.0f);
            m_playerInput->setInput(io::PIFire, 0.0f);
            m_playerInput->setInput(io::PIAltFire, 0.0f);
            m_playerInput->setInput(io::PIReload, 0.0f);
            return;
        }
    }
    resetAllControls(true, true, true);
}

// read in outline only. Its effect depends on the
// equipment's flag word, Information::getFaceing (slot 16), the mobile speed and a function-static
// `minThrottle`. The first gate releases sprint, yaw, pitch and roll and sets the throttle against
// the speed along the facing (clamped by minThrottle) or to zero; the second releases both look
// axes; the third releases fire, alt-fire, the nine weapon and eight position selects, flares
// crouch and lie (conditionally), use and reload.
// ELF 0x18ABB0 (1208 bytes). Every release writes 0.0f through Input::setInput(float).
// The soldier test is the equipment's type flag 0x200000; the vehicle throttle is
// -clamp(dot(speed, facing) * 0.5, -minThrottle, minThrottle) with minThrottle = 0.4f
// (ELF 0x4b6884). Flag 0x10 on the equipment skips the throttle; no null guard on that read.
void BotMain::resetAllControls(bool movement, bool look, bool actions)
{
    static float minThrottle = 0.4f;
    Information* const equipment = informationManager->getInformation(m_currentEquipment);
    const bool soldier = equipment && equipment->isOfType(ITSoldier);
    if (movement)
    {
        m_playerInput->setInput(io::PISprint, 0.0f);
        m_playerInput->setInput(io::PIYaw, 0.0f);
        m_playerInput->setInput(io::PIPitch, 0.0f);
        m_playerInput->setInput(io::PIRoll, 0.0f);
        if (!equipment->isOfType(ITAir))
        {
            IPIMobile* const mobile = equipment->getMobile();
            float throttle = 0.0f;
            if (mobile && !soldier)
            {
                const Vec3 facing = equipment->getFaceing();
                const Vec3& speed = mobile->getSpeed();
                const float along = (speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) * 0.5f;
                // the ELF's fucomi/fcmovnbe order: a NaN product passes through unclamped.
                float clamped;
                if (-minThrottle > along)
                    clamped = -minThrottle;
                else
                    clamped = along > minThrottle ? minThrottle : along;
                throttle = -clamped;
            }
            m_playerInput->setInput(io::PIThrottle, throttle);
        }
    }
    if (look)
    {
        m_playerInput->setInput(io::PIMouseLookX, 0.0f);
        m_playerInput->setInput(io::PIMouseLookY, 0.0f);
    }
    if (actions)
    {
        m_playerInput->setInput(io::PIFire, 0.0f);
        m_playerInput->setInput(io::PIAltFire, 0.0f);
        for (int i = io::PIWeaponSelect1; i <= io::PIPositionSelect8; ++i)
            m_playerInput->setInput(static_cast<io::PlayerInputMap>(i), 0.0f);
        m_playerInput->setInput(io::PIFlareFire, 0.0f);
        if (isDead() || !soldier)
        {
            m_playerInput->setInput(io::PICrouch, 0.0f);
            m_playerInput->setInput(io::PILie, 0.0f);
        }
        m_playerInput->setInput(io::PIUse, 0.0f);
        m_playerInput->setInput(io::PIReload, 0.0f);
    }
}

// ELF 0x18CAF0.
void BotMain::resetChangeTarget()
{
    m_changeTarget = static_cast<unsigned int>(-1);
}

// ELF 0x18CE60.
void BotMain::clearTestResults()
{
    m_poseTestTarget = -1;
    for (int k = 0; k < 4; ++k)
        m_poseTestResults[k] = false;
}

// ELF 0x18D660 (223 bytes). A pose is acceptable unless the firing target is an enemy; then
// the results of the last pose test count, if they were made against that target.
bool BotMain::checkPoseOk(bool& out, int pose)
{
    Information* const target = informationManager->getInformation(getFiringTarget());
    if (target && target->getSide() != getSide())
    {
        if (m_poseTestTarget != static_cast<int>(getFiringTarget()))
            return false;
        if (pose != SPCrouching)
        {
            out = m_poseTestResults[3];
            return m_poseTestResults[2];
        }
        out = m_poseTestResults[1];
        return m_poseTestResults[0];
    }
    return true;
}

// ELF 0x18CEE0 (1856 bytes): drives PICrouch / PILie from the current pose (the equipment's
// SoldierPose) towards the requested one. Lying down toggles PILie against its current input
// value (inlined Input read: mapped bit 37 -> inputs[37]). An unknown requested pose reports
// "Internal error, unknown pose." (BotMain.cpp lines 2723 / 2760) and returns early.
void BotMain::pollRequestedSoldierPose()
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    Information* const equipment = informationManager->getInformation(m_currentEquipment);
    if (!equipment)
        return;

    bool unused = false;
    const int current = equipment->getSoldierPose();
    const int requested = m_requestedSoldierPose;
    io::PlayerInput* const input = m_playerInput;
    // Presses PILie unless it is already fully pressed.
    struct Lie
    {
        static void toggle(io::PlayerInput* in)
        {
            const float value = ((in->mappingFlags >> io::PILie) & 1) ? in->inputs[io::PILie] : 0.0f;
            in->setInput(io::PILie, value != 1.0f ? 1.0f : 0.0f);
        }
    };

    bool release = false;       // the LAB_19d067 arm: release both, input inactive
    if (current == SPCrouching)
    {
        if (requested == SPCrouching)
        {
            input->setInput(io::PICrouch, 1.0f);
            input->setInput(io::PILie, 0.0f);
            m_poseInputActive = false;
        }
        else if (requested == SPStanding)
        {
            input->setInput(io::PILie, 0.0f);
            input->setInput(io::PICrouch, 0.0f);
            clearTestResults();
            m_poseInputActive = true;
        }
        else if (requested == SPLying)
        {
            if (checkPoseOk(unused, SPLying) || m_forcePose)
            {
                input->setInput(io::PICrouch, 0.0f);
                Lie::toggle(input);
                m_soldierPoseAlteredTime = now;
                m_poseInputActive = true;
            }
        }
        else if (requested == SPSwimming)
        {
            release = true;
        }
        else
        {
            Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotMain.cpp", 2723, "") << "Internal error, unknown pose.";
            return;
        }
    }
    else if (current == SPStanding)
    {
        if (requested == SPCrouching)
        {
            if (checkPoseOk(unused, SPCrouching) || m_forcePose)
            {
                input->setInput(io::PICrouch, 1.0f);
                input->setInput(io::PILie, 0.0f);
                m_poseInputActive = true;
            }
        }
        else if (requested == SPStanding)
        {
            input->setInput(io::PICrouch, 0.0f);
            input->setInput(io::PILie, 0.0f);
            m_poseInputActive = false;
        }
        else if (requested == SPLying)
        {
            if (checkPoseOk(unused, SPLying) || m_forcePose)
            {
                input->setInput(io::PICrouch, 0.0f);
                Lie::toggle(input);
                m_soldierPoseAlteredTime = now;
                m_poseInputActive = true;
            }
        }
        else if (requested == SPSwimming)
        {
            release = true;
        }
        else
        {
            return;
        }
    }
    else if (current == SPLying)
    {
        if (requested == SPCrouching)
        {
            if (checkPoseOk(unused, SPCrouching) || m_forcePose)
            {
                input->setInput(io::PICrouch, 1.0f);
                input->setInput(io::PILie, 0.0f);
                m_soldierPoseAlteredTime = now;
                m_poseInputActive = true;
            }
        }
        else if (requested == SPStanding)
        {
            input->setInput(io::PICrouch, 0.0f);
            Lie::toggle(input);
            clearTestResults();
            m_soldierPoseAlteredTime = now;
            m_poseInputActive = true;
        }
        else if (requested == SPLying || requested == SPSwimming)
        {
            release = true;
        }
        else
        {
            Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotMain.cpp", 2760, "") << "Internal error, unknown pose.";
            return;
        }
    }
    else if (current == SPSwimming)
    {
        release = true;
    }
    else
    {
        m_requestedSoldierPose = SPStanding;
        m_poseInputActive = false;
    }
    if (release)
    {
        input->setInput(io::PICrouch, 0.0f);
        input->setInput(io::PILie, 0.0f);
        m_poseInputActive = false;
    }

    m_forcePose = false;
    if (m_poseInputActive && equipment->isOfType(ITSoldier))
        input->setInput(io::PIFire, 0.0f);
}

// deactivated bots are dead without consulting the player.
bool BotMain::isDead() const
{
    return m_deactivated || !m_player->getIsAlive();
}

// +0x0C8 (+0xC4). The equipment is updated before either
// the bot or its player; an absent/stale information handle skips only that equipment arm.
bool BotMain::setSide(int side)
{
    Information* const equipment = informationManager->getInformation(m_currentEquipment);
    if (equipment)
    {
        // ELF 0x18C9E0: `mov 0x20(info),%eax; call *0x128(vtbl)` - IAIObject::setSide.
        equipment->getAIObject()->setSide(side);
    }
    m_side = side;
    m_player->setSide(side);
    return true;
}

// +0x0CC
// ELF 0x18CA80: IAIPlayer::setName (+0x7C).
void BotMain::setName(const std::string& name)
{
    m_player->setName(name);
}

// +0x0D0
bool BotMain::isDeactivated() const
{
    return m_deactivated;
}

// +0x0D4
void BotMain::setAsDeactivated(bool b)
{
    m_deactivated = b;
}

// +0x0D8
unsigned int BotMain::getCurrentEquipment() const
{
    return m_currentEquipment;
}

// +0x0DC
void BotMain::setCurrentEquipment(unsigned int handle)
{
    m_currentEquipment = handle;
}

// +0x0E0
int BotMain::getSide() const
{
    return m_side;
}

// +0x0E4
int BotMain::getBotType() const
{
    return 0;
}

// +0x0E8
unsigned int BotMain::getBotId() const
{
    return m_botId;
}

// +0x0EC
BotMemory* BotMain::getMemory()
{
    return m_memory;
}

// +0x0F0
// ELF 0x18C990: IAIPlayer::getName (+0x18).
const std::string& BotMain::getName() const
{
    return m_player->getName();
}

// +0x0F4
// ELF 0x18D960: a three-deep history; a repeated time is ignored, otherwise the older two
// shift down (+460 -> +468 -> +476).
void BotMain::setTimeOfLastGrenadeThrown(double t)
{
    const double last = m_grenadeThrownTime[0];
    if (t != last)
    {
        const double older = m_grenadeThrownTime[1];
        m_grenadeThrownTime[0] = t;
        m_grenadeThrownTime[1] = last;
        m_grenadeThrownTime[2] = older;
    }
}

// +0x0F8
double BotMain::getTimeOfGrenadeThrown(int i) const
{
    return m_grenadeThrownTime[i];
}

// +0x0FC
BotNavigation* BotMain::getBotNavigation()
{
    return m_navigation;
}

// no player null guard.
int BotMain::getSpawnPoint() const
{
    return m_player->getSpawnGroup();
}

// +0x104
// ELF 0x18B9E0: IAIPlayer::getSpawnKit (+0x44).
int BotMain::getSpawnKit() const
{
    return m_player->getSpawnKit();
}

// +0x108
// ELF 0x18BA00: spawn group (+0x40) then kit (+0x44).
void BotMain::getSpawnTarget(int& a, int& b) const
{
    a = m_player->getSpawnGroup();
    b = m_player->getSpawnKit();
}

// Keep fresh player/side/id reads
// after each callback, and clear the strategic area only after the SAI notification.
void BotMain::setSpawnTarget(int spawnGroup, int kit)
{
    m_player->setSpawnGroup(spawnGroup);
    m_player->setSpawnKit(kit);
    ISAI* const sai = IAIMain::m_instance->getSAI(m_side);
    sai->setNewBotKit(m_botId, kit);
    m_spawnToStrategicAreaId = -1;
}

// Set the engine spawn metadata, notify the side's SAI
// then store the strategic area. No player/SAI null guard or value validation is present.
void BotMain::setSpawnTarget(int strategicArea, int spawnGroup, int kit)
{
    m_player->setSpawnGroup(spawnGroup);
    m_player->setSpawnKit(kit);
    ISAI* const sai = IAIMain::m_instance->getSAI(m_side);
    sai->setNewBotKit(m_botId, kit);
    m_spawnToStrategicAreaId = strategicArea;
}

// +0x114
int BotMain::getSpawnToStrategicAreaId()
{
    return m_spawnToStrategicAreaId;
}

// +0x118
// ELF 0x18BB80: true while the player is dead (and arms +313), then true exactly once on the
// first call that sees the player alive again.
bool BotMain::isJustSpawned()
{
    if (!m_player->getIsAlive())
    {
        m_spawnPending = true;
        return true;
    }
    if (!m_spawnPending)
        return false;
    m_spawnPending = false;
    return true;
}

// +0x11C
IBotDeathCallBack* BotMain::getDeathCallBack() const
{
    return m_deathCallBack;
}

// destroy the previous callback before storing its replacement.
// No same-pointer guard: passing the current callback leaves the shipped dangling pointer.
void BotMain::setDeathCallBack(IBotDeathCallBack* cb)
{
    if (m_deathCallBack)
        delete m_deathCallBack;
    m_deathCallBack = cb;
}

// +0x124
IBotSpawnCallBack* BotMain::getSpawnCallBack() const
{
    return m_spawnCallBack;
}

// same ownership ordering as the death callback.
void BotMain::setSpawnCallBack(IBotSpawnCallBack* cb)
{
    if (m_spawnCallBack)
        delete m_spawnCallBack;
    m_spawnCallBack = cb;
}

// +0x12C
void BotMain::setIsFiring(bool b)
{
    m_isFiring = b;
}

// +0x130
bool BotMain::isFiring()
{
    return m_isFiring;
}

// +0x134
void BotMain::setAllowFire(bool b)
{
    m_fireAllowed = b;
}

// +0x138
bool BotMain::isFireAllowed()
{
    return m_fireAllowed;
}

// +0x13C
void BotMain::setSquadMember(SquadMember* m)
{
    m_squadMember = m;
}

// +0x140
SquadMember* BotMain::getSquadMember()
{
    return m_squadMember;
}

// +0x144
bool BotMain::isBotStuck()
{
    return m_botStuck;
}

// +0x148
void BotMain::setBotStuck(bool b)
{
    m_botStuck = b;
}

// +0x14C
// ELF 0x1888F0: the record with the game time and a 240 s life time (0x49CB3C).
void BotMain::placeC4(int placementType, Vec3 v, AIStrategicObject* o)
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_c4Explosives.isPlaced = true;
    m_c4Explosives.placementType = placementType;
    m_c4Explosives.object = o;
    m_c4Explosives.position = v;
    m_c4Explosives.placedTime = now;
    m_c4Explosives.lifeTime = 240.0;
}

// +0x150
// ELF 0x1889C0.
bool BotMain::isC4Placed()
{
    return m_c4Explosives.isPlaced;
}

// +0x154
// ELF 0x188980: type and position stay.
void BotMain::removeC4()
{
    m_c4Explosives.isPlaced = false;
    m_c4Explosives.object = 0;
    m_c4Explosives.placedTime = 0.0;
    m_c4Explosives.lifeTime = 0.0;
}

// +0x158
// ELF 0x188880: a copy of +0x1A4..+0x1CB.
Bot::C4State BotMain::getC4()
{
    return m_c4Explosives;
}

// +0x15C
// ELF 0x1889D0: a placed charge past its life time is removed (virtual +0x154).
void BotMain::updateC4()
{
    if (!m_c4Explosives.isPlaced)
        return;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now > m_c4Explosives.placedTime + m_c4Explosives.lifeTime)
        removeC4();
}

// +0x160
void BotMain::enableBotScriptDebugger(bool /*enable*/)
{
    // defect, reproduced: the argument is discarded and the debugger is always cleared.
    m_debugger = 0;
}

// +0x164
void* BotMain::getDebugger()
{
    return m_debugger;
}

// +0x168 is `Bot::getScriptContext` and is defined above, beside `Bot::getBotType`. BotMain does not
// override it - see the note in BotMain.h.

// +0x16C
void BotMain::event_projectile(FireObject* f, unsigned int h)
{
    // ELF 0x18B1A0: forwards to the senses (+0xC0 -> +0x68).
    m_sensingAgent->getSenses()->event_projectile(f, h);
}

// +0x170
// ELF 0x18B1E0: a hit on the fire-correction target (+80) confirms the shot (+4, +5 set, +84
// cleared); any other hit counts as a miss (Bot +0x174 event_shotMissed).
void BotMain::event_shotHit(int i, world::IObject* o, const Vec3& v)
{
    if (o == m_fireCorrection->m_shotTarget)
    {
        m_fireCorrection->m_flag4 = true;
        m_fireCorrection->m_flag5 = true;
        m_fireCorrection->m_flag84 = false;
        return;
    }
    event_shotMissed(i, v);
}

// +0x174
// ELF 0x18B230 (736 bytes). A miss of the correction shot (+0 the weapon) records where it went
// and the projectile's points from the collision handler, then drops the projectile.
void BotMain::event_shotMissed(int i, const Vec3& v)
{
    FireCorrectionData* const correction = m_fireCorrection;
    if (i != correction->m_weapon)
        return;
    correction->m_missedPosition = v;
    m_fireCorrection->m_double72 = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_fireCorrection->m_flag4 = false;
    m_fireCorrection->m_flag5 = true;
    IBFCollisionHandler* const collision =
        checkPointer(static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler));
    m_fireCorrection->m_projectilePrePoint = collision->botProjectilePrePoint(m_botId);       // +0x4C
    m_fireCorrection->m_projectilePostPoint = collision->botProjectilePostPoint(m_botId);     // +0x50
    m_fireCorrection->m_projectileTargetPoint = collision->botProjectileTargetPoint(m_botId); // +0x54
    m_fireCorrection->m_projectileMaxHeight = collision->botProjectileMaxHeight(m_botId);     // +0x58
    m_fireCorrection->m_flag5 = true;
    m_fireCorrection->m_flag84 = true;
    m_fireCorrection->m_flag85 = collision->botProjectilePassedPoint(m_botId);               // +0x44
    collision->removeBotProjectile(m_botId);                                                  // +0x40
}

// +0x178
void BotMain::event_heardObject(unsigned int h)
{
    // ELF 0x18A190: forwards to the senses (+0xC0 -> +0x68).
    m_sensingAgent->getSenses()->event_heardObject(h);
}

// +0x17C
bool BotMain::isInTransitionPhase() const
{
    return m_inTransitionPhase;
}

// +0x180
void BotMain::setIsInTransitionPhase(bool b)
{
    m_inTransitionPhase = b;
}

// +0x184
void BotMain::addToMoveInclination(float f)
{
    m_moveInclinationDelta += f;
}

// +0x188
void BotMain::addToFireInclination(float f)
{
    m_fireInclinationDelta += f;
}

// +0x18C
float BotMain::getMoveInclination()
{
    return m_moveInclination;
}

// +0x190
float BotMain::getFireInclination()
{
    return m_fireInclination;
}

// +0x194
// ELF 0x18CDC0. An unforced change waits while a pose input is held and for one second after
// the last pose change.
void BotMain::requestSoldierPose(int pose, bool b)
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (pose != m_requestedSoldierPose)
    {
        if ((m_poseInputActive || now <= 1.0 + m_soldierPoseAlteredTime) && !b)
            return;
        m_requestedSoldierPose = pose;
    }
    if (b)
        m_forcePose = true;
}

// +0x198
int BotMain::getRequestedSolderPose()
{
    return m_requestedSoldierPose;
}

// +0x19C
// ELF 0x18D620.
void BotMain::setPoseStatus(int pose, unsigned int h, bool a, bool b)
{
    m_poseTestTarget = static_cast<int>(h);
    if (pose == SPCrouching)
    {
        m_poseTestResults[0] = a;
        m_poseTestResults[1] = b;
    }
    else
    {
        m_poseTestResults[2] = a;
        m_poseTestResults[3] = b;
    }
}

// +0x1A0
// ELF 0x18D740: airborne when the equipment is ITAir or ITHelicopter (two single-bit tests,
// testb $0x10 then testb $0x20).
void BotMain::event_airborn()
{
    Information* const equipment = informationManager->getInformation(m_currentEquipment);
    m_airBorn = equipment && (equipment->isOfType(ITAir) || equipment->isOfType(ITHelicopter));
}

// +0x1A4
bool BotMain::isAirBorn() const
{
    return m_airBorn;
}

// +0x1A8
void BotMain::setLastChangeTime(double t)
{
    m_lastChangeTime = t;
}

// +0x1AC
void BotMain::setManDown(bool b)
{
    m_manDown = b;
}

// +0x1B0
bool BotMain::getManDown() const
{
    return m_manDown;
}

// +0x1B4
// ELF 0x18D7D0 (382 bytes). While not man-down, every REARM_PERIOD (6.0, double) of
// accumulated time increase each weapon of the armament is topped up by REARM_PERCENT (0.125f)
// for the bot's side. The weapon handles are copied first (a local vector).
void BotMain::rearm(Information* i)
{
    static double REARM_PERIOD = 6.0;
    static float REARM_PERCENT = 0.125f;
    if (m_manDown)
        return;
    m_rearmTimer += static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease();
    if (m_rearmTimer > REARM_PERIOD)
    {
        IPIArmament* const armament = i->getArmament();
        const std::vector<unsigned int> weapons = armament->getWeapons();
        for (std::vector<unsigned int>::const_iterator it = weapons.begin(); it != weapons.end(); ++it)
        {
            if (*it == static_cast<unsigned int>(-1))
                continue;
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(*it);
            if (weapon)
                armament->rearm(weapon, REARM_PERCENT, getSide());
        }
        m_rearmTimer = 0.0;
    }
}

// +0x1B8
void BotMain::setChangeTarget(unsigned int h)
{
    m_changeTarget = h;
}

// +0x1BC
unsigned int BotMain::getChangeTarget() const
{
    return m_changeTarget;
}

// +0x1C0
void BotMain::setRepairTarget(unsigned int h)
{
    m_repairTarget = h;
}

// +0x1C4
unsigned int BotMain::getRepairTarget() const
{
    return m_repairTarget;
}

// +0x1C8
void BotMain::setWantedRepairTarget(unsigned int h)
{
    m_wantedRepairTarget = h;
}

// +0x1CC
unsigned int BotMain::getWantedRepairTarget() const
{
    return m_wantedRepairTarget;
}

// +0x1D0
void BotMain::setBridgeRepairTarget(unsigned int h)
{
    m_bridgeRepairTarget = h;
}

// +0x1D4
unsigned int BotMain::getBridgeRepairTarget() const
{
    return m_bridgeRepairTarget;
}

// +0x1D8
void BotMain::setReviveTarget(unsigned int h)
{
    m_reviveTarget = h;
}

// +0x1DC
unsigned int BotMain::getReviveTarget() const
{
    return m_reviveTarget;
}

// +0x1E0
void BotMain::setIsRepaired(bool b)
{
    m_isRepaired = b;
}

// +0x1E4
bool BotMain::getIsRepaired() const
{
    return m_isRepaired;
}

// +0x1E8
void BotMain::setIsEngineerTarget(bool b)
{
    m_isEngineerTarget = b;
}

// +0x1EC
bool BotMain::getIsEngineerTarget() const
{
    return m_isEngineerTarget;
}

// +0x1F0
void BotMain::setShouldWaitForRepairs(bool b)
{
    m_shouldWaitForRepairs = b;
}

// +0x1F4
bool BotMain::getShouldWaitForRepairs() const
{
    return m_shouldWaitForRepairs;
}

// +0x1F8
void BotMain::setMineTarget(unsigned int h)
{
    m_mineTarget = h;
}

// +0x1FC
unsigned int BotMain::getMineTarget() const
{
    return m_mineTarget;
}

// +0x200
void BotMain::setNewAssignedAreas(int a, int b)
{
    m_assignedAreas[0] = a;
    m_assignedAreas[1] = b;
}

// +0x204
// ELF 0x18CCB0: the assigned areas equal their copies, `a` is the current waypoint's int, and an
// area is assigned at all.
bool BotMain::hasSameAssignedAreas(int a)
{
    bool same = false;
    if (m_assignedAreas[0] == m_assignedAreas[2] && m_assignedAreas[3] == m_assignedAreas[1] && a == m_int412)
        same = m_assignedAreas[0] != -1;
    return same;
}

// +0x208
void BotMain::copyAssignedAreas()
{
    m_assignedAreas[2] = m_assignedAreas[0];
    m_assignedAreas[3] = m_assignedAreas[1];
}

// +0x20C
// ELF 0x18CD40: the Vec3 at +400 and the int at +412.
void BotMain::setCurrentWaypoint(Vec3 v, int i)
{
    float* const waypoint = m_currentWaypoint;
    waypoint[0] = v.x;
    waypoint[1] = v.y;
    waypoint[2] = v.z;
    m_int412 = i;
}

// +0x210
// ELF 0x18CD90: the Vec3 at +400, by value.
Vec3 BotMain::getCurrentWaypoint()
{
    const float* const waypoint = m_currentWaypoint;
    Vec3 v;
    v.x = waypoint[0];
    v.y = waypoint[1];
    v.z = waypoint[2];
    return v;
}

// +0x214
void BotMain::resetAssignedAreas()
{
    // only two of the four. The copies at [1] and [3] are left alone; reproduced.
    m_assignedAreas[0] = -1;
    m_assignedAreas[2] = -1;
}

// +0x218
void BotMain::setTimeOfLastFailedStaticSkyAttack(double t)
{
    m_timeOfLastFailedStaticSkyAttack = t;
}

// +0x21C
double BotMain::getTimeOfLastFailedStaticSkyAttack() const
{
    return m_timeOfLastFailedStaticSkyAttack;
}

// +0x220
void BotMain::setTimeOfLastFailedStaticAttack(double t)
{
    m_timeOfLastFailedStaticAttack = t;
}

// +0x224
double BotMain::getTimeOfLastFailedStaticAttack() const
{
    return m_timeOfLastFailedStaticAttack;
}

// +0x228
void BotMain::setBotWantsToSimulateArtillery(bool b)
{
    m_wantsToSimulateArtillery = b;
}

// +0x22C
bool BotMain::getBotWantsToSimulateArtillery() const
{
    return m_wantsToSimulateArtillery;
}

// +0x230
void BotMain::setBotIsSimulatingArtillery(bool b)
{
    m_isSimulatingArtillery = b;
}

// +0x234
bool BotMain::getBotIsSimulatingArtillery() const
{
    return m_isSimulatingArtillery;
}

// +0x238
// ELF 0x18DA20 (112 bytes). Answers whether an earlier switch is still flagged and the side
// changes to another non-zero one; the flag (+512) is set by a switch between two non-zero
// sides and cleared by side 0.
bool BotMain::setAvoidSide(int side)
{
    bool result = false;
    if (m_avoidSideChanged && side != m_avoidSide && side != 0)
        result = true;
    if (side == 0)
        m_avoidSideChanged = false;
    else if (m_avoidSide != side && m_avoidSide != 0)
        m_avoidSideChanged = true;
    m_avoidSide = side;
    return result;
}

// +0x23C
int BotMain::getAvoidSide() const
{
    return m_avoidSide;
}

// +0x240
int BotMain::getMoveToStatus() const
{
    return m_moveToStatus;
}

// +0x244
// ELF 0x18D9A0 (128 bytes). At most one status change per game-time stamp; a stamp ahead of the
// clock (after a time reset) is cleared first.
void BotMain::setMoveToStatus(int status)
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (m_timeOfLastStateChange > now)
        m_timeOfLastStateChange = 0.0;
    if (now - m_timeOfLastStateChange > 0.0)
    {
        m_timeOfLastStateChange = now;
        m_moveToStatus = status;
    }
}

// +0x248
void BotMain::setCollidedObject(unsigned int h)
{
    m_collidedObject = h;
}

// +0x24C
unsigned int BotMain::getCollidedObject() const
{
    return m_collidedObject;
}

// +0x250
// ELF 0x18EC60: +528.
void BotMain::setCollidedObjectPosition(const Vec3& v)
{
    m_collidedObjectPosition[0] = v.x;
    m_collidedObjectPosition[1] = v.y;
    m_collidedObjectPosition[2] = v.z;
}

// +0x254
const Vec3& BotMain::getCollidedObjectPosition() const
{
    return *reinterpret_cast<const Vec3*>(m_collidedObjectPosition);
}

// +0x258
void BotMain::setIgnoreObstacles(bool b)
{
    // it also clears the avoid side - one setter, two fields. Reproduced.
    m_ignoreObstacles = b;
    m_avoidSide = 0;
}

// +0x25C
bool BotMain::getIgnoreObstacles() const
{
    return m_ignoreObstacles;
}

// +0x260
void BotMain::setSlowDownToTarget(unsigned int h)
{
    m_slowDownToTarget = h;
}

// +0x264
unsigned int BotMain::getSlowDownToTarget() const
{
    return m_slowDownToTarget;
}

// +0x268
double BotMain::getFriendsInLineOfFireTime() const
{
    return m_friendsInLineOfFireTime;
}

// +0x26C
void BotMain::setFriendsInLineOfFireTime(double t)
{
    m_friendsInLineOfFireTime = t;
}

// +0x270
void BotMain::setLastCheckMoveToFinishedTime(double t)
{
    m_lastCheckMoveToFinishedTime = t;
}

// +0x274
double BotMain::getLastCheckMoveToFinishedTime() const
{
    return m_lastCheckMoveToFinishedTime;
}

// +0x278
void BotMain::setIsGoingForFlag(bool b)
{
    m_isGoingForFlag = b;
}

// +0x27C
bool BotMain::getIsGoingForFlag() const
{
    return m_isGoingForFlag;
}

// +0x280
// ELF 0x18CE90: within changeTime (0.8, double, ELF 0x4b6888) of the last pose change.
bool BotMain::botInPoseTransition() const
{
    static double changeTime = 0.8;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    return now - m_soldierPoseAlteredTime < changeTime;
}

// +0x284
void BotMain::setUseStaticAttack(bool b)
{
    m_useStaticAttack = b;
}

// +0x288
bool BotMain::getUseStaticAttack() const
{
    return m_useStaticAttack;
}

// +0x28C
void BotMain::setTimeOfLastFailedSplashDamageAttack(double t)
{
    m_timeOfLastFailedSplashDamageAttack = t;
}

// +0x290
double BotMain::getTimeOfLastFailedSplashDamageAttack() const
{
    return m_timeOfLastFailedSplashDamageAttack;
}

// +0x294
// ELF 0x18EE00: +576.
void BotMain::setTakeOffPosition(const Vec3& v)
{
    m_takeOffPosition[0] = v.x;
    m_takeOffPosition[1] = v.y;
    m_takeOffPosition[2] = v.z;
}

// +0x298
const Vec3& BotMain::getTakeOffPosition() const
{
    return *reinterpret_cast<const Vec3*>(m_takeOffPosition);
}

// +0x29C
void BotMain::setQuadSeen()
{
    // ELF 0x18A250: forwards to the sensing agent (+0xC0).
    m_sensingAgent->setQuadSeen();
}

// +0x2A0
bool BotMain::isAttacking(unsigned int handle, double& out) const
{
    // ELF 0x18B140: forwards to the senses (+0xC0 -> +0x68).
    return m_sensingAgent->getSenses()->isAttacking(handle, out);
}

// +0x2A4
// ELF 0x18B510 (368 bytes). The correction shot timed out: drop the projectile.
void BotMain::event_shotTimeOut(int i)
{
    IBFCollisionHandler* const collision = static_cast<IBFCollisionHandler*>(CommonSingletonServer::m_iBFCollisionHandler);
    if (i == m_fireCorrection->m_weapon)
        checkPointer(collision)->removeBotProjectile(m_botId);          // slot +0x40
}

// +0x2A8
// ELF 0x18C520 (192 bytes). Stamps the last-fire time (+172). A shot may carry a fire correction
// when the previous one was confirmed (+5) and none is pending (+84): it is booked (+5 cleared,
// +6 set, +72 = -1.0, the weapon and target stored); otherwise +6 is cleared. Both paths return
// the fire-correction record + 56 (the predicted target), re-reading the record pointer;
// AICollisionHandler::handleProjectileFire (0x382340) books it as the projectile's target point.
// [INFERRED] reference rather than pointer spelling.
const Vec3& BotMain::event_firing(bool& b, int i, const Vec3& v)
{
    m_double172 = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    FireCorrectionData* const correction = m_fireCorrection;
    b = correction->m_flag5 && !correction->m_flag84;
    if (!b)
    {
        correction->m_flag6 = false;
        return m_fireCorrection->m_predictedTarget;
    }
    correction->m_flag5 = false;
    correction->m_flag6 = true;
    correction->m_double72 = -1.0f;
    correction->m_weapon = i;
    correction->m_firedTarget[0] = v.x;
    correction->m_firedTarget[1] = v.y;
    correction->m_firedTarget[2] = v.z;
    return m_fireCorrection->m_predictedTarget;
}

// ELF 0x18BBD0: `lea 0x13c(this)`.
const Vec3& BotMain::getSpawnCoord() const
{
    return *reinterpret_cast<const Vec3*>(m_spawnCoord);
}

// ELF 0x18BBE0: a component copy into +316. BotManager::setBotIsAlive calls it on every spawn.
void BotMain::setSpawnCoord(const Vec3& v)
{
    m_spawnCoord[0] = v.x;
    m_spawnCoord[1] = v.y;
    m_spawnCoord[2] = v.z;
}

// ELF 0x18C7B0: `fldl 0xac(this)` - the double at +172.
double BotMain::getTimeOfLastFire() const
{
    return m_double172;
}

// ELF 0x18AB80: fst preserves the returned value in st(0).
float BotMain::setInvAttention(float value)
{
    m_inverseAttention = value;
    return value;
}

// ELF 0x18A360.
unsigned char BotMain::getMode() { return m_mode; }

// ELF 0x18A520.
void BotMain::setMode(unsigned char mode) { m_mode = mode; }

// ELF 0x18A390.
float BotMain::getLastScout() { return m_lastScout; }

// ELF 0x18A6C0: does not delete the behaviour or remove its queued index.
void BotMain::removeBehaviour(BotBehaviour* behaviour)
{
    m_behaviours[behaviour->getIndex()] = 0;
    m_urgencyGenerators[behaviour->getIndex()] = 0;
}

// ELF 0x18A6F0.
BotBehaviour* BotMain::getActiveBehaviour()
{
    const int index = m_memory->m_int0;
    return index == -1 ? 0 : m_behaviours[index];
}

// ELF 0x18AB40.
float BotMain::setObedience(float value) { return m_obedience = value; }

// ELF 0x18AB50.
float BotMain::getAttention() { return m_float48; }

// ELF 0x18AB60.
float BotMain::setAttention(float value) { return m_float48 = value; }

// ELF 0x18AB70.
float BotMain::getInvAttention() { return m_inverseAttention; }

// ELF 0x18AB90.
float BotMain::getMarksmanship() { return m_float32; }

// ELF 0x18B070.
const Vec3& BotMain::getLookDir() { return m_lookDir; }

// ELF 0x18B080.
void BotMain::setLookDir(const Vec3& direction) { m_lookDir = direction; }

// ELF 0x18B180: borrowed attacker map owned by the sensing state.
void* BotMain::getAttackers() const { return &m_sensingAgent->getSenses()->m_map100; }

// ELF 0x18B890. Shipped defect: this validation only reports an error.
bool BotMain::validateDataStructures()
{
    Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotMain.cpp", 1764, "")
        << "Doesn't work, didn't think it needed to.";
    return true;
}

// ELF 0x18C7C0.
double BotMain::getHadPrecisionTimeStamp() const { return m_time276; }

// ELF 0x18C820.
void BotMain::clearMoveInclination()
{
    m_moveInclination = 0.0f;
    m_moveInclinationDelta = 0.0f;
}

// ELF 0x18C8F0.
void BotMain::clearFireInclination()
{
    m_fireInclination = 0.0f;
    m_fireInclinationDelta = 0.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
