// [TEMP] [CHOSEN, not recovered] Mac fake-SDK check for FalconDevice::updateCurrentState
// (BF2.exe 0x6A52A0), built as bf2_falcon_update_check. The actual FalconDevice and HapticSettings
// run; the NovintHFX SDK, the Win32 clock, the active control map and ControlSettings are fakes
// that record what the device asks of them. No Falcon, DLL or Windows is involved. The Windows twin
// falcon_device_runtime.cmd links the stock NovintHFX.dll with no device connected.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <list>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>
#define private public
#include "../BF2/IO/Input/FalconDevice.h"
#undef private
#include "../BF2/Game/ControlSettings.h"
#include "../BF2/Game/HapticSettings.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"

using namespace dice::hfe;
using namespace dice::hfe::io;

static unsigned checks, failures;
static void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
static bool same(double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); }

[[noreturn]] static void unsupported(const char* name)
{
    std::fprintf(stderr, "[TEMP] falcon_update check: %s is not modelled\n", name);
    std::fflush(stderr);
    std::abort();
}

// ---- Win32 clock -------------------------------------------------------------------------------
static DWORD g_tick;
static int g_foregroundCalls;
extern "C" DWORD GetTickCount() { return g_tick; }
extern "C" HWND GetForegroundWindow() { ++g_foregroundCalls; return reinterpret_cast<HWND>(0x1234); }

// ---- fake NovintHFX SDK ------------------------------------------------------------------------
namespace fake {
struct Log { std::vector<std::string> calls; void add(const std::string& s) { calls.push_back(s); }
             std::string joined() const { std::string r; for (auto& c : calls) r += c + ";"; return r; }
             void clear() { calls.clear(); } };
Log sdk;
double toolPosition[3];
int buttonData;
int deviceCount = 1;
int liveParams;
struct Captured { double lower[3], upper[3], lowerStiffness[3], upperStiffness[3]; int value114; };
std::vector<Captured> updates;
const char* createType;
const char* createName;
}

NovintHFX::ApplicationData::mouse_settings::mouse_settings() { std::memset(m_sdkState, 0, sizeof(m_sdkState)); }
NovintHFX::HapticsSystem::HapticsSystem() {}
static NovintHFX::Device g_deviceObject;
static NovintHFX::Stack* g_stack;
extern "C" NovintHFX::HapticsSystem Haptics;
NovintHFX::HapticsSystem Haptics;
bool NovintHFX::HapticsSystem::AttachApplication(ApplicationData& a)
{ fake::sdk.add(a.window == reinterpret_cast<HWND>(0x1234) ? "attach foreground" : "attach other"); return true; }
void NovintHFX::HapticsSystem::RecountConnectedDevices() { fake::sdk.add("recount"); }
int NovintHFX::HapticsSystem::ConnectedDeviceCount() const { return fake::deviceCount; }
bool NovintHFX::HapticsSystem::RunDevice(Device** d, int index)
{ fake::sdk.add("run " + std::to_string(index)); *d = &g_deviceObject; return true; }
void NovintHFX::HapticsSystem::StartMouse(Device* d) { fake::sdk.add(d == &g_deviceObject ? "startMouse device" : "startMouse other"); }
bool NovintHFX::HapticsSystem::StopRunningDevice(Device** d) { fake::sdk.add("stop"); *d = 0; return true; }
int NovintHFX::HapticsSystem::InputUpdate() { fake::sdk.add("systemInput"); return 0; }
NovintHFX::Device::Device() {}
void NovintHFX::Device::InputUpdate() { fake::sdk.add("deviceInput"); }
void NovintHFX::Device::GetButtonData(int* down, int* pressed, int* released) const
{ fake::sdk.add("buttons"); *down = fake::buttonData; *pressed = 0; *released = 0; }
void NovintHFX::Device::GetToolPosition(double* const p) const
{ p[0] = fake::toolPosition[0]; p[1] = fake::toolPosition[1]; p[2] = fake::toolPosition[2]; }
NovintHFX::Stack* NovintHFX::Device::GetEffectStack() { fake::sdk.add("stack"); return g_stack; }
NovintHFX::Stack::Stack() {}

namespace fake { int vect3Constructions; }
NovintHFX::Vect3::Vect3(const double& x, const double& y, const double& z)
{ ++fake::vect3Constructions; m_unresolved[0] = x; m_unresolved[1] = y; m_unresolved[2] = z; }
double& NovintHFX::Vect3::operator[](int i) { return m_unresolved[i]; }
NovintHFX::Vect3& NovintHFX::Vect3::operator=(const Vect3& other)
{
    for (int i = 0; i < 3; ++i) m_unresolved[i] = other.m_unresolved[i];
    return *this;
}
NovintHFX::Vect3 NovintHFX::Vect3::operator*(const double& scale) const
{ return Vect3(m_unresolved[0] * scale, m_unresolved[1] * scale, m_unresolved[2] * scale); }
NovintHFX::Vect3 NovintHFX::Vect3::Normalize() const
{
    const double length = std::sqrt(m_unresolved[0] * m_unresolved[0]
        + m_unresolved[1] * m_unresolved[1] + m_unresolved[2] * m_unresolved[2]);
    return length == 0.0 ? Vect3(0.0, 0.0, 0.0) : *this * (1.0 / length);
}

namespace fake { double lastForce[3]; }
struct FakeEffect final : IHapticEffect {
    int id = 0;
    FakeEffect() { std::memset(m_unresolved04, 0, sizeof(m_unresolved04)); m_stateFlags = 0; }
    explicit FakeEffect(int effectId) : FakeEffect() { id = effectId; }
    void unresolvedSlot48() override { unsupported("effect slot 48"); }
    void SetForce(const NovintHFX::Vect3& force) override
    {
        NovintHFX::Vect3& v = const_cast<NovintHFX::Vect3&>(force);
        char line[96];
        std::snprintf(line, sizeof(line), "force %d %g %g %g", id, v[0], v[1], v[2]);
        fake::sdk.add(line);
        for (int i = 0; i < 3; ++i) fake::lastForce[i] = v[i];
    }
    void unresolvedSlot50() override { unsupported("effect slot 50"); }
    void SetSmoothTime(int value) override { fake::sdk.add("smooth " + std::to_string(id) + " " + std::to_string(value)); }
    void unresolvedSlot00() override { unsupported("effect slot 00"); }
    void unresolvedSlot04() override { unsupported("effect slot 04"); }
    void unresolvedSlot08() override { unsupported("effect slot 08"); }
    void unresolvedSlot0C() override { unsupported("effect slot 0C"); }
    void unresolvedSlot10() override { unsupported("effect slot 10"); }
    void unresolvedSlot14() override { unsupported("effect slot 14"); }
    void unresolvedSlot18() override { unsupported("effect slot 18"); }
    void unresolvedSlot1C() override { unsupported("effect slot 1C"); }
    void unresolvedSlot20() override { unsupported("effect slot 20"); }
    void unresolvedSlot24() override { unsupported("effect slot 24"); }
    void unresolvedSlot28() override { unsupported("effect slot 28"); }
    void unresolvedSlot2C() override { unsupported("effect slot 2C"); }
    void unresolvedSlot30() override { unsupported("effect slot 30"); }
    void unresolvedSlot34() override { unsupported("effect slot 34"); }
    void unresolvedSlot38() override { unsupported("effect slot 38"); }
    void unresolvedSlot3C() override { unsupported("effect slot 3C"); }
    void unresolvedSlot40() override { unsupported("effect slot 40"); }
    bool UpdateParams(IHapticEffectParam* p) override
    {
        auto* box = static_cast<NovintHFX::ControlBoxEffectParams*>(p);
        fake::Captured c;
        for (int i = 0; i < 3; ++i) {
            c.lower[i] = box->m_lower[i]; c.upper[i] = box->m_upper[i];
            c.lowerStiffness[i] = box->m_lowerStiffness[i]; c.upperStiffness[i] = box->m_upperStiffness[i];
        }
        c.value114 = box->m_unresolved114;
        fake::updates.push_back(c);
        fake::sdk.add(std::string("update flags=") + std::to_string(m_stateFlags & 1));
        return true;
    }
};
IHapticEffect::IHapticEffect() {}
static FakeEffect g_effect;
namespace fake { int constantEffects; int constantParam18 = -1; std::string params18; }
bool NovintHFX::Stack::CreateNewEffect(const char* type, IHapticEffectParam* params, const char* name, IHapticEffect** out)
{
    fake::createType = type; fake::createName = name;
    fake::sdk.add(std::string("create ") + type + " " + name);
    if (std::string(type) == "CONSTANT" || std::string(type) == "Constant") {
        // Each CONSTANT effect is its own object (ids 10, 11, ...); the params' +0x18 is recorded.
        static FakeEffect constants[8] = { FakeEffect(10), FakeEffect(11), FakeEffect(12), FakeEffect(13),
                                           FakeEffect(14), FakeEffect(15), FakeEffect(16), FakeEffect(17) };
        fake::constantParam18 = static_cast<NovintHFX::ConstantEffectParams*>(params)->m_unresolved18;
        fake::params18 += std::to_string(fake::constantParam18) + ",";
        *out = &constants[fake::constantEffects++ % 8];
        return true;
    }
    if (std::string(type) == "Envelope") {
        // Fire-and-forget: the out pointer is null; record what the effect would be built from.
        auto* envelope = static_cast<NovintHFX::EnvelopeEffectParams*>(params);
        char line[160];
        std::snprintf(line, sizeof(line), "envelope %d %.17g %.17g %.17g%s", envelope->m_unresolved18,
                      envelope->m_force[0], envelope->m_force[1], envelope->m_force[2], out ? " out" : "");
        fake::sdk.add(line);
        return true;
    }
    *out = &g_effect;
    return true;
}
NovintHFX::BaseEffectParams::BaseEffectParams() { std::memset(m_baseState, 0, sizeof(m_baseState)); ++fake::liveParams; }
NovintHFX::BaseEffectParams::~BaseEffectParams() { --fake::liveParams; fake::sdk.add("params destroyed"); }
bool NovintHFX::BaseEffectParams::RunSetCommand(char*) { unsupported("RunSetCommand"); }
bool NovintHFX::BaseEffectParams::SetVar(const char*, const char*) { unsupported("SetVar(cc,cc)"); }
bool NovintHFX::BaseEffectParams::CreateStandardEffect(Device*, const char*, IHapticEffect**) { unsupported("CreateStandardEffect"); }
bool NovintHFX::BaseEffectParams::CreateStandardEffect(Stack*, const char*, IHapticEffect**) { unsupported("CreateStandardEffect"); }
NovintHFX::ControlBoxEffectParams::ControlBoxEffectParams() throw()
{
    // Poison every field so the check sees exactly what updateCurrentState writes.
    for (int i = 0; i < 3; ++i) m_lower[i] = m_upper[i] = m_lowerStiffness[i] = m_upperStiffness[i] = -777.0;
    m_unresolved114 = -1;
    fake::sdk.add("params constructed");
}
const char* NovintHFX::ControlBoxEffectParams::StandardEffectName() const { return "ControlBox"; }
#define FAKE_VAR(sig) bool NovintHFX::ControlBoxEffectParams::sig { unsupported(#sig); }
FAKE_VAR(SetVar(const char*, double)) FAKE_VAR(SetVar(const char*, bool)) FAKE_VAR(SetVar(const char*, float))
FAKE_VAR(SetVar(const char*, int)) FAKE_VAR(SetVar(const char*, void*)) FAKE_VAR(SetVar(const char*, int, double))
FAKE_VAR(SetVar(const char*, int, bool)) FAKE_VAR(SetVar(const char*, int, float)) FAKE_VAR(SetVar(const char*, int, int))
FAKE_VAR(SetVar(const char*, int, void*)) FAKE_VAR(GetVar(const char*, float&)) FAKE_VAR(GetVar(const char*, double&))
FAKE_VAR(GetVar(const char*, bool&)) FAKE_VAR(GetVar(const char*, int&)) FAKE_VAR(GetVar(const char*, void*))
FAKE_VAR(GetVar(const char*, int, float&)) FAKE_VAR(GetVar(const char*, int, double&)) FAKE_VAR(GetVar(const char*, int, bool&))
FAKE_VAR(GetVar(const char*, int, int&)) FAKE_VAR(GetVar(const char*, int, void*))
#undef FAKE_VAR
char NovintHFX::ControlBoxEffectParams::GetVarType(const char*, int*) const { unsupported("GetVarType"); }
NovintHFX::ParamsStaticClass* NovintHFX::ControlBoxEffectParams::GetStaticInfoClass() { unsupported("GetStaticInfoClass"); }

