// [CHOSEN, not recovered] The Bink 1 API subset Video.cpp calls (Bink.h), implemented for the macOS
// client and Windows x64 on the vendored FFmpeg 7.1.1 Bink decoders (Code/BF2/External/ffmpeg-bink,
// LGPL-2.1+). Owner decisions 2026-11-01 and 2026-11-07 (Code/BF2/External/ffmpeg-bink/README.md): play the shipped
// .bik movies natively; Windows x86 keeps RAD's binkw32.dll, which is not recovered.
//
// This file is the platform-neutral part: the .bik container reader, decoding, the movie clock and
// the colour conversion. The sound device is a BinkPortAudioOutput (BinkAudioCoreAudio.cpp on
// macOS, BinkAudioDirectSound.cpp on Windows x64). It is port code: the clock and the conversion
// reproduce what the black-box harness (Code/Tests/movies, Code/BF2/External/ffmpeg-bink/README.md) measured on
// retail binkw32.dll through its exported API, not binkw32's internals.

#include "Bink.h"
#include "BinkPort.h"

#if BF2_BINK_PORT

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace renddx9abi {

namespace {

const uint32_t kAudioUseDct  = 0x1000;   // header track flags
const uint32_t kAudioStereo  = 0x2000;
const unsigned kMaxTracks    = 256;
const unsigned kSurfaceMask  = 0x0F;     // BinkCopyToBuffer surface type bits
// Header video flag: the file stores luma only (FFmpeg's BINK_FLAG_GRAY). Both menu movies
// (menu.bik, menu_loggedin.bik) set it. FFmpeg's decoder leaves the chroma planes at 0 for such
// files, which would convert to saturated green; retail binkw32 shows them grey (harness: its
// BGRx output has R = G = B on every pixel of menu_loggedin.bik).
const uint32_t kVideoGray    = 0x00020000;

uint32_t rl32(const uint8_t* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

[[noreturn]] void binkFrontier(const char* what)
{
    std::fprintf(stderr, "[FRONTIER] renddx9abi::%s\n", what);
    std::fflush(stderr);
    std::abort();
}

inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

double nowSeconds()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

BinkPortAudioTap           g_tap = 0;
void*                      g_tapUser = 0;
bool                       g_soundSelected = false;   // BinkSetSoundSystem succeeded
BinkPortAudioOutputFactory g_outputFactory = 0;

// One movie's decoded audio between the decoder (main thread, BinkDoFrame) and the output's
// thread. An underrun while the movie still has audio to come marks the ring starved; BinkWait
// then restarts the sound, as binkw32 does when its DirectSound buffer runs dry.
class AudioRing : public BinkPortAudioSource
{
public:
    void init(int channels, int sampleRate)
    {
        m_channels = channels;
        m_buf.assign(size_t(sampleRate) * size_t(channels) * 4, 0.0f);   // 4 s of slack
        reset();
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_head = m_count = 0;
        m_primed = false;
        m_starved = false;
    }

    void push(const float* const* planes, int channels, int samples)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const size_t cap = m_buf.size();
        for (int s = 0; s < samples; ++s)
        {
            if (m_count + size_t(m_channels) > cap)
                break;   // cannot happen with the read-ahead limit below; drop rather than block
            for (int c = 0; c < m_channels; ++c)
            {
                m_buf[(m_head + m_count) % cap] = planes[std::min(c, channels - 1)][s];
                ++m_count;
            }
        }
        m_primed = true;
    }

    // Queues `frames` frames of silence and clears the starved mark.
    void pushSilence(size_t frames)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const size_t cap = m_buf.size();
        const size_t n = std::min(frames * size_t(m_channels), cap - m_count);
        for (size_t i = 0; i < n; ++i)
            m_buf[(m_head + m_count + i) % cap] = 0.0f;
        m_count += n;
        m_starved = false;
    }

    // Frames buffered (decoded, not yet pulled by the output).
    size_t frames()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count / size_t(m_channels);
    }

    void setExhausted(bool done) { m_exhausted = done; }
    bool starved() const { return m_starved; }

    size_t pull(float* out, size_t frames) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const size_t cap = m_buf.size();
        const size_t want = frames * size_t(m_channels);
        const size_t n = std::min(want, m_count);
        for (size_t i = 0; i < n; ++i)
            out[i] = m_buf[(m_head + i) % cap];
        std::fill(out + n, out + want, 0.0f);    // underrun plays silence
        m_head = (m_head + n) % cap;
        m_count -= n;
        if (n < want && m_primed && !m_exhausted)
            m_starved = true;
        return n / size_t(m_channels);
    }

