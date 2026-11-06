// [TEMP] [CHOSEN, not recovered]
// Tests plan execution and control updates with production bodies, the real InformationManager pool, and real input writes.
// Chosen lifetimes seed state and record call order; full bot construction, pathfinding, and AI frames are outside this fixture.
#include "plan_executor_runtime.h"
#include "../../BF2/Core/Debug.h"
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "MSVC x86 fixture only");

[[noreturn]] static void unexpected(const char* name)
{
    std::fprintf(stderr, "UNEXPECTED plan collaborator: %s\n", name);
    std::fflush(stderr);
    std::abort();
}

static std::string mode;
static int checks = 0;
static std::string calls;
static void require(bool condition, const char* message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL %s: %s [calls=%s]\n", mode.c_str(), message, calls.c_str());
        std::fflush(stderr);
        std::_Exit(1);
    }
}
static void trace(const std::string& item) { if (!calls.empty()) calls += ','; calls += item; }
static float fromBits(std::uint32_t bits) { float value; std::memcpy(&value, &bits, 4); return value; }
static const double nan_ = std::numeric_limits<double>::quiet_NaN();

namespace dice { namespace hfe { namespace ai {
class ObservedBot; class ObservedPlayer; class ObservedMobile; class ObservedPhysical;
} } }

// scripted collaborator state
struct FixtureBotState
{
    bool dead = false, deactivated = false, squad = true, hasPlan = false, hasDefaultPlan = false;
    unsigned int equipment = 0;
    IAIPlayer* player = nullptr;
    std::vector<IAIPlayer*> players;           // consumed one per getPlayer() call when non-empty
    BotNavigation* navigation = nullptr;
    Information* rearmedWith = nullptr;
    double planSlice = -1.0;
    bool planSliceSeen = false;
    BotLodLevel lod = BotLodFull;
    int decideResult = 2;
    std::vector<double> decideSlices, seeSlices, senseSlices;
    int pathCalls = 0, pathNullCalls = 0;
    int vehicleNumber = 0;
    bool searchActive = false, searchNew = false;
};
static FixtureBotState bots[6];
static std::vector<double> pathfinderBudgets;
static std::vector<std::pair<int, IAIObject*> > occupied;
struct FixturePlayerState { std::string name = "bot"; IAIObject* current = nullptr; IAIObject* fallback = nullptr; bool climbing = false; };
static FixturePlayerState players[3];
struct FixturePhysicalState { bool air = false, water = false; };
static FixturePhysicalState physicals[2];
static std::vector<double> updateTimes;
static std::size_t updateIndex = 0;
static double increase = 0.5;
static int squadToken;

static bool botIsDead(const ObservedBot& b);
static bool botIsDeactivated(const ObservedBot& b);
static SquadMember* botSquadMember(const ObservedBot& b);
static unsigned int botEquipment(const ObservedBot& b);
static IAIPlayer* botPlayer(const ObservedBot& b);
static BotNavigation* botNavigation(const ObservedBot& b);
static void botRearm(const ObservedBot& b, Information* i);
static void botPlanExecution(const ObservedBot& b, const double& slice);
static bool botHasPlan(const ObservedBot& b);
static bool botHasDefaultPlan(const ObservedBot& b);
static void botUpdatePlayerAction(const ObservedBot& b, bool stalled);
static const std::string& playerName(const ObservedPlayer& p);
static IAIObject* playerCurrentVehicle(const ObservedPlayer& p);
static IAIObject* playerDefaultVehicle(const ObservedPlayer& p);
static bool playerClimbing(const ObservedPlayer& p);
static const Vec3& mobileSpeed(const ObservedMobile& m);
static bool physicalInAir(const ObservedPhysical& p);
static bool physicalInWater(const ObservedPhysical& p);
static double timerUpdate()
{
    if (updateIndex >= updateTimes.size()) unexpected("unscripted IBFTimer::getUpdateTime");
    trace("T");
    return updateTimes[updateIndex++];
}
static double timerIncrease() { trace("I"); return increase; }
static int effectorRespawn(IAIPlayer* player);
static BotLodLevel botLod(const ObservedBot& b);
static int botDecide(const ObservedBot& b, const double& slice);
static void botSee(const ObservedBot& b, const double& dt);
static void botSense(const ObservedBot& b, const double& dt);
static void botPath(const ObservedBot& b, bool& a, bool* c);
static void pathfinderUpdate(double budget);
static void environmentOccupied(std::vector<std::pair<int, IAIObject*> >& out);
static double gameTime = 42.0;
static double timerGame() { trace("G"); return gameTime; }

#include "mocks.inl"

static std::string id(int identity) { return std::to_string(identity); }
static bool botIsDead(const ObservedBot& b) { trace("D" + id(b.identity)); return bots[b.identity].dead; }
static bool botIsDeactivated(const ObservedBot& b) { trace("V" + id(b.identity)); return bots[b.identity].deactivated; }
static SquadMember* botSquadMember(const ObservedBot& b)
{
    trace("Q" + id(b.identity));
    return bots[b.identity].squad ? reinterpret_cast<SquadMember*>(&squadToken) : nullptr;
}
static unsigned int botEquipment(const ObservedBot& b) { trace("E" + id(b.identity)); return bots[b.identity].equipment; }
static IAIPlayer* botPlayer(const ObservedBot& b)
{
    trace("P" + id(b.identity));
    FixtureBotState& s = bots[b.identity];
    if (s.players.empty()) return s.player;
    IAIPlayer* const next = s.players.front();
    s.players.erase(s.players.begin());
    return next;
}
static BotNavigation* botNavigation(const ObservedBot& b) { trace("N" + id(b.identity)); return bots[b.identity].navigation; }
static void botRearm(const ObservedBot& b, Information* i) { trace("R" + id(b.identity)); bots[b.identity].rearmedWith = i; }
static void botPlanExecution(const ObservedBot& b, const double& slice)
{
    trace("X" + id(b.identity));
    bots[b.identity].planSlice = slice;
    bots[b.identity].planSliceSeen = true;
}
static bool botHasPlan(const ObservedBot& b) { trace("H" + id(b.identity)); return bots[b.identity].hasPlan; }
static bool botHasDefaultPlan(const ObservedBot& b) { trace("F" + id(b.identity)); return bots[b.identity].hasDefaultPlan; }
static void botUpdatePlayerAction(const ObservedBot& b, bool stalled) { trace("U" + id(b.identity) + (stalled ? "1" : "0")); }
static const std::string& playerName(const ObservedPlayer& p) { trace("n" + id(p.identity)); return players[p.identity].name; }
static IAIObject* playerCurrentVehicle(const ObservedPlayer& p) { trace("C" + id(p.identity)); return players[p.identity].current; }
static IAIObject* playerDefaultVehicle(const ObservedPlayer& p) { trace("O" + id(p.identity)); return players[p.identity].fallback; }
static bool playerClimbing(const ObservedPlayer& p) { trace("L" + id(p.identity)); return players[p.identity].climbing; }
static const Vec3& mobileSpeed(const ObservedMobile& m) { trace("M"); return m.speed; }
static bool physicalInAir(const ObservedPhysical& p) { trace("A"); return physicals[p.identity].air; }
static bool physicalInWater(const ObservedPhysical& p) { trace("W"); return physicals[p.identity].water; }
static int effectorRespawn(IAIPlayer* player)
{
    trace(player ? "S" + id(static_cast<ObservedPlayer*>(player)->identity) : std::string("S-"));
    return 0;
}
static BotLodLevel botLod(const ObservedBot& b) { trace("l" + id(b.identity)); return bots[b.identity].lod; }
static int botDecide(const ObservedBot& b, const double& slice)
{
    trace("d" + id(b.identity));
    bots[b.identity].decideSlices.push_back(slice);
    return bots[b.identity].decideResult;
}
static void botSee(const ObservedBot& b, const double& dt) { trace("s" + id(b.identity)); bots[b.identity].seeSlices.push_back(dt); }
static void botSense(const ObservedBot& b, const double& dt) { trace("u" + id(b.identity)); bots[b.identity].senseSlices.push_back(dt); }
static void botPath(const ObservedBot& b, bool& a, bool* c)
{
    (void)a;
    trace(std::string(c ? "p" : "q") + id(b.identity));
    ++(c ? bots[b.identity].pathCalls : bots[b.identity].pathNullCalls);
}
static void pathfinderUpdate(double budget) { trace("Z"); pathfinderBudgets.push_back(budget); }
static void environmentOccupied(std::vector<std::pair<int, IAIObject*> >& out)
{
    trace("Y" + std::to_string(out.size()));   // the executor must have emptied it first
    out = occupied;
}
static int vehicleNumbers[2] = {0, 0};

