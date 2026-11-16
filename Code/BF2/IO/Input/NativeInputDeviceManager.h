// [CHOSEN, not recovered] Native client input manager and devices shared by Cocoa and SDL.
// They replace DX8InputDeviceManager and its DirectInput devices while keeping their interfaces,
// sampling algorithms and quirks (buffered events consumed by timestamp, axis scaling through the active control map,
// DIK_ scan codes, button numbering), with events supplied by Platform/Host/ClientHost.h.
// Native host contracts: docs/linux-vulkan-client.md and docs/macos-s2-4.md (issue #36).
//
// The manager is registered under CID_DX8InputDeviceManager and the registry name
// "dice.hfe.io.InputDeviceManager.DX8", so BF2EngineSetup's singleton selection and
// every engine caller stay unchanged.

#ifndef IO_INPUT_NATIVEINPUTDEVICEMANAGER_H
#define IO_INPUT_NATIVEINPUTDEVICEMANAGER_H

#include "../../Core/BuildConfig.h"
#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))

#include "Input.h"
#include "../../Platform/Host/ClientHost.h"
#include <list>
#include <map>
#include <string>
#include <vector>

namespace dice { namespace hfe { namespace io {

// One buffered device event, the native counterpart of DIDEVICEOBJECTDATA: offset is the
// DirectInput data-format offset (DIK_ code, DIMOFS_*), data the DirectInput value (0x80 for a
// press, a signed count for an axis), timeStamp milliseconds on ClientHost::tickCountMs's clock,
// sequence a process-wide increasing number.
struct NativeInputEvent
{
    unsigned int offset;
    unsigned int data;
    unsigned int timeStamp;
    unsigned int sequence;
};

// Returns the next event sequence number (shared by all devices, as DirectInput's is).
unsigned int nextNativeInputSequence();

// Keyboard device: DIK_ key states sampled from host key events. Buffers up to 100 events between
// polls, like the DirectInput keyboard's DIPROP_BUFFERSIZE.
class NativeKeyboardDevice final : public IKeyboardDevice, private host::IHostEventListener
{
public:
    NativeKeyboardDevice();
    ~NativeKeyboardDevice();
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    InputDeviceTypes getType() const override;
    bool setCooperativeMode(InputDeviceMode mode) override;
    InputDeviceMode getCooperativeMode() const override;
    bool activate() override;
    bool deActivate() override;
    void update(float dt) override;
    bool isActive() const override;
    bool getNextInput(InputDeviceAxes&, InputDeviceButtons&, InputDeviceKeys&, bool&) override;
    bool keyDown(InputDeviceKeys key) const override;
    bool keyPressed(InputDeviceKeys key) const override;
    bool keyReleased(InputDeviceKeys key) const override;
    std::wstring getKeyName(InputDeviceKeys key) const override;
    virtual bool init();

private:
    void onKey(const host::HostKeyEvent& event) override;
    void onFocusChanged(bool windowKey, bool appActive) override;
    void updateCurrentState(float dt);

    int m_refCount;
    InputDeviceMode m_mode;
    bool m_initialized;                         // stands for DX8's m_device
    bool m_active;
    bool m_inputLost;                           // focus left the window since the last poll
    bool m_overflow;
    std::vector<NativeInputEvent> m_pending;       // the DirectInput device buffer
    std::list<NativeInputEvent> m_events;
    unsigned char m_keys[256], m_previousKeys[256];
};

// Mouse device: relative X/Y/wheel and eight buttons from host mouse events, in DirectInput's
// c_dfDIMouse2 layout. Buffers up to 128 events between polls.
class NativeMouseDevice final : public IAdvancedInputDevice, private host::IHostEventListener
{
public:
    NativeMouseDevice();
    ~NativeMouseDevice();
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    InputDeviceTypes getType() const override;
    bool setCooperativeMode(InputDeviceMode mode) override;
    InputDeviceMode getCooperativeMode() const override;
    bool activate() override;
    bool deActivate() override;
    void update(float deltaTime) override;
    bool isActive() const override;
    bool getNextInput(InputDeviceAxes&, InputDeviceButtons&, InputDeviceKeys&, bool&) override;
    int getNumAxes() const override;
    float readAxis(InputDeviceAxes axis, bool) const override;
    void peakXYAxis(float& x, float& y) override;
    void setInvertAxis(InputDeviceAxes axis, bool invert) override;
    bool getInvertAxis(InputDeviceAxes axis) const override;
    void setAxisScale(InputDeviceAxes axis, float scale) override;
    float getAxisScale(InputDeviceAxes axis) const override;
    void setAxisAdd(InputDeviceAxes axis, float add) override;
    float getAxisAdd(InputDeviceAxes axis) const override;
    int getNumButtons() const override;
    bool buttonDown(InputDeviceButtons button) const override;
    bool buttonPressed(InputDeviceButtons button) const override;
    bool buttonReleased(InputDeviceButtons button) const override;
    float buttonPressedSampleLatency(InputDeviceButtons button) const override;
    virtual bool init();

private:
    void onMouseMove(const host::HostMouseMoveEvent& event) override;
    void onRawMouseMove(const host::HostRawMouseEvent& event) override;
    void addMotion(double dx, double dy, unsigned int timeMs);
    void onMouseButton(const host::HostMouseButtonEvent& event) override;
    void onMouseWheel(const host::HostMouseWheelEvent& event) override;
    void onFocusChanged(bool windowKey, bool appActive) override;
    void push(unsigned int offset, unsigned int data, unsigned int timeStamp);
    void clearState();
    void applyRelativeMode();
    void updateSamples(float deltaTime);

