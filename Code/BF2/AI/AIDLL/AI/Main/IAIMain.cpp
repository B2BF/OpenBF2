// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\Main\IAIMain.cpp" (the constructor
// 0x10055900 asserts "!m_instance", "Singleton already created"; asserts are compiled out of retail
// 1.50). Split out of AIMain.cpp; the functions are unchanged. The ELF also keeps them apart from
// AIMain.cpp's functions (0x3E6790.. against AIMain's 0x39xxxx).

#include "AIMain.h"

namespace dice {
namespace hfe {
namespace ai {

// Active IAIMain instance; construction replaces it unconditionally.
IAIMain* IAIMain::m_instance = 0;

// Construction replaces the active IAIMain pointer. Destruction clears it unconditionally.
// Only one IAIMain may be alive when callers use this singleton.
// ELF 0x3E6790 / 0x3E67C0
IAIMain::IAIMain()
{
    m_instance = this;
}

// ELF 0x3E67F0 / 0x3E6820 / 0x3E6850
IAIMain::~IAIMain()
{
    m_instance = 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
