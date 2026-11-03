// Exposes engine time to the AI.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_BFTIMER_H
#define AI_AIINTERFACE_BFKNOWLEDGE_BFTIMER_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "AIAdapterTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// Stored wall-clock value used by the AI timer.
// [RETIRE-WHEN: aidll-01-adapters] Confirm and validate the Win32 clock route against the
// original Win32 image before retiring that chosen route; the ELF timeval calls are verified.
struct BFTimerWallClock
{
    long tv_sec;
    long tv_usec;
};

void bfTimerNow(BFTimerWallClock* out);

} // namespace ai
} // namespace hfe
} // namespace dice

namespace dice {
namespace hfe {

class IBase;

namespace ai {

class IBFTimer
{
public:
    IBFTimer();
    virtual ~IBFTimer();

    virtual double getGameTime() const = 0;
    virtual double getTimeIncrease() const = 0;
    virtual double getUpdateTime() const = 0;
    virtual double getWallClockTime() const = 0;
    virtual void setPause(bool paused) = 0;
    virtual bool getPause() = 0;
    virtual void reset() = 0;
};

class BFTimer : public IBFTimer
{
public:
    BFTimer();
    virtual ~BFTimer();

    virtual double getGameTime() const;
    virtual double getTimeIncrease() const;
    virtual double getUpdateTime() const;
    virtual double getWallClockTime() const;
    virtual void setPause(bool paused);
    virtual bool getPause();
    virtual void reset();

    // not virtual - they are absent from the vtable and are called by setPause.
    void pause();
    void unpause();

private:
    /* +0x00 */ // vptr
    /* +0x04 */ bool  m_paused;          // pause/unpause/getPause/reset - byte at +4
    /* +0x08 */ double m_resetMark;       // reset stores 0.0 as a qword here; [UNRESOLVED] name
    /* +0x10 */ BFTimerWallClock* m_wallClock;   // `new(8)` + gettimeofday in the constructor
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_BFTIMER_H
