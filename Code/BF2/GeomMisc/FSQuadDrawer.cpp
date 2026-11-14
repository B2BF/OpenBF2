// Full-screen quad and grid submission for the FSQuadDrawer effect passes.

#ifdef BF2_IMAGE_RENDDX9

#include "FSQuadDrawer.h"
#include "Debug.h"

#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {

extern void* g_shaderManager;  // dword_1023B97C
extern void* g_rendererBase;   // dword_1023B970

// BufferManager.cpp.  These are the exact wrappers used by the PE bodies above.
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts,
                       unsigned int countVerts, unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  vertexBufferRelease(void* wrapper);

void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices,
                      unsigned int countIndices, unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  indexBufferBind(void* wrapper);
void  indexBufferRelease(void* wrapper);

void  vertexDeclarationBind(void* wrapper);

// Effect.cpp.  stencilpass is deliberately the one borrowed parameter; every other parameter is
// obtained through the caller-reference-taking wrapper and released by FSQuadDrawer::~FSQuadDrawer.
bool         effectHasTechnique(void* effect, const std::string& name);
unsigned int effectCurrentPassCount(void* effect);

// StateManager.cpp / BF2Render.cpp.
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex);

} // namespace dx9rend

namespace geom {

// XpLightManager.cpp - the render-target selector with level 0 and face 0x7FFFFFFF; a null texture selects the
// temporary back buffer (+0xF4) or the back buffer (+0x76C).
void lightManagerSetRenderTargetTexture(unsigned int index, dx9rend::EngineTexture* texture);

namespace {

struct QuadVertex
{
    float x, y, z, w;
    float u, v;
};

struct DrawRecord
{
    unsigned int baseVertex;
    unsigned int vertexCount;
    unsigned int startIndex;
    unsigned int primitiveCount;
};

static_assert(sizeof(QuadVertex) == 24, "FSQuadDrawer creates a stride-24 vertex buffer");
static_assert(sizeof(DrawRecord) == 16, "FSQuadDrawer cache draw records are four dwords");

// The per-dimension value of the cache at +0x08: the image's 0x6E8-byte node carries the key and
// 1748 value bytes, built by sub_10084E90 on first use of a width/height pair.
struct DimensionCacheValue
{
    DimensionCacheValue();

