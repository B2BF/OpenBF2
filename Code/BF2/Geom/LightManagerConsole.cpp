// Path:     [INFERRED] companion to the attested Geom/ModuleGeom.cpp.
// Contents: LightManager console publication recovered from RendDX9.dll.

// The shipped TU installs SetConsoleClassDefaults("lightManager", 2, 0), then constructs its
// generated bindings in the order retained below.  ModuleGeom::init registers this TU's list with
// the host console registry; ModuleGeom::close unregisters it.  Publication is native-only because
// ConsoleObject and the registry both carry host-toolchain std::string/container ABI.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"


#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "XpLightManager.h"

namespace dice {
namespace hfe {
namespace geom {

extern LightManager* g_LightManager;

namespace {

const char kLightManagerConsoleTag[] = "geom.lightManager";
const char kShadowManagerConsoleTag[] = "geom.shadowManager";

inline LightManager* lightManager()
{
    return g_LightManager;
}

void consoleInitializeLightManager()
{
    initializeLightManager(lightManager());
}

BF2_CONSOLE_OBJECT("lightManager", 2, 0)

// PE constructor 0x100BC86C / executor 0x100BC8C1.  The executor calls the direct initializer,
// not a virtual slot, and is inactive while the LightManager singleton is absent.
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(init, consoleInitializeLightManager,
                                   lightManager() != 0, kLightManagerConsoleTag)

BF2_CONSOLE_PROPERTY(skyColor, Vec3, lightManager(),
                     setSkyColor, getSkyColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(ambientColor, Vec3, lightManager(),
                     setAmbientColor, getAmbientColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(treeAmbientColor, Vec3, lightManager(),
                     setTreeAmbientColor, getTreeAmbientColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(treeSunColor, Vec3, lightManager(),
                     setTreeSunColor, getTreeSunColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(treeSkyColor, Vec3, lightManager(),
                     setTreeSkyColor, getTreeSkyColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(staticSunColor, Vec3, lightManager(),
                     setStaticSunColor, getStaticSunColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(staticSkyColor, Vec3, lightManager(),
                     setStaticSkyColor, getStaticSkyColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(staticSpecularColor, Vec3, lightManager(),
                     setStaticSpecularColor, getStaticSpecularColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(singlePointColor, Vec3, lightManager(),
                     setSinglePointColor, getSinglePointColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(dynamicPointColor, Vec3, lightManager(),
                     setDynamicPointColor, getDynamicPointColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(effectSunColor, Vec3, lightManager(),
                     setEffectSunColor, getEffectSunColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(effectShadowColor, Vec3, lightManager(),
                     setEffectShadowColor, getEffectShadowColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(defaultEffectLightAffectionFactor, float, lightManager(),
                     setDefaultEffectLightAffectionFactor,
                     getDefaultEffectLightAffectionFactor, kLightManagerConsoleTag)

// Ground-hemi string property, available while the light manager exists.
// High/low color and static-blending bindings remain unavailable pending typed wrappers.
void setGroundHemiName(const std::string& name)
{
    lightManager()->setGroundHemi(name);
}

std::string getGroundHemiName()
{
    const std::string& name = lightManager()->getGroundHemi();
    return std::string(name.c_str(), name.size());
}

BF2_CONSOLE_PROPERTY_FN(groundHemi, std::string, lightManager() != 0,
                        getGroundHemiName(), setGroundHemiName, kLightManagerConsoleTag)

// Binding 29 is the connected stock path: PE 0x1015BD70/0x1015BD80 forward it to the published
// HemiMapManager without a null guard.
BF2_CONSOLE_PROPERTY(hemiLerpBias, float, lightManager(),
                     setHemiLerpBias, getHemiLerpBias, kLightManagerConsoleTag)

// Binding 30 follows hemiLerpBias in the shipped static-construction order.
BF2_CONSOLE_PROPERTY(lightCullDistance, float, lightManager(),
                     setLightCullDistance, getLightCullDistance, kLightManagerConsoleTag)

// Bindings 31..34 (per-geometry maximum lights) remain unpublished. Binding 35 follows them:
// PE constructor 0x100BF515, executor 0x100BF5D3, .CRT entry 0x102366E4. Reads call +0x80;
// writes pass the parsed float to +0x7C. The next ambient-alpha range binding remains absent.
BF2_CONSOLE_PROPERTY(skinnedMeshShaderLodDistance, float, lightManager(),
                     setSkinnedMeshShaderLodDistance, getSkinnedMeshShaderLodDistance,
                     kLightManagerConsoleTag)

BF2_CONSOLE_PROPERTY(sunDirection, Vec3, lightManager(),
                     setSunDirection, getSunDirection, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(sunColor, Vec3, lightManager(),
                     setSunColor, getSunColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(sunSpecColor, Vec3, lightManager(),
                     setSunSpecColor, getSunSpecColor, kLightManagerConsoleTag)
BF2_CONSOLE_PROPERTY(enableSun, bool, lightManager(),
                     setSunEnabled, getSunEnabled, kLightManagerConsoleTag)

// Bindings 40 and 41 remain unpublished.  Binding 42 is the last binding before weatherManager's
// defaults.  The public console spelling
// is instruction-proven as dynamicPointScale even though the evidence-scoped interface wrapper
// describes the underlying value as a dynamic-light range scale.
BF2_CONSOLE_PROPERTY(dynamicPointScale, float, lightManager(),
                     setDynamicLightRangeScale, getDynamicLightRangeScale,
                     kLightManagerConsoleTag)

// A separate shipped console name for the same LightManager singleton. PE 101D1AEA installs
// SetConsoleClassDefaults("shadowManager", 2, 0), after weatherManager and before HemiMapManager.
// Keep the recovered subset in .CRT order: 102367E4, 102367F0, 102367F4. Other shadowManager
// commands remain unpublished until their complete accessor contracts are recovered.
BF2_CONSOLE_OBJECT("shadowManager", 2, 0)

// PE constructor 100C3A4C / parser 100C3ABD / executor int, +28C setter/+290 getter.
BF2_CONSOLE_PROPERTY(shadowFromSkinnedMeshLod, int, lightManager(),
                     setSkinnedMeshShadowLod, getSkinnedMeshShadowLod, kShadowManagerConsoleTag)
// PE constructor 100C3D6D / parser 100C3DDE / executor float, +2A4/+2A8.
BF2_CONSOLE_PROPERTY(shadowSunScalePower, float, lightManager(),
                     setShadowSunScalePower, getShadowSunScalePower, kShadowManagerConsoleTag)
// PE constructor 100C3E7A / parser 100C3EFA / executor Vec2, +2BC/+2C0.
BF2_CONSOLE_PROPERTY(shadowSunHeightOverFade, Vec2, lightManager(),
                     setShadowSunHeightOverFade, getShadowSunHeightOverFade, kShadowManagerConsoleTag)

} // anonymous namespace

bool registerLightManagerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kLightManagerConsoleTag));
}

bool unregisterLightManagerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kLightManagerConsoleTag));
}

bool registerShadowManagerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kShadowManagerConsoleTag));
}

bool unregisterShadowManagerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kShadowManagerConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
