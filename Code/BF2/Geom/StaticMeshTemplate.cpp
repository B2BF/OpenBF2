// Path: attested by RendDX9.dll __FILE__ at 0x1021C2B8.
// Static mesh loading, properties and lifetime, recovered from RendDX9.dll 1.50 (PE only).
// Material merging and vertex-buffer construction live in StaticMeshMaterial.cpp;
// base readers live in MeshTemplate.cpp. Named [FRONTIER] bodies remain explicit.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/Ra/RaShader.h"
#include "../RendDX9/Ra/RaShaderPermutator.h"
#include "../Game/Main/IVideoOptions.h"
#include "StaticMeshTemplate.h"
#include "StaticMesh.h"

#include <map>
#include <new>
#include <string.h>
#include <math.h>                        // loadOccluders' normalize (sub_100777A0)
#include <stdlib.h>                      // loadOccluders' numeric scans - see OccScanner

namespace dice {
namespace hfe {

namespace dx9rend {

// BF2Render.cpp - 0x1023D098. readLods and readMaterials read three ints out of it directly, the
// way the PE does; the Renderer class itself is not laid out in this translation unit.
extern IBase* g_Renderer;

// TextureManager.cpp - sub_10017DF0 (TM+0x98). readMaterials saves and restores it around the
// material pass. `textureManagerSwapDefaultMipClass` (sub_10017E00) is already in RendDX9Base.h.
int textureManagerDefaultMipClass();

// BufferManager.cpp - the shipped inline `if (!--wrapper[1]) { sub_10014510(wrapper);
// engineFree(wrapper); }` dance the destructor performs seven times.
void vertexBufferRelease(void* wrapper);

// RenderQueues.cpp - dword_1023E408, the 184-byte render-queue table whose tail is the Ra shader
// table. Every `getShader` call in initTechniques loads it straight into ecx (0x10116F53 and the
// eleven rows after it, plus 0x1019243D inside the mask builder) with no null test; neither does
// this declaration's users. RaShader.h models the fields the lookups touch.
extern void* g_renderQueues;

// BF2Render.cpp - the shared typed seam for Renderer vtable slot +0xAC (0x10032740,
// `return *(BYTE*)(this + 1892)`). initTechniques picks "Water" over "WaterBase" with it at
// The shipped call site loads g_Renderer (0x1023D098) rather than g_rendererBase
// (0x1023B970); BF2Render.cpp records that both globals hold the same Renderer `this`.
bool rendererIsExpansionPack();

} // namespace dx9rend

namespace geom {


// Everything this translation unit shares with StaticMeshStateManager.cpp - the singleton, the
// by-singleton createMaterial form (PE 0x101A9270, reached as
// `sub_101A9270(this+0x30, stream, this[42], this)` with ecx = dword_102651C0), the shader-combo
// flag table dword_1023A9D0 and the per-quality mask read - is declared in StaticMeshTemplate.h.


namespace {


// Renderer fields read by byte offset, exactly as the shipped bodies do. BufferManager.cpp reaches
// into g_Renderer the same way; the Renderer's full layout is not recovered.

int rendererField(unsigned int byteOffset)
{
    if (!::dice::hfe::dx9rend::g_Renderer)
        return 0;
    return *reinterpret_cast<int*>(
        reinterpret_cast<unsigned char*>(::dice::hfe::dx9rend::g_Renderer) + byteOffset);
}

// readLods: g_Renderer + 2020 (0x7E4) and + 2028 (0x7EC) - the renderer-level lod-skip settings.
// [INFERRED] roles: the first is a flat number of lods to drop, the second is an offset added to
// the template's own +0x2B8 before the result is clamped at zero.
int rendererLodSkipBase()   { return rendererField(2020); }
int rendererLodSkipBias()   { return rendererField(2028); }

// readMaterials / initTechniques use the renderer's mapped shader-path index.
unsigned int rendererShaderPath()
{
    return ::dice::hfe::dx9rend::g_Renderer
        ? static_cast<unsigned int>(::dice::hfe::dx9rend::rendererGetShaderPath(
              ::dice::hfe::dx9rend::g_Renderer))
        : 0u;
}

// sub_1000D950 on dword_1023B988 - the ResourceManager byte at +0x04. TextureManager.cpp has the
// same one-line body behind an anonymous namespace, so the shared typed accessor is used here.
bool resourceLoadingDeferred()
{
    return ::dice::hfe::dx9rend::resourceManagerIsReady();
}


// sub_10075110 - case-insensitive substring search. Read in full: it copies both strings, lowers
// each with sub_10074D10 (RendDX9Base.h's pathToLower - tolower plus the five Swedish maps), and
// then runs std::string::find on the lowered pair. Reproduced with the same two lowered copies.

const unsigned int kNpos = 0xFFFFFFFFu;

// sub_1007B220 - the shared unit scale the constructor publishes into both +0x138 and +0x144.
// MeshTemplate.cpp carries the same file-local helper with the full evidence note (a VC7.1
// magic-static Vec3(1,1,1) at dword_10247BFC); both copies are file-scope, as the shipped one is.
const Vec3& unitScale()
{
    static Vec3 value = { 1.0f, 1.0f, 1.0f };
    return value;
}

unsigned int findCaseInsensitive(const vc71::string& haystack, const vc71::string& needle)
{
    vc71::string lowerHaystack(haystack.c_str());
    vc71::string lowerNeedle(needle.c_str());
    ::dice::hfe::dx9rend::pathToLower(lowerHaystack);
    ::dice::hfe::dx9rend::pathToLower(lowerNeedle);

    const char* found = ::strstr(lowerHaystack.c_str(), lowerNeedle.c_str());
    if (!found)
        return kNpos;
    return static_cast<unsigned int>(found - lowerHaystack.c_str());
}

// std::string::find(const char*, 0, len) - the case-sensitive one load() uses for "vegitation".
unsigned int findCaseSensitive(const vc71::string& haystack, const char* needle)
{
    const char* found = ::strstr(haystack.c_str(), needle);
    if (!found)
        return kNpos;
    return static_cast<unsigned int>(found - haystack.c_str());
}


// VC7.1 std::vector helpers. Element type and growth policy are read from the shipped bodies
// (sub_1011F230, sub_1011CAA0, sub_1004E420): the buffer comes from the engine allocator, growth is
// "half again, clamped up to what was asked for", and the allocator word at +0x00 is never touched.
// These are source-level container operations, not a transcription of allocator internals.


unsigned int vectorSizeBytes(const vc71::vector_layout& v)
{
    return v.first ? static_cast<unsigned int>(v.last - v.first) : 0u;
}

unsigned int vectorCapacityBytes(const vc71::vector_layout& v)
{
    return v.first ? static_cast<unsigned int>(v.end - v.first) : 0u;
}

// The common grow half of the three resize bodies: reserve `neededElems`, memcpy the old bytes
// over (every element type resized here is trivially relocatable - the geom/lod/float records hold
// no owning members at the point any of these run), free the old block.
void vectorReserveRaw(vc71::vector_layout& v, unsigned int elemSize, unsigned int neededElems)
{
    const unsigned int sizeElems = vectorSizeBytes(v) / elemSize;
    const unsigned int capElems  = vectorCapacityBytes(v) / elemSize;
    if (capElems >= neededElems)
        return;

    unsigned int newCap = capElems + (capElems >> 1);
    if (newCap < neededElems)
        newCap = neededElems;

    char* buffer = static_cast<char*>(::renddx9abi::engineAlloc(newCap * elemSize));
    if (!buffer)
        return;

    if (v.first && sizeElems)
        ::memcpy(buffer, v.first, sizeElems * elemSize);
    if (v.first)
        ::renddx9abi::engineFree(v.first);

    v.first = buffer;
    v.last  = buffer + sizeElems * elemSize;
    v.end   = buffer + newCap * elemSize;
}

// sub_1011E640 - destroy [first, last) of the 32-byte StaticMeshGeom. The second vector (+0x10,
// buffer at +0x14) is freed first and the first (+0x00, buffer at +0x04) second, and both are then
// zeroed; the allocator words are left as they are.
void staticMeshGeomsDestroyRange(char* first, char* last)
{
    for (char* p = first; p != last; p += sizeof(StaticMeshGeom))
    {
        vc71::vector_layout* distances = reinterpret_cast<vc71::vector_layout*>(p + 0x10);
        if (distances->first)
            ::renddx9abi::engineFree(distances->first);
        distances->first = 0;
        distances->last  = 0;
        distances->end   = 0;

        vc71::vector_layout* lods = reinterpret_cast<vc71::vector_layout*>(p);
        if (lods->first)
            ::renddx9abi::engineFree(lods->first);
        lods->first = 0;
        lods->last  = 0;
        lods->end   = 0;
    }
}

// sub_1011F870 - vector<StaticMeshGeom>::resize(count, StaticMeshGeom()). The by-value element the
// call site builds on the stack is a zeroed 32-byte block with the two allocator words left alone,
// which is what a default-constructed pair of empty vectors is.
void staticMeshGeomsResize(vc71::vector_layout& v, unsigned int count)
{
    const unsigned int sizeElems = vectorSizeBytes(v) / sizeof(StaticMeshGeom);
    if (sizeElems >= count)
    {
        if (!v.first || count >= sizeElems)
            return;
        char* newLast = v.first + count * sizeof(StaticMeshGeom);
        if (newLast == v.last)
            return;
        staticMeshGeomsDestroyRange(newLast, v.last);
        v.last = newLast;
        return;
    }

    vectorReserveRaw(v, sizeof(StaticMeshGeom), count);
    if (!v.first)
        return;
    for (unsigned int i = sizeElems; i < count; ++i)
    {
        StaticMeshGeom* g = reinterpret_cast<StaticMeshGeom*>(v.first) + i;
        g->lods.first = 0;         g->lods.last = 0;         g->lods.end = 0;
        g->lodDistances.first = 0; g->lodDistances.last = 0; g->lodDistances.end = 0;
    }
    v.last = v.first + count * sizeof(StaticMeshGeom);
}

// sub_1011D380 - vector<StaticMeshLod>::resize(count, StaticMeshLod()). The by-value element on the
// stack is 104 bytes with +0x04..+0x23 zeroed and the MeshBounds `dirty` byte at +0x24 set to 1;
// +0x00 (nodeCount) is left uninitialised, which readLods then memsets away anyway. There is no
// destructor: the record is trivially destructible.
void staticMeshLodsResize(vc71::vector_layout& v, unsigned int count)
{
    const unsigned int sizeElems = vectorSizeBytes(v) / sizeof(StaticMeshLod);
    if (sizeElems >= count)
    {
        if (!v.first || count >= sizeElems)
            return;
        char* newLast = v.first + count * sizeof(StaticMeshLod);
        if (newLast != v.last)
            v.last = newLast;
        return;
    }

    vectorReserveRaw(v, sizeof(StaticMeshLod), count);
    if (!v.first)
        return;
    for (unsigned int i = sizeElems; i < count; ++i)
    {
        StaticMeshLod* lod = reinterpret_cast<StaticMeshLod*>(v.first) + i;
        ::memset(reinterpret_cast<char*>(lod) + 4, 0, sizeof(StaticMeshLod) - 4);
        lod->bounds.dirty = 1;
    }
    v.last = v.first + count * sizeof(StaticMeshLod);
}

// sub_100EC600 - vector<float>::resize(count, value). Shrinking is the shipped `memmove(dst, last,
// 0)` no-op that only moves _Last, which is how a POD vector shrinks.
void floatVectorResize(vc71::vector_layout& v, unsigned int count, float value)
{
    const unsigned int sizeElems = vectorSizeBytes(v) / sizeof(float);
    if (sizeElems >= count)
    {
        if (!v.first || count >= sizeElems)
            return;
        char* newLast = v.first + count * sizeof(float);
        if (newLast != v.last)
            v.last = newLast;
        return;
    }

    vectorReserveRaw(v, sizeof(float), count);
    if (!v.first)
        return;
    for (unsigned int i = sizeElems; i < count; ++i)
        reinterpret_cast<float*>(v.first)[i] = value;
    v.last = v.first + count * sizeof(float);
}

// sub_100EE010 -> sub_100EDCB0 - vector<vector<float>>::resize(count, vector<float>()). 16-byte
// elements; the grown tail is a run of empty vectors with their allocator words untouched.
void floatVectorVectorResize(vc71::vector_layout& v, unsigned int count)
{
    const unsigned int sizeElems = vectorSizeBytes(v) / sizeof(vc71::vector_layout);
    if (sizeElems >= count)
    {
        if (!v.first || count >= sizeElems)
            return;
        char* newLast = v.first + count * sizeof(vc71::vector_layout);
        if (newLast == v.last)
            return;
        for (char* p = newLast; p != v.last; p += sizeof(vc71::vector_layout))
        {
            vc71::vector_layout* inner = reinterpret_cast<vc71::vector_layout*>(p);
            if (inner->first)
                ::renddx9abi::engineFree(inner->first);
            inner->first = 0; inner->last = 0; inner->end = 0;
        }
        v.last = newLast;
        return;
    }

    vectorReserveRaw(v, sizeof(vc71::vector_layout), count);
    if (!v.first)
        return;
    for (unsigned int i = sizeElems; i < count; ++i)
    {
        vc71::vector_layout* inner = reinterpret_cast<vc71::vector_layout*>(v.first) + i;
        inner->first = 0; inner->last = 0; inner->end = 0;
    }
    v.last = v.first + count * sizeof(vc71::vector_layout);
}


// sub_1011FB40 / sub_1011E5F0 / sub_10120D40 - the occluder-group vector's teardown, 48 bytes per
// group. One group is three 16-byte vectors at +0x00, +0x10 and +0x20; the third and second are
// freed outright, and the first's 32-byte elements each own one further vector at +0x14
// (sub_1011E5F0 frees only that one - it does not touch the element's +0x04 vector, unlike
// sub_1011E640). The .occ loader that fills any of this is the frontier below, so in this sitting
// the vector is always empty.

void occluderGroupDestroy(char* group)
{
    vc71::vector_layout* third = reinterpret_cast<vc71::vector_layout*>(group + 0x20);
    if (third->first)
        ::renddx9abi::engineFree(third->first);
    third->first = 0; third->last = 0; third->end = 0;

    vc71::vector_layout* second = reinterpret_cast<vc71::vector_layout*>(group + 0x10);
    if (second->first)
        ::renddx9abi::engineFree(second->first);
    second->first = 0; second->last = 0; second->end = 0;

    vc71::vector_layout* firstVec = reinterpret_cast<vc71::vector_layout*>(group);
    if (firstVec->first)
    {
        for (char* p = firstVec->first; p != firstVec->last; p += 32)
        {
            vc71::vector_layout* inner = reinterpret_cast<vc71::vector_layout*>(p + 0x10);
            if (inner->first)
                ::renddx9abi::engineFree(inner->first);
            inner->first = 0; inner->last = 0; inner->end = 0;
        }
        ::renddx9abi::engineFree(firstVec->first);
    }
    firstVec->first = 0; firstVec->last = 0; firstVec->end = 0;
}

// sub_10120D40 - destroy every 48-byte group, free the buffer, zero the three pointers.
void occluderGroupsDestroy(vc71::vector_layout& v)
{
    if (v.first)
    {
        for (char* p = v.first; p != v.last; p += 48)
            occluderGroupDestroy(p);
        ::renddx9abi::engineFree(v.first);
    }
    v.first = 0;
    v.last  = 0;
    v.end   = 0;
}


// Live-instance set uses 20-byte nodes.
// [CHOSEN, not recovered] Ordering and duplicate rejection are preserved without red-black
// rebalancing (RW-74). Header allocation failure still reaches unguarded flag stores.
vc71::set_node* setConstructHead()
{
    vc71::set_node* node =
        static_cast<vc71::set_node*>(::renddx9abi::engineAlloc(sizeof(vc71::set_node)));
    if (node)
        node->left = 0;
    if (node)
        node->parent = 0;
    if (node)
        node->right = 0;
    node->color = 1;
    node->isNil = 0;
    return node;
}

// sub_100FFB10 - _Erase(node): recurse right, free, walk left. Same shape as the recovered walk in
// GeometryTemplateManager.cpp; the set owns no values, so nothing is destroyed but the node.
void setEraseSubtree(vc71::set_node* node)
{
    while (node && !node->isNil)
    {
        setEraseSubtree(node->right);
        vc71::set_node* left = node->left;
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

// Clears the entire set; partial-range erase is not implemented here.
void setClear(vc71::set_layout& s)
{
    if (!s.head)
        return;
    setEraseSubtree(s.head->parent);
    s.head->parent = s.head;
    s.size = 0;
    s.head->left  = s.head;
    s.head->right = s.head;
}

// sub_1011BEC0 - insert(value). Its descent (walk from head->parent, remember the last node and
// which way we turned, reject a duplicate) is exactly what vc71::set_layout::insert already
// carries, so the shared helper is used rather than a second copy; its RW-74 caveat above applies
// unchanged.
bool setInsert(vc71::set_layout& s, void* value)
{
    return s.insert(reinterpret_cast<unsigned int>(value));
}


// Unsigned-to-float compatibility map with 24-byte nodes; string-key map helpers do not fit.


struct FloatMapNode
{
    FloatMapNode* left;     // +0x00
    FloatMapNode* parent;   // +0x04
    FloatMapNode* right;    // +0x08
    unsigned int  key;      // +0x0C
    float         value;    // +0x10
    unsigned char color;    // +0x14
    unsigned char isNil;    // +0x15
    unsigned char pad[2];   // +0x16
};

// sub_10049650 - find(key): descend remembering the last node not less than the key, then reject
// it if it is the header or if the key really is smaller. Returns the header on a miss.
FloatMapNode* floatMapFind(const vc71::map_layout& m, unsigned int key)
{
    FloatMapNode* head = reinterpret_cast<FloatMapNode*>(m.head);
    if (!head)
        return 0;

    FloatMapNode* best = head;
    FloatMapNode* node = head->parent;
    while (!node->isNil)
    {
        if (node->key >= key)
        {
            best = node;
            node = node->left;
        }
        else
        {
            node = node->right;
        }
    }
    if (best == head || key < best->key)
        return head;
    return best;
}

// sub_100EC4A0 - operator[](key): the same descent, and on a miss a fresh node with a zero value
// is linked in and its value slot returned.
float* floatMapAt(vc71::map_layout& m, unsigned int key)
{
    FloatMapNode* head = reinterpret_cast<FloatMapNode*>(m.head);
    if (!head)
        return 0;

    FloatMapNode* best = head;
    FloatMapNode* node = head->parent;
    while (!node->isNil)
    {
        if (node->key >= key)
        {
            best = node;
            node = node->left;
        }
        else
        {
            node = node->right;
        }
    }
    if (best != head && key >= best->key)
        return &best->value;

    FloatMapNode* fresh =
        static_cast<FloatMapNode*>(::renddx9abi::engineAlloc(sizeof(FloatMapNode)));
    if (!fresh)
        return 0;
    fresh->left = head;
    fresh->right = head;
    fresh->key = key;
    fresh->value = 0.0f;
    fresh->color = 0;
    fresh->isNil = 0;
    fresh->pad[0] = 0;
    fresh->pad[1] = 0;

    if (head->parent == head)
    {
        fresh->parent = head;
        head->parent = fresh;
        head->left   = fresh;
        head->right  = fresh;
    }
    else
    {
        FloatMapNode* attach = head->parent;
        FloatMapNode* attachParent = head;
        bool left = true;
        while (!attach->isNil)
        {
            attachParent = attach;
            left = key < attach->key;
            attach = left ? attach->left : attach->right;
        }
        fresh->parent = attachParent;
        if (left)
        {
            attachParent->left = fresh;
            if (attachParent == head->left)
                head->left = fresh;
        }
        else
        {
            attachParent->right = fresh;
            if (attachParent == head->right)
                head->right = fresh;
        }
    }
    ++m.size;
    return &fresh->value;
}

// sub_100EB890 - insert(pair(key, value)). Its descent is byte-for-byte sub_1011BEC0's with the
// 24-byte node's flag offsets; where the key is already present nothing is written, which is why
// setMaterialValue's insert arm can silently do nothing on a repeat.
void floatMapInsert(vc71::map_layout& m, unsigned int key, float value)
{
    // The only call site has already established that the key is absent (its find returned end),
    // so this is always the creating path; _Tree::insert's "leave a duplicate alone" arm cannot be
    // reached from here and is therefore not modelled separately.
    float* slot = floatMapAt(m, key);
    if (slot)
        *slot = value;
}


// The D3D "preload" seam behind slot +0xAC. sub_10014610 (vertex buffer), sub_10014400 (index
// buffer) and sub_10014920 (texture) are comdat-identical: `if (wrapper[2]) wrapper[2]->vtbl[9]()`,
// and vtable slot 9 of every IDirect3DResource9 is PreLoad.

// Index-buffer access requires a constructed buffer; the wrapper is not null-checked.

struct D3DResourceWrapper
{
    void*                vptr;      // +0x00
    int                  refCount;  // +0x04
    IDirect3DResource9*  resource;  // +0x08
};

void resourcePreload(void* wrapper)
{
    D3DResourceWrapper* self = static_cast<D3DResourceWrapper*>(wrapper);
    if (self->resource)
        self->resource->PreLoad();
}


// Technique tables initialized before material loading.

// byte_1023982C is the once-gate: it ships as 1 and the initialiser clears it on success.


unsigned char g_staticMeshTechniquesPending = 1;        // byte_1023982C, initial value 1


// Private technique registry keyed by unsigned mask. Its nodes are not shared across
// image boundaries and must not use the 48-byte string-key compatibility helpers.

typedef std::map<unsigned int, ::dice::hfe::dx9rend::Ra::RaShader*> StaticMeshTechniqueMap;

StaticMeshTechniqueMap g_staticMeshTechniques;

// Unsigned-vector helpers for shader-quality masks, allocated on the engine heap.

void unsignedVectorPushBack(vc71::vector_layout& v, unsigned int value)
{
    const unsigned int sizeElems = vectorSizeBytes(v) / sizeof(unsigned int);
    vectorReserveRaw(v, sizeof(unsigned int), sizeElems + 1u);
    if (!v.first)
        return;
    reinterpret_cast<unsigned int*>(v.first)[sizeElems] = value;
    v.last = v.first + (sizeElems + 1u) * sizeof(unsigned int);
}

void unsignedVectorResize(vc71::vector_layout& v, unsigned int count, unsigned int value)
{
    const unsigned int sizeElems = vectorSizeBytes(v) / sizeof(unsigned int);
    if (sizeElems >= count)
    {
        if (!v.first || count >= sizeElems)
            return;
        char* newLast = v.first + count * sizeof(unsigned int);
        if (newLast != v.last)
            v.last = newLast;
        return;
    }

    vectorReserveRaw(v, sizeof(unsigned int), count);
    if (!v.first)
        return;
    for (unsigned int i = sizeElems; i < count; ++i)
        reinterpret_cast<unsigned int*>(v.first)[i] = value;
    v.last = v.first + count * sizeof(unsigned int);
}


// Shader feature names in bit order: each entry encodes 1 << index.

const char* const kStaticMeshShaderBitNames[] =
{
    "lightmap", "base", "detail", "dirt", "crack", "parallaxdetail",
    "nbase", "ndetail", "ncrack", "shadow", "pointlight", "alphatest",
    0
};


// Returns 1 << index for a name in the null-terminated table, or zero without logging.
// names must be nonnull. The shift counter retains its byte-width wrap behavior.

unsigned int shaderBitFromName(const char* const* names, const vc71::string& name)
{
    unsigned char shift = 0;

    if (*names == 0)
        return 0;

    const char* const* it = names;
    for (;;)
    {
        // The shipped body copy-constructs a std::string from the table entry for every probe and
        // destroys it again; sub_10075000 is _stricmp on the two c_str()s.
        const vc71::string candidate(*it);
        const bool match = ::_stricmp(name.c_str(), candidate.c_str()) == 0;
        if (match)
            break;

        ++it;
        ++shift;
        if (*it == 0)
            return 0;
    }

    return 1u << shift;
}


// Builds per-quality masks and registered techniques from a MetaFx.
// bitNames is a NUL-terminated feature-name table.

bool staticMeshBuildTechniqueMasks(const vc71::string& mfxPath,
                                   const vc71::string& techniquePrefix,
                                   const char* const* bitNames,
                                   StaticMeshTechniqueMap& techniqueMap,
                                   vc71::vector_layout& maskVector)
{
    // sub_100EB1E0(head->parent), the recursive _Erase of the whole tree,
    // then the four header stores that relink it empty. That is map::clear() on 24-byte nodes; see
    // the note on g_staticMeshTechniques above for why this is no longer the 48-byte helper.
    techniqueMap.clear();

    // 0x10192204 - `mov ecx, dword_1023D954; push edx; call sub_10033D40`.
    ::dice::hfe::dx9rend::Ra::MetaFx* const fx =
        ::dice::hfe::dx9rend::effectManagerGetMetaFx(mfxPath);
    if (!fx)
        return false;                       // pop esi / xor al, al / retn 14h

    // 0x10192223 - `lea esi, [ebx+0A8h]; test esi, esi; jz loc_10192316`.

    // Keep the null branch around the deprecation-list accessor.
    const std::vector< ::dice::hfe::dx9rend::Ra::MetaFxStringArray>* const deprecation =
        &fx->getDeprecationList();

    if (deprecation)
    {
        // sub_100AEFF0(0, 0) on the mask vector: resize(0), i.e. clear.
        unsignedVectorResize(maskVector, 0u, 0u);

        unsigned int accumulated = 0;                       // var_8
        unsignedVectorPushBack(maskVector, 0xFFFFFFFFu);    // 0x10192254, sub_100E22D0(&-1)
        unsigned int passCount = 1;                         // arg_10, reused as the counter

        // Each row adds features to drop; lower quality levels inherit all previous exclusions.
        for (std::vector< ::dice::hfe::dx9rend::Ra::MetaFxStringArray>::const_iterator row =
                 deprecation->begin();
             row != deprecation->end(); ++row)
        {
            unsigned int groupFlags = 0;                    // arg_0, reused
            for (::dice::hfe::dx9rend::Ra::MetaFxStringArray::const_iterator name = row->begin();
                 name != row->end(); ++name)
            {
                groupFlags |= shaderBitFromName(bitNames, *name);
            }

            accumulated |= groupFlags;
            unsignedVectorPushBack(maskVector, ~accumulated);
            ++passCount;
        }

        // 0x101922FC - `if (passCount < 4) passCount = 4; resize(passCount, -1)`. An empty
        // deprecation list reaches this with passCount == 1 by falling through 0x10192304.
        unsignedVectorResize(maskVector, passCount < 4u ? 4u : passCount, 0xFFFFFFFFu);
    }

    // 0x10192316 - `quality = g_Renderer[0x6CC]; if (maskVector.size() < quality) quality = size`.

    // THE clamp IS `<`, NOT `<=`, SO IT clamps TO size() AND NOT size()-1. A shader-path index at
    // or past the number of mask entries indexes one element past the end, twice, just
    // below. Preserved: it is the shipped comparison (`cmp eax, esi; jnb short`).
    unsigned int quality = rendererShaderPath();
    const unsigned int maskCount = vectorSizeBytes(maskVector) / sizeof(unsigned int);
    if (maskCount < quality)
        quality = maskCount;

    // The lookup uses "hasshadow", but the table names it "shadow"; clearing the
    // resulting zero mask intentionally leaves the shadow bit set.
    if (!::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(4))
    {
        const vc71::string hasShadow("hasshadow");

        // The shipped body computes the element address before the call (0x10192361) and applies
        // the AND after it. Kept in that order because the element address is taken from the
        // vector's _First as it stands at that instant.
        unsigned int* const slot =
            reinterpret_cast<unsigned int*>(maskVector.first) + quality;
        *slot &= ~shaderBitFromName(bitNames, hasShadow);
    }

    const unsigned int mask =
        reinterpret_cast<const unsigned int*>(maskVector.first)[quality];

    // Numeric bit masks key permutations, collapsing duplicates before ordered registration.
    std::map<unsigned int, vc71::string> techniqueList;

    if (fx->buildPermutationBits(techniqueList, bitNames))
    {
        // the ordinary in-order walk from _Myhead->_Left to _Myhead.
        for (std::map<unsigned int, vc71::string>::const_iterator it = techniqueList.begin();
             it != techniqueList.end(); ++it)
        {
            const unsigned int bits = it->first;

            // 0x101923E0 - `find(bits & mask) == end()` AND `(mask & bits) == bits`. The second
            // test makes the first a plain `find(bits)`: a permutation is registered only when
            // every bit it needs survives this quality level and nothing has claimed it yet.
            if (techniqueMap.find(bits & mask) == techniqueMap.end() &&
                (mask & bits) == bits)
            {
                // 0x1019240E - `string(prefix) += name`, then a copy for the call argument. The
                // shipped pair of temporaries is the inlining of `techniquePrefix + name`.
                vc71::string techniqueName(techniquePrefix);
                techniqueName += it->second;

                // 0x1019243D - `mov ecx, dword_1023E408; call sub_10041F20` with skipFileCheck
                // true here and false at every row of initTechniques: a generated permutation has
                // no shaders/rashader<name>.fx on disk, so it must be built rather than looked up.
                ::dice::hfe::dx9rend::Ra::RaShader* const shader =
                    ::dice::hfe::dx9rend::Ra::getShader(
                        static_cast< ::dice::hfe::dx9rend::Ra::RaShaderTable*>(
                            ::dice::hfe::dx9rend::g_renderQueues),
                        techniqueName, true);

                // 0x1019245B, sub_100EBA10 - insert(pair), which keeps an existing key. The guard
                // above already proved there is none.
                techniqueMap.insert(std::make_pair(bits, shader));
            }
        }

        // sub_1003C960 with ecx = fx. Drops every generated .fx from the effect
        // manager's in-memory file store now that the shaders that wanted them are built.
        fx->dropGeneratedFiles();
    }

    return true;                            // mov al, 1
}


// Registers a file-backed fixed technique without replacing an existing mask entry.
// Generated permutations use a separate path that skips file-existence checks.

void staticMeshRegisterTechnique(const char* name, unsigned int flags)
{
    const vc71::string techniqueName(name);
    ::dice::hfe::dx9rend::Ra::RaShader* const shader =
        ::dice::hfe::dx9rend::Ra::getShader(
            static_cast< ::dice::hfe::dx9rend::Ra::RaShaderTable*>(
                ::dice::hfe::dx9rend::g_renderQueues),
            techniqueName, false);

    g_staticMeshTechniques.insert(std::make_pair(flags, shader));
}


// Initializes shared static-mesh techniques and quality masks while their gate is pending.
// Preserve shader-feature masks and repeated VideoOptions queries for each quality test.

void staticMeshInitTechniques()
{
    // Report build failure only when this call actually attempted initialization.
    bool built = false;
    bool attempted = false;
    if (g_staticMeshTechniquesPending)                  // byte_1023982C, 0x10116EAC
    {
        attempted = true;
        vc71::string prefix("STM");
        vc71::string mfx("shaders/RaShaderSTM.mfx");
        built = staticMeshBuildTechniqueMasks(mfx, prefix, kStaticMeshShaderBitNames,
                                              g_staticMeshTechniques,
                                              g_staticMeshQualityMasks);
    }

    if (!built)
    {
        // Diagnose an empty quality-mask table once. The following compatibility access
        // remains unchecked, so this report identifies the cause without suppressing the fault.
        static bool reported = false;
        if (attempted && !reported)
        {
            reported = true;
            ::renddx9abi::logf(
                "StaticMeshTemplate: the STM technique init FAILED, so the quality-mask vector is "
                "empty and the next material read will fault at staticMeshQualityMask() reading "
                "address 0. That fault is a SYMPTOM - look above for the metaShader load failure. "
                "Retail indexes this vector unguarded too; no guard is added here.");
        }
        return;                                         // 0x10116F27, jz loc_10117325
    }

    // the point-light quality setting.
    if (::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(3))
        staticMeshRegisterTechnique("Leafpointlight", 0xF0000400u);

    // the shadow quality setting.
    if (::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(4))
    {
        staticMeshRegisterTechnique("TrunkSTMDetailShadowed", 0xF9000200u);
        staticMeshRegisterTechnique("TrunkSTMBaseShadowed",   0xFA000200u);
        staticMeshRegisterTechnique("LeafShadowed",           0xF0000200u);
    }

    // a second, separately re-fetched shadow test, with the point-light test nested
    // inside it rather than ANDed with it. Written the way the image branches.
    if (::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(4))
    {
        if (::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(3))
            staticMeshRegisterTechnique("LeafpointlightShadowed", 0xF0000600u);
    }

    staticMeshRegisterTechnique("TrunkSTMDetail",    0xF9000000u);
    staticMeshRegisterTechnique("TrunkSTMBase",      0xFA000000u);
    staticMeshRegisterTechnique("Leaf",              0xF0000000u);
    staticMeshRegisterTechnique("Road",              0xF1000000u);
    staticMeshRegisterTechnique("RoadDetailNoBlend", 0xF4000000u);
    staticMeshRegisterTechnique("RoadDetail",        0xF2000000u);

    // Renderer vslot +0xAC, `return *(BYTE*)(this + 1892)`: the expansion-pack flag
    // picks the full water shader over the base one. The name is chosen before the string is
    // constructed, which is why this is one row and not two.
    staticMeshRegisterTechnique(
        ::dice::hfe::dx9rend::rendererIsExpansionPack() ? "Water" : "WaterBase", 0xF8000000u);

    staticMeshRegisterTechnique("Default",           0x01000000u);

    g_staticMeshTechniquesPending = 0;
}

} // anonymous namespace

// Keep the shared technique gate and initializer in their existing owner.
void staticMeshRearmTechniques()
{
    g_staticMeshTechniquesPending = 1;
}

void staticMeshPrimeTechniques()
{
    staticMeshInitTechniques();
}


// Exposed for Geom/StaticMesh.cpp's post-load (sub_100FC190), which resizes the instance's own two
// per-geom vectors with these same two shipped bodies. In retail these are std::vector<T>::resize
// instantiations, so there is no single "real" function to move here - a thin forward keeps the one
// implementation in the file that recovered it. [CHOSEN, not recovered] - plumbing only.

void staticMeshFloatVectorResize(vc71::vector_layout& v, unsigned int count, float value)
{
    floatVectorResize(v, count, value);
}

void staticMeshUnsignedVectorResize(vc71::vector_layout& v, unsigned int count, unsigned int value)
{
    unsignedVectorResize(v, count, value);
}


// The factory registered under "dice.hfe.geom.GeometryTemplate.StaticMesh".
// PE 0x100B064C (RefFactory_geom_GeometryTemplate_StaticMesh).

IBase* __fastcall createStaticMeshTemplate(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(0x2CC);
    StaticMeshTemplate* self = 0;
    if (storage)
        self = new (storage) StaticMeshTemplate();

    // The shipped body dispatches through the object it just built without testing it: a failed
    // allocation faults on the queryInterface call. Preserved.
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}


// Constructor - PE 0x10119E00. The base MeshTemplate constructor (sub_101924F0) runs first, then
// the two vtable slots, then the members in shipped order. The last store is `m_keepData = 0`,
// which reaches back into the base.

StaticMeshTemplate::StaticMeshTemplate()
    : MeshTemplate()
{
    m_refCount = 1;                       // +0xC8

    m_materials.first = 0;                // +0xD0 .. +0xD8; the allocator word is NOT written
    m_materials.last  = 0;
    m_materials.end   = 0;

    m_geoms.first = 0;                    // +0xE0 .. +0xE8; ditto
    m_geoms.last  = 0;
    m_geoms.end   = 0;

    // sub_1011B6F0 on &m_instances, then the header node is made the nil sentinel and self-linked.
    // The comparator word at +0xEC is never written, exactly as shipped.
    m_instances.head = setConstructHead();
    m_instances.head->isNil  = 1;
    m_instances.head->parent = m_instances.head;
    m_instances.head->left   = m_instances.head;
    m_instances.head->right  = m_instances.head;
    m_instances.size = 0;                 // +0xF4

    // +0xFC MeshBounds - eleven zero dwords with ONE byte store in the middle: the `dirty` flag at
    // +0x11C (this + 284) is set to 1, exactly as meshBoundsExpand would leave it. The three pad
    // bytes after it are NOT touched, so they stay whatever the allocator handed over.
    m_bounds.min.x = 0.0f; m_bounds.min.y = 0.0f; m_bounds.min.z = 0.0f;
    m_bounds.max.x = 0.0f; m_bounds.max.y = 0.0f; m_bounds.max.z = 0.0f;
    m_bounds.originRadius = 0.0f;
    m_bounds.radius = 0.0f;
    m_bounds.dirty = 1;
    m_bounds.extent.x = 0.0f; m_bounds.extent.y = 0.0f; m_bounds.extent.z = 0.0f;
    m_bounds.center.x = 0.0f; m_bounds.center.y = 0.0f; m_bounds.center.z = 0.0f;

    // sub_1007B220 - the function-local `static const Vec3(1,1,1)` MeshTemplate.cpp names
    // unitScale(). The PE calls it twice, once per scale, and copies z, y, x in that order; both
    // calls are kept. (MeshTemplate.cpp's copy is file-local, so this one is too.)
    m_decompressScale = unitScale();                 // +0x138
    m_compressScale   = unitScale();                 // +0x144
    // +0x150 m_uvDecompress is NOT written by the constructor.

    m_vertexStream = 0;                   // +0x154
    m_tangentStream = 0;                  // +0x16C
    // +0x184 m_hasTwoTangentSets is NOT written by the constructor - it starts as whatever the
    // 0x2CC-byte allocation held, and only buildVertexBuffers ever sets it. Left uninitialised.
    m_finished = 0;                       // +0x185
    m_hasAlphaMaterial = 0;               // +0x186
    m_shaderFlagsAll = 0;                 // +0x188
    m_shaderFlagsUsed = 0;                // +0x18C
    m_byte190 = 0;                        // +0x190
    m_skippingLods = 0;                   // +0x191
    m_lightmapOverride = 0;               // +0x192
    m_doNotGenerateLightmaps = 0;         // +0x193
    m_compressVertexData = 1;             // +0x294
    m_maxTextureRepeat = 16;              // +0x298
    m_isRoad = 0;                         // +0x29C
    m_roadNoBlend = 0;                    // +0x29D
    m_isVegetation = 0;                   // +0x29E
    m_dword2A0 = -1;                      // +0x2A0
    m_isWater = 0;                        // +0x2A4
    m_byte2A5 = 0;                        // +0x2A5
    m_float2A8 = 10.0f;                   // +0x2A8
    m_float2AC = 25.0f;                   // +0x2AC
    m_byte2B0 = 0;                        // +0x2B0
    m_keepVegetationNormals = 0;          // +0x2B1
    m_dword2B4 = 0;                       // +0x2B4
    m_dword2B8 = 0;                       // +0x2B8

    m_occluderGroups.first = 0;           // +0x2C0 .. +0x2C8
    m_occluderGroups.last  = 0;
    m_occluderGroups.end   = 0;

    m_uvStreams[0] = 0;                   // +0x158 .. +0x168 - stored after the tail above
    m_uvStreams[1] = 0;
    m_uvStreams[2] = 0;
    m_uvStreams[3] = 0;
    m_uvStreams[4] = 0;

    ::memset(m_lightmapSizes, 0xFF, sizeof(m_lightmapSizes));   // +0x194, 0x100 bytes

    m_keepData = 0;                       // +0x04, the very last store
}


// sub_10115FA0 - the first half of the destructor: release every material's shader record, then
// drop the six vertex streams and the tangent stream.

// The material walk is driven by the base's +0x50 lod-count vector, not by the material containers
// themselves: geoms = (m_lodCounts.last - m_lodCounts.first) / 4, and for each geom it visits
// m_lodCounts[geom] lods. A shader record is released through IBase slot +0x08 (see the file
// header for the corrected vtable reading).

namespace {

void staticMeshReleaseShadersAndStreams(StaticMeshTemplate* self)
{
    const unsigned int geomCount =
        self->m_lodCounts.first
            ? static_cast<unsigned int>(self->m_lodCounts.last - self->m_lodCounts.first) >> 2
            : 0u;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const int lodCount = reinterpret_cast<int*>(self->m_lodCounts.first)[geom];
        if (!lodCount)
            continue;

        vc71::vector_layout* lodVectors =
            reinterpret_cast<vc71::vector_layout*>(
                reinterpret_cast<vc71::vector_layout*>(self->m_materials.first)[geom].first);

        for (int lod = 0; lod < lodCount; ++lod)
        {
            vc71::vector_layout& materials = lodVectors[lod];
            if (!materials.first)
                continue;
            const unsigned int count =
                static_cast<unsigned int>(materials.last - materials.first) / 516u;
            for (unsigned int i = 0; i < count; ++i)
            {
                StaticMeshMaterial* material =
                    reinterpret_cast<StaticMeshMaterial*>(materials.first + 516u * i);
                if (material->m_shaderMaterial)
                    reinterpret_cast<IBase*>(material->m_shaderMaterial)->release();
            }
        }
    }

    // `if (p && !--p[1]) { vertexBufferDtor(p); engineFree(p); }` - the shipped inline wrapper
    // release, with sub_10014510 as the destructor. BufferManager.cpp carries the same dance as
    // vertexBufferRelease(); it is repeated inline here because the PE repeats it inline, seven
    // times, and because the wrapper destructor's resource-tracker row is that file's frontier.
    if (self->m_vertexStream)
        ::dice::hfe::dx9rend::vertexBufferRelease(self->m_vertexStream);
    for (int i = 0; i < 5; ++i)
        if (self->m_uvStreams[i])
            ::dice::hfe::dx9rend::vertexBufferRelease(self->m_uvStreams[i]);
    if (self->m_tangentStream)
        ::dice::hfe::dx9rend::vertexBufferRelease(self->m_tangentStream);
}

} // anonymous namespace


// Destructor body - PE 0x10119FF0. Both vtable slots are restored first (MSVC's usual
// destructor preamble), then the members go down in shipped order: shader records and streams,
// the occluder groups, the instance set, the geom vector, the material vector, base.

void StaticMeshTemplate::destroyMembers()
{
    staticMeshReleaseShadersAndStreams(this);
    occluderGroupsDestroy(m_occluderGroups);                      // sub_10120D40 on +0x2BC

    setClear(m_instances);                                        // sub_101006E0(begin, end)
    ::renddx9abi::engineFree(m_instances.head);
    m_instances.head = 0;
    m_instances.size = 0;

    if (m_geoms.first)
    {
        staticMeshGeomsDestroyRange(m_geoms.first, m_geoms.last);
        ::renddx9abi::engineFree(m_geoms.first);
    }
    m_geoms.first = 0;
    m_geoms.last  = 0;
    m_geoms.end   = 0;

    if (m_materials.first)
    {
        staticMeshMaterialsDestroyGeoms(reinterpret_cast<unsigned char*>(m_materials.first),
                                        reinterpret_cast<unsigned char*>(m_materials.last));
        ::renddx9abi::engineFree(m_materials.first);
    }
    m_materials.first = 0;
    m_materials.last  = 0;
    m_materials.end   = 0;

    MeshTemplate::destroyMembers();
}

// the deleting destructor at slot +0xB0.
void* StaticMeshTemplate::destroy(unsigned char flags)
{
    destroyMembers();
    if ((flags & 1) != 0)
        ::renddx9abi::engineFree(this);
    return this;
}


// The overridden IBase quartet. StaticMesh counts at +0xC8 rather than the base's +0x10; the
// queryInterface at 0x100EE940 is shared with MeshTemplate (one body, two vtables).

void StaticMeshTemplate::addRef()
{
    ++m_refCount;
}

int StaticMeshTemplate::getRef()
{
    return m_refCount;
}

int StaticMeshTemplate::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        destroy(1);                                               // through the +0xB0 slot
    return remaining;
}

void* StaticMeshTemplate::queryInterface(unsigned int iid) const
{
    if (iid != 1 && iid != 0x17701FFFu)
    {
        // The `&& this` test is the shipped one. It cannot fail in a member function, but MSVC
        // emitted it because the returned pointer is `this + 12` and a null base must stay null.
        if (iid == 0x3709A4FFu && this)
            return const_cast<void*>(static_cast<const void*>(
                static_cast<const ::dice::hfe::io::IConsoleSaveable*>(this)));
        return 0;
    }
    return const_cast<StaticMeshTemplate*>(this);
}

// a plain load of GeometryTemplate_StaticMesh_reflectionTypeId.
unsigned int StaticMeshTemplate::getClassId() const
{
    return 0x870CA653u;
}


// createInstance - PE 0x10116E00.

void* StaticMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(0x37C);
    void* instance = 0;
    if (storage)
    {
        // sub_100FDC20 with ecx = the allocation and the template pushed.  ;
        // see Geom/StaticMesh.cpp.  The 0x37C here and the class's own static_assert are the same
        // number checked from both sides.
        instance = staticMeshConstruct(storage, this);
    }

