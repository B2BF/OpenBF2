// Game player with input, vehicle, camera and network replication state.
// Interface declaration order and distinct base subobjects are ABI-sensitive.
// Replication cursors belong to NetworkableBase; do not duplicate them as Player fields.
// [FRONTIER] Unimplemented declarations remain explicit boundaries at their methods.

#ifndef GAME_GAMELOGIC_PLAYER_H     // [INFERRED] guard style
#define GAME_GAMELOGIC_PLAYER_H

#include "../Objects/IPlayer.h"
#include "../../IO/Console/IConsoleSaveable.h"
#include "../../IO/Network/NetworkableBase.h"
#include "../../IO/Input/PlayerInput.h"   // m_playerInput is a whole one, BY value (+0x338)

#include <string>
#include <list>
#include <map>

namespace dice {
namespace hfe {

struct PlayerVOHelp; // [INFERRED] type spelling; retail client's owned voice-over tutorial state.

namespace world { class CPersistentExplosionEffectManager; class GrapplingHookRope; class Zipline; }

// Player replication uses a four-slot snapshot ring. Ping is copied through its
// low 16 bits. NetworkableBase owns the ring cursors.
namespace world { class ICameraObject; class IPlayer;
                  ICameraObject* getCommanderCameraForPlayer(const IPlayer* player); }
namespace world { class IPlayerControlObject; }   // getWeaponVehicle's return type
namespace world { class GameLogicDamage; }        // serverGiveDamage stamps +0x264 (team kill)

// Network updates use four per-recipient record slots.

const int kNetUpdateSlots    = 4;
const int kNetUpdateSlotSize = 0xBC;   // 188

// Per-recipient replication snapshot. updateStateMask compares current/previous
// slots and marks changed fields; buff stores the packed replicated state.
struct NetUpdateSlot                             // 0xBC (188)
{
    int           someVal;                       // +0x00
    unsigned char unk04;                         // +0x04
    unsigned char isAlive;                       // +0x05  updateStateMask's getIsAlive() snapshot
    unsigned char unk06[2];                      // +0x06
    int           someScore2;                    // +0x08  a clamped score with its own dirty bit
    unsigned char buff[176];                     // +0x0C  the packed replicated fields
};

class Player : public world::IPlayer, public io::IConsoleSaveable, public io::NetworkableBase
{
public:
    // Local aliases keep inherited interface vocabulary without exporting a using-directive.
    typedef world::IObject            IObject;
    typedef world::IObjectTemplate    IObjectTemplate;
    typedef world::ICameraObject      ICameraObject;
    typedef world::PlayerScoreData    PlayerScoreData;
    typedef world::UpdateFreqencyType UpdateFreqencyType;
    typedef world::PlayerVotingStatus PlayerVotingStatus;
    typedef io::IStream               IStream;      // io::IConsoleSaveable::makeScript

    // Mine-proximity sweep state: new entries warn, re-seen entries remain silent, and
    // untouched entries are removed with an all-clear notification.
    struct ItemStatus
    {
        ItemStatus() : m_status(0) {}
        int m_status;                                 // 0 new / 1 seen / 2 warned-or-stale
    };

    //  (C2) / 0x80E89C0 (C1), 2,568 / 2,419 B -  The field-by-field seed of a
    // 1,100-byte object. PlayerManager::createPlayer is the only caller.
    Player(int index, world::CPersistentExplosionEffectManager* explosionEffects);
    virtual ~Player();

    // the IBase quartet, slots +0x00..+0x0C - all
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();                             // deletes via vtable +0x18
    virtual void* queryInterface(unsigned int) const;
    virtual unsigned int getClassID() const;             // CID_Player

    // IPlayer overrides. queryInterface returns adjusted secondary-base pointers for
    // IConsoleSaveable and INetworkable; retain those pointers at their declared type.

