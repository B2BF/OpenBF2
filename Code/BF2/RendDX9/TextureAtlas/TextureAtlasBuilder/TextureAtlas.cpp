// Atlas packing, page groups, and .tai output for TextureAtlasBuilder.
#if defined(BF2_IMAGE_TEXTUREATLASBUILDER)

#include "TextureAtlas.h"
#include "Packer.h"
#include "Vc71Sort.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace textureatlas {


// The interfaces' exported members. The two virtual destructors (0x10002240 / 0x10002210, 7 B each)
// only restore the interface vftable; the default and copy constructors and operator= (exports 1-4,
// 7, 8) are the compiler's, exported because the classes are dllexport. They sit at the head of this
// TU's code in the image (0x10002210..0x10002275), before the atlas base - [INFERRED] placement.

ITextureAtlasConfiguration::~ITextureAtlasConfiguration()
{
}

ITextureAtlas::~ITextureAtlas()
{
}

namespace {

// nullsub_1 (0x10002320) - the atlas's default error and warning sink (the constructor stores it in
// both). The configuration has its own copy (nullsub_2, TextureAtlasConfiguration.cpp).
void __fastcall ignoreMessage(const std::string& /*message*/)
{
}

// (24 B, ECX = a, EDX = b) - sort mode 0, the default: larger area first.
// `imul` is a 32-bit signed product, as shipped.
bool __fastcall compareArea(const TextureInfo& a, const TextureInfo& b)
{
    return a.rect.w * a.rect.h > b.rect.w * b.rect.h;
}

// (106 B, ECX = a, EDX = b) - sort mode 1: the larger side first, then the wider, then the
// taller. Each side pair goes through std::max's reference form (`(a < b) ? b : a`, 0x10002359..
// 0x10002371). NON-strict, as shipped: two equal textures compare true both ways (the final
// `setle` at 0x100023B3). std::sort's contract does not allow that; the VC7.1 algorithm the DLL
// used (Vc71Sort.h) survives it, which is why mode 1 never crashed - nothing in RendDX9 or the
// command-line tool sets it.
bool __fastcall compareSize(const TextureInfo& a, const TextureInfo& b)
{
    if (std::max(a.rect.w, a.rect.h) > std::max(b.rect.w, b.rect.h))
        return true;
    if (std::max(b.rect.w, b.rect.h) > std::max(a.rect.w, a.rect.h))
        return false;
    if (a.rect.w > b.rect.w)
        return true;
    if (b.rect.w > a.rect.w)
        return false;
    if (a.rect.h > b.rect.h)
        return true;
    if (b.rect.h > a.rect.h)
        return false;
    return true;
}

// Sorts format names descending. The ABI takes both TextureInfo records by value.
bool __fastcall compareFormat(TextureInfo a, TextureInfo b)
{
    return a.format > b.format;
}

} // anonymous namespace

// see TextureAtlas.h. `i = 1; if (v > 1) do i += i; while (i < v)`.
int __fastcall nextPowerOfTwo(int value)
{
    int result = 1;
    while (result < value)
        result *= 2;
    return result;
}

// (6 B each): the base class's fixed limit. The D3D9 leaf overrides both
// with its device-dependent fields (0 x 0 without a device).
int TextureAtlas::maxWidth()
{
    return 2048;
}

int TextureAtlas::maxHeight()
{
    return 2048;
}


// Builds atlas pages and writes their normalized rectangles to a .tai file.
// Returns false if page construction fails; output-stream failures are not reported.
// [PLATFORM] Text formatting requires VC7.1 round-half-up behavior, provided by
// legacy_stdio_float_rounding.obj in the builder target.

bool TextureAtlas::save(const char* taiPath)
{
    if (!buildTextures())
        return false;

    std::ofstream out(taiPath);

    out << "# " << taiPath << std::endl;
    out << "# " << std::endl;
    out << "# " << std::endl;
    out << "# <filename>\t\t<atlas filename>, <atlas idx>, <woffset>, <hoffset>, <width>, <height>" << std::endl;
    out << "# " << std::endl;
    out << "# Texture <filename> can be found in texture atlas <atlas filename>, i.e.," << std::endl;
    out << "# <group><idx>.dds with texture coordinates boundary given by:" << std::endl;
    out << "#   A = ( <woffset>, <hoffset> )" << std::endl;
    out << "#   B = ( <woffset> + <width>, <hoffset> + <height> )" << std::endl;
    out << "# " << std::endl;
    out << "# where coordinates (0,0) and (1,1) of the original texture map correspond" << std::endl;
    out << "# to coordinates A and B, respectively, in the texture atlas." << std::endl;
    out << std::endl << std::endl;

    for (std::vector<Group>::iterator page = m_pages.begin(); page != m_pages.end(); ++page)
    {
        for (std::vector<TextureInfo>::iterator tex = page->textures.begin();
             tex != page->textures.end(); ++tex)
        {
            const double borderU = tex->border ? static_cast<double>(page->border) / page->width : 0.0;
            const double borderV = tex->border ? static_cast<double>(page->border) / page->height : 0.0;
            const float u = static_cast<float>(static_cast<double>(tex->rect.x) / page->width + borderU);
            const float v = static_cast<float>(static_cast<double>(tex->rect.y) / page->height + borderV);
            const float w = static_cast<float>(static_cast<double>(tex->rect.w) / page->width - (borderU + borderU));
            const float h = static_cast<float>(static_cast<double>(tex->rect.h) / page->height - (borderV + borderV));

            out << tex->name << "\t\t" << page->name << ".dds, " << page->index << ", ";
            out << u << ", " << v << ", " << w << ", " << h << std::endl;
        }
    }
    return true;
}


