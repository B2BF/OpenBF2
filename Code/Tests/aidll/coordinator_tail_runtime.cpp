// [TEMP]
// Chosen native lifetimes and typed observers trace coordinator calls; unexpected virtuals abort.
#include "coordinator_tail_runtime.h"
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "strict MSVC x86 fixture");
static unsigned checks;
static std::string mode, calls;
[[noreturn]] static void unexpected(const char* reason)
{ std::fprintf(stderr, "FAIL %s\n", reason); std::fflush(stderr); std::exit(2); }
static void require(bool value, const char* reason) { ++checks; if (!value) unexpected(reason); }
static void trace(const std::string& text) { if (!calls.empty()) calls += ','; calls += text; }
static std::function<void()> abortCheck;
static void onAbort(int)
{ require(bool(abortCheck), "unexpected abort"); abortCheck(); std::printf("VERIFIED %s\n", mode.c_str()); std::fflush(stdout); }
static float fromBits(std::uint32_t bits) { float value; std::memcpy(&value, &bits, 4); return value; }
static std::uint32_t toBits(float value) { std::uint32_t bits; std::memcpy(&bits, &value, 4); return bits; }
static Vec3 vecBits(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return Vec3{fromBits(x), fromBits(y), fromBits(z)}; }
static bool equal(const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// observer state
static SAI* commanderSai;
static std::function<bool(int)> alive = [](int) { return true; };
static std::function<bool(int)> down = [](int) { return false; };
static std::function<void(int)> aliveHook;
static CommunicationAgent* radios[4];
static unsigned requests;
static int requestSquad;
static IAIPlayer* requestPlayer;
static unsigned controlPointCalls;
static bool controlPointActive;
static Vec3 controlPointPosition{};
static float controlPointRadius = 10.0f;
static unsigned clockCalls;
static SquadFormation* testedFormation;
static const std::list<AIRadioMessageInfo>* inboxSeed;
static bool commanderSeed;   // the chosen SAI lifetime's private commander flag

static SAI* observeSai(int side) { trace("sai" + std::to_string(side)); return commanderSai; }
static bool observeAlive(int id) { trace("alive" + std::to_string(id)); if (aliveHook) aliveHook(id); return alive(id); }
static bool observeDown(int id) { trace("down" + std::to_string(id)); return down(id); }
static CommunicationAgent* observeRadio(int id) { trace("radio" + std::to_string(id)); return radios[id]; }
static int observeRequest(int squad, IAIPlayer* player)
{ trace("request" + std::to_string(squad)); ++requests; requestSquad = squad; requestPlayer = player; return 77; }
static bool observeControlPoint(unsigned int index, Vec3& position, float& radius, bool& active)
{
    trace("cp" + std::to_string(index)); ++controlPointCalls; controlPointActive = active;
    position = controlPointPosition; radius = controlPointRadius;
    return false;   // discarded by the caller in both images
}
class ObservedTimer : public IBFTimer {
public:
    double getGameTime() const override { ++clockCalls; require(!testedFormation->m_b56, "flag clears before clock"); return 20.125; }
    double getTimeIncrease() const override { unexpected("time increase"); }
    double getUpdateTime() const override { unexpected("update clock"); }
    double getWallClockTime() const override { unexpected("wall clock"); }
    void setPause(bool) override { unexpected("pause"); }
    bool getPause() override { unexpected("pause query"); }
    void reset() override { unexpected("timer reset"); }
};
#include "mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Native lifetimes for the objects the tail touches; no publication.
void* CommonSingletonServer::m_iBFTimer = nullptr;
void* CommonSingletonServer::m_iBFSquad = nullptr;
void* CommonSingletonServer::m_iaiGame = nullptr;
IAIMain* IAIMain::m_instance = nullptr;
IAIMain::IAIMain() {} IAIMain::~IAIMain() {}
IBFTimer::IBFTimer() {} IBFTimer::~IBFTimer() {}
IAIPlayer::~IAIPlayer() {}
Bot::~Bot() {}
IBFSquad::IBFSquad() {} IBFSquad::~IBFSquad() {}
IAIGame::IAIGame() {} IAIGame::~IAIGame() {}
ISAI::~ISAI() {}
SAI::SAI(int side, int, int, Vec2, Vec2) : m_side(side), m_flag1D1(commanderSeed) {}
SAI::~SAI() {}
AIStrategicObject::AIStrategicObject(float, float) { for (int i = 0; i < 4; ++i) m_status[i] = StatusMap::Unknown; }
AIStrategicObject::~AIStrategicObject() {}
AIStrategicArea::AIStrategicArea(const std::string& name, unsigned int id, float radius)
    : AIStrategicObject(radius, 0.0f), m_name(name), m_controlPointId(id) {}
AIStrategicArea::~AIStrategicArea() {}
SquadCoordinator::SquadCoordinator(int side, int index)
    : m_squadIndex(index), m_side(side), m_strategicAreaAssignment(nullptr), m_i40(4), m_squadLeaderIndex(0),
      m_formation(nullptr), m_d124(0.0), m_b172(false), m_b196(false), m_needOrderMessageTimer(100.0)
{ m_v28 = {}; m_v108 = {4, 4, 4}; m_v176 = {}; m_d188 = 0.0; }
SquadFormation::SquadFormation(SquadCoordinator* coordinator)
    : m_coordinator(coordinator), m_numberOfSlots(6), m_b56(false), m_b84(false), m_b85(false), m_d92(0.0)
{ m_v60 = {}; m_v72 = {1, 2, 3}; }
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player, int squadIndex, int side, int index)
    : m_squadIndex(squadIndex), m_memberIndex(index), m_isSquadLeader(false), m_playerId(0), m_player(player),
      m_bot(nullptr), m_squad(squad), m_side(side), m_havePath(true), m_positionValid(true), m_flag4A(false),
      m_moved(false)
{ m_position = {}; }
SquadMember::~SquadMember() {}
CommunicationAgent::CommunicationAgent(BotMain*)
    : m_bot(nullptr), m_hub(nullptr), m_receivedMessages(18)
{ if (inboxSeed) m_receivedMessages[12] = *inboxSeed; }
} } }

