// [INFERRED] local ABI header; no Novint implementation is reconstructed here.
// Narrow declarations from BF2.exe's NovintHFX imports and its call-site layouts.
// See IO/Input/FALCON_DEVICE_STATUS.md. The matching stock DLL supplies all bodies.
#ifndef EXTERNAL_NOVINTHFX_ABI_H
#define EXTERNAL_NOVINTHFX_ABI_H
#include <windows.h>
#include <cstddef>
// [CHOSEN, not recovered] Test seam: the Mac fake-SDK check (Code/Tests/falcon_update_runtime.cpp)
// builds with BF2_NOVINT_FAKE_SDK and supplies every body; product images always import.
#if defined(BF2_NOVINT_FAKE_SDK)
#define NOVINTHFX_IMPORT
#else
#define NOVINTHFX_IMPORT __declspec(dllimport)
#endif
namespace NovintHFX {
class Stack;
class Device;
// Three doubles (24 bytes: BF2.exe 0x4A34BA builds one at ebp-0x54, the next local at ebp-0x3C).
// Only SDK members touch the components. [UNRESOLVED] member name. A struct:
// the SDK's own Vect3 imports mangle it U (??4Vect3@NovintHFX@@QAEAAU01@ABU01@@Z).
struct Vect3 {
    NOVINTHFX_IMPORT Vect3(const double&, const double&, const double&);
    NOVINTHFX_IMPORT double& operator[](int);
    NOVINTHFX_IMPORT Vect3& operator=(const Vect3&);
    NOVINTHFX_IMPORT Vect3 operator*(const double&) const;
    NOVINTHFX_IMPORT Vect3 Normalize() const;
private:
    double m_unresolved[3];
};
}
class IHapticEffect;

// Effect parameter interface, global namespace (CreateNewEffect's PAUIHapticEffectParam@@).
// Slot order from BF2.exe's local ControlBoxEffectParams vtable 0x9116E0. MSVC groups overloads
// of one name at the first one's slot in reverse declaration order, so each group below is
// declared last slot first.
struct IHapticEffectParam {
    virtual bool RunSetCommand(char*) = 0;                                   // +0x00
    virtual const char* StandardEffectName() const = 0;                      // +0x04
    virtual bool SetVar(const char*, double) = 0;                            // +0x2C
    virtual bool SetVar(const char*, bool) = 0;                              // +0x28
    virtual bool SetVar(const char*, float) = 0;                             // +0x24
    virtual bool SetVar(const char*, int) = 0;                               // +0x20
    virtual bool SetVar(const char*, void*) = 0;                             // +0x1C
    virtual bool SetVar(const char*, int, double) = 0;                       // +0x18
    virtual bool SetVar(const char*, int, bool) = 0;                         // +0x14
    virtual bool SetVar(const char*, int, float) = 0;                        // +0x10
    virtual bool SetVar(const char*, int, int) = 0;                          // +0x0C
    virtual bool SetVar(const char*, int, void*) = 0;                        // +0x08
    virtual bool GetVar(const char*, float&) = 0;                            // +0x54
    virtual bool GetVar(const char*, double&) = 0;                           // +0x50
    virtual bool GetVar(const char*, bool&) = 0;                             // +0x4C
    virtual bool GetVar(const char*, int&) = 0;                              // +0x48
    virtual bool GetVar(const char*, void*) = 0;                             // +0x44
    virtual bool GetVar(const char*, int, float&) = 0;                       // +0x40
    virtual bool GetVar(const char*, int, double&) = 0;                      // +0x3C
    virtual bool GetVar(const char*, int, bool&) = 0;                        // +0x38
    virtual bool GetVar(const char*, int, int&) = 0;                         // +0x34
    virtual bool GetVar(const char*, int, void*) = 0;                        // +0x30
    // +0x58..+0x94: header-inline typed forwarders (BF2.exe 0x5A42D0..0x5A43B0, 0x5D1600,
    // 0x626BF0, 0x83C790), each a tail call of the slot named. [INFERRED] names: the bodies show
    // only their targets.
    virtual bool SetFloatVar(const char* n, float v) { return SetVar(n, v); }              // +0x5C -> +0x24
    virtual bool SetFloatVar(const char* n, int i, float v) { return SetVar(n, i, v); }    // +0x58 -> +0x10
    virtual bool SetDoubleVar(const char* n, double v) { return SetVar(n, v); }            // +0x64 -> +0x2C
    virtual bool SetDoubleVar(const char* n, int i, double v) { return SetVar(n, i, v); }  // +0x60 -> +0x18
    virtual bool SetIntVar(const char* n, int v) { return SetVar(n, v); }                  // +0x6C -> +0x20
    virtual bool SetIntVar(const char* n, int i, int v) { return SetVar(n, i, v); }        // +0x68 -> +0x0C
    virtual bool SetBoolVar(const char* n, bool v) { return SetVar(n, v); }                // +0x74 -> +0x28
    virtual bool SetBoolVar(const char* n, int i, bool v) { return SetVar(n, i, v); }      // +0x70 -> +0x14
    virtual bool GetFloatVar(const char* n, float& v) { return GetVar(n, v); }             // +0x7C -> +0x54
    virtual bool GetFloatVar(const char* n, int i, float& v) { return GetVar(n, i, v); }   // +0x78 -> +0x40
    virtual bool GetDoubleVar(const char* n, double& v) { return GetVar(n, v); }           // +0x84 -> +0x50
    virtual bool GetDoubleVar(const char* n, int i, double& v) { return GetVar(n, i, v); } // +0x80 -> +0x3C
    virtual bool GetIntVar(const char* n, int& v) { return GetVar(n, v); }                 // +0x8C -> +0x48
    virtual bool GetIntVar(const char* n, int i, int& v) { return GetVar(n, i, v); }       // +0x88 -> +0x34
    virtual bool GetBoolVar(const char* n, bool& v) { return GetVar(n, v); }               // +0x94 -> +0x4C
    virtual bool GetBoolVar(const char* n, int i, bool& v) { return GetVar(n, i, v); }     // +0x90 -> +0x38
};

