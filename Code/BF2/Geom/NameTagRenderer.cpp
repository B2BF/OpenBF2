// Path: [INFERRED]; see NameTagRenderer.h.
// Nametag atlas, resources and lifetime, recovered from RendDX9.dll 1.50.
// PE range: 0x101B0500..0x101B5D20. Per-frame drawing is in NameTagRendererDraw.inl.

#ifdef BF2_IMAGE_RENDDX9

#include "NameTagRenderer.h"
#include "NametagManager.h"   // the items and the draw closure's statics (s_pulse, s_maxCallDistance)

#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>

#include <cmath>
#include <cstring>
#include <set>

namespace dice {
namespace hfe {

namespace dx9rend {

extern void* g_shaderManager;   // dword_1023B97C

// BufferManager.cpp - the wrappers every recovered geometry class uses (FSQuadDrawer.cpp declares
// the same list).
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);     // (0x100120A0 is inside it)
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);        // the shipped inline --ref dance around 0x10014510
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  indexBufferRelease(void* wrapper);         // the shipped inline --ref dance around 0x10014300

// NEW seams - drafted in integration/BufferManager_additions.inl for BufferManager.cpp, whose
// VertexBuffer wrapper and pointer sets are private to that TU.
//   The inline decrement removeTexture ships (0x101B285E..0x101B2879): `--wrapper[1]`; destroy
//   (sub_10014510 + engine free) at zero; return the remaining count, because the caller branches
//   on it afterwards.
int   vertexBufferReleaseReference(void* wrapper);
//   PE 0x100119A0 on dword_1023B978: find the wrapper in the default-pool set (+0x18), else in the
//   managed-pool set (+0x0C); release that set's reference and erase the node.
void  bufferManagerRemoveVertexBuffer(void* wrapper);

} // namespace dx9rend

namespace geom {

// GeomMisc/FontSystem/FontSystem.cpp. The image builds Font::Ported with engineAlloc(0xC58) +
// geom_Font_Ported_ctor (0x10087560); the tree's builder is this existing seam.
IBase* createFontPortedDirect();
// NEW seams - drafted in integration/FontSystem_additions.inl (Font::Ported is private to that TU).
//   Font::Ported vslot +0x10, load(const std::string&) (0x100876A0).
bool  fontPortedLoad(IBase* font, const std::string& path);
//   Font::Ported::drawToTexture, PE 0x10087320 (non-virtual): blit `text` glyph by glyph into
//   `target` at *position, advancing position->x; returns the advance in pixels.
float fontPortedDrawToTexture(IBase* font, dx9rend::EngineTexture* target, POINT* position,
                              const std::string& text, bool fromSystemMemory);

namespace {

inline IDirect3DDevice9* device()
{
    // dword_1023B970 + 0x0C, re-read at every use exactly as the image does.
    return static_cast<IDirect3DDevice9*>(dx9rend::renderDevice());
}

// The owner-side texture release every body here inlines: `if (!--tex[1]) { sub_10014E20(tex);
// engine free }`. Deliberately NOT textureReleaseRef: that one nulls the caller's pointer, and the
// shipped destructor leaves its static pointers set.
void releaseTexture(dx9rend::EngineTexture* texture)
{
    if (!--texture->refCount)
    {
        dx9rend::textureDtor(texture);
        ::renddx9abi::engineFree(texture);
    }
}

inline void setVertex(NametagVertex& vertex, float x, float y, float z, float u, float v)
{
    vertex.x = x;
    vertex.y = y;
    vertex.z = z;
    vertex.u = u;
    vertex.v = v;
}

// The quad setTexture (key 0) and createBar build: two triangles, (TL, TR, BR) and (TL, BR, BL), UVs
// over the whole texture. The arrow quad in onRenderPathActivated is the same shape with +-1.
void fillFullQuad(NametagVertex* v, float halfWidth, float halfHeight)
{
    setVertex(v[0], -halfWidth,  halfHeight, 0.0f, 0.0f, 0.0f);
    setVertex(v[1],  halfWidth,  halfHeight, 0.0f, 1.0f, 0.0f);
    setVertex(v[2],  halfWidth, -halfHeight, 0.0f, 1.0f, 1.0f);
    setVertex(v[3], -halfWidth,  halfHeight, 0.0f, 0.0f, 0.0f);
    setVertex(v[4],  halfWidth, -halfHeight, 0.0f, 1.0f, 1.0f);
    setVertex(v[5], -halfWidth, -halfHeight, 0.0f, 0.0f, 1.0f);
}

// The two releases removeTexture inlines for every key it drops (0x101B2822..0x101B288C).
void releaseIconResources(NametagIcon& icon)
{
    if (dx9rend::EngineTexture* const texture = icon.texture)
    {
        const int remaining = --texture->refCount;
        if (remaining == 0)
        {
            dx9rend::textureDtor(texture);
            ::renddx9abi::engineFree(texture);
        }
        // A texture that survived the decrement is also dropped from the TextureManager's cache
        // (sub_100191C0), which releases the cache's own reference. The image re-reads the map
        // slot for the argument; it still holds the same pointer.
        if (remaining != 0)
            dx9rend::textureManagerEvictFromCache(icon.texture);
    }
    if (icon.vertexBuffer)
    {
        const int remaining = dx9rend::vertexBufferReleaseReference(icon.vertexBuffer);
        // Likewise the BufferManager's tracking-set reference (sub_100119A0).
        if (remaining != 0)
            dx9rend::bufferManagerRemoveVertexBuffer(icon.vertexBuffer);
    }
}

const float kInverseTextureWidth  = 0.001953125f;    // flt_101E14C4 = 1/512
const float kInverseTextureHeight = 0.0009765625f;   // flt_10221E20 = 1/1024

} // anonymous namespace


// The shared statics. Zero-initialised like the image's .bss words; the two maps are constructed by
// the TU's static initialisers in the image's order (s_bars 0x101D65B0 first, then s_icons
// 0x101D65F0) and destroyed at exit by 0x101DA070 / 0x101DA0B0, which erase every node without
// releasing the textures and vertex buffers the values point at - exactly what the std::map
// destructors do with these POD values.

IBase*                    NameTagRenderer::s_font                = 0;
dx9rend::Effect*          NameTagRenderer::s_effect              = 0;
dx9rend::ShaderParameter* NameTagRenderer::s_texLayer0           = 0;
dx9rend::ShaderParameter* NameTagRenderer::s_texLayer1           = 0;
dx9rend::ShaderParameter* NameTagRenderer::s_transformationArray = 0;
dx9rend::ShaderParameter* NameTagRenderer::s_colorArray          = 0;
dx9rend::ShaderParameter* NameTagRenderer::s_fadeOut             = 0;
dx9rend::ShaderParameter* NameTagRenderer::s_alphaArray          = 0;
dx9rend::EngineTexture*   NameTagRenderer::s_arrowTexture        = 0;
int                       NameTagRenderer::s_instanceCount       = 0;
NameTagRenderer::BarMap   NameTagRenderer::s_bars;
NameTagRenderer::IconMap  NameTagRenderer::s_icons;


// Initializes the trees and text textures. Render-path activation must initialize the
// remaining resource pointers before destruction; the constructor leaves them untouched.
// The three trees are constructed first, as members (heads sub_101315F0, sub_1008A320,
// sub_101B4870).

NameTagRenderer::NameTagRenderer()
    : m_stagingTexture(0)
    , m_nameTexture(0)
{
    ++s_instanceCount;                // dword_10266D9C
    m_dirty    = 0;
    m_byte3849 = 0;
    m_byte384A = 0;
    m_fadeStart = 0.0f;               // stored as the integer 0 (same bits)
    m_fadeEnd   = 0.0f;
}


// Releases this renderer's resources, then unregisters its render-path listener.
// The final instance releases shared icon/bar resources once per distinct texture;
// shared maps retain their entries until process teardown. The member trees are destroyed after
// the body, m_set20 first, then m_slotMap, then m_freeSlots (0x101B55A0, sub_1008A450,
// sub_10131B00).

NameTagRenderer::~NameTagRenderer()
{
    if (s_arrowTexture)
        releaseTexture(s_arrowTexture);
    if (m_arrowVertexBuffer)
        dx9rend::vertexBufferRelease(m_arrowVertexBuffer);
    if (m_stagingTexture)
        releaseTexture(m_stagingTexture);
    if (s_fadeOut)
        s_fadeOut->release();
    if (s_alphaArray)
        s_alphaArray->release();
    if (s_colorArray)
        s_colorArray->release();
    if (s_transformationArray)
        s_transformationArray->release();
    if (m_indexBuffer)
        dx9rend::indexBufferRelease(m_indexBuffer);
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    if (m_nameTexture)
        releaseTexture(m_nameTexture);
    if (s_texLayer0)
        s_texLayer0->release();
    if (s_texLayer1)
        s_texLayer1->release();
    if (s_effect)
        dx9rend::effectRelease(s_effect);      // Effect vslot +0x08
    if (s_font)
        s_font->release();                     // Font::Ported vslot +0x08

    {
        std::set<dx9rend::EngineTexture*> released;
        if (--s_instanceCount == 0)
        {
            for (IconMap::iterator it = s_icons.begin(); it != s_icons.end(); ++it)
            {
                if (released.find(it->second.texture) == released.end())
                {
                    if (it->second.vertexBuffer)
                        dx9rend::vertexBufferRelease(it->second.vertexBuffer);
                    if (it->second.texture)
                        releaseTexture(it->second.texture);
                    released.insert(it->second.texture);
                }
            }
            for (BarMap::iterator it = s_bars.begin(); it != s_bars.end(); ++it)
            {
                if (it->second.full.texture)
                    releaseTexture(it->second.full.texture);
                if (it->second.empty.texture)
                    releaseTexture(it->second.empty.texture);
                // The bar's two halves share one vertex buffer; only the first copy is released.
                if (it->second.full.vertexBuffer)
                    dx9rend::vertexBufferRelease(it->second.full.vertexBuffer);
            }
        }

        dx9rend::unregisterRenderPathListener(static_cast<dx9rend::RenderPathListener*>(this));
    }
}


// 0x101B0700, 66 B, `retn 10h`. Store order as shipped: count, type, colour count, then the fade
// pair (end through the FPU, start as an integer copy), then register.

void NameTagRenderer::initialize(int type, int slotCount, int colorCount, const Vec2& fadeRange)
{
    m_slotCount  = slotCount;
    m_type       = type;
    m_colorCount = colorCount;
    m_fadeEnd    = fadeRange.y;
    m_fadeStart  = fadeRange.x;
    // RendDX9_registerRenderPathListener(this): once a render path has been published the list calls
    // onRenderPathActivated right here, inside the manager's constructor.
    dx9rend::registerRenderPathListener(static_cast<dx9rend::RenderPathListener*>(this));
}


// 0x101B08F0, 1,221 B, `retn 4` - where every GPU resource is made.

// IT IS NOT idempotent, AND that IS shipped. Each activation creates NEW textures and buffers for
// +0x2C/+0x30/+0x34/+0x38/+0x3C without releasing the previous ones, and takes one more reference on
// every static. A render-path change therefore leaks one set per renderer. Preserved.

void NameTagRenderer::onRenderPathActivated(int path)
{
    (void)path;   // never read

    // the shared font (dword_10266D78)
    if (!s_font)
    {
        // The image null-tests the allocation only to decide what to store; the load below then
        // dispatches through whatever was stored, null included. This tree's
        // createFontPortedDirect uses `new`, so on out-of-memory it throws where retail stores 0.
        s_font = createFontPortedDirect();
        fontPortedLoad(s_font, std::string("Fonts/BF1942.font"));   // result ignored
    }
    else
    {
        s_font->addRef();                                            // vslot +0x00
    }

    // the shared effect and its six parameters (dword_10266D7C..D94)
    if (!s_effect)
    {
        s_effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
            dx9rend::g_shaderManager, "mods/xpack/shaders/Nametag"));
        // sub_1001E420 each: the caller-facing lookup that takes a reference. Order as shipped.
        s_texLayer0 = dx9rend::effectGetShaderParameterTyped(s_effect, std::string("TEXLAYER0"));
        s_texLayer1 = dx9rend::effectGetShaderParameterTyped(s_effect, std::string("TEXLAYER1"));
        s_transformationArray =
            dx9rend::effectGetShaderParameterTyped(s_effect, std::string("TransformationArray"));
        s_colorArray = dx9rend::effectGetShaderParameterTyped(s_effect, std::string("ColorArray"));
        s_fadeOut = dx9rend::effectGetShaderParameterTyped(s_effect, std::string("FadeOut"));
        s_alphaArray = dx9rend::effectGetShaderParameterTyped(s_effect, std::string("AlphaArray"));
    }
    else
    {
        dx9rend::effectAddRef(s_effect);                             // Effect vslot +0x00
        s_texLayer0->addRef();                                       // ShaderParameter vslot +0x04
        s_texLayer1->addRef();
        s_transformationArray->addRef();
        s_colorArray->addRef();
        s_fadeOut->addRef();
        s_alphaArray->addRef();
    }

    // the shared arrow texture (dword_10266D98)
    if (!s_arrowTexture)
        s_arrowTexture = static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerGetTexture(std::string("Menu/Nametag/loactionArrow.dds")));
    else
        dx9rend::textureAddRef(s_arrowTexture);                      // shared table slot +0x08

    // the name atlas: 512 x 1024 A8R8G8B8, one mip, usage 0, D3DPOOL_DEFAULT, recreate mode on
    // (sub_10017E40 swaps the recreate-mode byte; the image keeps only AL and pushes the whole
    // dword back, whose upper bytes the setter ignores.)
    const int savedRecreateMode = dx9rend::textureManagerSwapRecreateMode(1);
    int savedManaged = dx9rend::textureManagerSwapManaged(0);
    m_nameTexture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
        TextureWidth, TextureHeight, D3DFMT_A8R8G8B8, 1, 0, 0));
    dx9rend::textureManagerSwapManaged(savedManaged);
    dx9rend::textureManagerSwapRecreateMode(savedRecreateMode);

    // the blank row: 512 x 16 A8R8G8B8 in D3DPOOL_SYSTEMMEM (managed flag 2), zero-filled
    savedManaged = dx9rend::textureManagerSwapManaged(2);
    m_stagingTexture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
        TextureWidth, RowHeight, D3DFMT_A8R8G8B8, 1, 0, 0));
    dx9rend::textureManagerSwapManaged(savedManaged);

    // No test of the lock record. The byte count is bpp * height * width / 8 with a signed divide
    // (the image's cdq / and 7 / sar 3), multiplied in that order.
    dx9rend::TextureLockRecord* const lock =
        dx9rend::textureLock(m_stagingTexture, 0, 0x7FFFFFFFu);
    std::memset(lock->pBits, 0,
                (lock->bpp * static_cast<int>(lock->height) * static_cast<int>(lock->width)) / 8);
    dx9rend::textureUnlock(m_stagingTexture, lock);

    // the buffers: IB, the slot VB, the arrow VB, in that order
    m_indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(
        static_cast<unsigned int>(m_slotCount * 12), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16);
    m_vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        static_cast<unsigned int>(m_slotCount * 8), sizeof(NametagVertex), D3DUSAGE_WRITEONLY,
        "NametagRenderer");
    m_arrowVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        6, sizeof(NametagVertex), D3DUSAGE_WRITEONLY, "NametagRenderer");

    // Two quads per slot: (i, i+1, i+2), (i+1, i+2, i+3), then the same for i+4. The bound is re-read
    // from m_slotCount every pass, and the loop re-tests the pointer - the rotated `p && i < n` form.
    unsigned short* indices = static_cast<unsigned short*>(
        dx9rend::indexBufferLock(m_indexBuffer, 0, 0, 0));
    for (int base = 0; indices != 0 && base < m_slotCount * 8; base += 8)
    {
        *indices++ = static_cast<unsigned short>(base);
        *indices++ = static_cast<unsigned short>(base + 1);
        *indices++ = static_cast<unsigned short>(base + 2);
        *indices++ = static_cast<unsigned short>(base + 1);
        *indices++ = static_cast<unsigned short>(base + 2);
        *indices++ = static_cast<unsigned short>(base + 3);
        *indices++ = static_cast<unsigned short>(base + 4);
        *indices++ = static_cast<unsigned short>(base + 5);
        *indices++ = static_cast<unsigned short>(base + 6);
        *indices++ = static_cast<unsigned short>(base + 5);
        *indices++ = static_cast<unsigned short>(base + 6);
        *indices++ = static_cast<unsigned short>(base + 7);
    }
    dx9rend::indexBufferUnlock(m_indexBuffer);

    // The arrow: the unit quad, positions and UVs only - the four blendindices shorts of these six
    // vertices are never written (drawArrows fills slot and colours per item before each draw).
    NametagVertex* const arrow = static_cast<NametagVertex*>(
        dx9rend::vertexBufferLock(m_arrowVertexBuffer, 0, 0, 0));
    if (arrow)
        fillFullQuad(arrow, 1.0f, 1.0f);
    dx9rend::vertexBufferUnlock(m_arrowVertexBuffer);

    // The whole 64-record block, then every vertex's blendindices.x = its slot (for m_slotCount
    // slots; the rest stay 0).
    std::memset(m_slots, 0, sizeof(m_slots));
    for (int slot = 0; slot < m_slotCount; ++slot)
        for (int vertex = 0; vertex < NametagSlotRecord::VertexCount; ++vertex)
            m_slots[slot].vertices[vertex].slot = static_cast<short>(slot);
}

