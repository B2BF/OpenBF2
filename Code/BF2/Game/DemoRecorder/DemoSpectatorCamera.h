// Demo playback spectator camera. Path attested by the ELF's translation-unit initializer; offsets
// are the ELF's. [PLATFORM] BF2.exe aligns the double at +0x74, so its members from there on sit
// 4 bytes later (0xA8 bytes instead of 0xA4).
// [FRONTIER] Camera-track (.bf2cam) recording and playback.

#ifndef GAME_DEMORECORDER_DEMOSPECTATORCAMERA_H
#define GAME_DEMORECORDER_DEMOSPECTATORCAMERA_H

#include "../../Core/WeakPtr.h"
#include "../../IO/Input/Input.h"
#include "../../Math/Mat4.h"

namespace dice {
namespace hfe {

namespace io { class IStream; class PlayerInput; }
namespace world { class FreeCamera; class IObject; class IPlayer; }

class DemoRecorder;

// One camera-track sample. [INFERRED] placement in this translation unit and field names.
struct CameraUpdate
{
    CameraUpdate();

    /* +0x00 */ int   mode;
    /* +0x04 */ int   tick;
    /* +0x08 */ int   playerId;
    /* +0x0C */ float followYaw;
    /* +0x10 */ float followPitch;
    /* +0x14 */ float followDistance;
    /* +0x18 */ float followHeight;
    /* +0x1C */ float rotation[4];
    /* +0x2C */ float position[3];
};

class DemoSpectatorCamera
{
public:
    // [INFERRED] enumerator names.
    enum Mode
    {
        Mode_FreeCamera = 0,
        Mode_FollowPlayer = 1
    };

    explicit DemoSpectatorCamera(DemoRecorder* recorder);
    ~DemoSpectatorCamera();

    void setPlaybackMode(bool playback);
    void setPlaybackStream(io::IStream* stream);
    bool getEOF() const;
    void setRecordStream(io::IStream* stream);

    // Resets the id to -1 when it no longer resolves.
    world::IPlayer* getCurrentPlayer();
    int  getCurrentDemoPlayerId();
    bool getIsFreeFlying();

    // The first call only arms the controls.
    void handlePlayerInput(const io::PlayerInput& input, float deltaTime);
    bool nextCamera();
    bool nextPlayer();
    bool previousPlayer();

    // Either pointer may be null; the fov is always 1.
    void getPlaybackMatrixAndFOV(Mat4* out, float* fov, float interpolation);
    void updateTicks(int ticks);
    void demoVisualUpdate(float time);

    static Mode intToMode(int value);

private:
    // True once per press.
    static bool checkSingleKey(const io::PlayerInput& input, io::PlayerInputMap key, bool& latch);
    void updateSpectateTarget(int direction);
    bool ensureCamerasCreated();
    world::FreeCamera* getFreeCamera();               // [INFERRED] return type
    void setMode(Mode mode);
    void nextMode();
    void handleFollowInput(const io::PlayerInput& input, float deltaTime);
    void updateLookAtMatrix();
    void calcCameraFromUpdates(float interpolation);  // [FRONTIER]

public:
    // [INFERRED] member names; [UNRESOLVED] marks the fields no recovered body explains.
    /* +0x00 */ DemoRecorder*           m_recorder;
    /* +0x04 */ bool*                   m_keyLatches;      // ::operator new / ::operator delete
    /* +0x08 */ int                     m_currentPlayerId; // -1 = none
    /* +0x0C */ WeakPtr<world::IObject> m_freeCamera;      // destroyed by the destructor
    /* +0x10 */ int                     m_mode;
    /* +0x14 */ bool                    m_playbackMode;
    /* +0x15 */ bool                    m_endOfTrack;
    /* +0x18 */ Mat4                    m_transform;
    /* +0x58 */ float                   m_yaw;
    /* +0x5C */ float                   m_pitch;
    /* +0x60 */ float                   m_float60;         // [UNRESOLVED]
    /* +0x64 */ int                     m_updateCount;
    /* +0x68 */ CameraUpdate*           m_updates;
    /* +0x6C */ io::IStream*            m_playbackStream;  // not owned
    /* +0x70 */ io::IStream*            m_recordStream;    // not owned
    /* +0x74 */ double                  m_lastUpdateTime;
    /* +0x7C */ CameraUpdate*           m_currentUpdate;
    /* +0x80 */ int                     m_lastTicks;
    /* +0x84 */ int                     m_dword84;         // [UNRESOLVED]
    /* +0x88 */ int                     m_dword88;         // [UNRESOLVED]
    /* +0x8C */ int                     m_dword8C;         // [UNRESOLVED]
    /* +0x90 */ float                   m_followDistance;
    /* +0x94 */ float                   m_followHeight;
    /* +0x98 */ bool                    m_inputArmed;
    /* +0x99 */ bool                    m_byte99;          // [UNRESOLVED]
    /* +0x9C */ int                     m_trackTimeOffset;
    /* +0xA0 */ float                   m_visualTime;

private:
    DemoSpectatorCamera(const DemoSpectatorCamera&);       // not in the binary; not generated
    DemoSpectatorCamera& operator=(const DemoSpectatorCamera&);
};

} // namespace hfe
} // namespace dice

#endif // GAME_DEMORECORDER_DEMOSPECTATORCAMERA_H
