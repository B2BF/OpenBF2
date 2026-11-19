// Font metrics, atlas storage and text rendering support. The FontRenderer batcher is the retail
// one; the Swiff font wrapper uses native storage for recovered run and selection operations.
// [CHOSEN, not recovered] Native STL and wrapper layout.
// [RETIRE-WHEN: RW-80] Complete the emitter's precision boundaries and replace bounded text
// storage and counted font ownership when their native contracts are validated with menu/HUD text.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include "../../Core/EventManager.h" // FontSystem renderer-event registration.
#include "../../Core/SettingsRepostitory.h" // GSLanguage for the lexicon character table.
#include "../Debug.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/ShaderParameter.h"
#include "../../Menu/MenuViewport.h"
#include "../../RendDX9/Texture.h"             // EngineTexture

#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../../../SwiffPlayerAbi/HiDpiGlyphs.h"   // [CHOSEN, not recovered] HiDPI menu text
#include "HiDpiFontTexture.h"
#include <algorithm>
#include <ctype.h>

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <new>
#include <list>
#include <map>
#include <sstream>
#include <vector>

namespace dice {
namespace hfe {

// The RendDX9 render seams this stack rides on - the same set SwiffRender.cpp declares.
namespace dx9rend {
extern void* g_shaderManager;
void* shaderManagerGetEffect(void*, const char*);
class Effect;
void* effectGetShaderParameter(Effect*, const std::string&);
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
    float          intField; // = f2 - the ink width (the layout adds it, sub_100A4BF0)
    float          rightBearing; // = f3 - the right side bearing (the layout adds it too)
    unsigned short x;        // atlas rect, texels
    unsigned short y;
    unsigned short x2;
    unsigned short y2;
    float          bearingY; // = the int - the vertical Y-offset (top position, baseline-aligning)
    float          height;   // y2 - y, precomputed
};
static_assert(sizeof(DifGlyph) == 32, "DifFont glyph records contain only scalar fields");

// [CHOSEN, not recovered] The 28 bytes DifFont::addGlyph copies behind the glyph code: DifGlyph's
// fields from bearingX on. The source type is unobservable because addGlyph has no caller.
struct DifGlyphMetrics
{
    float          bearingX;
    float          intField;
    float          rightBearing;
    unsigned short x, y, x2, y2;
    float          bearingY;
    float          height;
};
static_assert(sizeof(DifGlyphMetrics) == 28, "addGlyph copies seven dwords");

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

// [CHOSEN, not recovered] Typed native view of the first ten attested Font slots.
// Tables 0x1020D564 and 0x1021E0D0 share this order; there is no destructor slot.
class NativeFontView
{
public:
    virtual void getRun(unsigned int*, unsigned int*, float*) = 0;
    virtual void getFirst(unsigned int*) = 0;
    virtual void invalidate() = 0;
    virtual bool wouldOverflow() = 0;
    virtual void setText(const wchar_t*, int) = 0;
    virtual void draw(const float*, unsigned int) = 0;
    virtual const float* getColor() = 0;
    virtual char measure(int, float*, float*) = 0;
    virtual char hit(int*, float, float) = 0;
    virtual const std::vector<GlyphCell>* getCells() = 0;
};

// PE 0x100A5490
static double roundFontPosition(float value)
{
    // The addition is spilled as double before the CRT floor call.
    return ::floor(static_cast<double>(value) + 0.5);
}


// Parsed font metrics, atlas and projection. Device-reset callbacks do not refresh the projection;
// FontSystem reloads fonts when display resolution changes.

class DifFont : public dx9rend::DeviceLostResetListener
{
public:
    // PE 0x100A53B0. Native reference bookkeeping and pre-load zero initialization
    // remain [CHOSEN, not recovered] safety behavior on failed loading paths.
    DifFont(const std::string& directory, const std::string& key)
              : m_refCount(1), m_key(), m_directory(),
                m_atlas(0), m_atlasW(0), m_atlasH(0), m_version(0), m_size(0.0f),
                m_scaleX(1.0f), m_scaleY(1.0f), m_fontRenderer(0), m_deviceDetached(false)
    {
        m_projVp[0] = m_projVp[1] = m_projVp[2] = m_projVp[3] = 0.0f;
        for (int i = 0; i < 16; ++i) m_projection[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        m_directory = directory;
        m_key = key;
    }

    // The three listener slots, 0x1020D5A4 - each shipped body is `ret` (see the class note).
    void onDeviceLost() override;
    void onDeviceReset() override;
    void onDeviceDestroy() override;    // 0x10037D40, the image's shared nullsub

    // sub_100A4420 - open "<path>.dif", parse it, load "<path>.dds", build the projection, and
    // resolve the FontRenderer singleton. Returns false when the .dif is missing (the shipped
    // FontSystem.cpp:356 "Font X doesn't exist" diagnostic).
    bool load(const ui::MenuViewport& viewport);

    // Glyph lookup; native callers fold nonbreaking space as the original layout/emitter do.
    const DifGlyph* glyph(unsigned short ch) const;
    // Kerning between two characters, zero when absent.
    float kerning(unsigned short a, unsigned short b) const;
    // sub_100A4BF0 (vtable +0x0C) - lay `len` characters of `text` out into `out[0..len)`.
    void layout(const wchar_t* text, unsigned int len, GlyphCell* out) const;
    // sub_100A48C0 - the glyph under x (text-field caret placement); sub_100A4A00 - the glyph range
    // a drag from (x1, y1) to (x2, y2) covers. RW-80.
    bool hitTest(const wchar_t* text, unsigned int len, int* out, float x, float y,
                 unsigned int first, unsigned int end) const;
    bool selectRange(const wchar_t* text, unsigned int len, int* selStart, int* selEnd,
                     float x1, float y1, float x2, float y2,
                     unsigned int first, unsigned int end) const;

    // sub_100A3450 - the loader's last act on a fresh font: store the resolution scale in +136/+140.
    // measure() multiplies its answer by it and draw() puts it in the scaling matrix, so a font that
    // never receives it reports and renders at the atlas's own authored size.
    // It also selects the FontRenderer's technique for every font: RegularScaled above unit scale.
    void         setScale(float x, float y);

    void*        atlas()      const { return m_atlas; }
    unsigned int atlasW()     const { return m_atlasW; }
    unsigned int atlasH()     const { return m_atlasH; }
    float        size()       const { return m_size; }      // the .dif's authored size, for tracing
    const float* projVp()     const { return m_projVp; }   // [TEMP] RW-89 tracer
    float        scaleX()     const { return m_scaleX; }
    float        scaleY()     const { return m_scaleY; }
    const float* projection() const { return m_projection; }
    void*        fontRenderer() const { return m_fontRenderer; }
    void drawSelection(NativeFontView* font, const float* matrix, float width);

    // [CHOSEN, not recovered] HiDPI menu text (SwiffPlayerAbi/HiDpiGlyphs.h): after setScale, a
    // font scaled up for the display whose name a loaded SWF exports gets its atlas rebuilt from
    // that SWF's outlines at an integer multiple of the authored resolution. Metrics, layout,
    // quads and normalised texel rectangles are unchanged.
    void hidpiUpgrade();

    void addRef() { ++m_refCount; }
    int  release();

    // [CHOSEN, not recovered] The device half of the destructor (PE 0x100A5290: the listener
    // unregistration and the atlas reference), run once. FontSystem's release calls it for a font
    // a counted wrapper still holds, because the shipped FontSystem::clear (0x100A0B30) destroys
    // every cached font there; release() runs it for the rest.
    void detachDevice();

private:
    void readHeader(std::istream& input);
    void readGlyphs(std::istream& input);
    void readKerning(std::istream& input);
    void addGlyph(unsigned short code, const DifGlyphMetrics* metrics);
    void addKerning(unsigned short first, unsigned short second, float amount);

    int          m_refCount;
    std::map<unsigned short, DifGlyph*> m_glyphs;
    std::map<unsigned int, float> m_kerns; // packed first character in the low word
    std::string m_displayName;
    std::string m_key;
    std::string m_directory;
    void*        m_atlas;       // EngineTexture* from the manager
    unsigned int m_atlasW;
    unsigned int m_atlasH;
    unsigned int m_version;
    float        m_size;
    float        m_scaleX;
    float        m_scaleY;
    float        m_projection[16];
    float        m_projVp[4];        // [TEMP] RW-89: the viewport the projection was built from
    void*        m_fontRenderer;
    bool         m_deviceDetached;  // [CHOSEN] detachDevice has run
    // [CHOSEN, not recovered] HiDPI menu text state. A replaced atlas
    // stays referenced until detachDevice: the FontRenderer may still hold it for a pending batch.
    void*        m_hidpiRetired = 0;
};

// PE 0x100A34D0
void DifFont::readHeader(std::istream& input)
{
    input >> m_version;
    // [CHOSEN, not recovered] Preserve the prior parser's defined storage if
    // truncated input would make the retail first-character read uninitialized.
    char name[33] = {};
    input >> name[0];
    input.putback(name[0]);
    input.getline(name, 33);
    m_displayName += name;
    input >> m_atlasW;
    if (m_version > 1u)
        input >> m_atlasH;
    else
        m_atlasH = m_atlasW;
    input >> m_size;
}

// PE 0x100A41B0
void DifFont::readGlyphs(std::istream& input)
{
    // [CHOSEN, not recovered] Retain defined missing-token values rather than
    // restoring the retail parser's first-row uninitialized reads.
    unsigned int count = 0;
    input >> count;
    unsigned short code = 0;
    float left = 0.0f, width = 0.0f, right = 0.0f;
    int top = 0;
    unsigned int x = 0, y = 0, x2 = 0, y2 = 0;
    for (; count && !input.eof(); --count)
    {
        input >> code >> left >> width >> right >> top;
        input >> x >> y >> x2 >> y2;
        DifGlyph* glyph = static_cast<DifGlyph*>(::renddx9abi::engineAlloc(sizeof(DifGlyph)));
        if (glyph)
        {
            // Native padding keeps the prior defined initialization.
            new (glyph) DifGlyph();
            glyph->code = code;
            glyph->bearingX = left;
            glyph->intField = width;
            glyph->rightBearing = right;
            glyph->x = static_cast<unsigned short>(x);
            glyph->y = static_cast<unsigned short>(y);
            glyph->x2 = static_cast<unsigned short>(x2);
            glyph->y2 = static_cast<unsigned short>(y2);
            glyph->bearingY = static_cast<float>(top);
            glyph->height = static_cast<float>(static_cast<int>(glyph->y2) - static_cast<int>(glyph->y));
        }
        // shipped defect: a duplicate retains the first glyph and leaks this allocation.
        m_glyphs.insert(std::make_pair(code, glyph));
    }
}

// PE 0x100A4120
void DifFont::readKerning(std::istream& input)
{
    // [CHOSEN, not recovered] Keep the existing missing-token zero initialization.
    unsigned int count = 0;
    input >> count;
    unsigned short a = 0, b = 0;
    float amount = 0.0f;
    for (; count && !input.eof(); --count)
    {
        input >> a >> b >> amount;
        const unsigned int key = static_cast<unsigned int>(a) | (static_cast<unsigned int>(b) << 16);
        m_kerns.insert(std::make_pair(key, amount));
    }
}

// Allocates a glyph record for code with the given metrics and inserts it; an existing entry for
// code is kept and the new record leaks, and a failed allocation inserts null.
// PE 0x100A3D40. unreachable in 1.50: no code or data references. [INFERRED] Name.
void DifFont::addGlyph(unsigned short code, const DifGlyphMetrics* metrics)
{
    DifGlyph* glyph = static_cast<DifGlyph*>(::renddx9abi::engineAlloc(sizeof(DifGlyph)));
    if (glyph)
    {
        glyph->code = code;
        glyph->bearingX = metrics->bearingX;
        glyph->intField = metrics->intField;
        glyph->rightBearing = metrics->rightBearing;
        glyph->x = metrics->x;
        glyph->y = metrics->y;
        glyph->x2 = metrics->x2;
        glyph->y2 = metrics->y2;
        glyph->bearingY = metrics->bearingY;
        glyph->height = metrics->height;
    }
    m_glyphs.insert(std::make_pair(code, glyph));
}

// Inserts the kerning amount for a character pair (first character in the key's low word); an
// existing entry is kept.
// PE 0x100A3DA0. unreachable in 1.50: no code or data references. [INFERRED] Name.
void DifFont::addKerning(unsigned short first, unsigned short second, float amount)
{
    const unsigned int key = static_cast<unsigned int>(first) | (static_cast<unsigned int>(second) << 16);
    m_kerns.insert(std::make_pair(key, amount));
}

// Finds the glyph of a character, folding nonbreaking space to space; null when absent.
// PE 0x100A3850. unreachable in 1.50 as an out-of-line copy: no code or data references; the
// layout and FontRenderer::reserve inline it.
const DifGlyph* DifFont::glyph(unsigned short ch) const
{
    if (ch == 160)          // nbsp -> space, exactly as sub_100A38D0 folds it
        ch = 32;
    const auto found = m_glyphs.find(ch);
    return found == m_glyphs.end() ? 0 : found->second;
}

// Kerning between two characters (first character in the key's low word), zero when absent.
// PE 0x100A3890. unreachable in 1.50 as an out-of-line copy: no code or data references; the
// layout inlines it.
float DifFont::kerning(unsigned short a, unsigned short b) const
{
    const unsigned int key = static_cast<unsigned int>(a) | (static_cast<unsigned int>(b) << 16);
    const auto found = m_kerns.find(key);
    return found == m_kerns.end() ? 0.0f : found->second;
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

// sub_100A48C0 (thiscall, ret 18h): no text answers false. x is divided by the X scale (+0x88; y is
// not read); the text is laid out (vtable +0x0C into a vector of 16-byte cells). No cells: *out =
// first, true. With s = min(first, n - 1) and e = min(end, n - 1): x < cells[s].cellLeft -> *out =
// first, true; x >= cells[e].right -> *out = end, true; otherwise the first i from s (while i < n
// and i <= e) with cellLeft <= x < right -> *out = i, true. None: false, *out left at the index the
// walk stopped on. The comparisons are the x87 tests' ordered forms (fcomp/fnstsw: test ah,5 / jp,
// test ah,1 / jne); a NaN x falls through every one of them to the final false.
bool DifFont::hitTest(const wchar_t* text, unsigned int len, int* out, float x, float /*y*/,
                      unsigned int first, unsigned int end) const
{
    if (!text)
        return false;
    const float px = x / m_scaleX;
    GlyphCell cells[512];
    if (len > 512) len = 512;          // [CHOSEN, not recovered] SwiffFontObject's own text cap
    layout(text, len, cells);
    const unsigned int n = len;
    if (n == 0)
    {
        *out = static_cast<int>(first);
        return true;
    }
    const unsigned int last = n - 1;
    const unsigned int s = (last < first) ? last : first;
    const unsigned int e = (last < end) ? last : end;
    if (px < cells[s].cellLeft)
    {
        *out = static_cast<int>(first);
        return true;
    }
    if (!(px < cells[e].right))
    {
        *out = static_cast<int>(end);
        return true;
    }
    unsigned int i = s;
    *out = static_cast<int>(i);
    while (i < n)
    {
        if (i > e)
            break;
        if (!(px < cells[i].cellLeft) && px < cells[i].right)
            return true;               // *out already holds i
        ++i;
        *out = static_cast<int>(i);
    }
    return false;
}

// sub_100A4A00 (thiscall, ret 24h): the four coordinates scaled and rounded, floor(v / scale + 0.5)
// (+0x88 for x, +0x8C for y; the CRT's double floor). y2 < 0 or y1 above the authored size (+0x84,
// the .dif header's float, 0x100A3544) refuses. No cells refuses. With s = min(first, n - 1) and
// e = min(end, n - 1): x2 < cells[s].cellLeft or x1 >= cells[e].right refuses. Then from i = s,
// while i < n and i <= e and not both found: the first cell holding x1 (cellLeft <= x1 < right)
// gives *selStart, the first holding x2 gives *selEnd. A start not found is `first`, an end not
// found is `end`. True.
bool DifFont::selectRange(const wchar_t* text, unsigned int len, int* selStart, int* selEnd,
                          float x1, float y1, float x2, float y2,
                          unsigned int first, unsigned int end) const
{
    const float sx1 = static_cast<float>(::floor(static_cast<double>(x1 / m_scaleX + 0.5f)));
    const float sy1 = static_cast<float>(::floor(static_cast<double>(y1 / m_scaleY + 0.5f)));
    const float sx2 = static_cast<float>(::floor(static_cast<double>(x2 / m_scaleX + 0.5f)));
    const double sy2 = ::floor(static_cast<double>(y2 / m_scaleY + 0.5f));
    if (sy2 < 0.0)
        return false;
    if (sy1 > m_size)
        return false;
    GlyphCell cells[512];
    if (len > 512) len = 512;          // [CHOSEN, not recovered] SwiffFontObject's own text cap
    layout(text, len, cells);
    const unsigned int n = len;
    if (n == 0)
        return false;
    const unsigned int last = n - 1;
    const unsigned int s = (last < first) ? last : first;
    const unsigned int e = (last < end) ? last : end;
    if (sx2 < cells[s].cellLeft)
        return false;
    if (!(sx1 < cells[e].right))
        return false;
    bool haveStart = false, haveEnd = false;
    for (unsigned int i = s; i < n && i <= e && !(haveStart && haveEnd); ++i)
    {
        const float l = cells[i].cellLeft;
        const float r = cells[i].right;
        if (!haveStart && !(sx1 < l) && sx1 < r)
        {
            *selStart = static_cast<int>(i);
            haveStart = true;
        }
        if (!haveEnd && !(sx2 < l) && sx2 < r)
        {
            *selEnd = static_cast<int>(i);
            haveEnd = true;
        }
    }
    if (!haveStart)
        *selStart = static_cast<int>(first);
    if (!haveEnd)
        *selEnd = static_cast<int>(end);
    return true;
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
        // The destructor's order (PE 0x100A5290): the device half, then the glyph storage.
        detachDevice();
        for (auto it = m_glyphs.begin(); it != m_glyphs.end(); ++it)
            if (it->second)
                ::renddx9abi::engineFree(it->second);
        this->~DifFont();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}


// PE 0x100A5290's first half: unregister (unregistered failed-load candidates are harmless), then
// drop the font's own atlas reference (0x100A52AB..0x100A52CD: `if (!--atlas->refCount)
// { sub_10014E20; free }`); the TextureManager's cache keeps its reference and lets it go in the
// destroy broadcast.
void DifFont::detachDevice()
{
    if (m_deviceDetached)
        return;
    m_deviceDetached = true;
    dx9rend::unregisterDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this));
    if (m_atlas)
    {
        ::dice::hfe::dx9rend::EngineTexture* atlas =
            static_cast< ::dice::hfe::dx9rend::EngineTexture*>(m_atlas);
        ::dice::hfe::dx9rend::textureReleaseRef(atlas);
        m_atlas = 0;
    }
    hidpiRemovePending(this);
    if (m_hidpiRetired)     // [CHOSEN, not recovered] HiDPI menu text: the replaced shipped atlas
    {
        ::dice::hfe::dx9rend::EngineTexture* retired =
            static_cast< ::dice::hfe::dx9rend::EngineTexture*>(m_hidpiRetired);
        ::dice::hfe::dx9rend::textureReleaseRef(retired);
        m_hidpiRetired = 0;
    }
}


// Load the glyph metrics and atlas. Project menu-local pixels into the Flash viewport.

bool DifFont::load(const ui::MenuViewport& viewport)
{
    const std::string path = m_directory + m_key;
    const std::string difPath = path + ".dif";
    ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile(difPath.c_str());
    if (!file.ok())
        return false;      // this candidate path has no .dif - getFont tries the next and logs

    // [CHOSEN, not recovered] A counted native string stream replaces istrstream's
    // borrowed buffer while retaining formatted extraction and EOF/fail state.
    std::istringstream input(std::string(reinterpret_cast<const char*>(file.data), file.size));
    while (!input.eof())
    {
        std::string word;
        input >> word;
        if (word.empty())
            break;
        if (word == "header")
            readHeader(input);
        else if (word == "kerning")
            readKerning(input);
        else if (word == "glyphs")
            readGlyphs(input);
    }
    ::renddx9abi::freeEngineFile(file);

    // The .dds atlas
    const std::string ddsPath = path + ".dds";
    // 0x100A45F0..0x100A4694: the atlas load is bracketed by the ResourceManager defer byte
    // (RM+0x04): save it (sub_1000D950), clear it (sub_1000D960 with edi = 0), getTexture
    // (sub_10019260), restore it. So the atlas is read at once even while a level load defers
    // texture reads - the loading screen creates its fonts in that window.
    const bool deferSaved = ::dice::hfe::dx9rend::resourceManagerIsReady();
    ::dice::hfe::dx9rend::resourceManagerSetReady(0);
    m_atlas = ::dice::hfe::dx9rend::textureManagerGetTexture(std::string(ddsPath));
    ::dice::hfe::dx9rend::resourceManagerSetReady(deferSaved ? 1 : 0);

    // Positions are local to the menu. The device viewport supplies the screen offset.
    if (viewport.width && viewport.height)
    {
        m_projection[0]  =  2.0f / viewport.width;
        m_projection[5]  = -2.0f / viewport.height;
        m_projection[12] = -1.0f;
        m_projection[13] = 1.0f;
        m_projVp[0] = float(viewport.x); m_projVp[1] = float(viewport.y);
        m_projVp[2] = float(viewport.width); m_projVp[3] = float(viewport.height);
    }

    // Register only after loading succeeds; requires the renderer's listener registry.
    dx9rend::registerDeviceLostResetListener(static_cast<dx9rend::DeviceLostResetListener*>(this), 2);

    // The FontRenderer singleton (published by ModuleGeomMisc once this stack registers)
    if (::dice::hfe::classManager)
        m_fontRenderer = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("FontRenderer"));

