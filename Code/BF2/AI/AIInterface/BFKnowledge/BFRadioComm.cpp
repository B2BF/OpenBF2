// Placement attested by the ELF's "AI/AIInterface/BFKnowledge/BFRadioComm.cpp" __FILE__ string.

#include "BFRadioComm.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Core/Debug.h"
#include "../../../Core/EventManager.h"
#include "../../../Game/IEventListener.h"                    // ECNetwork
#include "../../../Game/GameEvents/RadioMessageEvent.h"      // RadioMessageReceivedEvent

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// First wins in, exact match out.
IBFRadioComm::IBFRadioComm()
{
    if (!CommonSingletonServer::m_iBFRadioComm)
        classManager->registerSimpleSingleton(std::string("IBFRadioComm"), this);
}

IBFRadioComm::~IBFRadioComm()
{
    if (CommonSingletonServer::m_iBFRadioComm == this)
        classManager->unregisterSimpleSingleton(std::string("IBFRadioComm"));
}

BFRadioComm::BFRadioComm()
{
}

BFRadioComm::~BFRadioComm()
{
}

// bf2 0x840B900: the AI's only way to speak. The AI channel is mapped to the engine channel
// 0..8 -> 6, 7, 8, 3, 2, 4, 10, 11, 14 (an unknown one reports an Error and becomes 19), and a
// RadioMessageReceivedEvent {connection = playerId, message = a, sender = b, team = c, flags = d,
// that channel, type = channel} is posted as ECNetwork event at time 0 (EventManager +0x10).
// The allocation goes through checkPointer (its "AI/AIDLL/AI/AIUtils/checkPointer.h" Debug block
// is inlined here). [INFERRED] int: the value is the postEvent result left in eax; the only
// caller (CommunicationHub::filterAndSendAIRadioMessage) discards it.
int BFRadioComm::sendAIRadioMessage(int playerId, ChannelMessageType channel, AIRadioChannelType type, unsigned char a, int b, unsigned char c, unsigned char d)
{
    int engineChannel;
    switch (static_cast<int>(type))
    {
    case 0: engineChannel = 6; break;
    case 1: engineChannel = 7; break;
    case 2: engineChannel = 8; break;
    case 3: engineChannel = 3; break;
    case 4: engineChannel = 2; break;
    case 5: engineChannel = 4; break;
    case 6: engineChannel = 10; break;
    case 7: engineChannel = 11; break;
    case 8: engineChannel = 14; break;
    default:
        engineChannel = 19;
        Debug(DebugTypeError, __FILE__, 77, "") << "Unknown AI radio channel number "
                                                  << static_cast<int>(type);   // line 0x4D
        break;
    }
    EventNode* const event = checkPointer(new RadioMessageReceivedEvent(
        playerId, a, static_cast<unsigned char>(b), c, d, engineChannel, static_cast<int>(channel)));
    return static_cast<int>(eventManager->postEvent(ECNetwork, event, 0.0f));
}

} // namespace ai
} // namespace hfe
} // namespace dice
