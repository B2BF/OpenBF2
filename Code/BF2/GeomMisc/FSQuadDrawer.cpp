// Full-screen texture blit support.

#ifdef BF2_IMAGE_RENDDX9

#include "FSQuadDrawer.h"

#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendDX9/PostProductionFrameWork.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/Vc71String.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {

extern void* g_shaderManager;  // dword_1023B97C
extern void* g_stateManager;   // dword_1023B984
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
bool         effectHasTechnique(void* effect, const vc71::string& name);
unsigned int effectCurrentPassCount(void* effect);

// StateManager.cpp / BF2Render.cpp.
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex);

} // namespace dx9rend

namespace geom {

namespace {

static const unsigned int kCacheNodeSize = 0x6E8;
static const unsigned int kCacheColorOffset = kCacheNodeSize - 4;
static const unsigned int kCacheNilOffset = kCacheNodeSize - 3;

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

inline unsigned char* cacheRaw(vc71::map_node* node)
{
    return reinterpret_cast<unsigned char*>(node);
}

inline vc71::map_node* cacheNode(unsigned char* node)
{
    return reinterpret_cast<vc71::map_node*>(node);
}

inline unsigned char*& cacheLink(unsigned char* node, unsigned int index)
{
    return reinterpret_cast<unsigned char**>(node)[index];
}

inline unsigned int& cacheKey(unsigned char* node)
{
    return *reinterpret_cast<unsigned int*>(node + 12);
}

inline bool cacheIsNil(const unsigned char* node)
{
    return node[kCacheNilOffset] != 0;
}

void destroyCacheBranch(unsigned char* node, unsigned char* head)
{
    if (!node || node == head || cacheIsNil(node))
        return;
    destroyCacheBranch(cacheLink(node, 0), head);
    destroyCacheBranch(cacheLink(node, 2), head);
    ::renddx9abi::engineFree(node);
}

bool multisampleActive()
{
    // PE sub_1000A6B0.  Renderer forced-on byte +0x514 wins, then forced-off +0x515; otherwise
    // inspect the first two-dword mode in the base's VC7 vector at +0xC0.
    unsigned char* const base = static_cast<unsigned char*>(dx9rend::g_rendererBase);
    if (base[0x514])
        return true;
    if (base[0x515])
        return false;
    unsigned int* const first = *reinterpret_cast<unsigned int**>(base + 0xC0);
    unsigned int* const last = *reinterpret_cast<unsigned int**>(base + 0xC4);
    return first && ((last - first) >> 1) && *reinterpret_cast<int*>(first) > 0;
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
    virtual vc71::string getRenderPathListenerName() const;
    void drawHemiMap(dx9rend::EngineTexture* source,
                     dx9rend::EngineTexture* destination);
    void drawCurrentEffect();
    void blitToBackBuffer(dx9rend::EngineTexture* source);
    void drawTexturePass(dx9rend::EngineTexture* source,
                         dx9rend::EngineTexture* destination, unsigned pass, unsigned grid);

private:
    unsigned char* findCacheNode(unsigned int key) const;
    unsigned char* insertCacheNode(unsigned int key);
    DrawRecord* ensureHemiQuad(int width, int height, unsigned grid = 0);
    void buildHemiQuad(int width, int height, DrawRecord* record, unsigned grid);

    /* 0x004 */ unsigned int       m_listenerState; // base ctor leaves this word unwritten
    /* 0x008 */ vc71::map_layout   m_dimensionCache;
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

static_assert(sizeof(FSQuadDrawer) == 0xE4,
              "ModuleGeomMisc::init allocates 228 bytes at PE 0x1007D0AF");

FSQuadDrawer::FSQuadDrawer()
{
    // VC7.1 std::map header at +0x08 and its 0x6E8-byte sentinel.  The comparator word at +0x08,
    // head at +0x0C and count at +0x10 are the exact retail layout.
    m_dimensionCache.construct(kCacheNodeSize);

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
    m_parameters[0] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("TEXLAYER0"));
    m_parameters[1] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("TEXLAYER1"));
    m_parameters[2] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("STENCILREF"));
    m_parameters[3] = dx9rend::effectGetShaderParameterBorrowedTyped(
        effect, vc71::string("STENCILPASS"));
    m_parameters[4] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("CONVERTPOSTO8BITMAT"));
    m_parameters[5] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("SCALEDOWN2X2SAMPLEOFFSETS"));
    m_parameters[6] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("SCALEDOWN4X4SAMPLEOFFSETS"));
    m_parameters[7] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("SCALEDOWN4X4LINEARSAMPLEOFFSETS"));
    m_parameters[8] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("GAUSSIANBLUR5X5CHEAPSAMPLEOFFSETS"));
    m_parameters[9] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("GAUSSIANBLUR5X5CHEAPSAMPLEWEIGHTS"));
    m_parameters[10] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("GAUSSIANBLUR15X15HORIZONTALSAMPLEOFFSETS"));
    m_parameters[11] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("GAUSSIANBLUR15X15HORIZONTALSAMPLEWEIGHTS"));
    m_parameters[12] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("GAUSSIANBLUR15X15VERTICALSAMPLEOFFSETS"));
    m_parameters[13] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("GAUSSIANBLUR15X15VERTICALSAMPLEWEIGHTS"));
    m_parameters[14] = dx9rend::effectGetShaderParameterTyped(
        effect, vc71::string("GROWABLEPOISSON13SAMPLEOFFSETS"));

    // texelsize is fetched here in retail, before the glow/bloom group, even though it is stored at
    // the final object slot (+0xDC).  Preserve that observable effect-map lookup order.
    m_parameters[24] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("TEXELSIZE"));
    m_parameters[15] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("GLOWHORIZOFFSETS"));
    m_parameters[16] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("GLOWHORIZWEIGHTS"));
    m_parameters[17] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("GLOWVERTOFFSETS"));
    m_parameters[18] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("GLOWVERTWEIGHTS"));
    m_parameters[19] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("HIGHPASSGATE"));
    m_parameters[20] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("BLOOMHORIZOFFSETS"));
    m_parameters[21] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("BLOOMVERTOFFSETS"));
    m_parameters[22] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("BLURSTRENGTH"));
    m_parameters[23] = dx9rend::effectGetShaderParameterTyped(effect, vc71::string("CUSTOMMTX"));

    if (dx9rend::effectHasTechnique(m_effect, vc71::string("Blit")))
        dx9rend::effectSetTechniqueByName(m_effect, vc71::string("Blit"));
    else if (dx9rend::effectHasTechnique(m_effect, vc71::string("Blit_1_4")))
        dx9rend::effectSetTechniqueByName(m_effect, vc71::string("Blit_1_4"));
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

    unsigned char* const head = cacheRaw(m_dimensionCache.head);
    if (head)
    {
        destroyCacheBranch(cacheLink(head, 1), head);
        ::renddx9abi::engineFree(head);
    }
    m_dimensionCache.head = 0;
    m_dimensionCache.size = 0;
}

