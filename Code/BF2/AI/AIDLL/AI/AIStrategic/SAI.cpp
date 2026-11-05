// Strategic simulation and supporting statistics.
// [RETIRE-WHEN: aidll-02-skeleton] chooseStrategy's body is recovered; remove this note after
// the coordinator validates strategy loading and selection at runtime.

#include "SAI.h"

#include <cmath>
// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF 0x330020,
// __tcf_0 0x330080, _GLOBAL__I 0x3300B0). The image has four more such triads for the TUs this file
// merges [INFERRED]: CommanderInterface (0x323420 / 0x323480 / 0x3234B0), ISAI and
// EnemyStatistics (0x323A40 / 0x323AA0 / 0x323AD0), SAIBotDCB (0x331F30 / 0x331F90 / 0x331FC0)
// and SAIBotSCB (0x332200 / 0x332260 / 0x332290).
#include <iostream>
#include <queue>
#include <sstream>
#include <string>

#include "../Main/AISettingsInterfaces.h"                  // IAISettings::getMaxNBots
#include "../Main/BotBehaviourManager.h"                   // getBehaviourId, by name
#include "../Main/CommonSingletonServer.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/UnitPredicate.h"
#include "../AIBot/BotCallBack.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotSquad.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadMember.h"                  // setHavePath
#include "../AIBot/WaypointList.h"                 // updateBotPositions: +5 / getId
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../Main/AIRandom.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategicMap/BotSpawner.h"
#include "Strategy.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../Main/IBotManager.h"
#include "../Main/StrategyManager.h"
#include "../AIUtils/checkPointer.h"
#include "../../../AIInterface/BFKnowledge/BFCommander.h"  // request-denial callbacks
#include "../../../AIInterface/BFKnowledge/BFGame.h"       // IAIGame counters
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"     // IAIPlayer::setSpawnGroup
#include "../../../AIInterface/BFKnowledge/BFGameEffector.h"   // IAIGameEffector::respawnBot
#include "../../../AIInterface/BFKnowledge/BFTimer.h"      // IBFTimer::getGameTime
#include "../../../../Core/Debug.h"                      // update's shipped invalid-state diagnostic

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF: std::list<(anonymous)::SquareInfo> nodes carry 8 bytes, {int +0, float +4}; list::sort
// (0x330C40, merge 0x3317F0) orders ascending by the float. Member names [INFERRED].
struct SquareInfo
{
    int m_index;      // strategic-object index (+0xF0 of the object)
    float m_value;
    bool operator<(const SquareInfo& other) const { return m_value < other.m_value; }
};

// ELF heap helpers 0x330FE0 / 0x3311F0: the object with the smallest assigned part of its wanted
// strength is on top. The side is the comparator's only state.
struct sobOrdering
{
    explicit sobOrdering(int side) : m_side(side) {}
    bool operator()(const AIStrategicObject* a, const AIStrategicObject* b) const
    {
        return a->getAssignedPartOfWantedStrength(m_side) > b->getAssignedPartOfWantedStrength(m_side);
    }
    int m_side;
};

} // namespace

// The concrete callback names and signatures are ELF-attested. Their source-file placement here
// is [INFERRED]; only SAI.cpp is attested for this part of the strategic subsystem.
class SAIBotDCB : public IBotDeathCallBack
{
public:
    // The SAI pointer is non-owning, at +8 on both ports.
    SAIBotDCB(Bot* bot, ISAI* sai) : IBotDeathCallBack(bot), m_sai(sai) {}
    // PE deleting destructor : only base destruction.
    virtual ~SAIBotDCB() {}

    // Neither pointer is guarded.
    virtual void reportDeath()
    {
        m_sai->setBotIsDead(m_bot->getBotId());
    }

private:
    ISAI* m_sai;
};

class SAIBotSCB : public IBotSpawnCallBack
{
public:
    SAIBotSCB(Bot* bot, ISAI* sai) : IBotSpawnCallBack(bot), m_sai(sai) {}
    // PE deleting destructor : only base destruction.
    virtual ~SAIBotSCB() {}

    // Preserve the repeated virtual squad lookups and priority.
    // setBotIsAlive and the three recieve* callees retain their own named frontiers.
    virtual void reportSpawn()
    {
        m_sai->setBotIsAlive(m_bot->getBotId());
        if (m_bot->getBotSquad()->hasAttackOrder())
            m_bot->getBotSquad()->recieveAttackSquadOrder();
        else if (m_bot->getBotSquad()->hasDefendOrder())
            m_bot->getBotSquad()->recieveDefendSquadOrder();
        else if (m_bot->getBotSquad()->hasMoveOrderOrder())
            m_bot->getBotSquad()->recieveMoveSquadOrder();
    }

private:
    ISAI* m_sai;
};

static_assert(sizeof(void*) != 4 || sizeof(SAIBotDCB) == 12, "SAIBotDCB x86 layout");
static_assert(sizeof(void*) != 4 || sizeof(SAIBotSCB) == 12, "SAIBotSCB x86 layout");

// Running weighted statistics across size buckets. Its two array allocations are unchecked.
EnemyStatistics::EnemyStatistics(unsigned int size)
    : m_size(size)
    , m_weight(1.0f)
    , m_samples(0)
{
    m_strength = new float[size];
    m_count    = new float[m_size];

    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = 0.0f;
        m_count[i]    = 0.0f;
    }
}

EnemyStatistics::EnemyStatistics(unsigned int size, float weight)
    : m_size(size)
    , m_weight(weight)
    , m_samples(0)
{
    m_strength = new float[size];
    m_count    = new float[m_size];

    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = 0.0f;
        m_count[i]    = 0.0f;
    }
}

EnemyStatistics::~EnemyStatistics()
{
    if (m_strength)
        delete[] m_strength;

    if (m_count)
        delete[] m_count;
}

// Updates each bucket with (n * old + weight * observation) / (n + weight), using the
// entry sample count for every bucket. Integer counts are accumulated as floats.
void EnemyStatistics::sample(float* strength, int* count)
{
    const float samples = static_cast<float>(m_samples);
    const float total   = samples + m_weight;

    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = samples / total * m_strength[i];
        m_strength[i] = m_weight / total * strength[i] + m_strength[i];

        m_count[i] = static_cast<float>(m_samples) / total * m_count[i];
        m_count[i] = m_weight / total * static_cast<float>(count[i]) + m_count[i];
    }

    ++m_samples;
}

// both of these walk this object'S size, not the other's. Copying from a differently-sized
// statistics object reads past its arrays. Shipped; no check here either.
void EnemyStatistics::copy(EnemyStatistics& other)
{
    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = other.m_strength[i];
        m_count[i]    = other.m_count[i];
    }

    m_samples = other.m_samples;
}

void EnemyStatistics::add(EnemyStatistics& other)
{
    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] += other.m_strength[i];
        m_count[i]    += other.m_count[i];
    }

    m_samples += other.m_samples;
}

void EnemyStatistics::clear()
{
    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = 0.0f;
        m_count[i]    = 0.0f;
    }

    m_samples = 0;
}

// CommanderInterface - one byte, no state, six orders. The constructor and destructor are five
// bytes each: a return.
CommanderInterface::CommanderInterface()
{
}

CommanderInterface::~CommanderInterface()
{
}


// ELF IBFCommander 0x10: (position, squad id + 1, side + 1, no target); side is read first.
void CommanderInterface::giveAttackOrder(const Vec3& position, SquadCoordinator* squad)
{
    const int side = squad->getSide() + 1;
    const int id = squad->getSquadId() + 1;
    static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->giveAttackOrder(position, id, side, 0);
}

// ELF IBFCommander 0x14: (position, squad id + 1, side + 1, no target); side is read first.
void CommanderInterface::giveMoveOrder(const Vec3& position, SquadCoordinator* squad)
{
    const int side = squad->getSide() + 1;
    const int id = squad->getSquadId() + 1;
    static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->giveMoveOrder(position, id, side, 0);
}

// ELF IBFCommander 0x18: (position, squad id + 1, side + 1, no target); side is read first.
void CommanderInterface::giveDefendOrder(const Vec3& position, SquadCoordinator* squad)
{
    const int side = squad->getSide() + 1;
    const int id = squad->getSquadId() + 1;
    static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->giveDefendOrder(position, id, side, 0);
}

// ELF IBFCommander 0x10: (position, the bot squad's id +4, the bot's side, no target) - no +1
// here, unlike the squad overloads; the squad is read first.
void CommanderInterface::giveAttackOrder(const Vec3& position, Bot* bot)
{
    BotSquad* const squad = bot->getBotSquad();
    const int side = bot->getSide();
    static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->giveAttackOrder(position, squad->getSquadId(), side, 0);
}

// ELF IBFCommander 0x14: (position, the bot squad's id +4, the bot's side, no target) - no +1
// here, unlike the squad overloads; the squad is read first.
void CommanderInterface::giveMoveOrder(const Vec3& position, Bot* bot)
{
    BotSquad* const squad = bot->getBotSquad();
    const int side = bot->getSide();
    static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->giveMoveOrder(position, squad->getSquadId(), side, 0);
}

// ELF IBFCommander 0x18: (position, the bot squad's id +4, the bot's side, no target) - no +1
// here, unlike the squad overloads; the squad is read first.
void CommanderInterface::giveDefendOrder(const Vec3& position, Bot* bot)
{
    BotSquad* const squad = bot->getBotSquad();
    const int side = bot->getSide();
    static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->giveDefendOrder(position, squad->getSquadId(), side, 0);
}


// ISAI - the interface. Its destructor is : a vptr store and a return, like every other
// bare interface destructor in this subsystem. No registration and no roster - `ISAI` is not a
// singleton, it is a per-side object `AIMain` owns by pointer.
ISAI::~ISAI()
{
}

// the unwind path the
// constructor's possible throw needs is the other hundred bytes.
// ELF 0x3239B0.
SAI* ISAI::create(int side, int gridWidth, int gridHeight, Vec2 min, Vec2 max)
{
    return new SAI(side, gridWidth, gridHeight, min, max);
}

// Starts the three integer states at -1 and the float accumulator at zero.
SAI::BotInfo::BotInfo(Bot* bot)
    : m_bot(bot)
    , m_currentlyOrderedSquare(-1)
    , m_lastOrderedSquare(-1)
    , m_currentlyOccupiedSquare(-1)
    , m_float10(0.0f)
    , m_double14(0.0)
{
}

// Clears both 44-element state arrays, then sets each final normalization slot to one.
void SAI::clearStates(float* a, float* b)
{
    for (int i = 0; i <= 43; ++i)
    {
        a[i] = 0.0f;
        b[i] = 0.0f;
    }

    a[43] = 1.0f;
    b[43] = 1.0f;
}

