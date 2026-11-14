// Path [INFERRED]. Client PE recovery; symbolized ELF supplies names/signatures.
// [PLATFORM] ELF has no Novint imports and compiled-out init/update/device calls.
// BF2_NOVINT_FAKE_SDK builds the Windows body against the Mac check's fake SDK.
#include "FalconDevice.h"
#if !DEDICATED && !defined(_WIN32) && !defined(BF2_NOVINT_FAKE_SDK)
namespace dice { namespace hfe { namespace io {
// [CHOSEN, not recovered] Native POSIX clients have no Falcon (see FalconDevice.h); the menu's
// mouse-mode global and the control-box flag keep their shipped initial values.
int g_falconMouseMode = 1; // PE978FC4
bool g_falconControlBoxDirty = true; // PE978FC8
// [CHOSEN, not recovered] No Falcon effect is ever created here, so every target stays null and
// MainMenuLogic's zeroing finds nothing to call.
void zeroHapticForceTargets() {}
void updateShakeForce(world::IPlayerControlObject*, float, float, float, float) {}
bool hapticDeviceConnected() { return false; }
void updateRecoilForce(float, float, int) {}
void updateSoldierCraftPhysicsForce(const Vec3&, float) {}
void updateVehicleCraftPhysicsForce(const Vec3&, float, int) {}
void soldierRecoilPunch(int, bool, unsigned int, const float*, float, float, float, float) {}
void fireRecoilPunch(float, float, float) {}
void weaponRecoilPunch(int, const std::string&, float, float, float) {}
void kitRecoilPunch(int, const std::string&, float) {}
void updateDamageForce(world::IPlayer*, world::IPlayer*, const Mat4&, const Mat4&, float) {}
}
IHapticEffect* g_hapticForceTargets[6] = { 0, 0, 0, 0, 0, 0 }; // ds:0x9EBEC8.. (BF2ClientSeams.h)
} }
#elif !DEDICATED
#include "../../Core/Ids.h"
#include "../../Game/ControlSettings.h"
#include "../../Game/HapticSettings.h"
#include "../../Platform/X87Compat.h"
#include "../../Math/Vec3.h"
#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>
// [CHOSEN, not recovered] x64 (https://github.com/B2BF/OpenBF2/issues/7): NovintHFX ships only as a
// 32-bit DLL. The x64 image never calls it: init() reports the device absent through the same
// `false` retail returns with no device connected, so m_device stays null and every device path
// below returns at its existing null test. No SDK symbol is referenced there: the frame update and the
// force zeroing compile to their no-device bodies.
#if defined(_M_IX86) || defined(BF2_NOVINT_FAKE_SDK)
#define BF2_FALCON_SDK 1
#else
#define BF2_FALCON_SDK 0
#endif
namespace dice { namespace hfe {
IHapticEffect* g_hapticForceTargets[6] = { 0, 0, 0, 0, 0, 0 }; // ds:0x9EBEC8.. (BF2ClientSeams.h)
extern IBase* g_controlSettings; // ds:0x98738C, Game/Main/BF2ClientSeams.h
extern IBase* g_hapticSettings;  // ds:0x987394, Game/Main/BF2ClientSeams.h
namespace io {
extern IBase* inputDeviceManager; // PE 0x9FFCB0, singleton registration 0x698542
extern IBase* controlMapManager;  // PE 0x9FFCB4, Core/Globals.cpp
extern float inputSampleLatency;  // PE 0xA07E4C, IO/Input/InputManager.cpp
NovintHFX::Device* g_falconDevice = 0;
int g_falconMouseMode = 1; // PE978FC4
bool g_falconControlBoxDirty = true; // PE978FC8
#if BF2_FALCON_SDK
namespace {
// Initialized data read only by updateCurrentState. [INFERRED] names.
double s_falconMouseGain = 1000.0;      // ds:0x979018: mouse-mode position gain
short s_controlBoxForceValue = 3000;    // ds:0x979010: control box +0x114 [UNRESOLVED] meaning
int s_smoothingWindow = 3;              // ds:0x979020: samples between the delta's two ends
int s_smoothingCount = 0;               // ds:0xA03948: samples queued in each delta history
IHapticEffect* s_controlBoxEffect = 0;  // ds:0xA03944: created once, then updated in place

// BF2.exe 0x6A589B..0x6A58A1 calls the imported ControlBoxEffectParams constructor and then stores
// its own vtable (0x9116E0), whose deleting destructor (0x5E2EA0) calls ~BaseEffectParams directly:
// the object is a BF2-side class derived from the SDK type with implicit members only.
// [INFERRED] name.
class FalconControlBoxParams : public NovintHFX::ControlBoxEffectParams {};

// Retail re-stores its own vtable after the SDK constructor: BF2.exe 0x5A4174 stores 0x8E7698,
// and 0x5E65BA stores 0x8ED440. As above, these BF2-side derived classes have implicit members only.
// [INFERRED] names.
class FalconConstantParams : public NovintHFX::ConstantEffectParams {};
class FalconEnvelopeParams : public NovintHFX::EnvelopeEffectParams {};

// Retail indexes the returned Vec3 as a float array ([eax + 4*esi]).
float component(const Vec3& v, int axis)
{
    return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
}
// The three position histories are function-local statics in retail (guards ds:0xA03988, bits
// 1/2/4; atexit 0x87A040/0x87A030/0x87A020). Dinkumware's pop_front ignores an empty deque.
void popFrontIfAny(std::deque<double>& history)
{
    if (!history.empty()) history.pop_front();
}
// One smoothing step at a full window: push, drop the oldest, and return the slope across it.
double windowDelta(std::deque<double>& history, double value)
{
    history.push_back(value);
    popFrontIfAny(history);
    return (history.back() - history.front()) / static_cast<double>(s_smoothingWindow * 2);
}
// Outside the dead zone the offset past its edge drives the axis at `scale`; inside it, the
// smoothed delta drives it at `deltaScale`. Each settings read is a fresh call, as in retail.
double axisValue(IHapticSettings* haptics, int axis, double position, double delta,
                 double scale, double deltaScale)
{
    if (component(haptics->getAimSensitivityLength(), axis) < position)
        return (position - component(haptics->getAimSensitivityLength(), axis))
            * component(haptics->getTurnSensitivity(), axis) * scale;
    if (-component(haptics->getAimSensitivityLength(), axis) > position)
        return (component(haptics->getAimSensitivityLength(), axis) + position)
            * component(haptics->getTurnSensitivity(), axis) * scale;
    return delta * deltaScale;
}
} // namespace
#endif // BF2_FALCON_SDK: the frame update's state and helpers

void zeroHapticForceTargets()
{
#if !BF2_FALCON_SDK
    // x64: no Falcon effect is ever created, so every target is null.
#else
    const double x = 0.0, y = 0.0, z = 0.0;
    NovintHFX::Vect3 zero(x, y, z);
    static const int retailOrder[6] = { 2, 3, 4, 5, 0, 1 }; // 0x9EBF00..0x9EBF0C, then 0x9EBEC8/CC
    for (int i = 0; i < 6; ++i) {
        IHapticEffect* target = g_hapticForceTargets[retailOrder[i]];
        if (target) target->SetForce(zero);
    }
#endif
}

#if BF2_FALCON_SDK
namespace {
// BF2.exe 0x5A2790: the sign of a double, 0 for 0 and NaN.
double hapticSign(double value)
{
    if (value > 0.0) return 1.0;
    if (value < 0.0) return -1.0;
    return 0.0;
}
// BF2.exe 0x5A6280: the same for a float.
float hapticSignF(float value)
{
    if (value > 0.0f) return 1.0f;
    if (value < 0.0f) return -1.0f;
    return 0.0f;
}
} // namespace
#endif

void updateShakeForce(world::IPlayerControlObject* pco, float explosionRoll, float speedRoll,
                      float closeVehiclePitch, float recoilY)
{
#if !BF2_FALCON_SDK
    (void) pco; (void) explosionRoll; (void) speedRoll; (void) closeVehiclePitch; (void) recoilY;
#else
    if (!g_falconDevice)
        return;
    IHapticEffect*& vehicleShake = g_hapticForceTargets[0];   // ds:0x9EBEC8
    IHapticEffect*& soldierShake = g_hapticForceTargets[1];   // ds:0x9EBECC
    if (vehicleShake && soldierShake) {
        IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
        float x = haptics->getExplosionShakeScale() * explosionRoll;
        x = haptics->getSpeedShakeScale() * speedRoll + x;
        x = haptics->getGeneralShakeScale() * closeVehiclePitch + x;
        x = haptics->getUnresolved48C() * recoilY + x;
        float shared = haptics->getExplosionShakeScale() * explosionRoll;
        shared = haptics->getSpeedShakeScale() * speedRoll + shared;
        shared = haptics->getGeneralShakeScale() * closeVehiclePitch + shared;
        // Retail keeps this product on the x87 stack and stores the sum straight to a double; the
        // product of two floats is exact in double.
        const double y = static_cast<double>(haptics->getUnresolved490()) * recoilY + shared;
        const double xd = x, z = 0.0;
        NovintHFX::Vect3 force(xd, y, z);
        const float absX = static_cast<float>(std::fabs(force[0]));
        if (haptics->getForceCapX() < absX)
            force[0] = hapticSign(force[0]) * haptics->getForceCapX();
        const float absY = static_cast<float>(std::fabs(force[1]));
        if (haptics->getForceCapY() < absY)
            force[1] = hapticSign(force[1]) * haptics->getForceCapY();
        world::IObject* const occupied = static_cast<world::IObject*>(pco->queryInterface(world::IID_IObject));
        const bool soldier = occupied->getTemplate()->getClassID() == world::CID_SoldierTemplate;
        (soldier ? soldierShake : vehicleShake)->SetForce(force);
        return;
    }
    FalconConstantParams* params = new FalconConstantParams;
    params->m_unresolved18 = 0x14;
    g_falconDevice->GetEffectStack()->CreateNewEffect("CONSTANT", params, "ShakeForce", &vehicleShake);
    g_falconDevice->GetEffectStack()->CreateNewEffect("CONSTANT", params, "ShakeForce", &soldierShake);
    delete params;
#endif
}

bool hapticDeviceConnected()
{
#if !BF2_FALCON_SDK
    return false;
#else
    return g_falconDevice != 0;
#endif
}

void updateRecoilForce(float yawDelta, float pitchRecoil, int weaponType)
{
#if !BF2_FALCON_SDK
    (void) yawDelta; (void) pitchRecoil; (void) weaponType;
#else
    IHapticEffect*& recoil = g_hapticForceTargets[2];   // ds:0x9EBF00
    if (!recoil) {
        FalconConstantParams* params = new FalconConstantParams;
        params->m_unresolved18 = 10;
        g_falconDevice->GetEffectStack()->CreateNewEffect("Constant", params, "Recoil", &recoil);
        delete params;
    }
    const double x0 = 0.0, y0 = 0.0, z0 = 0.0;
    NovintHFX::Vect3 force(x0, y0, z0);
    if (yawDelta != 0.0f || pitchRecoil != 0.0f) {
        IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
        float yawScale, pitchScale;
        switch (weaponType) {   // the jump table at 0x5AF5DC
        case 0:  yawScale = haptics->getRecoilYawScaleRIFLE();         pitchScale = haptics->getRecoilPitchScaleRIFLE(); break;
        case 1:  yawScale = haptics->getRecoilYawScaleRIFLELAUNCHER(); pitchScale = haptics->getRecoilPitchScaleRIFLELAUNCHER(); break;
        case 2:  yawScale = haptics->getRecoilYawScaleCARBINE();       pitchScale = haptics->getRecoilPitchScaleCARBINE(); break;
        case 3:  yawScale = haptics->getRecoilYawScaleLMG();           pitchScale = haptics->getRecoilPitchScaleLMG(); break;
        case 4:  yawScale = haptics->getRecoilYawScaleSNIPER();        pitchScale = haptics->getRecoilPitchScaleSNIPER(); break;
        case 5:  yawScale = haptics->getRecoilYawScalePISTOL();        pitchScale = haptics->getRecoilPitchScalePISTOL(); break;
        case 6:  yawScale = haptics->getRecoilYawScaleLAUNCHER();      pitchScale = haptics->getRecoilPitchScaleLAUNCHER(); break;
        case 7:  yawScale = haptics->getRecoilYawScaleSMG();           pitchScale = haptics->getRecoilPitchScaleSMG(); break;
        case 8:  yawScale = haptics->getRecoilYawScaleSHOTGUN();       pitchScale = haptics->getRecoilPitchScaleSHOTGUN(); break;
        case 9:  yawScale = haptics->getRecoilYawScaleKNIFE();         pitchScale = haptics->getRecoilPitchScaleKNIFE(); break;
        case 10: yawScale = haptics->getRecoilYawScaleDROP();          pitchScale = haptics->getRecoilPitchScaleDROP(); break;
        case 11: yawScale = haptics->getRecoilYawScaleTHROWN();        pitchScale = haptics->getRecoilPitchScaleTHROWN(); break;
        case 12: yawScale = haptics->getRecoilYawScaleDEFIB();         pitchScale = haptics->getRecoilPitchScaleDEFIB(); break;
        default: yawScale = 0.0f; pitchScale = 0.0f; break;
        }
        float x = yawDelta;
        if (haptics->getForceCapX() < static_cast<float>(std::fabs(yawDelta)))
            x = hapticSignF(yawDelta) * haptics->getForceCapX();
        float y = pitchRecoil;
        if (haptics->getForceCapY() < static_cast<float>(std::fabs(pitchRecoil)))
            y = hapticSignF(pitchRecoil) * haptics->getForceCapY();
        // Retail multiplies on the x87 stack and stores doubles; float products are exact in double.
        const double fx = static_cast<double>(yawScale) * x;
        const double fy = -(static_cast<double>(pitchScale) * y);
        force = NovintHFX::Vect3(fx, fy, z0);
    }
    recoil->SetForce(force);
#endif
}

#if BF2_FALCON_SDK
namespace {
// One CraftPhysics effect: created once with params +0x18 = its smooth time.
void createCraftPhysicsEffect(IHapticEffect*& effect, const char* name, int smoothTime)
{
    if (effect)
        return;
    FalconConstantParams* params = new FalconConstantParams;
    params->m_unresolved18 = smoothTime;
    g_falconDevice->GetEffectStack()->CreateNewEffect("Constant", params, name, &effect);
    delete params;
}
// An acceleration component capped by the force cap with its sign (0x5AF229..0x5AF2D4).
float capCraftPhysics(float value, float cap, IHapticSettings* haptics, float (IHapticSettings::*getCap)())
{
    if (cap < static_cast<float>(std::fabs(value)))
        return hapticSignF(value) * (haptics->*getCap)();
    return value;
}
} // namespace
#endif

#if BF2_FALCON_SDK
namespace {
// One HapticSettings physics block (SOLDIER, LAND, SEA, AIR, HELI share the layout, 0xC8 apart).
struct CraftPhysicsProfile {
    Vec3 (IHapticSettings::*scale)();
    float (IHapticSettings::*x[3])();      // Low, Med, High
    float (IHapticSettings::*y[3])();
    float (IHapticSettings::*z[3])();
    float (IHapticSettings::*transitionMed)();
    float (IHapticSettings::*transitionHigh)();
    int (IHapticSettings::*smoothX[3])();
    int (IHapticSettings::*smoothY[3])();
    int (IHapticSettings::*smoothZ[3])();
};
#define BF2_CRAFT_PROFILE(C) { &IHapticSettings::getPhysics##C##Scale, \
    { &IHapticSettings::getPhysics##C##ScaleXLow, &IHapticSettings::getPhysics##C##ScaleXMed, &IHapticSettings::getPhysics##C##ScaleXHigh }, \
    { &IHapticSettings::getPhysics##C##ScaleYLow, &IHapticSettings::getPhysics##C##ScaleYMed, &IHapticSettings::getPhysics##C##ScaleYHigh }, \
    { &IHapticSettings::getPhysics##C##ScaleZLow, &IHapticSettings::getPhysics##C##ScaleZMed, &IHapticSettings::getPhysics##C##ScaleZHigh }, \
    &IHapticSettings::getPhysics##C##TransitionValueMed, &IHapticSettings::getPhysics##C##TransitionValueHigh, \
    { &IHapticSettings::getPhysics##C##SmoothTimeXLow, &IHapticSettings::getPhysics##C##SmoothTimeXMed, &IHapticSettings::getPhysics##C##SmoothTimeXHigh }, \
    { &IHapticSettings::getPhysics##C##SmoothTimeYLow, &IHapticSettings::getPhysics##C##SmoothTimeYMed, &IHapticSettings::getPhysics##C##SmoothTimeYHigh }, \
    { &IHapticSettings::getPhysics##C##SmoothTimeZLow, &IHapticSettings::getPhysics##C##SmoothTimeZMed, &IHapticSettings::getPhysics##C##SmoothTimeZHigh } }
const CraftPhysicsProfile kSoldierProfile = BF2_CRAFT_PROFILE(SOLDIER);
const CraftPhysicsProfile kLandProfile = BF2_CRAFT_PROFILE(LAND);
const CraftPhysicsProfile kSeaProfile = BF2_CRAFT_PROFILE(SEA);
const CraftPhysicsProfile kAirProfile = BF2_CRAFT_PROFILE(AIR);
const CraftPhysicsProfile kHeliProfile = BF2_CRAFT_PROFILE(HELI);
#undef BF2_CRAFT_PROFILE

// The CraftPhysics core shared by Soldier::handlePlayerInput (0x5AED77..0x5AF3F6) and
// PlayerControlObject::handleUpdate (0x5FADAE..0x5FBCD8). The soldier's block takes the Low smooth
// times in every band; the vehicle's takes the band's own.
void craftPhysicsForce(std::vector<Vec3>& velocities, std::vector<float>& tickTimes, const Vec3& localVelocity,
                       float dt, const CraftPhysicsProfile& profile, bool smoothTimesByBand)
{
    Vec3 acceleration;
    acceleration.x = acceleration.y = acceleration.z = 0.0f;
    if (velocities.size() < 3) {
        velocities.push_back(localVelocity);
        tickTimes.push_back(dt);
    } else {
        velocities.erase(velocities.begin());
        velocities.push_back(localVelocity);
        tickTimes.erase(tickTimes.begin());
        tickTimes.push_back(dt);
        const float span = tickTimes[1] + tickTimes[2];
        const Vec3& oldest = velocities.front();
        const Vec3& newest = velocities.back();
        const float dx = oldest.x - newest.x, dy = oldest.y - newest.y, dz = oldest.z - newest.z;
        // 0x5A6370 keeps 1 / span on the x87 stack for z and stores it rounded for x and y.
        const double inverse = 1.0 / static_cast<double>(span);
        const float inverseRounded = static_cast<float>(inverse);
        acceleration.x = inverseRounded * dx;
        acceleration.y = inverseRounded * dy;
        acceleration.z = static_cast<float>(inverse * dz);
    }
    const double ax = acceleration.x, ay = acceleration.y, az = acceleration.z;
    const float length = static_cast<float>(std::sqrt(ax * ax + ay * ay + az * az));

    IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
    int band;
    if ((haptics->*profile.transitionMed)() > length)
        band = 0;
    else if ((haptics->*profile.transitionHigh)() > length)
        band = 1;
    else
        band = 2;
    const float sx = (haptics->*profile.x[band])() * (haptics->*profile.scale)().x;
    const float sy = (haptics->*profile.y[band])() * (haptics->*profile.scale)().y;
    const float sz = (haptics->*profile.z[band])() * (haptics->*profile.scale)().z;
    const int smoothBand = smoothTimesByBand ? band : 0;
    const int smoothX = (haptics->*profile.smoothX[smoothBand])();
    const int smoothY = (haptics->*profile.smoothY[smoothBand])();
    const int smoothZ = (haptics->*profile.smoothZ[smoothBand])();

    IHapticEffect*& effectX = g_hapticForceTargets[3];   // ds:0x9EBF04
    IHapticEffect*& effectY = g_hapticForceTargets[4];   // ds:0x9EBF08
    IHapticEffect*& effectZ = g_hapticForceTargets[5];   // ds:0x9EBF0C
    createCraftPhysicsEffect(effectX, "CraftPhysicsX", smoothX);
    createCraftPhysicsEffect(effectY, "CraftPhysicsY", smoothY);
    createCraftPhysicsEffect(effectZ, "CraftPhysicsZ", smoothZ);

    const double zero = 0.0;
    NovintHFX::Vect3 force(zero, zero, zero);
    const float cx = capCraftPhysics(acceleration.x, haptics->getForceCapX(), haptics, &IHapticSettings::getForceCapX);
    const float cy = capCraftPhysics(acceleration.y, haptics->getForceCapY(), haptics, &IHapticSettings::getForceCapY);
    const float cz = capCraftPhysics(acceleration.z, haptics->getForceCapZ(), haptics, &IHapticSettings::getForceCapZ);
    // Retail multiplies on the x87 stack and stores doubles; float products are exact in double.
    const double fx = -(static_cast<double>(sx) * cx);
    force = NovintHFX::Vect3(fx, zero, zero);
    effectX->SetSmoothTime(smoothX);
    effectX->SetForce(force);
    const double fy = -(static_cast<double>(sy) * cy);
    force = NovintHFX::Vect3(zero, fy, zero);
    effectY->SetSmoothTime(smoothY);
    effectY->SetForce(force);
    const double fz = static_cast<double>(sz) * cz;
    force = NovintHFX::Vect3(zero, zero, fz);
    effectZ->SetSmoothTime(smoothZ);
    effectZ->SetForce(force);
}
} // namespace
#endif

void updateSoldierCraftPhysicsForce(const Vec3& localVelocity, float dt)
{
#if !BF2_FALCON_SDK
    (void) localVelocity; (void) dt;
#else
    static std::vector<Vec3> velocities;   // ds:0x9EC550 (guard bit 2 of ds:0x9EC560)
    static std::vector<float> tickTimes;   // ds:0x9EC540 (guard bit 1)
    craftPhysicsForce(velocities, tickTimes, localVelocity, dt, kSoldierProfile, false);
#endif
}

void updateVehicleCraftPhysicsForce(const Vec3& localVelocity, float dt, int vehicleCategory)
{
#if !BF2_FALCON_SDK
    (void) localVelocity; (void) dt; (void) vehicleCategory;
#else
    static std::vector<Vec3> velocities;   // ds:0x9F06BC (guard bit 1 of ds:0x9F06CC)
    static std::vector<float> tickTimes;   // ds:0x9F06AC (guard bit 2)
    const CraftPhysicsProfile* profile;
    switch (vehicleCategory) {   // the jump table at 0x5FBE40
    case 1:  profile = &kSeaProfile; break;
    case 2:  profile = &kAirProfile; break;
    case 3:  profile = &kHeliProfile; break;
    default: profile = &kLandProfile; break;   // 0 and every other value (0x5FB75C)
    }
    craftPhysicsForce(velocities, tickTimes, localVelocity, dt, *profile, true);
#endif
}

void fireRecoilPunch(float punchTime, float punchScale, float fireStrength)
{
#if !BF2_FALCON_SDK
    (void) punchTime; (void) punchScale; (void) fireStrength;
#else
    FalconEnvelopeParams* params = new FalconEnvelopeParams;
    params->m_unresolved18 = static_cast<int>(punchTime);   // fistp with truncation
    IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
    const float strength = static_cast<float>(std::fabs(fireStrength));
    if (haptics->getForceCapZ() < strength)
        params->m_force[2] = static_cast<double>(haptics->getForceCapZ()) * punchScale;
    else
        params->m_force[2] = static_cast<double>(punchScale) * fireStrength;
    g_falconDevice->GetEffectStack()->CreateNewEffect("Envelope", params, "RecoilPunch", 0);
    delete params;
#endif
}

void weaponRecoilPunch(int weaponType, const std::string& templateName, float cameraRecoilSpeed,
                       float cameraRecoilSize, float fireStrength)
{
#if !BF2_FALCON_SDK
    (void) weaponType; (void) templateName; (void) cameraRecoilSpeed; (void) cameraRecoilSize; (void) fireStrength;
#else
    IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
    float punchTime, punchScale;
    switch (weaponType - 13) {   // the jump table at 0x60A5C0
#define BF2_V_RECOIL(n, type) \
    case n: \
        punchTime = haptics->getRecoilPunchTime##type(); \
        punchScale = haptics->getRecoilPunchScale##type(); \
        haptics->setUnresolved48C(haptics->getRecoilYawScale##type()); \
        haptics->setUnresolved490(haptics->getRecoilPitchScale##type()); \
        break;
    BF2_V_RECOIL(0, V_LMG)
    BF2_V_RECOIL(1, V_HMG)
    BF2_V_RECOIL(2, V_HELIGUN)
    BF2_V_RECOIL(3, V_AIRGUN)
    BF2_V_RECOIL(4, V_MISSILE)
    BF2_V_RECOIL(5, V_ARMORCANNON)
    BF2_V_RECOIL(6, V_COAXIALGUN)
#undef BF2_V_RECOIL
    default: {
        std::string name(templateName);
        for (std::size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(std::tolower(name[i]));
        if (name.find("firingport", 0, 10) != std::string::npos) {
            punchTime = haptics->getRecoilPunchTimeRIFLE();
            punchScale = haptics->getRecoilPunchScaleRIFLE();
            haptics->setUnresolved48C(haptics->getRecoilYawScaleRIFLE());
            haptics->setUnresolved490(haptics->getRecoilPitchScaleRIFLE());
        } else {
            punchTime = 1.0f;
            punchScale = 0.0f;
            haptics->setUnresolved48C(0.0f);
            haptics->setUnresolved490(0.0f);
        }
        break;
    }
    }
    punchScale = punchScale * cameraRecoilSize;
    punchTime = punchTime * cameraRecoilSpeed;
    fireRecoilPunch(punchTime, punchScale, fireStrength);
#endif
}

void kitRecoilPunch(int weaponType, const std::string& templateName, float fireStrength)
{
#if !BF2_FALCON_SDK
    (void) weaponType; (void) templateName; (void) fireStrength;
#else
    IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
    float punchTime, punchScale;
    switch (weaponType - 9) {   // the jump table at 0x5E2E90
    case 0: punchTime = haptics->getRecoilPunchTimeKNIFE();  punchScale = haptics->getRecoilPunchScaleKNIFE(); break;
    case 1: punchTime = haptics->getRecoilPunchTimeDROP();   punchScale = haptics->getRecoilPunchScaleDROP(); break;
    case 2: punchTime = haptics->getRecoilPunchTimeTHROWN(); punchScale = haptics->getRecoilPunchScaleTHROWN(); break;
    case 3: punchTime = haptics->getRecoilPunchTimeDEFIB();  punchScale = haptics->getRecoilPunchScaleDEFIB(); break;
    default: {
        std::string name(templateName);
        for (std::size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(std::tolower(name[i]));
        if (name.find("kit", 0, 3) != std::string::npos || name.find("wrench", 0, 6) != std::string::npos) {
            punchTime = haptics->getRecoilPunchTimeDROP();
            punchScale = haptics->getRecoilPunchScaleDROP();
        } else {
            punchTime = 1.0f;
            punchScale = 0.0f;
        }
        break;
    }
    }
    fireRecoilPunch(punchTime, punchScale, fireStrength);
#endif
}

void soldierRecoilPunch(int weaponType, bool hasRandomRecoilForce, unsigned int recoilSamples, const float* lastSample,
                        float forceUpValue, float forceUpVariation, float forceModifier, float fireStrength)
{
#if !BF2_FALCON_SDK
    (void) weaponType; (void) hasRandomRecoilForce; (void) recoilSamples; (void) lastSample;
    (void) forceUpValue; (void) forceUpVariation; (void) forceModifier; (void) fireStrength;
#else
    IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
    float punchTime, punchScale;
    switch (weaponType) {   // the jump table at 0x60969C
    case 0:  punchTime = haptics->getRecoilPunchTimeRIFLE();         punchScale = haptics->getRecoilPunchScaleRIFLE(); break;
    case 1:  punchTime = haptics->getRecoilPunchTimeRIFLELAUNCHER(); punchScale = haptics->getRecoilPunchScaleRIFLELAUNCHER(); break;
    case 2:  punchTime = haptics->getRecoilPunchTimeCARBINE();       punchScale = haptics->getRecoilPunchScaleCARBINE(); break;
    case 3:  punchTime = haptics->getRecoilPunchTimeLMG();           punchScale = haptics->getRecoilPunchScaleLMG(); break;
    case 4:  punchTime = haptics->getRecoilPunchTimeSNIPER();        punchScale = haptics->getRecoilPunchScaleSNIPER(); break;
    case 6:  punchTime = haptics->getRecoilPunchTimeLAUNCHER();      punchScale = haptics->getRecoilPunchScaleLAUNCHER(); break;
    case 7:  punchTime = haptics->getRecoilPunchTimeSMG();           punchScale = haptics->getRecoilPunchScaleSMG(); break;
    case 8:  punchTime = haptics->getRecoilPunchTimeSHOTGUN();       punchScale = haptics->getRecoilPunchScaleSHOTGUN(); break;
    case 9:  punchTime = haptics->getRecoilPunchTimeKNIFE();         punchScale = haptics->getRecoilPunchScaleKNIFE(); break;
    case 10: punchTime = haptics->getRecoilPunchTimeDROP();          punchScale = haptics->getRecoilPunchScaleDROP(); break;
    case 11: punchTime = haptics->getRecoilPunchTimeTHROWN();        punchScale = haptics->getRecoilPunchScaleTHROWN(); break;
    case 12: punchTime = haptics->getRecoilPunchTimeDEFIB();         punchScale = haptics->getRecoilPunchScaleDEFIB(); break;
    default: punchTime = haptics->getRecoilPunchTimePISTOL();        punchScale = haptics->getRecoilPunchScalePISTOL(); break;
    }
    if (hasRandomRecoilForce) {
        punchTime = static_cast<float>(recoilSamples) * punchTime;
        // x87: ((variation + value) * 0.5) * modifier * last, negated, times the scale; one rounding.
        punchScale = static_cast<float>(-(((static_cast<double>(forceUpVariation) + forceUpValue) * 0.5)
                                          * forceModifier * *lastSample) * punchScale);
    }
    fireRecoilPunch(punchTime, punchScale, fireStrength);
#endif
}

void updateDamageForce(world::IPlayer* victim, world::IPlayer* localPlayer, const Mat4& lookAt,
                       const Mat4& ownerTransform, float damage)
{
#if !BF2_FALCON_SDK
    (void) victim; (void) localPlayer; (void) lookAt; (void) ownerTransform; (void) damage;
#else
    // The forward row in the owner's frame, accumulated on the x87 stack in retail's order.
    const double fx = lookAt.m[8], fy = lookAt.m[9], fz = lookAt.m[10];
    const Mat4& o = ownerTransform;
    const float c = static_cast<float>(fy * o.m[9] + fz * o.m[10] + fx * o.m[8]);
    const float a = static_cast<float>(fy * o.m[1] + fz * o.m[2] + fx * o.m[0]);
    const float b = static_cast<float>(fy * o.m[5] + fz * o.m[6] + fx * o.m[4]);
    if (victim != localPlayer || !g_falconDevice)
        return;
    FalconEnvelopeParams* params = new FalconEnvelopeParams;
    params->m_unresolved18 = 100;
    IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
    const double magnitude = static_cast<double>(haptics->getDamageScale()) * damage * 0.5;
    const double x = -static_cast<double>(a), y = b, z = c;
    NovintHFX::Vect3 force = NovintHFX::Vect3(x, y, z).Normalize() * magnitude;
    params->m_force[0] = force[0];
    params->m_force[1] = force[1];
    params->m_force[2] = force[2];
    g_falconDevice->GetEffectStack()->CreateNewEffect("Envelope", params, "Damage", 0);
    delete params;
#endif
}

// PE 0x6A52A0. Consumes the samples update(0) queued that are older than the advanced sample
// clock: smoothed position deltas, the mouse or control-box mode, then the button state.
void FalconDevice::updateCurrentState(float dt)
{
#if !BF2_FALCON_SDK
    // x64: unreachable (update() returns while m_device is null, and it always is).
    (void) dt;
#else
    IControlMap* map = static_cast<IControlMapManager*>(controlMapManager)->getActive();
    m_axes[2] = 0.0;
    m_axes[1] = 0.0;
    m_axes[0] = 0.0;
    m_previousButtons = m_buttons;
    m_sampleTime += bf2x87::toIntTruncate(static_cast<double>(dt) * 1000.0);
    IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
    while (!m_samples.empty()) {
        FalconInputFormat& sample = m_samples.front();
        if (sample.time >= m_sampleTime) break;
        inputSampleLatency = static_cast<float>(
            static_cast<double>(static_cast<unsigned int>(m_pollTime - sample.time))
            * static_cast<double>(0.001f));
        const double x = sample.position[0];
        const double y = map && map->getInvertMouse() ? -sample.position[1] : sample.position[1];
        const double z = map && map->getInvertMouse() ? -sample.position[2] : sample.position[2];
        static std::deque<double> historyX;
        static std::deque<double> historyY;
        static std::deque<double> historyZ;
        double deltaX = 0.0, deltaY = 0.0, deltaZ = 0.0;
        if (s_smoothingCount < s_smoothingWindow) {
            historyX.push_back(x);
            historyY.push_back(y);
            historyZ.push_back(z);
            ++s_smoothingCount;
        } else if (s_smoothingCount == s_smoothingWindow) {
            deltaX = windowDelta(historyX, x * component(haptics->getAimSensitivity(), 0));
            deltaY = windowDelta(historyY, y * component(haptics->getAimSensitivity(), 1));
            deltaZ = windowDelta(historyZ, z * component(haptics->getAimSensitivity(), 2));
        } else {
            popFrontIfAny(historyX);
            popFrontIfAny(historyY);
            popFrontIfAny(historyZ);
            --s_smoothingCount;
        }
        // While a binding is being captured the frame ends here: the sample stays queued and
        // the axis scaling below is skipped (0x6A57CE -> 0x6A5C7B).
        if (static_cast<IControlSettings*>(g_controlSettings)->isMappingInput()) return;
        if (g_falconMouseMode > 0 && isActive()) {
            m_axes[0] = s_falconMouseGain * x;
            m_axes[1] = -(s_falconMouseGain * y);
            if (g_falconMouseMode == 1) {
                m_haptics->StartMouse(m_device);
                g_falconMouseMode = 2;
                if (s_controlBoxEffect && !(s_controlBoxEffect->m_stateFlags & 1))
                    s_controlBoxEffect->m_stateFlags |= 1;
            }
        }
        if (g_falconMouseMode < 1 && isActive()) {
            bool rebuild = false;
            if (g_falconMouseMode == 0) {
                g_falconMouseMode = -1;
                rebuild = true;
            } else if (g_falconControlBoxDirty) {
                rebuild = true;
            }
            if (rebuild) {
                g_falconControlBoxDirty = false;
                FalconControlBoxParams* params = new FalconControlBoxParams;
                for (int i = 0; i < 3; ++i) {
                    // The vertical walls sit 0.01 lower than the configured length.
                    const float lowerOffset = i == 2 ? 0.01f : 0.0f;
                    params->m_lower[i] = -static_cast<double>(component(haptics->getControlBoxLength(), i))
                        - static_cast<double>(lowerOffset);
                    params->m_lowerStiffness[i] = component(haptics->getControlBoxStiffness(), i);
                    const float upperOffset = i == 2 ? 0.01f : 0.0f;
                    params->m_upper[i] = static_cast<double>(component(haptics->getControlBoxLength(), i))
                        - static_cast<double>(upperOffset);
                    params->m_upperStiffness[i] = component(haptics->getControlBoxStiffness(), i);
                }
                params->m_unresolved114 = s_controlBoxForceValue;
                if (!s_controlBoxEffect)
                    g_falconDevice->GetEffectStack()->CreateNewEffect("BOX", params, "ControlBox",
                                                                      &s_controlBoxEffect);
                s_controlBoxEffect->UpdateParams(params);
                if (s_controlBoxEffect->m_stateFlags & 1) s_controlBoxEffect->m_stateFlags &= ~1;
                delete params;
            }
            m_axes[0] = axisValue(haptics, 0, x, deltaX, 2000.0, 2500.0);
            m_axes[1] = axisValue(haptics, 1, y, deltaY, -2000.0, -2500.0);
            m_axes[2] = axisValue(haptics, 2, z, deltaZ, -2000.0, -2500.0);
        }
        int pressed, released;
        m_device->GetButtonData(reinterpret_cast<int*>(&sample.buttons), &pressed, &released);
        m_buttons = sample.buttons;
        m_samples.pop_front();
    }
    // Retail scales four doubles from +0x18; the fourth is the bytes of m_axisTravel[0..1].
    // The active map is not null-tested here.
    for (int i = 0; i < 3; ++i)
        m_axes[i] *= map->getAxisScale(static_cast<InputDeviceAxes>(i));
    double fourth;
    std::memcpy(&fourth, m_axisTravel, sizeof(fourth));
    fourth *= map->getAxisScale(static_cast<InputDeviceAxes>(3));
    std::memcpy(m_axisTravel, &fourth, sizeof(fourth));
#endif
}
namespace {
FalconInputFormat inputBuffer[1]; // PE 0xA03918, static polling buffer
const float lowerTravel[3] = { -0.0025f, -0.0025f, -0.001f };
const float upperTravel[3] = { 0.0025f, 0.0025f, 0.001f };
}
FalconDevice::FalconDevice()
    : m_refCount(1), m_active(false), m_pollTime(0),
#if BF2_FALCON_SDK
      m_haptics(&Haptics),
#else
      m_haptics(0),
#endif
      m_device(0)
{
#if defined(_M_IX86)
    static_assert(offsetof(FalconDevice, m_samples) == 0xE4, "x86 scalar prefix");
#endif
    static_assert(sizeof(FalconInputFormat) == 40, "x86 input sample alignment");
    m_field100 = 0;
    g_falconDevice = 0;
    m_buttons = 0;
    m_previousButtons = 0;
    for (int i = 0; i < 3; ++i) {
        m_axisScale[i] = 1.0f;
        m_axes[i] = 0.0;
        m_invertAxis[i] = false;
        m_axisTravel[i] = 0;
    }
    m_field0fc = false;
    m_field0f0 = 0;
    m_field0f4 = 0;
    m_field0f8 = 0;
}
FalconDevice::~FalconDevice()
{
#if BF2_FALCON_SDK
    if (m_device) {
        m_haptics->StopRunningDevice(&m_device);
        m_haptics = 0;
        g_falconDevice = 0;
        m_device = 0;
    }
#endif
}
void FalconDevice::addRef() { ++m_refCount; }
int FalconDevice::getRef() { return m_refCount; }
int FalconDevice::release()
{
    const int refs = --m_refCount;
    if (!refs) delete this;
    return refs;
}
void* FalconDevice::queryInterface(unsigned int iid) const
{
    return iid == IID_IBase || iid == IID_IInputDevice || iid == IID_IAdvancedInputDevice
        ? const_cast<FalconDevice*>(this) : 0;
}
InputDeviceTypes FalconDevice::getType() const { return IDTFalcon; }
bool FalconDevice::setCooperativeMode(InputDeviceMode mode) { m_mode = mode; return true; }
InputDeviceMode FalconDevice::getCooperativeMode() const { return m_mode; }
bool FalconDevice::activate()
{
    if (!m_device) return false;
    if (!m_active) m_active = true;
    return true;
}
bool FalconDevice::deActivate()
{
    // Retail intentionally does not clear m_active or stop the device here.
    return m_device != 0;
}
bool FalconDevice::isActive() const { return m_device && m_active; }
int FalconDevice::getNumAxes() const { return m_numAxes; }
int FalconDevice::getNumButtons() const { return m_numButtons; }
float FalconDevice::readAxis(InputDeviceAxes axis, bool) const
{
    // The scaling argument and configured scale are ignored by this body.
    return static_cast<float>(m_invertAxis[axis] ? -m_axes[axis] : m_axes[axis]);
}
void FalconDevice::setInvertAxis(InputDeviceAxes axis, bool invert)
{
    if (axis <= m_numAxes) m_invertAxis[axis] = invert;
}
bool FalconDevice::getInvertAxis(InputDeviceAxes axis) const
{
    return axis <= m_numAxes ? m_invertAxis[axis] : false;
}
void FalconDevice::setAxisScale(InputDeviceAxes axis, float scale)
{
    if (axis <= m_numAxes) m_axisScale[axis] = scale;
}
float FalconDevice::getAxisScale(InputDeviceAxes axis) const
{
    return axis <= m_numAxes ? m_axisScale[axis] : 0.0f;
}
// These no-op/zero bodies are present in both binaries, not frontier substitutes.
void FalconDevice::setAxisAdd(InputDeviceAxes, float) {}
float FalconDevice::getAxisAdd(InputDeviceAxes) const { return 0.0f; }
bool FalconDevice::buttonDown(InputDeviceButtons button) const { return (m_buttons & button) != 0; }
bool FalconDevice::buttonPressed(InputDeviceButtons button) const
{
    return !(m_previousButtons & button) && (m_buttons & button);
}
bool FalconDevice::buttonReleased(InputDeviceButtons button) const
{
    return (m_previousButtons & button) && !(m_buttons & button);
}
float FalconDevice::buttonPressedSampleLatency(InputDeviceButtons button) const
{
    for (unsigned int i = 0; i < 32; ++i)
        if (static_cast<unsigned int>(button) & (1u << i)) return m_buttonLatency[i];
    return 0.0f;
}
bool FalconDevice::init()
{
    if (!inputDeviceManager) return false;
#if !BF2_FALCON_SDK
    std::fprintf(stderr, "[x64] dice::hfe::io::FalconDevice::init: no 64-bit NovintHFX SDK; "
                         "the Falcon is reported as not connected\n");
    std::fflush(stderr);
    return false;
#else
    const HWND window = GetForegroundWindow();
    NovintHFX::ApplicationData application;
    application.window = window;
    bool active = false;
    if (m_haptics->AttachApplication(application)) {
        m_haptics->RecountConnectedDevices();
        if (m_haptics->ConnectedDeviceCount() < 1) return false;
        active = m_haptics->RunDevice(&m_device, 0);
        g_falconDevice = m_device;
        m_haptics->StartMouse(m_device);
    }
    m_active = active;
    m_numAxes = 3;
    m_numButtons = 4;
    // AttachApplication failure still returns true; zero devices returns false.
    return true;
#endif
}
bool FalconDevice::getNextInput(InputDeviceAxes& axis, InputDeviceButtons& button,
                               InputDeviceKeys& key, bool& negative)
{
    if (!m_active) return false;
    if (!m_device) return false;
#if BF2_FALCON_SDK
    m_device->InputUpdate();
    int buttons, pressed, released;
    m_device->GetButtonData(&buttons, &pressed, &released);
    for (unsigned int i = 0; i < m_numButtons; ++i) {
        if (static_cast<unsigned int>(buttons) & (1u << i)) {
            axis = static_cast<InputDeviceAxes>(15);
            button = static_cast<InputDeviceButtons>(1u << i);
            key = static_cast<InputDeviceKeys>(0);
            return true; // negative output deliberately untouched for buttons
        }
    }
    double position[3];
    m_device->GetToolPosition(position);
    for (int i = 0; i < m_numAxes; ++i) {
        int& travel = m_axisTravel[i];
        if (lowerTravel[i] > position[i]) {
            if (!travel) travel = 1;
            else if (travel == 2) travel = 3;
        }
        if (upperTravel[i] < position[i]) {
            if (!travel) { travel = 2; continue; }
            if (travel == 1) travel = 3;
        }
        if (travel == 3) {
            for (int j = 0; j < m_numAxes; ++j) m_axisTravel[j] = 0;
            axis = static_cast<InputDeviceAxes>(i);
            button = static_cast<InputDeviceButtons>(0);
            key = static_cast<InputDeviceKeys>(0);
            negative = m_axes[i] < 0.0;
            return true;
        }
    }
#endif
    return false;
}
void FalconDevice::update(float dt)
{
    if (!m_active || !m_device) return;
    if (dt > 0.0f) {
        updateCurrentState(dt);
        return;
    }
#if BF2_FALCON_SDK
    if (m_samples.size() > 256) m_samples.clear();
    m_haptics->InputUpdate();
    double position[3];
    m_device->GetToolPosition(position);
    inputBuffer[0].position[0] = position[0];
    inputBuffer[0].position[1] = position[1];
    inputBuffer[0].position[2] = position[2];
    int pressed, released;
    m_device->GetButtonData(reinterpret_cast<int*>(&inputBuffer[0].buttons), &pressed, &released);
    inputBuffer[0].time = GetTickCount();
    m_sampleTime = m_pollTime;
    m_pollTime = GetTickCount();
    for (unsigned int i = 0; i < 1; ++i) m_samples.push_back(inputBuffer[i]);
#endif
}
} } }
#endif