// chosen lifetimes: fixture scaffolding, not recovery
struct InfoFields { Information* owner; void** armament; void** mobile; IPIPhysical** physical; };
static std::vector<InfoFields> infoFields;
static bool* lastPending = nullptr;
struct ManagerView
{
    std::vector<BotManager::BotInfo*>* infos = nullptr;
    BotManager::BotQueue* queueA = nullptr;
    BotManager::BotQueue* queueB = nullptr;
    BotManager::SensingQueue* sensing = nullptr;
    unsigned int* cursor = nullptr;
    bool* toggle = nullptr;
    unsigned int* executions = nullptr;
    unsigned int* count180 = nullptr;
    unsigned int* count184 = nullptr;
    std::vector<BotManager::BotInfo*>* selected = nullptr;
    std::vector<std::pair<int, IAIObject*> >* occupiedObjects = nullptr;
    unsigned int* pathCursor = nullptr;
    int* hearingResume = nullptr;
};
static ManagerView view;
static bool initialToggle = true;
static unsigned int initialCursor = 0;
static unsigned int fullPriority = 3;
static double plannedThreshold = 0.5, unplannedThreshold = 0.3;
static unsigned int plannedInterleave = 2, unplannedInterleave = 2;
static unsigned int fullTicks = 6, mediumTicks = 4, lightTicks = 2;
static double sensingQuotient = 0.25;
static unsigned int initialPathCursor = 0;
static int initialHearingResume = 0;
static bool* lastSearchActive = nullptr;
static bool* lastSearchNew = nullptr;
static io::PlayerInput* mainInput = nullptr;
static unsigned int mainEquipment = 0;
static int mainTicks = 0;
static int* ticks = nullptr;

namespace dice { namespace hfe { namespace ai {
void* CommonSingletonServer::m_iBFTimer = nullptr;
void* CommonSingletonServer::m_iaiGameEffector = nullptr;
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}
IAIGameEffector::IAIGameEffector() {}
IAIGameEffector::~IAIGameEffector() {}
IAIPlayer::~IAIPlayer() {}
IPIMobile::~IPIMobile() {}
int IPIMobile::getType() { unexpected("IPIMobile::getType"); }
void IPIMobile::setTime() { unexpected("IPIMobile::setTime"); }

int IPIPhysical::getType() { unexpected("IPIPhysical::getType"); }
Bot::~Bot() {}
int Bot::getBotType() const { unexpected("Bot::getBotType"); }
BAPContext* Bot::getScriptContext() { unexpected("Bot::getScriptContext"); }
IBotManager::IBotManager() {}
IBotManager::~IBotManager() {}
IBotManagerModifier::IBotManagerModifier() {}
IBotManagerModifier::~IBotManagerModifier() {}
Information::Information(IAIObject* object, double time)
    : m_objectTypeFlags(0), m_strength(0.0f), m_time(time), m_object(object), m_isBooked(false), m_bookingPlayerId(-1),
      m_armament(nullptr), m_controlInfo(nullptr), m_cover(nullptr), m_mobile(nullptr), m_physical(nullptr),
      m_unit(nullptr)
{
    infoFields.push_back(InfoFields{this, reinterpret_cast<void**>(&m_armament), reinterpret_cast<void**>(&m_mobile), &m_physical});
}
Information::~Information() {}
int Information::getInformationSort() { unexpected("Information::getInformationSort"); }
BotNavigation::BotNavigation(BotMain* owner) : m_bot(owner), m_flag192(false), m_flag193(false), m_flag316(false)
{
    lastPending = &m_flag192;
    lastSearchActive = &m_flag193;
    lastSearchNew = &m_flag316;
}
BotNavigation::~BotNavigation() {}
// Chosen collaborator (the production body is a named stop in Information.cpp): answers per mobile.
int IPIMobile::getVehicleNumber() const
{
    const int identity = static_cast<const ObservedMobile*>(this)->identity;
    trace("K" + std::to_string(identity));
    return vehicleNumbers[identity];
}
IAIPathfinding* IAIPathfinding::instance = nullptr;
IAIPathfinding::IAIPathfinding() { instance = this; }
IAIPathfinding::~IAIPathfinding() { instance = nullptr; }
IAIPathfindingSetup* IAIPathfindingSetup::instance = nullptr;
IAIPathfindingSetup::IAIPathfindingSetup() { instance = this; }
IAIPathfindingSetup::~IAIPathfindingSetup() { instance = nullptr; }
} } }
typedef PathfindingEngine<QTSearchNode, QTSearchNodeManager, QTSearchNode, unsigned short, QTPath> PolygonEngine;
typedef PathfindingEngine<ClusterNode, ClusterNodeManager, ClusterNode, unsigned short, ClusterNodePath> ClusterEngine;
static PolygonEngine* chosenPolygonEngine = nullptr;
static ClusterEngine* chosenClusterEngine = nullptr;
namespace dice { namespace hfe { namespace ai {
// Chosen: only the two engine pointers updatePathfinding reads are set; nothing else is built.
AIPathfinding::AIPathfinding() : m_engine(chosenPolygonEngine), m_clusterEngine(chosenClusterEngine) {}
AIPathfinding::~AIPathfinding() {}
void* CommonSingletonServer::m_iaiObjectEnvironment = nullptr;
IAIObjectEnvironment::IAIObjectEnvironment() {}
IAIObjectEnvironment::~IAIObjectEnvironment() {}
BotManager::BotInfo::BotInfo(Bot* bot)
    : m_bot(bot), m_word4(0), m_priority(0), m_word12(0), m_sensingPriority(0), m_flag20(false),
      m_double24(0.0), m_word36(0), m_word40(0), m_word44(0), m_double48(0.0), m_double56(0.0),
      m_double64(0.0), m_flag72(true), m_flag73(false), m_double76(0.0), m_damage(0.0f), m_attackerIndex(0)
{
}
BotManager::BotManager()
    : m_planned(new BotQueue), m_unplanned(new BotQueue), m_sensingQueue(new SensingQueue),
      m_lodLevelFullPriority(fullPriority), m_plannedDecisionMakingThresholdFull(plannedThreshold),
      m_unplannedDecisionMakingThresholdFull(unplannedThreshold), m_planExecutionsThisTick(7),
      m_tickCount180(8), m_tickCount184(9), m_planHalfToggle(initialToggle), m_planCursor(initialCursor)
{
    m_plannedDecisionMakingInterleave = plannedInterleave;
    m_unplannedDecisionMakingInterleave = unplannedInterleave;
    m_lodLevelFullTicks = fullTicks;
    m_lodLevelMediumTicks = mediumTicks;
    m_lodLevelLightTicks = lightTicks;
    m_sensingQuotient = sensingQuotient;
    m_sensingQuotientInverse = 1.0 - sensingQuotient;
    m_pathfindingCursor = initialPathCursor;
    m_hearingResumeId = initialHearingResume;
    view.selected = &m_selectedBots;
    view.occupiedObjects = &m_occupiedObjects;
    view.pathCursor = &m_pathfindingCursor;
    view.hearingResume = &m_hearingResumeId;
    view.infos = &m_bots;
    view.queueA = m_planned;
    view.queueB = m_unplanned;
    view.sensing = m_sensingQueue;
    view.cursor = &m_planCursor;
    view.toggle = &m_planHalfToggle;
    view.executions = &m_planExecutionsThisTick;
    view.count180 = &m_tickCount180;
    view.count184 = &m_tickCount184;
}
BotManager::~BotManager()
{
    delete m_planned;
    delete m_unplanned;
    delete m_sensingQueue;
}
BotMain::BotMain(IAIPlayer*, unsigned int, float*, float, int, BotOrder*, BotMemory*, PlanInterpreter*, int,
                 UrgencyGenerator*, UrgencyGenerator**)
    : m_currentEquipment(mainEquipment), m_stalledPlanTicks(mainTicks)
{
    m_playerInput = mainInput;
    ticks = &m_stalledPlanTicks;
}
BotMain::~BotMain() {}
} } }

