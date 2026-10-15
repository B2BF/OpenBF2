// Path:     attested - "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\GeometryTemplateManager.cpp"
//           appears verbatim in RendDX9.dll at 0x1021EAE8.
// Contents: BF2 1.50 PE recovery of the template registry/create/find/active ownership spine and
//           the stock-sky console subset.  There is no useful dedicated Linux renderer witness.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ModuleCommon.h"
#include <string>
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Core/EventManager.h"
#include "../RendDX9/RendDX9Base.h"
#include "../IO/Console/Console.h"          // getConsoleWorkingPath: typed (RW-266)
#include "MeshTemplate.h"
#include "GeometryTemplateManager.h"

#include <new>
#include <string.h>

#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"

namespace dice {
namespace hfe {
namespace dx9rend { bool readLevelLoadBoolSetting(const char*, bool); }
namespace geom {

const unsigned int IID_IGeometryTemplate = 0x17701FFFu;
extern const unsigned int IID_IGeometryTemplateManager = 0xAD9665BCu;
extern const unsigned int CID_GeometryTemplateManager  = 0x870CA329u;

extern IBase* g_GeometryTemplateManager;

bool GeometryTemplateNameLess::operator()(const std::string& a, const std::string& b) const
{
    return ::_stricmp(a.c_str(), b.c_str()) < 0;
}

// Slot +0x10 on the interface prefix is still unknown.  Do not invent a declaration merely to
// reach the instruction-proven +0x14/+0x1C setters: keep the owner opaque and isolate those two
// exact x86 calls below.  IBase's four-slot prefix is independently established and remains typed.
class IGeometryTemplate;

namespace {

IBase* templateBase(IGeometryTemplate* value)
{
    return reinterpret_cast<IBase*>(value);
}

void setTemplateName(IGeometryTemplate* value, const std::string& name)
{
    typedef void (__thiscall *Fn)(void*, const std::string&);
    void* target = (*reinterpret_cast<void***>(value))[0x14 / 4];
    reinterpret_cast<Fn>(target)(value, name);
}

void setTemplatePath(IGeometryTemplate* value, const std::string& path)
{
    typedef void (__thiscall *Fn)(void*, const std::string&);
    void* target = (*reinterpret_cast<void***>(value))[0x1C / 4];
    reinterpret_cast<Fn>(target)(value, path);
}

// The owning map's pair destructor at 0x100B1174 releases its interface value first and destroys
// its key second.
void releaseMappedInterface(IGeometryTemplate* value)
{
    IBase* const base = templateBase(value);
    if (base)
        base->release();
}

// Unlinks one entry, then releases its interface - the observable re-entrant ordering of
// PE 0x100C7371: a re-entrant lookup during the release no longer finds the entry.
void eraseOwnedEntry(GeometryTemplateRegistry& map, GeometryTemplateRegistry::iterator entry)
{
    IGeometryTemplate* const value = entry->second;
    map.erase(entry);
    releaseMappedInterface(value);
}

// std::map::_Erase from the root: right subtree, node, left subtree - descending key order -
// with the tree still linked while each interface is released; then the map is relinked empty.
void clearOwnedMap(GeometryTemplateRegistry& map)
{
    for (GeometryTemplateRegistry::reverse_iterator it = map.rbegin(); it != map.rend(); ++it)
        releaseMappedInterface(it->second);
    map.clear();
}

// clear, then the head node. The owner frees its storage afterwards without running a C++
// destructor, so the map is destroyed here.
void destroyOwnedMap(GeometryTemplateRegistry& map)
{
    clearOwnedMap(map);
    map.~GeometryTemplateRegistry();
}

// A non-owning, manually-lived slot lets release() reproduce retail's exact member teardown order:
// optimizer map, vector, string, active reference, template map.  A normal C++ string member would
// be destroyed after the destructor body and therefore move it past the active release.
union ManualStringStorage
{
    char  bytes[sizeof(std::string)];
    void* alignment;

    ManualStringStorage() {}
    ~ManualStringStorage() {}

