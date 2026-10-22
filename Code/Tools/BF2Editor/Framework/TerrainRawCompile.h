// Compiles the edited terrain into terraindata.raw (version 0x1A) the way the retail editor's terrain save writes it.
// Per patch: the header (grid place, the height range of its (patchSize + 1)^2 vertices, the bounding sphere of
// those points), the vertices (base-grid code, height in heightmap units, two geomorph deltas, packed normal) and the
// detail layers - a cell's two triangles where the layer's weight map is non-zero within two texels of the cell, a
// triangle dropped when all its corners lie more than 3 m under the sea level, and a seventh record with the triangles
// that touch the sea; each record's triangles in quadtree order with its LOD records. Then the surrounding sectors
// (heights and normals over the stepped side or corner grid as the game places them, the height range and bounding
// sphere of the full grid) and the culler (a quadtree over the map: squares, neighbours, height ranges, flags). The
// rest of the header, the terrain LOD block and the vertex format come from a template: the level's current file.
// Engine-independent: heights come through a callback. Editor-only code.
#ifndef BF2EDITOR_FRAMEWORK_TERRAINRAWCOMPILE_H
#define BF2EDITOR_FRAMEWORK_TERRAINRAWCOMPILE_H

#include "Framework/EditorConfig.h"
#include "Framework/TerrainRawFile.h"

#include <functional>
#include <string>
#include <vector>

namespace bf2editor
{

struct TerrainCompileSource
{
    // The terrain height in world units at world (x, z): the level's heightmap cluster, which answers for the
    // primary map and for the surrounding heightmaps. As precise as the source has it: the retail editor's x87 code
    // keeps the cluster's height unrounded (a heightmap value times its vertical scale), and the geomorph deltas are
    // computed from that; everything else rounds it to a float.
    std::function<double(float x, float z)> height;
    // The sea level the deep-water rule and the water record compare with (the cluster's sea water level).
    float seaWaterLevel = 0.0f;
    // The primary heightmap's columns (patchCount * patchSize + 1); 0 takes that from the template.
    int heightmapSize = 0;
    // Patch p's (file order) weight map for layer 0..5: weightSize^2 bytes, texel (x, z) at x + weightSize * z;
    // null when the patch has no such map (no weight anywhere).
    std::function<const unsigned char*(int patch, int layer)> weights;
    int weightSize = 512;
    // The surrounding sectors to write, slots 0..7 in ascending order (the sectors whose heightmap exists).
    std::vector<int> ringSlots;
    // When set, the header's detail textures and far terrain values are replaced (the ground properties).
    bool replaceDetailTextures = false;
    std::vector<TerrainRawDetailTexture> detailTextures;
    bool replaceFar = false;
    float farSideTiling[2] = { 0.0f, 0.0f };
    float farTopTilingHi = 0.0f;
    float farTopTilingLow = 0.0f;
    float farYOffset = 0.0f;
};

// Builds `out` from `templ` and `source`. False with `error` when the template is not a grid the compile handles
// (patch size 128, a square patch grid) or a callback is missing.
bool compileTerrainRaw(const TerrainRawFile& templ, const TerrainCompileSource& source, TerrainRawFile& out,
                       std::string& error);

// One patch layer's records from its triangles (indices into the (patchSize + 1)^2 vertices, in the writer's scan
// order: cells row by row, per cell (a, b, c) then (c, b, d)): the triangles reordered into the quadtree (each node's
// triangles that fit a child quadrant moved to the front, children recursing down to squares of patchSize / 8
// cells) and the LOD records in level order from the root, six words each, `recordCount` of them.
void orderLayerTriangles(std::vector<unsigned short>& indices, int patchSize, int recordCount,
                         std::vector<unsigned>& lodWords);

// The normal packing of a vertex: 0xFF000000 | x << 16 | y << 8 | z, each component as (int)(c * 127) + 127.
unsigned packTerrainNormal(float x, float y, float z);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_TERRAINRAWCOMPILE_H
