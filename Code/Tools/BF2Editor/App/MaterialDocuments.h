// Code/Tools/BF2Editor/App/MaterialDocuments.h

// The material editor's two files: Common/Material/materialManagerDefine.con and materialManagerSettings.con, read through the engine's
// own file manager - so exactly what the engine ran when the level loaded - and kept as Framework/MaterialFiles
// documents, edited in place. As with the object templates (App/TemplateDocuments), a file read from a mounted
// archive is read-only: the stock files sit in Common_server.zip and Common_client.zip, which shadow any loose
// copy, so they are edited with the Common archives extracted and left unmounted (+looseCommon 1). A save
// plans the loose path the engine would write (FileManager::getWritePath: the mod's own folder), CRLF as the
// engine's text-mode writes leave it; Framework/SavePlan writes it through source control, atomically, and
// verifies it. Editor-only code (D3).
#ifndef BF2EDITOR_APP_MATERIALDOCUMENTS_H
#define BF2EDITOR_APP_MATERIALDOCUMENTS_H

#include "Framework/EditorConfig.h"

#include "Framework/MaterialFiles.h"

#include <string>

namespace bf2editor
{

class SavePart;

class MaterialDocuments
{
public:
    MaterialDocuments();

    // Both files read through the engine (once; clear() forgets them). False with `error` when the engine
    // has neither.
    bool load(std::string& error);
    bool loaded() const { return m_loaded; }
    void clear();

    MaterialDefineFile& defines() { return m_defineFile; }
    MaterialCellFile& cells() { return m_cellFile; }
    const MaterialDefineFile& defines() const { return m_defineFile; }
    const MaterialCellFile& cells() const { return m_cellFile; }

    const std::string& definePath() const { return m_define.path; }
    const std::string& cellPath() const { return m_cells.path; }
    const std::string& defineWritePath() const { return m_define.writePath; }
    const std::string& cellWritePath() const { return m_cells.writePath; }
    // Either file read from a mounted archive (so neither can be saved where the game would read it).
    bool archived() const { return m_define.archived || m_cells.archived; }
    bool modified() const;
    // The modified files into `part`, taken as saved once written; nothing when none is. The part is refused
    // (false) while the files are archived.
    bool planSave(SavePart& part);

private:
    struct File
    {
        std::string path;
        std::string writePath;
        std::string savedText;
        bool        exists;
        bool        archived;
        File() : exists(false), archived(false) {}
    };
    bool read(File& file, const std::string& path, std::string& text);

    File               m_define;
    File               m_cells;
    MaterialDefineFile m_defineFile;
    MaterialCellFile   m_cellFile;
    bool               m_loaded;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_MATERIALDOCUMENTS_H
