// RainManager, recovered from RendDX9.dll 1.50 (0x10157B30..0x1015A580 and its factory 0x100B0C25).
// Attested source path: RendDX9_r's __FILE__ for Geom/Rain/RainManager.cpp (docs/tracks/rend/
// rdebug-findings.md). The vector operations 0x10159F90..0x1015A389 and 0x1015A4D0 are the
// std::vector<RainParticle> instantiation behind m_particles.resize().

#ifdef BF2_IMAGE_RENDDX9

#include "../../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#include "../../RendDX9/RendDX9Math.h"   // rendmath::normalize (sub_100777A0)
#include "RainManager.h"
#include "../../Math/Mat4.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/ShaderParameter.h"
#include "../../RendDX9/Texture.h"
#include "../../RendMisc/IRenderView.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/PluginEntry.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_shaderManager;      // dword_1023B97C
// BufferManager.cpp / Renderer seams, declared as LightningManager.cpp declares them.
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);   // sub_10011FF0
void  vertexBufferRelease(void* wrapper);                           // inline --ref, sub_10014510, free
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);                         // sub_10014630
bool  vertexBufferUnlock(void* wrapper);                            // sub_10014690
void  vertexBufferBindStream(void* wrapper, unsigned int stream);   // sub_100146C0
void  vertexDeclarationBind(void* wrapper);                         // sub_100147C0
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);              // sub_10005870
} // namespace dx9rend

namespace geom {

// StaticMesh.cpp - the shared PE 0x10077A30 product (output = left * right).
void geometryRendererMatrixProduct(Mat4& output, const Mat4& left, const Mat4& right);

namespace {

const unsigned int kIID_IBase        = 1u;
const unsigned int kIID_IRainManager = 0xB801DD07u;   // dword_1021AEA8

// The box drawn per cell in render mode 2: twelve edges of two vertices.
const unsigned int kCellBoxVertexCount = 24;

// dword_1021E6D8: the primitive type per render mode, which is also the vertex count of one
// primitive (render() advances the start vertex by type * primitives).
const unsigned int kRenderModePrimitiveType[3] = { 1u /* D3DPT_POINTLIST */,
                                                   2u /* D3DPT_LINELIST */,
                                                   2u /* D3DPT_LINELIST */ };

// The point-sprite render states setShaderConstants sets through the state manager (its
// ID3DXEffectStateManager::SetRenderState slot +0x1C). The D3D9 subset header carries no
// point-sprite enumerators, so the values are spelled here.
const unsigned int kRenderStatePointSpriteEnable = 156;   // D3DRS_POINTSPRITEENABLE
const unsigned int kRenderStatePointScaleEnable  = 157;   // D3DRS_POINTSCALEENABLE
const unsigned int kRenderStatePointSizeMin      = 155;   // D3DRS_POINTSIZE_MIN
const unsigned int kRenderStatePointSizeMax      = 166;   // D3DRS_POINTSIZE_MAX

// flt_10239E60, .data, pi / 180. Its only reader is updateDeviations. [INFERRED] name.
float g_rainDegreesToRadians = 0.0174532924f;

// The rain vertex, 16 bytes: the stride createVertexBuffer asks for. The last two bytes are
// never written by any shipped builder.
struct RainVertex
{
    float         x, y, z;       // +0x00  cell-local position
    unsigned char cell;          // +0x0C  cell % 32, the CELLPOSITIONS index of its draw batch
    unsigned char group;         // +0x0D  the deviation group
    unsigned char unused[2];     // +0x0E
};

// unk_1021E57C: POSITION float3 at 0, COLOR ubyte4 at 12.
const D3DVERTEXELEMENT9 kRainVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_UBYTE4, 0, D3DDECLUSAGE_COLOR,    0 },
    { 0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0 }
};

// fistp under a chop control word.
inline int truncateToInt(float value)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return bf2x87::toIntTruncate(value);
#else
    return static_cast<int>(value);
#endif
}

// The bit pattern of a float render-state value.
inline unsigned int floatBits(float value)
{
    unsigned int bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Mat4 copy (0x10030DD0) followed by the product into the original: `m = m * rhs`.
void multiplyInPlace(Mat4& m, const Mat4& rhs)
{
    const Mat4 left = m;
    geometryRendererMatrixProduct(m, left, rhs);
}

inline void writeVertex(RainVertex* vertex, float x, float y, float z, unsigned char cell,
                        unsigned char group)
{
    vertex->x = x;
    vertex->y = y;
    vertex->z = z;
    vertex->cell = cell;
    vertex->group = group;
}

} // anonymous namespace


