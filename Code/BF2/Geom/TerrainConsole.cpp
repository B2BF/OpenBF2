
// Native terrain console bindings. Creation is always available; other commands
// require the terrain singleton.

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/PluginAbi.h"

#include "Terrain.h"
#include "MeshTemplate.h"                     // RaMaterialParameters - the terrain's material block

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Math/Vec3.h"

#include <cstring>
#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

// Stands in for the original's file-scope list (the static at 0x1024CCD8).
const char kTerrainConsoleTag[] = "geom.terrain";

// create - ctor 0x100B4F0B, executor the argument as a std::string into
// ModuleGeom's registration (0x100B118C).
void consoleCreateTerrain(const std::string& className)
{
    registerTerrainSingleton(std::string(className.c_str()));
}

// init - ctor 0x100B6868, executor slot +0x14.
void consoleInitTerrain()
{
    terrain()->init();
}

// load - ctor 0x100B68D5, executor slot +0x18 with the argument copied to a
// std::string.
void consoleLoadTerrain(const std::string& path)
{
    terrain()->load(std::string(path.c_str()));
}

// forceRenderStyle - ctor 0x100B502C, executor get slot +0x24, set slot +0x20.
int  getForceRenderStyle() { return terrain()->getRenderStyle(); }
void setForceRenderStyle(int style) { terrain()->setRenderStyle(style); }

// sunColor - ctor 0x100B6137, executor get slot +0x74 (the stored colour times
// four), set slot +0x70 with the three components.
Vec3 getSunColor()
{
    Vec3 v;
    terrain()->getSunColor(&v);
    return v;
}
void setSunColor(const Vec3& v) { terrain()->setSunColor(v.x, v.y, v.z); }

// GIColor - ctor 0x100B62AD, executor get slot +0x80 (twice the stored colour),
// set slot +0x7C.
Vec3 getGIColor()
{
    Vec3 v;
    terrain()->getGIColorTimes2(&v);
    return v;
}
void setGIColor(const Vec3& v) { terrain()->setGIColor(v.x, v.y, v.z); }

// waterSunIntensity - ctor 0x100B5A6B, executor the get reads the material block's
// +0x50; the set (0x100B5B72) writes the vec4 (1, intensity, 0.8, 1) over the block's +0x4C.
float getWaterSunIntensity()
{
    return static_cast<const RaMaterialParameters*>(terrain()->m_materialParameters)->pointColor.value[1];   // +0x50
}
void setWaterSunIntensity(float intensity)
{
    const float record[4] = { 1.0f, intensity, 0.8f, 1.0f };
    ::memcpy(static_cast<RaMaterialParameters*>(terrain()->m_materialParameters)->pointColor.value,
             record, sizeof(record));                                                               // +0x4C
}

#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
// drawPrimaryLightmaps / drawSurroundingLightmaps - the 2006 retail editor's bool properties (its renderer's binding
// constructors 0x101DD570 / 0x101DDBF0, which set and read its Terrain +0x261 / +0x263); 1.50 binds neither. The
// retail editor set them before tracing the terrain's lightmaps. They switch the two gates the terrain light sampler
// reads when it gathers its patches - +0x358 the primary patches, +0x359 the ring patches - which also gate the
// terrain's draw. [CHOSEN, not recovered] bound in the editor's flavour only; [INFERRED] the 1.50 fields they map to.
bool getDrawPrimaryLightmaps() { return terrain()->m_drawMain != 0; }
void setDrawPrimaryLightmaps(bool on) { terrain()->m_drawMain = on ? 1 : 0; }
bool getDrawSurroundingLightmaps() { return terrain()->m_flag359 != 0; }
void setDrawSurroundingLightmaps(bool on) { terrain()->m_flag359 = on ? 1 : 0; }
#endif

BF2_CONSOLE_OBJECT("terrain", 2, 0)

BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(create, consoleCreateTerrain, true, std::string, kTerrainConsoleTag)
BF2_CONSOLE_PROPERTY_FN(forceRenderStyle, int, terrain() != 0,
                        getForceRenderStyle(), setForceRenderStyle, kTerrainConsoleTag)
BF2_CONSOLE_PROPERTY_FN(waterSunIntensity, float, terrain() != 0,
                        getWaterSunIntensity(), setWaterSunIntensity, kTerrainConsoleTag)
BF2_CONSOLE_PROPERTY_FN(sunColor, Vec3, terrain() != 0, getSunColor(), setSunColor, kTerrainConsoleTag)
BF2_CONSOLE_PROPERTY_FN(GIColor, Vec3, terrain() != 0, getGIColor(), setGIColor, kTerrainConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(init, consoleInitTerrain, terrain() != 0, kTerrainConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(load, consoleLoadTerrain, terrain() != 0, std::string, kTerrainConsoleTag)
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
BF2_CONSOLE_PROPERTY_FN(drawPrimaryLightmaps, bool, terrain() != 0,
                        getDrawPrimaryLightmaps(), setDrawPrimaryLightmaps, kTerrainConsoleTag)
BF2_CONSOLE_PROPERTY_FN(drawSurroundingLightmaps, bool, terrain() != 0,
                        getDrawSurroundingLightmaps(), setDrawSurroundingLightmaps, kTerrainConsoleTag)
#endif

} // anonymous namespace

bool registerTerrainConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kTerrainConsoleTag));
}

bool unregisterTerrainConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kTerrainConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
