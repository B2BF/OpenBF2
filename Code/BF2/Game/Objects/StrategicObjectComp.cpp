// Strategic-object components and groups of destructible pieces.

#include "StrategicObjectComp.h"

#include "IObject.h"
#include "StrategicObjectCompTemplate.h"
#include "../IGameLogic.h"
#include "../../Core/Ids.h"

namespace dice { namespace hfe { extern IGameLogic* gameLogic; } }

namespace dice {
namespace hfe {
namespace world {

StrategicObjectComp::StrategicObjectComp(StrategicObjectCompTemplate* componentTemplate)
    : m_refCount(1), m_template(componentTemplate), m_object(0), m_index(0), m_holder(0)
{
}
StrategicObjectComp::~StrategicObjectComp() {} // vptr restore only; destroy() is separate
void StrategicObjectComp::addRef() { ++m_refCount; }
int StrategicObjectComp::getRef() { return m_refCount; }
int StrategicObjectComp::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        delete this;
    return remaining;
}
void* StrategicObjectComp::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectComponent || iid == IID_IStrategicObjectComp)
        return const_cast<StrategicObjectComp*>(this);
    return 0;
}
unsigned int StrategicObjectComp::getInterfaceID() const { return IID_IStrategicObjectComp; }
void StrategicObjectComp::init(IObject* object)
{
    m_object = object;
    gameLogic->addStrategicObject(object); // no singleton guard in either port
}
void StrategicObjectComp::destroy() { gameLogic->removeStrategicObject(m_object); }
void StrategicObjectComp::setObject(IObject* object) { m_object = object; }
IObject* StrategicObjectComp::getObject() const { return m_object; }
bool StrategicObjectComp::hasUpdate() const { return false; }
void StrategicObjectComp::handleUpdate(float) {}
const std::string& StrategicObjectComp::getStrategicObjectName() { return m_template->getStrategicObjectName(); }
const std::string& StrategicObjectComp::getIntactIconName() { return m_template->getIntactIcon(); }
const std::string& StrategicObjectComp::getDestroyedIconName() { return m_template->getDestroyedIcon(); }
std::intptr_t StrategicObjectComp::getIntactIconPtr() { return m_template->getIntactIconPtr(); }
std::intptr_t StrategicObjectComp::getDestroyedIconPtr() { return m_template->getDestroyedIconPtr(); }
int StrategicObjectComp::getPart() { return m_template->getPart(); }
int StrategicObjectComp::getIndex() { return m_index; }
void StrategicObjectComp::setIndex(int index) { m_index = index; }
void* StrategicObjectComp::getHudInfo() { return &m_template->m_strategicObjectName; }
void* StrategicObjectComp::getHudIcons() { return &m_template->m_destroyedIcon; }
bool StrategicObjectComp::has3dMapIcon() const { return m_template->has3dMapIcon(); }
int StrategicObjectComp::get3dMapIcon() const { return m_template->get3dMapIcon(); }
bool StrategicObjectComp::isOnePiece() const { return m_template->getIsOnePiece(); }
bool StrategicObjectComp::isPartOfDestroyedHolder() const
{
    return m_holder && m_holder->getNrDestroyedObjects() > 0;
}
const Vec3& StrategicObjectComp::getHolderPosition() const
{
    if (!m_holder)
        return Vec3::getZero();
    // Both ports share this one mutable result across all component instances.
    static Vec3 position = {0.0f, 0.0f, 0.0f};
    position = m_holder->getPosition();
    return position;
}
#if defined(_WIN64) || defined(__LP64__)
// [CHOSEN, not recovered] Native vptr/template/object/holder addresses, unchanged int refcount/index.
// GameLogic and this component reach the holder through typed members, never retail byte offsets.
static_assert(sizeof(StrategicObjectComp) == 48, "native x64 strategic component");
#else
static_assert(sizeof(StrategicObjectComp) == 24, "retail x86 strategic component");
#endif

// Initializes an empty holder; addStrategicObject assigns its index after the first insertion.
StrategicObjectHolder::StrategicObjectHolder()
    : m_nrObjects(0)
    , m_nrDestroyedObjects(0)
    , m_float18(0.0f)
    , m_float1C(0.0f)
    , m_float20(0.0f)
{
}

void StrategicObjectHolder::addObject(IObject* object)
{
    m_objects.push_back(object);
    ++m_nrObjects;
}

// . First match only, and it leaves a hole rather than erasing.
void StrategicObjectHolder::removeObject(IObject* object)
{
    for (unsigned int i = 0; i < m_objects.size(); ++i)
    {
        if (m_objects[i] == object)
        {
            m_objects[i] = 0;
            --m_nrObjects;
            return;
        }
    }
}

IObject* StrategicObjectHolder::getObjectByIndex(unsigned int index) const
{
    if (index < m_objects.size())
        return m_objects[index];
    return 0;
}

// Returns the mean position of live pieces, skipping holes; returns zero when none remain.
Vec3 StrategicObjectHolder::getPosition() const
{
    Vec3 result;
    result.x = 0.0f;
    result.y = 0.0f;
    result.z = 0.0f;

    if (m_nrObjects <= 0)
        return result;

    for (unsigned int i = 0; i < m_objects.size(); ++i)
    {
        IObject* object = m_objects[i];
        if (!object)
            continue;                                  // a hole removeObject left behind

        const Vec3& pos = object->getAbsolutePosition();   // slot 27 +0x6C
        result.x += pos.x;
        result.y += pos.y;
        result.z += pos.z;
    }

    const float inv = 1.0f / static_cast<float>(m_nrObjects);
    result.x *= inv;
    result.y *= inv;
    result.z *= inv;
    return result;
}

} // namespace world
} // namespace hfe
} // namespace dice
