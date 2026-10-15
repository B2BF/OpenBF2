// Path: [INFERRED] header beside MeshParticleMeshTemplate.cpp, attested at RendDX9 0x1021CBE0.
// Mesh particle and legacy effect template, recovered from RendDX9.dll 1.50 (PE only).
// Loads .bundledMesh files, including the base trailer; there are no LOD distances.
// Retail layout: 0x164 bytes; primary table 0x1021CC68 has 61 slots.
// Slot +0xF0's name remains [UNRESOLVED]; preserve its two-argument declaration.

#ifndef GEOM_PARTICLESYSTEM_MESHPARTICLEMESH_MESHPARTICLEMESHTEMPLATE_H
#define GEOM_PARTICLESYSTEM_MESHPARTICLEMESH_MESHPARTICLEMESHTEMPLATE_H

#ifdef BF2_IMAGE_RENDDX9

#include "../../MeshTemplate.h"
#include <cstddef>
#include <set>
#include <vector>

namespace dice {
namespace hfe {
namespace dx9rend { class Effect; class ShaderParameter; }
namespace geom {

class BundledMeshState;
class MeshParticleMesh;


// The 72-byte lod record readLods builds in m_geoms. The same shape BundledMeshTemplate.h calls
// BundledMeshLod (partCount +0, bounds +4, renderKind +0x40, hasAlphaTest +0x44), read from this
// class's own bodies: readLods fills +0x00/+0x04, readMaterials writes +0x40/+0x44, and the instance
// reads +0x00 (the renderer's batch size), +0x04 (bounds) and +0x40 (getGeometryType).
// Kept as its own type: dice's source for this class is a separate file and nothing here proves
// the two classes shared the declaration. [INFERRED] field names, from the Bundled sibling.

struct MeshParticleMeshLod
{
    unsigned int  partCount;     // +0x00  read after the bounds; 26 for mp_spark/mp_rockshard_l
    MeshBounds    bounds;        // +0x04  0x3C bytes; +0x24 is bounds.dirty
    int           renderKind;    // +0x40  readMaterials: 3, or 2 once a material has alphaMode 1
    unsigned char hasAlphaTest;  // +0x44  readMaterials: 1 once a material has alphaMode 2
    unsigned char pad45[3];      // +0x45
};


// The 24-byte material record readMaterials builds in m_materials[geom][lod]. [INFERRED] names; the
// roles are proven by the readers and the draw (0x101AFDA0): the draw passes +0x10/+0x14/+0x08/+0x0C
// to DrawIndexedPrimitive as primCount/numVertices/baseVertex/startIndex, and binds state's first
// texture. The destructor releases `state` through its IBase +0x08.

struct MeshParticleMeshMaterial
{
    BundledMeshState* state;          // +0x00  BundledMeshStateManager::read, refcounted
    int               alphaMode;      // +0x04  0 opaque, 1 transparent (never drawn), 2 alpha test
    unsigned int      vertexStart;    // +0x08  the draw's baseVertex
    unsigned int      indexStart;     // +0x0C
    unsigned int      primitiveCount; // +0x10  derived: list -> indices/3, strip -> indices-2
    unsigned int      vertexCount;    // +0x14
};

static_assert(sizeof(MeshParticleMeshLod) == 72, "readLods / the instance stride by 72 (0x38E38E39 >> 4)");
static_assert(offsetof(MeshParticleMeshLod, renderKind) == 0x40, "readMaterials writes lod +0x40");
static_assert(offsetof(MeshParticleMeshLod, hasAlphaTest) == 0x44, "readMaterials writes lod +0x44");
// The rebuild keeps the records in std::vectors and indexes them by type; nothing here divides by a
// literal size, so the retail size is x86 evidence only (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(MeshParticleMeshMaterial) == 24, "the destructor and the draw divide by 24");
#endif


// Shared mesh-particle effect state, initialized on demand by material loading.
// The last template releases these references; renderer draws borrow them.

struct MeshParticleMeshEffectState
{
    int                       templateCount;       // ds:1025BEDC  live MeshParticleMeshTemplates
    dx9rend::Effect*          effect;              // ds:1025BEE0  "Shaders/MeshParticleMesh"
    dx9rend::ShaderParameter* worldViewProjection; // ds:1025BEE4  "WorldViewProjection"
    dx9rend::ShaderParameter* unused08;            // ds:1025BEE8  [UNRESOLVED] never assigned
    dx9rend::ShaderParameter* color1;              // ds:1025BEEC  "COLOR1"
    dx9rend::ShaderParameter* color2;              // ds:1025BEF0  "COLOR2"
    dx9rend::ShaderParameter* colorBlendGraph;     // ds:1025BEF4  "COLORBLENDGRAPH"
    dx9rend::ShaderParameter* fogRange;            // ds:1025BEF8  "fogRange"
    dx9rend::ShaderParameter* fogColor;            // ds:1025BEFC  "fogColor"
    dx9rend::ShaderParameter* effectSunColor;      // ds:1025BF00  "EffectSunColor"
    dx9rend::ShaderParameter* effectShadowColor;   // ds:1025BF04  "EffectShadowColor"
    dx9rend::ShaderParameter* texture0;            // ds:1025BF08  "Texture0"
    dx9rend::ShaderParameter* texture1;            // ds:1025BF0C  "Texture1"
    dx9rend::ShaderParameter* oneBoneSkinning;     // ds:1025BF10  "matONEBONESKINNING"
};

#if defined(_M_IX86)
static_assert(offsetof(MeshParticleMeshEffectState, effect) == 0x1025BEE0 - 0x1025BEDC, "effect");
static_assert(offsetof(MeshParticleMeshEffectState, oneBoneSkinning) == 0x1025BF10 - 0x1025BEDC,
              "the fourteen statics keep their retail spacing");
#endif

extern MeshParticleMeshEffectState g_meshParticleMeshEffectState;

// Owns per-geometry LOD/material vectors and tracks the instances it creates.
// queryInterface shares RoadCompiled's body and does not expose IConsoleSaveable.
// Inherits readTrailer: .bundledMesh includes the base trailer dword.
class MeshParticleMeshTemplate : public MeshTemplate
{
public:
    typedef std::vector<MeshParticleMeshMaterial>           LodMaterials;   // one lod's materials
    typedef std::vector<std::vector<LodMaterials> >          MaterialTable;  // geom -> lod -> material
    typedef std::vector<std::vector<MeshParticleMeshLod> >   LodTable;       // geom -> lod
    typedef std::set<MeshParticleMesh*>                      InstanceSet;

