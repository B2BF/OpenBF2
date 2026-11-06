// [INFERRED] beside the attested Trigger.cpp. Manager-facing runtime declarations.
// ELF constructors 81C4E60/81C68A0 and dedicated PE 545E20/544270 establish
// the bases and member sequence. Offsets below are ELF evidence, not native offsets.
// Construction, own virtual surfaces and motion recovered from both dedicated images.
#ifndef GAME_OBJECTS_GAME_TRIGGERABLE_H
#define GAME_OBJECTS_GAME_TRIGGERABLE_H

#include "../Base/Bundle.h"
#include "../../../Sound/ISoundNode.h"
#include <string>
#include <vector>

namespace dice { namespace hfe {
class ISound;
class TriggerableTargetNetworkable;
namespace ai { class BAPConTriggerableActive; class BBPTriggerable; }
namespace world {
class TriggerableTemplate;
class TriggerableTargetTemplate;
class TriggerableManager;

class Triggerable : public Bundle, public ISoundNode
{
public:
    explicit Triggerable(const TriggerableTemplate*);
    virtual ~Triggerable();
    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int) const;
    virtual void init();
    virtual void destroy(bool);
    virtual void handleMessage(TemplateMessage, IPlayer*, bool);
    virtual const Mat4& getTransform();             // ELF primary +180
    virtual const Math::Vec3* getVelocity();         // ELF primary +184
    virtual int getTargetMovementType();            // ELF +188: signed -1 or template dword

    int getTriggerId() const;
    int getTypes() const;
    float getRadius() const;
    bool checkTriggerableArea();                    // ELF81C5270
    bool isOccupied();                              // ELF81C54E0
    bool isInArea(const Vec3&);                      // ELF81C5620, strict radius comparison
    void updateOnTime(float);
    bool handleTrigger(float);                      // ELF81C57A0, caller tests AL
    void resetTriggerTargets();                     // ELF81C58A0
    void refresh();                                 // ELF81C5980
    bool allTargetsStopped();
    void playActivateSound();
    void playDeActivateSound();

private:
    friend class TriggerableManager; // Retail reads the cached template and toggles +16C directly.
    friend class TriggerableTarget;
    // [INFERRED] friendship: the AI condition reads +16C inline (libaidll ELF 0x1EAF50).
    friend class ai::BAPConTriggerableActive;
    // [INFERRED] friendship: the pattern reads the cached template inline (libaidll ELF 0x2A6F56).
    friend class ai::BBPTriggerable;
    int m_refCount;                                 // +140
    const TriggerableTemplate* m_template;           // +144
    int m_id;                                       // +148
    float m_float14C;                               // [UNRESOLVED name], ctor 5
    unsigned int m_unresolved150[2];                 // +150/+154 not initialized by either ctor
    std::vector<IObject*> m_objectsInArea;            // +158, grid-query storage
    ISound* m_startupSound;                          // +164
    ISound* m_finishedSound;                         // +168
    bool m_bool16C;                                 // [UNRESOLVED name], ctor false
};

class TriggerableTarget : public Bundle, public ISoundNode
{
public:
    explicit TriggerableTarget(const TriggerableTargetTemplate*);
    virtual ~TriggerableTarget();
    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int) const;
    virtual void init();
    virtual void destroy(bool);
    virtual void setName(const std::string&);
    virtual bool handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&,
                                 int, int, CollisionFaceInfo*, CollisionFaceInfo*);
    virtual void handleMessage(TemplateMessage, IPlayer*, bool);
    virtual void initNetworkableComp();
    virtual const Mat4& getTransform();             // ELF primary +180
    virtual const Math::Vec3* getVelocity();         // ELF primary +184
    std::string getName(); // NON-const, NON-virtual, BY VALUE; ELF81C7F00 / PE545060 inline copy.
    void refresh();
    void handleUpdate(IObject*, Triggerable*, float); // NON-virtual overload, ELF81C70E0 / PE544B20
    void activate();
    void deactivate();
    Vec3 getFinishPoint();                           // by value, ELF81C7C80 hidden result
    void setPos(Vec3);
    void setRot(Vec3);

private:
    friend class Triggerable;
    friend class dice::hfe::TriggerableTargetNetworkable;
    int m_refCount;                                 // +140
    const TriggerableTargetTemplate* m_template;     // +144
    std::string m_name;                             // +148
    int m_state;                                    // +14C; motion state 0..6
    int m_reverse;                                  // +150; toggles 0/1 on each traversal
    int m_cyclesRemaining;                          // +154; decremented on arrival
    float m_delay;                                  // +158; motion-phase timer
    Vec3 m_vector15C;                               // [UNRESOLVED name], ctor 0/0/0
    float m_float168;                               // [UNRESOLVED name], ctor 0
    Vec3 m_current;                                 // +16C [INFERRED motion names]
    Vec3 m_finish;                                  // +178
    Vec3 m_startPoint;                              // +184
    Vec3 m_endPoint;                                // +190
    Vec3 m_direction;                               // +19C
    Vec3 m_savedPosition;                           // +1A8
    Vec3 m_savedSecondaryPosition;                  // +1B4
    Vec3 m_savedRotation;                           // +1C0
    Vec3 m_savedSecondaryRotation;                  // +1CC
    ISound* m_startupSound;                          // +1D8
    ISound* m_movingSound;                           // +1DC
    ISound* m_finishedSound;                         // +1E0
    bool m_initialized;                             // +1E4; original transforms captured
};
} } }
#endif