// [TEMP] [CHOSEN, not recovered] Unused effect fixture; SDK state is poisoned, not reconstructed.
NovintHFX::ConstantEffectParams::ConstantEffectParams() throw()
{
    m_unresolved18 = m_unresolved1C = -1;
    fake::sdk.add("constant params constructed");
}
const char* NovintHFX::ConstantEffectParams::StandardEffectName() const { return "Constant"; }
#define FAKE_VAR(sig) bool NovintHFX::ConstantEffectParams::sig { unsupported(#sig); }
FAKE_VAR(SetVar(const char*, double)) FAKE_VAR(SetVar(const char*, bool)) FAKE_VAR(SetVar(const char*, float))
FAKE_VAR(SetVar(const char*, int)) FAKE_VAR(SetVar(const char*, void*)) FAKE_VAR(SetVar(const char*, int, double))
FAKE_VAR(SetVar(const char*, int, bool)) FAKE_VAR(SetVar(const char*, int, float)) FAKE_VAR(SetVar(const char*, int, int))
FAKE_VAR(SetVar(const char*, int, void*)) FAKE_VAR(GetVar(const char*, float&)) FAKE_VAR(GetVar(const char*, double&))
FAKE_VAR(GetVar(const char*, bool&)) FAKE_VAR(GetVar(const char*, int&)) FAKE_VAR(GetVar(const char*, void*))
FAKE_VAR(GetVar(const char*, int, float&)) FAKE_VAR(GetVar(const char*, int, double&)) FAKE_VAR(GetVar(const char*, int, bool&))
FAKE_VAR(GetVar(const char*, int, int&)) FAKE_VAR(GetVar(const char*, int, void*))
#undef FAKE_VAR
char NovintHFX::ConstantEffectParams::GetVarType(const char*, int*) const { unsupported("GetVarType"); }
NovintHFX::ParamsStaticClass* NovintHFX::ConstantEffectParams::GetStaticInfoClass() { unsupported("GetStaticInfoClass"); }

// [TEMP] [CHOSEN, not recovered] Unused effect fixture; SDK state is poisoned, not reconstructed.
NovintHFX::EnvelopeEffectParams::EnvelopeEffectParams() throw()
{
    m_unresolved18 = -1;
    std::memset(m_unresolved1C, 0xCC, sizeof(m_unresolved1C));
    for (int i = 0; i < 3; ++i) m_force[i] = -777.0;
    fake::sdk.add("envelope params constructed");
}
const char* NovintHFX::EnvelopeEffectParams::StandardEffectName() const { return "Envelope"; }
#define FAKE_VAR(sig) bool NovintHFX::EnvelopeEffectParams::sig { unsupported(#sig); }
FAKE_VAR(SetVar(const char*, double)) FAKE_VAR(SetVar(const char*, bool)) FAKE_VAR(SetVar(const char*, float))
FAKE_VAR(SetVar(const char*, int)) FAKE_VAR(SetVar(const char*, void*)) FAKE_VAR(SetVar(const char*, int, double))
FAKE_VAR(SetVar(const char*, int, bool)) FAKE_VAR(SetVar(const char*, int, float)) FAKE_VAR(SetVar(const char*, int, int))
FAKE_VAR(SetVar(const char*, int, void*)) FAKE_VAR(GetVar(const char*, float&)) FAKE_VAR(GetVar(const char*, double&))
FAKE_VAR(GetVar(const char*, bool&)) FAKE_VAR(GetVar(const char*, int&)) FAKE_VAR(GetVar(const char*, void*))
FAKE_VAR(GetVar(const char*, int, float&)) FAKE_VAR(GetVar(const char*, int, double&)) FAKE_VAR(GetVar(const char*, int, bool&))
FAKE_VAR(GetVar(const char*, int, int&)) FAKE_VAR(GetVar(const char*, int, void*))
#undef FAKE_VAR
char NovintHFX::EnvelopeEffectParams::GetVarType(const char*, int*) const { unsupported("GetVarType"); }
NovintHFX::ParamsStaticClass* NovintHFX::EnvelopeEffectParams::GetStaticInfoClass() { unsupported("GetStaticInfoClass"); }

// ---- engine globals and fakes ------------------------------------------------------------------
namespace dice { namespace hfe {
ClassManager* classManager = nullptr;
EventManager* eventManager = nullptr;
hud::BF2HudManager* hudManager = nullptr;
IBase* g_controlSettings = nullptr;
IBase* g_hapticSettings = nullptr;
namespace io {
IBase* inputDeviceManager = nullptr;
IBase* controlMapManager = nullptr;
float inputSampleLatency = -1.0f;
}
} }

struct FakeMap final : IControlMap {
    bool invert = false;
    float scale[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    int invertReads = 0, scaleReads = 0;
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return 0; }
    void setName(const std::string&) override { unsupported("setName"); }
    const std::string& getName() const override { unsupported("getName"); }
    int getID() const override { unsupported("getID"); }
    void setID(int) override { unsupported("setID"); }
    bool axisExists(int) const override { unsupported("axisExists"); }
    bool triggerExists(int) const override { unsupported("triggerExists"); }
    bool addAxisMapping(int, int, InputDeviceFlags, InputDeviceAxes, bool, bool) override { unsupported("addAxisMapping"); }
    bool addAxisMapping(int, InputDeviceFlags, InputDeviceAxes, bool, bool) override { unsupported("addAxisMapping"); }
    bool addAxisMapping(int, InputDeviceFlags, InputDeviceButtons, InputDeviceButtons, bool) override { unsupported("addAxisMapping"); }
    bool addAxisMapping(int, InputDeviceFlags, InputDeviceKeys, InputDeviceKeys, bool) override { unsupported("addAxisMapping"); }
    bool addAxisMapping(int, int, InputDeviceFlags, InputDevicePOVs, bool, bool, bool) override { unsupported("addAxisMapping"); }
    bool addAxisMapping(int, InputDeviceFlags, InputDeviceButtons, InputDeviceFlags, InputDeviceKeys, bool) override { unsupported("addAxisMapping"); }
    bool addAxisMapping(int, InputDeviceFlags, InputDeviceKeys, InputDeviceFlags, InputDeviceButtons, bool) override { unsupported("addAxisMapping"); }
    bool addTriggerMapping(int, InputDeviceFlags, InputDeviceButtons, float, bool) override { unsupported("addTriggerMapping"); }
    bool addTriggerMapping(int, InputDeviceFlags, InputDeviceKeys, float, bool) override { unsupported("addTriggerMapping"); }
    bool addTriggerMapping(int, int, InputDeviceFlags, InputDeviceAxes, bool, float, float) override { unsupported("addTriggerMapping"); }
    void removeMapping(unsigned int) override { unsupported("removeMapping"); }
    void reset() override { unsupported("reset"); }
    void reset(int, bool, bool, bool) override { unsupported("reset"); }
    void setButtonRiseTime(float) override { unsupported("setButtonRiseTime"); }
    void setButtonFallTime(float) override { unsupported("setButtonFallTime"); }
    void setDoubleTapTime(float) override { unsupported("setDoubleTapTime"); }
    float getAxisValue(int) const override { unsupported("getAxisValue"); }
    bool isAxisValueAnalogue(int) const override { unsupported("isAxisValueAnalogue"); }
    int getTriggerState(int) const override { unsupported("getTriggerState"); }
    float getTriggerSampleLatency(int) const override { unsupported("getTriggerSampleLatency"); }
    bool validate() override { unsupported("validate"); }
    void update(float) override { unsupported("update"); }
    void setInvertMouse(bool) override { unsupported("setInvertMouse"); }
    bool getInvertMouse() override { ++invertReads; return invert; }
    void setMouseSensitivity(float) override { unsupported("setMouseSensitivity"); }
    float getMouseSensitivity() override { unsupported("getMouseSensitivity"); }
    void setKeyboardSensitivity(float) override { unsupported("setKeyboardSensitivity"); }
    float getKeyboardSensitivity() override { unsupported("getKeyboardSensitivity"); }
    void setAxisScale(InputDeviceAxes, float) override { unsupported("setAxisScale"); }
    float getAxisScale(InputDeviceAxes a) const override { ++const_cast<FakeMap*>(this)->scaleReads; return scale[a]; }
    void setYawFactor(float) override { unsupported("setYawFactor"); }
    void setPitchFactor(float) override { unsupported("setPitchFactor"); }
    float getYawFactor() override { unsupported("getYawFactor"); }
    float getPitchFactor() override { unsupported("getPitchFactor"); }
    void save(std::ostream&, bool) override { unsupported("save"); }
    void dump(bool) override { unsupported("dump"); }
    bool findDuplicate(InputDeviceFlags&, InputDeviceAxes&, InputDeviceButtons&, InputDeviceKeys&,
                       DuplicateKeyVector&) override { unsupported("findDuplicate"); }
    bool getInputMappedToMouseButton(bool, PlayerInputMap&) override { unsupported("getInputMappedToMouseButton"); }
};

struct FakeMaps final : IControlMapManager {
    FakeMap map;
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return 0; }
    void update(float) override { unsupported("maps update"); }
    IControlMap* create(const std::string&) override { unsupported("create"); }
    IControlMap* getControlMap(const std::string&) const override { unsupported("getControlMap"); }
    IControlMap* getControlMap(int) const override { unsupported("getControlMap"); }
    void deleteControlMap(const std::string&) override { unsupported("deleteControlMap"); }
    void deleteControlMap(int) override { unsupported("deleteControlMap"); }
    void deleteControlMap(const IControlMap*) override { unsupported("deleteControlMap"); }
    void setActive(const std::string&) override { unsupported("setActive"); }
    void setActive(const IControlMap*) override { unsupported("setActive"); }
    IControlMap* getActive() const override { return const_cast<FakeMap*>(&map); }
    int mapNextInput(IControlMap*, int, bool, bool, bool, bool, std::string&) override { unsupported("mapNextInput"); }
    float getRepeatRate(int, bool) override { unsupported("getRepeatRate"); }
    void applyActive() override { unsupported("applyActive"); }
};

struct FakeControlSettings final : IControlSettings {
    bool mapping = false;
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return 0; }
    std::string getFileName() override { unsupported("getFileName"); }
    std::string getSettingsScript() override { unsupported("getSettingsScript"); }
    bool restoreControlMaps() override { unsupported("restoreControlMaps"); }
    void backupControlMaps() override { unsupported("backupControlMaps"); }
    void backupControlMapsByName() override { unsupported("backupControlMapsByName"); }
    bool setDefaults() override { unsupported("setDefaults"); }
    void resetControlMaps() override { unsupported("resetControlMaps"); }
    void updateInputDevices() override { unsupported("updateInputDevices"); }
    int mapNextInput(int, bool, int, bool, bool) override { unsupported("mapNextInput"); }
    bool selectControlMap(const std::string&) override { unsupported("selectControlMap"); }
    bool getBindingDescription(io::IControlMap*, int, bool, bool, bool, std::string&, std::wstring&) override { unsupported("getBindingDescription"); }
    std::string getActiveControlMapName() override { unsupported("getActiveControlMapName"); }
    float getMouseSensitivity() override { unsupported("getMouseSensitivity"); }
    void setMouseSensitivity(float) override { unsupported("setMouseSensitivity"); }
    float getSlot48() override { unsupported("getSlot48"); }
    void setSlot4C(float) override { unsupported("setSlot4C"); }
    float getSlot50() override { unsupported("getSlot50"); }
    void setSlot54(float) override { unsupported("setSlot54"); }
    float getYawFactor() override { unsupported("getYawFactor"); }
    void setYawFactor(float) override { unsupported("setYawFactor"); }
    float getPitchFactor() override { unsupported("getPitchFactor"); }
    void setPitchFactor(float) override { unsupported("setPitchFactor"); }
    bool getInvertMouse() override { unsupported("getInvertMouse"); }
    void setInvertMouse(bool) override { unsupported("setInvertMouse"); }
    float getKeyboardSensitivity() override { unsupported("getKeyboardSensitivity"); }
    void setKeyboardSensitivity(float) override { unsupported("setKeyboardSensitivity"); }
    bool apply() override { unsupported("apply"); }
    void deleteControlMapBackups() override { unsupported("deleteControlMapBackups"); }
    void activateMouse() override { unsupported("activateMouse"); }
    void deactivateMouse() override { unsupported("deactivateMouse"); }
    bool isMappingInput() override { return mapping; }
    void setMappingInput(bool) override { unsupported("setMappingInput"); }
    std::string getMappedInputName() override { unsupported("getMappedInputName"); }
};