void FSQuadDrawer::onRenderPathActivated(int path)
{
    (void)path;
    // The shipped body does not release the prior value on repeated activation.
    m_depthComplexityTexture = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerGetTexture(vc71::string("Common/Textures/DepthComplex2")));
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

vc71::string FSQuadDrawer::getRenderPathListenerName() const
{
    // The retail table points at the shared DebugTextDrawer name body (0x10098150).
    return vc71::string("DebugTextDrawer");
}

unsigned char* FSQuadDrawer::findCacheNode(unsigned int key) const
{
    unsigned char* const head = cacheRaw(m_dimensionCache.head);
    unsigned char* node = cacheLink(head, 1); // sentinel parent is the root
    while (!cacheIsNil(node))
    {
        const unsigned int nodeKey = cacheKey(node);
        if (key == nodeKey)
            return node;
        node = cacheLink(node, key < nodeKey ? 0u : 2u);
    }
    return head;
}

unsigned char* FSQuadDrawer::insertCacheNode(unsigned int key)
{
    // [CHOSEN, not recovered] Preserves node ABI, key ordering, stable addresses, and sentinel links without red-black rebalancing.
    // [RETIRE-WHEN: RW-74] Replace with the shared exact VC7 tree insertion/rebalance helper.
    unsigned char* const head = cacheRaw(m_dimensionCache.head);
    unsigned char* parent = head;
    unsigned char* node = cacheLink(head, 1);
    bool goesLeft = true;
    while (!cacheIsNil(node))
    {
        parent = node;
        goesLeft = key < cacheKey(node);
        node = cacheLink(node, goesLeft ? 0u : 2u);
    }

    unsigned char* inserted = static_cast<unsigned char*>(
        ::renddx9abi::engineAlloc(kCacheNodeSize));
    std::memset(inserted, 0, kCacheNodeSize);
    cacheLink(inserted, 0) = head;
    cacheLink(inserted, 1) = parent;
    cacheLink(inserted, 2) = head;
    cacheKey(inserted) = key;
    inserted[kCacheColorOffset] = 1; // black; no balancing is needed for the private lookup tree
    inserted[kCacheNilOffset] = 0;

    // The cheap Gaussian pass reads four vec4 offsets from the dimension record.
    const unsigned short width = static_cast<unsigned short>(key);
    const unsigned short height = static_cast<unsigned short>(key >> 16);
    const float inverseU = static_cast<float>(1.0 / width);
    const float inverseV = static_cast<float>(1.0 / height);
    const float halfU = static_cast<float>(0.5 / width);
    const float halfV = static_cast<float>(0.5 / height);
    float (*samples)[4] = reinterpret_cast<float (*)[4]>(inserted + 16 + 384);
    unsigned sample = 0;
    for (int x = -2; x < 2; x += 2)
        for (int y = -2; y < 2; y += 2, ++sample)
        {
            samples[sample][0] = static_cast<float>(2.0 * x * inverseU + halfU);
            samples[sample][1] = static_cast<float>(2.0 * y * inverseV + halfV);
            samples[sample][2] = samples[sample][3] = 0.0f;
        }

    if (parent == head)
    {
        cacheLink(head, 0) = inserted;
        cacheLink(head, 1) = inserted;
        cacheLink(head, 2) = inserted;
    }
    else
    {
        cacheLink(parent, goesLeft ? 0u : 2u) = inserted;
        if (key < cacheKey(cacheLink(head, 0)))
            cacheLink(head, 0) = inserted;
        if (key > cacheKey(cacheLink(head, 2)))
            cacheLink(head, 2) = inserted;
    }
    ++m_dimensionCache.size;
    return inserted;
}

