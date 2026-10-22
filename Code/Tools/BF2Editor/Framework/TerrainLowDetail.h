// A terrain patch's low-detail map, the texture the game draws the far terrain with, built from the patch's detail
// weights the way the renderer builds it (QuadPatchEditable::rebuildLowDetailMap): red is the saturated sum of the
// weights of the layers whose low-detail type is 1, blue that of the layers whose type is 2, green and alpha stay 0.
// Layer 3k + c's weight is byte c of detail map k's texel. And the reverse for a level without ground properties: the
// layers' types that best explain its compiled detail and low-detail maps [added: the retail editor took the types only
// from Editor/GroundProperties.con, which no stock level ships]. Engine-free. Editor-only code.
#ifndef BF2EDITOR_FRAMEWORK_TERRAINLOWDETAIL_H
#define BF2EDITOR_FRAMEWORK_TERRAINLOWDETAIL_H

#include "Framework/EditorConfig.h"
#include "Framework/DdsImage.h"

#include <vector>

namespace bf2editor
{

// `detail[k]` is the patch's detail map k (layers 3k..3k+2), null when it has none; a map that is not `size` square adds
// nothing. `types[l]` is layer l's low-detail type: 1 red, 2 blue, any other value neither.
DdsImage buildLowDetailMap(const DdsImage* const detail[2], const int types[6], int size);

// One patch of a level's compiled textures.
struct LowDetailSample
{
    const DdsImage* detail[2] = { 0, 0 };   // the game detail maps, null when absent
    const DdsImage* lowDetail = 0;          // the game low-detail map
};

// The types (0, 1 or 2 per layer) whose low-detail maps come closest to the samples', compared at each low-detail map's
// size (detail maps of another size scaled to it) on a grid of at most 64 x 64 texels per patch. Of equally close
// choices the one with fewer typed layers wins, so a layer without weight stays 0. `error` gets the mean absolute
// difference of red and blue, 0..255. False when no sample has both a low-detail map and a detail map.
bool inferLowDetailTypes(const std::vector<LowDetailSample>& samples, int types[6], float& error);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_TERRAINLOWDETAIL_H