private:
    std::mutex         m_mutex;
    std::vector<float> m_buf;
    size_t             m_head = 0, m_count = 0;
    int                m_channels = 2;
    bool               m_primed = false;
    std::atomic<bool>  m_exhausted{false};
    std::atomic<bool>  m_starved{false};
};

struct AudioTrack
{
    int          sampleRate = 0;
    int          channels = 1;
    bool         dct = false;
    FFBinkAudio* decoder = 0;
};

// BINK must stay the first base: Video.cpp reads Width/Height/Frames/FrameNum off the handle.
struct BinkHandle : BINK
{
    std::FILE*             file = 0;
    std::string            path;
    uint32_t               tag = 0;
    uint32_t               fpsNum = 1, fpsDen = 1;
    double                 period = 0.0;      // seconds per frame
    std::vector<uint32_t>  offset;            // Frames + 1 entries, keyframe bit cleared
    std::vector<uint8_t>   keyframe;
    std::vector<AudioTrack> tracks;
    FFBinkVideo*           video = 0;
    FFBinkPicture          picture;
    bool                   havePicture = false;
    bool                   gray = false;      // kVideoGray: neutral chroma in BinkCopyToBuffer
    unsigned               nextDecode = 0;    // 0-based frame the decoder state expects next
    std::vector<uint8_t>   packet;
    std::vector<uint8_t>   audioPacket;
    // Sound (track 0, when a sound system was selected).
    BinkPortAudioOutput*   audio = 0;
    AudioRing              ring;
    unsigned               audioNext = 0;     // 0-based frame whose audio packet decodes next
    std::vector<unsigned long long> audioStart;   // Frames + 1: track 0 samples before each frame
    unsigned               soundSkips = 0;
    // Clock: `due` is the wall time at which FrameNum may be shown; `rebase`, when set by
    // BinkWait, replaces the next frame's due time (a late frame drops its lag).
    double                 due = 0.0;
    double                 rebase = -1.0;
    bool                   started = false;   // clock and sound start at the first Wait/DoFrame
    unsigned long long     shown = 0;         // frames advanced by BinkNextFrame
    bool                   paused = false;
};

bool readFrame(BinkHandle* h, unsigned index, std::vector<uint8_t>& into)
{
    const uint32_t begin = h->offset[index], end = h->offset[index + 1];
    const size_t size = end - begin;
    into.resize(size + FFBINK_PACKET_PADDING);
    if (std::fseek(h->file, long(begin), SEEK_SET) != 0
        || std::fread(into.data(), 1, size, h->file) != size)
        return false;
    std::memset(into.data() + size, 0, FFBINK_PACKET_PADDING);
    return true;
}

void audioSink(void* user, const float* const* planes, int channels, int samples)
{
    BinkHandle* h = static_cast<BinkHandle*>(user);
    if (g_tap)
        g_tap(g_tapUser, planes, channels, samples, h->tracks[0].sampleRate);
    else if (h->audio)
        h->ring.push(planes, channels, samples);
}

bool wantAudio(const BinkHandle* h)
{
    return !h->tracks.empty() && h->tracks[0].decoder && (g_tap || h->audio);
}

// Decodes track 0's packet of frame `index` into the ring (or the tap).
bool decodeAudio(BinkHandle* h, unsigned index)
{
    if (!readFrame(h, index, h->audioPacket))
        return false;
    const size_t total = h->offset[index + 1] - h->offset[index];
    if (total < 4)
        return false;
    const uint32_t size = rl32(&h->audioPacket[0]);
    if (size > total - 4)
        return false;
    if (size >= 4)
    {
        std::memmove(h->audioPacket.data(), h->audioPacket.data() + 4, size);
        std::memset(h->audioPacket.data() + size, 0, FFBINK_PACKET_PADDING);
        ffbink_audio_decode(h->tracks[0].decoder, h->audioPacket.data(), int(size), audioSink, h);
    }
    return true;
}

