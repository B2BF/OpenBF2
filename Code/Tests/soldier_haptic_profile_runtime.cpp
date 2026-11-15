// [TEMP] [CHOSEN, not recovered] The Soldier's control-box profile switch
// (Game/Objects/Game/SoldierHapticProfile.cpp, ledger N2 17a) against a complete fake HapticSettings and
// fake player-control templates (CTest bf2_soldier_haptic_profile_check). The fakes are generated from
// their headers and fail loudly for every call the code under test must not make.
#include "../BF2/Game/Objects/Game/SoldierHapticProfile.h"
#include "../BF2/Game/HapticSettings.h"
#include "../BF2/Game/Objects/IPlayerControlObjectTemplate.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace dice::hfe;
using namespace dice::hfe::world;

static unsigned checks, failures;
static void check(bool value, const std::string& label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label.c_str()); }
}
[[noreturn]] static void unexpected(const char* name)
{
    std::fprintf(stderr, "[TEMP] soldier_haptic_profile check: unexpected call %s\n", name);
    std::fflush(stderr);
    std::abort();
}
static std::vector<std::string> calls;
static Vec3 v3(float x, float y, float z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}
static std::string fmt(float x, float y, float z)
{
    char b[64];
    std::snprintf(b, sizeof(b), "%g %g %g", x, y, z);
    return b;
}

