// Bitmap font loading, measurement and drawing. [INFERRED] Source placement.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include "../../RendDX9/RendDX9Base.h"       // textureManagerGetTexture / SwapManaged
#include "../../RendDX9/ShaderParameter.h"   // the TEXLAYER0 handle
#include "../../RendDX9/Texture.h"           // EngineTexture, textureReleaseRef
#include "../../RendMisc/IRenderView.h"      // the viewport the draw maps pixels through
#include "../../Math/Vec3.h"                 // Vec2 - the scale pair (+0x28/+0x2C)
#include <d3dx9.h>                            // HRESULT, RECT, D3DCOLOR, STDMETHODCALLTYPE
#include <cstdio>
#include <cstdlib>

#include <stdio.h>
#include <string.h>

namespace dice {
namespace hfe {

namespace dx9rend {
// The BufferManager / Effect / renderer seams the shared-resource block and the draw call. All of
// them already exist in the tree (BufferManager.cpp, ShaderManager.cpp, StateManager.cpp) except
// the three marked NEW, whose bodies the dc_console report gives for BufferManager.cpp / Effect.cpp.
extern void* g_shaderManager;                                       // dword_1023B97C
extern void* g_rendererBase;                                        // dword_1023B970 (EditorFont)
int   bufferManagerSwapPool(int value);                             // sub_100113B0 - BufferManager+0x08
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);   // sub_10011FF0
void  vertexBufferAddRef(void* wrapper);                            // NEW - the wrapper's vslot +0x08
void  vertexBufferRelease(void* wrapper);                           // the inline --ref, sub_10014510, free
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);                         // sub_10014630
bool  vertexBufferUnlock(void* wrapper);                            // sub_10014690
void  vertexBufferBindStream(void* wrapper, unsigned int stream);   // sub_100146C0
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);              // sub_10005870 on dword_1023B970
void  effectBeginCurrentTechniqueStates(void* effect);              // NEW - sub_1001DB60
void  effectEndCurrentTechniqueStates(void* effect);                // NEW - sub_1001DB70
} // namespace dx9rend

namespace geom {

// From ModuleGeomMisc's class list: createInstance(81007, 80001) in
// BF2Render_initializeFontAndConsoleOverlay, and the classId read as data at 0x1007D7D5.
extern const unsigned int IID_IFontPorted = 80001u;
extern const unsigned int CID_FontPorted  = 81007u;


// Stores UV coordinates and width for 256 glyphs.
// BufferManager and TextureManager handle GPU resets; this class is not a device listener.

struct Glyph
{
    float u;      // +0  texel u, already biased by half a texel
    float v;      // +4  texel v, likewise
    float width;  // +8  advance, in texels
};

// One vertex of the shared "PortedFontRend" buffer - 28 bytes, the stride createVertexBuffer is given
// (0x1C) and exactly the declaration PortedFontShader.fx's "Text" technique annotates:
// FLOAT4 position, D3DCOLOR color, FLOAT2 TEXCOORD0. The position is already in clip space (w = 1) -
// the vertex shader HPosVS passes it through untouched.
struct PortedFontVertex
{
    float        x, y, z, w;   // +0x00
    unsigned int colour;       // +0x10
    float        u, v;         // +0x14
};

static_assert(sizeof(PortedFontVertex) == 28, "createVertexBuffer(6144, 28, ...) - the stride");

class FontPorted : public IBase
{
public:
    FontPorted();
    ~FontPorted();                                          // not a slot; release() calls it

    virtual void  addRef();                                 // +0x00  0x1000CB20 (folded)
    virtual int   getRef();                                 // +0x04  0x101A9A10 (folded)
    virtual int   release();                                // +0x08
    virtual void* queryInterface(unsigned int iid) const;   // +0x0C

    virtual bool load(const std::string& path);            // +0x10  0x100876A0   read
    // Separate draw names preserve virtual slot order; MSVC may regroup same-named virtual overloads.
    virtual void drawTextColoured(float x, float y, const std::string& text,
                                  unsigned int colour);     // +0x14  0x10087A90   written
    virtual void drawText(float x, float y, const std::string& text);   // +0x18  0x10087130  written
    virtual int  getHeight();                               // +0x1C  0x10087150   written
    virtual int  getCharacterWidth(unsigned char character); // +0x20  0x100871B0   written
    virtual int  getTextWidth(const std::string& text);     // +0x24  0x10087160   written
    // The text scale at +0x08/+0x0C. [INFERRED] names and spellings: no caller in either image;
    // the setter takes two dwords (`retn 8`), the getter returns the pair through the hidden
    // return pointer (`retn 4`), as OverlayTexture's size/position getters do.
    virtual void setScale(float x, float y);                // +0x28  0x100872E0
    virtual Vec2 getScale() const;                          // +0x2C  0x10087300
    virtual void setColour(unsigned colour);                // +0x30  0x10087200   written
    virtual unsigned getColour();                           // +0x34  0x10087210   the last slot

