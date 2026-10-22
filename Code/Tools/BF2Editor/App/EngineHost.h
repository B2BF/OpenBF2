// Editor-specific engine startup using BF2Engine and BF2EngineSetup. Initializes required
// subsystems in order without invoking the game's menu/profile/HUD startup.
#ifndef BF2EDITOR_APP_ENGINEHOST_H
#define BF2EDITOR_APP_ENGINEHOST_H

#include "Framework/EditorConfig.h"

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

struct IDirect3DDevice9;
namespace dice { namespace hfe { class BF2Engine; namespace dx9rend { class IRenderer; } } }

namespace bf2editor
{

struct EngineConfig
{
    std::string mod;            // a folder under mods/, e.g. "bf2"
    bool        useArchives;    // mount the mod's ServerArchives.con / ClientArchives.con zips (read-only)
    bool        looseObjects;   // ... but not those mounted at Objects (the object editor's loose mode)
    bool        looseCommon;    // ... nor those mounted at Common (the material editor's loose mode)
    bool        ignoreAsserts;  // Vars "hack-ignore-asserts" = 1: an Assert is logged instead of exit(1)
    bool        hud = false;    // the game's HUD: its modules, the HudManager singleton and its init (needs a window)

    // The renderer (WP4). No window: the engine comes up without one, as a batch run needs.
    HWND        window;         // the editor window the renderer draws into
    int         width;          // its client size
    int         height;
    // The retail renderer switches (+skipMips, +deviceBehavior, ...) as the editor's command line
    // validated them. 1.50's parameter parser has none of them, so EngineHost writes the GS settings
    // RendDX9 reads. Without +deviceBehavior the editor asks for 64 (hardware vertex processing, not a
    // pure device): ImGui's DX9 backend reads device state back.
    std::vector<std::pair<std::string, std::string> > rendererOptions;

    EngineConfig() : useArchives(true), looseObjects(false), looseCommon(false), ignoreAsserts(false), window(0), width(0),
                     height(0) {}
};

class EngineHost
{
public:
    EngineHost();
    ~EngineHost();                  // shuts down what is up

    // Brings the engine up. On failure it logs the step that failed, undoes what came up and
    // returns false.
    bool boot(const EngineConfig& config);
    void shutdown();

    bool isUp() const { return m_phase == PhaseUp; }
    dice::hfe::BF2Engine* engine() const { return m_engine; }

    // The renderer, when boot() was given a window (0 otherwise).
    dice::hfe::dx9rend::IRenderer* renderer() const { return m_renderer; }
    IDirect3DDevice9* device() const { return m_device; }
    const std::string& rendererSummary() const { return m_rendererSummary; }

    // The game's HUD ([CHOSEN, not recovered]: the editor's own start-up runs the client's HUD steps): up when boot() was
    // asked for it and its init succeeded. hudLevelLoaded() is the HUD's BF2HudManager::onLevelLoaded. The renderer
    // updates and paints the HUD itself; App/HudEditor decides each frame whether it is drawn.
    bool hudUp() const { return m_hudUp; }
    void hudLevelLoaded();

private:
    EngineHost(const EngineHost&);
    EngineHost& operator=(const EngineHost&);

    bool fail(const std::string& step);
    bool startRenderer(const EngineConfig& config);
    bool m_hudUp = false;

    // How far boot() got, so shutdown() undoes exactly that.
    enum Phase
    {
        PhaseDown,      // nothing exists
        PhaseEngine,    // System + BF2Engine (+ its log and setup objects) exist
        PhaseModules,   // the module lists are registered
        PhaseUp         // everything boot() brings up
    };

    Phase                          m_phase;
    dice::hfe::BF2Engine*          m_engine;
    HWND                           m_window;       // the editor's, seated in a BF2Window; never ours to destroy
    dice::hfe::dx9rend::IRenderer* m_renderer;
    IDirect3DDevice9*              m_device;
    std::string                    m_rendererSummary;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_ENGINEHOST_H
