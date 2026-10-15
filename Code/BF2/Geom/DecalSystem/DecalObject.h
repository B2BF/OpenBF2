// Path: [INFERRED] header beside DecalObject.cpp, attested at RendDX9 0x102218B0.
// Decal container and pooled records, recovered from RendDX9.dll 1.50.
// ELF EditableDecal callers confirm four roles; its two destructor slots shift offsets.
// Retail container: 0x44 bytes, table 0x10221860. Slot names are [INFERRED].

#ifndef BF2_GEOM_DECALSYSTEM_DECALOBJECT_H
#define BF2_GEOM_DECALSYSTEM_DECALOBJECT_H

#ifdef BF2_IMAGE_RENDDX9

#include "../../Math/Mat4.h"
#include "../../Math/Vec3.h"
#include "IDecalManager.h"
#include "IDecalTemplate.h"

#include <cstddef>

namespace dice {
namespace hfe {

class IBase;

namespace dx9rend {
struct EngineTexture;       // RendDX9/Texture.h - the batch key the render pass compares
}

namespace world {
class IObject;              // Game/Objects/IObject.h - the decal owner (host object, BF2.exe)
class ICollisionMesh;       // Physics/ICollisionMesh.h - the hit object's collision mesh (host)
class CollisionFaceInfo;    // no recovered layout; byte +0 lod, byte +1 geometry, u16 +2 face
}

namespace geom {


// Position and UV vertex used by decal records and clipping.
// [CHOSEN] Plain aggregate; allocation helpers zero all five floats.

struct DecalVertex
{
    Vec3  position;         // +0x00  object space for an object decal, world space for the ground
    float u;                // +0x0C  atlas / animation-frame texture coordinate
    float v;                // +0x10
};


// DecalFace - 8 bytes, the collision mesh's face record as RendDX9 reads it: three vertex indices and
// a material. The host type is world::CollisionFace (Physics/CollisionGeometry.h: short v0, v1, v2;
// unsigned short material); RendDX9 reads the indices zero-extended (movzx, 0x101AD39C / 0x101AD695),
// hence unsigned here. The terrain arm's static table (0x1023AE68) uses the same shape.
// [CHOSEN] name - a RendDX9-side view of a host POD, identical in both flavours.

struct DecalFace
{
    unsigned short v0;          // +0x00
    unsigned short v1;          // +0x02
    unsigned short v2;          // +0x04
    unsigned short material;    // +0x06  never read by the decal code
};

// DecalRecord is a 0x58-byte entry borrowed from DecalManager's 2048-slot pool.
// [INFERRED] field names; offsets are instruction-proven. freeRecord releases vertices.
// The constructor clears only next, position and normal; remaining bytes stay untouched.
struct DecalRecord
{
    DecalRecord()                           // seven stores, nothing else (0x10124480..0x1012449A)
        : next(0)
    {
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = 0.0f;
        normal.x = 0.0f;
        normal.y = 0.0f;
        normal.z = 0.0f;
    }

