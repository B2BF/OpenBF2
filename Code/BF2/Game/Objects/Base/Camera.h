// Vehicle camera with authored view modes and separate camera and console interfaces.
// Use compiler-adjusted interface casts. [INFERRED] Source placement.
#ifndef GAME_OBJECTS_BASE_CAMERA_H
#define GAME_OBJECTS_BASE_CAMERA_H

#include "../Game/RotationalBundle.h"
#include "../ICameraObject.h"
#include "../../../IO/Input/PlayerInput.h"
#include "../CameraTemplate.h"
#include "../../../Math/Quat.h"
#include <list>
#include <vector>
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // getPcoId walks for one

namespace dice {
namespace hfe {
namespace world {

class Camera : public RotationalBundle, public ICameraObject
{
public:

    explicit Camera(const CameraTemplate* templ);

    // IBase, on the Camera itself. The refcount is at +0x1B4 - inside the ICameraObject
    // sub-object, not beside Object's own.
    virtual void* queryInterface(unsigned int iid) const;
    virtual void  addRef();                                 // void
    virtual int   getRef();
    virtual int   release();

    // ICameraObject, the part that findCameras and Player need
    virtual void setCameraId(int id);                       // +0x28
    virtual int  getCameraId();                             // +0x2C
    virtual int  getPcoId();                                // +0x48
    virtual bool isActive() const;                          // +0x20
    virtual void setIsActive(bool active);                  // +0x24
    virtual bool getToggleMouseLook();                      // +0x44
    virtual CameraViewMode getViewMode() const;             // +0x1C
    virtual void setInterpolatedMode(bool on);              // +0x14
    virtual void setTraceObjectId(unsigned int id);         // +0x34
    virtual unsigned int getTraceObjectId();                // +0x38

    // Recovered state machine; see the .cpp for server gates and client-only tail.
    virtual bool setViewMode(CameraViewMode mode, IObject* object, bool force);   // +0x18

    virtual void init();

    // Handles player input for this camera.
    virtual void handlePlayerInput(IPlayer*, const io::PlayerInput&, float, unsigned int);

    // the view-mode state machine.
    void resetChaseCamera();

    // WeaponBasedRecoilComp's camera tail calls this concrete helper after converting the
    // ICameraObject face back to Camera.  It only arms recoil in view modes 1 and 2.
    void addRecoilImpulse(float speed, float size);

    // Switches vehicle/weapon subgeometry only for the local human occupant.
    void changeSubGeometry(unsigned int viewMode, int subGeom);

    // Tracks entry/exit and restores local-player subgeometry, then forwards to RotationalBundle.
    virtual void handleMessage(TemplateMessage message, IPlayer* player, bool flag);
    // , non-virtual; BF2.exe inlines it at both handleMessage sites.
    void setRecordingValid(bool valid);

    // ELF 0x81DBA90 (496 B) / BF2.exe 0x5A3EA0: the local occupant's recoil and shake, the frame
    // clock, the periodic close-vehicle scan and the free-camera view switch, then RotationalBundle.
    virtual void handleUpdate(float deltaTime, unsigned int flags);
    void updateCameraRecoil(float deltaTime);   // ELF 0x81DC1D0 / BF2.exe 0x5A1A10
    void updateCameraShake(float deltaTime);    // ELF 0x81DC270 / BF2.exe 0x5A1A90
    void updateCloseVehicleShake();             // ELF 0x81DC550 / BF2.exe 0x5A3830

    // Camera::Frame is exactly a recorded world position followed by its camera-clock timestamp.
    // The 16-byte payload is proved independently by the ELF list-node allocation and by both
    // readFrame bodies' node+8/+12/+16/+20 reads.
    struct Frame
    {
        Vec3 position;
        float time;
    };

    Vec3 readFrame();
    void viewFrame();
    void writeFrame(const Vec3& position, float interval);

