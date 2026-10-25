// [TEMP] [CHOSEN, not recovered] Actual Video/Bink/D3D9 component fixture.
// The only host substitutes are the path-list provider, device/listener registry,
// allocator and diagnostic sink. Codec, texture upload and video lifetime are real.
// No full renderer, movie shader, engine startup or audible-output claim is made.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../RendDX9Abi/HfeAbi.h"
#include "../RendDX9Abi/PluginEntry.h"
#include "../BF2/RendDX9/RendDX9Base.h"
#define private public
#include "../BF2/GeomMisc/MenuRend/Video.cpp"
#undef private

namespace {
int checks = 0, failures = 0;
IDirect3DDevice9* testDevice = nullptr;
std::vector<std::pair<void*, int>> listeners;
bool check(bool ok, const char* label)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    std::fflush(stdout);
    if (!ok) ++failures;
    return ok;
}
[[noreturn]] void cold() { std::fputs("unexpected fixture host call\n", stderr); std::abort(); }

// Only getPathList is exercised; other host operations refuse visibly.
class Paths final : public dice::hfe::io::IFileManager
{
public:
    std::list<std::string> paths = { "mods/bf2/" };
    void* getPathList() const override { return const_cast<std::list<std::string>*>(&paths); }
    void addRef() override { cold(); }
    int getRef() override { cold(); }
    int release() override { cold(); }
    void* queryInterface(unsigned) const override { cold(); }
    void init() override { cold(); }
    bool isLastFileOpenedFromArchive() const override { cold(); }
    dice::hfe::io::IStream* open(const std::string&, FileMode, FileAccess, unsigned) override { cold(); }
    bool fileExists(const std::string&) override { cold(); }
    bool deleteFile(const std::string&) override { cold(); }
    bool moveFile(const std::string&, const std::string&) override { cold(); }
    bool copyFile(const std::string&, const std::string&, bool) override { cold(); }
    bool readFile(const std::string&, void*, unsigned, unsigned&) override { cold(); }
    bool writeFile(const std::string&, const void*, unsigned, bool) override { cold(); }
    int findFiles(const std::string&, bool, std::list<dice::hfe::io::FileInfo>&) override { cold(); }
    bool getFileInfo(const std::string&, dice::hfe::io::FileInfo&) override { cold(); }
    bool compareFileTime(const std::string&, const std::string&) override { cold(); }
    bool getFullFileName(std::string&) override { cold(); }
    void update() override { cold(); }
    const std::string& getHomeSubstitution() const override { cold(); }
    const std::string& getRootSubstitution() const override { cold(); }
    void setBaseSystem(dice::hfe::io::IFileSystem*) override { cold(); }
    dice::hfe::io::IFileSystem* getBaseSystem() override { cold(); }
    dice::hfe::io::IStream* openwin32(const std::string&, FileMode, FileAccess, unsigned) override { cold(); }
    void addArchiveHandler(dice::hfe::io::IArchiveHandler*) override { cold(); }
    void removeArchiveHandler(dice::hfe::io::IArchiveHandler*) override { cold(); }
    bool addPathOverload(const std::string&, const std::string&) override { cold(); }
    bool removePathOverload(const std::string&, const std::string&) override { cold(); }
    bool addMergeDirectory(const std::string&, const std::string&) override { cold(); }
    bool removeMergeDirectory(const std::string&) override { cold(); }
    dice::hfe::io::IArchive* openArchive(const std::string&, int, bool) override { cold(); }
    void permitMountArchives(bool) override { cold(); }
    dice::hfe::io::IArchive* mountArchive(const std::string&, const std::string&, bool) override { cold(); }
    bool unmountArchive(const std::string&) override { cold(); }
    void unmountAllArchives() override { cold(); }
    void getAllArchives(std::vector<dice::hfe::io::IArchive*>&) override { cold(); }
    bool hasMountedArchives() const override { cold(); }
    std::string stripModPath(const std::string&) override { cold(); }
    void addPath(const std::string&) override { cold(); }
    void removePath(const std::string&) override { cold(); }
    void removeAllPaths() override { cold(); }
    void getWritePath(const std::string&, std::string&) override { cold(); }
    int getFileResolveMode() const override { cold(); }
    void setFileResolveMode(int) override { cold(); }
    std::string fileExistsEx(const std::string&, bool) override { cold(); }
    std::string fileExistsEx2(const std::string&) override { cold(); }
    void setModPaths(std::vector<std::string>) override { cold(); }
};
}
namespace dice { namespace hfe {
renddx9abi::HostFileManager* fileManager = nullptr;
namespace io {
void IFileSystem::init() { cold(); }
bool IFileSystem::isLastFileOpenedFromArchive() const { cold(); }
}
namespace dx9rend {
void* renderDevice() { return testDevice; }
void registerDeviceLostResetListener(void* listener, int priority)
{ listeners.push_back({listener, priority}); }
void unregisterDeviceLostResetListener(void* listener)
{
    const auto it = std::find_if(listeners.begin(), listeners.end(),
        [listener](const std::pair<void*, int>& entry) { return entry.first == listener; });
    check(it != listeners.end(), "listener unregister matches a live registration");
    if (it != listeners.end()) listeners.erase(it);
}
} } }
namespace renddx9abi {
void* engineAlloc(unsigned size) { return std::malloc(size); }
void engineFree(void* p) { std::free(p); }
void logf(const char* format, ...)
{ va_list args; va_start(args, format); std::vprintf(format, args); va_end(args); std::puts(""); }
}
namespace {
using namespace dice::hfe::geom;
bool verifyFrame(Video* video, VideoTarget* target)
{
    const unsigned w = target->width(), h = target->height();
    std::vector<unsigned> expected(w * h, 0xA5A5A5A5u);
    renddx9abi::BinkCopyToBuffer(video->m_hbink, expected.data(), w * 4, h, 0, 0,
        renddx9abi::BINK_COPYALL | renddx9abi::BINK_SURFACE_32BGRA);
    auto texture = static_cast<IDirect3DTexture9*>(videoTargetD3DTexture(target));
    D3DLOCKED_RECT locked{};
    if (!check(SUCCEEDED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)),
               "lock actual uploaded movie texture for readback")) return false;
    unsigned mismatches = 0, changed = 0;
    for (unsigned y = 0; y < video->height(); ++y)
    {
        const unsigned* row = reinterpret_cast<const unsigned*>(
            static_cast<const char*>(locked.pBits) + y * locked.Pitch);
        for (unsigned x = 0; x < video->width(); ++x)
        {
            if ((row[x] & 0xFFFFFFu) != (expected[y * w + x] & 0xFFFFFFu)) ++mismatches;
            if ((row[x] & 0xFFFFFFu) != 0xA5A5A5u) ++changed;
        }
    }
    texture->UnlockRect(0);
    std::printf("frame=%u dimensions=%ux%u target=%ux%u mismatches=%u changed=%u\n",
        video->m_hbink->FrameNum, video->width(), video->height(), w, h, mismatches, changed);
    return check(mismatches == 0 && changed > 0,
        "production texture upload matches real codec RGB output pixel for pixel");
}
void exercise(const char* name)
{
    std::printf("MOVIE %s\n", name);
    auto base = createVideo(IID_IVideo, nullptr);
    auto video = static_cast<Video*>(static_cast<void*>(base));
    if (!check(video && video->getRef() == 1, "factory accepts the original movie interface")) return;
    check(base->queryInterface(IID_IVideo) == base && !base->queryInterface(0xB8032904u),
        "QI accepts the recovered IID and rejects the obsolete value");
    videoInitSound(video);
    if (!check(videoOpen(video, name), "open stock movie through native host path list"))
    { video->release(); return; }
    check(video->width() > 0 && video->height() > 0 && video->m_hbink->Frames > 12,
          "real codec reports dimensions and frame count");
    check(listeners.size() == 2 && listeners[0].second == 5 && listeners[1].second == 1,
          "video and texture register their respective reset priorities");
    check(video->frame()->getRef() == 1, "video initially owns the target alone");
    typedef void* (__thiscall* Acquire)(void*);
    auto target = static_cast<VideoTarget*>(reinterpret_cast<Acquire>(
        (*reinterpret_cast<void***>(video))[7])(video));
    check(target == video->frame() && target->getRef() == 2, "virtual getFrame acquires an owned target reference");
    check(target->queryInterface(IID_IVideoTarget) == target && !target->queryInterface(0xB8032903u),
          "target QI uses its original interface ID");
    check(videoFrameD3DTexture(video) == videoTargetD3DTexture(target) && target->getRef() == 2,
          "internal texture readers borrow without retaining the target");
    auto second = static_cast<VideoTarget*>(videoAcquireFrame(video));
    check(second == target && target->getRef() == 3, "typed frame acquisition has the same ownership contract");
    second->release();
    auto texture = static_cast<IDirect3DTexture9*>(videoTargetD3DTexture(target));
    D3DLOCKED_RECT initial{};
    if (check(SUCCEEDED(texture->LockRect(0, &initial, nullptr, target->m_dynamic ? D3DLOCK_DISCARD : 0)),
              "initialize actual destination texture with an unmistakable sentinel"))
    {
        for (unsigned y = 0; y < target->height(); ++y)
            std::memset(static_cast<char*>(initial.pBits) + y * initial.Pitch, 0xA5, target->width() * 4);
        texture->UnlockRect(0);
    }
    const DWORD deadline = GetTickCount() + 4000;
    bool observedWait = false;
    while (video->m_hbink->FrameNum < 13 && static_cast<LONG>(deadline - GetTickCount()) > 0)
    {
        if (renddx9abi::BinkWait(video->m_hbink))
        {
            const unsigned before = video->m_hbink->FrameNum;
            videoDoFrame(video);
            if (video->m_hbink->FrameNum == before) observedWait = true;
        }
        else videoDoFrame(video);
        Sleep(1);
    }
    check(video->m_hbink->FrameNum >= 13, "production timing loop decodes and advances twelve real frames");
    check(observedWait, "BinkWait prevents premature frame advancement");
    verifyFrame(video, target);
    const unsigned pausedFrame = video->m_hbink->FrameNum;
    video->onDeviceLost();
    Sleep(50);
    videoDoFrame(video);
    check(video->m_hbink->FrameNum == pausedFrame, "video pause prevents decode while device is unavailable");
    video->onDeviceReset();
    check(!video->m_paused && !videoIsDone(video), "reset resumes an unfinished stock movie");
    video->release();
    check(target->getRef() == 1 && listeners.size() == 1,
          "owned frame survives video destruction and codec close");
    D3DSURFACE_DESC desc{};
    check(SUCCEEDED(texture->GetLevelDesc(0, &desc)) && desc.Width == target->width(),
          "retained frame texture remains a live D3D object");
    target->release();
    check(listeners.empty(), "final frame release unregisters and destroys the target");
}
}
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (!check(SetCurrentDirectoryA(argv[1]) != 0, "select stock content root without modifying it")) return 1;
    Paths paths;
    dice::hfe::fileManager = &paths;
    HWND window = CreateWindowExA(0, "STATIC", "BF2 video component", WS_POPUP, 0, 0, 64, 64,
                                  nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!check(window && d3d, "create hidden window and real Direct3D9 provider")) return 1;
    D3DADAPTER_IDENTIFIER9 adapter{};
    d3d->GetAdapterIdentifier(0, 0, &adapter);
    std::printf("adapter=%s driver=%s\n", adapter.Description, adapter.Driver);
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    if (!check(SUCCEEDED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                         &pp, &testDevice)), "create hardware D3D9 device")) return 1;
    for (const char* movie : {"Movies/EA.bik", "Movies/Dice.bik", "Movies/Legal.bik", "Movies/Intro.bik", "Movies/Welcome.bik"})
        exercise(movie);
    auto missing = static_cast<Video*>(static_cast<void*>(createVideo(IID_IVideo, nullptr)));
    check(!videoOpen(missing, "Movies/__bf2_fixture_missing__.bik") && !videoAcquireFrame(missing),
          "missing movie returns failure with no target reference");
    videoShutdownSound(missing);
    missing->release();
    check(listeners.empty(), "failed-open teardown leaves no listeners");
    check(!createVideo(0xFFFFFFFFu, nullptr) && listeners.empty(), "unknown IID refusal releases the newly created video");
    testDevice->Release(); testDevice = nullptr;
    d3d->Release(); DestroyWindow(window); dice::hfe::fileManager = nullptr;
    std::printf("video_bink_d3d_runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
