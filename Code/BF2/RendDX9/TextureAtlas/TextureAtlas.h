// Path:     [INFERRED] header name in the attested directory Code/BF2/RendDX9/TextureAtlas/. The
//           implementation file is attested - TextureAtlas.cpp is quoted with lines 62 and 75 by
//           its own loadTAI diagnostics - but no __FILE__ names its header.
// Contents: the 20-byte TextureAtlas, recovered from RendDX9.dll. One atlas object per
//           .tai file; the TextureManager's atlas cache (TextureManager.cpp) owns a vector of them.

#ifndef BF2_RENDDX9_TEXTUREATLAS_TEXTUREATLAS_H
#define BF2_RENDDX9_TEXTUREATLAS_TEXTUREATLAS_H

#include <string>

#include <list>
#include <map>
#include <string>
#include <vector>

namespace dice {
namespace hfe {
namespace dx9rend {

struct EngineTexture;

// The 20-byte per-entry record (retail: node+40 of the atlas's 0x40-byte map nodes):
//     +0x00 width  +0x04 height  +0x08 uOffset  +0x0C vOffset  +0x10 the atlas page texture
// The .tai line supplies them as (uOffset, vOffset, width, height) and loadTAI stores them
// re-ordered exactly like this; Meme::Texture::setTexture reads all five.
struct TextureAtlasEntry
{
    float          width;      // +0x00
    float          height;     // +0x04
    float          uOffset;    // +0x08
    float          vOffset;    // +0x0C
    EngineTexture* page;       // +0x10
};


// The atlas's entries, keyed by the normalized source name loadTAI computes (strcmp order).
typedef std::map<std::string, TextureAtlasEntry> TextureAtlasEntryMap;

// dice::hfe TextureAtlas, ONE vptr (off_101DE47C). Retail is 20 bytes: vptr, refCount and a 12-byte
// VC7.1 map at +0x08. The native entry map is this toolset's std::map (issue #7), so owners
// allocate sizeof(TextureAtlas) and construct it with textureAtlasCtor.
struct TextureAtlas
{
    void*                vptr;      // +0x00
    int                  refCount;  // +0x04, ctor: 1
    TextureAtlasEntryMap entries;   // retail +0x08
};

TextureAtlas*      textureAtlasCtor(void* storage);
// The number of entries the atlas holds.
unsigned int       textureAtlasEntryCount(const TextureAtlas* atlas);
void               textureAtlasDtor(TextureAtlas* atlas);
bool               textureAtlasLoadTAI(TextureAtlas* atlas,
                                       const std::string& path);          // TextureAtlas_loadTAI, 0x10015970
TextureAtlasEntry* textureAtlasFind(TextureAtlas* atlas,
                                    const std::string& name);
// Releases entry page references and empties the map while retaining its sentinel.
void               textureAtlasReleaseAll(TextureAtlas* atlas);
// Appends distinct pages in map order to the caller-owned vector.
void               textureAtlasCollectPages(TextureAtlas* atlas,
                                            std::vector<EngineTexture*>& pages);
// Appends normalized entry names in map order.
void               textureAtlasCollectNames(TextureAtlas* atlas,
                                            std::vector<std::string>& names);


// Generates <outputDirectory><atlasName>.tai and numbered DDS pages from source names.
// Returns false only for an empty source list; builder diagnostics do not affect the result.
bool               textureAtlasBuild(TextureAtlas* atlas, std::string outputDirectory,
                                     std::string atlasName, std::string inputDirectory,
                                     const std::list<std::string>& textures, bool dxt5);
// the entry the callers use (Overgrowth.generateAtlas / save /
// reload, DecalManager.generateTexture, the undergrowth rebuild): copies its three strings BY value
// and forwards to the builder.
bool               textureAtlasGenerate(TextureAtlas* atlas, std::string outputDirectory,
                                        std::string atlasName, std::string inputDirectory,
                                        const std::list<std::string>& textures, bool dxt5);
// generate("", name, "", textures, false). No caller in the 1.50 image.
bool               textureAtlasGenerate(TextureAtlas* atlas, const std::string& atlasName,
                                        const std::list<std::string>& textures);

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_RENDDX9_TEXTUREATLAS_TEXTUREATLAS_H
