// Parses and resolves .tai entries and drives the atlas builder.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include "../RendDX9Base.h"
#include "../Texture.h"
#include "TextureAtlas.h"
#include "TextureAtlasBuilder/ITextureAtlas.h"     // the two creates - TextureAtlasBuilder.dll imports

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <map>
#include <new>
#include <string>
#include <utility>

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_rendererBase;   // dword_1023B970 (RendDX9Init.cpp) - the generator reads its device

namespace {


// Four-slot atlas interface: addRef, getRef, release, and queryInterface.


void RD9_THUNK atlas_addRef(void* self RD9_THUNK_EDX) { ++static_cast<TextureAtlas*>(self)->refCount; }
int  RD9_THUNK atlas_getRef(void* self RD9_THUNK_EDX) { return static_cast<TextureAtlas*>(self)->refCount; }

// (34 B), the release: `r = --refCount; if (!r) { sub_10015930(this);
// sub_1005F360(this); } return r;`. The StaticMeshRenderer's destructor calls it on its ten lightmap
// atlases (0x10121C10..0x10121C1D).
int RD9_THUNK atlas_slot08(void* self RD9_THUNK_EDX)
{
    TextureAtlas* const atlas = static_cast<TextureAtlas*>(self);
    const int remaining = --atlas->refCount;
    if (remaining == 0)
    {
        textureAtlasDtor(atlas);
        ::renddx9abi::engineFree(atlas);
    }
    return remaining;
}

void* RD9_THUNK atlas_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return iid == RD9_IID_IBASE ? self : 0;                // 0x10025930, comdat-shared
}

void* const kTextureAtlasVtable[4] =
{
    reinterpret_cast<void*>(&atlas_addRef),          // +0x00
    reinterpret_cast<void*>(&atlas_getRef),          // +0x04
    reinterpret_cast<void*>(&atlas_slot08),          // +0x08
    reinterpret_cast<void*>(&atlas_queryInterface)   // +0x0C
};


// The parser's scanner. The shipped body reads through an istringstream - operator>>(string&) for
// the two tokens, operator>>(uint&) once, then four operator>>(float&) each preceded by a single
// raw get(char) that eats the separator (read from the disassembly at 0x10015C2F..0x10015CAF).
// These little readers reproduce those exact semantics over the line buffer: token/number reads
// skip leading whitespace; get() consumes exactly one character; a failed conversion stops the
// whole tail, as a set failbit would.

struct LineScanner
{
    const char* cur;
    const char* end;
    bool        failed;

    LineScanner(const char* b, const char* e) : cur(b), end(e), failed(false) {}

    void skipWs()
    {
        while (cur < end && (*cur == ' ' || *cur == '\t' || *cur == '\v' || *cur == '\f'))
            ++cur;
    }

    bool readToken(std::string& out)
    {
        if (failed) return false;
        skipWs();
        const char* start = cur;
        while (cur < end && *cur != ' ' && *cur != '\t' && *cur != '\v' && *cur != '\f')
            ++cur;
        if (cur == start) { failed = true; return false; }
        pathAssignSubstring(out, start, static_cast<unsigned int>(cur - start));
        return true;
    }

    void readUInt(unsigned int& out)
    {
        if (failed) return;
        skipWs();
        const char* start = cur;
        unsigned int v = 0;
        while (cur < end && *cur >= '0' && *cur <= '9') { v = v * 10 + (*cur - '0'); ++cur; }
        if (cur == start) { failed = true; return; }
        out = v;
    }

    void getChar()
    {
        if (failed) return;
        if (cur < end) ++cur; else failed = true;
    }

    void readFloat(float& out)
    {
        if (failed) return;
        skipWs();
        char* stop = 0;
        const double v = ::strtod(cur, &stop);
        if (stop == cur || stop > end) { failed = true; return; }
        cur = stop;
        out = static_cast<float>(v);
    }
};

} // anonymous namespace


// sub_10015E60 - the constructor: vptr, refcount 1 and an empty entry map.

TextureAtlas* textureAtlasCtor(void* storage)
{
    TextureAtlas* a = new (storage) TextureAtlas();
    a->vptr     = const_cast<void**>(kTextureAtlasVtable);
    a->refCount = 1;
    return a;
}

unsigned int textureAtlasEntryCount(const TextureAtlas* atlas)
{
    return static_cast<unsigned int>(atlas->entries.size());
}


