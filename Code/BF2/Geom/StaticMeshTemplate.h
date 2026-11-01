// Path: [INFERRED] header beside attested StaticMeshTemplate.cpp (RendDX9 0x1021C2B8)
// and StaticMeshStateManager.cpp (0x10221xxx region).
// Static mesh template and material records, recovered from RendDX9.dll 1.50 (PE only).
// Retail template layout: 0x2CC bytes; constructor 0x10119E00, primary table 0x1021C520.

#ifndef BF2_GEOM_STATICMESHTEMPLATE_H
#define BF2_GEOM_STATICMESHTEMPLATE_H

#include "MeshTemplate.h"

namespace dice {
namespace hfe {
namespace geom {

class StaticMeshTemplate;


// StaticMeshLod - 104 bytes (0x68), a POD (readLods memsets it, nothing destroys it).

struct StaticMeshLod
{
    unsigned int  nodeCount;         // +0x00  the file's node count; readLods seeks past 64 B each
    MeshBounds    bounds;            // +0x04 .. +0x40
    unsigned int  primitiveType;     // +0x40  readMaterials: the template's primitive type
    unsigned int  primitiveCount;    // +0x44  readMaterials: += material primitive count
    unsigned int  vertexCount;       // +0x48  readMaterials: += material vertex count
    unsigned int  vertexStart;       // +0x4C  readMaterials: the first material's
    unsigned int  indexStart;        // +0x50  readMaterials: the first material's
    unsigned char hasAlphaMaterial;  // +0x54  readMaterials: a material with alphaMode == 1
    unsigned char needsAltStreams;   // +0x55  readMaterials: (flags & 0xC0) || (flags & 0x100)
    unsigned char pad56[6];          // +0x56
    // +0x5C..+0x67 is the shadow index-merge state: StaticMeshTemplate::mergeShadowIndexRanges
    // (0x10116580) sets the flag once the lod's materials have been rebased onto one vertex start
    // and sums their counts, and the StaticMesh shadow draw (0x100FD580) submits the whole lod in
    // one call when the flag is up.  readMaterials zeroes the flag.
    unsigned char byte5C;            // +0x5C  readMaterials writes 0; the merge writes 1
    unsigned char pad5D[3];          // +0x5D
    unsigned int  mergedPrimitiveCount; // +0x60  the merge: sum of the lod's material primitive counts
    unsigned int  mergedVertexCount;    // +0x64  the merge: sum of the lod's material vertex counts
};

static_assert(sizeof(StaticMeshLod) == 0x68, "readLods divides by 104 and memsets 0x68");


// StaticMeshGeom - 32 bytes: the lod records and the lod switch distances of one geom.
// The destructor at 0x1011E640 frees the second vector first, then the first.

struct StaticMeshGeom
{
    vc71::vector_layout lods;          // +0x00  vector<StaticMeshLod>
    vc71::vector_layout lodDistances;  // +0x10  vector<float>
};

static_assert(sizeof(StaticMeshGeom) == 0x20, "the geom vector divides by 32");


// ShaderMaterial - the 0x4C-byte record StaticMeshStateManager::createMaterial (0x101A9270)
// returns for every mesh material: the lowercased fx name, the shader-combo id decoded from it
// (0x101A8D10), and up to eight EngineTexture references. Slot 0 of `textures` is never filled:
// the first map lands in slot 1, so the sky reads its sky/cloud textures at +0x2C / +0x30.
// vtable off_10221558 - an IBase quartet plus a deleting destructor, five slots. (An earlier draft
// of this comment called it the shared three-slot refcount table Texture.h describes. It is not,
// and believing that turns every disposal into an addRef; see the slot dump below the struct.)

struct ShaderMaterial
{
    void*         vptr;          // +0x00  off_10221558
    int           refCount;      // +0x04  ctor 1
    vc71::string  name;          // +0x08
    unsigned int  shaderCombo;   // +0x24  index into dword_1023A9D0 (the combo -> flags table)
    void*         textures[8];   // +0x28  EngineTexture*, slot 0 unused
    unsigned int  textureCount;  // +0x48
};

static_assert(sizeof(ShaderMaterial) == 0x4C, "createMaterial allocates 0x4C");

// Releases through the material's five-slot interface (+0x08), not the texture-wrapper ABI.
int shaderMaterialRelease(ShaderMaterial* material);   // the record's own vtable slot +0x08


// Material draw streams and parameters with a fixed compatibility layout.
// The embedded Ra parameter block occupies 0x150 bytes at +0x2C.

class StaticMeshMaterial
{
public:
    StaticMeshMaterial();
    StaticMeshMaterial(const StaticMeshMaterial& other);
    StaticMeshMaterial& operator=(const StaticMeshMaterial& other);
    void destroyMembers();                                   // (destroy(0) calls it)

