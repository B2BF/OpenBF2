// Font metrics, atlas storage and text rendering support.
// [CHOSEN, not recovered] Internal containers and tokenization.
// [RETIRE-WHEN: RW-80] Replace chosen storage, parsing and drawing with the validated font pipeline.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include "../../Core/EventManager.h" // FontSystem renderer-event registration.
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/ShaderParameter.h"

#include "../../../RendDX9Abi/D3dx9Sdk.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <new>

namespace dice {
namespace hfe {

// The RendDX9 render seams this stack rides on - the same set SwiffRender.cpp declares.
namespace dx9rend {
extern void* g_shaderManager;
void* shaderManagerGetEffect(void*, const char*);
class Effect;
void* effectGetShaderParameter(Effect*, const vc71::string&);
void* effectFindTechniqueRecord(void* effect, const char* name);
void  effectSetTechniqueRecord(void* effect, void* record);
unsigned int effectBegin(void* effect);
void  effectBeginPass(void* effect, unsigned int index);
void  effectEnd(void* effect);
void* effectD3dxHandle(void* effect);
int   bufferManagerSwapPool(int value);
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  vertexBufferRelease(void* wrapper);
// BufferManager.cpp - ufferManager::removeVertexBuffer (the tracking set's reference).
void  bufferManagerRemoveVertexBuffer(void* wrapper);
void* vertexDeclarationCreate(const void* elements);
void  vertexDeclarationBind(void* wrapper);
void  vertexDeclarationRelease(void* wrapper);
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type, unsigned int startVertex);
} // namespace dx9rend

namespace geom {


// One .dif glyph: code, left bearing, ink width, right bearing, vertical offset,
// Then the atlas rectangle in texels. Advance is the sum of the three horizontal fields.

struct DifGlyph
{
    unsigned short code;     // the character
    float          bearingX; // f1 - left side bearing (drawn at pen + this)
    float          advance;  // = f1 + f2 + f3 - the pen advance (computed in load)
    float          bearingY; // = the int - the vertical Y-offset (top position, baseline-aligning)
    float          intField; // = f2 - the ink width (the layout adds it, sub_100A4BF0)
    float          rightBearing; // = f3 - the right side bearing (the layout adds it too)
    short          x;        // atlas rect, texels
    short          y;
    short          x2;
    short          y2;
    float          height;   // y2 - y, precomputed
};

// One laid-out character: the 16-byte record sub_100A4BF0 pushes per character, in font units
// (unscaled). `left` is where the glyph's quad starts, `right` the ceiling of where its pen ends,
// `cellLeft` the lesser of `left` and the floored pen before kerning, `top` the glyph's floored Y
// offset. The emitter draws a glyph at (left - 0.5, top - 0.5); the visible-window test measures
// a run of glyphs as `cells[last].right - cells[first].left`.
struct GlyphCell
{
    float cellLeft;          // +0
    float right;             // +4
    float left;              // +8
    float top;               // +12
};


// Parsed font metrics, atlas and projection. Device-reset callbacks do not refresh the projection;
// FontSystem reloads fonts when display resolution changes.

class DifFont : public dx9rend::DeviceLostResetListener
{
public:
    DifFont() : m_glyphs(0), m_glyphCount(0), m_glyphCap(0),
                m_kerns(0), m_kernCount(0), m_kernCap(0),
                m_atlas(0), m_atlasW(0), m_atlasH(0), m_size(0.0f),
                m_scaleX(1.0f), m_scaleY(1.0f), m_fontRenderer(0)
    {
        m_projVp[0] = m_projVp[1] = m_projVp[2] = m_projVp[3] = 0.0f;
        for (int i = 0; i < 16; ++i) m_projection[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }

    // The three listener slots, 0x1020D5A4 - each shipped body is `ret` (see the class note).
    void onDeviceLost() override;
    void onDeviceReset() override;
    void onDeviceDestroy() override;    // 0x10037D40, the image's shared nullsub

    // sub_100A4420 - open "<path>.dif", parse it, load "<path>.dds", build the projection, and
    // resolve the FontRenderer singleton. Returns false when the .dif is missing (the shipped
    // FontSystem.cpp:356 "Font X doesn't exist" diagnostic).
    bool load(const char* path);

    // sub_100A57E0 - glyph by character (160 = nbsp folds to 32 = space, as the emitter does).
    const DifGlyph* glyph(unsigned short ch) const;
    // sub_100A5850 - kerning between two characters, 0 when absent.
    float kerning(unsigned short a, unsigned short b) const;
    // sub_100A4BF0 (vtable +0x0C) - lay `len` characters of `text` out into `out[0..len)`.
    void layout(const wchar_t* text, unsigned int len, GlyphCell* out) const;

    // sub_100A3450 - the loader's last act on a fresh font: store the resolution scale in +136/+140.
    // measure() multiplies its answer by it and draw() puts it in the scaling matrix, so a font that
    // never receives it reports and renders at the atlas's own authored size.
    void         setScale(float x, float y) { m_scaleX = x; m_scaleY = y; }

    void*        atlas()      const { return m_atlas; }
    unsigned int atlasW()     const { return m_atlasW; }
    unsigned int atlasH()     const { return m_atlasH; }
    float        size()       const { return m_size; }      // the .dif's authored size, for tracing
    const float* projVp()     const { return m_projVp; }   // [TEMP] RW-89 tracer
    float        scaleX()     const { return m_scaleX; }
    float        scaleY()     const { return m_scaleY; }
    const float* projection() const { return m_projection; }
    void*        fontRenderer() const { return m_fontRenderer; }

    void addRef() { ++m_refCount; }
    int  release();

private:
    DifGlyph* insertGlyph(unsigned short code);

    int          m_refCount;
    DifGlyph*    m_glyphs;      // sorted by code (binary search on lookup)
    unsigned int m_glyphCount;
    unsigned int m_glyphCap;
    struct Kern { unsigned int key; float amount; };
    Kern*        m_kerns;       // sorted by key = (a<<16)|b
    unsigned int m_kernCount;
    unsigned int m_kernCap;
    void*        m_atlas;       // EngineTexture* from the manager
    unsigned int m_atlasW;
    unsigned int m_atlasH;
    float        m_size;
    float        m_scaleX;
    float        m_scaleY;
    float        m_projection[16];
    float        m_projVp[4];        // [TEMP] RW-89: the viewport the projection was built from
    void*        m_fontRenderer;
};

// dx9rend seams this file reaches across for.
namespace {


// A minimal whitespace tokenizer over an in-memory text buffer - [CHOSEN] in place of the shipped
// std::istrstream. It yields the same tokens the shipped `operator>>` sequence consumes: runs of
// non-whitespace separated by any run of spaces / tabs / CR / LF.

class Tokenizer
{
public:
    Tokenizer(const char* data, unsigned int size) : m_p(data), m_end(data + size) {}

    // Next whitespace-delimited token into `out` (NUL-terminated, capped). Returns false at EOF.
    bool next(char* out, unsigned int cap)
    {
        while (m_p < m_end && isSpace(*m_p)) ++m_p;
        if (m_p >= m_end)
            return false;
        unsigned int n = 0;
        while (m_p < m_end && !isSpace(*m_p))
        {
            if (n + 1 < cap)
                out[n++] = *m_p;
            ++m_p;
        }
        out[n] = '\0';
        return true;
    }

    int   nextInt()   { char t[64];  return next(t, sizeof(t)) ? ::atoi(t) : 0; }
    float nextFloat() { char t[64];  return next(t, sizeof(t)) ? static_cast<float>(::atof(t)) : 0.0f; }
    bool  nextToken(char* out, unsigned int cap) { return next(out, cap); }

