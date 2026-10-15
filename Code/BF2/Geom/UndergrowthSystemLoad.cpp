// Reads/writes grass density data and builds mesh/atlas resources.
// [CHOSEN, not recovered] split from UndergrowthSystem.cpp; method names are [INFERRED].
// [CHOSEN, not recovered] diagnostics use the renderer log without bridging the debug dispatcher.
// Float computations assume the renderer's 24-bit x87 precision (RW-240); generated colour
// bytes can differ from files created by an editor running at extended precision.

#ifdef BF2_IMAGE_RENDDX9

#include "UndergrowthSystem.h"

#include "MeshTemplate.h"                 // MeshBounds, meshBoundsExpand / meshBoundsFinalize
#include "Terrain.h"                      // geom::Terrain - the "Terrain" singleton (ITerrain 820004)
#include "../IO/Tokenizer.h"
#include "../Physics/Heightmap.h"
#include "../Physics/HeightmapCluster.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendDX9/TextureAtlas/TextureAtlas.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>
#include "../Core/SettingsRepostitory.h"  // settings->stringGet, typed (the XpLightManager precedent)

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {

// Declared per TU, as every other client of these does (NameTagRenderer.cpp, MeshTemplate.cpp).
extern void* g_shaderManager;                                              // dword_1023B97C
extern void* g_bufferManager;                                              // dword_1023B978
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const std::string& name, unsigned int usage);
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);

// TextureAtlas.cpp owns the two atlas walks this path calls: textureAtlasCollectPages (sub_10015850)
// and textureAtlasCollectNames (sub_10015DF0), declared in TextureAtlas.h.

} // namespace dx9rend