// ELF 0x323AF0 (C2; C1 0x3249A0). Creates strategic state for side. Grid dimensions and bounds are
// retained in the signature but unused. +0xE0 is not constructed (no store in either body).
SAI::SAI(int side, int, int, Vec2, Vec2)
    : m_flagAC(true)
    , m_idleBehaviourId(0)
    , m_moveToBehaviourId(0)
    , m_avoidBehaviourId(0)
    , m_fireBehaviourId(0)
    , m_intD8(0)
    , m_intDC(0)
    // The strategy cursor starts at `m_strategies.end`, which on a freshly built empty vector is
    // the null `finish` pointer. The binary copies the vector's finish word into it.
    , m_activeStrategy(0)
    , m_enemyStats(0)
    , m_friendlyStats(0)
    , m_int18C(19)
    , m_float190(-1000.0f)
    , m_side(side)
    , m_sideMask(1 << side)
    // the other side'S BIT, and the expression only has two answers - this is a two-sided game.
    , m_enemyMask((side == 1) ? 4 : 2)
    , m_flag1C0(false)
    , m_flag1C1(false)
    , m_flag1CC(false)
    , m_flag1CD(false)
    , m_flag1CE(true)
    , m_flag1CF(false)
    , m_flag1D0(true)
    , m_flag1D1(false)
    , m_strategicObjectIterator()
    , m_stateA(0)
    , m_stateB(0)
    , m_stateC(0)
    , m_stateD(0)
    , m_int1F4(0)
    , m_commander(0)
{
    m_commander = checkPointer(new CommanderInterface());

    m_idleBehaviourId   = BotBehaviourManager::m_instance->getBehaviourId(std::string("Idle"));
    m_moveToBehaviourId = BotBehaviourManager::m_instance->getBehaviourId(std::string("MoveTo"));
    m_avoidBehaviourId  = BotBehaviourManager::m_instance->getBehaviourId(std::string("Avoid"));
    m_fireBehaviourId   = BotBehaviourManager::m_instance->getBehaviourId(std::string("Fire"));

    // four deadlines off one clock read. Three at a minute out and one at ten seconds, with the
    // fifth left at zero - so three slow reviews and one fast one, all stamped at construction.
    const double now =
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();

    m_time80 = now + 10.0;
    m_time68 = now + 60.0;
    m_time70 = now + 60.0;
    m_time78 = now + 60.0;
    m_time88 = 0.0;

    m_stateA = checkPointer(new float[44]);
    m_stateB = checkPointer(new float[44]);
    m_stateC = checkPointer(new float[44]);
    m_stateD = checkPointer(new float[44]);

    // the pairing is (A, C) and (B, D), not (A) and (C, D).
    clearStates(m_stateA, m_stateC);
    clearStates(m_stateB, m_stateD);

    // Not checkPointer'ed, unlike everything above.
    m_enemyStats    = new EnemyStatistics(6);
    m_friendlyStats = new EnemyStatistics(6);

    for (int i = 0; i <= 5; ++i)
    {
        m_enemyStrengths[i] = 0.0f;
        m_enemyCounts[i] = 0.0f;
        m_friendlyStrengths[i] = 0.0f;
        m_friendlyCounts[i] = 0.0f;
    }

    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    m_bots.resize(settings->getMaxNBots());

    for (int i = 0; i < settings->getMaxNBots(); ++i)
        m_bots[i] = 0;
}

// ELF 0x3257A0 (D2; D1 0x325E70, D0 0x326540; from BF2AI::clear): deletes the commander interface
// (+0x1F8), the two EnemyStatistics (+0x124/+0x128), delete[]s the four state rows (+0x1E4..+0x1F0),
// each tested for null; every container member then goes in reverse declaration order. The
// BotInfo pointers in m_bots are not deleted (no BotInfo destructor call in the body).
SAI::~SAI()
{
    if (m_commander)
        delete m_commander;
    if (m_enemyStats)
        delete m_enemyStats;
    if (m_friendlyStats)
        delete m_friendlyStats;
    if (m_stateA)
        delete[] m_stateA;
    if (m_stateB)
        delete[] m_stateB;
    if (m_stateC)
        delete[] m_stateC;
    if (m_stateD)
        delete[] m_stateD;
}


// Map-dimension setters intentionally ignore their arguments, matching the shipped bodies.
void SAI::setMapXDimension(int)
{
}

void SAI::setMapYDimension(int)
{
}


// ELF 0x326C10: appends {0, 0, 0, 1, strategy} and points the active strategy at the first one.
void SAI::addStrategy(Strategy* strategy)
{
    StrategyContainer container;
    container.m_f00 = 0.0f;
    container.m_f04 = 0.0f;
    container.m_startTime = 0.0f;
    container.m_i0C = 1;
    container.strategy = strategy;
    m_strategies.push_back(container);
    m_activeStrategy = &m_strategies[0];
}

// One resumable pass, with a budget check after each
// advancing stage. Equality and NaN pass the budget check; state 4's frequency gate instead
// requires an ordered strict greater-than. The prefix always runs, even for exhausted budgets.
// State 5 and 16..18 are holes. The scheduled helpers retain their own named frontiers.
void SAI::update(double budget)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    updateRequestDenyTimers();

    for (std::vector<BotInfo*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        if (*it)
        {
#if defined(_MSC_VER)
            // [PLATFORM] PE reloads the entry after the timer callback and does not round
            // the interval to float before adding it to the stored float.
            const double increase = timer->getTimeIncrease();
            BotInfo* const info = *it;
            info->m_float10 = static_cast<float>(info->m_float10 + increase);
#else
            BotInfo* const info = *it;
            const float increase = static_cast<float>(timer->getTimeIncrease());
            info->m_float10 += increase;
#endif
        }
    }

    switch (m_int18C)
    {
    case 19:
        m_int18C = 0;
        // fall through
    case 0:
        m_int18C = 1;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 1:
        updateStrengths();
        m_int18C = 2;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 2:
    {
        const bool next = !m_int1F4;
        clearStates(next ? m_stateB : m_stateA, next ? m_stateD : m_stateC);
        m_int18C = 3;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    }
    case 3:
    {
        const bool next = !m_int1F4;
        updateStates(next ? m_stateB : m_stateA, next ? m_stateD : m_stateC);
        m_int18C = 4;
        m_int1F4 = next;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    }
    case 4:
    {
#if defined(_MSC_VER)
        // [PLATFORM] PE retains settings across getGameTime; ELF reloads it afterward.
        const IAISettings* const settings =
            static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
        const double gameTime = timer->getGameTime();
        const double elapsed = gameTime - m_float190;
#else
        const double gameTime = timer->getGameTime();
        const double elapsed = gameTime - m_float190;
        const IAISettings* const settings =
            static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
#endif
        if (!(elapsed > settings->getSAIUpdateFrequency()))
            return;
        prepareUpdate();
        m_int18C = 6;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    }
    case 6:
        if (!m_flag1D1)
        {
            if (!updateStrategicObjects())
                return;
            checkCommanderUseArtillery();
        }
        m_int18C = 7;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 7:
        if (!m_flag1D1)
            chooseStrategy();
        m_int18C = 8;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 8:
        modifyStrategicObjectsTemperatures();
        m_int18C = 9;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 9:
        updateBotPositions();
        m_int18C = 10;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 10:
        if (!m_flag1D1)
            updateDistribution();
        m_int18C = 11;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 11:
        if (!m_flag1D1)
            distributeResources();
        if (!haveAnyOwnedCps(m_side))
        {
            m_int18C = 15;
            return;
        }
        m_int18C = 12;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 12:
        if (!m_flag1D1)
            distributeDefence();
        m_int18C = 13;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 13:
        if (!m_flag1D1)
            distributeAttack();
        m_int18C = 14;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 14:
        // The ELF calls this empty direct helper; PE eliminates both the test and call.
        if (!m_flag1D1)
            distributeArtillery();
        m_int18C = 19;
        timer->getUpdateTime(); // Shipped final sample, discarded without another budget check.
        return;
    case 15:
        if (!m_flag1D1)
            distributeWithNoAreas();
        m_int18C = 19;
        timer->getUpdateTime();
        return;
    default:
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategic/SAI.cpp", 1044, "")
            << "Unhadled case in SAI::Update()."; // Preserve the shipped spelling and source line.
        return;
    }
}

// Null bots do nothing; dead bots first receive (-1,-1,13).
// Squad registration is now a typed call; its narrower relocation/formation frontiers remain
// visible in the owning bodies. Only a completed registration reaches the publication tail.
void SAI::addBot(Bot* bot)
{
    if (!bot)
        return;

    if (bot->isDead())
        bot->setSpawnTarget(-1, -1, 13);

    SquadManager::m_instance->addSquadMember(bot->getPlayerId(), m_side);

    // Recovered tail, reached only after squad registration returns.
    // The vector is ELF +0x94 / PE +0xAC; port-specific STL layouts must not be copied as offsets.
    m_vector94.push_back(bot);
    bot->setDeathCallBack(checkPointer(new SAIBotDCB(bot, this)));
    bot->setSpawnCallBack(checkPointer(new SAIBotSCB(bot, this)));
}

// ELF 0x328D10: handleDeadBot(botId).
void SAI::setBotIsDead(int botId)
{
    handleDeadBot(botId);
}

// ELF 0x328DB0: a known bot leaves the strategic bookkeeping (BotInfo::remove); its assigned
// object (m_currentlyOrderedSquare, read before the removal) and spawn kit (+0x104) are recorded in the dead-bot
// map (+0x5C, insert_unique), its player's spawn group is reset to 0 (IAIPlayer +0x38), and the
// record is marked dead.
void SAI::handleDeadBot(int botId)
{
    BotInfo* const info = m_bots[botId];
    if (!info)
        return;
    const int kit = info->m_bot->getSpawnKit();
    const int object = m_bots[botId]->m_currentlyOrderedSquare;
    m_bots[botId]->remove(this);
    m_map5C.insert(std::make_pair(botId, std::make_pair(object, kit)));
    m_bots[botId]->m_bot->getPlayer()->setSpawnGroup(0);
    m_bots[botId]->setAsDead(this);
}

// ELF 0x328AE0: a known, living bot is placed at the closest strategic object to its current
// equipment's position (getClosestObject(pos, equipment type, vehicle number or -1, side)) and
// leaves the dead-bot map; the map is searched linearly (`cmpl %ecx,0x10(%edx)`) and the first
// match erased. The first spawn after a reset (+0xAC) resets the bridges.
void SAI::setBotIsAlive(int botId)
{
    if (!m_bots[botId])
        return;
    if (m_flagAC)
    {
        AIStrategicObjectManager::getInstance()->resetBridges();
        m_flagAC = false;
    }
    if (m_bots[botId]->m_bot->isDead())
        return;
    Information* const information =
        informationManager->getInformation(m_bots[botId]->m_bot->getCurrentEquipment());
    if (!information || !information->getUnit())
        return;
    int vehicle = -1;
    if (information->getMobile())
        vehicle = information->getMobile()->getVehicleNumber();
    const int equipment = information->getUnit()->getEquipmentType();
    const Vec3& position = information->getPosition();
    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getClosestObject(position, equipment, vehicle, m_side);
    m_bots[botId]->respawn(object, this);
    for (std::map<int, std::pair<int, int> >::iterator it = m_map5C.begin(); it != m_map5C.end(); ++it)
    {
        if (it->first == botId)
        {
            m_map5C.erase(it);
            break;
        }
    }
}

// ELF 0x329300: a dead bot only leaves the dead-bot map (linear key search, first match), a
// living one leaves the bookkeeping (BotInfo::remove); then its record is deleted.
void SAI::removeBot(Bot* bot)
{
    const int id = bot->getBotId();
    if (!m_bots[id])
        return;
    std::map<int, std::pair<int, int> >::iterator it = m_map5C.begin();
    for (; it != m_map5C.end(); ++it)
        if (it->first == id)
            break;
    if (it != m_map5C.end())
        m_map5C.erase(it);
    else
        m_bots[id]->remove(this);
    delete m_bots[id];
    m_bots[id] = 0;
}

// Both test the requested bit; x86 masks shift count.
bool SAI::isEnemy(int side) const
{
    return (static_cast<unsigned int>(m_enemyMask) & (1u << (side & 31))) != 0;
}

// the same bit test on the friendly mask.
bool SAI::isFriend(int side) const
{
    return (static_cast<unsigned int>(m_sideMask) & (1u << (side & 31))) != 0;
}

// ELF 0x329450 / 0x329460 / 0x329470 / 0x329480: `lea 0x12C/0x144/0x15C/0x174(this)`; the
// index argument is ignored. The "types" rows hold the float counts the SAI accumulates
// (`+= 1.0f`), so they are returned as float* (the shared declaration said int*).
float* SAI::getEnemyStrengths(int) const
{
    return const_cast<float*>(m_enemyStrengths);
}

