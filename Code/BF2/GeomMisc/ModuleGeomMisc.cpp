// Registers geometry utility classes. [INFERRED] Source placement.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginModules.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Frontier.h"
#include "FSQuadDrawer.h"

namespace dice {
namespace hfe {
namespace geom {

// DebugTextWriter.cpp. Declared rather than pulled through a header - three symbols.
IBase* __fastcall createDebugTextWriter(unsigned int iid, IBase* outer);
IBase* __fastcall createDebugTextWriterManager(unsigned int iid, IBase* outer);
extern const unsigned int IID_IDebugTextWriter;
extern const unsigned int CID_DebugTextWriter;

// FontSystem/FontSystem.cpp.
IBase* __fastcall createFontPorted(unsigned int iid, IBase* outer);
extern const unsigned int IID_IFontPorted;
extern const unsigned int CID_FontPorted;

// OverlayTexture.cpp.
IBase* __fastcall createOverlayTexture(unsigned int iid, IBase* outer);
extern const unsigned int IID_IOverlayTexture;
extern const unsigned int CID_OverlayTexture;

// DebugLineGraphD3d.cpp
extern const unsigned int CID_DebugLineGraph;
IBase* __fastcall createDebugLineGraph(unsigned int iid, IBase* outer);

// MenuRend/MemeTexture.cpp
extern const unsigned int CID_MemeTexture;
IBase* __fastcall createMemeTexture(unsigned int iid, IBase* outer);
extern const unsigned int CID_FSMoviePlayer;
IBase* __fastcall createFSMoviePlayer(unsigned int iid, IBase* outer);
extern const unsigned int CID_QuadDrawer;
IBase* __fastcall createQuadDrawer(unsigned int iid, IBase* outer);

// EditorSpline.cpp / PerlinNoise.cpp - the editor's spline (81033) and the road-edge noise
// (81035); created only through createInstance.
extern const unsigned int CID_EditorSpline;
IBase* __fastcall createEditorSpline(unsigned int iid, IBase* outer);
extern const unsigned int CID_PerlinNoise;
IBase* __fastcall createPerlinNoise(unsigned int iid, IBase* outer);

// FontSystem/SwiffFont.cpp - the Flash font stack (RW-80). Both are getSingletonList entries below
// (FontSystem cid 81044, FontRenderer cid 81047); registering the classes here is what makes those
// two singletons resolvable, so SwiffRender::createFont can build a real font wrapper.
extern const unsigned int CID_FontSystem;      // 81044
IBase* __fastcall createFontSystem(unsigned int iid, IBase* outer);
extern const unsigned int CID_FontRenderer;    // 81047
IBase* __fastcall createFontRenderer(unsigned int iid, IBase* outer);

// Register both Bink movie classes so FSMoviePlayer and SwiffRender can create Video by name.
extern const unsigned int CID_Video;           // 81048 [INFERRED]
IBase* __fastcall createVideo(unsigned int iid, IBase* outer);
extern const unsigned int CID_VideoTarget;     // 81049 [INFERRED]
IBase* __fastcall createVideoTarget(unsigned int iid, IBase* outer);

// One shared pointer per singleton registration.
IBase* g_QuadDrawer             = 0;
IBase* g_DebugRenderer          = 0;
IBase* g_DebugLineDrawer        = 0;
IBase* g_DebugCircleDrawer      = 0;
IBase* g_DebugTextDrawer        = 0;
IBase* g_DebugLineGraphRenderer = 0;
IBase* g_DebugTextWriter        = 0;
IBase* g_DebugTextWriterFixed   = 0;
IBase* g_DebugTextWriterManager = 0;
IBase* g_FontSystem             = 0;
IBase* g_FontRenderer           = 0;

RD9_MODULE_IBASE_BODIES(ModuleGeomMisc)


// Appends geometry-utility class registrations, including the shared debug-text writer implementation.

void ModuleGeomMisc::getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::geom::ModuleGeomMisc::getClassList", 0x10);

    ::renddx9abi::HostVectorWriter<ClassRegInfo> out(hostVector);

    // Both DebugTextWriter singleton names use this class and interface ID.
    ClassRegInfo info;
    info.classId = CID_DebugTextWriter;             // 81041
    info.name    = "dice.hfe.geom.DebugTextWriter";
    info.create  = RD9_CLASS_FACTORY(&createDebugTextWriter);
    out.push_back(info);

    ClassRegInfo writerManager;
    writerManager.classId = 81042u;
    writerManager.name = "dice.hfe.geom.DebugTextWriterManager";
    writerManager.create = RD9_CLASS_FACTORY(&createDebugTextWriterManager);
    out.push_back(writerManager);