namespace geom {

namespace {


// Case-sensitive token comparison; keyword matching uses a separate rule.
bool tokenEquals(const Token& token, const char* text)
{
    return std::strcmp(token.text.c_str(), text) == 0;
}

// (287 B) / ELF 0x84395F0: the number starting at `token`, consuming its tokens. A sign
// token is glued to the next token; a following "." token glues "." and the token after it (the
// scanner splits "1.5" into "1", ".", "5"). atof of the text, narrowed to float (`fstp dword` then
// `fld dword`, both images).
float tokenizerGetNumber(Token& token)
{
    std::string number(token.text.c_str());
    ++token;
    if (number == "-" || number == "+")
    {
        number += token.text.c_str();
        ++token;
    }
    if (tokenEquals(token, "."))
    {
        ++token;
        number += std::string(".") + token.text.c_str();
        ++token;
    }
    return static_cast<float>(std::atof(number.c_str()));
}

// (155 B) / ELF 0x84397E0: the sign rule of getNumber, no fraction, atoi.
int tokenizerGetIntNumber(Token& token)
{
    std::string number(token.text.c_str());
    ++token;
    if (number == "-" || number == "+")
    {
        number += token.text.c_str();
        ++token;
    }
    return std::atoi(number.c_str());
}

// sub_10075080 (63 B, fastcall ecx/edx): two std::strings equal under _stricmp (MSVCR71 `stricmp`).
// Every keyword test builds std::string(keyword) and std::string(token.text.c_str()) and hands both
// over; the tree's NametagManagerConsole.cpp carries the same body under the same name.
bool equalsNoCase(const char* a, const char* b)
{
    return ::_stricmp(a, b) == 0;
}

bool isKeyword(const Token& token, const char* keyword)
{
    return equalsNoCase(token.text.c_str(), keyword);
}

// sub_1000ED40 (34 B, __stdcall, `retn 0Ch`): fld low; fld high; fld value; fcomi st,st1;
// fcmovnb st,st1; fcomi st,st2; fcmovb st,st2. The high bound first; an unordered compare sets CF, so
// a NaN comes back as `low`. The answer is re-loaded from its float spill. (Same reading as
// NametagManagerUpdate.inl's nametagClampFloat and ../ug_class's clampRange.)
float clampRange(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// lcgrand_step (0x1009EF30) - RendDX9's copy of dice::hfe::Random::getRaw (ELF 0x8166CE0): two
// Schrage steps, 24112 (0x5E30) then 26143 (0x661F), the state written back. Core/Random.cpp is not
// in the RendDX9 manifest, so the arithmetic is reproduced (NametagManager.cpp does the same).
unsigned int lehmerStep(unsigned int value, unsigned int multiplier)
{
    const unsigned int low = (value & 0xFFFFu) * multiplier;
    const unsigned int high = static_cast<unsigned int>(static_cast<int>(value) >> 16) * multiplier;
    const int middle = static_cast<int>((low >> 16) + high);
    const unsigned int next = (low & 0xFFFFu) + ((static_cast<unsigned int>(middle) & 0x7FFFu) << 16)
        + static_cast<unsigned int>(middle >> 15);
    const int reduced = static_cast<int>(next - 0x7FFFFFFFu);
    return reduced < 0 ? next : static_cast<unsigned int>(reduced);
}

unsigned int randomGetRaw(Random& random)
{
    random.m_seed = lehmerStep(lehmerStep(random.m_seed, 24112u), 26143u);
    return random.m_seed;
}

// std::string operator+(const string&, const char*) / (const string&, const string&) - the shipped
// code inlines MSVCP71's: copy-construct, operator+=, copy the result out. std::string publishes no
// operator+, so these carry the same three steps. [CHOSEN, not recovered] helpers.
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

// The dice smart pointers' assignment from a raw pointer, as the load inlines it for the effect
// (0x1014CDEB..0x1014CDFC) and every parameter: release the old one through vtable +0x08, null the
// slot, store the new one - without taking a reference. getEffect hands back a counted reference;
// the parameters come from the borrowing lookup 0x1001E3A0 and are addRef'd afterwards.
void assignEffect(dx9rend::Effect*& slot, dx9rend::Effect* value)
{
    if (slot)
    {
        dx9rend::effectRelease(slot);                                        // vtable +0x08
        slot = 0;
    }
    slot = value;
}

void bindParameter(dx9rend::ShaderParameter*& slot, dx9rend::Effect* effect, const char* name)
{
    dx9rend::ShaderParameter* const parameter =
        dx9rend::effectGetShaderParameterBorrowedTyped(effect, std::string(name));
    if (slot)
    {
        slot->release();                                                     // vtable +0x08
        slot = 0;
    }
    slot = parameter;
    slot->addRef();                                                          // vtable +0x04, unguarded
}

// sub_1014DFD0 (71 B) - SmartPtr<Texture>::operator=(Texture*): the old texture released with the
// inline refcount dance (`if (!--t->refCount) { sub_10014E20(t); sub_1005F360(t); }`), the slot nulled,
// the new pointer stored - NO reference taken (the load adds one itself). Other callers: 0x10176D20,

void assignTexture(dx9rend::EngineTexture*& slot, dx9rend::EngineTexture* value)
{
    if (slot)
        dx9rend::textureReleaseRef(slot);                                    // --ref; dtor + free; null
    slot = value;
}

// Initializes zero bounds and marks derived values dirty.
void initZeroBounds(MeshBounds& bounds)
{
    bounds.min.x = 0.0f; bounds.min.y = 0.0f; bounds.min.z = 0.0f;
    bounds.max.x = 0.0f; bounds.max.y = 0.0f; bounds.max.z = 0.0f;
    bounds.originRadius = 0.0f;
    bounds.radius = 0.0f;
    bounds.dirty = 1;
    bounds.extent.x = 0.0f; bounds.extent.y = 0.0f; bounds.extent.z = 0.0f;
    bounds.center.x = 0.0f; bounds.center.y = 0.0f; bounds.center.z = 0.0f;
}

void initEmptyBounds(MeshBounds& bounds)
{
    bounds.min.x = FLT_MAX;  bounds.min.y = FLT_MAX;  bounds.min.z = FLT_MAX;
    bounds.max.x = -FLT_MAX; bounds.max.y = -FLT_MAX; bounds.max.z = -FLT_MAX;
    bounds.extent.x = 0.0f; bounds.extent.y = 0.0f; bounds.extent.z = 0.0f;
    bounds.center.x = 0.0f; bounds.center.y = 0.0f; bounds.center.z = 0.0f;
    bounds.dirty = 1;
}


// Host-call adapters (RW-266).


// g_hfeClassManager->getSingleton(std::string("HeightmapCluster")) - ClassManager +0x48, typed in
// both flavours through HfeAbi.h's IClassManager (HostString carrier); the cluster and heightmap
// interfaces are typed in both flavours, as Terrain.cpp and XpLightManager.cpp already call them.
world::IHeightmapCluster* heightmapCluster()
{
    return static_cast<world::IHeightmapCluster*>(static_cast<void*>(
        ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("HeightmapCluster"))));
}

// ClassManager +0x4C (ELF +0x50 _ZN...14getSingletonAsERKSsj - `void* getSingletonAs(const
// std::string&, unsigned int)`), 0x1014D747 with the ITerrain iid dword_1021AE18 = 820004.
// RAW in both flavours: HfeAbi.h's IClassManager - which both flavours dispatch through, with the
// HostString carrier - declares this slot `reserved_getSingletonAs()`. The offset is the same table
// the typed getSingleton (+0x48) already trusts; the carrier is HostString, so the string that
// crosses is the host's in each flavour. [CHOSEN, not recovered] seam - retire it by typing the slot
// in HfeAbi.h: `virtual void* getSingletonAs(const renddx9abi::HostString&, unsigned int) = 0;`.
void* classManagerGetSingletonAs(const renddx9abi::HostString& name, unsigned int iid)
{
    typedef void* (__thiscall* GetSingletonAsFn)(void*, const renddx9abi::HostString&, unsigned int);
    void* const manager = ::dice::hfe::classManager;
    return reinterpret_cast<GetSingletonAsFn>((*reinterpret_cast<void***>(manager))[0x4C / 4])(
        manager, name, iid);
}

// SettingsRepostitory +0x40, stringGet(const std::string& key, const std::string& fallback) ->
// const std::string& (the result is copy-constructed at once). A std::string crosses, so:
// native typed, compat raw - XpLightManager.cpp's simpleShadowStringSetting is the same seam.
const renddx9abi::HostString& settingsStringGet(const renddx9abi::HostString& key,
                                                const renddx9abi::HostString& fallback)
{
    return ::dice::hfe::settings->stringGet(key, fallback);
}

// FileManager +0x18 open(const std::string&, FileMode, FileAccess, unsigned int) - typed in both
// flavours (HostFileAbi.h; EngineFile.cpp is the precedent). The load path uses (2, 0) to read and
// (0, 2) to write.
renddx9abi::HostStream* openFile(const std::string& name, int mode, int access)
{
    return ::dice::hfe::fileManager->open(renddx9abi::makeHostString(name),
                                          static_cast<renddx9abi::HostFileMode>(mode),
                                          static_cast<renddx9abi::HostFileAccess>(access), 0);
}

// sub_101501E0 (182 B, fastcall ecx = stream, edx = string&), read in the instruction stream: a
// signed length, then the characters through a std::vector<char> buffer resize(length, 0)'d
// (0x1014F500 is vector<char>::resize). A length <= 0 empties the string and succeeds; a short read
// empties it and fails. `erase(0, npos)` and `assign(p, n)` are the shipped calls. The callers ignore
// the answer. [INFERRED] name.
bool readString(renddx9abi::HostStream* stream, std::string& text)
{
    int length = 0;
    stream->read(&length, 4);                                                // stream +0x10
    if (length <= 0)
    {
        text = "";                                                           // erase(0, npos)
        return true;
    }
    std::vector<char> buffer;
    buffer.resize(static_cast<unsigned int>(length), 0);
    if (stream->read(&buffer[0], static_cast<unsigned int>(length)) != static_cast<unsigned int>(length))
    {
        text = "";                                                           // erase(0, npos)
        return false;
    }
    text = std::string(&buffer[0], static_cast<unsigned int>(length));      // assign(p, n)
    return true;
}

// The level's own files, relative to the level path load() was handed.
const char* const kRawDataDirectory = "../RawData/Undergrowth/";

} // anonymous namespace


// UndergrowthType's constructor: the three strings, CrossSize (1, 1),
// RandomSizeScale (1, 1), SlopeRange (0, 1), Skew false, Density 0.01 (0x3C23D70A), Variation 0.2
// (0x3E4CCCCD), TerrainColorScale 1, TypeSwayScale 1, and the three vectors' pointers zeroed (their
// allocator words are not written). Callers: the cfg reader's "Type" arm 0x1014BC76 and loadDat's
// resize temporary 0x1014B6F4.

UndergrowthType::UndergrowthType()
    : density(0.01f)
    , variation(0.2f)
    , terrainColorScale(1.0f)
    , typeSwayScale(1.0f)
    , skew(false)
{
    size.x = 1.0f;
    size.y = 1.0f;
    randomSizeScale.x = 1.0f;
    randomSizeScale.y = 1.0f;
    slopeRange.x = 0.0f;
    slopeRange.y = 1.0f;
}


// Generates sixteen sway speed, amplitude, and phase entries from the object's RNG.
// Runs before configuration parsing, so new Sway settings affect the next load.

void UndergrowthSystem::initSwayTables()
{
    for (int i = 0; i < 16; ++i)
    {
        int r = (static_cast<int>(randomGetRaw(m_random) | 0x80u) >> 7) + 1;
        m_windSpeed[i] = static_cast<float>(r) * m_swaySpeedDiff * 5.96046448e-08f + m_swaySpeed;
        r = (static_cast<int>(randomGetRaw(m_random) | 0x80u) >> 7) + 1;
        m_windAmplitude[i] = static_cast<float>(r) * m_swayScaleDiff * 5.96046448e-08f + m_swayScale;
        r = (static_cast<int>(randomGetRaw(m_random) | 0x80u) >> 7) + 1;
        m_windPhase[i] = static_cast<float>(r) * 3.74507039e-07f;
    }
}


// Replaces level undergrowth from configuration, baked data/atlas or rebuilt meshes.
// Copies the input before clearing old state and marks loaded only after buffers exist.
// A failed configuration read leaves the system unloaded.

void UndergrowthSystem::load(const std::string& levelPathArgument)
{
    std::string levelPath(levelPathArgument);
    bool rebuild = false;                                                    // [ebp+arg_0+3]

    if (m_loadStarted)
        clear();
    m_loadStarted = true;

    if (m_editorMode)
    {
        if (levelPath.empty())                                               // operator==(path, "")
        {
            levelPath = m_levelPath;
            rebuild = true;
        }
        else if (!m_levelPath.empty())                                       // operator!=(m_path, "")
        {
            rebuild = true;
        }
        else
        {
            m_levelPath = levelPath;
        }
    }

    initSwayTables();

    Tokenizer tokenizer;
    if (!tokenizer.init(concat(levelPath, "Undergrowth.cfg").c_str()))
        return;

    // Undergrowth.cfg
    // One keyword per step, tested in this order (each through equalsNoCase except "}", which is
    // the case-sensitive token compare). The current material and type are plain pointers into the
    // vectors: neither is null-tested by the arms that use them (a "Type" before any "Material", or
    // a "Mesh" outside a Type, faults in retail - preserved), and a "Material" does NOT reset the
    // current type.
    UndergrowthMaterial* material = 0;                                       // [ebp-84h]
    UndergrowthType*     type = 0;                                           // [ebp-0Ch]
    {
        // The loop runs to the tokenizer's END token and compares line and column only
        // (Tokenizer.h's operator!=).
        Token token = tokenizer.token(0);
        while (token != tokenizer.token(tokenizer.count()))
        {
            if (isKeyword(token, "Material"))
            {
                // "Material <name> <id> {": the name, a fresh record pushed and taken from the back,
                // the id read (it consumes its token), then the "{" stepped over.
                ++token;
                const std::string name(token.text.c_str());
                ++token;
                m_materials.push_back(UndergrowthMaterial());
                material = &m_materials.back();
                material->name = name;
                material->id = tokenizerGetIntNumber(token);
                ++token;
            }
            else if (isKeyword(token, "Type"))
            {
                // "Type <name> {".
                ++token;
                const std::string name(token.text.c_str());
                ++token;
                ++token;
                material->types.push_back(UndergrowthType());
                type = &material->types.back();
                type->name = name;
            }
            else if (tokenEquals(token, "}"))
            {
                ++token;
                if (type)
                    type = 0;
                else if (material)
                    material = 0;
                else
                    ::renddx9abi::logf("UndergrowthSystem.cpp:489 Error");  // 0x1E9
            }
            else if (isKeyword(token, "GeneralHeight"))
            {
                ++token;
                material->generalHeight = tokenizerGetNumber(token);         // fstp [edx+20h]
            }
            else if (isKeyword(token, "Mesh"))
            {
                ++token;
                type->mesh = token.text.c_str();                             // operator=(const char*)
                ++token;
            }
            else if (isKeyword(token, "Texture"))
            {
                ++token;
                type->texture = token.text.c_str();
                ++token;
            }
            else if (isKeyword(token, "CrossSize") || isKeyword(token, "Size"))
            {
                ++token;
                type->size.x = tokenizerGetNumber(token);                    // fstp [ebx+54h]
                type->size.y = tokenizerGetNumber(token);                    // fstp [ebx+58h]
            }
            else if (isKeyword(token, "RandomSizeScale"))
            {
                ++token;
                type->randomSizeScale.x = tokenizerGetNumber(token);
                type->randomSizeScale.y = tokenizerGetNumber(token);
            }
            else if (isKeyword(token, "SlopeRange"))
            {
                ++token;
                type->slopeRange.x = tokenizerGetNumber(token);
                type->slopeRange.y = tokenizerGetNumber(token);
            }
            else if (isKeyword(token, "Density"))
            {
                // Hex-Rays shows the unclamped value stored; the stream (push 10.0,
                // push 0.01, push v; call; fstp [ebx+6Ch]) stores the clamp's x87 return.
                ++token;
                type->density = clampRange(tokenizerGetNumber(token), 0.01f, 10.0f);
            }
            else if (isKeyword(token, "Variation"))
            {
                ++token;
                type->variation = clampRange(tokenizerGetNumber(token), -1.0f, 1.0f);
            }
            else if (isKeyword(token, "TerrainColorScale"))
            {
                ++token;
                type->terrainColorScale = tokenizerGetNumber(token);
            }
            else if (isKeyword(token, "TypeSwayScale"))
            {
                ++token;
                type->typeSwayScale = tokenizerGetNumber(token);
            }
            else if (isKeyword(token, "Skew"))
            {
                ++token;
                type->skew = tokenizerGetIntNumber(token) != 0;             // setnz
            }
            else if (isKeyword(token, "Material"))
            {
                // Unreachable: the earlier "Material" branch consumes this token.
                // Only loadDat fills this list.
                ++token;
                const int id = tokenizerGetIntNumber(token);
                type->materials.push_back(id);
            }
            else if (isKeyword(token, "ViewDistance"))
            {
                ++token;
                m_viewDistance = tokenizerGetNumber(token);                  // +0x30
            }
            else if (isKeyword(token, "ViewDistanceFadeScale"))
            {
                ++token;
                m_viewDistanceFadeScale = tokenizerGetNumber(token);         // +0x34
            }
            else if (isKeyword(token, "ViewDistanceHeightScale"))
            {
                ++token;
                m_viewDistanceHeightScale = tokenizerGetNumber(token);       // +0x38
            }
            else if (isKeyword(token, "ViewDistanceHeight2Scale"))
            {
                ++token;
                m_viewDistanceHeight2Scale = tokenizerGetNumber(token);      // +0x3C
            }
            else if (isKeyword(token, "ViewDistanceStreamingScale"))
            {
                ++token;
                m_viewDistanceStreamingScale = tokenizerGetNumber(token);    // +0x40
            }
            else if (isKeyword(token, "PatchSubdivide"))
            {
                // Read and discarded (the int is not stored). +0x44 keeps the
                // constructor's 15 whatever the level says; the load reads +0x44 below.
                ++token;
                tokenizerGetIntNumber(token);
            }
            else if (isKeyword(token, "SwaySpeed"))
            {
                ++token;
                m_swaySpeed = tokenizerGetNumber(token);                     // +0x48
            }
            else if (isKeyword(token, "SwaySpeedDiff"))
            {
                ++token;
                m_swaySpeedDiff = tokenizerGetNumber(token);                 // +0x4C
            }
            else if (isKeyword(token, "SwayScale"))
            {
                ++token;
                m_swayScale = tokenizerGetNumber(token);                     // +0x50
            }
            else if (isKeyword(token, "SwayScaleDiff"))
            {
                ++token;
                m_swayScaleDiff = tokenizerGetNumber(token);                 // +0x54
            }
            else if (isKeyword(token, "LightingScale"))
            {
                ++token;
                m_lightingScale = tokenizerGetNumber(token);                 // +0xDC
            }
            else if (isKeyword(token, "AlphaRef"))
            {
                ++token;
                m_alphaRef = tokenizerGetNumber(token);                      // +0x108
            }
            else
            {
                ::renddx9abi::logf("UndergrowthSystem.cpp:625 Unknown token \"%s\"",   // 0x271
                                   token.text.c_str());
                ++token;
            }
        }
    }                                                                        // 0x1014CA04: ~token

    // the types' data and the atlas
    const int savedMipClass = dx9rend::textureManagerSwapDefaultMipClass(8);
    if (rebuild || !loadDat(levelPath))                                      // 0x1014CA1A / CA2C
    {
        // The rebuild branch: the editor, or a level with no Undergrowth.dat. Its output directory is
        // the editor's save path - " ERROR " in a game, where nobody sets that key.
        const std::string savePath(renddx9abi::fromHostString(settingsStringGet(
            renddx9abi::HostString("Editor_LevelSavePath"), renddx9abi::HostString(" ERROR "))));

        if (!dx9rend::textureAtlasLoadTAI(m_atlas, concat(levelPath, "UndergrowthAtlas.tai")))
        {
            buildAtlas(savePath, levelPath);
            if (!dx9rend::textureAtlasLoadTAI(m_atlas, concat(levelPath, "UndergrowthAtlas.tai")))
                ::renddx9abi::logf("UndergrowthSystem.cpp:645 Error loading undergrowth texture atlas");
        }

        if (!buildMeshes(false))                                             // sub_10148FC0(0)
        {
            // The atlas no longer matches the types: every page handed back to the texture cache,
            // the atlas rebuilt and re-read, the meshes rebuilt. (No releaseAll here - loadTAI does
            // that itself first.)
            if (m_atlas)
            {
                std::vector<dx9rend::EngineTexture*> pages;
                dx9rend::textureAtlasCollectPages(m_atlas, pages);
                for (unsigned int i = 0; i < pages.size(); ++i)
                    dx9rend::textureManagerEvictFromCache(pages[i]);
            }
            buildAtlas(savePath, levelPath);
            if (!dx9rend::textureAtlasLoadTAI(m_atlas, concat(levelPath, "UndergrowthAtlas.tai")))
                ::renddx9abi::logf("UndergrowthSystem.cpp:661 Error loading undergrowth texture atlas");
            buildMeshes(true);                                               // sub_10148FC0(1)
        }
        saveDat(savePath, levelPath);
    }
    else
    {
        // sub_100019A0 is operator+(const string&, const char*) by value; the answer is ignored.
        dx9rend::textureAtlasLoadTAI(m_atlas, concat(levelPath, "UndergrowthAtlas.tai"));
    }
    dx9rend::textureManagerSwapDefaultMipClass(savedMipClass);

    // the grass texture: the atlas's first page
    // m_atlas is not tested here (0x1014CD79), unlike the rebuild arm above.
    std::vector<dx9rend::EngineTexture*> pages;
    dx9rend::textureAtlasCollectPages(m_atlas, pages);
    if (pages.empty())
    {
        assignTexture(m_atlasTexture, 0);                                    // sub_1014DFD0(0)
    }
    else
    {
        assignTexture(m_atlasTexture, pages[0]);
        if (m_atlasTexture)
            dx9rend::textureAddRef(m_atlasTexture);                          // shared table +0x08
    }

    // the effect and its parameters, in the shipped order
    assignEffect(m_effect, static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
                               dx9rend::g_shaderManager, "Shaders/Undergrowth")));
    bindParameter(m_paramTexLayer0, m_effect, "TEXLAYER0");                  // +0x6C
    bindParameter(m_paramTexLayer1, m_effect, "TEXLAYER1");                  // +0x70
    bindParameter(m_paramTexLayer2, m_effect, "TEXLAYER2");                  // +0x74
    bindParameter(m_paramWorldViewProjection, m_effect, "WorldViewProjection");   // +0x7C
    bindParameter(m_paramWorldView, m_effect, "WorldView");                  // +0x80
    bindParameter(m_paramPosOffsetAndScale, m_effect, "PosOffsetAndScale");  // +0x84
    bindParameter(m_paramTerrainTexCoordScaleAndOffset, m_effect,
                  "TerrainTexCoordScaleAndOffset");                          // +0x88
    bindParameter(m_paramCameraPos, m_effect, "CameraPos");                  // +0x8C
    bindParameter(m_paramSwayOffset, m_effect, "SwayOffset");                // +0x90
    bindParameter(m_paramFadeAndHeightScaleOffset, m_effect, "FadeAndHeightScaleOffset");   // +0x94
    bindParameter(m_paramSunColor, m_effect, "SunColor");                    // +0x98
    bindParameter(m_paramGIColor, m_effect, "GIColor");                      // +0x9C
    bindParameter(m_paramStaticPointColor, m_effect, "StaticPointColor");    // +0xA0
    bindParameter(m_paramFogColor, m_effect, "fogColor");                    // +0xA8
    bindParameter(m_paramFogRange, m_effect, "fogRange");                    // +0xA4
    bindParameter(m_paramAmbientIlluminationRange, m_effect, "AmbientIlluminationRange");   // +0xAC
    bindParameter(m_paramWaterFogColor, m_effect, "WaterFogColor");          // +0xB4
    bindParameter(m_paramWaterFogRange, m_effect, "WaterFogRange");          // +0xB0
    bindParameter(m_paramPointLightPosAtten, m_effect, "PointLightPosAtten");   // +0xB8
    bindParameter(m_paramPointLightColor, m_effect, "PointLightColor");      // +0xBC
    bindParameter(m_paramAlphaRef, m_effect, "AlphaRef");                    // +0xC0
    bindParameter(m_paramTransparencyX8, m_effect, "TRANSPARENCY_X8");       // +0xC4
    bindParameter(m_paramCameraToWater, m_effect, "CameraToWater");          // +0xC8
    bindParameter(m_paramShadowMap, m_effect, "SHADOWMAP");                  // +0xCC
    bindParameter(m_paramShadowProjMatrix, m_effect, "ShadowProjMatrix");    // +0xD0
    bindParameter(m_paramShadowTrapMatrix, m_effect, "ShadowTrapMatrix");    // +0xD4

    // the density map, sized from the primary heightmap
    m_heightmap = heightmapCluster()->getPrimaryHeightmap();                 // cluster +0x40
    int sizeX = 0;
    int sizeZ = 0;
    m_heightmap->getSize(sizeX, sizeZ);                                      // IHeightmap +0x2C
    m_densityWidth = sizeX;                                                  // +0x578
    m_densityHeight = sizeZ;                                                 // +0x57C
    // Three getScale (+0x54) calls, made in this order (0x1014D5E0, 0x1014D5ED, 0x1014D5FA) - the z
    // from the first, y from the second, x from the third - then the three reciprocals.
    const Vec3& scaleForZ = m_heightmap->getScale();
    const Vec3& scaleForY = m_heightmap->getScale();
    const Vec3& scaleForX = m_heightmap->getScale();
    m_invHeightmapScale.x = 1.0f / scaleForX.x;                              // +0x56C
    m_invHeightmapScale.y = 1.0f / scaleForY.y;                              // +0x570
    m_invHeightmapScale.z = 1.0f / scaleForZ.z;                              // +0x574

    // sub_1005F340 is the engine's operator new[] (no cookie for bytes); the inline rep stosd/stosb
    // is the memset. Freed by clear() through sub_1005F370.
    m_densityMap = new unsigned char[static_cast<unsigned int>(m_densityWidth * m_densityHeight)];
    std::memset(m_densityMap, 0, static_cast<unsigned int>(m_densityWidth * m_densityHeight));
    if (::dice::hfe::fileManager->fileExists(renddx9abi::makeHostString(
            concat(levelPath, "Undergrowth.raw"))))                          // FileManager +0x1C
    {
        const unsigned int bytes = static_cast<unsigned int>(m_densityWidth * m_densityHeight);
        unsigned int bytesRead = 0;                                          // [ebp+arg_0], ignored
        ::dice::hfe::fileManager->readFile(renddx9abi::makeHostString(
            concat(levelPath, "Undergrowth.raw")), m_densityMap, bytes, bytesRead);   // +0x2C
    }

    // the cell grid: every terrain patch cut PatchSubdivide x PatchSubdivide
    Terrain* const terrain = static_cast<Terrain*>(static_cast<IBase*>(classManagerGetSingletonAs(
        renddx9abi::HostString("Terrain"), 820004u)));                      // ClassManager +0x4C
    const unsigned int subdivide = m_patchSubdivide;                         // +0x44
    m_terrain = terrain;                                                     // +0xD8
    m_cellsPerSide = static_cast<unsigned int>(terrain->getPatchCount()) * subdivide;   // Terrain +0x30
    m_cellCount = m_cellsPerSide * m_cellsPerSide;
    m_cells = new UndergrowthCell[m_cellCount];                              // 0x68 * n + 4 (cookie)

    const Vec3* const worldScale = terrain->getPrimaryWorldScale();          // Terrain +0x38
    const float subdivideF = static_cast<float>(subdivide);                  // unsigned -> float
    const int patchSize = terrain->getPatchSize();                           // Terrain +0x34
    m_cellSize = static_cast<float>(patchSize) * worldScale->x / subdivideF; // +0xE8
    // `fdivr [esi+30h]`, ceil (the double CRT one), then fistp under a truncating control word.
    const int radius = static_cast<int>(std::ceil(static_cast<double>(m_viewDistance / m_cellSize)));
    m_patchCount = static_cast<unsigned int>((2 * radius + 1) * (2 * radius + 1) + 4);   // +0xF0
    m_patches = new UndergrowthPatch[m_patchCount];                          // 0x30 * n + 4 (cookie)

    // the per-patch buffer sizes: the busiest material at full density
    m_patchVertexCount = 0;                                                  // +0x58
    m_patchIndexCount = 0;                                                   // +0x5C
    if (m_materials.size() == 0)
        return;

    // Byte +0x581 (simpleShaderEnable): `setnz cl; lea ecx, [ecx*8+10h]` - 24 bytes a vertex for the
    // simple shader, 16 otherwise.
    const unsigned int vertexStride = (m_simpleShaderEnable ? 1u : 0u) * 8u + 16u;
    for (unsigned int m = 0; m < m_materials.size(); ++m)
    {
        const UndergrowthMaterial& current = m_materials[m];
        unsigned int vertices = 0;
        unsigned int indices = 0;
        for (unsigned int t = 0; t < current.types.size(); ++t)
        {
            const UndergrowthType& each = current.types[t];
            // density * cellSize * cellSize, truncated (fistp under the truncating control word),
            // computed twice as shipped.
            vertices += each.vertices.size() *
                        static_cast<unsigned int>(static_cast<int>(each.density * m_cellSize * m_cellSize));
            indices += each.indices.size() *
                       static_cast<unsigned int>(static_cast<int>(each.density * m_cellSize * m_cellSize));
        }
        if (vertices > m_patchVertexCount)                                   // unsigned (jbe)
            m_patchVertexCount = vertices;
        if (indices > m_patchIndexCount)
            m_patchIndexCount = indices;
    }
    if (m_patchVertexCount == 0)
        return;
    m_patchVertexCount += m_patchVertexCount / 10u;                          // 10% headroom (unsigned)
    m_patchIndexCount += m_patchIndexCount / 10u;

    // every cell: its patch's textures and heights, or nothing when the patch has no detail --
    for (unsigned int c = 0; c < m_cellCount; ++c)
    {
        UndergrowthCell* const cell = &m_cells[c];
        const unsigned int patchesPerSide = m_cellsPerSide / subdivide;      // unsigned div
        const float patchWorldSize = subdivideF * m_cellSize;
        // The cell's index is recomputed from its address ((cell - m_cells) / 0x68), then split.
        const unsigned int index = static_cast<unsigned int>(cell - m_cells);
        const unsigned int row = index / m_cellsPerSide;
        const unsigned int column = index % m_cellsPerSide;
        const unsigned int patchX = column / subdivide;
        const unsigned int patchZ = row / subdivide;
        const int patchIndex = static_cast<int>(patchZ * static_cast<unsigned int>(
            terrain->getPatchCount()) + patchX);                             // Terrain +0x30, again
        cell->noDetail = terrain->getPatchHasNoDetail(patchIndex);           // Terrain +0x4C
        if (cell->noDetail)
            continue;

        const float halfExtent = static_cast<float>(patchesPerSide >> 1) * patchWorldSize;
        const float originX = static_cast<float>(patchX) * patchWorldSize - halfExtent;
        const float originZ = static_cast<float>(patchZ) * patchWorldSize - halfExtent;
        const float inverse = 1.0f / patchWorldSize;
        cell->texScaleU = inverse;
        cell->texScaleV = inverse;
        cell->texOffsetU = -(originX * cell->texScaleU);
        cell->texOffsetV = -(originZ * cell->texScaleV);
        cell->colorMap = static_cast<dx9rend::EngineTexture*>(
            terrain->getPatchColormap(patchIndex));                          // Terrain +0x44
        cell->lightMap = static_cast<dx9rend::EngineTexture*>(
            terrain->getPatchLightmap(patchIndex));                          // Terrain +0x48
        cell->minHeight = terrain->getPatchField9C(patchIndex);              // Terrain +0x3C
        cell->maxHeight = terrain->getPatchField98(patchIndex) + 1.5f;       // Terrain +0x40, flt_101DDF1C

        // The cell's box (x/z from the grid, y from the two heights just stored), then the height
        // map's true extremes over it replace both, and the top gets 2 more (flt_101E0550).
        MeshBounds bounds;
        initZeroBounds(bounds);
        cellBounds(cell, &bounds, false);
        m_heightmap->getMinMaxHeightInWorldCoords(bounds.min.x, bounds.min.z,
                                                  bounds.max.x, bounds.max.z,
                                                  cell->minHeight, cell->maxHeight);   // IHeightmap +0xD0
        cell->maxHeight = cell->maxHeight + 2.0f;
    }

    for (unsigned int c = 0; c < m_cellCount; ++c)
        computeCellTypeFractions(&m_cells[c]);

    for (unsigned int p = 0; p < m_patchCount; ++p)
    {
        UndergrowthPatch& patch = m_patches[p];
        patch.vertexBuffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, m_patchVertexCount,
                                                         vertexStride,
                                                         std::string("UndergrowthSystem"),
                                                         8u);                // D3DUSAGE_WRITEONLY
        patch.indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(m_patchIndexCount, 8u,
                                                                    101u);   // D3DFMT_INDEX16
    }

    m_loaded = true;
    // `pages` is freed on every exit from here on (0x1014DD96 / 0x1014DA87), then the tokenizer
    // (sub_100768E0) and the path copy.
}


