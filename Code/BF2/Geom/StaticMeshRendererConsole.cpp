// Native StaticMeshRenderer console bindings: the two object-lightmap atlas builders,
// loadAllLightMapFiles and loadAllLightMapFilesDLL. [INFERRED] file placement; 1.50 registers these
// rows in ModuleGeom's list after the StaticMeshRenderer properties, which this file does not bind.
// Compatibility builds cannot share host ConsoleObject layouts and omit these bindings.

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/PluginAbi.h"
#if BF2_PLUGIN_ABI_NATIVE

#include "StaticMeshRenderer.h"

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"

#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

// Stands in for the original's list (ModuleGeom's, which also holds this module's other rows) -
// the tree keeps one keyed list per binding TU.
const char kStaticMeshRendererConsoleTag[] = "geom.staticMeshRenderer";

// The renderer as both rows' activity tests and executors find it: the ClassManager singleton
// "StaticMeshRenderer", looked up by name on every call.
StaticMeshRenderer* staticMeshRendererSingleton()
{
    IBase* const found = classManager->getSingleton(::renddx9abi::HostString("StaticMeshRenderer"));
    return static_cast<StaticMeshRenderer*>(static_cast<void*>(found));
}

// Both executors copy their two arguments, then call the builder on the singleton without a null
// check of their own.
bool consoleLoadAllLightMapFiles(const std::string& directory, const std::string& extension)
{
    const vc71::string directoryCopy(directory.c_str());
    const vc71::string extensionCopy(extension.c_str());
    return staticMeshRendererLoadAllLightMapFiles(staticMeshRendererSingleton(), directoryCopy,
                                                  extensionCopy);
}

bool consoleLoadAllLightMapFilesDll(const std::string& directory, const std::string& extension)
{
    const vc71::string directoryCopy(directory.c_str());
    const vc71::string extensionCopy(extension.c_str());
    return staticMeshRendererLoadAllLightMapFilesDll(staticMeshRendererSingleton(), directoryCopy,
                                                     extensionCopy);
}

} // anonymous namespace

// `SetConsoleClassDefaults("StaticMeshRenderer", 2, 0)`.
BF2_CONSOLE_OBJECT("StaticMeshRenderer", 2, 0)

// StaticMeshRenderer.loadAllLightMapFiles <directory> <extension> - the batched atlas builder.
BF2_CONSOLE_METHOD_2_RET_ACTIVE(loadAllLightMapFiles, consoleLoadAllLightMapFiles, bool,
                                staticMeshRendererSingleton() != 0, std::string, std::string,
                                kStaticMeshRendererConsoleTag)
// StaticMeshRenderer.loadAllLightMapFilesDLL <directory> <extension> - the single-atlas builder.
BF2_CONSOLE_METHOD_2_RET_ACTIVE(loadAllLightMapFilesDLL, consoleLoadAllLightMapFilesDll, bool,
                                staticMeshRendererSingleton() != 0, std::string, std::string,
                                kStaticMeshRendererConsoleTag)

bool registerStaticMeshRendererConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kStaticMeshRendererConsoleTag));
}

bool unregisterStaticMeshRendererConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kStaticMeshRendererConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_PLUGIN_ABI_NATIVE
#endif // BF2_IMAGE_RENDDX9
