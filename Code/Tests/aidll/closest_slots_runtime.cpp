// [TEMP] Exact extracted production bodies are linked separately. CLOSEST_OBSERVER replaces
// only the selector, deliberately, so the actual outer loop and actual swap can be observed.
#include "closest_slots_runtime.h"

using dice::hfe::ai::SquadFormation;
using Slot = SquadFormation::Slot;
static int checks;
static void check(bool value, const char* message)
{
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(90); }
}

static bool watchAllocation;
static int allocations, releases;
static void* temporary;
static void (*onAllocation)();
void* operator new(std::size_t size)
{
    void* const result = std::malloc(size);
    if (!result) throw std::bad_alloc();
    if (watchAllocation && size == sizeof(Slot))
    {
        watchAllocation = false;
        temporary = result;
        ++allocations;
        if (onAllocation) onAllocation();
    }
    return result;
}
void operator delete(void* pointer) noexcept
{
    if (pointer && pointer == temporary) { ++releases; temporary = nullptr; }
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Native lifetime only. Actual constructor is not under test.
SquadFormation::SquadFormation(SquadCoordinator* coordinator)
    : m_coordinator(coordinator), m_formation(0), m_direction{}, m_numberOfSlots(0),
      m_b36(false), m_formationMaxSpeed(0), m_side(0), m_i48(0), m_f52(0), m_b56(false),
      m_v60{}, m_v72{}, m_b84(false), m_b85(false), m_i88(0), m_d92(0),
      m_trailingPosition{}
{}
} } }

static void fill(Slot& s, int n)
{
    s.hasSquadMemberAssigned = (n & 1) != 0;
    s.m_memberIndex = -1000 - n;
    s.m_offset = {float(n), float(n + 10), float(n + 20)};
    s.m_i20 = n + 30;
    s.m_b24 = (n & 1) == 0;
    s.m_i28 = n * -17.25f;
    s.m_referenceSlot = n + 40;
}
static void sameUnswapped(const Slot& a, const Slot& b)
{
    check(a.hasSquadMemberAssigned == b.hasSquadMemberAssigned, "occupied retained");
    check(a.m_offset.x == b.m_offset.x && a.m_offset.y == b.m_offset.y &&
          a.m_offset.z == b.m_offset.z, "offset retained");
    check(a.m_i20 == b.m_i20, "i20 retained");
    check(a.m_referenceSlot == b.m_referenceSlot, "reference slot retained");
}
static void samePayload(const Slot& a, const Slot& b)
{
    check(a.m_memberIndex == b.m_memberIndex, "member index exchanged");
    check(a.m_b24 == b.m_b24, "b24 exchanged");
    check(a.m_i28 == b.m_i28, "i28 exchanged");
}

static SquadFormation* callbackFormation;
static Slot* replacementOne;
static Slot* replacementTwo;
static void replaceSlotsDuringAllocation()
{
    callbackFormation->m_slots[1] = replacementOne;
    callbackFormation->m_slots[2] = replacementTwo;
}

#ifdef CLOSEST_OBSERVER
static const char* scenario;
static int observed[12], observedMember[12], count;
static Slot* callbackNewSlot;
namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Only the separate observer executable uses this receiver.
int SquadFormation::getClosestSlotIndex(int index)
{
    check(count < 12, "bounded selector calls");
    observed[count] = index;
    observedMember[count++] = m_slots[index]->m_memberIndex;
    if (!std::strcmp(scenario, "refuse")) return -1;
    if (!std::strcmp(scenario, "same")) return index;
    if (!std::strcmp(scenario, "zero")) return 0;
    if (!std::strcmp(scenario, "cross")) return index == 1 ? 2 : -1;
    if (!std::strcmp(scenario, "shrink")) { m_numberOfSlots = 2; return -1; }
    if (!std::strcmp(scenario, "grow"))
    {
        if (index == 1) { m_slots.push_back(callbackNewSlot); m_numberOfSlots = 3; }
        return -1;
    }
    if (!std::strcmp(scenario, "replace"))
    {
        if (index == 1) m_slots[2] = callbackNewSlot;
        return -1;
    }
    if (!std::strcmp(scenario, "clear_next"))
    {
        if (index == 1) m_slots[2]->hasSquadMemberAssigned = false;
        return -1;
    }
    if (!std::strcmp(scenario, "replace_current"))
    {
        m_slots[1] = callbackNewSlot;
        m_numberOfSlots = 2;
        return 2;
    }
    std::abort();
}
} } }
#endif