    ::renddx9abi::logf("DifFont::load(\"%s\"): %u glyphs, %u kerning pairs, atlas %ux%u -> 0x%p"
                       "  projFrom vp=(%d,%d,%d,%d)",
                       path.c_str(), static_cast<unsigned int>(m_glyphs.size()),
                       static_cast<unsigned int>(m_kerns.size()), m_atlasW, m_atlasH,
                       m_atlas,
                       (int) m_projVp[0], (int) m_projVp[1], (int) m_projVp[2], (int) m_projVp[3]);
    return true;
}


// [CHOSEN, not recovered] HiDPI menu text. See SwiffPlayerAbi/HiDpiGlyphs.h.
// A .dif font's shipped atlas is the reference: each glyph's coverage centroid (horizontal) and
// one baseline for the font (vertical) place a SWF outline inside the glyph's own texel
// rectangle, and the EM size is the one whose outlines best reproduce the bitmaps. Candidates
// are the fonts exported under the .dif's name, then every outline font the loaded movies define.
// A font no candidate reproduces (another typeface,
// effects such as outlines, digit-segment fonts) keeps its shipped atlas. Characters without an
// outline keep their bitmap, resampled. Fit each loaded atlas against the current movie outlines.
namespace {

struct HiDpiGlyph
{
    unsigned short code;
    float top;                  // .dif vertical offset: the cell's top below the line top
    int   x, y, w, h;           // texel rectangle
    float cx, cy;               // bitmap coverage centroid in the rectangle (texel centres +0.5)
    float inkH;                 // rows with coverage >= 0.5
    std::vector<float> bitmap;  // w*h coverage
};

// One candidate's glyph references at EM = 1 px: centroid from the pen origin and ink height.
struct HiDpiRef
{
    std::vector<unsigned char> has;
    std::vector<float> cx, cy, inkH;
};

const float kHiDpiRefEm = 64.0f;
const float kHiDpiMaxError = 0.20f;     // matching typefaces score well below, others well above

bool hidpiReadDif(const std::string& base, unsigned int maxDimension,
                  unsigned int& W, unsigned int& H, float& size,
                  std::vector<HiDpiGlyph>& glyphs)
{
    ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile((base + ".dif").c_str());
    if (!file.ok())
        return false;
    std::istringstream in(std::string(reinterpret_cast<const char*>(file.data), file.size));
    ::renddx9abi::freeEngineFile(file);
    unsigned int version = 0;
    W = H = 0;
    size = 0.0f;
    bool haveGlyphs = false;
    while (in)
    {
        std::string word;
        in >> word;
        if (word.empty())
            break;
        if (word == "header")
        {
            if (W != 0)
                return false;
            in >> version;
            char name[33] = {};
            in >> name[0];
            in.putback(name[0]);
            in.getline(name, 33);
            in >> W;
            if (version > 1u) in >> H; else H = W;
            in >> size;
            if (!in || W == 0 || H == 0 || W > maxDimension || H > maxDimension ||
                !(size > 0.0f && size <= static_cast<float>(maxDimension)))
                return false;
        }
        else if (word == "glyphs")
        {
            if (haveGlyphs || W == 0 || H == 0)
                return false;
            unsigned int n = 0;
            in >> n;
            if (!in || n > 65536)
                return false;
            // Allow shared cells, but bound the coverage buffers for overlapping mod glyphs.
            size_t remainingPixels = static_cast<size_t>(W) * H * 2;
            for (; n && in; --n)
            {
                unsigned int code = 0, x = 0, y = 0, x2 = 0, y2 = 0;
                float l = 0, wd = 0, r = 0;
                int top = 0;
                in >> code >> l >> wd >> r >> top >> x >> y >> x2 >> y2;
                if (!in || code > 65535 || x2 > W || y2 > H)
                    return false;
                if (x2 <= x || y2 <= y)
                    continue;
                const size_t cells = static_cast<size_t>(x2 - x) * (y2 - y);
                if (cells > remainingPixels)
                    return false;
                remainingPixels -= cells;
                HiDpiGlyph g;
                g.code = (unsigned short) code; g.top = (float) top;
                g.x = (int) x; g.y = (int) y; g.w = (int) (x2 - x); g.h = (int) (y2 - y);
                g.cx = g.cy = g.inkH = 0.0f;
                glyphs.push_back(g);
            }
            haveGlyphs = true;
        }
        else if (word == "kerning")
        {
            unsigned int n = 0;
            in >> n;
            for (; n && in; --n)
            {
                unsigned int a = 0, b = 0;
                float k = 0;
                in >> a >> b >> k;
            }
        }
    }
    return haveGlyphs && W && H;
}

// The atlas's coverage. Every .dif atlas is uncompressed A4R4G4B4; white ones only (a coloured
// one carries an outline or shadow effect the outlines cannot reproduce).
bool hidpiReadAtlas(const std::string& base, unsigned int W, unsigned int H, std::vector<float>& alpha)
{
    ::renddx9abi::EngineFileData dds = ::renddx9abi::readEngineFile((base + ".dds").c_str());
    if (!dds.ok())
        return false;
    const size_t pixelCount = static_cast<size_t>(W) * H;
    bool usable = dds.size >= 128 + pixelCount * 2 && ::memcmp(dds.data, "DDS ", 4) == 0;
    unsigned int hdr[31] = {};
    if (usable)
    {
        ::memcpy(hdr, dds.data + 4, sizeof(hdr));
        usable = hdr[0] == 124 && hdr[18] == 32 && hdr[2] == H && hdr[3] == W &&
                 (!(hdr[1] & 8) || hdr[4] == W * 2) && hdr[19] == 0x41 && hdr[21] == 16 &&
                 hdr[22] == 0x0F00 && hdr[23] == 0x00F0 && hdr[24] == 0x000F && hdr[25] == 0xF000;
    }
    if (usable)
    {
        alpha.resize(pixelCount);
        const unsigned char* px = dds.data + 128;
        for (size_t i = 0; i < pixelCount && usable; ++i)
        {
            const unsigned int p = (unsigned int) px[2 * i] | ((unsigned int) px[2 * i + 1] << 8);
            const unsigned int a = p >> 12;
            if (a && (p & 0x0FFF) != 0x0FFF)
                usable = false;
            alpha[i] = (float) a / 15.0f;
        }
    }
    ::renddx9abi::freeEngineFile(dds);
    return usable;
}

void hidpiMeasure(std::vector<HiDpiGlyph>& glyphs, const std::vector<float>& alpha, unsigned int W)
{
    for (HiDpiGlyph& q : glyphs)
    {
        q.bitmap.resize((size_t) q.w * q.h);
        double m = 0, sx = 0, sy = 0;
        int rows = 0;
        for (int y = 0; y < q.h; ++y)
        {
            float rowMax = 0.0f;
            for (int x = 0; x < q.w; ++x)
            {
                const float a = alpha[(size_t) (q.y + y) * W + q.x + x];
                q.bitmap[(size_t) y * q.w + x] = a;
                m += a; sx += a * (x + 0.5); sy += a * (y + 0.5);
                if (a > rowMax) rowMax = a;
            }
            if (rowMax >= 0.5f) ++rows;
        }
        q.inkH = (float) rows;
        if (m > 0.0) { q.cx = (float) (sx / m); q.cy = (float) (sy / m); }
        else q.inkH = -1.0f;    // no ink
    }
}

void hidpiReference(const openbf2::HiDpiGlyphs& outlines, const char* list, int cand, const std::vector<HiDpiGlyph>& glyphs, HiDpiRef& ref,
                    std::vector<float>& scratch)
{
    const int w = (int) (kHiDpiRefEm * 2.0f) + 8, h = (int) (kHiDpiRefEm * 2.0f) + 8;
    const float ox = kHiDpiRefEm * 0.5f + 4.0f, oy = kHiDpiRefEm * 1.4f + 4.0f;
    ref.has.assign(glyphs.size(), 0);
    ref.cx.assign(glyphs.size(), 0.0f);
    ref.cy.assign(glyphs.size(), 0.0f);
    ref.inkH.assign(glyphs.size(), 0.0f);
    scratch.resize((size_t) w * h);
    for (size_t i = 0; i < glyphs.size(); ++i)
    {
        if (!outlines.raster(list, cand, glyphs[i].code, kHiDpiRefEm, ox, oy, &scratch[0], w, h, 1))
            continue;
        double m = 0, sx = 0, sy = 0;
        int rows = 0;
        for (int y = 0; y < h; ++y)
        {
            float rowMax = 0.0f;
            for (int x = 0; x < w; ++x)
            {
                const float a = scratch[(size_t) y * w + x];
                m += a; sx += a * (x + 0.5); sy += a * (y + 0.5);
                if (a > rowMax) rowMax = a;
            }
            if (rowMax >= 0.5f) ++rows;
        }
        if (m <= 0.0)
            continue;
        ref.has[i] = 1;
        ref.cx[i] = (float) ((sx / m - ox) / kHiDpiRefEm);
        ref.cy[i] = (float) ((sy / m - oy) / kHiDpiRefEm);
        ref.inkH[i] = (float) rows / kHiDpiRefEm;
    }
}

float hidpiMedian(std::vector<float>& v)
{
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

// Relative squared error of a candidate at EM size em over glyphs[0..count); fills the baseline.
float hidpiEvaluate(const openbf2::HiDpiGlyphs& outlines, const char* list, int cand, float em, const std::vector<HiDpiGlyph>& glyphs,
                    size_t count, const HiDpiRef& ref, float* baseline, std::vector<float>& scratch)
{
    std::vector<float> bases;
    for (size_t i = 0; i < count; ++i)
        if (ref.has[i])
            bases.push_back(glyphs[i].top + glyphs[i].cy - ref.cy[i] * em);
    if (bases.empty() || bases.size() < count / 2)
        return 1e9f;
    const float base = hidpiMedian(bases);
    *baseline = base;
    double err = 0, norm = 0;
    for (size_t i = 0; i < count; ++i)
    {
        const HiDpiGlyph& q = glyphs[i];
        const size_t n = (size_t) q.w * q.h;
        for (size_t k = 0; k < n; ++k) norm += (double) q.bitmap[k] * q.bitmap[k];
        if (!ref.has[i])
        {
            for (size_t k = 0; k < n; ++k) err += (double) q.bitmap[k] * q.bitmap[k];
            continue;
        }
        scratch.resize(n);
        outlines.raster(list, cand, q.code, em, q.cx - ref.cx[i] * em, base - q.top,
                                &scratch[0], q.w, q.h, 4);
        for (size_t k = 0; k < n; ++k)
        {
            const double d = (double) scratch[k] - q.bitmap[k];
            err += d * d;
        }
    }
    return norm > 0 ? (float) (err / norm) : 1e9f;
}

// The EM size whose outline ink heights best match the bitmaps' (median ratio).
float hidpiEstimateEm(const std::vector<HiDpiGlyph>& glyphs, const HiDpiRef& ref, float nominal)
{
    std::vector<float> ems;
    for (size_t i = 0; i < glyphs.size(); ++i)
        if (ref.has[i] && glyphs[i].inkH >= 4.0f && ref.inkH[i] > 0.05f)
            ems.push_back(glyphs[i].inkH / ref.inkH[i]);
    return ems.size() >= 4 ? hidpiMedian(ems) : nominal;
}

struct HiDpiChoice { int cand; float em, base, err; };

// Quick (a few sizes around the estimate, first 24 glyphs) or full (fine search, all glyphs).
HiDpiChoice hidpiFit(const openbf2::HiDpiGlyphs& outlines, const char* list, int cand, const std::vector<HiDpiGlyph>& fit, float nominal,
                     bool quick, float startEm, std::vector<float>& scratch)
{
    HiDpiChoice best = { cand, 0.0f, 0.0f, 1e9f };
    HiDpiRef ref;
    hidpiReference(outlines, list, cand, fit, ref, scratch);
    const float em0 = startEm > 0.0f ? startEm : hidpiEstimateEm(fit, ref, nominal);
    if (quick)
    {
        const size_t count = fit.size() < 24 ? fit.size() : 24;
        for (float k = 0.92f; k <= 1.081f; k += 0.04f)
        {
            float b = 0.0f;
            const float e = hidpiEvaluate(outlines, list, cand, em0 * k, fit, count, ref, &b, scratch);
            if (e < best.err) { best.err = e; best.em = em0 * k; best.base = b; }
        }
        return best;
    }
    for (float k = 0.96f; k <= 1.0401f; k += 0.005f)
    {
        float b = 0.0f;
        const float e = hidpiEvaluate(outlines, list, cand, em0 * k, fit, fit.size(), ref, &b, scratch);
        if (e < best.err) { best.err = e; best.em = em0 * k; best.base = b; }
    }
    return best;
}

void* hidpiCreateTexture(const std::vector<unsigned int>& pixels, unsigned int w, unsigned int h)
{
    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(::dice::hfe::dx9rend::renderDevice());
    IDirect3DTexture9* tex = 0;
    if (!device || device->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr) != 0 || !tex)
        return 0;
    IDirect3DSurface9* surface = 0;
    tex->GetSurfaceLevel(0, &surface);
    RECT rect = { 0, 0, (LONG) w, (LONG) h };
    const bool loaded = surface && D3DXLoadSurfaceFromMemory(surface, 0, 0, &pixels[0], D3DFMT_A8R8G8B8,
                                                             w * 4, 0, &rect, 0xFFFFFFFF, 0) == 0;
    if (surface)
        surface->Release();
    void* storage = loaded ? ::renddx9abi::engineAlloc(sizeof(::dice::hfe::dx9rend::EngineTexture)) : 0;
    if (!storage)
    {
        tex->Release();
        return 0;
    }
    ::dice::hfe::dx9rend::EngineTexture* engine = ::dice::hfe::dx9rend::textureCtor(storage);
    engine->pool = 1;           // D3DPOOL_MANAGED: device-loss listeners leave it alone
    const unsigned int desc[7] = { w, h, 1, 1, (unsigned int) D3DFMT_A8R8G8B8, 3, 0x7FFFFFFF };
    ::dice::hfe::dx9rend::textureAttach(engine, tex, desc);
    tex->Release();
    return engine;
}

struct PendingFont
{
    void* owner;
    void (*retry)(void*);
    unsigned int generation;
};

const openbf2::HiDpiGlyphs* movieOutlines()
{
    return static_cast<const openbf2::HiDpiGlyphs*>(
        classManager->getSimpleSingleton(openbf2::glyphServiceName));
}

std::vector<PendingFont>& hidpiPending()
{
    static std::vector<PendingFont> s_pending;
    return s_pending;
}

void hidpiProcessPending()
{
    const auto* outlines = movieOutlines();
    if (!outlines) return;
    const unsigned int generation = outlines->generation();
    // A successful retry removes its entry.
    const auto pending = hidpiPending();
    for (const auto& entry : pending)
    {
        auto& live = hidpiPending();
        auto found = std::find_if(live.begin(), live.end(),
            [&](const PendingFont& p) { return p.owner == entry.owner; });
        if (found != live.end() && found->generation != generation)
        {
            found->generation = generation;
            entry.retry(entry.owner);
        }
    }
}

} // namespace

// 1: *outTexture holds a new reference to the rebuilt atlas. 0: no matching outlines yet (retry
// once more movies are loaded). -1: never (disabled, unreadable or coloured atlas).
int hidpiBuildFontTexture(const std::string& base, const char* exportName, int N, void** outTexture)
{
    *outTexture = 0;
    if (N < 2)
        return -1;
    const auto* provider = movieOutlines();
    if (!provider)
        return 0;
    const openbf2::HiDpiGlyphs& outlines = *provider;
    const char* exportList = (exportName && *exportName) ? exportName : 0;
    const int exportCount = exportList ? outlines.candidates(exportList) : 0;
    const int starCount = outlines.candidates("*");
    if (!exportCount && !starCount)
        return 0;
    unsigned int W = 0, H = 0;
    float size = 0.0f;
    std::vector<HiDpiGlyph> glyphs;
    std::vector<float> alpha;
    const unsigned int maxAtlasDimension = 4096;
    if (!hidpiReadDif(base, maxAtlasDimension / N, W, H, size, glyphs) ||
        !hidpiReadAtlas(base, W, H, alpha))
        return -1;
    hidpiMeasure(glyphs, alpha, W);
    std::vector<HiDpiGlyph> fit;
    for (const HiDpiGlyph& q : glyphs)
        if (q.inkH >= 0.0f && q.code > 32 && q.code < 127)
            fit.push_back(q);
    // Letters and digits first: the quick screen uses the first 24.
    std::stable_sort(fit.begin(), fit.end(), [](const HiDpiGlyph& a, const HiDpiGlyph& b) {
        const bool an = isalnum(a.code) != 0, bn = isalnum(b.code) != 0;
        return an && !bn;
    });
    if (fit.size() < 12)
        return -1;

    const float nominal = size;

    std::vector<float> scratch;
    const char* list = 0;
    HiDpiChoice choice = { -1, 0.0f, 0.0f, 1e9f };
    for (int i = 0; i < exportCount && i < 4; ++i)
    {
        const HiDpiChoice q = hidpiFit(outlines, exportList, i, fit, nominal, true, 0.0f, scratch);
        const HiDpiChoice f = hidpiFit(outlines, exportList, i, fit, nominal, false, q.em, scratch);
        if (f.err < choice.err) { choice = f; list = exportList; }
    }
    if (choice.err > kHiDpiMaxError)
    {
        std::vector<HiDpiChoice> quick;
        for (int i = 0; i < starCount; ++i)
            quick.push_back(hidpiFit(outlines, "*", i, fit, nominal, true, 0.0f, scratch));
        std::sort(quick.begin(), quick.end(),
                  [](const HiDpiChoice& a, const HiDpiChoice& b) { return a.err < b.err; });
        for (size_t i = 0; i < quick.size() && i < 2; ++i)
        {
            if (quick[i].err >= 1e9f)
                break;
            const HiDpiChoice f = hidpiFit(outlines, "*", quick[i].cand, fit, nominal, false, quick[i].em, scratch);
            if (f.err < choice.err) { choice = f; list = "*"; }
        }
    }
    if (choice.cand < 0 || choice.err > kHiDpiMaxError)
        return 0;

    // The atlas at N x: the shipped bitmap resampled, then every glyph with an outline redrawn.
    const unsigned int HW = W * N, HH = H * N;
    std::vector<unsigned int> pixels((size_t) HW * HH);
    for (unsigned int Y = 0; Y < HH; ++Y)
    {
        const float sy = ((float) Y + 0.5f) / (float) N - 0.5f;
        int y0 = (int) floorf(sy);
        const float fy = sy - (float) y0;
        int y1 = y0 + 1;
        if (y0 < 0) y0 = 0;
        if (y1 > (int) H - 1) y1 = (int) H - 1;
        for (unsigned int X = 0; X < HW; ++X)
        {
            const float sx = ((float) X + 0.5f) / (float) N - 0.5f;
            int x0 = (int) floorf(sx);
            const float fx = sx - (float) x0;
            int x1 = x0 + 1;
            if (x0 < 0) x0 = 0;
            if (x1 > (int) W - 1) x1 = (int) W - 1;
            const float a = (alpha[(size_t) y0 * W + x0] * (1 - fx) + alpha[(size_t) y0 * W + x1] * fx) * (1 - fy)
                          + (alpha[(size_t) y1 * W + x0] * (1 - fx) + alpha[(size_t) y1 * W + x1] * fx) * fy;
            pixels[(size_t) Y * HW + X] = ((unsigned int) (a * 255.0f + 0.5f) << 24) | 0x00FFFFFFu;
        }
    }
    HiDpiRef ref;
    hidpiReference(outlines, list, choice.cand, glyphs, ref, scratch);
    unsigned int fromOutlines = 0, withInk = 0;
    for (size_t i = 0; i < glyphs.size(); ++i)
    {
        const HiDpiGlyph& q = glyphs[i];
        if (q.inkH < 0.0f)
            continue;
        ++withInk;
        if (!ref.has[i])
            continue;
        const int cw = q.w * N, ch = q.h * N;
        scratch.resize((size_t) cw * ch);
        if (!outlines.raster(list, choice.cand, q.code, choice.em * (float) N,
                                     (q.cx - ref.cx[i] * choice.em) * (float) N,
                                     (choice.base - q.top) * (float) N, &scratch[0], cw, ch, 4))
            continue;
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x)
            {
                float a = scratch[(size_t) y * cw + x];
                if (a > 1.0f) a = 1.0f;
                pixels[(size_t) (q.y * N + y) * HW + q.x * N + x] =
                    ((unsigned int) (a * 255.0f + 0.5f) << 24) | 0x00FFFFFFu;
            }
        ++fromOutlines;
    }
    void* texture = hidpiCreateTexture(pixels, HW, HH);
    if (!texture)
        return -1;
    *outTexture = texture;
    ::renddx9abi::logf("[FontSystem] HiDPI \"%s\": SWF %s \"%s\", EM %.2f px, baseline %.2f, error %.3f; "
                       "atlas %ux%u -> %ux%u, %u of %u glyphs from outlines",
                       base.c_str(), list == exportList ? exportName : "*",
                       outlines.name(list, choice.cand), choice.em, choice.base, choice.err,
                       W, H, HW, HH, fromOutlines, withInk);
    return 1;
}