// Construction - 0x10159D30. The effect and the declaration are created at once; the particle
// buffer only when a particle count is set.
RainManager::RainManager()
    : m_refCount(1), m_effect(0), m_vertexDeclaration(0), m_vertexBuffer(0),
      m_vertexBufferCount(0), m_texture(0), m_paramWorldViewProj(0), m_paramParticleColor(0),
      m_paramTexture(0), m_paramCameraPos(0), m_paramFadeOutRange(0), m_paramFadeOutDelta(0),
      m_paramCellPositions(0), m_paramDeviationGroups(0), m_paramParticleSize(0),
      m_paramParticleMaxSize(0), m_paramPointScale(0), m_time(0.0), m_enabled(false),
      m_cellCenterFrozen(false), m_renderMode(0), m_particleSize(8.0f),
      m_particleMaxSize(12.0f), m_fallSpeed(3.5f), m_fadeOutFactor(0.875f), m_lineLength(3.0f),
      m_particles()
{
    m_cellCenter.x = m_cellCenter.y = m_cellCenter.z = m_cellCenter.w = 0.0f;
    m_cameraOffset.x = m_cameraOffset.y = m_cameraOffset.z = 0.0f;
    m_cellExtents.x = 40.0f;
    m_cellExtents.y = 20.0f;
    m_cellExtents.z = 40.0f;
    m_pointScale.x = 1.0f;
    m_pointScale.y = 0.0f;
    m_pointScale.z = 0.0f;
    m_particleColor.x = 1.0f;
    m_particleColor.y = 1.0f;
    m_particleColor.z = 1.0f;
    m_particleColor.w = 0.5f;
    m_fallDirection.x = 0.0f;
    m_fallDirection.y = -1.0f;
    m_fallDirection.z = 0.0f;
    for (int i = 0; i < kCellCount; ++i)
        m_cells[i].x = m_cells[i].y = m_cells[i].z = m_cells[i].w = 0.0f;
    m_deviationSpeed.x = m_deviationSpeed.y = 0.0f;
    m_deviationAmplitude.x = m_deviationAmplitude.y = m_deviationAmplitude.z = 0.0f;
    for (int i = 0; i < kDeviationCount; ++i)
    {
        m_deviationOffsets[i].x = m_deviationOffsets[i].y = 0.0f;
        m_deviationOffsets[i].z = m_deviationOffsets[i].w = 0.0f;
    }
    // m_frameTime, m_cellPrimitiveCounts and m_deviations stay unwritten, as shipped: update()
    // stores the frame time before anything reads it, and setEnabled() fills the counts (through
    // setRenderMode) and the deviation groups before render() can use them.
    initResources();
}

// 0x10159EB0 (0x1015A580 deleting). The texture reference is not released: releaseResources
// leaves m_texture alone, and only setTexture drops it - shipped leak.
RainManager::~RainManager()
{
    releaseResources();
}

void RainManager::operator delete(void* memory)
{
    ::renddx9abi::engineFree(memory);
}


// The IBase quartet.

void RainManager::addRef()
{
    ++m_refCount;
}

int RainManager::getRef()
{
    return m_refCount;
}

// sub_1015A4B0 (shared): at zero, the deleting destructor (slot +0x10, flag 1).
int RainManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// IBase (1) and IID_IRainManager answer `this`.
// PE 0x1015A480.
void* RainManager::queryInterface(unsigned int iid) const
{
    if (iid == kIID_IBase)
        return const_cast<RainManager*>(this);
    return iid != kIID_IRainManager ? 0 : const_cast<RainManager*>(this);
}


// PE 0x101580D0. No technique is selected here; setRenderMode selects one.
bool RainManager::initResources()
{
    m_effect = static_cast<dx9rend::Effect*>(
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "Shaders/Rain"));
    if (!m_effect)
        return false;
    lookupShaderParameters();
    m_vertexDeclaration = dx9rend::vertexDeclarationCreate(kRainVertexElements);
    return m_vertexDeclaration != 0;
}

