// The "no waypoints" list: holds one position (the bot's own at creation) and reports it as the
// goal. Evidence: libaidll.so 1.5.3153 (ia-32). Attested file AIBot/WPEmpty.cpp (FILE symbol
// beside 0x1B9230); vtable 0x4B7320 (D1, D0, getUrgency, getGoalPoint, getId; the rest are
// WaypointList's); constructors 0x1B9050 / 0x1B90C0; destructors 0x1B9130 / 0x1B9150 / 0x1B9170.
// Size 0x1C (`operator new(0x1c)` in BotMain::updateBot 0x18BF50).

#ifndef AI_AIDLL_AI_AIBOT_WPEMPTY_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPEMPTY_H

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class WPEmpty : public WaypointList
{
public:
    explicit WPEmpty(const Vec3& position);                     // ELF 0x1B9050
    virtual ~WPEmpty();                                          // ELF 0x1B9130

    virtual float getUrgency(const Vec3& position, Bot* bot);    // ELF 0x1B91B0
    virtual bool  getGoalPoint(Vec3& out);                       // ELF 0x1B9200: always true
    virtual int   getId() { return 8; }                          // ELF 0x1B92E0 (weak)

    // [INFERRED] names; the stores are attested (constructor 0x1B9050).
    // +0x08: WaypointList::m_float8 (base constructor: -1.0f)
    /* +0x0C */ unsigned int m_int12;      // 1 [UNRESOLVED]
    /* +0x10 */ Vec3         m_position;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPEMPTY_H
