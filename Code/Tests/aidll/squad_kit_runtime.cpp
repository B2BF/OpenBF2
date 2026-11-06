// [TEMP] Exact production query bodies are in a separate generated TU. Constructors and
// destructors below are chosen native lifetimes, not retail initialization tests. The
// getSquadMember collaborator models lookup callbacks; the presence query uses the actual
// SquadMember::getPlayer extract and a typed IAIPlayer observer. No byte-object fabrication.
#include "squad_kit_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;

static_assert(sizeof(void*) == 4, "MSVC x86 fixture only");
static_assert(std::is_same<decltype(&SquadManager::getSquadFromPlayer),
    SquadCoordinator*(SquadManager::*)(int, int)>::value, "signed inputs, nonconst pointer lookup");
static_assert(std::is_same<decltype(&SquadCoordinator::hasMemberWithKit),
    bool(SquadCoordinator::*)(KitType)>::value, "typed enum input, nonconst bool query");
static_assert(std::is_same<decltype(&IAIPlayer::getSpawnKit),
    int(IAIPlayer::*)() const>::value, "full signed integer kit result");

static unsigned checks = 0;
[[noreturn]] static void unexpected(const char* message)
{
    std::fprintf(stderr, "FAIL %s\n", message); std::fflush(stderr); std::abort();
}
static void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) unexpected(message);
}

static std::vector<SquadCoordinator*>* sideVectors = nullptr;
static std::vector<int> lookupTrace, lookupIds, kitTrace;
static std::function<SquadMember*(SquadCoordinator*, int, int&)> lookup;
static std::function<int(int, int)> kitQuery;
static int observeKit(int identity, int value)
{
    kitTrace.push_back(identity);
    return kitQuery ? kitQuery(identity, value) : value;
}
#include "squad_kit_mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Fixture-only lifetimes and collaborator. Each native STL
// member constructs/destructs normally; unrelated engine/formation work is omitted.
SquadManager::SquadManager() { sideVectors = m_Squads; }
SquadCoordinator::SquadCoordinator(int side, int squadIndex)
    : m_squadIndex(squadIndex), m_side(side), m_squadLeaderIndex(-1), m_formation(nullptr) {}
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player,
                        int squadIndex, int side, int memberIndex)
    : m_squadIndex(squadIndex), m_memberIndex(memberIndex), m_isSquadLeader(false),
      m_playerId(-1), m_player(player), m_bot(nullptr), m_squad(squad), m_side(side), m_request(nullptr) {}
