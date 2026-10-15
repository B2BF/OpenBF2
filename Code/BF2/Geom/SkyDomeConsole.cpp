// Sky-dome console properties, available while the singleton exists.
// [RETIRE-WHEN: skydome-typed-owner] Retired: bindings use typed methods and field accessors.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"


#include "SkyDome.h"

#include "../../RendDX9Abi/HfeAbi.h"
#include <string>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

const char kSkyDomeConsoleTag[] = "geom.skyDome";

// g_SkyDome is published as an IBase* by ModuleGeom.cpp.  SkyDome derives from IBase first, so the
// interface subobject sits at offset zero and this downcast adjusts nothing - the same reason the
// renderer can hand the object straight to its three guarded calls.
inline SkyDome* skyDome()
{
    return static_cast<SkyDome*>(g_SkyDome);
}

// The console boundary copies the object's string for the host console (both are std::string).
inline std::string toConsole(const std::string& value)
{
    return std::string(value.c_str());
}

inline std::string toRetail(const std::string& value)
{
    return std::string(value.c_str());
}

// Small named adapters keep the generated bindings readable.  The retail slot each one used to
// reach by offset is noted beside it.
float getDomeRotation() { return skyDome()->getDomeRotation(); }                     // +0x28
void setDomeRotation(float value) { skyDome()->setDomeRotation(value); }             // +0x24

std::string getSkyTemplate() { return toConsole(skyDome()->getSkyTemplate()); }      // +0x30
void setSkyTemplate(const std::string& value)                                        // +0x2C
{
    skyDome()->setSkyTemplate(toRetail(value));
}

std::string getFlareTemplate() { return toConsole(skyDome()->getFlareTemplate()); }  // +0x88
void setFlareTemplate(const std::string& value)                                      // +0x84
{
    skyDome()->setFlareTemplate(toRetail(value));
}

// The getter is NOT const in retail and neither is it here: SkyDome::getFlareTexture stores 1
// into the flare's occlusion reference before returning the name, so reading skyDome.flareTexture
// from the console resets the flare to "fully visible".  Calling the real method preserves that.
std::string getFlareTexture() { return toConsole(skyDome()->getFlareTexture()); }    // +0x90
void setFlareTexture(const std::string& value)                                       // +0x8C
{
    skyDome()->setFlareTexture(toRetail(value));
}

// The three direct-field properties.  Retail has no vtable slot for any of them; the class exposes
// typed accessors so this file does not have to know the layout.
Vec3 getFlareDirection() { return skyDome()->flareDirection(); }                     // field +0x1A0
void setFlareDirection(const Vec3& value) { skyDome()->setFlareDirectionField(value); }

float getFlareFadeAdd() { return skyDome()->flareFadeAdd(); }                        // field +0x16C
void setFlareFadeAdd(float value) { skyDome()->setFlareFadeAddField(value); }

float getFlareFadeMul() { return skyDome()->flareFadeMul(); }                        // field +0x170
void setFlareFadeMul(float value) { skyDome()->setFlareFadeMulField(value); }

std::string getSkyTexture() { return toConsole(skyDome()->getSkyTexture()); }        // +0x38
void setSkyTexture(const std::string& value)                                         // +0x34
{
    skyDome()->setSkyTexture(toRetail(value));
}

bool getHasCloudLayer() { return skyDome()->getHasCloudLayer(); }                    // +0x40
void setHasCloudLayer(bool value) { skyDome()->setHasCloudLayer(value); }            // +0x3C

bool getHasCloudLayer2() { return skyDome()->getHasCloudLayer2(); }                  // +0x48
void setHasCloudLayer2(bool value) { skyDome()->setHasCloudLayer2(value); }          // +0x44

std::string getCloudTemplate() { return toConsole(skyDome()->getCloudTemplate()); }  // +0x50
void setCloudTemplate(const std::string& value)                                      // +0x4C
{
    skyDome()->setCloudTemplate(toRetail(value));
}

std::string getCloudTexture() { return toConsole(skyDome()->getCloudTexture()); }    // +0x58
void setCloudTexture(const std::string& value)                                       // +0x54
{
    skyDome()->setCloudTexture(toRetail(value));
}

std::string getCloudTexture2() { return toConsole(skyDome()->getCloudTexture2()); }  // +0x60
void setCloudTexture2(const std::string& value)                                      // +0x5C
{
    skyDome()->setCloudTexture2(toRetail(value));
}

Vec2 getScrollDirection() { return skyDome()->getScrollDirection(); }                // +0x68
void setScrollDirection(const Vec2& value) { skyDome()->setScrollDirection(value); } // +0x64

Vec2 getScrollDirection2() { return skyDome()->getScrollDirection2(); }              // +0x70
void setScrollDirection2(const Vec2& value) { skyDome()->setScrollDirection2(value); } // +0x6C

Vec2 getFadeCloudsDistances() { return skyDome()->getFadeCloudsDistances(); }        // +0x78
void setFadeCloudsDistances(const Vec2& value)                                       // +0x74
{
    skyDome()->setFadeCloudsDistances(value);
}

Vec2 getCloudLerpFactors() { return skyDome()->getCloudLerpFactors(); }              // +0x80
void setCloudLerpFactors(const Vec2& value) { skyDome()->setCloudLerpFactors(value); } // +0x7C

float getLightingBlend() { return skyDome()->getLightingBlend(); }                   // +0x9C
void setLightingBlend(float value) { skyDome()->setLightingBlend(value); }           // +0x98

Vec3 getLightingColor() { return skyDome()->getLightingColor(); }                    // +0xA4
void setLightingColor(const Vec3& value) { skyDome()->setLightingColor(value); }     // +0xA0

BF2_CONSOLE_OBJECT("skyDome", 2, 0)

BF2_CONSOLE_PROPERTY_FN(domeRotation, float, skyDome() != 0,
                        getDomeRotation(), setDomeRotation, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(skyTemplate, std::string, skyDome() != 0,
                        getSkyTemplate(), setSkyTemplate, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(flareTemplate, std::string, skyDome() != 0,
                        getFlareTemplate(), setFlareTemplate, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(flareTexture, std::string, skyDome() != 0,
                        getFlareTexture(), setFlareTexture, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(flareDirection, Vec3, skyDome() != 0,
                        getFlareDirection(), setFlareDirection, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(flareFadeAdd, float, skyDome() != 0,
                        getFlareFadeAdd(), setFlareFadeAdd, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(flareFadeMul, float, skyDome() != 0,
                        getFlareFadeMul(), setFlareFadeMul, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(skyTexture, std::string, skyDome() != 0,
                        getSkyTexture(), setSkyTexture, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(hasCloudLayer, bool, skyDome() != 0,
                        getHasCloudLayer(), setHasCloudLayer, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(hasCloudLayer2, bool, skyDome() != 0,
                        getHasCloudLayer2(), setHasCloudLayer2, kSkyDomeConsoleTag)
// Retail 0x10138420 ignores this setter's argument and copies the current skyTemplate string.
// Calling SkyDome::setCloudTemplate, rather than assigning the field here, preserves that shipped
// defect - the class reproduces it, so the binding does not have to know about it.
BF2_CONSOLE_PROPERTY_FN(cloudTemplate, std::string, skyDome() != 0,
                        getCloudTemplate(), setCloudTemplate, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cloudTexture, std::string, skyDome() != 0,
                        getCloudTexture(), setCloudTexture, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cloudTexture2, std::string, skyDome() != 0,
                        getCloudTexture2(), setCloudTexture2, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(scrollDirection, Vec2, skyDome() != 0,
                        getScrollDirection(), setScrollDirection, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(scrollDirection2, Vec2, skyDome() != 0,
                        getScrollDirection2(), setScrollDirection2, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(fadeCloudsDistances, Vec2, skyDome() != 0,
                        getFadeCloudsDistances(), setFadeCloudsDistances, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cloudLerpFactors, Vec2, skyDome() != 0,
                        getCloudLerpFactors(), setCloudLerpFactors, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(lightingBlend, float, skyDome() != 0,
                        getLightingBlend(), setLightingBlend, kSkyDomeConsoleTag)
BF2_CONSOLE_PROPERTY_FN(lightingColor, Vec3, skyDome() != 0,
                        getLightingColor(), setLightingColor, kSkyDomeConsoleTag)

} // anonymous namespace

bool registerSkyDomeConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kSkyDomeConsoleTag));
}

bool unregisterSkyDomeConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kSkyDomeConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
