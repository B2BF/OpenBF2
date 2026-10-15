// Path:     [ATTESTED] Physics/CollisionMeshTemplate.cpp in both dedicated PE and ELF diagnostics.

#include "CollisionMeshTemplate.h"

#include <cfloat>                                 // FLT_EPSILON - engine normalisation
#include <cmath>                                  // sqrtf / fabsf - buildEdgeInfo
#include <cstdlib>                                // abort - the loader's shipped hard-fails
#include <cstring>                                // memset
#include <cstdio>                                 // debug-template name suffix
#include <map>                                    // debug faces grouped by material
#include "../Core/Debug.h"
#include "../Core/Ids.h"
#include "../Core/SettingsRepostitory.h"         // settings->boolGet("keepAINav") - dice's spelling
#include "../Game/Objects/ObjectTemplate.h"      // getMaterialVector (+0xBC) - createMaterialMapping
#include "../Game/Utility/Vars.h"                 // the load tail's two debug-mesh cells
#include "../Geom/IGeometryTemplate.h"
#if !DEDICATED && defined(_M_IX86)
#include "../Geom/GeometryTemplateManager.h"
#endif
#include "../IO/Stream/IStream.h"                // INonlinearStream - the parser's input
#include "CollisionManager.h"                    // ICollisionManager::drawTriangle/drawLine (debug)
#include "GeomUtil.h"                            // checkCapsuleEdgesCollision - the capsule query