DrawRecord* FSQuadDrawer::ensureHemiQuad(int width, int height, unsigned grid)
{
    const unsigned int key = static_cast<unsigned short>(width)
                           | (static_cast<unsigned int>(static_cast<unsigned short>(height)) << 16);
    unsigned char* node = findCacheNode(key);
    if (node == cacheRaw(m_dimensionCache.head))
        node = insertCacheNode(key);

    // The two cached grids share their dimension key and have separate draw records.
    DrawRecord* const record = reinterpret_cast<DrawRecord*>(node + 48 + grid * 16);
    if (!record->primitiveCount)
        buildHemiQuad(width, height, record, grid);
    return record;
}

void FSQuadDrawer::buildHemiQuad(int width, int height,
                                 DrawRecord* record, unsigned grid)
{
    const unsigned cells = grid ? 10 : 1;
    const unsigned side = cells + 1;
    record->baseVertex = m_vertexCursor;
    record->vertexCount = side * side;
    record->startIndex = m_indexCursor;
    record->primitiveCount = cells * cells * 2;

    QuadVertex* const vertices = static_cast<QuadVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, m_vertexCursor, record->vertexCount, 0));
    unsigned short* indices = static_cast<unsigned short*>(
        dx9rend::indexBufferLock(m_indexBuffer, m_indexCursor, record->primitiveCount * 3, 0));

    const float halfTexelU = static_cast<float>(0.5 / static_cast<unsigned short>(width));
    const float halfTexelV = static_cast<float>(0.5 / static_cast<unsigned short>(height));
    for (unsigned y = 0; y != side; ++y)
    {
        const double v = static_cast<double>(y) / cells;
        for (unsigned x = 0; x != side; ++x)
        {
            const double u = static_cast<double>(x) / cells;
            QuadVertex& vertex = vertices[y * side + x];
            vertex.x = static_cast<float>(u + u - 1.0);
            vertex.y = static_cast<float>(-(v + v - 1.0));
            vertex.z = 0.0f; vertex.w = 1.0f;
            vertex.u = static_cast<float>(u + halfTexelU);
            vertex.v = static_cast<float>(v + halfTexelV);
        }
    }
    for (unsigned y = 0; y != cells; ++y)
        for (unsigned x = 0; x != cells; ++x)
        {
            const unsigned base = y * side + x;
            *indices++ = static_cast<unsigned short>(base);
            *indices++ = static_cast<unsigned short>(base + 1);
            *indices++ = static_cast<unsigned short>(base + side);
            *indices++ = static_cast<unsigned short>(base + side);
            *indices++ = static_cast<unsigned short>(base + 1);
            *indices++ = static_cast<unsigned short>(base + side + 1);
        }
    m_indexCursor += record->primitiveCount * 3;
    m_vertexCursor += record->vertexCount;
    dx9rend::indexBufferUnlock(m_indexBuffer);
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
}

