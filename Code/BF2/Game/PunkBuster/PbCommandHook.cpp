// The console pre-dispatch hook for PunkBuster commands, BF2.exe 0x402FD0 / ELF 0x812E690
// (HandleCommandHookPB). Compiled into every image; the dedicated ELF has only the server arm.
// [CHOSEN, not recovered] placement: see PbGlue.h.
#include "PbGlue.h"
#include <cctype>

bool HandleCommandHookPB(const char* command, int length)
{
    if (length < 3 || std::toupper(command[0]) != 'P' || std::toupper(command[1]) != 'B' || command[2] != '_')
        return false;
    if (length >= 6 && std::toupper(command[3]) == 'S' && std::toupper(command[4]) == 'V' && command[5] == '_') {
        PbServerTrapConsole(const_cast<char*>(command), length);
        return true;
    }
#if !DEDICATED
    PbClientTrapConsole(const_cast<char*>(command), length);
#endif
    return true;
}
