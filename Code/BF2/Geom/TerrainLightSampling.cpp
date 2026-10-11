// The editable terrain's light sampler (TerrainLightSampling.h), recovered from RendDX9.dll 1.50.
// The vector<TerrainLightBlock> internals are replaced by the tree's VC7.1 vector helpers (RW-74).
// Shipped defects kept: the half-texel offset is added in block-normalised units to world
// coordinates; the writer leaks both level surfaces of its conversion copy and dereferences a failed
// conversion texture; storeResults uses the lock's width as the texel pitch.
#ifdef BF2_IMAGE_RENDDX9

#include "TerrainLightSampling.h"
#include "TerrainEditable.h"
#include "QuadPatchEditable.h"
#include "SurroundingPatchEditable.h"
#include "Terrain.h"                              // terrainReleaseTexture
#include "LightmapGeneration/LightmapGeneration.h"    // g_LightmapGeneration - the lightmap format
#include "LightmapGeneration/MeshLightSampling.h"     // lightmapMoveAlphaToBlue

#include "../Core/Ids.h"                          // IID_ILightSampling
#include "../Physics/HeightmapCluster.h"
#include "../RendDX9/RendDX9Base.h"               // the TextureManager seams
#include "../RendDX9/Texture.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9Abi/Frontier.h"            // renddx9abi::logf - dice's debug stream
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"         // classManager
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"

#include <cstdarg>
#include <cstdio>

namespace dice {
namespace hfe {
namespace geom {

namespace {

const unsigned int kFormatA8R8G8B8 = 21u;
const unsigned int kFormatDXT1     = 0x31545844u;     // 'DXT1'
const unsigned int kFormatDXT5     = 0x35545844u;     // 'DXT5'
const int          kPoolManaged    = 1;               // the TextureManager's +0x20 creation pool
const int          kPoolScratch    = 3;

inline ::dice::hfe::world::IHeightmapCluster* heightmapClusterByName()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));                    // ClassManager +0x48
    return static_cast< ::dice::hfe::world::IHeightmapCluster*>(static_cast<void*>(found));
}

// The image's printf-into-a-string helper (TerrainEditable.cpp carries the same body): formats into
// a 0xFFF-character window and substitutes the shipped warning when the text does not fit.
void formatString(vc71::string& out, const char* format, ...)
{
    char buffer[0x1000];
    va_list args;
    va_start(args, format);
    const int written = _vsnprintf(buffer, 0xFFF, format, args);
    va_end(args);
    if (written < 0)
        std::snprintf(buffer, sizeof(buffer), "%s",
                      "WARNING: The original message was too long for formatString(). \n");
    else
        buffer[written] = 0;
    out = buffer;
}

// The box's inline size accessor: refresh while dirty, then the extent.
inline const Vec3& boxExtent(TerrainAabb& box)
{
    if (box.dirty)
        box.refresh();
    return box.extent;
}

// The shift loop buildBlocks inlines four times: 1 << (the number of halvings of value >> 1),
// i.e. the largest power of two not above value, and 1 for 0 and 1. The value is unsigned.
inline unsigned int floorPowerOfTwo(unsigned int value)
{
    int shift = 0;
    for (unsigned int rest = value >> 1; rest != 0; rest >>= 1)
        ++shift;
    return 1u << shift;
}

// The record buildBlocks builds on its stack: an empty box (zeroes, dirty), zero sizes and
// indices, not written.
inline void clearBlock(TerrainLightBlock& block)
{
    block.bounds.min.x = 0.0f; block.bounds.min.y = 0.0f; block.bounds.min.z = 0.0f;
    block.bounds.max.x = 0.0f; block.bounds.max.y = 0.0f; block.bounds.max.z = 0.0f;
    block.bounds.cornerLength = 0.0f;
    block.bounds.radius = 0.0f;
    block.bounds.dirty = 1;
    block.bounds.extent.x = 0.0f; block.bounds.extent.y = 0.0f; block.bounds.extent.z = 0.0f;
    block.bounds.centre.x = 0.0f; block.bounds.centre.y = 0.0f; block.bounds.centre.z = 0.0f;
    block.width = 0;
    block.height = 0;
    block.written = 0;
}

inline TerrainLightBlock* blockAt(vc71::vector_layout& blocks, unsigned int index)
{
    return reinterpret_cast<TerrainLightBlock*>(blocks.first) + index;
}

} // anonymous namespace