    // The header's font name occupies one complete line and may contain spaces. In particular the
    // stock dynamicText_13.dif spells it "Helvetica LT CondensedBold". Treating that line as one
    // whitespace token shifts `LT`/`CondensedBold` into the atlas dimensions, producing a 0x0 atlas
    // and an authored size of 256; the right-aligned EOR Game Mode/Map Size text is then submitted
    // but rasterized off-screen. The shipped reader consumes the name as a string line.
    bool nextLine(char* out, unsigned int cap)
    {
        // nextInt() leaves the cursor on the separator after the version.  Some shipped DIFs pad
        // that line with tabs before CR/LF, so consume the complete whitespace run; doing the two
        // classes separately would stop on the CR and return an empty font name.
        while (m_p < m_end && isSpace(*m_p)) ++m_p;
        if (m_p >= m_end)
        {
            if (cap) out[0] = '\0';
            return false;
        }

        unsigned int n = 0;
        while (m_p < m_end && *m_p != '\r' && *m_p != '\n')
        {
            if (n + 1 < cap)
                out[n++] = *m_p;
            ++m_p;
        }
        while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) --n;
        if (cap) out[n] = '\0';
        return true;
    }

private:
    static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
    const char* m_p;
    const char* m_end;
};

int compareGlyph(const void* a, const void* b)
{
    unsigned short ca = static_cast<const DifGlyph*>(a)->code;
    unsigned short cb = static_cast<const DifGlyph*>(b)->code;
    return (ca < cb) ? -1 : (ca > cb) ? 1 : 0;
}

} // anonymous namespace

DifGlyph* DifFont::insertGlyph(unsigned short code)
{
    if (m_glyphCount >= m_glyphCap)
    {
        const unsigned int cap = m_glyphCap ? m_glyphCap * 2 : 64;
        DifGlyph* grown = static_cast<DifGlyph*>(::renddx9abi::engineAlloc(cap * sizeof(DifGlyph)));
        if (!grown)
            return 0;
        for (unsigned int i = 0; i < m_glyphCount; ++i)
            grown[i] = m_glyphs[i];
        if (m_glyphs)
            ::renddx9abi::engineFree(m_glyphs);
        m_glyphs = grown;
        m_glyphCap = cap;
    }
    DifGlyph* g = &m_glyphs[m_glyphCount++];
    ::memset(g, 0, sizeof(*g));
    g->code = code;
    return g;
}

const DifGlyph* DifFont::glyph(unsigned short ch) const
{
    if (ch == 160)          // nbsp -> space, exactly as sub_100A38D0 folds it
        ch = 32;
    // binary search - the table was sorted after parsing
    unsigned int lo = 0, hi = m_glyphCount;
    while (lo < hi)
    {
        const unsigned int mid = (lo + hi) >> 1;
        if (m_glyphs[mid].code < ch)      lo = mid + 1;
        else if (m_glyphs[mid].code > ch) hi = mid;
        else                              return &m_glyphs[mid];
    }
    return 0;
}

float DifFont::kerning(unsigned short a, unsigned short b) const
{
    const unsigned int key = (static_cast<unsigned int>(a) << 16) | b;
    unsigned int lo = 0, hi = m_kernCount;
    while (lo < hi)
    {
        const unsigned int mid = (lo + hi) >> 1;
        if (m_kerns[mid].key < key)      lo = mid + 1;
        else if (m_kerns[mid].key > key) hi = mid;
        else                             return m_kerns[mid].amount;
    }
    return 0.0f;
}

// Writes one glyph record per input character; the caller supplies enough output capacity.
// Missing glyphs repeat the previous record without advancing. Nonbreaking spaces use the space glyph.
// [CHOSEN, not recovered] Uses caller-provided array storage.
void DifFont::layout(const wchar_t* text, unsigned int len, GlyphCell* out) const
{
    float pen = 0.0f;
    GlyphCell cell = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (unsigned int i = 0; i < len; ++i)
    {
        const unsigned short raw = static_cast<unsigned short>(text[i]);
        const DifGlyph* g = glyph(raw);                 // folds 160 -> 32
        if (g)
        {
            const float before = pen;
            if (i != 0)
                pen += kerning(static_cast<unsigned short>(text[i - 1]), raw);
            const float left = ::floorf(g->bearingX + pen + 0.5f);
            const float floorBefore = ::floorf(before);
            cell.left     = left;
            cell.cellLeft = (floorBefore >= left) ? left : floorBefore;
            const float end = g->rightBearing + g->intField + left;
            cell.right    = ::ceilf(end);
            cell.top      = ::floorf(g->bearingY);
            pen = end;
        }
        out[i] = cell;
    }
}

// Intentionally empty device lifecycle callbacks.
void DifFont::onDeviceLost()    {}
void DifFont::onDeviceReset()   {}
void DifFont::onDeviceDestroy() {}

int DifFont::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        // Unregister before releasing resources; unregistered failed-load candidates are harmless.
        dx9rend::unregisterDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this));
        if (m_glyphs) ::renddx9abi::engineFree(m_glyphs);
        if (m_kerns)  ::renddx9abi::engineFree(m_kerns);
        // The atlas is a counted texture reference - dropped through the manager.
        if (m_atlas)
            ::dice::hfe::dx9rend::textureManagerEvictFromCache(m_atlas);
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}


// sub_100A4420, faithfully: parse the .dif, load the .dds atlas, build the ortho projection from the
// device's current viewport, resolve the FontRenderer. The projection is the shipped one:
//     m00 =  2 / vpHeight-ish ... (see below) - it maps the font's pixel space to clip space using
// the back buffer viewport (device GetViewport), the same values the shipped code reads at v33..v36.

bool DifFont::load(const char* path)
{
    m_refCount = 1;

    // The .dif text file
    char difPath[512];
    ::_snprintf_s(difPath, sizeof(difPath), _TRUNCATE, "%s.dif", path);
    ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile(difPath);
    if (!file.ok())
        return false;      // this candidate path has no .dif - getFont tries the next and logs

    Tokenizer tok(reinterpret_cast<const char*>(file.data), file.size);
    char word[64];

    // header section
    if (!tok.nextToken(word, sizeof(word)) || ::strcmp(word, "header") != 0)
    {
        ::renddx9abi::freeEngineFile(file);
        ::renddx9abi::logf("FontSystem.cpp: \"%s.dif\" is missing its header", path);
        return false;
    }
    const int version = tok.nextInt();
    tok.nextLine(word, sizeof(word));             // the complete font-name line (unused here)
    m_atlasW = static_cast<unsigned int>(tok.nextInt());
    m_atlasH = (version <= 1) ? m_atlasW : static_cast<unsigned int>(tok.nextInt());
    m_size   = tok.nextFloat();

    // the remaining sections, in whatever order they appear
    while (tok.nextToken(word, sizeof(word)))
    {
        if (::strcmp(word, "glyphs") == 0)
        {
            const int count = tok.nextInt();
            for (int i = 0; i < count; ++i)
            {
                const int   code = tok.nextInt();
                const float f1   = tok.nextFloat();    // left side bearing
                const float f2   = tok.nextFloat();    // ink width
                const float f3   = tok.nextFloat();    // right side bearing
                const int   yoff = tok.nextInt();      // the INT = vertical Y-offset (top position)
                const int   x    = tok.nextInt();
                const int   y    = tok.nextInt();
                const int   x2   = tok.nextInt();
                const int   y2   = tok.nextInt();
                DifGlyph* g = insertGlyph(static_cast<unsigned short>(code));
                if (!g)
                    break;
                // Glyph advance is left bearing plus ink width plus right bearing; the integer field is a vertical offset.
                g->bearingX = f1;
                g->advance  = f1 + f2 + f3;
                g->bearingY = static_cast<float>(yoff);   // the vertical top offset
                g->intField = f2;                          // ink width
                g->rightBearing = f3;                      // right side bearing
                g->x  = static_cast<short>(x);
                g->y  = static_cast<short>(y);
                g->x2 = static_cast<short>(x2);
                g->y2 = static_cast<short>(y2);
                g->height = static_cast<float>(y2 - y);
            }
        }
        else if (::strcmp(word, "kerning") == 0)
        {
            const int count = tok.nextInt();
            if (count > 0)
            {
                m_kernCap = static_cast<unsigned int>(count);
                m_kerns = static_cast<Kern*>(::renddx9abi::engineAlloc(m_kernCap * sizeof(Kern)));
            }
            for (int i = 0; i < count; ++i)
            {
                // Kerning amounts are fractional and must be parsed as floats.
                const int   a   = tok.nextInt();
                const int   b   = tok.nextInt();
                const float amt = tok.nextFloat();
                if (m_kerns && m_kernCount < m_kernCap)
                {
                    m_kerns[m_kernCount].key =
                        (static_cast<unsigned int>(static_cast<unsigned short>(a)) << 16)
                        | static_cast<unsigned short>(b);
                    m_kerns[m_kernCount].amount = amt;
                    ++m_kernCount;
                }
            }
        }
    }
    ::renddx9abi::freeEngineFile(file);

    // Sort both tables so lookups are a binary search (the shipped std::map is ordered; ours is a
    // sorted array to the same observable effect).
    if (m_glyphs && m_glyphCount > 1)
        ::qsort(m_glyphs, m_glyphCount, sizeof(DifGlyph), &compareGlyph);
    if (m_kerns && m_kernCount > 1)
    {
        // insertion sort on the small kerning table by key
        for (unsigned int i = 1; i < m_kernCount; ++i)
        {
            Kern k = m_kerns[i];
            unsigned int j = i;
            while (j > 0 && m_kerns[j - 1].key > k.key) { m_kerns[j] = m_kerns[j - 1]; --j; }
            m_kerns[j] = k;
        }
    }

    // The .dds atlas
    char ddsPath[512];
    ::_snprintf_s(ddsPath, sizeof(ddsPath), _TRUNCATE, "%s.dds", path);
    m_atlas = ::dice::hfe::dx9rend::textureManagerGetTexture(vc71::string(ddsPath));

    // Builds the viewport orthographic projection, with translation in the fourth matrix row.
    void* device = ::dice::hfe::dx9rend::renderDevice();
    if (device)
    {
        D3DVIEWPORT9 vp;
        ::memset(&vp, 0, sizeof(vp));
        static_cast<IDirect3DDevice9*>(device)->GetViewport(&vp);
        const float w = static_cast<float>(vp.Width  ? vp.Width  : 1);
        const float h = static_cast<float>(vp.Height ? vp.Height : 1);
        for (int i = 0; i < 16; ++i) m_projection[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        m_projection[0]  =  2.0f / w;
        m_projection[5]  = -2.0f / h;
        m_projection[12] = 2.0f * static_cast<float>(vp.X) / w - 1.0f;
        m_projection[13] = 1.0f - 2.0f * static_cast<float>(vp.Y) / h;
        m_projVp[0] = (float) vp.X; m_projVp[1] = (float) vp.Y;     // [TEMP] RW-89 tracer only
        m_projVp[2] = (float) vp.Width; m_projVp[3] = (float) vp.Height;
    }

    // Register only after loading succeeds; requires the renderer's listener registry.
    dx9rend::registerDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this), 2);

    // The FontRenderer singleton (published by ModuleGeomMisc once this stack registers)
    if (::dice::hfe::classManager)
        m_fontRenderer = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("FontRenderer"));

    ::renddx9abi::logf("DifFont::load(\"%s\"): %u glyphs, %u kerning pairs, atlas %ux%u -> 0x%08X"
                       "  projFrom vp=(%d,%d,%d,%d)",
                       path, m_glyphCount, m_kernCount, m_atlasW, m_atlasH,
                       ::renddx9abi::addr(m_atlas),
                       (int) m_projVp[0], (int) m_projVp[1], (int) m_projVp[2], (int) m_projVp[3]);
    return true;
}


