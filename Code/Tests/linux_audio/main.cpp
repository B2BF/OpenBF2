// [TEMP] [CHOSEN, not recovered] SDL dummy-device lifecycle and real SDK codec checks.
// Synthetic PCM only: no microphone access, network or engine startup.
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <SDL3/SDL.h>
#include "../../RendDX9Abi/BinkPort.h"
#include "../../BF2/External/GameSpy/Voice2/gv.h"

static void require(bool ok, const char* label)
{
    if(!ok)
    {
        std::fprintf(stderr, "FAIL: %s (%s)\n", label, SDL_GetError());
        std::exit(1);
    }
}

class Tone : public renddx9abi::BinkPortAudioSource
{
public:
    std::atomic<unsigned long long> frames{0};
    size_t pull(float* buffer, size_t count) override
    {
        const auto first = frames.fetch_add(count);
        for(size_t i = 0; i < count; ++i)
            buffer[2*i] = buffer[2*i+1] = float(0.1 * std::sin((first+i) * 0.062689));
        return count;
    }
};

static GVDevice expectedCapture;
static GVDevice expectedPlayback;
static unsigned captureFilters;
static unsigned playbackFilters;
static void captureFilter(GVDevice device, GVSample*, GVFrameStamp)
{ require(device == expectedCapture, "capture filter receiver"); ++captureFilters; }
static void playbackFilter(GVDevice device, GVSample*, GVFrameStamp)
{ require(device == expectedPlayback, "playback filter receiver"); ++playbackFilters; }

int main(int argc, char** argv)
{
    Tone tone;
    std::unique_ptr<renddx9abi::BinkPortAudioOutput> output(renddx9abi::binkPortCreateSystemAudioOutput());
    if(argc == 2 && std::strcmp(argv[1], "--unavailable") == 0)
    {
        require(!output->open(48000, 2, &tone), "unavailable movie output refused");
        require(!gvStartup(), "unavailable voice output refused");
        gvCleanup();
        std::puts("PASS: unavailable audio does not report successful device startup");
        return 0;
    }
    require(!output->open(0, 2, &tone), "invalid movie rate refused");
    for(int repeat = 0; repeat < 3; ++repeat)
    {
        require(output->open(48000, 2, &tone), "movie output open");
        require(output->playedFrames() == 0, "movie initially paused");
        output->setRunning(true);
        SDL_Delay(150);
        const auto first = output->playedFrames();
        require(first > 0, "movie output clock advances");
        output->setRunning(false);
        const auto paused = output->playedFrames();
        SDL_Delay(60);
        require(output->playedFrames() == paused, "movie output clock freezes on pause");
        output->setRunning(true);
        SDL_Delay(100);
        require(output->playedFrames() > paused, "movie output clock resumes");
        output->close();
        const auto closed = tone.frames.load();
        SDL_Delay(30);
        require(tone.frames.load() == closed, "close quiesces movie source callbacks");
    }
    for(int repeat = 0; repeat < 3; ++repeat)
    {
        require(gvStartup(), "Voice2 startup");
        gvSetSampleRate(8000);
        require(gvSetCodec(repeat == 0 ? GVCodecRaw : GVCodecAverage), "voice codec setup");
        int samplesPerFrame = 0, encodedFrame = 0;
        gvGetCodecInfo(&samplesPerFrame, &encodedFrame, NULL);
        GVDeviceInfo info[2];
        const int count = gvListDevices(info, 2, GV_CAPTURE_AND_PLAYBACK);
        require(count == 2, "dummy input/output enumerated");
        expectedCapture = expectedPlayback = NULL;
        for(int i = 0; i < count; ++i)
        {
            require(!gvNewDevice(info[i].m_id, GV_CAPTURE_AND_PLAYBACK), "incompatible direction refused");
            GVDevice device = gvNewDevice(info[i].m_id, info[i].m_deviceType);
            require(device != NULL, "voice device open");
            require(!gvIsDeviceStarted(device, info[i].m_deviceType), "voice initially stopped");
            if(info[i].m_deviceType == GV_CAPTURE)
            {
                expectedCapture = device;
                gvSetCaptureThreshold(device, 0);
                gvSetFilter(device, GV_CAPTURE, captureFilter);
            }
            else
            {
                expectedPlayback = device;
                gvSetFilter(device, GV_PLAYBACK, playbackFilter);
            }
            require(gvStartDevice(device, info[i].m_deviceType), "voice starts");
        }
        const unsigned oldCapture = captureFilters, oldPlayback = playbackFilters;
        std::vector<GVByte> packet(size_t(encodedFrame) + 16, 0x5a);
        bool gotCapture = false;
        bool gotCaptureAfterChange = false;
        for(int tick = 0; tick < 40; ++tick)
        {
            if(tick == 20)
            {
                const GVCodec next = repeat == 0 ? GVCodecAverage
                                     : repeat == 1 ? GVCodecLowBandwidth : GVCodecRaw;
                require(gvSetCodec(next), "change active voice codec/quality");
                gvGetCodecInfo(&samplesPerFrame, &encodedFrame, NULL);
                packet.assign(size_t(encodedFrame) + 16, 0x5a);
            }
            gvThink();
            require(gvIsDeviceStarted(expectedCapture, GV_CAPTURE)
                    && gvIsDeviceStarted(expectedPlayback, GV_PLAYBACK), "voice devices remain started");
            int length = encodedFrame;
            GVFrameStamp stamp = 0;
            GVScalar volume = 1;
            if(gvCapturePacket(expectedCapture, packet.data(), &length, &stamp, &volume))
            {
                require(length == encodedFrame, "one encoded capture frame");
                require(volume == 0, "dummy input remains silent");
                for(int i = encodedFrame; i < encodedFrame + 16; ++i)
                    require(packet[i] == 0x5a, "capture packet respects caller capacity");
                gvPlayPacket(expectedPlayback, packet.data(), length, 1, stamp, GVFalse);
                gotCapture = true;
                gotCaptureAfterChange = gotCaptureAfterChange || tick > 20;
            }
            SDL_Delay(10);
        }
        require(gotCapture && gotCaptureAfterChange && captureFilters > oldCapture && playbackFilters > oldPlayback,
                "capture codec and playback filters exercised");
        gvStopDevice(expectedCapture, GV_CAPTURE);
        gvStopDevice(expectedPlayback, GV_PLAYBACK);
        require(!gvIsDeviceStarted(expectedCapture, GV_CAPTURE), "capture stops");
        require(gvGetAvailableCaptureBytes(expectedCapture) == 0, "stopped capture reports no packet");
        require(gvStartDevice(expectedCapture, GV_CAPTURE), "capture restarts");
        require(gvStartDevice(expectedPlayback, GV_PLAYBACK), "playback restarts");
        gvThink();
        gvFreeDevice(expectedCapture);
        gvFreeDevice(expectedPlayback);
        gvCleanup();
    }
    std::puts("PASS: movie open/pause/resume/close; Voice2 raw/Speex capture/playback, quality changes and cleanup");
}
