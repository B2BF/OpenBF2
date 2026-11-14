// [CHOSEN, not recovered] Engine-free check of dice::hfe::goUpInPath's leading-'/' rule.
// Windows (build.cmd): PE BF2.exe 0x00431BE0 is a jump to the worker 0x00431B60, so a leading '/'
// is always dropped; drive paths keep their form. macOS client (build.sh, DEDICATED=0): game paths
// lose a leading '/' as on Windows, host-absolute paths keep it. POSIX dedicated (DEDICATED=1): the
// ELF rule (0x083D1090), every '/' path keeps it.
#include "../../BF2/Core/StringUtil.h"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int failures = 0;
void expect(const std::string& in, const std::string& want)
{
    std::string p = in;
    dice::hfe::goUpInPath(p);
    if (p != want) { std::fprintf(stderr, "FAIL: goUpInPath(\"%s\") = \"%s\", want \"%s\"\n", in.c_str(), p.c_str(), want.c_str()); ++failures; }
}
}

int main()
{
    expect("objects/a/../b.dds", "objects/b.dds");
    expect("objects//weapons/x.dds", "objects/weapons/x.dds");
#if defined(_WIN32)
    expect("/objects/weapons/handheld/usrif_fnscarl/textures/dds/1p_usrif_fnscar_c.dds",
           "objects/weapons/handheld/usrif_fnscarl/textures/dds/1p_usrif_fnscar_c.dds");
    expect("/objects/a/../b.dds", "objects/b.dds");
    expect("C:/Games/bf2/mods/x/../bf2/a.con", "C:/Games/bf2/mods/bf2/a.con");
    const char* pass = "PASS: goUpInPath Windows (PE): a leading '/' is dropped, drive paths unchanged";
#else
    const std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/Users";
#if DEDICATED
    expect("/objects/weapons/handheld/usrif_fnscarl/textures/dds/1p_usrif_fnscar_c.dds",
           "/objects/weapons/handheld/usrif_fnscarl/textures/dds/1p_usrif_fnscar_c.dds");
    expect("/objects/a/../b.dds", "/objects/b.dds");
    const char* pass = "PASS: goUpInPath dedicated (ELF): '/' paths stay absolute";
#else
    expect("/objects/weapons/handheld/usrif_fnscarl/textures/dds/1p_usrif_fnscar_c.dds",
           "objects/weapons/handheld/usrif_fnscarl/textures/dds/1p_usrif_fnscar_c.dds");
    expect("/objects/a/../b.dds", "objects/b.dds");
    expect("/Levels/devils_perch/Info/loadMap.png", "Levels/devils_perch/Info/loadMap.png");
    const char* pass = "PASS: goUpInPath client: game paths drop a leading '/', host paths keep it";
#endif
    expect(home + "/x/../y.con", home + "/y.con");          // host path keeps its '/'
    expect("/private/tmp/a/../b", "/private/tmp/b");
#endif
    if (failures) return 1;
    std::puts(pass);
    return 0;
}