// PE 0x10157B40, in this order.
void RainManager::lookupShaderParameters()
{
    m_paramWorldViewProj = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("WORLDVIEWPROJ"));
    m_paramParticleColor = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("PARTICLECOLOR"));
    m_paramTexture = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXTURE"));
    m_paramCameraPos = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("CAMERAPOS"));
    m_paramFadeOutRange = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("FADEOUTRANGE"));
    m_paramFadeOutDelta = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("FADEOUTDELTA"));
    m_paramCellPositions = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("CELLPOSITIONS"));
    m_paramDeviationGroups = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("DEVIATIONGROUPS"));
    m_paramParticleSize = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("PARTICLESIZE"));
    m_paramParticleMaxSize = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("PARTICLEMAXSIZE"));
    m_paramPointScale = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("POINTSCALE"));
}

// PE 0x10157CF0, in the shipped order; every slot is nulled.
void RainManager::releaseShaderParameters()
{
    if (m_paramPointScale)
        m_paramPointScale->release();
    m_paramPointScale = 0;
    if (m_paramParticleSize)
        m_paramParticleSize->release();
    m_paramParticleSize = 0;
    if (m_paramParticleMaxSize)
        m_paramParticleMaxSize->release();
    m_paramParticleMaxSize = 0;
    if (m_paramDeviationGroups)
        m_paramDeviationGroups->release();
    m_paramDeviationGroups = 0;
    if (m_paramCellPositions)
        m_paramCellPositions->release();
    m_paramCellPositions = 0;
    if (m_paramFadeOutRange)
        m_paramFadeOutRange->release();
    m_paramFadeOutRange = 0;
    if (m_paramFadeOutDelta)
        m_paramFadeOutDelta->release();
    m_paramFadeOutDelta = 0;
    if (m_paramCameraPos)
        m_paramCameraPos->release();
    m_paramCameraPos = 0;
    if (m_paramTexture)
        m_paramTexture->release();
    m_paramTexture = 0;
    if (m_paramParticleColor)
        m_paramParticleColor->release();
    m_paramParticleColor = 0;
    if (m_paramWorldViewProj)
        m_paramWorldViewProj->release();
    m_paramWorldViewProj = 0;
}

// PE 0x10158140. The parameters, the buffer, the declaration, then the effect.
void RainManager::releaseResources()
{
    releaseShaderParameters();
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    m_vertexBuffer = 0;
    if (m_vertexDeclaration)
        dx9rend::vertexDeclarationRelease(m_vertexDeclaration);
    m_vertexDeclaration = 0;
    if (m_effect)
        dx9rend::effectRelease(m_effect);
    m_effect = 0;
}

// PE 0x10158220. A zero count leaves no buffer and still succeeds; a failed creation also
// clears the stored count.
bool RainManager::createVertexBuffer(unsigned int count)
{
    if (m_vertexBuffer)
    {
        dx9rend::vertexBufferRelease(m_vertexBuffer);
        m_vertexBuffer = 0;
    }
    m_vertexBufferCount = count;
    if (count)
    {
        m_vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
            count, sizeof(RainVertex), D3DUSAGE_WRITEONLY, "RainManager");
        if (!m_vertexBuffer)
        {
            m_vertexBufferCount = 0;
            return false;
        }
    }
    return true;
}

// PE 0x101582D0: the low corner of one cell around its centre.
Vec3 RainManager::getCellMin() const
{
    const float sizeX = m_cellExtents.x + m_cellExtents.x;
    const float sizeY = m_cellExtents.y + m_cellExtents.y;
    const float sizeZ = m_cellExtents.z + m_cellExtents.z;
    Vec3 corner;
    corner.x = sizeX * 0.2f * -0.5f;
    corner.y = sizeY * 0.2f * -0.5f;
    corner.z = sizeZ * 0.2f * -0.5f;
    return corner;
}

// PE 0x10158330: the high corner of one cell around its centre.
Vec3 RainManager::getCellMax() const
{
    const float sizeX = m_cellExtents.x + m_cellExtents.x;
    const float sizeY = m_cellExtents.y + m_cellExtents.y;
    const float sizeZ = m_cellExtents.z + m_cellExtents.z;
    Vec3 corner;
    corner.x = sizeX * 0.2f * 0.5f;
    corner.y = sizeY * 0.2f * 0.5f;
    corner.z = sizeZ * 0.2f * 0.5f;
    return corner;
}

