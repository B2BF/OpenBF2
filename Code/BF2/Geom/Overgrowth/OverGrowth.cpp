// Owns overgrowth geometry, resources, settings, and instance tables.
#ifdef BF2_IMAGE_RENDDX9
#include "OverGrowth.h"
#include "OverGrowthCell.h"
#include "../MeshTemplate.h"                  // IGeometryTemplate (load, createInstance)
#include "../GeometryTemplateManager.h"       // IGeometryTemplateManager, g_GeometryTemplateManager
// XpLightManager.cpp's pattern: in the retail-ABI flavour HfeAbi.h already supplies IBase and the
// class-registration records, so the engine headers' own copies are suppressed around their include.
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../Physics/HeightmapCluster.h"
#include "../../Physics/Heightmap.h"
#include "../ITerrainEditable.h"              // the painters' overlays (IID 820005)
#include "../Terrain.h"                       // the "Terrain" singleton's patch size / count
#include "../../RendDX9/Texture.h"            // EngineTexture, textureReleaseRef (10014E20)
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/PluginEntry.h"  // classManager
#include <string>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <new>
#include <set>
#include <string>

namespace dice { namespace hfe {
// PluginEntry.cpp - the cached host ObjectTemplateManager (0x10244ADC), declared as BF2Render.cpp
// declares its siblings.
extern IBase* objectTemplateManager;
} }

