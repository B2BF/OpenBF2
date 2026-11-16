// [CHOSEN, not recovered] macOS port: GCMouse raw motion. See MacRawMouse.h.

#include "MacRawMouse.h"
#include "../Host/ClientHost.h"

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>

#include <atomic>

#if !__has_feature(objc_arc)
#error "MacRawMouse.mm must be compiled with -fobjc-arc"
#endif

namespace dice {
namespace hfe {
namespace macos {

namespace {

// 4096 samples: four seconds of a 1000 Hz mouse without a pump. The DirectInput mouse buffer
// (128 records) overflows long before that and the device clears its state then anyway.
const unsigned int kRingSize = 4096;            // a power of two
RawMouseSample g_ring[kRingSize];
std::atomic<unsigned int> g_head(0);            // written by the producer only
std::atomic<unsigned int> g_tail(0);            // written by the consumer only
std::atomic<unsigned int> g_dropped(0);
std::atomic<int> g_count(0);

bool g_started = false;
dispatch_queue_t g_queue = nil;
NSMutableSet<GCMouse*>* g_attached = nil;
id g_connectObserver = nil;
id g_disconnectObserver = nil;

void attach(GCMouse* mouse)
{
    if (!mouse || [g_attached containsObject:mouse])
        return;
    [g_attached addObject:mouse];
    g_count.store(static_cast<int>(g_attached.count));
    mouse.handlerQueue = g_queue;
    // GCMouseInput reports y up positive; DirectInput's Y grows downwards.
    mouse.mouseInput.mouseMovedHandler = ^(GCMouseInput* input, float deltaX, float deltaY) {
        (void)input;
        rawMousePublish(deltaX, -deltaY, host::ClientHost::tickCountMs());
    };
}

void detach(GCMouse* mouse)
{
    if (!mouse || ![g_attached containsObject:mouse])
        return;
    mouse.mouseInput.mouseMovedHandler = nil;
    [g_attached removeObject:mouse];
    g_count.store(static_cast<int>(g_attached.count));
}

} // namespace

void rawMouseStart()
{
    if (g_started)
        return;
    g_started = true;
    g_queue = dispatch_queue_create("openbf2.rawmouse", DISPATCH_QUEUE_SERIAL);
    g_attached = [NSMutableSet set];
    NSNotificationCenter* centre = [NSNotificationCenter defaultCenter];
    NSOperationQueue* main = [NSOperationQueue mainQueue];
    g_connectObserver = [centre addObserverForName:GCMouseDidConnectNotification object:nil
                                             queue:main usingBlock:^(NSNotification* note) {
        attach((GCMouse*)note.object);
    }];
    g_disconnectObserver = [centre addObserverForName:GCMouseDidDisconnectNotification object:nil
                                                queue:main usingBlock:^(NSNotification* note) {
        detach((GCMouse*)note.object);
    }];
    for (GCMouse* mouse in [GCMouse mice])
        attach(mouse);
}

int rawMouseCount() { return g_count.load(); }

bool rawMousePublish(float dx, float dy, unsigned int timeMs)
{
    const unsigned int head = g_head.load();
    if (head - g_tail.load() >= kRingSize)
    {
        g_dropped.fetch_add(1);
        return false;
    }
    RawMouseSample& slot = g_ring[head & (kRingSize - 1)];
    slot.dx = dx;
    slot.dy = dy;
    slot.timeMs = timeMs;
    g_head.store(head + 1);                     // publishes the slot
    return true;
}

std::size_t rawMouseDrain(RawMouseSample* out, std::size_t max)
{
    unsigned int tail = g_tail.load();
    const unsigned int head = g_head.load();
    std::size_t n = 0;
    while (tail != head && n < max)
    {
        out[n++] = g_ring[tail & (kRingSize - 1)];
        ++tail;
    }
    g_tail.store(tail);                         // releases the slots to the producer
    return n;
}

unsigned int rawMouseDropped() { return g_dropped.load(); }

#if defined(OPENBF2_HOST_TEST_SEAMS)
void rawMouseSetCountForTest(int count) { g_count.store(count); }
#endif

} // namespace macos
} // namespace hfe
} // namespace dice