    setInsert(m_instances, instance);                             // sub_1011BEC0 into +0xEC

    if (!m_loaded && !resourceLoadingDeferred())
        load();                                                   // through the +0x2C slot

    m_keepData = 1;
    return instance;
}


// Loads <path>Meshes/<name>.staticMesh; returns false when already loaded.
// Both candidate paths intentionally use the same expression and are tested separately.
// The occluder path omits the Meshes/ component.

bool StaticMeshTemplate::load()
{
    if (m_loaded)
        return false;

    static const char* const kMeshesFolder = "Meshes/";           // unk_10263DD4

    vc71::string candidateA(m_path.c_str());
    candidateA += kMeshesFolder;
    candidateA += m_name;
    candidateA += ".staticMesh";

    vc71::string candidateB(m_path.c_str());
    candidateB += kMeshesFolder;
    candidateB += m_name;
    candidateB += ".staticMesh";

    ::renddx9abi::HostFileManager* files = ::dice::hfe::fileManager;

    if (!files->fileExists(::renddx9abi::makeHostString(candidateA)))     // +0x1C
    {
        // StaticMeshTemplate.cpp:455, Debug type 6.
        ::renddx9abi::logf("StaticMeshTemplate.cpp:455: Cannot load StaticMesh %s "
                           "Please make sure it exists!", candidateA.c_str());
        return false;
    }

    m_isVegetation =
        static_cast<unsigned char>(findCaseSensitive(candidateA, "vegitation") != kNpos);
    m_isWater =
        static_cast<unsigned char>(findCaseInsensitive(candidateA,
                                                       vc71::string("objects/water/")) != kNpos);
    m_isRoad =
        static_cast<unsigned char>(findCaseInsensitive(candidateA,
                                                       vc71::string("objects/roads/")) != kNpos);
    if (m_isRoad)
        m_roadNoBlend =
            static_cast<unsigned char>(findCaseInsensitive(candidateA,
                                                           vc71::string("noBlend")) != kNpos);

    m_dword8C = 0;

    // The second existence test picks which of the two identical paths is opened. Since they are
    // the same string, candidate B is always the one taken when the file is there at all.
    const bool haveB = files->fileExists(::renddx9abi::makeHostString(candidateB));
    ::renddx9abi::HostStream* stream = files->open(
        ::renddx9abi::makeHostString(haveB ? candidateB : candidateA),
        static_cast<::renddx9abi::HostFileMode>(2),
        static_cast<::renddx9abi::HostFileAccess>(0), 0);           // +0x18

    if (stream)
    {
        const bool ok = loadFromStream(static_cast<::renddx9abi::HostNonlinearStream*>(stream));
        stream->release();                                          // +0x08
        if (!ok)
            return false;
    }
    // note THE missing ARM: when open() returns null the shipped body does NOT return - it falls
    // straight through and marks the template loaded. Preserved.

    m_loaded = 1;

    vc71::string occPath(m_path.c_str());                           // sub_10001920: path + name
    occPath += m_name;
    occPath += ".occ";

    ::renddx9abi::HostStream* occ = files->open(
        ::renddx9abi::makeHostString(occPath),
        static_cast<::renddx9abi::HostFileMode>(2),
        static_cast<::renddx9abi::HostFileAccess>(0), 0);
    if (occ)
    {
        loadOccluders(occ);
        occ->release();                                             // +0x08
    }

    // Walk the live instances and hand each the freshly loaded template. The set is empty until
    // createInstance has run, and the callback belongs to the unrecovered instance class.
    for (vc71::set_node* node = m_instances.head ? m_instances.head->left : 0;
         node && node != m_instances.head;
         node = vc71::set_layout::next(node))
    {
        // The set stores the instance pointer in its node's `key` word (set_node +0x0C).
        StaticMesh* const instance = reinterpret_cast<StaticMesh*>(node->key);
        if (instance)
            instance->postLoad();
    }

    return true;
}


// setLodDistance - PE 0x10119110. Two arms: with a loaded geom vector it writes into the geom's
// own lodDistances (shifted down by the number of lods readLods skipped); without one it grows the
// base's +0x60 vector<vector<float>> and pre-fills the defaults.

void StaticMeshTemplate::setLodDistance(int geom, int lod, float distance)
{
    if (geom < 0 || lod < 0 || distance < 0.0f)
    {
        ::renddx9abi::logf("StaticMeshTemplate.cpp:555: Negative subgeometry lodswitch or "
                           "distance %s", m_name.c_str());
        return;
    }

    if (m_geoms.first && ((m_geoms.last - m_geoms.first) >> 5) != 0)
    {
        const int geomCount = static_cast<int>((m_geoms.last - m_geoms.first) >> 5);
        if (geom >= geomCount)
        {
            ::renddx9abi::logf("StaticMeshTemplate.cpp:563:  Invalid Subgeometry %s",
                               m_name.c_str());
            return;
        }

        const int skipped = reinterpret_cast<int*>(m_geomInts.first)[geom];
        StaticMeshGeom* g = reinterpret_cast<StaticMeshGeom*>(m_geoms.first) + geom;
        const int stored  = static_cast<int>(vectorSizeBytes(g->lodDistances) / sizeof(float));
        const int limit   = skipped + stored;                       // sub_1004E300 == size()

        if (lod >= limit)
        {
            // The decompiler shuffles the three inserted values across phantom stack slots; the
            // literal text and its operands are recovered, the exact interleave is [INFERRED].
            ::renddx9abi::logf("StaticMeshTemplate.cpp:571:  Invalid lodSwitch %d must be smaller "
                               "than %d %s", lod, limit, m_name.c_str());
            return;
        }

        if (lod >= skipped)
            reinterpret_cast<float*>(g->lodDistances.first)[lod - skipped] = distance;
        return;
    }

    // the not-yet-loaded arm
    bool grewOuter = false;
    const unsigned int outerSize = vectorSizeBytes(m_lodDistances) / sizeof(vc71::vector_layout);
    if (outerSize < static_cast<unsigned int>(geom + 1))
    {
        floatVectorVectorResize(m_lodDistances, static_cast<unsigned int>(geom + 1));
        grewOuter = true;
    }

    vc71::vector_layout* inner =
        reinterpret_cast<vc71::vector_layout*>(m_lodDistances.first) + geom;
    const unsigned int innerSize = vectorSizeBytes(*inner) / sizeof(float);
    if (innerSize < static_cast<unsigned int>(lod + 1))
    {
        floatVectorResize(*inner, static_cast<unsigned int>(lod + 1), 0.0f);
        if (grewOuter)
        {
            // The shipped fill is unrolled four wide (25/50/75/100, then +100 per group) with a
            // scalar tail whose seed is `25 * (i + 1)`. Both forms compute the same sequence.
            float* values = reinterpret_cast<float*>(inner->first);
            for (int i = 0; i < lod; ++i)
                values[i] = static_cast<float>((i + 1) * 25);
        }
    }

    reinterpret_cast<float*>(
        (reinterpret_cast<vc71::vector_layout*>(m_lodDistances.first) + geom)->first)[lod] = distance;
}

// Unlike the setter, the getter does NOT shift by the skipped-lod count.
float StaticMeshTemplate::getLodDistance(int geom, int lod)
{
    if (m_geoms.first && ((m_geoms.last - m_geoms.first) >> 5) != 0)
    {
        StaticMeshGeom* g = reinterpret_cast<StaticMeshGeom*>(m_geoms.first) + geom;
        return reinterpret_cast<float*>(g->lodDistances.first)[lod];
    }
    return -1.0f;
}


// The base's +0x80 map<unsigned, float>, keyed (geom << 16) | (lod << 8) | material. Only the LOW
// byte of geom and lod reaches the key - the shipped body loads each through a byte register.

namespace {

unsigned int materialValueKey(int geom, int lod, unsigned char material)
{
    const unsigned short pair =
        static_cast<unsigned short>((static_cast<unsigned char>(geom) << 8) |
                                     static_cast<unsigned char>(lod));
    return static_cast<unsigned int>(material) | (static_cast<unsigned int>(pair) << 8);
}

} // anonymous namespace


void StaticMeshTemplate::setMaterialValue(int geom, int lod, unsigned char material, float value)
{
    const unsigned int key = materialValueKey(geom, lod, material);

    if (floatMapFind(m_materialValues, key) == reinterpret_cast<FloatMapNode*>(m_materialValues.head))
        floatMapInsert(m_materialValues, key, value);
    else
        *floatMapAt(m_materialValues, key) = value;
}


float StaticMeshTemplate::getMaterialValue(int geom, int lod, unsigned char material)
{
    const unsigned int key = materialValueKey(geom, lod, material);

    if (floatMapFind(m_materialValues, key) == reinterpret_cast<FloatMapNode*>(m_materialValues.head))
        return 15.0f;
    return *floatMapAt(m_materialValues, key);
}


// The flag accessors. Each is one or two instructions in the image; the addresses are on the line.

bool StaticMeshTemplate::isWater() const                       { return m_isWater != 0; }
void StaticMeshTemplate::setDoNotGenerateLightmaps(bool value) { m_doNotGenerateLightmaps = value ? 1 : 0; }
bool StaticMeshTemplate::getDoNotGenerateLightmaps() const     { return m_doNotGenerateLightmaps != 0; }
void StaticMeshTemplate::setLightmapOverride(bool value)       { m_lightmapOverride = value ? 1 : 0; }
bool StaticMeshTemplate::getLightmapOverride() const           { return m_lightmapOverride != 0; }
void StaticMeshTemplate::setCompressVertexData(bool value)     { m_compressVertexData = value ? 1 : 0; }
bool StaticMeshTemplate::getCompressVertexData() const         { return m_compressVertexData != 0; }
void StaticMeshTemplate::setMaxTextureRepeat(int value)        { m_maxTextureRepeat = value; }
int  StaticMeshTemplate::getMaxTextureRepeat() const           { return m_maxTextureRepeat; }

// The write only happens with the lightmap override set, and when it is clear the
// shipped body returns an uninitialised eax - harmless here because the slot is void.
void StaticMeshTemplate::setLightmapSize(int a, int b, int size)
{
    if (m_lightmapOverride)
        m_lightmapSizes[b + 8 * a] = size;
}

// no range check, no override test.
int StaticMeshTemplate::getLightmapSize(int a, int b)
{
    return m_lightmapSizes[8 * a + b];
}


// getMaterialCount - PE 0x10115DE0. Sums the material count of `lod` across every geom.

int StaticMeshTemplate::getMaterialCount(unsigned int lod)
{
    int total = 0;
    unsigned int geom = 0;
    for (;;)
    {
        if (!m_geoms.first ||
            geom >= static_cast<unsigned int>((m_geoms.last - m_geoms.first) >> 5))
            return total;

        StaticMeshGeom* g = reinterpret_cast<StaticMeshGeom*>(m_geoms.first) + geom;
        if (g->lods.first &&
            lod < static_cast<unsigned int>(g->lods.last - g->lods.first) / sizeof(StaticMeshLod))
        {
            vc71::vector_layout* lodVectors =
                reinterpret_cast<vc71::vector_layout*>(
                    reinterpret_cast<vc71::vector_layout*>(m_materials.first)[geom].first);
            vc71::vector_layout& materials = lodVectors[lod];
            if (materials.first)
                total += static_cast<int>(
                    static_cast<unsigned int>(materials.last - materials.first) / 516u);
        }
        ++geom;
    }
}


// getLodPrimitiveCount - PE 0x10115EB0. The same walk, summing each material's own
// getPrimitiveCount (StaticMeshMaterial vtable +0x10).

int StaticMeshTemplate::getLodPrimitiveCount(unsigned int lod)
{
    int total = 0;
    unsigned int geom = 0;
    for (;;)
    {
        if (!m_geoms.first ||
            geom >= static_cast<unsigned int>((m_geoms.last - m_geoms.first) >> 5))
            return total;

        StaticMeshGeom* g = reinterpret_cast<StaticMeshGeom*>(m_geoms.first) + geom;
        if (g->lods.first &&
            lod < static_cast<unsigned int>(g->lods.last - g->lods.first) / sizeof(StaticMeshLod))
        {
            vc71::vector_layout* lodVectors =
                reinterpret_cast<vc71::vector_layout*>(
                    reinterpret_cast<vc71::vector_layout*>(m_materials.first)[geom].first);
            vc71::vector_layout& materials = lodVectors[lod];
            unsigned int index = 0;
            for (;;)
            {
                if (!materials.first)
                    break;
                if (index >= static_cast<unsigned int>(materials.last - materials.first) / 516u)
                    break;
                StaticMeshMaterial* material =
                    reinterpret_cast<StaticMeshMaterial*>(materials.first + 516u * index);
                total += material->getPrimitiveCount();
                ++index;
            }
        }
        ++geom;
    }
}


// decompressPosition - PE 0x10115A70. Multiply by the compress scale, truncate TO INT, multiply by
// the decompress scale. The y and z products are computed before x is stored; that order is kept
// because the (int) truncation makes it observable.

Vec3* StaticMeshTemplate::decompressPosition(Vec3* out, const Vec3* in)
{
    const float y = static_cast<float>(static_cast<int>(m_compressScale.y * in->y));
    const float z = static_cast<float>(static_cast<int>(m_compressScale.z * in->z));
    const float yOut = y * m_decompressScale.y;
    const float zOut = z * m_decompressScale.z;
    out->x = static_cast<float>(static_cast<int>(m_compressScale.x * in->x)) * m_decompressScale.x;
    out->y = yOut;
    out->z = zOut;
    return out;
}


// Slot +0xAC - PE 0x10118170. See the file header: this is a preload pass, not a release.

// The once-initialised static vector at unk_1025B8B8 collects the distinct textures of the shader
// records of geom 0, lod 0 only (the shipped walk never advances past the first lod of the first
// geom) and preloads each. The template's +0x08 is then stamped with the renderer's frame ordinal.

namespace {

vc71::vector_layout g_preloadTextures = { 0, 0, 0, 0 };        // unk_1025B8B8
bool                g_preloadTexturesReady = false;            // dword_1025B8C8 bit 0

} // anonymous namespace

void StaticMeshTemplate::releaseDeviceResources()
{
    if (!g_preloadTexturesReady)
    {
        g_preloadTexturesReady = true;
        g_preloadTextures.first = 0;
        g_preloadTextures.last  = 0;
        g_preloadTextures.end   = 0;
        // atexit(sub_101D95D0) - the static vector's teardown. The rebuild's process teardown
        // releases the heap wholesale, so the registration is not reproduced.
    }

    g_preloadTextures.last = g_preloadTextures.first;          // sub_100EBAD0(0, 0) - resize(0)

    if (m_vertexStream)
        resourcePreload(m_vertexStream);
    for (int i = 0; i < 5; ++i)
        if (m_uvStreams[i])
            resourcePreload(m_uvStreams[i]);
    if (m_tangentStream)
        resourcePreload(m_tangentStream);

    // No null test, exactly as shipped - see resourcePreload's note.
    resourcePreload(m_indexBuffer);

    if (m_materials.first && ((m_materials.last - m_materials.first) >> 4) != 0)
    {
        vc71::vector_layout* lodVectors =
            reinterpret_cast<vc71::vector_layout*>(
                reinterpret_cast<vc71::vector_layout*>(m_materials.first)[0].first);
        char* first = lodVectors[0].first;
        char* last  = lodVectors[0].last;
        for (char* p = first; p != last; p += 516)
        {
            StaticMeshMaterial* material = reinterpret_cast<StaticMeshMaterial*>(p);
            void** textures = material->m_shaderMaterial->textures;      // +0x28, eight slots
            for (int slot = 0; slot < 8; ++slot)
            {
                void* texture = textures[slot];
                if (!texture)
                    continue;

                bool seen = false;
                for (char* q = g_preloadTextures.first; q != g_preloadTextures.last; q += 4)
                    if (*reinterpret_cast<void**>(q) == texture)
                    {
                        seen = true;
                        break;
                    }
                if (seen)
                    continue;

                const unsigned int used = vectorSizeBytes(g_preloadTextures) / 4;
                if (g_preloadTextures.first && used < vectorCapacityBytes(g_preloadTextures) / 4)
                {
                    *reinterpret_cast<void**>(g_preloadTextures.last) = texture;
                    g_preloadTextures.last += 4;
                }
                else
                {
                    vectorReserveRaw(g_preloadTextures, 4, used + 1);
                    if (g_preloadTextures.first)
                    {
                        *reinterpret_cast<void**>(g_preloadTextures.last) = texture;
                        g_preloadTextures.last += 4;
                    }
                }
            }
        }
    }

    for (char* q = g_preloadTextures.first; q != g_preloadTextures.last; q += 4)
        resourcePreload(*reinterpret_cast<void**>(q));

    m_dword08 = static_cast<int>(::dice::hfe::dx9rend::rendererFrameOrdinal());   // +0x08
}

// the occluder-group accessors, 48 bytes per group.
int StaticMeshTemplate::getOccluderGroupCount()
{
    if (!m_occluderGroups.first)
        return 0;
    return static_cast<int>((m_occluderGroups.last - m_occluderGroups.first) / 48);
}

void* StaticMeshTemplate::getOccluderGroup(unsigned int index)
{
    if (m_occluderGroups.first &&
        (m_occluderGroups.last - m_occluderGroups.first) / 48 != 0 &&
        m_occluderGroups.first &&
        index < static_cast<unsigned int>((m_occluderGroups.last - m_occluderGroups.first) / 48))
        return m_occluderGroups.first + 48 * index;
    return 0;
}

// load stock .occ groups into m_occluderGroups.
// The header token is ignored; corner indices are read as int then truncated to byte,
// and vertex x is negated. Record vectors transfer ownership; scratch storage is freed.
// Preserve the missing allocation/range guards and VC7.1 vector allocator words.

namespace {

// [CHOSEN, not recovered] Scanner matching the compatibility stream's extraction rules.
// EOF is set even when the final token was extracted; failed numeric reads leave the
// destination unchanged. The backing buffer includes a private NUL terminator.
class OccScanner
{
public:
    OccScanner(const char* buffer, unsigned int size)
        : m_p(buffer), m_end(buffer + size), m_eof(false) {}

