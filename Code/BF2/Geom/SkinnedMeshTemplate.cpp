// Loads skinned-mesh geometry, LODs, materials and shared shadow parameters.


#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
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
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {
// sub_10017E00 on dword_1023B974 - declared per-TU, as StaticMeshStateManager.cpp and
// StaticMesh.cpp both do; the texture manager publishes no header for it.
int textureManagerSwapDefaultMipClass(int value);

// BufferManager.cpp - the wrapper fields (+0x08 D3D object, +0x28 stride).
unsigned int vertexBufferStride(void* wrapper);
IDirect3DVertexBuffer9* vertexBufferD3D(void* wrapper);
IDirect3DIndexBuffer9* indexBufferD3D(void* wrapper);
} // namespace dx9rend

namespace geom {

namespace {

// Tracks live instances in the set RendDX9/BF2Render.cpp's setLods also walks.
void instanceSetInsert(std::set<SkinnedMesh*>& set, SkinnedMesh* instance)
{
    // The shipped insert returns pair<iterator,bool> through a hidden slot; every caller in the
    // image discards it, and so does this one.
    static_cast<void>(set.insert(instance));
}

} // namespace


// Removes the instance from the unique-key set; does nothing when absent.

void instanceSetErase(std::set<SkinnedMesh*>& set, SkinnedMesh* instance)
{
    set.erase(instance);
}


// Creates the requested template interface. Allocation failure is not checked before
// queryInterface, preserving the original failure behavior.

IBase* __fastcall createSkinnedMeshTemplate(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(sizeof(SkinnedMeshTemplate));
    SkinnedMeshTemplate* self = 0;
    if (storage)
        self = new (storage) SkinnedMeshTemplate();

    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}


// Initializes derived state after MeshTemplate and clears the keep byte last. The material table
// (+0xCC) and the geom table (+0xDC) are constructed as members.
// Padding at +0xED..+0xFB remains untouched.

SkinnedMeshTemplate::SkinnedMeshTemplate()
    : MeshTemplate()
{
    m_refCount = 1;                       // +0xC8

    m_byte0EC = 0;                        // +0xEC
    m_byte0FC = 0;                        // +0xFC

    // The instance set (0x101117A0's header node, retail +0x100) is constructed as a member.

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

// MeshTemplate's per-geom vectors - +0x50 lod counts, +0x60 lod distances, +0x70 skipped lods.
template<class T>
unsigned int meshGeomCount(const std::vector<T>& v)
{
    return static_cast<unsigned int>(v.size());
}

template<class T>
T& meshGeomInt(std::vector<T>& v, unsigned int geom)
{
    return v.data()[geom];
}

// The source lod distances of `geom` and their count; null and 0 when the table has no such geom.
template<class Inner>
const float* meshLodDistances(const std::vector<Inner>& v, unsigned int geom, int& count)
{
    count = 0;
    if (v.size() <= geom)
        return 0;
    count = static_cast<int>(v[geom].size());
    return v[geom].data();
}

// g_Renderer + 0x7E4 - the renderer-level lod-skip setting readLods clamps against.
// StaticMeshTemplate.cpp carries the same read as `rendererLodSkipBase`, file-local.
int skinnedMeshLodSkipBase()
{
    return dx9rend::rendererTailInt(0x7E4);
}

// sub_10112CF0 - resize a vector of 68-byte bone-matrix records. \u2605 THE prototype IS AN identity
// matrix AT offset +0x04, not +0x00: the shipped builder stores 1.0f to +0x04, +0x18, +0x2C and
// +0x40 of a zeroed 68-byte block, which is a Mat4 diagonal shifted up by one dword. So each record
// is {dword, Mat4}.
void skinnedMeshMatrixVectorResize(std::vector<SkinnedMeshPart>& v, unsigned int count)
{
    SkinnedMeshPart prototype;
    std::memset(&prototype, 0, sizeof(prototype));
    prototype.transform.m[0]  = 1.0f;                     // +0x04
    prototype.transform.m[5]  = 1.0f;                     // +0x18
    prototype.transform.m[10] = 1.0f;                     // +0x2C
    prototype.transform.m[15] = 1.0f;                     // +0x40
    v.resize(count, prototype);
}

// Resizes a texture-pointer vector, filling new elements with null.
void textureVectorResize(std::vector<dx9rend::EngineTexture*>& v, unsigned int count)
{
    v.resize(count, static_cast<dx9rend::EngineTexture*>(0));
}

void skinnedMeshReserveGeoms(SkinnedMeshTemplate::MaterialTable& companions, unsigned int count)
{
    companions.resize(count);
}

void skinnedMeshReserveLods(SkinnedMeshTemplate::GeomMaterials* geomCompanion, int count)
{
    if (!geomCompanion || count < 0)
        return;
    geomCompanion->resize(static_cast<unsigned int>(count));
}

// Expands a lod's materials with copies of the prototype.
// Each record is overwritten by material parsing before its storage is used.
void skinnedMeshReserveMaterials(SkinnedMeshTemplate::LodMaterials* lodCompanion,
                                 unsigned int count, const SkinnedMeshMaterial* prototype)
{
    if (!lodCompanion || !prototype)
        return;
    lodCompanion->resize(count, *prototype);
}

// Destroys a material and frees its engine allocation when flags bit 0 is set.
// Returns the original pointer. The material keeps the nine-slot interface used by render queues.
void* skinnedMeshMaterialDestroyFlags(void* material, unsigned char flags)
{
    static_cast<SkinnedMeshMaterial*>(material)->~SkinnedMeshMaterial();
    if (flags & 1)
        ::renddx9abi::engineFree(material);
    return material;
}

// Binds vertex streams and a declaration cached by shader build serial.
// Missing elements report failure on every call and are not cached.
bool skinnedMeshMaterialApplyStreams(void* material, void* shaderState)
{
    namespace Ra = ::dice::hfe::dx9rend::Ra;
    SkinnedMeshMaterial* const m = static_cast<SkinnedMeshMaterial*>(material);
    Ra::RaShader* const shader = static_cast<Ra::RaShader*>(shaderState);
    SkinnedMeshMaterial::DeclarationMap& declarations = m->declarations;          // +0x1BC
    const unsigned int key = static_cast<unsigned int>(shader->m_buildSerial);  // +0x174

    SkinnedMeshMaterial::DeclarationMap::iterator node = declarations.find(key);
    if (node == declarations.end())
    {
        Ra::RaStreamRequirements* const required =
            Ra::raStreamRequirementsCreate(shader->m_requiredVertexElements);   // sub_10049330 + sub_1004A0F0
        Ra::RaDeclarationBuilder builder;
        Ra::raDeclarationBuilderConstruct(builder);

        const std::vector<const Ra::RaVertexElementDesc*>& elements = m->elements;   // +0x28
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
        // Insert first (the new value is zero-filled), then build and store the declaration.
        SkinnedMeshDeclarationEntry& inserted = declarations[key];
        inserted.declaration =
            Ra::raVertexDeclarationCached(Ra::raDeclarationBuilderFinish(builder), device);
        node = declarations.find(key);
        Ra::raDeclarationBuilderDestroy(builder);
        Ra::raStreamRequirementsDestroy(required);
    }

    // The bind, in the shipped order: stream 0 from +0x194 at the +0x198 stride, the declaration,
    // then the index buffer at +0x19C.
    ::dice::hfe::dx9rend::stateManagerSetStreamSource(
        0, m->vertexBuffer, 0, m->vertexStride);                               // +0x58
    ::dice::hfe::dx9rend::stateManagerSetVertexDeclaration(
        node->second.declaration);                                              // +0x5C
    ::dice::hfe::dx9rend::stateManagerSetIndices(m->indexBuffer);              // +0x54
    return true;
}

SkinnedMeshMaterial* materialOf(void* self)
{
    return static_cast<SkinnedMeshMaterial*>(self);
}

// The nine __fastcall entries the table holds. The shipped records are __thiscall, which is this
// receiver-in-ecx shape with the unused edx slot - the same idiom RenderQueues.cpp's own vtables
// use, and the same one `materialApplyStreams` calls back through.
void* RD9_THUNK smm_destroy(void* self RD9_THUNK_EDX, unsigned char flags)
{ return skinnedMeshMaterialDestroyFlags(self, flags); }

unsigned char RD9_THUNK smm_applyStreams(void* self RD9_THUNK_EDX, void* shaderState)
{ return skinnedMeshMaterialApplyStreams(self, shaderState) ? 1u : 0u; }

int RD9_THUNK smm_getVertexCount(void* self RD9_THUNK_EDX)
{ return static_cast<int>(materialOf(self)->vertexCount); }              // +0x24
int RD9_THUNK smm_slot0C(void* RD9_THUNK_EDX)                 { return -1; }
int RD9_THUNK smm_getPrimitiveCount(void* self RD9_THUNK_EDX)
{ return static_cast<int>(materialOf(self)->primitiveCount); }           // +0x20
int RD9_THUNK smm_getDword190(void* self RD9_THUNK_EDX)
{ return static_cast<int>(materialOf(self)->primitiveType); }            // +0x190
int RD9_THUNK smm_getVertexStart(void* self RD9_THUNK_EDX)
{ return static_cast<int>(materialOf(self)->vertexStart); }              // +0x18
int RD9_THUNK smm_slot1C(void* RD9_THUNK_EDX)                 { return 0; }
int RD9_THUNK smm_getIndexStart(void* self RD9_THUNK_EDX)
{ return static_cast<int>(materialOf(self)->indexStart); }               // +0x1C

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

} // anonymous namespace


// Constructs a material record.
// Installs the nine-slot render interface and skinning vertex elements; hasTangent controls
// tangent-space support. Missing support reports the existing diagnostic. The texture vector, the
// element vector, the +0x1A0 string and the declaration map are constructed as members.

SkinnedMeshMaterial::SkinnedMeshMaterial(bool hasTangent)
    : vtable(kSkinnedMeshMaterialVtable),
      dword14(0), vertexStart(0), indexStart(0), primitiveCount(0), vertexCount(0)   // +0x14..+0x24
{
    ::dice::hfe::dx9rend::Ra::raMaterialParametersConstruct(parameters);   // +0x38

    hasNormalMap      = 0;                             // +0x188
    objSpaceNormalMap = 0;                             // +0x189
    isAlpha           = 0;                             // +0x18A
    hasEnvMap         = 0;                             // +0x18B
    flag18C           = 0;                             // +0x18C

    primitiveType = 4;                                 // +0x190, four, not zero
    vertexBuffer  = 0;                                 // +0x194
    vertexStride  = 0;                                 // +0x198
    indexBuffer   = 0;                                 // +0x19C

    elements.push_back(&::dice::hfe::dx9rend::Ra::g_desc_1023EF04);   // Vtx_Position
    elements.push_back(&::dice::hfe::dx9rend::Ra::g_desc_1023EF7C);   // Vtx_Normal
    elements.push_back(&::dice::hfe::dx9rend::Ra::g_desc_1023ED74);   // Vtx_BoneWeights
    elements.push_back(&::dice::hfe::dx9rend::Ra::g_desc_1023EEDC);   // Vtx_BoneIdcs
    elements.push_back(&::dice::hfe::dx9rend::Ra::g_desc_1023ED4C);   // Vtx_TBase

    if (hasTangent)
    {
        elements.push_back(&::dice::hfe::dx9rend::Ra::g_desc_1023EFF4);
    }
    else
    {
        ::renddx9abi::logf("SkinnedMeshTemplate.cpp:101: tl: Material created _without_ tangent "
                           "support. Something must be wrong! (because we haven't implemented "
                           "this yet!!)");
    }
}

// [CHOSEN, not recovered] Copy-construction (the lod material vectors' resize from a prototype):
// a fresh parameter block assigned from the source, every other field copied.
SkinnedMeshMaterial::SkinnedMeshMaterial(const SkinnedMeshMaterial& other)
    : vtable(other.vtable), textures(other.textures),
      dword14(other.dword14), vertexStart(other.vertexStart), indexStart(other.indexStart),
      primitiveCount(other.primitiveCount), vertexCount(other.vertexCount),
      elements(other.elements),
      hasNormalMap(other.hasNormalMap), objSpaceNormalMap(other.objSpaceNormalMap),
      isAlpha(other.isAlpha), hasEnvMap(other.hasEnvMap), flag18C(other.flag18C),
      primitiveType(other.primitiveType), vertexBuffer(other.vertexBuffer),
      vertexStride(other.vertexStride), indexBuffer(other.indexBuffer),
      string1A0(other.string1A0), declarations(other.declarations)
{
    std::memcpy(pad18D, other.pad18D, sizeof(pad18D));
    ::dice::hfe::dx9rend::Ra::raMaterialParametersConstruct(parameters);
    ::dice::hfe::dx9rend::Ra::raMaterialParametersAssign(parameters, other.parameters);
}

// Copies a material into its live record. Map assignment guards self-assignment and
// clears the destination before copying.
SkinnedMeshMaterial& SkinnedMeshMaterial::operator=(const SkinnedMeshMaterial& other)
{
    // +0x04 - the texture vector. 4-byte elements.
    textures = other.textures;

    // +0x14 .. +0x24 - five dwords, four of which the draws read back as baseVertex, startIndex,
    // primitive count and vertex count.
    dword14        = other.dword14;
    vertexStart    = other.vertexStart;
    indexStart     = other.indexStart;
    primitiveCount = other.primitiveCount;
    vertexCount    = other.vertexCount;

    // +0x28 - the vertex-element vector. Also 4-byte elements (pointers to RaVertexElementDesc).
    elements = other.elements;

    ::dice::hfe::dx9rend::Ra::raMaterialParametersAssign(parameters, other.parameters);

    hasNormalMap      = other.hasNormalMap;                                // the five flag bytes
    objSpaceNormalMap = other.objSpaceNormalMap;
    isAlpha           = other.isAlpha;
    hasEnvMap         = other.hasEnvMap;
    flag18C           = other.flag18C;
    primitiveType = other.primitiveType;                                   // +0x190 .. +0x19C
    vertexBuffer  = other.vertexBuffer;
    vertexStride  = other.vertexStride;
    indexBuffer   = other.indexBuffer;

    string1A0 = other.string1A0;

    // The self-assignment guard compares the two map addresses, not the objects.
    declarations = other.declarations;
    return *this;
}

// Destroys the parameter block, then the members (the declaration map, the string, the element
// and texture vectors) release their storage.
SkinnedMeshMaterial::~SkinnedMeshMaterial()
{
    ::dice::hfe::dx9rend::Ra::raMaterialParametersDestroy(parameters);

    // [FRONTIER] The shared material base table is unavailable. Leave the destroyed
    // record's table pointer null; records must be reconstructed before any further dispatch.
    vtable = 0;
}

// Reads materials for each geometry/LOD, resolving textures and shader flags.
// The stream and LOD records must already be initialized. Uses mip class 12 for these loads
// and restores the previous class on normal return. Shared shadow state initializes first.
void SkinnedMeshTemplate::readMaterials(::renddx9abi::HostNonlinearStream* stream)
{

    // Keep the skinned-mesh mip class separate from static meshes (class 13).
    const int savedMipClass = ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(12);

    skinnedMeshInitTechniques();                       // sub_1010F190 - fills the mask vector

    const unsigned int geomCount = meshGeomCount(m_lodCounts);

    skinnedMeshReserveGeoms(m_materials, geomCount);   // sub_10115380 - one entry per geom

    // Initialize shared shadow parameters before reading material records.
    skinnedMeshInitShadowState();

    unsigned int skippedVertices = 0;                  // v93
    unsigned int skippedIndices  = 0;                  // v90

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const int lodCount = meshGeomInt(m_lodCounts, geom);
        // readLods records the number of LODs to skip for each geometry.
        int lodsToSkip = meshGeomInt(m_geomInts, geom);

        // The geom's own companion entry - the receiver measured at 0x1010FF49.
        GeomMaterials* const geomCompanion = m_materials.data() + geom;
        skinnedMeshReserveLods(geomCompanion, lodCount);   // sub_10114D00 - one entry per lod

        int destinationLod = 0;                        // the lod record / material vector index

        for (int lod = 0; lod < lodCount; )
        {
            // The 32-byte geom record's first vector holds 132-byte lod records; its companion in
            // the other table holds one material vector per lod. Both are indexed by the
            // destination lod, which is what the skip dance below keeps still.
            SkinnedMeshLod* const lodRecord = m_geoms.data()[geom].lods.data() + destinationLod;
            LodMaterials* const companion = geomCompanion->data() + destinationLod;

            // Reset per-LOD humanskin and alpha-test flags before scanning materials.
            lodRecord->humanSkin    = 0;               // lod +0x80
            lodRecord->hasAlphaTest = 0;               // lod +0x81

            unsigned int materialCount = 0;
            stream->read(&materialCount, 4);
            {
                // The prototype the resize copies, built on this frame exactly as the shipped
                // body builds it at `mov ecx, esp` immediately before the call.
                const SkinnedMeshMaterial materialPrototype(true);
                skinnedMeshReserveMaterials(companion, materialCount, &materialPrototype);
            }

            for (unsigned int material = 0; material < materialCount; ++material)
            {
                char* shaderName = 0;
                {
                // Build textures in the temporary material before committing it.
                SkinnedMeshMaterial scratchMaterial(true);
                std::vector<dx9rend::EngineTexture*>& textures = scratchMaterial.textures;   // +0x04

                unsigned int skipBytes = 0;
                stream->read(&skipBytes, 4);
                stream->skip(skipBytes);

                // The shader name, lowercased in place. The length is read, the buffer is
                // allocated length + 1 and NUL-terminated by hand - the stream stores no terminator.
                unsigned int nameLength = 0;
                stream->read(&nameLength, 4);
                shaderName = static_cast<char*>(::renddx9abi::engineAlloc(nameLength + 1));
                stream->read(shaderName, nameLength);
                shaderName[nameLength] = 0;
                ::strlwr(shaderName);

                // Match substrings in the lowercase shader name; draw flags belong to the LOD.
                if (std::strstr(shaderName, "tangent"))
                    m_byte0FC = 1;                     // +0xFC, the template-wide tangent flag
                if (std::strstr(shaderName, "humanskin"))
                    lodRecord->humanSkin = 1;

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

                    std::string prefix("");
                    std::string name(rawName);

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

                    // Skip bump maps at nonzero shader quality, and skip all textures for omitted LODs.
                    const unsigned int shaderQuality =
                        ::dice::hfe::dx9rend::rendererRaPath();             // Renderer +0x6CC
                    const bool isBumpName = std::strstr(name.c_str(), "_b.dds") != 0
                                         || std::strstr(name.c_str(), "_b_os.dds") != 0;
                    if (lodsToSkip <= 0 && (!shaderQuality || !isBumpName))
                    {
                        std::string full(prefix);
                        full += name;
                        void* const texture =
                            ::dice::hfe::dx9rend::textureManagerGetTexture(full);
                        if (!textures.empty())
                            textures[t] = static_cast<dx9rend::EngineTexture*>(texture);

                        // Slot 1 counts as a real texture only when it is not a SpecularLUT.
                        if (t == 1 && !std::strstr(name.c_str(), "SpecularLUT"))
                            hasRealSecondTexture = true;
                    }

                    ::renddx9abi::engineFree(rawName);
                }
                (void)hasRealSecondTexture;

                // Read draw-range values directly into the material record.
                stream->read(&scratchMaterial.vertexStart, 4);    // +0x18 baseVertex
                stream->read(&scratchMaterial.indexStart, 4);     // +0x1C startIndex

                unsigned int indexCount = 0;
                stream->read(&indexCount, 4);

                // Derive triangle-list/strip counts; other primitive types keep the initialized zero.
                if (m_primitiveType == 4)                         // D3DPT_TRIANGLELIST
                    scratchMaterial.primitiveCount = indexCount / 3u;      // +0x20
                else if (m_primitiveType == 5)                    // D3DPT_TRIANGLESTRIP
                    scratchMaterial.primitiveCount = indexCount - 2u;

                stream->read(&scratchMaterial.vertexCount, 4);    // +0x24 vertex count
                stream->skip(4);
                stream->skip(4);

                // The destination, measured at the record inside the companion.
                SkinnedMeshMaterial& record = companion->data()[material];

                record = scratchMaterial;

                if (lodsToSkip <= 0)
                {
                    // keep: slide this material's data down over whatever earlier lods skipped,
                    // and rebase its starts by the same amounts. Both arms are guarded on the
                    // running total being non-zero, so the first kept lod moves nothing.
                    if (skippedIndices)
                    {
                        unsigned int& startIndex = record.indexStart;   // +0x1C
                        const unsigned int previous = startIndex;
                        startIndex = previous - skippedIndices;
                        std::memmove(m_indexData + startIndex, m_indexData + previous,
                                     2u * indexCount);
                    }
                    if (skippedVertices)
                    {
                        unsigned int& baseVertex = record.vertexStart;  // +0x18
                        const unsigned int previous = baseVertex;
                        baseVertex = previous - skippedVertices;
                        const unsigned int vertexCount = record.vertexCount;   // +0x24
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
                        const unsigned int held = static_cast<unsigned int>(textures.size());
                        if (!held)
                            break;
                        if (j >= held)
                            break;
                        dx9rend::EngineTexture* const* const slots = textures.data();
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
                    skippedVertices += record.vertexCount;   // +0x24
                    skippedIndices  += indexCount;
                }

                }   // the scratch material is destroyed here

                ::renddx9abi::engineFree(shaderName);
            }

            // Reuse the destination slot after consuming a skipped LOD from the stream.
            if (lodsToSkip > 0)
            {
                --destinationLod;
                --lodsToSkip;
                --lod;
            }
            ++destinationLod;
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
    void* const storage = ::renddx9abi::engineAlloc(sizeof(SkinnedMesh));
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
        typedef void*(__thiscall * DtorFn)(void*, unsigned int);
        reinterpret_cast<DtorFn>(instance->m_vptr[0x34 / 4])(instance, 1);
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

    std::string path(m_path.c_str());
    path += kMeshesFolder;
    path += m_name;
    path += ".skinnedMesh";

    // Built, never read. See the header note.
    std::string gpuPath(m_path.c_str());
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
    for (std::set<SkinnedMesh*>::iterator node = m_instances.begin(); node != m_instances.end();
         ++node)
    {
        if (SkinnedMesh* const instance = *node)
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
    const unsigned int geomCount = meshGeomCount(m_lodCounts);

    // sub_10115700 - resize the 32-byte geom-record vector. Its prototype is a default record:
    // two empty vectors.
    m_geoms.resize(geomCount);
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

        const int lodCount = meshGeomInt(m_lodCounts, geom);
        if (skip >= rendererSkip)
            skip = rendererSkip;
        if (lodCount - 1 < skip)
            skip = lodCount - 1;

        meshGeomInt(m_geomInts, geom) = skip;                        // +0x70 remembers it
        const int kept = lodCount - skip;
        meshGeomInt(m_lodCounts, geom) = kept;

        SkinnedMeshGeomRecord& geomRecord = m_geoms[geom];
        std::vector<SkinnedMeshLod>& lods = geomRecord.lods;                // +0x00, 132-byte lods
        std::vector<float>& distances = geomRecord.distances;               // +0x10, floats

        // New lod records are all-zero (bounds not dirty) with empty part vectors.
        lods.resize(static_cast<unsigned int>(kept));
        const float kZeroDistance = 0.0f;
        distances.resize(static_cast<unsigned int>(kept - 1), kZeroDistance);

        // read each kept lod
        // The skip is implemented by rewinding the destination, so a skipped lod is read and
        // then overwritten by the next one. Same net-zero dance as readMaterials'.
        int remainingSkips = skip;
        int lodIndex       = 0;                        // also the destination record
        if (kept > 0)
        {
            do
            {
                SkinnedMeshLod* const lod = lods.data() + lodIndex;

                // The same version gate the StaticMesh sibling documents.
                if (m_head == 0 && m_version < 10u)
                    meshBoundsReadLegacy(lod->bounds, stream);
                else
                    meshBoundsReadMinMax(lod->bounds, stream);

                // +0x3C counts the bone-matrix vectors beginning at +0x40, with 16-byte stride.
                stream->read(&lod->paletteCount, 4);                        // +0x3C
                const unsigned int arrays = lod->paletteCount;
                if (arrays)
                {
                    std::vector<SkinnedMeshPart>* palette = lod->parts;     // +0x40
                    for (unsigned int a = 0; a < arrays; ++a)
                    {
                        unsigned int count = 0;
                        stream->read(&count, 4);
                        skinnedMeshMatrixVectorResize(*palette, count);
                        stream->read(palette->data(), 68 * count);
                        ++palette;
                    }
                }

                if (remainingSkips > 0)
                {
                    --remainingSkips;
                    --lodIndex;
                }
                ++lodIndex;
            }
            while (lodIndex < kept);
        }

        // Copy distance source[skip + lod], but advance the accumulator by source[lod].
        // Preserve this asymmetric indexing and the 50-unit default step.
        const int   skipped       = meshGeomInt(m_geomInts, geom);
        const int   distanceCount = kept - 1;
        float       accumulator   = static_cast<float>(skipped + 1) * 50.0f;

        for (int lod = 0; lod < distanceCount; ++lod)
        {
            float* const destination = distances.data();

            bool stored = false;
            int innerSize = 0;
            const float* const source = meshLodDistances(m_lodDistances, geom, innerSize);
            if (innerSize > lod)
            {
                destination[lod] = source[skipped + lod];        // skip-adjusted
                accumulator     += source[lod];                  // NOT skip-adjusted
                stored = true;
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
        const unsigned int geomCount = meshGeomCount(m_lodCounts);
        int total = 0;
        for (unsigned int g = 0; g < geomCount; ++g)
        {
            const unsigned int lodCount = static_cast<unsigned int>(meshGeomInt(m_lodCounts, g));
            for (unsigned int l = 0; l < lodCount; ++l)
                total += getVertexCount(static_cast<int>(g), static_cast<int>(l), -1);
        }
        return total;
    }

    // The table is indexed unguarded, as shipped.
    const LodMaterials& lodMaterials = m_materials.data()[geom].data()[lod];

    if (material == -1)
    {
        int count = 0;
        const SkinnedMeshMaterial* record = lodMaterials.data();
        const int materials = static_cast<int>(lodMaterials.size());
        for (int m = 0; m < materials; ++m, ++record)
            count += static_cast<int>(record->vertexCount);                 // +0x24
        return count;
    }

    return static_cast<int>(lodMaterials.data()[material].vertexCount);
}

// +0xA0, how many 456-byte material records this lod has, summed over every geom.
int SkinnedMeshTemplate::getMaterialCount(unsigned int lod)
{
    int total = 0;
    const unsigned int geomCount = static_cast<unsigned int>(m_geoms.size());

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const std::vector<SkinnedMeshLod>& lods = m_geoms[geom].lods;
        if (lods.empty() || lod >= static_cast<unsigned int>(lods.size()))
            continue;

        const LodMaterials& lodMaterials = m_materials.data()[geom].data()[lod];
        total += static_cast<int>(lodMaterials.size());
    }
    return total;
}

// +0xA4, the same walk, but asking each material record's OWN vtable +0x10 and summing
// the answers. [UNRESOLVED] what that slot returns; MeshTemplate.h names this getLodPrimitiveCount.
int SkinnedMeshTemplate::getLodPrimitiveCount(unsigned int lod)
{
    int total = 0;
    const unsigned int geomCount = static_cast<unsigned int>(m_geoms.size());

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const std::vector<SkinnedMeshLod>& lods = m_geoms[geom].lods;
        if (lods.empty() || lod >= static_cast<unsigned int>(lods.size()))
            continue;

        LodMaterials& lodMaterials = m_materials.data()[geom].data()[lod];
        if (lodMaterials.empty())
            continue;

        const int materials = static_cast<int>(lodMaterials.size());
        SkinnedMeshMaterial* record = lodMaterials.data();
        for (int m = 0; m < materials; ++m, ++record)
        {
            typedef int(__thiscall * Fn)(void*);
            total += reinterpret_cast<Fn>(record->vtable[0x10 / 4])(record);
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

// Releases the previous texture through a temporary, then replaces the holder's value.
void skinnedMeshAssignTextureSlot(RaMaterialParameters::Texture& holder, void* texture)
{
    void** const held = &holder.value;
    if (*held == texture)
        return;                                                // identity guard, as shipped
    ::dice::hfe::dx9rend::EngineTexture* doomed =
        static_cast< ::dice::hfe::dx9rend::EngineTexture*>(*held);
    ::dice::hfe::dx9rend::textureReleaseRef(doomed);
    *held = texture;
    if (texture)
    {
        typedef void(__thiscall * AddRefFn)(void*);
        reinterpret_cast<AddRefFn>(
            (*reinterpret_cast<void***>(texture))[0x08 / 4])(texture);
    }
}

// The vertex-buffer wrapper's stride (+0x28), or 0 for no wrapper.
unsigned int bufferStride(void* wrapper)
{
    return wrapper ? ::dice::hfe::dx9rend::vertexBufferStride(wrapper) : 0u;
}

// The wrappers' D3D interface pointers (+0x08), or null for no wrapper.
IDirect3DVertexBuffer9* vertexBufferObject(void* wrapper)
{
    return wrapper ? ::dice::hfe::dx9rend::vertexBufferD3D(wrapper) : 0;
}

IDirect3DIndexBuffer9* indexBufferObject(void* wrapper)
{
    return wrapper ? ::dice::hfe::dx9rend::indexBufferD3D(wrapper) : 0;
}

} // namespace

void SkinnedMeshTemplate::finalizeMaterials()
{
    const unsigned int geomCount = static_cast<unsigned int>(m_materials.size());

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        GeomMaterials& geomCompanion = m_materials[geom];
        if (geomCompanion.empty())
            continue;

        const unsigned int lodCount = static_cast<unsigned int>(geomCompanion.size());

        for (unsigned int lod = 0; lod < lodCount; ++lod)
        {
            LodMaterials& lodCompanion = geomCompanion[lod];
            if (lodCompanion.empty())
                continue;

            const int materials = static_cast<int>(lodCompanion.size());
            SkinnedMeshMaterial* record = lodCompanion.data();

            // The `+0xNN` / decimal comments are retail record offsets; the ones below +0x188
            // are inside the parameter block.
            for (int m = 0; m < materials; ++m, ++record)
            {
                record->primitiveType = m_primitiveType;                     // 400, from +0xAC
                record->vertexBuffer = vertexBufferObject(m_vertexBuffer);   // 404
                record->vertexStride = bufferStride(m_vertexBuffer);         // 408
                record->indexBuffer = indexBufferObject(m_indexBuffer);      // 412
                RaMaterialParameters& block = skinnedMeshMaterialParameters(*record);
                block.specularPower.value = 64.0f;                           // 188

                if (record->isAlpha)                                         // 394
                {
                    block.alphaTest.value = 1;                               // 84
                    block.alphaTestRef.value = 127;                          // 220
                    block.cullMode.value = 1;                                // 212
                }

                // The textures come from the record's OWN texture vector (+0x04, whose storage
                // pointer is at +0x08): texture 0 and texture 1, unguarded once storage exists.
                if (record->textures.capacity())
                {
                    dx9rend::EngineTexture* const* const textures = record->textures.data();
                    skinnedMeshAssignTextureSlot(block.diffuseMap, textures[0]);   // 244
                    if (record->hasNormalMap)                                // 392
                        skinnedMeshAssignTextureSlot(block.normalMap, textures[1]);   // 276
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