void NameTagRenderer::onRenderPathDeactivated()             // nullsub_15, 0x10037D40 (`ret`)
{
}

void NameTagRenderer::onRenderPathChanged(int path)         // nullsub_53, 0x100E13B0 (`ret 4`)
{
    (void)path;
}

void NameTagRenderer::onRenderPathNotification(int path)    // nullsub_53, 0x100E13B0 (`ret 4`)
{
    (void)path;
}

int NameTagRenderer::getCurrentRenderPath() const           // ICF_getDwordAt0x4, 0x101A9A10
{
    return m_renderPath;                                    // never written - see the header
}

std::string NameTagRenderer::getRenderPathListenerName() const
{
    return std::string("NameTagRenderer");                 // aNametagrendere_0, capital T
}


// The device forwarding NametagManager's listener subobject performs for each renderer.

void NameTagRenderer::onDeviceLost()                        // a tail jump
{
    dx9rend::textureReleaseD3DResource(m_nameTexture);      // sub_10014E70, no null test
}

void NameTagRenderer::onDeviceReset()
{
    // Re-created into the same wrapper; no managed / recreate-mode swap around it this time.
    m_nameTexture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
        TextureWidth, TextureHeight, D3DFMT_A8R8G8B8, 1, 0, m_nameTexture));
}