static InfoFields& fieldsOf(Information* information)
{
    for (InfoFields& f : infoFields)
        if (f.owner == information) return f;
    unexpected("unknown Information");
}

// the stop check, run from SIGABRT after a production frontier has reported
static std::string stopCalls;
static int stopTicks = -1;
static bool (*stopExtra)() = nullptr;     // an extra state predicate for a stop, when set
static void onAbort(int)
{
    if (!stopCalls.empty() || stopTicks >= 0)
    {
        if (calls == stopCalls && (stopTicks < 0 || (ticks && *ticks == stopTicks)) && (!stopExtra || stopExtra()))
        {
            std::printf("VERIFIED %s\n", mode.c_str());
            std::fflush(stdout);
            return;
        }
        std::fprintf(stderr, "FAIL %s: stop state calls=[%s] ticks=%d\n", mode.c_str(), calls.c_str(),
                     ticks ? *ticks : -99);
        std::fflush(stderr);
        std::_Exit(1);
    }
}

// executor world
struct World
{
    ObservedTimer timer;
    ObservedEffector effector;
    ObservedPathfinder pathfinder;
    ObservedEnvironment environment;
    InformationManager manager;
    ObservedBot bot[6];
    ObservedPlayer player[3];
    ObservedInformation info[6];
    ObservedMobile mobile[2];
    ObservedPhysical physical[2];
    BotNavigation* navigation[6] = {};
    bool* navigationPending[6] = {};
    bool* navigationActive[6] = {};
    bool* navigationNew[6] = {};
    unsigned int handle[6] = {};
    io::PlayerInput input[6];
    BotManager* botManager = nullptr;
    int objectA = 0, objectB = 0;

    World()
    {
        CommonSingletonServer::m_iBFTimer = &timer;
        CommonSingletonServer::m_iaiGameEffector = &effector;
        CommonSingletonServer::m_iaiObjectEnvironment = &environment;
        informationManager = &manager;
        for (int i = 0; i < 3; ++i) player[i].identity = i;
        for (int i = 0; i < 2; ++i) { mobile[i].identity = i; physical[i].identity = i; }
        for (int i = 0; i < 6; ++i)
        {
            bot[i].identity = i;
            navigation[i] = new BotNavigation(nullptr);
            navigationPending[i] = lastPending;     // exported by the chosen constructor just run
            navigationActive[i] = lastSearchActive;
            navigationNew[i] = lastSearchNew;
            bots[i].navigation = navigation[i];
            bots[i].player = &player[0];
            handle[i] = manager.getPool()->acquire(&info[i]);
            bots[i].equipment = handle[i];
            std::memset(&input[i], 0, sizeof(input[i]));
            bot[i].setInput(&input[i]);
            InfoFields& f = fieldsOf(&info[i]);
            *f.physical = &physical[0];
        }
    }
    void make(unsigned int count)
    {
        botManager = new BotManager;
        for (unsigned int i = 0; i < count; ++i)
            view.infos->push_back(new BotManager::BotInfo(&bot[i]));
    }
    BotManager::BotInfo* record(unsigned int i) { return (*view.infos)[i]; }
    void pending(int i, bool value) { *navigationPending[i] = value; }
    void run(double slice)
    {
        calls.clear();
        updateIndex = 0;
        botManager->actionExecutePlan(slice);
    }
    void decide(double slice) { calls.clear(); updateIndex = 0; botManager->actionDecisionMaking(slice); }
    void pathfind(double slice) { calls.clear(); updateIndex = 0; botManager->actionPathfinding(slice); }
    void sense(double slice) { calls.clear(); updateIndex = 0; botManager->actionSensing(slice); }
    void hear(double slice) { calls.clear(); updateIndex = 0; botManager->actionHearing(slice); }
};

