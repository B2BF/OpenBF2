// Shared static-mesh shader-combination decoder and material cache.
// StaticMeshRenderer references the singleton for its lifetime.
// ShaderMaterial has a five-slot interface with a virtual release/deleting destructor;
// it must not use the texture wrapper's three-slot reference-count table.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include <string>
#include "../RendDX9/Texture.h"
#include "StaticMeshTemplate.h"

#include <algorithm>
#include <new>
#include <string.h>

namespace dice {
namespace hfe {

namespace dx9rend {
// BF2Render.cpp - the Renderer's +0x6CC shader-quality index (RAPATH).
unsigned int rendererRaPath();
// TextureManager.cpp - declared here rather than pulled in through RendDX9Base.h, the same way
// MeshTemplate.cpp declares the BufferManager entry points it needs.
void* textureManagerGetTexture(const std::string& name);
int   textureManagerDefaultMipClass();                      // sub_10017DF0 on dword_1023B974
int   textureManagerSwapDefaultMipClass(int value);
} // namespace dx9rend

namespace geom {


// Maps shader combinations to texture features. The parallax bit consumes no texture slot;
// detail, dirt, crack and their normal maps are subject to quality gates.

extern const unsigned int g_shaderComboFlags[16] =
{
    0x00000042u, 0x00000086u, 0x000000A6u, 0x0000008Eu,
    0x000000AEu, 0x00000196u, 0x000001B6u, 0x0000019Eu,
    0x000001BEu, 0xFFFFFFFFu, 0x00000002u, 0x00000006u,
    0x0000000Eu, 0x00000016u, 0x0000001Eu, 0xFFFFFFFFu
};


// Quality masks indexed without bounds checks by the renderer's shader quality.
// Technique initialization must populate this vector before any material uses it.
// [CHOSEN, not recovered] Storage is owned by this translation unit.
// [RETIRE-WHEN: the STM technique init fills it.]

std::vector<unsigned int> g_staticMeshQualityMasks;

namespace {

// g_Renderer + 0x6CC, the renderer's shader-quality index, through BF2Render.cpp's accessor (the
// same dword VideoOptions.cpp's rendererSetShaderPathAndUpdate writes, PE 0x100286C0).
inline int rendererShaderQualityIndex()
{
    return static_cast<int>(::dice::hfe::dx9rend::rendererRaPath());
}

// The two std::string operations the shipped bodies use, spelled out as they ran. Both are equivalent for
// the NUL-terminated names these strings hold; neither of the shipped call sites can see an
// embedded NUL, because every name arrives from the file as a counted run that is then
// NUL-terminated by hand.
inline bool stringContains(const std::string& haystack, const char* needle)
{
    return ::strstr(haystack.c_str(), needle) != 0;
}

inline bool stringEquals(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && ::strcmp(a.c_str(), b.c_str()) == 0;
}

} // anonymous namespace

unsigned int staticMeshQualityMask()
{
    // PE: mov eax, g_Renderer / mov ecx, [eax+6CCh] / mov edx, dword_1025B8AC / mov ebx, [edx+ecx*4]
    return g_staticMeshQualityMasks.data()[rendererShaderQualityIndex()];
}

// dword_102651C0. Null until StaticMeshRenderer's constructor creates it; nulled again only by that
// same constructor's allocation-failure arm (PE 0x101221B0).
StaticMeshStateManager* g_staticMeshStateManager = 0;


// ShaderMaterial - the 0x4C-byte counted record, vtable off_10221558. Every body below is one
// shipped function.

namespace {

void* const* shaderMaterialVtable();

// sub_101A91B0, retn 4 - erase every occurrence of `value` from the singleton's cache. The shipped
// call site (PE 0x101A9243) loads ecx from dword_102651C0 and pushes the record, so `this` is the
// state manager, not the record.

// IT IS NOT guarded. If the singleton has already been released, ecx is null and [ecx+0x0C]
// faults; no guard is added here either.
void staticMeshStateManagerRemoveFromCache(StaticMeshStateManager* self, const void* value)
{
    StaticMeshStateManager::ShaderMaterialCache& cache = self->m_cache;
    cache.erase(std::remove(cache.begin(), cache.end(), value), cache.end());
}

// sub_101A9200 - the record's member teardown. Republishes the vtable, releases the eight texture
// references with the texture family's explicit `if (t && !--t->refCount)` pattern, unlinks itself
// from the singleton's cache and destroys the name. In that order.
void shaderMaterialDestroyMembers(ShaderMaterial* self)
{
    self->vptr = const_cast<void**>(shaderMaterialVtable());
    for (int i = 0; i < 8; ++i)
    {
        ::dice::hfe::dx9rend::EngineTexture* const tex =
            static_cast<::dice::hfe::dx9rend::EngineTexture*>(self->textures[i]);
        if (tex && --tex->refCount == 0)
        {
            ::dice::hfe::dx9rend::textureDtor(tex);
            ::renddx9abi::engineFree(tex);
        }
    }
    staticMeshStateManagerRemoveFromCache(g_staticMeshStateManager, self);
    self->name.~basic_string();
}

int RD9_THUNK shaderMaterial_addRef(void* self RD9_THUNK_EDX)
{
    return ++static_cast<ShaderMaterial*>(self)->refCount;
}

int RD9_THUNK shaderMaterial_getRef(void* self RD9_THUNK_EDX)
{
    return static_cast<ShaderMaterial*>(self)->refCount;
}

void* RD9_THUNK shaderMaterial_destroy(void* self RD9_THUNK_EDX, unsigned char flags)
{

    shaderMaterialDestroyMembers(static_cast<ShaderMaterial*>(self));
    if ((flags & 1) != 0)
        ::renddx9abi::engineFree(self);
    return self;
}

int RD9_THUNK shaderMaterial_release(void* self RD9_THUNK_EDX)
{
    // decrement, and on zero dispatch through slot +0x10 with 1.
    ShaderMaterial* const p = static_cast<ShaderMaterial*>(self);
    const int remaining = --p->refCount;
    if (remaining == 0)
        shaderMaterial_destroy(self RD9_THUNK_ARG, 1);
    return remaining;
}

void* RD9_THUNK shaderMaterial_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return iid != 1u ? 0 : self;
}

// off_10221558, five slots.
void* const kShaderMaterialVtable[5] =
{
    reinterpret_cast<void*>(&shaderMaterial_addRef),          // +0x00
    reinterpret_cast<void*>(&shaderMaterial_getRef),          // +0x04
    reinterpret_cast<void*>(&shaderMaterial_release),         // +0x08
    reinterpret_cast<void*>(&shaderMaterial_queryInterface),  // +0x0C
    reinterpret_cast<void*>(&shaderMaterial_destroy)          // +0x10
};

void* const* shaderMaterialVtable() { return kShaderMaterialVtable; }

} // anonymous namespace

// The typed form of the disposal every owner performs - mergeMaterials (PE 0x10119CFE),
// readMaterials' lod-skip arm and the template destructor's helper all reach it as
// `((record + 2))(record)`, i.e. a virtual call through slot +0x08. Dispatching through the
// record's own vptr rather than calling the body directly keeps that property.
int shaderMaterialRelease(ShaderMaterial* material)
{
    typedef int (__thiscall* ReleaseFn)(void*);
    void** const table = reinterpret_cast<void**>(material->vptr);
    return reinterpret_cast<ReleaseFn>(table[2])(material);
}

StaticMeshStateManager::StaticMeshStateManager()
{
    // 0x101A90F0, in shipped order; the cache and the prefix are default-constructed by the
    // compiler before the body.
    m_vptr        = const_cast<void**>(shaderMaterialVtable());   // see the note below
    m_refCount    = 1;
    m_texturePrefix = "Textures/";
    m_template    = 0;

    // off_102216E4 is the manager's OWN four-slot quartet and shares its first two bodies with
    // Nothing in the recovered image dispatches through it - the renderer's
    // constructor and destructor call the constructor and sub_101A9150 directly - so publishing a
    // separate four-entry table here would add a surface no recovered call site uses. The slot is
    // filled with the record table's address purely so the field is not left indeterminate; if a
    // dispatching caller is ever found, give the manager its own table then.
}

void StaticMeshStateManager::destroyMembers()
{
    // The vtable is republished, the cache emptied (the shipped clear() expansion erases the first
    // element repeatedly), then the prefix is destroyed and only then the cache's buffer freed.
    // The owner frees the storage afterwards without running a C++ destructor.
    m_vptr = const_cast<void**>(shaderMaterialVtable());
    m_cache.clear();
    m_texturePrefix.~basic_string();
    m_cache.~ShaderMaterialCache();
}

int StaticMeshStateManager::release()
{

    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        destroyMembers();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

namespace {


// Decodes a lowercased shader filename, testing longer names first.
// Normalizes basendetail to basenbase so both names share the same cached material identity.

void staticMeshDecodeShaderCombo(StaticMeshStateManager* self, ShaderMaterial* material)
{
    std::string& name = material->name;

    if (stringContains(name, "basedetaildirtcrackndetailncrack"))
    {
        material->shaderCombo = stringContains(name, "parallaxdetail") ? 8u : 7u;
        return;
    }
    if (stringContains(name, "basedetaildirtcrack"))
    {
        material->shaderCombo = 14u;
        return;
    }
    if (stringContains(name, "basedetailcrackndetailncrack"))
    {
        material->shaderCombo = stringContains(name, "parallaxdetail") ? 6u : 5u;
        return;
    }
    if (stringContains(name, "basedetailcrack"))
    {
        material->shaderCombo = 13u;
        return;
    }
    if (stringContains(name, "basedetaildirtndetail"))
    {
        material->shaderCombo = stringContains(name, "parallaxdetail") ? 4u : 3u;
        return;
    }
    if (stringContains(name, "basedetaildirt"))
    {
        material->shaderCombo = 12u;
        return;
    }
    if (stringContains(name, "basedetailndetail"))
    {
        material->shaderCombo = stringContains(name, "parallaxdetail") ? 2u : 1u;
        return;
    }
    if (stringContains(name, "basedetail"))
    {
        material->shaderCombo = 11u;
        return;
    }
    if (stringContains(name, "basendetail"))
    {
        material->shaderCombo = 0u;
        name = "basenbase";
        return;
    }
    if (stringContains(name, "base"))
    {
        material->shaderCombo = 10u;
        return;
    }

    // StaticMeshStateManager.cpp:153. The shipped stream is
    //     "Material Error: Unknown staticmesh shader combo: " << combo << " " << getName()
    //     << std::endl << getName() << " is flawed please fix and reexport."
    // getName() (template vslot +0x18) really is called twice (PE 0x101A8F83 and 0x101A8F91), and
    // the combo it prints is the caller's zero-initialised value, because nothing on this path ever
    // set it. Rendered through the rebuild's log, the way every recovered diagnostic in this image
    // is; the text is unchanged.
    ::renddx9abi::logf(
        "StaticMeshStateManager.cpp:153: Material Error: Unknown staticmesh shader combo: %u %s\n"
        "%s is flawed please fix and reexport.",
        material->shaderCombo,
        self->m_template->getName().c_str(),
        self->m_template->getName().c_str());
}


// Shared texture-bit cursor; persists across materials and skips bit 5.
// The unsigned sentinel triggers the first-material rewind so the second map uses slot 2.
unsigned int g_shaderTextureBitCursor = 0xFFFFFFFFu;      // dword_1023AD40

int staticMeshNextTextureSlot(int combo, int currentSlot)
{
    unsigned int cursor = g_shaderTextureBitCursor;
    if (cursor > 8u && currentSlot == 1)
        cursor = 1u;

    for (;;)
    {
        ++cursor;
        switch (g_shaderComboFlags[combo] & (1u << cursor))
        {
        case 0x002u: g_shaderTextureBitCursor = cursor; return 1;
        case 0x004u: g_shaderTextureBitCursor = cursor; return 2;
        case 0x008u: g_shaderTextureBitCursor = cursor; return 3;
        case 0x010u: g_shaderTextureBitCursor = cursor; return 4;
        case 0x040u:
        case 0x080u: g_shaderTextureBitCursor = cursor; return 5;
        case 0x100u: g_shaderTextureBitCursor = cursor; return 6;
        default:
            if (cursor <= 8u)
                continue;
            g_shaderTextureBitCursor = cursor;
            return -1;
        }
    }
}


// sub_101A9010 - linear cache search. Identity is the name, the combo and all eight texture
// pointers. The shipped comparison sets a flag for slots 0..6 and folds slot 7 into the final
// `&&`, so slots 0..6 are always compared even after a mismatch - no short circuit. Reproduced.

ShaderMaterial* staticMeshFindCachedMaterial(StaticMeshStateManager* self,
                                             const ShaderMaterial* wanted)
{
    const unsigned int count = static_cast<unsigned int>(self->m_cache.size());
    for (unsigned int i = 0; i < count; ++i)
    {
        ShaderMaterial* const entry = self->m_cache[i];
        if (stringEquals(entry->name, wanted->name) && entry->shaderCombo == wanted->shaderCombo)
        {
            bool same = (entry->textures[0] == wanted->textures[0]);
            if (entry->textures[1] != wanted->textures[1]) same = false;
            if (entry->textures[2] != wanted->textures[2]) same = false;
            if (entry->textures[3] != wanted->textures[3]) same = false;
            if (entry->textures[4] != wanted->textures[4]) same = false;
            if (entry->textures[5] != wanted->textures[5]) same = false;
            if (entry->textures[6] != wanted->textures[6]) same = false;
            if (entry->textures[7] == wanted->textures[7] && same)
                return entry;
        }
    }
    return 0;
}


// The stack-resident ShaderMaterial createMaterial builds. Raw storage, so the record is destroyed
// exactly once - by shaderMaterialDestroyMembers - on every one of the shipped body's four exits.
struct ShaderMaterialScratch
{
    void* words[(sizeof(ShaderMaterial) + sizeof(void*) - 1) / sizeof(void*)];
    ShaderMaterial& get() { return *reinterpret_cast<ShaderMaterial*>(words); }
};

} // anonymous namespace


// Reads a material from stream and returns the shared cached record, allocating on a miss.
// Map names without '/' use path + m_texturePrefix. owner is stored in the material;
// version is retained in the signature but unused.

ShaderMaterial* StaticMeshStateManager::createMaterial(const std::string& path,
                                                       ::renddx9abi::HostNonlinearStream* stream,
                                                       unsigned int version,
                                                       StaticMeshTemplate* owner)
{
    (void)version;   // see the note above: the shipped body never reads it.

    m_template = owner;

    ShaderMaterialScratch scratch;
    ShaderMaterial& temp = scratch.get();
    temp.vptr     = const_cast<void**>(shaderMaterialVtable());
    temp.refCount = 1;
    new (&temp.name) std::string();
    ::memset(temp.textures, 0, sizeof(temp.textures));
    temp.shaderCombo  = 0;
    temp.textureCount = 0;

    // The .fx name: a length dword, that many bytes, NUL-terminated by hand, lowercased in place.
    {
        int nameLength = 0;
        stream->read(&nameLength, 4);
        char* const raw = static_cast<char*>(::renddx9abi::engineAlloc(nameLength + 1));
        stream->read(raw, nameLength);
        raw[nameLength] = 0;
        ::_strlwr(raw);
        temp.name = raw;
        ::renddx9abi::engineFree(raw);
    }

    staticMeshDecodeShaderCombo(this, &temp);

    int mapCount = 0;
    stream->read(&mapCount, 4);

    int slot = 1;
    for (int i = 0; i < mapCount; ++i)
    {
        int nameLength = 0;
        stream->read(&nameLength, 4);
        char* const raw = static_cast<char*>(::renddx9abi::engineAlloc(nameLength + 1));
        stream->read(raw, nameLength);
        raw[nameLength] = 0;

        std::string prefix;
        std::string mapName(raw);

        if (!stringContains(mapName, "/"))
        {
            std::string built(path);
            built += m_texturePrefix;
            prefix = built;
        }

        if (stringContains(mapName, "default_sdgbmfbf_color_lut"))
        {
            // StaticMeshStateManager.cpp:238.
            ::renddx9abi::logf("StaticMeshStateManager.cpp:238: Mesh has specular lut "
                               "default_sdgbmfbf_color_lut which is obsolete, please re-export!");
            mapName = "_SpecularLUT_pow36_";
        }
        if (stringContains(mapName, "SpecularLUT_pow36"))
        {
            mapName = "@@ABSOLUTE@@Common/Textures/SpecularLUT_pow36.dds";
            slot = 7;
        }

        // A skipped LOD loads NO textures AT ALL, but the map name is still consumed from the
        // stream above and the slot cursor below still advances. Shipped.
        if (!m_template->m_skippingLods)
        {
            const unsigned int quality = staticMeshQualityMask();
            if (((quality & 0x010u) != 0 || !stringContains(mapName, "_cr.dds"))  &&
                ((quality & 0x100u) != 0 || !stringContains(mapName, "_crb.dds")) &&
                ((quality & 0x004u) != 0 || !stringContains(mapName, "_de.dds"))  &&
                ((quality & 0x080u) != 0 || !stringContains(mapName, "_deb.dds")) &&
                ((quality & 0x008u) != 0 || !stringContains(mapName, "_di.dds")))
            {
                const int previousMipClass = ::dice::hfe::dx9rend::textureManagerDefaultMipClass();
                if (owner->m_isVegetation)
                    ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(9);

                std::string full(prefix);
                full += mapName;
                temp.textures[slot] = ::dice::hfe::dx9rend::textureManagerGetTexture(full);

                ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(previousMipClass);
                ++temp.textureCount;
            }
            ::renddx9abi::engineFree(raw);
        }
        // Skipped LODs retain raw; the free occurs only in the non-skipping branch.

        slot = staticMeshNextTextureSlot(static_cast<int>(temp.shaderCombo), slot);
    }

    if (m_template->m_skippingLods)
    {
        shaderMaterialDestroyMembers(&temp);
        return 0;
    }

    if (ShaderMaterial* const cached = staticMeshFindCachedMaterial(this, &temp))
    {
        shaderMaterial_addRef(cached RD9_THUNK_ARG);        // the shipped `(v23)(v23)` - slot +0x00
        shaderMaterialDestroyMembers(&temp);
        return cached;
    }

    ShaderMaterial* fresh =
        static_cast<ShaderMaterial*>(::renddx9abi::engineAlloc(sizeof(ShaderMaterial)));
    if (fresh)
    {
        fresh->vptr     = const_cast<void**>(shaderMaterialVtable());
        fresh->refCount = 1;
        new (&fresh->name) std::string();
        fresh->shaderCombo = 0;
        ::memset(fresh->textures, 0, sizeof(fresh->textures));
        fresh->textureCount = 0;
    }
    // Allocation failure is not guarded before this dereference.
    fresh->name         = temp.name;
    fresh->shaderCombo  = temp.shaderCombo;
    fresh->textureCount = temp.textureCount;
    for (int i = 0; i < 8; ++i)
    {
        if (temp.textures[i])
        {
            fresh->textures[i] = temp.textures[i];
            ::dice::hfe::dx9rend::textureAddRef(
                static_cast<::dice::hfe::dx9rend::EngineTexture*>(temp.textures[i]));
        }
    }

    m_cache.push_back(fresh);                    // sub_101A97E0, insert(end, 1, value)
    shaderMaterialDestroyMembers(&temp);
    return fresh;
}


// The cross-translation-unit entry points. They are free functions so neither StaticMeshTemplate.cpp
// nor StaticMeshRenderer.cpp needs this file's private class body.


// The signature StaticMeshTemplate::readMaterials calls, as agreed with the coordinator.
ShaderMaterial* staticMeshStateManagerCreateMaterial(const std::string& path,
                                                     ::renddx9abi::HostNonlinearStream* stream,
                                                     unsigned int version,
                                                     StaticMeshTemplate* owner)
{
    return g_staticMeshStateManager->createMaterial(path, stream, version, owner);
}

// StaticMeshRenderer's constructor: engineAlloc (retail 0x38) then 0x101A90F0.
StaticMeshStateManager* staticMeshStateManagerConstruct(void* storage)
{
    return new (storage) StaticMeshStateManager();
}

// the member teardown the renderer's destructor calls before engineFree.
void staticMeshStateManagerDestroyMembers(StaticMeshStateManager* self)
{
    self->destroyMembers();
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
