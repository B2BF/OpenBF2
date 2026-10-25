// Authoritative round state, tickets and map transitions. Reset returns to pre-game
// and resets managers, connections and network ids; the next pre-game tick arms
// spawners and advances to playing. Per-state bodies retain their own frontiers.
// [CHOSEN] Some field names describe observed roles; offsets are platform-specific.

#ifndef GAME_GAMELOGIC_SERVERGAMELOGIC_H     // [INFERRED] guard style
#define GAME_GAMELOGIC_SERVERGAMELOGIC_H

#include <map>
#include <vector>

#include "GameLogic.h"
#include "TicketHistory.h"                 // m_ticketHistory (+0x280) - the real element type
#include "../../Core/IBase.h"   // IBase - create()'s return and its aggregation argument

namespace dice {
namespace hfe {

namespace world { class IProjectileObject; }

class ServerGameLogic : public GameLogic
{
public:
    ServerGameLogic();                                //  (C2) / 0x80FAF80 (C1) - read, written

    // the IGameLogic/IBase quartet, ALL four overridden here
    // IGameLogic declares them pure at +0x00..+0x0C; the bodies the ELF emits are ServerGameLogic's
    // own, not GameLogic's, so they are declared on this class. Without them this class is abstract
    // and `create` below cannot new it.
    virtual void  addRef();                           // +0x00
    virtual int   getRef();                           // +0x04
    virtual int   release();                          // +0x08
    virtual void* queryInterface(unsigned int) const; // +0x0C

    // Creates the server game-logic implementation for the requested interface.
    static IBase* create(unsigned int iid, IBase* outer);

    // Clears the networkable registry, chains to GameLogic::init(), and returns true. Neither
    // callee's return value is propagated in retail.
    virtual bool init();
    //  (D2) / 0x80FB310 (D1), 499 B - the commander-tools and UAV
    // managers, then the members, then ~GameLogic.
    virtual ~ServerGameLogic();
    virtual bool unload();

    // the state machine, all [S] overrides of IGameLogic
    virtual void  update(float deltaTime);            //   +0x20
    virtual void  reset();                            //   +0x24

    virtual void  playEffect(Vec3 position, const world::IObjectTemplate* effect, Vec3 direction);
    virtual void  spawnMultipleObjects(const world::IObjectTemplate* objectTemplate, int count);
    virtual world::IObject* spawnObject(const std::string& name, Vec3 position, Vec3 rotation, int team);
    virtual world::IObject* spawnObject(const world::IObjectTemplate* objectTemplate, Vec3 position,
                                        Vec3 rotation, int team);

    // The `gameLogic.createObject` body (ELF 0x4a8020, dedicated PE 0x455B60). Its only caller is
    // the ConsoleClass195 executor. Non-virtual and server-only: ClientGameLogic::createObject is
    // an empty stub. Looks the template up by name, spawns it at the local player's vehicle (or
    // free camera), activates a projectile result, then applies the debug +0x274 spawn scale.
    void createObject(std::string name);

    // Processes player input actions each tick. PlayerInput is passed by value; the
    // trailing flag is forwarded only to weapon selection.
    virtual void  checkPlayerTriggers(world::IPlayer* player, world::IObject* vehicle,
                                      io::PlayerInput input, bool flag);

    // [CHOSEN] Factored input handling for zipline/rope refunds. Uses standing height
    // for the short zipline search and selects the last eligible rope link; rope
    // cooldown belongs to the rope, not the link.
    static void checkSpecialForcesTriggers(world::IPlayer* player, world::IObject* soldier,
                                           const io::PlayerInput& input);

    // Toggles entry/exit with a shared cooldown. The enter flag selects a 3 m query and
    // each entry point's configured radius; false uses a flat 5 m radius for spawning.
    // [INFERRED] The parameter name does not describe that radius selection.
    void toggleEntryPoint(world::IPlayer* player, bool enter);
    // Returns whether a nearby kit was picked up; callers can issue a new kit on failure.
    bool pickUpClosestKit(world::IPlayer* player, world::IObject* soldier,
                          float radius);
    // The other half of that pair - the fallback when nothing is within reach.
    void getNewKit(world::IPlayer* player);

