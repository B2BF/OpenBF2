// Tracks the open level's source records and their loaded objects for saving.
// StaticObjects.con is required; missing ambient/gameplay files are logged and leave related edits
// unsaved. Records bind at most one object and unmatched records retain their original text.
// A save plans the changed files (Framework/SavePlan), which writes them atomically through source control and
// verifies them. Unserializable edits keep the document modified and are reported. Editor-only.
#ifndef BF2EDITOR_APP_LEVELDOCUMENT_H
#define BF2EDITOR_APP_LEVELDOCUMENT_H

#include "Framework/EditorConfig.h"

#include "Framework/LevelObjectFile.h"
#include "Framework/LevelSettingsFile.h"

#include "Math/Mat4.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace dice { namespace hfe { namespace world { class IObject; } } }

namespace bf2editor
{

class GameplayLayerModel;
class SavePlan;

class LevelDocument
{
public:
    LevelDocument();

    // After the level has loaded: finds the level under `mods/<mod>/Levels/<level>/` in the working
    // directory (the BF2 root), reads its object files, binds their records and snapshots every root
    // object. `gameMode` and `layerSize` name the gameplay layer the load ran (EditorLevel::layerSize).
    // With `layers`, the retail layer model is decided first: when it
    // holds, the gameplay document is the editor file (Editor/GamePlayObjects.con, read or imported) and
    // `runLines` makes the objects of the other layers in the engine before the records are bound.
    bool open(const std::string& mod, const std::string& level, const std::string& gameMode, int layerSize,
              std::string& error, GameplayLayerModel* layers = 0,
              const std::function<void(const std::vector<std::string>&)>& runLines = std::function<void(const std::vector<std::string>&)>());
    bool isOpen() const { return m_open; }
    // "mods/bf2/Levels/Strike_at_Karkand/", as on disk.
    const std::string& levelDirectory() const { return m_levelDirectory; }

    // The files, StaticObjects.con first.
    size_t fileCount() const { return m_files.size(); }
    const std::string& fileName(size_t file) const { return m_files[file].name; }     // within the level
    const std::string& filePath(size_t file) const { return m_files[file].path; }     // from the BF2 root
    const LevelObjectFile& objects(size_t file) const { return m_files[file].objects; }
    size_t boundRecords(size_t file) const;
    // Objects of the shared templates are made in StaticObjects.con only (see above).
    bool takesNewObjects(size_t file) const { return file == 0; }
    // The loaded gameplay layer (GamePlayObjects.con), where gameplay objects are made; false when it is
    // not open.
    bool gameplayFile(size_t& file) const;
    // AmbientObjects.con, where triggers and effect areas are made; false when it is not open.
    bool ambientFile(size_t& file) const;
    // The level's gameplay layers on disk (GameModes/<mode>/<size>/GamePlayObjects.con), the mode as its
    // folder is spelled, sorted.
    std::vector<std::pair<std::string, int> > layersOnDisk() const;
    // A copy of the open gameplay layer, as saved, as GameModes/<mode>/<size>/GamePlayObjects.con - which
    // must not exist yet - and the level's map description listing it (`listed` says whether it can): the files
    // to write, {path, content} each, for the caller to write as one save. False with `error` when the copy
    // cannot be made.
    bool duplicateLayer(const std::string& gameMode, int size, std::vector<std::pair<std::string, std::string> >& files,
                        bool& listed, std::string& error);
    // Another layer's GamePlayObjects.con, read and parsed (never run); false when it is not on disk.
    bool readLayer(const std::string& gameMode, int size, LevelObjectFile& out) const;
    // The layers the level's map description (Info/<level>.desc) lists; false when it has none.
    bool describedLayers(std::vector<std::pair<std::string, int> >& layers) const;
    // Info/<level>.desc as on disk; "" when there is none.
    std::string descriptionPath() const;
    // The layer most of the file's records name (a gameplay layer gives all its objects one); -1 for none.
    int fileLayer(size_t file) const { return m_files[file].objects.dominantLayer(); }
    // The records naming the template that are not deleted, over every file.
    int liveRecords(const std::string& templateName) const;

    struct RecordRef
    {
        size_t file;
        size_t record;
        RecordRef() : file(0), record(0) {}
    };
    bool recordOf(unsigned id, RecordRef& ref) const;
    unsigned idOf(const RecordRef& ref) const;
    // The placement the object's record holds now (as last read from the engine).
    bool placementOf(unsigned id, ObjectPlacement& out) const;

