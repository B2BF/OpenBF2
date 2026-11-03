// Fixed-capacity pointer pool with FIFO slot reuse. Header path [INFERRED].
// Handles combine a one-based 16-bit slot index with a 16-bit generation; zero is invalid.
// release increments the generation and appends the slot to the free tail, rejecting stale handles.
// The x86 layout is N eight-byte records followed by 32-bit head and tail indices.
// The pool does not own the pointed-to objects.

#ifndef CORE_SPHANDLEPOOL_H
#define CORE_SPHANDLEPOOL_H

namespace dice {
namespace hfe {

template <class T, int N>
class SpHandlePool
{
public:
    // A handle is `index | (generation << 16)`; 0 is never a valid handle.
    typedef unsigned int Handle;

    struct Record
    {
        T*             m_value;          // +0
        unsigned short m_next;           // +4  the free chain, holding a 1-based index
        unsigned short m_generation;     // +6  bumped by release(), checked by every accessor
    };

    // Initializes empty records with generation zero and links every slot into the free list.
    // libaidll.so: the SpHandlePool<ai::WeaponInfo, 65535> instantiation is ELF 0x36D580 (weak).
    SpHandlePool()
    {
        for (int i = 0; i < N; ++i)
        {
            m_records[i].m_value      = 0;
            m_records[i].m_next       = 0;
            m_records[i].m_generation = 0;
        }

        // 1-based: record i points at handle i + 2, i.e. at record i + 1. The chain therefore reads
        // 1 -> 2 -> ... -> N -> 0, with 0 as the terminator.
        for (int i = 0; i <= N - 2; ++i)
            m_records[i].m_next = static_cast<unsigned short>(i + 2);

        m_records[N - 1].m_next = 0;     // +0x7FFF4 - end of chain

        m_freeHead = 1;                  // +0x7FFF8
        m_freeTail = N;                  // +0x7FFFC - 0xFFFF for N == 65535
    }

    // Associates value with the next free slot and returns its handle, or zero when exhausted.
    // The pool borrows value and does not manage its lifetime.
    Handle acquire(T* value)
    {
        const int head = static_cast<int>(m_freeHead);
        if (head <= 0)
            return 0;

        Record& rec = m_records[head - 1];
        const unsigned short generation = rec.m_generation;
        rec.m_value = value;

        const Handle handle = static_cast<Handle>(head)
                            | (static_cast<Handle>(generation) << 16);

        unsigned int next;
        if (m_freeTail == m_freeHead)
        {
            m_freeTail = 0;
            next = 0;
        }
        else
        {
            next = rec.m_next;
        }
        m_freeHead = next;

        return handle;
    }

    // Releases the slot and increments its 16-bit generation, invalidating old handles until wraparound.
    // Wrap at 65536 recycles is intentional. The void return type remains [UNRESOLVED].
    void release(Handle handle)
    {
        const unsigned short index = static_cast<unsigned short>(handle);
        if (!index)
            return;

        Record& rec = m_records[index - 1];
        const unsigned short generation = rec.m_generation;
        if (generation != static_cast<unsigned short>(handle >> 16))
            return;

        rec.m_generation = static_cast<unsigned short>(generation + 1);
        rec.m_next       = 0;
        rec.m_value      = 0;

        if (m_freeTail)
            m_records[m_freeTail - 1].m_next = index;
        else
            m_freeHead = index;
        m_freeTail = index;
    }

    // Inlined at `WeaponInfoManager::getInformation`, 0x36D370, 44 B. Null for a stale or zero
    // handle; there is no separate validity query.
    T* get(Handle handle) const
    {
        const unsigned short index = static_cast<unsigned short>(handle);
        if (!index)
            return 0;

        const Record& rec = m_records[index - 1];
        if (rec.m_generation != static_cast<unsigned short>(handle >> 16))
            return 0;

        return rec.m_value;
    }

    // Inlined at `WeaponInfoManager::setInformation`, 0x36D3A0, 56 B. Same guard as get(), then a
    // plain store. Return type [UNRESOLVED], `void` for the same reason as release().
    void set(Handle handle, T* value)
    {
        const unsigned short index = static_cast<unsigned short>(handle);
        if (!index)
            return;

        Record& rec = m_records[index - 1];
        if (rec.m_generation != static_cast<unsigned short>(handle >> 16))
            return;

        rec.m_value = value;
    }

    // Replaces the pointer for a matching nonzero handle. Returns false when its generation does not match.
    bool trySet(Handle handle, T* value)
    {
        const unsigned short index = static_cast<unsigned short>(handle);
        if (!index)
            return false;

        Record& rec = m_records[index - 1];
        if (rec.m_generation != static_cast<unsigned short>(handle >> 16))
            return false;

        rec.m_value = value;
        return true;
    }

    // Inlined at `WeaponInfoManager::swapInformation`, 0x36D4B0, 128 B. Both handles are validated
    // before either store, so a half-swap is impossible.
    bool swap(Handle a, Handle b)
    {
        const unsigned short ia = static_cast<unsigned short>(a);
        const unsigned short ib = static_cast<unsigned short>(b);
        if (!ia || !ib)
            return false;

        Record& ra = m_records[ia - 1];
        Record& rb = m_records[ib - 1];
        if (ra.m_generation != static_cast<unsigned short>(a >> 16))
            return false;
        if (rb.m_generation != static_cast<unsigned short>(b >> 16))
            return false;

        T* tmp = ra.m_value;
        ra.m_value = rb.m_value;
        rb.m_value = tmp;
        return true;
    }

    // Rebuilds the free chain and returns true without clearing values or changing generations.
    // The caller must release owned objects and discard outstanding handles before reset.
    bool reset()
    {
        for (int i = 0; i <= N - 2; ++i)
            m_records[i].m_next = static_cast<unsigned short>(i + 2);

        m_records[N - 1].m_next = 0;
        m_freeHead = 1;
        m_freeTail = N;
        return true;
    }

private:
    /* +0x00000 */ Record       m_records[N];
    /* +0x7FFF8 */ unsigned int m_freeHead;
    /* +0x7FFFC */ unsigned int m_freeTail;
};

} // namespace hfe
} // namespace dice

#endif // CORE_SPHANDLEPOOL_H
