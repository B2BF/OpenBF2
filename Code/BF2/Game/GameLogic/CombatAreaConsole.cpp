// Console bindings for authored combat boundaries and their active-area cursor.
// create/active select the area modified by subsequent properties; registration order is retained.
// [CHOSEN, not recovered] Vehicle-category arguments use int: the shipped metadata names
// world::VehicleCategory, but CombatAreaManager stores and exposes the value as int, so reflection
// records the int the executors actually pass.
#include "../../Core/BuildConfig.h"

#include "../../Core/ConsoleObjectMacro.h"
#include "../../Math/Vec3.h"                 // Vec2 and its stream operators
#include "CombatArea.h"
#include "CombatAreaManager.h"

#include <string>

namespace dice {
namespace hfe {
namespace {

const char* const kCombatAreaConsoleTag = "gameLogic.combatArea";

// `CAM_acc_active`. The manager singleton, then its authoring cursor.
// TWO null checks are NOT the same thing: this returns 0 when there is no manager and also when
// the manager has no active area, and the eight property rows below guard on the combined result.
CombatArea* activeCombatArea()
{
    CombatAreaManager* manager = CombatAreaManager::getInstance();   // ds:0x8AAC96C
    return manager ? manager->m_activeArea : 0;
}

// The four authoring verbs guard on the manager alone (their isObjectActive bodies are a bare
// `MEMORY[0x8AAC96C] != 0`), because `create` is what produces an active area in the first place.
CombatAreaManager* combatAreaManager()
{
    return CombatAreaManager::getInstance();
}

void combatAreaCreate(const std::string& name)        { combatAreaManager()->createCombatArea(name); }
void combatAreaSetActive(const std::string& name)     { combatAreaManager()->setActiveCombatArea(name); }
void combatAreaDeleteActive()                         { combatAreaManager()->deleteActiveCombatArea(); }
std::string combatAreaActiveName()                    { return combatAreaManager()->getActiveCombatAreaName(); }

// `min` and `max` are each ONE console property over TWO adjacent floats, not a stored Vec2:
// the executors do `*(area+4) = v.x; *(area+8) = v.y` and `*(area+12) = v.x; *(area+16) = v.y`.
Vec2 combatAreaGetMin()
{
    CombatArea* a = activeCombatArea();
    Vec2 v; v.x = a->m_minX; v.y = a->m_minZ; return v;
}
void combatAreaSetMin(const Vec2& v)   { CombatArea* a = activeCombatArea(); a->m_minX = v.x; a->m_minZ = v.y; }
Vec2 combatAreaGetMax()
{
    CombatArea* a = activeCombatArea();
    Vec2 v; v.x = a->m_maxX; v.y = a->m_maxZ; return v;
}
void combatAreaSetMax(const Vec2& v)   { CombatArea* a = activeCombatArea(); a->m_maxX = v.x; a->m_maxZ = v.y; }

// Adding a third point permanently marks the area as a polygon; polygon tests ignore rectangle bounds.
void combatAreaAddAreaPoint(const Vec2& point)
{
    CombatArea* area = activeCombatArea();
    area->m_points.push_back(point);
    if (area->m_points.size() > 2)
        area->m_isPolygon = 1;
}

// The manager rows. `use` writes the byte at +0x10 directly; the two defaults are written directly
// on the set path but read back through THE getters with a null object, which is the arm of each
// getter that returns exactly that field. Both getters null-guard, so the get path is safe.
void combatAreaManagerSetUse(bool use)                 { combatAreaManager()->m_useCombatAreas = use; }
float combatAreaManagerGetDamage()                     { return combatAreaManager()->getDamage(0); }
void combatAreaManagerSetDamage(float v)               { combatAreaManager()->m_defaultDamage = v; }
float combatAreaManagerGetTimeAllowedOutside()         { return combatAreaManager()->getTimeAllowedOutside(0); }
void combatAreaManagerSetTimeAllowedOutside(float v)   { combatAreaManager()->m_defaultTimeAllowedOutside = v; }
void combatAreaManagerAddVehicleDamage(int category, float damage)  { combatAreaManager()->addVehicleDamage(category, damage); }
void combatAreaManagerAddVehicleTimer(int category, float seconds)  { combatAreaManager()->addVehicleTimer(category, seconds); }

} // namespace

// `CombatArea` - ConsoleClass294..311, bases ds:0x8ACF580..0x8ACFDC0. Defaults at 0x8529B3F.
BF2_CONSOLE_OBJECT("CombatArea", 2, 0)

// ConsoleClass294 - exec 0x854BA90, isObjectActive 0x854B760. Find-or-create, then make active.
BF2_CONSOLE_METHOD_1_ACTIVE(create, combatAreaCreate, combatAreaManager() != 0,
                            std::string, kCombatAreaConsoleTag)

// ConsoleClass295 - exec 0x854BC80, isObjectActive 0x854BB60. argc exactly 0.
BF2_CONSOLE_METHOD_0_ACTIVE(deleteActiveCombatArea, combatAreaDeleteActive, combatAreaManager() != 0,
                            kCombatAreaConsoleTag)

// ConsoleClass296 - exec 0x854C040, isObjectActive 0x854BD10.
// A name that matches nothing clears THE cursor; see CombatAreaManager::setActiveCombatArea.
BF2_CONSOLE_METHOD_1_ACTIVE(active, combatAreaSetActive, combatAreaManager() != 0,
                            std::string, kCombatAreaConsoleTag)

// ConsoleClass297 - exec 0x854C460, isObjectActive 0x854C340. argc exactly 0, returns std::string.
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getActiveCombatAreaName, combatAreaActiveName, std::string,
                                combatAreaManager() != 0, kCombatAreaConsoleTag)

// the eight property rows, every one gated on the cursor
// ConsoleClass304 - exec 0x854C9F0. +0x30, the AI layer. Constructor default 1.
BF2_CONSOLE_FIELD(layer, int, activeCombatArea(), m_layer, kCombatAreaConsoleTag)

// ConsoleClass305 - exec 0x854CF60. +0x1C, 0 = any team.
BF2_CONSOLE_FIELD(team, int, activeCombatArea(), m_team, kCombatAreaConsoleTag)

// ConsoleClass306 - exec 0x854D540. +0x04 / +0x08.
BF2_CONSOLE_PROPERTY_FN(min, Vec2, activeCombatArea() != 0,
                        combatAreaGetMin(), combatAreaSetMin, kCombatAreaConsoleTag)

// ConsoleClass307 - exec 0x854DB50. +0x0C / +0x10.
BF2_CONSOLE_PROPERTY_FN(max, Vec2, activeCombatArea() != 0,
                        combatAreaGetMax(), combatAreaSetMax, kCombatAreaConsoleTag)

// ConsoleClass308 - exec 0x854E0D0. +0x20, a byte. This is the exclusion flag: isPlayerInside runs
// the inclusions first and the exclusions second, and being inside an exclusion clears the result.
BF2_CONSOLE_FIELD(inverted, bool, activeCombatArea(), m_isExclusion, kCombatAreaConsoleTag)

// ConsoleClass309 - exec 0x854E640. +0x14, the vehicle-category filter. 4 = all, 5 = 2 and 3.
BF2_CONSOLE_FIELD(vehicles, int, activeCombatArea(), m_vehicleCategory, kCombatAreaConsoleTag)

// ConsoleClass310 - exec 0x854EB90. +0x21, a byte, and the field directly beside `inverted` - see
// the width note in CombatArea.h. It is the AI's filter: getAiCombatAreas copies out only the
// areas whose byte is set.
BF2_CONSOLE_FIELD(usedByPathFinding, bool, activeCombatArea(), m_usedByPathFinding, kCombatAreaConsoleTag)

// ConsoleClass311 - exec 0x854EF90, isObjectActive 0x854EC50. argc exactly 1, void, takes a Vec2.
BF2_CONSOLE_METHOD_1_ACTIVE(addAreaPoint, combatAreaAddAreaPoint, activeCombatArea() != 0,
                            Vec2, kCombatAreaConsoleTag)

// `CombatAreaManager` - ConsoleClass314..318, bases ds:0x8ACFE80..0x8AD0180. Defaults at 0x8528DCB.
// Every row here guards on the manager, never on the cursor.
BF2_CONSOLE_OBJECT("CombatAreaManager", 2, 0)

// Set-only console method. Enables authored combat areas; disabled mode uses
// heightmap bounds instead.
BF2_CONSOLE_METHOD_1_ACTIVE(use, combatAreaManagerSetUse, combatAreaManager() != 0,
                            bool, kCombatAreaConsoleTag)

// ConsoleClass315 - exec 0x854F8C0, isObjectActive 0x854F400. argc exactly 2. Writes the +0x1C map.
BF2_CONSOLE_METHOD_2_ACTIVE(addVehicleDamage, combatAreaManagerAddVehicleDamage,
                            combatAreaManager() != 0, int, float, kCombatAreaConsoleTag)

// ConsoleClass316 - exec 0x854FE20, isObjectActive 0x854F960. argc exactly 2. Writes the +0x28 map.
BF2_CONSOLE_METHOD_2_ACTIVE(addVehicleTimer, combatAreaManagerAddVehicleTimer,
                            combatAreaManager() != 0, int, float, kCombatAreaConsoleTag)

// ConsoleClass317 - exec 0x8550350. SET writes +0x18 directly; GET calls getDamage(NULL).
BF2_CONSOLE_PROPERTY_FN(damage, float, combatAreaManager() != 0,
                        combatAreaManagerGetDamage(), combatAreaManagerSetDamage,
                        kCombatAreaConsoleTag)

// ConsoleClass318 - exec 0x85508A0. SET writes +0x14 directly; GET calls getTimeAllowedOutside(NULL).
BF2_CONSOLE_PROPERTY_FN(timeAllowedOutside, float, combatAreaManager() != 0,
                        combatAreaManagerGetTimeAllowedOutside(), combatAreaManagerSetTimeAllowedOutside,
                        kCombatAreaConsoleTag)

std::list<ConsoleObject*>& combatAreaConsoleObjects()
{
    return consoleBindingList(kCombatAreaConsoleTag);
}

} // namespace hfe
} // namespace dice
