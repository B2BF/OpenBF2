// Path [INFERRED]. Client PE recovery; symbolized ELF supplies names/signatures.
// [PLATFORM] ELF has no Novint imports and compiled-out init/update/device calls.
#include "FalconDevice.h"
#if !DEDICATED
#include "../../Core/Ids.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
namespace dice { namespace hfe { namespace io {
extern IBase* inputDeviceManager; // PE 0x9FFCB0, singleton registration 0x698542
NovintHFX::Device* g_falconDevice = 0;
int g_falconMouseMode = 1; // PE978FC4; updateCurrentState's consumer remains its named frontier.
// [TEMP] [CHOSEN, not recovered] Reports and stops when client Falcon sampling is reached.
// [RETIRE-WHEN: FALCON-STATE-DIAGNOSTIC] Replace with complete sampling, haptics/settings and
// force-effect behavior, retaining the observed fourth-axis defect.
void FalconDevice::updateCurrentState(float)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::io::FalconDevice::updateCurrentState reached\n");
    std::fflush(stderr);
    std::abort();
}
namespace {
FalconInputFormat inputBuffer[1]; // PE 0xA03918, static polling buffer
const float lowerTravel[3] = { -0.0025f, -0.0025f, -0.001f };
const float upperTravel[3] = { 0.0025f, 0.0025f, 0.001f };
}
// [CHOSEN, not recovered] x64 (https://github.com/B2BF/OpenBF2/issues/7): NovintHFX ships only as a
// 32-bit DLL. The x64 image never calls it: init() reports the device absent through the same
// `false` retail returns with no device connected, so m_device stays null and every device path
// below returns at its existing null test.
#if defined(_M_IX86)
#define BF2_FALCON_SDK 1
#else
#define BF2_FALCON_SDK 0
#endif
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
        updateCurrentState(dt); // [FRONTIER] retained actual positive-dt dependency
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
