// Demo playback spectator camera, recovered from the symbolized dedicated ELF.

#include "DemoSpectatorCamera.h"

#include "DemoRecorder.h"
#include "../IGame.h"
#include "../SingletonServer.h"
#include "../Objects/IObject.h"
#include "../Objects/IObjectTemplate.h"
#include "../Objects/IPlayer.h"
#include "../Objects/Base/FreeCamera.h"
#include "../Utility/Vars.h"
#include "../../Core/Ids.h"
#include "../../IO/Console/Console.h"
#include "../../IO/Input/PlayerInput.h"
#include "../../Scene/Object/PlayerManager.h"
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

namespace dice {
namespace hfe {

namespace {

float* g_freeCamSpeed      = Vars::getFloat("demo-fspeed", 100.0f);
int*   g_useOffset         = Vars::getInt("demo-cam-offset", 1);
float* g_offsetSpeed       = Vars::getFloat("demo-cam-offset-speed", 1.0f);
float* g_offsetMinDistance = Vars::getFloat("demo-cam-offset-min", 0.2f);
float* g_offsetSlowFactor  = Vars::getFloat("demo-cam-offset-slow", 0.8f);
float* g_mouseWheelFactor  = Vars::getFloat("demo-cam-mouse-wheel-scale", 0.85f);
int*   g_360capture        = Vars::getInt("demo-cam-360-capture", 0);
float* g_360captureAngle   = Vars::getFloat("demo-cam-360-capture-move", 0.0f);

// Simulation rates, in percent, for weapon keys 1..9. [INFERRED] constness.
const int g_slomoRatios[9] = { 0, 100, 5, 25, 50, 75, 150, 300, 1000 };

// [CHOSEN, not recovered] helper for the mapping test the shipped bodies inline.
inline float mappedInput(const io::PlayerInput& input, io::PlayerInputMap index)
{
    return ((input.mappingFlags >> index) & 1ull) ? input.inputs[index] : 0.0f;
}

// As shipped, a NaN takes the lower bound.
inline float clampFloat(float value, float low, float high)
{
    return value >= low ? (value > high ? high : value) : low;
}

} // namespace

// The four follow fields are left as allocated, as shipped.
CameraUpdate::CameraUpdate()
{
    rotation[3] = 1.0f;
    mode        = 0;
    tick        = 0;
    playerId    = -1;
    rotation[0] = 0.0f;
    rotation[1] = 0.0f;
    rotation[2] = 0.0f;
    position[0] = 0.0f;
    position[1] = 0.0f;
    position[2] = 0.0f;
}

// Stores in the shipped order; m_dword8C is left as allocated.
DemoSpectatorCamera::DemoSpectatorCamera(DemoRecorder* recorder)
{
    m_recorder        = recorder;
    m_keyLatches      = static_cast<bool*>(::operator new(0xC));
    m_currentPlayerId = -1;
    m_mode            = Mode_FreeCamera;
    m_playbackMode    = false;
    m_endOfTrack      = false;
    for (int i = 0; i < 16; ++i)
        m_transform.m[i] = 0.0f;
    m_transform.m[15] = 1.0f;
    m_transform.m[10] = 1.0f;
    m_transform.m[5]  = 1.0f;
    m_transform.m[0]  = 1.0f;
    m_yaw             = 0.0f;
    m_pitch           = 0.0f;
    m_float60         = 0.0f;
    m_updateCount     = 4;
    m_updates         = new CameraUpdate[m_updateCount];
    m_playbackStream  = 0;
    m_recordStream    = 0;
    m_lastUpdateTime  = -1.0;
    m_currentUpdate   = new CameraUpdate;
    m_followDistance  = 2.0f;
    m_lastTicks       = 0;
    m_trackTimeOffset = 0;
    m_dword84         = 0;
    m_dword88         = 0;
    m_followHeight    = 0.0f;
    m_inputArmed      = false;
    m_byte99          = false;
    m_visualTime      = 0.0f;
    for (int i = 0; i < 12; ++i)
        m_keyLatches[i] = false;
}

DemoSpectatorCamera::~DemoSpectatorCamera()
{
    delete m_currentUpdate;
    if (m_updates)
        delete[] m_updates;
    ::operator delete(m_keyLatches);
    if (world::IObject* freeCamera = m_freeCamera.get())
        freeCamera->destroy(true);
}

void DemoSpectatorCamera::setPlaybackMode(bool playback)
{
    m_playbackMode = playback;
}

void DemoSpectatorCamera::setPlaybackStream(io::IStream* stream)
{
    m_endOfTrack     = false;
    m_playbackStream = stream;
}

bool DemoSpectatorCamera::getEOF() const
{
    return m_playbackMode && m_endOfTrack;
}

void DemoSpectatorCamera::setRecordStream(io::IStream* stream)
{
    m_recordStream = stream;
}

world::IPlayer* DemoSpectatorCamera::getCurrentPlayer()
{
    world::IPlayer* player = 0;
    if (m_currentPlayerId >= 0)
    {
        player = world::playerManager->getPlayerFromId(m_currentPlayerId);
        if (!player)
            m_currentPlayerId = -1;
    }
    return player;
}

bool DemoSpectatorCamera::checkSingleKey(const io::PlayerInput& input, io::PlayerInputMap key,
                                         bool& latch)
{
    const bool pressed = static_cast<int>(key) < 64
                      && ((input.mappingFlags >> key) & 1ull) != 0
                      && input.inputs[key] > 0.5f;
    if (pressed)
    {
        if (!latch)
        {
            latch = true;
            return true;
        }
    }
    else
    {
        latch = false;
    }
    return false;
}

void DemoSpectatorCamera::handlePlayerInput(const io::PlayerInput& input, float deltaTime)
{
    if (!ensureCamerasCreated())
        return;

    if (!m_inputArmed)
    {
        if (!m_playbackMode && m_recordStream)
            game->setSimulationRate(0);
        m_inputArmed = true;
        return;
    }

    if (checkSingleKey(input, io::PIAltFire, m_keyLatches[0]))
        nextCamera();
    if (checkSingleKey(input, io::PIAction, m_keyLatches[1]))
    {
        if (mappedInput(input, io::PISprint) == 0.0f)
            nextPlayer();
        else
            previousPlayer();
    }
    if (checkSingleKey(input, io::PIReload, m_keyLatches[2]))
    {
        if (m_recordStream)
            m_recorder->stopCameraRecording();
        io::mainConsole->executeLine(std::string("renderer.stopRenderMovie\n"),
                                     io::Console::ignoredString_,
                                     false, false, false, false, false);
    }
    for (int i = 0; i < 9; ++i)
    {
        if (checkSingleKey(input, static_cast<io::PlayerInputMap>(io::PIWeaponSelect1 + i),
                           m_keyLatches[3 + i]))
            game->setSimulationRate(g_slomoRatios[i]);
    }

    if (m_mode == Mode_FreeCamera && getFreeCamera())
        getFreeCamera()->handlePlayerInput(0, input, deltaTime, 0);
    else
        handleFollowInput(input, deltaTime);
}

bool DemoSpectatorCamera::nextCamera()
{
    nextMode();
    return true;
}

bool DemoSpectatorCamera::nextPlayer()
{
    setMode(Mode_FollowPlayer);
    updateSpectateTarget(1);
    return true;
}

bool DemoSpectatorCamera::previousPlayer()
{
    setMode(Mode_FollowPlayer);
    updateSpectateTarget(-1);
    return true;
}

int DemoSpectatorCamera::getCurrentDemoPlayerId()
{
    return m_currentPlayerId;
}

// Follows the next alive player in a vehicle, wrapping over the 256 ids.
void DemoSpectatorCamera::updateSpectateTarget(int direction)
{
    // The shipped local type: direction 0 keeps the current id as the first candidate.
    struct candidate_t
    {
        candidate_t(int dir, int playerId)
        {
            this->dir = dir;
            id        = playerId;
            if (this->dir < -1)
                this->dir = -1;
            if (this->dir > 1)
                this->dir = 1;
            if (this->dir == 0)
                this->dir = 1;
            else
                id = (this->dir + id + 256) % 256;
        }

        int dir;
        int id;
    };

    const int start = getCurrentPlayer() ? m_currentPlayerId : 0;
    candidate_t candidate(direction, start);
    for (int tries = 0; tries < 256; ++tries)
    {
        world::IPlayer* player = world::playerManager->getPlayerFromId(candidate.id);
        if (player && player->getIsAlive() && player->getVehicle())
        {
            m_currentPlayerId = candidate.id;
            break;
        }
        candidate.id = (candidate.dir + candidate.id + 256) % 256;
    }
    if (!getCurrentPlayer())
        setMode(Mode_FreeCamera);
}

void DemoSpectatorCamera::getPlaybackMatrixAndFOV(Mat4* out, float* fov, float interpolation)
{
    if (fov)
        *fov = 1.0f;
    if (!out)
        return;

    if (!m_playbackMode)
    {
        if (!getFreeCamera() || m_mode != Mode_FreeCamera)
            updateLookAtMatrix();
        else
            m_transform = getFreeCamera()->getTransformation(m_visualTime);
    }
    else
    {
        calcCameraFromUpdates(interpolation);
    }
    *out = m_transform;
}

void DemoSpectatorCamera::updateTicks(int ticks)
{
    if (m_recorder->isOpenForPlayback() && !m_recorder->playbackTickSynced)
        return;
    m_lastTicks = ticks;
    if (!ticks)
        return;
    if (!m_recordStream)
        return;

    // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: demo-camera-track] Camera-track
    // recording, reached only after initRecord's camera arm, which still stops. Retire together
    // with that arm and CameraUpdate::writeToStream.
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoSpectatorCamera::updateTicks camera-track "
                         "recording reached\n");
    std::fflush(stderr);
    std::abort();
}

// The createObject result is not null-checked, as shipped.
// [PLATFORM] BF2.exe reaches the (name, class id) getTemplate at +0x30, not +0x24.
bool DemoSpectatorCamera::ensureCamerasCreated()
{
    if (!getFreeCamera())
    {
        world::IObjectTemplate* const objectTemplate =
            world::objectTemplateManager->getTemplate(std::string("MultiPlayerFreeCamera"),
                                                      world::CID_FreeCameraTemplate);
        if (objectTemplate)
        {
            m_freeCamera = objectTemplate->createObject()->getWeakPtr();
            if (getFreeCamera())
            {
                getFreeCamera()->updateFlags(0, 0x3800);
                Mat4 start;
                for (int i = 0; i < 16; ++i)
                    start.m[i] = 0.0f;
                start.m[15] = 1.0f;
                start.m[10] = 1.0f;
                start.m[5]  = 1.0f;
                start.m[0]  = 1.0f;
                start.m[13] = 75.0f;
                getFreeCamera()->setAbsoluteTransformation(start);
            }
        }
    }
    return getFreeCamera() != 0;
}

void DemoSpectatorCamera::demoVisualUpdate(float time)
{
    m_visualTime = time;
    if (m_playbackMode)
        return;
    if (!ensureCamerasCreated())
        return;

    if (m_mode == Mode_FreeCamera && getFreeCamera())
    {
        getFreeCamera()->handleFrameUpdate(time);
        getFreeCamera()->handleUpdate(time, 0);
    }
    if (SingletonServer::m_iNametagManager)
        SingletonServer::m_iNametagManager->setSpectatedPlayer(getCurrentPlayer());
}

world::FreeCamera* DemoSpectatorCamera::getFreeCamera()
{
    return static_cast<world::FreeCamera*>(m_freeCamera.get());
}

void DemoSpectatorCamera::setMode(Mode mode)
{
    m_mode = mode;
    if (mode == Mode_FreeCamera)
    {
        if (world::FreeCamera* freeCamera = getFreeCamera())
            freeCamera->setAbsoluteTransformation(m_transform);
    }
}

bool DemoSpectatorCamera::getIsFreeFlying()
{
    return m_mode == Mode_FreeCamera;
}

void DemoSpectatorCamera::nextMode()
{
    if (m_mode == Mode_FreeCamera)
    {
        setMode(Mode_FollowPlayer);
        updateSpectateTarget(0);
        if (getCurrentPlayer())
            return;
    }
    setMode(Mode_FreeCamera);
}

// [PLATFORM] BF2.exe multiplies the same factors in another order; the ELF's order is kept.
void DemoSpectatorCamera::handleFollowInput(const io::PlayerInput& input, float deltaTime)
{
    if (*g_360capture != 0)
    {
        if (*g_360captureAngle == 0.0f)
            return;
        m_yaw = *g_360captureAngle + m_yaw;
        Vars::set(std::string("demo-cam-360-capture-move"), std::string("0"));
        return;
    }

    const float lookY    = mappedInput(input, io::PIMouseLookY);
    const float lookX    = mappedInput(input, io::PIMouseLookX);
    const float throttle = mappedInput(input, io::PIThrottle);
    const float up       = mappedInput(input, io::PILie);
    const float down     = mappedInput(input, io::PICrouch);
    const float sprint   = mappedInput(input, io::PISprint);

    m_followDistance = (1.0f - sprint * *g_offsetSlowFactor) * -throttle * *g_offsetSpeed
                     * deltaTime + m_followDistance;
    const float heightInput = (up - down) * 0.5f;
    m_pitch = lookY * deltaTime * *g_freeCamSpeed + m_pitch;
    m_yaw   = lookX * deltaTime * *g_freeCamSpeed + m_yaw;

    m_followHeight = heightInput * *g_offsetSpeed * deltaTime + m_followHeight;
    if (m_followHeight < -10.0f)
        m_followHeight = -10.0f;
    if (m_followHeight > 10.0f)
        m_followHeight = 10.0f;

    m_pitch = clampFloat(m_pitch, -89.0f, 89.0f);

    if (mappedInput(input, io::PINextItem) != 0.0f)
        m_followDistance = *g_mouseWheelFactor * m_followDistance;
    else if (mappedInput(input, io::PIPrevItem) != 0.0f)
        m_followDistance = m_followDistance / *g_mouseWheelFactor;

    m_followDistance = clampFloat(m_followDistance, *g_offsetMinDistance, 1000.0f);
}

// [PLATFORM] The ELF negates a zero x angle (-0.0f); BF2.exe passes 0.0f.
void DemoSpectatorCamera::updateLookAtMatrix()
{
    world::IPlayer* const player = getCurrentPlayer();
    if (!player)
        return;
    world::IObject* const vehicle = player->getVehicle();
    if (!vehicle)
        return;

    world::IObject* const root = world::getRootParent(vehicle);
    float distance = root->getVisualBoundingRadius();
    const float yaw   = ::fmodf(m_yaw, 360.0f);
    const float pitch = ::fmodf(m_pitch, 360.0f);
    const Vec3& position = root->getAbsolutePosition();
    const Vec3 target = { position.x, position.y + m_followHeight, position.z };
    if (*g_useOffset != 0)
        distance = m_followDistance;
    const Vec3 offset = { distance, 0.0f, 0.0f };

    Mat4 rotation = Mat4::getIdentity();
    const Vec3 angles = { yaw, 0.0f, pitch };
    setRotationZXY(rotation, -angles.z, -angles.y, -angles.x);

    const Vec3 up = { 0.0f, 1.0f, 0.0f };
    const Vec3 eye = {
        rotation.m[0] * offset.x + rotation.m[4] * offset.y + rotation.m[8]  * offset.z
            + rotation.m[12] + target.x,
        rotation.m[1] * offset.x + rotation.m[5] * offset.y + rotation.m[9]  * offset.z
            + rotation.m[13] + target.y,
        rotation.m[2] * offset.x + rotation.m[6] * offset.y + rotation.m[10] * offset.z
            + rotation.m[14] + target.z
    };
    calcLookAtMatrix(m_transform, eye, target, up);
}

void DemoSpectatorCamera::calcCameraFromUpdates(float)
{
    // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: demo-camera-track] Camera-track
    // playback, reached only after initPlayback's camera-track arm, which still stops. Retire
    // together with readCameraUpdates and CameraUpdate::readFromStream.
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoSpectatorCamera::calcCameraFromUpdates "
                         "reached\n");
    std::fflush(stderr);
    std::abort();
}

DemoSpectatorCamera::Mode DemoSpectatorCamera::intToMode(int value)
{
    return value == 1 ? Mode_FollowPlayer : Mode_FreeCamera;
}

} // namespace hfe
} // namespace dice