    // the non-virtual round handlers
    void  setGameStatus(GameStatus status, bool, float delay);   // the bool is
                                                      //   never read by the body (it is stored
                                                      //   nowhere and passed nowhere) [UNRESOLVED
                                                      //   its role]; both callers pass false
    void  uPreGame(float deltaTime);
    void  uFirstPreGame();
    // The three sub-ticks uPlaying drives are recovered; the shared explosion phase is documented
    // in GameLogicExplosion.h.
    void  uPlayingSpawning(float deltaTime);
    // Applies death delay, credit, vehicle exit, squad and visual updates around the base kill.
    // Returns the base result and invokes the requested final Python callback.
    virtual bool killPlayer(world::IPlayer* player, bool a, bool b);
    void  spawnPlayer(world::IPlayer* player,
                      std::pair<Vec3, Vec3>* at);
                      // Handles ECGame create/destroy/spawn events. Event 4 initializes the player's default team synchronously.
    virtual void handleEvent(EventCategory category, unsigned int id,
                             EventNode* node, void* data);          // override, 0x81011E0, 538 B

    // handleEvent's ECGame callees - non-virtual, called directly (not through a vtable) exactly as
    // the ELF does. Each returns 1 like the original; handleEvent discards it.
    int _createPlayer(world::IPlayer* player);        // setTeam(2)/setSpawnGroup(0)/
                                                      //   setIsAlive(false)/setKit(2)
    int _createObject(world::IObject* object);        // `return 1`, ignores its arg
    // Kills occupants of every seat on the destroyed object's control object.
    int _destroyObject(world::IObject* object);
    int _destroyPlayer(world::IPlayer* player);

    void  uPlayingInsideGameArea(float deltaTime);
    int   getTicketLimitReachedId(unsigned int teamIndex, int tickets);
    void  uPlayingTicketSystem(float deltaTime);
    void  uPlaying(float deltaTime);                  // Updates spawning, explosions, combat area, and tickets in order.
    void  uFirstPlaying();                            // : m_firstPlaying = false
    void  uEndGame(float deltaTime);
    void  uFirstEndGame();
    void  restartMap();                               //   the round reset
    void  loadNextLevel();                            //   unload + loadPath
    int   calcNextMapIndex() const;                   //   (current+1 or next) % count
    // the round END. reached from runNextLevel and (once
    // bound) python's sgl_endGame. _ZN4dice3hfe15ServerGameLogic7endGameEji - non-virtual.
    void  endGame(unsigned int winnerTeam, int victoryType);

    // Returns the map-list index used by the status display.
    int getCurrentLevel() const;
    int getNextLevel() const;                         // : return calcNextMapIndex()

    // StatusMonitor::update reads m_roundsPlayed (+0x210) directly to build the "Round: n/m" field,
    // exactly as the ELF does - there is no accessor for it in the binary.
    friend class StatusMonitor;


