// Reads compiled terrain detail-layer metadata in renderer order. [CHOSEN] Seeds editor
// ground properties from it when GroundProperties.con is absent.
#ifndef BF2EDITOR_FRAMEWORK_TERRAINRAWLAYERS_H
#define BF2EDITOR_FRAMEWORK_TERRAINRAWLAYERS_H

#include "Framework/EditorConfig.h"

#include <string>
#include <vector>

namespace bf2editor
{

struct TerrainRawLayer
{
    std::string name;               // the texture name as the file holds it
    int         planeMap = 0;
    float       sideTiling[2] = { 0.0f, 0.0f };
    float       topTiling = 0.0f;
    float       yOffset = 0.0f;
    bool        envMap = false;
};

struct TerrainRawLayers
{
    unsigned version = 0;           // the header's first word (0x1A on 1.50 content) and its "has version" word
    unsigned hasVersion = 0;
    unsigned patchCount = 0;        // patches per side
    float    farSideTiling[2] = { 0.0f, 0.0f };
    float    farTopTilingHi = 0.0f;
    float    farTopTilingLow = 0.0f;
    float    farYOffset = 0.0f;
    std::vector<TerrainRawLayer> layers;

    // False with `error` when the bytes end early or a count is implausible.
    bool read(const std::string& bytes, std::string& error);
};

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_TERRAINRAWLAYERS_H
