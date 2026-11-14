// [TEMP] [CHOSEN, not recovered] Production Video movie policy with engine-free allocation,
// listener, log and Bink observers. No decoder, GPU, content or client launch.
//
// Policy since 2026-11-07 (Code/BF2/External/ffmpeg-bink/README.md): macOS and Windows x64 play movies through
// the Bink stand-in, as x86 does through binkw32.dll; the "movie skipped" refusal is retired.
// This fixture links Video.cpp against fake Bink entry points and checks what Video does with
// them: sound selection, the two-root path search, the summary read, pause/resume on device
// loss, isDone, and that every opened handle is closed exactly once. With no render device the
// VideoTarget cannot be created, so open answers false after a successful BinkOpen and leaves
// the handle for close/dtor (the shipped order, see Video::open).
#include <cstdarg>
#include <list>
#include <map>
#include <string>
#include <vector>
#include "../../BF2/GeomMisc/MenuRend/Video.cpp"

namespace {
int checks = 0, failures = 0, allocations = 0, listeners = 0;
std::vector<std::string> logs;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
[[noreturn]] void unexpected(const char* what)
{
    std::fprintf(stderr, "Unexpected %s in the movie-policy fixture\n", what);
    std::abort();
}

// --- fake Bink -------------------------------------------------------------------------------
renddx9abi::BINK fakeMovie = { 320, 240, 3396, 1 };
std::vector<std::string> opened;           // every BinkOpen name, in order
std::string presentSuffix;                 // a name ending in this opens fakeMovie
int summaries = 0, closesOfMovie = 0, waits = 0, soundSelections = 0;
std::vector<int> pauses;
bool endsWith(const std::string& s, const std::string& tail)
{
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// Only getPathList is used; other host operations refuse visibly.
class Paths final : public dice::hfe::io::IFileManager
{
public:
    std::list<std::string> paths = { "mods/bf2/" };
    void* getPathList() const override { return const_cast<std::list<std::string>*>(&paths); }
    void addRef() override { unexpected("FileManager call"); }
    int getRef() override { unexpected("FileManager call"); }
    int release() override { unexpected("FileManager call"); }
    void* queryInterface(unsigned) const override { unexpected("FileManager call"); }
    void init() override { unexpected("FileManager call"); }
    bool isLastFileOpenedFromArchive() const override { unexpected("FileManager call"); }
    dice::hfe::io::IStream* open(const std::string&, FileMode, FileAccess, unsigned) override { unexpected("FileManager call"); }
    bool fileExists(const std::string&) override { unexpected("FileManager call"); }
    bool deleteFile(const std::string&) override { unexpected("FileManager call"); }
    bool moveFile(const std::string&, const std::string&) override { unexpected("FileManager call"); }
    bool copyFile(const std::string&, const std::string&, bool) override { unexpected("FileManager call"); }
    bool readFile(const std::string&, void*, unsigned, unsigned&) override { unexpected("FileManager call"); }
    bool writeFile(const std::string&, const void*, unsigned, bool) override { unexpected("FileManager call"); }
    int findFiles(const std::string&, bool, std::list<dice::hfe::io::FileInfo>&) override { unexpected("FileManager call"); }
    bool getFileInfo(const std::string&, dice::hfe::io::FileInfo&) override { unexpected("FileManager call"); }
    bool compareFileTime(const std::string&, const std::string&) override { unexpected("FileManager call"); }
    bool getFullFileName(std::string&) override { unexpected("FileManager call"); }
    void update() override { unexpected("FileManager call"); }
    const std::string& getHomeSubstitution() const override { unexpected("FileManager call"); }
    const std::string& getRootSubstitution() const override { unexpected("FileManager call"); }
    void setBaseSystem(dice::hfe::io::IFileSystem*) override { unexpected("FileManager call"); }
    dice::hfe::io::IFileSystem* getBaseSystem() override { unexpected("FileManager call"); }
    dice::hfe::io::IStream* openwin32(const std::string&, FileMode, FileAccess, unsigned) override { unexpected("FileManager call"); }
    void addArchiveHandler(dice::hfe::io::IArchiveHandler*) override { unexpected("FileManager call"); }
    void removeArchiveHandler(dice::hfe::io::IArchiveHandler*) override { unexpected("FileManager call"); }
    bool addPathOverload(const std::string&, const std::string&) override { unexpected("FileManager call"); }
    bool removePathOverload(const std::string&, const std::string&) override { unexpected("FileManager call"); }
    bool addMergeDirectory(const std::string&, const std::string&) override { unexpected("FileManager call"); }
    bool removeMergeDirectory(const std::string&) override { unexpected("FileManager call"); }
    dice::hfe::io::IArchive* openArchive(const std::string&, int, bool) override { unexpected("FileManager call"); }
    void permitMountArchives(bool) override { unexpected("FileManager call"); }
    dice::hfe::io::IArchive* mountArchive(const std::string&, const std::string&, bool) override { unexpected("FileManager call"); }
    bool unmountArchive(const std::string&) override { unexpected("FileManager call"); }
    void unmountAllArchives() override { unexpected("FileManager call"); }
    void getAllArchives(std::vector<dice::hfe::io::IArchive*>&) override { unexpected("FileManager call"); }
    bool hasMountedArchives() const override { unexpected("FileManager call"); }
    std::string stripModPath(const std::string&) override { unexpected("FileManager call"); }
    void addPath(const std::string&) override { unexpected("FileManager call"); }
    void removePath(const std::string&) override { unexpected("FileManager call"); }
    void removeAllPaths() override { unexpected("FileManager call"); }
    void getWritePath(const std::string&, std::string&) override { unexpected("FileManager call"); }
    int getFileResolveMode() const override { unexpected("FileManager call"); }
    void setFileResolveMode(int) override { unexpected("FileManager call"); }
    std::string fileExistsEx(const std::string&, bool) override { unexpected("FileManager call"); }
    std::string fileExistsEx2(const std::string&) override { unexpected("FileManager call"); }
    void setModPaths(std::vector<std::string>) override { unexpected("FileManager call"); }
};
Paths pathProvider;
} // namespace

namespace renddx9abi {
void* engineAlloc(unsigned int size) { ++allocations; return std::malloc(size); }
void engineFree(void* p) { --allocations; std::free(p); }
void logf(const char* format, ...)
{
    char text[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    logs.push_back(text);
}
extern "C" {
HBINK BinkOpen(const char* name, unsigned int)
{
    opened.push_back(name);
    return (!presentSuffix.empty() && endsWith(name, presentSuffix)) ? &fakeMovie : 0;
}
void  BinkClose(HBINK b) { if (b == &fakeMovie) ++closesOfMovie; else if (b) unexpected("handle"); }
int   BinkWait(HBINK) { ++waits; return 1; }
int   BinkDoFrame(HBINK) { unexpected("BinkDoFrame without a frame target"); }
void  BinkNextFrame(HBINK) { unexpected("BinkNextFrame without a frame target"); }
void  BinkCopyToBuffer(HBINK, void*, int, unsigned int, unsigned int, unsigned int, unsigned int)
{ unexpected("BinkCopyToBuffer without a frame target"); }
void  BinkGetSummary(HBINK b, BINKSUMMARY* s)
{
    ++summaries;
    check(b == &fakeMovie, "summary of the opened movie");
    std::memset(s, 0, sizeof(*s));
    s->Width = b->Width;
    s->Height = b->Height;
    s->TotalFrames = b->Frames;
}
void  BinkPause(HBINK b, int pause) { check(b == &fakeMovie, "pause targets the open movie"); pauses.push_back(pause); }
int   BinkSetSoundSystem(void* open, unsigned int param)
{
    ++soundSelections;
    check(open == reinterpret_cast<void*>(&BinkOpenDirectSound) && param == 0,
          "BinkSetSoundSystem(BinkOpenDirectSound, 0), as retail");
    return 1;
}
void* BinkOpenDirectSound(unsigned int) { unexpected("BinkOpenDirectSound called directly"); }
} // extern "C"
} // namespace renddx9abi

namespace dice { namespace hfe {
renddx9abi::HostFileManager* fileManager = nullptr;
namespace io {
void IFileSystem::init() { unexpected("FileSystem call"); }
bool IFileSystem::isLastFileOpenedFromArchive() const { unexpected("FileSystem call"); }
}
namespace dx9rend {
void registerDeviceLostResetListener(void*, int priority)
{
    check(priority == 5 || priority == 1, "Video (5) or VideoTarget (1) listener");
    ++listeners;
}
void unregisterDeviceLostResetListener(void*) { --listeners; }
void* renderDevice() { return nullptr; }   // device-less host: VideoTarget::create answers false
} } }
HRESULT D3DXCreateTexture(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT,
                         D3DPOOL, IDirect3DTexture9**) { unexpected("texture creation"); }

namespace {
bool anyLog(const char* needle)
{
    for (const std::string& l : logs)
        if (l.find(needle) != std::string::npos)
            return true;
    return false;
}
}

int main()
{
    using namespace dice::hfe::geom;
    dice::hfe::fileManager = &pathProvider;

    // Sound selection: one BinkSetSoundSystem per initSound, as FSMoviePlayer::load does.
    {
        Video* video = reinterpret_cast<Video*>(createVideo(IID_IVideo, nullptr));
        check(video != nullptr && listeners == 1, "Video creation owns one listener");
        check(video->initSound(), "initSound answers true");
        check(soundSelections == 1, "initSound selects Bink's sound system");
        video->release();
    }

    // A movie that is not there: both roots are searched, then a "no .bik" diagnostic.
    const char* missing[] = {
        "Movies/EA.bik", "Movies/dice.bik", "Movies/legal.bik", "Movies/intro.bik",
        "Movies/welcome.bik", "mods/bf2/Movies/menu_loggedin.bik", "Movies/loading.bik"
    };
    for (const char* path : missing)
    {
        opened.clear();
        logs.clear();
        Video* video = reinterpret_cast<Video*>(createVideo(IID_IVideo, nullptr));
        check(!videoOpen(video, path), "a missing movie opens false");
        check(opened.size() == 2, "executable root, then working-directory root (RW-105)");
        check(opened.size() == 2 && endsWith(opened[0], std::string("mods/bf2/") + path)
              && endsWith(opened[1], std::string("mods/bf2/") + path), "candidate = root + path entry + name");
        check(anyLog("BinkOpen found no .bik"), "missing movie diagnostic");
        check(!anyLog("movie skipped"), "no 'movie skipped' refusal (retired)");
        check(!videoAcquireFrame(video) && !videoFrameD3DTexture(video), "no fabricated frame");
        check(video->width() == 0 && video->height() == 0, "no fabricated dimensions");
        video->onDeviceLost();
        video->onDeviceReset();
        check(pauses.empty(), "no BinkPause without a movie");
        check(video->release() == 0, "a failed movie can be released");
        check(!videoCreateAndOpen(path), "Swiff movie creation answers 0 and releases");
    }
    check(closesOfMovie == 0, "nothing opened, nothing closed");

    // A movie that is there: it is opened at the first root and summarised; without a render
    // device the frame target fails, the handle stays for close, and release closes it once.
    {
        presentSuffix = "mods/bf2/mods/bf2/Movies/menu.bik";
        opened.clear();
        logs.clear();
        Video* video = reinterpret_cast<Video*>(createVideo(IID_IVideo, nullptr));
        check(!videoOpen(video, "mods/bf2/Movies/menu.bik"), "device-less open answers false at the target");
        check(opened.size() == 1 && summaries == 1, "BinkOpen succeeded at the first root, then BinkGetSummary");
        check(video->width() == 320 && video->height() == 240, "dimensions from the summary");
        check(anyLog("VideoTarget could not be created"), "target diagnostic");
        check(!anyLog("movie skipped") && !anyLog("no .bik"), "no refusal diagnostics");
        video->onDeviceLost();
        video->onDeviceReset();
        check(pauses.size() == 2 && pauses[0] == 1 && pauses[1] == 0, "device loss pauses, reset resumes");
        videoDoFrame(video);   // not paused: asks BinkWait, which answers "not yet"
        check(waits == 1, "doFrame asks BinkWait");
        fakeMovie.FrameNum = 3395;
        check(videoIsDone(video) == 0, "isDone false before the last frame");
        fakeMovie.FrameNum = 3396;
        check(videoIsDone(video) == 1, "isDone = FrameNum >= Frames");
        check(video->release() == 0 && closesOfMovie == 1, "release closes the movie exactly once");
        presentSuffix.clear();
    }

    Video* video = reinterpret_cast<Video*>(createVideo(IID_IVideo, nullptr));
    opened.clear();
    logs.clear();
    check(!videoOpen(video, nullptr) && opened.empty() && logs.empty(),
          "null movie path refuses without a search or diagnostic");
    video->release();
    check(allocations == 0 && listeners == 0, "no leaked allocation or listener");
    std::printf("Bink movie policy: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