    // Templates the level files define (phase C): which file, its class; its lines; an edit of one.
    bool templateFile(const std::string& name, size_t& file, std::string& className) const;
    std::vector<LevelObjectFile::TemplateLine> templateLines(size_t file, const std::string& name) const;
    bool setTemplateValue(size_t file, const std::string& name, const std::string& property, const std::string& value);
    // A new object's template, and one gone with its last object (phase C).
    bool addTemplate(size_t file, const std::string& className, const std::string& name,
                     const std::vector<std::pair<std::string, std::string> >& lines, bool addTrigger = false);
    // An ambient sound's record in the file's editor block (the one not deleted when there are more).
    bool soundRecord(size_t file, const std::string& name, RecordRef& ref) const;
    void setRecordPlacement(const RecordRef& ref, const ObjectPlacement& placement);
    bool setTemplateDeleted(size_t file, const std::string& name, bool deleted);
    // A template renamed in its file, with its records (LevelObjectFile::renameTemplate).
    bool renameTemplate(size_t file, const std::string& from, const std::string& to);
    // The records of an object template's objects renamed in every file; how many.
    size_t renameObjectRecords(const std::string& from, const std::string& to);
    // Combat areas (phase C): the file's block made to hold the engine's area as it is now (`data`), or
    // left out (`data` 0: the engine has none of that name); a new block when the file has none.
    bool syncCombatArea(size_t file, const std::string& name, const LevelObjectFile::CombatAreaData* data,
                        std::string& error);
    bool combatAreaWritable(size_t file, size_t area) const { return m_files[file].objects.isCombatAreaWritable(area); }
    void setCombatAreaSettings(size_t file, const LevelObjectFile::CombatAreaSettings& settings)
    {
        m_files[file].objects.setCombatAreaSettings(settings);
    }

    // Objects the editor made or unmade (WP6 create/delete).
    RecordRef addRecord(size_t file, unsigned id, const ObjectPlacement& placement);
    void setRecordDeleted(const RecordRef& ref, bool deleted);
    void rebind(const RecordRef& ref, unsigned id);         // a deleted object made again has a new id

    // The level's settings files (phase D): Init.con, Sky.con and Water.con, read when the document opens
    // (one that is missing, or that the editor cannot write back unchanged, is left out) and saved with
    // the objects. 0 when the level has none of that name.
    LevelSettingsFile* settingsFile(const std::string& name);
    size_t settingsFileCount() const { return m_settings.size(); }
    const std::string& settingsFileName(size_t i) const { return m_settings[i].name; }

    // What a save would do now.
    struct FileReport
    {
        int moved;          // records whose placement or properties changed
        int added;
        int deleted;
        int templates;      // templates with edited lines
        int areas;          // combat areas written differently (changed, new or deleted)
        bool areaSettings;  // the combat area manager's lines changed
        bool changed;       // the file would be written
    };
    struct Report
    {
        int moved;          // over every file
        int added;
        int deleted;
        std::vector<FileReport> files;      // per file, in file order
        std::vector<unsigned> unsaved;      // changed objects that belong to no record
        std::vector<std::string> settings;  // the settings files that changed
    };
    Report inspect();
    // "StaticObjects.con: 2 changed, 1 added, 0 deleted; AmbientObjects.con: 1 changed" - every file
    // with a change, or all of them when `everyFile`; "no changes" when there is nothing to list.
    std::string describe(const Report& report, bool everyFile) const;

    // The files a save writes, as parts of `plan` (Framework/SavePlan), one per file so that one that cannot be
    // written does not keep the others from being saved: every file that changed, or every file with `always`
    // (File > Resave: an unchanged one gets the same bytes back). Each part binds its file's records to the text
    // once it is written. `report` says what the save does. False with `error` when no document is open.
    bool planSave(SavePlan& plan, Report& report, bool always, std::string& error);

private:
    struct Snapshot
    {
        dice::hfe::Mat4 transform;
        int             layer;
        int             group;
        bool            disableChildren;
        bool            notInAI;
        bool            isOvergrowth;
        int             lightSourceMask;
        int             controlPointId;     // a spawner's; -1 for other objects
    };
    static Snapshot snapshotOf(dice::hfe::world::IObject* object);
    struct File
    {
        std::string           name;
        std::string           path;
        std::string           text;         // the file as last read or written
        LevelObjectFile       objects;
        std::vector<unsigned> ids;          // per record; 0 when unbound
    };

    bool openFile(const std::string& name, bool required, std::string& error);
    void pullFromEngine();
    void updatePreload();
    bool changedSinceOpen(unsigned id) const;
    void planFile(SavePlan& plan, size_t file, bool always);
    // The written file is the document now: its records in the order they were written, bound again.
    void bindWritten(size_t file, const std::string& text, const std::vector<size_t>& order);
    void indexIds(size_t file);

    struct SettingsFile
    {
        std::string       name;
        std::string       path;
        LevelSettingsFile file;
    };
    std::vector<SettingsFile>      m_settings;
    void planSettingsFile(SavePlan& plan, size_t index, bool always);
    bool                           m_open;
    std::string                    m_levelDirectory;
    std::vector<File>              m_files;
    std::map<unsigned, RecordRef>  m_recordById;
    std::map<unsigned, Snapshot>   m_snapshot;      // every root object, as it was after the load
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_LEVELDOCUMENT_H