// Loads baked material/type records and geometry from Undergrowth.dat.
// Requires a complete valid file; individual read results and counts are unchecked.

bool UndergrowthSystem::loadDat(const std::string& levelPath)
{
    renddx9abi::HostStream* const file = openFile(concat(levelPath, "Undergrowth.dat"), 2, 0);
    if (!file)
        return false;

    unsigned int version;                                                    // [ebp-1Ch]
    unsigned int materialCount;                                              // [ebp-18h]
    file->read(&version, 4);
    file->read(&materialCount, 4);
    m_materials.resize(materialCount);

    for (unsigned int m = 0; m < m_materials.size(); ++m)
    {
        UndergrowthMaterial& material = m_materials[m];
        readString(file, material.name);
        file->read(&material.id, 4);
        file->read(&material.generalHeight, 4);
        unsigned int typeCount = 0;
        file->read(&typeCount, 4);
        material.types.resize(typeCount);

        for (unsigned int t = 0; t < material.types.size(); ++t)
        {
            UndergrowthType& type = material.types[t];
            readString(file, type.name);
            readString(file, type.mesh);
            readString(file, type.texture);
            file->read(&type.size, 8);
            file->read(&type.randomSizeScale, 8);
            file->read(&type.slopeRange, 8);
            file->read(&type.density, 4);
            type.density = clampRange(type.density, 0.01f, 10.0f);
            file->read(&type.variation, 4);
            type.variation = clampRange(type.variation, -1.0f, 1.0f);
            file->read(&type.terrainColorScale, 4);
            if (version >= 2u)                                               // unsigned (jb)
                file->read(&type.typeSwayScale, 4);
            file->read(&type.skew, 1);

            unsigned int count = 0;
            file->read(&count, 4);
            type.vertices.resize(count);
            file->read(type.vertices.data(), count * 24u);
            count = 0;
            file->read(&count, 4);
            type.indices.resize(count);
            file->read(type.indices.data(), count * 2u);
            count = 0;
            file->read(&count, 4);
            type.materials.resize(count);
            file->read(type.materials.data(), count * 4u);
        }
    }

    file->release();                                                         // stream +0x08
    return true;
}


