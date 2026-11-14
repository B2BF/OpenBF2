// Path: [INFERRED] beside Core/LoadStat.h. Contents recovered from the symbolized ELF.
// Tracks loading progress and invokes registered callbacks; retail field layout is in LoadStat.h.

#include "LoadStat.h"
#include "System.h"     // g_system and its getWallClockTime - executeLoadCallback's clock source
#include "SystemInfo.h" // systemInfo() - setNextLoadPart's discarded getMemoryUsage call
#include <cstring>     // strlen - setNextLoadPart

namespace dice {
namespace hfe {

// Initializes callback pointers and load state, keeping the start-time sentinel at -1.0f.
// Calls resetLoadStats afterward; that reset also serves between-level transitions.
LoadStat::LoadStat()
    : m_loadCallback(0)         // +0x00
    , m_atIncCallback(0)        // +0x04
    , m_unknown08(false)        // +0x08  0x83C474B, a byte store
    , m_unknown09(false)        // +0x09  [CHOSEN] - neither body touches it; see below
    , m_inCallback(false)       // +0x0A  0x83C474F, a byte store
    , m_unknown1C(-1.0f)        // +0x1C  0x83C4774, the 0xBF800000 literal
    , m_unknown34(false)        // +0x34  0x83C4777, a byte store
{
    // [CHOSEN] Initialize the unused byte at +0x09 to false; retail leaves it untouched.

    resetLoadStats();
}

// Clears the loading state. [UNRESOLVED] The original also clears a separate file-scope value not modeled here.
void LoadStat::resetLoadStats()
{
    m_string10.clear();          // 0x83C47FE, _M_mutate(0, size, 0) - first
    m_string0C.clear();          // 0x83C481D, same shape

    m_unknown0B     = false;     // 0x83C483E, a byte store
    m_unknown14     = 0;
    m_unknown24     = 0;
    m_loadCounter   = 0;         // +0x20
    m_unknown28     = 0;
    m_existsCounter = 0;         // +0x2C

    // 0x83C486A zeroes the global at ds:0x8ABBC9C here - loadStat + 4, not a member of this object.
    // Deliberately not modelled; see the note above. Nothing recovered reads it.

    m_unknown18     = 0;
    m_unknown30     = 0;
    m_unknown38     = 0;
    m_unknown3C     = 0;

}

// Sets the unthrottled loading callback. There is no matching unregister operation for this callback slot.
void LoadStat::registerAtIncLoadCallback(void (*callback)())
{
    m_atIncCallback = callback;
}

// Increments loading progress by n and invokes the installed callback on every increment.
// Callback throttling belongs to executeLoadCallback; moving it here changes repaint frequency.
void LoadStat::incLoadCounter(int n)
{
    m_loadCounter += n;

    // Dispatch depends on the main callback, not the optional at-increment callback.
    // Client loading installs the main callback without the dedicated-only increment hook.
    if (m_loadCallback)
        executeLoadCallback();
}

// Invokes eligible loading callbacks with re-entry and suppression guards.
// The at-inc callback is unthrottled; the main callback uses the configured interval.
namespace {

// ds:0x8ABBCA0. File-scope, not a member: the next wall-clock time at which the main load callback
// may fire. Zero-initialised, so the first eligible increment fires immediately.
double s_nextCallbackTime = 0.0;

// Minimum interval between main loading callbacks, in seconds. Level loading may change this value.
double secondsBetweenUpdate = 0.5;

} // namespace

void LoadStat::executeLoadCallback()
{
    if (m_inCallback)                       // +0x0A - re-entrancy guard
        return;

    if (!g_system)
        return;

    if (!m_loadCallback)                    // +0x00 - nothing to eventually call
        return;

    if (m_unknown08)                        // +0x08 - must be clear; [UNRESOLVED] what sets it
        return;

    // unthrottled, on every single call. See the note above.
    if (m_atIncCallback)
        m_atIncCallback();

    // g_system is deliberately re-read here rather than cached across the callback above.
    if (s_nextCallbackTime <= g_system->getWallClockTime())
    {
        m_inCallback = true;
        s_nextCallbackTime = g_system->getWallClockTime() + secondsBetweenUpdate;
        m_loadCallback();
        m_inCallback = false;
    }
}

// Sets the shared callback throttle interval in seconds.
void LoadStat::setSecondsBetweenCallbacks(double seconds)
{
    secondsBetweenUpdate = seconds;
}

// Replaces the load callback, stamping wall-clock time only when the previous slot was empty.
void LoadStat::registerLoadCallback(void (*fn)())
{
    if (!m_loadCallback)
        m_unknown1C = static_cast<float>(g_system->getWallClockTime());
    m_loadCallback = fn;
}


// Records which file is currently being loaded, for the progress display.

//  A plain std::string assignment  -  no dirty flag, no notification. The status screen polls it;
// nothing is pushed. That matters for a rebuild tempted to add an observer here.
void LoadStat::setCurrentFile(const std::string& file)
{
#if !DEDICATED
    // BF2.exe 0x45D610 (all seven FileManager/Win32FileSystem callers): `lea eax,[ecx+28h];
    // push eax; add ecx,0Ch; call basic_string::assign; ret 4`. The client ignores the file name
    // and copies the phase name (+0x28 in the MSVC layout, m_string10; resetLoadStats 0x45D540
    // clears +0x28 then +0x0C, as the Linux reset clears m_string10 then m_string0C). So the
    // loading screen's progressText (endOfRound.swf, dice.bf2.Client.getCurrentLoadFile) shows
    // the load phase, never individual files.
    (void) file;
    m_string0C = m_string10;
#else
    // Linux server (0x83C4980): update the polled loading filename, no callback, no dirty flag.
    m_string0C = file;
#endif
}


// Heap singleton constructed before main; callers rely on it being initialized during startup.
LoadStat* loadStat = new LoadStat();      // ds:0x8ABBC98, operator new(0x40)

// Advances the cumulative progress window by the phase weight and resets its step counter.
// Queries memory usage before validating payload/count; null payload or zero steps then returns.
// A null phase name clears both strings. The final clamp bounds accumulated progress.
void LoadStat::setNextLoadPart(NextLoadPart* part)
{
    systemInfo()->getMemoryUsage();          // +0x14, discarded - see the note above

    if (!part || !part->steps)
        return;

    m_loadCounter = 0;                       // +0x20
    m_unknown24   = part->steps;             // +0x24 - the phase's expected total

    m_unknown38   = m_unknown3C;             // +0x38 = the old end
    m_unknown3C   = m_unknown3C + 0.15000001f * part->weight;   // +0x3C, flt_8898908

    if (part->name)
    {
        m_string10.assign(part->name, strlen(part->name));      // +0x10
        m_string0C.assign(m_string10);                          // +0x0C
    }
    else
    {
        m_string0C.clear();
        m_string10.clear();
    }

    if (m_unknown3C > 1.0f)
        m_unknown3C = 1.0f;
}


// Marks completion before the last callback, waits 100 ms, then drops callbacks and resets statistics.
// Rearms the time sentinel only when its current value is positive.
void LoadStat::unregisterLoadCallback()
{
    m_unknown0B = true;                      // +0x0B - loadCallback's "complete" flag
    executeLoadCallback();                   // the last one, and the only one that sees the flag

    g_system->sleep(100);                    // System +0x20 - see the note above

    m_loadCallback = 0;                      // +0x00
    if (m_unknown1C > 0.0f)
        m_unknown1C = -1.0f;                 // 0xBF800000, and only when it was positive

    resetLoadStats();
}

} // namespace hfe
} // namespace dice
