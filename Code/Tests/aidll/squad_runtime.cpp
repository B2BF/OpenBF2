// [TEMP] [CHOSEN, not recovered] Offline admission fixture. The whole production
// SquadMember.cpp and SquadFormation.cpp are linked unchanged, without /FORCE.
// Player/bot/game/timer and coordinator collaborators model only the stated cases.
// Every unused virtual aborts. No server, live vehicle, AI tick or engine squad runs.
#include "../../BF2/AI/AIDLL/AI/AIBot/SquadMember.h"
#include "../../BF2/AI/AIDLL/AI/AIBot/SquadFormation.h"
#include "../../BF2/AI/AIDLL/AI/AIBot/SquadCoordinator.h"
#include "../../BF2/AI/AIDLL/AI/AIBot/BotMain.h"
#include "../../BF2/AI/AIDLL/AI/Main/IBotManager.h"
#include "../../BF2/AI/AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/AIPlayer.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/BFGame.h"
#include "../../BF2/AI/AIInterface/BFKnowledge/BFTimer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <cmath>
#include <limits>
#include <new>
#include <string>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "This fixture requires MSVC x86");
static_assert(sizeof(SquadMember) == 112, "PE SquadMember allocation size");
static_assert(offsetof(SquadMember, m_time24) == 40, "PE double alignment");
static_assert(offsetof(SquadMember, m_position) == 64, "PE position offset");

static std::string calls;
static void trace(const char* name) { if (!calls.empty()) calls += ','; calls += name; }
__declspec(noreturn) static void unexpected(const char* name)
{
    std::fprintf(stderr, "UNEXPECTED collaborator: %s\n", name);
    std::fflush(stderr);
    std::abort();
}
static void require(bool condition, const char* what)
{
    if (!condition) { std::fprintf(stderr, "FAIL %s; calls=%s\n", what, calls.c_str()); std::exit(1); }
}
static void expectCalls(const char* expected) { require(calls == expected, expected); calls.clear(); }
static bool same(const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static SquadMember* observedMember;
static SquadFormation* observedFormation;
static bool observeConstruction;
static bool coordinatorHasPosition;
static unsigned char constructorSentinel;

static void observeBinding(SquadMember* member)
{
    if (!observeConstruction) return;
    require(member->m_playerId == 123 && member->m_squadIndex == 7 && member->m_memberIndex == 2,
            "IDs visible during binding");
    require(member->m_side == 1 && member->m_maxSpeed == 0 && member->m_estimatedArrival == 1,
            "side/speed visible during binding");
    require(same(member->m_position, Vec3{}) && same(member->m_vector54, Vec3{}) && same(member->m_vector60, Vec3{}),
            "all vectors initialized before binding");
    // Byte reads avoid interpreting an intentionally uninitialized bool representation.
    const unsigned char* raw = reinterpret_cast<const unsigned char*>(member);
    require(raw[52] == constructorSentinel && raw[53] == constructorSentinel && raw[76] == constructorSentinel,
            "later flags remain untouched during binding");
}
static void observeVehicleRead()
{
    if (observedMember)
        require(observedMember->m_positionValid && observedMember->m_moved,
                "vehicle getter precedes flag clearing");
}
static void observeTimerRead()
{
    if (observedFormation)
        require(!observedFormation->m_b56, "movement flag cleared before clock read");
}

#include "squad_runtime_mocks.inl"

namespace dice { namespace hfe { namespace ai {
// Only collaborator definitions live here. None shadows a member/formation method.
IAIPlayer::~IAIPlayer() {}
Bot::~Bot() {}
int Bot::getBotType() const { unexpected("Bot::getBotType"); }
BAPContext* Bot::getScriptContext() { unexpected("Bot::getScriptContext"); }
IBotManager* IBotManager::instance = nullptr;
IBotManager::IBotManager() {}
IBotManager::~IBotManager() {}
IAIGame::IAIGame() {}
IAIGame::~IAIGame() {}
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}
void* CommonSingletonServer::m_iaiGame = nullptr;
void* CommonSingletonServer::m_iBFTimer = nullptr;
SquadCoordinator::SquadCoordinator(int side, int index) : m_side(side), m_squadIndex(index), m_v108{} {}
int SquadCoordinator::getSide() { trace("side"); return m_side; }
SquadMember* SquadCoordinator::getSquadMember(int index)
{
    trace("member");
    require(index >= 0 && static_cast<unsigned>(index) < m_squadMembers.size(), "fixture member index");
    return m_squadMembers[index];
}
bool SquadCoordinator::squadHasPosition() { trace("position"); return coordinatorHasPosition; }
} } }

