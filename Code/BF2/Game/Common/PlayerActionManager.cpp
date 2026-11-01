// Player action buffering and network transmission.
#include "PlayerActionManager.h"
#include "ClientConnection.h"                 // PacketStatus's typed stream-manager mask

#include "../../Core/Ids.h"                      // IID_IBase, IID_IStreamManager
#include "../../Core/Debug.h"
#include "../../Core/StringUtil.h"
#include "../../IO/BitStream.h"
#include "../GameLogic/ActionBuffer.h"           // pushBack - where received input lands
#include "../Objects/IPlayer.h"

#include <cstring>                               // memmove - add()'s slide

namespace dice {
namespace hfe {

// Seeds action indices and manager id with -1 sentinels. Builds neutral send actions
// through PlayerAction::set, not a byte fill. Inherited counters, connection id, and
// interpolation time remain uninitialized until their owners set them.
PlayerActionManager::PlayerActionManager()
    : m_refCount(1)                                      // +0x00C
    , m_error(0)                                         // +0x014
    , m_sendActionCount(0)                               // +0x06C
    , m_dword70(0)                                       // +0x070
    , m_streamManagerId(-1)                              // +0x074  the write-once sentinel
    , m_recvActionHighWater(-1)                          // +0x078  index 0 must be acceptable
    , m_byte7C(false)                                    // +0x07C
    , m_player(0)                                        // +0x084
{
    // through set(), NOT memset - see above.
    io::PlayerInput zeroInput;
    std::memset(&zeroInput, 0, sizeof(zeroInput));

    PlayerAction zeroAction;
    // set() does not write m_inputIndex, so the index is cleared first. transmit sends slot 0's
    // index as the packet's base index while add() is still sliding these placeholders out.
    zeroAction.m_inputIndex = 0;
    zeroAction.set(zeroInput);

    for (int i = 0; i < 3; ++i)
        m_sendActions[i] = zeroAction;
}

// ~PlayerActionManager - ELF D1 0x809D2C0, 14 bytes. It re-seats its vptr and does nothing else:
// every member is a scalar, a pointer it does not own, or a PlayerAction by value.
PlayerActionManager::~PlayerActionManager()
{
}

// IBase and IStreamManager queries return the same unadjusted interface pointer.
void PlayerActionManager::addRef()  { ++m_refCount; }
int  PlayerActionManager::getRef()  { return m_refCount; }

int PlayerActionManager::release()
{
    if (--m_refCount == 0)
    {
        delete this;
        return 0;
    }
    return m_refCount;
}

void* PlayerActionManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<PlayerActionManager*>(this);
    if (iid == IID_IStreamManager)
        return const_cast<PlayerActionManager*>(this);
    return 0;
}

// The IStreamManager accessors. Each is its own tiny function and each pins one offset.
// setStreamManagerId IS write-once - the -1 test is the whole reason it is 24 bytes rather than 14.
// A manager registered twice keeps the id it was given first, which is what freezes
// ClientConnection's playerAction/gameEvent/ghost registration order onto the wire.
int  PlayerActionManager::getStreamManagerId() const { return m_streamManagerId; }   // +0x074
int  PlayerActionManager::getConnectionId()          { return m_connectionId; }      // +0x010
int  PlayerActionManager::getError()                 { return m_error; }             // +0x014
void PlayerActionManager::setConnectionId(int id)    { m_connectionId = id; }        // +0x010

void PlayerActionManager::setStreamManagerId(int id)
{
    if (m_streamManagerId == -1)                         // write-once
        m_streamManagerId = id;
}

// setPlayer - ELF 0x809D2F0, 17 bytes. One store. It does NOT clear the received-action high
// water mark, so a connection that changes player keeps the old input-index floor.
void PlayerActionManager::setPlayer(world::IPlayer* player)
{
    m_player = player;
}

// Intentionally ignores packet acknowledgments. Repeating the last three actions
// in each packet supplies reliability without an acknowledgment-driven resend queue.
void PlayerActionManager::handlePacketStatus(PacketStatus&, bool&)
{
}

// Appends to the three-action resend window, sliding existing entries even before
// the window fills. Always returns true; the Boolean return type is [CHOSEN].
bool PlayerActionManager::add(const PlayerAction& action)
{
    if (++m_sendActionCount > 3)
        m_sendActionCount = 3;

    std::memmove(&m_sendActions[0], &m_sendActions[1], 2 * sizeof(PlayerAction));   // 56 bytes
    m_sendActions[m_sendActionCount - 1] = action;
    return true;
}

// Decodes player actions and reconstructs consecutive input indices from one signed base.
// An absent block succeeds. An action decode failure sets m_error = 2 and returns false.
// The four-bit count is not checked against the six-entry receive array; valid callers
// must respect that capacity. The sign-and-magnitude base cannot represent INT_MIN.
bool PlayerActionManager::processReceivedPacket(io::BitStream* stream)
{
    const unsigned int bitsBefore = stream->getBitPosition();

    unsigned int present = 0;
    stream->readBits(&present, 1);
    if (present != 1)
        return true;                                     // nothing here, and that is not an error

    unsigned int count = 0;
    stream->readBits(&count, 4);                         // 0..15 against a 6-slot array

    unsigned int interpolationTime = 0;
    stream->readBits(&interpolationTime, 9);
    m_playerInterpolationTime = static_cast<int>(interpolationTime);   // +0x080
    if (m_player)
        m_player->setInterpolationTime(static_cast<int>(interpolationTime));

    int baseIndex = 0;
    if (count > 0)
    {
        unsigned int sign = 0;
        stream->readBits(&sign, 1);
        unsigned int magnitude = 0;
        stream->readBits(&magnitude, 31);                // sign-and-magnitude - see above
        baseIndex = (sign == 1) ? -static_cast<int>(magnitude) : static_cast<int>(magnitude);
    }

    for (unsigned int i = 0; i < count; ++i)             // NO bound against 6
    {
        if (!m_recvActions[i].readFromStream(stream))
        {
            m_error = 2;
            return false;                                // > GameServer disconnects, reason 29
        }
        m_recvActions[i].m_inputIndex = static_cast<int>(i) + baseIndex;
    }

    processRcvdPlayerActions(static_cast<int>(count));
    m_readBits = stream->getBitPosition() - bitsBefore;          // +0x004
    return true;
}

// Queues only actions newer than the connection's input high-water mark. Returns false
// for a zero count or when no player is bound; changing players does not reset the
// high-water mark.
bool PlayerActionManager::processRcvdPlayerActions(int count)
{
    if (!count)
        return false;

    // [CHOSEN, not recovered] BF2NULL fix, https://github.com/B2BF/OpenBF2/issues/27.
    // Retail dereferences m_player unguarded here (PE 0x616F9F, the advisory's faulting
    // instruction). A connection has no player until its ClientInfo creates one, or at all
    // when that creation is refused, so a client that sends an action block in that window
    // crashes the server. Such actions are dropped without moving the high-water mark.
    if (!m_player)
        return false;

    ActionBuffer* const buffer = m_player->getActionBuffer();

    for (int i = 0; i < count; ++i)
    {
        const int inputIndex = m_recvActions[i].m_inputIndex;
        if (inputIndex > m_recvActionHighWater)          // strictly greater - see above
        {
            m_recvActionHighWater = inputIndex;
            buffer->pushBack(m_recvActions[i]);
        }
    }
    return true;
}

// Writes buffered actions with a 14-bit header and retains them for retransmission.
// Early presence-zero exits leave m_writeBits unchanged. Payload failure sets error 1
// and returns without restoring the narrowed budget or updating m_writeBits.
unsigned int PlayerActionManager::transmit(io::BitStream* stream, PacketStatus& status,
                                         unsigned int budget)
{
    if (!m_player)
    {
        unsigned int nothing = 0;
        stream->writeBits(&nothing, 1);
        return 1;
    }

    const unsigned int startBits = stream->m_highWaterBits;
    const unsigned int savedLimit = stream->m_bitLimit;
    if (budget)
    {
        const unsigned int limit = startBits + budget;
        stream->m_bitLimit = savedLimit <= limit ? savedLimit : limit;
    }

    io::BSPosition mark;
    mark.m_position = stream->m_readWriteSwitch == 1 ? stream->m_readBitPos : stream->m_bitPos;
    mark.m_readWriteSwitch = stream->m_readWriteSwitch;

    stream->m_byte38 = true;
    stream->m_positionCache = stream->m_readWriteSwitch == 1 ? stream->m_readBitPos : stream->m_bitPos;
    stream->m_rwSwitchCache = stream->m_readWriteSwitch;
    stream->m_lengthCache = stream->m_highWaterBits;
    if (!stream->skipBits(14))
    {
        stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword3C));
        stream->m_byte38 = false;
        stream->m_highWaterBits = stream->m_lengthCache;
        stream->m_errorCode = 0;
        stream->m_error = false;
        if (budget)
            stream->m_bitLimit = savedLimit;
        unsigned int nothing = 0;
        stream->writeBits(&nothing, 1);
        return 1;
    }
    stream->m_byte38 = false;

    unsigned int written = 0;
    if (m_sendActionCount > 0)
    {
        const int inputIndex = m_sendActions[0].m_inputIndex;
        unsigned int sign = inputIndex < 0 ? 1u : 0u;
        stream->writeBits(&sign, 1);
        unsigned int magnitude = inputIndex < 0
            ? 0u - static_cast<unsigned int>(inputIndex) : static_cast<unsigned int>(inputIndex);
        stream->writeBits(&magnitude, 31);

        for (int i = 0; i < m_sendActionCount; ++i)
        {
            if (!m_sendActions[i].writeToStream(stream))
            {
                Debug(DebugTypeError, "Game/Common/PlayerActionManager.cpp", 129)
                    << formatString("PlayerActionManager failed to write stream size %d numNewActions %d",
                                    (stream->m_highWaterBits >> 3) + ((stream->m_highWaterBits & 7) != 0),
                                    written);
                m_error = 1;
                return 0;
            }
            ++written;
        }
    }

    if (written)
    {
        status.m_streamManagerMask |= 1u << getStreamManagerId();
        stream->setWritePosition(mark);
        unsigned int present = 1;
        stream->writeBits(&present, 1);
        stream->writeBits(&written, 4);
        unsigned int interpolation = m_playerInterpolationTime;
        stream->writeBits(&interpolation, 9);
        stream->resetWritePosition();
    }
    else
    {
        stream->setPosition(mark);
        stream->setBitSize(startBits);
        unsigned int nothing = 0;
        stream->writeBits(&nothing, 1);
    }

    if (budget)
        stream->m_bitLimit = savedLimit;
    const unsigned int used = stream->m_highWaterBits - startBits;
    m_writeBits = used;
    return used;
}

