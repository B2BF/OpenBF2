// Static geometry instance with an ILightSampleable secondary interface at +0x30.
// The opaque 0x224-byte block at +0x38 stores per-object shader parameters.


#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_STATICMESH_H
#define BF2_GEOM_STATICMESH_H

#include <stddef.h>   // offsetof - the layout assertions below
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../Core/IBase.h"
#include "GeometryInstanceData.h"
#include "../Math/Mat4.h"
#include "../RendDX9/Ra/RaShader.h"    // RaObjectParameters - the block embedded at +0x38

namespace dice { namespace hfe { class Frustum; } }

namespace dice {
namespace hfe {
namespace geom {

// The VC7.1 std::map header the constructor builds at +0x260: an allocator word, the sentinel, and
// the size. Same shape ObjectDrawer carries for its two trees, and for the same reason - nothing
// recovered so far reads through it, so it stays an opaque header rather than an invented map.
// Its node is forty bytes (sub_100FFB50 allocates 0x28): left/parent/right, a 24-byte value,
// then the colour byte at +0x24 and the nil flag at +0x25.
struct StaticMeshMapHeader
{
    void*        alloc;     // +0x00  never written by the constructor
    void*        head;      // +0x04  the sentinel; all three of ITS links point at itself
    unsigned int size;      // +0x08  0
};

class StaticMesh : public GeometryInstanceData
{
public:
    // 0x100FDC20, 209 B of stores. `objectTemplate` is the StaticMeshTemplate that made this.
    explicit StaticMesh(void* objectTemplate);

    // the real destructor. Slot +0x34 is the MSVC scalar deleting destructor
    // (0x10103C80), which calls this and then frees the block when its flag bit 0 is set.
    ~StaticMesh();

    // the IBase quartet, primary slots +0x00..+0x0C
    // addRef returns THE NEW count (0x10103C00 is `add [ecx+34h],1` then `mov eax,[ecx+34h]`),
    // it is not void. release likewise returns the remaining count.
    int   addRef();                                          // +0x00
    int   getRef() const;                                    // +0x04
    int   release();                                         // +0x08
    void* queryInterface(unsigned int iid);                  // +0x0C

    // +0x50, 0x100FF2B0. Stores the transform, then a ONE-shot block behind +0x348.
    void  setGeometryTransformation(const Mat4* transform);

    // +0xCC, the shared const-0 stub this tree already documents three times over.
    // This is the "next geometry in the chain" slot Object::updateGeometry and ObjectDrawer's
    // deferred pass both walk; a StaticMesh never chains, so the walk stops after one step.
    int   getNextGeometry();

    // +0xF4, 0x10103C40 - `*(this + 0x1E8) = value`. +0x1E8 lands inside the Ra block at
    // 0x1E8 - 0x38 = 0x1B0, which is RaObjectParameters::transparency.value - and the draw's very
    // first early-out is `if (transparency < 1e-5) return;`. So this is the fade.
    void  setFade(float value);

    // +0x9C, 0x100FB2A0.  world::setGeometry calls this right after setComponent succeeds.
    void  setOwnerObject(void* object);

    // Caches and returns the owner's root parent.
    void* refreshRootObject();

    // the read accessors
    // Eight one-line slots, each read from its own instructions so the arity is certain - which is
    // the only thing that makes them safe to write rather than stop (see the .cpp's callee-cleanup
    // note).  The two index fields they expose, +0x314 and +0x318, are the geom and lod that
    // setGeom/setLod drive and that the post-load reads back.
    int   getGeomIndex() const;                              // +0x70
    int   getLodIndex() const;                               // +0x7C
    int   getDword31C() const;                               // +0x88
    void  updateFlags(unsigned int set, unsigned int clear); // +0xC0
    unsigned int getFlags() const;                           // +0xC4
    void  stampFrameOrdinal();                               // +0xF0
    void  setDword350(void* value);                          // +0xAC
    void* getDword350() const;                               // +0xB0

    // Returns the owning geometry template.
    void* getTemplate() const;

