// Code/Tools/BF2Editor/App/SavePreviewPanel.h

// The save preview and the Backups window. File > Save (Ctrl+S) shows what the save writes before anything is
// written: each file with what it replaces, and the line diff of a text file; Write writes them all or none. File >
// Backups lists the level's backups (each holds what one save replaced) and puts the level back to before any of them.
// [CHOSEN, not recovered]: the retail editor saved at once and kept no backup (issue draft 13). Editor-only code (D3).
#ifndef BF2EDITOR_APP_SAVEPREVIEWPANEL_H
#define BF2EDITOR_APP_SAVEPREVIEWPANEL_H

#include "Framework/EditorConfig.h"

#include "Framework/SavePlan.h"

#include <string>
#include <vector>

namespace bf2editor
{

class EditSession;

class SavePreviewPanel
{
public:
    // The preview on the next draw (File > Preview the save, or a save with the preview option on).
    void requestPreview() { m_requested = true; }
    void openBackups()
    {
        m_showBackups = true;
        m_refreshBackups = true;
    }
    // Each frame with a level open. `previewOption` is the option the preview's checkbox shows and changes.
    void draw(EditSession& session, bool& previewOption);

private:
    void drawPreview(EditSession& session, bool& previewOption);
    void drawBackups(EditSession& session);
    void selectFile(EditSession& session, int index);

    bool                    m_requested = false;
    std::string             m_summary;          // the preview's answer: why it could not be made, when it was not
    bool                    m_planned = false;
    int                     m_selected = -1;
    std::vector<std::string> m_diff;            // the selected file's diff, a line each
    bool                    m_showBackups = false;
    bool                    m_refreshBackups = false;
    std::vector<SaveBackup> m_backups;
    int                     m_backupSelected = -1;
    bool                    m_confirmRestore = false;
    std::string             m_backupResult;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_SAVEPREVIEWPANEL_H