    DecalRecord*            next;           // +0x00  free-list link (DecalManager +0x290)
    Vec3                    position;       // +0x04
    unsigned char           unknown10[4];   // +0x10  [UNRESOLVED]
    unsigned int            diffuse;        // +0x14  D3DCOLOR
    float                   startFrame;     // +0x18
    unsigned char           unknown1C;      // +0x1C  [UNRESOLVED]
    unsigned char           staticSerial;   // +0x1D
    unsigned char           unknown1E[6];   // +0x1E  [UNRESOLVED]
    unsigned short          templateIndex;  // +0x24
    short                   instanceIndex;  // +0x26  render-owned
    float                   alpha;          // +0x28
    unsigned char           unknown2C[4];   // +0x2C  [UNRESOLVED]
    dx9rend::EngineTexture* texture;        // +0x30
    float                   age;            // +0x34
    float                   timeToLive;     // +0x38
    unsigned char           unknown3C[8];   // +0x3C  [UNRESOLVED]
    Vec3                    normal;         // +0x44
    DecalVertex*            vertices;       // +0x50
    unsigned int            vertexCount;    // +0x54
};


// IDecalObject - [INFERRED name] the pure interface behind table 0x10221834. BF2.exe reaches the
// object only through it (DecalObjectAccess.h's raw slots, IObject::getDecalObject's void*).
// Declaration order IS the table order; MSVC's single destructor slot sits first, as shipped.

class IDecalObject
{
public:
    virtual ~IDecalObject() = 0;                                                // +0x00
    // +0x04 / +0x08: a pointer stored at +0x34. The render pass selects its bucket 2 when it is
    // non-null (0x101A9B1B) and then reads the pointee's +0x2F8 / +0x3B8 matrices and +0x438 vector
    // (0x101A9D7C..0x101A9DA5) - XpLightManager's shadow render record shape (XpRenderRecordLayout:
    // shadowProj / shadowTrap / atlasBounds). [INFERRED] name; NO caller of the setter was found in
    // BF2.exe or RendDX9 1.50, so +0x34 stays null and bucket 2 is dead in a shipped session.
    virtual void  setShadowRecord(void* record) = 0;                            // +0x04
    virtual void* getShadowRecord() const = 0;                                  // +0x08
    virtual void  setVisible(bool visible) = 0;                                 // +0x0C
    virtual bool  isVisible() const = 0;                                        // +0x10
    virtual void  clear() = 0;                                                  // +0x14
    virtual unsigned int add(const Mat4* transform, const IDecalTemplate* decalTemplate,
                             float scale, world::ICollisionMesh* collisionMesh,
                             world::CollisionFaceInfo* faceInfo) = 0;           // +0x18
    virtual void  remove(unsigned int decalId) = 0;                             // +0x1C
    virtual unsigned int getNumDecals() const = 0;                              // +0x20
    virtual bool  update(float deltaTime) = 0;                                  // +0x24
    virtual void  release() = 0;                                                // +0x28
};

// (7 B) - `mov [ecx], offset 0x10221834; retn`: the pure destructor's body.
inline IDecalObject::~IDecalObject() {}

// The per-face clip planes - a VC7.1 std::vector<Plane> file static at 0x10265520 in the image.
// Defined in DecalObject.cpp; only the clipper's signature names it here.
struct DecalClipPlanes;

// Per-owner decal container. release() queues deletion for DecalManager::update.
// The dynamic ring holds capacity - 1 entries; the static list has no bound check.
// Static IDs pre-increment from 1, so the first is 2; remove(0) does nothing.
class DecalObject : public IDecalObject
{
public:
    explicit DecalObject(world::IObject* owner);
    virtual ~DecalObject();

    // [CHOSEN] class-level allocation: retail's global operator new / delete ARE sub_1005F320 /
    // sub_1005F360, which ::renddx9abi::engineAlloc / engineFree reproduce. createClient's
    // `new DecalObject(owner)` and the scalar deleting destructor then pair on the engine heap in
    // both flavours without depending on the global replacement. NON-throwing, so the new-expression
    // tests the block before constructing - createClient's shipped `test eax, eax; jz` (0x10124636:
    // a failed allocation answers 0 and constructs nothing).
    static void* operator new(std::size_t size) noexcept;
    static void  operator delete(void* block) noexcept;
    // The placement pair, so a class-level operator new does not hide `new (storage) DecalObject`
    // (D2's stand-in constructs that way).
    static void* operator new(std::size_t, void* storage) noexcept { return storage; }
    static void  operator delete(void*, void*) noexcept {}

    virtual void  setShadowRecord(void* record);                                // +0x04
    virtual void* getShadowRecord() const;                                      // +0x08
    virtual void  setVisible(bool visible);                                     // +0x0C
    virtual bool  isVisible() const;                                            // +0x10
    virtual void  clear();                                                      // +0x14
    virtual unsigned int add(const Mat4* transform, const IDecalTemplate* decalTemplate,
                             float scale, world::ICollisionMesh* collisionMesh,
                             world::CollisionFaceInfo* faceInfo);               // +0x18
    virtual void  remove(unsigned int decalId);                                 // +0x1C
    virtual unsigned int getNumDecals() const;                                  // +0x20
    virtual bool  update(float deltaTime);                                      // +0x24
    virtual void  release();                                                    // +0x28