    bool eof() const { return m_eof; }

    // std::operator>>(istream&, string&). The token is returned as a range into the buffer: both
    // uses here either compare it or throw it away, so nothing is copied and there is no length
    // limit to get wrong.
    bool token(const char** begin, const char** end)
    {
        skipSpace();
        if (m_p >= m_end)
        {
            m_eof = true;                  // eofbit, and failbit - nothing was extracted
            return false;
        }
        *begin = m_p;
        while (m_p < m_end && !isSpace(*m_p))
            ++m_p;
        *end = m_p;
        if (m_p >= m_end)
            m_eof = true;                  // eofbit even though a token was extracted
        return true;
    }

    bool tokenIs(const char* literal)
    {
        const char* begin = 0;
        const char* end   = 0;
        if (!token(&begin, &end))
            return false;
        const unsigned int n = static_cast<unsigned int>(end - begin);
        return ::strlen(literal) == n && ::memcmp(begin, literal, n) == 0;
    }

    // operator>>(int&) / operator>>(float&): on failure the destination is left alone.
    bool readInt(int* out)
    {
        skipSpace();
        if (m_p >= m_end)
        {
            m_eof = true;
            return false;
        }
        char* stop = 0;
        const long value = ::strtol(m_p, &stop, 10);
        if (stop == m_p)
            return false;
        m_p = stop;
        *out = static_cast<int>(value);
        if (m_p >= m_end)
            m_eof = true;
        return true;
    }