// PE 0x10158390. Each axis in steps of 0.001, drawing x, y, z in that order. An axis spanning
// less than 0.001 divides by zero, as shipped.
Vec3 RainManager::randomPointInBox(const Vec3& min, const Vec3& max) const
{
    const float spanX = max.x - min.x;
    const float spanY = max.y - min.y;
    const float spanZ = max.z - min.z;
    Vec3 point;
    {
        const int r = std::rand();
        point.x = static_cast<float>(r % truncateToInt(spanX * 1000.0f)) * 0.001f + min.x;
    }
    {
        const int r = std::rand();
        point.y = static_cast<float>(r % truncateToInt(spanY * 1000.0f)) * 0.001f + min.y;
    }
    {
        const int r = std::rand();
        point.z = static_cast<float>(r % truncateToInt(spanZ * 1000.0f)) * 0.001f + min.z;
    }
    return point;
}

// PE 0x10158490. The box is the cell (both corners inlined); the group is drawn last.
void RainManager::initParticle(RainParticle& particle, int cell)
{
    particle.position = randomPointInBox(getCellMin(), getCellMax());
    particle.cell = cell;
    particle.group = std::rand() % 16;
}

// PE 0x10159270. Particles are spread evenly over the cells; the cells form a 6 x 6 x 6 grid
// around the cell centre, x varying fastest, then z, then y. Each cell keeps the centre's w.
void RainManager::initParticles()
{
    for (unsigned int i = 0; i < m_particles.size(); ++i)
        initParticle(m_particles[i],
                     static_cast<int>(i * static_cast<unsigned int>(kCellCount)
                                      / m_particles.size()));

    const float spanX = (m_cellExtents.x + m_cellExtents.x) * 0.2f;
    const float spanY = (m_cellExtents.y + m_cellExtents.y) * 0.2f;
    const float spanZ = (m_cellExtents.z + m_cellExtents.z) * 0.2f;
    for (unsigned int j = 0; j < static_cast<unsigned int>(kCellCount); ++j)
    {
        const float x = (static_cast<float>(j % 6) - 2.5f) * spanX;
        const float y = (static_cast<float>(j / 36 % 6) - 2.5f) * spanY;
        const float z = (static_cast<float>(j / 6 % 6) - 2.5f) * spanZ;
        Vec4& cell = m_cells[j];
        cell.w = m_cellCenter.w;
        cell.z = z + m_cellCenter.z;
        cell.x = x + m_cellCenter.x;
        cell.y = y + m_cellCenter.y;
    }
}

// PE 0x10157DB0. Two random angles in tenths of a degree, then a random fraction of each
// non-zero amplitude axis (a zero axis draws no number).
void RainManager::initDeviations()
{
    const float* const amplitude = &m_deviationAmplitude.x;
    for (int i = 0; i < kDeviationCount; ++i)
    {
        RainDeviation& deviation = m_deviations[i];
        deviation.angle[0] = static_cast<float>(std::rand() % 3600) * 0.1f;
        deviation.angle[1] = static_cast<float>(std::rand() % 3600) * 0.1f;
        float* const out = &deviation.amplitude.x;
        for (int k = 0; k < 3; ++k)
        {
            if (amplitude[k] != 0.0f)
                out[k] = static_cast<float>(std::rand() % 1000) / (1000.0f / amplitude[k]);
            else
                out[k] = 0.0f;
        }
    }
}

// PE 0x10157E70. Publishes each group's offset from its current angles, then advances the angles
// by deviationSpeed degrees per second, wrapping only above 360.
void RainManager::updateDeviations()
{
    for (int i = 0; i < kDeviationCount; ++i)
    {
        RainDeviation& deviation = m_deviations[i];
        Vec4& offset = m_deviationOffsets[i];
        const float angle0 = g_rainDegreesToRadians * deviation.angle[0];
        const float angle1 = g_rainDegreesToRadians * deviation.angle[1];
        offset.w = 0.0f;
        offset.x = std::cos(angle0) * deviation.amplitude.x;
        offset.y = std::sin(angle1) * deviation.amplitude.y;
        offset.z = std::sin(angle0) * deviation.amplitude.z;

        const float next0 = static_cast<float>(m_frameTime * m_deviationSpeed.x + deviation.angle[0]);
        deviation.angle[0] = next0;
        if (next0 > 360.0f)
            deviation.angle[0] = next0 - 360.0f;
        const float next1 = static_cast<float>(m_frameTime * m_deviationSpeed.y + deviation.angle[1]);
        deviation.angle[1] = next1;
        if (next1 > 360.0f)
            deviation.angle[1] = next1 - 360.0f;
    }
}