void FSQuadDrawer::drawCurrentEffect()
{
    const float* viewport = dx9rend::rendererGetRenderView()->getViewport();
    const int height = static_cast<int>(viewport[3]);
    const int width = static_cast<int>(viewport[2]);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    DrawRecord* record = ensureHemiQuad(width, height);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::indexBufferBind(m_indexBuffer);
    dx9rend::rendererDrawIndexedPrimitive(record->primitiveCount, D3DPT_TRIANGLELIST,
        static_cast<int>(record->baseVertex), 0, record->vertexCount, record->startIndex);
}

void FSQuadDrawer::blitToBackBuffer(dx9rend::EngineTexture* source)
{
    const unsigned short width = static_cast<unsigned short>(source->width);
    const unsigned short height = static_cast<unsigned short>(source->height);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    DrawRecord* record = ensureHemiQuad(width, height);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::indexBufferBind(m_indexBuffer);
    dx9rend::stateManagerPush();
    IDirect3DSurface9* saved = *reinterpret_cast<IDirect3DSurface9**>(
        static_cast<unsigned char*>(dx9rend::g_stateManager) + 0x1E90);
    if (saved)
        saved->AddRef();
    dx9rend::rendererPostSelectBackBuffer();
    m_parameters[0]->setValue(source, 0);
    // Pass zero has no per-dimension parameter upload.
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, 0);
    dx9rend::rendererDrawIndexedPrimitive(record->primitiveCount, D3DPT_TRIANGLELIST,
        static_cast<int>(record->baseVertex), 0, record->vertexCount, record->startIndex);
    dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerSetRenderTarget(0, saved);
    if (saved)
        saved->Release();
    dx9rend::stateManagerPop();
}

void FSQuadDrawer::drawHemiMap(dx9rend::EngineTexture* source,
                               dx9rend::EngineTexture* destination)
{
    drawTexturePass(source, destination, 17, 0);
}

void FSQuadDrawer::drawTexturePass(dx9rend::EngineTexture* source,
                                  dx9rend::EngineTexture* destination,
                                  unsigned pass, unsigned grid)
{
    if (dx9rend::effectCurrentPassCount(m_effect) <= pass)
        return;

    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    const unsigned short width = static_cast<unsigned short>(source->width);
    const unsigned short height = static_cast<unsigned short>(source->height);
    DrawRecord* const record = ensureHemiQuad(width, height, grid);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::indexBufferBind(m_indexBuffer);

    // HemiMapManager's call fixes the general method's push-state byte to true.
    dx9rend::stateManagerPush();

    // sub_10025A70(0): StateManager's ref-counted desired RT0 at +0x1E90.
    IDirect3DSurface9* savedRenderTarget = *reinterpret_cast<IDirect3DSurface9**>(
        static_cast<unsigned char*>(dx9rend::g_stateManager) + 0x1E90);
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

    // Only the recovered history-capture pass uploads cached Gaussian offsets.
    if (pass == 9)
    {
        const unsigned key = width | (static_cast<unsigned>(height) << 16);
        m_parameters[8]->setValue(findCacheNode(key) + 16 + 384, 0);
    }
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, pass);
    dx9rend::rendererDrawIndexedPrimitive(record->primitiveCount, D3DPT_TRIANGLELIST,
                                          static_cast<int>(record->baseVertex), 0,
                                          record->vertexCount, record->startIndex);
    dx9rend::effectEnd(m_effect);

    if (multisampleActive())
    {
        IDirect3DSurface9* const rendererDepth = *reinterpret_cast<IDirect3DSurface9**>(
            static_cast<unsigned char*>(dx9rend::g_rendererBase) + 0x770);
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

void fsQuadDrawerDrawCurrentEffect() { g_FSQuadDrawer->drawCurrentEffect(); }
void fsQuadDrawerBlitToBackBuffer(dx9rend::EngineTexture* source)
{
    g_FSQuadDrawer->blitToBackBuffer(source);
}

void fsQuadDrawerCaptureTinnitus(dx9rend::EngineTexture* source,
                                dx9rend::EngineTexture* destination)
{
    g_FSQuadDrawer->drawTexturePass(source, destination, 9, 1);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
