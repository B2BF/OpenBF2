// Controls AI level of detail and update budgets.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_AILODMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_AILODMANAGER_H

#include <map>
#include <set>
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class Bot;
class IAIPlayer;

// Per-player position and distance used when selecting bot detail levels.
class PlayerLODStats
{
public:
    PlayerLODStats();
    explicit PlayerLODStats(IAIPlayer* player);
    PlayerLODStats(const PlayerLODStats& other);
    ~PlayerLODStats();
    void update(float basicDistance, float speedScale);

    // ELF offsets; names are [INFERRED]. The player is borrowed.
    /* +0x00 */ Vec3 m_position;
    /* +0x0C */ IAIPlayer* m_player;
    /* +0x10 */ float m_lodDistance;
};

class AILODManager
{
public:
    AILODManager(float basicLODDistance, float f36);
    ~AILODManager();                                   // ELF 0x39B690

    // The whole vtable. See the header note.
    virtual bool reset();

    void setBasicLODDistance(float distance);
    bool lodEnable(bool enable);   // ELF 0x39C060 answers the stored value (ConsoleClass293 keeps it)
    bool lodEnable();

    void updateAllPlayers();
    void updatePlayers();
    bool updatePlayer(int playerId);                  // [INFERRED] bool: every exit returns 1
    void updatePlayerMap();
    void updateBots();
    bool updateBot(Bot* bot);
    void removePlayer(int playerId);

    static AILODManager* m_instance;

    // layout, on the ELF, read store by store out of the constructor
    /*  +4 */ std::map<int, PlayerLODStats> m_playerStats;   // `reset` clears it
    /* +16 */ std::set<int> m_updatedPlayerIds;             // [INFERRED] IDs seen in the current sweep
    /* +28 */ float m_basicLODDistance;    // = the first argument; `setBasicLODDistance` writes it
    /* +32 */ float m_maxLODDistance;      // = the first argument too - see the header note [INFERRED]
    /* +36 */ float m_f36;                 // speed scale; second constructor argument
    /* +40 */ bool  m_lodEnable;           // = true; the `lodEnable` pair, and `reset` raises it again
    /* +44 */ int   m_playerCursor;        // = -1; next player ID [INFERRED]
    /* +48 */ int   m_botCursor;           // = 0; next bot index [INFERRED]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AILODMANAGER_H
