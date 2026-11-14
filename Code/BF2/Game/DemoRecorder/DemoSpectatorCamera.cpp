// Demo spectator camera: records the local view into a .bf2cam track and replays it.
// Path attested by the anonymous-namespace symbol
// _GLOBAL__N_Game_DemoRecorder_DemoSpectatorCamera.cpp...g_freeCamSpeed (ELF 0x8154610).
// Recovered from the symbolized dedicated ELF; every body was read there. The client BF2.exe twins
// (0x6298C0..0x62AEFF) match the ELF bodies; none of the ELF bodies is compiled down.
// The static string array dice::hfe::defaultSquadNames this TU's initializer also builds (ELF
// 0x8154163..0x8154404) comes from an included squad header, not from this file; it is not used here.

#include "DemoSpectatorCamera.h"

#include "DemoRecorder.h"                     // isOpenForPlayback, tickResynced, demoConfig, stopCameraRecording
#include "../IGame.h"                         // game->setSimulationRate (+0xE4), queryInterface
#include "../SingletonServer.h"               // m_iNametagManager->setSpectatedPlayer (+0x60)
#include "../Utility/Vars.h"                  // the demo-cam-* console variables
#include "../Objects/IObject.h"
#include "../Objects/IObjectTemplate.h"       // createObject (+0x54)
#include "../Objects/IPlayer.h"               // getVehicle +0x48, getId +0x54, getIsAlive +0x6C
#include "../Objects/ICameraObject.h"         // getTransformation (+0x10)
#include "../Objects/Base/FreeCamera.h"       // the free camera's own table (+0x180 getTransformation)
#include "../../Core/Ids.h"                   // IID_IGameServer, CID_FreeCameraTemplate, IID_IPlayerControlObject
#include "../../Core/System.h"                // WorldPref::getTickCount
#include "../../IO/Console/Console.h"         // mainConsole->executeLine, Console::ignoredString_
#include "../../IO/Stream/IStream.h"          // read +0x10, write +0x14, getAvailableBytes +0x1C
#include "../../Scene/Object/PlayerManager.h" // getPlayerFromId +0x20, getLocalHumanPlayer +0x30
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // getTemplate(name, cid) +0x28
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"    // getActiveCamera +0x44

#include <cmath>                              // fmodf
#include <cstring>                            // memset
#include <string>