// Decodes six signed-magnitude axes, button masks and one flag. Only inputs 40..48
// of the high button mask replicate; input indices are reconstructed by the manager.
// Read/write failures clear the stream error and return false. An axis of -32768
// cannot survive the 15-bit magnitude encoding and decodes as zero.
bool PlayerAction::readFromStream(io::BitStream* stream)
{
    for (int i = 0; i < 6; ++i)                          //  loop
    {
        unsigned int sign = 0;
        stream->readBits(&sign, 1);
        unsigned int magnitude = 0;
        stream->readBits(&magnitude, 15);
        m_axis[i] = (sign == 1)
            ? static_cast<short>(-static_cast<int>(magnitude))   //  `neg eax`
            : static_cast<short>(magnitude);
    }

    unsigned int maskLow = 0;
    stream->readBits(&maskLow, 32);
    m_buttonMaskLow = maskLow;                           // +0x10

    unsigned int maskHigh = 0;
    stream->readBits(&maskHigh, 9);                      // nine bits - see above
    m_buttonMaskHigh = maskHigh;                         // +0x14

    unsigned int flag = 0;
    stream->readBits(&flag, 1);
    m_flag = (flag == 1);                                //  setz [+0x18]

    if (stream->hasError())                              //  cmp [+0x1C]
    {
        stream->clearError();                            // +0x20 = 0, then +0x1C = 0, as shipped
        return false;
    }
    return true;
}

bool PlayerAction::writeToStream(io::BitStream* stream)
{
    for (int i = 0; i < 6; ++i)
    {
        const int axis = m_axis[i];                      // sign-extended short
        if (axis < 0)
        {
            unsigned int sign = 1;
            stream->writeBits(&sign, 1);
            unsigned int magnitude = static_cast<unsigned int>(-axis);
            stream->writeBits(&magnitude, 15);
        }
        else
        {
            unsigned int sign = 0;
            stream->writeBits(&sign, 1);
            unsigned int magnitude = static_cast<unsigned int>(axis);
            stream->writeBits(&magnitude, 15);
        }
    }

    unsigned int maskLow = m_buttonMaskLow;
    stream->writeBits(&maskLow, 32);

    unsigned int maskHigh = m_buttonMaskHigh;
    stream->writeBits(&maskHigh, 9);                     // nine bits - the symmetric truncation

    unsigned int flag = m_flag ? 1u : 0u;
    stream->writeBits(&flag, 1);

    if (stream->hasError())
    {
        stream->clearError();
        return false;
    }
    return true;
}

} // namespace hfe
} // namespace dice