    // Returns the selected LOD's bounds; requires a selected LOD.
    // The ABI returns a reference, represented here by a pointer to the unmodeled type.
    const void* getLocalBounds() const;

    // +0xA4, the root parent refreshRootObject() memoised at +0x334.
    void* getRootObject() const;

    // +0x10, stores four bytes into +0x308. [INFERRED] float, from the
    // constructor's -1.0f; the shipped move is untyped.
    void  setFloat308(float value);

    // +0x24 / +0x28, the setter and getter for +0x34C. [INFERRED]
    // names; nothing recovered so far reads the field.
    void  setDword34C(unsigned int value);
    unsigned int getDword34C() const;

    // +0x2C, 0x10103BC0 - `mov eax, 3; retn`. A per-class constant.
    // [INFERRED] as a kind discriminator: XpLightManager's three reflection-type constants are
    // 0x870CA651 Bundled / ...52 Skinned / ...53 StaticMesh, and StaticMesh answering 3 fits
    // that 1/2/3 ordering. NOT confirmed against the other two meshes' vtables, so the
    // enumeration itself stays unrecovered - only the constant is evidence.
    int   getGeometryType() const;

    // Access flag bit 1 through virtual updateFlags/getFlags; only enable's low byte matters.
    void  setFlagBit1(unsigned char enable);
    bool  getFlagBit1() const;

    // +0xE0 / +0xE4, the setter and getter for +0x28C.
    // This is the field the constructor pointedly does NOT write; it is populated
    // through this slot during the template'S load, which is why it was reached before any
    // frame was drawn.
    void  setDword28C(void* value);
    void* getDword28C() const;

    // +0x80, the number of LODs the current geom has.
    // This is the slot that settled the ArmorGLComp/Kit vs SingleFireComp contradiction
    // (retirements RW-139): it counts 104-byte lod records, the same stride setGeom and setLod
    // index by, which is what proved PE +0x80 - not +0x78 - is getLodCount.
    int   getLodCount() const;

    // +0x74, how many geoms the template has. Not a lod count: it counts
    // 32-byte geom records where getLodCount counts 104-byte lod records. That distinction is
    // what RW-140 turned on - ArmorGLComp, Kit and Object::SetActiveLod all want this one.
    int   getGeomCount() const;

    // +0x6C, select the geom, clamping the current lod to that geom's lod count.
    void  setGeom(int geom);

    // +0x78, select the lod.  -1 means "back to lod 0, and clear the explicit flag".
    void  setLod(int lod);

    // sub_100FC190 - the per-instance post-load. Not a vtable slot: the template calls it
    // directly on each live instance once its own load() finishes, and the constructor calls
    // it too when the template was already loaded.
    void  postLoad();

    // THE draw and the rest of the table,
    // +0x18, the dispatcher: the expansion-pack arm (sub_100FE130) when
    // Renderer::isExpansionPack(), else the vanilla arm.  ObjectDrawer's deferred pass calls it
    // with (renderView, occluder, 1); the vanilla arm reads none of the three.
    void  draw(void* renderView, void* occluder, int flag);
    // sub_100FC7D0, 3,494 B - the vanilla BF2 1.50 arm.  Camera distance, lod selection, the
    // lightmap, the per-instance parameter refresh, the point-light gather, then one render entry
    // per material (plus one per gathered light) into the Ra render queues.
    void  drawVanilla();

    // sub_100FB3B0 - the per-draw view prelude: the +0x308 fov override pushed through the
    // current render view, and the pending world-inverse refresh.  [INFERRED] name.
    void  prepareView();
    // sub_100FBCB0 - the per-draw RaObjectParameters refresh: world, world*viewProjection,
    // the camera in object space, the world up axis in object space, and the two shadow
    // matrices when this instance is the frame's shadow caster.  [INFERRED] name.
    void  updateObjectParameters();
    // sub_10198BA0 - the lightmap loader: the (geom, lod) entry of the +0x260 map, filled from
    // the level's lightmap atlas (or a loose .dds in editor mode) the first time it is empty.
    void  loadLightmap();
    // sub_101988B0 - the per-vertex ".lights" light-index lists behind StaticMeshRenderer's
    // +0x61 flag.  [INFERRED] name.
    void  loadVertexLightLists();
    // sub_100FB0B0 - the feature bits one material draws with: the shader combo's flags, the
    // lightmap bit, the shadow-caster bit and the alpha-test bit.  [INFERRED] name.
    unsigned int materialShaderBits(void* material, bool pointLightPass);