    std::string& get() { return *reinterpret_cast<std::string*>(bytes); }
};

static_assert(sizeof(ManualStringStorage) == sizeof(std::string),
              "manual string storage must match the string");

const renddx9abi::HostString& getConsoleWorkingPath()
{
    // The published pointer addresses the Console object even though it has no IBase base.
    return reinterpret_cast<io::Console*>(mainConsole)->getWorkingPath();
}

} // anonymous namespace

// Primary vtable 0x10218E48, exactly 25 slots.  Names on the stripped-PE methods are semantic;
// slot positions, signatures, ownership, call ordering, and return behavior are instruction-proven.
class GeometryTemplateManager : public IBase
{
public:
    GeometryTemplateManager();

    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    virtual void registerTemplate(IGeometryTemplate* value,
                                  const ::renddx9abi::HostString& name);      // +0x10
    virtual IGeometryTemplate* createTemplate(unsigned int classId,
                                               const ::renddx9abi::HostString& name); // +0x14
    virtual IGeometryTemplate* getTemplate(const ::renddx9abi::HostString& name);      // +0x18
    virtual void setActive(IGeometryTemplate* value);                          // +0x1C
    virtual IGeometryTemplate* getActive() const;                              // +0x20
    virtual void removeTemplate(const ::renddx9abi::HostString& name);         // +0x24
    virtual void clear();                                                       // +0x28
    virtual void slot_0x2C();
    virtual void slot_0x30();
    virtual void slot_0x34();
    virtual void slot_0x38();
    virtual void loadUsedTemplates();                                        // +0x3C
    virtual void resetTemplateUse();                                         // +0x40
    virtual int getUsedTemplateCount();                                      // +0x44
    virtual void slot_0x48();
    virtual void slot_0x4C();
    virtual bool registerOptimizer(const std::string& className,
                                   const std::string& key);                   // +0x50
    virtual void runOptimizers();                                               // +0x54
    virtual void clearOptimizers();                                             // +0x58
    virtual void slot_0x5C();
    virtual GeometryTemplateRegistry* getTemplateRegistry();                    // +0x60

private:
    void destroyOwnedMembers();
    IGeometryTemplate* createQualified(const std::string& typeName,
                                       const std::string& fullName);

