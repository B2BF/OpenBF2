// Vegetation resource loading, atlas adoption and editor export.
// preLoad loads only the atlas; resource pre/post-load performs level initialization.
#ifdef BF2_IMAGE_RENDDX9
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../../Platform/X87Compat.h"
#endif
#include "OverGrowth.h"
#include "OverGrowthCell.h"                 // createOverGrowthCell (100D0C90)
#include "../MeshTemplate.h"                  // MeshBounds, meshBoundsExpand/Finalize, IGeometryTemplate
#include "../StaticMeshTemplate.h"            // StaticMeshTemplate::m_materials, StaticMeshMaterial, ShaderMaterial
#include "../GeometryTemplateManager.h"       // IGeometryTemplateManager, g_GeometryTemplateManager
#include "../../Core/Ids.h"                   // IID_IGeometry (dword_1021AE58 = 0x492FE0FE)
#include "../../Math/Mat4.h"
// XpLightManager.cpp's pattern: in the retail-ABI flavour HfeAbi.h already supplies IBase and the
// class-registration records, so the engine headers' own copies are suppressed around their include.
#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../Game/Objects/ObjectLayoutAccess.h"
#include "../../Physics/HeightmapCluster.h"
#include "../../Physics/Heightmap.h"
#include "../../Scene/Object/ObjectManager.h"
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../Core/StringUtil.h"             // getLowerCase (sub_10074E80)
#include "../../RendDX9/RendDX9Base.h"        // pathToLower, pathStripExtension, readStringSettingDefault,
                                              // textureManagerSwapDefaultMipClass
#include "../../RendDX9/Texture.h"            // EngineTexture, textureAddRef, textureReleaseRef
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/EngineFile.h"   // engineFileExists (FileManager +0x1C)
#include "../../../RendDX9Abi/Frontier.h"     // logf
#include "../../../RendDX9Abi/HostFileAbi.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/PluginEntry.h"  // classManager, fileManager
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace dice { namespace hfe {
// PluginEntry.cpp - two more cached host singletons (defined there beside the boot set; BF2Render.cpp
// declares objectManager the same way).
extern IBase* objectManager;
extern IBase* objectTemplateManager;
} }