// binkw32 decodes a frame's audio with the frame: the sound runs ahead of the picture by the
// audio the file interleaves before it, never by more. The port does the same, so the sound runs
// dry when the picture falls behind by that lead (harness: Dice.bik at a 20 Hz game loop restarts
// its sound at about 0.67 s of lag, as retail does).
void decodeAudioThrough(BinkHandle* h, unsigned index)
{
    if (!wantAudio(h))
        return;
    while (h->audioNext <= index && h->audioNext < h->Frames)
    {
        if (!decodeAudio(h, h->audioNext))
            break;
        ++h->audioNext;
    }
    // Exhausted once no later frame carries audio (Dice.bik's last four frames have none).
    h->ring.setExhausted(h->audioNext >= h->Frames || h->audioStart.empty()
                         || h->audioStart[h->audioNext] >= h->audioStart[h->Frames]);
}

// Splits one frame into its per-track audio packets and the video packet and decodes the video.
bool decodeVideo(BinkHandle* h, unsigned index)
{
    if (!readFrame(h, index, h->packet))
    {
        std::fprintf(stderr, "[bink] %s: frame %u could not be read\n", h->path.c_str(), index);
        return false;
    }
    const size_t total = h->offset[index + 1] - h->offset[index];
    size_t pos = 0;
    for (size_t t = 0; t < h->tracks.size(); ++t)
    {
        if (pos + 4 > total)
            return false;
        const uint32_t size = rl32(&h->packet[pos]);
        pos += 4;
        if (size > total - pos)
            return false;
        pos += size;
    }
    const int vsize = int(total - pos);
    std::memmove(h->packet.data(), h->packet.data() + pos, size_t(vsize));
    std::memset(h->packet.data() + vsize, 0, FFBINK_PACKET_PADDING);
    if (ffbink_video_decode(h->video, h->packet.data(), vsize, &h->picture) < 0)
    {
        std::fprintf(stderr, "[bink] %s: frame %u failed to decode\n", h->path.c_str(), index);
        return false;
    }
    h->havePicture = true;
    h->nextDecode = index + 1;
    return true;
}

// Track 0's sample position at the start of every frame, from each audio packet's leading dword
// (its decoded size in 16-bit bytes; frame 0 carries the lead, about 0.78 s in BF2's movies,
// then one 1920-sample block per frame that has audio).
void buildAudioIndex(BinkHandle* h)
{
    const AudioTrack& t = h->tracks[0];
    const unsigned bytesPerFrame = 2u * unsigned(t.channels);
    h->audioStart.assign(h->Frames + 1, 0);
    unsigned long long pos = 0;
    for (unsigned i = 0; i < h->Frames; ++i)
    {
        h->audioStart[i] = pos;
        uint8_t p[8];
        if (std::fseek(h->file, long(h->offset[i]), SEEK_SET) != 0 || std::fread(p, 1, 8, h->file) != 8)
            continue;
        if (rl32(p) >= 4)
            pos += rl32(p + 4) / bytesPerFrame;
    }
    h->audioStart[h->Frames] = pos;
}

// The loop wrap: the ring is emptied and track 0 decodes again from frame 0's packet (at the next
// BinkDoFrame), as the movie began (retail: Stop, SetCurrentPosition(0), Play on its sound
// buffer). The emptied ring is not primed, so the silence until then is no underrun.
void restartSound(BinkHandle* h)
{
    if ((!h->audio && !g_tap) || h->tracks.empty() || !h->tracks[0].decoder)
        return;
    ffbink_audio_flush(h->tracks[0].decoder);
    h->ring.reset();
    h->audioNext = 0;
    h->ring.setExhausted(false);
}

// After the sound ran dry (the picture fell behind by the audio lead, or the game stalled):
// retail restarts its sound buffer, plays silence for about the lead (0.75 s in Dice.bik) and
// then continues the audio stream where it ran out, so that the next sample sounds at its own
// frame's time again (harness: Dice.bik at a 20 Hz loop and after a 2 s stall). The port queues
// the same gap: the next undecoded sample is due (its time - the current frame's time) from now.
void resumeSoundAfterGap(BinkHandle* h)
{
    const int rate = h->tracks[0].sampleRate;
    const unsigned frame = h->FrameNum - 1;
    const double next = double(h->audioStart[std::min(h->audioNext, h->Frames)]) / rate
                        + double(h->ring.frames()) / rate;   // ring is (nearly) empty here
    const double gap = next - double(frame) * h->period;
    h->ring.pushSilence(gap > 0.0 ? size_t(gap * rate) : 0);
    decodeAudioThrough(h, frame);
}