struct Nothing final : IBase {
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return 0; }
};

// ---- helpers -----------------------------------------------------------------------------------
static void poll(FalconDevice& d, DWORD tick, double x, double y, double z, int buttons)
{
    g_tick = tick;
    fake::toolPosition[0] = x; fake::toolPosition[1] = y; fake::toolPosition[2] = z;
    fake::buttonData = buttons;
    d.update(0.0f);
}
static double fromFloat(float f) { return static_cast<double>(f); }

// ---- updateShakeForce's soldier test: the occupied object's template class id -------------------
#include "../BF2/Core/Ids.h"
#include "../BF2/Game/Objects/IObject.h"
#include "../BF2/Game/Objects/IObjectTemplate.h"
#include "../BF2/Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
namespace {
using namespace dice::hfe::world;
[[noreturn]] void unexpected(const char* name) { unsupported(name); }
struct ShakeTemplate final : IObjectTemplate {
    unsigned int classId = 0;
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("IObjectTemplate::queryInterface"); }
    unsigned int getClassID() const override { return classId; }
    void templateInitialize() override { unexpected("IObjectTemplate::templateInitialize"); }
    bool writeBinary(io::IStream*) const override { unexpected("IObjectTemplate::writeBinary"); }
    bool readBinary(io::IStream*) override { unexpected("IObjectTemplate::readBinary"); }
    void setName(const std::string&) override { unexpected("IObjectTemplate::setName"); }
    const std::string& getName() const override { unexpected("IObjectTemplate::getName"); }
    const std::string& getCreator() const override { unexpected("IObjectTemplate::getCreator"); }
    void setCreator(const std::string&) override { unexpected("IObjectTemplate::setCreator"); }
    const std::string& getModifiedByUser() const override { unexpected("IObjectTemplate::getModifiedByUser"); }
    void setModifiedByUser(const std::string&) override { unexpected("IObjectTemplate::setModifiedByUser"); }
    void setId(unsigned int) override { unexpected("IObjectTemplate::setId"); }
    unsigned int getId() const override { unexpected("IObjectTemplate::getId"); }
    unsigned int getFlags() const override { unexpected("IObjectTemplate::getFlags"); }
    unsigned int updateFlags(unsigned int set, unsigned int clear) override { unexpected("IObjectTemplate::updateFlags"); }
    void* queryImplementation(unsigned int classId) override { unexpected("IObjectTemplate::queryImplementation"); }
    IObject* createObject() const override { unexpected("IObjectTemplate::createObject"); }
    void setComponent(unsigned int classId, IBase* component) override { unexpected("IObjectTemplate::setComponent"); }
    IObjectTemplateComponent* queryComponent(unsigned int) override { unexpected("IObjectTemplate::queryComponent"); }
    IObjectTemplateComponent* getComponentFromClassID(unsigned int) const override { unexpected("IObjectTemplate::getComponentFromClassID"); }
    void objectCreated(IObject*) override { unexpected("IObjectTemplate::objectCreated"); }
    void objectDestroyed(IObject*) override { unexpected("IObjectTemplate::objectDestroyed"); }
    void preCache() const override { unexpected("IObjectTemplate::preCache"); }
    void setCullRadiusScale(float) override { unexpected("IObjectTemplate::setCullRadiusScale"); }
    float getCullRadiusScale() const override { unexpected("IObjectTemplate::getCullRadiusScale"); }
    void setPhysicsComponent(IObject*) const override { unexpected("IObjectTemplate::setPhysicsComponent"); }
    void removePhysicsComponent(IObject*) const override { unexpected("IObjectTemplate::removePhysicsComponent"); }
    void setPhysicsNodeComponent(IObject*) const override { unexpected("IObjectTemplate::setPhysicsNodeComponent"); }
    void removePhysicsNodeComponent(IObject*) const override { unexpected("IObjectTemplate::removePhysicsNodeComponent"); }
    void setResponsePhysicsComponent(IObject*) const override { unexpected("IObjectTemplate::setResponsePhysicsComponent"); }
    void removeResponsePhysicsComponent(IObject*) const override { unexpected("IObjectTemplate::removeResponsePhysicsComponent"); }
    void setNetworkableInfo(const std::string&) override { unexpected("IObjectTemplate::setNetworkableInfo"); }
    const std::string& getNetworkableInfo() const override { unexpected("IObjectTemplate::getNetworkableInfo"); }
    void setCollisionMesh(const std::string&) override { unexpected("IObjectTemplate::setCollisionMesh"); }
    const std::string& getCollisionMesh() const override { unexpected("IObjectTemplate::getCollisionMesh"); }
    const Vec3& getAnchor() const override { unexpected("IObjectTemplate::getAnchor"); }
    void setAnchor(const Vec3&) override { unexpected("IObjectTemplate::setAnchor"); }
    const Vec3& getAnchorOffset() const override { unexpected("IObjectTemplate::getAnchorOffset"); }
    void setAnchorOffset(const Vec3&) override { unexpected("IObjectTemplate::setAnchorOffset"); }
    Vec3 getAnchorPos() const override { unexpected("IObjectTemplate::getAnchorPos"); }
    void setIsOvergrowth(bool) override { unexpected("IObjectTemplate::setIsOvergrowth"); }
    bool getIsOvergrowth() const override { unexpected("IObjectTemplate::getIsOvergrowth"); }
    std::vector<std::pair<std::string, unsigned int> >* getMaterialVector() override { unexpected("IObjectTemplate::getMaterialVector"); }
    ComponentMap* getComponentMap() override { unexpected("IObjectTemplate::getComponentMap"); }
    void setMaterial(int) override { unexpected("IObjectTemplate::setMaterial"); }
    int getMaterial() const override { unexpected("IObjectTemplate::getMaterial"); }
    void setSpecialMaterials(unsigned int) override { unexpected("IObjectTemplate::setSpecialMaterials"); }
    unsigned int getSpecialMaterials() const override { unexpected("IObjectTemplate::getSpecialMaterials"); }
};
struct ShakePco final : IPlayerControlObject {
    IObject* occupied = 0;
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int iid) const override
    {
        if (iid != IID_IObject) unexpected("IPlayerControlObject::queryInterface(other)");
        return occupied;
    }
    IPlayer* getOccupingPlayer() const override { unexpected("IPlayerControlObject::getOccupingPlayer"); }
    IObject* getOccupingObject() const override { unexpected("IPlayerControlObject::getOccupingObject"); }
    bool isOccupied() const override { unexpected("IPlayerControlObject::isOccupied"); }
    bool enter(IPlayer* player) override { unexpected("IPlayerControlObject::enter"); }
    void exit() override { unexpected("IPlayerControlObject::exit"); }
    void exitPlayer() override { unexpected("IPlayerControlObject::exitPlayer"); }
    void onEnter(IObject*) override { unexpected("IPlayerControlObject::onEnter"); }
    void onExit() override { unexpected("IPlayerControlObject::onExit"); }
    int getPcoId() const override { unexpected("IPlayerControlObject::getPcoId"); }
    void setPcoId(int id) override { unexpected("IPlayerControlObject::setPcoId"); }
    std::map<int, ICameraObject*>* getCameras() override { unexpected("IPlayerControlObject::getCameras"); }
    std::map<int, ICameraObject*>* getOwnCameras() override { unexpected("IPlayerControlObject::getOwnCameras"); }
    void nextCamera() override { unexpected("IPlayerControlObject::nextCamera"); }
    ICameraObject* getActiveCamera() override { unexpected("IPlayerControlObject::getActiveCamera"); }
    void setActiveCamera(int) override { unexpected("IPlayerControlObject::setActiveCamera"); }
    bool getExitPosition(Vec3& out) override { unexpected("IPlayerControlObject::getExitPosition"); }
    bool getAltExitPosition(Vec3& out) override { unexpected("IPlayerControlObject::getAltExitPosition"); }
    void addExternalCamera(ICameraObject* camera) override { unexpected("IPlayerControlObject::addExternalCamera"); }
    void removeExternalCamera(ICameraObject* camera) override { unexpected("IPlayerControlObject::removeExternalCamera"); }
    bool canSpawn(const Mat4& transform) override { unexpected("IPlayerControlObject::canSpawn"); }
    std::map<int, IPlayerControlObject*>* getPcos() override { unexpected("IPlayerControlObject::getPcos"); }
    const std::vector<int>* getPcoPosId() override { unexpected("IPlayerControlObject::getPcoPosId"); }
    void addWeapon(IObject* weapon) override { unexpected("IPlayerControlObject::addWeapon"); }
    void* getWeapons() override { unexpected("IPlayerControlObject::getWeapons"); }
    IObject* getActiveWeapon(int) const override { unexpected("IPlayerControlObject::getActiveWeapon"); }
    void* getWeapons(int) override { unexpected("IPlayerControlObject::getWeapons"); }
    void setActiveWeapon(IObject* weapon, int) override { unexpected("IPlayerControlObject::setActiveWeapon"); }
    void toggleOnWeaponLock(IObject*) override { unexpected("IPlayerControlObject::toggleOnWeaponLock"); }
    int getArtPos() override { unexpected("IPlayerControlObject::getArtPos"); }
    bool checkSpecialToggle(io::PlayerInput input) override { unexpected("IPlayerControlObject::checkSpecialToggle"); }
    bool checkSpecialToggleLastUpdate(io::PlayerInputMap map) override { unexpected("IPlayerControlObject::checkSpecialToggleLastUpdate"); }
    void setTeam(int team) override { unexpected("IPlayerControlObject::setTeam"); }
    void setDefaultTeam(int team) override { unexpected("IPlayerControlObject::setDefaultTeam"); }
    int getDefaultTeam() override { unexpected("IPlayerControlObject::getDefaultTeam"); }
    int getTeam() override { unexpected("IPlayerControlObject::getTeam"); }
    float getTimeToLiveUnused() override { unexpected("IPlayerControlObject::getTimeToLiveUnused"); }
    void setTimeToLiveUnused(float t) override { unexpected("IPlayerControlObject::setTimeToLiveUnused"); }
    bool isAllowedToExit() override { unexpected("IPlayerControlObject::isAllowedToExit"); }
    void addPlayerInputObject(IObject*) override { unexpected("IPlayerControlObject::addPlayerInputObject"); }
    void removePlayerInputObject(IObject*) override { unexpected("IPlayerControlObject::removePlayerInputObject"); }
    void setTimeSpawned(float t) override { unexpected("IPlayerControlObject::setTimeSpawned"); }
    float getTimeSpawned() const override { unexpected("IPlayerControlObject::getTimeSpawned"); }
    void setObjectSpawnerId(int id) override { unexpected("IPlayerControlObject::setObjectSpawnerId"); }
    int getObjectSpawnerId() const override { unexpected("IPlayerControlObject::getObjectSpawnerId"); }
    void setObjectSpawnerHolding(bool holding) override { unexpected("IPlayerControlObject::setObjectSpawnerHolding"); }
    bool getObjectSpawnerHolding() const override { unexpected("IPlayerControlObject::getObjectSpawnerHolding"); }
    const Vec3& getNameTagOffset() override { unexpected("IPlayerControlObject::getNameTagOffset"); }
    const Vec3& getRidingWithOffset() override { unexpected("IPlayerControlObject::getRidingWithOffset"); }
    float getRidingWithDistMod() override { unexpected("IPlayerControlObject::getRidingWithDistMod"); }
    int getVehicleCameraShake() override { unexpected("IPlayerControlObject::getVehicleCameraShake"); }
    float getVehicleFov() override { unexpected("IPlayerControlObject::getVehicleFov"); }
    void addVehicleMod(VehicleMod mod) override { unexpected("IPlayerControlObject::addVehicleMod"); }
    void removeVehicleMod(VehicleMod mod) override { unexpected("IPlayerControlObject::removeVehicleMod"); }
    int getVehicleMod(VehicleMod mod) override { unexpected("IPlayerControlObject::getVehicleMod"); }
    const std::string& getSoundFilter() const override { unexpected("IPlayerControlObject::getSoundFilter"); }
    int getCockpitLod() override { unexpected("IPlayerControlObject::getCockpitLod"); }
    int getCockpitSubGeom() override { unexpected("IPlayerControlObject::getCockpitSubGeom"); }
    float getShakeFactor() const override { unexpected("IPlayerControlObject::getShakeFactor"); }
    bool getIsSprinting() const override { unexpected("IPlayerControlObject::getIsSprinting"); }
    void setIsSprinting(bool) override { unexpected("IPlayerControlObject::setIsSprinting"); }
    float getSprintStamina() const override { unexpected("IPlayerControlObject::getSprintStamina"); }
    void setSprintStamina(float) override { unexpected("IPlayerControlObject::setSprintStamina"); }
    void setSprintStaminaDissipationFactor(float) override { unexpected("IPlayerControlObject::setSprintStaminaDissipationFactor"); }
    float getSprintFactor() override { unexpected("IPlayerControlObject::getSprintFactor"); }
    bool getIsUnderwater() const override { unexpected("IPlayerControlObject::getIsUnderwater"); }
    float getNormalizedUnderwaterTime() const override { unexpected("IPlayerControlObject::getNormalizedUnderwaterTime"); }
    void setDriverDecalObject(IObject*) override { unexpected("IPlayerControlObject::setDriverDecalObject"); }
    IObject* getDriverDecalObject() const override { unexpected("IPlayerControlObject::getDriverDecalObject"); }
    void addDriverDecalTransformation(const Mat4&) override { unexpected("IPlayerControlObject::addDriverDecalTransformation"); }
    std::vector<Mat4>* getDriverDecalTransformations() const override { unexpected("IPlayerControlObject::getDriverDecalTransformations"); }
    int getSpecialToggleWeaponInput() const override { unexpected("IPlayerControlObject::getSpecialToggleWeaponInput"); }
    void setIsDucking(bool) override { unexpected("IPlayerControlObject::setIsDucking"); }
    bool getIsDucking() override { unexpected("IPlayerControlObject::getIsDucking"); }
    bool getAutoUseAbility() const override { unexpected("IPlayerControlObject::getAutoUseAbility"); }
    bool applyCameraAnimation(const Mat4& in, Mat4& out) override { unexpected("IPlayerControlObject::applyCameraAnimation"); }
    bool allowEnterOtherVehicle() override { unexpected("IPlayerControlObject::allowEnterOtherVehicle"); }
    bool isChildSoldierActive() const override { unexpected("IPlayerControlObject::isChildSoldierActive"); }
    bool getNeverDrawAs1p() const override { unexpected("IPlayerControlObject::getNeverDrawAs1p"); }
    void removeChildVehicle(IObject*) override { unexpected("IPlayerControlObject::removeChildVehicle"); }
    int getAmmoStatus() override { unexpected("IPlayerControlObject::getAmmoStatus"); }
    void setAmmoStatus(int) override { unexpected("IPlayerControlObject::setAmmoStatus"); }
    void onFirstGhost() override { unexpected("IPlayerControlObject::onFirstGhost"); }
    void setGiveUnmannedInput(bool) override { unexpected("IPlayerControlObject::setGiveUnmannedInput"); }
    void preHandlePlayerInput(io::PlayerInput& input, float dt) const override { unexpected("IPlayerControlObject::preHandlePlayerInput"); }
};
// Never constructed: updateShakeForce reads only the template field (IObject::getTemplate, inline).
struct ObjectFields : IObject {
    static void setTemplate(IObject* o, const IObjectTemplate* t) { static_cast<ObjectFields*>(o)->m_template = t; }
};
} // namespace