    int                 m_refCount;        // +0x04
    GeometryTemplateRegistry m_templates;  // +0x08, no-case string -> owning template pointer
    IGeometryTemplate*  m_active;          // +0x14, owning
    ManualStringStorage m_string18;        // +0x18, purpose unresolved
    // [UNRESOLVED] element type: nothing recovered reads or fills it.
    std::vector<unsigned char> m_vector34; // +0x34
    // Owning optimizer pointers. Nothing inserts yet (registerOptimizer's insertion is a frontier),
    // so its comparator is unobserved; it shares the registry's type. [CHOSEN, not recovered]
    GeometryTemplateRegistry m_optimizers; // +0x44
};

// Retail allocation (RefFactory_geom_GeometryTemplateManager) is 0x50 bytes. Both maps are
// default-constructed; the string slot is built in place.
GeometryTemplateManager::GeometryTemplateManager()
    : m_refCount(1)
    , m_active(0)
{
    new (m_string18.bytes) std::string();
}

void GeometryTemplateManager::addRef()
{
    ++m_refCount;
}

int GeometryTemplateManager::getRef()
{
    return m_refCount;
}

void GeometryTemplateManager::destroyOwnedMembers()
{
    // PE 0x100C8751, in this exact order.
    destroyOwnedMap(m_optimizers);

    typedef std::vector<unsigned char> UnresolvedVector;
    m_vector34.~UnresolvedVector();

    m_string18.get().~basic_string();

    if (m_active)
        templateBase(m_active)->release();

    destroyOwnedMap(m_templates);
}

int GeometryTemplateManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        destroyOwnedMembers();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

void* GeometryTemplateManager::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IGeometryTemplateManager)
        return const_cast<GeometryTemplateManager*>(this);
    return 0;
}

void GeometryTemplateManager::setActive(IGeometryTemplate* value)
{
    if (m_active)
    {
        // Retail clears the field only after release; re-entrant getActive therefore still sees
        // the old pointer during its final callback.
        templateBase(m_active)->release();
        m_active = 0;
    }

    m_active = value;
    if (value)
        templateBase(value)->addRef();
}

IGeometryTemplate* GeometryTemplateManager::getActive() const
{
    return m_active;
}

void GeometryTemplateManager::registerTemplate(IGeometryTemplate* value,
                                               const ::renddx9abi::HostString& hostName)
{
    if (!value)
        return;

    // The registry's own key copy of the host string.
    const std::string& name = ::renddx9abi::fromHostString(hostName);

    // operator[]: the existing entry, or a fresh one holding null.
    const GeometryTemplateRegistry::iterator node =
        m_templates.insert(
            GeometryTemplateRegistry::value_type(name, static_cast<IGeometryTemplate*>(0))).first;

    IBase* old = templateBase(node->second);
    if (old)
    {
        old->release();
        node->second = 0;
    }

    node->second = value;
    templateBase(value)->addRef();
    setTemplateName(value, name);
    setActive(value);
}

IGeometryTemplate* GeometryTemplateManager::createTemplate(unsigned int classId,
                                                           const ::renddx9abi::HostString& hostName)
{
    const std::string& name = ::renddx9abi::fromHostString(hostName);

    const GeometryTemplateRegistry::iterator found = m_templates.find(name);
    if (found != m_templates.end())
    {
        IGeometryTemplate* value = found->second;
        setActive(value);
        return value;
    }

    IGeometryTemplate* value = reinterpret_cast<IGeometryTemplate*>(
        classManager->createInstance(classId, IID_IGeometryTemplate, 0));
    if (!value)
    {
        setActive(0);
        return 0;
    }

    // operator[] again: the factory may have registered the name in the meantime.
    const GeometryTemplateRegistry::iterator node =
        m_templates.insert(
            GeometryTemplateRegistry::value_type(name, static_cast<IGeometryTemplate*>(0))).first;

    node->second = value;
    reinterpret_cast<GeometryTemplate*>(value)->m_keepData = 0; // the shipped load/keep byte, +4
    templateBase(value)->addRef();                  // registry ownership
    setTemplateName(value, name);

    const renddx9abi::HostString& hostPath = getConsoleWorkingPath();
    const std::string templatePath(hostPath.data(),
                                    static_cast<unsigned int>(hostPath.size()));
    setTemplatePath(value, templatePath);

    setActive(value);
    templateBase(value)->release();                 // drop the factory reference
    return value;                                   // borrowed from registry/active
}

IGeometryTemplate* GeometryTemplateManager::createQualified(const std::string& typeName,
                                                            const std::string& fullName)
{
    std::string className("dice.hfe.geom.GeometryTemplate.");
    className += typeName;

    unsigned int classId = 0;
    if (!classManager->getClassID(renddx9abi::HostString(className.c_str()), &classId))
        return 0;
    // createTemplate is a host-typed slot; the internal caller passes a host string.
    return createTemplate(classId, ::renddx9abi::toHostString(fullName));
}

IGeometryTemplate* GeometryTemplateManager::getTemplate(const ::renddx9abi::HostString& hostName)
{
    const std::string& name = ::renddx9abi::fromHostString(hostName);

    const GeometryTemplateRegistry::iterator found = m_templates.find(name);
    if (found != m_templates.end())
        return found->second;

    unsigned int colon = 0;
    while (colon != name.size() && name.c_str()[colon] != ':')
        ++colon;
    if (colon == name.size())
        return 0;

    const std::string prefix(name.c_str(), colon);
    const std::string suffix(name.c_str() + colon + 1, name.size() - colon - 1);
    (void)suffix; // Constructed and destroyed by retail, but never read (shipped quirk).
    return createQualified(prefix, name); // The full `type:name`, not the suffix, is the map key.
                                          // (createQualified re-enters createTemplate, which is
                                          //  a host-typed slot - see its toHostString below.)
}

void GeometryTemplateManager::removeTemplate(const ::renddx9abi::HostString& hostName)
{
    const std::string& name = ::renddx9abi::fromHostString(hostName);

    const GeometryTemplateRegistry::iterator node = m_templates.find(name);
    if (node != m_templates.end())
        eraseOwnedEntry(m_templates, node);
}

void GeometryTemplateManager::clear()
{
    if (m_active)
    {
        templateBase(m_active)->release();
        m_active = 0;
    }
    clearOwnedMap(m_templates);
    clearOwnedMap(m_optimizers);
}

#define RD9_GTM_FRONTIER(method, slotOffset)                                            \
    void GeometryTemplateManager::method()                                              \
    {                                                                                   \
        RD9_FRONTIER("dice::hfe::geom::GeometryTemplateManager::" #method, slotOffset); \
    }

RD9_GTM_FRONTIER(slot_0x2C, 0x2C)
RD9_GTM_FRONTIER(slot_0x30, 0x30)
RD9_GTM_FRONTIER(slot_0x34, 0x34)
RD9_GTM_FRONTIER(slot_0x38, 0x38)
RD9_GTM_FRONTIER(slot_0x4C, 0x4C)
RD9_GTM_FRONTIER(slot_0x5C, 0x5C)

#undef RD9_GTM_FRONTIER

// clear each template's use byte without changing registry ownership.
void GeometryTemplateManager::resetTemplateUse()
{
    for (GeometryTemplateRegistry::iterator node = m_templates.begin();
         node != m_templates.end(); ++node)
        reinterpret_cast<GeometryTemplate*>(node->second)->m_keepData = 0;
}

// The ResourceManager gate precedes the walk; loaded state is not tested.
int GeometryTemplateManager::getUsedTemplateCount()
{
    if (!dx9rend::resourceManagerIsReady())
        return 0;
    int count = 0;
    for (GeometryTemplateRegistry::iterator node = m_templates.begin();
         node != m_templates.end(); ++node)
        if (reinterpret_cast<GeometryTemplate*>(node->second)->m_keepData)
            ++count;
    return count;
}

// Source-level map erase preserves the PE's advance-before-release order.
void GeometryTemplateManager::loadUsedTemplates()
{
    if (!dx9rend::resourceManagerIsReady())
        return;

    struct Progress { int current; float weight; const char* label; };
    // The count is dispatched through the published singleton, not necessarily this receiver.
    const int count = static_cast<GeometryTemplateManager*>(g_GeometryTemplateManager)
                          ->getUsedTemplateCount();
    Progress progress = { count + 1000, 1.0f, "Geometries" };
    eventManager->postEvent(ECCore, 4, &progress, 0.0f);

    if (m_active)
    {
        templateBase(m_active)->release();
        m_active = 0;
    }
    if (!dx9rend::readLevelLoadBoolSetting("GSKeepTemplates", false))
    {
        GeometryTemplateRegistry::iterator node = m_templates.begin();
        while (node != m_templates.end())
        {
            const GeometryTemplateRegistry::iterator current = node;
            ++node;
            if (!reinterpret_cast<GeometryTemplate*>(current->second)->m_keepData)
                eraseOwnedEntry(m_templates, current);
        }
    }
    else
    {
        // This counterintuitive message belongs to the keep-templates branch in the PE.
        ::renddx9abi::logf("GeometryTemplateManager.cpp:309: Do not keep geom templates");
    }

    for (GeometryTemplateRegistry::iterator node = m_templates.begin();
         node != m_templates.end(); ++node)
    {
        GeometryTemplate* const value = reinterpret_cast<GeometryTemplate*>(node->second);
        if (value->m_keepData && !value->isLoaded())
            value->load();
        value->m_keepData = 0;
    }
}


// Technique initialization gates start pending and clear after successful initialization.
// [INFERRED] Owns the skinned and bundled technique gates.
// [FRONTIER] Static-mesh re-arming needs access to its existing file-local gate;
// expose that gate or an accessor and verify shader changes across level reloads.
// Do not create a second store: current static-mesh tables otherwise retain the first shader set.

unsigned char g_skinnedMeshTechniquesPending = 1;          // byte_102397D4, initial value 1
unsigned char g_bundledMeshTechniquesPending = 1;          // byte_10239294, initial value 1

// Re-arms geometry technique initialization after shader archives change.
// [INFERRED] slot name. Static-mesh re-arming still reports a missing cross-file store.
// An empty registry returns after re-arming; a nonempty registry reaches the temp_rel
// priming [FRONTIER] until template creation and all three technique initializers are callable.
void GeometryTemplateManager::slot_0x48()
{
    // The three once-gates.  [INFERRED] placement for the two that have no rebuilt owner yet: this
    // translation unit holds their only writer, and their readers (the BundledMesh and SkinnedMesh
    // initTechniques bodies) are not recovered, so they move to those files when those land.
    g_skinnedMeshTechniquesPending  = 1;   // byte_102397D4
    g_bundledMeshTechniquesPending  = 1;   // byte_10239294

    // byte_1023982C - see the block above. Named and counted rather than written to a second store.
    // No value is fabricated: the shipped store is a plain byte nothing in this slot reads back.
    {
        static unsigned int hits = 0;
        ++hits;
        if (hits <= 4u || (hits & (hits - 1u)) == 0u)
            ::renddx9abi::logf("[FRONTIER] dice::hfe::geom - the byte_1023982C re-arm "
                               "(g_staticMeshTechniquesPending) reached from "
                               "GeometryTemplateManager::slot_0x48 (hit #%u) - SKIPPED: the store "
                               "is file-local to Geom/StaticMeshTemplate.cpp's anonymous "
                               "namespace. Retail would set it to 1 so the next material read "
                               "rebuilds the static-mesh technique tables", hits);
    }

    if (m_templates.size() == 0)           // cmp [esi+10h], 0 / jbe loc_1015D652 - unsigned
        return;                            // `result` is still the 1 loaded at 1015D536

    RD9_FRONTIER("dice::hfe::geom::GeometryTemplateManager::slot_0x48 "
                 "(the temp_rel priming tail: createTemplate is a frontier and the three "
                 "initTechniques bodies are unrecovered)", 0x48);
}

bool GeometryTemplateManager::registerOptimizer(const std::string& className,
                                                const std::string& key)
{
    std::string full("dice.hfe.geom.GeometryOptimizer.");
    full += className;

    unsigned int classId = 0;
    if (!classManager->getClassID(renddx9abi::HostString(full.c_str()), &classId))
        return false;

    (void)key;
    RD9_FRONTIER("dice::hfe::geom::GeometryTemplateManager::slot_0x4C "
                 "(optimizer insertion after a resolved class)", 0x4C);
    return false;
}

void GeometryTemplateManager::runOptimizers()
{
    if (m_optimizers.empty())
        return;
    RD9_FRONTIER("dice::hfe::geom::GeometryTemplateManager::runOptimizers "
                 "(nonempty registry; optimizer +0x14 interface prefix is unrecovered)", 0x54);
}

void GeometryTemplateManager::clearOptimizers()
{
    clearOwnedMap(m_optimizers);
}

GeometryTemplateRegistry* GeometryTemplateManager::getTemplateRegistry()
{
    return &m_templates;
}

IBase* __fastcall createGeometryTemplateManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    GeometryTemplateManager* self = new GeometryTemplateManager();
    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}


namespace {

const char kGeometryTemplateConsoleTag[] = "geom.geometryTemplate.sky";

GeometryTemplateManager* templateManager()
{
    return static_cast<GeometryTemplateManager*>(g_GeometryTemplateManager);
}

IGeometryTemplate* activeGeometryTemplate()
{
    if (!templateManager())
        return 0;
    IGeometryTemplate* active = templateManager()->getActive();
    return active ? static_cast<IGeometryTemplate*>(
                        templateBase(active)->queryInterface(IID_IGeometryTemplate)) : 0;
}

void* templateVirtual(IGeometryTemplate* value, unsigned int byteOffset)
{
    return (*reinterpret_cast<void***>(value))[byteOffset / 4];
}

bool getTemplateBool(unsigned int byteOffset)
{
    IGeometryTemplate* value = activeGeometryTemplate();
    typedef bool (__thiscall *Fn)(void*);
    return reinterpret_cast<Fn>(templateVirtual(value, byteOffset))(value);
}

void setTemplateBool(unsigned int byteOffset, bool setting)
{
    IGeometryTemplate* value = activeGeometryTemplate();
    typedef void (__thiscall *Fn)(void*, bool);
    reinterpret_cast<Fn>(templateVirtual(value, byteOffset))(value, setting);
}

int getTemplateInt(unsigned int byteOffset)
{
    IGeometryTemplate* value = activeGeometryTemplate();
    typedef int (__thiscall *Fn)(void*);
    return reinterpret_cast<Fn>(templateVirtual(value, byteOffset))(value);
}

void setTemplateInt(unsigned int byteOffset, int setting)
{
    IGeometryTemplate* value = activeGeometryTemplate();
    typedef void (__thiscall *Fn)(void*, int);
    reinterpret_cast<Fn>(templateVirtual(value, byteOffset))(value, setting);
}

IGeometryTemplate* consoleCreateTemplate(const std::string& type, const std::string& name)
{
    std::string className("dice.hfe.geom.GeometryTemplate.");
    className += type.c_str();

    unsigned int classId = 0;
    if (classManager->getClassID(renddx9abi::HostString(className.c_str()), &classId))
    {
        const std::string templateName(name.c_str());
        templateManager()->createTemplate(classId, ::renddx9abi::toHostString(templateName));
    }

    // PE helper 0x100B1389 always returns null even when creation succeeds; the generated binding
    // nevertheless advertises IGeometryTemplate* and marks that null as a return value.
    return 0;
}

BF2_CONSOLE_OBJECT("GeometryTemplate", 2, 0)

class GeometryTemplateCreateBinding : public ConsoleObjectBaseImpl
{
public:
    GeometryTemplateCreateBinding()
        : m_returnValue(0)
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "create";
        m_type = 0;
        m_minNoArgs = 2;
        m_maxNoArgs = 2;
        m_returnType = "IGeometryTemplate*";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "std::string";
        m_argTypes[1] = "std::string";
        m_argNeedsConversion[0] = 0;
        m_argNeedsConversion[1] = 0;
        consoleBindingList(kGeometryTemplateConsoleTag).push_back(this);
    }

