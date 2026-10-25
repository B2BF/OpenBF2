// Shared IGameLogic implementation used by client and server game logic. Member
// offset comments describe the original ELF; this source-level declaration is
// not a binary-layout overlay. [UNRESOLVED] Fields retain offset-based names.
#ifndef GAME_GAMELOGIC_GAMELOGIC_H     // [INFERRED] guard style
#define GAME_GAMELOGIC_GAMELOGIC_H

#include <list>
#include <map>
#include <set>
#include <string>
#include "../IGameLogic.h"
#include "../../Math/Vec3.h"      // completes Vec3 for the pre-spawn camera members
#include "../IEventListener.h"     // the second base - see the note on the class below
#include "../../IO/File/IFileSystem.h"   // NoCaseStringCompare for the reverse team-name map

namespace dice {
namespace hfe {

namespace world { class ObjectSpawnerTemplate; }   // convertOneDynamicObject's map value
namespace world { class IPlayerControlObject; }    // switchToWeaponFromIndex's receiver
namespace world { class StrategicObjectHolder; }   // the strategic-object map's value
class VoiceMessages;

// Shared game-logic implementation and local event listener.
// The distinct IEventListener base receives load/lifecycle events; one handleEvent
// override serves both interfaces with compiler-generated subobject adjustment.
class GameLogic : public IGameLogic, public IEventListener
{
public:
    //  (D2) / 0x80CB120 (D1), 2,570 B - (the clean-exit path, through
    // ServerGameLogic::release's `delete this`). Unregisters the three event handlers the ctor
    // registered, deletes the five level managers, resets the VoIP block, releases the vote
    // managers, resets the heightmap cluster; see the body.
    virtual ~GameLogic();
    // Exact nonvirtual dependency of Game's two local-message helpers. A field load - m_voiceMessages
    // at +0x108 (`mov eax, [eax+108h]`, ELF 0x80D81F0 / PE 0x465980).
    VoiceMessages* getVoiceMessages() const;

    // Seeds state, registers game-logic events/listeners, and constructs level manager singletons.
    GameLogic();

    // Marks the object-template registry as engine-initialized. Both retail builds dispatch
    // setEngineIsInitialized() at ObjectTemplateManager +0x10 and return true unconditionally.
    virtual bool init();

    // Loads level content for GameServer.
    virtual bool load(const std::string& levelPath);
    virtual bool unload();                                    // returns true

    // Loads initialization and content console scripts during level loading.
    virtual bool loadInitFile(const std::string& path);
    virtual bool loadAllConFiles(const std::string& path);

    // Maintains the shared control-point registry; the containers own their counts.
    virtual void addControlPoint(world::IObject* cp, int group);
    virtual void removeControlPoint(world::IObject* cp);

    // Reports whether spawn-point initialization/destruction is inside precaching.
    virtual bool isPrecaching() const;

    // The stock GameLogicInit.con property. Both ports store/read the byte immediately after
    // m_isPrecaching; the setter is nonvirtual and the getter occupies IGameLogic +0x22C.
    void setEnableVoHelp(bool enable);

    // slots +0x194 / +0x19C, (the PCO helper burn-down)
    // PlayerControlObject's root lifecycle: init registers, reset deregisters. addPco 0x80D6450
    // (62 B): insert_unique(id -> object) - a duplicate id keeps the first. removePco 0x80D64A0
    // (150 B): find + erase.
    virtual void addPco(world::IObject* pco);
    virtual std::map<unsigned int, world::IObject*>* getPcoMap() const; // `lea eax,[this+90h]`
    virtual void removePco(world::IObject* pco);

    // Groups nearby strategic-object pieces under a name plus holder-index key.
    // Addition assigns a holder index; removing the last piece deletes its holder.
    virtual void  addStrategicObject(world::IObject* object);
    virtual std::map<std::string, world::StrategicObjectHolder*>*
                  getStrategicObjectMap();
    virtual void  removeStrategicObject(world::IObject* object);

    // Object-spawner registry keyed by object ID. The getter returns borrowed map storage.
    virtual void addObjectSpawner(world::IObject* spawner);
    virtual void* getObjectSpawnerMap() const;
    virtual void removeObjectSpawner(world::IObject* spawner);
    virtual world::IObject* getObjectSpawner(unsigned int id) const;

    // Removes a registered target during its destruction.
    virtual void removeTargetObject(world::IObject* target);          // +0x1B4