// Undergrowth.dat writer: `<savePath>/<levelPath>/Undergrowth.dat`,
// opened (0, 2, 0) - the write pair - through FileManager +0x18; version 2 and the layout loadDat
// reads, every field through stream +0x14. Each path gets a '/' unless it is empty or already ends
// in '\\' or '/' (`at(size() - 1)`, whose range check cannot fire under the size test). A failed open
// writes nothing. In a game the save path is " ERROR " (see load()).

void UndergrowthSystem::saveDat(const std::string& savePath, const std::string& levelPath)
{
    std::string directory(savePath);
    if (directory.size() > 0)
    {
        const char last = directory.c_str()[directory.size() - 1];
        if (last != '\\' && last != '/')
            directory += std::string("/");                                  // += (const string&)
    }
    std::string level(levelPath);
    if (level.size() > 0)
    {
        const char last = level.c_str()[level.size() - 1];
        if (last != '\\' && last != '/')
            level += std::string("/");
    }

    renddx9abi::HostStream* const file =
        openFile(concat(concat(directory, level), "Undergrowth.dat"), 0, 2);
    if (!file)
        return;

    const int version = 2;
    file->write(&version, 4);
    const unsigned int materialCount = m_materials.size();
    file->write(&materialCount, 4);

    for (unsigned int m = 0; m < m_materials.size(); ++m)
    {
        const UndergrowthMaterial& material = m_materials[m];
        const unsigned int nameLength = material.name.size();
        file->write(&nameLength, 4);
        file->write(material.name.c_str(), nameLength);
        const int id = material.id;
        file->write(&id, 4);
        const float generalHeight = material.generalHeight;
        file->write(&generalHeight, 4);
        const unsigned int typeCount = material.types.size();
        file->write(&typeCount, 4);

        for (unsigned int t = 0; t < material.types.size(); ++t)
        {
            const UndergrowthType& type = material.types[t];
            unsigned int length = type.name.size();
            file->write(&length, 4);
            file->write(type.name.c_str(), length);
            length = type.mesh.size();
            file->write(&length, 4);
            file->write(type.mesh.c_str(), length);
            length = type.texture.size();
            file->write(&length, 4);
            file->write(type.texture.c_str(), length);
            file->write(&type.size, 8);
            file->write(&type.randomSizeScale, 8);
            file->write(&type.slopeRange, 8);
            const float density = type.density;
            file->write(&density, 4);
            const float variation = type.variation;
            file->write(&variation, 4);
            const float terrainColorScale = type.terrainColorScale;
            file->write(&terrainColorScale, 4);
            const float typeSwayScale = type.typeSwayScale;
            file->write(&typeSwayScale, 4);
            const bool skew = type.skew;
            file->write(&skew, 1);
            unsigned int count = type.vertices.size();
            file->write(&count, 4);
            file->write(type.vertices.data(), count * 24u);
            count = type.indices.size();
            file->write(&count, 4);
            file->write(type.indices.data(), count * 2u);
            count = type.materials.size();
            file->write(&count, 4);
            file->write(type.materials.data(), count * 4u);
        }
    }

    file->release();
}