namespace dice { namespace hfe { namespace geom {
namespace {



// The std::string operations the bodies use, in the shipped VC7.1 expansions.

// std::operator+(const string&, const char*) - VC7.1 inlines it as `copy the left, then +=`.
std::string concat(const std::string& left, const char* right)
{
    std::string result(left);
    result += right;
    return result;
}


// std::string::find(const char*, size_type off, size_type count) (MSVCP71 import at 100CD108 /
// 100CD22B, called with off 0 and count 5). The library algorithm: scan for the first character,
// then compare the whole needle; npos when absent.
const unsigned int kNpos = 0xFFFFFFFFu;
unsigned int find(const std::string& haystack, const char* needle, unsigned int count)
{
    const unsigned int size = haystack.size();
    if (count == 0)
        return 0;
    if (count > size)
        return kNpos;
    const char* const text = haystack.c_str();
    for (unsigned int i = 0; i + count <= size; ++i)
        if (text[i] == needle[0] && std::memcmp(text + i, needle, count) == 0)
            return i;
    return kNpos;
}

// Texture-name ordering is case-insensitive and stops at the first NUL.
struct StringLess
{
    bool operator()(const std::string& left, const std::string& right) const
    {
        // Ordering embedded in insert at 0x100CF6D3 and 0x100CF755.
        return ::_stricmp(left.c_str(), right.c_str()) < 0;
    }
};
typedef std::set<std::string, StringLess> TextureNameSet;   // set<std::string>, 0x2C-byte nodes

// std::operator<<(ostream&, const string&) (sub_10001C10, 509 B, the image's own instantiation).
// Every caller here streams with width 0, where the inserter's padding arms are dead, so the
// characters are written as they are.
std::ostream& operator<<(std::ostream& out, const std::string& value)
{
    return out.write(value.c_str(), static_cast<std::streamsize>(value.size()));
}

// sub_1000ED40 (34 B, __stdcall, read in full): fcomi/fcmovnb against the high bound first
// (moves when value >= high, ordered), then fcomi/fcmovb against the LOW bound (moves when below
// OR unordered). So a NaN comes back as `low`.
// SkyDome.cpp and SkinnedMesh.cpp carry file-local transcriptions of the same body that differ
// from this reading and from each other (NaN pass-through; `<=` at the low bound).
float clampFloat(float value, float low, float high)
{
    float result = value;
    if (result >= high)
        result = high;
    if (!(result >= low))
        result = low;
    return result;
}


// Host-side seams (RW-266 rule). Typed in both flavours where the tree's MSVC table matches the
// retail PE slot and no STL crosses; otherwise typed native / raw retail slot in the #else.

IGeometryTemplateManager* geometryTemplateManager()
{
    return static_cast<IGeometryTemplateManager*>(static_cast<void*>(g_GeometryTemplateManager));
}

// FileManager +0x2C readFile / +0x30 writeFile - HostFileAbi.h types both flavours.
bool hostReadFile(const std::string& path, void* buffer, unsigned int size, unsigned int& read)
{
    return ::dice::hfe::fileManager->readFile(renddx9abi::toHostString(path), buffer, size, read);
}

bool hostWriteFile(const std::string& path, const void* data, unsigned int size, bool flag)
{
    return ::dice::hfe::fileManager->writeFile(renddx9abi::toHostString(path), data, size, flag);
}

// ObjectTemplateManager PE +0x34. MSVC groups the five getTemplate overloads in reverse declaration
// order at the first one's position, so +0x34 is getTemplate(const std::string&) - the one-argument
// call 1015EBB8..1015EBC1 makes. A std::string crosses: typed native, raw retail slot otherwise.

// ObjectManager +0x34 getAllObjectsFromGrid - a std::map<unsigned int, IObject*> by reference.
// Typed native; the retail flavour walks the VC7.1 map with the inlined _Tree::iterator::_Inc the
// callers carry (0x100CBF4C..0x100CBF98, 0x100CC359..0x100CC397): _Isnil at node +0x15.
typedef std::map<unsigned int, world::IObject*> HostObjectMap;

HostObjectMap& allObjectsFromGrid()
{
    return reinterpret_cast<world::ObjectManager*>(::dice::hfe::objectManager)->getAllObjectsFromGrid();
}

class HostObjectWalk
{
public:
    HostObjectWalk() : m_map(allObjectsFromGrid()), m_it(m_map.begin()) {}
    bool done() const { return m_it == m_map.end(); }
    world::IObject* object() const { return m_it->second; }
    void next() { ++m_it; }
private:
    HostObjectMap&          m_map;
    HostObjectMap::iterator m_it;
    HostObjectWalk(const HostObjectWalk&);
    HostObjectWalk& operator=(const HostObjectWalk&);
};


// RendDX9's own geometry instances (StaticMesh & co.): not modelled as a C++ interface in this tree,
// so each slot is reached by offset through the instance's retail-ordered table - XpLightManager.cpp
// geometrySlot / NametagManager.cpp geometryBoundingRadius precedent. Both flavours lay these out
// identically (the objects are this image's).

inline void* geometrySlot(void* geometry, unsigned int offset)
{
    return (*reinterpret_cast<void***>(geometry))[offset / 4];
}
IGeometryTemplate* geometryGetTemplate(void* geometry)                         // +0x38
{
    typedef void* (__thiscall* Fn)(void*);
    return static_cast<IGeometryTemplate*>(reinterpret_cast<Fn>(geometrySlot(geometry, 0x38))(geometry));
}
MeshBounds* geometryLocalBounds(void* geometry)                                // +0x40 (no argument)
{
    typedef void* (__thiscall* Fn)(void*);
    return static_cast<MeshBounds*>(reinterpret_cast<Fn>(geometrySlot(geometry, 0x40))(geometry));
}
float geometryBoundingRadius(void* geometry)                                   // +0x44
{
    typedef float (__thiscall* Fn)(void*);
    return reinterpret_cast<Fn>(geometrySlot(geometry, 0x44))(geometry);
}

// The template whose first geom's first lod materials the loaders walk (StaticMeshTemplate.h's
// staticMeshFirstLodMaterialCount / staticMeshFirstLodMaterial: m_materials.first -> [0].first ->
// the lod-0 materials) - 100CD0A4..100CD0B8 / 100CC3BE..100CC3C7. The retail code reads the
// template this way with NO type check (vegetation is StaticMesh).
const StaticMeshTemplate* vegetationTemplate(IGeometryTemplate* geometryTemplate)
{
    return static_cast<const StaticMeshTemplate*>(geometryTemplate);
}
// A material's first texture reference, slot 1 of the ShaderMaterial (+0x28 -> +0x2C), and its name
// (+0x58) - 100CD0E3..100CD0EE / 100CC3FC..100CC405. No null tests, as shipped.
const std::string& materialTextureName(const StaticMeshMaterial* material)
{
    return static_cast<const dx9rend::EngineTexture*>(material->m_shaderMaterial->textures[1])->name;
}


// The patch array: operator new[] (1005F340) - the element count ahead of the first element - then
// the vector constructor iterator sub_10001000 (42 B, read: `count - 1; js` skips a count <= 0, else
// count forward constructions). A failed allocation leaves a null.
OverGrowthPatch* newPatchArray(int count)
{
    return new (std::nothrow) OverGrowthPatch[count];
}


// Grid helpers over the two byte maps (+0x80 density = material index per texel, +0x84 shadow).

// sub_100CA450 (296 B, read in full): zero the sixteen per-material maxima (+0x114..+0x150, the
// object's last field), then every patch (row by row) computes its coverage and raises the maxima on
// a strict, ordered greater-than.
void computePatchDensities(OverGrowth* self)
{
    for (unsigned int l = 0; l < 16; ++l)
        self->m_materialCoverageMax[l] = 0.0f;
    for (int z = 0; z < self->m_patchCountZ; ++z)
    {
        for (int x = 0; x < self->m_patchCountX; ++x)
        {
            OverGrowthPatch* const patch = self->m_patches + (self->m_patchCountX * z + x);
            computeCoverage(self, patch);
            for (unsigned int l = 0; l < 16; ++l)
                if (patch->coverage[l] > self->m_materialCoverageMax[l])
                    self->m_materialCoverageMax[l] = patch->coverage[l];
        }
    }
}

// The patch instance vector's push_back (the inline store when there is room, else _Insert_n
// 100D09B0 -> 100D0270).
void pushBackInstance(OverGrowthPatch* patch, OverGrowthInstance& instance)
{
    patch->instances.push_back(instance);
}


// Converts static overgrowth objects into patch instances and detaches their geometry component.
// Requires an in-range patch index and matching _lod template; missing templates are logged.

void addStaticObjects(OverGrowth* self)
{
    for (HostObjectWalk walk; !walk.done(); walk.next())
    {
        world::IObject* const object = walk.object();
        void* const geometry = world::object_detail::LayoutAccess::geometry(object);   // [esi+38h]
        if (!object->getIsOvergrowth() || !geometry)                                     // +0x12C
            continue;

        const int patchX = static_cast<int>(
            (object->getAbsoluteTransformation().m[12] - self->m_patchOrigin.x) /
            self->m_cellSize);                                                           // +0x80
        const int patchZ = static_cast<int>(
            (object->getAbsoluteTransformation().m[14] - self->m_patchOrigin.z) /
            self->m_cellSize);                                                           // +0x80
        OverGrowthPatch* const patch = self->m_patches + (patchZ * self->m_patchCountX + patchX);

        const std::string name(geometryGetTemplate(geometry)->getName());       // +0x38, then +0x18
        IGeometryTemplate* lodTemplate = 0;
        {
            const std::string lodName = concat(name, "_lod");
            lodTemplate = geometryTemplateManager()->getTemplate(renddx9abi::toHostString(lodName));
        }
        if (!lodTemplate)
        {
            // sub_10074300(6, __FILE__, 964, "") << ... ; sub_10074690 - dice's debug stream, kept as
            // one log line as every recovered diagnostic in this image is.
            ::renddx9abi::logf("OverGrowth.cpp:964 Growth can't find geometryTemplate: %s_lod",
                               name.c_str());
            continue;
        }

        OverGrowthInstance instance;
        constructInstance(instance);
        instance.geometryIndex = self->findOrAddGeometry(name);
        instance.transform = object->getAbsoluteTransformation();                  // +0x80
        instance.cullRadius = geometryBoundingRadius(geometry) * instance.scale;
        instance.radiusToSame = geometryBoundingRadius(geometry) * instance.scale;
        instance.radiusToOthers = geometryBoundingRadius(geometry) * instance.scale;
        instance.generated = 0;
        instance.shadow = sampleShadow(self, instance.transform.m[12], instance.transform.m[14]);
        pushBackInstance(patch, instance);

        OverGrowthBoxCorners box;
        constructBoxCorners(box, *geometryLocalBounds(geometry), instance.transform);
        boundsExpandByCorners(patch->bounds, box);
        object->setComponent(IID_IGeometry, 0);                                    // +0x54
    }
}


// sub_100CC1F0 (782 B, retn 4, read in full) - the texture files the overgrowth draws.
// The distinct geometry templates of every pair's "_lod" record, plus - only when +0xF0 is set - the
// "_lod" templates of the static overgrowth objects (here getIsOvergrowth runs before the geometry
// field is read). Every template is load()ed (+0x2C) in set (pointer) order, and each material of its
// first geom's first lod adds "<texture name, separators '/', extension cut>.dds" to `out`.
// Callers: postLoadResources (the set is discarded - the loads are the effect), save, generateAtlas.

// PE 0x100CC1F0
void collectTextureNames(OverGrowth* self, TextureNameSet& out)
{
    std::set<IGeometryTemplate*> templates;
    const std::vector<OverGrowthGeometryPair>& geometries = self->m_geometries;
    for (unsigned int i = 0; i < geometries.size(); ++i)
        templates.insert(geometryGetTemplate(geometries[i].lod.geometry));

    if (self->m_editorEnable)
    {
        for (HostObjectWalk walk; !walk.done(); walk.next())
        {
            world::IObject* const object = walk.object();
            if (!object->getIsOvergrowth())                                               // +0x12C
                continue;
            void* const geometry = world::object_detail::LayoutAccess::geometry(object); // +0x38
            if (!geometry)
                continue;
            const std::string name(geometryGetTemplate(geometry)->getName());
            IGeometryTemplate* lodTemplate = 0;
            {
                const std::string lodName = concat(name, "_lod");
                lodTemplate = geometryTemplateManager()->getTemplate(renddx9abi::toHostString(lodName));
            }
            if (lodTemplate)
                templates.insert(lodTemplate);
        }
    }

    for (std::set<IGeometryTemplate*>::const_iterator it = templates.begin(); it != templates.end(); ++it)
    {
        IGeometryTemplate* const geometryTemplate = *it;
        geometryTemplate->load();                                                          // +0x2C
        const unsigned int count = staticMeshFirstLodMaterialCount(vegetationTemplate(geometryTemplate));
        for (unsigned int k = 0; k < count; ++k)
        {
            const StaticMeshMaterial* const material =
                staticMeshFirstLodMaterial(vegetationTemplate(geometryTemplate), k);
            std::string stripped;
            dx9rend::pathStripExtension(stripped, materialTextureName(material));
            out.insert(concat(stripped, ".dds"));
        }
    }
}

// The atlas owner's delete: `if (p) { sub_10015930(p); operator delete(p) }`.
void deleteAtlas(dx9rend::TextureAtlas* atlas)
{
    if (atlas)
    {
        dx9rend::textureAtlasDtor(atlas);
        ::renddx9abi::engineFree(atlas);
    }
}

// operator new(0x14) + sub_10015E60, a failed allocation giving null.
dx9rend::TextureAtlas* newAtlas()
{
    void* const memory = ::renddx9abi::engineAlloc(sizeof(dx9rend::TextureAtlas));
    return memory ? dx9rend::textureAtlasCtor(memory) : 0;
}

// The page adoption postLoadResources (100CD747..100CD799) and generateAtlas (100CCF65..100CCFB7)
// share: with an atlas, its first page (sub_10015850's distinct-page list, a local vector) becomes
// +0x100 and is addRef'd through its table's +0x08. A previous +0x100 is NOT released here (clear does
// that) - shipped.
void adoptFirstAtlasPage(OverGrowth* self)
{
    if (!self->m_atlas)
        return;
    std::vector<dx9rend::EngineTexture*> pages;
    dx9rend::textureAtlasCollectPages(self->m_atlas, pages);
    if (!pages.empty())
    {
        dx9rend::EngineTexture* const page = pages[0];
        self->m_atlasPage = page;
        dx9rend::textureAddRef(page);                                                 // vslot +0x08
    }
}

// sub_100C8790 (131 B, __fastcall(ostream&, const Mat4&), read in full): "[a/b/c/d]" per row, four rows.
std::ostream& writeMatrix(std::ostream& out, const Mat4& matrix)
{
    const float* row = matrix.m;
    for (unsigned int r = 0; r < 4; ++r, row += 4)
        out << '[' << row[0] << '/' << row[1] << '/' << row[2] << '/' << row[3] << ']';
    return out;
}

// sub_100CAA50 (786 B, read in full) - "<path>OvergrowthCollision.con": per patch a "rem Patch: x, z"
// header and, for every instance with a geometry index >= 0 AND flag +0x58 set, an Object.create of
// the named record (+0x50) with its transform and isOvergrowth; then the patch and object totals.
// Returns at once when there is no patch array.
void saveCollision(OverGrowth* self)
{
    if (!self->m_patches)
        return;
    std::ostringstream out;
    unsigned int objects = 0;
    for (int z = 0; z < self->m_patchCountZ; ++z)
    {
        for (int x = 0; x < self->m_patchCountX; ++x)
        {
            const OverGrowthPatch* const patch = self->m_patches + (self->m_patchCountX * z + x);
            out << "rem Patch: " << x << ", " << z << std::endl << std::endl;
            const OverGrowthInstanceVector& instances = patch->instances;
            for (unsigned int k = 0; k < instances.size(); ++k)
            {
                const OverGrowthInstance* const instance = &instances[k];
                if (instance->geometryIndex < 0 || !instance->generated)
                    continue;
                const OverGrowthGeometryPair* const pair =
                    self->m_geometries.data() + instance->geometryIndex;
                out << "Object.create " << pair->full.name << std::endl;
                writeMatrix(out << "Object.absoluteTransformation ",
                            instance->transform) << std::endl;
                out << "Object.isOvergrowth 1" << std::endl;
                ++objects;
                out << std::endl;
            }
        }
    }
    out << "rem Patches: " << self->m_patchCountX << "x" << self->m_patchCountZ << std::endl;
    out << "rem Objects: " << objects << std::endl;

    const std::string sizeText = out.str();                                               // str() #1: size
    const std::string dataText = out.str();                                               // str() #2: data
    hostWriteFile(concat(self->m_path, "OvergrowthCollision.con"),
                  dataText.c_str(), static_cast<unsigned int>(sizeText.size()), true);  // FileManager +0x30
}

// sub_100C98A0 / sub_100C9910 (107 B each, read in full): the two byte maps written back whole.
// These names carry a leading backslash ("\\Overgrowth.raw") that initialize's reads do not.
void saveDensityMap(OverGrowth* self)
{
    const unsigned int bytes = self->m_mapHeight * self->m_mapWidth;
    hostWriteFile(concat(self->m_path, "\\Overgrowth.raw"),
                  self->m_densityMap, bytes, true);
}
// PE 0x100C9910 (the shadow-map twin of saveDensityMap).
void saveShadowMap(OverGrowth* self)
{
    const unsigned int bytes = self->m_mapHeight * self->m_mapWidth;
    hostWriteFile(concat(self->m_path, "\\OvergrowthShadowmap.raw"),
                  self->m_shadowMap, bytes, true);
}

} // namespace


// PE 0x100CB050: unreachable in 1.50: no code or data references. The destructor is the instance
// vector's own: its storage (+0x10) is released and the begin/end/capacity words (+0x10/+0x14/+0x18)
// are zeroed, which the member destructor does.
OverGrowthPatch::~OverGrowthPatch()
{
}

// sub_100CAFF0 (86 B, read in full): position and the vector zeroed, the bounds zeroed with their
// dirty byte (+0x7C) set; coverage[] is left unconstructed.
OverGrowthPatch::OverGrowthPatch()
    : instances()
{
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    bounds.min.x = 0.0f;
    bounds.min.y = 0.0f;
    bounds.min.z = 0.0f;
    bounds.max.x = 0.0f;
    bounds.max.y = 0.0f;
    bounds.max.z = 0.0f;
    bounds.originRadius = 0.0f;
    bounds.radius = 0.0f;
    bounds.dirty = 1;
    bounds.extent.x = 0.0f;
    bounds.extent.y = 0.0f;
    bounds.extent.z = 0.0f;
    bounds.center.x = 0.0f;
    bounds.center.y = 0.0f;
    bounds.center.z = 0.0f;
}


// Initializes maps and geometry indices from the primary heightmap, then applies view distance.
// Requires heightmap singletons; missing raw maps retain zero density/full shadow defaults.

// PE 0x1015F5F0
void OverGrowth::initialize()
{
    IBase* const cluster = ::dice::hfe::classManager->getSingleton(
        renddx9abi::HostString("HeightmapCluster"));                               // +0x48
    m_cluster = static_cast<world::IHeightmapCluster*>(static_cast<void*>(cluster));
    world::IHeightmap* const heightmap =
        static_cast<world::IHeightmapCluster*>(static_cast<void*>(cluster))->getPrimaryHeightmap();   // +0x40
    m_heightmap = heightmap;
    int sizeX;
    int sizeZ;
    heightmap->getSize(sizeX, sizeZ);                                                 // +0x2C
    m_mapWidth = static_cast<unsigned int>(sizeX);
    m_mapHeight = static_cast<unsigned int>(sizeZ);

    {
        const Vec3& scale = m_heightmap->getScale();                               // +0x54
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] The unsigned FILD conversion stays exact until the product store.
        m_extentX = bf2x87::store(bf2x87::mul(bf2x87::Real(m_mapWidth), bf2x87::Real(scale.x)));
#else
        m_extentX = static_cast<float>(m_mapWidth) * scale.x;
#endif
    }
    {
        const Vec3& scale = m_heightmap->getScale();                               // +0x54 again
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        m_extentZ = bf2x87::store(bf2x87::mul(bf2x87::Real(m_mapHeight), bf2x87::Real(scale.z)));
#else
        m_extentZ = static_cast<float>(m_mapHeight) * scale.z;
#endif
    }
    const float halfX = m_extentX * -0.5f;                                            // ds:102089FC
    m_originX = halfX;
    const float halfZ = m_extentZ * -0.5f;
    m_originZ = halfZ;
    m_patchOrigin.y = 0.0f;
    m_patchOrigin.x = halfX;
    m_patchOrigin.z = halfZ;

