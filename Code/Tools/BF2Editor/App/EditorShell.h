// Code/Tools/BF2Editor/App/EditorShell.h

// The editor's ImGui shell (WP4): the dockspace over the main window (its central node is left
// transparent, so the engine's 3D view shows through), the main menu and the always-available panels.
// Editing modules add their own panels later. Editor-only code (D3).
#ifndef BF2EDITOR_APP_EDITORSHELL_H
#define BF2EDITOR_APP_EDITORSHELL_H

#include "Framework/EditorConfig.h"

#include "App/FrameProfile.h"
#include "App/MinimapPanel.h"
#include "App/ResourceBrowser.h"
#include "App/SavePreviewPanel.h"
#include "App/SessionPanels.h"

#include <string>
#include <vector>

namespace bf2editor
{

class HudEditor;

// What the shell shows about the running editor, filled in by EditorApp each frame.
struct ShellStatus
{
    std::string mod;            // e.g. mods/bf2
    std::string level;          // "" when no level is open
    bool        archives;
    std::string renderer;       // one line about the device, "" before the renderer is up
    std::string camera;         // the free camera, "" with no level open
    float       frameMs;        // the last frame's duration
    unsigned    frame;          // frames since the loop started

    ShellStatus() : archives(false), frameMs(0.0f), frame(0) {}
};

class EditSession;
class EditorSettings;

// Tools > Options: what the editor keeps between runs
// besides its panels and keys.
struct ShellOptions
{
    bool        rememberCamera;     // a level opens where its camera was left, else at the default start
    float       viewDistance;       // the camera's far plane, metres
    std::string startupCommand;     // a console line run once a level has opened
    std::string rawDataPath;        // where the import file dialogs start (terrain rasters, meshes)
    std::string packFolder;         // the mod folder File > Pack the level writes into (<game>/mods/<mod>)
    int         style;              // the colours: 0 dark, 1 light, 2 classic
    // The retail host's custom commands (Tools > Custom command / Custom script 1..3): a console line, and a
    // script file whose lines run one after another.
    std::string customCommands[3];
    std::string customFiles[3];
    // The rest of the retail host's options: the log to a file beside the editor, the mouse
    // look sensitivity, and the editor idle while another window has the focus.
    bool        logToFile;
    float       mouseSensitivity;
    bool        pauseOnFocusLoss;
    // The retail host's volume popup: the sound engine's master volume, 0..100, and sound on or off
    // (retail kept the volume as InitSettings.SetMasterVolume in bf2editor/Settings/EngineInit.con; here it is kept
    // with the other options). The editor runs with +noSound 1, so both wait for it.
    int         masterVolume = 100;
    bool        soundOn = true;
    // [CHOSEN, not recovered] (issue draft 13): File > Save shows what it writes first, and how many backups of its
    // saves a level keeps (0: every one).
    bool        previewSave = true;
    int         backupsKept = 10;
    ShellOptions() : rememberCamera(true), viewDistance(1500.0f), style(0), logToFile(false), mouseSensitivity(1.0f),
                     pauseOnFocusLoss(false) {}
};

class EditorShell
{
public:
    EditorShell();

    // Builds this frame's UI. Call between ImGuiHost::beginFrame and endFrame.
    void draw(const ShellStatus& status);

    // The user chose File > Exit.
    bool quitRequested() const { return m_quit; }
    // File > Gameplay layer: the level's layers (a switch starts the editor again on one) and copies of
    // the open one as a new size of its mode.
    void drawLayerMenu();
    void clearQuit() { m_quit = false; }

    // View > Pause the renderer: the world is not drawn while set (the view keeps its last frame); Render
    // one frame draws one more. Also EditorView.pauseRenderer / renderOneFrame.
    bool renderPaused() const { return m_renderPaused; }
    void setRenderPaused(bool paused);
    // View > Pause the game update: the frame-update objects (effects, animation) stop
    // while the view keeps drawing; events and the object grid keep running, so edits still show.
    bool updatePaused() const { return m_updatePaused; }
    void setUpdatePaused(bool paused) { m_updatePaused = paused; }
    // View > The level's view distance (V2): the camera's far plane from the level's
    // GameLogic.MaximumLevelViewDistance instead of Options.
    bool levelViewDistance() const { return m_levelViewDistance; }
    void setLevelViewDistance(bool level) { m_levelViewDistance = level; }
    // View > Wireframe: the world drawn as lines.
    bool wireframe() const { return m_wireframe; }
    void setWireframe(bool on) { m_wireframe = on; }
    void requestOneFrame() { m_renderOneFrame = true; }
    bool takeOneFrame() { const bool one = m_renderOneFrame; m_renderOneFrame = false; return one; }

    // Closing with unsaved changes (WP6): while `ask` is set, a modal asks what to do. `summary` says
    // what is unsaved. Returns the choice made this frame, None while nothing is chosen.
    enum class CloseChoice
    {
        None,
        Save,
        Discard,
        Cancel
    };
    CloseChoice drawClosePrompt(bool ask, const std::string& summary);

    ResourceBrowser& resources() { return m_resources; }
    MinimapPanel& minimap() { return m_minimap; }

    // With a level open: the Edit menu and the Selection, Objects, History and Console panels (WP5).
    void setSession(EditSession* session) { m_session = session; }
    // The HUD editor (+hud 1, App/HudEditor), once the HUD is up; 0 hides its panel and menu item.
    void setHudEditor(HudEditor* hud) { m_hud = hud; }
    SessionPanels& panels() { return m_panels; }
    // The Profiler panel's data; the frame loop adds one frame's phases each frame.
    FrameProfile& profile() { return m_profile; }

