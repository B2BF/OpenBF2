// [TEMP] [CHOSEN, not recovered] Real BinkOpen Linux filename-policy check, issue #36.
// Uses a caller-supplied movie read-only; no sound system or graphics device is initialized.
#include "../../RendDX9Abi/BinkPort.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
void require(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

std::vector<unsigned char> firstFrame(const std::string& path)
{
    using namespace renddx9abi;
    HBINK movie = BinkOpen(path.c_str(), 0);
    require(movie != nullptr, path.c_str());
    BinkDoFrame(movie);
    FFBinkPicture picture;
    require(binkPortPicture(movie, &picture), "movie decodes a real frame");
    std::vector<unsigned char> pixels(std::size_t(movie->Width) * movie->Height * 4);
    BinkCopyToBuffer(movie, pixels.data(), int(movie->Width * 4), movie->Height, 0, 0,
                     BINK_COPYALL | BINK_SURFACE_32BGRA);
    BinkClose(movie);
    return pixels;
}

void rejected(const std::string& path)
{
    renddx9abi::HBINK movie = renddx9abi::BinkOpen(path.c_str(), 0);
    if (movie) renddx9abi::BinkClose(movie);
    require(movie == nullptr, path.c_str());
}
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s READ_ONLY_MOVIE EXISTING_SCRATCH_DIRECTORY\n", argv[0]);
        return 2;
    }
    char* resolved = ::realpath(argv[1], nullptr);
    require(resolved != nullptr, "resolve supplied movie");
    const std::string source(resolved);
    std::free(resolved);
    const auto expected = firstFrame(source);
    std::string pattern = std::string(argv[2]) + "/bink-paths-XXXXXX";
    std::vector<char> scratch(pattern.begin(), pattern.end()); scratch.push_back(0);
    require(::mkdtemp(scratch.data()) != nullptr, "create private fixture directory");
    const std::string root(scratch.data());
    require(::mkdir((root + "/MixedMovies").c_str(), 0700) == 0, "create mixed-case directory");
    for (const char* name : {"Dice.bik", "Twin.bik", "TWIN.bik"})
        require(::symlink(source.c_str(), (root + "/MixedMovies/" + name).c_str()) == 0,
                "link read-only movie fixture");
    for (const char* suffix : {"/MixedMovies/Dice.bik", "/mixedmovies/dice.bik", "/mIxEdMoViEs/dIcE.BiK"})
        require(firstFrame(root + suffix) == expected, "case variants decode identical pixels");
    std::string windowsPath = root + "/mixedmovies/dice.bik";
    std::replace(windowsPath.begin(), windowsPath.end(), '/', '\\');
    require(firstFrame(windowsPath) == expected, "Windows separators decode identical pixels");
    rejected(root + "/MixedMovies/missing.bik");
    rejected(root + "/MissingMovies/Dice.bik");
    rejected(root + "/mixedmovies/twin.bik");
    require(firstFrame(root + "/MixedMovies/Twin.bik") == expected, "exact match wins over case-only alternatives");
    { std::ofstream invalid(root + "/MixedMovies/Invalid.bik"); invalid << "not a movie"; }
    rejected(root + "/mixedmovies/invalid.bik");
    std::printf("PASS: real BinkOpen case/separator variants; missing, ambiguous and invalid files refused; fixture %s\n", root.c_str());
}
