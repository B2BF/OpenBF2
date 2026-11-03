// Condition: the object is man down. Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols);
// every body carries its ELF address.

#include "BAPConObjectManDown.h"

#include "BAPActionInline.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EFA20 (C1; C2 0x1EF9F0): +0x04 = 0 by the base, +0x08 = the object handle.
BAPConObjectManDown::BAPConObjectManDown(unsigned int object)
{
    m_object = object;
}

// ELF 0x1EFA70 (D1; D2 0x1EFA50, D0 0x1EFA90): base only.
BAPConObjectManDown::~BAPConObjectManDown()
{
}

// ELF 0x1EFAD0: the object's physical plug-in getManDown (+0x5C); false without information.
bool BAPConObjectManDown::evaluate(Bot*)
{
    Information* info = bapinline::info(m_object);
    bool result = false;
    if (info)
        result = info->getPhysical()->getManDown();
    return result;
}

// ELF 0x1EFB40
BAPConditionType BAPConObjectManDown::getConditionType() const
{
    return BAPCTObjectManDown;
}

} // namespace ai
} // namespace hfe
} // namespace dice
