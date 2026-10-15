// Registers render views, environment maps and their console bindings.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginModules.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Frontier.h"
#include "EnvMapSystem/EnvMapManager.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../../RendDX9Abi/PluginEntry.h"

namespace dice {
namespace hfe {
namespace Rend {

// Declared here rather than through a header: RenderView.cpp is the only other file in this image
// that defines it, and a one-line extern is smaller than a header for one symbol.
IBase* __fastcall createRenderView(unsigned int iid, IBase* outer);

extern const unsigned int IID_IRenderView;
extern const unsigned int CID_RenderView;

// The two globals RendModule's registrations point at - 0x10267A30 and 0x10267A34 in the shipped
// image, adjacent, which is what a pair of file-scope pointers looks like.
IBase* g_RenderView    = 0;
IBase* g_EnvMapManager = 0;

RD9_MODULE_IBASE_BODIES(RendModule)


// Publishes RenderView followed by EnvMapManager.

void RendModule::getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::Rend::RendModule::getClassList", 0x10);

    ::renddx9abi::HostVectorWriter<ClassRegInfo> out(hostVector);

    // RenderView now has a body - see RendMisc/RenderView.cpp. Registering it is what turns the
    // "RenderView" singleton below from null into a real object, and it is also what
    // Renderer::initialize's createInstance(820301, 820201, 0) resolves against.
    ClassRegInfo info;
    info.classId = CID_RenderView;
    info.name    = "dice.hfe.Rend.RenderView";
    info.create  = RD9_CLASS_FACTORY(&createRenderView);
    out.push_back(info);

    info.classId = 820302u;
    info.name    = "dice.hfe.Rend.EnvMapManager";
    info.create  = RD9_CLASS_FACTORY(&createEnvMapManager);
    out.push_back(info);
}


// getSingletonList, 0x101C236E. Two entries, both flags == 2, read from the
// sub_100518D0(name, instance, iid, classId, flags) call sites.

// The RenderView pair is corroborated from the other end: Renderer::initialize (0x1002C8C0) asks
// the host for createInstance(820301, 820201, 0) - the same classId and iid, in the argument order
// createInstance takes.

void RendModule::getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::Rend::RendModule::getSingletonList", 0x14);

    ::renddx9abi::HostVectorWriter<SingletonRegInfo> out(hostVector);

    {
        SingletonRegInfo info;
        info.name     = "RenderView";
        info.instance = &g_RenderView;
        info.iid      = IID_IRenderView;
        info.classId  = CID_RenderView;
        info.flags    = 2;
        out.push_back(info);
    }
    {
        SingletonRegInfo info;
        info.name     = "EnvMapManager";
        info.instance = &g_EnvMapManager;
        info.iid      = 820202u;
        info.classId  = 820302u;
        info.flags    = 2;
        out.push_back(info);
    }
}


// Native environment-map console bindings, available while the manager exists.
// Method results are discarded; ConsoleObject layouts must match the host toolchain.

namespace {

const char kEnvMapManagerConsoleTag[] = "rend.envmapManager";

inline EnvMapManager* envMapManager()
{
    return static_cast<EnvMapManager*>(g_EnvMapManager);
}

// the argument copied, then the manager's +0x34 (loadInfo) - the renderer's own string at the
// call, converted here, once.
void consoleLoadEMIFile(const std::string& path)
{
    envMapManager()->loadInfo(std::string(path.c_str(), static_cast<unsigned int>(path.size())));
}

void consoleGenerateEnvMaps()
{
    envMapManager()->generateEnvMaps();
}

void consoleSaveEnvMaps()
{
    envMapManager()->save();
}

bool consoleGetPerPixelEnvironmentMapping()
{
    return envMapManager()->getPerPixelEnabled();
}

void consoleSetPerPixelEnvironmentMapping(bool enabled)
{
    envMapManager()->setPerPixelEnabled(enabled);
}

BF2_CONSOLE_OBJECT("envmapManager", 2, 0)

BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(loadEMIFile, consoleLoadEMIFile, g_EnvMapManager != 0,
                                   std::string, kEnvMapManagerConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(generateEnvMaps, consoleGenerateEnvMaps, g_EnvMapManager != 0,
                                   kEnvMapManagerConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(save, consoleSaveEnvMaps, g_EnvMapManager != 0,
                                   kEnvMapManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(perPixelEnvironmentMappingEnabled, bool, g_EnvMapManager != 0,
                        consoleGetPerPixelEnvironmentMapping(),
                        consoleSetPerPixelEnvironmentMapping, kEnvMapManagerConsoleTag)

} // anonymous namespace

// PE 0x101C2013 (19 B), `mov ecx, dword_10244A98; push offset 0x10267A38; call
// 0x1007B570` - ConsoleObjects::registerConsoleObjects(this TU's list) - then `mov al, 1`. The native
// flavour publishes the list above; the compatibility flavour publishes no console list at all (the
// registry carries the host toolchain's STL - the same split ModuleGeom::init and DX9RendModule::init
// make), so there it answers true with nothing registered.
bool RendModule::init()
{
    RD9_TRACE("dice::hfe::Rend::RendModule::init", 0x18);
    mainConsoleObjects->registerConsoleObjects(consoleBindingList(kEnvMapManagerConsoleTag));
    return true;
}

// PE 0x101C2026 (19 B), the twin - ConsoleObjects::unregisterConsoleObjects
// (0x1007B6D0) on the same list, then true.
bool RendModule::close()
{
    RD9_TRACE("dice::hfe::Rend::RendModule::close", 0x1C);
    mainConsoleObjects->unregisterConsoleObjects(consoleBindingList(kEnvMapManagerConsoleTag));
    return true;
}

} // namespace Rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