static void livingBots(World&, unsigned int count)
{
    for (unsigned int i = 0; i < count; ++i)
    {
        bots[i].dead = false; bots[i].deactivated = false; bots[i].squad = true;
    }
}

static std::string executeHead(int i)
{
    const std::string s = id(i);
    return "D" + s + ",V" + s + ",Q" + s + ",E" + s + ",P" + s + ",n0,N" + s;
}

// Dead rosters: each served bot costs one isDead; the tail sweeps the cursor bot.
static void countCase(unsigned int size, bool before, unsigned int expectedCount, unsigned int start)
{
    World w;
    for (unsigned int i = 0; i < size; ++i) bots[i].dead = true;
    initialToggle = before;
    initialCursor = start;
    w.make(size);
    updateTimes = {10.0};
    w.run(1.0);
    std::string expected = "T";
    unsigned int cursor = start;
    for (unsigned int k = 0; k < expectedCount; ++k)
    {
        expected += ",D" + id(static_cast<int>(cursor));
        cursor = (cursor + 1) % size;
    }
    for (unsigned int k = expectedCount; k < size; ++k)
        expected += ",U" + id(static_cast<int>(cursor)) + "0";
    require(calls == expected, "served bots in cursor order, then the tail on the cursor bot");
    require(*view.cursor == cursor, "cursor advanced once per served bot, modulo the size");
    require(*view.toggle == !before, "half toggle flipped");
    require(*view.executions == 0 && *view.count180 == 0 && *view.count184 == 0, "three counters zeroed");
    require(view.queueA->empty() && view.queueB->empty() && view.sensing->empty(), "no bot queued");
}

// One living, idle bot the executor serves in budget unless told otherwise.
struct Served
{
    World w;
    explicit Served(unsigned int size = 1)
    {
        livingBots(w, size);
        initialToggle = false;       // flips to true: size 1 -> one bot served
        initialCursor = 0;
        w.make(size);
        for (unsigned int i = 0; i < size; ++i)
        {
            w.record(i)->m_double24 = 0.25;
            w.record(i)->m_double48 = 1.0;
            w.record(i)->m_word36 = 4;
            w.record(i)->m_priority = 10;
            w.record(i)->m_sensingPriority = 20;
        }
    }
};