    // Provides the team flag used when a control-point template has no explicit flag name.
    virtual const std::string& getTeamFlag(int team) const;

    // Stores or retrieves VOIP enablement, address, server/client ports, and quality.
    // Preserve the declared address types: unsigned long input and unsigned int output.
    virtual void  storeVoipServerInfo(bool enabled, unsigned long address, unsigned short serverPort,
                                      unsigned short bfClientPort, int quality);
    virtual void  getVoipServerInfo(bool& enabled, unsigned int& address, unsigned short& serverPort,
                                    unsigned short& bfClientPort, int& quality);

    // Returns client VOIP state cached by the local player's update.
    virtual bool  isVoipClientRunning();                       //   (byte +0x17C)

    // Round state, combat-area bounds, and world-object registries. reset enters pre-game;
    // ServerGameLogic's first pre-game update resets control points before play begins.
    virtual void  update(float deltaTime);                    // empty base
    virtual void  reset();
    virtual void  setUseActiveCombatArea(bool use);
    virtual bool  getUseActiveCombatArea();
    virtual void  setActiveCombatArea(float a, float b, float c, float d);
    virtual void  getActiveCombatArea(float* a, float* b, float* c, float* d);
    virtual bool  isServer() const;
    virtual int   getGameStatus();
    virtual BeginRoundState getBeginRoundState();
    virtual void  setBeginRoundState(BeginRoundState state);
    virtual unsigned int getNextStateChangeAtTick();          //   (+0x194)
    virtual void  setNextStateChangeAtTick(unsigned int tick); //   (+0x194)
    virtual void  addRemoteControlledObject(world::IObject* object);
    virtual void* getRemoteControlledObjectMap() const;                    // `lea eax,[this+B4h]`
    virtual void  removeRemoteControlledObject(world::IObject* object);
    virtual world::IObject* getRemoteControlledObject(unsigned int id) const;
    virtual void  addProjectile(world::IObject* projectile);
    virtual void* getProjectileMap() const;                   // `lea eax,[this+9Ch]`
    virtual void  removeProjectile(world::IObject* projectile);
    virtual void  addSupplyDepot(world::IObject* depot);
    virtual void* getSupplyDepotMap() const;                  // `lea eax,[this+84h]`
    virtual void  removeSupplyDepot(world::IObject* depot);
    virtual Vec3  getBeforeSpawnCameraPosition() const;
    virtual Vec3  getBeforeSpawnCameraDirection() const;
    virtual void  resetPlayerFreeCameraPosition(world::IPlayer* player);
    virtual bool  isLevelLoaded() const;
    virtual void  killAllPlayers();
    virtual void  resetAllPlayers();
    virtual void  resetAllObjects();
    void          dropAllKits();                              // NON-virtual (direct call)
    void          destroyAllControlPoints();                  // snapshot + destroy
    // Stores the pre-spawn camera transform, then re-seats the local human player's free camera.
    // NON-virtual: the gameLogic.setBeforeSpawnCamera executor calls it directly.
    void          setBeforeSpawnCamera(Vec3 position, Vec3 direction);

    // Marks root objects created-on-load so round resets preserve them. Returns false
    // if load cancellation interrupts the registry walk.
    virtual bool  setCreatedOnLoadFlag();
    virtual bool  loadUpdateComponents();
    // Creates the sunlight LightSource template before loading object scripts.
    virtual void  createSunTemplate();

    // Shared IGameLogic/IEventListener override. Handles core and game-logic events;
    // ECGame is registered but intentionally ignored here.
    virtual void handleEvent(EventCategory category, unsigned int id,
                             EventNode* node, void* data);

    // Returns a borrowed reference to the team's name.
    virtual const std::string& getTeamName(int team);         //   (+0x48 map)
    virtual void  createPreStoredObjects();
    void          endLoadStats();

    // Static input helpers used by per-player trigger processing.
    static bool getIsSprintRequested(io::PlayerInput input, world::IObject* vehicle);

    // Processes weapon keys, cycling, underwater switching, gas-mask and night-vision controls.
    void selectWeapon(world::IPlayer* player, world::IObject* vehicle,
                      io::PlayerInput input, bool flag);

    // Changes the active weapon and returns whether it changed.
    bool switchToWeaponFromIndex(world::IPlayerControlObject* pco, world::IPlayer* player,
                                 world::IObject* vehicle, bool flag,
                                 int itemIndex);

