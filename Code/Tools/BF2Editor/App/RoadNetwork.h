// The level's editable roads, as the retail editor's road handler (LevelEditor) and spline manager (EditorEngine) kept
// them: the renderer's EditorSpline, RoadTemplate, PerlinNoise and RoadEditable objects (class manager instances held
// here: a road holds no reference to its spline, template or noise), built by the retail console rows - from the mod's
// road templates (Objects/Roads/Splines/<name>.con, their editor branch), the level's Editor/Splines.con and
// Editor/Roads.con, or typed - and shown through the renderer's editable-road queue. Saving writes those files back and
// compiles the roads (CompiledRoads.con and Roads/<road>_compiled.mesh, through the renderer's own compile into a staging
// folder, the mesh given the chunk table the 1.50 compile leaves out).
// Built differently from retail: the level's other compiled roads stay in CompiledRoads.con (retail rewrote the file from
// its editable roads only, which emptied every stock level's: none ships editable sources); a template is written only
// when it changed (retail rewrote every loaded template); nothing is written for a level that never had editable roads.
// The renderer's road objects need the editor RendDX9 image, a loaded level and the renderer's road singletons.
// Editor-only code (D3).
#ifndef BF2EDITOR_APP_ROADNETWORK_H
#define BF2EDITOR_APP_ROADNETWORK_H

#include "Framework/EditorConfig.h"
#include "Framework/RoadFiles.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace dice { namespace hfe { namespace geom {
class IEditorSpline;
class IPerlinNoise;
class IRoadEditable;
class IRoadTemplate;
} } }

namespace bf2editor
{

class RoadNetwork
{
public:
    RoadNetwork();
    ~RoadNetwork();

    // After the level loaded: the mod's road templates, then the level's Editor/Splines.con and Editor/Roads.con.
    // `levelName` is the level's folder name, `levelPath` its engine path with a trailing slash. `report` says what was
    // made and what was refused; false only when the renderer has no road classes (the other image).
    bool open(const std::string& levelName, const std::string& levelPath, std::string& report);
    // Releases every road, spline, template and noise object.
    void close();
    bool isOpen() const { return m_open; }

    // One retail console row - SplineManager.*, Road.*, RoadTemplate.* or RoadTemplateTexture.* - as the retail editor
    // ran it: rows build the spline (CreateInstance .. End), road (CreateInstance and the property rows after it) or
    // template (CreateInstance and the rows after it) last begun. The retail UI rows (SplineManager.SelectSpline,
    // Undo*, ...) are refused. Marks the roads changed when the row changed something that is saved.
    bool retailRow(const ConsoleRow& row, std::string& result);

    // What there is, sorted (spline ids ascending, names without regard to case).
    std::vector<int> splineIds() const;
    std::vector<std::string> roadNames() const;
    std::vector<std::string> templateNames() const;
    bool hasSpline(int id) const { return m_splines.count(id) != 0; }
    bool hasRoad(const std::string& name) const;
    bool hasTemplate(const std::string& name) const;
    // The road built on a spline ("" when none).
    std::string roadOnSpline(int id) const;
    // The roads built with a template.
    std::vector<std::string> roadsWithTemplate(const std::string& name) const;
    int nextSplineId() const;

    // An object's state as its file records it, read back from the renderer's object.
    bool splineRecord(int id, SplineRecord& record) const;
    bool roadRecord(const std::string& name, RoadRecord& record) const;
    bool templateRecord(const std::string& name, RoadTemplateRecord& record) const;
    // A spline's record moved `offset` metres sideways, as the renderer's offset copy (IEditorSpline +0x10) makes it - the
    // retail spline paste's copy, settings and point properties included. `record.id` is left for the caller. False
    // without the spline.
    bool offsetSplineRecord(int id, float offset, SplineRecord& record, std::string& error) const;
    // Rebuilds every road from its template, as the retail RefreshRoads did (template-changed posted for each road);
    // returns how many.
    int refreshRoads();
    // Makes an object hold a state, creating it when it does not exist; a null record removes it. A spline or template
    // a road is built on is not removed, nor a road given a spline or template that does not exist. The edit commands
    // and the undo history's replays go through these.
    bool applySpline(int id, const SplineRecord* record, std::string& error);
    bool applyRoad(const std::string& name, const RoadRecord* record, std::string& error);
    bool applyTemplate(const std::string& name, const RoadTemplateRecord* record, std::string& error);

    // A road drawn as its ghost (the quick preview of columns the retail editor showed first) or built final (the road
    // the compile writes). Not saved: a road loads as a ghost (retail).
    bool setGhost(const std::string& name, bool ghost, std::string& error);
    bool ghost(const std::string& name) const;
    // The retail "Apply spline": the road's footprint (its template's width plus 2 m and the noise's range) rendered
    // top down and its heights written into the editable heightmap cluster, the terrain told to rebuild the rectangle
    // (RE+7C; the editor's renderer takes the 8-bit target, which the shipped body skips on current cards); then the road
    // is rebuilt. False when the road cannot be built (a spline of fewer than four points).
    bool projectToTerrain(const std::string& name, std::string& error);

