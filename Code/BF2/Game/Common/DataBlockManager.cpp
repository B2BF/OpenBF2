// Splits and assembles network data blocks, transferring completed payloads into events.
#include "DataBlockManager.h"

#include <cstdio>                               // fprintf - the oversized-announcement report
#include <cstring>                              // memcpy - addDataChunk and enqueueDataBlock

#include "GameEventManager.h"                   // addEvent / getConnectionId
#include "../GameEvents/DataBlockEvent.h"       // the wire event update() emits

namespace dice {
namespace hfe {

// Completed data-block event takes ownership of its payload.
DataBlockReadyEvent::DataBlockReadyEvent(int connectionId, unsigned int blockEventId,
                                         unsigned int size, unsigned char* data)
    : m_connectionId(connectionId)              // +0x1C
    , m_blockEventId(blockEventId)              // +0x20
    , m_size(size)                              // +0x24
    , m_data(data)                              // +0x28
{
    // EventNode() has already zeroed m_next (+0x18) - the ELF's `mov [ebx+18h], 0`.
}

// Owns and frees the buffer transferred from the completed data block.
DataBlockReadyEvent::~DataBlockReadyEvent()
{
    if (m_data)
        operator delete[](m_data);
}

int DataBlockReadyEvent::getType() const
{
    return 1;                                   // `mov eax, 1`
}

// Creates an empty outgoing queue associated with the supplied game-event manager.
DataBlockManager::DataBlockManager(GameEventManager* gameEvents, bool isRecording)
    : m_sending(0)                              // +0x2C
    , m_receiving(0)                            // +0x30
    , m_gameEvents(gameEvents)                  // +0x34
    , m_isRecording(isRecording)                // +0x38
{
}

// Does not delete queued block pointers; pending blocks and their buffers leak on teardown.
DataBlockManager::~DataBlockManager()
{
}

// Copies caller bytes immediately, so the input buffer need not outlive the call.
// Recording mode invokes virtual update immediately to write the full block.
bool DataBlockManager::enqueueDataBlock(unsigned int blockEventId, const void* data, int size)
{
    if (!data || !size)
        return false;

    DataBlock* const block = new DataBlock;
    block->m_size         = 0;
    block->m_position     = 0;
    block->m_data         = 0;
    block->m_blockEventId = blockEventId;
    block->m_size         = static_cast<unsigned int>(size);
    block->m_data         = new unsigned char[size];
    block->m_position     = 0;
    std::memcpy(block->m_data, data, size);

    m_pending.push_back(block);

    if (m_isRecording)
        update();                               // virtual self-call - see above

    return true;
}

// Sends a block announcement and up to three 63-byte payload chunks per tick.
// Recording mode drains the entire block in one call. On completion the sender
// frees both block and buffer; the receiver instead transfers its buffer to an event.
void DataBlockManager::update()
{
    if (!m_sending)
    {
        if (m_pending.empty())
            return;
        m_sending = m_pending.front();
        m_pending.pop_front();
    }
    if (!m_sending)
        return;

    if (m_sending->m_position == 0)
    {
        DataBlockEvent* const announce =
            new DataBlockEvent(m_sending->m_blockEventId, m_sending->m_size);
        m_gameEvents->addEvent(announce);
    }

    int budget = 3;
    while (m_sending->m_position < m_sending->m_size
           && (m_isRecording || --budget != -1))        // recording ignores the budget
    {
        unsigned int chunk = 63;
        if (m_sending->m_size - m_sending->m_position < 63u)
            chunk = m_sending->m_size - m_sending->m_position;

        DataBlockEvent* const payload =
            new DataBlockEvent(m_sending->m_data + m_sending->m_position, chunk);
        m_gameEvents->addEvent(payload);

        m_sending->m_position += chunk;
    }

    if (m_sending->m_position >= m_sending->m_size)
    {
        if (m_sending->m_data)
            operator delete[](m_sending->m_data);
        delete m_sending;                       // the sender frees both - see above
        m_sending = 0;
    }
}

// Starts assembling one incoming block. Returns false if another is in progress.
// ELF 0x808C7E0 passes the announced size to operator new[] (0x808C837) unchecked; the size
// comes straight off the wire (DataBlockEvent::deSerialize, ELF 0x806F9F0, 32 bits), so any
// peer could make the receiver attempt a 4 GB allocation.
// [CHOSEN, not recovered] Announcements above MAX_RECEIVE_SIZE are refused like a busy
// manager: nothing is allocated, m_receiving stays empty, and the block's chunks are then
// dropped by addDataChunk. The connection is kept. Logged once per process.
bool DataBlockManager::newDataBlock(unsigned int blockEventId, unsigned int size)
{
    if (m_receiving)
        return false;                           // already assembling one

    if (size > MAX_RECEIVE_SIZE)
    {
        static bool reported = false;
        if (!reported)
        {
            reported = true;
            std::fprintf(stderr, "[CHOSEN, not recovered] DataBlockManager::newDataBlock: refused "
                                 "block %u announcing %u bytes (limit %u); further refusals are "
                                 "not logged\n", blockEventId, size,
                         static_cast<unsigned int>(MAX_RECEIVE_SIZE));
        }
        return false;
    }

    DataBlock* const block = new DataBlock;
    block->m_position     = 0;
    block->m_data         = 0;
    block->m_size         = size;
    block->m_blockEventId = blockEventId;
    block->m_data         = new unsigned char[size];
    block->m_position     = 0;

    m_receiving = block;
    return true;
}

// Appends a chunk only if it fits the announced size. Completion transfers buffer
// ownership to DataBlockReadyEvent before deleting the DataBlock wrapper.
bool DataBlockManager::addDataChunk(void* chunk, int size)
{
    if (!m_receiving)
        return false;

    if (static_cast<unsigned int>(size) + m_receiving->m_position > m_receiving->m_size)
        return false;                           // fails closed - see above

    std::memcpy(m_receiving->m_data + m_receiving->m_position, chunk, size);
    m_receiving->m_position += size;

    if (m_receiving->m_position == m_receiving->m_size)
    {
        DataBlockReadyEvent* const ready =
            new DataBlockReadyEvent(m_gameEvents->getConnectionId(),
                                    m_receiving->m_blockEventId,
                                    m_receiving->m_size,
                                    m_receiving->m_data);
        eventManager->postEvent(ECNetwork, ready, 0.0f);

        delete m_receiving;                     // the block only - the event owns the bytes now
        m_receiving = 0;
    }
    return true;
}

} // namespace hfe
} // namespace dice