    float         halfTexel[2];                     // +0x000  0.5 / width, 0.5 / height
    float         zeroOne0[2];                      // +0x008  0, 1
    float         inverseSize[2];                   // +0x010  1 / width, 1 / height
    float         zeroOne1[2];                      // +0x018  0, 1
    DrawRecord    records[2];                       // +0x020 (node +48): 1x1 quad, 10x10 grid
    float         scaleDown2x2[4][4];               // +0x040 sub_10083F50, pass case 1
    float         scaleDown4x4[16][4];              // +0x080 sub_10083FC0, pass case 2
    float         scaleDown4x4Linear[4][4];         // +0x180 sub_10084070, pass case 3
    float         horizontalTaps[4][4];             // +0x1C0 sub_10084070, pass case 0x11
    float         verticalTaps[4][4];               // +0x200 sub_10084070, pass case 0x12
    float         gaussian5x5Offsets[13][4];        // +0x240 sub_10084A80, pass cases 4 / 9
    float         gaussian5x5Weights[13];           // +0x310
    float         gaussian15HorizontalOffsets[15][4]; // +0x344 sub_10084BF0, pass case 5
    float         gaussian15HorizontalWeights[15];  // +0x434
    float         gaussian15VerticalOffsets[15][4]; // +0x470 sub_10084CF0, pass case 6
    float         gaussian15VerticalWeights[15];    // +0x560
    float         poisson13[12][4];                 // +0x59C sub_100841E0, pass case 7
    float         glowHorizontalOffsets[5];         // +0x65C sub_10084390 (weights +0x14)
    float         glowVerticalOffsets[5];           // +0x670 sub_10084390 (weights +0x38)
    float         glowHorizontalWeights[5];         // +0x684
    float         glowVerticalWeights[5];           // +0x698
    float         bloomHorizontalOffsets[5];        // +0x6AC sub_10084550
    float         bloomVerticalOffsets[5];          // +0x6C0
};

static_assert(sizeof(DimensionCacheValue) == 0x6D4, "the 1748-byte per-dimension value");
static_assert(offsetof(DimensionCacheValue, records) == 0x20, "draw records at value +0x20");
static_assert(offsetof(DimensionCacheValue, scaleDown4x4Linear) == 0x180, "pass-9 offsets");
static_assert(offsetof(DimensionCacheValue, gaussian5x5Weights) == 0x310, "5x5 weights");
static_assert(offsetof(DimensionCacheValue, gaussian15HorizontalOffsets) == 0x344, "15x15 h");
static_assert(offsetof(DimensionCacheValue, gaussian15VerticalWeights) == 0x560, "15x15 v");
static_assert(offsetof(DimensionCacheValue, poisson13) == 0x59C, "poisson offsets");
static_assert(offsetof(DimensionCacheValue, glowHorizontalOffsets) == 0x65C, "glow offsets");
static_assert(offsetof(DimensionCacheValue, bloomVerticalOffsets) == 0x6C0, "bloom offsets");

// The value's constructor. The image zeroes the four texel float pairs (+0x00..+0x1C), the
// primitive count of both draw records (+0x2C, +0x3C) and every four-float offset table (+0x40,
// +0x80, +0x180, +0x1C0, +0x200, +0x240, +0x344, +0x470, +0x59C); the remaining draw-record words
// and the weight, glow and bloom floats are left for insertDimensionValue's builders.
// PE 0x100861E0
DimensionCacheValue::DimensionCacheValue()
{
    // [CHOSEN, not recovered] Memory-safety exception (docs/macos-port.md): the words
    // the image leaves unwritten are zeroed as well, so the map insertion never copies an
    // uninitialised draw-record word. Every word the image writes is zero here too.
    std::memset(static_cast<void*>(this), 0, sizeof *this);
}

// sub_100841E0's (x, y) multipliers, entry 0 first (0x1020AACC/0x1020AAD0 down to
// 0x1020AA74/0x1020AA78).
const unsigned int kPoissonBits[12][2] =
{
    { 0xBEA70542u, 0xBECFC654u }, { 0xBF5713ADu, 0xBD96B11Cu }, { 0xBF32276Bu, 0x3EEA0DDCu },
    { 0xBE5039ACu, 0x3F1EE73Eu }, { 0x3F765BEAu, 0xBE47A9A0u }, { 0x3EF265F1u, 0xBEF5C5F8u },
    { 0x3F04FB12u, 0x3F445B8Eu }, { 0x3E3DE97Du, 0xBF64A3C6u }, { 0x3F01E6FFu, 0x3D83F141u },
    { 0x3F657BC8u, 0x3ED32DB2u }, { 0xBEA4D552u, 0xBF6EBFDBu }, { 0xBF4AA39Cu, 0xBF190386u },
};

// The shipped exponent argument is spilled as float and clamped to [-10, 10].
// [CHOSEN, not recovered] Binary64 libm substitutes for FLDL2E/F2XM1/FSCALE. For these
// fixed kernels, all 43 normalized float weights match the original x87 sequence.
double gaussianExp(double radiusSquared)
{
    float exponent = static_cast<float>(radiusSquared * -0.5f);
    if (exponent >= 10.0f)                                  // fcmovnb: at or above the high bound
        exponent = 10.0f;
    if (!(exponent >= -10.0f))                              // fcmovb: below the low bound, or NaN
        exponent = -10.0f;
    return std::exp(static_cast<double>(exponent));
}

// The 1 / sqrt(2 pi) factor (the double at 0x1020AAE8).
const double kTwoPi = 6.2831854820251465;

float floatFromBits(unsigned int bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// sub_10084390: nine weights folded into five bilinear taps (pairs 0-1, 2-3, 4-5, 6-7, then 8
// alone) at texel spacing `texel`; the tap sums are stored and the offsets centred on tap 4.
void buildGlowTaps(const float* weights, float* offsets, float* sums, float texel)
{
    static const double kPairBase[4] = { 0.0, 2.0, 4.0, 6.0 };
    for (unsigned int i = 0; i != 4; ++i)
    {
        const double first = weights[2 * i];
        const double second = weights[2 * i + 1];
        const double sum = second + first;
        sums[i] = static_cast<float>(sum);
        offsets[i] = static_cast<float>((second / sum + kPairBase[i] - 4.0) * texel);
    }
    const double last = weights[8];
    sums[4] = weights[8];
    offsets[4] = static_cast<float>((0.0 / last + 8.0 - 4.0) * texel);
}

// sub_10083D80 reads the grid's cells per side from word tables 0x1020AA48 (columns) and
// 0x1020AA4C (rows): {1, 10} for grid 0 and grid 1.
const unsigned short kGridColumns[2] = { 1, 10 };
const unsigned short kGridRows[2]    = { 1, 10 };

// sub_100846E0's case byte per pass (0x100849E4, passes 2..43); 0x13 is the no-upload default.
const unsigned char kPassConstantCase[42] =
{
    0x00, 0x13, 0x13, 0x13, 0x13, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x07, 0x13,
    0x13, 0x13, 0x13, 0x13, 0x08, 0x13, 0x09, 0x13, 0x13, 0x0A, 0x0B, 0x0B, 0x0C, 0x0D,
    0x13, 0x0D, 0x13, 0x13, 0x13, 0x0E, 0x13, 0x0F, 0x0F, 0x10, 0x10, 0x0C, 0x11, 0x12
};

typedef std::map<unsigned int, DimensionCacheValue> DimensionCache;   // width | height << 16

bool multisampleActive()
{
    // PE sub_1000A6B0.  Renderer forced-on byte +0x514 wins, then forced-off +0x515; otherwise
    // inspect the first two-dword mode in the base's multisample list (retail +0xC0).
    const dx9rend::RendDX9Base* const base =
        static_cast<const dx9rend::RendDX9Base*>(dx9rend::g_rendererBase);
    if (base->forceMultisample)
        return true;
    if (base->disableMultisample)
        return false;
    return dx9rend::multiSampleListActive(base);
}

// The declaration bytes at PE 0x102388C4:
//   stream 0, +0 FLOAT4 POSITION0; stream 0, +16 FLOAT2 TEXCOORD0; D3DDECL_END.
const D3DVERTEXELEMENT9 kQuadDeclaration[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

// Replaces a matrix with its general inverse: the matrix is copied, the copy inverted into it
// (fullInvert), and the matrix answered. [INFERRED] name; Mat4's in-place inverse, emitted next to
// its one caller (uploadPassConstants, pass case 0).
// PE 0x100861B0
Mat4& invertMatrixInPlace(Mat4& matrix)
{
    const Mat4 source = matrix;
    fullInvert(source, matrix);
    return matrix;
}

} // anonymous namespace

// The 0x50-byte settings block at FSQuadDrawer +0x14 (sub_100845F0 returns it, sub_10084E60
// replaces it): the horizontal and vertical glow weights, the high-pass gate and the blur strength.
// (FSQuadDrawerSettings is declared in FSQuadDrawer.h: weights +0x00, gate +0x48, blur +0x4C.)
static_assert(sizeof(FSQuadDrawerSettings) == 0x50, "the 0x50-byte settings block");

// The out-of-line helpers below have [INFERRED] names. The first four are unreachable in 1.50: no
// code or data references; each takes its argument on the stack and pops it (retn 4).

// PE 0x10083D40: the reciprocal of a 16-bit dimension.
float reciprocalOfDimension(unsigned short dimension)
{
    return 1.0f / static_cast<float>(static_cast<int>(dimension));
}

// PE 0x10083D60: half the reciprocal of a 16-bit dimension.
float halfReciprocalOfDimension(unsigned short dimension)
{
    return 0.5f / static_cast<float>(static_cast<int>(dimension));
}

// PE 0x10084510: stores the high-pass gate 0.75 into a settings block (+0x48).
void setDefaultHighPassGate(FSQuadDrawerSettings* settings)
{
    settings->highPassGate = 0.75f;
}

// PE 0x10084530: clears the blur strength of a settings block (+0x4C).
void clearBlurStrength(FSQuadDrawerSettings* settings)
{
    settings->blurStrength = 0.0f;
}

// The two-dimensional Gaussian e^(-(x^2 + y^2) / (2 rho^2)) / sqrt(pi * 2 rho^2) at (x, y); the
// exponent is spilled as float and clamped to [-10, 10] (the shared clamp: the high bound first, an
// unordered value lands on the low bound). The fixed builders below inline it with rho = 1.
// [INFERRED] name.
// PE 0x10084A10. Unreachable in 1.50: no code or data references.
float gaussianWeight(float x, float y, float rho)
{
    const float twoRhoSquared = rho * rho + rho * rho;
    float exponent = (x * x + y * y) * (-1.0f / twoRhoSquared);
    if (exponent >= 10.0f)
        exponent = 10.0f;
    if (!(exponent >= -10.0f))
        exponent = -10.0f;
    // [CHOSEN, not recovered] Binary64 libm exp for FLDL2E/F2XM1/FSCALE, as gaussianExp above.
    const float power = static_cast<float>(std::exp(static_cast<double>(exponent)));
    return power * (1.0f / std::sqrt(twoRhoSquared * 3.14159274f));     // flt_1020AA38
}

// {1 / width, 1 / height} of a texture, from the low 16 bits of each dimension (the texture's
// dims pair). [INFERRED] name and FSQuadDrawer.cpp placement: the body lies among this file's
// helpers while its retail caller is staticMeshMaterialBindTextures (0x1011599F).
// PE 0x10084660
Vec2 textureReciprocalSize(const dx9rend::EngineTexture* texture)
{
    const unsigned int* const dims = dx9rend::textureDims(texture);
    const int width = static_cast<unsigned short>(dims[0]);
    const int height = static_cast<unsigned short>(dims[1]);
    Vec2 result;
    result.x = 1.0f / static_cast<float>(width);
    result.y = 1.0f / static_cast<float>(height);
    return result;
}

// PE 0x100846A0
// {0.5 / width, 0.5 / height} of a texture, from the low 16 bits of each dimension: the half-texel
// offset. [INFERRED] name and placement, as above. Retail calls it as an FSQuadDrawer member
// (ecx = dword_10249978, never read); its callers are Renderer::renderFrame (call 0x100300F4, the
// accumulation-map projection), Terrain::applyFrameConstants (0x100D9B10) and
// RoadCompiledRenderer::updateFrameConstants (0x101454F9).
Vec2 textureHalfTexelSize(const dx9rend::EngineTexture* texture)
{
    const unsigned int* const dims = dx9rend::textureDims(texture);
    const int width = static_cast<unsigned short>(dims[0]);
    const int height = static_cast<unsigned short>(dims[1]);
    Vec2 result;
    result.x = 0.5f / static_cast<float>(width);
    result.y = 0.5f / static_cast<float>(height);
    return result;
}

class FSQuadDrawer : public dx9rend::RenderPathListener
{
public:
    FSQuadDrawer();
    ~FSQuadDrawer();

    virtual void onRenderPathActivated(int path);
    virtual void onRenderPathDeactivated();
    virtual void onRenderPathChanged(int path);
    virtual void onRenderPathNotification(int path);
    virtual int getCurrentRenderPath() const;
    virtual std::string getRenderPathListenerName() const;
    void drawHemiMap(dx9rend::EngineTexture* source,
                     dx9rend::EngineTexture* destination);
    void drawTexturePass(unsigned int pass, dx9rend::EngineTexture* source,
                         dx9rend::EngineTexture* destination, bool pushState,
                         unsigned int grid);                                   // sub_100859F0
    void drawSourcePass(unsigned short width, unsigned short height, unsigned int pass,
                        dx9rend::EngineTexture* source, bool selectBackBuffer, bool pushState,
                        unsigned int grid, const void* customMatrix);         // sub_10085B50
    void drawQuad(unsigned short width, unsigned short height, unsigned int grid); // sub_10085990
    void drawViewportQuad(unsigned int grid);                                  // sub_10085CB0
    void resetStencilCuller(int stencilRef, int stencilPass, bool pushState);        // sub_10085D30
    void drawStencilPasses(dx9rend::EngineTexture* target, bool pushState);           // sub_10085E70
    void drawStencilResolve(dx9rend::EngineTexture* source, bool selectBackBuffer,
                            bool pushState);                                          // sub_10086030
    const FSQuadDrawerSettings& settings() const { return m_settings; }      // sub_100845F0
    void setSettings(const FSQuadDrawerSettings& settings);                    // sub_10084E60

private:
    void setDefaultWeights(FSQuadDrawerSettings& settings);                    // sub_100844A0
    void resetSettings();                                                      // sub_10084600
    void buildScaleDown2x2(DimensionCacheValue& value);                        // sub_10083F50
    void buildScaleDown4x4(DimensionCacheValue& value);                        // sub_10083FC0
    void buildScaleDown4x4Linear(DimensionCacheValue& value);                  // sub_10084070
    void buildPoisson13(DimensionCacheValue& value);                           // sub_100841E0
    static void buildGlowTapPairs(const float* weights, DimensionCacheValue& value); // sub_10084460
    void buildBloomOffsets(DimensionCacheValue& value);                        // sub_10084550
    void rebuildGlowTaps(DimensionCacheValue& value);                          // sub_10084620
    void buildGaussian5x5(DimensionCacheValue& value);                         // sub_10084A80
    void buildGaussian15Horizontal(DimensionCacheValue& value);                // sub_10084BF0
    void buildGaussian15Vertical(DimensionCacheValue& value);                  // sub_10084CF0
    void refreshCachedGlowTaps();                                              // sub_10084DF0
    DimensionCache::iterator insertDimensionValue(unsigned short width, unsigned short height);
    DimensionCacheValue& dimensionValue(unsigned short width, unsigned short height);
    DimensionCacheValue& textureDimensionValue(dx9rend::EngineTexture* texture);
    DrawRecord* ensureTextureQuad(dx9rend::EngineTexture* texture, unsigned int grid);
    DrawRecord* ensureHemiQuad(unsigned short width, unsigned short height, unsigned int grid);
    DrawRecord* buildDimensionQuad(unsigned short width, unsigned short height, unsigned int grid);
    void buildHemiQuad(unsigned short width, unsigned short height, DrawRecord* record,
                       unsigned int grid);
    void uploadPassConstants(unsigned int pass, DimensionCacheValue& value);

    /* 0x004 */ unsigned int       m_listenerState; // base ctor leaves this word unwritten
    /* 0x008 */ DimensionCache     m_dimensionCache;
    /* 0x014 */ FSQuadDrawerSettings m_settings;
    /* 0x064 */ unsigned int       m_vertexCursor;
    /* 0x068 */ unsigned int       m_indexCursor;
    /* 0x06C */ void*              m_vertexBuffer;
    /* 0x070 */ void*              m_indexBuffer;
    /* 0x074 */ void*              m_vertexDeclaration;
    /* 0x078 */ dx9rend::Effect*   m_effect;
    /* 0x07C */ dx9rend::ShaderParameter* m_parameters[25];
    /* 0x0E0 */ dx9rend::EngineTexture* m_depthComplexityTexture; // set by path activation
};

// PE 0x10085320. The missing-technique diagnostic may return when filtered by Debug.
FSQuadDrawer::FSQuadDrawer()
{
    // The +0x08 dimension cache (its 0x6E8-byte sentinel in the image) is constructed empty.

    m_settings.blurStrength = 0.0f;
    m_vertexCursor = 0;
    m_indexCursor = 0;
    m_settings.highPassGate = 0.75f;
    setDefaultWeights(m_settings);

    // Preserve the shipped construction order: declaration, VB, IB, effect, parameters.
    m_vertexDeclaration = dx9rend::vertexDeclarationCreate(kQuadDeclaration);
    m_vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(514, 24, 8, "FSQuadDrawer");
    m_indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(1536, 8, D3DFMT_INDEX16);
    m_effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, "mods/xpack/Shaders/FSQuadDrawer"));

