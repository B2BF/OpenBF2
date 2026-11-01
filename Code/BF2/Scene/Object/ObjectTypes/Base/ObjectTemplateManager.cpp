// Owns named object templates and template-class registration. Source path is attested.
// The three object-based lookup overloads remain [CHOSEN, not recovered] null-return placeholders;
// their comments state the required behavior without claiming it is implemented.

// Include Ids before IObjectTemplate so later extern declarations retain the constants' internal linkage.
#include "ObjectTemplateManager.h"
#include <cstdio>    // [TEMP] probe F - createTemplate failure tracer; retire with RW-97
#if !defined(_WIN32)
#include <algorithm>
#endif
#include "../../../../Core/Ids.h"
#include "../../../../Game/Objects/IObjectTemplate.h"     // [UNRESOLVED] true home; not attested
#include "../../../../Game/Objects/ObjectTemplate.h"      // getClassID (+0x10) / preCache (+0x6C)
#include "../../../../Core/ClassManager.h"
#include "../../../../Core/Debug.h"
#include "../../../../Core/EventManager.h"
#include "../../../../Core/StringUtil.h"
#include "../../../../IO/Console/Console.h"
#include "../../ObjectManager.h"                          // world::objectManager, destroyObjects

// createTemplate reuses case-insensitive names or creates through ClassManager.
// New templates enter the name and ID indexes plus the template list; the current
// .con file supplies provenance and the name is registered for console completion.
// Preserve the separate sound-object ID sequence and the published call order.

