// [CHOSEN, not recovered] macOS port: game-controller device over GCController. The accessor
// bodies, the getNextInput thresholds and the axis/POV/button scaling follow
// GameControllerDevice.cpp; DirectInput's joystick is replaced by a polled extended gamepad
// (Platform/macOS/MacGamepad) presented with the axis, button and POV layout an Xbox controller
// has under DirectInput. See MacInputDeviceManager.h and docs/macos-s2-4.md.

#include "MacInputDeviceManager.h"
#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))
#include "../../Core/Ids.h"
#include "../../Platform/macOS/MacGamepad.h"
#include <cmath>

namespace dice { namespace hfe { namespace io {
extern IBase* inputDeviceManager;

namespace {
unsigned int buttonBit(unsigned int index) { return 1u << (index & 31u); }

void addPovButton(unsigned int& buttons, int direction, unsigned int base)
{
    switch (direction)
    {
    case 0:     buttons |= buttonBit(base);     break;
    case 9000:  buttons |= buttonBit(base + 1); break;
    case 18000: buttons |= buttonBit(base + 2); break;
    case 27000: buttons |= buttonBit(base + 3); break;
    default: break;
    }
}

// DirectInput range -1000..1000 (enumAxis's DIPROP_RANGE), rounded to whole units as a device
// reports them.
long diAxis(float value)
{
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;
    return std::lround(static_cast<double>(value) * 1000.0);
}
} // namespace

MacGameControllerDevice::MacGameControllerDevice()
{
    m_refCount = 1;
    m_mode = IDM_EXCLUSIVE;
    m_active = false;
    m_bound = false;
    m_identity = 0;
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
    for (int i = 0; i != 4; ++i)
        m_povs[i] = -1;
    m_numAxes = 0;
    m_numPOVs = 0;
    m_numButtons = 0;
    m_deviceIndex = -1;
    m_povsAsButtons = true;
}

MacGameControllerDevice::~MacGameControllerDevice() {}

void MacGameControllerDevice::addRef() { ++m_refCount; }
int MacGameControllerDevice::getRef() { return m_refCount; }
int MacGameControllerDevice::release()
{
    const int refs = --m_refCount;
    if (!refs) delete this;
    return refs;
}
void* MacGameControllerDevice::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IInputDevice || iid == IID_IAdvancedInputDevice)
        return const_cast<MacGameControllerDevice*>(this);
    return nullptr;
}
InputDeviceTypes MacGameControllerDevice::getType() const { return IDTGameController; }
InputDeviceMode MacGameControllerDevice::getCooperativeMode() const { return m_mode; }
bool MacGameControllerDevice::setCooperativeMode(InputDeviceMode mode)
{
    m_mode = mode;
    return true;
}
bool MacGameControllerDevice::activate()
{
    if (!m_bound) return false;
    m_active = macos::CocoaHost::instance().hasFocus();
    return true;
}
bool MacGameControllerDevice::deActivate()
{
    if (!m_bound) return false;
    m_active = false;
    return true;
}
bool MacGameControllerDevice::isActive() const { return m_bound && m_active; }
int MacGameControllerDevice::getNumAxes() const { return m_numAxes; }
void MacGameControllerDevice::peakXYAxis(float& x, float& y) { y = 0; x = 0; }

void MacGameControllerDevice::setInvertAxis(InputDeviceAxes axis, bool invert)
{ if (axis <= m_numAxes) m_invertAxis[axis] = invert; }
bool MacGameControllerDevice::getInvertAxis(InputDeviceAxes axis) const
{ return axis <= m_numAxes ? m_invertAxis[axis] : false; }
void MacGameControllerDevice::setAxisScale(InputDeviceAxes axis, float scale)
{ if (axis <= m_numAxes) m_axisScale[axis] = scale; }
float MacGameControllerDevice::getAxisScale(InputDeviceAxes axis) const
{ return axis <= m_numAxes ? m_axisScale[axis] : 0.0f; }
void MacGameControllerDevice::setAxisAdd(InputDeviceAxes axis, float add)
{ if (axis <= m_numAxes) m_axisAdd[axis] = add; }
float MacGameControllerDevice::getAxisAdd(InputDeviceAxes axis) const
{ return axis <= m_numAxes ? m_axisAdd[axis] : 0.0f; }
float MacGameControllerDevice::readAxis(InputDeviceAxes axis, bool) const
{ return m_invertAxis[axis] ? m_axisAdd[axis] - m_axes[axis] : m_axisAdd[axis] + m_axes[axis]; }
int MacGameControllerDevice::getNumButtons() const
{ return m_povsAsButtons ? int(m_numButtons + 4u * m_numPOVs) : int(m_numButtons); }
bool MacGameControllerDevice::buttonDown(InputDeviceButtons button) const
{ return (static_cast<unsigned int>(button) & m_buttons) != 0; }
bool MacGameControllerDevice::buttonPressed(InputDeviceButtons button) const
{
    const unsigned int bit = static_cast<unsigned int>(button);
    return !(bit & m_previousButtons) && (bit & m_buttons);
}
bool MacGameControllerDevice::buttonReleased(InputDeviceButtons button) const
{
    const unsigned int bit = static_cast<unsigned int>(button);
    return (bit & m_previousButtons) && !(bit & m_buttons);
}
float MacGameControllerDevice::buttonPressedSampleLatency(InputDeviceButtons) const { return 0.0f; }
int MacGameControllerDevice::getNumPOVs() const { return m_numPOVs; }
int MacGameControllerDevice::readPOV(InputDevicePOVs pov) const
{ return pov < m_numPOVs ? m_povs[pov] : -1; }
bool MacGameControllerDevice::slot_0x70() const { return false; }
int MacGameControllerDevice::getDeviceIndex() const { return m_deviceIndex; }

bool MacGameControllerDevice::getNextInput(InputDeviceAxes& axis, InputDeviceButtons& button,
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

// Polled once per positive-dt pass, like DX8 (which skips only negative dt). A controller that
// disappeared, or whose index now names another controller, yields no new state.
void MacGameControllerDevice::update(float deltaTime)
{
    if (deltaTime < 0.0f || !m_bound) return;
    if (macos::gamepadIdentity(m_deviceIndex) != m_identity) return;
    macos::GamepadState state;
    if (!macos::readGamepad(m_deviceIndex, state)) return;

    // X, Y, Z, Rx, Ry in DirectInput order; DirectInput Y axes grow downwards and Z is the
    // combined trigger axis (left trigger positive).
    const long axes[5] = {
        diAxis(state.leftX), diAxis(-state.leftY),
        diAxis(state.leftTrigger - state.rightTrigger),
        diAxis(state.rightX), diAxis(-state.rightY) };
    for (int i = 0; i != 5; ++i)
        m_axes[i] = float(double(axes[i]) * double(0.001f) * double(m_axisScale[i]));
    m_povs[0] = state.dpad;

    m_previousButtons = m_buttons;
    m_buttons = state.buttons & ((1u << m_numButtons) - 1u);
    for (int i = 0; i < m_numPOVs; ++i)
        addPovButton(m_buttons, m_povs[i], m_numButtons + 4u * i);
}

bool MacGameControllerDevice::init(int index)
{
    if (inputDeviceManager)
    {
        if (index < 0 || index >= macos::gamepadCount())
            return false;
        m_deviceIndex = index;
        m_identity = macos::gamepadIdentity(index);
        m_bound = m_identity != 0;
        if (!m_bound) return false;
        if (!setCooperativeMode(IDM_EXCLUSIVE)) return false;
        m_numAxes = 5;
        m_numButtons = macos::GamepadButtonCount;
        m_numPOVs = 1;
    }
    return true;
}

} } }
#endif