    MeshParticleMeshTemplate();
    // The destructor body: the deleting slot frees the storage afterwards without running a C++
    // destructor, so this destroys this class's members itself, in the shipped order.
    void destroyMembers();                                          // (the dtor body)

    // The overridden IBase trio counts at +0xC8 - the same three bodies StaticMeshTemplate,
    // SkinnedMeshTemplate and BundledMeshTemplate use. queryInterface is a different body.
    virtual void  addRef();                                         // +0x00
    virtual int   getRef();                                         // +0x04
    virtual int   release();                                        // +0x08
    virtual void* queryInterface(unsigned int iid) const;           // +0x0C

    virtual unsigned int getClassId() const;                        // +0x10
    virtual void  setName(const std::string& name);                // +0x14
    virtual void* createInstance();                                 // +0x24
    virtual bool  load();                                           // +0x2C
    virtual void* destroy(unsigned char flags);                     // +0xB0
    virtual void  readLods(::renddx9abi::HostNonlinearStream* stream);      // +0xD8
    virtual void  readMaterials(::renddx9abi::HostNonlinearStream* stream); // +0xDC

    // +0xF0, THE class'S ONE NEW virtual, appended after MeshTemplate's sixty. MSVC
    // appends a new virtual of the primary base chain at the end of the primary table, which is
    // exactly +0xF0 (BundledMeshTemplate's own +0xF0..+0xFC prove the mechanism in this tree).
    // [UNRESOLVED] name and value type: the body is an untyped dword store. No recovered caller.
    virtual void  setDwordF8(int index, unsigned int value);        // +0xF0

    // data, in shipped order; the `+0xNN` comments are the retail offsets
    int                 m_refCount;          // +0x0C8
    MaterialTable       m_materials;         // +0x0CC  geom -> lod -> MeshParticleMeshMaterial
    InstanceSet         m_instances;         // +0x0DC  MeshParticleMesh*
    LodTable            m_geoms;             // +0x0E8  geom -> MeshParticleMeshLod
    unsigned int        m_dwordF8[26];       // +0x0F8  [UNRESOLVED], constructor-untouched
    unsigned char       m_isDecal;           // +0x160  [INFERRED] name
    unsigned char       m_pad161[3];         // +0x161
};

// the factory registered under "dice.hfe.geom.GeometryTemplate.MeshParticleMesh".
IBase* __fastcall createMeshParticleMeshTemplate(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
#endif // GEOM_PARTICLESYSTEM_MESHPARTICLEMESH_MESHPARTICLEMESHTEMPLATE_H