// Builds UndergrowthAtlas from case-insensitively unique type textures, falling back to
// each type's mesh and then default.dds. Sources are disk paths relative to the working
// directory under ../RawData/Undergrowth. Writes a DXT5 atlas and .tai file under savePath.

void UndergrowthSystem::buildAtlas(const std::string& savePath, const std::string& levelPath)
{
    const std::string rawDirectory(kRawDataDirectory);
    std::list<std::string> textures;
    for (unsigned int m = 0; m < m_materials.size(); ++m)
    {
        const UndergrowthMaterial& material = m_materials[m];
        for (unsigned int t = 0; t < material.types.size(); ++t)
        {
            const UndergrowthType& type = material.types[t];
            std::string name;
            if (type.texture.size() > 0)                                     // +0x4C, unsigned
                name = concat(concat(rawDirectory, type.texture), ".dds");
            else if (type.mesh.size() > 0)                                   // +0x30
                name = concat(concat(rawDirectory, type.mesh), ".dds");
            else
                name = concat(rawDirectory, "default.dds");

            std::list<std::string>::iterator it = textures.begin();
            for (; it != textures.end(); ++it)
            {
                if (equalsNoCase(it->c_str(), name.c_str()))
                    break;
            }
            if (it == textures.end())
                textures.push_back(name);
        }
    }
    if (textures.empty())                                                    // [ebp-4] == 0
        textures.push_back(concat(rawDirectory, "default.dds"));

    // The generator (0x10148013, ECX = m_atlas at +0x2CC, `this` unread by it): the save path, the
    // level path + "UndergrowthAtlas", an empty input directory, the list, DXT5; the answer ignored.
    // Reached only by the rebuild branch (the editor, or a level without Undergrowth.dat); the stock
    // levels ship their .dat and .tai.
    dx9rend::textureAtlasGenerate(m_atlas, savePath, concat(levelPath, "UndergrowthAtlas"),
                                  std::string(""), textures, true);
}


