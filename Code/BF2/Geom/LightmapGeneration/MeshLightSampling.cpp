// The mesh light sampler (MeshLightSampling.h), recovered from RendDX9.dll 1.50. The three record
// vectors are std::vectors: the generator reads the triangle vector through
// LightSampleBlock::triangles.
// Shipped defects kept: storeResults addresses texels by sample index (no pitch); the destructor
// reads the lightmap format before testing for a texture, leaks both level surfaces of its
// conversion copy and dereferences a failed conversion texture; the grid halving strides rows by
// twice the new width; a resolution modifier of zero or below never ends the loader's halving.
#ifdef BF2_IMAGE_RENDDX9

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#endif
#include "MeshLightSampling.h"
#include "LightmapGeneration.h"                   // g_LightmapGeneration - format and trace mode
#include "../XpLightManager.h"                    // the lightmap alpha cutoff range

#include "../../RendDX9/RendDX9Base.h"            // the TextureManager seams
#include "../../RendDX9/Texture.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../../../RendDX9Abi/EngineFile.h"       // EngineStream - FileManager open / read / seek
#include "../../../RendDX9Abi/Frontier.h"         // renddx9abi::logf - dice's debug stream

#include <cfloat>
#include <cmath>
#include <cstring>
#include <float.h>                                // _isnan

namespace dice {
namespace hfe {
namespace geom {

extern LightManager* g_LightManager;              // XpLightManager.cpp

namespace {

// Core/Ids.h's IID_ILightSampling, kept file-local: Ids.h also defines the LightManager ids that
// XpLightManager.h declares extern (LightmapGeneration.cpp's precedent).
const unsigned int kIIDLightSampling = 1979283965u;
const unsigned int kFormatA8R8G8B8   = 21u;
const unsigned int kFormatDXT1       = 0x31545844u;   // 'DXT1'
const unsigned int kFormatDXT5       = 0x35545844u;   // 'DXT5'
const int          kPoolManaged      = 1;             // the TextureManager's +0x20 creation pool
const int          kPoolScratch      = 3;

// A sample is present when a magnitude exceeds FLT_EPSILON or is unordered.
inline bool hasPosition(const Vec3& position)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] FCOM signals invalid for unordered components.
    return !bf2x87::lessEqual(bf2x87::wide(std::fabs(position.x)), bf2x87::wide(FLT_EPSILON))
        || !bf2x87::lessEqual(bf2x87::wide(std::fabs(position.y)), bf2x87::wide(FLT_EPSILON))
        || !bf2x87::lessEqual(bf2x87::wide(std::fabs(position.z)), bf2x87::wide(FLT_EPSILON));
#else
    return !(std::fabs(position.x) <= FLT_EPSILON) || !(std::fabs(position.y) <= FLT_EPSILON)
        || !(std::fabs(position.z) <= FLT_EPSILON);
#endif
}

// Zeroes a position with a NaN component (x, then y, then z tested as doubles).
inline void zeroNanPosition(Vec3& position)
{
    if (_isnan(position.x) || _isnan(position.y) || _isnan(position.z))
    {
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = 0.0f;
    }
}

// The image's shared clamp: the high bound is tested first; a NaN fails the first compare and
// takes the low bound.
inline float clampValue(float value, float low, float high)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Match the shared PE 0x1000ED40 FCOMI exception flags.
    if (bf2x87::greaterEqual(bf2x87::wide(value), bf2x87::wide(high)))
        value = high;
    if (!bf2x87::greaterEqual(bf2x87::wide(value), bf2x87::wide(low)))
        value = low;
#else
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
#endif
    return value;
}

// One packed channel: __ftol2's 64-bit truncation, its low byte kept.
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] Keep the x87 value through __ftol2, without a float store.
using SampleReal = bf2x87::Real;
inline SampleReal sampleProduct(float a, float b)
{
    return bf2x87::mul(bf2x87::wide(a), bf2x87::wide(b));
}
#else
using SampleReal = float;
inline SampleReal sampleProduct(float a, float b) { return a * b; }
#endif

