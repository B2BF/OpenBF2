// Owns physics materials and their interaction cells; publishes the material-manager interface.
// Source placement is [INFERRED].

#include "MaterialManager.h"
#include "MMCell.h"                 // removeAll destroys cells - killMMCell lives there
#include "Material.h"               // the per-material property getters
#include "../Core/StringUtil.h"
#include "../IO/Stream/IStream.h"
#include "../Game/Objects/IObjectTemplate.h"
#include <cstdio>
#include <cstdlib>
#if defined(_MSC_VER)
#include <intrin.h>                 // __debugbreak, retail's int 3 for an id >= 10000
#endif
#include <sstream>

namespace dice {
namespace hfe {
namespace world {

// The IBase quartet - 0x8110DB0 / 0x8110DC0 / 0x8110DD0 / 0x8110E10. all , and
// release saves the decremented count across virtual deletion: ELF 0x8110DD0 keeps
// it in EBX (return at 0x8110E00), PE 0x5EE000 in ESI (return at 0x5EE016).
// Neither reads the object after deletion. The ELF additionally null-checks `this`
// before dispatch, a check that cannot fire through a valid virtual call.
void MaterialManager::addRef()
{
    ++m_refCount;
}

int MaterialManager::getRef()
{
    return m_refCount;
}

int MaterialManager::release()
{
    const int remaining = --m_refCount;

    if (remaining == 0)
        delete this;

    return remaining;
}

void* MaterialManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<MaterialManager*>(this);

    // 0xC4AE. no named symbol exists for this id - it is a bare literal in the binary, both here
    // and in world::setMaterialManager, which is why Core/Ids.h does not carry it. Its CID sibling
    // is 0xC4AF (50351), the value BF2EngineSetup.cpp passes and the one the ClassRegInfo row in
    // Core/ModuleRegistry.cpp registers.
    if (iid == 0xC4AE)
        return const_cast<MaterialManager*>(this);