// Builds each type's mesh or crossed quads in atlas space, then verifies exact atlas coverage.
// Returns false on a missing texture, retaining earlier rebuilt types. The argument is unused.

bool UndergrowthSystem::buildMeshes(bool /*secondPass - never read*/)
{
    std::set<std::string> usedTextures;
    for (unsigned int m = 0; m < m_materials.size(); ++m)
    {
        UndergrowthMaterial& material = m_materials[m];
        for (unsigned int t = 0; t < material.types.size(); ++t)
        {
            UndergrowthType& type = material.types[t];
            const std::string rawDirectory(kRawDataDirectory);
            std::string textureName;
            if (type.texture.size() > 0)
                textureName = concat(concat(rawDirectory, type.texture), ".dds");
            else if (type.mesh.size() > 0)
                textureName = concat(concat(rawDirectory, type.mesh), ".dds");
            else
                textureName = concat(rawDirectory, "default.dds");

            const dx9rend::TextureAtlasEntry* const entry =
                dx9rend::textureAtlasFind(m_atlas, textureName);
            UndergrowthAtlasRect rect;
            rect.width = 1.0f;
            rect.height = 1.0f;
            rect.uOffset = 0.0f;
            rect.vOffset = 0.0f;
            if (entry)
            {
                rect.width = entry->width;
                rect.height = entry->height;
                rect.uOffset = entry->uOffset;
                rect.vOffset = entry->vOffset;
                std::string stripped;
                dx9rend::pathStripExtension(stripped, textureName);
                usedTextures.insert(std::string(stripped.c_str()));
            }
            else if (textureName.size() > 0)
            {
                return false;
            }

            if (type.mesh.size() > 0)
            {
                loadMesh(type, rect);
            }
            else
            {
                UndergrowthVertex quad[8];                                   // positions zeroed
                const float halfWidth = type.size.x * -0.5f;                 // flt_102089FC
                const float height = type.size.y;
                const float otherHalf = type.size.x * 0.5f;                  // flt_101DD4F0
                quad[0].position.x = halfWidth; quad[0].position.y = 0.0f;   quad[0].position.z = 0.0f;
                quad[1].position.x = otherHalf; quad[1].position.y = 0.0f;   quad[1].position.z = 0.0f;
                quad[2].position.x = otherHalf; quad[2].position.y = height; quad[2].position.z = 0.0f;
                quad[3].position.x = halfWidth; quad[3].position.y = height; quad[3].position.z = 0.0f;
                quad[4].position.x = 0.0f;      quad[4].position.y = 0.0f;   quad[4].position.z = halfWidth;
                quad[5].position.x = 0.0f;      quad[5].position.y = 0.0f;   quad[5].position.z = otherHalf;
                quad[6].position.x = 0.0f;      quad[6].position.y = height; quad[6].position.z = otherHalf;
                quad[7].position.x = 0.0f;      quad[7].position.y = height; quad[7].position.z = halfWidth;

                // uv in 1/32767 atlas units (flt_10219558), each through __ftol2 and its low word.
                const short u0 = static_cast<short>(static_cast<int>(rect.uOffset * 32767.0f));
                const short v0 = static_cast<short>(static_cast<int>(rect.vOffset * 32767.0f));
                const short u1 = static_cast<short>(
                    static_cast<int>((rect.uOffset + rect.width) * 32767.0f));
                const short v1 = static_cast<short>(
                    static_cast<int>((rect.vOffset + rect.height) * 32767.0f));
                const float sway = type.typeSwayScale;
                for (int half = 0; half < 2; ++half)                         // the two quads
                {
                    UndergrowthVertex* const q = &quad[half * 4];
                    q[0].texCoord[0] = u0; q[0].texCoord[1] = v1; q[0].sway = 0.0f;
                    q[1].texCoord[0] = u1; q[1].texCoord[1] = v1; q[1].sway = 0.0f;
                    q[2].texCoord[0] = u1; q[2].texCoord[1] = v0; q[2].sway = sway;
                    q[3].texCoord[0] = u0; q[3].texCoord[1] = v0; q[3].sway = sway;
                    // +0x10 is left as the stack had it - the stock .dat shows the garbage.
                }
                const unsigned short crossIndices[12] = { 0, 1, 3, 1, 2, 3, 4, 5, 7, 5, 6, 7 };

                type.vertices.clear();                                       // _Tidy: freed
                type.indices.clear();
                for (int i = 0; i < 8; ++i)
                    type.vertices.push_back(quad[i]);
                for (int i = 0; i < 12; ++i)
                    type.indices.push_back(crossIndices[i]);                 // inline, or 0x1008F7C0
            }

            for (unsigned int v = 0; v < type.vertices.size(); ++v)
            {
                type.vertices[v].colorScale = static_cast<unsigned char>(
                    static_cast<int>((1.0f - type.terrainColorScale) * 255.0f));   // flt_101E14C0
            }
        }
    }

    std::vector<std::string> atlasNames;
    dx9rend::textureAtlasCollectNames(m_atlas, atlasNames);

    // Every used texture must be in the atlas ...
    for (std::set<std::string>::const_iterator it = usedTextures.begin(); it != usedTextures.end(); ++it)
    {
        unsigned int i = 0;
        for (; i < atlasNames.size(); ++i)
        {
            if (equalsNoCase(atlasNames[i].c_str(), it->c_str()))
                break;
        }
        if (i == atlasNames.size())
            return false;
    }
    // ... and every atlas entry must be used.
    for (unsigned int i = 0; i < atlasNames.size(); ++i)
    {
        std::set<std::string>::const_iterator it = usedTextures.begin();
        for (; it != usedTextures.end(); ++it)
        {
            if (equalsNoCase(atlasNames[i].c_str(), it->c_str()))
                break;
        }
        if (it == usedTextures.end())
            return false;
    }
    return true;
}


