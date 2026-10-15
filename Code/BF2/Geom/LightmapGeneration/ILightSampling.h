// Light-sampler interface the lightmap generator drives (IID_ILightSampling, Core/Ids.h).
// [INFERRED] header placement: no header path is attested. The two implementations live in the
// attested Geom/TerrainLightSampling.cpp and Geom/LightmapGeneration/MeshLightSampling.cpp.
// Slot names are [INFERRED] from the two implementations and from LightmapGeneration's calls.

#ifndef BF2_GEOM_LIGHTMAPGENERATION_ILIGHTSAMPLING_H
#define BF2_GEOM_LIGHTMAPGENERATION_ILIGHTSAMPLING_H

#include "../../Core/IBase.h"
#include "../../Math/Vec3.h"
#include "../QuadPatch.h"                 // TerrainAabb - the engine's LocalBoundingBox shape

#include <stddef.h>

namespace dice {
namespace hfe {
namespace geom {

// One sample of a batch. getSamples fills position, normal and triangle; the generator writes the
// four result channels and the two flags; storeResults packs the channels into the lightmap texel
// (A = result[3], R = result[0], G = result[1], B = result[2], each * 255 and truncated).
struct LightSample
{
    // The generator builds its batch arrays with this: position and normal zeroed, everything else
    // left as allocated.
    LightSample()
    {
        position.x = 0.0f; position.y = 0.0f; position.z = 0.0f;
        normal.x = 0.0f;   normal.y = 0.0f;   normal.z = 0.0f;
    }

    Vec3          position;     // +0x00
    Vec3          normal;       // +0x0C
    // +0x18 - index into the block's triangle list (the generator's per-light back-face bits); the
    // terrain sampler always stores 0, the mesh sampler's invalid samples carry 0xFFFFFFFF.
    unsigned int  triangle;
    // +0x1C..+0x28 - the terrain sampler overwrites result[0] with the water depth below sea level.
    float         result[4];
    unsigned char done;         // +0x2C  the generator skips samples it has marked finished
    unsigned char directlyLit;  // +0x2D  set when a light exceeds directlyLightedThreshold
    unsigned char pad2E[2];
};

// The descriptor getBlock answers by value (through the hidden return pointer).
struct LightSampleBlock
{
    unsigned int  sampleCount;  // +0x00  texels in the block (terrain) / valid samples (mesh)
    TerrainAabb   bounds;       // +0x04  the block's world box (the mesh sampler leaves it empty)
    // +0x40 - the mesh sampler's std::vector<MeshSampleTriangle> of 92-byte triangle records itself
    // (its m_triangles; MeshLightSampling.h); 0 for the terrain sampler, which has no triangles. The
    // generator reads the vector's elements and each record's three normals.
    const void*   triangles;
};

// Generator-facing sampler. Created by ILightSampleable::createLightSampler and released through
// ILightSampleable::destroyLightSampler, which is when the lightmap file is written.
class ILightSampling : public IBase
{
public:
    // Number of blocks (terrain: one per patch and ring patch; mesh: always 1).
    virtual unsigned int getBlockCount() = 0;                                           // +0x10
    // Describes block `block`. The mesh sampler ignores the index.
    virtual LightSampleBlock getBlock(unsigned int block) = 0;                           // +0x14
    // Fills at most `maxCount` samples of `block` into `out` and answers how many were filled.
    // Selecting a different block first writes the previous block's lightmap.
    virtual unsigned int getSamples(unsigned int block, unsigned int maxCount,
                                    LightSample* out) = 0;                               // +0x18
    // Packs the last batch's results into the current block's texture; the terrain sampler
    // writes the block's lightmap once its last row is stored. Always answers true.
    virtual bool storeResults() = 0;                                                     // +0x1C
    // Records one point light (colour, world position, attenuation range) for the sampler and
    // answers its index (the mesh sampler: -1 while its record vector has no storage). The terrain
    // sampler ignores it and answers 0. [INFERRED] name.
    virtual int addLight(Vec3 color, Vec3 position, float range) = 0;                    // +0x20
    // The mesh sampler's recorded lights: its std::vector<MeshSampleLight> (m_lights) of 28-byte
    // (position, colour, range) records as addLight stores them; null for the terrain sampler. No
    // caller is known.
    // [INFERRED] name.
    virtual void* getLights() = 0;                                                       // +0x24

protected:
    ~ILightSampling() {}
};

#if defined(_M_IX86)
static_assert(sizeof(LightSample) == 0x30, "the generator strides samples by 48 bytes - see 0x10199F45");
static_assert(offsetof(LightSample, triangle) == 0x18, "triangle index at +0x18");
static_assert(offsetof(LightSample, result) == 0x1C, "results from +0x1C");
static_assert(offsetof(LightSample, done) == 0x2C, "done flag at +0x2C");
static_assert(offsetof(LightSample, directlyLit) == 0x2D, "directly-lit flag at +0x2D");
static_assert(sizeof(LightSampleBlock) == 0x44, "the block descriptor is 0x44 bytes - see 0x1019A1D7");
static_assert(offsetof(LightSampleBlock, bounds) == 0x04, "block bounds at +0x04");
static_assert(offsetof(LightSampleBlock, triangles) == 0x40, "block triangles at +0x40");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_LIGHTMAPGENERATION_ILIGHTSAMPLING_H
