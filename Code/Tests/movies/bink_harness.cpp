// [TEMP] [CHOSEN, not recovered] Black-box Bink harness (Code/BF2/External/ffmpeg-bink/README.md).
//
// One source, two builds:
//   - retail: a 32-bit Windows program (i686-w64-mingw32 or MSVC x86) that loads RAD's binkw32.dll
//     by name and calls its exports through GetProcAddress. Nothing of binkw32 is linked or read
//     except through its exported API.
//   - port:   the same program linked against our stand-in (RendDX9Abi/BinkFFmpeg.cpp), built
//     natively on macOS (bf2_bink_harness) or for Windows x64.
//
// The program drives the API the way Video.cpp does (Wait; DoFrame; CopyToBuffer(BINKSURFACE32 |
// BINKCOPYALL); NextFrame) and logs what it observes. For audio, the retail build hands binkw32 a
// fake IDirectSound (BinkSetSoundSystem(BinkOpenDirectSound, (U32)lpDS), the documented
// "use this DirectSound object" form). The fake is an ideal sound card: its play cursor runs on
// the wall clock, it records every block binkw32 writes, and a capture thread copies what passes
// the play cursor into the "heard" stream. The port build installs the same ideal card through
// binkPortSetAudioOutput, so both sides are measured the same way.
//
// Modes (all output goes to files under <out>; nothing game-derived belongs in the repository):
//   frames <bik> <out> <surface> <f1> [f2 ...]   copy frame N (1-based) with surface type 3
//                                                (BGRx), 13 (YUY2) or 15 (YV12) to <out>/fN_sS.raw
//   play <bik> <out> [options]                   game-loop playback, logs <out>/video.csv,
//                                                <out>/summary.txt and, with --sound,
//                                                <out>/heard.s16 (interleaved s16) and
//                                                <out>/writes.bin (retail only)
//     --sound            select the sound system (as FSMoviePlayer does)
//     --hz N             game loop rate (default 60)
//     --stall F:MS       sleep MS once, right after frame F was shown (repeatable)
//     --pause F:MS       BinkPause(1) after frame F, sleep MS, BinkPause(0) (repeatable)
//     --loops N          stop after N wraps (default 1, i.e. one pass incl. the wrap)
//     --seconds S        hard stop after S seconds (default 600)
//     --cardrate R       ideal card speed factor (default 1.0)
//     --realds           retail only: let binkw32 open its own DirectSound (no capture)
//     --delay MS         sleep MS between BinkOpen and the first BinkWait
//     --sysaudio         port only: play through the platform output (AudioQueue/DirectSound)
//                        instead of the ideal card; heard.s16 is not written
//   decodeaudio <bik> <out>                      port only: track 0 decoded straight through the
//                                                audio tap to <out>/ref.s16 (the reference the
//                                                heard streams are aligned against) and ref.f32

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

#if defined(BINK_HARNESS_RETAIL)
#  define NOMINMAX
#  include <windows.h>
#  include <mmsystem.h>
#  include <dsound.h>
#else
#  include "../../RendDX9Abi/Bink.h"
#  include "../../RendDX9Abi/BinkPort.h"
#endif

namespace {

// ---------------------------------------------------------------------------------------------
// The API table.
struct BinkHead { unsigned Width, Height, Frames, FrameNum; };   // the fields Video.cpp reads
struct Summary  { unsigned v[29]; };                             // BINKSUMMARY, 29 dwords

#if defined(BINK_HARNESS_RETAIL)
#  define BHCALL __stdcall
#else
#  define BHCALL
#endif

struct Api
{
    void*    (BHCALL* Open)(const char*, unsigned);
    void     (BHCALL* Close)(void*);
    int      (BHCALL* Wait)(void*);
    int      (BHCALL* DoFrame)(void*);
    void     (BHCALL* NextFrame)(void*);
    void     (BHCALL* CopyToBuffer)(void*, void*, int, unsigned, unsigned, unsigned, unsigned);
    void     (BHCALL* GetSummary)(void*, Summary*);
    void     (BHCALL* Pause)(void*, int);
    int      (BHCALL* SetSoundSystem)(void*, unsigned);
    void*    (BHCALL* OpenDirectSound)(unsigned);
} api;

double nowSec()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void sleepMs(double ms)
{
    if (ms > 0)
        std::this_thread::sleep_for(std::chrono::microseconds(long(ms * 1000.0)));
}

std::string g_out;
FILE* openOut(const char* name, const char* mode = "wb")
{
    std::string p = g_out + "/" + name;
    FILE* f = std::fopen(p.c_str(), mode);
    if (!f)
    {
        std::fprintf(stderr, "cannot write %s\n", p.c_str());
        std::exit(2);
    }
    return f;
}

// ---------------------------------------------------------------------------------------------
// The ideal sound card shared by both builds: a ring the producer writes, a play cursor on the
// wall clock (scaled by --cardrate), and a capture thread that appends what the cursor passed.
struct IdealCard
{
    std::mutex          mutex;
    std::vector<uint8_t> ring;            // retail: the DirectSound buffer bytes
    int                 rate = 0, channels = 0, blockAlign = 0;
    double              speed = 1.0;
    bool                playing = false;
    double              playStart = 0;    // wall time at which cursor position `base` was
    unsigned long long  base = 0;         // cumulative bytes played before playStart
    unsigned long long  captured = 0;     // cumulative bytes copied to heard
    unsigned long long  ringOffset = 0;   // ring position = (played + ringOffset) % size
    FILE*               heard = 0;
    std::atomic<bool>   quit{false};
    std::thread         thread;

