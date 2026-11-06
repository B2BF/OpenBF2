// [TEMP] Two recovered queries and actual unAssignSquad are extracted verbatim.
// All constructors/destructors below are chosen native lifetimes. Real typed area
// objects provide pointer identity; no production area initialization is exercised.
#include "squad_assignment_runtime.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "MSVC x86 only");
static_assert(sizeof(AIStrategicArea*) == sizeof(int) && alignof(AIStrategicArea*) == alignof(int),
              "resolved field preserves native x86 size and alignment");
static_assert(std::is_same<decltype(SquadCoordinator::m_strategicAreaAssignment), AIStrategicArea*>::value,
              "assignment is an area pointer");
static_assert(std::is_same<decltype(&SquadCoordinator::isSquadAssignedToAnyArea), bool(SquadCoordinator::*)()>::value,
              "nonconst bool query");
static_assert(std::is_same<decltype(&SquadCoordinator::getSquadStrategicAreaAssignment), AIStrategicArea*(SquadCoordinator::*)()>::value,
              "nonconst borrowed pointer query");

static unsigned checks = 0, destroyedAreas = 0;
[[noreturn]] static void unexpected(const char* message)
{
    std::fprintf(stderr, "FAIL %s\n", message); std::fflush(stderr); std::abort();
}
static void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) unexpected(message);
}
#include "squad_assignment_mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Native lifetime setup solely for the queried fields.
SquadCoordinator::SquadCoordinator(int side, int squadIndex)
    : m_squadIndex(squadIndex), m_side(side), m_strategicAreaAssignment(nullptr),
      m_i40(4), m_i44(0), m_i60(0), m_squadLeaderIndex(-1), m_formation(nullptr), m_missionStatus(0) {}
AIStrategicObject::AIStrategicObject(float, float) {}
AIStrategicObject::~AIStrategicObject() {}
AIStrategicArea::AIStrategicArea(const std::string& name, unsigned int, float radius)
    : AIStrategicObject(radius, 0.0f), m_name(name) {}
AIStrategicArea::~AIStrategicArea() { ++destroyedAreas; }
} } }

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2) unexpected("mode required");
    const std::string mode(argv[1]);
    SquadCoordinator first(0, 3), second(1, 9);
    {
        AIStrategicArea a("first", 1, 0.0f), b("second", 2, 0.0f);
        if (mode == "null")
        {
            first.m_i40 = 123; first.m_i44 = -3; first.m_i60 = 10; first.m_missionStatus = 4;
            require(!first.isSquadAssignedToAnyArea(), "null assignment false despite nonzero neighboring state");
            require(first.getSquadStrategicAreaAssignment() == nullptr, "stored null returned");
            require(first.m_i40 == 123 && first.m_i44 == -3 && first.m_i60 == 10 && first.m_missionStatus == 4,
                    "queries leave neighboring state untouched");
            require(destroyedAreas == 0, "no area lifetime effects");
        }
        else if (mode == "identity")
        {
            first.m_strategicAreaAssignment = &a; second.m_strategicAreaAssignment = &b;
            require(first.isSquadAssignedToAnyArea(), "nonnull assignment true without calling area virtuals");
            require(first.getSquadStrategicAreaAssignment() == &a, "exact first area identity");
            require(second.isSquadAssignedToAnyArea(), "second coordinator has independent assignment");
            require(second.getSquadStrategicAreaAssignment() == &b, "exact second area identity");
            first.m_strategicAreaAssignment = &b;
            require(first.getSquadStrategicAreaAssignment() == &b, "fresh pointer read after replacement");
            require(first.isSquadAssignedToAnyArea(), "replacement remains assigned");
            first.m_strategicAreaAssignment = nullptr;
            require(!first.isSquadAssignedToAnyArea(), "fresh null after clear");
            require(first.getSquadStrategicAreaAssignment() == nullptr, "no cached pointer after clear");
            require(second.getSquadStrategicAreaAssignment() == &b, "other coordinator remains unchanged");
            require(destroyedAreas == 0, "queries do not own areas");
        }
        else if (mode == "unassign")
        {
            first.m_strategicAreaAssignment = &a; second.m_strategicAreaAssignment = &b;
            first.m_i40 = -9; first.m_i44 = 51; first.m_missionStatus = 123;
            first.m_i60 = 777; first.m_side = 13; first.m_squadIndex = -29;
            first.m_v28 = {1,2,3}; first.m_v64 = {4,5,6};
            require(first.isSquadAssignedToAnyArea(), "assigned before real unassignment");
            first.unAssignSquad();
            require(!first.isSquadAssignedToAnyArea(), "real unassignment makes query false");
            require(first.getSquadStrategicAreaAssignment() == nullptr, "real unassignment clears pointer");
            require(first.m_i40 == 4 && first.m_i44 == 0 && first.m_missionStatus == 0, "actual three other stores preserved");
            require(first.m_i60 == 777 && first.m_side == 13 && first.m_squadIndex == -29, "unrelated scalar fields preserved");
            require(first.m_v28.x == 1 && first.m_v28.y == 2 && first.m_v28.z == 3 &&
                    first.m_v64.x == 4 && first.m_v64.y == 5 && first.m_v64.z == 6, "neighbor vectors preserved");
            require(second.getSquadStrategicAreaAssignment() == &b, "other assignment untouched");
            require(destroyedAreas == 0, "unassignment does not delete borrowed area");
        }
        else unexpected("unknown mode");
    }
    require(destroyedAreas == 2, "chosen native areas destroyed only when lexical lifetimes end");
    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
    return 0;
}