    virtual void  setName(const std::string& name);   // +0x20
    virtual const std::string& getName() const;   // +0x24  (Player +0x5C)
    virtual void  setFlags(const Flag32Bits<IPlayer>& flags);   // +0x28  updateFlags on Player +0x58
    virtual bool  testFlags(const Flag32Bits<IPlayer>& flags) const;   // +0x2C
    virtual void  handleUpdate(float dt, unsigned int);   // +0x30  (Player: empty)
    virtual void  handleFrameUpdate(float dt);   // +0x34  527 B
    virtual UpdateFreqencyType getUpdateFrequencyType() const;   // +0x38  (Player +0xA0)
    virtual void  setUpdateFrequencyType(UpdateFreqencyType type);   // +0x3C
    virtual void  handleInput(io::PlayerInput& input, float dt, unsigned int);   // +0x40  948 B [UNRESOLVED return]
    virtual void  getAutoInput(io::PlayerInput& out, bool);   // +0x44  887 B
    virtual IObject* getVehicle() const;   // +0x48    115 B - the WeakPtr'd vehicle,
    virtual bool  setVehicle(IObject* vehicle, int seat);   // +0x4C
    virtual int   getInputId() const;   // +0x50  (Player +0x98)
    virtual int   getId() const;   // +0x54  (Player +0x9C)
    // Remote-state accessors and AI-mode selection. setIsAIPlayer switches input
    // ownership between an ActionBuffer and AI PlayerInput.
    virtual void  setIsRemote(bool remote);   // +0x58   (byte +0x165)
    virtual bool  getIsRemote() const;   // +0x5C   (byte +0x165)
    virtual void  setIsAIPlayer(bool ai);   // +0x60
    // Sets alive state and always clears the trace-player ID. Death also clears man-down directly, bypassing notification.
    virtual void  setIsAlive(bool alive);   // +0x68
    virtual bool  getIsAlive() const;   // +0x6C  (byte +0xB9)
    virtual void  setSuicide(bool);   // +0x70 (byte +0xBA)
    virtual bool  getSuicide() const;   // +0x74 (byte +0xBA)
    // Reads the player's AI flag rather than the base interface default.
    virtual bool  getIsAIPlayer() const;   // +0x64   (byte +0xB8)
    virtual bool  getIsConnected() const;   // +0x78  (byte +0x17C)
    virtual void  setIsConnected(bool);   // +0x7C  53 B
    virtual float getCrossHairFadeValue() const;   // +0x80  (float +0x25C)
    virtual void  setViewDistance(float);   // +0x84  238 B
    virtual float getViewDistance() const;   // +0x88  126 B
    virtual void  setFov(float fov, float time);   // +0x8C
    virtual float getFov() const;   // +0x90 (float +0xA8)
    virtual float getEndFov() const;   // +0x94 (float +0xA4)
    virtual ActionBuffer* getActionBuffer();   // +0x98  (Player +0x158)
    // The `void*` is IPlayer's frozen signature; the field it returns is a
    // real io::PlayerInput* (IO/Input/PlayerInput.h) - setIsAIPlayer is what allocates it, and the
    // one call site in GameServer.cpp already casts the result back.
    virtual void* getAIPlayerInput() const;   // +0x9C   (Player +0x15C)
    virtual void  setDefaultVehicle(IObject*);   // +0xA0  203 B
    virtual IObject* getDefaultVehicle();   // +0xA4  131 B - 0x80F3540, and it mutates
    virtual IObject* getFreeCamera() const;   // +0xA8  20 B - the +0x6C weak block's target
    // The overhead camera wins against being turned ON but not against
    // being turned OFF; the `true` half then puts the player's control-object camera into view
    // mode 1. That half is unexercised - FreeCamera::setViewMode is [FRONTIER].
    virtual void  setFreeCameraEnabled(bool);   // +0xAC
    virtual bool  getFreeCameraEnabled() const;   // +0xB0  (byte +0x70)
    virtual void  toggleFreeCameraEnabled();   // +0xB4  / PE 0x47BD30: virtual setter(!stored flag)
    virtual void  setOverheadCameraEnabled(bool);   // +0xB8
    virtual bool  getOverheadCameraEnabled() const;   // +0xBC  (byte +0x71)
    virtual void  setOverheadCameraTopDownView(bool);   // +0xC0; updates the commander-camera flag first.
    virtual bool  getOverheadCameraTopDownView() const;   // +0xC4; requires commander status and an enabled camera.
    virtual void  setCommanderCamOverhead(bool);   // +0xC8 (byte +0x73)
    virtual bool  getCommanderCamOverhead() const;   // +0xCC
    virtual int   getTickCount() const;   // +0xD0     (m_tickCount +0x248)
    virtual void  addTick();   // +0xD4      ++
    virtual void  subTick();   // +0xD8      --
    virtual void  resetTickCounter();   // +0xDC      = 0
    virtual float getTimeOutSideWorld() const;   // +0xE0  (float +0x24C)
    virtual void  setTeam(int team, bool keepVerifyPending);   // +0xE4
    virtual int   getTeam() const;   // +0xE8  (int +0xBC)
    virtual unsigned int getSpawnAtTick() const;   // +0xEC  (uint +0xC8)
    virtual void  setSpawnAtTick(unsigned int tick);   // +0xF0
    virtual float getTimeToSpawn() const;   //  / PE 0x472360: signed tick delta, zero floor
    virtual float getManDownTimeLeft() const;   // +0xF8  37 B
    // The only one of the cluster that does work: no-change early-out,
    // NametagManager +0x1C, store, then stamp m_manDownAtTick only when raising. The notification
    // fires before the store, so a synchronous listener sees the OLD value.
    virtual void  setIsManDown(bool);   // +0xFC
    virtual bool  getIsManDown();   // +0x100 (byte +0xCC)
    virtual unsigned int getManDownAtTick();   // +0x104 27 B - +0xD0 while man-down, else 0
    virtual void  setSquadId(unsigned int id);   // +0x108; narrows to unsigned short.
    virtual int   getSquadId() const;   // +0x10C (int +0xF0)
    virtual void  setIsCommander(bool);   // +0x110; forwards to the int overload.
    virtual bool  getIsCommander() const;   // +0x114 (byte +0xF4)
    virtual void  setUAVIsReady(bool);   // +0x118 (byte +0xF6)
    virtual bool  getUAVIsReady() const;   // +0x11C 59 B
    virtual void  setSatelliteIsReady(bool);   // +0x120 (byte +0xF7)
    virtual bool  getSatelliteIsReady() const;   // +0x124 59 B
    virtual void  setLastTimeCommander(double t);   // +0x128 (double +0x10C)
    virtual double getLastTimeCommander() const;   // +0x12C
    virtual void  setCommanderReapplyWaitTime(double t);   // +0x130 (double +0x114)
    virtual double getCommanderReapplyWaitTime() const;   // +0x134
    virtual void  setIsSquadLeader(bool);   // +0x138; forwards to the int overload.
    virtual bool  getIsSquadLeader() const;   // +0x13C (byte +0xF5)
    virtual bool  getIsGassed();   // +0x140 25 B - float +0x2F4 > 0
    virtual void  setIsInsideCP(bool);   // +0x144 (byte +0x108)
    virtual bool  getIsInsideCP();   // +0x148
    virtual void  setHasSimulatedFrame(bool);   // +0x14C (byte +0x166)
    virtual bool  getHasSimulatedFrame() const;   // +0x150
    virtual void  setSpawnGroup(int group);   // +0x154 (int +0xD4)
    virtual int   getSpawnGroup() const;   // +0x158
    virtual void  setInterpolationTime(unsigned int ms);   // +0x15C; unsigned clamp to 0x200.
    virtual unsigned int getInterpolationTime() const;   // +0x160 (uint +0xE0)
    virtual void  setPing(unsigned int ping);   // +0x164 27 B - clamped to 0x3FF
    virtual unsigned int getPing() const;   // +0x168 (uint +0xDC)
    virtual void  setKit(int kit);   // +0x16C (int +0x54)
    virtual int   getKit() const;   // +0x170
    virtual PlayerScoreData* getScoreData();   // +0x174 (Player +0xEC)
    virtual void  setFlagHolder(bool);   // +0x178 (byte +0x124)
    virtual bool  getFlagHolder() const;   // +0x17C
    // Selects commander camera first, then the free-camera parent chain, then the
    // vehicle camera. Preserve separate vehicle lookups in the fallback.
    virtual ICameraObject* getCamera() const;   // +0x180 211 B - commander cam / free cam / vehicle's
    virtual void  setCamera(ICameraObject* camera);   // +0x184 (Player +0x68)
    virtual bool  hasFirstPersonView() const;   // [PLATFORM] ELF slot +0x188; PE +0x184.
    virtual double getTimeConnected() const;   // +0x18C (double +0x180)
    virtual io::INetworkable* getNetworkable();   // +0x190 (Player +0x270)
    virtual bool  getStateSync(io::BitStream* stream, IObject* object);   // +0x194; writes player sync and returns true.
    // [PLATFORM] setStateSync occupies ELF +0x198 and PE +0x194.
    // It returns false only after clearing a latched stream error.
    virtual bool  setStateSync(io::BitStream* stream, IObject* object, double);
    virtual void  reset();   // +0x19C; round-scoped reset.
    virtual void  setFiringState(bool);   // +0x1A0 43 B
    virtual bool  getFiringState() const;   // +0x1A4 33 B
    virtual void  setAltFiringState(bool);   // +0x1A8 (byte +0x161)
    virtual bool  getAltFiringState() const;   // +0x1AC
    virtual void  setFlareFiringState(bool);   // +0x1B0 (byte +0x162)
    virtual bool  getFlareFiringState() const;   // +0x1B4
    virtual void  setSprintState(bool);   // +0x1B8 (byte +0x163)
    virtual bool  getSprintState() const;   // +0x1BC
    virtual void  setSprintStateLastTick(bool);   // +0x1C0 (byte +0x164)
    virtual bool  getSprintStateLastTick() const;   // +0x1C4
    virtual void  setReloadState();   // +0x1C8 (byte +0x244 = 1)
    virtual bool  getAndResetReloadState();   // +0x1CC 25 B - read, then clear
    virtual void  updateFiringState(unsigned int, IObject*);   // +0x1D0; refreshes each weapon slot.
    virtual void  updateGhostFiringState();   // +0x1D4 377 B [UNRESOLVED return]
    virtual void  setHitIndicator(bool);   // +0x1D8 32 B - also fades +0x25C to 1.0
    virtual bool  getHitIndicator() const;   // +0x1DC; consumes the hit-indicator latch.
    // [PLATFORM] updateHitFromDirection occupies ELF +0x1E0 and PE +0x1DC; returns float.
    virtual float updateHitFromDirection(const Vec3& direction);
    virtual float getHitFromDirection() const;   // +0x1E4 (float +0x188)
    // [PLATFORM] Replaces camera shake and resets its state to 2; does not accumulate.
    virtual void  updateCameraShake(float amount);
    virtual float getCameraShake() const;   // +0x1EC (float +0x190)
    virtual unsigned int getSquadInviteMask() const;   // +0x1F0 (uint +0x11C)
    virtual void  setSquadInviteMask(unsigned int mask);   // +0x1F4
    virtual bool  getIsInvitedToSquad(int squad) const;   // +0x1F8 30 B - mask bit test
    virtual void  setKitTimeStamp(double t);   // +0x1FC (double +0x4C)
    virtual double getKitTimeStamp() const;   // +0x200
    // [PLATFORM] ELF slots +0x204/+0x208; PE +0x200/+0x204 (one destructor slot fewer).
    virtual void  setCurrentFiringButton(io::PlayerInputMap button);
    virtual io::PlayerInputMap getCurrentFiringButton() const;
    virtual void  setPersonalDecalTemplate(const IObjectTemplate* decal);   // +0x20C (Player +0x200)
    virtual const IObjectTemplate* getPersonalDecalTemplate() const;   // +0x210
    virtual void  enableVOHelp(bool);   // +0x214   - empty, as shipped
    virtual void  enableVOHelpMessage(int);   // +0x218   - empty
    virtual void  disableVOHelpMessage(int);   // +0x21C   - empty
    virtual bool  getVOHelpMessagePlayed(int);   // +0x220   - returns true
    virtual void  doAddTouchItem(IPlayer* other);   //  / PE 0x472A90: guarded nametag relay
    virtual void  setHealRepairedAtTick(unsigned int tick, bool giving, bool repairing);   // +0x228 35 B
    virtual unsigned int getHealRepairedAtTick() const;   // +0x22C (uint +0x128)
    virtual void  setGaveAmmoAtTick(unsigned int tick, bool giving);   // +0x230 26 B
    virtual unsigned int getGaveAmmoAtTick() const;   // +0x234 (uint +0x12C)
    virtual void  setFailedExitAtTick(unsigned int tick);   // +0x238 (uint +0x130)
    virtual unsigned int getFailedExitAtTick() const;   // +0x23C
    virtual bool  getIsGivingAbility() const;   // +0x240 (byte +0x134)
    virtual bool  getIsRepairing() const;   // +0x244 (byte +0x135)
    virtual bool  getIsGivingReplenish() const;   // +0x248 (byte +0x136)
    virtual int   getChosenUnlockLevel(int kit, int slot) const;   // +0x24C
    virtual void  setChosenUnlockLevel(int kit, int slot, int level);   // +0x250
    virtual bool  isUnlocked(int kit, int slot, int level);   //  / PE 0x478FE0, profile + kit limits
    virtual int   getUnlockLevel(int kit, int slot);   // +0x258
    virtual void  setUnlocks(std::map<int, int>* unlocks);   // +0x25C 67 B - copies into +0x278
    virtual std::map<int, int>* getUnlocks();   // +0x260 (Player +0x278)
    virtual void  setPeerAddress(const std::string& address);   // +0x264 -> std::string::assign (+0x2AC)
    virtual const std::string& getPeerAddress() const;   // +0x268
    virtual bool  getVoipEnabled();   // +0x26C (byte +0x2B0)
    virtual unsigned short getVoipSessionId();   // +0x270 (word +0x2B2)
    virtual void  setVoipSessionId(unsigned short id);   // +0x274
    virtual bool  getIsMuted();   // +0x278 (byte +0x2B4)
    virtual bool  getIsTalking();   // +0x27C 55 B - against the wall clock
    virtual double getLastTalkTime();   // +0x280 (double +0x2B8)
    virtual double getStopTalkingThreshold();   // +0x284 (double +0x2C0)
    virtual void  setIsMuted(bool);   // +0x288 51 B
    virtual void  setIsTalking(bool);   // +0x28C 50 B
    virtual void  setVoipEnabled(bool);   // +0x290 188 B
    virtual void  setVotingStatus(PlayerVotingStatus status);   // +0x294 (dword +0x2C8)
    virtual PlayerVotingStatus getVotingStatus();   // +0x298
    virtual void  setParticipateInVoting(bool);   // +0x29C (byte +0x2CC)
    virtual bool  getParticipateInVoting();   // +0x2A0
    virtual void  setTargetOfVote(bool);   // +0x2A4 (byte +0x2CD)
    virtual bool  getTargetOfVote();   // +0x2A8
    virtual int   getProfileId();   // +0x2AC (int +0x2D0)
    virtual void  setProfileId(int id);   // +0x2B0
    virtual void  setBlurryVision(float, float, float);   // +0x2B4 471 B
    virtual float getBlurValue() const;   // +0x2B8 (float +0x300)
    virtual bool  getIsNearGasCloud() const;   // +0x2BC (byte +0x305)
    virtual bool  getIsGasMaskOn() const;   // +0x2C0 (byte +0x304)
    virtual void  setIsGasMaskOn(bool, bool);   // +0x2C4 850 B
    virtual void  setGasMaskHudItem(bool);   // +0x2C8 (Player: empty)
    virtual float getPlayerGasMaskChangeTimer() const;   // +0x2CC (float +0x308)
    virtual float getPlayerNightVisionChangeTimer() const;   // +0x2D0 (float +0x30C)
    virtual bool  getIsUnderwater() const;   // +0x2D4 (Player: false)
    virtual float getNormalizedNightVisionPower();   // +0x2D8 (Player: 1.0f)
    virtual void  setNightVision(bool, bool);   // +0x2DC 138 B
    virtual void  rechargeNightVision();   // +0x2E0 (Player: empty)
    virtual void  setVisualNightVision(bool);   // +0x2E4 1,000 B
    virtual bool  getNightVision() const;   // +0x2E8 (byte +0x320)
    virtual void  disableFlashBangEffect();   // +0x2EC 59 B
    virtual void  disableBlurryVision();   // +0x2F0 378 B
    virtual io::PlayerInput getPlayerInput();   //  / PE 0x47C3D0: BY value, native aggregate copy
    virtual void  resetOnDeath();   // +0x2F8 100 B
    virtual bool  isPlayerInNightMap() const;   // +0x2FC (Player: false)
    // whatever this stores, THE next setIsAlive erases - either argument. See the cluster note.
    virtual void  setTracePlayerId(int id);   // +0x300 (int +0x314)
    virtual int   getTracePlayerId();   // +0x304   NOT const in the ELF either
    virtual void  allocateFreeCamera();   // +0x308 918 B
    virtual bool  getIsThisASpam(bool);   // +0x30C 233 B
    virtual void  setRadioSpamFlagCount(int);   // +0x310
    virtual int   getRadioSpamFlagCount();   // +0x314
    virtual void  setRadioSpamBlockedFlagCount(int);   // +0x318
    virtual int   getRadioSpamBlockedFlagCount();   // +0x31C
    virtual void  setRadioSpamFilterFlagTimer(float);   // +0x320
    virtual float getRadioSpamFilterFlagTimer();   // +0x324
    virtual void  resetSpamFilter();   // +0x328
    virtual void  getCloseMines(std::vector<IObject*>& out) const;   // +0x32C  / PE 479250: append live targets
    virtual bool  getAutoReload() const;   // +0x330 (byte +0x318)
    virtual void  setAutoReload(bool);   // +0x334
    virtual void  updateClientSettings();   // +0x338  (empty) / BF2.exe 0x4FA820: m_autoReload <- GeneralSettings