    // , `retn 10h` - not a vtable slot. Its only caller in the image is
    // NameTagRenderer::bufferItem (0x101B2467), which blits a player's name into the name texture.
    float drawToTexture(dx9rend::EngineTexture* target, POINT* position,
                        const std::string& text, bool fromSystemMemory);

private:
    int           m_refCount;      // +0x004  ctor: 1
    float         m_scaleX;        // +0x008  ctor: 1.0 - the text draw multiplies every advance by it
    float         m_scaleY;        // +0x00C  ctor: 1.0 - ... and the line height by this one
    // +0x010: the constructor writes integer 0 (== 0.0f); the text draw stores it as the Z of every
    // vertex it emits (`fld [esi+10h]` -> vertex +0x08). Renamed from m_zero10 on that proof.
    float         m_depth;         // +0x010  ctor: 0
    unsigned int  m_colour;        // +0x014  ctor: 0xFF000000 - opaque black, as DebugTextWriter's;
                                   //         the colour +0x18 hands to +0x14

    Glyph         m_glyphs[256];   // +0x018 .. +0xC17

    int           m_height;        // +0xC18  "Height = %d"
    int           m_betweenWidth;  // +0xC1C  "BetweenWidth = %d"
    int           m_spaceWidth;    // +0xC20  "SpaceWidth = %d"
    unsigned char m_alphaTest;     // +0xC24  "AlphaTest = %d"   != 0
    unsigned char m_alphaBlend;    // +0xC25  "AlphaBlend = %d"  != 0
    unsigned char m_padC26[2];
    int           m_textureWidth;  // +0xC28  "TextureWidth = %d"
    int           m_textureHeight; // +0xC2C  "TextureHeight = %d"
    std::string  m_textureName;   // +0xC30  the "Texture = %s" value
    void*         m_texture;       // +0xC4C  loaded from m_textureName
    void*         m_textureDef;    // +0xC50  loaded from m_textureName + "_def"
    void*         m_textureSys; // System-memory texture cache; loaded on first use and released before the other textures.
};




// The first font creates shared draw resources; later instances add references.
// Each destructor releases its references without clearing the shared cache pointers.

namespace {

void*                     s_portedFontVertexBuffer = 0;   // dword_10249C74
dx9rend::Effect*          s_portedFontEffect       = 0;   // dword_10249C78
dx9rend::ShaderParameter* s_portedFontTexLayer0    = 0;   // dword_10249C7C

// The vertex store the text draw performs six times per glyph; w is always the constant 1.0f
// (`mov edi, 3F800000h` held across the whole loop).
inline void writePortedFontVertex(PortedFontVertex& out, float x, float y, float z,
                                  unsigned int colour, float u, float v)
{
    out.x = x;
    out.y = y;
    out.z = z;
    out.w = 1.0f;
    out.colour = colour;
    out.u = u;
    out.v = v;
}

} // anonymous namespace


// Clears glyph UV coordinates; widths are populated when the font loads.
// [CHOSEN, not recovered] Initial metrics are zeroed instead of left uninitialized.

FontPorted::FontPorted()
    : m_refCount(1)
    , m_scaleX(1.0f)
    , m_scaleY(1.0f)
    , m_depth(0.0f)
    , m_colour(0xFF000000u)
    , m_height(0)
    , m_betweenWidth(0)
    , m_spaceWidth(0)
    , m_alphaTest(0)
    , m_alphaBlend(0)
    , m_textureWidth(0)
    , m_textureHeight(0)
    , m_texture(0)
    , m_textureDef(0)
    , m_textureSys(0)
{
    m_padC26[0] = 0;
    m_padC26[1] = 0;

    for (int i = 0; i < 256; ++i)
    {
        m_glyphs[i].u = 0.0f;
        m_glyphs[i].v = 0.0f;
        // .width deliberately not written - see the note above.
    }

    // Creates or retains the shared dynamic vertex buffer, effect and parameter.
    // Only the vertex buffer guards initialization; a failed acquisition can cause other resources to be reacquired.
    if (!s_portedFontVertexBuffer)
    {
        const int savedPool = dx9rend::bufferManagerSwapPool(0);            // D3DPOOL_DEFAULT
        s_portedFontVertexBuffer =
            dx9rend::bufferManagerCreateVertexBuffer(6144, 28, 0x208, "PortedFontRend");
        dx9rend::bufferManagerSwapPool(savedPool);

        s_portedFontEffect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
            dx9rend::g_shaderManager, "mods/xpack/Shaders/PortedFontShader"));
        s_portedFontTexLayer0 =
            dx9rend::effectGetShaderParameterTyped(s_portedFontEffect, std::string("TEXLAYER0"));