    unsigned char* const density = static_cast<unsigned char*>(::renddx9abi::engineAlloc(
        m_mapHeight * m_mapWidth));
    m_densityMap = density;
    std::memset(density, 0, m_mapHeight * m_mapWidth);
    unsigned char* const shadow = static_cast<unsigned char*>(::renddx9abi::engineAlloc(
        m_mapHeight * m_mapWidth));
    m_shadowMap = shadow;
    std::memset(shadow, 0xFF, m_mapHeight * m_mapWidth);

    unsigned int bytesRead;                                                            // var_10, output only
    const std::string& path = m_path;
    if (::renddx9abi::engineFileExists(concat(path, "Overgrowth.raw")))               // +0x1C
    {
        const unsigned int bytes = m_mapHeight * m_mapWidth;
        hostReadFile(concat(path, "Overgrowth.raw"), m_densityMap, bytes,
                     bytesRead);                                                        // +0x2C
    }
    if (::renddx9abi::engineFileExists(concat(path, "OvergrowthShadowmap.raw")))
    {
        const unsigned int bytes = m_mapHeight * m_mapWidth;
        hostReadFile(concat(path, "OvergrowthShadowmap.raw"), m_shadowMap,
                     bytes, bytesRead);
    }

    setViewDistance(m_viewDistance);                                                  // vtable +0x0C