namespace dice {
namespace hfe {

namespace {

// The TU's console variables, registered by the static initializer (ELF 0x815442D..0x8154532)
// in this order. Each holds the live storage address Vars hands back.
float* g_freeCamSpeed      = Vars::getFloat("demo-fspeed", 100.0f);              // ds:0x8AACC80
int*   g_useOffset         = Vars::getInt("demo-cam-offset", 1);                 // ds:0x8AACC84
float* g_offsetSpeed       = Vars::getFloat("demo-cam-offset-speed", 1.0f);      // ds:0x8AACC88
float* g_offsetMinDistance = Vars::getFloat("demo-cam-offset-min", 0.2f);        // ds:0x8AACC8C
float* g_offsetSlowFactor  = Vars::getFloat("demo-cam-offset-slow", 0.8f);       // ds:0x8AACC90
float* g_mouseWheelFactor  = Vars::getFloat("demo-cam-mouse-wheel-scale", 0.85f); // ds:0x8AACC94
int*   g_360capture        = Vars::getInt("demo-cam-360-capture", 0);            // ds:0x8AACC98
float* g_360captureAngle   = Vars::getFloat("demo-cam-360-capture-move", 0.0f);  // ds:0x8AACC9C

// ds:0x8893E20 (.rodata): the simulation rates PIWeaponSelect1..9 pick.
const int g_slomoRatios[9] = { 0, 100, 5, 25, 50, 75, 150, 300, 1000 };

// [CHOSEN, not recovered] spelling of the inline the ELF expands at every analog read: the value
// when the input's mapping bit is set, otherwise 0.0f (fldz). The retail inline is not in the tree.
inline float mappedInput(const io::PlayerInput& input, io::PlayerInputMap index)
{
    if ((input.mappingFlags >> (static_cast<unsigned int>(index) & 0x3Fu)) & 1ULL)
        return input.inputs[index];
    return 0.0f;
}

// ELF 0x8154690 (W) float lerp<float> and 0x8154630 (W) Vec3 lerp<Vec3> / PE 0x629DC0.
// Both compute (1 - t) * a + t * b. They are instances of one anonymous-namespace template in the
// ELF; the tree's Vec3 has no arithmetic operators, so the two instances are spelled as explicit
// specializations [CHOSEN, not recovered].
template <class T> T lerp(T a, T b, float t);

template <> float lerp<float>(float a, float b, float t)
{
    return (1.0f - t) * a + t * b;
}

template <> Vec3 lerp<Vec3>(Vec3 a, Vec3 b, float t)
{
    const float s = 1.0f - t;
    Vec3 result = { s * a.x + t * b.x, s * a.y + t * b.y, s * a.z + t * b.z };
    return result;
}

// [CHOSEN, not recovered] spelling of the identity fill the constructors and ensureCamerasCreated
// expand: sixteen zero stores, then 1.0f on the diagonal (the tree's Mat4 has no such inline).
inline void setIdentity(Mat4& m)
{
    for (int i = 0; i < 16; ++i)
        m.m[i] = 0.0f;
    m.m[15] = 1.0f;
    m.m[10] = 1.0f;
    m.m[5]  = 1.0f;
    m.m[0]  = 1.0f;
}

} // namespace

// construction

// ELF 0x8151F50 (C1) / 0x81520B0 (C2), 343 B each.
DemoSpectatorCamera::DemoSpectatorCamera(DemoRecorder* owner)
{
    m_owner           = owner;
    m_keyLatches      = new KeyLatches;                  // operator new(0xC)
    m_currentPlayerId = -1;
    m_mode            = Mode_FreeFlying;
    m_playbackMode    = false;
    m_eof             = false;
    setIdentity(m_matrix);
    m_yaw             = 0.0f;
    m_pitch           = 0.0f;
    m_numUpdates      = 0;
    m_maxUpdates      = 4;
    m_updates         = new CameraUpdate[4];             // operator new[](0xE0), no cookie
    m_playbackStream  = 0;
    m_recordStream    = 0;
    m_double74        = -1.0;
    m_spareUpdate     = new CameraUpdate;                // operator new(0x38)
    m_followDistance  = 2.0f;
    m_ticks           = 0;
    m_tickOffset      = 0;
    m_float84         = 0.0f;
    m_float88         = 0.0f;
    m_heightOffset    = 0.0f;
    m_inputPrimed     = false;
    m_tickOffsetSet   = false;
    m_visualTime      = 0.0f;
    std::memset(m_keyLatches, 0, sizeof(KeyLatches));    // rep stosd, 3 dwords
}

// ELF 0x8152210 / 0x81522A0, 138 B. The free camera is destroyed (IObject::destroy(true), slot
// +0x34) before the member WeakPtr drops its control block.
DemoSpectatorCamera::~DemoSpectatorCamera()
{
    delete m_spareUpdate;
    delete[] m_updates;
    delete m_keyLatches;
    if (m_freeCamera.get())
        m_freeCamera.get()->destroy(true);
}

// trivial accessors

// ELF 0x8152330
void DemoSpectatorCamera::setPlaybackMode(bool playback)
{
    m_playbackMode = playback;
}

// ELF 0x8152340. A new stream clears the end-of-track flag.
void DemoSpectatorCamera::setPlaybackStream(io::IStream* stream)
{
    m_eof            = false;
    m_playbackStream = stream;
}

// ELF 0x8152360 / PE 0x6298E0
bool DemoSpectatorCamera::getEOF() const
{
    return m_playbackMode && m_eof;
}

// ELF 0x8152380
void DemoSpectatorCamera::setRecordStream(io::IStream* stream)
{
    m_recordStream = stream;
}

// ELF 0x8152390. A stale id is reset to -1 when the player is gone.
world::IPlayer* DemoSpectatorCamera::getCurrentPlayer()
{
    if (m_currentPlayerId < 0)
        return 0;
    world::IPlayer* player = world::playerManager->getPlayerFromId(m_currentPlayerId);
    if (!player)
        m_currentPlayerId = -1;
    return player;
}

// ELF 0x8152790 / PE 0x68E7B0
int DemoSpectatorCamera::getCurrentDemoPlayerId()
{
    return m_currentPlayerId;
}

// ELF 0x81531A0
world::FreeCamera* DemoSpectatorCamera::getFreeCamera()
{
    return static_cast<world::FreeCamera*>(m_freeCamera.get());
}

// ELF 0x8153210 / PE 0x629990
bool DemoSpectatorCamera::getIsFreeFlying()
{
    return m_mode == Mode_FreeFlying;
}

// ELF 0x81540F0
DemoSpectatorCamera::Mode DemoSpectatorCamera::intToMode(int value)
{
    switch (value)
    {
    case 0:  return Mode_FreeFlying;
    case 1:  return Mode_FollowPlayer;
    default: return Mode_FreeFlying;
    }
}

// input

// ELF 0x81523D0. Edge detector: true only on the frame a mapped input crosses 0.5. An id above 63
// has no mapping bit and reads as released.
bool DemoSpectatorCamera::checkSingleKey(const io::PlayerInput& input, io::PlayerInputMap key,
                                         bool& latch)
{
    bool pressed = false;
    if (static_cast<int>(key) <= 63
        && ((input.mappingFlags >> (static_cast<unsigned int>(key) & 0x3Fu)) & 1ULL))
        pressed = input.inputs[key] > 0.5f;

    if (pressed)
    {
        if (!latch)
        {
            latch = true;
            return true;
        }
        return false;
    }
    latch = false;
    return false;
}

// ELF 0x8152490 / PE 0x62ACF0. The first call only primes (and, while recording live, pauses the
// simulation); later calls dispatch the demo keys, then feed the active camera.
void DemoSpectatorCamera::handlePlayerInput(const io::PlayerInput& input, float deltaTime)
{
    if (!ensureCamerasCreated())
        return;

    if (!m_inputPrimed)
    {
        if (!m_playbackMode && m_recordStream)
            game->setSimulationRate(0);
        m_inputPrimed = true;
        return;
    }

    if (checkSingleKey(input, io::PIAltFire, m_keyLatches->latched[0]))
        nextCamera();

    if (checkSingleKey(input, io::PIAction, m_keyLatches->latched[1]))
    {
        if (mappedInput(input, io::PISprint) == 0.0f)
            nextPlayer();
        else
            previousPlayer();
    }

    if (checkSingleKey(input, io::PIReload, m_keyLatches->latched[2]))
    {
        if (m_recordStream)
            m_owner->stopCameraRecording();
        io::mainConsole->executeLine(std::string("renderer.stopRenderMovie\n"),
                                     io::Console::ignoredString_, false, false, false, false, false);
    }

    for (int i = 0; i <= 8; ++i)
    {
        if (checkSingleKey(input, static_cast<io::PlayerInputMap>(io::PIWeaponSelect1 + i),
                           m_keyLatches->latched[3 + i]))
            game->setSimulationRate(g_slomoRatios[i]);
    }

    if (m_mode == Mode_FreeFlying && getFreeCamera())
        getFreeCamera()->handlePlayerInput(0, input, deltaTime, 0);     // IObject slot +0x100
    else
        handleFollowInput(input, deltaTime);
}

// ELF 0x81526F0 / PE 0x62ABF0
bool DemoSpectatorCamera::nextCamera()
{
    nextMode();
    return true;
}

// ELF 0x8152710 / PE 0x62AC00
bool DemoSpectatorCamera::nextPlayer()
{
    setMode(Mode_FollowPlayer);
    updateSpectateTarget(1);
    return true;
}

// ELF 0x8152750 / PE 0x62AC20
bool DemoSpectatorCamera::previousPlayer()
{
    setMode(Mode_FollowPlayer);
    updateSpectateTarget(-1);
    return true;
}

// ELF 0x81527A0 / PE 0x62A730. Walks the 256 player ids from the current one in the given
// direction and follows the first live player that has a vehicle; falls back to free flight.
void DemoSpectatorCamera::updateSpectateTarget(int direction)
{
    // ELF 0x8154540 candidate_t::candidate_t(int, int). The step is clamped to -1/+1; a zero step
    // becomes +1 without advancing, so direction 0 re-tests the current id first.
    struct candidate_t
    {
        int step;   // +0x00
        int id;     // +0x04

        candidate_t(int dir, int start)
            : step(dir), id(start)
        {
            if (step < -1)
                step = -1;
            if (step > 1)
                step = 1;
            if (step != 0)
                advance();
            else
                step = 1;
        }

        // [INFERRED] inline: the same signed (id + step + 256) % 256 the loop expands.
        void advance()
        {
            id = (id + step + 256) % 256;
        }
    };

    candidate_t candidate(direction, getCurrentPlayer() ? m_currentPlayerId : 0);
    for (int i = 0; i < 256; ++i)
    {
        world::IPlayer* player = world::playerManager->getPlayerFromId(candidate.id);
        if (player && player->getIsAlive() && player->getVehicle())
        {
            m_currentPlayerId = candidate.id;
            break;
        }
        candidate.advance();
    }

    if (!getCurrentPlayer())
        setMode(Mode_FreeFlying);
}

// ELF 0x8153290 / PE 0x62A300. Follow-mode controls: mouse look turns, throttle zooms (sprint
// slows it), lie/crouch move the look-at height, the next/prev item keys scale the distance.
void DemoSpectatorCamera::handleFollowInput(const io::PlayerInput& input, float deltaTime)
{
    if (*g_360capture)
    {
        // 360 capture: one externally driven yaw step per request, then the request is consumed.
        if (*g_360captureAngle != 0.0f)
        {
            m_yaw += *g_360captureAngle;
            Vars::set(std::string("demo-cam-360-capture-move"), std::string("0"));
        }
        return;
    }

    const float lookY    = mappedInput(input, io::PIMouseLookY);
    const float lookX    = mappedInput(input, io::PIMouseLookX);
    const float throttle = mappedInput(input, io::PIThrottle);
    const float lie      = mappedInput(input, io::PILie);
    const float crouch   = mappedInput(input, io::PICrouch);
    const float sprint   = mappedInput(input, io::PISprint);

    m_followDistance += -throttle * (1.0f - sprint * *g_offsetSlowFactor) * *g_offsetSpeed * deltaTime;
    m_pitch          += lookY * deltaTime * *g_freeCamSpeed;
    m_yaw            += lookX * deltaTime * *g_freeCamSpeed;
    m_heightOffset   += (lie - crouch) * 0.5f * *g_offsetSpeed * deltaTime;

    if (m_heightOffset < -10.0f)
        m_heightOffset = -10.0f;
    if (m_heightOffset > 10.0f)
        m_heightOffset = 10.0f;

    if (m_pitch < -89.0f)
        m_pitch = -89.0f;
    else if (m_pitch > 89.0f)
        m_pitch = 89.0f;

    if (mappedInput(input, io::PINextItem) != 0.0f)
        m_followDistance = *g_mouseWheelFactor * m_followDistance;
    else if (mappedInput(input, io::PIPrevItem) != 0.0f)
        m_followDistance = m_followDistance / *g_mouseWheelFactor;

    const float minDistance = *g_offsetMinDistance;
    if (m_followDistance < minDistance)
        m_followDistance = minDistance;
    else if (m_followDistance > 1000.0f)
        m_followDistance = 1000.0f;
}

// modes and the free camera

// ELF 0x8152F50 / PE 0x62A830. Creates the MultiPlayerFreeCamera object on first use and parks it
// 75 m up at the origin. Returns whether a free camera exists.
bool DemoSpectatorCamera::ensureCamerasCreated()
{
    if (!getFreeCamera())
    {
        world::IObjectTemplate* objectTemplate = world::objectTemplateManager->getTemplate(
            std::string("MultiPlayerFreeCamera"), world::CID_FreeCameraTemplate);
        if (objectTemplate)
        {
            m_freeCamera = objectTemplate->createObject()->getWeakPtr();
            if (getFreeCamera())
            {
                getFreeCamera()->updateFlags(0, 0x3800);                  // slot +0x60
                Mat4 transform;
                setIdentity(transform);
                transform.m[13] = 75.0f;
                getFreeCamera()->setAbsoluteTransformation(transform);   // slot +0x8C
            }
        }
    }
    return getFreeCamera() != 0;
}

// ELF 0x81531C0. Entering free flight hands the current view to the free camera.
void DemoSpectatorCamera::setMode(Mode mode)
{
    m_mode = mode;
    if (mode == Mode_FreeFlying)
    {
        if (world::FreeCamera* freeCamera = getFreeCamera())
            freeCamera->setAbsoluteTransformation(m_matrix);
    }
}

// ELF 0x8153230 / PE 0x62AA10 (GameClient also calls the PE copy directly).
void DemoSpectatorCamera::nextMode()
{
    if (m_mode == Mode_FreeFlying)
    {
        setMode(Mode_FollowPlayer);
        updateSpectateTarget(0);
        if (getCurrentPlayer())
            return;
    }
    setMode(Mode_FreeFlying);
}

// per-frame

// ELF 0x8153100 / PE 0x62A950. Only the live view (not a playing track) steps the free camera.
void DemoSpectatorCamera::demoVisualUpdate(float time)
{
    m_visualTime = time;
    if (m_playbackMode)
        return;
    if (!ensureCamerasCreated())
        return;

    if (m_mode == Mode_FreeFlying && getFreeCamera())
    {
        getFreeCamera()->handleFrameUpdate(time);                         // slot +0xAC
        getFreeCamera()->handleUpdate(time, 0);                           // slot +0xB0
    }

    INametagManager* nametags = SingletonServer::m_iNametagManager;
    if (nametags)
        nametags->setSpectatedPlayer(getCurrentPlayer());
}

// ELF 0x8152890 / PE 0x62AC40. The FOV out-parameter is always 1.0f.
void DemoSpectatorCamera::getPlaybackMatrixAndFOV(Mat4* matrix, float* fov, float time)
{
    if (fov)
        *fov = 1.0f;
    if (!matrix)
        return;

    if (m_playbackMode)
        calcCameraFromUpdates(time);
    else if (!getFreeCamera() || m_mode != Mode_FreeFlying)
        updateLookAtMatrix();
    else
        m_matrix = getFreeCamera()->getTransformation(m_visualTime);     // FreeCamera +0x180

    *matrix = m_matrix;
}

// ELF 0x81535B0 / PE 0x629B10. Orbit the followed player's root vehicle: yaw/pitch rotate an
// offset of length followDistance (or the vehicle's visual radius when demo-cam-offset is 0)
// around the vehicle position raised by heightOffset.
void DemoSpectatorCamera::updateLookAtMatrix()
{
    world::IPlayer* player = getCurrentPlayer();
    if (!player)
        return;
    world::IObject* vehicle = player->getVehicle();
    if (!vehicle)
        return;

    world::IObject* root = world::getRootParent(vehicle);
    const float radius = root->getVisualBoundingRadius();                 // slot +0x94
    const float yaw    = std::fmod(m_yaw, 360.0f);
    const float pitch  = std::fmod(m_pitch, 360.0f);

    const Vec3& rootPosition = root->getAbsolutePosition();               // slot +0x6C
    const Vec3 target = { rootPosition.x, rootPosition.y + m_heightOffset, rootPosition.z };

    Vec3 offset;
    offset.x = *g_useOffset ? m_followDistance : radius;
    offset.y = 0.0f;
    offset.z = 0.0f;

    Mat4 rotation = Mat4::getIdentity();
    const Vec3 angles = { yaw, 0.0f, pitch };
    setRotationZXY(rotation, -angles.z, -angles.y, -angles.x);

    const float* m = rotation.m;
    const Vec3 eye = {
        offset.x * m[0] + offset.y * m[4] + offset.z * m[8]  + m[12] + target.x,
        offset.x * m[1] + offset.y * m[5] + offset.z * m[9]  + m[13] + target.y,
        offset.x * m[2] + offset.y * m[6] + offset.z * m[10] + m[14] + target.z
    };
    const Vec3 up = { 0.0f, 1.0f, 0.0f };
    calcLookAtMatrix(m_matrix, eye, target, up);
}

// recording

// ELF 0x8152A10 / PE 0x62A000. While recording (or once playback has resynced the tick) samples
// the camera, smooths it over the last four samples and writes one CameraUpdate per elapsed tick.
// A listen server records the local player's vehicle camera instead of the spectator view.
void DemoSpectatorCamera::updateTicks(int ticks)
{
    if (m_owner->isOpenForPlayback() && !m_owner->tickResynced)
        return;

    m_ticks = ticks;
    if (ticks == 0 || !m_recordStream)
        return;

    CameraUpdate update;
    update.tick = static_cast<unsigned int>(WorldPref::getTickCount());

    world::IPlayer* localPlayer = world::playerManager->getLocalHumanPlayer();
    if (localPlayer && game->queryInterface(IID_IGameServer))
    {
        update.mode = Mode_FreeFlying;
        world::IObject* vehicle = localPlayer->getVehicle();
        if (!vehicle)
        {
            const Vec3 parked = { 0.0f, 100.0f, 0.0f };
            update.position = parked;
            update.rotation.fromMat(Mat4::getIdentity());
        }
        else
        {
            world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
                vehicle->queryInterface(world::IID_IPlayerControlObject));
            if (pco)
            {
                world::ICameraObject* camera = pco->getActiveCamera();
                if (camera)
                {
                    const Mat4 transform = camera->getTransformation(m_visualTime);
                    update.position.x = transform.m[12];
                    update.position.y = transform.m[13];
                    update.position.z = transform.m[14];
                    update.rotation.fromMat(transform);
                }
            }
        }
    }
    else
    {
        update.playerId       = getCurrentPlayer() ? getCurrentPlayer()->getId() : -1;
        update.yaw            = m_yaw;
        update.pitch          = m_pitch;
        update.followDistance = m_followDistance;
        update.heightOffset   = m_heightOffset;
        update.mode           = m_mode;
        update.position.x     = m_matrix.m[12];
        update.position.y     = m_matrix.m[13];
        update.position.z     = m_matrix.m[14];
        update.rotation.fromMat(m_matrix);
    }

