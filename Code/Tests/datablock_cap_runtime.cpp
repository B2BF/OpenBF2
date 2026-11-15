// [TEMP] [CHOSEN, not recovered] Receive-size limit of DataBlockManager::newDataBlock.
// Links the actual DataBlockManager, DataBlockEvent, GameEvent, EventManager and BitStream
// translation units. Announcements travel through DataBlockEvent::serialize/deSerialize as on
// the wire; the GameEventManager is replaced by a recorder. No sockets, content or retail DLLs.
#include "../BF2/Core/Debug.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/Common/DataBlockManager.h"
#include "../BF2/Game/Common/GameEvent.h"
#include "../BF2/Game/Common/GameEventManager.h"
#include "../BF2/Game/GameEvents/DataBlockEvent.h"
#include "../BF2/IO/BitStream.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
[[noreturn]] void unexpected(const char* method)
{
    std::fprintf(stderr, "[FRONTIER] datablock fixture unexpected %s\n", method);
    std::fflush(stderr);
    std::abort();
}

int failures = 0;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++failures;
    }
}
}

namespace dice { namespace hfe {
IGame* game = nullptr;
EventManager* eventManager = nullptr;
namespace world { class PlayerManager; PlayerManager* playerManager = nullptr; }

Debug::Debug(DebugType, const std::string&, int, const std::string&) { unexpected("Debug"); }
Debug::~Debug() { unexpected("~Debug"); }

// The recorder stands in for the concrete stream manager (not linked). Only the connection
// id and the outgoing queue are under test; every other entry point is fail-loud.
std::vector<GameEvent*> sentEvents;

GameEventManager::GameEventManager(bool) : m_connectionId(7), m_error(0) {}
GameEventManager::~GameEventManager() {}
void GameEventManager::addRef() { unexpected("addRef"); }
int GameEventManager::getRef() { unexpected("getRef"); }
int GameEventManager::release() { unexpected("release"); }
void* GameEventManager::queryInterface(unsigned int) const { unexpected("queryInterface"); }
int GameEventManager::getStreamManagerId() const { unexpected("getStreamManagerId"); }
void GameEventManager::setStreamManagerId(int) { unexpected("setStreamManagerId"); }
bool GameEventManager::processReceivedPacket(io::BitStream*) { unexpected("processReceivedPacket"); }
void GameEventManager::handlePacketStatus(PacketStatus&, bool&) { unexpected("handlePacketStatus"); }
unsigned int GameEventManager::transmit(io::BitStream*, PacketStatus&, unsigned int) { unexpected("transmit"); }
void GameEventManager::setConnectionId(int) { unexpected("setConnectionId"); }
int GameEventManager::getConnectionId() { return m_connectionId; }
int GameEventManager::getError() { unexpected("getError"); }
void GameEventManager::reset() { unexpected("reset"); }
bool GameEventManager::addEvent(GameEvent* event) { sentEvents.push_back(event); return true; }
GameEvent* GameEventManager::getNextRcvdEvent() { unexpected("getNextRcvdEvent"); }
bool GameEventManager::addEventToSendQueue(GameEvent*) { unexpected("addEventToSendQueue"); }
}}

using namespace dice::hfe;