// the glyph vertex and the two vertex declarations, parsed from the shipped blobs:
//   unk_1020D1C0 (FONT_VBS)      -> position(FLOAT3)@0 + color(D3DCOLOR)@12 + texcoord(FLOAT2)@16
//   unk_1020D1E0 (fontselection) -> position(FLOAT3)@0 only
// Same 4-u16-per-element encoding RendDX9 uses everywhere (see SwiffRender's kSwiffDeclElements).

struct FontVertex { float x, y, z; unsigned int color; float u, v; };
static_assert(sizeof(FontVertex) == 24, "the FONT_VBS stride");

namespace {

const unsigned short kFontDeclElements[4][4] =
{
    { 0, 0,  0x0002, 0x0000 },      // stream 0, offset 0,  FLOAT3,   position
    { 0, 12, 0x0004, 0x000A },      // stream 0, offset 12, D3DCOLOR, color
    { 0, 16, 0x0001, 0x0005 },      // stream 0, offset 16, FLOAT2,   texcoord
    { 0xFF, 0, 0x0011, 0x0000 },    // D3DDECL_END
};
const unsigned short kFontSelDeclElements[2][4] =
{
    { 0, 0,  0x0002, 0x0000 },      // position only
    { 0xFF, 0, 0x0011, 0x0000 },    // D3DDECL_END
};

// sub_10014930 - the EngineTexture's D3D texture lives at its +8 (dword index 2).
void* engineTextureD3D(void* engineTexture)
{
    return engineTexture ? reinterpret_cast<void**>(engineTexture)[2] : 0;
}

// ShaderParameter / D3DX helpers. The handle has a recovered typed owner contract, so retrieving
// its D3DX handle is an ordinary member call.
void* paramGetHandle(void* p)     // +0x0C (sub_10021190): return m_handle
{
    if (!p) return 0;
    return static_cast<dx9rend::ShaderParameter*>(p)->getD3DXParam();
}
void fxSetTexture(void* fx, void* handle, void* texture)
{
    if (!fx) return;
    static_cast<ID3DXEffect*>(fx)->SetTexture(::renddx9abi::d3dxHandle(handle),
                                              static_cast<IDirect3DBaseTexture9*>(texture));
}

// Row-major 4x4 multiply, out = a * b (D3DXMatrixMultiply's convention).
void matMul(float* out, const float* a, const float* b)
{
    float r[16];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j]
                         + a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
    ::memcpy(out, r, sizeof(r));
}

} // anonymous namespace


// [CHOSEN, not recovered] Draws each run immediately with CPU-transformed clip-space vertices.
// Registers for device resets at priority 5.
// [RETIRE-WHEN: RW-80] Implement and validate the original three-buffer batching path.

extern const unsigned int CID_FontRenderer = 81047u;   // matches ModuleGeomMisc's getSingletonList
extern const unsigned int IID_IFontRenderer = 80048u;

const void** fontRendererVtable();          // defined after the shims below
const void** fontRendererListenerVtable();

namespace {
// The fontselection unit quad {(0,0,0),(1,0,0),(0,1,0),(1,1,0)} - the twelve dwords both the init
// (0x100A143C..0x100A145C) and the reset (0x100A17D8..0x100A17F8) store, in the same order.
const float kFontSelectionQuad[12] = { 0,0,0,  1,0,0,  0,1,0,  1,1,0 };
} // anonymous namespace

class FontRenderer
{
public:
    FontRenderer();                                         // geom_FontRenderer_ctor, 0x100A1740

    bool init();                                            // sub_100A1240 (lazy, on first draw)
    void refindTechniques();                                // sub_100A1470
    void drawRun(const FontVertex* verts, unsigned int count, void* atlas, bool scaled);

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;
    void  destroy(char flags);                              // +0x10

    void  onDeviceLost();                                   // listener +0x00, 0x100A1220
    void  onDeviceReset();                                  // listener +0x04, 0x100A17B0
    // Both listener bodies are handed the subobject (0x100A1883 steps back with `lea ecx,[esi-4]`).
    static FontRenderer* fromListener(void* sub)
    {
        return reinterpret_cast<FontRenderer*>(static_cast<char*>(sub) - 4);
    }

private:
    bool ensureInit();
    void teardown();                                        // sub_100A18A0
    void advanceRing();                                     // sub_100A1660, the ring step

    const void** m_vptr;           // +0 - IBase vtable (must be first; queryInterface returns this)
    const void** m_listenerVptr;   // +4 - the DeviceLostResetListener subobject (0x1020D268)
    void* m_effect;
    void* m_paramTransform;
    void* m_paramDiffuse;
    void* m_paramTexture;
    void* m_decl;                  // FONT_VBS declaration (pos+color+uv)
    void* m_selDecl;               // fontselection declaration (pos only)
    void* m_vb[3];                 // FONT_VBS ring
    void* m_selVb;                 // fontselection quad
    void* m_techRegular;
    void* m_techRegularScaled;
    void* m_techSelectionQuad;
    unsigned int m_vbIndex;
    bool  m_initialized;
    bool  m_initFailed;
    // [CHOSEN] the shipped +0x5C "ring buffer locked" byte. drawRun raises it only across its own
    // lock..unlock, so a lost walk never finds it set here; the shipped batcher left the buffer
    // locked between runs, which is what its lost body exists to undo.
    bool  m_ringLocked;
    int   m_refCount;
};

// Registers at priority 5 before clearing the ring-buffer slots. Requires the renderer's listener registry.
FontRenderer::FontRenderer()
    : m_vptr(fontRendererVtable()), m_listenerVptr(fontRendererListenerVtable()),
      m_effect(0), m_paramTransform(0), m_paramDiffuse(0), m_paramTexture(0),
      m_decl(0), m_selDecl(0), m_selVb(0),
      m_techRegular(0), m_techRegularScaled(0), m_techSelectionQuad(0),
      m_vbIndex(0), m_initialized(false), m_initFailed(false), m_ringLocked(false), m_refCount(1)
{
    static_assert(offsetof(FontRenderer, m_listenerVptr) == 4,
                  "both FontRenderer listener bodies are handed this + 4");
    dx9rend::registerDeviceLostResetListener(&m_listenerVptr, 5);
    m_vb[0] = m_vb[1] = m_vb[2] = 0;
}

