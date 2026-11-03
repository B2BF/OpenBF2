// Area condition. Evidence: libaidll.so (ia-32) 0x3322B0..0x332770.

#include "SCArea.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF 0x3326E0,
// __tcf_0 0x332740, _GLOBAL__I 0x332770).
#include <iostream>

#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"

namespace dice {
namespace hfe {
namespace ai {

SCArea::SCArea(const std::string& name, const std::string& areaName, int object,
               StatusMap::Status status)
    : StrategyCondition(name, 0.0f, static_cast<Compare>(9), Friendly, Crisp,
                        static_cast<Conditional>(44)),
      m_object(object),
      m_areaName(areaName),
      m_status(status)
{
}

SCArea::~SCArea()
{
}

// ELF 0x3325B0: 1 when the object is an area (getType 0) whose status for this side is the
// wanted one, -1 otherwise. The object is not null-checked.
float SCArea::compare(float*, float*, int side) const
{
    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getStrategicObject(m_object);
    if (object->getType() != 0)
        return -1.0f;
    return object->getStatus(side) == m_status ? 1.0f : -1.0f;
}

// ELF 0x332630: resolves the area by name through the manager's active-area cursor.
int SCArea::updateArea()
{
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    manager->setActive(m_areaName);
    AIStrategicArea* const area = manager->getActiveArea();
    if (area)
        m_object = area->getIndex();
    else
        m_object = -1;
    return m_object;
}

} // namespace ai
} // namespace hfe
} // namespace dice