static void runExecutor()
{
    if (mode == "empty_roster")
    {
        World w;
        initialToggle = true;
        initialCursor = 0;
        w.make(0);
        BotManager::BotInfo stale(&w.bot[0]);
        view.queueA->push(&stale); view.queueB->push(&stale); view.queueB->push(&stale); view.sensing->push(&stale);
        updateTimes = {10.0};
        w.run(1.0);
        require(calls == "T", "only the deadline read");
        require(view.queueA->empty() && view.queueB->empty() && view.sensing->empty(), "all three queues emptied");
        require(*view.executions == 0 && *view.count180 == 0 && *view.count184 == 0, "counters zeroed");
        require(*view.toggle == false && *view.cursor == 0, "toggle flipped, cursor untouched");
    }
    else if (mode == "count_1_false") countCase(1, true, 0, 0);
    else if (mode == "count_1_true") countCase(1, false, 1, 0);
    else if (mode == "count_2") countCase(2, true, 2, 0);
    else if (mode == "count_3_false") countCase(3, true, 1, 0);
    else if (mode == "count_3_true") countCase(3, false, 2, 0);
    else if (mode == "count_4") countCase(4, true, 3, 0);
    else if (mode == "count_5_false") countCase(5, true, 2, 0);
    else if (mode == "count_5_true") countCase(5, false, 3, 0);
    else if (mode == "cursor_wrap") countCase(3, false, 2, 2);
    else if (mode == "skip_arms")
    {
        World w;
        bots[1].dead = true;
        bots[2].deactivated = true;
        bots[3].squad = false;
        initialToggle = true;
        initialCursor = 0;
        w.make(4);
        (*view.infos)[0]->m_bot = nullptr;     // a null bot record
        updateTimes = {10.0};
        w.run(1.0);
        require(calls == "T,D1,D2,V2,U30", "null, dead and deactivated records only move the cursor");
        require(*view.cursor == 3, "three served");
        w.run(1.0);
        require(calls == "T,D3,V3,Q3,D1,U20", "a squadless bot is skipped too; the tail sweeps bot 2");
        require(*view.cursor == 2, "wrapped past the null record");
        require(view.sensing->empty(), "nothing queued");
    }
    else if (mode == "no_information")
    {
        World w;
        livingBots(w, 2);
        bots[0].equipment = 0;                                       // the null handle
        bots[1].equipment = w.handle[1] ^ 0x10000u;                  // a stale generation
        initialToggle = true;
        w.make(2);
        updateTimes = {10.0};
        w.run(1.0);
        require(calls == "T,D0,V0,Q0,E0,P0,n0,D1,V1,Q1,E1,P1,n0",
                "the name is copied before the information test, then the bot is skipped");
        require(*view.cursor == 0 && view.sensing->empty() && *view.executions == 0, "skipped without a slot");
    }
    else if (mode == "execute_armed" || mode == "execute_unarmed")
    {
        Served s;
        const bool armed = mode == "execute_armed";
        if (armed) *fieldsOf(&s.w.info[0]).armament = &squadToken;
        updateTimes = {10.0, 10.5, 10.75};
        s.w.run(1.0);
        const std::string rearm = armed ? ",R0" : "";
        require(calls == "T," + executeHead(0) + rearm + ",I,T,X0,T,P0,L0,H0,U00", "served in budget, in order");
        require(bots[0].rearmedWith == (armed ? &s.w.info[0] : nullptr), "rearmed with its own equipment");
        require(bots[0].planSliceSeen && bots[0].planSlice == 0.0, "plan executed with a 0.0 slice");
        BotManager::BotInfo* r = s.w.record(0);
        require(r->m_double24 == 0.75, "time since decision grew by the increase");
        require(r->m_double48 == 1.25, "run time banked (10.75 - 10.5)");
        require(r->m_word36 == 5, "run count");
        require(r->m_priority == 13 && r->m_sensingPriority == 23, "both priorities gained the full LOD priority");
        require(*view.executions == 1, "counted");
        require(view.queueA->empty() && view.queueB->size() == 1 && view.queueB->top() == r, "unplanned queue");
        require(view.sensing->size() == 1 && view.sensing->top() == r, "sensing queue");
        require(*view.cursor == 0, "cursor wrapped");
    }
    else if (mode == "out_of_budget" || mode == "deadline_equal" || mode == "deadline_nan")
    {
        Served s(2);
        if (mode == "out_of_budget") updateTimes = {10.0, 11.0};
        else if (mode == "deadline_equal") updateTimes = {10.0, 10.5};
        else updateTimes = {10.0, nan_};
        s.w.run(0.5);
        require(calls == "T," + executeHead(0) + ",I,T,H0,U01,U00,U00",
                "not in budget: queued, told stalled, then the tail sweeps the SAME bot twice");
        BotManager::BotInfo* r = s.w.record(0);
        require(!bots[0].planSliceSeen && r->m_double48 == 1.0 && r->m_word36 == 4, "no plan run");
        require(r->m_double24 == 0.75 && r->m_priority == 13, "still aged and prioritised");
        require(*view.executions == 1 && *view.cursor == 0, "counted, cursor not moved");
        require(view.queueB->size() == 1 && view.sensing->size() == 1, "still queued");
    }
    else if (mode == "two_bots_budget_split")
    {
        Served s(2);
        updateTimes = {10.0, 10.2, 10.4, 11.0};
        s.w.run(1.0);
        require(calls == "T," + executeHead(0) + ",I,T,X0,T,P0,L0,H0,U00," + executeHead(1) + ",I,T,H1,U11,U10",
                "the first bot runs, the second is out of budget and swept once");
        require(*view.executions == 2 && *view.cursor == 1, "both counted; the cursor rests on the stalled bot");
        require(view.sensing->size() == 2, "both in the sensing queue");
    }
    else if (mode.compare(0, 6, "queue_") == 0)
    {
        // The chosen constructor reads the thresholds, so they are set before the manager exists.
        if (mode == "queue_unplanned_equal") unplannedThreshold = 0.75;   // 0.25 + 0.5 lands on it
        Served s;
        if (mode == "queue_planned") { bots[0].hasPlan = true; }
        if (mode == "queue_planned_equal") { bots[0].hasPlan = true; s.w.record(0)->m_double24 = 0.0; }
        if (mode == "queue_default_plan") { bots[0].hasPlan = true; bots[0].hasDefaultPlan = true; }
        if (mode == "queue_unplanned") { s.w.record(0)->m_double24 = -0.25; }
        updateTimes = {10.0, 10.5, 10.75};
        s.w.run(1.0);
        const std::string plan = bots[0].hasPlan ? ",H0,F0" : ",H0";
        require(calls == "T," + executeHead(0) + ",I,T,X0,T,P0,L0" + plan + ",U00", "plan questions in order");
        BotManager::BotInfo* r = s.w.record(0);
        const bool toA = mode == "queue_planned";
        const bool toB = mode == "queue_default_plan";
        require(view.queueA->size() == (toA ? 1u : 0u), "planned queue only past the planned FULL threshold");
        require(view.queueB->size() == (toB ? 1u : 0u), "unplanned queue only past the unplanned FULL threshold");
        require(view.sensing->size() == 1 && view.sensing->top() == r, "always sensed");
    }
    else if (mode == "ladder_mapped" || mode == "ladder_unmapped")
    {
        Served s;
        players[0].climbing = true;
        io::PlayerInput& in = s.w.input[0];
        in.inputs[io::PIThrottle] = 0.25f;
        in.inputs[io::PIMouseLookY] = 0.5f;
        in.inputs[io::PIYaw] = 0.75f;
        in.setInputIsMapped(io::PIMouseLookY, true);
        in.setInputIsMapped(io::PIYaw, true);
        if (mode == "ladder_mapped") in.setInputIsMapped(io::PIThrottle, true);
        updateTimes = {10.0, 10.5, 10.75};
        s.w.run(1.0);
        require(calls == "T," + executeHead(0) + ",I,T,X0,T,P0,L0,H0,U00", "climber served");
        require(in.inputs[io::PIThrottle] == (mode == "ladder_mapped" ? 1.0f : 0.25f), "throttle pushed only if mapped");
        require(in.inputs[io::PIMouseLookY] == 1.0f, "look up");
        require(in.inputs[io::PIYaw] == 0.75f, "nothing else touched");
    }
    else if (mode.compare(0, 8, "pending_") == 0)
    {
        Served s;
        s.w.pending(0, true);
        *fieldsOf(&s.w.info[0]).mobile = &s.w.mobile[0];
        Vec3& v = s.w.mobile[0].speed;
        v = Vec3{0.0f, 0.0f, 0.0f};
        const std::string head = "T," + executeHead(0);
        // `bot->getPlayer() && bot->getPlayer()->getCurrentVehicle()`, then `if (bot->getPlayer())`.
        const std::string vehicleCheck = ",P0,P0,C0,P0";
        // The onFoot comparison's two getters may be evaluated in either order (the images differ).
        const std::string pairA = ",P0,O0,P0,C0", pairB = ",P0,P0,C0,O0";
        const std::string executed = ",I,T,X0,T,P0,L0,H0,U00";
        const std::string respawned = ",P0,S0,U00";
        updateTimes = {10.0, 10.5, 10.75};
        if (mode == "pending_vehicle_stop")
        {
            players[0].current = reinterpret_cast<IAIObject*>(&s.w.objectA);
            stopCalls = head + ",P0,P0,C0,P0,C0";       // the check, then the vehicle the stop names
            s.w.run(1.0);
            unexpected("the vehicle arm returned");
        }
        std::string middle, tail;
        if (mode == "pending_no_player")
        {
            bots[0].players = {&s.w.player[0], nullptr, nullptr, &s.w.player[0]};
            s.w.run(1.0);
            require(calls == head + ",P0,P0,M,A,W" + executed, "no player: no vehicle query, not on foot");
        }
        else
        {
            if (mode == "pending_equal_on_foot") v = Vec3{0.5f, 0.0f, 0.0f};
            if (mode == "pending_fast" || mode == "pending_in_water" || mode == "pending_airborne") v = Vec3{0.6f, 0.0f, 0.0f};
            if (mode == "pending_slow_not_on_foot") players[0].fallback = reinterpret_cast<IAIObject*>(&s.w.objectB);
            if (mode == "pending_no_mobile") *fieldsOf(&s.w.info[0]).mobile = nullptr;
            if (mode == "pending_in_water") physicals[0].water = true;
            if (mode == "pending_airborne") { physicals[0].air = true; physicals[0].water = true; }
            if (mode == "pending_nan_speed") v = Vec3{static_cast<float>(nan_), 0.0f, 0.0f};
            if (mode == "pending_witness") v = Vec3{fromBits(0x3E93CCFA), fromBits(0x3E93CCFA), fromBits(0x3E93CDBB)};
            s.w.run(1.0);
            std::string rest;
            if (mode == "pending_slow_on_foot" || mode == "pending_equal_on_foot") rest = ",M" + respawned;
            else if (mode == "pending_no_mobile") rest = respawned;
            else if (mode == "pending_in_water") rest = ",M,A,W" + respawned;
            else if (mode == "pending_airborne") rest = ",M,A" + executed;
            else rest = ",M,A,W" + executed;     // fast, not on foot, NaN, witness
            const std::string a = head + vehicleCheck + pairA + rest, b = head + vehicleCheck + pairB + rest;
            require(calls == a || calls == b, "pending arm: vehicle query, on-foot test, stall test in order");
        }
        const bool respawn = calls.find(",S0,") != std::string::npos;
        require(respawn == (mode == "pending_slow_on_foot" || mode == "pending_equal_on_foot" ||
                            mode == "pending_no_mobile" || mode == "pending_in_water"), "respawn decision");
        require(*view.cursor == 0 && *view.executions == (respawn ? 0u : 1u), "respawned bots are not counted");
        require(view.sensing->size() == (respawn ? 0u : 1u), "respawned bots are not queued");
        require(s.w.record(0)->m_double24 == (respawn ? 0.25 : 0.75), "respawned bots do not age");
    }
    else
        unexpected("unknown executor mode");
}