// A running effect, owned by the device's effect stack (global namespace, PAPAVIHapticEffect@@).
// [UNRESOLVED] Only what BF2.exe 0x6A5835/0x6A5995/0x6A599D uses is declared; the haptic-effect
// sites (ledger N2) name the remaining slots.
class IHapticEffect {
public:
    virtual void unresolvedSlot00() = 0;
    virtual void unresolvedSlot04() = 0;
    virtual void unresolvedSlot08() = 0;
    virtual void unresolvedSlot0C() = 0;
    virtual void unresolvedSlot10() = 0;
    virtual void unresolvedSlot14() = 0;
    virtual void unresolvedSlot18() = 0;
    virtual void unresolvedSlot1C() = 0;
    virtual void unresolvedSlot20() = 0;
    virtual void unresolvedSlot24() = 0;
    virtual void unresolvedSlot28() = 0;
    virtual void unresolvedSlot2C() = 0;
    virtual void unresolvedSlot30() = 0;
    virtual void unresolvedSlot34() = 0;
    virtual void unresolvedSlot38() = 0;
    virtual void unresolvedSlot3C() = 0;
    virtual void unresolvedSlot40() = 0;
    virtual bool UpdateParams(IHapticEffectParam*) = 0;  // +0x44 [INFERRED] name and result
    virtual void unresolvedSlot48() = 0;
    virtual void SetForce(const NovintHFX::Vect3&) = 0;  // +0x4C [INFERRED] name and result; 0x4A34C3
    virtual void unresolvedSlot50() = 0;
    // +0x54 [INFERRED] name and result: Soldier's CraftPhysics effects pass HapticSettings'
    // PhysicsSOLDIERSmoothTime* here every tick (0x5AF322), the value also given as params +0x18.
    virtual void SetSmoothTime(int) = 0;
    unsigned char m_unresolved04[8];
    unsigned char m_stateFlags;  // +0x0C; bit 0 [INFERRED] suspended, read and written inline
protected:
    IHapticEffect();  // DLL-owned; created only by Stack::CreateNewEffect
};