    // Non-virtual members load() calls directly:
    void beginLoadStats(const std::string& levelPath);
    bool convertDynamicObjects();                             // frame
                                                              // [CHOSEN] Map from template ID to synthesized dynamic-object spawner template.
    typedef std::map<int, world::ObjectSpawnerTemplate*> SpawnerTemplateMap;
    void convertOneDynamicObject(world::IObject* object, SpawnerTemplateMap& spawners);

public:
    // Fallback target-ID sequence: returns counter - 10000, then increments.
    // [UNRESOLVED] Original access relationship; TargetObject modifies it directly.
    int         m_targetObjectIdCounter;   // +0x0C [CHOSEN name]

    // Kit data keyed by (team << 8) | kit: soldier names, kit names, and a separate
    // resolved-pointer cache. Updating a name does not invalidate that cache.
    /* +0x10 */ std::map<int, std::string> m_soldierSkins;   // [CHOSEN] name, from getSoldierSkin
    /* +0x1C */ std::map<int, std::string> m_kits;           // > the kit template's name
    // THE cache, and it is a memoisation not a registration: getKitTemplate answers from here when
    // it can, otherwise resolves +0x1C's name through objectTemplateManager and inserts the result.
    // Nothing ever evicts it - a template swapped at runtime keeps answering the old pointer.
    /* +0x28 */ std::map<int, const world::IObjectTemplate*> m_kitTemplates;

    friend class ServerGameLogic;   // spawnPlayer reads two of the three kit maps directly

public:
    // Registers team/kit template pairs from level initialization scripts for subsequent spawning.
    virtual void setKit(int team, int kit,
                        const std::string& kitTemplate,
                        const std::string& soldierTemplate);              // +0x140
    virtual world::IObjectTemplate* getKitTemplate(int team, int kit);    // +0x144
    virtual std::string getKitTemplateString(int team, int kit);          // +0x148
    virtual std::string getSoldierSkin(int team, int kit);                // +0x14C
    virtual void* getKits() const;                                        // +0x150