    // Font implementation used by the console overlay.
    ClassRegInfo font;
    font.classId = CID_FontPorted;                  // 81007
    font.name    = "dice.hfe.geom.Font.Ported";
    font.create  = RD9_CLASS_FACTORY(&createFontPorted);
    out.push_back(font);

    // OverlayTexture - the second half of initializeFontAndConsoleOverlay's pair. Its name and
    // classId are the ones getClassList uses at 0x1007D858 and 0x1007D866.
    ClassRegInfo overlay;
    overlay.classId = CID_OverlayTexture;           // 81008
    overlay.name    = "dice.hfe.geom.OverlayTexture";
    overlay.create  = RD9_CLASS_FACTORY(&createOverlayTexture);
    out.push_back(overlay);

    // Registers the graphs created by Renderer::initialize.
    ClassRegInfo graph;
    graph.classId = CID_DebugLineGraph;             // 81038
    graph.name    = "dice.hfe.geom.DebugLineGraph";
    graph.create  = RD9_CLASS_FACTORY(&createDebugLineGraph);
    out.push_back(graph);

    // HUD construction requires Meme.Texture; its factory result is used without a null check.
    ClassRegInfo meme;
    meme.classId = CID_MemeTexture;                 // 81032
    meme.name    = "dice.hfe.geom.Meme.Texture";
    meme.create  = RD9_CLASS_FACTORY(&createMemeTexture);
    out.push_back(meme);

    // Movie-player factory used during client startup; unavailable video loading reports failure.
    ClassRegInfo movie;
    movie.classId = CID_FSMoviePlayer;              // 81046, dword_1020AF2C
    movie.name    = "dice.hfe.geom.FSMoviePlayer";
    movie.create  = RD9_CLASS_FACTORY(&createFSMoviePlayer);
    out.push_back(movie);

    // Menu texture batch renderer published through g_QuadDrawer.
    ClassRegInfo quad;
    quad.classId = CID_QuadDrawer;                  // 81031, dword_1020AEF0
    quad.name    = "dice.hfe.geom.Meme.QuadDrawer";
    quad.create  = RD9_CLASS_FACTORY(&createQuadDrawer);
    out.push_back(quad);

    // Editor spline and road-noise factories; created on demand rather than as singletons.
    ClassRegInfo spline;
    spline.classId = CID_EditorSpline;              // 81033
    spline.name    = "dice.hfe.geom.EditorSpline";
    spline.create  = RD9_CLASS_FACTORY(&createEditorSpline);
    out.push_back(spline);

    ClassRegInfo noise;
    noise.classId = CID_PerlinNoise;                // 81035
    noise.name    = "dice.hfe.geom.PerlinNoise";
    noise.create  = RD9_CLASS_FACTORY(&createPerlinNoise);
    out.push_back(noise);

    // Registers font classes; GPU resources are acquired when an instance first needs them.
    // [INFERRED] Fully qualified class names.
    ClassRegInfo fontSys;
    fontSys.classId = CID_FontSystem;               // 81044
    fontSys.name    = "dice.hfe.geom.FontSystem";
    fontSys.create  = RD9_CLASS_FACTORY(&createFontSystem);
    out.push_back(fontSys);

    ClassRegInfo fontRend;
    fontRend.classId = CID_FontRenderer;            // 81047
    fontRend.name    = "dice.hfe.geom.FontRenderer";
    fontRend.create  = RD9_CLASS_FACTORY(&createFontRenderer);
    out.push_back(fontRend);

    // Video + VideoTarget - the Bink menu-movie pair (RW-105 covers open()'s fallback pass). Registering them makes
    // createInstanceByName("dice.hfe.geom.Video") resolve, which is how FSMoviePlayer (intro movies)
    // and SwiffRender (the menu background '?' fill) obtain a player. [INFERRED] cids.
    ClassRegInfo video;
    video.classId = CID_Video;                      // 81048
    video.name    = "dice.hfe.geom.Video";
    video.create  = RD9_CLASS_FACTORY(&createVideo);
    out.push_back(video);

    ClassRegInfo videoTarget;
    videoTarget.classId = CID_VideoTarget;          // 81049
    videoTarget.name    = "dice.hfe.geom.VideoTarget";
    videoTarget.create  = RD9_CLASS_FACTORY(&createVideoTarget);
    out.push_back(videoTarget);

