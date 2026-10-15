// Game player state, input and replication; distinct from world::Player.
// Path attested by __FILE__; recovery uses the symbolized dedicated ELF.
// Client-only update work remains marked at its use sites.
// Historical tracking: [RETIRE-WHEN: player-updatestatemask]

#include "Player.h"

#include "../../Core/Debug.h"
#include "../../Core/SettingsRepostitory.h"
#include "../../IO/Stream/IStream.h"
#include "ClientGameLogic.h"                   // getAutoInput's runtime !isServer() arm
#include "../Main/Game.h"                      // isDedicated() - updateClientSettings' client arm
#include "../GeneralSettings.h"                // IGeneralSettings::getAutoReload - updateClientSettings
#include "../../Core/ClassManager.h"
#include "../../Core/EventManager.h"
#include "../../AI/AIInterface/AIInstance.h"      // ai::g_aiInstance - see the ctor's tail
#include "../../IO/Network/INetworkable.h"
#include "../../Core/Ids.h"                 // IID_ICameraObject / IID_IPlayerControlObject
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // getActiveWeapon /
                                                                        // getWeapons - see
                                                                        // getWeaponVehicle
#include "../Objects/IObject.h"             // queryInterface + the +0x28 parent link
#include "../Objects/IWeaponObject.h"       // typed firearm state reads
#include "../Objects/Projectiles/GenericProjectile.h" // typed mine-warning component reads
#include "../Objects/PlayerControlObjectTemplate.h" // hasGasCloudImmunity's template byte
#include "../../IO/BitStream.h"             // getStateSync's wire
#include "../Objects/GameLogicComp/ArmorGLComp.h"   // the health byte - getStateSync
#include "../../Math/Mat4.h"                // getIdentity / setRotationZXY - allocateFreeCamera
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // getTemplate
#include "../../IO/Network/NetworkManager.h"  // addNetworkable; StatePrecisionGhost - getNetUpdate
#include "../../IO/Network/NetworkStats.h"    // the TU's RegistredNetworkStats("Player")
#include "../../IO/Network/INetworkableObject.h"  // setOwnerID - the object-list pair
#include "../../Core/System.h"                 // g_system - setIsConnected
#include "../../IO/Console/Console.h"          // setVisualNightVision's normal.con switch
#include "../../Sound/BfSound.h"               // handleBlurryVision's retail sound state
#include "../SingletonServer.h"                // m_iNametagManager - setTeam
#include "../Common/ClientVoteManager.h"       // setTeam's local-human branch
#include "../IGameLogic.h"                    // getBeforeSpawnCamera{Position,Direction} +0x20C/+0x210
#include "../Objects/Game/KitTemplate.h"      // getUnlockLevel reads m_creationIndex raw (friend)
#include "../../Scene/Object/PlayerManager.h" // getLocalHumanPlayer - getCommanderCameraForPlayer
#include "../../Geom/ParticleSystem/ParticleSystemManager.h" // typed gas-cloud manager/instance ABI

#include <cfloat>    // FLT_EPSILON - updateHitFromDirection's Vec3 normalisation
#include <cmath>     // floor - init(); ceil - getStateSync's health byte
#include <cstdio>    // targeted runtime probes elsewhere in this TU
#include <cstdlib>
#include <cstring>   // memset - init()
#include <vector>    // getWeapons() points at a std::vector - see getWeaponVehicle
#include "../Objects/Fx/PersistentExplosionEffectManager.h"  // update()'s first statement
#include "../Objects/FreeCameraTemplate.h"     // allocateFreeCamera builds the camera
#include "../Objects/ObjectLayoutAccess.h"     // parent() - _setVehicle walks to the control object
#include "../Objects/ICameraObject.h"          // _setVehicle adopts the vehicle's active camera
#include "../../IO/Input/PlayerInput.h"        // setIsAIPlayer allocates one (0x114 B)
#include "../GameServer/ServerInfo.h"          // the radio spam-filter settings - update()
#include "../IGame.h"                          // game->sendGameEvent / sendGameEventIfPlaying
#include "../Common/GameEvent.h"               // PostRemoteEvent - emptyMineMap
#include "../GameEvents/VoipOnOffEvent.h"       // setVoipEnabled's client-only relay
#include "../GameEvents/SetNightVisionEvent.h"  // handleNightVision's client relay
#include "../GameEvents/VerifyPlayerTeamEvent.h"
#include "../GameEvents/PlayerTearGassedEvent.h"
#include "../GameServer/VoipServerConnection.h" // squad/commander changes refresh voice routing
#include "SquadManager/ISquadManager.h"         // updateOrderMarkers / commander camera mode
#include "../Objects/Game/CommanderToolsManager.h" // updateCommanderStatus's per-team state

#include "../../Scene/Object/PlayerScoreData.h"   // sizeof 0x60 - the ctor allocates one
#include <list>                                   // updateAbility's skip list
#include <set>                                    // updateAbility's "already served" set
#include "../../Scene/Object/ObjectManager.h"     // objectManager + getObjectsFromGrid
#include "../../Scene/Object/ObjectPredicator.h"  // PCOPredicator / AbilityPredicator
#include "../Objects/Game/Soldier.h"              // getKitType / getKit / getOccupingPlayer
#include "../Objects/Game/SoldierTemplate.h"      // gas-mask sprint factor in setIsGasMaskOn
#include "../Objects/Game/Kit.h"                  // the kit's template
#include "../Objects/GameLogicComp/IArmorGLComp.h"   // getDefaultMaterial
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "ActionBuffer.h"                         // sizeof 0x2C - ditto

namespace dice {
namespace hfe {

// [INFERRED] Client voice-over tutorial state owned by Player. Dedicated players leave it null.
// The constructor and help/update behavior remain declaration-only; the trailing pairs store
// message intervals and countdowns.
struct PlayerVOHelp
{
    PlayerVOHelp(); // [FRONTIER] client-only constructor, PE 0x5ED220; not published by this change.
    ~PlayerVOHelp() = default; //  / server PE 0x47A100 / client PE 0x504720, inlined.

    bool played[82];
    bool disabled[82];
    int minimumExperience[82];
    bool messageActive;
    float messageDuration;
    float messageElapsed;
    float nextMessageDelay;
    bool enabled;
    bool inVehicle;
    std::string labels[82]; // +0x200 on both ports; let the host STL determine stride.
    struct Timer { float interval, remaining; } timers[82];
};

// ds:0x8ACBFA4 - Core/Globals.cpp owns it, declared IBase* with no layout. Same
// declaration GameLogic.cpp:1028 uses; the two must stay identical or MSVC mangles two
// different symbols.
extern IBase* squadManager;
extern ClassManager* classManager;

// Core/Globals.cpp. Same declarations the rest of the tree uses - they must match exactly or
// MSVC mangles separate symbols.
class IGame;
extern IGame*      game;
extern IGameLogic* gameLogic;

// Both shipped ports expose these as two distinct writable namespace globals. They intentionally
// remain distinct even though retail initializes both to 0.95f.
float MAX_MASK_TIMEOUT_OFF_TIME = 0.95f;
float MAX_MASK_TIMEOUT_ON_TIME  = 0.95f;

// Initialize player state, owned action/score helpers, and replication history. Preserve the
// nonzero sentinels and initial flags. The retail networkable subobject uses PlayerNetworkable's
// vtable; this C++ model provides the networkable overrides on Player instead.
Player::Player(int index, world::CPersistentExplosionEffectManager* explosionEffects)
    : m_refs(1)
    , m_kitTimestamp(0.0)
    , m_kit(0)
    , m_flags(0)
    // m_playerName: default-constructed - see the note above about unk_8990C4C
    , m_field60(0)
    , m_vehicleWeak(0)
    , m_camera(0)
    , m_freeCameraWeak(0)
    , m_freeCameraEnabled(true)
    , m_overheadCameraEnabled(false)
    , m_overheadCameraTopDownView(false)
    , m_commanderCamOverhead(false)
    , m_field74(0)
    , m_field78(0)
    , m_field7C(0)
    , m_field80(0)
    , m_inputId(-1)
    , m_playerId(index)
    , m_updateFrequencyType(0)
    , m_endFov(0.0f)
    , m_fov(0.0f)
    , m_fovRate(0.0f)
    , m_defaultVehicleWeak(0)
    , m_fieldB4(-1)
    , m_isAI(false)
    , m_isAlive(false)
    , m_suicide(false)
    , m_team(-1)
    , m_verifyTeamSendCount(0)
    , m_verifyTeamPending(false)
    , m_spawnAtTick(0)
    , m_isManDown(false)
    , m_manDownAtTick(0)
    , m_spawnGroup(0)
    , m_fieldD8(false)
    , m_ping(0)
    , m_interpolationTime(0)
    , m_worldTime(-99999.0f)
    , m_fieldE8(false)
    , m_scoreData(0)                            // set below - it is a heap allocation
    , m_squadId(0)
    , m_isCommander(false)
    , m_isSquadLeader(false)
    , m_uavIsReady(false)
    , m_satelliteIsReady(false)
    // +0xF8..+0x107 IS NOT initialised BY THE ELF's constructor and is not initialised here.
    // See the header: the six commander-toy mirror fields in that hole are written only by
    // updateCommanderStatus, and only for a commander. Reproduced as found.
    , m_isInsideCP(false)
    , m_lastTimeCommander(-99999.0)             // a double
    , m_commanderReapplyWaitTime(0.0)
    , m_squadInviteMask(0)
    , m_field120(0)
    , m_flagHolder(false)
    , m_healRepairedAtTick(0)
    , m_gaveAmmoAtTick(0)
    , m_failedExitAtTick(0)
    , m_isGivingAbility(false)
    , m_isRepairing(false)
    , m_isGivingReplenish(false)
    , m_closeMineCheckTimer(0.0f)               // +0x144 - NOT a spam counter, see the header
    , m_radioSpamFlagCount(0)                   // +0x148
    , m_radioSpamBlockedFlagCount(0)            // +0x14C
    , m_radioSpamFilterFlagTimer(-1.0f)         // +0x150
    , m_actionBuffer(0)                         // set below - it is a heap allocation
    , m_aiPlayerInput(0)
    , m_field168(-1.0)
    , m_field170(false)
    , m_field174(-1.0)
    , m_isConnected(false)
    , m_timeConnected(-1.0)
    , m_hitFromDirection(0.0f)
    , m_someHitCounter(0)
    , m_cameraShake(0.0f)              // +0x190 - inside the same zero run its neighbours transcribe
    , m_field194(0)
    , m_weaponSlotActive()                      // the +0x198 bool[3], zero-initialised

    , m_currentFiringButton(io::PINone)         // enum sentinel 64, not zero
    , m_personalDecalTemplate(0)
    , m_field204(false)
    , m_firePressCount(0)
    , m_netUpdateBuffer(0)                       // init() allocates the 4 x 188 B ring
    , m_reloadState(false)
    , m_tickCount(0)
    , m_timeOutSideWorld(0.0f)
    , m_field250(0)
    , m_field254(0)
    , m_field258(0)
    , m_crossHairFadeValue(0.0f)
    , m_hitIndicator(0)                         // +0x260 - the latch getHitIndicator consumes
    , m_field264(-1)
    , m_field268(0)
    , m_field26C(0)
    , m_networkable(0)                          // chosen below
    , m_voHelp(0)
    , m_field2A8(false)
    , m_peerAddress("N/A")                      // NOT empty
    , m_voipEnabled(false)
    , m_voipSessionId(0)
    , m_isMuted(false)
    , m_lastTalkTime(0.0)
    , m_stopTalkingThreshold(0.5)
    , m_votingStatus(0)
    , m_participateInVoting(true)
    , m_targetOfVote(false)
    , m_field2D0(0)
    , m_checkVoip(true)
    , m_profileId(0)
    , m_field2E0(1.0f)
    , m_field2E4(0.0f)
    , m_field2E8(0)
    , m_field2EC(0)
    , m_field2F0(0)
    , m_field2F4(0.0f)
    , m_field2F8(1.0f)
    , m_gasDamage(0.0f)
    , m_blurValue(0.0f)
    , m_isGasMaskOn(false)
    , m_isNearGasCloud(false)
    , m_playerGasMaskChangeTimer(0.0f)
    , m_playerNightVisionChangeTimer(0.0f)
    , m_nightVisionVisualTransitionTimer(0.0f)
    , m_tracePlayerId(-1)
    , m_autoReload(false)
    , m_field31C(0.0f)
    , m_nightVision(false)
    , m_byte321(false)
    , m_field324(9999.0f)
    , m_byte328(false)
    , m_byte329(false)
    , m_byte32A(true)
    , m_byte32B(true)
    , m_byte32C(false)
    , m_byte32D(false)
    , m_byte32E(false)
    , m_isInTearGas(false)
    , m_byte330(false)
    , m_byte331(false)
    , m_explosionEffects(explosionEffects)
{
    // Retail installs PlayerNetworkable's vtable on the networkable subobject here; this model
    // uses Player's overrides. Initialize the weapon arrays and input state separately.
    for (int i = 0; i < 3; ++i)
    {
        m_ammoCountWeap1[i] = 0; m_magSizeWeap1[i] = 0; m_array1B4[i] = 0; m_array1C0[i] = 0;
        m_currNbMagsWeap1[i] = 0; m_maxNrOfMagsWeap1[i] = 0; m_weaponHeat[i] = 0.0f; m_array1F0[i] = 0;
    }
    // Only the 64 input floats are zeroed; mapping flags and trailing bytes remain uninitialized.
    for (int i = 0; i <= 0x3F; ++i)
        m_playerInput.inputs[i] = 0.0f;

    for (int i = 0; i < 8; ++i)
        m_field160[i] = false;

    m_scoreData = new world::PlayerScoreData();     // operator new(0x60)

    // Publish the networkable base subobject. Its interface ID is distinct from the primary
    // IBase and IPlayer interfaces, so the retail interface-selection comparison chooses this arm.
    m_networkable = static_cast<io::NetworkableBase*>(this);

    m_actionBuffer = new ActionBuffer();            // operator new(0x2C) - no arguments
    if (!m_actionBuffer)
    {
        // The site that attests this file's line 275.
        Debug(DEBUGTYPE_6, "Game/GameLogic/Player.cpp", 275, __FILE__)
            << "Failed to create actionBuffer";
        return;                                     // the ELF skips the rest of the body too
    }

    // A virtual call from A constructor, on the table just stamped in. This is
    // io::INetworkable's +0x18 slot, not a private helper - see Player.h.
    init(0);

    // Retail also sets a networkable-base float to -1 here; that field has no exposed accessor
    // in this model. Notify AI of the player only when its singleton exists.
    if (ai::g_aiInstance)
        eventManager->postEvent(static_cast<EventCategory>(3), 12, this, 0.0f);   // +0x14

    // Seeds only the final two unlock maps with keys 0–6 and zero values.
    for (int m = 0; m < 2; ++m)
        for (int k = 0; k <= 6; ++k)
            m_unlockMaps[2 + m][k] = 0;

    allocateFreeCamera();
}


// Allocate four replication snapshots and seed each with the current player state. The argument
// is unused. Return false if allocation fails; the constructor ignores that result. Restore the
// ring cursor to its head after seeding. Spawn groups outside 0..255 become zero, and the repeated
// spawn-group queries must remain separate virtual calls.
bool Player::init(void* param)
{
    (void)param;    // the constructor passes 0 and the body never reads it

    NetUpdateSlot* buffer = new NetUpdateSlot[kNetUpdateSlots];   // 4 x 0xBC; typed, but filled by offset

    // The pre-pass: two fields per slot, before any snapshot goes in.
    for (int i = 0; i < kNetUpdateSlots; ++i)
    {
        unsigned char* slot = reinterpret_cast<unsigned char*>(&buffer[i]);
        *reinterpret_cast<int*>(slot + 0x00) = 0;
        *(slot + 0x04)                       = 0;
    }

    m_netUpdateBuffer = buffer;
    if (!buffer)
        return false;

    m_next = m_head;

    for (int idx = 0; idx <= 3; ++idx)
    {
        m_next = idx;
        unsigned char* slot = reinterpret_cast<unsigned char*>(&m_netUpdateBuffer[m_next]);

        *reinterpret_cast<int*>(slot + 0x30) = getSpawnGroup();
        if (getSpawnGroup() < 0 || getSpawnGroup() > 255)
            *reinterpret_cast<int*>(slot + 0x30) = 0;

        *(slot + 0x50)                       = (unsigned char)m_flagHolder;
        *reinterpret_cast<int*>(slot + 0x18) = m_scoreData->m_score;
        *reinterpret_cast<int*>(slot + 0x0C) = m_scoreData->m_field14;
        *reinterpret_cast<int*>(slot + 0x10) = m_scoreData->m_field04;
        *reinterpret_cast<int*>(slot + 0x08) = m_scoreData->m_field10;
        *(slot + 0x51)                       = (unsigned char)m_field160[7];
        *reinterpret_cast<int*>(slot + 0x54) = m_firePressCount;   // +0x208 - trigger pulls

        // Initializes three weapon-state blocks, leaving their remaining state zero for firing updates.
        for (int w = 0; w < 3; ++w)
        {
            unsigned char* block = slot + 0x58 + 24 * w;
            *(block + 0x00)                       = (unsigned char)m_field160[w];   // firing trio
            *reinterpret_cast<int*>(block + 0x04) = 0;
            *reinterpret_cast<int*>(block + 0x08) = 0;
            *(block + 0x0C)                       = 0;
            *reinterpret_cast<int*>(block + 0x10) = 0;
            *reinterpret_cast<int*>(block + 0x14) = 0;
        }

        *(slot + 0xA0)                                  = (unsigned char)m_field160[3];  // sprint
        *(slot + 0xA1)                                  = (unsigned char)m_field160[4];
        *reinterpret_cast<int*>(slot + 0x1C)            = m_team;
        *reinterpret_cast<unsigned short*>(slot + 0xAA) = (unsigned short)m_ping;  // low word only
        *reinterpret_cast<int*>(slot + 0xA4)            = m_spawnAtTick;
        *(slot + 0xA8)                                  = (unsigned char)m_isManDown;

        if (getCamera())
            *reinterpret_cast<int*>(slot + 0x2C) = static_cast<int>(getCamera()->getViewMode());  // vtable +0x1C
        else
            *reinterpret_cast<int*>(slot + 0x2C) = 0;

        *(slot + 0xAE) = 0;

        // truncated TO A byte after A floor(). A player outside the world for more than 255
        // seconds wraps rather than saturating.
        *(slot + 0xAC) = (unsigned char)(int)std::floor(m_timeOutSideWorld);

        *reinterpret_cast<int*>(slot + 0x20) = m_scoreData->m_field54;
        *reinterpret_cast<int*>(slot + 0x24) = m_scoreData->m_field58;
        *reinterpret_cast<int*>(slot + 0x28) = m_scoreData->m_field5C;
        *(slot + 0xAD)                       = getKitAndUnlockLevelNetworkValue();
        *(slot + 0x3C)                       = (unsigned char)m_isSquadLeader;
        *(slot + 0x3D)                       = (unsigned char)m_isCommander;
        *reinterpret_cast<int*>(slot + 0xB4) = m_tracePlayerId;
        *reinterpret_cast<int*>(slot + 0xB8) = getMaskSetting();
    }

    // Initializes cached ghost-fire state left untouched by construction.
    std::memset(m_ghostFire20C, 0, 12);
    std::memset(m_ghostFire218, 0, 12);
    std::memset(m_ghostFire224, 0,  3);   // the ELF coalesces these three bytes into a word
                                          // store + a byte store; identical in effect
    std::memset(m_ghostFire228, 0, 12);
    std::memset(m_ghostFire234, 0, 12);

    m_next = m_head;    // the walk above is undone - see the header block
    return true;
}

namespace {
// Typed views into a net-update slot's raw buff[] payload (buff[N] == slot offset 0x0C + N). The ELF
// does these as raw offset moves of the natural width; a few are deliberately narrowed (ping is a
// word, many flags are bytes). Some offsets are not 4-aligned (buff[158]); x86 tolerates the
// unaligned access, and reproducing it is the point.
inline int&            slotDword(NetUpdateSlot* s, int off) { return *reinterpret_cast<int*>(&s->buff[off]); }
inline unsigned short& slotWord (NetUpdateSlot* s, int off) { return *reinterpret_cast<unsigned short*>(&s->buff[off]); }
inline float&          slotFloat(NetUpdateSlot* s, int off) { return *reinterpret_cast<float*>(&s->buff[off]); }
// clamp a score to [lo, 2047]; lo is -2047 for signed counters, 0 for the rest.
inline int clampScore(int v, int lo) { if (v < lo) return lo; if (v >= 2048) return 2047; return v; }
} // namespace

// Snapshot replicated state into the next ping-pong slot and OR changed-field bits into mask[0].
// mask must contain at least three words: mask[1] requests team resends and mask[2] gates sprint
// state. This path alternates two slots despite the four-slot allocation. Preserve repeated
// getters and the field-specific score limits and storage widths.
void Player::updateStateMask(unsigned int* mask)
{
    // advance the 2-slot ping-pong
    const int prev = m_head;                     // dword11o <- old m_head
    m_prev = prev;
    const int cur = (m_head + 1) & 1;
    m_head = cur;                                 // dword7o
    m_next = cur;                                 // currentBufferIdx

    NetUpdateSlot* const c = &m_netUpdateBuffer[cur];    // current snapshot
    NetUpdateSlot* const p = &m_netUpdateBuffer[prev];   // previous, to delta against

    // isAlive - bit 0x1
    c->isAlive = static_cast<unsigned char>(getIsAlive());
    if (p->isAlive != c->isAlive)
    {
        *mask |= 0x1u;

    }

    // spawn group -> buff[36] - bit 0x2, then clamped to [0, 255]
    slotDword(c, 36) = getSpawnGroup();
    if (slotDword(c, 36) != slotDword(p, 36))
        *mask |= 0x2u;
    if (getSpawnGroup() < 0 || getSpawnGroup() > 255)
        slotDword(c, 36) = 0;

    // the score block (all share bit 0x8)
    slotDword(c, 12) = clampScore(m_scoreData->m_score,   -2047);   // totalScore
    if (slotDword(c, 12) != slotDword(p, 12)) *mask |= 0x8u;
    c->someScore2    = clampScore(m_scoreData->m_field10,  0);       // someScore2 (its own slot field)
    if (c->someScore2 != p->someScore2)       *mask |= 0x8u;
    slotDword(c, 0)  = clampScore(m_scoreData->m_field14,  0);       // someScore1
    if (slotDword(c, 0)  != slotDword(p, 0))  *mask |= 0x8u;
    slotDword(c, 4)  = clampScore(m_scoreData->m_field04, -2047);    // dword4
    if (slotDword(c, 4)  != slotDword(p, 4))  *mask |= 0x8u;
    slotDword(c, 8)  = clampScore(m_scoreData->m_field18,  0);       // dword18
    if (slotDword(c, 8)  != slotDword(p, 8))  *mask |= 0x8u;

    // flag holder -> buff[68] - bit 0x200000
    c->buff[68] = static_cast<unsigned char>(m_flagHolder);
    if (p->buff[68] != c->buff[68]) *mask |= 0x200000u;

    // byte167 -> buff[69] - bit 0x10
    c->buff[69] = static_cast<unsigned char>(m_field160[7]);
    if (p->buff[69] != c->buff[69]) *mask |= 0x10u;

    // reload -> bit 0x10000, and consume the one-shot flag
    if (m_reloadState)
    {
        *mask |= 0x10000u;
        m_reloadState = false;
    }

    // fire-press count -> buff[72] - bit 0x2000000
    slotDword(c, 72) = m_firePressCount;
    if (slotDword(c, 72) != slotDword(p, 72)) *mask |= 0x2000000u;

    // three 24-byte weapon blocks: firing flag + deviation snapshot
    static const unsigned int firingBits[3] = { 0x40u, 0x80u, 0x20000u };
    const unsigned char firing[3] =
        { m_field160[0], m_field160[1], m_field160[2] };   // firing / alt-fire / flare
    for (int w = 0; w < 3; ++w)
    {
        c->buff[24 * w + 76] = firing[w] != 0;
        // the firing bit is set if it changed OR a reload happened this frame (bit 0x10000)
        if (p->buff[24*w+76] != c->buff[24*w+76] || (*mask & 0x10000u) != 0)
            *mask |= firingBits[w];

        int dev = m_ghostFire20C[w];                       // deviation, clamped to [-1, 62]
        if (dev >= 62) dev = 62;
        if (dev < -1)  dev = -1;
        slotDword(c, 24*w + 80) = dev;
        slotDword(c, 24*w + 84) = m_ghostFire218[w];
        slotDword(c, 24*w + 92) = *reinterpret_cast<const int*>(&m_ghostFire228[w]);   // float bits
        c->buff[24*w + 88]      = m_ghostFire224[w];
        slotDword(c, 24*w + 96) = m_ghostFire234[w];
    }

    // sprint -> buff[148] - bit 0x40000; the last-tick copy only when mask[2]&4
    c->buff[148] = static_cast<unsigned char>(m_field160[3]);
    if (p->buff[148] != c->buff[148]) *mask |= 0x40000u;
    if ((mask[2] & 4u) != 0)
        c->buff[149] = static_cast<unsigned char>(m_field160[4]);

    // heal / gave-ammo "recent" flags: set for the 10 ticks after the event (unsigned compare)
    if (static_cast<unsigned int>(m_healRepairedAtTick + 10) > WorldPref::getTickCount()) *mask |= 0x80000u;
    if (static_cast<unsigned int>(m_gaveAmmoAtTick + 10) > WorldPref::getTickCount()) *mask |= 0x100000u;

    // team -> buff[16] - bit 0x200, with a 20-frame re-send after a team change (mask[1]&2)
    slotDword(c, 16) = m_team;
    if (slotDword(c, 16) != slotDword(p, 16)) *mask |= 0x200u;
    if ((mask[1] & 2u) != 0)
    {
        const int frameDelayCount = 20;          // ELF static ds:0x89867C4 = 0x14
        m_field258 = frameDelayCount;
        *mask -= 0x200u;                          // clear the immediate team bit; it is re-sent below
    }
    if (m_field258 > 0)
    {
        --m_field258;
        if (m_field258 <= 2) *mask |= 0x200u;
    }

    // ping -> buff[158] - bit 0x100 (16-bit)
    slotWord(c, 158) = static_cast<unsigned short>(getPing());
    if (slotWord(c, 158) != slotWord(p, 158)) *mask |= 0x100u;

    // spawn tick -> buff[152] - bit 0x20
    slotDword(c, 152) = m_spawnAtTick;
    if (slotDword(c, 152) != slotDword(p, 152)) *mask |= 0x20u;

    // man-down -> buff[156] - bit 0x20 (shared with spawn tick)
    c->buff[156] = static_cast<unsigned char>(m_isManDown);
    if (p->buff[156] != c->buff[156]) *mask |= 0x20u;

    // camera view mode -> buff[32] - bit 0x400 (only when there is a camera). ICameraObject vtable
    // +0x1C is getViewMode() const (CameraViewMode); the ELF re-fetches getCamera() for the call, so
    // this does too. buff[162] is unconditionally cleared inside the camera arm.
    if (getCamera())
    {
        slotDword(c, 32) = static_cast<int>(getCamera()->getViewMode());   // vtable +0x1C
        if (slotDword(c, 32) != slotDword(p, 32)) *mask |= 0x400u;
        c->buff[162] = 0;
    }

    // time-outside-world (floor, truncated to a byte) -> buff[160] - bit 0x800
    c->buff[160] = static_cast<unsigned char>(static_cast<int>(std::floor(m_timeOutSideWorld)));
    if (p->buff[160] != c->buff[160]) *mask |= 0x800u;

    // rank + three career score dwords -> buff[44]/[20]/[24]/[28] - all bit 0x4000
    slotDword(c, 44) = m_scoreData->m_rank;
    if (slotDword(c, 44) != slotDword(p, 44)) *mask |= 0x4000u;
    slotDword(c, 20) = m_scoreData->m_field54;
    if (slotDword(c, 20) != slotDword(p, 20)) *mask |= 0x4000u;
    slotDword(c, 24) = m_scoreData->m_field58;
    if (slotDword(c, 24) != slotDword(p, 24)) *mask |= 0x4000u;
    slotDword(c, 28) = m_scoreData->m_field5C;
    if (slotDword(c, 28) != slotDword(p, 28)) *mask |= 0x4000u;

    // kit low-nibble | (chosen unlock level << 4) -> buff[161] - bit 0x1000
    c->buff[161] = static_cast<unsigned char>(
        (16 * getChosenUnlockLevel(getKit(), getTeam())) | (getKit() & 0xF));
    if (c->buff[161] != p->buff[161]) *mask |= 0x1000u;

    // squad id -> buff[40] - bit 0x2000
    slotDword(c, 40) = m_squadId;
    if (slotDword(c, 40) != slotDword(p, 40)) *mask |= 0x2000u;

    // free / overhead camera flags -> buff[163]/[164] - bits 0x8000 / 0x400000
    c->buff[163] = static_cast<unsigned char>(m_freeCameraEnabled);
    if (p->buff[163] != c->buff[163]) *mask |= 0x8000u;
    c->buff[164] = static_cast<unsigned char>(m_overheadCameraEnabled);
    if (p->buff[164] != c->buff[164]) *mask |= 0x400000u;

    // squad-invite mask -> buff[165] - bit 0x800000
    c->buff[165] = static_cast<unsigned char>(m_squadInviteMask);
    if (c->buff[165] != p->buff[165]) *mask |= 0x800000u;

    // trace player id -> buff[168] - bit 0x1000000
    slotDword(c, 168) = m_tracePlayerId;
    if (slotDword(c, 168) != slotDword(p, 168)) *mask |= 0x1000000u;

    // the commander / status cluster, all under bit 0x4000000
    c->buff[48] = static_cast<unsigned char>(m_isSquadLeader);            // no bit of its own
    c->buff[49] = static_cast<unsigned char>(m_isCommander);
    if (p->buff[49] != c->buff[49]) *mask |= 0x4000000u;
    c->buff[50] = static_cast<unsigned char>(getUAVIsReady());
    if (p->buff[50] != c->buff[50]) *mask |= 0x4000000u;
    c->buff[51] = static_cast<unsigned char>(getSatelliteIsReady());
    if (p->buff[51] != c->buff[51]) *mask |= 0x4000000u;
    c->buff[52] = static_cast<unsigned char>(m_fieldF8);                  // dword35 bytes 0..2
    if (p->buff[52] != c->buff[52]) *mask |= 0x4000000u;
    c->buff[53] = static_cast<unsigned char>(m_fieldF9);
    if (p->buff[53] != c->buff[53]) *mask |= 0x4000000u;
    c->buff[54] = static_cast<unsigned char>(m_fieldFA);
    if (p->buff[54] != c->buff[54]) *mask |= 0x4000000u;
    slotDword(c, 56) = m_fieldFC;                                         // dword36
    if (slotDword(c, 56) != slotDword(p, 56)) *mask |= 0x4000000u;
    slotDword(c, 60) = m_field100;                                        // dword37
    if (slotDword(c, 60) != slotDword(p, 60)) *mask |= 0x4000000u;
    slotDword(c, 64) = m_field104;                                        // dword38
    if (slotDword(c, 64) != slotDword(p, 64)) *mask |= 0x4000000u;

    // mask setting -> buff[172] - bit 0x8000000
    slotDword(c, 172) = getMaskSetting();
    if (slotDword(c, 172) != slotDword(p, 172)) *mask |= 0x8000000u;
}

// Player updates carry a 28-bit field mask followed by fields in wire order. Squad leadership
// and connection state are always sent; weapon state is sent only at ghost precision. The
// baseline argument is unused. Preserve the ranged coder's extra bit for byte-sized ranges
// and the distinct hot/cold weapon-heat quantization.
namespace {

Vec3 playerStatsColour()
{
    Vec3 c; c.x = 240.0f; c.y = 100.0f; c.z = 100.0f;
    return c;
}
io::RegistredNetworkStats g_playerStats("Player", playerStatsColour());

void netWriteBit(io::BitStream& stream, bool value)
{
    unsigned int bit = value ? 1u : 0u;
    stream.writeBits(&bit, 1);
}

// The inlined BitStream.inl:59 range coder - bounds test, bit-width loop, the fatal Debug arm.
void netWriteRanged(io::BitStream& stream, int value, int lower, int upper)
{
    if (value >= lower && value <= upper)
    {
        const unsigned int range = static_cast<unsigned int>(upper - lower);
        unsigned int bits = 0;
        do
            ++bits;
        while (range + 1 > (1u << bits) - 1);
        unsigned int scratch = static_cast<unsigned int>(value - lower);
        stream.writeBits(&scratch, bits);
    }
    else
    {
        stream.setError(4);
        Debug(DEBUGTYPE_7, "IO/BitStream.inl", 59, "")
            << "(fatal error) Value out of bounds val: " << value
            << " lower " << lower << " upper: " << upper;
    }
}


// Local readers corresponding to the network bit writers.
bool netReadBit(io::BitStream& stream)
{
    unsigned int bit = 0;
    stream.readBits(&bit, 1);
    return bit != 0;
}

// Read a value using the writer's ranged-width calculation, then add lower. The decoded value
// is not range-checked. A wrapped zero span consumes no bits; a 0..255 range consumes nine bits.
int netReadRanged(io::BitStream& stream, int lower, int upper)
{
    const unsigned int span = static_cast<unsigned int>(upper - lower) + 1u;
    unsigned int bits = 0;
    if (span != 0)                                  // 's `je` - see above
    {
        do
            ++bits;
        while (span > (1u << bits) - 1);
    }
    unsigned int scratch = 0;
    stream.readBits(&scratch, bits);
    return static_cast<int>(scratch) + lower;
}

// Read sign and magnitude. bits includes the sign bit, so a 12-bit field has 11 magnitude bits.
int netReadSigned(io::BitStream& stream, unsigned int bits)
{
    unsigned int scratch = 0;
    stream.readBits(&scratch, 1);
    const bool negative = (scratch == 1);
    scratch = 0;
    stream.readBits(&scratch, bits - 1);
    return negative ? -static_cast<int>(scratch) : static_cast<int>(scratch);
}

} // anonymous namespace

// Decode a player update and apply its masked fields while preserving locally predicted firing
// and gas-mask state. baseline and precision are ignored. Heal/ammo notification bits carry no
// payload and use the receiver's tick; heat and commander meters have additional normalization.
// Use this build's snapshot layout: the original PE and ELF records have different offsets.
void Player::setNetUpdate(io::BitStream& stream, float updateTime, bool ghostActive,
                          io::BaseLineData* /*baseline*/, int /*precision*/)
{
    // there IS NO error test here. PE 0x50057E's `cmp [esi+0x34], 1` is getBitPosition
    // inlined (+0x34 is m_readWriteSwitch; the error latch is the byte at +0x1C) and the first
    // statement of the shipped body is the position stamp. See the block above.
    m_updateStartBitPos = stream.getBitPosition();
    makeRoomForNextUpdate(ghostActive);

    if (m_head != m_next)
        m_netUpdateBuffer[m_next] = m_netUpdateBuffer[m_head];   // rep movsd: the whole record

    NetUpdateSlot* const c = &m_netUpdateBuffer[m_next];
    // record +0x00, NOT buff[0]. The PE does `fld [ebp+0xC] / fstp [record]` at displacement
    // zero, and buff starts at +0x0C - buff[0] is someScore1, which an earlier draft would have
    // clobbered. someVal is declared int but is written as a float by both ports.
    *reinterpret_cast<float*>(&c->someVal) = updateTime;

    unsigned int stateMask = 0;
    stream.readBits(&stateMask, 28);                     // `push 0x1C`

    if (stateMask & 0x1)
        c->isAlive = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);

