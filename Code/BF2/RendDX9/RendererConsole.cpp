
// Native renderer console bindings for fog, presentation and editor capture/LOD settings.
// [INFERRED] companion source placement. Availability follows the owning renderer or
// post-processing singleton; only the bindings declared below are published.

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/PluginAbi.h"
#if BF2_PLUGIN_ABI_NATIVE
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../Core/IBase.h"
#include "../Core/Types.h"
#include "../Math/Vec3.h"
#include "RendDX9Base.h"

namespace dice {
namespace hfe {
namespace dx9rend {

extern IBase* g_PostProductionFrameWorkOwner;     // BF2Render.cpp - ds:0x1023F7C0
extern void*  g_rendererBase;                     // RendDX9Init.cpp - dword_1023B970
extern IBase* g_Renderer;                         // BF2Render.cpp - dword_1023D098

// BF2Render.cpp - Renderer::makeTopWorldScreen (0x1002C5F0) and the two forced-lod
// setters (0x1002D480 / 0x1002D4A0) with the renderer dwords their GET arms read (+0x7DC / +0x7E0).
void rendererMakeTopWorldScreen(unsigned int resolution, renddx9abi::HostString name,
                                Vec3 position, float orthoExtent);
void rendererSetSkinnedMeshLod(int lod);
void rendererSetBundledMeshLod(int lod);
int  rendererGetSkinnedMeshLod();
int  rendererGetBundledMeshLod();
// BF2Render.cpp - drawHud's arms (0x1005CCC0) over the renderer byte +0x6B1.
bool rendererGetDrawHud();
void rendererSetDrawHud(bool on);

namespace {

// Stands in for the original's file-scope list (the static at 0x10241668).
const char kRendererConsoleTag[] = "dx9rend.renderer";


// Boolean presentation properties: zero arguments reads, one writes.
// Require an active renderer; changing vsync resets its device.

bool getVSync()                     { return rendererGetVSync(g_rendererBase); }
void setVSync(bool value)           { rendererSetVSync(g_rendererBase, value); }
bool getPresentAsync()              { return rendererGetPresentAsync(g_rendererBase); }
void setPresentAsync(bool value)    { rendererSetPresentAsync(g_rendererBase, value); }
bool getPresentSpinIfBusy()         { return rendererGetPresentSpinIfBusy(g_rendererBase); }
void setPresentSpinIfBusy(bool value) { rendererSetPresentSpinIfBusy(g_rendererBase, value); }

// fogColor - ctor 0x100586B0 (type "Vec3"). Read arm 0x100588D0 -> the stored,
// normalised colour times 255. Write arm 0x10058930 -> divided by 255 on the way in.
Vec3 getFogColor()
{
    Vec3 v;
    postProductionGetFogColor255(&v.x);
    return v;
}

void setFogColor(const Vec3& v)
{
    postProductionSetFogColor255(&v.x);
}

// waterFogColor - ctor 0x10058950 (type "Vec3"). Read arm 0x10058AF1 -> 0x1004F700, write arm
// 0x10058B3D -> 0x1004F690, which stores the one colour into both water-fog colour slots.
Vec3 getWaterFogColor()
{
    Vec3 v;
    postProductionGetWaterFogColor255(&v.x);
    return v;
}

void setWaterFogColor(const Vec3& v)
{
    postProductionSetWaterFogColor255(&v.x);
}


// Native capture adapter. The renderer's compatibility path captures at a fixed 2048 square.

void consoleMakeTopWorldScreen(U32 resolution, const std::string& name, const Vec3& position,
                               float orthoExtent)
{
    rendererMakeTopWorldScreen(resolution, renddx9abi::HostString(name), position, orthoExtent);
}

BF2_CONSOLE_OBJECT("renderer", 2, 0)

// in the shipped static-initialiser order, ahead of fogColor.
BF2_CONSOLE_PROPERTY_FN(vsync, bool, g_rendererBase != 0,
                        getVSync(), setVSync, kRendererConsoleTag)
BF2_CONSOLE_PROPERTY_FN(presentAsync, bool, g_rendererBase != 0,
                        getPresentAsync(), setPresentAsync, kRendererConsoleTag)
BF2_CONSOLE_PROPERTY_FN(presentSpinIfBusy, bool, g_rendererBase != 0,
                        getPresentSpinIfBusy(), setPresentSpinIfBusy, kRendererConsoleTag)

BF2_CONSOLE_METHOD_4_STATIC_ACTIVE(makeTopWorldScreen, consoleMakeTopWorldScreen, g_Renderer != 0,
                                   U32, std::string, Vec3, float, kRendererConsoleTag)

BF2_CONSOLE_PROPERTY_FN(fogColor, Vec3, g_PostProductionFrameWorkOwner != 0,
                        getFogColor(), setFogColor, kRendererConsoleTag)
BF2_CONSOLE_PROPERTY_FN(waterFogColor, Vec3, g_PostProductionFrameWorkOwner != 0,
                        getWaterFogColor(), setWaterFogColor, kRendererConsoleTag)

BF2_CONSOLE_PROPERTY_FN(forceSkinnedMeshLod, int, g_Renderer != 0,
                        rendererGetSkinnedMeshLod(), rendererSetSkinnedMeshLod, kRendererConsoleTag)
BF2_CONSOLE_PROPERTY_FN(forceBundledMeshLod, int, g_Renderer != 0,
                        rendererGetBundledMeshLod(), rendererSetBundledMeshLod, kRendererConsoleTag)

// drawHud - ctor 0x1005CB70 (type "bool"), active while g_Renderer exists (0x1005B930).
BF2_CONSOLE_PROPERTY_FN(drawHud, bool, g_Renderer != 0,
                        rendererGetDrawHud(), rendererSetDrawHud, kRendererConsoleTag)

} // anonymous namespace

bool registerRendererConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kRendererConsoleTag));
}

bool unregisterRendererConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kRendererConsoleTag));
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_PLUGIN_ABI_NATIVE
#endif // BF2_IMAGE_RENDDX9
