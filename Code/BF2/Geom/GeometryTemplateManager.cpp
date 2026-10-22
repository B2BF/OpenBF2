// Path:     attested - "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\GeometryTemplateManager.cpp"
//           appears verbatim in RendDX9.dll at 0x1021EAE8.
// Contents: BF2 1.50 PE recovery of the template registry/create/find/active ownership spine and
//           the stock-sky console subset.  There is no useful dedicated Linux renderer witness.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Core/EventManager.h"
#include "../RendDX9/RendDX9Base.h"
#if BF2_PLUGIN_ABI_NATIVE
#include "../IO/Console/Console.h"          // getConsoleWorkingPath: typed (RW-266)
#endif
#include "MeshTemplate.h"

#include <new>

#if BF2_PLUGIN_ABI_NATIVE
#include "../Core/ConsoleObjectMacro.h"
#include "../Core/ConsoleObjects.h"
#endif

namespace dice {
namespace hfe {
namespace dx9rend { bool readLevelLoadBoolSetting(const char*, bool); }
namespace geom {

const unsigned int IID_IGeometryTemplate = 0x17701FFFu;
extern const unsigned int IID_IGeometryTemplateManager = 0xAD9665BCu;
extern const unsigned int CID_GeometryTemplateManager  = 0x870CA329u;

extern IBase* g_GeometryTemplateManager;

// Slot +0x10 on the interface prefix is still unknown.  Do not invent a declaration merely to
// reach the instruction-proven +0x14/+0x1C setters: keep the owner opaque and isolate those two
// exact x86 calls below.  IBase's four-slot prefix is independently established and remains typed.
class IGeometryTemplate;

namespace {

IBase* templateBase(IGeometryTemplate* value)
{
    return reinterpret_cast<IBase*>(value);
}

void setTemplateName(IGeometryTemplate* value, const vc71::string& name)
{
    typedef void (__thiscall *Fn)(void*, const vc71::string&);
    void* target = (*reinterpret_cast<void***>(value))[0x14 / sizeof(void*)];
    reinterpret_cast<Fn>(target)(value, name);
}

void setTemplatePath(IGeometryTemplate* value, const vc71::string& path)
{
    typedef void (__thiscall *Fn)(void*, const vc71::string&);
    void* target = (*reinterpret_cast<void***>(value))[0x1C / sizeof(void*)];
    reinterpret_cast<Fn>(target)(value, path);
}

// The owning map's pair destructor at 0x100B1174 releases its interface value first and destroys
// its key second.  mapEraseWithValueDestroy invokes this hook only after the node has been unlinked,
// matching the observable re-entrant ordering of PE 0x100C7371.
void releaseMappedInterface(vc71::map_node* node)
{
    IBase* value = static_cast<IBase*>(vc71::mapNodeValue(node));
    if (value)
        value->release();
}

// recurse through the right subtree, destroy the current node, then
// continue down the left side.  The tree stays linked during value release just as retail's
// std::map::_Erase does; this deliberately preserves its awkward re-entrant view while clearing.
// The traversal algorithm is recovered, but the node shape remains Vc71Map's unbalanced
// [CHOSEN, not recovered] RW-74 tree; destruction order can therefore differ after insertions.
void clearOwnedSubtree(vc71::map_node* node, vc71::map_node* head)
{
    while (node && node != head)
    {
        clearOwnedSubtree(node->right, head);
        vc71::map_node* left = node->left;
        releaseMappedInterface(node);
        reinterpret_cast<vc71::string*>(node->value)->~string();
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

void clearOwnedMap(vc71::map_layout& map)
{
    if (!map.head)
        return;
    clearOwnedSubtree(map.head->parent, map.head);
    map.relinkEmpty();
}

void destroyOwnedMap(vc71::map_layout& map)
{
    clearOwnedMap(map);
    if (map.head)
        ::renddx9abi::engineFree(map.head);
    map.comp = 0;
    map.head = 0;
    map.size = 0;
}

// A non-owning, manually-lived slot lets release() reproduce retail's exact member teardown order:
// optimizer map, vector, string, active reference, template map.  A normal C++ string member would
// be destroyed after the destructor body and therefore move it past the active release.
union Vc71StringStorage
{
    char  bytes[sizeof(vc71::string)];
    void* alignment;

    Vc71StringStorage() {}
    ~Vc71StringStorage() {}

    vc71::string& get() { return *reinterpret_cast<vc71::string*>(bytes); }
};

static_assert(sizeof(Vc71StringStorage) == sizeof(vc71::string),
              "manual VC7.1 string storage must remain 28 bytes");

const renddx9abi::HostString& getConsoleWorkingPath()
{
    // HostString selects the native or VC7.1 return type across the console boundary.
    // The published pointer addresses the Console object even though it has no IBase base.
#if BF2_PLUGIN_ABI_NATIVE
    return reinterpret_cast<io::Console*>(mainConsole)->getWorkingPath();
#else
    typedef const renddx9abi::HostString& (__thiscall *Fn)(void*);
    void* console = static_cast<void*>(mainConsole);
    void* target = (*reinterpret_cast<void***>(console))[0x84 / sizeof(void*)];
    return reinterpret_cast<Fn>(target)(console);
#endif
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
    virtual bool registerOptimizer(const vc71::string& className,
                                   const vc71::string& key);                   // +0x50
    virtual void runOptimizers();                                               // +0x54
    virtual void clearOptimizers();                                             // +0x58
    virtual void slot_0x5C();
    virtual vc71::map_layout* getTemplateRegistry();                            // +0x60

private:
    void destroyOwnedMembers();
    IGeometryTemplate* createQualified(const vc71::string& typeName,
                                       const vc71::string& fullName);

    int                 m_refCount;        // +0x04
    vc71::map_layout    m_templates;       // +0x08, no-case string -> owning template pointer
    IGeometryTemplate*  m_active;          // +0x14, owning
    Vc71StringStorage   m_string18;        // +0x18, purpose unresolved
    vc71::vector_layout m_vector34;        // +0x34
    vc71::map_layout    m_optimizers;      // +0x44, owning optimizer pointer
};

static_assert(sizeof(GeometryTemplateManager) == 0x50,
              "RefFactory_geom_GeometryTemplateManager allocates 0x50");

GeometryTemplateManager::GeometryTemplateManager()
    : m_refCount(1)
    , m_active(0)
{
    m_templates.construct();
    new (m_string18.bytes) vc71::string();
    m_vector34.alloc = 0;
    m_vector34.first = 0;
    m_vector34.last  = 0;
    m_vector34.end   = 0;
    m_optimizers.construct();
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

    if (m_vector34.first)
        ::renddx9abi::engineFree(m_vector34.first);
    m_vector34.first = 0;
    m_vector34.last = 0;
    m_vector34.end = 0;

    m_string18.get().~string();

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

    // The registry is keyed by vc71 strings; convert once here.  Identity in the compat flavour.
    const vc71::string& name = ::renddx9abi::toVc71String(hostName);

    vc71::map_node* node = vc71::mapInsertInsensitive(m_templates, 0x30, name);
    if (!node)
    {
        RD9_FRONTIER("dice::hfe::geom::GeometryTemplateManager::registerTemplate "
                     "(VC7.1 map allocation failed)", 0x10);
    }

    IBase* old = static_cast<IBase*>(vc71::mapNodeValue(node));
    if (old)
    {
        old->release();
        vc71::mapNodeValueRef(node) = 0;
    }

    vc71::mapNodeValueRef(node) = value;
    templateBase(value)->addRef();
    setTemplateName(value, name);
    setActive(value);
}

IGeometryTemplate* GeometryTemplateManager::createTemplate(unsigned int classId,
                                                           const ::renddx9abi::HostString& hostName)
{
    const vc71::string& name = ::renddx9abi::toVc71String(hostName);

    vc71::map_node* found = vc71::mapFindInsensitive(m_templates, name);
    if (found)
    {
        IGeometryTemplate* value =
            static_cast<IGeometryTemplate*>(vc71::mapNodeValue(found));
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

    vc71::map_node* node = vc71::mapInsertInsensitive(m_templates, 0x30, name);
    if (!node)
    {
        RD9_FRONTIER("dice::hfe::geom::GeometryTemplateManager::createTemplate "
                     "(VC7.1 map allocation failed)", 0x14);
    }

    vc71::mapNodeValueRef(node) = value;
    reinterpret_cast<unsigned char*>(value)[4] = 0; // GeometryTemplate's shipped load/keep byte.
    templateBase(value)->addRef();                  // registry ownership
    setTemplateName(value, name);

    const renddx9abi::HostString& hostPath = getConsoleWorkingPath();
#if BF2_PLUGIN_ABI_NATIVE
    const vc71::string templatePath(hostPath.data(),
                                    static_cast<unsigned int>(hostPath.size()));
    setTemplatePath(value, templatePath);
#else
    setTemplatePath(value, hostPath);
#endif

    setActive(value);
    templateBase(value)->release();                 // drop the factory reference
    return value;                                   // borrowed from registry/active
}

IGeometryTemplate* GeometryTemplateManager::createQualified(const vc71::string& typeName,
                                                            const vc71::string& fullName)
{
    vc71::string className("dice.hfe.geom.GeometryTemplate.");
    className += typeName;

    unsigned int classId = 0;
    if (!classManager->getClassID(renddx9abi::HostString(className.c_str()), &classId))
        return 0;
    // createTemplate is a host-typed slot, so an internal vc71 caller converts on the way in.
    return createTemplate(classId, ::renddx9abi::toHostString(fullName));
}

IGeometryTemplate* GeometryTemplateManager::getTemplate(const ::renddx9abi::HostString& hostName)
{
    const vc71::string& name = ::renddx9abi::toVc71String(hostName);

    vc71::map_node* found = vc71::mapFindInsensitive(m_templates, name);
    if (found)
        return static_cast<IGeometryTemplate*>(vc71::mapNodeValue(found));

    unsigned int colon = 0;
    while (colon != name.size() && name.c_str()[colon] != ':')
        ++colon;
    if (colon == name.size())
        return 0;

    const vc71::string prefix(name.c_str(), colon);
    const vc71::string suffix(name.c_str() + colon + 1, name.size() - colon - 1);
    (void)suffix; // Constructed and destroyed by retail, but never read (shipped quirk).
    return createQualified(prefix, name); // The full `type:name`, not the suffix, is the map key.
                                          // (createQualified re-enters createTemplate, which is
                                          //  a host-typed slot - see its toHostString below.)
}

void GeometryTemplateManager::removeTemplate(const ::renddx9abi::HostString& hostName)
{
    const vc71::string& name = ::renddx9abi::toVc71String(hostName);

    vc71::map_node* node = vc71::mapFindInsensitive(m_templates, name);
    if (node)
        vc71::mapEraseWithValueDestroy(m_templates, node, &releaseMappedInterface);
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
    for (vc71::map_node* node = vc71::mapFirst(m_templates); node;
         node = vc71::mapNext(m_templates, node))
        reinterpret_cast<GeometryTemplate*>(vc71::mapNodeValue(node))->m_keepData = 0;
}

// The ResourceManager gate precedes the walk; loaded state is not tested.
int GeometryTemplateManager::getUsedTemplateCount()
{
    if (!dx9rend::resourceManagerIsReady())
        return 0;
    int count = 0;
    for (vc71::map_node* node = vc71::mapFirst(m_templates); node;
         node = vc71::mapNext(m_templates, node))
        if (reinterpret_cast<GeometryTemplate*>(vc71::mapNodeValue(node))->m_keepData)
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
        vc71::map_node* node = vc71::mapFirst(m_templates);
        while (node)
        {
            vc71::map_node* const current = node;
            node = vc71::mapNext(m_templates, node);
            if (!reinterpret_cast<GeometryTemplate*>(vc71::mapNodeValue(current))->m_keepData)
                vc71::mapEraseWithValueDestroy(m_templates, current, releaseMappedInterface);
        }
    }
    else
    {
        // This counterintuitive message belongs to the keep-templates branch in the PE.
        ::renddx9abi::logf("GeometryTemplateManager.cpp:309: Do not keep geom templates");
    }

    for (vc71::map_node* node = vc71::mapFirst(m_templates); node;
         node = vc71::mapNext(m_templates, node))
    {
        GeometryTemplate* const value =
            reinterpret_cast<GeometryTemplate*>(vc71::mapNodeValue(node));
        if (value->m_keepData && !value->isLoaded())
            value->load();
        value->m_keepData = 0;
    }
}


// Technique initialization gates start pending and clear after successful initialization.
// [INFERRED] Owns the skinned and bundled technique gates.
void staticMeshRearmTechniques();
void staticMeshPrimeTechniques();
void bundledMeshInitTechniques();
void skinnedMeshInitTechniques();

unsigned char g_skinnedMeshTechniquesPending = 1;          // byte_102397D4, initial value 1
unsigned char g_bundledMeshTechniquesPending = 1;          // byte_10239294, initial value 1

// Re-arms geometry technique initialization after shader archives change.
// [INFERRED] slot name. A nonempty registry primes each shared table through a temporary
// template, preserving the factory, active-template and removal side effects.
void GeometryTemplateManager::slot_0x48()
{
    staticMeshRearmTechniques();
    g_skinnedMeshTechniquesPending  = 1;   // byte_102397D4
    g_bundledMeshTechniquesPending  = 1;   // byte_10239294

    if (m_templates.size == 0)             // cmp [esi+10h], 0 / jbe loc_1015D652 - unsigned
        return;                            // `result` is still the 1 loaded at 1015D536

    createTemplate(0x870CA653u, ::renddx9abi::HostString("temp_rel"));
    staticMeshPrimeTechniques();
    removeTemplate(::renddx9abi::HostString("temp_rel"));
    createTemplate(0x870CA651u, ::renddx9abi::HostString("temp_rel"));
    bundledMeshInitTechniques();
    removeTemplate(::renddx9abi::HostString("temp_rel"));
    createTemplate(0x870CA652u, ::renddx9abi::HostString("temp_rel"));
    skinnedMeshInitTechniques();
    removeTemplate(::renddx9abi::HostString("temp_rel"));
}

// Internal bridges retain typed virtual dispatch without exposing the private concrete class.
void geometryTemplateManagerClear()
{
    static_cast<GeometryTemplateManager*>(g_GeometryTemplateManager)->clear();
}

void geometryTemplateManagerRearmTechniques()
{
    static_cast<GeometryTemplateManager*>(g_GeometryTemplateManager)->slot_0x48();
}

bool GeometryTemplateManager::registerOptimizer(const vc71::string& className,
                                                const vc71::string& key)
{
    vc71::string full("dice.hfe.geom.GeometryOptimizer.");
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

vc71::map_layout* GeometryTemplateManager::getTemplateRegistry()
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

#if BF2_PLUGIN_ABI_NATIVE

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
    return (*reinterpret_cast<void***>(value))[byteOffset / sizeof(void*)];
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
    vc71::string className("dice.hfe.geom.GeometryTemplate.");
    className += type.c_str();

    unsigned int classId = 0;
    if (classManager->getClassID(renddx9abi::HostString(className.c_str()), &classId))
    {
        const vc71::string templateName(name.c_str());
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
            const vc71::string templateName(value.c_str());
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

#endif // BF2_PLUGIN_ABI_NATIVE

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