struct Fixture
{
    SquadCoordinator squad;
    SquadFormation formation;
    SquadFormation::Slot slots[6];
    ObservedPlayer players[4];
    ObservedBot bots[4];
    SquadMember members[4];
    SAI sai;
    ObservedMain main;
    ObservedSquads squads;
    ObservedGame game;
    ObservedTimer timer;
    AIStrategicArea area;
    Fixture()
        : squad(0, 5), formation(&squad),
          members{{&squad, &players[0], 5, 0, 0}, {&squad, &players[1], 5, 0, 1},
                  {&squad, &players[2], 5, 0, 2}, {&squad, &players[3], 5, 0, 3}},
          sai(1, 0, 0, Vec2(), Vec2()), area("fixture", 502, 10.0f)
    {
        squad.m_formation = &formation; testedFormation = &formation;
        for (auto& slot : slots) formation.m_slots.push_back(&slot);
        for (int i = 0; i < 4; ++i)
        {
            players[i].identity = bots[i].identity = i;
            members[i].m_bot = &bots[i];
            members[i].m_playerId = 7 + i;
            members[i].m_position = {static_cast<float>(i), 0, 0};
        }
        squad.m_squadMembers = {&members[0], &members[1], &members[2]};
        IAIMain::m_instance = &main; commanderSai = &sai;
        CommonSingletonServer::m_iBFSquad = &squads;
        CommonSingletonServer::m_iaiGame = &game;
        CommonSingletonServer::m_iBFTimer = &timer;
        // Not owned by either side: StatusMap::Owned is 0, and 0 is what the tail tests.
        area.m_status[0] = area.m_status[1] = StatusMap::Hostile;
    }
    bool pathsIntact() const
    { for (const auto& m : members) if (!m.m_havePath || m.m_flag4A) return false; return true; }
    bool pathsCleared(int count) const
    { for (int i = 0; i < count; ++i) if (members[i].m_havePath || !members[i].m_flag4A) return false; return true; }
};