        // [CHOSEN] diagnostic, not in retail (once, on the first construction).
        ::renddx9abi::logf("Font::Ported: shared draw resources created - PortedFontRend 0x%p, "
                           "PortedFontShader 0x%p, TEXLAYER0 0x%p",
                           s_portedFontVertexBuffer,
                           static_cast<const void*>(s_portedFontEffect),
                           static_cast<const void*>(s_portedFontTexLayer0));
    }
    else
    {
        dx9rend::vertexBufferAddRef(s_portedFontVertexBuffer);              // wrapper vslot +0x08
        dx9rend::effectAddRef(s_portedFontEffect);                          // Effect vslot +0x00
        s_portedFontTexLayer0->addRef();                                    // ShaderParameter +0x04
    }
}


// Releases the three textures and shared draw resources.
// Cached global pointers remain intact because other owners may retain the resources.

FontPorted::~FontPorted()
{
    dx9rend::EngineTexture* texture = static_cast<dx9rend::EngineTexture*>(m_textureSys);
    dx9rend::textureReleaseRef(texture);                                    // +0xC54
    texture = static_cast<dx9rend::EngineTexture*>(m_texture);
    dx9rend::textureReleaseRef(texture);                                    // +0xC4C
    texture = static_cast<dx9rend::EngineTexture*>(m_textureDef);
    dx9rend::textureReleaseRef(texture);                                    // +0xC50

    dx9rend::vertexBufferRelease(s_portedFontVertexBuffer);                 // dword_10249C74
    s_portedFontTexLayer0->release();                                       // dword_10249C7C, +0x08
    dx9rend::effectRelease(s_portedFontEffect);                             // dword_10249C78, +0x08
}

void FontPorted::addRef() { ++m_refCount; }
int  FontPorted::getRef() { return m_refCount; }

// release - 0x10087E30: `--[this+4]`; at zero the destructor 0x10087220 and then sub_1005F360 (the
// engine free); returns the decremented count either way.
// PE 0x10087E30
int FontPorted::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~FontPorted();
        ::renddx9abi::engineFree(this);                                      // sub_1005F360
    }
    return remaining;
}

// PE 0x10088030: IDs 1 and 80001 (word 0x1020AE20) answer the receiver, everything else null; ret 4.
void* FontPorted::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IFontPorted)
        return const_cast<FontPorted*>(this);
    return 0;
}

