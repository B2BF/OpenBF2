
// Native Object.geometry bindings: GenerateLightmapForThisInstance (the active geometry's
// lightmap-generation flag) and loadMesh for compiled roads.
// Compatibility builds cannot share host ConsoleObject layouts and omit these bindings.

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/PluginAbi.h"

#include "MeshTemplate.h"                  // IGeometryTemplate (getClassId, +0x10)
#include "RoadCompiled.h"
#include "RoadCompiledTemplate.h"          // CID_RoadCompiledTemplate

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../Game/Objects/IObject.h"
#include "../Scene/Object/ObjectManager.h"

#include <string>

namespace dice {
namespace hfe {

extern IBase* objectManager;               // PluginEntry.cpp

namespace geom {

namespace {

// Stands in for the original's list (ModuleGeom's unk_102543C4, which also holds this module's other
// rows) - the tree keeps one keyed list per binding TU.
const char kObjectGeometryConsoleTag[] = "geom.object";

// IID_IGeometry - dword_1021AE58, 0x492FE0FE, read as data by the lookup below. A file constant like
// GeometryTemplateManager.cpp's and MeshTemplate.cpp's: Core/Ids.h carries the same value but also
// defines CID_RoadCompiledTemplate with internal linkage, which RoadCompiledTemplate.h declares extern.
const unsigned int kIID_IGeometry = 0x492FE0FEu;

// the active object's IGeometry component, shared by all six rows: the host
// ObjectManager (tested), its active object (+0x90, tested), then queryComponent(IID_IGeometry,
// IID_IGeometry) (+0x58).
IBase* activeObjectGeometry()
{
    world::ObjectManager* const manager =
        reinterpret_cast<world::ObjectManager*>(::dice::hfe::objectManager);
    if (!manager)
        return 0;
    world::IObject* const object = manager->getActiveObject();              // +0x90
    if (!object)
        return 0;
    return object->queryComponent(kIID_IGeometry, kIID_IGeometry);          // +0x58
}

// that geometry as a RoadCompiled: only when its template (IGeometry +0x38) is a
// GeometryTemplate.RoadCompiled (getClassId +0x10 against the reflection id, read as data). The
// component is the IGeometry subobject, so the object is that member's offset lower (retail 4
// bytes, `lea eax, [esi-4]`).
// IGeometry has no typed interface in this tree; +0x38 is dispatched through the component's own
// table, as XpLightManager.cpp and ObjectDrawer.cpp do.
RoadCompiled* activeRoadCompiled()
{
    IBase* const geometry = activeObjectGeometry();
    if (!geometry)
        return 0;

    typedef IGeometryTemplate* (__thiscall* GetTemplateFn)(void*);
    IGeometryTemplate* const tmpl = reinterpret_cast<GetTemplateFn>(
        (*reinterpret_cast<void***>(geometry))[0x38 / 4])(geometry);
    if (!tmpl)
        return 0;
    if (tmpl->getClassId() != CID_RoadCompiledTemplate)
        return 0;
    return reinterpret_cast<RoadCompiled*>(reinterpret_cast<unsigned char*>(geometry)
                                           - offsetof(RoadCompiled, m_geometry));
}

// geometry.GenerateLightmapForThisInstance: the active object's geometry flag behind IGeometry
// +0xBC (get) / +0xB8 (set) - on a StaticMesh, bit 1 of its flags (StaticMesh::getFlagBit1 /
// setFlagBit1). The executor fetches the geometry again without a null check.
bool getGenerateLightmapForThisInstance()
{
    IBase* const geometry = activeObjectGeometry();
    typedef bool (__thiscall* GetFn)(void*);
    return reinterpret_cast<GetFn>(
        (*reinterpret_cast<void***>(geometry))[0xBC / 4])(geometry);
}

void setGenerateLightmapForThisInstance(bool value)
{
    IBase* const geometry = activeObjectGeometry();
    typedef void (__thiscall* SetFn)(void*, bool);
    reinterpret_cast<SetFn>(
        (*reinterpret_cast<void***>(geometry))[0xB8 / 4])(geometry, value);
}

} // anonymous namespace

// `SetConsoleClassDefaults("Object", 2, 0)` - stored as "object", which is how the .con files spell it.
BF2_CONSOLE_OBJECT("Object", 2, 0)

// Object.geometry.GenerateLightmapForThisInstance - a bool property, active while the active object
// has a geometry. Registered ahead of geometry.loadMesh, its neighbour in the shipped list.
BF2_CONSOLE_PROPERTY_FN_NAMED("geometry.GenerateLightmapForThisInstance", bool,
                              activeObjectGeometry() != 0, getGenerateLightmapForThisInstance(),
                              setGenerateLightmapForThisInstance, kObjectGeometryConsoleTag)

// Binds geometry.loadMesh to the active compiled road; requires one string argument.
// The console checks activity before execution; the executor repeats the lookup without
// a null guard. Keep class and constructor in one macro invocation for matching __LINE__.
#define BF2_OBJECT_GEOMETRY_LOADMESH_ROW(fnNameLiteral, listTag)                                     \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD_NAMED(fnNameLiteral, 0, 1, 1, "void", false)                                       \
            m_argTypes[0] = "std::string";                                                         \
    BF2_CC_TAIL(1, listTag)                                                                        \
    virtual bool isObjectActive() const { return activeRoadCompiled() != 0; }                      \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 1) return;                                                               \
        const std::string copy(BF2_CC_ARG(std::string, 0).c_str());                               \
        m_hasReturnValue = false;                                                                  \
        activeRoadCompiled()->loadMesh(copy);                                                      \
    }                                                                                              \
    virtual void setArgFromString(int index, const std::string& value)                             \
    {                                                                                              \
        BF2_CC_CONVERT(std::string, 1)                                                             \
    }                                                                                              \
    BF2_CC_NO_RETURN_AS_STRING                                                                     \
    BF2_CC_COMMON_HOOKS                                                                            \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

BF2_OBJECT_GEOMETRY_LOADMESH_ROW("geometry.loadMesh", kObjectGeometryConsoleTag)
#undef BF2_OBJECT_GEOMETRY_LOADMESH_ROW

bool registerObjectGeometryConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kObjectGeometryConsoleTag));
}

bool unregisterObjectGeometryConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kObjectGeometryConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
