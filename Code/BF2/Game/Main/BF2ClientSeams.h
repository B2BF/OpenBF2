// [CHOSEN, not recovered] Client-only boundaries for interfaces not yet available here.
// Known-signature frontiers diagnose and stop at use.
// [RETIRE-WHEN: client-seams] Replace each seam and its diagnostic body with a typed
// call when the owning class and interface are established.

#ifndef GAME_MAIN_BF2CLIENTSEAMS_H            // [CHOSEN] guard
#define GAME_MAIN_BF2CLIENTSEAMS_H

#include "../../Core/BuildConfig.h"

#if !DEDICATED

#include <string>
#include <vector>

namespace renddx9abi { struct HostDisplayMode; }

namespace dice {
namespace hfe {

class IBase;
class BF2Engine;
class BF2Window;
class DemoRecorder;
namespace io { class GameInput; class PlayerInput; class IControlMap; }
namespace dx9rend { class IRenderer; }

// Client renderer facade; preserve the complete virtual surface.
class BF2Render
{
public:
    explicit BF2Render(BF2Engine* engine);           // BF2.exe `BF2Render_ctor`
    virtual ~BF2Render();                            // slot 0
    virtual bool init();                             // +0x04    written
    virtual bool rendererSlot0x14();                 // +0x08  -> Renderer10009DD0, AL=1
    virtual bool rendererSlot0x18();                 // +0x0C  -> Renderer10009DE0, AL=1
    // +0x10  , the image's shared one-byte `ret`: written as the empty body it is. No stack
    //        arguments (a bare `ret`), no BF2.exe caller found. [UNRESOLVED] name.
    virtual void slot0x10();
    virtual void update(double frameTime);           // +0x14
    virtual bool render(double frameTime, DemoRecorder* recorder); // +0x18
    virtual void beforeFrame();                      // +0x1C    [INFERRED] role
    // Requests a fullscreen toggle. [UNRESOLVED] Original return type; callers discard the result.
    virtual unsigned int toggleFullscreen();
    // [INFERRED name] Lists screen modes for the renderer console command.
    virtual void listScreenModes();
    virtual int  getDisplayModeIndex() const;        // +0x28    `renderer.screenMode` GET (0x428DFD)
    // Selects the requested display mode; out-of-range indices are ignored.
    virtual void setDisplayMode(int index);
    virtual IBase* getRenderView();                  // +0x30
    virtual IBase* getPostProductionFrameWork();     // +0x34

    // Inlined field read in DirectInput fallback callers (e.g. PE 0x6ADA80).
    BF2Window* getWindow() const { return m_window; }
    // Inlined +0x10 read in every VideoSettings body that reaches the renderer's options
    // (PE 4B1DB0..4B2810: `mov eax, g_bf2Render; mov ecx, [eax+10h]`).
    dx9rend::IRenderer* getRenderer() const { return m_renderer; }

private:
    void shutdown();

