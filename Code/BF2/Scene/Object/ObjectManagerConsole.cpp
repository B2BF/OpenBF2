// Object/ObjectTemplate console bindings split from ObjectManager; file placement is [CHOSEN].
// Commands act on the active object or template and are registered by world::Module.
// Unimplemented bindings remain [FRONTIER] where marked.
#include "../../Core/ConsoleObjectMacro.h"
#include "ObjectManager.h"                       // objectManager->getActiveObject() - the receiver
#include "ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/IObjectTemplate.h"
#include "PlayerManager.h"                      // playerManager - Player.active's receiver
#include "../../Game/Objects/IPlayer.h"         // world::IPlayer - Player.active's argument/result
#include "WindManager.h"                        // world::windManager - WindManager.*'s receiver
#include "ObjectUtil.h"                         // Object.setIsDisabledRecursive - updateFlagsRecursive
#include "../../Game/Objects/ObjectLayoutAccess.h"   // Object.togglePause - the raw +0x14 template read
#include "../../Game/Objects/Fx/EffectBundle.h"      // Object.togglePause - EffectBundle::togglePause
#include "../../Core/Ids.h"                           // CID_EffectBundleTemplate
#include "../../Math/Mat4.h"                     // get/setRotationZXY - Object.rotation
#include "../../Math/Vec3.h"
#include <list>                                 // ObjectTemplate.list - getGlobalTemplateList
#include <sstream>
#include <string>