    std::vector<OverGrowthMaterial>& layers = m_materials;
    for (unsigned int i = 0; i < layers.size(); ++i)
    {
        OverGrowthMaterial& layer = layers[i];
        std::vector<OverGrowthType>& types = layer.types;
        for (unsigned int j = 0; j < types.size(); ++j)
        {
            OverGrowthType& type = types[j];
            type.geometryIndex = findOrAddGeometry(type.geometry);
        }
    }
    m_initialized = 1;
}


// Initializes resources and patch bounds at terrain height, then adopts static objects unless disabled.
// Empty paths leave state unchanged.

void OverGrowth::preLoadResources()
{
    if (m_path.empty())
        return;
    OverGrowth::initialize();

    m_patches = newPatchArray(m_patchCountX * m_patchCountZ);
    for (int z = 0; z < m_patchCountZ; ++z)
    {
        const float rowZ = static_cast<float>(z);
        for (int x = 0; x < m_patchCountX; ++x)
        {
            OverGrowthPatch* const patch = m_patches + (m_patchCountX * z + x);
            const float offsetZ = rowZ * m_cellSize;
            patch->position.x = static_cast<float>(x) * m_cellSize + m_patchOrigin.x;
            patch->position.y = m_patchOrigin.y;
            patch->position.z = offsetZ + m_patchOrigin.z;

            const float half = m_cellSize * 0.5f;                                       // ds:101DD4F0
            const float height = m_cluster->getHeightInWorldCoords(
                half + patch->position.x, half + patch->position.z);                    // +0x6C
            patch->position.y = height;

            const float centreHalf = m_cellSize * 0.5f;
            const float centreX = centreHalf + patch->position.x;
            const float centreZ = centreHalf + patch->position.z;
            MeshBounds& bounds = patch->bounds;
            bounds.min.x = centreX;
            bounds.min.y = height;
            bounds.min.z = centreZ;
            bounds.dirty = 1;
            bounds.max.x = centreX;
            bounds.max.y = height;
            bounds.max.z = centreZ;
            bounds.dirty = 1;
        }
    }

    if (!m_editorEnable)
        addStaticObjects(this);
}


// Finalizes patch densities, normalizes type weights and allocates cell buffers.
// Adopts the atlas page and rebuilds cells; an empty resource path does nothing.

void OverGrowth::postLoadResources()
{
    if (m_path.empty())
        return;

    // per geometry: the vegetation stream sizes and the lod fade (skipped while reloading)
    if (!m_reloading)
    {
        for (unsigned int i = 0; i < m_geometries.size(); ++i)
        {
            for (unsigned int r = 0; r < 2; ++r)                      // the named record, then "_lod"
            {
                OverGrowthGeometryRecord* const record =
                    r == 0 ? &m_geometries[i].full : &m_geometries[i].lod;
                IGeometryTemplate* const geometryTemplate =
                    geometryGetTemplate(record->geometry);                             // +0x38
                for (unsigned int k = 0;
                     k < staticMeshFirstLodMaterialCount(vegetationTemplate(geometryTemplate)); ++k)
                {
                    const StaticMeshMaterial* const material =
                        staticMeshFirstLodMaterial(vegetationTemplate(geometryTemplate), k);
                    const std::string lowered = getLowerCase(materialTextureName(material));
                    unsigned int tiled = 0;
                    if (find(lowered, "tile_", 5) != kNpos)
                        tiled = 1;
                    record->vertexCounts[tiled] += material->m_vegetationVertexCount;
                    record->indexCounts[tiled] += material->m_vegetationIndexCount;
                }                                                   // `lowered` dies here (100CD13F)
                MeshBounds* const bounds = geometryLocalBounds(record->geometry);      // +0x40
                if (bounds->dirty)
                    meshBoundsFinalize(*bounds);
                const float scaled = bounds->radius * 0.2f;                             // ds:101DDF30
                record->distanceScale = clampFloat(scaled * scaled, 0.3f, 1.0f);
            }
        }
    }

    computePatchDensities(this);

    // per layer: the geometry each unit of coverage costs, and the type weights
    unsigned int layerVertices[32];                                                     // var_100
    unsigned int layerIndices[32];                                                      // var_80
    std::memset(layerVertices, 0, sizeof(layerVertices));
    std::memset(layerIndices, 0, sizeof(layerIndices));
    for (unsigned int i = 0; i < m_materials.size(); ++i)
    {
        OverGrowthMaterial& layer = m_materials[i];
        for (unsigned int j = 0; j < 2; ++j)
        {
            float densitySum = 0.0f;                                                    // ds:101DDE64
            const unsigned int slot = layer.id * 2 + j;
            layerVertices[slot] = 0;
            layerIndices[slot] = 0;
            std::vector<OverGrowthType>& types = layer.types;
            for (unsigned int k = 0; k < types.size(); ++k)
            {
                OverGrowthType& type = types[k];
                const int perCell = static_cast<int>(m_cellSize * m_cellSize *
                                                     type.density * 0.001f);          // ds:101DDE54
                const OverGrowthGeometryPair& pair = m_geometries.data()[type.geometryIndex];
                layerVertices[layer.id * 2 + j] += pair.lod.vertexCounts[j] * perCell;
                densitySum += type.density;
                layerIndices[layer.id * 2 + j] += pair.lod.indexCounts[j] * perCell;
            }
            for (unsigned int k = 0; k < types.size(); ++k)
            {
                OverGrowthType& type = types[k];
                type.probability = type.density / densitySum;
            }
        }
    }

    // the largest per-patch vertex / index counts, per stream
    m_streamVertexCounts[0] = 10;
    m_streamVertexCounts[1] = 10;
    m_streamIndexCounts[0] = 10;
    m_streamIndexCounts[1] = 10;
    for (int z = 0; z < m_patchCountZ; ++z)
    {
        for (int x = 0; x < m_patchCountX; ++x)
        {
            const OverGrowthPatch* const patch = m_patches + (m_patchCountX * z + x);
            for (unsigned int j = 0; j < 2; ++j)
            {
                unsigned int vertices = 0;
                unsigned int indices = 0;
                for (unsigned int l = 0; l < 16; ++l)
                {
                    vertices += static_cast<int>(static_cast<float>(layerVertices[l * 2 + j]) *
                                                 patch->coverage[l]);
                    indices += static_cast<int>(static_cast<float>(layerIndices[l * 2 + j]) *
                                                patch->coverage[l]);
                }
                const OverGrowthInstanceVector& instances = patch->instances;
                for (unsigned int k = 0; k < instances.size(); ++k)
                {
                    const OverGrowthInstance& instance = instances[k];
                    if (instance.generated)
                        continue;
                    const OverGrowthGeometryPair& pair = m_geometries.data()[instance.geometryIndex];
                    vertices += pair.lod.vertexCounts[j];
                    indices += pair.lod.indexCounts[j];
                }
                if (vertices > m_streamVertexCounts[j])
                    m_streamVertexCounts[j] = vertices;
                if (indices > m_streamIndexCounts[j])
                    m_streamIndexCounts[j] = indices;
            }
        }
    }

    // the N x N cells
    const int gridSize = m_gridSize;
    m_cells = static_cast<OverGrowthCell**>(
        ::renddx9abi::engineAlloc(static_cast<unsigned int>(gridSize * gridSize) * sizeof(OverGrowthCell*)));
    for (int z = 0; z < m_gridSize; ++z)
    {
        for (int x = 0; x < m_gridSize; ++x)
        {
            OverGrowthCell** const slot = m_cells + (m_gridSize * z + x);
            *slot = createOverGrowthCell();
        }
    }

    // the atlas, the fade state, and the first cell build
    {
        TextureNameSet names;
        collectTextureNames(this, names);
        adoptFirstAtlasPage(this);
        m_fadeBand = 0.0f;
        m_minimumHeightFade = 1.0f;
        m_loaded = 1;
        rebuildCells();                                                                 // vtable +0x14
    }
}


// sub_100C9CD0 (359 B, read in full) - resolve still-unresolved type geometry indices (index < 0,
// signed), then create the atlas owner and load "<path>OvergrowthAtlas.tai" with the texture manager's
// default mip class forced to 9 around the load; a failed load deletes the owner again. A previous
// owner at +0xFC is NOT deleted here - shipped (reload clears first; the console's direct call does
// not). Callers: reload and the console command at 100C5538.

// PE 0x100C9CD0
void OverGrowth::preLoad()
{
    OverGrowth* const self = this;
    std::vector<OverGrowthMaterial>& layers = self->m_materials;
    for (unsigned int i = 0; i < layers.size(); ++i)
    {
        OverGrowthMaterial& layer = layers[i];
        std::vector<OverGrowthType>& types = layer.types;
        for (unsigned int j = 0; j < types.size(); ++j)
        {
            OverGrowthType& type = types[j];
            if (type.geometryIndex < 0)
                type.geometryIndex = self->findOrAddGeometry(type.geometry);
        }
    }

    self->m_atlas = newAtlas();
    const int previousMipClass = dx9rend::textureManagerSwapDefaultMipClass(9);
    bool loaded;
    {
        const std::string file = concat(self->m_path, "OvergrowthAtlas.tai");
        loaded = dx9rend::textureAtlasLoadTAI(self->m_atlas, file);
    }
    if (!loaded)
    {
        deleteAtlas(self->m_atlas);
        self->m_atlas = 0;
    }
    dx9rend::textureManagerSwapDefaultMipClass(previousMipClass);
}


// Regenerates the editor atlas using Editor_LevelSavePath, falling back to GSModDirectory.
// Reloads its first page with mip class 9. The previous page is overwritten without release.

void OverGrowth::generateAtlas()
{
    OverGrowth* const self = this;
    TextureNameSet names;
    collectTextureNames(self, names);
    std::list<std::string> files(names.begin(), names.end());

    std::string saveDirectory;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", "", saveDirectory);          // +0x40
    if (saveDirectory.size() == 0)
        dx9rend::readStringSettingDefault("GSModDirectory", "", saveDirectory);            // +0x40

    deleteAtlas(self->m_atlas);
    self->m_atlas = newAtlas();
    const int previousMipClass = dx9rend::textureManagerSwapDefaultMipClass(9);
    dx9rend::textureAtlasGenerate(self->m_atlas, saveDirectory,
                                  concat(self->m_path, "OvergrowthAtlas"),
                                  saveDirectory, files, true);
    if (!dx9rend::textureAtlasLoadTAI(self->m_atlas, concat(self->m_path, "OvergrowthAtlas.tai")))
    {
        deleteAtlas(self->m_atlas);
        self->m_atlas = 0;
    }
    adoptFirstAtlasPage(self);
    dx9rend::textureManagerSwapDefaultMipClass(previousMipClass);
}


// sub_100CD7E0 (1082 B, read in full) - the editor save: regenerates "<path>Overgrowth.con" (settings,
// then per layer its material line and per type its six properties, then path and preLoad), writes
// the collision script and both byte maps, and - unless reloading - rebuilds the atlas when the
// overgrowth draws any texture. Callers: reload, the console command at 100C5E09.

void OverGrowth::save()
{
    OverGrowth* const self = this;
    std::ostringstream out;
    out << "Overgrowth.viewDistance " << self->m_viewDistance << std::endl;
    out << "Overgrowth.viewDistanceScale " << self->m_viewDistanceScale << std::endl;
    out << "Overgrowth.closeDistPercentage " << self->m_closeDistPercentage << std::endl;
    out << std::endl;
    const std::vector<OverGrowthMaterial>& layers = self->m_materials;
    for (const OverGrowthMaterial* layer = layers.data(); layer != layers.data() + layers.size();
         ++layer)
    {
        out << "Overgrowth.addMaterial " << layer->name << " "
            << layer->id << std::endl;
        out << "Overgrowth.setActiveMaterial " << layer->name << std::endl;
        const std::vector<OverGrowthType>& types = layer->types;
        for (const OverGrowthType* type = types.data(); type != types.data() + types.size(); ++type)
        {
            out << "Overgrowth.addType " << type->name << std::endl;
            out << "Overgrowth.setActiveType " << type->name << std::endl;
            out << "OvergrowthType.geometry " << type->geometry << std::endl;
            out << "OvergrowthType.density " << type->density << std::endl;
            out << "OvergrowthType.normalScale " << type->normalScale << std::endl;
            out << "OvergrowthType.rotationScale " << type->rotationScale << std::endl;
            out << "OvergrowthType.minRadiusToSame " << type->minRadiusToSame << std::endl;
            out << "OvergrowthType.minRadiusToOthers " << type->minRadiusToOthers << std::endl;
        }
        out << std::endl;
    }
    out << "Overgrowth.path " << self->m_path << std::endl;
    out << "Overgrowth.preLoad" << std::endl;
    {
        const std::string sizeText = out.str();                                           // str() #1
        const std::string dataText = out.str();                                           // str() #2
        hostWriteFile(concat(self->m_path, "Overgrowth.con"),
                      dataText.c_str(), static_cast<unsigned int>(sizeText.size()), true);
    }
    saveCollision(self);
    saveDensityMap(self);
    saveShadowMap(self);
    if (!self->m_reloading)
    {
        TextureNameSet names;
        collectTextureNames(self, names);
        if (names.size() > 0)
            self->generateAtlas();
    }
}

} } }
#endif