// Loads font metrics and the normal/default-pool textures through the host filesystem.
// Glyph widths remain in texels; UVs are normalized and biased to texel centres.
// Returns success without reloading once the default-pool texture exists.
bool FontPorted::load(const std::string& path)
{
    RD9_TRACE("dice::hfe::geom::Font::Ported::load", 0x10);

    if (m_textureDef)
        return true;   // the original's `if (!this[3152])` guard - a reload is a no-op

    ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile(path.c_str());
    if (!file.ok())
    {
        ::renddx9abi::logf("Font::Ported::load(\"%s\"): the FileManager could not open it - this is "
                           "the original's file-open-failure path, returning false",
                           path.c_str());
        return false;
    }

    // Clear every glyph so characters absent from the font file have zero advance.
    for (int i = 0; i < 256; ++i)
    {
        m_glyphs[i].u = 0.0f;
        m_glyphs[i].v = 0.0f;
        m_glyphs[i].width = 0.0f;
    }

    // The parse. Plain text, line by line, exactly as documented above. The buffer is not
    // null-terminated, so the line splitter works on an explicit end pointer.
    const char* cur = reinterpret_cast<const char*>(file.data);
    const char* end = cur + file.size;

    std::string textureName;
    int glyphCount = 0;

    while (cur < end)
    {
        const char* lineEnd = cur;
        while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r')
            ++lineEnd;

        char line[256];
        unsigned len = static_cast<unsigned>(lineEnd - cur);
        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        for (unsigned i = 0; i < len; ++i)
            line[i] = cur[i];
        line[len] = '\0';

        cur = lineEnd;
        while (cur < end && (*cur == '\n' || *cur == '\r'))
            ++cur;

        if (len == 0)
            continue;

        char nameBuf[128];
        int  value = 0;

        // The keyed lines. The original reads each with its own comparison; sscanf with " = "
        // reproduces the accepted spellings without inventing tolerance the original lacks.
        if (::sscanf(line, "Texture = %127s", nameBuf) == 1)          { textureName = nameBuf; continue; }
        if (::sscanf(line, "TextureWidth = %d", &value) == 1)         { m_textureWidth  = value; continue; }
        if (::sscanf(line, "TextureHeight = %d", &value) == 1)        { m_textureHeight = value; continue; }
        if (::sscanf(line, "BetweenWidth = %d", &value) == 1)         { m_betweenWidth  = value; continue; }
        if (::sscanf(line, "SpaceWidth = %d", &value) == 1)           { m_spaceWidth    = value; continue; }
        if (::sscanf(line, "Height = %d", &value) == 1)               { m_height        = value; continue; }
        if (::sscanf(line, "AlphaTest = %d", &value) == 1)            { m_alphaTest     = value ? 1 : 0; continue; }
        if (::sscanf(line, "AlphaBlend = %d", &value) == 1)           { m_alphaBlend    = value ? 1 : 0; continue; }

        // A glyph line: "%c %f %f %f". The original stops at the first line that does not yield
        // four fields; here the keyed lines are already consumed above, so a failure just skips.
        char  ch = 0;
        float left = 0.0f, top = 0.0f, right = 0.0f;
        if (::sscanf(line, "%c %f %f %f", &ch, &left, &top, &right) == 4)
        {
            const unsigned char index = static_cast<unsigned char>(ch);

            // the texel-centre bias and the asymmetry. u and v are normalised with a half-texel
            // offset; the width stays in texels. Getting either wrong misplaces every glyph.
            if (m_textureWidth > 0 && m_textureHeight > 0)
            {
                m_glyphs[index].u = (0.5f / m_textureWidth) + left * (1.0f / m_textureWidth);
                m_glyphs[index].v = (0.5f / m_textureHeight) + top * (1.0f / m_textureHeight);
            }
            m_glyphs[index].width = right - left + 1.0f;
            ++glyphCount;
        }
    }

    ::renddx9abi::freeEngineFile(file);

    // The shipped tail: the two texture loads
    // `std::string::operator=(this + 3120, v35)` and everything after, from 0x100876A0.
    m_textureName = textureName;

    // The release-if-present pattern - `if (v && !--*((_DWORD*)v + 1)) { sub_10014E20; free }` -
    // on both slots. Unreachable while the m_textureDef guard above holds, reproduced for shape.
    {
        dx9rend::EngineTexture* old = static_cast<dx9rend::EngineTexture*>(m_texture);
        dx9rend::textureReleaseRef(old);
        m_texture = 0;
        old = static_cast<dx9rend::EngineTexture*>(m_textureDef);
        dx9rend::textureReleaseRef(old);
        m_textureDef = 0;
    }

    m_texture = dx9rend::textureManagerGetTexture(m_textureName);

    // sub_10017E20(0) around the "_def" load only: the def texture is created in D3DPOOL_DEFAULT.
    const int savedManaged = dx9rend::textureManagerSwapManaged(0);
    std::string defName(m_textureName);
    defName += "_def";
    m_textureDef = dx9rend::textureManagerGetTexture(defName);
    dx9rend::textureManagerSwapManaged(savedManaged);

    ::renddx9abi::logf("Font::Ported::load(\"%s\"): PARSED - texture=\"%s\" %dx%d, height=%d "
                       "between=%d space=%d alphaTest=%d alphaBlend=%d, %d glyphs; textures "
                       "0x%p and (def, DEFAULT pool) 0x%p",
                       path.c_str(), textureName.c_str(), m_textureWidth, m_textureHeight,
                       m_height, m_betweenWidth, m_spaceWidth, m_alphaTest, m_alphaBlend,
                       glyphCount, m_texture, m_textureDef);
    return true;
}