// PE 0x10158580 (unrolled nine times): every cell falls by direction * speed * frame time. The
// fourth component is advanced by speed * 0, as shipped.
void RainManager::moveCells()
{
    for (int i = 0; i < kCellCount; ++i)
    {
        const float frameTime = static_cast<float>(m_frameTime);
        Vec4& cell = m_cells[i];
        cell.x += m_fallDirection.x * m_fallSpeed * frameTime;
        cell.y += m_fallDirection.y * m_fallSpeed * frameTime;
        cell.z += m_fallDirection.z * m_fallSpeed * frameTime;
        cell.w += m_fallSpeed * 0.0f * frameTime;
    }
}

// PE 0x10158A00. A cell that left the volume around the centre (extent plus half a cell on each
// side) moves by whole volume-plus-cell spans until it is back inside. A zero span with a cell
// outside never terminates, as shipped.
void RainManager::wrapCells()
{
    const float* const extents = &m_cellExtents.x;
    float size[3];
    size[0] = extents[0] + extents[0];
    size[1] = extents[1] + extents[1];
    size[2] = extents[2] + extents[2];
    float span[3];
    span[0] = size[0] * 0.2f;
    span[1] = size[1] * 0.2f;
    span[2] = size[2] * 0.2f;

    for (int i = 0; i < kCellCount; ++i)
    {
        Vec4& cell = m_cells[i];
        float relative[4];
        relative[0] = cell.x - m_cellCenter.x;
        relative[1] = cell.y - m_cellCenter.y;
        relative[2] = cell.z - m_cellCenter.z;
        relative[3] = cell.w - m_cellCenter.w;
        for (int k = 0; k < 3; ++k)
        {
            const float half = span[k] * 0.5f;
            const float upper = half + extents[k];
            if (relative[k] > upper)
            {
                const float step = size[k] + span[k];
                float value = relative[k];
                do
                    value -= step;
                while (value > upper);
                relative[k] = value;
            }
            const float lower = -extents[k] - half;
            if (relative[k] < lower)
            {
                const float step = size[k] + span[k];
                float value = relative[k];
                do
                    value += step;
                while (value < lower);
                relative[k] = value;
            }
        }
        cell.x = relative[0] + m_cellCenter.x;
        cell.y = relative[1] + m_cellCenter.y;
        cell.z = relative[2] + m_cellCenter.z;
        cell.w = relative[3] + m_cellCenter.w;
    }
}

// PE 0x10157F40. Nothing in the image calls it; render() sets the same parameter inline.
void RainManager::setCellPositions(int first, int count)
{
    m_paramCellPositions->setValue(&m_cells[first], static_cast<unsigned int>(count));
}

// PE 0x10158E30. Without a texture nothing is set and the draw is skipped. The world is the
// identity, so WORLDVIEWPROJ is the view-projection.
bool RainManager::setShaderConstants()
{
    if (!m_texture)
        return false;

    dx9rend::stateManagerSetRenderState(kRenderStatePointSizeMin, floatBits(0.0f));
    dx9rend::stateManagerSetRenderState(kRenderStatePointSizeMax, floatBits(64.0f));
    dx9rend::stateManagerSetRenderState(kRenderStatePointScaleEnable, 1);
    dx9rend::stateManagerSetRenderState(kRenderStatePointSpriteEnable, 1);

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();   // Renderer +0x50
    Mat4 worldViewProj = Mat4::getIdentity();
    multiplyInPlace(worldViewProj, *view->getViewProjectionMatrixPtr());       // +0x84
    m_paramWorldViewProj->setValue(&worldViewProj, 0);
    m_paramParticleColor->setValue(&m_particleColor, 0);
    m_paramParticleSize->setFloat(m_particleSize);
    m_paramParticleMaxSize->setFloat(m_particleMaxSize);
    m_paramPointScale->setValue(&m_pointScale, 0);
    m_paramTexture->setValue(m_texture, 0);
    m_paramCameraPos->setValue(&m_cellCenter, 0);

    Vec3 fadeOutRange;
    fadeOutRange.x = m_fadeOutFactor * m_cellExtents.x;
    fadeOutRange.y = m_fadeOutFactor * m_cellExtents.y;
    fadeOutRange.z = m_fadeOutFactor * m_cellExtents.z;
    Vec3 fadeOutDelta;
    fadeOutDelta.x = m_cellExtents.x - fadeOutRange.x;
    fadeOutDelta.y = m_cellExtents.y - fadeOutRange.y;
    fadeOutDelta.z = m_cellExtents.z - fadeOutRange.z;
    m_paramFadeOutRange->setValue(&fadeOutRange, 0);
    m_paramFadeOutDelta->setValue(&fadeOutDelta, 0);
    m_paramDeviationGroups->setValue(m_deviationOffsets, kDeviationCount);
    return true;
}