    unsigned long long playedLocked(double t) const
    {
        if (!playing)
            return base;
        const double bytes = (t - playStart) * speed * rate * blockAlign;
        unsigned long long b = base + (unsigned long long)(bytes > 0 ? bytes : 0);
        return b - b % unsigned(blockAlign);
    }
    unsigned long long played()
    {
        std::lock_guard<std::mutex> l(mutex);
        return playedLocked(nowSec());
    }
    void start()
    {
        std::lock_guard<std::mutex> l(mutex);
        if (playing) return;
        playing = true;
        playStart = nowSec();
    }
    void stop()
    {
        std::lock_guard<std::mutex> l(mutex);
        if (!playing) return;
        base = playedLocked(nowSec());
        playing = false;
    }
    // Copies ring bytes between the last capture and the cursor (ring is a circular buffer).
    void captureLocked(unsigned long long upTo)
    {
        if (!heard || ring.empty())
        {
            captured = upTo;
            return;
        }
        while (captured < upTo)
        {
            const size_t pos = size_t((captured + ringOffset) % ring.size());
            size_t n = size_t(std::min<unsigned long long>(upTo - captured, ring.size() - pos));
            std::fwrite(&ring[pos], 1, n, heard);
            captured += n;
        }
    }
    void run()
    {
        while (!quit)
        {
            {
                std::lock_guard<std::mutex> l(mutex);
                captureLocked(playedLocked(nowSec()));
            }
            sleepMs(1);
        }
    }
    void launch() { thread = std::thread([this] { run(); }); }
    void finish()
    {
        quit = true;
        if (thread.joinable()) thread.join();
        if (heard) std::fclose(heard);
        heard = 0;
    }
};
IdealCard g_card;
bool      g_cardOpened = false;

#if defined(BINK_HARNESS_RETAIL)
// ---------------------------------------------------------------------------------------------
// Fake DirectSound (retail build). Every call is logged to <out>/ds.log; secondary-buffer writes
// go to <out>/writes.bin as {double t, uint32 offset, uint32 bytes, uint64 played} + data.
FILE* g_dslog = 0;
FILE* g_writes = 0;
void dslog(const char* fmt, ...)
{
    if (!g_dslog) return;
    va_list ap;
    va_start(ap, fmt);
    std::fprintf(g_dslog, "%.6f ", nowSec());
    std::vfprintf(g_dslog, fmt, ap);
    std::fputc('\n', g_dslog);
    va_end(ap);
    std::fflush(g_dslog);
}

class FakeBuffer : public IDirectSoundBuffer
{
public:
    explicit FakeBuffer(bool primary) : m_primary(primary) {}
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** out)
    {
        if (riid == IID_IUnknown || riid == IID_IDirectSoundBuffer) { *out = this; AddRef(); return S_OK; }
        dslog("buffer QI %08lx-%04x-%04x -> E_NOINTERFACE", riid.Data1, riid.Data2, riid.Data3);
        *out = 0;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return ++m_ref; }
    STDMETHOD_(ULONG, Release)()
    {
        ULONG r = --m_ref;
        if (!r) { dslog("buffer%s released", m_primary ? "(primary)" : ""); }
        return r;   // kept alive: the capture thread may still read the ring
    }
    // IDirectSoundBuffer
    STDMETHOD(GetCaps)(LPDSBCAPS caps)
    {
        caps->dwFlags = m_flags;
        caps->dwBufferBytes = DWORD(m_primary ? 0 : g_card.ring.size());
        caps->dwUnlockTransferRate = 0;
        caps->dwPlayCpuOverhead = 0;
        return S_OK;
    }
    STDMETHOD(GetCurrentPosition)(LPDWORD play, LPDWORD write)
    {
        if (m_primary) { if (play) *play = 0; if (write) *write = 0; return S_OK; }
        std::lock_guard<std::mutex> l(g_card.mutex);
        const unsigned long long p = g_card.playedLocked(nowSec());
        const size_t size = g_card.ring.size();
        // write cursor: 10 ms ahead, block aligned (a typical DirectSound safety gap)
        unsigned gap = unsigned(g_card.rate / 100) * unsigned(g_card.blockAlign);
        if (play)  *play  = DWORD((p + g_card.ringOffset) % size);
        if (write) *write = DWORD((p + g_card.ringOffset + gap) % size);
        ++m_posCalls;
        return S_OK;
    }
    STDMETHOD(GetFormat)(LPWAVEFORMATEX f, DWORD size, LPDWORD written)
    {
        if (f && size >= sizeof(WAVEFORMATEX)) *f = m_fmt;
        if (written) *written = sizeof(WAVEFORMATEX);
        return S_OK;
    }
    STDMETHOD(GetVolume)(LPLONG v) { *v = m_volume; return S_OK; }
    STDMETHOD(GetPan)(LPLONG p) { *p = m_pan; return S_OK; }
    STDMETHOD(GetFrequency)(LPDWORD f) { *f = m_freq; return S_OK; }
    STDMETHOD(GetStatus)(LPDWORD s)
    {
        *s = (m_primary ? (m_playing ? DSBSTATUS_PLAYING : 0)
                        : (g_card.playing ? DSBSTATUS_PLAYING | (m_loop ? DSBSTATUS_LOOPING : 0) : 0));
        return S_OK;
    }
    STDMETHOD(Initialize)(LPDIRECTSOUND, LPCDSBUFFERDESC) { return DSERR_ALREADYINITIALIZED; }
    STDMETHOD(Lock)(DWORD off, DWORD bytes, LPVOID* p1, LPDWORD b1, LPVOID* p2, LPDWORD b2, DWORD flags)
    {
        if (m_primary) { dslog("primary Lock"); return DSERR_INVALIDCALL; }
        const DWORD size = DWORD(g_card.ring.size());
        if (flags & DSBLOCK_FROMWRITECURSOR) { DWORD w; GetCurrentPosition(0, &w); off = w; }
        if (flags & DSBLOCK_ENTIREBUFFER) bytes = size;
        if (off >= size || bytes > size) { dslog("Lock bad %lu %lu", off, bytes); return DSERR_INVALIDPARAM; }
        {
            // binkw32 refills the ring right up to the play cursor: copy what the cursor has
            // passed before the region is handed out for overwriting.
            std::lock_guard<std::mutex> l(g_card.mutex);
            g_card.captureLocked(g_card.playedLocked(nowSec()));
        }
        DWORD first = std::min(bytes, size - off);
        *p1 = &g_card.ring[off];
        *b1 = first;
        if (p2) { *p2 = first < bytes ? &g_card.ring[0] : 0; }
        if (b2) { *b2 = bytes - first; }
        m_lockOff = off;
        m_lockBytes = bytes;
        ++m_locks;
        return S_OK;
    }
    STDMETHOD(Play)(DWORD r1, DWORD prio, DWORD flags)
    {
        dslog("buffer%s Play flags=%lx", m_primary ? "(primary)" : "", flags);
        (void)r1; (void)prio;
        if (m_primary) { m_playing = true; return S_OK; }
        m_loop = (flags & DSBPLAY_LOOPING) != 0;
        g_card.start();
        return S_OK;
    }
    STDMETHOD(SetCurrentPosition)(DWORD pos)
    {
        dslog("SetCurrentPosition %lu", pos);
        std::lock_guard<std::mutex> l(g_card.mutex);
        // Time keeps running; only the ring position the cursor reads from jumps.
        unsigned long long p = g_card.playedLocked(nowSec());
        const size_t size = g_card.ring.size();
        g_card.captureLocked(p);
        g_card.ringOffset = (pos + size - p % size) % size;
        return S_OK;
    }
    STDMETHOD(SetFormat)(LPCWAVEFORMATEX f)
    {
        dslog("buffer%s SetFormat tag=%u ch=%u rate=%lu bits=%u", m_primary ? "(primary)" : "",
              f->wFormatTag, f->nChannels, f->nSamplesPerSec, f->wBitsPerSample);
        m_fmt = *f;
        return S_OK;
    }
    STDMETHOD(SetVolume)(LONG v) { if (v != m_volume) dslog("SetVolume %ld", v); m_volume = v; return S_OK; }
    STDMETHOD(SetPan)(LONG p) { if (p != m_pan) dslog("SetPan %ld", p); m_pan = p; return S_OK; }
    STDMETHOD(SetFrequency)(DWORD f) { if (f != m_freq) dslog("SetFrequency %lu", f); m_freq = f; return S_OK; }
    STDMETHOD(Stop)()
    {
        dslog("buffer%s Stop", m_primary ? "(primary)" : "");
        if (m_primary) { m_playing = false; return S_OK; }
        g_card.stop();
        return S_OK;
    }
    STDMETHOD(Unlock)(LPVOID p1, DWORD b1, LPVOID p2, DWORD b2)
    {
        if (m_primary) return S_OK;
        if (g_writes)
        {
            std::lock_guard<std::mutex> l(g_card.mutex);
            double t = nowSec();
            uint32_t off = uint32_t(static_cast<uint8_t*>(p1) - &g_card.ring[0]);
            uint32_t n = b1 + (p2 ? b2 : 0);
            unsigned long long played = g_card.playedLocked(t);
            std::fwrite(&t, 8, 1, g_writes);
            std::fwrite(&off, 4, 1, g_writes);
            std::fwrite(&n, 4, 1, g_writes);
            std::fwrite(&played, 8, 1, g_writes);
            std::fwrite(p1, 1, b1, g_writes);
            if (p2 && b2) std::fwrite(p2, 1, b2, g_writes);
        }
        return S_OK;
    }
    STDMETHOD(Restore)() { return S_OK; }