    // +0x14, 0x10082D10 - `fld -1.0; retn`.  A per-class constant; meaning [UNRESOLVED].
    float slot14ReturnsMinusOne() const;
    // +0x1C, the silhouette draw ("StaticMeshSilhouette"), attested to
    // StaticMeshLightmapGeneration.cpp:278.  Two stack arguments: the first is never read; a
    // non-zero second takes the "MattiasH removed me" arm, which only logs.  With the renderer's
    // +0x63 flag off (retail) or an alpha material in the template it defers to the slow walk.
    void  drawSilhouette(void* unused, int legacyArm);
    // sub_10198DF0 - the slow silhouette ("StaticMeshSilhouetteSlow"): every visible material
    // through the shadow effect's lightmapGeneration technique, alpha-tested ones through the
    // AlphaTest variant.  Line 207 of the same file holds its own "removed" arm.
    void  drawSilhouetteSlow(void* unused, int legacyArm);
    // +0x20, the shadow-MAP draw ("StaticMeshDrawShadowMap").  Six stack arguments,
    // only the first read: the caster record whose light matrix is at +0x1F8, or null for the
    // caster this instance was registered with (+0x34C, matrices at +0x238/+0x278) when that
    // registration is this frame's.
    void  drawShadowMap(void* caster, unsigned int, unsigned int, unsigned int, unsigned int,
                        unsigned int);
    // sub_100FB440 - is this material's box inside the frustum (the current view's when null)?
    // True when the renderer's +0x60 cull flag is off; a rejection bumps the renderer's +0x5C.
    bool  materialVisible(void* material, const Frustum* frustum);
    // +0x30, 0x1005DA00 - `xor al,al; retn 4`: false, one argument.
    bool  slot30ReturnsFalse(int) const;
    // +0x3C / +0xC8, 0x10088060 - `xor eax,eax; retn 4`: zero, one argument.
    int   slot3CReturnsZero(int) const;
    // +0x44, the selected lod's bounding radius (finalising the bounds first),
    // cached in +0x304; 0.01f when no lod is selected.
    float getBoundingRadius();
    // +0x48, the transform slot +0x50 stored, or the shared identity when none.
    const Mat4* getGeometryTransformation() const;
    // +0x4C, two arguments, both ignored: logs "A Static Mesh should not have any
    // dynamic parts..." at StaticMesh.cpp:2238 (dice's own __FILE__ and __LINE__).
    void  setDynamicPartTransformation(unsigned int part, const Mat4* transform);
    // +0x68, 0x10082D30 - `m_name = value` through MSVCP71's string operator=.
    void  setName(const vc71::string& value);
    // +0x84, select ONE material for drawing (+0x356 up, +0x31C = index); -1
    // clears both.  With the flag up the draw submits nothing at all.  [INFERRED] name.
    void  setMaterialOverride(int index);
    // +0x8C, the selected lod's material count.
    int   getMaterialCount() const;
    // +0xE8, the number of world-space occluder records (+0x338, 36 B each).
    int   getOccluderRecordCount() const;
    // +0xEC, the i-th occluder record, or null past the end.
    const void* getOccluderRecord(unsigned int index) const;

