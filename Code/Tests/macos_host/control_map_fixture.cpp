// [TEMP] Scaffolding for macos_host_test. Supplies the two engine globals the macOS input
// devices read (io::inputDeviceManager, io::controlMapManager; production definitions live in
// Core/Globals.cpp) and a control-map manager whose active map answers only what
// NativeMouseDevice asks: per-axis scale 1, mouse sensitivity 1, no inversion. Every other method
// reports itself and aborts; the fixture models nothing else.

#include "../../BF2/IO/Input/Input.h"
#include <cstdio>
#include <cstdlib>

namespace dice { namespace hfe { namespace io {

IBase* inputDeviceManager = 0;
IBase* controlMapManager  = 0;

namespace {

[[noreturn]] void unsupported(const char* name)
{
    std::fprintf(stderr, "[TEMP] macos_host_test control-map fixture: %s is not modelled\n", name);
    std::fflush(stderr);
    std::abort();
}

class FixtureControlMap final : public IControlMap
{
public:
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
    bool getInvertMouse() override { return false; }
    void setMouseSensitivity(float) override { unsupported("setMouseSensitivity"); }
    float getMouseSensitivity() override { return 1.0f; }
    void setKeyboardSensitivity(float) override { unsupported("setKeyboardSensitivity"); }
    float getKeyboardSensitivity() override { unsupported("getKeyboardSensitivity"); }
    void setAxisScale(InputDeviceAxes, float) override { unsupported("setAxisScale"); }
    float getAxisScale(InputDeviceAxes) const override { return 1.0f; }
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

class FixtureControlMapManager final : public IControlMapManager
{
public:
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return 0; }
    void update(float) override { unsupported("ControlMapManager::update"); }
    IControlMap* create(const std::string&) override { unsupported("create"); }
    IControlMap* getControlMap(const std::string&) const override { unsupported("getControlMap"); }
    IControlMap* getControlMap(int) const override { unsupported("getControlMap"); }
    void deleteControlMap(const std::string&) override { unsupported("deleteControlMap"); }
    void deleteControlMap(int) override { unsupported("deleteControlMap"); }
    void deleteControlMap(const IControlMap*) override { unsupported("deleteControlMap"); }
    void setActive(const std::string&) override { unsupported("setActive"); }
    void setActive(const IControlMap*) override { unsupported("setActive"); }
    IControlMap* getActive() const override { return const_cast<FixtureControlMap*>(&m_map); }
    int mapNextInput(IControlMap*, int, bool, bool, bool, bool, std::string&) override { unsupported("mapNextInput"); }
    float getRepeatRate(int, bool) override { unsupported("getRepeatRate"); }
    void applyActive() override { unsupported("applyActive"); }
private:
    FixtureControlMap m_map;
};

FixtureControlMapManager g_fixtureManager;

} // namespace

void installControlMapFixture() { controlMapManager = &g_fixtureManager; }

} } }