inline unsigned int channelByte(SampleReal value)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return static_cast<unsigned int>(bf2x87::toInt64Truncate(value)) & 0xFFu;
#else
    return static_cast<unsigned int>(static_cast<long long>(value)) & 0xFFu;
#endif
}

// Drops an owner reference; the last one destroys and frees the texture.
inline void releaseTexture(dx9rend::EngineTexture* texture)
{
    if (texture && --texture->refCount == 0)
    {
        dx9rend::textureDtor(texture);
        ::renddx9abi::engineFree(texture);
    }
}

// Fills every empty (zero) interior texel of `source` with the per-channel average of its
// non-empty 3 x 3 neighbourhood, written to `destination`; border texels are left alone. Both
// buffers hold the result afterwards: `destination` starts as a copy of `source` and is copied
// back. [INFERRED] name.
// PE 0x101BD160
void lightmapDilate(unsigned int* source, unsigned int* destination, unsigned int width,
                    unsigned int height)
{
    const unsigned int bytes = 4u * height * width;
    std::memcpy(destination, source, bytes);
    for (unsigned int y = 1; y < height - 1; ++y)
    {
        for (unsigned int x = 1; x < width - 1; ++x)
        {
            if (source[y * width + x] != 0)
                continue;
            unsigned int count = 0;
            unsigned int red = 0;
            unsigned int green = 0;
            unsigned int blue = 0;
            unsigned int alpha = 0;
            for (unsigned int ny = y - 1; ny <= y + 1; ++ny)
            {
                for (unsigned int nx = x - 1; nx <= x + 1; ++nx)
                {
                    const unsigned int texel = source[ny * width + nx];
                    if (!texel)
                        continue;
                    red += (texel >> 16) & 0xFFu;
                    green += (texel >> 8) & 0xFFu;
                    blue += texel & 0xFFu;
                    alpha += texel >> 24;
                    ++count;
                }
            }
            if (count)
            {
                destination[y * width + x] = ((blue / count) & 0xFFu)
                                           | (((green / count) & 0xFFu) << 8)
                                           | (((red / count) & 0xFFu) << 16)
                                           | ((alpha / count) << 24);
            }
        }
    }
    std::memcpy(source, destination, bytes);
}

} // anonymous namespace


// Answers the grid sample at the fractional position (x, y): column = trunc(width * x), row =
// trunc(height * y). The record is copied with cleared results and done flag. [INFERRED] name.
// PE 0x101BD7F0: unreachable in 1.50: no code or data references.
// [CHOSEN, not recovered] Memory safety: the shipped body indexes the grid unchecked and copies the
// stack byte after its truncation temporary into directlyLit (+0x2D). An index outside the grid
// answers an empty record (zero position and normal, triangle 0xFFFFFFFF, the invalid-sample marker),
// and directlyLit is zero.
LightSample MeshLightSampler::getSampleAt(float x, float y)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Keep the x87 products through FISTP's truncation.
    const int row = bf2x87::toIntTruncate(sampleProduct(static_cast<float>(m_textureSizeY), y));
    const int column = bf2x87::toIntTruncate(sampleProduct(static_cast<float>(m_textureSizeX), x));
#else
    const int row = static_cast<int>(static_cast<float>(m_textureSizeY) * y);
    const int column = static_cast<int>(static_cast<float>(m_textureSizeX) * x);
#endif
    const unsigned int index = static_cast<unsigned int>(row) * m_textureSizeX
                             + static_cast<unsigned int>(column);
    LightSample sample;
    if (m_samples.data() && index < m_samples.size())
    {
        const MeshSampleRecord& record = m_samples[index];
        sample.position = record.position;
        sample.normal = record.normal;
        sample.triangle = record.triangle;
    }
    else
    {
        sample.triangle = 0xFFFFFFFFu;
    }
    sample.result[0] = 0.0f;
    sample.result[1] = 0.0f;
    sample.result[2] = 0.0f;
    sample.result[3] = 0.0f;
    sample.done = 0;
    sample.directlyLit = 0;
    sample.pad2E[0] = 0;
    sample.pad2E[1] = 0;
    return sample;
}