bool FontRenderer::init()
{
    using namespace dx9rend;
    m_effect = shaderManagerGetEffect(g_shaderManager, "mods/xpack/Shaders/Font");
    if (!m_effect)
    {
        ::renddx9abi::logf("[FRONTIER] FontRenderer::init: getEffect(\"mods/xpack/Shaders/Font\") "
                           "returned null - no font effect, text will not render");
        return false;
    }
    Effect* fx = static_cast<Effect*>(m_effect);
    m_paramTransform = effectGetShaderParameter(fx, vc71::string("TRANSFORM"));
    m_paramDiffuse   = effectGetShaderParameter(fx, vc71::string("DIFFUSE"));
    m_paramTexture   = effectGetShaderParameter(fx, vc71::string("TEXTURE"));

    m_decl    = vertexDeclarationCreate(kFontDeclElements);
    m_selDecl = vertexDeclarationCreate(kFontSelDeclElements);

    m_techRegular       = effectFindTechniqueRecord(m_effect, "Regular");
    m_techRegularScaled = effectFindTechniqueRecord(m_effect, "RegularScaled");
    m_techSelectionQuad = effectFindTechniqueRecord(m_effect, "SelectionQuad");

    const int prevPool = bufferManagerSwapPool(0);
    for (int i = 0; i < 3; ++i)
        m_vb[i] = bufferManagerCreateVertexBuffer(10002, 24, 0x208, "FONT_VBS");
    m_selVb = bufferManagerCreateVertexBuffer(4, 12, 0x208, "FONTSELECTION");
    bufferManagerSwapPool(prevPool);

    // Seed the fontselection quad with the shipped unit quad {(0,0,0),(1,0,0),(0,1,0),(1,1,0)}.
    float* q = static_cast<float*>(vertexBufferLock(m_selVb, 0, 0, 0x2000));
    if (q)
    {
        ::memcpy(q, kFontSelectionQuad, sizeof(kFontSelectionQuad));
        vertexBufferUnlock(m_selVb);
    }

    ::renddx9abi::logf("FontRenderer::init: effect=0x%08X techniques R/RS/SQ=0x%08X/0x%08X/0x%08X, "
                       "3x FONT_VBS + FONTSELECTION created",
                       ::renddx9abi::addr(m_effect), ::renddx9abi::addr(m_techRegular),
                       ::renddx9abi::addr(m_techRegularScaled), ::renddx9abi::addr(m_techSelectionQuad));
    return true;
}

void FontRenderer::refindTechniques()
{
    if (!m_effect)
        return;
    m_techRegular       = dx9rend::effectFindTechniqueRecord(m_effect, "Regular");
    m_techRegularScaled = dx9rend::effectFindTechniqueRecord(m_effect, "RegularScaled");
    m_techSelectionQuad = dx9rend::effectFindTechniqueRecord(m_effect, "SelectionQuad");
    m_vbIndex = 0;
}

void FontRenderer::drawRun(const FontVertex* verts, unsigned int count, void* atlas, bool scaled)
{
    using namespace dx9rend;
    if (!count || !ensureInit() || !m_vb[m_vbIndex])
        return;

    // The FONT_VBS holds 10002 vertices; never memcpy past it. A single run this long is not a real
    // menu string, but clamping keeps a pathological caller from overrunning the buffer.
    if (count > 10002u)
        count = (10002u / 3u) * 3u;

    void* vb = m_vb[m_vbIndex];
    void* base = vertexBufferLock(vb, 0, 0, 0x2000);       // D3DLOCK_DISCARD
    if (!base)
        return;
    m_ringLocked = true;                                   // the shipped +0x5C, for the lost body
    ::memcpy(base, verts, count * sizeof(FontVertex));
    vertexBufferUnlock(vb);
    m_ringLocked = false;

    effectSetTechniqueRecord(m_effect, scaled ? m_techRegularScaled : m_techRegular);
    fxSetTexture(effectD3dxHandle(m_effect), paramGetHandle(m_paramTexture), engineTextureD3D(atlas));

    vertexBufferBindStream(vb, 0);
    vertexDeclarationBind(m_decl);

    // sub_100A15A0 issues a single pass (begin -> beginPass(0) -> DrawPrimitive -> end); the Font
    // technique is single-pass, so it never loops passes or calls EndPass. Matched here.
    effectBegin(m_effect);
    effectBeginPass(m_effect, 0);
    rendererDrawPrimitive(count / 3, 4 /* D3DPT_TRIANGLELIST */, 0);
    effectEnd(m_effect);

    m_vbIndex = (m_vbIndex + 1) % 3;
}

// GPU init is deferred to the first glyph draw so that registering the singleton (which happens at
// module time, possibly before the D3D device and shader manager are live) can never fault or disturb
// the already-verified menu. By the first text draw the menu is up and the device is ready.
// (The constructor's device-lost registration is not deferred: it needs only the renderer base's
// list, which DX9RendModule - registered first by initDll - builds in the Renderer's constructor
// before ModuleGeomMisc's singletons exist. A registered renderer with nothing initialized walks
// through both listener bodies without touching anything: every step is null-tested.)
bool FontRenderer::ensureInit()
{
    if (m_initialized) return true;
    if (m_initFailed)  return false;
    if (init()) { m_initialized = true; return true; }
    m_initFailed = true;
    return false;
}


// Unlocks an outstanding ring buffer before the lower-priority BufferManager releases it.
// The current immediate drawing path does not leave buffers locked between calls.

void FontRenderer::onDeviceLost()
{
    if (m_ringLocked)
    {
        dx9rend::vertexBufferUnlock(m_vb[m_vbIndex]);
        m_ringLocked = false;
    }
}


// Refills the selection quad, refreshes shader techniques and restarts the vertex-buffer ring.

void FontRenderer::onDeviceReset()
{
    if (m_selVb)
    {
        float* q = static_cast<float*>(dx9rend::vertexBufferLock(m_selVb, 0, 0, 0x2000));
        if (q)
            ::memcpy(q, kFontSelectionQuad, sizeof(kFontSelectionQuad));
        dx9rend::vertexBufferUnlock(m_selVb);
    }
    if (m_effect)
    {
        m_techRegular       = dx9rend::effectFindTechniqueRecord(m_effect, "Regular");
        m_techRegularScaled = dx9rend::effectFindTechniqueRecord(m_effect, "RegularScaled");
        m_techSelectionQuad = dx9rend::effectFindTechniqueRecord(m_effect, "SelectionQuad");
    }
    if (m_vb[m_vbIndex])
        advanceRing();
}

// sub_100A1660 - the ring step: unlock the current buffer if locked, advance the index (wrapping at
// 3), and restart the batch on the new buffer. [CHOSEN] Only the first two are this model's: the
// shipped tail then takes a D3DLOCK_DISCARD lock on the new buffer and keeps it (+0x2C pointer, +0x5C
// = 1) and resets the batch counters (+0x30/+0x54/+0x58 = 0, +0x64 = +0x60, +0x68 += 1). drawRun
// takes its own discard lock for every run, so there is no persistent lock or batch to restart.
void FontRenderer::advanceRing()
{
    if (m_ringLocked)
    {
        dx9rend::vertexBufferUnlock(m_vb[m_vbIndex]);
        m_ringLocked = false;
    }
    if (++m_vbIndex >= 3)
        m_vbIndex = 0;
}

// Releases shader parameters, effects, declarations and buffers, checking each resource for null.
void FontRenderer::teardown()
{
    if (m_paramTransform) static_cast<dx9rend::ShaderParameter*>(m_paramTransform)->release();
    m_paramTransform = 0;
    if (m_paramDiffuse)   static_cast<dx9rend::ShaderParameter*>(m_paramDiffuse)->release();
    m_paramDiffuse = 0;
    if (m_paramTexture)   static_cast<dx9rend::ShaderParameter*>(m_paramTexture)->release();
    m_paramTexture = 0;
    if (m_effect)         dx9rend::effectRelease(static_cast<dx9rend::Effect*>(m_effect));
    m_effect = 0;

    if (m_decl)    dx9rend::vertexDeclarationRelease(m_decl);
    m_decl = 0;
    if (m_selDecl) dx9rend::vertexDeclarationRelease(m_selDecl);
    m_selDecl = 0;

    for (int i = 0; i < 3; ++i)
    {
        if (m_vb[i])
        {
            dx9rend::bufferManagerRemoveVertexBuffer(m_vb[i]);
            if (m_vb[i])                                   // the shipped re-read (0x100A1949)
                dx9rend::vertexBufferRelease(m_vb[i]);
        }
        m_vb[i] = 0;
    }
    if (m_selVb)
        dx9rend::bufferManagerRemoveVertexBuffer(m_selVb);
    if (m_selVb)
        dx9rend::vertexBufferRelease(m_selVb);
    m_selVb = 0;
}

// Final release invokes the virtual destructor so the listener is unregistered before storage is freed.
int FontRenderer::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        typedef void* (__fastcall* DestroyFn)(void*, void*, char);
        reinterpret_cast<DestroyFn>(const_cast<void*>(m_vptr[0x10 / 4]))(this, 0, 1);
    }
    return remaining;
}

// Unregisters the listener before teardown and frees storage when flags bit zero is set.
void FontRenderer::destroy(char flags)
{
    m_vptr         = fontRendererVtable();
    m_listenerVptr = fontRendererListenerVtable();
    dx9rend::unregisterDeviceLostResetListener(&m_listenerVptr);
    teardown();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
}

void* FontRenderer::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IFontRenderer)
        return const_cast<FontRenderer*>(this);
    return 0;
}