namespace dice {
namespace hfe {
#if !DEDICATED
extern IBase* g_geometryTemplateManager;           // Game/Main/BF2Render.cpp
#endif
namespace geom {
// Type name attested by ELF 0x83A6250/0x83A63D0. Field names are [INFERRED]; both engine
// builders and RendDX9 0x10093590/0x10093330 use two ints followed by one Vec3 (20 bytes).
struct DebugXMeshFaceColorInfo
{
    int firstIndex;
    int indexCount;
    Vec3 color;
};
static_assert(sizeof(DebugXMeshFaceColorInfo) == 20, "debug face-color record width");
}
namespace world {

// Initializes LOD bounds, BSP, empty containers, and null debug/adjacency storage.
CollisionLod::CollisionLod()
{
    maxBoundingRadius = 0.0f;
    debugMesh = dwordB0 = dwordB4 = 0;
    faceAdjacency = 0;
}

// Deletes adjacency storage before automatic member teardown. Debug references must
// be released separately by CollisionMeshTemplate::destroyDebugMeshes.
CollisionLod::~CollisionLod()
{
    delete[] faceAdjacency;
}

// Shared query settings and temporary face/debug vectors; collision queries are not reentrant.
// [CHOSEN] static values replace value/pointer pairs that are never rebound.
// The unused setting's purpose remains [UNRESOLVED].
namespace {
bool  s_debugDrawQueries      = false;
bool  s_sphereComputedNormal  = true;
bool  s_sphereExpandedEdges   = true;
bool  s_sphereFacePass        = true;
bool  s_debugDrawHitFace      = false;
float s_sphereStationarySpeed = 0.01f;
float s_edgeMaxNormalY        = 0.86602539f;
} // namespace
std::vector<const BspNode::Face*> CollisionMeshTemplate::m_temporaryFaces;

// [INFERRED names] - both vectors are constructed and destroyed by this TU in the ELF and PE.
// ResponsePhysicsManager::checkSoldierObjectVsObjects clears them at the start of every soldier
// check; CollisionManager::drawDebugPrimitives consumes them when the matching debug switch is on.
std::vector<Vec3> collisionTestedFaces;
std::vector<Vec3> collisionIntersectFaces;

// , read in full: the vptr, both strings empty, everything else zero.
CollisionMeshTemplate::CollisionMeshTemplate()
{
    // m_lods (+0x04..+0x0F): the three zeroed words = an empty std::vector<CollisionGeom>
    m_field10 = 0;
    m_materialIndex = 0;
    m_materialCount = 0;
    m_field1C = 0;
    m_ownerTemplate = 0;
    m_loaded = 0;
    m_loadRequested = 0;
    m_dataSize = 0;
    m_versionToLoad = 0;
    m_versionToLoad2 = 0;
}

// Destroys debug meshes before releasing owned geometry and LOD resources.
CollisionMeshTemplate::~CollisionMeshTemplate()
{
    destroyDebugMeshes(false);
    for (unsigned int geom = 0; geom < m_lods.size(); ++geom)
    {
        if (!isGeomValid(static_cast<int>(geom)))
            continue;
        CollisionGeom& g = m_lods[geom];
        for (unsigned int lod = 0; lod < g.lods.size(); ++lod)
            delete getValidLodRaw(static_cast<int>(geom), static_cast<int>(lod));
    }
    // Both ports use scalar delete here, despite createMaterialMapping's new unsigned short[].
    // Preserve the shipped mismatch; do not replace it with delete[].
    delete m_materialIndex;
    m_materialIndex = 0;
}

// Lazily builds a shared u16 material-ID remap from the object template's material vector.
// Null/empty vectors leave it unbuilt; later callers reuse the first nonempty mapping.
void CollisionMeshTemplate::createMaterialMapping(const IObjectTemplate* objectTemplate)
{
    if (m_materialIndex)
        return;

    std::vector<std::pair<std::string, unsigned int> >* materials =
        const_cast<ObjectTemplate*>(
            static_cast<const ObjectTemplate*>(objectTemplate))->getMaterialVector();
    if (!materials || materials->empty())
        return;

    const int count = static_cast<int>(materials->size());
    m_materialCount = count;
    m_materialIndex = new unsigned short[count];
    for (int i = 0; i < count; ++i)
        m_materialIndex[i] = static_cast<unsigned short>((*materials)[i].second);
}

// the accessors the loader uses
void CollisionMeshTemplate::setVersionToLoad(unsigned int major, unsigned int minor)
{
    m_versionToLoad  = static_cast<short>(major);
    m_versionToLoad2 = static_cast<short>(minor);
}

// a geom index in range and with at least one lod slot.
bool CollisionMeshTemplate::isGeomValid(int geom) const
{
    return static_cast<unsigned int>(geom) < m_lods.size() && !m_lods[geom].lods.empty();
}

// the col-layer remap test: remap[lod] names a slot in range whose Lod exists.
bool CollisionMeshTemplate::isLodValid(int geom, int lod) const
{
    const CollisionGeom& g = m_lods[geom];
    const int slot = g.colRemap[lod];
    return static_cast<unsigned int>(slot) < g.lods.size() && g.lods[slot] != 0;
}

// getValidLod - the Lod at remap[lod]. The two shipped variants differ on absent layers: the
// non-const body (retail ELF 0x71c800) indexes lods[remap[lod]] unchecked, while the const body
// (retail ELF 0x719810) clamps the remapped slot into [0, lods.size()-1] before indexing.
// getBoundingBox is const and relies on that clamp, so an absent layer (colRemap == -1) resolves
// to lod 0 instead of walking the vector out of bounds. (PE: 0x83A1710 / 0x83A16C0.)
CollisionLod* CollisionMeshTemplate::getValidLod(int geom, int lod)
{
    return m_lods[geom].lods[m_lods[geom].colRemap[lod]];
}
const CollisionLod* CollisionMeshTemplate::getValidLod(int geom, int lod) const
{
    const CollisionGeom& g = m_lods[geom];
    int slot = g.colRemap[lod];
    const int count = static_cast<int>(g.lods.size());
    if (slot >= count)
        slot = count - 1;
    if (slot < 0)
        slot = 0;
    return g.lods[slot];
}

// push the vertex, grow the bounding box.
void CollisionMeshTemplate::addVertex(int geom, int lod, const Vec3& v)
{
    CollisionLod* l = getValidLod(geom, lod);
    l->vertices.push_back(v);
    LocalBoundingBox& bb = l->boundingBox;                    // expand(), inline: min/max per axis
    if (v.x < bb.m_min.x) bb.m_min.x = v.x;  if (v.x > bb.m_max.x) bb.m_max.x = v.x;
    if (v.y < bb.m_min.y) bb.m_min.y = v.y;  if (v.y > bb.m_max.y) bb.m_max.y = v.y;
    if (v.z < bb.m_min.z) bb.m_min.z = v.z;  if (v.z > bb.m_max.z) bb.m_max.z = v.z;
    bb.m_derivedDataDirty = true;
}

// push the face (three shorts + material, the 8-byte record).
void CollisionMeshTemplate::addFace(int geom, int lod, unsigned int a, unsigned int b,
                                    unsigned int c, unsigned short material)
{
    CollisionFace f;
    f.v0 = static_cast<short>(a);
    f.v1 = static_cast<short>(b);
    f.v2 = static_cast<short>(c);
    f.material = material;
    getValidLod(geom, lod)->faces.push_back(f);
}

void CollisionMeshTemplate::verifyPivot() const {}          // 5 B - empty, as shipped

// "use collision as first person": drop geom 0. Every geom takes the next
// geom's lod list and remap, and the last geom is erased. (Called by CollisionManager::load when
// no part had a valid geom 0 - a mesh authored with only wreck geometry promotes it.)
void CollisionMeshTemplate::setUseCollisionAsFirstPerson()
{
    const int last = static_cast<int>(m_lods.size()) - 1;
    for (int i = 0; i < last; ++i)
    {
        m_lods[i].lods = m_lods[i + 1].lods;
        for (int r = 0; r < 5; ++r)
            m_lods[i].colRemap[r] = m_lods[i + 1].colRemap[r];
    }
    if (last >= 0)
        m_lods.resize(last);
}

// Loads collision geometry after the manager has consumed the file header and set its version.
// fromFile reads little-endian geometry/LOD records; false creates the built-in soldier capsule.
// ownerTemplate is unused. Older versions use sequential LODs; version 9+ supplies type indices.
// Preserve AI-nav skipping, LOD fallback mapping and consumed-byte accounting. Versions before 8
// abort at the missing-BSP-flag boundary; version 10+ validates and narrows adjacency indices.
// The loaded flag is set before optional material conversion and debug-mesh generation.
namespace {
bool  s_keepAINavRead = false;                   // the ELF's guard @ 0x8ABBA60
bool  s_keepAINav     = false;                   // ...and its value @ 0x8ABBA68

inline void readU32(io::INonlinearStream* s, unsigned int& out) { s->read(&out, 4); }
inline void skip(io::INonlinearStream* s, int bytes)            { s->seek(static_cast<io::SeekOrigin>(1), bytes); }
} // namespace

bool CollisionMeshTemplate::load(io::INonlinearStream* stream, const IObjectTemplate* /*owner*/,
                                 bool fromFile)
{
    const bool v7  = m_versionToLoad || m_versionToLoad2 > 6;
    const bool v8  = m_versionToLoad || m_versionToLoad2 > 7;
    const bool v9  = m_versionToLoad || m_versionToLoad2 > 8;
    const bool v10 = m_versionToLoad || m_versionToLoad2 > 9;

    if (!fromFile)
    {
        // the soldier capsule
        // Both ports join the ordinary load tail after building this mesh. PE 0x6BD96E/0x6BD97A
        // and ELF 0x83A0C43/0x839F8BC store 4096 and publish m_loaded respectively. The old port
        // returned here instead, so CollisionMesh::hasLod rejected this otherwise populated mesh.
        m_lods.resize(1);
        CollisionGeom& g = m_lods[0];
        g.lods.resize(2);
        g.lods[0] = new CollisionLod();
        g.lods[1] = new CollisionLod();
        g.colRemap[0] = g.colRemap[1] = g.colRemap[2] = g.colRemap[3] = 0;
        g.colRemap[4] = -1;

        static const float kVerts[17][3] = {
            { 0.0f, -1.0f, 0.0f }, {-0.2f, -0.6f, -0.2f}, { 0.2f, -0.6f, -0.2f},
            { 0.2f, -0.6f,  0.2f}, {-0.2f, -0.6f,  0.2f}, {-0.4f,  0.0f, -0.2f},
            {-0.2f,  0.0f, -0.4f}, { 0.2f,  0.0f, -0.4f}, { 0.4f,  0.0f, -0.2f},
            { 0.4f,  0.0f,  0.2f}, { 0.2f,  0.0f,  0.4f}, {-0.2f,  0.0f,  0.4f},
            {-0.4f,  0.0f,  0.2f}, { 0.0f,  0.8f, -0.4f}, { 0.4f,  0.8f,  0.0f},
            { 0.0f,  0.8f,  0.4f}, {-0.4f,  0.8f,  0.0f} };
        for (int i = 0; i < 17; ++i)
        {
            Vec3 v; v.x = kVerts[i][0]; v.y = kVerts[i][1]; v.z = kVerts[i][2];
            addVertex(0, 0, v);
        }
        static const unsigned short kFaces[30][3] = {
            {1,0,2},{2,0,3},{3,0,4},{4,0,1},{5,1,6},{6,1,7},{7,1,2},{7,2,8},{8,2,9},{9,2,3},
            {9,3,10},{10,3,11},{11,3,4},{11,4,12},{12,4,5},{5,4,1},{7,13,6},{8,13,7},{14,13,8},
            {9,14,8},{10,14,9},{15,14,10},{11,15,10},{12,15,11},{16,15,12},{5,16,12},{6,16,5},
            {13,16,6},{14,15,13},{15,16,13} };
        for (int i = 0; i < 30; ++i)
            addFace(0, 0, kFaces[i][0], kFaces[i][1], kFaces[i][2], 0x18);

        // PE calls Lod::operator= at 0x6BD95E; ELF inlines the same deep vector/member assignment
        // through 0x83A0C31. Finalise the source bounding data before making the copy.
        CollisionLod* const primary = g.lods[0];
        if (primary->boundingBox.m_derivedDataDirty)
            primary->boundingBox.calcRadius();
        // ELF 0x83A09F5 / PE 0x6BD952 copy LocalBoundingBox +0x18: radius from origin.
        primary->maxBoundingRadius = primary->boundingBox.m_radiusFromOrigin;
        *g.lods[1] = *primary;

        m_dataSize = 4096;
        m_loaded = 1;
        if (!v7)
            giveVerticesMaterial();

        const int loadDebugMeshes = *hfe::Vars::getInt("coll-load-debugmeshes", 0);
        const int forceOffDebugMeshes = *hfe::Vars::getInt("coll-forceOff-debugmeshes", 0);
        if (loadDebugMeshes && !forceOffDebugMeshes)
            buildDebugMeshes(false);
        return true;
    }

    // The file
    if (!s_keepAINavRead)
    {
        const bool def = false;
        s_keepAINav = settings->boolGet(std::string("keepAINav"), def);
        s_keepAINavRead = true;
    }

    const int startPos = stream->getPosition();
    int skippedNav = 0;

    unsigned int geomCount = 0;
    readU32(stream, geomCount);
    m_lods.resize(geomCount);

    for (unsigned int gi = 0; gi < geomCount; ++gi)
    {
        CollisionGeom& g = m_lods[gi];
        unsigned int lodCount = 0;
        readU32(stream, lodCount);
        for (int r = 0; r < 5; ++r) g.colRemap[r] = -1;

        if (!v9)
        {
            g.lods.resize(lodCount);
            for (unsigned int s = 0; s < g.lods.size(); ++s)
                g.lods[s] = new CollisionLod();
        }

        int lastCreated = -1;
        for (unsigned int k = 0; k < lodCount; ++k)
        {
            unsigned int colType = k;
            if (v9) readU32(stream, colType);
            g.colRemap[colType] = static_cast<int>(colType);

            if (!s_keepAINav && colType == 3)
            {
                // The AI-nav skip
                g.colRemap[3] = -1;
                const int navStart = stream->getPosition();
                unsigned int n = 0;
                readU32(stream, n); skip(stream, 8 * n);
                readU32(stream, n); skip(stream, 12 * n);
                if (v7) skip(stream, 2 * n);
                skip(stream, 24);
                if (v8)
                {
                    unsigned char flag = '0';
                    stream->read(&flag, 1);
                    if (flag == '1')
                    {
                        skip(stream, 24);
                        readU32(stream, n); skip(stream, 16 * n);
                        readU32(stream, n); skip(stream, 2 * n);
                    }
                }
                if (v10) { readU32(stream, n); skip(stream, 4 * n); }
                if (!v9) g.lods[k] = 0;
                skippedNav = stream->getPosition() - navStart;
                continue;
            }

            CollisionLod* lod;
            if (v9)
            {
                if (g.lods.size() < colType + 1) g.lods.resize(colType + 1);
                for (int m = lastCreated + 1; m < static_cast<int>(colType); ++m) g.lods[m] = 0;
                g.lods[colType] = new CollisionLod();
                lastCreated = static_cast<int>(colType);
                lod = g.lods[colType];
            }
            else
            {
                lod = g.lods[k];
            }

            unsigned int faceCount = 0;
            readU32(stream, faceCount);
            lod->faces.resize(faceCount);
            if (faceCount) stream->read(&lod->faces[0], 8 * faceCount);

            unsigned int vertCount = 0;
            readU32(stream, vertCount);
            lod->vertices.resize(vertCount);
            if (vertCount) stream->read(&lod->vertices[0], 12 * vertCount);

            if (v7)
            {
                lod->materials.resize(vertCount);              // per vertex, as shipped
                if (vertCount) stream->read(&lod->materials[0], 2 * vertCount);
            }

            stream->read(&lod->boundingBox, 24);               // min + max
            lod->boundingBox.calcRadius();
            if (lod->boundingBox.m_derivedDataDirty)
                lod->boundingBox.calcRadius();                 // the doubled call, as shipped
            // ELF 0x839F7EA / PE 0x6BD0AF read box +0x18, not the centre radius at +0x1C.
            // Collision culling centres this sphere on the object's origin; using the smaller
            // centre radius drops offset wheel contacts while their tyres still touch the deck.
            lod->maxBoundingRadius = lod->boundingBox.m_radiusFromOrigin;

            if (!v8)
                std::abort();                                  // shipped: pre-v8 = hard fail

            unsigned char bspFlag = '0';
            stream->read(&bspFlag, 1);
            if (bspFlag == '1')
            {
                lod->bsp.load(stream, &lod->vertices, &lod->faces);   // -> BspV8::load
                if (v10)
                {
                    unsigned int n = 0;                        // = 3 * faceCount in shipped files
                    readU32(stream, n);
                    lod->faceAdjacency = new unsigned short[n];
                    int* raw = new int[n];
                    if (n) stream->read(raw, 4 * n);
                    for (unsigned int i = 0; i < n; ++i)
                    {
                        if (raw[i] == -1)
                            lod->faceAdjacency[i] = 0xFFFF;
                        else if (static_cast<unsigned int>(raw[i]) >= faceCount)
                            std::abort();                      // shipped range check
                        else
                            lod->faceAdjacency[i] = static_cast<unsigned short>(raw[i]);
                    }
                    delete[] raw;
                }
            }
        }

        // Geom remap starts at +0x0C;
        // PE 0x6BD1C2..0x6BD1D5: it starts at +0x10 after the larger STL vector.
        // Both fall back 2 <- 1, then 3 <- 2. Layer 0 never supplies vehicle
        // collision: stock bushes with only projectile geometry must stay passable.
        if (g.colRemap[2] == -1) g.colRemap[2] = g.colRemap[1];
        if (g.colRemap[3] == -1) g.colRemap[3] = g.colRemap[2];
    }

    // +0x14 gets the part's byte count (minus the skipped nav span), stored as an int
    m_dataSize = stream->getPosition() - startPos - skippedNav;

    // The tail (0x839F8BC..0x839F928)
    m_loaded = 1;                                       // the gate. See the header block.

    if (!v7)
        giveVerticesMaterial();                         // pre-v7: material was per face

    const int loadDebugMeshes  = *hfe::Vars::getInt("coll-load-debugmeshes", 0);
    const int forceOffDebugMeshes = *hfe::Vars::getInt("coll-forceOff-debugmeshes", 0);
    if (loadDebugMeshes && !forceOffDebugMeshes)
        buildDebugMeshes(false);

    return true;
}

// Converts pre-v7 face materials into per-vertex values, assigning each face's material to all three vertices.
// The LOD loop uses the full vector size despite colRemap having five entries; preserve that range behavior.
void CollisionMeshTemplate::giveVerticesMaterial()
{
    for (unsigned int geom = 0; geom < m_lods.size(); ++geom)
    {
        if (!isGeomValid(static_cast<int>(geom)))
            continue;

        CollisionGeom& g = m_lods[geom];
        for (unsigned int lod = 0; lod < g.lods.size(); ++lod)
        {
            if (!isLodValid(static_cast<int>(geom), static_cast<int>(lod)))
                continue;

            CollisionLod* l = getValidLod(static_cast<int>(geom), static_cast<int>(lod));
            l->materials.resize(l->vertices.size());            // grow with 0 / shrink by erase

            for (unsigned int f = 0; f < l->faces.size(); ++f)
            {
                const CollisionFace& face = l->faces[f];
                l->materials[face.v0] = face.material;
                l->materials[face.v1] = face.material;
                l->materials[face.v2] = face.material;
            }
        }
    }
}

// Builds missing debug geometry for each valid geometry/LOD pair. Requires a usable
// geometry backend; enabling the debug switch alone is unsafe in dedicated images.
void CollisionMeshTemplate::buildDebugMeshes(bool wreck)
{
    for (unsigned int geom = 0; geom < m_lods.size(); ++geom)
    {
        if (!isGeomValid(static_cast<int>(geom)))
            continue;

        CollisionGeom& g = m_lods[geom];
        for (unsigned int lod = 0; lod < g.lods.size(); ++lod)
        {
            if (!isLodValid(static_cast<int>(geom), static_cast<int>(lod)))
                continue;
            if (!getValidLod(static_cast<int>(geom), static_cast<int>(lod))->debugMesh)   // +0xAC
                buildDebugMesh(static_cast<int>(geom), static_cast<int>(lod), wreck);
        }
    }
}

namespace {
// [CHOSEN, not recovered] Native ABI adapter for the still-opaque geom::IGeometry and
// DebugXMeshTemplate owners. These are logical vtable ordinals, never retail byte offsets
// divided by native pointer size. No partial virtual interface or renderer factory is published.
template <typename Fn>
Fn collisionDebugSlot(IBase* object, unsigned int ordinal)
{
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[ordinal]);
}

IBase* collisionDebugManager()
{
#if DEDICATED
    // ELF 83A62A8/83A6400 and dedicated PE 6BAA3B/6BAB26 literally read address zero.
    // Preserve that shipped failure, with no new guard. Client PE 70451D/7045F8 uses its
    // real manager global; the two images must not acquire a fabricated null singleton.
    IBase* const manager = 0;
    return manager;
#else
    return ::dice::hfe::g_geometryTemplateManager;
#endif
}

typedef geom::DebugXMeshFaceColorInfo DebugFaceColor;
static int collisionDebugMeshNumber = 0;
static const Vec3 collisionDebugColors[10] = {
    { .66f, .66f, .185f }, { .185f, .66f, .185f }, { .185f, .185f, .66f },
    { .185f, .66f, .66f }, { .66f, .185f, .185f }, { .66f, .66f, .66f },
    { .66f, .185f, .66f }, { .185f, .185f, .185f }, { 1.f, 1.f, 1.f }, { 0.f, 0.f, 0.f }
};

// PE 6BA9E0 / ELF 83A6250 (createDebugMesh). [CHOSEN, not recovered] The local helper
// keeps the existing IBase ownership view until the complete IGeometry type is recovered.
void createCollisionDebugMesh(IBase** output, const std::string& name, int vertexCount,
                             int faceCount, unsigned char* vertices, unsigned short* indices,
                             std::vector<DebugFaceColor> colors)
{
    char suffix[8];                               // shipped fixed buffer, including its limit
    std::sprintf(suffix, "%d", collisionDebugMeshNumber);
    const std::string templateName = name + suffix;
    IBase* manager = collisionDebugManager();
#if !DEDICATED && defined(_M_IX86)
    geom::IGeometryTemplate* meshTemplate =
        static_cast<geom::IGeometryTemplateManager*>(manager)->createTemplate(
            geom::CID_DebugXMeshTemplate, templateName);
#else
    // Other images retain the host string ABI and the dedicated null-manager fault.
    typedef IBase* (__thiscall* CreateTemplate)(IBase*, unsigned int, const std::string&);
    geom::IGeometryTemplate* meshTemplate = static_cast<geom::IGeometryTemplate*>(
        collisionDebugSlot<CreateTemplate>(manager, 5)(
            manager, geom::CID_DebugXMeshTemplate, templateName));
#endif
    *output = static_cast<IBase*>(meshTemplate->createInstance());
#if defined(_WIN32)
    const unsigned int setNameOrdinal = 26;       // PE +68; ELF +6C, after its extra dtor
#else
    const unsigned int setNameOrdinal = 27;
#endif
    // DebugXMesh +68 (RendDX9 10082D30) takes the shared std::string (issue #7).
    typedef void (__thiscall* SetName)(IBase*, const std::string&);
    collisionDebugSlot<SetName>(*output, setNameOrdinal)(*output, std::string("collisionMesh"));
    // RendDX9 template table 1020C5C0: +D8=10092AE0, +DC=10093590. The sixth
    // argument is a byte-tested optimization bool; the seventh is a VertexDeclaration*.
    typedef bool (__thiscall* Build)(IBase*, int, int, int, unsigned char*, unsigned short*,
                                    bool, void*);
    if (collisionDebugSlot<Build>(meshTemplate, 54)(meshTemplate, vertexCount, 3 * faceCount,
                                                   24, vertices, indices, false, 0))
    {
        typedef void (__thiscall* SetColors)(IBase*, const DebugFaceColor*, int);
        collisionDebugSlot<SetColors>(meshTemplate, 55)(meshTemplate, colors.data(),
                                                       static_cast<int>(colors.size()));
    }
    else
        *output = 0;                             // shipped loss of reference; no release
}

// PE 6BAB00 / ELF 83A63D0 (extractAdjacencyFromDebugMesh). The name and colors parameters
// are unused. RendDX9 +E0 (100928A0) returns the D3DX mesh's unsigned face count; +E4
// (100928C0) takes float epsilon, ushort output[], unsigned count and narrows dword neighbours.
void extractCollisionDebugAdjacency(const std::string&, int vertexCount, int faceCount,
                                   unsigned char* vertices, unsigned short* indices,
                                   std::vector<DebugFaceColor>, unsigned short** output)
{
    const std::string name("adjTempTemplate");
    IBase* manager = collisionDebugManager();
#if !DEDICATED && defined(_M_IX86)
    IBase* meshTemplate = static_cast<geom::IGeometryTemplateManager*>(manager)->createTemplate(
        geom::CID_DebugXMeshTemplate, name);
#else
    typedef IBase* (__thiscall* CreateTemplate)(IBase*, unsigned int, const std::string&);
    IBase* meshTemplate = collisionDebugSlot<CreateTemplate>(manager, 5)(
        manager, geom::CID_DebugXMeshTemplate, name);
#endif
    typedef bool (__thiscall* Build)(IBase*, int, int, int, unsigned char*, unsigned short*,
                                    bool, void*);
    if (collisionDebugSlot<Build>(meshTemplate, 54)(meshTemplate, vertexCount, 3 * faceCount,
                                                   24, vertices, indices, false, 0))
    {
        if (output)
        {
            typedef unsigned int (__thiscall* FaceCount)(IBase*);
            const unsigned int faces = collisionDebugSlot<FaceCount>(meshTemplate, 56)(meshTemplate);
            if (faces)
            {
                const unsigned int count = 3 * faces;
                *output = new unsigned short[count];
                typedef bool (__thiscall* Adjacency)(IBase*, float, unsigned short*, unsigned int);
                if (!collisionDebugSlot<Adjacency>(meshTemplate, 57)(meshTemplate, .001f, *output, count))
                {
                    delete[] *output;
                    *output = 0;
                }
            }
        }
    }
    else
        Debug(DebugTypeError, "Physics/CollisionMeshTemplate.cpp", 1807, "")
            << "Failed to generate adjacency";
    manager = collisionDebugManager();            // client reloads the global after callbacks
#if !DEDICATED && defined(_M_IX86)
    static_cast<geom::IGeometryTemplateManager*>(manager)->removeTemplate(name);
#else
    typedef void (__thiscall* RemoveTemplate)(IBase*, const std::string&);
    collisionDebugSlot<RemoveTemplate>(manager, 9)(manager, name);
#endif
}

Vec3 collisionDebugCorner(const LocalBoundingBox& box, int corner)
{
    // PE 68D1B0 / inlined ELF builder: bit 2 selects x, bit 1 y, bit 0 z.
    const Vec3 result = { (corner & 4) ? box.m_max.x : box.m_min.x,
                          (corner & 2) ? box.m_max.y : box.m_min.y,
                          (corner & 1) ? box.m_max.z : box.m_min.z };
    return result;
}
} // anonymous namespace

// ELF 83A46C0 (7,046 B) / PE 6BBD60, complete bodies read. Mesh, bounding box, then pivot;
// the adjacency-only branch intentionally leaves its two raw arrays allocated in both ports.
void CollisionMeshTemplate::buildDebugMesh(int geom, int lod, bool wreck)
{
    CollisionLod* data = getValidLod(geom, lod);
    std::map<int, int> materialCounts;
    std::map<int, unsigned short*> materialIndices;
    const int faceCount = getFaceCount(geom, lod);
    unsigned short* indices = new unsigned short[3 * faceCount];
    const CollisionFace* faces = getFaceVector(geom, lod);
    for (int i = 0; i < faceCount; ++i)
    {
        const int material = faces[i].material;
        if (materialCounts.find(material) == materialCounts.end())
            materialCounts.insert(std::make_pair(material, 1));
        else
            ++materialCounts[material];
    }
    std::vector<DebugFaceColor> colors(materialCounts.size());
    int colorIndex = 0;
    int firstIndex = 0;
    for (std::map<int, int>::const_iterator it = materialCounts.begin(); it != materialCounts.end(); ++it)
    {
        DebugFaceColor& color = colors[colorIndex++];
        color.color = collisionDebugColors[it->first % 10];
        color.firstIndex = firstIndex;
        color.indexCount = 3 * it->second;
        materialIndices.insert(std::make_pair(it->first, indices + firstIndex));
        firstIndex += 3 * it->second;
    }
    for (int i = 0; i < faceCount; ++i)
    {
        const int material = faces[i].material;
        materialIndices[material][0] = static_cast<unsigned short>(faces[i].v0);
        materialIndices[material][1] = static_cast<unsigned short>(faces[i].v1);
        materialIndices[material][2] = static_cast<unsigned short>(faces[i].v2);
        materialIndices[material] = materialIndices[material] + 3;
    }
    const int vertexCount = static_cast<int>(getVertexCount(geom, lod));
    Vec3* vertices = new Vec3[2 * vertexCount]();
    const Vec3* source = getVertexVector(geom, lod);
    const Vec3 up = { 0.f, 1.f, 0.f };
    for (int i = 0; i < vertexCount; ++i)
    {
        vertices[2 * i] = source[i];
        vertices[2 * i + 1] = up;
    }
    if (wreck)
    {
        extractCollisionDebugAdjacency("_debugMesh_", vertexCount, faceCount,
            reinterpret_cast<unsigned char*>(vertices), indices, colors, &data->faceAdjacency);
        return;                                  // do not repair the shipped array leak
    }
    createCollisionDebugMesh(&data->debugMesh, "_debugMesh_", vertexCount, faceCount,
        reinterpret_cast<unsigned char*>(vertices), indices, colors);
    colors.resize(0);
    delete[] indices;
    delete[] vertices;

    std::vector<Vec3> debugVertices;
    std::vector<unsigned short> debugIndices;
    LocalBoundingBox& box = data->boundingBox;
    Vec3 centre = { 0.f, 0.f, 0.f };
    for (int i = 0; i < 8; ++i)
    {
        const Vec3 corner = collisionDebugCorner(box, i);
        centre.x += corner.x;
        centre.y += corner.y;
        centre.z += corner.z;
        debugVertices.push_back(collisionDebugCorner(box, i));
        debugVertices.push_back(up);
    }
    centre.x *= .125f;
    centre.y *= .125f;
    centre.z *= .125f;
    const int quads[6][4] = { {1,5,7,3}, {2,6,4,0}, {0,1,3,2},
                              {4,6,7,5}, {0,4,5,1}, {3,7,6,2} };
    for (int i = 0; i < 6; ++i)
        for (int j = 1; j <= 2; ++j)
        {
            debugIndices.push_back(static_cast<unsigned short>(quads[i][0]));
            debugIndices.push_back(static_cast<unsigned short>(quads[i][j]));
            debugIndices.push_back(static_cast<unsigned short>(quads[i][j + 1]));
        }
    const Vec3& boxColor = collisionDebugColors[0];
    DebugFaceColor color = { 0, 36, { boxColor.x * .5f, boxColor.y * .5f, boxColor.z * .5f } };
    colors.push_back(color);
    createCollisionDebugMesh(&data->dwordB0, "_debugBoundingBox_",
        static_cast<int>(debugVertices.size() / 2), static_cast<int>(debugIndices.size() / 3),
        reinterpret_cast<unsigned char*>(debugVertices.data()), debugIndices.data(), colors);

    debugVertices.resize(48, Vec3{});
    debugIndices.resize(108, 0);
    if (box.m_derivedDataDirty)
        box.calcRadius();
    const float radius = box.m_radiusFromOrigin;
    const float scaled = .01f * radius;
    const float capped = scaled < .1f ? scaled : .1f;
    const float halfWidth = (capped > .05f ? capped : .05f) * .5f;
    for (int axis = 2; axis >= 0; --axis)
        for (int i = 0; i < 12; ++i)
        {
            if (i < 8)
            {
                const Vec3& corner = debugVertices[2 * i];
                const float delta[3] = { corner.x - centre.x, corner.y - centre.y, corner.z - centre.z };
                float p[3];
                // PE tests x87 C0, ELF uses fcmovnb: unordered selects negative too.
                p[axis] = delta[axis] >= 0.f ? radius : -radius;
                for (int j = 1; j <= 2; ++j)
                {
                    const int component = (axis + j) % 3;
                    p[component] = delta[component] >= 0.f ? halfWidth : -halfWidth;
                }
                const Vec3 point = { p[0], p[1], p[2] };
                debugVertices[2 * (i + 8 * axis)] = point;
                if (axis > 0)
                    debugVertices[2 * (i + 8 * axis) + 1] = up;
            }
            if (axis > 0)
                for (int j = 0; j < 3; ++j)
                    debugIndices[3 * (i + 12 * axis) + j] =
                        static_cast<unsigned short>(8 * axis + debugIndices[3 * i + j]);
        }
    const Vec3& firstAxisColor = collisionDebugColors[1];
    colors[0].color = { firstAxisColor.x * .5f, firstAxisColor.y * .5f, firstAxisColor.z * .5f };
    for (int axis = 1; axis <= 2; ++axis)
    {
        const Vec3& axisColor = collisionDebugColors[axis + 1];
        DebugFaceColor axisInfo = { 36 * axis, 36,
            { axisColor.x * .5f, axisColor.y * .5f, axisColor.z * .5f } };
        colors.push_back(axisInfo);
    }
    static const int vertexVecSize = static_cast<int>(debugVertices.size() / 2);
    static const int indexVecSize = static_cast<int>(debugIndices.size() / 3);
    createCollisionDebugMesh(&data->dwordB4, "_debugPivot_", vertexVecSize, indexVecSize,
        reinterpret_cast<unsigned char*>(debugVertices.data()), debugIndices.data(), colors);
    ++collisionDebugMeshNumber;
}

// ELF 83A1900 / PE 6B9370. Both use +50 for transform and +18 for draw, before the
// destructor-related slot difference. The stored pointer is reread after each virtual call.
void CollisionMeshTemplate::drawDebugMesh(int geom, int lod, const Mat4& transform,
                                         bool mesh, bool bounds, bool pivot)
{
    if (!isLodValid(geom, lod))
        return;
    CollisionLod* data = getValidLod(geom, lod);
    typedef void (__thiscall* Transform)(IBase*, const Mat4*);
    typedef void (__thiscall* Draw)(IBase*, void*, void*, int);
    if (mesh && data->debugMesh)
    {
        collisionDebugSlot<Transform>(data->debugMesh, 20)(data->debugMesh, &transform);
        collisionDebugSlot<Draw>(data->debugMesh, 6)(data->debugMesh, 0, 0, 1);
    }
    if (bounds && data->dwordB0)
    {
        collisionDebugSlot<Transform>(data->dwordB0, 20)(data->dwordB0, &transform);
        collisionDebugSlot<Draw>(data->dwordB0, 6)(data->dwordB0, 0, 0, 1);
    }
    if (pivot && data->dwordB4)
    {
        collisionDebugSlot<Transform>(data->dwordB4, 20)(data->dwordB4, &transform);
        collisionDebugSlot<Draw>(data->dwordB4, 6)(data->dwordB4, 0, 0, 1);
    }
}

// Releases debug references for each remapped LOD. Clears the primary reference to
// prevent repeated releases; a null primary leaves both secondary references untouched.
void CollisionMeshTemplate::destroyDebugMeshes(bool)
{
    for (unsigned int geom = 0; geom < m_lods.size(); ++geom)
    {
        if (!isGeomValid(static_cast<int>(geom)))
            continue;
        CollisionGeom& g = m_lods[geom];
        for (unsigned int lod = 0; lod < g.lods.size(); ++lod)
        {
            if (!isLodValid(static_cast<int>(geom), static_cast<int>(lod)))
                continue;
            CollisionLod* const data = getValidLod(static_cast<int>(geom), static_cast<int>(lod));
            if (data->debugMesh)
            {
                data->debugMesh->release();
                data->debugMesh = 0;
                if (data->dwordB0)
                    data->dwordB0->release();
                data->dwordB0 = 0;
                if (data->dwordB4)
                    data->dwordB4->release();
                data->dwordB4 = 0;
            }
        }
    }
}

// Builds wall-edge index pairs for collision layer 1, using the normal-Y threshold.
// Visits layers 1 and 2 for every valid geometry; degenerate faces use normal Y=1.
// Preserve the discarded getValidLod call. The manager invokes this only on mesh part 0.
void CollisionMeshTemplate::buildEdgeInfo()
{
    for (unsigned int geom = 0; geom < m_lods.size(); ++geom)
    {
        if (!isGeomValid(static_cast<int>(geom)))
            continue;
        for (int lod = 1; lod <= 2; ++lod)
        {
            if (!isLodValid(static_cast<int>(geom), lod))
                continue;
            (void)getValidLod(static_cast<int>(geom), lod);
            bool degenerateSeen = false;
            const CollisionFace* faces = getFaceVector(static_cast<int>(geom), lod);
            const Vec3* verts = getVertexVector(static_cast<int>(geom), lod);
            const unsigned int faceCount = static_cast<unsigned int>(getFaceCount(static_cast<int>(geom), lod));
            for (unsigned int j = 0; j < faceCount; ++j)
            {
                const CollisionFace& f = faces[j];
                const int idx[3] = { f.v0, f.v1, f.v2 };
                const Vec3& p0 = verts[idx[0]];
                const Vec3& p1 = verts[idx[1]];
                const Vec3& p2 = verts[idx[2]];
                const float e1x = p1.x - p0.x, e1y = p1.y - p0.y, e1z = p1.z - p0.z;
                const float e2x = p2.x - p0.x, e2y = p2.y - p0.y, e2z = p2.z - p0.z;
                const float len1 = sqrtf(e1z * e1z + e1y * e1y + e1x * e1x);
                const float len2 = sqrtf(e2y * e2y + e2x * e2x + e2z * e2z);
                float ny;
                if (len1 > 0.000099999997f && len2 > 0.000099999997f)
                {
                    const float nx = e1y * e2z - e1z * e2y;
                    ny = e1z * e2x - e2z * e1x;
                    const float nz = e2y * e1x - e2x * e1y;
                    const float l2 = nz * nz + ny * ny + nx * nx;
                    if (fabsf(l2 - 1.0f) >= 1.1920929e-7f)
                    {
                        if (fabsf(l2) >= 1.1920929e-7f)
                            ny = ny * (1.0f / sqrtf(l2));
                        else
                            ny = 0.0f;
                    }
                }
                else
                {
                    if (!degenerateSeen) degenerateSeen = true;
                    ny = 1.0f;
                }
                if (lod == 1 && s_edgeMaxNormalY >= fabsf(ny))
                {
                    addEdgeIndex(static_cast<int>(geom), 1, idx[0]);
                    addEdgeIndex(static_cast<int>(geom), 1, idx[1]);
                    addEdgeIndex(static_cast<int>(geom), 1, idx[1]);
                    addEdgeIndex(static_cast<int>(geom), 1, idx[2]);
                    addEdgeIndex(static_cast<int>(geom), 1, idx[2]);
                    addEdgeIndex(static_cast<int>(geom), 1, idx[0]);
                }
            }
        }
    }
}

// push one u16 onto the Lod's edge table.
void CollisionMeshTemplate::addEdgeIndex(int geom, int lod, int index)
{
    getValidLod(geom, lod)->edgeIndices.push_back(static_cast<unsigned short>(index));
}

// the accessors - each `getValidLod(geom, lod)->member`, so each inherits getValidLod's shipped
// no-range-check hazard. Data pointers are the vectors' begin pointers (null when empty).
unsigned int CollisionMeshTemplate::getVertexCount(int geom, int lod) const
{
    return static_cast<unsigned int>(getValidLod(geom, lod)->vertices.size());
}
const Vec3* CollisionMeshTemplate::getVertexVector(int geom, int lod) const
{
    const CollisionLod* l = getValidLod(geom, lod);
    return l->vertices.empty() ? 0 : &l->vertices[0];
}
// the vertex's material through the object's mapping when one was built.
int CollisionMeshTemplate::getMaterialForVertex(int geom, int lod, int vertex) const
{
    const CollisionLod* l = getValidLod(geom, lod);
    const unsigned short raw = l->materials[vertex];
    return m_materialIndex ? m_materialIndex[raw] : raw;
}
int CollisionMeshTemplate::getFaceCount(int geom, int lod) const
{
    return static_cast<int>(getValidLod(geom, lod)->faces.size());
}
const CollisionFace* CollisionMeshTemplate::getFaceVector(int geom, int lod) const
{
    const CollisionLod* l = getValidLod(geom, lod);
    return l->faces.empty() ? 0 : &l->faces[0];
}
// lod 4 (the fifth col slot) has a floor: a radius under 0.01 reads as 0.3.
float CollisionMeshTemplate::getBoundingRadius(int geom, int lod) const
{
    const CollisionLod* l = getValidLod(geom, lod);
    if (lod == 4 && l->maxBoundingRadius < 0.0099999998f)
        return 0.30000001f;
    return l->maxBoundingRadius;
}
const LocalBoundingBox* CollisionMeshTemplate::getBoundingBox(int geom, int lod) const
{
    return &getValidLod(geom, lod)->boundingBox;
}
int CollisionMeshTemplate::getEdgeIndexVectorSize(int geom, int lod) const
{
    return static_cast<int>(getValidLod(geom, lod)->edgeIndices.size());
}
const unsigned short* CollisionMeshTemplate::getEdgeIndexVector(int geom, int lod) const
{
    const CollisionLod* l = getValidLod(geom, lod);
    return l->edgeIndices.empty() ? 0 : &l->edgeIndices[0];
}
// each - the RAW slot (no remap), range-checked, null when absent.
const CollisionLod* CollisionMeshTemplate::getValidLodRaw(int geom, int lod) const
{
    const CollisionGeom& g = m_lods[geom];
    return (lod >= 0 && static_cast<unsigned int>(lod) < g.lods.size()) ? g.lods[lod] : 0;
}
CollisionLod* CollisionMeshTemplate::getValidLodRaw(int geom, int lod)
{
    CollisionGeom& g = m_lods[geom];
    return (lod >= 0 && static_cast<unsigned int>(lod) < g.lods.size()) ? g.lods[lod] : 0;
}
// not a maximum: the first valid lod's radius (geom order, then raw slot
// order), 0 if none. The name promises more than the body does.
float CollisionMeshTemplate::getMaxBoundingRadius() const
{
    for (unsigned int geom = 0; geom < m_lods.size(); ++geom)
    {
        if (!isGeomValid(static_cast<int>(geom)))
            continue;
        for (unsigned int lod = 0; lod < m_lods[geom].lods.size(); ++lod)
            if (isLodValid(static_cast<int>(geom), static_cast<int>(lod)))
                return getValidLod(static_cast<int>(geom), static_cast<int>(lod))->maxBoundingRadius;
    }
    return 0.0f;
}
// the union of every valid lod's box; returns whether any was found.
bool CollisionMeshTemplate::getBoundingBoxMinMax(Vec3& outMin, Vec3& outMax) const
{
    bool first = true;
    for (unsigned int geom = 0; geom < m_lods.size(); ++geom)
    {
        if (!isGeomValid(static_cast<int>(geom)))
            continue;
        for (unsigned int lod = 0; lod < m_lods[geom].lods.size(); ++lod)
        {
            if (!isLodValid(static_cast<int>(geom), static_cast<int>(lod)))
                continue;
            const LocalBoundingBox& bb = getValidLod(static_cast<int>(geom), static_cast<int>(lod))->boundingBox;
            if (first)
            {
                outMin = bb.m_min;
                outMax = bb.m_max;
                first = false;
            }
            else
            {
                if (outMin.x > bb.m_min.x) outMin.x = bb.m_min.x;
                if (outMin.y > bb.m_min.y) outMin.y = bb.m_min.y;
                if (outMin.z > bb.m_min.z) outMin.z = bb.m_min.z;
                if (outMax.x <= bb.m_max.x) outMax.x = bb.m_max.x;
                if (outMax.y <= bb.m_max.y) outMax.y = bb.m_max.y;
                if (outMax.z <= bb.m_max.z) outMax.z = bb.m_max.z;
            }
        }
    }
    return !first;
}
// both (geom, lod) pairs valid and naming the same Lod object. The final
// compare indexes the lod vectors by the RAW lod index, not through the remap - as shipped.
bool CollisionMeshTemplate::sameInstance(int geomA, int lodA, int geomB, int lodB) const
{
    if (!isGeomValid(geomA)) return false;
    if (!isGeomValid(geomB)) return false;
    if (!isLodValid(geomA, lodA)) return false;
    if (!isLodValid(geomB, lodB)) return false;
    return m_lods[geomA].lods[lodA] == m_lods[geomB].lods[lodB];
}
// the three neighbours of `face` from the v10 adjacency table; false when the
// Lod has none. `out` is the u16 triple (BaseVector3<u16>) - written through its three shorts.
bool CollisionMeshTemplate::getAdjacentCollisionFaceIndices(int geom, int lod, unsigned int face,
        BaseVector3<unsigned short, VectorTraits3<unsigned short> >& out) const
{
    const CollisionLod* l = getValidLod(geom, lod);
    if (!l->faceAdjacency)
        return false;
    unsigned short* o = reinterpret_cast<unsigned short*>(&out);
    for (int i = 0; i < 3; ++i)
        o[i] = l->faceAdjacency[3 * face + i];
    return true;
}
// every valid geom must have a valid col 0.
bool CollisionMeshTemplate::verify() const
{
    for (unsigned int geom = 0; geom < m_lods.size(); ++geom)
        if (isGeomValid(static_cast<int>(geom)) && !isLodValid(static_cast<int>(geom), 0))
            return false;
    return true;
}

// Returns the closest accepted face for a ray, or false when no face or V8 tree exists.
// Outputs are normal then position; optional material IDs use the object's mapping (out-of-range maps to 0).
// faceInfo is stamped only for a strictly positive face index; face 0 leaves it unchanged.
bool CollisionMeshTemplate::getDistance(int geom, int lod, bool noFacingFilter,
        const Mat4& objectToWorld, const Mat4& worldToObject,
        const Vec3& rayStart, const Vec3& rayEnd, Vec3& outNormal, Vec3& outPosition,
        float& outPlaneDist, float& outAlong, int* outMaterial, CollisionFaceInfo* faceInfo,
        bool frontFaces, bool firstHit) const
{
    const CollisionLod* l = getValidLod(geom, lod);
    if (!l || !l->bsp.bspV8)
        return false;
    const float* o2w = objectToWorld.m;
    const float* w2o = worldToObject.m;

    // 1. into object space
    const float sx = rayStart.x - o2w[12], sy = rayStart.y - o2w[13], sz = rayStart.z - o2w[14];
    Vec3 localStart;
    localStart.x = w2o[4] * sy + w2o[0] * sx + w2o[8] * sz;
    localStart.y = w2o[9] * sz + w2o[5] * sy + w2o[1] * sx;
    localStart.z = sz * w2o[10] + sy * w2o[6] + sx * w2o[2];
    Vec3 localDir;                                              // rayEnd is used as a direction
    localDir.x = w2o[4] * rayEnd.y + w2o[0] * rayEnd.x + w2o[8] * rayEnd.z;
    localDir.y = w2o[9] * rayEnd.z + w2o[5] * rayEnd.y + w2o[1] * rayEnd.x;
    localDir.z = rayEnd.y * w2o[6] + rayEnd.x * w2o[2] + rayEnd.z * w2o[10];

    // 2. the tree
    Vec3 point;
    const BspNode::Face* face = l->bsp.getClosestFace(&point, &outAlong, &outPlaneDist,
                                                      localStart, localDir, l->boundingBox,
                                                      l->vertices, noFacingFilter, !frontFaces,
                                                      firstHit);
    if (!face)
        return false;

    // 3. [debug] draw the hit face - the toggle is compiled in at 0, never set by content
    if (s_debugDrawHitFace)
    {
        const Vec3* verts = getVertexVector(geom, lod);
        Vec3 w[3];
        const unsigned short vi[3] = { face->v0, face->v1, face->v2 };
        for (int i = 0; i < 3; ++i)
        {
            const Vec3& v = verts[vi[i]];
            w[i].x = v.z * o2w[8] + v.y * o2w[4] + o2w[0] * v.x + o2w[12];
            w[i].y = v.y * o2w[5] + o2w[1] * v.x + v.z * o2w[9] + o2w[13];
            w[i].z = v.z * o2w[10] + v.x * o2w[2] + v.y * o2w[6] + o2w[14];
        }
        // ELF: classManager->getSingleton("CollisionManager") -> +0x7C drawTriangle; the same
        // object as the world::collisionManager global.
        if (collisionManager)
            collisionManager->drawTriangle(w[0], w[1], w[2], 0xFF00007Fu);
    }

    // 4. back to world space
    const Vec3& n = face->normal;
    outNormal.x = o2w[4] * n.y + o2w[0] * n.x + o2w[8] * n.z;
    outNormal.y = o2w[9] * n.z + o2w[5] * n.y + o2w[1] * n.x;
    outNormal.z = n.z * o2w[10] + n.y * o2w[6] + n.x * o2w[2];
    outPosition.x = o2w[4] * point.y + o2w[0] * point.x + o2w[8] * point.z + o2w[12];
    outPosition.y = o2w[9] * point.z + o2w[5] * point.y + o2w[1] * point.x + o2w[13];
    outPosition.z = point.z * o2w[10] + point.y * o2w[6] + point.x * o2w[2] + o2w[14];

    // 5. the material
    if (outMaterial)
    {
        const unsigned short mat = face->material;
        if (m_materialIndex)
            *outMaterial = (static_cast<int>(mat) < m_materialCount) ? m_materialIndex[mat] : 0;
        else
            *outMaterial = mat;
    }

    // 6. the face index into the caller's CollisionFaceInfo (+0x02, u16) - only when > 0
    if (faceInfo)
    {
        const int index = l->bsp.getFaceIndex(face);
        if (index > 0)
            *reinterpret_cast<unsigned short*>(reinterpret_cast<unsigned char*>(faceInfo) + 2)
                = static_cast<unsigned short>(index);
    }
    return true;
}

// Tests the capsule p0->p0+axis with radius against a valid LOD and appends contacts.
// Output vectors are normals, positions and distances; only new entries transform back to world space.
// Returns whether this call hit anything. Missing V8 trees return false; invalid LOD pointers are unguarded.
bool CollisionMeshTemplate::getDistanceToCapsules(int geom, int lod,
        const Mat4& objectToWorld, const Mat4& worldToObject,
        const Vec3& p0, const Vec3& axis, float radius,
        std::vector<Vec3>& outNormals, std::vector<Vec3>& outPositions,
        std::vector<float>& outDistances) const
{
    const CollisionLod* l = getValidLod(geom, lod);
    if (!l->bsp.bspV8)
        return false;
    const float* o2w = objectToWorld.m;
    const float* w2o = worldToObject.m;

    // 1. into object space
    const float sx = p0.x - o2w[12], sy = p0.y - o2w[13], sz = p0.z - o2w[14];
    Vec3 lp0, laxis, lp1;
    lp0.x = w2o[8] * sz + w2o[4] * sy + w2o[0] * sx;
    lp0.y = w2o[9] * sz + w2o[5] * sy + w2o[1] * sx;
    lp0.z = sz * w2o[10] + sy * w2o[6] + sx * w2o[2];
    laxis.x = w2o[4] * axis.y + w2o[0] * axis.x + w2o[8] * axis.z;
    laxis.y = w2o[9] * axis.z + w2o[5] * axis.y + w2o[1] * axis.x;
    laxis.z = axis.z * w2o[10] + axis.y * w2o[6] + axis.x * w2o[2];
    lp1.x = laxis.x + lp0.x;
    lp1.y = laxis.y + lp0.y;
    lp1.z = laxis.z + lp0.z;

    // 2. gather
    m_temporaryFaces.resize(0);
    l->bsp.getCollidingFacesInsideCapsule(m_temporaryFaces, lp0, lp1, radius);

    // 3. test
    bool anyHit = false;
    const unsigned int firstNew = static_cast<unsigned int>(outDistances.size());
    const Vec3* verts = l->vertices.empty() ? 0 : &l->vertices[0];
    for (std::vector<const BspNode::Face*>::const_iterator it = m_temporaryFaces.begin();
         it != m_temporaryFaces.end(); ++it)
    {
        const BspNode::Face* f = *it;
        Vec3 normal;   normal.x = normal.y = normal.z = 0.0f;
        Vec3 position; position.x = position.y = position.z = 0.0f;
        float distance = 0.0f;
        if (checkCapsuleEdgesCollision(verts[f->v0], verts[f->v1], verts[f->v2], f->normal,
                                       lp0, lp1, radius, position, normal, distance))
        {
            anyHit = true;
            outNormals.push_back(normal);
            outPositions.push_back(position);
            outDistances.push_back(distance);
        }
    }

    // 4. the new entries back to world
    for (unsigned int i = firstNew; i < outDistances.size(); ++i)
    {
        Vec3& n = outNormals[i];
        const Vec3 nl = n;
        n.x = o2w[4] * nl.y + o2w[0] * nl.x + o2w[8] * nl.z;
        n.y = o2w[9] * nl.z + o2w[5] * nl.y + o2w[1] * nl.x;
        n.z = nl.z * o2w[10] + nl.y * o2w[6] + nl.x * o2w[2];
        Vec3& p = outPositions[i];
        const Vec3 pl = p;
        p.x = o2w[4] * pl.y + o2w[0] * pl.x + o2w[8] * pl.z + o2w[12];
        p.y = o2w[9] * pl.z + o2w[5] * pl.y + o2w[1] * pl.x + o2w[13];
        p.z = pl.z * o2w[10] + pl.y * o2w[6] + pl.x * o2w[2] + o2w[14];
        if (s_debugDrawQueries)
        {
            // ELF: DebugRenderer singleton +0x14 (CID_DebugSphereMeshTemplate, a 0x50-byte
            // sphere descriptor at radius 0.1) - a client debug renderer [FRONTIER]; then the
            // CollisionManager's +0x78 drawLine(position, position + normal, 0xFFFF).
            Vec3 tip;
            tip.x = n.x + p.x; tip.y = n.y + p.y; tip.z = n.z + p.z;
            if (collisionManager)
                collisionManager->drawLine(p, tip, 0xFFFFu);
        }
    }
    return anyHit;
}

// Queries a swept sphere; secondary is unused. A stationary sphere tests its endpoint.
// Moving spheres test shifted faces first and expanded finite edges only if no face hits.
// Renderer-only debug branches are omitted; their original switches default to false.
bool CollisionMeshTemplate::getDistanceToSphere(int geom, int lod, bool /*secondary*/,
        const Mat4& objectToWorld, const Mat4& worldToObject,
        const Vec3& center, const Vec3& velocity, float radius,
        std::vector<Vec3>& outNormals, std::vector<Vec3>& outPositions,
        std::vector<float>& outDistancesA, std::vector<float>& outDistancesB,
        std::vector<int>* outMaterials) const
{
#if defined(_WIN32)
    // [PLATFORM] PE 0x6BAC62..0x6BAC92 clamps the remapped slot before dereferencing it. The
    // symbolised ELF calls its unchecked getValidLod helper instead.
    const CollisionGeom& collisionGeom = m_lods[geom];
    int remappedLod = collisionGeom.colRemap[lod];
    const int lodCount = static_cast<int>(collisionGeom.lods.size());
    if (remappedLod >= lodCount)
        remappedLod = lodCount - 1;
    if (remappedLod < 0)
        remappedLod = 0;
    const CollisionLod* l = collisionGeom.lods[remappedLod];
#else
    const CollisionLod* l = getValidLod(geom, lod);
#endif
    if (!l->bsp.bspV8)
        return false;

    const float* o2w = objectToWorld.m;
    const float* w2o = worldToObject.m;

    // World -> object. The centre is translated and rotated; velocity is rotated only.
    const float sx = center.x - o2w[12];
    const float sy = center.y - o2w[13];
    const float sz = center.z - o2w[14];
    Vec3 localCenter;
    localCenter.x = w2o[4] * sy + w2o[0] * sx + w2o[8] * sz;
    localCenter.y = w2o[9] * sz + w2o[5] * sy + w2o[1] * sx;
    localCenter.z = sz * w2o[10] + sy * w2o[6] + sx * w2o[2];
    Vec3 localVelocity;
    localVelocity.x = w2o[4] * velocity.y + w2o[0] * velocity.x + w2o[8] * velocity.z;
    localVelocity.y = w2o[9] * velocity.z + w2o[5] * velocity.y + w2o[1] * velocity.x;
    localVelocity.z = velocity.z * w2o[10] + velocity.y * w2o[6] + velocity.x * w2o[2];
    Vec3 localEnd;
    localEnd.x = localCenter.x + localVelocity.x;
    localEnd.y = localCenter.y + localVelocity.y;
    localEnd.z = localCenter.z + localVelocity.z;

    // The stationary decision is deliberately made from the world velocity, before its rotation.
    const float stationarySpeedSquared = s_sphereStationarySpeed * s_sphereStationarySpeed;
    const bool stationary = velocity.x * velocity.x + velocity.y * velocity.y
                          + velocity.z * velocity.z <= stationarySpeedSquared;

    m_temporaryFaces.resize(0);
    if (stationary)
        l->bsp.getCollidingFacesInsideCapsule(m_temporaryFaces, localCenter, localCenter,
                                              radius * 1.5f);
    else
        l->bsp.getCollidingFacesInsideCapsule(m_temporaryFaces, localCenter, localEnd, radius);

    bool anyHit = false;
    bool faceHit = false;
    const unsigned int firstNew = static_cast<unsigned int>(outDistancesA.size());
    const Vec3* vertices = &l->vertices[0];

    if (s_sphereFacePass)
    {
        for (std::vector<const BspNode::Face*>::const_iterator it = m_temporaryFaces.begin();
             it != m_temporaryFaces.end(); ++it)
        {
            const BspNode::Face* face = *it;
            const Vec3* triangle[3] = {
                &vertices[face->v0], &vertices[face->v1], &vertices[face->v2]
            };
            Vec3 position;
            Vec3 normal;
            float distanceA = 0.0f;
            float distanceB = 0.0f;
            bool accepted;
            if (stationary)
            {
                accepted = checkSphereTriCollision(triangle, face->normal, localEnd, radius,
                                                    position, normal, distanceA)
                        && distanceA <= 0.0f;
                distanceB = distanceA;
            }
            else
            {
                normal = face->normal;
                accepted = checkShiftedFaceCollision(*triangle[0], *triangle[1], *triangle[2],
                                                      face->normal, localCenter, localEnd, radius,
                                                      position, distanceA, distanceB, objectToWorld)
                        && distanceA <= 0.0f;
            }

            if (!accepted)
                continue;
            anyHit = true;
            faceHit = true;
            outPositions.push_back(position);
            outNormals.push_back(normal);
            outDistancesA.push_back(distanceA);
            outDistancesB.push_back(distanceB);
            if (outMaterials)
                outMaterials->push_back(m_materialIndex ? m_materialIndex[face->material]
                                                        : static_cast<int>(face->material));
        }
    }

    // Face-pass results are still in object space. Transform only entries appended by this call.
    for (unsigned int i = firstNew; i < outDistancesA.size(); ++i)
    {
        Vec3& normal = outNormals[i];
        const Vec3 localNormal = normal;
        normal.x = o2w[4] * localNormal.y + o2w[0] * localNormal.x
                 + o2w[8] * localNormal.z;
        normal.y = o2w[9] * localNormal.z + o2w[5] * localNormal.y
                 + o2w[1] * localNormal.x;
        normal.z = localNormal.z * o2w[10] + localNormal.y * o2w[6]
                 + localNormal.x * o2w[2];

        Vec3& position = outPositions[i];
        const Vec3 localPosition = position;
        position.x = o2w[4] * localPosition.y + o2w[0] * localPosition.x
                   + o2w[8] * localPosition.z + o2w[12];
        position.y = o2w[9] * localPosition.z + o2w[5] * localPosition.y
                   + o2w[1] * localPosition.x + o2w[13];
        position.z = localPosition.z * o2w[10] + localPosition.y * o2w[6]
                   + localPosition.x * o2w[2] + o2w[14];
    }

    // Retail enters the finite-edge fallback only when the whole moving face pass found nothing.
    if (!stationary && !faceHit && s_sphereExpandedEdges)
    {
        for (std::vector<const BspNode::Face*>::const_iterator it = m_temporaryFaces.begin();
             it != m_temporaryFaces.end(); ++it)
        {
            const BspNode::Face* face = *it;
            Vec3 localPosition;
            Vec3 localNormal;
            float distanceA = 0.0f;
            float distanceB = 0.0f;
            if (!checkExpandedEdgesCollision(0u, vertices[face->v0], vertices[face->v1],
                                              vertices[face->v2], face->normal, localCenter,
                                              localEnd, radius, localPosition, localNormal,
                                              distanceA, distanceB, objectToWorld)
                || distanceA >= 0.0f)                  // strict `< 0` in retail
                continue;

            Vec3 worldNormal;
            worldNormal.x = o2w[4] * localNormal.y + o2w[0] * localNormal.x
                          + o2w[8] * localNormal.z;
            worldNormal.y = o2w[9] * localNormal.z + o2w[5] * localNormal.y
                          + o2w[1] * localNormal.x;
            worldNormal.z = localNormal.z * o2w[10] + localNormal.y * o2w[6]
                          + localNormal.x * o2w[2];
            if (worldNormal.y < 0.0f)
            {
                worldNormal.y = 0.0f;
                const float lengthSquared = worldNormal.x * worldNormal.x
                                          + worldNormal.z * worldNormal.z;
                if (lengthSquared < FLT_EPSILON)
                    continue;
                const float inverseLength = 1.0f / sqrtf(lengthSquared);
                worldNormal.x *= inverseLength;
                worldNormal.z *= inverseLength;
            }

            Vec3 worldPosition;
            worldPosition.x = o2w[4] * localPosition.y + o2w[0] * localPosition.x
                            + o2w[8] * localPosition.z + o2w[12];
            worldPosition.y = o2w[9] * localPosition.z + o2w[5] * localPosition.y
                            + o2w[1] * localPosition.x + o2w[13];
            worldPosition.z = localPosition.z * o2w[10] + localPosition.y * o2w[6]
                            + localPosition.x * o2w[2] + o2w[14];

            anyHit = true;
            outPositions.push_back(worldPosition);
            // Shipped quirk: disabling computed normals appends the untransformed face normal.
            outNormals.push_back(s_sphereComputedNormal ? worldNormal : face->normal);
            outDistancesA.push_back(distanceA);
            outDistancesB.push_back(distanceB);
            if (outMaterials)
                outMaterials->push_back(m_materialIndex ? m_materialIndex[face->material]
                                                        : static_cast<int>(face->material));
        }
    }
    return anyHit;
}

} // namespace world
} // namespace hfe
} // namespace dice