float* SAI::getEnemyTypes(int) const
{
    return const_cast<float*>(m_enemyCounts);
}

float* SAI::getFriendlyStrengths(int) const
{
    return const_cast<float*>(m_friendlyStrengths);
}

float* SAI::getFriendlyTypes(int) const
{
    return const_cast<float*>(m_friendlyCounts);
}

// ELF 0x32D5F0: the bot's squad member (by player id, IBotManager::getBot +0x14) forgets its
// path, then handleChangingBot.
void SAI::setBotHasChangedEquipment(int botId)
{
    Bot* const bot = IBotManager::instance->getBot(botId);
    SquadMember* const member = SquadManager::m_instance->getSquadMember(bot->getPlayerId(), m_side);
    if (member)
        member->setHavePath(false);
    handleChangingBot(botId);
}

// ELF 0x328FE0: a bot that is not in the free set gives up its assignment - an assigned object
// (m_currentlyOrderedSquare) is left through removeAssigned(release) + free(false), else a current object
// (m_currentlyOccupiedSquare) through free(true). Its last ordered square becomes -1, and when its current
// equipment's closest strategic object differs from the current one it moves there (occupy).
void SAI::handleChangingBot(int botId)
{
    if (!m_bots[botId])
        return;
    if (m_freeBots.find(botId) == m_freeBots.end())
    {
        BotInfo* const info = m_bots[botId];
        if (info->m_currentlyOrderedSquare != -1)
        {
            AIStrategicObject* const object =
                AIStrategicObjectManager::getInstance()->getStrategicObject(info->m_currentlyOrderedSquare);
            m_bots[botId]->removeAssigned(object, true, this);
            m_bots[botId]->free(false, this);
        }
        else if (info->m_currentlyOccupiedSquare != -1)
        {
            info->free(true, this);
        }
    }
    m_bots[botId]->setLastOrderedSquare(-1);
    Information* const information =
        informationManager->getInformation(m_bots[botId]->m_bot->getCurrentEquipment());
    if (!information || !information->getUnit())
        return;
    IPIMobile* const mobile = information->getMobile();
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    int vehicle = -1;
    if (mobile)
        vehicle = mobile->getVehicleNumber();
    const int equipment = information->getUnit()->getEquipmentType();
    AIStrategicObject* const closest =
        manager->getClosestObject(information->getPosition(), equipment, vehicle, m_side);
    if (closest->getIndex() == m_bots[botId]->m_currentlyOccupiedSquare)
        return;
    m_bots[botId]->occupy(manager->getStrategicObject(m_bots[botId]->m_currentlyOccupiedSquare), closest, this);
}

// find, update the second member of the existing pair, or do nothing.
// Missing bot IDs are not inserted. The first member of the pair is left untouched.
void SAI::setNewBotKit(int botId, int kit)
{
    std::map<int, std::pair<int, int> >::iterator it = m_map5C.find(botId);
    if (it != m_map5C.end())
        it->second.second = kit;
}

// return the member map's address
// at +0x5C / +0x68. The caller copies the map; this getter neither copies nor mutates it.
// [INFERRED] const-reference spelling, as documented on the shared declaration.
const std::map<int, std::pair<int, int> >& SAI::getDeadBots() const
{
    return m_map5C;
}

// ELF 0x326CC0: a player giving up the commander seat (true -> false) unassigns the side's
// squads and restarts the commander timers: +0x68/+0x70/+0x78 = now + 60, +0x80 = now + 10.
void SAI::setPlayerIsCommander(bool isCommander)
{
    if (!isCommander && m_flag1D1)
    {
        SquadManager::m_instance->unAssignSquads(m_side);
        const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        m_time68 = now + 60.0f;
        m_time70 = now + 60.0f;
        m_time78 = now + 60.0f;
        m_time80 = now + 10.0f;
    }
    m_flag1D1 = isCommander;
}

// byte getter at +0x1D1 / +0x221.
bool SAI::isPlayerCommander()
{
    return m_flag1D1;
}

// ELF 0x327420. Ignored while a player commands the side. Without artillery (IBFCommander +0x38
// gameHasArtillery, +0x3C anyArtilleryPiecesLeft) an "unavailable" answer (type 2) is queued; with
// the artillery timer (+0x68) running a denial (type 1). Otherwise the timer re-arms 60 s ahead, the
// engine commander fires (+0x24, the request's x/z) and the squads are warned.
// [CHOSEN, not recovered] the shipped body never materialises a return value (eax is whatever the
// last call left); the courier passes it on and the engine ignores it. This build answers false.
bool SAI::playerRequestsArtillery(int playerId, Vec3 position)
{
    if (m_flag1D1)
        return false;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    IBFCommander* const commander = static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander);
    if (commander->gameHasArtillery(m_side) && commander->anyArtilleryPiecesLeft(m_side))
    {
        if (now > m_time68)
        {
            m_time68 = now + 60.0f;
            Vec2 xz;
            xz.x = position.x;
            xz.y = position.z;
            commander->fireArtillery(m_side, playerId, xz);
            SquadManager::m_instance->incomingArtillery(position, m_side);
        }
        else
        {
            DenyRequest* const request = new DenyRequest();
            request->m_type = 1;
            request->m_deadline = now + 2.0f + static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f * 3.0f;
            request->m_playerId = playerId;
            m_denyRequests.push_back(request);
        }
    }
    else
    {
        DenyRequest* const request = new DenyRequest();
        request->m_type = 2;
        request->m_deadline = now + 2.0f + static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f * 3.0f;
        request->m_playerId = playerId;
        m_denyRequests.push_back(request);
    }
    return false;
}

// ELF 0x327720. Ignored while a player commands the side; the UAV timer (+0x80) re-arms 30 s
// ahead and the engine commander sends it (+0x28), otherwise a denial (type 3) is queued.
// [CHOSEN, not recovered] no shipped return value; see playerRequestsArtillery.
bool SAI::playerRequestsUav(int playerId, Vec3 position)
{
    if (m_flag1D1)
        return false;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now > m_time80)
    {
        m_time80 = now + 30.0f;
        Vec2 xz;
        xz.x = position.x;
        xz.y = position.z;
        static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->sendUav(m_side, playerId, xz);
    }
    else
    {
        DenyRequest* const request = new DenyRequest();
        request->m_type = 3;
        request->m_deadline = now + 2.0f + static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f * 3.0f;
        request->m_playerId = playerId;
        m_denyRequests.push_back(request);
    }
    return false;
}

// ELF 0x3278C0. As the UAV request with the supply timer (+0x70, 60 s), dropSupply (+0x2C) and
// denial type 0. [CHOSEN, not recovered] no shipped return value; see playerRequestsArtillery.
bool SAI::playerRequestsSupply(int playerId, Vec3 position)
{
    if (m_flag1D1)
        return false;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now > m_time70)
    {
        m_time70 = now + 60.0f;
        Vec2 xz;
        xz.x = position.x;
        xz.y = position.z;
        static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->dropSupply(m_side, playerId, xz);
    }
    else
    {
        DenyRequest* const request = new DenyRequest();
        request->m_type = 0;
        request->m_deadline = now + 2.0f + static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f * 3.0f;
        request->m_playerId = playerId;
        m_denyRequests.push_back(request);
    }
    return false;
}

// ELF 0x327A60. As the UAV request with the vehicle-drop timer (+0x78, 60 s), dropVehicle (+0x30)
// and denial type 4. [CHOSEN, not recovered] no shipped return value; see playerRequestsArtillery.
bool SAI::playerRequestsVehicleDrop(int playerId, Vec3 position)
{
    if (m_flag1D1)
        return false;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now > m_time78)
    {
        m_time78 = now + 60.0f;
        Vec2 xz;
        xz.x = position.x;
        xz.y = position.z;
        static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->dropVehicle(m_side, playerId, xz);
    }
    else
    {
        DenyRequest* const request = new DenyRequest();
        request->m_type = 4;
        request->m_deadline = now + 2.0f + static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f * 3.0f;
        request->m_playerId = playerId;
        m_denyRequests.push_back(request);
    }
    return false;
}

void SAI::updateRequestDenyTimers()
{
    // Cancellation deliberately retains the shipped
    // invalid-iterator defect: erases, then reads the erased
    // node; frees it, then reads [edi]. A nonempty queue
    // therefore makes the following ++it unsafe. Do not turn this into erase(it++).
    if (m_flag1D1)
    {
        for (auto it = m_denyRequests.begin(); it != m_denyRequests.end(); ++it)
        {
            ::operator delete(*it);
            m_denyRequests.erase(it);
        }
        return;
    }

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    for (auto it = m_denyRequests.begin(); it != m_denyRequests.end(); )
    {
        // The comparison is ordered and strict: equality and NaN retain the request.
        if (now > (*it)->m_deadline)
        {
            switch ((*it)->m_type)
            {
            case 0:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    denySupplyRequest(m_side, (*it)->m_playerId);
                break;
            case 1:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    denyArtilleryRequest(m_side, (*it)->m_playerId);
                break;
            case 2:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    ArtilleryUnavailable(m_side, (*it)->m_playerId);
                break;
            case 3:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    denyUavRequest(m_side, (*it)->m_playerId);
                break;
            case 4:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    denyVehicleDropRequest(m_side, (*it)->m_playerId);
                break;
            }
            // Both ports reload the payload after the callback, then advance before erase.
            ::operator delete(*it);
            m_denyRequests.erase(it++);
        }
        else
            ++it;
    }
}

// ELF 0x326D60. The AI commander fires artillery when the player is not the commander, the game
// has more than one bot slot, the side has artillery pieces left and both timers (+0x68, +0x88)
// have expired. +0x88 is re-armed 10 s ahead first. A base under hostile takeover (virtual +0x6C)
// is shelled at a random point unless friendlies are within the safe distance or the 1-in-10 roll
// prefers camping humans; otherwise camping humans are the target. Firing re-arms +0x68 60 s
// ahead, asks the engine commander (+0x24 fireArtillery(side, -1, xz)) and warns the squads.
// [CHOSEN, not recovered] the roll is compared in double; the ELF compares the x87 product.
void SAI::checkCommanderUseArtillery()
{
    bool singleSlot = false;
    if (CommonSingletonServer::m_iaiSettings)
        singleSlot = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots() - 1 <= 0;
    if (m_flag1D1 || singleSlot)
        return;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    IBFCommander* const commander = static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander);
    if (!commander->gameHasArtillery(m_side) || !commander->anyArtilleryPiecesLeft(m_side))
        return;
    if (!(now > m_time68) || !(now > m_time88))
        return;
    m_time88 = now + 10.0f;
    SquadManager* const squads = SquadManager::m_instance;
    Vec2 position;
    position.x = 0.0f;
    position.y = 0.0f;
    AIStrategicObject* base = 0;
    if (hostileTakeoveInProgress(base, position))
    {
        const double roll = static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f;
        if (roll > 0.1f)
        {
            IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
            Vec3 target;
            target.x = position.x;
            target.y = terrain->getTerrainAltitude(position.x, position.y);
            target.z = position.y;
            const float safe = squads->getArtillerySafeDistance();
            if (!squads->friendliesCloseTo(target, safe, m_side))
            {
                m_time68 = now + 60.0f;
                commander->fireArtillery(m_side, -1, position);
                squads->incomingArtillery(target, m_side);
                return;
            }
        }
    }
    if (!squads->haveCampingHumans(m_side, position))
        return;
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    Vec3 target;
    target.x = position.x;
    target.y = terrain->getTerrainAltitude(position.x, position.y);
    target.z = position.y;
    m_time68 = now + 60.0f;
    commander->fireArtillery(m_side, -1, position);
    squads->incomingArtillery(target, m_side);
}

