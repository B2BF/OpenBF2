// The AI radio singleton. Nothing in libaidll.so constructs one: the constructors (ELF 0x3B3140,
// 0x3B3190) have no callers and no PLT entry, and this file's static initializer (0x3B3390) only
// sets up <iostream>. m_instance (.bss) is written only by the constructor and the destructor, so
// it stays null; AISettings::~AISettings and AISettings::reset still read it, and the destructor
// deletes it when set.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_AIRADIO_H      // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_AIRADIO_H

namespace dice {
namespace hfe {
namespace ai {

class AIRadio
{
public:
    AIRadio();                      // ELF 0x3B3140 (C2), 0x3B3190 (C1)
    ~AIRadio();                     // ELF 0x3B31E0 (D2), 0x3B3240 (D1) - not virtual

    void clearMessagesForBot(int);  // ELF 0x3B3330 - an empty body
    // ELF 0x3B32A0: the update time follows the game time in steps of at least 0.5 s.
    void update();
    // ELF 0x3B32F0: asks the settings for the bot count, discards it, returns 1. [INFERRED] bool,
    // like the sibling resets; the mangled name carries no return type.
    bool reset();
    void setLastUpdateTime();       // ELF 0x3B3340: the game time
    void setLastUpdateTime(float);  // ELF 0x3B3380

    static AIRadio* m_instance;

private:
    // Retail layout as far as the recovered bodies touch it; the size is not attested (no
    // allocation site exists).
    /* +0x00 */ int m_unresolved0;          // [UNRESOLVED] never touched by the recovered bodies
    // Freed with delete[] by the destructor; the constructor never writes it (shipped). The
    // element type is [UNRESOLVED]; delete[] with no element destructor fits a trivial type.
    /* +0x04 */ unsigned char* m_unresolved4;
    /* +0x08 */ double m_lastUpdateTime;    // [INFERRED] name: update and setLastUpdateTime use it
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIRADIO_H