    // [CHOSEN, not recovered] Player implements the replication overrides directly here.
    // The original uses a PlayerNetworkable table on its networkable subobject; preserve
    // that interface behavior until the separate class is represented.
    virtual bool init(void*);                                         // +0x18
    virtual void updateStateMask(unsigned int*);                      // +0x2C
    virtual bool getNetUpdate(io::BitStream&, io::BaseLineData*,
                              unsigned int stateMask, int precision); // +0x30
    virtual void setNetUpdate(io::BitStream&, float updateTime, bool ghostActive,
                              io::BaseLineData*, int precision);      // vptr +0x34 [FRONTIER]

    // io::IConsoleSaveable: 1 slots

    virtual bool makeScript(IStream*);

    // Replication budgets use a 48-bit minimum. moveTo is server-only, requires a
    // vehicle, and returns an empty string on success or a refusal message.
    virtual unsigned int getMinimumBitSize() const;   // +0x350   - `return 48`
    virtual std::string  moveTo(const Vec3& position);

    // Updates explosions, server-side abilities/mines/commander state, VOIP, radio
    // limits, and client team verification once per fixed step. Mine proximity runs
    // on its own 0.2-second cadence while alive.
    void update(float deltaTime);
    // Alive-player updates: kit abilities and nearby mines. Mine checks run at 5 Hz.
    void updateAbility(float deltaTime);
    void updateIsCloseToMine(float deltaTime);
    // [CHOSEN] Shared emitter for mine events 57 and 58.
    void postMineEvent(unsigned int eventId);

