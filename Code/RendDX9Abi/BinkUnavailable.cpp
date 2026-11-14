// [FRONTIER] [TEMP] [CHOSEN, not recovered] The ten binkw32.dll imports (Bink.h) for native POSIX
// renderer images other than macOS, where no Bink runtime exists; they stop at Video's named
// frontiers first. macOS implements the entry points in BinkFFmpeg.cpp (Code/BF2/External/ffmpeg-bink/README.md). These bodies keep a
// renderer link honest. Each one reports itself and aborts, never returning a handle or a result.
// [RETIRE-WHEN: posix-bink-decoder] Remove when the owner chooses a Bink runtime or replacement
// decoder for macOS (docs/macos-port.md, "Bink, Novint and VoIP capture (S2.8)").

#include "Bink.h"

#if !defined(_WIN32) && !defined(__APPLE__)

#include <cstdio>
#include <cstdlib>

namespace renddx9abi {

namespace {
[[noreturn]] void binkUnavailable(const char* name)
{
    std::fprintf(stderr, "[FRONTIER] renddx9abi::%s reached: no Bink runtime on this platform\n", name);
    std::fflush(stderr);
    std::abort();
}
} // namespace

extern "C" {
HBINK BinkOpen(const char*, unsigned int)                    { binkUnavailable("BinkOpen"); }
void  BinkClose(HBINK)                                        { binkUnavailable("BinkClose"); }
int   BinkWait(HBINK)                                         { binkUnavailable("BinkWait"); }
int   BinkDoFrame(HBINK)                                      { binkUnavailable("BinkDoFrame"); }
void  BinkNextFrame(HBINK)                                    { binkUnavailable("BinkNextFrame"); }
void  BinkCopyToBuffer(HBINK, void*, int, unsigned int, unsigned int, unsigned int, unsigned int)
                                                              { binkUnavailable("BinkCopyToBuffer"); }
void  BinkGetSummary(HBINK, BINKSUMMARY*)                     { binkUnavailable("BinkGetSummary"); }
void  BinkPause(HBINK, int)                                   { binkUnavailable("BinkPause"); }
int   BinkSetSoundSystem(void*, unsigned int)                 { binkUnavailable("BinkSetSoundSystem"); }
void* BinkOpenDirectSound(unsigned int)                       { binkUnavailable("BinkOpenDirectSound"); }
} // extern "C"

} // namespace renddx9abi

#endif // !_WIN32 && !__APPLE__
