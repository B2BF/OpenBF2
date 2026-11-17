// [CHOSEN, not recovered] Native keyboard device. The sampling bodies (getNextInput,
// key queries, updateCurrentState, the -1/positive update protocol) follow KeyboardDevice.cpp;
// the DirectInput device is replaced by normalized DIK key events from ClientHost.
// See NativeInputDeviceManager.h for the shared native input contract.

#include "NativeInputDeviceManager.h"
#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))
#include "../../Core/Ids.h"
#include <cstring>
#include <cwctype>

namespace dice { namespace hfe { namespace io {
extern IBase* inputDeviceManager;

namespace {
const size_t kKeyboardBufferSize = 100;         // KeyboardDevice::init's DIPROP_BUFFERSIZE
host::ClientHost& clientHost() { return host::ClientHost::instance(); }
}

NativeKeyboardDevice::NativeKeyboardDevice()
    : m_refCount(1), m_mode(IDM_NONEXCLUSIVE), m_initialized(false), m_active(false),
      m_inputLost(false), m_overflow(false)
{
    // DX8 leaves both arrays untouched until sampled; the port starts them cleared.
    std::memset(m_keys, 0, sizeof(m_keys));
    std::memset(m_previousKeys, 0, sizeof(m_previousKeys));
}

NativeKeyboardDevice::~NativeKeyboardDevice()
{
    if (m_initialized)
        clientHost().removeListener(this);
}

void NativeKeyboardDevice::addRef() { ++m_refCount; }
int NativeKeyboardDevice::getRef() { return m_refCount; }
int NativeKeyboardDevice::release()
{
    const int count = --m_refCount;
    if (count == 0) delete this;
    return count;
}
void* NativeKeyboardDevice::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IInputDevice || iid == IID_IKeyboardDevice)
        return const_cast<NativeKeyboardDevice*>(this);
    return 0;
}
InputDeviceTypes NativeKeyboardDevice::getType() const { return IDTKeyboard; }
InputDeviceMode NativeKeyboardDevice::getCooperativeMode() const { return m_mode; }

// Both DirectInput levels are foreground levels; host key events reach the device only while
// the game window has focus in either mode, so the mode is recorded and changes nothing else.
bool NativeKeyboardDevice::setCooperativeMode(InputDeviceMode mode)
{
    m_mode = mode;
    return true;
}

// Acquire succeeds only for the foreground window; success is reported either way, as in DX8.
bool NativeKeyboardDevice::activate()
{
    if (!m_initialized) return false;
    if (!m_active) {
        m_active = clientHost().hasFocus();
        m_pending.clear();
        m_overflow = false;
        m_inputLost = false;
    }
    return true;
}

bool NativeKeyboardDevice::deActivate()
{
    if (!m_initialized) return false;
    if (m_active) {
        m_active = false;
        m_events.clear();
        m_pending.clear();
        std::memset(m_keys, 0, sizeof(m_keys));
    }
    return true;
}

bool NativeKeyboardDevice::isActive() const { return m_initialized && m_active; }

bool NativeKeyboardDevice::getNextInput(InputDeviceAxes& axis, InputDeviceButtons& button,
                                     InputDeviceKeys& key, bool&)
{
    for (int i = 0; i < 255; ++i) {
        if (keyPressed(static_cast<InputDeviceKeys>(i))) {
            axis = static_cast<InputDeviceAxes>(15);
            button = static_cast<InputDeviceButtons>(0);
            key = static_cast<InputDeviceKeys>(i);
            return true;
        }
    }
    return false;
}

bool NativeKeyboardDevice::keyDown(InputDeviceKeys key) const
{ return m_keys[static_cast<unsigned char>(key)] != 0; }
bool NativeKeyboardDevice::keyPressed(InputDeviceKeys key) const
{
    const unsigned char k = static_cast<unsigned char>(key);
    return !m_previousKeys[k] && m_keys[k];
}
bool NativeKeyboardDevice::keyReleased(InputDeviceKeys key) const
{
    const unsigned char k = static_cast<unsigned char>(key);
    return m_previousKeys[k] && !m_keys[k];
}

// Printable keys are named by the character the current layout puts on them (so an AZERTY
// keyboard names DIK_Q "A", as DirectInput does on Windows); other keys use the fixed English
// fallback names supplied by the host.
std::wstring NativeKeyboardDevice::getKeyName(InputDeviceKeys key) const
{
    const unsigned char dik = static_cast<unsigned char>(key);
    const bool keypad = (dik >= 71 && dik <= 83) || dik == 55 || dik == 181 || dik == 141 || dik == 179;
    if (!keypad) {
        const std::u16string chars = clientHost().keyCharacter(dik);
        if (chars.size() == 1 && chars[0] > 0x20) {
            std::wstring name(1, static_cast<wchar_t>(std::towupper(static_cast<wint_t>(chars[0]))));
            return name;
        }
    }
    const char* english = clientHost().keyDisplayName(dik);
    std::wstring name;
    for (const char* c = english; c && *c; ++c)
        name.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
    return name;
}

bool NativeKeyboardDevice::init()
{
    if (!inputDeviceManager) return false;
    clientHost().addListener(this);
    m_initialized = true;
    if (!setCooperativeMode(IDM_NONEXCLUSIVE)) return false;
    std::memset(m_keys, 0, sizeof(m_keys));
    return true;
}

void NativeKeyboardDevice::onKey(const host::HostKeyEvent& event)
{
    if (!m_active || event.repeat)              // DirectInput reports transitions only
        return;
    const unsigned char dik = event.dik;
    if (dik == 0)
        return;
    if (m_pending.size() >= kKeyboardBufferSize) {
        m_overflow = true;                      // DI_BUFFEROVERFLOW on the next poll
        return;
    }
    NativeInputEvent record;
    record.offset = dik;
    record.data = event.down ? 0x80u : 0u;
    record.timeStamp = event.timeMs;
    record.sequence = nextNativeInputSequence();
    m_pending.push_back(record);
}

void NativeKeyboardDevice::onFocusChanged(bool windowKey, bool appActive)
{
    if (!(windowKey && appActive) && m_active)
        m_inputLost = true;                     // DIERR_INPUTLOST on the next poll
}

void NativeKeyboardDevice::updateCurrentState(float dt)
{
    std::memcpy(m_previousKeys, m_keys, sizeof(m_keys));
    unsigned int firstTimestamp = 0, boundary = 0;
    while (!m_events.empty()) {
        const NativeInputEvent& event = m_events.front();
        if (!firstTimestamp) {
            firstTimestamp = event.timeStamp;
            boundary = firstTimestamp - static_cast<int>(static_cast<double>(dt) * -1000.0);
        }
        if (event.timeStamp >= boundary) break;
        m_keys[event.offset] = static_cast<unsigned char>((event.data >> 7) & 1);
        m_events.pop_front();
    }
}

void NativeKeyboardDevice::update(float dt)
{
    if (!m_active || !m_initialized) return;
    if (dt > 0.0f) {
        updateCurrentState(dt);
        return;
    }
    // Poll: the GetDeviceData counterpart.
    if (m_inputLost) {
        m_events.clear();
        m_pending.clear();
        std::memset(m_keys, 0, sizeof(m_keys));
        if (!clientHost().hasFocus()) return;         // reacquire fails without the foreground
        m_inputLost = false;
    }
    if (m_overflow) {
        m_events.clear();
        m_pending.clear();
        m_overflow = false;
        std::memset(m_keys, 0, sizeof(m_keys));
        return;
    }
    for (size_t i = 0; i < m_pending.size(); ++i)
        if (m_pending[i].offset < 255)
            m_events.push_back(m_pending[i]);
    m_pending.clear();
}

} } }
#endif