void hidpiAddPending(void* owner, void (*retry)(void*))
{
    auto& pending = hidpiPending();
    for (const auto& entry : pending)
        if (entry.owner == owner)
            return;
    if (pending.empty())
    {
        static openbf2::GlyphListener listener = { &hidpiProcessPending };
        classManager->registerSimpleSingleton(openbf2::glyphListenerName, &listener);
    }
    const auto* outlines = movieOutlines();
    pending.push_back({ owner, retry, outlines ? outlines->generation() : 0u });
}

void hidpiRemovePending(void* owner)
{
    auto& pending = hidpiPending();
    for (auto it = pending.begin(); it != pending.end(); ++it)
    {
        if (it->owner == owner)
        {
            pending.erase(it);
            if (pending.empty())
                classManager->unregisterSimpleSingleton(openbf2::glyphListenerName);
            return;
        }
    }
}

void DifFont::hidpiUpgrade()
{
    if (!m_atlas || m_scaleX <= 1.05f || m_deviceDetached)
        return;
    // "<export name>_<pixel height>"
    std::string name = m_key;
    {
        const size_t us = name.rfind('_');
        bool digits = us != std::string::npos && us + 1 < name.size();
        for (size_t i = us + 1; digits && i < name.size(); ++i)
            if (name[i] < '0' || name[i] > '9')
                digits = false;
        if (digits) name.resize(us); else name.clear();
    }
    int N = (int) ceilf(m_scaleX - 0.05f);
    if (N < 2) N = 2;
    if (N > 4) N = 4;
    void* hi = 0;
    const int result = hidpiBuildFontTexture(m_directory + m_key, name.c_str(), N, &hi);
    if (result == 0)
    {
        hidpiAddPending(this, [](void* self) { static_cast<DifFont*>(self)->hidpiUpgrade(); });
        return;
    }
    hidpiRemovePending(this);
    if (result < 0 || !hi)
        return;
    if (m_hidpiRetired)
    {
        ::dice::hfe::dx9rend::EngineTexture* retired =
            static_cast< ::dice::hfe::dx9rend::EngineTexture*>(m_hidpiRetired);
        ::dice::hfe::dx9rend::textureReleaseRef(retired);
    }
    m_hidpiRetired = m_atlas;
    m_atlas = hi;
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

// sub_10014930 - the EngineTexture's D3D texture, its +8 baseTexture.
void* engineTextureD3D(void* engineTexture)
{
    return engineTexture ? static_cast<dx9rend::EngineTexture*>(engineTexture)->baseTexture : 0;
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


// The Flash text batcher. Fonts append clip-space glyph quads to the current FONT_VBS ring buffer;
// SwiffRender flushes the queued run before its own geometry, at the mask brackets and at the end of
// a display. Registers for device resets at priority 5. Layout: the retail 0x78-byte object.

extern const unsigned int CID_FontRenderer = 81047u;   // matches ModuleGeomMisc's getSingletonList
extern const unsigned int IID_IFontRenderer = 80048u;

const void** fontRendererVtable();          // defined after the shims below
const void** fontRendererListenerVtable();

namespace {
// The fontselection unit quad {(0,0,0),(1,0,0),(0,1,0),(1,1,0)} - the twelve dwords both the init
// (0x100A143C..0x100A145C) and the reset (0x100A17D8..0x100A17F8) store, in the same order.
const float kFontSelectionQuad[12] = { 0,0,0,  1,0,0,  0,1,0,  1,1,0 };
const unsigned int kFontRingVertices = 10002u;   // 0x2712, each FONT_VBS
} // anonymous namespace

class FontRenderer
{
public:
    FontRenderer();                                         // 0x100A1740

    void init();                                            // 0x100A1240, SwiffRender::init
    void begin();                                           // 0x100A1470, SwiffRender::beginDisplay
    void endFrame();                                        // 0x100A1500, SwiffRender::endDisplay
    void setTechnique(const std::string& name);             // 0x100A1540, DifFont::setScale
    void setTexture(void* atlas) { m_texture = atlas; }     // 0x100A1590
    void flush();                                           // 0x100A15A0
    void advanceRing();                                     // 0x100A1660
    void lockAt(unsigned int firstVertex);                  // 0x100A16C0
    void unlock();                                          // 0x100A1720
    void teardown();                                        // 0x100A18A0, SwiffRender::shutdown
    void drawSelection(const float* matrix);

    // The batch state a font reads and advances while it appends a run (0x100A3DE0, 0x100A38D0).
    void*        texture() const       { return m_texture; }
    bool         locked() const        { return m_locked != 0; }
    bool         lockedThisFrame() const { return m_lockFrame == m_frame; }
    unsigned int cursor() const        { return m_cursor; }
    FontVertex*  reserve(unsigned int count);

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;
    void  destroy(char flags);                              // +0x10
    void  destructorBody();

    void  onDeviceLost();                                   // listener +0x00, 0x100A1220
    void  onDeviceReset();                                  // listener +0x04, 0x100A17B0
    // Both listener bodies are handed the subobject (0x100A1883 steps back with `lea ecx,[esi-4]`).
    static FontRenderer* fromListener(void* sub)
    {
        return reinterpret_cast<FontRenderer*>(static_cast<char*>(sub)
                                               - offsetof(FontRenderer, m_listenerVptr));
    }

private:
    friend class SwiffFontObject; // [CHOSEN, not recovered] Same-TU native batch-state access.
    const void** m_vptr;           // +0x00  IBase vtable 0x1020D274
    const void** m_listenerVptr;   // +0x04  DeviceLostResetListener 0x1020D268
    int          m_refCount;       // +0x08
    void*        m_texture;        // +0x0C  the atlas the queued run samples (EngineTexture*)
    void*        m_effect;         // +0x10  "mods/xpack/Shaders/Font"
    void*        m_paramTransform; // +0x14
    void*        m_paramDiffuse;   // +0x18
    void*        m_paramTexture;   // +0x1C
    void*        m_selDecl;        // +0x20  fontselection declaration (position only)
    void*        m_decl;           // +0x24  FONT_VBS declaration (position, colour, uv)
    void*        m_selVb;          // +0x28  FONTSELECTION
    FontVertex*  m_write;          // +0x2C  the locked range of the current ring buffer
    unsigned int m_count;          // +0x30  vertices queued since the last flush
    unsigned int m_vbIndex;        // +0x34  0..2
    void*        m_vb[3];          // +0x38  FONT_VBS ring
    void*        m_tech[3];        // +0x44  Regular, RegularScaled, SelectionQuad
    unsigned int m_techIndex;      // +0x50  0 Regular, 1 RegularScaled
    unsigned int m_start;          // +0x54  first vertex of the queued run
    unsigned int m_cursor;         // +0x58  vertices written to the current buffer
    unsigned char m_locked;        // +0x5C
    unsigned char m_pad5D[3];
    unsigned int m_frame;          // +0x60  endFrame counts displays
    unsigned int m_lockFrame;      // +0x64  m_frame when the current lock was taken
    unsigned int m_ringGeneration; // +0x68  advanceRing counts buffer switches
    unsigned int m_selectionFrame; // +0x6C  a text draw with a selection stores m_frame here
    unsigned int m_u70;            // +0x70  init clears it; no other access found
    unsigned int m_runs;           // +0x74  runs appended since begin
};

// PE 0x100A19C0
void FontRenderer::drawSelection(const float* matrix)
{
    using namespace dx9rend;
    flush();
    static_cast<ShaderParameter*>(m_paramTransform)->setValue(matrix, 0);
    const float color[4] = {0.0f, 0.0f, 0.0f, 0.3f};
    static_cast<ShaderParameter*>(m_paramDiffuse)->setValue(color, 0);
    effectSetTechniqueRecord(m_effect, m_tech[2]);
    vertexBufferBindStream(m_selVb, 0);
    vertexDeclarationBind(m_selDecl);
    effectBegin(m_effect);
    effectBeginPass(m_effect, 0);
    rendererDrawPrimitive(2, 5, 0);
    effectEnd(m_effect);
}

// PE 0x100A3560
void DifFont::drawSelection(NativeFontView* font, const float* matrix, float width)
{
    if (!font || m_scaleX == 0.0f || m_scaleY == 0.0f || width == 0.0f)
        return;
    unsigned int first, count;
    float offset;
    font->getRun(&first, &count, &offset);
    if (!count)
        return;
    D3DXMATRIX translated, projected, scaling, finalMatrix;
    memcpy(&translated, matrix, sizeof(translated));
    translated._41 += offset;
    D3DXMatrixMultiply(&projected, &translated, reinterpret_cast<const D3DXMATRIX*>(m_projection));
#if defined(_WIN32)
    D3DXMatrixScaling(&scaling, width, m_size, 1.0f);
#else
    // [CHOSEN, not recovered] The native D3DX subset has no scaling entry point.
    memset(&scaling, 0, sizeof(scaling));
    scaling._11 = width;
    scaling._22 = m_size;
    scaling._33 = scaling._44 = 1.0f;
#endif
    D3DXMatrixMultiply(&finalMatrix, &scaling, &projected);
    static_cast<FontRenderer*>(m_fontRenderer)->drawSelection(&finalMatrix._11);
}

// PE 0x100A1740. Registers the listener at priority 5 before the ring slots are cleared.
FontRenderer::FontRenderer()
    : m_vptr(fontRendererVtable()), m_listenerVptr(fontRendererListenerVtable()), m_refCount(1),
      m_texture(0), m_effect(0), m_paramTransform(0), m_paramDiffuse(0), m_paramTexture(0),
      m_selDecl(0), m_selVb(0), m_count(0), m_vbIndex(0), m_techIndex(0), m_start(0), m_cursor(0),
      m_locked(0), m_frame(0), m_lockFrame(0), m_ringGeneration(0)
{
#if defined(_M_IX86)
    static_assert(offsetof(FontRenderer, m_listenerVptr) == 0x04, "listener subobject");
    static_assert(offsetof(FontRenderer, m_write) == 0x2C, "the locked pointer");
    static_assert(offsetof(FontRenderer, m_vb) == 0x38, "the FONT_VBS ring");
    static_assert(offsetof(FontRenderer, m_locked) == 0x5C, "the lock byte");
    static_assert(sizeof(FontRenderer) == 0x78, "the factory's allocation (0x1007CF9C push 78h)");
#endif
    dx9rend::registerDeviceLostResetListener(&m_listenerVptr, 5);
    m_vb[0] = m_vb[1] = m_vb[2] = 0;
}

// PE 0x100A1240. The effect, its three parameters, both declarations, the three techniques, the
// cleared batch state, the ring and the selection quad. No step is null-tested.
void FontRenderer::init()
{
    using namespace dx9rend;
    m_effect = shaderManagerGetEffect(g_shaderManager, "mods/xpack/Shaders/Font");
    Effect* fx = static_cast<Effect*>(m_effect);
    m_paramTransform = effectGetShaderParameter(fx, std::string("TRANSFORM"));
    m_paramDiffuse   = effectGetShaderParameter(fx, std::string("DIFFUSE"));
    m_paramTexture   = effectGetShaderParameter(fx, std::string("TEXTURE"));

    m_decl    = vertexDeclarationCreate(kFontDeclElements);       // 0x1020D1C0
    m_selDecl = vertexDeclarationCreate(kFontSelDeclElements);    // 0x1020D1E0

    m_tech[0] = effectFindTechniqueRecord(m_effect, "Regular");
    m_tech[1] = effectFindTechniqueRecord(m_effect, "RegularScaled");
    m_tech[2] = effectFindTechniqueRecord(m_effect, "SelectionQuad");

    m_locked = 0;
    m_count = 0;
    m_cursor = 0;
    m_vbIndex = 0;
    m_frame = 0;
    m_lockFrame = 0;
    m_ringGeneration = 0;
    m_runs = 0;
    m_u70 = 0;
    m_selectionFrame = 0;

    const int prevPool = bufferManagerSwapPool(0);
    for (int i = 0; i < 3; ++i)
        m_vb[i] = bufferManagerCreateVertexBuffer(kFontRingVertices, 24, 0x208, "FONT_VBS");
    m_selVb = bufferManagerCreateVertexBuffer(4, 12, 0x208, "FONTSELECTION");
    bufferManagerSwapPool(prevPool);

    float* q = static_cast<float*>(vertexBufferLock(m_selVb, 0, 0, 0x2000));
    ::memcpy(q, kFontSelectionQuad, sizeof(kFontSelectionQuad));
    vertexBufferUnlock(m_selVb);
}

// PE 0x100A1470. A display starts a new run at the start of the current buffer; the lock and the
// ring position carry over. The techniques are looked up again in case the effect was rebuilt.
void FontRenderer::begin()
{
    m_count = 0;
    m_cursor = 0;
    m_start = 0;
    m_runs = 0;
    m_tech[0] = dx9rend::effectFindTechniqueRecord(m_effect, "Regular");
    m_tech[1] = dx9rend::effectFindTechniqueRecord(m_effect, "RegularScaled");
    m_tech[2] = dx9rend::effectFindTechniqueRecord(m_effect, "SelectionQuad");
}

// PE 0x100A1500. A selection older than two displays expires; the display count advances and the
// TEXTURE sampler is cleared.
void FontRenderer::endFrame()
{
    if (m_selectionFrame && m_selectionFrame + 2 < m_frame)
        m_selectionFrame = 0;
    ++m_frame;
    fxSetTexture(dx9rend::effectD3dxHandle(m_effect), paramGetHandle(m_paramTexture), 0);
}

// PE 0x100A1540. Any other name leaves the index unchanged.
void FontRenderer::setTechnique(const std::string& name)
{
    if (name == "Regular")
        m_techIndex = 0;
    else if (name == "RegularScaled")
        m_techIndex = 1;
}

// PE 0x100A15A0. Draws the queued run as one triangle list from m_start, then starts the next run
// where this one ended. The single pass is not closed with EndPass.
void FontRenderer::flush()
{
    using namespace dx9rend;
    if (!m_count)
        return;
    effectSetTechniqueRecord(m_effect, m_tech[m_techIndex]);
    fxSetTexture(effectD3dxHandle(m_effect), paramGetHandle(m_paramTexture),
                 engineTextureD3D(m_texture));
    if (m_locked)
    {
        vertexBufferUnlock(m_vb[m_vbIndex]);
        m_locked = 0;
    }
    vertexBufferBindStream(m_vb[m_vbIndex], 0);
    vertexDeclarationBind(m_decl);
    effectBegin(m_effect);
    effectBeginPass(m_effect, 0);
    rendererDrawPrimitive(m_count / 3, 4 /* D3DPT_TRIANGLELIST */, m_start);
    effectEnd(m_effect);
    m_count = 0;
    m_start = m_cursor;
}

// PE 0x100A1660. Moves to the next ring buffer, takes a discard lock on it and restarts the run.
void FontRenderer::advanceRing()
{
    if (m_locked)
    {
        dx9rend::vertexBufferUnlock(m_vb[m_vbIndex]);
        m_locked = 0;
    }
    if (++m_vbIndex >= 3)
        m_vbIndex = 0;
    ++m_ringGeneration;
    m_count = 0;
    m_cursor = 0;
    m_start = 0;
    m_lockFrame = m_frame;
    m_write = static_cast<FontVertex*>(dx9rend::vertexBufferLock(m_vb[m_vbIndex], 0, 0, 0x2000));
    m_locked = 1;
}

// PE 0x100A16C0. Locks the rest of the current buffer from firstVertex with D3DLOCK_NOOVERWRITE.
void FontRenderer::lockAt(unsigned int firstVertex)
{
    if (m_locked)
    {
        dx9rend::vertexBufferUnlock(m_vb[m_vbIndex]);
        m_locked = 0;
    }
    m_write = static_cast<FontVertex*>(dx9rend::vertexBufferLock(
        m_vb[m_vbIndex], firstVertex, kFontRingVertices - firstVertex, 0x1000));
    m_locked = 1;
    m_lockFrame = m_frame;
    m_count = 0;
}

// PE 0x100A1720.
void FontRenderer::unlock()
{
    if (m_locked)
    {
        dx9rend::vertexBufferUnlock(m_vb[m_vbIndex]);
        m_locked = 0;
    }
}

// The append step of 0x100A38D0 (0x100A3A11..0x100A3A34): the run's slot in the locked range,
// then both counters advance by the reserved vertex count.
FontVertex* FontRenderer::reserve(unsigned int count)
{
    FontVertex* out = m_write + m_count;
    m_count += count;
    m_cursor += count;
    return out;
}

// PE 0x100A1220 (listener +0x00). An outstanding lock is released before the BufferManager, at a
// lower priority, releases the buffers.
void FontRenderer::onDeviceLost()
{
    if (m_locked)
    {
        dx9rend::vertexBufferUnlock(m_vb[m_vbIndex]);
        m_locked = 0;
    }
}

// PE 0x100A17B0 (listener +0x04). Refills the selection quad, looks the techniques up again and,
// when the ring exists, moves to the next buffer.
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
        m_tech[0] = dx9rend::effectFindTechniqueRecord(m_effect, "Regular");
        m_tech[1] = dx9rend::effectFindTechniqueRecord(m_effect, "RegularScaled");
        m_tech[2] = dx9rend::effectFindTechniqueRecord(m_effect, "SelectionQuad");
    }
    if (m_vb[m_vbIndex])
        advanceRing();
}