    virtual void* destroy(unsigned char flags);        // +0x00
    virtual bool  applyStreams(void* shaderState);     // +0x04
    virtual int   getVertexCount() const;              // +0x08
    virtual int   slot_0x0C() const;                   // +0x0C
    virtual int   getPrimitiveCount() const;           // +0x10
    virtual int   getDword1F0() const;                 // +0x14
    virtual int   getVertexStart() const;              // +0x18
    virtual int   slot_0x1C() const;                   // +0x1C
    virtual int   getIndexStart() const;               // +0x20

    vc71::vector_layout m_vector04;          // +0x04  vector<vc71::vector_layout>
    vc71::vector_layout m_vector14;          // +0x14  vector of 4-byte elements
    unsigned int        m_flags24;           // +0x24
    ShaderMaterial*     m_shaderMaterial;    // +0x28
    unsigned char       m_parameters[0x150]; // +0x2C  the Ra parameter block, 0x10044270
    unsigned int        m_alphaMode;         // +0x17C
    unsigned int        m_vertexStart;       // +0x180
    unsigned int        m_indexStart;        // +0x184
    unsigned int        m_primitiveCount;    // +0x188  index count / 3 (list) or - 2 (strip)
    unsigned int        m_vertexCount;       // +0x18C
    unsigned int        m_u4;                // +0x190
    unsigned int        m_u5u6;              // +0x194
    MeshBounds          m_bounds;            // +0x198
    unsigned int        m_dword1D4;          // +0x1D4  never written by the constructor
    unsigned int        m_dword1D8;          // +0x1D8  never written by the constructor
    void*               m_vegetationVertices;     // +0x1DC  32 B per vertex
    unsigned short*     m_vegetationIndices;      // +0x1E0
    unsigned int        m_vegetationVertexCount;  // +0x1E4
    unsigned int        m_vegetationIndexCount;   // +0x1E8
    unsigned char       m_vegetationNonTiled;     // +0x1EC
    unsigned char       m_pad1ED[3];
    unsigned int        m_dword1F0;          // +0x1F0  finalizeMaterials: the template's +0xAC
    StaticMeshTemplate* m_template;          // +0x1F4
    vc71::map_layout    m_map1F8;            // +0x1F8  40-byte nodes (0x1011B560)
};

static_assert(sizeof(StaticMeshMaterial) == 0x204, "every material walk steps by 516");

// Shared resize/destruction helpers for nested geometry, LOD, and material containers.
void staticMeshFloatVectorResize(vc71::vector_layout& v, unsigned int count, float value);
void staticMeshUnsignedVectorResize(vc71::vector_layout& v, unsigned int count,
                                    unsigned int value);

} // namespace geom
namespace dx9rend { namespace Ra { class RaShader; } }
namespace geom {

// Looks up a shader by feature bits, logging misses and retrying the default mask 0x1000000.
// Returns null if both lookups fail.
::dice::hfe::dx9rend::Ra::RaShader* staticMeshFindShaderBits(unsigned int bits);

// sub_10191CE0 - the feature names of every set bit, concatenated in bit order, from the twelve-
// entry name table.  The miss log's " looked up name: " operand.
void staticMeshShaderBitsToName(vc71::string& out, unsigned int bits);

void staticMeshMaterialsResizeGeoms(vc71::vector_layout& geoms, unsigned int count);
void staticMeshMaterialsResizeLods(vc71::vector_layout& lods, unsigned int count);
void staticMeshMaterialsResize(vc71::vector_layout& materials, unsigned int count);
void staticMeshMaterialsDestroyLods(unsigned char* first, unsigned char* last);
void staticMeshMaterialsDestroyGeoms(unsigned char* first, unsigned char* last);


// Ref-counted material cache owned by StaticMeshRenderer.
// Resolves unqualified texture names using template path followed by the texture prefix.

class StaticMeshStateManager
{
public:
    StaticMeshStateManager();
    void destroyMembers();

    int   addRef()  { return ++m_refCount; }
    int   getRef()  { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) { return iid != 1u ? 0 : this; }

    // 0x101A9270. `version` is the template's file version: the argument slot exists (retn 0x10)
    // but the shipped body never reads it.
    ShaderMaterial* createMaterial(const vc71::string& path,
                                   ::renddx9abi::HostNonlinearStream* stream,
                                   unsigned int version,
                                   StaticMeshTemplate* owner);

