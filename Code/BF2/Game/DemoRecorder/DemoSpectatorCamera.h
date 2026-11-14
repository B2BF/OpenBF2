// Demo spectator camera: the free-flying / follow-player camera rig a DemoRecorder owns while it
// records or plays a .bf2cam camera track. Recovered from the symbolized dedicated ELF
// (0x8151F50..0x8154690, TU Game/DemoRecorder/DemoSpectatorCamera.cpp by its anonymous-namespace
// symbol) and cross-checked against BF2.exe 0x6298C0..0x62AEFF.
// Nonpolymorphic. ELF size 0xA4 (DemoRecorder::initPlayback allocates operator new(0xA4)); the
// PE layout differs from +0x74 on (MSVC aligns the double to 8), so the offsets below are ELF only.

#ifndef GAME_DEMORECORDER_DEMOSPECTATORCAMERA_H     // [INFERRED] guard style
#define GAME_DEMORECORDER_DEMOSPECTATORCAMERA_H

#include "../../Core/WeakPtr.h"
#include "../../Math/Mat4.h"
#include "../../Math/Quat.h"
#include "../../Math/Vec3.h"
#include "../../IO/Input/PlayerInput.h"     // io::PlayerInput / io::PlayerInputMap

namespace dice {
namespace hfe {

namespace io { class IStream; }
namespace world { class IObject; class IPlayer; class FreeCamera; }

class DemoRecorder;
struct CameraUpdate;

class DemoSpectatorCamera
{
public:
    // The type name is attested by setMode(Mode)'s mangling. Only 0 and 1 exist (intToMode maps
    // everything else to 0). Enumerator spellings [CHOSEN, not recovered]; 0 is the free-flying
    // mode (getIsFreeFlying tests == 0), 1 follows a player.
    enum Mode
    {
        Mode_FreeFlying   = 0,
        Mode_FollowPlayer = 1
    };

    explicit DemoSpectatorCamera(DemoRecorder* owner);   // ELF 0x8151F50 (C1) / 0x81520B0 (C2)
    ~DemoSpectatorCamera();                              // ELF 0x8152210 / 0x81522A0

    // what DemoRecorder drives
    void setPlaybackMode(bool playback);                 // ELF 0x8152330
    void setPlaybackStream(io::IStream* stream);         // ELF 0x8152340
    bool getEOF() const;                                 // ELF 0x8152360 / PE 0x6298E0
    void setRecordStream(io::IStream* stream);           // ELF 0x8152380
    void updateTicks(int ticks);                         // ELF 0x8152A10 / PE 0x62A000
    void handlePlayerInput(const io::PlayerInput& input, float deltaTime);   // ELF 0x8152490 / PE 0x62ACF0
    void demoVisualUpdate(float time);                   // ELF 0x8153100 / PE 0x62A950
    void getPlaybackMatrixAndFOV(Mat4* matrix, float* fov, float time);      // ELF 0x8152890 / PE 0x62AC40
    bool nextCamera();                                   // ELF 0x81526F0 / PE 0x62ABF0
    bool nextPlayer();                                   // ELF 0x8152710 / PE 0x62AC00
    bool previousPlayer();                               // ELF 0x8152750 / PE 0x62AC20
    int  getCurrentDemoPlayerId();                       // ELF 0x8152790 / PE 0x68E7B0 (ICF-shared)
    bool getIsFreeFlying();                              // ELF 0x8153210 / PE 0x629990
    world::IPlayer* getCurrentPlayer();                  // ELF 0x8152390
    // [INFERRED] return type: every call through it dispatches the FreeCamera table (ELF vtable
    // +0x180 = FreeCamera::getTransformation(float)); the object is the CID_FreeCameraTemplate
    // template's createObject() result.
    world::FreeCamera* getFreeCamera();                  // ELF 0x81531A0
    void setMode(Mode mode);                             // ELF 0x81531C0
    void nextMode();                                     // ELF 0x8153230 / PE 0x62AA10

    static Mode intToMode(int value);                    // ELF 0x81540F0 (no this)

    // Helpers. Access is not visible in the Itanium manglings; private is [INFERRED].
private:
    // ELF 0x81523D0 - static (the callers pass no this).
    static bool checkSingleKey(const io::PlayerInput& input, io::PlayerInputMap key, bool& latch);
    void updateSpectateTarget(int direction);            // ELF 0x81527A0
    bool ensureCamerasCreated();                         // ELF 0x8152F50 / PE 0x62A830
    void handleFollowInput(const io::PlayerInput& input, float deltaTime);   // ELF 0x8153290 / PE 0x62A300
    void updateLookAtMatrix();                           // ELF 0x81535B0 / PE 0x629B10
    void calcCameraFromUpdates(float time);              // ELF 0x8153880 / PE 0x62AAD0
    bool readCameraUpdates();                            // ELF 0x8153A40 / PE 0x62A570