// PE 0x100A18A0. Releases the parameters, the effect, both declarations and the buffers, testing
// each for null.
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

// PE 0x100A1AC0. The final release runs the virtual destructor, which unregisters the listener.
int FontRenderer::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        typedef void* (__thiscall* DestroyFn)(void*, char);
        reinterpret_cast<DestroyFn>(const_cast<void*>(m_vptr[0x10 / 4]))(this, 1);
    }
    return remaining;
}

// Restores both tables, unregisters the device listener, then tears the resources down.
// PE 0x100A1A60. unreachable in 1.50: no code or data references to this out-of-line copy; the
// deleting destructor 0x100A1AE0 inlines it. [INFERRED] Name.
void FontRenderer::destructorBody()
{
    m_vptr         = fontRendererVtable();
    m_listenerVptr = fontRendererListenerVtable();
    dx9rend::unregisterDeviceLostResetListener(&m_listenerVptr);
    teardown();
}

// PE 0x100A1AE0. Unregisters the listener, tears down and frees when bit 0 of flags is set.
void FontRenderer::destroy(char flags)
{
    destructorBody();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
}

// PE 0x100A1A90.
void* FontRenderer::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IFontRenderer)
        return const_cast<FontRenderer*>(this);
    return 0;
}

// The FontRenderer operations SwiffRender.cpp calls on its +1036 singleton.
void fontRendererInit(void* fr)     { static_cast<FontRenderer*>(fr)->init(); }
void fontRendererBegin(void* fr)    { static_cast<FontRenderer*>(fr)->begin(); }
void fontRendererFlush(void* fr)    { static_cast<FontRenderer*>(fr)->flush(); }
void fontRendererUnlock(void* fr)   { static_cast<FontRenderer*>(fr)->unlock(); }
void fontRendererEndFrame(void* fr) { static_cast<FontRenderer*>(fr)->endFrame(); }
void fontRendererTeardown(void* fr) { static_cast<FontRenderer*>(fr)->teardown(); }

