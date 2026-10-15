// Native undergrowth console bindings, available while the system singleton exists.
// These ConsoleObject instances must not cross the VC7.1 host boundary.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"


#include "UndergrowthSystem.h"

#include <string>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"

#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

const char kUndergrowthConsoleTag[] = "geom.undergrowth";

// g_UndergrowthSystem is published as an IBase*; UndergrowthSystem derives from IBase alone, so the
// downcast adjusts nothing.
inline UndergrowthSystem* undergrowth()
{
    return static_cast<UndergrowthSystem*>(g_UndergrowthSystem);
}

// the row copies its argument and calls +0x10 with the copy. The console speaks this
// toolchain's std::string; the object keeps the 28-byte VC7.1 one.
void undergrowthLoad(const std::string& levelPath)
{
    undergrowth()->load(std::string(levelPath.c_str()));
}

void undergrowthReload() { undergrowth()->reload(); }                                // +0x18

void setEnabled(bool value) { undergrowth()->setEnabled(value); }                    // +0x30
void setDynamicShadowsEnable(bool value) { undergrowth()->setDynamicShadowsEnable(value); } // +0x38
void setSimpleShaderEnable(bool value) { undergrowth()->setSimpleShaderEnable(value); }     // +0x40
void setZPassPatchCount(unsigned int value) { undergrowth()->setZPassPatchCount(value); }   // +0x48
void setTest(unsigned int value) { undergrowth()->setTestValue(value); }             // field +0x08
void setTest2(float value) { undergrowth()->setTest2Value(value); }                  // field +0x0C
void setViewDistanceScale(float value) { undergrowth()->setViewDistanceScale(value); } // +0x68

// sub_1000ED40 as the row inlines the call: fcmovnb against the high bound, then fcmovb against the
// low one - an unordered (NaN) value takes the LOW bound. See UndergrowthSystem.cpp's clampRange.
void setViewDistanceFade(float value)                                                // field +0x58C
{
    float v = value;
    if (v >= 1.0f)
        v = 1.0f;
    if (!(v >= 0.0f))
        v = 0.0f;
    undergrowth()->setViewDistanceFadeField(v);
}

BF2_CONSOLE_OBJECT("undergrowth", 2, 0)

BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(load, undergrowthLoad, undergrowth() != 0, std::string,
                                   kUndergrowthConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(reload, undergrowthReload, undergrowth() != 0,
                                   kUndergrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(enabled, bool, undergrowth() != 0,
                        undergrowth()->getEnabled(), setEnabled, kUndergrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dynamicShadowsEnable, bool, undergrowth() != 0,
                        undergrowth()->getDynamicShadowsEnable(), setDynamicShadowsEnable,
                        kUndergrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(simpleShaderEnable, bool, undergrowth() != 0,
                        undergrowth()->getSimpleShaderEnable(), setSimpleShaderEnable,
                        kUndergrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(zPassPatchCount, unsigned int, undergrowth() != 0,
                        undergrowth()->getZPassPatchCount(), setZPassPatchCount,
                        kUndergrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(test, unsigned int, undergrowth() != 0,
                        undergrowth()->testValue(), setTest, kUndergrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(test2, float, undergrowth() != 0,
                        undergrowth()->test2Value(), setTest2, kUndergrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(viewDistanceScale, float, undergrowth() != 0,
                        undergrowth()->getViewDistanceScale(), setViewDistanceScale,
                        kUndergrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(viewDistanceFade, float, undergrowth() != 0,
                        undergrowth()->viewDistanceFade(), setViewDistanceFade,
                        kUndergrowthConsoleTag)

} // anonymous namespace

bool registerUndergrowthConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kUndergrowthConsoleTag));
}

bool unregisterUndergrowthConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kUndergrowthConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
