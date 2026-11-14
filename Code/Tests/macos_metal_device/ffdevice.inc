// [CHOSEN, not recovered] 0.3.0 piece 3: ProcessVertices through this device for every case of
// Code/Tests/ff_reference, with the same calls as the Windows generator (ff_run.inc), written in the
// reference file format. Code/Tests/ff_reference/ff_check compares it with ff_reference.txt
// (`ff_check ff_reference.txt --device OUT`).
#include "../ff_reference/ff_compat_macos.h"
#include "../ff_reference/ff_cases.h"
using namespace ffref;
#include "../ff_reference/ff_run.inc"

static int ffDeviceRun(IDirect3DDevice9* d, const char* path)
{
    FILE* f = std::fopen(path, "w");
    CHECK(f != nullptr);
    std::fprintf(f, "device macOS Metal\n");
    const FFRunResult r = ffRun(f, d);
    std::fclose(f);
    std::printf("ff-device: %d cases, %d vertex lines, %d failures -> %s\n", r.written, r.vertexLines, r.failures, path);
    return r.failures ? 1 : 0;
}
