// The editor's HUD (+hud 1) and its HUD editor - added: the retail editor left HUD scripts to DICE's MEME editor.
// The HUD is the game's own (BF2HudManager, Menu/HUD/HudSetup), drawn by the renderer over the view: in the play
// test as the client draws it, and in the HUD editor's preview with the selected part's show variables held so it
// appears without a player. Edits change the scripts (Framework/HudSetup, through the undo history) and, where the
// builder can take them again safely, the live HUD at once.
#ifndef BF2EDITOR_APP_HUDEDITOR_H
#define BF2EDITOR_APP_HUDEDITOR_H

#include "Framework/EditorConfig.h"
#include "App/EditSession.h"
#include "Framework/EditorConsole.h"
#include "Framework/HudSetup.h"

#include <map>
#include <string>
#include <vector>

namespace bf2editor
{

// [CHOSEN, not recovered] The editor's HUD and HUD editor are added (issue draft 16); the HUD itself is the game's.
class HudEditor : public IHudCommands, public ISavedDocument
{
public:
    explicit HudEditor(EditSession& session);
    ~HudEditor();

    // Once a frame after the world update. `inGame`: the play test's soldier is up. Decides whether the HUD is drawn
    // (renderer.drawHud) and in which mode, holds the preview's variables, and runs the HUD's widget update when no
    // player drives it (BF2HudManager::update does only for a local player in a vehicle).
    void update(float seconds, bool inGame);
    bool previewing() const { return m_preview; }

    // The "HUD editor" window and, while the preview is on, the selected part's outline over the view: dragging it
    // moves it, its corner resizes it. `view` is the 3D view's rectangle (x, y, width, height) in window pixels.
    void drawPanel(bool* open, const float view[4], bool inGame);

    // The scripts as edited (Framework/HudSetup).
    const HudSetup& setup() const { return m_setup; }

    // ISavedDocument: the level's save writes the changed scripts into every archive mounted on their folder that holds
    // them (the game client reads Menu_client.zip; the editor mounts Menu_server.zip before it), each archive written
    // again with its other entries copied as stored. The engine lets go of those archives while they are replaced and
    // mounts them again, in their order, after. Only archives inside the open level's mod are written.
    void planSave(SavePlan& plan) override;
    std::string unsavedText() const override;

    // IHudCommands (EditorHud.*).
    std::string describeHud() const override;
    bool setHudPreview(bool on, std::string& result) override;
    bool setHudVariable(const std::string& name, const std::string& value, std::string& result) override;
    bool selectHudNode(const std::string& node, std::string& result) override;
    std::string describeHudNode(const std::string& node) const override;
    bool editHudLine(int op, int script, int line, const std::string& text, std::string& result) override;
    bool setHudRect(const std::string& node, int x, int y, int width, int height, std::string& result) override;
    bool setHudRow(const std::string& node, int row, const std::string& words, std::string& result) override;
    bool addHudRow(const std::string& node, const std::string& method, const std::string& words,
                   std::string& result) override;
    bool removeHudRow(const std::string& node, int row, std::string& result) override;
    std::string describeHudChanges() const override;
    bool hudScreenPoint(const std::string& node, int which, float& x, float& y, std::string& error) const override;

private:
    // A HUD variable the preview holds at a value, written each frame (the game's own updates may move it).
    struct HeldVariable
    {
        std::string name;
        int         type;       // 0 int, 1 bool, 2 float
        int         intValue;
        float       floatValue;
    };

    bool load(std::string& error);
    void select(int node);
    bool resolveHeld(const std::string& name, const std::string& types, const std::string& value, HeldVariable& out) const;
    void applyHeld();
    void restoreAutoHeld();
    // An edit of one line through the undo history: `doScript`/`undoScript` are EditorHud.*Line rows.
    bool runEdit(const std::string& label, const std::string& doScript, const std::string& undoScript,
                 std::string& result);
    // The live HUD given a script line again, where that is safe; `note` says what happened.
    void applyLive(size_t script, size_t line, std::string& note);
    bool applyLiveRect(size_t node, const int rect[4]);
    // The node's rectangle on the 800x600 HUD canvas: its own position plus its parents'. False without one.
    bool canvasRect(size_t node, float out[4]) const;
    bool rowText(const HudRef& ref, std::string& words) const;
    void drawOverlay(const float view[4]);
    void drawTree(const std::string& parent, int depth);

    EditSession&              m_session;
    HudSetup                  m_setup;
    std::string               m_loadError;
    std::vector<HeldVariable> m_held;           // EditorHud.set: held while previewing
    std::vector<HeldVariable> m_autoHeld;       // what shows the selected part
    std::map<std::string, HeldVariable> m_saved; // the values the auto-held variables had before
    bool                      m_preview = false;
    bool                      m_inGame = false;
    int                       m_drawn = -1;     // the renderer.drawHud value last set; -1 before the first update
    int                       m_selected = -1;
    std::string               m_lastNote;       // what the last edit did on the live HUD
    // The panel's state.
    char                      m_filter[64] = {};
    char                      m_addMethod[64] = {};
    char                      m_addWords[256] = {};
    char                      m_driveTemplate[96] = {};
    std::vector<std::string>  m_rowEdits;       // the selected node's rows as typed, one buffer each
    unsigned                  m_rowEditsFor = 0; // the edit generation the buffers were filled for
    unsigned                  m_generation = 1; // bumped by every edit and selection
    int                       m_rectEdit[4] = {};
    // A drag in the view: moving (1) or resizing (2) the selected part from where it stood.
    int                       m_dragMode = 0;
    int                       m_dragStart[4] = {};
    float                     m_dragMouse[2] = {};
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_HUDEDITOR_H