    // shift the history and append the new sample
    for (int i = 1; i < m_maxUpdates; ++i)
        m_updates[i - 1] = m_updates[i];
    m_updates[m_maxUpdates - 1] = update;

    // the written position is the history's mean...
    Vec3 sum = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < m_maxUpdates; ++i)
    {
        sum.x += m_updates[i].position.x;
        sum.y += m_updates[i].position.y;
        sum.z += m_updates[i].position.z;
    }
    const float scale = 1.0f / static_cast<float>(m_maxUpdates);
    update.position.x = sum.x * scale;
    update.position.y = sum.y * scale;
    update.position.z = sum.z * scale;

    // ...and its rotation a chain of half-way slerps over entries 0..3 (fixed, as shipped).
    Quat first  = { 0.0f, 0.0f, 0.0f, 1.0f };
    Quat second = { 0.0f, 0.0f, 0.0f, 1.0f };
    first.slerp(m_updates[0].rotation, m_updates[1].rotation, 0.5f);
    second.slerp(first, m_updates[2].rotation, 0.5f);
    update.rotation.slerp(second, m_updates[3].rotation, 0.5f);

    for (int i = 0; i < ticks; ++i)
    {
        update.writeToStream(m_recordStream);
        ++update.tick;
    }
}

// playback

// ELF 0x8153A40 / PE 0x62A570. Drops samples older than the current tick, then refills the
// four-entry window from the track. Without demoConfig.syncCameraAndDemo the first sample read
// fixes a tick offset that maps track ticks onto the current world tick. Returns whether the
// window is full; an empty or exhausted stream sets the end-of-track flag.
bool DemoSpectatorCamera::readCameraUpdates()
{
    if (!m_playbackStream || !m_playbackStream->getAvailableBytes())
    {
        m_eof = true;
        return false;
    }

    const unsigned int now = static_cast<unsigned int>(WorldPref::getTickCount());

    for (int i = 0; m_numUpdates > 0 && i < m_maxUpdates; ++i)
    {
        if (m_updates[0].tick >= now)
            break;
        for (int j = 0; j < m_numUpdates - 1; ++j)
            m_updates[j] = m_updates[j + 1];
        --m_numUpdates;
    }

    const int toRead = m_maxUpdates - m_numUpdates;
    if (toRead == 0)
        return true;

    for (int k = 0; k < toRead; ++k)
    {
        const int index = m_maxUpdates - toRead + k;
        if (!m_playbackStream->getAvailableBytes())
            break;

        m_updates[index].readFromStream(m_playbackStream);
        if (!m_tickOffsetSet && !m_owner->demoConfig.syncCameraAndDemo)
            m_tickOffset = static_cast<int>(now - m_updates[index].tick);
        m_tickOffsetSet = true;

        m_updates[index].tick += m_tickOffset;
        if (m_updates[index].tick < now)
            --k;                                     // stale: read the next sample into this slot
        else
            ++m_numUpdates;
    }
    return m_numUpdates == m_maxUpdates;
}