// Draws coloured text at pixel coordinates as one batch of non-space glyph quads.
// [INFERRED] method name. The text must fit the shared 1024-glyph buffer; no bounds check is made.
// Coordinates ignore the viewport origin and use no half-pixel offset. Lock/unlock failure
// Abandons the draw. Float intermediates depend on the renderer's 24-bit precision (RW-240).
void FontPorted::drawTextColoured(float x, float y, const std::string& text, unsigned int colour)
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();
    const float inverseWidth = 1.0f / view->getViewport()[2];
    const float clipScaleX = inverseWidth + inverseWidth;
    const float inverseHeight = 1.0f / view->getViewport()[3];               // fetched again
    const float clipScaleY = inverseHeight + inverseHeight;

    const int length = static_cast<int>(text.size());
    if (length <= 0)
        return;

    PortedFontVertex* vertex = static_cast<PortedFontVertex*>(
        dx9rend::vertexBufferLock(s_portedFontVertexBuffer, 0, 0, D3DLOCK_DISCARD));
    if (!vertex)
        return;
    ::memset(vertex, 0, static_cast<unsigned int>(length) * (6u * sizeof(PortedFontVertex)));

    int glyphCount = 0;
    for (int i = 0; i < length; ++i)
    {
        float advance;
        if (text.c_str()[i] == ' ')
        {
            advance = static_cast<float>(m_spaceWidth);                       // +0xC20
        }
        else
        {
            const Glyph& glyph = m_glyphs[static_cast<unsigned char>(text.c_str()[i])];
            const float width = glyph.width;
            const float u1 = glyph.u + width / static_cast<float>(m_textureWidth);
            const float lineHeight = static_cast<float>(m_height);
            const float v1 = glyph.v + lineHeight / static_cast<float>(m_textureHeight);
            const float x1 = x + width * m_scaleX;
            const float y1 = y + lineHeight * m_scaleY;

            const float left   = clipScaleX * x - 1.0f;
            const float top    = -(clipScaleY * y - 1.0f);
            const float right  = x1 * clipScaleX - 1.0f;
            const float bottom = -(y1 * clipScaleY - 1.0f);

            ++glyphCount;
            writePortedFontVertex(vertex[0], left,  top,    m_depth, colour, glyph.u, glyph.v);
            writePortedFontVertex(vertex[1], right, top,    m_depth, colour, u1,      glyph.v);
            writePortedFontVertex(vertex[2], right, bottom, m_depth, colour, u1,      v1);
            writePortedFontVertex(vertex[3], left,  top,    m_depth, colour, glyph.u, glyph.v);
            writePortedFontVertex(vertex[4], right, bottom, m_depth, colour, u1,      v1);
            writePortedFontVertex(vertex[5], left,  bottom, m_depth, colour, glyph.u, v1);
            vertex += 6;

            advance = static_cast<float>(m_betweenWidth) + width;             // +0xC1C
        }
        x = advance * m_scaleX + x;
    }

    if (!dx9rend::vertexBufferUnlock(s_portedFontVertexBuffer))
        return;
    if (!glyphCount)
        return;

    dx9rend::effectSetTechniqueByName(s_portedFontEffect, std::string("Text"));
    dx9rend::effectBeginCurrentTechniqueStates(s_portedFontEffect);         // "Text_States"
    s_portedFontTexLayer0->setValue(m_texture, 0);                          // +0xC4C
    dx9rend::effectBegin(s_portedFontEffect);
    dx9rend::effectBeginPass(s_portedFontEffect, 0);
    dx9rend::vertexBufferBindStream(s_portedFontVertexBuffer, 0);
    dx9rend::rendererDrawPrimitive(static_cast<unsigned int>(glyphCount * 2),
                                   D3DPT_TRIANGLELIST, 0);
    dx9rend::effectEnd(s_portedFontEffect);
    dx9rend::effectEndCurrentTechniqueStates(s_portedFontEffect);
}

// Draws text through the colored virtual overload using the current default color.
void FontPorted::drawText(float x, float y, const std::string& text)
{
    drawTextColoured(x, y, text, m_colour);
}

// +0x1C getHeight, PE 0x10087150 (7 B): `mov eax, [ecx+0C18h]; ret` - the parsed "Height = %d".
int FontPorted::getHeight()
{
    return m_height;
}

// +0x20, PE 0x100871B0: a space returns SpaceWidth (+0xC20); any other byte is its glyph width
// (+0x20 + 12 * c), truncated by the fistp under a chop control word, plus BetweenWidth (+0xC1C).
int FontPorted::getCharacterWidth(unsigned char character)
{
    return character == ' ' ? m_spaceWidth : static_cast<int>(m_glyphs[character].width) + m_betweenWidth;
}

// +0x24, PE 0x10087160: the sum of the +0x20 widths, each byte through the virtual slot.
int FontPorted::getTextWidth(const std::string& text)
{
    int width = 0;
    for (unsigned i = 0; i < text.size(); ++i)
        width += getCharacterWidth(static_cast<unsigned char>(text.c_str()[i]));
    return width;
}

// +0x30 (PE 0x10087200) and +0x34 (PE 0x10087210): the default colour at +0x14.
void FontPorted::setColour(unsigned colour) { m_colour = colour; }
unsigned FontPorted::getColour() { return m_colour; }

// +0x28, PE 0x100872E0: store both words, x to +0x08 and y to +0x0C.
void FontPorted::setScale(float x, float y)
{
    m_scaleX = x;
    m_scaleY = y;
}

// +0x2C, PE 0x10087300: copy {+0x08, +0x0C} out through the hidden return pointer.
Vec2 FontPorted::getScale() const
{
    Vec2 scale;
    scale.x = m_scaleX;
    scale.y = m_scaleY;
    return scale;
}

