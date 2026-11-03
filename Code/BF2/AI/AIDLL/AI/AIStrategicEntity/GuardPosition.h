// Guard-position order and bot recruitment state.
// [INFERRED] Source placement.
#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_GUARDPOSITION_H
#define AI_AIDLL_AI_AISTRATEGICENTITY_GUARDPOSITION_H

#include "../../../../Math/Vec3.h"
#include "../AIStrategic/StatusMap.h"
#include <string>
#include <vector>

namespace dice { namespace hfe { namespace ai {

class Bot;
enum CombatStrengthType : int;

class GuardPosition
{
public:
    // The position is `position` on the terrain, one metre up; the facing starts at (1, 0, 0).
    GuardPosition(const Vec2& position, CombatStrengthType strengthType, int side, int kit);
    // Member cleanup only: the borrowed bot is not deleted and no callback is invoked.
    ~GuardPosition();

    CombatStrengthType getStrengthType() const;
    const Vec3& getPosition() const;
    void setRotation(const Vec3& rotation);
    const Vec3& getRotation() const;
    void setActive(bool active);
    bool getActive() const;
    bool active() const;
    void setSide(int side);
    int getSide() const;
    void setMinDistance(float distance);
    float getMinDistance() const;
    // True while no bot was ever recruited: the recruited flag is tested before the bot.
    bool needBot() const;
    // Takes `bot`: installs this position's death and spawn callbacks, then either spawns a dead
    // bot here or sends a living one on its way.
    void recruit(Bot* bot);
    // Gives the bot a waypoint list that walks to the position.
    void botIsAlive(Bot* bot);
    void botIsDead(Bot* bot);
    // Sets the bot's spawn target (group 1, this position's kit) and force-spawns it here.
    void spawnBot(Bot* bot);
    // Hands the bot back to the bot pool. The recruited flag stays set.
    void lostBot();
    // Releases the bot when any abort condition holds: an area of the position's side with the
    // condition's status. Area indices are resolved by name on first use.
    void update();
    void addStrategicAreaAbortCondition(const std::string& areaName, StatusMap::Status status);

private:
    // Area abort condition. New entries start with areaIndex -1.
    // [INFERRED] Field names; preserve member order and types.
    struct AreaCondition
    {
        std::string areaName;
        int areaIndex;
        StatusMap::Status status;
    };

    // Offsets are ELF reference offsets; the prefix through +48 agrees with the Win32 PE.
    /* +0  */ Vec3 m_position;
    /* +12 */ Vec3 m_rotation;
    /* +24 */ CombatStrengthType m_strengthType;
    /* +28 */ bool m_active;
    /* +32 */ int m_side;
    /* +36 */ int m_kit;                     // spawnBot's setSpawnTarget kit [INFERRED name]
    /* +40 */ float m_minDistance;
    /* +44 */ Bot* m_bot;
    /* +48 */ bool m_recruited;              // [INFERRED] name; recruit sets it, nothing clears it
    /* +52 */ std::vector<AreaCondition> m_areaConditions;
};

} } }

#endif