    dx9rend::Effect* const effect = m_effect;
    m_parameters[0] = dx9rend::effectGetShaderParameterTyped(effect, std::string("TEXLAYER0"));
    m_parameters[1] = dx9rend::effectGetShaderParameterTyped(effect, std::string("TEXLAYER1"));
    m_parameters[2] = dx9rend::effectGetShaderParameterTyped(effect, std::string("STENCILREF"));
    m_parameters[3] = dx9rend::effectGetShaderParameterBorrowedTyped(
        effect, std::string("STENCILPASS"));
    m_parameters[4] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("CONVERTPOSTO8BITMAT"));
    m_parameters[5] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("SCALEDOWN2X2SAMPLEOFFSETS"));
    m_parameters[6] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("SCALEDOWN4X4SAMPLEOFFSETS"));
    m_parameters[7] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("SCALEDOWN4X4LINEARSAMPLEOFFSETS"));
    m_parameters[8] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("GAUSSIANBLUR5X5CHEAPSAMPLEOFFSETS"));
    m_parameters[9] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("GAUSSIANBLUR5X5CHEAPSAMPLEWEIGHTS"));
    m_parameters[10] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("GAUSSIANBLUR15X15HORIZONTALSAMPLEOFFSETS"));
    m_parameters[11] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("GAUSSIANBLUR15X15HORIZONTALSAMPLEWEIGHTS"));
    m_parameters[12] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("GAUSSIANBLUR15X15VERTICALSAMPLEOFFSETS"));
    m_parameters[13] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("GAUSSIANBLUR15X15VERTICALSAMPLEWEIGHTS"));
    m_parameters[14] = dx9rend::effectGetShaderParameterTyped(
        effect, std::string("GROWABLEPOISSON13SAMPLEOFFSETS"));

    // texelsize is fetched here in retail, before the glow/bloom group, even though it is stored at
    // the final object slot (+0xDC).  Preserve that observable effect-map lookup order.
    m_parameters[24] = dx9rend::effectGetShaderParameterTyped(effect, std::string("TEXELSIZE"));
    m_parameters[15] = dx9rend::effectGetShaderParameterTyped(effect, std::string("GLOWHORIZOFFSETS"));
    m_parameters[16] = dx9rend::effectGetShaderParameterTyped(effect, std::string("GLOWHORIZWEIGHTS"));
    m_parameters[17] = dx9rend::effectGetShaderParameterTyped(effect, std::string("GLOWVERTOFFSETS"));
    m_parameters[18] = dx9rend::effectGetShaderParameterTyped(effect, std::string("GLOWVERTWEIGHTS"));
    m_parameters[19] = dx9rend::effectGetShaderParameterTyped(effect, std::string("HIGHPASSGATE"));
    m_parameters[20] = dx9rend::effectGetShaderParameterTyped(effect, std::string("BLOOMHORIZOFFSETS"));
    m_parameters[21] = dx9rend::effectGetShaderParameterTyped(effect, std::string("BLOOMVERTOFFSETS"));
    m_parameters[22] = dx9rend::effectGetShaderParameterTyped(effect, std::string("BLURSTRENGTH"));
    m_parameters[23] = dx9rend::effectGetShaderParameterTyped(effect, std::string("CUSTOMMTX"));

    if (dx9rend::effectHasTechnique(m_effect, std::string("Blit")))
        dx9rend::effectSetTechniqueByName(m_effect, std::string("Blit"));
    else if (dx9rend::effectHasTechnique(m_effect, std::string("Blit_1_4")))
        dx9rend::effectSetTechniqueByName(m_effect, std::string("Blit_1_4"));
    else
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\FSQuadDrawer.cpp", 79, "")
            << "Failed to find any valid techniques";

    dx9rend::registerRenderPathListener(static_cast<dx9rend::RenderPathListener*>(this));
}

