// Skeletal geometry instance used by soldiers and their first-person weapons.
// [UNRESOLVED] original source path; this filename is [INFERRED].
// Preserves the IGeometry virtual surface. An FOV of -1 selects the view's normal projection;
// other values select the first-person override. A missing transform uses the shared identity.

#ifdef BF2_IMAGE_RENDDX9
#ifndef BF2_GEOM_SKINNEDMESH_H
#define BF2_GEOM_SKINNEDMESH_H

#include <stddef.h>   // offsetof - the layout assertions below
#include <string>
#include "../Math/Mat4.h"
#include "../RendDX9/Ra/RaShader.h"   // RaObjectParameters, RaLightRecord
#include "GeometryInstanceData.h"

#include <vector>

namespace dice {
namespace hfe {
namespace world { class LightSource; }
namespace geom {


// The host owns the used-bone output vector: the toolset's std::vector; the interface stays a
// pointer.
typedef std::vector<int> HostIntVector;

class SkinnedMeshTemplate;
struct SkinnedMeshLod;
struct SkinnedMeshMaterial;

// The palette geometry is shipped, not chosen: the constructor allocates four runs of twenty-SIX
// Mat4 pointers, one 0x40 block each, and sets every matrix to identity twice over - once inside
// the allocation guard and once after the store. Twenty-six is BF2's per-batch bone limit.
enum { kSkinnedMeshPalettes = 4, kSkinnedMeshBonesPerPalette = 26 };

class SkinnedMesh : public GeometryInstanceData
{
public:
    // 0x101A7650. `objectTemplate` is the SkinnedMeshTemplate that made this; createInstance
    // (0x1010F270) allocates 0x598 and calls straight here.
    explicit SkinnedMesh(void* objectTemplate);

    // the IBase quartet, +0x00..+0x0C
    // The first three are NOT StaticMesh's. StaticMesh counts at +0x34 and uses 0x10103C00/10/20;
    // this class counts at +0x30 and uses 0x101071D0/E0/F0. Different offsets, different bodies -
    // which is exactly the sanity check that says the two layouts really do diverge here.
    int   addRef();                                          // +0x00  (shared)
    int   getRef() const;                                    // +0x04  (shared)
    int   release();                                         // +0x08  (shared)
    void* queryInterface(unsigned int iid);                  // +0x0C  0x101A89E0  OWN

    // the FOV override, +0x10 / +0x14
    // 0x101A8A90 returns the value it stored - the shipped body really does hand the argument back,
    // which is why Soldier::hackSetFov assigns from it.
    void setFov(float fov);                                  // +0x10  0x101A8A90  OWN
    float getFov() const;                                    // +0x14  0x101A8AA0  OWN

    // +0x24 / +0x28
    // The setter/getter pair for +0x33C. [UNRESOLVED] role; the draw chases the stored pointer's
    // +0x2F8, +0x338 and +0x3B8 as three Mat4s, so it is an object with a matrix triple.
    void* setDword33C(void* value);                          // +0x24  0x101A8A10  OWN
    void* getDword33C() const;                               // +0x28  0x101A8A20  OWN

    // identity and ownership
    void* getTemplate() const;                               // +0x38  0x101A8A30  OWN
    const void* getLocalBounds() const;                      // +0x40  0x101A8A40  OWN
    float getBoundingRadius() const;                         // +0x44  0x101A68E0  OWN
    const Mat4* getGeometryTransformation() const;           // +0x48  0x101A67C0  OWN
    void  setGeometryTransformation(const Mat4* transform);  // +0x50  0x101A67D0  OWN

    // Borrows the skeleton matrix array. Visible-part gathering omits the excluded part;
    // its palette uses a shared constant matrix. An excluded id of -1 suppresses nothing.
    void setBoneTransforms(const Mat4* bones, unsigned int count); // +0x54  0x101A8A50  OWN
    void gatherVisibleParts(HostIntVector* out);             // +0x58  0x101A7570  OWN
    void setExcludedPart(int part, bool enable);             // +0x60  0x101A67E0  OWN