    bool readFloat(float* out)
    {
        skipSpace();
        if (m_p >= m_end)
        {
            m_eof = true;
            return false;
        }
        char* stop = 0;
        const double value = ::strtod(m_p, &stop);
        if (stop == m_p)
            return false;
        m_p = stop;
        *out = static_cast<float>(value);
        if (m_p >= m_end)
            m_eof = true;
        return true;
    }

private:
    // The "C" locale's space|blank set, written out so no locale or signed-char question arises.
    static bool isSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    }

    void skipSpace()
    {
        while (m_p < m_end && isSpace(*m_p))
            ++m_p;
    }

    const char* m_p;
    const char* m_end;
    bool        m_eof;
};

// sub_100777A0 - Vec3 normalize in place, with the shipped |v|^2 within-epsilon short circuits.
// The same three-line body StaticMesh.cpp and StaticMeshMaterial.cpp each carry at their own file
// scope; repeated here for the same reason - all of them are file-local in the image.
void occNormalizeInPlace(float* v)
{
    const float lengthSquared = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (::fabs(static_cast<double>(lengthSquared) - 1.0) < 0.00000011920929)
        return;
    if (::fabs(static_cast<double>(lengthSquared)) < 0.00000011920929)
    {
        v[0] = 0.0f; v[1] = 0.0f; v[2] = 0.0f;
        return;
    }
    const float inverse = static_cast<float>(1.0 / ::sqrt(static_cast<double>(lengthSquared)));
    v[0] *= inverse; v[1] *= inverse; v[2] *= inverse;
}


// Finds or creates an edge and appends its reference to the plane.
// Reversed orientation matches; same orientation logs an error and keeps searching.
// The stored index is narrowed to a byte, with a separate new-edge flag.

void addOccluderEdge(unsigned char* group, unsigned char* plane,
                     unsigned char a, unsigned char b)
{
    vc71::vector_layout& edges = *reinterpret_cast<vc71::vector_layout*>(group + 0x20);
    vc71::vector_layout& refs  = *reinterpret_cast<vc71::vector_layout*>(plane + 0x10);

    const unsigned int count = edges.first
        ? static_cast<unsigned int>(edges.last - edges.first) / 4u
        : 0u;

    for (unsigned int i = 0; i < count; ++i)
    {
        const unsigned char* const edge =
            reinterpret_cast<const unsigned char*>(edges.first) + 4u * i;

        if (edge[0] == a && edge[1] == b)
        {
            // StaticMeshTemplate.cpp:310, Debug type 6 - and it does NOT stop.
            ::renddx9abi::logf("StaticMeshTemplate.cpp:310: EDGE ERROR");
        }

        if (edge[0] == b && edge[1] == a)
        {
            const unsigned short ref = static_cast<unsigned short>(static_cast<unsigned char>(i));
            vc71::vectorPushBackIn(refs, 2u, &ref);
            return;
        }
    }

    // Not there. The index is taken before the push, so it is the slot the new edge lands in.
    const unsigned char newIndex = static_cast<unsigned char>(count);

    unsigned char edge[4];
    edge[0] = a;
    edge[1] = b;
    // warning: the shipped element's upper two bytes are the caller's register leftovers - the
    // record is pushed straight out of the argument dword before BYTE2/BYTE3 are rewritten.
    // Nothing ever reads them (the plane pass reads [0] and [1], and the consumer does not touch
    // the edge list at all), so they are zeroed here rather than copied indeterminate.
    edge[2] = 0;
    edge[3] = 0;
    vc71::vectorPushBackIn(edges, 4u, edge);

    const unsigned short ref =
        static_cast<unsigned short>(static_cast<unsigned short>(newIndex) | 0x0100u);
    vc71::vectorPushBackIn(refs, 2u, &ref);
}

} // namespace

void StaticMeshTemplate::loadOccluders(::renddx9abi::HostStream* stream)
{
    // 0x1011A0DB -> the stream's own +0x1C, operator new, then the whole-file read.
    const unsigned int size = static_cast<unsigned int>(stream->getAvailableBytes());   // +0x1C
    char* const buffer = static_cast<char*>(::renddx9abi::engineAlloc(size + 1u));
    stream->read(buffer, size);                                                         // +0x10
    buffer[size] = '\0';

    OccScanner scanner(buffer, size);

    // The version line, read and dropped. warning: its state is NOT tested (0x1011A179 is
    // followed straight by the second string's constructor) - only the marker read below is.
    const char* tokenBegin = 0;
    const char* tokenEnd   = 0;
    scanner.token(&tokenBegin, &tokenEnd);

    // EOF is checked before matching, so a final token without trailing whitespace is skipped.
    bool isGroup = scanner.tokenIs("GROUP");

    int corner = 0;        // var_68: NOT pre-zeroed per read by the shipped body; it persists

    while (!scanner.eof() && isGroup)
    {
        // The 48-byte group record, built on the stack exactly as the shipped body builds it.
        // The nine stores at 0x1011A1EB..0x1011A203 leave all three allocator words untouched;
        // zeroing the whole record avoids copying indeterminate bytes into the vector and is
        // otherwise identical - the same choice StaticMesh.cpp makes for its 36-byte record.
        unsigned char group[48];
        for (unsigned int z = 0; z < sizeof(group); ++z)
            group[z] = 0;

        vc71::vector_layout& planes = *reinterpret_cast<vc71::vector_layout*>(group + 0x00);
        vc71::vector_layout& points = *reinterpret_cast<vc71::vector_layout*>(group + 0x10);
        vc71::vector_layout& edges  = *reinterpret_cast<vc71::vector_layout*>(group + 0x20);

        // the quad corner indices, into a scratch byte vector (0x1011A230..0x1011A28E)
        // This fourth vector is not part of the record; it is freed at the end of the group.
        vc71::vector_layout cornerBytes;
        cornerBytes.alloc = 0;
        cornerBytes.first = 0;
        cornerBytes.last  = 0;
        cornerBytes.end   = 0;

        int quadCount = 0;
        scanner.readInt(&quadCount);

        for (int k = 0; k < 4 * quadCount; ++k)
        {
            scanner.readInt(&corner);
            const unsigned char index = static_cast<unsigned char>(corner);
            vc71::vectorPushBackIn(cornerBytes, 1u, &index);
        }

        // the vertices (0x1011A290..0x1011A2FE)
        int vertexCount = 0;
        scanner.readInt(&vertexCount);

        for (int v = 0; v < vertexCount; ++v)
        {
            float position[3];
            position[0] = 0.0f;                 // pre-zeroed every iteration, as shipped
            position[1] = 0.0f;
            position[2] = 0.0f;
            scanner.readFloat(&position[0]);
            scanner.readFloat(&position[1]);
            scanner.readFloat(&position[2]);
            position[0] = -position[0];         // X, and only X
            vc71::vectorPushBackIn(points, 12u, position);
        }

        // one plane per quad, four edges per plane (0x1011A314..0x1011A3D1)
        for (int quad = 0; quad < quadCount; ++quad)
        {
            // The 32-byte template: the shipped body zeroes the normal and the nested vector's
            // three words and leaves `d` and the allocator word alone (0x1011A314..0x1011A332 is
            // six stores, not eight). `d` is overwritten by the plane pass below for every
            // element, so zeroing all 32 changes nothing observable.
            unsigned char blank[32];
            for (unsigned int z = 0; z < sizeof(blank); ++z)
                blank[z] = 0;
            vc71::vectorPushBackIn(planes, 32u, blank);

            for (int c = 0; c < 4; ++c)
            {
                const int next = (c == 3) ? 0 : c + 1;
                const unsigned char* const quadCorners =
                    reinterpret_cast<const unsigned char*>(cornerBytes.first) + 4 * quad;

                // Recomputed per corner, as shipped: planes.last - 32 is the element just added.
                unsigned char* const plane =
                    reinterpret_cast<unsigned char*>(planes.last) - 32;

                addOccluderEdge(group, plane, quadCorners[c], quadCorners[next]);
            }
        }

        // Edge 0 and edge 1 intentionally use opposite direction-flag conventions.
        // The plane anchor always uses edge 0's v0, independently of that flag.
        if (planes.first && (static_cast<unsigned int>(planes.last - planes.first) >> 5) != 0)
        {
            const unsigned int planeCount =
                static_cast<unsigned int>(planes.last - planes.first) >> 5;

            for (unsigned int p = 0; p < planeCount; ++p)
            {
                unsigned char* const plane =
                    reinterpret_cast<unsigned char*>(planes.first) + 32u * p;
                const vc71::vector_layout& refs =
                    *reinterpret_cast<const vc71::vector_layout*>(plane + 0x10);

                const unsigned char* const ref =
                    reinterpret_cast<const unsigned char*>(refs.first);
                const unsigned char* const edgeBase =
                    reinterpret_cast<const unsigned char*>(edges.first);
                const float* const pointBase = reinterpret_cast<const float*>(points.first);

                // edge 0 -> the first tangent, and the anchor
                const unsigned char* const edge0 = edgeBase + 4u * ref[0];
                const float* firstA;
                const float* secondA;
                if (ref[1] != 0)
                {
                    firstA  = pointBase + 3u * edge0[0];
                    secondA = pointBase + 3u * edge0[1];
                }
                else
                {
                    firstA  = pointBase + 3u * edge0[1];
                    secondA = pointBase + 3u * edge0[0];
                }
                const float ax = firstA[0] - secondA[0];
                const float ay = firstA[1] - secondA[1];
                const float az = firstA[2] - secondA[2];
                const float* const anchor = pointBase + 3u * edge0[0];

                // edge 1 -> the second tangent
                const unsigned char* const edge1 = edgeBase + 4u * ref[2];
                const float* firstB;
                const float* secondB;
                if (ref[3] == 0)
                {
                    firstB  = pointBase + 3u * edge1[0];
                    secondB = pointBase + 3u * edge1[1];
                }
                else
                {
                    firstB  = pointBase + 3u * edge1[1];
                    secondB = pointBase + 3u * edge1[0];
                }
                const float bx = firstB[0] - secondB[0];
                const float by = firstB[1] - secondB[1];
                const float bz = firstB[2] - secondB[2];

                // cross(a, b), written in the shipped operand order so the rounding matches:
                // 0x1011A50x emits y first (v51), then x, then z.
                float normal[3];
                normal[1] = az * bx - bz * ax;
                normal[0] = bz * ay - by * az;
                normal[2] = by * ax - ay * bx;

                occNormalizeInPlace(normal);

                float* const out = reinterpret_cast<float*>(plane);
                out[0] = normal[0];
                out[1] = normal[1];
                out[2] = normal[2];
                // The shipped grouping, z and y first: -((nz*p.z + ny*p.y) + nx*p.x).
                out[3] = -((normal[2] * anchor[2] + normal[1] * anchor[1]) + normal[0] * anchor[0]);
            }
        }

        // the record goes in, and only then do the buffers get released
        // Ownership of the three record vectors moves here - see the header note. The scratch
        // corner bytes are not part of the record, so they are freed.
        vc71::vectorPushBackIn(m_occluderGroups, 48u, group);

        if (cornerBytes.first)
            ::renddx9abi::engineFree(cornerBytes.first);

        // the next marker (0x1011A5FE..0x1011A63D)
        isGroup = scanner.tokenIs("GROUP");
    }

    // the file buffer, and then the stream objects unwind.
    ::renddx9abi::engineFree(buffer);
}


// Reads each geometry's LODs, overwriting skipped records before retaining later LODs.
// Header zero with version <10 uses legacy bounds; other files use min/max bounds.

void StaticMeshTemplate::readLods(::renddx9abi::HostNonlinearStream* stream)
{
    const unsigned int geomCount =
        m_lodCounts.first
            ? static_cast<unsigned int>(m_lodCounts.last - m_lodCounts.first) >> 2
            : 0u;

    staticMeshGeomsResize(m_geoms, geomCount);
    if (!geomCount)
        return;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        // The renderer's skip budget: (rendererLodSkipBias + this->+0x2B8) clamped at zero, plus
        // the flat base, then capped by the template's own per-role maximum.
        int budget = rendererLodSkipBias() + m_dword2B8;
        if (budget <= 0)
            budget = 0;
        budget += rendererLodSkipBase();

        int cap = 0;
        if (geom == 0)
            cap = m_maxSkip3pLods;                                // +0xC0
        else if (geom == 1)
            cap = m_maxSkipWreckLods;                             // +0xC4

        const int lodCount = reinterpret_cast<int*>(m_lodCounts.first)[geom];
        if (cap < budget)
            budget = cap;

        int skip = lodCount - 1;
        if (skip >= budget)
            skip = budget;

        reinterpret_cast<int*>(m_geomInts.first)[geom] = skip;     // +0x70 remembers the skip
        const int kept = lodCount - skip;
        reinterpret_cast<int*>(m_lodCounts.first)[geom] = kept;

        StaticMeshGeom* g = reinterpret_cast<StaticMeshGeom*>(m_geoms.first) + geom;
        staticMeshLodsResize(g->lods, static_cast<unsigned int>(kept));
        floatVectorResize(g->lodDistances, static_cast<unsigned int>(kept - 1), 0.0f);

        int remainingSkips = skip;
        int lodIndex = 0;
        int byteOffset = 0;
        if (kept > 0)
        {
            do
            {
                StaticMeshLod* lod =
                    reinterpret_cast<StaticMeshLod*>(g->lods.first + byteOffset);
                ::memset(lod, 0, sizeof(StaticMeshLod));

                if (m_head || m_version >= 10u)
                    meshBoundsReadMinMax(lod->bounds, stream);
                else
                    meshBoundsReadLegacy(lod->bounds, stream);

                meshBoundsExpand(m_bounds, lod->bounds.min);       // sub_100796A0, twice
                meshBoundsExpand(m_bounds, lod->bounds.max);

                stream->read(&lod->nodeCount, 4);                  // +0x10
                stream->seek(static_cast<::renddx9abi::HostSeekOrigin>(1),
                             static_cast<int>(lod->nodeCount << 6));   // +0x30

                if (remainingSkips > 0)
                {
                    --remainingSkips;
                    --lodIndex;
                    byteOffset -= static_cast<int>(sizeof(StaticMeshLod));
                }
                byteOffset += static_cast<int>(sizeof(StaticMeshLod));
                ++lodIndex;
            }
            while (lodIndex < kept);
        }

        // the lod switch distances
        // The accumulator starts at (skip + 1) * 25. Where the base's +0x60 vector has an entry it
        // is copied across THE skip (source index lod + skip) - but the accumulator is advanced by
        // the entry at index lod. That asymmetry is in the instructions (0x101198ED stores from
        // [ebx+eax] with ebx = 4*(lod+skip), 0x10119900 adds [edx+ecx*4] with ecx = lod); it is
        // preserved rather than tidied.
        const int skipped = reinterpret_cast<int*>(m_geomInts.first)[geom];
        const int distanceCount = kept - 1;
        float accumulator = static_cast<float>((skipped + 1) * 25);

        for (int lod = 0; lod < distanceCount; ++lod)
        {
            float* destination = reinterpret_cast<float*>(g->lodDistances.first);

            bool haveStored = false;
            if (m_lodDistances.first &&
                static_cast<unsigned int>((m_lodDistances.last - m_lodDistances.first) >> 4) > geom)
            {
                vc71::vector_layout* inner =
                    reinterpret_cast<vc71::vector_layout*>(m_lodDistances.first) + geom;
                const unsigned int innerSize = vectorSizeBytes(*inner) / sizeof(float);
                if (static_cast<int>(innerSize) > lod)
                {
                    const float* stored = reinterpret_cast<const float*>(inner->first);
                    destination[lod] = stored[lod + skipped];
                    accumulator += stored[lod];
                    haveStored = true;
                }
            }

            if (!haveStored)
            {
                destination[lod] = accumulator;
                accumulator += 25.0f;
            }
        }
    }
}