namespace {
void  __fastcall fr_addRef(void* s, void*)  { static_cast<FontRenderer*>(s)->addRef(); }
int   __fastcall fr_getRef(void* s, void*)  { return static_cast<FontRenderer*>(s)->getRef(); }
int   __fastcall fr_release(void* s, void*) { return static_cast<FontRenderer*>(s)->release(); }
void* __fastcall fr_queryInterface(void* s, void*, unsigned int iid)
{ return static_cast<FontRenderer*>(s)->queryInterface(iid); }
void* __fastcall fr_destroy(void* s, void*, char flags)
{ static_cast<FontRenderer*>(s)->destroy(flags); return s; }

// 0x1020D274, five slots in the shipped order.
const void* kFontRendererVtable[5] =
{
    reinterpret_cast<void*>(&fr_addRef),          // +0x00
    reinterpret_cast<void*>(&fr_getRef),          // +0x04
    reinterpret_cast<void*>(&fr_release),         // +0x08
    reinterpret_cast<void*>(&fr_queryInterface),  // +0x0C
    reinterpret_cast<void*>(&fr_destroy),         // +0x10
};

// 0x1020D268, the listener table (the three dwords before the primary).
void __fastcall fr_onDeviceLost(void* sub, void*)  { FontRenderer::fromListener(sub)->onDeviceLost(); }
void __fastcall fr_onDeviceReset(void* sub, void*) { FontRenderer::fromListener(sub)->onDeviceReset(); }
void __fastcall fr_onDeviceDestroy(void*, void*)   { /* 0x10037D40: ret, the shared nullsub */ }

const void* kFontRendererListenerVtable[3] =
{
    reinterpret_cast<void*>(&fr_onDeviceLost),    // +0x00
    reinterpret_cast<void*>(&fr_onDeviceReset),   // +0x04
    reinterpret_cast<void*>(&fr_onDeviceDestroy), // +0x08
};
} // anonymous namespace

const void** fontRendererVtable() { return kFontRendererVtable; }
const void** fontRendererListenerVtable() { return kFontRendererListenerVtable; }

// RefFactory for dice.hfe.geom.FontRenderer. init() is not called here - it runs lazily on first draw.
IBase* __fastcall createFontRenderer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(FontRenderer));
    if (!storage)
        return 0;
    FontRenderer* self = new (storage) FontRenderer();
    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}


// Font object exposed through an opaque handle.
// [CHOSEN, not recovered] Internal storage layout; the public virtual interface remains fixed.

class SwiffFontObject
{
public:
    SwiffFontObject(DifFont* font, const wchar_t* initialText);

    // The slots SwiffRender forwards to (all __fastcall(self, edx, ...) to match __thiscall dispatch).
    static void  __fastcall vRelease(void* self, void*);                                    // +8
    static int   __fastcall vSetText(void* self, void*, const wchar_t* text, int flag);     // +16
    static void  __fastcall vDraw(void* self, void*, const float* m, unsigned int color);   // +20
    static char  __fastcall vGetExtent(void* self, void*, int arg, float* outW, float* outH);// +28
    static char  __fastcall vForward32(void* self, void*, int arg, float x, float y);       // +32
    static char  __fastcall vForward40(void* self, void*, int a, int b);                     // +40
    static char  __fastcall vForward44(void* self, void*, float x1, float y1,
                                       float x2, float y2);                                  // +44
    static int   __fastcall vForward48(void* self, void*);                                   // +48

    DifFont* font() const { return m_font; }

    // The two setters SwiffRender's font slots drive (sub_101530D0 / sub_10153120). Public so the
    // namespace-scope seams can reach them; the shipped code pokes the equivalent byte offsets.
    void configure(float boxWidth, int align);   // +120 box width, +136 alignment (offset left to setText)
    void setScrollIndex(int value);               // +104 caret index, then the visible window (sub_100A3710)

private:
    void setText(const wchar_t* text, int flag);
    void setColor(unsigned int rgba);
    void draw(const float* m, unsigned int color);
    bool measure(unsigned int index, float* outW, float* outH) const;
    void recomputeWindow();

    static const unsigned int kTextCap = 512;

    const void** m_vptr;           // must be first - SwiffRender reads *font as the vtable
    DifFont*     m_font;
    // [CHOSEN, not recovered] Inline text storage avoids repeated heap allocation; the original uses vc71::wstring.
    wchar_t      m_textBuf[kTextCap];
    unsigned int m_textLen;
    unsigned int m_color;          // packed D3DCOLOR the emitter writes per-vertex (shipped +68)
    float        m_colR, m_colG, m_colB, m_colA;   // components (shipped +76..88), kept for the shadow
    float        m_boxWidth;       // alignment box width (shipped +120)
    int          m_align;          // 0=left/none, 1=right, 2=center (shipped +136; sub_100A4F20's enum)
    float        m_alignOffset;    // computed x offset (shipped +140)
    unsigned int m_shadowColor2;   // shipped +128, 0 = no drop shadow
    // Visible glyph range computed by setScrollIndex to fit the box and keep the caret in view.
    unsigned int m_scrollIndex;    // shipped +104, the caret index the window keeps in view
    unsigned int m_firstVisible;   // shipped +108, the first glyph drawn
    unsigned int m_visibleCount;   // shipped +112, how many glyphs are drawn from there
    float        m_scrollOffset;   // shipped +116, -(scaleX * left of the first drawn glyph), pixels
    int          m_refCount;

    void assignText(const wchar_t* text);
    void computeAlign();
};

// Wrapper dispatch table. Unused window and interactive-editing slots remain null; drawing reads window fields directly.
namespace {
const void* kSwiffFontVtable[13] =
{
    0,                                                              // 0
    0,                                                              // 1
    reinterpret_cast<void*>(&SwiffFontObject::vRelease),            // 2  (+8)  release
    0,                                                              // 3
    reinterpret_cast<void*>(&SwiffFontObject::vSetText),           // 4  (+16) setText
    reinterpret_cast<void*>(&SwiffFontObject::vDraw),              // 5  (+20) draw
    0,                                                              // 6
    reinterpret_cast<void*>(&SwiffFontObject::vGetExtent),        // 7  (+28) measure
    reinterpret_cast<void*>(&SwiffFontObject::vForward32),         // 8  (+32)
    0,                                                              // 9
    reinterpret_cast<void*>(&SwiffFontObject::vForward40),         // 10 (+40)
    reinterpret_cast<void*>(&SwiffFontObject::vForward44),         // 11 (+44)
    reinterpret_cast<void*>(&SwiffFontObject::vForward48),         // 12 (+48)
};

// [CHOSEN, not recovered] Interactive text queries return not-handled and report once.
// [RETIRE-WHEN: RW-80] Replace when editable-field behavior is implemented.
void fontForwardNote()
{
    static bool noted = false;
    if (noted) return;
    noted = true;
    ::renddx9abi::logf("[FRONTIER] SwiffFontObject: an interactive text-field query slot "
                       "(+32/+40/+44/+48) was reached - display labels return 'not handled'; the "
                       "editable-field path (sub_100A51B0/sub_10152080/sub_100A5200/sub_100A3440) is "
                       "unwritten (RW-80)");
}
} // anonymous namespace

SwiffFontObject::SwiffFontObject(DifFont* font, const wchar_t* initialText)
    : m_vptr(kSwiffFontVtable), m_font(font), m_textLen(0),
      m_color(0xFFFFFFFFu), m_colR(1.0f), m_colG(1.0f), m_colB(1.0f), m_colA(1.0f),
      m_boxWidth(0.0f), m_align(0), m_alignOffset(0.0f), m_shadowColor2(0),
      m_scrollIndex(0), m_firstVisible(0), m_visibleCount(0), m_scrollOffset(0.0f), m_refCount(1)
{
    m_textBuf[0] = 0;
    if (font)
        font->addRef();
    if (initialText)
        assignText(initialText);
}

void SwiffFontObject::assignText(const wchar_t* text)
{
    unsigned int n = 0;
    while (text && text[n] && n + 1 < kTextCap)
    {
        m_textBuf[n] = text[n];
        ++n;
    }
    m_textBuf[n] = 0;
    m_textLen = n;
}

// sub_100A4F20 - store the text, then compute the alignment x-offset from the box width.
static bool swiffFontTraceOn();   // [TEMP] RW-89, defined beside configure() below
void SwiffFontObject::setText(const wchar_t* text, int /*flag*/)
{
    if (swiffFontTraceOn())
    {
        char narrow[64]; int n = 0;
        for (const wchar_t* p = text; p && *p && n < 63; ++p) narrow[n++] = (*p < 128) ? (char) *p : '?';
        narrow[n] = 0;
        ::renddx9abi::logf("[font] text %p '%s'", this, narrow);
    }
    assignText(text);
    computeAlign();
}