int main()
{
    FakeMaps maps;
    FakeControlSettings controls;
    Nothing deviceManager;
    io::controlMapManager = &maps;
    io::inputDeviceManager = &deviceManager;
    g_controlSettings = &controls;
    HapticSettings* hapticOwner = new HapticSettings;
    g_hapticSettings = static_cast<IHapticSettings*>(hapticOwner);
    IHapticSettings* haptics = static_cast<IHapticSettings*>(g_hapticSettings);
    haptics->setAimSensitivity(2.0f, 3.0f, 0.5f);
    haptics->setAimSensitivityLength(0.25f, 0.5f, 0.125f);
    haptics->setTurnSensitivity(4.0f, 0.75f, 1.5f);
    haptics->setControlBoxLength(0.75f, 1.25f, 0.5f);
    haptics->setControlBoxStiffness(100.0f, 200.0f, 300.0f);
    static NovintHFX::Stack stackObject;
    g_stack = &stackObject;

    FalconDevice* device = new FalconDevice;
    device->m_sampleTime = 0;
    check(device->init() && device->m_active && device->m_device == &g_deviceObject, "fake device runs");
    check(fake::sdk.joined() == "attach foreground;recount;run 0;startMouse device;", "init's SDK sequence");
    check(g_falconMouseMode == 1 && g_falconControlBoxDirty, "shipped initial modes");
    fake::sdk.clear();

    // --- mouse mode: raw position, gain 1000, StartMouse once, the axis scaling -----------------
    poll(*device, 1000, 0.5, 0.25, -0.125, 0x5);   // sample clock 0 -> poll time 1000
    poll(*device, 1010, 0.75, -0.5, 0.25, 0x2);
    check(device->m_samples.size() == 2 && device->m_sampleTime == 1000 && device->m_pollTime == 1010,
          "update(0) queues samples and lags the sample clock one poll");
    maps.map.scale[0] = 2.0f; maps.map.scale[1] = 0.5f; maps.map.scale[2] = 4.0f; maps.map.scale[3] = 1.0f;
    device->m_buttons = 0x40;
    fake::sdk.clear();
    device->update(0.0205f);                        // +20 ms (0.0205*1000 truncates)
    check(device->m_sampleTime == 1020, "dt*1000 truncates into the sample clock");
    check(device->m_samples.empty(), "both samples older than the clock are consumed");
    check(device->m_previousButtons == 0x40 && device->m_buttons == 0x2, "buttons: previous frame kept, last sample wins");
    check(fake::sdk.joined() == "startMouse device;buttons;buttons;", "StartMouse once, before the first sample's button read");
    check(g_falconMouseMode == 2, "mode 1 becomes 2 after StartMouse");
    check(same(device->m_axes[0], 1000.0 * 0.75 * 2.0) && same(device->m_axes[1], -(1000.0 * -0.5) * 0.5)
          && same(device->m_axes[2], 0.0), "mouse axes: gain, negated y, the map's scale; z stays zero");
    check(io::inputSampleLatency == 0.0f, "latency (poll time - sample time) * 0.001");
    check(maps.map.invertReads == 4, "getInvertMouse read twice per sample (y and z)");
    check(maps.map.scaleReads == 4, "four axis-scale reads (the fourth-axis defect)");

    // --- latency of an older sample ------------------------------------------------------------
    poll(*device, 1100, 0.0, 0.0, 0.0, 0);
    poll(*device, 1137, 0.0, 0.0, 0.0, 0);
    device->m_samples.pop_back();                    // keep only the 1100 sample, polled at 1137
    device->update(0.2f);
    check(io::inputSampleLatency == static_cast<float>(37.0 * static_cast<double>(0.001f)), "latency 37 ms");

    // --- control-box mode: rebuild on mode 0, params from HapticSettings, effect created once --
    maps.map.scale[0] = maps.map.scale[1] = maps.map.scale[2] = 1.0f;
    g_falconMouseMode = 0;
    g_effect.m_stateFlags = 0;
    fake::sdk.clear();
    poll(*device, 2000, 0.1, 0.1, 0.1, 0);           // inside every dead zone
    fake::sdk.clear();
    device->update(1.0f);
    check(g_falconMouseMode == -1 && !g_falconControlBoxDirty, "mode 0 becomes -1 and clears the flag");
    check(fake::sdk.joined() == "params constructed;stack;create BOX ControlBox;update flags=0;params destroyed;buttons;",
          "rebuild: construct, create once, update, delete");
    check(fake::liveParams == 0, "params deleted through the virtual destructor");
    check(fake::updates.size() == 1, "one parameter update");
    const fake::Captured& c = fake::updates.back();
    check(same(c.lower[0], -fromFloat(0.75f)) && same(c.lower[1], -fromFloat(1.25f))
          && same(c.lower[2], -fromFloat(0.5f) - fromFloat(0.01f)), "lower walls: -length, z 0.01 lower");
    check(same(c.upper[0], fromFloat(0.75f)) && same(c.upper[1], fromFloat(1.25f))
          && same(c.upper[2], fromFloat(0.5f) - fromFloat(0.01f)), "upper walls: length, z 0.01 lower");
    check(c.lowerStiffness[0] == 100.0 && c.lowerStiffness[1] == 200.0 && c.lowerStiffness[2] == 300.0
          && c.upperStiffness[0] == 100.0 && c.upperStiffness[2] == 300.0, "stiffness both sides");
    check(c.value114 == 3000, "+0x114 = 3000");

    // --- no rebuild without the flag; the flag forces one; the effect is not created again -----
    fake::sdk.clear();
    poll(*device, 2100, 0.1, 0.1, 0.1, 0);
    fake::sdk.clear();
    device->update(1.0f);
    check(fake::sdk.joined() == "buttons;", "no rebuild while mode -1 and clean");
    g_falconControlBoxDirty = true;
    g_effect.m_stateFlags = 1;
    poll(*device, 2200, 0.1, 0.1, 0.1, 0);
    fake::sdk.clear();
    device->update(1.0f);
    check(fake::sdk.joined() == "params constructed;update flags=1;params destroyed;buttons;",
          "the flag rebuilds into the existing effect");
    check(g_effect.m_stateFlags == 0, "rebuild clears the suspended bit");

    // --- back to mouse mode: StartMouse and the suspended bit ----------------------------------
    g_falconMouseMode = 1;
    poll(*device, 2300, 0.1, 0.1, 0.1, 0);
    fake::sdk.clear();
    device->update(1.0f);
    check(fake::sdk.joined() == "startMouse device;buttons;" && g_effect.m_stateFlags == 1 && g_falconMouseMode == 2,
          "entering mouse mode suspends the existing control-box effect");
    g_falconMouseMode = -1;

    // --- dead zones, turn sensitivity and the smoothed deltas -----------------------------------
    // The smoothing history is already full (count == window); feed positions and check the axes.
    // Each delta is (newest - oldest) / (2 * window) over aim-scaled positions.
    std::vector<double> hx, hy, hz;   // model of the three histories after the earlier samples
    // Rebuild the model by replaying: earlier samples were (0.5,.25,-.125),(0.75,-.5,.25),(0,0,0),
    // then (0.1,0.1,0.1) x4; the first three were pushed raw, the rest aim-scaled with a pop.
    hx = { 0.5, 0.75, 0.0 }; hy = { 0.25, -0.5, 0.0 }; hz = { -0.125, 0.25, 0.0 };
    auto step = [](std::vector<double>& h, double v) { h.push_back(v); h.erase(h.begin()); return (h.back() - h.front()) / 6.0; };
    for (int i = 0; i < 4; ++i) { step(hx, 0.1 * 2.0f); step(hy, 0.1 * 3.0f); step(hz, 0.1 * 0.5f); }
    struct Case { double x, y, z; const char* label; };
    const Case cases[] = {
        { 0.5, 0.75, 0.5, "beyond the upper dead-zone edge" },
        { -0.5, -0.75, -0.5, "beyond the lower dead-zone edge" },
        { 0.2, 0.25, 0.1, "inside the dead zone: smoothed delta" },
        { std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, "NaN takes the delta path" },
    };
    for (const Case& k : cases) {
        poll(*device, g_tick + 100, k.x, k.y, k.z, 0);
        device->update(1.0f);
        const double dX = step(hx, k.x * 2.0f), dY = step(hy, k.y * 3.0f), dZ = step(hz, k.z * 0.5f);
        auto expect = [](double p, float dz, float turn, double scale, double delta, double deltaScale) {
            if (dz < p) return (p - dz) * turn * scale;
            if (-dz > p) return (dz + p) * turn * scale;
            return delta * deltaScale;
        };
        const double ex = expect(k.x, 0.25f, 4.0f, 2000.0, dX, 2500.0);
        const double ey = expect(k.y, 0.5f, 0.75f, -2000.0, dY, -2500.0);
        const double ez = expect(k.z, 0.125f, 1.5f, -2000.0, dZ, -2500.0);
        check(same(device->m_axes[0], ex) && same(device->m_axes[1], ey) && same(device->m_axes[2], ez), k.label);
    }

    // --- inverted mouse negates y and z before everything ---------------------------------------
    maps.map.invert = true;
    poll(*device, g_tick + 100, 0.5, 0.75, 0.5, 0);
    device->update(1.0f);
    {
        const double dX = step(hx, 0.5 * 2.0f), dY = step(hy, -0.75 * 3.0f), dZ = step(hz, -0.5 * 0.5f);
        (void) dX; (void) dY; (void) dZ;
        check(same(device->m_axes[1], (-0.75 + 0.5f) * 0.75f * -2000.0)
              && same(device->m_axes[2], (-0.5 + 0.125f) * 1.5f * -2000.0), "inverted y and z");
    }
    maps.map.invert = false;

    // --- binding capture ends the frame early ---------------------------------------------------
    controls.mapping = true;
    device->m_buttons = 0x9;
    maps.map.scale[0] = 3.0f;
    poll(*device, g_tick + 100, 0.5, 0.5, 0.5, 0x4);
    const int scaleReadsBefore = maps.map.scaleReads;
    fake::sdk.clear();
    device->update(1.0f);
    check(device->m_samples.size() == 1, "capture: the sample stays queued");
    check(device->m_buttons == 0x9 && device->m_previousButtons == 0x9, "capture: no button read");
    check(device->m_axes[0] == 0.0 && device->m_axes[1] == 0.0 && device->m_axes[2] == 0.0, "capture: axes zeroed only");
    check(maps.map.scaleReads == scaleReadsBefore && fake::sdk.calls.empty(), "capture: no scaling, no SDK call");
    controls.mapping = false;
    device->m_samples.clear();
    maps.map.scale[0] = 1.0f;

    // --- the fourth-axis defect: m_axisTravel[0..1] scaled as a double ---------------------------
    {
        const double bits = 1.5;
        std::memcpy(device->m_axisTravel, &bits, sizeof(bits));
        device->m_axisTravel[2] = 7;
        maps.map.scale[3] = 2.0f;
        device->update(1.0f);                        // empty queue: only the scaling runs
        double after;
        std::memcpy(&after, device->m_axisTravel, sizeof(after));
        check(after == 3.0 && device->m_axisTravel[2] == 7, "fourth scale lands on m_axisTravel[0..1] only");
        maps.map.scale[3] = 1.0f;
        std::memset(device->m_axisTravel, 0, sizeof(device->m_axisTravel));
    }

    // --- empty queue: axes reset, scaling only ------------------------------------------------
    device->m_axes[0] = 5.0;
    device->update(1.0f);
    check(device->m_axes[0] == 0.0 && device->m_axes[1] == 0.0, "every frame starts from zero axes");

    // --- MainMenuLogic's zeroing of the haptic force effects (0x4A3490..0x4A3535) ---------------
    {
        fake::sdk.clear();
        fake::vect3Constructions = 0;
        io::zeroHapticForceTargets();
        check(fake::sdk.calls.empty() && fake::vect3Constructions == 1, "no effects: one zero vector, no call");
        static FakeEffect targets[6] = { FakeEffect(0), FakeEffect(1), FakeEffect(2), FakeEffect(3), FakeEffect(4), FakeEffect(5) };
        for (int i = 0; i < 6; ++i) g_hapticForceTargets[i] = &targets[i];
        g_hapticForceTargets[3] = 0;     // ds:0x9EBF04 absent
        fake::sdk.clear();
        fake::vect3Constructions = 0;
        io::zeroHapticForceTargets();
        check(fake::sdk.joined() == "force 2 0 0 0;force 4 0 0 0;force 5 0 0 0;force 0 0 0 0;force 1 0 0 0;",
              "zero force in retail order 0x9EBF00..0x9EBF0C, 0x9EBEC8, 0x9EBECC; nulls skipped");
        check(fake::vect3Constructions == 1, "one Vect3 shared by every call");
        for (int i = 0; i < 6; ++i) g_hapticForceTargets[i] = 0;
    }

    // --- Camera::handleUpdate's Novint ShakeForce block (0x5A3F4F..0x5A41D7, ledger N2 17b) --------
    {
        ShakeTemplate vehicleTemplate, soldierTemplate;
        vehicleTemplate.classId = 1;
        soldierTemplate.classId = CID_SoldierTemplate;
        alignas(16) static unsigned char occupiedStorage[sizeof(IObject)];
        IObject* occupied = reinterpret_cast<IObject*>(occupiedStorage);
        ObjectFields::setTemplate(occupied, &vehicleTemplate);
        ShakePco pco;
        pco.occupied = occupied;
        haptics->setExplosionShakeScale(2.0f);
        haptics->setSpeedShakeScale(3.0f);
        haptics->setGeneralShakeScale(0.5f);
        haptics->setUnresolved48C(0.25f);
        haptics->setUnresolved490(0.75f);
        haptics->setForceCapX(10.0f);
        haptics->setForceCapY(10.0f);

        NovintHFX::Device* const saved = io::g_falconDevice;
        io::g_falconDevice = 0;
        fake::sdk.clear();
        io::updateShakeForce(&pco, 0.1f, 0.2f, 0.4f, 0.8f);
        check(fake::sdk.calls.empty() && !g_hapticForceTargets[0], "shake force: no device, nothing");
        io::g_falconDevice = saved;
        check(io::g_falconDevice != 0, "shake force: the fake device is the current one");

        fake::sdk.clear();
        fake::constantEffects = 0;
        io::updateShakeForce(&pco, 0.1f, 0.2f, 0.4f, 0.8f);
        check(fake::sdk.joined() == "constant params constructed;stack;create CONSTANT ShakeForce;stack;create CONSTANT ShakeForce;params destroyed;",
              "shake force: first call creates the two CONSTANT ShakeForce effects from one params, then deletes it");
        check(fake::constantParam18 == 0x14 && g_hapticForceTargets[0] && g_hapticForceTargets[1]
              && g_hapticForceTargets[0] != g_hapticForceTargets[1], "shake force: params +0x18 = 20; two effects stored");

        fake::sdk.clear();
        io::updateShakeForce(&pco, 0.1f, 0.2f, 0.4f, 0.8f);
        // x = 2*0.1 + 3*0.2 + 0.5*0.4 + 0.25*0.8 = 1.2; y = 2*0.1 + 3*0.2 + 0.5*0.4 + 0.75*0.8 = 1.6
        check(fake::sdk.joined() == "force 10 1.2 1.6 0;", "shake force: the vehicle effect gets (x, y, 0) from the scaled terms");

        ObjectFields::setTemplate(occupied, &soldierTemplate);
        haptics->setForceCapX(1.0f);
        haptics->setForceCapY(1.6f);
        fake::sdk.clear();
        io::updateShakeForce(&pco, 0.1f, 0.2f, 0.4f, 0.8f);
        check(fake::sdk.joined() == "force 11 1 1.6 0;", "shake force: the soldier effect; x capped at 1");
        {
            // y = 1.000000x (float) + 0.75 * 0.8f in double: |y| rounds to 1.6f, the cap, so it is not
            // above it and keeps its unrounded value (a clamp would store 1.6f itself).
            const float shared = 2.0f * 0.1f + 3.0f * 0.2f + 0.5f * 0.4f;
            const double expectedY = static_cast<double>(0.75f) * 0.8f + shared;
            check(static_cast<float>(expectedY) == 1.6f && expectedY != static_cast<double>(1.6f)
                  && fake::lastForce[1] == expectedY, "shake force: y equal to its cap at float precision is not clamped");
        }
        haptics->setForceCapY(1.5f);
        fake::sdk.clear();
        io::updateShakeForce(&pco, -0.1f, -0.2f, -0.4f, -0.8f);
        check(fake::sdk.joined() == "force 11 -1 -1.5 0;", "shake force: both caps keep the sign");
        // At cap 0 a -0 term is not above it and keeps its sign; a clamp would store sign(-0) * 0 = +0.
        haptics->setForceCapX(0.0f);
        haptics->setForceCapY(0.0f);
        fake::sdk.clear();
        io::updateShakeForce(&pco, -0.0f, -0.0f, -0.0f, -0.0f);
        check(fake::sdk.joined() == "force 11 -0 -0 0;", "shake force: -0 at a zero cap is left alone");
        // A negative cap is below every |v|: a zero term becomes sign(0) * cap = 0 * -1 = -0.
        haptics->setForceCapX(-1.0f);
        haptics->setForceCapY(-1.0f);
        fake::sdk.clear();
        io::updateShakeForce(&pco, 0.0f, 0.0f, 0.0f, 0.0f);
        check(fake::sdk.joined() == "force 11 -0 -0 0;", "shake force: a negative cap clamps a zero term with sign 0");
        for (int i = 0; i < 6; ++i) g_hapticForceTargets[i] = 0;
    }

    // --- Soldier::handlePlayerInput's Novint "Recoil" block (0x5AE335..0x5AE72B, ledger N2 17b) -----
    {
        NovintHFX::Device* const saved = io::g_falconDevice;
        io::g_falconDevice = 0;
        check(!io::hapticDeviceConnected(), "recoil force: no device, not connected");
        io::g_falconDevice = saved;
        check(io::hapticDeviceConnected(), "recoil force: the fake device is connected");

        fake::sdk.clear();
        fake::constantEffects = 0;
        fake::constantParam18 = -1;
        io::updateRecoilForce(0.0f, 0.0f, 0);
        check(fake::sdk.joined() == "constant params constructed;stack;create Constant Recoil;params destroyed;force 10 0 0 0;",
              "recoil force: first call creates the Constant \"Recoil\" effect, then sets a zero force");
        check(fake::constantParam18 == 10 && g_hapticForceTargets[2], "recoil force: params +0x18 = 10, stored at 0x9EBF00");

        // Distinct scales per weapon type: yaw (t + 1) / 4, pitch (t + 1) / 8.
        typedef void (IHapticSettings::*Setter)(float);
        const Setter yawSetters[13] = {
            &IHapticSettings::setRecoilYawScaleRIFLE, &IHapticSettings::setRecoilYawScaleRIFLELAUNCHER,
            &IHapticSettings::setRecoilYawScaleCARBINE, &IHapticSettings::setRecoilYawScaleLMG,
            &IHapticSettings::setRecoilYawScaleSNIPER, &IHapticSettings::setRecoilYawScalePISTOL,
            &IHapticSettings::setRecoilYawScaleLAUNCHER, &IHapticSettings::setRecoilYawScaleSMG,
            &IHapticSettings::setRecoilYawScaleSHOTGUN, &IHapticSettings::setRecoilYawScaleKNIFE,
            &IHapticSettings::setRecoilYawScaleDROP, &IHapticSettings::setRecoilYawScaleTHROWN,
            &IHapticSettings::setRecoilYawScaleDEFIB };
        const Setter pitchSetters[13] = {
            &IHapticSettings::setRecoilPitchScaleRIFLE, &IHapticSettings::setRecoilPitchScaleRIFLELAUNCHER,
            &IHapticSettings::setRecoilPitchScaleCARBINE, &IHapticSettings::setRecoilPitchScaleLMG,
            &IHapticSettings::setRecoilPitchScaleSNIPER, &IHapticSettings::setRecoilPitchScalePISTOL,
            &IHapticSettings::setRecoilPitchScaleLAUNCHER, &IHapticSettings::setRecoilPitchScaleSMG,
            &IHapticSettings::setRecoilPitchScaleSHOTGUN, &IHapticSettings::setRecoilPitchScaleKNIFE,
            &IHapticSettings::setRecoilPitchScaleDROP, &IHapticSettings::setRecoilPitchScaleTHROWN,
            &IHapticSettings::setRecoilPitchScaleDEFIB };
        for (int t = 0; t < 13; ++t) {
            (haptics->*yawSetters[t])((t + 1) / 4.0f);
            (haptics->*pitchSetters[t])((t + 1) / 8.0f);
        }
        haptics->setForceCapX(100.0f);
        haptics->setForceCapY(100.0f);

        fake::sdk.clear();
        io::updateRecoilForce(0.0f, 0.0f, 3);
        check(fake::sdk.joined() == "force 10 0 0 0;", "recoil force: created once; both zero gives a zero force");

        bool everyType = true;
        for (int t = 0; t < 13; ++t) {
            fake::sdk.clear();
            io::updateRecoilForce(2.0f, 4.0f, t);
            const double ex = (t + 1) / 4.0 * 2.0, ey = -((t + 1) / 8.0 * 4.0);
            everyType = everyType && fake::sdk.calls.size() == 1
                        && fake::lastForce[0] == ex && fake::lastForce[1] == ey && fake::lastForce[2] == 0.0;
        }
        check(everyType, "recoil force: weapon types 0..12 select their own yaw and (negated) pitch scale");
        fake::sdk.clear();
        io::updateRecoilForce(2.0f, 4.0f, 13);
        io::updateRecoilForce(2.0f, 4.0f, -1);
        check(fake::sdk.joined() == "force 10 0 -0 0;force 10 0 -0 0;", "recoil force: other weapon types scale by 0");

        fake::sdk.clear();
        io::updateRecoilForce(0.5f, 0.0f, 0);   // RIFLE: 0.25 / 0.125
        io::updateRecoilForce(0.0f, 0.5f, 0);
        check(fake::sdk.joined() == "force 10 0.125 -0 0;force 10 0 -0.0625 0;", "recoil force: either value alone is enough");

        haptics->setForceCapX(1.5f);
        haptics->setForceCapY(3.0f);
        fake::sdk.clear();
        io::updateRecoilForce(2.0f, 3.0f, 0);
        io::updateRecoilForce(-2.0f, -4.0f, 0);
        check(fake::sdk.joined() == "force 10 0.375 -0.375 0;force 10 -0.375 0.375 0;",
              "recoil force: each value capped with its sign; equal to the cap stays");
        haptics->setForceCapX(-1.0f);
        fake::sdk.clear();
        io::updateRecoilForce(0.0f, 1.0f, 0);
        check(fake::sdk.joined() == "force 10 -0 -0.125 0;", "recoil force: a negative cap clamps a zero term with sign 0");
        // At cap 0 a -0 pitch is not above it and keeps its sign: -(0.125 * -0) = +0 (a clamp: -0).
        haptics->setForceCapX(100.0f);
        haptics->setForceCapY(0.0f);
        fake::sdk.clear();
        io::updateRecoilForce(1.0f, -0.0f, 0);
        check(fake::sdk.joined() == "force 10 0.25 0 0;", "recoil force: -0 pitch at a zero cap is left alone");
        for (int i = 0; i < 6; ++i) g_hapticForceTargets[i] = 0;
    }

    // --- Soldier::handlePlayerInput's Novint "CraftPhysics" block (0x5AECE8..0x5AF3F6, ledger N2 17b) --
    {
        haptics->setPhysicsSOLDIERScale(2.0f, 3.0f, 4.0f);
        haptics->setPhysicsSOLDIERScaleXLow(0.5f);  haptics->setPhysicsSOLDIERScaleYLow(0.25f); haptics->setPhysicsSOLDIERScaleZLow(0.125f);
        haptics->setPhysicsSOLDIERScaleXMed(5.0f);  haptics->setPhysicsSOLDIERScaleYMed(6.0f);  haptics->setPhysicsSOLDIERScaleZMed(7.0f);
        haptics->setPhysicsSOLDIERScaleXHigh(10.0f); haptics->setPhysicsSOLDIERScaleYHigh(20.0f); haptics->setPhysicsSOLDIERScaleZHigh(30.0f);
        haptics->setPhysicsSOLDIERTransitionValueMed(1.0f);
        haptics->setPhysicsSOLDIERTransitionValueHigh(10.0f);
        haptics->setPhysicsSOLDIERSmoothTimeXLow(11); haptics->setPhysicsSOLDIERSmoothTimeYLow(12); haptics->setPhysicsSOLDIERSmoothTimeZLow(13);
        haptics->setPhysicsSOLDIERSmoothTimeXMed(97); haptics->setPhysicsSOLDIERSmoothTimeYMed(97); haptics->setPhysicsSOLDIERSmoothTimeZMed(97);
        haptics->setPhysicsSOLDIERSmoothTimeXHigh(98); haptics->setPhysicsSOLDIERSmoothTimeYHigh(98); haptics->setPhysicsSOLDIERSmoothTimeZHigh(98);
        haptics->setForceCapX(1000.0f); haptics->setForceCapY(1000.0f); haptics->setForceCapZ(1000.0f);
        auto v3 = [](float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; };

        fake::sdk.clear();
        fake::constantEffects = 0;
        fake::params18.clear();
        io::updateSoldierCraftPhysicsForce(v3(0, 0, 0), 0.1f);
        check(fake::sdk.joined() ==
              "constant params constructed;stack;create Constant CraftPhysicsX;params destroyed;"
              "constant params constructed;stack;create Constant CraftPhysicsY;params destroyed;"
              "constant params constructed;stack;create Constant CraftPhysicsZ;params destroyed;"
              "smooth 10 11;force 10 -0 0 0;smooth 11 12;force 11 0 -0 0;smooth 12 13;force 12 0 0 0;",
              "craft physics: the three Constant effects are created once, then each gets its smooth time and axis");
        check(fake::params18 == "11,12,13," && g_hapticForceTargets[3] && g_hapticForceTargets[4] && g_hapticForceTargets[5],
              "craft physics: params +0x18 = the X/Y/Z Low smooth times; stored at 0x9EBF04/08/0C");
        fake::sdk.clear();
        io::updateSoldierCraftPhysicsForce(v3(1, 0, 0), 0.2f);
        io::updateSoldierCraftPhysicsForce(v3(2, 0, 0), 0.25f);
        const std::string zeroTick = "smooth 10 11;force 10 -0 0 0;smooth 11 12;force 11 0 -0 0;smooth 12 13;force 12 0 0 0;";
        check(fake::sdk.joined() == zeroTick + zeroTick, "craft physics: no acceleration until three samples are held");

        // History (1,0,0) (2,0,0) (4,8,-2); span 0.25 + 0.25: a = (oldest - newest) * 2 = (-6, -16, 4),
        // |a| = 17.5 >= 10: High scales (20, 60, 120); Low smooth times still.
        fake::sdk.clear();
        io::updateSoldierCraftPhysicsForce(v3(4, 8, -2), 0.25f);
        check(fake::sdk.joined() == "smooth 10 11;force 10 120 0 0;smooth 11 12;force 11 0 960 0;smooth 12 13;force 12 0 0 480;",
              "craft physics: oldest minus newest over the last two tick times; High band; -x, -y, +z");
        // (2,0,0) (4,8,-2) (2.5,1,0): a = (-1, -2, 0), |a| = 2.24: Med scales (10, 18, 28).
        fake::sdk.clear();
        io::updateSoldierCraftPhysicsForce(v3(2.5f, 1, 0), 0.25f);
        check(fake::sdk.joined() == "smooth 10 11;force 10 10 0 0;smooth 11 12;force 11 0 36 0;smooth 12 13;force 12 0 0 0;",
              "craft physics: Med band");
        // (4,8,-2) (2.5,1,0) (4,8.25,-2): a = (0, -0.5, 0), |a| = 0.5: Low scales (1, 0.75, 0.5).
        fake::sdk.clear();
        io::updateSoldierCraftPhysicsForce(v3(4, 8.25f, -2), 0.25f);
        check(fake::sdk.joined() == "smooth 10 11;force 10 -0 0 0;smooth 11 12;force 11 0 0.375 0;smooth 12 13;force 12 0 0 0;",
              "craft physics: Low band");
        // (2.5,1,0) (4,8.25,-2) (2.5,1.5,0): a = (0, -1, 0), |a| = 1 = Med's value: Med.
        fake::sdk.clear();
        io::updateSoldierCraftPhysicsForce(v3(2.5f, 1.5f, 0), 0.25f);
        check(fake::sdk.joined() == "smooth 10 11;force 10 -0 0 0;smooth 11 12;force 11 0 18 0;smooth 12 13;force 12 0 0 0;",
              "craft physics: a length equal to the Med value is Med");
        // (4,8.25,-2) (2.5,1.5,0) (0,0,10): a = (8, 16.5, -24), High; capped to (5, 10, -20) with sign.
        haptics->setForceCapX(5.0f); haptics->setForceCapY(10.0f); haptics->setForceCapZ(20.0f);
        fake::sdk.clear();
        io::updateSoldierCraftPhysicsForce(v3(0, 0, 10), 0.25f);
        check(fake::sdk.joined() == "smooth 10 11;force 10 -100 0 0;smooth 11 12;force 11 0 -600 0;smooth 12 13;force 12 0 0 -2400;",
              "craft physics: each acceleration component capped with its sign");
        // (2.5,1.5,0) (0,0,10) (2.5,-3.5,0): a = (0, 10, 0), |a| = 10 = High's value: High (60); y equal
        // to its cap is kept.
        fake::sdk.clear();
        io::updateSoldierCraftPhysicsForce(v3(2.5f, -3.5f, 0), 0.25f);
        check(fake::sdk.joined() == "smooth 10 11;force 10 -0 0 0;smooth 11 12;force 11 0 -600 0;smooth 12 13;force 12 0 0 0;",
              "craft physics: a length equal to the High value is High; a component equal to its cap is kept");
        // (0,0,9) (0,0,5) (0,0,0) over 0.05 + 0.1: 1 / 0.15 stays unrounded for z (0x5A6370), so
        // a.z = 59.999996f, not the 60 a rounded reciprocal gives; High: z force 120 * a.z.
        haptics->setForceCapX(1e6f); haptics->setForceCapY(1e6f); haptics->setForceCapZ(1e6f);
        io::updateSoldierCraftPhysicsForce(v3(0, 0, 9), 0.25f);
        io::updateSoldierCraftPhysicsForce(v3(0, 0, 5), 0.05f);
        io::updateSoldierCraftPhysicsForce(v3(0, 0, 0), 0.1f);
        const float span = 0.05f + 0.1f;
        const float unrounded = static_cast<float>((1.0 / span) * 9.0f);
        check(unrounded != static_cast<float>(1.0 / span) * 9.0f && fake::lastForce[2] == 120.0 * unrounded,
              "craft physics: z uses the unrounded reciprocal of the span");
        // (0,0,5) (0,0,0) (0,0,4.8) over 0.1 + 0.25: a.z = 0.2 / 0.35, Low: z force 0.5 * a.z.
        io::updateSoldierCraftPhysicsForce(v3(0, 0, 4.8f), 0.25f);
        check(std::fabs(fake::lastForce[2] - 0.5 * (0.2 / 0.35)) < 1e-5, "craft physics: Low band z scale");
        // (0,-0,0) (0,0,4.8) (0,0,0): a.y = -0; at cap 0 it is not above it and keeps its sign, so
        // the y force is -(0.75 * -0) = +0 (a clamp would store sign(-0) * 0 = +0, giving -0).
        io::updateSoldierCraftPhysicsForce(v3(0, -0.0f, 0), 0.25f);
        io::updateSoldierCraftPhysicsForce(v3(0, 0, 4.8f), 0.25f);
        haptics->setForceCapY(0.0f);
        fake::sdk.clear();
        io::updateSoldierCraftPhysicsForce(v3(0, 0, 0), 0.25f);
        check(fake::sdk.joined() == "smooth 10 11;force 10 -0 0 0;smooth 11 12;force 11 0 0 0;smooth 12 13;force 12 0 0 0;",
              "craft physics: -0 at a zero cap is left alone");
        for (int i = 0; i < 6; ++i) g_hapticForceTargets[i] = 0;
    }

    // --- PlayerControlObject::handleUpdate's CraftPhysics (0x5FADAE..0x5FBCD8): categories, band smooth times --
    {
        auto v3 = [](float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; };
        // c = 0 LAND, 1 SEA, 2 AIR, 3 HELI: Z scale (c+1) / (c+1)*10 / (c+1)*100, smooth (c+1)*100 + 1..9.
        haptics->setPhysicsLANDScale(1.0f, 1.0f, 1.0f);
        haptics->setPhysicsLANDScaleXLow(1.0f); haptics->setPhysicsLANDScaleYLow(1.0f); haptics->setPhysicsLANDScaleZLow(1.0f);
        haptics->setPhysicsLANDScaleXMed(1.0f); haptics->setPhysicsLANDScaleYMed(1.0f); haptics->setPhysicsLANDScaleZMed(10.0f);
        haptics->setPhysicsLANDScaleXHigh(1.0f); haptics->setPhysicsLANDScaleYHigh(1.0f); haptics->setPhysicsLANDScaleZHigh(100.0f);
        haptics->setPhysicsLANDTransitionValueMed(1.0f); haptics->setPhysicsLANDTransitionValueHigh(10.0f);
        haptics->setPhysicsLANDSmoothTimeXLow(101); haptics->setPhysicsLANDSmoothTimeYLow(102); haptics->setPhysicsLANDSmoothTimeZLow(103);
        haptics->setPhysicsLANDSmoothTimeXMed(104); haptics->setPhysicsLANDSmoothTimeYMed(105); haptics->setPhysicsLANDSmoothTimeZMed(106);
        haptics->setPhysicsLANDSmoothTimeXHigh(107); haptics->setPhysicsLANDSmoothTimeYHigh(108); haptics->setPhysicsLANDSmoothTimeZHigh(109);
        haptics->setPhysicsSEAScale(1.0f, 1.0f, 1.0f);
        haptics->setPhysicsSEAScaleXLow(1.0f); haptics->setPhysicsSEAScaleYLow(1.0f); haptics->setPhysicsSEAScaleZLow(2.0f);
        haptics->setPhysicsSEAScaleXMed(1.0f); haptics->setPhysicsSEAScaleYMed(1.0f); haptics->setPhysicsSEAScaleZMed(20.0f);
        haptics->setPhysicsSEAScaleXHigh(1.0f); haptics->setPhysicsSEAScaleYHigh(1.0f); haptics->setPhysicsSEAScaleZHigh(200.0f);
        haptics->setPhysicsSEATransitionValueMed(1.0f); haptics->setPhysicsSEATransitionValueHigh(10.0f);
        haptics->setPhysicsSEASmoothTimeXLow(201); haptics->setPhysicsSEASmoothTimeYLow(202); haptics->setPhysicsSEASmoothTimeZLow(203);
        haptics->setPhysicsSEASmoothTimeXMed(204); haptics->setPhysicsSEASmoothTimeYMed(205); haptics->setPhysicsSEASmoothTimeZMed(206);
        haptics->setPhysicsSEASmoothTimeXHigh(207); haptics->setPhysicsSEASmoothTimeYHigh(208); haptics->setPhysicsSEASmoothTimeZHigh(209);
        haptics->setPhysicsAIRScale(1.0f, 1.0f, 1.0f);
        haptics->setPhysicsAIRScaleXLow(1.0f); haptics->setPhysicsAIRScaleYLow(1.0f); haptics->setPhysicsAIRScaleZLow(3.0f);
        haptics->setPhysicsAIRScaleXMed(1.0f); haptics->setPhysicsAIRScaleYMed(1.0f); haptics->setPhysicsAIRScaleZMed(30.0f);
        haptics->setPhysicsAIRScaleXHigh(1.0f); haptics->setPhysicsAIRScaleYHigh(1.0f); haptics->setPhysicsAIRScaleZHigh(300.0f);
        haptics->setPhysicsAIRTransitionValueMed(1.0f); haptics->setPhysicsAIRTransitionValueHigh(10.0f);
        haptics->setPhysicsAIRSmoothTimeXLow(301); haptics->setPhysicsAIRSmoothTimeYLow(302); haptics->setPhysicsAIRSmoothTimeZLow(303);
        haptics->setPhysicsAIRSmoothTimeXMed(304); haptics->setPhysicsAIRSmoothTimeYMed(305); haptics->setPhysicsAIRSmoothTimeZMed(306);
        haptics->setPhysicsAIRSmoothTimeXHigh(307); haptics->setPhysicsAIRSmoothTimeYHigh(308); haptics->setPhysicsAIRSmoothTimeZHigh(309);
        haptics->setPhysicsHELIScale(1.0f, 1.0f, 1.0f);
        haptics->setPhysicsHELIScaleXLow(1.0f); haptics->setPhysicsHELIScaleYLow(1.0f); haptics->setPhysicsHELIScaleZLow(4.0f);
        haptics->setPhysicsHELIScaleXMed(1.0f); haptics->setPhysicsHELIScaleYMed(1.0f); haptics->setPhysicsHELIScaleZMed(40.0f);
        haptics->setPhysicsHELIScaleXHigh(1.0f); haptics->setPhysicsHELIScaleYHigh(1.0f); haptics->setPhysicsHELIScaleZHigh(400.0f);
        haptics->setPhysicsHELITransitionValueMed(1.0f); haptics->setPhysicsHELITransitionValueHigh(10.0f);
        haptics->setPhysicsHELISmoothTimeXLow(401); haptics->setPhysicsHELISmoothTimeYLow(402); haptics->setPhysicsHELISmoothTimeZLow(403);
        haptics->setPhysicsHELISmoothTimeXMed(404); haptics->setPhysicsHELISmoothTimeYMed(405); haptics->setPhysicsHELISmoothTimeZMed(406);
        haptics->setPhysicsHELISmoothTimeXHigh(407); haptics->setPhysicsHELISmoothTimeYHigh(408); haptics->setPhysicsHELISmoothTimeZHigh(409);
        haptics->setForceCapX(1e6f); haptics->setForceCapY(1e6f); haptics->setForceCapZ(1e6f);
        for (int i = 0; i < 6; ++i) g_hapticForceTargets[i] = 0;
        auto tick = [](int x, int y, int z, const char* fz) {
            return "smooth 10 " + std::to_string(x) + ";force 10 -0 0 0;smooth 11 " + std::to_string(y)
                   + ";force 11 0 -0 0;smooth 12 " + std::to_string(z) + ";force 12 0 0 " + fz + ";";
        };
        fake::sdk.clear();
        fake::constantEffects = 0;
        fake::params18.clear();
        io::updateVehicleCraftPhysicsForce(v3(0, 0, 0), 0.25f, 0);
        check(fake::params18 == "101,102,103," && fake::sdk.calls.size() == 18
              && fake::sdk.calls[12] == "smooth 10 101" && fake::sdk.calls[17] == "force 12 0 0 0",
              "vehicle craft physics: its own effects-creation pass; LAND's Low smooth times");
        fake::sdk.clear();
        io::updateVehicleCraftPhysicsForce(v3(0, 0, 0), 0.25f, 1);
        io::updateVehicleCraftPhysicsForce(v3(0, 0, 0), 0.25f, 2);
        check(fake::sdk.joined() == tick(201, 202, 203, "0") + tick(301, 302, 303, "0"),
              "vehicle craft physics: SEA and AIR Low smooth times while the history fills");
        // (0,0,0) (0,0,0) (0,0,-1): a.z = 2, Med: AIR z scale 30, Med smooth times.
        fake::sdk.clear();
        io::updateVehicleCraftPhysicsForce(v3(0, 0, -1), 0.25f, 2);
        check(fake::sdk.joined() == tick(304, 305, 306, "60"), "vehicle craft physics: AIR, Med band and Med smooth times");
        fake::sdk.clear();
        io::updateVehicleCraftPhysicsForce(v3(0, 0, -1), 0.25f, 3);
        check(fake::sdk.joined() == tick(404, 405, 406, "80"), "vehicle craft physics: HELI, Med");
        fake::sdk.clear();
        io::updateVehicleCraftPhysicsForce(v3(0, 0, -1), 0.25f, 1);
        check(fake::sdk.joined() == tick(201, 202, 203, "0"), "vehicle craft physics: SEA, Low (no acceleration)");
        // (0,0,-1) (0,0,-1) (0,0,-11): a.z = 20, High; category 7 falls back to LAND.
        fake::sdk.clear();
        io::updateVehicleCraftPhysicsForce(v3(0, 0, -11), 0.25f, 7);
        check(fake::sdk.joined() == tick(107, 108, 109, "2000"), "vehicle craft physics: an unknown category is LAND; High band and High smooth times");
        for (int i = 0; i < 6; ++i) g_hapticForceTargets[i] = 0;
    }

    // --- The RecoilPunch envelope (0x6095EF..0x60968A) and SoldierBasedRecoilComp's choice (0x609353..) --
    {
        haptics->setForceCapZ(5.0f);
        fake::sdk.clear();
        io::fireRecoilPunch(2.9f, 0.5f, 3.0f);
        check(fake::sdk.joined() == "envelope params constructed;stack;create Envelope RecoilPunch;envelope 2 -777 -777 1.5;params destroyed;",
              "recoil punch: Envelope params, time truncated, z = scale * strength below the cap, not kept, deleted");
        fake::sdk.clear();
        io::fireRecoilPunch(-2.9f, 0.5f, 10.0f);
        io::fireRecoilPunch(1.0f, 0.5f, -10.0f);
        io::fireRecoilPunch(1.0f, 0.5f, -5.0f);
        std::string got;
        for (const std::string& c : fake::sdk.calls) if (c.compare(0, 9, "envelope ") == 0 && c.compare(0, 15, "envelope params") != 0) got += c + ";";
        check(got == "envelope -2 -777 -777 2.5;envelope 1 -777 -777 2.5;envelope 1 -777 -777 -2.5;",
              "recoil punch: truncation toward zero; above the Z cap z = cap * scale without the sign; equal stays");

        // Punch time t+1, scale (t+1)*10 per HapticSettings type; PISTOL also takes 5, 13 and -1.
        typedef void (IHapticSettings::*Setter)(float);
        const Setter times[13] = {
            &IHapticSettings::setRecoilPunchTimeRIFLE, &IHapticSettings::setRecoilPunchTimeRIFLELAUNCHER,
            &IHapticSettings::setRecoilPunchTimeCARBINE, &IHapticSettings::setRecoilPunchTimeLMG,
            &IHapticSettings::setRecoilPunchTimeSNIPER, &IHapticSettings::setRecoilPunchTimePISTOL,
            &IHapticSettings::setRecoilPunchTimeLAUNCHER, &IHapticSettings::setRecoilPunchTimeSMG,
            &IHapticSettings::setRecoilPunchTimeSHOTGUN, &IHapticSettings::setRecoilPunchTimeKNIFE,
            &IHapticSettings::setRecoilPunchTimeDROP, &IHapticSettings::setRecoilPunchTimeTHROWN,
            &IHapticSettings::setRecoilPunchTimeDEFIB };
        const Setter scales[13] = {
            &IHapticSettings::setRecoilPunchScaleRIFLE, &IHapticSettings::setRecoilPunchScaleRIFLELAUNCHER,
            &IHapticSettings::setRecoilPunchScaleCARBINE, &IHapticSettings::setRecoilPunchScaleLMG,
            &IHapticSettings::setRecoilPunchScaleSNIPER, &IHapticSettings::setRecoilPunchScalePISTOL,
            &IHapticSettings::setRecoilPunchScaleLAUNCHER, &IHapticSettings::setRecoilPunchScaleSMG,
            &IHapticSettings::setRecoilPunchScaleSHOTGUN, &IHapticSettings::setRecoilPunchScaleKNIFE,
            &IHapticSettings::setRecoilPunchScaleDROP, &IHapticSettings::setRecoilPunchScaleTHROWN,
            &IHapticSettings::setRecoilPunchScaleDEFIB };
        for (int t = 0; t < 13; ++t) { (haptics->*times[t])(t + 1.0f); (haptics->*scales[t])((t + 1) * 10.0f); }
        haptics->setForceCapZ(1e6f);
        bool everyType = true;
        for (int t = 0; t < 13; ++t) {
            fake::sdk.clear();
            io::soldierRecoilPunch(t, false, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f);
            char want[96];
            std::snprintf(want, sizeof(want), "envelope %d -777 -777 %.17g", t + 1, (t + 1) * 10.0);
            everyType = everyType && fake::sdk.calls.size() == 5 && fake::sdk.calls[3] == want;
        }
        check(everyType, "recoil punch: weapon types 0..12 select their own punch time and scale");
        fake::sdk.clear();
        io::soldierRecoilPunch(13, false, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f);
        io::soldierRecoilPunch(-1, false, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f);
        got.clear();
        for (const std::string& c : fake::sdk.calls) if (c.compare(0, 9, "envelope ") == 0 && c.compare(0, 15, "envelope params") != 0) got += c + ";";
        check(got == "envelope 6 -777 -777 60;envelope 6 -777 -777 60;", "recoil punch: other weapon types use PISTOL's");

        // With a random recoil force: time *= 4 samples; scale = -(((3 + 1) * 0.5) * 2 * 0.5) * 10 = -20.
        const float graph[4] = { 9.0f, 9.0f, 9.0f, 0.5f };
        fake::sdk.clear();
        io::soldierRecoilPunch(0, true, 4, &graph[3], 1.0f, 3.0f, 2.0f, 1.5f);
        check(fake::sdk.calls.size() == 5 && fake::sdk.calls[3] == "envelope 4 -777 -777 -30",
              "recoil punch: a random recoil force scales the time by the samples and the scale by the graph's last sample");
    }

    // --- WeaponBasedRecoilComp's RecoilPunch choice (0x60A0E7..0x60A4F8) ------------------------------
    {
        typedef void (IHapticSettings::*Setter)(float);
        struct V { Setter time, scale, yaw, pitch; };
        const V types[7] = {
            { &IHapticSettings::setRecoilPunchTimeV_LMG, &IHapticSettings::setRecoilPunchScaleV_LMG, &IHapticSettings::setRecoilYawScaleV_LMG, &IHapticSettings::setRecoilPitchScaleV_LMG },
            { &IHapticSettings::setRecoilPunchTimeV_HMG, &IHapticSettings::setRecoilPunchScaleV_HMG, &IHapticSettings::setRecoilYawScaleV_HMG, &IHapticSettings::setRecoilPitchScaleV_HMG },
            { &IHapticSettings::setRecoilPunchTimeV_HELIGUN, &IHapticSettings::setRecoilPunchScaleV_HELIGUN, &IHapticSettings::setRecoilYawScaleV_HELIGUN, &IHapticSettings::setRecoilPitchScaleV_HELIGUN },
            { &IHapticSettings::setRecoilPunchTimeV_AIRGUN, &IHapticSettings::setRecoilPunchScaleV_AIRGUN, &IHapticSettings::setRecoilYawScaleV_AIRGUN, &IHapticSettings::setRecoilPitchScaleV_AIRGUN },
            { &IHapticSettings::setRecoilPunchTimeV_MISSILE, &IHapticSettings::setRecoilPunchScaleV_MISSILE, &IHapticSettings::setRecoilYawScaleV_MISSILE, &IHapticSettings::setRecoilPitchScaleV_MISSILE },
            { &IHapticSettings::setRecoilPunchTimeV_ARMORCANNON, &IHapticSettings::setRecoilPunchScaleV_ARMORCANNON, &IHapticSettings::setRecoilYawScaleV_ARMORCANNON, &IHapticSettings::setRecoilPitchScaleV_ARMORCANNON },
            { &IHapticSettings::setRecoilPunchTimeV_COAXIALGUN, &IHapticSettings::setRecoilPunchScaleV_COAXIALGUN, &IHapticSettings::setRecoilYawScaleV_COAXIALGUN, &IHapticSettings::setRecoilPitchScaleV_COAXIALGUN } };
        for (int k = 0; k < 7; ++k) {
            (haptics->*types[k].time)(k + 1.0f);
            (haptics->*types[k].scale)((k + 1) * 10.0f);
            (haptics->*types[k].yaw)((k + 1) * 0.25f);
            (haptics->*types[k].pitch)((k + 1) * 0.125f);
        }
        haptics->setRecoilPunchTimeRIFLE(3.0f); haptics->setRecoilPunchScaleRIFLE(7.0f);
        haptics->setRecoilYawScaleRIFLE(0.75f); haptics->setRecoilPitchScaleRIFLE(0.375f);
        haptics->setForceCapZ(1e6f);
        auto envelope = [&]() {
            for (const std::string& c : fake::sdk.calls)
                if (c.compare(0, 9, "envelope ") == 0 && c.compare(0, 15, "envelope params") != 0) return c;
            return std::string("(none)");
        };
        bool everyType = true;
        for (int k = 0; k < 7; ++k) {
            fake::sdk.clear();
            haptics->setUnresolved48C(-1.0f); haptics->setUnresolved490(-1.0f);
            io::weaponRecoilPunch(13 + k, "tankgun", 2.0f, 0.5f, 1.0f);   // time (k+1)*2, scale (k+1)*10*0.5
            char want[96];
            std::snprintf(want, sizeof(want), "envelope %d -777 -777 %.17g", (k + 1) * 2, (k + 1) * 5.0);
            everyType = everyType && envelope() == want && haptics->getUnresolved48C() == (k + 1) * 0.25f
                        && haptics->getUnresolved490() == (k + 1) * 0.125f;
        }
        check(everyType, "weapon recoil punch: types 13..19 take their own time and scale and set +0x48C/+0x490");
        fake::sdk.clear();
        io::weaponRecoilPunch(4, "US_FiringPort_M16", 2.0f, 0.5f, 1.0f);
        check(envelope() == "envelope 6 -777 -777 3.5" && haptics->getUnresolved48C() == 0.75f && haptics->getUnresolved490() == 0.375f,
              "weapon recoil punch: another type named *firingport* (any case) takes RIFLE's four values");
        fake::sdk.clear();
        io::weaponRecoilPunch(20, "tankgun", 2.9f, 0.5f, 1.0f);
        check(envelope() == "envelope 2 -777 -777 0" && haptics->getUnresolved48C() == 0.0f && haptics->getUnresolved490() == 0.0f,
              "weapon recoil punch: otherwise time 1 (times the speed), scale 0, and both set to 0");
        fake::sdk.clear();
        io::weaponRecoilPunch(12, "firingpor", 1.0f, 1.0f, 1.0f);
        check(envelope() == "envelope 1 -777 -777 0", "weapon recoil punch: types below 13 are not vehicle types");
    }

    // --- GenericFireArm::postFire's kit RecoilPunch (0x5E2A98..0x5E2CDA) -------------------------------
    {
        haptics->setRecoilPunchTimeKNIFE(2.0f);  haptics->setRecoilPunchScaleKNIFE(20.0f);
        haptics->setRecoilPunchTimeDROP(3.0f);   haptics->setRecoilPunchScaleDROP(30.0f);
        haptics->setRecoilPunchTimeTHROWN(4.0f); haptics->setRecoilPunchScaleTHROWN(40.0f);
        haptics->setRecoilPunchTimeDEFIB(5.0f);  haptics->setRecoilPunchScaleDEFIB(50.0f);
        haptics->setForceCapZ(1e6f);
        auto envelopes = [&]() {
            std::string r;
            for (const std::string& c : fake::sdk.calls)
                if (c.compare(0, 9, "envelope ") == 0 && c.compare(0, 15, "envelope params") != 0) r += c + ";";
            return r;
        };
        fake::sdk.clear();
        for (int t = 9; t <= 12; ++t) io::kitRecoilPunch(t, "rifle", 1.0f);
        check(envelopes() == "envelope 2 -777 -777 20;envelope 3 -777 -777 30;envelope 4 -777 -777 40;envelope 5 -777 -777 50;",
              "kit recoil punch: types 9..12 take KNIFE, DROP, THROWN, DEFIB");
        fake::sdk.clear();
        io::kitRecoilPunch(0, "US_MedKit", 1.0f);
        io::kitRecoilPunch(13, "ch_WRENCH_2", 1.0f);
        io::kitRecoilPunch(8, "knife", 1.0f);
        io::kitRecoilPunch(8, "ki", 2.0f);
        check(envelopes() == "envelope 3 -777 -777 30;envelope 3 -777 -777 30;envelope 1 -777 -777 0;envelope 1 -777 -777 0;",
              "kit recoil punch: other types named *kit* or *wrench* (any case) take DROP's, else time 1 and scale 0");
    }

    // --- GameLogicDamage::serverGiveDamage's "Damage" envelope (0x5E6566..0x5E66A2) ---------------------
    {
        Mat4 lookAt, owner;
        for (int i = 0; i < 16; ++i) { lookAt.m[i] = 0.0f; owner.m[i] = 0.0f; }
        lookAt.m[8] = 0.48f; lookAt.m[9] = 0.6f; lookAt.m[10] = 0.64f;    // the forward row
        owner.m[0] = owner.m[5] = owner.m[10] = owner.m[15] = 1.0f;
        haptics->setDamageScale(2.0f);
        world::IPlayer* const local = reinterpret_cast<world::IPlayer*>(0x1000);
        world::IPlayer* const other = reinterpret_cast<world::IPlayer*>(0x2000);
        fake::sdk.clear();
        io::updateDamageForce(other, local, lookAt, owner, 10.0f);
        check(fake::sdk.calls.empty(), "damage: another victim, nothing");
        NovintHFX::Device* const saved = io::g_falconDevice;
        io::g_falconDevice = 0;
        io::updateDamageForce(local, local, lookAt, owner, 10.0f);
        io::g_falconDevice = saved;
        check(fake::sdk.calls.empty(), "damage: no device, nothing");
        io::updateDamageForce(local, local, lookAt, owner, 10.0f);
        // identity owner: (a, b, c) = (0.48, 0.6, 0.64); direction (-0.48, 0.6, 0.64) * (2 * 10 * 0.5)
        const NovintHFX::Vect3 expected = NovintHFX::Vect3(-static_cast<double>(0.48f), static_cast<double>(0.6f), static_cast<double>(0.64f)).Normalize() * 10.0;
        NovintHFX::Vect3& e = const_cast<NovintHFX::Vect3&>(expected);
        char want[160];
        std::snprintf(want, sizeof(want), "envelope 100 %.17g %.17g %.17g", e[0], e[1], e[2]);
        check(fake::sdk.joined() == std::string("envelope params constructed;stack;create Envelope Damage;") + want + ";params destroyed;",
              "damage: Envelope params +0x18 = 100, the normalized (-a, b, c) times scale * damage * 0.5, not kept");
        // Rotated owner: rows (0,0,1) (1,0,0) (0,1,0): a = 0.64, b = 0.48, c = 0.6.
        for (int i = 0; i < 16; ++i) owner.m[i] = 0.0f;
        owner.m[2] = 1.0f; owner.m[4] = 1.0f; owner.m[9] = 1.0f; owner.m[15] = 1.0f;
        fake::sdk.clear();
        io::updateDamageForce(local, local, lookAt, owner, 10.0f);
        const NovintHFX::Vect3 rotated = NovintHFX::Vect3(-static_cast<double>(0.64f), static_cast<double>(0.48f), static_cast<double>(0.6f)).Normalize() * 10.0;
        NovintHFX::Vect3& r = const_cast<NovintHFX::Vect3&>(rotated);
        std::snprintf(want, sizeof(want), "envelope 100 %.17g %.17g %.17g", r[0], r[1], r[2]);
        check(fake::sdk.calls.size() == 5 && fake::sdk.calls[3] == want, "damage: the forward row dotted with the owner's rows 0, 1, 2");
    }

    device->release();
    std::printf("falcon update runtime %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
