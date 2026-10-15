// Implementation path attested by BF2 1.50 RendDX9.dll string 0x1021A3C8.
// PE-only renderer class: factory100B05C8, ctor100EA460, table1021A760.
#ifndef BF2_GEOM_BUNDLEDMESHTEMPLATE_H
#define BF2_GEOM_BUNDLEDMESHTEMPLATE_H
#ifdef BF2_IMAGE_RENDDX9

#include "MeshTemplate.h"
#include <cstddef>
#include <map>
#include <set>
#include <vector>

namespace dice { namespace hfe {
namespace dx9rend { namespace Ra { struct RaShader; } }
namespace geom {
class BundledMeshMaterial;
class BundledMesh;

// readLods100E9F50 / instance101049B0 prove the 72/32-byte strides.
struct BundledMeshLod
{
    unsigned int partCount;              // +00
    MeshBounds bounds;                   // +04
    int renderKind;                      // +40, readMaterials defaults3, alpha changes2
    unsigned char hasAlphaTest;          // +44
    unsigned char pad45[3];
};
struct BundledMeshGeom
{
    std::vector<BundledMeshLod> lods;
    std::vector<float> distances;
};
static_assert(sizeof(BundledMeshLod)==72, "Bundled lod stride");

// m_materials holds geom -> lod -> material pointers; m_instances the live instances. The three
// value maps store each value's 32 bits as written, keyed (geom << 16) | (lod << 8) | material.
class BundledMeshTemplate : public MeshTemplate
{
public:
    typedef std::vector<BundledMeshMaterial*> MaterialList;
    typedef std::vector<std::vector<MaterialList> > MaterialTree;
    typedef std::vector<BundledMeshGeom> GeomVector;
    typedef std::map<unsigned int, unsigned int> ValueMap;
    typedef std::set<BundledMesh*> InstanceSet;

    BundledMeshTemplate();
    // The destructor body (then the base's): the deleting slot frees the storage afterwards
    // without running a C++ destructor, so this destroys every member itself.
    void destroyMembers();
    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual unsigned int getClassId() const;
    virtual void setName(const std::string& name);
    virtual void* createInstance();
    virtual bool load();
    virtual void setLodDistance(int geom, int lod, float distance);
    virtual float getLodDistance(int geom, int lod);
    virtual void slot_0x40(int geom, int lod, int material, int valueBits);
    virtual float slot_0x44(int geom, int lod, int material);
    virtual void slot_0x48(int value);
    virtual bool slot_0x4C();
    virtual unsigned int getMemoryUsage(int mask);
    virtual int getPrimitiveCount(int geom, int lod, int material);
    virtual int getVertexCount(int geom, int lod, int material);
    virtual void setCompressVertexData(bool value);
    virtual bool getCompressVertexData() const;
    virtual void slot_0x80(int value);
    virtual bool slot_0x84();
    virtual void setMaxTextureRepeat(int value);
    virtual int getMaxTextureRepeat() const;
    virtual void slot_0x90(int value);
    virtual bool slot_0x94();
    virtual void slot_0x98(int value);
    virtual bool slot_0x9C();
    virtual int getMaterialCount(unsigned int lod);
    virtual int getLodPrimitiveCount(unsigned int lod);
    virtual Vec3* decompressPosition(Vec3* out, const Vec3* in);
    virtual void releaseDeviceResources();
    virtual void* destroy(unsigned char flags);
    virtual void readVertices(renddx9abi::HostNonlinearStream* stream);
    virtual void mergeMaterials();
    virtual void buildVertexBuffers();
    virtual void finalizeMaterials();
    virtual void readLods(renddx9abi::HostNonlinearStream* stream);
    virtual void readMaterials(renddx9abi::HostNonlinearStream* stream);
    virtual void setSpecularStaticGloss(int geom, int lod, unsigned char material, float value); // +F0,100E91C0
    virtual float getSpecularStaticGloss(int geom, int lod, unsigned char material); // +F4,100E9240
    virtual void setMaterialValue100(int geom, int lod, unsigned char material, int value); // +F8,100E92A0
    virtual int getMaterialValue100(int geom, int lod, unsigned char material); // +FC,100E9320
    virtual bool makeScript(::dice::hfe::io::IStream* stream);

    int m_refCount;                       // +0C8
    MaterialTree m_materials;              // +0CC
    InstanceSet m_instances;               // +0DC
    ValueMap m_materialValues0E8;          // +0E8 packed key -> float bits
    ValueMap m_map0F4;                     // +0F4 packed key -> static gloss float bits
    ValueMap m_map100;                     // +100 packed key -> int, draw material+94
    unsigned int m_dword10C;              // +10C
    GeomVector m_geoms;                    // +110
    unsigned int m_maxPartCount;          // +120
    unsigned char m_hasOpaqueMaterials;   // +124 readMaterials kind!=1
    unsigned char m_byte125;              // +125 slots48/4C
    unsigned char m_pad126[2];
    MeshBounds* m_partBounds[26];          // +128
    unsigned char m_hasEnvironmentMap;    // +190
    unsigned char m_compressVertexData;   // +191
    unsigned char m_byte192;              // +192 slots80/84
    unsigned char m_pad193;
    int m_maxTextureRepeat;               // +194
    unsigned char m_byte198;              // +198 slots90/94
    unsigned char m_byte199;              // +199 slots98/9C
    unsigned char m_pad19A[2];
    MeshBounds m_bounds;                  // +19C
    Vec3 m_compressScale;                 // +1D8
    Vec3 m_decompressScale;               // +1E4
    float m_uvDecompress;                 // +1F0
    unsigned int m_dword1F4;              // +1F4 ctor untouched
    unsigned int m_liveInstanceCount;     // +1F8
};

void bundledMeshInitTechniques();
dx9rend::Ra::RaShader* bundledMeshShaderForBits(unsigned int bits);
unsigned int bundledMeshQualityMask();                      // draw's unchecked quality indexing
unsigned int bundledMeshQualityMask(unsigned int index);
IBase* __fastcall createBundledMeshTemplate(unsigned int iid, IBase* outer);

// Instance-owned bodies, published by BundledMesh.cpp.
void* bundledMeshConstruct(void* storage, BundledMeshTemplate* templ);
void bundledMeshInitTemplate(void* instance);
void* bundledMeshDestroyFlags(void* instance, unsigned char flags);

} } }
#endif
#endif