    if (stateMask & 0x2)
        slotDword(c, 36) = netReadRanged(stream, 0, 255);          // spawnGroup - nine bits

    if (stateMask & 0x8) slotDword(c, 12) = netReadSigned(stream, 12);      // totalScore
    if (stateMask & 0x8) c->someScore2    = netReadRanged(stream, 0, 2047);
    if (stateMask & 0x8) slotDword(c, 0)  = netReadRanged(stream, 0, 2047);
    if (stateMask & 0x8) slotDword(c, 4)  = netReadSigned(stream, 12);
    if (stateMask & 0x8) slotDword(c, 8)  = netReadRanged(stream, 0, 2047);

    if (stateMask & 0x200000)
        c->buff[68] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // flagHolder
    if (stateMask & 0x10)
        c->buff[69] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);

    // These notification bits carry no payload. Start the heal/ammo feedback window on the
    // receiver's local tick.
    if (stateMask & 0x80000)
        m_healRepairedAtTick = WorldPref::getTickCount();
    if (stateMask & 0x100000)
        m_gaveAmmoAtTick = WorldPref::getTickCount();

    // Read weapon state from mask bits regardless of precision; only the writer has a
    // StatePrecisionGhost gate.
    {
        world::IPlayerControlObject* pco = 0;
        if (getVehicle())                                // the dead-weak drop, as in the writer
            pco = getWeaponVehicle();

        static const unsigned int firingBits[3] = { 0x40u, 0x80u, 0x20000u };

        if (stateMask & 0x2000000)
        {
            unsigned int presses = 0;
            stream.readBits(&presses, 2);
            slotDword(c, 72) = static_cast<int>(presses);
        }

        for (int w = 0; w <= 2; ++w)
        {
            if ((firingBits[w] & stateMask) == 0 && (stateMask & 0x10000) == 0)
                continue;

            const bool firing = netReadBit(stream);
            c->buff[24 * w + 76] = static_cast<unsigned char>(firing ? 1 : 0);

            if (!firing && (stateMask & 0x10000) == 0)
                continue;

            slotDword(c, 24 * w + 80) = netReadRanged(stream, 0, 63) - 1;   // the writer sent dev+1
            {
                unsigned int scratch = 0;
                stream.readBits(&scratch, 2);
                slotDword(c, 24 * w + 84) = static_cast<int>(scratch);
            }
            c->buff[24 * w + 88] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);

            const bool hot = netReadBit(stream);
            if (hot)
            {
                unsigned int raw = 0;
                stream.readBits(&raw, 8);
                double heat = static_cast<double>(raw);
                heat = heat * (1.0 / 255.0);             // ds:0x8AC480
                heat = heat * 10.0;                      // ds:0x8913C0 - a second multiply, in order
                slotFloat(c, 24 * w + 92) = static_cast<float>(heat);
            }
            else
            {
                unsigned int raw = 0;
                stream.readBits(&raw, 6);
                double heat = static_cast<double>(raw);
                heat = heat * (1.0 / 63.0);              // ds:0x8AC478
                slotFloat(c, 24 * w + 92) = static_cast<float>(heat);
            }

            // After dequantization, clear heat only when a weapon interface explicitly reports that
            // it does not use heat. A missing interface preserves the decoded value.
            if (pco)
            {
                world::IObject* const weapon = pco->getActiveWeapon(w);      // +0x70
                if (weapon)
                {
                    world::IWeaponObject* const weaponInterface =
                        static_cast<world::IWeaponObject*>(
                            weapon->queryInterface(world::IID_IWeaponObject));
                    if (weaponInterface && !weaponInterface->getUsesHeat())
                        slotFloat(c, 24 * w + 92) = 0.0f;
                }
            }

            {
                unsigned int scratch = 0;
                stream.readBits(&scratch, 4);
                slotDword(c, 24 * w + 96) = static_cast<int>(scratch);
            }
        }
    }

    if (stateMask & 0x40000)
        c->buff[148] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // sprint
    if (stateMask & 0x40000)
        c->buff[149] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // sprint last tick

    if (stateMask & 0x100)
        slotWord(c, 158) =
            static_cast<unsigned short>(netReadRanged(stream, 0, 1023));   // ping, a word

    if (stateMask & 0x20)
    {
        unsigned int spawnTick = 0;
        stream.readBits(&spawnTick, 32);
        slotDword(c, 152) = static_cast<int>(spawnTick);
    }
    if (stateMask & 0x20)
        c->buff[156] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // isManDown

    if (stateMask & 0x200)
        slotDword(c, 16) = netReadRanged(stream, 0, 3) - 1;        // team; the writer sent team+1

    if (stateMask & 0x400)
        slotDword(c, 32) = netReadRanged(stream, 0, 8);            // camera view mode
    if (stateMask & 0x400)
        c->buff[162] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);

    // nine bits each - and the correction recorded at the writer's copy generalises: every
    // 0..255 arm costs nine, because netReadRanged's loop takes a step past (1<<8)-1 = 255.
    if (stateMask & 0x800)
        c->buff[160] = static_cast<unsigned char>(netReadRanged(stream, 0, 255));
    if (stateMask & 0x1000)
        c->buff[161] = static_cast<unsigned char>(netReadRanged(stream, 0, 255));

    if (stateMask & 0x2000)
        slotDword(c, 40) = netReadRanged(stream, 0, 15);           // squadId
    if (stateMask & 0x4000)
        slotDword(c, 44) = netReadRanged(stream, 0, 31) - 1;       // rank; the writer sent rank+1
    if (stateMask & 0x4000)
        slotDword(c, 20) = netReadRanged(stream, 0, 255);
    if (stateMask & 0x4000)
        slotDword(c, 24) = netReadRanged(stream, 0, 255);
    if (stateMask & 0x4000)
        slotDword(c, 28) = netReadRanged(stream, 0, 255);

    if (stateMask & 0x8000)                              // PE `test ah, ah / jns` = bit 15
        c->buff[163] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // free camera
    if (stateMask & 0x400000)
        c->buff[164] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // overhead camera
    if (stateMask & 0x800000)
    {
        unsigned int inviteMask = 0;
        stream.readBits(&inviteMask, 8);
        c->buff[165] = static_cast<unsigned char>(inviteMask);
    }
    if (stateMask & 0x1000000)
        slotDword(c, 168) = netReadRanged(stream, 0, 65536) - 1;   // tracePlayerId; writer sent id+1

    c->buff[48] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // isSquadLeader:
                                                         // always, no mask gate - as in the writer

    if (stateMask & 0x4000000)                           // the commander cluster
    {
        const bool commander = netReadBit(stream);
        c->buff[49] = static_cast<unsigned char>(commander ? 1 : 0);
        if (commander)
        {
            c->buff[50] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // uavReady
            c->buff[51] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);   // satelliteReady
            c->buff[52] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);
            c->buff[53] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);
            c->buff[54] = static_cast<unsigned char>(netReadBit(stream) ? 1 : 0);

            for (int f = 0; f < 3; ++f)
            {
                unsigned int raw = 0;
                stream.readBits(&raw, 8);
                float value = static_cast<float>(static_cast<double>(raw) * (1.0 / 255.0));
                // Snap commander meter values within half a quantization step to zero. The inclusive
                // comparison deliberately preserves NaNs.
                if (std::fabs(value) <= 0.001953125f)
                    value = 0.0f;
                slotFloat(c, 56 + 4 * f) = value;
            }
        }
    }

    m_isConnected = netReadBit(stream);                  // a live field, unmasked, as shipped

    if (stateMask & 0x8000000)
        slotDword(c, 172) = netReadRanged(stream, 0, 3);

    // [UNRESOLVED] The original read-error diagnostic uses an unidentified Debug overload.
    // Do not substitute DebugTypeError, which aborts here; this path clears the latch and continues.
    if (stream.hasError())                               // +0x1C, the real latch
        stream.clearError();

    g_playerStats.m_readBits +=
        static_cast<int>(stream.getBitPosition() - m_updateStartBitPos);

    if (stateMask == 0)                                  // nothing changed, no apply
        return;

    // Apply decoded fields in their original order. Local firing and gas-mask state remain
    // predicted. Direct field stores intentionally bypass setter notifications; the reload bit
    // re-arms the one-shot state consumed by updateStateMask.

    if (stateMask & 0x1)
        setIsAlive(c->isAlive != 0);                          // +0x68

    if (stateMask & 0x2)
        setSpawnGroup(slotDword(c, 36));                      // +0x154

    if (stateMask & 0x10)
        setCallForMedic(c->buff[69] != 0);
        // Network firing and sprint state does not overwrite the local human's prediction.
    if (world::playerManager->getLocalHumanPlayer() != this)   // +0x30
    {
        static const unsigned int firingBits[3] = { 0x40u, 0x80u, 0x20000u };
        unsigned int firedMask = 0;

        for (int w = 0; w < 3; ++w)
        {
            if ((firingBits[w] & stateMask) == 0)
                continue;

            const bool firing = c->buff[24 * w + 76] != 0;
            if (w == 0)
            {
                setFiringState(firing);                       // +0x1A0
                // Only primary fire sets this latch; alternate fire and flares do not.
                if (firing)
                    m_field204 = true;
            }
            else if (w == 1)
                setAltFiringState(firing);                    // +0x1A8
            else
                setFlareFiringState(firing);                  // +0x1B0

            if (firing || (stateMask & 0x10000) != 0)
                firedMask |= (1u << w);
        }

        // the fan-out is Player::forceSetRemoteClientAmmo - the ELF apply
        // calls it BY name with exactly this mask (ELF 0x80ECDF0; client PE 0x4FF640). The old
        // note's "ELF 0x80F31A0" was the neighbouring setCallForMedic. See the body.
        if (firedMask != 0 || (stateMask & 0x10000) != 0)
            forceSetRemoteClientAmmo(firedMask);

        // Only the low two bits of the press-count delta matter; a delta of four raises no change.
        if (stateMask & 0x2000000)
        {
            const int delta = slotDword(c, 72) - slotDword(&m_netUpdateBuffer[m_head], 72);
            if ((delta & 3) != 0)
                m_field204 = true;
        }
        m_firePressCount = slotDword(c, 72);      // unconditional, outside the 0x2000000 arm

        if (stateMask & 0x40000)
        {
            setSprintState(c->buff[148] != 0);                // +0x1B8
            setSprintStateLastTick(c->buff[149] != 0);        // +0x1C0
        }
    }

    if (stateMask & 0x10000)
        m_reloadState = true;                     // the flag updateStateMask consumes

    // Preserve the separate state-mask tests.
    if (stateMask & 0x8) m_scoreData->m_score   = slotDword(c, 12);
    if (stateMask & 0x8) m_scoreData->m_field10 = c->someScore2;
    if (stateMask & 0x8) m_scoreData->m_field14 = slotDword(c, 0);
    if (stateMask & 0x8) m_scoreData->m_field04 = slotDword(c, 4);
    if (stateMask & 0x8) m_scoreData->m_field18 = slotDword(c, 8);

    if (stateMask & 0x200000)
        setFlagHolder(c->buff[68] != 0);                      // +0x178

    if (stateMask & 0x100)
        m_ping = slotWord(c, 158);                            // raw, not setPing

    if (stateMask & 0x20)
    {
        m_spawnAtTick = slotDword(c, 152);
        m_isManDown   = c->buff[156] != 0;                    // raw, not setIsManDown
    }

    if (stateMask & 0x200)
        setTeam(slotDword(c, 16), false);                     // +0xE4, keepVerifyPending = false

    if (stateMask & 0x400)
    {
        // getCamera() IS called twice - once to test, once to use (PE 0x5015A8 and 0x5015B6).
        // The same quirk updateStateMask's camera arm already carries, and the same family as
        // getNetUpdate's double getIsCommander. Folding it would change how many times an
        // override's accessor runs.
        if (getCamera())                                      // +0x180
            getCamera()->setViewMode(
                static_cast<world::CameraViewMode>(slotDword(c, 32)), 0, false);   // +0x18
    }

    if (stateMask & 0x800)
        m_timeOutSideWorld = static_cast<float>(c->buff[160]);   // fild/fstp - a byte to a float

    if (stateMask & 0x1000)
    {
        // The kit byte packs kit and unlock level into low and high nibbles.
        // Read the team between payload reads; setter arguments are kit, team, level.
        setKit(c->buff[161] & 0xF);                           // +0x16C
        setChosenUnlockLevel(c->buff[161] & 0xF, getTeam(), c->buff[161] >> 4);   // +0x250
    }

    if (stateMask & 0x2000)
        setSquadId(static_cast<unsigned int>(slotDword(c, 40)));   // +0x108

    if (stateMask & 0x4000)
        m_scoreData->setRank(static_cast<unsigned int>(slotDword(c, 44)), this);
    if (stateMask & 0x4000) m_scoreData->m_field54 = slotDword(c, 20);
    if (stateMask & 0x4000) m_scoreData->m_field58 = slotDword(c, 24);
    if (stateMask & 0x4000) m_scoreData->m_field5C = slotDword(c, 28);

    if (stateMask & 0x8000)
        setFreeCameraEnabled(c->buff[163] != 0);              // +0xAC
    if (stateMask & 0x400000)
        setOverheadCameraEnabled(c->buff[164] != 0);          // +0xB8
    if (stateMask & 0x800000)
        m_squadInviteMask = c->buff[165];                     // raw
    if (stateMask & 0x1000000)
        m_tracePlayerId = slotDword(c, 168);                  // raw

    m_isSquadLeader = c->buff[48] != 0;           // unmasked, matching the unmasked read

    // Notify commander changes only when the flag actually changes.
    if (c->buff[49] != static_cast<unsigned char>(m_isCommander))
        setIsCommander(c->buff[49] != 0);                     // +0x110

    // Reread commander status after its setter before applying commander state.
    if (m_isCommander)
    {
        m_uavIsReady       = c->buff[50] != 0;
        m_satelliteIsReady = c->buff[51] != 0;
        m_fieldF8          = c->buff[52] != 0;
        m_fieldF9          = c->buff[53] != 0;
        m_fieldFA          = c->buff[54] != 0;
        m_fieldFC          = slotDword(c, 56);
        m_field100         = slotDword(c, 60);
        m_field104         = slotDword(c, 64);
    }

    // Reread the local-human identity rather than reusing the earlier lookup.
    if (stateMask & 0x8000000)
    {
        if (world::playerManager->getLocalHumanPlayer() != this)
            setMaskSetting(slotDword(c, 172));
    }
}