// Loads normalized source-name entries and retains one page reference per entry.
// Lines contain source, page, ignored index and UV rectangle; '#' lines are skipped.

bool textureAtlasLoadTAI(TextureAtlas* atlas, const std::string& path)
{
    // the first call: every entry's page reference released and the map emptied
    // (sub_100157A0), so a reload never stacks a second set of entries. (em_unload
    // verifier; on the paths recovered so far the atlas is already empty here.)
    textureAtlasReleaseAll(atlas);

    ::renddx9abi::EngineStream file;
    if (!file.open(path))
    {
        // TextureAtlas.cpp:62, severity 6 - `Couldn't open TAI file "<path>"`.
        ::renddx9abi::logf("TextureAtlas.cpp:62: Couldn't open TAI file \"%s\"", path.c_str());
        return false;
    }

    const int size = file.getSize();
    if (size > 0)
    {
        char* buf = static_cast<char*>(::renddx9abi::engineAlloc(static_cast<unsigned int>(size) + 2));
        if (file.read(buf, static_cast<unsigned int>(size)) != static_cast<unsigned int>(size))
        {
            // TextureAtlas.cpp:75 - `Couldn't read TAI file "<path>"`. The shipped early-out
            // leaks the buffer (no sub_1005F370 on this path); reproduced as-is because the path
            // is unreachable on an intact archive and the leak is the shipped behaviour.
            ::renddx9abi::logf("TextureAtlas.cpp:75: Couldn't read TAI file \"%s\"", path.c_str());
            return false;
        }
        buf[size]     = '\n';
        buf[size + 1] = '\0';

        const char* cur = buf;
        const char* bufEnd = buf + size + 1;
        int entries = 0;

        while (cur < bufEnd)
        {
            // getline(v33, 1023): up to 1023 chars, the '\n' consumed and not stored.
            const char* lineStart = cur;
            const char* lineEnd   = lineStart;
            while (lineEnd < bufEnd && *lineEnd != '\n' &&
                   (lineEnd - lineStart) < 1023)
                ++lineEnd;
            cur = (lineEnd < bufEnd && *lineEnd == '\n') ? lineEnd + 1 : lineEnd;

            // the trailing-'\r' erase, then the empty-line skip
            if (lineEnd > lineStart && lineEnd[-1] == '\r')
                --lineEnd;
            if (lineEnd == lineStart)
                continue;

            LineScanner s(lineStart, lineEnd);

            std::string key;
            if (!s.readToken(key))
                continue;
            if (::strcmp(key.c_str(), "#") == 0)           // unk_101DE48C - the comment sentinel
                continue;

            std::string pageFile;
            unsigned int index = 0;
            float uOffset = 0.0f, vOffset = 0.0f, width = 0.0f, height = 0.0f;

            s.readToken(pageFile);
            s.readUInt(index);        // read and discarded, as shipped
            s.getChar();
            s.readFloat(uOffset);
            s.getChar();
            s.readFloat(vOffset);
            s.getChar();
            s.readFloat(width);
            s.getChar();
            s.readFloat(height);
            // On a malformed line the shipped code stores whatever its uninitialised stack slots
            // held (istream leaves the targets untouched on failure). Ours stores the zeros above -
            // a bounded divergence that only a malformed .tai can observe, chosen over reproducing
            // stack garbage.

            // The page texture, through the by-name loader - ecx = dword_1023B974 at 0x10015CDC.
            EngineTexture* page =
                static_cast<EngineTexture*>(textureManagerGetTexture(pageFile));

            pathToLower(key);
            pathReplaceSeparators(key, '/');               // sub_100751C0 -> sub_10075730
            std::string stripped;
            pathStripExtension(stripped, key);
            key = stripped;

            // operator[]: a repeated name overwrites the existing entry.
            TextureAtlasEntry& rec = atlas->entries[key];
            rec.width   = width;                           // the shipped re-ordering
            rec.height  = height;
            rec.uOffset = uOffset;
            rec.vOffset = vOffset;
            rec.page    = page;
            ++entries;
        }

        ::renddx9abi::engineFree(buf);
        ::renddx9abi::logf("TextureAtlas: \"%s\" parsed - %d entries", path.c_str(), entries);
    }

    return true;                                           // the shipped tail returns 1 even for size <= 0
}


// sub_10015930 - the destructor. Restores the vptr, releases the page references
// (sub_100157A0, textureAtlasReleaseAll) and destroys the then-empty entry map. Every caller frees
// the storage next.

