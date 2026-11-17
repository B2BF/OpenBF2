// [CHOSEN, not recovered] Linux Bink audio output through SDL3 (issue #36).
// The decoder supplies interleaved float samples; SDL converts to the default device format.

#include "BinkPort.h"

#if BF2_BINK_PORT && defined(__linux__) && defined(BF2_LINUX_CLIENT)

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace renddx9abi {
namespace {

class SDLAudioOutput : public BinkPortAudioOutput
{
public:
    ~SDLAudioOutput() override { close(); }

    bool open(int rate, int channels, BinkPortAudioSource* source) override
    {
        close();
        if (rate <= 0 || channels <= 0 || channels > 8 || !source)
            return false;
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
            return fail("initialize audio");
        m_initialized = true;
        m_channels = channels;
        m_source = source;
        m_buffer.resize(kFrames * channels);
        const SDL_AudioSpec spec = { SDL_AUDIO_F32, channels, rate };
        m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, this);
        if (!m_stream)
        {
            fail("open output");
            close();
            return false;
        }
        SDL_AudioSpec deviceSpec;
        int deviceFrames = 0;
        if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(m_stream), &deviceSpec, &deviceFrames)
            || deviceSpec.freq <= 0 || deviceFrames <= 0)
        {
            fail("query output format");
            close();
            return false;
        }
        m_latency = (static_cast<unsigned long long>(deviceFrames) * rate + deviceSpec.freq - 1)
                    / deviceSpec.freq;
        return true;
    }

    void setRunning(bool run) override
    {
        if (!m_stream || run == m_running)
            return;
        if (run)
        {
            if (!SDL_ResumeAudioStreamDevice(m_stream))
            {
                fail("resume output");
                return;
            }
            m_running = true;
        }
        else
        {
            if (!SDL_PauseAudioStreamDevice(m_stream))
            {
                fail("pause output");
                return;
            }
            m_position = position();
            m_running = false;
        }
    }

    void close() override
    {
        // Destroy joins/quiesces the device callback before its source or scratch data die.
        if (m_stream)
            SDL_DestroyAudioStream(m_stream);
        m_stream = 0;
        m_source = 0;
        m_running = false;
        m_fed = m_position = m_latency = 0;
        if (m_initialized)
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_initialized = false;
    }

    unsigned long long playedFrames() override
    {
        if (m_running)
            m_position = position();
        return m_position;
    }

private:
    static const size_t kFrames = 1024;
    SDL_AudioStream* m_stream = 0;
    BinkPortAudioSource* m_source = 0;
    std::vector<float> m_buffer;
    int m_channels = 0;
    bool m_initialized = false;
    bool m_running = false;
    unsigned long long m_fed = 0; // Protected by the SDL stream lock.
    unsigned long long m_position = 0;
    unsigned long long m_latency = 0;

    static bool fail(const char* operation)
    {
        std::fprintf(stderr, "Bink SDL: could not %s: %s\n", operation, SDL_GetError());
        std::fflush(stderr);
        return false;
    }

    static void SDLCALL feed(void* context, SDL_AudioStream* stream, int additional, int)
    {
        SDLAudioOutput& self = *static_cast<SDLAudioOutput*>(context);
        const size_t frameBytes = sizeof(float) * self.m_channels;
        size_t remaining = additional > 0 ? (static_cast<size_t>(additional) + frameBytes - 1) / frameBytes : 0;
        while (remaining)
        {
            const size_t frames = std::min(remaining, kFrames);
            self.m_source->pull(self.m_buffer.data(), frames);
            if (!SDL_PutAudioStreamData(stream, self.m_buffer.data(), static_cast<int>(frames * frameBytes)))
            {
                fail("queue samples");
                return;
            }
            self.m_fed += frames;
            remaining -= frames;
        }
    }

    unsigned long long position()
    {
        // SDL exposes queue consumption, not the DAC cursor. Subtract one device buffer as a
        // bounded latency estimate; keep the movie clock monotonic and frozen across pauses.
        SDL_LockAudioStream(m_stream);
        const int queuedBytes = SDL_GetAudioStreamQueued(m_stream);
        const unsigned long long queued = queuedBytes < 0 ? m_fed
            : static_cast<unsigned long long>(queuedBytes) / (sizeof(float) * m_channels);
        const unsigned long long pending = queued + m_latency;
        const unsigned long long consumed = m_fed > pending ? m_fed - pending : 0;
        SDL_UnlockAudioStream(m_stream);
        return std::max(m_position, consumed);
    }
};

const size_t SDLAudioOutput::kFrames;
} // namespace

BinkPortAudioOutput* binkPortCreateSystemAudioOutput() { return new SDLAudioOutput; }
} // namespace renddx9abi
#endif