bool Player::getNetUpdate(io::BitStream& stream, io::BaseLineData* /*baseline*/,
                          unsigned int stateMask, int precision)
{
    m_updateStartBitPos = stream.getBitPosition();
    NetUpdateSlot* const c = &m_netUpdateBuffer[m_head];

    unsigned int mask = stateMask;
    stream.writeBits(&mask, 28);

    if (stateMask & 0x1)
        netWriteBit(stream, c->isAlive != 0);

    if (stateMask & 0x2)
        netWriteRanged(stream, slotDword(c, 36), 0, 255);                   // spawnGroup

    if (stateMask & 0x8)                                                    // the score block
    {
        const int totalScore = slotDword(c, 12);
        if (totalScore < 0)
        {
            netWriteBit(stream, true);
            unsigned int magnitude = static_cast<unsigned int>(-totalScore);
            stream.writeBits(&magnitude, 11);
        }
        else
        {
            netWriteBit(stream, false);
            unsigned int magnitude = static_cast<unsigned int>(totalScore);
            stream.writeBits(&magnitude, 11);
        }
    }
    if (stateMask & 0x8)
        netWriteRanged(stream, c->someScore2, 0, 2047);
    if (stateMask & 0x8)
        netWriteRanged(stream, slotDword(c, 0), 0, 2047);
    if (stateMask & 0x8)
    {
        const int score4 = slotDword(c, 4);
        if (score4 < 0)
        {
            netWriteBit(stream, true);
            unsigned int magnitude = static_cast<unsigned int>(-score4);
            stream.writeBits(&magnitude, 11);
        }
        else
        {
            netWriteBit(stream, false);
            unsigned int magnitude = static_cast<unsigned int>(score4);
            stream.writeBits(&magnitude, 11);
        }
    }
    if (stateMask & 0x8)
        netWriteRanged(stream, slotDword(c, 8), 0, 2047);

    if (stateMask & 0x200000)
        netWriteBit(stream, c->buff[68] != 0);                              // flagHolder
    if (stateMask & 0x10)
        netWriteBit(stream, c->buff[69] != 0);

    // the weapon section - ghost precision only
    if (io::StatePrecisionGhost == precision)
    {
        world::IPlayerControlObject* pco = 0;
        if (getVehicle())                                // the dead-weak drop runs, as shipped
            pco = getWeaponVehicle();

        static const unsigned int firingBits[3] = { 0x40u, 0x80u, 0x20000u };

        if (stateMask & 0x2000000)
        {
            unsigned int presses = static_cast<unsigned int>(slotDword(c, 72)) & 3u;
            stream.writeBits(&presses, 2);
        }

        for (int w = 0; w <= 2; ++w)
        {
            if ((firingBits[w] & stateMask) == 0 && (stateMask & 0x10000) == 0)
                continue;

            const bool firing = c->buff[24 * w + 76] != 0;
            netWriteBit(stream, firing);

            if (!firing && (stateMask & 0x10000) == 0)
                continue;

            netWriteRanged(stream, slotDword(c, 24 * w + 80) + 1, 0, 63);   // deviation + 1
            {
                unsigned int scratch = static_cast<unsigned int>(slotDword(c, 24 * w + 84));
                stream.writeBits(&scratch, 2);
            }
            netWriteBit(stream, c->buff[24 * w + 88] != 0);

            // the overheat fork - hot only when the weapon says it uses heat AND heat >= 1.0
            bool hot = false;
            if (pco)
            {
                world::IObject* const weapon = pco->getActiveWeapon(w);     // +0x70
                if (weapon)
                {
                    world::IWeaponObject* const weaponInterface =
                        static_cast<world::IWeaponObject*>(
                            weapon->queryInterface(world::IID_IWeaponObject));
                    if (weaponInterface && weaponInterface->getUsesHeat())
                        hot = slotFloat(c, 24 * w + 92) >= 1.0f;
                }
            }
            netWriteBit(stream, hot);
            if (hot)
            {
                double scaled = slotFloat(c, 24 * w + 92) / 10.0;
                if (scaled < 0.0)
                    scaled = 0.0;
                else if (scaled > 1.0)
                    scaled = 1.0;
                unsigned int quantised = static_cast<unsigned int>(scaled * 255.0);
                stream.writeBits(&quantised, 8);
            }
            else
            {
                double scaled = slotFloat(c, 24 * w + 92);
                if (scaled < 0.0)
                    scaled = 0.0;
                else if (scaled > 1.0)
                    scaled = 1.0;
                unsigned int quantised = static_cast<unsigned int>(scaled * 63.0);
                stream.writeBits(&quantised, 6);
            }

            {
                unsigned int scratch = static_cast<unsigned int>(slotDword(c, 24 * w + 96));
                stream.writeBits(&scratch, 4);
            }
        }
    }

    if (stateMask & 0x40000)
        netWriteBit(stream, c->buff[148] != 0);                             // sprint
    if (stateMask & 0x40000)
        netWriteBit(stream, c->buff[149] != 0);                             // sprint last tick

    if (stateMask & 0x100)
        netWriteRanged(stream, slotWord(c, 158), 0, 1023);                  // ping (a word)

    if (stateMask & 0x20)
    {
        unsigned int spawnTick = static_cast<unsigned int>(slotDword(c, 152));
        stream.writeBits(&spawnTick, 32);
    }
    if (stateMask & 0x20)
        netWriteBit(stream, c->buff[156] != 0);                             // isManDown

    if (stateMask & 0x200)
        netWriteRanged(stream, slotDword(c, 16) + 1, 0, 3);                 // team + 1

    if (stateMask & 0x400)
        netWriteRanged(stream, slotDword(c, 32), 0, 8);                     // camera view mode
    if (stateMask & 0x400)
        netWriteBit(stream, c->buff[162] != 0);

    // These byte values occupy nine bits on the wire, with a zero high bit. Using eight bits
    // would shift every field that follows.
    if (stateMask & 0x800)
    {
        unsigned int timeOutside = c->buff[160];                            // 9 bits, no check
        stream.writeBits(&timeOutside, 9);
    }
    if (stateMask & 0x1000)
    {
        unsigned int kitByte = c->buff[161];                                // 9 bits, no check
        stream.writeBits(&kitByte, 9);
    }

    if (stateMask & 0x2000)
        netWriteRanged(stream, slotDword(c, 40), 0, 15);                    // squadId
    if (stateMask & 0x4000)
        netWriteRanged(stream, slotDword(c, 44) + 1, 0, 31);                // rank + 1
    if (stateMask & 0x4000)
        netWriteRanged(stream, slotDword(c, 20), 0, 255);
    if (stateMask & 0x4000)
        netWriteRanged(stream, slotDword(c, 24), 0, 255);
    if (stateMask & 0x4000)
        netWriteRanged(stream, slotDword(c, 28), 0, 255);

    if (stateMask & 0x8000)
        netWriteBit(stream, c->buff[163] != 0);                             // free camera
    if (stateMask & 0x400000)
        netWriteBit(stream, c->buff[164] != 0);                             // overhead camera
    if (stateMask & 0x800000)
    {
        unsigned int inviteMask = c->buff[165];
        stream.writeBits(&inviteMask, 8);
    }
    if (stateMask & 0x1000000)
        netWriteRanged(stream, slotDword(c, 168) + 1, 0, 65536);            // tracePlayerId + 1

    netWriteBit(stream, c->buff[48] != 0);          // isSquadLeader - always, no mask gate

    if (stateMask & 0x4000000)                                              // the commander cluster
    {
        const bool commander = c->buff[49] != 0;
        netWriteBit(stream, commander);
        if (commander)
        {
            netWriteBit(stream, c->buff[50] != 0);                          // uavReady
            netWriteBit(stream, c->buff[51] != 0);                          // satelliteReady
            netWriteBit(stream, c->buff[52] != 0);
            netWriteBit(stream, c->buff[53] != 0);
            netWriteBit(stream, c->buff[54] != 0);

            for (int f = 0; f < 3; ++f)
            {
                double scaled = slotFloat(c, 56 + 4 * f);
                if (scaled < 0.0)
                    scaled = 0.0;
                else if (scaled > 1.0)
                    scaled = 1.0;
                unsigned int quantised = static_cast<unsigned int>(scaled * 255.0);
                stream.writeBits(&quantised, 8);
            }
        }
    }

    netWriteBit(stream, m_isConnected);             // live field, unmasked, as shipped

    if (stateMask & 0x8000000)
        netWriteRanged(stream, slotDword(c, 172), 0, 3);                    // mask setting

    if (stream.hasError())
    {
        stream.clearError();
        return false;
    }
    g_playerStats.m_writeBits +=
        static_cast<int>(stream.getBitPosition() - m_updateStartBitPos);

    return true;
}

// . IPlayer +0x158. The whole body is one load - RAW-verified
// (`mov eax, [eax+0D4h]`), which also re-confirms m_spawnGroup's offset a third time
// (the ctor's store, IPlayer.h's "(int +0xD4)" annotation, and now this).
int Player::getSpawnGroup() const
{
    return m_spawnGroup;
}


namespace {

// io::PlayerInput's press test, written once. dice emitted it inline at each of handleInput's
// three sites (`shrd ecx, ebx, N / and eax, 1` against the 64-bit mapping mask, then a compare
// against 0.5f). The same helper exists file-locally in ServerGameLogic.cpp for the same reason.
inline bool inputPressed(const io::PlayerInput& input, io::PlayerInputMap index)
{
    if (!(input.mappingFlags & (1ULL << index)))   // +0x100 - one 64-bit field
        return false;
    return input.inputs[index] > 0.5f;
}

// Walk the object and its parents until a player-control object supplies its active camera.
// Return null when the chain contains no matching interface.
world::ICameraObject* cameraFromControlObjectChain(world::IObject* object)
{
    for (world::IObject* obj = object; obj != 0; obj = obj->getParent())
    {
        // A free camera can be the first player-control object in this chain; use its active camera.
        world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
            obj->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            return pco->getActiveCamera();                     // PCO +0x44
    }
    return 0;
}

} // anonymous namespace

// Return the commander camera only for the local human commander. Preserve the repeated
// commander query. A squad camera lacking ICameraObject violates the invariant and aborts.
namespace world {

ICameraObject* getCommanderCameraForPlayer(const IPlayer* player)
{
    if (!player)
        return 0;
    if (!playerManager)
        return 0;
    if (player != playerManager->getLocalHumanPlayer())
        return 0;
    if (!player->getIsCommander())
        return 0;
    if (!squadManager)                                  // ds:0x8ACBFA4
        return 0;

    bool overheadOk = false;
    if (player->getOverheadCameraTopDownView())
        overheadOk = player->getCommanderCamOverhead();

    // see the header block: getIsCommander is already known true here.
    if (player->getIsCommander() && !overheadOk)
        return 0;

    IObject* cam = static_cast<IObject*>(
        static_cast<ISquadManager*>(squadManager)->getCommanderCam(player->getTeam()));
    if (!cam)
        return 0;

    ICameraObject* camera =
        static_cast<ICameraObject*>(cam->queryInterface(IID_ICameraObject));
    if (!camera)
        abort();                                        // the original's, not a stand-in
    return camera;
}

} // namespace world

// Store the borrowed camera pointer without changing reference counts or releasing the
// previous camera. Camera resolution separately prefers commander and free-camera sources.
void Player::setCamera(world::ICameraObject* camera)
{
    m_camera = camera;                                                        // +0x68
}

world::ICameraObject* Player::getCamera() const
{
    world::ICameraObject* camera = world::getCommanderCameraForPlayer(this);
    if (camera)
        return camera;

    if (getFreeCameraEnabled())                         // +0xB0, reads byte +0x70
    {
        // The +0x6C weak block: its target sits at +4. IPlayer.h records this as
        // "the +0x6C weak block's target", which is what getFreeCamera returns.
        world::IObject* target = 0;
        if (m_freeCameraWeak)
            target = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_freeCameraWeak)->target;
        if (target)
            camera = cameraFromControlObjectChain(target);
    }
    if (camera)
        return camera;

    if (!getVehicle())                                  // first of two identical calls
        return 0;
    world::IObject* vehicle = getVehicle();             // second - reproduced, not folded
    if (!vehicle)
        return 0;

    return cameraFromControlObjectChain(vehicle);
}

// Return whether the local human player is using view mode 1 or 2; remote players return false.
// Camera overrides resolve the vehicle's active camera directly. Preserve the repeated mode query.
bool Player::hasFirstPersonView() const
{
    world::IPlayer* const localPlayer = world::playerManager->getLocalHumanPlayer();
    if (localPlayer != this)
        return false;

    world::ICameraObject* camera = 0;
    if (m_overheadCameraEnabled || m_freeCameraEnabled)
        camera = cameraFromControlObjectChain(localPlayer->getVehicle());
    else
        camera = localPlayer->getCamera();

    return camera
        && (camera->getViewMode() == static_cast<world::CameraViewMode>(1)
            || camera->getViewMode() == static_cast<world::CameraViewMode>(2));
}

// . IPlayer +0xB0. One byte load - and it re-confirms m_freeCameraEnabled at +0x70,
// which is the one name in that camera cluster the order-matching pass got right.
bool Player::getFreeCameraEnabled() const
{
    return m_freeCameraEnabled;
}

//  / PE 0x47BD30: read the flag directly, then call the setter virtually.
// There is no getFreeCameraEnabled virtual query in either port.
void Player::toggleFreeCameraEnabled()
{
    setFreeCameraEnabled(!m_freeCameraEnabled);
}

// Return the top-down flag only while this player is a commander with the overhead camera enabled.
bool Player::getOverheadCameraTopDownView() const
{
    if (m_isCommander && m_overheadCameraEnabled)                    // +0xF4, then +0x71
        return m_overheadCameraTopDownView != 0;                     // +0x72
    return false;
}

// . IPlayer +0xC0. forwards first, through its OWN vtable: the top-down write
// always mirrors into setCommanderCamOverhead (+0xC8, virtual - `call [edx+0C8h]`, preserved as a
// virtual call), and only then guard-stores its own byte.
void Player::setOverheadCameraTopDownView(bool topDown)
{
    setCommanderCamOverhead(topDown);                                // virtual +0xC8
    if (topDown != (m_overheadCameraTopDownView != 0))               // the +0x72 guard-store
        m_overheadCameraTopDownView = topDown;
}

//  / 0x80F9000, 12 B - the plain +0x73 pair the mirror above lands in.
void Player::setCommanderCamOverhead(bool overhead)
{
    m_commanderCamOverhead = overhead;
}

bool Player::getCommanderCamOverhead() const
{
    return m_commanderCamOverhead;
}

// Return the free-camera weak target without testing its cached/dead flag.
world::IObject* Player::getFreeCamera() const
{
    unsigned char* block = static_cast<unsigned char*>(m_freeCameraWeak);
    if (!block)
        return 0;
    return reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target;
}

// Enable or disable the free camera. Enabling is ignored while the overhead camera is active;
// disabling always clears the flag. Enabling selects mode 1 on the free-camera object's active
// camera. Read the weak target directly rather than dispatching the getFreeCamera override.
void Player::setFreeCameraEnabled(bool enabled)
{
    // The overhead camera wins - but only against being turned ON.
    if (enabled && m_overheadCameraEnabled)                    // byte +0x71
        return;

    m_freeCameraEnabled = enabled;                             // byte +0x70
    if (!enabled)
        return;

    unsigned char* block = static_cast<unsigned char*>(m_freeCameraWeak);   // +0x6C
    if (!(block ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target : 0))
        return;

    // second read of the same block - reproduced, not folded (see the note above)
    world::IObject* freeCamera = block ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target : 0;

    world::ICameraObject* camera = cameraFromControlObjectChain(freeCamera);
    if (camera)
        camera->setViewMode(static_cast<world::CameraViewMode>(1), 0, true);   // +0x18
}

//  / PE 0x472A20, 30 B. IPlayer +0xB8. Disabling the overhead camera
// also disables its top-down mode through the virtual setter; enabling it leaves that mode alone.
void Player::setOverheadCameraEnabled(bool enabled)
{
    m_overheadCameraEnabled = enabled;                    // ELF +0x71 / PE +0x8D
    if (!enabled)
        setOverheadCameraTopDownView(false);               // virtual +0xC0
}

// . IPlayer +0xBC. checkPlayerTriggers' second guard.
bool Player::getOverheadCameraEnabled() const
{
    return m_overheadCameraEnabled;                       // byte +0x71
}

unsigned int Player::getSpawnAtTick() const
{
    return static_cast<unsigned int>(m_spawnAtTick);      // +0xC8
}

// . IPlayer +0xF0.
void Player::setSpawnAtTick(unsigned int tick)
{
    m_spawnAtTick = static_cast<int>(tick);               // +0xC8
}

// . IPlayer +0x100.
bool Player::getIsManDown()
{
    return m_isManDown;                                   // byte +0xCC
}

// Return the saved man-down tick only while the player is down; otherwise return zero.
unsigned int Player::getManDownAtTick()
{
    if (!m_isManDown)                                     // byte +0xCC
        return 0;
    return static_cast<unsigned int>(m_manDownAtTick);    // +0xD0
}

// Returns the spawn countdown while man-down, otherwise zero.
float Player::getManDownTimeLeft() const
{
    if (!m_isManDown)
        return 0.0f;
    return getTimeToSpawn();                              // +0xF4
}

//  / PE 0x472360. Subtraction wraps in 32 bits before signed conversion.
// [PLATFORM] ELF rounds the integer to float then divides by 30; PE's x87 multiplies the
// signed integer by a single-precision reciprocal. Neither port uses an unsigned countdown.
float Player::getTimeToSpawn() const
{
    const int ticks = static_cast<int>(static_cast<unsigned int>(m_spawnAtTick) -
                                       WorldPref::getTickCount());
#if defined(_WIN32)
    const float seconds = static_cast<float>(static_cast<double>(ticks) * (1.0f / 30.0f));
#else
    const float seconds = static_cast<float>(ticks) / 30.0f;
#endif
    return seconds < 0.0f ? 0.0f : seconds;
}

// Return the vehicle weak target. If its cached/dead flag is set, release the weak block,
// clear the stored pointer, and return null despite this method being const. Use the native
// weak-pointer layout: its target is at +4 on x86 and +8 on x64.
world::IObject* Player::getVehicle() const
{
    unsigned char* block = static_cast<unsigned char*>(m_vehicleWeak);

    world::IObject* target = 0;
    if (block)
        target = reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target;
    if (!target)
        return 0;

    // IObject +0x04, bit 0x20 - the "this object is gone" flag.
    if (!(world::object_detail::LayoutAccess::flags(target) & 0x20))
        return target;                                  // the normal path

    // the drop. Everything below is a WeakPtr assignment to null, written out inline.
    Player* self = const_cast<Player*>(this);

    if (block)
    {
        int* refcount = reinterpret_cast<int*>(block);
        if (--(*refcount) == 0)
            ::operator delete(block);
    }
    self->m_vehicleWeak = 0;

    // The original then releases the value it is assigning from, which is the null it just
    // built on the stack - so this arm is inert. Reproduced because it is what makes the sequence
    // recognisable as a WeakPtr assignment rather than an ad-hoc clear.

    return 0;
}

// . IPlayer +0x170. One load.
int Player::getKit() const
{
    return m_kit;
}

// . IPlayer +0x16C. The bare store of the selected kit index (+0x54); no validation
// here - GameServer::handleClientInfo hands it a kit id the client already chose.
void Player::setKit(int kit)
{
    m_kit = kit;                                         // +0x54
}

// . IPlayer +0xE8. One load - m_team's offset now confirmed a third way (the ctor's
// store, init()'s read, and this).
int Player::getTeam() const
{
    return m_team;
}

// Pack the kit into the low nibble and its chosen unlock level into the high nibble. Both
// values are truncated to four bits. Preserve the two separate getKit calls.
unsigned char Player::getKitAndUnlockLevelNetworkValue() const
{
    const int kit    = getKit() & 0x0F;                                  // +0x170
    const int team   = getTeam();                                        // +0xE8
    const int unlock = getChosenUnlockLevel(getKit(), team);             // +0x24C, getKit again

    return (unsigned char)(kit | ((unlock << 4) & 0xF0));
}

// Encode equipment state as 1 for gas mask, 0 for night vision, or 2 for neither.
int Player::getMaskSetting() const
{
    if (getIsGasMaskOn())               // +0x2C0, byte +0x304
        return 1;
    return getNightVision() ? 0 : 2;    // +0x2E8, byte +0x320
}

// Apply equipment state 0 (night vision), 1 (gas mask), or 2 (neither), then clear both
// transition timers. Each valid arm calls both setters. Unknown settings change nothing.
void Player::setMaskSetting(int setting)
{
    if (setting == 0)                                   // night vision
    {
        m_playerGasMaskChangeTimer     = 0.0f;          // +0x308
        m_playerNightVisionChangeTimer = 0.0f;          // +0x30C
        setIsGasMaskOn(false, false);                   // +0x2C4
        setNightVision(true, false);                    // +0x2DC
    }
    else if (setting == 1)                              // gas mask
    {
        m_playerGasMaskChangeTimer     = 0.0f;
        m_playerNightVisionChangeTimer = 0.0f;
        setIsGasMaskOn(true, false);
        setNightVision(false, false);
    }
    else if (setting == 2)                              // neither
    {
        m_playerGasMaskChangeTimer     = 0.0f;
        m_playerNightVisionChangeTimer = 0.0f;
        setIsGasMaskOn(false, false);
        setNightVision(false, false);
    }
}

// Set the medic-request flag only for a teammate of the local human player; clearing is
// unconditional. Enabling stores the timestamp before the flag, while disabling clears them
// in the opposite order. Cleared time is zero, distinct from the constructor's -1 sentinel.
void Player::setCallForMedic(bool state)
{
    if (!state)
    {
        m_field160[7] = false;
        m_field168    = 0.0;                    // ds:0x880F08 - zero, not the -1.0 sentinel
        return;
    }

    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();   // +0x30
    if (!local)
        return;                                 // no local player - nothing is written at all
    if (local->getTeam() != getTeam())           // +0xE8 on both
        return;                                 // enemy - dropped silently

    m_field168    = WorldPref::getWorldTime();
    m_field160[7] = state;                      // the ARG, not a literal true
}

// Look up kit in the unlock map selected by slot, returning zero for a missing entry.
// slot indexes from m_unlockMaps[1], not [0], and must be within the available map range.
int Player::getChosenUnlockLevel(int kit, int slot) const
{
    const std::map<int,int>& m = m_unlockMaps[1 + slot];     // base +0x284 = element 1

    std::map<int,int>::const_iterator it = m.find(kit);
    if (it == m.end())
        return 0;
    return it->second;
}

float Player::getBlurValue() const
{
    return m_blurValue;                                    // +0x300
}

// . IPlayer +0x2C0. One byte load.
bool Player::getIsGasMaskOn() const
{
    return m_isGasMaskOn;
}

// setIsGasMaskOn - ELF 0x80F6A70, 850 B / PE 0x4762C0. The transition gate is shared with night
// vision: forced requests always land, otherwise the timer must have expired unless this is a
// remote player on a client. The local sound state intentionally contains repeated unconditional
// play/stop calls, and the final sprint-factor arm reads the raw default-vehicle weak target.
void Player::setIsGasMaskOn(bool enabled, bool force)
{
    if (force
        || m_playerGasMaskChangeTimer <= 0.0f
        || (world::playerManager->getLocalHumanPlayer() != this && !gameLogic->isServer()))
    {
        m_isGasMaskOn = enabled;
        m_playerGasMaskChangeTimer = enabled
            ? MAX_MASK_TIMEOUT_ON_TIME
            : MAX_MASK_TIMEOUT_OFF_TIME;

        if (world::playerManager->getLocalHumanPlayer() == this)
        {
            if (m_blurValue > 0.0f)
            {
                if (m_isGasMaskOn)
                {
                    if (!m_isInTearGas)
                        g_bfSound->playSound(std::string("S_GasMaskTearGas"));
                    m_isInTearGas = true;
                    g_bfSound->stopSound(std::string("S_TearGas"));
                    m_byte32E = false;
                }
                else
                {
                    g_bfSound->stopSound(std::string("S_GasMaskTearGas"));
                    const bool startTearGas = !m_byte32E;
                    m_isInTearGas = false;
                    if (startTearGas)
                        g_bfSound->playSound(std::string("S_TearGas"));
                    m_byte32E = true;
                }
            }
            else if (m_isGasMaskOn)
            {
                g_bfSound->playSound(std::string("S_HeavyBreathing"));
                m_byte330 = true;
            }
            else
            {
                g_bfSound->stopSound(std::string("S_HeavyBreathing"));
                m_byte330 = false;
            }
        }

        unsigned char* const block = static_cast<unsigned char*>(m_defaultVehicleWeak);
        world::IObject* const defaultVehicle = block
            ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target
            : 0;
        if (defaultVehicle)
        {
            world::Soldier* const soldier = static_cast<world::Soldier*>(defaultVehicle);
            const float factor = enabled
                ? soldier->m_soldierTemplate->m_gasMaskSprintFactor
                : 1.0f;
            soldier->setSprintStaminaDissipationFactor(factor);
        }

        setGasMaskHudItem(enabled);
    }
}

