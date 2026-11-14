// [TEMP] [CHOSEN, not recovered] The Soldier's Novint control-box profile switch
// (Game/Objects/Game/SoldierHapticProfile.cpp, ledger N2 17a) against a complete fake HapticSettings and
// fake player-control templates (CTest bf2_soldier_haptic_profile_check). The fakes are generated from
// their headers and fail loudly for every call the code under test must not make.
#include "../BF2/Game/Objects/Game/SoldierHapticProfile.h"
#include "../BF2/Game/HapticSettings.h"
#include "../BF2/Game/Objects/IPlayerControlObjectTemplate.h"
#include "../BF2/Game/Objects/GenericFireArmTemplate.h"
#include "../BF2/Game/Objects/Weapons/GenericFireArm.h"
#include "../BF2/IO/Input/FalconHaptics.h"
#include "../BF2/Game/Objects/IObject.h"
#include "../BF2/Game/Objects/IPlayer.h"
#include "../BF2/Math/Mat4.h"
#include "../BF2/Physics/IPhysicsNode.h"
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

// ---- updateSoldierRecoilForce: io::updateRecoilForce is recorded ----
void dice::hfe::world::HapticWeaponAccess::setTemplate(IObject* weapon, const GenericFireArmTemplate* weaponTemplate)
{
    static_cast<GenericFireArm*>(weapon)->m_template = weaponTemplate;
}
static std::vector<std::string> recoilCalls;
namespace dice { namespace hfe { namespace io {
void updateRecoilForce(float yawDelta, float pitchRecoil, int weaponType)
{
    char b[96];
    std::snprintf(b, sizeof(b), "recoil %.3f %.3f %d", yawDelta, pitchRecoil, weaponType);
    recoilCalls.push_back(b);
}
} } }
namespace dice { namespace hfe { namespace io {
void updateSoldierCraftPhysicsForce(const Vec3& v, float dt)
{
    char b[128];
    std::snprintf(b, sizeof(b), "craft %.6g %.6g %.6g %.3f", v.x, v.y, v.z, dt);
    recoilCalls.push_back(b);
}
void updateVehicleCraftPhysicsForce(const Vec3& v, float dt, int category)
{
    char b[128];
    std::snprintf(b, sizeof(b), "vehicle craft %.6g %.6g %.6g %.3f %d", v.x, v.y, v.z, dt, category);
    recoilCalls.push_back(b);
}
} } }
// ---- updateSoldierCraftPhysics: a controlling player whose vehicle has a fixed inverse transform ----
namespace dice { namespace hfe { namespace world {
// IObject's out-of-line members (IObject.cpp brings the object manager); none is reached.
IObject::IObject() {}
IObject::~IObject() {}
IPhysicsNode::IPhysicsNode() {}
IPhysicsNode::~IPhysicsNode() {}
WeakPtr<IObject> IObject::getWeakPtr() { unexpected("IObject::getWeakPtr"); }
void IObject::SetActiveLod(ObjectLodType) { unexpected("IObject::SetActiveLod"); }
void IObject::forceDestroy() { unexpected("IObject::forceDestroy"); }
std::string IObject::getArmorInfo() { unexpected("IObject::getArmorInfo"); }
void IObject::destroyObject() { unexpected("IObject::destroyObject"); }
void IObject::setObjectCacheID(unsigned int) { unexpected("IObject::setObjectCacheID"); }
void IObject::enableCachedObject() { unexpected("IObject::enableCachedObject"); }
void IObject::disableCachedObject() { unexpected("IObject::disableCachedObject"); }
void IObject::setGridCullerObject(CullerObject*) { unexpected("IObject::setGridCullerObject"); }
void IObject::setLatencyCullerObject(CullerObject*) { unexpected("IObject::setLatencyCullerObject"); }
void IObject::internal_assignNewObjectID() { unexpected("IObject::internal_assignNewObjectID"); }
void* IObject::getDecalObject() const { unexpected("IObject::getDecalObject"); }
io::INetworkable* IObject::getNetworkable() const { unexpected("IObject::getNetworkable"); }
int IObject::getGeometryPart() const { unexpected("IObject::getGeometryPart"); }
bool IObject::hasCollisionLod(int) const { unexpected("IObject::hasCollisionLod"); }
unsigned int IObject::getObjectCacheID() const { unexpected("IObject::getObjectCacheID"); }
CullerObject* IObject::getGridCullerObject() const { unexpected("IObject::getGridCullerObject"); }
CullerObject* IObject::getLatencyCullerObject() const { unexpected("IObject::getLatencyCullerObject"); }
bool IObject::isCreatedByLocalPlayer() const { unexpected("IObject::isCreatedByLocalPlayer"); }
} } }
namespace {
using namespace dice::hfe::world;
using dice::hfe::Mat4;
struct ShakeVehicle final : IObject {
    Mat4 inverse;
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("IObject::queryInterface"); }
    const Mat4& getInvAbsoluteTransformation() const override { recoilCalls.push_back("vehicle.inverse"); return inverse; }
    unsigned int getClassID() const override { unexpected("IObject::getClassID"); }
    WeakPtr<IObject> getWeakPtr() override { unexpected("IObject::getWeakPtr"); }
    void init() override { unexpected("IObject::init"); }
    void disableCachedObject() override { unexpected("IObject::disableCachedObject"); }
    void enableCachedObject() override { unexpected("IObject::enableCachedObject"); }
    unsigned int getObjectCacheID() const override { unexpected("IObject::getObjectCacheID"); }
    void setObjectCacheID(unsigned int) override { unexpected("IObject::setObjectCacheID"); }
    void destroy(bool force) override { unexpected("IObject::destroy"); }
    void forceDestroy() override { unexpected("IObject::forceDestroy"); }
    bool isObjectDestroyed() override { unexpected("IObject::isObjectDestroyed"); }
    void setObjectInDestroyList(bool) override { unexpected("IObject::setObjectInDestroyList"); }
    bool getObjectInDestroyList() const override { unexpected("IObject::getObjectInDestroyList"); }
    void setGridCullerObject(CullerObject*) override { unexpected("IObject::setGridCullerObject"); }
    CullerObject* getGridCullerObject() const override { unexpected("IObject::getGridCullerObject"); }
    void setLatencyCullerObject(CullerObject*) override { unexpected("IObject::setLatencyCullerObject"); }
    CullerObject* getLatencyCullerObject() const override { unexpected("IObject::getLatencyCullerObject"); }
    bool setComponent(unsigned int classId, IBase*) override { unexpected("IObject::setComponent"); }
    IBase* queryComponent(unsigned int classId, unsigned int iid) override { unexpected("IObject::queryComponent"); }
    void updateFlags(unsigned int set, unsigned int clear) override { unexpected("IObject::updateFlags"); }
    const std::string& getName() const override { unexpected("IObject::getName"); }
    void setName(const std::string&) override { unexpected("IObject::setName"); }
    const Vec3& getAbsolutePosition() const override { unexpected("IObject::getAbsolutePosition"); }
    void setAbsolutePosition(const Vec3&) override { unexpected("IObject::setAbsolutePosition"); }
    const Vec3& getAbsolutePositionSecondary() const override { unexpected("IObject::getAbsolutePositionSecondary"); }
    void setAbsolutePositionSecondary(const Vec3&) override { unexpected("IObject::setAbsolutePositionSecondary"); }
    const Vec3& getRotationSecondary() override { unexpected("IObject::getRotationSecondary"); }
    void setRotationSecondary(const Vec3&) override { unexpected("IObject::setRotationSecondary"); }
    const Mat4& getAbsoluteTransformation() const override { unexpected("IObject::getAbsoluteTransformation"); }
    void setAbsoluteTransformation(const Mat4&) override { unexpected("IObject::setAbsoluteTransformation"); }
    const Mat4& getVisualAbsoluteTransformation() const override { unexpected("IObject::getVisualAbsoluteTransformation"); }
    float getVisualBoundingRadius() const override { unexpected("IObject::getVisualBoundingRadius"); }
    float getPhysicBoundingRadius() const override { unexpected("IObject::getPhysicBoundingRadius"); }
    float getBoundingRadiusModifier() const override { unexpected("IObject::getBoundingRadiusModifier"); }
    LocalBoundingBox* getFullBoundingBox() override { unexpected("IObject::getFullBoundingBox"); }
    LocalBoundingBox* getMineBoundingBox() override { unexpected("IObject::getMineBoundingBox"); }
    void handleVisualUpdate(float, float) override { unexpected("IObject::handleVisualUpdate"); }
    void handleFrameUpdate(float) override { unexpected("IObject::handleFrameUpdate"); }
    void handleUpdate(float, unsigned int) override { unexpected("IObject::handleUpdate"); }
    bool handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&, int, int, CollisionFaceInfo*, CollisionFaceInfo*) override { unexpected("IObject::handleCollision"); }
    void updateGeometry() override { unexpected("IObject::updateGeometry"); }
    int getUpdateFrequency() const override { unexpected("IObject::getUpdateFrequency"); }
    UpdateFreqencyType getUpdateFrequencyType() const override { unexpected("IObject::getUpdateFrequencyType"); }
    void setUpdateFrequencyType(UpdateFreqencyType) override { unexpected("IObject::setUpdateFrequencyType"); }
    int getGeometryPart() const override { unexpected("IObject::getGeometryPart"); }
    bool hasCollisionLod(int) const override { unexpected("IObject::hasCollisionLod"); }
    int getMaterial() const override { unexpected("IObject::getMaterial"); }
    void SetActiveLod(ObjectLodType) override { unexpected("IObject::SetActiveLod"); }
    io::INetworkable* getNetworkable() const override { unexpected("IObject::getNetworkable"); }
    void* getDecalObject() const override { unexpected("IObject::getDecalObject"); }
    const Vec3& getRelativePosition() const override { unexpected("IObject::getRelativePosition"); }
    void setRelativePosition(const Vec3&) override { unexpected("IObject::setRelativePosition"); }
    const Mat4& getRelativeTransformation() const override { unexpected("IObject::getRelativeTransformation"); }
    void setRelativeTransformation(const Mat4&) override { unexpected("IObject::setRelativeTransformation"); }
    IObject* getChild() const override { unexpected("IObject::getChild"); }
    void addChild(IObject*) override { unexpected("IObject::addChild"); }
    void removeChild(IObject*) override { unexpected("IObject::removeChild"); }
    void resetCachedRootParents() override { unexpected("IObject::resetCachedRootParents"); }
    void handlePlayerInput(IPlayer*, const io::PlayerInput&, float, unsigned int) override { unexpected("IObject::handlePlayerInput"); }
    void handleMessage(TemplateMessage, IPlayer*, bool) override { unexpected("IObject::handleMessage"); }
    char getInputId() const override { unexpected("IObject::getInputId"); }
    void setInputId(char) override { unexpected("IObject::setInputId"); }
    char getChildId() const override { unexpected("IObject::getChildId"); }
    void setChildId(char) override { unexpected("IObject::setChildId"); }
    unsigned char getAlpha() override { unexpected("IObject::getAlpha"); }
    unsigned char getLayer() override { unexpected("IObject::getLayer"); }
    void setLayer(unsigned char) override { unexpected("IObject::setLayer"); }
    unsigned char getGroup() override { unexpected("IObject::getGroup"); }
    void setGroup(unsigned char) override { unexpected("IObject::setGroup"); }
    void setIsOvergrowth(bool) override { unexpected("IObject::setIsOvergrowth"); }
    bool getIsOvergrowth() override { unexpected("IObject::getIsOvergrowth"); }
    void setNotInAI(bool) override { unexpected("IObject::setNotInAI"); }
    bool getNotInAI() const override { unexpected("IObject::getNotInAI"); }
    void setLightSourceMask(int) override { unexpected("IObject::setLightSourceMask"); }
    int getLightSourceMask() const override { unexpected("IObject::getLightSourceMask"); }
    bool getDisableChildren() override { unexpected("IObject::getDisableChildren"); }
    void setDisableChildren(bool) override { unexpected("IObject::setDisableChildren"); }
    std::string getArmorInfo() override { unexpected("IObject::getArmorInfo"); }
    bool isCreatedByLocalPlayer() const override { unexpected("IObject::isCreatedByLocalPlayer"); }
    void internal_assignNewObjectID() override { unexpected("IObject::internal_assignNewObjectID"); }
    void destroyObject() override { unexpected("IObject::destroyObject"); }
    void addSibling(IObject*) override { unexpected("IObject::addSibling"); }
    void removeSibling(IObject*) override { unexpected("IObject::removeSibling"); }
    void setParent(IObject*) override { unexpected("IObject::setParent"); }
};
struct Controller final : IPlayer {
    IObject* vehicle = 0;
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("IPlayer::queryInterface"); }
    IObject* getVehicle() const override { recoilCalls.push_back("player.vehicle"); return vehicle; }
    unsigned int getClassID() const override { unexpected("IPlayer::getClassID"); }
    WeakPtr<IPlayer> getWeakPtr() override { unexpected("IPlayer::getWeakPtr"); }
    void setName(const std::string& name) override { unexpected("IPlayer::setName"); }
    const std::string& getName() const override { unexpected("IPlayer::getName"); }
    void setFlags(const Flag32Bits<IPlayer>& flags) override { unexpected("IPlayer::setFlags"); }
    bool testFlags(const Flag32Bits<IPlayer>& flags) const override { unexpected("IPlayer::testFlags"); }
    void handleUpdate(float dt, unsigned int) override { unexpected("IPlayer::handleUpdate"); }
    void handleFrameUpdate(float dt) override { unexpected("IPlayer::handleFrameUpdate"); }
    UpdateFreqencyType getUpdateFrequencyType() const override { unexpected("IPlayer::getUpdateFrequencyType"); }
    void setUpdateFrequencyType(UpdateFreqencyType type) override { unexpected("IPlayer::setUpdateFrequencyType"); }
    void handleInput(io::PlayerInput& input, float dt, unsigned int) override { unexpected("IPlayer::handleInput"); }
    void getAutoInput(io::PlayerInput& out, bool) override { unexpected("IPlayer::getAutoInput"); }
    bool setVehicle(IObject* vehicle, int seat) override { unexpected("IPlayer::setVehicle"); }
    int getInputId() const override { unexpected("IPlayer::getInputId"); }
    int getId() const override { unexpected("IPlayer::getId"); }
    void setIsRemote(bool remote) override { unexpected("IPlayer::setIsRemote"); }
    bool getIsRemote() const override { unexpected("IPlayer::getIsRemote"); }
    void setIsAIPlayer(bool ai) override { unexpected("IPlayer::setIsAIPlayer"); }
    bool getIsAIPlayer() const override { unexpected("IPlayer::getIsAIPlayer"); }
    void setIsAlive(bool alive) override { unexpected("IPlayer::setIsAlive"); }
    bool getIsAlive() const override { unexpected("IPlayer::getIsAlive"); }
    void setSuicide(bool) override { unexpected("IPlayer::setSuicide"); }
    bool getSuicide() const override { unexpected("IPlayer::getSuicide"); }
    bool getIsConnected() const override { unexpected("IPlayer::getIsConnected"); }
    void setIsConnected(bool) override { unexpected("IPlayer::setIsConnected"); }
    float getCrossHairFadeValue() const override { unexpected("IPlayer::getCrossHairFadeValue"); }
    void setViewDistance(float) override { unexpected("IPlayer::setViewDistance"); }
    float getViewDistance() const override { unexpected("IPlayer::getViewDistance"); }
    void setFov(float fov, float time) override { unexpected("IPlayer::setFov"); }
    float getFov() const override { unexpected("IPlayer::getFov"); }
    float getEndFov() const override { unexpected("IPlayer::getEndFov"); }
    ActionBuffer* getActionBuffer() override { unexpected("IPlayer::getActionBuffer"); }
    void* getAIPlayerInput() const override { unexpected("IPlayer::getAIPlayerInput"); }
    void setDefaultVehicle(IObject*) override { unexpected("IPlayer::setDefaultVehicle"); }
    IObject* getDefaultVehicle() override { unexpected("IPlayer::getDefaultVehicle"); }
    IObject* getFreeCamera() const override { unexpected("IPlayer::getFreeCamera"); }
    void setFreeCameraEnabled(bool) override { unexpected("IPlayer::setFreeCameraEnabled"); }
    bool getFreeCameraEnabled() const override { unexpected("IPlayer::getFreeCameraEnabled"); }
    void toggleFreeCameraEnabled() override { unexpected("IPlayer::toggleFreeCameraEnabled"); }
    void setOverheadCameraEnabled(bool) override { unexpected("IPlayer::setOverheadCameraEnabled"); }
    bool getOverheadCameraEnabled() const override { unexpected("IPlayer::getOverheadCameraEnabled"); }
    void setOverheadCameraTopDownView(bool) override { unexpected("IPlayer::setOverheadCameraTopDownView"); }
    bool getOverheadCameraTopDownView() const override { unexpected("IPlayer::getOverheadCameraTopDownView"); }
    void setCommanderCamOverhead(bool) override { unexpected("IPlayer::setCommanderCamOverhead"); }
    bool getCommanderCamOverhead() const override { unexpected("IPlayer::getCommanderCamOverhead"); }
    int getTickCount() const override { unexpected("IPlayer::getTickCount"); }
    void addTick() override { unexpected("IPlayer::addTick"); }
    void subTick() override { unexpected("IPlayer::subTick"); }
    void resetTickCounter() override { unexpected("IPlayer::resetTickCounter"); }
    float getTimeOutSideWorld() const override { unexpected("IPlayer::getTimeOutSideWorld"); }
    void setTeam(int team, bool) override { unexpected("IPlayer::setTeam"); }
    int getTeam() const override { unexpected("IPlayer::getTeam"); }
    unsigned int getSpawnAtTick() const override { unexpected("IPlayer::getSpawnAtTick"); }
    void setSpawnAtTick(unsigned int tick) override { unexpected("IPlayer::setSpawnAtTick"); }
    float getTimeToSpawn() const override { unexpected("IPlayer::getTimeToSpawn"); }
    float getManDownTimeLeft() const override { unexpected("IPlayer::getManDownTimeLeft"); }
    void setIsManDown(bool) override { unexpected("IPlayer::setIsManDown"); }
    bool getIsManDown() override { unexpected("IPlayer::getIsManDown"); }
    unsigned int getManDownAtTick() override { unexpected("IPlayer::getManDownAtTick"); }
    void setSquadId(unsigned int id) override { unexpected("IPlayer::setSquadId"); }
    int getSquadId() const override { unexpected("IPlayer::getSquadId"); }
    void setIsCommander(bool) override { unexpected("IPlayer::setIsCommander"); }
    bool getIsCommander() const override { unexpected("IPlayer::getIsCommander"); }
    void setUAVIsReady(bool) override { unexpected("IPlayer::setUAVIsReady"); }
    bool getUAVIsReady() const override { unexpected("IPlayer::getUAVIsReady"); }
    void setSatelliteIsReady(bool) override { unexpected("IPlayer::setSatelliteIsReady"); }
    bool getSatelliteIsReady() const override { unexpected("IPlayer::getSatelliteIsReady"); }
    void setLastTimeCommander(double t) override { unexpected("IPlayer::setLastTimeCommander"); }
    double getLastTimeCommander() const override { unexpected("IPlayer::getLastTimeCommander"); }
    void setCommanderReapplyWaitTime(double t) override { unexpected("IPlayer::setCommanderReapplyWaitTime"); }
    double getCommanderReapplyWaitTime() const override { unexpected("IPlayer::getCommanderReapplyWaitTime"); }
    void setIsSquadLeader(bool) override { unexpected("IPlayer::setIsSquadLeader"); }
    bool getIsSquadLeader() const override { unexpected("IPlayer::getIsSquadLeader"); }
    bool getIsGassed() override { unexpected("IPlayer::getIsGassed"); }
    void setIsInsideCP(bool) override { unexpected("IPlayer::setIsInsideCP"); }
    bool getIsInsideCP() override { unexpected("IPlayer::getIsInsideCP"); }
    void setHasSimulatedFrame(bool) override { unexpected("IPlayer::setHasSimulatedFrame"); }
    bool getHasSimulatedFrame() const override { unexpected("IPlayer::getHasSimulatedFrame"); }
    void setSpawnGroup(int group) override { unexpected("IPlayer::setSpawnGroup"); }
    int getSpawnGroup() const override { unexpected("IPlayer::getSpawnGroup"); }
    void setInterpolationTime(unsigned int ms) override { unexpected("IPlayer::setInterpolationTime"); }
    unsigned int getInterpolationTime() const override { unexpected("IPlayer::getInterpolationTime"); }
    void setPing(unsigned int ping) override { unexpected("IPlayer::setPing"); }
    unsigned int getPing() const override { unexpected("IPlayer::getPing"); }
    void setKit(int kit) override { unexpected("IPlayer::setKit"); }
    int getKit() const override { unexpected("IPlayer::getKit"); }
    PlayerScoreData* getScoreData() override { unexpected("IPlayer::getScoreData"); }
    void setFlagHolder(bool) override { unexpected("IPlayer::setFlagHolder"); }
    bool getFlagHolder() const override { unexpected("IPlayer::getFlagHolder"); }
    ICameraObject* getCamera() const override { unexpected("IPlayer::getCamera"); }
    void setCamera(ICameraObject* camera) override { unexpected("IPlayer::setCamera"); }
    bool hasFirstPersonView() const override { unexpected("IPlayer::hasFirstPersonView"); }
    double getTimeConnected() const override { unexpected("IPlayer::getTimeConnected"); }
    io::INetworkable* getNetworkable() override { unexpected("IPlayer::getNetworkable"); }
    bool getStateSync(io::BitStream* stream, IObject* object) override { unexpected("IPlayer::getStateSync"); }
    bool setStateSync(io::BitStream* stream, IObject* object, double) override { unexpected("IPlayer::setStateSync"); }
    void reset() override { unexpected("IPlayer::reset"); }
    void setFiringState(bool) override { unexpected("IPlayer::setFiringState"); }
    bool getFiringState() const override { unexpected("IPlayer::getFiringState"); }
    void setAltFiringState(bool) override { unexpected("IPlayer::setAltFiringState"); }
    bool getAltFiringState() const override { unexpected("IPlayer::getAltFiringState"); }
    void setFlareFiringState(bool) override { unexpected("IPlayer::setFlareFiringState"); }
    bool getFlareFiringState() const override { unexpected("IPlayer::getFlareFiringState"); }
    void setSprintState(bool) override { unexpected("IPlayer::setSprintState"); }
    bool getSprintState() const override { unexpected("IPlayer::getSprintState"); }
    void setSprintStateLastTick(bool) override { unexpected("IPlayer::setSprintStateLastTick"); }
    bool getSprintStateLastTick() const override { unexpected("IPlayer::getSprintStateLastTick"); }
    void setReloadState() override { unexpected("IPlayer::setReloadState"); }
    bool getAndResetReloadState() override { unexpected("IPlayer::getAndResetReloadState"); }
    void updateFiringState(unsigned int, IObject*) override { unexpected("IPlayer::updateFiringState"); }
    void updateGhostFiringState() override { unexpected("IPlayer::updateGhostFiringState"); }
    void setHitIndicator(bool) override { unexpected("IPlayer::setHitIndicator"); }
    bool getHitIndicator() const override { unexpected("IPlayer::getHitIndicator"); }
    float updateHitFromDirection(const Vec3& direction) override { unexpected("IPlayer::updateHitFromDirection"); }
    float getHitFromDirection() const override { unexpected("IPlayer::getHitFromDirection"); }
    void updateCameraShake(float amount) override { unexpected("IPlayer::updateCameraShake"); }
    float getCameraShake() const override { unexpected("IPlayer::getCameraShake"); }
    unsigned int getSquadInviteMask() const override { unexpected("IPlayer::getSquadInviteMask"); }
    void setSquadInviteMask(unsigned int mask) override { unexpected("IPlayer::setSquadInviteMask"); }
    bool getIsInvitedToSquad(int squad) const override { unexpected("IPlayer::getIsInvitedToSquad"); }
    void setKitTimeStamp(double t) override { unexpected("IPlayer::setKitTimeStamp"); }
    double getKitTimeStamp() const override { unexpected("IPlayer::getKitTimeStamp"); }
    void setCurrentFiringButton(io::PlayerInputMap button) override { unexpected("IPlayer::setCurrentFiringButton"); }
    io::PlayerInputMap getCurrentFiringButton() const override { unexpected("IPlayer::getCurrentFiringButton"); }
    void setPersonalDecalTemplate(const IObjectTemplate* decal) override { unexpected("IPlayer::setPersonalDecalTemplate"); }
    const IObjectTemplate* getPersonalDecalTemplate() const override { unexpected("IPlayer::getPersonalDecalTemplate"); }
    void enableVOHelp(bool) override { unexpected("IPlayer::enableVOHelp"); }
    void enableVOHelpMessage(int) override { unexpected("IPlayer::enableVOHelpMessage"); }
    void disableVOHelpMessage(int) override { unexpected("IPlayer::disableVOHelpMessage"); }
    bool getVOHelpMessagePlayed(int) override { unexpected("IPlayer::getVOHelpMessagePlayed"); }
    void doAddTouchItem(IPlayer* other) override { unexpected("IPlayer::doAddTouchItem"); }
    void setHealRepairedAtTick(unsigned int tick, bool giving, bool repairing) override { unexpected("IPlayer::setHealRepairedAtTick"); }
    unsigned int getHealRepairedAtTick() const override { unexpected("IPlayer::getHealRepairedAtTick"); }
    void setGaveAmmoAtTick(unsigned int tick, bool giving) override { unexpected("IPlayer::setGaveAmmoAtTick"); }
    unsigned int getGaveAmmoAtTick() const override { unexpected("IPlayer::getGaveAmmoAtTick"); }
    void setFailedExitAtTick(unsigned int tick) override { unexpected("IPlayer::setFailedExitAtTick"); }
    unsigned int getFailedExitAtTick() const override { unexpected("IPlayer::getFailedExitAtTick"); }
    bool getIsGivingAbility() const override { unexpected("IPlayer::getIsGivingAbility"); }
    bool getIsRepairing() const override { unexpected("IPlayer::getIsRepairing"); }
    bool getIsGivingReplenish() const override { unexpected("IPlayer::getIsGivingReplenish"); }
    int getChosenUnlockLevel(int kit, int slot) const override { unexpected("IPlayer::getChosenUnlockLevel"); }
    void setChosenUnlockLevel(int kit, int slot, int level) override { unexpected("IPlayer::setChosenUnlockLevel"); }
    bool isUnlocked(int kit, int slot, int level) override { unexpected("IPlayer::isUnlocked"); }
    int getUnlockLevel(int kit, int slot) override { unexpected("IPlayer::getUnlockLevel"); }
    void setUnlocks(std::map<int, int>* unlocks) override { unexpected("IPlayer::setUnlocks"); }
    std::map<int, int>* getUnlocks() override { unexpected("IPlayer::getUnlocks"); }
    void setPeerAddress(const std::string& address) override { unexpected("IPlayer::setPeerAddress"); }
    const std::string& getPeerAddress() const override { unexpected("IPlayer::getPeerAddress"); }
    bool getVoipEnabled() override { unexpected("IPlayer::getVoipEnabled"); }
    unsigned short getVoipSessionId() override { unexpected("IPlayer::getVoipSessionId"); }
    void setVoipSessionId(unsigned short id) override { unexpected("IPlayer::setVoipSessionId"); }
    bool getIsMuted() override { unexpected("IPlayer::getIsMuted"); }
    bool getIsTalking() override { unexpected("IPlayer::getIsTalking"); }
    double getLastTalkTime() override { unexpected("IPlayer::getLastTalkTime"); }
    double getStopTalkingThreshold() override { unexpected("IPlayer::getStopTalkingThreshold"); }
    void setIsMuted(bool) override { unexpected("IPlayer::setIsMuted"); }
    void setIsTalking(bool) override { unexpected("IPlayer::setIsTalking"); }
    void setVoipEnabled(bool) override { unexpected("IPlayer::setVoipEnabled"); }
    void setVotingStatus(PlayerVotingStatus status) override { unexpected("IPlayer::setVotingStatus"); }
    PlayerVotingStatus getVotingStatus() override { unexpected("IPlayer::getVotingStatus"); }
    void setParticipateInVoting(bool) override { unexpected("IPlayer::setParticipateInVoting"); }
    bool getParticipateInVoting() override { unexpected("IPlayer::getParticipateInVoting"); }
    void setTargetOfVote(bool) override { unexpected("IPlayer::setTargetOfVote"); }
    bool getTargetOfVote() override { unexpected("IPlayer::getTargetOfVote"); }
    int getProfileId() override { unexpected("IPlayer::getProfileId"); }
    void setProfileId(int id) override { unexpected("IPlayer::setProfileId"); }
    void setBlurryVision(float, float, float) override { unexpected("IPlayer::setBlurryVision"); }
    float getBlurValue() const override { unexpected("IPlayer::getBlurValue"); }
    bool getIsNearGasCloud() const override { unexpected("IPlayer::getIsNearGasCloud"); }
    bool getIsGasMaskOn() const override { unexpected("IPlayer::getIsGasMaskOn"); }
    void setIsGasMaskOn(bool, bool) override { unexpected("IPlayer::setIsGasMaskOn"); }
    void setGasMaskHudItem(bool) override { unexpected("IPlayer::setGasMaskHudItem"); }
    float getPlayerGasMaskChangeTimer() const override { unexpected("IPlayer::getPlayerGasMaskChangeTimer"); }
    float getPlayerNightVisionChangeTimer() const override { unexpected("IPlayer::getPlayerNightVisionChangeTimer"); }
    bool getIsUnderwater() const override { unexpected("IPlayer::getIsUnderwater"); }
    float getNormalizedNightVisionPower() override { unexpected("IPlayer::getNormalizedNightVisionPower"); }
    void setNightVision(bool, bool) override { unexpected("IPlayer::setNightVision"); }
    void rechargeNightVision() override { unexpected("IPlayer::rechargeNightVision"); }
    void setVisualNightVision(bool) override { unexpected("IPlayer::setVisualNightVision"); }
    bool getNightVision() const override { unexpected("IPlayer::getNightVision"); }
    void disableFlashBangEffect() override { unexpected("IPlayer::disableFlashBangEffect"); }
    void disableBlurryVision() override { unexpected("IPlayer::disableBlurryVision"); }
    io::PlayerInput getPlayerInput() override { unexpected("IPlayer::getPlayerInput"); }
    void resetOnDeath() override { unexpected("IPlayer::resetOnDeath"); }
    bool isPlayerInNightMap() const override { unexpected("IPlayer::isPlayerInNightMap"); }
    void setTracePlayerId(int id) override { unexpected("IPlayer::setTracePlayerId"); }
    int getTracePlayerId() override { unexpected("IPlayer::getTracePlayerId"); }
    void allocateFreeCamera() override { unexpected("IPlayer::allocateFreeCamera"); }
    bool getIsThisASpam(bool) override { unexpected("IPlayer::getIsThisASpam"); }
    void setRadioSpamFlagCount(int) override { unexpected("IPlayer::setRadioSpamFlagCount"); }
    int getRadioSpamFlagCount() override { unexpected("IPlayer::getRadioSpamFlagCount"); }
    void setRadioSpamBlockedFlagCount(int) override { unexpected("IPlayer::setRadioSpamBlockedFlagCount"); }
    int getRadioSpamBlockedFlagCount() override { unexpected("IPlayer::getRadioSpamBlockedFlagCount"); }
    void setRadioSpamFilterFlagTimer(float) override { unexpected("IPlayer::setRadioSpamFilterFlagTimer"); }
    float getRadioSpamFilterFlagTimer() override { unexpected("IPlayer::getRadioSpamFilterFlagTimer"); }
    void resetSpamFilter() override { unexpected("IPlayer::resetSpamFilter"); }
    void getCloseMines(std::vector<IObject*>& out) const override { unexpected("IPlayer::getCloseMines"); }
    bool getAutoReload() const override { unexpected("IPlayer::getAutoReload"); }
    void setAutoReload(bool) override { unexpected("IPlayer::setAutoReload"); }
    void updateClientSettings() override { unexpected("IPlayer::updateClientSettings"); }
};
struct HapticPhysics final : IPhysicsNode {
    Vec3 speed;
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("IPhysicsNode::queryInterface"); }
    const Vec3& getPositionalSpeed() const override { recoilCalls.push_back("physics.speed"); return speed; }
    const Vec3& getPositionalAcceleration() const override { recoilCalls.push_back("physics.acceleration"); return speed; }
    void setEnabled(bool) override { unexpected("IPhysicsNode::setEnabled"); }
    bool getEnabled() const override { unexpected("IPhysicsNode::getEnabled"); }
    void setIsDamaged(bool) override { unexpected("IPhysicsNode::setIsDamaged"); }
    bool getIsDamaged() const override { unexpected("IPhysicsNode::getIsDamaged"); }
    void setObject(IObject*) override { unexpected("IPhysicsNode::setObject"); }
    IObject* getObject() const override { unexpected("IPhysicsNode::getObject"); }
    void setParent(IPhysicsNode*) override { unexpected("IPhysicsNode::setParent"); }
    IPhysicsNode* getParent() const override { unexpected("IPhysicsNode::getParent"); }
    float getBoundingRadius() const override { unexpected("IPhysicsNode::getBoundingRadius"); }
    const Mat4& getAbsoluteTransformation() const override { unexpected("IPhysicsNode::getAbsoluteTransformation"); }
    void setAbsoluteTransformation(const Mat4&) override { unexpected("IPhysicsNode::setAbsoluteTransformation"); }
    const Mat4& getRelativeTransformation() const override { unexpected("IPhysicsNode::getRelativeTransformation"); }
    const Vec3& getAbsolutePosition() const override { unexpected("IPhysicsNode::getAbsolutePosition"); }
    void setAbsolutePosition(const Vec3&) override { unexpected("IPhysicsNode::setAbsolutePosition"); }
    const Mat4& getPrevTransformation() const override { unexpected("IPhysicsNode::getPrevTransformation"); }
    void setPrevTransformation(const Mat4&) override { unexpected("IPhysicsNode::setPrevTransformation"); }
    void setOldPosition(const Vec3&) override { unexpected("IPhysicsNode::setOldPosition"); }
    const Vec3& getRelativePosition() const override { unexpected("IPhysicsNode::getRelativePosition"); }
    const Vec3& getRotationalSpeed() const override { unexpected("IPhysicsNode::getRotationalSpeed"); }
    void setPositionalSpeed(const Vec3&) override { unexpected("IPhysicsNode::setPositionalSpeed"); }
    void setRotationalSpeed(const Vec3&) override { unexpected("IPhysicsNode::setRotationalSpeed"); }
    void addPositionalSpeed(const Vec3&) override { unexpected("IPhysicsNode::addPositionalSpeed"); }
    void addRotationalSpeed(const Vec3&) override { unexpected("IPhysicsNode::addRotationalSpeed"); }
    void setPositionalAcceleration(const Vec3&) override { unexpected("IPhysicsNode::setPositionalAcceleration"); }
    void setRotationalAcceleration(const Vec3&) override { unexpected("IPhysicsNode::setRotationalAcceleration"); }
    void setPositionalFriction(const Vec3&) override { unexpected("IPhysicsNode::setPositionalFriction"); }
    void setRotationalFriction(const Vec3&) override { unexpected("IPhysicsNode::setRotationalFriction"); }
    void addSpeedAtAbsolutePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addSpeedAtAbsolutePosition"); }
    void addSpeedAtRelativePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addSpeedAtRelativePosition"); }
    void addAccelerationAtAbsolutePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addAccelerationAtAbsolutePosition"); }
    void addAccelerationAtRelativePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addAccelerationAtRelativePosition"); }
    void addFrictionAtAbsolutePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addFrictionAtAbsolutePosition"); }
    void addPositionalAccelerationToRootParent(const Vec3&) override { unexpected("IPhysicsNode::addPositionalAccelerationToRootParent"); }
    void addRotationalAccelerationAtAbsolutePosition(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addRotationalAccelerationAtAbsolutePosition"); }
    const Vec3& getTangentSpeed(const Vec3&) override { unexpected("IPhysicsNode::getTangentSpeed"); }
    const Vec3& getTangentAcceleration(const Vec3&) override { unexpected("IPhysicsNode::getTangentAcceleration"); }
    const Vec3& getRotationalAcceleration() const override { unexpected("IPhysicsNode::getRotationalAcceleration"); }
    const Vec3& getPositionalFriction() const override { unexpected("IPhysicsNode::getPositionalFriction"); }
    const Vec3& getRotationalFriction() const override { unexpected("IPhysicsNode::getRotationalFriction"); }
    void updatePhysics(float dt) override { unexpected("IPhysicsNode::updatePhysics"); }
    void setDrag(float) override { unexpected("IPhysicsNode::setDrag"); }
    float getDrag() const override { unexpected("IPhysicsNode::getDrag"); }
    void setDragOffset(const Vec3&) override { unexpected("IPhysicsNode::setDragOffset"); }
    const Vec3& getDragOffset() const override { unexpected("IPhysicsNode::getDragOffset"); }
    void setDragModifier(const Vec3&) override { unexpected("IPhysicsNode::setDragModifier"); }
    const Vec3& getDragModifier() const override { unexpected("IPhysicsNode::getDragModifier"); }
    void setMass(float) override { unexpected("IPhysicsNode::setMass"); }
    float getMass() const override { unexpected("IPhysicsNode::getMass"); }
    void setCenterOfMassOffset(const Vec3&) override { unexpected("IPhysicsNode::setCenterOfMassOffset"); }
    const Vec3& getCenterOfMassOffset() const override { unexpected("IPhysicsNode::getCenterOfMassOffset"); }
    void setCenterOfCollisionOffset(const Vec3&) override { unexpected("IPhysicsNode::setCenterOfCollisionOffset"); }
    const Vec3& getCenterOfCollisionOffset() const override { unexpected("IPhysicsNode::getCenterOfCollisionOffset"); }
    void setInertiaModifier(const Vec3&) override { unexpected("IPhysicsNode::setInertiaModifier"); }
    const Vec3& getInertiaModifier() const override { unexpected("IPhysicsNode::getInertiaModifier"); }
    void setGravityModifier(float) override { unexpected("IPhysicsNode::setGravityModifier"); }
    float getGravityModifier() const override { unexpected("IPhysicsNode::getGravityModifier"); }
    void setHasSeparatePhysicsUpdate(bool) override { unexpected("IPhysicsNode::setHasSeparatePhysicsUpdate"); }
    bool getHasSeparatePhysicsUpdate() const override { unexpected("IPhysicsNode::getHasSeparatePhysicsUpdate"); }
    void setUnderWater(float) override { unexpected("IPhysicsNode::setUnderWater"); }
    float getUnderWater() const override { unexpected("IPhysicsNode::getUnderWater"); }
    void setUnderwaterYComponent(float) override { unexpected("IPhysicsNode::setUnderwaterYComponent"); }
    bool getIsTreadingWater() const override { unexpected("IPhysicsNode::getIsTreadingWater"); }
    void reset(bool) override { unexpected("IPhysicsNode::reset"); }
    bool getIsSleeping() const override { unexpected("IPhysicsNode::getIsSleeping"); }
    void setIsSleeping() override { unexpected("IPhysicsNode::setIsSleeping"); }
    void setSleepiness(int) override { unexpected("IPhysicsNode::setSleepiness"); }
    int getSleepiness() const override { unexpected("IPhysicsNode::getSleepiness"); }
    void setIsSleepier() override { unexpected("IPhysicsNode::setIsSleepier"); }
    void setIsAwake() override { unexpected("IPhysicsNode::setIsAwake"); }
    bool getIsMobile() const override { unexpected("IPhysicsNode::getIsMobile"); }
    bool getIsPoint() const override { unexpected("IPhysicsNode::getIsPoint"); }
    void initLatencyCompensation() override { unexpected("IPhysicsNode::initLatencyCompensation"); }
    bool hasLatencyCompensation() override { unexpected("IPhysicsNode::hasLatencyCompensation"); }
    void updateLatencyState() override { unexpected("IPhysicsNode::updateLatencyState"); }
    const Mat4& getLatencyCompensatedTransformation(float, int*) override { unexpected("IPhysicsNode::getLatencyCompensatedTransformation"); }
    const Vec3& getAbsolutePositionalSpeed() override { unexpected("IPhysicsNode::getAbsolutePositionalSpeed"); }
    IPhysicsNode* getRootNode() override { unexpected("IPhysicsNode::getRootNode"); }
    void setHandleCollisionSpeed(float) override { unexpected("IPhysicsNode::setHandleCollisionSpeed"); }
    float getHandleCollisionSpeed() const override { unexpected("IPhysicsNode::getHandleCollisionSpeed"); }
    const Vec3& getInverseInertia() override { unexpected("IPhysicsNode::getInverseInertia"); }
    void addRelativeImpulse(const Vec3&, const Vec3&) override { unexpected("IPhysicsNode::addRelativeImpulse"); }
    void setFloaterMod(float) override { unexpected("IPhysicsNode::setFloaterMod"); }
    float getFloaterMod() const override { unexpected("IPhysicsNode::getFloaterMod"); }
    const Vec3& getLinearIntegrationVelocity() const override { unexpected("IPhysicsNode::getLinearIntegrationVelocity"); }
    void setLinearIntegrationVelocity(const Vec3&) override { unexpected("IPhysicsNode::setLinearIntegrationVelocity"); }
    const Vec3& getAngularIntegrationVelocity() const override { unexpected("IPhysicsNode::getAngularIntegrationVelocity"); }
    void setAngularIntegrationVelocity(const Vec3&) override { unexpected("IPhysicsNode::setAngularIntegrationVelocity"); }
    void setTreeCollisionRadius(float) override { unexpected("IPhysicsNode::setTreeCollisionRadius"); }
    float getTreeCollisionRadius() const override { unexpected("IPhysicsNode::getTreeCollisionRadius"); }
    void postFrameUpdate() override { unexpected("IPhysicsNode::postFrameUpdate"); }
    void setLocalLinearSpeed(const Vec3&) override { unexpected("IPhysicsNode::setLocalLinearSpeed"); }
    const Vec3& getLocalLinearSpeed() const override { unexpected("IPhysicsNode::getLocalLinearSpeed"); }
    void setLocalAngularSpeed(const Vec3&) override { unexpected("IPhysicsNode::setLocalAngularSpeed"); }
    const Vec3& getLocalAngularSpeed() const override { unexpected("IPhysicsNode::getLocalAngularSpeed"); }
    void setOtherCollisionLod(int) override { unexpected("IPhysicsNode::setOtherCollisionLod"); }
    int getOtherCollisionLod() const override { unexpected("IPhysicsNode::getOtherCollisionLod"); }
    bool getShouldGravityCompensateOnContact() const override { unexpected("IPhysicsNode::getShouldGravityCompensateOnContact"); }
    void setHasContact(bool) override { unexpected("IPhysicsNode::setHasContact"); }
    bool getHasContact() const override { unexpected("IPhysicsNode::getHasContact"); }
    void setIsInAir(bool) override { unexpected("IPhysicsNode::setIsInAir"); }
    bool getIsInAir() const override { unexpected("IPhysicsNode::getIsInAir"); }
    void setSleepTimeout(int) override { unexpected("IPhysicsNode::setSleepTimeout"); }
    void setIsPlatformPhysics(bool) override { unexpected("IPhysicsNode::setIsPlatformPhysics"); }
    bool getIsPlatformPhysics() const override { unexpected("IPhysicsNode::getIsPlatformPhysics"); }
};
} // namespace
static std::string recoilJoined()
{
    std::string r;
    for (const std::string& c : recoilCalls) r += c + ";";
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

    // ---- updateSoldierRecoilForce (0x5AE352..0x5AE463) ----
    {
        // Neither object is constructed: only the firearm's template pointer and the template's
        // weaponType are read.
        alignas(16) static unsigned char templateStorage[sizeof(GenericFireArmTemplate)];
        alignas(16) static unsigned char weaponStorage[sizeof(GenericFireArm)];
        GenericFireArmTemplate* weaponTemplate = reinterpret_cast<GenericFireArmTemplate*>(templateStorage);
        GenericFireArm* weapon = reinterpret_cast<GenericFireArm*>(weaponStorage);
        weaponTemplate->m_weaponType = 7;
        HapticWeaponAccess::setTemplate(reinterpret_cast<IObject*>(weapon), weaponTemplate);
        IPlayer* const local = reinterpret_cast<IPlayer*>(0x1000);
        IPlayer* const other = reinterpret_cast<IPlayer*>(0x2000);
        IObject* const asObject = reinterpret_cast<IObject*>(weapon);

        recoilCalls.clear();
        updateSoldierRecoilForce(0, local, 1.0f, 1.0f, asObject);
        updateSoldierRecoilForce(other, local, 1.0f, 1.0f, asObject);
        updateSoldierRecoilForce(0, 0, 1.0f, 1.0f, asObject);
        check(recoilCalls.empty(), "recoil: no occupant, another occupant, or no local player: nothing");
        updateSoldierRecoilForce(local, local, 0.0f, 0.0f, 0);
        updateSoldierRecoilForce(local, local, 0.0f, -0.0f, asObject);
        check(recoilJoined() == "recoil 0.000 0.000 -1;recoil 0.000 -0.000 -1;",
              "recoil: both zero: the weapon is not read (null is fine), even when there is one");
        recoilCalls.clear();
        updateSoldierRecoilForce(local, local, 0.5f, 0.0f, asObject);
        updateSoldierRecoilForce(local, local, 0.0f, -0.25f, asObject);
        weaponTemplate->m_weaponType = 12;
        updateSoldierRecoilForce(local, local, -1.0f, 2.0f, asObject);
        check(recoilJoined() == "recoil 0.500 0.000 7;recoil 0.000 -0.250 7;recoil -1.000 2.000 12;",
              "recoil: either value non-zero reads the firearm template's weaponType; values passed through");
    }

    // ---- updateSoldierCraftPhysics (0x5AECF5..0x5AED72) ----
    {
        ShakeVehicle vehicle;
        // A rotation with distinct terms: rows (0,1,0) (-1,0,0) (0,0,1), plus a scale on z.
        for (int i = 0; i < 16; ++i) vehicle.inverse.m[i] = 0.0f;
        vehicle.inverse.m[1] = 1.0f; vehicle.inverse.m[4] = -1.0f; vehicle.inverse.m[10] = 2.0f;
        vehicle.inverse.m[15] = 1.0f;   // translation (m[12..14]) is ignored
        vehicle.inverse.m[12] = 100.0f; vehicle.inverse.m[13] = 200.0f; vehicle.inverse.m[14] = 300.0f;
        Controller controller;
        controller.vehicle = &vehicle;
        IPlayer* const local = reinterpret_cast<IPlayer*>(0x1000);
        IPlayer* const other = reinterpret_cast<IPlayer*>(0x2000);
        Vec3 velocity;
        velocity.x = 3.0f; velocity.y = 5.0f; velocity.z = 7.0f;

        recoilCalls.clear();
        updateSoldierCraftPhysics(0, local, &controller, velocity, 0.02f);
        updateSoldierCraftPhysics(other, local, &controller, velocity, 0.02f);
        updateSoldierCraftPhysics(0, 0, &controller, velocity, 0.02f);
        check(recoilCalls.empty(), "craft physics: no occupant or another occupant: nothing, the vehicle is not read");
        updateSoldierCraftPhysics(local, local, &controller, velocity, 0.02f);
        // v * M: x = 3*m0 + 5*m4 + 7*m8 = -5; y = 3*m1 + 5*m5 + 7*m9 = 3; z = 3*m2 + 5*m6 + 7*m10 = 14
        check(recoilJoined() == "player.vehicle;vehicle.inverse;craft -5 3 14 0.020;",
              "craft physics: the velocity in the controller's vehicle frame (v * inverse), and dt");
    }

    // ---- updateVehicleCraftPhysics (0x5FAC8C..0x5FADAE): mode 3, the vehicle's inverse transform ----
    {
        ShakeVehicle vehicle;
        for (int i = 0; i < 16; ++i) vehicle.inverse.m[i] = 0.0f;
        vehicle.inverse.m[1] = 1.0f; vehicle.inverse.m[4] = -1.0f; vehicle.inverse.m[10] = 2.0f;
        vehicle.inverse.m[15] = 1.0f;
        vehicle.inverse.m[12] = 100.0f; vehicle.inverse.m[13] = 200.0f; vehicle.inverse.m[14] = 300.0f;
        HapticPhysics physics;
        physics.speed.x = 3.0f; physics.speed.y = 5.0f; physics.speed.z = 7.0f;
        recoilCalls.clear();
        updateVehicleCraftPhysics(&physics, &vehicle, 0, 0.04f, 2);
        check(recoilJoined() == "physics.acceleration;vehicle.inverse;physics.speed;vehicle craft -5 3 14 0.040 2;",
              "vehicle craft physics: acceleration read and dropped, speed * the vehicle's inverse transform, dt, category");
    }

    std::printf("soldier haptic profile %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