// binkw32 starts its sound buffer and its clock at the first BinkWait, not in BinkOpen (harness:
// with 500 ms between BinkOpen and the first BinkWait, DirectSound Play came 516 ms after
// CreateSoundBuffer and the movie still took 6200 ms from the first frame, sound in sync).
void startClock(BinkHandle* h)
{
    if (h->started || h->paused)
        return;
    h->started = true;
    h->due = nowSeconds();
    if (h->audio)
        h->audio->setRunning(true);
}

void closeHandle(BinkHandle* h)
{
    if (!h)
        return;
    if (h->audio)
    {
        // One line per movie with sound: how much the device actually played.
        std::fprintf(stderr, "[bink] %s: closed after %llu frames; sound played %.1f s, %u restart(s)\n",
                     h->path.c_str(), h->shown,
                     double(h->audio->playedFrames()) / h->tracks[0].sampleRate, h->soundSkips);
        h->audio->close();
        delete h->audio;
    }
    for (AudioTrack& t : h->tracks)
        ffbink_audio_close(t.decoder);
    ffbink_video_close(h->video);
    if (h->file)
        std::fclose(h->file);
    delete h;
}

} // anonymous namespace

bool binkPortPicture(HBINK bnk, FFBinkPicture* out)
{
    BinkHandle* h = static_cast<BinkHandle*>(bnk);
    if (!h || !h->havePicture)
        return false;
    *out = h->picture;
    return true;
}

void binkPortSetAudioTap(BinkPortAudioTap tap, void* user)
{
    g_tap = tap;
    g_tapUser = user;
}

void binkPortSetAudioOutput(BinkPortAudioOutputFactory factory)
{
    g_outputFactory = factory;
}