void NameTagRenderer::onDeviceDestroy()                     // (nullsub_67)
{
}


// 0x101B0640, 103 B, `retn 8`. Upload the blank 512x16 staging row over the slot's row.

void NameTagRenderer::clearSlotRow(int slot, int unused)
{
    (void)unused;
    IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_stagingTexture, 0, 0x7FFFFFFFu);
    IDirect3DSurface9* const target = dx9rend::textureGetLevelSurface(m_nameTexture, 0, 0x7FFFFFFFu);
    POINT row;
    row.x = 0;
    row.y = slot << 4;
    device()->UpdateSurface(source, 0, target, &row);       // device vslot +0x78
    source->Release();
    target->Release();
}


// Updates a tag's slot colours and dirtiness, optionally rebuilding its text/quad geometry.
// Rebuilding blits the rank icon and name into the shared texture; unknown ranks can use
// the first registered icon. The fallback/end iterator read remains unchecked.
// [INFERRED] ranks 0/1 reserve no insignia width. Preserve odd-width texel alignment.

void NameTagRenderer::bufferItem(NametagItem* item, bool rebuildText)
{
    // The item's fields used here: m_slot (the row in m_slots and the texture), m_name (the text
    // blitted into the row), m_rankIcon (the s_icons key blitted at the row's left, -1 = none),
    // m_colorIndex / m_flashColorIndex (the name quad's ColorArray pair, -1 = no cross-fade),
    // m_flags (bit 1: show the rank icon; bit 3: draw the name even in arrow mode) and m_arrowMode.
    NametagItem& fields = *item;

    if (rebuildText)
        clearSlotRow(fields.m_slot, 0);

    POINT pen;                                   // var_1C / var_18
    pen.x = 0;
    pen.y = fields.m_slot << 4;
    int iconWidth = 0;                           // var_8

    if (rebuildText)
    {
        const int rankIcon = fields.m_rankIcon;
        IconMap::iterator it = s_icons.find(rankIcon);
        if (((fields.m_flags >> 1) & 1) != 0 && rankIcon != -1)
        {
            if (it == s_icons.end() && s_icons.size() != 0)
                it = s_icons.begin();
            if (it != s_icons.end())
            {
                IDirect3DSurface9* const source =
                    dx9rend::textureGetLevelSurface(it->second.texture, 0, 0x7FFFFFFFu);
                IDirect3DSurface9* const target =
                    dx9rend::textureGetLevelSurface(m_nameTexture, 0, 0x7FFFFFFFu);
                RECT rect;
                rect.left   = it->second.left;
                rect.top    = it->second.top;
                rect.right  = it->second.right;
                rect.bottom = it->second.bottom;
                if (fields.m_rankIcon != -1)
                    device()->UpdateSurface(source, &rect, target, &pen);    // vslot +0x78
                source->Release();
                target->Release();
            }
        }
        if (it->first > 1)                       // it may be end() - see the note above
            iconWidth = 16;

        pen.x += iconWidth;
        pen.y += 3;
        if (!fields.m_arrowMode || ((fields.m_flags >> 3) & 1) != 0)
            fontPortedDrawToTexture(s_font, m_nameTexture, &pen, fields.m_name, true);   // result dropped
    }

    const float invHalfWidth =
        1.0f / (dx9rend::rendererGetRenderView()->getViewport()[2] * 0.5f);   // var_C
    const float invHalfHeight =
        1.0f / (dx9rend::rendererGetRenderView()->getViewport()[3] * 0.5f);   // var_14
    const float halfPen = static_cast<float>(pen.x) * 0.5f;                   // arg_0 as a float
    const float pixelBias =
        (halfPen - static_cast<float>(std::floor(static_cast<double>(halfPen)))) * invHalfWidth;

    const short iconColor = (m_type == 0 && fields.m_colorIndex == 3) ? 3 : 0;
    NametagSlotRecord& record = m_slots[fields.m_slot];
    NametagVertex* const v = record.vertices;

    if (rebuildText)
    {
        const float halfWidth  = halfPen * invHalfWidth;                      // var_4
        const float left       = -halfWidth - pixelBias;
        const float halfHeight = invHalfHeight * 8.0f;                        // flt_1020AAD4
        const float rowTop     = static_cast<float>(fields.m_slot << 4) * kInverseTextureHeight;
        const float iconWidthF = static_cast<float>(iconWidth);
        const float iconRight  = (iconWidthF - halfPen) * invHalfWidth - pixelBias;
        const float iconU      = iconWidthF * kInverseTextureWidth;

        setVertex(v[0], left, halfHeight, 0.0f, 0.0f, rowTop);
        setVertex(v[1], iconRight, halfHeight, 0.0f, iconU,
                  static_cast<float>(fields.m_slot << 4) * kInverseTextureHeight);
        const float negHalfHeight = -halfHeight; // the image parks it in var_14 for v7
        setVertex(v[2], left, negHalfHeight, 0.0f, 0.0f,
                  static_cast<float>((fields.m_slot + 1) << 4) * kInverseTextureHeight);
        setVertex(v[3], iconRight, negHalfHeight, 0.0f, iconU,
                  static_cast<float>((fields.m_slot + 1) << 4) * kInverseTextureHeight);

        // The name quad's left edge IS the icon quad's right edge: whole-vertex copies (indices too;
        // the colour indices are overwritten below).
        v[4] = v[1];
        const float right = halfWidth - pixelBias;                            // arg_0 reused
        setVertex(v[5], right, halfHeight, 0.0f,
                  static_cast<float>(pen.x) * kInverseTextureWidth,
                  static_cast<float>(fields.m_slot << 4) * kInverseTextureHeight);
        v[6] = v[3];
        setVertex(v[7], right, negHalfHeight, 0.0f,
                  static_cast<float>(pen.x) * kInverseTextureWidth,
                  static_cast<float>((fields.m_slot + 1) << 4) * kInverseTextureHeight);
    }

    // Colour indices: the icon quad takes 3 (renderer 0's colour-3 soldiers) or 0; the name quad the
    // item's pair, m_flashColorIndex == -1 meaning "no cross-fade". The item is re-read for every
    // vertex.
    for (int i = NametagSlotRecord::IconQuad; i < NametagSlotRecord::NameQuad; ++i)
    {
        v[i].colorIndex1 = iconColor;
        v[i].colorIndex2 = iconColor;
    }
    for (int i = NametagSlotRecord::NameQuad; i < NametagSlotRecord::VertexCount; ++i)
    {
        v[i].colorIndex1 = static_cast<short>(fields.m_colorIndex);
        v[i].colorIndex2 = static_cast<short>(fields.m_flashColorIndex != -1 ? fields.m_flashColorIndex
                                                                             : fields.m_colorIndex);
    }

    m_dirty = 1;
}


