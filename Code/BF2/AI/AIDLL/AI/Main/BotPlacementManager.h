// Holds the per-team bot placements that console commands register and spawnBot consumes.
// Constructed when the DLL loads (this file's static initializer) and destroyed by BF2AI::clear.
// Source placement is attested by spawnBot's diagnostic path.

#ifndef AI_AIDLL_AI_MAIN_BOTPLACEMENTMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_BOTPLACEMENTMANAGER_H

#include <list>
#include <string>

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// One registered placement. Retail element size is 0x24 (list node 0x2C): the list's __clear
// (0x3E4C40) destroys strings at +0x00, +0x1C and +0x20; +0x04..+0x1B hold no destructor and are
// filled from addPlacement's two Vec3 arguments. Field names [INFERRED] from addPlacement's
// parameter order (std::string const&, Vec3 const&, Vec3 const&, std::string const&, int,
// std::string const&); the int selects the team list and is not stored.
struct BotPlacement
{
    /* +0x00 */ std::string m_name;
    /* +0x04 */ Vec3 m_position;
    /* +0x10 */ Vec3 m_rotation;
    /* +0x1C */ std::string m_string1C;     // [UNRESOLVED] meaning
    /* +0x20 */ std::string m_string20;     // [UNRESOLVED] meaning
};

class BotPlacementManager
{
public:
    BotPlacementManager();          // ELF 0x3E2D30 (C2), 0x3E2E30 (C1)
    ~BotPlacementManager();         // ELF 0x3E2E80 (D2), 0x3E2EF0 (D1) - not virtual

    // Appends a placement to team 1 or 2. The team index is unchecked.
    bool addPlacement(const std::string& name, const Vec3& position, const Vec3& rotation,
                      const std::string& string1C, int team, const std::string& string20);
    // Emits the retail error, then consumes one placement when available. If the error is
    // filtered, the retail body only renames the last-created bot; it creates no new bot.
    bool spawnBot(int team);

    static BotPlacementManager* m_instance;

private:
    // Retail size 8 (`operator new(8)` in the static initializer): two list heads, one per team,
    // indexed team - 1 by addPlacement. The constructor and destructor walk them as an array.
    /* +0x00 */ std::list<BotPlacement> m_placements[2];
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_BOTPLACEMENTMANAGER_H
