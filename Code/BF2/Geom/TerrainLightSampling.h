// The editable terrain's light sampler: one lightmap block per primary patch and per existing ring
// patch, sampled on the heightmap and written as the patch lightmaps when each block completes.
// [INFERRED] header placement beside the attested Geom/TerrainLightSampling.cpp (its __FILE__ is
// "...\Code\BF2\Geom\TerrainLightSampling.cpp"). Class and member names are [INFERRED].

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_TERRAINLIGHTSAMPLING_H
#define BF2_GEOM_TERRAINLIGHTSAMPLING_H

#include "LightmapGeneration/ILightSampling.h"
#include "QuadPatch.h"

#include <stddef.h>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend { struct EngineTexture; }

namespace geom {

class TerrainEditable;

// One block: a patch's (or ring patch's) world box and its lightmap size.
struct TerrainLightBlock
{
    TerrainAabb   bounds;      // +0x00  y range 0..0; extent.x / .z drive the lightmap size
    unsigned int  width;       // +0x3C  the largest power of two <= extent.x * resolution (ring: / 8)
    unsigned int  height;      // +0x40  the same over extent.z
    int           patchX;      // +0x44  the patch's file index x, or -1 for a ring patch
    int           patchZ;      // +0x48  the patch's file index z, or the ring slot 0..7
    unsigned char written;     // +0x4C  raised when the block's lightmap is written
    unsigned char pad4D[3];
};

class TerrainLightSampler : public ILightSampling
{
public:
    // Stores `terrain`; resolution 1, no block selected. The block list is empty until init().
    explicit TerrainLightSampler(TerrainEditable* terrain);
    // Releases the block texture and frees the block list. Does not write a pending lightmap.
    ~TerrainLightSampler();

    // Builds the block list (buildBlocks) and answers true.
    bool init();

    // IBase
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();                                     // destroys and frees at zero
    virtual void* queryInterface(unsigned int iid) const;        // 1 and IID_ILightSampling

    // ILightSampling
    virtual unsigned int getBlockCount();
    virtual LightSampleBlock getBlock(unsigned int block);
    virtual unsigned int getSamples(unsigned int block, unsigned int maxCount, LightSample* out);
    virtual bool storeResults();
    virtual int  addLight(Vec3 color, Vec3 position, float range);   // ignored, answers 0
    virtual void* getLights();                                       // null

    // One block per primary patch while the terrain draws them (+0x358), then one per existing
    // ring patch while it draws those (+0x359).
    void buildBlocks();
    // Makes `block` current: resets the texel cursor and keeps the block texture only when its
    // size matches, else creates a fresh A8R8G8B8 texture in the scratch pool.
    void selectBlock(unsigned int block);
    // Writes the current block's texture and deselects it. Patches go to
    // "<level save path><lightmap base>%02ix%02i.dds" as DXT1 (lightmapFormat 1) or DXT5 with mips,
    // ring patches to "<level save path><editor lightmap base>_s%i.dds" uncompressed.
    void writeCurrentLightmap();

    // data, in shipped order (the vptr is +0x00)
    int                      m_refCount;            // +0x04  1
    std::vector<TerrainLightBlock> m_blocks;        // +0x08
    dx9rend::EngineTexture*  m_texture;             // +0x18  the current block's lock texture
    float                    m_resolutionModifier;  // +0x1C  1.0; createLightSampler stores its argument
    int                      m_currentBlock;        // +0x20  -1 = none
    TerrainLightBlock*       m_current;             // +0x24
    unsigned int             m_nextX;               // +0x28  the sampling cursor (not set by the ctor)
    unsigned int             m_nextZ;               // +0x2C
    unsigned int             m_batchX;              // +0x30  where the last batch started
    unsigned int             m_batchZ;              // +0x34
    unsigned int             m_batchCount;          // +0x38  samples in the last batch
    LightSample*             m_batch;               // +0x3C  the caller's buffer of the last batch
    TerrainEditable*         m_terrain;             // +0x40

private:
    TerrainLightSampler(const TerrainLightSampler&);
    TerrainLightSampler& operator=(const TerrainLightSampler&);
};

#if defined(_M_IX86)
static_assert(sizeof(TerrainLightBlock) == 0x50, "one 80-byte block - see the /80 at 0x10199EBD");
static_assert(offsetof(TerrainLightBlock, width) == 0x3C, "width at +0x3C");
static_assert(offsetof(TerrainLightBlock, patchX) == 0x44, "patch x at +0x44");
static_assert(offsetof(TerrainLightBlock, written) == 0x4C, "written flag at +0x4C");
static_assert(offsetof(TerrainLightSampler, m_blocks) == 0x08, "block list at +0x08");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_TERRAINLIGHTSAMPLING_H
#endif // BF2_IMAGE_RENDDX9