namespace {
void  RD9_THUNK fr_addRef(void* s RD9_THUNK_EDX)  { static_cast<FontRenderer*>(s)->addRef(); }
int   RD9_THUNK fr_getRef(void* s RD9_THUNK_EDX)  { return static_cast<FontRenderer*>(s)->getRef(); }
int   RD9_THUNK fr_release(void* s RD9_THUNK_EDX) { return static_cast<FontRenderer*>(s)->release(); }
void* RD9_THUNK fr_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{ return static_cast<FontRenderer*>(s)->queryInterface(iid); }
void* RD9_THUNK fr_destroy(void* s RD9_THUNK_EDX, char flags)
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
void RD9_THUNK fr_onDeviceLost(void* sub RD9_THUNK_EDX)  { FontRenderer::fromListener(sub)->onDeviceLost(); }
void RD9_THUNK fr_onDeviceReset(void* sub RD9_THUNK_EDX) { FontRenderer::fromListener(sub)->onDeviceReset(); }
void RD9_THUNK fr_onDeviceDestroy(void* RD9_THUNK_EDX)   { /* 0x10037D40: ret, the shared nullsub */ }

const void* kFontRendererListenerVtable[3] =
{
    reinterpret_cast<void*>(&fr_onDeviceLost),    // +0x00
    reinterpret_cast<void*>(&fr_onDeviceReset),   // +0x04
    reinterpret_cast<void*>(&fr_onDeviceDestroy), // +0x08
};
} // anonymous namespace

const void** fontRendererVtable() { return kFontRendererVtable; }
const void** fontRendererListenerVtable() { return kFontRendererListenerVtable; }

// PE 0x100A3450. The FontRenderer is resolved by load(); the comparison is `scale.x > 1.0f`.
void DifFont::setScale(float x, float y)
{
    m_scaleX = x;
    m_scaleY = y;
    if (m_fontRenderer)
        static_cast<FontRenderer*>(m_fontRenderer)->setTechnique(
            m_scaleX > 1.0f ? std::string("RegularScaled") : std::string("Regular"));
}

// RefFactory for dice.hfe.geom.FontRenderer. SwiffRender::init runs init() (0x10152560).
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
    static void  RD9_THUNK vInvalidate(void* self RD9_THUNK_EDX);                                 // +8
    static int   RD9_THUNK vSetText(void* self RD9_THUNK_EDX, const wchar_t* text, int flag);     // +16
    static void  RD9_THUNK vDraw(void* self RD9_THUNK_EDX, const float* m, unsigned int color);   // +20
    static char  RD9_THUNK vGetExtent(void* self RD9_THUNK_EDX, int arg, float* outW, float* outH);// +28
    static void RD9_THUNK vGetRun(void* self RD9_THUNK_EDX, unsigned int*, unsigned int*, float*);
    static void RD9_THUNK vGetFirst(void* self RD9_THUNK_EDX, unsigned int*);
    static bool RD9_THUNK vWouldOverflow(void* self RD9_THUNK_EDX);
    static const std::vector<GlyphCell>* RD9_THUNK vGetCells(void* self RD9_THUNK_EDX);
    static const float* RD9_THUNK vGetColor(void* self RD9_THUNK_EDX);
    static void RD9_THUNK vBaseGetRun(void* self RD9_THUNK_EDX, unsigned int*, unsigned int*, float*);
    static void RD9_THUNK vBaseGetFirst(void* self RD9_THUNK_EDX, unsigned int*);
    static void RD9_THUNK vBaseSetText(void* self RD9_THUNK_EDX, const wchar_t*, int);
    static void RD9_THUNK vBaseDraw(void* self RD9_THUNK_EDX, const float*, unsigned int);
    static const float* RD9_THUNK vBaseGetColor(void* self RD9_THUNK_EDX);
    static char RD9_THUNK vBaseGetExtent(void* self RD9_THUNK_EDX, int, float*, float*);
    static char RD9_THUNK vBaseHit(void* self RD9_THUNK_EDX, int*, float, float);
