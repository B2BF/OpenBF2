// [TEMP] [CHOSEN, not recovered] Focused prepareUpdate caller fixture.
// Actual caller, BotInfo ctor, clearSpawns and InformationManager/pool are retained.
// Native SAI/Information/IPIUnit lifetimes and helper/spawner/virtual receivers are chosen.
#include "sai_prepare_runtime.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "MSVC x86 fixture only");
static_assert(sizeof(SAI::BotInfo) == 32, "PE BotInfo allocation");
static_assert(offsetof(SAI::BotInfo, m_double14) == 24, "PE aligned trailing double");
[[noreturn]] static void unexpected(const char* message)
{
    std::fprintf(stderr, "UNEXPECTED %s\n", message); std::fflush(stderr); std::abort();
}
static void require(bool value, const char* message) { if (!value) unexpected(message); }
struct Event { const char* name; int value; };
static Event events[256];
static unsigned eventCount;
static void trace(const char* name, int value)
{
    require(eventCount < 256, "bounded trace"); events[eventCount++] = {name, value};
}
static std::string mode;
static std::vector<SAI::BotInfo*>* slots;
static std::vector<Bot*> *pending, *removing;
static std::map<int, std::pair<int, int>>* spawnRequests;
static std::map<int, int>* spawnCounts;
static std::list<int>* teleports;
static std::set<int>* unchangedSet;
static float* totals[4];
static Bot* bots[10];
static IAIPlayer* players[10];
static SAI* subject;
static InformationManager* otherManager;
static IAISettings* otherSettings;
struct BotConfig { bool dead = true; unsigned handle = 0; unsigned idCalls = 0; };
static BotConfig botData[10];
struct Allocation { SAI::BotInfo* pointer; bool live; };
static Allocation allocations[40];
static unsigned allocationCount;
static void* deletionWatch;
static unsigned deletionCount;
static bool inCaller;
static SAI::BotInfo* replacementInfo;
static void remember(SAI::BotInfo* pointer)
{
    for (unsigned i = 0; i < allocationCount; ++i) if (allocations[i].pointer == pointer) return;
    require(allocationCount < 40, "bounded allocation records");
    allocations[allocationCount++] = {pointer, true};
}
void* operator new(std::size_t size)
{
    if (void* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept
{
    for (unsigned i = 0; i < allocationCount; ++i)
        if (allocations[i].pointer == pointer) allocations[i].live = false;
    if (pointer && pointer == deletionWatch && inCaller) {
        ++deletionCount; trace("delete", 3);
        require((*slots)[4] != nullptr, "null store occurs after delete");
    }
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

static void checkCtor(SAI::BotInfo* info, Bot* bot)
{
    require(info && info->m_bot == bot && info->m_currentlyOrderedSquare == -1 && info->m_lastOrderedSquare == -1 &&
            info->m_currentlyOccupiedSquare == -1 && info->m_float10 == 0.0f && info->m_double14 == 0.0,
            "actual BotInfo constructor fields precede next callback");
    remember(info);
}
static unsigned observeId(int id)
{
    trace("id", id); const unsigned call = ++botData[id].idCalls;
    if (mode == "pending_dead" && id == 1 && call == 1)
        (*pending)[0] = bots[2]; // Admission retains its initial Bot* across callbacks.
    if (mode == "removal_reload" && id == 8) {
        require(call == 1, "initial removal queue pointer queried once");
        (*removing)[0] = bots[7];
        return 1;
    }
    if (mode == "removal_reload" && id == 7) {
        require(call <= 3, "three fresh queries after queue pointer replacement");
        return call + 1;
    }
    return static_cast<unsigned>(id);
}
static bool observeDead(int id)
{
    trace("dead", id);
    if (mode.find("pending_") == 0 && (*slots)[id]) checkCtor((*slots)[id], bots[id]);
    if (mode == "strength_reload" && id == 1) (*slots)[1] = replacementInfo;
    return botData[id].dead;
}
static IAIPlayer* observePlayer(int id)
{
    trace("player", id);
    require(spawnRequests->count(id) == 1, "spawn map insertion precedes player callback");
    if (mode == "pending_live_end" && id == 1) pending->push_back(bots[2]);
    return players[id];
}
static void observeSpawnGroup(int id, int group)
{
    trace("group", id); require(group == 0, "dead admission resets spawn group to zero");
    checkCtor((*slots)[id], bots[id]);
}
static void observeTransition(int id, bool value)
{
    trace("transition", id); require(value, "dead admission enters transition phase");
}
static unsigned observeEquipment(int id)
{
    trace("equipment", id);
    if (mode == "strength_reload" && id == 2) informationManager = otherManager;
    return botData[id].handle;
}
static int settingsCalls;
static bool observeRespawn(int id)
{
    trace("respawn", id); ++settingsCalls;
    require(pending->empty() && pending->capacity() == 0 && teleports->empty(), "earlier queues cleared before spawn traversal");
    if (mode == "spawn_mutation" && settingsCalls == 1) {
        (*slots)[1] = replacementInfo;
        (*spawnRequests)[1].first = 91;
        CommonSingletonServer::m_iaiSettings = otherSettings;
    }
    return mode == "spawn_mutation";
}
struct InfoConfig
{
    IPIUnit** unitSlot;
    IPIPhysical** physicalSlot;
    float strengths[6];
    int category;
};
static InfoConfig information[4];
static int constructing;
static int observeCategory(int id)
{
    trace("category", id);
    for (int i = 0; i < 6; ++i)
        require(std::memcmp(&totals[2][i], &information[id].strengths[i], sizeof(float)) == 0,
                "all six assignments precede physical category callback");
    return information[id].category;
}

#include "sai_prepare_mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [TEMP] Native member construction, with typed handles to fixture inputs only.
ISAI::~ISAI() {}
SAI::SAI(int side, int, int, Vec2, Vec2) : m_side(side)
{
    slots = &m_bots; pending = &m_vector94; removing = &m_vectorA0;
    spawnRequests = &m_map5C; spawnCounts = &m_map1D8; teleports = &m_list110;
    unchangedSet = &m_assignedBots;
    totals[0] = m_enemyStrengths; totals[1] = m_enemyCounts;
    totals[2] = m_friendlyStrengths; totals[3] = m_friendlyCounts;
    m_bots.resize(10, nullptr); m_vector94.reserve(10); m_vectorA0.reserve(10);
    for (int a = 0; a < 4; ++a) for (int i = 0; i < 6; ++i) totals[a][i] = float(100 * (a + 1) + i);
}
SAI::~SAI() {}
Bot::~Bot() {}
int Bot::getBotType() const { unexpected("Bot::getBotType"); }
BAPContext* Bot::getScriptContext() { unexpected("Bot::getScriptContext"); }
IAIPlayer::~IAIPlayer() {}
IAISettings::IAISettings() {}
IAISettings::~IAISettings() {}
void* CommonSingletonServer::m_iaiSettings = nullptr;
Information::Information(IAIObject* object, double time)
    : m_time(time), m_object(object), m_physical(nullptr), m_unit(nullptr)
{
    information[constructing].unitSlot = &m_unit;
    information[constructing].physicalSlot = &m_physical;
}
Information::~Information() {}
int Information::getInformationSort() { unexpected("Information::getInformationSort"); }

IPIUnit::IPIUnit(Information* owner)
    : m_battleStrengths(information[constructing].strengths), m_strategicStrengths(nullptr),
      m_strengthMod(0), m_ammo(0), m_ammoTime(0), m_ownsPointer24(false), m_pointer24(nullptr)
{ m_information = owner; }
IPIUnit::~IPIUnit() {}
int IPIUnit::getType() { unexpected("IPIUnit::getType"); }
void IPIUnit::setTime() { unexpected("IPIUnit::setTime"); }
void IPIUnit::setTime(double) { unexpected("IPIUnit::setTime(double)"); }

int IPIPhysical::getType() { unexpected("IPIPhysical::getType"); }

// Chosen receiving helpers: their production frontiers are not bypassed in a game build.
void SAI::setBotIsAlive(int id)
{
    trace("alive", id); require(this == subject && id == 1, "alive callback receiver/argument");
    checkCtor((*slots)[id], bots[id]);
    require(spawnRequests->empty(), "alive admission does not insert a dead-bot request");
    botData[id].dead = true; // Keep the later independently tested strength pass empty here.
}
void SAI::handleTeleportingBot(int id)
{
    trace("teleport", id); require(this == subject, "teleport receiver");
    require(pending->empty() && pending->capacity() == 0, "pending buffer released before teleports");
    require(spawnCounts->size() == 2 && spawnCounts->at(4) == 0, "spawn keys retained and counters zeroed");
    require(totals[2][0] == 300.0f && totals[3][5] == 405.0f, "teleports precede friendly reset");
    if (id == 11) {
        require(spawnCounts->at(9) == 0, "first teleport sees zeroed count");
        spawnCounts->at(9) = 3; teleports->push_back(22);
    } else require(id == 22 && spawnCounts->at(9) == 3, "appended teleport and one clear sweep");
}
void SAI::BotInfo::remove(SAI* owner)
{
    trace("remove", int(m_bot == bots[2] ? 2 : m_bot == bots[1] ? 1 : 9));
    require(owner == subject, "remove owner");
    if (mode == "removal_reload") {
        require(this == (*slots)[2], "second id selects remove receiver");
        (*slots)[3] = replacementInfo;
        deletionWatch = replacementInfo;
    } else if (mode == "removal_live_end" && m_bot == bots[1]) {
        removing->push_back(bots[2]);
    }
}
bool BotSpawner::findSuitableSpawnGroup(float* enemyStrengths, float* enemyCounts,
                                      float* friendlyStrengths, float* friendlyCounts,
                                      int strategicObjectId, Bot* bot)
{
    const int id = bot == bots[1] ? 1 : bot == bots[2] ? 2 : -1;
    trace("spawn", id);
    require(enemyStrengths == totals[0] && enemyCounts == totals[1] &&
            friendlyStrengths == totals[2] && friendlyCounts == totals[3], "spawner array argument order");
    require(id > 0, "captured pre-settings bot forwarded");
    if (id == 1) {
        require(strategicObjectId == 91, "map pair reloaded after settings callback");
        spawnRequests->insert({2, {72, 88}});
    } else require(strategicObjectId == 72, "new map entry visited with its first payload");
    return false; // Caller discards the real Boolean contract; no entry is removed on false.
}
} } }

