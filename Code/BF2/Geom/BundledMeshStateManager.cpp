// Path: attested by RendDX9.dll 1.50 string 0x10220D08 and diagnostic 0x101931B4.
// Complete manager/state tables and reader recovered from 0x10192BF0..0x10193881.
// This is client renderer code; the dedicated ELF contains no counterpart.
#ifdef BF2_IMAGE_RENDDX9
#include "BundledMeshStateManager.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../RendDX9/Texture.h"
#include <algorithm>
#include <cstring>
#include <new>
#include <string>

namespace dice { namespace hfe {
namespace dx9rend {
void pathToLower(std::string& value);
void* textureManagerGetTexture(const std::string& name);
unsigned int rendererRaPath();   // BF2Render.cpp - the Renderer's +0x6CC shader path
}
namespace geom {

BundledMeshStateManager* g_bundledMeshStateManager = 0;

namespace {
bool contains(const std::string& value, const char* needle)
{
    return std::strstr(value.c_str(), needle) != 0;
}

// Texture's independent three-slot table has addRef at +8, unlike IBase's +0.
// The existing EngineTexture carrier has an explicit vptr; retain the virtual call.
void addTextureReference(dx9rend::EngineTexture* texture)
{
    typedef void (__thiscall* AddRef)(void*);
    void** const table = *reinterpret_cast<void***>(texture);
    reinterpret_cast<AddRef>(table[2])(texture);
}
}

BundledMeshState::BundledMeshState() : m_refCount(1)
{
    // Inlined in 0x10192FF0 at 0x10192FFF and 0x10193455. m_textures is default-constructed empty.
    m_animated = m_normalMap = m_extraMap = m_envMap = m_colorMapGloss = 0;
    m_feature39 = m_alphaTest = m_alphaOne = m_cockpit = m_noHemiLight = 0;
}

BundledMeshState::~BundledMeshState()
{
    // release each texture, unlink from the global cache, free vector,
    // then destroy the name. The singleton dereference deliberately has no guard. The vector and
    // then the name are freed by their member destructors, in that order.
    const unsigned int count = static_cast<unsigned int>(m_textures.size());
    for (unsigned int i = 0; i < count; ++i)
    {
        dx9rend::EngineTexture* const texture = m_textures[i];
        if (texture && --texture->refCount == 0)
        {
            dx9rend::textureDtor(texture);
            ::renddx9abi::engineFree(texture);
        }
    }
    g_bundledMeshStateManager->remove(this);
}

void BundledMeshState::addRef() { ++m_refCount; }
int BundledMeshState::getRef() { return m_refCount; }
int BundledMeshState::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        destroy(1);
    return remaining;
}
void* BundledMeshState::queryInterface(unsigned int iid) const
{
    return iid == 1u ? const_cast<BundledMeshState*>(this) : 0;
}
void* BundledMeshState::destroy(unsigned char flags)
{
    this->~BundledMeshState();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}

void BundledMeshState::deriveFeatures()
{
    // lower a copy only. Feature derivation does not canonicalize the
    // name used by the case-sensitive cache, and only raises bytes already zeroed.
    std::string lowered(m_name);
    dx9rend::pathToLower(lowered);
    if (contains(lowered, "animated")) m_animated = 1;
    if (contains(lowered, "envmap")) m_envMap = 1;
    if (contains(lowered, "colormapgloss")) m_colorMapGloss = 1;
    else if (!m_normalMap) m_feature39 = 1;
    if (m_colorMapGloss && contains(lowered, "alpha_test")) m_alphaTest = 1;
    if (contains(lowered, "alpha_one")) m_alphaOne = 1;
    if (contains(lowered, "cockpit")) m_cockpit = 1;
    if (contains(lowered, "nohemilight")) m_noHemiLight = 1;
}

BundledMeshStateManager::BundledMeshStateManager() : m_refCount(1)
{
    // m_cache is default-constructed empty; the prefix lives in a union and is built here.
    new (&m_texturePrefix) std::string();
    m_enabled = m_enabled9 = 1;
    m_texturePrefix = "Textures/";
}

BundledMeshStateManager::~BundledMeshStateManager()
{
    // the cache does NOT own state references. Retail repeatedly erases
    // its first pointer, then destroys the string, then frees the vector allocation - the last
    // with m_cache's own destructor, after this body.
    m_cache.clear();
    m_texturePrefix.~basic_string();
}

void BundledMeshStateManager::addRef() { ++m_refCount; }
int BundledMeshStateManager::getRef() { return m_refCount; }
int BundledMeshStateManager::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        this->~BundledMeshStateManager();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}
void* BundledMeshStateManager::queryInterface(unsigned int iid) const
{
    return iid == 1u ? const_cast<BundledMeshStateManager*>(this) : 0;
}
void BundledMeshStateManager::setEnabled(bool enabled) { m_enabled = enabled; }
bool BundledMeshStateManager::getEnabled() const { return m_enabled != 0; }

BundledMeshState* BundledMeshStateManager::find(const BundledMeshState& state) const
{
    // identity is name and texture pointers, not the derived flags.
    const unsigned int count = static_cast<unsigned int>(m_cache.size());
    for (unsigned int i = 0; i < count; ++i)
    {
        BundledMeshState* const candidate = m_cache[i];
        if (candidate->m_name.size() != state.m_name.size() ||
            std::memcmp(candidate->m_name.c_str(), state.m_name.c_str(), state.m_name.size()) != 0)
            continue;
        const unsigned int textures = static_cast<unsigned int>(candidate->m_textures.size());
        if (textures != state.m_textures.size())
            continue;
        bool same = true;
        for (unsigned int j = 0; j < textures; ++j)
        {
            if (candidate->m_textures[j] != state.m_textures[j])
                same = false;
        }
        if (same)
            return candidate;
    }
    return 0;
}

void BundledMeshStateManager::remove(const BundledMeshState* state)
{
    // every occurrence, the survivors kept in order.
    m_cache.erase(std::remove(m_cache.begin(), m_cache.end(), state), m_cache.end());
}

BundledMeshState* BundledMeshStateManager::read(void* effect, const std::string& templatePath,
    ::renddx9abi::HostNonlinearStream* stream, bool arg4, bool skipTextures)
{
    (void)effect; // The retail first argument is never read.
    BundledMeshState temp;
    int length;
    stream->read(&length, 4);
    char* raw = static_cast<char*>(::renddx9abi::engineAlloc(length + 1));
    stream->read(raw, length);
    raw[length] = 0;
    temp.m_name = raw;
    ::renddx9abi::engineFree(raw);

    int mapCount;
    stream->read(&mapCount, 4);
    temp.m_textures.resize(static_cast<unsigned int>(mapCount), 0);   // 0x101930AB -> 0x100EBAD0
    for (int i = 0; i < mapCount; ++i)
    {
        stream->read(&length, 4);
        if (length)
        {
            raw = static_cast<char*>(::renddx9abi::engineAlloc(length + 1));
            stream->read(raw, length);
            raw[length] = 0;
            std::string prefix;
            std::string mapName(raw);
            if (!contains(mapName, "/"))
            {
                prefix = templatePath;
                prefix += m_texturePrefix;
            }
            if (contains(mapName, "default_sdgbmfbf_color_lut"))
            {
                ::renddx9abi::logf("BundledMeshStateManager.cpp:178: Mesh has specular lut "
                    "default_sdgbmfbf_color_lut which is obsolete, please re-export!");
                mapName = "_SpecularLUT_pow36_";
            }
            if (contains(mapName, "SpecularLUT_pow36"))
                mapName = "@@ABSOLUTE@@Common/Textures/SpecularLUT_pow36.dds";

            if (!skipTextures &&
                (dx9rend::rendererRaPath() == 0 ||                     // Renderer +0x6CC
                 !contains(mapName, "_b.dds")))
            {
                std::string full(prefix);
                full += mapName;
                temp.m_textures[i] =
                    static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(full));
                const std::string original(raw);
                // Preserve the unsigned size()-6 comparison, including its short-name quirk.
                if (i == 1 && original.rfind("_b.dds") == original.size() - 6u)
                    temp.m_normalMap = 1;
                else if (i > 0 && original.find("SpecularLUT") == std::string::npos)
                    temp.m_extraMap = 1;
            }
            ::renddx9abi::engineFree(raw);
        }
        else
        {
            // This path deliberately ignores skipTextures and the shader-quality gate.
            std::string full(templatePath);
            full += m_texturePrefix;
            full += "default";
            temp.m_textures[i] =
                static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(full));
        }
    }
    temp.deriveFeatures();
    if (BundledMeshState* const cached = find(temp))
    {
        cached->addRef();
        return cached;
    }

    void* const storage = ::renddx9abi::engineAlloc(sizeof(BundledMeshState));
    BundledMeshState* const fresh = storage ? new (storage) BundledMeshState() : 0;
    // Retail dereferences null on allocation failure; no invented success/guard.
    fresh->m_name = temp.m_name;
    fresh->m_textures.resize(static_cast<unsigned int>(mapCount), 0);   // 0x101934A6 -> 0x100EBAD0
    for (int i = 0; i < mapCount; ++i)
    {
        dx9rend::EngineTexture* const texture = temp.m_textures[i];
        fresh->m_textures[i] = texture;
        if (texture)
            addTextureReference(texture);
    }
    std::memcpy(&fresh->m_animated, &temp.m_animated, 10);
    if (!arg4)
    {
        // 0x1019352E..0x1019356A only increments a dead local after the vectors
        // have already been sized. It does NOT add an environment texture slot.
        std::string lowered(temp.m_name);
        dx9rend::pathToLower(lowered);
        if (contains(lowered, "envmap")) ++mapCount;
    }
    m_cache.push_back(fresh);
    return fresh;
}

void bundledMeshStateManagerRetain()
{
    if (g_bundledMeshStateManager)
        g_bundledMeshStateManager->addRef();
    else
    {
        void* const storage = ::renddx9abi::engineAlloc(sizeof(BundledMeshStateManager));
        g_bundledMeshStateManager = storage ? new (storage) BundledMeshStateManager() : 0;
    }
}
void bundledMeshStateManagerRelease()
{
    if (g_bundledMeshStateManager && !g_bundledMeshStateManager->release())
        g_bundledMeshStateManager = 0;
}

} } }
#endif