#if !defined(_MSC_VER) || defined(_WIN64)
    // [CHOSEN, not recovered] Native (and MSVC x64, where an int cannot carry the pointer): the
    // shipped +32 and +40 bodies take int* outs. Each arm's definition below carries the citation.
    static char  RD9_THUNK vForward32(void* self RD9_THUNK_EDX, int* outPos, float x, float y);   // +32
    static char  RD9_THUNK vForward40(void* self RD9_THUNK_EDX, int* outA, int* outB);            // +40
#else
    static char  RD9_THUNK vForward32(void* self RD9_THUNK_EDX, int arg, float x, float y);       // +32
    static char  RD9_THUNK vForward40(void* self RD9_THUNK_EDX, int a, int b);                     // +40
#endif
    static char  RD9_THUNK vForward44(void* self RD9_THUNK_EDX, float x1, float y1,
                                       float x2, float y2);                                  // +44
    static int   RD9_THUNK vForward48(void* self RD9_THUNK_EDX);                                   // +48

    DifFont* font() const { return m_font; }

    // The two setters SwiffRender's font slots drive (sub_101530D0 / sub_10153120). Public so the
    // namespace-scope seams can reach them; the shipped code pokes the equivalent byte offsets.
    void configure(float boxWidth, int align);   // +120 box width, +136 alignment (offset left to setText)
    void setScrollIndex(int value);               // +104 caret index, then the visible window (sub_100A3710)
    // [CHOSEN, not recovered] Named access to the native wrapper's existing window recomputation.
    void refreshWindow() { recomputeWindow(); }
    void destroy();                               // SwiffRender slot 27's delete (0x100A42F0 + free)

private:
    void setText(const wchar_t* text, int flag);
    void setColor(unsigned int rgba);
    void draw(const float* m, unsigned int color);
    void drawRun(const float* m);
    void initializeBase(DifFont* font, const wchar_t* text);
    bool measure(unsigned int index, float* outW, float* outH) const;
    void recomputeWindow();

    static const unsigned int kTextCap = 512;

    const void** m_vptr;           // must be first - SwiffRender reads *font as the vtable
    DifFont*     m_font;
    // [CHOSEN, not recovered] Inline text storage avoids repeated heap allocation; the original uses std::wstring.
    wchar_t      m_textBuf[kTextCap];
    unsigned int m_textLen;
    unsigned int m_color;          // packed D3DCOLOR the emitter writes per-vertex (shipped +68)
    unsigned int m_previousColor;
    float        m_colB, m_colG, m_colR, m_colA;   // original blue/green/red/alpha component order
    std::vector<GlyphCell> m_cells;
    unsigned int m_runStart;
    unsigned int m_runCount;
    unsigned int m_runGeneration;
    unsigned char m_dirty;
    unsigned char m_drawingSelection;
    float m_lastX, m_lastY;
    float        m_boxWidth;       // alignment box width (shipped +120)
    int          m_align;          // 0=left/none, 1=right, 2=center (shipped +136; sub_100A4F20's enum)
    float        m_alignOffset;    // computed x offset (shipped +140)
    // The text-field selection (RW-80): shipped +0x7C start and +0x80 end glyph index, zeroed by the
    // constructor (0x100A272B) and by +0x30 (sub_100A3440), read by +0x28 (sub_10152080) and by the
    // draw's highlight arm (0x100A4FF0 / 0x100A5013: a non-zero end draws it). +0x80 was earlier read
    // here as a drop-shadow colour; nothing wrote or read that member.
    int          m_selStart;       // shipped +124
    int          m_selEnd;         // shipped +128
    // Visible glyph range computed by setScrollIndex to fit the box and keep the caret in view.
    unsigned int m_scrollIndex;    // shipped +104, the caret index the window keeps in view
    unsigned int m_firstVisible;   // shipped +108, the first glyph drawn
    unsigned int m_visibleCount;   // shipped +112, how many glyphs are drawn from there
    float        m_scrollOffset;   // shipped +116, -(scaleX * left of the first drawn glyph), pixels

    void assignText(const wchar_t* text);
    void computeAlign();
};

// Base and derived wrapper tables preserve the attested slot order.
namespace {
const void* kSwiffFontVtable[13] =
{
    reinterpret_cast<void*>(&SwiffFontObject::vGetRun),           // 0
    reinterpret_cast<void*>(&SwiffFontObject::vGetFirst),         // 1
    reinterpret_cast<void*>(&SwiffFontObject::vInvalidate),         // 2  (+8)  invalidate
    reinterpret_cast<void*>(&SwiffFontObject::vWouldOverflow),    // 3
    reinterpret_cast<void*>(&SwiffFontObject::vSetText),           // 4  (+16) setText
    reinterpret_cast<void*>(&SwiffFontObject::vDraw),              // 5  (+20) draw
    reinterpret_cast<void*>(&SwiffFontObject::vGetColor),         // 6
    reinterpret_cast<void*>(&SwiffFontObject::vGetExtent),        // 7  (+28) measure
    reinterpret_cast<void*>(&SwiffFontObject::vForward32),         // 8  (+32)
    reinterpret_cast<void*>(&SwiffFontObject::vGetCells),          // 9
    reinterpret_cast<void*>(&SwiffFontObject::vForward40),         // 10 (+40)
    reinterpret_cast<void*>(&SwiffFontObject::vForward44),         // 11 (+44)
    reinterpret_cast<void*>(&SwiffFontObject::vForward48),         // 12 (+48)
};

const void* kBaseFontVtable[10] =
{
    reinterpret_cast<void*>(&SwiffFontObject::vBaseGetRun),
    reinterpret_cast<void*>(&SwiffFontObject::vBaseGetFirst),
    reinterpret_cast<void*>(&SwiffFontObject::vInvalidate),
    reinterpret_cast<void*>(&SwiffFontObject::vWouldOverflow),
    reinterpret_cast<void*>(&SwiffFontObject::vBaseSetText),
    reinterpret_cast<void*>(&SwiffFontObject::vBaseDraw),
    reinterpret_cast<void*>(&SwiffFontObject::vBaseGetColor),
    reinterpret_cast<void*>(&SwiffFontObject::vBaseGetExtent),
    reinterpret_cast<void*>(&SwiffFontObject::vBaseHit),
    reinterpret_cast<void*>(&SwiffFontObject::vGetCells),
};

} // anonymous namespace

// PE 0x10151FE0 (derived constructor: base PE 0x100A4E20, then +0x68..+0x8C zeroed, vptr 0x1021E0D0).
// [CHOSEN, not recovered] The shipped constructor takes no reference on the DifFont (the borrowed
// FontSystem::getFont result, PE 0x100A09C0) and the shipped destroy (PE 0x100A42F0) releases none.
// The addRef below is balanced by exactly one release in SwiffFontObject::destroy, reached from
// sr_releaseFontHandle (PE 0x10154870); it keeps the font alive if FontSystem tears down first.
SwiffFontObject::SwiffFontObject(DifFont* font, const wchar_t* initialText)
    : m_vptr(kBaseFontVtable), m_font(font), m_textLen(0),
      m_color(0xFFFFFFFFu), m_previousColor(0), m_colB(1.0f), m_colG(1.0f), m_colR(1.0f), m_colA(1.0f),
      m_runStart(0), m_runCount(0), m_runGeneration(0), m_dirty(1), m_drawingSelection(0), m_lastX(0), m_lastY(0),
      m_boxWidth(0.0f), m_align(0), m_alignOffset(0.0f), m_selStart(0), m_selEnd(0),
      m_scrollIndex(0), m_firstVisible(0), m_visibleCount(0), m_scrollOffset(0.0f)
{
    initializeBase(font, initialText);
    if (font)
        font->addRef();
    m_vptr = kSwiffFontVtable;
}

// PE 0x100A4E20. [CHOSEN, not recovered] Native outer construction supplies
// bounded text storage and defined cache/color fields where retail leaves bytes unwritten.
void SwiffFontObject::initializeBase(DifFont* font, const wchar_t* text)
{
    m_vptr = kBaseFontVtable;
    m_textBuf[0] = 0;
    m_textLen = 0;
    m_font = font;
    m_runCount = 0;
    m_runStart = 0;
    m_runGeneration = 0;
    m_color = 0xFFFFFFFFu;
    m_previousColor = 0;
    if (text)
        vBaseSetText(this RD9_THUNK_ARG, text, 1);
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
void SwiffFontObject::setText(const wchar_t* text, int flag)
{
    if (swiffFontTraceOn())
    {
        char narrow[64]; int n = 0;
        for (const wchar_t* p = text; p && *p && n < 63; ++p) narrow[n++] = (*p < 128) ? (char) *p : '?';
        narrow[n] = 0;
        ::renddx9abi::logf("[font] text %p '%s'", this, narrow);
    }
    vBaseSetText(this RD9_THUNK_ARG, text, flag);
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
    const unsigned int packed = (b3 << 24) | (b0 << 16) | (b1 << 8) | b2;
    if (m_previousColor != packed)
    {
        m_color = packed;
        m_previousColor = packed;
        m_dirty = 1;
    }
}

// PE 0x100A6010
void RD9_THUNK SwiffFontObject::vBaseGetRun(void* self RD9_THUNK_EDX,
    unsigned int* first, unsigned int* count, float* offset)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    *first = o->m_runStart;
    *count = o->m_runCount;
    *offset = 0.0f;
}

// PE 0x100A6030
void RD9_THUNK SwiffFontObject::vBaseGetFirst(void* self RD9_THUNK_EDX, unsigned int* first)
{
    *first = static_cast<SwiffFontObject*>(self)->m_runStart;
}

// PE 0x100A33E0
void RD9_THUNK SwiffFontObject::vGetRun(void* self RD9_THUNK_EDX,
    unsigned int* first, unsigned int* count, float* offset)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    if (o->m_drawingSelection)
    {
        *first = static_cast<unsigned int>(o->m_selStart) * 6u;
        *count = (static_cast<unsigned int>(o->m_selEnd) - static_cast<unsigned int>(o->m_selStart)) * 6u;
    }
    else
    {
        *first = o->m_runStart + o->m_firstVisible * 6u;
        *count = o->m_visibleCount * 6u;
    }
    *offset = o->m_alignOffset + o->m_scrollOffset;
}

// PE 0x100A33D0
void RD9_THUNK SwiffFontObject::vGetFirst(void* self RD9_THUNK_EDX, unsigned int* first)
{
    *first = static_cast<SwiffFontObject*>(self)->m_firstVisible;
}

// PE 0x100A3380
bool RD9_THUNK SwiffFontObject::vWouldOverflow(void* self RD9_THUNK_EDX)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    o->m_runCount = o->m_textLen * 6u;
    const FontRenderer* const fr = static_cast<FontRenderer*>(o->m_font->fontRenderer());
    return fr->m_cursor + o->m_runCount > kFontRingVertices;
}

// PE 0x100A4330
const std::vector<GlyphCell>* RD9_THUNK SwiffFontObject::vGetCells(void* self RD9_THUNK_EDX)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    if (o->m_cells.size() != o->m_textLen)
    {
        std::vector<GlyphCell>().swap(o->m_cells);
        o->m_cells.resize(o->m_textLen);
        o->m_font->layout(o->m_textBuf, o->m_textLen, o->m_cells.data());
    }
    return &o->m_cells;
}

// PE 0x100A43A0
void RD9_THUNK SwiffFontObject::vBaseSetText(void* self RD9_THUNK_EDX, const wchar_t* text, int flag)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    std::vector<GlyphCell>().swap(o->m_cells);
    o->assignText(text);
    if (static_cast<unsigned char>(flag))
        reinterpret_cast<NativeFontView*>(self)->invalidate();
}

// PE 0x100A6040
const float* RD9_THUNK SwiffFontObject::vBaseGetColor(void* self RD9_THUNK_EDX)
{
    return &static_cast<SwiffFontObject*>(self)->m_colB;
}

// PE 0x10152030
const float* RD9_THUNK SwiffFontObject::vGetColor(void* self RD9_THUNK_EDX)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    if (o->m_drawingSelection)
    {
        static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        return white;
    }
    return &o->m_colB;
}

// PE 0x100A4E80
char RD9_THUNK SwiffFontObject::vBaseGetExtent(void* self RD9_THUNK_EDX,
    int index, float* width, float* height)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    if (!o->m_font)
        return 0;
    return o->measure(static_cast<unsigned int>(index), width, height) ? 1 : 0;
}

// PE 0x100A4EC0
char RD9_THUNK SwiffFontObject::vBaseHit(void* self RD9_THUNK_EDX,
    int* index, float x, float y)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    if (!o->m_font)
        return 0;
    return o->m_font->hitTest(o->m_textBuf, o->m_textLen, index, x, y, 0, o->m_textLen) ? 1 : 0;
}

