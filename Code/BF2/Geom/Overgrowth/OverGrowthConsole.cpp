// Native console bindings for Overgrowth and OvergrowthType (RW-267).
// [INFERRED] companion file placement; [INFERRED] original grouping with OverGrowth.cpp.
// Commands require the level's OverGrowth owner. Preserve the duplicate viewDistance registration:
// the multimap resolves its first active entry. Compatibility builds publish no console rows.
#ifdef BF2_IMAGE_RENDDX9
#include "../../../RendDX9Abi/PluginAbi.h"

#include "OverGrowth.h"

#include "../../../RendDX9Abi/PluginEntry.h"
#include <string>
#include "../../Core/ConsoleObjectMacro.h"
#include "../../Core/ConsoleObjects.h"
#include "../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace dx9rend {
extern void* g_OverGrowth;   // dword_10258F38 - BF2Render.cpp
}
namespace geom {

namespace {

// Stands in for the original's file-scope list (the image's one ModuleGeom list, unk_102543C4).
const char kOverGrowthConsoleTag[] = "geom.overgrowth";

// addMaterial's metadata spells its second argument "uint" (ctor 0x100C566E stores it at +0x54)
// and its setArgFromString (0x100C5751) extracts it with istream >> unsigned int. A file-local
// typedef reproduces both halves, as GameLogicConsole.cpp does for "KitType" / "U32".
typedef unsigned int uint;

inline OverGrowth* overGrowth()
{
    return static_cast<OverGrowth*>(dx9rend::g_OverGrowth);
}

// the shared isObjectActive of all 34 rows.
inline bool overGrowthActive()
{
    return dx9rend::g_OverGrowth != 0;
}

// The console boundary copies the owner's string for the host console (both are std::string).
inline std::string toConsole(const std::string& value)
{
    return ::renddx9abi::toHostString(value);
}

inline std::string toRetail(const std::string& value)
{
    return ::renddx9abi::fromHostString(value);
}

// Overgrowth
// Each adapter names its row's ctor / executor and the owner call the executor makes.

// path - ctor 0x100C538A, exec get = copy of 0x100C92D0's `&this->+0x88`;
// set = 0x100C92C0 (operator= into +0x88) with the argument copied to a local first.
std::string consoleGetPath() { return toConsole(overGrowth()->getPath()); }
void consoleSetPath(const std::string& path) { overGrowth()->setPath(toRetail(path)); }

// preLoad - ctor 0x100C54E6, exec argc 0 -> 0x100C9CD0.
void consolePreLoad() { overGrowth()->preLoad(); }

// postLoad - ctor 0x100C5553, exec argc 0 -> 0x100CC500 then 0x100CD020 on the same
// cached owner pointer (`mov esi, ecx`).
void consolePostLoad()
{
    OverGrowth* const owner = overGrowth();
    owner->preLoadResources();
    owner->postLoadResources();
}

// getMaterialList - ctor 0x100C55CB, exec argc 0 -> 0x100C9E40.
std::string consoleGetMaterialList() { return toConsole(overGrowth()->getMaterialList()); }

// addMaterial - ctor 0x100C566E, exec argc 2 -> bool 0x100CDEE0(name copy, uint).
bool consoleAddMaterial(const std::string& name, uint id)
{
    return overGrowth()->addMaterial(toRetail(name), id);
}

// removeActiveMaterial - ctor 0x100C57FE, exec argc 0 -> bool 0x100CDE00.
bool consoleRemoveActiveMaterial() { return overGrowth()->removeActiveMaterial(); }

// setActiveMaterial - ctor 0x100C5879, exec argc 1 -> 0x100C9EE0(name copy).
void consoleSetActiveMaterial(const std::string& name) { overGrowth()->setActiveMaterial(toRetail(name)); }

// getActiveMaterialName - ctor 0x100C599E, exec argc 0 -> 0x100C8A00.
std::string consoleGetActiveMaterialName() { return toConsole(overGrowth()->getActiveMaterialName()); }

// getTypeList - ctor 0x100C5A41, exec argc 0 -> 0x100C9F20.
std::string consoleGetTypeList() { return toConsole(overGrowth()->getTypeList()); }

// addType - ctor 0x100C5AE4, exec argc 1 -> bool 0x100CC6A0(name copy).
bool consoleAddType(const std::string& name) { return overGrowth()->addType(toRetail(name)); }

// removeActiveType - ctor 0x100C5C17, exec argc 0 -> bool 0x100CBB30.
bool consoleRemoveActiveType() { return overGrowth()->removeActiveType(); }

// setActiveType - ctor 0x100C5C92, exec argc 1 -> 0x100CA050(name copy).
void consoleSetActiveType(const std::string& name) { overGrowth()->setActiveType(toRetail(name)); }

// save - ctor 0x100C5DB7, exec argc 0 -> 0x100CD7E0 (OverGrowthLoad.cpp).
void consoleSave() { overGrowth()->save(); }

// viewDistance (both rows) - exec get = virtual +0x10 (100D1920, the +0x24 float),
// set = virtual +0x0C (100CBA30) with the parsed float.
float consoleGetViewDistance() { return overGrowth()->getViewDistance(); }
void consoleSetViewDistance(float distance) { overGrowth()->setViewDistance(distance); }

// viewDistanceScale - ctor 0x100C5FE9, exec the +0x28 float read/written directly.
float consoleGetViewDistanceScale() { return overGrowth()->getViewDistanceScale(); }
void consoleSetViewDistanceScale(float scale) { overGrowth()->setViewDistanceScale(scale); }

// exportToTerrainTexture - ctor 0x100C60E7, exec argc 0 -> 0x100C8F40.
void consoleExportToTerrainTexture() { overGrowth()->exportToTerrainTexture(); }

// paintMaterial - ctor 0x100C6154 (metadata "float, float, float, int"), exec argc 4 ->
// (f, f, f, the int as a byte). The member takes an 8-bit value (OverGrowth.h): 1.50's
// executor pushes the parsed dword and the body reads its low byte; the 2006 editor's executor
// narrows it first (`mov dl, byte ptr`), as this cast does.
void consolePaintMaterial(float x, float z, float radius, int material)
{
    overGrowth()->paintMaterial(x, z, radius, static_cast<unsigned char>(material));
}

// paintShadowmap - ctor 0x100C6354, exec argc 4 -> 0x100C9670, the same shape.
void consolePaintShadowmap(float x, float z, float radius, int value)
{
    overGrowth()->paintShadowmap(x, z, radius, static_cast<unsigned char>(value));
}

// getGeometryList - ctor 0x100C6554, exec argc 0 -> 0x100CDC20.
std::string consoleGetGeometryList() { return toConsole(overGrowth()->getGeometryList()); }

// generateAll - ctor 0x100C65F7, exec argc 0 -> virtual +0x2C (reload, 100CDDC0).
void consoleGenerateAll() { overGrowth()->reload(); }

// getLodDistance - ctor 0x100C6664, exec argc 0 -> float 0x100C92E0.
float consoleGetLodDistance() { return overGrowth()->getLodDistance(); }

// generateAtlas - ctor 0x100C66DF, exec argc 0 -> 0x100CCD20 (OverGrowthLoad.cpp).
void consoleGenerateAtlas() { overGrowth()->generateAtlas(); }

// closeDistPercentage / closeDistFade / lodTimer - ctors 0x100C674C / 0x100C6850 / 0x100C6954,
// execs the +0xC8 / +0xCC / +0xF8 floats, directly.
float consoleGetCloseDistPercentage() { return overGrowth()->getCloseDistPercentage(); }
void consoleSetCloseDistPercentage(float value) { overGrowth()->setCloseDistPercentage(value); }
float consoleGetCloseDistFade() { return overGrowth()->getCloseDistFade(); }
void consoleSetCloseDistFade(float value) { overGrowth()->setCloseDistFade(value); }
float consoleGetLodTimer() { return overGrowth()->getLodTimer(); }
void consoleSetLodTimer(float value) { overGrowth()->setLodTimer(value); }

// editorEnable - ctor 0x100C6A58, exec 0x100C6B16. typed "float" IN THE metadata over a bool
// member: get = `movzx eax, al; fild` of 0x100C8C50's byte (so 0 or 1); set = 0x100C8C40 with
// `value != 0.0f` (`fucompp` against flt_101DDE64 = 0.0, `test ah, 44h; jnp` - a NaN counts as
// true). Content writes `Overgrowth.editorEnable 1` only in Init.con's BF2Editor branch.
float consoleGetEditorEnable() { return static_cast<float>(overGrowth()->getEditorEnable()); }
void consoleSetEditorEnable(float value) { overGrowth()->setEditorEnable(value != 0.0f); }

// OvergrowthType - every row acts on the owner's active type (+0xEC)

// geometry - ctor 0x100C6B84, exec get 0x100C8A70 (the type's +0x1C, or ""),
// set 0x100C8A40 (argument copied to a local first).
std::string consoleGetTypeGeometry() { return toConsole(overGrowth()->getTypeGeometry()); }
void consoleSetTypeGeometry(const std::string& geometry) { overGrowth()->setTypeGeometry(toRetail(geometry)); }

// density - ctor 0x100C6CED, exec get 0x100C8AE0, set 0x100C8AB0 (+0x3C).
float consoleGetTypeDensity() { return overGrowth()->getTypeDensity(); }
void consoleSetTypeDensity(float value) { overGrowth()->setTypeDensity(value); }

// normalScale - ctor 0x100C6DF4, exec get 0x100C8B30, set 0x100C8B00 (+0x54).
float consoleGetTypeNormalScale() { return overGrowth()->getTypeNormalScale(); }
void consoleSetTypeNormalScale(float value) { overGrowth()->setTypeNormalScale(value); }

// rotationScale - ctor 0x100C6EFB, exec get 0x100C8B80, set 0x100C8B50 (+0x40).
float consoleGetTypeRotationScale() { return overGrowth()->getTypeRotationScale(); }
void consoleSetTypeRotationScale(float value) { overGrowth()->setTypeRotationScale(value); }

// minRadiusToSame - ctor 0x100C7002, exec get 0x100C8BD0, set 0x100C8BA0 (+0x4C).
float consoleGetTypeMinRadiusToSame() { return overGrowth()->getTypeMinRadiusToSame(); }
void consoleSetTypeMinRadiusToSame(float value) { overGrowth()->setTypeMinRadiusToSame(value); }

// minRadiusToOthers - ctor 0x100C7109, exec get 0x100C8C20, set 0x100C8BF0 (+0x50).
float consoleGetTypeMinRadiusToOthers() { return overGrowth()->getTypeMinRadiusToOthers(); }
void consoleSetTypeMinRadiusToOthers(float value) { overGrowth()->setTypeMinRadiusToOthers(value); }

// randomScale - ctor 0x100C7210, exec get 0x100C9640 (by value), set 0x100C9610 with
// the Vec2 pushed by value (+0x44/+0x48). No stock content writes it.
Vec2 consoleGetTypeRandomScale() { return overGrowth()->getTypeRandomScale(); }
void consoleSetTypeRandomScale(const Vec2& value) { overGrowth()->setTypeRandomScale(value); }


// THE rows, in the shipped .CRT$XCU order. Column notes: the .CRT slot, then the row's vtable.

BF2_CONSOLE_OBJECT("Overgrowth", 2, 0)

BF2_CONSOLE_PROPERTY_FN(path, std::string, overGrowthActive(),
                        consoleGetPath(), consoleSetPath, kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(preLoad, consolePreLoad, overGrowthActive(),
                                   kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(postLoad, consolePostLoad, overGrowthActive(),
                                   kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getMaterialList, consoleGetMaterialList, std::string,
                                overGrowthActive(), kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(addMaterial, consoleAddMaterial, bool, overGrowthActive(),
                                std::string, uint, kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(removeActiveMaterial, consoleRemoveActiveMaterial, bool,
                                overGrowthActive(), kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setActiveMaterial, consoleSetActiveMaterial,
                                   overGrowthActive(), std::string, kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getActiveMaterialName, consoleGetActiveMaterialName,
                                std::string, overGrowthActive(), kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getTypeList, consoleGetTypeList, std::string,
                                overGrowthActive(), kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addType, consoleAddType, bool, overGrowthActive(),
                                std::string, kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(removeActiveType, consoleRemoveActiveType, bool,
                                overGrowthActive(), kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setActiveType, consoleSetActiveType,
                                   overGrowthActive(), std::string, kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(save, consoleSave, overGrowthActive(),
                                   kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(viewDistance, float, overGrowthActive(),
                        consoleGetViewDistance(), consoleSetViewDistance, kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(viewDistance, float, overGrowthActive(),
                        consoleGetViewDistance(), consoleSetViewDistance, kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(viewDistanceScale, float, overGrowthActive(),
                        consoleGetViewDistanceScale(), consoleSetViewDistanceScale,
                        kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(exportToTerrainTexture, consoleExportToTerrainTexture,
                                   overGrowthActive(), kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(paintMaterial, consolePaintMaterial, overGrowthActive(),
                                   float, float, float, int, kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(paintShadowmap, consolePaintShadowmap,
                                   overGrowthActive(), float, float, float, int,
                                   kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getGeometryList, consoleGetGeometryList, std::string,
                                overGrowthActive(), kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(generateAll, consoleGenerateAll, overGrowthActive(),
                                   kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getLodDistance, consoleGetLodDistance, float,
                                overGrowthActive(), kOverGrowthConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(generateAtlas, consoleGenerateAtlas, overGrowthActive(),
                                   kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(closeDistPercentage, float, overGrowthActive(),
                        consoleGetCloseDistPercentage(), consoleSetCloseDistPercentage,
                        kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(closeDistFade, float, overGrowthActive(),
                        consoleGetCloseDistFade(), consoleSetCloseDistFade, kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(lodTimer, float, overGrowthActive(),
                        consoleGetLodTimer(), consoleSetLodTimer, kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(editorEnable, float, overGrowthActive(),
                        consoleGetEditorEnable(), consoleSetEditorEnable, kOverGrowthConsoleTag)

BF2_CONSOLE_OBJECT("OvergrowthType", 2, 0)

BF2_CONSOLE_PROPERTY_FN(geometry, std::string, overGrowthActive(),
                        consoleGetTypeGeometry(), consoleSetTypeGeometry, kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(density, float, overGrowthActive(),
                        consoleGetTypeDensity(), consoleSetTypeDensity, kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(normalScale, float, overGrowthActive(),
                        consoleGetTypeNormalScale(), consoleSetTypeNormalScale,
                        kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(rotationScale, float, overGrowthActive(),
                        consoleGetTypeRotationScale(), consoleSetTypeRotationScale,
                        kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(minRadiusToSame, float, overGrowthActive(),
                        consoleGetTypeMinRadiusToSame(), consoleSetTypeMinRadiusToSame,
                        kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(minRadiusToOthers, float, overGrowthActive(),
                        consoleGetTypeMinRadiusToOthers(), consoleSetTypeMinRadiusToOthers,
                        kOverGrowthConsoleTag)
BF2_CONSOLE_PROPERTY_FN(randomScale, Vec2, overGrowthActive(),
                        consoleGetTypeRandomScale(), consoleSetTypeRandomScale,
                        kOverGrowthConsoleTag)

} // anonymous namespace

// ModuleGeom::init / close pass this list to the host registry.
// One list for both console objects, as the retail rows share one list.
bool registerOverGrowthConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kOverGrowthConsoleTag));
}

bool unregisterOverGrowthConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kOverGrowthConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