    // Overrides of the shared game-logic interface. ServerGameLogic supplies additional
    // server behavior where these base implementations are inert.
    virtual void  AddInvalidDropVehicleObject(std::string, bool);           // +0x2AC
    virtual void  ClearDropVehicles();                                      // +0x2D0 (dice caps)
    virtual std::list<std::pair<std::string, bool> >*
                  GetInvalidVehicleDropObjectList();                        // +0x2B0
    virtual void  addFlag(world::IObject*);                                 // +0x188
    virtual void  addPreStoredTemplate(unsigned int, unsigned int,
                                       unsigned int);                       // +0x34
    virtual void  addTargetObject(world::IObject*);                         // +0x1AC
    virtual void  becomeSquadLeader(bool);                                  // +0x274
    virtual void  clearConsole();                                           // +0x2E0
    virtual bool  close();                              // +0x1C [S]
    virtual void  doImpact(int, int, world::MaterialCollisionType,
                           const Vec3&, const Vec3&, const Vec3&,
                           world::IObject*, world::IObject*,
                           world::IProjectileObject*,
                           world::CollisionFaceInfo*, Vec3, bool);          // +0x10C
    virtual world::IObject* findBestTargetObject(const Vec3&, const Vec3&,
                             world::TargetType, float, float, world::IObject*,
                             world::IObject*, float, const world::IObject*,
                             bool, bool, bool);                             // +0x1BC
    virtual bool  getAirMouseInverted();                                    // +0x268
    virtual float getAirMouseSensitivity();                                 // +0x270
    virtual world::IObject* getControlPoint(unsigned int);                  // +0x178
    virtual std::map<unsigned int, world::IObject*>* getControlPointMap() const; // +0x174
    virtual bool  getControlPointsInGroup(std::vector<world::IObject*>&,
                                          int group);                       // +0x180
    virtual int   getDebugMissiles();                                       // +0x2B8
    virtual int   getDebugReplenish();                                      // +0x2C0
    virtual const std::string& getDefaultLevel() const;                     // +0xD8
    virtual int   getDropVehicleDeviationMinRadius();                       // +0x64
    virtual int   getDropVehicleDeviationRadius();                          // +0x60
    virtual bool  getEnableVoHelp();                                        // +0x22C
    virtual unsigned int getEndPreStoredNetID() const;                      // +0x40
    virtual float getExtraUAVVehicleFlightHeight() const;                   // +0xC8
    virtual int   getFeslEnvironment();                                     // +0x250
    virtual void* getFlagMap() const;                                       // +0x18C opaque
    virtual const Vec4& getFogStartEndAndBase() const;                      // +0xD0
    virtual world::IObject* getGroundDecalObject();                         // +0x2A0 a DecalObject - see m_groundDecalObject
    virtual float getHitIndicationThreshold();                              // +0x284 [S]
    virtual float getMaximumLevelViewDistance() const;                      // +0xC0
    virtual bool  getNoVehiclesMode();                                      // +0x84
    virtual int   getNumDropVehiclesForTeam(int team);                      // +0x58
    virtual int   getNumberOfControlPointGroups();                          // +0x184
    virtual float getSatelliteScanActiveTime() const;                       // +0x28C
    virtual float getSatelliteScanReloadTime() const;                       // +0x298
    virtual float getSatelliteScanVisibleTime() const;                      // +0x29C
    virtual unsigned int getStartPreStoredNetID() const;                    // +0x3C
    virtual float getSupplyDropHeight();                                    // +0x238
    virtual float getSupplyDropNumSecsToLive();                             // +0x248
    virtual world::IObject* getTargetObject(unsigned int) const;            // +0x1B8
    virtual void* getTargetObjectMap() const;                               // +0x1B0 opaque
    // returns A reference - and this is the second member of this class
    // to need it (getTeamFlag was the first; its note at 0x80D7700 records the same reasoning).
    // The body computes an address and returns it in eax with a plain `retn` - no sret slot, no
    // string copy, `this` in arg0. A by-value return could not do that.
    virtual const std::string& getTeamDropVehicle(int team) const;           // +0x50
    virtual int   getTeamFromName(const std::string&);                      // +0x4C
    // (0x80D76A0, 37 B) and BY reference - see IGameLogic.h's note. The
    // range test is `lea eax, [team-1]; cmp eax, 1; jbe`, i.e. unsigned (team - 1) <= 1.
    virtual const std::string& getTeamLanguage(int) const;                  // +0x1FC
    virtual float getTimeToNextStateChange();                               // +0x16C
    virtual float getTimeUntilRestartMap();                                 // +0x138
    virtual float getUAVActiveTime() const;                                 // +0x288
    virtual float getUAVRadius() const;                                     // +0x294
    virtual float getUAVReloadTime() const;                                 // +0x290
    virtual float getVehicleDropHeight();                                   // +0x240
    virtual bool  giveDamage(world::IObject*, float, world::IPlayer*,
                             std::list<WeakPtr<world::IPlayer> >*,
                             world::IObject*, int, world::IObject*,
                             world::IObject*, Vec3, Vec3, int,
                             bool, bool, bool);                             // +0xE4 [S] - bool, see IGameLogic.h
    virtual void  handleDrop(world::IObject*, world::IPlayer*,
                             world::IObject*, bool);                        // +0xF0 [S]
    virtual void  handleExplosion(Vec3, float, int, float, world::IPlayer*,
                                  world::IArmorGLComp*, int, world::IObject*,
                                  world::IObject*, float, world::IObject*,
                                  world::IObject*, float, bool, bool);      // +0xE8 [S]
    virtual void  handlePickup(world::IObject*, world::IPlayer*,
                               world::IObject*, bool);                      // +0xEC [S]
    virtual bool  heal(world::IObject*, float, world::IPlayer*, int, int,
                       float*, bool, bool);                                 // +0x104 [S]
    virtual bool  isLevelLoadedXP();                                        // +0x80
    virtual bool  isOmitted(std::string);                                   // +0x2A8
    virtual void  loadVoiceMessages();                                      // +0x2F0
    virtual void  noVehiclesOmit(std::string);                              // +0x2A4
    virtual void  playCollisionEffect(Vec3, const world::IObjectTemplate*,
                                      Vec3, Vec3);                          // +0x110 [CHOSEN] void - see IGameLogic.h
    virtual bool  preCacheSpawnedTemplates();                               // +0x2DC
    virtual bool  killPlayer(world::IPlayer*, bool, bool);   // +0x6C [S]; respawn calls this base implementation directly.
    virtual bool  reSpawnPlayer(world::IPlayer*);                           // +0x70 [S]
    virtual void  refreshTriggerables();                                    // +0xDC
    virtual bool  remoteCommand(int, int, int, int, int);                   // +0x124
    virtual void  removeFlag(world::IObject*);                              // +0x190
    virtual bool  replenishAmmo(world::IObject*, float, world::IPlayer*,
                                int, int, float*, bool);                    // +0x108 [S]
    virtual void  restartDemoPlayback();                // +0x28
    virtual bool  selectSpawnGroup(world::IPlayer*, int);                   // +0x74 [S]
    virtual bool  selectTeam(world::IPlayer*, int);                         // +0x78 [S]
    virtual void  setAirMouseSensitivity(float);                            // +0x26C
    virtual void  setDebugMissiles(int);                                    // +0x2B4
    virtual void  setDebugReplenish(int);                                   // +0x2BC
    virtual void  setDefaultLevel(const std::string&);                      // +0xD4
    virtual void  setDropVehicleDeviationRadius(int, int);                  // +0x5C
    virtual void  setExtraUAVVehicleFlightHeight(float);                    // +0xC4
    virtual void  setFeslEnvironment(int);                                  // +0x24C
    virtual void  setFogStartEndAndBase(const Vec4&);                       // +0xCC
    virtual void  setHitIndicationThreshold(float);                         // +0x280 [S]
    virtual void  setMaximumLevelViewDistance(float);                       // +0xBC
    virtual void  setNoVehiclesMode(bool);                                  // +0x88
    virtual void  setNumDropVehiclesForTeam(int team, int n);               // +0x54
    virtual void  setSupplyDropHeight(float);                               // +0x234
    virtual void  setSupplyDropNumSecsToLive(float);                        // +0x244
    virtual void  setTeamDropVehicle(int, const std::string&);              // +0x2CC
    virtual void  setTeamFlag(int, const std::string&);                     // +0x200
    virtual void  setTeamLanguage(int, const std::string&);                 // +0x1F8
    virtual void  setTeamName(int team, const std::string&);                // +0x44
    virtual void  setVehicleDropHeight(float);                              // +0x23C
    virtual void  startupVoipClient();                                      // +0x25C
    virtual void  stopPlayingVoMessages();                                  // +0x230
    virtual void  suicide();                                                // +0x68 [S]
    virtual void  triggerSpecialGameEvent(SpecialGameEventType, void*);     // +0x120 [S]

protected:
    // Recorded offsets are from ELF; MSVC lays these fields out differently.
    bool        m_isLevelLoaded;      // +0x38   = 0. load() refuses re-entry while set
    // THE reader IS Player::reset (ELF 0x80F3700 / PE 0x4725A0) asks
    // isLevelLoadedXP() before it bothers switching night vision off. That is the whole use - the
    // flag exists so a round reset can tell "a level is up" from "we are between levels". The
    // accessor itself (0x80C6770) is written in GameLogic.cpp; it was declared-only until then.
    bool        m_isLevelLoadedXP;    // +0x39   = 0. set beside it at exit
    bool        m_isPrecaching;       // +0x14C  = 0. bracket around the precache tail, with
                                      //          ObjectTemplate::setIsPreCaching's global twin
    bool        m_enableVoHelp;       // +0x14D ELF / +0x1FD PE; ctor default false
    // The VoIP server-info quintet (store/getVoipServerInfo above). Initialisers [UNRESOLVED] -
    // GameLogic's ctor is unrecovered; zero here (the ELF's ServerGameLogic object is heap-new'd,
    // and startServer stores all five before anything reads them).
    bool           m_voipEnabled = false;       // +0x170  (nsdmi: GameLogic has no ctor in the tree)
    unsigned int   m_voipServerAddress = 0;     // +0x174
    unsigned short m_voipServerPort = 0;        // +0x178
    unsigned short m_voipBFClientPort = 0;      // +0x17A
    // Voice-client running state; dedicated startup leaves it false.
    bool           m_isVoipClientRunning = false;   // +0x17C
    int            m_voipQuality = 0;           // +0x180
    std::string m_levelPath;          // +0x184 - the level path minus its trailing slash (IDA's
                                      //          label "server_zip", from the "/server.zip" append)
    // ELF +0x188/+0x18C; PE +0x258/+0x25C. Read/write bodies and constructor seeds were
    // checked in both images. Native fields follow compiler layout; these are not wire offsets.
    bool m_airMouseInverted;
    float m_airMouseSensitivity;
    int m_feslEnvironment;            // ELF +0x160 / PE +0x218, constructor zero
    // Two team languages indexed by team - 1, used to build radio-sample paths.
    std::string m_teamLanguage[2];    // +0xDC, +0xE0   teams 1 and 2