// The vectors start empty and the string empty; the size fields, the cursor and the batch count
// are written by load() and getSamples before they are read.
// PE 0x101BD8D0
MeshLightSampler::MeshLightSampler()
    : m_refCount(1),
      m_texture(0),
      m_lockInfo(0),
      m_batch(0),
      m_sizeCap(-1)
{
}

// PE 0x101BD920
MeshLightSampler::~MeshLightSampler()
{
    const bool dxt1 = g_LightmapGeneration->m_lightmapFormat == 1;           // +0x124
    if (!m_texture)
        return;

    const unsigned int texelCount = m_textureSizeX * m_textureSizeY;
    unsigned int* const copy = static_cast<unsigned int*>(::renddx9abi::engineAlloc(4u * texelCount));
    std::memcpy(copy, m_lockInfo->pBits, 4u * texelCount);
    for (int pass = 3; pass != 0; --pass)
        lightmapDilate(copy, static_cast<unsigned int*>(m_lockInfo->pBits), m_textureSizeX,
                       m_textureSizeY);
    ::renddx9abi::engineFree(copy);
    if (dxt1)
        lightmapMoveAlphaToBlue(static_cast<unsigned int*>(m_lockInfo->pBits), m_textureSizeX,
                                m_textureSizeY);
    dx9rend::textureUnlock(m_texture, m_lockInfo);

    const int savedPool = dx9rend::textureManagerSwapManaged(kPoolManaged);
    const unsigned int* const dims = dx9rend::textureDims(m_texture);
    dx9rend::EngineTexture* const converted = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerCreateEmpty(dims[0], dx9rend::textureDims(m_texture)[1],
                                           dxt1 ? kFormatDXT1 : kFormatDXT5, 0, 0, 0));
    if (!converted)
        ::renddx9abi::logf("MeshLightSampling.cpp:162: Failed to create conversion texture");
    // The source level is fetched first; neither surface reference is released (shipped).
    IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_texture, 0, 0x7FFFFFFF);
    IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(converted, 0, 0x7FFFFFFF);
    D3DXLoadSurfaceFromSurface(destination, 0, 0, source, 0, 0, 0xFFFFFFFFu, 0);
    D3DXFilterTexture(converted->baseTexture, 0, 0, 0xFFFFFFFFu);
    dx9rend::textureManagerSaveTexture(converted, m_outputName);
    dx9rend::textureManagerSwapManaged(savedPool);
    releaseTexture(converted);
    releaseTexture(m_texture);
}

void MeshLightSampler::addRef() { ++m_refCount; }
int  MeshLightSampler::getRef() { return m_refCount; }

