// [INFERRED] Header for the combat-area registry.
#ifndef GAME_GAMELOGIC_COMBATAREAMANAGER_H
#define GAME_GAMELOGIC_COMBATAREAMANAGER_H

#include <vector>
#include <map>
#include <string>
#include "../../Math/Vec3.h"

namespace dice {
namespace hfe {

class CombatArea;
namespace world { class IPlayer; class IObject; }

// Owns battlefield boundaries and their authoring cursor. The first constructed manager
// installs itself as the singleton; destruction clears that pointer only for its owner.
class CombatAreaManager
{
public:
    CombatAreaManager();                                    //  (C1), 340 B
    ~CombatAreaManager();                                   // Releases owned combat areas.

    // Test the player vehicle against area filters, or terrain bounds when no inclusion applies.
    // Null players and players without a controllable vehicle return false.
    bool isPlayerInside(world::IPlayer* player);

    // Append pathfinding-enabled areas; return true only when the resulting list has one area.
    bool getAiCombatAreas(std::vector<CombatArea*>& areas) const;

    // Apply inclusions and then exclusions from a borrowed AI area list.
    bool isAiPositionInside(const Vec3& position, const std::vector<CombatArea*>& areas) const;

    // Return the category grace period in seconds. Null objects, ladders, and absent entries
    // use the default. A non-null object must have a valid template.
    float getTimeAllowedOutside(world::IObject* object);

    // Return the category damage, falling back to the default for null objects or missing entries.
    float getDamage(world::IObject* object);
    void reset();                                           // unload's level reset,

    // Find or create an area by case-insensitive name, make it active, and return a borrowed pointer.
    CombatArea* createCombatArea(const std::string& name);

    // Select an area by case-insensitive name; a miss clears the active cursor and returns null.
    CombatArea* setActiveCombatArea(const std::string& name);

    // Erase and delete the active area if it belongs to this manager.
    void deleteActiveCombatArea();

    // Return the active area name, or an empty string when none is selected.
    std::string getActiveCombatAreaName();

    // Set or replace a per-category damage override.
    void addVehicleDamage(int vehicleCategory, float damage);

    // Set or replace a per-category grace period in seconds.
    void addVehicleTimer(int vehicleCategory, float seconds);

    static CombatAreaManager* getInstance() { return m_instance; }   // ds:0x8AAC96C

    // Borrowed authoring cursor; m_areas owns the areas. Offsets below describe the original
    // 32-bit layout and cannot be applied to another compiler's STL layout.
    /* +0x00 */ CombatArea* m_activeArea;                   // ctor 0
    /* +0x04 */ std::vector<CombatArea*> m_areas;           // begin +0x04 / end +0x08 / cap +0x0C

    // One-byte flag; padding before the following float is part of the original layout.
    /* +0x10 */ bool m_useCombatAreas;                       // ctor false

    // Fallback values used when no category override exists.
    /* +0x14 */ float m_defaultTimeAllowedOutside;           // ctor 10.0f
    /* +0x18 */ float m_defaultDamage;                       // ctor 20.0f

    // Per-category overrides. Only the timer map is seeded, with 10 seconds for categories 0 and 2.
    /* +0x1C */ std::map<int, float> m_vehicleDamage;         // ctor leaves it empty
    /* +0x28 */ std::map<int, float> m_vehicleTimers;         // ctor {0: 10.0f, 2: 10.0f}

private:
    static CombatAreaManager* m_instance;                    // ds:0x8AAC96C
};

} // namespace hfe
} // namespace dice

#endif // GAME_GAMELOGIC_COMBATAREAMANAGER_H