// ELF 0x327210. The first strategic object that is a control point (hasObjectTypeFlag(16)),
// being taken over by the enemy of this side and with a takeover counter below 0.75 is returned
// in base; position becomes a uniform random point of the box around its position (slot 3),
// mirrored at the area centre (+0x110): centre + 2 (position - centre) u, x then y.
bool SAI::hostileTakeoveInProgress(AIStrategicObject*& base, Vec2& position)
{
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        AIStrategicObject* const object = *it;
        if (!object || !object->hasObjectTypeFlag(static_cast<AIStrategicObject::StrategicObjectType>(16)))
            continue;
        if (!object->isEnemyTakingOverBase(m_side))
            continue;
        if (!(0.75f > object->getControlPointTakeOverCounter()))
            continue;
        base = object;
        const Vec2 centre = static_cast<AIStrategicArea*>(object)->m_centre;
        const Vec2 corner = object->getPosition(m_side);
        const double u = static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f;
        position.x = static_cast<float>(centre.x + (static_cast<double>(corner.x) - centre.x) * 2.0 * u);
        const double v = static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f;
        position.y = static_cast<float>(centre.y + (static_cast<double>(corner.y) - centre.y) * 2.0 * v);
        return true;
    }
    return false;
}

// ELF 0x3270A0: the closest object (getClosestObject(pos, side)) must be an owned control point
// (slot 19, type 16). Its control point (area +0x13C) gives centre and radius through IAIGame
// (+0x28); a positive maxRadius above that radius replaces it (`fcmovbe`). Inside means a squared
// 3-D distance below the squared radius and an active point; then object is the closest one.
bool SAI::isPosInsideOwnedControlPoint(AIStrategicObject*& object, Vec3 position, float maxRadius)
{
    bool inside = false;
    AIStrategicObject* const closest = AIStrategicObjectManager::getInstance()->getClosestObject(position, m_side);
    if (!closest->hasObjectTypeFlag(AIStrategicObject::ControlPoint) || closest->getStatus(m_side) != StatusMap::Owned)
        return inside;
    float radius = 0.0f;
    Vec3 centre;
    centre.x = 0.0f;
    centre.y = 0.0f;
    centre.z = 0.0f;
    bool active;
    static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getControlPointInfo(
        static_cast<AIStrategicArea*>(closest)->m_controlPointId, centre, radius, active);
    if (maxRadius > 0.0f)
        radius = maxRadius <= radius ? radius : maxRadius;
    radius = radius * radius;
    const float dx = centre.x - position.x;
    const float dy = centre.y - position.y;
    const float dz = centre.z - position.z;
    const float distance = dx * dx + dy * dy + dz * dz;
    if (active && radius > distance)
    {
        inside = true;
        object = closest;
    }
    return inside;
}

// ELF 0x328590: any strategic object (not only control points) the side owns.
bool SAI::haveAnyOwnedCps(int side)
{
    bool owned = false;
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if (*it && (*it)->getStatus(side) == StatusMap::Owned)
        {
            owned = true;
            break;
        }
    }
    return owned;
}

// ELF 0x3285F0: an owned object whose control point (area +0x13C, -1 for none) the game reports
// as active. The position and radius outputs are zeroed once, before the loop.
bool SAI::haveAnyOwnedAndTakeableCps(int side)
{
    bool found = false;
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    float radius = 0.0f;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        AIStrategicObject* const object = *it;
        if (!object || object->getStatus(side) != StatusMap::Owned)
            continue;
        bool active = false;
        const unsigned int cp = static_cast<AIStrategicArea*>(object)->m_controlPointId;
        if (cp != static_cast<unsigned int>(-1))
            static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getControlPointInfo(cp, position, radius, active);
        if (active)
        {
            found = true;
            break;
        }
    }
    return found;
}

// ELF 0x3286C0: false as soon as an active control point is not owned by the side.
bool SAI::ownAllTakeableCps(int side)
{
    bool all = true;
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    float radius = 0.0f;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        AIStrategicObject* const object = *it;
        if (!object || object->getStatus(side) == StatusMap::Owned)
            continue;
        bool active = false;
        const unsigned int cp = static_cast<AIStrategicArea*>(object)->m_controlPointId;
        if (cp != static_cast<unsigned int>(-1))
            static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getControlPointInfo(cp, position, radius, active);
        if (active)
        {
            all = false;
            break;
        }
    }
    return all;
}

// preserve unAssignSquads' full integer return.
// The caller's full-EAX additions establish the count contract; the body remains stopped.
// ELF 0x32D680: the object's index (+0xF0) and the side go to SquadManager::unAssignSquads.
int SAI::releaseSquadsFromObject(int object, int side)
{
    AIStrategicObject* const target = AIStrategicObjectManager::getInstance()->getStrategicObject(object);
    return SquadManager::m_instance->unAssignSquads(target->getIndex(), side);
}

// ELF 0x32D6D0: the unit leaves the object's assigned set and SAI's occupied set, then
// m_bots[*it]->free(false) runs - with the iterator removeAssigned has already advanced, so
// the image frees the *next* assigned unit (`movl (%esi),%eax` reloads it after the call).
// [CHOSEN, not recovered] When the released unit was the last one, the image reads the old
// libstdc++ header node's stale value as a bot index; here that second call is skipped.
// The side argument is unused; the image answers 1.
bool SAI::releaseUnitFromObject(int object, std::set<int>::iterator& it, int)
{
    AIStrategicObject* const target = AIStrategicObjectManager::getInstance()->getStrategicObject(object);
    m_bots[*it]->removeAssigned(target, it, true, this);
    if (it != target->getAssignedUnits(m_side).end())
        m_bots[*it]->free(false, this);
    return true;
}

// ELF 0x329250: erase the bot from the set at +0xF0; answers whether it was there.
bool SAI::foundDeadBot(int botId)
{
    std::set<int>::iterator it = m_setF0.find(botId);
    if (it == m_setF0.end())
        return false;
    m_setF0.erase(it);
    return true;
}

// ELF 0x32D770: membership in the set at +0xB0.
bool SAI::hasChangedEquipment(int botId)
{
    return m_setB0.find(botId) != m_setB0.end();
}

// ELF 0x32D7D0: erase from the set at +0xB0 when present.
void SAI::equipmentChangedFinished(int botId)
{
    std::set<int>::iterator it = m_setB0.find(botId);
    if (it != m_setB0.end())
        m_setB0.erase(it);
}

// ELF 0x32D870: answers the bot's current object index (m_currentlyOccupiedSquare, read first).
int SAI::assignBot(std::set<int>::iterator& it, int object, int botId, bool attacker)
{
    const int current = m_bots[botId]->m_currentlyOccupiedSquare;
    AIStrategicObject* const target = AIStrategicObjectManager::getInstance()->getStrategicObject(object);
    m_bots[botId]->assignToObject(it, target, attacker, this);
    return current;
}

// ELF 0x32E830: the byte at +0x1CF.
bool SAI::strategicObjectsHasChanged()
{
    return m_flag1CF;
}

// ELF 0x32E840: the id is appended to the teleporter list (+0x110).
void SAI::registerTeleporter(int id)
{
    m_list110.push_back(id);
}

// ELF 0x32E380: the container's first two floats (last distribution score, latest score); zero for
// an index past the end.
Vec2 SAI::getStrategyHeat(unsigned int index)
{
    Vec2 heat;
    heat.x = 0.0f;
    heat.y = 0.0f;
    if (index < m_strategies.size())
    {
        heat.x = m_strategies[index].m_f00;
        heat.y = m_strategies[index].m_f04;
    }
    return heat;
}

// ELF 0x32E420: one "<index> <name>" line per container that has a strategy.
std::string SAI::listStrategies()
{
    std::ostringstream out;
    for (unsigned int i = 0; i < m_strategies.size(); ++i)
    {
        if (m_strategies[i].strategy)
            out << i << " " << m_strategies[i].strategy->getName() << std::endl;
    }
    return out.str();
}

// ELF 0x32EA90: the active container's strategy, unguarded.
Strategy* SAI::getActiveStrategy()
{
    return m_activeStrategy->strategy;
}

// ELF 0x32EAB0: the active container's three floats (zero, with no active container) and its
// strategy. The strategy is read even when the cursor is at end(), as shipped.
Strategy* SAI::getActiveStrategy(float& a, float& b, float& c)
{
    if (m_activeStrategy != m_strategies.data() + m_strategies.size())
    {
        a = m_activeStrategy->m_f00;
        b = m_activeStrategy->m_f04;
        c = m_activeStrategy->m_startTime;
    }
    else
    {
        a = 0.0f;
        b = 0.0f;
        c = 0.0f;
    }
    return m_activeStrategy->strategy;
}

// ELF 0x32E2D0: membership in the free set (+0x44).
bool SAI::isBotFree(int botId) const
{
    return m_freeBots.find(botId) != m_freeBots.end();
}

// ELF 0x32E320: membership in the occupied set (+0xFC).
bool SAI::isBotAssigned(int botId) const
{
    return m_assignedBots.find(botId) != m_assignedBots.end();
}

// ELF 0x32CEE0: empty.
void SAI::updateMap(double)
{
}

// ELF 0x329490: one 1.0f modifier per behaviour (IAISettings +0xC getNumberOfBehaviours); the
// caller owns the array.
float* SAI::createUnitMods() const
{
    const int count = static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNumberOfBehaviours();
    float* const mods = checkPointer(new float[count]);
    for (int i = 0; i < count; ++i)
        mods[i] = 1.0f;
    return mods;
}

// insert (point,1), increment only on duplicate.
// The native map lives at retail ELF +0x1D8 / PE +0x228; no key validation.
void SAI::addSpawnToPoint(int point)
{
    const std::pair<std::map<int, int>::iterator, bool> inserted =
        m_map1D8.insert(std::make_pair(point, 1));
    if (!inserted.second)
        ++inserted.first->second;
}

// ELF 0x32EF90: the spawn count of the point (map +0x1D8), 0 when unknown.
int SAI::getSpawnsToPoint(int point) const
{
    std::map<int, int>::const_iterator it = m_map1D8.find(point);
    if (it == m_map1D8.end())
        return 0;
    return it->second;
}

// ELF 0x32E240 / 0x32E270 / 0x32E2A0: the bot's current object (m_currentlyOccupiedSquare), ordered object
// (m_currentlyOrderedSquare) and last ordered square (m_lastOrderedSquare) - indices, not positions (eax, no sret). An unknown
// bot answers -22, -44 and -1 (`movl $0xffffffea/$0xffffffd4/$0xffffffff,%edx`).
int SAI::getBotCurrentPos(int botId) const
{
    const BotInfo* const info = m_bots[botId];
    return info ? info->m_currentlyOccupiedSquare : -22;
}

int SAI::getBotOrderedPos(int botId) const
{
    const BotInfo* const info = m_bots[botId];
    return info ? info->m_currentlyOrderedSquare : -44;
}

int SAI::getBotLastOrderedPos(int botId) const
{
    const BotInfo* const info = m_bots[botId];
    return info ? info->m_lastOrderedSquare : -1;
}

// Accumulate into the previous totals, then decay; no initial clear.
// Nonempty information can still reach the named Information/IPIUnit forwarding frontiers.
void SAI::updateStrengths()
{
    std::vector<unsigned int> handles;
    UnitPredicate* predicate = checkPointer(new UnitPredicate);
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)->
        getAllInformation(handles, m_side, true, predicate, nullptr);

