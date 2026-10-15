// The mesh light sampler's translation unit (attested Geom/LightmapGeneration/MeshLightSampling.cpp,
// "...\Code\BF2\Geom\LightmapGeneration\MeshLightSampling.cpp"). [INFERRED] header placement, and
// class, record and member names; the 2006 editor's assertions name m_samples, m_textureSizeX/Y,
// m_realSampleCount, m_texture and m_lockInfo.
//
// StaticMesh::createLightSampler makes one sampler per traced (geom, lod): the precomputed sample grid
// of the template's .samples file becomes a single block of samples for the lightmap generator, the
// sampler packs the generator's results into an A8R8G8B8 lock texture, and its destructor writes the
// lightmap (dilated, converted to DXT1 or DXT5 with a full mip chain).

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_LIGHTMAPGENERATION_MESHLIGHTSAMPLING_H
#define BF2_GEOM_LIGHTMAPGENERATION_MESHLIGHTSAMPLING_H

#include "ILightSampling.h"
#include "../../Math/Vec3.h"
#include <string>

#include <stddef.h>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend { struct EngineTexture; struct TextureLockRecord; }

namespace geom {

// Prepares an A8R8G8B8 lightmap for DXT1 (which keeps no alpha): every texel's alpha byte moves
// into blue and alpha becomes 0xFF. Rows are `width` texels apart (no pitch). Shared with the
// terrain sampler's writer. [INFERRED] name.
void lightmapMoveAlphaToBlue(unsigned int* texels, unsigned int width, unsigned int height);

// One texel of the sample grid as the .samples file stores it.
struct MeshSampleRecord
{
    // The loader's fill value: position and normal zeroed, the triangle left as allocated.
    MeshSampleRecord()
    {
        position.x = 0.0f; position.y = 0.0f; position.z = 0.0f;
        normal.x = 0.0f;   normal.y = 0.0f;   normal.z = 0.0f;
    }

    Vec3          position;     // +0x00  object space; all components ~0 marks an empty texel
    Vec3          normal;       // +0x0C
    unsigned int  triangle;     // +0x18  index into the triangle list, 0xFFFFFFFF when invalid
};

// One triangle of the sample grid's mesh. The generator reads the three normals for its
// per-light back-face bits.
struct MeshSampleTriangle
{
    // The loader's fill value: the seven vectors zeroed, the two trailing words left as allocated.
    MeshSampleTriangle()
    {
        for (int i = 0; i < 3; ++i)
        {
            position[i].x = 0.0f; position[i].y = 0.0f; position[i].z = 0.0f;
            normal[i].x = 0.0f;   normal[i].y = 0.0f;   normal[i].z = 0.0f;
        }
        unresolved48.x = 0.0f; unresolved48.y = 0.0f; unresolved48.z = 0.0f;
    }

    Vec3          position[3];  // +0x00
    Vec3          normal[3];    // +0x24
    Vec3          unresolved48; // +0x48  zeroed, never read here [UNRESOLVED] role
    int           unresolved54; // +0x54  the loader stores -1 [UNRESOLVED] role
    int           unresolved58; // +0x58  the loader stores -1 [UNRESOLVED] role
};

// One local light ILightSampling::addLight recorded (getLights answers the vector holding them).
struct MeshSampleLight
{
    Vec3          position;     // +0x00
    Vec3          color;        // +0x0C
    float         range;        // +0x18  the attenuation range
};

class MeshLightSampler : public ILightSampling
{
public:
    // Reference count 1, no texture, no sample batch, no size cap. The rest is set by
    // createLightSampler and load().
    MeshLightSampler();
    // With a texture (a successful load), dilates the packed results three times, converts them
    // to DXT1 (lightmapFormat 1, after moving alpha into blue) or DXT5 with a full mip chain, and
    // saves "<output name>.dds"; then releases both textures and frees the records.
    ~MeshLightSampler();

