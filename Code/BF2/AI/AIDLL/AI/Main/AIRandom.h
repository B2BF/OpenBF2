// AI random-number generator with explicit or clock-based seeding.

#ifndef AI_AIDLL_AI_MAIN_AIRANDOM_H
#define AI_AIDLL_AI_MAIN_AIRANDOM_H

namespace dice {
namespace hfe {
namespace ai {

class AIRandom
{
public:
    AIRandom();
    ~AIRandom();                    // - not virtual

    void init();
    unsigned int generateNumbers() const;   // (_ZNK..., so const)

    // Returns the next tempered random value. [CHOSEN] Name for an inline operation.
    unsigned int next()
    {
        // Unsigned comparison, including the inlined draws at ELF 0x30C60D / 0x30CD9F.
        if (static_cast<unsigned int>(m_index) > kStateSize - 1)
            generateNumbers();

        unsigned int y = m_state[m_index++];
        y ^= y >> 11;
        y ^= (y << 7)  & 0x2B5B2500u;
        y ^= (y << 15) & 0xDB8B0000u;
        y ^= y >> 18;
        return y;
    }

    // The scale is 2.3283064e-10 == 1/2^32, computed in x87 long double and stored to a float, so
    // the result is [0, 1). [CHOSEN] name.
    float nextFloat()
    {
        return static_cast<float>(static_cast<double>(next()) * 2.3283064e-10);
    }

    static AIRandom* m_instance;

    // n and m are MT19937's, read from the two loop bounds (0xE2 = 226 and 0x26E = 622) and from
    // the +397 / -227 offsets in the twist.
    enum { kStateSize = 624, kOffset = 397 };

private:
    // [CHOSEN, not recovered] access control. The paired spawn selector inspects
    // this index before the draw: PE spills its running weight to float only
    // across the refill call. No new state, virtual slot or random policy.
    friend class BotSpawner;
public:
    // [CHOSEN, not recovered] Visibility for the generated inline seed property.
    /* +0x000 */ int          m_seed;
private:
    /* +0x004 */ unsigned int m_state[kStateSize];
    /* +0x9C4 */ int          m_index;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIRANDOM_H