// Imports OBJ positions and UVs into atlas-mapped vertices, then derives height-based sway.
// Requires valid triangular faces and in-range indices. The compatibility text reader
// is unchecked: malformed numeric input can loop, and its input buffer lacks a terminator.

void UndergrowthSystem::loadMesh(UndergrowthType& type, const UndergrowthAtlasRect& rect)
{
    const std::string fileName =
        concat(concat(std::string(kRawDataDirectory), type.mesh), ".obj");
    renddx9abi::HostStream* const file = openFile(fileName, 2, 0);
    if (!file)
    {
        ::renddx9abi::logf("UndergrowthSystem.cpp:1217 Couldn't open undergrowth mesh file \"%s\"",
                           fileName.c_str());                                // 0x4C1
        type.vertices.clear();
        type.indices.clear();
        return;
    }

    type.indices.clear();
    type.vertices.clear();

    const unsigned int size = static_cast<unsigned int>(file->getAvailableBytes());   // stream +0x1C
    char* const text = new char[size];
    file->read(text, size);                                                  // stream +0x10
    for (unsigned int i = 0; i < size; ++i)
    {
        if (text[i] == '/')
            text[i] = ' ';
    }

    {
        // The temporary string dies right after the stream is built (0x10148338), as here.
        std::istringstream in((std::string(text)));                          // unterminated
        std::vector<Vec3> positions;
        std::vector<Vec2> texCoords;
        while (!in.eof())                                                    // state & eofbit
        {
            std::string token;
            in >> token;
            if (token == "v")
            {
                Vec3 position;
                position.x = 0.0f;
                position.y = 0.0f;
                position.z = 0.0f;
                in >> position.x >> position.y >> position.z;
                position.z = -position.z;
                positions.push_back(position);
            }
            else if (token == "vt")
            {
                Vec2 texCoord;
                texCoord.x = 0.0f;
                texCoord.y = 0.0f;
                in >> texCoord.x >> texCoord.y;
                texCoord.y = 1.0f - texCoord.y;
                texCoords.push_back(texCoord);
            }
            else if (token == "f")
            {
                for (int corner = 0; corner < 3; ++corner)
                {
                    unsigned int face[3];                                    // not initialised
                    for (int k = 0; k < 3; ++k)
                        in >> face[k];                                       // operator>>(unsigned&)

                    UndergrowthVertex vertex;
                    vertex.position = positions[face[0] - 1];
                    const Vec2& uv = texCoords[face[1] - 1];
                    vertex.texCoord[0] = static_cast<short>(
                        static_cast<int>((uv.x * rect.width + rect.uOffset) * 32767.0f));
                    vertex.texCoord[1] = static_cast<short>(
                        static_cast<int>((uv.y * rect.height + rect.vOffset) * 32767.0f));

                    unsigned int found = type.vertices.size();
                    for (unsigned int i = 0; i < type.vertices.size(); ++i)
                    {
                        const UndergrowthVertex& other = type.vertices[i];
                        if (std::fabs(other.position.x - vertex.position.x) <= FLT_EPSILON &&
                            std::fabs(other.position.y - vertex.position.y) <= FLT_EPSILON &&
                            std::fabs(other.position.z - vertex.position.z) <= FLT_EPSILON &&
                            other.texCoord[0] == vertex.texCoord[0] &&
                            other.texCoord[1] == vertex.texCoord[1])         // flt_101E05A0
                        {
                            found = i;
                            break;
                        }
                    }
                    if (found == type.vertices.size())
                        type.vertices.push_back(vertex);
                    type.indices.push_back(static_cast<unsigned short>(found));
                }
            }
        }

        MeshBounds bounds;
        initEmptyBounds(bounds);
        for (unsigned int i = 0; i < type.vertices.size(); ++i)
            meshBoundsExpand(bounds, type.vertices[i].position);
        for (unsigned int i = 0; i < type.vertices.size(); ++i)
        {
            UndergrowthVertex& vertex = type.vertices[i];
            if (bounds.dirty)                                                // the lazy extent
                meshBoundsFinalize(bounds);
            vertex.sway = (vertex.position.y - bounds.min.y) / bounds.extent.y * type.typeSwayScale;
        }

        delete[] text;
    }                                                                        // ~texCoords, ~positions,
                                                                             // ~istringstream
    file->release();                                                         // stream +0x08
}


