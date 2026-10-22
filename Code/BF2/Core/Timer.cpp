// Wall-clock counter and elapsed-time timer.
//
// Recovered from fh2 (PerformanceCounter 0x74B130-0x74B1A0, Timer 0x74AAE0-0x74AC10). The POSIX arm
// reads gettimeofday and reports microseconds; the Win32 arm is [PLATFORM] and uses the performance
// counter. The shipped update() truncates the tick delta and the running total to 32 bits before
// scaling, so long-running timers lose the high bits - preserve that.

#include "Timer.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <sys/time.h>
#endif

namespace dice {
namespace hfe {

PerformanceCounter::PerformanceCounter()
{
}

long long PerformanceCounter::getFrequency()
{
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
#else
    return 1000000;
#endif
}

long long PerformanceCounter::getCount()
{
#if defined(_WIN32)
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
#else
    struct timeval now;
    now.tv_sec = 0;
    now.tv_usec = 0;
    gettimeofday(&now, 0);
    return now.tv_usec + 1000000LL * now.tv_sec;
#endif
}

Timer::Timer()
    : m_lastCount(0)
    , m_accumCount(0)
    , m_deltaTime(0.0)
    , m_time(0.0)
    , m_secondsPerCount(0.0)
{
    m_secondsPerCount = 1.0 / static_cast<double>(getFrequency());
    reset(0.0);
}

void Timer::reset(double seconds)
{
    m_lastCount = getCount();
    m_accumCount = (unsigned int)(int)(seconds / m_secondsPerCount);
    update();
}

void Timer::update()
{
    long long count = getCount();
    long long delta = count - m_lastCount;
    double scale = m_secondsPerCount;
    m_lastCount = count;
    m_accumCount += delta;
    m_deltaTime = (double)(int)delta * scale;
    m_time = (double)(int)m_accumCount * scale;
}

double Timer::getDeltaTime() const
{
    return m_deltaTime;
}

double Timer::getTime() const
{
    return m_time;
}

} // namespace hfe
} // namespace dice