// 0x101B2800, 368 B. ECX = key, EDX = count.

// Missing keys are inserted and immediately erased through operator[].
// A positive count releases resources once per key, although registration shares one reference;
// callers use count 0 to avoid releasing the same resources repeatedly.

void NameTagRenderer::removeTexture(int key, int count)
{
    if (count == 0)
    {
        releaseIconResources(s_icons[key]);
        s_icons.erase(key);
        return;
    }
    if (count > 0)
    {
        for (int i = key; count != 0; ++i, --count)
        {
            releaseIconResources(s_icons[i]);
            s_icons.erase(i);        // inlined in the image: equal_range 0x101B4E20 + erase 0x101B5420
        }
    }
}


// Registers an icon texture region and quad, optionally sharing them across consecutive keys.
// Rank/target textures use system memory. Existing entries are overwritten without release;
// buffer locks and texture dimensions must be valid.

void NameTagRenderer::setTexture(int key, int count, const std::string& textureName,
                                 int x, int y, int width, int height)
{
    const int bottom = y + height;               // arg_10 += arg_8
    const int right  = width + x;                // edi = arg_C + arg_4

    // var_14: written and read under the same test only. Initialised here to keep the compiler
    // quiet; the image leaves it as stack garbage on the other path, where nothing reads it.
    int savedManaged = 0;
    if (key == kRankIconKey || key == kTargetIconKey)
        savedManaged = dx9rend::textureManagerSwapManaged(2);
    dx9rend::EngineTexture* const texture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(textureName));
    if (key == kRankIconKey || key == kTargetIconKey)
        dx9rend::textureManagerSwapManaged(savedManaged);

    void* const vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        6, sizeof(NametagVertex), D3DUSAGE_WRITEONLY, "NametagRenderer");
    NametagVertex* const v =
        static_cast<NametagVertex*>(dx9rend::vertexBufferLock(vertexBuffer, 0, 0, 0));

    const float invHalfWidth =
        1.0f / (dx9rend::rendererGetRenderView()->getViewport()[2] * 0.5f);
    const float* const viewport = dx9rend::rendererGetRenderView()->getViewport();
    const int   cellWidth  = right - x;
    const float halfWidth  = static_cast<float>(cellWidth) * 0.5f * invHalfWidth;   // 0.5 is a double
    const float invHalfHeight = 1.0f / (viewport[3] * 0.5f);
    const float halfHeight = invHalfHeight * (static_cast<float>(bottom - y) * 0.5f);

    if (key == 0)
    {
        fillFullQuad(v, halfWidth, halfHeight);
    }
    else
    {
        const float invTextureWidth  =
            1.0f / static_cast<float>(dx9rend::textureDims(texture)[0]);
        const float invTextureHeight =
            1.0f / static_cast<float>(dx9rend::textureDims(texture)[1]);
        const float u0 = static_cast<float>(x) * invTextureWidth;
        const float u1 = static_cast<float>(right) * invTextureWidth;
        const float v0 = static_cast<float>(y) * invTextureHeight;
        const float v1 = static_cast<float>(bottom) * invTextureHeight;
        setVertex(v[0], -halfWidth,  halfHeight, 0.0f, u0, v0);
        setVertex(v[1],  halfWidth,  halfHeight, 0.0f, u1, v0);
        setVertex(v[2],  halfWidth, -halfHeight, 0.0f, u1, v1);
        setVertex(v[3], -halfWidth,  halfHeight, 0.0f, u0, v0);
        setVertex(v[4],  halfWidth, -halfHeight, 0.0f, u1, v1);
        setVertex(v[5], -halfWidth, -halfHeight, 0.0f, u0, v1);
    }
    dx9rend::vertexBufferUnlock(vertexBuffer);

    if (count == 0)
    {
        (void)dx9rend::textureDims(texture);
        NametagIcon& icon = s_icons[key];
        icon.texture      = texture;
        icon.vertexBuffer = vertexBuffer;
        icon.left         = x;
        icon.top          = y;
        icon.right        = right - 1;
        icon.bottom       = bottom - 1;
        return;
    }

    int cellLeft  = x;
    int cellRight = right - 1;
    for (int i = 0; i < count; ++i)
    {
        (void)dx9rend::textureDims(texture);
        NametagIcon& icon = s_icons[key + i];
        icon.texture      = texture;
        icon.vertexBuffer = vertexBuffer;
        icon.left         = cellLeft;
        icon.top          = y;
        icon.right        = cellRight;
        icon.bottom       = bottom - 1;
        cellRight += cellWidth;
        cellLeft  += cellWidth;
    }
}