    // Camera virtual surface.  Remaining [FRONTIER] entries below are still declaration-only;
    // the update and transformation slots used by authored view modes are concrete.
    virtual void disableCachedObject();                     // slot 9
    virtual void resetCameraView(bool);                     // +0x50
    virtual void updateCameraView(bool);                    // +0x54
    // Resolves the current view mode, caches its transform, and returns the member reference.
    virtual const Mat4& getTransformation(float);           // +0x10
    virtual float getActiveFOV() const;                     // +0x58
    virtual void prevViewMode();
    virtual void handleExplosion(float);                    // +0x4C
    virtual void setExternCameraTrans(Mat4 transform);
    virtual Mat4* getExternCameraTrans();                   // +0x40
    // Interior lighting values are returned independently of view mode.
    virtual const Vec3& getStaticSunDirection() const;      // +0x64
    virtual const Vec3& getStaticSunColor() const;          // +0x68
    virtual const Vec3& getStaticSunDamageColor() const;    // +0x6C
    virtual const Vec3& getStaticSunAmbientColor() const;   // +0x78
    virtual float getInsideDamageFrequency() const;         // +0x70
    virtual float getInsideDamageThreshold() const;         // +0x74
    // Inside mode uses template lighting gates; dynamic sun negates the static-sun flag.
    virtual bool  getAllowDynamicLights() const;            // +0x5C
    virtual bool  getAllowDynamicSun() const;               // +0x60

    // the per-mode transformation family - NON-virtual, the +0x10 dispatcher's arms
    // CVMInside copies the absolute transform, then the client applies the parent PCO's camera
    // animation, roll and terrain/water constraints (BF2.exe 0x5A2920).
    const Mat4& getTransformationCVMInside(float);
    const Mat4& getTransformationCVMNose(float);
    const Mat4& getTransformationCVMChase(float, bool);
    const Mat4& getTransformationCVMTrace(float);
    const Mat4& getTransformationCVMFlyBy(float);

