// Registers animation-system, value-holder, manager, bundle, trigger and ragdoll console commands.
// Object names are registered capitalized and resolved case-insensitively.
// The animation playback settings include the remaining RW-276 integration surface;
// Ragdoll constraint serialization still stops at its named boundary.

#include "AnimationManager.h"
#include "RagDollTemplate.h"            // the RagDoll console object
#include "AnimationSystem.h"
#include "BoneAnimation.h"
#include "Bundle.h"
#include "CameraSpring.h"
#include "Trigger.h"
#include "TriggerManager.h"
#include "ValueHolder.h"

#include "../../Core/ConsoleObjectMacro.h"
#include "../../Math/Vec3.h"

#include <cstdio>       // the `save` stop
#include <cstdlib>
#include <cmath>
#include <string>

namespace dice {
namespace hfe {
namespace animationTools {

namespace {

// Console list registered and removed by AnimModule.
const char* const kBoneAnimConsoleTag = "anim.boneAnimation";


// Returns the active animation template; console bindings without an active target do nothing.

anim::AnimationSystemTemplate* activeTemplate()
{
    return anim::animationSystemTemplateManager
         ? anim::animationSystemTemplateManager->getActive() : 0;
}

// in the ELF. The spring belongs to the active template and may be absent - a .inc that
// sets cameraSpring.amplitude before cameraSpring.use gets nothing, silently.
anim::CameraSpring* getActiveCameraSpring()
{
    anim::AnimationSystemTemplate* t = activeTemplate();
    return t ? t->getCameraSpring() : 0;
}

anim::BundleManager* getActiveBundleManager()
{
    anim::AnimationSystemTemplate* t = activeTemplate();
    return t ? t->getBundleManager() : 0;
}

anim::TriggerManager* getActiveTriggerManager()
{
    anim::AnimationSystemTemplate* t = activeTemplate();
    return t ? t->getTriggerManager() : 0;
}

// Value holders are process-wide, independent of the active animation template.
anim::ValueHolder* getActiveValueHolder()
{
    return anim::valueHolderManager ? anim::valueHolderManager->getActive() : 0;
}

anim::Bundle* getActiveBundle()
{
    anim::BundleManager* m = getActiveBundleManager();
    return m ? m->getActiveBundlePtr() : 0;
}

// AnimationSystem: the manager itself (cc123..cc126, cc150)
void animSysCreate(const std::string& path)      { anim::animationSystemTemplateManager->create(path); }
void animSysSetActive(const std::string& name)   { anim::animationSystemTemplateManager->setActiveSystem(name); }
std::string animSysList()                        { return anim::animationSystemTemplateManager->list(); }
void animSysHide()                               { anim::animationSystemTemplateManager->hide(); }
void animSysSetPlayback(float m)                 { anim::animationSystemTemplateManager->setPlaybackModifier(m); }
bool animSysMgrReady()                           { return anim::animationSystemTemplateManager != 0; }

// AnimationSystem: the active template (cc138..cc140)
void animSysSaveActive()                         { activeTemplate()->saveActive(); }
void animSysSave(const std::string& path)        { activeTemplate()->save(path); }
void animSysReloadScript()                       { activeTemplate()->reloadScript(); }

// Bundle and trigger listings return console strings; the static type list needs no active template.
// The generated console methods return the created object; the shipped executor exposes that
// pointer through the standard pointer formatter.
anim::Bundle* animSysCreateBundle(const std::string& n)   { return getActiveBundleManager()->createBundle(n); }
void animSysDeleteActiveBundle()                 { getActiveBundleManager()->deleteActiveBundle(); }
void animSysSetActiveBundle(const std::string& n){ getActiveBundleManager()->setActiveBundle(n); }
std::string animSysListBundles()                 { return getActiveBundleManager()->listBundles(); }
// two arguments: `animationSystem.createTrigger Trigger root` is type then name, in that order -
// 8,007 shipped lines say so, and the mangling agrees (createTrigger(std::string, std::string)).
anim::Trigger* animSysCreateTrigger(const std::string& type, const std::string& name)
{
    return getActiveTriggerManager()->createTrigger(type, name);
}
void animSysDeleteActiveTrigger()                { getActiveTriggerManager()->deleteActiveTrigger(); }
void animSysSetActiveTrigger(const std::string& n)
{
    getActiveTriggerManager()->setActiveTrigger(n, 0);
}
std::string animSysListTriggers()                { return getActiveTriggerManager()->listTriggers(); }
std::string animSysListTriggerTypes()            { return anim::TriggerManager::listTriggerTypes(); }

// AnimationSystem: value holders (cc154..cc157)
void animSysCreateValueHolder(const std::string& n) { anim::valueHolderManager->createValueHolder(n); }
std::string animSysListValueHolders()               { return anim::valueHolderManager->listValueHolders(); }
void animSysSaveValueHolders()                      { anim::valueHolderManager->saveValueHolders(); }
void animSysSetActiveValueHolder(const std::string& n) { anim::valueHolderManager->setActiveByName(n); }

// AnimationSystem: the camera spring (cc161..cc168)
// cc161 is the ODD one out and it has to be: `use` lives on the template (it builds and frees the
// spring), while the other seven live on the spring itself. That is why a .inc must say
// `cameraSpring.use 1` first - and every shipped one does, on its very first line.
void animSpringUse(bool on)                      { activeTemplate()->setUseCameraSpring(on); }
void animSpringAmplitude(const Vec3& v)          { getActiveCameraSpring()->amplitude = v; }
void animSpringZoomDamping(const Vec3& v)        { getActiveCameraSpring()->zoomDamping = v; }
void animSpringStiffness(const Vec3& v)          { getActiveCameraSpring()->stiffness = v; }
void animSpringMove(const Vec3& v)               { getActiveCameraSpring()->move = v; }
void animSpringHit(const Vec3& v)                { getActiveCameraSpring()->hit = v; }
void animSpringFire(const Vec3& v)               { getActiveCameraSpring()->fire = v; }
void animSpringLook(const Vec2& v)               { getActiveCameraSpring()->look = v; }

// Arguments are min, normal, max; the stored normal value is absolute.
void valuesSet(float mn, float nrm, float mx)
{
    anim::ValueHolder* h = getActiveValueHolder();
    h->min = mn; h->normal = std::fabs(nrm); h->max = mx;
}
void vhSetMin(float v)            { getActiveValueHolder()->min = v; }
void vhSetMax(float v)            { getActiveValueHolder()->max = v; }
// Stores the absolute normal value.
void vhSetNormal(float v)         { getActiveValueHolder()->normal = std::fabs(v); }
void vhSetPassOn(unsigned int v)  { getActiveValueHolder()->passOnMessage = v; }
void vhSetStopOn(unsigned int v)  { getActiveValueHolder()->stopOnMessage = v; }

// AnimationBundle (cc197..cc217)
// the cursor is getActiveBundle(), which `animationSystem.createBundle` sets - which is why a
// .inc reads as a bundle name followed by a run of nameless property lines.
void abSetLength(float v)          { getActiveBundle()->length = v; }
void abSetFadeInTime(float v)      { getActiveBundle()->fadeInTime = v; }
void abSetFadeOutTime(float v)     { getActiveBundle()->fadeOutTime = v; }
void abSetPlayBackward(bool v)     { getActiveBundle()->playBackward = v; }
void abSetJumpToLast(bool v)       { getActiveBundle()->jumpToLastAnimationAtStop = v; }
void abSetPlayForever(bool v)      { getActiveBundle()->playForever = v; }
void abSetIsLooping(bool v)        { getActiveBundle()->isLooping = v; }
void abSetAbruptPlayback(bool v)   { getActiveBundle()->abruptPlayback = v; }
void abSetUseSpeedAsTime(bool v)   { getActiveBundle()->useSpeedAsTime = v; }
std::string abListAnimations()     { return getActiveBundle()->listAnimations(); }
std::string abListEvents()         { return getActiveBundle()->listEvents(); }
// these six all resolve their name through the animation manager, which is unrecovered - so
// they are bound and they reach a ghost. addAnimation is 7,326 shipped lines, the second-busiest
// command in the content set, so binding it is what names the next front rather than hiding it.
void abAddAnimation(const std::string& n)    { getActiveBundle()->addAnimation(n); }
void abRemoveAnimation(const std::string& n) { getActiveBundle()->removeAnimation(n); }
void abSetAnimStart(const std::string& n, float t) { getActiveBundle()->setAnimationStartTime(n, t); }
void abSetAnimLength(const std::string& n, float t) { getActiveBundle()->setAnimationLength(n, t); }
void abAddEvent(const std::string& n)        { getActiveBundle()->addEvent(n); }
void abRemoveEvent(const std::string& n)     { getActiveBundle()->removeEvent(n); }
void abSetEventStart(const std::string& n, float t) { getActiveBundle()->setEventStartTime(n, t); }
// Requires an active bundle. Named-animation getters return -1.0 when the name is absent.
float abGetAnimStart(const std::string& n)  { return getActiveBundle()->getAnimationStartTime(n); }
float abGetAnimLength(const std::string& n) { return getActiveBundle()->getAnimationLength(n); }
float abGetEventStart(const std::string& n) { return getActiveBundle()->getEventStartTime(n); }

// Returns the active trigger of the required type; the base trigger type accepts any subclass.
anim::Trigger* activeTrigger()
{
    anim::TriggerManager* m = getActiveTriggerManager();
    return m ? m->getActiveTriggerPtr(anim::Trigger::TRIGGERTYPE_BASE) : 0;
}
anim::MovementTrigger* activeMovementTrigger()
{
    anim::TriggerManager* m = getActiveTriggerManager();
    return m ? static_cast<anim::MovementTrigger*>(
                   m->getActiveTriggerPtr(anim::Trigger::TRIGGERTYPE_MOVEMENT)) : 0;
}
anim::IdleTrigger* activeIdleTrigger()
{
    anim::TriggerManager* m = getActiveTriggerManager();
    return m ? static_cast<anim::IdleTrigger*>(
                   m->getActiveTriggerPtr(anim::Trigger::TRIGGERTYPE_IDLE)) : 0;
}
anim::MessageTrigger* activeMessageTrigger()
{
    anim::TriggerManager* m = getActiveTriggerManager();
    return m ? static_cast<anim::MessageTrigger*>(
                   m->getActiveTriggerPtr(anim::Trigger::TRIGGERTYPE_MESSAGE)) : 0;
}

// Returns the active trigger's virtual type name.
std::string atGetTypeName()                 { return activeTrigger()->getTypeName(); }

void atAddChild(const std::string& n)       { activeTrigger()->addChild(n); }
void atRemoveChild(const std::string& n)    { activeTrigger()->removeChild(n); }
void atAddBundle(const std::string& n)      { activeTrigger()->addBundle(n); }
void atRemoveBundle(const std::string& n)   { activeTrigger()->removeBundle(n); }
std::string atListBundles()                 { return activeTrigger()->listBundles(); }
void atSetFadeInTime(float v)               { activeTrigger()->fadeInTime = v; }
void atSetMessage(int v)                    { activeMessageTrigger()->m_message = v; }
void atSetValueHolder(const std::string& n) { activeMovementTrigger()->setValueHolder(n); }
void atClearValueHolder()                   { activeMovementTrigger()->clearValueHolder(); }
void atSetUseDirection(bool v)              { activeMovementTrigger()->setUseDirection(v); }
void atSetTriggerOnAccel(bool v)            { activeMovementTrigger()->setTriggerOnAcceleration(v); }

// the Vec2 is two floats on the trigger, not a Vec2 member - and cc238's executor writes a
// third: `idleTime 5/10` stores +0x28, +0x2C and then +0x30 with the same value as +0x28. So setting
// the range also resets the countdown to the minimum. Measured, not assumed.
void atSetIdleTime(const Vec2& v)
{
    anim::IdleTrigger* t = activeIdleTrigger();
    t->m_idleTimeMin  = v.x;
    t->m_idleTimeMax  = v.y;
    t->m_nextIdleTime = v.x;
}
Vec2 atGetIdleTime()
{
    anim::IdleTrigger* t = activeIdleTrigger();
    Vec2 v; v.x = t->m_idleTimeMin; v.y = t->m_idleTimeMax; return v;
}

// Returns the manager's active BoneAnimation for console property access, or null.
anim::BoneAnimation* activeAnimation()
{
    return anim::animationManager ? anim::animationManager->getActiveAnimation() : 0;
}

anim::BoneAnimation* animSysCreateAnimation(const std::string& n) { return anim::animationManager->getAnimation(n); }
std::string animSysListAnimations()               { return anim::animationManager->listAnimations(); }
void animSysSetActiveAnimation(const std::string& n)
{
    anim::animationManager->setActiveAnimationFile(n);
}

void amSetLength(float v)              { activeAnimation()->setLength(v); }
void amSetLooping(bool v)              { activeAnimation()->looping = v; }
// Console accessors for animation fade and jitter settings.
void amSetFadeInTime(float v)              { activeAnimation()->m_blendInTime = v; }       // +0x10
void amSetIgnoreMotherOrientation(int v)   { activeAnimation()->m_baseBone = v; }          // +0x24
void amSetNoiceFreq(int v)                 { activeAnimation()->m_jitterChance = v; }      // +0x44
void amSetNoiceAmplitude(float v)          { activeAnimation()->m_jitterAmplitude = v; }   // +0x48

} // namespace


// Sets the console object for subsequent bindings. [CHOSEN] Access level and category.

BF2_CONSOLE_OBJECT("AnimationSystem", 2, 0)

BF2_CONSOLE_METHOD_1_ACTIVE(create, animSysCreate, animSysMgrReady(), std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(active, std::string, animSysMgrReady(),
                        anim::animationSystemTemplateManager->getActiveSystem(),
                        animSysSetActive, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, animSysList, std::string, animSysMgrReady(), kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(hide, animSysHide, animSysMgrReady(), kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(playbackModifier, float, animSysMgrReady(),
                        anim::animationSystemTemplateManager->getPlaybackModifier(),
                        animSysSetPlayback, kBoneAnimConsoleTag)

BF2_CONSOLE_METHOD_0_ACTIVE(saveActive, animSysSaveActive, activeTemplate() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(save, animSysSave, activeTemplate() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(reloadScript, animSysReloadScript, activeTemplate() != 0, kBoneAnimConsoleTag)

BF2_CONSOLE_METHOD_1_RET_ACTIVE(createBundle, animSysCreateBundle, anim::Bundle*,
                                getActiveBundleManager() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(deleteActiveBundle, animSysDeleteActiveBundle, getActiveBundleManager() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(listBundles, animSysListBundles, std::string, getActiveBundleManager() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(activeBundle, std::string, getActiveBundleManager() != 0,
                        getActiveBundleManager()->getActiveBundle(),
                        animSysSetActiveBundle, kBoneAnimConsoleTag)

BF2_CONSOLE_METHOD_2_RET_ACTIVE(createTrigger, animSysCreateTrigger, anim::Trigger*,
                                getActiveTriggerManager() != 0, std::string, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(deleteActiveTrigger, animSysDeleteActiveTrigger, getActiveTriggerManager() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(listTriggers, animSysListTriggers, std::string, getActiveTriggerManager() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(listTriggerTypes, animSysListTriggerTypes, std::string, true, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(activeTrigger, std::string, getActiveTriggerManager() != 0,
                        getActiveTriggerManager()->getActiveTrigger(),
                        animSysSetActiveTrigger, kBoneAnimConsoleTag)

BF2_CONSOLE_METHOD_1_RET_ACTIVE(createAnimation, animSysCreateAnimation, anim::BoneAnimation*,
                                anim::animationManager != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(listAnimations, animSysListAnimations, std::string, anim::animationManager != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(activeAnimation, std::string, anim::animationManager != 0,
                        anim::animationManager->getActiveAnimationFile(),
                        animSysSetActiveAnimation, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(createValueHolder, animSysCreateValueHolder, anim::valueHolderManager != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(listValueHolders, animSysListValueHolders, std::string, anim::valueHolderManager != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(saveValueHolders, animSysSaveValueHolders, anim::valueHolderManager != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(activeValueHolder, std::string, anim::valueHolderManager != 0,
                        anim::valueHolderManager->getActiveByName(),
                        animSysSetActiveValueHolder, kBoneAnimConsoleTag)

// the dotted names are dice'S, not a nested object: the console class at cc161 stores the literal
// "cameraSpring.use" as its method name. The console splits on the first dot only, so the object is
// AnimationSystem and the method is `cameraSpring.use`, dot and all.
BF2_CONSOLE_PROPERTY_FN(cameraSpring.use, bool, activeTemplate() != 0,
                        activeTemplate()->getUseCameraSpring(), animSpringUse, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSpring.amplitude, Vec3, getActiveCameraSpring() != 0,
                        getActiveCameraSpring()->amplitude, animSpringAmplitude, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSpring.zoomDamping, Vec3, getActiveCameraSpring() != 0,
                        getActiveCameraSpring()->zoomDamping, animSpringZoomDamping, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSpring.stiffness, Vec3, getActiveCameraSpring() != 0,
                        getActiveCameraSpring()->stiffness, animSpringStiffness, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSpring.move, Vec3, getActiveCameraSpring() != 0,
                        getActiveCameraSpring()->move, animSpringMove, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSpring.hit, Vec3, getActiveCameraSpring() != 0,
                        getActiveCameraSpring()->hit, animSpringHit, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSpring.fire, Vec3, getActiveCameraSpring() != 0,
                        getActiveCameraSpring()->fire, animSpringFire, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSpring.look, Vec2, getActiveCameraSpring() != 0,
                        getActiveCameraSpring()->look, animSpringLook, kBoneAnimConsoleTag)


// Console properties for the active value holder.

BF2_CONSOLE_OBJECT("AnimationValueHolder", 2, 0)

BF2_CONSOLE_METHOD_0_RET_ACTIVE(getFilename, anim::ValueHolderManager::getFilename, std::string,
                                anim::valueHolderManager != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_3(values, valuesSet, getActiveValueHolder() != 0, float, float, float, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(min, float, getActiveValueHolder() != 0,
                        getActiveValueHolder()->min, vhSetMin, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(max, float, getActiveValueHolder() != 0,
                        getActiveValueHolder()->max, vhSetMax, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(normal, float, getActiveValueHolder() != 0,
                        getActiveValueHolder()->normal, vhSetNormal, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(passOnMessage, unsigned int, getActiveValueHolder() != 0,
                        getActiveValueHolder()->passOnMessage, vhSetPassOn, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(stopOnMessage, unsigned int, getActiveValueHolder() != 0,
                        getActiveValueHolder()->stopOnMessage, vhSetStopOn, kBoneAnimConsoleTag)


// Animation-bundle properties and animation/event timeline operations.

BF2_CONSOLE_OBJECT("AnimationBundle", 2, 0)

BF2_CONSOLE_PROPERTY_FN(length, float, getActiveBundle() != 0,
                        getActiveBundle()->length, abSetLength, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(fadeInTime, float, getActiveBundle() != 0,
                        getActiveBundle()->fadeInTime, abSetFadeInTime, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(fadeOutTime, float, getActiveBundle() != 0,
                        getActiveBundle()->fadeOutTime, abSetFadeOutTime, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(playBackward, bool, getActiveBundle() != 0,
                        getActiveBundle()->playBackward, abSetPlayBackward, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(jumpToLastAnimationAtStop, bool, getActiveBundle() != 0,
                        getActiveBundle()->jumpToLastAnimationAtStop, abSetJumpToLast, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(playForever, bool, getActiveBundle() != 0,
                        getActiveBundle()->playForever, abSetPlayForever, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(isLooping, bool, getActiveBundle() != 0,
                        getActiveBundle()->isLooping, abSetIsLooping, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(abruptPlayback, bool, getActiveBundle() != 0,
                        getActiveBundle()->abruptPlayback, abSetAbruptPlayback, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(useSpeedAsTime, bool, getActiveBundle() != 0,
                        getActiveBundle()->useSpeedAsTime, abSetUseSpeedAsTime, kBoneAnimConsoleTag)

BF2_CONSOLE_METHOD_0_RET_ACTIVE(listAnimations, abListAnimations, std::string, getActiveBundle() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(listEvents, abListEvents, std::string, getActiveBundle() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(addAnimation, abAddAnimation, getActiveBundle() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(removeAnimation, abRemoveAnimation, getActiveBundle() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(setAnimationStartTime, abSetAnimStart, getActiveBundle() != 0, std::string, float, kBoneAnimConsoleTag)
// Timeline getters take an animation or event name and return its timing value.
BF2_CONSOLE_METHOD_1_RET_ACTIVE(getAnimationStartTime, abGetAnimStart, float, getActiveBundle() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(setAnimationLength, abSetAnimLength, getActiveBundle() != 0, std::string, float, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(getAnimationLength, abGetAnimLength, float, getActiveBundle() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(addEvent, abAddEvent, getActiveBundle() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(removeEvent, abRemoveEvent, getActiveBundle() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(setEventStartTime, abSetEventStart, getActiveBundle() != 0, std::string, float, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(getEventStartTime, abGetEventStart, float, getActiveBundle() != 0, std::string, kBoneAnimConsoleTag)


// Animation-trigger properties, child relationships and value-holder bindings.

BF2_CONSOLE_OBJECT("AnimationTrigger", 2, 0)

// Exposes the active trigger's type name to editor tooling.
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getTypeName, atGetTypeName, std::string, activeTrigger() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(addChild, atAddChild, activeTrigger() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(removeChild, atRemoveChild, activeTrigger() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(addBundle, atAddBundle, activeTrigger() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(removeBundle, atRemoveBundle, activeTrigger() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(listBundles, atListBundles, std::string, activeTrigger() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(fadeInTime, float, activeTrigger() != 0,
                        activeTrigger()->fadeInTime, atSetFadeInTime, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(message, int, activeMessageTrigger() != 0,
                        activeMessageTrigger()->m_message, atSetMessage, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(valueHolder, std::string, activeMovementTrigger() != 0,
                        activeMovementTrigger()->getValueHolder(), atSetValueHolder, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(clearValueHolder, atClearValueHolder, activeMovementTrigger() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(useDirection, bool, activeMovementTrigger() != 0,
                        activeMovementTrigger()->getUseDirection(), atSetUseDirection, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(triggerOnAcceleration, bool, activeMovementTrigger() != 0,
                        activeMovementTrigger()->getTriggerOnAcceleration(), atSetTriggerOnAccel, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(idleTime, Vec2, activeIdleTrigger() != 0,
                        atGetIdleTime(), atSetIdleTime, kBoneAnimConsoleTag)


// Console bindings for the selected bone animation.

BF2_CONSOLE_OBJECT("AnimationManager", 2, 0)

BF2_CONSOLE_PROPERTY_FN(length, float, activeAnimation() != 0,
                        activeAnimation()->length, amSetLength, kBoneAnimConsoleTag)

// Read-only animation property binding. Writes are rejected by the console before execution.
#define BF2_BONEANIM_CONSOLE_READONLY_FN(fnName, T, typeName, activeExpr, getExpr, listTag)        \
namespace {                                                                                        \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                      \
{                                                                                                  \
    BF2_CC_HEAD(fnName, 2, 0, 0, typeName, true)                                                   \
    BF2_CC_TAIL(0, listTag)                                                                        \
    virtual bool isObjectActive() const { return (activeExpr); }                                   \
    virtual void executeObjectMethod()                                                             \
    {                                                                                              \
        if (m_argCount != 0) return;                                                               \
        m_hasReturnValue = true;                                                                   \
        m_returnValue = (getExpr);                                                                 \
    }                                                                                              \
    virtual void setArgFromString(int, const std::string&) { }                                     \
    BF2_CC_RETURN_AS_STRING                                                                        \
    BF2_CC_COMMON_HOOKS                                                                            \
private:                                                                                           \
    T m_returnValue;                                                                               \
};                                                                                                 \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                \
}

BF2_BONEANIM_CONSOLE_READONLY_FN(originalLength, float, "float", activeAnimation() != 0,
                                 activeAnimation()->originalLength, kBoneAnimConsoleTag)
#undef BF2_BONEANIM_CONSOLE_READONLY_FN

BF2_CONSOLE_PROPERTY_FN(looping, bool, activeAnimation() != 0,
                        activeAnimation()->looping, amSetLooping, kBoneAnimConsoleTag)

// Zero arguments read a property; one argument writes it.
BF2_CONSOLE_PROPERTY_FN(fadeInTime, float, activeAnimation() != 0,
                        activeAnimation()->m_blendInTime, amSetFadeInTime, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(ignoreMotherOrientation, int, activeAnimation() != 0,
                        activeAnimation()->m_baseBone, amSetIgnoreMotherOrientation, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(noiceFreq, int, activeAnimation() != 0,
                        activeAnimation()->m_jitterChance, amSetNoiceFreq, kBoneAnimConsoleTag)
BF2_CONSOLE_PROPERTY_FN(noiceAmplitude, float, activeAnimation() != 0,
                        activeAnimation()->m_jitterAmplitude, amSetNoiceAmplitude, kBoneAnimConsoleTag)

// Preserve exact command spelling (including noiceFreq/noiceAmplitude), argument types
// and active-object checks; these are consumed by shipped animation scripts.


// Ragdoll-template console bindings require an active template.

namespace {

inline anim::RagDollTemplate* ragDollTemplate() { return anim::g_ragDollTemplate; }

void rdReset()                        { ragDollTemplate()->m_resetRequested = true; }      // cc96: +0x02 = 1
// Constraint serialization remains a named stop. [TEMP] [CHOSEN, not recovered]
// [RETIRE-WHEN: ragdoll-write-constraints]
void rdSave()
{
    std::fprintf(stderr, "[FRONTIER] dice::anim::RagDollTemplate::writeConstraints reached\n");
    std::fflush(stderr);
    std::abort();
}
void rdActive(bool v)                 { ragDollTemplate()->m_active = v; }                 // cc98:  +0x01
void rdDrawConstraints(bool v)        { ragDollTemplate()->m_drawConstraints = v; }        // cc99:  +0x00
void rdGravity(float v)               { ragDollTemplate()->m_gravity = v; }                // cc100: +0x04
void rdSkeleton(const std::string& n) { ragDollTemplate()->skeleton(n); }                  // cc101
void rdAddParticle(int bone, float m) { ragDollTemplate()->addParticle(bone, m); }         // cc102
void rdAddBone(int a, int b)          { ragDollTemplate()->m_bones.push_back(std::make_pair(a, b)); }   // cc103: +0x20
void rdLockBone(int a, int b)         { ragDollTemplate()->lockBone(a, b); }               // cc104
void rdToSkeleton(int a, int b, int c){ ragDollTemplate()->toSkeleton(a, b, c); }          // cc105
void rdLinkMode(int index, int mode)  { ragDollTemplate()->m_linkModes[index] = mode; }    // cc106: +0x40[index]
void rdAddConstraint(int a, int b)    { ragDollTemplate()->addDistanceConstraint(a, b); }  // cc107
void rdAddCapsuleCollision(int a, int b) { ragDollTemplate()->addCapsuleCollision(a, b); } // cc108
void rdAddAngularConstraint(int a, int b, int c, float deg)
                                      { ragDollTemplate()->addAngularConstraint(a, b, c, deg); }        // cc109
void rdAddDihedralAngleConstraint(int a, int b, int c, int d, float deg)
                                      { ragDollTemplate()->addDihedralAngleConstraint(a, b, c, d, deg); }   // cc110
void rdRemoveConstraint(int a, int b) { ragDollTemplate()->removeConstraint(a, b); }       // cc111
void rdAddForce(int bone, const Vec3& f) { ragDollTemplate()->addForce(bone, f); }         // cc112
void rdSetParticleCollisionCriteria(float a, float b)
                                      { ragDollTemplate()->setParticleCollisionCriteria(a, b); }        // cc113
void rdAddJointConstraint(int a, int b) { ragDollTemplate()->addJointConstraint(a, b); }   // cc115
void rdAddDistanceLessThanConstraint(int a, int b, float d)
                                      { ragDollTemplate()->addDistanceLessThanConstraint(a, b, d); }    // cc117

} // namespace

BF2_CONSOLE_OBJECT("RagDoll", 2, 0)

BF2_CONSOLE_METHOD_0_ACTIVE(reset, rdReset, ragDollTemplate() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(save, rdSave, ragDollTemplate() != 0, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(active, rdActive, ragDollTemplate() != 0, bool, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(drawConstraints, rdDrawConstraints, ragDollTemplate() != 0, bool, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(gravity, rdGravity, ragDollTemplate() != 0, float, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(skeleton, rdSkeleton, ragDollTemplate() != 0, std::string, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(addParticle, rdAddParticle, ragDollTemplate() != 0, int, float, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(addBone, rdAddBone, ragDollTemplate() != 0, int, int, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(lockBone, rdLockBone, ragDollTemplate() != 0, int, int, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_3(toSkeleton, rdToSkeleton, ragDollTemplate() != 0, int, int, int, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(linkMode, rdLinkMode, ragDollTemplate() != 0, int, int, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(addConstraint, rdAddConstraint, ragDollTemplate() != 0, int, int, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(addCapsuleCollision, rdAddCapsuleCollision, ragDollTemplate() != 0, int, int, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_4(addAngularConstraint, rdAddAngularConstraint, ragDollTemplate() != 0, int, int, int, float, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_5(addDihedralAngleConstraint, rdAddDihedralAngleConstraint, ragDollTemplate() != 0, int, int, int, int, float, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(removeConstraint, rdRemoveConstraint, ragDollTemplate() != 0, int, int, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(addForce, rdAddForce, ragDollTemplate() != 0, int, Vec3, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(setParticleCollisionCriteria, rdSetParticleCollisionCriteria, ragDollTemplate() != 0, float, float, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(addJointConstraint, rdAddJointConstraint, ragDollTemplate() != 0, int, int, kBoneAnimConsoleTag)
BF2_CONSOLE_METHOD_3(addDistanceLessThanConstraint, rdAddDistanceLessThanConstraint, ragDollTemplate() != 0, int, int, float, kBoneAnimConsoleTag)

// Returns the console list registered by AnimModule.
ConsoleObjectList& boneAnimationConsoleObjects()
{
    return consoleBindingList(kBoneAnimConsoleTag);
}

} // namespace animationTools
} // namespace hfe
} // namespace dice