#if defined(_MSC_VER)
    // [PLATFORM] PE retains the last secondary lookup's manager for the next root lookup.
    // ELF reloads the global at the start of each root, and before each secondary callback.
    InformationManager* manager = handles.empty() ? nullptr : informationManager;
#endif
    for (auto it = handles.begin(); it != handles.end(); ++it)
    {
#if !defined(_MSC_VER)
        InformationManager* manager = informationManager;
#endif
        Information* root = manager->getInformation(*it);
        int secondary = root->getFirstSecondaryObjectIterator();
        const int rootSide = root->getSide();
        const bool friendly = rootSide == m_side;
        float* strengths = friendly ? m_friendlyStrengths : m_enemyStrengths;
        float* counts = friendly ? m_friendlyCounts : m_enemyCounts;
        Information* current = root;
        do
        {
            // Keep the selected unit across isOccupied/getSecurity, even if a callback replaces it.
            IPIUnit* unit = current->getUnit();
            if (unit->isOccupied())
            {
                const float security = current->getSecurity();
                for (int category = 0; category != 6; ++category)
                {
                    // Both shipped x87 paths retain the product through the add before storing
                    // float. Widen explicitly so modern SSE does not round the product early.
                    strengths[category] = static_cast<float>(
                        static_cast<double>(security) * unit->getBattleStrengths()[category]
                        + strengths[category]);
                }
                const int category = current->getPhysical()->getStrengthType();
                counts[category] += security;
            }

#if !defined(_MSC_VER)
            manager = informationManager;
#endif
            // Iteration always belongs to the original record, with the current SAI side.
            const unsigned int handle = root->getSecondaryObject(secondary, m_side);
#if defined(_MSC_VER)
            manager = informationManager;
#endif
            current = manager->getInformation(handle);
        } while (current);
    }

    for (int category = 0; category != 6; ++category)
    {
        if (m_friendlyStrengths[category] > 0.001f)
            m_friendlyStrengths[category] *= 0.5f;
        if (m_friendlyCounts[category] > 0.001f)
            m_friendlyCounts[category] *= 0.5f;
        if (m_enemyStrengths[category] > 0.001f)
            m_enemyStrengths[category] *= 0.5f;
        if (m_enemyCounts[category] > 0.001f)
            m_enemyCounts[category] *= 0.5f;
    }
    delete predicate;
}

// Updates strategic state in callback order with live vector iteration and repeated
// Route/Bridge tests. Non-area objects reuse the preceding area's output and border flag.
void SAI::updateStates(float* a, float* b)
{
    float* selected = 0;
    bool neighbour = false;
    const int enemySide = m_side == 1 ? 2 : 1;
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    a[13] = static_cast<float>(game->getTickets(m_side));
    b[13] = static_cast<float>(game->getTickets(enemySide));
    a[14] = static_cast<float>(game->getNumberOfControlPoints(m_side));
    b[14] = static_cast<float>(game->getNumberOfControlPoints(enemySide));

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    a[15] = static_cast<float>(now);
    b[15] = static_cast<float>(now);
    StrategyContainer* const end = m_strategies.empty()
        ? m_strategies.data() : m_strategies.data() + m_strategies.size();
    a[16] = m_activeStrategy == end ? 0.0f : static_cast<float>(now - m_activeStrategy->m_startTime);
    b[16] = 0.0f;
    a[17] = static_cast<float>(m_list1BC.size());
    a[18] = static_cast<float>(m_list1B8.size());
    a[19] = 0.0f; b[19] = 0.0f;
    a[20] = 0.0f; b[20] = 0.0f;
    a[21] = 0.0f; b[21] = 0.0f;
    a[39] = 0.0f; b[39] = 0.0f;
    a[38] = 0.0f; b[38] = 0.0f;
    b[41] = 0.0f; a[41] = 0.0f;
    a[40] = 0.0f; b[40] = 0.0f;
    a[42] = 0.0f; b[42] = 0.0f;

    std::set<int> attack;
    std::set<int> defence;
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if ((*it)->getType() == 0)
        {
            switch ((*it)->getStatus(m_side))
            {
            case StatusMap::Owned:
                a[19] += 1.0f;
                b[21] += 1.0f;
                selected = a;
                neighbour = (*it)->hasNeighbourWithStatus(StatusMap::Hostile, m_side);
                if (neighbour) { a[39] += 1.0f; b[41] += 1.0f; }
                else           { a[40] += 1.0f; b[42] += 1.0f; }
                break;
            case StatusMap::Hostile:
                a[21] += 1.0f;
                b[19] += 1.0f;
                selected = b;
                neighbour = (*it)->hasNeighbourWithStatus(StatusMap::Owned, m_side);
                if (neighbour) { b[39] += 1.0f; a[41] += 1.0f; }
                else           { b[40] += 1.0f; a[42] += 1.0f; }
                break;
            case StatusMap::Neutral:
                a[20] += 1.0f;
                b[20] += 1.0f;
                if ((*it)->hasNeighbouringObjectWithStatus(StatusMap::Owned, m_side))
                    a[38] += 1.0f;
                if ((*it)->hasNeighbouringObjectWithStatus(StatusMap::Hostile, m_side))
                    b[38] += 1.0f;
                selected = 0;
                break;
            default:
                selected = 0;
                neighbour = false;
                break;
            }
        }

        if ((*it)->getNumberOfAssignedUnits(m_side) > 0)
        {
            if ((*it)->isAttack(m_side))
                attack.insert((*it)->getIndex());
            else
                defence.insert((*it)->getIndex());
        }

        if (selected)
        {
            // Each test reloads *it, including after a preceding virtual callback replaced it.
            static const AIStrategicObject::StrategicObjectType areaFlags[] = {
                AIStrategicObject::Flank, AIStrategicObject::Base, AIStrategicObject::Close,
                AIStrategicObject::Centre, AIStrategicObject::Remote
            };
            for (int i = 0; i != 5; ++i)
            {
                if ((*it)->hasObjectTypeFlag(areaFlags[i]))
                {
                    selected[22 + i] += 1.0f;
                    if (neighbour)
                        selected[33 + i] += 1.0f;
                }
            }
            static const AIStrategicObject::StrategicObjectType otherFlags[] = {
                AIStrategicObject::Route, AIStrategicObject::Bridge,
                AIStrategicObject::Route, AIStrategicObject::Bridge,
                AIStrategicObject::North, AIStrategicObject::West,
                AIStrategicObject::South, AIStrategicObject::East
            };
            static const int indices[] = { 27, 28, 27, 28, 29, 30, 31, 32 };
            for (int i = 0; i != 8; ++i)
                if ((*it)->hasObjectTypeFlag(otherFlags[i]))
                    selected[indices[i]] += 1.0f;
        }
    }
    b[17] = static_cast<float>(attack.size());
    b[18] = static_cast<float>(defence.size());
}

// Prepares admitted bots and resource strengths. Admission retains its Bot pointer
// across callbacks; strength/removal traversals reload the element. Preserve all four
// id queries during removal.
void SAI::prepareUpdate()
{
    for (std::vector<Bot*>::iterator it = m_vector94.begin(); it != m_vector94.end(); ++it)
    {
        Bot* const bot = *it;
        if (bot)
        {
            BotInfo* const info = checkPointer(new BotInfo(bot));
            m_bots[bot->getBotId()] = info;
            if (bot->isDead())
            {
                m_map5C.insert(std::make_pair(static_cast<int>(bot->getBotId()), std::make_pair(-1, 13)));
                bot->getPlayer()->setSpawnGroup(0);
                bot->setIsInTransitionPhase(true);
            }
            else
                setBotIsAlive(bot->getBotId());
            (void)m_assignedBots.find(bot->getBotId()); // shipped discarded lookup
        }
    }

    // [PLATFORM] PE frees/nulls all three vector pointers; ELF erase retains capacity.
#if defined(_MSC_VER)
    std::vector<Bot*>().swap(m_vector94);
#else
    m_vector94.erase(m_vector94.begin(), m_vector94.end());
#endif
    clearSpawnsToPoints();
    for (std::list<int>::iterator it = m_list110.begin(); it != m_list110.end(); ++it)
        handleTeleportingBot(*it);
    m_list110.clear();

    for (int i = 0; i != 6; ++i)
    {
        m_friendlyStrengths[i] = 0.0f;
        m_friendlyCounts[i] = 0.0f;
    }
    for (std::vector<BotInfo*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        if (*it && !(*it)->m_bot->isDead())
        {
            // [PLATFORM] ELF captures the manager before the equipment callback. PE does so
            // only afterward, and does not read it at all when the handle's low half is zero.
#if !defined(_MSC_VER)
            InformationManager* const manager = informationManager;
#endif
            const unsigned int handle = (*it)->m_bot->getCurrentEquipment();
#if defined(_MSC_VER)
            if ((handle & 0xffffu) == 0)
                continue;
            InformationManager* const manager = informationManager;
#endif
            Information* const information = manager->getInformation(handle);
            if (information && information->getUnit())
            {
                IPIUnit* const unit = information->getUnit();
                // Assignment, not summation: the last live unit supplies all six strengths.
                for (int i = 0; i != 6; ++i)
                    m_friendlyStrengths[i] = unit->getBattleStrengths()[i];
                const int category = information->getPhysical()->getStrengthType();
                m_friendlyCounts[category] += 1.0f;
            }
        }
    }

    for (std::map<int, std::pair<int, int> >::iterator it = m_map5C.begin(); it != m_map5C.end();)
    {
        IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
        BotInfo* const info = m_bots[it->first];
        if (!info)
        {
            std::map<int, std::pair<int, int> >::iterator removed = it++;
            m_map5C.erase(removed);
        }
        else
        {
            Bot* const bot = info->m_bot;
            if (settings->isRespawnAllowed())
                BotSpawner::findSuitableSpawnGroup(m_enemyStrengths, m_enemyCounts,
                    m_friendlyStrengths, m_friendlyCounts, it->second.first, bot);
            ++it;
        }
    }

    for (std::vector<Bot*>::iterator it = m_vectorA0.begin(); it != m_vectorA0.end(); ++it)
    {
        if (m_bots[(*it)->getBotId()])
        {
            m_bots[(*it)->getBotId()]->remove(this);
            delete m_bots[(*it)->getBotId()];
            m_bots[(*it)->getBotId()] = 0;
        }
    }
#if defined(_MSC_VER)
    std::vector<Bot*>().swap(m_vectorA0);
#else
    m_vectorA0.erase(m_vectorA0.begin(), m_vectorA0.end());
#endif
}

// Reset mapped values
// retaining keys and nodes. The live native end is checked after each iterator advance.
void SAI::clearSpawnsToPoints()
{
    for (std::map<int, int>::iterator it = m_map1D8.begin(); it != m_map1D8.end(); ++it)
        it->second = 0;
}

void SAI::BotInfo::remove(SAI* sai)
{
    const int id = m_bot->getBotId();
    AIStrategicObject* const current = AIStrategicObjectManager::getInstance()->getStrategicObject(m_currentlyOccupiedSquare);
    AIStrategicObject* const assigned = AIStrategicObjectManager::getInstance()->getStrategicObject(m_currentlyOrderedSquare);
    if (assigned)
    {
        removeAssigned(assigned, true, sai);
        current->removeAsPresentUnit(id, sai->m_side);
    }
    else if (current)
    {
        if (sai->m_assignedBots.find(id) != sai->m_assignedBots.end())
            free(true, sai);
        current->removeAsPresentUnit(id, sai->m_side);
    }
    std::set<int>::iterator it = sai->m_freeBots.find(id);
    if (it != sai->m_freeBots.end())
        sai->m_freeBots.erase(it);
}