static SAI::BotInfo* makeInfo(int id)
{
    SAI::BotInfo* result = new SAI::BotInfo(bots[id]); remember(result); return result;
}
static void checkTrace(const char* expected)
{
    std::string actual;
    for (unsigned i = 0; i < eventCount; ++i) {
        if (i) actual += ',';
        actual += events[i].name; actual += std::to_string(events[i].value);
    }
    if (actual != expected) {
        std::fprintf(stderr, "FAIL trace\nexpected: %s\nactual:   %s\n", expected, actual.c_str()); std::exit(2);
    }
}

int main(int argc, char** argv)
{
    require(argc == 2, "one mode"); mode = argv[1];
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    InformationManager firstManager, secondManager;
    informationManager = &firstManager; otherManager = &secondManager;
    ObservedBot botObjects[10]; ObservedPlayer playerObjects[10];
    for (int i = 0; i < 10; ++i) {
        botObjects[i].identity = playerObjects[i].identity = i;
        bots[i] = &botObjects[i]; players[i] = &playerObjects[i];
    }
    ObservedSettings settings, alternateSettings; alternateSettings.identity = 1;
    CommonSingletonServer::m_iaiSettings = &settings; otherSettings = &alternateSettings;
    ObservedInformation* info[4]; IPIUnit* units[4]; ObservedPhysical physical[4];
    unsigned handles[4];
    for (int i = 0; i < 4; ++i) {
        constructing = i; info[i] = new ObservedInformation;
        units[i] = new IPIUnit(info[i]); *information[i].unitSlot = units[i];
        physical[i].identity = i; *information[i].physicalSlot = &physical[i];
        information[i].category = i;
        for (int c = 0; c < 6; ++c) information[i].strengths[c] = float(10 * (i + 1) + c);
        handles[i] = firstManager.getPool()->acquire(info[i]);
    }
    const unsigned alternateHandle = secondManager.getPool()->acquire(info[3]);
    require(alternateHandle == handles[0], "parallel genuine pools provide same initial handle");
    SAI sai(1, 0, 0, Vec2(), Vec2()); subject = &sai;
    unchangedSet->insert(1); unchangedSet->insert(42);
    (*spawnCounts)[4] = 8; (*spawnCounts)[9] = -2;
    const char* expected = "";
    int expectedStrength = -1;
    float expectedCounts[6] = {};

    if (mode == "empty") {
    } else if (mode == "pending_nulls") {
        pending->push_back(nullptr); pending->push_back(nullptr);
    } else if (mode == "pending_dead") {
        pending->push_back(bots[1]); (*spawnRequests)[1] = {77, 88};
        expected = "id1,dead1,id1,player1,group1,transition1,id1,dead1,respawn0";
    } else if (mode == "pending_alive") {
        pending->push_back(bots[1]); botData[1].dead = false;
        expected = "id1,dead1,id1,alive1,id1,dead1";
    } else if (mode == "pending_live_end") {
        pending->push_back(bots[1]);
        expected = "id1,dead1,id1,player1,group1,transition1,id1,id2,dead2,id2,player2,group2,transition2,id2,dead1,dead2,respawn0,respawn0";
    } else if (mode == "teleport_live_end") {
        teleports->push_back(11); expected = "teleport11,teleport22";
    } else if (mode == "strength_filters") {
        for (int i = 1; i <= 4; ++i) (*slots)[i] = makeInfo(i);
        botData[2].dead = botData[3].dead = botData[4].dead = false;
        botData[3].handle = handles[0] ^ 0x10000u; // Mismatched generation.
        botData[4].handle = handles[1]; *information[1].unitSlot = nullptr;
        expected = "dead1,dead2,equipment2,dead3,equipment3,dead4,equipment4";
    } else if (mode == "strength_overwrite") {
        for (int i = 1; i <= 3; ++i) {
            (*slots)[i] = makeInfo(i); botData[i].dead = false; botData[i].handle = handles[i - 1];
        }
        information[0].category = information[1].category = 2; information[2].category = 5;
        information[2].strengths[0] = -0.0f;
        expectedStrength = 2; expectedCounts[2] = 2.0f; expectedCounts[5] = 1.0f;
        expected = "dead1,equipment1,category0,dead2,equipment2,category1,dead3,equipment3,category2";
    } else if (mode == "strength_reload") {
        (*slots)[1] = makeInfo(1); replacementInfo = makeInfo(2);
        botData[1].dead = false; botData[2].handle = handles[0];
        expectedStrength = 3; expectedCounts[3] = 1.0f;
        expected = "dead1,equipment2,category3";
    } else if (mode == "spawn_nulls") {
        (*spawnRequests)[1] = {71, 81}; (*spawnRequests)[3] = {73, 83};
        CommonSingletonServer::m_iaiSettings = nullptr;
    } else if (mode == "spawn_disabled") {
        (*slots)[1] = makeInfo(1); (*spawnRequests)[1] = {71, 81};
        expected = "dead1,respawn0";
    } else if (mode == "spawn_mutation") {
        (*slots)[1] = makeInfo(1); (*slots)[2] = makeInfo(2); replacementInfo = makeInfo(9);
        (*spawnRequests)[1] = {71, 81};
        expected = "dead1,dead2,respawn0,spawn1,respawn1,spawn2";
    } else if (mode == "removal_absent") {
        removing->push_back(bots[8]); expected = "id8";
    } else if (mode == "removal_reload") {
        for (int i = 1; i <= 4; ++i) (*slots)[i] = makeInfo(i);
        replacementInfo = makeInfo(9); removing->push_back(bots[8]);
        expected = "dead1,dead2,dead3,dead4,id8,id7,remove2,id7,delete3,id7";
    } else if (mode == "removal_live_end") {
        (*slots)[1] = makeInfo(1); (*slots)[2] = makeInfo(2); removing->push_back(bots[1]);
        expected = "dead1,dead2,id1,id1,remove1,id1,id1,id2,id2,remove2,id2,id2";
    } else unexpected("unknown mode");

    inCaller = true; sai.prepareUpdate(); inCaller = false;
    checkTrace(expected);
    require(pending->empty() && pending->capacity() == 0 && removing->empty() && removing->capacity() == 0,
            "PE releases both queue buffers, including pre-reserved empty queues");
    require(teleports->empty(), "teleport list cleared");
    require(spawnCounts->size() == 2 && spawnCounts->at(4) == 0 &&
            spawnCounts->at(9) == (mode == "teleport_live_end" ? 3 : 0), "spawn map payload reset preserves keys");
    require(unchangedSet->size() == 2 && unchangedSet->count(1) && unchangedSet->count(42), "find does not erase found set entry");
    for (int i = 0; i < 6; ++i) {
        require(totals[0][i] == float(100 + i) && totals[1][i] == float(200 + i), "enemy arrays unchanged");
        const float value = expectedStrength < 0 ? 0.0f : information[expectedStrength].strengths[i];
        require(std::memcmp(&totals[2][i], &value, 4) == 0 && totals[3][i] == expectedCounts[i], "exact final friendly strengths/counts");
    }
    if (mode == "pending_dead") require(spawnRequests->at(1) == std::make_pair(77, 88), "insert retains existing request payload");
    if (mode == "pending_live_end") {
        require(slots->at(1) && slots->at(2), "both admitted native infos retained");
        require(spawnRequests->at(1) == std::make_pair(-1, 13) && spawnRequests->at(2) == std::make_pair(-1, 13), "new request payloads");
    }
    if (mode == "spawn_nulls") require(spawnRequests->empty(), "null-slot requests erased safely");
    if (mode == "spawn_disabled") require(spawnRequests->size() == 1 && spawnRequests->at(1) == std::make_pair(71, 81), "disabled request retained");
    if (mode == "spawn_mutation") require(spawnRequests->size() == 2 && settingsCalls == 2, "false spawner results retained and appended map entry visited");
    if (mode == "removal_reload") {
        require(botData[8].idCalls == 1 && botData[7].idCalls == 3 && deletionCount == 1 && slots->at(4) == nullptr && slots->at(3) == replacementInfo,
                "independent remove/delete/clear IDs and post-remove reload");
    }
    if (mode == "removal_live_end") require(!slots->at(1) && !slots->at(2), "both live-end removal entries cleared");
    for (SAI::BotInfo* pointer : *slots) if (pointer) remember(pointer);
    for (unsigned i = 0; i < allocationCount; ++i) if (allocations[i].live) delete allocations[i].pointer;
    for (int i = 0; i < 4; ++i) { delete units[i]; delete info[i]; }
    std::printf("PASS %s\n", mode.c_str());
    return 0;
}