// PE 0x10159BC0. Rebuilds the vertex buffer for the mode and selects its technique; the mode is
// stored even when it is none of the three. The line arm creates its buffer twice, as shipped.
void RainManager::setRenderMode(int mode)
{
    switch (mode)
    {
    case 0:
        buildPointBuffer();
        dx9rend::effectSetTechniqueByName(m_effect, std::string("Point"));
        break;
    case 1:
        createVertexBuffer(static_cast<unsigned int>(m_particles.size() + m_particles.size()));
        buildLineBuffer();
        dx9rend::effectSetTechniqueByName(m_effect, std::string("Line"));
        break;
    case 2:
        buildCellBuffer();
        dx9rend::effectSetTechniqueByName(m_effect, std::string("Cells"));
        break;
    default:
        break;
    }
    m_renderMode = mode;
}

// PE 0x10159960. One point per particle; each cell counts its particles.
void RainManager::buildPointBuffer()
{
    std::memset(m_cellPrimitiveCounts, 0, sizeof(m_cellPrimitiveCounts));
    createVertexBuffer(static_cast<unsigned int>(m_particles.size()));
    if (!m_vertexBuffer)
        return;
    RainVertex* vertex = static_cast<RainVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, 0, 0, D3DLOCK_DISCARD));
    for (unsigned int i = 0; i < m_particles.size(); ++i)
    {
        const RainParticle& particle = m_particles[i];
        writeVertex(vertex, particle.position.x, particle.position.y, particle.position.z,
                    static_cast<unsigned char>(particle.cell % kCellsPerDraw),
                    static_cast<unsigned char>(particle.group));
        ++m_cellPrimitiveCounts[particle.cell];
        ++vertex;
    }
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
}

// PE 0x10159A50. One streak per particle, from the particle back along the fall direction.
void RainManager::buildLineBuffer()
{
    std::memset(m_cellPrimitiveCounts, 0, sizeof(m_cellPrimitiveCounts));
    createVertexBuffer(static_cast<unsigned int>(m_particles.size() + m_particles.size()));
    if (!m_vertexBuffer)
        return;
    RainVertex* vertex = static_cast<RainVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, 0, 0, D3DLOCK_DISCARD));
    for (unsigned int i = 0; i < m_particles.size(); ++i)
    {
        const RainParticle& particle = m_particles[i];
        writeVertex(vertex, particle.position.x, particle.position.y, particle.position.z,
                    static_cast<unsigned char>(particle.cell % kCellsPerDraw),
                    static_cast<unsigned char>(particle.group));
        ++vertex;
        const float streakX = m_lineLength * m_fallDirection.x;
        const float streakY = m_lineLength * m_fallDirection.y;
        const float streakZ = m_lineLength * m_fallDirection.z;
        writeVertex(vertex, particle.position.x - streakX, particle.position.y - streakY,
                    particle.position.z - streakZ,
                    static_cast<unsigned char>(particle.cell % kCellsPerDraw),
                    static_cast<unsigned char>(particle.group));
        ++m_cellPrimitiveCounts[particle.cell];
        ++vertex;
    }
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
}