// Builds techniques and reads materials with mip class 10, compacting kept LOD data.
// Versions below 11 compute bounds from vertices; newer versions read min/max bounds.

void StaticMeshTemplate::readMaterials(::renddx9abi::HostNonlinearStream* stream)
{
    const int savedMipClass = ::dice::hfe::dx9rend::textureManagerDefaultMipClass();
    ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(10);

    staticMeshInitTechniques();

    const unsigned int geomCount =
        m_lodCounts.first
            ? static_cast<unsigned int>(m_lodCounts.last - m_lodCounts.first) >> 2
            : 0u;

    staticMeshMaterialsResizeGeoms(m_materials, geomCount);

    unsigned int droppedVertices = 0;
    unsigned int droppedIndices  = 0;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const int lodCount = reinterpret_cast<int*>(m_lodCounts.first)[geom];
        int remainingSkips = reinterpret_cast<int*>(m_geomInts.first)[geom];

        vc71::vector_layout& geomMaterials =
            reinterpret_cast<vc71::vector_layout*>(m_materials.first)[geom];
        staticMeshMaterialsResizeLods(geomMaterials, static_cast<unsigned int>(lodCount));
        if (lodCount <= 0)
            continue;

        StaticMeshGeom* g = reinterpret_cast<StaticMeshGeom*>(m_geoms.first) + geom;

        int lodIndex = 0;
        int lodByteOffset = 0;          // 104 * destination lod
        int materialByteOffset = 0;     //  16 * destination lod

        for (;;)
        {
            StaticMeshLod* lod =
                reinterpret_cast<StaticMeshLod*>(g->lods.first + lodByteOffset);
            vc71::vector_layout& lodMaterials =
                *reinterpret_cast<vc71::vector_layout*>(geomMaterials.first + materialByteOffset);

            unsigned int materialCount = 0;
            stream->read(&materialCount, 4);                       // +0x10

            // The shipped call builds a default StaticMeshMaterial on the stack and passes it by
            // value; the tree's helper takes only the count and default-constructs internally.
            staticMeshMaterialsResize(lodMaterials, materialCount);

            m_skippingLods = static_cast<unsigned char>(remainingSkips > 0);
            lod->primitiveCount = 0;
            lod->vertexCount    = 0;
            lod->hasAlphaMaterial = 0;
            lod->byte5C = 0;
            m_dword0F8 = 3;

            unsigned int materialIndex = 0;
            while (materialCount)
            {
                StaticMeshMaterial* material =
                    reinterpret_cast<StaticMeshMaterial*>(lodMaterials.first + 516u * materialIndex);

                stream->read(&material->m_alphaMode, 4);           // +0x17C
                if (material->m_alphaMode == 1)
                    m_dword0F8 = 2;
                else if (material->m_alphaMode == 2)
                    m_hasAlphaMaterial = 1;

                unsigned int nameLength = 0;
                stream->read(&nameLength, 4);
                stream->skip(nameLength);                          // +0x18 - the fx name is skipped

                ShaderMaterial* shader =
                    staticMeshStateManagerCreateMaterial(m_path, stream, m_version, this);
                material->m_shaderMaterial = shader;

                if (!m_skippingLods)
                {
                    const unsigned int comboFlags = g_shaderComboFlags[shader->shaderCombo];
                    const unsigned int qualityMask = staticMeshQualityMask();
                    m_shaderFlagsAll |= comboFlags;
                    const unsigned int used = qualityMask & comboFlags;
                    lod->needsAltStreams =
                        static_cast<unsigned char>((used & 0xC0u) != 0 || (used & 0x100u) != 0);
                    m_shaderFlagsUsed |= used;
                }

                stream->read(&material->m_vertexStart, 4);         // +0x180
                stream->read(&material->m_indexStart, 4);          // +0x184

                unsigned int indexCount = 0;
                stream->read(&indexCount, 4);
                if (m_primitiveType == 4)
                    material->m_primitiveCount = indexCount / 3;
                else if (m_primitiveType == 5)
                    material->m_primitiveCount = indexCount - 2;
                // any other primitive type leaves +0x188 as the resize left it

                stream->read(&material->m_vertexCount, 4);         // +0x18C
                stream->read(&material->m_u4, 4);                  // +0x190
                stream->read(&material->m_u5u6, 4);                // +0x194

                if (materialIndex == 0)
                {
                    lod->primitiveType   = m_primitiveType;
                    lod->primitiveCount += material->m_primitiveCount;
                    lod->vertexCount    += material->m_vertexCount;
                    lod->vertexStart     = material->m_vertexStart;
                    lod->indexStart      = material->m_indexStart;
                }

                if (material->m_alphaMode == 1)
                    lod->hasAlphaMaterial = 1;

                if (m_version < 11u)
                {
                    // 10000 / -10000 seeds, dirty set, then every index of this material expands
                    // the box. The vertex is selected by the LOW word of (vertexStart + index).
                    material->m_bounds.min.x = 10000.0f;
                    material->m_bounds.min.y = 10000.0f;
                    material->m_bounds.min.z = 10000.0f;
                    material->m_bounds.max.x = -10000.0f;
                    material->m_bounds.max.y = -10000.0f;
                    material->m_bounds.max.z = -10000.0f;
                    material->m_bounds.dirty = 1;

                    for (unsigned int i = 0; i < indexCount; ++i)
                    {
                        const unsigned short vertex = static_cast<unsigned short>(
                            static_cast<unsigned short>(material->m_vertexStart) +
                            m_indexData[i + material->m_indexStart]);
                        const Vec3* position = reinterpret_cast<const Vec3*>(
                            static_cast<char*>(m_vertexData) + m_vertexStride * vertex);
                        meshBoundsExpand(material->m_bounds, *position);
                    }
                }
                else
                {
                    meshBoundsReadMinMax(material->m_bounds, stream);
                }

                if (remainingSkips <= 0)
                {
                    // A kept lod: slide its data down over whatever the skipped lods left behind.
                    if (droppedIndices)
                    {
                        const unsigned int oldStart = material->m_indexStart;
                        material->m_indexStart = oldStart - droppedIndices;
                        ::memmove(m_indexData + material->m_indexStart,
                                  m_indexData + oldStart,
                                  2u * indexCount);
                    }
                    if (droppedVertices)
                    {
                        const unsigned int oldStart = material->m_vertexStart;
                        const unsigned int count    = material->m_vertexCount;
                        material->m_vertexStart = oldStart - droppedVertices;
                        ::memmove(static_cast<char*>(m_vertexData) +
                                      m_vertexStride * material->m_vertexStart,
                                  static_cast<char*>(m_vertexData) + m_vertexStride * oldStart,
                                  m_vertexStride * count);
                    }
                }
                else
                {
                    // A skipped lod: drop the shader reference and count the data away.
                    if (material->m_shaderMaterial)
                        reinterpret_cast<IBase*>(material->m_shaderMaterial)->release();
                    droppedVertices += material->m_vertexCount;
                    droppedIndices  += indexCount;
                }

                if (++materialIndex >= materialCount)
                    break;
            }

            if (remainingSkips > 0)
            {
                materialByteOffset -= 16;
                --remainingSkips;
                --lodIndex;
                lodByteOffset -= static_cast<int>(sizeof(StaticMeshLod));
            }
            materialByteOffset += 16;
            lodByteOffset += static_cast<int>(sizeof(StaticMeshLod));
            if (++lodIndex >= lodCount)
                break;
        }
    }

    m_indexCount  -= droppedIndices;
    m_vertexCount -= droppedVertices;
    m_finished = 1;

    ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(savedMipClass);
}


