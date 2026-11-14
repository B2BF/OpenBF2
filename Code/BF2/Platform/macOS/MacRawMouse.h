// [CHOSEN, not recovered] macOS port: unaccelerated mouse motion from the GameController
// framework's GCMouse (macOS 11), the counterpart of the device counts a DirectInput mouse reports.
// Every connected GCMouse is observed (connect/disconnect at run time); their deltas are summed
// into one stream, as DirectInput's system mouse merges every mouse. Design: docs/macos-s2-4.md
// ("Raw mouse deltas").
//
// Threading: the GCMouse handlers run on a private serial dispatch queue (the single producer).
// Samples reach the engine thread through a lock-free single-producer/single-consumer ring with
// sequentially consistent std::atomic indices; CocoaHost drains it
// on the engine (main) thread in pumpEvents. Connection bookkeeping runs on the main thread.

#ifndef PLATFORM_MACOS_MACRAWMOUSE_H
#define PLATFORM_MACOS_MACRAWMOUSE_H

#include <cstddef>

namespace dice {
namespace hfe {
namespace macos {

// One motion report: device counts, x right, y down (DirectInput's directions), on the
// CocoaHost::tickCountMs clock.
struct RawMouseSample
{
    float        dx, dy;
    unsigned int timeMs;
};

// Main thread. Starts observing GCMouse connections and attaches to the mice already present.
// Safe to call more than once.
void rawMouseStart();

// Number of connected GCMouse devices. While it is 0 the NSEvent deltas are used instead.
int rawMouseCount();

// Producer side (the handler queue, or a test). y is in DirectInput's direction (down positive).
// Returns false when the ring is full and the sample was dropped.
bool rawMousePublish(float dx, float dy, unsigned int timeMs);

// Consumer side (engine thread): moves up to max samples, oldest first, and returns the count.
std::size_t rawMouseDrain(RawMouseSample* out, std::size_t max);

// Samples dropped because the ring was full (the consumer did not pump for seconds).
unsigned int rawMouseDropped();

#if defined(OPENBF2_HOST_TEST_SEAMS)
// [TEMP] Test-only (Code/Tests/macos_input): pretend count GCMouse devices are connected, without
// touching the GameController framework. [RETIRE-WHEN: macos-client-window]
void rawMouseSetCountForTest(int count);
#endif

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // PLATFORM_MACOS_MACRAWMOUSE_H
