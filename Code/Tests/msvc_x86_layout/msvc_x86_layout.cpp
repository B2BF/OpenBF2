// [TEMP] Compiles the voice client's retail x86 layout asserts for the MSVC x86 ABI on any host:
// clang -target i686-pc-windows-msvc -fsyntax-only with the stub headers next to this file. The
// asserts themselves live in the headers under _M_IX86 (VoipClient 0xA0 with +0x44 / +0x88,
// BF2VoipClient 0xC8, VoipBandPassFilter 0x70). Only Windows x86 builds the real image.
#include "../../BF2/Game/VoipClient/BF2VoipClient.h"
static_assert(sizeof(void*) == 4, "the MSVC x86 target");
