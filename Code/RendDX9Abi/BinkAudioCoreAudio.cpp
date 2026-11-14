// [CHOSEN, not recovered] macOS sound output of the Bink stand-in (BinkFFmpeg.cpp, BinkPort.h):
// one AudioQueue per playing movie, pulling the decoded samples. It stands in for the DirectSound
// mixer binkw32 opens after BinkSetSoundSystem(BinkOpenDirectSound); AudioQueue is the plain
// system output that needs no device selection and runs its callback on its own thread. Port
// code, not a recovery (Code/BF2/External/ffmpeg-bink/README.md).

#include "BinkPort.h"

#if BF2_BINK_PORT && defined(__APPLE__)

#include <AudioToolbox/AudioToolbox.h>

#include <atomic>
#include <cstring>

namespace renddx9abi {

namespace {

class CoreAudioOutput : public BinkPortAudioOutput
{
public:
    ~CoreAudioOutput() override { close(); }

    bool open(int sampleRate, int channels, BinkPortAudioSource* source) override
    {
        m_source = source;
        m_channels = channels;
        AudioStreamBasicDescription f;
        std::memset(&f, 0, sizeof(f));
        f.mSampleRate       = sampleRate;
        f.mFormatID         = kAudioFormatLinearPCM;
        f.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        f.mChannelsPerFrame = UInt32(channels);
        f.mBitsPerChannel   = 32;
        f.mBytesPerFrame    = 4 * UInt32(channels);
        f.mFramesPerPacket  = 1;
        f.mBytesPerPacket   = f.mBytesPerFrame;
        if (AudioQueueNewOutput(&f, &CoreAudioOutput::callback, this, 0, 0, 0, &m_queue) != noErr)
        {
            m_queue = 0;
            return false;
        }
        if (AudioQueueCreateTimeline(m_queue, &m_timeline) != noErr)
            m_timeline = 0;
        // 3 x 1024 frames (about 70 ms at 44.1 kHz) bounds the output latency, close to the
        // write-ahead binkw32 keeps in its DirectSound buffer.
        const UInt32 bytes = kFramesPerBuffer * f.mBytesPerFrame;
        for (int i = 0; i < kBuffers; ++i)
        {
            if (AudioQueueAllocateBuffer(m_queue, bytes, &m_buffers[i]) != noErr)
            {
                close();
                return false;
            }
            fill(m_buffers[i]);
            AudioQueueEnqueueBuffer(m_queue, m_buffers[i], 0, 0);
        }
        return true;
    }

    void close() override
    {
        if (!m_queue)
            return;
        AudioQueueStop(m_queue, true);
        if (m_timeline)
            AudioQueueDisposeTimeline(m_queue, m_timeline);
        AudioQueueDispose(m_queue, true);
        m_queue = 0;
        m_timeline = 0;
    }

    void setRunning(bool run) override
    {
        if (!m_queue || run == m_running)
            return;
        m_running = run;
        if (run)
            AudioQueueStart(m_queue, 0);
        else
        {
            m_pausedAt = queuePosition();
            AudioQueuePause(m_queue);
        }
    }

    // The queue's sample time; it holds while paused. Before the first start, and if the timeline
    // is unavailable, the frames handed to the queue stand in (less the two queued buffers).
    unsigned long long playedFrames() override
    {
        if (!m_running)
            return m_pausedAt;
        return queuePosition();
    }

private:
    static const int    kBuffers = 3;
    static const UInt32 kFramesPerBuffer = 1024;

    unsigned long long queuePosition()
    {
        if (m_queue && m_timeline)
        {
            AudioTimeStamp ts;
            std::memset(&ts, 0, sizeof(ts));
            Boolean discontinuity = false;
            if (AudioQueueGetCurrentTime(m_queue, m_timeline, &ts, &discontinuity) == noErr
                && (ts.mFlags & kAudioTimeStampSampleTimeValid) && ts.mSampleTime >= 0)
                return (unsigned long long)ts.mSampleTime;
        }
        const unsigned long long fed = m_fed.load();
        const unsigned long long queued = (kBuffers - 1) * kFramesPerBuffer;
        return fed > queued ? fed - queued : 0;
    }

    static void callback(void* user, AudioQueueRef queue, AudioQueueBufferRef buffer)
    {
        CoreAudioOutput* self = static_cast<CoreAudioOutput*>(user);
        self->fill(buffer);
        AudioQueueEnqueueBuffer(queue, buffer, 0, 0);
    }

    void fill(AudioQueueBufferRef buffer)
    {
        const size_t frames = buffer->mAudioDataBytesCapacity / (sizeof(float) * size_t(m_channels));
        m_source->pull(static_cast<float*>(buffer->mAudioData), frames);
        buffer->mAudioDataByteSize = UInt32(frames * sizeof(float) * size_t(m_channels));
        m_fed += frames;
    }

    AudioQueueRef                   m_queue = 0;
    AudioQueueTimelineRef           m_timeline = 0;
    AudioQueueBufferRef             m_buffers[kBuffers] = {};
    BinkPortAudioSource*            m_source = 0;
    int                             m_channels = 2;
    bool                            m_running = false;
    unsigned long long              m_pausedAt = 0;
    std::atomic<unsigned long long> m_fed{0};
};

} // namespace

BinkPortAudioOutput* binkPortCreateSystemAudioOutput()
{
    return new CoreAudioOutput();
}

} // namespace renddx9abi

#endif // BF2_BINK_PORT && __APPLE__