    // Clears nearby mines and emits the transition notification when the player dies.
    void emptyMineMap();
    void updateCommanderStatus();
    // Switches vehicle state; null enters the free camera. Returns true.
    bool _setVehicle(world::IObject* vehicle, int seat);

    // Rebuilds weak references to vehicle parts and stamps their networkable owner IDs
    // after a successful vehicle change.
    void clearObjectList();
    void buildObjectList(world::IObject* object, int seat);

    // Values used to initialize replicated player state.
    unsigned char getKitAndUnlockLevelNetworkValue() const;
    int           getMaskSetting() const;
    // Applies mask state and resets the gas-mask/night-vision change timers.
    void          setMaskSetting(int setting);

    // Updates the teammate medic-request indicator and its timestamp on clients.
    // Dedicated images have no corresponding UI effect.
    void          setCallForMedic(bool state);

    // returns AN IPlayerControlObject*, NOT AN IObject* - it hands back queryInterface's own
    // result. The name says "vehicle" but the thing returned is the control-object interface of
    // whichever vehicle in the parent chain actually carries weapons. See the body.
    world::IPlayerControlObject* getWeaponVehicle();

    // Applies decoded remote weapon ammunition state to the player's live weapons.
    void          forceSetRemoteClientAmmo(unsigned int firedMask);