    // Reads `path` (a .samples file, the SMP1 / SMP2 layout or the legacy one without magic) into
    // the sample grid and remembers `outputName` for the destructor. The grid is halved while it is
    // wider than the size cap and while the resolution modifier is below 1 (doubling it each time),
    // NaN positions are zeroed, and an A8R8G8B8 texture of the final size is created, locked and
    // cleared. False only when the file cannot be opened.
    bool load(const std::string& path, const std::string& outputName);

    // IBase
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();                                     // destroys and frees at zero
    virtual void* queryInterface(unsigned int iid) const;        // 1 and IID_ILightSampling

    // ILightSampling
    virtual unsigned int getBlockCount();                        // always 1
    virtual LightSampleBlock getBlock(unsigned int block);
    virtual unsigned int getSamples(unsigned int block, unsigned int maxCount, LightSample* out);
    virtual bool storeResults();
    virtual int  addLight(Vec3 color, Vec3 position, float range);
    virtual void* getLights();                                   // &m_lights

    // Keeps every second sample in both directions and halves the grid size. The source row
    // stride is twice the new width, which equals the old width only for even widths (shipped).
    void halveSampleGrid();

    // data, in shipped order (the vptr is +0x00)
    int                          m_refCount;            // +0x04  1
    std::vector<MeshSampleRecord>   m_samples;          // +0x08  (first at +0x0C) the sample grid
    // +0x18 - the triangle list; LightSampleBlock::triangles points at this vector itself.
    std::vector<MeshSampleTriangle> m_triangles;
    std::vector<MeshSampleLight>    m_lights;           // +0x28  (first at +0x2C)
    float                        m_resolutionModifier;  // +0x38  createLightSampler's argument
    unsigned int                 m_textureSizeX;        // +0x3C  the grid (and lightmap) width
    unsigned int                 m_textureSizeY;        // +0x40  its height
    unsigned int                 m_realSampleCount;     // +0x44  samples with a position
    dx9rend::EngineTexture*      m_texture;             // +0x48  the A8R8G8B8 lock texture
    dx9rend::TextureLockRecord*  m_lockInfo;            // +0x4C  held from load() to the destructor
    unsigned int                 m_nextSample;          // +0x50  the sampling cursor
    unsigned int                 m_batchStart;          // +0x54  where the last batch started
    unsigned int                 m_batchCount;          // +0x58  samples in the last batch
    LightSample*                 m_batch;               // +0x5C  the caller's buffer of the last batch
    std::string                 m_outputName;          // +0x60  the lightmap name, without ".dds"
    unsigned int                 m_usedWidth;           // +0x7C  the largest column holding a valid triangle
    unsigned int                 m_usedHeight;          // +0x80  the last row holding a valid triangle
    // +0x84 - -1 (none); the template's lightmap size for this (geom, lod) when it overrides lightmaps.
    int                          m_sizeCap;

private:
    MeshLightSampler(const MeshLightSampler&);
    MeshLightSampler& operator=(const MeshLightSampler&);
};

// The .samples file record load() reads as one block (28 bytes per sample): every architecture.
static_assert(sizeof(MeshSampleRecord) == 28, "a sample is 28 bytes - see the /28 at 0x101BD3F0");
#if defined(_M_IX86)
static_assert(sizeof(MeshSampleTriangle) == 92, "a triangle is 92 bytes - see 0x101BDF50");
static_assert(offsetof(MeshSampleTriangle, normal) == 0x24, "triangle normals at +0x24");
static_assert(offsetof(MeshSampleTriangle, unresolved54) == 0x54, "the loader's -1 at +0x54");
static_assert(sizeof(MeshSampleLight) == 28, "a light record is 28 bytes - see 0x101BDBE7");
static_assert(offsetof(MeshLightSampler, m_samples) == 0x08, "samples at +0x08");
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_LIGHTMAPGENERATION_MESHLIGHTSAMPLING_H
#endif // BF2_IMAGE_RENDDX9
