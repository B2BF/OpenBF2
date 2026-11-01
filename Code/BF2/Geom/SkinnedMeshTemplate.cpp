// Loads skinned-mesh geometry, LODs, materials and shared shadow parameters.


#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/EngineFile.h"    // the file manager seams and logf
#include "../../RendDX9Abi/PluginEntry.h"    // dice::hfe::fileManager
#include "../../RendDX9Abi/HostFileAbi.h"    // HostFileManager / HostStream / makeHostString
#include "../RendDX9/Ra/RaShader.h"        // the Ra vertex-element descriptors
#include "../RendDX9/Texture.h"            // textureReleaseRef
#include "../RendDX9/RendDX9Base.h"        // resourceManagerIsReady
#include "SkinnedMesh.h"                   // the 0x598 instance this factory builds
#include "SkinnedMeshTemplate.h"

#include <new>
#include <cstring>

namespace dice {
namespace hfe {

namespace dx9rend {
// sub_10017E00 on dword_1023B974 - declared per-TU, as StaticMeshStateManager.cpp and
// StaticMesh.cpp both do; the texture manager publishes no header for it.
int textureManagerSwapDefaultMipClass(int value);

// The Renderer console object owns the mapped shader-path index the texture gate reads.
extern IBase* g_Renderer;
} // namespace dx9rend

namespace geom {

namespace {

// the std::set header node. byte-identical to StaticMeshTemplate.cpp's 0x1011B6F0:
// MSVC instantiated the same allocator once per translation unit, which is why the two addresses
// differ and the two bodies do not.

// The shipped guards are lopsided and are preserved: the three link stores are each guarded on
// the allocation succeeding, the two flag bytes are NOT. A failed allocation therefore faults here
// in retail too - and the caller's own `head->isNil = 1` faults as well.
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

// Tracks live instances in a compatibility set that preserves surviving node addresses.
// Pointer keys rely on the 32-bit target; this representation is not portable to 64 bits.
void instanceSetInsert(vc71::set_layout& set, void* instance)
{
    // The shipped insert returns pair<iterator,bool> through a hidden slot; every caller in the
    // image discards it, and so does this one.
    static_cast<void>(set.insert(reinterpret_cast<unsigned int>(instance)));
}

} // namespace


// Removes the instance from the unique-key set; does nothing when absent.

void instanceSetErase(vc71::set_layout& set, void* instance)
{
    if (vc71::set_node* const node = set.find(reinterpret_cast<unsigned int>(instance)))
        set.erase(node);
}


// Creates the requested template interface. Allocation failure is not checked before
// queryInterface, preserving the original failure behavior.

IBase* __fastcall createSkinnedMeshTemplate(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(0x10C);
    SkinnedMeshTemplate* self = 0;
    if (storage)
        self = new (storage) SkinnedMeshTemplate();

    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}


// Initializes derived state after MeshTemplate and clears the keep byte last.
// Vector allocator words and padding at +0xED..+0xFB remain untouched.

SkinnedMeshTemplate::SkinnedMeshTemplate()
    : MeshTemplate()
{
    m_refCount = 1;                       // +0xC8

    m_vector0CC.first = 0;                // +0xD0 .. +0xD8; the allocator word is NOT written
    m_vector0CC.last  = 0;
    m_vector0CC.end   = 0;

    m_vector0DC.first = 0;                // +0xE0 .. +0xE8; ditto
    m_vector0DC.last  = 0;
    m_vector0DC.end   = 0;

    m_byte0EC = 0;                        // +0xEC
    m_byte0FC = 0;                        // +0xFC

    // 0x101117A0 on the set, then the header node becomes the nil sentinel and self-links three
    // ways. The comparator word at +0x100 is never written, exactly as shipped.
    m_instances.head = setConstructHead();
    m_instances.head->isNil  = 1;
    m_instances.head->parent = m_instances.head;
    m_instances.head->left   = m_instances.head;
    m_instances.head->right  = m_instances.head;
    m_instances.size = 0;

    m_keepData = 0;                       // +0x04, and the constructor's last store
}


// Reference counting and interface lookup shared with StaticMeshTemplate.

void SkinnedMeshTemplate::addRef()
{
    ++m_refCount;
}

int SkinnedMeshTemplate::getRef()
{
    return m_refCount;
}

int SkinnedMeshTemplate::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        destroy(1);                                               // through the +0xB0 slot
    return remaining;
}

// The `&& this` test is the shipped one: it cannot fail inside a member function, but
// MSVC emitted it because the returned pointer is `this + 12` and a null base must stay null.
void* SkinnedMeshTemplate::queryInterface(unsigned int iid) const
{
    if (iid != 1 && iid != 0x17701FFFu)
    {
        if (iid == 0x3709A4FFu && this)
            return const_cast<void*>(static_cast<const void*>(
                static_cast<const ::dice::hfe::io::IConsoleSaveable*>(this)));
        return 0;
    }
    return const_cast<SkinnedMeshTemplate*>(this);
}


// The five helpers readMaterials drives that this tree does not carry. Each is one named seam so the
// body above reads as the shipped sequence. [FRONTIER] [RETIRE-WHEN: skinnedmesh-instance]

namespace {

// Destroys a material-map subtree; the 20-byte POD values own no resources.
// The companion material vectors are nested by geometry, LOD and material.
// [CHOSEN, not recovered] VC7.1 tree traversal preserves keys, values and sentinel links.
// [RETIRE-WHEN: RW-74] Replace the traversal when the original allocation/erase helpers are
// implemented and their node ownership and observable tree behavior are verified.
void materialMapDestroySubtree(vc71::map_node_u32* node)
{
    while (node && !node->isNil)
    {
        materialMapDestroySubtree(node->right);
        vc71::map_node_u32* const left = node->left;
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

void materialMapClear(vc71::map_layout& map)                               // sub_10049900, fast arm
{
    if (!map.head)
        return;
    vc71::map_node_u32* const head = reinterpret_cast<vc71::map_node_u32*>(map.head);
    materialMapDestroySubtree(head->parent);
    head->parent = head;
    head->left   = head;
    head->right  = head;
    map.size     = 0;
}

vc71::map_node_u32* materialMapCopySubtree(vc71::map_node_u32* head,
                                           const vc71::map_node_u32* source,
                                           vc71::map_node_u32* parent)
{
    if (!source || source->isNil)
        return head;                       // a nil child points at the sentinel, as shipped

    vc71::map_node_u32* const node = static_cast<vc71::map_node_u32*>(
        ::renddx9abi::engineAlloc(sizeof(vc71::map_node_u32)));
    if (!node)
        return head;

    std::memcpy(node, source, sizeof(vc71::map_node_u32));   // key, value and colour
    node->parent = parent;
    node->left   = materialMapCopySubtree(head, source->left,  node);
    node->right  = materialMapCopySubtree(head, source->right, node);
    return node;
}

void materialMapCopy(vc71::map_layout& destination, const vc71::map_layout& source)
{
    if (!destination.head || !source.head)
        return;
    vc71::map_node_u32* const head =
        reinterpret_cast<vc71::map_node_u32*>(destination.head);
    const vc71::map_node_u32* const sourceHead =
        reinterpret_cast<const vc71::map_node_u32*>(source.head);

    head->parent     = materialMapCopySubtree(head, sourceHead->parent, head);
    destination.size = source.size;

    // The leftmost/rightmost links are RE-derived by walking the new tree, not copied.
    if (head->parent->isNil)
    {
        head->left  = head;
        head->right = head;
        return;
    }
    vc71::map_node_u32* node = head->parent;
    while (!node->left->isNil)
        node = node->left;
    head->left = node;

    node = head->parent;
    while (!node->right->isNil)
        node = node->right;
    head->right = node;
}

// g_Renderer + 0x7E4 - the renderer-level lod-skip setting readLods clamps against.
// StaticMeshTemplate.cpp carries the same read as `rendererLodSkipBase`, file-local.
int skinnedMeshLodSkipBase()
{
    return *reinterpret_cast<const int*>(
        reinterpret_cast<const unsigned char*>(dx9rend::g_Renderer) + 2020);
}

// Forward declaration: vectorResizeRaw is defined further down this file.
void vectorResizeRaw(vc71::vector_layout& v, unsigned int count, unsigned int elemSize,
                     const void* prototype);

// sub_10112CF0 - resize a vector of 68-byte bone-matrix records. \u2605 THE prototype IS AN identity
// matrix AT offset +0x04, not +0x00: the shipped builder stores 1.0f to +0x04, +0x18, +0x2C and
// +0x40 of a zeroed 68-byte block, which is a Mat4 diagonal shifted up by one dword. So each record
// is {dword, Mat4}.
void skinnedMeshMatrixVectorResize(vc71::vector_layout& v, unsigned int count)
{
    unsigned char prototype[68];
    std::memset(prototype, 0, sizeof(prototype));
    *reinterpret_cast<float*>(prototype + 0x04) = 1.0f;
    *reinterpret_cast<float*>(prototype + 0x18) = 1.0f;
    *reinterpret_cast<float*>(prototype + 0x2C) = 1.0f;
    *reinterpret_cast<float*>(prototype + 0x40) = 1.0f;
    vectorResizeRaw(v, count, 68, prototype);
}

enum { kSkinnedMeshMaterialStride = 456 };   // the record skinnedMeshMaterialConstruct builds


// The shrink/grow body all three share, and the one `skinnedMeshReserveLightRecords` already uses.
// [CHOSEN, not recovered] in its allocation detail, under the standing [RETIRE-WHEN: RW-74] policy.
void vectorResizeRaw(vc71::vector_layout& v, unsigned int count, unsigned int elemSize,
                     const void* prototype)
{
    const unsigned int size =
        v.first ? static_cast<unsigned int>((v.last - v.first) / elemSize) : 0u;
    if (count <= size)
    {
        v.last = v.first + elemSize * count;       // shrink: republish _Last
        return;
    }

    const unsigned int capacity =
        v.first ? static_cast<unsigned int>((v.end - v.first) / elemSize) : 0u;
    if (count > capacity)
    {
        char* const grown = static_cast<char*>(::renddx9abi::engineAlloc(elemSize * count));
        if (!grown)
            return;
        if (v.first)
        {
            std::memcpy(grown, v.first, elemSize * size);
            ::renddx9abi::engineFree(v.first);
        }
        v.first = grown;
        v.end   = grown + elemSize * count;
    }
    v.last = v.first + elemSize * count;

    for (unsigned int i = size; i < count; ++i)
        std::memcpy(v.first + elemSize * i, prototype, elemSize);
}

// sub_10111DF0 / sub_100EC310 - `vector<T>::operator=` over 4-byte elements: an empty source frees
// and clears, otherwise the destination is grown if needed and the elements copied.
// SkinnedMesh.cpp carries this same body in ITS anonymous namespace, so it cannot be shared;
// this is that transcription repeated, not a second reading. [RETIRE-WHEN: RW-74]
void vectorAssignRaw(vc71::vector_layout& dst, const vc71::vector_layout& src, unsigned elemSize)
{
    if (&dst == &src)
        return;

    const unsigned int srcCount =
        src.first ? static_cast<unsigned int>((src.last - src.first) / elemSize) : 0u;
    if (!srcCount)
    {
        if (dst.first)
            ::renddx9abi::engineFree(dst.first);
        dst.first = 0;
        dst.last  = 0;
        dst.end   = 0;
        return;
    }

    const unsigned int capacity =
        dst.first ? static_cast<unsigned int>((dst.end - dst.first) / elemSize) : 0u;
    if (srcCount > capacity)
    {
        if (dst.first)
            ::renddx9abi::engineFree(dst.first);
        char* const grown = static_cast<char*>(::renddx9abi::engineAlloc(elemSize * srcCount));
        dst.first = grown;
        dst.end   = grown ? grown + elemSize * srcCount : 0;
        if (!grown)
        {
            dst.last = 0;
            return;
        }
    }
    std::memcpy(dst.first, src.first, elemSize * srcCount);
    dst.last = dst.first + elemSize * srcCount;
}

// Resizes a texture-pointer vector, filling new elements with null.
void textureVectorResize(vc71::vector_layout& v, unsigned int count)
{
    static void* const kNullTexture = 0;
    vectorResizeRaw(v, count, sizeof(void*), &kNullTexture);
}


// Copies a material into its live record. Map assignment guards self-assignment and
// clears the destination before copying.

void skinnedMeshMaterialCommit(void* record, const void* scratch)
{
    unsigned char* const d = static_cast<unsigned char*>(record);
    const unsigned char* const a = static_cast<const unsigned char*>(scratch);

    // +0x04 - the texture vector. 4-byte elements.
    vectorAssignRaw(*reinterpret_cast<vc71::vector_layout*>(d + 0x04),
                    *reinterpret_cast<const vc71::vector_layout*>(a + 0x04), 4);

    // +0x14 .. +0x24 - five dwords, four of which the draws read back as baseVertex, startIndex,
    // primitive count and vertex count.
    std::memcpy(d + 0x14, a + 0x14, 0x28 - 0x14);

    // +0x28 - the vertex-element vector. Also 4-byte elements (pointers to RaVertexElementDesc).
    vectorAssignRaw(*reinterpret_cast<vc71::vector_layout*>(d + 0x28),
                    *reinterpret_cast<const vc71::vector_layout*>(a + 0x28), 4);

    ::dice::hfe::dx9rend::Ra::raMaterialParametersAssign(d + 0x38, a + 0x38);

    std::memcpy(d + 0x188, a + 0x188, 5);                                  // the five flag bytes
    std::memcpy(d + 0x190, a + 0x190, 0x1A0 - 0x190);

    *reinterpret_cast<vc71::string*>(d + 0x1A0) =
        *reinterpret_cast<const vc71::string*>(a + 0x1A0);

    // The self-assignment guard compares the two map addresses, not the objects.
    if (d + 0x1BC != a + 0x1BC)
    {
        materialMapClear(*reinterpret_cast<vc71::map_layout*>(d + 0x1BC));
        materialMapCopy(*reinterpret_cast<vc71::map_layout*>(d + 0x1BC),
                        *reinterpret_cast<const vc71::map_layout*>(a + 0x1BC));
    }
}

void skinnedMeshMaterialDestroy(void* scratch)
{
    unsigned char* const m = static_cast<unsigned char*>(scratch);

    vc71::map_layout& map = *reinterpret_cast<vc71::map_layout*>(m + 0x1BC);
    materialMapClear(map);
    if (map.head)
        ::renddx9abi::engineFree(map.head);
    map.head = 0;
    map.size = 0;

    reinterpret_cast<vc71::string*>(m + 0x1A0)->~string();

    ::dice::hfe::dx9rend::Ra::raMaterialParametersDestroy(m + 0x38);

    vc71::vector_layout& elements = *reinterpret_cast<vc71::vector_layout*>(m + 0x28);
    if (elements.first)
        ::renddx9abi::engineFree(elements.first);
    elements.first = 0;
    elements.last  = 0;
    elements.end   = 0;

    vc71::vector_layout& textures = *reinterpret_cast<vc71::vector_layout*>(m + 0x04);
    if (textures.first)
        ::renddx9abi::engineFree(textures.first);
    textures.first = 0;
    textures.last  = 0;
    textures.end   = 0;

    // [FRONTIER] The shared material base table is unavailable. Leave the destroyed
    // record's vptr null; records must be reconstructed before any further dispatch.
    *reinterpret_cast<void**>(m) = 0;
}

// Empty nested-vector value.
const vc71::vector_layout kEmptyNestedVector = { 0, 0, 0, 0 };

// StaticMeshMaterial.cpp already carries this body, but in ITS anonymous namespace, so it has
// internal linkage and cannot be shared. This is that transcription repeated, not a second reading.
// The growth policy is the shipped one: capacity + capacity/2, floored at size + 1.
void pointerVectorPush(vc71::vector_layout& v, const void* value)
{
    const unsigned int size =
        v.first ? static_cast<unsigned int>((v.last - v.first) >> 2) : 0u;
    const unsigned int capacity =
        v.first ? static_cast<unsigned int>((v.end - v.first) >> 2) : 0u;

    if (size == capacity)
    {
        unsigned int grown = capacity + (capacity >> 1);
        if (grown < size + 1u)
            grown = size + 1u;
        char* const buffer = static_cast<char*>(::renddx9abi::engineAlloc(grown * 4u));
        if (!buffer)
            return;
        if (size)
            std::memcpy(buffer, v.first, size * 4u);
        if (v.first)
            ::renddx9abi::engineFree(v.first);
        v.first = buffer;
        v.last  = buffer + size * 4u;
        v.end   = buffer + grown * 4u;
    }

    *reinterpret_cast<const void**>(v.last) = value;
    v.last += 4;
}

void skinnedMeshReserveGeoms(vc71::vector_layout& companions, unsigned int count)
{
    vectorResizeRaw(companions, count, sizeof(vc71::vector_layout), &kEmptyNestedVector);
}

void skinnedMeshReserveLods(vc71::vector_layout* geomCompanion, int count)
{
    if (!geomCompanion || count < 0)
        return;
    vectorResizeRaw(*geomCompanion, static_cast<unsigned int>(count),
                    sizeof(vc71::vector_layout), &kEmptyNestedVector);
}

// Expands materials with shallow byte copies of the prototype.
// Each record must be overwritten by material parsing before its shared storage is used.
void skinnedMeshReserveMaterials(vc71::vector_layout* lodCompanion, unsigned int count,
                                 const void* prototype)
{
    if (!lodCompanion || !prototype)
        return;
    vectorResizeRaw(*lodCompanion, count, kSkinnedMeshMaterialStride, prototype);
}

// Destroys a material and frees its engine allocation when flags bit 0 is set.
// Returns the original pointer. The material keeps the nine-slot interface used by render queues.
void* skinnedMeshMaterialDestroyFlags(void* material, unsigned char flags)
{
    skinnedMeshMaterialDestroy(material);
    if (flags & 1)
        ::renddx9abi::engineFree(material);
    return material;
}

// Binds vertex streams and a declaration cached by shader build serial.
// Missing elements report failure on every call and are not cached.
bool skinnedMeshMaterialApplyStreams(void* material, void* shaderState)
{
    namespace Ra = ::dice::hfe::dx9rend::Ra;
    unsigned char* const m = static_cast<unsigned char*>(material);
    Ra::RaShader* const shader = static_cast<Ra::RaShader*>(shaderState);
    vc71::map_layout& declarations = *reinterpret_cast<vc71::map_layout*>(m + 0x1BC);
    const unsigned int key = static_cast<unsigned int>(shader->m_buildSerial);  // +0x174

    vc71::map_node_u32* node = vc71::mapFindU32(declarations, key);
    if (!node)
    {
        Ra::RaStreamRequirements* const required =
            Ra::raStreamRequirementsCreate(shader->m_requiredVertexElements);   // sub_10049330 + sub_1004A0F0
        Ra::RaDeclarationBuilder builder;
        Ra::raDeclarationBuilderConstruct(builder);

        const vc71::vector_layout& elements =
            *reinterpret_cast<const vc71::vector_layout*>(m + 0x28);
        if (Ra::raStreamRequirementsConsume(required, elements))
            Ra::raDeclarationBuilderAddStream(builder, elements);

        if (Ra::raStreamRequirementsStarving(required))
        {
            char missing[1024];
            Ra::raStreamRequirementsDescribe(required, missing, sizeof(missing));
            ::renddx9abi::logf("SkinnedMeshTemplate.cpp:126 Starving streams in the shader %s, %s",
                               shader->m_name.c_str(), missing);
            Ra::raDeclarationBuilderDestroy(builder);
            Ra::raStreamRequirementsDestroy(required);
            return false;
        }

        IDirect3DDevice9* const device = Ra::raRendererDevice();                // renderer +0x0C
        vc71::map_node_u32* const inserted = vc71::mapInsertU32(declarations, key);
        *reinterpret_cast<IDirect3DVertexDeclaration9**>(inserted->value) =
            Ra::raVertexDeclarationCached(Ra::raDeclarationBuilderFinish(builder),
                                          device);
        node = vc71::mapFindU32(declarations, key);
        Ra::raDeclarationBuilderDestroy(builder);
        Ra::raStreamRequirementsDestroy(required);
    }

    // The bind, in the shipped order: stream 0 from +0x194 at the +0x198 stride, the declaration,
    // then the index buffer at +0x19C.
    ::dice::hfe::dx9rend::stateManagerSetStreamSource(
        0, *reinterpret_cast<IDirect3DVertexBuffer9**>(m + 0x194), 0,
        *reinterpret_cast<unsigned int*>(m + 0x198));                           // +0x58
    ::dice::hfe::dx9rend::stateManagerSetVertexDeclaration(
        *reinterpret_cast<IDirect3DVertexDeclaration9**>(node->value));         // +0x5C
    ::dice::hfe::dx9rend::stateManagerSetIndices(
        *reinterpret_cast<IDirect3DIndexBuffer9**>(m + 0x19C));                 // +0x54
    return true;
}

// The five accessors and the two constants, each one instruction of body.
inline int materialDword(const void* material, unsigned int offset)
{
    return *reinterpret_cast<const int*>(static_cast<const unsigned char*>(material) + offset);
}

// The nine __fastcall entries the table holds. The shipped records are __thiscall, which is this
// receiver-in-ecx shape with the unused edx slot - the same idiom RenderQueues.cpp's own vtables
// use, and the same one `materialApplyStreams` calls back through.
void* __fastcall smm_destroy(void* self, void*, unsigned char flags)
{ return skinnedMeshMaterialDestroyFlags(self, flags); }

unsigned char __fastcall smm_applyStreams(void* self, void*, void* shaderState)
{ return skinnedMeshMaterialApplyStreams(self, shaderState) ? 1u : 0u; }

int __fastcall smm_getVertexCount(void* self, void*)    { return materialDword(self, 0x24); }
int __fastcall smm_slot0C(void*, void*)                 { return -1; }
int __fastcall smm_getPrimitiveCount(void* self, void*) { return materialDword(self, 0x20); }
int __fastcall smm_getDword190(void* self, void*)       { return materialDword(self, 0x190); }
int __fastcall smm_getVertexStart(void* self, void*)    { return materialDword(self, 0x18); }
int __fastcall smm_slot1C(void*, void*)                 { return 0; }
int __fastcall smm_getIndexStart(void* self, void*)     { return materialDword(self, 0x1C); }

void* const kSkinnedMeshMaterialVtable[9] =
{
    reinterpret_cast<void*>(&smm_destroy),             // +0x00
    reinterpret_cast<void*>(&smm_applyStreams),        // +0x04
    reinterpret_cast<void*>(&smm_getVertexCount),      // +0x08
    reinterpret_cast<void*>(&smm_slot0C),              // +0x0C
    reinterpret_cast<void*>(&smm_getPrimitiveCount),   // +0x10
    reinterpret_cast<void*>(&smm_getDword190),         // +0x14
    reinterpret_cast<void*>(&smm_getVertexStart),      // +0x18
    reinterpret_cast<void*>(&smm_slot1C),              // +0x1C
    reinterpret_cast<void*>(&smm_getIndexStart)        // +0x20
};


// Constructs a material in storage large enough for the 456-byte record.
// Installs the nine-slot render interface and skinning vertex elements; hasTangent controls
// tangent-space support. Missing support reports the existing diagnostic.

void skinnedMeshMaterialConstruct(void* storage, bool hasTangent)
{
    unsigned char* const m = static_cast<unsigned char*>(storage);

    *reinterpret_cast<void* const**>(m) = kSkinnedMeshMaterialVtable;
    std::memset(m + 0x08, 0, 0x28 - 0x08);             // +0x08 .. +0x24

    // +0x28: the vertex-element vector. Allocator word untouched; first/last/end zeroed.
    vc71::vector_layout& elements = *reinterpret_cast<vc71::vector_layout*>(m + 0x28);
    elements.first = 0;
    elements.last  = 0;
    elements.end   = 0;

    ::dice::hfe::dx9rend::Ra::raMaterialParametersConstruct(m + 0x38);

    m[0x188] = 0;
    m[0x189] = 0;
    m[0x18A] = 0;
    m[0x18B] = 0;
    m[0x18C] = 0;

    *reinterpret_cast<unsigned int*>(m + 0x190) = 4;   // four, not zero
    *reinterpret_cast<unsigned int*>(m + 0x194) = 0;
    *reinterpret_cast<unsigned int*>(m + 0x198) = 0;
    *reinterpret_cast<unsigned int*>(m + 0x19C) = 0;

    new (m + 0x1A0) vc71::string();                    // +0x1A0
    reinterpret_cast<vc71::map_layout*>(m + 0x1BC)->construct(sizeof(vc71::map_node_u32));

    pointerVectorPush(elements, &::dice::hfe::dx9rend::Ra::g_desc_1023EF04);   // Vtx_Position
    pointerVectorPush(elements, &::dice::hfe::dx9rend::Ra::g_desc_1023EF7C);   // Vtx_Normal
    pointerVectorPush(elements, &::dice::hfe::dx9rend::Ra::g_desc_1023ED74);   // Vtx_BoneWeights
    pointerVectorPush(elements, &::dice::hfe::dx9rend::Ra::g_desc_1023EEDC);   // Vtx_BoneIdcs
    pointerVectorPush(elements, &::dice::hfe::dx9rend::Ra::g_desc_1023ED4C);   // Vtx_TBase

    if (hasTangent)
    {
        pointerVectorPush(elements, &::dice::hfe::dx9rend::Ra::g_desc_1023EFF4);
    }
    else
    {
        ::renddx9abi::logf("SkinnedMeshTemplate.cpp:101: tl: Material created _without_ tangent "
                           "support. Something must be wrong! (because we haven't implemented "
                           "this yet!!)");
    }
}


} // anonymous namespace

// Reads materials for each geometry/LOD, resolving textures and shader flags.
// The stream and LOD records must already be initialized. Uses mip class 12 for these loads
// and restores the previous class on normal return. Shared shadow state initializes first.
void SkinnedMeshTemplate::readMaterials(::renddx9abi::HostNonlinearStream* stream)
{

    // Keep the skinned-mesh mip class separate from static meshes (class 13).
    const int savedMipClass = ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(12);

    skinnedMeshInitTechniques();                       // sub_1010F190 - fills the mask vector

    const unsigned int geomCount =
        m_lodCounts.first
            ? static_cast<unsigned int>((m_lodCounts.last - m_lodCounts.first) >> 2)
            : 0u;

    skinnedMeshReserveGeoms(m_vector0CC, geomCount);   // sub_10115380 - one entry per geom

    // Initialize shared shadow parameters before reading material records.
    skinnedMeshInitShadowState();

    unsigned int skippedVertices = 0;                  // v93
    unsigned int skippedIndices  = 0;                  // v90

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const int lodCount =
            reinterpret_cast<const int*>(m_lodCounts.first)[geom];
        // readLods records the number of LODs to skip for each geometry.
        int lodsToSkip = reinterpret_cast<const int*>(m_geomInts.first)[geom];

        // The geom's own companion entry - the receiver measured at 0x1010FF49.
        vc71::vector_layout* const geomCompanion =
            reinterpret_cast<vc71::vector_layout*>(m_vector0CC.first + 16 * geom);
        skinnedMeshReserveLods(geomCompanion, lodCount);   // sub_10114D00 - one entry per lod

        int lodByteOffset       = 0;                   // 132 * destination lod
        int companionByteOffset = 0;                   // 16  * destination lod

        for (int lod = 0; lod < lodCount; )
        {
            // The 32-byte geom record's first vector holds 132-byte lod records; its companion in
            // the other vector holds 16-byte ones. Both are indexed by the destination lod, which
            // is what the skip dance below keeps still.
            unsigned char* const lodRecord =
                reinterpret_cast<unsigned char*>(
                    reinterpret_cast<const vc71::vector_layout*>(
                        m_vector0DC.first + 32 * geom)->first) + lodByteOffset;
            vc71::vector_layout* const companion =
                reinterpret_cast<vc71::vector_layout*>(
                    reinterpret_cast<unsigned char*>(geomCompanion->first) + companionByteOffset);

            // Reset per-LOD humanskin and alpha-test flags before scanning materials.
            lodRecord[0x80] = 0;
            lodRecord[0x81] = 0;

            unsigned int materialCount = 0;
            stream->read(&materialCount, 4);
            // The prototype the resize copies, built on this frame exactly as the shipped
            // body builds it at `mov ecx, esp` immediately before the call.
            unsigned char materialPrototype[kSkinnedMeshMaterialStride];
            skinnedMeshMaterialConstruct(materialPrototype, true);
            skinnedMeshReserveMaterials(companion, materialCount,
                                        materialPrototype);

            for (unsigned int material = 0; material < materialCount; ++material)
            {
                // Build textures in the temporary material before committing it.
                unsigned char scratchMaterial[kSkinnedMeshMaterialStride];
                skinnedMeshMaterialConstruct(scratchMaterial, true);
                vc71::vector_layout& textures =
                    *reinterpret_cast<vc71::vector_layout*>(scratchMaterial + 0x04);

                unsigned int skipBytes = 0;
                stream->read(&skipBytes, 4);
                stream->skip(skipBytes);

                // The shader name, lowercased in place. The length is read, the buffer is
                // allocated length + 1 and NUL-terminated by hand - the stream stores no terminator.
                unsigned int nameLength = 0;
                stream->read(&nameLength, 4);
                char* const shaderName =
                    static_cast<char*>(::renddx9abi::engineAlloc(nameLength + 1));
                stream->read(shaderName, nameLength);
                shaderName[nameLength] = 0;
                ::strlwr(shaderName);

                // Match substrings in the lowercase shader name; draw flags belong to the LOD.
                if (std::strstr(shaderName, "tangent"))
                    m_byte0FC = 1;                     // +0xFC, the template-wide tangent flag
                if (std::strstr(shaderName, "humanskin"))
                    lodRecord[0x80] = 1;

                // Gather textures in the temporary material's vector.
                unsigned int textureCount = 0;
                stream->read(&textureCount, 4);

                textureVectorResize(textures, textureCount);

                bool hasRealSecondTexture = false;                  // the shipped `v57`
                for (unsigned int t = 0; t < textureCount; ++t)
                {
                    unsigned int textureNameLength = 0;
                    stream->read(&textureNameLength, 4);
                    if (!textureNameLength)
                        continue;                                   // the slot is left as-is

                    char* const rawName =
                        static_cast<char*>(::renddx9abi::engineAlloc(textureNameLength + 1));
                    stream->read(rawName, textureNameLength);
                    rawName[textureNameLength] = 0;

                    vc71::string prefix("");
                    vc71::string name(rawName);

                    // A name with no '/' is relative: it gets the template's path plus
                    // *unk_1025B53C, which is the literal "Textures/" (static init 0x101D31C0).
                    if (!std::strchr(rawName, '/'))
                    {
                        prefix = m_path;
                        prefix += "Textures/";
                    }

                    // Rewrite the obsolete name so the redirect applies in this same pass.
                    if (std::strstr(rawName, "default_sdgbmfbf_color_lut"))
                    {
                        ::renddx9abi::logf("SkinnedMeshTemplate.cpp:546: Mesh %s has specular lut "
                                           "default_sdgbmfbf_color_lut which is obsolete, please "
                                           "re-export!", m_name.c_str());
                        name = "_SpecularLUT_pow36_";
                    }
                    if (std::strstr(name.c_str(), "SpecularLUT_pow36"))
                        name = "@@ABSOLUTE@@Common/Textures/SpecularLUT_pow36.dds";

                    // Skip bump maps at a nonzero shader path, and all textures for omitted LODs.
                    const unsigned int shaderPath = static_cast<unsigned int>(
                        ::dice::hfe::dx9rend::rendererGetShaderPath(
                            ::dice::hfe::dx9rend::g_Renderer));
                    const bool isBumpName = std::strstr(name.c_str(), "_b.dds") != 0
                                         || std::strstr(name.c_str(), "_b_os.dds") != 0;
                    if (lodsToSkip <= 0 && (!shaderPath || !isBumpName))
                    {
                        vc71::string full(prefix);
                        full += name;
                        void* const texture =
                            ::dice::hfe::dx9rend::textureManagerGetTexture(full);
                        if (textures.first)
                            reinterpret_cast<void**>(textures.first)[t] = texture;

                        // Slot 1 counts as a real texture only when it is not a SpecularLUT.
                        if (t == 1 && !std::strstr(name.c_str(), "SpecularLUT"))
                            hasRealSecondTexture = true;
                    }

                    ::renddx9abi::engineFree(rawName);
                }
                (void)hasRealSecondTexture;

                // Read draw-range values directly into the material record.
                stream->read(scratchMaterial + 0x18, 4);          // baseVertex
                stream->read(scratchMaterial + 0x1C, 4);          // startIndex

                unsigned int indexCount = 0;
                stream->read(&indexCount, 4);

                // Derive triangle-list/strip counts; other primitive types keep the initialized zero.
                if (m_primitiveType == 4)                         // D3DPT_TRIANGLELIST
                    *reinterpret_cast<unsigned int*>(scratchMaterial + 0x20) = indexCount / 3u;
                else if (m_primitiveType == 5)                    // D3DPT_TRIANGLESTRIP
                    *reinterpret_cast<unsigned int*>(scratchMaterial + 0x20) = indexCount - 2u;

                stream->read(scratchMaterial + 0x24, 4);          // vertex count
                stream->skip(4);
                stream->skip(4);

                // The destination, measured at the record inside the companion.
                unsigned char* const record =
                    reinterpret_cast<unsigned char*>(companion->first)
                    + kSkinnedMeshMaterialStride * material;

                skinnedMeshMaterialCommit(record, scratchMaterial);

                if (lodsToSkip <= 0)
                {
                    // keep: slide this material's data down over whatever earlier lods skipped,
                    // and rebase its starts by the same amounts. Both arms are guarded on the
                    // running total being non-zero, so the first kept lod moves nothing.
                    if (skippedIndices)
                    {
                        unsigned int& startIndex =
                            *reinterpret_cast<unsigned int*>(record + 0x1C);
                        const unsigned int previous = startIndex;
                        startIndex = previous - skippedIndices;
                        std::memmove(m_indexData + startIndex, m_indexData + previous,
                                     2u * indexCount);
                    }
                    if (skippedVertices)
                    {
                        unsigned int& baseVertex =
                            *reinterpret_cast<unsigned int*>(record + 0x18);
                        const unsigned int previous = baseVertex;
                        baseVertex = previous - skippedVertices;
                        const unsigned int vertexCount =
                            *reinterpret_cast<const unsigned int*>(record + 0x24);
                        std::memmove(
                            static_cast<char*>(m_vertexData) + baseVertex * m_vertexStride,
                            static_cast<char*>(m_vertexData) + previous * m_vertexStride,
                            m_vertexStride * vertexCount);
                    }
                }
                else
                {
                    // Release skipped textures and accumulate counts for rebasing retained materials.
                    // Reload the vector's first pointer on each iteration.
                    for (unsigned int j = 0;; ++j)
                    {
                        void** const slots = reinterpret_cast<void**>(textures.first);
                        if (!slots)
                            break;
                        const unsigned int held =
                            static_cast<unsigned int>((textures.last - textures.first) >> 2);
                        if (j >= held)
                            break;
                        if (slots[j])
                        {
                            // The release-if-present pattern this tree already carries:
                            // `if (!--ref) { dtor; engineFree; }`, refcount at +0x04.

                            // Release a copy so the slot remains unchanged for material destruction.
                            ::dice::hfe::dx9rend::EngineTexture* doomed =
                                static_cast< ::dice::hfe::dx9rend::EngineTexture*>(slots[j]);
                            ::dice::hfe::dx9rend::textureReleaseRef(doomed);
                        }
                    }
                    skippedVertices += *reinterpret_cast<const unsigned int*>(record + 0x24);
                    skippedIndices  += indexCount;
                }

                skinnedMeshMaterialDestroy(scratchMaterial);

                ::renddx9abi::engineFree(shaderName);
            }

            // Reuse the destination slot after consuming a skipped LOD from the stream.
            if (lodsToSkip > 0)
            {
                lodByteOffset       -= 132;
                companionByteOffset -= 16;
                --lodsToSkip;
                --lod;
            }
            lodByteOffset       += 132;
            companionByteOffset += 16;
            if (++lod >= lodCount)
                break;
        }
    }

    // The totals are reduced by what was skipped, not recomputed.
    m_indexCount  -= skippedIndices;                   // +0xB8
    m_vertexCount -= skippedVertices;                  // +0xB4

    ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(savedMipClass);
}


// Registers the new instance before loading so load callbacks can update it.
// Failure unregisters and destroys it through its engine-heap deleting slot.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: skinnedmesh-template]
// Retire remaining template boundaries after their declared behavior is implemented and validated.

void* SkinnedMeshTemplate::createInstance()
{
    void* const storage = ::renddx9abi::engineAlloc(0x598);
    SkinnedMesh* const instance =
        storage ? skinnedMeshConstruct(storage, this) : 0;

    if (m_loaded)                                                   // +0x4C
    {
        // The constructor already selects the loaded template's geom/lod (PE 0x101A78B1).
        // Only instances created before loading need insertion and load()'s refresh walk.
        m_keepData = 1;
        return instance;
    }

    instanceSetInsert(m_instances, instance);
    if (m_loaded
        || ::dice::hfe::dx9rend::resourceManagerIsReady()           // sub_1000D950(ds:1023B988)
        || load())                                                  // vslot +0x2C
    {
        m_keepData = 1;
        return instance;
    }

    instanceSetErase(m_instances, instance);
    if (instance)
    {
        typedef void*(__fastcall * DtorFn)(void*, void*, unsigned int);
        reinterpret_cast<DtorFn>(instance->m_vptr[0x34 / sizeof(void*)])(instance, 0, 1);
    }
    return 0;
}


// Loads the named .skinnedMesh and updates live instances before marking it loaded.
// Returns false when already loaded; parser failure is not propagated.

bool SkinnedMeshTemplate::load()
{
    if (m_loaded)                                                     // +0x4C
        return false;

    static const char* const kMeshesFolder = "Meshes/";               // unk_10263DD4

    vc71::string path(m_path.c_str());
    path += kMeshesFolder;
    path += m_name;
    path += ".skinnedMesh";

    // Built, never read. See the header note.
    vc71::string gpuPath(m_path.c_str());
    gpuPath += kMeshesFolder;
    gpuPath += "gpu_";
    gpuPath += m_name;
    gpuPath += ".skinnedMesh";

    ::renddx9abi::HostFileManager* const files = ::dice::hfe::fileManager;

    if (!files->fileExists(::renddx9abi::makeHostString(path)))        // +0x1C
    {
        // SkinnedMeshTemplate.cpp:226, Debug type 6.
        ::renddx9abi::logf("SkinnedMeshTemplate.cpp:226: Cannot load SkinnedMesh %s "
                           "Please make sure it exists!", path.c_str());
        return false;
    }

    ::renddx9abi::HostStream* const stream = files->open(
        ::renddx9abi::makeHostString(path),
        static_cast< ::renddx9abi::HostFileMode>(2),
        static_cast< ::renddx9abi::HostFileAccess>(0), 0);            // +0x18
    if (stream)
    {
        // The answer is discarded - see the header note.
        static_cast<void>(loadFromStream(
            static_cast< ::renddx9abi::HostNonlinearStream*>(stream)));   // +0xB4
        stream->release();                                            // +0x08
    }

    // Re-point every live instance at the data that just arrived. The set is empty until
    // createInstance has run.
    for (vc71::set_node* node = m_instances.head ? m_instances.head->left : 0;
         node && node != m_instances.head;
         node = vc71::set_layout::next(node))
    {
        if (SkinnedMesh* const instance = reinterpret_cast<SkinnedMesh*>(node->key))
        {
            instance->refreshSelection();
        }
    }

    m_loaded = 1;                                                     // after the walk
    return true;
}

// Skinned meshes have no trailer dword. Leave the stream positioned at the first LOD;
// consuming the base class trailer would misalign bounds and every following count.
void SkinnedMeshTemplate::readTrailer(::renddx9abi::HostNonlinearStream* stream)
{
    (void)stream;   // the shared empty body. Deliberately reads nothing.
}

void SkinnedMeshTemplate::readLods(::renddx9abi::HostNonlinearStream* stream)
{
    const unsigned int geomCount =
        m_lodCounts.first
            ? static_cast<unsigned int>(m_lodCounts.last - m_lodCounts.first) >> 2
            : 0u;

    // sub_10115700 - resize the 32-byte geom-record vector. Its prototype is a default record:
    // two empty vectors and the pad.
    static const unsigned char kEmptyGeomRecord[32] = { 0 };
    vectorResizeRaw(m_vector0DC, geomCount, 32, kEmptyGeomRecord);
    if (!geomCount)
        return;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        // One geometry allows geom 0 to skip; two allow geom 1; larger counts skip none.
        // Both cases use m_maxSkip3pLods.
        const int rendererSkip = skinnedMeshLodSkipBase();          // g_Renderer + 0x7E4
        int skip = 0;
        if (geomCount == 1)      { if (geom == 0) skip = m_maxSkip3pLods; }
        else if (geomCount == 2) { if (geom == 1) skip = m_maxSkip3pLods; }

        const int lodCount = reinterpret_cast<int*>(m_lodCounts.first)[geom];
        if (skip >= rendererSkip)
            skip = rendererSkip;
        if (lodCount - 1 < skip)
            skip = lodCount - 1;

        reinterpret_cast<int*>(m_geomInts.first)[geom] = skip;       // +0x70 remembers it
        const int kept = lodCount - skip;
        reinterpret_cast<int*>(m_lodCounts.first)[geom] = kept;

        unsigned char* const geomRecord =
            reinterpret_cast<unsigned char*>(m_vector0DC.first) + 32 * geom;
        vc71::vector_layout& lods =
            *reinterpret_cast<vc71::vector_layout*>(geomRecord);            // +0x00, 132-byte lods
        vc71::vector_layout& distances =
            *reinterpret_cast<vc71::vector_layout*>(geomRecord + 0x10);     // +0x10, floats

        static const unsigned char kEmptyLodRecord[132] = { 0 };
        vectorResizeRaw(lods, static_cast<unsigned int>(kept), 132, kEmptyLodRecord);
        const float kZeroDistance = 0.0f;
        vectorResizeRaw(distances, static_cast<unsigned int>(kept - 1), 4,
                        &kZeroDistance);

        // read each kept lod
        // The skip is implemented by rewinding the destination, so a skipped lod is read and
        // then overwritten by the next one. Same net-zero dance as readMaterials'.
        int remainingSkips = skip;
        int lodIndex       = 0;
        int byteOffset     = 0;
        if (kept > 0)
        {
            do
            {
                unsigned char* const lod =
                    reinterpret_cast<unsigned char*>(lods.first) + byteOffset;

                // The same version gate the StaticMesh sibling documents.
                if (m_head == 0 && m_version < 10u)
                    meshBoundsReadLegacy(*reinterpret_cast<MeshBounds*>(lod), stream);
                else
                    meshBoundsReadMinMax(*reinterpret_cast<MeshBounds*>(lod), stream);

                // +0x3C counts the bone-matrix vectors beginning at +0x40, with 16-byte stride.
                stream->read(lod + 60, 4);                                  // +0x3C
                const unsigned int arrays = *reinterpret_cast<unsigned int*>(lod + 60);
                if (arrays)
                {
                    vc71::vector_layout* palette =
                        reinterpret_cast<vc71::vector_layout*>(lod + 64);   // +0x40
                    for (unsigned int a = 0; a < arrays; ++a)
                    {
                        unsigned int count = 0;
                        stream->read(&count, 4);
                        skinnedMeshMatrixVectorResize(*palette, count);
                        stream->read(palette->first, 68 * count);
                        ++palette;
                    }
                }

                if (remainingSkips > 0)
                {
                    --remainingSkips;
                    --lodIndex;
                    byteOffset -= 132;
                }
                byteOffset += 132;
                ++lodIndex;
            }
            while (lodIndex < kept);
        }

        // Copy distance source[skip + lod], but advance the accumulator by source[lod].
        // Preserve this asymmetric indexing and the 50-unit default step.
        const int   skipped       = reinterpret_cast<int*>(m_geomInts.first)[geom];
        const int   distanceCount = kept - 1;
        float       accumulator   = static_cast<float>(skipped + 1) * 50.0f;

        for (int lod = 0; lod < distanceCount; ++lod)
        {
            float* const destination = reinterpret_cast<float*>(distances.first);

            bool stored = false;
            if (m_lodDistances.first
                && static_cast<unsigned int>(
                       (m_lodDistances.last - m_lodDistances.first) >> 4) > geom)
            {
                const vc71::vector_layout* const inner =
                    reinterpret_cast<const vc71::vector_layout*>(m_lodDistances.first) + geom;
                const int innerSize =
                    inner->first
                        ? static_cast<int>((inner->last - inner->first) >> 2) : 0;
                if (innerSize > lod)
                {
                    const float* const source =
                        reinterpret_cast<const float*>(inner->first);
                    destination[lod] = source[skipped + lod];    // skip-adjusted
                    accumulator     += source[lod];              // NOT skip-adjusted
                    stored = true;
                }
            }

            if (!stored)
            {
                destination[lod] = accumulator;
                accumulator += 50.0f;
            }
        }
    }
}


// The five slots the RW-193 audit turned up. except finalizeMaterials.


// +0x5C, 0x1010EF00. (-1, -1) means "every geom and lod", and the body recurses through its own
// vtable slot to reach each one. Otherwise it sums, or returns, the 456-byte material record's
// +0x24 - the vertex count readMaterials' tail writes.
int SkinnedMeshTemplate::getVertexCount(int geom, int lod, int material)
{
    if (geom == -1 && lod == -1)
    {
        const unsigned int geomCount =
            m_lodCounts.first
                ? static_cast<unsigned int>(m_lodCounts.last - m_lodCounts.first) >> 2
                : 0u;
        int total = 0;
        for (unsigned int g = 0; g < geomCount; ++g)
        {
            const unsigned int lodCount =
                static_cast<unsigned int>(reinterpret_cast<int*>(m_lodCounts.first)[g]);
            for (unsigned int l = 0; l < lodCount; ++l)
                total += getVertexCount(static_cast<int>(g), static_cast<int>(l), -1);
        }
        return total;
    }

    const vc71::vector_layout* const lodCompanion =
        reinterpret_cast<const vc71::vector_layout*>(
            reinterpret_cast<const vc71::vector_layout*>(m_vector0CC.first)[geom].first) + lod;

    if (material == -1)
    {
        int count = 0;
        const unsigned char* record =
            reinterpret_cast<const unsigned char*>(lodCompanion->first);
        const int materials =
            record ? static_cast<int>((lodCompanion->last - lodCompanion->first) / 456) : 0;
        for (int m = 0; m < materials; ++m, record += 456)
            count += *reinterpret_cast<const int*>(record + 36);
        return count;
    }

    return *reinterpret_cast<const int*>(
        reinterpret_cast<const unsigned char*>(lodCompanion->first) + 456 * material + 36);
}

// +0xA0, how many 456-byte material records this lod has, summed over every geom.
int SkinnedMeshTemplate::getMaterialCount(unsigned int lod)
{
    int total = 0;
    const unsigned int geomCount =
        m_vector0DC.first
            ? static_cast<unsigned int>(m_vector0DC.last - m_vector0DC.first) >> 5
            : 0u;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const vc71::vector_layout* const lods =
            reinterpret_cast<const vc71::vector_layout*>(
                reinterpret_cast<const unsigned char*>(m_vector0DC.first) + 32 * geom);
        if (!lods->first
            || lod >= static_cast<unsigned int>((lods->last - lods->first) / 132))
            continue;

        const vc71::vector_layout* const lodCompanion =
            reinterpret_cast<const vc71::vector_layout*>(
                reinterpret_cast<const vc71::vector_layout*>(m_vector0CC.first)[geom].first) + lod;
        if (lodCompanion->first)
            total += static_cast<int>((lodCompanion->last - lodCompanion->first) / 456);
    }
    return total;
}

// +0xA4, the same walk, but asking each material record's OWN vtable +0x10 and summing
// the answers. [UNRESOLVED] what that slot returns; MeshTemplate.h names this getLodPrimitiveCount.
int SkinnedMeshTemplate::getLodPrimitiveCount(unsigned int lod)
{
    int total = 0;
    const unsigned int geomCount =
        m_vector0DC.first
            ? static_cast<unsigned int>(m_vector0DC.last - m_vector0DC.first) >> 5
            : 0u;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const vc71::vector_layout* const lods =
            reinterpret_cast<const vc71::vector_layout*>(
                reinterpret_cast<const unsigned char*>(m_vector0DC.first) + 32 * geom);
        if (!lods->first
            || lod >= static_cast<unsigned int>((lods->last - lods->first) / 132))
            continue;

        const vc71::vector_layout* const lodCompanion =
            reinterpret_cast<const vc71::vector_layout*>(
                reinterpret_cast<const vc71::vector_layout*>(m_vector0CC.first)[geom].first) + lod;
        if (!lodCompanion->first)
            continue;

        const int materials =
            static_cast<int>((lodCompanion->last - lodCompanion->first) / 456);
        unsigned char* record = reinterpret_cast<unsigned char*>(lodCompanion->first);
        for (int m = 0; m < materials; ++m, record += 456)
        {
            typedef int(__fastcall * Fn)(void*, void*);
            total += reinterpret_cast<Fn>(
                (*reinterpret_cast<void***>(record))[0x10 / sizeof(void*)])(record, 0);
        }
    }
    return total;
}

