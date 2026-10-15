// Shared manager contract for decal clients, templates and frame updates.
// The decals-enabled flag (retail +0x2A6) is set by VideoOptions through DecalManager::setEnabledField.
#ifndef BF2_GEOM_DECALSYSTEM_IDECALMANAGER_H
#define BF2_GEOM_DECALSYSTEM_IDECALMANAGER_H

#include "../../Core/IBase.h"
#include "IDecalTemplate.h"

namespace dice {
namespace hfe {
namespace geom {

class IDecalManager : public IBase
{
public:
    // +0x00 addRef 0x10132630, +0x04 getRef 0x10051710, +0x08 release 0x10126F00, +0x0C
    // queryInterface 0x10126EB0 (iid 1 and 820018) - IBase's four.
    virtual void  reserved10() = 0;                                    // +0x10 101240F0 `retn`
    // +0x14 10124610 (retn 4). The decal owner: BF2.exe passes 0 for the ground decals (GameLogic's
    // constructor, 0x4F6FA1) and the SimpleObject itself for object decals (0x627CB3). Answers the
    // new DecalObject (0x44 bytes, table DecalObjectAccess.h), or 0 while decals are
    // disabled (+0x2A6).
    virtual void* createClient(void* owner) = 0;
    // [INFERRED] Queues a released decal for deletion by update().
    virtual void  enqueue(void* decalObject) = 0;
    // +0x1C 10124BE0 (retn 4). The template by name - created on first use, "To many decalManager
    // templates" past the limit. world::DecalTemplate::templateInitialize stores the answer and then
    // calls its +0x04.
    virtual IDecalTemplate* addTemplate(const DecalTemplateString& name) = 0;
    virtual void  reserved20(void* argument) = 0;                      // +0x20 10124100 `retn 4`
    // +0x24 10124670 (retn 4) - the DecalObject joins the active set (its +0x04 set); its first decal
    // calls it. [INFERRED] name.
    virtual void  addInstance(void* decalObject) = 0;
    virtual void  removeInstance(void* decalObject) = 0;               // +0x28 10124820 (retn 4)
    virtual void  resetPool() = 0;                                     // +0x2C 101242F0 (retn)
    virtual void  update(float deltaTime) = 0;                         // +0x30 101248E0 (retn 4)
    // +0x34..+0x50: three setters that return nothing and four getters that return a float in
    // st(0) - see DecalManager.cpp (RW-153: a value-returning setter leaks an x87 register).
    // The decals' time to live in seconds: DecalObject::add stamps each record with it and update
    // retires a ring decal whose age reaches it.
    virtual void  setTimeToLive(float seconds) = 0;                    // +0x34 / +0x10
    virtual float getTimeToLive() const = 0;                           // +0x38
    virtual void  setFadeDistance(float distance) = 0;                 // +0x3C / +0x2B0
    virtual float getFadeDistance() const = 0;                         // +0x40
    // +0x44 10124280 (retn 4) - the console's `startFadePercentageOfFadeDistancel` [sic] setter.
    // The body never stores its argument (it only re-derives the fades) - shipped.
    virtual void  setStartFadePercentageOfFadeDistance(float percentage) = 0;
    virtual float getStartFadePercentageOfFadeDistance() const = 0;    // +0x48 / +0x2BC
    virtual void  setFadeOutAtPercentageOfTimeToLive(float percentage) = 0; // +0x4C / +0x1C
    virtual float getFadeOutAtPercentageOfTimeToLive() const = 0;      // +0x50
    virtual void  reserved54() = 0;                                    // +0x54 10124150 `retn`

protected:
    ~IDecalManager() {}
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif
