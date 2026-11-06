// Fixed-size element pools for small blocks of the main thread.
//
// Seventeen size classes (the table below) each own a chain of 1 MiB slabs taken from the CRT heap
// on first use. A block of n bytes (header included) is served by the smallest class of at least
// n bytes; larger blocks and blocks of other threads go to the CRT directly (Memory.cpp).
// Placement [INFERRED]; class names follow the DLL's error strings ("ChunkPools::free",
// "MemoryPool error" in the 2006 build).

#include <cstdio>
#include <cstdlib>
#include <new>

#include "MemoryInternal.h"

namespace dice {
namespace hfe {
namespace memory {

namespace {

// Size classes in bytes, header included; -1 ends the table. 58 is not a multiple of four, so that
// class's elements round up to 60.
const int kPoolSizes[] = {4, 8, 12, 16, 20, 24, 28, 32, 48, 58, 64, 68, 72, 76, 80, 84, 88, -1};

const int kPoolChunkSize = 0x100000;

unsigned int s_maxPoolSize;
int s_numPools;
ChunkPool* s_pools;
ChunkPool** s_poolBySize;

int roundToPoolAlignment(int size)
{
    return (size & ~(kPoolAlignment - 1)) + ((size & (kPoolAlignment - 1)) != 0 ? kPoolAlignment : 0);
}

} // namespace

// PE 0x100016C0.
ChunkPool::ChunkPool()
    : m_freeHead(0)
{
}

// PE 0x100016D0.
void ChunkPool::addToFreeList(MemoryPool* pool)
{
    if (!pool->m_inFreeList) {
        pool->m_inFreeList = true;
        pool->m_nextFree = m_freeHead;
        m_freeHead = pool;
    }
}

// PE 0x100016F0.
void ChunkPool::updateFreeList()
{
    MemoryPool* const head = m_freeHead;
    if (!head)
        return;
    if (head->m_numUsed + 1 >= head->m_capacity) {
        head->m_inFreeList = false;
        m_freeHead = head->m_nextFree;
    }
}

// PE 0x10001710.
bool ChunkPools::isActive()
{
    return s_numPools > 0;
}

// PE 0x10001720.
MemoryPool::MemoryPool()
    : m_numAllocs(0)
    , m_next(0)
    , m_prev(0)
    , m_memory(0)
    , m_freeList(0)
    , m_chainIndex(0)
    , m_inFreeList(false)
    , m_nextFree(0)
{
}

// PE 0x10001740. Releases the rest of the chain, then this slab's memory.
MemoryPool::~MemoryPool()
{
    if (m_next) {
        m_next->~MemoryPool();
        std::free(m_next);
    }
    if (m_memory)
        std::free(m_memory);
}

// PE 0x10001770.
void MemoryPool::init(int elementSize, int chunkSize)
{
    m_numUsed = 0;
    m_memory = 0;
    m_freeList = 0;
    m_chunkSize = chunkSize;
    m_elementSize = roundToPoolAlignment(elementSize);
    m_capacity = chunkSize / m_elementSize;
}

// PE 0x100017B0.
void MemoryPool::free(void* element)
{
    *static_cast<void**>(element) = m_freeList;
    --m_numUsed;
    m_freeList = element;
}

// PE 0x100017D0. Releases the lookup table, every chain and the class table. The lookup table
// pointer is left dangling.
void ChunkPools::shutdown()
{
    std::free(s_poolBySize);
    for (int i = 0; i < s_numPools; ++i) {
        MemoryPool* const first = s_pools[i].m_first;
        first->~MemoryPool();
        std::free(s_pools[i].m_first);
    }
    // Allocated with new[] in init and released with the CRT free, as shipped.
    std::free(s_pools);
    s_numPools = 0;
}

// PE 0x10001870.
char* ChunkPools::getStatus(char* buffer)
{
    if (s_numPools <= 0)
        return buffer + std::sprintf(buffer, "ChunkPools not activated\n");

    buffer += std::sprintf(buffer, "ChunkPools\n");
    for (int i = 0; i < s_numPools; ++i)
        buffer += std::sprintf(buffer, "%-*d", 16, kPoolSizes[i]);
    buffer += std::sprintf(buffer, "\n");
    for (int i = 0; i < s_numPools; ++i)
        buffer += std::sprintf(buffer, "%-*u", 16, static_cast<unsigned int>(s_pools[i].m_first->m_numAllocs));
    buffer += std::sprintf(buffer, "\n");
    return buffer;
}

// PE 0x10001940. Returns `block` to slab `chainIndex` of the class serving `size`.
bool ChunkPools::free(void* block, int size, int chainIndex)
{
    ChunkPool* const pool = s_poolBySize[size];
    if (pool && chainIndex >= 0 && chainIndex < ChunkPool::kMaxChains) {
        MemoryPool* const slab = pool->m_chains[chainIndex];
        if (slab) {
            slab->free(block);
            --s_poolBySize[size]->m_first->m_numAllocs;
            s_poolBySize[size]->addToFreeList(slab);
            return true;
        }
    }
    memoryError(false, "memory.dll: chunkPools::free fails, should not come here!");
    return false;
}

// PE 0x100019C0. Registers a new slab at its chain position and offers it for allocation.
void ChunkPools::setInternalPoolByIndex(int size, int chainIndex, MemoryPool* slab)
{
    ChunkPool* const pool = s_poolBySize[size];
    if (chainIndex >= 0 && chainIndex < ChunkPool::kMaxChains) {
        pool->m_chains[chainIndex] = slab;
        pool->addToFreeList(slab);
        return;
    }
    memoryError(false, "memory.dll: setInternalPoolByIndex() fails, should not come here");
}

// PE 0x10001A10.
void* MemoryPool::alloc(unsigned int size, int* chainIndex)
{
    m_chainIndex = *chainIndex;
    if (m_numUsed < m_capacity) {
        if (!m_memory) {
            m_memory = static_cast<char*>(std::malloc(m_elementSize * m_capacity));
            for (int i = 0; i < m_capacity - 1; ++i)
                *reinterpret_cast<char**>(m_memory + i * m_elementSize) = m_memory + (i + 1) * m_elementSize;
            *reinterpret_cast<char**>(m_memory + (m_capacity - 1) * m_elementSize) = 0;
            m_freeList = m_memory;
        }
        ++m_numUsed;
        void* const element = m_freeList;
        m_freeList = *static_cast<void**>(element);
        return element;
    }

    ++*chainIndex;
    if (!m_next) {
        m_next = new (std::malloc(sizeof(MemoryPool))) MemoryPool();
        m_next->init(m_elementSize, m_chunkSize);
        m_next->m_prev = this;
        ChunkPools::setInternalPoolByIndex(size, *chainIndex, m_next);
    }
    return m_next->alloc(size, chainIndex);
}

// PE 0x10001B40. Builds the classes and the size-to-class lookup (sizes 0..largest class).
void ChunkPools::init()
{
    s_maxPoolSize = 0;
    int count = 0;
    for (; kPoolSizes[count] != -1; ++count) {
        if (static_cast<unsigned int>(kPoolSizes[count]) > s_maxPoolSize)
            s_maxPoolSize = kPoolSizes[count];
    }
    s_numPools = count;

    s_poolBySize = static_cast<ChunkPool**>(std::malloc((s_maxPoolSize + 1) * sizeof(ChunkPool*)));
    for (unsigned int i = 0; i <= s_maxPoolSize; ++i)
        s_poolBySize[i] = 0;

    s_pools = new ChunkPool[s_numPools];

    int nextSize = 0;
    for (int i = 0; i < s_numPools; ++i) {
        const int size = kPoolSizes[i];
        ChunkPool* const pool = &s_pools[i];
        pool->m_first = new (std::malloc(sizeof(MemoryPool))) MemoryPool();
        pool->m_first->init(size, kPoolChunkSize);
        for (; nextSize <= size; ++nextSize)
            s_poolBySize[nextSize] = pool;

        ChunkPool* const owner = s_poolBySize[size];
        MemoryPool* const first = pool->m_first;
        owner->m_chains[0] = first;
        owner->addToFreeList(first);
    }
}

// PE 0x10001D00.
void* ChunkPools::alloc(unsigned int size, int* chainIndex)
{
    if (size == 0 || size > s_maxPoolSize)
        return 0;
    ChunkPool* const pool = s_poolBySize[size];
    if (!pool)
        return 0;

    void* element;
    MemoryPool* const slab = pool->m_freeHead;
    if (slab) {
        pool->updateFreeList();
        *chainIndex = slab->m_chainIndex;
        element = slab->alloc(size, chainIndex);
    } else {
        *chainIndex = 0;
        element = pool->m_first->alloc(size, chainIndex);
    }
    if (!element)
        return 0;
    ++pool->m_first->m_numAllocs;
    return element;
}

} // namespace memory
} // namespace hfe
} // namespace dice
