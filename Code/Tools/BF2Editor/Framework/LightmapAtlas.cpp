// Code/Tools/BF2Editor/Framework/LightmapAtlas.cpp - see LightmapAtlas.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "Framework/LightmapAtlas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace bf2editor
{

namespace
{

const size_t kHeaderSize = 128;                 // "DDS " and the 124-byte header
const unsigned kMipMapCountFlag = 0x20000;      // DDSD_MIPMAPCOUNT

unsigned readU32(const std::string& data, size_t at)
{
    unsigned value = 0;
    std::memcpy(&value, data.data() + at, 4);
    return value;
}

void writeU32(std::string& data, size_t at, unsigned value)
{
    std::memcpy(&data[at], &value, 4);
}

std::string trimmed(const std::string& text)
{
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

unsigned blocksAcross(unsigned size)
{
    return size < 4 ? 1u : (size + 3u) / 4u;
}

std::string lowered(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(text[i] >= 'A' && text[i] <= 'Z' ? text[i] - 'A' + 'a' : text[i]);
    return text;
}

} // namespace

bool parseAtlasIndex(const std::string& text, std::vector<AtlasEntry>& entries, std::string& error)
{
    entries.clear();
    std::istringstream in(text);
    std::string line;
    int number = 0;
    while (std::getline(in, line))
    {
        ++number;
        const std::string t = trimmed(line);
        if (t.empty() || t[0] == '#')
            continue;
        const size_t space = t.find_first_of(" \t");
        AtlasEntry entry;
        std::vector<std::string> fields;
        if (space != std::string::npos)
        {
            entry.texture = t.substr(0, space);
            std::string rest = trimmed(t.substr(space));
            size_t start = 0;
            while (start <= rest.size())
            {
                size_t comma = rest.find(',', start);
                if (comma == std::string::npos)
                    comma = rest.size();
                fields.push_back(trimmed(rest.substr(start, comma - start)));
                start = comma + 1;
            }
        }
        if (fields.size() != 6)
        {
            char where[64];
            std::snprintf(where, sizeof(where), "line %d of the atlas index is not an entry", number);
            error = where;
            return false;
        }
        entry.atlasFile = fields[0];
        entry.page = std::atoi(fields[1].c_str());
        for (int k = 0; k < 4; ++k)
            entry.rect[k] = static_cast<float>(std::atof(fields[2 + k].c_str()));
        entries.push_back(entry);
    }
    return true;
}

std::string atlasFileName(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::vector<std::string> unnamedAtlasPages(const std::vector<AtlasEntry>& entries, const std::vector<std::string>& files)
{
    std::vector<std::string> named;
    for (size_t i = 0; i < entries.size(); ++i)
        named.push_back(lowered(atlasFileName(entries[i].atlasFile)));
    std::sort(named.begin(), named.end());
    std::vector<std::string> unnamed;
    for (size_t i = 0; i < files.size(); ++i)
    {
        const std::string name = lowered(atlasFileName(files[i]));
        if (name.compare(0, 13, "lightmapatlas") == 0 && name.size() > 17 && name.compare(name.size() - 4, 4, ".dds") == 0
            && !std::binary_search(named.begin(), named.end(), name))
            unnamed.push_back(files[i]);
    }
    return unnamed;
}

bool cutAtlasTexture(const std::string& page, const AtlasEntry& entry, std::string& texture, std::string& error)
{
    texture.clear();
    if (page.size() < kHeaderSize || page.compare(0, 4, "DDS ") != 0)
    {
        error = "the atlas page is not a DDS";
        return false;
    }
    const unsigned pageHeight = readU32(page, 12), pageWidth = readU32(page, 16);
    const unsigned pageMips = (readU32(page, 8) & kMipMapCountFlag) ? std::max(1u, readU32(page, 28)) : 1u;
    const std::string fourCC = page.substr(84, 4);
    const unsigned blockBytes = fourCC == "DXT1" ? 8u : (fourCC == "DXT3" || fourCC == "DXT5") ? 16u : 0u;
    if (!blockBytes || !pageWidth || !pageHeight)
    {
        error = "the atlas page is not DXT1, DXT3 or DXT5";
        return false;
    }
    const long x0 = std::lround(entry.rect[0] * pageWidth), y0 = std::lround(entry.rect[1] * pageHeight);
    const long w = std::lround(entry.rect[2] * pageWidth), h = std::lround(entry.rect[3] * pageHeight);
    if (w <= 0 || h <= 0 || x0 < 0 || y0 < 0 || x0 + w > static_cast<long>(pageWidth) || y0 + h > static_cast<long>(pageHeight)
        || x0 % 4 || y0 % 4 || w % 4 || h % 4)
    {
        error = "the texture " + entry.texture + " does not lie on the page's blocks";
        return false;
    }
    // Its own full chain, as far as the page goes.
    unsigned mips = 1;
    while (((static_cast<unsigned>(w) >> mips) || (static_cast<unsigned>(h) >> mips)) && mips < pageMips)
        ++mips;
    // The page's levels, one after the other after the header.
    std::vector<size_t> pageLevel(pageMips);
    size_t at = kHeaderSize;
    for (unsigned m = 0; m < pageMips; ++m)
    {
        pageLevel[m] = at;
        at += static_cast<size_t>(blocksAcross(std::max(1u, pageWidth >> m))) * blocksAcross(std::max(1u, pageHeight >> m))
            * blockBytes;
    }
    if (at > page.size())
    {
        error = "the atlas page is cut short";
        return false;
    }
    texture = page.substr(0, kHeaderSize);
    for (unsigned m = 0; m < mips; ++m)
    {
        const unsigned pageBlocksX = blocksAcross(std::max(1u, pageWidth >> m));
        const unsigned pageBlocksY = blocksAcross(std::max(1u, pageHeight >> m));
        const unsigned blocksX = blocksAcross(std::max(1u, static_cast<unsigned>(w) >> m));
        const unsigned blocksY = blocksAcross(std::max(1u, static_cast<unsigned>(h) >> m));
        const unsigned originX = (static_cast<unsigned>(x0) >> m) / 4u, originY = (static_cast<unsigned>(y0) >> m) / 4u;
        for (unsigned row = 0; row < blocksY; ++row)
        {
            const unsigned y = std::min(originY + row, pageBlocksY - 1);
            for (unsigned column = 0; column < blocksX; ++column)
            {
                const unsigned x = std::min(originX + column, pageBlocksX - 1);
                texture.append(page, pageLevel[m] + (static_cast<size_t>(y) * pageBlocksX + x) * blockBytes, blockBytes);
            }
        }
    }
    writeU32(texture, 12, static_cast<unsigned>(h));
    writeU32(texture, 16, static_cast<unsigned>(w));
    writeU32(texture, 20, blocksAcross(static_cast<unsigned>(w)) * blocksAcross(static_cast<unsigned>(h)) * blockBytes);
    writeU32(texture, 28, mips);
    writeU32(texture, 8, mips > 1 ? (readU32(texture, 8) | kMipMapCountFlag) : (readU32(texture, 8) & ~kMipMapCountFlag));
    const unsigned kComplexMipmap = 0x8u | 0x400000u;                       // DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
    writeU32(texture, 108, mips > 1 ? (readU32(texture, 108) | kComplexMipmap) : (readU32(texture, 108) & ~kComplexMipmap));
    return true;
}

} // namespace bf2editor