// The sampling cursor and the batch fields are left unset; selectBlock and getSamples
// write them before they are read.
TerrainLightSampler::TerrainLightSampler(TerrainEditable* terrain)
    : m_refCount(1),
      m_texture(0),
      m_resolutionModifier(1.0f),
      m_currentBlock(-1),
      m_current(0),
      m_terrain(terrain)
{
    m_blocks.first = 0;
    m_blocks.last = 0;
    m_blocks.end = 0;
}

// Drops the texture's owner reference (destroyed at zero), then frees the block storage.
TerrainLightSampler::~TerrainLightSampler()
{
    terrainReleaseTexture(m_texture);
    if (m_blocks.first)
        ::renddx9abi::engineFree(m_blocks.first);
    m_blocks.first = 0;
    m_blocks.last = 0;
    m_blocks.end = 0;
}

// No block current, the blocks built; always succeeds.
bool TerrainLightSampler::init()
{
    m_current = 0;
    m_currentBlock = -1;
    buildBlocks();
    return true;
}

void TerrainLightSampler::addRef() { ++m_refCount; }
int  TerrainLightSampler::getRef() { return m_refCount; }

// Answers the count after the decrement; zero destroys and frees.
int TerrainLightSampler::release()
{
    const int count = --m_refCount;
    if (count == 0)
    {
        this->~TerrainLightSampler();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// Shared with the mesh sampler in the image: IBase (1) and IID_ILightSampling answer this.
void* TerrainLightSampler::queryInterface(unsigned int iid) const
{
    TerrainLightSampler* const self = const_cast<TerrainLightSampler*>(this);
    if (iid == 1u)
        return self;
    return iid == IID_ILightSampling ? self : 0;
}

unsigned int TerrainLightSampler::getBlockCount()
{
    return ::vc71::vectorSizeIn(m_blocks, sizeof(TerrainLightBlock));
}

// The texel count (height * width) and a copy of the block's box; no triangles.
LightSampleBlock TerrainLightSampler::getBlock(unsigned int block)
{
    const TerrainLightBlock* const source = blockAt(m_blocks, block);
    LightSampleBlock result;
    result.sampleCount = source->height * source->width;
    result.bounds = source->bounds;
    result.triangles = 0;
    return result;
}

// Continues the current block (selecting `block` first when it differs) row by row:
// every texel becomes a sample at its spot of the block's box, height and normal from the
// HeightmapCluster (Phong normals on for the batch), clamped up to sea level with an up normal.
unsigned int TerrainLightSampler::getSamples(unsigned int block, unsigned int maxCount,
                                             LightSample* out)
{
    if (static_cast<unsigned int>(m_currentBlock) != block)
    {
        writeCurrentLightmap();
        selectBlock(block);
    }
    m_batchZ = m_nextZ;
    m_batchCount = 0;
    m_batchX = m_nextX;
    m_batch = out;

    TerrainLightBlock* const current = m_current;
    const Vec3& extent = boxExtent(current->bounds);
    const float stepX = 1.0f / static_cast<float>(current->width);
    const float stepZ = 1.0f / static_cast<float>(current->height);

    world::IHeightmapCluster* const cluster = heightmapClusterByName();
    cluster->setUsePhongNormalInterpolation(true);                            // +0xA8
    const float seaLevel = cluster->getSeaWaterLevel();                       // +0x60

    LightSample* sample = out;
    for (unsigned int i = 0; i < maxCount; ++i, ++sample)
    {
        if (m_nextZ >= m_current->height)
            break;
        const float z = static_cast<float>(m_nextZ) * stepZ * extent.z + m_current->bounds.min.z;
        const float x = static_cast<float>(m_nextX) * stepX * extent.x + m_current->bounds.min.x;
        sample->position.y = 100000.0f;
        sample->position.x = x;
        sample->position.z = z;
        sample->position.x = stepX * 0.5f + sample->position.x;
        sample->position.z = stepZ * 0.5f + sample->position.z;

        unsigned char material;
        const float height = cluster->getHeightAndNormalInWorldCoords(
            sample->position.x, sample->position.z, sample->normal, material);   // +0x80
        sample->position.y = height;
        if (height < seaLevel)
        {
            sample->position.y = seaLevel;
            sample->normal.x = 0.0f;
            sample->normal.y = 1.0f;
            sample->normal.z = 0.0f;
        }
        sample->triangle = 0;

        ++m_nextX;
        if (m_nextX >= m_current->width)
        {
            ++m_nextZ;
            m_nextX = 0;
        }
        ++m_batchCount;
    }
    cluster->setUsePhongNormalInterpolation(false);
    return m_batchCount;
}

// Asks the heightmap again for every sample of the last batch (position.y and the normal
// are overwritten, unclamped), puts the water depth below sea level into result[0] (0..3 m -> 0..1,
// left alone above water), and stores A = result[3], R = result[0], G = result[1], B = result[2]
// (each * 255, truncated, low byte kept) at the batch's texels. The finished block is written.
bool TerrainLightSampler::storeResults()
{
    world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const float seaLevel = cluster->getSeaWaterLevel();                       // +0x60
    dx9rend::TextureLockRecord* lock;
    do
    {
        lock = dx9rend::textureLock(m_texture, 0, 0x7FFFFFFF);
    }
    while (!lock);
    unsigned int* const texels = static_cast<unsigned int*>(lock->pBits);

    for (unsigned int i = 0; i < m_batchCount; ++i)
    {
        LightSample& sample = m_batch[i];
        unsigned char material;
        const float height = cluster->getHeightAndNormalInWorldCoords(
            sample.position.x, sample.position.z, sample.normal, material);   // +0x80
        sample.position.y = height;
        const float depth = seaLevel - height;
        if (depth >= 0.0f)
        {
            if (depth < 3.0f)
                sample.result[0] = depth * 0.33333334f;
            else
                sample.result[0] = 1.0f;
        }
        const unsigned int a = static_cast<unsigned int>(static_cast<int>(sample.result[3] * 255.0f)) & 0xFFu;
        const unsigned int r = static_cast<unsigned int>(static_cast<int>(sample.result[0] * 255.0f)) & 0xFFu;
        const unsigned int g = static_cast<unsigned int>(static_cast<int>(sample.result[1] * 255.0f)) & 0xFFu;
        const unsigned int b = static_cast<unsigned int>(static_cast<int>(sample.result[2] * 255.0f)) & 0xFFu;
        texels[lock->width * m_batchZ + m_batchX] = (a << 24) | (r << 16) | (g << 8) | b;
        ++m_batchX;
        if (m_batchX >= lock->width)
        {
            ++m_batchZ;
            m_batchX = 0;
        }
    }

    const unsigned int lockHeight = lock->height;
    dx9rend::textureUnlock(m_texture, lock);
    if (m_nextZ >= lockHeight)
        writeCurrentLightmap();
    return true;
}

// The terrain sampler keeps no lights: always 0.
int TerrainLightSampler::addLight(Vec3, Vec3, float)
{
    return 0;
}

// No light records: always null.
void* TerrainLightSampler::getLights()
{
    return 0;
}

void TerrainLightSampler::buildBlocks()
{
    if (m_terrain->m_drawMain)                                               // +0x358
    {
        for (int i = 0; i < m_terrain->m_patchCount * m_terrain->m_patchCount; ++i)
        {
            QuadPatchEditable* const patch = m_terrain->m_editablePatches[i];
            int fileX;
            int fileZ;
            patch->getFileIndices(&fileX, &fileZ);
            TerrainLightBlock block;
            clearBlock(block);
            block.patchZ = fileZ;
            block.patchX = fileX;
            patch->getWorldBounds(block.bounds);
            const float resolution = m_resolutionModifier;
            const int width = static_cast<int>(boxExtent(block.bounds).x * resolution);
            const int height = static_cast<int>(boxExtent(block.bounds).z * resolution);
            block.width = floorPowerOfTwo(static_cast<unsigned int>(width));
            block.height = floorPowerOfTwo(static_cast<unsigned int>(height));
            ::vc71::vectorPushBackIn(m_blocks, sizeof(TerrainLightBlock), &block);
        }
    }
    if (m_terrain->m_flag359)                                                // +0x359
    {
        for (int slot = 0; slot < 8; ++slot)
        {
            SurroundingPatchEditable* const ring = m_terrain->m_editableSurrounding[slot];   // +0x788
            if (!ring)
                continue;
            TerrainLightBlock block;
            clearBlock(block);
            block.patchX = -1;
            block.patchZ = slot;
            ring->getWorldBounds(block.bounds);
            const float resolution = m_resolutionModifier;
            const int width = static_cast<int>(boxExtent(block.bounds).x * resolution) / 8;
            const int height = static_cast<int>(boxExtent(block.bounds).z * resolution) / 8;
            block.width = floorPowerOfTwo(static_cast<unsigned int>(width));
            block.height = floorPowerOfTwo(static_cast<unsigned int>(height));
            ::vc71::vectorPushBackIn(m_blocks, sizeof(TerrainLightBlock), &block);
        }
    }
}

// A texture of another size is dropped (owner reference only); a new one is made in
// the scratch pool (the TextureManager's +0x20 swapped to 3 around the create), one level.
void TerrainLightSampler::selectBlock(unsigned int block)
{
    m_currentBlock = static_cast<int>(block);
    m_current = blockAt(m_blocks, block);
    m_nextX = 0;
    m_nextZ = 0;
    if (m_texture)
    {
        if (dx9rend::textureDims(m_texture)[0] != m_current->width
            || dx9rend::textureDims(m_texture)[1] != m_current->height)
        {
            dx9rend::EngineTexture* const texture = m_texture;
            if (--texture->refCount == 0)
            {
                dx9rend::textureDtor(texture);
                ::renddx9abi::engineFree(texture);
            }
            m_texture = 0;
        }
    }
    if (!m_texture)
    {
        const int savedPool = dx9rend::textureManagerSwapManaged(kPoolScratch);
        m_texture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            m_current->width, m_current->height, kFormatA8R8G8B8, 1, 0, 0));
        dx9rend::textureManagerSwapManaged(savedPool);
    }
}