// sub_100A4F20's offset arm. The alignment enum is 0=left, 1=right, 2=center; the offset is the
// slack between the field's box width and the measured text width, taken whole for right and halved
// for center. Any other value leaves the previous offset untouched (the shipped early return). This
// runs from setText, not from configure - so the field's box width and alignment must be set before
// the text is, which is the order SwiffPlayer uses.
void SwiffFontObject::computeAlign()
{
    if (m_align == 1 || m_align == 2)          // right / center
    {
        float w = 0.0f, h = 0.0f;
        measure(m_textLen, &w, &h);             // the whole text (sub_100A4810 with wcslen); w stays 0
                                                // for empty/no-font, matching the shipped path
        m_alignOffset = (m_align == 2) ? (m_boxWidth - w) * 0.5f
                                       : (m_boxWidth - w);
        // [TEMP] RW-89 alignment tracer: box width, measured text width and the resulting offset,
        // the three numbers a centring complaint actually needs. Same gate as the cfg/text lines.
        if (swiffFontTraceOn())
            ::renddx9abi::logf("[font] align %p box=%.1f textw=%.1f align=%d -> off=%.1f fontScale=%.4f",
                               this, m_boxWidth, w, m_align, m_alignOffset,
                               m_font ? m_font->scaleX() : -1.0f);
    }
    else if (m_align == 0)                       // left / none
    {
        m_alignOffset = 0.0f;
    }
    // m_align >= 3: leave m_alignOffset unchanged (sub_100A4F20 returns without writing +140).
}

// [TEMP] Enables font layout tracing when BF2_RENDDX9_SWIFFLOG is set.
static bool swiffFontTraceOn()
{
    static int s_on = -1;
    if (s_on < 0) { char v[8]; s_on = ::GetEnvironmentVariableA("BF2_RENDDX9_SWIFFLOG", v, sizeof(v)) > 0 ? 1 : 0; }
    return s_on == 1;
}

void SwiffFontObject::configure(float boxWidth, int align)
{
    if (swiffFontTraceOn() && (m_boxWidth != boxWidth || m_align != align))
        ::renddx9abi::logf("[font] cfg %p w=%.1f align=%d", this, boxWidth, align);
    m_boxWidth = boxWidth;
    m_align    = align;
}

// Stores the caret index and recomputes the visible glyph window.
void SwiffFontObject::setScrollIndex(int value)
{
    m_scrollIndex = static_cast<unsigned int>(value);
    recomputeWindow();
}

// Recomputes the glyph range fitting the scaled box while keeping the caret's preceding glyph visible.
// Empty text clears the range and horizontal offset.
void SwiffFontObject::recomputeWindow()
{
    if (!m_font)
        return;
    const unsigned int len = m_textLen;
    m_visibleCount = 0;
    if (!len)
    {
        m_firstVisible = 0;
        m_scrollOffset = 0.0f;
        return;
    }
    if (m_scrollIndex > len)
        m_scrollIndex = len;
    if (m_firstVisible > m_scrollIndex)
        m_firstVisible = m_scrollIndex;
    if (m_firstVisible >= len)
        m_firstVisible = len - 1;
    const float box = ::floorf(m_boxWidth / m_font->scaleX() + 0.5f);

    GlyphCell cells[kTextCap];
    m_font->layout(m_textBuf, len, cells);
    while (m_scrollIndex > m_firstVisible
           && cells[m_scrollIndex - 1].right - cells[m_firstVisible].left > box)
        ++m_firstVisible;
    while (m_firstVisible + m_visibleCount < len
           && cells[m_firstVisible + m_visibleCount].right - cells[m_firstVisible].left <= box)
        ++m_visibleCount;
    m_scrollOffset = -(m_font->scaleX() * cells[m_firstVisible].left);
}

// sub_100A3000 - unpack the incoming D3DCOLOR to four [0,1] floats, then repack with the shipped
// channel swizzle (argb in -> the byte order the Font atlas/shader expects). Rounding is identity for
// 0..255 inputs, so the packed result is just the swizzle; the float components are kept for reuse.
void SwiffFontObject::setColor(unsigned int rgba)
{
    const unsigned int b0 = (rgba)       & 0xFF;
    const unsigned int b1 = (rgba >> 8)  & 0xFF;
    const unsigned int b2 = (rgba >> 16) & 0xFF;
    const unsigned int b3 = (rgba >> 24) & 0xFF;
    m_colB = static_cast<float>(b0) * 0.0039215689f;
    m_colG = static_cast<float>(b1) * 0.0039215689f;
    m_colR = static_cast<float>(b2) * 0.0039215689f;
    m_colA = static_cast<float>(b3) * 0.0039215689f;
    m_color = (b3 << 24) | (b0 << 16) | (b1 << 8) | b2;
}

// Measures the scaled start of character index, or the final right edge when index is past the text.
// outH receives the glyph's vertical offset. Returns false with zero outputs for empty text.
bool SwiffFontObject::measure(unsigned int index, float* outW, float* outH) const
{
    if (outW) *outW = 0.0f;
    if (outH) *outH = 0.0f;
    if (!m_font || !m_textLen)
        return false;

    GlyphCell cells[kTextCap];
    m_font->layout(m_textBuf, m_textLen, cells);
    const GlyphCell* cell;
    float x;
    if (index < m_textLen)
    {
        cell = &cells[index];
        x = cell->cellLeft;
    }
    else
    {
        cell = &cells[m_textLen - 1];
        x = cell->right;
    }
    if (outW) *outW = m_font->scaleX() * x;
    if (outH) *outH = cell->top * m_font->scaleY();
    return true;
}