extern "C" {

// Answers 0 for a missing or unreadable file, as binkw32 does; Video::open walks its search list.
HBINK BinkOpen(const char* name, unsigned int flags)
{
    (void)flags;   // Video passes 0
    if (!name)
        return 0;
    std::string hostPath(name);
#if !defined(_WIN32)
    // binkw32 opens through the Win32 file API, where '\\' separates directories; the intro list
    // asks for "Movies\\EA.bik". POSIX needs '/'.
    std::replace(hostPath.begin(), hostPath.end(), '\\', '/');
#endif
    std::FILE* f = std::fopen(hostPath.c_str(), "rb");
    if (!f)
        return 0;
    BinkHandle* h = new BinkHandle();
    h->file = f;
    h->path = name;
    std::memset(&h->picture, 0, sizeof(h->picture));

    uint8_t hdr[44];
    if (std::fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
    {
        closeHandle(h);
        return 0;
    }
    h->tag = rl32(hdr);
    const char rev = char(hdr[3]);
    const bool bik1 = hdr[0] == 'B' && hdr[1] == 'I' && hdr[2] == 'K'
                      && std::strchr("bfghik", rev) && rev;
    const uint32_t fileSize = rl32(hdr + 4) + 8;
    const uint32_t frames   = rl32(hdr + 8);
    h->Width  = rl32(hdr + 20);
    h->Height = rl32(hdr + 24);
    h->fpsNum = rl32(hdr + 28);
    h->fpsDen = rl32(hdr + 32);
    const uint32_t videoFlags = rl32(hdr + 36);
    const uint32_t numTracks  = rl32(hdr + 40);
    h->gray = (videoFlags & kVideoGray) != 0;
    if (!bik1 || !frames || frames > 1000000 || !h->Width || !h->Height || h->Width > 7680
        || h->Height > 4800 || !h->fpsNum || !h->fpsDen || numTracks > kMaxTracks)
    {
        std::fprintf(stderr, "[bink] %s: not a supported Bink 1 file (tag %.4s)\n", name,
                     reinterpret_cast<const char*>(hdr));
        closeHandle(h);
        return 0;
    }
    h->period = double(h->fpsDen) / double(h->fpsNum);
    if (rev == 'k')
        std::fseek(f, 4, SEEK_CUR);   // unknown field of the 'k' revision
    if (numTracks)
    {
        std::vector<uint8_t> t(numTracks * 12);
        if (std::fread(t.data(), 1, t.size(), f) != t.size())
        {
            closeHandle(h);
            return 0;
        }
        h->tracks.resize(numTracks);
        for (uint32_t i = 0; i < numTracks; ++i)
        {
            const uint8_t* p = &t[numTracks * 4 + i * 4];   // after the max-decoded-size dwords
            h->tracks[i].sampleRate = int(p[0] | p[1] << 8);
            const uint32_t tf = uint32_t(p[2] | p[3] << 8);
            h->tracks[i].channels = (tf & kAudioStereo) ? 2 : 1;
            h->tracks[i].dct = (tf & kAudioUseDct) != 0;
        }
    }
    std::vector<uint8_t> idx(size_t(frames) * 4);
    if (std::fread(idx.data(), 1, idx.size(), f) != idx.size())
    {
        closeHandle(h);
        return 0;
    }
    h->offset.resize(frames + 1);
    h->keyframe.resize(frames);
    for (uint32_t i = 0; i < frames; ++i)
    {
        const uint32_t v = rl32(&idx[i * 4]);
        h->offset[i] = v & ~1u;
        h->keyframe[i] = (i == 0) || (v & 1u);
    }
    h->offset[frames] = fileSize;
    for (uint32_t i = 0; i < frames; ++i)
        if (h->offset[i + 1] <= h->offset[i])
        {
            std::fprintf(stderr, "[bink] %s: invalid frame index\n", name);
            closeHandle(h);
            return 0;
        }

    h->video = ffbink_video_open(h->tag, videoFlags, int(h->Width), int(h->Height));
    if (!h->video)
    {
        std::fprintf(stderr, "[bink] %s: video decoder refused the stream\n", name);
        closeHandle(h);
        return 0;
    }
    h->Frames   = frames;
    h->FrameNum = 1;
    // Bink plays the first track; only a selected sound system (or the test tap) gets samples.
    if (!h->tracks.empty() && (g_soundSelected || g_tap))
    {
        AudioTrack& t = h->tracks[0];
        t.decoder = ffbink_audio_open(h->tag, t.sampleRate, t.channels, t.dct ? 1 : 0);
        if (!t.decoder)
            binkFrontier("BinkOpen: the movie's first audio track uses a Bink audio variant the "
                         "vendored decoder subset does not include (RDFT)");
        buildAudioIndex(h);
        if (!g_tap)
        {
            h->ring.init(t.channels, t.sampleRate);
            h->audio = g_outputFactory ? g_outputFactory() : binkPortCreateSystemAudioOutput();
            if (!h->audio || !h->audio->open(t.sampleRate, t.channels, &h->ring))
            {
                // [CHOSEN] binkw32 also plays a movie without sound when its mixer cannot open.
                std::fprintf(stderr, "[bink] %s: sound output unavailable, playing silent\n", name);
                if (h->audio) { h->audio->close(); delete h->audio; }
                h->audio = 0;
            }
            else
                std::fprintf(stderr, "[bink] %s: sound %d Hz, %d channel(s)\n", name, t.sampleRate,
                             t.channels);
        }
        decodeAudioThrough(h, 0);
    }
    return h;
}

void BinkClose(HBINK bnk)
{
    closeHandle(static_cast<BinkHandle*>(bnk));
}

// Nonzero while the clock has not reached FrameNum. Measured on retail (Code/BF2/External/ffmpeg-bink/README.md):
// - the picture runs on the wall clock, not on the sound device's position (a card running 10 %
//   fast or slow does not change the movie's length);
// - with sound, a late picture catches up: Wait answers 0 on every call until it is on time;
// - if the sound ran dry meanwhile, the sound restarts at the current frame, the frame shows at
//   once and the next one is due two periods later;
// - without sound, a frame found more than about two periods late (2P - 2 ms; retail rebased at
//   65.7 ms and not at 62 ms, 30 fps) shows at once and the next one is due two periods after
//   this call: the lag is dropped, not caught up.
int BinkWait(HBINK bnk)
{
    BinkHandle* h = static_cast<BinkHandle*>(bnk);
    if (h->paused)
        return 1;
    startClock(h);
    const double now = nowSeconds();
    if (h->audio && h->ring.starved())
    {
        ++h->soundSkips;
        resumeSoundAfterGap(h);
        h->rebase = now + 2.0 * h->period - 0.001;
        return 0;
    }
    if (now < h->due)
        return 1;
    if (!h->audio && now - h->due > 2.0 * h->period - 0.002)
        h->rebase = now + 2.0 * h->period - 0.001;
    return 0;
}

// Decodes FrameNum (1-based). Sequential playback decodes one frame; anything else restarts from
// the nearest keyframe at or before it.
int BinkDoFrame(HBINK bnk)
{
    BinkHandle* h = static_cast<BinkHandle*>(bnk);
    startClock(h);
    const unsigned target = h->FrameNum - 1;
    if (target >= h->Frames)
        return 0;
    if (target != h->nextDecode)
    {
        unsigned k = target;
        while (k > 0 && !h->keyframe[k])
            --k;
        if (h->nextDecode > target || h->nextDecode < k)
        {
            if (k == 0)
                ffbink_video_flush(h->video);
            h->nextDecode = k;
        }
        while (h->nextDecode < target)
            if (!decodeVideo(h, h->nextDecode))
                return 0;
    }
    decodeVideo(h, target);
    decodeAudioThrough(h, target);
    return 0;
}

// Advances to the next frame and schedules it. After the last frame Bink wraps to frame 1, which
// loops menu movies; retail restarts its sound buffer there (Stop, SetCurrentPosition(0), Play).
void BinkNextFrame(HBINK bnk)
{
    BinkHandle* h = static_cast<BinkHandle*>(bnk);
    const bool wrap = h->FrameNum >= h->Frames;
    h->FrameNum = wrap ? 1 : h->FrameNum + 1;
    ++h->shown;
    if (h->rebase >= 0.0)
        h->due = h->rebase;
    else
        h->due += h->period;
    h->rebase = -1.0;
    if (wrap)
        restartSound(h);
}

} // extern "C"

// YUV 4:2:0 to BGRx (BINKSURFACE32), as measured on retail binkw32 (Code/Tests/movies harness and
// tools/bink/fit_colour.py, Code/BF2/External/ffmpeg-bink/README.md; fitted on 14 frames, 3.4 M pixels, and
// checked on 36 frames of all seven movies, which now match retail exactly in B, G and R):
// - its planar output equals FFmpeg's decoded planes, so only this conversion can differ;
// - chroma is not interpolated: pixel (x, y) uses U/V sample (x/2, y/2);
// - each channel is a luma term plus chroma terms, each from its own integer table:
//   Y' = max(0, floor((Y - 16) * cy / 65536)) (clamped below only: for Y < 16 retail adds the
//   chroma terms to 0; above 235 it keeps the unclamped value, e.g. 267 for Y = 246),
//   B = clamp(Y' + trunc(cbu * (U - 128) / 65536)),
//   R = clamp(Y' + trunc(crv * (V - 128) / 65536)),
//   G = clamp(Y' + trunc(-cgu * (U - 128) / 65536) + trunc(-cgv * (V - 128) / 65536)),
//   trunc rounding toward zero. Every retail pixel fits with cy in [76305, 76309], cbu in
//   [132202, 132221], crv in [104579, 104605], cgu in [25645, 25700], cgv in [53248, 53302]
//   (BT.601 limited range); the constants below are the band centres. The data covered U 91-193
//   and V 63-209.
// - a grayscale file (kVideoGray) has no chroma in its bitstream; retail's planar output gives
//   U = V = 128 for it, so B = G = R = Y'.
// Full-range 'k' files (none in BF2) are not measured: the same table scheme with JPEG constants.
// The x byte is 0 on retail; D3DFMT_X8R8G8B8 ignores it, and the port writes 0xFF because the
// Metal stand-in samples the texture as BGRA.
namespace {
struct ConversionTables
{
    int y[256], b[256], r[256], gu[256], gv[256];
    void build(bool fullRange)
    {
        const int cy  = fullRange ? 65536  : 76307;
        const int yo  = fullRange ? 0      : 16;
        const int cbu = fullRange ? 116130 : 132212;
        const int crv = fullRange ? 91881  : 104592;
        const int cgu = fullRange ? 22554  : 25672;
        const int cgv = fullRange ? 46802  : 53275;
        for (int i = 0; i < 256; ++i)
        {
            const int c = i - 128;
            y[i]  = std::max(0, int(std::floor(double((i - yo) * cy) / 65536.0)));
            b[i]  = (cbu * c) / 65536;    // C++ integer division truncates toward zero
            r[i]  = (crv * c) / 65536;
            gu[i] = (-cgu * c) / 65536;
            gv[i] = (-cgv * c) / 65536;
        }
    }
};

const ConversionTables& conversionTables(bool fullRange)
{
    static ConversionTables limited, full;
    static bool built = false;
    if (!built)
    {
        limited.build(false);
        full.build(true);
        built = true;
    }
    return fullRange ? full : limited;
}
} // namespace

extern "C" {

void BinkCopyToBuffer(HBINK bnk, void* dest, int destpitch, unsigned int destheight,
                      unsigned int destx, unsigned int desty, unsigned int flags)
{
    BinkHandle* h = static_cast<BinkHandle*>(bnk);
    if ((flags & kSurfaceMask) != BINK_SURFACE_32BGRA)
        binkFrontier("BinkCopyToBuffer: only the 32-bit BGRx surface Video.cpp uses is implemented");
    if (!h->havePicture || !dest || desty >= destheight)
        return;
    const FFBinkPicture& p = h->picture;
    const ConversionTables& t = conversionTables(p.full_range != 0);
    const unsigned rows = std::min<unsigned>(h->Height, destheight - desty);
    for (unsigned y = 0; y < rows; ++y)
    {
        uint8_t* out = static_cast<uint8_t*>(dest) + size_t(desty + y) * size_t(destpitch) + destx * 4;
        const uint8_t* py = p.plane[0] + size_t(y) * p.linesize[0];
        const uint8_t* pu = p.plane[1] + size_t(y >> 1) * p.linesize[1];
        const uint8_t* pv = p.plane[2] + size_t(y >> 1) * p.linesize[2];
        for (unsigned x = 0; x < h->Width; ++x)
        {
            const int Y = t.y[py[x]];
            const int U = h->gray ? 128 : pu[x >> 1];
            const int V = h->gray ? 128 : pv[x >> 1];
            out[0] = uint8_t(clamp255(Y + t.b[U]));
            out[1] = uint8_t(clamp255(Y + t.gu[U] + t.gv[V]));
            out[2] = uint8_t(clamp255(Y + t.r[V]));
            out[3] = 0xFF;
            out += 4;
        }
    }
}

void BinkGetSummary(HBINK bnk, BINKSUMMARY* sum)
{
    BinkHandle* h = static_cast<BinkHandle*>(bnk);
    std::memset(sum, 0, sizeof(*sum));
    sum->Width             = h->Width;
    sum->Height            = h->Height;
    sum->FileFrameRate     = h->fpsNum;
    sum->FileFrameRateDiv  = h->fpsDen;
    sum->FrameRate         = h->fpsNum;
    sum->FrameRateDiv      = h->fpsDen;
    sum->TotalFrames       = h->Frames;
    sum->TotalPlayedFrames = unsigned(h->shown);
    sum->SoundSkips        = h->soundSkips;
    sum->TotalTime         = unsigned(double(h->Frames) * h->fpsDen * 1000.0 / h->fpsNum);
}

// Measured on retail: pausing stops the sound buffer and resuming plays it again; the current
// frame becomes due 34-51 ms after the resume at 30 fps (the port takes 1.25 periods), and a lag
// from before the pause is dropped, not caught up.
// [CHOSEN] With sound, retail's resume is glitchy: binkw32 keeps filling its stopped buffer, so
// after the resume it plays newer audio (about 0.8 s of sound skipped), then restarts the sound
// and shows the picture about 0.3 s late. The port resumes the sound where it stopped, in sync.
void BinkPause(HBINK bnk, int pause)
{
    BinkHandle* h = static_cast<BinkHandle*>(bnk);
    const bool want = pause != 0;
    if (want == h->paused)
        return;
    h->paused = want;
    if (!h->started)
        return;   // the clock and the sound start at the first Wait after the resume
    if (!want)
        h->due = nowSeconds() + 1.25 * h->period;
    if (h->audio)
        h->audio->setRunning(!want);
}

// open is the BinkOpenDirectSound stand-in; a non-null answer selects Bink's sound output for
// movies opened afterwards, as binkw32 does.
int BinkSetSoundSystem(void* open, unsigned int param)
{
    typedef void* (*OpenFn)(unsigned int);
    if (!open)
        return 0;
    g_soundSelected = reinterpret_cast<OpenFn>(open)(param) != 0;
    return g_soundSelected ? 1 : 0;
}

// Selects the platform sound output (BinkPort.h); the device itself opens per movie in BinkOpen.
void* BinkOpenDirectSound(unsigned int param)
{
    (void)param;
    return reinterpret_cast<void*>(&BinkOpenDirectSound);
}

} // extern "C"

} // namespace renddx9abi

#endif // BF2_BINK_PORT