    // the fields the manager and the render pass touch directly in the image (no slot)
    // Named for D2's stand-in (manager/DecalObjectStandIn.h) where it has a name.
    bool&         activeFlag()            { return m_active; }        // +0x04  addInstance 0x101246F9
                                                                      //        sets, removeInstance
                                                                      //        0x1012482B clears
    unsigned int  staticCount() const     { return m_staticCount; }   // +0x10  render 0x101AA991
    bool&         renderFlag()            { return m_visible; }       // +0x19  render reads and
                                                                      //        clears 0x101AAAEB
    world::IObject* ownerObject() const   { return m_owner; }         // +0x1C  render 0x101A9BA4
    void*         ownerGeometry() const   { return m_ownerGeometry; } // +0x20  render 0x101A9B2C
    DecalRecord** ringRecords() const     { return m_ring; }          // +0x28  render 0x101AA9D0
    DecalRecord** staticRecords() const   { return m_static; }        // +0x2C  render 0x101AA996
    unsigned int  ringInsert() const      { return m_insert; }        // +0x38  render's stop
    unsigned int  ringOldest() const      { return m_oldest; }        // +0x3C  render's start
    unsigned int  ringMask() const        { return m_mask; }          // +0x40

private:
    // the lists (inline members in the image; 0x101AC770 is the ring pop's unreferenced
    //      out-of-line copy, inlined at every use)
    void pushStatic(DecalRecord* record);
    void pushRing(DecalRecord* record);
    DecalRecord* popRing();

    // add's two geometry builders and the projection they share (thiscall in the image;
    //      `this` is passed and unused)
    void buildQuad(DecalRecord* record, const Mat4* transform,
                   const IDecalTemplate* decalTemplate, float scale);           // (retn 10h)
    void buildProjected(DecalRecord* record, const Mat4* transform,
                        const IDecalTemplate* decalTemplate, float scale,
                        world::ICollisionMesh* collisionMesh,
                        world::CollisionFaceInfo* faceInfo);                    // (retn 18h)
    bool getTerrainPatch(float x, float z, Vec3* vertices, unsigned int* face); // (retn 10h)
    void projectOntoFace(unsigned int face, const Vec3& direction, const DecalVertex* quad,
                         DecalRecord* record, float maxAngle, int depth,
                         const Vec3* vertices, const DecalFace* faces, int faceCount,
                         world::ICollisionMesh* collisionMesh,
                         world::CollisionFaceInfo* faceInfo);                   // (retn 2Ch)
    bool buildClipPlanes(unsigned int face, const Vec3& direction, float maxAngle,
                         const Vec3* vertices, const DecalFace* faces,
                         int faceCount);                                        // (retn 18h)
    int  clipPolygon(const DecalVertex* input, unsigned int count,
                     const DecalClipPlanes& planes, const DecalVertex** output); // (retn 10h)

    DecalObject(const DecalObject&);                                            // not copyable
    DecalObject& operator=(const DecalObject&);

    // the retail layout (+0x00 is the vptr)
    bool             m_active;          // +0x04  in the manager's active set (addInstance /
                                        //        removeInstance write it; add and the deferred
                                        //        delete read it)
    unsigned char    m_pad05[3];
    unsigned int     m_capacity;        // +0x08  the ring size: 32 with an owner, 64 without
    unsigned int     m_count;           // +0x0C  live ring decals
    unsigned int     m_staticCount;     // +0x10  live static decals
    unsigned int     m_unknown14;       // +0x14  [UNRESOLVED] constructor 0, no other access found
    bool             m_unknown18;       // +0x18  [UNRESOLVED] constructor 0, no other access found
    bool             m_visible;         // +0x19  constructor 1; setVisible; the render pass clears it
                                        //        every frame after reading it (0x101AAAE3 / 0x101AAAEB)
    bool             m_wasVisible;      // +0x1A  constructor 1; setVisible; update copies +0x19
    unsigned char    m_pad1B;
    world::IObject*  m_owner;           // +0x1C  the constructor's argument
    void*            m_ownerGeometry;   // +0x20  add caches the first non-null IObject +0x38 up the
                                        //        owner's parent chain
    DecalRecord**    m_buffer;          // +0x24  the block (delete[]d by the destructor)
    DecalRecord**    m_ring;            // +0x28  m_buffer + 8
    DecalRecord**    m_static;          // +0x2C  m_buffer
    unsigned int     m_nextId;          // +0x30  constructor 1
    void*            m_shadowRecord;    // +0x34  slots +0x04 / +0x08
    unsigned int     m_insert;          // +0x38  the ring slot the next decal takes
    unsigned int     m_oldest;          // +0x3C  the ring slot of the oldest decal
    unsigned int     m_mask;            // +0x40  capacity - 1