namespace dice { namespace hfe {
struct FakeHapticSettings final : IHapticSettings {
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("queryInterface"); }
    std::string getFileName() override { unexpected("IHapticSettings::getFileName"); }
    std::string getSettingsScript() override { unexpected("IHapticSettings::getSettingsScript"); }
    bool apply() override { unexpected("IHapticSettings::apply"); }
    void setDefaults() override { unexpected("IHapticSettings::setDefaults"); }
    void setDefaultValues() override { unexpected("IHapticSettings::setDefaultValues"); }
    void loadCurrentProfileFile() override { unexpected("IHapticSettings::loadCurrentProfileFile"); }
    bool reloadCurrentProfileFile() override { unexpected("IHapticSettings::reloadCurrentProfileFile"); }
    float getForceCapX() override { unexpected("IHapticSettings::getForceCapX"); }
    void setForceCapX(float) override { unexpected("IHapticSettings::setForceCapX"); }
    float getForceCapY() override { unexpected("IHapticSettings::getForceCapY"); }
    void setForceCapY(float) override { unexpected("IHapticSettings::setForceCapY"); }
    float getForceCapZ() override { unexpected("IHapticSettings::getForceCapZ"); }
    void setForceCapZ(float) override { unexpected("IHapticSettings::setForceCapZ"); }
    Vec3 getControlBoxLength() override { unexpected("IHapticSettings::getControlBoxLength"); }
    Vec3 getControlBoxStiffness() override { unexpected("IHapticSettings::getControlBoxStiffness"); }
    void setControlBoxLengthSOLDIER(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxLengthSOLDIER"); }
    void setControlBoxStiffnessSOLDIER(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxStiffnessSOLDIER"); }
    void setControlBoxLengthAIR(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxLengthAIR"); }
    void setControlBoxStiffnessAIR(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxStiffnessAIR"); }
    void setControlBoxLengthHELI(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxLengthHELI"); }
    void setControlBoxStiffnessHELI(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxStiffnessHELI"); }
    void setControlBoxLengthLAND(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxLengthLAND"); }
    void setControlBoxStiffnessLAND(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxStiffnessLAND"); }
    void setControlBoxLengthSEA(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxLengthSEA"); }
    void setControlBoxStiffnessSEA(float x, float y, float z) override { unexpected("IHapticSettings::setControlBoxStiffnessSEA"); }
    Vec3 getAimSensitivity() override { unexpected("IHapticSettings::getAimSensitivity"); }
    Vec3 getAimSensitivityLength() override { unexpected("IHapticSettings::getAimSensitivityLength"); }
    Vec3 getTurnSensitivity() override { unexpected("IHapticSettings::getTurnSensitivity"); }
    void setAimSensitivitySOLDIER(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivitySOLDIER"); }
    void setAimSensitivityLengthSOLDIER(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivityLengthSOLDIER"); }
    void setTurnSensitivitySOLDIER(float x, float y, float z) override { unexpected("IHapticSettings::setTurnSensitivitySOLDIER"); }
    void setAimSensitivityAIR(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivityAIR"); }
    void setAimSensitivityLengthAIR(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivityLengthAIR"); }
    void setTurnSensitivityAIR(float x, float y, float z) override { unexpected("IHapticSettings::setTurnSensitivityAIR"); }
    void setAimSensitivityHELI(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivityHELI"); }
    void setAimSensitivityLengthHELI(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivityLengthHELI"); }
    void setTurnSensitivityHELI(float x, float y, float z) override { unexpected("IHapticSettings::setTurnSensitivityHELI"); }
    void setAimSensitivityLAND(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivityLAND"); }
    void setAimSensitivityLengthLAND(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivityLengthLAND"); }
    void setTurnSensitivityLAND(float x, float y, float z) override { unexpected("IHapticSettings::setTurnSensitivityLAND"); }
    void setAimSensitivitySEA(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivitySEA"); }
    void setAimSensitivityLengthSEA(float x, float y, float z) override { unexpected("IHapticSettings::setAimSensitivityLengthSEA"); }
    void setTurnSensitivitySEA(float x, float y, float z) override { unexpected("IHapticSettings::setTurnSensitivitySEA"); }
    Vec3 getPhysicsSOLDIERScale() override { unexpected("IHapticSettings::getPhysicsSOLDIERScale"); }
    void setPhysicsSOLDIERScale(float x, float y, float z) override { unexpected("IHapticSettings::setPhysicsSOLDIERScale"); }
    float getPhysicsSOLDIERScaleXLow() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleXLow"); }
    void setPhysicsSOLDIERScaleXLow(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleXLow"); }
    float getPhysicsSOLDIERScaleYLow() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleYLow"); }
    void setPhysicsSOLDIERScaleYLow(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleYLow"); }
    float getPhysicsSOLDIERScaleZLow() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleZLow"); }
    void setPhysicsSOLDIERScaleZLow(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleZLow"); }
    float getPhysicsSOLDIERScaleXMed() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleXMed"); }
    void setPhysicsSOLDIERScaleXMed(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleXMed"); }
    float getPhysicsSOLDIERScaleYMed() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleYMed"); }
    void setPhysicsSOLDIERScaleYMed(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleYMed"); }
    float getPhysicsSOLDIERScaleZMed() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleZMed"); }
    void setPhysicsSOLDIERScaleZMed(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleZMed"); }
    float getPhysicsSOLDIERScaleXHigh() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleXHigh"); }
    void setPhysicsSOLDIERScaleXHigh(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleXHigh"); }
    float getPhysicsSOLDIERScaleYHigh() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleYHigh"); }
    void setPhysicsSOLDIERScaleYHigh(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleYHigh"); }
    float getPhysicsSOLDIERScaleZHigh() override { unexpected("IHapticSettings::getPhysicsSOLDIERScaleZHigh"); }
    void setPhysicsSOLDIERScaleZHigh(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERScaleZHigh"); }
    float getPhysicsSOLDIERTransitionValue(int level) override { unexpected("IHapticSettings::getPhysicsSOLDIERTransitionValue"); }
    void setPhysicsSOLDIERTransitionValue(int level, float value) override { unexpected("IHapticSettings::setPhysicsSOLDIERTransitionValue"); }
    float getPhysicsSOLDIERTransitionValueMed() override { unexpected("IHapticSettings::getPhysicsSOLDIERTransitionValueMed"); }
    void setPhysicsSOLDIERTransitionValueMed(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERTransitionValueMed"); }
    float getPhysicsSOLDIERTransitionValueHigh() override { unexpected("IHapticSettings::getPhysicsSOLDIERTransitionValueHigh"); }
    void setPhysicsSOLDIERTransitionValueHigh(float) override { unexpected("IHapticSettings::setPhysicsSOLDIERTransitionValueHigh"); }
    int getPhysicsSOLDIERSmoothTimeX(int level) override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeX"); }
    void setPhysicsSOLDIERSmoothTimeX(int level, int value) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeX"); }
    int getPhysicsSOLDIERSmoothTimeY(int level) override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeY"); }
    void setPhysicsSOLDIERSmoothTimeY(int level, int value) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeY"); }
    int getPhysicsSOLDIERSmoothTimeZ(int level) override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeZ"); }
    void setPhysicsSOLDIERSmoothTimeZ(int level, int value) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeZ"); }
    int getPhysicsSOLDIERSmoothTimeXLow() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeXLow"); }
    void setPhysicsSOLDIERSmoothTimeXLow(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeXLow"); }
    int getPhysicsSOLDIERSmoothTimeYLow() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeYLow"); }
    void setPhysicsSOLDIERSmoothTimeYLow(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeYLow"); }
    int getPhysicsSOLDIERSmoothTimeZLow() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeZLow"); }
    void setPhysicsSOLDIERSmoothTimeZLow(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeZLow"); }
    int getPhysicsSOLDIERSmoothTimeXMed() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeXMed"); }
    void setPhysicsSOLDIERSmoothTimeXMed(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeXMed"); }
    int getPhysicsSOLDIERSmoothTimeYMed() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeYMed"); }
    void setPhysicsSOLDIERSmoothTimeYMed(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeYMed"); }
    int getPhysicsSOLDIERSmoothTimeZMed() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeZMed"); }
    void setPhysicsSOLDIERSmoothTimeZMed(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeZMed"); }
    int getPhysicsSOLDIERSmoothTimeXHigh() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeXHigh"); }
    void setPhysicsSOLDIERSmoothTimeXHigh(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeXHigh"); }
    int getPhysicsSOLDIERSmoothTimeYHigh() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeYHigh"); }
    void setPhysicsSOLDIERSmoothTimeYHigh(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeYHigh"); }
    int getPhysicsSOLDIERSmoothTimeZHigh() override { unexpected("IHapticSettings::getPhysicsSOLDIERSmoothTimeZHigh"); }
    void setPhysicsSOLDIERSmoothTimeZHigh(int) override { unexpected("IHapticSettings::setPhysicsSOLDIERSmoothTimeZHigh"); }
    Vec3 getPhysicsLANDScale() override { unexpected("IHapticSettings::getPhysicsLANDScale"); }
    void setPhysicsLANDScale(float x, float y, float z) override { unexpected("IHapticSettings::setPhysicsLANDScale"); }
    float getPhysicsLANDScaleXLow() override { unexpected("IHapticSettings::getPhysicsLANDScaleXLow"); }
    void setPhysicsLANDScaleXLow(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleXLow"); }
    float getPhysicsLANDScaleYLow() override { unexpected("IHapticSettings::getPhysicsLANDScaleYLow"); }
    void setPhysicsLANDScaleYLow(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleYLow"); }
    float getPhysicsLANDScaleZLow() override { unexpected("IHapticSettings::getPhysicsLANDScaleZLow"); }
    void setPhysicsLANDScaleZLow(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleZLow"); }
    float getPhysicsLANDScaleXMed() override { unexpected("IHapticSettings::getPhysicsLANDScaleXMed"); }
    void setPhysicsLANDScaleXMed(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleXMed"); }
    float getPhysicsLANDScaleYMed() override { unexpected("IHapticSettings::getPhysicsLANDScaleYMed"); }
    void setPhysicsLANDScaleYMed(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleYMed"); }
    float getPhysicsLANDScaleZMed() override { unexpected("IHapticSettings::getPhysicsLANDScaleZMed"); }
    void setPhysicsLANDScaleZMed(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleZMed"); }
    float getPhysicsLANDScaleXHigh() override { unexpected("IHapticSettings::getPhysicsLANDScaleXHigh"); }
    void setPhysicsLANDScaleXHigh(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleXHigh"); }
    float getPhysicsLANDScaleYHigh() override { unexpected("IHapticSettings::getPhysicsLANDScaleYHigh"); }
    void setPhysicsLANDScaleYHigh(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleYHigh"); }
    float getPhysicsLANDScaleZHigh() override { unexpected("IHapticSettings::getPhysicsLANDScaleZHigh"); }
    void setPhysicsLANDScaleZHigh(float) override { unexpected("IHapticSettings::setPhysicsLANDScaleZHigh"); }
    float getPhysicsLANDTransitionValue(int level) override { unexpected("IHapticSettings::getPhysicsLANDTransitionValue"); }
    void setPhysicsLANDTransitionValue(int level, float value) override { unexpected("IHapticSettings::setPhysicsLANDTransitionValue"); }
    float getPhysicsLANDTransitionValueMed() override { unexpected("IHapticSettings::getPhysicsLANDTransitionValueMed"); }
    void setPhysicsLANDTransitionValueMed(float) override { unexpected("IHapticSettings::setPhysicsLANDTransitionValueMed"); }
    float getPhysicsLANDTransitionValueHigh() override { unexpected("IHapticSettings::getPhysicsLANDTransitionValueHigh"); }
    void setPhysicsLANDTransitionValueHigh(float) override { unexpected("IHapticSettings::setPhysicsLANDTransitionValueHigh"); }
    int getPhysicsLANDSmoothTimeX(int level) override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeX"); }
    void setPhysicsLANDSmoothTimeX(int level, int value) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeX"); }
    int getPhysicsLANDSmoothTimeY(int level) override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeY"); }
    void setPhysicsLANDSmoothTimeY(int level, int value) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeY"); }
    int getPhysicsLANDSmoothTimeZ(int level) override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeZ"); }
    void setPhysicsLANDSmoothTimeZ(int level, int value) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeZ"); }
    int getPhysicsLANDSmoothTimeXLow() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeXLow"); }
    void setPhysicsLANDSmoothTimeXLow(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeXLow"); }
    int getPhysicsLANDSmoothTimeYLow() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeYLow"); }
    void setPhysicsLANDSmoothTimeYLow(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeYLow"); }
    int getPhysicsLANDSmoothTimeZLow() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeZLow"); }
    void setPhysicsLANDSmoothTimeZLow(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeZLow"); }
    int getPhysicsLANDSmoothTimeXMed() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeXMed"); }
    void setPhysicsLANDSmoothTimeXMed(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeXMed"); }
    int getPhysicsLANDSmoothTimeYMed() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeYMed"); }
    void setPhysicsLANDSmoothTimeYMed(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeYMed"); }
    int getPhysicsLANDSmoothTimeZMed() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeZMed"); }
    void setPhysicsLANDSmoothTimeZMed(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeZMed"); }
    int getPhysicsLANDSmoothTimeXHigh() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeXHigh"); }
    void setPhysicsLANDSmoothTimeXHigh(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeXHigh"); }
    int getPhysicsLANDSmoothTimeYHigh() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeYHigh"); }
    void setPhysicsLANDSmoothTimeYHigh(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeYHigh"); }
    int getPhysicsLANDSmoothTimeZHigh() override { unexpected("IHapticSettings::getPhysicsLANDSmoothTimeZHigh"); }
    void setPhysicsLANDSmoothTimeZHigh(int) override { unexpected("IHapticSettings::setPhysicsLANDSmoothTimeZHigh"); }
    Vec3 getPhysicsAIRScale() override { unexpected("IHapticSettings::getPhysicsAIRScale"); }
    void setPhysicsAIRScale(float x, float y, float z) override { unexpected("IHapticSettings::setPhysicsAIRScale"); }
    float getPhysicsAIRScaleXLow() override { unexpected("IHapticSettings::getPhysicsAIRScaleXLow"); }
    void setPhysicsAIRScaleXLow(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleXLow"); }
    float getPhysicsAIRScaleYLow() override { unexpected("IHapticSettings::getPhysicsAIRScaleYLow"); }
    void setPhysicsAIRScaleYLow(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleYLow"); }
    float getPhysicsAIRScaleZLow() override { unexpected("IHapticSettings::getPhysicsAIRScaleZLow"); }
    void setPhysicsAIRScaleZLow(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleZLow"); }
    float getPhysicsAIRScaleXMed() override { unexpected("IHapticSettings::getPhysicsAIRScaleXMed"); }
    void setPhysicsAIRScaleXMed(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleXMed"); }
    float getPhysicsAIRScaleYMed() override { unexpected("IHapticSettings::getPhysicsAIRScaleYMed"); }
    void setPhysicsAIRScaleYMed(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleYMed"); }
    float getPhysicsAIRScaleZMed() override { unexpected("IHapticSettings::getPhysicsAIRScaleZMed"); }
    void setPhysicsAIRScaleZMed(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleZMed"); }
    float getPhysicsAIRScaleXHigh() override { unexpected("IHapticSettings::getPhysicsAIRScaleXHigh"); }
    void setPhysicsAIRScaleXHigh(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleXHigh"); }
    float getPhysicsAIRScaleYHigh() override { unexpected("IHapticSettings::getPhysicsAIRScaleYHigh"); }
    void setPhysicsAIRScaleYHigh(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleYHigh"); }
    float getPhysicsAIRScaleZHigh() override { unexpected("IHapticSettings::getPhysicsAIRScaleZHigh"); }
    void setPhysicsAIRScaleZHigh(float) override { unexpected("IHapticSettings::setPhysicsAIRScaleZHigh"); }
    float getPhysicsAIRTransitionValue(int level) override { unexpected("IHapticSettings::getPhysicsAIRTransitionValue"); }
    void setPhysicsAIRTransitionValue(int level, float value) override { unexpected("IHapticSettings::setPhysicsAIRTransitionValue"); }
    float getPhysicsAIRTransitionValueMed() override { unexpected("IHapticSettings::getPhysicsAIRTransitionValueMed"); }
    void setPhysicsAIRTransitionValueMed(float) override { unexpected("IHapticSettings::setPhysicsAIRTransitionValueMed"); }
    float getPhysicsAIRTransitionValueHigh() override { unexpected("IHapticSettings::getPhysicsAIRTransitionValueHigh"); }
    void setPhysicsAIRTransitionValueHigh(float) override { unexpected("IHapticSettings::setPhysicsAIRTransitionValueHigh"); }
    int getPhysicsAIRSmoothTimeX(int level) override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeX"); }
    void setPhysicsAIRSmoothTimeX(int level, int value) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeX"); }
    int getPhysicsAIRSmoothTimeY(int level) override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeY"); }
    void setPhysicsAIRSmoothTimeY(int level, int value) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeY"); }
    int getPhysicsAIRSmoothTimeZ(int level) override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeZ"); }
    void setPhysicsAIRSmoothTimeZ(int level, int value) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeZ"); }
    int getPhysicsAIRSmoothTimeXLow() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeXLow"); }
    void setPhysicsAIRSmoothTimeXLow(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeXLow"); }
    int getPhysicsAIRSmoothTimeYLow() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeYLow"); }
    void setPhysicsAIRSmoothTimeYLow(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeYLow"); }
    int getPhysicsAIRSmoothTimeZLow() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeZLow"); }
    void setPhysicsAIRSmoothTimeZLow(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeZLow"); }
    int getPhysicsAIRSmoothTimeXMed() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeXMed"); }
    void setPhysicsAIRSmoothTimeXMed(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeXMed"); }
    int getPhysicsAIRSmoothTimeYMed() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeYMed"); }
    void setPhysicsAIRSmoothTimeYMed(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeYMed"); }
    int getPhysicsAIRSmoothTimeZMed() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeZMed"); }
    void setPhysicsAIRSmoothTimeZMed(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeZMed"); }
    int getPhysicsAIRSmoothTimeXHigh() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeXHigh"); }
    void setPhysicsAIRSmoothTimeXHigh(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeXHigh"); }
    int getPhysicsAIRSmoothTimeYHigh() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeYHigh"); }
    void setPhysicsAIRSmoothTimeYHigh(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeYHigh"); }
    int getPhysicsAIRSmoothTimeZHigh() override { unexpected("IHapticSettings::getPhysicsAIRSmoothTimeZHigh"); }
    void setPhysicsAIRSmoothTimeZHigh(int) override { unexpected("IHapticSettings::setPhysicsAIRSmoothTimeZHigh"); }
    Vec3 getPhysicsHELIScale() override { unexpected("IHapticSettings::getPhysicsHELIScale"); }
    void setPhysicsHELIScale(float x, float y, float z) override { unexpected("IHapticSettings::setPhysicsHELIScale"); }
    float getPhysicsHELIScaleXLow() override { unexpected("IHapticSettings::getPhysicsHELIScaleXLow"); }
    void setPhysicsHELIScaleXLow(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleXLow"); }
    float getPhysicsHELIScaleYLow() override { unexpected("IHapticSettings::getPhysicsHELIScaleYLow"); }
    void setPhysicsHELIScaleYLow(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleYLow"); }
    float getPhysicsHELIScaleZLow() override { unexpected("IHapticSettings::getPhysicsHELIScaleZLow"); }
    void setPhysicsHELIScaleZLow(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleZLow"); }
    float getPhysicsHELIScaleXMed() override { unexpected("IHapticSettings::getPhysicsHELIScaleXMed"); }
    void setPhysicsHELIScaleXMed(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleXMed"); }
    float getPhysicsHELIScaleYMed() override { unexpected("IHapticSettings::getPhysicsHELIScaleYMed"); }
    void setPhysicsHELIScaleYMed(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleYMed"); }
    float getPhysicsHELIScaleZMed() override { unexpected("IHapticSettings::getPhysicsHELIScaleZMed"); }
    void setPhysicsHELIScaleZMed(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleZMed"); }
    float getPhysicsHELIScaleXHigh() override { unexpected("IHapticSettings::getPhysicsHELIScaleXHigh"); }
    void setPhysicsHELIScaleXHigh(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleXHigh"); }
    float getPhysicsHELIScaleYHigh() override { unexpected("IHapticSettings::getPhysicsHELIScaleYHigh"); }
    void setPhysicsHELIScaleYHigh(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleYHigh"); }
    float getPhysicsHELIScaleZHigh() override { unexpected("IHapticSettings::getPhysicsHELIScaleZHigh"); }
    void setPhysicsHELIScaleZHigh(float) override { unexpected("IHapticSettings::setPhysicsHELIScaleZHigh"); }
    float getPhysicsHELITransitionValue(int level) override { unexpected("IHapticSettings::getPhysicsHELITransitionValue"); }
    void setPhysicsHELITransitionValue(int level, float value) override { unexpected("IHapticSettings::setPhysicsHELITransitionValue"); }
    float getPhysicsHELITransitionValueMed() override { unexpected("IHapticSettings::getPhysicsHELITransitionValueMed"); }
    void setPhysicsHELITransitionValueMed(float) override { unexpected("IHapticSettings::setPhysicsHELITransitionValueMed"); }
    float getPhysicsHELITransitionValueHigh() override { unexpected("IHapticSettings::getPhysicsHELITransitionValueHigh"); }
    void setPhysicsHELITransitionValueHigh(float) override { unexpected("IHapticSettings::setPhysicsHELITransitionValueHigh"); }
    int getPhysicsHELISmoothTimeX(int level) override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeX"); }
    void setPhysicsHELISmoothTimeX(int level, int value) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeX"); }
    int getPhysicsHELISmoothTimeY(int level) override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeY"); }
    void setPhysicsHELISmoothTimeY(int level, int value) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeY"); }
    int getPhysicsHELISmoothTimeZ(int level) override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeZ"); }
    void setPhysicsHELISmoothTimeZ(int level, int value) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeZ"); }
    int getPhysicsHELISmoothTimeXLow() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeXLow"); }
    void setPhysicsHELISmoothTimeXLow(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeXLow"); }
    int getPhysicsHELISmoothTimeYLow() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeYLow"); }
    void setPhysicsHELISmoothTimeYLow(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeYLow"); }
    int getPhysicsHELISmoothTimeZLow() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeZLow"); }
    void setPhysicsHELISmoothTimeZLow(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeZLow"); }
    int getPhysicsHELISmoothTimeXMed() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeXMed"); }
    void setPhysicsHELISmoothTimeXMed(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeXMed"); }
    int getPhysicsHELISmoothTimeYMed() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeYMed"); }
    void setPhysicsHELISmoothTimeYMed(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeYMed"); }
    int getPhysicsHELISmoothTimeZMed() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeZMed"); }
    void setPhysicsHELISmoothTimeZMed(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeZMed"); }
    int getPhysicsHELISmoothTimeXHigh() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeXHigh"); }
    void setPhysicsHELISmoothTimeXHigh(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeXHigh"); }
    int getPhysicsHELISmoothTimeYHigh() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeYHigh"); }
    void setPhysicsHELISmoothTimeYHigh(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeYHigh"); }
    int getPhysicsHELISmoothTimeZHigh() override { unexpected("IHapticSettings::getPhysicsHELISmoothTimeZHigh"); }
    void setPhysicsHELISmoothTimeZHigh(int) override { unexpected("IHapticSettings::setPhysicsHELISmoothTimeZHigh"); }
    Vec3 getPhysicsSEAScale() override { unexpected("IHapticSettings::getPhysicsSEAScale"); }
    void setPhysicsSEAScale(float x, float y, float z) override { unexpected("IHapticSettings::setPhysicsSEAScale"); }
    float getPhysicsSEAScaleXLow() override { unexpected("IHapticSettings::getPhysicsSEAScaleXLow"); }
    void setPhysicsSEAScaleXLow(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleXLow"); }
    float getPhysicsSEAScaleYLow() override { unexpected("IHapticSettings::getPhysicsSEAScaleYLow"); }
    void setPhysicsSEAScaleYLow(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleYLow"); }
    float getPhysicsSEAScaleZLow() override { unexpected("IHapticSettings::getPhysicsSEAScaleZLow"); }
    void setPhysicsSEAScaleZLow(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleZLow"); }
    float getPhysicsSEAScaleXMed() override { unexpected("IHapticSettings::getPhysicsSEAScaleXMed"); }
    void setPhysicsSEAScaleXMed(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleXMed"); }
    float getPhysicsSEAScaleYMed() override { unexpected("IHapticSettings::getPhysicsSEAScaleYMed"); }
    void setPhysicsSEAScaleYMed(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleYMed"); }
    float getPhysicsSEAScaleZMed() override { unexpected("IHapticSettings::getPhysicsSEAScaleZMed"); }
    void setPhysicsSEAScaleZMed(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleZMed"); }
    float getPhysicsSEAScaleXHigh() override { unexpected("IHapticSettings::getPhysicsSEAScaleXHigh"); }
    void setPhysicsSEAScaleXHigh(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleXHigh"); }
    float getPhysicsSEAScaleYHigh() override { unexpected("IHapticSettings::getPhysicsSEAScaleYHigh"); }
    void setPhysicsSEAScaleYHigh(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleYHigh"); }
    float getPhysicsSEAScaleZHigh() override { unexpected("IHapticSettings::getPhysicsSEAScaleZHigh"); }
    void setPhysicsSEAScaleZHigh(float) override { unexpected("IHapticSettings::setPhysicsSEAScaleZHigh"); }
    float getPhysicsSEATransitionValue(int level) override { unexpected("IHapticSettings::getPhysicsSEATransitionValue"); }
    void setPhysicsSEATransitionValue(int level, float value) override { unexpected("IHapticSettings::setPhysicsSEATransitionValue"); }
    float getPhysicsSEATransitionValueMed() override { unexpected("IHapticSettings::getPhysicsSEATransitionValueMed"); }
    void setPhysicsSEATransitionValueMed(float) override { unexpected("IHapticSettings::setPhysicsSEATransitionValueMed"); }
    float getPhysicsSEATransitionValueHigh() override { unexpected("IHapticSettings::getPhysicsSEATransitionValueHigh"); }
    void setPhysicsSEATransitionValueHigh(float) override { unexpected("IHapticSettings::setPhysicsSEATransitionValueHigh"); }
    int getPhysicsSEASmoothTimeX(int level) override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeX"); }
    void setPhysicsSEASmoothTimeX(int level, int value) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeX"); }
    int getPhysicsSEASmoothTimeY(int level) override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeY"); }
    void setPhysicsSEASmoothTimeY(int level, int value) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeY"); }
    int getPhysicsSEASmoothTimeZ(int level) override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeZ"); }
    void setPhysicsSEASmoothTimeZ(int level, int value) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeZ"); }
    int getPhysicsSEASmoothTimeXLow() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeXLow"); }
    void setPhysicsSEASmoothTimeXLow(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeXLow"); }
    int getPhysicsSEASmoothTimeYLow() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeYLow"); }
    void setPhysicsSEASmoothTimeYLow(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeYLow"); }
    int getPhysicsSEASmoothTimeZLow() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeZLow"); }
    void setPhysicsSEASmoothTimeZLow(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeZLow"); }
    int getPhysicsSEASmoothTimeXMed() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeXMed"); }
    void setPhysicsSEASmoothTimeXMed(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeXMed"); }
    int getPhysicsSEASmoothTimeYMed() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeYMed"); }
    void setPhysicsSEASmoothTimeYMed(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeYMed"); }
    int getPhysicsSEASmoothTimeZMed() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeZMed"); }
    void setPhysicsSEASmoothTimeZMed(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeZMed"); }
    int getPhysicsSEASmoothTimeXHigh() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeXHigh"); }
    void setPhysicsSEASmoothTimeXHigh(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeXHigh"); }
    int getPhysicsSEASmoothTimeYHigh() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeYHigh"); }
    void setPhysicsSEASmoothTimeYHigh(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeYHigh"); }
    int getPhysicsSEASmoothTimeZHigh() override { unexpected("IHapticSettings::getPhysicsSEASmoothTimeZHigh"); }
    void setPhysicsSEASmoothTimeZHigh(int) override { unexpected("IHapticSettings::setPhysicsSEASmoothTimeZHigh"); }
    float getDamageScale() override { unexpected("IHapticSettings::getDamageScale"); }
    void setDamageScale(float) override { unexpected("IHapticSettings::setDamageScale"); }
    float getRecoilPunchScale(int weaponClass) override { unexpected("IHapticSettings::getRecoilPunchScale"); }
    void setRecoilPunchScale(int weaponClass, float value) override { unexpected("IHapticSettings::setRecoilPunchScale"); }
    float getRecoilPunchTime(int weaponClass) override { unexpected("IHapticSettings::getRecoilPunchTime"); }
    void setRecoilPunchTime(int weaponClass, float value) override { unexpected("IHapticSettings::setRecoilPunchTime"); }
    float getRecoilYawScale(int weaponClass) override { unexpected("IHapticSettings::getRecoilYawScale"); }
    void setRecoilYawScale(int weaponClass, float value) override { unexpected("IHapticSettings::setRecoilYawScale"); }
    float getRecoilPitchScale(int weaponClass) override { unexpected("IHapticSettings::getRecoilPitchScale"); }
    void setRecoilPitchScale(int weaponClass, float value) override { unexpected("IHapticSettings::setRecoilPitchScale"); }
    float getRecoilPunchScaleRIFLE() override { unexpected("IHapticSettings::getRecoilPunchScaleRIFLE"); }
    void setRecoilPunchScaleRIFLE(float) override { unexpected("IHapticSettings::setRecoilPunchScaleRIFLE"); }
    float getRecoilPunchTimeRIFLE() override { unexpected("IHapticSettings::getRecoilPunchTimeRIFLE"); }
    void setRecoilPunchTimeRIFLE(float) override { unexpected("IHapticSettings::setRecoilPunchTimeRIFLE"); }
    float getRecoilYawScaleRIFLE() override { unexpected("IHapticSettings::getRecoilYawScaleRIFLE"); }
    void setRecoilYawScaleRIFLE(float) override { unexpected("IHapticSettings::setRecoilYawScaleRIFLE"); }
    float getRecoilPitchScaleRIFLE() override { unexpected("IHapticSettings::getRecoilPitchScaleRIFLE"); }
    void setRecoilPitchScaleRIFLE(float) override { unexpected("IHapticSettings::setRecoilPitchScaleRIFLE"); }
    float getRecoilPunchScaleRIFLELAUNCHER() override { unexpected("IHapticSettings::getRecoilPunchScaleRIFLELAUNCHER"); }
    void setRecoilPunchScaleRIFLELAUNCHER(float) override { unexpected("IHapticSettings::setRecoilPunchScaleRIFLELAUNCHER"); }
    float getRecoilPunchTimeRIFLELAUNCHER() override { unexpected("IHapticSettings::getRecoilPunchTimeRIFLELAUNCHER"); }
    void setRecoilPunchTimeRIFLELAUNCHER(float) override { unexpected("IHapticSettings::setRecoilPunchTimeRIFLELAUNCHER"); }
    float getRecoilYawScaleRIFLELAUNCHER() override { unexpected("IHapticSettings::getRecoilYawScaleRIFLELAUNCHER"); }
    void setRecoilYawScaleRIFLELAUNCHER(float) override { unexpected("IHapticSettings::setRecoilYawScaleRIFLELAUNCHER"); }
    float getRecoilPitchScaleRIFLELAUNCHER() override { unexpected("IHapticSettings::getRecoilPitchScaleRIFLELAUNCHER"); }
    void setRecoilPitchScaleRIFLELAUNCHER(float) override { unexpected("IHapticSettings::setRecoilPitchScaleRIFLELAUNCHER"); }
    float getRecoilPunchScaleCARBINE() override { unexpected("IHapticSettings::getRecoilPunchScaleCARBINE"); }
    void setRecoilPunchScaleCARBINE(float) override { unexpected("IHapticSettings::setRecoilPunchScaleCARBINE"); }
    float getRecoilPunchTimeCARBINE() override { unexpected("IHapticSettings::getRecoilPunchTimeCARBINE"); }
    void setRecoilPunchTimeCARBINE(float) override { unexpected("IHapticSettings::setRecoilPunchTimeCARBINE"); }
    float getRecoilYawScaleCARBINE() override { unexpected("IHapticSettings::getRecoilYawScaleCARBINE"); }
    void setRecoilYawScaleCARBINE(float) override { unexpected("IHapticSettings::setRecoilYawScaleCARBINE"); }
    float getRecoilPitchScaleCARBINE() override { unexpected("IHapticSettings::getRecoilPitchScaleCARBINE"); }
    void setRecoilPitchScaleCARBINE(float) override { unexpected("IHapticSettings::setRecoilPitchScaleCARBINE"); }
    float getRecoilPunchScaleLMG() override { unexpected("IHapticSettings::getRecoilPunchScaleLMG"); }
    void setRecoilPunchScaleLMG(float) override { unexpected("IHapticSettings::setRecoilPunchScaleLMG"); }
    float getRecoilPunchTimeLMG() override { unexpected("IHapticSettings::getRecoilPunchTimeLMG"); }
    void setRecoilPunchTimeLMG(float) override { unexpected("IHapticSettings::setRecoilPunchTimeLMG"); }
    float getRecoilYawScaleLMG() override { unexpected("IHapticSettings::getRecoilYawScaleLMG"); }
    void setRecoilYawScaleLMG(float) override { unexpected("IHapticSettings::setRecoilYawScaleLMG"); }
    float getRecoilPitchScaleLMG() override { unexpected("IHapticSettings::getRecoilPitchScaleLMG"); }
    void setRecoilPitchScaleLMG(float) override { unexpected("IHapticSettings::setRecoilPitchScaleLMG"); }
    float getRecoilPunchScaleSNIPER() override { unexpected("IHapticSettings::getRecoilPunchScaleSNIPER"); }
    void setRecoilPunchScaleSNIPER(float) override { unexpected("IHapticSettings::setRecoilPunchScaleSNIPER"); }
    float getRecoilPunchTimeSNIPER() override { unexpected("IHapticSettings::getRecoilPunchTimeSNIPER"); }
    void setRecoilPunchTimeSNIPER(float) override { unexpected("IHapticSettings::setRecoilPunchTimeSNIPER"); }
    float getRecoilYawScaleSNIPER() override { unexpected("IHapticSettings::getRecoilYawScaleSNIPER"); }
    void setRecoilYawScaleSNIPER(float) override { unexpected("IHapticSettings::setRecoilYawScaleSNIPER"); }
    float getRecoilPitchScaleSNIPER() override { unexpected("IHapticSettings::getRecoilPitchScaleSNIPER"); }
    void setRecoilPitchScaleSNIPER(float) override { unexpected("IHapticSettings::setRecoilPitchScaleSNIPER"); }
    float getRecoilPunchScalePISTOL() override { unexpected("IHapticSettings::getRecoilPunchScalePISTOL"); }
    void setRecoilPunchScalePISTOL(float) override { unexpected("IHapticSettings::setRecoilPunchScalePISTOL"); }
    float getRecoilPunchTimePISTOL() override { unexpected("IHapticSettings::getRecoilPunchTimePISTOL"); }
    void setRecoilPunchTimePISTOL(float) override { unexpected("IHapticSettings::setRecoilPunchTimePISTOL"); }
    float getRecoilYawScalePISTOL() override { unexpected("IHapticSettings::getRecoilYawScalePISTOL"); }
    void setRecoilYawScalePISTOL(float) override { unexpected("IHapticSettings::setRecoilYawScalePISTOL"); }
    float getRecoilPitchScalePISTOL() override { unexpected("IHapticSettings::getRecoilPitchScalePISTOL"); }
    void setRecoilPitchScalePISTOL(float) override { unexpected("IHapticSettings::setRecoilPitchScalePISTOL"); }
    float getRecoilPunchScaleLAUNCHER() override { unexpected("IHapticSettings::getRecoilPunchScaleLAUNCHER"); }
    void setRecoilPunchScaleLAUNCHER(float) override { unexpected("IHapticSettings::setRecoilPunchScaleLAUNCHER"); }
    float getRecoilPunchTimeLAUNCHER() override { unexpected("IHapticSettings::getRecoilPunchTimeLAUNCHER"); }
    void setRecoilPunchTimeLAUNCHER(float) override { unexpected("IHapticSettings::setRecoilPunchTimeLAUNCHER"); }
    float getRecoilYawScaleLAUNCHER() override { unexpected("IHapticSettings::getRecoilYawScaleLAUNCHER"); }
    void setRecoilYawScaleLAUNCHER(float) override { unexpected("IHapticSettings::setRecoilYawScaleLAUNCHER"); }
    float getRecoilPitchScaleLAUNCHER() override { unexpected("IHapticSettings::getRecoilPitchScaleLAUNCHER"); }
    void setRecoilPitchScaleLAUNCHER(float) override { unexpected("IHapticSettings::setRecoilPitchScaleLAUNCHER"); }
    float getRecoilPunchScaleSMG() override { unexpected("IHapticSettings::getRecoilPunchScaleSMG"); }
    void setRecoilPunchScaleSMG(float) override { unexpected("IHapticSettings::setRecoilPunchScaleSMG"); }
    float getRecoilPunchTimeSMG() override { unexpected("IHapticSettings::getRecoilPunchTimeSMG"); }
    void setRecoilPunchTimeSMG(float) override { unexpected("IHapticSettings::setRecoilPunchTimeSMG"); }
    float getRecoilYawScaleSMG() override { unexpected("IHapticSettings::getRecoilYawScaleSMG"); }
    void setRecoilYawScaleSMG(float) override { unexpected("IHapticSettings::setRecoilYawScaleSMG"); }
    float getRecoilPitchScaleSMG() override { unexpected("IHapticSettings::getRecoilPitchScaleSMG"); }
    void setRecoilPitchScaleSMG(float) override { unexpected("IHapticSettings::setRecoilPitchScaleSMG"); }
    float getRecoilPunchScaleSHOTGUN() override { unexpected("IHapticSettings::getRecoilPunchScaleSHOTGUN"); }
    void setRecoilPunchScaleSHOTGUN(float) override { unexpected("IHapticSettings::setRecoilPunchScaleSHOTGUN"); }
    float getRecoilPunchTimeSHOTGUN() override { unexpected("IHapticSettings::getRecoilPunchTimeSHOTGUN"); }
    void setRecoilPunchTimeSHOTGUN(float) override { unexpected("IHapticSettings::setRecoilPunchTimeSHOTGUN"); }
    float getRecoilYawScaleSHOTGUN() override { unexpected("IHapticSettings::getRecoilYawScaleSHOTGUN"); }
    void setRecoilYawScaleSHOTGUN(float) override { unexpected("IHapticSettings::setRecoilYawScaleSHOTGUN"); }
    float getRecoilPitchScaleSHOTGUN() override { unexpected("IHapticSettings::getRecoilPitchScaleSHOTGUN"); }
    void setRecoilPitchScaleSHOTGUN(float) override { unexpected("IHapticSettings::setRecoilPitchScaleSHOTGUN"); }
    float getRecoilPunchScaleKNIFE() override { unexpected("IHapticSettings::getRecoilPunchScaleKNIFE"); }
    void setRecoilPunchScaleKNIFE(float) override { unexpected("IHapticSettings::setRecoilPunchScaleKNIFE"); }
    float getRecoilPunchTimeKNIFE() override { unexpected("IHapticSettings::getRecoilPunchTimeKNIFE"); }
    void setRecoilPunchTimeKNIFE(float) override { unexpected("IHapticSettings::setRecoilPunchTimeKNIFE"); }
    float getRecoilYawScaleKNIFE() override { unexpected("IHapticSettings::getRecoilYawScaleKNIFE"); }
    void setRecoilYawScaleKNIFE(float) override { unexpected("IHapticSettings::setRecoilYawScaleKNIFE"); }
    float getRecoilPitchScaleKNIFE() override { unexpected("IHapticSettings::getRecoilPitchScaleKNIFE"); }
    void setRecoilPitchScaleKNIFE(float) override { unexpected("IHapticSettings::setRecoilPitchScaleKNIFE"); }
    float getRecoilPunchScaleDEFIB() override { unexpected("IHapticSettings::getRecoilPunchScaleDEFIB"); }
    void setRecoilPunchScaleDEFIB(float) override { unexpected("IHapticSettings::setRecoilPunchScaleDEFIB"); }
    float getRecoilPunchTimeDEFIB() override { unexpected("IHapticSettings::getRecoilPunchTimeDEFIB"); }
    void setRecoilPunchTimeDEFIB(float) override { unexpected("IHapticSettings::setRecoilPunchTimeDEFIB"); }
    float getRecoilYawScaleDEFIB() override { unexpected("IHapticSettings::getRecoilYawScaleDEFIB"); }
    void setRecoilYawScaleDEFIB(float) override { unexpected("IHapticSettings::setRecoilYawScaleDEFIB"); }
    float getRecoilPitchScaleDEFIB() override { unexpected("IHapticSettings::getRecoilPitchScaleDEFIB"); }
    void setRecoilPitchScaleDEFIB(float) override { unexpected("IHapticSettings::setRecoilPitchScaleDEFIB"); }
    float getRecoilPunchScaleDROP() override { unexpected("IHapticSettings::getRecoilPunchScaleDROP"); }
    void setRecoilPunchScaleDROP(float) override { unexpected("IHapticSettings::setRecoilPunchScaleDROP"); }
    float getRecoilPunchTimeDROP() override { unexpected("IHapticSettings::getRecoilPunchTimeDROP"); }
    void setRecoilPunchTimeDROP(float) override { unexpected("IHapticSettings::setRecoilPunchTimeDROP"); }
    float getRecoilYawScaleDROP() override { unexpected("IHapticSettings::getRecoilYawScaleDROP"); }
    void setRecoilYawScaleDROP(float) override { unexpected("IHapticSettings::setRecoilYawScaleDROP"); }
    float getRecoilPitchScaleDROP() override { unexpected("IHapticSettings::getRecoilPitchScaleDROP"); }
    void setRecoilPitchScaleDROP(float) override { unexpected("IHapticSettings::setRecoilPitchScaleDROP"); }
    float getRecoilPunchScaleTHROWN() override { unexpected("IHapticSettings::getRecoilPunchScaleTHROWN"); }
    void setRecoilPunchScaleTHROWN(float) override { unexpected("IHapticSettings::setRecoilPunchScaleTHROWN"); }
    float getRecoilPunchTimeTHROWN() override { unexpected("IHapticSettings::getRecoilPunchTimeTHROWN"); }
    void setRecoilPunchTimeTHROWN(float) override { unexpected("IHapticSettings::setRecoilPunchTimeTHROWN"); }
    float getRecoilYawScaleTHROWN() override { unexpected("IHapticSettings::getRecoilYawScaleTHROWN"); }
    void setRecoilYawScaleTHROWN(float) override { unexpected("IHapticSettings::setRecoilYawScaleTHROWN"); }
    float getRecoilPitchScaleTHROWN() override { unexpected("IHapticSettings::getRecoilPitchScaleTHROWN"); }
    void setRecoilPitchScaleTHROWN(float) override { unexpected("IHapticSettings::setRecoilPitchScaleTHROWN"); }
    float getRecoilPunchScaleV_LMG() override { unexpected("IHapticSettings::getRecoilPunchScaleV_LMG"); }
    void setRecoilPunchScaleV_LMG(float) override { unexpected("IHapticSettings::setRecoilPunchScaleV_LMG"); }
    float getRecoilPunchTimeV_LMG() override { unexpected("IHapticSettings::getRecoilPunchTimeV_LMG"); }
    void setRecoilPunchTimeV_LMG(float) override { unexpected("IHapticSettings::setRecoilPunchTimeV_LMG"); }
    float getRecoilYawScaleV_LMG() override { unexpected("IHapticSettings::getRecoilYawScaleV_LMG"); }
    void setRecoilYawScaleV_LMG(float) override { unexpected("IHapticSettings::setRecoilYawScaleV_LMG"); }
    float getRecoilPitchScaleV_LMG() override { unexpected("IHapticSettings::getRecoilPitchScaleV_LMG"); }
    void setRecoilPitchScaleV_LMG(float) override { unexpected("IHapticSettings::setRecoilPitchScaleV_LMG"); }
    float getRecoilPunchScaleV_HMG() override { unexpected("IHapticSettings::getRecoilPunchScaleV_HMG"); }
    void setRecoilPunchScaleV_HMG(float) override { unexpected("IHapticSettings::setRecoilPunchScaleV_HMG"); }
    float getRecoilPunchTimeV_HMG() override { unexpected("IHapticSettings::getRecoilPunchTimeV_HMG"); }
    void setRecoilPunchTimeV_HMG(float) override { unexpected("IHapticSettings::setRecoilPunchTimeV_HMG"); }
    float getRecoilYawScaleV_HMG() override { unexpected("IHapticSettings::getRecoilYawScaleV_HMG"); }
    void setRecoilYawScaleV_HMG(float) override { unexpected("IHapticSettings::setRecoilYawScaleV_HMG"); }
    float getRecoilPitchScaleV_HMG() override { unexpected("IHapticSettings::getRecoilPitchScaleV_HMG"); }
    void setRecoilPitchScaleV_HMG(float) override { unexpected("IHapticSettings::setRecoilPitchScaleV_HMG"); }
    float getRecoilPunchScaleV_HELIGUN() override { unexpected("IHapticSettings::getRecoilPunchScaleV_HELIGUN"); }
    void setRecoilPunchScaleV_HELIGUN(float) override { unexpected("IHapticSettings::setRecoilPunchScaleV_HELIGUN"); }
    float getRecoilPunchTimeV_HELIGUN() override { unexpected("IHapticSettings::getRecoilPunchTimeV_HELIGUN"); }
    void setRecoilPunchTimeV_HELIGUN(float) override { unexpected("IHapticSettings::setRecoilPunchTimeV_HELIGUN"); }
    float getRecoilYawScaleV_HELIGUN() override { unexpected("IHapticSettings::getRecoilYawScaleV_HELIGUN"); }
    void setRecoilYawScaleV_HELIGUN(float) override { unexpected("IHapticSettings::setRecoilYawScaleV_HELIGUN"); }
    float getRecoilPitchScaleV_HELIGUN() override { unexpected("IHapticSettings::getRecoilPitchScaleV_HELIGUN"); }
    void setRecoilPitchScaleV_HELIGUN(float) override { unexpected("IHapticSettings::setRecoilPitchScaleV_HELIGUN"); }
    float getRecoilPunchScaleV_AIRGUN() override { unexpected("IHapticSettings::getRecoilPunchScaleV_AIRGUN"); }
    void setRecoilPunchScaleV_AIRGUN(float) override { unexpected("IHapticSettings::setRecoilPunchScaleV_AIRGUN"); }
    float getRecoilPunchTimeV_AIRGUN() override { unexpected("IHapticSettings::getRecoilPunchTimeV_AIRGUN"); }
    void setRecoilPunchTimeV_AIRGUN(float) override { unexpected("IHapticSettings::setRecoilPunchTimeV_AIRGUN"); }
    float getRecoilYawScaleV_AIRGUN() override { unexpected("IHapticSettings::getRecoilYawScaleV_AIRGUN"); }
    void setRecoilYawScaleV_AIRGUN(float) override { unexpected("IHapticSettings::setRecoilYawScaleV_AIRGUN"); }
    float getRecoilPitchScaleV_AIRGUN() override { unexpected("IHapticSettings::getRecoilPitchScaleV_AIRGUN"); }
    void setRecoilPitchScaleV_AIRGUN(float) override { unexpected("IHapticSettings::setRecoilPitchScaleV_AIRGUN"); }
    float getRecoilPunchScaleV_MISSILE() override { unexpected("IHapticSettings::getRecoilPunchScaleV_MISSILE"); }
    void setRecoilPunchScaleV_MISSILE(float) override { unexpected("IHapticSettings::setRecoilPunchScaleV_MISSILE"); }
    float getRecoilPunchTimeV_MISSILE() override { unexpected("IHapticSettings::getRecoilPunchTimeV_MISSILE"); }
    void setRecoilPunchTimeV_MISSILE(float) override { unexpected("IHapticSettings::setRecoilPunchTimeV_MISSILE"); }
    float getRecoilYawScaleV_MISSILE() override { unexpected("IHapticSettings::getRecoilYawScaleV_MISSILE"); }
    void setRecoilYawScaleV_MISSILE(float) override { unexpected("IHapticSettings::setRecoilYawScaleV_MISSILE"); }
    float getRecoilPitchScaleV_MISSILE() override { unexpected("IHapticSettings::getRecoilPitchScaleV_MISSILE"); }
    void setRecoilPitchScaleV_MISSILE(float) override { unexpected("IHapticSettings::setRecoilPitchScaleV_MISSILE"); }
    float getRecoilPunchScaleV_ARMORCANNON() override { unexpected("IHapticSettings::getRecoilPunchScaleV_ARMORCANNON"); }
    void setRecoilPunchScaleV_ARMORCANNON(float) override { unexpected("IHapticSettings::setRecoilPunchScaleV_ARMORCANNON"); }
    float getRecoilPunchTimeV_ARMORCANNON() override { unexpected("IHapticSettings::getRecoilPunchTimeV_ARMORCANNON"); }
    void setRecoilPunchTimeV_ARMORCANNON(float) override { unexpected("IHapticSettings::setRecoilPunchTimeV_ARMORCANNON"); }
    float getRecoilYawScaleV_ARMORCANNON() override { unexpected("IHapticSettings::getRecoilYawScaleV_ARMORCANNON"); }
    void setRecoilYawScaleV_ARMORCANNON(float) override { unexpected("IHapticSettings::setRecoilYawScaleV_ARMORCANNON"); }
    float getRecoilPitchScaleV_ARMORCANNON() override { unexpected("IHapticSettings::getRecoilPitchScaleV_ARMORCANNON"); }
    void setRecoilPitchScaleV_ARMORCANNON(float) override { unexpected("IHapticSettings::setRecoilPitchScaleV_ARMORCANNON"); }
    float getRecoilPunchScaleV_COAXIALGUN() override { unexpected("IHapticSettings::getRecoilPunchScaleV_COAXIALGUN"); }
    void setRecoilPunchScaleV_COAXIALGUN(float) override { unexpected("IHapticSettings::setRecoilPunchScaleV_COAXIALGUN"); }
    float getRecoilPunchTimeV_COAXIALGUN() override { unexpected("IHapticSettings::getRecoilPunchTimeV_COAXIALGUN"); }
    void setRecoilPunchTimeV_COAXIALGUN(float) override { unexpected("IHapticSettings::setRecoilPunchTimeV_COAXIALGUN"); }
    float getRecoilYawScaleV_COAXIALGUN() override { unexpected("IHapticSettings::getRecoilYawScaleV_COAXIALGUN"); }
    void setRecoilYawScaleV_COAXIALGUN(float) override { unexpected("IHapticSettings::setRecoilYawScaleV_COAXIALGUN"); }
    float getRecoilPitchScaleV_COAXIALGUN() override { unexpected("IHapticSettings::getRecoilPitchScaleV_COAXIALGUN"); }
    void setRecoilPitchScaleV_COAXIALGUN(float) override { unexpected("IHapticSettings::setRecoilPitchScaleV_COAXIALGUN"); }
    float getUnresolved48C() override { unexpected("IHapticSettings::getUnresolved48C"); }
    void setUnresolved48C(float) override { unexpected("IHapticSettings::setUnresolved48C"); }
    float getUnresolved490() override { unexpected("IHapticSettings::getUnresolved490"); }
    void setUnresolved490(float) override { unexpected("IHapticSettings::setUnresolved490"); }
    float getGeneralShakeScale() override { unexpected("IHapticSettings::getGeneralShakeScale"); }
    void setGeneralShakeScale(float) override { unexpected("IHapticSettings::setGeneralShakeScale"); }
    float getExplosionShakeScale() override { unexpected("IHapticSettings::getExplosionShakeScale"); }
    void setExplosionShakeScale(float) override { unexpected("IHapticSettings::setExplosionShakeScale"); }
    float getSpeedShakeScale() override { unexpected("IHapticSettings::getSpeedShakeScale"); }
    void setSpeedShakeScale(float) override { unexpected("IHapticSettings::setSpeedShakeScale"); }
    int getWeaponClass(const std::string& name) override { unexpected("IHapticSettings::getWeaponClass"); }
    void addWeaponClassMap(const std::string& name, int weaponClass) override { unexpected("IHapticSettings::addWeaponClassMap"); }
    Vec3 getControlBoxLengthSOLDIER() override { calls.push_back("getControlBoxLengthSOLDIER"); return v3(1.0f, 2.0f, 3.0f); }
    Vec3 getControlBoxStiffnessSOLDIER() override { calls.push_back("getControlBoxStiffnessSOLDIER"); return v3(11.0f, 12.0f, 13.0f); }
    Vec3 getAimSensitivitySOLDIER() override { calls.push_back("getAimSensitivitySOLDIER"); return v3(21.0f, 22.0f, 23.0f); }
    Vec3 getAimSensitivityLengthSOLDIER() override { calls.push_back("getAimSensitivityLengthSOLDIER"); return v3(31.0f, 32.0f, 33.0f); }
    Vec3 getTurnSensitivitySOLDIER() override { calls.push_back("getTurnSensitivitySOLDIER"); return v3(41.0f, 42.0f, 43.0f); }
    Vec3 getControlBoxLengthAIR() override { calls.push_back("getControlBoxLengthAIR"); return v3(101.0f, 102.0f, 103.0f); }
    Vec3 getControlBoxStiffnessAIR() override { calls.push_back("getControlBoxStiffnessAIR"); return v3(111.0f, 112.0f, 113.0f); }
    Vec3 getAimSensitivityAIR() override { calls.push_back("getAimSensitivityAIR"); return v3(121.0f, 122.0f, 123.0f); }
    Vec3 getAimSensitivityLengthAIR() override { calls.push_back("getAimSensitivityLengthAIR"); return v3(131.0f, 132.0f, 133.0f); }
    Vec3 getTurnSensitivityAIR() override { calls.push_back("getTurnSensitivityAIR"); return v3(141.0f, 142.0f, 143.0f); }
    Vec3 getControlBoxLengthHELI() override { calls.push_back("getControlBoxLengthHELI"); return v3(201.0f, 202.0f, 203.0f); }
    Vec3 getControlBoxStiffnessHELI() override { calls.push_back("getControlBoxStiffnessHELI"); return v3(211.0f, 212.0f, 213.0f); }
    Vec3 getAimSensitivityHELI() override { calls.push_back("getAimSensitivityHELI"); return v3(221.0f, 222.0f, 223.0f); }
    Vec3 getAimSensitivityLengthHELI() override { calls.push_back("getAimSensitivityLengthHELI"); return v3(231.0f, 232.0f, 233.0f); }
    Vec3 getTurnSensitivityHELI() override { calls.push_back("getTurnSensitivityHELI"); return v3(241.0f, 242.0f, 243.0f); }
    Vec3 getControlBoxLengthLAND() override { calls.push_back("getControlBoxLengthLAND"); return v3(301.0f, 302.0f, 303.0f); }
    Vec3 getControlBoxStiffnessLAND() override { calls.push_back("getControlBoxStiffnessLAND"); return v3(311.0f, 312.0f, 313.0f); }
    Vec3 getAimSensitivityLAND() override { calls.push_back("getAimSensitivityLAND"); return v3(321.0f, 322.0f, 323.0f); }
    Vec3 getAimSensitivityLengthLAND() override { calls.push_back("getAimSensitivityLengthLAND"); return v3(331.0f, 332.0f, 333.0f); }
    Vec3 getTurnSensitivityLAND() override { calls.push_back("getTurnSensitivityLAND"); return v3(341.0f, 342.0f, 343.0f); }
    Vec3 getControlBoxLengthSEA() override { calls.push_back("getControlBoxLengthSEA"); return v3(401.0f, 402.0f, 403.0f); }
    Vec3 getControlBoxStiffnessSEA() override { calls.push_back("getControlBoxStiffnessSEA"); return v3(411.0f, 412.0f, 413.0f); }
    Vec3 getAimSensitivitySEA() override { calls.push_back("getAimSensitivitySEA"); return v3(421.0f, 422.0f, 423.0f); }
    Vec3 getAimSensitivityLengthSEA() override { calls.push_back("getAimSensitivityLengthSEA"); return v3(431.0f, 432.0f, 433.0f); }
    Vec3 getTurnSensitivitySEA() override { calls.push_back("getTurnSensitivitySEA"); return v3(441.0f, 442.0f, 443.0f); }
    void setControlBoxLength(float x, float y, float z) override { calls.push_back("setControlBoxLength " + fmt(x, y, z)); }
    void setControlBoxStiffness(float x, float y, float z) override { calls.push_back("setControlBoxStiffness " + fmt(x, y, z)); }
    void setAimSensitivity(float x, float y, float z) override { calls.push_back("setAimSensitivity " + fmt(x, y, z)); }
    void setAimSensitivityLength(float x, float y, float z) override { calls.push_back("setAimSensitivityLength " + fmt(x, y, z)); }
    void setTurnSensitivity(float x, float y, float z) override { calls.push_back("setTurnSensitivity " + fmt(x, y, z)); }
};
namespace world {
struct FakeTemplate final : IPlayerControlObjectTemplate {
    std::string name;
    int vehicleType = 0, category = 0, controls = 0;
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("queryInterface"); }
    int getVehicleType() override { calls.push_back(name + ".type"); return vehicleType; }
    int getVehicleCategory() const override { calls.push_back(name + ".category"); return category; }
    int getControlsCategory() const override { calls.push_back(name + ".controls"); return controls; }
    void setPcoId(int) override { unexpected("IPlayerControlObjectTemplate::setPcoId"); }
    int getPcoId() override { unexpected("IPlayerControlObjectTemplate::getPcoId"); }
    void setPcoFlags(int) override { unexpected("IPlayerControlObjectTemplate::setPcoFlags"); }
    unsigned int getPcoFlags() const override { unexpected("IPlayerControlObjectTemplate::getPcoFlags"); }
    void setVehicleCategory(VehicleCategory) override { unexpected("IPlayerControlObjectTemplate::setVehicleCategory"); }
    void setControlsCategory(VehicleCategory) override { unexpected("IPlayerControlObjectTemplate::setControlsCategory"); }
    void setVehicleType(VehicleType) override { unexpected("IPlayerControlObjectTemplate::setVehicleType"); }
    void setToolTipType(ToolTipType) override { unexpected("IPlayerControlObjectTemplate::setToolTipType"); }
    int getToolTipType() override { unexpected("IPlayerControlObjectTemplate::getToolTipType"); }
    void setSoldierExitLocation(const Vec3&, const Vec3&) override { unexpected("IPlayerControlObjectTemplate::setSoldierExitLocation"); }
    void setSoldierExitPosition(const Vec3&) override { unexpected("IPlayerControlObjectTemplate::setSoldierExitPosition"); }
    Vec3 getSoldierExitPosition() override { unexpected("IPlayerControlObjectTemplate::getSoldierExitPosition"); }
    void setSoldierExitRotation(const Vec3&) override { unexpected("IPlayerControlObjectTemplate::setSoldierExitRotation"); }
    Vec3 getSoldierExitRotation() override { unexpected("IPlayerControlObjectTemplate::getSoldierExitRotation"); }
    void setArtPos(bool) override { unexpected("IPlayerControlObjectTemplate::setArtPos"); }
    bool getArtPos() override { unexpected("IPlayerControlObjectTemplate::getArtPos"); }
    void setDirBarXScale(float) override { unexpected("IPlayerControlObjectTemplate::setDirBarXScale"); }
    float getDirBarXScale() override { unexpected("IPlayerControlObjectTemplate::getDirBarXScale"); }
    void setDirBarYScaleAbove(float) override { unexpected("IPlayerControlObjectTemplate::setDirBarYScaleAbove"); }
    float getDirBarYScaleAbove() override { unexpected("IPlayerControlObjectTemplate::getDirBarYScaleAbove"); }
    void setDirBarYScaleBelow(float) override { unexpected("IPlayerControlObjectTemplate::setDirBarYScaleBelow"); }
    float getDirBarYScaleBelow() override { unexpected("IPlayerControlObjectTemplate::getDirBarYScaleBelow"); }
    void setDirBarRotate(float) override { unexpected("IPlayerControlObjectTemplate::setDirBarRotate"); }
    float getDirBarRotate() override { unexpected("IPlayerControlObjectTemplate::getDirBarRotate"); }
    void setDirBarYScaleMin(float) override { unexpected("IPlayerControlObjectTemplate::setDirBarYScaleMin"); }
    float getDirBarYScaleMin() override { unexpected("IPlayerControlObjectTemplate::getDirBarYScaleMin"); }
    void setDirBarYScaleMax(float) override { unexpected("IPlayerControlObjectTemplate::setDirBarYScaleMax"); }
    float getDirBarYScaleMax() override { unexpected("IPlayerControlObjectTemplate::getDirBarYScaleMax"); }
    void setNameTagOffset(Vec3) override { unexpected("IPlayerControlObjectTemplate::setNameTagOffset"); }
    const Vec3& getNameTagOffset() override { unexpected("IPlayerControlObjectTemplate::getNameTagOffset"); }
    void setRidingWithOffset(Vec3) override { unexpected("IPlayerControlObjectTemplate::setRidingWithOffset"); }
    const Vec3& getRidingWithOffset() override { unexpected("IPlayerControlObjectTemplate::getRidingWithOffset"); }
    void setRidingWithDistMod(float) override { unexpected("IPlayerControlObjectTemplate::setRidingWithDistMod"); }
    float getRidingWithDistMod() override { unexpected("IPlayerControlObjectTemplate::getRidingWithDistMod"); }
    void setVehicleCameraShake(CameraShakeTriggers) override { unexpected("IPlayerControlObjectTemplate::setVehicleCameraShake"); }
    int getVehicleCameraShake() override { unexpected("IPlayerControlObjectTemplate::getVehicleCameraShake"); }
    void setVehicleFov(float) override { unexpected("IPlayerControlObjectTemplate::setVehicleFov"); }
    float getVehicleFov() override { unexpected("IPlayerControlObjectTemplate::getVehicleFov"); }
    void setSeatLeftRotationLimit(float) override { unexpected("IPlayerControlObjectTemplate::setSeatLeftRotationLimit"); }
    float getSeatLeftRotationLimit() const override { unexpected("IPlayerControlObjectTemplate::getSeatLeftRotationLimit"); }
    void setSeatRightRotationLimit(float) override { unexpected("IPlayerControlObjectTemplate::setSeatRightRotationLimit"); }
    float getSeatRightRotationLimit() const override { unexpected("IPlayerControlObjectTemplate::getSeatRightRotationLimit"); }
    void seatInformation(const std::string&, const Vec3&, const Vec3&) override { unexpected("IPlayerControlObjectTemplate::seatInformation"); }
    void setSeatAnimationSystem(const std::string&) override { unexpected("IPlayerControlObjectTemplate::setSeatAnimationSystem"); }
    const std::string& getSeatAnimationSystem() override { unexpected("IPlayerControlObjectTemplate::getSeatAnimationSystem"); }
    void setSeatParent(const std::string&) override { unexpected("IPlayerControlObjectTemplate::setSeatParent"); }
    const std::string& getSeatParent() const override { unexpected("IPlayerControlObjectTemplate::getSeatParent"); }
    void setSeatPosition(const Vec3&) override { unexpected("IPlayerControlObjectTemplate::setSeatPosition"); }
    const Vec3& getSeatPosition() const override { unexpected("IPlayerControlObjectTemplate::getSeatPosition"); }
    void setSeatRotation(const Vec3&) override { unexpected("IPlayerControlObjectTemplate::setSeatRotation"); }
    const Vec3& getSeatRotation() const override { unexpected("IPlayerControlObjectTemplate::getSeatRotation"); }
    const Mat4& getSeatTransformation() const override { unexpected("IPlayerControlObjectTemplate::getSeatTransformation"); }
    const std::string& getSoundFilter() const override { unexpected("IPlayerControlObjectTemplate::getSoundFilter"); }
    void setSoundFilter(const std::string&) override { unexpected("IPlayerControlObjectTemplate::setSoundFilter"); }
    float getListenerObstruction() const override { unexpected("IPlayerControlObjectTemplate::getListenerObstruction"); }
    void setListenerObstruction(float) override { unexpected("IPlayerControlObjectTemplate::setListenerObstruction"); }
    void setMaintainCameraOnExit(bool) override { unexpected("IPlayerControlObjectTemplate::setMaintainCameraOnExit"); }
    bool getMaintainCameraOnExit() override { unexpected("IPlayerControlObjectTemplate::getMaintainCameraOnExit"); }
    void setMaintainCameraOnEnter(bool) override { unexpected("IPlayerControlObjectTemplate::setMaintainCameraOnEnter"); }
    bool getMaintainCameraOnEnter() override { unexpected("IPlayerControlObjectTemplate::getMaintainCameraOnEnter"); }
    void setNeverDrawAs1p(bool) override { unexpected("IPlayerControlObjectTemplate::setNeverDrawAs1p"); }
    bool getNeverDrawAs1p() const override { unexpected("IPlayerControlObjectTemplate::getNeverDrawAs1p"); }
};
} } }

static std::string joined()
{
    std::string r;
    for (const std::string& c : calls) r += c + ";";
    return r;
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    FakeHapticSettings settings;
    // ---- applyHapticProfile: five copies, each getter called once per component ----
    const char* names[5] = { "SOLDIER", "AIR", "HELI", "LAND", "SEA" };
    const char* kinds[5] = { "ControlBoxLength", "ControlBoxStiffness", "AimSensitivity", "AimSensitivityLength", "TurnSensitivity" };
    const HapticProfile profiles[5] = { HapticProfileSoldier, HapticProfileAir, HapticProfileHeli, HapticProfileLand, HapticProfileSea };
    for (int p = 0; p < 5; ++p) {
        calls.clear();
        applyHapticProfile(&settings, profiles[p]);
        std::vector<std::string> sets;
        std::map<std::string, int> gets;
        for (const std::string& c : calls) {
            if (c.compare(0, 3, "set") == 0) sets.push_back(c);
            else ++gets[c];
        }
        bool ok = sets.size() == 5 && gets.size() == 5 && calls.size() == 20;
        for (int k = 0; k < 5 && ok; ++k) {
            const int base = p * 100 + k * 10;
            ok = sets[k] == std::string("set") + kinds[k] + " " + fmt(base + 1.0f, base + 2.0f, base + 3.0f)
                 && gets[std::string("get") + kinds[k] + names[p]] == 3;
        }
        // Each copy's three getter calls come right before its setter.
        for (int k = 0; k < 5 && ok; ++k)
            for (int g = 0; g < 3; ++g) ok = calls[k * 4 + g] == std::string("get") + kinds[k] + names[p];
        check(ok, std::string("profile ") + names[p] + ": the five copies from its own values, 3 getter calls each");
    }

    // ---- hapticProfileForEntered ----
    FakeTemplate entered, root;
    entered.name = "entered";
    root.name = "root";
    const int soldierTypes[] = { 8, 9, 15, 19, 20, 24, 25 };
    for (int t : soldierTypes) {
        entered.vehicleType = t;
        root.category = 2;
        calls.clear();
        check(hapticProfileForEntered(&entered, &root) == HapticProfileSoldier && joined() == "entered.type;",
              "type " + std::to_string(t) + ": SOLDIER, nothing else read");
    }
    entered.vehicleType = 26;
    const struct { int category, controls; HapticProfile expected; const char* reads; } t26[] = {
        { 2, 0, HapticProfileAir, "entered.type;entered.category;" },
        { 3, 1, HapticProfileHeli, "entered.type;entered.category;entered.controls;" },
        { 3, 0, HapticProfileSoldier, "entered.type;entered.category;entered.controls;" },
        { 0, 1, HapticProfileSoldier, "entered.type;entered.category;" },
        { 1, 1, HapticProfileSoldier, "entered.type;entered.category;" },
        { 7, 1, HapticProfileSoldier, "entered.type;entered.category;" },
    };
    for (const auto& c : t26) {
        entered.category = c.category;
        entered.controls = c.controls;
        root.category = 0;
        calls.clear();
        check(hapticProfileForEntered(&entered, &root) == c.expected && joined() == c.reads,
              "type 26, category " + std::to_string(c.category) + ", controls " + std::to_string(c.controls));
    }
    const int otherTypes[] = { -1, 0, 7, 10, 11, 12, 13, 14, 16, 17, 18, 21, 22, 23, 27, 100 };
    const struct { int category; HapticProfile expected; } byRoot[] = {
        { 0, HapticProfileLand }, { 1, HapticProfileSea }, { 2, HapticProfileAir }, { 3, HapticProfileHeli },
        { 4, HapticProfileSoldier }, { -1, HapticProfileSoldier },
    };
    for (int t : otherTypes) {
        bool ok = true;
        for (const auto& r : byRoot) {
            entered.vehicleType = t;
            entered.category = 2;  // never read for these types
            root.category = r.category;
            calls.clear();
            ok = ok && hapticProfileForEntered(&entered, &root) == r.expected && joined() == "entered.type;root.category;";
        }
        check(ok, "type " + std::to_string(t) + ": the root's category (0 LAND, 1 SEA, 2 AIR, 3 HELI, else SOLDIER)");
    }

    std::printf("soldier haptic profile %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