// . IPlayer +0x2E8. One byte load.
bool Player::getNightVision() const
{
    return m_nightVision;
}

// Advance render-frame effects, FOV interpolation, and local HUD state using deltaTime.
// This is separate from fixed-step update; setFov only supplies the target and rate.
void Player::handleFrameUpdate(float deltaTime)
{
    if (m_byte32B && world::playerManager->getLocalHumanPlayer() == this)
    {
        m_byte32B = false;
        setNightVision(false, true);
    }

    m_crossHairFadeValue -= deltaTime * 3.0f;
    if (m_crossHairFadeValue <= 0.0001f)
        m_crossHairFadeValue = 0.0f;

    if (m_fov > m_endFov)
    {
        m_fov -= deltaTime * m_fovRate;
        if (m_fov <= m_endFov)
            m_fov = m_endFov;
    }
    else if (m_fov < m_endFov)
    {
        m_fov += deltaTime * m_fovRate;
        if (m_fov >= m_endFov)
            m_fov = m_endFov;
    }

    if (world::playerManager->getLocalHumanPlayer() == this)
    {
        world::ICameraObject* camera = m_camera;
        if (camera && !camera->isOverheadCamera())
        {
            // The repeated getViewMode call is shipped: mode zero short-circuits, every other mode
            // is fetched again before the comparison with one.
            if (static_cast<int>(camera->getViewMode()) == 0
                || static_cast<int>(camera->getViewMode()) == 1
                || m_isManDown
                || !m_isAlive)
            {
                setGasMaskHudItem(getIsGasMaskOn());
                setNightVisionMaskOverlay(getNightVision());
            }
            else
            {
                if (getIsGasMaskOn())
                    setGasMaskHudItem(false);
                if (getNightVision())
                    setNightVisionMaskOverlay(false);
            }
        }
    }

    handleGasClouds(deltaTime);
    handleNightVision(deltaTime);

    if (m_playerGasMaskChangeTimer > 0.0f)
        m_playerGasMaskChangeTimer -= deltaTime;
    if (m_nightVisionVisualTransitionTimer > 0.0f)
        m_nightVisionVisualTransitionTimer -= deltaTime;
}

//  / PE counterpart: one float load from the crosshair frame-state field.
float Player::getCrossHairFadeValue() const
{
    return m_crossHairFadeValue;
}

//  / PE counterpart: one byte load.
bool Player::getIsNearGasCloud() const
{
    return m_isNearGasCloud;
}

//  / PE nullsub.  These HUD functions are genuinely empty in the shipped
// dedicated image; their calls and argument evaluation remain observable and are retained above.
void Player::setGasMaskHudItem(bool /*enabled*/)
{
}

float Player::getPlayerGasMaskChangeTimer() const
{
    return m_playerGasMaskChangeTimer;
}

float Player::getPlayerNightVisionChangeTimer() const
{
    return m_playerNightVisionChangeTimer;
}

float Player::getNormalizedNightVisionPower()
{
    return 1.0f;
}

//  / PE 0x4760D0.  This deliberately reads both weak blocks directly instead of
// calling getVehicle/getDefaultVehicle: those getters may release a destroyed weak target, while
// the retail immunity predicate has no such cleanup side effect.
bool Player::hasGasCloudImmunity() const
{
    bool openVehicle = true;

    unsigned char* vehicleBlock = static_cast<unsigned char*>(m_vehicleWeak);
    world::IObject* vehicle = vehicleBlock
        ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(vehicleBlock)->target
        : 0;

    if (vehicle)
    {
        unsigned char* defaultBlock = static_cast<unsigned char*>(m_defaultVehicleWeak);
        world::IObject* defaultVehicle = defaultBlock
            ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(defaultBlock)->target
            : 0;

        if (defaultVehicle)
        {
            // Preserve retail's repeated weak-target reads rather than caching the first values.
            defaultVehicle = defaultBlock
                ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(defaultBlock)->target
                : 0;
            vehicle = vehicleBlock
                ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(vehicleBlock)->target
                : 0;

            if (defaultVehicle != vehicle)
            {
                vehicle = vehicleBlock
                    ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(vehicleBlock)->target
                    : 0;
                const world::PlayerControlObjectTemplate* vehicleTemplate =
                    static_cast<const world::PlayerControlObjectTemplate*>(vehicle->getTemplate());
                openVehicle = vehicleTemplate->m_isOpenVehicle;
            }
        }
    }

    return m_isGasMaskOn || !openVehicle;
}

// Update blur, then scan particle systems for gas exposure unless the player is immune.
// A missing particle-system manager ends the scan; dedicated images normally take that path.
void Player::handleGasClouds(float deltaTime)
{
    handleBlurryVision(deltaTime);

    if (hasGasCloudImmunity())
        return;

    geom::IParticleSystemManager* const particleSystemManager =
        geom::asParticleSystemManager(
            classManager->getSingleton(std::string("ParticleSystemManager")));
    if (!particleSystemManager)
        return;

    // The renderer's active-instance vector, shared by reference (issue #7).
    const geom::ParticleSystemInstanceList* const systems = particleSystemManager->getInstances();
    // The PE re-reads the vector's size on every iteration; retain that rather than snapshotting
    // a container that instance calls may mutate.
    for (unsigned int i = 0; i < static_cast<unsigned int>(systems->size()); ++i)
    {
        geom::IParticleSystemInstance* const system = (*systems)[i];
        if (!system->getTemplate()->getGasCloudType())
            continue;

        // return, not continue: a player with no default vehicle abandons the whole scan
        // mid-loop rather than skipping this system. Shipped on both ports.
        unsigned char* const defaultBlock = static_cast<unsigned char*>(m_defaultVehicleWeak);
        world::IObject* const vehicle = defaultBlock
            ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(defaultBlock)->target
            : 0;
        if (!vehicle)
            return;

        // The cloud is asked for its position before the player is, and the delta is
        // cloud - player, in that order.
        const Vec3& cloud = system->getPosition();
        const Vec3& here = vehicle->getAbsolutePosition();          // IObject slot 27
        const float dx = cloud.x - here.x;
        const float dy = cloud.y - here.y;
        const float dz = cloud.z - here.z;

        const float radius = system->getGasCloudRadius();
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        const float radiusSquared = radius * radius;

        // Two tests against the same distance and they are not exclusive: five radii squared
        // only raises the proximity flag, one radius squared drives the blur.
        if (distanceSquared < radiusSquared * 5.0f)
            m_isNearGasCloud = true;

        if (distanceSquared >= radiusSquared)
            continue;

        // The template is fetched again for the equality test, and once more for each of the
        // three values - five +0x8C calls per blurring system in all. Preserved.
        if (system->getTemplate()->getGasCloudType() != 1)
            continue;

        geom::IParticleSystemTemplate* const forDamage = system->getTemplate();
        const float damage = forDamage->getGasCloudDamage();
        geom::IParticleSystemTemplate* const forHold = system->getTemplate();
        const float holdTime = forHold->getBlurryVisionHoldTime();
        geom::IParticleSystemTemplate* const forBlur = system->getTemplate();
        const float blurryTime = forBlur->getBlurryVisionTime();
        // Fetched damage, hold, blur (right-to-left evaluation in PE), then passed blur, hold,
        // damage. Five getTemplate calls per blurring system are preserved.
        setBlurryVision(blurryTime, holdTime, damage);              // PE +0x2B0 / ELF +0x2B4
    }
}

// Updates gas/blur state, including unconditional default-vehicle lookup and local-only sound/damage effects.
void Player::handleBlurryVision(float deltaTime)
{
    if (getIsGasMaskOn()
        && !m_isInTearGas
        && world::playerManager->getLocalHumanPlayer() == this)
    {
        ISound* transition = static_cast<ISound*>(
            g_bfSound->getSound(std::string("S_GasMaskTearGasTransition")));
        if (transition && !transition->isPlaying() && !m_byte330)
        {
            g_bfSound->playSound(std::string("S_HeavyBreathing"));
            m_byte330 = true;
        }
    }

    // Unconditional in both shipped images, even when neither blur timer is active.
    world::IObject* defaultVehicle = getDefaultVehicle();

    if (m_gasDamage > 0.0f)
    {
        m_gasDamage -= deltaTime;

        if (world::playerManager->getLocalHumanPlayer() == this
            && defaultVehicle
            && defaultVehicle->getTemplate()->getClassID() == world::CID_SoldierTemplate
            && m_field2E4 > 0.0f)
        {
            const Vec3 up = { 0.0f, 1.0f, 0.0f };
            const Vec3 position = defaultVehicle->getAbsolutePosition();
            gameLogic->giveDamage(defaultVehicle, deltaTime * m_field2E4,
                                  0, 0, 0, -1, 0, 0,
                                  position, up, -1, false, true, false);
        }

        m_blurValue = 1.0f;
    }
    else
    {
        m_field2F4 -= deltaTime;
        if (m_field2F4 > 0.0f)
        {
            m_blurValue = m_field2F4 / m_field2F8;
        }
        else
        {
            m_blurValue = 0.0f;
            m_field2F4 = 0.0f;
            m_gasDamage = 0.0f;

            if (world::playerManager->getLocalHumanPlayer() == this)
            {
                if (m_byte32E)
                {
                    g_bfSound->stopSound(std::string("S_TearGas"));
                    m_byte32E = false;
                }

                if (m_isInTearGas)
                {
                    g_bfSound->stopSound(std::string("S_GasMaskTearGas"));
                    m_isInTearGas = false;
                    g_bfSound->playSound(std::string("S_GasMaskTearGasTransition"));
                }
            }
        }
    }
}

//  / PE 0x472FC0.  The one-second time stamp refresh deliberately samples exact time
// twice on the refresh frame.  Power depletion sends the event only from a local non-server player.
void Player::handleNightVision(float deltaTime)
{
    const float exactTime = static_cast<float>(g_system->getExactTime());
    if (exactTime - m_field31C >= 1.0f)
        m_field31C = static_cast<float>(g_system->getExactTime());

    if (m_field324 <= 0.0f)
    {
        if (getNightVision())
        {
            setNightVision(false, false);
            if (!gameLogic->isServer()
                && world::playerManager->getLocalHumanPlayer() == this)
            {
                game->sendGameEventToAllPlaying(new SetNightVisionEvent(getId(), false));
            }
        }
        m_field324 = 0.0f;
    }

    if (m_playerNightVisionChangeTimer > 0.0f)
        m_playerNightVisionChangeTimer -= deltaTime;
}

//  / PE 0x4730E0.  Both named retail timeout constants contain 0.95f.
void Player::setNightVision(bool enabled, bool force)
{
    if (force
        || m_playerNightVisionChangeTimer <= 0.0f
        || (world::playerManager->getLocalHumanPlayer() != this && !gameLogic->isServer()))
    {
        m_playerNightVisionChangeTimer = 0.95f;
        setVisualNightVision(enabled);
        m_nightVision = enabled;
    }
}

//  / PE single retn: an exact shipped empty body.
void Player::rechargeNightVision()
{
}

//  / PE 0x474980.  The local visual arm changes the map's console script; remote and
// dedicated players retain only the one-shot recharge latch at the tail.
void Player::setVisualNightVision(bool enabled)
{
    if (world::playerManager->getLocalHumanPlayer() == this)
    {
        // Preserve the observed call order: map name first, level path second, concatenated path
        // in level-path + map-name order.
        const std::string& mapName = game->getCurrentMapName();
        const std::string& levelPath = game->getCurrentMapLevelPath();
        const std::string basePath = levelPath + mapName;

        if (enabled)
        {
            if (m_nightVisionVisualTransitionTimer <= 0.0f)
                m_nightVisionVisualTransitionTimer = 0.95f;
        }
        else
        {
            io::mainConsole->run(basePath + "/normal.con",
                                 std::string(), std::string(), std::string(), std::string(),
                                 std::string(), std::string(), std::string(), std::string(),
                                 io::Console::ignoredString_);
        }

        setNightVisionHudItem(enabled);
    }

    if (enabled && m_byte32A)
    {
        rechargeNightVision();
        m_byte32A = false;
    }
}

//  / 0x80F7D30, both 5 B; PE emits matching nullsubs.  They are intentionally not
// folded out at source level because retail still evaluates arguments and calls them.
void Player::setNightVisionHudItem(bool /*enabled*/)
{
}

void Player::setNightVisionMaskOverlay(bool /*enabled*/)
{
}

// Extend blur only when the requested fade/hold duration exceeds the current effect. Sound
// is attempted before that check. Capture the initial vehicle position only near the start
// of a fade, and broadcast an event only when the runtime game exposes IGameServer.
void Player::setBlurryVision(float fadeTime, float holdTime, float damage)
{
    if (m_field2F4 <= 0.0f && fadeTime > 0.0f && !m_byte32E
        && world::playerManager->getLocalHumanPlayer() == this)
    {
        g_bfSound->playSound(std::string("S_TearGas"));
        m_byte32E = true;
    }

    if (!(fadeTime + holdTime > m_field2F4 + m_gasDamage))
        return;

    if (m_field2F4 <= 0.1f && fadeTime > 0.0f)
    {
        WeakPtr<world::IObject>::Ctrl* const control =
            static_cast<WeakPtr<world::IObject>::Ctrl*>(m_defaultVehicleWeak);
        world::IObject* const vehicle = control ? control->target : 0;
        if (vehicle)
        {
            const Vec3& position = vehicle->getAbsolutePosition();
            // These existing three dword carriers hold float bits, not numeric integers.
            std::memcpy(&m_field2E8, &position.x, sizeof(float));
            std::memcpy(&m_field2EC, &position.y, sizeof(float));
            std::memcpy(&m_field2F0, &position.z, sizeof(float));
        }
    }
    m_field2E4 = damage;
    m_field2F4 = fadeTime;
    m_field2F8 = fadeTime;
    m_gasDamage = holdTime;
    if (!game->queryInterface(IID_IGameServer))
        game->sendGameEventToAllPlaying(
            new PlayerTearGassedEvent(getId(), m_field2F4, m_gasDamage, m_field2E4));
}

//  (140 B) / PE 479250 (209 B): append in map order, skipping expired/null
// weak targets. Neither image clears the destination or materializes a return value.
void Player::getCloseMines(std::vector<IObject*>& out) const
{
    for (std::map<WeakPtr<world::IObject>, ItemStatus>::const_iterator it = m_closeMines.begin();
         it != m_closeMines.end(); ++it)
    {
        world::IObject* const object = it->first.get();
        if (object)
            out.push_back(object);
    }
}

// disableFlashBangEffect - ELF 0x80F7D50, 59 B. Only the local human player has a screen to clear,
// and the global explosion-effect manager (ds:0x8AB08E0) is null-tested before it is asked to
// kill every effect. A dedicated server has no local human player, so this is a two-call no-op.
void Player::disableFlashBangEffect()
{
    if (this == world::playerManager->getLocalHumanPlayer())              // +0x30
    {
        if (world::explosionEffectManager)                                // ds:0x8AB08E0
            world::explosionEffectManager->killAllEffects();
    }
}

// resetOnDeath - ELF 0x80F7D90, 100 B. A soldier body stops its drowning sound, the +0x321 byte is
// cleared, and the two visual-effect resets below run - both through the vtable, as compiled.
// The template read is the raw +0x14 field, not the getTemplate slot - LayoutAccess reproduces it.
void Player::resetOnDeath()
{
    world::IObject* const body = getDefaultVehicle();                    // +0xA4
    if (body && world::object_detail::LayoutAccess::objectTemplate(body)->getClassID()
                    == world::CID_SoldierTemplate)
        static_cast<world::Soldier*>(body)->stopDrowningSound();
    m_byte321 = false;                                                   // +0x321
    disableBlurryVision();                                               // +0x2F0
    disableFlashBangEffect();                                            // +0x2EC
}

// Clear blur state. For the local human player, stop each active tear-gas or poison loop
// and clear its latch; remote players receive only the state reset.
void Player::disableBlurryVision()
{
    m_blurValue = 0.0f;                                                  // +0x300
    m_field2E4  = 0.0f;                                                  // +0x2E4
    m_field2F4  = 0.0f;                                                  // +0x2F4
    m_gasDamage = 0.0f;                                                  // +0x2FC

    if (this != world::playerManager->getLocalHumanPlayer())             // +0x30
        return;

    if (m_byte32E)
    {
        g_bfSound->stopSound(std::string("S_TearGas"));                  // +0x30
        m_byte32E = false;
    }
    if (m_isInTearGas)
    {
        g_bfSound->stopSound(std::string("S_GasMaskTearGas"));
        m_isInTearGas = false;
    }
    if (m_byte331)
    {
        g_bfSound->stopSound(std::string("S_Poisoned"));
        m_byte331 = false;
    }
}

// Reset per-round score and world-boundary state while preserving career statistics and rank.
// Restore the -99999 world-time sentinel. The owned score object must be valid.
void Player::resetStats()
{
    m_worldTime        = -99999.0f;                                      // +0xE4  sentinel
    m_timeOutSideWorld = 0.0f;                                           // +0x24C
    m_field250         = 0;                                              // +0x250

    m_scoreData->reset();                                                // +0xEC - no null check
}

// Reset level-dependent state and round statistics while preserving identity, team, connection,
// ping, and network ID. Drop the vehicle with seat -1. gameLogic must exist; night vision is
// disabled only when a level is loaded, while gas-mask and blur resets run unconditionally.
void Player::reset()
{
    m_firePressCount = 0;                                                // +0x208

    resetStats();

    setVehicle(0, -1);                                                   // +0x4C  - nothing, nowhere
    setDefaultVehicle(0);                                                // +0xA0

    m_camera          = 0;                                               // +0x68
    m_spawnGroup      = 0;                                               // +0xD4
    m_squadInviteMask = 0;                                               // +0x11C
    m_isInsideCP      = false;                                           // +0x108
    m_kitTimestamp    = 0.0;                                             // member +0x4C (double)
    m_isCommander     = false;                                           // +0xF4

    if (gameLogic->isLevelLoadedXP())                                    // IGameLogic +0x80
        setNightVision(false, true);                                     // +0x2DC

    setIsGasMaskOn(false, true);                                         // +0x2C4
    disableBlurryVision();                                               // +0x2F0
}

// Check the radio/chat spam limit, doubled for commanders. A rejected message increments
// the blocked count and starts its timer if needed. Accepted messages increment the flag
// count only when countIt is true; update later decays these counters.
bool Player::getIsThisASpam(bool countIt)
{
    int maxFlagCount = serverInfo->m_block.radioMaxSpamFlagCount;        // ServerInfo +0x2C
    if (getIsCommander())                                                // +0x114
        maxFlagCount *= 2;

    const bool spam = getRadioSpamFlagCount() >= maxFlagCount;           // +0x314
    if (spam)
    {
        if (!getRadioSpamBlockedFlagCount())                             // +0x31C
            setRadioSpamFilterFlagTimer(static_cast<float>(WorldPref::getWorldTime()));   // +0x320
        setRadioSpamBlockedFlagCount(getRadioSpamBlockedFlagCount() + 1);   // +0x318
    }
    else if (countIt)
    {
        if (getRadioSpamFilterFlagTimer() < 0.0f)                        // +0x324
            setRadioSpamFilterFlagTimer(static_cast<float>(WorldPref::getWorldTime()));
        setRadioSpamFlagCount(getRadioSpamFlagCount() + 1);              // +0x310
    }
    return spam;
}

namespace {
// The hand-rolled weak block the three vehicle/camera slots hold: [+0] refcount, [+4] target.
// Dropping one is exactly what WeakPtr's destructor does - decrement, free at zero. Player keeps
// the blocks as void* (see the header), so the destructor releases them by hand.
void releasePlayerWeakBlock(void* block)
{
    if (!block)
        return;
    int* const count = static_cast<int*>(block);
    if ((*count)-- == 1)
        ::operator delete(block);
}
} // namespace

// Notify AI, destroy the free camera, exit the vehicle, and release owned input, score,
// tutorial, and replication data before base destruction. The vehicle's player-control
// interface is required. Release weak references without taking ownership of their targets.
Player::~Player()
{
    if (ai::g_aiInstance)                                                // ds:0x8AC37F8
        eventManager->postEvent(ECAI, 13, this, 0.0f);                   // +0x14

    if (world::IObject* const freeCamera = getFreeCamera())              // the +0x6C block's target
        game->destroyObject(freeCamera);                                 // IGame +0xA0

    {
        unsigned char* const block = static_cast<unsigned char*>(m_vehicleWeak);   // +0x64
        world::IObject* const vehicle =
            block ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target : 0;
        if (vehicle)
        {
            world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
                vehicle->queryInterface(world::IID_IPlayerControlObject));
            pco->exit();                                                 // +0x20 - pco unchecked
        }
    }

    delete m_actionBuffer;                                               // +0x158
    delete[] m_netUpdateBuffer;                                          // +0x240

    delete m_voHelp;                                                     // +0x274; null on dedicated

    delete m_scoreData;                                                  // +0xEC
    delete m_aiPlayerInput;                                              // +0x15C

    releasePlayerWeakBlock(m_defaultVehicleWeak);                        // +0xB0
    releasePlayerWeakBlock(m_freeCameraWeak);                            // +0x6C
    releasePlayerWeakBlock(m_vehicleWeak);                               // +0x64
}

namespace {


} // anonymous namespace

// Create the player's free camera when an id is assigned. Camera network ids use
// playerId + 257, separate from player ids. Clear vehicle control when unoccupied.
void Player::allocateFreeCamera()
{
    if (m_playerId == -1)               // the sentinel id gets no camera at all
        return;

    world::IObject* existing = 0;
    if (m_freeCameraWeak)
        existing = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_freeCameraWeak)->target;
    if (existing)
        return;                         // already has one

    world::IObjectTemplate* tmpl =
        world::objectTemplateManager->getTemplate(std::string("MultiPlayerFreeCamera"));
    if (!tmpl)
        return;                         // silent - no diagnostic, as getTemplate promises

    Mat4 xf = Mat4::getIdentity();

    const Vec3 dir = gameLogic->getBeforeSpawnCameraDirection();
    setRotationZXY(xf, -dir.z, -dir.y, -dir.x);     // cross-wired - see the header block

    const Vec3 pos = gameLogic->getBeforeSpawnCameraPosition();
    xf.m[12] = pos.x;
    xf.m[13] = pos.y;
    xf.m[14] = pos.z;

    world::IObject* camera =
        static_cast<world::FreeCameraTemplate*>(tmpl)->createObject();   // template vtable +0x54
    camera->setAbsoluteTransformation(xf);
    camera->init();                                                      // object vtable +0x20
    camera->setName(getName() + "_Camera");

    // Transfer the temporary weak handle's reference into m_freeCamera.
    WeakPtr<world::IObject> handle = camera->getWeakPtr();               // slot 7, +1 on the block
    void* block = handle.m_ctrl;                                         // the control block
    handle.m_ctrl = 0;                                                   // see above - no double release
    if (block)
        ++(*reinterpret_cast<int*>(block));                 // addref the new
    if (m_freeCameraWeak)                                   // release the old
    {
        int* old = reinterpret_cast<int*>(m_freeCameraWeak);
        if ((*old)-- == 1)
            ::operator delete(m_freeCameraWeak);
    }
    m_freeCameraWeak = block;
    if (block)                                              // release the temporary
    {
        int* n = reinterpret_cast<int*>(block);
        if ((*n)-- == 1)
            ::operator delete(block);
    }

    world::IObject* target = 0;
    if (m_freeCameraWeak)
        target = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_freeCameraWeak)->target;

    // vslot 216 = slot 54 = IObject::getNetworkable.
    const int netId = getId() + 257;                        // see the header block
    io::networkManager->addNetworkable(target->getNetworkable(), static_cast<unsigned short>(netId));
    game->confirmGhost(getId(), netId);                      // IGame +0x70

    // the vehicle side effect
    world::IObject* vehicle = 0;
    if (m_vehicleWeak)
        vehicle = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_vehicleWeak)->target;
    if (!vehicle)
        _setVehicle(0, 0);
}

