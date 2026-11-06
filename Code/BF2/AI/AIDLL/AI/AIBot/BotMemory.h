// Per-bot behavior urgency state. BotMain initializes m_double16 after construction.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_BOTMEMORY_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_BOTMEMORY_H

namespace dice {
namespace hfe {
namespace ai {

class BotBehaviour;

class BotMemory
{
public:
    BotMemory();
    ~BotMemory();

    void reset();
    // ELF 0x18F2D0 returns a float (x87 st0): the urgency factor for behaviour at time.
    float modifyBehaviourUrgency(BotBehaviour* behaviour, double time);

    // Retail x86 field offsets.
    /*  0 */ int    m_int0;         // = -1 and BotMain writes it again from outside
    /*  4 */ int    m_int4;         // = -1
    /*  8 */ float  m_float8;       // = 1.0f
    /* 12 */ int    m_int12;        // = 1
    /* 16 */ double m_double16;     // Initialized by BotMain after construction.
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTMEMORY_H