// +0xC4, 0x1010E5E0. THE override IS `nullsub_58` - IDA's name for an empty body. This class
// genuinely merges nothing, and MeshTemplate's base slot is `ret`, so the eight years this went
// undeclared changed nothing. Written anyway, so the slot table stops lying.
void SkinnedMeshTemplate::mergeMaterials()
{
}


// Finalizes draw-state handles and retained textures for every material.
// Texture replacement releases the old reference only when pointer identity changes.

namespace {

// Releases the previous texture through a temporary, then replaces the slot.
void skinnedMeshAssignTextureSlot(unsigned char* record, unsigned int slot, void* texture)
{
    void** const held = reinterpret_cast<void**>(record + slot);
    if (*held == texture)
        return;                                                // identity guard, as shipped
    ::dice::hfe::dx9rend::EngineTexture* doomed =
        static_cast< ::dice::hfe::dx9rend::EngineTexture*>(*held);
    ::dice::hfe::dx9rend::textureReleaseRef(doomed);
    *held = texture;
    if (texture)
    {
        typedef void(__fastcall * AddRefFn)(void*, void*);
        reinterpret_cast<AddRefFn>(
            (*reinterpret_cast<void***>(texture))[0x08 / sizeof(void*)])(texture, 0);
    }
}

unsigned int bufferHandleAt(void* wrapper, unsigned int byteOffset)
{
    return wrapper
        ? *reinterpret_cast<unsigned int*>(
              static_cast<unsigned char*>(wrapper) + byteOffset)
        : 0u;
}

} // namespace