static SquadCoordinator::SquadMemberFire* fire(SquadMember* member, double time, Vec3 position)
{
    auto* record = new SquadCoordinator::SquadMemberFire();
    record->m_time = time; record->m_member = member; record->m_position = position; record->m_playerId = member->m_playerId;
    return record;
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::signal(SIGABRT, onAbort);
    if (argc != 2) unexpected("mode required");
    mode = argv[1];
    std::list<AIRadioMessageInfo> inbox;
    AIRadioMessageInfo message{};
    if (mode.rfind("incoming_", 0) == 0)
    {
        message.m_senderPlayerId = mode == "incoming_other_sender" ? 99 : 7;
        if (mode != "incoming_empty") { inbox.push_back(message); message.m_senderPlayerId = 99; inbox.push_back(message); }
        inboxSeed = &inbox;
    }
    CommunicationAgent radioA(nullptr), radioB(nullptr);
    radios[0] = radios[2] = radios[3] = &radioA; radios[1] = &radioB;
    commanderSeed = mode.rfind("orders_", 0) == 0 && mode != "orders_not_commander";
    Fixture fx;
    auto& squad = fx.squad;
    auto& formation = fx.formation;
    auto* m = fx.members;

    if (mode.rfind("position_", 0) == 0)
    {
        for (int i = 0; i < 4; ++i) m[i].m_position = {10.0f * i + 1, 2, 3};
        std::string expected;
        if (mode == "position_anchor") { fx.slots[0].m_memberIndex = 2; expected = "alive2"; }
        else if (mode == "position_leader") { fx.slots[0].m_memberIndex = -1; squad.m_squadLeaderIndex = 1; expected = "alive1"; }
        else if (mode == "position_first_alive")
        {
            alive = [](int id) { return id == 2; };
            aliveHook = [&](int id) { if (id == 2) squad.m_squadMembers[2] = &m[3]; };
            expected = "alive0,alive0,alive1,alive2";
        }
        else if (mode == "position_nobody") { alive = [](int) { return false; }; expected = "alive0,alive0,alive1,alive2"; }
        else if (mode == "position_invalid_copy") { m[0].m_positionValid = false; expected = "alive0"; }
        else unexpected("position mode");
        squad.updateSquadPosition();
        require(calls == expected, "exact alive query order");
        const Vec3 want = mode == "position_anchor" ? m[2].m_position : mode == "position_leader" ? m[1].m_position :
            mode == "position_first_alive" ? m[3].m_position : mode == "position_nobody" ? Vec3{4, 4, 4} : m[0].m_position;
        require(equal(squad.m_v108, want), "squad position source");
        require(fx.pathsIntact() && clockCalls == 0, "position update has no other effect");
    }
    else if (mode.rfind("shots_", 0) == 0)
    {
        squad.m_d124 = 50.0;
        fx.slots[0].hasSquadMemberAssigned = true;   // make the forced movement update observable
        int resets = 0; std::size_t left = 0;
        if (mode == "shots_empty") {}
        else if (mode == "shots_invalid") { m[1].m_positionValid = false; squad.m_list12.push_back(fire(&m[1], 60.0, m[1].m_position)); }
        else if (mode == "shots_expired") { squad.m_list12.push_back(fire(&m[1], 49.0, m[1].m_position)); resets = 1; }
        else if (mode == "shots_equal_time") { squad.m_list12.push_back(fire(&m[1], 50.0, m[1].m_position)); left = 1; }
        else if (mode == "shots_moved") { squad.m_list12.push_back(fire(&m[1], 60.0, {m[1].m_position.x + 5.5f, 0, 0})); resets = 1; }
        else if (mode == "shots_edge") { squad.m_list12.push_back(fire(&m[1], 60.0, {m[1].m_position.x + 3, 4, 0})); left = 1; }
        else if (mode == "shots_mixed")
        {
            m[2].m_positionValid = false;
            squad.m_list12.push_back(fire(&m[2], 60.0, m[2].m_position));   // quiet removal
            squad.m_list12.push_back(fire(&m[1], 60.0, m[1].m_position));   // kept
            squad.m_list12.push_back(fire(&m[0], 10.0, m[0].m_position));   // expired, resets
            resets = 1; left = 1;
        }
        else unexpected("shots mode");
        SquadCoordinator::SquadMemberFire* kept = left ? *std::next(squad.m_list12.begin(), mode == "shots_mixed" ? 1 : 0) : nullptr;
        squad.updateShotsFired();
        require(squad.m_list12.size() == left, "records kept");
        if (left) require(squad.m_list12.front() == kept, "the kept record is the same object");
        require(resets ? fx.pathsCleared(3) && m[3].m_havePath : fx.pathsIntact(), "path reset only on expiry/movement");
        require(clockCalls == static_cast<unsigned>(resets), "forced movement stamp only on expiry/movement");
        require(formation.m_b56 == (resets != 0), "movement flag written by the forced update");
        for (auto* record : squad.m_list12) delete record;
    }
    else if (mode.rfind("repair_", 0) == 0)
    {
        squad.m_b172 = mode != "repair_idle"; squad.m_b196 = true; squad.m_d188 = 10.0; squad.m_v176 = {5, 6, 7};
        double time = 20.0;
        if (mode == "repair_equal") time = 10.0;
        if (mode == "repair_nan") squad.m_d188 = std::numeric_limits<double>::quiet_NaN();
        squad.updateRepairRequest(time);
        const bool expired = mode == "repair_expired";
        require(squad.m_b172 == (mode != "repair_idle" && !expired), "request flag");
        require(squad.m_b196 == !expired, "confirmation cleared only on expiry");
        require(equal(squad.m_v176, {5, 6, 7}) && (mode == "repair_nan" || squad.m_d188 == 10.0), "position and deadline retained");
    }
    else if (mode.rfind("orders_", 0) == 0)
    {
        squad.m_v28 = {0, 0, 0}; m[0].m_position = {3, 0, 0};   // 9 < 225
        double time = 125.0; bool reset = true; std::string expected = "sai1";
        const std::string leaderChecks = ",alive0,down0";
        if (mode == "orders_not_commander") {}
        else if (mode == "orders_taking_flag")
        {
            squad.m_strategicAreaAssignment = &fx.area; controlPointPosition = {3, 0, 1};
            expected += ",cp502,alive0,down0";
        }
        else if (mode == "orders_order_state") squad.m_i40 = 1;
        else if (mode == "orders_null_leader") squad.m_squadMembers[0] = nullptr;
        else if (mode == "orders_human_leader") m[0].m_bot = nullptr;
        else if (mode == "orders_moving") m[0].m_moved = true;
        else if (mode == "orders_dead") { alive = [](int) { return false; }; expected += ",alive0"; }
        else if (mode == "orders_man_down") { down = [](int) { return true; }; expected += leaderChecks; }
        else if (mode == "orders_far_hostile")
        { m[0].m_position = {15, 0, 0}; squad.m_strategicAreaAssignment = &fx.area; squad.m_i40 = 2; expected += ",cp502,alive0,down0,alive1,down1,alive2,down2" + leaderChecks; controlPointRadius = 0.5f; }
        else if (mode == "orders_far_unassigned") { m[0].m_position = {15, 0, 0}; expected += leaderChecks; }
        else if (mode == "orders_waiting") { time = 120.0; reset = false; expected += leaderChecks; }
        else if (mode == "orders_invalid_sentinel") { m[0].m_positionValid = false; expected += leaderChecks; }
        else if (mode == "orders_send_stop" || mode == "orders_owned_area_stop")
        {
            if (mode == "orders_owned_area_stop")
            {
                m[0].m_position = {20, 0, 0}; squad.m_strategicAreaAssignment = &fx.area; squad.m_i40 = 0;
                fx.area.m_status[0] = StatusMap::Owned;   // skips the taking-flag query entirely
            }
            abortCheck = [&, expected, leaderChecks] {
                require(calls == expected + leaderChecks + ",request6,radio0", "request precedes the radio lookup");
                require(requests == 1 && requestSquad == 6 && requestPlayer == &fx.players[0], "engine request: one-based squad, leader's player");
                require(squad.m_needOrderMessageTimer == 100.0, "timer not yet restarted at the stop");
            };
            squad.updateNeedOrdersMessage(time);
            unexpected("the send arm should stop at the radio");
        }
        else unexpected("orders mode");
        squad.updateNeedOrdersMessage(time);
        require(calls == expected, "exact collaborator order");
        require(squad.m_needOrderMessageTimer == (reset ? time : 100.0), "timer restart on every tick but the waiting one");
        require(requests == 0, "no engine request");
    }
    else if (mode.rfind("incoming_", 0) == 0)
    {
        m[0].m_bot = nullptr;   // a human leader unless the mode says otherwise
        formation.m_b84 = true;
        std::string expected = "alive1,radio1";
        if (mode == "incoming_bot_leader") { m[0].m_bot = &fx.bots[0]; expected = ""; }
        else if (mode == "incoming_null_leader") { squad.m_squadMembers[0] = nullptr; expected = ""; }
        else if (mode == "incoming_not_leaving") { formation.m_b84 = false; expected = ""; }
        else if (mode == "incoming_no_bot") { alive = [](int) { return false; }; expected = "alive1,alive2"; }
        else if (mode == "incoming_empty" || mode == "incoming_other_sender") {}
        else if (mode == "incoming_rejoin_stop")
        {
            abortCheck = [&, expected] {
                require(calls == expected, "first living bot's radio");
                require(formation.m_b85 && !formation.m_b84, "the formation rejoins before the radio send");
                require(fx.pathsIntact(), "paths are invalidated only after the send");
            };
            squad.updateIncomingMessages();
            unexpected("rejoin should stop at the radio");
        }
        else unexpected("incoming mode");
        squad.updateIncomingMessages();
        require(calls == expected, "exact collaborator order");
        require(!formation.m_b85 && formation.m_b84 == (mode != "incoming_not_leaving") && fx.pathsIntact(), "no rejoin");
    }
    else if (mode.rfind("taking_", 0) == 0)
    {
        squad.m_strategicAreaAssignment = &fx.area; controlPointPosition = {0, 0, 0}; controlPointRadius = 10.0f;
        bool expected = false; std::string trail = "cp502,alive0,down0,alive1,down1,alive2,down2";
        m[0].m_position = {20, 0, 0}; m[1].m_position = {0, 20, 0}; m[2].m_position = {0, 0, 20};
        if (mode == "taking_no_area") { squad.m_strategicAreaAssignment = nullptr; trail = ""; }
        else if (mode == "taking_owned") { fx.area.m_status[0] = StatusMap::Owned; trail = ""; }
        else if (mode == "taking_no_control_point") { fx.area.m_controlPointId = static_cast<unsigned int>(-1); trail = ""; }
        else if (mode == "taking_inside") { m[1].m_position = {3, 4, 0}; expected = true; trail = "cp502,alive0,down0,alive1,down1"; }
        else if (mode == "taking_edge") { m[0].m_position = {6, 8, 0}; m[1].m_position = {10, 0, 0}; }
        else if (mode == "taking_excluded")
        {
            alive = [](int id) { return id != 0; }; down = [](int id) { return id == 1; };
            for (int i = 0; i < 3; ++i) m[i].m_position = {1, 0, 0};
            m[2].m_positionValid = false; trail = "cp502,alive0,alive1,down1,alive2,down2";
        }
        else unexpected("taking mode");
        require(squad.isSquadTakingFlag() == expected, "taking-flag verdict");
        require(calls == trail, "exact query order");
        if (!trail.empty()) require(controlPointActive, "the discarded active output is seeded true");
    }
    else if (mode == "control_point_query")
    {
        Vec3 position{}; float radius = -1.0f; controlPointPosition = {7, 8, 9}; controlPointRadius = 12.5f;
        require(fx.area.getControlPointPosAndRadius(position, radius), "assigned id answers true even though the query returned false");
        require(equal(position, {7, 8, 9}) && radius == 12.5f && controlPointActive && calls == "cp502", "outputs forwarded");
        fx.area.m_controlPointId = static_cast<unsigned int>(-1); position = {}; radius = -1.0f;
        require(!fx.area.getControlPointPosAndRadius(position, radius) && controlPointCalls == 1, "unassigned id: false, no query");
        require(radius == -1.0f && equal(position, {}), "outputs untouched when unassigned");
    }
    else if (mode == "witness_moved_wide")
    {
        const Vec3 f = vecBits(1084227580u, 864026624u, 0), p = vecBits(3051356160u, 0, 0);
        const double dx = static_cast<double>(f.x) - p.x, dy = static_cast<double>(f.y) - p.y;
        require(fixtureShooterMovedAway(f, p), "PE spills dx: moved");
        require(!((dy * dy) + dx * dx > 25.0), "witness separates from a wide dx");
        require(!fixtureShooterMovedAway({5, 0, 0}, {}) && fixtureShooterMovedAway({5.5f, 0, 0}, {}), "strict 25 edge");
        require(!fixtureShooterMovedAway({std::numeric_limits<float>::quiet_NaN(), 0, 0}, {}), "NaN keeps the record");
    }
    else if (mode == "witness_moved_naive")
    {
        const Vec3 f = vecBits(1084227328u, 864026624u, 0), p = vecBits(3103784960u, 0, 0);
        volatile float dx = f.x - p.x, dy = f.y - p.y; volatile float naive = dy * dy + dx * dx;
        require(fixtureShooterMovedAway(f, p) && !(naive > 25.0f), "wide squares separate from float squares");
    }
    else if (mode == "witness_goto_wide")
    {
        const Vec3 p = vecBits(1097859068u, 0, 0), g = vecBits(3059744768u, 0, 0);
        const double d = static_cast<double>(p.x) - g.x;
        require(toBits(fixtureLeaderGotoDistanceSq(p, g)) == 1130430464u, "PE key is exactly 225.0f: not near");
        require(static_cast<float>(d * d) < 225.0f, "witness separates from a wide delta");
    }
    else if (mode == "witness_inside_wide")
    {
        const Vec3 c = vecBits(1092616189u, 0, 0), p = vecBits(3051356160u, 0, 0);
        const float r2 = fromBits(1120403453u);
        const double d = static_cast<double>(c.x) - p.x;
        require(!fixtureInsideControlPointRadius(c, p, r2) && d * d < r2, "float delta separates from a wide delta");
        require(!fixtureInsideControlPointRadius({10, 0, 0}, {}, 100.0f) && fixtureInsideControlPointRadius({9, 0, 0}, {}, 100.0f), "strict radius edge");
    }
    else if (mode == "witness_inside_naive")
    {
        const Vec3 c = vecBits(1092615999u, 0, 0), p = vecBits(3107979264u, 0, 0);
        const float r2 = fromBits(1120403454u);
        volatile float d = c.x - p.x; volatile float naive = d * d;
        require(fixtureInsideControlPointRadius(c, p, r2) && !(naive < r2), "wide square separates from a float square");
    }
    else unexpected("unknown mode");
    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
    return 0;
}
