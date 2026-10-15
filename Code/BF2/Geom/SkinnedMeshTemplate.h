// Path: [INFERRED] header beside SkinnedMeshTemplate.cpp, attested at RendDX9 0x1021BF40.
// Soldier mesh template, recovered from RendDX9.dll 1.50 (PE only).
// Retail layout: 0x10C bytes; constructor 0x101106B0, primary table 0x1021C160.
// Override positions come from the vtable, including shared and empty bodies.

#ifndef GEOM_SKINNEDMESHTEMPLATE_H     // [INFERRED] guard style, matching StaticMeshTemplate.h
#define GEOM_SKINNEDMESHTEMPLATE_H

#ifdef BF2_IMAGE_RENDDX9

#include "MeshTemplate.h"
#include "../Math/Mat4.h"
#include <string>

#include <map>
#include <set>
#include <vector>

struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexDeclaration9;

namespace dice {
namespace hfe {
namespace dx9rend {
struct EngineTexture;
namespace Ra { struct RaVertexElementDesc; }
}
namespace geom {

class SkinnedMesh;

// sub_10112090 through the template's +0x100 set - erase one live instance. Published because
// `SkinnedMesh::~SkinnedMesh` is what calls it; see the definition's note for why the shipped
// inlined `equal_range` + range-erase and this erase-by-key are the same operation.
void instanceSetErase(std::set<SkinnedMesh*>& set, SkinnedMesh* instance);


// One bone part of a lod's palette (68 bytes): the skeleton bone it follows and the part's own
// transform. readLods resizes these with an identity transform and a zero part id.
struct SkinnedMeshPart
{
    int  partId;      // +0x00  index into the instance's skeleton matrices
    Mat4 transform;   // +0x04
};

static_assert(sizeof(SkinnedMeshPart) == 68, "readLods reads parts as one 68-byte-stride block");

// One 132-byte lod record. readLods fills the bounds, the palette count and the part vectors;
// readMaterials clears and sets the two flags.
struct SkinnedMeshLod
{
    MeshBounds                    bounds;        // +0x00  the draws read radius +0x1C, dirty +0x20
    unsigned int                  paletteCount;  // +0x3C  how many part vectors follow
    std::vector<SkinnedMeshPart>  parts[4];      // +0x40  one per material palette, 16-byte stride
    unsigned char                 humanSkin;     // +0x80  a material shader name holds "humanskin"
    unsigned char                 hasAlphaTest;  // +0x81  [INFERRED] the shadow draw's alpha gate
    unsigned char                 pad82[2];      // +0x82
};

// One geom's 32-byte record: its lods and the lod switch distances (one fewer than the lods).
struct SkinnedMeshGeomRecord
{
    std::vector<SkinnedMeshLod>  lods;        // +0x00
    std::vector<float>           distances;   // +0x10
};

// The vertex-declaration cache's mapped value. The shipped node carries 20 value bytes; only the
// first word, the cached declaration, is read or written. [UNRESOLVED] the remaining sixteen.
struct SkinnedMeshDeclarationEntry
{
    IDirect3DVertexDeclaration9* declaration;   // +0x00
    unsigned char                unresolved04[16];
};

// One 456-byte skinned-mesh material record (retail +0x00..+0x1C7). The render queues dispatch
// through the nine-slot table at +0x00; SkinnedMeshTemplate.cpp defines the special members:
// construction installs the table and the skinning vertex elements, copy builds a fresh
// parameter block and assigns it, assignment is the shipped commit, and destruction clears the
// table pointer.
struct SkinnedMeshMaterial
{
    typedef std::map<unsigned int, SkinnedMeshDeclarationEntry> DeclarationMap;

    explicit SkinnedMeshMaterial(bool hasTangent);
    SkinnedMeshMaterial(const SkinnedMeshMaterial& other);
    SkinnedMeshMaterial& operator=(const SkinnedMeshMaterial& other);
    ~SkinnedMeshMaterial();