// [FRONTIER] Serializes mesh-template overrides as console commands.
// Remains a stopping boundary until output ordering is implemented and validated.

bool StaticMeshTemplate::makeScript(::dice::hfe::io::IStream* stream)
{
    (void)stream;
    RD9_FRONTIER("dice::hfe::geom::StaticMeshTemplate::makeScript (0x10116140, the editor .con "
                 "writer; its ostream insertion order is not legibly recovered)", 0x00);
    return false;
}


// sub_10191CE0, the feature names of every set bit, concatenated in bit order.
// The shipped loop tests each of the 32 bit positions and, for a set bit,
// recomputes its index by scanning up from zero - which is that same position - before reading the
// name table.  Reproduced as written.  A bit above the twelve named ones reads past the table's
// terminator in the shipped body too; no recovered caller passes one.

void staticMeshShaderBitsToName(vc71::string& out, unsigned int bits)
{
    vc71::string names("");
    for (unsigned int i = 0; i < 32u; ++i)
    {
        const unsigned int bit = bits & (1u << i);
        if (!bit)
            continue;
        unsigned int index = 0;
        if ((bit & 1u) == 0)
        {
            do
                ++index;
            while (((bit >> index) & 1u) == 0);
        }
        names += kStaticMeshShaderBitNames[index];
    }
    out = names;
}