// The destructor's request omission is observed, not silently cleaned up by a fake destructor.
// The fixture itself frees that allocation afterwards so repeated cases do not leak.
static void* watchedRequest;
static bool requestDeleted;
void* operator new(std::size_t size)
{
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept
{
    if (p && p == watchedRequest) requestDeleted = true;
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }

static int constructorCase(bool botCase)
{
    TestPlayer player;
    TestBot bot;
    TestManager manager;
    manager.result = botCase ? &bot : nullptr;
    IBotManager::instance = &manager;
    SquadCoordinator coordinator(1, 7);
    alignas(SquadMember) unsigned char storage[112];
    unsigned char expected[112];
    constructorSentinel = botCase ? 0xA5 : 0x5A;
    std::memset(storage, constructorSentinel, sizeof(storage));
    std::memset(expected, constructorSentinel, sizeof(expected));
    observeConstruction = botCase;
    SquadMember* member = new (storage) SquadMember(&coordinator, &player, 7, 1, 2);
    observeConstruction = false;
    expectCalls(botCase ? "id,lookup,bind" : "id,lookup");

    // Independent byte oracle from the PE constructor stores; padding must stay untouched.
    const int squad = 7, index = 2, id = 123, side = 1;
    const float one = 1.0f, nineHundred = 900.0f;
    IAIPlayer* playerPointer = &player;
    Bot* botPointer = botCase ? &bot : nullptr;
    SquadCoordinator* coordinatorPointer = &coordinator;
    SquadMember::RequestState* request = member->m_request;
    std::memcpy(expected, &squad, 4); std::memcpy(expected + 4, &index, 4);
    expected[8] = 0; std::memcpy(expected + 12, &id, 4);
    std::memcpy(expected + 16, &playerPointer, 4); std::memcpy(expected + 20, &botPointer, 4);
    std::memcpy(expected + 24, &coordinatorPointer, 4); std::memcpy(expected + 28, &side, 4);
    std::memcpy(expected + 32, &one, 4); std::memset(expected + 40, 0, 14);
    std::memcpy(expected + 56, &request, 4); std::memset(expected + 60, 0, 20);
    std::memcpy(expected + 80, &nineHundred, 4); expected[84] = 0;
    std::memset(expected + 88, 0, 24);
    require(std::memcmp(storage, expected, sizeof(storage)) == 0, "all 112 constructor bytes including padding");
    require(request && request->m_action == 0 && request->m_handle == 0, "request initialization");
    require(member->getPlayer() == &player && member->getPlayerId() == 123 && member->getMemberIndex() == 2,
            "member accessors");
    require(member->isBot() == botCase && !member->isSquadLeader(), "bot/leader predicates");
    member->setIsSquadLeader(true); require(member->isSquadLeader(), "leader set true");
    member->setIsSquadLeader(false); require(!member->isSquadLeader(), "leader set false");
    member->setHavePath(true); require(member->m_havePath && !member->m_flag4A, "path true pair");
    member->setHavePath(false); require(!member->m_havePath && member->m_flag4A, "path false pair");
    watchedRequest = request;
    member->~SquadMember();
    IAIPlayer* clearedPlayer;
    Bot* clearedBot;
    std::memcpy(&clearedPlayer, storage + 16, 4); std::memcpy(&clearedBot, storage + 20, 4);
    require(!clearedPlayer && !clearedBot && !requestDeleted, "destructor clears only borrowed pointers");
    require(!botCase || bot.member == member, "retail leaves bot back-pointer");
    watchedRequest = nullptr;
    delete request;
    return 0;
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    if (argc != 2) return 2;
    const std::string mode = argv[1];
    if (mode == "constructor_bot" || mode == "constructor_human")
    {
        constructorCase(mode == "constructor_bot");
        std::printf("PASS %s\n", argv[1]);
        return 0;
    }

    TestPlayer player;
    TestManager manager;
    TestGame game;
    TestTimer timer;
    IBotManager::instance = &manager;
    CommonSingletonServer::m_iaiGame = &game;
    CommonSingletonServer::m_iBFTimer = &timer;
    SquadCoordinator coordinator(1, 7);
    SquadMember member(&coordinator, &player, 7, 1, 1);
    coordinator.m_squadMembers.resize(3, &member);
    calls.clear();
    if (mode == "dead") { require(!member.isAllowedInFormation(), "dead refusal"); expectCalls("alive"); }
    else if (mode == "man_down")
    {
        player.alive = true; player.down = true;
        require(!member.isAllowedInFormation(), "man-down refusal"); expectCalls("alive,down");
    }
    else if (mode == "no_vehicle" || mode == "allowed_frontier")
    {
        player.alive = true;
        if (mode == "allowed_frontier") player.vehicle = reinterpret_cast<IAIObject*>(0x1234);
        require(!member.isAllowedInFormation(), "null vehicle refusal");
        require(mode != "allowed_frontier", "UNEXPECTED_RETURN"); expectCalls("alive,down,vehicle");
    }
    else if (mode == "position_null" || mode == "position_frontier")
    {
        member.m_position = {1, 2, 3}; member.m_positionValid = true; member.m_moved = true;
        observedMember = &member;
        if (mode == "position_frontier") player.vehicle = reinterpret_cast<IAIObject*>(0x1234);
        member.updateMemberPos(); observedMember = nullptr;
        require(mode != "position_frontier", "UNEXPECTED_RETURN"); expectCalls("vehicle");
        require(!member.m_positionValid && !member.m_moved && same(member.m_position, Vec3{1, 2, 3}),
                "null vehicle preserves coordinates and clears flags");
    }
    else
    {
        SquadFormation formation(&coordinator);
        expectCalls("side,size");
        require(formation.m_slots.size() == 3 && formation.m_trailingPositions.size() == 3,
                "three parallel formation slots");
        require(formation.m_side == 1 && formation.getFormationMaxSpeed() == 5.0f,
                "formation side and empty speed");
        observedFormation = &formation;
        if (mode == "formation_constructor")
        {
            for (int i = 0; i < 3; ++i)
                require(!formation.m_slots[i]->hasSquadMemberAssigned && !formation.m_trailingPositions[i].first,
                        "formation starts empty");
            require(same(formation.m_direction, Vec3{1, 0, 0}) &&
                    same(formation.m_slots[0]->m_offset, Vec3{0, 0, 0}) &&
                    same(formation.m_slots[1]->m_offset, Vec3{1, 0, 0}) &&
                    same(formation.m_slots[2]->m_offset, Vec3{-1, 0, 0}), "line construction offsets");
        }
        else if (mode.compare(0, 5, "join_") == 0)
        {
            formation.m_slots[0]->hasSquadMemberAssigned = true;
            formation.m_slots[0]->m_memberIndex = mode == "join_duplicate" ? 1 : 0;
            formation.m_v72 = {4, 5, 6}; formation.m_v60 = {7, 8, 9};
            formation.m_b56 = true;
            member.setHavePath(true);
            SquadFormation::Slot* slot = formation.m_slots[1];
            slot->m_i20 = 88; slot->m_b24 = true; slot->m_i28 = 99;
            slot->m_offset = {2, 4, 6}; slot->m_referenceSlot = 77;
            if (mode == "join_full")
                for (auto* s : formation.m_slots) { s->hasSquadMemberAssigned = true; s->m_memberIndex = 9; }
            formation.joinFormation(1);
            expectCalls("time,member");
            require(formation.m_b56 && std::fabs(formation.m_d92 - 10.2) < 1e-12 &&
                    same(formation.m_v60, formation.m_v72), "forced movement after admission");
            require(!member.m_havePath && member.m_flag4A, "admission resets path even when full");
            if (mode == "join_full")
            {
                require(slot->m_memberIndex == 9 && slot->m_i20 == 88 && slot->m_b24 && slot->m_i28 == 99,
                        "full formation preserves slot state");
            }
            else
            {
                require(slot->hasSquadMemberAssigned && slot->m_memberIndex == 1 && slot->m_i20 == -1 &&
                        !slot->m_b24 && slot->m_i28 == 0 && !formation.m_slots[2]->hasSquadMemberAssigned,
                        "first free slot admission");
                require(same(slot->m_offset, Vec3{2, 4, 6}) && slot->m_referenceSlot == 77,
                        "admission preserves slot geometry");
                if (mode == "join_duplicate") require(formation.m_slots[0]->m_memberIndex == 1,
                                                       "duplicate member remains in earlier slot");
            }
        }
        else if (mode.compare(0, 9, "movement_") == 0)
        {
            formation.m_b56 = true; formation.m_d92 = 9.0;
            formation.m_v60 = {0, 0, 0}; formation.m_v72 = {3, 0, 0};
            formation.m_slots[0]->hasSquadMemberAssigned = mode != "movement_empty";
            if (mode == "movement_distance_boundary") formation.m_v72.x = 2;
            if (mode == "movement_deadline_boundary") timer.now = 9;
            if (mode == "movement_nan_distance") formation.m_v72.x = std::numeric_limits<float>::quiet_NaN();
            if (mode == "movement_nan_time" || mode == "movement_forced") timer.now = std::numeric_limits<double>::quiet_NaN();
            const bool force = mode == "movement_forced";
            formation.updateMovementFlag(force);
            if (mode == "movement_empty")
            {
                expectCalls(""); require(formation.m_b56 && formation.m_d92 == 9 && formation.m_v60.x == 0,
                                         "empty slot zero leaves movement state untouched");
            }
            else
            {
                expectCalls("time");
                const bool success = mode == "movement_success" || force;
                require(formation.m_b56 == success, "movement thresholds");
                if (success)
                    require(same(formation.m_v60, formation.m_v72) &&
                            (force ? std::isnan(formation.m_d92) : std::fabs(formation.m_d92 - 10.2) < 1e-12),
                            "movement stores deadline and anchor");
                else require(formation.m_d92 == 9 && same(formation.m_v60, Vec3{}), "refused movement preserves deadline/anchor");
            }
        }
        else if (mode.compare(0, 6, "reset_") == 0)
        {
            formation.m_slots[0]->hasSquadMemberAssigned = mode != "reset_empty";
            coordinatorHasPosition = mode == "reset_frontier";
            formation.m_v72 = {11, 12, 13};
            for (unsigned i = 0; i != 3; ++i)
            {
                formation.m_trailingPositions[i] = std::make_pair(true, Vec3{4, 5, 6});
                formation.m_slots[i]->m_i20 = 42;
                formation.m_slots[i]->m_memberIndex = 20 + i;
                formation.m_slots[i]->m_b24 = true;
            }
            formation.resetTrailingPositions();
            require(mode != "reset_frontier", "UNEXPECTED_RETURN");
            expectCalls(mode == "reset_empty" ? "" : "position");
            require(same(formation.m_trailingPosition, Vec3{11, 12, 13}), "trailing anchor copy");
            for (unsigned i = 0; i != 3; ++i)
                require(!formation.m_trailingPositions[i].first && same(formation.m_trailingPositions[i].second, Vec3{4, 5, 6}) &&
                        formation.m_slots[i]->m_i20 == -1 && formation.m_slots[i]->m_memberIndex == 20 + i &&
                        formation.m_slots[i]->m_b24, "trailing reset clears only validity and slot target index");
            require(formation.m_slots[0]->hasSquadMemberAssigned == (mode != "reset_empty"), "reset preserves occupancy");
        }
        else return 2;
        observedFormation = nullptr;
        for (auto* slot : formation.m_slots) delete slot; // Fixture cleanup; retail formation teardown is not tested.
    }
    delete member.m_request; // Fixture cleanup for the deliberately preserved retail omission.
    std::printf("PASS %s\n", argv[1]);
    return 0;
}
