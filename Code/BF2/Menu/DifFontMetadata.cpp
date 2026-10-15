// Font metadata containers. Preserve key widths, ordering and duplicate-insert behavior.
// [INFERRED] Source placement.
#include "DifFontMetadata.h"
#if !DEDICATED
#include <cmath>
#include <cstring>
#include <istream>
namespace dice { namespace hfe { namespace menu {
DifFontMetadata::~DifFontMetadata()
{
    for (auto& item : m_original) if (item.second) delete item.second;
    for (auto& item : m_scaled) if (item.second) delete item.second;
}
DifGlyph* DifFontMetadata::findGlyph(int character)
{
    auto found = m_scaled.find(static_cast<std::uint16_t>(character));
    return found == m_scaled.end() ? 0 : found->second;
}
float DifFontMetadata::getKerning(int first, int second)
{
    const std::uint32_t key = static_cast<std::uint16_t>(first) |
        (static_cast<std::uint32_t>(static_cast<std::uint16_t>(second)) << 16);
    auto found = m_kerning.find(key);
    return found == m_kerning.end() ? 0.0f : found->second;
}
void DifFontMetadata::readHeader(std::istream& stream)
{
    stream >> m_version;
    char name[36];
    stream >> name[0];
    stream.putback(name[0]);
    stream.getline(name, 33);
    m_headerName += name;
    stream >> m_textureWidth;
    if (m_version > 1u) stream >> m_textureHeight;
    else m_textureHeight = m_textureWidth;
    stream >> m_lineHeight;
}
void DifFontMetadata::readKerning(std::istream& stream)
{
    unsigned int count;
    stream >> count;
    while (count && !stream.eof())
    {
        unsigned short first = 0, second = 0;
        float amount = 0.0f;
        stream >> first >> second >> amount;
        const std::uint32_t key = first | (static_cast<std::uint32_t>(second) << 16);
        m_kerning.insert(std::make_pair(key, amount)); // duplicate keeps first
        --count;
    }
}
void DifFontMetadata::readGlyphs(std::istream& stream)
{
    unsigned int count;
    stream >> count;
    while (count && !stream.eof())
    {
        unsigned short character;
        float left, width, right;
        int vertical;
        unsigned int x0, y0, x1, y1;
        stream >> character >> left >> width >> right >> vertical;
        stream >> x0 >> y0 >> x1 >> y1;
        DifGlyph* glyph = new DifGlyph;
        if (glyph)
        {
            glyph->leftBearing = left;
            glyph->width = width;
            glyph->rightBearing = right;
            glyph->x0 = static_cast<std::uint16_t>(x0);
            glyph->y0 = static_cast<std::uint16_t>(y0);
            glyph->x1 = static_cast<std::uint16_t>(x1);
            glyph->y1 = static_cast<std::uint16_t>(y1);
            glyph->verticalOffset = static_cast<float>(vertical);
            glyph->scaledHeight = static_cast<float>(static_cast<int>(glyph->y1) - glyph->y0);
            glyph->character = character;
        }
        // Retail ignores insertion failure and leaks a duplicate's new record.
        m_original.insert(std::make_pair(character, glyph));
        --count;
    }
}
void DifFontMetadata::clearScaledGlyphs()
{
    for (auto& item : m_original)
    {
        if (m_scaled[item.first])
        {
            DifGlyph* glyph = m_scaled[item.first];
            if (glyph) delete glyph;
            m_scaled[item.first] = 0;
        }
    }
}
namespace {
double scaleMetric(float value, float factor, float reciprocal)
{
    // Unlike Style positioning, fmod uses the pre-float-store product here.
    const double product = static_cast<double>(value) * factor;
    const float storedProduct = static_cast<float>(product);
    const double rounded = std::floor(!(std::fmod(product, 1.0) >= 0.5) ?
        storedProduct : static_cast<double>(storedProduct) + 1.0);
    return rounded * reciprocal;
}
}
void DifFontMetadata::scaleGlyphs(float factor, float reciprocal)
{
    for (auto& item : m_original)
    {
        const DifGlyph& original = *item.second;
        const float left = static_cast<float>(scaleMetric(original.leftBearing, factor, reciprocal));
        const float width = static_cast<float>(scaleMetric(original.width, factor, reciprocal));
        const float right = static_cast<float>(scaleMetric(original.rightBearing, factor, reciprocal));
        const float vertical = static_cast<float>(scaleMetric(original.verticalOffset, factor, reciprocal));
        const float rawHeight = static_cast<float>(static_cast<int>(original.y1) - original.y0);
        const double scaledHeight = scaleMetric(rawHeight, factor, reciprocal);
        DifGlyph* glyph = new DifGlyph;
        if (glyph)
        {
            glyph->character = item.first;
            glyph->leftBearing = left;
            glyph->width = width;
            glyph->rightBearing = right;
            glyph->x0 = original.x0; glyph->y0 = original.y0;
            glyph->x1 = original.x1; glyph->y1 = original.y1;
            glyph->verticalOffset = vertical;
            glyph->scaledHeight = scaledHeight == 0.0 ? rawHeight : static_cast<float>(scaledHeight);
        }
        m_scaled[item.first] = glyph;
    }
}
void DifFontMetadata::buildScaledGlyphs(int resolution)
{
    clearScaledGlyphs();
    switch (resolution)
    {
    case 1: scaleGlyphs(0.949999988079071f, 1.25f); return;
    case 3: scaleGlyphs(1.25f, 0.78125f); return;
    case 4: case 7: case 8: scaleGlyphs(1.5f, 0.6944444179534912f); return;
    case 5: scaleGlyphs(1.5f, 0.625f); return;
    default:
        for (auto& item : m_original)
        {
            DifGlyph* glyph = new DifGlyph;
            if (glyph) std::memcpy(glyph, item.second, sizeof(DifGlyph));
            m_scaled[item.first] = glyph;
        }
    }
}
int DifFontMetadata::resolutionForWidth(int& width)
{
    switch (width)
    {
    case 640: return 1;
    case 800: return 2;
    case 1024: return 3;
    case 1152: return 4;
    case 1280: return 5;
    case 1600: return 6;
    case 1920: return 7;
    case 2048: return 8;
    default: width = 800; return 2;
    }
}
static_assert(sizeof(DifGlyph) == 32, "eight-word scalar glyph record");
#if defined(_M_IX86)
static_assert(sizeof(DifFontMetadata) == 100, "native x86 STL metadata");
#endif
} } }
#endif