// Returns the shader for bits, falling back to the default mask after logging a miss.
// Returns null and logs if the default is absent.

::dice::hfe::dx9rend::Ra::RaShader* staticMeshFindShaderBits(unsigned int bits)
{
    StaticMeshTechniqueMap::const_iterator found = g_staticMeshTechniques.find(bits);
    if (found != g_staticMeshTechniques.end())
        return found->second;

    {
        vc71::string names("");
        staticMeshShaderBitsToName(names, bits);
        ::renddx9abi::logf("StaticMeshTemplate.cpp:2076 Couldn't find shader bits: %u looked up "
                           "name: %s", bits, names.c_str());
    }

    found = g_staticMeshTechniques.find(0x1000000u);
    if (found != g_staticMeshTechniques.end())
        return found->second;

    ::renddx9abi::logf("StaticMeshTemplate.cpp:2084 Couldn't find shader default");
    return 0;
}


// Shared technique-mask builder used by static and skinned mesh templates.

bool meshBuildTechniqueMasks(const vc71::string& mfxPath,
                             const vc71::string& techniquePrefix,
                             const char* const* bitNames,
                             std::map<unsigned int, ::dice::hfe::dx9rend::Ra::RaShader*>& techniqueMap,
                             vc71::vector_layout& maskVector)
{
    return staticMeshBuildTechniqueMasks(mfxPath, techniquePrefix, bitNames,
                                         techniqueMap, maskVector);
}

} // namespace geom