void textureAtlasDtor(TextureAtlas* atlas)
{
    atlas->vptr = const_cast<void**>(kTextureAtlasVtable);
    textureAtlasReleaseAll(atlas);
    atlas->~TextureAtlas();
}


// Releases one page reference per atlas entry, then clears entries while retaining the head.
// Every entry must hold a valid page; shared pages receive one release per recorded reference.

void textureAtlasReleaseAll(TextureAtlas* atlas)
{
    for (TextureAtlasEntryMap::iterator it = atlas->entries.begin();
         it != atlas->entries.end(); ++it)
    {
        EngineTexture* const page = it->second.page;
        if (--page->refCount == 0)                                             // unguarded
        {
            textureDtor(page);
            ::renddx9abi::engineFree(page);
        }
    }

    // sub_10016D80 over the root, then the head relinked to itself and the size zeroed.
    atlas->entries.clear();
}


// Appends distinct atlas pages in map traversal order, avoiding pages already in the vector.

void textureAtlasCollectPages(TextureAtlas* atlas, std::vector<EngineTexture*>& pages)
{
    for (TextureAtlasEntryMap::iterator it = atlas->entries.begin();
         it != atlas->entries.end(); ++it)
    {
        EngineTexture* const page = it->second.page;
        unsigned int i = 0;
        while (i < pages.size() && pages[i] != page)
            ++i;
        if (i == pages.size())
            pages.push_back(page);
    }
}


// sub_10015DF0: every entry's key (the normalised name loadTAI keyed it by), in map order,
// push_back'ed through vector<string>::push_back. Callers: UndergrowthSystem's buildMeshes
// (0x1014963F) and OverGrowth (0x100CBFEC).

void textureAtlasCollectNames(TextureAtlas* atlas, std::vector<std::string>& names)
{
    for (TextureAtlasEntryMap::const_iterator it = atlas->entries.begin();
         it != atlas->entries.end(); ++it)
        names.push_back(it->first);
}


// sub_100156F0 - the lookup. Normalises exactly as loadTAI keyed (lowercase, '/'-separators,
// extension stripped), then lower_bound + the miss check; a hit returns the entry, a miss 0.

TextureAtlasEntry* textureAtlasFind(TextureAtlas* atlas, const std::string& name)
{
    std::string key(name);
    pathToLower(key);
    pathReplaceSeparators(key, '/');
    std::string stripped;
    pathStripExtension(stripped, key);
    key = stripped;

    TextureAtlasEntryMap::iterator it = atlas->entries.find(key);
    if (it == atlas->entries.end())
        return 0;
    return &it->second;
}


// Texture-atlas generation and builder-DLL integration.


namespace {

// std::operator+(const string&, const char* / const string&) - VC7.1 inlines both as `copy the left,
// then +=` (0x100161A4..0x100161EC).
std::string concat(const std::string& left, const char* right)
{
    std::string result(left);
    result += right;
    return result;
}

std::string concat(const std::string& left, const std::string& right)
{
    std::string result(left);
    result += right;
    return result;
}

// the string BY value, every character through MSVCR71's toupper
// sign-extended (`movsx`), end() re-read on every pass, returned by value through EBX (/GL). The same
// body as the DLL's own 0x10008360. std::string has no mutable access, so the characters are
// upper-cased in a scratch copy and assigned back - the result is the shipped one.
std::string toUpper(std::string s)
{
    std::string chars(s.c_str(), s.size());
    for (std::string::iterator it = chars.begin(); it != chars.end(); ++it)
        *it = static_cast<char>(toupper(*it));
    pathAssignSubstring(s, chars.c_str(), static_cast<unsigned int>(chars.size()));
    return s;
}

// nullsub_2 (0x100156D0) / nullsub_3 (0x100156E0) - the error and the warning sink the builder hands
// both objects: a bare `retn`, the message never read. That is why the string's STL generation does
// not matter here (TextureAtlasBuilder/ITextureAtlas.h): this image may load the retail DLL (VC7.1
// strings) or the rebuilt one.
void __fastcall discardAtlasBuilderError(const std::string& /*message*/)
{
}

void __fastcall discardAtlasBuilderWarning(const std::string& /*message*/)
{
}

// The map's comparator, inlined in its insert (0x10016C60): `stricmp(a.c_str(), b.c_str()) < 0`
// through MSVCR71's stricmp - lower-casing, unsigned byte order. The ucrt's _stricmp orders the same
// in the "C" locale, the only one this image runs in.
struct NameLessNoCase
{
    bool operator()(const std::string& a, const std::string& b) const
    {
        return ::_stricmp(a.c_str(), b.c_str()) < 0;
    }
};

} // anonymous namespace