    bool isObjectActive() const override { return true; }

    void executeObjectMethod() override
    {
        if (m_argCount == 2)
        {
            m_hasReturnValue = true;
            m_returnValue = consoleCreateTemplate(BF2_CC_ARG(std::string, 0),
                                                   BF2_CC_ARG(std::string, 1));
        }
    }

    void setArgFromString(int index, const std::string& value) override
    {
        BF2_CC_CONVERT(std::string, 1)
        BF2_CC_CONVERT(std::string, 2)
    }

    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS

private:
    IGeometryTemplate* m_returnValue;
};

GeometryTemplateCreateBinding s_geometryTemplateCreateBinding;

BF2_CONSOLE_OBJECT("geometryTemplate", 2, 0)

class GeometryTemplateActiveBinding : public ConsoleObjectBaseImpl
{
public:
    GeometryTemplateActiveBinding()
        : m_returnValue(0)
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "active";
        m_type = 1;
        m_minNoArgs = 0;
        m_maxNoArgs = 1;
        m_returnType = "IGeometryTemplate*";
        m_hasReturnValue = false;
        m_isDynamic = true;
        m_argTypes[0] = "IGeometryTemplate*";
        m_argNeedsConversion[0] = -1;
        consoleBindingList(kGeometryTemplateConsoleTag).push_back(this);
    }

