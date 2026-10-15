// [INFERRED] local ABI header; no Novint implementation is reconstructed here.
// Narrow declarations from BF2.exe's NovintHFX imports and its call-site layouts.
// See IO/Input/FALCON_DEVICE_STATUS.md. The matching stock DLL supplies all bodies.
#ifndef EXTERNAL_NOVINTHFX_ABI_H
#define EXTERNAL_NOVINTHFX_ABI_H
#include <windows.h>
namespace NovintHFX {
struct ApplicationData {
    struct mouse_settings {
        __declspec(dllimport) mouse_settings();
    private:
        // Opaque SDK state, 28-byte stack extent at BF2.exe 0x6A4EF0.
        // Its own SDK constructor owns every field; no fabricated initialization.
        alignas(4) unsigned char m_sdkState[28];
    };
    HWND window; // [INFERRED] name; HWND at byte 0, 0x6A4F23.
    mouse_settings mouse;
};
class Device {
public:
    __declspec(dllimport) void InputUpdate();
    __declspec(dllimport) void GetButtonData(int*, int*, int*) const;
    __declspec(dllimport) void GetToolPosition(double* const) const;
private:
    Device(); // Opaque DLL-owned instance; never allocate from this declaration.
};
class HapticsSystem {
public:
    __declspec(dllimport) bool AttachApplication(ApplicationData&);
    __declspec(dllimport) void RecountConnectedDevices();
    __declspec(dllimport) int ConnectedDeviceCount() const;
    __declspec(dllimport) bool RunDevice(Device**, int);
    __declspec(dllimport) void StartMouse(Device*);
    __declspec(dllimport) bool StopRunningDevice(Device**);
    __declspec(dllimport) int InputUpdate();
private:
    HapticsSystem(); // Opaque DLL-owned instance; never allocate or copy.
};
}
// PE IAT 0x87F788: a single IAT load supplies this, i.e. the exported object's
// address. This is neither a pointer variable nor a callable Haptics() function.
extern "C" __declspec(dllimport) NovintHFX::HapticsSystem Haptics;
static_assert(sizeof(NovintHFX::ApplicationData::mouse_settings) == 28, "SDK ABI");
#if defined(_M_IX86)
static_assert(sizeof(NovintHFX::ApplicationData) == 32, "x86 SDK ABI");
#endif
// [CHOSEN, not recovered] x64 (https://github.com/B2BF/OpenBF2/issues/7): the SDK exists only as a
// 32-bit DLL, so an x64 image imports nothing from it; FalconDevice reports no device there.
#endif