// PE 0x10085030 (called from ModuleGeomMisc::close at 0x1007D37D).
FSQuadDrawer::~FSQuadDrawer()
{
    // Retail has no null guard here.  A successfully published render path has populated this
    // field through onRenderPathActivated before ModuleGeomMisc closes.
    dx9rend::EngineTexture* const depth = m_depthComplexityTexture;
    if (!--depth->refCount)
    {
        dx9rend::textureDtor(depth);
        ::renddx9abi::engineFree(depth);
    }

    // Exact PE release order.  stencilpass (index 3 / +0x88) is borrowed and is deliberately not
    // released by this owner.
    for (unsigned int i = 4; i <= 14; ++i)
        m_parameters[i]->release();
    m_parameters[17]->release();
    m_parameters[18]->release();
    m_parameters[15]->release();
    m_parameters[16]->release();
    m_parameters[20]->release();
    m_parameters[21]->release();
    m_parameters[23]->release();
    m_parameters[22]->release();
    m_parameters[19]->release();
    m_parameters[24]->release();
    m_parameters[2]->release();
    m_parameters[1]->release();
    m_parameters[0]->release();
    dx9rend::effectRelease(m_effect);

    dx9rend::indexBufferRelease(m_indexBuffer);
    dx9rend::vertexBufferRelease(m_vertexBuffer);
    dx9rend::vertexDeclarationRelease(m_vertexDeclaration);
    dx9rend::unregisterRenderPathListener(static_cast<dx9rend::RenderPathListener*>(this));

    // The dimension cache's nodes and head are released by its own destructor, which runs next.
}

void FSQuadDrawer::onRenderPathActivated(int path)
{
    (void)path;
    // The shipped body does not release the prior value on repeated activation.
    m_depthComplexityTexture = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerGetTexture(std::string("Common/Textures/DepthComplex2")));
}

void FSQuadDrawer::onRenderPathDeactivated()
{
    // Shared nullsub_15 at PE 0x10037D40.
}

void FSQuadDrawer::onRenderPathChanged(int path)
{
    (void)path; // shared nullsub_53 (ret 4)
}

void FSQuadDrawer::onRenderPathNotification(int path)
{
    (void)path; // shared nullsub_53 (ret 4)
}

int FSQuadDrawer::getCurrentRenderPath() const
{
    // PE 0x101A9A10 is exactly `return *(this + 4)`.
    return static_cast<int>(m_listenerState);
}

std::string FSQuadDrawer::getRenderPathListenerName() const
{
    // The retail table points at the shared DebugTextDrawer name body (0x10098150).
    return std::string("DebugTextDrawer");
}

// The default glow weights, the same nine for both directions; the high-pass gate and the blur
// strength are written by the callers (the constructor and resetSettings).
// PE 0x100844A0
void FSQuadDrawer::setDefaultWeights(FSQuadDrawerSettings& settings)
{
    static const float weights[18] =
    {
        0.05f, 0.20f, 0.50f, 0.90f, 1.00f, 0.90f, 0.50f, 0.20f, 0.05f,
        0.05f, 0.20f, 0.50f, 0.90f, 1.00f, 0.90f, 0.50f, 0.20f, 0.05f
    };
    for (unsigned int i = 0; i < 18; ++i)
        settings.weights[i] = weights[i];
}

// PE 0x10084600. Unreachable in 1.50: no code or data references (no call, jump or stored
// pointer anywhere in the image). Puts the settings block back to the constructor's defaults:
// blur strength 0 (+0x60), high-pass gate 0.75 (+0x5C), then the default glow weights through
// sub_100844A0 on the same object. Unlike setSettings it does not rebuild the cached glow taps.
// [INFERRED] name; the FSQuadDrawer receiver follows from the settings offsets and the member
// call that keeps ecx.
void FSQuadDrawer::resetSettings()
{
    m_settings.blurStrength = 0.0f;
    m_settings.highPassGate = 0.75f;
    setDefaultWeights(m_settings);
}

// The 2x2 scale-down offsets (+0x40): x in {-1, 0} outer; (x / width, -1 / height, 0, 0) then
// (x / width, 0, 0, 0).
// PE 0x10083F50
void FSQuadDrawer::buildScaleDown2x2(DimensionCacheValue& value)
{
    const double invW = value.inverseSize[0];
    const double invH = value.inverseSize[1];
    for (int x = -1, i = 0; x < 1; ++x, i += 2)
    {
        const float column = static_cast<float>(x * invW);
        const float rowA = static_cast<float>(invH * -1.0);
        const float rowB = static_cast<float>(invH * 0.0);
        value.scaleDown2x2[i][0] = column;     value.scaleDown2x2[i][1] = rowA;
        value.scaleDown2x2[i][2] = 0.0f;       value.scaleDown2x2[i][3] = 0.0f;
        value.scaleDown2x2[i + 1][0] = column; value.scaleDown2x2[i + 1][1] = rowB;
        value.scaleDown2x2[i + 1][2] = 0.0f;   value.scaleDown2x2[i + 1][3] = 0.0f;
    }
}

// The 4x4 scale-down offsets (+0x80): x in -2..1 outer; rows -2, -1, 0, 1 texels, z and w zero,
// the last row the inverse height itself.
// PE 0x10083FC0
void FSQuadDrawer::buildScaleDown4x4(DimensionCacheValue& value)
{
    const double invW = value.inverseSize[0];
    const double invH = value.inverseSize[1];
    for (int x = -2, i = 0; x < 2; ++x)
    {
        static const double kRows[4] = { -2.0, -1.0, 0.0, 1.0 };
        for (unsigned int r = 0; r != 4; ++r, ++i)
        {
            value.scaleDown4x4[i][0] = static_cast<float>(x * invW);
            value.scaleDown4x4[i][1] = static_cast<float>(r == 3 ? invH : invH * kRows[r]);
            value.scaleDown4x4[i][2] = 0.0f;
            value.scaleDown4x4[i][3] = 0.0f;
        }
    }
}

// The bilinear 4x4 scale-down offsets (+0x180): x in {-2, 0} outer, y in {-2, 0} inner; each
// entry (2x / width + halfU, 2y / height + halfV, 0, 0). Then the one-dimensional taps -1, 0, 1,
// 2 texels along x (+0x1C0) and along y (+0x200), every other component zero.
// PE 0x10084070
void FSQuadDrawer::buildScaleDown4x4Linear(DimensionCacheValue& value)
{
    const double invW = value.inverseSize[0];
    const double invH = value.inverseSize[1];
    unsigned int sample = 0;
    for (int x = -2; x < 2; x += 2)
        for (int y = -2; y < 2; y += 2, ++sample)
        {
            value.scaleDown4x4Linear[sample][0] =
                static_cast<float>(2.0 * x * value.inverseSize[0] + value.halfTexel[0]);
            value.scaleDown4x4Linear[sample][1] =
                static_cast<float>(2.0 * y * value.inverseSize[1] + value.halfTexel[1]);
            value.scaleDown4x4Linear[sample][2] = 0.0f;
            value.scaleDown4x4Linear[sample][3] = 0.0f;
        }
    for (unsigned int i = 0; i != 4; ++i)
    {
        value.horizontalTaps[i][1] = 0.0f;
        value.horizontalTaps[i][2] = 0.0f;
        value.horizontalTaps[i][3] = 0.0f;
        value.verticalTaps[i][0] = 0.0f;
        value.verticalTaps[i][2] = 0.0f;
        value.verticalTaps[i][3] = 0.0f;
    }
    value.horizontalTaps[0][0] = static_cast<float>(invW * -1.0);
    value.horizontalTaps[1][0] = static_cast<float>(invW * 0.0);
    value.horizontalTaps[2][0] = value.inverseSize[0];
    value.horizontalTaps[3][0] = static_cast<float>(invW + invW);
    value.verticalTaps[0][1] = static_cast<float>(invH * -1.0);
    value.verticalTaps[1][1] = static_cast<float>(invH * 0.0);
    value.verticalTaps[2][1] = value.inverseSize[1];
    value.verticalTaps[3][1] = static_cast<float>(invH + invH);
}

// The twelve Poisson-disc taps (+0x59C), scaled to texels, z and w zero.
// PE 0x100841E0
void FSQuadDrawer::buildPoisson13(DimensionCacheValue& value)
{
    const double invW = value.inverseSize[0];
    const double invH = value.inverseSize[1];
    for (unsigned int i = 0; i != 12; ++i)
    {
        value.poisson13[i][0] = static_cast<float>(invW * floatFromBits(kPoissonBits[i][0]));
        value.poisson13[i][1] = static_cast<float>(invH * floatFromBits(kPoissonBits[i][1]));
        value.poisson13[i][2] = 0.0f;
        value.poisson13[i][3] = 0.0f;
    }
}

// The glow taps of a value from nine horizontal weights (`weights`, at 1 / width) and the nine
// vertical ones after them (at 1 / height). The image keeps no receiver for it.
// PE 0x10084460. Unreachable in 1.50: no code or data references.
void FSQuadDrawer::buildGlowTapPairs(const float* weights, DimensionCacheValue& value)
{
    buildGlowTaps(&weights[0], value.glowHorizontalOffsets, value.glowHorizontalWeights,
                  value.inverseSize[0]);
    buildGlowTaps(&weights[9], value.glowVerticalOffsets, value.glowVerticalWeights,
                  value.inverseSize[1]);
}

// The bloom taps (+0x6AC, +0x6C0) at -3.5, -1.5, 0, 1.5, 3.5 texels.
// PE 0x10084550
void FSQuadDrawer::buildBloomOffsets(DimensionCacheValue& value)
{
    const double invW = value.inverseSize[0];
    const double invH = value.inverseSize[1];
    static const double kBloom[5] = { -3.5, -1.5, 0.0, 1.5, 3.5 };
    for (unsigned int i = 0; i != 5; ++i)
        value.bloomHorizontalOffsets[i] = static_cast<float>(invW * kBloom[i]);
    for (unsigned int i = 0; i != 5; ++i)
        value.bloomVerticalOffsets[i] = static_cast<float>(invH * kBloom[i]);
}

// The glow taps of one value from the drawer's own weights: the horizontal ones from weights 0..8
// at 1 / width, the vertical ones from weights 9..17 at 1 / height.
// PE 0x10084620. Unreachable in 1.50: no code or data references.
void FSQuadDrawer::rebuildGlowTaps(DimensionCacheValue& value)
{
    buildGlowTaps(&m_settings.weights[0], value.glowHorizontalOffsets,
                  value.glowHorizontalWeights, value.inverseSize[0]);
    buildGlowTaps(&m_settings.weights[9], value.glowVerticalOffsets,
                  value.glowVerticalWeights, value.inverseSize[1]);
}

// The 13 taps with |x| + |y| <= 2 (+0x240, x outer), z and w zero; weights e^(-r^2 / 2) /
// sqrt(2 pi) (+0x310), summed into a float and normalized.
// PE 0x10084A80
void FSQuadDrawer::buildGaussian5x5(DimensionCacheValue& value)
{
    const double invW = value.inverseSize[0];
    const double invH = value.inverseSize[1];
    float sum = 0.0f;
    unsigned int tap = 0;
    for (int x = -2; x <= 2; ++x)
        for (int y = -2; y <= 2; ++y)
        {
            if ((x < 0 ? -x : x) + (y < 0 ? -y : y) > 2)
                continue;
            value.gaussian5x5Offsets[tap][0] = static_cast<float>(x * invW);
            value.gaussian5x5Offsets[tap][1] = static_cast<float>(y * invH);
            value.gaussian5x5Offsets[tap][2] = 0.0f;
            value.gaussian5x5Offsets[tap][3] = 0.0f;
            const double weight = gaussianExp(static_cast<double>(y) * y
                                              + static_cast<double>(x) * x)
                                * (1.0f / std::sqrt(kTwoPi));
            value.gaussian5x5Weights[tap] = static_cast<float>(weight);
            sum = static_cast<float>(weight + sum);
            ++tap;
        }
    const double scale = 1.0f / static_cast<double>(sum);
    for (unsigned int i = 0; i != 13; ++i)
        value.gaussian5x5Weights[i] = static_cast<float>(scale * value.gaussian5x5Weights[i]);
}

// 15 taps from -7 to 7 along x (+0x344, y, z and w zero) and their weights (+0x434); the
// 1 / sqrt(2 pi) factor is rounded to float first, the weights normalized as above.
// PE 0x10084BF0
void FSQuadDrawer::buildGaussian15Horizontal(DimensionCacheValue& value)
{
    const double invW = value.inverseSize[0];
    const float factor = static_cast<float>(1.0f / std::sqrt(kTwoPi));
    float sum = 0.0f;
    for (int i = -7; i <= 7; ++i)
    {
        float* const offset = value.gaussian15HorizontalOffsets[i + 7];
        offset[0] = static_cast<float>(i * invW);
        offset[1] = 0.0f;
        offset[2] = 0.0f;
        offset[3] = 0.0f;
        const double weight = gaussianExp(static_cast<double>(i) * i) * factor;
        value.gaussian15HorizontalWeights[i + 7] = static_cast<float>(weight);
        sum = static_cast<float>(weight + sum);
    }
    const double scale = 1.0f / static_cast<double>(sum);
    for (unsigned int i = 0; i != 15; ++i)
        value.gaussian15HorizontalWeights[i] =
            static_cast<float>(scale * value.gaussian15HorizontalWeights[i]);
}

// The vertical counterpart: taps along y (+0x470, x, z and w zero), weights at +0x560.
// PE 0x10084CF0
void FSQuadDrawer::buildGaussian15Vertical(DimensionCacheValue& value)
{
    const double invH = value.inverseSize[1];
    const float factor = static_cast<float>(1.0f / std::sqrt(kTwoPi));
    float sum = 0.0f;
    for (int i = -7; i <= 7; ++i)
    {
        float* const offset = value.gaussian15VerticalOffsets[i + 7];
        offset[0] = 0.0f;
        offset[1] = static_cast<float>(i * invH);
        offset[2] = 0.0f;
        offset[3] = 0.0f;
        const double weight = gaussianExp(static_cast<double>(i) * i) * factor;
        value.gaussian15VerticalWeights[i + 7] = static_cast<float>(weight);
        sum = static_cast<float>(weight + sum);
    }
    const double scale = 1.0f / static_cast<double>(sum);
    for (unsigned int i = 0; i != 15; ++i)
        value.gaussian15VerticalWeights[i] =
            static_cast<float>(scale * value.gaussian15VerticalWeights[i]);
}

// Rebuilds the glow taps of every cached dimension value from the current weights, horizontal then
// vertical per node; the cache end is re-read after each node.
// PE 0x10084DF0
void FSQuadDrawer::refreshCachedGlowTaps()
{
    for (DimensionCache::iterator it = m_dimensionCache.begin(); it != m_dimensionCache.end();
         ++it)
    {
        DimensionCacheValue& value = it->second;
        buildGlowTaps(&m_settings.weights[0], value.glowHorizontalOffsets,
                      value.glowHorizontalWeights, value.inverseSize[0]);
        buildGlowTaps(&m_settings.weights[9], value.glowVerticalOffsets,
                      value.glowVerticalWeights, value.inverseSize[1]);
    }
}

// Builds the value of a width/height pair and inserts it under width | height << 16: the texel
// pairs, then every offset table in the image's call order, then the glow taps (buildGlowTaps
// twice). Answers the inserted node.
// PE 0x10084E90
DimensionCache::iterator FSQuadDrawer::insertDimensionValue(unsigned short width,
                                                            unsigned short height)
{
    DimensionCacheValue value;
    const float inverseWidth = static_cast<float>(1.0 / width);
    const float inverseHeight = static_cast<float>(1.0 / height);
    value.halfTexel[0] = static_cast<float>(0.5 / width);
    value.halfTexel[1] = static_cast<float>(0.5 / height);
    value.zeroOne0[0] = 0.0f;
    value.zeroOne0[1] = 1.0f;
    value.inverseSize[0] = inverseWidth;
    value.inverseSize[1] = inverseHeight;
    value.zeroOne1[0] = 0.0f;
    value.zeroOne1[1] = 1.0f;

    buildScaleDown2x2(value);
    buildScaleDown4x4(value);
    buildScaleDown4x4Linear(value);
    buildGaussian5x5(value);
    buildGaussian15Horizontal(value);
    buildGaussian15Vertical(value);
    buildPoisson13(value);
    buildBloomOffsets(value);
    buildGlowTaps(&m_settings.weights[0], value.glowHorizontalOffsets,
                  value.glowHorizontalWeights, value.inverseSize[0]);
    buildGlowTaps(&m_settings.weights[9], value.glowVerticalOffsets,
                  value.glowVerticalWeights, value.inverseSize[1]);

    const unsigned int key = static_cast<unsigned int>(width)
                           | (static_cast<unsigned int>(height) << 16);
    return m_dimensionCache.insert(std::make_pair(key, value)).first;
}

// The value of a width/height pair (key width | height << 16; the map's find), built and
// inserted on a miss.
// PE 0x10085200. That out-of-line copy is unreachable in 1.50: no code or data references; the
// image's callers (sub_10085250, buildDimensionQuad, drawSourcePass) carry inlined copies.
DimensionCacheValue& FSQuadDrawer::dimensionValue(unsigned short width, unsigned short height)
{
    const unsigned int key = static_cast<unsigned int>(width)
                           | (static_cast<unsigned int>(height) << 16);
    DimensionCache::iterator node = m_dimensionCache.find(key);
    if (node == m_dimensionCache.end())
        node = insertDimensionValue(width, height);
    return node->second;
}

// The value at a texture's size: the low 16 bits of its width and height (the dims pair),
// then the find-or-insert above.
// PE 0x10085250
DimensionCacheValue& FSQuadDrawer::textureDimensionValue(dx9rend::EngineTexture* texture)
{
    const unsigned int* const dims = dx9rend::textureDims(texture);
    return dimensionValue(static_cast<unsigned short>(dims[0]),
                          static_cast<unsigned short>(dims[1]));
}

// The draw record of `grid` at a texture's size, built on first use.
// PE 0x10085960. Unreachable in 1.50: no code or data references.
DrawRecord* FSQuadDrawer::ensureTextureQuad(dx9rend::EngineTexture* texture, unsigned int grid)
{
    const unsigned int* const dims = dx9rend::textureDims(texture);
    return ensureHemiQuad(static_cast<unsigned short>(dims[0]),
                          static_cast<unsigned short>(dims[1]), grid);
}

// sub_10085900: the record of `grid` (value +0x20 + 16 * grid) is built on first use.
DrawRecord* FSQuadDrawer::ensureHemiQuad(unsigned short width, unsigned short height,
                                         unsigned int grid)
{
    const unsigned int key = static_cast<unsigned int>(width)
                           | (static_cast<unsigned int>(height) << 16);
    DimensionCache::iterator node = m_dimensionCache.find(key);
    if (node != m_dimensionCache.end() && node->second.records[grid].primitiveCount)
        return &node->second.records[grid];
    return buildDimensionQuad(width, height, grid);
}

// PE 0x100852B0
DrawRecord* FSQuadDrawer::buildDimensionQuad(unsigned short width, unsigned short height,
                                             unsigned int grid)
{
    DrawRecord* const record = &dimensionValue(width, height).records[grid];
    buildHemiQuad(width, height, record, grid);
    return record;
}

// sub_10083D80: a columns x rows grid over clip space, rows outer and columns inner; each vertex
// is (2u - 1, -(2v - 1), 0, 1) with texture coordinates (u + 0.5 / width, v + 0.5 / height).
// Two triangles per cell: (i, i + 1, i + side), (i + side, i + 1, i + side + 1).
void FSQuadDrawer::buildHemiQuad(unsigned short width, unsigned short height,
                                 DrawRecord* record, unsigned int grid)
{
    const unsigned int columns = kGridColumns[grid];
    const unsigned int rows = kGridRows[grid];
    const unsigned int side = columns + 1;
    record->baseVertex = m_vertexCursor;
    record->vertexCount = static_cast<unsigned short>((rows + 1) * side);
    record->startIndex = m_indexCursor;
    record->primitiveCount = static_cast<unsigned short>(rows * columns * 2);
    const unsigned int indexCount = static_cast<unsigned short>(record->primitiveCount * 3);

    QuadVertex* vertices = static_cast<QuadVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, m_vertexCursor, record->vertexCount, 0));
    unsigned short* indices = static_cast<unsigned short*>(
        dx9rend::indexBufferLock(m_indexBuffer, m_indexCursor, indexCount, 0));

    // PE 0x10083D60, inlined here with float spills at 0x10083E1E / 0x10083E2A.
    const float halfTexelU = static_cast<float>(0.5 / width);
    const float halfTexelV = static_cast<float>(0.5 / height);
    for (unsigned int r = 0; r <= rows; ++r)
    {
        const double v = static_cast<double>(r) / rows;
        const float y = static_cast<float>(-(v + v - 1.0));
        for (unsigned int c = 0; c <= columns; ++c, ++vertices)
        {
            const double u = static_cast<double>(c) / columns;
            vertices->x = static_cast<float>(u + u - 1.0);
            vertices->y = y;
            vertices->z = 0.0f;
            vertices->w = 1.0f;
            vertices->u = static_cast<float>(u + halfTexelU);
            vertices->v = static_cast<float>(v + halfTexelV);
        }
    }

    unsigned int rowBase = 1;
    for (unsigned int r = 0; r < rows; ++r, rowBase += side)
    {
        unsigned int next = rowBase;
        for (unsigned int c = 0; c < columns; ++c, ++next)
        {
            *indices++ = static_cast<unsigned short>(next - 1);
            *indices++ = static_cast<unsigned short>(next);
            *indices++ = static_cast<unsigned short>(side + next - 1);
            *indices++ = static_cast<unsigned short>(side + next - 1);
            *indices++ = static_cast<unsigned short>(next);
            *indices++ = static_cast<unsigned short>(side + next);
        }
    }

    m_indexCursor += indexCount;
    m_vertexCursor += record->vertexCount;
    dx9rend::indexBufferUnlock(m_indexBuffer);
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
}

// sub_100846E0: per-pass shader constants from the dimension value and the settings, selected
// through the case table at 0x100849E4 (jump table 0x10084994). Passes outside 2..43 and the 0x13
// entries upload nothing.
void FSQuadDrawer::uploadPassConstants(unsigned int pass, DimensionCacheValue& value)
{
    if (pass < 2 || pass - 2 >= sizeof(kPassConstantCase))
        return;
    const unsigned int index = kPassConstantCase[pass - 2];
    switch (index)
    {
    case 0x00:                                                     // PE 0x10084720
    {
        Mat4 inverseProjection = *dx9rend::rendererGetRenderView()->getProjectionMatrixPtr();
        invertMatrixInPlace(inverseProjection);                    // sub_100861B0
        m_parameters[4]->setValue(&inverseProjection, 0);          // CONVERTPOSTO8BITMAT
        return;
    }
    case 0x01:                                                     // 0x1008475C
        m_parameters[5]->setValue(value.scaleDown2x2, 0);          // SCALEDOWN2X2SAMPLEOFFSETS
        return;
    case 0x02:                                                     // 0x10084778
        m_parameters[6]->setValue(value.scaleDown4x4, 0);          // SCALEDOWN4X4SAMPLEOFFSETS
        return;
    case 0x03:                                                     // 0x10084797
        m_parameters[7]->setValue(value.scaleDown4x4Linear, 0);    // SCALEDOWN4X4LINEAR...
        return;
    case 0x09:                                                     // 0x100847F4
        m_parameters[22]->setFloat(m_settings.blurStrength);
        // falls into case 4
    case 0x04:                                                     // 0x10084803
        m_parameters[8]->setValue(value.gaussian5x5Offsets, 0);    // GAUSSIANBLUR5X5CHEAP...
        m_parameters[9]->setValue(value.gaussian5x5Weights, 0);
        return;
    case 0x05:                                                     // 0x10084836
        m_parameters[10]->setValue(value.gaussian15HorizontalOffsets, 0);
        m_parameters[11]->setValue(value.gaussian15HorizontalWeights, 0);
        return;
    case 0x06:                                                     // 0x10084869
        m_parameters[12]->setValue(value.gaussian15VerticalOffsets, 0);
        m_parameters[13]->setValue(value.gaussian15VerticalWeights, 0);
        return;
    case 0x07:                                                     // 0x1008489C
        m_parameters[14]->setValue(value.poisson13, 0);            // GROWABLEPOISSON13...
        return;
    case 0x08:                                                     // 0x10084707
        m_parameters[22]->setValue(&m_settings.blurStrength, 0);   // BLURSTRENGTH
        return;
    case 0x0A:                                                     // 0x100848E0
        m_parameters[15]->setValue(value.glowHorizontalOffsets, 0);
        m_parameters[16]->setValue(value.glowHorizontalWeights, 0);
        return;
    case 0x0B:                                                     // 0x10084913
        m_parameters[17]->setValue(value.glowVerticalOffsets, 0);
        m_parameters[18]->setValue(value.glowVerticalWeights, 0);
        return;
    case 0x0D:                                                     // 0x100848BA
        m_parameters[22]->setFloat(m_settings.blurStrength);
        // falls into case 0x0C
    case 0x0C:                                                     // 0x100848C9
        m_parameters[19]->setFloat(m_settings.highPassGate);       // HIGHPASSGATE
        return;
    case 0x0E:                                                     // 0x10084946
        m_parameters[20]->setValue(value.bloomHorizontalOffsets, 0);
        return;
    case 0x10:                                                     // 0x10084964
        m_parameters[22]->setFloat(m_settings.blurStrength);
        // falls into case 0x0F
    case 0x0F:                                                     // 0x10084973
        m_parameters[21]->setValue(value.bloomVerticalOffsets, 0);
        return;
    case 0x11:                                                     // 0x100847B6
        m_parameters[7]->setValue(value.horizontalTaps, 0);
        return;
    case 0x12:                                                     // 0x100847D5
        m_parameters[7]->setValue(value.verticalTaps, 0);
        return;
    case 0x13:                                                     // 0x100848D8
        return;
    }
}