void SkinnedMeshTemplate::finalizeMaterials()
{
    const unsigned int geomCount =
        m_vector0CC.first
            ? static_cast<unsigned int>(m_vector0CC.last - m_vector0CC.first) >> 4
            : 0u;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const vc71::vector_layout* const geomCompanion =
            reinterpret_cast<const vc71::vector_layout*>(m_vector0CC.first) + geom;
        if (!geomCompanion->first)
            continue;

        const unsigned int lodCount =
            static_cast<unsigned int>(geomCompanion->last - geomCompanion->first) >> 4;

        for (unsigned int lod = 0; lod < lodCount; ++lod)
        {
            const vc71::vector_layout* const lodCompanion =
                reinterpret_cast<const vc71::vector_layout*>(geomCompanion->first) + lod;
            if (!lodCompanion->first)
                continue;

            const int materials =
                static_cast<int>((lodCompanion->last - lodCompanion->first) / 456);
            unsigned char* record = reinterpret_cast<unsigned char*>(lodCompanion->first);

            for (int m = 0; m < materials; ++m, record += 456)
            {
                *reinterpret_cast<unsigned int*>(record + 400) = m_primitiveType;          // +0xAC
                *reinterpret_cast<unsigned int*>(record + 404) =
                    bufferHandleAt(m_vertexBuffer, 8);
                *reinterpret_cast<unsigned int*>(record + 408) =
                    bufferHandleAt(m_vertexBuffer, 40);
                *reinterpret_cast<unsigned int*>(record + 412) =
                    bufferHandleAt(m_indexBuffer, 8);
                *reinterpret_cast<float*>(record + 188) = 64.0f;

                if (record[394])
                {
                    record[84] = 1;
                    *reinterpret_cast<int*>(record + 220) = 127;
                    *reinterpret_cast<int*>(record + 212) = 1;
                }

                // The textures come from the record's OWN texture vector: +0x08 is its `first`,
                // so `(record + 8)` is texture 0 and `*(*(record + 8) + 4)` is texture 1.
                void** const textures = *reinterpret_cast<void***>(record + 8);
                if (textures)
                {
                    skinnedMeshAssignTextureSlot(record, 244, textures[0]);
                    if (record[392])
                        skinnedMeshAssignTextureSlot(record, 276, textures[1]);
                }
            }
        }
    }
}

// +0x10, (6 B): `mov eax, ds:1021AF00; retn`, the SkinnedMesh class id. See the
// declaration: without it the class answered MeshTemplate's 0x870CA650.
unsigned int SkinnedMeshTemplate::getClassId() const
{
    return 0x870CA652u;
}

void SkinnedMeshTemplate::setLodDistance(int, int, float)
{
    RD9_FRONTIER("dice::hfe::geom::SkinnedMeshTemplate::setLodDistance (PE 0x1010FAA0)", 0x30);
}

float SkinnedMeshTemplate::getLodDistance(int, int)
{
    RD9_FRONTIER("dice::hfe::geom::SkinnedMeshTemplate::getLodDistance (PE 0x1010EB10)", 0x34);
    return 0.0f;
}

unsigned int SkinnedMeshTemplate::getMemoryUsage(int)
{
    RD9_FRONTIER("dice::hfe::geom::SkinnedMeshTemplate::getMemoryUsage (PE 0x1010F810)", 0x54);
    return 0;
}

int SkinnedMeshTemplate::getPrimitiveCount(int, int, int)
{
    RD9_FRONTIER("dice::hfe::geom::SkinnedMeshTemplate::getPrimitiveCount (PE 0x1010E5F0)", 0x58);
    return 0;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
