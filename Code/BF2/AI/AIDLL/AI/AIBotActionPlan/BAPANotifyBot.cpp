// Adds a plan-variable amount to a sensed object's notification word for type zero.

// static init: ELF 0x1DD970 (__tcf_0 0x1DD9D0, _GLOBAL__I 0x1DDA00): <iostream>'s Init object only.
#include <iostream>

#include "BAPANotifyBot.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DD6D0 (C1; C2 0x1DD670)
BAPANotifyBot::BAPANotifyBot(Type type, BAPVariable<int>* amount, BAPVariable<unsigned int>* object)
    : BAPAction(UninitializedPatternId()), m_type(type), m_amount(amount), m_object(object)
{
}

// ELF 0x1DD770 (D1; D2 0x1DD730, D0 0x1DD7B0)
BAPANotifyBot::~BAPANotifyBot()
{
}

// ELF 0x1DD800
bool BAPANotifyBot::execute(Bot* bot, bool& success, bool& finished)
{
    if (m_type == 0) {
        SensedObject* object = bot->getSensedObjectFromHandle(m_object->m_value);
        if (object)
            object->m_unresolved44 += m_amount->m_value;
    } else {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBotActionPlan/BAPANotifyBot.cpp", 45, "") << "Unkown type";
    }
    success = true;
    finished = true;
    return true;
}

// ELF 0x1DD950
BAPStatementType BAPANotifyBot::getId() const
{
    return BAPSTNotifyBot;
}

// ELF 0x1DD960
bool BAPANotifyBot::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