// Draws the computed visible glyph range with pixel-snapped positioning, alignment and scrolling offsets.
// A window with no visible glyphs draws nothing.
void SwiffFontObject::draw(const float* m, unsigned int color)
{
    setColor(color);
    if (!m_font || !m_textLen)
        return;
    if (m_font->scaleX() == 0.0f || m_font->scaleY() == 0.0f)    // sub_100A3DE0's first tests
        return;
    if (!m_visibleCount)                                            // slot 0's count, 0 -> return
        return;
    FontRenderer* fr = static_cast<FontRenderer*>(m_font->fontRenderer());
    if (!fr)
        return;

    // [TEMP] RW-89 vertical tracer: the line origin this label is drawn from, in screen pixels,
    // beside the first glyph's own top offset and height. Those are the only two inputs to where a
    // label lands vertically, so a label that sits low is one or the other.
    if (swiffFontTraceOn())
    {
        const DifGlyph* g0 = 0;
        for (unsigned int i = 0; i < m_textLen && !g0; ++i)
            g0 = m_font->glyph(m_textBuf[i] == 160 ? 32 : m_textBuf[i]);
        char narrow[32]; int n = 0;
        for (unsigned int i = 0; i < m_textLen && n < 31; ++i)
            narrow[n++] = (m_textBuf[i] < 128) ? (char) m_textBuf[i] : '?';
        narrow[n] = 0;
        int dvp[4] = {0, 0, 0, 0};
        if (void* dev = ::dice::hfe::dx9rend::renderDevice())
        {
            D3DVIEWPORT9 v; ::memset(&v, 0, sizeof(v));
            static_cast<IDirect3DDevice9*>(dev)->GetViewport(&v);
            dvp[0] = (int) v.X; dvp[1] = (int) v.Y; dvp[2] = (int) v.Width; dvp[3] = (int) v.Height;
        }
        const float* pv = m_font->projVp();
        ::renddx9abi::logf("[font] draw %p '%s' lineY=%.1f lineX=%.1f off=%.1f yoff=%.1f gh=%.1f size=%.1f"
                           "  projVp=(%d,%d,%d,%d) drawVp=(%d,%d,%d,%d) win=%u+%u/%u box=%.1f scroll=%.1f",
                           this, narrow, m[13], m[12], m_alignOffset,
                           g0 ? g0->bearingY : -999.0f, g0 ? g0->height : -999.0f, m_font->size(),
                           (int) pv[0], (int) pv[1], (int) pv[2], (int) pv[3],
                           dvp[0], dvp[1], dvp[2], dvp[3],
                           m_firstVisible, m_visibleCount, m_textLen, m_boxWidth, m_scrollOffset);
    }

    // Round horizontal placement with floor(x + offsets + 0.5); vertical placement uses floor(y).
    float t[16];
    ::memcpy(t, m, sizeof(t));
    t[12] = ::floorf(m[12] + (m_alignOffset + m_scrollOffset) + 0.5f);
    t[13] = ::floorf(m[13]);
    t[0]  = 1.0f;                        // sub_100A3DE0 forces the diagonal scale to 1
    t[5]  = 1.0f;

    float tmp[16];
    matMul(tmp, t, m_font->projection());               // textMatrix x projection

    float scaling[16] = { 0 };
    scaling[0]  = m_font->scaleX();
    scaling[5]  = m_font->scaleY();
    scaling[10] = 1.0f;
    scaling[15] = 1.0f;
    float fm[16];
    matMul(fm, scaling, tmp);                            // scaling x (textMatrix x projection)

    // Shader scaling uses scaleX > 1; texel bias separately tests exact unit scale.
    const bool  scaled = m_font->scaleX() > 1.0f;
    const float invW = 1.0f / static_cast<float>(m_font->atlasW() ? m_font->atlasW() : 1);
    const float invH = 1.0f / static_cast<float>(m_font->atlasH() ? m_font->atlasH() : 1);
    const float bias = (m_font->scaleX() == 1.0f) ? 0.0099999998f : 0.0f;

    FontVertex* scratch = static_cast<FontVertex*>(
        ::renddx9abi::engineAlloc(6 * m_visibleCount * sizeof(FontVertex)));
    if (!scratch)
        return;

    // The positions come from the layout (sub_100A4BF0), read at the window's indices: the emitter
    // walks records and characters from the first visible one (a4 = slot 1's answer, sub_100A33D0).
    // [CHOSEN] `i < m_textLen` is added to the window bound: the shipped trusts the window, which is
    // stale only if the text shrinks between a caret call and a draw - never on SwiffPlayer's path
    // (formatText ends with the caret calls) - and this port's buffers are fixed arrays.
    GlyphCell cells[kTextCap];
    m_font->layout(m_textBuf, m_textLen, cells);
    const unsigned int end = m_firstVisible + m_visibleCount;
    unsigned int vc = 0;
    for (unsigned int i = m_firstVisible; i < end && i < m_textLen; ++i)
    {
        unsigned short ch = m_textBuf[i];
        if (ch == 160) ch = 32;
        const DifGlyph* g = m_font->glyph(ch);
        if (!g)
            continue;

        if (g->x2 > g->x && g->y2 > g->y)                // a visible (non-space) glyph
        {
            const float u1 = (static_cast<float>(g->x)  + bias) * invW;
            const float v1 = (static_cast<float>(g->y)  + bias) * invH;
            const float u2 = (static_cast<float>(g->x2) + bias) * invW;
            const float v2 = (static_cast<float>(g->y2) + bias) * invH;
            // Position: the laid-out left (the pen plus the left bearing, snapped to a whole unit)
            // and top (the glyph's own vertical Y-offset from the .dif int), each less half a unit.
            // The Y-offset already baseline-aligns everything - caps at 0/-1, x-height at 2,
            // comma/period at 8 - so short and tall letters share the baseline and descenders drop
            // below it, exactly as the shipped layout does.
            const float glyphH = static_cast<float>(g->y2 - g->y);
            const float left   = cells[i].left - 0.5f;
            const float top    = cells[i].top - 0.5f;
            const float right  = left + static_cast<float>(g->x2 - g->x);
            const float bottom = top  + glyphH;

            FontVertex tl, tr, bl, br;
            tl.x = left  * fm[0] + top    * fm[4] + fm[8] * 0.5f + fm[12];
            tl.y = left  * fm[1] + top    * fm[5] + fm[9] * 0.5f + fm[13];
            tr.x = right * fm[0] + top    * fm[4] + fm[8] * 0.5f + fm[12];
            tr.y = right * fm[1] + top    * fm[5] + fm[9] * 0.5f + fm[13];
            bl.x = left  * fm[0] + bottom * fm[4] + fm[8] * 0.5f + fm[12];
            bl.y = left  * fm[1] + bottom * fm[5] + fm[9] * 0.5f + fm[13];
            br.x = right * fm[0] + bottom * fm[4] + fm[8] * 0.5f + fm[12];
            br.y = right * fm[1] + bottom * fm[5] + fm[9] * 0.5f + fm[13];

            tl.z = tr.z = bl.z = br.z = 0.0f;
            tl.color = tr.color = bl.color = br.color = m_color;
            tl.u = u1; tl.v = v1;
            tr.u = u2; tr.v = v1;
            bl.u = u1; bl.v = v2;
            br.u = u2; br.v = v2;

            scratch[vc++] = tl; scratch[vc++] = tr; scratch[vc++] = bl;
            scratch[vc++] = bl; scratch[vc++] = tr; scratch[vc++] = br;
        }
    }

    if (vc)
        fr->drawRun(scratch, vc, m_font->atlas(), scaled);
    ::renddx9abi::engineFree(scratch);
}

// [CHOSEN, not recovered] Decrements the wrapper reference without freeing it while list ownership is unfinished.
// [RETIRE-WHEN: RW-80 font-list ops] The owning font list must release wrappers during removal.
void __fastcall SwiffFontObject::vRelease(void* self, void*)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    if (o->m_refCount > 0)
        --o->m_refCount;
    // No free - the font list owns the wrapper; its (withheld) erase performs the teardown.
}
int __fastcall SwiffFontObject::vSetText(void* self, void*, const wchar_t* text, int flag)
{
    static_cast<SwiffFontObject*>(self)->setText(text, flag);
    return 0;
}
void __fastcall SwiffFontObject::vDraw(void* self, void*, const float* m, unsigned int color)
{
    static_cast<SwiffFontObject*>(self)->draw(m, color);
}
// sub_100A5160 - the extent up to character `arg` (SwiffPlayer asks with the caret for the caret's x
// and with the line length for word wrap), plus the window's scroll offset. No font: 0 with the
// outs untouched; no text: 0 with both zeroed (sub_100A4810's own first stores).
char __fastcall SwiffFontObject::vGetExtent(void* self, void*, int arg, float* outW, float* outH)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    if (!o->m_font)
        return 0;
    if (!o->measure(static_cast<unsigned int>(arg), outW, outH))
        return 0;
    *outW = o->m_scrollOffset + *outW;
    return 1;
}
char __fastcall SwiffFontObject::vForward32(void*, void*, int, float, float) { fontForwardNote(); return 0; }
char __fastcall SwiffFontObject::vForward40(void*, void*, int, int)          { fontForwardNote(); return 0; }
char __fastcall SwiffFontObject::vForward44(void*, void*, float, float, float, float) { fontForwardNote(); return 0; }
int  __fastcall SwiffFontObject::vForward48(void*, void*)                    { fontForwardNote(); return 0; }


// Caches fonts by name and size, and reloads them when the display width changes.
// A zero width uses GSWindowSizeX. [CHOSEN, not recovered] Cache storage uses an array instead of a map.

extern const unsigned int CID_FontSystem = 81044u;    // matches ModuleGeomMisc's getSingletonList
extern const unsigned int IID_IFontSystem = 80045u;

const void** fontSystemVtable();      // defined after the shims below
const void** fontSystemEventVtable(); // defined after the shims below

class FontSystem
{
public:
    FontSystem();

    DifFont* getFont(const char* name, int size);

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;

    // the +4 subobject's one slot, IEventListener::handleEvent(category, id, node,
    // data), thiscall with `retn 10h`: the front's __fastcall shim shape (ecx = the subobject).
    static void __fastcall handleRendererEvent(void* listener, void*, int category, unsigned int id,
                                               void* node, void* data);

private:
    void     init();                                  // sub_100A0A80 - clear, then register (11)
    void     clear();                                 // sub_100A0B30 - drop every cached font
    bool     reloadFonts(int width, int height);      // sub_100A0F10
    DifFont* loadFont(const char* key, int width);    // sub_100A0480 (+ sub_100A0350's insert)

    struct Entry { char key[128]; DifFont* font; };
    const void** m_vptr;           // +0 - IBase vtable (must be first)
    const void** m_eventVptr;      // +4 - the ECRenderer listener, 0x1020D16C (the handler's `add ecx, -4`)
    Entry*       m_entries;
    unsigned int m_count;
    unsigned int m_cap;
    int          m_refCount;
};

// the two vptrs, refcount 1, the empty cache, then sub_100A0A80.
FontSystem::FontSystem()
    : m_vptr(fontSystemVtable()), m_eventVptr(fontSystemEventVtable()),
      m_entries(0), m_count(0), m_cap(0), m_refCount(1)
{
    init();
}

// sub_100A0A80 (174 B). The EventManager is dereferenced unguarded, as shipped - it is one of the
// singletons initDll requires, and this class is a flags-2 singleton built after initDll. The
// handler goes in at priority 0, the same category and priority as SwiffRender's.
void FontSystem::init()
{
    clear();
    ::dice::hfe::eventManager->registerEventHandler(
        static_cast<EventCategory>(11), reinterpret_cast<IEventListener*>(&m_eventVptr), 0);   // +0x2C
}

// Clears cached fonts. [CHOSEN, not recovered] Uses counted releases so persistent wrappers retain their fonts.
void FontSystem::clear()
{
    for (unsigned int i = 0; i < m_count; ++i)
        if (m_entries[i].font)                                         // `test edi, edi; je`
            m_entries[i].font->release();
    m_count = 0;
}