    void**              m_vptr;           // +0x00
    int                 m_refCount;       // +0x04
    vc71::vector_layout m_cache;          // +0x08  vector<ShaderMaterial*>
    vc71::string        m_texturePrefix;  // +0x18
    StaticMeshTemplate* m_template;       // +0x34
};

static_assert(sizeof(StaticMeshStateManager) == 0x38,
              "StaticMeshRenderer's constructor pushes 0x38 for this object (PE 0x10122194)");

extern StaticMeshStateManager* g_staticMeshStateManager;   // dword_102651C0

// The two hooks StaticMeshRenderer's constructor and destructor use (PE 0x1012218C..0x101221B5 and
// 0x10121BD8..0x10121BF1). They keep the renderer out of this class's storage decisions.
StaticMeshStateManager* staticMeshStateManagerConstruct(void* storage);
void                    staticMeshStateManagerDestroyMembers(StaticMeshStateManager* self);

// The by-singleton form StaticMeshTemplate::readMaterials calls.
ShaderMaterial* staticMeshStateManagerCreateMaterial(const vc71::string& path,
                                                     ::renddx9abi::HostNonlinearStream* stream,
                                                     unsigned int version,
                                                     StaticMeshTemplate* owner);

// dword_1023A9D0 - shader combo (0..14) -> feature flags. Sixteen entries; 9 and 15 are the
// 0xFFFFFFFF holes sub_101A8D10's ladder cannot produce. StaticMeshStateManager.cpp documents the
// bit meanings.
extern const unsigned int g_shaderComboFlags[16];

// dword_1025B8AC, the `first` pointer of the per-shader-quality mask vector. It is defined in
// StaticMeshStateManager.cpp and filled by the StaticMesh technique initialisation (sub_101921D0).
extern vc71::vector_layout g_staticMeshQualityMasks;

// Indexed by the renderer's mapped shader path, with no bounds check because retail has none.
unsigned int staticMeshQualityMask();

// Owns static mesh LODs, materials, vertex streams and occluder groups.
// Tracks live instances separately; derived reference counting uses +0xC8.
// Preserve the VC7.1 container headers and constructor-untouched fields.
class StaticMeshTemplate : public MeshTemplate
{
public:
    StaticMeshTemplate();
    void destroyMembers();                                          // (then base)

    // sub_10116580, rebase every lod's material index ranges onto the lod's
    // first material's vertex start so the shadow draw can submit a whole lod in one call.  Runs
    // once (the +0x190 byte gates it), only for triangle lists (+0xAC == 4), and only for lods whose
    // materials all fit 16-bit indices from that base.  Called by the StaticMesh shadow draw.
    // [INFERRED] name.
    void mergeShadowIndexRanges();

    // The overridden quartet counts at +0xC8.
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();                                        // 0x100EEA30 -> destroy(1)
    virtual void* queryInterface(unsigned int iid) const;           // +0xC

    virtual unsigned int getClassId() const;
    virtual void* createInstance();
    virtual bool load();
    virtual void setLodDistance(int geom, int lod, float distance);
    virtual float getLodDistance(int geom, int lod);
    virtual void setMaterialValue(int geom, int lod, unsigned char material, float value);
    virtual float getMaterialValue(int geom, int lod, unsigned char material);
    virtual bool isWater() const;
    virtual unsigned int getMemoryUsage(int mask);
    virtual int getPrimitiveCount(int geom, int lod, int material);
    virtual int getVertexCount(int geom, int lod, int material);
    virtual void setDoNotGenerateLightmaps(bool value);
    virtual bool getDoNotGenerateLightmaps() const;
    virtual void setLightmapOverride(bool value);
    virtual bool getLightmapOverride() const;
    virtual void setLightmapSize(int a, int b, int size);
    virtual int getLightmapSize(int a, int b);
    virtual void setCompressVertexData(bool value);
    virtual bool getCompressVertexData() const;
    virtual void setMaxTextureRepeat(int value);
    virtual int getMaxTextureRepeat() const;
    virtual int getMaterialCount(unsigned int lod);
    virtual int getLodPrimitiveCount(unsigned int lod);
    virtual Vec3* decompressPosition(Vec3* out, const Vec3* in);
    virtual void releaseDeviceResources();
    virtual void* destroy(unsigned char flags);
    virtual void mergeMaterials();
    virtual void buildVertexBuffers();
    virtual void buildIndexBuffer();                                // (then base)
    virtual void finalizeMaterials();
    virtual void readLods(renddx9abi::HostNonlinearStream* stream);
    virtual void readMaterials(renddx9abi::HostNonlinearStream* stream);
    virtual int  getOccluderGroupCount();                           // +0xF0
    virtual void* getOccluderGroup(unsigned int index);             // +0xF4