    ::renddx9abi::logf("ModuleGeomMisc::getClassList: 11 classes registered "
                       "(dice.hfe.geom.DebugTextWriter cid 81041, "
                       "dice.hfe.geom.Font.Ported cid 81007, "
                       "dice.hfe.geom.OverlayTexture cid 81008, "
                       "dice.hfe.geom.DebugLineGraph cid 81038, "
                       "dice.hfe.geom.Meme.Texture cid 81032, "
                       "dice.hfe.geom.FSMoviePlayer cid 81046, "
                       "dice.hfe.geom.Meme.QuadDrawer cid 81031, "
                       "dice.hfe.geom.FontSystem cid 81044, "
                       "dice.hfe.geom.FontRenderer cid 81047, "
                       "dice.hfe.geom.Video cid 81048, "
                       "dice.hfe.geom.VideoTarget cid 81049)");
    // The line above predates roads step 2 and still counts eleven; these two make thirteen.
    ::renddx9abi::logf("ModuleGeomMisc::getClassList: +2 road-editor classes registered "
                       "(dice.hfe.geom.EditorSpline cid 81033, dice.hfe.geom.PerlinNoise cid 81035)");
}


// Appends geometry-utility singleton registrations. Proportional and fixed debug text use separate instances of one class.

void ModuleGeomMisc::getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::geom::ModuleGeomMisc::getSingletonList", 0x14);

    struct Entry { const char* name; IBase** instance; unsigned int iid; unsigned int classId; };
    static const Entry entries[11] =
    {
        { "QuadDrawer",             &g_QuadDrawer,             80031u, 81031u },
        { "DebugRenderer",          &g_DebugRenderer,          80034u, 81034u },
        { "DebugLineDrawer",        &g_DebugLineDrawer,        80013u, 81021u },
        { "DebugCircleDrawer",      &g_DebugCircleDrawer,      80014u, 81022u },
        { "DebugTextDrawer",        &g_DebugTextDrawer,        80038u, 81037u },
        { "DebugLineGraphRenderer", &g_DebugLineGraphRenderer, 80040u, 81039u },
        { "DebugTextWriterManager", &g_DebugTextWriterManager, 80044u, 81042u },
        { "DebugTextWriter",        &g_DebugTextWriter,        80043u, 81041u },
        { "DebugTextWriterFixed",   &g_DebugTextWriterFixed,   80043u, 81041u },
        { "FontSystem",             &g_FontSystem,             80045u, 81044u },
        { "FontRenderer",           &g_FontRenderer,           80048u, 81047u },
    };

    ::renddx9abi::HostVectorWriter<SingletonRegInfo> out(hostVector);

    for (int i = 0; i < 11; ++i)
    {
        SingletonRegInfo info;
        info.name     = entries[i].name;
        info.instance = entries[i].instance;
        info.iid      = entries[i].iid;
        info.classId  = entries[i].classId;
        info.flags    = 2;
        out.push_back(info);
    }
}

bool ModuleGeomMisc::init()
{
    RD9_TRACE("dice::hfe::geom::ModuleGeomMisc::init", 0x18);

    // [CHOSEN, not recovered] [FRONTIER] Generated console bindings are withheld.
    // [RETIRE-WHEN: renddx9-geommisc-console-list] Implement and validate the original binding list before publication.
    ::renddx9abi::logf("[FRONTIER] dice::hfe::geom::ModuleGeomMisc::init: "
                       "TU console list 0x10248B18 "
                       "is not registered (PE call 0x1007B570 withheld)");

    // The remainder of the shipped body allocates 0xE4 bytes, constructs FSQuadDrawer and stores
    // it in dword_10249978.  initializeFSQuadDrawer preserves the null-on-allocation-failure arm.
    initializeFSQuadDrawer();
    return true;
}

bool ModuleGeomMisc::close()
{
    RD9_TRACE("dice::hfe::geom::ModuleGeomMisc::close", 0x1C);

    // [CHOSEN, not recovered] [RETIRE-WHEN: renddx9-geommisc-console-list]
    // [FRONTIER] The console list was not published, so close must not unregister an invented list.
    ::renddx9abi::logf("[FRONTIER] dice::hfe::geom::ModuleGeomMisc::close: "
                       "TU console list 0x10248B18 "
                       "was not unregistered (PE call 0x1007B6D0 withheld)");

    // Shipped order after unregister: FSQuadDrawer::~FSQuadDrawer, then engineFree.  Retail leaves
    // dword_10249978 holding the freed address; destroyFSQuadDrawer deliberately preserves that.
    destroyFSQuadDrawer();
    return true;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