    int m_refCount;
    InputDeviceMode m_mode;
    unsigned int m_numButtons;
    unsigned int m_buttons, m_previousButtons;
    float m_axes[4];
    bool m_axisArmed[4];
    int m_numAxes;
    bool m_initialized;
    bool m_active;
    bool m_inputLost;
    bool m_overflow;
    float m_axisScale[4];
    bool m_invertAxis[4];
    float m_buttonLatency[32];
    unsigned int m_pollTime, m_sampleTime;
    double m_remainderX, m_remainderY;          // sub-count motion carried to the next event
    std::vector<NativeInputEvent> m_pending;
    std::list<NativeInputEvent> m_samples;
    std::list<NativeInputEvent> m_peakSamples;
    unsigned int m_lastSequence;
};

// Game controller: an extended gamepad (GCController) presented as the DirectInput joystick an
// Xbox controller reports: axes X, Y, Z (triggers), Rx, Ry; ten buttons; one POV hat whose four
// cardinal directions also appear as buttons 10..13.
class NativeGameControllerDevice final : public IAdvancedInputDevice
{
public:
    NativeGameControllerDevice();
    ~NativeGameControllerDevice();
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    InputDeviceTypes getType() const override;
    bool setCooperativeMode(InputDeviceMode mode) override;
    InputDeviceMode getCooperativeMode() const override;
    bool activate() override;
    bool deActivate() override;
    void update(float deltaTime) override;
    bool isActive() const override;
    bool getNextInput(InputDeviceAxes&, InputDeviceButtons&, InputDeviceKeys&, bool&) override;
    int getNumAxes() const override;
    float readAxis(InputDeviceAxes, bool) const override;
    void peakXYAxis(float&, float&) override;
    void setInvertAxis(InputDeviceAxes, bool) override;
    bool getInvertAxis(InputDeviceAxes) const override;
    void setAxisScale(InputDeviceAxes, float) override;
    float getAxisScale(InputDeviceAxes) const override;
    void setAxisAdd(InputDeviceAxes, float) override;
    float getAxisAdd(InputDeviceAxes) const override;
    int getNumButtons() const override;
    bool buttonDown(InputDeviceButtons) const override;
    bool buttonPressed(InputDeviceButtons) const override;
    bool buttonReleased(InputDeviceButtons) const override;
    float buttonPressedSampleLatency(InputDeviceButtons) const override;
    virtual int getNumPOVs() const;
    virtual int readPOV(InputDevicePOVs) const;
    virtual bool slot_0x70() const;
    virtual bool init(int index);
    virtual int getDeviceIndex() const;

private:
    int m_refCount;
    InputDeviceMode m_mode;
    unsigned int m_numButtons;
    unsigned int m_buttons, m_previousButtons;
    float m_axes[8];
    int m_numAxes;
    bool m_bound;                               // stands for DX8's m_device
    const void* m_identity;
    float m_axisScale[8];
    float m_axisAdd[8];
    bool m_invertAxis[8];
    bool m_axisReady[8];
    int m_povs[4];
    int m_deviceIndex;
    int m_numPOVs;
    bool m_active;
    bool m_povsAsButtons;
};

// Device manager. initDevices opens the keyboard, mouse and the first two game controllers;
// the Falcon is not available on these native backends and is reported as not opened.
class NativeInputDeviceManager final : public IInputDeviceManager
{
public:
    NativeInputDeviceManager();
    ~NativeInputDeviceManager();
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    bool initDevices(unsigned int* deviceMask) override;
    IInputDevice* getInputDevice(InputDeviceFlags flags) const override;
    unsigned int getInputDevices() const override;
    bool activateDevices(unsigned int mask) override;
    bool deActivateDevices(unsigned int mask) override;
    void update(float dt) override;
    unsigned int getActiveInputDevices() const override;
    void* getAllDevices() override;
    static IBase* create(unsigned int iid, IBase* outer);
private:
    int m_refCount;
    std::map<InputDeviceFlags, IInputDevice*> m_devices;
    unsigned int m_activeDevices;
};

} } }

#endif // !DEDICATED && __APPLE__
#endif // IO_INPUT_NATIVEINPUTDEVICEMANAGER_H