int main(int argc, char** argv)
{
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    if (argc != 2) return 2;
    const char* const mode = argv[1];
    SquadFormation f(nullptr);
    Slot a, b, c, d, e;
    fill(a, 1); fill(b, 2); fill(c, 3); fill(d, 4); fill(e, 5);
    f.m_slots = {&a, &b, &c};
    f.m_numberOfSlots = 3;
    const Slot savedA = a, savedB = b, savedC = c, savedD = d, savedE = e;

    if (!std::strcmp(mode, "swap") || !std::strcmp(mode, "swap_same") ||
        !std::strcmp(mode, "swap_allocation_mutation"))
    {
        callbackFormation = &f; replacementOne = &d; replacementTwo = &e;
        if (!std::strcmp(mode, "swap_allocation_mutation"))
            onAllocation = replaceSlotsDuringAllocation;
        watchAllocation = true;
        const int second = !std::strcmp(mode, "swap_same") ? 1 : 2;
        f.swapSlots(1, second);
        check(allocations == 1 && releases == 1 && !temporary,
              "one temporary allocation and release even same slot");
        sameUnswapped(b, savedB); sameUnswapped(c, savedC);
        samePayload(b, second == 1 ? savedB : savedC);
        samePayload(c, second == 1 ? savedC : savedB);
        samePayload(a, savedA); sameUnswapped(a, savedA);
        if (onAllocation)
        {
            check(f.m_slots[1] == &d && f.m_slots[2] == &e, "allocator changes retained in vector");
            samePayload(d, savedD); sameUnswapped(d, savedD);
            samePayload(e, savedE); sameUnswapped(e, savedE);
        }
        else check(f.m_slots[0] == &a && f.m_slots[1] == &b && f.m_slots[2] == &c,
                   "slot pointers retained");
    }
#ifndef CLOSEST_OBSERVER
    else if (!std::strcmp(mode, "max_zero") || !std::strcmp(mode, "max_one") ||
             !std::strcmp(mode, "all_free"))
    {
        if (!std::strcmp(mode, "max_zero")) { f.m_slots.clear(); f.m_numberOfSlots = 0; }
        else if (!std::strcmp(mode, "max_one")) { f.m_slots.resize(1); f.m_numberOfSlots = 1; }
        else { b.hasSquadMemberAssigned = false; c.hasSquadMemberAssigned = false; }
        f.updateClosestSlots();
        samePayload(a, savedA); sameUnswapped(a, savedA);
        check(allocations == 0, "no swap allocation");
    }
    else if (!std::strcmp(mode, "selector_frontier"))
    {
        b.m_memberIndex = -2147483647;
        f.getClosestSlotIndex(1);
        return 91;
    }
    else if (!std::strcmp(mode, "outer_frontier"))
    {
        b.hasSquadMemberAssigned = true;
        f.updateClosestSlots();
        return 92;
    }
    else return 4;
#else
    else
    {
        scenario = mode;
        b.hasSquadMemberAssigned = c.hasSquadMemberAssigned = d.hasSquadMemberAssigned = true;
        callbackNewSlot = &d;
        if (!std::strcmp(mode, "grow")) { f.m_slots.resize(2); f.m_slots.shrink_to_fit(); f.m_numberOfSlots = 2; }
        if (!std::strcmp(mode, "zero") || !std::strcmp(mode, "replace_current")) f.m_numberOfSlots = 2;
        f.updateClosestSlots();
        check(count >= 1 && observed[0] == 1, "slot zero skipped");
        check(observedMember[0] == savedB.m_memberIndex, "first input slot member");
        if (!std::strcmp(mode, "refuse") || !std::strcmp(mode, "same"))
        {
            check(count == 2 && observed[1] == 2, "both occupied slots visited");
            samePayload(b, savedB); samePayload(c, savedC);
        }
        else if (!std::strcmp(mode, "zero"))
        {
            check(count == 1, "single occupied input");
            samePayload(a, savedB); samePayload(b, savedA);
        }
        else if (!std::strcmp(mode, "cross"))
        {
            check(count == 2 && observed[1] == 2, "later slot visited");
            check(observedMember[1] == savedB.m_memberIndex, "swap precedes later selector");
            samePayload(b, savedC); samePayload(c, savedB);
        }
        else if (!std::strcmp(mode, "shrink")) check(count == 1, "fresh reduced bound");
        else if (!std::strcmp(mode, "grow") || !std::strcmp(mode, "replace"))
        {
            check(count == 2 && observed[1] == 2, "fresh increased bound/vector");
            check(observedMember[1] == savedD.m_memberIndex, "fresh replacement slot");
        }
        else if (!std::strcmp(mode, "clear_next")) check(count == 1, "fresh future occupancy");
        else if (!std::strcmp(mode, "replace_current"))
        {
            check(count == 1 && f.m_slots[1] == &d, "replacement before swap lookup");
            samePayload(d, savedC); samePayload(c, savedD); samePayload(b, savedB);
        }
        else return 3;
    }
#endif
    std::printf("PASS %s %d\n", mode, checks);
    return 0;
}
