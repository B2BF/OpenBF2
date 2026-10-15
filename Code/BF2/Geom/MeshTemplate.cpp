// Shared mesh-template lifecycle and static/bundled/skinned mesh loading.
// Manager creation initializes keepData; file loading initializes the header fields.
// Reads and allocations are unchecked, so callers must provide valid mesh content.
// Buffer construction frees staging allocations without nulling them; destruction does not
// free them again, and repeated/failed loads retain the existing lifetime quirks.

#ifdef BF2_IMAGE_RENDDX9

#include "MeshTemplate.h"

#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../IO/Stream/IStream.h"

#include <math.h>
#include <sstream>
#include <stdexcept>
#include <string.h>

namespace dice {
namespace hfe {


// The dx9rend seams this file calls. Declared locally rather than by including RendDX9Base.h, the
// same way DebugLineGraphD3d.cpp / FSQuadDrawer.cpp / SwiffFont.cpp do: RendDX9Base.h drags in
// windows.h and d3d9.h and this TU needs none of it. Signatures are copied verbatim from
// BufferManager.cpp and BF2Render.cpp so the mangled names match.

namespace dx9rend {

extern void* g_bufferManager;                                            // dword_1023B978

void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const std::string& name, unsigned int usage);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);                                // 0x10014510 + the
                                                                         //   caller-side --ref
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  indexBufferRelease(void* wrapper);                                 // 0x10014300 + --ref

void* vertexDeclarationCreate(const void* elements);
void  vertexDeclarationRelease(void* wrapper);                           // 0x10014790 + --ref

unsigned int rendererFrameOrdinal();                                     // g_rendererBase +0x54

} // namespace dx9rend

namespace geom {

namespace {


// The three interface ids queryInterface (0x100EE940) compares against. The shipped body reads them
// out of data - dword_102088DC, dword_1021AE5C and dword_10208DC0 - rather than as immediates, which
// is why they are named constants here too. IID_IGeometryTemplate agrees with the value
// GeometryTemplateManager.cpp already carries.

const unsigned int kIID_IBase             = 1u;             // dword_102088DC
const unsigned int kIID_IGeometryTemplate = 0x17701FFFu;    // dword_1021AE5C  (393224191)
const unsigned int kIID_IConsoleSaveable  = 923378943u;     // dword_10208DC0  (0x3709A4FF)

// getClassId, a bare `mov eax, 870CA650h`.
const unsigned int kCID_MeshTemplate = 0x870CA650u;

// The `Meshes/` sub-directory readHeader splices between the template's path and its name. In the
// PE this is a file-scope `const std::string` at 0x10263DD4, constructed by the static initialiser
// at 0x101D5760 and shared with at least SkinnedMeshTemplate::load (0x1010E840) and three other
// bodies; the concatenation there is `path + <this> + name`. Held as a literal rather than as a
// namespace-scope std::string because only the bytes are observable and a dynamically initialised
// string object in a DLL would add a static-init ordering dependency this file does not need.
const char kMeshesSubDirectory[] = "Meshes/";


// Returns unit scale for the base template's uncompressed positions.

const Vec3& unitScale()
{
    // Constant-initialised, so the current toolchain emits no guard. The retail guard exists only
    // because dice wrote a constructor call; the published value is identical.
    static Vec3 value = { 1.0f, 1.0f, 1.0f };
    return value;
}



// sub_100E7180 - corner `index` of the axis-aligned box, as three separate float stores into the
// caller's Vec3. Bit 2 of the index selects max.x, bit 1 max.y, bit 0 max.z; an out-of-range index
// yields (0, 0, 0). Transcribed here because the shipped body sits outside this file's address
// range (0x100E7180) - it is identical-code-folded with whatever else asks a box for a corner.

void meshBoundsCorner(const MeshBounds& bounds, Vec3& out, int index)
{
    switch (index)
    {
    case 0: out.x = bounds.min.x; out.y = bounds.min.y; out.z = bounds.min.z; break;
    case 1: out.x = bounds.min.x; out.y = bounds.min.y; out.z = bounds.max.z; break;
    case 2: out.x = bounds.min.x; out.y = bounds.max.y; out.z = bounds.min.z; break;
    case 3: out.x = bounds.min.x; out.y = bounds.max.y; out.z = bounds.max.z; break;
    case 4: out.x = bounds.max.x; out.y = bounds.min.y; out.z = bounds.min.z; break;
    case 5: out.x = bounds.max.x; out.y = bounds.min.y; out.z = bounds.max.z; break;
    case 6: out.x = bounds.max.x; out.y = bounds.max.y; out.z = bounds.min.z; break;
    case 7: out.x = bounds.max.x; out.y = bounds.max.y; out.z = bounds.max.z; break;
    default: out.x = 0.0f; out.y = 0.0f; out.z = 0.0f; break;
    }
}

} // anonymous namespace


// Expands bounds to include point, testing each maximum before its minimum.
// Always marks derived bounds dirty.
void meshBoundsExpand(MeshBounds& bounds, const Vec3& point)
{
    if (point.x > bounds.max.x) bounds.max.x = point.x;
    if (point.x < bounds.min.x) bounds.min.x = point.x;
    if (point.y > bounds.max.y) bounds.max.y = point.y;
    if (point.y < bounds.min.y) bounds.min.y = point.y;
    if (point.z > bounds.max.z) bounds.max.z = point.z;
    if (point.z < bounds.min.z) bounds.min.z = point.z;
    bounds.dirty = 1;
}

// derive originRadius, center, radius and extent from min/max, then clear dirty.

// Both radii are the largest corner distance, not half a diagonal, and both loops run all eight
// corners through the same helper. originRadius is measured from the origin and is computed before
// the centre exists; radius is measured from the centre. The squared maximum is accumulated in a
// float and only the final value goes through sqrt, exactly as shipped.
void meshBoundsFinalize(MeshBounds& bounds)
{
    float longest = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        Vec3 corner;
        meshBoundsCorner(bounds, corner, i);
        const float d = corner.x * corner.x + corner.y * corner.y + corner.z * corner.z;
        if (d > longest)
            longest = d;
    }
    bounds.originRadius = static_cast<float>(::sqrt(static_cast<double>(longest)));

