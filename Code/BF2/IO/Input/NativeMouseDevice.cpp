// [CHOSEN, not recovered] Native mouse device. The sampling bodies (getNextInput,
// peakXYAxis, updateSamples, the -1/positive update protocol, the 256-entry queue limits and the
// axis scaling through the active control map) follow MouseDevice.cpp; DirectInput's buffered
// c_dfDIMouse2 data is replaced by host mouse events. See NativeInputDeviceManager.h.

#include "NativeInputDeviceManager.h"
#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))
#include "../../Core/Ids.h"
#include <cmath>

namespace dice { namespace hfe { namespace io {
extern IBase* inputDeviceManager;
extern IBase* controlMapManager;

namespace {
// DIMOUSESTATE2 data-format offsets (DIMOFS_*).
const unsigned int kOffsetX = 0, kOffsetY = 4, kOffsetZ = 8, kOffsetButton0 = 12;
const size_t kMouseBufferSize = 128;            // MouseDevice::init's DIPROP_BUFFERSIZE
// [CHOSEN] c_dfDIMouse2 carries eight buttons; macOS does not report a button count.
const unsigned int kMouseButtons = 8;
// [CHOSEN] X, Y and the wheel, as DirectInput reports for a wheel mouse.
const int kMouseAxes = 3;
host::ClientHost& clientHost() { return host::ClientHost::instance(); }
}

NativeMouseDevice::NativeMouseDevice()
    : m_refCount(1), m_mode(IDM_EXCLUSIVE), m_numButtons(0), m_buttons(0), m_previousButtons(0),
      m_numAxes(0), m_initialized(false), m_active(false), m_inputLost(false), m_overflow(false),
      m_pollTime(0), m_sampleTime(0), m_remainderX(0.0), m_remainderY(0.0), m_lastSequence(0)
{
    for (int i = 0; i < 4; ++i) {
        m_axes[i] = 0.0f;
        m_axisScale[i] = 0.03f;
        m_invertAxis[i] = false;
        m_axisArmed[i] = false;
    }
    // DX8 leaves the latencies and sample time untouched; the port clears them.
    for (int i = 0; i < 32; ++i)
        m_buttonLatency[i] = 0.0f;
}

NativeMouseDevice::~NativeMouseDevice()
{
    if (m_initialized) {
        if (m_active && m_mode == IDM_EXCLUSIVE)
            clientHost().setRelativeMouseMode(false);
        clientHost().removeListener(this);
    }
}

void NativeMouseDevice::addRef() { ++m_refCount; }
int NativeMouseDevice::getRef() { return m_refCount; }
int NativeMouseDevice::release()
{
    const int count = --m_refCount;
    if (count == 0)
        delete this;
    return count;
}
void* NativeMouseDevice::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IInputDevice || iid == IID_IAdvancedInputDevice)
        return const_cast<NativeMouseDevice*>(this);
    return 0;
}
InputDeviceTypes NativeMouseDevice::getType() const { return static_cast<InputDeviceTypes>(1); }
InputDeviceMode NativeMouseDevice::getCooperativeMode() const { return m_mode; }

// Exclusive is DirectInput's DISCL_EXCLUSIVE | DISCL_FOREGROUND: while acquired the cursor is
// hidden and held. That is the host's relative mouse mode. Non-exclusive leaves the
// cursor alone and still delivers relative motion.
bool NativeMouseDevice::setCooperativeMode(InputDeviceMode mode)
{
    m_mode = mode;
    applyRelativeMode();
    return true;
}

void NativeMouseDevice::applyRelativeMode()
{
    if (m_initialized)
        clientHost().setRelativeMouseMode(m_active && m_mode == IDM_EXCLUSIVE);
}

bool NativeMouseDevice::activate()
{
    if (!m_initialized)
        return false;
    if (!m_active) {
        m_active = clientHost().hasFocus();
        m_pending.clear();
        m_overflow = false;
        m_inputLost = false;
        m_remainderX = m_remainderY = 0.0;
        applyRelativeMode();
    }
    return true;                                        // also when acquisition fails
}

bool NativeMouseDevice::deActivate()
{
    if (!m_initialized)
        return false;
    if (m_active) {
        m_active = false;
        applyRelativeMode();
        m_samples.clear();                              // peak queue is retained
        m_pending.clear();
        m_buttons = m_previousButtons = 0;
        m_axes[0] = m_axes[1] = m_axes[2] = m_axes[3] = 0.0f;
    }
    return true;
}

bool NativeMouseDevice::isActive() const { return m_initialized && m_active; }
int NativeMouseDevice::getNumAxes() const { return m_numAxes; }
int NativeMouseDevice::getNumButtons() const { return m_numButtons; }
float NativeMouseDevice::readAxis(InputDeviceAxes axis, bool) const
{
    return m_invertAxis[axis] ? -m_axes[axis] : m_axes[axis];
}
void NativeMouseDevice::setInvertAxis(InputDeviceAxes axis, bool invert)
{
    if (axis <= m_numAxes) m_invertAxis[axis] = invert;
}
bool NativeMouseDevice::getInvertAxis(InputDeviceAxes axis) const
{
    return axis <= m_numAxes ? m_invertAxis[axis] : false;
}
void NativeMouseDevice::setAxisScale(InputDeviceAxes axis, float scale)
{
    if (axis <= m_numAxes) m_axisScale[axis] = scale;
}
float NativeMouseDevice::getAxisScale(InputDeviceAxes axis) const
{
    return axis <= m_numAxes ? m_axisScale[axis] : 0.0f;
}
void NativeMouseDevice::setAxisAdd(InputDeviceAxes, float) {}
float NativeMouseDevice::getAxisAdd(InputDeviceAxes) const { return 0.0f; }

bool NativeMouseDevice::buttonDown(InputDeviceButtons button) const
{
    return (m_buttons & button) != 0;
}
bool NativeMouseDevice::buttonPressed(InputDeviceButtons button) const
{
    return (m_previousButtons & button) == 0 && (m_buttons & button) != 0;
}
bool NativeMouseDevice::buttonReleased(InputDeviceButtons button) const
{
    return (m_previousButtons & button) != 0 && (m_buttons & button) == 0;
}
float NativeMouseDevice::buttonPressedSampleLatency(InputDeviceButtons button) const
{
    for (unsigned int i = 0; i < 32; ++i)
        if ((1u << i) & button)
            return m_buttonLatency[i];
    return 0.0f;
}

bool NativeMouseDevice::getNextInput(InputDeviceAxes& axis, InputDeviceButtons& button,
                                  InputDeviceKeys& key, bool& negative)
{
    for (unsigned int i = 0; i < m_numButtons; ++i) {
        const InputDeviceButtons candidate = static_cast<InputDeviceButtons>(1u << (i & 31));
        if (buttonPressed(candidate)) {
            axis = static_cast<InputDeviceAxes>(15);
            button = candidate;
            key = static_cast<InputDeviceKeys>(0);
            return true;
        }
    }
    for (int i = 0; i < m_numAxes; ++i) {
        const float magnitude = std::fabs(m_axes[i]);
        if (magnitude < 0.1f) {
            m_axisArmed[i] = true;
        } else if (m_axisArmed[i] && magnitude >= 0.45f) {
            m_axisArmed[i] = false;
            axis = static_cast<InputDeviceAxes>(i);
            button = static_cast<InputDeviceButtons>(0);
            key = static_cast<InputDeviceKeys>(0);
            negative = m_axes[i] < 0.0f;
            return true;
        }
    }
    return false;
}

void NativeMouseDevice::peakXYAxis(float& x, float& y)
{
    y = 0.0f;
    x = 0.0f;
    while (!m_peakSamples.empty()) {
        const NativeInputEvent& event = m_peakSamples.front();
        if (event.sequence > m_lastSequence) {
            if (event.offset == kOffsetX)
                x += static_cast<int>(event.data);
            else if (event.offset == kOffsetY)
                y += static_cast<int>(event.data);
        }
        m_peakSamples.pop_front();
    }
    IControlMap* map = static_cast<IControlMapManager*>(controlMapManager)->getActive();
    x = map->getAxisScale(static_cast<InputDeviceAxes>(0)) * x;
    x = map->getMouseSensitivity() * x;
    y = map->getAxisScale(static_cast<InputDeviceAxes>(1)) * y;
    y = map->getMouseSensitivity() * y;
    if (map->getInvertMouse())
        y = -y;
}

void NativeMouseDevice::updateSamples(float deltaTime)
{
    IControlMap* map = static_cast<IControlMapManager*>(controlMapManager)->getActive();
    m_axes[0] = m_axes[1] = m_axes[2] = 0.0f;
    m_previousButtons = m_buttons;
    m_sampleTime += static_cast<int>(static_cast<double>(deltaTime) * 1000.0);
    while (!m_samples.empty()) {
        const NativeInputEvent& event = m_samples.front();
        if (event.timeStamp >= m_sampleTime)
            break;
        const float age = static_cast<float>(m_pollTime - event.timeStamp) * 0.001f;
        const float latency = age <= 0.033333335f ? age : 0.033333335f;
        if (event.offset == kOffsetX) {
            m_axes[0] += static_cast<int>(event.data);
        } else if (event.offset == kOffsetY) {
            m_axes[1] += static_cast<int>(event.data);
        } else if (event.offset == kOffsetZ) {
            m_axes[2] += static_cast<int>(event.data);
        } else if (event.offset >= kOffsetButton0 && event.offset < kOffsetButton0 + 8) {
            const unsigned int index = event.offset - kOffsetButton0;
            const unsigned int mask = 1u << index;
            if (event.data & 0x80) {
                m_buttons |= mask;
                m_buttonLatency[index] = latency;
            } else {
                m_buttons &= ~mask;
            }
        }
        m_lastSequence = event.sequence;
        m_samples.pop_front();
    }
    for (int i = 0; i < 4; ++i)
        m_axes[i] = map->getAxisScale(static_cast<InputDeviceAxes>(i)) * m_axes[i];
}

void NativeMouseDevice::clearState()
{
    m_samples.clear();
    m_peakSamples.clear();
    m_pending.clear();
    m_buttons = m_previousButtons = 0;
    m_axes[0] = m_axes[1] = m_axes[2] = m_axes[3] = 0.0f;
}

void NativeMouseDevice::update(float deltaTime)
{
    if (!m_active || !m_initialized)
        return;
    if (deltaTime > 0.0f) {
        updateSamples(deltaTime);
        return;
    }
    if (m_peakSamples.size() > 256)
        m_peakSamples.clear();
    if (m_samples.size() > 256) {
        m_samples.clear();
        m_peakSamples.clear();
    }
    if (m_inputLost) {                                  // DIERR_INPUTLOST, then reacquire
        clearState();
        if (!clientHost().hasFocus())
            return;
        m_inputLost = false;
    }
    if (m_overflow) {                                   // any non-DI_OK result clears
        clearState();
        m_overflow = false;
        return;
    }
    m_sampleTime = m_pollTime;
    m_pollTime = host::ClientHost::tickCountMs();
    for (size_t i = 0; i < m_pending.size(); ++i) {
        m_samples.push_back(m_pending[i]);
        m_peakSamples.push_back(m_pending[i]);
    }
    m_pending.clear();
}

bool NativeMouseDevice::init()
{
    if (!inputDeviceManager)
        return false;
    clientHost().addListener(this);
    m_initialized = true;
    m_numButtons = kMouseButtons;
    m_numAxes = kMouseAxes;
    return setCooperativeMode(static_cast<InputDeviceMode>(0)) != false;
}

void NativeMouseDevice::push(unsigned int offset, unsigned int data, unsigned int timeStamp)
{
    if (m_pending.size() >= kMouseBufferSize) {
        m_overflow = true;
        return;
    }
    NativeInputEvent record;
    record.offset = offset;
    record.data = data;
    record.timeStamp = timeStamp;
    record.sequence = nextNativeInputSequence();
    m_pending.push_back(record);
}

// [CHOSEN] Relative motion, in DirectInput counts. With a GCMouse connected the counts are its
// unaccelerated device deltas (onRawMouseMove). Without one (a
// trackpad-only Mac) they fall back to NSEvent deltas, one point per count, which carry the
// system pointer acceleration. Fractions are carried to the next report so slow motion is kept.
void NativeMouseDevice::addMotion(double dx, double dy, unsigned int timeMs)
{
    m_remainderX += dx;
    m_remainderY += dy;
    const double wholeX = std::trunc(m_remainderX);
    const double wholeY = std::trunc(m_remainderY);
    m_remainderX -= wholeX;
    m_remainderY -= wholeY;
    if (wholeX != 0.0)
        push(kOffsetX, static_cast<unsigned int>(static_cast<int>(wholeX)), timeMs);
    if (wholeY != 0.0)
        push(kOffsetY, static_cast<unsigned int>(static_cast<int>(wholeY)), timeMs);
}

void NativeMouseDevice::onMouseMove(const host::HostMouseMoveEvent& event)
{
    if (!m_active || clientHost().rawMouseAvailable())
        return;
    addMotion(event.dx, event.dy, event.timeMs);
}

void NativeMouseDevice::onRawMouseMove(const host::HostRawMouseEvent& event)
{
    if (!m_active || !clientHost().rawMouseAvailable())
        return;
    addMotion(event.dx, event.dy, event.timeMs);
}

void NativeMouseDevice::onMouseButton(const host::HostMouseButtonEvent& event)
{
    if (!m_active || event.button < 0 || event.button >= static_cast<int>(kMouseButtons))
        return;
    push(kOffsetButton0 + static_cast<unsigned int>(event.button), event.down ? 0x80u : 0u,
         event.timeMs);
}

void NativeMouseDevice::onMouseWheel(const host::HostMouseWheelEvent& event)
{
    if (!m_active)
        return;
    push(kOffsetZ, static_cast<unsigned int>(event.delta), event.timeMs);
}

void NativeMouseDevice::onFocusChanged(bool windowKey, bool appActive)
{
    if (!(windowKey && appActive) && m_active)
        m_inputLost = true;
}

} } }
#endif