    // The position captured when a tear-gas exposure starts (see m_tearGasPosition).
    // [CHOSEN, not recovered] Neither image has this accessor: the retail AI image reads the three
    // floats at raw x86 offsets +0x2E8/+0x2EC/+0x2F0 of the world::IPlayer it gets from
    // IAIPlayer::getPlayer (BBPAvoidCollisionInfantery::createPlan, libaidll 0x22E304..0x22E310,
    // three `flds`). The inline accessor gives the native AI image the same field without copying
    // x86 offsets.
    const Vec3&   getTearGasPosition() const { return m_tearGasPosition; }

private:
    // Non-vtable Player helpers, all attested by their ELF manglings and direct callers. Keeping
    // these private avoids inventing a wider gameplay API for implementation details.
    void forceSetClientAmmo(unsigned int slotMask);
    void updateWeaponLimits(world::IObject* object);
    void setSquadId(unsigned short id);
    void setIsCommander(int commander);
    void setIsSquadLeader(int leader);
    bool hasGasCloudImmunity() const;
    void handleGasClouds(float deltaTime);
    void handleBlurryVision(float deltaTime);
    void handleNightVision(float deltaTime);
    void setNightVisionHudItem(bool enabled);                  // exact empty body
    void setNightVisionMaskOverlay(bool enabled);              // exact empty body
    // Non-virtual statistics reset used by round reset.
    void resetStats();
    // Non-virtual flag combinator; returns the updated word without accessing player state.
    unsigned int updateFlags(unsigned int word, const Flag32Bits<IPlayer>& flags);
                                                            // Narrow friendship permits direct state updates that have no public setter,
                                                            // including the server's out-of-bounds timer and game-logic kill latch.
    friend class ServerGameLogic;
    friend class GameLogic;                // killPlayer's +0xD8 kill-latch store
    friend class world::GameLogicDamage;   // the one direct store into +0x264 (0x80E11EB), see below
    friend class world::GrapplingHookRope; // detachSoldier's attested +0xE4 climb cooldown write
    // Zipline::detachSoldier writes the same raw world-time cooldown in both shipped ports
    // (ELF 0x81D0680 / PE 0x57CF30). Narrow friend, no setter.
    friend class world::Zipline;

protected:

