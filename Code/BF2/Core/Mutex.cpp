// Opaque mutex ownership with a one-pointer public layout; source path [INFERRED].
// The POSIX payload follows the original except its type (recursive, see the constructor); the Windows
// primitive is a chosen platform adaptation.

#include "Mutex.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <pthread.h>
#endif

#include <new>

namespace dice {
namespace hfe {

// Allocates and initializes the platform mutex payload.
Mutex::Mutex()
{
#if defined(_WIN32)
    // not recovered — the Windows stand-in. Same shape: heap payload, no attributes, no error check.
    CRITICAL_SECTION* p = static_cast<CRITICAL_SECTION*>(::operator new(sizeof(CRITICAL_SECTION)));
    InitializeCriticalSection(p);
    m_mutex = p;
#else
    // the allocation size is A literal 0x18 in the binary, not sizeof(pthread_mutex_t). On the
    // build dice used those agreed; on another they need not. Written as sizeof() here because a
    // hardcoded 24 would be silently wrong on any platform whose mutex is larger.
    pthread_mutex_t* p = static_cast<pthread_mutex_t*>(::operator new(sizeof(pthread_mutex_t)));
    // [CHOSEN, not recovered] Recursive: a deliberate divergence from the Linux retail image in favour
    // of the Windows retail behaviour. The ELF passes null attributes (0x83D2CC0), a normal glibc
    // mutex; the shared code was written against Windows' recursive CRITICAL_SECTION and relocks on
    // one thread: CBF2VoipServer::Shutdown holds mMutex and calls ReleaseAllInstances,
    // which locks it again (ELF 0x84EC920 -> 0x84EC6CD, the same in the PE). With a normal mutex
    // that path hangs forever (local-host shutdown on macOS and the Linux dedicated server). No
    // user waits on its own lock or pairs it with a condition variable, so recursion only removes
    // self-deadlocks; other threads are excluded exactly as before.
    // [CHOSEN, not recovered] owner-approved 2026-11-13 (in place of a linked issue): matches the
    // Windows retail image (CRITICAL_SECTION recursion) on every platform; deviates from the Linux
    // retail image, whose non-recursive mutex self-deadlocks in CBF2VoipServer::Shutdown. [PLATFORM]
    // retail defect of the Linux image: Mutex::Mutex (ELF 0x83D2CA0 / 0x83D2CE0) calls
    // pthread_mutex_init(p, NULL); CBF2VoipServer::Shutdown (ELF 0x84EC900) locks this+8 at 0x84EC920,
    // and ReleaseAllInstances relocks it at 0x84EC6CD.
    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(p, &attributes);
    pthread_mutexattr_destroy(&attributes);
    m_mutex = p;
#endif
}


// Destroys the owned platform mutex when present.
Mutex::~Mutex()
{
    if (m_mutex == 0)               // the null check is in the binary, not defensive padding
        return;

#if defined(_WIN32)
    CRITICAL_SECTION* p = static_cast<CRITICAL_SECTION*>(m_mutex);
    DeleteCriticalSection(p);
    ::operator delete(p);
#else
    pthread_mutex_t* p = static_cast<pthread_mutex_t*>(m_mutex);
    pthread_mutex_destroy(p);
    ::operator delete(p);
#endif
    m_mutex = 0;
}

// Acquires the platform mutex. Lock errors are not reported to the caller.
void Mutex::lock()
{
#if defined(_WIN32)
    EnterCriticalSection(static_cast<CRITICAL_SECTION*>(m_mutex));
#else
    pthread_mutex_lock(static_cast<pthread_mutex_t*>(m_mutex));
#endif
}

// The same shape.
void Mutex::unlock()
{
#if defined(_WIN32)
    LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(m_mutex));
#else
    pthread_mutex_unlock(static_cast<pthread_mutex_t*>(m_mutex));
#endif
}

} // namespace hfe
} // namespace dice