// PE 0x100A40F0
void RD9_THUNK SwiffFontObject::vBaseDraw(void* self RD9_THUNK_EDX,
    const float* matrix, unsigned int color)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    o->setColor(color);
    if (o->m_font)
        o->drawRun(matrix);
}

// PE 0x100A4FD0
void SwiffFontObject::draw(const float* matrix, unsigned int color)
{
    setColor(color);
    if (!m_font)
        return;
    // [CHOSEN, not recovered] Preserve the existing native missing-renderer guard.
    if (!m_font->fontRenderer())
        return;
    if (m_selEnd)
    {
        FontRenderer* const fr = static_cast<FontRenderer*>(m_font->fontRenderer());
        fr->m_selectionFrame = fr->m_frame;
    }
    drawRun(matrix);
    if (!m_selEnd)
        return;
    float start, end, height;
    if (!measure(static_cast<unsigned int>(m_selStart), &start, &height) ||
        !measure(static_cast<unsigned int>(m_selEnd), &end, &height))
        return;
    m_drawingSelection = 1;
    D3DXMATRIX translation, translated;
#if defined(_WIN32)
    D3DXMatrixTranslation(&translation, start, 0.0f, 0.0f);
#else
    // [CHOSEN, not recovered] The native D3DX subset has no translation entry point.
    memset(&translation, 0, sizeof(translation));
    translation._11 = translation._22 = translation._33 = translation._44 = 1.0f;
    translation._41 = start;
#endif
    D3DXMatrixMultiply(&translated, reinterpret_cast<const D3DXMATRIX*>(matrix), &translation);
    drawRun(matrix);
    m_font->drawSelection(reinterpret_cast<NativeFontView*>(this), &translated._11, end - start);
    m_drawingSelection = 0;
}

// Measures the scaled start of character index, or the final right edge when index is past the text.
// outH receives the glyph's vertical offset. Returns false with zero outputs for empty text.
// PE 0x100A4810: measurement operation. [CHOSEN, not recovered] The wrapper owns bounded
// text/cell storage and retains its existing nullable-output guards.
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
void SwiffFontObject::drawRun(const float* m)
{
    if (!m_font || !m_textLen)
        return;
    if (m_font->scaleX() == 0.0f || m_font->scaleY() == 0.0f)    // sub_100A3DE0's first tests
        return;
    FontRenderer* fr = static_cast<FontRenderer*>(m_font->fontRenderer());
    if (!fr)
        return;
    NativeFontView* const view = reinterpret_cast<NativeFontView*>(this);
    unsigned int firstVertex, vertexCount;
    float offset;
    view->getRun(&firstVertex, &vertexCount, &offset);
    if (!vertexCount)
        return;
    if (fr->m_selectionFrame || (!m_dirty && m_runGeneration == fr->m_ringGeneration &&
        firstVertex != 0 && firstVertex != fr->m_cursor))
    {
        fr->flush();
        fr->advanceRing();
    }
    if (m_dirty && m_runGeneration != 0)
    {
        fr->flush();
        fr->advanceRing();
    }
    if (view->wouldOverflow())
    {
        fr->flush();
        fr->advanceRing();
    }
    if (fr->texture() != m_font->atlas())
    {
        fr->flush();
        fr->setTexture(m_font->atlas());
    }
    if (m_lastX == m[12] && m_lastY == m[13] && !m_dirty && m_runGeneration == fr->m_ringGeneration)
    {
        fr->m_count += vertexCount;
        fr->m_cursor += vertexCount;
        ++fr->m_runs;
        return;
    }
    if (!fr->locked() && !fr->lockedThisFrame())
    {
        fr->flush();
        fr->advanceRing();
    }
    unsigned int first;
    view->getFirst(&first);
    m_lastX = m[12];
    m_lastY = m[13];

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
                           "  projVp=(%d,%d,%d,%d) drawVp=(%d,%d,%d,%d) win=%u+%u/%u box=%.1f scroll=%.1f col=%08X",
                           this, narrow, m[13], m[12], m_alignOffset,
                           g0 ? g0->bearingY : -999.0f, g0 ? g0->height : -999.0f, m_font->size(),
                           (int) pv[0], (int) pv[1], (int) pv[2], (int) pv[3],
                           dvp[0], dvp[1], dvp[2], dvp[3],
                           m_firstVisible, m_visibleCount, m_textLen, m_boxWidth, m_scrollOffset, m_color);
    }

    // Round horizontal placement with floor(x + offsets + 0.5); vertical placement uses floor(y).
    float t[16];
    ::memcpy(t, m, sizeof(t));
    const float translatedX = m[12] + offset; // PE 0x100A3FB0 spills before roundFontPosition.
    t[12] = static_cast<float>(roundFontPosition(translatedX));
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

    if (!fr->locked())
        fr->lockAt(fr->cursor());

    // The positions come from the layout (sub_100A4BF0), read at the window's indices: the emitter
    // walks records and characters from the first visible one (a4 = slot 1's answer, sub_100A33D0).
    // [CHOSEN] `i < m_textLen` is added to the window bound: the shipped trusts the window, which is
    // stale only if the text shrinks between a caret call and a draw - never on SwiffPlayer's path
    // (formatText ends with the caret calls) - and this port's buffers are fixed arrays.
    const unsigned int end = first + vertexCount / 6u;
    unsigned int present = 0;
    for (unsigned int i = first; i < end && i < m_textLen; ++i)
    {
        const unsigned short ch = static_cast<unsigned short>(m_textBuf[i]);
        if (m_font->glyph(ch == 160 ? 32 : ch))
            ++present;
    }
    m_runStart = fr->m_cursor;
    m_runGeneration = fr->m_ringGeneration;
    // Texel bias tests exact unit scale (0x100A39D8); the technique was chosen by DifFont::setScale.
    // [CHOSEN, not recovered] Retain the existing zero-dimension divisor omission.
    const float invW = 1.0f / static_cast<float>(m_font->atlasW() ? m_font->atlasW() : 1);
    const float invH = 1.0f / static_cast<float>(m_font->atlasH() ? m_font->atlasH() : 1);
    const float bias = (m_font->scaleX() == 1.0f) ? 0.0099999998f : 0.0f;
    const std::vector<GlyphCell>& cells = *view->getCells();
    FontVertex* output = fr->reserve(present * 6u);
    if (!output)
    {
        ++fr->m_runs;
        return;
    }
    unsigned int vc = 0;
    for (unsigned int i = first; i < end && i < m_textLen; ++i)
    {
        unsigned short ch = m_textBuf[i];
        if (ch == 160) ch = 32;
        const DifGlyph* g = m_font->glyph(ch);
        if (!g)
            continue;

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

            output[vc++] = tl; output[vc++] = tr; output[vc++] = bl;
            output[vc++] = bl; output[vc++] = tr; output[vc++] = br;
        }
    }
    m_dirty = 0;
    ++fr->m_runs;
}

// PE 0x100A33B0
void RD9_THUNK SwiffFontObject::vInvalidate(void* self RD9_THUNK_EDX)
{
    SwiffFontObject* const o = static_cast<SwiffFontObject*>(self);
    if (o->m_font && o->m_textLen)
        o->m_dirty = 1;
}
int RD9_THUNK SwiffFontObject::vSetText(void* self RD9_THUNK_EDX, const wchar_t* text, int flag)
{
    static_cast<SwiffFontObject*>(self)->setText(text, flag);
    return 0;
}
void RD9_THUNK SwiffFontObject::vDraw(void* self RD9_THUNK_EDX, const float* m, unsigned int color)
{
    static_cast<SwiffFontObject*>(self)->draw(m, color);
}
// sub_100A5160 - the extent up to character `arg` (SwiffPlayer asks with the caret for the caret's x
// and with the line length for word wrap), plus the window's scroll offset. No font: 0 with the
// outs untouched; no text: 0 with both zeroed (sub_100A4810's own first stores).
char RD9_THUNK SwiffFontObject::vGetExtent(void* self RD9_THUNK_EDX, int arg, float* outW, float* outH)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    if (!o->m_font)
        return 0;
    if (!vBaseGetExtent(self RD9_THUNK_ARG, arg, outW, outH))
        return 0;
    *outW = o->m_scrollOffset + *outW;
    return 1;
}
#if !defined(_MSC_VER) || defined(_WIN64)
// sub_100A51B0 (+0x20): no font answers false. Otherwise DifFont::hitTest(text, out, x - scroll
// offset (+0x74), y, first visible (+0x6C), first + visible count (+0x70)) and its answer.
char RD9_THUNK SwiffFontObject::vForward32(void* self RD9_THUNK_EDX, int* outPos, float x, float y)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    if (!o->m_font)
        return 0;
    return o->m_font->hitTest(o->m_textBuf, o->m_textLen, outPos, x - o->m_scrollOffset, y,
                              o->m_firstVisible, o->m_visibleCount + o->m_firstVisible) ? 1 : 0;
}
// sub_10152080 (+0x28): *outA = selection start (+0x7C), *outB = selection end (+0x80); the answer
// is end != 0 (`cmp 0,[ecx+80h]; sbb; neg`).
char RD9_THUNK SwiffFontObject::vForward40(void* self RD9_THUNK_EDX, int* outA, int* outB)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    *outA = o->m_selStart;
    *outB = o->m_selEnd;
    return o->m_selEnd != 0 ? 1 : 0;
}
#else
// The 32-bit declarations carry the same out pointers as ints (x86: identical bits). The same
// retail bodies as the arm above, which carries their citations.
char RD9_THUNK SwiffFontObject::vForward32(void* self RD9_THUNK_EDX, int arg, float x, float y)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    if (!o->m_font)
        return 0;
    return o->m_font->hitTest(o->m_textBuf, o->m_textLen, reinterpret_cast<int*>(arg),
                              x - o->m_scrollOffset, y, o->m_firstVisible,
                              o->m_visibleCount + o->m_firstVisible) ? 1 : 0;
}
char RD9_THUNK SwiffFontObject::vForward40(void* self RD9_THUNK_EDX, int a, int b)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    *reinterpret_cast<int*>(a) = o->m_selStart;
    *reinterpret_cast<int*>(b) = o->m_selEnd;
    return o->m_selEnd != 0 ? 1 : 0;
}
#endif
// sub_100A5200 (+0x2C): no font answers false. Otherwise the selection is cleared through this
// object's own +0x30, then DifFont::selectRange(text, &start, &end, x1 - offset, y1, x2 - offset, y2,
// first visible, first + count). A refusal answers false; an empty range (start == end) is cleared
// again through +0x30 and answers false; otherwise true.
char RD9_THUNK SwiffFontObject::vForward44(void* self RD9_THUNK_EDX, float x1, float y1, float x2, float y2)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    if (!o->m_font)
        return 0;
    (void) vForward48(self RD9_THUNK_ARG);       // shipped: this->vtbl[+0x30], this table's vForward48
    if (!o->m_font->selectRange(o->m_textBuf, o->m_textLen, &o->m_selStart, &o->m_selEnd,
                                x1 - o->m_scrollOffset, y1, x2 - o->m_scrollOffset, y2,
                                o->m_firstVisible, o->m_visibleCount + o->m_firstVisible))
        return 0;
    if (o->m_selStart == o->m_selEnd)
    {
        (void) vForward48(self RD9_THUNK_ARG);
        return 0;
    }
    return 1;
}
// sub_100A3440 (+0x30): start = end = 0; EAX is the zero it stored.
int  RD9_THUNK SwiffFontObject::vForward48(void* self RD9_THUNK_EDX)
{
    SwiffFontObject* o = static_cast<SwiffFontObject*>(self);
    o->m_selStart = 0;
    o->m_selEnd = 0;
    return 0;
}


// Caches fonts by name and size, and reloads them when the display width changes.
// A zero width uses GSWindowSizeX. Cache keys are lowercased before lookup and insertion.

extern const unsigned int CID_FontSystem = 81044u;    // matches ModuleGeomMisc's getSingletonList
extern const unsigned int IID_IFontSystem = 80045u;

const void** fontSystemVtable();      // defined after the shims below
const void** fontSystemEventVtable(); // defined after the shims below

class FontSystem
{
public:
    FontSystem();

    DifFont* getFont(const char* key);

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;

    // the +4 subobject's one slot, IEventListener::handleEvent(category, id, node,
    // data), thiscall with `retn 10h`: the front's __fastcall shim shape (ecx = the subobject).
    static void RD9_THUNK handleRendererEvent(void* listener RD9_THUNK_EDX, int category, unsigned int id,
                                               void* node, void* data);

private:
    ~FontSystem();
    void     init();                                  // sub_100A0A80 - clear, then register (11)
    void     clear();                                 // sub_100A0B30 - drop every cached font
    bool     reloadFonts(int width, int height);      // sub_100A0F10
    DifFont* loadFont(const char* key, const ui::MenuViewport& viewport);
    DifFont* createLoadedFont(std::string directory, const char* key, const ui::MenuViewport& viewport);
    unsigned short* buildLexiconCharacters();
    bool     readLexiconCharacters(const std::string& path,
                                   std::map<unsigned short, bool>& characters);

    const void** m_vptr;           // +0 - IBase vtable (must be first)
    const void** m_eventVptr;      // +4 - the ECRenderer listener, 0x1020D16C (the handler's `add ecx, -4`)
    int          m_refCount;
    void*        m_unusedLexicon; // PE +0x0C; only the unreferenced buildLexiconCharacters fills it
    std::map<std::string, DifFont*> m_cache;
};