    void* const*                                    vtable;           // +0x000  nine slots
    std::vector<dx9rend::EngineTexture*>            textures;         // +0x004
    unsigned int                                    dword14;          // +0x014  [UNRESOLVED]
    unsigned int                                    vertexStart;      // +0x018  baseVertex
    unsigned int                                    indexStart;       // +0x01C
    unsigned int                                    primitiveCount;   // +0x020
    unsigned int                                    vertexCount;      // +0x024
    std::vector<const dx9rend::Ra::RaVertexElementDesc*> elements;    // +0x028
    // The Ra material block (raMaterialParametersConstruct, RaMaterialParameters); retail
    // +0x038..+0x187.
    alignas(RaMaterialParameters) unsigned char     parameters[sizeof(RaMaterialParameters)];
    unsigned char                                   hasNormalMap;     // +0x188  define bit 1
    unsigned char                                   objSpaceNormalMap;// +0x189  define bit 2
    unsigned char                                   isAlpha;          // +0x18A  the alpha pass
    unsigned char                                   hasEnvMap;        // +0x18B  define bit 3
    unsigned char                                   flag18C;          // +0x18C  [UNRESOLVED]
    unsigned char                                   pad18D[3];        // +0x18D
    unsigned int                                    primitiveType;    // +0x190  constructed 4
    IDirect3DVertexBuffer9*                         vertexBuffer;     // +0x194
    unsigned int                                    vertexStride;     // +0x198
    IDirect3DIndexBuffer9*                          indexBuffer;      // +0x19C
    std::string                                    string1A0;        // +0x1A0  [UNRESOLVED] role
    DeclarationMap                                  declarations;     // +0x1BC  by shader build serial
};

// The material's Ra parameter block (retail record +0x38), typed.
inline RaMaterialParameters& skinnedMeshMaterialParameters(SkinnedMeshMaterial& material)
{
    return *reinterpret_cast<RaMaterialParameters*>(material.parameters);
}


// Soldier mesh loader with derived reference counting at +0xC8.
// readTrailer is intentionally empty: reading the base trailer would shift the stream.
// Preserve the unresolved +0xED..+0xFB bytes.
class SkinnedMeshTemplate : public MeshTemplate
{
public:
    typedef std::vector<SkinnedMeshMaterial>          LodMaterials;    // one lod's materials
    typedef std::vector<LodMaterials>                 GeomMaterials;   // one geom's lods
    typedef std::vector<GeomMaterials>                MaterialTable;   // geom -> lod -> material
    typedef std::vector<SkinnedMeshGeomRecord>        GeomTable;

    SkinnedMeshTemplate();

    // The overridden quartet counts at +0xC8 - the same three bodies StaticMeshTemplate uses,
    // shared in the image (0x100EEA10 / 0x100EEA20 / 0x100EEA30) with its own queryInterface.
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    // Returns the skinned-template class ID used by shadow selection and effect lookup.
    virtual unsigned int getClassId() const;                        // +0x10
    virtual void* createInstance();                                 // +0x24
    virtual bool  load();                                           // +0x2C
    virtual void  setLodDistance(int geom, int lod, float distance);// +0x30
    virtual float getLodDistance(int geom, int lod);                // +0x34
    virtual unsigned int getMemoryUsage(int mask);                  // +0x54
    virtual int   getPrimitiveCount(int geom, int lod, int material);// +0x58

    // Skinned meshes omit the base trailer dword. LODs include bounds, bone matrices
    // and fallback distances; materials initialize technique masks and shared shadow state.
    virtual void  readTrailer(::renddx9abi::HostNonlinearStream* stream);

    virtual void  readLods(::renddx9abi::HostNonlinearStream* stream);

    // Skinned-template overrides used by geometry queries and the load pipeline.
    virtual int   getVertexCount(int geom, int lod, int material);   // +0x5C
    virtual int   getMaterialCount(unsigned int lod);                // +0xA0
    virtual int   getLodPrimitiveCount(unsigned int lod);            // +0xA4
    virtual void  mergeMaterials();                                  // +0xC4
    virtual void  finalizeMaterials();                               // +0xD0

    virtual void  readMaterials(::renddx9abi::HostNonlinearStream* stream);

    // data, in shipped order; the `+0xNN` comments are the retail offsets
    int                 m_refCount;          // +0xC8
    MaterialTable       m_materials;         // +0xCC  geom -> lod -> SkinnedMeshMaterial
    GeomTable           m_geoms;             // +0xDC  geom -> lods and lod distances
    unsigned char       m_byte0EC;            // +0xEC
    unsigned char       m_unknown0ED[15];     // +0xED..+0xFB  [UNRESOLVED], ctor-untouched
    unsigned char       m_byte0FC;            // +0xFC
    unsigned char       m_pad0FD[3];          // +0xFD..+0xFF
    std::set<SkinnedMesh*> m_instances;       // +0x100  live instances, in pointer order
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
#endif // GEOM_SKINNEDMESHTEMPLATE_H