// the base constructor: the vptr, the empty string and vectors, sort mode 0 and
// nullsub_1 as both sinks.

TextureAtlas::TextureAtlas()
    : m_sortMode(0),
      m_error(ignoreMessage),
      m_warning(ignoreMessage)
{
}


// Replaces pages with a packing of configuration's textures, grouped by format as requested.
// Mutates nonpositive group mip counts to the input maximum. Oversized textures are reported
// and dropped; nonempty page dimensions are rounded up to powers of two.

void TextureAtlas::build(ITextureAtlasConfiguration* configuration)
{
    Packer packer;

    m_pending.clear();
    m_pages.clear();
    m_outputPath = configuration->getOutputPath();

    for (GroupIter group = configuration->groupsBegin(); group != configuration->groupsEnd(); ++group)
    {
        const bool splitByFormat = group->format.empty();

        int miplevels = 1;
        for (std::vector<TextureInfo>::iterator tex = group->textures.begin();
             tex != group->textures.end(); ++tex)
        {
            miplevels = std::max(miplevels, tex->miplevels);
            m_pending.push_back(*tex);
        }
        if (group->miplevels <= 0)
            group->miplevels = miplevels;

        int subGroupIndex = 0;
        while (m_pending.size() != 0)
        {
#if defined(__APPLE__) || defined(BF2_LINUX_CLIENT)
            // [CHOSEN, not recovered] Decimal conversion through the native CRT.
            m_pages.push_back(Group(group->name + std::to_string(subGroupIndex), 0, 0,
                                    group->format, group->miplevels, group->border));
#else
            char buffer[16];
            m_pages.push_back(Group(group->name + _itoa(subGroupIndex, buffer, 10), 0, 0,
                                    group->format, group->miplevels, group->border));
#endif
            Group& page = m_pages.back();

            if (m_sortMode == 1)
                vc71sort::sort(m_pending.begin(), m_pending.end(), compareSize);
            else
                vc71sort::sort(m_pending.begin(), m_pending.end(), compareArea);

            if (splitByFormat)
                std::stable_sort(m_pending.begin(), m_pending.end(), compareFormat);

            int width = group->width;
            if (width <= 0 || width > maxWidth())
                width = maxWidth();
            int height = group->height;
            if (height <= 0 || height > maxHeight())
                height = maxHeight();
            packer.init(width, height);

            {
                std::string format;
                std::vector<TextureInfo>::iterator tex = m_pending.begin();
                while (tex != m_pending.end())
                {
                    if (splitByFormat)
                    {
                        if (!format.empty() && format != tex->format)
                            break;
                        format = tex->format;
                    }

                    if (packer.insert(tex->rect))
                    {
                        page.textures.push_back(*tex);
                        if (tex->rect.x + tex->rect.w > page.width)
                            page.width = tex->rect.x + tex->rect.w;
                        if (tex->rect.y + tex->rect.h > page.height)
                            page.height = tex->rect.y + tex->rect.h;
                        tex = m_pending.erase(tex);
                    }
                    else if (packer.fits(tex->rect))
                    {
                        ++tex;
                    }
                    else
                    {
                        m_warning(std::string("Texture size larger than maximum atlas size: ") + tex->name);
                        tex = m_pending.erase(tex);
                    }
                }

                if (splitByFormat)
                    page.format = format;
            }
            packer.clear();

            if (page.textures.size() != 0)
            {
                page.width = nextPowerOfTwo(page.width);
                page.height = nextPowerOfTwo(page.height);
                page.index = subGroupIndex++;
            }
            else
            {
                m_pages.pop_back();
            }
        }
    }
}

// (13 B each): plain stores.
void TextureAtlas::setSortMode(int mode)
{
    m_sortMode = mode;
}

void TextureAtlas::setErrorCallback(MessageFn callback)
{
    m_error = callback;
}

void TextureAtlas::setWarningCallback(MessageFn callback)
{
    m_warning = callback;
}

// (3 B): `mov al, 1; ret` - the base renders nothing and reports success.
bool TextureAtlas::buildTextures()
{
    return true;
}

// no body of its own - the members are destroyed in reverse order (the pending
// list, the pages, the output path) and the interface's vptr restored. 0x100073A0 is the scalar
// deleting destructor the vftable holds.
TextureAtlas::~TextureAtlas()
{
}

} // namespace textureatlas

#endif // BF2_IMAGE_TEXTUREATLASBUILDER