    // layout, 816 bytes. Only the fields this sitting proved are named.
    /* +0x080 */ // the IConsoleSaveable sub-object's vptr (a CameraScript VMT)
    /* +0x196 */ // Object's "inited" byte, set by init()
    /* +0x1B0 */ // the ICameraObject sub-object's vptr
    /* +0x1B4 */ int  m_refCount;                    // ctor 1  NOT Object's refcount
    /* +0x1B8 */ int  m_viewMode;                    // ctor 1  (CVMInside)
    /* +0x1BC */ float m_float1BC;                   // ctor 8.0f   [UNRESOLVED]
    // The chase working set is a quaternion and a smoothed distance. resetChaseCamera clears only
    // x/y/z and the distance, deliberately retaining w (the ctor's 1.0f).
    /* +0x1C0 */ Quat  m_chaseRotation;              // ctor (0,0,0,1)
    /* +0x1D0 */ float m_chaseDistance;              // ctor 0
    /* +0x1D4 */ float m_frameTime;                  // ctor 0; Camera::handleUpdate advances it
    /* +0x1D8 */ bool m_recordingValid;               // ctor false - setRecordingValid's field
    // A std::list IS four bytes here and its sentinel node lives on the heap: the constructor
    // allocates 24 and self-links it. resetCameraView and two setViewMode modes clear it.
    /* +0x1DC */ std::list<Frame> m_traceBuffer;
    /* +0x1E0 */ bool m_isActive;                    // ctor false
    /* +0x1E4 */ int  m_pcoId;                       // ctor: from THE template (+532)
    // Trace mode smooths a persistent look-at target; trace origin stores the previous output position.
    /* +0x1E8 */ Vec3 m_traceAhead;                  // persistent/smoothed look-at target
    /* +0x1F4 */ Vec3 m_traceOrigin;                 // previous output-camera position
    /* +0x200 */ int  m_dword200;                    // ctor 0.  [UNRESOLVED]
    /* +0x204 */ int  m_dword204;                    // ctor 0.  [UNRESOLVED]
    /* +0x208 */ float m_float208;                   // ctor -1.0f. [UNRESOLVED]
    // A full Mat4 AT +0x20C..+0x24B - which is what makes +0x24C the next field. The ctor and
    // resetCameraView both set it to identity; setViewMode's CVMExternTrace mode reads its third row
    // as the forward axis and its fourth as the translation.
    /* +0x20C */ Mat4 m_externCameraTrans;
    /* +0x24C */ int  m_previousViewMode;            // ctor 1  [CHOSEN] name
    /* +0x250 */ int  m_savedViewMode;               // ctor 8 = none; handleUpdate's free-camera switch [INFERRED] name
    /* +0x254 */ unsigned int m_traceObjectId;       // ctor 0
    /* +0x258 */ int  m_dword258;                    // ctor -1     [UNRESOLVED]
    /* +0x25C */ float m_float25C;                   // ctor 1.0f   [UNRESOLVED]
    /* +0x260 */ Mat4 m_defaultTransformation;       // ctor identity - the 1.0f at +0x260 / +0x274 /
                                                     // +0x288 +0x29C is the diagonal, stride 20
    /* +0x2A0 */ int  m_cameraId;                    // ctor: from THE template (+540)
    /* +0x2A4 */ bool m_interpolatedMode;            // ctor false
    /* ELF +0x2A8 / PE +0x2C8 */ float  m_explosionShakePeak;      // ctor 0
    /* ELF +0x2AC / PE +0x2D0 */ double m_explosionShakeStartTime; // ctor leaves untouched;
                                                                      // written with the first
                                                                      // stronger accepted shake
    // [UNRESOLVED] presentation working state between the accepted-explosion timestamp and the
    // recovered recoil block. Its bounds are direct: ELF +0x2B4..+0x2F3, PE +0x2D8..+0x31F.
    // The PE has eight extra bytes here after its differently aligned/client-side prefix.
    unsigned char m_cameraPresentationPrefix[0x10];
    /* ELF +0x2C4 / PE +0x2E8 */ float m_speedShakeTime;   // updateCameraShake's clock [INFERRED] name
    /* ELF +0x2C8 / PE +0x2EC */ float m_speedShakeRoll;
    /* ELF +0x2CC / PE +0x2F0 */ int   m_unresolved2CC;    // [UNRESOLVED]
    // updateCloseVehicleShake's grid-query result (ELF +0x2D0..+0x2DB begin/end/capacity).
    /* ELF +0x2D0 */ std::vector<IObject*> m_closeVehicles;  // [INFERRED] name
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned char m_cameraPresentationSuffix[0x30 - 4 - sizeof(std::vector<IObject*>)];
#elif defined(__i386__)
    unsigned char m_cameraPresentationSuffix[0x28 - 4 - sizeof(std::vector<IObject*>)];
#else
    unsigned char m_cameraPresentationSuffix[0x18];
#endif
    // The recoil/shake block. Its shipped absolute offsets differ between ELF and PE because
    // Camera's client-side preceding layout differs. addRecoilImpulse touches the first three and
    // m_cameraRecoilX; handleExplosion touches the two intervening explosion fields.
    /* ELF +0x2F4 / PE +0x320 */ bool  m_updateCameraRecoil;
    /* ELF +0x2F8 / PE +0x324 */ float m_recoilImpulseSpeed;
    /* ELF +0x2FC / PE +0x328 */ float m_recoilImpulseSize;
    /* ELF +0x300 / PE +0x32C */ float m_explosionShakeTimeRemaining;
    /* ELF +0x304 / PE +0x330 */ float m_explosionShakeMagnitude;
    /* ELF +0x308 / PE +0x334 */ float m_closeVehicleShakePhaseX;  // updateCameraShake [INFERRED] name
    /* ELF +0x30C / PE +0x338 */ float m_closeVehicleShakePhaseY;  // [INFERRED] name
    /* ELF +0x310 / PE +0x33C */ float m_cameraRecoilX;
    /* ELF +0x314 / PE +0x340 */ float m_closeVehicleCheckTime;  // handleUpdate's scan clock [INFERRED] name
    /* ELF +0x318 / PE +0x344 */ float m_soldierShakeX;
    /* ELF +0x31C / PE +0x348 */ float m_soldierShakeY;
    /* ELF +0x320 / PE +0x34C */ float m_cameraRecoilY;
    /* ELF +0x324 / PE +0x350 */ float m_soldierShakeRoll;
    /* ELF +0x328 / PE +0x354 */ float m_soldierShakePitch;
    /* ELF +0x32C / PE +0x358 */ float m_explosionShakeRoll;
    // The still-unnamed tail is predominantly the independent recoil/shake working state.  The
    // trace list, extern transform, and per-mode transformation working set above are now named.
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_BASE_CAMERA_H
