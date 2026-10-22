// The 2006 editor's `lightmapGeneration` console object, compiled only into the editor RendDX9
// flavour (BF2_IMAGE_EDITOR, native plug-in ABI). [CHOSEN, not recovered] 1.50 RendDX9 publishes
// no binding for the generator. This TU reproduces the binding names, argument types and static
// construction order of the 2006 editor's EditorRendDX9.dll (console class "LightmapGeneration",
// 2, 0; the rows are active while the generator exists), bound to the 1.50 generator's fields and
// its out-of-line accessors. Console lookup ignores case, so the stock `LightMapGeneration.*` lines
// reach these rows.
//
// Left out because 1.50 no longer has their fields: objectPointScale, terrainPointScale and
// doOvergrowth (2006 +0x154 / +0x150 / +0x158, beyond the 1.50 object), includeWaterDepth (2006
// +0x88, the font pointer in 1.50) and flush. getWaterSunIntensity is left out by the work order;
// the field it read (2006 +0x14C) survives as LightmapGeneration::m_waterSunIntensity.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/PluginAbi.h"

#if BF2_PLUGIN_ABI_NATIVE && defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR

#include "LightmapGeneration.h"
#include "../../Core/ConsoleObjectMacro.h"
#include "../../Core/ConsoleObjects.h"
#include "../../../RendDX9Abi/PluginEntry.h"

#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

// The 2006 numberOfGILights binding spells its type "uint" (OverGrowthConsole.cpp's typedef).
typedef unsigned int uint;

const char kLightmapGenerationConsoleTag[] = "geom.lightmapGeneration";

inline LightmapGeneration* generator()
{
    return g_LightmapGeneration;
}

inline bool generatorExists()
{
    return g_LightmapGeneration != 0;
}

// The 2006 light.* rows edit the generator's last light (the same lookup as LightmapGeneration.cpp's
// lastLight) and are active only while there is one.
inline LightInfo* currentLight()
{
    LightmapGeneration* const owner = g_LightmapGeneration;
    if (!owner || owner->m_lights.size() == 0)
        return 0;
    return &owner->m_lights.back();
}

inline bool currentLightExists()
{
    return currentLight() != 0;
}

void consoleInit()
{
    createLightmapGenerationSingleton();
}

bool consoleTraceTerrain()
{
    return generator()->traceTerrain(-1);                                  // ILightmapGeneration +0x1C
}

bool consoleTraceAllObjects()
{
    return generator()->traceAllObjects();
}

bool consoleTraceActiveObject()
{
    return generator()->traceActiveObject();
}

bool consoleTraceActiveObjectProgress(int index, int count)
{
    return generator()->traceActiveObjectProgress(index, count);
}

void consoleCreateLight()
{
    generator()->pushDefaultLight();
}

void setRenderTerrain(bool value) { generator()->m_renderTerrain = value; }
void setResolutionModifier(float value) { generator()->m_resolutionModifier = value; }
void setLogPath(const std::string& value) { generator()->m_logPath = value.c_str(); }
std::string getLogPath() { return std::string(generator()->m_logPath.c_str()); }

BF2_CONSOLE_OBJECT("LightmapGeneration", 2, 0)

BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(init, consoleInit, true, kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY(onlyNearest, bool, generator(), setOnlyNearest, getOnlyNearest,
                     kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(renderTerrain, bool, generatorExists(), generator()->m_renderTerrain != 0,
                        setRenderTerrain, kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY(shadowsNearCamera, float, generator(), setShadowsNearCamera,
                     getShadowsNearCamera, kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(resolutionModifier, float, generatorExists(),
                        generator()->m_resolutionModifier, setResolutionModifier,
                        kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(skyColor, Vec3, generatorExists(), generator()->m_skyColor,
                        generator()->setSkyColor, kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(ambientColor, Vec3, generatorExists(), generator()->m_ambientColor,
                        generator()->setAmbientColor, kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY(blockSize, int, generator(), setBlockSize, getBlockSize,
                     kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY(sampleOffset, float, generator(), setSampleOffset, getSampleOffset,
                     kLightmapGenerationConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(traceTerrain, consoleTraceTerrain, bool, generatorExists(),
                                kLightmapGenerationConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(traceAllObjects, consoleTraceAllObjects, bool, generatorExists(),
                                kLightmapGenerationConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(traceActiveObject, consoleTraceActiveObject, bool,
                                generatorExists(), kLightmapGenerationConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(traceActiveObjectProgress, consoleTraceActiveObjectProgress, bool,
                                generatorExists(), int, int, kLightmapGenerationConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(light.create, consoleCreateLight, generatorExists(),
                                   kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(noPhongAngle, bool, generator(), m_noPhongAngle, kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(generationMethod, int, generator(), m_generationMethod,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(phongAngleMode, int, generator(), m_phongAngleMode, kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(globalSoftness, float, generator(), m_globalSoftness,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(directlyLightedThreshold, float, generator(), m_directlyLightedThreshold,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(cameraNearPlane, float, generator(), m_cameraNearPlane,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(GILightsBaseDir, Vec3, generator(), m_giLightsBaseDir,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(GILightsIntensity, float, generator(), m_giLightsIntensity,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(GILightsShadowIntensity, float, generator(), m_giLightsShadowIntensity,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(GILightsFov, float, generator(), m_giLightsFov, kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(GILightsViewDistance, float, generator(), m_giLightsViewDistance,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(GILightColor, Vec3, generatorExists(), generator()->m_giLightColor,
                        generator()->setGILightColor, kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(numberOfGILights, uint, generator(), m_numberOfGILights,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(useGITopLight, bool, generator(), m_useGITopLight, kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(GITopLightIntensity, float, generator(), m_giTopLightIntensity,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(sunLightIntensity, float, generator(), m_sunLightIntensity,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(sunLightShadowIntensity, float, generator(), m_sunLightShadowIntensity,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(sunLightFov, float, generator(), m_sunLightFov, kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(enableIntensityLightmaps, bool, generator(), m_enableIntensityLightmaps,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(enableBackfaceCulling, bool, generator(), m_enableBackfaceCulling,
                  kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(lightmapFormat, int, generator(), m_lightmapFormat, kLightmapGenerationConsoleTag)
BF2_CONSOLE_FIELD(skyWhite, float, generator(), m_skyWhite, kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(useLog, std::string, generatorExists(), getLogPath(), setLogPath,
                        kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(light.direction, Vec3, currentLightExists(),
                        currentLight()->getDirection(), currentLight()->setDirection,
                        kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(light.intensity, float, currentLightExists(),
                        currentLight()->getIntensity(), currentLight()->setIntensity,
                        kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(light.shadowIntensity, float, currentLightExists(),
                        currentLight()->getShadowIntensity(), currentLight()->setShadowIntensity,
                        kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(light.selfShadowing, bool, currentLightExists(),
                        currentLight()->getSelfShadowing(), currentLight()->setSelfShadowing,
                        kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(light.objectShadows, bool, currentLightExists(),
                        currentLight()->getObjectShadows(), currentLight()->setObjectShadows,
                        kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(light.fov, float, currentLightExists(), currentLight()->getFov(),
                        currentLight()->setFov, kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(light.viewDistance, float, currentLightExists(),
                        currentLight()->getViewDistance(), currentLight()->setViewDistance,
                        kLightmapGenerationConsoleTag)
BF2_CONSOLE_PROPERTY_FN(light.softness, float, currentLightExists(), currentLight()->getSoftness(),
                        currentLight()->setSoftness, kLightmapGenerationConsoleTag)

} // anonymous namespace

// ModuleGeom::init / close (editor flavour only).
bool registerLightmapGenerationConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kLightmapGenerationConsoleTag));
}

bool unregisterLightmapGenerationConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kLightmapGenerationConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_PLUGIN_ABI_NATIVE && BF2_IMAGE_EDITOR
#endif // BF2_IMAGE_RENDDX9