// RefFactory_geom_Font_Ported, 0x1007C7C0. Same shape and __fastcall convention as the others.
IBase* __fastcall createFontPorted(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    FontPorted* self = new FontPorted();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

// DebugTextWriter's constructor builds one of these directly rather than through the registry.
IBase* createFontPortedDirect()
{
    return new FontPorted();
}


// =================================================================================================
// EditorFont - "dice.hfe.geom.Font.Editor" (getClassList 0x1007D811, class id dword_1020AF20 =
// 81043, factory 0x1007C810), the IFont (IID 80001) implementation on a D3DX font that the editor
// uses. [INFERRED] class name from the registry name; [INFERRED] placement in this TU from its
// bodies (0x10087EA0..0x100880B0) directly following FontPorted's; it has no Debug emitter, so no
// __FILE__ string attests it. No caller exists in any inspected image (BF2.exe references neither
// the class id nor the name); it is reached only through the class registry.
// Layout (retail 0x10 bytes): +0x00 IBase vptr (0x1020B0B4), +0x04 device-listener vptr
// (0x1020B0A8), +0x08 reference count, +0x0C the D3DX font.
// =================================================================================================

// The ID3DXFont the editor font draws through, in d3dx9core.h (April 2005) slot order; EditorFont
// calls Release (+0x08), DrawTextA (+0x38), OnLostDevice (+0x40) and OnResetDevice (+0x44).
// [CHOSEN, not recovered] local name: the D3DX headers of the native backends declare no ID3DXFont.
struct EditorFontD3DX
{
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(const void* iid, void** object) = 0;
    virtual ULONG   STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG   STDMETHODCALLTYPE Release() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** device) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDescA(void* desc) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDescW(void* desc) = 0;
    virtual BOOL    STDMETHODCALLTYPE GetTextMetricsA(void* metrics) = 0;
    virtual BOOL    STDMETHODCALLTYPE GetTextMetricsW(void* metrics) = 0;
    virtual void*   STDMETHODCALLTYPE GetDC() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGlyphData(UINT glyph, void** texture, RECT* blackBox,
                                                   POINT* cellInc) = 0;
    virtual HRESULT STDMETHODCALLTYPE PreloadCharacters(UINT first, UINT last) = 0;
    virtual HRESULT STDMETHODCALLTYPE PreloadGlyphs(UINT first, UINT last) = 0;
    virtual HRESULT STDMETHODCALLTYPE PreloadTextA(const char* text, INT count) = 0;
    virtual HRESULT STDMETHODCALLTYPE PreloadTextW(const wchar_t* text, INT count) = 0;
    virtual INT     STDMETHODCALLTYPE DrawTextA(void* sprite, const char* text, INT count,
                                                RECT* rect, DWORD format, D3DCOLOR colour) = 0;
    virtual INT     STDMETHODCALLTYPE DrawTextW(void* sprite, const wchar_t* text, INT count,
                                                RECT* rect, DWORD format, D3DCOLOR colour) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnLostDevice() = 0;
    virtual HRESULT STDMETHODCALLTYPE OnResetDevice() = 0;
};

// The D3DXCreateFontA import PE 0x10087E87 calls (d3dx9_25.dll): device, height 12, width 8,
// weight 400, mip levels 0, italic 0, charset 0, output precision 4, quality 4, pitch and family 0,
// "Arial", out. [FRONTIER] [TEMP] [CHOSEN, not recovered]: no backend of this tree provides the
// D3DX font, so the call reports and stops. Missing pieces (reviewed in round 16): (1) a system
// TrueType rasterizer for "Arial" at 12x8, weight 400 - the native backends have no GDI and the tree
// uses neither CoreText nor FreeType anywhere; (2) an ID3DXFont object in the Metal D3DX subset
// (Platform/Gfx/Metal/D3DX*, D3DX9Subset.h) with a glyph texture and a sprite-less DrawTextA quad
// path that saves and restores device state as D3DXLine.cpp does, honouring DT_CALCRECT (0x400) and
// DT_NOCLIP (0x100); (3) D3DXCreateFontA in Code/RendDX9Abi/D3dx9_25.def for the Windows images.
// EditorFont has no caller in any inspected image (class registry only), so no game path reaches
// this stop. [RETIRE-WHEN: d3dx-font] those pieces exist; then call D3DXCreateFontA with the
// arguments above, validated by an editor run that draws text through EditorFont.
HRESULT editorFontCreateD3DXFont(IDirect3DDevice9* /*device*/, EditorFontD3DX** /*font*/)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::geom::editorFontCreateD3DXFont reached: "
                         "D3DXCreateFontA (12x8, weight 400, \"Arial\") has no backend\n");
    std::fflush(stderr);
    std::abort();
}

class EditorFont : public IBase, public dx9rend::DeviceLostResetListener
{
public:
    EditorFont();
    ~EditorFont();                                          // not a slot; release() calls it

    virtual void  addRef();                                 // +0x00  0x1001A010 (folded)
    virtual int   getRef();                                 // +0x04  0x10051870 (folded)
    virtual int   release();                                // +0x08  0x10088070
    virtual void* queryInterface(unsigned int iid) const;   // +0x0C  0x10088030 (FontPorted's)