    // Template names replaced with dummy in no-vehicles mode. Appending allows duplicates.
    std::list<std::string> m_noVehiclesOmitList;   // +0xD8

    // +0xE4 in the ELF object - three consecutive std::strings, indexed 0..2 by getTeamFlag's
    // `lea ecx, [eax + team*4 + 0E4h]`. Team 0 is the neutral/unclaimed slot; 1 and 2 are the
    // playing teams, the same 1-based pair ControlPointTemplate's supply arrays use.
    std::string m_teamFlag[3];        // +0xE4, +0xE8, +0xEC

    // Team inputs are one-based and index these arrays as team - 1. ClearDropVehicles
    // clears only the names, retaining counts and deviation radii across level reload.
    std::string m_teamDropVehicle[2];          // +0x0F0, +0x0F4   teams 1 and 2
    int         m_numDropVehiclesForTeam[2];   // +0x0F8, +0x0FC   teams 1 and 2
    int         m_dropVehicleDeviationRadius;  // +0x100  set by setDropVehicleDeviationRadius's
                                               //          second argument - see its note
    int         m_dropVehicleDeviationMinRadius; // +0x104  ...and its first

    // The commander-drop altitude block. GameLogic's constructor seeds 50, 300, 50 in this
    // physical order (ELF +0x150/+0x154/+0x158; PE +0x200/+0x204/+0x208). Stock .con content can
    // replace each value through the paired virtual setter.
    float m_supplyDropHeight = 50.0f;
    float m_supplyDropNumSecsToLive = 300.0f;
    float m_vehicleDropHeight = 50.0f;