// ELF 0x8153880 / PE 0x62AAD0. Interpolates between window entries 0 and 1: free-mode samples
// carry a full transform, follow-mode samples the orbit parameters and the followed player.
void DemoSpectatorCamera::calcCameraFromUpdates(float time)
{
    if (!readCameraUpdates())
        return;

    const CameraUpdate* from = &m_updates[0];
    const CameraUpdate* to   = &m_updates[1];

    if (from->mode == Mode_FreeFlying)
    {
        Quat rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        rotation.slerp(from->rotation, to->rotation, time);
        rotation.toMat(m_matrix);
        const Vec3 position = lerp<Vec3>(from->position, to->position, time);
        m_matrix.m[12] = position.x;
        m_matrix.m[13] = position.y;
        m_matrix.m[14] = position.z;
    }
    else
    {
        m_yaw             = lerp<float>(from->yaw, to->yaw, time);
        m_pitch           = lerp<float>(from->pitch, to->pitch, time);
        m_followDistance  = lerp<float>(from->followDistance, to->followDistance, time);
        m_heightOffset    = lerp<float>(from->heightOffset, to->heightOffset, time);
        m_currentPlayerId = from->playerId;
        updateLookAtMatrix();
    }
}

// CameraUpdate

// ELF 0x8153C80 / 0x8153CC0. +0x0C..+0x18 stay uninitialized, as shipped.
CameraUpdate::CameraUpdate()
{
    mode       = DemoSpectatorCamera::Mode_FreeFlying;
    tick       = 0;
    playerId   = -1;
    rotation.x = 0.0f;
    rotation.y = 0.0f;
    rotation.z = 0.0f;
    rotation.w = 1.0f;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
}