    bool isObjectActive() const override { return templateManager() != 0; }

    void executeObjectMethod() override
    {
        if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            templateManager()->setActive(BF2_CC_ARG(IGeometryTemplate*, 0));
        }
        else if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = templateManager()->getActive();
        }
    }

    void setArgFromString(int index, const std::string& value) override
    {
        if (index == 1)
        {
            static IGeometryTemplate* resolved = 0;
            const std::string templateName(value.c_str());
            resolved = templateManager()->getTemplate(::renddx9abi::toHostString(templateName));
            m_args[0] = reinterpret_cast<std::string*>(&resolved);
        }
    }

    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS

private:
    IGeometryTemplate* m_returnValue;
};

GeometryTemplateActiveBinding s_geometryTemplateActiveBinding;

bool getLightmapOverride() { return getTemplateBool(0x6C); }
void setLightmapOverride(bool value) { setTemplateBool(0x68, value); }
bool getDoNotGenerateLightmaps() { return getTemplateBool(0x64); }
void setDoNotGenerateLightmaps(bool value) { setTemplateBool(0x60, value); }
bool getCompressVertexData() { return getTemplateBool(0x7C); }
void setCompressVertexData(bool value) { setTemplateBool(0x78, value); }
int getMaxTextureRepeat() { return getTemplateInt(0x8C); }
void setMaxTextureRepeat(int value) { setTemplateInt(0x88, value); }