    // Nonvirtual: load() calls it directly (0x1011A63x), it is not in either table. The .occ
    // reader and the only producer of m_occluderGroups - see the body for both records.
    void loadOccluders(renddx9abi::HostStream* stream);

    // io::IConsoleSaveable (table 0x1021C50C): the quartet thunks, then makeScript = 0x10116140.
    virtual bool makeScript(::dice::hfe::io::IStream* stream);

    // data, in shipped order
    int                 m_refCount;             // +0xC8
    vc71::vector_layout m_materials;            // +0xCC  geom -> lod -> StaticMeshMaterial
    vc71::vector_layout m_geoms;                // +0xDC  StaticMeshGeom
    vc71::set_layout    m_instances;            // +0xEC  StaticMesh*
    int                 m_dword0F8;             // +0xF8  never written by the constructor
    MeshBounds          m_bounds;               // +0xFC
    Vec3                m_decompressScale;      // +0x138
    Vec3                m_compressScale;        // +0x144
    float               m_uvDecompress;         // +0x150
    void*               m_vertexStream;         // +0x154
    void*               m_uvStreams[5];         // +0x158
    void*               m_tangentStream;        // +0x16C
    int                 m_streamIndexA;         // +0x170
    int                 m_streamIndexB;         // +0x174
    int                 m_streamIndexC;         // +0x178
    int                 m_extraUvCount;         // +0x17C
    unsigned int        m_declElementCount;     // +0x180
    unsigned char       m_hasTwoTangentSets;    // +0x184
    unsigned char       m_finished;             // +0x185
    unsigned char       m_hasAlphaMaterial;     // +0x186
    unsigned char       m_pad187;
    unsigned int        m_shaderFlagsAll;       // +0x188
    unsigned int        m_shaderFlagsUsed;      // +0x18C
    unsigned char       m_byte190;              // +0x190
    unsigned char       m_skippingLods;         // +0x191
    unsigned char       m_lightmapOverride;     // +0x192
    unsigned char       m_doNotGenerateLightmaps; // +0x193
    int                 m_lightmapSizes[64];    // +0x194  [8][8]
    unsigned char       m_compressVertexData;   // +0x294
    unsigned char       m_pad295[3];
    int                 m_maxTextureRepeat;     // +0x298
    unsigned char       m_isRoad;               // +0x29C
    unsigned char       m_roadNoBlend;          // +0x29D
    unsigned char       m_isVegetation;         // +0x29E
    unsigned char       m_pad29F;
    int                 m_dword2A0;             // +0x2A0  ctor -1
    unsigned char       m_isWater;              // +0x2A4
    unsigned char       m_byte2A5;              // +0x2A5
    unsigned char       m_pad2A6[2];
    float               m_float2A8;             // +0x2A8  ctor 10.0f
    float               m_float2AC;             // +0x2AC  ctor 25.0f
    unsigned char       m_byte2B0;              // +0x2B0
    unsigned char       m_keepVegetationNormals; // +0x2B1
    unsigned char       m_pad2B2[2];
    int                 m_dword2B4;             // +0x2B4
    int                 m_dword2B8;             // +0x2B8
    vc71::vector_layout m_occluderGroups;       // +0x2BC  48-byte elements
};

static_assert(sizeof(StaticMeshTemplate) == 0x2CC, "RefFactory_geom_GeometryTemplate_StaticMesh allocates 0x2CC");

// Accessors used across translation units (SkyDome reads the first geom's first lod's first
// material the way the PE does: m_materials.first -> [0].first -> [0]).
inline StaticMeshMaterial* staticMeshFirstMaterial(StaticMeshTemplate* t)
{
    vc71::vector_layout* geom = reinterpret_cast<vc71::vector_layout*>(t->m_materials.first);
    vc71::vector_layout* lod  = reinterpret_cast<vc71::vector_layout*>(geom->first);
    return reinterpret_cast<StaticMeshMaterial*>(lod->first);
}

// The factory registered under "dice.hfe.geom.GeometryTemplate.StaticMesh" (0x100B064C).
IBase* __fastcall createStaticMeshTemplate(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_STATICMESHTEMPLATE_H