// . IPlayer +0x24. `add eax, 5Ch` - it returns the address of the member, which is
// what a `const std::string&` return compiles to.
const std::string& Player::getName() const
{
    return m_playerName;
}

// . IPlayer +0x54. One load - m_playerId's offset confirmed a second way.
int Player::getId() const
{
    return m_playerId;
}

// Return the occupied seat index, or -1 when the player has no seat.
int Player::getInputId() const
{
    return m_inputId;                                        // +0x98
}

// Borrow a personal decal template; null restores the vehicle's default player tag.
void Player::setPersonalDecalTemplate(const world::IObjectTemplate* decal)
{
    m_personalDecalTemplate = decal;                         // +0x200
}

const world::IObjectTemplate* Player::getPersonalDecalTemplate() const
{
    return m_personalDecalTemplate;                          // +0x200
}

// Dedicated-image VO-help hooks are inert; the played query always returns true.
// These bodies do not implement client tutorial playback.
void Player::enableVOHelp(bool)
{
}

void Player::enableVOHelpMessage(int)
{
}

void Player::disableVOHelpMessage(int)
{
}

bool Player::getVOHelpMessagePlayed(int)
{
    return true;
}

// Rename through PlayerManager before changing the local name. Duplicate names
// are silently rejected.
void Player::setName(const std::string& name)
{
    if (world::playerManager->getPlayerFromName(name))
        return;                                     // taken - silently does nothing

    world::playerManager->renamePlayer(this, name); // before the field is updated
    m_playerName = name;
}

// Apply the set and clear masks to a word without changing the player.
unsigned int Player::updateFlags(unsigned int word, const Flag32Bits<IPlayer>& flags)
{
    return (word | flags.set) & ~flags.clear;
}

void Player::setFlags(const Flag32Bits<IPlayer>& flags)
{
    m_flags = static_cast<int>(updateFlags(static_cast<unsigned int>(m_flags), flags));
}

bool Player::testFlags(const Flag32Bits<IPlayer>& flags) const
{
    const unsigned int word = static_cast<unsigned int>(m_flags);
    if ((word & flags.set) != flags.set)
        return false;
    return (~word & flags.clear) == flags.clear;
}

// . IPlayer +0x190. One load. NOT const, unlike most of the accessor family -
// the mangling has no K, and IPlayer declares it the same way.
// What it returns was decided in the constructor, by the IID comparison recorded there: it is
// either `this` or the NetworkableBase sub-object, and in this binary always the latter.
io::INetworkable* Player::getNetworkable()
{
    return m_networkable;
}

// Reference-counted interfaces share this object's lifetime. queryInterface exposes
// IBase, IPlayer and the adjusted INetworkable subobject, but not INetworkableObject.
void Player::addRef()
{
    ++m_refs;
}

int Player::getRef()
{
    return m_refs;
}

// IT destroys through THE vtable. At zero it calls slot +0x18 - the deleting destructor - rather
// than `delete this` directly, so a subclass's D0 runs. The `this != 0` guard before that call is
// the original's and cannot fire from a member function; reproduced.
int Player::release()
{
    --m_refs;
    if (m_refs == 0)
        delete this;                // vtable +0x18, the D0 slot
    return m_refs;
}

void* Player::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == world::IID_IPlayer)
        return const_cast<Player*>(this);

    // The observed x86 +0x0C is the NetworkableBase sub-object. A native base cast retains
    // the same face on x86 and follows its compiler-selected location in the x64 port.
    if (iid == io::IID_INetworkable)
        return static_cast<io::NetworkableBase*>(const_cast<Player*>(this));

    return 0;
}

// . Loads the CID_Player global (ds:0x896ABD4) rather than an immediate.
unsigned int Player::getClassID() const
{
    return world::CID_Player;
}

// Mark the connection state. Connecting stamps the clock; disconnecting keeps
// the previous connection timestamp.
void Player::setIsConnected(bool connected)
{
    m_isConnected = connected;
    if (connected)
        m_timeConnected = g_system->getExactTime();   // ds:0x8AC0D20, vtable +0x18
}

// Apply the local auto-reload setting when GeneralSettings exists.
// Dedicated servers leave client settings unchanged.
void Player::updateClientSettings()
{
#if !DEDICATED
    if (isDedicated())
        return;
    IBase* settings;
    {
        const std::string name("GeneralSettings");           // ds:0x891314
        settings = classManager->getSingleton(name);         // +0x48 on the client table
    }
    if (settings)
        setAutoReload(static_cast<IGeneralSettings*>(settings)->getAutoReload());   // +0xE0, then +0x330
#endif
}

// . IPlayer +0x270. returns unsigned short, which is what pins the field's width -
// and the tree's own IPlayer.h had recorded "(word +0x2B2)" all along.
unsigned short Player::getVoipSessionId()
{
    return m_voipSessionId;
}

// . IPlayer +0x274. The mangling is ...setVoipSessionIdEt - `t`, unsigned short.
// GameServer::createPlayer feeds this AN int. VoipServerConnection::CreateClientSessionId returns
// an int and the result goes straight in here, so a session id above 65535 truncates silently. The
// narrowing is the original's, not this rebuild's.
void Player::setVoipSessionId(unsigned short sessionId)
{
    m_voipSessionId = sessionId;
}

// . IPlayer +0x98. One load - IPlayer.h already recorded "(Player +0x158)".
ActionBuffer* Player::getActionBuffer()
{
    return m_actionBuffer;
}

// Simulation tick counter, reset when queued player actions are cleared.
int  Player::getTickCount() const  { return m_tickCount; }   // +0xD0
void Player::addTick()             { ++m_tickCount; }         // +0xD4
void Player::subTick()             { --m_tickCount; }         // +0xD8
void Player::resetTickCounter()    { m_tickCount = 0; }       // +0xDC

// Clamp the interpolation delay to 512 milliseconds.
void Player::setInterpolationTime(unsigned int time)
{
    m_interpolationTime = time < 0x200u ? time : 0x200u;
}

unsigned int Player::getInterpolationTime() const
{
    return m_interpolationTime;
}

// . IPlayer +0x154. One byte store at +0x166 - which lands inside the eight-byte
// run this header calls m_field160[8], at index 6. Six of those eight now have meanings
// (0..2 the firing trio, 3/4 the sprint pair, 6 this, 7 read by init); only index 5 is unaccounted.
void Player::setHasSimulatedFrame(bool simulated)
{
    m_field160[6] = simulated;
}

// Report whether this frame has been simulated; collision handling consumes
// the latch before another simulation step.
bool Player::getHasSimulatedFrame() const
{
    return m_field160[6];
}

// Find the first parent control object with weapons, searching the current vehicle
// before the default vehicle. The returned interface is borrowed.
// [CHOSEN ret] getWeapons is viewed as std::vector<IObject*> and must be non-null.
namespace {

// Walk up the parent chain from `start`, returning the first IPlayerControlObject that has weapons.
world::IPlayerControlObject* armedControlObjectFrom(world::IObject* start)
{
    world::IPlayerControlObject* pco = 0;
    for (world::IObject* obj = start; obj != 0; obj = obj->getParent())
    {
        pco = static_cast<world::IPlayerControlObject*>(
                  obj->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            break;
    }
    if (!pco)
        return 0;

    // unguarded by design - see the header block
    const std::vector<world::IObject*>* weapons =
        static_cast<const std::vector<world::IObject*>*>(pco->getWeapons());
    return weapons->empty() ? 0 : pco;
}

} // anonymous namespace

world::IPlayerControlObject* Player::getWeaponVehicle()
{
    if (world::IPlayerControlObject* pco = armedControlObjectFrom(getVehicle()))
        return pco;
    return armedControlObjectFrom(getDefaultVehicle());
}

// Apply the decoded weapon snapshot to the remote vehicle, including fire/reload
// state. Heat is applied only to weapons that use it; the spare-magazine flag
// selects one or two magazines.
void Player::forceSetRemoteClientAmmo(unsigned int firedMask)
{
    if (!getVehicle())
        return;
    world::IPlayerControlObject* const pco = getWeaponVehicle();
    if (!pco)
        return;

    for (int w = 0; w <= 2; ++w)
    {
        if (((1u << w) & firedMask) == 0)
            continue;
        world::IObject* const weapon = pco->getActiveWeapon(w);
        if (!weapon)
            continue;
        world::IWeaponObject* const weaponInterface =
            static_cast<world::IWeaponObject*>(weapon->queryInterface(world::IID_IWeaponObject));
        if (!weaponInterface)
            continue;

        weaponInterface->setAmmoInMag(0, slotDword(&m_netUpdateBuffer[m_next], 24 * w + 80));
        weaponInterface->setFireRate(
            static_cast<world::FireRate>(slotDword(&m_netUpdateBuffer[m_next], 24 * w + 84)));
        if (weaponInterface->getUsesHeat())
            weaponInterface->setHeat(slotFloat(&m_netUpdateBuffer[m_next], 24 * w + 92));
        if (m_netUpdateBuffer[m_next].buff[24 * w + 88])
            weaponInterface->setCurrentNrOfMags(2);
        else
            weaponInterface->setCurrentNrOfMags(1);
        weaponInterface->setFlags(
            static_cast<unsigned int>(slotDword(&m_netUpdateBuffer[m_next], 24 * w + 96)));
    }
}

// Snapshot enabled weapon slots for ghost replication; missing slots are cleared.
// The spare-magazine flag records whether more than one magazine remains.
// [FRONTIER] Specialized weapon components remain incomplete (RW-6b);
// componentless weapons retain their default values.
void Player::updateGhostFiringState()
{
    world::IPlayerControlObject* weaponVehicle = getWeaponVehicle();
    if (!weaponVehicle)
    {
        std::memset(m_ghostFire20C, 0, 12);
        std::memset(m_ghostFire218, 0, 12);
        std::memset(m_ghostFire224, 0,  3);
        std::memset(m_ghostFire228, 0, 12);
        std::memset(m_ghostFire234, 0, 12);
        return;
    }

    for (int i = 0; i <= 2; ++i)
    {
        world::IObject* weapon = weaponVehicle->getActiveWeapon(i);
        world::IWeaponObject* w = weapon ? static_cast<world::IWeaponObject*>(
            weapon->queryInterface(world::IID_IWeaponObject)) : 0;

        // the order is the ELF's and the short-circuiting is load bearing: no weapon, no
        // IWeaponObject, disabled, or a zero +0xA8 all fall to the same else
        if (w != 0 && (weapon->getFlags() & 1) == 0 && w->getMagSize() != 0)
        {
            m_ghostFire20C[i] = w->getAmmoInMag(0);
            m_ghostFire218[i] = w->getFireRate();
            m_ghostFire224[i] = (w->getCurrentNrOfMags() > 1) ? 1 : 0; // > 1, see the block
            m_ghostFire228[i] = w->getHeat();
            m_ghostFire234[i] = w->getFlags();
        }
        else
        {
            m_ghostFire20C[i] = 0;
            m_ghostFire218[i] = 0;
            m_ghostFire224[i] = 0;
            m_ghostFire228[i] = 0.0f;
            m_ghostFire234[i] = 0;
        }
    }
}

// Consumes and clears the hit-indicator latch despite being const.
bool Player::getHitIndicator() const
{
    const unsigned char value = m_hitIndicator;
    m_hitIndicator = 0;
    return value != 0;
}

// Store the normalized horizontal hit angle and repeat it in two state syncs.
// The angle is not wrapped positive and NaNs propagate.
// [PLATFORM] The return type is float; declaring it void leaks the x86 FP stack.
float Player::updateHitFromDirection(const Vec3& direction)
{
    Vec3 horizontal = { direction.x, 0.0f, direction.z };

    const float lengthSquared =
        horizontal.z * horizontal.z
        + (horizontal.x * horizontal.x + horizontal.y * horizontal.y);

    if (!(std::fabs(lengthSquared - 1.0f) < FLT_EPSILON))
    {
        if (std::fabs(lengthSquared) < FLT_EPSILON)
        {
            horizontal.x = 0.0f;
            horizontal.y = 0.0f;
            horizontal.z = 0.0f;
        }
        else
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            horizontal.x *= inverseLength;
            horizontal.y *= inverseLength;
            horizontal.z *= inverseLength;
        }
    }

    const float angle = std::fmod(std::atan2(horizontal.x, horizontal.z), 6.2831855f);
    m_hitFromDirection = angle;
    m_someHitCounter = 2;
    return angle;
}

// Return the last hit angle without consuming or clamping it.
float Player::getHitFromDirection() const
{
    return m_hitFromDirection;                                          // ELF +0x188 / PE +0x1B8
}

// Replace the camera-shake amount and repeat it in two state syncs.
// This does not accumulate, clamp or decay the value.
void Player::updateCameraShake(float amount)
{
    m_cameraShake = amount;
    m_field194 = 2;
}

// Return the last camera-shake amount without consuming it.
float Player::getCameraShake() const
{
    return m_cameraShake;                                               // ELF +0x190 / PE +0x1C0
}

// forceSetClientAmmo - ELF 0x80ECF80, 221 B; client PE 0x4FF770 and dedicated PE 0x476610,
// both 183 B. setStateSync calls this after decoding the three weapon slots. The initial
// getVehicle() is a gate only; the weapon interface is then found independently through
// getWeaponVehicle(). If that first gate is null, retail clears only the ammo-in-mag triple.
void Player::forceSetClientAmmo(unsigned int slotMask)
{
    if (!getVehicle())
    {
        std::memset(m_ammoCountWeap1, 0, sizeof(m_ammoCountWeap1));
        return;
    }

    world::IPlayerControlObject* const pco = getWeaponVehicle();
    if (!pco)
        return;

    for (int slot = 0; slot <= 2; ++slot)
    {
        if (((1u << slot) & slotMask) == 0)
            continue;

        world::IObject* const object = pco->getActiveWeapon(slot);
        if (!object)
            continue;

        world::IWeaponObject* const weapon = static_cast<world::IWeaponObject*>(
            object->queryInterface(world::IID_IWeaponObject));
        if (!weapon)
            continue;

        world::WeaponState state;
        state.ammoInMag       = m_ammoCountWeap1[slot];
        state.fireRate        = static_cast<world::FireRate>(m_array1B4[slot]);
        state.currentNrOfMags = m_currNbMagsWeap1[slot];
        state.camera          = m_array1C0[slot];
        state.flags           = static_cast<unsigned int>(m_array1F0[slot]);
        state.heat            = m_weaponHeat[slot];
        weapon->setStateSync(state);
    }
}