    // Names that make a commander vehicle landing invalid. Both ports expose the list itself:
    // ELF GetInvalidVehicleDropObjectList returns this+0x15C; PE returns this+0x20C. The pair's
    // string/bool shape is fixed by AddInvalidDropVehicleObject's list instantiation and by the
    // landing test's name comparison followed by its bool read.
    std::list<std::pair<std::string, bool> > m_invalidVehicleDropObjects;

    // Owned level voice registry. Loading replaces the prior registry before publishing the new one.
    VoiceMessages* m_voiceMessages;            // +0x108

    // The paired maps maintained by setTeamName. The reverse lookup is case-insensitive and its
    // getTeamFromName reader deliberately assumes the key exists; the forward reader returns a
    // stable empty string on a miss.
    std::map<std::string, int, NoCaseStringCompare> m_teamNameMap; // +0x3C
    std::map<int, std::string> m_teamNames;                         // +0x48

    // Template counts for object-cache prewarming. Temporary creations consume a
    // bracketed network-ID range which is returned after warming.
    std::map<unsigned int, unsigned int> m_preStoredObjects;   // +0x164 header, +0x168 count
    // Renderer-owned ground decal client, not a world object. [CHOSEN] The frozen
    // IGameLogic getter still uses IObject* at this seam; release uses the decal interface.
    // The pre-stored network-id range uses -1 to mean no range.
    void* m_groundDecalObject = 0;                       // +0xCC   (ELF zero) / 0x4F6FDA (PE store)
    unsigned int m_startPreStoredNetID = 0xFFFFFFFFu;    // +0xD0
    unsigned int m_endPreStoredNetID   = 0xFFFFFFFFu;    // +0xD4

    // The control-point registry addControlPoint/removeControlPoint maintain. ELF offsets +0x54
    // (map: header ptr + size word) and +0x60; key = the control point's controlerId (= its
    // template's controlPointId), value = the object; the groups map collects ids per groupId.
    std::map<unsigned int, world::IObject*> m_controlPointMap;    // +0x54
    std::map<int, std::set<int> >           m_controlPointGroups; // +0x60
    // Two otherwise-unexposed object registries cleared by unload(). Their value type is proven by
    // the std::map instantiations in GameLogic::unload; semantic names follow the IDB.
    std::map<unsigned int, world::IObject*> m_flagMap;            // +0x6C