namespace dice {
namespace hfe {
namespace world {

namespace {

// Stands in for the original's file-scope list (ELF 0x8B1B944). world::Module::init registers it.
const char* const kObjectManagerConsoleTag = "world.objectManager";

// The create target: ConsoleClass353's executor (0x87F1C20) calls the manager's string,string
// overload (+0x5C) and returns the template pointer - which createTemplate has already published
// and made the active cursor on every path (including the already-exists path, which re-activates
// the existing template, and the unknown-class path, which clears the cursor - a .con typo makes
// every following property line silently inert, exactly as shipped).
IObjectTemplate* consoleObjectTemplateCreate(const std::string& className, const std::string& name)
{
    return objectTemplateManager->createTemplate(className, name);   // +0x5C
}

} // namespace

// Registers ObjectTemplate creation under category 11 (Flags).
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 11)

// Read/write bit 1 on any active template to control saving in a separate file.
namespace {
void objTemplSaveInSeparateFileSet(bool v)
{
    IObjectTemplate* t = objectTemplateManager->getActiveObjectTemplate();
    if (v)
        t->updateFlags(1, 0);       // +0x4C
    else
        t->updateFlags(0, 1);
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(saveInSeparateFile, bool, objectTemplateManager->getActiveObjectTemplate() != 0, (objectTemplateManager->getActiveObjectTemplate()->getFlags() & 1) != 0, objTemplSaveInSeparateFileSet, kObjectManagerConsoleTag)

// Boolean properties update the active template's flags; an unused retail scratch store is omitted.
// castsDynamicShadow is held under RW-261. Retail duplicates active-template queries; this helper uses one.
// The create command's current category differs from retail (2,0); this block uses (2,11).
namespace {
template <unsigned int Bit>
void objTemplFlagSet(bool v)
{
    IObjectTemplate* t = objectTemplateManager->getActiveObjectTemplate();
    if (v)
        t->updateFlags(Bit, 0);       // +0x4C
    else
        t->updateFlags(0, Bit);
}
template <unsigned int Bit>
bool objTemplFlagGet()
{
    return (objectTemplateManager->getActiveObjectTemplate()->getFlags() & Bit) != 0;
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(createVisibleInEditor, bool, objectTemplateManager->getActiveObjectTemplate() != 0, objTemplFlagGet<0x8>(), objTemplFlagSet<0x8>, kObjectManagerConsoleTag)
// createNotInGrid and createInvisible set/clear template flag bits 2 and 4 (dedicated PE executors
// 0x6CE8C0 / 0x6CEAE0); makeScript writes them back from the same bits.
BF2_CONSOLE_PROPERTY_FN(createNotInGrid, bool, objectTemplateManager->getActiveObjectTemplate() != 0, objTemplFlagGet<0x2>(), objTemplFlagSet<0x2>, kObjectManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(createInvisible, bool, objectTemplateManager->getActiveObjectTemplate() != 0, objTemplFlagGet<0x4>(), objTemplFlagSet<0x4>, kObjectManagerConsoleTag)

// Read/write bit 0x10 on the active template, excluding it from editor saveAll output.
namespace {
void objTemplIsNotSaveableSet(bool v)
{
    IObjectTemplate* t = objectTemplateManager->getActiveObjectTemplate();
    if (v)
        t->updateFlags(16, 0);
    else
        t->updateFlags(0, 16);
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(isNotSaveable, bool, objectTemplateManager->getActiveObjectTemplate() != 0, (objectTemplateManager->getActiveObjectTemplate()->getFlags() & 16) != 0, objTemplIsNotSaveableSet, kObjectManagerConsoleTag)

// Template flags include use tracking and editor creation. Object initialization marks
// templates used; createdInEditor lets makeScript emit create when the other save flags permit it.
BF2_CONSOLE_PROPERTY_FN(templateHasBeenUsed, bool, objectTemplateManager->getActiveObjectTemplate() != 0, objTemplFlagGet<0x20>(), objTemplFlagSet<0x20>, kObjectManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(createdInEditor, bool, objectTemplateManager->getActiveObjectTemplate() != 0, objTemplFlagGet<0x80>(), objTemplFlagSet<0x80>, kObjectManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(isDestructable, bool, objectTemplateManager->getActiveObjectTemplate() != 0, objTemplFlagGet<0x200>(), objTemplFlagSet<0x200>, kObjectManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(hasRelativeOffset, bool, objectTemplateManager->getActiveObjectTemplate() != 0, objTemplFlagGet<0x1000>(), objTemplFlagSet<0x1000>, kObjectManagerConsoleTag)

// Preserves the preCacheObject command's no-op behavior: setter ignores its argument
// and getter returns false. This command does not set template bit 0x40.
namespace {
void objTemplPreCacheObjectSet(bool)
{
    objectTemplateManager->getActiveObjectTemplate();     // fetched and discarded, as shipped
}
bool objTemplPreCacheObjectGet()
{
    objectTemplateManager->getActiveObjectTemplate();     // fetched and discarded, as shipped
    return false;
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(preCacheObject, bool, objectTemplateManager->getActiveObjectTemplate() != 0, objTemplPreCacheObjectGet(), objTemplPreCacheObjectSet, kObjectManagerConsoleTag)

// Creates a template from exactly two strings: registered class name and template name.
// Returns the template pointer formatted as text.
BF2_CONSOLE_METHOD_2_RET(create, consoleObjectTemplateCreate, IObjectTemplate*, std::string, std::string, kObjectManagerConsoleTag)

// the `Object` block - SetConsoleClassDefaults("Object", 2, 0) at 0x87E0AC7. Receiver: the active
// object. Only `layer` null-checks it (its executor tests getActiveObject() before the vcall);
// the others dereference the pointer isObjectActive already vouched for - as shipped.
BF2_CONSOLE_OBJECT("Object", 2, 0)

// Zero-argument pause command active only for an exact EffectBundleTemplate object.
// Execution relies on availability checking and does not guard the receiver again.
namespace {
EffectBundle* activeEffectBundle()
{
    IObject* object = objectManager->getActiveObject();                                     // +0x90
    if (object && object_detail::LayoutAccess::objectTemplate(object)->getClassID() == CID_EffectBundleTemplate)
        return static_cast<EffectBundle*>(object);
    return 0;
}
void objectTogglePause()
{
    activeEffectBundle()->togglePause();
}
} // namespace
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(togglePause, objectTogglePause, activeEffectBundle() != 0, kObjectManagerConsoleTag)

// Reads or writes the active object's name; requires an active object.
BF2_CONSOLE_PROPERTY(name, std::string, objectManager->getActiveObject(), setName, getName, kObjectManagerConsoleTag)

// Object.absolutePosition - ConsoleClass247 (exec 0x87E7D60, 88 B): SET -> getActiveObject()->
// setAbsolutePosition(Vec3) (+0x70); GET -> getAbsolutePosition() (+0x6C) into the Vec3 return
// member (+0xA4). isObjectActive 0x87E7380: getActiveObject() != 0.
BF2_CONSOLE_PROPERTY(absolutePosition, Vec3, objectManager->getActiveObject(), setAbsolutePosition, getAbsolutePosition, kObjectManagerConsoleTag)

// Reads or writes the active object's absolute transform using the Mat4 text format.
BF2_CONSOLE_PROPERTY(absoluteTransformation, Mat4, objectManager->getActiveObject(), setAbsoluteTransformation, getAbsoluteTransformation, kObjectManagerConsoleTag)

// Reads or writes yaw/pitch/roll in degrees. Converts to negated ZXY Euler components
// and uses relative transforms when parented, absolute transforms otherwise.
namespace {
void objectRotationSet(const Vec3& v)
{
    IObject* object = objectManager->getActiveObject();
    Mat4 m = object->getParent() ? object->getRelativeTransformation()
                                 : object->getAbsoluteTransformation();
    setRotationZXY(m, -v.z, -v.y, -v.x);
    if (object->getParent())
        object->setRelativeTransformation(m);
    else
        object->setAbsoluteTransformation(m);
}
Vec3 objectRotationGet()
{
    IObject* object = objectManager->getActiveObject();
    const Mat4& m = object->getParent() ? object->getRelativeTransformation()
                                        : object->getAbsoluteTransformation();
    float z = 0.0f, x = 0.0f, y = 0.0f;
    getRotationZXY(m, z, x, y);
    Vec3 out;
    out.x = -y;
    out.y = -x;
    out.z = -z;
    return out;
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(rotation, Vec3, objectManager->getActiveObject() != 0, objectRotationGet(), objectRotationSet, kObjectManagerConsoleTag)

// Object.layer - ConsoleClass256 (exec 0x87EB460, 87 B): an INT property over the object's u8
// layer - SET: `if (active) active->setLayer((u8)v)` (+0x120); GET: `active ? getLayer() : 0`
// (+0x11C). The one binding in the block that null-checks the active object.
namespace {
void objectLayerSet(int v)
{
    IObject* object = objectManager->getActiveObject();
    if (object)
        object->setLayer(static_cast<unsigned char>(v));
}
int objectLayerGet()
{
    IObject* object = objectManager->getActiveObject();
    return object ? static_cast<int>(object->getLayer()) : 0;
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(layer, int, objectManager->getActiveObject() != 0, objectLayerGet(), objectLayerSet, kObjectManagerConsoleTag)

// Reads or writes the active object's byte-sized group through an integer property.
// No active object makes the setter a no-op and the getter return zero.
namespace {
void objectGroupSet(int v)
{
    IObject* object = objectManager->getActiveObject();
    if (object)
        object->setGroup(static_cast<unsigned char>(v));
}
int objectGroupGet()
{
    IObject* object = objectManager->getActiveObject();
    return object ? static_cast<int>(object->getGroup()) : 0;
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(group, int, objectManager->getActiveObject() != 0, objectGroupGet(), objectGroupSet, kObjectManagerConsoleTag)

// Object.isOvergrowth - ConsoleClass249 (exec 0x87E9000, 87 B): setIsOvergrowth (+0x12C) /
// getIsOvergrowth (+0x130); the bool return member packs at +0xA2.
BF2_CONSOLE_PROPERTY(isOvergrowth, bool, objectManager->getActiveObject(), setIsOvergrowth, getIsOvergrowth, kObjectManagerConsoleTag)

// Object.notInAI - ConsoleClass250 (base 0x8B1C7E0, exec 0x87E9590, active 0x87E92A0): SET ->
// setNotInAI(bool) (+0x134, IObject slot 77); GET -> getNotInAI() (+0x138, slot 78) into the +0xA2
// bool return member. SET is the lower slot here and GET the higher, the reverse of
// disableChildren below - IObject's declaration order, not a reading error.
BF2_CONSOLE_PROPERTY(notInAI, bool, objectManager->getActiveObject(), setNotInAI, getNotInAI, kObjectManagerConsoleTag)

// Object.setLightSourceMask <int> - ConsoleClass251 (exec 0x87E9970, 43 B): argc must be 1 ->
// getActiveObject()->setLightSourceMask(int) (+0x13C). isObjectActive 0x87E9680: the active test.
namespace {
void objectSetLightSourceMask(int mask)
{
    objectManager->getActiveObject()->setLightSourceMask(mask);
}
} // namespace
BF2_CONSOLE_METHOD_1_ACTIVE(setLightSourceMask, objectSetLightSourceMask, objectManager->getActiveObject() != 0, int, kObjectManagerConsoleTag)

// Object.disableChildren - ConsoleClass253 (base 0x8B1CA20, exec 0x87EA270, active 0x87E9F80):
// SET -> setDisableChildren(bool) (+0x148, IObject slot 82); GET -> getDisableChildren() (+0x144,
// slot 81). No null guard in either arm - it dereferences what isObjectActive vouched for, the
// block's majority behaviour.
BF2_CONSOLE_PROPERTY(disableChildren, bool, objectManager->getActiveObject(), setDisableChildren, getDisableChildren, kObjectManagerConsoleTag)

// Object.setIsDisabledRecursive <bool> - dedicated PE ctor sub_6CDB10 (vtable off_7FDB20, m_type = 0,
// argc 1, arg "bool", return "void", m_isDynamic = 0). isObjectActive sub_6CDC00 selects flag 1 and
// returns `getActiveObject() != 0` (ObjectManager vtable +0x90). Executor sub_6CDC90 -> sub_6CC160 ->
// sub_6D8D80 applies `updateFlags(selector, 0)` to the active object and every descendant, or
// `updateFlags(0, selector)` when clearing; the walk follows getChild (+0xEC) and the +0x2C sibling
// link. Selector 1 is OFIsDisabled: the sibling command setIsVisibleRecursive shares the same shape
// with selector 2 (OFIsVisible), fixing the mapping. `world::updateFlagsRecursive` is that walk.
namespace {
void objectSetIsDisabledRecursive(bool disable)
{
    const unsigned int mask = IObject::OFIsDisabled;
    updateFlagsRecursive(objectManager->getActiveObject(),
                         disable ? mask : 0u, disable ? 0u : mask);
}
} // namespace
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setIsDisabledRecursive, objectSetIsDisabledRecursive, objectManager->getActiveObject() != 0, bool, kObjectManagerConsoleTag)

// Player.active - get or select the active player. Written out rather than through a macro because
// the two argument counts differ in their return: argc 0 returns the active player, argc 1 sets it
// and publishes no value. Dedicated PE defaults 0x773610; ctor 0x6CFF50 (vtable off_7FE598,
// +0xA1 = 0); isObjectActive 0x4921F0 is unconditionally true; executor 0x6D0090 reads the active
// player through playerManager +0x80 / writes it through +0x84. A name argument resolves through
// playerManager->getPlayerFromName (PE parser 0x6DB5D0, slot +0x1C); the getter formats the player's
// name, or NoPlayer when null (PE formatter 0x6DB5A0). Registered on the objectManager/world-Module
// list, matching the retail translation unit.
BF2_CONSOLE_OBJECT("Player", 2, 0)
namespace {
class ConsoleClassPlayerActive : public ::dice::hfe::ConsoleObjectBaseImpl
{
public:
    ConsoleClassPlayerActive()
    {
        m_objectName     = ::dice::hfe::consoleDefaultName();
        m_access         = ::dice::hfe::consoleDefaultAccess();
        m_category       = ::dice::hfe::consoleDefaultCategory();
        m_functionName   = "active";
        m_type           = 1;
        m_minNoArgs      = 0;
        m_maxNoArgs      = 1;
        m_returnType     = "IPlayer*";
        m_hasReturnValue = false;
        m_isDynamic      = false;
        m_argTypes[0]    = "IPlayer*";
        m_argNeedsConversion[0] = (std::string(m_argTypes[0]) == "std::string") ? 0 : -1;
        ::dice::hfe::consoleBindingList(kObjectManagerConsoleTag).push_back(this);
    }
    virtual bool isObjectActive() const { return true; }
    virtual void executeObjectMethod()
    {
        if (m_argCount == 0)
        {
            m_hasReturnValue = true;
            m_returnValue = playerManager->getActivePlayer();
        }
        else if (m_argCount == 1)
        {
            m_hasReturnValue = false;
            playerManager->setActivePlayer(BF2_CC_ARG(IPlayer*, 0));
        }
    }
    virtual std::string getReturnValueAsString() const
    {
        if (!m_hasReturnValue)
            return std::string();
        if (!m_returnValue)
            return std::string("NoPlayer");
        return m_returnValue->getName();
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        if (index == 1)
        {
            static IPlayer* s = 0;
            std::string name;
            std::istringstream in(value);
            in >> name;
            s = playerManager->getPlayerFromName(name);
            m_args[0] = reinterpret_cast<std::string*>(&s);
        }
    }
    BF2_CC_COMMON_HOOKS
private:
    IPlayer* m_returnValue;
};
static ConsoleClassPlayerActive s_consoleClassPlayerActive;
} // namespace

// windManager.globalWindSpeed / globalWindDirection - dedicated PE defaults 0x772E9E
// ("WindManager", access 2, category 0); ctors 0x6C8BD0 / 0x6C8D80 (vtable off_7FC5A8 / off_7FC640,
// set/get properties, m_isDynamic = 1). isObjectActive 0x6C8E80 is `dword_89AE54 != 0` (windManager);
// executors 0x6C8D20 / 0x6C8F10 dispatch WindManager +0x1C/+0x20 (speed get/set) and +0x24/+0x28
// (direction get/set). Retail appends these to the objectManager/world-Module list (0x89B270).
BF2_CONSOLE_OBJECT("WindManager", 2, 0)
namespace {
WindManager* activeWindManager() { return static_cast<WindManager*>(windManager); }
void setGlobalWindSpeed(float speed) { activeWindManager()->setGlobalWindSpeed(speed); }
void setGlobalWindDirection(const Vec3& direction)
{
    activeWindManager()->setGlobalWindDirection(direction);
}
} // namespace
BF2_CONSOLE_PROPERTY_FN(globalWindSpeed, float, activeWindManager() != 0,
                        activeWindManager()->getGlobalWindSpeed(), setGlobalWindSpeed,
                        kObjectManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(globalWindDirection, Vec3, activeWindManager() != 0,
                        activeWindManager()->getGlobalWindDirection(), setGlobalWindDirection,
                        kObjectManagerConsoleTag)

// objectTemplate.name / objectTemplate.list - dedicated PE defaults 0x7733DE ("ObjectTemplate",
// access 2, category 0); both registered on the objectManager/world-Module list (0x89B270).
namespace {
IObjectTemplate* activeObjectTemplate()
{
    return objectTemplateManager->getActiveObjectTemplate();   // +0x50
}

// Newline-joined names of every registered template whose name starts with filter (all of them when
// filter is empty), matching PE helper 0x6F1200's iteration of objectTemplateManager +52.
std::string objectTemplateList(const std::string& filter)
{
    std::string result;
    std::list<IObjectTemplate*>* templates = objectTemplateManager->getGlobalTemplateList();   // +0x64
    if (!templates)
        return result;
    for (std::list<IObjectTemplate*>::const_iterator it = templates->begin(); it != templates->end(); ++it)
    {
        IObjectTemplate* t = *it;
        if (!t)
            continue;
        const std::string& name = t->getName();                // +0x28
        if (name.compare(0, filter.size(), filter) == 0)
        {
            result += name;
            result += "\n";
        }
    }
    return result;
}
} // namespace

// ctor 0x6CE090 (vtable off_7FDCE0, m_type = 2, argc 0, returns std::string, m_isDynamic = 1).
// isObjectActive 0x6CE130 = `getActiveObjectTemplate() != 0`; executor 0x6CE150 returns the active
// template's name.
namespace {
class ConsoleClassObjectTemplateName : public ConsoleObjectBaseImpl
{
public:
    ConsoleClassObjectTemplateName()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "name";
        m_type = 2;
        m_minNoArgs = 0;
        m_maxNoArgs = 0;
        m_returnType = "std::string";
        m_hasReturnValue = false;
        m_isDynamic = true;
        consoleBindingList(kObjectManagerConsoleTag).push_back(this);
    }
    bool isObjectActive() const override { return activeObjectTemplate() != 0; }
    void executeObjectMethod() override
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = true;
        m_returnValue = activeObjectTemplate()->getName();
    }
    void setArgFromString(int, const std::string&) override { }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
static ConsoleClassObjectTemplateName s_consoleObjectTemplateName;
} // namespace

// ctor 0x6D4140 (vtable off_7FFE28, method, argc 0..1, arg std::string, returns std::string,
// m_isDynamic = 0). isObjectActive 0x4921F0 is unconditionally true; executor 0x6D5A70 forwards the
// argument (empty when omitted) to the template-list helper.
namespace {
class ConsoleClassObjectTemplateList : public ConsoleObjectBaseImpl
{
public:
    ConsoleClassObjectTemplateList()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "list";
        m_type = 0;
        m_minNoArgs = 0;
        m_maxNoArgs = 1;
        m_returnType = "std::string";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "std::string";
        m_argNeedsConversion[0] = 0;
        consoleBindingList(kObjectManagerConsoleTag).push_back(this);
    }
    bool isObjectActive() const override { return true; }
    void executeObjectMethod() override
    {
        if (m_argCount > 1) return;
        std::string filter;
        if (m_argCount == 1)
            filter = BF2_CC_ARG(std::string, 0);
        m_hasReturnValue = true;
        m_returnValue = objectTemplateList(filter);
    }
    void setArgFromString(int index, const std::string& value) override { BF2_CC_CONVERT(std::string, 1) }
    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
private:
    std::string m_returnValue;
};
static ConsoleClassObjectTemplateList s_consoleObjectTemplateList;
} // namespace

// Handed to the console registry by world::Module::init, the way the original registers 0x8B1B944.
ConsoleObjectList& objectManagerConsoleObjects()
{
    return consoleBindingList(kObjectManagerConsoleTag);
}

} // namespace world
} // namespace hfe
} // namespace dice