namespace {
// Captures completed blocks instead of queueing them.
struct RecordingEventManager : EventManager {
    std::vector<DataBlockReadyEvent*> ready;
    unsigned int postEvent(EventCategory category, EventNode* node, float) override
    {
        require(category == ECNetwork, "completed block is posted on ECNetwork");
        ready.push_back(static_cast<DataBlockReadyEvent*>(node));
        return 0;
    }
};

// Exposes the decoded announcement/chunk fields of the production event.
struct WireEvent : DataBlockEvent {
    bool isNewBlock() const { return m_isNewBlock; }
    unsigned int blockEventId() const { return m_blockEventId; }
    unsigned int blockLength() const { return m_blockLength; }
    unsigned char* data() { return m_data; }
    unsigned char size() const { return m_size; }
};

// Serializes one event as the sender would and decodes it as the receiver does. The receiver
// reads the type id itself before calling deSerialize (zero bits here: no event types are
// registered in this fixture).
bool transfer(GameEvent* sent, WireEvent& received)
{
    unsigned char buffer[256] = {};
    io::BitStream out(buffer, sizeof(buffer));
    if (!sent->serialize(&out))
        return false;
    io::BitStream in(buffer, sizeof(buffer));
    in.setReadMode();
    in.setBitSize(8 * sizeof(buffer));
    int type = 0;
    in.readBits(&type, gameEventTypeIdBitCount());
    return received.deSerialize(&in);
}

// Delivers one decoded event the way DataBlockEvent::executeClient/executeServer do.
bool deliver(DataBlockManager& receiver, WireEvent& event)
{
    if (event.isNewBlock())
        return receiver.newDataBlock(event.blockEventId(), event.blockLength());
    return receiver.addDataChunk(event.data(), event.size());
}

// Sends one block from sender to receiver through the wire format. Returns the number of
// events the receiver refused.
int sendBlock(DataBlockManager& sender, DataBlockManager& receiver, unsigned int id,
              const std::vector<unsigned char>& payload)
{
    sentEvents.clear();
    sender.enqueueDataBlock(id, payload.data(), static_cast<int>(payload.size()));
    while (sender.m_sending || !sender.m_pending.empty())
        sender.update();
    int refused = 0;
    for (GameEvent* sent : sentEvents) {
        WireEvent received;
        if (!transfer(sent, received) || !deliver(receiver, received))
            ++refused;
        delete sent;
    }
    sentEvents.clear();
    return refused;
}

// Feeds a forged announcement followed by the chunks of a real payload.
int forgeBlock(DataBlockManager& receiver, unsigned int id, unsigned int announcedLength,
               unsigned int chunkCount)
{
    int refused = 0;
    DataBlockEvent announce(id, announcedLength);
    WireEvent decoded;
    require(transfer(&announce, decoded), "forged announcement decodes");
    require(decoded.isNewBlock() && decoded.blockLength() == announcedLength,
            "forged announcement keeps its 32-bit length on the wire");
    if (!deliver(receiver, decoded))
        ++refused;
    unsigned char bytes[DataBlockEvent::MAX_CHUNK_SIZE];
    std::memset(bytes, 0xA5, sizeof(bytes));
    for (unsigned int i = 0; i < chunkCount; ++i) {
        DataBlockEvent chunk(bytes, sizeof(bytes));
        WireEvent chunkDecoded;
        require(transfer(&chunk, chunkDecoded), "forged chunk decodes");
        if (!deliver(receiver, chunkDecoded))
            ++refused;
    }
    return refused;
}

std::vector<unsigned char> pattern(unsigned int size, unsigned char seed)
{
    std::vector<unsigned char> bytes(size);
    for (unsigned int i = 0; i < size; ++i)
        bytes[i] = static_cast<unsigned char>(seed + i * 31u);
    return bytes;
}

bool completed(RecordingEventManager& events, unsigned int id, const std::vector<unsigned char>& payload)
{
    if (events.ready.size() != 1)
        return false;
    DataBlockReadyEvent* const block = events.ready.front();
    events.ready.clear();
    const bool same = block->m_connectionId == 7 && block->m_blockEventId == id
        && block->m_size == payload.size()
        && std::memcmp(block->m_data, payload.data(), payload.size()) == 0;
    delete block;
    return same;
}
}

int main()
{
    RecordingEventManager events;
    eventManager = &events;
    GameEventManager senderEvents(false);
    GameEventManager receiverEvents(false);
    DataBlockManager sender(&senderEvents, false);
    DataBlockManager receiver(&receiverEvents, false);

    // 1. A normal block (EndOfRoundInfo's staging size) assembles and completes.
    const std::vector<unsigned char> endOfRound = pattern(7166, 3);
    require(sendBlock(sender, receiver, 8, endOfRound) == 0, "normal block accepted");
    require(completed(events, 8, endOfRound), "normal block delivered intact");
    require(receiver.m_receiving == nullptr, "receiver idle after a normal block");

    // 2. A 4 GB announcement is refused without allocating, and its chunks are dropped.
    require(forgeBlock(receiver, 7, 0xFFFFFFFFu, 3) == 4, "4 GB announcement and its chunks refused");
    require(receiver.m_receiving == nullptr, "receiver idle after a refused announcement");
    require(events.ready.empty(), "refused block posts nothing");

    // 3. One byte above the limit is refused; the limit itself is accepted.
    require(forgeBlock(receiver, 7, DataBlockManager::MAX_RECEIVE_SIZE + 1u, 1) == 2,
            "limit + 1 refused");
    require(receiver.m_receiving == nullptr, "receiver idle after limit + 1");
    require(forgeBlock(receiver, 7, DataBlockManager::MAX_RECEIVE_SIZE, 1) == 0,
            "announcement at the limit accepted");
    require(receiver.m_receiving != nullptr
            && receiver.m_receiving->m_size == DataBlockManager::MAX_RECEIVE_SIZE,
            "block at the limit is being assembled");
    delete[] receiver.m_receiving->m_data;
    delete receiver.m_receiving;
    receiver.m_receiving = nullptr;

    // 4. After the refusals the same receiver still assembles real blocks.
    const std::vector<unsigned char> remoteText = pattern(200, 9);
    require(sendBlock(sender, receiver, 7, remoteText) == 0, "block after refusals accepted");
    require(completed(events, 7, remoteText), "block after refusals delivered intact");

    // 5. A block at the limit travels end to end.
    const std::vector<unsigned char> largest = pattern(DataBlockManager::MAX_RECEIVE_SIZE, 1);
    require(sendBlock(sender, receiver, 5, largest) == 0, "block at the limit accepted end to end");
    require(completed(events, 5, largest), "block at the limit delivered intact");

    eventManager = nullptr;
    if (failures) {
        std::fprintf(stderr, "FAIL datablock receive limit (%d)\n", failures);
        return 1;
    }
    std::printf("PASS datablock receive limit\n");
    return 0;
}