// Answers the count after the decrement; zero destroys and frees.
// PE 0x101BEF70
int MeshLightSampler::release()
{
    const int count = --m_refCount;
    if (count == 0)
    {
        this->~MeshLightSampler();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// The body the terrain sampler shares in the image: IBase (1) and IID_ILightSampling answer this.
// PE 0x101BECB0
void* MeshLightSampler::queryInterface(unsigned int iid) const
{
    MeshLightSampler* const self = const_cast<MeshLightSampler*>(this);
    if (iid == 1u)
        return self;
    return iid == kIIDLightSampling ? self : 0;
}

// PE 0x101BD310
unsigned int MeshLightSampler::getBlockCount()
{
    return 1;
}

// The present samples as the count, an empty dirty box and the triangle vector itself.
// PE 0x101BD320
LightSampleBlock MeshLightSampler::getBlock(unsigned int)
{
    LightSampleBlock result;
    result.sampleCount = m_realSampleCount;
    TerrainAabb empty;
    empty.min.x = 0.0f; empty.min.y = 0.0f; empty.min.z = 0.0f;
    empty.max.x = 0.0f; empty.max.y = 0.0f; empty.max.z = 0.0f;
    empty.cornerLength = 0.0f;
    empty.radius = 0.0f;
    empty.dirty = 1;
    empty.extent.x = 0.0f; empty.extent.y = 0.0f; empty.extent.z = 0.0f;
    empty.centre.x = 0.0f; empty.centre.y = 0.0f; empty.centre.z = 0.0f;
    result.bounds = empty;
    result.triangles = &m_triangles;
    return result;
}

// Continues from the cursor over the grid: NaN positions are zeroed in place, samples without a
// position are skipped, and each present one is copied (position, normal, triangle) into the next
// record of `out` with cleared results and done flag. The block index is ignored.
// PE 0x101BD3C0
unsigned int MeshLightSampler::getSamples(unsigned int, unsigned int maxCount, LightSample* out)
{
    m_batch = out;
    m_batchStart = m_nextSample;
    m_batchCount = 0;
    LightSample* sample = out;
    while (m_samples.data() && m_nextSample < m_samples.size() && maxCount != 0)
    {
        zeroNanPosition(m_samples[m_nextSample].position);
        const MeshSampleRecord& record = m_samples[m_nextSample];
        if (!hasPosition(record.position))
        {
            ++m_nextSample;
            continue;
        }
        sample->position = record.position;
        sample->normal = record.normal;
        sample->triangle = record.triangle;
        sample->result[3] = 0.0f;
        sample->result[2] = 0.0f;
        sample->result[1] = 0.0f;
        sample->result[0] = 0.0f;
        sample->done = 0;
        ++sample;
        --maxCount;
        ++m_batchCount;
        ++m_nextSample;
    }
    return m_batchCount;
}

// Packs the last batch into the lock texture at each result's sample index (skipping samples
// without a position, as getSamples did). Intensity mode (the generator's
// enableIntensityLightmaps): A = result[3], R/G/B = result[0..2]. Colour mode: R/G/B as before and
// A from the LightManager's lightmap alpha cutoff range - the sample's directly-lit flag when the
// range starts at 0, else the luminance 0.3 R + 0.59 G + 0.1 B (clamped to [0, 1]) mapped from the
// range onto [0, 1]. The range is read either way. Always true.
// PE 0x101BD580
bool MeshLightSampler::storeResults()
{
    unsigned int index = m_batchStart;
    for (unsigned int i = 0; i < m_batchCount; ++i, ++index)
    {
        while (!hasPosition(m_samples[index].position))
            ++index;
        unsigned int* const texels = static_cast<unsigned int*>(m_lockInfo->pBits);
        SampleReal alpha;
        if (g_LightManager->getLightMapAmbientAlphaCutoffRange().x == 0.0f)
        {
            alpha = static_cast<SampleReal>(m_batch[i].directlyLit);
        }
        else
        {
            const LightSample& sample = m_batch[i];
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            // [CHOSEN, not recovered] The luminance has one float argument store.
            const float luminance = clampValue(bf2x87::store(bf2x87::add(
                bf2x87::add(sampleProduct(sample.result[2], 0.1f),
                            sampleProduct(sample.result[1], 0.59f)),
                sampleProduct(sample.result[0], 0.3f))), 0.0f, 1.0f);
#else
            const float luminance = clampValue(
                sample.result[2] * 0.1f + sample.result[1] * 0.59f + sample.result[0] * 0.3f,
                0.0f, 1.0f);
#endif
            const float lowCutoff = g_LightManager->getLightMapAmbientAlphaCutoffRange().x;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const bool below = bf2x87::less(bf2x87::wide(luminance), bf2x87::wide(lowCutoff));
#else
            const bool below = luminance < lowCutoff;
#endif
            if (below)
            {
                alpha = 0.0f;
            }
            else
            {
                const float highCutoff = g_LightManager->getLightMapAmbientAlphaCutoffRange().y;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                const bool above = bf2x87::greaterEqual(bf2x87::wide(luminance), bf2x87::wide(highCutoff));
#else
                const bool above = luminance >= highCutoff;
#endif
                if (above)
                    alpha = 1.0f;
                else
                {
                    const Vec2& low = g_LightManager->getLightMapAmbientAlphaCutoffRange();
                    const Vec2& high = g_LightManager->getLightMapAmbientAlphaCutoffRange();
                    const Vec2& last = g_LightManager->getLightMapAmbientAlphaCutoffRange();
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                    // [CHOSEN, not recovered] No store between the quotient and channel conversion.
                    alpha = bf2x87::div(bf2x87::sub(bf2x87::wide(luminance), bf2x87::wide(low.x)),
                        bf2x87::sub(bf2x87::wide(high.y), bf2x87::wide(last.x)));
#else
                    alpha = (luminance - low.x) / (high.y - last.x);
#endif
                }
            }
        }

        const LightSample& sample = m_batch[i];
        if (g_LightmapGeneration->m_enableIntensityLightmaps)                 // +0x120
        {
            const unsigned int a = channelByte(sampleProduct(sample.result[3], 255.0f));
            const unsigned int r = channelByte(sampleProduct(sample.result[0], 255.0f));
            const unsigned int g = channelByte(sampleProduct(sample.result[1], 255.0f));
            const unsigned int b = channelByte(sampleProduct(sample.result[2], 255.0f));
            texels[index] = (a << 24) | (r << 16) | (g << 8) | b;
        }
        else
        {
            const unsigned int r = channelByte(sampleProduct(sample.result[0], 255.0f));
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const unsigned int a = channelByte(bf2x87::mul(alpha, bf2x87::wide(255.0f)));
#else
            const unsigned int a = channelByte(alpha * 255.0f);
#endif
            const unsigned int g = channelByte(sampleProduct(sample.result[1], 255.0f));
            const unsigned int b = channelByte(sampleProduct(sample.result[2], 255.0f));
            texels[index] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    return true;
}

// Appends (position, colour, range) and answers its index, or -1 while the vector has no storage.
// PE 0x101BDB90
int MeshLightSampler::addLight(Vec3 color, Vec3 position, float range)
{
    MeshSampleLight light;
    light.position = position;
    light.color = color;
    light.range = range;
    m_lights.push_back(light);
    if (!m_lights.data())
        return -1;
    return static_cast<int>(m_lights.size()) - 1;
}

// PE 0x101BECE0
void* MeshLightSampler::getLights()
{
    return &m_lights;
}

// PE 0x101BDC10
void MeshLightSampler::halveSampleGrid()
{
    m_textureSizeX >>= 1;
    m_textureSizeY >>= 1;
    for (unsigned int y = 0; y < m_textureSizeY; ++y)
    {
        for (unsigned int x = 0; x < m_textureSizeX; ++x)
        {
            const unsigned int row = y * m_textureSizeX;
            m_samples[x + row] = m_samples[2u * (x + 2u * row)];
        }
    }
    m_samples.resize(m_textureSizeX * m_textureSizeY);
}

// PE 0x101BDCD0
bool MeshLightSampler::load(const std::string& path, const std::string& outputName)
{
    m_outputName = outputName;
    ::renddx9abi::EngineStream file;
    if (!file.open(path, 2, 0, 0))
        return false;

    // The magic is compared as a five-byte string; a short read leaves the shipped stack bytes
    // undefined, so they start zeroed here. [CHOSEN, not recovered] the zero fill.
    char magic[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    file.read(magic, 4);
    int version;
    if (std::strcmp(magic, "SMP1") == 0)
        version = 1;
    else if (std::strcmp(magic, "SMP2") == 0)
        version = 2;
    else
    {
        file.seek(0, 0);
        version = 0;
    }
    file.read(&m_textureSizeX, 4);
    file.read(&m_textureSizeY, 4);
    m_samples.resize(m_textureSizeX * m_textureSizeY);

    if (version != 0)
    {
        file.read(m_samples.data(), 28u * m_samples.size());
        unsigned int triangleCount;
        file.read(&triangleCount, 4);
        unsigned int usedWidth = 0;
        unsigned int lastRow = 0;
        for (unsigned int y = 0; y < m_textureSizeY; ++y)
        {
            if (m_textureSizeX == 0)
                continue;
            unsigned int rowLast = 0;
            for (unsigned int x = 0; x < m_textureSizeX; ++x)
            {
                MeshSampleRecord& record = m_samples[x + y * m_textureSizeX];
                if (record.triangle < triangleCount)
                {
                    rowLast = x;
                    lastRow = y;
                }
                else
                {
                    record.triangle = 0xFFFFFFFFu;
                }
            }
            if (rowLast > usedWidth)
                usedWidth = rowLast;
        }
        m_usedWidth = usedWidth;
        m_usedHeight = lastRow;

        m_triangles.resize(triangleCount);
        for (unsigned int t = 0; t < triangleCount; ++t)
        {
            m_triangles[t].unresolved54 = -1;
            m_triangles[t].unresolved58 = -1;
            for (int corner = 0; corner < 3; ++corner)
            {
                file.read(&m_triangles[t].position[corner], 12);
                file.read(&m_triangles[t].normal[corner], 12);
            }
        }
    }
    else
    {
        for (unsigned int i = 0; i < m_textureSizeX * m_textureSizeY; ++i)
        {
            file.read(&m_samples[i], 24);
            m_samples[i].triangle = 0xFFFFFFFFu;
        }
    }

    if (m_sizeCap > 0 && m_sizeCap < static_cast<int>(m_textureSizeX))
    {
        do
            halveSampleGrid();
        while (m_sizeCap < static_cast<int>(m_textureSizeX));
    }
    while (m_resolutionModifier < 1.0f)
    {
        halveSampleGrid();
        m_resolutionModifier = m_resolutionModifier + m_resolutionModifier;
    }
    file.close();

    m_realSampleCount = 0;
    const unsigned int count = m_textureSizeY * m_textureSizeX;
    for (unsigned int i = 0; i < count; ++i)
    {
        zeroNanPosition(m_samples[i].position);
        if (hasPosition(m_samples[i].position))
            ++m_realSampleCount;
    }
    m_nextSample = 0;
    m_batchStart = 0;

    const int savedPool = dx9rend::textureManagerSwapManaged(kPoolScratch);
    m_texture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
        m_textureSizeX, m_textureSizeY, kFormatA8R8G8B8, 1, 0, 0));
    dx9rend::textureManagerSwapManaged(savedPool);
    do
    {
        m_lockInfo = dx9rend::textureLock(m_texture, 0, 0x7FFFFFFF);
    }
    while (!m_lockInfo);
    std::memset(m_lockInfo->pBits, 0, 4u * m_textureSizeY * m_textureSizeX);
    return true;
}


// Each texel is rebuilt as 0xFF000000 | R << 16 | G << 8 | A from its own bytes; an empty row only
// advances the cursor.
// PE 0x101BD100
void lightmapMoveAlphaToBlue(unsigned int* texels, unsigned int width, unsigned int height)
{
    if (height == 0)
        return;
    unsigned char* row = reinterpret_cast<unsigned char*>(texels);
    for (unsigned int remaining = height; remaining != 0; --remaining)
    {
        unsigned char* texel = row;
        for (unsigned int x = width; x != 0; --x)
        {
            unsigned int value = 0xFFFFFF00u | texel[2];                      // R
            value = (value << 8) | texel[1];                                  // G
            value = (value << 8) | texel[3];                                  // the old alpha
            *reinterpret_cast<unsigned int*>(texel) = value;
            texel += 4;
        }
        row += width * 4u;
    }
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
