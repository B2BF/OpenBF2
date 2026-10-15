// Builds and owns decals in object space, projecting onto collision faces or terrain.
// Geometry uses float rounding and the per-site arithmetic order of the renderer.

#ifdef BF2_IMAGE_RENDDX9

#include "DecalObject.h"

#include "../../Math/Plane3.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"

// The host (BF2.exe) interfaces add dispatches into, typed in both flavours (RW-266): each virtual
// used below sits at its retail PE slot in this tree's MSVC table; no STL type crosses
// those calls. IObject's +0x28 / +0x38
// are its retail-aligned prefix (ObjectLayoutAccess).
#include "../../Game/Objects/ObjectLayoutAccess.h"      // world::IObject + its retail-aligned prefix
#include "../../Physics/ICollisionMesh.h"               // hasLod / getVertexVector / getFaceCount /
                                                         // getFaceVector
#include "../../Physics/HeightmapCluster.h"             // getPrimaryScale / getSquareInWorldCoords

#include <cfloat>
#include <cmath>
#include <cstring>
#include <set>
#include <vector>

namespace dice {
namespace hfe {
namespace geom {

extern IBase* g_DecalManager;               // ModuleGeom.cpp, dword_1024B908
extern IBase* g_ParticleSystemManager;      // ModuleGeom.cpp, dword_1024B900
// ParticleSystemManager.cpp (published in ParticleSystemManager.h): the manager's HeightmapCluster
// pointer (retail +0x8C14), read without a null test on the manager.
IBase* particleSystemManagerHeightmapCluster(IBase* manager);

// the per-face clip planes, a VC7.1 std::vector<Plane> file static (constructed empty,
// destroyed by the atexit thunk 0x101D9F90). File-private: nothing outside this file reads its
// layout, so it is this toolchain's vector. [CHOSEN, not recovered] the wrapper.
struct DecalClipPlanes
{
    std::vector<Plane3> planes;
};

namespace {


// .data the math header's per-TU degrees-to-radians float (0x3C8EFA35). Its double and
// the radians-to-degrees pair beside it (0x1023ADD8..0x1023ADEF) and 0.01f (0x1023ADF0) are never
// read here and are not reproduced.
float s_degreesToRadians = 0.0174532924f;

// .data the terrain patch's face adjacency (18 faces x three u16, 0xFFFF = none): the
// 3 x 3 cells getTerrainPatch fills, two faces per cell. Read-only in the image (101AD5E0 only).
const unsigned short s_terrainAdjacency[18][3] =
{
    { 0xFFFF, 0xFFFF,      2 }, {      0,      2,      6 }, {      1, 0xFFFF,      3 },
    {      2,      4,      8 }, {      3, 0xFFFF,      5 }, {      4, 0xFFFF,     10 },
    { 0xFFFF,      1,      7 }, {      6,      8,     12 }, {      7,      3,      9 },
    {      8,     10,     14 }, {      9,      5,     11 }, {     10, 0xFFFF,     16 },
    { 0xFFFF,      7,     13 }, {     12,     14, 0xFFFF }, {     13,      9,     15 },
    {     14,     16, 0xFFFF }, {     15,     11,     17 }, {     16, 0xFFFF, 0xFFFF },
};

// .data the terrain patch's faces: cell c's corners are 4c..4c+3 (getSquareInWorldCoords
// order), split (4c, 4c+1, 4c+2) / (4c+2, 4c+1, 4c+3). Faces 8 and 9 are the centre cell's.
const DecalFace s_terrainFaces[18] =
{
    {  0,  1,  2, 0 }, {  2,  1,  3, 0 }, {  4,  5,  6, 0 }, {  6,  5,  7, 0 },
    {  8,  9, 10, 0 }, { 10,  9, 11, 0 }, { 12, 13, 14, 0 }, { 14, 13, 15, 0 },
    { 16, 17, 18, 0 }, { 18, 17, 19, 0 }, { 20, 21, 22, 0 }, { 22, 21, 23, 0 },
    { 24, 25, 26, 0 }, { 26, 25, 27, 0 }, { 28, 29, 30, 0 }, { 30, 29, 31, 0 },
    { 32, 33, 34, 0 }, { 34, 33, 35, 0 },
};

// .data the unit quad in the decal's XZ plane, (u, v) in -1..1.
const DecalVertex s_quadTemplate[4] =
{
    { { -1.0f, 0.0f, -1.0f }, -1.0f,  1.0f },
    { {  1.0f, 0.0f, -1.0f },  1.0f,  1.0f },
    { {  1.0f, 0.0f,  1.0f },  1.0f, -1.0f },
    { { -1.0f, 0.0f,  1.0f }, -1.0f, -1.0f },
};

// .data writable, and never written. [INFERRED] names.
bool s_projectOntoFacePlane = true;         // byte 1: snap each clipped vertex onto its face plane
int  s_maxProjectionDepth   = 4;            // the flood fill's neighbour depth

// .bss 0x102654C0.. - the clipper's two outcode rows, the unit quad's working copy, its two vertex
// ping-pong buffers (ONE 16-element array in the image, 0x101D60C0 zeroes it), the visited-face set
// (0x101D6100 / atexit 0x101D9FD0), the terrain patch's 36 corners (0x101D6170) and the projected
// geometry (0x101D6140). The clip planes (0x10265520) are s_clipPlanes.
unsigned char            s_outcodes[2][8];
DecalVertex              s_quad[4];
DecalClipPlanes          s_clipPlanes;
DecalVertex              s_clipBuffer[2][8];
std::set<unsigned short> s_visitedFaces;
Vec3                     s_terrainVertices[36];
// 0x10265838 - 128 elements in the image (0x101D6140 loops 0x80), and projectOntoFace resets the
// count only after a triangle lands: from 126 vertices the next triangle writes index 128, one
// vertex (20 bytes) past the array, over the next statics (two vectors nothing else uses, at
// the first one's atexit free then frees a float). The 129th slot here
// absorbs exactly that write, so the shipped sequence - write, then reset to 0 - is kept without
// letting it land on whatever this toolchain places next. [CHOSEN, not recovered] the size.
DecalVertex              s_geometry[129];

// Not reproduced (never read in the image): the 27 nato squad-name strings every includer of one
// shared header constructs (0x101D5EC0), 1 / sqrt(2) (0x101D6080), the two vectors above, and the
// object whose empty destructor is registered at 0x101D61A0.


// g_DecalManager is the manager's primary interface pointer (ModuleGeom's getSingletonList).
IDecalManager* decalManager()
{
    return static_cast<IDecalManager*>(g_DecalManager);
}

// ParticleSystemManager +0x8C14 - m_heightmapCluster, the "HeightmapCluster" singleton the particle
// system caches (0x100D2FD0). The struct is TU-local to ParticleSystemManager.cpp, which publishes
// the read; retail loads the field directly.
IBase* particleHeightmapCluster()
{
    return particleSystemManagerHeightmapCluster(g_ParticleSystemManager);
}


// Every draw inlines `lcgrand_step; or al, 80h; sar eax, 7; add eax, 1`.
int randomInteger()
{
    return (static_cast<int>(rendererRandomRaw() | 0x80u) >> 7) + 1;
}

// `fild; fmul 2^-23 (flt_10221810); fsub 1.0; fstp` - a symmetric draw in (-1, 1].
float randomSymmetric()
{
    return static_cast<float>(randomInteger()) * 1.1920928955078125e-07f - 1.0f;
}

// PE 0x1000ED40 (34 B, stdcall) - clamp with the high bound tested first (fcomi/fcmovnb) and an
// unordered value landing on the LOW bound (fcomi/fcmovb).
float clampValue(float value, float low, float high)
{
    float result = value;
    if (result >= high)
        result = high;
    if (!(result >= low))
        result = low;
    return result;
}

// normalise in place: |v|^2 = (x*x + y*y) + z*z kept as a float; within
// FLT_EPSILON (flt_10208958) of 1 it is left alone; below FLT_EPSILON it is zeroed (answering
// -20001, discarded by every caller here); otherwise each component is scaled by 1 / sqrt(|v|^2).
int normalizeInPlace(Vec3& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return 0;
    if (std::fabs(lengthSquared) < FLT_EPSILON)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return -20001;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    v.x = inverse * v.x;
    v.y = inverse * v.y;
    v.z = inverse * v.z;
    return 0;
}

// PE 0x1007C440 (83 B, thiscall on the matrix) - transformVector: the 3x3 part only. Every input is
// read before the first store, so `in` and `out` may alias.
void transformVector(const Mat4& m, const Vec3& in, Vec3& out)
{
    const float z = m.m[2] * in.x + m.m[10] * in.z + m.m[6] * in.y;
    const float y = m.m[1] * in.x + m.m[9] * in.z + m.m[5] * in.y;
    const float x = m.m[8] * in.z + m.m[4] * in.y + in.x * m.m[0];
    out.x = x;
    out.y = y;
    out.z = z;
}

// the plane through three points: normal = normalise((b - a) x
// (c - a)), d = -(normal . a). The normal member is zeroed first (its constructor), as shipped.
Plane3 planeFromPoints(const Vec3& a, const Vec3& b, const Vec3& c)
{
    Plane3 plane;
    plane.normal.x = 0.0f;
    plane.normal.y = 0.0f;
    plane.normal.z = 0.0f;
    const float ux = c.x - a.x;
    const float uy = c.y - a.y;
    const float uz = c.z - a.z;
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float vz = b.z - a.z;
    Vec3 normal;
    normal.x = uz * vy - vz * uy;
    normal.y = vz * ux - vx * uz;
    normal.z = vx * uy - vy * ux;
    normalizeInPlace(normal);
    plane.normal = normal;
    plane.d = -(normal.x * a.x + normal.y * a.y + normal.z * a.z);
    return plane;
}

// copy a plane, RE-normalising the copied normal (d is read first and
// stored last).
void planeAssign(Plane3& destination, const Plane3& source)
{
    const float d = source.d;
    destination.normal = source.normal;
    normalizeInPlace(destination.normal);
    destination.d = d;
}

// buildClipPlanes' edge normal - normalise(edge x faceNormal), the body the three edges share

Vec3 edgeNormal(const Vec3& from, const Vec3& to, const Vec3& faceNormal)
{
    const float ex = to.x - from.x;
    const float ey = to.y - from.y;
    const float ez = to.z - from.z;
    Vec3 normal;
    normal.x = faceNormal.z * ey - ez * faceNormal.y;
    normal.y = ez * faceNormal.x - faceNormal.z * ex;
    normal.z = ex * faceNormal.y - ey * faceNormal.x;
    normalizeInPlace(normal);
    return normal;
}

// `new DecalVertex[count]` - operator new[] (sub_1005F340, the engine heap) and the element
// constructor's five zero stores per vertex, run only when the block arrived (0x101ACBCF /
// 0x101AE3CF). The answer is not otherwise checked here; each caller keeps its own shipped test.
DecalVertex* allocateVertices(unsigned int count)
{
    DecalVertex* const block = static_cast<DecalVertex*>(
        ::renddx9abi::engineAlloc(count * static_cast<unsigned int>(sizeof(DecalVertex))));
    if (block)
    {
        for (unsigned int i = 0; i < count; ++i)
        {
            block[i].position.x = 0.0f;
            block[i].position.y = 0.0f;
            block[i].position.z = 0.0f;
            block[i].u = 0.0f;
            block[i].v = 0.0f;
        }
    }
    return block;
}

// CollisionFaceInfo - no recovered layout in the tree; the host's own writers stamp it by raw byte
// offset in both images (Physics/CollisionMesh.cpp: byte 0 the lod, byte 1 the geometry;
// CollisionMeshTemplate.cpp: the u16 at +2 the face), and RendDX9 reads it the same way. A POD of
// four bytes, identical in both flavours.
int faceInfoLod(const world::CollisionFaceInfo* info)               // `mov r, [info]; and r, 0FFh`
{
    return reinterpret_cast<const unsigned char*>(info)[0];
}
int faceInfoGeometry(const world::CollisionFaceInfo* info)          // `movzx r, byte ptr [info+1]`
{
    return reinterpret_cast<const unsigned char*>(info)[1];
}
unsigned int faceInfoFace(const world::CollisionFaceInfo* info)     // `movzx r, word ptr [info+2]`
{
    return *reinterpret_cast<const unsigned short*>(
        reinterpret_cast<const unsigned char*>(info) + 2);
}

// Writes three adjacent collision-face indices and reports whether adjacency exists.
bool collisionMeshAdjacentFaces(world::ICollisionMesh* mesh, int geometry, int lod,
                                unsigned int face, unsigned short* out)
{
    typedef BaseVector3<unsigned short, VectorTraits3<unsigned short> > FaceTriple;
    return mesh->getAdjacentCollisionFaceIndices(geometry, lod, face,
                                                 *reinterpret_cast<FaceTriple*>(out));
}

// PE 0x101AECD0 (112 B; ECX = the point, EDX = the planes) - one bit per plane the point is strictly
// in front of (distance > 0; zero and unordered distances stay inside). The plane count is re-read
// every iteration and `1 << i` is a byte shift.
unsigned char computeOutcode(const Vec3& point, const std::vector<Plane3>& planes)
{
    unsigned char code = 0;
    for (unsigned int i = 0; i < planes.size(); ++i)
    {
        const Plane3& plane = planes[i];
        const float distance =
            plane.normal.z * point.z + plane.normal.x * point.x + point.y * plane.normal.y + plane.d;
        if (distance > 0.0f)
            code = static_cast<unsigned char>(code | static_cast<unsigned char>(1u << i));
    }
    return code;
}

// projectOntoFace's per-vertex emit (inlined three times per triangle in the image,
// 0x101AD717..0x101AD9AA): copy, snap onto the face plane when the flag is set, lift by `offset`,
// count it.
void emitVertex(DecalRecord* record, const DecalVertex& source, const Plane3& facePlane,
                const Vec3& offset)
{
    DecalVertex& target = record->vertices[record->vertexCount];
    target = source;
    if (s_projectOntoFacePlane)
    {
        const float distance = facePlane.normal.y * target.position.y
                             + facePlane.normal.z * target.position.z
                             + facePlane.normal.x * target.position.x + facePlane.d;
        const float pushY = facePlane.normal.y * distance;
        const float pushZ = facePlane.normal.z * distance;
        target.position.x = target.position.x - facePlane.normal.x * distance;
        target.position.y = target.position.y - pushY;
        target.position.z = target.position.z - pushZ;
    }
    target.position.x = offset.x + target.position.x;
    target.position.y = offset.y + target.position.y;
    target.position.z = offset.z + target.position.z;
    ++record->vertexCount;
}

// The quad setup both builders open with (0x101AC929..0x101ACA45 / 0x101ADF8F..0x101AE0AB, the same
// instructions twice): copy the unit quad, draw the rotation (one symmetric draw, the template's
// +0x44 then +0x4C), draw the size spread (one draw, +0x14), and place the texture window - the
// template's atlas offset, moved to the start frame's cell when the template animates.
struct QuadSetup
{
    float cosine;
    float sine;
    float randomSize;
    float originU;
    float originV;
};

void setupQuad(QuadSetup& setup, const DecalRecord* record, const IDecalTemplate* decalTemplate)
{
    for (int i = 0; i < 4; ++i)                                     // rep movsd, 20 dwords
        s_quad[i] = s_quadTemplate[i];

    const float rotationDraw = randomSymmetric();
    const float randomRotation = decalTemplate->getRandomRotation() * rotationDraw;   // +0x44
    // The product stays in st(0) through fcos / fsin (never stored first).
    const float angle = (decalTemplate->getRotation() + randomRotation) * s_degreesToRadians;   // +0x4C
    setup.cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
    setup.sine = static_cast<float>(std::sin(static_cast<double>(angle)));

    const float sizeDraw = randomSymmetric();
    setup.randomSize = decalTemplate->getRandomSize() * sizeDraw;                     // +0x14

    const Vec2& offset = decal_template_state::uvOffset(decalTemplate);
    setup.originU = offset.x;
    setup.originV = offset.y;
    if (decal_template_state::animationEnable(decalTemplate)
        && decal_template_state::animationFrameCount(decalTemplate) != 0)
    {
        // fistp in truncation mode, then an unsigned divide by the frames across - no guard
        // against a zero divisor, as shipped.
        const unsigned int frame =
            static_cast<unsigned int>(static_cast<int>(record->startFrame));
        const unsigned int column = frame % decal_template_state::animationFrameCountX(decalTemplate);
        const Vec2& extent = decal_template_state::uvExtent(decalTemplate);
        setup.originU = static_cast<float>(column) * extent.x
                      + decal_template_state::uvOffset(decalTemplate).x;
        setup.originV = static_cast<float>(frame - column)
                      * decal_template_state::frameCountXReciprocal(decalTemplate) * extent.y
                      + decal_template_state::uvOffset(decalTemplate).y;
    }
}

} // anonymous namespace


// construction


void* DecalObject::operator new(std::size_t size) noexcept
{
    return ::renddx9abi::engineAlloc(static_cast<unsigned int>(size));
}

void DecalObject::operator delete(void* block) noexcept
{
    ::renddx9abi::engineFree(block);
}

// in the image's store order. The block is not null-tested: an
// allocation failure faults on the first terminator store, as shipped.
DecalObject::DecalObject(world::IObject* owner)
{
    m_count = 0;
    m_staticCount = 0;
    m_unknown14 = 0;
    m_unknown18 = false;
    m_visible = true;
    m_wasVisible = true;
    m_owner = owner;
    m_ownerGeometry = 0;
    m_nextId = 1;
    m_shadowRecord = 0;
    m_capacity = owner ? 0x20u : 0x40u;
    // `new DecalRecord*[m_capacity + 8]` - operator new[] (sub_1005F340): capacity * 4 + 0x20 bytes.
    DecalRecord** const block = static_cast<DecalRecord**>(::renddx9abi::engineAlloc(
        (m_capacity + 8u) * static_cast<unsigned int>(sizeof(DecalRecord*))));
    m_static = block;
    m_buffer = block;
    m_ring = block + 8;
    m_ring[0] = 0;
    m_static[0] = 0;
    m_mask = m_capacity - 1u;
    m_insert = 0;
    m_oldest = 0;
    m_active = false;
}

// (complete, unreferenced) / 0x101AEC80 (scalar deleting, +0x00): clear by a direct call,
// leave the manager's active set, free the block (operator delete[], sub_1005F370).
DecalObject::~DecalObject()
{
    DecalObject::clear();
    decalManager()->removeInstance(this);                                  // IDecalManager +0x28
    ::renddx9abi::engineFree(m_buffer);
}


void DecalObject::setShadowRecord(void* record)         // +0x04,
{
    m_shadowRecord = record;
}

void* DecalObject::getShadowRecord() const              // +0x08,
{
    return m_shadowRecord;
}

void DecalObject::setVisible(bool visible)              // +0x0C, both bytes
{
    m_visible = visible;
    m_wasVisible = visible;
}

bool DecalObject::isVisible() const                     // +0x10,
{
    return m_visible;
}

unsigned int DecalObject::getNumDecals() const          // +0x20, static first
{
    return m_staticCount + m_count;
}

// +0x28, hand the object to the manager's deferred-delete queue.
void DecalObject::release()
{
    decalManager()->enqueue(this);                                         // IDecalManager +0x18
}


// THE lists


// (38 B, retn 4). No bound: the eighth static's terminator lands in m_ring[0].
void DecalObject::pushStatic(DecalRecord* record)
{
    m_static[m_staticCount] = record;
    ++m_staticCount;
    m_static[m_staticCount] = 0;
}

// (76 B, retn 4). An empty ring restarts at slot 0 (oldest 0, insert 1).
void DecalObject::pushRing(DecalRecord* record)
{
    if (m_count == 0)
    {
        m_insert = 1;
        m_count = 1;
        m_oldest = 0;
        m_ring[0] = record;
        return;
    }
    const unsigned int slot = m_insert;
    m_insert = m_mask & (slot + 1u);
    ++m_count;
    m_ring[slot] = record;
}

// (38 B, unreferenced; inlined by clear, update and add's eviction). No empty test.
DecalRecord* DecalObject::popRing()
{
    DecalRecord** const slot = &m_ring[m_oldest];
    DecalRecord* const record = *slot;
    *slot = 0;
    --m_count;
    m_oldest = (m_oldest + 1u) & m_mask;
    return record;
}

// +0x14, drain the ring oldest first, then the statics (swap-with-last from
// slot 0), then zero both ring cursors. The manager is NOT told: the object stays in the active
// set until update finds it empty.
void DecalObject::clear()
{
    while (m_count > 0)
        decal_manager_state::freeRecord(popRing());
    while (m_static[0])
    {
        DecalRecord* const record = m_static[0];
        --m_staticCount;
        m_static[0] = m_static[m_staticCount];
        m_static[m_staticCount] = 0;
        decal_manager_state::freeRecord(record);
    }
    m_insert = 0;
    m_oldest = 0;
}

// +0x1C, drop one static decal by id. The id compares zero-extended
// against the whole argument, so no argument above 255 ever matches. Ring decals are never
// removable this way. The manager is not told.
void DecalObject::remove(unsigned int decalId)
{
    if (!decalId)
        return;
    DecalRecord** const statics = m_static;
    if (!statics[0])
        return;
    unsigned int index = 0;
    while (statics[index]->staticSerial != decalId)
    {
        ++index;
        if (!m_static[index])
            return;
    }
    DecalRecord* const record = statics[index];
    --m_staticCount;
    statics[index] = statics[m_staticCount];
    m_static[m_staticCount] = 0;
    decal_manager_state::freeRecord(record);
}

// +0x24, the manager's per-frame tick over its active set.
//   * an ownerless (ground) object is always visible; +0x1A takes this frame's +0x19;
//   * an object nobody drew ages its ring decals twice as fast;
//   * ring decals age oldest to newest, then the oldest retire while age >= the manager's lifetime
//     (asked again here, +0x38 - the record's own +0x38 is not read);
//   * an object left with no decal at all leaves the active set.
// Always answers true (`mov al, 1`); the manager ignores it.
bool DecalObject::update(float deltaTime)
{
    if (!m_owner)
        m_visible = true;
    m_wasVisible = m_visible;
    if (!m_visible)
        deltaTime = deltaTime + deltaTime;

    for (unsigned int slot = m_mask & m_oldest; slot != m_insert; slot = (slot + 1u) & m_mask)
    {
        DecalRecord* const record = m_ring[slot];
        record->age = deltaTime + record->age;
    }

    const float lifetime = decalManager()->getTimeToLive();              // IDecalManager +0x38
    while (m_count > 0)
    {
        if (!(m_ring[m_oldest]->age >= lifetime))                          // `test ah, 1`: below
            break;                                                         // or unordered stays
        decal_manager_state::freeRecord(popRing());
    }

    if (m_staticCount + m_count == 0)
        decalManager()->removeInstance(this);                              // IDecalManager +0x28
    return true;
}


// add - +0x18, 0x101AE4B0 (941 B, retn 14h)

unsigned int DecalObject::add(const Mat4* transform, const IDecalTemplate* decalTemplate,
                              float scale, world::ICollisionMesh* collisionMesh,
                              world::CollisionFaceInfo* faceInfo)
{
    if (!decal_manager_state::decalsEnabled())                             // manager +0x2A6
        return 0;
    // An object nobody drew last frame takes static decals only.
    if (!m_wasVisible && m_owner && !decal_template_state::isStatic(decalTemplate))
        return 0;

    // The eviction runs before the static test: a static decal still costs the ring its oldest
    // entry when the ring is one short of full.
    if (m_count + 1u >= m_capacity)
        decal_manager_state::freeRecord(popRing());
    DecalRecord* const record = decal_manager_state::allocateRecord();
    record->vertices = 0;                                                  // unchecked record
    const bool firstDecal = !m_active && m_staticCount == 0 && m_count == 0;

    record->timeToLive = decalManager()->getTimeToLive();               // IDecalManager +0x38
    record->age = 0.0f;
    record->templateIndex = decal_template_state::index(decalTemplate);
    record->texture = decal_template_state::renderTexture(decalTemplate);
    record->alpha = 1.0f;

    // The colour: the template's colour times clamp(intensity * draw + 1, 0, 1), each channel
    // truncated (fistp, chop) and packed 0xFFrrggbb. Hex-Rays drops the clamp's answer; the
    // instruction stream uses it for all three channels.
    const float colorDraw = randomSymmetric();
    const float intensity = clampValue(
        decalTemplate->getRandomColorIntensity() * colorDraw + 1.0f, 0.0f, 1.0f);   // +0x1C
    const Vec4& color = decal_template_state::color(decalTemplate);
    const int red = static_cast<int>(intensity * color.x * 255.0f);
    unsigned int packed = (static_cast<unsigned int>(red) | 0xFFFFFF00u) << 8;
    const int green = static_cast<int>(intensity * color.y * 255.0f);
    packed = (packed | (static_cast<unsigned int>(green) & 0xFFu)) << 8;
    const int blue = static_cast<int>(intensity * color.z * 255.0f);
    packed |= static_cast<unsigned int>(blue) & 0xFFu;
    record->diffuse = packed;

    if (decal_template_state::animationRandomizedStartFrame(decalTemplate))
    {
        const int frameDraw = randomInteger();
        // (draw * (count - 1)) * 2^-24, the count unsigned: a zero count wraps to 2^32 - 1.
        record->startFrame = static_cast<float>(frameDraw)
                           * static_cast<float>(decal_template_state::animationFrameCount(decalTemplate) - 1u)
                           * 5.9604644775390625e-08f;
    }
    else
    {
        record->startFrame = 0.0f;
    }

    // The transform into the owner's space: local = transform * owner^-1, and the normal is the
    // transform's row 1 turned by the owner's inverse (3x3 only). Without an owner both stay world.
    Mat4 local = *transform;
    if (m_owner)
    {
        const Mat4& inverse = m_owner->getInvAbsoluteTransformation();    // IObject PE +0x84
        const Mat4 copy = local;
        local.mult(copy, inverse);
        Vec3 row1;
        row1.x = transform->m[4];
        row1.y = transform->m[5];
        row1.z = transform->m[6];
        transformVector(inverse, row1, record->normal);
    }
    else
    {
        record->normal.x = transform->m[4];
        record->normal.y = transform->m[5];
        record->normal.z = transform->m[6];
    }

    // The projected arm needs a HeightmapCluster (even for an object decal) and a max angle that is
    // not <= 0 (`test ah, 41h; jnp`: an unordered angle projects).
    if (particleHeightmapCluster() && !(decal_manager_state::maxProjectionAngle() <= 0.0f))
        buildProjected(record, &local, decalTemplate, scale, collisionMesh, faceInfo);
    else
        buildQuad(record, &local, decalTemplate, scale);

    // The original transform's translation - world space, even for an object decal.
    record->position.x = transform->m[12];
    record->position.y = transform->m[13];
    record->position.z = transform->m[14];

    if (record->vertexCount == 0)
    {
        decal_manager_state::freeRecord(record);
        return 0;
    }

    if (decal_template_state::isStatic(decalTemplate))
    {
        if (m_nextId >= 0xFFu)
            m_nextId = 0;
        ++m_nextId;
        record->staticSerial = static_cast<unsigned char>(m_nextId);
        pushStatic(record);
    }
    else
    {
        record->staticSerial = 0;
        pushRing(record);
    }

    if (firstDecal)
        decalManager()->addInstance(this);                                 // IDecalManager +0x24

    // Cache the first geometry up the owner's parent chain for the render pass (once).
    if (!m_ownerGeometry)
    {
        world::IObject* object = m_owner;
        if (object)
        {
            while (!world::object_detail::LayoutAccess::geometry(object))              // +0x38
            {
                object = world::object_detail::LayoutAccess::parent(object);          // +0x28
                if (!object)
                    return record->staticSerial;
            }
            m_ownerGeometry = world::object_detail::LayoutAccess::geometry(object);
        }
    }
    return record->staticSerial;
}


// buildQuad - 0x101AC920 (1,159 B, retn 10h). The quad arm: the unit quad rotated about Y, scaled,
// laid at the manager's surface offset, windowed into the template's texture, into the transform's
// space, and emitted as two triangles (0, 1, 2) (0, 2, 3) into a fresh six-vertex block.

void DecalObject::buildQuad(DecalRecord* record, const Mat4* transform,
                            const IDecalTemplate* decalTemplate, float scale)
{
    QuadSetup setup;
    setupQuad(setup, record, decalTemplate);

    const Mat4& m = *transform;
    for (int i = 0; i < 4; ++i)
    {
        const DecalVertex& source = s_quadTemplate[i];
        DecalVertex& corner = s_quad[i];
        corner.position.x = setup.cosine * source.position.x - setup.sine * source.position.z;
        corner.position.z = setup.sine * source.position.x + setup.cosine * source.position.z;
        const float size = (decalTemplate->getSize() + setup.randomSize) * scale;   // +0x0C per corner
        corner.position.x = size * corner.position.x;
        corner.position.y = size * corner.position.y;
        corner.position.z = size * corner.position.z;
        corner.position.y = decal_manager_state::surfaceOffset();          // replaces the scaled y
        corner.u = source.u;
        corner.v = source.v;
        const Vec2& extent = decal_template_state::uvExtent(decalTemplate);
        corner.u = (corner.u + 1.0f) * extent.x * 0.5f;
        corner.v = (corner.v + 1.0f) * extent.y * 0.5f;
        corner.u = setup.originU + corner.u;
        corner.v = setup.originV + corner.v;

        const float x = corner.position.x;
        const float y = corner.position.y;
        const float z = corner.position.z;
        const float tz = x * m.m[2] + z * m.m[10] + y * m.m[6] + m.m[14];
        const float ty = z * m.m[9] + x * m.m[1] + y * m.m[5] + m.m[13];
        const float tx = x * m.m[0] + z * m.m[8] + m.m[4] * y + m.m[12];
        corner.position.x = tx;
        corner.position.y = ty;
        corner.position.z = tz;
    }

    record->vertices = allocateVertices(6);                                // 0x78 bytes
    record->vertexCount = 0;
    // NO null guard: a failed allocation is written through, as shipped (0x101ACC3D..).
    record->vertices[record->vertexCount] = s_quad[0];
    ++record->vertexCount;
    record->vertices[record->vertexCount] = s_quad[1];
    ++record->vertexCount;
    record->vertices[record->vertexCount] = s_quad[2];
    ++record->vertexCount;
    record->vertices[record->vertexCount] = s_quad[0];
    ++record->vertexCount;
    record->vertices[record->vertexCount] = s_quad[2];
    ++record->vertexCount;
    record->vertices[record->vertexCount] = s_quad[3];
    ++record->vertexCount;
}


// buildProjected - 0x101ADF80 (1,315 B, retn 18h). The projected arm: the same quad (NOT laid at
// the surface offset - the lift comes later, along the projection direction) clipped onto the hit
// collision face and its neighbours, or onto the terrain cell under the transform.

// An object decal with no collision face - PlayerControlObject's driver tags pass (0, 0) - takes
// the terrain arm with its owner-space translation as world x / z. Shipped, kept.

void DecalObject::buildProjected(DecalRecord* record, const Mat4* transform,
                                 const IDecalTemplate* decalTemplate, float scale,
                                 world::ICollisionMesh* collisionMesh,
                                 world::CollisionFaceInfo* faceInfo)
{
    QuadSetup setup;
    setupQuad(setup, record, decalTemplate);

    const Mat4& m = *transform;
    for (int i = 0; i < 4; ++i)
    {
        const DecalVertex& source = s_quadTemplate[i];
        DecalVertex& corner = s_quad[i];
        corner.position.x = setup.cosine * source.position.x - setup.sine * source.position.z;
        corner.position.z = setup.sine * source.position.x + setup.cosine * source.position.z;
        const float size = (decalTemplate->getSize() + setup.randomSize) * scale;   // +0x0C per corner
        corner.position.x = size * corner.position.x;
        corner.position.y = size * corner.position.y;
        corner.position.z = size * corner.position.z;
        corner.u = source.u;
        corner.v = source.v;
        const Vec2& extent = decal_template_state::uvExtent(decalTemplate);
        corner.u = (corner.u + 1.0f) * extent.x * 0.5f;
        corner.v = (corner.v + 1.0f) * extent.y * 0.5f;
        corner.u = setup.originU + corner.u;
        corner.v = setup.originV + corner.v;

        const float x = corner.position.x;
        const float y = corner.position.y;
        const float z = corner.position.z;
        const float tz = m.m[10] * z + y * m.m[6] + m.m[2] * x + m.m[14];
        const float ty = m.m[9] * z + m.m[5] * y + m.m[1] * x + m.m[13];
        const float tx = m.m[0] * x + m.m[8] * z + y * m.m[4] + m.m[12];
        corner.position.x = tx;
        corner.position.y = ty;
        corner.position.z = tz;
    }

    const float maxAngle = decal_manager_state::maxProjectionAngle();      // manager +0x2C4
    s_clipPlanes.planes.resize(0);                                         // (0, Plane())
    s_visitedFaces.clear();
    record->vertexCount = 0;
    record->vertices = s_geometry;

    if (faceInfo && collisionMesh)
    {
        // `cmp al, 1`: exactly true.
        if (collisionMesh->hasLod(faceInfoLod(faceInfo)) == true)                          // +0x28
        {
            const Vec3* const vertices = static_cast<const Vec3*>(
                collisionMesh->getVertexVector(faceInfoLod(faceInfo)));                     // +0x40
            const DecalFace* const faces = static_cast<const DecalFace*>(
                collisionMesh->getFaceVector(faceInfoLod(faceInfo)));                       // +0x4C
            const int faceCount = collisionMesh->getFaceCount(faceInfoLod(faceInfo));      // +0x48
            Vec3 direction;                                                // -(row 1): into the surface
            direction.x = -m.m[4];
            direction.y = -m.m[5];
            direction.z = -m.m[6];
            projectOntoFace(faceInfoFace(faceInfo), direction, s_quad, record, maxAngle,
                            s_maxProjectionDepth, vertices, faces, faceCount,
                            collisionMesh, faceInfo);
        }
    }
    else
    {
        unsigned int face;
        if (getTerrainPatch(m.m[12], m.m[14], s_terrainVertices, &face))
        {
            Vec3 down;
            down.x = 0.0f;
            down.y = -1.0f;
            down.z = 0.0f;
            // The terrain arm skips the face-angle test (max angle 0) and has no mesh to ask for
            // neighbours: the static adjacency above stands in.
            projectOntoFace(face, down, s_quad, record, 0.0f, s_maxProjectionDepth,
                            s_terrainVertices, s_terrainFaces, 18, 0, 0);
        }
    }

    const unsigned int count = record->vertexCount;
    if (count == 0)
    {
        record->vertices = 0;
        return;
    }
    DecalVertex* const vertices = allocateVertices(count);
    record->vertices = vertices;
    if (!vertices)
    {
        // vertexCount keeps its value with a null block - the render pass would walk it. Shipped.
        ::renddx9abi::logf("DecalObject.cpp:370 Failed to allocate geometry mem");     // 0x172
        return;
    }
    std::memcpy(vertices, s_geometry, count * sizeof(DecalVertex));
}


// getTerrainPatch - 0x101ACDB0 (243 B, retn 10h). The 3 x 3 heightmap cells centred on (x, z), four
// corners each, into `vertices` (36); the face is the centre cell's (8, or 9 when the square is not
// flipped) - or the sentinel 0x1267 when the centre square did not answer, which reports false.
// Every cell is asked even after a miss; the cluster and its scale are fetched once.

bool DecalObject::getTerrainPatch(float x, float z, Vec3* vertices, unsigned int* face)
{
    world::IHeightmapCluster* const cluster = static_cast<world::IHeightmapCluster*>(
        static_cast<void*>(particleHeightmapCluster()));
    const Vec3& scale = cluster->getPrimaryScale();                        // PE +0x54
    const float stepX = scale.x;
    const float stepZ = scale.z;
    const float originX = x - stepX;
    unsigned int cell = 0;
    *face = 0x1267;
    const float originZ = z - stepZ;

    for (unsigned int row = 0; row < 3; ++row)
    {
        const float cellZ = static_cast<float>(row) * stepZ + originZ;
        Vec3* corners = vertices + row * 12;
        for (unsigned int column = 0; column < 3; ++column)
        {
            bool flipped;
            const float cellX = static_cast<float>(column) * stepX + originX;
            if (cluster->getSquareInWorldCoords(cellX, cellZ, corners, flipped) && cell == 4)   // +0x88
                *face = (flipped ? 0u : 1u) + 8u;
            ++cell;
            corners += 4;
        }
    }
    return *face != 0x1267;
}


// Flood-fills neighboring faces, clipping and projecting the quad with surface offset.
// Reaching 128 vertices clears the whole record at that recursion level; callers continue.
// Stops on depth exhaustion, complete containment, or clip rejection.

void DecalObject::projectOntoFace(unsigned int face, const Vec3& direction, const DecalVertex* quad,
                                  DecalRecord* record, float maxAngle, int depth,
                                  const Vec3* vertices, const DecalFace* faces, int faceCount,
                                  world::ICollisionMesh* collisionMesh,
                                  world::CollisionFaceInfo* faceInfo)
{
    s_visitedFaces.insert(static_cast<unsigned short>(face));
    s_clipPlanes.planes.resize(0);
    if (!buildClipPlanes(face, direction, maxAngle, vertices, faces, faceCount))
        return;

    // [CHOSEN] initialised: retail leaves the out-pointer unwritten when the clip rejects (-1), and
    // its first test below then compares that garbage with `quad` - either way the -1 test returns.
    const DecalVertex* clipped = 0;
    const int clippedCount = clipPolygon(quad, 4, s_clipPlanes, &clipped);
    if (clippedCount >= 3)
    {
        // The plane's constructor zeroes the normal only (0x101AD678); d is [CHOSEN] zero here -
        // retail leaves it unwritten, and it is read only under the flag, which writes it.
        Plane3 facePlane;
        facePlane.normal.x = 0.0f;
        facePlane.normal.y = 0.0f;
        facePlane.normal.z = 0.0f;
        facePlane.d = 0.0f;
        if (s_projectOntoFacePlane)
        {
            const DecalFace& corners = faces[face];
            planeAssign(facePlane, planeFromPoints(vertices[corners.v0], vertices[corners.v1],
                                                   vertices[corners.v2]));
        }
        const float lift = -decal_manager_state::surfaceOffset();          // manager +0x2C0
        Vec3 offset;
        offset.x = lift * direction.x;
        offset.y = lift * direction.y;
        offset.z = lift * direction.z;

        const unsigned int polygonCount = static_cast<unsigned int>(clippedCount);
        for (unsigned int i = 1; i < polygonCount - 1u; ++i)
        {
            emitVertex(record, clipped[0], facePlane, offset);
            emitVertex(record, clipped[i], facePlane, offset);
            emitVertex(record, clipped[i + 1u], facePlane, offset);
            if (record->vertexCount >= 0x80u)
            {
                record->vertexCount = 0;
                return;
            }
        }
    }

    if (depth <= 0)
        return;
    if (clipped == quad)
        return;
    if (clippedCount == -1)
        return;

    unsigned short neighbours[3];
    const unsigned short* adjacent;
    if (collisionMesh && faceInfo)
    {
        if (!collisionMeshAdjacentFaces(collisionMesh, faceInfoGeometry(faceInfo),
                                        faceInfoLod(faceInfo), face, neighbours))  // PE +0x38
            return;
        adjacent = neighbours;
    }
    else
    {
        adjacent = s_terrainAdjacency[face];
    }
    for (int k = 0; k < 3; ++k)
    {
        const unsigned short next = adjacent[k];
        if (next != 0xFFFFu && s_visitedFaces.find(next) == s_visitedFaces.end())
            projectOntoFace(next, direction, quad, record, maxAngle, depth - 1, vertices, faces,
                            faceCount, collisionMesh, faceInfo);
    }
}


// buildClipPlanes - 0x101AD390 (589 B, retn 18h). The face's plane; when a max angle is given
// (> 0) the face is refused when its normal is more than that many degrees from the projection
// direction (the cosine clamped twice before acos, as shipped); then the three edge planes
// normalise(edge x normal) - pointing out of the triangle for the face winding - are pushed. The
// three d's use different association orders, kept. faceCount is not read.

bool DecalObject::buildClipPlanes(unsigned int face, const Vec3& direction, float maxAngle,
                                  const Vec3* vertices, const DecalFace* faces, int faceCount)
{
    const DecalFace& corners = faces[face];
    const Vec3& p0 = vertices[corners.v0];
    const Vec3& p1 = vertices[corners.v1];
    const Vec3& p2 = vertices[corners.v2];
    const Plane3 facePlane = planeFromPoints(p0, p1, p2);
    const Vec3& normal = facePlane.normal;

    if (maxAngle > 0.0f)                                                   // `test ah, 41h; jnz`
    {
        const float cosine = clampValue(
            clampValue(normal.z * direction.z + normal.y * direction.y + normal.x * direction.x,
                       -1.0f, 1.0f),
            -1.0f, 1.0f);
        const double angle = std::acos(static_cast<double>(cosine));       // _CIacos
        if (static_cast<double>(s_degreesToRadians * maxAngle) < angle)    // unordered keeps it
            return false;
    }

    Plane3 edge0;
    edge0.normal = edgeNormal(p0, p1, normal);
    edge0.d = -(edge0.normal.x * p0.x + edge0.normal.y * p0.y + edge0.normal.z * p0.z);
    Plane3 edge1;
    edge1.normal = edgeNormal(p1, p2, normal);
    edge1.d = -(edge1.normal.x * p1.x + edge1.normal.z * p1.z + edge1.normal.y * p1.y);
    Plane3 edge2;
    edge2.normal = edgeNormal(p2, p0, normal);
    edge2.d = -(edge2.normal.x * p2.x + edge2.normal.z * p2.z + edge2.normal.y * p2.y);

    s_clipPlanes.planes.push_back(edge0);
    s_clipPlanes.planes.push_back(edge1);
    s_clipPlanes.planes.push_back(edge2);
    return true;
}


// Clips against planes and returns the vertex count, with output pointing to input or
// scratch storage. Returns -1 for a near-parallel crossing. The zero-seeded rejection
// mask never rejects early; wholly outside polygons are clipped down to zero vertices.

int DecalObject::clipPolygon(const DecalVertex* input, unsigned int count,
                             const DecalClipPlanes& clip, const DecalVertex** output)
{
    const std::vector<Plane3>& planes = clip.planes;
    if (planes.empty())
    {
        *output = input;
        return static_cast<int>(count);
    }

    unsigned char* codes = s_outcodes[0];
    unsigned char* nextCodes = s_outcodes[1];
    for (unsigned int i = 0; i < count; ++i)
        codes[i] = computeOutcode(input[i].position, planes);

    unsigned char anyOutside = 0;
    for (unsigned int p = 0; p < planes.size(); ++p)
    {
        const unsigned char bit = static_cast<unsigned char>(1u << p);
        unsigned char allOutside = 0;                                      // see the header note
        for (unsigned int i = 0; i < count; ++i)
        {
            const unsigned char code = static_cast<unsigned char>(codes[i] & bit);
            anyOutside = static_cast<unsigned char>(anyOutside | code);
            allOutside = static_cast<unsigned char>(allOutside & code);
        }
        if (allOutside)
            return -1;
    }
    if (!anyOutside)
    {
        *output = input;
        return static_cast<int>(count);
    }

    DecalVertex* destination = s_clipBuffer[0];
    const DecalVertex* source = input;
    unsigned int current = count;
    for (unsigned int p = 0; p < planes.size(); ++p)
    {
        const unsigned char bit = static_cast<unsigned char>(1u << p);
        const Plane3& plane = planes[p];
        DecalVertex* out = destination;
        unsigned int produced = 0;
        for (unsigned int j = 0; j < current; ++j)
        {
            const DecalVertex& a = source[j];
            const unsigned char codeA = codes[j];
            if (!(bit & codeA))
            {
                nextCodes[produced] = codeA;
                *out = a;
                ++produced;
                ++out;
            }
            const unsigned int k = (j + 1u) % current;
            if ((codeA & bit) != (codes[k] & bit))
            {
                const DecalVertex& b = source[k];
                const float dx = b.position.x - a.position.x;
                const float dy = b.position.y - a.position.y;
                const float dz = b.position.z - a.position.z;
                const float du = b.u - a.u;
                const float dv = b.v - a.v;
                const float denominator =
                    dz * plane.normal.z + dx * plane.normal.x + dy * plane.normal.y;
                if (std::fabs(denominator) < 1.1920928955078125e-07f)
                    return -1;
                const float distance = a.position.z * plane.normal.z + plane.normal.y * a.position.y
                                     + a.position.x * plane.normal.x + plane.d;
                const float t = distance * (-1.0f / denominator);
                const float stepY = dy * t;
                const float stepZ = dz * t;
                out->position.x = dx * t + a.position.x;
                out->position.y = stepY + a.position.y;
                out->position.z = stepZ + a.position.z;
                const float stepU = du * t;
                const float stepV = dv * t;
                out->u = stepU + a.u;
                out->v = stepV + a.v;
                nextCodes[produced] = computeOutcode(out->position, planes);
                ++produced;
                ++out;
            }
        }
        source = destination;
        destination = (destination == s_clipBuffer[0]) ? s_clipBuffer[1] : s_clipBuffer[0];
        codes = nextCodes;
        nextCodes = (codes == s_outcodes[0]) ? s_outcodes[1] : s_outcodes[0];
        current = produced;
    }
    *output = source;
    return static_cast<int>(current);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
