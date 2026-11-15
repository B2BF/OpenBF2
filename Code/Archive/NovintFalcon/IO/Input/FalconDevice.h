// Falcon input device. [INFERRED] Header placement. Preserve its 27-slot interface without a destructor slot.
// BF2_NOVINT_FAKE_SDK: the Mac fake-SDK check compiles the Windows class against fake SDK bodies.
#ifndef IO_INPUT_FALCONDEVICE_H
#define IO_INPUT_FALCONDEVICE_H
#include "../../Core/BuildConfig.h"
#if !DEDICATED
#include "FalconHaptics.h"   // g_falconControlBoxDirty and the haptic-effect helpers (ledger N2)
class IHapticEffect; // NovintHFX.h (global namespace); the haptic force effects (BF2ClientSeams.h)
#if !defined(_WIN32) && !defined(BF2_NOVINT_FAKE_SDK)
// [CHOSEN, not recovered] Native POSIX clients: the Novint HFX SDK is a Windows-only DLL and no
// Falcon is ever present, so neither the SDK header nor FalconDevice exists there; the device
// manager reports IDFFalcon as not opened (MacInputDeviceManager). Only the menu's mouse-mode
// global, which the menu and HUD code write on every platform, is declared. See
// docs/macos-port.md, "Bink, Novint and VoIP capture (S2.8)".
namespace dice { namespace hfe { namespace io {
extern int g_falconMouseMode; // [INFERRED] name, PE978FC4 initialized1; HUD/updateCurrentState writers
} } }
#else
#include "Input.h"
#include "../../External/NovintHFX/NovintHFX.h"
#include <list>
namespace dice { namespace hfe { namespace io {
// Type name attested in the ELF list specialization; fields from PE 0x6A5C90.
struct FalconInputFormat {
    unsigned int buttons;
    double position[3];
    DWORD time;
};
extern NovintHFX::Device* g_falconDevice; // [INFERRED] name, PE 0xA03940
extern int g_falconMouseMode; // [INFERRED] name, PE978FC4 initialized1; HUD/updateCurrentState writers
class FalconDevice : public IAdvancedInputDevice {
public:
    FalconDevice();
    ~FalconDevice(); // PE 0x6A50D0, nonvirtual
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int) const override;
    InputDeviceTypes getType() const override;
    bool setCooperativeMode(InputDeviceMode) override;
    InputDeviceMode getCooperativeMode() const override;
    bool activate() override;
    bool deActivate() override;
    void update(float) override;
    bool isActive() const override;
    bool getNextInput(InputDeviceAxes&, InputDeviceButtons&, InputDeviceKeys&, bool&) override;
    int getNumAxes() const override;
    float readAxis(InputDeviceAxes, bool) const override;
    // +38 retains the recovered IAdvancedInputDevice::peakXYAxis default.
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
    virtual bool init();
private:
    // PE 0x6A52A0, ELF 0x8499C80 (whose sample loop is compiled out); positive-dt frame update.
    void updateCurrentState(float);
    int m_refCount;                         // PE +04
    unsigned int m_buttons;                // +08
    InputDeviceMode m_mode;                 // +0C; ctor leaves untouched
    unsigned int m_numButtons;              // +10; assigned in init
    unsigned int m_previousButtons;        // +14
    double m_axes[3];                       // +18
    int m_axisTravel[3];                    // +30
    int m_numAxes;                          // +3C; assigned in init
    bool m_active;                          // +40
    float m_axisScale[3];                   // +44
    bool m_invertAxis[3];                   // +50
    float m_buttonLatency[32];              // +54; ctor leaves untouched
    DWORD m_pollTime, m_sampleTime;          // +D4/+D8; D8 untouched
    NovintHFX::HapticsSystem* m_haptics;      // +DC
    NovintHFX::Device* m_device;             // +E0
    std::list<FalconInputFormat> m_samples;  // +E4, native STL size
    DWORD m_field0f0, m_field0f4, m_field0f8;  // [UNRESOLVED] initialized zero
    bool m_field0fc;                        // [UNRESOLVED] initialized false
    DWORD m_field100;                       // [UNRESOLVED] initialized zero
};
} } }
#endif // _WIN32
#endif
#endif
