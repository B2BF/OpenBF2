// Client HUD blink controllers. [INFERRED] Names and header placement.
#ifndef MENU_HUDBLINKER_H
#define MENU_HUDBLINKER_H

#include "../Core/BuildConfig.h"
#if !DEDICATED
#include <vector>

namespace dice { namespace hfe { namespace hud {

class HudBlinker
{
public:
    HudBlinker(float* source, bool* output, float maximum, float minimum,
               float period, float otherPeriod, bool adaptive);
    // The retail table at 0x92FDCC has only update. Do not add a virtual destructor.
    ~HudBlinker() = default;
    virtual void update(float dt);

protected:
    float* m_source;          // PE +0x04
    bool* m_output;           // +0x08
    float m_maximum;          // +0x0C
    float m_minimum;          // +0x10
    float m_period;           // +0x14
    float m_otherPeriod;      // +0x18
    bool m_adaptive;          // +0x1C
    float m_elapsed;          // +0x20
};
#if defined(_M_IX86)
static_assert(sizeof(void*) == 4, "HUD recovery requires x86");
static_assert(sizeof(HudBlinker) == 0x24, "Non-STL retail blink-controller layout");
#endif

// [INFERRED] names/inheritance: the same 0x24-byte prefix, sole update slot and
// base-pointer ownership at 0x7A6004 / 0x46B500 establish these controller variants.
class HudHighBlinker : public HudBlinker
{
public:
    HudHighBlinker(float* source, bool* output, float lower, float upper,
                   float period, float otherPeriod, bool adaptive);
    void update(float dt) override;
};
#if defined(_M_IX86)
static_assert(sizeof(HudHighBlinker) == 0x24, "Retail high-threshold blink layout");
#endif

class HudFloatBlinker : public HudBlinker
{
public:
    HudFloatBlinker(bool* gate, float* output, bool gateValue, float period, bool smooth);
    void update(float dt) override;

private:
    float* m_floatOutput;                            // PE +0x24
    bool m_smooth;                                   // +0x28
    float m_direction;                               // +0x2C, initially -1
    bool* m_gate;                                    // +0x30
    bool m_gateValue;                                // +0x34
};
#if defined(_M_IX86)
static_assert(sizeof(HudFloatBlinker) == 0x38, "Retail float-output blink layout");
#endif

class HudBlinkManager
{
public:
    HudBlinkManager();
    ~HudBlinkManager();
    void add(HudBlinker* blinker);
    void update(float dt);

private:
    // PE +0x00: VC7.1 vector is 16 bytes; use the native STL, not retail allocator words.
    std::vector<HudBlinker*> m_blinkers;
};

} } }
#endif
#endif