// PE 0x100A10A0: the two vptrs, refcount 1, the empty cache, then init().
FontSystem::FontSystem()
    : m_vptr(fontSystemVtable()), m_eventVptr(fontSystemEventVtable()),
      m_refCount(1), m_unusedLexicon(0)
{
    init();
}

// PE 0x100A0A80: clear() inlined (each cached font's detachDevice 0x100A5290 and free, then the
// tree erase 0x1009FF20 and head reset), then registerEventHandler(11, listener, 0) through
// EventManager +0x2C; returns true, which the constructor discards. The EventManager is dereferenced unguarded, as shipped - it is one of the
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
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
        if (it->second)                                               // `test edi, edi; je`
            it->second->release();
    m_cache.clear();
}


// PE 0x100A1060
void RD9_THUNK FontSystem::handleRendererEvent(void* listener RD9_THUNK_EDX, int category, unsigned int id,
                                                void* /*node*/, void* data)
{
#if defined(_M_IX86)
    static_assert(offsetof(FontSystem, m_eventVptr) == 4,
                  "the listener subobject sits at +4 - the slot's `add ecx, -4` depends on it");
#endif
    if (category != 11)                                                // `cmp [ebp+8], 0Bh`
        return;
    if (id == 0 || id > 2)                                             // `test; jbe` / `cmp 2; ja`
        return;
    // The payload is dereferenced with no null test, as shipped.
    const unsigned int packed = *static_cast<const unsigned int*>(data);
    FontSystem* const self = reinterpret_cast<FontSystem*>(
        static_cast<unsigned char*>(listener) - offsetof(FontSystem, m_eventVptr));   // `add ecx, -4`
    self->reloadFonts(static_cast<int>(packed >> 16),                  // `shr eax, 10h`, pushed last
                      static_cast<int>(packed & 0xFFFFu));             // `and edx, 0FFFFh`
}

// sub_100A0F10 (329 B, `retn 8`). The keys are snapshotted (the shipped std::vector<std::string>,
// map order), every font destroyed (see clear()'s ) and the cache emptied, then every key is loaded
// again through the loader for the resized menu viewport. The answer is true only
// when every reload succeeded; the one caller discards it.
bool FontSystem::reloadFonts(int width, int height)
{
    const auto viewport = ui::MenuViewport::fit(width, height);
    if (!viewport.width || !viewport.height)
        return false;
    std::vector<std::string> keys;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
    {
        keys.push_back(it->first);
        if (it->second)
            it->second->release();                                    // existing counted native ownership
    }
    m_cache.clear();                                                   // sub_1009FF20 + head reset

    bool ok = true;                                                    // [ebp-1] = 1
    for (const std::string& key : keys)
        if (!loadFont(key.c_str(), viewport))
            ok = false;
    return ok;
}

// PE 0x100A09C0
DifFont* FontSystem::getFont(const char* name)
{
    DifFont* result;
    {
        std::string key(name);
        dx9rend::pathToLower(key);
        const auto found = m_cache.find(key);
        if (found != m_cache.end())
            result = found->second;
        else
        {
            const std::string requested(name);
            const int width = dx9rend::readIntSetting("GSWindowSizeX", 800);
            const int height = dx9rend::readIntSetting("GSWindowSizeY", 600);
            result = loadFont(requested.c_str(), ui::MenuViewport::fit(width, height));
        }
    }
    return result;
}

// Collects every character the current language's lexicon files use and stores them, ascending
// and terminated by 0xFFFF, as a new bf_malloc'd table in +0x0C (the previous table is freed; with
// no character the slot stays null). The language comes from GSLanguage, "English" when empty;
// every non-empty file under "localization/<language>/", subdirectories included, is read.
// Returns the table.
// PE 0x100A0C40. unreachable in 1.50: no code or data references. [INFERRED] Name.
unsigned short* FontSystem::buildLexiconCharacters()
{
    std::map<unsigned short, bool> characters;
    std::string language = ::dice::hfe::settings->stringGet(std::string("GSLanguage"), std::string(""));
    if (language == "")
        language = "English";
    std::string pattern;
    {
        const std::string directory = std::string("localization/") + language;
        pattern = directory + "/";
    }
    std::list< ::dice::hfe::io::FileInfo> files;
    ::dice::hfe::fileManager->findFiles(pattern, true, files);                 // +0x34
    for (std::list< ::dice::hfe::io::FileInfo>::iterator it = files.begin(); it != files.end(); ++it)
    {
        if (it->size != 0 && !it->isDirectory)
        {
            const std::string path = pattern + it->name;
            readLexiconCharacters(path, characters);
        }
    }
    if (m_unusedLexicon)
        ::renddx9abi::engineFree(m_unusedLexicon);
    m_unusedLexicon = 0;
    if (!characters.empty())
    {
        unsigned short* const table = static_cast<unsigned short*>(
            ::renddx9abi::engineAlloc(static_cast<unsigned int>(characters.size()) * 2u + 2u));
        m_unusedLexicon = table;
        unsigned int count = 0;
        for (std::map<unsigned short, bool>::const_iterator it = characters.begin();
             it != characters.end(); ++it)
            static_cast<unsigned short*>(m_unusedLexicon)[count++] = it->first;
        static_cast<unsigned short*>(m_unusedLexicon)[count] = 0xFFFF;
    }
    return static_cast<unsigned short*>(m_unusedLexicon);
}

// Reads one UTF-16LE lexicon file (FF FE fingerprint) of key and value lines, each terminated by
// 0x1B and followed by one skipped unit, and records every value character other than CR and LF
// (nonbreaking space as space). Whitespace between records is skipped. Returns false when the
// file cannot be opened, its fingerprint is missing or wrong (both reported as errors), or a seek
// fails.
// PE 0x100A0030. unreachable in 1.50: its only caller is buildLexiconCharacters. [INFERRED] Name.
bool FontSystem::readLexiconCharacters(const std::string& path,
                                       std::map<unsigned short, bool>& characters)
{
    static const char* const kFile =
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\FontSystem\\FontSystem.cpp";
    ::renddx9abi::EngineStream stream;
    if (!stream.open(path, 2, 0, 0))
        return false;
    unsigned char fingerprint[2];
    if (!stream.read(fingerprint, 2))
    {
        ::renddx9abi::Debug(DebugTypeError, kFile, 112, "")
            << "while reading lexicon file '" << path << "': couldn't read fingerprint";
        return false;
    }
    if (fingerprint[0] != 0xFF || fingerprint[1] != 0xFE)
    {
        ::renddx9abi::Debug(DebugTypeError, kFile, 117, "")
            << "while reading lexicon file '" << path << "': bad unicode fingerprint";
        return false;
    }
    while (stream.getAvailableBytes() != 0)
    {
        std::u16string key;
        std::u16string value;
        // The shipped seek tests read only al.
        if (!stream.readWideLine(key, 0x1B) || !static_cast<unsigned char>(stream.seek(1, 2))
            || !stream.readWideLine(value, 0x1B))
            return false;
        if (value.size() == 0)
        {
            // Shipped dead store: the converted key is never read.
            char narrowKey[0x104];
#if defined(_WIN32)
            ::wcstombs(narrowKey, reinterpret_cast<const wchar_t*>(key.c_str()), 0x104);
#else
            // [PLATFORM] wchar_t is 32-bit here; the 16-bit units are widened for the CRT call.
            const std::wstring wideKey(key.begin(), key.end());
            ::wcstombs(narrowKey, wideKey.c_str(), 0x104);
#endif
        }
        if (!static_cast<unsigned char>(stream.seek(1, 2)))
            return false;
        for (unsigned int i = 0; i < value.size(); ++i)
        {
            if (value[i] == 0x0A || value[i] == 0x0D)
                continue;
            if (value[i] == 0xA0)
                value[i] = 0x20;
            const unsigned short character = static_cast<unsigned short>(value[i]);
            if (characters.find(character) == characters.end())
                characters.insert(std::make_pair(character, true));
        }
        unsigned short unit;
        while (stream.read(&unit, 2))
        {
            if (unit != 0x0A && unit != 0x0D && unit != 0x20 && unit != 0x09)
                break;
        }
        if (stream.getAvailableBytes() != 0)
            stream.seek(1, -2);
    }
    return true;
}

// PE 0x100A0350
DifFont* FontSystem::createLoadedFont(std::string directory, const char* key, const ui::MenuViewport& viewport)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(DifFont));
    DifFont* font = 0;
    if (storage)
    {
        const std::string name(key);
        font = new (storage) DifFont(directory, name);
    }
    if (!font)
        return 0;
    if (!font->load(viewport))
    {
        font->release();
        return 0;
    }
    std::string cacheKey(key);
    dx9rend::pathToLower(cacheKey);
    m_cache.insert(std::make_pair(cacheKey, font));
    return font;
}

// Select the font assets and scale using the drawable width of the Flash menu.
DifFont* FontSystem::loadFont(const char* key, const ui::MenuViewport& viewport)
{
    const unsigned res = viewport.width;
    float reference = 800.0f;
    const char* resFolder = "";
    if (res == 800)
        resFolder = "800";
    else if (res >= 1024)
        reference = 1024.0f;
    const float fontScale = static_cast<float>(res) / reference;

    std::string langStr;
    ::dice::hfe::dx9rend::readStringSetting("GSLanguage", langStr);
    const char* lang = (!langStr.empty()) ? langStr.c_str() : "english";

    // Searches the language-specific font directory before the common resolution directory.
    // [CHOSEN, not recovered] Normalizes repeated path separators.
    char cand[2][384];
    unsigned int nc = 0;
    if (resFolder[0])
    {
        ::_snprintf_s(cand[nc++], sizeof(cand[0]), _TRUNCATE, "Fonts/%s/%s/", lang, resFolder);
        ::_snprintf_s(cand[nc++], sizeof(cand[0]), _TRUNCATE, "Fonts/%s/", resFolder);
    }
    else
    {
        ::_snprintf_s(cand[nc++], sizeof(cand[0]), _TRUNCATE, "Fonts/%s/", lang);
        ::_snprintf_s(cand[nc++], sizeof(cand[0]), _TRUNCATE, "Fonts/");
    }

    DifFont* font = 0;
    for (unsigned int i = 0; i < nc; ++i)
    {
        if (DifFont* const f = createLoadedFont(std::string(cand[i]), key, viewport))
        {
            font = f;
            break;
        }
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
    font->hidpiUpgrade();                       // [CHOSEN, not recovered] HiDPI menu text

    return font;
}

// PE 0x100A0BD0: both vptrs, unregister from category 11 (EventManager +0x30), free +0x0C, clear
// (0x100A0B30, the [CHOSEN] counted form below), then the tree erase and head reset.
FontSystem::~FontSystem()
{
    m_vptr = fontSystemVtable();
    m_eventVptr = fontSystemEventVtable();
    ::dice::hfe::eventManager->unregisterEventHandler(
        static_cast<EventCategory>(11), reinterpret_cast<IEventListener*>(&m_eventVptr));
    if (m_unusedLexicon)
        ::renddx9abi::engineFree(m_unusedLexicon);
    // [CHOSEN, not recovered] Keep persistent wrappers' counted native ownership.
    // A live wrapper retains the font object while its device half is detached.
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
        if (DifFont* const font = it->second)
            if (font->release() > 0)
                font->detachDevice();
    m_cache.clear();
}

int FontSystem::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        this->~FontSystem();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// PE 0x100A11C0
void* FontSystem::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IFontSystem)
        return const_cast<FontSystem*>(this);
    return 0;
}

namespace {
void  RD9_THUNK fs_addRef(void* s RD9_THUNK_EDX)  { static_cast<FontSystem*>(s)->addRef(); }
int   RD9_THUNK fs_getRef(void* s RD9_THUNK_EDX)  { return static_cast<FontSystem*>(s)->getRef(); }
int   RD9_THUNK fs_release(void* s RD9_THUNK_EDX) { return static_cast<FontSystem*>(s)->release(); }
void* RD9_THUNK fs_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
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
    // [CHOSEN, not recovered] Preserve PE 0x10152F26's %d spelling without
    // restoring sprintf's fixed-buffer overflow for long font names.
    char number[32];
    ::_snprintf_s(number, sizeof(number), _TRUNCATE, "%d", size);
    std::string key(name ? name : "");
    key += '_';
    key += number;
    DifFont* font = fs->getFont(key.c_str());
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

void swiffFontRefreshWindow(void* font)
{
    static_cast<SwiffFontObject*>(font)->refreshWindow();
}

// SwiffRender slot 27 (0x10154870) deletes a wrapper: its destructor (0x100A42F0), then the engine
// free (0x1005F360). The caller has tested the pointer.
// [CHOSEN, not recovered] This wrapper holds a reference on its DifFont (the shipped constructor
// 0x100A4E20 takes none), so the delete returns it; the shipped destructor frees the wrapper's
// glyph vector and text string; this layout uses a native vector and bounded inline text.
void SwiffFontObject::destroy()
{
    if (m_font)
        m_font->release();
    m_font = 0;
    this->~SwiffFontObject();
    ::renddx9abi::engineFree(this);
}

void swiffFontDelete(void* font)
{
    static_cast<SwiffFontObject*>(font)->destroy();
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