// updateWeaponLimits - ELF 0x80ED060, 277 B; client PE 0x4FE400 and dedicated PE 0x475540,
// both 183 B. The two getMaxNrOfMags calls are intentional and shipped. Missing control/weapon
// interfaces leave the corresponding cached values untouched.
void Player::updateWeaponLimits(world::IObject* object)
{
    if (!object)
        return;

    world::IPlayerControlObject* pco = 0;
    for (world::IObject* walk = object; walk != 0; walk = walk->getParent())
    {
        pco = static_cast<world::IPlayerControlObject*>(
            walk->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            break;
    }
    if (!pco)
        return;

    for (int slot = 0; slot <= 2; ++slot)
    {
        world::IObject* const objectForSlot = pco->getActiveWeapon(slot);
        world::IWeaponObject* const weapon = objectForSlot
            ? static_cast<world::IWeaponObject*>(
                  objectForSlot->queryInterface(world::IID_IWeaponObject))
            : 0;
        if (!weapon)
            continue;

        m_maxNrOfMagsWeap1[slot] = weapon->getMaxNrOfMags();
        const int maxMagsProbe = weapon->getMaxNrOfMags();
        m_currNbMagsWeap1[slot] =
            maxMagsProbe != -1 ? weapon->getCurrentNrOfMags() : -1;
        m_magSizeWeap1[slot] = weapon->getMagSize();
        if (m_currNbMagsWeap1[slot] > m_maxNrOfMagsWeap1[slot])
            m_currNbMagsWeap1[slot] = m_maxNrOfMagsWeap1[slot];
    }
}

// Refresh only the masked weapon slots, using object's control parent when supplied.
// Without a control object only ammo is cleared. Invalid weapon slots clear ammo
// and active state while retaining the other fields. Magazine counts are clamped
// to each weapon's maximum.
void Player::updateFiringState(unsigned int slotMask, world::IObject* object)
{
    world::IPlayerControlObject* pco = 0;
    if (object)
    {
        for (world::IObject* walk = object; walk != 0; walk = walk->getParent())
        {
            pco = static_cast<world::IPlayerControlObject*>(
                      walk->queryInterface(world::IID_IPlayerControlObject));
            if (pco)
                break;
        }
    }
    else
    {
        pco = getWeaponVehicle();
    }

    if (!pco)
    {
        std::memset(m_ammoCountWeap1, 0, 12);            // only the ammo triple, as shipped
        return;
    }

    for (int slot = 0; slot <= 2; ++slot)
    {
        if (((1u << slot) & slotMask) == 0)
            continue;

        world::IObject* const weapon = pco->getActiveWeapon(slot);              // +0x70
        world::IWeaponObject* const w = weapon ? static_cast<world::IWeaponObject*>(
            weapon->queryInterface(world::IID_IWeaponObject)) : 0;

        if (w != 0 && (weapon->getFlags() & 1) == 0)     // bit 0 = disabled, as the ghost twin
        {
            m_weaponSlotActive[slot] = true;
            m_ammoCountWeap1[slot]   = w->getAmmoInMag(0);
            const int magsProbe      = w->getMaxNrOfMags();                     // call #1
            m_currNbMagsWeap1[slot]  = (magsProbe != -1) ? w->getCurrentNrOfMags() : -1;
            m_array1B4[slot]         = w->getFireRate();

            // [TEMP] fire-mode wire tracer, armed by BF2_TRACE_FIRERATE=1. Reports the value the
            // HUD is actually told, per slot, on change. [RETIRE-WHEN: firerate-toggle-diagnosed]
            {
                static int frArmed = -1;
                if (frArmed < 0)
                    frArmed = std::getenv("BF2_TRACE_FIRERATE") ? 1 : 0;
                const int frId = getId();
                if (frArmed && frId >= 0 && frId < 64)
                {
                    // keyed by player as well as slot: one shared latch made two players at
                    // different fire rates print on every tick.
                    static int frLast[64][3];
                    static bool frInit = false;
                    if (!frInit)
                    {
                        frInit = true;
                        for (int a = 0; a < 64; ++a)
                            for (int b = 0; b < 3; ++b)
                                frLast[a][b] = -1;
                    }
                    if (frLast[frId][slot] != m_array1B4[slot])
                    {
                        frLast[frId][slot] = m_array1B4[slot];
                        std::fprintf(stderr,
                                     "[FRTRACE] cache p%d slot=%d fireRate=%d weapon=%s\n",
                                     frId, slot, m_array1B4[slot],
                                     weapon->getTemplate()->getName().c_str());
                        std::fflush(stderr);
                    }
                }
            }
            m_array1C0[slot]         = w->getZoom();
            m_array1F0[slot]         = w->getFlags();
            m_weaponHeat[slot]       = w->getHeat();
            m_maxNrOfMagsWeap1[slot] = w->getMaxNrOfMags();                     // call #2, as shipped
            m_magSizeWeap1[slot]     = w->getMagSize();
            if (m_currNbMagsWeap1[slot] > m_maxNrOfMagsWeap1[slot])
                m_currNbMagsWeap1[slot] = m_maxNrOfMagsWeap1[slot];
        }
        else
        {
            m_ammoCountWeap1[slot] = 0;
            m_weaponSlotActive[slot] = false;
        }
    }
}

// Write control state to a non-null stream, consuming hit/shake repeat counters.
// Weapon state is included only when object is supplied. Health reads the default
// vehicle weak target directly. Returns true; preserve the ranged field widths.
bool Player::getStateSync(io::BitStream* stream, world::IObject* object)
{
    typedef world::object_detail::LayoutAccess L;
    const unsigned int stateSyncStart = stream->getBitPosition();

    {
        unsigned int bit = getHitIndicator() ? 1u : 0u;          // virtual - and it consumes
        stream->writeBits(&bit, 1);
    }
    {
        unsigned int bit = m_isGivingReplenish ? 1u : 0u;
        stream->writeBits(&bit, 1);
    }
    {
        unsigned int bit = m_isGivingAbility ? 1u : 0u;
        stream->writeBits(&bit, 1);
    }
    {
        unsigned int bit = m_isRepairing ? 1u : 0u;
        stream->writeBits(&bit, 1);
    }

    stream->writeBits(&m_squadInviteMask, 8);

    const bool hasHitDirection = m_someHitCounter != 0;
    {
        unsigned int bit = hasHitDirection ? 1u : 0u;
        stream->writeBits(&bit, 1);
    }
    if (hasHitDirection)
    {
        double scaled = (m_hitFromDirection / 6.2831855 + 1.0) * 0.5;
        if (scaled < 0.0)
            scaled = 0.0;
        else if (scaled > 1.0)
            scaled = 1.0;
        unsigned int direction = static_cast<unsigned int>(scaled * 255.0);
        stream->writeBits(&direction, 8);
        --m_someHitCounter;
    }

    const bool hasCameraShake = m_field194 != 0;
    {
        unsigned int bit = hasCameraShake ? 1u : 0u;
        stream->writeBits(&bit, 1);
    }
    if (hasCameraShake)
    {
        float shake = m_cameraShake;
        if (shake >= 25.0f)
            shake = 25.0f;
        double scaled = shake / 25.0;
        if (scaled < 0.0)
            scaled = 0.0;
        else if (scaled > 1.0)
            scaled = 1.0;
        unsigned int amount = static_cast<unsigned int>(scaled * 255.0);
        stream->writeBits(&amount, 8);
        --m_field194;
    }

    if (object)
    {
        updateFiringState(static_cast<unsigned int>(-1), object);

        world::IPlayerControlObject* pco = 0;
        for (world::IObject* walk = object; walk != 0; walk = walk->getParent())
        {
            pco = static_cast<world::IPlayerControlObject*>(
                      walk->queryInterface(world::IID_IPlayerControlObject));
            if (pco)
                break;
        }

        for (int slot = 0; slot <= 2; ++slot)
        {
            const bool active = m_weaponSlotActive[slot];
            {
                unsigned int bit = active ? 1u : 0u;
                stream->writeBits(&bit, 1);
            }
            if (!active)
            {
                // [TEMP] fire-mode send tracer. An inactive slot carries NO fire-rate bits, so the
                // client keeps its previous value. [RETIRE-WHEN: firerate-toggle-diagnosed]
                static int frInactiveArmed = -1;
                if (frInactiveArmed < 0)
                    frInactiveArmed = std::getenv("BF2_TRACE_FIRERATE") ? 1 : 0;
                if (frInactiveArmed && slot == 0)
                {
                    static bool frInactiveSeen = false;
                    if (!frInactiveSeen)
                    {
                        frInactiveSeen = true;
                        std::fprintf(stderr,
                                     "[FRTRACE] SENT slot=0 INACTIVE - no fire rate on the wire"
                                     " (player=%d)\n", getId());
                        std::fflush(stderr);
                    }
                }
                continue;
            }

            {
                unsigned int scratch = static_cast<unsigned int>(m_array1C0[slot]);
                stream->writeBits(&scratch, 2);
            }
            {
                unsigned int scratch = static_cast<unsigned int>(m_array1F0[slot]);
                stream->writeBits(&scratch, 2);
            }
            {
                unsigned int scratch = static_cast<unsigned int>(m_array1B4[slot]);
                stream->writeBits(&scratch, 2);

                // [TEMP] fire-mode send tracer, armed by BF2_TRACE_FIRERATE=1. Proves what
                // getStateSync actually put on the wire, as opposed to what updateFiringState
                // cached. [RETIRE-WHEN: firerate-toggle-diagnosed]
                static int frSendArmed = -1;
                if (frSendArmed < 0)
                    frSendArmed = std::getenv("BF2_TRACE_FIRERATE") ? 1 : 0;
                const int frSendId = getId();
                if (frSendArmed && slot == 0 && frSendId >= 0 && frSendId < 64)
                {
                    static unsigned int frSendLast[64];
                    static bool frSendInit = false;
                    if (!frSendInit)
                    {
                        frSendInit = true;
                        for (int a = 0; a < 64; ++a)
                            frSendLast[a] = 0xFFFFFFFFu;
                    }
                    if (frSendLast[frSendId] != scratch)
                    {
                        frSendLast[frSendId] = scratch;
                        std::fprintf(stderr, "[FRTRACE] SENT p%d slot=0 fireRate=%u\n",
                                     frSendId, scratch);
                        std::fflush(stderr);
                    }
                }
            }

            // ammo + 1, ranged 0..1023 - the inlined BitStream.inl:59 coder
            {
                const int value = m_ammoCountWeap1[slot] + 1;
                const int lower = 0;
                const int upper = 1023;
                if (value >= lower && value <= upper)
                {
                    const unsigned int range = static_cast<unsigned int>(upper - lower);
                    unsigned int bits = 0;
                    do
                        ++bits;
                    while (range + 1 > (1u << bits) - 1);
                    unsigned int scratch = static_cast<unsigned int>(value - lower);
                    stream->writeBits(&scratch, bits);
                }
                else
                {
                    stream->setError(4);
                    Debug(DEBUGTYPE_7, "IO/BitStream.inl", 59, "")
                        << "(fatal error) Value out of bounds val: " << value
                        << " lower " << lower << " upper: " << upper;
                }
            }

            // mags + 1, ranged 0..511
            {
                const int value = m_currNbMagsWeap1[slot] + 1;
                const int lower = 0;
                const int upper = 511;
                if (value >= lower && value <= upper)
                {
                    const unsigned int range = static_cast<unsigned int>(upper - lower);
                    unsigned int bits = 0;
                    do
                        ++bits;
                    while (range + 1 > (1u << bits) - 1);
                    unsigned int scratch = static_cast<unsigned int>(value - lower);
                    stream->writeBits(&scratch, bits);
                }
                else
                {
                    stream->setError(4);
                    Debug(DEBUGTYPE_7, "IO/BitStream.inl", 59, "")
                        << "(fatal error) Value out of bounds val: " << value
                        << " lower " << lower << " upper: " << upper;
                }
            }

            // the heat byte, behind the weapon's own +0x30 gate
            if (pco)
            {
                world::IObject* const weapon = pco->getActiveWeapon(slot);      // +0x70
                if (weapon)
                {
                    world::IWeaponObject* const w = static_cast<world::IWeaponObject*>(
                        weapon->queryInterface(world::IID_IWeaponObject));
                    if (w && w->getUsesHeat())
                    {
                        float heat = m_weaponHeat[slot];
                        if (heat < 0.0f)
                            heat = 0.0f;
                        else if (heat > 6.0f)
                            heat = 6.0f;
                        double scaled = heat / 6.0;
                        if (scaled > 1.0)
                            scaled = 1.0;
                        unsigned int quantised = static_cast<unsigned int>(scaled * 255.0);
                        stream->writeBits(&quantised, 8);
                    }
                }
            }
        }
    }

    stream->writeBits(&m_spawnAtTick, 32);

    {
        unsigned int bit = m_isManDown ? 1u : 0u;
        stream->writeBits(&bit, 1);
    }

    int healthByte = 0;
    {
        unsigned char* const block = static_cast<unsigned char*>(m_defaultVehicleWeak);
        world::IObject* const soldier =
            block ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target : 0;   // plain +4, no staleness

        if (soldier &&
            soldier->getTemplate()->getClassID() == world::CID_SoldierTemplate)  // template +0x10
        {
            world::ArmorGLComp* const armor =
                static_cast<world::ArmorGLComp*>(L::armorComponent(soldier));   // object +0x3C
            if (armor)
            {
                float hitPoints = armor->getHitPoints();
                if (hitPoints <= 0.0f)
                    hitPoints = 0.0f;
                // NO guard ON maxHitPoints - a template with 0 there divides by zero here. The
                // ELF has no guard either; recorded, not corrected.
                healthByte = static_cast<int>(
                    std::ceil(hitPoints) / armor->getMaxHitPoints() * 255.0f);
            }
        }
    }
    if (healthByte < 0)
    {
        unsigned int negative = 1;
        stream->writeBits(&negative, 1);
        unsigned int magnitude = static_cast<unsigned int>(-healthByte);
        stream->writeBits(&magnitude, 8);
    }
    else
    {
        unsigned int negative = 0;
        stream->writeBits(&negative, 1);
        unsigned int magnitude = static_cast<unsigned int>(healthByte);
        stream->writeBits(&magnitude, 8);
    }

    {
        unsigned int bit = m_isInsideCP ? 1u : 0u;
        stream->writeBits(&bit, 1);
    }
    {
        unsigned int bit = m_field160[3] ? 1u : 0u;      // +0x163 sprintState
        stream->writeBits(&bit, 1);
    }
    {
        unsigned int bit = m_field160[4] ? 1u : 0u;      // +0x164 sprintStateLastTick
        stream->writeBits(&bit, 1);
    }

    return true;
}

// Read control state; time is unused. A cleared hit bit does not clear a pending hit.
// Weapon data is read only for object's control parent; heat is present only for
// weapons that use it. Health uses the default weak target without a stale check
// or a maximum-health guard. Reads continue after a stream error; the tail clears
// the error and returns false.
bool Player::setStateSync(io::BitStream* stream, world::IObject* object, double /*time*/)
{
    typedef world::object_detail::LayoutAccess L;

    unsigned int value = 0;
    stream->readBits(&value, 1);
    if (value == 1)
        setHitIndicator(true);

    value = 0;
    stream->readBits(&value, 1);
    m_isGivingReplenish = value == 1;

    value = 0;
    stream->readBits(&value, 1);
    m_isGivingAbility = value == 1;

    value = 0;
    stream->readBits(&value, 1);
    m_isRepairing = value == 1;

    value = 0;
    stream->readBits(&value, 8);
    setSquadInviteMask(value);

    value = 0;
    stream->readBits(&value, 1);
    if (value == 1)
    {
        unsigned int directionByte = 0;
        stream->readBits(&directionByte, 8);

        double direction = static_cast<double>(directionByte) / 255.0;
        direction = direction + direction - 1.0;
        if (std::fabs(direction) <= 0.0078431377f)
            direction = 0.0;

        const bool firstDirection = m_someHitCounter == 0;
        m_hitFromDirection = static_cast<float>(direction * 6.2831855f);
        if (firstDirection)
            eventManager->postEvent(ECHud, 52, 0, 0.0f);
        m_someHitCounter = 2;
    }
    if (m_someHitCounter)
        --m_someHitCounter;

    value = 0;
    stream->readBits(&value, 1);
    if (value == 1)
    {
        unsigned int shakeByte = 0;
        stream->readBits(&shakeByte, 8);

        const bool firstShake = m_field194 == 0;
        m_cameraShake = static_cast<float>(
            static_cast<double>(shakeByte) / 255.0 * 25.0);
        if (firstShake && getCamera())
            getCamera()->handleExplosion(m_cameraShake);       // shipped repeated virtual call
        m_field194 = 2;
    }
    if (m_field194)
        --m_field194;

    world::IPlayerControlObject* pco = 0;
    for (world::IObject* walk = object; walk != 0; walk = walk->getParent())
    {
        pco = static_cast<world::IPlayerControlObject*>(
            walk->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            break;
    }

    if (pco)
    {
        world::IObject* const pcoObject = static_cast<world::IObject*>(
            pco->queryInterface(world::IID_IObject));
        updateWeaponLimits(pcoObject);

        for (int slot = 0; slot <= 2; ++slot)
        {
            value = 0;
            stream->readBits(&value, 1);
            m_weaponSlotActive[slot] = value == 1;
            if (!m_weaponSlotActive[slot])
                continue;

            value = 0;
            stream->readBits(&value, 2);
            m_array1C0[slot] = static_cast<int>(value);

            value = 0;
            stream->readBits(&value, 2);
            m_array1F0[slot] = static_cast<int>(value);

            value = 0;
            stream->readBits(&value, 2);
            m_array1B4[slot] = static_cast<int>(value);

            unsigned int ammoBits = 0;
            do
                ++ammoBits;
            while (((1u << ammoBits) - 1u) < 0x400u);
            value = 0;
            stream->readBits(&value, ammoBits);
            m_ammoCountWeap1[slot] = static_cast<int>(value) - 1;

            unsigned int magazineBits = 0;
            do
                ++magazineBits;
            while (((1u << magazineBits) - 1u) < 0x200u);
            value = 0;
            stream->readBits(&value, magazineBits);
            m_currNbMagsWeap1[slot] = static_cast<int>(value) - 1;

            world::IObject* const weaponObject = pco->getActiveWeapon(slot);
            if (weaponObject)
            {
                world::IWeaponObject* const weapon = static_cast<world::IWeaponObject*>(
                    weaponObject->queryInterface(world::IID_IWeaponObject));
                if (weapon)
                {
                    if (weapon->getUsesHeat())
                    {
                        value = 0;
                        stream->readBits(&value, 8);
                        m_weaponHeat[slot] = static_cast<float>(
                            static_cast<double>(value) / 255.0 * 6.0);
                    }
                    else
                    {
                        m_weaponHeat[slot] = 0.0f;
                    }
                }
            }
        }

        unsigned int slotMask = 0;
        for (int slot = 0; slot <= 2; ++slot)
            slotMask |= static_cast<unsigned int>(m_weaponSlotActive[slot]) << slot;
        forceSetClientAmmo(slotMask);
    }

    value = 0;
    stream->readBits(&value, 32);
    m_spawnAtTick = value;

    value = 0;
    stream->readBits(&value, 1);
    m_isManDown = value == 1;

    value = 0;
    stream->readBits(&value, 1);
    const bool negativeHealth = value == 1;
    value = 0;
    stream->readBits(&value, 8);
    const int healthByte = negativeHealth
        ? -static_cast<int>(value)
        : static_cast<int>(value);

    unsigned char* const block = static_cast<unsigned char*>(m_defaultVehicleWeak);
    world::IObject* const soldier =
        block ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target : 0;   // plain +4, no staleness
    if (soldier &&
        soldier->getTemplate()->getClassID() == world::CID_SoldierTemplate)
    {
        world::ArmorGLComp* const armor =
            static_cast<world::ArmorGLComp*>(L::armorComponent(soldier));
        if (armor)
        {
            const float hitPoints = armor->getMaxHitPoints() *
                (static_cast<float>(healthByte) / 255.0f);
            armor->setHitPoints(hitPoints);
        }
    }

    value = 0;
    stream->readBits(&value, 1);
    m_isInsideCP = value == 1;

    value = 0;
    stream->readBits(&value, 1);
    m_field160[3] = value == 1;

    value = 0;
    stream->readBits(&value, 1);
    m_field160[4] = value == 1;

    if (!stream->hasError())
        return true;

    stream->clearError();
    return false;
}

// Return the default vehicle, releasing and clearing a weak target marked invalid.
world::IObject* Player::getDefaultVehicle()
{
    unsigned char* block = static_cast<unsigned char*>(m_defaultVehicleWeak);

    world::IObject* target = 0;
    if (block)
        target = reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target;
    if (!target)
        return 0;

    // IObject +0x04, bit 0x20 - the "this object is gone" flag.
    if (!(world::object_detail::LayoutAccess::flags(target) & 0x20))
        return target;                                  // the normal path

    // the drop. A WeakPtr assignment to null, written out inline.
    if (block)
    {
        int* refcount = reinterpret_cast<int*>(block);
        if (--(*refcount) == 0)
            ::operator delete(block);
    }
    m_defaultVehicleWeak = 0;

    // the inert second release - see the header block
    return 0;
}

// . IPlayer +0x64. AND ITS absence WAS A silent BUG, not a frontier: IPlayer
// carries a non-pure default returning false, so an undeclared override here linked and RAN,
// answering false for every AI player instead of faulting at RVA 0. Caught by reading the vtable
// slot rather than by a crash.
bool Player::getIsAIPlayer() const
{
    return m_isAI;
}

// . IPlayer +0x78. The plain accessor PlayerManager::getNumberOfAliveHumanPlayers
// pairs with the one above.
bool Player::getIsConnected() const
{
    return m_isConnected;
}


namespace {

    // Create the team-verification event using adjusted interface pointers.
    GameEvent* createVerifyPlayerTeamEvent(world::IPlayer* localPlayer, Player* player)
    {
        return new VerifyPlayerTeamEvent(localPlayer, player);
    }

} // namespace

// Set alive state and clear the trace player id. Setting false also clears man-down
// directly, without its nametag notification; setting true leaves man-down unchanged.
void Player::setIsAlive(bool alive)
{
    if (!alive)
        m_isManDown = false;                              // byte +0xCC  direct, NOT setIsManDown
    m_isAlive = alive;                                    // byte +0xB9
    m_tracePlayerId = -1;                                 // int  +0x314 on both arms
}

// . IPlayer +0xFC.
void Player::setIsManDown(bool manDown)
{
    if (manDown == m_isManDown)                           // byte +0xCC - no-change early-out
        return;

    // before the store, and unguarded - the same shape setTeam uses. On a dedicated server this is
    // SingletonServer::init's NullNametagManager fallback, so it is a real call into a real object.
    SingletonServer::m_iNametagManager->addTouchItem(this);            // +0x1C

    m_isManDown = manDown;
    if (manDown)
        m_manDownAtTick = WorldPref::getTickCount();      // int +0xD0 - stamped only on the raise
}

// . IPlayer +0x70.
void Player::setSuicide(bool suicide)
{
    m_suicide = suicide;                                  // byte +0xBA
}

// . IPlayer +0x74.
bool Player::getSuicide() const
{
    return m_suicide;                                     // byte +0xBA
}

// . IPlayer +0x300. See the cluster note: setIsAlive erases this.
void Player::setTracePlayerId(int id)
{
    m_tracePlayerId = id;                                 // int +0x314
}

// . IPlayer +0x304. NOT const in the ELF either - the mangling has no K.
int Player::getTracePlayerId()
{
    return m_tracePlayerId;                               // int +0x314
}

bool Player::getIsSquadLeader() const
{
    return m_isSquadLeader;                               // byte +0xF5
}

// Normalize the public squad id to 16 bits before applying membership side effects.
void Player::setSquadId(unsigned int id)
{
    setSquadId(static_cast<unsigned short>(id));
}


void Player::setSquadId(unsigned short id)
{
    m_squadId = id;                                      // ELF +0xF0 / PE +0x10C

    if (SingletonServer::m_iNametagManager)              // ELF ds:0x8AB0980 / PE ds:0x841950
    {
        world::IPlayer* const localPlayer =
            world::playerManager->getLocalHumanPlayer();
        if (localPlayer && getId() == localPlayer->getId())
            static_cast<ISquadManager*>(squadManager)->updateOrderMarkers(localPlayer);
    }

    if (gameLogic->isServer() && !m_isAI)
        VoipServerConnection::GetInstance()->UpdatePlayerRouting(this);
}

//  / PE 0x4746B0, 24 B. The bool is explicitly normalised to an int.
void Player::setIsSquadLeader(bool leader)
{
    setIsSquadLeader(leader ? 1 : 0);
}


void Player::setIsSquadLeader(int leader)
{
    m_isSquadLeader = leader != 0;                       // ELF +0xF5 / PE +0x111

    if (gameLogic->isServer() && !m_isAI)
        VoipServerConnection::GetInstance()->UpdatePlayerRouting(this);

    static_cast<ISquadManager*>(squadManager)->updateOrderMarkers(this);
}

//  / PE 0x4746D0, 24 B. The bool is explicitly normalised to an int.
void Player::setIsCommander(bool commander)
{
    setIsCommander(commander ? 1 : 0);
}


void Player::setIsCommander(int commander)
{
    const bool wasCommander = m_isCommander;

    if (commander)
        m_commanderReapplyWaitTime = 0.0;

    if (wasCommander && !commander)
    {
        setLastTimeCommander(g_system->getExactTime());
        if (gameLogic->isServer())
            static_cast<ISquadManager*>(squadManager)->setCommanderToSoldierMode(getId());
    }

    m_isCommander = commander != 0;                       // ELF +0xF4 / PE +0x110

    if (gameLogic->isServer())
        VoipServerConnection::GetInstance()->UpdatePlayerRouting(this);
}

// . IPlayer +0x10C. The same shape one field over, and the very next call in
// spawnPlayer's squad arm - which the test above currently skips, so it is unexercised too.
int Player::getSquadId() const
{
    return m_squadId;                                     // int +0xF0
}

// . IPlayer +0x6C.
bool Player::getIsAlive() const
{
    return m_isAlive;                                     // byte +0xB9
}

// Returns this player's remote-state flag.
bool Player::getIsRemote() const
{
    return m_field160[5];                                 // byte +0x165
}

// . IPlayer +0x58. Its twin, and the reason the field is [5] of that array.
void Player::setIsRemote(bool remote)
{
    m_field160[5] = remote;                               // byte +0x165
}

// . IPlayer +0x2B0. A plain store into +0x2D8.
void Player::setProfileId(int id)
{
    m_profileId = id;
}

// . IPlayer +0x264. A std::string::assign into +0x2AC (m_peerAddress).
void Player::setPeerAddress(const std::string& address)
{
    m_peerAddress.assign(address);
}

// . IPlayer +0x268 (const). Hands back the stored dotted-quad std::string by
// reference (+0x2xx) - no copy, exactly as the ELF returns &m_peerAddress.
const std::string& Player::getPeerAddress() const
{
    return m_peerAddress;
}

// . IPlayer +0x2AC. Returns the GameSpy profile id (+0x2D8). PlayerManager::
// getPlayerFromProfileId scans every player through this on each join to catch a reconnect.
int Player::getProfileId()
{
    return m_profileId;                                  // +0x2D8
}

// . IPlayer +0x290. Sets the flag, then - client-only - relays it to the server.
// THE relay IS dead ON A dedicated server. It fires only when `this` is the local human player
// (there is none on a dedicated server) AND `game` is NOT an IGameServer (queryInterface returns
// null - i.e. this process is a client). Reproduced in full; on the server it reduces to the store.
void Player::setVoipEnabled(bool enabled)
{
    m_voipEnabled = enabled;

    void* const asGameServer = game->queryInterface(IID_IGameServer);   // non-null iff game IS a server
    if (game
        && this == world::playerManager->getLocalHumanPlayer()
        && !asGameServer)
    {
        game->sendGameEvent(0xFFFFFFFFu, new VoipOnOffEvent(m_playerId, m_voipEnabled));   // connection id -1
    }
}

// . IPlayer +0x26C. The bare flag read; unlike setVoipEnabled it has no client relay.
bool Player::getVoipEnabled()
{
    return m_voipEnabled;
}

// . IPlayer +0x25C. Exactly two std::map assignments: the profile-unlock pair at
// +0x278/+0x284 receives the caller's two-map array. Retail performs no null check.
void Player::setUnlocks(std::map<int, int>* unlocks)
{
    m_unlockMaps[0] = unlocks[0];
    m_unlockMaps[1] = unlocks[1];
}

// . IPlayer +0x260. Returns the base of the unlock-map array at +0x278 (m_unlockMaps);
// the wire (UnlockEvent) reads the first two maps. ServerConnection::clientSendPlayerDatabase hands
// it to UnlockEvent for the joining player.
std::map<int, int>* Player::getUnlocks()
{
    return m_unlockMaps;                                  // array base = &m_unlockMaps[0], i.e. +0x278
}

// Store a chosen unlock level keyed by kit number. Chosen maps use slot + 1;
// profile unlock maps instead use team - 1 and the kit's creation index.
// Unlock queries require a valid team/kit template.
void Player::setChosenUnlockLevel(int kit, int slot, int level)
{
    m_unlockMaps[slot + 1][kit] = level;                  // +0x284 + 12*slot, map::operator[]
}

int Player::getUnlockLevel(int kit, int slot)
{
    // no null check on the template - see the block above.
    const world::KitTemplate* const kitTemplate =
        static_cast<world::KitTemplate*>(gameLogic->getKitTemplate(slot, kit));   // IGameLogic +0x144

    const std::map<int, int>& unlocks = m_unlockMaps[slot - 1];       // +0x26C + 12*slot
    const std::map<int, int>::const_iterator it = unlocks.find(kitTemplate->m_creationIndex);
    if (it == unlocks.end())
        return 0;
    return it->second;
}

bool Player::isUnlocked(int kit, int slot, int level)
{
    world::KitTemplate* const kitTemplate =
        static_cast<world::KitTemplate*>(gameLogic->getKitTemplate(slot, kit));
    const int creationIndex = kitTemplate->m_creationIndex; // unguarded, before the map lookup
    const std::map<int, int>& unlocks = m_unlockMaps[slot - 1];
    const std::map<int, int>::const_iterator it = unlocks.find(creationIndex);
    if (it == unlocks.end() || it->second < level)
        return false;
    return kitTemplate->getHighestUnlockLevel() >= level;
}

//  / PE 0x4753D0 (secondary IConsoleSaveable face, this adjustment -8).
// The second flag read supplies the emitted value; it must not be replaced by constant true.
bool Player::makeScript(IStream* stream)
{
    std::ostringstream script;
    const std::string& name = getName();
    const std::string& className = world::playerManager->getClassNameFromPlayerClassID(getClassID());
    script << "Player.create " << className << " " << name << std::endl;
    if (testFlags(g_playerFlagBit0))
        script << "Player.isAi " << testFlags(g_playerFlagBit0) << std::endl;
    if (testFlags(g_playerFlagBit1))
        script << "Player.isNetwork " << testFlags(g_playerFlagBit1) << std::endl;
    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

//  / PE 0x472650. This API emits the shipped Debug record and changes no setting.
void Player::setViewDistance(float)
{
    Debug(DEBUGTYPE_6, "Game/GameLogic/Player.cpp", 2394, "")
        << "Cannot change view distance on player. Set globally in BF2Render.";
}

//  / PE 0x4726D0. The global setting is an integer, default 500.
float Player::getViewDistance() const
{
    return static_cast<float>(settings->intGet("GSViewDistance", 500));
}

//  / PE 0x472A90. NametagManager slots differ: ELF +0x1C / PE +0x30.
void Player::doAddTouchItem(world::IPlayer* other)
{
    if (SingletonServer::m_iNametagManager)
        SingletonServer::m_iNametagManager->addTouchItem(other);
}

//  / PE 0x472BC0. Both return a string by value, empty on success.
std::string Player::moveTo(const Vec3& position)
{
    if (!gameLogic->isServer())
        return "Cannot move player from client.";
    world::IObject* const root = world::getRootParent(getVehicle());
    if (!root)
        return "Player has no vehicle.";
    root->setAbsolutePosition(position);
    return "";
}

// Shared state accessors recovered from the ELF and PE.
bool Player::getAutoReload() const { return m_autoReload; }
void Player::setAutoReload(bool enabled) { m_autoReload = enabled; }
void Player::setKitTimeStamp(double time) { m_kitTimestamp = time; }
world::UpdateFreqencyType Player::getUpdateFrequencyType() const
{
    return static_cast<world::UpdateFreqencyType>(m_updateFrequencyType);
}
void Player::setUpdateFrequencyType(world::UpdateFreqencyType type) { m_updateFrequencyType = type; }

// These are the actual dedicated-image bodies, not temporary success/default shims.
//  / PE 0x729830 (folded ret 8).
void Player::handleUpdate(float, unsigned int) {}

bool Player::getIsUnderwater() const { return false; }
bool Player::isPlayerInNightMap() const { return false; }

//  / PE 0x47C3D0. By-value input snapshot: compiler-owned aggregate copy.
// [PLATFORM] PE copies its whole 0x118-byte object; ELF emits a 273-byte logical copy.
io::PlayerInput Player::getPlayerInput() { return m_playerInput; }

//  / PE 0x475150. [INFERRED] name for the boolean: remote input bypasses
// auto-reload and pending-exit handling, and suppresses fire when the weapon needs a reload.
void Player::getAutoInput(io::PlayerInput& input, bool remote)
{
    if (!remote)
    {
        const WeakPtr<world::IObject>::Ctrl* const vehicle =
            static_cast<const WeakPtr<world::IObject>::Ctrl*>(m_vehicleWeak);
        if (m_autoReload && vehicle && vehicle->target)
        {
            world::IPlayerControlObject* const control = static_cast<world::IPlayerControlObject*>(
                vehicle->target->queryInterface(world::IID_IPlayerControlObject));
            if (control)
            {
                for (int slot = 0; slot < 3; ++slot)
                {
                    world::IObject* const object = control->getActiveWeapon(slot);
                    if (object)
                    {
                        world::IWeaponObject* const weapon = static_cast<world::IWeaponObject*>(
                            object->queryInterface(world::IID_IWeaponObject));
                        if (weapon && weapon->shouldReload())
                        {
                            input.setInput(io::PIReload, true);
                            break;
                        }
                    }
                }
            }
        }
        // This is present in both dedicated images: retain the real runtime server test.
        if (!gameLogic->isServer() && static_cast<ClientGameLogic*>(gameLogic)->getHasPendingExit())
        {
            input.setInput(io::PIFire, false);
            input.setInput(io::PIAltFire, false);
            input.setInput(io::PIFlareFire, false);
        }
    }

    const auto value = [&input](io::PlayerInputMap button) {
        return (input.mappingFlags & (1ULL << button)) ? input.inputs[button] : 0.0f;
    };
    if (getVehicle() && (value(io::PIFire) != 0.0f || value(io::PIAltFire) != 0.0f ||
                         value(io::PIFlareFire) != 0.0f))
    {
        world::IPlayerControlObject* const control = static_cast<world::IPlayerControlObject*>(
            getVehicle()->queryInterface(world::IID_IPlayerControlObject)); // repeated call, no query guard
        for (int slot = 0; slot < 3; ++slot)
        {
            world::IObject* const object = control->getActiveWeapon(slot);
            if (object)
            {
                world::IWeaponObject* const weapon = static_cast<world::IWeaponObject*>(
                    object->queryInterface(world::IID_IWeaponObject)); // unguarded in this arm
                if ((remote && weapon->needsReload()) || weapon->getHasHeat() ||
                    weapon->isReloading() || (!remote && weapon->isWaitingForMissileConfirm()))
                {
                    const io::PlayerInputMap fire = weapon->getFireInput();
                    const io::PlayerInputMap altFire = weapon->getAltFireInput();
                    if (fire != static_cast<io::PlayerInputMap>(64) && weapon->launchesProjectile(0))
                        input.setInput(fire, false);
                    if (altFire != static_cast<io::PlayerInputMap>(64) && weapon->launchesProjectile(1))
                        input.setInput(altFire, false);
                }
            }
        }
    }
}

// . IPlayer +0x164. The wire ping field is 10 bits (see updateStateMask slot +0xAA),
// so the ceiling is enforced here at store time rather than at serialise time. Strict `< 1023`:
// exactly 1023 and anything above it all collapse to 1023.
void Player::setPing(unsigned int ping)
{
    unsigned int clamped = 1023;                          // 0x3FF - the 10-bit ceiling
    if (ping < 1023)
        clamped = ping;
    m_ping = clamped;                                     // +0xDC
}

// . IPlayer +0x168. The raw round-trip time in ms. The full 32-bit field is
// returned here; it is init()/updateStateMask that narrow it to the low word on the wire (see the
// m_ping notes at the field and at slot +0xAA / buff[158]).
unsigned int Player::getPing() const
{
    return m_ping;                                        // +0xDC
}

// . IPlayer +0x174. Hands out the PlayerScoreData the ctor allocated (+0xEC); the
// score serialiser and the damage/rank paths read its fields (see the slot map at the top of file).
world::PlayerScoreData* Player::getScoreData()
{
    return m_scoreData;                                   // +0xEC
}

// . IPlayer +0x114.
bool Player::getIsCommander() const
{
    return m_isCommander;                                 // byte +0xF4
}

// . IPlayer +0x144.
void Player::setIsInsideCP(bool inside)
{
    m_isInsideCP = inside;                                // byte +0x108
}

// . IPlayer +0x148. Reads the "standing inside a control point" flag (byte +0x108).
bool Player::getIsInsideCP()
{
    return m_isInsideCP;
}

//  / 0x80F50E0, 17 B each. IPlayer +0x118 / +0x120. updateCommanderStatus is the only
// recovered caller of either - and it calls both on every server tick of every non-commander.
void Player::setUAVIsReady(bool ready)
{
    m_uavIsReady = ready;                                 // byte +0xF6
}

void Player::setSatelliteIsReady(bool ready)
{
    m_satelliteIsReady = ready;                           // byte +0xF7
}

//  / 0x80F50A0, 59 B each. IPlayer +0x11C / +0x124. commander-gated: the ready flags are
// only meaningful for the team commander, so both getters return false for everyone else regardless
// of what setUAVIsReady/setSatelliteIsReady last stored on them. updateStateMask reads both every
// tick (buff[50]/buff[51]); the gate is why a non-commander never leaks a stale ready bit.
bool Player::getUAVIsReady() const
{
    if (getIsCommander())                                 // virtual, IPlayer +0x114
        return m_uavIsReady;                              // byte +0xF6
    return false;
}

bool Player::getSatelliteIsReady() const
{
    if (getIsCommander())                                 // virtual, IPlayer +0x114
        return m_satelliteIsReady;                        // byte +0xF7
    return false;
}

// : `mov eax, 30h`. io::INetworkable's slot, at Player's primary +0x350. A constant -
// the player's replication budget floor, in bits. Also one of the five missing overrides: without
// it NetworkManager sized every player's property descriptor with INetworkable's default instead.
unsigned int Player::getMinimumBitSize() const
{
    return 48;
}

// . IPlayer +0x9C. The `void*` is the interface's; the value is an io::PlayerInput*.
void* Player::getAIPlayerInput() const
{
    return m_aiPlayerInput;                               // +0x15C
}

// Switch between the human ActionBuffer and AI PlayerInput, destroying the outgoing
// input source. Repeating the current mode does not allocate or free either source.
void Player::setIsAIPlayer(bool ai)
{
    if (m_isAI)
    {
        if (!ai)
        {
            m_actionBuffer = new ActionBuffer();          // operator new(0x2Cu)
            ::operator delete(m_aiPlayerInput);           // no destructor, see above
            m_aiPlayerInput = 0;
        }
    }
    else if (ai)
    {
        if (m_actionBuffer)
            delete m_actionBuffer;                        // ~ActionBuffer then delete
        m_actionBuffer = 0;

        // : operator new(0x114u), then the ELF's own field-by-field clear - the two
        // mapping-mask dwords, the two unknowns, the flag byte, and last the 64 input floats. The
        // three padding bytes at +0x111..+0x113 are left alone, exactly as in ActionBuffer's ctor.
        io::PlayerInput* input = new io::PlayerInput;
        input->mappingFlags = 0;                          // +0x100 / +0x104, two dword stores
        input->dword0 = 0;
        input->dword1 = 0;
        input->flag = 0;
        for (int i = 0; i < 64; ++i)
            input->inputs[i] = 0.0f;                      // the ecx=0 store loop

        m_aiPlayerInput = input;
    }

    m_isAI = ai;                                          // byte +0xB8
}

// Refresh commander readiness from team tools on the server. Non-commanders have
// both ready flags cleared; clients receive their state through replication.
void Player::updateCommanderStatus()
{
    if (!gameLogic->isServer())                           // +0x13C
        return;

    if (!getIsCommander())                                // +0x114
    {
        setUAVIsReady(false);                             // +0x118
        setSatelliteIsReady(false);                       // +0x120
        return;
    }

    if (!world::commanderToolsManager)
        return;

    const world::CommanderToolState* const state =
        world::commanderToolsManager->getToolStateForTeam(getTeam());
    if (!state)
        return;

    setUAVIsReady(state->getUAVIsReady());
    setSatelliteIsReady(state->getScanIsReady());
    m_fieldF8 = state->getSatelliteCount() > 0;
    m_fieldF9 = state->getArtilleryCount() > 0;
    m_fieldFA = state->getUAVCount() > 0;

    // Retail copies the three float dwords verbatim into these int-backed replication fields.
    const float satelliteArmor = state->getSatelliteArmor();
    const float artilleryArmor = state->getArtilleryArmor();
    const float uavArmor = state->getUAVArmor();
    std::memcpy(&m_fieldFC, &satelliteArmor, sizeof(m_fieldFC));
    std::memcpy(&m_field100, &artilleryArmor, sizeof(m_field100));
    std::memcpy(&m_field104, &uavArmor, sizeof(m_field104));
}

// Clear mine proximity state and announce it once when the map is nonempty.
// Local humans receive a local event; the remote path uses the player id as
// the transport destination.
void Player::emptyMineMap()
{
    if (m_closeMines.empty())                             // +0x13C, the node count
        return;

    m_closeMines.clear();                                 // _M_erase + header reset

    int src = getId();                                    // +0x54, a virtual call in the ELF too

    if (!m_isAI && this == world::playerManager->getLocalHumanPlayer())
    {
        // ECHud (2), event 58. [UNRESOLVED] what 58 is - no recovered name for it; the payload is
        // the player id and the HUD is the audience, so "your mine warnings are gone" fits.
        eventManager->postEvent(ECHud, 58, &src, 0.0f);   // EventManager +0x14
    }
    else
    {
        // payloadBitSize 4 for a 4-byte payload. PostRemoteEvent stores that argument in a byte
        // (+0x48) and the tree's own note calls it a bit size; either this call is a bug or the
        // field is a byte count. Passed through unchanged - the wire will settle it.
        PostRemoteEvent* event = new PostRemoteEvent(ECHud, 58, false, &src, 4, 0.0f);
        game->sendGameEventIfPlaying(static_cast<unsigned int>(src), event);   // IGame +0x88
    }
}

// Update explosion effects, server abilities and mine warnings, commander status,
// voice state and radio throttling. Requires gameLogic and the relevant managers.
// Mine scans use a 0.2-second countdown; only the local human clears the voice latch.
void Player::update(float deltaTime)
{
    // 1 & 2: the persistent explosion effects
    world::explosionEffectManager->handleObject(this);

    // [PLATFORM] The client additionally updates an unidentified member before local
    // explosion effects. That client-only call remains unimplemented.
    // [RETIRE-WHEN: player-update-client-arm] Identify the member and recover its update.
    if (this == world::playerManager->getLocalHumanPlayer() && !getIsRemote())
        world::explosionEffectManager->updateExplosionEffects(deltaTime);

    // 3: a dead player is not inside a control point
    if (!getIsAlive() && m_isInsideCP)
        setIsInsideCP(false);                             // +0x144

    // 4: the server's half
    if (gameLogic->isServer())
    {
        if (getIsAlive())
        {
            updateAbility(deltaTime);                     // deltaTime is unused by the body
            m_closeMineCheckTimer -= deltaTime;
            if (m_closeMineCheckTimer <= 0.0f)
            {
                updateIsCloseToMine(deltaTime);
                m_closeMineCheckTimer = 0.2f;
            }
        }
        else
        {
            emptyMineMap();
        }

        updateCommanderStatus();
    }

    // 5: the one-shot VOIP re-check
    if (m_checkVoip && this == world::playerManager->getLocalHumanPlayer())
    {
        if (gameLogic)
            m_voipEnabled = gameLogic->isVoipClientRunning();   // +0x260
        m_checkVoip = false;
    }

    // Commanders receive twice the radio allowance and half the block duration.
    // Integer division rounds an odd duration toward zero.
    int maxSpamFlagCount = serverInfo->m_block.radioMaxSpamFlagCount;   // ServerInfo +0x2C
    int blockedDuration  = serverInfo->m_block.radioBlockedDuration;    // ServerInfo +0x30

    if (getIsCommander())                                 // +0x114
    {
        maxSpamFlagCount *= 2;
        blockedDuration  /= 2;
    }

    // Recheck the count after each clock call. The equality guard skips the
    // window check on the tick that armed the timer.
    if (m_radioSpamFlagCount >= maxSpamFlagCount &&
        static_cast<float>(WorldPref::getWorldTime()) - m_radioSpamFilterFlagTimer
            >= static_cast<float>(blockedDuration))
    {
        m_radioSpamFlagCount        = 0;
        m_radioSpamBlockedFlagCount = 0;
        m_radioSpamFilterFlagTimer  = -1.0f;              // the ctor's sentinel again
    }
    else if (m_radioSpamFlagCount < maxSpamFlagCount &&
             m_radioSpamFilterFlagTimer != static_cast<float>(WorldPref::getWorldTime()) &&
             static_cast<float>(WorldPref::getWorldTime()) - m_radioSpamFilterFlagTimer
                 >= static_cast<float>(serverInfo->m_block.radioSpamInterval))   // ServerInfo +0x28
    {
        m_radioSpamFlagCount        = 0;
        m_radioSpamBlockedFlagCount = 0;
        m_radioSpamFilterFlagTimer  = -1.0f;
    }

    // Spend at most three verification attempts. The counter advances before the
    // send conditions, so an attempt can be consumed without sending an event.
    if (m_verifyTeamSendCount <= 2 && m_verifyTeamPending)
    {
        m_verifyTeamPending = false;
        ++m_verifyTeamSendCount;

        world::IPlayer* localPlayer = world::playerManager->getLocalHumanPlayer();

        if (!gameLogic->isServer() && localPlayer && gameLogic && gameLogic->getGameStatus() == 1)
        {
            GameEvent* event = createVerifyPlayerTeamEvent(localPlayer, this);
            game->sendGameEvent(0xFFFFFFFFu, event);      // IGame +0x90, connection id -1
        }
    }
}

// The 57 / 58 emitter. The ELF spells this out three times - twice here and once in emptyMineMap -
// and all three are identical but for the event number, so it lives here once. A local human player
// gets it straight through the EventManager; anybody else gets a PostRemoteEvent on the wire.
void Player::postMineEvent(unsigned int eventId)
{
    int src = getId();                                    // +0x54, a virtual call in the ELF too

    if (!m_isAI && this == world::playerManager->getLocalHumanPlayer())   // +0xB8, +0x30
    {
        eventManager->postEvent(ECHud, eventId, &src, 0.0f);              // EventManager +0x14
    }
    else
    {
        // payloadBitSize 4 for a 4-byte payload - emptyMineMap's note applies here too.
        PostRemoteEvent* event = new PostRemoteEvent(ECHud, eventId, false, &src, 4, 0.0f);
        game->sendGameEventIfPlaying(static_cast<unsigned int>(src), event);   // IGame +0x88
    }
}

// Refresh mine proximity warnings on the server; the caller controls cadence.
// deltaTime is unused. Track newly seen, persistent and departed mines, then
// notify the local player or remote connection of changes.
void Player::updateIsCloseToMine(float /*deltaTime*/)
{
    if (!gameLogic->isServer())                                           // IGameLogic +0x13C
        return;

    unsigned char* const vehicleBlock = static_cast<unsigned char*>(m_vehicleWeak);   // +0x64
    world::IObject* const vehicle =
        vehicleBlock ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(vehicleBlock)->target : 0;
    if (!vehicle)
        return;

    std::vector<world::IObject*> objects;
    world::ObjectFlagPredicator predicator(0x1000000u);                   // bit 24 - is a root

    const float vehicleRadius = vehicle->getPhysicBoundingRadius();       // slot 38
    const Vec3& position = vehicle->getAbsolutePosition();                // slot 27
    world::objectManager->getObjectsFromGrid(position, vehicleRadius + 100.0f,
                                             objects, &predicator);       // +0x40

    bool anyClose = false;
    for (std::vector<world::IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        world::IObject* const obj = *it;
        if (!obj)
            continue;

        // classId AND iid are the same value here. Read raw.
        world::IProjectileDetonationComp* const detonation =
            static_cast<world::IProjectileDetonationComp*>(obj->queryComponent(
                world::IID_IProjectileDetonationComp, world::IID_IProjectileDetonationComp));
        if (!detonation)
            continue;
        if (!detonation->getShowMineIndicationIcon())
            continue;

        const Vec3& minePosition = obj->getAbsolutePosition();
        Vec3 d;
        d.x = position.x - minePosition.x;
        d.y = position.y - minePosition.y;
        d.z = position.z - minePosition.z;
        const float distance = sqrtf(d.z * d.z + d.y * d.y + d.x * d.x);
        if (distance > detonation->getDetectionRadius() + vehicleRadius)
            continue;

        world::IProjectileObject* const projectile =
            static_cast<world::IProjectileObject*>(obj->queryInterface(world::IID_IProjectileObject));
        if (!projectile)
            continue;

        unsigned char* const defaultBlock = static_cast<unsigned char*>(m_defaultVehicleWeak);
        world::IObject* const soldier =
            defaultBlock ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(defaultBlock)->target : 0;
        if (!soldier)
            continue;

        // engineer first, and it short-circuits: kit 4 never reaches the team comparison.
        if (static_cast<world::Soldier*>(soldier)->getKitType() != 4                  // +0x2A4
            && static_cast<world::GenericProjectile*>(projectile)->getTeam() != getTeam())
            continue;

        anyClose = true;
        std::map<WeakPtr<world::IObject>, ItemStatus>::iterator found =
            m_closeMines.find(obj->getWeakPtr());                         // slot 7
        if (found == m_closeMines.end())
            m_closeMines.insert(std::make_pair(obj->getWeakPtr(), ItemStatus()));
        else
            found->second.m_status = 1;
    }

    if (m_closeMines.empty() || !anyClose)
    {
        emptyMineMap();
        return;
    }

    std::map<WeakPtr<world::IObject>, ItemStatus>::iterator it = m_closeMines.begin();
    while (it != m_closeMines.end())
    {
        if (!it->first.get())                                             // the weak target
        {
            ++it;
            continue;
        }

        if (it->second.m_status == 0)
        {
            postMineEvent(57);                                            // "a mine is near you"
            it->second.m_status = 2;
            ++it;
        }
        else if (it->second.m_status == 1)
        {
            it->second.m_status = 2;                                      // already warned
            ++it;
        }
        else if (it->second.m_status == 2)
        {
            // not refreshed this pass - the mine is gone
            postMineEvent(58);                                            // "all clear"
            m_closeMines.erase(it++);
        }
        // no else: see the header note about the spin.
    }
}

// Apply the kit's in-vehicle ammo, heal or repair aura on the server; deltaTime
// is unused. Requires a soldier default vehicle and a vehicle with auto-use enabled.
// Successful targets are served again with strength divided among them. The
// nearby-occupant skip list is built but is not consulted by the second pass.
void Player::updateAbility(float /*deltaTime*/)
{
    typedef world::object_detail::LayoutAccess L;

    if (!gameLogic->isServer())                                            // IGameLogic +0x13C
        return;

    IGameLogic* const logic = gameLogic;

    unsigned char* vehicleBlock = static_cast<unsigned char*>(m_vehicleWeak);          // +0x64
    world::IObject* const vehicle =
        vehicleBlock ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(vehicleBlock)->target : 0;
    unsigned char* defaultBlock = static_cast<unsigned char*>(m_defaultVehicleWeak);   // +0xB0
    world::IObject* soldier =
        defaultBlock ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(defaultBlock)->target : 0;

    if (!vehicle || !soldier)
        return;
    if (soldier->getTemplate()->getClassID() != world::CID_SoldierTemplate)
        return;

    // the vehicle chain, upward, until something answers as a control object
    world::IPlayerControlObject* pco = 0;
    for (world::IObject* p = vehicle; ; )
    {
        pco = static_cast<world::IPlayerControlObject*>(
            p->queryInterface(world::IID_IPlayerControlObject));           // slot 3
        if (pco)
            break;
        p = L::parent(p);                                                  // +0x28
        if (!p)
            break;
    }
    // NO NULL guard in the ELF - a chain with no control object dereferences null here.
    // Preserved: the gates above make it unreachable in practice (the vehicle IS one).
    if (!pco->getAutoUseAbility())                                         // +0x134
        return;

    // the ELF re-reads both weak blocks rather than reusing the locals; preserved
    vehicleBlock = static_cast<unsigned char*>(m_vehicleWeak);
    if (!(vehicleBlock ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(vehicleBlock)->target : 0))
        return;
    defaultBlock = static_cast<unsigned char*>(m_defaultVehicleWeak);
    if (!(defaultBlock ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(defaultBlock)->target : 0))
        return;
    soldier = reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(defaultBlock)->target;
    if (soldier->getTemplate()->getClassID() != world::CID_SoldierTemplate)
        return;

    world::Soldier* const kitOwner = static_cast<world::Soldier*>(soldier);
    // getKitType() is asked once PER comparison throughout this body, never cached. Preserved.
    if (kitOwner->getKitType() != 5                                        // +0x2A4
        && kitOwner->getKitType() != 4
        && kitOwner->getKitType() != 3)
        return;

    {
        unsigned char* const b = static_cast<unsigned char*>(m_vehicleWeak);
        world::IObject* const v = b ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(b)->target : 0;
        const unsigned int cid = v->getTemplate()->getClassID();
        if (cid != world::CID_PlayerControlObjectTemplate
            && cid != world::CID_GrapplingHookRopeContainerTemplate
            && cid != world::CID_ZiplineContainerTemplate)
            return;
    }

    std::vector<world::IObject*> objects;
    std::set<world::IObject*> served;

    world::Kit* const kit = kitOwner->getKit();                            // +0x2A0
    const world::KitTemplate* const kitTemplate =
        static_cast<const world::KitTemplate*>(L::objectTemplate(kit));    // +0x14

    // pass 0: who is already someone's control object
    {
        world::PCOPredicator pcoPredicator;
        unsigned char* const b = static_cast<unsigned char*>(m_vehicleWeak);
        world::IObject* const v = b ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(b)->target : 0;
        world::objectManager->getObjectsFromGrid(v->getAbsolutePosition(),   // slot 27
                                                 kitTemplate->m_abilityInVehicleRadius,
                                                 objects, &pcoPredicator);   // +0x40 empty stub
    }

    std::list<world::IObject*> occupantBodies;
    for (std::vector<world::IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        world::IObject* const o = *it;
        const unsigned int cid = o->getTemplate()->getClassID();
        if (cid != world::CID_PlayerControlObjectTemplate
            && cid != world::CID_GrapplingHookRopeContainerTemplate
            && cid != world::CID_ZiplineContainerTemplate)
            continue;
        world::IPlayerControlObject* const other = static_cast<world::IPlayerControlObject*>(
            o->queryInterface(world::IID_IPlayerControlObject));
        if (!other)
            continue;
        IPlayer* const occupant = other->getOccupingPlayer();               // +0x10
        if (!occupant)
            continue;
        occupantBodies.insert(occupantBodies.end(), occupant->getDefaultVehicle());   // +0xA4
    }
    objects.erase(objects.begin(), objects.end());

    // pass 1: the real sweep
    {
        world::AbilityPredicator abilityPredicator(
            0x400u, 0,
            static_cast<unsigned int>(kitTemplate->m_abilityInVehicleMaterial), false);
        unsigned char* const b = static_cast<unsigned char*>(m_vehicleWeak);
        world::IObject* const v = b ? reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(b)->target : 0;
        world::objectManager->getObjectsFromGrid(v->getAbsolutePosition(),
                                                 kitTemplate->m_abilityInVehicleRadius,
                                                 objects, &abilityPredicator);   // empty stub
    }
    // the skip list is appended into the sweep result and then never consulted again - see above
    for (std::list<world::IObject*>::iterator it = occupantBodies.begin();
         it != occupantBodies.end(); ++it)
        objects.push_back(*it);

    for (std::vector<world::IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        world::IObject* const o = *it;
        world::IObject* target = 0;
        if (!o)
            continue;
        if (o->queryInterface(world::IID_IProjectileObject))
            continue;

        if (kitOwner->getKitType() == 5 || kitOwner->getKitType() == 3)
        {
            const unsigned int cid = o->getTemplate()->getClassID();
            if (cid == world::CID_PlayerControlObjectTemplate
                || cid == world::CID_GrapplingHookRopeContainerTemplate
                || cid == world::CID_ZiplineContainerTemplate)
            {
                world::IPlayerControlObject* const other =
                    static_cast<world::IPlayerControlObject*>(
                        o->queryInterface(world::IID_IPlayerControlObject));
                if (!other)
                    continue;
                if (kitOwner->getKitType() != 3)
                    continue;
                // ammo: never your own vehicle, and only the other team's - or a neutral one
                if (world::getRootParent(o) == world::getRootParent(vehicle))
                    continue;
                if (other->getTeam() == getTeam())                          // PCO +0x98 / Player +0xE8
                    continue;
                if (other->getTeam() != 0)
                    continue;
                target = o;
            }
            else if (cid == world::CID_SoldierTemplate)
            {
                IPlayer* const occupant =
                    static_cast<world::Soldier*>(o)->getOccupingPlayer();   // Soldier +0x180
                if (!occupant)
                    continue;
                if (occupant->getTeam() != getTeam())                       // IPlayer +0xE8
                    continue;
                target = o;
            }
        }
        else if (kitOwner->getKitType() == 4 && (L::flags(o) & 0x01000000u) != 0)   // bit 24: is a root
        {
            world::IPlayerControlObject* const other =
                static_cast<world::IPlayerControlObject*>(
                    o->queryInterface(world::IID_IPlayerControlObject));
            if (other && other->getTeam() != getTeam() && other->getTeam() != 0)
                continue;
            target = o;
        }

        if (!target)
            continue;
        world::IArmorGLComp* const armor =
            static_cast<world::IArmorGLComp*>(L::armorComponent(target));   // +0x3C
        if (!armor)
            continue;

        if (kitOwner->getKitType() == 5 || kitOwner->getKitType() == 4)
        {
            if (logic->heal(target, kitTemplate->m_abilityInVehicleStrength, this,
                            kitTemplate->m_abilityInVehicleMaterial,
                            armor->getDefaultMaterial(), 0, true, false))   // IGameLogic +0x104
                served.insert(target);
        }
        else if (kitOwner->getKitType() == 3)
        {
            if (logic->replenishAmmo(target, kitTemplate->m_abilityInVehicleStrength, this,
                                     kitTemplate->m_abilityInVehicleMaterial,
                                     armor->getDefaultMaterial(), 0, true))  // IGameLogic +0x108
                served.insert(target);
        }
    }

    // pass 2: the same objects again, with the strength shared
    if (!served.empty())
    {
        for (std::set<world::IObject*>::iterator it = served.begin(); it != served.end(); ++it)
        {
            world::IObject* const target = *it;
            if (!target)
                continue;
            world::IArmorGLComp* const armor =
                static_cast<world::IArmorGLComp*>(L::armorComponent(target));
            if (kitOwner->getKitType() == 5 || kitOwner->getKitType() == 4)
            {
                logic->heal(target,
                            kitTemplate->m_abilityInVehicleStrength
                                / static_cast<float>(served.size()),
                            this, kitTemplate->m_abilityInVehicleMaterial,
                            armor->getDefaultMaterial(), 0, false, false);
            }
            else if (kitOwner->getKitType() == 3)
            {
                // NOT divided on this arm - ammo pass 2 uses the full strength. Preserved.
                logic->replenishAmmo(target, kitTemplate->m_abilityInVehicleStrength, this,
                                     kitTemplate->m_abilityInVehicleMaterial,
                                     armor->getDefaultMaterial(), 0, false);
            }
        }
    }
}


// Assign the default vehicle weak handle. A non-null assignment aborts if a live
// default vehicle already exists; clearing is safe to repeat.
void Player::setDefaultVehicle(world::IObject* vehicle)
{
    void* incoming = 0;

    if (vehicle)
    {
        // the abort guard - see above
        world::IObject* existing = 0;
        if (m_defaultVehicleWeak)
            existing = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_defaultVehicleWeak)->target;
        if (existing)
            abort();

        WeakPtr<world::IObject> handle = vehicle->getWeakPtr();       // slot 7
        incoming = handle.m_ctrl;
        handle.m_ctrl = 0;                                            // the steal - see below
        if (incoming)
            ++(*reinterpret_cast<int*>(incoming));
    }

    // The assignment, written out the way the ELF spells it. The "release the temporary" step at
    // the bottom IS the returned WeakPtr's destructor - the same accounting allocateFreeCamera needs.
    if (m_defaultVehicleWeak)
    {
        int* old = reinterpret_cast<int*>(m_defaultVehicleWeak);
        if ((*old)-- == 1)
            ::operator delete(m_defaultVehicleWeak);
    }
    m_defaultVehicleWeak = incoming;
    if (incoming)
    {
        int* n = reinterpret_cast<int*>(incoming);
        if ((*n)-- == 1)
            ::operator delete(incoming);
    }
}

// Replace the controlled vehicle and seat. A null vehicle enters the free camera
// unless already in one; clear the default vehicle when it matches the old vehicle.
// For a new vehicle, send messages 2 then 4 and adopt its active camera.
bool Player::_setVehicle(world::IObject* vehicle, int seat)
{
    world::IObject* oldVehicle = 0;
    if (m_vehicleWeak)
        oldVehicle = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_vehicleWeak)->target;

    if (vehicle)
    {
        // The nearest control object at or above the new vehicle.
        world::IPlayerControlObject* pco = 0;
        for (world::IObject* node = vehicle; node; node = world::object_detail::LayoutAccess::parent(node))
        {
            pco = static_cast<world::IPlayerControlObject*>(
                      node->queryInterface(world::IID_IPlayerControlObject));
            if (pco)
                break;
        }

        m_inputId = seat;                                             // +0x98

        WeakPtr<world::IObject> handle = vehicle->getWeakPtr();       // slot 7
        void* block = handle.m_ctrl;
        handle.m_ctrl = 0;
        if (block)
            ++(*reinterpret_cast<int*>(block));
        if (m_vehicleWeak)
        {
            int* old = reinterpret_cast<int*>(m_vehicleWeak);
            if ((*old)-- == 1)
                ::operator delete(m_vehicleWeak);
        }
        m_vehicleWeak = block;
        if (block)
        {
            int* n = reinterpret_cast<int*>(block);
            if ((*n)-- == 1)
                ::operator delete(block);
        }

        // Resolve the vehicle weak reference again before each message.
        world::IObject* now = 0;
        if (m_vehicleWeak)
            now = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_vehicleWeak)->target;
        now->handleMessage(static_cast<world::TemplateMessage>(2), this, false);   // slot +0x104

        now = 0;
        if (m_vehicleWeak)
            now = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_vehicleWeak)->target;
        now->handleMessage(static_cast<world::TemplateMessage>(4), this, false);

        m_camera = static_cast<world::ICameraObject*>(pco->getActiveCamera());      // +0x68, PCO +0x44
        if (m_camera)
            m_camera->resetCameraView(true);                           // ICameraObject +0x50
        return true;
    }

    // the null path: leave whatever we were in, and fall back to the free camera
    world::IObject* freeCamera = 0;
    if (m_freeCameraWeak)
        freeCamera = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_freeCameraWeak)->target;

    if (oldVehicle &&
        oldVehicle->getTemplate()->getClassID() == world::CID_FreeCameraTemplate)
        return true;                                                  // already in a free camera

    if (oldVehicle == getDefaultVehicle())                            // +0xA4
        setDefaultVehicle(0);                                         // +0xA0

    if (m_vehicleWeak)
    {
        int* old = reinterpret_cast<int*>(m_vehicleWeak);
        if ((*old)-- == 1)
            ::operator delete(m_vehicleWeak);
    }
    m_vehicleWeak = 0;
    m_camera = 0;                                                     // +0x68

    if (freeCamera && !freeCamera->isObjectDestroyed())                // slot +0x3C
        game->enterVehicle(this, freeCamera, false);                   // IGame +0xA4

    return true;    // Always succeeds.
}