    // Server behavior overrides inherited IGameLogic slots; many base implementations
    // are intentionally inert, so these overrides are required for gameplay.
    virtual bool  close();                              // +0x1C [S]
    virtual unsigned int getDefaultNumberOfTickets(unsigned int);           // +0x98 [S]
    virtual unsigned int getDefaultNumberOfTicketsEx(unsigned int,
                                                     unsigned int);         // +0xA0 [S]
    virtual float getDefaultTimeToNextAIWave();                             // +0xA8 [S]
    virtual float getHitIndicationThreshold();                              // +0x284 [S]
    virtual bool  getOutputPlayerStats();                                   // +0x264 [S]
    virtual bool  getPause() const;                                         // +0x224 [S]
    virtual TicketHistory& getTicketHistory();                              // +0x278 [S]
    virtual float getTicketLossAtEndPerMin();                               // +0xB0 [S]
    virtual float getTicketLossPerMin(unsigned int team);                   // +0xB8 [S]
    virtual int   getTicketState(unsigned int team);                        // +0x90 [S]
    virtual bool  giveDamage(world::IObject*, float, world::IPlayer*,
                             std::list<WeakPtr<world::IPlayer> >*,
                             world::IObject*, int, world::IObject*,
                             world::IObject*, Vec3, Vec3, int,
                             bool, bool, bool);                             // +0xE4 [S] 0x8101DB0, 2,588 B
                                                                            //   (RW-30)
    virtual void  handleCollision(world::IObject*, world::IObject*,
                             const Vec3&, const Vec3&, const Vec3&, int, int,
                             world::CollisionFaceInfo*,
                             world::CollisionFaceInfo*);                    // +0x100 [S]
    virtual bool  handleCollisionForProjectile(world::IObject*, world::IObject*,
                             const Vec3&, const Vec3&, const Vec3&, int, int,
                             world::CollisionFaceInfo*,
                             world::CollisionFaceInfo*);                    // +0xF4 [S]
    virtual bool  handleCollisionForReplenisher(bool, world::IObject*,
                             world::IObject*, const Vec3&, const Vec3&,
                             const Vec3&, int, int);                        // +0xF8 [S]
    virtual bool  handleCollisionForResurrecter(world::IObject*, world::IObject*,
                             float, float, const Vec3&, const Vec3&,
                             const Vec3&, int, int);                        // +0xFC [S]
    virtual void  handleDrop(world::IObject*, world::IPlayer*,
                             world::IObject*, bool);                        // +0xF0 [S] ELF 0x80FBE10 / PE 0x454440
    virtual void  handleExplosion(Vec3, float, int, float, world::IPlayer*,
                                  world::IArmorGLComp*, int, world::IObject*,
                                  world::IObject*, float, world::IObject*,
                                  world::IObject*, float, bool, bool);      // +0xE8 [S]
    virtual void  handlePickup(world::IObject*, world::IPlayer*,
                               world::IObject*, bool);                      // +0xEC [S]
    virtual bool  heal(world::IObject*, float, world::IPlayer*, int, int,
                       float*, bool, bool);                                 // +0x104 [S]
    virtual bool  reSpawnPlayer(world::IPlayer*);                           // +0x70 [S]; returns the base killPlayer result.
    virtual bool  replenishAmmo(world::IObject*, float, world::IPlayer*,
                                int, int, float*, bool);                    // +0x108 [S]
    virtual void  runNextLevel();                       // +0x2C [S]
    virtual bool  selectKitAndUnlockLevel(world::IPlayer*, int, int);       // +0x7C [S]
    virtual bool  selectSpawnGroup(world::IPlayer*, int);                   // +0x74 [S]
    virtual bool  selectTeam(world::IPlayer*, int);                         // +0x78 [S]
    virtual void  setDefaultNumberOfTickets(unsigned int, unsigned int);    // +0x94 [S]
    virtual void  setDefaultNumberOfTicketsEx(unsigned int, unsigned int,
                                              unsigned int);                // +0x9C [S]
    virtual void  setDefaultTimeToNextAIWave(float);                        // +0xA4 [S]
    virtual void  setHitIndicationThreshold(float);                         // +0x280 [S]
    virtual void  setNextLevel(int);                                        // +0x30 [S]
    virtual void  setOutputPlayerStats(bool);                               // +0x304 [S]
    virtual void  setPause(bool);                                           // +0x220 [S]
    virtual void  setTicketHistory(const TicketHistory&);                   // +0x27C [S]
    virtual void  setTicketLossAtEndPerMin(float);                          // +0xAC [S]
    virtual void  setTicketLossPerMin(unsigned int team, float);            // +0xB4 [S]
    virtual void  setTicketState(unsigned int team, int);                   // +0x8C [S]

