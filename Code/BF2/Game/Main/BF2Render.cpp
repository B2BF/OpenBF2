// Client renderer lifecycle, frame, and display-mode wrappers. Console screen-mode
// commands and Alt+Enter use this class; video-options apply goes through VideoSettings.

#include "../../Core/BuildConfig.h"

#if !DEDICATED

#include "BF2ClientSeams.h"
#include "BF2Engine.h"
#include "BF2Window.h"
#include "IVideoOptions.h"

#include "../../Core/ClassManager.h"
#include "../../Core/Debug.h"                    // setDisplayMode's "ScreenMode out of range" (line 460)
#include "../../Core/EventManager.h"             // eventManager - the two ECRenderer posts
#include "../../Core/IBase.h"
#include "../../Core/IProfilerClient.h"
#include "../../Core/PluginDll.h"
#include "../../Core/SettingsRepostitory.h"
#include "../Utility/BuildNrUtil.h"
#include "../IGameLogic.h"
#include "../../../RendDX9Abi/DisplayModeAbi.h"

// render's own collaborators.
#include "../../Math/Mat4.h"
#include "../../RendMisc/IRenderer.h"
#include "../../RendMisc/IRenderView.h"          // the view `render` configures every frame
#include "../../Scene/Object/PlayerManager.h"    // world::playerManager (ds:0xA08F60)
#include "../DemoRecorder/DemoConfig.h"          // getFOV / getAspect on the embedded config
#include "../DemoRecorder/DemoRecorder.h"
#include "../Objects/ICameraObject.h"            // getTransformation(float) at +0x10
#include "../Objects/IPlayer.h"                  // getCamera (+0x17C) and getFov (+0x8C)
#include "../../Physics/LatencyCompensation.h"   // the two frame-telemetry words render publishes
#include "../../Physics/CollisionManager.h"

#include <algorithm>                              // std::find - listScreenModes' discarded search
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <windows.h>

