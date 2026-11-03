// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPASENDRADIOMESSAGE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPASENDRADIOMESSAGE_H

#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B8A40 (8 words), operator new 0x28, package F-infantry.
// Queues one radio message on the bot's CommunicationAgent (enqueueRadioMessage).
class BAPASendRadioMessage : public BAPAction
{
public:
    BAPASendRadioMessage(AIRadioMessageType, AIRadioChannelType, unsigned int, float, int, int);   // 0x1DFCF0
    virtual ~BAPASendRadioMessage();   // 0x1DFDA0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1DFE30, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1DFF60, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1DFEA0, slot +0x14; from the base declaration

private:
    /* +0x14 */ AIRadioMessageType m_type;
    /* +0x18 */ AIRadioChannelType m_channel;
    /* +0x1C */ unsigned int m_priority;
    /* +0x20 */ int m_target;          // the constructor's last int, enqueueRadioMessage's last argument
    /* +0x24 */ float m_delay;         // [INFERRED] name: enqueueRadioMessage's float
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPASENDRADIOMESSAGE_H
