// Full-screen texture blit support.

#ifdef BF2_IMAGE_RENDDX9

#include "FSQuadDrawer.h"

#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>

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
// 1748 value bytes. Only the draw records at value +0x20 are read (HemiMapManager uses record 0);
// the rest is [UNRESOLVED]. A new value starts zeroed, as the shipped insert leaves it.
struct DimensionCacheValue
{
    unsigned char unresolved000[0x20];
    DrawRecord    records[2];                       // value +0x20 (node +48)
    unsigned char unresolved040[0x6D4 - 0x40];
};

static_assert(sizeof(DimensionCacheValue) == 0x6D4, "the 1748-byte per-dimension value");

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

[[noreturn]] void noValidTechnique()
{
    // Retail raises DebugTypeError at FSQuadDrawer.cpp:79 with this exact text.  DebugTypeError is
    // fatal in this tree too; keep the failure explicit rather than publishing a drawer with no
    // selectable pass table.
    ::renddx9abi::logf("FSQuadDrawer.cpp:79 Failed to find any valid techniques");
    std::fprintf(stderr, "FSQuadDrawer.cpp:79 Failed to find any valid techniques\n");
    std::fflush(stderr);
    std::abort();
}

// The declaration bytes at PE 0x102388C4:
//   stream 0, +0 FLOAT4 POSITION0; stream 0, +16 FLOAT2 TEXCOORD0; D3DDECL_END.
const D3DVERTEXELEMENT9 kQuadDeclaration[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

} // anonymous namespace

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

private:
    DrawRecord* ensureHemiQuad(unsigned short width, unsigned short height);
    void buildHemiQuad(unsigned short width, unsigned short height, DrawRecord* record);

    /* 0x004 */ unsigned int       m_listenerState; // base ctor leaves this word unwritten
    /* 0x008 */ DimensionCache     m_dimensionCache;
    /* 0x014 */ float              m_weights[18];
    /* 0x05C */ float              m_highPassGate;
    /* 0x060 */ float              m_blurStrength;
    /* 0x064 */ unsigned int       m_vertexCursor;
    /* 0x068 */ unsigned int       m_indexCursor;
    /* 0x06C */ void*              m_vertexBuffer;
    /* 0x070 */ void*              m_indexBuffer;
    /* 0x074 */ void*              m_vertexDeclaration;
    /* 0x078 */ dx9rend::Effect*   m_effect;
    /* 0x07C */ dx9rend::ShaderParameter* m_parameters[25];
    /* 0x0E0 */ dx9rend::EngineTexture* m_depthComplexityTexture; // set by path activation
};

FSQuadDrawer::FSQuadDrawer()
{
    // The +0x08 dimension cache (its 0x6E8-byte sentinel in the image) is constructed empty.

    m_blurStrength = 0.0f;
    m_vertexCursor = 0;
    m_indexCursor = 0;
    m_highPassGate = 0.75f;

    static const float weights[18] =
    {
        0.05f, 0.20f, 0.50f, 0.90f, 1.00f, 0.90f, 0.50f, 0.20f, 0.05f,
        0.05f, 0.20f, 0.50f, 0.90f, 1.00f, 0.90f, 0.50f, 0.20f, 0.05f
    };
    for (unsigned int i = 0; i < 18; ++i)
        m_weights[i] = weights[i];

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
        noValidTechnique();

    dx9rend::registerRenderPathListener(static_cast<dx9rend::RenderPathListener*>(this));
}

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

DrawRecord* FSQuadDrawer::ensureHemiQuad(unsigned short width, unsigned short height)
{
    const unsigned int key = static_cast<unsigned int>(width)
                           | (static_cast<unsigned int>(height) << 16);
    DimensionCache::iterator node = m_dimensionCache.find(key);
    if (node == m_dimensionCache.end())
        node = m_dimensionCache.insert(std::make_pair(key, DimensionCacheValue())).first;

    // The value's two 16-byte draw records begin at its +0x20 (node+48).  HemiMapManager uses only
    // grid index zero, so no other post-process record is exposed.
    DrawRecord* const record = &node->second.records[0];
    if (!record->primitiveCount)
        buildHemiQuad(width, height, record);
    return record;
}

void FSQuadDrawer::buildHemiQuad(unsigned short width, unsigned short height,
                                 DrawRecord* record)
{
    record->baseVertex = m_vertexCursor;
    record->vertexCount = 4;
    record->startIndex = m_indexCursor;
    record->primitiveCount = 2;

    QuadVertex* const vertices = static_cast<QuadVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, m_vertexCursor, 4, 0));
    unsigned short* const indices = static_cast<unsigned short*>(
        dx9rend::indexBufferLock(m_indexBuffer, m_indexCursor, 6, 0));

    const float halfTexelU = 0.5f / static_cast<float>(width);
    const float halfTexelV = 0.5f / static_cast<float>(height);
    vertices[0].x = -1.0f; vertices[0].y =  1.0f; vertices[0].z = 0.0f; vertices[0].w = 1.0f;
    vertices[0].u = halfTexelU;        vertices[0].v = halfTexelV;
    vertices[1].x =  1.0f; vertices[1].y =  1.0f; vertices[1].z = 0.0f; vertices[1].w = 1.0f;
    vertices[1].u = 1.0f + halfTexelU; vertices[1].v = halfTexelV;
    vertices[2].x = -1.0f; vertices[2].y = -1.0f; vertices[2].z = 0.0f; vertices[2].w = 1.0f;
    vertices[2].u = halfTexelU;        vertices[2].v = 1.0f + halfTexelV;
    vertices[3].x =  1.0f; vertices[3].y = -1.0f; vertices[3].z = 0.0f; vertices[3].w = 1.0f;
    vertices[3].u = 1.0f + halfTexelU; vertices[3].v = 1.0f + halfTexelV;

    indices[0] = 0; indices[1] = 1; indices[2] = 2;
    indices[3] = 2; indices[4] = 1; indices[5] = 3;

    m_indexCursor += 6;
    m_vertexCursor += 4;
    dx9rend::indexBufferUnlock(m_indexBuffer);
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
}

void FSQuadDrawer::drawHemiMap(dx9rend::EngineTexture* source,
                               dx9rend::EngineTexture* destination)
{
    static const unsigned int kHemiBlitPass = 17;
    if (dx9rend::effectCurrentPassCount(m_effect) <= kHemiBlitPass)
        return;

    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    const unsigned short width = static_cast<unsigned short>(source->width);
    const unsigned short height = static_cast<unsigned short>(source->height);
    DrawRecord* const record = ensureHemiQuad(width, height);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::indexBufferBind(m_indexBuffer);

    // HemiMapManager's call fixes the general method's push-state byte to true.
    dx9rend::stateManagerPush();

    // sub_10025A70(0): StateManager's ref-counted desired RT0 at +0x1E90.
    IDirect3DSurface9* savedRenderTarget = dx9rend::stateManagerRenderTarget0();
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

    // PE sub_100846E0 has no case for pass 17: there are no per-dimension shader constants to
    // upload between target selection and Effect::begin for this operation.
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, kHemiBlitPass);
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

    dx9rend::stateManagerSetRenderTarget(0, savedRenderTarget);
    if (savedRenderTarget)
        savedRenderTarget->Release();
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

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