    // The ILightSampleable subobject at +0x30: +0x10 0x10197EF0 `mov al,1; retn`.
    bool  lightSampleableSlot10ReturnsTrue() const;
    // +0x14 and +0x18 are the light-sampler pair the lightmap generator uses: createLightSampler
    // makes a MeshLightSampler (LightmapGeneration/MeshLightSampling.h) from the template's
    // "<path>/Meshes/<name>.samples" (".samp_<geom><lod>" past geom 0, lod 0) and drops this
    // instance's cached lightmap; destroyLightSampler destroys it, which writes the lightmap, and
    // reloads the lightmap. See the .cpp.

    // The RaObjectParameters block embedded at +0x38, typed.  The draw writes eight of its
    // holders by name; see RaShader.h for the layout.
    ::dice::hfe::dx9rend::Ra::RaObjectParameters& objectParameters()
    {
        return *reinterpret_cast< ::dice::hfe::dx9rend::Ra::RaObjectParameters*>(m_objectParameters);
    }

    // layout, every offset from the constructor's own stores
    // +0x000..+0x02F: inherited GeometryInstanceData; the assertions below retain the ABI.
    void* const*        m_lightSampleableVptr;  // +0x030  secondary, 7 slots
    int                 m_refCount;             // +0x034  ctor 1

    // +0x038 .. +0x25B - raObjectParametersConstruct. See the header note.
    unsigned char       m_objectParameters[0x224];

    float               m_float25C;             // +0x25C  ctor 1.0f
    vc71::map_layout    m_map260;               // +0x260  the lightmap cache; 0x28 nodes
    unsigned int        m_dword26C;             // +0x26C  ctor 0
    float               m_float270;             // +0x270  ctor 1.0f
    float               m_float274;             // +0x274  ctor 1.0f
    unsigned int        m_dword278;             // +0x278  ctor 0
    unsigned int        m_dword27C;             // +0x27C  ctor 0
    void*               m_template;             // +0x280  the constructor's argument
    const Mat4*         m_transform;            // +0x284  ctor 0; slot +0x50 writes it
    unsigned int        m_dword288;             // +0x288  ctor 0
    // Not written by the constructor - slot +0xE0 sets it. See setDword28C.
    void*               m_dword28C;             // +0x28C
    Mat4                m_matrix290;            // +0x290  ctor identity
    // Per-draw light pointers, 0x44-byte object-space light records, and 28-byte
    // per-LOD light lists. The final word holds the per-vertex light-index buffer.
    vc71::vector_layout m_vector2D0;            // +0x2D0  freed by the destructor
    vc71::vector_layout m_vector2E0;            // +0x2E0  freed by the destructor
    vc71::vector_layout m_vector2F0;            // +0x2F0  freed by the destructor
    unsigned int        m_dword300;             // +0x300  ctor 0
    // +0x304 receives MeshBounds::radius (a float bit pattern) from the selected lod record.
    unsigned int        m_dword304;             // +0x304  ctor 0
    float               m_float308;             // +0x308  ctor -1.0f  (0xBF800000)
    unsigned char*      m_dword30C;             // +0x30C  ctor 0; the selected 104-byte lod record
    unsigned char*      m_dword310;             // +0x310  ctor 0; its 16-byte companion
    unsigned int        m_dword314;             // +0x314  ctor 0; the geom index
    unsigned int        m_dword318;             // +0x318  ctor 0; the lod index
    unsigned int        m_dword31C;             // +0x31C  ctor 0; the material the draw is on
    // +0x320: the distance-quality bucket the draw computes (0..4) from the Renderer's four
    // thresholds at +0x70C..+0x718; the draw writes it every frame, which is why the constructor
    // does not.
    unsigned int        m_dword320;             // +0x320  not written by the ctor
    unsigned int        m_dword324;             // +0x324  ctor 0
    unsigned int        m_dword328;             // +0x328  ctor 0
    unsigned int        m_dword32C;             // +0x32C  ctor 0; the flags (+0xC0 / +0xC4)
    void*               m_dword330;             // +0x330  ctor 0; the owning object (+0x9C)
    void*               m_dword334;             // +0x334  ctor 0; its cached root (+0xF8)
    // A fourth, and the only one whose elements get individually destroyed first: the
    // destructor calls sub_10103160 on it before freeing the block.
    vc71::vector_layout m_vector338;            // +0x338  freed by the destructor

