// [INFERRED] client Font/DifFont/BitmapFont source placement. All five virtual
// bodies of each class are instruction-backed.
#include "Font.h"
#if !DEDICATED
#include "DifFontMetadata.h"
#include "../External/BFMeme/Quad.h"
#include "../GeomMisc/MenuRend/IMemeTexture.h"
#include <cmath>
#include <cstdio>
namespace dice { namespace hfe { namespace menu {
namespace { int previousCharacter = 0; }           // PE A23DBC, shared across fonts
Font::Font(const char* name) : m_name(name), m_references(0) {}
Font::~Font() {}
bool Font::drawCharacter(int, meme::Quad*) { return false; }
float Font::getHeight() { return 0.0f; }
bool Font::getGlyphMetrics(int, meme::GlyphMetrics*) { return false; }
float Font::getKerning(int, int) { return 0.0f; }

DifFont::DifFont(const char* name) : Font(name), m_texture(0), m_metadata(0) {}
DifFont::~DifFont()
{
    if (m_texture) m_texture->release();
    if (m_metadata) delete m_metadata;
}
float DifFont::getHeight() { return m_metadata->getLineHeight(); }
float DifFont::getKerning(int previous, int current)
{ return m_metadata->getKerning(previous, current); }
bool DifFont::getGlyphMetrics(int character, meme::GlyphMetrics* output)
{
    DifGlyph* glyph = m_metadata->findGlyph(character);
    if (!glyph)
    {
        previousCharacter = 0;
        return false;                             // output remains untouched
    }
    const float height = static_cast<float>(static_cast<int>(glyph->y1) - glyph->y0);
    const double kerning = previousCharacter ? getKerning(previousCharacter, character) : 0.0;
    const float left = static_cast<float>(std::floor(static_cast<double>(glyph->leftBearing) + kerning + 0.5));
    const float vertical = static_cast<float>(std::floor(static_cast<double>(glyph->verticalOffset)));
    const float right = glyph->rightBearing;
    const float width = glyph->width;
    output->leftBearing = left;
    output->width = width;
    output->height = height;
    previousCharacter = character;
    output->rightBearing = right;
    output->verticalOffset = vertical;
    return true;
}
bool DifFont::drawCharacter(int character, meme::Quad* quad)
{
    const int lookup = character < 0 ? static_cast<unsigned char>(character) : character;
    DifGlyph* glyph = m_metadata->findGlyph(lookup);
    if (!glyph)
    {
        char decimal[20], hexadecimal[20];
        std::sprintf(decimal, "%d", lookup);
        std::sprintf(hexadecimal, "0x%X", lookup);
        return false;
    }
    if (character == 32) return true;
    const double uScale = 1.0 / static_cast<int>(m_metadata->getTextureWidth());
    const double vScale = 1.0 / static_cast<int>(m_metadata->getTextureHeight());
    const double bias = m_metadata->getScale() == 1.0f ? 0.009999999776482582 : 0.0;
    const float u0 = static_cast<float>((glyph->x0 + bias) * uScale);
    const float v0 = static_cast<float>((glyph->y0 + bias) * vScale);
    const float u1 = static_cast<float>((glyph->x1 + bias) * uScale);
    const float v1 = static_cast<float>((glyph->y1 + bias) * vScale);
    const int rawWidth = static_cast<int>(glyph->x1) - glyph->x0;
    quad->corners[1].x = static_cast<float>(static_cast<double>(quad->corners[1].x) - glyph->width);
    quad->corners[2].x = static_cast<float>(static_cast<double>(quad->corners[2].x) - glyph->width);
    quad->corners[1].x = static_cast<float>(static_cast<double>(rawWidth) + quad->corners[1].x);
    quad->corners[2].x = static_cast<float>(static_cast<double>(rawWidth) + quad->corners[2].x);
    m_texture->drawUV(quad, u0, v0, u1, v1);
    return true;                                  // renderer result ignored
}

BitmapFont::BitmapFont(const char* name)
    : Font(name), m_texture(0), m_height(0.0f), m_glyphs(new BitmapGlyph[256]) {}
BitmapFont::~BitmapFont()
{
    if (m_texture) m_texture->release();
    delete[] m_glyphs;
}
float BitmapFont::getHeight() { return m_height; }
bool BitmapFont::getGlyphMetrics(int character, meme::GlyphMetrics* output)
{
    const float height = m_height;
    const double width = static_cast<double>(m_glyphs[character].width) / m_scaleU;
    output->leftBearing = 0.0f;
    output->rightBearing = 0.0f;
    output->verticalOffset = 0.0f;
    output->width = static_cast<float>(width);
    output->height = height;
    return true;
}
bool BitmapFont::drawCharacter(int character, meme::Quad* quad)
{
    const BitmapGlyph& glyph = m_glyphs[character]; // retail has no index guard
    const float u1 = static_cast<float>(static_cast<double>(glyph.width) + glyph.u);
    const float v1 = static_cast<float>(static_cast<double>(glyph.height) + glyph.v);
    m_texture->drawUV(quad, glyph.u, glyph.v, u1, v1);
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(Font) == 32, "native x86 string-bearing Font");
static_assert(sizeof(DifFont) == 40, "native x86 DifFont");
static_assert(sizeof(BitmapFont) == 52, "native x86 BitmapFont");
#endif
static_assert(sizeof(BitmapGlyph) == 16, "four scalar glyph values");
} } }
#endif
