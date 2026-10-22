// Level-editing panels drawn while an EditSession is attached.
// Property drags preview live and commit one undoable edit on release. Console commands with unknown
// effects clear undo history. Shared object-template properties remain read-only outside their editor.
// The panels expose selection, history, layers and the specialized level tools. Editor-only.
#ifndef BF2EDITOR_APP_SESSIONPANELS_H
#define BF2EDITOR_APP_SESSIONPANELS_H

#include "Framework/EditorConfig.h"

#include "App/LibraryPanel.h"
#include "App/ObjectTemplatePanel.h"
#include "App/TerrainPanel.h"
#include "App/RoadPanel.h"
#include "App/AnimationPanel.h"
#include "App/MaterialPanel.h"
#include "App/WizardPanel.h"
#include "Math/Mat4.h"

#include <deque>
#include <string>
#include <vector>

namespace bf2editor
{

class EditSession;
class EditorSettings;
class LevelDocument;

class SessionPanels
{
public:
    SessionPanels();

    void draw(EditSession& session);
    // `resourceTemplate`: the template of the file selected in the Resources panel ("" for none), for
    // Replace.
    void editMenu(EditSession& session, const std::string& resourceTemplate);
    void viewMenuItems();

    void loadSettings(const EditorSettings& settings);
    void saveSettings(EditorSettings& settings) const;
    bool setPanelVisible(const std::string& name, bool visible);    // false: not one of these panels
    std::vector<std::string> openPanels() const;                     // the window titles of those open

    // Runs one console line as the Console panel does, and shows it there. `origin` prefixes the
    // log line ("console", "script"). False when an editor command refused.
    bool submit(EditSession& session, const std::string& line, const char* origin);

private:
    void drawSelection(EditSession& session);
    void drawObjects(EditSession& session);
    void drawHistory(EditSession& session);
    void drawConsole(EditSession& session);
    void drawLayers(EditSession& session);
    void drawProperties(EditSession& session);
    void drawCombatAreas(EditSession& session);
    void drawSounds(EditSession& session);
    void drawLevelSettings(EditSession& session);
    void drawGameplayLayers(EditSession& session);
    // The mod manager's pages: the mods, the open mod's mod.desc and the level's
    // Info/<level>.desc fields.
    void drawMod(EditSession& session);
    // The template's block from the file that defines it (ObjectTemplateManager::getWorkingFolderAndFile),
    // read once per template.
    void loadTemplateLines(const std::string& templateName);
    void drawLevelTemplate(EditSession& session, LevelDocument& document, size_t file, const std::string& name);
    void refreshObjects();
    // The "New gameplay object" dialog: a name for the new spawner, spawn point or control point, made
    // under the middle of the view.
    void drawNewGameplay(EditSession& session);
    // Field edits in the Selection panel: 0 none, 1 position, 2 rotation.
    void finishFieldEdit(EditSession& session);

    bool m_showSelection;
    bool m_showObjects;
    bool m_showHistory;
    bool m_showConsole;
    bool m_showLayers;
    bool m_showProperties;
    bool m_showCombatAreas;
    bool m_showSounds;
    bool m_showLevelSettings;
    bool m_showGameplayLayers;
    bool m_showTerrain;
    TerrainPanel m_terrain;
    bool m_showRoads = false;
    RoadPanel m_roadPanel;
    bool m_showLibrary;
    LibraryPanel m_library;
    bool m_showObjectTemplates;
    ObjectTemplatePanel m_objectTemplates;
    bool m_showWizard;
    WizardPanel m_wizardPanel;
    bool m_showMaterials;
    MaterialPanel m_materialPanel;
    bool m_showAnimations;
    AnimationPanel m_animationPanel;
    bool m_showMod;
    std::string m_descriptionEditing;   // "mod.title": the field whose buffer holds the user's typing
    char m_descriptionBuffer[1024];
    bool m_confirmSnapAll;          // Edit > Snap all objects to the ground: the confirmation is open
    char m_newSoundName[96];
    char m_newSoundFile[256];
    char  m_newAreaName[96];
    float m_newAreaSize;
    std::string              m_templateName;    // whose lines m_templateLines holds
    std::string              m_templateFile;
    std::vector<std::string> m_templateLines;
    char                     m_newProperty[64];
    char                     m_newValue[192];
    char                     m_renameBuffer[128];   // the Properties panel's new name for a level template
    std::string              m_renameFor;           // the template it was filled from
    int                      m_newGameplayKind;     // index into the dialog's kinds; -1 when closed
    bool                     m_openNewGameplay;     // open the dialog this frame
    char                     m_newGameplayName[128];
    char                     m_newGameplayEffect[128];  // an effect area's effect
    std::string              m_newGameplayError;
    int  m_targetLayer;         // the Layers panel's "move the selection to" field
    // The retail layer model's layer being edited in the Layers panel (E14), its name and filter boxes.
    int               m_layerEditing = 0;
    char              m_layerName[96] = "";
    char              m_newLayerName[96] = "";
    std::vector<char> m_layerModes;
    std::vector<char> m_layerPlayers;

    int             m_field;
    unsigned        m_fieldId;
    dice::hfe::Mat4 m_fieldStart;

    struct ObjectRow
    {
        unsigned    id;
        std::string label;
        std::string lowered;    // for the filter
        unsigned    classId;
        std::string className;
        int         layer;
    };
    std::vector<ObjectRow> m_rows;
    bool                   m_rowsStale;
    unsigned               m_rowsRevision;  // the history revision the rows were read at
    char                   m_filter[96];
    unsigned               m_classFilter;   // 0: every class
    char                   m_createTemplate[96];

    struct ConsoleLine
    {
        std::string text;
        int         kind;       // 0 typed, 1 answer, 2 error, 3 note
    };
    std::deque<ConsoleLine>  m_consoleLines;
    static const int         kKeptConsoleLines = 50;    // typed lines kept in the settings file
    std::vector<std::string> m_typed;       // for Up / Down
    int                      m_typedCursor;
    char                     m_input[512];
    bool                     m_scrollConsole;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_SESSIONPANELS_H
