// Wall-clock counter and elapsed-time timer used by the voice server.
//
// Path is [INFERRED]: no __FILE__ string attests this location. The class is dice::hfe::Timer,
// non-polymorphic, and PerformanceCounter is its stateless base. Timer::getCount is [PLATFORM]:
// the Linux arm reads gettimeofday and reports microseconds (getFrequency == 1000000); the Win32
// arm is not read yet and is expected to use the performance counter.
//
// Layout is the recovered x86-64 ELF: Timer is 48 bytes, member offsets below.

#ifndef CORE_TIMER_H   // [INFERRED] guard style
#define CORE_TIMER_H

namespace dice {
namespace hfe {

// Elapsed-time source. Stateless; the frequency is a constant on the recovered build.
class PerformanceCounter
{
public:
    PerformanceCounter();

    // Ticks per second.
    long long getFrequency();

    // Monotonic tick count.
    long long getCount();
};

// Tracks elapsed seconds across update() calls and supports a reset origin.
class Timer : public PerformanceCounter
{
public:
    Timer();

    // Re-origins the timer at the current tick count minus seconds.
    void reset(double seconds);

    // Samples the counter and advances deltaTime and time.
    void update();

    // Seconds elapsed since the last update.
    double getDeltaTime() const;

    // Seconds accumulated since reset.
    double getTime() const;

private:
    long long m_lastCount;          // +0x08
    long long m_accumCount;         // +0x10
    double m_deltaTime;             // +0x18
    double m_time;                  // +0x20
    double m_secondsPerCount;       // +0x28
};

} // namespace hfe
} // namespace dice

#endif // CORE_TIMER_H