namespace dice { namespace hfe { namespace geom {
namespace {

// The two owner tables and the two "active" cursors the console drives.
//   +0x3C  vector<OverGrowthMaterial>
//   +0x4C  vector<OverGrowthGeometryPair> (0xA0-byte records in the PE)
//   +0xE8  OverGrowthMaterial*  the active material   (setActiveMaterial; addMaterial nulls it)
//   +0xEC  OverGrowthType*      the active type        (setActiveType; OvergrowthType.* act on it)
std::vector<OverGrowthMaterial>& materialTable(OverGrowth* self) { return self->m_materials; }
const std::vector<OverGrowthMaterial>& materialTable(const OverGrowth* self) { return self->m_materials; }
OverGrowthMaterial*& activeMaterial(OverGrowth* self) { return self->m_activeMaterial; }
OverGrowthMaterial* activeMaterial(const OverGrowth* self) { return self->m_activeMaterial; }
OverGrowthType*& activeType(OverGrowth* self) { return self->m_activeType; }
OverGrowthType* activeType(const OverGrowth* self) { return self->m_activeType; }

// std::operator==(const string&, const string&) from MSVCP71 - compare(): same length and the
// same bytes (memcmp over the length).
bool sameString(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && std::memcmp(a.c_str(), b.c_str(), a.size()) == 0;
}

// sub_10075080 (fastcall, 63 B) - std::string equality through _stricmp on the two c_str()s.
bool equalsNoCase(const std::string& a, const std::string& b)
{
    return ::_stricmp(a.c_str(), b.c_str()) == 0;
}

// std::operator+(const string&, const char*) - VC7.1 inlines it as `copy the left, then +=`.
std::string concat(const std::string& left, const char* right)
{
    std::string result(left);
    result += right;
    return result;
}

// sub_10074AB0 - the image's printf-into-a-std::string helper, reproduced TU-locally as
// RaShader.cpp, QuadPatch.cpp and NametagManager.cpp already do (internal linkage each). One
// shared 4096-byte buffer, a 0xFFF limit, and an overflow replaced by the fixed warning.
char g_overGrowthFormatBuffer[0x1000];

std::string overGrowthFormatString(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = ::_vsnprintf(g_overGrowthFormatBuffer, 0xFFF, format, args);
    va_end(args);

    if (written != -1)
        g_overGrowthFormatBuffer[written] = '\0';
    else
        ::sprintf(g_overGrowthFormatBuffer, "%s",
                  "WARNING: The original message was too long for formatString(). \n");
    return std::string(g_overGrowthFormatBuffer);
}


// Geometry-table entries pair LOD and full-detail records, each with its instance cache.
// Geometry instances retain the same virtual order across host ABI flavors.
inline void* geometrySlot(void* geometry, unsigned int offset)
{
    return (*reinterpret_cast<void***>(geometry))[offset / 4];
}
void geometryAddRef(void* geometry)                                           // +0x00
{
    typedef int (__thiscall* Fn)(void*);
    reinterpret_cast<Fn>(geometrySlot(geometry, 0x00))(geometry);
}
void geometryRelease(void* geometry)                                          // +0x08
{
    typedef int (__thiscall* Fn)(void*);
    reinterpret_cast<Fn>(geometrySlot(geometry, 0x08))(geometry);
}

IGeometryTemplateManager* geometryTemplateManager()
{
    return static_cast<IGeometryTemplateManager*>(static_cast<void*>(g_GeometryTemplateManager));
}

// ObjectTemplateManager PE +0x34. MSVC groups the five getTemplate overloads in reverse declaration
// order at the first one's position, so +0x34 is getTemplate(const std::string&) - the one-argument
// call 1015EBB8..1015EBC1 makes. A std::string crosses: typed native, raw retail slot otherwise.
world::IObjectTemplate* objectTemplateByName(const std::string& name)
{
    return reinterpret_cast<world::ObjectTemplateManager*>(::dice::hfe::objectTemplateManager)
        ->getTemplate(renddx9abi::toHostString(name));
}

} // anonymous namespace

// sub_1015EB80 (123 B, read in full): members in declaration order (+0x1C, +0x30, +0x34, the vector,
// +0x4C = 1.0), the two count pairs in the body, then the two template lookups. +0x38 is not
// written. NO null test on the geometry template before load()/createInstance() - shipped.
OverGrowthGeometryRecord::OverGrowthGeometryRecord(const std::string& templateName)
    : name(templateName), geometry(0), objectTemplate(0), geometryTemplate(0), pool(),
      distanceScale(1.0f)
{
    vertexCounts[0] = 0;
    vertexCounts[1] = 0;
    indexCounts[0] = 0;
    indexCounts[1] = 0;

    world::IObjectTemplate* const found = objectTemplateByName(name);
    objectTemplate = found;
    if (found)
        found->preCache();                                                      // IObjectTemplate +0x68

    geometryTemplate = geometryTemplateManager()->getTemplate(
        renddx9abi::toHostString(name));                                        // +0x18
    geometryTemplate->load();                                                   // +0x2C
    geometry = geometryTemplate->createInstance();                              // +0x24
}

// sub_1015EC70 (230 B, read in full): the copy constructor. The initializer list copies the
// pointers, +0x38 and +0x4C (the vector starts empty); the body addRefs the geometry (its slot
// +0x00, NO null test), copies the two count pairs, then pushes and addRefs every vector entry.
OverGrowthGeometryRecord::OverGrowthGeometryRecord(const OverGrowthGeometryRecord& other)
    : name(other.name), geometry(other.geometry), objectTemplate(other.objectTemplate),
      geometryTemplate(other.geometryTemplate), poolUsed(other.poolUsed), pool(),
      distanceScale(other.distanceScale)
{
    geometryAddRef(geometry);
    vertexCounts[0] = other.vertexCounts[0];
    vertexCounts[1] = other.vertexCounts[1];
    indexCounts[0] = other.indexCounts[0];
    indexCounts[1] = other.indexCounts[1];

    for (unsigned int i = 0; i < other.pool.size(); ++i)
    {
        void* const entry = other.pool[i];
        pool.push_back(entry);                                                  // 1009A340 when full
        geometryAddRef(entry);                                                  // slot +0x00
    }
}

// sub_1015EC00 (101 B, read in full): release the geometry (slot +0x08, guarded), release every
// vector entry (slot +0x08, unguarded); the vector and then the name are destroyed as members.
OverGrowthGeometryRecord::~OverGrowthGeometryRecord()
{
    if (geometry)
        geometryRelease(geometry);
    for (unsigned int i = 0; i < pool.size(); ++i)
        geometryRelease(pool[i]);
}

namespace {

// Base cleanup (1015F9B0 / the base destructor 1015FA50): the geometry table (101607F0, each pair
// destroyed +0x50 then +0x00, 1015F9E9..1015FA13) and then the material table (100D28B0, each
// element's destructor releases its type vector and then its name).
void releaseTypeVectors(OverGrowth* self)
{
    self->m_geometries.clear();
    materialTable(self).clear();
}

// the two array frees are unconditional, then each field is cleared in order.
void clearBase(OverGrowth* self)
{
    ::renddx9abi::engineFree(self->m_densityMap);
    void* second = self->m_shadowMap;
    self->m_densityMap = 0;
    ::renddx9abi::engineFree(second);
    self->m_shadowMap = 0;
    if (!self->m_reloading)
        releaseTypeVectors(self);
    self->m_initialized = 0;
}

// sub_1015E5D0 (39 B) - the unpublished base class's setViewDistance (its own table 0x1021EBC0 holds
// it at +0x0C); the derived +0x0C calls it directly. Store the distance, then the cell size:
// distance / (grid / 2 - 1), the halving a signed `cdq; sub; sar 1` on the int at +0x10.
void baseSetViewDistance(OverGrowth* self, float distance)
{
    self->m_viewDistance = distance;
    self->m_cellSize = distance / static_cast<float>(self->m_gridSize / 2 - 1);
}

// sub_100D0550 (161 B, read in full), called with 3 by clear and setViewDistance: the vector
// deleting destructor. Elements are destroyed last TO first (the patch destructor is its vector's),
// then operator delete[] (1005F370) on the cookie. The scalar arm (flags without 2) is not reached
// by any caller.
void deletePatchArray(OverGrowthPatch* patches)
{
    delete[] patches;
}
}

OverGrowth::OverGrowth()
{
    // PE 1015F8F0, preserving untouched padding/fields rather than zeroing the whole object. The
    // three vectors and the path are empty-constructed as members.
    m_cameraPosition.x = 0.0f;
    m_cameraPosition.y = 0.0f;
    m_cameraPosition.z = 0.0f;
    m_gridSize = 10;
    m_cellSize = 0.0f;
    m_randomSeed = 0x759DC150;
    m_enabled = 1;
    m_unresolved1D = 0;
    m_initialized = 0;
    m_fadeBand = 0.3f;
    m_viewDistance = 0.0f;
    m_minimumHeightFade = 0.4f;
    m_cellFadeDistance = 12.0f;
    m_reseatCells = 0;
    m_viewDistanceScale = 1.0f;
    m_heightFadeDistance = 1.0f;
    m_extentX = 0.0f;
    m_extentZ = 0.0f;
    m_patchOrigin.x = 0.0f;
    m_patchOrigin.y = 0.0f;
    m_patchOrigin.z = 0.0f;
    m_mapWidth = 0;
    m_mapHeight = 0;
    m_densityMap = 0;
    m_shadowMap = 0;
    m_gridSize &= ~1;
    m_loaded = 0;
    m_reloading = 0;
    m_viewDistance = 80.0f;

    // PE 100CB940 derived half. The compiler supplies the complete 18-entry primary table.
    m_unresolvedD0 = 0;
    m_patches = 0;
    m_cells = 0;
    m_activeMaterial = 0;
    m_activeType = 0;
    m_editorEnable = 0;
    m_unresolvedF1 = 0;
    m_lodTimer = 0.0f;
    m_atlas = 0;
    m_atlasPage = 0;
    m_closeDistPercentage = 0.15f;
    m_closeDistFade = 0.3f;
    m_drawNear = 1;
    m_updateBeforeRender = 1;
    m_qualityScale = 1.0f;
    m_gridSize = 10;
    m_viewDistance = 500.0f;
}

OverGrowth::~OverGrowth()
{
    // PE 100D2720 -> 100CB6C0 -> 1015FA50. Base cleanup runs again; the path and the emptied
    // tables are then destroyed as members.
    OverGrowth::clear();
    m_unresolvedB4.clear();
    clearBase(this);
    releaseTypeVectors(this);
}
void OverGrowth::operator delete(void* memory) noexcept
{ ::renddx9abi::engineFree(memory); }

OverGrowth* createOverGrowth()
{
    void* memory = ::renddx9abi::engineAlloc(sizeof(OverGrowth));
    return memory ? ::new (memory) OverGrowth() : 0;
}


// OverGrowth::clear - +0x30, PE 100CB6C0 (219 B), read in full. Every arm written:
//   100CB6CC  delete[] the patches (100D0550 with 3), +0xE0 = 0.
//   100CB6D8  each non-null cell of the N x N grid (row z, column x) deleted through its table's
//             slot 0 with 1, then operator delete[] (1005F370) on the grid, +0xE4 = 0.
//   100CB741  the atlas owner: sub_10015930 then operator delete; +0xFC = 0.
//   100CB75B  the adopted page: `if (!--refCount) { sub_10014E20; operator delete }`, +0x100 = 0.
//   100CB78A  the base clear (1015F9B0), then +0xA4 = 0.

void OverGrowth::clear()
{
    if (m_patches)
        deletePatchArray(m_patches);
    m_patches = 0;
    if (m_cells)
    {
        for (int z = 0; z < m_gridSize; ++z)
        {
            for (int x = 0; x < m_gridSize; ++x)
            {
                OverGrowthCell* const cell = m_cells[m_gridSize * z + x];
                if (cell)
                    cell->destroy(1);
            }
        }
        ::renddx9abi::engineFree(m_cells);
        m_cells = 0;
    }
    if (dx9rend::TextureAtlas* const atlas = m_atlas)
    {
        dx9rend::textureAtlasDtor(atlas);
        ::renddx9abi::engineFree(atlas);
    }
    m_atlas = 0;
    if (m_atlasPage)
        dx9rend::textureReleaseRef(m_atlasPage);
    clearBase(this);
    m_loaded = 0;
}


// Replaces the patch grid for a view distance, fitting whole cells to the terrain extent.
// Before terrain dimensions exist, only distance and base cell size are updated.

void OverGrowth::setViewDistance(float distance)
{
    if (m_patches)
        deletePatchArray(m_patches);
    m_patches = 0;
    baseSetViewDistance(this, distance);

    const float extentX = m_extentX;
    const float extentZ = m_extentZ;
    const float extent = (extentX < extentZ) ? extentX : extentZ;
    if (extent == 0.0f)
        return;

    const double cells = std::floor(static_cast<double>(extent / m_cellSize));
    const float cellSize = extent / static_cast<float>(cells);                         // 100CBAA8 fdivr
    m_cellSize = cellSize;                                                             // 100CBAAF fst
    const float inverse = 1.0f / cellSize;                                             // 100CBAB2 fdivr 1.0
    m_patchCountX = static_cast<int>((m_extentX * inverse + 1.0f) * 0.5f) * 2;
    m_patchCountZ = static_cast<int>((m_extentZ * inverse + 1.0f) * 0.5f) * 2;
}

float OverGrowth::getViewDistance() const { return m_viewDistance; }                   // +0x10
OverGrowthCell* OverGrowth::getCell(int x, int z)                                      // +0x20
{
    return m_cells[x + z * m_gridSize];
}


// OverGrowth::reload - +0x2C, PE 100CDDC0 (59 B), read in full: the editor's save-and-regenerate. The
// console's `Overgrowth.generateAll` (executor 100C6649, a +0x2C call) and every OvergrowthType.*
// setter once +0xA4 is set reach it. +0xB0 brackets the whole regeneration so clearBase keeps the
// material/type/geometry tables and postLoad keeps the stream counts.
// save() comes first: a live regeneration rewrites <path>Overgrowth.con before rebuilding.

void OverGrowth::reload()
{
    m_reloading = 1;
    save();
    clear();                                                               // 100CDDD3 virtual +0x30
    preLoad();
    preLoadResources();
    postLoadResources();
    generateAtlas();
    m_reloading = 0;
}


// sub_1015F450 (407 B, retn 4, read in full): the index of `name` in the geometry table, matched on
// either record's name (+0x00 the "_lod" record, then +0x50); -1 for an empty name. A miss builds a
// pair (the "_lod" record first, then the named one) and keeps it only when both got a geometry
// instance (the named record tested first), answering the new last index; else -1.
// Callers: initialize, preLoad, the static-object adoption, and populatePatch (+0x44, 100CC800).

int OverGrowth::findOrAddGeometry(const std::string& name)
{
    if (name.size() == 0)                                                  // [name+0x14] == 0
        return -1;
    std::vector<OverGrowthGeometryPair>& geometries = m_geometries;
    for (unsigned int i = 0; i < geometries.size(); ++i)
    {
        const OverGrowthGeometryPair& pair = geometries[i];
        if (sameString(pair.lod.name, name) ||
            sameString(pair.full.name, name))
            return static_cast<int>(i);
    }

    const std::string full(name);                                        // var_20
    const std::string lod = concat(name, "_lod");                        // var_3C
    const OverGrowthGeometryPair pair(lod, full);                         // var_F8 / var_A8
    int index = -1;
    if (pair.full.geometry && pair.lod.geometry)
    {
        geometries.push_back(pair);
        index = static_cast<int>(geometries.size()) - 1;
    }
    return index;
}


// Default vegetation-type settings. Material selection is case-insensitive; type selection
// is case-sensitive and may retain an active type from another material.
OverGrowthType::OverGrowthType()
    : name(), geometry(), probability(1.0f), density(0.0f), rotationScale(0.0f),
      minRadiusToSame(0.0f), minRadiusToOthers(0.0f), normalScale(1.0f), geometryIndex(-1)
{
    randomScale.x = 1.0f;
    randomScale.y = 1.0f;
}

// addMaterial's temporary (100CDFA0..): the name default-constructed and the type vector empty.
// [CHOSEN, not recovered] the id: retail leaves the temporary's +0x1C unwritten (it is overwritten
// the moment the record lands in the table); it is zeroed here so no copy reads an indeterminate
// value.
OverGrowthMaterial::OverGrowthMaterial()
    : name(), id(0), types()
{
}

// Overgrowth.path

// 100C92C0 (12 B): `add ecx, 88h; jmp std::string::operator=` - the only writer of +0x88 in the
// image (its one caller is the path executor 100C5407). A non-empty +0x88 is what opens
// preLoadResources / postLoadResources / render / updateCells (their +0x9C size tests).
void OverGrowth::setPath(const std::string& path)
{
    m_path = path;
}

// 100C92D0 (7 B): `lea eax, [ecx+88h]; retn`.
const std::string& OverGrowth::getPath() const
{
    return m_path;
}

// the material table

// every material as "<name> [<id>]", backslash-separated, in table order.
std::string OverGrowth::getMaterialList() const
{
    std::string list;
    const std::vector<OverGrowthMaterial>& materials = materialTable(this);
    for (const OverGrowthMaterial* it = materials.data(); it != materials.data() + materials.size(); ++it)
    {
        if (list.size())
            list += "\\";
        list += overGrowthFormatString("%s [%d]", it->name.c_str(), it->id);
    }
    return list;
}

// 100CDEE0 (365 B), read in full. A duplicate ID answers false (the name is not compared). A new
// record is a default-constructed temporary pushed at the end (100D2D90); then ITS id is written,
// then its name, and both cursors are cleared.
bool OverGrowth::addMaterial(const std::string& name, unsigned int id)
{
    std::vector<OverGrowthMaterial>& materials = materialTable(this);
    for (OverGrowthMaterial* it = materials.data(); it != materials.data() + materials.size(); ++it)
    {
        const OverGrowthMaterial material(*it);                            // by value, as shipped
        if (id == material.id)
            return false;
    }

    materials.push_back(OverGrowthMaterial());
    OverGrowthMaterial& added = materials.back();                          // `[this+44h] - 30h`
    added.id = id;                                                         // +0x1C first,
    added.name = name;                                                     // then the name
    activeMaterial(this) = 0;                                              // +0xE8
    activeType(this) = 0;                                                  // +0xEC
    return true;
}

// 100CDE00 (210 B), read in full. Only the active material can be removed. Its types are cleared
// (100CFE80) before the erase moves the tail down over it (100D1A70), destroys the last record
// (100D28B0) and shortens the table; both cursors are then cleared.
bool OverGrowth::removeActiveMaterial()
{
    if (!activeMaterial(this))
        return false;
    std::vector<OverGrowthMaterial>& materials = materialTable(this);
    for (OverGrowthMaterial* it = materials.data(); it != materials.data() + materials.size(); ++it)
    {
        const OverGrowthMaterial material(*it);                            // by value, as shipped
        if (activeMaterial(this) == it)
        {
            activeMaterial(this)->types.clear();
            materials.erase(materials.begin() + (it - materials.data()));
            activeType(this) = 0;
            activeMaterial(this) = 0;
            return true;
        }
    }
    return false;
}

// the first material whose name matches case-INsensitively becomes active. A miss
// changes nothing, and the active type is never touched here.
void OverGrowth::setActiveMaterial(const std::string& name)
{
    std::vector<OverGrowthMaterial>& materials = materialTable(this);
    for (OverGrowthMaterial* it = materials.data(); it != materials.data() + materials.size(); ++it)
    {
        if (equalsNoCase(it->name, name))                                  // 100C9EF5 sub_10075080
        {
            activeMaterial(this) = it;
            return;
        }
    }
}

// the active material's name, or "" (unk_101DC333) without one.
std::string OverGrowth::getActiveMaterialName() const
{
    const OverGrowthMaterial* const material = activeMaterial(this);
    if (material)
        return material->name;
    return std::string("");
}

// the active material's type table

// the active material's type names ("%s"), backslash-separated; each record copied by
// value first; the end re-read through +0xE8 every pass.
std::string OverGrowth::getTypeList() const
{
    std::string list;
    const OverGrowthMaterial* const material = activeMaterial(this);
    if (material)
    {
        for (const OverGrowthType* it = material->types.data();
             it != activeMaterial(this)->types.data() + activeMaterial(this)->types.size(); ++it)
        {
            const OverGrowthType type(*it);                                // by value, as shipped
            if (list.size())
                list += "\\";
            list += overGrowthFormatString("%s", type.name.c_str());
        }
    }
    return list;
}

// 100CC6A0 (343 B), read in full. Needs an active material; a duplicate name answers false. The new
// record is a default-constructed temporary pushed at the end (100D1950); its name AND geometry are
// both set to the given name; then the previously active type is re-selected by its saved name
// (an empty name when there was none, which setActiveType turns into "no active type").
bool OverGrowth::addType(const std::string& name)
{
    if (!activeMaterial(this))
        return false;
    for (OverGrowthType* it = activeMaterial(this)->types.data();
         it != activeMaterial(this)->types.data() + activeMaterial(this)->types.size(); ++it)
    {
        const OverGrowthType type(*it);                                    // by value, as shipped
        if (sameString(type.name, name))
            return false;
    }

    std::string previous;
    if (activeType(this))
        previous = activeType(this)->name;
    activeMaterial(this)->types.push_back(OverGrowthType());
    OverGrowthType& added = activeMaterial(this)->types.back();            // `[+0E8h]+28h - 5Ch`
    added.name = name;
    added.geometry = name;
    setActiveType(previous);
    return true;
}

// 100CBB30 (130 B), read in full. Needs an active material AND an active type; erases the active
// type from the active material (100D06B0) and clears the type cursor only.
bool OverGrowth::removeActiveType()
{
    if (!activeMaterial(this))
        return false;
    if (!activeType(this))
        return false;
    for (OverGrowthType* it = activeMaterial(this)->types.data();
         it != activeMaterial(this)->types.data() + activeMaterial(this)->types.size(); ++it)
    {
        const OverGrowthType type(*it);
        if (activeType(this) == it)
        {
            std::vector<OverGrowthType>& types = activeMaterial(this)->types;
            types.erase(types.begin() + (it - types.data()));
            activeType(this) = 0;
            return true;
        }
    }
    return false;
}

// 100CA050 (215 B), read in full. The first type of the active material whose name matches
// (case-sensitive) becomes active; no active material, or a miss, nulls the active type.
void OverGrowth::setActiveType(const std::string& name)
{
    OverGrowthMaterial* const material = activeMaterial(this);
    if (material)
    {
        for (OverGrowthType* it = material->types.data();
             it != activeMaterial(this)->types.data() + activeMaterial(this)->types.size(); ++it)
        {
            const OverGrowthType type(*it);                                // by value, as shipped
            if (sameString(type.name, name))
            {
                activeType(this) = it;
                return;
            }
        }
    }
    activeType(this) = 0;
}

// the active type's properties (OvergrowthType.*)
// Every setter: no active type -> nothing; else the field, then `if (+0xA4) this->+0x2C()` - the
// live regeneration, armed only once postLoadResources has set +0xA4 (100CD7AA). Every getter
// without an active type answers 0.0f (flt_101DDE64), "" or (1, 1).

void OverGrowth::setTypeGeometry(const std::string& geometry)
{
    OverGrowthType* const type = activeType(this);
    if (!type)
        return;
    type->geometry = geometry;
    if (m_loaded)
        reload();
}
std::string OverGrowth::getTypeGeometry() const
{
    const OverGrowthType* const type = activeType(this);
    if (type)
        return type->geometry;
    return std::string("");
}

void OverGrowth::setTypeDensity(float value)
{
    OverGrowthType* const type = activeType(this);
    if (!type)
        return;
    type->density = value;
    if (m_loaded)
        reload();
}
float OverGrowth::getTypeDensity() const
{
    const OverGrowthType* const type = activeType(this);
    return type ? type->density : 0.0f;
}

void OverGrowth::setTypeNormalScale(float value)
{
    OverGrowthType* const type = activeType(this);
    if (!type)
        return;
    type->normalScale = value;
    if (m_loaded)
        reload();
}
float OverGrowth::getTypeNormalScale() const
{
    const OverGrowthType* const type = activeType(this);
    return type ? type->normalScale : 0.0f;
}

void OverGrowth::setTypeRotationScale(float value)
{
    OverGrowthType* const type = activeType(this);
    if (!type)
        return;
    type->rotationScale = value;
    if (m_loaded)
        reload();
}
float OverGrowth::getTypeRotationScale() const
{
    const OverGrowthType* const type = activeType(this);
    return type ? type->rotationScale : 0.0f;
}

void OverGrowth::setTypeMinRadiusToSame(float value)
{
    OverGrowthType* const type = activeType(this);
    if (!type)
        return;
    type->minRadiusToSame = value;
    if (m_loaded)
        reload();
}
float OverGrowth::getTypeMinRadiusToSame() const
{
    const OverGrowthType* const type = activeType(this);
    return type ? type->minRadiusToSame : 0.0f;
}

void OverGrowth::setTypeMinRadiusToOthers(float value)
{
    OverGrowthType* const type = activeType(this);
    if (!type)
        return;
    type->minRadiusToOthers = value;
    if (m_loaded)
        reload();
}
float OverGrowth::getTypeMinRadiusToOthers() const
{
    const OverGrowthType* const type = activeType(this);
    return type ? type->minRadiusToOthers : 0.0f;
}

void OverGrowth::setTypeRandomScale(Vec2 value)                            // 100C9610, `retn 8`
{
    OverGrowthType* const type = activeType(this);
    if (!type)
        return;
    type->randomScale.x = value.x;                                         // +0x44
    type->randomScale.y = value.y;                                         // +0x48
    if (m_loaded)
        reload();
}
Vec2 OverGrowth::getTypeRandomScale() const
{
    const OverGrowthType* const type = activeType(this);
    if (type)
        return type->randomScale;
    Vec2 unit;
    unit.x = 1.0f;                                                         // 3F800000h
    unit.y = 1.0f;
    return unit;
}

// plain fields and small accessors

float OverGrowth::getViewDistanceScale() const { return m_viewDistanceScale; }         // exec 100C60A7
void  OverGrowth::setViewDistanceScale(float scale) { m_viewDistanceScale = scale; }
float OverGrowth::getCloseDistPercentage() const { return m_closeDistPercentage; }     // exec 100C680A
void  OverGrowth::setCloseDistPercentage(float value) { m_closeDistPercentage = value; }
float OverGrowth::getCloseDistFade() const { return m_closeDistFade; }                 // exec 100C690E
void  OverGrowth::setCloseDistFade(float value) { m_closeDistFade = value; }
float OverGrowth::getLodTimer() const { return m_lodTimer; }                           // exec 100C6A12
void  OverGrowth::setLodTimer(float value) { m_lodTimer = value; }
float OverGrowth::getQualityScale() const { return m_qualityScale; }
void  OverGrowth::setQualityScale(float scale) { m_qualityScale = scale; }

// 100C8C40 (13 B) / the editor byte +0xF0. When set, preLoadResources skips the
// isOvergrowth-object adoption (100CC683 -> 100CBC80), the atlas texture collection 100CC1F0 adds
// those objects' "_lod" templates (100CC282), and rebuildCells branches on it (100CAF2A).
void OverGrowth::setEditorEnable(bool enable) { m_editorEnable = enable; }
bool OverGrowth::getEditorEnable() const { return m_editorEnable != 0; }

// 100C92E0 (16 B): +0xF4 (VideoOptions' scale) * +0xC8 (closeDistPercentage) * +0x24 (viewDistance),
// multiplied left to right.
float OverGrowth::getLodDistance() const
{
    return m_qualityScale * m_closeDistPercentage * m_viewDistance;
}

// 100CDC20 (407 B), read in full: a std::set<std::string> of every NON-empty type geometry name
// over every material (index loops), then the set's names in order, backslash-separated.
std::string OverGrowth::getGeometryList() const
{
    std::set<std::string> names;                                           // 100CDC2A sub_100CE890
    const std::vector<OverGrowthMaterial>& materials = materialTable(this);
    for (unsigned int m = 0; m < materials.size(); ++m)
    {
        const OverGrowthMaterial& material = materials[m];
        for (unsigned int t = 0; t < material.types.size(); ++t)
        {
            const OverGrowthType& type = material.types[t];
            if (!type.geometry.empty())                                    // type +0x30, _Mysize
                names.insert(std::string(type.geometry.c_str(), type.geometry.size()));
        }
    }
    std::string list;
    for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it)
    {
        if (list.size())
            list += "\\";
        list += it->c_str();
    }
    return list;
}


// Editor painting requires initialized maps and an editable terrain singleton.
// Disc bounds are half-open. Export uses patch-width rows and stops at the first out-of-range patch.

namespace {

// dword_1021AE18 - ITerrain, the id the "Terrain" singleton is asked for.
const unsigned int kIID_ITerrain = 820004u;

// dword_10239050 - the material overlay's colours for map values 0..15, A8R8G8B8 from the image bytes
// (the same sixteen values as QuadPatchEditable.cpp's dword_1023AB28, a separate table in the image).
const unsigned int kMaterialOverlayPalette[16] =
{
    0xFF000000u, 0xFF0000FFu, 0xFF00FFFFu, 0xFF00FF00u, 0xFFFFFF00u, 0xFFFF0000u, 0xFFFF00FFu, 0xFFFFFFFFu,
    0xFF000077u, 0xFF007777u, 0xFF007700u, 0xFF777700u, 0xFF770000u, 0xFF770077u, 0xFF777777u, 0xFF333333u,
};

// dword_10258E4C / dword_10258E50 - zero-initialised (.bss) ints the material / shadow exporter adds to
// every map index; nothing in the image writes either (the 2006 build carries the same pair,
// 0x105A6580 / 0x105A6584). [UNRESOLVED] purpose and names.
int g_materialOverlayIndexBias = 0;
int g_shadowOverlayIndexBias = 0;

// ClassManager +0x48 getSingleton("HeightmapCluster") - typed in both flavours (HfeAbi.h). Every body
// below asks by name; none uses the cluster initialize cached at +0xAC.
world::IHeightmapCluster* heightmapClusterByName()
{
    return static_cast<world::IHeightmapCluster*>(static_cast<void*>(
        ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("HeightmapCluster"))));
}

// ClassManager +0x4C getSingletonAs("Terrain", ITerrain) (ELF +0x50 _ZN...14getSingletonAsERKSsj -
// `void* getSingletonAs(const std::string&, unsigned int)`). RAW in both flavours: HfeAbi.h's
// IClassManager, which both flavours dispatch through with the HostString carrier, still declares the
// slot reserved_getSingletonAs(). [CHOSEN, not recovered] - the third copy of the RoadEditable.cpp /
// UndergrowthSystemLoad.cpp seam; all three retire by typing the slot in HfeAbi.h.
Terrain* terrainSingleton()
{
    typedef void* (__thiscall* GetSingletonAsFn)(void*, const renddx9abi::HostString&, unsigned int);
    void* const manager = ::dice::hfe::classManager;
    return static_cast<Terrain*>(static_cast<IBase*>(reinterpret_cast<GetSingletonAsFn>(
        (*reinterpret_cast<void***>(manager))[0x4C / 4])(manager, renddx9abi::HostString("Terrain"),
                                                         kIID_ITerrain)));
}

// The painters' distance terms, `fild a; fisub b` / `fild a; fsub [float b]` under RendDX9's 24-bit
// precision control: the exact difference, rounded once to single precision.
float differenceAsFloat(double a, double b)
{
    return static_cast<float>(a - b);
}

} // namespace

// 100CA130 (`retn 10h`) - Overgrowth.paintMaterial. A non-zero material must be the id (+0x1C) of a
// material in the table (the 2006 build's `missing` flag) or nothing happens; 0 erases. The rectangle
// goes to the overlay exporter only while the terrain shows technique 5.
void OverGrowth::paintMaterial(float x, float z, float radius, unsigned char material)
{
    if (material != 0)                                                                   // `cmp byte ptr`
    {
        const std::vector<OverGrowthMaterial>& materials = materialTable(this);
        bool missing = true;
        for (unsigned int i = 0; i < materials.size(); ++i)
        {
            if (materials[i].id == material)                                             // movzx
            {
                missing = false;
                break;
            }
        }
        if (missing)
            return;
    }

    world::IHeightmap* const heightmap = heightmapClusterByName()->getPrimaryHeightmap();   // +0x40
    int sizeX = 0;
    int sizeZ = 0;
    heightmap->getSize(sizeX, sizeZ);                                                         // +0x2C
    const float gridX = x / heightmap->getScale().x + static_cast<float>(sizeX / 2);         // +0x54, three
    const float gridZ = z / heightmap->getScale().z + static_cast<float>(sizeZ / 2);         // calls, in
    const float gridRadius = radius / heightmap->getScale().x;                                // this order
    const int centreX = static_cast<int>(gridX);          // fistp under a truncating control word
    const int centreZ = static_cast<int>(gridZ);
    const int reach = static_cast<int>(gridRadius);       // __ftol2
    const int x0 = centreX - reach <= 0 ? 0 : centreX - reach;
    const int x1 = sizeX < centreX + reach ? sizeX : centreX + reach;
    const int z0 = centreZ - reach <= 0 ? 0 : centreZ - reach;
    const int z1 = sizeZ < centreZ + reach ? sizeZ : centreZ + reach;
    for (int ix = x0; ix < x1; ++ix)
    {
        const float dx = differenceAsFloat(ix, centreX);
        const float dx2 = dx * dx;
        const float centreZf = static_cast<float>(centreZ);
        for (int iz = z0; iz < z1; ++iz)
        {
            const float dz = differenceAsFloat(iz, centreZf);
            if (!(std::sqrt(dz * dz + dx2) > gridRadius))                                 // test ah, 41h
                m_densityMap[iz * sizeX + ix] = material;
        }
    }

    ITerrainEditable* const editable =
        static_cast<ITerrainEditable*>(terrainSingleton()->queryInterface(kIID_ITerrainEditable));
    if (editable->getShaderTechnique() == 5)                                                  // E+0x8C
        exportMaterialRectToTerrainTexture(static_cast<unsigned int>(x0), static_cast<unsigned int>(x1),
                                           static_cast<unsigned int>(z0), static_cast<unsigned int>(z1));
}

// 100C9670 (`retn 10h`) - Overgrowth.paintShadowmap: paintMaterial's disc over the +0x84 shadow map,
// with no check on the value (the 2006 build logs every call first; 1.50 does not); technique 6.
void OverGrowth::paintShadowmap(float x, float z, float radius, unsigned char value)
{
    world::IHeightmap* const heightmap = heightmapClusterByName()->getPrimaryHeightmap();   // +0x40
    int sizeX = 0;
    int sizeZ = 0;
    heightmap->getSize(sizeX, sizeZ);                                                         // +0x2C
    const float gridX = x / heightmap->getScale().x + static_cast<float>(sizeX / 2);
    const float gridZ = z / heightmap->getScale().z + static_cast<float>(sizeZ / 2);
    const float gridRadius = radius / heightmap->getScale().x;
    const int centreX = static_cast<int>(gridX);
    const int centreZ = static_cast<int>(gridZ);
    const int reach = static_cast<int>(gridRadius);
    const int x0 = centreX - reach <= 0 ? 0 : centreX - reach;
    const int x1 = sizeX < centreX + reach ? sizeX : centreX + reach;
    const int z0 = centreZ - reach <= 0 ? 0 : centreZ - reach;
    const int z1 = sizeZ < centreZ + reach ? sizeZ : centreZ + reach;
    for (int ix = x0; ix < x1; ++ix)
    {
        const float dx = differenceAsFloat(ix, centreX);
        const float dx2 = dx * dx;
        const float centreZf = static_cast<float>(centreZ);
        for (int iz = z0; iz < z1; ++iz)
        {
            const float dz = differenceAsFloat(iz, centreZf);
            if (!(std::sqrt(dz * dz + dx2) > gridRadius))
                m_shadowMap[iz * sizeX + ix] = value;
        }
    }

    ITerrainEditable* const editable =
        static_cast<ITerrainEditable*>(terrainSingleton()->queryInterface(kIID_ITerrainEditable));
    if (editable->getShaderTechnique() == 6)
        exportShadowmapRectToTerrainTexture(static_cast<unsigned int>(x0), static_cast<unsigned int>(x1),
                                            static_cast<unsigned int>(z0), static_cast<unsigned int>(z1));
}

// 100C8F40 (`retn`) - Overgrowth.exportToTerrainTexture, also TerrainEditable's technique-5 switch:
// the whole material map, texels 0 .. size - 2 per axis.
void OverGrowth::exportToTerrainTexture()
{
    int sizeX = 0;
    int sizeZ = 0;
    heightmapClusterByName()->getPrimaryHeightmap()->getSize(sizeX, sizeZ);
    exportMaterialRectToTerrainTexture(0, static_cast<unsigned int>(sizeX - 2),
                                       0, static_cast<unsigned int>(sizeZ - 2));
}

// 100C9250 (`retn`) - the shadow-map twin, reached only from TerrainEditable's technique-6 switch.
void OverGrowth::exportShadowmapToTerrainTexture()
{
    int sizeX = 0;
    int sizeZ = 0;
    heightmapClusterByName()->getPrimaryHeightmap()->getSize(sizeX, sizeZ);
    exportShadowmapRectToTerrainTexture(0, static_cast<unsigned int>(sizeX - 2),
                                        0, static_cast<unsigned int>(sizeZ - 2));
}

// 100C8C90 (`retn 10h`) - the material map into the technique-5 overlay (E+0xEC) of every patch the
// texel rectangle touches, patch columns outer: map value v <= 15 takes the palette, anything else
// v * 0x010101 (a grey with alpha 0). The order of the host calls is the shipped one.
void OverGrowth::exportMaterialRectToTerrainTexture(unsigned int x0, unsigned int x1,
                                                    unsigned int z0, unsigned int z1)
{
    Terrain* const terrain = terrainSingleton();
    ITerrainEditable* const editable =
        static_cast<ITerrainEditable*>(terrain->queryInterface(kIID_ITerrainEditable));
    if (editable->getShaderTechnique() != 5)                                                  // E+0x8C
        return;
    if (!m_densityMap)
    {
        editable->setShaderTechnique(0);                                                      // E+0x88
        return;
    }
    int sizeX = 0;
    int sizeZ = 0;
    heightmapClusterByName()->getPrimaryHeightmap()->getSize(sizeX, sizeZ);
    const unsigned int patchSize = static_cast<unsigned int>(terrain->getPatchSize());       // P+0x34
    const unsigned int patchCount = static_cast<unsigned int>(terrain->getPatchCount());     // P+0x30
    const unsigned int lastPatchX = x1 / patchSize;                                           // `div`
    const unsigned int firstPatchZ = z0 / patchSize;
    const unsigned int lastPatchZ = z1 / patchSize;
    for (unsigned int patchX = x0 / patchSize; patchX <= lastPatchX; ++patchX)
    {
        const unsigned int texelX = patchX * patchSize;
        unsigned int patchIndex = firstPatchZ * patchCount + patchX;
        for (unsigned int patchZ = firstPatchZ; patchZ <= lastPatchZ; ++patchZ, patchIndex += patchCount)
        {
            if (patchIndex >= patchCount * patchCount)
                return;
            const unsigned int texelZ = patchZ * patchSize;
            IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(
                static_cast<dx9rend::EngineTexture*>(editable->getPatchOvergrowthOverlay(
                    static_cast<int>(patchIndex))), 0, 0x7FFFFFFFu);                          // E+0xEC
            RECT rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = static_cast<LONG>(patchSize);
            rect.bottom = static_cast<LONG>(patchSize);
            D3DLOCKED_RECT locked;
            surface->LockRect(&locked, &rect, 0);                                             // HRESULT unread
            unsigned int* column = static_cast<unsigned int*>(locked.pBits);
            for (unsigned int j = 0; j < patchSize; ++j, ++column)
            {
                unsigned int* texel = column;
                for (unsigned int i = 0; i < patchSize; ++i, texel += patchSize)
                {
                    const unsigned int value = m_densityMap[
                        (texelZ + i) * static_cast<unsigned int>(sizeX) + texelX + j
                        + static_cast<unsigned int>(g_materialOverlayIndexBias)];
                    *texel = value <= 15 ? kMaterialOverlayPalette[value] : value * 0x010101u;
                }
            }
            surface->UnlockRect();
            surface->Release();
        }
    }
}

// 100C8FB0 (`retn 10h`) - the shadow map into the technique-6 overlay (E+0xF0), every value as the grey
// v * 0x010101 (alpha 0); otherwise 100C8C90 line for line.
void OverGrowth::exportShadowmapRectToTerrainTexture(unsigned int x0, unsigned int x1,
                                                     unsigned int z0, unsigned int z1)
{
    Terrain* const terrain = terrainSingleton();
    ITerrainEditable* const editable =
        static_cast<ITerrainEditable*>(terrain->queryInterface(kIID_ITerrainEditable));
    if (editable->getShaderTechnique() != 6)
        return;
    if (!m_shadowMap)
    {
        editable->setShaderTechnique(0);
        return;
    }
    int sizeX = 0;
    int sizeZ = 0;
    heightmapClusterByName()->getPrimaryHeightmap()->getSize(sizeX, sizeZ);
    const unsigned int patchSize = static_cast<unsigned int>(terrain->getPatchSize());
    const unsigned int patchCount = static_cast<unsigned int>(terrain->getPatchCount());
    const unsigned int lastPatchX = x1 / patchSize;
    const unsigned int firstPatchZ = z0 / patchSize;
    const unsigned int lastPatchZ = z1 / patchSize;
    for (unsigned int patchX = x0 / patchSize; patchX <= lastPatchX; ++patchX)
    {
        const unsigned int texelX = patchX * patchSize;
        unsigned int patchIndex = firstPatchZ * patchCount + patchX;
        for (unsigned int patchZ = firstPatchZ; patchZ <= lastPatchZ; ++patchZ, patchIndex += patchCount)
        {
            if (patchIndex >= patchCount * patchCount)
                return;
            const unsigned int texelZ = patchZ * patchSize;
            IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(
                static_cast<dx9rend::EngineTexture*>(editable->getPatchOvergrowthShadowOverlay(
                    static_cast<int>(patchIndex))), 0, 0x7FFFFFFFu);                          // E+0xF0
            RECT rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = static_cast<LONG>(patchSize);
            rect.bottom = static_cast<LONG>(patchSize);
            D3DLOCKED_RECT locked;
            surface->LockRect(&locked, &rect, 0);
            unsigned int* column = static_cast<unsigned int*>(locked.pBits);
            for (unsigned int j = 0; j < patchSize; ++j, ++column)
            {
                unsigned int* texel = column;
                for (unsigned int i = 0; i < patchSize; ++i, texel += patchSize)
                {
                    const unsigned int value = m_shadowMap[
                        (texelZ + i) * static_cast<unsigned int>(sizeX) + texelX + j
                        + static_cast<unsigned int>(g_shadowOverlayIndexBias)];
                    *texel = value * 0x010101u;
                }
            }
            surface->UnlockRect();
            surface->Release();
        }
    }
}

} } }

namespace dice { namespace hfe { namespace dx9rend {
extern void* g_OverGrowth;
// ResourceManager's direct calls have no null guard in the PE. The owner exists from DLL load
// (BF2Render.cpp's g_OverGrowth, the 0x101D22E0 initialiser) and again after every CEPreLoad.
void overGrowthPreLoadResources() { static_cast<geom::OverGrowth*>(g_OverGrowth)->preLoadResources(); }
void overGrowthPostLoadResources() { static_cast<geom::OverGrowth*>(g_OverGrowth)->postLoadResources(); }
} } }
#endif