    virtual bool load(const std::string& path);            // +0x10  0x10087E60
    virtual void drawTextColoured(float x, float y, const std::string& text,
                                  unsigned int colour);     // +0x14  0x10087EC0
    virtual void drawText(float x, float y, const std::string& text);   // +0x18  0x10087EA0
    // +0x1C..+0x34: the shared do-nothing bodies the image folds (0x100ED3B0 `xor eax, eax; ret`,
    // 0x10088060 `xor eax, eax; ret 4`, 0x100E1480 `ret 8`, 0x100525C0 the zeroed pair through the
    // hidden return, 0x100E13B0 `ret 4`).
    virtual int  getHeight();                               // +0x1C  0x100ED3B0
    virtual int  getCharacterWidth(unsigned char character); // +0x20  0x10088060
    virtual int  getTextWidth(const std::string& text);     // +0x24  0x10088060
    virtual void setScale(float x, float y);                // +0x28  0x100E1480
    virtual Vec2 getScale() const;                          // +0x2C  0x100525C0
    virtual void setColour(unsigned colour);                // +0x30  0x100E13B0
    virtual unsigned getColour();                           // +0x34  0x100ED3B0
    virtual void releaseFont();                             // +0x38  0x10087F60 [INFERRED] name

    virtual void onDeviceLost();                            // listener +0x00  0x10087F80
    virtual void onDeviceReset();                           // listener +0x04  0x10087F90
    virtual void onDeviceDestroy();                         // listener +0x08  0x10087FA0

private:
    int             m_refCount;                             // +0x08  ctor: 1
    EditorFontD3DX* m_font;                                 // +0x0C
};

// PE 0x10087FF0. The listener vptr passes through the base table (0x101DC4A0), the count becomes 1
// and the listener is registered at priority 1 (renderer list +0x5C, 0x10009D60).
// [CHOSEN, not recovered] m_font = 0: retail never initialises +0x0C (the engine heap does not
// zero), so a font released before load() reads a stale pointer; the memory-safety exception
// keeps that uninitialised read out (docs/macos-port.md, "Port rules").
EditorFont::EditorFont()
    : m_refCount(1), m_font(0)
{
    dx9rend::registerDeviceLostResetListener(this, 1);
}

// PE 0x10087FB0: unregister while a renderer exists, then release the font.
EditorFont::~EditorFont()
{
    if (dx9rend::g_rendererBase)
        dx9rend::unregisterDeviceLostResetListener(this);
    if (m_font)
    {
        m_font->Release();
        m_font = 0;
    }
}

void EditorFont::addRef() { ++m_refCount; }
int  EditorFont::getRef() { return m_refCount; }

// PE 0x10088070: the decremented count; at zero the destructor and the engine free (0x1005F360).
int EditorFont::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~EditorFont();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// PE 0x10088030, shared with FontPorted: IBase or IID 80001.
void* EditorFont::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IFontPorted)
        return const_cast<EditorFont*>(static_cast<const EditorFont*>(this));
    return 0;
}

// PE 0x10087E60 (`retn 4`): the path is never read; the D3DX font is created on the renderer's
// device (+0x0C) into +0x0C, and the answer is whether D3DXCreateFontA returned exactly S_OK.
bool EditorFont::load(const std::string& /*path*/)
{
    return editorFontCreateD3DXFont(
        static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device, &m_font) == 0;
}

// PE 0x10087EC0. The rectangle collapses to the truncated position; DrawTextA runs twice, first
// with DT_CALCRECT (0x400), then with DT_NOCLIP (0x100), both without a sprite. The font is called
// without a null test (shipped: drawing before a successful load dereferences null).
void EditorFont::drawTextColoured(float x, float y, const std::string& text, unsigned int colour)
{
    RECT rect;
    rect.left = static_cast<LONG>(x);
    rect.right = rect.left;
    rect.top = static_cast<LONG>(y);
    rect.bottom = rect.top;
    m_font->DrawTextA(0, text.c_str(), static_cast<INT>(text.size()), &rect, 0x400u, colour);
    m_font->DrawTextA(0, text.c_str(), static_cast<INT>(text.size()), &rect, 0x100u, colour);
}

// PE 0x10087EA0: the coloured draw through its own slot +0x14 in opaque black.
void EditorFont::drawText(float x, float y, const std::string& text)
{
    drawTextColoured(x, y, text, 0xFF000000u);
}

int  EditorFont::getHeight() { return 0; }
int  EditorFont::getCharacterWidth(unsigned char) { return 0; }
int  EditorFont::getTextWidth(const std::string&) { return 0; }
void EditorFont::setScale(float, float) {}
Vec2 EditorFont::getScale() const
{
    Vec2 scale;
    scale.x = 0.0f;
    scale.y = 0.0f;
    return scale;
}
void EditorFont::setColour(unsigned) {}
unsigned EditorFont::getColour() { return 0; }

// PE 0x10087F60: release the font and clear the member.
void EditorFont::releaseFont()
{
    if (m_font)
    {
        m_font->Release();
        m_font = 0;
    }
}

