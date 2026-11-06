// Mutex, event and thread wrappers linked into Memory.dll. Only the mutex has users (the histogram
// lock and two locks nothing takes); the event and thread classes have no callers in the DLL.
// Placement [INFERRED].
//
// The Windows bodies are the shipped ones. [CHOSEN, not recovered] The POSIX bodies map them onto
// pthreads: a recursive mutex for the critical section, a mutex and condition variable for the
// manual-reset event, pthread_join for the exit-code poll, and no priority change.

#include <cstdlib>

#include "MemoryInternal.h"

namespace dice {
namespace hfe {
namespace memory {

#if !defined(_WIN32)
struct Event::Impl
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool signalled;
};
#endif

// PE 0x10002650.
Mutex::Mutex()
{
#if defined(_WIN32)
    m_section = static_cast<CRITICAL_SECTION*>(std::malloc(sizeof(CRITICAL_SECTION)));
    if (m_section)
        InitializeCriticalSection(m_section);
#else
    m_section = static_cast<pthread_mutex_t*>(std::malloc(sizeof(pthread_mutex_t)));
    if (m_section) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(m_section, &attr);
        pthread_mutexattr_destroy(&attr);
    }
#endif
}

// PE 0x10002670.
void Mutex::lock()
{
#if defined(_WIN32)
    EnterCriticalSection(m_section);
#else
    pthread_mutex_lock(m_section);
#endif
}

// PE 0x10002680.
void Mutex::unlock()
{
#if defined(_WIN32)
    LeaveCriticalSection(m_section);
#else
    pthread_mutex_unlock(m_section);
#endif
}

// PE 0x10002690. Manual reset, initially clear.
Event::Event()
{
#if defined(_WIN32)
    m_handle = static_cast<HANDLE*>(std::malloc(sizeof(HANDLE)));
    if (m_handle)
        *m_handle = CreateEventA(0, TRUE, FALSE, 0);
#else
    m_handle = static_cast<Impl*>(std::malloc(sizeof(Impl)));
    if (m_handle) {
        pthread_mutex_init(&m_handle->mutex, 0);
        pthread_cond_init(&m_handle->cond, 0);
        m_handle->signalled = false;
    }
#endif
}

// PE 0x100026C0.
void Event::signal()
{
#if defined(_WIN32)
    SetEvent(*m_handle);
#else
    pthread_mutex_lock(&m_handle->mutex);
    m_handle->signalled = true;
    pthread_cond_broadcast(&m_handle->cond);
    pthread_mutex_unlock(&m_handle->mutex);
#endif
}

// PE 0x100026D0.
bool Event::wait(unsigned int milliseconds)
{
#if defined(_WIN32)
    if (milliseconds == 0)
        milliseconds = INFINITE;
    return WaitForSingleObject(*m_handle, milliseconds) == WAIT_OBJECT_0;
#else
    pthread_mutex_lock(&m_handle->mutex);
    if (milliseconds == 0) {
        while (!m_handle->signalled)
            pthread_cond_wait(&m_handle->cond, &m_handle->mutex);
    } else {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += milliseconds / 1000;
        deadline.tv_nsec += static_cast<long>(milliseconds % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }
        while (!m_handle->signalled) {
            if (pthread_cond_timedwait(&m_handle->cond, &m_handle->mutex, &deadline) != 0)
                break;
        }
    }
    const bool signalled = m_handle->signalled;
    pthread_mutex_unlock(&m_handle->mutex);
    return signalled;
#endif
}

// PE 0x10002700. Runs the thread body; the exit code is always 0.
#if defined(_WIN32)
DWORD WINAPI Thread::threadProc(void* thread)
{
    static_cast<Thread*>(thread)->run();
    return 0;
}
#else
void* Thread::threadProc(void* thread)
{
    static_cast<Thread*>(thread)->run();
    return 0;
}
#endif

// PE 0x10002720.
Thread::Thread()
{
    m_data = new Data;
    m_data->handle = 0;
    m_data->id = 0;
    m_data->running = false;
}

// PE 0x10002760; the deleting form is PE 0x100028F0.
Thread::~Thread()
{
    delete m_data;
}

// PE 0x10002780.
bool Thread::start(int unused, int priority)
{
    (void)unused;
#if defined(_WIN32)
    m_data->handle = CreateThread(0, 0, threadProc, this, 0, &m_data->id);
    if (!m_data->handle) {
        m_data->running = false;
        return false;
    }
    m_data->running = true;
    if (priority == PriorityNormal)
        return true;
    int level = THREAD_PRIORITY_NORMAL;
    switch (priority) {
    case PriorityIdle:
        level = THREAD_PRIORITY_IDLE;
        break;
    case PriorityBelowNormal:
        level = THREAD_PRIORITY_BELOW_NORMAL;
        break;
    case PriorityAboveNormal:
        level = THREAD_PRIORITY_ABOVE_NORMAL;
        break;
    case PriorityTimeCritical:
        level = THREAD_PRIORITY_TIME_CRITICAL;
        break;
    }
    SetThreadPriority(m_data->handle, level);
    return true;
#else
    (void)priority;
    if (pthread_create(&m_data->handle, 0, threadProc, this) != 0) {
        m_data->running = false;
        return false;
    }
    m_data->running = true;
    return true;
#endif
}

// PE 0x10002850. The shipped body polls the exit code until the thread is no longer active.
bool Thread::join()
{
    if (m_data->running) {
#if defined(_WIN32)
        DWORD exitCode = STILL_ACTIVE;
        do {
            if (!GetExitCodeThread(m_data->handle, &exitCode))
                break;
        } while (exitCode == STILL_ACTIVE);
        CloseHandle(m_data->handle);
#else
        pthread_join(m_data->handle, 0);
#endif
        m_data->handle = 0;
        m_data->running = false;
    }
    return true;
}

// PE 0x100028B0.
Mutex::~Mutex()
{
#if defined(_WIN32)
    DeleteCriticalSection(m_section);
#else
    pthread_mutex_destroy(m_section);
#endif
    std::free(m_section);
}

// PE 0x100028D0.
Event::~Event()
{
#if defined(_WIN32)
    CloseHandle(*m_handle);
#else
    pthread_cond_destroy(&m_handle->cond);
    pthread_mutex_destroy(&m_handle->mutex);
#endif
    std::free(m_handle);
}

// PE 0x100029D0.
ThreadId threadingCurrentThreadId()
{
    return currentThreadId();
}

} // namespace memory
} // namespace hfe
} // namespace dice
