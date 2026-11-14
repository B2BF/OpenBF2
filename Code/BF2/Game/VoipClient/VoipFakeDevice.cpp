// [CHOSEN, not recovered] See VoipFakeDevice.h.
#include "../../External/GameSpy/Voice2/gv.h"
extern "C" {
#include "../../External/GameSpy/Voice2/gvDevice.h"
#include "../../External/GameSpy/Voice2/gvCodec.h"
}
#include "VoipFakeDevice.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace dice {
namespace hfe {
namespace VoipFakeDevice {

namespace {

std::uint32_t defaultClock() { return VoipClient::tickCount(); }

Clock         s_clock = &defaultClock;
int           s_amplitude = 12000;
std::uint32_t s_burstMs = 1500;
std::uint32_t s_gapMs = 1500;
int           s_capturedFrames = 0;
int           s_playedFrames = 0;

// Per device: the clock origin and how many capture/playback frames have been served.
struct Pacing
{
    GVIDevice*    device;
    std::uint32_t start;
    long long     capture;
    long long     playback;
};
std::vector<Pacing> s_devices;

Pacing* pacingFor(GVIDevice* device)
{
    for (std::size_t i = 0; i < s_devices.size(); ++i)
        if (s_devices[i].device == device)
            return &s_devices[i];
    return 0;
}

long long framesDue(const Pacing& pacing)
{
    const std::uint32_t elapsed = s_clock() - pacing.start;
    const long long samples = static_cast<long long>(elapsed) * gvGetSampleRate() / 1000;
    return GVISamplesPerFrame > 0 ? samples / GVISamplesPerFrame : 0;
}

void fillFrame(GVSample* frame, long long frameIndex)
{
    const double rate = static_cast<double>(gvGetSampleRate());
    const std::uint32_t period = s_burstMs + s_gapMs;
    for (int i = 0; i < GVISamplesPerFrame; ++i)
    {
        const long long sample = frameIndex * GVISamplesPerFrame + i;
        const std::uint32_t ms = static_cast<std::uint32_t>(sample * 1000 / static_cast<long long>(rate));
        const bool on = period == 0 || (ms % period) < s_burstMs;
        frame[i] = on ? static_cast<GVSample>(s_amplitude * std::sin(6.283185307179586 * 300.0 * sample / rate))
                      : static_cast<GVSample>(0);
    }
}

// Drains the playback frames that are due, so sources start and stop talking on time.
void drainPlayback(GVIDevice* device, Pacing& pacing)
{
    if (!(device->m_types & GV_PLAYBACK))
        return;
    const long long due = framesDue(pacing);
    std::vector<GVSample> frame(static_cast<std::size_t>(GVISamplesPerFrame));
    while (pacing.playback < due)
    {
        gvGetCustomPlaybackAudio(device, frame.data(), GVISamplesPerFrame);
        ++pacing.playback;
        ++s_playedFrames;
    }
}

int availableCaptureBytes(GVDevice device)
{
    Pacing* pacing = pacingFor(device);
    if (!pacing)
        return 0;
    const long long pending = framesDue(*pacing) - pacing->capture;
    return pending > 0 ? static_cast<int>(pending * GVIBytesPerFrame) : 0;
}

// One frame per call while frames are due; silent frames below the threshold are consumed and end
// the caller's loop, exactly as a hardware device reports "no packet".
GVBool capturePacket(GVDevice device, GVByte* packet, int* len, GVFrameStamp* frameStamp, GVScalar* volume)
{
    Pacing* pacing = pacingFor(device);
    const int room = *len;
    *len = 0;
    if (volume)
        *volume = 0;
    if (!pacing)
        return GVFalse;
    drainPlayback(device, *pacing);
    if (pacing->capture >= framesDue(*pacing) || room < GVIEncodedFrameSize)
        return GVFalse;
    std::vector<GVSample> frame(static_cast<std::size_t>(GVISamplesPerFrame));
    fillFrame(frame.data(), pacing->capture);
    ++pacing->capture;
    ++s_capturedFrames;
    int packetLen = room;
    const GVBool over = gvSetCustomCaptureAudio(device, frame.data(), GVISamplesPerFrame, packet,
                                                &packetLen, frameStamp, volume);
    *len = over ? packetLen : 0;
    return over;
}

void (*s_customFree)(GVIDevice*) = 0;

void freeDevice(GVIDevice* device)
{
    for (std::size_t i = 0; i < s_devices.size(); ++i)
    {
        if (s_devices[i].device == device)
        {
            s_devices.erase(s_devices.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    if (s_customFree)
        s_customFree(device);
}

} // namespace

void setClock(Clock clock) { s_clock = clock ? clock : &defaultClock; }

void setSignal(int amplitude, std::uint32_t burstMs, std::uint32_t gapMs)
{
    s_amplitude = amplitude;
    s_burstMs = burstMs;
    s_gapMs = gapMs;
}

bool requested()
{
    const char* value = std::getenv("BF2_VOIP_CAPTURE");
    return value && std::strcmp(value, "fake") == 0;
}

VoipDevice create()
{
    GVDevice device = gvNewCustomDevice(GV_CAPTURE_AND_PLAYBACK);
    if (!device)
        return 0;
    s_customFree = device->m_methods.m_freeDevice;
    device->m_methods.m_freeDevice = &freeDevice;
    device->m_methods.m_capturePacket = &capturePacket;
    device->m_methods.m_getAvailableCaptureBytes = &availableCaptureBytes;
    Pacing pacing = { device, s_clock(), 0, 0 };
    s_devices.push_back(pacing);
    return device;
}

int capturedFrames() { return s_capturedFrames; }
int playedFrames()   { return s_playedFrames; }

} // namespace VoipFakeDevice
} // namespace hfe
} // namespace dice