// PE 0x10087F80 / 0x10087F90: forward the device events to the font when one exists.
void EditorFont::onDeviceLost()
{
    if (m_font)
        m_font->OnLostDevice();
}
void EditorFont::onDeviceReset()
{
    if (m_font)
        m_font->OnResetDevice();
}

// PE 0x10087FA0: adjust to the primary object and dispatch its slot +0x38.
void EditorFont::onDeviceDestroy()
{
    releaseFont();
}

// RefFactory_geom_Font_Editor, PE 0x1007C810: refuse aggregation, construct, ask the requested
// interface, release on refusal. The shipped body queries without testing the allocation.
IBase* __fastcall createEditorFont(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    EditorFont* self = new EditorFont();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

extern const unsigned int CID_EditorFont = 81043u;          // dword_1020AF20


// Draws text into the target texture, advances x and returns the total advance; empty text returns zero.
// Spaces advance without drawing. System-memory sources use UpdateSurface; other sources use StretchRect.

float FontPorted::drawToTexture(dx9rend::EngineTexture* target, POINT* position,
                                const std::string& text, bool fromSystemMemory)
{
    const int length = static_cast<int>(text.size());
    if (length <= 0)
        return 0.0f;                                                // flt_101DDE64

    if (fromSystemMemory && !m_textureSys)
    {
        const int savedManaged = dx9rend::textureManagerSwapManaged(2);   // sub_10017E20
        std::string sysName(m_textureName);                             // sub_100019A0:
        sysName += "_sys";                                                //   name + "_sys"
        m_textureSys = dx9rend::textureManagerGetTexture(sysName);        // sub_10019260
        dx9rend::textureManagerSwapManaged(savedManaged);
    }

    IDirect3DSurface9* const destination =
        dx9rend::textureGetLevelSurface(target, 0, 0x7FFFFFFFu);          // sub_10014950
    IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(
        static_cast<dx9rend::EngineTexture*>(fromSystemMemory ? m_textureSys : m_textureDef),
        0, 0x7FFFFFFFu);
    const float startX = static_cast<float>(position->x);

    for (int i = 0; i < length; ++i)
    {
        int advance;
        if (text.c_str()[i] == ' ')
        {
            advance = m_spaceWidth;                                     // +0xC20
        }
        else
        {
            const Glyph& glyph = m_glyphs[static_cast<unsigned char>(text.c_str()[i])];
            RECT glyphRect;
            glyphRect.left   = static_cast<int>(static_cast<float>(m_textureWidth) * glyph.u);
            glyphRect.top    = static_cast<int>(static_cast<float>(m_textureHeight) * glyph.v);
            glyphRect.right  = static_cast<int>(static_cast<float>(glyphRect.left) + glyph.width);
            glyphRect.bottom = m_height + glyphRect.top;
            RECT destinationRect;
            destinationRect.left   = position->x;
            destinationRect.top    = position->y;
            destinationRect.right  = static_cast<int>(static_cast<float>(position->x) + glyph.width);
            destinationRect.bottom = m_height + position->y;
            IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice());
            if (fromSystemMemory)
                device->UpdateSurface(source, &glyphRect, destination, position);
            else
                device->StretchRect(source, &glyphRect, destination, &destinationRect,
                                    D3DTEXF_POINT);
            advance = static_cast<int>(glyph.width);
        }
        position->x += advance;
    }

    source->Release();
    destination->Release();
    return static_cast<float>(position->x) - startX;
}

// The two seams NameTagRenderer.cpp declares. Font::Ported vslot +0x10 is load(const std::string&)
// (0x100876A0); the typed call below dispatches through that same slot.
bool fontPortedLoad(IBase* font, const std::string& path)
{
    return static_cast<FontPorted*>(font)->load(path);
}

float fontPortedDrawToTexture(IBase* font, dx9rend::EngineTexture* target, POINT* position,
                              const std::string& text, bool fromSystemMemory)
{
    return static_cast<FontPorted*>(font)->drawToTexture(target, position, text, fromSystemMemory);
}

// Typed font adapters for renderer console drawing.
int fontPortedGetHeight(IBase* font)
{
    return static_cast<FontPorted*>(font)->getHeight();
}

void fontPortedDrawText(IBase* font, float x, float y, const std::string& text)
{
    static_cast<FontPorted*>(font)->drawText(x, y, text);
}

int fontPortedGetTextWidth(IBase* font, const std::string& text)
{ return static_cast<FontPorted*>(font)->getTextWidth(text); }
unsigned fontPortedGetColour(IBase* font)
{ return static_cast<FontPorted*>(font)->getColour(); }
void fontPortedSetColour(IBase* font, unsigned colour)
{ static_cast<FontPorted*>(font)->setColour(colour); }

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