    bool         m_primary;
    ULONG        m_ref = 1;
    DWORD        m_flags = 0;
    WAVEFORMATEX m_fmt = {};
    LONG         m_volume = 0, m_pan = 0;
    DWORD        m_freq = 0;
    bool         m_playing = false, m_loop = false;
    DWORD        m_lockOff = 0, m_lockBytes = 0;
    unsigned long m_locks = 0, m_posCalls = 0;
};

class FakeDS : public IDirectSound
{
public:
    STDMETHOD(QueryInterface)(REFIID riid, void** out)
    {
        if (riid == IID_IUnknown || riid == IID_IDirectSound) { *out = this; AddRef(); return S_OK; }
        dslog("ds QI %08lx -> E_NOINTERFACE", riid.Data1);
        *out = 0;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return ++m_ref; }
    STDMETHOD_(ULONG, Release)() { ULONG r = --m_ref; dslog("ds Release -> %lu", r); return r; }
    STDMETHOD(CreateSoundBuffer)(LPCDSBUFFERDESC d, LPDIRECTSOUNDBUFFER* out, LPUNKNOWN)
    {
        const bool primary = (d->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0;
        FakeBuffer* b = new FakeBuffer(primary);
        b->m_flags = d->dwFlags;
        if (primary)
        {
            dslog("CreateSoundBuffer primary flags=%lx", d->dwFlags);
        }
        else
        {
            const WAVEFORMATEX* f = d->lpwfxFormat;
            dslog("CreateSoundBuffer flags=%lx bytes=%lu tag=%u ch=%u rate=%lu bits=%u align=%u",
                  d->dwFlags, d->dwBufferBytes, f->wFormatTag, f->nChannels, f->nSamplesPerSec,
                  f->wBitsPerSample, f->nBlockAlign);
            if (g_cardOpened)
                dslog("second secondary buffer: not captured");
            else
            {
                std::lock_guard<std::mutex> l(g_card.mutex);
                g_card.ring.assign(d->dwBufferBytes, 0);
                g_card.rate = int(f->nSamplesPerSec);
                g_card.channels = f->nChannels;
                g_card.blockAlign = f->nBlockAlign;
                b->m_fmt = *f;
                b->m_freq = f->nSamplesPerSec;
                g_cardOpened = true;
            }
        }
        *out = b;
        return S_OK;
    }
    STDMETHOD(GetCaps)(LPDSCAPS caps)
    {
        DWORD sz = caps->dwSize;
        std::memset(caps, 0, sz);
        caps->dwSize = sz;
        caps->dwFlags = DSCAPS_PRIMARY16BIT | DSCAPS_PRIMARYSTEREO | DSCAPS_SECONDARY16BIT
                        | DSCAPS_SECONDARYSTEREO | DSCAPS_CONTINUOUSRATE;
        caps->dwMinSecondarySampleRate = 100;
        caps->dwMaxSecondarySampleRate = 200000;
        caps->dwMaxHwMixingAllBuffers = 0;
        dslog("ds GetCaps");
        return S_OK;
    }
    STDMETHOD(DuplicateSoundBuffer)(LPDIRECTSOUNDBUFFER, LPDIRECTSOUNDBUFFER*) { dslog("Duplicate"); return E_NOTIMPL; }
    STDMETHOD(SetCooperativeLevel)(HWND, DWORD lvl) { dslog("SetCooperativeLevel %lu", lvl); return S_OK; }
    STDMETHOD(Compact)() { return S_OK; }
    STDMETHOD(GetSpeakerConfig)(LPDWORD c) { *c = DSSPEAKER_STEREO; return S_OK; }
    STDMETHOD(SetSpeakerConfig)(DWORD) { return S_OK; }
    STDMETHOD(Initialize)(LPCGUID) { return DSERR_ALREADYINITIALIZED; }
    ULONG m_ref = 1;
};
FakeDS g_fakeDS;

HMODULE g_dll = 0;
template <class T> void bind(T& slot, const char* name)
{
    slot = reinterpret_cast<T>(GetProcAddress(g_dll, name));
    if (!slot) { std::fprintf(stderr, "binkw32.dll lacks %s\n", name); std::exit(2); }
}
void loadApi()
{
    g_dll = LoadLibraryA("binkw32.dll");
    if (!g_dll) { std::fprintf(stderr, "binkw32.dll not found\n"); std::exit(2); }
    bind(api.Open, "_BinkOpen@8");
    bind(api.Close, "_BinkClose@4");
    bind(api.Wait, "_BinkWait@4");
    bind(api.DoFrame, "_BinkDoFrame@4");
    bind(api.NextFrame, "_BinkNextFrame@4");
    bind(api.CopyToBuffer, "_BinkCopyToBuffer@28");
    bind(api.GetSummary, "_BinkGetSummary@8");
    bind(api.Pause, "_BinkPause@8");
    bind(api.SetSoundSystem, "_BinkSetSoundSystem@8");
    bind(api.OpenDirectSound, "_BinkOpenDirectSound@4");
}

unsigned long long cardPlayedFrames()
{
    return g_cardOpened ? g_card.played() / unsigned(g_card.blockAlign) : 0;
}

bool selectSound(bool realDS)
{
    if (realDS)
        return api.SetSoundSystem(reinterpret_cast<void*>(api.OpenDirectSound), 0) != 0;
    g_dslog = openOut("ds.log", "w");
    g_writes = openOut("writes.bin");
    g_card.heard = openOut("heard.s16");
    g_card.launch();
    return api.SetSoundSystem(reinterpret_cast<void*>(api.OpenDirectSound),
                              unsigned(reinterpret_cast<uintptr_t>(static_cast<IDirectSound*>(&g_fakeDS)))) != 0;
}

#else   // port build
// ---------------------------------------------------------------------------------------------
// The port's audio goes through the same ideal card: binkPortSetAudioOutput installs an output
// whose "device" is g_card. The port renders float; the card stores interleaved s16 so the heard
// file has the format of the retail capture.
struct CardOutput : renddx9abi::BinkPortAudioOutput
{
    renddx9abi::BinkPortAudioSource* src = 0;
    std::thread pump;
    std::atomic<bool> quit{false};
    bool open(int sampleRate, int channels, renddx9abi::BinkPortAudioSource* source) override
    {
        if (g_cardOpened) return false;
        src = source;
        {
            std::lock_guard<std::mutex> l(g_card.mutex);
            g_card.rate = sampleRate;
            g_card.channels = channels;
            g_card.blockAlign = 2 * channels;
            g_card.ring.clear();   // heard is written by the pump below
        }
        g_cardOpened = true;
        pump = std::thread([this] { run(); });
        return true;
    }
    void setRunning(bool run) override { if (run) g_card.start(); else g_card.stop(); }
    void close() override
    {
        quit = true;
        if (pump.joinable()) pump.join();
    }
    unsigned long long playedFrames() override { return g_card.played() / unsigned(g_card.blockAlign); }
    void run()
    {
        std::vector<float> f;
        std::vector<int16_t> s;
        while (!quit)
        {
            unsigned long long upTo = g_card.played();
            unsigned long long have = g_card.captured;
            if (upTo > have)
            {
                const size_t frames = size_t((upTo - have) / unsigned(g_card.blockAlign));
                f.resize(frames * g_card.channels);
                s.resize(f.size());
                src->pull(f.data(), frames);
                for (size_t i = 0; i < f.size(); ++i)
                {
                    float v = f[i] * 32768.0f;
                    v = v > 32767.0f ? 32767.0f : (v < -32768.0f ? -32768.0f : v);
                    s[i] = int16_t(v < 0 ? v - 0.5f : v + 0.5f);
                }
                if (g_card.heard) std::fwrite(s.data(), 2, s.size(), g_card.heard);
                g_card.captured = upTo;
            }
            sleepMs(1);
        }
    }
};
CardOutput* g_portOut = 0;
renddx9abi::BinkPortAudioOutput* makeCardOutput() { return g_portOut = new CardOutput(); }

void loadApi()
{
    using namespace renddx9abi;
    api.Open = reinterpret_cast<void* (*)(const char*, unsigned)>(&BinkOpen);
    api.Close = reinterpret_cast<void (*)(void*)>(&BinkClose);
    api.Wait = reinterpret_cast<int (*)(void*)>(&BinkWait);
    api.DoFrame = reinterpret_cast<int (*)(void*)>(&BinkDoFrame);
    api.NextFrame = reinterpret_cast<void (*)(void*)>(&BinkNextFrame);
    api.CopyToBuffer = reinterpret_cast<void (*)(void*, void*, int, unsigned, unsigned, unsigned, unsigned)>(&BinkCopyToBuffer);
    api.GetSummary = reinterpret_cast<void (*)(void*, Summary*)>(&BinkGetSummary);
    api.Pause = reinterpret_cast<void (*)(void*, int)>(&BinkPause);
    api.SetSoundSystem = reinterpret_cast<int (*)(void*, unsigned)>(&BinkSetSoundSystem);
    api.OpenDirectSound = reinterpret_cast<void* (*)(unsigned)>(&BinkOpenDirectSound);
}

unsigned long long cardPlayedFrames()
{
    return g_cardOpened ? g_card.played() / unsigned(g_card.blockAlign) : 0;
}

bool selectSound(bool system)
{
    if (!system)
    {
        g_card.heard = openOut("heard.s16");
        renddx9abi::binkPortSetAudioOutput(&makeCardOutput);
    }
    return api.SetSoundSystem(reinterpret_cast<void*>(api.OpenDirectSound), 0) != 0;
}
#endif

BinkHead* head(void* h) { return static_cast<BinkHead*>(h); }

int modeFrames(int argc, char** argv)
{
    // frames <bik> <out> <surface> f...
    if (argc < 6) return 2;
    const unsigned surface = unsigned(std::atoi(argv[4]));
    std::vector<unsigned> want;
    unsigned last = 0;
    for (int i = 5; i < argc; ++i) { want.push_back(unsigned(std::atoi(argv[i]))); last = std::max(last, want.back()); }
    void* h = api.Open(argv[2], 0);
    if (!h) { std::fprintf(stderr, "BinkOpen failed\n"); return 1; }
    BinkHead* b = head(h);
    std::printf("open %ux%u frames=%u framenum=%u\n", b->Width, b->Height, b->Frames, b->FrameNum);
    const unsigned w = b->Width, hh = b->Height;
    int pitch = int(w) * 4;
    size_t bytes = size_t(pitch) * hh;
    if (surface == 13 || surface == 14) { pitch = int(w) * 2; bytes = size_t(pitch) * hh; }
    if (surface == 15) { pitch = int(w); bytes = size_t(w) * hh * 3 / 2; }
    std::vector<uint8_t> buf(bytes + 64);
    for (unsigned n = 1; n <= last; ++n)
    {
        api.DoFrame(h);
        for (unsigned f : want)
            if (f == n)
            {
                std::memset(buf.data(), 0xCD, buf.size());
#if !defined(BINK_HARNESS_RETAIL)
                // The port converts only to BGRx (the surface Video.cpp uses); for the planar
                // dump it writes its decoded picture in YV12 order (Y, V, U) instead.
                if (surface == 15)
                {
                    FFBinkPicture pic;
                    if (renddx9abi::binkPortPicture(static_cast<renddx9abi::HBINK>(h), &pic))
                    {
                        uint8_t* o = buf.data();
                        for (unsigned y = 0; y < hh; ++y, o += w)
                            std::memcpy(o, pic.plane[0] + size_t(y) * pic.linesize[0], w);
                        for (int pl : {2, 1})
                            for (unsigned y = 0; y < hh / 2; ++y, o += w / 2)
                                std::memcpy(o, pic.plane[pl] + size_t(y) * pic.linesize[pl], w / 2);
                    }
                }
                else
#endif
                api.CopyToBuffer(h, buf.data(), pitch, hh, 0, 0, 0x80000000u | surface);
                char name[64];
                std::snprintf(name, sizeof(name), "f%u_s%u.raw", n, surface);
                FILE* o = openOut(name);
                std::fwrite(buf.data(), 1, bytes, o);
                std::fclose(o);
                std::printf("wrote %s (%zu bytes, framenum %u)\n", name, bytes, b->FrameNum);
            }
        api.NextFrame(h);
    }
    api.Close(h);
    return 0;
}

struct Event { unsigned frame; double ms; bool done; };

int modePlay(int argc, char** argv)
{
    if (argc < 4) return 2;
    const char* bik = argv[2];
    bool sound = false, realDS = false;
    double hz = 60, seconds = 600;
    int loops = 1;
    double delayMs = 0;
    std::vector<Event> stalls, pauses;
    for (int i = 4; i < argc; ++i)
    {
        std::string a = argv[i];
        auto ev = [&](std::vector<Event>& v) {
            unsigned f = 0; double ms = 0;
            std::sscanf(argv[++i], "%u:%lf", &f, &ms);
            v.push_back({f, ms, false});
        };
        if (a == "--sound") sound = true;
        else if (a == "--realds" || a == "--sysaudio") realDS = true;
        else if (a == "--hz") hz = std::atof(argv[++i]);
        else if (a == "--seconds") seconds = std::atof(argv[++i]);
        else if (a == "--loops") loops = std::atoi(argv[++i]);
        else if (a == "--cardrate") g_card.speed = std::atof(argv[++i]);
        else if (a == "--delay") delayMs = std::atof(argv[++i]);
        else if (a == "--stall") ev(stalls);
        else if (a == "--pause") ev(pauses);
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }
    if (sound && !selectSound(realDS))
        std::fprintf(stderr, "BinkSetSoundSystem answered 0\n");
    const double tOpen0 = nowSec();
    void* h = api.Open(bik, 0);
    const double tOpen1 = nowSec();
    if (!h) { std::fprintf(stderr, "BinkOpen failed\n"); return 1; }
    BinkHead* b = head(h);
    FILE* log = openOut("video.csv", "w");
    std::fprintf(log, "# %s %ux%u frames=%u open_ms=%.3f hz=%.1f cardrate=%.3f\n", bik, b->Width,
                 b->Height, b->Frames, (tOpen1 - tOpen0) * 1000.0, hz, g_card.speed);
    std::fprintf(log, "t_ms,before,after,doframe,waits,heard_frames,event\n");
    std::vector<uint8_t> buf(size_t(b->Width) * b->Height * 4);
    sleepMs(delayMs);
    const double t0 = nowSec();
    std::fprintf(log, "# first wait %.3f ms after open\n", (t0 - tOpen1) * 1000.0);
    const double period = 1.0 / hz;
    double next = t0;
    int wraps = 0;
    unsigned waits = 0;
    while (nowSec() - t0 < seconds)
    {
        if (!api.Wait(h))
        {
            const unsigned before = b->FrameNum;
            const int r = api.DoFrame(h);
            api.CopyToBuffer(h, buf.data(), int(b->Width) * 4, b->Height, 0, 0, 0x80000003u);
            api.NextFrame(h);
            const unsigned after = b->FrameNum;
            const double t = nowSec();
            std::fprintf(log, "%.3f,%u,%u,%d,%u,%llu,\n", (t - t0) * 1000.0, before, after, r, waits,
                         cardPlayedFrames());
            waits = 0;
            if (after < before && ++wraps >= loops)
                break;
            for (Event& e : stalls)
                if (!e.done && e.frame == before)
                {
                    e.done = true;
                    std::fprintf(log, "%.3f,%u,%u,,,%llu,stall %.0f\n", (nowSec() - t0) * 1000.0, after,
                                 after, cardPlayedFrames(), e.ms);
                    sleepMs(e.ms);
                    std::fprintf(log, "%.3f,%u,%u,,,%llu,stall end\n", (nowSec() - t0) * 1000.0, after,
                                 after, cardPlayedFrames());
                    next = nowSec();
                }
            for (Event& e : pauses)
                if (!e.done && e.frame == before)
                {
                    e.done = true;
                    api.Pause(h, 1);
                    std::fprintf(log, "%.3f,%u,%u,,,%llu,pause %.0f\n", (nowSec() - t0) * 1000.0, after,
                                 after, cardPlayedFrames(), e.ms);
                    // keep the game loop running (Video::doFrame returns while paused)
                    const double until = nowSec() + e.ms / 1000.0;
                    while (nowSec() < until) sleepMs(period * 1000.0);
                    api.Pause(h, 0);
                    std::fprintf(log, "%.3f,%u,%u,,,%llu,resume\n", (nowSec() - t0) * 1000.0, after,
                                 after, cardPlayedFrames());
                    next = nowSec();
                }
        }
        else
            ++waits;
        next += period;
        const double now = nowSec();
        if (next > now) sleepMs((next - now) * 1000.0);
        else next = now;
    }
    const double tEnd = nowSec();
    // Let the audio tail play out, then summarise.
    if (sound) sleepMs(300);
    Summary s;
    std::memset(&s, 0, sizeof(s));
    api.GetSummary(h, &s);
    FILE* sum = openOut("summary.txt", "w");
    static const char* names[29] = {"Width", "Height", "TotalTime", "FileFrameRate", "FileFrameRateDiv",
        "FrameRate", "FrameRateDiv", "TotalOpenTime", "TotalFrames", "TotalPlayedFrames", "SkippedFrames",
        "SkippedBlits", "SoundSkips", "TotalBlitTime", "TotalReadTime", "TotalVideoDecompTime",
        "TotalAudioDecompTime", "TotalReadSpeed", "SlowestFrameTime", "Slowest2FrameTime", "SlowestFrameNum",
        "Slowest2FrameNum", "AverageDataRate", "AverageFrameSize", "HighestMemAmount", "TotalIOMemory",
        "HighestIOUsed", "Highest1SecRate", "Highest1SecFrame"};
    for (int i = 0; i < 29; ++i) std::fprintf(sum, "%s %u\n", names[i], s.v[i]);
    std::fprintf(sum, "wall_ms %.3f\nfinal_framenum %u\ncard_rate %d\ncard_channels %d\ncard_frames %llu\n",
                 (tEnd - t0) * 1000.0, b->FrameNum, g_card.rate, g_card.channels, cardPlayedFrames());
    std::fclose(sum);
    std::fclose(log);
    api.Close(h);
    g_card.finish();
#if defined(BINK_HARNESS_RETAIL)
    if (g_writes) std::fclose(g_writes);
    if (g_dslog) std::fclose(g_dslog);
#endif
    std::printf("done: %d wraps, final framenum %u, %.1f s\n", wraps, b->FrameNum, tEnd - t0);
    return 0;
}

#if !defined(BINK_HARNESS_RETAIL)
FILE* g_ref = 0;
FILE* g_reff = 0;
void refTap(void*, const float* const* planes, int channels, int samples, int)
{
    for (int i = 0; i < samples; ++i)
        for (int c = 0; c < channels; ++c)
            std::fwrite(&planes[c][i], 4, 1, g_reff);
    std::vector<int16_t> s(size_t(samples) * channels);
    for (int i = 0; i < samples; ++i)
        for (int c = 0; c < channels; ++c)
        {
            float v = planes[c][i] * 32768.0f;
            v = v > 32767.0f ? 32767.0f : (v < -32768.0f ? -32768.0f : v);
            s[size_t(i) * channels + c] = int16_t(v < 0 ? v - 0.5f : v + 0.5f);
        }
    std::fwrite(s.data(), 2, s.size(), g_ref);
}

int modeDecodeAudio(int argc, char** argv)
{
    (void)argc;
    g_ref = openOut("ref.s16");
    g_reff = openOut("ref.f32");
    renddx9abi::binkPortSetAudioTap(&refTap, 0);
    void* h = api.Open(argv[2], 0);
    if (!h) return 1;
    BinkHead* b = head(h);
    const unsigned frames = b->Frames;
    for (unsigned n = 0; n < frames; ++n)
    {
        api.DoFrame(h);
        if (n + 1 < frames)
            api.NextFrame(h);   // not past the last frame: the wrap would restart the sound
    }
    api.Close(h);
    renddx9abi::binkPortSetAudioTap(0, 0);
    std::fclose(g_ref);
    std::fclose(g_reff);
    return 0;
}
#endif

} // namespace

int main(int argc, char** argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: bink_harness frames|play <bik> <out> ...\n");
        return 2;
    }
#if defined(BINK_HARNESS_RETAIL)
    timeBeginPeriod(1);
#endif
    g_out = argv[3];
    loadApi();
    std::string mode = argv[1];
    int r = 2;
    if (mode == "frames") r = modeFrames(argc, argv);
    else if (mode == "play") r = modePlay(argc, argv);
#if !defined(BINK_HARNESS_RETAIL)
    else if (mode == "decodeaudio") r = modeDecodeAudio(argc, argv);
#endif
    if (r == 2) std::fprintf(stderr, "bad arguments\n");
    std::fflush(0);
#if defined(BINK_HARNESS_RETAIL)
    // binkw32's service threads can keep the process alive at exit under Wine; every output is
    // closed by now.
    TerminateProcess(GetCurrentProcess(), UINT(r));
#endif
    return r;
}