    longest = 0.0f;
    bounds.center.x = (bounds.max.x + bounds.min.x) * 0.5f;
    bounds.center.y = (bounds.max.y + bounds.min.y) * 0.5f;
    bounds.center.z = (bounds.max.z + bounds.min.z) * 0.5f;

    for (int i = 0; i < 8; ++i)
    {
        Vec3 corner;
        meshBoundsCorner(bounds, corner, i);
        const float dx = corner.x - bounds.center.x;
        const float dy = corner.y - bounds.center.y;
        const float dz = corner.z - bounds.center.z;
        const float d = dz * dz + dy * dy + dx * dx;
        if (d > longest)
            longest = d;
    }
    bounds.radius = static_cast<float>(::sqrt(static_cast<double>(longest)));

    bounds.extent.x = bounds.max.x - bounds.min.x;
    bounds.extent.y = bounds.max.y - bounds.min.y;
    bounds.extent.z = bounds.max.z - bounds.min.z;
    bounds.dirty = 0;
}

// the OLD on-disk form: one 36-byte block straight over min, max, the two radii and
// the dirty dword, then dirty is forced back to 1. Nothing is finalised and the read result is
// ignored; the answer is an unconditional true.
bool meshBoundsReadLegacy(MeshBounds& bounds, renddx9abi::HostStream* stream)
{
    stream->read(&bounds, 36);
    bounds.dirty = 1;
    return true;
}

// the current form: min then max as two separate 12-byte reads, dirty, then finalize.
// Two reads rather than one 24-byte read, as shipped.
bool meshBoundsReadMinMax(MeshBounds& bounds, renddx9abi::HostStream* stream)
{
    stream->read(&bounds.min, 12);
    stream->read(&bounds.max, 12);
    bounds.dirty = 1;
    meshBoundsFinalize(bounds);
    return true;
}

