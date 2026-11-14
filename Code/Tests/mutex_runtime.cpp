// [TEMP] dice::hfe::Mutex (CTest bf2_mutex_check): one thread may relock it (as the Windows
// CRITICAL_SECTION the shared code relies on allows), and another thread stays excluded until every
// nested lock is released. A hang fails the test through the watchdog.
#include "../BF2/Core/Mutex.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

static unsigned checks, failures;
static void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}

int main()
{
    std::thread watchdog([] {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::printf("FAIL watchdog: a lock hung\n");
        std::fflush(stdout);
        std::_Exit(2);
    });
    watchdog.detach();

    dice::hfe::Mutex mutex;
    mutex.lock();
    mutex.lock();                          // CBF2VoipServer::Shutdown -> ReleaseAllInstances
    check(true, "the same thread relocks without hanging");

    std::atomic<bool> acquired(false);
    std::thread other([&] {
        mutex.lock();
        acquired = true;
        mutex.unlock();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    check(!acquired, "another thread waits while the mutex is held twice");
    mutex.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    check(!acquired, "one unlock of two keeps the other thread out");
    mutex.unlock();
    other.join();
    check(acquired, "the last unlock lets the other thread in");

    mutex.lock();
    mutex.unlock();
    check(true, "the mutex is reusable afterwards");

    std::printf("mutex runtime %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
