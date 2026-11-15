// [TEMP] Bounded Falcon test. Stock Novint DLL imports; no replacement device bodies.
#include <list>
#include <string>
#include <vector>
#include <windows.h>
#define private public
#include "../BF2/IO/Input/FalconDevice.h"
#undef private
#include "../BF2/Core/Ids.h"
#include <cstdio>
#include <cstring>
#include <new>
#include <limits>
namespace dice { namespace hfe {
// The frame update's collaborators; this fixture never reaches it (no device, update returns first).
IBase* g_controlSettings = 0;
IBase* g_hapticSettings = 0;
namespace io { IBase* inputDeviceManager = 0; IBase* controlMapManager = 0; float inputSampleLatency = 0.0f; }
} }
using namespace dice::hfe;
using namespace dice::hfe::io;
int main()
{
    int checks = 0, failed = 0;
    auto check = [&](bool value, const char* name) {
        ++checks;
        if (!value) { ++failed; std::printf("FAIL %s\n", name); }
    };
    alignas(4) unsigned char applicationMemory[40];
    std::memset(applicationMemory, 0xA5, sizeof(applicationMemory));
    NovintHFX::ApplicationData* application = new(applicationMemory + 4) NovintHFX::ApplicationData;
    check(applicationMemory[3] == 0xA5 && applicationMemory[36] == 0xA5,
        "actual SDK mouse-settings constructor stays in caller extent");
    application->~ApplicationData();
    void* memory = ::operator new(sizeof(FalconDevice));
    std::memset(memory, 0xA5, sizeof(FalconDevice));
    FalconDevice* device = new(memory) FalconDevice;
    IAdvancedInputDevice* api = device;
    check(device->getRef() == 1, "initial reference");
    check(device->m_haptics == &Haptics && device->m_device == 0, "real imported object address");
    check(device->m_samples.empty(), "native sample list");
    check(device->m_buttons == 0 && device->m_previousButtons == 0, "button initialization");
    check(device->m_pollTime == 0 && device->m_sampleTime == 0xA5A5A5A5, "timestamp untouched bytes");
    check(device->m_numButtons == 0xA5A5A5A5 && device->m_numAxes == static_cast<int>(0xA5A5A5A5), "counts await init");
    check(*reinterpret_cast<unsigned int*>(&device->m_buttonLatency[0]) == 0xA5A5A5A5, "latency untouched");
    check(api->getType() == IDTFalcon, "Falcon type");
    check(!api->isActive() && !api->activate() && !api->deActivate(), "no-device activation");
    check(!device->init(), "real missing-manager init guard");
    check(device->queryInterface(IID_IBase) == device && device->queryInterface(IID_IInputDevice) == device
        && device->queryInterface(IID_IAdvancedInputDevice) == device, "interface IDs");
    check(device->queryInterface(0) == 0 && device->getRef() == 1, "query no ref change");
    check(api->setCooperativeMode(static_cast<InputDeviceMode>(9))
        && api->getCooperativeMode() == static_cast<InputDeviceMode>(9), "mode accepts arbitrary value");
    // [TEMP] Model only the post-init counts / sampled scalar state, without inventing a SDK device.
    device->m_numAxes = 3;
    device->m_numButtons = 4;
    check(api->getNumAxes() == 3 && api->getNumButtons() == 4, "count getters");
    for (int i = 0; i < 3; ++i) {
        const auto axis = static_cast<InputDeviceAxes>(i);
        check(device->m_axes[i] == 0 && device->m_axisTravel[i] == 0
            && api->getAxisScale(axis) == 1 && !api->getInvertAxis(axis), "axis initial state");
        device->m_axes[i] = 0.125 * (i + 1);
        api->setAxisScale(axis, 7.0f);
        api->setInvertAxis(axis, true);
        check(api->getAxisScale(axis) == 7.0f && api->getInvertAxis(axis), "axis configuration");
        check(api->readAxis(axis, true) == -0.125f * (i + 1)
            && api->readAxis(axis, false) == -0.125f * (i + 1), "readAxis ignores scale flag");
    }
    check(!api->getInvertAxis(static_cast<InputDeviceAxes>(4))
        && api->getAxisScale(static_cast<InputDeviceAxes>(4)) == 0, "greater-than-count rejection");
    api->setAxisAdd(IDAxis_0, 12);
    check(api->getAxisAdd(IDAxis_0) == 0, "recovered axis-add no-op");
    float x = 7, y = 9;
    api->peakXYAxis(x, y);
    check(x == 0 && y == 0, "inherited zero peak");
    device->m_buttons = 0x80000005u;
    device->m_previousButtons = 6;
    check(api->buttonDown(static_cast<InputDeviceButtons>(1)) && !api->buttonDown(static_cast<InputDeviceButtons>(2)), "button mask down");
    check(api->buttonPressed(static_cast<InputDeviceButtons>(1)) && !api->buttonPressed(static_cast<InputDeviceButtons>(4)), "button edge press");
    check(api->buttonReleased(static_cast<InputDeviceButtons>(2)) && !api->buttonReleased(static_cast<InputDeviceButtons>(4)), "button edge release");
    device->m_buttonLatency[0] = 0.25f;
    device->m_buttonLatency[31] = 0.75f;
    check(api->buttonPressedSampleLatency(static_cast<InputDeviceButtons>(0x80000001u)) == 0.25f, "lowest latency bit wins");
    check(api->buttonPressedSampleLatency(static_cast<InputDeviceButtons>(0x80000000u)) == 0.75f, "latency high bit");
    check(api->buttonPressedSampleLatency(static_cast<InputDeviceButtons>(0)) == 0, "zero latency mask");
    InputDeviceAxes axis = IDAxis_2;
    InputDeviceButtons button = static_cast<InputDeviceButtons>(7);
    InputDeviceKeys key = static_cast<InputDeviceKeys>(8);
    bool negative = true;
    check(!api->getNextInput(axis, button, key, negative) && axis == IDAxis_2 && button == 7 && key == 8 && negative,
        "inactive mapping leaves outputs untouched");
    device->m_active = true;
    check(!api->isActive() && !api->getNextInput(axis, button, key, negative), "null device second guard");
    api->update(0);
    api->update(1);
    api->update(std::numeric_limits<float>::quiet_NaN());
    check(device->m_samples.empty(), "no-device polling and frame guards");
    api->addRef();
    check(api->getRef() == 2 && api->release() == 1, "reference increment/decrement");
    check(api->release() == 0, "real final release and destructor");
    std::printf("falcon device runtime %d/%d checks passed (native size %u)\n", checks - failed, checks,
        unsigned(sizeof(FalconDevice)));
    std::fflush(stdout);
    return failed != 0 ? 1 : 0;
}