// Change vehicle ownership and control, exiting an old free camera first.
// Entering an ordinary vehicle from an empty current slot is fatal: the free camera
// must be established first. Every normal return is true.
bool Player::setVehicle(world::IObject* vehicle, int seat)
{
    world::IObject* current = getVehicle();                           // +0x48

    if (vehicle == current && seat == getInputId())                   // +0x50
        return true;                                                  // nothing is changing

    if (!vehicle)
    {
        if (!current)
            return true;                                              // already in nothing
    }
    else if (!current &&
             vehicle->getTemplate()->getClassID() != world::CID_FreeCameraTemplate)
    {
        // The site that attests this file's line 901. fatal - see the note above.
        Debug(DEBUGTYPE_6, "Game/GameLogic/Player.cpp", 901, __FILE__)
            << "trying to set " << vehicle->getTemplate()->getName()
            << " without first having a freeCamera as vehicle";
    }

    if (vehicle && current &&
        current->getTemplate()->getClassID() == world::CID_FreeCameraTemplate)
        game->exitVehicle(this, true, true);                          // IGame +0xA8

    if (!_setVehicle(vehicle, seat))                                  // always true - see above
        return false;

    m_inputId = seat;                                                 // +0x98 (again)
    clearObjectList();

    // Resolve the stored vehicle again; _setVehicle may have replaced or cleared it.
    world::IObject* now = 0;
    if (m_vehicleWeak)
        now = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_vehicleWeak)->target;
    if (now)
    {
        // A queryInterface(IID_IObject) ON something already typed AS AN IObject*. It is not a
        // no-op in general - a multiply-derived object hands back its IObject sub-object here - so
        // it is kept rather than elided.
        buildObjectList(static_cast<world::IObject*>(
                            now->queryInterface(world::IID_IObject)), m_inputId);
    }
    return true;
}