SquadMember::~SquadMember() {}
IAIPlayer::~IAIPlayer() {}
SquadMember* SquadCoordinator::getSquadMember(int id, int& index)
{
    lookupTrace.push_back(m_squadIndex);
    lookupIds.push_back(id);
    if (!lookup) unexpected("unexpected lookup collaborator");
    return lookup(this, id, index);
}
} } }

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2) unexpected("mode required");
    const std::string mode(argv[1]);
    SquadManager manager;
    SquadCoordinator a(0, 10), b(0, 20), c(1, 30), replacement(0, 40);
    ObservedPlayer p1(1, 1), p2(2, 5), p3(3, 5);
    SquadMember m1(&a, &p1, 0, 0, 0), m2(&a, &p2, 0, 0, 1), m3(&a, &p3, 0, 0, 2);
    const KitType medic = static_cast<KitType>(5);

    if (mode == "manager_empty")
    {
        require(manager.getSquadFromPlayer(-1, 1) == nullptr, "side one empty returns null");
        require(manager.getSquadFromPlayer(0, 2) == nullptr, "side two empty returns null");
        require(lookupTrace.empty(), "empty vectors never query");
    }
    else if (mode == "manager_order")
    {
        sideVectors[0] = {&a, &b, &replacement}; sideVectors[1] = {&c};
        lookup = [&](SquadCoordinator* q, int id, int& index) {
            require(id == -7654321, "negative player id forwarded unchanged");
            index = -999;
            return q == &a ? nullptr : &m1;
        };
        require(manager.getSquadFromPlayer(-7654321, 1) == &b, "first matching coordinator wins");
        require(lookupTrace == std::vector<int>({10, 20}), "only prefix queried in order");
        lookupTrace.clear();
        require(manager.getSquadFromPlayer(-7654321, 2) == &c, "one-based side two selected");
        require(lookupTrace == std::vector<int>({30}), "other side queried alone");
    }
    else if (mode == "manager_miss")
    {
        sideVectors[0] = {&a, &b, &replacement}; sideVectors[1] = {&c};
        lookup = [](SquadCoordinator*, int, int& index) { index = 123; return static_cast<SquadMember*>(nullptr); };
        require(manager.getSquadFromPlayer((std::numeric_limits<int>::min)(), 1) == nullptr, "exhaustion returns null");
        require(lookupTrace == std::vector<int>({10, 20, 40}), "every current coordinator visited");
        require(lookupIds == std::vector<int>(3, (std::numeric_limits<int>::min)()), "full signed minimum forwarded each time");
    }
    else if (mode == "manager_fresh")
    {
        sideVectors[0] = {&a};
        lookup = [&](SquadCoordinator* q, int, int& index) {
            require(q == &a, "original query receiver"); index = 0;
            const auto prior = sideVectors[0].data();
            sideVectors[0].reserve(sideVectors[0].capacity() + 17);
            require(sideVectors[0].data() != prior, "native vector storage relocated");
            sideVectors[0][0] = &replacement; return &m1;
        };
        require(manager.getSquadFromPlayer(23, 1) == &replacement, "success reloads vector and entry after callback");
        require(lookupTrace == std::vector<int>({10}), "replacement is returned without querying it");
    }
    else if (mode == "manager_append")
    {
        sideVectors[0] = {&a};
        lookup = [&](SquadCoordinator* q, int, int& index) {
            index = 88;
            if (q == &a) { sideVectors[0].push_back(&b); return static_cast<SquadMember*>(nullptr); }
            return &m2;
        };
        require(manager.getSquadFromPlayer(9, 1) == &b, "live appended coordinator searched");
        require(lookupTrace == std::vector<int>({10, 20}), "append callback trace");
    }
    else if (mode == "manager_shrink")
    {
        sideVectors[0] = {&a, &b};
        lookup = [&](SquadCoordinator*, int, int&) {
            sideVectors[0].clear(); return static_cast<SquadMember*>(nullptr);
        };
        require(manager.getSquadFromPlayer(9, 1) == nullptr, "live shrink ends search");
        require(lookupTrace == std::vector<int>({10}), "removed coordinator not queried");
    }
    else if (mode == "kit_empty_null")
    {
        require(!a.hasMemberWithKit(medic), "empty member vector false");
        a.m_squadMembers = {nullptr, nullptr};
        require(!a.hasMemberWithKit(medic), "null member entries skipped");
        require(kitTrace.empty(), "null members never query player");
    }
    else if (mode == "kit_order")
    {
        a.m_squadMembers = {nullptr, &m1, nullptr, &m2, &m3};
        require(a.hasMemberWithKit(medic), "first matching kit found");
        require(kitTrace == std::vector<int>({1, 2}), "null skip and first match short circuit");
        kitTrace.clear(); p2.kit = 6; p3.kit = 7;
        require(!a.hasMemberWithKit(medic), "all nonmatching players return false");
        require(kitTrace == std::vector<int>({1, 2, 3}), "full miss visits each nonnull member once");
    }
    else if (mode == "kit_integer")
    {
        a.m_squadMembers = {&m1, &m2}; p1.kit = 261; p2.kit = -251;
        require(!a.hasMemberWithKit(medic), "not an eight-bit kit comparison");
        require(kitTrace == std::vector<int>({1, 2}), "integer nonmatches both queried");
        kitTrace.clear(); p1.kit = (std::numeric_limits<int>::min)();
        require(a.hasMemberWithKit(static_cast<KitType>((std::numeric_limits<int>::min)())), "negative full enum matches");
        require(kitTrace == std::vector<int>({1}), "signed exact match short circuits");
    }
    else if (mode == "kit_replace")
    {
        a.m_squadMembers = {&m1, &m3};
        kitQuery = [&](int identity, int kit) {
            if (identity == 1) {
                a.m_squadMembers.reserve(a.m_squadMembers.capacity() + 17);
                a.m_squadMembers[1] = &m2;
            }
            return kit;
        };
        require(a.hasMemberWithKit(medic), "replacement member found after relocation");
        require(kitTrace == std::vector<int>({1, 2}), "fresh next member not old entry");
    }
    else if (mode == "kit_append")
    {
        a.m_squadMembers = {&m1};
        kitQuery = [&](int identity, int kit) { if (identity == 1) a.m_squadMembers.push_back(&m2); return kit; };
        require(a.hasMemberWithKit(medic), "live appended member searched");
        require(kitTrace == std::vector<int>({1, 2}), "appended player is queried");
    }
    else if (mode == "kit_shrink")
    {
        a.m_squadMembers = {&m1, &m2};
        kitQuery = [&](int, int kit) { a.m_squadMembers.clear(); return kit; };
        require(!a.hasMemberWithKit(medic), "nonmatch callback shrink ends scan");
        require(kitTrace == std::vector<int>({1}), "removed member never queried");
    }
    else if (mode == "kit_match_mutation")
    {
        a.m_squadMembers = {&m2, &m3};
        kitQuery = [&](int, int kit) { a.m_squadMembers.clear(); return kit; };
        require(a.hasMemberWithKit(medic), "matching callback result survives roster removal");
        require(kitTrace == std::vector<int>({2}), "match does not reread or continue roster");
    }
    else unexpected("unknown mode");
    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
    return 0;
}