    friend struct DecalObjectLayoutCheck;
};

// Offsets the manager's pool and render path depend on (x86 only: the retail layout). Nothing in
// either object crosses the host boundary, so the layout is the same in both RendDX9 flavours.
struct DecalObjectLayoutCheck
{
    // The host's collision-face array is read through this view: every architecture.
    static_assert(sizeof(DecalFace) == 8, "the collision face stride is 8 ([face*8])");
#if defined(_M_IX86)
    static_assert(sizeof(DecalVertex) == 0x14, "the decal vertex stride is 20 (lea [n+n*4]*4)");
    static_assert(offsetof(DecalVertex, u) == 0x0C, "u at +0x0C");
    static_assert(offsetof(DecalVertex, v) == 0x10, "v at +0x10");
    static_assert(sizeof(DecalRecord) == 0x58, "the pool stride is 0x58 (sub_10124450)");
    static_assert(offsetof(DecalRecord, position) == 0x04, "+0x04");
    static_assert(offsetof(DecalRecord, diffuse) == 0x14, "+0x14");
    static_assert(offsetof(DecalRecord, startFrame) == 0x18, "+0x18");
    static_assert(offsetof(DecalRecord, unknown1C) == 0x1C, "+0x1C");
    static_assert(offsetof(DecalRecord, staticSerial) == 0x1D, "+0x1D");
    static_assert(offsetof(DecalRecord, templateIndex) == 0x24, "+0x24");
    static_assert(offsetof(DecalRecord, instanceIndex) == 0x26, "+0x26");
    static_assert(offsetof(DecalRecord, alpha) == 0x28, "+0x28");
    static_assert(offsetof(DecalRecord, texture) == 0x30, "+0x30");
    static_assert(offsetof(DecalRecord, age) == 0x34, "+0x34");
    static_assert(offsetof(DecalRecord, timeToLive) == 0x38, "+0x38");
    static_assert(offsetof(DecalRecord, normal) == 0x44, "+0x44");
    static_assert(offsetof(DecalRecord, vertices) == 0x50, "+0x50");
    static_assert(offsetof(DecalRecord, vertexCount) == 0x54, "+0x54");