// setLightmapSizeAll geom lod size: the active template's setLightmapSize (+0x70) with the three
// arguments as given; a StaticMesh template stores the size only while lightmapOverride is set.
// The executor fetches the template again without a null check.
void setLightmapSizeAll(int geom, int lod, int size)
{
    activeGeometryTemplate()->setLightmapSize(geom, lod, size);
}

BF2_CONSOLE_METHOD_3_STATIC_ACTIVE(setLightmapSizeAll, setLightmapSizeAll,
                                   activeGeometryTemplate() != 0, int, int, int,
                                   kGeometryTemplateConsoleTag)

BF2_CONSOLE_PROPERTY_FN(lightmapOverride, bool, activeGeometryTemplate() != 0,
                        getLightmapOverride(), setLightmapOverride,
                        kGeometryTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(doNotGenerateLightmaps, bool, activeGeometryTemplate() != 0,
                        getDoNotGenerateLightmaps(), setDoNotGenerateLightmaps,
                        kGeometryTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(compressVertexData, bool, activeGeometryTemplate() != 0,
                        getCompressVertexData(), setCompressVertexData,
                        kGeometryTemplateConsoleTag)
BF2_CONSOLE_PROPERTY_FN(maxTextureRepeat, int, activeGeometryTemplate() != 0,
                        getMaxTextureRepeat(), setMaxTextureRepeat,
                        kGeometryTemplateConsoleTag)

} // anonymous namespace

bool registerGeometryTemplateConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kGeometryTemplateConsoleTag));
}

bool unregisterGeometryTemplateConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kGeometryTemplateConsoleTag));
}


} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