    // [CHOSEN, not recovered] type for the +0x04 block: operator new(0xC) / operator delete
    // (scalar forms, so not a new[] array), zeroed by a 3-dword rep stosd, then handed out as
    // bool& at +0..+11. Index 0 = PIAltFire, 1 = PIAction, 2 = PIReload, 3..11 = PIWeaponSelect1..9.
    struct KeyLatches
    {
        bool latched[12];
    };

    // Member names are [INFERRED] from their readers and writers unless noted.
    /* +0x00 */ DemoRecorder*            m_owner;
    /* +0x04 */ KeyLatches*              m_keyLatches;
    /* +0x08 */ int                      m_currentPlayerId;   // ctor -1
    /* +0x0C */ WeakPtr<world::IObject>  m_freeCamera;        // the MultiPlayerFreeCamera object
    /* +0x10 */ Mode                     m_mode;
    /* +0x14 */ bool                     m_playbackMode;      // setPlaybackMode
    /* +0x15 */ bool                     m_eof;               // set when the camera track runs dry
    /* +0x18 */ Mat4                     m_matrix;            // ctor identity; the output transform
    /* +0x58 */ float                    m_yaw;               // degrees
    /* +0x5C */ float                    m_pitch;             // degrees, clamped to +-89
    /* +0x60 */ int                      m_numUpdates;        // valid entries in m_updates
    /* +0x64 */ int                      m_maxUpdates;        // ctor 4
    /* +0x68 */ CameraUpdate*            m_updates;           // new CameraUpdate[4] (0xE0, no cookie)
    /* +0x6C */ io::IStream*             m_playbackStream;    // not owned
    /* +0x70 */ io::IStream*             m_recordStream;      // not owned
    /* +0x74 */ double                   m_double74;          // [UNRESOLVED] ctor -1.0, never read in this TU
    /* +0x7C */ CameraUpdate*            m_spareUpdate;       // [UNRESOLVED] ctor new CameraUpdate, dtor deletes, nothing else
    /* +0x80 */ int                      m_ticks;             // updateTicks stores its argument
    /* +0x84 */ float                    m_float84;           // [UNRESOLVED] ctor 0, never read in this TU
    /* +0x88 */ float                    m_float88;           // [UNRESOLVED] ctor 0, never read in this TU
    /* +0x8C */ int                      m_unused8C;          // [CHOSEN, not recovered] no instruction touches +0x8C
    /* +0x90 */ float                    m_followDistance;    // ctor 2.0; clamped to [demo-cam-offset-min, 1000]
    /* +0x94 */ float                    m_heightOffset;      // clamped to +-10
    /* +0x98 */ bool                     m_inputPrimed;       // the first handlePlayerInput only primes
    /* +0x99 */ bool                     m_tickOffsetSet;
    /* +0x9C */ int                      m_tickOffset;        // playback tick -> world tick shift
    /* +0xA0 */ float                    m_visualTime;        // demoVisualUpdate's argument

    DemoSpectatorCamera(const DemoSpectatorCamera&);            // not in the binary; not generated
    DemoSpectatorCamera& operator=(const DemoSpectatorCamera&);
};

// One sampled camera state, 0x38 bytes in the ELF; the .bf2cam track is a sequence of these.
// Field names are [INFERRED] from the camera members updateTicks copies into them.
struct CameraUpdate
{
    CameraUpdate();                                      // ELF 0x8153C80 / 0x8153CC0
    void writeToStream(io::IStream* stream) const;       // ELF 0x8153D00
    void readFromStream(io::IStream* stream);            // ELF 0x8153F20

    /* +0x00 */ DemoSpectatorCamera::Mode mode;
    /* +0x04 */ unsigned int tick;           // compared unsigned (jae) against the world tick
    /* +0x08 */ int          playerId;       // follow mode only
    /* +0x0C */ float        yaw;            // follow mode only; the ctor leaves +0x0C..+0x18 unset
    /* +0x10 */ float        pitch;
    /* +0x14 */ float        followDistance;
    /* +0x18 */ float        heightOffset;
    /* +0x1C */ Quat         rotation;       // free mode only
    /* +0x2C */ Vec3         position;       // free mode only
};

} // namespace hfe
} // namespace dice

#endif // GAME_DEMORECORDER_DEMOSPECTATORCAMERA_H