    // Which panels are open, in the editor's settings file (panels.<name> = 0|1).
    void loadSettings(const EditorSettings& settings);
    void saveSettings(EditorSettings& settings) const;

    // Opens or closes a panel by its title, case ignored (EditorView.panel); false for no such panel.
    // "all": 0 hides every open panel for a full view of the level (F11), 1 brings them back.
    bool setPanelVisible(const std::string& name, bool visible);
    const ShellOptions& options() const { return m_options; }
    // EditorTools.* (Framework/EditorConsole.h ToolsHandler): "list", "setCommand" / "setFile" <1..3> <text>,
    // "runCommand" / "runFile" <1..3>.
    bool toolsAction(const std::string& action, int index, const std::string& argument, std::string& result);

    // The dockspace's central node - the part of the window where the 3D view shows - as found by the
    // last draw(), in window pixels.
    float viewX() const { return m_view[0]; }
    float viewY() const { return m_view[1]; }
    float viewWidth() const { return m_view[2]; }
    float viewHeight() const { return m_view[3]; }

private:
    void drawMenu();
    void drawCameraMenu();
    void drawLog();
    void drawEngine(const ShellStatus& status);
    void drawAbout();
    void drawHelpMenu();
    void drawRecordPopup();
    void drawCatalogue();
    // Tools > Keyboard shortcuts: every action's key, changed by pressing a new one.
    void drawShortcuts();
    // Tools > Options.
    void drawOptions();
    // File > Pack the level for the game: the mod folder, then EditorLevel.pack.
    void drawPackPopup();
    // File > Save level as: the new name, then EditorLevel.saveAs.
    void drawSaveAsPopup();
    // File > New level: the retail Create new terrain dialog's values, then EditorLevel.create.
    void drawNewLevelPopup();
    // Compile > Generate lightmaps: the retail lightmap dialog's terrain options, then EditorLighting.lightmaps.
    void drawLightmapsPopup();
    // The Light menu (the retail host's light presets and the sun from the camera), and its save popup.
    void drawLightMenu();
    void drawLightPresetPopup();
    // Light > Lightmap light setups: the object or terrain setup's twelve settings.
    void drawLightSetups();
    bool m_showLightSetups = false;
    std::string m_lightSetupEditing;
    char m_lightSetupValue[64] = "";
    void applyStyle() const;

    ResourceBrowser m_resources;
    MinimapPanel    m_minimap;
    SessionPanels   m_panels;
    SavePreviewPanel m_savePreview;
    FrameProfile    m_profile;
    EditSession*    m_session;
    HudEditor*      m_hud = 0;
    float           m_view[4];
    bool m_showLog;
    bool m_showEngine;
    bool m_showResources;
    bool m_showProfiler;
    bool m_showMinimap;
    bool m_showHudEditor = false;
    bool m_renderPaused;
    bool m_updatePaused;
    bool m_levelViewDistance;
    bool m_wireframe = false;
    bool m_renderOneFrame;
    bool m_showDemo;
    bool m_showAbout;
    bool m_showCatalogue;           // Help > Retail command catalogue
    bool m_openRecord;              // Tools > Record the view
    char m_recordPath[400];
    char m_catalogueFilter[96];
    bool m_showShortcuts;
    bool m_showOptions;
    ShellOptions m_options;
    char m_startupBuffer[512];
    char m_rawDataBuffer[512];
    char m_packBuffer[512];
    char m_customBuffers[6][512];   // Options: the three custom commands, then the three custom scripts
    bool m_openLightPreset;
    char m_lightPresetName[80];
    std::string m_lightPresetResult;
    bool m_openPack;
    std::string m_packResult;
    bool m_openSaveAs;
    char m_saveAsName[80];
    bool m_saveAsOpen;
    std::string m_saveAsResult;
    bool m_openNewLevel = false;
    char m_newLevelName[80] = "";
    int  m_newLevelSize = 2;         // index into 256, 512, 1024
    int  m_newLevelScale = 1;        // index into 1, 2, 4
    int  m_newLevelHeight = 25;
    int  m_newLevelLowRes = 1;       // index into 128, 256, 512, 1024
    bool m_newLevelOpen = true;
    std::string m_newLevelResult;
    bool m_openLightmaps = false;
    int  m_lightmapQuality = 1;        // the retail dialog's resolutions: 0.25, 0.5, 1, 2
    bool m_lightmapPrimary = true;
    bool m_lightmapSecondary = true;
    bool m_lightmapTerrain = true;      // the terrain part (Primary / Secondary)
    bool m_lightmapObjects = false;     // the objects' part
    bool m_lightmapSelected = false;    // the objects' part: the selected ones only
    bool m_lightmapNearest = false;
    bool m_lightmapResume = false;
    std::string m_lightmapResult;
    std::string m_capturing;        // the action whose new key the Keyboard shortcuts window waits for
    int  m_captureCooldown;         // frames left in which shortcuts stay off after a capture
    bool m_quit;
    bool m_logAutoScroll;
    // The full view: the panels that were open when every panel was hidden.
    std::vector<std::string> m_hiddenForFullView;
    std::string m_focusRequest;     // a panel opened by name comes to the front on the next frame
    float m_cameraTarget[3];    // Camera > Set position
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_EDITORSHELL_H