    BF2Engine* m_engine;                             // +0x04
    BF2Window* m_window;                             // +0x08
    void*      m_rendererDll;                        // +0x0C
    dx9rend::IRenderer* m_renderer;                  // +0x10
    IBase*     m_postProductionFrameWork;            // +0x14
    renddx9abi::HostDisplayMode* m_displayModeCarrier; // +0x18; selected in RendDX9Abi
    int        m_displayModeIndex;                   // +0x1C; constructed as -1
};
#if defined(_M_IX86)
static_assert(sizeof(BF2Render) == 0x20, "BF2Render must retain the retail x86 allocation size");
#endif

namespace hud {
class BF2HudManager; // Menu/BF2HudManager.h: recovered construction, incomplete virtual surface
} // namespace hud

// The voice client (BF2.exe vtable 0x89C1A8): Game/VoipClient/BF2VoipClient.h. Callers include that
// header; this one only names the type for the singleton slot below.
class BF2VoipClient;

// Globals the client image owns. Defined in Game/Main/BF2Engine.cpp ([CHOSEN] placement: their
// .data neighbours are that TU's statics - see the definitions).
extern hud::BF2HudManager* hudManager;               // ds:0xA10890 - the "HudManager" singleton slot
extern BF2VoipClient*      bf2VoipClient;            // ds:0x9A4DA0 - the "Bf2VoipClient" singleton slot
extern BF2Render*          g_bf2Render;              // ds:0x9A5580 - published by BF2Render ctor
extern IBase*              g_geometryTemplateManager;// ds:0x9A5578 - 20 PE readers after init
extern IBase*              g_renderer;               // BF2.exe `g_Renderer` - [UNRESOLVED] symbol name
extern IBase*&             unresolvedGlobalA33C08;   // ds:0xA33C08 = ai::g_aiInstance (BF2.exe 0x801030,
                                                     //   AIInstanceServer::updateInstance); bf2_w32ded ds:0x8B25B0

// The client's singleton slots (BF2.exe 0x4301A9 registers them; ds:0x987340..0x987398). The
// first two rows pass IID 1 and flags 1, transcribed as-is in Core/ModuleRegistry.cpp.
extern IBase* g_hudInformationLayer;                 // ds:0x98734C  "HudInformationLayer"    CID 121008
extern IBase* g_distributedLightRenderer;            // ds:0x987350  "DistributedLightRenderer" CID 121009
extern IBase* g_localProfileManager;                 // ds:0x987364  "LocalProfileManager"
extern IBase* g_profileManager;                      // ds:0x987360  "ProfileManager"
extern IBase* g_rankManager;                         // ds:0x987368  "RankManager"
extern IBase* g_clanManager;                         // ds:0x98736C  "ClanManager"
extern IBase* g_newsManager;                         // ds:0x987370  "NewsManager"
extern IBase* g_autoPatch;                           // ds:0x987374  "AutoPatch"
extern IBase* g_persistence;                         // ds:0x987378  "Persistence"
extern IBase* g_menuTeamManager;                     // ds:0x98737C  "MenuTeamManager"
extern IBase* g_globalSettings;                      // ds:0x987380  "GlobalSettings"
extern IBase* g_videoSettings;                       // ds:0x987388  "VideoSettings"
extern IBase* g_audioSettings;                       // ds:0x987384  "AudioSettings"
extern IBase* g_controlSettings;                     // ds:0x98738C  "ControlSettings"
extern IBase* g_generalSettings;                     // ds:0x987390  "GeneralSettings"
extern IBase* g_hapticSettings;                      // ds:0x987394  "HapticSettings"
extern IBase* g_gameServerSettings;                  // ds:0x987354  "GameServerSettings" - the row that
                                                     //   leads 0x4301A9 (IID 121047, CID 121048, flags 2);
                                                     //   read by LocalProfileManager::createDefaultProfile.
                                                     //   [UNRESOLVED] which module's list registers it -
                                                     //   the slot exists, the registry row is not added.
extern IBase* g_serverBrowserManager;                // ds:0x987344  "ServerBrowserManager"
extern IBase* g_swiffHost;                           // ds:0x98735C  "SwiffHost"
extern IBase* g_demoLibrary;                         // ds:0x987348  "DemoLibrary"
extern IBase* g_serverLogoManager;                   // ds:0x987340  "ServerLogoManager"

// Remaining client seams use role-based names where known and vslot names where
// [UNRESOLVED]. Slot comments describe the PE ABI; typed interfaces own other services.
bool        modInfo_vslot0x30(IBase* modInfo, const std::string& modName);      // IModInfo +0x30   startGame join result 8: getModFromPath != null
bool        unresolvedGlobalA33C08_vslot0x18();                                 // +0x18   mainLoop (ragdoll viewer), 0x40D227: IAIInstance::isInitialized

} // namespace hfe
} // namespace dice

#endif // !dedicated
#endif // GAME_MAIN_BF2CLIENTSEAMS_H