// field-by-field copy. It copies the dirty byte at +0x20 and skips the three padding
// bytes after it; the source's +0x21..+0x23 are not carried over. Preserved.
void meshBoundsCopy(MeshBounds& dst, const MeshBounds& src)
{
    dst.min.x = src.min.x;
    dst.min.y = src.min.y;
    dst.min.z = src.min.z;
    dst.max.x = src.max.x;
    dst.max.y = src.max.y;
    dst.max.z = src.max.z;
    dst.originRadius = src.originRadius;
    dst.radius = src.radius;
    dst.dirty = src.dirty;
    dst.extent.x = src.extent.x;
    dst.extent.y = src.extent.y;
    dst.extent.z = src.extent.z;
    dst.center.x = src.center.x;
    dst.center.y = src.center.y;
    dst.center.z = src.center.z;
}


// Initializes ownership and container state. keepData is assigned by the factory;
// file-header fields remain unset until loading, and the VC7.1 vectors' allocator words are
// untouched.

MeshTemplate::MeshTemplate()
{
    m_dword08 = 0;
    m_refCount = 1;
    // m_name (+0x14), m_path (+0x30), the three per-geom vectors (+0x50, +0x60, +0x70) and the
    // material-value map (+0x80) are default-constructed by the compiler here; the retail body
    // calls std::string::string() on the two strings in the same places, zeroes the vectors and
    // builds the map's head node (sub_100EB100) after them.
    m_loaded = 0;

    m_maxSkip3pLods    = 1;
    m_maxSkipWreckLods = 1;

    m_dword8C           = 0;
    m_vertexData        = 0;
    m_vertexBuffer      = 0;
    m_indexData         = 0;
    m_indexBuffer       = 0;
    m_vertexDeclaration = 0;
    m_u2                = 8;
}


// Releases wrappers, material map, containers, and strings in that order.
// Wrapper references must be valid. Staging vertex/index buffers are released by their build
// steps, not here. Called through the destroy slot, which then frees the storage without running
// a C++ destructor - so the std members are destroyed explicitly here.

void MeshTemplate::destroyMembers()
{
    // `if (p) { if (!--p[1]) { <destroy>; engineFree(p); } }` three times over. The inner half is
    // what the three dx9rend release seams carry; the outer null test stays here, where the PE has
    // it - the seams themselves do not test.
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    if (m_indexBuffer)
        dx9rend::indexBufferRelease(m_indexBuffer);
    if (m_vertexDeclaration)
        dx9rend::vertexDeclarationRelease(m_vertexDeclaration);

    // ~_Tree: erase [begin(), end()) (sub_100EBD80), then free the header.
    m_materialValues.~MaterialValueMap();

    // The three vectors, last first; the lod-distance vector's inner vectors go before its buffer
    // (sub_100ED1C0).
    m_geomInts.~GeomIntVector();
    m_lodDistances.~LodDistanceVector();
    m_lodCounts.~GeomIntVector();

    m_path.~basic_string();
    m_name.~basic_string();
}

// the deleting destructor in vtable slot +0xB0: run the body, then free the object
// only when bit 0 of the flags is set, and return `this` either way.
void* MeshTemplate::destroy(unsigned char flags)
{
    destroyMembers();
    if ((flags & 1) != 0)
        ::renddx9abi::engineFree(this);
    return this;
}


// IBase. The base counts at +0x10; StaticMeshTemplate overrides all four and counts at +0xC8.


// 0x1010DE10 - `++*(this + 0x10)`. The result is materialised but IBase declares the slot void.
void MeshTemplate::addRef()
{
    ++m_refCount;
}


int MeshTemplate::getRef()
{
    return m_refCount;
}

// decrement, and on zero dispatch through the virtual deleting-destructor slot +0xB0
// with flags 1. The new count is returned in both arms.
int MeshTemplate::release()
{
    const int count = --m_refCount;
    if (count == 0)
        destroy(1);
    return count;
}

// IID_IBase and IID_IGeometryTemplate answer with the primary subobject;
// IID_IConsoleSaveable answers with the one at +0x0C. The `test eax, eax` before `add eax, 0Ch` is
// MSVC's own null guard on the base adjust - reproduced by writing the cast, not by hand.
void* MeshTemplate::queryInterface(unsigned int iid) const
{
    if (iid != kIID_IBase && iid != kIID_IGeometryTemplate)
    {
        if (iid == kIID_IConsoleSaveable)
            return static_cast<io::IConsoleSaveable*>(const_cast<MeshTemplate*>(this));
        return 0;
    }
    return const_cast<MeshTemplate*>(this);
}