// stall (BotMain::updatePlayerAction)
static void runStall()
{
    InformationManager manager;
    informationManager = &manager;
    ObservedInformation information;
    ObservedMobile mobile;
    const unsigned int handle = manager.getPool()->acquire(&information);
    *fieldsOf(&information).mobile = &mobile;
    io::PlayerInput input;
    std::memset(&input, 0, sizeof(input));
    const io::PlayerInputMap released[] = {io::PILie, io::PIFire, io::PIAltFire, io::PIReload};
    for (io::PlayerInputMap i : released) { input.inputs[i] = 1.0f; input.setInputIsMapped(i, true); }
    input.inputs[io::PIThrottle] = 1.0f;
    input.setInputIsMapped(io::PIThrottle, true);
    mainInput = &input;
    mainEquipment = handle;
    mainTicks = 0;
    if (mode == "stall_clear") mainTicks = 7;
    if (mode == "stall_at_limit") mainTicks = 14;
    if (mode == "stall_over_limit_stop") mainTicks = 15;
    if (mode == "stall_coast_unmapped")
    {
        input.setInputIsMapped(io::PIFire, false);
        input.setInputIsMapped(io::PIReload, false);
    }
    mobile.speed = Vec3{0.2f, 0.0f, 0.0f};
    if (mode == "stall_nan") mobile.speed = Vec3{static_cast<float>(nan_), 0.0f, 0.0f};
    if (mode == "stall_witness") mobile.speed = Vec3{fromBits(0x3D6C7B50), fromBits(0x3D6C7B50), fromBits(0x3D6C7C11)};
    if (mode == "stall_slow_stop") mobile.speed = Vec3{0.05f, 0.0f, 0.0f};
    if (mode == "stall_no_mobile_stop") *fieldsOf(&information).mobile = nullptr;
    BotMain bot(nullptr, 0, nullptr, 0.0f, 0, nullptr, nullptr, nullptr, 0, nullptr, nullptr);
    calls.clear();
    if (mode == "stall_over_limit_stop") { stopCalls = ""; stopTicks = 16; }
    if (mode == "stall_slow_stop") { stopCalls = "M"; stopTicks = 1; }
    if (mode == "stall_no_mobile_stop") { stopCalls = ""; stopTicks = 1; }
    bot.updatePlayerAction(mode != "stall_clear");
    if (stopTicks >= 0) unexpected("the reset arm returned");
    if (mode == "stall_clear")
    {
        require(*ticks == 0 && calls.empty(), "a served bot clears its stall count and touches nothing");
        require(input.inputs[io::PIFire] == 1.0f, "no input released");
        return;
    }
    require(calls == "M", "one speed read");
    require(*ticks == (mode == "stall_at_limit" ? 15 : 1), "stall counted");
    const bool unmapped = mode == "stall_coast_unmapped";
    require(input.inputs[io::PILie] == 0.0f && input.inputs[io::PIAltFire] == 0.0f, "lie and alt-fire released");
    require(input.inputs[io::PIFire] == (unmapped ? 1.0f : 0.0f), "fire released only if mapped");
    require(input.inputs[io::PIReload] == (unmapped ? 1.0f : 0.0f), "reload released only if mapped");
    require(input.inputs[io::PIThrottle] == 1.0f, "movement kept while coasting");
}

// Formats the real Debug(Error) callback for exact comparison before ~Debug aborts.
static void fixtureDebug(DebugType type, const std::string& module, const std::string& file, int line,
                         const std::string&, const std::string& text, const std::string&, void*)
{
    std::fprintf(stderr, "DEBUG %d %s %s %d %s\n", static_cast<int>(type), module.c_str(), file.c_str(), line,
                 text.c_str());
    std::fflush(stderr);
}
static bool selectedEmpty() { return view.selected && view.selected->empty(); }
static bool occupiedThree() { return view.occupiedObjects && view.occupiedObjects->size() == 3; }
static std::string timers(int n)
{
    std::string s;
    for (int i = 0; i < n; ++i) s += (s.empty() ? "T" : ",T");
    return s;
}

