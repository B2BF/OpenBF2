// Object/ObjectTemplate console bindings split from ObjectManager; file placement is [CHOSEN].
// Commands act on the active object or template and are registered by world::Module.
// Unimplemented bindings remain [FRONTIER] where marked.
#include "../../Core/ConsoleObjectMacro.h"
#include "ObjectManager.h"                       // objectManager->getActiveObject() - the receiver
#include "ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../Game/Objects/ObjectLayoutAccess.h"   // Object.togglePause - the raw +0x14 template read
#include "../../Game/Objects/Fx/EffectBundle.h"      // Object.togglePause - EffectBundle::togglePause
#include "../../Core/Ids.h"                           // CID_EffectBundleTemplate
#include "../../Math/Mat4.h"                     // get/setRotationZXY - Object.rotation
#include "../../Math/Vec3.h"
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

// Handed to the console registry by world::Module::init, the way the original registers 0x8B1B944.
ConsoleObjectList& objectManagerConsoleObjects()
{
    return consoleBindingList(kObjectManagerConsoleTag);
}

} // namespace world
} // namespace hfe
} // namespace dice
