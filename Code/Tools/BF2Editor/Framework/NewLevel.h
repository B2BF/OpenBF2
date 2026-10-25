// A new level as the retail editor's terrain editor makes one ("Create new terrain" and the save it forces): flat
// heightmaps at the default height - the primary 16-bit at a vertical scale of 0.0025, eight unmodified 8-bit
// surrounding heightmaps at 0.64 -, the level's scripts in the retail writers' text, the retail default colormap and
// detail maps copied for every patch, white terrain lightmaps, the HUD map and shadow map defaults, and the compiled
// terrain. Built differently from retail: Sky.con keeps the casing Init.con runs it by (retail wrote sky.con), the map
// description and the lightmap atlas index carry our own placeholder text, the patch textures keep the stock lowercase
// names (retail wrote Tx..), the editor maps are left to the editor's first terrain save, and every patch gets its
// low-detail map (black: no layer has a low-detail type), which the retail save's texture tool did not write. Added:
// an empty conquest gameplay layer for 16 players (GameModes/gpm_cq/16/GamePlayObjects.con), without which the game's
// level load - and this editor's, which goes through it - refuses the level; the retail editor opened its new level
// through Init.con's editor branch instead and left the layers to its layer panel.
// Engine-free: the caller supplies the default content and another level's terraindata.raw for the terrain LOD block.
// Editor-only code.
#ifndef BF2EDITOR_FRAMEWORK_NEWLEVEL_H
#define BF2EDITOR_FRAMEWORK_NEWLEVEL_H

#include "Framework/EditorConfig.h"
#include "Framework/TerrainRawFile.h"

#include <memory>
#include <string>
#include <vector>

namespace bf2editor
{

// The retail "Create new terrain" dialog's values.
struct NewLevelSettings
{
    std::string name;
    int         size = 1024;            // the primary heightmap's cells per side: 256, 512 or 1024
    int         scale = 2;              // metres per cell: 1, 2 or 4
    int         defaultHeight = 25;     // metres, 0..100
    int         lowResSize = 256;       // each surrounding heightmap's cells per side: 128, 256, 512 or 1024

    int worldSize() const { return size * scale; }
    // The surrounding heightmaps' metres per cell, as retail computed it (integer division first: 0 is refused).
    int lowResScale() const { return lowResSize > 0 ? scale * (size / lowResSize) : 0; }
    int patchCount() const { return size / 128; }
};

// The retail dialog's rules: a level name (letters, digits and underscores), the listed sizes and scales, a default
// height of 0..100 and a low-res scale of at least 1. False with `error` naming the first value refused.
bool checkNewLevelSettings(const NewLevelSettings& settings, std::string& error);

// The default content the retail editor copies from bf2editor/Content/Terrain, as file bytes.
struct NewLevelContent
{
    std::string colormap;               // defaultcolor.dds
    std::string detailmap1;             // defaultdetail_1.dds
    std::string detailmap2;             // defaultdetail_2.dds
    std::string ingameMap;              // defaultingamemap.dds
    std::string commanderMap;           // defaultcommandermap.dds
    std::string commanderMicromap;      // defaultcommandermicromap.dds
    std::string shadowmap;              // DefaultShadowmap/SimpleShadowmap.raw
};

// One file of the new level: its path relative to the level folder and its bytes (copies share one buffer).
struct NewLevelFile
{
    std::string                        path;
    std::shared_ptr<const std::string> bytes;
};

// Every file of the new level. `lodSource` is any terraindata.raw with the 128-cell patch grid: its terrain LOD block
// is taken over (the records are the same in every such file; only the index order differs). False with `error` when
// the settings are refused or the LOD source is not usable.
bool buildNewLevel(const NewLevelSettings& settings, const NewLevelContent& content, const TerrainRawFile& lodSource,
                   std::vector<NewLevelFile>& files, std::string& error);

// The terrain lighting preset the retail editor writes into a new level's Editor/TerrainLight.con - the lightmap
// generator's sun, GI lights and sky fill - as the console lines of the file's BF2Editor branch.
std::vector<std::string> terrainLightDefaults();

// Writes `files` into the level folder `target`, which must not exist: into `target`.tmp first (refused when that is
// there, say from an interrupted run), moved into place when whole. On a failure the files and folders written are
// removed again. False with `error` naming what failed.
bool writeNewLevel(const std::string& target, const std::vector<NewLevelFile>& files, std::string& error);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_NEWLEVEL_H