namespace NovintHFX {
struct ParamsStaticClass;  // [UNRESOLVED] opaque; returned by GetStaticInfoClass

// Shared effect-parameter base. Its own virtuals follow the interface's (+0x98..+0xAC).
class BaseEffectParams : public IHapticEffectParam {
public:
    NOVINTHFX_IMPORT bool RunSetCommand(char*) override;
    NOVINTHFX_IMPORT virtual ~BaseEffectParams();                                    // +0x98
    NOVINTHFX_IMPORT virtual bool SetVar(const char*, const char*);                 // +0x9C
    using IHapticEffectParam::SetVar;
    virtual char GetVarType(const char*, int*) const = 0;                            // +0xA0
    virtual ParamsStaticClass* GetStaticInfoClass() = 0;                             // +0xA4
    NOVINTHFX_IMPORT virtual bool CreateStandardEffect(Device*, const char*, IHapticEffect**);  // +0xAC
    NOVINTHFX_IMPORT virtual bool CreateStandardEffect(Stack*, const char*, IHapticEffect**);   // +0xA8
protected:
    BaseEffectParams();  // reached only through the derived SDK constructors
private:
    alignas(4) unsigned char m_baseState[20];  // +0x04 [UNRESOLVED] SDK-owned
};

class ConstantEffectParams : public BaseEffectParams {
public:
    // [INFERRED] throw(): BF2.exe's new-expression at 0x5A416E has no unwind state around the
    // constructor call, so the compiler treated the construction as non-throwing.
    NOVINTHFX_IMPORT ConstantEffectParams() throw();
    NOVINTHFX_IMPORT const char* StandardEffectName() const override;
    NOVINTHFX_IMPORT bool SetVar(const char*, double) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, bool) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, float) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, void*) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, double) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, bool) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, float) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, int) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, void*) override;
    using BaseEffectParams::SetVar;
    NOVINTHFX_IMPORT bool GetVar(const char*, float&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, double&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, bool&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, void*) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, float&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, double&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, bool&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, int&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, void*) override;
    NOVINTHFX_IMPORT char GetVarType(const char*, int*) const override;
    NOVINTHFX_IMPORT ParamsStaticClass* GetStaticInfoClass() override;
    int m_unresolved18;  // +0x18 [UNRESOLVED] meaning; BF2.exe 0x5A418F writes 0x14 for "ShakeForce"
    int m_unresolved1C;  // +0x1C [UNRESOLVED]
};

class EnvelopeEffectParams : public BaseEffectParams {
public:
    // [INFERRED] throw(): BF2.exe's new-expression at 0x5E65B4 has no unwind state around the
    // constructor call, so the compiler treated the construction as non-throwing.
    NOVINTHFX_IMPORT EnvelopeEffectParams() throw();
    NOVINTHFX_IMPORT const char* StandardEffectName() const override;
    NOVINTHFX_IMPORT bool SetVar(const char*, double) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, bool) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, float) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, void*) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, double) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, bool) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, float) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, int) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, void*) override;
    using BaseEffectParams::SetVar;
    NOVINTHFX_IMPORT bool GetVar(const char*, float&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, double&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, bool&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, void*) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, float&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, double&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, bool&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, int&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, void*) override;
    NOVINTHFX_IMPORT char GetVarType(const char*, int*) const override;
    NOVINTHFX_IMPORT ParamsStaticClass* GetStaticInfoClass() override;
    int m_unresolved18;  // +0x18 [UNRESOLVED] meaning; BF2.exe 0x5E65C4 writes 0x64 for "Damage"
    alignas(4) unsigned char m_unresolved1C[12];  // +0x1C [UNRESOLVED]
    double m_force[3];  // +0x28 [INFERRED] name; BF2.exe 0x5E665C..0x5E667A write the scaled direction
};

// The control box BF2 builds in FalconDevice::updateCurrentState (BF2.exe 0x6A587F): 0x120
// bytes, SDK constructor, every virtual imported except the interface's inline forwarders.
class ControlBoxEffectParams : public BaseEffectParams {
public:
    // [INFERRED] throw(): BF2.exe's new-expression at 0x6A587F has no unwind state around the
    // constructor call, so the compiler treated the construction as non-throwing.
    NOVINTHFX_IMPORT ControlBoxEffectParams() throw();
    NOVINTHFX_IMPORT const char* StandardEffectName() const override;
    NOVINTHFX_IMPORT bool SetVar(const char*, double) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, bool) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, float) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, void*) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, double) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, bool) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, float) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, int) override;
    NOVINTHFX_IMPORT bool SetVar(const char*, int, void*) override;
    using BaseEffectParams::SetVar;
    NOVINTHFX_IMPORT bool GetVar(const char*, float&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, double&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, bool&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, void*) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, float&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, double&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, bool&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, int&) override;
    NOVINTHFX_IMPORT bool GetVar(const char*, int, void*) override;
    NOVINTHFX_IMPORT char GetVarType(const char*, int*) const override;
    NOVINTHFX_IMPORT ParamsStaticClass* GetStaticInfoClass() override;
    // Written inline by BF2.exe 0x6A58E4..0x6A593E. [INFERRED] names: the lower and upper box
    // walls and the spring stiffness on each side, per axis.
    double m_lower[3];            // +0x18
    double m_upper[3];            // +0x30
    double m_unresolved48[6];     // +0x48 [UNRESOLVED]
    double m_lowerStiffness[3];   // +0x78
    double m_upperStiffness[3];   // +0x90
    alignas(4) unsigned char m_unresolvedA8[0x6C];  // +0xA8 [UNRESOLVED]
    int m_unresolved114;          // +0x114; BF2 writes 3000 (ds:0x979010) [UNRESOLVED] meaning
    alignas(4) unsigned char m_unresolved118[8];    // +0x118 [UNRESOLVED]
};
}