    // Changed since the open or the last save.
    bool changed() const { return m_changed; }
    // A one-line summary: counts, and what the open refused.
    std::string describe() const;
    // Everything a save writes, staged under a folder of the editor's own: Editor/Splines.con, Editor/Roads.con,
    // CompiledRoads.con and Roads/<road>_compiled.mesh for the level as {staged path, path relative to the level folder},
    // and every changed template's <name>.con, _compiled.con and _compiled.dat as {staged path, engine path the file
    // manager writes} in `templateFiles`. `levelDirectory` is the level folder the files go to; its CompiledRoads.con is
    // the one merged (the engine's copy when it has none). Roads with no geometry yet (fewer than four points) are
    // compiled into neither file and named in `result`. Nothing is staged when nothing changed, unless `force` asks for
    // a level that has or had editable roads. False with `result` saying why.
    bool stageSave(const std::string& levelDirectory, bool force,
                   std::vector<std::pair<std::string, std::string> >& levelFiles,
                   std::vector<std::pair<std::string, std::string> >& templateFiles, std::string& result);
    // The level has editable roads or splines, or had the files at the open.
    bool hasEditableRoads() const { return m_hadFiles || !m_splines.empty() || !m_roads.empty(); }
    // After the staged files were written: the roads are unchanged again.
    void markSaved();

private:
    struct Spline
    {
        dice::hfe::geom::IEditorSpline* object = 0;
    };
    struct Template
    {
        std::string                     name;
        dice::hfe::geom::IRoadTemplate* object = 0;
        std::string                     file;       // the engine path it was read from; empty for a new template
        bool                            changed = false;
    };
    struct Road
    {
        std::string                     name;
        int                             splineId = -1;
        std::string                     templateName;
        dice::hfe::geom::IRoadEditable* object = 0;
        dice::hfe::geom::IPerlinNoise*  noise = 0;
    };
    struct NoCase
    {
        bool operator()(const std::string& a, const std::string& b) const;
    };

    bool splineRow(const ConsoleRow& row, std::string& result);
    bool roadRow(const ConsoleRow& row, std::string& result);
    bool templateRow(const ConsoleRow& row, std::string& result);
    bool textureRow(const ConsoleRow& row, std::string& result);
    // The runs of the open: a template file's editor branch, a level file's rows; `made` counts objects begun.
    void runRows(const std::vector<ConsoleRow>& rows, const std::string& source, int& refused);

    dice::hfe::geom::IEditorSpline* createSplineObject(std::string& error);
    dice::hfe::geom::IRoadTemplate* createTemplateObject(std::string& error);
    // A road object on a spline and template, with its edge noise as the retail road handler made it (persistence 0.2,
    // range -0.15..0.15, three octaves; the seed when not -1), queued for drawing.
    bool createRoad(const std::string& name, int splineId, const std::string& templateName, int seed, std::string& error);
    void releaseRoad(Road& road);
    void writeSplineState(dice::hfe::geom::IEditorSpline* spline, const SplineRecord& record);
    // A spline object's state as its file records it.
    static void readSplineRecord(dice::hfe::geom::IEditorSpline* spline, int id, SplineRecord& record);
    void writeTemplateState(dice::hfe::geom::IRoadTemplate* object, const RoadTemplateRecord& record);
    void writeRoadState(Road& road, const RoadRecord& record);
    Road* findRoad(const std::string& name);
    const Road* findRoad(const std::string& name) const;
    Template* findTemplate(const std::string& name);
    const Template* findTemplate(const std::string& name) const;
    // Rebuilds the road on a spline, after its points changed.
    void rebuildRoadOn(int splineId);

    bool                                  m_open = false;
    bool                                  m_changed = false;
    bool                                  m_hadFiles = false;     // the level had Editor/Splines.con or Roads.con
    std::string                           m_levelName;
    std::string                           m_levelPath;
    std::map<int, Spline>                 m_splines;
    std::vector<Template>                 m_templates;
    std::vector<Road>                     m_roads;
    std::set<std::string, NoCase>         m_loadedRoads;          // the roads Roads.con made at the open
    std::vector<std::string>              m_refusals;             // what the open could not make, the first few
    int                                   m_refusedRows = 0;

    // The retail rows' building state.
    dice::hfe::geom::IEditorSpline*       m_buildSpline = 0;
    int                                   m_buildSplineId = -1;
    dice::hfe::geom::IRoadTemplate*       m_buildTemplate = 0;    // CreateInstance'd, named by SetName
    std::string                           m_buildTemplateName;
    int                                   m_textureLayer = 0;     // RoadTemplate.SetIsPrimaryTexture: 1 -> 0, 0 -> 1
    std::string                           m_activeRoad;           // Road.CreateInstance's, which Road.* rows set
    bool                                  m_loading = false;      // rows of the open: templates read are unchanged
    std::string                           m_currentSource;        // the file the open is running
    std::set<std::string, NoCase>         m_pendingRebuild;       // roads the open rebuilds once, after its rows
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_ROADNETWORK_H