    // The PCO registry addPco/removePco maintain (ELF +0x90 header ptr, +0x94 the std::map size
    // word - the same shape as the control-point map's, and the same "there is no separate
    // counter" lesson: the ELF's `--dword94` in removePco is the map's own size). Key = the
    // object's id (+0x10). IDB name: playerControlObjectMap.
    std::map<unsigned int, world::IObject*> m_playerControlObjectMap;   // +0x90
    // The object-spawner registry (ELF +0x78 header ptr, +0x7C the size word - the same shape;
    // removeObjectSpawner's `--nbObjectSpawners` is that size). Key = the spawner's object id.
    // IDB name: objectSpawnerMap.
    std::map<unsigned int, world::IObject*> m_objectSpawnerMap;         // +0x78
    // Supply-depot registry maintained by depot initialization/destruction.
    std::map<unsigned int, world::IObject*> m_supplyDepotMap;           // +0x84
    std::map<unsigned int, world::IObject*> m_projectileMap;            // +0x9C
    std::map<unsigned int, world::IObject*> m_targetObjectMap;           // +0xA8
    // Remote-controlled objects keyed by preserved ID; artillery changes refresh
    // an existing ArtilleryManager.
    std::map<unsigned int, world::IObject*> m_remoteControlledObjectMap;   // +0xB4
    // Strategic-group key is the component name plus its holder-index character.
    std::map<std::string, world::StrategicObjectHolder*> m_strategicObjectMap;   // +0xC0

    // [CHOSEN] Game status starts at zero before ServerGameLogic::reset assigns it.
    // Other defaults mirror constructor state; ServerGameLogic sets m_isServer true.
    int          m_gameStatus = 0;                 // +0x34  (GameStatus values, Game/IGameLogic.h)
    bool         m_isServer = false;               // +0x3A  isServer(); ServerGameLogic ctor -> true
    int          m_beginRoundState = 0;            // +0x190 get/setBeginRoundState (BeginRoundState)
    unsigned int m_nextStateChangeAtTick = 0;      // +0x194 get/setNextStateChangeAtTick - the tick
                                                   //   uPlaying arms for the next round transition
                                                   //   (0 means "no deadline"); StatusMonitor
                                                   //   subtracts getTickCount() from it to show the
                                                   //   countdown
    Vec3         m_beforeSpawnCameraPosition{ 0.0f, 150.0f, 0.0f };  // +0x198  free camera's pre-spawn seat
    Vec3         m_beforeSpawnCameraDirection{ 0.0f, 0.0f, 0.0f };   // +0x1A4
    bool         m_useActiveCombatArea = false;    // +0x1D0
    float        m_activeCombatArea1 = 0.0f;       // +0x1D4  IDB names activeCombatArea1..4 - the four
    float        m_activeCombatArea2 = 0.0f;       // +0x1D8  floats set/getActiveCombatArea carry;
    float        m_activeCombatArea3 = 1024.0f;    // +0x1DC  [UNRESOLVED] which is min/max x/z.
    float        m_activeCombatArea4 = 1024.0f;    // +0x1E0  reset() re-seats them (0, 0, 512, 512)
    // Fallback level used when the rotation supplies no entry.
    std::string  m_defaultLevel;                   // +0x1E4  = "gulf_of_oman"
    bool         m_noVehiclesMode;                 // +0x1E8  ctor 0x80C8BCE = 0; get/setNoVehiclesMode
                                                   // Load cancellation flag, cleared by construction and polled between load phases.
                                                   // [UNRESOLVED] Original name and setter; cancellation cleans up and returns false.
    bool m_byte1B0;                   // +0x1B0  = 0

    // Commander-asset defaults: UAV active/reload/radius 30 s/5 s/100 m;
    // satellite active/reload/visible 11 s/16 s/7 s.
    float m_uavActiveTime;
    float m_uavReloadTime;
    float m_uavRadius;

    // Extra vertical clearance added to the authored UAV flight height. Both accessors are
    // literal field operations (ELF 0x80C6950/0x80C6970, PE 0x45BB70/0x45BB80); the constructor
    // zeroes the field immediately after the UAV trio (ELF +0x1C0, PE +0x290).
    float m_extraUAVVehicleFlightHeight;

    float m_satelliteScanActiveTime;
    float m_satelliteScanReloadTime;
    float m_satelliteScanVisibleTime;
};


// Free GameLogic.cpp helper recovered from ELF 0x80D9710 and PE 0x4655F0.  The mask may carry all
// three buttons at once; retail dispatches every set bit in primary/alternate/flare order.
void sendFireMessages(unsigned int fireMask, world::IPlayer* player, world::IObject* vehicle,
                      bool flag);

} // namespace hfe
} // namespace dice

#endif // GAME_GAMELOGIC_GAMELOGIC_H
