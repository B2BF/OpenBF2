// Game-controller device sampling and callbacks.
#include "GameControllerDevice.h"
#if !DEDICATED
#include "../../Core/Debug.h"
#include "../../Core/Ids.h"
#include <Windows.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <cmath>
#include <cstring>
#include <new>

namespace dice { namespace hfe { namespace io {
extern IBase* inputDeviceManager;

namespace {
const char kSourcePath[] = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\IO\\Input\\GameControllerDevice.cpp";
// PE dword_A04AE0: one shared 128-entry buffered-input workspace, not a member.
DIDEVICEOBJECTDATA g_controllerData[128];

struct ControllerEnumeration
{
    IDirectInputDevice8A* device;
    IDirectInput8A* directInput;
    int requestedIndex;
    int currentIndex;
};

// stop at the selected index even if CreateDevice failed.
BOOL CALLBACK enumController(const DIDEVICEINSTANCEA* instance, void* context)
{
    ControllerEnumeration& state = *static_cast<ControllerEnumeration*>(context);
    if (state.requestedIndex == state.currentIndex)
    {
        state.directInput->CreateDevice(instance->guidInstance, &state.device, nullptr);
        return DIENUM_STOP;
    }
    ++state.currentIndex;
    return DIENUM_CONTINUE;
}

struct AxisEnumeration
{
    IDirectInputDevice8A* device;
    unsigned int mask;
    int sliders;
};

// GetFocus's return is discarded, and each GUID comparison is made
// independently after setting the per-object range. Failure stops enumeration.
BOOL CALLBACK enumAxis(const DIDEVICEOBJECTINSTANCEA* object, void* context)
{
    AxisEnumeration& state = *static_cast<AxisEnumeration*>(context);
    GetFocus();
    DIPROPRANGE range;
    range.diph.dwSize = sizeof(range);
    range.diph.dwHeaderSize = sizeof(range.diph);
    range.diph.dwObj = object->dwType;
    range.diph.dwHow = DIPH_BYID;
    range.lMin = -1000;
    range.lMax = 1000;
    if (FAILED(state.device->SetProperty(DIPROP_RANGE, &range.diph)))
        return DIENUM_STOP;
    if (!std::memcmp(&object->guidType, &GUID_XAxis, sizeof(GUID))) state.mask |= 1;
    if (!std::memcmp(&object->guidType, &GUID_YAxis, sizeof(GUID))) state.mask |= 2;
    if (!std::memcmp(&object->guidType, &GUID_ZAxis, sizeof(GUID))) state.mask |= 4;
    if (!std::memcmp(&object->guidType, &GUID_RxAxis, sizeof(GUID))) state.mask |= 8;
    if (!std::memcmp(&object->guidType, &GUID_RyAxis, sizeof(GUID))) state.mask |= 16;
    if (!std::memcmp(&object->guidType, &GUID_RzAxis, sizeof(GUID))) state.mask |= 32;
    if (!std::memcmp(&object->guidType, &GUID_Slider, sizeof(GUID)))
    {
        state.mask |= 64;
        ++state.sliders;
    }
    return DIENUM_CONTINUE;
}

std::uint32_t buttonBit(unsigned int index)
{
    // Shipped x86 variable shifts wrap at 32, even for devices with >32 buttons.
    return std::uint32_t(1) << (index & 31u);
}

void addPovButton(std::uint32_t& buttons, DWORD direction, unsigned int base)
{
    // Both the immediate and buffered paths recognize only the four cardinals.
    switch (direction)
    {
    case 0: buttons |= buttonBit(base); break;
    case 9000: buttons |= buttonBit(base + 1); break;
    case 18000: buttons |= buttonBit(base + 2); break;
    case 27000: buttons |= buttonBit(base + 3); break;
    }
}
}

// m_mode, m_numAxes, m_povs and m_directInput remain untouched.
GameControllerDevice::GameControllerDevice()
{
    m_refCount = 1;
    m_active = false;
    m_device = nullptr;
    m_capabilities = nullptr;
    m_buttons = 0;
    m_previousButtons = 0;
    for (int i = 0; i != 8; ++i)
    {
        m_axes[i] = 0.0f;
        m_axisScale[i] = 1.0f;
        m_axisAdd[i] = 0.0f;
        m_invertAxis[i] = false;
        m_axisReady[i] = false;
    }
    m_axisMask = 0;
    m_numSliders = 0;
    m_numPOVs = 0;
    m_numButtons = 0;
    m_deviceIndex = -1;
    m_povsAsButtons = true;
}

// Initialization failures leave cleanup to this exact ownership path.
GameControllerDevice::~GameControllerDevice()
{
    if (m_capabilities)
    {
        delete m_capabilities;
        m_capabilities = nullptr;
    }
    if (m_device)
    {
        m_device->Unacquire();
        m_device->Release();
        m_device = nullptr;
        m_deviceIndex = -1;
    }
    if (m_directInput)
    {
        m_directInput->Release();
        m_directInput = nullptr;
    }
}

void GameControllerDevice::addRef() { ++m_refCount; }
int GameControllerDevice::getRef() { return m_refCount; }
int GameControllerDevice::release()
{
    const int refs = --m_refCount;
    if (!refs) delete this;
    return refs;
}
void* GameControllerDevice::queryInterface(unsigned int iid) const
{
    if (iid == 1 || iid == IID_IInputDevice || iid == IID_IAdvancedInputDevice)
        return const_cast<GameControllerDevice*>(this);
    return nullptr;
}
InputDeviceTypes GameControllerDevice::getType() const { return IDTGameController; }
InputDeviceMode GameControllerDevice::getCooperativeMode() const { return m_mode; }
bool GameControllerDevice::setCooperativeMode(InputDeviceMode mode)
{
    const DWORD flags = mode ? DISCL_NONEXCLUSIVE | DISCL_FOREGROUND
                             : DISCL_EXCLUSIVE | DISCL_FOREGROUND;
    if (FAILED(m_device->SetCooperativeLevel(GetFocus(), flags))) return false;
    m_mode = mode;
    return true;
}
bool GameControllerDevice::activate()
{
    if (!m_device) return false;
    m_active = m_device->Acquire() == DI_OK;
    return true;
}
bool GameControllerDevice::deActivate()
{
    if (!m_device) return false;
    m_device->Unacquire();
    m_active = false;
    return true;
}
bool GameControllerDevice::isActive() const { return m_device && m_active; }
int GameControllerDevice::getNumAxes() const { return m_numAxes; }
void GameControllerDevice::peakXYAxis(float& x, float& y) { y = 0; x = 0; }

// Axis setters/getters deliberately keep the signed <= bound and missing lower
// bound. readAxis has no bound at all, ignores its bool and does not scale again.
void GameControllerDevice::setInvertAxis(InputDeviceAxes axis, bool invert)
{ if (axis <= m_numAxes) m_invertAxis[axis] = invert; }
bool GameControllerDevice::getInvertAxis(InputDeviceAxes axis) const
{ return axis <= m_numAxes ? m_invertAxis[axis] : false; }
void GameControllerDevice::setAxisScale(InputDeviceAxes axis, float scale)
{ if (axis <= m_numAxes) m_axisScale[axis] = scale; }
float GameControllerDevice::getAxisScale(InputDeviceAxes axis) const
{ return axis <= m_numAxes ? m_axisScale[axis] : 0.0f; }
void GameControllerDevice::setAxisAdd(InputDeviceAxes axis, float add)
{ if (axis <= m_numAxes) m_axisAdd[axis] = add; }
float GameControllerDevice::getAxisAdd(InputDeviceAxes axis) const
{ return axis <= m_numAxes ? m_axisAdd[axis] : 0.0f; }
float GameControllerDevice::readAxis(InputDeviceAxes axis, bool) const
{ return m_invertAxis[axis] ? m_axisAdd[axis] - m_axes[axis] : m_axisAdd[axis] + m_axes[axis]; }
int GameControllerDevice::getNumButtons() const
{ return m_povsAsButtons ? int(m_numButtons + 4u * m_numPOVs) : int(m_numButtons); }
bool GameControllerDevice::buttonDown(InputDeviceButtons button) const
{ return (std::uint32_t(button) & m_buttons) != 0; }
bool GameControllerDevice::buttonPressed(InputDeviceButtons button) const
{ return !(std::uint32_t(button) & m_previousButtons) && (std::uint32_t(button) & m_buttons); }
bool GameControllerDevice::buttonReleased(InputDeviceButtons button) const
{ return (std::uint32_t(button) & m_previousButtons) && !(std::uint32_t(button) & m_buttons); }
float GameControllerDevice::buttonPressedSampleLatency(InputDeviceButtons) const
{ return 0.0f; }
int GameControllerDevice::getNumPOVs() const { return m_numPOVs; }
int GameControllerDevice::readPOV(InputDevicePOVs pov) const
{ return pov < m_numPOVs ? m_povs[pov] : -1; }
bool GameControllerDevice::slot_0x70() const { return false; }              // 0x5AB2B0, real retail body
int GameControllerDevice::getDeviceIndex() const { return m_deviceIndex; }

// Buttons take precedence and leave the caller's direction bool alone.
bool GameControllerDevice::getNextInput(InputDeviceAxes& axis, InputDeviceButtons& button,
                                         InputDeviceKeys& key, bool& negative)
{
    for (int i = 0; i < getNumButtons(); ++i)
    {
        const InputDeviceButtons bit = static_cast<InputDeviceButtons>(buttonBit(i));
        if (buttonPressed(bit))
        {
            axis = static_cast<InputDeviceAxes>(15);
            button = bit;
            key = static_cast<InputDeviceKeys>(0);
            return true;
        }
    }
    for (int i = 0; i < m_numAxes; ++i)
    {
        const float magnitude = std::fabs(m_axes[i]);
        if (magnitude < 0.2f)
            m_axisReady[i] = true;
        else if (m_axisReady[i] && magnitude > 0.95f)
        {
            m_axisReady[i] = false;
            axis = static_cast<InputDeviceAxes>(i);
            button = static_cast<InputDeviceButtons>(0);
            key = static_cast<InputDeviceKeys>(0);
            negative = m_axes[i] < 0.0f;
            return true;
        }
    }
    return false;
}

// Poll/reacquire, immediate state, then buffered-press merge. No focus
// or m_active guard, no capability clamping, and no bounded retry were shipped.
void GameControllerDevice::update(float deltaTime)
{
    if (deltaTime < 0.0f || !m_device) return;
    if (FAILED(m_device->Poll()))
    {
        while (m_device->Acquire() == DIERR_INPUTLOST) {}
        return;
    }
    DIJOYSTATE2 state;
    if (FAILED(m_device->GetDeviceState(sizeof(state), &state))) return;
    const LONG axes[6] = {state.lX, state.lY, state.lZ, state.lRx, state.lRy, state.lRz};
    int target = 0;
    for (int source = 0; source != 6; ++source)
    {
        if (m_axisMask & (1u << source))
        {
            m_axes[target] = float(double(axes[source]) * double(0.001f) * double(m_axisScale[target]));
            ++target;
        }
    }
    if (m_numSliders > 0)
    {
        m_axes[target] = float(double(state.rglSlider[0]) * double(0.001f) * double(m_axisScale[target]));
        ++target;
    }
    if (m_numSliders > 1)
        m_axes[target] = float(double(state.rglSlider[1]) * double(0.001f) * double(m_axisScale[target]));
    for (int i = 0; i < m_numPOVs; ++i)
        m_povs[i] = LOWORD(state.rgdwPOV[i]) == 0xFFFF ? -1 : int(state.rgdwPOV[i]);

    m_previousButtons = m_buttons;
    m_buttons = 0;
    for (unsigned int i = 0; i < m_numButtons; ++i)
        m_buttons |= ((state.rgbButtons[i] & 0x80) ? buttonBit(i) : 0u);
    for (int i = 0; i < m_numPOVs; ++i)
        addPovButton(m_buttons, DWORD(m_povs[i]), m_numButtons + 4u * i);

    DWORD count = 128;
    if (FAILED(m_device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), g_controllerData, &count, 0))) return;
    for (DWORD eventIndex = 0; eventIndex < count; ++eventIndex)
    {
        const DIDEVICEOBJECTDATA& event = g_controllerData[eventIndex];
        for (int i = 0; i < int(m_numButtons); ++i)
            if (event.dwOfs == DWORD(DIJOFS_BUTTON(i)) && (event.dwData & 0x80))
                m_buttons |= buttonBit(i);
        for (int i = 0; i < m_numPOVs; ++i)
            if (event.dwOfs == DWORD(DIJOFS_POV(i)))
                addPovButton(m_buttons, event.dwData, m_numButtons + 4u * i);
    }
}

// each controller owns a separate DirectInput8 instance. Error
// diagnostics are real severity-6 Debug calls; their filtering/abort is shared.
bool GameControllerDevice::initDirectInput()
{
    const HRESULT result = DirectInput8Create(GetModuleHandleA(nullptr), 0x0800,
        IID_IDirectInput8A, reinterpret_cast<void**>(&m_directInput), nullptr);
    if (SUCCEEDED(result)) return true;
    switch (result)
    {
    case DIERR_BETADIRECTINPUTVERSION:
        Debug(DebugTypeError, kSourcePath, 80, "") << "InputDeviceBase error betaversion"; break;
    case DIERR_INVALIDPARAM:
        Debug(DebugTypeError, kSourcePath, 83, "") << "InputDeviceBase error invalid param"; break;
    case DIERR_OLDDIRECTINPUTVERSION:
        Debug(DebugTypeError, kSourcePath, 86, "") << "InputDeviceBase error oldversion"; break;
    case DIERR_OUTOFMEMORY:
        Debug(DebugTypeError, kSourcePath, 89, "") << "InputDeviceBase error out of memory"; break;
    default:
        Debug(DebugTypeError, kSourcePath, 93, "") << "InputDeviceBase error other error " << result; break;
    }
    return false;
}

bool GameControllerDevice::init(int index)
{
    if (inputDeviceManager)
    {
        if (!initDirectInput())
        {
            Debug(DebugTypeError, kSourcePath, 176, "") << "init failed";
            return false;
        }
        if (!m_directInput) return false;
        ControllerEnumeration selected = {nullptr, m_directInput, index, 0};
        if (FAILED(m_directInput->EnumDevices(DI8DEVCLASS_GAMECTRL, enumController,
                                              &selected, DIEDFL_ATTACHEDONLY))) return false;
        m_device = selected.device;
        if (!m_device || FAILED(m_device->SetDataFormat(&c_dfDIJoystick2))) return false;
        DIPROPDWORD buffering;
        buffering.diph.dwSize = sizeof(buffering);
        buffering.diph.dwHeaderSize = sizeof(buffering.diph);
        buffering.diph.dwObj = 0;
        buffering.diph.dwHow = DIPH_DEVICE;
        buffering.dwData = 128;
        if (FAILED(m_device->SetProperty(DIPROP_BUFFERSIZE, &buffering.diph)))
            Debug(DebugTypeError, kSourcePath, 226, "") << "Failed to init buffered mode";
        if (!setCooperativeMode(IDM_EXCLUSIVE)) return false;
        m_capabilities = new(std::nothrow) DIDEVCAPS;
        if (!m_capabilities) return false;
        m_capabilities->dwSize = sizeof(DIDEVCAPS);
        if (FAILED(m_device->GetCapabilities(m_capabilities))) return false;
        m_numAxes = int(m_capabilities->dwAxes);
        m_numButtons = m_capabilities->dwButtons;
        m_numPOVs = int(m_capabilities->dwPOVs);
        AxisEnumeration axes = {m_device, 0, 0};
        if (FAILED(m_device->EnumObjects(enumAxis, &axes, DIDFT_AXIS))) return false;
        m_numSliders = axes.sliders;
        m_axisMask = axes.mask;
    }
    return true;
}

#if defined(_M_IX86)
static_assert(sizeof(GameControllerDevice) == 0xBC, "BF2 client GameControllerDevice layout");
#endif
static_assert(sizeof(DIDEVCAPS) == 44, "Retail DirectInput capabilities size");
static_assert(sizeof(DIJOYSTATE2) == 272, "Retail joystick2 data format");
// GetDeviceData is passed sizeof(DIDEVICEOBJECTDATA) and a typed array, so the x64 stride
// (24 bytes, UINT_PTR uAppData) needs no adjustment.
#if defined(_M_IX86)
static_assert(sizeof(DIDEVICEOBJECTDATA) == 20, "Retail x86 buffered record stride");
#endif

} } }
#endif