// Creates or replaces a textured bar using corner coordinates, not width/height.
// Loads both textures in the current pool and shares one quad between them.
// Existing bar resources are not released, and the vertex-buffer lock is not checked.

void NameTagRenderer::createBar(int key, const std::string& fullTextureName,
                                const std::string& emptyTextureName, int left, int top,
                                int right, int bottom, int fillStart, int fillEnd)
{
    dx9rend::EngineTexture* const fullTexture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(fullTextureName));
    dx9rend::EngineTexture* const emptyTexture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(emptyTextureName));
    void* const vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        6, sizeof(NametagVertex), D3DUSAGE_WRITEONLY, "NametagRenderer");
    NametagVertex* const v =
        static_cast<NametagVertex*>(dx9rend::vertexBufferLock(vertexBuffer, 0, 0, 0));

    const float invHalfWidth =
        1.0f / (dx9rend::rendererGetRenderView()->getViewport()[2] * 0.5f);
    const float* const viewport = dx9rend::rendererGetRenderView()->getViewport();
    const int   width      = right - left;
    const float halfWidth  = static_cast<float>(width) * 0.5f * invHalfWidth;
    const float invHalfHeight = 1.0f / (viewport[3] * 0.5f);
    const float halfHeight = invHalfHeight * (static_cast<float>(bottom - top) * 0.5f);
    fillFullQuad(v, halfWidth, halfHeight);
    dx9rend::vertexBufferUnlock(vertexBuffer);

    NametagBar bar;
    bar.full.texture       = fullTexture;
    bar.full.vertexBuffer  = vertexBuffer;
    bar.full.left          = left;
    bar.full.top           = top;
    bar.full.right         = right;
    bar.full.bottom        = bottom;
    bar.empty.texture      = emptyTexture;
    bar.empty.vertexBuffer = vertexBuffer;
    bar.empty.left         = left;
    bar.empty.top          = top;
    bar.empty.right        = right;
    bar.empty.bottom       = bottom;
    bar.fillStart          = fillStart;
    bar.fillEnd            = fillEnd;
    bar.width              = width;
    s_bars[key] = bar;                                             // 0x101B5C50, then 15 dwords
}

// ECX = key: equal_range (0x101B4E80) + erase(range) (0x101B54E0), nothing released.
// Unreferenced in the image.
void NameTagRenderer::removeBar(int key)
{
    s_bars.erase(key);
}

unsigned int NameTagRenderer::iconCount()
{
    return static_cast<unsigned int>(s_icons.size());             // dword_102670AC
}

// 0x101B0570 - `mov byte ptr [ecx+384Ah], 1; retn`. Unreferenced in the image.
void NameTagRenderer::setByte384A()
{
    m_byte384A = 1;
}

} // namespace geom
} // namespace hfe
} // namespace dice

// The per-frame draw closure (0x101B3E50 and its callees), interleaved with the class's other
// functions in the image and therefore part of this translation unit.
#include "NameTagRendererDraw.inl"

#endif // BF2_IMAGE_RENDDX9
