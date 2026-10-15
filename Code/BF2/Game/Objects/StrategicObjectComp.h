// Groups world objects into one strategic landmark, counts destroyed pieces and averages their map position.
// [INFERRED] Header placement.

#ifndef GAME_OBJECTS_STRATEGICOBJECTCOMP_H     // [INFERRED] guard style
#define GAME_OBJECTS_STRATEGICOBJECTCOMP_H

#include <vector>

#include "../../Math/Vec3.h"
#include "IStrategicObjectComp.h"

namespace dice {
namespace hfe {
class GameLogic;
namespace geom { class NametagManager; }   // RendDX9's - a friend below (RW-266, as Soldier.h's)
namespace world {

class IObject;
class StrategicObjectCompTemplate;
class StrategicObjectHolder;

// ELF 8228800 / dedicated PE 58B7B0: 24 bytes in both ports, nonowning template/object/holder.
class StrategicObjectComp : public IStrategicObjectComp
{
public:
    explicit StrategicObjectComp(StrategicObjectCompTemplate*);
    void addRef() override;
    int getRef() override;
    int release() override;
    void* queryInterface(unsigned int) const override;
    unsigned int getInterfaceID() const override;
    void init(IObject*) override;
    void destroy() override;
    void setObject(IObject*) override;
    IObject* getObject() const override;
    bool hasUpdate() const override;
    void handleUpdate(float) override;
    const std::string& getStrategicObjectName() override;
    const std::string& getIntactIconName() override;
    const std::string& getDestroyedIconName() override;
    std::intptr_t getIntactIconPtr() override;
    std::intptr_t getDestroyedIconPtr() override;
    int getPart() override;
    int getIndex() override;
    void setIndex(int) override;
    void* getHudInfo() override;
    void* getHudIcons() override;
    bool has3dMapIcon() const override;
    int get3dMapIcon() const override;
    bool isOnePiece() const override;
    bool isPartOfDestroyedHolder() const override;
    const Vec3& getHolderPosition() const override;
    virtual ~StrategicObjectComp(); // PE +74; ELF +74/+78, after the interface slots

private:
    friend class ::dice::hfe::GameLogic;
    // RendDX9's nametag pass reads m_holder at its fixed +0x14 (retail 0x10130CA1, addTouchItem);
    // the native renderer reads it typed through this line (RW-266). No layout or table change.
    friend class ::dice::hfe::geom::NametagManager;
    int m_refCount;                             // +04
    StrategicObjectCompTemplate* m_template;   // +08
    IObject* m_object;                          // +0C
    int m_index;                               // +10
    StrategicObjectHolder* m_holder;            // +14
};

class StrategicObjectHolder
{
public:
    // Initializes an empty holder; the caller assigns its index after adding the first object.
    StrategicObjectHolder();

    // . push_back + bump the live count. The inlined push_back carries a null test
    // on _M_finish before the store - the compiler's own check inside construct(), dead for a vector
    // that has any capacity at all - so a plain push_back reproduces it exactly.
    void addObject(IObject* object);

    // Replaces the first matching pointer with null and decrements the live count without shrinking.
    void removeObject(IObject* object);

    // . Bounds-checked against the vector's size, not the live count, and returns
    // null both for an out-of-range index and for a hole.
    IObject* getObjectByIndex(unsigned int index) const;

    // Computes the mean of live pieces on each call; returns zero when the live count is nonpositive.
    Vec3 getPosition() const;

    // [CHOSEN] names. No symbol exists for either - dice reaches +0x10 with a raw store from
    // ArmorGLComp and a raw read from StrategicObjectComp::isPartOfDestroyedHolder, so whatever
    // fronted it was inline in this header and its name did not survive. The field is attested by
    // both of those bodies plus ArmorGLComp::updateStrategicObject's ++ / --.
    int  getNrDestroyedObjects() const { return m_nrDestroyedObjects; }
    void setNrDestroyedObjects(int n)  { m_nrDestroyedObjects = n; }

    int  getIndex() const { return m_index; }        // [CHOSEN] name, same argument
    void setIndex(int i)  { m_index = i; }

private:
    friend class ::dice::hfe::GameLogic;             // recovered direct live-count read / index store
    /* +0x00 */ std::vector<IObject*> m_objects;   // holes and all
    /* +0x0C */ int m_nrObjects;                   // live entries, not m_objects.size()
    // Destroyed-piece count maintained by ArmorGLComp; values are not clamped.
    /* +0x10 */ int m_nrDestroyedObjects;
    // The holder's own index within its name group - the same value the component carries, and the
    // digit the map key ends in. GameLogic::addStrategicObject assigns both from its search loop.
    /* +0x14 */ int m_index;                       // [CHOSEN] name
    // [UNRESOLVED] - three dwords zeroed at construction and touched by none of the four bodies
    // above. Their shape (three consecutive, zero-initialised, at the tail) reads like a Vec3, but
    // no reader has been found and getPosition demonstrably does not use them.
    /* +0x18 */ float m_float18;
    /* +0x1C */ float m_float1C;
    /* +0x20 */ float m_float20;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_STRATEGICOBJECTCOMP_H
