// [TEMP] The PunkBuster glue objects at the 32-bit x86 ABI the vendor libraries read: the headers' own
// offset asserts (stPbCl: BF2.exe; stPbSv: BF2.exe and the ELF, same offsets) plus the sizes. Compiled
// syntax-only for i686-pc-windows-msvc and i386-linux-gnu (CTests bf2_punkbuster_*_x86_layout).
#include "../../BF2/Game/PunkBuster/PbServer.h"
#if defined(_M_IX86)
#include "../../BF2/Game/PunkBuster/PbClient.h"
static_assert(sizeof(stPbCl) == 0x1AC, "pbcl: ds:0x96E5A8..0x96E753");
#endif
static_assert(sizeof(void*) == 4, "a 32-bit x86 target");
static_assert(offsetof(stPbSv, m_sendToClient) == 0x148 && offsetof(stPbSv, m_sendToAddrPort) == 0x154, "pbsv ABI");
static_assert(offsetof(stPbSv, m_connectString) == 0x158 && offsetof(stPbSv, m_dllHook164) == 0x164, "pbsv ABI");
static_assert(sizeof(stPbSv) == 0x174, "pbsv: the ELF object ends after the capture pair (BF2.exe's at +0x16C)");