// Identity, name and path.


unsigned int MeshTemplate::getClassId() const
{
    return kCID_MeshTemplate;
}

// four one-line accessors over the two strings.
void MeshTemplate::setName(const std::string& name)
{
    m_name = name;
}

const std::string& MeshTemplate::getName() const
{
    return m_name;
}

void MeshTemplate::setPath(const std::string& path)
{
    m_path = path;
}

const std::string& MeshTemplate::getPath() const
{
    return m_path;
}

// the base never instantiates anything; the three concrete templates override this.
void* MeshTemplate::createInstance()
{
    return 0;
}


bool MeshTemplate::isLoaded() const
{
    return m_loaded != 0;
}

// the base has no file of its own to open, so it refuses. StaticMesh/BundledMesh/
// SkinnedMesh each override this with their own path builder and loadFromStream call.
bool MeshTemplate::load()
{
    return 0;
}


// The default table. Every body below is a shared one - the linker folded each with the identical
// default in some other class, which is why their addresses sit far outside this file's range. They
// are written out here as ordinary members because that is what the source they were folded from
// looked like: the constants and empty bodies of a base that implements nothing.

// The slot arities are instruction-proven: each default's `retn N` fixes its argument count, and the
// StaticMeshTemplate overrides agree slot for slot. See MeshTemplate.h's table.


void MeshTemplate::setLodDistance(int, int, float) { }                        // nullsub_33 0x10083AB0
float MeshTemplate::getLodDistance(int, int) { return -1.0f; }
void MeshTemplate::setMaterialValue(int, int, unsigned char, float) { }       // nullsub_32 0x10083A80
float MeshTemplate::getMaterialValue(int, int, unsigned char) { return 15.0f; }
void MeshTemplate::slot_0x40(int, int, int, int) { }                          // nullsub_32 0x10083A80
float MeshTemplate::slot_0x44(int, int, int) { return 0.25f; }
void MeshTemplate::slot_0x48(int) { }                                         // nullsub_53 0x100E13B0
bool MeshTemplate::slot_0x4C() { return 0; }
bool MeshTemplate::isWater() const { return 0; }
unsigned int MeshTemplate::getMemoryUsage(int) { return 0; }
int MeshTemplate::getPrimitiveCount(int, int, int) { return 0; }
int MeshTemplate::getVertexCount(int, int, int) { return 0; }
void MeshTemplate::setDoNotGenerateLightmaps(bool) { }                        // nullsub_53 0x100E13B0
bool MeshTemplate::getDoNotGenerateLightmaps() const { return 0; }
void MeshTemplate::setLightmapOverride(bool) { }                              // nullsub_53 0x100E13B0
bool MeshTemplate::getLightmapOverride() const { return 0; }
void MeshTemplate::setLightmapSize(int, int, int) { }                         // nullsub_33 0x10083AB0
int MeshTemplate::getLightmapSize(int, int) { return 0; }
void MeshTemplate::setCompressVertexData(bool) { }                            // nullsub_53 0x100E13B0
bool MeshTemplate::getCompressVertexData() const { return 1; }
void MeshTemplate::slot_0x80(int) { }                                         // nullsub_53 0x100E13B0
bool MeshTemplate::slot_0x84() { return 0; }
void MeshTemplate::setMaxTextureRepeat(int) { }                               // nullsub_53 0x100E13B0
int MeshTemplate::getMaxTextureRepeat() const { return 16; }
void MeshTemplate::slot_0x90(int) { }                                         // nullsub_53 0x100E13B0
bool MeshTemplate::slot_0x94() { return 0; }
void MeshTemplate::slot_0x98(int) { }                                         // nullsub_53 0x100E13B0
bool MeshTemplate::slot_0x9C() { return 0; }
int MeshTemplate::getMaterialCount(unsigned int) { return 0; }
int MeshTemplate::getLodPrimitiveCount(unsigned int) { return 0; }

