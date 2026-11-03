// Memory-backed stream operations and seek calculation. [INFERRED] Source placement.

#include "MemStream.h"

#include <cstring>      // memcpy
#include <iostream>

// static init: ELF 0x492B40 (__tcf_0 0x492BA0, _GLOBAL__I 0x492BD0): iostream only.
// Engine 0x84388A0/0x84388F0/0x8438900 has the same initialization and teardown.

namespace dice {
namespace hfe {
namespace io {

// Writes the new seek position only on success.
// End-relative negative offsets use size - offset, preserving the existing beyond-end result.
bool calcSeek(SeekOrigin origin, int offset, unsigned int size, unsigned int& pos)
{
    if (origin == SEEKORIGIN_BEGIN)
    {
        if (offset < 0 || static_cast<unsigned int>(offset) > size)
            return false;
        pos = static_cast<unsigned int>(offset);
        return true;
    }

    if (origin == SEEKORIGIN_CURRENT)
    {
        const int target = static_cast<int>(pos) + offset;
        if (target < 0 || static_cast<unsigned int>(target) > size)
            return false;
        pos = static_cast<unsigned int>(target);
        return true;
    }

    if (origin == SEEKORIGIN_END)
    {
        if (offset > 0)
            return false;
        if (static_cast<unsigned int>(-offset) > size)
            return false;
        pos = size - static_cast<unsigned int>(offset);     // the defect - do not "fix" the sign
        return true;
    }

    return false;
}

// 0x8489AD0 (C2) / 0x8489B10 (C1). Both vptrs, refCount 1, everything else zero.
MemStream::MemStream()
    : m_refCount(1)     // +0x08
    , m_buffer(0)       // +0x0C
    , m_size(0)         // +0x10
    , m_maxSize(0)      // +0x14
    , m_position(0)     // +0x18
    , m_sizeLimit(0)    // +0x1C
{
}

// 0x8489B50/0x8489B80 (D2/D1), 0x8489BB0 (D0). Frees the buffer if one was ever allocated.
MemStream::~MemStream()
{
    if (m_buffer)
        delete[] m_buffer;
}

// IBase

void MemStream::addRef() { ++m_refCount; }
int  MemStream::getRef() { return m_refCount; }

// Releases one reference, deleting the stream at zero. Returns the remaining count.
int MemStream::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// the six-iid list that attests the whole interface spine (see MemStream.h).
void* MemStream::queryInterface(unsigned int iid) const
{
    MemStream* self = const_cast<MemStream*>(this);

    if (iid == IID_IBase || iid == IID_IStream || iid == IID_INonlinearStream ||
        iid == IID_IMemMapStream || iid == IID_IMemStream)
        return static_cast<IMemStream*>(self);

    if (iid == IID_IAsyncStream)
        return static_cast<IAsyncStream*>(self);        // the +4 subobject, as the ELF's cmovz

    return 0;
}

// IStream

// bounded copy-out. A read past the end returns the short count, not a failure.
unsigned int MemStream::read(void* dest, unsigned int n)
{
    if (!n)
        return 0;

    unsigned int count = m_size - m_position;
    if (m_position + n <= m_size)
        count = n;

    memcpy(dest, m_buffer + m_position, count);
    m_position += count;
    return count;
}

// Writes up to the configured size limit, returning zero when the write would exceed it.
// Growing replaces the buffer without copying or freeing its old storage; pre-size before writing.
unsigned int MemStream::write(const void* src, unsigned int n)
{
    if (!n)
        return 0;

    if (m_sizeLimit && m_position + n > m_sizeLimit)
        return 0;

    if (m_position + n > m_maxSize)
    {
        unsigned int grown = m_maxSize + 2 * n;
        if (m_maxSize >= n)
            grown = 2 * m_maxSize;
        setMaxSize(grown);
    }

    memcpy(m_buffer + m_position, src, n);
    if (m_position + n > m_size)
        m_size = m_position + n;
    m_position += n;
    return n;
}

// forward-only, clamped to size; answers false only when already exhausted.
// The gate is a virtual getAvailableBytes() call on itself, kept virtual as the binary has it.
int MemStream::skip(unsigned int n)
{
    if (!getAvailableBytes())
        return 0;

    m_position += n;
    if (m_position > m_size)
        m_position = m_size;
    return 1;
}

int MemStream::getAvailableBytes() { return static_cast<int>(m_size - m_position); }

// shipped defect 2: a MemStream cannot clone.
IStream* MemStream::clone() { return 0; }

bool MemStream::canRead()  { return true; }
bool MemStream::canWrite() { return true; }

// INonlinearStream

// the position moves only when calcSeek validates.
int MemStream::seek(SeekOrigin origin, int offset)
{
    unsigned int pos = m_position;
    if (!calcSeek(origin, offset, m_size, pos))
        return 0;

    m_position = pos;
    return 1;
}

int MemStream::getPosition() { return static_cast<int>(m_position); }
int MemStream::getSize()     { return static_cast<int>(m_size); }

// IMemMapStream / IMemStream

void* MemStream::getPtr() { return m_buffer; }

// grows the backing store if needed, then clamps the position into the new size.
void MemStream::setSize(unsigned int n)
{
    if (n > m_maxSize)
        setMaxSize(n);

    m_size = n;
    if (m_position > n)
        m_position = n;
}

void MemStream::clear()
{
    m_size = 0;
    m_position = 0;
}

void         MemStream::setSizeLimit(unsigned int n) { m_sizeLimit = n; }
unsigned int MemStream::getSizeLimit()               { return m_sizeLimit; }

// shipped defect 1, reproduced exactly: `new char[n]` straight into the member.
// No copy of the old contents, no delete of the old block. See write()'s note.
void MemStream::setMaxSize(unsigned int n)
{
    m_buffer = new char[n];
    m_maxSize = n;
}

unsigned int MemStream::getMaxSize() { return m_maxSize; }

// IAsyncStream: synchronous over memcpy
// neither begin* validates offset or count against the buffer - a bad offset memcpys wild, in
// the original too. And the token is handed back at refcount 1; releasing it is the caller's job
// (endRead/endWrite only read the value).

IAsyncResult* MemStream::beginRead(void* dest, unsigned int offset, unsigned int n)
{
    memcpy(dest, m_buffer + offset, n);
    return new DummySyncResult<unsigned int>(n);
}

unsigned int MemStream::endRead(IAsyncResult* result)
{
    return static_cast<DummySyncResult<unsigned int>*>(result)->getValue();
}

IAsyncResult* MemStream::beginWrite(const void* src, unsigned int offset, unsigned int n)
{
    memcpy(m_buffer + offset, src, n);
    return new DummySyncResult<unsigned int>(n);
}

unsigned int MemStream::endWrite(IAsyncResult* result)
{
    return static_cast<DummySyncResult<unsigned int>*>(result)->getValue();
}

// create - 0x84FD940, the canonical factory: aggregation refused, new, queryInterface,
// release-on-failure. Registered by FileModule::getClassList (Core/ModuleRegistry.cpp:600).
IBase* MemStream::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    MemStream* self = new MemStream();

    void* itf = self->queryInterface(iid);
    if (itf == 0)
        self->release();

    return static_cast<IBase*>(itf);
}

} // namespace io
} // namespace hfe
} // namespace dice