    // geom / lod selection, +0x6C .. +0x80
    // The same five slots StaticMesh.h documents, over the template's two parallel vectors.
    // setGeomIndex does NOT clamp the lod the way StaticMesh's does - it resets it to 0.
    // setLodIndex's out-of-range arm re-enters through its OWN vtable slot with -1. That is a
    //   virtual self-call in the shipped code, not a direct one, and it is reproduced as such.
    void setGeomIndex(int geom);                             // +0x6C  0x101A7010  OWN
    int  getGeomIndex() const;                               // +0x70  0x101A6810  OWN
    int  getGeomCount() const;                               // +0x74  0x101A6CE0  OWN
    void* setLodIndex(int lod);                              // +0x78  0x101A7060  OWN
    int  getLodIndex() const;                                // +0x7C  0x101A8A70  OWN
    int  getLodCount() const;                                // +0x80  0x101A7140  OWN

    // the owning object, +0x9C .. +0xB0
    // world::setGeometry calls setOwnerObject right after setComponent succeeds - the same
    // sequence StaticMesh.h records for its +0x9C.
    void  setOwnerObject(void* object);                      // +0x9C  0x101A6930  OWN
    void* getOwnerObject() const;                            // +0xA0  0x101A8A80  OWN
    void* refreshRootObject();                               // +0xA4  0x101A6940  OWN
    unsigned char refreshAndGetByte334();                    // +0xA8  0x101A6980  OWN

    // Shadow-caster frame stamp. Set only when the owner is its own root;
    // reads delegate to the root's geometry when the owner is parented.
    void  setShadowCasterFrame(void* frame);                 // +0xAC  0x101A69A0  OWN
    void* getShadowCasterFrame();                            // +0xB0  0x101A69D0  OWN

    // +0xF4, 0x101A8AB0 - `*(this + 0x510) = value`. +0x510 lands inside the Ra block, at the
    // `Transparency` holder; the draw's entry gate reads it as a float and refuses the whole body
    // below 0.00001f, then flags "is translucent" below 1.0f.
    void setTransparency(float alpha);                       // +0xF4  0x101A8AB0  OWN

    // the two private per-frame helpers, both in this TU
    // Re-selects the LOD once per frame, stamping +0x350. Called by the main draw, the
    // shadow draw AND gatherVisibleParts. It does nothing when the lod was chosen explicitly
    // (+0x44), which is what makes setLodIndex's explicit flag load-bearing.
    void updateLodSelection();
    // Rebuilds the four bone palettes once per frame, stamping +0x34C. Each part's
    // matrix is `partTransform * boneTransforms[partId]`; the suppressed part and the
    // no-skeleton case take shared constant matrices instead.
    void updateBonePalettes();
    // Re-point +0x34 / +0x38 at the current geom/lod out of the template's two
    // vectors. SkinnedMeshTemplate::load calls it on every live instance once the file is in, which
    // is what turns a selection made against empty vectors into a live one.
    // It has NO guards - no loaded test, no bounds test. The set only ever holds real instances
    // and load() only calls it after the read, which is the whole of the shipped protection.
    void* refreshSelection();
    // Three ray probes whose results land in +0x48 / +0x4C / +0x50.
    // The body is a named stop.
    void updateHeightOverTerrain();                          // 0x101A6A20  frontier
    // The per-material shader-permutation mask the main draw stamps into every queue
    // entry. A __thiscall with three stack arguments - the draw's decompilation drops the
    // receiver, and the standalone view restores it (it dispatches +0xA8 and +0xB0).
    unsigned int buildShaderDefineMask(const SkinnedMeshMaterial* material, bool alphaPass,
                                       unsigned int quality);

