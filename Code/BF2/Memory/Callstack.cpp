// Call-stack capture for the allocation records. Compiled out in the shipped configuration: the
// capture reports zero frames and nothing ever activates it. The lock is constructed and destroyed
// with the DLL but never taken.
//
// Names follow the matching functions of the 2006 Memory_r.dll, which implements them
// (OutputDebugString notice, frame-pointer walk); [INFERRED]. Placement [INFERRED].

#include "MemoryInternal.h"

namespace dice {
namespace hfe {
namespace memory {

namespace {

bool s_callstackActive;
Mutex s_callstackLock;

} // namespace

// PE 0x10001670.
bool isCallstackActive()
{
    return s_callstackActive;
}

// PE 0x10001680.
void initCallstack()
{
}

// PE 0x10001690.
void shutdownCallstack()
{
}

// PE 0x100016A0. Returns the number of frames stored.
int captureCallstack(unsigned int* frames, int maxFrames, int skipFrames)
{
    (void)frames;
    (void)maxFrames;
    (void)skipFrames;
    return 0;
}

// PE 0x100016B0. [UNRESOLVED] Identity unknown; the 2006 build's counterpart also returns 0. No
// caller, so the parameter list is not attested; declared without parameters.
int callstackModuleCount()
{
    return 0;
}

} // namespace memory
} // namespace hfe
} // namespace dice