    // +0x348 gates the one-shot inside setGeometryTransformation; the constructor sets it.
    unsigned char       m_byte348;              // +0x348  ctor 1
    unsigned char       m_pad349[3];
    // +0x34C / +0x350: the shadow-caster record this instance was registered with (its matrices
    // at +0x2F8 and +0x3B8 feed ShadowProjMat / ShadowTrapMat) and the frame ordinal of that
    // registration; the parameter refresh treats the pair as live only when the ordinal is the
    // current frame's.  Slots +0x24/+0x28 and +0xAC/+0xB0.  [INFERRED] roles.
    unsigned int        m_dword34C;             // +0x34C  ctor 0
    void*               m_dword350;             // +0x350  ctor 0; slots +0xAC / +0xB0
    unsigned char       m_byte354;              // +0x354  ctor 0; the draw: "point lights gathered"
    // sub_100FBCB0 (the parameter refresh) writes +0x355 on every draw,
    // before the draw reads it - it is "this instance is the frame's shadow caster" (+0x34C set
    // and +0x350 == the current frame ordinal).  Not a constructor field, not uninitialised.
    unsigned char       m_byte355;              // +0x355  the shadow-caster-this-frame flag
    unsigned char       m_byte356;              // +0x356  ctor 0; slot +0x84's override flag
    unsigned char       m_byte357;              // +0x357  ctor 0; the lod was chosen explicitly
    unsigned char       m_byte358;              // +0x358  ctor 0; slot +0x50 sets it: inverse pending
    unsigned char       m_byte359;              // +0x359  ctor 0; the post-load has run
    unsigned char       m_byte35A;              // +0x35A  ctor 0; "no lightmap for this instance"
    unsigned char       m_pad35B;
    // Parallel per-geometry bounds radii and LOD-distance counts.
    vc71::vector_layout m_geomRadii;            // +0x35C
    vc71::vector_layout m_geomLodDistanceCounts;// +0x36C - its +0x378 is the last ctor store
};

// Two helpers StaticMesh.cpp defines and StaticMeshLightmapGeneration.cpp (the attested TU of
// the silhouette pair) also uses.
struct StaticMeshRenderer;
StaticMeshRenderer* staticMeshRenderer();                       // the singleton, typed
const float* staticMeshPackVec4(const float v[3], float w);
// sub_10103CE0 - RendDX9's own copy of world::getRootParent (the DLL cannot import the host's);
// the light manager's shadow-caster predicate walks it too.
void* staticMeshRootParent(void* object);

#if defined(_M_IX86)

// Assert interior offsets as well as total size: callers access this compatibility layout directly.

static_assert(offsetof(StaticMesh, m_vptr) == 0x000, "m_vptr");
static_assert(offsetof(StaticMesh, m_name) == 0x004, "m_name");
static_assert(offsetof(StaticMesh, m_dword20) == 0x020, "m_dword20");
static_assert(offsetof(StaticMesh, m_dword24) == 0x024, "m_dword24");
static_assert(offsetof(StaticMesh, m_float28) == 0x028, "m_float28");
static_assert(offsetof(StaticMesh, m_float2C) == 0x02C, "m_float2C");
static_assert(offsetof(StaticMesh, m_lightSampleableVptr) == 0x030, "m_lightSampleableVptr");
static_assert(offsetof(StaticMesh, m_refCount) == 0x034, "m_refCount");
static_assert(offsetof(StaticMesh, m_float25C) == 0x25C, "m_float25C");
static_assert(offsetof(StaticMesh, m_map260) == 0x260, "m_map260");
static_assert(offsetof(StaticMesh, m_dword26C) == 0x26C, "m_dword26C");
static_assert(offsetof(StaticMesh, m_float270) == 0x270, "m_float270");
static_assert(offsetof(StaticMesh, m_float274) == 0x274, "m_float274");
static_assert(offsetof(StaticMesh, m_dword278) == 0x278, "m_dword278");
static_assert(offsetof(StaticMesh, m_dword27C) == 0x27C, "m_dword27C");
static_assert(offsetof(StaticMesh, m_template) == 0x280, "m_template");
static_assert(offsetof(StaticMesh, m_transform) == 0x284, "m_transform");
static_assert(offsetof(StaticMesh, m_dword288) == 0x288, "m_dword288");
static_assert(offsetof(StaticMesh, m_dword28C) == 0x28C, "m_dword28C");
static_assert(offsetof(StaticMesh, m_matrix290) == 0x290, "m_matrix290");
static_assert(offsetof(StaticMesh, m_vector2D0) == 0x2D0, "m_vector2D0");
static_assert(offsetof(StaticMesh, m_vector2E0) == 0x2E0, "m_vector2E0");
static_assert(offsetof(StaticMesh, m_vector2F0) == 0x2F0, "m_vector2F0");
static_assert(offsetof(StaticMesh, m_dword300) == 0x300, "m_dword300");
static_assert(offsetof(StaticMesh, m_dword304) == 0x304, "m_dword304");
static_assert(offsetof(StaticMesh, m_float308) == 0x308, "m_float308");
static_assert(offsetof(StaticMesh, m_dword30C) == 0x30C, "m_dword30C");
static_assert(offsetof(StaticMesh, m_dword310) == 0x310, "m_dword310");
static_assert(offsetof(StaticMesh, m_dword314) == 0x314, "m_dword314");
static_assert(offsetof(StaticMesh, m_dword318) == 0x318, "m_dword318");
static_assert(offsetof(StaticMesh, m_dword31C) == 0x31C, "m_dword31C");
static_assert(offsetof(StaticMesh, m_dword320) == 0x320, "m_dword320");
static_assert(offsetof(StaticMesh, m_dword324) == 0x324, "m_dword324");
static_assert(offsetof(StaticMesh, m_dword328) == 0x328, "m_dword328");
static_assert(offsetof(StaticMesh, m_dword32C) == 0x32C, "m_dword32C");
static_assert(offsetof(StaticMesh, m_dword330) == 0x330, "m_dword330");
static_assert(offsetof(StaticMesh, m_dword334) == 0x334, "m_dword334");
static_assert(offsetof(StaticMesh, m_vector338) == 0x338, "m_vector338");
static_assert(offsetof(StaticMesh, m_byte348) == 0x348, "m_byte348");
static_assert(offsetof(StaticMesh, m_dword34C) == 0x34C, "m_dword34C");
static_assert(offsetof(StaticMesh, m_dword350) == 0x350, "m_dword350");
static_assert(offsetof(StaticMesh, m_byte354) == 0x354, "m_byte354");
static_assert(offsetof(StaticMesh, m_byte355) == 0x355, "m_byte355");
static_assert(offsetof(StaticMesh, m_byte356) == 0x356, "m_byte356");
static_assert(offsetof(StaticMesh, m_byte357) == 0x357, "m_byte357");
static_assert(offsetof(StaticMesh, m_byte358) == 0x358, "m_byte358");
static_assert(offsetof(StaticMesh, m_byte359) == 0x359, "m_byte359");
static_assert(offsetof(StaticMesh, m_byte35A) == 0x35A, "m_byte35A");
static_assert(offsetof(StaticMesh, m_geomRadii) == 0x35C, "m_geomRadii");
static_assert(offsetof(StaticMesh, m_geomLodDistanceCounts) == 0x36C, "m_geomLodDistanceCounts");

// 0x378 + 4 = 0x37C, and StaticMeshTemplate::createInstance allocates exactly that.
static_assert(sizeof(StaticMesh) == 0x37C,
              "StaticMeshTemplate::createInstance (0x10116E00) allocates 0x37C");
#endif

// 0x100FDC20, called on storage the template already allocated.
StaticMesh* staticMeshConstruct(void* storage, void* objectTemplate);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_STATICMESH_H
#endif // BF2_IMAGE_RENDDX9