// Release network ownership of all live tracked objects, then clear the list.
// Entries must expose a networkable interface, as guaranteed by buildObjectList.
void Player::clearObjectList()
{
    for (std::list<WeakPtr<world::IObject> >::iterator it = m_objectList.begin();
         it != m_objectList.end(); ++it)
    {
        world::IObject* object = it->get();
        if (!object)
            continue;                                                 // the handle went stale

        io::INetworkable* networkable = object->getNetworkable();      // slot 54 (+0xD8)
        io::INetworkableObject* netObject = static_cast<io::INetworkableObject*>(
            networkable->queryInterface(io::IID_INetworkableObject));
        if (netObject)
            netObject->setOwnerID(-1);                                 // +0x10
    }

    m_objectList.clear();
}

// Claim the object and eligible replicating children for this player, depth first.
// Soldiers and flags are included; other player-control objects own themselves.
// The object must be non-null. seat is forwarded but does not select the owner id.
void Player::buildObjectList(world::IObject* object, int seat)
{
    if (object->getNetworkable())                                      // slot 54 (+0xD8)
    {
        io::INetworkableObject* netObject =
            object->getNetworkable()->m_networkableObject;             // INetworkable +0x18
        if (netObject)
            netObject->setOwnerID(m_playerId);                         // +0x10  NOT `seat`

        m_objectList.push_back(object->getWeakPtr());                  // slot 7, inserted at end()
    }

    for (world::IObject* child = object->getChild();                   // slot 60 (+0xF0)
         child;
         child = world::object_detail::LayoutAccess::sibling(child))   // +0x2C
    {
        if (child->getTemplate()->getClassID() != world::CID_SoldierTemplate)
        {
            if (child->queryInterface(world::IID_IPlayerControlObject))
                continue;                                              // its own vehicle - stop
            if (child->getTemplate()->getClassID() != world::CID_FlagTemplate)
                continue;                                              // ordinary geometry
        }
        buildObjectList(child, seat);
    }
}


// Set the camera FOV target; zero uses the camera's active FOV. time is the ramp
// duration in seconds, with zero snapping immediately. Repeating the current target
// is a no-op even if time changes. Without a camera nothing changes.
void Player::setFov(float fov, float time)
{
    world::ICameraObject* camera = getCamera();                    // IPlayer +0x180
    if (!camera)
        return;

    if (fov == 0.0f)
        fov = camera->getActiveFOV();                              // ICameraObject +0x58

    if (fov == m_endFov)                                           // +0xA4 - already heading there
        return;

    if (time == 0.0f)
    {
        if (fov == 0.0f)
            fov = camera->getActiveFOV();                          // again - see the note
        m_endFov = fov;
        m_fov    = fov;                                            // +0xA8 - snap
    }
    else
    {
        if (fov == 0.0f)
            fov = camera->getActiveFOV();                          // again
        const float current = m_fov;
        m_endFov  = fov;
        m_fovRate = fabsf(current - fov) / time;                   // +0xAC - units per second
    }
}

float Player::getFov() const
{
    return m_fov;                                                  // +0xA8
}

float Player::getEndFov() const
{
    return m_endFov;                                               // +0xA4
}


// Store primary firing state and count rising edges for replication.
void Player::setFiringState(bool firing)
{
    // THE counter, and it only ever goes up on a fresh press.
    if (firing && !m_field160[0])
        ++m_firePressCount;                          // +0x208
    m_field160[0] = firing;                          // +0x160
}

void Player::setAltFiringState(bool firing)          { m_field160[1] = firing; }   // +0x161
void Player::setFlareFiringState(bool firing)        { m_field160[2] = firing; }   // +0x162
void Player::setSprintState(bool sprinting)          { m_field160[3] = sprinting; }// +0x163
bool Player::getSprintState() const                  { return m_field160[3]; }
void Player::setSprintStateLastTick(bool sprinting)  { m_field160[4] = sprinting; }// +0x164
bool Player::getSprintStateLastTick() const          { return m_field160[4]; }

// Timestamp used for kit-pickup cooldown checks.
double Player::getKitTimeStamp() const               { return m_kitTimestamp; }    // +0x4C

// Store the current firing button without range checks or other firing-state effects.
// PINone is valid; the value persists until another setter call.
void Player::setCurrentFiringButton(io::PlayerInputMap button)
{
    m_currentFiringButton = button;                                           // +0x1FC
}

io::PlayerInputMap Player::getCurrentFiringButton() const
{
    return m_currentFiringButton;                                             // +0x1FC
}

// Record the support cooldown tick together with its giving/repairing flags.
void Player::setHealRepairedAtTick(unsigned int tick, bool giving, bool repairing)
{
    m_healRepairedAtTick = tick;                             // +0x128
    m_isGivingAbility    = giving;                           // +0x134
    m_isRepairing        = repairing;                        // +0x135
}
unsigned int Player::getHealRepairedAtTick() const   { return m_healRepairedAtTick; }   // +0x128
void Player::setGaveAmmoAtTick(unsigned int tick, bool giving)
{
    m_gaveAmmoAtTick     = tick;                             // +0x12C
    m_isGivingReplenish  = giving;                           // +0x136
}
unsigned int Player::getGaveAmmoAtTick() const       { return m_gaveAmmoAtTick; }       // +0x12C
void Player::setFailedExitAtTick(unsigned int tick)  { m_failedExitAtTick = tick; }     // +0x130
unsigned int Player::getFailedExitAtTick() const     { return m_failedExitAtTick; }     // +0x130
bool Player::getIsGivingAbility() const              { return m_isGivingAbility; }      // +0x134
bool Player::getIsRepairing() const                  { return m_isRepairing; }          // +0x135
bool Player::getIsGivingReplenish() const            { return m_isGivingReplenish; }    // +0x136

// the flag carrier: 0x80F9240 / 0x80F9260
void Player::setFlagHolder(bool holder)              { m_flagHolder = holder; }         // +0x124
bool Player::getFlagHolder() const                   { return m_flagHolder; }           // +0x124

// the firing trio: 0x80F9390 / 0x80F93E0 / 0x80F9410
// getFiringState IS NOT AN accessor. It ORs the live firing bool with a second field at +0x204,
// so a player counts as firing when either is set. [UNRESOLVED] what +0x204 is - the ctor zeroes it
// and nothing recovered writes it - but the OR is compiled and is reproduced.
bool Player::getFiringState() const
{
    return m_field160[0] != 0 || m_field204 != 0;            // +0x160 || +0x204
}
bool Player::getAltFiringState() const                { return m_field160[1] != 0; }    // +0x161
bool Player::getFlareFiringState() const              { return m_field160[2] != 0; }    // +0x162

// the reload latch: 0x80F9490
// read AND clear - the name says so and the body agrees. Its setter (setReloadState) is already
// written below; this is the half that was missing.
bool Player::getAndResetReloadState()
{
    const bool state = m_reloadState;                        // +0x244
    m_reloadState = false;
    return state;
}

// commander timers: 0x80F9520 / 0x80F9540 / 0x80F9550 / 0x80F9570
// both ARE doubles. m_lastTimeCommander's -99999.0 sentinel was already recorded; the reapply
// timer at +0x114 is the field this sweep corrected from two ints to one double.
void   Player::setLastTimeCommander(double t)         { m_lastTimeCommander = t; }        // +0x10C
double Player::getLastTimeCommander() const           { return m_lastTimeCommander; }     // +0x10C
void   Player::setCommanderReapplyWaitTime(double t)  { m_commanderReapplyWaitTime = t; } // +0x114
double Player::getCommanderReapplyWaitTime() const    { return m_commanderReapplyWaitTime; }

// connection clocks: 0x80F9620 / 0x80F9640
// ONE IS A float AND THE other A double, which is why they are declared that way.
float  Player::getTimeOutSideWorld() const            { return m_timeOutSideWorld; }      // +0x24C
double Player::getTimeConnected() const               { return m_timeConnected; }         // +0x180

// the hit indicator: 0x80F9650
// A setter with A side effect: taking a hit also slams the crosshair fade to 1.0, and only on
// the true arm - clearing the indicator leaves the fade alone to decay on its own.
void Player::setHitIndicator(bool hit)
{
    if (hit)
        m_crossHairFadeValue = 1.0f;                         // +0x25C
    m_hitIndicator = hit;                                    // +0x260
}

// squad invites: 0x80F96B0 / 0x80F96C0
// getIsInvitedToSquad IS A BIT test, not a field read: one bit per squad in the invite mask.
unsigned int Player::getSquadInviteMask() const       { return m_squadInviteMask; }       // +0x11C
//  / PE 0x4727F0, 16 B. The full dword is retained; only the low eight bits
// are carried by the player state-sync wire.
void Player::setSquadInviteMask(unsigned int mask)    { m_squadInviteMask = mask; }       // +0x11C
bool Player::getIsInvitedToSquad(int squad) const
{
    return (m_squadInviteMask & (1u << squad)) != 0;         // +0x11C, shl by the squad index
}

// voice: 0x80F97F0 / 0x80F9800 / 0x80F9810
bool   Player::getIsMuted()                           { return m_isMuted; }               // +0x2B4
double Player::getLastTalkTime()                      { return m_lastTalkTime; }          // +0x2B8
double Player::getStopTalkingThreshold()              { return m_stopTalkingThreshold; }  // +0x2C0

// Stamp the last-talk time, or clear it when talking stops. Talking status expires
// after the configured threshold; muting uses the virtual setIsTalking method.
void Player::setIsTalking(bool talking)
{
    m_lastTalkTime = talking ? g_system->getExactTime() : 0.0;                // +0x2B8
}

bool Player::getIsTalking()
{
    // `fucomip` + `setbe`: the elapsed time is the left operand and the comparison is <=.
    return (g_system->getExactTime() - m_lastTalkTime) <= m_stopTalkingThreshold;
}

void Player::setIsMuted(bool muted)
{
    m_isMuted = muted;                                                        // +0x2B4
    if (muted)
        setIsTalking(false);                                                  // virtual, +0x28C
}

// voting: 0x80F9820 / 0x80F9840 / 0x80F9850 / 0x80F9870 / 0x80F9880 / 0x80F98A0
void Player::setVotingStatus(PlayerVotingStatus status)
{
    m_votingStatus = static_cast<int>(status);               // +0x2C8
}
Player::PlayerVotingStatus Player::getVotingStatus()
{
    return static_cast<PlayerVotingStatus>(m_votingStatus);  // +0x2C8
}
void Player::setParticipateInVoting(bool participate) { m_participateInVoting = participate; } // +0x2CC
bool Player::getParticipateInVoting()                 { return m_participateInVoting; }   // +0x2CC
void Player::setTargetOfVote(bool target)             { m_targetOfVote = target; }        // +0x2CD
bool Player::getTargetOfVote()                        { return m_targetOfVote; }          // +0x2CD

// Gas is active while the blur-fade countdown is strictly positive.
bool Player::getIsGassed()
{
    return m_field2F4 > 0.0f;                                // +0x2F4, strictly greater
}

// the radio spam filter
// ELF stores the trio at +0x148/+0x14C/+0x150; PE stores it at +0x164/+0x168/+0x16C because the
// ports' preceding STL members have different layouts. Typed member access preserves each build's
// own layout. All seven bodies are plain field operations in both shipped images.
void Player::setRadioSpamFlagCount(int count)          { m_radioSpamFlagCount = count; }
int  Player::getRadioSpamFlagCount()                   { return m_radioSpamFlagCount; }
void Player::setRadioSpamBlockedFlagCount(int count)   { m_radioSpamBlockedFlagCount = count; }
int  Player::getRadioSpamBlockedFlagCount()            { return m_radioSpamBlockedFlagCount; }
void Player::setRadioSpamFilterFlagTimer(float timer)  { m_radioSpamFilterFlagTimer = timer; }
float Player::getRadioSpamFilterFlagTimer()            { return m_radioSpamFilterFlagTimer; }

//  (35 B), PE 0x472D10 (25 B). The -1.0f timer is the same sentinel established by
// the constructor and tested by getIsThisASpam before arming a new counting window.
void Player::resetSpamFilter()
{
    m_radioSpamFlagCount = 0;
    m_radioSpamBlockedFlagCount = 0;
    m_radioSpamFilterFlagTimer = -1.0f;
}

void Player::setReloadState()                        { m_reloadState = true; }        // +0x244


// Route input to the commander camera, free camera or controlled vehicle.
// Overhead-camera handling clears input before any free-camera fallthrough.
// Mouse-look suppresses vehicle steering while held; weapon-select 8 requests
// suicide while driving the free camera.
void Player::handleInput(io::PlayerInput& input, float dt, unsigned int flags)
{
    world::IObject* vehicle = 0;
    if (m_vehicleWeak)
        vehicle = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_vehicleWeak)->target;              // +0x64 -> [+4]

    m_playerInput = input;                                                 // +0x338, 273 bytes

    // the commander camera
    if (m_overheadCameraEnabled)                                           // +0x71
    {
        world::IObject* commanderCam = static_cast<world::IObject*>(
            static_cast<ISquadManager*>(squadManager)->getCommanderCam(getTeam())); // +0x74 / IPlayer +0xE8
        if (commanderCam)
        {
            commanderCam->handlePlayerInput(this, input, dt, flags);       // slot 64
            // THE input IS consumed. Everything below sees zeros.
            for (int i = 0; i <= 63; ++i)
                input.inputs[i] = 0.0f;
        }
    }

    // the free camera
    if (m_freeCameraEnabled)                                               // +0x70, true by default
    {
        world::IObject* freeCamera = 0;
        if (m_freeCameraWeak)
            freeCamera = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_freeCameraWeak)->target;    // +0x6C -> [+4]

        if (inputPressed(input, io::PIMouseLook))
        {
            // "give me the vehicle back" - and with no vehicle, nothing happens at all.
            if (vehicle)
                vehicle->handlePlayerInput(this, input, dt, flags);
            return;
        }

        if (freeCamera)
        {
            freeCamera->handlePlayerInput(this, input, dt, flags);
            if (inputPressed(input, io::PIWeaponSelect8))
                gameLogic->killPlayer(this, true, false);                  // IGameLogic +0x6C
        }
        return;
    }

    // the vehicle
    if (!vehicle)
        return;

    world::IObject* defaultVehicle = 0;
    if (m_defaultVehicleWeak)
        defaultVehicle = static_cast<WeakPtr<world::IObject>::Ctrl*>(m_defaultVehicleWeak)->target;   // +0xB0 -> [+4]

    if (vehicle != defaultVehicle &&
        m_camera && m_camera->getToggleMouseLook())                        // +0x68 / ICameraObject +0x44
    {
        io::PlayerInput filtered = input;
        // Mouse-look suppresses vehicle steering axes; otherwise suppress the mouse-look axes.
        if (inputPressed(input, io::PIMouseLook))
        {
            filtered.setInput(io::PIPitch, 0.0f);   // gcc inlined this one - see the note
            filtered.setInput(io::PIRoll,  0.0f);
            filtered.setInput(io::PIYaw,   0.0f);
        }
        else
        {
            filtered.setInput(io::PIMouseLookX, 0.0f);
            filtered.setInput(io::PIMouseLookY, 0.0f);
        }
        vehicle->handlePlayerInput(this, filtered, dt, flags);
        return;
    }

    vehicle->handlePlayerInput(this, input, dt, flags);
}


// setSpawnGroup - 0x80F9200, 17 B. IPlayer +0x154. One store, and the mirror of getSpawnGroup.
// NO validation AT ALL - any int goes in. uPlayingSpawning's `> 0` test is what makes 0 mean
// "not waiting to spawn", and setTeam below is what puts it back to 0.
void Player::setSpawnGroup(int group)
{
    m_spawnGroup = group;                                            // +0xD4
}

// Change team, posting the event before storing the new value, then clear the
// spawn group. Set a replacement spawn group afterward. The current team is a no-op.
// keepVerifyPending arms verification without resetting attempts; false clears both.
// Requires the nametag manager.
void Player::setTeam(int team, bool keepVerifyPending)
{
    if (m_team == team)                                              // +0xBC
        return;

    // THE payload IS TWO ints ON THE stack, passed by address and not owned by the event.
    int payload[2];
    payload[0] = m_playerId;                                         // +0x9C
    payload[1] = team;
    eventManager->postEvent(ECGameLogic, 8, payload, 0.0f);          // +0x14

    m_team = team;                                                   // +0xBC
    m_spawnGroup = 0;                                                // +0xD4  cleared - see above

    SingletonServer::m_iNametagManager->addTouchItem(this);          // +0x1C, unguarded

    if (keepVerifyPending)
    {
        m_verifyTeamPending = true;                                  // +0xC4
    }
    else
    {
        m_verifyTeamPending = false;
        m_verifyTeamSendCount = 0;                                   // +0xC0
    }

    // Refresh the local human's vote state after changing teams.
    if (!m_isAI &&                                                   // +0xB8
        world::playerManager->getLocalHumanPlayer() == this)         // +0x30
    {
        // Request the vote state on clients; the dedicated implementation is inert.
        ClientVoteManager::getInstance()->requestStateUpdate(getId());   // +0x54
    }
}


// `mov eax, [ebp+arg_0]` and nothing else. See the note at the declaration.
world::IPlayer* getPlayer(world::IPlayer* player)
{
    return player;
}

} // namespace hfe
} // namespace dice