void SAI::BotInfo::removeAssigned(AIStrategicObject* object, bool release, SAI* sai)
{
    std::set<int>::iterator it = object->getAssignedUnits(sai->m_side).find(m_bot->getBotId());
    removeAssigned(object, it, release, sai);
}

void SAI::BotInfo::occupy(AIStrategicObject* from, AIStrategicObject* to, SAI* sai)
{
    std::set<int>::iterator it = from->getPresentUnits(sai->m_side).find(m_bot->getBotId());
    occupy(it, from, to, sai);
}

void SAI::BotInfo::assignToObject(std::set<int>::iterator& it, AIStrategicObject* object, bool attacker, SAI* sai)
{
    m_currentlyOrderedSquare = object->getIndex();
    m_lastOrderedSquare = object->getIndex();
    m_double14 = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    AIStrategicObject* const current = AIStrategicObjectManager::getInstance()->getStrategicObject(m_currentlyOccupiedSquare);
    if (attacker)
        current->unregisterAvailableAttacker(it, sai->m_side);
    else
        current->unregisterAvailableDefender(it, sai->m_side);
    object->getAssignedUnits(sai->m_side).insert(m_bot->getBotId());
}

void SAI::BotInfo::assignToObject(AIStrategicObject* object, bool attacker, SAI* sai)
{
    std::set<int>::iterator it = AIStrategicObjectManager::getInstance()->getStrategicObject(m_currentlyOccupiedSquare)->
        getAvailableBots(attacker, sai->m_side).find(m_bot->getBotId());
    assignToObject(it, object, attacker, sai);
}

void SAI::BotInfo::assign(bool attacker, SAI* sai)
{
    std::set<int>::iterator it = sai->m_freeBots.find(m_bot->getBotId());
    assign(it, attacker, sai);
}

void SAI::BotInfo::setAsDead(SAI*)
{
    m_currentlyOrderedSquare = -1;
    m_lastOrderedSquare = -1;
    m_currentlyOccupiedSquare = -1;
    m_float10 = 0.0f;
}

void SAI::BotInfo::setAsFree(SAI* sai)
{
    sai->m_freeBots.insert(m_bot->getBotId());
}

// ELF 0x328EF0: with a suitable spawn group the bot enters its transition phase (+0x180),
// is handled as dead, gets the group and is respawned through the game effector (+0x1C).
void SAI::handleTeleportingBot(int botId)
{
    if (!m_bots[botId])
        return;
    AIStrategicObject* object;
    const int group = BotSpawner::findSuitableSpawnGroup(m_side, object, m_bots[botId]->m_bot);
    if (!group)
        return;
    m_bots[botId]->m_bot->setIsInTransitionPhase(true);
    handleDeadBot(botId);
    m_bots[botId]->m_bot->getPlayer()->setSpawnGroup(group);
    static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)
        ->respawnBot(m_bots[botId]->m_bot->getPlayer());
}

bool SAI::updateStrategicObjects()
{
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    bool changed = false;
    if (m_flag1D0)
    {
        m_flag1CF = false;
        m_flag1D0 = false;
        m_strategicObjectIterator = objects.begin();
        m_enemyStats->clear();
        m_friendlyStats->clear();
    }

    // Both ports dereference unconditionally and process exactly one object. The concrete
    // update returns void. There is no empty-vector guard or inner budget check.
    (*m_strategicObjectIterator)->update(m_side, this, changed);
    m_flag1CF = m_flag1CF || changed;

    // The callback may change the cursor/entry/side. Capture status before the getter
    // then load the selected destination; the shipped numeric split is zero/nonzero.
    AIStrategicObject* const object = *m_strategicObjectIterator;
    const int side = m_side;
    const StatusMap::Status status = object->getStatus(side);
    EnemyStatistics& statistics = object->getBattleStatistics(side);
    if (status == 0)
        m_enemyStats->add(statistics);
    else
        m_friendlyStats->add(statistics);

    ++m_strategicObjectIterator;
    if (m_strategicObjectIterator != objects.end())
        return false;

    if (m_flag1CF)
    {
        // Retain this invocation's vector, but reload its live end, entry and side.
        for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin();
             it != objects.end(); ++it)
        {
            (*it)->setAsDirty(m_side);
            (*it)->releaseAssignedResources(m_side);
        }
    }
    m_flag1D0 = true;
    return true;
}

// ELF 0x329A10. A strategy forced for this side (StrategyManager::getForcedStrategy) is scored
// once (result discarded) and activated. Otherwise every strategy is re-scored into m_f04
// (clamped at 0) against the latest friendly/enemy state arrays; when a score moved by 17% or
// more against the last distribution's m_f00 (ratio <= 0.83 or >= 1.2), or when the active
// strategy has outlived m_f20 * (2 - 2 * 0.5^m_i0C) seconds (then m_i0C counts up), the scores
// are copied to m_f00 and a strategy is drawn at random, weighted by its positive score.
// shipped defect: an empty strategy list still dereferences the end() active-strategy cursor.
void SAI::chooseStrategy()
{
    float total = 0.0f;
    bool changed = false;
    StrategyManager::getInstance()->isDebugEnabled();   // empty debug branch in the image
    Strategy* const forced = StrategyManager::getInstance()->getForcedStrategy(m_side);
    if (forced)
    {
        forced->calculatePrerequisite(m_int1F4 ? m_stateB : m_stateA, m_int1F4 ? m_stateD : m_stateC,
                                      m_activeStrategy->strategy, m_side);
        for (std::vector<StrategyContainer>::iterator it = m_strategies.begin(); it != m_strategies.end(); ++it)
            if (it->strategy == forced)
                changeStrategy(it);
        return;
    }
    for (std::vector<StrategyContainer>::iterator it = m_strategies.begin(); it != m_strategies.end(); ++it)
    {
        it->m_f04 = it->strategy->calculatePrerequisite(
            m_int1F4 ? m_stateB : m_stateA, m_int1F4 ? m_stateD : m_stateC,
            m_activeStrategy->strategy, m_side);
        if (total > it->m_f04)              // total is still 0 here
            it->m_f04 = total;
        float ratio;
        if (it->m_f04 != 0.0f)
            ratio = it->m_f00 / it->m_f04;
        else
            ratio = it->m_f00 == 0.0f ? 1.0f : 0.0f;
        if (ratio <= 0.83 || ratio >= 1.2)
            changed = true;
    }
    if (!changed)
    {
        const float limit = m_activeStrategy->strategy->m_f20;
        const float p = powf(0.5f, static_cast<float>(m_activeStrategy->m_i0C));
        const float d = 0.5f - p;
        const double threshold = ((d + d) + 1.0f) * limit;
        const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        if (!(now - m_activeStrategy->m_startTime >= threshold))
            return;
        ++m_activeStrategy->m_i0C;
    }

    for (std::vector<StrategyContainer>::iterator it = m_strategies.begin(); it != m_strategies.end(); ++it)
    {
        it->m_f00 = it->m_f04;
        total += it->m_f04 > 0.0f ? it->m_f04 : 0.0f;
    }
    float pick = static_cast<float>(AIRandom::m_instance->next()) * 2.3283064365386963e-10f * total;
    std::vector<StrategyContainer>::iterator it = m_strategies.begin();
    for (; it != m_strategies.end(); ++it)
    {
        pick -= it->m_f00 > 0.0f ? it->m_f00 : 0.0f;
        if (0.0 >= pick)
            break;
    }
    if (it == m_strategies.end())
        --it;
    changeStrategy(it);
}

// ELF 0x329610. Activating another strategy releases every object's assigned resources and
// frees all its available attackers and defenders; the new strategy's clock restarts.
void SAI::changeStrategy(std::vector<StrategyContainer>::iterator& it)
{
    if (m_activeStrategy == &*it)
        return;
    m_activeStrategy = &*it;
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    std::set<int>::iterator unit;
    for (std::vector<AIStrategicObject*>::const_iterator object = objects.begin(); object != objects.end(); ++object)
    {
        (*object)->releaseAssignedResources(m_side);
        std::set<int>& attackers = (*object)->getAvailableAttackers(m_side);
        for (unit = attackers.begin(); unit != attackers.end();)
            m_bots[*unit]->free(*object, true, unit, this);
        std::set<int>& defenders = (*object)->getAvailableDefenders(m_side);
        for (unit = defenders.begin(); unit != defenders.end();)
            m_bots[*unit]->free(*object, false, unit, this);
    }
    StrategyContainer* const active = m_activeStrategy;
    active->m_i0C = 1;
    active->m_startTime = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

// ELF 0x32FDF0. The occupied-set erase uses find() unchecked, as shipped.
void SAI::BotInfo::free(AIStrategicObject* object, bool attacker, std::set<int>::iterator& it, SAI* sai)
{
    const int id = m_bot->getBotId();
    if (attacker)
        object->unregisterAvailableAttacker(it, sai->m_side);
    else
        object->unregisterAvailableDefender(it, sai->m_side);
    sai->m_assignedBots.erase(sai->m_assignedBots.find(id));
    sai->m_freeBots.insert(m_bot->getBotId());
}

// ELF 0x3299A0: every strategic object (no null check) applies the active strategy's
// modifiers; the returned temperature is discarded.
void SAI::modifyStrategicObjectsTemperatures()
{
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
        (*it)->updateTemperature(m_activeStrategy->strategy, m_side);
}

// ELF 0x32F290. The occupied-set find result is unused in the image.
void SAI::BotInfo::occupy(std::set<int>::iterator& it, AIStrategicObject* from, AIStrategicObject* to, SAI* sai)
{
    const int id = m_bot->getBotId();
    to->setAsPresentUnit(id, sai->m_side);
    bool moved = false;
    if (from->unregisterAvailableAttacker(id, sai->m_side))
    {
        to->registerAvailableAttacker(id, sai->m_side);
        moved = true;
    }
    else if (from->unregisterAvailableDefender(id, sai->m_side))
    {
        to->registerAvailableDefender(id, sai->m_side);
        moved = true;
    }
    if (moved)
        sai->m_assignedBots.find(id);
    from->removeAsPresentUnit(it, sai->m_side);
    m_currentlyOccupiedSquare = to->getIndex();
}

// ELF 0x32D910. Per strategic area (getType 0): a present bot whose equipment fits a neighbour
// (+0x128 objects: allowed vehicle group, not expelled, an area, not a landing-zone user) with a
// stronger gravity at the bot's position moves there. Then the area's assigned units and its
// available attackers and defenders are retained (slot 28) when they have reached a MoveTo
// waypoint, sense at most one object, are present and their last order is older than 20 s on
// foot or 35 s in a vehicle.
void SAI::updateBotPositions()
{
    std::set<int>::iterator unit;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    IBotManager* const bots = IBotManager::instance;
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator object = objects.begin(); object != objects.end(); ++object)
    {
        std::set<int>& present = (*object)->getPresentUnits(m_side);
        if ((*object)->getType() != 0)
            continue;
        const std::vector<AIStrategicObject*>& neighbours =
            static_cast<AIStrategicArea*>(*object)->m_neighbouringObjects;
        for (unit = present.begin(); unit != present.end();)
        {
            Bot* const bot = bots->getBotFromId(*unit);
            Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
            if (!information || !information->getUnit())
            {
                ++unit;
                continue;
            }
            const int equipment = information->getUnit()->getEquipmentType();
            const int group = settings->getVehicleGroup(equipment);
            AIStrategicObject* best = *object;
            float bestGravity = best->gravity(information->getPosition(), m_side);
            for (std::vector<AIStrategicObject*>::const_iterator n = neighbours.begin(); n != neighbours.end(); ++n)
            {
                if (!(*n)->isAllowedVehicleGroup(group))
                    continue;
                if ((*n)->isExpelledUnit(equipment) || (*n)->getType() != 0
                    || static_cast<AIStrategicArea*>(*n)->isLandingZoneUser(equipment))
                    continue;
                const float gravity = (*n)->gravity(information->getPosition(), m_side);
                if (gravity > bestGravity)
                {
                    bestGravity = gravity;
                    best = *n;
                }
            }
            if (best == *object)
                ++unit;
            else
                m_bots[*unit]->occupy(unit, *object, best, this);
        }

        const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        std::set<int>* const groups[3] = {
            &(*object)->getAssignedUnits(m_side),
            &(*object)->getAvailableAttackers(m_side),
            &(*object)->getAvailableDefenders(m_side) };
        for (int g = 0; g < 3; ++g)
        {
            for (std::set<int>::iterator it = groups[g]->begin(); it != groups[g]->end(); ++it)
            {
                Bot* const bot = bots->getBotFromId(*it);
                const unsigned int equipment = bot->getCurrentEquipment();
                const float wait = equipment != bot->getPlayer()->getSoldier()->getInformation(m_side) ? 35.0f : 20.0f;
                // 0x32DC8C..0x32DF66: a reached (+5) MoveTo list (getId +0x10 == 1), the last order
                // (BotInfo +0x14) older than `wait` (`fsubl 0x14; fucompi; jbe`), at most one sensed
                // object (map node count `cmpl $0x1,0x4(%eax); ja`) and still present: retain the
                // bot (slot 28, (side, unit, this)) and stamp the order time.
                WaypointList* const waypoints = bot->getWaypoints();
                if (!waypoints->m_flag5 || waypoints->getId() != 1)
                    continue;
                if (!(now - m_bots[*it]->m_double14 > wait))
                    continue;
                if (bot->getSensedObjects().size() > 1)
                    continue;
                if (!(*object)->isPresentUnit(*it, m_side))
                    continue;
                (*object)->retainBot(m_side, *it, this);
                m_bots[*it]->m_double14 = now;
            }
        }
    }
}