    // Offsets describe the 32-bit ELF layout; compilers may lay out the bases
    // differently. Preserve member order, types, and initial values. Positional names
    // mark fields whose semantic names remain unresolved.
    /* +0x48  */ int    m_refs;                      // 1 - NOT 0. addRef/getRef/release ride on this
    // One double timestamp, including both recorded x86 words.
    /* +0x4C  */ double m_kitTimestamp;              // 0.0  (occupies +0x4C..+0x53)
    /* +0x54  */ int    m_kit;                       // 0    IPlayer.h: setKit -> (int +0x54)
    /* +0x58  */ int    m_flags;                     // 0    IPlayer.h: setFlags -> Player +0x58
    /* +0x5C  */ std::string m_playerName;           // IPlayer.h: getName -> (Player +0x5C)
    /* +0x60  */ int    m_field60;                   // 0
    // Vehicle weak control block. Recorded offsets describe retail x86; native x64
    // places its target pointer at +8 rather than +4.
    /* +0x64  */ void*  m_vehicleWeak;               // 0 weak block: [+0] refcount, [+4] target
    // Camera pointer and free-camera weak block are distinct from the vehicle weak reference.
    /* +0x68  */ world::ICameraObject* m_camera;     // 0 setCamera's target
    /* +0x6C  */ void*  m_freeCameraWeak;            // 0 a weak block; its target (+4) is the free
                                                     //   camera. getCamera and getFreeCamera both
                                                     //   dereference it as `block ? block[+4] : 0`.
    /* +0x70  */ bool   m_freeCameraEnabled;         // 1 true by default
    /* +0x71  */ bool   m_overheadCameraEnabled;     // 0
    /* +0x72  */ bool   m_overheadCameraTopDownView; // 0
    /* +0x73  */ bool   m_commanderCamOverhead;      // 0
    /* +0x74  */ void*  m_field74;                   // 0 was m_freeCameraObject - that is the
                                                     //   +0x6C block's target, not this
    /* +0x78  */ int    m_field78;                   // 0
    /* +0x7C  */ int    m_field7C;                   // 0
    /* +0x80  */ int    m_field80;                   // 0
    /* +0x98  */ int    m_inputId;                   // 1 sentinel, not 0
    /* +0x9C  */ int    m_playerId;                  // the ctor argument. IPlayer.h: getId +0x9C
    /* +0xA0  */ int    m_updateFrequencyType;       // 0    IPlayer.h: getUpdateFrequencyType +0xA0
    // FOV target, current value, and transition rate in FOV units per second.
    /* +0xA4  */ float  m_endFov;                    // 0.0f  the value being interpolated TO
    /* +0xA8  */ float  m_fov;                       // 0.0f  what getFov() returns
    /* +0xAC  */ float  m_fovRate;                   // 0.0f  FOV units per second
    // Counted weak-control block for the default vehicle, with refcount and target pointer.
    /* +0xB0  */ void*  m_defaultVehicleWeak;        // 0 weak block: [+0] refcount, [+4] target
    /* +0xB4  */ int    m_fieldB4;                   // 1 sentinel
    /* +0xB8  */ bool   m_isAI;                      // 0    Player +0xB8 per IPlayer.h getIsAIPlayer
    /* +0xB9  */ bool   m_isAlive;                   // 0
    /* +0xBA  */ bool   m_suicide;                   // 0
    /* +0xBC  */ int    m_team;                      // 1 sentinel; name confirmed by init()
    // [CHOSEN] Team-verification state. setTeam rearms it; update sends at most three requests.
    /* +0xC0  */ int    m_verifyTeamSendCount;       // 0
    /* +0xC4  */ bool   m_verifyTeamPending;         // 0
    /* +0xC8  */ int    m_spawnAtTick;               // 0   name confirmed by init()
    /* +0xCC  */ bool   m_isManDown;                 // 0   name confirmed by init()
    /* +0xD0  */ int    m_manDownAtTick;             // 0
    /* +0xD4  */ int    m_spawnGroup;                // 0    IPlayer.h: setSpawnGroup -> (int +0xD4)
    /* +0xD8  */ bool   m_fieldD8;                   // Kill latch; set by GameLogic::killPlayer. Reader remains [UNRESOLVED].
    /* +0xDC  */ unsigned int m_ping;                // 0 confirmed; init() sends only the low word
    /* +0xE0  */ unsigned int m_interpolationTime;   // 0    IPlayer.h: +0xE0
    /* +0xE4  */ float  m_worldTime;                 // 99999.0f sentinel
    /* +0xE8  */ bool   m_fieldE8;                   // 0
    /* +0xEC  */ world::PlayerScoreData* m_scoreData;// new PlayerScoreData - operator new(0x60)
    /* +0xF0  */ int    m_squadId;                   // 0
    /* +0xF4  */ bool   m_isCommander;               // 0 named by init() (slot +0x3D)
    /* +0xF5  */ bool   m_isSquadLeader;             // 0 named by init() (slot +0x3C)
    /* +0xF6  */ bool   m_uavIsReady;                // 0
    /* +0xF7  */ bool   m_satelliteIsReady;          // 0
    // Commander-tool availability and armor values, copied by updateCommanderStatus.
    // These fields are intentionally uninitialized until the commander update runs;
    // integer members carry the armor floats' bit patterns.
    /* +0xF8  */ bool   m_fieldF8;                   // NOT initialised by the constructor
    /* +0xF9  */ bool   m_fieldF9;                   // ditto
    /* +0xFA  */ bool   m_fieldFA;                   // ditto
    /* +0xFC  */ int    m_fieldFC;                   // ditto
    /* +0x100 */ int    m_field100;                  // ditto
    /* +0x104 */ int    m_field104;                  // ditto
    /* +0x108 */ bool   m_isInsideCP;                // 0
    /* +0x10C */ double m_lastTimeCommander;         // 99999.0 A double, and a sentinel
    // Commander reapply delay is one double, not two integer fields.
    /* +0x114 */ double m_commanderReapplyWaitTime;  // 0.0
    /* +0x11C */ unsigned int m_squadInviteMask;     // Only the low eight bits are replicated.
    /* +0x120 */ int    m_field120;                  // 0
    // Whether the player currently holds a flag.
    /* +0x124 */ bool   m_flagHolder;                // 0 confirmed three ways
    /* +0x128 */ unsigned int m_healRepairedAtTick;  // Initially zero.
    /* +0x12C */ unsigned int m_gaveAmmoAtTick;      // Initially zero.
    /* +0x130 */ unsigned int m_failedExitAtTick;    // Initially zero.
    /* +0x134 */ bool   m_isGivingAbility;           // 0 was m_isSquadLeader - that is +0xF5
    /* +0x135 */ bool   m_isRepairing;               // 0
    /* +0x136 */ bool   m_isGivingReplenish;         // 0
    // Nearby mines are tracked by weak object handle. The separate countdown refreshes
    // proximity every 0.2 seconds; following fields hold radio-spam state.
    /* +0x138 */ std::map<WeakPtr<world::IObject>, ItemStatus> m_closeMines;   // +0x138 header ptr,
                                                     //   +0x13C node count, +0x140 the comparator
    /* +0x144 */ float  m_closeMineCheckTimer;       // 0.0f  [CHOSEN] - update()'s 0.2 s cadence
    /* +0x148 */ int    m_radioSpamFlagCount;        // 0     IPlayer.h records (int +0x148)
    /* +0x14C */ int    m_radioSpamBlockedFlagCount; // 0     IPlayer.h records (int +0x14C)
    /* +0x150 */ float  m_radioSpamFilterFlagTimer;  // 1.0f IPlayer.h records (float +0x150)
    // Weak handles to the current vehicle and its replicating parts. Rebuilt on
    // vehicle changes; targets may disappear between rebuilds.
    /* +0x154 */ std::list<WeakPtr<world::IObject> > m_objectList;   // hand-built head
                                                     //   (12 B, self-linked) in the ctor
    /* +0x158 */ ActionBuffer* m_actionBuffer;       // new ActionBuffer. IPlayer.h: +0x158
    // Owned bot input. A player has either this or a human ActionBuffer; switching AI state replaces the other.
    /* +0x15C */ io::PlayerInput* m_aiPlayerInput;   // 0
    // Replicated state bytes: firing trio, sprint pair, remote flag, simulated-frame
    // flag, and one unresolved state byte.
    /* +0x160 */ bool   m_field160[8];               // 0 x8; [0..2] firing trio, [3] sprint,
                                                     // [4] sprintLastTick, [7] read by init()
    /* +0x168 */ double m_field168;                  // 1.0 sentinel
    /* +0x170 */ bool   m_field170;                  // 0
    /* +0x174 */ double m_field174;                  // 1.0 sentinel
    /* +0x17C */ bool   m_isConnected;               // 0
    /* +0x180 */ double m_timeConnected;             // 1.0 sentinel
    // Hit direction is quantized as an angle and sent twice after update.
    // [PLATFORM] Preserve angle/counter order; compiler layout determines their offsets.
    /* +0x188 */ float  m_hitFromDirection;          // 0
    /* +0x18C */ int    m_someHitCounter;            // 0 - two-send hit-direction counter
    /* +0x190 */ float  m_cameraShake;               // Replicated as min(value,25)/25 in a byte.
    /* +0x194 */ int    m_field194;                  // 0 - the camera-shake send counter
                                                     // [CHOSEN name] Per-weapon-slot active flags.
    /* +0x198 */ bool   m_weaponSlotActive[3];       // 0 x3
    /* +0x19B */ // one pad byte to the dword arrays
    // Three weapon slots carry ammunition, magazine counts, and heat.
    // [UNRESOLVED] m_array1B4, m_array1C0, and m_array1F0 synchronize two-bit fields.
    // [INFERRED] magSize/maxMag names; [CHOSEN] heat-field names.
    /* +0x19C */ int    m_ammoCountWeap1[3];
    /* +0x1A8 */ int    m_magSizeWeap1[3];
    /* +0x1B4 */ int    m_array1B4[3];
    /* +0x1C0 */ int    m_array1C0[3];
    /* +0x1CC */ int    m_currNbMagsWeap1[3];
    /* +0x1D8 */ int    m_maxNrOfMagsWeap1[3];
    /* +0x1E4 */ float  m_weaponHeat[3];
    /* +0x1F0 */ int    m_array1F0[3];
    /* +0x1FC */ io::PlayerInputMap m_currentFiringButton; // io::PINone (64) NOT 0
    // Unlockable vehicle tag; null selects decal_defaultPlayerTag.
    /* +0x200 */ const world::IObjectTemplate* m_personalDecalTemplate;   // 0
    /* +0x204 */ bool   m_field204;                  // 0
    // Counts rising edges of primary fire, not frames spent firing; replicated to clients.
    /* +0x208 */ int    m_firePressCount;            // 0  [CHOSEN name]
    // Cached firing state for three weapon slots, refreshed during the player tick.
    // Each slot is cleared when its weapon is unavailable or fails the update predicates.
    // The byte and float arrays retain their distinct element widths.
    /* +0x20C */ int           m_ghostFire20C[3];    // init(): 12 B zeroed   <- IWeaponObject +0x94(0)
    /* +0x218 */ int           m_ghostFire218[3];    // init(): 12 B zeroed   <- IWeaponObject +0x14
    /* +0x224 */ unsigned char m_ghostFire224[3];    // init(): 3 B zeroed    <- IWeaponObject +0xA0 > 1
    /* +0x228 */ float         m_ghostFire228[3];    // init(): 12 B zeroed   <- IWeaponObject +0x24
    /* +0x234 */ int           m_ghostFire234[3];    // init(): 12 B zeroed   <- IWeaponObject +0x11C
    /* +0x240 */ NetUpdateSlot* m_netUpdateBuffer;    // 0 in the ctor; init() allocates kNetUpdateSlots
    /* +0x244 */ bool   m_reloadState;               // Consumed by getAndResetReloadState.
    /* +0x248 */ int    m_tickCount;                 // 0  get/add/sub/resetTickCounter
    /* +0x24C */ float  m_timeOutSideWorld;          // 0    IPlayer.h: getTimeOutSideWorld +0x24C
    /* +0x250 */ int    m_field250;                  // 0
    /* +0x254 */ int    m_field254;                  // 0
    /* +0x258 */ int    m_field258;                  // 0
    /* +0x25C */ float  m_crossHairFadeValue;        // 0; getCrossHairFadeValue / frame decay
    // Mutable because getHitIndicator consumes and clears the latch.
    /* +0x260 */ mutable unsigned char m_hitIndicator;   // 0; 3 pad bytes to +0x264
    /* +0x264 */ int    m_field264;                  // Punish/forgive record: killer ID + 256 on punishable team-kill; starts at 1. Reader [UNRESOLVED].
    /* +0x268 */ int    m_field268;                  // 0
    /* +0x26C */ int    m_field26C;                  // 0
    /* +0x270 */ io::INetworkable* m_networkable;    // IPlayer.h: getNetworkable -> (Player +0x270)
    /* +0x274 */ PlayerVOHelp* m_voHelp;            // 0 on dedicated; client PE +0x2A4
    // Four contiguous unlock maps form two logical pairs; setUnlocks replaces only the first pair.
    // getChosenUnlockLevel starts at the second map and can cross that logical pair boundary.
    // Keep the contiguous declaration required by that indexing.
    /* +0x278 */ std::map<int,int> m_unlockMaps[4];  // +0x278 / +0x284 / +0x290 / +0x29C
    /* +0x2A8 */ bool   m_field2A8;                  // 0
    /* +0x2AC */ std::string m_peerAddress;          // "N/A" NOT empty
    /* +0x2B0 */ bool   m_voipEnabled;               // 0
    /* +0x2B2 */ unsigned short m_voipSessionId;     // 0 getVoipSessionId, "(word +0x2B2)"
    /* +0x2B4 */ bool   m_isMuted;                   // 0
    /* +0x2B8 */ double m_lastTalkTime;              // 0.0
    /* +0x2C0 */ double m_stopTalkingThreshold;      // 0.5 NOT 0
    /* +0x2C8 */ int    m_votingStatus;              // 0 setVotingStatus, "(dword +0x2C8)"
    /* +0x2CC */ bool   m_participateInVoting;       // 1 true by default
    /* +0x2CD */ bool   m_targetOfVote;              // 0
    /* +0x2D0 */ int    m_field2D0;                  // 0 was m_votingStatus - that is +0x2C8
    /* +0x2D4 */ bool   m_checkVoip;                 // 1 true by default
    /* +0x2D8 */ int    m_profileId;                 // 0
    /* +0x2E0 */ float  m_field2E0;                  // 1.0f NOT 0
    /* +0x2E4 */ float  m_field2E4;                  // 0; gas damage per second [UNRESOLVED name]
    // +0x2E8..+0x2F0: one Vec3, zeroed by the constructor; setBlurryVision (bf2 0x80F6890,
    // 0x80F691D..0x80F693A) copies the three words of the default vehicle's getAbsolutePosition
    // (+0x6C) through a base register at +0x2E8 when a gas exposure starts; the AI image reads
    // them as floats. No other writer in the bf2 image. [INFERRED] name.
    /* +0x2E8 */ Vec3   m_tearGasPosition;           // (0, 0, 0)
    /* +0x2F4 */ float  m_field2F4;                  // 0; blur fade countdown [UNRESOLVED name]
    /* +0x2F8 */ float  m_field2F8;                  // 1.0f NOT 0
    /* +0x2FC */ float  m_gasDamage;                 // 0
    /* +0x300 */ float  m_blurValue;                 // 0
    /* +0x304 */ bool   m_isGasMaskOn;               // 0
    /* +0x305 */ bool   m_isNearGasCloud;            // 0
    // Resolved together from handleFrameUpdate and the two named timer accessors. The earlier
    // names were shifted by one field: +0x308 is gas-mask, +0x30C is night-vision, while +0x310 is
    // a separate visual-NV transition countdown written by setVisualNightVision.
    /* +0x308 */ float  m_playerGasMaskChangeTimer;       // 0
    /* +0x30C */ float  m_playerNightVisionChangeTimer;   // 0
    /* +0x310 */ float  m_nightVisionVisualTransitionTimer; // 0 [CHOSEN role/name]
    /* +0x314 */ int    m_tracePlayerId;             // 1 sentinel
    /* +0x318 */ bool   m_autoReload;                // 0
    /* +0x31C */ float  m_field31C;                  // 0
    /* +0x320 */ bool   m_nightVision;               // 0
    /* +0x321 */ bool   m_byte321;                   // 0
    /* +0x324 */ float  m_field324;                  // 9999.0f NOT 0
    /* +0x328 */ bool   m_byte328;                   // 0
    /* +0x329 */ bool   m_byte329;                   // 0
    /* +0x32A */ bool   m_byte32A;                   // 1 true by default
    /* +0x32B */ bool   m_byte32B;                   // 1 true by default
    /* +0x32C */ bool   m_byte32C;                   // 0
    /* +0x32D */ bool   m_byte32D;                   // 0
    /* +0x32E */ bool   m_byte32E;                   // 0
    /* +0x32F */ bool   m_isInTearGas;               // 0
    /* +0x330 */ bool   m_byte330;                   // 0
    /* +0x331 */ bool   m_byte331;                   // 0
    /* +0x334 */ world::CPersistentExplosionEffectManager* m_explosionEffects;   // the ctor argument
    // Private copy of the last supplied input, independent of later caller changes.
    /* +0x338 */ io::PlayerInput m_playerInput;      // 0x338..0x44B - the whole tail of the object
};


// Returns its argument unchanged.
world::IPlayer* getPlayer(world::IPlayer* player);

} // namespace hfe
} // namespace dice

#endif // GAME_GAMELOGIC_PLAYER_H
