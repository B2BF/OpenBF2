// [CHOSEN, not recovered] Bink 1.x declarations used by Video, retaining the imported
// entry-point signatures and static DLL linkage. Handle frame counters and summary
// dimensions must keep the offsets read by Video.
#pragma once

namespace renddx9abi {

// The Bink handle. Only the frame counters are read by the recovered code; the rest of the struct is
// opaque, so it is padded out and never dereferenced beyond these fields.
struct BINK
{
    unsigned int Width;        // +0x00
    unsigned int Height;       // +0x04
    unsigned int Frames;       // +0x08  total frame count
    unsigned int FrameNum;     // +0x0C  current frame (1-based); FrameNum >= Frames -> finished
    // The real struct continues; nothing past here is touched by the wrapper.
};
typedef BINK* HBINK;

// BinkGetSummary's output. The wrapper reads only the first two dwords (Width, Height); the struct is
// larger in the SDK but the tail is never read here.
struct BINKSUMMARY
{
    unsigned int Width;        // +0x00
    unsigned int Height;       // +0x04
    unsigned int TotalTime;
    unsigned int FileFrameRate;
    unsigned int FileFrameRateDiv;
    unsigned int FrameRate;
    unsigned int FrameRateDiv;
    unsigned int TotalOpenTime;
    unsigned int TotalFrames;
    unsigned int TotalPlayedFrames;
    unsigned int SkippedFrames;
    unsigned int SkippedBlits;
    unsigned int SoundSkips;
    unsigned int TotalBlitTime;
    unsigned int TotalReadTime;
    unsigned int TotalVideoDecompTime;
    unsigned int TotalAudioDecompTime;
    unsigned int TotalReadSpeed;
    unsigned int SlowestFrameTime;
    unsigned int Slowest2FrameTime;
    unsigned int SlowestFrameNum;
    unsigned int Slowest2FrameNum;
    unsigned int AverageDataRate;
    unsigned int AverageFrameSize;
    unsigned int HighestMemAmount;
    unsigned int TotalIOMemory;
    unsigned int HighestIOUsed;
    unsigned int Highest1SecRate;
    unsigned int Highest1SecFrame;
};

// The BinkCopyToBuffer surface-format flag the wrapper uses to write 32-bit bgra into a locked D3D
// texture (D3DFMT_X8R8G8B8). BINKSURFACE32 in the SDK.
const unsigned int BINK_SURFACE_32BGRA = 3u;
const unsigned int BINK_COPYALL        = 0x80000000u;

// [CHOSEN, not recovered] Only the x86 image imports binkw32.dll (issue #7). The macOS client and
// Windows x64 implement the same ten entry points on the vendored FFmpeg Bink decoders
// (BinkFFmpeg.cpp, Code/BF2/External/ffmpeg-bink/README.md): BF2_BINK_PORT. Other images declare nothing, so any
// reference fails to compile there.
#if !defined(_M_IX86) && (defined(__APPLE__) || defined(_WIN64))
#define BF2_BINK_PORT 1
#else
#define BF2_BINK_PORT 0
#endif

#if defined(_M_IX86)
extern "C" {

// binkw32.dll - the ten imports the shipped image carries (see binkw32.def). __stdcall.
HBINK        __stdcall BinkOpen(const char* name, unsigned int flags);
void         __stdcall BinkClose(HBINK bnk);
int          __stdcall BinkWait(HBINK bnk);                     // nonzero => not time for next frame yet
int          __stdcall BinkDoFrame(HBINK bnk);                  // decode the current frame
void         __stdcall BinkNextFrame(HBINK bnk);                // advance to the next frame
void         __stdcall BinkCopyToBuffer(HBINK bnk, void* dest, int destpitch,
                                        unsigned int destheight, unsigned int destx,
                                        unsigned int desty, unsigned int flags);
void         __stdcall BinkGetSummary(HBINK bnk, BINKSUMMARY* sum);
void         __stdcall BinkPause(HBINK bnk, int pause);
int          __stdcall BinkSetSoundSystem(void* open, unsigned int param);
void*        __stdcall BinkOpenDirectSound(unsigned int param);

} // extern "C"
#elif BF2_BINK_PORT
// [CHOSEN, not recovered] The port (macOS, Windows x64): same names and argument lists, native
// calling convention (x64 has only one). BinkOpenDirectSound keeps its name so Video::initSound
// stays one arm; it selects the port's sound output (AudioQueue on macOS, DirectSound on x64)
// that stands in for binkw32's DirectSound mixer.
extern "C" {

HBINK        BinkOpen(const char* name, unsigned int flags);
void         BinkClose(HBINK bnk);
int          BinkWait(HBINK bnk);
int          BinkDoFrame(HBINK bnk);
void         BinkNextFrame(HBINK bnk);
void         BinkCopyToBuffer(HBINK bnk, void* dest, int destpitch, unsigned int destheight,
                              unsigned int destx, unsigned int desty, unsigned int flags);
void         BinkGetSummary(HBINK bnk, BINKSUMMARY* sum);
void         BinkPause(HBINK bnk, int pause);
int          BinkSetSoundSystem(void* open, unsigned int param);
void*        BinkOpenDirectSound(unsigned int param);

} // extern "C"
#endif // _M_IX86 / BF2_BINK_PORT

} // namespace renddx9abi