// Builds an atlas from loose files beneath inputDirectory; relative paths use the cwd.
// Names are deduplicated case-insensitively, preserving the first spelling.
// Returns false only for an empty input list; build and save failures are not propagated.
// dxt5 selects the output compression group. Atlas resources stay owned by the DLL.

bool textureAtlasBuild(TextureAtlas* /*thisAtlas - never read*/, std::string outputDirectory,
                       std::string atlasName, std::string inputDirectory,
                       const std::list<std::string>& textures, bool dxt5)
{
    if (textures.size() == 0)
        return false;

    std::map<std::string, int, NameLessNoCase> uniqueNames;
    for (std::list<std::string>::const_iterator it = textures.begin(); it != textures.end(); ++it)
        uniqueNames.insert(std::make_pair(*it, 0));

    const std::string format(dxt5 ? "DXT5" : "");

    if (inputDirectory.size() != 0)
    {
        const char last = inputDirectory.c_str()[inputDirectory.size() - 1];
        if (last != '/' && last != '\\')
            inputDirectory += "/";
    }
    if (outputDirectory.size() != 0)
    {
        const char last = outputDirectory.c_str()[outputDirectory.size() - 1];
        if (last != '/' && last != '\\')
            outputDirectory += "/";
    }

    // name.rfind('.') - the last '.', searched over the whole counted string.
    const unsigned int nameSize = static_cast<unsigned int>(atlasName.size());
    unsigned int dot = nameSize;
    for (unsigned int i = nameSize; i-- > 0; )
    {
        if (atlasName.c_str()[i] == '.')
        {
            dot = i;
            break;
        }
    }
    if (dot != nameSize)
    {
        std::string suffix;                                                      // name.substr(dot)
        pathAssignSubstring(suffix, atlasName.c_str() + dot, nameSize - dot);
        const std::string upper = toUpper(suffix);
        // std::operator==(const string&, const char*): the whole string against ".TAC".
        if (upper.size() == 4 && ::memcmp(upper.c_str(), ".TAC", 4) == 0)
        {
            std::string stem;                                                    // name.substr(0, dot)
            pathAssignSubstring(stem, atlasName.c_str(), dot);
            atlasName = stem;
        }
    }

    textureatlas::ITextureAtlasConfiguration* const configuration =
        textureatlas::ITextureAtlasConfiguration::create();
    configuration->setErrorCallback(discardAtlasBuilderError);
    configuration->setWarningCallback(discardAtlasBuilderWarning);
    configuration->setInputPath(inputDirectory.c_str());
    configuration->setOutputPath(outputDirectory.c_str());
    configuration->addGroup(atlasName.c_str(), 0, 0, format.c_str(), 0, 1);
    for (std::map<std::string, int, NameLessNoCase>::const_iterator it = uniqueNames.begin();
         it != uniqueNames.end(); ++it)
        configuration->addTexture(it->first.c_str(), false);

    textureatlas::ITextureAtlas* const builder = textureatlas::ITextureAtlas::create(
        static_cast<RendDX9Base*>(g_rendererBase)->device);
    builder->setErrorCallback(discardAtlasBuilderError);
    builder->setWarningCallback(discardAtlasBuilderWarning);
    builder->build(configuration);
    builder->save(concat(concat(outputDirectory, atlasName), ".tai").c_str());

    delete builder;
    delete configuration;
    return true;
}


// (104 B, __thiscall, `retn 5Ch`) - copies its three strings into the builder's by-value
// slots and forwards; `this` passes through unread.

bool textureAtlasGenerate(TextureAtlas* atlas, std::string outputDirectory, std::string atlasName,
                          std::string inputDirectory, const std::list<std::string>& textures, bool dxt5)
{
    return textureAtlasBuild(atlas, outputDirectory, atlasName, inputDirectory, textures, dxt5);
}

// generate("", name, "", textures, false). No caller in the 1.50 image.
bool textureAtlasGenerate(TextureAtlas* atlas, const std::string& atlasName,
                          const std::list<std::string>& textures)
{
    return textureAtlasGenerate(atlas, std::string(""), atlasName, std::string(""), textures, false);
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
