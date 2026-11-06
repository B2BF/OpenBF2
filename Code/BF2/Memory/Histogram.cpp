// Allocation-size histogram: live and peak block counts for each size below 10000 bytes, one
// bucket for larger blocks, and the live and peak byte totals.
//
// In the shipped configuration nothing calls histogramAlloc or histogramFree, so the histogram
// never activates: the queries read zero and the dump writes no file. Placement [INFERRED].

#include <cstdio>
#include <cstring>

#include "MemoryInternal.h"

namespace dice {
namespace hfe {
namespace memory {

namespace {

const unsigned int kHistogramSizes = 10000;

struct HistogramEntry
{
    unsigned int peak;
    unsigned int current;
};

HistogramEntry s_entries[kHistogramSizes];
bool s_active;
unsigned int s_peakLarge;
unsigned int s_currentLarge;
unsigned int s_peakTotal;
unsigned int s_currentTotal;
Mutex s_lock;

} // namespace

// PE 0x10002300.
unsigned int histogramCurrentTotal()
{
    s_lock.lock();
    const unsigned int total = s_currentTotal;
    s_lock.unlock();
    return total;
}

// PE 0x10002320.
unsigned int histogramPeakTotal()
{
    s_lock.lock();
    const unsigned int total = s_peakTotal;
    s_lock.unlock();
    return total;
}

// PE 0x10002340. Writes the histogram to `fileName`; nothing happens before the first allocation.
void histogramDump(const char* fileName)
{
    s_lock.lock();
    if (!s_active) {
        s_lock.unlock();
        return;
    }
    FILE* file = std::fopen(fileName, BF2_MEMORY_TEXT_WRITE);
    if (!file) {
        s_lock.unlock();
        return;
    }
    std::fprintf(file, "Allocations greater than %u bytes\n", kHistogramSizes);
    std::fprintf(file, "Current:%u Peak:%u\n", s_currentLarge, s_peakLarge);
    std::fprintf(file, "Total bytes allocated. Current:%u Peak:%u\n\n", s_currentTotal, s_peakTotal);
    for (unsigned int size = 0; size < kHistogramSizes; ++size) {
        if (s_entries[size].current > 0)
            std::fprintf(file, "Size:%u Current:%u Peak:%u\n", size, s_entries[size].current, s_entries[size].peak);
    }
    std::fprintf(file, "\nBlocks where current is zero\n\n");
    for (unsigned int size = 0; size < kHistogramSizes; ++size) {
        if (s_entries[size].current == 0 && s_entries[size].peak > 0)
            std::fprintf(file, "Size:%u Peak:%u\n", size, s_entries[size].peak);
    }
    std::fclose(file);
    s_lock.unlock();
}

// PE 0x10002460. Clears the per-size peaks; the totals keep theirs.
void histogramResetPeaks()
{
    s_lock.lock();
    for (unsigned int size = 0; size < kHistogramSizes; ++size)
        s_entries[size].peak = 0;
    s_lock.unlock();
}

// PE 0x10002490.
void histogramPeaksToCurrent()
{
    s_lock.lock();
    for (unsigned int size = 0; size < kHistogramSizes; ++size)
        s_entries[size].peak = s_entries[size].current;
    s_lock.unlock();
}

// PE 0x100024C0.
void histogramResetCurrent()
{
    s_lock.lock();
    for (unsigned int size = 0; size < kHistogramSizes; ++size)
        s_entries[size].current = 0;
    s_lock.unlock();
}

// PE 0x100024F0.
void histogramResetAll()
{
    s_lock.lock();
    if (s_active)
        std::memset(s_entries, 0, sizeof(s_entries));
    s_lock.unlock();
}

// PE 0x10002530. The first allocation clears the table and activates the histogram.
void histogramAlloc(unsigned int size)
{
    s_lock.lock();
    if (!s_active) {
        std::memset(s_entries, 0, sizeof(s_entries));
        s_active = true;
    }
    s_currentTotal += size;
    if (s_currentTotal > s_peakTotal)
        s_peakTotal = s_currentTotal;
    if (size >= kHistogramSizes) {
        ++s_currentLarge;
        if (s_currentLarge > s_peakLarge)
            s_peakLarge = s_currentLarge;
    } else {
        ++s_entries[size].current;
        if (s_entries[size].current > s_entries[size].peak)
            s_entries[size].peak = s_entries[size].current;
    }
    s_lock.unlock();
}

// PE 0x100025E0. Counts never drop below zero; the byte total is not clamped.
void histogramFree(unsigned int size)
{
    s_lock.lock();
    if (s_active) {
        s_currentTotal -= size;
        if (size >= kHistogramSizes) {
            if (s_currentLarge != 0)
                --s_currentLarge;
        } else {
            if (s_entries[size].current > 0)
                --s_entries[size].current;
        }
    }
    s_lock.unlock();
}

} // namespace memory
} // namespace hfe
} // namespace dice
