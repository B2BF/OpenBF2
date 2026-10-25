// Loads TerrainEditable from the editor branch of Terrain.con so height edits rebuild visible patches.
// [CHOSEN] Retains the compiled terrain because undergrowth holds a raw pointer to it.
// Stages terrain textures before init, which may write missing detail maps; opening leaves the level untouched.
// Unsupported far-tiling settings are reported. The editable terrain is retained because its base destructor
// would release uninitialized shader handles. Requires the editor RendDX9 image.
#ifndef BF2EDITOR_APP_EDITABLETERRAIN_H
#define BF2EDITOR_APP_EDITABLETERRAIN_H

#include "Framework/EditorConfig.h"

#include "App/TerrainEditing.h"
#include "Framework/GroundPropertiesFile.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace dice { namespace hfe { namespace geom { class ITerrainEditable; } } }

namespace bf2editor
{

class EditableTerrain
{
public:
    // After the level loaded: Terrain.con's editor branch run against the renderer's TerrainEditable. False with
    // `report` saying why (the compiled terrain then stays).
    bool open(const std::string& level, std::string& report);
    bool active() const { return m_editable != 0; }
    dice::hfe::geom::ITerrainEditable* editable() const { return m_editable; }
    // The look: 0 colormap and lightmap, 1 grid, 2 topographic grid, 3 detail layers, 4 undergrowth overlay, 7 base
    // pass (5, 6 and 8 stop at frontiers in 1.50's renderer: refused).
    bool setTechnique(int technique, std::string& error);
    int technique() const;
    // "TerrainEditable, patch size 128, technique 0, ..." or why there is none.
    std::string describe() const;
    // The staging folder the patch textures were read from (empty before open).
    const std::string& stagingRoot() const { return m_staging; }

    // Painting, as the retail TerrainEditor plugin's mode 7 did it: one
    // TerrainEditable call per tick - the detail layer (paintDetail), the colour (paintColor, or paintColorFromTexture
    // from the layer's colour texture), or both; the other button erases (detail with the negative amount, colour
    // always through the texture slot, as retail). amount = (int)(strength * 2.55). The renderer quarters `radius`
    // (TerrainEditable R2), which is why retail drew its cursor at a quarter of the brush.
    bool paint(const TerrainBrushSettings& brush, float x, float z, float radius, bool secondary, std::string& error);
    // Merge (retail TerrainEditor.MergeEditableAndStatic: commitDetailLayers / commitColorMapEdits) and Clear
    // (ClearEditable: revertDetailLayers / clearColorMapEdits), for the maps the paint mode names. The detail maps the
    // save writes hold unmerged strokes, so clearing them marks their patches changed again; the save commits colour
    // strokes into the colormap, which a later clear no longer reaches.
    bool mergeEdits(TerrainPaintMode mode, std::string& result);
    bool clearEdits(TerrainPaintMode mode, std::string& result);
    // Painted since the open or the last saveMaps.
    bool painted() const { return m_painted; }

    // The level's overgrowth (retail mode 9), through the renderer's Overgrowth rows: whether there is one to paint
    // (its rows answer only while the level has one, and its maps exist once its post-load ran with a path), its
    // materials as "name id" pairs, and one paint tick - Overgrowth.paintMaterial or paintShadowmap at world x, z with
    // a radius in metres; the other button erases (material 0, or shadow 0 [CHOSEN]: retail erased the material map
    // in either mode). A tick that repeats the previous command is not sent again (as retail). Not undoable: the maps
    // live in the renderer (as retail).
    bool overgrowthAvailable(std::string& why) const;
    std::vector<std::pair<std::string, int> > overgrowthMaterials() const;
    bool paintOvergrowth(const TerrainBrushSettings& brush, float x, float z, float radius, bool secondary,
                         std::string& error);
    bool overgrowthPainted() const { return m_overgrowthPainted; }
    // Regenerates the overgrowth's plants from its maps (Overgrowth.generateAll with the editor generator on). The
    // renderer's writes, the maps it reads back and the atlas it builds go to the staging folder, so the level's own
    // files are unchanged; the atlas sources the level's OvergrowthAtlas.tai lists are staged first. Returns false
    // with `result` saying why.
    bool generateOvergrowth(std::string& result);
    // Regenerates and saves the overgrowth into the staging folder (generateAll, then Overgrowth.save, so the collision
    // script holds the regenerated plants) and lists the staged files to copy into the level as {staged path, path
    // relative to the level folder}: the data files always, the atlas only when the overgrowth's types changed.
    bool saveOvergrowth(std::vector<std::pair<std::string, std::string> >& files, std::string& result);
    void markOvergrowthSaved() { m_overgrowthPainted = false; }

    // Terrain lightmaps (the retail Compile > Generate Lightmaps, its terrain part): the renderer's generator traces
    // the primary patches and/or the ring patches (`lightmapGeneration.traceTerrain` after `terrain.drawPrimaryLightmaps`
    // / `drawSurroundingLightmaps`, the 2006 editor's bindings, which the editor's renderer binds) into the staging
    // folder the patches read their textures from, and the patches reload them. The two switches also gate the
    // terrain's draw, so they are put back after (retail left them as set). The generator's settings are the caller's.
    // `files` lists the lightmaps written, as {staged path, path relative to the level folder}.
    bool traceLightmaps(bool primary, bool surrounding, std::vector<std::pair<std::string, std::string> >& files,
                        std::string& result);