// PE 0x10158B80. The twelve edges of one cell box for each of the 216 cells, every cell
// counting twelve lines: per y, the two z edges then the two x edges at each x / z; then the
// four y edges.
void RainManager::buildCellBuffer()
{
    if (!createVertexBuffer(kCellCount * kCellBoxVertexCount))
        return;
    RainVertex* vertex = static_cast<RainVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, 0, 0, D3DLOCK_DISCARD));
    const Vec3 cellMin = getCellMin();
    const Vec3 cellMax = getCellMax();
    Vec3 corners[2];
    corners[0] = cellMin;
    corners[1] = cellMax;

    for (int i = 0; i < kCellCount; ++i)
    {
        m_cellPrimitiveCounts[i] = 12;
        const unsigned char index = static_cast<unsigned char>(i % kCellsPerDraw);
        for (int j = 0; j < 2; ++j)
        {
            const float y = corners[j].y;
            for (int k = 0; k < 2; ++k)
            {
                writeVertex(vertex++, corners[k].x, y, cellMin.z, index, 0);
                writeVertex(vertex++, corners[k].x, y, cellMax.z, index, 0);
            }
            for (int k = 0; k < 2; ++k)
            {
                writeVertex(vertex++, cellMin.x, y, corners[k].z, index, 0);
                writeVertex(vertex++, cellMax.x, y, corners[k].z, index, 0);
            }
        }
        for (int j = 0; j < 2; ++j)
        {
            for (int k = 0; k < 2; ++k)
            {
                writeVertex(vertex++, corners[k].x, cellMin.y, corners[j].z, index, 0);
                writeVertex(vertex++, corners[k].x, cellMax.y, corners[j].z, index, 0);
            }
        }
    }
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
}


// PE 0x10157B30. The particle count goes through the virtual setter.
void RainManager::reset()
{
    setParticleCount(0);
    m_enabled = false;
}

// PE 0x10158DA0. Unless frozen, the cell centre follows the camera (plus the offset) even while
// disabled; the w component is kept.
void RainManager::update(double deltaTime)
{
    if (!m_cellCenterFrozen)
    {
        dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();   // Renderer +0x50
        const Mat4* const camera = view->getViewMatrixPtr();                       // +0x64
        m_cellCenter.x = camera->m[12];
        m_cellCenter.y = camera->m[13];
        m_cellCenter.z = camera->m[14];
        m_cellCenter.x = m_cameraOffset.x + m_cellCenter.x;
        m_cellCenter.y = m_cameraOffset.y + m_cellCenter.y;
        m_cellCenter.z = m_cameraOffset.z + m_cellCenter.z;
    }
    if (m_enabled)
    {
        m_frameTime = deltaTime;
        m_time = deltaTime + m_time;
        moveCells();
        updateDeviations();
        wrapCells();
    }
}

// PE 0x10159030. The cells go to the shader 32 at a time (CELLPOSITIONS); each batch draws the
// primitives its cells counted, in buffer order.
void RainManager::render()
{
    if (!m_enabled)
        return;
    dx9rend::stateManagerPush();
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    if (setShaderConstants())
    {
        dx9rend::effectBegin(m_effect);
        dx9rend::effectBeginPass(m_effect, 0);
        unsigned int startVertex = 0;
        int remaining = kCellCount;
        const int* counts = m_cellPrimitiveCounts;
        const Vec4* cells = m_cells;
        do
        {
            const int batch = remaining < kCellsPerDraw ? remaining : kCellsPerDraw;
            m_paramCellPositions->setValue(cells, static_cast<unsigned int>(batch));
            unsigned int primitives = 0;
            for (int i = 0; i < batch; ++i)
                primitives += static_cast<unsigned int>(counts[i]);
            dx9rend::rendererDrawPrimitive(primitives, kRenderModePrimitiveType[m_renderMode],
                                           startVertex);
            startVertex += kRenderModePrimitiveType[m_renderMode] * primitives;
            cells += kCellsPerDraw;
            counts += kCellsPerDraw;
            remaining -= kCellsPerDraw;
        }
        while (remaining > 0);
        dx9rend::effectEnd(m_effect);
    }
    dx9rend::stateManagerPop();
}


// The properties.

// PE 0x10159D00. Enabling re-scatters the particles, rolls the deviations and rebuilds the
// buffer for the current mode; disabling keeps everything.
void RainManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (enabled)
    {
        initParticles();
        initDeviations();
        setRenderMode(m_renderMode);
    }
}
bool RainManager::getEnabled() const { return m_enabled; }

// PE 0x101581B0.
void RainManager::setTexture(const std::string& name)
{
    if (m_texture)
        dx9rend::textureReleaseRef(m_texture);
    m_texture = 0;
    if (!name.empty())
        m_texture = static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerGetTexture(std::string(name.c_str())));
}

// PE 0x10159F00. VC7.1's clear() frees the storage (the listing's free plus three zeroed
// pointers).
void RainManager::setParticleCount(unsigned int count)
{
    if (createVertexBuffer(count))
        m_particles.resize(count);
    else
        m_particles.clear();
}
unsigned int RainManager::getParticleCount() const
{
    return static_cast<unsigned int>(m_particles.size());
}