// Computes type 0..15 density fractions over the cell's inclusive texel rectangle.
// Uses heightmap width as row stride; load must keep density and heightmap widths equal.

void UndergrowthSystem::computeCellTypeFractions(UndergrowthCell* cell)
{
    world::IHeightmap* const heightmap = heightmapCluster()->getPrimaryHeightmap();   // +0x40
    int posX, posZ;
    heightmap->getPos(posX, posZ);                                           // IHeightmap +0x34, unused
    int width, height;
    heightmap->getSize(width, height);                                       // IHeightmap +0x2C

    MeshBounds bounds;
    initZeroBounds(bounds);
    cellBounds(cell, &bounds, false);

    int x0, z0, x1, z1;
    float fracX, fracZ;
    worldToDensityCell(bounds.min.x, bounds.min.z, &x0, &z0, &fracX, &fracZ);
    worldToDensityCell(bounds.max.x, bounds.max.z, &x1, &z1, &fracX, &fracZ);

    unsigned int histogram[256];
    std::memset(histogram, 0, sizeof(histogram));                            // rep stosd, 0x100
    for (int z = z0; z <= z1; ++z)
    {
        const unsigned char* const densityRow = m_densityMap + z * width;
        for (int x = x0; x <= x1; ++x)
            ++histogram[densityRow[x]];
    }

    const unsigned int texels = static_cast<unsigned int>((z1 - z0 + 1) * (x1 - x0 + 1));
    for (int i = 0; i < 16; ++i)
        cell->typeFractions[i] = static_cast<float>(histogram[i]) / static_cast<float>(texels);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
