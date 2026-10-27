// Path: attested by RendDX9.dll 1.50 string 0x10220D08 and diagnostic 0x101931B4.
// Complete manager/state tables and reader recovered from 0x10192BF0..0x10193881.
// This is client renderer code; the dedicated ELF contains no counterpart.
#ifdef BF2_IMAGE_RENDDX9
#include "BundledMeshStateManager.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/Texture.h"
#include <cstring>
#include <new>
#include <string>

namespace dice { namespace hfe {
namespace dx9rend {
extern IBase* g_Renderer;
void pathToLower(vc71::string& value);
void* textureManagerGetTexture(const vc71::string& name);
}
namespace geom {

BundledMeshStateManager* g_bundledMeshStateManager = 0;

namespace {
unsigned int pointerCount(const vc71::vector_layout& v)
{
    return v.first ? static_cast<unsigned int>((v.last - v.first) / 4) : 0;
}

// Both callers resize a newly constructed vector, with zero as the fill value
// (0x101930AB and 0x101934A6 -> 0x100EBAD0). The allocator word remains untouched.
void resizeEmptyTextureVector(vc71::vector_layout& v, unsigned int count)
{
    if (count)
    {
        v.first = static_cast<char*>(::operator new(count * sizeof(void*)));
        std::memset(v.first, 0, count * sizeof(void*));
        v.last = v.end = v.first + count * sizeof(void*);
    }
}

bool contains(const vc71::string& value, const char* needle)
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
    // Inlined in 0x10192FF0 at 0x10192FFF and 0x10193455.
    m_textures.first = m_textures.last = m_textures.end = 0;
    m_animated = m_normalMap = m_extraMap = m_envMap = m_colorMapGloss = 0;
    m_feature39 = m_alphaTest = m_alphaOne = m_cockpit = m_noHemiLight = 0;
}

BundledMeshState::~BundledMeshState()
{
    // release each texture, unlink from the global cache, free vector,
    // then destroy the name. The singleton dereference deliberately has no guard.
    const unsigned int count = pointerCount(m_textures);
    for (unsigned int i = 0; i < count; ++i)
    {
        dx9rend::EngineTexture* const texture =
            reinterpret_cast<dx9rend::EngineTexture**>(m_textures.first)[i];
        if (texture && --texture->refCount == 0)
        {
            dx9rend::textureDtor(texture);
            ::renddx9abi::engineFree(texture);
        }
    }
    g_bundledMeshStateManager->remove(this);
    if (m_textures.first)
        ::renddx9abi::engineFree(m_textures.first);
    m_textures.first = m_textures.last = m_textures.end = 0;
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
    vc71::string lowered(m_name);
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
    m_cache.first = m_cache.last = m_cache.end = 0;
    new (&m_texturePrefix) vc71::string();
    m_enabled = m_enabled9 = 1;
    m_texturePrefix = "Textures/";
}

BundledMeshStateManager::~BundledMeshStateManager()
{
    // the cache does NOT own state references. Retail repeatedly erases
    // its first pointer, then destroys the string, then frees the vector allocation.
    char* const first = m_cache.first;
    while (first != m_cache.last)
    {
        std::memmove(first, first + 4, m_cache.last - (first + 4));
        m_cache.last -= 4;
    }
    m_texturePrefix.~string();
    if (m_cache.first)
        ::renddx9abi::engineFree(m_cache.first);
    m_cache.first = m_cache.last = m_cache.end = 0;
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
    const unsigned int count = pointerCount(m_cache);
    for (unsigned int i = 0; i < count; ++i)
    {
        BundledMeshState* const candidate = reinterpret_cast<BundledMeshState**>(m_cache.first)[i];
        if (candidate->m_name.size() != state.m_name.size() ||
            std::memcmp(candidate->m_name.c_str(), state.m_name.c_str(), state.m_name.size()) != 0)
            continue;
        const unsigned int textures = pointerCount(candidate->m_textures);
        if (textures != pointerCount(state.m_textures))
            continue;
        bool same = true;
        for (unsigned int j = 0; j < textures; ++j)
        {
            if (reinterpret_cast<void**>(candidate->m_textures.first)[j] !=
                reinterpret_cast<void**>(state.m_textures.first)[j])
                same = false;
        }
        if (same)
            return candidate;
    }
    return 0;
}

void BundledMeshStateManager::remove(const BundledMeshState* state)
{
    char* cursor = m_cache.first;
    if (cursor == m_cache.last)
        return;
    char* next = cursor + 4;
    do
    {
        if (*reinterpret_cast<BundledMeshState**>(cursor) == state)
        {
            std::memmove(cursor, next, m_cache.last - next);
            m_cache.last -= 4;
        }
        else
        {
            cursor += 4;
            next += 4;
        }
    } while (cursor != m_cache.last);
}

BundledMeshState* BundledMeshStateManager::read(void* effect, const vc71::string& templatePath,
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
    resizeEmptyTextureVector(temp.m_textures, static_cast<unsigned int>(mapCount));
    for (int i = 0; i < mapCount; ++i)
    {
        stream->read(&length, 4);
        if (length)
        {
            raw = static_cast<char*>(::renddx9abi::engineAlloc(length + 1));
            stream->read(raw, length);
            raw[length] = 0;
            vc71::string prefix;
            vc71::string mapName(raw);
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
                (dx9rend::rendererGetShaderPath(dx9rend::g_Renderer) == 0 ||
                 !contains(mapName, "_b.dds")))
            {
                vc71::string full(prefix);
                full += mapName;
                reinterpret_cast<void**>(temp.m_textures.first)[i] = dx9rend::textureManagerGetTexture(full);
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
            vc71::string full(templatePath);
            full += m_texturePrefix;
            full += "default";
            reinterpret_cast<void**>(temp.m_textures.first)[i] = dx9rend::textureManagerGetTexture(full);
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
    resizeEmptyTextureVector(fresh->m_textures, static_cast<unsigned int>(mapCount));
    for (int i = 0; i < mapCount; ++i)
    {
        dx9rend::EngineTexture* const texture =
            reinterpret_cast<dx9rend::EngineTexture**>(temp.m_textures.first)[i];
        reinterpret_cast<dx9rend::EngineTexture**>(fresh->m_textures.first)[i] = texture;
        if (texture)
            addTextureReference(texture);
    }
    std::memcpy(&fresh->m_animated, &temp.m_animated, 10);
    if (!arg4)
    {
        // 0x1019352E..0x1019356A only increments a dead local after the vectors
        // have already been sized. It does NOT add an environment texture slot.
        vc71::string lowered(temp.m_name);
        dx9rend::pathToLower(lowered);
        if (contains(lowered, "envmap")) ++mapCount;
    }
    vc71::vectorRef<BundledMeshState*>(&m_cache).push_back(fresh);
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