// sub_10084E60: replace the settings block, then (sub_10084DF0) rebuild the glow taps of every
// cached dimension value from the new weights.
void FSQuadDrawer::setSettings(const FSQuadDrawerSettings& settings)
{
    m_settings = settings;
    refreshCachedGlowTaps();
}

// sub_100859F0 (pass, source, destination, pushState, grid).
void FSQuadDrawer::drawTexturePass(unsigned int pass, dx9rend::EngineTexture* source,
                                   dx9rend::EngineTexture* destination, bool pushState,
                                   unsigned int grid)
{
    if (dx9rend::effectCurrentPassCount(m_effect) <= pass)
        return;

    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    DimensionCacheValue& value = textureDimensionValue(source);
    const unsigned short width = static_cast<unsigned short>(source->width);
    const unsigned short height = static_cast<unsigned short>(source->height);
    DrawRecord* const record = ensureHemiQuad(width, height, grid);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::indexBufferBind(m_indexBuffer);

    if (pushState)
        dx9rend::stateManagerPush();

    // sub_10025A70(0): StateManager's ref-counted desired RT0 at +0x1E90.
    IDirect3DSurface9* savedRenderTarget = dx9rend::stateManagerGetRenderTarget(0);
    if (savedRenderTarget)
        savedRenderTarget->AddRef();

    // Pass the EngineTexture pointer itself to the shader parameter.
    m_parameters[0]->setValue(source, 0); // TEXLAYER0

    IDirect3DSurface9* const destinationSurface =
        dx9rend::textureGetLevelSurface(destination, 0, 0x7FFFFFFFu);
    dx9rend::stateManagerSetRenderTarget(0, destinationSurface);
    if (destinationSurface)
        destinationSurface->Release();

    if (multisampleActive())
        dx9rend::stateManagerSetDepthStencilSurface(0);

    uploadPassConstants(pass, value);
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, pass);
    dx9rend::rendererDrawIndexedPrimitive(record->primitiveCount, D3DPT_TRIANGLELIST,
                                          static_cast<int>(record->baseVertex), 0,
                                          record->vertexCount, record->startIndex);
    dx9rend::effectEnd(m_effect);

    if (multisampleActive())
    {
        // The Renderer's depth-stencil reference, retail +0x770.
        IDirect3DSurface9* const rendererDepth = dx9rend::rendererDepthStencilSurface();
        dx9rend::stateManagerSetDepthStencilSurface(rendererDepth);
    }

    dx9rend::rendererSetRenderTarget(0, savedRenderTarget);       // sub_10005770, result unread
    if (savedRenderTarget)
        savedRenderTarget->Release();
    if (pushState)
        dx9rend::stateManagerPop();
}

// HemiMapManager::makeHemiMap (0x1012BCB5) passes (17, source, destination, true, 0). Pass 17's
// case entry is the no-upload default.
void FSQuadDrawer::drawHemiMap(dx9rend::EngineTexture* source,
                               dx9rend::EngineTexture* destination)
{
    drawTexturePass(17, source, destination, true, 0);
}

// sub_10085B50 (width, height, pass, source, selectBackBuffer, pushState, grid, customMatrix).
// Unlike sub_100859F0 it checks no pass count and leaves the depth stencil alone.
void FSQuadDrawer::drawSourcePass(unsigned short width, unsigned short height, unsigned int pass,
                                  dx9rend::EngineTexture* source, bool selectBackBuffer,
                                  bool pushState, unsigned int grid, const void* customMatrix)
{
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    DimensionCacheValue& value = dimensionValue(width, height);
    DrawRecord* const record = ensureHemiQuad(width, height, grid);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::indexBufferBind(m_indexBuffer);

    if (pushState)
        dx9rend::stateManagerPush();

    IDirect3DSurface9* savedRenderTarget = dx9rend::stateManagerGetRenderTarget(0);
    if (savedRenderTarget)
        savedRenderTarget->AddRef();

    if (selectBackBuffer)
        lightManagerSetRenderTargetTexture(0, 0);                  // sub_10005920(0, 0, 0, max)
    if (customMatrix)
        m_parameters[23]->setValue(customMatrix, 0);               // +0xD8 CUSTOMMTX
    m_parameters[0]->setValue(source, 0);                         // TEXLAYER0

    uploadPassConstants(pass, value);
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, pass);
    dx9rend::rendererDrawIndexedPrimitive(record->primitiveCount, D3DPT_TRIANGLELIST,
                                          static_cast<int>(record->baseVertex), 0,
                                          record->vertexCount, record->startIndex);
    dx9rend::effectEnd(m_effect);

    dx9rend::rendererSetRenderTarget(0, savedRenderTarget);       // sub_10005770, result unread
    if (savedRenderTarget)
        savedRenderTarget->Release();
    if (pushState)
        dx9rend::stateManagerPop();
}

// sub_10085990: the grid draw alone, with whatever effect pass the caller has begun.
void FSQuadDrawer::drawQuad(unsigned short width, unsigned short height, unsigned int grid)
{
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    DrawRecord* const record = ensureHemiQuad(width, height, grid);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::indexBufferBind(m_indexBuffer);
    dx9rend::rendererDrawIndexedPrimitive(record->primitiveCount, D3DPT_TRIANGLELIST,
                                          static_cast<int>(record->baseVertex), 0,
                                          record->vertexCount, record->startIndex);
}

// sub_10085CB0: the Renderer's view (+0x48) viewport (+0x14); height then width are truncated
// (_ftol2) and the quad is drawn at that size.
void FSQuadDrawer::drawViewportQuad(unsigned int grid)
{
    const float* const viewport = dx9rend::rendererGetRenderView()->getViewport();
    const int height = static_cast<int>(viewport[3]);
    const int width = static_cast<int>(viewport[2]);
    drawQuad(static_cast<unsigned short>(width), static_cast<unsigned short>(height), grid);
}