    // the draws
    // +0x18, the main draw.
    // three stack arguments. The shipped body is `__userpurge(ecx, ebx, edi, a4, a5, a6)`; the
    // ebx/edi pair is decompiler noise, the three stack slots are real, and a thunk that declared
    // two would corrupt the stack on every retail call (the callee cleans).
    void draw(void*, void*, void*);                          // +0x18
    // Draws the shadow pass; only caster and alphaPass are consumed.
    void drawShadowMap(void* caster, unsigned int, unsigned int, unsigned int,
                       bool alphaPass, unsigned int);        // +0x20
    // the real destructor, reached through the +0x34 scalar deleting destructor.
    // NOT read. It has 104 Mat4 allocations and four vectors to undo, so it is not guessable.
    ~SkinnedMesh();                                          // 0x101A7A10        frontier

    // data, in shipped order; the `+0xNN` comments are the retail offsets
    // +0x000..+0x02F: inherited GeometryInstanceData.
    int                 m_refCount;             // +0x030  ctor 1  StaticMesh keeps its at +0x34

    // +0x034 / +0x038: the current selection, rewritten together by +0x6C and +0x78: the
    // template's 132-byte lod record and that lod's vector of 456-byte material records.
    SkinnedMeshLod*     m_lodRecord;            // +0x034  ctor 0, then the template gate
    std::vector<SkinnedMeshMaterial>* m_lodMaterials;   // +0x038  ctor 0, then the template gate
    unsigned int        m_geomIndex;            // +0x03C  ctor 0; slots +0x6C / +0x70
    unsigned int        m_lodIndex;             // +0x040  ctor 0; slots +0x78 / +0x7C
    unsigned char       m_lodExplicit;          // +0x044  ctor 0; +0x78 sets it, -1 clears it
    unsigned char       m_pad045[3];            // +0x045
    // [INFERRED] Height-probe results consumed by draw: normalized height, hemi-range factor,
    // and a hit flag that suppresses ambient scaling. updateHeightOverTerrain writes them.
    unsigned int        m_dword48;              // +0x048  ctor 0
    unsigned int        m_dword4C;              // +0x04C  ctor 0
    unsigned char       m_byte50;               // +0x050  ctor 0
    unsigned char       m_pad051[3];            // +0x051
    void*               m_heightmapCluster;     // +0x054  ctor 0, and the constructor's last store
    SkinnedMeshTemplate* m_template;            // +0x058  the constructor's argument
    const Mat4*         m_transform;            // +0x05C  ctor 0; slots +0x48 / +0x50

    Mat4                m_matrix60;             // +0x060  ctor identity
    Mat4                m_matrixA0;             // +0x0A0  ctor identity
    Mat4                m_matrixE0;             // +0x0E0  ctor identity
    Mat4                m_matrix120;            // +0x120  ctor identity

    unsigned int        m_dword160;             // +0x160  NOT written by the constructor
    // +0x164 is a vector, and that is why the constructor "skips" +0x164 and zeroes exactly
    // +0x168/+0x16C/+0x170 in retail. The draw passes `this + 0x164` to XpLightManager's gather and
    // then walks (last - first) >> 2 pointers out of it, so the element type is world::LightSource*.
    std::vector<world::LightSource*> m_lights;  // +0x164
    // The skeleton palette the caller hands in. updateBonePalettes indexes it with
    // `partRecord[0] << 6`, a stride of sizeof(Mat4) - which is what proves the type.
    const Mat4*         m_boneTransforms;       // +0x174  ctor 0; slot +0x54
    unsigned int        m_boneTransformCount;   // +0x178  ctor 0; slot +0x54
    int                 m_excludedPart;         // +0x17C  ctor -1; slots +0x58 / +0x60

    // Four bone palettes of twenty-six matrices, 104 pointers in all.
    Mat4*               m_bonePalettes[kSkinnedMeshPalettes][kSkinnedMeshBonesPerPalette];
                                                // +0x180 .. +0x31C
    unsigned int        m_boneCounts[kSkinnedMeshPalettes];  // +0x320  ctor {0,0,0,0}