// ELF 0x8153D00. Record: tick, mode, then either the free-mode quaternion (x y z w) and position,
// or the follow-mode player id and orbit parameters; every field 4 bytes. The stream's
// return values are ignored.
void CameraUpdate::writeToStream(io::IStream* stream) const
{
    stream->write(&tick, 4);
    stream->write(&mode, 4);
    if (mode == DemoSpectatorCamera::Mode_FreeFlying)
    {
        stream->write(&rotation.x, 4);
        stream->write(&rotation.y, 4);
        stream->write(&rotation.z, 4);
        stream->write(&rotation.w, 4);
        stream->write(&position.x, 4);
        stream->write(&position.y, 4);
        stream->write(&position.z, 4);
    }
    else
    {
        stream->write(&playerId, 4);
        stream->write(&yaw, 4);
        stream->write(&pitch, 4);
        stream->write(&followDistance, 4);
        stream->write(&heightOffset, 4);
    }
}

// ELF 0x8153F20. The inverse of writeToStream; an unknown mode reads as free mode, and a
// free-mode position with squared length above 1e10 is discarded (zeroed).
void CameraUpdate::readFromStream(io::IStream* stream)
{
    stream->read(&tick, 4);
    int rawMode;
    stream->read(&rawMode, 4);
    mode = DemoSpectatorCamera::intToMode(rawMode);
    if (mode == DemoSpectatorCamera::Mode_FreeFlying)
    {
        stream->read(&rotation.x, 4);
        stream->read(&rotation.y, 4);
        stream->read(&rotation.z, 4);
        stream->read(&rotation.w, 4);
        stream->read(&position.x, 4);
        stream->read(&position.y, 4);
        stream->read(&position.z, 4);
        if (position.x * position.x + position.y * position.y + position.z * position.z > 1.0e10f)
        {
            position.x = 0.0f;
            position.y = 0.0f;
            position.z = 0.0f;
        }
    }
    else
    {
        stream->read(&playerId, 4);
        stream->read(&yaw, 4);
        stream->read(&pitch, 4);
        stream->read(&followDistance, 4);
        stream->read(&heightOffset, 4);
    }
}

} // namespace hfe
} // namespace dice