// copy the shared unit vector into the caller's buffer and hand the buffer back.
// THE input IS ignored. The body never touches its second argument; the base class stores no
// compressed positions, so the "decompression" it publishes is a constant. Preserved as read.
Vec3* MeshTemplate::decompressPosition(Vec3* out, const Vec3*)
{
    const Vec3& scale = unitScale();
    out->y = scale.y;
    out->x = scale.x;
    out->z = scale.z;
    return out;
}

// stamp the renderer's current frame ordinal into +0x08 and return it. The retail call
// is g_rendererBase vslot +0x54 (getFrameOrdinal, 0x1000CB10); this tree's typed accessor for that
// slot is dx9rend::rendererFrameOrdinal(). Nothing else in the base releases a device resource -
// the vertex/index buffers survive a device reset through BufferManager's own listener.
void MeshTemplate::releaseDeviceResources()
{
    m_dword08 = static_cast<int>(dx9rend::rendererFrameOrdinal());
}


// Loads through virtual readers, then builds the buffers. Trailer parsing precedes LODs
// and materials. Only readHeader can report failure; later results do not stop the load.

bool MeshTemplate::loadFromStream(renddx9abi::HostNonlinearStream* stream)
{
    if (!readHeader(stream))
        return false;

    readGeometry(stream);
    readTrailer(stream);
    readLods(stream);
    readMaterials(stream);
    mergeMaterials();
    buildVertexBuffers();
    buildIndexBuffer();
    finalizeMaterials();
    return true;
}


// Reads the header/version and consumes three reserved words and a reserved byte.
// Returns false and logs the mesh path when the header is invalid.

bool MeshTemplate::readHeader(renddx9abi::HostNonlinearStream* stream)
{
    stream->read(&m_head, 4);
    stream->read(&m_version, 4);

    if (m_version >= 3u)
    {
        // [UNRESOLVED] three dwords and one byte that the base reads and discards. The concrete
        // templates do not re-read them, so whatever they mean is carried by nothing in 1.50.
        unsigned int  unused0 = 0;
        unsigned int  unused1 = 0;
        unsigned int  unused2 = 0;
        unsigned char unused3 = 0;
        stream->read(&unused0, 4);
        stream->read(&unused1, 4);
        stream->read(&unused2, 4);
        stream->read(&unused3, 1);
        return true;
    }

    // `std::string(m_path) += "Meshes/"` then `+= m_name` - two operator+ temporaries in the PE,
    // one string here; the bytes are identical.
    std::string full(m_path);
    full += kMeshesSubDirectory;
    full += m_name;

    ::renddx9abi::logf("MeshTemplate.cpp:160: Cannot load Mesh %s Wrong version %u."
                       " Should be at least 3. Please make sure it exists and reexport!",
                       full.c_str(), m_version);
    return false;
}


// Reads geometry counts, declarations, vertex data and indices.
// Requires valid file sizes; counts and allocation results are unchecked.

void MeshTemplate::readGeometry(renddx9abi::HostNonlinearStream* stream)
{
    unsigned int geomCount = 0;
    stream->read(&geomCount, 4);

    m_lodCounts.resize(geomCount, 0);                                 // sub_10192790
    m_geomInts.resize(geomCount, 0);

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        stream->read(&m_lodCounts[geom], 4);
        m_geomInts[geom] = 0;
    }

    unsigned int attributeCount = 0;
    stream->read(&attributeCount, 4);

    unsigned char* const attributes =
        static_cast<unsigned char*>(::renddx9abi::engineAlloc(8 * attributeCount));
    unsigned char* cursor = attributes;
    for (unsigned int i = 0; i < attributeCount; ++i)
    {
        stream->read(cursor, 8);
        cursor += 8;
    }

    m_vertexDeclaration = dx9rend::vertexDeclarationCreate(attributes);
    ::renddx9abi::engineFree(attributes);

    stream->read(&m_primitiveType, 4);
    stream->read(&m_vertexStride, 4);
    stream->read(&m_vertexCount, 4);

    readVertices(stream);

    stream->read(&m_indexCount, 4);
    m_indexData = static_cast<unsigned short*>(::renddx9abi::engineAlloc(2 * m_indexCount));
    stream->read(m_indexData, 2 * m_indexCount);
}