void SAI::BotInfo::free(bool release, SAI* sai)
{
    const int id = m_bot->getBotId();
    AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(m_currentlyOccupiedSquare);
    if (release)
    {
        if (!object->unregisterAvailableAttacker(id, sai->m_side))
            object->unregisterAvailableDefender(id, sai->m_side);
        sai->m_assignedBots.erase(sai->m_assignedBots.find(id));
    }
    sai->m_freeBots.insert(m_bot->getBotId());
}

namespace {

// The unit of a bot's current equipment, or null (the inlined handle lookup of the image).
IPIUnit* currentUnit(unsigned int botId)
{
    Bot* const bot = IBotManager::instance->getBotFromId(botId);
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    return information ? information->getUnit() : 0;
}

// The mean of a unit's strategic strengths, summed in a float.
float meanStrength(IPIUnit* unit)
{
    float sum = 0.0f;
    for (int i = 0; i < unit->getNStrategicStrengths(); ++i)
        sum += unit->getStrategicStrengths()[i];
    sum /= static_cast<float>(unit->getNStrategicStrengths());
    return sum;
}

} // namespace

// ELF 0x329E60. Recounts the side's attack (+0xD8, first strength), defence (+0xDC, second
// strength) and idle (+0xE0, mean strength of present units, less the attackers' and
// defenders' mean strengths at the end) resources over all strategic objects. With +0x1CD /
// +0x1CC set, the available attackers / defenders are freed instead (+0x1CE marks it).
void SAI::updateDistribution()
{
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    float attackSum = 0.0f;
    float defenceSum = 0.0f;
    m_flag1CE = false;
    m_intD8 = 0;
    m_intDC = 0;
    m_intE0 = 0;
    for (std::vector<AIStrategicObject*>::const_iterator object = objects.begin(); object != objects.end(); ++object)
    {
        if (m_flag1CD)
        {
            while ((*object)->getAvailableAttackers(m_side).size())
            {
                m_bots[*(*object)->getAvailableAttackers(m_side).begin()]->free(true, this);
                m_flag1CE = true;
            }
        }
        else
        {
            for (std::set<int>::iterator it = (*object)->getAvailableAttackers(m_side).begin();
                 it != (*object)->getAvailableAttackers(m_side).end(); ++it)
            {
                IPIUnit* const unit = currentUnit(*it);
                if (!unit)
                    continue;
                m_intD8 += static_cast<int>(unit->getStrategicStrengths()[0]);
                attackSum += meanStrength(unit);
            }
        }
        if (m_flag1CC)
        {
            while ((*object)->getAvailableDefenders(m_side).size())
            {
                m_bots[*(*object)->getAvailableDefenders(m_side).begin()]->free(true, this);
                m_flag1CE = true;
            }
        }
        else
        {
            for (std::set<int>::iterator it = (*object)->getAvailableDefenders(m_side).begin();
                 it != (*object)->getAvailableDefenders(m_side).end(); ++it)
            {
                IPIUnit* const unit = currentUnit(*it);
                if (!unit)
                    continue;
                m_intDC += static_cast<int>(unit->getStrategicStrengths()[1]);
                defenceSum += meanStrength(unit);
            }
        }
        for (std::set<int>::iterator it = (*object)->getPresentUnits(m_side).begin();
             it != (*object)->getPresentUnits(m_side).end(); ++it)
        {
            IPIUnit* const unit = currentUnit(*it);
            if (unit)
                m_intE0 += static_cast<int>(meanStrength(unit));
        }
        for (std::set<int>::iterator it = (*object)->getAssignedUnits(m_side).begin();
             it != (*object)->getAssignedUnits(m_side).end(); ++it)
        {
            IPIUnit* const unit = currentUnit(*it);
            if (!unit)
                continue;
            if ((*object)->isAttack(m_side))
            {
                m_intD8 += static_cast<int>(unit->getStrategicStrengths()[0]);
                attackSum += meanStrength(unit);
            }
            else
            {
                m_intDC += static_cast<int>(unit->getStrategicStrengths()[1]);
                defenceSum += meanStrength(unit);
            }
        }
    }
    defenceSum = defenceSum + attackSum;
    m_intE0 -= static_cast<int>(defenceSum);
}

void SAI::BotInfo::respawn(AIStrategicObject* object, SAI* sai)
{
    m_currentlyOccupiedSquare = object->getIndex();
    object->setAsPresentUnit(m_bot->getBotId(), sai->m_side);
    sai->m_assignedBots.find(m_bot->getBotId());              // result unused in the image
    sai->m_freeBots.insert(m_bot->getBotId());
}

void SAI::BotInfo::assign(std::set<int>::iterator& it, bool attacker, SAI* sai)
{
    AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(m_currentlyOccupiedSquare);
    if (attacker)
        object->registerAvailableAttacker(m_bot->getBotId(), sai->m_side);
    else
        object->registerAvailableDefender(m_bot->getBotId(), sai->m_side);
    sai->m_freeBots.erase(it++);
    sai->m_assignedBots.insert(m_bot->getBotId());
}

void SAI::BotInfo::removeAssigned(AIStrategicObject* object, std::set<int>::iterator& it, bool release, SAI* sai)
{
    object->getAssignedUnits(sai->m_side).erase(it++);
    if (release)
    {
        sai->m_assignedBots.erase(sai->m_assignedBots.find(m_bot->getBotId()));
    }
    else
    {
        AIStrategicObject* const current = AIStrategicObjectManager::getInstance()->getStrategicObject(m_currentlyOccupiedSquare);
        if (current->isAttack(sai->m_side))
            current->registerAvailableAttacker(m_bot->getBotId(), sai->m_side);
        else
            current->registerAvailableDefender(m_bot->getBotId(), sai->m_side);
    }
    m_currentlyOrderedSquare = -1;
    m_double14 = 0.0;
}

int SAI::BotInfo::setLastOrderedSquare(int square)
{
    m_double14 = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_lastOrderedSquare = square;
    return square;
}

// ELF 0x32A690. Every free bot (+0x44) is placed at its strategic object (or, after a respawn,
// the closest fitting one), re-ordered there when its last ordered square differs, and assigned
// as attacker or defender: by the strategy's aggression share against the current attack share
// (all attack without friendly areas, all defence when nothing is neutral or hostile). Then
// +0x1CD / +0x1CC ask the next updateDistribution to free attackers / defenders when their
// share exceeds 1.2 times the wanted one. The unused bool[getMaxNBots] and the two
// list<pair<float, int> > locals are kept as shipped.
void SAI::distributeResources()
{
    std::list<std::pair<float, int> > unusedA;
    std::list<std::pair<float, int> > unusedB;
    bool lastWasDefence = true;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    const int slots = settings->getMaxNBots();
    bool* const flags = checkPointer(new bool[slots]);
    for (int i = 0; i < slots; ++i)
        flags[i] = false;

    const float* const friendly = m_int1F4 ? m_stateB : m_stateA;
    float attackShare;
    float defenceShare;
    if (friendly[StrategyCondition::NumberOfFriendlyAreas] == 0.0f)
    {
        attackShare = 1.0f;
        defenceShare = 0.0f;
    }
    else if (friendly[StrategyCondition::NumberOfNeutralAreas] == 0.0f
             && friendly[StrategyCondition::NumberOfHostileAreas] == 0.0f)
    {
        attackShare = 0.0f;
        defenceShare = 1.0f;
    }
    else
    {
        attackShare = m_activeStrategy->strategy->getAggression();
        defenceShare = 1.0f - m_activeStrategy->strategy->getAggression();
    }

    for (std::set<int>::iterator it = m_freeBots.begin(); it != m_freeBots.end();)
    {
        BotInfo* const info = m_bots[*it];
        if (!info)
        {
            ++it;
            continue;
        }
        Information* const information = informationManager->getInformation(info->m_bot->getCurrentEquipment());
        if (!information)
        {
            ++it;
            continue;
        }
        AIStrategicObject* object = manager->getStrategicObject(info->m_currentlyOccupiedSquare);
        if (!object)
        {
            if (!information->getUnit())
            {
                ++it;
                continue;
            }
            int vehicle = -1;
            if (information->getMobile())
                vehicle = information->getMobile()->getVehicleNumber();
            const int equipment = information->getUnit()->getEquipmentType();
            object = manager->getClosestObject(information->getPosition(), equipment, vehicle, m_side);
            if (!object)
            {
                ++it;
                continue;
            }
            m_bots[*it]->respawn(object, this);
        }
        else
        {
            object->setAsPresentUnit(*it, m_side);
        }
        if (object->getIndex() != m_bots[*it]->m_lastOrderedSquare)
        {
            object->retainBot(m_side, *it, this);
            m_bots[*it]->setLastOrderedSquare(object->getIndex());
        }
        IPIUnit* const unit = information->getUnit();   // not null-checked on this path
        const int attack = m_intD8;
        const int defence = m_intDC;
        bool toAttack;
        if (attack + defence == 0)
            toAttack = attackShare >= 0.5f;
        else if (0.0f >= attackShare)
            toAttack = false;
        else if (0.0f >= defenceShare)
            toAttack = true;
        else
            toAttack = attackShare >= static_cast<float>(attack) / static_cast<float>(attack + defence);
        if (toAttack)
            m_intD8 = static_cast<int>(unit->getStrategicStrengths()[0]) + attack;
        else
            m_intDC = static_cast<int>(unit->getStrategicStrengths()[1]) + defence;
        m_intE0 -= static_cast<int>(meanStrength(unit));
        lastWasDefence = !toAttack;
        m_bots[*it]->assign(it, toAttack, this);
    }

    const float total = static_cast<float>(m_intD8 + m_intDC + m_intE0);
    if (total == 0.0f)
    {
        m_flag1CD = false;
        m_flag1CC = false;
    }
    else
    {
        if (static_cast<float>(m_intD8) / total > attackShare * 1.2f && !lastWasDefence)
        {
            m_flag1CD = true;
            m_flag1CE = true;
        }
        else
        {
            m_flag1CD = false;
        }
        if (static_cast<float>(m_intDC) / total > 1.2f * defenceShare && lastWasDefence)
        {
            m_flag1CC = true;
            m_flag1CE = true;
        }
        else
        {
            m_flag1CC = false;
        }
    }
    delete[] flags;
}