    // Python ticket controls use distinct integer/float types. setTicketChangePerSecond
    // updates the team's ticket bleed.
    void setNumberOfTickets(unsigned int team, unsigned int tickets);
    void setTicketLimit(unsigned int team, int id, int limit);
    void setTicketChangePerSecond(unsigned int team, float rate);
    void setUnlocks(world::IPlayer* player, std::map<int, int>* unlocks);
    // Non-virtual support-action helper. Signature attested by
    // _ZN4dice3hfe15ServerGameLogic9resurrectEPNS0_5world17IProjectileObjectEPNS2_7IObjectEfffPNS2_7IPlayerEii.
    bool resurrect(world::IProjectileObject* projectile, world::IObject* object,
                   float amount, float resurrectDamage, float restoreHitPoints,
                   world::IPlayer* player, int materialA, int materialB);
    // Non-virtual getters used by Python parameter lookup.
    unsigned int getNumberOfTickets(unsigned int team) const;   // TeamScore +0x84 raw
    int   getTicketLimit(unsigned int team, int key);           // the limit map find
    float getTicketChangePerSecond(unsigned int team);
    virtual void  suicide();                                                // +0x68 [S]
    virtual void  toggleLocalFreeCamera();                                  // +0x208 [S]
    void do_toggleFreeCamera(world::IPlayer*);          // inlined in PE 0x4558A0
    virtual void  togglePause();                                            // +0x218 [S]
    virtual void  triggerSpecialGameEvent(SpecialGameEventType, void*);     // +0x120 [S]

protected:
    // THE fields, in ELF order (the ctor's stores 0x80FAD60..0x80FAE9B + reset's)
    int          m_refCount;                          // +0x1EC = 1 (addRef 0x8105AD0 / release 0x8105B00 - unwritten)
    bool         m_preGame;                           // +0x1F0  reset -> true; uFirstPreGame -> false
    bool         m_firstPlaying;                      // +0x1F1  reset -> true; uFirstPlaying -> false
    bool         m_firstEndGame;                      // +0x1F2  reset -> true; uFirstEndGame -> false
    unsigned int m_roundStartTick;                    // Round start tick used for the time limit; -1 means not started.
    unsigned int m_tickCount;                         // +0x1F8 = 0   IDB "tickCount"
    float        m_startDelay;                        // +0x1FC = 0.0 IDB "startDelay" - uPreGame's spawn-at
                                                      //          tick = 30 * this + now
    float        m_endOfRoundDelay;                   // +0x200  uFirstEndGame seats it from ServerSettings;
                                                      //          uEndGame counts it down (unseeded by the ctor)
    int          m_dword204;                          // +0x204 = 0   [UNRESOLVED]
    float        m_defaultTimeToNextAIWave;           // +0x208 = 8.0 (set/getDefaultTimeToNextAIWave)
    int          m_dword20C;                          // +0x20C = 0   [UNRESOLVED]
    int          m_roundsPlayed;                      // +0x210 = 0   [CHOSEN] - uEndGame ++s it against
    bool         m_roundGoing;                        // +0x214 = false  IDB "roundGoing"
    float        m_ticketChangeRemainder[2];          // +0x218 [team-1] fractional bleed carried
                                                      //          between playing ticks
    float        m_ticketChangePerSecond[2];          // +0x220 [team-1]  reset -> 0, 0 (set/getTicketChangePerSecond)
    bool         m_byte228;                           // +0x228  reset -> false  [UNRESOLVED]
    bool         m_byte229;                           // +0x229  reset -> false  [UNRESOLVED]
    float        m_ticketLossPerMin[2];               // +0x22C [team-1]  reset -> 10.0, 10.0 (set/getTicketLossPerMin)
    float        m_ticketLossAtEndPerMin;             // +0x234 = 1000.0 (set/getTicketLossAtEndPerMin)
    unsigned int m_defaultNumberOfTickets[2];         // +0x238 [team-1]  reset -> 50, 50 (setDefaultNumberOfTickets)
    bool         m_byte240;                           // +0x240  reset -> false  [UNRESOLVED]
    bool         m_byte241;                           // +0x241  reset -> false  [UNRESOLVED]
    std::map<int, int> m_ticketLimits[2];             // +0x244 / +0x250 [team-1], {ID -> threshold}; reset clears
    int          m_previousTicketCount[2];            // +0x25C / +0x260 [team-1] = defaults at reset,
                                                      //          then the rolling limit-check snapshot
    float        m_hitIndicationThreshold;            // +0x264 = 0.02 (set/getHitIndicationThreshold)
    bool         m_byte268;                           // +0x268 = true   [UNRESOLVED]
    bool         m_byte269;                           // +0x269 = false  [UNRESOLVED]
    bool         m_unlockAll;                         // +0x26A = false  IDB "unlockAll" (setUnlockAll)
    bool         m_byte26B;                           // +0x26B = false  [UNRESOLVED]
    int          m_statusBeforePause;                 // ELF +0x26C / PE +0x354; [INFERRED] name, togglePause stores it
    bool         m_outputPlayerStats;                 // +0x270 = false (set/getOutputPlayerStats)
    int          m_dword274;                          // +0x274 = 0   [UNRESOLVED]
    int          m_currentLevel;                      // +0x278 = 0   IDB "currentLevel" (getCurrentLevel)
    int          m_nextLevel;                         // +0x27C = -1  IDB "nextLevel" (set/getNextLevel)
    TicketHistory m_ticketHistory;                    // +0x280; completed-round history.
    bool         m_shouldUpdateTicketSystem;          // +0x28C = false  IDB "shouldUpdateTicketSystem"
    bool         m_byte28D;                           // +0x28D = true   [UNRESOLVED]
    // The secondary IEventListener base reaches the same handleEvent implementation
    // through compiler-generated this adjustment.
};

} // namespace hfe
} // namespace dice

#endif // GAME_GAMELOGIC_SERVERGAMELOGIC_H