static void runDecision()
{
    const unsigned int size = mode == "dm_spare_negative" ? 6u : 4u;
    if (mode == "dm_spare_negative") plannedInterleave = 1;
    World w;
    livingBots(w, size);
    w.make(size);
    for (unsigned int i = 0; i < size; ++i)
    {
        w.record(i)->m_priority = static_cast<int>(100 - 10 * i);    // pop order 0, 1, 2, ...
        w.record(i)->m_behaviourUpdateCount = 5;
        w.record(i)->m_double24 = 0.75;
        w.record(i)->m_double56 = 1.0;
        w.record(i)->m_word40 = 3;
    }
    updateTimes.clear();
    for (int i = 0; i < 40; ++i) updateTimes.push_back(10.0 + 0.125 * i);
    if (mode == "dm_empty")
    {
        view.selected->push_back(w.record(0));
        w.decide(1.0);
        require(calls == "T" && view.selected->empty(), "empty queues: only the deadline read, scratch emptied");
        return;
    }
    if (mode == "dm_light_stop" || mode == "dm_light_planned_stop")
    {
        setDebugCallback(DebugTypeError, fixtureDebug, nullptr);
        bots[0].lod = BotLodLight;
        (mode == "dm_light_stop" ? view.queueB : view.queueA)->push(w.record(0));
        stopCalls = "T,l0";
        stopExtra = selectedEmpty;          // the push_back after the Debug is never reached
        w.decide(1.0);
        unexpected("the light-LOD Debug(Error) returned");
    }
    if (mode == "dm_interleave")
    {
        bots[2].lod = BotLodMedium;
        bots[3].lod = BotLodMedium;
        view.queueB->push(w.record(0)); view.queueB->push(w.record(1)); view.queueB->push(w.record(2));
        view.queueA->push(w.record(3));
        w.decide(2.0);
        std::string expected = "T,l0,l1,l2,l3";
        for (int i = 0; i < 4; ++i)
            expected += ",D" + id(i) + ",E" + id(i) + ",l" + id(i) + ",T,d" + id(i) + ",T,T";
        require(calls == expected, "unplanned limit 2 + 1 spare, then one planned; all four decide");
        const double perTick = 2.0 / 20.0;   // weight = 2 full * 6 + 2 medium * 4
        for (int i = 0; i < 4; ++i)
        {
            BotManager::BotInfo* r = w.record(i);
            const double ticksFor = i < 2 ? 6.0 : 4.0;
            require(bots[i].decideSlices.size() == 1 && bots[i].decideSlices[0] == ticksFor * perTick, "LOD share");
            require(r->m_behaviourUpdateCount == 7 && r->m_word40 == 4 && r->m_double24 == 0.0, "banked, counted, aged 0");
            require(r->m_double56 == 1.0 + 0.125 && r->m_priority == 0, "run time added, priority reset");
        }
        require(view.queueA->empty() && view.queueB->empty() && view.selected->size() == 4, "both drained");
        return;
    }
    if (mode == "dm_spare_negative")
    {
        for (unsigned int i = 0; i < 3; ++i) view.queueB->push(w.record(i));
        for (unsigned int i = 3; i < 6; ++i) view.queueA->push(w.record(i));
        w.decide(2.0);
        require(view.selected->size() == 3, "unplanned limit 2 (spare clamped to 0) + planned 1");
        require((*view.selected)[0] == w.record(0) && (*view.selected)[1] == w.record(1) &&
                (*view.selected)[2] == w.record(3), "highest priority first from each queue");
        require(view.queueB->size() == 1 && view.queueA->size() == 2, "the rest wait");
        require(bots[0].decideSlices.size() == 1 && bots[3].decideSlices.size() == 1 && bots[2].decideSlices.empty(),
                "only the selected decide");
        return;
    }
    if (mode == "dm_skips")
    {
        bots[0].dead = true;
        bots[1].equipment = 0;
        view.queueB->push(w.record(0)); view.queueB->push(w.record(1));
        view.queueA->push(w.record(2));
        w.decide(1.0);
        require(calls == "T,l0,l1,l2,D0,D1,E1,D2,E2,l2,T,d2,T,T",
                "dead and equipment-less entries are passed over without a clock check");
        require(w.record(0)->m_priority == 100 && w.record(1)->m_priority == 90, "skipped keep their priority");
        require(w.record(2)->m_priority == 0 && bots[2].decideSlices.size() == 1, "the third decides");
        return;
    }
    if (mode == "dm_zero_slice")
    {
        view.queueB->push(w.record(0));
        updateTimes = {10.0, 10.0};
        w.decide(0.0);
        require(calls == "T,l0,D0,E0,l0,T", "a zero share decides nothing but still resets and checks the clock");
        require(bots[0].decideSlices.empty() && w.record(0)->m_word40 == 3 && w.record(0)->m_priority == 0,
                "no decision counted, priority reset");
        return;
    }
    if (mode == "dm_deadline")
    {
        view.queueB->push(w.record(0)); view.queueB->push(w.record(1));
        updateTimes = {10.0, 10.1, 10.2, 11.5};
        w.decide(1.0);
        require(calls == "T,l0,l1,D0,E0,l0,T,d0,T,T", "the walk ends once the clock passes the deadline");
        require(bots[1].decideSlices.empty() && w.record(1)->m_priority == 90, "the second bot waits");
        return;
    }
    unexpected("unknown decision mode");
}

static void runPathfinding()
{
    World w;
    const unsigned int size = mode == "pf_empty_roster" ? 0u : mode == "pf_skips" ? 6u : mode == "pf_sweep" ? 2u : 1u;
    livingBots(w, size);
    if (mode == "pf_sweep") initialPathCursor = 1;
    w.make(size);
    for (unsigned int i = 0; i < size; ++i)
    {
        *w.navigationActive[i] = true;
        *w.navigationNew[i] = true;
        *fieldsOf(&w.info[i]).mobile = &w.mobile[0];
    }
    vehicleNumbers[0] = 0;
    updateTimes.clear();
    for (int i = 0; i < 20; ++i) updateTimes.push_back(10.0 + 0.001 * i);
    const double budget = ((10.0 + 1.0) - (10.0 + 1.0 * 0.1)) * 0.9;
    if (mode == "pf_empty_roster")
    {
        w.pathfind(1.0);
        require(calls == "T,Z" && pathfinderBudgets.size() == 1 && pathfinderBudgets[0] == budget,
                "the searcher gets 0.9 of the slice left after the bots' tenth");
        return;
    }
    if (mode == "pf_serve")
    {
        w.pathfind(1.0);
        require(calls == "T,T,D0,V0,Q0,N0,N0,E0,K0,p0,Z,D0,V0,Q0,N0,N0,q0", "served, then swept");
        require(bots[0].pathCalls == 1 && bots[0].pathNullCalls == 1 && *view.pathCursor == 0, "one of each");
        require(pathfinderBudgets.size() == 1 && pathfinderBudgets[0] == budget, "searcher budget");
        return;
    }
    if (mode == "pf_skips")
    {
        bots[0].dead = true;
        bots[1].deactivated = true;
        bots[2].squad = false;
        *w.navigationActive[3] = false;
        *w.navigationNew[4] = false;
        *fieldsOf(&w.info[5]).mobile = &w.mobile[1];
        vehicleNumbers[1] = -1;
        w.pathfind(1.0);
        require(calls == "T,T,D0,T,D1,V1,T,D2,V2,Q2,T,D3,V3,Q3,N3,T,D4,V4,Q4,N4,N4,T,D5,V5,Q5,N5,N5,E5,K1,Z,"
                         "D0,D1,V1,D2,V2,Q2,D3,V3,Q3,N3,D4,V4,Q4,N4,N4,q4,D5,V5,Q5,N5,N5,q5",
                "every gate in order; the sweep needs only an active search");
        require(*view.pathCursor == 0 && bots[5].pathCalls == 0, "one full pass; a negative vehicle is skipped");
        return;
    }
    if (mode == "pf_time_out")
    {
        updateTimes = {10.0, 10.2};
        w.pathfind(1.0);
        require(calls == "T,T,Z,D0,V0,Q0,N0,N0,q0", "the bots' tenth already spent: straight to the searcher");
        require(bots[0].pathCalls == 0 && *view.pathCursor == 0, "cursor unmoved");
        return;
    }
    if (mode == "pf_sweep")
    {
        *w.navigationNew[0] = false;
        *w.navigationNew[1] = false;
        w.pathfind(1.0);
        require(calls == "T,T,D1,V1,Q1,N1,N1,T,D0,V0,Q0,N0,N0,Z,D0,V0,Q0,N0,N0,q0,D1,V1,Q1,N1,N1,q1",
                "the pass starts at the cursor and wraps; the sweep takes every active search");
        require(*view.pathCursor == 1 && bots[0].pathNullCalls == 1 && bots[1].pathNullCalls == 1, "wrapped once");
        return;
    }
    unexpected("unknown pathfinding mode");
}