// ELF 0x32AE70: per category (or over all objects when there are none), unless the active
// strategy runs no defences.
void SAI::distributeDefence()
{
    if (!m_activeStrategy->strategy->getNumberOfDefences())
        return;
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    if (!manager->getNumberOfCategories())
    {
        categoryDistributeDefence(manager->getStrategicObjects());
        return;
    }
    for (int category = 0; category < manager->getNumberOfCategories(); ++category)
        categoryDistributeDefence(manager->getStrategicObjectsOfCategory(category));
}

// ELF 0x32AF10. Objects needing defence (need > 0.1) are ranked by need, highest first; objects
// with no need that are not attacked release their resources. Beyond twice the strategy's
// defence count, entries below 90% of the need at that rank are dropped (releasing their
// resources unless attacked). With +0x1CC set (and +0x1CE clear) and nothing released yet, the
// lowest-need defence releases resources instead. Otherwise up to getNumberOfDefences() areas
// are set to defend and fed one squad at a time, the least supplied first (sobOrdering).
void SAI::categoryDistributeDefence(const std::vector<AIStrategicObject*>& objects)
{
    std::list<SquareInfo> squares;
    int released = 0;
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if (!*it)
            continue;
        SquareInfo square;
        square.m_index = -1;
        square.m_value = (*it)->calculateDefenseNeed(m_side);
        if (square.m_value > 0.1f)
        {
            square.m_index = (*it)->getIndex();
            squares.push_back(square);
        }
        else if (0.0f >= square.m_value && !(*it)->isAttack(m_side))
        {
            released += (*it)->releaseAssignedResources(m_side);
        }
    }
    squares.sort();
    squares.reverse();
    if (squares.empty())
        return;

    float threshold = 0.0f;
    unsigned int rank = 0;
    std::list<SquareInfo>::iterator it = squares.begin();
    for (; it != squares.end(); ++it)
    {
        ++rank;
        const unsigned int keep = m_activeStrategy->strategy->getNumberOfDefences() * 2;
        if (rank == keep)
            threshold = it->m_value;
        else if (rank > keep && threshold * 0.9f > it->m_value)
            break;
    }
    while (it != squares.end())
    {
        AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
        if (object && !object->isAttack(m_side))
            released += object->releaseAssignedResources(m_side);
        it = squares.erase(it);
    }
    if (squares.empty())
        return;

    if (m_flag1CC && !m_flag1CE && !released)
    {
        squares.reverse();
        for (it = squares.begin(); it != squares.end(); ++it)
        {
            AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
            if (!object || object->isAttack(m_side))
                continue;
            released += object->releaseAssignedResources(m_side);
            if (released > 0)
                return;
        }
        return;
    }

    unsigned int count = 0;
    std::priority_queue<AIStrategicObject*, std::vector<AIStrategicObject*>, sobOrdering> queue((sobOrdering(m_side)));
    for (it = squares.begin(); it != squares.end(); ++it)
    {
        AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
        if (!object)
            continue;
        object->setAttack(false, m_side);
        queue.push(object);
        ++count;
        if (!(m_activeStrategy->strategy->getNumberOfDefences() > count))
            break;
    }
    while (!queue.empty())
    {
        AIStrategicObject* const object = queue.top();
        queue.pop();
        if (object->collectAdditionalSquadResources(1, m_side, this))
            queue.push(object);
    }
}

// ELF 0x32B9B0: per category (or over all objects when there are none), unless the active
// strategy runs no attacks.
void SAI::distributeAttack()
{
    if (!m_activeStrategy->strategy->getNumberOfAttacks())
        return;
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    if (!manager->getNumberOfCategories())
    {
        categoryDistributeAttack(manager->getStrategicObjects());
        return;
    }
    for (int category = 0; category < manager->getNumberOfCategories(); ++category)
        categoryDistributeAttack(manager->getStrategicObjectsOfCategory(category));
}

// ELF 0x32BA50. Areas worth attacking (value > 0.001) are ranked, highest first. The first
// getNumberOfAttacks() set the threshold; up to twice that many are kept while they reach 80% of
// it, the rest are dropped, releasing their resources when attacked. With +0x1CD set (and +0x1CE
// clear) and nothing released, the lowest entry is dropped as well and attacked areas release
// resources instead, until something was released. Otherwise already attacked areas are chosen
// first (up to numberOfAttacks + 1 entries, as shipped), filled up from the top of the ranking,
// set to attack, collect their squads (slot 23) and are then fed one more squad at a time, the
// least supplied first. The isAttack call for non-positive values is discarded, as shipped.
void SAI::categoryDistributeAttack(const std::vector<AIStrategicObject*>& objects)
{
    std::list<SquareInfo> squares;
    std::list<SquareInfo> chosen;
    int released = 0;
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if (!*it)
            continue;
        SquareInfo square;
        square.m_index = -1;
        square.m_value = (*it)->calculateAttackValue(m_side);
        if (square.m_value > 0.001f)
        {
            square.m_index = (*it)->getIndex();
            squares.push_back(square);
        }
        else if (0.0f >= square.m_value)
        {
            (*it)->isAttack(m_side);
        }
    }
    squares.sort();
    squares.reverse();
    if (squares.empty())
        return;

    float threshold = 0.0f;
    unsigned int rank = 0;
    std::list<SquareInfo>::iterator it = squares.begin();
    while (it != squares.end())
    {
        ++rank;
        if (rank <= m_activeStrategy->strategy->getNumberOfAttacks())
            threshold = it->m_value;
        else if (rank > m_activeStrategy->strategy->getNumberOfAttacks() * 2
                 || threshold * 0.8f > it->m_value)
            break;
        ++it;
    }
    while (it != squares.end())
    {
        AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
        if (object && object->isAttack(m_side))
            released += object->releaseAssignedResources(m_side);
        it = squares.erase(it);
    }
    const bool shed = m_flag1CD && !m_flag1CE && !released;
    if (shed)
    {
        it = squares.end();
        --it;
        while (it != squares.end())
            it = squares.erase(it);
    }
    if (squares.empty())
        return;
    if (shed)
    {
        for (it = squares.begin(); it != squares.end(); ++it)
        {
            AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
            if (!object || !object->isAttack(m_side))
                continue;
            released += object->releaseAssignedResources(m_side);
            if (released > 0)
                return;
        }
        return;
    }

    it = squares.begin();
    while (it != squares.end() && !(chosen.size() > m_activeStrategy->strategy->getNumberOfAttacks()))
    {
        AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
        if (object && object->isAttack(m_side))
        {
            chosen.push_back(*it);
            it = squares.erase(it);
        }
        else
        {
            ++it;
        }
    }
    it = squares.begin();
    while (!(chosen.size() > m_activeStrategy->strategy->getNumberOfAttacks()) && it != squares.end())
    {
        chosen.push_back(*it);
        ++it;
    }
    for (it = chosen.begin(); it != chosen.end(); ++it)
    {
        AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
        if (!object)
            continue;
        object->setAttack(true, m_side);
        object->collectSquadResources(m_side, this);
    }
    std::priority_queue<AIStrategicObject*, std::vector<AIStrategicObject*>, sobOrdering> queue((sobOrdering(m_side)));
    for (it = chosen.begin(); it != chosen.end(); ++it)
    {
        AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
        if (object)
            queue.push(object);
    }
    while (!queue.empty())
    {
        AIStrategicObject* const object = queue.top();
        queue.pop();
        if (object->collectAdditionalSquadResources(1, m_side, this))
            queue.push(object);
    }
}

// ELF 0x32B6E0: per category, or over all objects when there are none.
void SAI::distributeWithNoAreas()
{
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    if (!manager->getNumberOfCategories())
    {
        categoryDistributeWithNoAreas(manager->getStrategicObjects());
        return;
    }
    for (int category = 0; category < manager->getNumberOfCategories(); ++category)
        categoryDistributeWithNoAreas(manager->getStrategicObjectsOfCategory(category));
}

// ELF 0x32B770. Every non-null object becomes a SquareInfo {index, 1.0f} (in object order, not
// sorted here); each one that still resolves is set to attack unless the side owns it, then
// collects two additional squads (slot 24 with (2, side, this)). The second list is unused.
void SAI::categoryDistributeWithNoAreas(const std::vector<AIStrategicObject*>& objects)
{
    std::list<SquareInfo> squares;
    std::list<SquareInfo> unused;
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if (!*it)
            continue;
        SquareInfo square;
        square.m_value = 1.0f;
        square.m_index = (*it)->getIndex();
        squares.push_back(square);
    }
    for (std::list<SquareInfo>::iterator it = squares.begin(); it != squares.end(); ++it)
    {
        AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(it->m_index);
        if (!object)
            continue;
        object->setAttack(object->getStatus(m_side) != StatusMap::Owned, m_side);
        object->collectAdditionalSquadResources(2, m_side, this);
    }
}

// Recovered empty helper: , (frame prologue/epilogue and return only).
// PE SAI::update omits the empty call at state 14.
void SAI::distributeArtillery()
{
}

// SampleHelper: complete paired bodies, unsigned live bound and no index/size guards.
SampleHelper::SampleHelper(unsigned int size) : m_size(size)
{
}

void SampleHelper::addSample(float* input, int index, float* strength, int* count)
{
    for (unsigned int i = 0; i < m_size; ++i)
        strength[i] = input[i] + strength[i];
    ++count[index];
}

void SampleHelper::clear(float* strength, int* count)
{
    for (unsigned int i = 0; i < m_size; ++i)
    {
        strength[i] = 0.0f;
        count[i] = 0;
    }
}

// ELF 0x331B80 (weak, inline in the header): the owned commander interface at +0x1F8.
CommanderInterface* SAI::getCommanderInterface() const
{
    return m_commander;
}

// ELF 0x32E8A0: per-round state back to its constructed values (the strategy containers' first
// two floats, the four six-bucket rows, the flags, the resumable update state 19 with the -1000.0
// timestamp, +0xD8/+0xDC), the deny requests freed, the four timers restarted from the game time
// (+0x68/+0x70/+0x78 now + 60, +0x80 now + 10, +0x88 zero), the squad manager reset, +0xAC set.
bool SAI::reset()
{
    for (std::vector<StrategyContainer>::iterator it = m_strategies.begin(); it != m_strategies.end(); ++it)
    {
        it->m_f04 = 0.0f;
        it->m_f00 = 0.0f;
    }
    for (int i = 0; i <= 5; ++i)
    {
        m_enemyStrengths[i] = 0.0f;
        m_enemyCounts[i] = 0.0f;
        m_friendlyStrengths[i] = 0.0f;
        m_friendlyCounts[i] = 0.0f;
    }
    m_flag1C0 = false;
    m_flag1C1 = false;
    m_flag1CE = true;
    m_float190 = -1000.0f;
    m_int18C = 19;
    m_intD8 = 0;
    m_intDC = 0;
    for (std::list<DenyRequest*>::iterator it = m_denyRequests.begin(); it != m_denyRequests.end();)
    {
        ::operator delete(*it);
        it = m_denyRequests.erase(it);
    }
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_time80 = now + 10.0f;
    m_time68 = now + 60.0f;
    m_time70 = now + 60.0f;
    m_time78 = now + 60.0f;
    m_time88 = 0.0f;
    SquadManager::m_instance->reset();
    m_flagAC = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