// In DXT1 mode the block texture's alpha first moves into blue (lightmapMoveAlphaToBlue, which the
// mesh sampler shares). Ring patches save the A8R8G8B8 texture as it is; patches convert it to DXT1
// or DXT5 in a managed copy with a full mip chain (D3DXFilterTexture) and save that.
void TerrainLightSampler::writeCurrentLightmap()
{
    if (m_currentBlock == -1)
        return;
    m_current->written = 1;
    const bool dxt1 = g_LightmapGeneration->m_lightmapFormat == 1;           // +0x124
    if (m_current->patchX == -1)
    {
        vc71::string name;
        {
            const vc71::string base = m_terrain->getEditorLightmapBaseName();       // P+0x104
            const TerrainEditableString path = m_terrain->getLevelSavePath();      // E+0x90
            formatString(name, "%s%s_s%i", path.c_str(), base.c_str(), m_current->patchZ);
        }
        if (dxt1)
        {
            dx9rend::TextureLockRecord* lock;
            do
            {
                lock = dx9rend::textureLock(m_texture, 0, 0x7FFFFFFF);
            }
            while (!lock);
            lightmapMoveAlphaToBlue(static_cast<unsigned int*>(lock->pBits), lock->width, lock->height);
            dx9rend::textureUnlock(m_texture, lock);
        }
        dx9rend::textureManagerSaveTexture(m_texture, name);
        m_currentBlock = -1;
        m_current = 0;
        return;
    }

    vc71::string name;
    {
        TerrainEditableNameResult base = m_terrain->getLightmapBaseName();    // E+0x74
        const TerrainEditableString path = m_terrain->getLevelSavePath();    // E+0x90
        formatString(name, "%s%s%02ix%02i", path.c_str(), base.c_str(),
                     m_current->patchX, m_current->patchZ);
    }
    const int savedPool = dx9rend::textureManagerSwapManaged(kPoolManaged);
    if (dxt1)
    {
        dx9rend::TextureLockRecord* lock;
        do
        {
            lock = dx9rend::textureLock(m_texture, 0, 0x7FFFFFFF);
        }
        while (!lock);
        lightmapMoveAlphaToBlue(static_cast<unsigned int*>(lock->pBits), lock->width, lock->height);
        dx9rend::textureUnlock(m_texture, lock);
    }
    const unsigned int* const dims = dx9rend::textureDims(m_texture);
    dx9rend::EngineTexture* const converted = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerCreateEmpty(dims[0], dx9rend::textureDims(m_texture)[1],
                                           dxt1 ? kFormatDXT1 : kFormatDXT5, 0, 0, 0));
    if (!converted)
        ::renddx9abi::logf("TerrainLightSampling.cpp:205: Failed to create conversion texture");
    // The source level is fetched first; neither surface reference is released (shipped).
    IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_texture, 0, 0x7FFFFFFF);
    IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(converted, 0, 0x7FFFFFFF);
    D3DXLoadSurfaceFromSurface(destination, 0, 0, source, 0, 0, 0xFFFFFFFFu, 0);
    D3DXFilterTexture(converted->baseTexture, 0, 0, 0xFFFFFFFFu);
    dx9rend::textureManagerSaveTexture(converted, name);
    dx9rend::textureManagerSwapManaged(savedPool);
    terrainReleaseTexture(converted);
    m_currentBlock = -1;
    m_current = 0;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
