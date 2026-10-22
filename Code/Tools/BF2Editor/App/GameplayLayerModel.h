// Loads Editor/GamePlayObjects.con and Layer.con, or imports game-mode files that round-trip byte-for-byte.
// If the loaded mode cannot be represented, edits its file directly instead of enabling the layer model.
// Instantiates objects from other layers for editing. Saves editor metadata and represented mode buckets;
// unmatched imports remain untouched.
#ifndef BF2EDITOR_APP_GAMEPLAYLAYERMODEL_H
#define BF2EDITOR_APP_GAMEPLAYLAYERMODEL_H

#include "Framework/EditorConfig.h"

#include "Framework/GameplayLayers.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace bf2editor
{

class SavePart;

class GameplayLayerModel
{
public:
    // `levelDirectory`: the level's folder as it is spelled on disk, ending in '/'; `mod`: for LayerInit.con.
    bool open(const std::string& levelDirectory, const std::string& mod, const std::string& gameMode, int layerSize,
              std::string& report);
    void close();
    bool active() const { return m_active; }
    bool editorFileExists() const { return m_editorFileExists; }
    const std::string& editorText() const { return m_editorText; }
    const std::string& editorPath() const { return m_editorPath; }     // on disk (it may not exist yet)
    // The lines that make the objects the engine has not got (`present` answers for a template's or an area's name).
    std::vector<std::string> missingObjectLines(const std::function<bool(const std::string&, bool)>& present) const;
    std::vector<GameplayLayer>& layers() { return m_layers; }
    const std::vector<GameplayLayer>& layers() const { return m_layers; }
    const GameplayLayer* layer(int id) const;
    const GameplayFilters& filters() const { return m_filters; }
    const std::vector<std::pair<std::string, std::string> >& unmanaged() const { return m_unmanaged; }
    const std::vector<std::string>& notes() const { return m_notes; }
    std::string layerFileText() const { return renderLayerFile(m_layers); }
    bool layersChanged() const { return layerFileText() != m_savedLayerText; }
    // After the editor file's text is final (`editorText`, as saved or as it stands), into `part` (Framework/SavePlan):
    // the editor file when it does not exist yet, every game-mode file the layers make that differs from its file,
    // and Layer.con when it changed; the model takes them as saved once they are written. `editorFilePlanned`: the
    // level document's part writes the editor file already. False, with the part refused, when the editor file is not
    // in the retail layout.
    bool planGenerated(const std::string& editorText, SavePart& part, bool editorFilePlanned);
    int  activeLayer = 1;           // where new gameplay objects go
    // A free layer id: the smallest of 2..19 no layer holds (retail refuses 20 and more).
    int  freeLayerId() const;

private:
    bool                                             m_active = false;
    bool                                             m_editorFileExists = false;
    std::string                                      m_levelDirectory;
    std::string                                      m_editorPath;
    std::string                                      m_editorText;
    std::string                                      m_savedLayerText;
    GameplayFilters                                  m_filters;
    std::vector<GameplayLayer>                       m_layers;
    std::vector<std::pair<std::string, std::string> > m_unmanaged;
    std::vector<std::pair<std::string, std::string> > m_existing;   // game-mode files on disk
    std::vector<std::string>                         m_notes;
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_GAMEPLAYLAYERMODEL_H
