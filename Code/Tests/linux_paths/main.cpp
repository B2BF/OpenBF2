// [TEMP] [CHOSEN, not recovered] Native disk spelling and production ZIP open fixture.
// No renderer, server or alternate production bodies. Issue #36.
#include "../../BF2/Platform/Linux/ClientFilePaths.h"
#include "../../BF2/IO/File/ZipArchive.h"
#include "../../BF2/Core/StringUtil.h"
#include <unistd.h>
#include <fstream>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
namespace {
void check(bool value, const char* name)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
}
int main(int argc, char** argv)
{
    using dice::hfe::linuxclient::resolveClientPath;
    check(argc == 3, "supply scratch directory and shader ZIP path");
    const std::string root = argv[1];
    check(mkdir(root.c_str(), 0700) == 0 || errno == EEXIST, "scratch directory");
    check(mkdir((root + "/MixedCase").c_str(), 0700) == 0 || errno == EEXIST, "mixed-case directory");
    { std::ofstream file(root + "/MixedCase/ClientArchives.con"); file << "fixture"; }
    std::string path = root + "/mixedcase/clientarchives.con";
    resolveClientPath(path);
    check(path == root + "/MixedCase/ClientArchives.con", "existing components regain disk spelling");
    path = root + "/MixedCase/NewProfile.con";
    resolveClientPath(path);
    check(path == root + "/MixedCase/NewProfile.con", "new leaf retains requested spelling");
    path = root + "/mixedcase/NewDirectory/NewFile.con";
    resolveClientPath(path);
    check(path == root + "/MixedCase/NewDirectory/NewFile.con", "missing suffix retains requested spelling");
    { std::ofstream file(root + "/MixedCase/A"); file << "upper"; }
    { std::ofstream file(root + "/MixedCase/a"); file << "lower"; }
    path = root + "/MixedCase/A";
    resolveClientPath(path);
    check(path == root + "/MixedCase/A", "exact path wins over case-only sibling");
    { std::ofstream file(root + "/MixedCase/AB"); file << "upper"; }
    { std::ofstream file(root + "/MixedCase/ab"); file << "lower"; }
    path = root + "/MixedCase/Ab";
    resolveClientPath(path);
    check(path.empty(), "ambiguous nonexact names refuse resolution");
    check(dice::hfe::isHostAbsolutePath(root), "host root distinguished from virtual asset root");
    check(!dice::hfe::isHostAbsolutePath("/BF2FixtureMissingHostRoot/asset"), "virtual leading slash remains relative");
    dice::hfe::io::ZipArchive::FileHandle archive;
    check(archive.init(argv[2]), "production ZIP handle opens lowercased copied archive path");
    check(unzLocateFile(archive.fileHandle, "StaticMesh.fx", 1) == UNZ_OK, "StaticMesh.fx entry found");
    check(unzOpenCurrentFile(archive.fileHandle) == UNZ_OK, "shader stream opens");
    char bytes[64];
    check(unzReadCurrentFile(archive.fileHandle, bytes, sizeof(bytes)) > 0, "shader bytes readable");
    unzCloseCurrentFile(archive.fileHandle);
    std::puts("PASS: physical paths and production ZIP open preserve Windows content casing");
}