    return 0;
}

// Starts with no selected attacker/defender material (sentinel 10001) and a 10000-bucket cell table.
// Zeroing m_unknown10 is [CHOSEN]; the original leaves it uninitialized.
MaterialManager::MaterialManager()
    : m_refCount(1)
    , m_unknown10(0)            // [CHOSEN] - the original leaves it uninitialised
    , m_attMaterial(10001)
    , m_defMaterial(10001)
    , m_speedDamage(0.1f)
    , m_defaultDamage(1.0f)
    , m_cells(10000)            // not the 3011 default - see above
{
    // m_materials is empty-constructed, which is what the binary's allocate(24) self-linked header
    // amounts to.
}

// ELF D1 0x810E6A0 / dedicated PE 0x5ED040: remove owned materials/cells, then the
// compiler destroys the Hash and map members. The calls in the two bodies are direct.
MaterialManager::~MaterialManager()
{
    MaterialManager::removeAll();
}

// ELF 0x810E8D0 / PE 0x5ECB80. Allocation precedes the ID search. Retail leaks this
// allocation when all 255 IDs are occupied; it neither selects the result nor deduplicates names.
Material* MaterialManager::createMaterial(const std::string& name)
{
    Material* material = new Material();
    unsigned int id = 0;
    while (m_materials.find(id) != m_materials.end())
    {
        if (++id > 254u)
            return 0;
    }
    m_materials.insert(std::make_pair(id, material));
    material->setName(name);
    return material;
}

// ELF 0x810E990 / PE 0x5EC780. Delete before erasing the map node; selected IDs and
// cells referring to the ID remain unchanged. [PLATFORM] ELF aborts, PE int3s for id >= 10000.
void MaterialManager::removeMaterial(unsigned int id)
{
    if (!(id < 10000u))
    {
        // Retail's own guard, not a frontier: BF2.exe `int 3`, the bf2 ELF `call abort`.
#if defined(_MSC_VER)
        __debugbreak();
#else
        std::abort();
#endif
    }
    std::map<unsigned int, Material*>::iterator it = m_materials.find(id);
    if (it != m_materials.end())
    {
        delete it->second;
        m_materials.erase(it);
    }
}

// ELF 0x810F140 / PE 0x5EBF80. Ordered walk; ID zero is also the missing-name result.
unsigned int MaterialManager::getMaterialId(const std::string& name)
{
    for (std::map<unsigned int, Material*>::iterator it = m_materials.begin(); it != m_materials.end(); ++it)
        if (compareIgnoreCase(it->second->getName(), name) == 0)
            return it->first;
    return 0;
}

// ELF 0x8110230 / PE 0x5EC520. Cache each candidate before its virtual name query,
// exactly as retail; duplicate names return the candidate with the lowest material ID.
Material* MaterialManager::getMaterialByName(const std::string& name)
{
    for (std::map<unsigned int, Material*>::iterator it = m_materials.begin(); it != m_materials.end(); ++it)
    {
        Material* material = it->second;
        if (compareIgnoreCase(material->getName(), name) == 0)
            return material;
    }
    return 0;
}

// ELF 0x810F260 / PE 0x5EC030. Repeated virtual getters are intentional: the guard
// and the emitted value are separate calls. The ID is stored unsigned but inserted signed.
bool MaterialManager::saveMaterials(io::IStream* stream)
{
    std::ostringstream out;
    for (std::map<unsigned int, Material*>::iterator it = m_materials.begin(); it != m_materials.end(); ++it)
    {
        Material* material = it->second;
        if (material->getName().empty())
            continue;
        out << "Material.active " << static_cast<int>(it->first) << std::endl;
        out << "Material.name \"" << material->getName() << '"' << std::endl;
        out << "Material.type " << material->getType() << std::endl;
        out << "Material.friction " << material->getFriction() << std::endl;
        out << "Material.elasticity " << material->getElasticity() << std::endl;
        out << "Material.resistance " << material->getResistance() << std::endl;
        if (material->getProjectileCollisionHardness() != 0.0f)
            out << "Material.projectileCollisionHardness " << material->getProjectileCollisionHardness() << std::endl;
        if (material->getDamageLoss() != -1.0f)
        {
            out << "Material.damageLoss " << material->getDamageLoss() << std::endl;
            if (material->getMinDamageLoss() != 0.0f)
                out << "Material.minDamageLoss " << material->getMinDamageLoss() << std::endl;
            if (material->getMaxDamageLoss() != 100000.0f)
                out << "Material.maxDamageLoss " << material->getMaxDamageLoss() << std::endl;
        }
        if (material->getPenetrationDeviation() != 0.1f)
            out << "Material.penetrationDeviation " << material->getPenetrationDeviation() << std::endl;
        if (material->getOverrideNeverPenetrate()) out << "Material.overrideNeverPenetrate 1" << std::endl;
        if (material->getHasWaterPhysics()) out << "Material.hasWaterPhysics " << material->getHasWaterPhysics() << std::endl;
        if (material->getIsBarbwire()) out << "Material.isBarbwire " << material->getIsBarbwire() << std::endl;
        if (material->getIsOneSided()) out << "Material.isOneSided " << material->getIsOneSided() << std::endl;
        if (material->getIsSeeThrough()) out << "Material.isSeeThrough " << material->getIsSeeThrough() << std::endl;
        out << std::endl;
    }
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

// ELF 0x810FB90 / PE 0x5ECC10. Iterate the material-map Cartesian product in key
// order, re-querying getMaterialMap at each loop test. Cells are concrete MMCells;
// retail reads their authoring maps/damage field directly instead of calling getters.
bool MaterialManager::saveCells(io::IStream* stream)
{
    std::ostringstream out;
    for (std::map<unsigned int, Material*>::const_iterator a = getMaterialMap().begin(); a != getMaterialMap().end(); ++a)
    {
        for (std::map<unsigned int, Material*>::const_iterator b = getMaterialMap().begin(); b != getMaterialMap().end(); ++b)
        {
            MMCell* cell = static_cast<MMCell*>(getCell(a->first, b->first));
            if (!cell)
                continue;
            out << "MaterialManager.createCell " << static_cast<int>(a->first) << ' ' << static_cast<int>(b->first) << std::endl;
            if (cell->m_damageMod >= 0.0f) out << "MaterialManager.damageMod " << cell->m_damageMod << std::endl;
            for (const auto& effect : cell->m_effectTemplates)
                if (effect.second) out << "MaterialManager.setEffectTemplate " << effect.first << ' ' << effect.second->getName() << std::endl;
            for (const auto& decal : cell->m_decalTemplates)
                if (decal.second) out << "MaterialManager.setDecalTemplate " << decal.first << ' ' << decal.second->getName() << std::endl;
            for (const auto& sound : cell->m_soundTemplates)
                if (sound.second.soundTemplate) out << "MaterialManager.setSoundTemplate " << sound.first << ' ' << sound.second.soundTemplate->getName() << std::endl;
        }
    }
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

// The field-read accessors, all recovered
unsigned int MaterialManager::getAttMaterial() const   { return m_attMaterial; }
unsigned int MaterialManager::getDefMaterial() const   { return m_defMaterial; }
float        MaterialManager::getSpeedDamage() const   { return m_speedDamage; }
float        MaterialManager::getDefaultDamage() const { return m_defaultDamage; }

void MaterialManager::setSpeedDamage(float v)   { m_speedDamage = v; }
void MaterialManager::setDefaultDamage(float v) { m_defaultDamage = v; }

// 11 bytes, `add eax, 8` - the map itself, not a copy.
const std::map<unsigned int, Material*>& MaterialManager::getMaterialMap()
{
    return m_materials;
}

// 28 bytes. A convenience overload: it loads m_attMaterial and tail-calls the id-taking form
// through the vtable (`call dword ptr [ecx+24h]`), so an override of slot +36 changes this too.
Material* MaterialManager::getMaterialPtr()
{
    return getMaterialPtr(m_attMaterial);
}

// Returns the requested material or retries material zero through virtual dispatch.
// A missing default material returns null.
Material* MaterialManager::getMaterialPtr(unsigned int id)
{
    std::map<unsigned int, Material*>::iterator it = m_materials.find(id);
    if (it != m_materials.end())
        return it->second;
    if (id)
        return getMaterialPtr(0u);
    return 0;
}

// Returns the first material with the requested id by walking the map, or null.
// Does not substitute material zero on a miss.
Material* MaterialManager::getMaterialById(unsigned int id)
{
    for (std::map<unsigned int, Material*>::iterator it = m_materials.begin();
         it != m_materials.end(); ++it)
    {
        if (it->first == id)
            return it->second;
    }
    return 0;
}

// MaterialManager::create - , and identical in shape to
// SpawnManager::create: aggregation refused outright (a non-null outer returns 0 before allocating),
// then new / queryInterface / release-on-failure. `operator new(0x30u)` is what fixes sizeof at 48.
IBase* MaterialManager::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    MaterialManager* self = new MaterialManager();       // operator new(0x30u)

    void* itf = self->queryInterface(iid);               // virtual, vtable +0x0C
    if (itf == 0)
        self->release();                                 // virtual, vtable +0x08

    return static_cast<IBase*>(itf);
}

// killMMCell - See MMCell.h for why it exists at all.
void killMMCell(const IMMCell* cell)
{
    if (cell)
        delete cell;                                     // 0x810EA72 - `call [eax+4]`, the D0 slot
}

// Deletes all materials, then material-pair cells, and clears their containers.
// Materials use virtual deletion, not IBase::release. Values remain in their containers until each
// deletion pass completes, so concurrent lookups are invalid during teardown.
// The original map stores IMaterial*; the current concrete-pointer API retains the same ownership.
void MaterialManager::removeAll()
{
    for (std::map<unsigned int, Material*>::iterator i = m_materials.begin();
         i != m_materials.end(); ++i)
    {
        if (i->second)                                   // the binary's null guard
            delete i->second;                            // vtable +0x04, the D0 slot
    }
    m_materials.clear();                                 // 0x810EB33 - _M_erase + header reset

    typedef Hash<unsigned int, IMMCell*, UIntHashFunc> CellTable;
    for (unsigned int bucket = 0; bucket < m_cells.m_maxSize; ++bucket)
    {
        for (CellTable::Node* n = m_cells.m_buckets[bucket]; n != 0; n = n->next)
            killMMCell(n->value);                        // node->value is at +0x08
    }
    m_cells.clear();                                     // frees the nodes
}

// Computes the ordered material-pair key as a * 10000 + b.
unsigned int getMaterialHashKey(unsigned int a, unsigned int b)
{
    return a * 10000u + b;
}

// Material IDs must be below 10000; setters break on invalid IDs rather than clamp.
// [PLATFORM] MSVC reverses adjacent overloaded virtual pairs relative to ELF; keep declaration order.

// 0x5ECB10: `Material.active N`. The material is created if the id is new (a default-constructed
// Material inserted under the id - and a failed allocation returns without selecting it), then
// selected as the attacking material of the current pair.
void MaterialManager::setAttMaterial(unsigned int id)
{
    if (!(id < 10000u))
    {
        // Retail's own guard, not a frontier: BF2.exe `int 3`, the bf2 ELF `call abort`.
#if defined(_MSC_VER)
        __debugbreak();
#else
        std::abort();
#endif
    }
    if (m_materials.find(id) == m_materials.end())
    {
        Material* m = new Material();
        if (!m)
            return;
        m_materials.insert(std::make_pair(id, m));
    }
    m_attMaterial = id;
}

// Material-pair selection and lazy cell creation. Invalid IDs hit retail's own trap.
// Preserve defending-before-attacking stores, the post-allocation bucket lookup and virtual overload dispatch.
void MaterialManager::setDefMaterial(unsigned int id)
{
    if (!(id < 10000u))
    {
        // Retail's own guard, not a frontier: BF2.exe `int 3`, the bf2 ELF `call abort`.
#if defined(_MSC_VER)
        __debugbreak();
#else
        std::abort();
#endif
    }
    m_defMaterial = id;
}

// 0x5ED2B0: `MaterialManager.createCell a b`. Both ids range-tested (null above 9999), the pair
// becomes the current one (+0x14 / +0x18 - which is how the `damageMod` / `set*Template` lines that
// follow it in materialManagerSettings.con find their cell through the no-argument forms), then
// the cell is looked up by world::getMaterialHashKey and created - `new MMCell`, inserted - when
// missing. Returns the cell either way.
IMMCell* MaterialManager::getCreateCell(unsigned int a, unsigned int b)
{
    if (a >= 10000u || b >= 10000u)
        return 0;
    const unsigned int key = getMaterialHashKey(a, b);
    m_defMaterial = b;
    m_attMaterial = a;
    IMMCell* cell;
    if (!m_cells.get(key, cell))
    {
        cell = new MMCell();
        m_cells.insert(key, cell);
    }
    return cell;
}

// the current pair, through the vtable, after the same two range tests.
IMMCell* MaterialManager::getCreateCell()
{
    const unsigned int a = m_attMaterial;
    if (a < 10000u && m_defMaterial < 10000u)
        return getCreateCell(a, m_defMaterial);
    return 0;
}

IMMCell* MaterialManager::getCell(unsigned int a, unsigned int b)
{
    if (a > 9999u || b > 9999u)
        return 0;
    const unsigned int key = getMaterialHashKey(a, b);
    IMMCell* cell;
    if (m_cells.get(key, cell))
        return cell;
    return 0;
}

// The pair currently selected by the console (m_attMaterial, m_defMaterial),
// through the vtable (`call [eax+50h]`) after the same two range tests.
IMMCell* MaterialManager::getCell()
{
    const unsigned int a = m_attMaterial;
    if (a <= 9999u && m_defMaterial <= 9999u)
        return getCell(a, m_defMaterial);
    return 0;
}

// each - the same shape three times: the material by id
// (getMaterialPtr, which itself falls back to id 0 for an unknown non-zero id), then material 0
// explicitly, then 1.0 when even that is missing. Both lookups go through the vtable (+0x24).
float MaterialManager::getFrictionForMaterial(unsigned int id)
{
    Material* m = getMaterialPtr(id);
    if (!m)
    {
        m = getMaterialPtr(0u);
        if (!m)
            return 1.0f;
    }
    return m->getFriction();                                    // Material slot 7
}

float MaterialManager::getElasticityForMaterial(unsigned int id)
{
    Material* m = getMaterialPtr(id);
    if (!m)
    {
        m = getMaterialPtr(0u);
        if (!m)
            return 1.0f;
    }
    return m->getElasticity();                                  // slot 9
}

float MaterialManager::getResistanceForMaterial(unsigned int id)
{
    Material* m = getMaterialPtr(id);
    if (!m)
    {
        m = getMaterialPtr(0u);
        if (!m)
            return 1.0f;
    }
    return m->getResistance();                                  // slot 11
}

// The (a, b) cell's damage modifier unless the cell is missing or answers
// -1; then the (a, 0) cell's; then m_defaultDamage.
float MaterialManager::getDamageMod(unsigned int a, unsigned int b)
{
    IMMCell* cell = getCell(a, b);                               // vtable +0x50
    float mod;
    if (!cell || (mod = cell->getDamageMod()) == -1.0f)
    {
        cell = getCell(a, 0u);
        if (!cell)
            return m_defaultDamage;
        mod = cell->getDamageMod();
        if (mod == -1.0f)
            return m_defaultDamage;
    }
    return mod;
}

// The (a, b) cell's effect template at `index` (the cell is asked with `true`
// as its second argument), else the (a, 0) cell's, else null.
void* MaterialManager::getEffectTemplate(unsigned int a, unsigned int b, int index)
{
    IMMCell* cell = getCell(a, b);
    void* result;
    if (!cell || (result = cell->getEffectTemplate(index, true)) == 0)   // IMMCell slot 6
    {
        cell = getCell(a, 0u);
        if (!cell)
            return 0;
        result = cell->getEffectTemplate(index, true);
        if (!result)
            return 0;
    }
    return result;
}

// ELF 0x810EE00 / PE 0x5EBC90. Typed dispatch accounts for both ports' different
// overloaded-slot/destructor layouts and native pointer width. The fallback repeats when b == 0.
void* MaterialManager::getDecalTemplate(unsigned int a, unsigned int b, int index)
{
    IMMCell* cell = getCell(a, b);
    IObjectTemplate* result = 0;
    if (cell)
        result = cell->getDecalTemplate(index, true);
    if (!result)
    {
        cell = getCell(a, 0u);
        if (!cell)
            return 0;
        result = cell->getDecalTemplate(index, true);
        if (!result)
            return 0;
    }
    return result;
}

// getSoundTemplate - / Prefer the exact attacking /
// defending-material cell, then retry with defending material zero. Both cell queries request the
// fallback sound-template entry with `true`.
IObjectTemplate* MaterialManager::getSoundTemplate(unsigned int a, unsigned int b, int index)
{
    IMMCell* cell = getCell(a, b);
    IObjectTemplate* result = 0;
    if (cell)
        result = cell->getSoundTemplate(index, true);
    if (!result)
    {
        cell = getCell(a, 0u);
        if (!cell)
            return 0;
        result = cell->getSoundTemplate(index, true);
        if (!result)
            return 0;
    }
    return result;
}

// As getEffectTemplate, through IMMCell::getSound(index, flag) (slot 13) -
// the caller's flag is passed on both times.
void* MaterialManager::getSound(unsigned int a, unsigned int b, int index, bool flag)
{
    IMMCell* cell = getCell(a, b);
    void* result;
    if (!cell || (result = cell->getSound(index, flag)) == 0)
    {
        cell = getCell(a, 0u);
        if (!cell)
            return 0;
        result = cell->getSound(index, flag);
        if (!result)
            return 0;
    }
    return result;
}

} // namespace world
} // namespace hfe
} // namespace dice