    static_assert(sizeof(DecalObject) == 0x44, "createClient allocates 0x44");
    static_assert(offsetof(DecalObject, m_active) == 0x04, "+0x04");
    static_assert(offsetof(DecalObject, m_capacity) == 0x08, "+0x08");
    static_assert(offsetof(DecalObject, m_count) == 0x0C, "+0x0C");
    static_assert(offsetof(DecalObject, m_staticCount) == 0x10, "+0x10");
    static_assert(offsetof(DecalObject, m_unknown14) == 0x14, "+0x14");
    static_assert(offsetof(DecalObject, m_unknown18) == 0x18, "+0x18");
    static_assert(offsetof(DecalObject, m_visible) == 0x19, "+0x19");
    static_assert(offsetof(DecalObject, m_wasVisible) == 0x1A, "+0x1A");
    static_assert(offsetof(DecalObject, m_owner) == 0x1C, "+0x1C");
    static_assert(offsetof(DecalObject, m_ownerGeometry) == 0x20, "+0x20");
    static_assert(offsetof(DecalObject, m_buffer) == 0x24, "+0x24");
    static_assert(offsetof(DecalObject, m_ring) == 0x28, "+0x28");
    static_assert(offsetof(DecalObject, m_static) == 0x2C, "+0x2C");
    static_assert(offsetof(DecalObject, m_nextId) == 0x30, "+0x30");
    static_assert(offsetof(DecalObject, m_shadowRecord) == 0x34, "+0x34");
    static_assert(offsetof(DecalObject, m_insert) == 0x38, "+0x38");
    static_assert(offsetof(DecalObject, m_oldest) == 0x3C, "+0x3C");
    static_assert(offsetof(DecalObject, m_mask) == 0x40, "+0x40");
#endif
};


// D2 interplay - declared here, defined by the DecalManager / DecalTemplate drafts (D2) or the
// integrator. Each is a field (or an inline member) DecalObject's bodies reach directly in the image
// - no virtual call - so none may be routed through a virtual getter: that would add dispatches the
// shipped code does not make. [CHOSEN] seam names; the offsets are the shipped objects' (DecalManager
// 0x2CC bytes; the renderer DecalTemplate, D2's DecalTemplate.h, whose native layout moves after
// +0x3C because its two strings are host strings - one more reason these are accessors, not
// offsets).

namespace decal_manager_state
{
bool         decalsEnabled();                   // +0x2A6 byte (VideoOptions::apply's raw write)
// The free-list pop add inlines (0x101AE55B..0x101AE56B): `head = head ? head->next : 0` and the
// OLD head is answered unchecked - an exhausted pool hands back 0 and add writes its +0x50 at once.
DecalRecord* allocateRecord();                  // +0x290
// DecalManager::freeRecord [D2's name] - 0x101AC620 (53 B, retn 4, thiscall on the manager), the
// out-of-line copy of the inline member clear, remove, update and add's eviction all inline:
//     if (record->vertices) { delete record->vertices; record->vertices = 0; }
//     record->next = m_freeRecords; m_freeRecords = record;                     // +0x290
// scalar delete (sub_1005F360) of a new[] block (sub_1005F340): one heap, one body. Kept.
void         freeRecord(DecalRecord* record);
float        surfaceOffset();                   // +0x2C0  0.01f (constructor, 0x10125385)
float        maxProjectionAngle();              // +0x2C4  60.0f (constructor, 0x1012538F), degrees
}

namespace decal_template_state
{
const Vec4&     color(const IDecalTemplate* decalTemplate);            // +0x08 (D2 colorVec4)
bool            isStatic(const IDecalTemplate* decalTemplate);         // +0x20
const Vec2&     uvOffset(const IDecalTemplate* decalTemplate);         // +0x34 (D2 uvOffset)
unsigned short  index(const IDecalTemplate* decalTemplate);            // +0x58 (D2 getIndex)
// `+0x7C ? [+0x7C]+0x10 : +0x78` (0x101AE5AF..0x101AE5C3) - the atlas page's texture when the
// template sits in DecalAtlas.tai, else its own (D2 getAtlasEntry / getTexture).
dx9rend::EngineTexture* renderTexture(const IDecalTemplate* decalTemplate);
bool            animationRandomizedStartFrame(const IDecalTemplate* decalTemplate);   // +0x80
bool            animationEnable(const IDecalTemplate* decalTemplate);  // +0x88
unsigned int    animationFrameCount(const IDecalTemplate* decalTemplate);  // +0x94
unsigned int    animationFrameCountX(const IDecalTemplate* decalTemplate); // +0x98
float           frameCountXReciprocal(const IDecalTemplate* decalTemplate); // +0x9C (D2's name)
const Vec2&     uvExtent(const IDecalTemplate* decalTemplate);         // +0xA0 (D2 m_uvExtent)
}

// PE 0x1009EF30 (lcgrand_step) on the renderer-local Random at the ONE stream the
// particle system and the decals draw from. The tree keeps that stream file-static in
// ParticleSystemManager.cpp's anonymous namespace (psmType0Random / psmEmissionRaw); the integrator
// exposes it under this name. Declared, not defined.
unsigned int rendererRandomRaw();

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9

#endif // BF2_GEOM_DECALSYSTEM_DECALOBJECT_H
