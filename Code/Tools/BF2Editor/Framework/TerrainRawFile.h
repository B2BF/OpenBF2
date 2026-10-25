// The compiled terrain file, terraindata.raw, as the 1.50 game's Terrain::load reads it (version 0x1A):
// parsed into its blocks in file order and written back byte for byte. Fields whose meaning is not
// established keep their file names; floats are copied, never recomputed. Engine-independent.
#ifndef BF2EDITOR_FRAMEWORK_TERRAINRAWFILE_H
#define BF2EDITOR_FRAMEWORK_TERRAINRAWFILE_H

#include "Framework/EditorConfig.h"

#include <string>
#include <vector>

namespace bf2editor
{

// One detail texture record of the header (the ground properties' layer as the game draws it).
struct TerrainRawDetailTexture
{
    std::string   name;                 // texture path without extension, as the file holds it
    unsigned char planeMap = 0;
    float         sideTiling[2] = { 0.0f, 0.0f };
    float         topTiling = 0.0f;
    float         yOffset = 0.0f;
    unsigned char envMap = 0;
};

// One detail layer of a patch: the triangles drawn with one detail texture, as 16-bit indices into the
// patch's vertices, and the layer's LOD records (six words each in the file).
struct TerrainRawPatchLayer
{
    int                         marker = 0;         // the word ahead of the record; -1 ends the list
    unsigned                    detailIndex = 0;
    float                       vector24[3] = { 0.0f, 0.0f, 0.0f };
    int                         int30 = 0;
    unsigned                    word0C = 0;
    unsigned char               bytes10[4] = { 0, 0, 0, 0 };
    std::vector<unsigned>       lodWords;
    std::vector<unsigned short> indices;
};

// One patch of the patch grid: its place, bounds, vertices and detail layers.
struct TerrainRawPatch
{
    int                               gridX = 0;    // centred on 0
    int                               gridZ = 0;
    float                             maxHeight = 0.0f;
    float                             minHeight = 0.0f;
    float                             sphereCentre[3] = { 0.0f, 0.0f, 0.0f };
    float                             sphereRadius = 0.0f;
    std::string                       vertices;     // (patchSize + 1)^2 records of 24 bytes, 20 when compact
    std::vector<TerrainRawPatchLayer> layers;
};

// One sector of the surrounding ring (slots 0..7; the even slots are corners).
struct TerrainRawRingPatch
{
    int         slot = 0;
    float       maxY = 0.0f;
    float       minY = 0.0f;
    float       centre[3] = { 0.0f, 0.0f, 0.0f };
    float       radius = 0.0f;
    std::string heights;                // a float per vertex: the height over the secondary vertical scale
    std::string normals;                // 3 floats per vertex
};

// One node of the culler's quadtree (children of node k are 4k+1..4k+4).
struct TerrainRawCullerNode
{
    unsigned short neighbour[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // 0xFFFF: none
    float          minimum[3] = { 0.0f, 0.0f, 0.0f };
    float          maximum[3] = { 0.0f, 0.0f, 0.0f };
    unsigned char  byteEC = 0;
    unsigned char  hasPatchList = 0;
    unsigned char  byteEE = 0;
    unsigned char  patchIndex = 0;      // 0xFF: none
};

class TerrainRawFile
{
public:
    // False with `error` when the bytes are not a complete version-0x1A file: a block ends early, a count
    // is implausible, or bytes follow the culler.
    bool parse(const std::string& bytes, std::string& error);
    // The file as parse() read it, or as the fields now describe it.
    std::string render() const;

    // The vertex count of a ring sector in slot `slot` for this file's patch grid and world scales.
    unsigned ringVertexCount(int slot) const;

    unsigned short version = 0x1A;
    unsigned short hasVersion = 1;
    float          primaryScale[3] = { 0.0f, 0.0f, 0.0f };
    float          secondaryScale[3] = { 0.0f, 0.0f, 0.0f };
    unsigned       word2C = 0;
    float          maxHeight = 0.0f;
    float          minHeight = 0.0f;
    unsigned       patchSize = 0;
    unsigned char  subdividePatches = 0;
    unsigned       patchCount = 0;      // per side
    unsigned       detailmapSize = 0;
    unsigned       colormapSize = 0;
    std::string    colormapBaseName;
    std::string    detailmapBaseName;
    std::string    lowDetailmapBaseName;
    std::string    lightmapBaseName;
    float          farSideTiling[2] = { 0.0f, 0.0f };
    float          farTopTilingHi = 0.0f;
    float          farTopTilingLow = 0.0f;
    float          farYOffset = 0.0f;
    float          sunColor[3] = { 0.0f, 0.0f, 0.0f };
    float          giColor[3] = { 0.0f, 0.0f, 0.0f };
    float          waterColor[3] = { 0.0f, 0.0f, 0.0f };
    std::vector<TerrainRawDetailTexture> detailTextures;
    // The terrain's 85 LOD records: the first 21 hold 38 words each in the file, the rest 6.
    std::vector<unsigned>       lodWords;
    std::vector<unsigned short> lodIndices;
    unsigned char               compactVertices = 0;
    std::vector<TerrainRawPatch>      patches;      // patchCount^2, in file order
    std::vector<TerrainRawRingPatch>  ring;
    std::vector<TerrainRawCullerNode> culler;
};

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_TERRAINRAWFILE_H