static void runSensing()
{
    World w;
    livingBots(w, 3);
    w.make(3);
    for (unsigned int i = 0; i < 3; ++i)
    {
        w.record(i)->m_sensingPriority = static_cast<int>(30 - 10 * i);
        w.record(i)->m_word44 = 2;
        w.record(i)->m_double64 = 1.0;
    }
    bots[1].lod = BotLodMedium;
    bots[2].lod = BotLodLight;
    updateTimes.clear();
    for (int i = 0; i < 20; ++i) updateTimes.push_back(10.0 + 0.125 * i);
    if (mode == "se_zero_weight")
    {
        w.sense(1.0);
        require(calls == "T" && view.selected->empty(), "an empty queue senses nothing");
        return;
    }
    for (unsigned int i = 0; i < 3; ++i) view.sensing->push(w.record(i));
    if (mode == "se_drop_invalid") bots[0].equipment = 0;
    if (mode == "se_deadline") updateTimes = {10.0, 10.1, 10.2, 12.0};
    w.sense(1.2);
    const double share = (1.2 / 12.0) * 6.0;     // weight = full ticks 6 * two taken
    if (mode == "se_take_two")
    {
        require(calls == "T,E0,l0,E1,l1,l0,T,s0,u0,T,T,l1,T,s1,u1,T,T", "two taken, both sense at the full-LOD rate");
        for (int i = 0; i < 2; ++i)
        {
            require(bots[i].seeSlices.size() == 1 && bots[i].seeSlices[0] == share * 0.25, "see share");
            require(bots[i].senseSlices.size() == 1 && bots[i].senseSlices[0] == share * 0.75, "sensing share");
            require(w.record(i)->m_word44 == 3 && w.record(i)->m_sensingPriority == 0 &&
                    w.record(i)->m_double64 == 1.125, "counted, priority reset, time banked");
        }
        require(view.sensing->size() == 1 && bots[2].seeSlices.empty(), "the third waits");
        return;
    }
    if (mode == "se_drop_invalid")
    {
        require(calls == "T,E0,E1,l1,E2,l2,l1,T,s1,u1,T,T,l2,T,s2,u2,T,T", "an equipment-less bot is dropped, uncounted");
        require(view.sensing->empty() && w.record(0)->m_sensingPriority == 30, "dropped for good");
        return;
    }
    if (mode == "se_deadline")
    {
        require(calls == "T,E0,l0,E1,l1,l0,T,s0,u0,T,T", "the walk ends once the clock passes the deadline");
        require(bots[1].seeSlices.empty() && w.record(1)->m_sensingPriority == 20, "the second waits");
        return;
    }
    unexpected("unknown sensing mode");
}

static void runHearing()
{
    World w;
    initialHearingResume = 9;
    w.make(0);
    view.occupiedObjects->push_back(std::make_pair(1, reinterpret_cast<IAIObject*>(&w.objectA)));
    updateTimes = {10.0};
    if (mode == "he_empty")
    {
        occupied.clear();
        w.hear(1.0);
        require(calls == "T,Y0,G", "emptied, refilled from the engine, game time read");
        require(view.occupiedObjects->empty() && *view.hearingResume == 9, "no pass: the resume id stays");
        return;
    }
    if (mode == "he_resume_stop")
    {
        occupied = {std::make_pair(5, reinterpret_cast<IAIObject*>(&w.objectA)),
                    std::make_pair(9, reinterpret_cast<IAIObject*>(&w.objectB)),
                    std::make_pair(11, reinterpret_cast<IAIObject*>(&w.objectA))};
        stopCalls = "T,Y0,G";
        stopExtra = occupiedThree;
        w.hear(1.0);
        unexpected("the hearing loop returned");
    }
    unexpected("unknown hearing mode");
}

// AIPathfinding::updatePathfinding over the real PathfindingEngine::update template; an in-flight
// search reaches the (header) AStarResource::update stop.
static void runPathfinder()
{
    ObservedTimer timer;
    CommonSingletonServer::m_iBFTimer = &timer;
    PolygonEngine polygon;
    ClusterEngine cluster;
    chosenPolygonEngine = &polygon;
    chosenClusterEngine = &cluster;
    AIPathfinding pathfinding;
    if (mode == "ap_cluster_stop")
    {
        cluster.m_searches.push_back(new AStarResource<ClusterNode, ClusterNodeManager, ClusterNode, unsigned short,
                                                       ClusterNodePath>(nullptr));
        cluster.m_inFlight.push_back(0);
    }
    if (mode == "ap_polygon_stop" || mode == "ap_polygon_spent")
    {
        polygon.m_searches.push_back(new AStarResource<QTSearchNode, QTSearchNodeManager, QTSearchNode, unsigned short,
                                                       QTPath>(nullptr));
        polygon.m_inFlight.push_back(0);
    }
    // deadline base, cluster engine deadline, cluster check, polygon budget, polygon deadline, check
    updateTimes = {10.0, 10.1, 10.2, 10.3, 10.4, mode == "ap_polygon_spent" ? 11.5 : 10.5};
    updateIndex = 0;
    calls.clear();
    if (mode == "ap_cluster_stop") stopCalls = "T,T,T";              // share 1/(0+1): 11.1 > 10.2
    if (mode == "ap_polygon_stop") stopCalls = "T,T,T,T,T,T";        // 10.4 + (11.0 - 10.3) > 10.5
    pathfinding.updatePathfinding(1.0);
    if (!stopCalls.empty()) unexpected("the in-flight search was not stepped");
    require(calls == "T,T,T,T,T,T", "idle: 0/0 share leaves the cluster loop shut; each engine reads twice");
    if (mode == "ap_polygon_spent")
        require(polygon.m_inFlight.size() == 1 && polygon.m_inFlight.front() == 0, "a spent budget steps nothing");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2) unexpected("mode required");
    mode = argv[1];
    std::signal(SIGABRT, onAbort);
    if (mode.compare(0, 6, "stall_") == 0) runStall();
    else if (mode.compare(0, 3, "dm_") == 0) runDecision();
    else if (mode.compare(0, 3, "pf_") == 0) runPathfinding();
    else if (mode.compare(0, 3, "se_") == 0) runSensing();
    else if (mode.compare(0, 3, "he_") == 0) runHearing();
    else if (mode.compare(0, 3, "ap_") == 0) runPathfinder();
    else runExecutor();
    std::printf("PASS %s %d checks\n", mode.c_str(), checks);
    return 0;
}