namespace NovintHFX {
struct ApplicationData {
    struct mouse_settings {
        NOVINTHFX_IMPORT mouse_settings();
    private:
        // Opaque SDK state, 28-byte stack extent at BF2.exe 0x6A4EF0.
        // Its own SDK constructor owns every field; no fabricated initialization.
        alignas(4) unsigned char m_sdkState[28];
    };
    HWND window; // [INFERRED] name; HWND at byte 0, 0x6A4F23.
    mouse_settings mouse;
};
class Stack {
public:
    NOVINTHFX_IMPORT bool CreateNewEffect(const char*, IHapticEffectParam*, const char*, IHapticEffect**);
#if defined(BF2_NOVINT_FAKE_SDK)
public: // [CHOSEN, not recovered] test seam: the fake SDK owns its instances
#else
private:
#endif
    Stack(); // Opaque DLL-owned instance.
};
class Device {
public:
    NOVINTHFX_IMPORT void InputUpdate();
    NOVINTHFX_IMPORT void GetButtonData(int*, int*, int*) const;
    NOVINTHFX_IMPORT void GetToolPosition(double* const) const;
    NOVINTHFX_IMPORT Stack* GetEffectStack();
#if defined(BF2_NOVINT_FAKE_SDK)
public: // [CHOSEN, not recovered] test seam: the fake SDK owns its instances
#else
private:
#endif
    Device(); // Opaque DLL-owned instance; never allocate from this declaration.
};
class HapticsSystem {
public:
    NOVINTHFX_IMPORT bool AttachApplication(ApplicationData&);
    NOVINTHFX_IMPORT void RecountConnectedDevices();
    NOVINTHFX_IMPORT int ConnectedDeviceCount() const;
    NOVINTHFX_IMPORT bool RunDevice(Device**, int);
    NOVINTHFX_IMPORT void StartMouse(Device*);
    NOVINTHFX_IMPORT bool StopRunningDevice(Device**);
    NOVINTHFX_IMPORT int InputUpdate();
#if defined(BF2_NOVINT_FAKE_SDK)
public: // [CHOSEN, not recovered] test seam: the fake SDK defines Haptics
#else
private:
#endif
    HapticsSystem(); // Opaque DLL-owned instance; never allocate or copy.
};
}
// PE IAT 0x87F788: a single IAT load supplies this, i.e. the exported object's
// address. This is neither a pointer variable nor a callable Haptics() function.
extern "C" NOVINTHFX_IMPORT NovintHFX::HapticsSystem Haptics;
static_assert(sizeof(NovintHFX::ApplicationData::mouse_settings) == 28, "SDK ABI");
#if defined(_M_IX86)
static_assert(sizeof(NovintHFX::ApplicationData) == 32, "x86 SDK ABI");
static_assert(sizeof(NovintHFX::ControlBoxEffectParams) == 0x120, "BF2.exe 0x6A587F allocation");
static_assert(offsetof(NovintHFX::ControlBoxEffectParams, m_lower) == 0x18, "x86 SDK ABI");
static_assert(offsetof(NovintHFX::ControlBoxEffectParams, m_upper) == 0x30, "x86 SDK ABI");
static_assert(offsetof(NovintHFX::ControlBoxEffectParams, m_lowerStiffness) == 0x78, "x86 SDK ABI");
static_assert(offsetof(NovintHFX::ControlBoxEffectParams, m_upperStiffness) == 0x90, "x86 SDK ABI");
static_assert(offsetof(NovintHFX::ControlBoxEffectParams, m_unresolved114) == 0x114, "x86 SDK ABI");
static_assert(offsetof(IHapticEffect, m_stateFlags) == 0x0C, "x86 SDK ABI");
static_assert(sizeof(NovintHFX::Vect3) == 24, "BF2.exe 0x4A34BA stack extent");
#endif
// [CHOSEN, not recovered] x64 (https://github.com/B2BF/OpenBF2/issues/7): the SDK exists only as a
// 32-bit DLL, so an x64 image imports nothing from it; FalconDevice reports no device there.
#endif
