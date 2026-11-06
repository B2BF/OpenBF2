// [TEMP] Queries are exact production extracts. Only constructors/destructor below
// are chosen: containers have native lifetimes, unrelated player/formation work is omitted.
// These non-polymorphic query classes have no generated/mock vtables or virtual calls.
#include "squad_queries_runtime.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;

static_assert(sizeof(void*) == 4, "MSVC x86 fixture only");
static_assert(sizeof(Vec3) == 12, "three float coordinates");
static_assert(std::is_same<decltype(SquadMember::m_squad), SquadCoordinator*>::value, "squad pointer type");
static_assert(std::is_same<decltype(SquadCoordinator::m_squadLeaderIndex), int>::value, "signed leader index");
static_assert(std::is_same<decltype(SquadCoordinator::m_squadMembers), std::vector<SquadMember*>>::value, "native pointer vector");
static_assert(std::is_same<decltype(SquadCoordinator::m_v108), Vec3>::value, "position type");
static_assert(std::is_same<decltype(&SquadMember::getSquad), SquadCoordinator*(SquadMember::*)()>::value, "non-const pointer query");
static_assert(std::is_same<decltype(&SquadCoordinator::getSquadPosition), void(SquadCoordinator::*)(Vec3&)>::value, "non-const output query");
static_assert(std::is_same<decltype(&SquadCoordinator::getSquadLeader), SquadMember*(SquadCoordinator::*)()>::value, "non-const leader query");

namespace dice { namespace hfe { namespace ai {
SquadCoordinator::SquadCoordinator(int side, int squadIndex)
    : m_squadIndex(squadIndex), m_side(side), m_squadLeaderIndex(-1), m_formation(nullptr), m_v108{0,0,0} {}
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player, int squadIndex, int side, int memberIndex)
    : m_squadIndex(squadIndex), m_memberIndex(memberIndex), m_isSquadLeader(false),
      m_playerId(-1), m_player(player), m_bot(nullptr), m_squad(squad), m_side(side), m_request(nullptr) {}
SquadMember::~SquadMember() {}
} } }

[[noreturn]] static void fail(const char* message)
{
    std::fprintf(stderr, "FAIL %s\n", message); std::fflush(stderr); std::abort();
}
static void require(bool condition, const char* message) { if (!condition) fail(message); }
static float fromBits(unsigned bits) { float result; std::memcpy(&result, &bits, 4); return result; }
static bool same(const Vec3& a, const Vec3& b) { return std::memcmp(&a, &b, sizeof(Vec3)) == 0; }

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    require(argc == 2, "mode");
    const std::string mode(argv[1]);
    if (mode == "layout")
    {
        std::printf("{\"pointer_bytes\":%zu,\"vec3_bytes\":%zu,\"member_bytes\":%zu,\"member_squad_offset\":%zu,\"coordinator_bytes\":%zu,\"leader_index_offset\":%zu,\"position_offset\":%zu,\"vector_bytes\":%zu,\"list_bytes\":%zu}\n",
            sizeof(void*), sizeof(Vec3), sizeof(SquadMember), offsetof(SquadMember,m_squad),
            sizeof(SquadCoordinator), offsetof(SquadCoordinator,m_squadLeaderIndex), offsetof(SquadCoordinator,m_v108),
            sizeof(std::vector<SquadMember*>), sizeof(std::list<int>));
        return 0;
    }
    SquadCoordinator first(0, 3), second(1, 4);
    SquadMember member(&first, nullptr, 3, 0, 0), another(&second, nullptr, 4, 1, 0);
    if (mode == "squad_pointer")
    {
        require(member.getSquad() == &first, "stored coordinator identity");
        member.m_squad = &second;
        require(member.getSquad() == &second, "fresh stored pointer");
        member.m_squad = nullptr;
        require(member.getSquad() == nullptr, "stored null returned unchanged");
        require(another.getSquad() == &second, "other member unaffected");
    }
    else if (mode == "position")
    {
        struct Output { unsigned before; Vec3 value; unsigned after; } output = {0x12345678u, {}, 0x87654321u};
        first.m_v28 = {21,22,23}; first.m_v64 = {31,32,33}; first.m_v176 = {41,42,43};
        first.m_v108 = {fromBits(0x80000000u), fromBits(0x7FC12345u), fromBits(0x7F800000u)};
        const Vec3 original = first.m_v108;
        first.getSquadPosition(output.value);
        require(same(output.value, original) && same(first.m_v108, original), "copies signed zero, quiet NaN payload and infinity without changing source");
        require(output.before == 0x12345678u && output.after == 0x87654321u, "only output Vec3 written");
        first.m_v108 = {-7, 11, 13};
        require(same(output.value, original), "output is a value copy");
        first.getSquadPosition(output.value);
        require(same(output.value, first.m_v108), "fresh position read");
        first.getSquadPosition(first.m_v108);
        require(same(output.value, first.m_v108), "self alias preserves values");
        second.getSquadPosition(output.value);
        const Vec3 zero = {};
        require(same(output.value, zero), "zero position copied without a validity gate");
        require(first.m_v28.x == 21 && first.m_v64.y == 32 && first.m_v176.z == 43, "neighbor positions untouched");
    }
    else if (mode == "leader")
    {
        first.m_squadMembers = {&member, nullptr, &another};
        first.m_squadLeaderIndex = 0;
        require(first.getSquadLeader() == &member, "first leader index");
        first.m_squadLeaderIndex = 2;
        require(first.getSquadLeader() == &another, "last leader index");
        first.m_squadMembers[2] = &member;
        require(first.getSquadLeader() == &member, "fresh indexed entry");
        first.m_squadMembers.reserve(first.m_squadMembers.capacity() + 17);
        first.m_squadMembers[2] = &another;
        require(first.getSquadLeader() == &another, "native vector relocation uses current storage");
        first.m_squadLeaderIndex = 1;
        require(first.getSquadLeader() == nullptr, "in-bounds null entry returned unchanged");
        require(first.m_squadMembers.size() == 3 && first.m_squadLeaderIndex == 1, "no roster or leader mutation");
    }
    else if (mode == "chain")
    {
        first.m_squadMembers = {&another, &member}; first.m_squadLeaderIndex = 0;
        second.m_v108 = {4,5,6};
        Vec3 output = {};
        member.getSquad()->getSquadLeader()->getSquad()->getSquadPosition(output);
        require(same(output, second.m_v108), "actual typed getter chain");
    }
    else fail("unknown mode");
    std::printf("PASS %s\n", mode.c_str());
    return 0;
}