namespace dx9rend {
// BufferManager.cpp - declared here the way MeshTemplate.cpp declares the same pair.
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
}

namespace geom {


// Merges triangle-list index ranges once per template when each LOD fits 16-bit indices.
// Rebases onto the first material for a single shadow draw. Whole-buffer unlock still runs
// after a failed lock; index arithmetic intentionally remains 16-bit.

void StaticMeshTemplate::mergeShadowIndexRanges()
{
    if (m_byte190)
        return;

    if (m_primitiveType == 4u)
    {
        const unsigned int geomCount = m_lodCounts.first
            ? static_cast<unsigned int>(m_lodCounts.last - m_lodCounts.first) >> 2
            : 0u;
        for (unsigned int g = 0; g < geomCount; ++g)
        {
            const int lodCount = reinterpret_cast<const int*>(m_lodCounts.first)[g];
            if (!lodCount)
                continue;

            const vc71::vector_layout* const lodRecords =
                reinterpret_cast<const vc71::vector_layout*>(
                    reinterpret_cast<const vc71::vector_layout*>(m_materials.first)[g].first);
            StaticMeshLod* const lods = reinterpret_cast<StaticMeshLod*>(
                reinterpret_cast<const StaticMeshGeom*>(m_geoms.first)[g].lods.first);

            for (int l = 0; l < lodCount; ++l)
            {
                const vc71::vector_layout& record = lodRecords[l];
                StaticMeshLod& lod = lods[l];
                if (lod.byte5C)
                    continue;

                StaticMeshMaterial* const first =
                    reinterpret_cast<StaticMeshMaterial*>(record.first);
                if (!first)
                    continue;
                const unsigned int materialCount =
                    static_cast<unsigned int>(record.last - record.first) / 516u;
                if (!materialCount)
                    continue;

                bool fits = true;
                for (unsigned int m = 0; m < materialCount; ++m)
                {
                    const StaticMeshMaterial& material = first[m];
                    // `jle` - a signed compare of the 32-bit sum, as shipped.
                    if (static_cast<int>(material.m_vertexCount - first->m_vertexStart
                                         + material.m_vertexStart) > 0xFFFF)
                    {
                        fits = false;
                        break;
                    }
                }
                if (!fits)
                    continue;

                lod.mergedPrimitiveCount = 0;
                lod.mergedVertexCount    = 0;
                lod.vertexStart          = first->m_vertexStart;

                unsigned short* const indices = static_cast<unsigned short*>(
                    dx9rend::indexBufferLock(m_indexBuffer, 0, 0, 0));
                if (indices)
                {
                    for (unsigned int m = 0; m < materialCount; ++m)
                    {
                        StaticMeshMaterial& material = first[m];
                        lod.mergedPrimitiveCount += material.m_primitiveCount;
                        lod.mergedVertexCount    += material.m_vertexCount;
                        const int indexCount = 3 * static_cast<int>(material.m_primitiveCount);
                        const unsigned short delta = static_cast<unsigned short>(
                            static_cast<unsigned short>(material.m_vertexStart)
                            - static_cast<unsigned short>(lod.vertexStart));
                        for (int k = 0; k < indexCount; ++k)
                            indices[material.m_indexStart + k] =
                                static_cast<unsigned short>(indices[material.m_indexStart + k] + delta);
                        material.m_vertexStart = lod.vertexStart;
                    }
                }
                lod.byte5C = 1;
                dx9rend::indexBufferUnlock(m_indexBuffer);
            }
        }
    }

    m_byte190 = 1;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