    // The painted maps written by the renderer into the staging folder (save flag 2: the editor detail maps; +0x18:
    // the edited colormaps); `files` gets each staged file under the level's Editor folder as {staged path, path
    // relative to the level folder}. Flag 2 also rebuilds the primary heightmap's material grid from the layers'
    // materials, which a stock level's layers lack (retail's save zeroed Karkand's): the grid is written back from the
    // level's own material file [CHOSEN].
    bool saveMaps(std::vector<std::pair<std::string, std::string> >& files, std::string& error);
    void markMapsSaved() { m_painted = false; }

    // The compiled terrain written into the staging folder under Levels/<level>/: terraindata.raw (Framework/
    // TerrainRawCompile: the level's file as the template, the heights and the sea level from the heightmap cluster, the
    // weights from the staged editor detail maps - saveMaps writes the painted ones -, the surrounding sectors whose
    // heightmap the cluster holds, the ground properties' layers and far terrain), and the game textures of the patches
    // painted since the last compile (Framework/TextureEncode): their detail maps from the editor detail maps, their
    // colormaps from the editor colormaps, and their low-detail maps from the editor detail maps and the layers'
    // low-detail types (Framework/TerrainLowDetail) - every patch's when a type changed since the level's maps were
    // built, none while the types are unknown. `files` gets each as {staged path, path relative to the level folder}.
    // The level's own files are not changed. False with `result` saying why.
    bool compileTerrain(std::vector<std::pair<std::string, std::string> >& files, std::string& result);
    // After the compiled files were copied into the level: the painted patches start over, and the level's low-detail
    // maps hold the current types.
    void markCompiled();

    // The ring of surrounding patches the renderer draws around the primary tile (retail's Show / Hide on the sector
    // grid: createSurroundingPatch / removeSurroundingPatch; the view only). x, z: the sector, -1..1, not 0, 0.
    bool setSectorShown(int x, int z, bool shown, std::string& result);
    bool sectorShown(int x, int z) const;

    // Ground properties (Editor/GroundProperties.con, the retail TerrainEditor plugin's file): read at open, else seeded
    // from the compiled terrain's layers (Framework/TerrainRawLayers; no stock level ships the file), and applied to
    // the renderer's six layer records (App/TerrainLayerRecords). A seeded level is not changed until a setting is.
    // When no layer has a low-detail type (the compiled file holds none), the types are recovered from the level's
    // compiled detail and low-detail maps [added].
    const GroundProperties& ground() const { return m_ground; }
    const std::string& groundSource() const { return m_groundSource; }
    bool groundChanged() const { return !(m_ground.layers == m_groundSaved.layers && m_ground.misc == m_groundSaved.misc); }
    void markGroundSaved() { m_groundSaved = m_ground; }
    // One setting of a layer (0..5) or of the terrain (`layer` -1), applied to the renderer; `result` says what was not.
    bool setGround(int layer, const std::string& name, const std::string& value, std::string& result);
    // A layer taken out of the file (undoing its creation); the renderer's record keeps its settings.
    bool removeGroundLayer(int layer, std::string& result);

private:
    void loadGround(const std::string& levelPath, const std::vector<std::string>& lines);
    // The layers' low-detail types recovered from the level's game detail and low-detail maps (patchCount per side)
    // when they explain them closely; true when they were taken.
    bool recoverLowDetailTypes(unsigned patchCount);
    // Layer l's low-detail type in the ground properties, 0 without the layer.
    int lowDetailType(int layer) const;

    dice::hfe::geom::ITerrainEditable* m_editable = 0;
    std::string                        m_staging;
    std::string                        m_level;
    std::vector<std::string>           m_notApplied;   // editor-branch lines the editor could not apply
    std::vector<std::string>           m_baseNames;    // the four texture base names, as Terrain.con gives them
    std::string                        m_lightmapBase; // the lightmap one
    bool                               m_painted = false;
    bool                               m_overgrowthPainted = false;
    std::string                        m_lastOvergrowthCommand;
    bool                               m_sectorShown[8] = {};   // by ring slot
    std::set<std::pair<int, int> >     m_detailPainted;         // the patches (grid x, z) a detail stroke reached
    std::set<std::pair<int, int> >     m_colorPainted;          // and a colour stroke
    std::set<std::pair<int, int> >     m_detailUnmerged;        // detail strokes not merged yet (Clear reverts them)
    int                                m_editorColormaps = 0;   // patches given an editor colormap at open (paintable)
    GroundProperties                   m_ground;
    GroundProperties                   m_groundSaved;
    std::string                        m_groundSource;
    bool                               m_lowDetailTypesKnown = false;   // read from the file, recovered, or set
    int                                m_compiledLowDetailTypes[6] = {}; // the types the level's low-detail maps hold
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_EDITABLETERRAIN_H