    void*               m_ownerObject;          // +0x330  ctor 0; slots +0x9C / +0xA0
    unsigned char       m_byte334;              // +0x334  ctor 0; slots +0xA4 / +0xA8
    unsigned char       m_pad335[3];            // +0x335
    void*               m_shadowCasterFrame;    // +0x338  ctor 0; slots +0xAC / +0xB0
    void*               m_dword33C;             // +0x33C  ctor 0; slots +0x24 / +0x28
    float               m_fov;                  // +0x340  ctor -1.0f; slots +0x10 / +0x14
    float               m_distanceToCamera;     // +0x344  ctor 0; the draw computes it
    unsigned int        m_dword348;             // +0x348  NOT written; the draw uses its low byte
    // The two per-frame one-shot stamps, each holding the renderer's frame ordinal. They are what
    // make updateBonePalettes and updateLodSelection safe to call from three places apiece.
    unsigned int        m_paletteFrameStamp;    // +0x34C  ctor 0; updateBonePalettes
    unsigned int        m_lodFrameStamp;        // +0x350  ctor 0; updateLodSelection
    float               m_float354;             // +0x354  ctor 1.0f; the draw eases it toward the
                                                //         light manager's value each frame
    double              m_double358;            // +0x358  ctor 0.0; the draw's last-update time

    // +0x360 .. +0x583 - raObjectParametersConstruct (0x100458E0), the same 0x224-byte block
    // StaticMesh embeds at its +0x38 and Terrain.cpp already drives. Its `Transparency` holder is
    // the +0x510 that slot +0xF4 writes.
    ::dice::hfe::dx9rend::Ra::RaObjectParameters m_objectParameters;   // +0x360

    // +0x584: the draw sizes this to one 68-byte record per gathered light.
    std::vector< ::dice::hfe::dx9rend::Ra::RaLightRecord> m_lightRecords;   // +0x584
    unsigned int        m_dword594;             // +0x594  NOT written by the constructor
};

#if defined(_M_IX86)

// Check individual offsets because total size alone cannot detect member drift.

static_assert(offsetof(SkinnedMesh, m_vptr) == 0x000, "m_vptr");
static_assert(offsetof(SkinnedMesh, m_name) == 0x004, "m_name");
static_assert(sizeof(((SkinnedMesh*)0)->m_bonePalettes) == 0x1A0, "104 Mat4* = 0x180..0x31F");
#endif

// the SkinnedMesh technique initialisation. It is what fills the per-quality
// shader mask vector, and SkinnedMeshTemplate::readMaterials calls it first thing. Declared here
// because its caller lives in the other translation unit.
void skinnedMeshInitTechniques();

// 0x1010FD00's one-shot - creates the five shadow-state globals at ds:1025B230..240 (the Effect
// "Shaders/SkinnedMesh" plus vpLightMat / vpLightTrapezMat / TEXLAYER0 / BoneArray) on the first
// call and addRefs them on every later one. SkinnedMeshTemplate::readMaterials calls it; the
// storage lives beside the draw that reads it.
void skinnedMeshInitShadowState();

// 0x101A7650, called on storage the template already allocated.
SkinnedMesh* skinnedMeshConstruct(void* storage, void* objectTemplate);

// SkinnedMeshRenderer (SkinnedMeshRenderer.cpp), as the draw reaches it through the published
// singleton. sub_10123A40 answers the renderer's mode word, which the draw compares against 2.
// sub_10123A50 / sub_10123A90 queue a mesh by its camera distance into the renderer's first and
// third distance-ordered queues; both return at once when the renderer or the mesh is null.
int  skinnedMeshRendererMode();
void skinnedMeshRendererSubmitA(SkinnedMesh* mesh);
void skinnedMeshRendererSubmitB(SkinnedMesh* mesh);

// Primary compatibility table. Keep each thunk's declared stack-argument count.
extern void* const g_skinnedMeshVtable[62];

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_SKINNEDMESH_H
#endif // BF2_IMAGE_RENDDX9