// PE 0x10159240. Freezes the centre first.
void RainManager::setCellCenter(const Vec4& center)
{
    m_cellCenterFrozen = true;
    m_cellCenter.x = center.x;
    m_cellCenter.y = center.y;
    m_cellCenter.z = center.z;
    m_cellCenter.w = center.w;
}
const Vec4& RainManager::getCellCenter() const { return m_cellCenter; }

// PE 0x10159220.
void RainManager::setCameraOffset(const Vec3& offset)
{
    m_cameraOffset.x = offset.x;
    m_cameraOffset.y = offset.y;
    m_cameraOffset.z = offset.z;
}
const Vec3& RainManager::getCameraOffset() const { return m_cameraOffset; }

// PE 0x10159140.
void RainManager::setCellExtents(const Vec3& extents)
{
    m_cellExtents.x = extents.x;
    m_cellExtents.y = extents.y;
    m_cellExtents.z = extents.z;
}
const Vec3& RainManager::getCellExtents() const { return m_cellExtents; }

// PE 0x101591C0.
void RainManager::setPointScale(const Vec3& scale)
{
    m_pointScale.x = scale.x;
    m_pointScale.y = scale.y;
    m_pointScale.z = scale.z;
}
const Vec3& RainManager::getPointScale() const { return m_pointScale; }

// PE 0x10159170.
void RainManager::setParticleColor(const Vec4& color)
{
    m_particleColor.x = color.x;
    m_particleColor.y = color.y;
    m_particleColor.z = color.z;
    m_particleColor.w = color.w;
}
const Vec4& RainManager::getParticleColor() const { return m_particleColor; }

// PE 0x10157FA0.
void RainManager::setParticleSize(float size) { m_particleSize = size; }
float RainManager::getParticleSize() const { return m_particleSize; }

// PE 0x101580B0.
void RainManager::setParticleMaxSize(float size) { m_particleMaxSize = size; }
float RainManager::getParticleMaxSize() const { return m_particleMaxSize; }

// PE 0x101591F0.
void RainManager::setFallDirection(const Vec3& direction)
{
    m_fallDirection.x = direction.x;
    m_fallDirection.y = direction.y;
    m_fallDirection.z = direction.z;
    rendmath::normalize(m_fallDirection);
}
const Vec3& RainManager::getFallDirection() const { return m_fallDirection; }

// PE 0x10157FC0.
void RainManager::setFallSpeed(float speed) { m_fallSpeed = speed; }
float RainManager::getFallSpeed() const { return m_fallSpeed; }

// PE 0x10157FE0.
void RainManager::setFadeOutFactor(float factor) { m_fadeOutFactor = factor; }
float RainManager::getFadeOutFactor() const { return m_fadeOutFactor; }

// PE 0x10158B50.
void RainManager::setDeviationAmplitude(const Vec3& amplitude)
{
    m_deviationAmplitude.x = amplitude.x;
    m_deviationAmplitude.y = amplitude.y;
    m_deviationAmplitude.z = amplitude.z;
    initDeviations();
}
const Vec3& RainManager::getDeviationAmplitude() const { return m_deviationAmplitude; }

// PE 0x101591A0.
void RainManager::setDeviationSpeed(const Vec2& speed)
{
    m_deviationSpeed.x = speed.x;
    m_deviationSpeed.y = speed.y;
}
const Vec2& RainManager::getDeviationSpeed() const { return m_deviationSpeed; }

// PE 0x10158010.
void RainManager::setCellCenterFrozen(bool frozen) { m_cellCenterFrozen = frozen; }
bool RainManager::getCellCenterFrozen() const { return m_cellCenterFrozen; }

// PE 0x10159CC0.
void RainManager::setDrawCells(bool draw) { setRenderMode(draw ? 2 : 0); }
bool RainManager::getDrawCells() const { return m_renderMode == 2; }

// PE 0x10159CE0.
void RainManager::setDrawLines(bool draw) { setRenderMode(draw ? 1 : 0); }
bool RainManager::getDrawLines() const { return m_renderMode == 1; }

// PE 0x10158090.
void RainManager::setLineLength(float length) { m_lineLength = length; }
float RainManager::getLineLength() const { return m_lineLength; }


// 0x100B0C25: an aggregate outer is refused; a failed query releases the new object.
IBase* __fastcall createRainManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(RainManager));
    RainManager* const self = storage ? new (storage) RainManager() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