// the base's vertex block is raw: stride * count bytes into +0x90, verbatim. The
// product is computed twice, once for the allocation and once for the read length, exactly as
// shipped (stride first in both).
void MeshTemplate::readVertices(renddx9abi::HostNonlinearStream* stream)
{
    m_vertexData = ::renddx9abi::engineAlloc(m_vertexStride * m_vertexCount);
    stream->read(m_vertexData, m_vertexStride * m_vertexCount);
}

// The base has no material table to merge; derived templates override this no-op.
void MeshTemplate::mergeMaterials()
{
}


// Uploads staging vertices into a write-only buffer and frees the staging block.
// Creation is not null-checked. Lock counts are vertices, not bytes; the freed staging
// pointer is deliberately left unchanged.

void MeshTemplate::buildVertexBuffers()
{
    m_vertexBuffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager,
                                                 m_vertexCount, m_vertexStride,
                                                 std::string("MeshTemplate"), 8);

    void* const locked = dx9rend::vertexBufferLock(m_vertexBuffer, 0, m_vertexCount, 0);
    if (locked)
    {
        ::memcpy(locked, m_vertexData, m_vertexStride * m_vertexCount);
        dx9rend::vertexBufferUnlock(m_vertexBuffer);
    }
    ::renddx9abi::engineFree(m_vertexData);
}


// the same dance for the indices. usage 8 = D3DUSAGE_WRITEONLY, format 101 =
// D3DFMT_INDEX16, which is why the byte count is 2 * indexCount everywhere. The lock is in index
// units. Same three missing guards as buildVertexBuffers.

void MeshTemplate::buildIndexBuffer()
{
    m_indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(m_indexCount, 8, 101);

    void* const locked = dx9rend::indexBufferLock(m_indexBuffer, 0, m_indexCount, 0);
    if (locked)
    {
        ::memcpy(locked, m_indexData, 2 * m_indexCount);
        dx9rend::indexBufferUnlock(m_indexBuffer);
    }
    ::renddx9abi::engineFree(m_indexData);
}

// The base has no material finalization work.
void MeshTemplate::finalizeMaterials()
{
}

// four bytes into +0xBC, immediately after the index block. Constructed as 8.
void MeshTemplate::readTrailer(renddx9abi::HostNonlinearStream* stream)
{
    stream->read(&m_u2, 4);
}

// The base parses no LOD or material tables; derived templates override these no-ops.
void MeshTemplate::readLods(renddx9abi::HostNonlinearStream*)
{
}

void MeshTemplate::readMaterials(renddx9abi::HostNonlinearStream*)
{
}


// The two lod-skip properties and their console emitter.


// plain field access at +0xC0 and +0xC4. The
// two setters are folded with every other "store the argument at this offset" body in the image.
void MeshTemplate::setMaxSkip3pLods(int value)
{
    m_maxSkip3pLods = value;
}

int MeshTemplate::getMaxSkip3pLods() const
{
    return m_maxSkip3pLods;
}

void MeshTemplate::setMaxSkipWreckLods(int value)
{
    m_maxSkipWreckLods = value;
}

int MeshTemplate::getMaxSkipWreckLods() const
{
    return m_maxSkipWreckLods;
}


// Writes nondefault template properties as unsigned console values.
// Returns whether the stream accepted every output byte; an unchanged template succeeds empty.

bool MeshTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;

    if (m_maxSkip3pLods != 1)
        out << "GeometryTemplate.maxSkip3pLods "
            << static_cast<unsigned int>(m_maxSkip3pLods) << std::endl;

    const int wreckLods = m_maxSkipWreckLods;
    if (wreckLods != 1)
        out << "GeometryTemplate.maxSkipWreckLods "
            << static_cast<unsigned int>(wreckLods) << std::endl;

    const std::string script = out.str();
    const unsigned int length = static_cast<unsigned int>(script.size());
    return stream->write(script.c_str(), length) == length;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