namespace dice {
namespace hfe {

extern ClassManager*        classManager;
extern SettingsRepostitory* settings;
extern BF2Engine*           bf2Engine;      // Shared engine owner.
                                            // [UNRESOLVED] Profiler/thread registry published by the client runtime.
                                            // [CHOSEN] Local placement; initialization queries it conditionally, while the
                                            // original VTune render bracket requires a published instance.
IBase* profilerSingleton = 0;

// Frame telemetry is owned by LatencyCompensation and written only by rendering.
// Its product supplies the clamped render-ahead latency.

BF2Render* g_bf2Render               = 0; // ds:0x9A5580
IBase*     g_geometryTemplateManager = 0; // ds:0x9A5578

namespace {

// Pauses or resumes VTune around repeated demo-frame rendering.
// [CHOSEN, not recovered] The null check stops at a named frontier until the
// profiler publisher is recovered. [RETIRE-WHEN: client-frame-arm]
void profilerSetSuspended(IBase* profiler, bool suspended)
{
    if (!profiler)
    {
        std::fprintf(stderr, "[FRONTIER] dice::hfe::BF2Render::render: the ds:0x9A5C6C profiler "
                             "singleton is unpublished (its owner at PE 0x68CC60 is unrecovered), "
                             "so its +0x2C(%d) VTune bracket cannot be dispatched\n",
                     suspended ? 1 : 0);
        std::fflush(stderr);
        std::abort();
    }
    static_cast<IProfilerClient*>(profiler)->pauseVTune(suspended);
}

// Releases collision debug geometry before the renderer module is unloaded.
void notifyCollisionManagerRendererShutdown(IBase* manager)
{
    static_cast<world::ICollisionManager*>(manager)->destroyDebugCollisionMeshes();
}

// The attested __FILE__ setDisplayMode's Debug() site carries (BF2.exe ds:0x89C538) - the same
// literal this file's Path line cites.
const char kSourcePath[] =
    "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Game\\Main\\BF2Render.cpp";

// View of RendDX9's plugin-owned display modes. Both plugin flavors retain the
// VC7.1 container layout and 40-byte records; this is not a host-owned exchange
// container. The retail display-mode carrier embeds one record at offset 4.
struct RendererDisplayMode
{
    /* +0x00 */ unsigned short      width;
    /* +0x02 */ unsigned short      height;
    /* +0x04 */ unsigned short      refreshHz;
    /* +0x06 */ unsigned char       bpp;
    /* +0x07 */ unsigned char       pad07;          // never copied by 0x4547F0
    /* +0x08 */ vc71::vector_layout depthFormats;   // D3DFORMAT dwords
    /* +0x18 */ vc71::vector_layout multiSamples;   // renddx9abi::DisplayModeMultiSample
};
static_assert(sizeof(RendererDisplayMode) == 40, "Renderer +0x3C hands back 40-byte records");

// size() with VC7.1's own arithmetic: 0 while _First is null, else (_Last - _First) / 40 as a signed
// pointer difference - the imul / sar / shr-add sequence at 0x453849 and 0x454970 is a signed divide.
int rendererModeCount(const vc71::vector_layout& modes)
{
    if (!modes.first)
        return 0;
    return static_cast<int>(modes.last - modes.first) / static_cast<int>(sizeof(RendererDisplayMode));
}

const RendererDisplayMode& rendererModeAt(const vc71::vector_layout& modes, unsigned int index)
{
    return reinterpret_cast<const RendererDisplayMode*>(modes.first)[index];
}

// Requests modes with all three filter flags enabled. Renderer caching means
// an earlier VideoOptions query can already have populated the list with its filter.
const vc71::vector_layout& rendererDisplayModes(dx9rend::IRenderer* renderer)
{
    dx9rend::DisplayModeFilter filter;
    filter.aspect4by3                = true;                  // [ebp-4]
    filter.crossCheckDisplaySettings = true;                  // [ebp-3]
    filter.minimumResolution         = true;                  // [ebp-2]
    return *static_cast<const vc71::vector_layout*>(renderer->slot_0x3C(&filter));
}

// Requires a nonempty depth-format list; init and setDisplayMode retain one entry.
// Returns a borrowed reference. Native and compatibility empty-list behavior differs.
const unsigned int& carrierDepthFront(const renddx9abi::HostDisplayMode& carrier)
{
    return *renddx9abi::hostDepthFirst(carrier);
}

// Copies display-mode scalars and assigns both owned lists.
// [PLATFORM] The native carrier flattens the fields; compatibility uses the ABI
// carrier's owned-range assignment.
void assignDisplayMode(renddx9abi::HostDisplayMode& carrier, const RendererDisplayMode& mode)
{
    carrier.width     = mode.width;
    carrier.height    = mode.height;
    carrier.refreshHz = mode.refreshHz;
    carrier.bpp       = mode.bpp;

    const unsigned int* depthFirst =
        reinterpret_cast<const unsigned int*>(mode.depthFormats.first);
    const unsigned int* depthLast =
        reinterpret_cast<const unsigned int*>(mode.depthFormats.last);
    const renddx9abi::DisplayModeMultiSample* sampleFirst =
        reinterpret_cast<const renddx9abi::DisplayModeMultiSample*>(mode.multiSamples.first);
    const renddx9abi::DisplayModeMultiSample* sampleLast =
        reinterpret_cast<const renddx9abi::DisplayModeMultiSample*>(mode.multiSamples.last);
#if BF2_PLUGIN_ABI_NATIVE
    carrier.depthFormats.assign(depthFirst, depthLast);
    carrier.multiSamples.assign(sampleFirst, sampleLast);
#else
    renddx9abi::assignOwnedDisplayVector<unsigned int>(carrier.depthFormats, depthFirst, depthLast);
    renddx9abi::assignOwnedDisplayVector<renddx9abi::DisplayModeMultiSample>(
        carrier.multiSamples, sampleFirst, sampleLast);
#endif
}

} // anonymous namespace

BF2Render::BF2Render(BF2Engine* engine)
    : m_engine(engine)
    , m_window(0)
    , m_rendererDll(0)
    , m_renderer(0)
    , m_postProductionFrameWork(0)
    , m_displayModeCarrier(0)
    , m_displayModeIndex(-1)
{
    // : the constructor publishes itself and the shipped destructor does not clear it.
    g_bf2Render = this;
}

BF2Render::~BF2Render()
{
    shutdown();
}

void BF2Render::beforeFrame()
{
    // Retail checks the DLL handle but does not check the renderer pointer here.
    if (m_rendererDll && (!gameLogic || gameLogic->isLevelLoaded()))
        m_renderer->beginFrame();
}

void BF2Render::update(double frameTime)
{
    if (m_rendererDll && m_renderer && (!gameLogic || gameLogic->isLevelLoaded()))
        m_renderer->update(frameTime);
}

int BF2Render::getDisplayModeIndex() const { return m_displayModeIndex; }
IBase* BF2Render::getPostProductionFrameWork() { return m_postProductionFrameWork; }
bool BF2Render::rendererSlot0x14() // unguarded tail dispatch
{ return m_renderer->slot_0x14(); }
bool BF2Render::rendererSlot0x18() // unguarded tail dispatch
{ return m_renderer->slot_0x18(); }

IBase* BF2Render::getRenderView()
{
    return m_renderer->getRenderView();
}

bool BF2Render::init()
{
    m_window = m_engine->getWindow();
    assert(m_window && "create a window");              // BF2Render.cpp:166

    // [CHOSEN, not recovered] The "-LC" branding stays in the window title. It used to be part
    // of BuildNrUtil::getBuildNrString, which is protocol - the GameSpy buildNr key and the
    // gamever filter clause - so a branded stamp cut the rebuilt client off from every real server.
    // A window title reaches nothing but the taskbar.
    std::ostringstream title;
    title << "BF2 (v" << BuildNrUtil::getBuildNrString() << "-LC"
          << ", pid: " << ::GetCurrentProcessId() << ")";

    const bool fullscreen = settings->boolGet("GSFullScreen", false);
    const int width        = settings->intGet("GSWindowSizeX", 800);
    const int height       = settings->intGet("GSWindowSizeY", 600);

    if (!m_window->isCreated()
        && !m_window->createFromSettings(title.str(), width, height,
                                         fullscreen, fullscreen, -1))
        return false;

    assert(m_window->getHwnd() && "window not created"); // BF2Render.cpp:188

    m_displayModeCarrier = new renddx9abi::HostDisplayMode();
    m_displayModeCarrier->fullscreen = fullscreen ? 1 : 0;
    m_displayModeCarrier->width       = static_cast<unsigned short>(width);
    m_displayModeCarrier->height      = static_cast<unsigned short>(height);
    m_displayModeCarrier->refreshHz   = static_cast<unsigned short>(
        settings->intGet("GSRefresh", 85));
    m_displayModeCarrier->bpp         = 32;
    m_displayModeCarrier->vsync       = 0;

#if BF2_PLUGIN_ABI_NATIVE
    m_displayModeCarrier->depthFormats.push_back(75u); // D3DFMT_D24S8
    m_displayModeCarrier->windows.push_back(
        reinterpret_cast<void*>(m_window->getHwnd()));
#else
    vc71::vectorRef<unsigned int>(&m_displayModeCarrier->depthFormats).push_back(75u);
    void* hwnd = reinterpret_cast<void*>(m_window->getHwnd());
    vc71::vectorRef<void*>(&m_displayModeCarrier->windows).push_back(hwnd);
#endif

    if (fullscreen)
        m_window->toggleFullscreenStyle();

    SingletonRegInfo temporaryWindow;
    temporaryWindow.name     = "hWndBf2TempSlt";
    temporaryWindow.instance = reinterpret_cast<IBase**>(&m_displayModeCarrier);
    temporaryWindow.iid      = 0;
    temporaryWindow.classId  = 0;
    temporaryWindow.flags    = 4;
    classManager->registerSingleton(temporaryWindow);

    m_rendererDll = LoadLibraryBf("RendDX9.dll");
    if (!m_rendererDll)
    {
        // Shipped behavior: failure returns without unregistering the temporary singleton. The
        // carrier is likewise only destroyed when a renderer DLL handle exists in shutdown().
        return false;
    }

    g_geometryTemplateManager = classManager->getSingleton("GeometryTemplateManager");
    m_renderer = static_cast<dx9rend::IRenderer*>(classManager->getSingleton("Renderer"));
    g_renderer                = m_renderer;

    // Unchecked in BF2.exe: a successfully initialized plugin is required to publish Renderer.
    m_renderer->initialize();

    m_postProductionFrameWork = classManager->getSingleton("PostProductionFrameWork");
    classManager->unregisterSingleton("hWndBf2TempSlt");

    // Two result-discarded virtual calls at 0x4547BD/0x4547C2, preserved in order.
    (void)m_window->getHasFocus();
    (void)m_window->isCreated();
    return true;
}

void BF2Render::shutdown()
{
    IBase* collisionManager = classManager->getSingleton("CollisionManager");
    if (collisionManager)
        notifyCollisionManagerRendererShutdown(collisionManager);

    if (m_rendererDll)
    {
        if (m_displayModeCarrier)
        {
#if !BF2_PLUGIN_ABI_NATIVE
            // The rebuilt client uses native mode. Keep the retail branch complete for isolated
            // ABI compilation: its POD carrier has no automatic vector destructors.
            renddx9abi::clearOwnedDisplayVector<void*>(m_displayModeCarrier->windows);
            renddx9abi::clearOwnedDisplayVector<renddx9abi::DisplayModeMultiSample>(
                m_displayModeCarrier->multiSamples);
            renddx9abi::clearOwnedDisplayVector<unsigned int>(
                m_displayModeCarrier->depthFormats);
#endif
            delete m_displayModeCarrier;
        }

        FreeLibraryBf(m_rendererDll, "Renderer");
    }
}

// Configures the frame's camera and view distance, then drives renderer output.
// A renderer module must be loaded. Active paths require player manager, render view,
// video options and profiler services; those dependencies are not null-checked.
bool BF2Render::render(double frameTime, DemoRecorder* recorder)
{
    // : the module handle decides, not the renderer pointer.
    if (!m_rendererDll)
        return false;

    // : nothing is drawn until the level is in - and that counts as a rendered frame.
    if (gameLogic && !gameLogic->isLevelLoaded())
        return true;

    if (recorder && recorder->isOpenForPlayback())
    {
        // [UNRESOLVED] This aggregate starts uninitialized, unlike the original identity
        // Mat4 constructor. getPlaybackMatrixAndFOV writes all of it whenever the spectator
        // rig exists (playback always creates one) and leaves it untouched otherwise.
        Mat4 transform;
        recorder->getPlaybackMatrixAndFOV(&transform, 0);
        const float fov    = recorder->demoConfig.getFOV();
        const float aspect = recorder->demoConfig.getAspect();
        m_renderer->getRenderView()->setFov(fov);                  // +0x1C
        m_renderer->getRenderView()->setViewMatrix(transform.m);   // +0x60
        m_renderer->getRenderView()->setAspect(aspect);            // +0x40
    }
    else if (world::playerManager->getLocalHumanPlayer())
    {
        // : retail asks the manager again here and a third time below, and asks the player
        // for its camera twice. Both repeats are preserved.
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (player->getCamera())
        {
            Mat4 transform(world::playerManager->getLocalHumanPlayer()->getCamera()
                               ->getTransformation(static_cast<float>(frameTime)));
            const float fov = player->getFov();                          // +0x8C
            m_renderer->getRenderView()->setAdjustedFov(fov);       // +0x18
            m_renderer->getRenderView()->setViewMatrix(transform.m);// +0x60
        }
    }

    // : the far plane, reached on every path - including the two that set no view at all.
    const int         viewDistanceDefault = 500;                    // 0x1F4
    const std::string viewDistanceName("GSViewDistance");
    IVideoOptions* videoOptions = m_renderer->getVideoOptions();    // +0x74, before the settings read
    const int& viewDistance = settings->intGet(viewDistanceName, viewDistanceDefault);
    const float farPlane = videoOptions->getViewDistanceScale() * viewDistance;
    m_renderer->getRenderView()->setFarPlane(farPlane);             // +0x38

    // : ordinarily one pass. A demo carrying the VTune flag suspends the profiler around
    // the loop and repeats the same frame vtuneRepeatCount times. The `config` null test retail
    // emits is dead - the pointer is the address of an embedded member - but it is what the
    // instructions do, so it stays.
    const DemoConfig* config = 0;
    if (recorder)
        config = &recorder->demoConfig;

    int passes = 1;
    if (recorder && recorder->byte84)
    {
        profilerSetSuspended(profilerSingleton, false);
        passes = config ? config->vtuneRepeatCount : 1;
    }

    // : the first pass gets the real frame time and every repeat gets 0.0 (ds:0x880F08), so
    // the scene is redrawn without advancing. `result` starts true, so a zero count answers true.
    double passFrameTime = frameTime;
    bool result = true;
    while (passes-- != 0)
    {
        // Pass the clamped main-loop delta to rendering and HUD animation.
        result = m_renderer->renderFrame(passFrameTime, bf2Engine->mainLoopDeltaTime); // +0x28
        passFrameTime = 0.0;
        if (!result)
            break;
    }

    // : the telemetry pair, published only when the value is strictly positive. Retail
    // fetches +0xA0 twice - once to test it, once to divide - and that is kept.
    if (m_renderer->getFrameRate() > 0.0)                           // ds:0x93C850 is 0.0
    {
        rendererInverseFrameTime = static_cast<float>(1.0 / m_renderer->getFrameRate());
        rendererFrameCount = static_cast<int>(m_renderer->getRenderAheadLimit());
    }

    if (recorder && recorder->byte84)
        profilerSetSuspended(profilerSingleton, true);

    return result;
}

// Intentionally empty virtual slot; its original name remains [UNRESOLVED].
void BF2Render::slot0x10()
{
}

// Toggles window style and renderer fullscreen state, updates settings, then posts the event.
// Requires window, carrier, renderer, settings and event manager. Renderer refusal is ignored.
// The event payload contains height alone; consumers expecting packed dimensions observe width zero.
// [UNRESOLVED] The source return type is unknown; the declared unsigned result preserves postEvent's value.
unsigned int BF2Render::toggleFullscreen()
{
    m_window->toggleFullscreenStyle();                                          // +0x48
    m_displayModeCarrier->fullscreen = (m_displayModeCarrier->fullscreen == 0);
    m_renderer->setDisplayMode(*m_displayModeCarrier);                          // +0x10

    settings->boolSet(std::string("GSFullScreen"),                              // +0x54
                      reinterpret_cast<const bool&>(m_displayModeCarrier->fullscreen));

    long long payload = static_cast<int>(m_displayModeCarrier->height);         // movzx / cdq
    return eventManager->postEvent(static_cast<EventCategory>(11), 1,          // ECRenderer,
                                   &payload, 0.0f);                             //   REToggleFullscreen
}

// Ensures display modes are collected and checks their depth formats. Produces no
// listing because the original print path is absent. [INFERRED] Method spelling.
void BF2Render::listScreenModes()
{
    const vc71::vector_layout& modes = rendererDisplayModes(m_renderer);        // +0x3C
    for (unsigned int i = 0; i < static_cast<unsigned int>(rendererModeCount(modes)); ++i)
    {
        const RendererDisplayMode& mode = rendererModeAt(modes, i);
        const unsigned int* first = reinterpret_cast<const unsigned int*>(mode.depthFormats.first);
        const unsigned int* last  = reinterpret_cast<const unsigned int*>(mode.depthFormats.last);
        (void)std::find(first, last, carrierDepthFront(*m_displayModeCarrier)); // discarded
    }
}

// Selects a display mode; an out-of-range index is a fatal error.
// Preserves the current depth format, clears multisampling, resizes the window and
// posts REChangeResolution after applying the mode. No pre-change event is posted.
// Requires the renderer, mode carrier, window and event manager; renderer refusal is ignored.
// [PLATFORM] Native vector operations retain their own allocation semantics.
void BF2Render::setDisplayMode(int index)
{
    const vc71::vector_layout& modes = rendererDisplayModes(m_renderer);        // +0x3C
    if (index < 0 || index >= rendererModeCount(modes))
    {
        Debug(DebugTypeError, kSourcePath, 460, "") << "ScreenMode out of range";
        return;
    }

    const unsigned int depthFormat = carrierDepthFront(*m_displayModeCarrier);
    assignDisplayMode(*m_displayModeCarrier, rendererModeAt(modes, index));
#if BF2_PLUGIN_ABI_NATIVE
    m_displayModeCarrier->depthFormats.clear();
    m_displayModeCarrier->depthFormats.push_back(depthFormat);
    m_displayModeCarrier->multiSamples.clear();
#else
    renddx9abi::clearOwnedDisplayVector<unsigned int>(m_displayModeCarrier->depthFormats);
    vc71::vectorRef<unsigned int>(&m_displayModeCarrier->depthFormats).push_back(depthFormat);
    renddx9abi::clearOwnedDisplayVector<renddx9abi::DisplayModeMultiSample>(
        m_displayModeCarrier->multiSamples);
#endif

    m_window->resizeClient(m_displayModeCarrier->width,                         // +0x44
                           m_displayModeCarrier->height);
    m_renderer->setDisplayMode(*m_displayModeCarrier);                          // +0x10
    m_displayModeIndex = index;

    // `shl ecx, 10h / add ecx, edx` (0x454A27..0x454A2A). Unsigned here only so a width above
    // 0x7FFF cannot overflow a signed shift; the dword is bit-identical.
    unsigned int packed = (static_cast<unsigned int>(m_displayModeCarrier->width) << 16)
                        + m_displayModeCarrier->height;
    eventManager->postEvent(static_cast<EventCategory>(11), 2, &packed, 0.0f); // ECRenderer,
}                                                                               //   REChangeResolution

} // namespace hfe
} // namespace dice

#endif // !dedicated