namespace dice {
namespace hfe {

// calcStringHashValueNoCase and compareIgnoreCase come from Core/StringUtil.h (included above).
// The hash is the one that keys m_objTemplateNameHashes; see Core/Hash.h.

extern ClassManager* classManager;                  // memory[0x8AC85A0] in the decompiler output

namespace world {

// [CHOSEN, not recovered] cast spelling assumes the template interface is the primary base.
// Concrete methods below use the ObjectTemplate view; keep inheritance/layout compatibility explicit.
static const ObjectTemplate* asObjectTemplate(const IObjectTemplate* t)
{
    return reinterpret_cast<const ObjectTemplate*>(t);
}

// Starts with one reference and separate normal/sound ID sequences (sound begins at 0x1000000).
// Registers the core listener at priority -1000 and reports registration failure through Debug.
// The default Hash constructor's spelling is [CHOSEN].
ObjectTemplateManager::ObjectTemplateManager()
  : m_refCount(1),
    m_objectTemplateIdCounter(0),
    m_soundObjectTemplateIdCounter(0x1000000),      // not zero - see note 2
    m_displayError(false),
    m_activeTemplate(0),
    m_engineInitialized(false)
{
    if (!eventManager->registerEventHandler(ECCore, this, -1000))
    {
        Debug(DEBUGTYPE_6, "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.cpp", 51, __FUNCTION__)
            << "Failed to register ObjectTemplateManagers as event handler for ECCore.";
    }
}

// Unregisters the listener and destroys indexes without releasing contained templates.
// Call deleteAll(false) before destroying the manager to release its template ownership.
ObjectTemplateManager::~ObjectTemplateManager()
{
    eventManager->unregisterEventHandler(ECCore, this);
}

// The IBase quartet. addRef 0x8325640 (16 B), getRef 0x8325650 (13 B), release 0x8325660 (57 B),
// queryInterface 0x83256a0 (26 B). All
void ObjectTemplateManager::addRef()
{
    ++m_refCount;
}

int ObjectTemplateManager::getRef()
{
    return m_refCount;
}

// The count is decremented into a register first and that saved value is what comes back, so the
// return value is still valid after `delete this` has run. The ELF also tests `this` for null at
// 0x8325677 before deleting - dead code GCC could not prove away, not reproduced.
int ObjectTemplateManager::release()
{
    const int count = m_refCount - 1;
    m_refCount = count;
    if (count == 0)
        delete this;                    // vtable +0x8C, the deleting destructor (call at 0x8325680)
    return count;
}

// it does not answer for IEventListener. Only IID_IBase and IID_IObjectTemplateManager are
// accepted, and both return the offset-0 pointer. Anything asking this object for its event-
// listener interface gets null - EventManager only ever holds the pointer the constructor handed
// it directly.
void* ObjectTemplateManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<ObjectTemplateManager*>(this);
    if (iid == IID_IObjectTemplateManager)
        return const_cast<ObjectTemplateManager*>(this);
    return 0;
}

// reads the global. See Core/Ids.h.
unsigned int ObjectTemplateManager::getClassID() const
{
    return CID_ObjectTemplateManager;
}

// Takes no argument - it is a latch, not a setter, and nothing ever clears it.
void ObjectTemplateManager::setEngineIsInitialized()
{
    m_engineInitialized = true;
}


IObjectTemplate* ObjectTemplateManager::getActiveObjectTemplate() const
{
    return const_cast<IObjectTemplate*>(m_activeTemplate);
}

void ObjectTemplateManager::setActiveObjectTemplate(const IObjectTemplate* t)
{
    m_activeTemplate = t;
}


void ObjectTemplateManager::setDisplayError(bool displayError)
{
    m_displayError = displayError;
}

// empty. See the constructor's note 3: the subscription is the point, not the
// handler. Reached through the primary vtable at +0x90 and through the _ZThn4_ thunk at 0x8324d80.
void ObjectTemplateManager::handleEvent(EventCategory /*cat*/, unsigned int /*id*/,
                                        EventNode* /*node*/, void* /*data*/)
{
}

// empty / 0x8324c70, 5 bytes - empty. Neither direction of the binary
// template cache is implemented in the dedicated-server build. Do not write bodies for these.
void ObjectTemplateManager::writeBinary(const std::string& /*file*/) const
{
}

void ObjectTemplateManager::readBinary(const std::string& /*file*/)
{
}

// Creates and activates a template, or reactivates the existing case-insensitive name.
// Sound templates use their own ID sequence. The first defining console file remains associated
// with the ID. Failure clears the active template and returns null; preserve the local name copy.

IObjectTemplate* ObjectTemplateManager::createTemplate(unsigned int classId, const std::string& name)
{
    const std::string localName(name);

    // 1. duplicate check: Hash::exists, inlined here by GCC
    const unsigned int probeBucket =
        m_objTemplateNameHashes.bucketOf(localName);

    if (m_objTemplateNameHashes.exists(localName, probeBucket))
    {
        IObjectTemplate* existing = getTemplate(localName);   // virtual, slot +0x24
        setActiveObjectTemplate(existing);                    // virtual, slot +0x54
        return existing;                                      // not an error - see note 1
    }

    // 2. instantiate through the ClassManager
    // CID selects the concrete template class; IID_IObjectTemplate selects the interface to
    // return. See Core/ClassManager.h for why these are separate namespaces.
    IObjectTemplate* temp = static_cast<IObjectTemplate*>(
        classManager->createInstance(classId, IID_IObjectTemplate, 0));

    if (!temp)
    {
        setActiveObjectTemplate(0);
        return 0;
    }

    // 3. flags and identity
    if (m_engineInitialized)
        temp->updateFlags(0x4000, 0);        // [UNRESOLVED] flag name; set only post-init, so it
                                             // most likely marks "created at runtime, not at load"

    temp->setName(localName);

    if ((temp->getFlags() & 0x2000) != 0)    // [UNRESOLVED] flag name; selects the sound id space
        temp->setId(m_soundObjectTemplateIdCounter++);
    else
        temp->setId(m_objectTemplateIdCounter++);

    temp->templateInitialize();

    // 4. remember which .con file defined it
    const std::string& workingFile = io::mainConsole->getWorkingFile();
    if (!workingFile.empty())
        m_workingFolders.insert(std::make_pair(temp->getId(), workingFile));

    // 5. publish into all three indexes
    m_objTemplateTree.insert(std::make_pair(temp->getId(), static_cast<const IObjectTemplate*>(temp)));

    io::mainConsole->addAutoComplArgument(localName);         // stubbed in the dedicated build

    // Re-checked, not assumed: the ELF calls Hash<...>::exists() a second time here - out of line
    // this time, at 0x8323B24 - and re-derives the bucket with a second calcStringHashValueNoCase,
    // even though the probe above already answered. Preserved rather than optimised away.
    const unsigned int insertBucket =
        m_objTemplateNameHashes.bucketOf(localName);
    if (!m_objTemplateNameHashes.exists(localName, insertBucket))
        m_objTemplateNameHashes.insert(localName, temp, insertBucket);   // operator new(0xC), push-front

    m_globalTemplateList.insert(m_globalTemplateList.begin(), temp);

    setActiveObjectTemplate(temp);
    return temp;
}


// The string-keyed overload: resolve the template class name ("GenericFireArm", "SoldierTemplate",
// ...) to a CID, then delegate. Both calls go through the vtable, so a subclass can intercept.
// A className that is not registered resolves to CID 0, which ClassManager will not build - so a
// typo in a .con file produces a null template and a cleared cursor, not a diagnostic.
IObjectTemplate* ObjectTemplateManager::createTemplate(const std::string& className,
                                                       const std::string& name)
{
    const unsigned int cid = getClassIDFromTemplateClassName(className);   // slot +0x20
    return createTemplate(cid, name);                                      // slot +0x58
}

// Registers a CID/name pair, removing any case-insensitive duplicate name first: last registration wins.
// This differs from ClassManager's first-match policy. The original indirect caller remains [UNRESOLVED].
void ObjectTemplateManager::registerTemplateClass(unsigned int classId, const std::string& name)
{
    // Drop any existing entry whose name matches, regardless of its CID.
    typedef std::map<unsigned int, std::string>::iterator Iter;
    for (Iter it = m_tempClasses.begin(); it != m_tempClasses.end(); )
    {
        if (compareIgnoreCase(name.c_str(), it->second.c_str()) == 0)
        {
            m_tempClasses.erase(it);
            break;                                   // the loop breaks on the first match
        }
        ++it;
    }

    // Insert-or-assign under the CID.
    Iter slot = m_tempClasses.lower_bound(classId);
    if (slot == m_tempClasses.end() || classId < slot->first)
        slot = m_tempClasses.insert(slot, std::make_pair(classId, std::string()));

    slot->second.assign(name);
}


// Appends every (cid, class-name) pair to the caller's list, in map order - i.e. ascending CID.
// it does not clear the list first. Calling it twice with the same list appends twice.
void ObjectTemplateManager::getTemplateClasses(
    std::list<std::pair<unsigned int, std::string> >& out) const
{
    typedef std::map<unsigned int, std::string>::const_iterator Iter;
    for (Iter it = m_tempClasses.begin(); it != m_tempClasses.end(); ++it)
        out.insert(out.end(), std::make_pair(it->first, it->second));
}


// Returns the stored class name by reference, or function-local "UnknownTemplateClass"
// on a miss. The text alone cannot distinguish a miss from a class with that name.
const std::string& ObjectTemplateManager::getClassNameFromTemplateClassID(unsigned int classId) const
{
    static const std::string noClass("UnknownTemplateClass");

    typedef std::map<unsigned int, std::string>::const_iterator Iter;
    Iter it = m_tempClasses.find(classId);
    if (it == m_tempClasses.end())
        return noClass;
    return it->second;
}

// Returns the first class ID matching name case-insensitively, or 0.
// Preserve the length check before the bounded comparison.
unsigned int ObjectTemplateManager::getClassIDFromTemplateClassName(const std::string& name) const
{
    typedef std::map<unsigned int, std::string>::const_iterator Iter;
    for (Iter it = m_tempClasses.begin(); it != m_tempClasses.end(); ++it)
    {
        if (name.length() == it->second.length() &&
            compareIgnoreCase(name.c_str(), it->second.c_str(),
                              static_cast<int>(name.length())) == 0)
        {
            return it->first;
        }
    }
    return 0;
}


// The name index, case-insensitively. The empty-name early-out is real and comes first, before the
// hash is computed - so "" never reaches the table even if something managed to insert it.
// The body is Hash<...>::get inlined (the chain walk with strcasecmp at 0x8324236).
IObjectTemplate* ObjectTemplateManager::getTemplate(const std::string& name) const
{
    if (name.empty())
        return 0;

    const IObjectTemplate* found = 0;
    m_objTemplateNameHashes.get(name, found);        // leaves `found` untouched on a miss
    return const_cast<IObjectTemplate*>(found);
}


// A by-name lookup filtered by concrete class: the found template must report exactly `classId`
// from its own getClassID (template vtable +0x10). A subclass does not match its parent's CID.
// The inner lookup goes through the vtable (slot +0x24), so a subclass can intercept it.
IObjectTemplate* ObjectTemplateManager::getTemplate(const std::string& name,
                                                    unsigned int classId) const
{
    IObjectTemplate* t = getTemplate(name);                         // virtual, slot +0x24
    if (!t)
        return 0;
    if (asObjectTemplate(t)->getClassID() != classId)   // template vtable +0x10
        return 0;
    return t;
}


// Not a lookup at all - a checked cast. Hands back the template it was given if and only if the
// template's own getClassID matches, so callers can write one line instead of a test and a cast.
IObjectTemplate* ObjectTemplateManager::getTemplate(const IObjectTemplate* t,
                                                    unsigned int classId) const
{
    if (!t)
        return 0;
    if (asObjectTemplate(t)->getClassID() != classId)   // template vtable +0x10
        return 0;
    return const_cast<IObjectTemplate*>(t);
}


// The id index. this is vtable slot +0x60, out of sequence with the other four getTemplate
// overloads at +0x24..+0x30 - see the correction block in the header.
IObjectTemplate* ObjectTemplateManager::getTemplate(unsigned int templateId) const
{
    typedef std::map<unsigned int, const IObjectTemplate*>::const_iterator Iter;
    Iter it = m_objTemplateTree.find(templateId);
    if (it == m_objTemplateTree.end())
        return 0;
    return const_cast<IObjectTemplate*>(it->second);
}

// [CHOSEN, not recovered] returns null until the object-based lookup is implemented.
// Required behavior reads the object's template and returns it only when its class ID matches.
IObjectTemplate* ObjectTemplateManager::getTemplate(IObject* obj, unsigned int /*classId*/) const
{
    if (!obj)
        return 0;
    return 0;   // [CHOSEN, not recovered] - see above. 0x83238b0, 64 B.
}

// [CHOSEN, not recovered] returns null until the object traversal is implemented.
// Required behavior finds the first depth-first match by template class and input ID, following
// child/sibling links and recursing through the manager's virtual method.
IObject* ObjectTemplateManager::getObjectOfCIDAndInputId(IObject* obj, unsigned int /*cid*/,
                                                         int /*inputId*/) const
{
    if (!obj)
        return 0;
    return 0;   // [CHOSEN, not recovered] - see above. 0x83237a0, 144 B.
}

// [CHOSEN, not recovered] returns null until the object traversal is implemented.
// Required behavior finds the first depth-first interface/input-ID match through child/sibling links,
// recursing through the manager's virtual method.
IObject* ObjectTemplateManager::getObjectOfIIDAndInputId(IObject* obj, unsigned int /*iid*/,
                                                         int /*inputId*/) const
{
    if (!obj)
        return 0;
    return 0;   // [CHOSEN, not recovered] - see above. 0x8323830, 128 B.
}

// Adds to as another name for from; leaves the original name and template's own name unchanged.
// Returns false for empty from, an existing destination or a missing source.
bool ObjectTemplateManager::renameTemplate(const std::string& from, const std::string& to)
{
    const IObjectTemplate* found = 0;

    if (from.empty())
        return false;

    m_objTemplateNameHashes.get(to, found);
    if (found)
        return false;                    // destination name is taken

    m_objTemplateNameHashes.get(from, found);
    if (!found)
        return false;                    // source name does not exist

    const unsigned int bucket =
        m_objTemplateNameHashes.bucketOf(to);
    if (!m_objTemplateNameHashes.exists(to, bucket))
        m_objTemplateNameHashes.insert(to, found, bucket);

    return true;                         // the old name is still there - see the block above
}

// With onlyRuntimeCreated, deletes flagged runtime templates through virtual deleteTemplate.
// Otherwise releases all templates and clears indexes without removing autocomplete entries.
// Both clear active state and reset only the ordinary ID counter; sound IDs continue increasing.
void ObjectTemplateManager::deleteAll(bool onlyRuntimeCreated)
{
    typedef std::list<IObjectTemplate*>::iterator Iter;

    if (onlyRuntimeCreated)
    {
        for (Iter it = m_globalTemplateList.begin(); it != m_globalTemplateList.end(); )
        {
            Iter next = it;
            ++next;                                    // captured before the erase - see above
            IObjectTemplate* t = *it;
            if ((t->getFlags() & 0x4000) != 0)         // template vtable +0x48
                deleteTemplate(t);                     // virtual, slot +0x44
            it = next;
        }
    }
    else
    {
        for (Iter it = m_globalTemplateList.begin(); it != m_globalTemplateList.end(); ++it)
            (*it)->release();                          // template vtable +0x08

        m_objTemplateNameHashes.clear();
        m_globalTemplateList.clear();
        m_workingFolders.clear();
        m_objTemplateTree.clear();
    }

    m_activeTemplate = 0;
    m_objectTemplateIdCounter = 0;                     // the sound counter is left alone
}


// Deletes every template that does not carry flag 0x20. Read together with getAllUsedTemplates
// (which collects exactly the templates that do carry it) and preCacheObjects (which skips them),
// bit 0x20 is the "this template has been used / precached" latch. Game/Objects/ObjectTemplate.h
// records it from preCache()'s own early-out; these two call sites are the independent
// confirmation from the other side.
void ObjectTemplateManager::deleteAllNotUsed()
{
    typedef std::list<IObjectTemplate*>::iterator Iter;
    for (Iter it = m_globalTemplateList.begin(); it != m_globalTemplateList.end(); )
    {
        Iter next = it;
        ++next;                                        // deleteTemplate unlinks *it
        IObjectTemplate* t = *it;
        if ((t->getFlags() & 0x20) == 0)               // template vtable +0x48
            deleteTemplate(t);                         // virtual, slot +0x44
        it = next;
    }
}


// Both halves go through the vtable (+0x24 then +0x44). Note it does not check for null first -
// the by-pointer overload does that.
bool ObjectTemplateManager::deleteTemplate(const std::string& name)
{
    return deleteTemplate(getTemplate(name));
}

// Unpublishes a template from the active cursor, autocomplete, name/list/ID indexes and working folders,
// then releases its reference; other owners can keep it alive. Preserve repeated getId calls.
// Return type remains [UNRESOLVED]: bool is retained, but observed callers discard the value.
bool ObjectTemplateManager::deleteTemplate(IObjectTemplate* t)
{
    if (!t)
        return false;                                  // ELF: eax is left undefined here

    if (t == m_activeTemplate)
        m_activeTemplate = 0;

    io::mainConsole->removeAutoComplArgument(t->getName());

    m_objTemplateNameHashes.remove(t->getName());      // Hash::remove, inlined at 0x83245f0

    const auto it = std::find(m_globalTemplateList.begin(), m_globalTemplateList.end(), t);

    if (it != m_globalTemplateList.end())
        m_globalTemplateList.erase(it);
    m_objTemplateTree.erase(t->getId());
    m_workingFolders.erase(t->getId());

    return t->release() != 0;
}


// Re-points an already-created template at a different defining file. Returns false if the named
// template does not exist. Insert-if-absent, assign-if-present - the same shape as
// registerTemplateClass above.
bool ObjectTemplateManager::setWorkingFolderAndFile(const std::string& templateName,
                                                    const std::string& file)
{
    IObjectTemplate* temp = getTemplate(templateName);
    if (!temp)
        return false;

    typedef std::map<unsigned int, std::string>::iterator Iter;
    const unsigned int id = temp->getId();

    if (m_workingFolders.find(id) == m_workingFolders.end())
    {
        m_workingFolders.insert(std::make_pair(id, file));
    }
    else
    {
        Iter slot = m_workingFolders.lower_bound(id);
        if (slot == m_workingFolders.end() || id < slot->first)
            slot = m_workingFolders.insert(slot, std::make_pair(id, std::string()));
        slot->second.assign(file);
    }
    return true;
}


// Returns the defining file string by template id after resolving the supplied name.
// Missing templates and missing file records both return the shared empty-string reference.
const std::string& ObjectTemplateManager::getWorkingFolderAndFile(
    const std::string& templateName) const
{
    static const std::string emptyString;

    IObjectTemplate* temp = getTemplate(templateName);        // virtual, slot +0x24
    if (!temp)
        return emptyString;

    typedef std::map<unsigned int, std::string>::const_iterator Iter;
    Iter it = m_workingFolders.find(temp->getId());
    if (it == m_workingFolders.end())
        return emptyString;
    return it->second;
}


// Appends every template carrying flag 0x20 - the "used" latch, see deleteAllNotUsed - to the
// caller's vector, in creation order. It does not clear the vector first.
void ObjectTemplateManager::getAllUsedTemplates(std::vector<const IObjectTemplate*>& out) const
{
    typedef std::list<IObjectTemplate*>::const_iterator Iter;
    for (Iter it = m_globalTemplateList.begin(); it != m_globalTemplateList.end(); ++it)
    {
        if (((*it)->getFlags() & 0x20) != 0)              // template vtable +0x48
            out.push_back(*it);
    }
}

// Precaches every spawner and other templates with flag 0x40 but not 0x20, then destroys temporary objects.
// Preserve separate virtual getFlags calls for the two masks.
void ObjectTemplateManager::preCacheObjects() const
{
    typedef std::list<IObjectTemplate*>::const_iterator Iter;
    for (Iter it = m_globalTemplateList.begin(); it != m_globalTemplateList.end(); ++it)
    {
        const ObjectTemplate* t = asObjectTemplate(*it);

        const bool wantsPreCache  = (t->getFlags() & 0x40) != 0;     // template vtable +0x48
        const bool alreadyPreCached = (t->getFlags() & 0x20) != 0;   // called a second time

        if (t->getClassID() == CID_ObjectSpawnerTemplate ||          // template vtable +0x10
            (wantsPreCache && !alreadyPreCached))
        {
            t->preCache();                                           // template vtable +0x6C
        }
    }

    objectManager->destroyObjects();                                 // ds:0x8B1B920, vtable +0x98
}


// Hands out a pointer to the live list. Non-const, so callers can splice it.
std::list<IObjectTemplate*>* ObjectTemplateManager::getGlobalTemplateList()
{
    return &m_globalTemplateList;
}

// Registered through world::ClassFactory<ObjectTemplateManager>; no separate member factory is needed.

} // namespace world
} // namespace hfe
} // namespace dice
