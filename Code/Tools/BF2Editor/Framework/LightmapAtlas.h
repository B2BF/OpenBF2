// The object lightmap atlas a level's Lightmaps/Objects holds (LightmapAtlas.tai and its pages, TextureAtlasBuilder's
// output): its index read, and a texture cut back out of its page with every mip level - the level's blocks copied, so a
// DXT lightmap comes back as it was packed (a level smaller than a block takes the block it lies in). Stock levels ship
// only their atlas, so a partial object trace re-packs them from these. Engine-free (checked in bf2_editor_checks).
// Editor-only code (D3).
#ifndef BF2EDITOR_FRAMEWORK_LIGHTMAPATLAS_H
#define BF2EDITOR_FRAMEWORK_LIGHTMAPATLAS_H

#include "Framework/EditorConfig.h"

#include <string>
#include <vector>

namespace bf2editor
{

// One line of an atlas index: the packed texture's path, its page's path and index, and where it lies on the page as
// fractions of the page (width offset, height offset, width, height).
struct AtlasEntry
{
    std::string texture;
    std::string atlasFile;
    int         page = 0;
    float       rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// The entries of a .tai text (comment lines start with '#'). False with `error` naming the first line it cannot read.
bool parseAtlasIndex(const std::string& text, std::vector<AtlasEntry>& entries, std::string& error);

// The file name of a path, after its last slash ("house_high_06=00=-155=160=155.dds").
std::string atlasFileName(const std::string& path);

// The atlas pages among `files` (file names in a level's Lightmaps/Objects; a page is LightmapAtlas*.dds in any case)
// that no entry names - left behind when a smaller atlas replaced a larger one. The game reads only the named pages.
std::vector<std::string> unnamedAtlasPages(const std::vector<AtlasEntry>& entries, const std::vector<std::string>& files);

// The texture an entry names, cut out of its page's DDS bytes (DXT1, DXT3 or DXT5): a DDS of the entry's size with every
// mip level of its full chain the page has. False with `error` when the page is not such a DDS or the entry's rectangle
// is not on the page's 4-texel blocks.
bool cutAtlasTexture(const std::string& page, const AtlasEntry& entry, std::string& texture, std::string& error);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_LIGHTMAPATLAS_H