void __fastcall FontSystem::handleRendererEvent(void* listener, void*, int category, unsigned int id,
                                                void* /*node*/, void* data)
{
    static_assert(offsetof(FontSystem, m_eventVptr) == 4,
                  "the listener subobject sits at +4 - the slot's `add ecx, -4` depends on it");
    if (category != 11)                                                // `cmp [ebp+8], 0Bh`
        return;
    if (id == 0 || id > 2)                                             // `test; jbe` / `cmp 2; ja`
        return;
    // The payload is dereferenced with no null test, as shipped.
    const unsigned int packed = *static_cast<const unsigned int*>(data);
    FontSystem* const self = reinterpret_cast<FontSystem*>(
        static_cast<unsigned char*>(listener) - 4);                    // `add ecx, -4`
    self->reloadFonts(static_cast<int>(packed >> 16),                  // `shr eax, 10h`, pushed last
                      static_cast<int>(packed & 0xFFFFu));             // `and edx, 0FFFFh`
}

// sub_100A0F10 (329 B, `retn 8`). The keys are snapshotted (the shipped std::vector<std::string>,
// map order), every font destroyed (see clear()'s ) and the cache emptied, then every key is loaded
// again through the loader with the new width. `height` is never read. The answer is true only
// when every reload succeeded; the one caller discards it.
bool FontSystem::reloadFonts(int width, int /*height*/)
{
    const unsigned int count = m_count;
    typedef char Key[128];
    Key* keys = count ? static_cast<Key*>(::renddx9abi::engineAlloc(count * sizeof(Key))) : 0;
    for (unsigned int i = 0; i < count; ++i)
    {
        if (keys)
            ::strncpy_s(keys[i], sizeof(keys[i]), m_entries[i].key, _TRUNCATE);   // sub_10011220
        if (m_entries[i].font)
            m_entries[i].font->release();                              // sub_100A5290 + free
    }
    m_count = 0;                                                       // sub_1009FF20 + head reset

    bool ok = true;                                                    // [ebp-1] = 1
    for (unsigned int i = 0; keys && i < count; ++i)
        if (!loadFont(keys[i], width))                                 // sub_100A0480(key, width, 0)
            ok = false;

    if (keys)
        ::renddx9abi::engineFree(keys);
    return ok;
}

DifFont* FontSystem::getFont(const char* name, int size)
{
    char key[128];
    ::_snprintf_s(key, sizeof(key), _TRUNCATE, "%s_%d", name ? name : "", size);
    for (unsigned int i = 0; i < m_count; ++i)
        if (::strcmp(m_entries[i].key, key) == 0)
            return m_entries[i].font;

    // sub_100A09C0's miss arm: the loader with width 0 (0x100A0A2A..0x100A0A34).
    return loadFont(key, 0);
}

// Finds and loads a font for the requested width; zero uses the current display resolution.
DifFont* FontSystem::loadFont(const char* key, int width)
{
    // Selects the language and resolution directory. Zero width reads the current display setting.
    // Uses reference width 800 below 1024; the 800 directory is selected only for an exact width of 800.
    int res = width;
    if (res == 0)
    {
        res = ::dice::hfe::dx9rend::readIntSetting("GSWindowSizeX", 800);
        (void)::dice::hfe::dx9rend::readIntSetting("GSWindowSizeY", 600);
    }
    float reference = 800.0f;
    const char* resFolder = "";
    if (res == 800)
        resFolder = "800";
    else if (res >= 1024)
        reference = 1024.0f;
    const float fontScale = static_cast<float>(res) / reference;

    vc71::string langStr;
    ::dice::hfe::dx9rend::readStringSetting("GSLanguage", langStr);
    const char* lang = (!langStr.empty()) ? langStr.c_str() : "english";

    // Searches the language-specific font directory before the common resolution directory.
    // [CHOSEN, not recovered] Normalizes repeated path separators.
    char cand[2][384];
    unsigned int nc = 0;
    if (resFolder[0])
    {
        ::_snprintf_s(cand[nc++], sizeof(cand[0]), _TRUNCATE, "Fonts/%s/%s/%s", lang, resFolder, key);
        ::_snprintf_s(cand[nc++], sizeof(cand[0]), _TRUNCATE, "Fonts/%s/%s", resFolder, key);
    }
    else
    {
        ::_snprintf_s(cand[nc++], sizeof(cand[0]), _TRUNCATE, "Fonts/%s/%s", lang, key);
        ::_snprintf_s(cand[nc++], sizeof(cand[0]), _TRUNCATE, "Fonts/%s", key);
    }

    DifFont* font = 0;
    for (unsigned int i = 0; i < nc; ++i)
    {
        DifFont* f = static_cast<DifFont*>(::renddx9abi::engineAlloc(sizeof(DifFont)));
        if (!f)
            break;
        new (f) DifFont();
        if (f->load(cand[i]))
        {
            font = f;
            break;
        }
        f->release();       // missing .dif - free this attempt and try the next candidate
    }
    if (!font)
    {
        // FontSystem.cpp:356 - the shipped "doesn't exist in paths A or B" diagnostic, naming the
        // same two directories the lookup tried.
        ::renddx9abi::logf("FontSystem.cpp:356 Font \"%s\" doesn't exist in paths Fonts/%s/%s/ or "
                           "Fonts/%s/", key, lang, resFolder, resFolder);
        return 0;
    }
    font->setScale(fontScale, fontScale);      // sub_100A3450, both components from the x ratio

    if (m_count >= m_cap)
    {
        const unsigned int cap = m_cap ? m_cap * 2 : 8;
        Entry* grown = static_cast<Entry*>(::renddx9abi::engineAlloc(cap * sizeof(Entry)));
        if (!grown)
            return font;                     // usable, just not cached
        for (unsigned int i = 0; i < m_count; ++i) grown[i] = m_entries[i];
        if (m_entries) ::renddx9abi::engineFree(m_entries);
        m_entries = grown;
        m_cap = cap;
    }
    ::strncpy_s(m_entries[m_count].key, sizeof(m_entries[m_count].key), key, _TRUNCATE);
    m_entries[m_count].font = font;
    ++m_count;
    return font;
}

int FontSystem::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        // Unregister the event handler before clearing font cache and map storage.
        ::dice::hfe::eventManager->unregisterEventHandler(
            static_cast<EventCategory>(11), reinterpret_cast<IEventListener*>(&m_eventVptr));
        clear();
        if (m_entries) ::renddx9abi::engineFree(m_entries);
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

void* FontSystem::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IFontSystem)
        return const_cast<FontSystem*>(this);
    return 0;
}

namespace {
void  __fastcall fs_addRef(void* s, void*)  { static_cast<FontSystem*>(s)->addRef(); }
int   __fastcall fs_getRef(void* s, void*)  { return static_cast<FontSystem*>(s)->getRef(); }
int   __fastcall fs_release(void* s, void*) { return static_cast<FontSystem*>(s)->release(); }
void* __fastcall fs_queryInterface(void* s, void*, unsigned int iid)
{ return static_cast<FontSystem*>(s)->queryInterface(iid); }

const void* kFontSystemVtable[4] =
{
    reinterpret_cast<void*>(&fs_addRef),
    reinterpret_cast<void*>(&fs_getRef),
    reinterpret_cast<void*>(&fs_release),
    reinterpret_cast<void*>(&fs_queryInterface),
};

// The event-listener subobject has one virtual slot, separate from the primary table.
const void* kFontSystemEventVtable[1] =
{
    reinterpret_cast<void*>(&FontSystem::handleRendererEvent),
};
} // anonymous namespace

const void** fontSystemEventVtable() { return kFontSystemEventVtable; }

const void** fontSystemVtable() { return kFontSystemVtable; }

// RefFactory for dice.hfe.geom.FontSystem.
IBase* __fastcall createFontSystem(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(FontSystem));
    if (!storage)
        return 0;
    FontSystem* self = new (storage) FontSystem();
    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}


// SwiffRender::createFont's real body: resolve the FontSystem singleton, get the DifFont, and wrap
// it in a SwiffFontObject. This is the seam sr_createFont calls once the classes are registered.

void* createSwiffFont(const char* name, int size, const wchar_t* initialText)
{
    if (!::dice::hfe::classManager)
        return 0;
    void* fsBase = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("FontSystem"));
    if (!fsBase)
        return 0;
    FontSystem* fs = static_cast<FontSystem*>(fsBase);
    DifFont* font = fs->getFont(name, size);
    if (!font)
        return 0;
    SwiffFontObject* wrapper = static_cast<SwiffFontObject*>(
        ::renddx9abi::engineAlloc(sizeof(SwiffFontObject)));
    if (!wrapper)
        return 0;
    new (wrapper) SwiffFontObject(font, initialText);
    return wrapper;
}

// The two setter seams SwiffRender's font slots call (sr_fontSetSize / sr_fontSetField104). The
// wrapper is an opaque void* on that side; here it is the real SwiffFontObject, so set the named
// members. Null-guarded because the slots forward whatever handle SwiffPlayer holds.
void swiffFontConfigure(void* font, float boxWidth, int align)
{
    if (font)
        static_cast<SwiffFontObject*>(font)->configure(boxWidth, align);
}

void swiffFontSetScrollIndex(void* font, int value)
{
    if (font)
        static_cast<SwiffFontObject*>(font)->setScrollIndex(value);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