// The "ResetStencilCuller" technique, pass 0, over the viewport quad with STENCILREF and
// STENCILPASS set; the caller's technique is reselected afterwards when it had one, and the four
// colour-write masks (D3DRS_COLORWRITEENABLE, ...1, ...2, ...3) are reopened to 0xF.
// [INFERRED] name.
// PE 0x10085D30. Unreachable in 1.50: no code or data references.
void FSQuadDrawer::resetStencilCuller(int stencilRef, int stencilPass, bool pushState)
{
    if (pushState)
        dx9rend::stateManagerPush();
    const std::string savedTechnique = dx9rend::effectCurrentTechniqueName(m_effect);
    dx9rend::effectSetTechniqueByName(m_effect, std::string("ResetStencilCuller"));
    dx9rend::effectBegin(m_effect);
    m_parameters[2]->setInt(stencilRef);                             // +0x84 STENCILREF
    m_parameters[3]->setInt(stencilPass);                            // +0x88 STENCILPASS
    dx9rend::effectBeginPass(m_effect, 0);
    drawViewportQuad(0);
    dx9rend::effectEnd(m_effect);
    if (!savedTechnique.empty())
        dx9rend::effectSetTechniqueByName(m_effect, savedTechnique);
    dx9rend::stateManagerSetRenderState(0xA8u, 0xFu);                // D3DRS_COLORWRITEENABLE
    dx9rend::stateManagerSetRenderState(0xBEu, 0xFu);                // ...ENABLE1
    dx9rend::stateManagerSetRenderState(0xBFu, 0xFu);                // ...ENABLE2
    dx9rend::stateManagerSetRenderState(0xC0u, 0xFu);                // ...ENABLE3
    if (pushState)
        dx9rend::stateManagerPop();
}

// Selects `target` as render target 0 (a null target the back buffer), then the "StencilPasses"
// technique, pass 0, drawing the viewport quad once for every STENCILREF 0..255; the caller's
// technique is reselected afterwards when it had one. The render target is not restored.
// [INFERRED] name.
// PE 0x10085E70. Unreachable in 1.50: no code or data references.
void FSQuadDrawer::drawStencilPasses(dx9rend::EngineTexture* target, bool pushState)
{
    if (pushState)
        dx9rend::stateManagerPush();
    lightManagerSetRenderTargetTexture(0, target);                   // sub_10005920(0, t, 0, max)
    const std::string savedTechnique = dx9rend::effectCurrentTechniqueName(m_effect);
    dx9rend::effectSetTechniqueByName(m_effect, std::string("StencilPasses"));
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, 0);
    for (int stencilRef = 0; stencilRef < 256; ++stencilRef)
    {
        m_parameters[2]->setInt(stencilRef);                         // +0x84 STENCILREF
        drawViewportQuad(0);
    }
    dx9rend::effectEnd(m_effect);
    if (!savedTechnique.empty())
        dx9rend::effectSetTechniqueByName(m_effect, savedTechnique);
    if (pushState)
        dx9rend::stateManagerPop();
}

// "StencilPasses" pass 1 over the viewport quad with TEXLAYER0 = `source` and TEXLAYER1 = the
// depth-complexity texture, into render target 0 or (selectBackBuffer) the back buffer; the saved
// render target 0 is restored. Shipped: the pass is begun but the effect is never ended here.
// [INFERRED] name.
// PE 0x10086030. Unreachable in 1.50: no code or data references.
void FSQuadDrawer::drawStencilResolve(dx9rend::EngineTexture* source, bool selectBackBuffer,
                                      bool pushState)
{
    if (pushState)
        dx9rend::stateManagerPush();
    IDirect3DSurface9* savedRenderTarget = dx9rend::stateManagerGetRenderTarget(0);
    if (savedRenderTarget)
        savedRenderTarget->AddRef();
    if (selectBackBuffer)
        lightManagerSetRenderTargetTexture(0, 0);
    m_parameters[0]->setValue(source, 0);                            // TEXLAYER0
    m_parameters[1]->setValue(m_depthComplexityTexture, 0);          // TEXLAYER1
    const std::string savedTechnique = dx9rend::effectCurrentTechniqueName(m_effect);
    dx9rend::effectSetTechniqueByName(m_effect, std::string("StencilPasses"));
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, 1);
    drawViewportQuad(0);
    // shipped defect: no effectEnd before the technique is reselected.
    if (!savedTechnique.empty())
        dx9rend::effectSetTechniqueByName(m_effect, savedTechnique);
    dx9rend::rendererSetRenderTarget(0, savedRenderTarget);          // sub_10005770
    if (savedRenderTarget)
        savedRenderTarget->Release();
    if (pushState)
        dx9rend::stateManagerPop();
}

// dword_10249978.
FSQuadDrawer* g_FSQuadDrawer = 0;

void initializeFSQuadDrawer()
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(FSQuadDrawer));
    g_FSQuadDrawer = storage ? new (storage) FSQuadDrawer() : 0;
}

void destroyFSQuadDrawer()
{
    if (!g_FSQuadDrawer)
        return;
    FSQuadDrawer* const object = g_FSQuadDrawer;
    object->~FSQuadDrawer();
    ::renddx9abi::engineFree(object);
    // Retail ModuleGeomMisc::close does not clear dword_10249978 after freeing it.
}

void fsQuadDrawerDrawHemiMap(dx9rend::EngineTexture* source,
                             dx9rend::EngineTexture* destination)
{
    // The retail HemiMapManager call likewise has no null guard around dword_10249978.
    g_FSQuadDrawer->drawHemiMap(source, destination);
}

// The post-production callers (RendDX9/PostProductionFrameWork.cpp). As with the hemi call, none
// of the retail call sites guards dword_10249978.

// drawViewportQuad on the published drawer. Grid 0 at 0x1004F030, 0x1004FC04, 0x10050E95 and
// 0x100511A4; the gas-mask pass (0x1004EBB5) passes 1 on the higher shader paths.
void fsQuadDrawerDrawCurrentEffect(unsigned int grid)
{
    g_FSQuadDrawer->drawViewportQuad(grid);
}

// The source pass with (0, source, true, true, 0, 0) as at 0x10051313: pass 0 at the source's
// size into the selected back buffer, state pushed, no custom matrix. Pass 0 uploads no constants.
void fsQuadDrawerBlitToBackBuffer(dx9rend::EngineTexture* source)
{
    g_FSQuadDrawer->drawSourcePass(static_cast<unsigned short>(source->width),
                                   static_cast<unsigned short>(source->height), 0, source,
                                   true, true, 0, 0);
}

// drawTexturePass(9, source, destination, true, 1) as at 0x10050F1A: the 10x10 grid with pass 9's
// 4x4 linear scale-down offsets.
void fsQuadDrawerCaptureTinnitus(dx9rend::EngineTexture* source,
                                 dx9rend::EngineTexture* destination)
{
    g_FSQuadDrawer->drawTexturePass(9, source, destination, true, 1);
}

void fsQuadDrawerTexturePass(unsigned int pass, dx9rend::EngineTexture* source,
                             dx9rend::EngineTexture* destination, bool pushState,
                             unsigned int grid)
{
    g_FSQuadDrawer->drawTexturePass(pass, source, destination, pushState, grid);
}

// sub_10085CF0: the source's dimensions (sub_10014940) and sub_10085B50.
void fsQuadDrawerSourcePass(unsigned int pass, dx9rend::EngineTexture* source,
                            bool selectBackBuffer, bool pushState, unsigned int grid,
                            const void* customMatrix)
{
    g_FSQuadDrawer->drawSourcePass(static_cast<unsigned short>(source->width),
                                   static_cast<unsigned short>(source->height), pass, source,
                                   selectBackBuffer, pushState, grid, customMatrix);
}

// settings() / setSettings on the published drawer.
FSQuadDrawerSettings fsQuadDrawerGetSettings()
{
    return g_FSQuadDrawer->settings();
}

void fsQuadDrawerSetSettings(const FSQuadDrawerSettings& settings)
{
    g_FSQuadDrawer->setSettings(settings);
}

// The read-modify-write the distortion pass does (0x1004EDAA..0x1004EDD4): copy the settings
// (settings()), replace the blur strength, store them back (setSettings).
void fsQuadDrawerSetBlurStrength(float strength)
{
    FSQuadDrawerSettings settings = g_FSQuadDrawer->settings();
    settings.blurStrength = strength;
    g_FSQuadDrawer->setSettings(settings);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
