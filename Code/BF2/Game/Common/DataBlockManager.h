// Transfers buffers as an announcement followed by 63-byte event chunks. enqueueDataBlock
// and update drive sending; newDataBlock and addDataChunk drive receiving and post
// DataBlockReadyEvent on completion. Preserve virtual order, including the trailing
// destructor. [INFERRED] Header placement.

#ifndef GAME_COMMON_DATABLOCKMANAGER_H     // [INFERRED] guard style
#define GAME_COMMON_DATABLOCKMANAGER_H

#include <deque>

#include "../../Core/EventManager.h"       // EventNode - DataBlockReadyEvent derives from it

namespace dice {
namespace hfe {

class GameEventManager;

// Data-block progress record with no destructor. The current owner must release
// the buffer or transfer it to the completion event.
struct DataBlock
{
    /* +0x00 */ unsigned int   m_blockEventId;
    /* +0x04 */ unsigned int   m_size;
    /* +0x08 */ unsigned int   m_position;
    /* +0x0C */ unsigned char* m_data;
};

// ECNetwork completion event owning the received buffer. Its destructor releases
// the bytes transferred from DataBlockManager.
class DataBlockReadyEvent : public EventNode
{
public:
    DataBlockReadyEvent(int connectionId, unsigned int blockEventId,
                        unsigned int size, unsigned char* data);
    virtual ~DataBlockReadyEvent();
    virtual int getType() const;                             // +0x0C  : returns 1

    /* +0x1C */ int            m_connectionId;
    /* +0x20 */ unsigned int   m_blockEventId;
    /* +0x24 */ unsigned int   m_size;
    /* +0x28 */ unsigned char* m_data;
};

// Queues outgoing data blocks and assembles incoming chunks. Owns pending,
// sending, and receiving blocks; retains the supplied event-manager pointer.
// The recorded 60-byte layout belongs to the 32-bit ELF STL.
class DataBlockManager
{
public:
    // [CHOSEN, not recovered] Largest incoming block newDataBlock accepts (retail has no limit).
    // The largest fixed-size senders are EndOfRoundInfo (7,166 bytes), ServerInfo (895) and
    // MapInfo (369); the map list and remote-console text grow with their content but stay in
    // the tens of kilobytes. 1 MiB leaves a wide margin and bounds the memory a peer can
    // claim to one such buffer per connection.
    enum { MAX_RECEIVE_SIZE = 0x100000 };

    DataBlockManager(GameEventManager* gameEvents, bool isRecording);

    // Keep these four methods before the virtual destructor to preserve interface order.
    virtual bool enqueueDataBlock(unsigned int blockEventId,
                                  const void* data, int size);         // +0x00
    virtual void update();                                             // +0x04
    virtual bool newDataBlock(unsigned int blockEventId,
                              unsigned int size);                      // +0x08
    virtual bool addDataChunk(void* chunk, int size);                  // +0x0C

    virtual ~DataBlockManager();

public:
    /* +0x004 */ std::deque<DataBlock*> m_pending;
    /* +0x02C */ DataBlock*             m_sending;
    /* +0x030 */ DataBlock*             m_receiving;
    /* +0x034 */ GameEventManager*      m_gameEvents;
    /* +0x038 */ bool                   m_isRecording;
};

} // namespace hfe
} // namespace dice

#endif // GAME_COMMON_DATABLOCKMANAGER_H
