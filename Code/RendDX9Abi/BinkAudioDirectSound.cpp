// [CHOSEN, not recovered] Windows x64 sound output of the Bink stand-in (BinkFFmpeg.cpp,
// BinkPort.h). Retail binkw32 plays movie sound through DirectSound once Video::initSound has
// called BinkSetSoundSystem(BinkOpenDirectSound, 0); the port uses DirectSound too, so x64 sound
// goes through the same system API as the x86 image. The buffer copies what the black-box
// harness saw binkw32 create (Code/BF2/External/ffmpeg-bink/README.md): one looping secondary buffer per
// movie, 16-bit PCM at the file's rate and channel count, flags GETCURRENTPOSITION2 | GLOBALFOCUS
// | CTRLVOLUME | CTRLPAN | CTRLFREQUENCY (0x180E0), 0.6 s rounded up to 1 KiB. Port code, not a
// recovery: the feeding thread is chosen. It keeps only about 80 ms written ahead of the play
// cursor, so a sound restart (BinkWait) is heard after the same gap as on retail instead of
// after silence already queued.

#include "BinkPort.h"

#if BF2_BINK_PORT && defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "user32.lib")
#endif

namespace renddx9abi {

namespace {

class DirectSoundOutput : public BinkPortAudioOutput
{
public:
    ~DirectSoundOutput() override { close(); }

    bool open(int sampleRate, int channels, BinkPortAudioSource* source) override
    {
        m_source = source;
        m_channels = channels;
        m_blockAlign = 2u * unsigned(channels);
        if (FAILED(DirectSoundCreate8(0, &m_ds, 0)))
        {
            m_ds = 0;
            return false;
        }
        // The client window is not known here; the desktop window satisfies the API and
        // GLOBALFOCUS keeps the movie audible whichever window has the focus.
        m_ds->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
        WAVEFORMATEX f;
        std::memset(&f, 0, sizeof(f));
        f.wFormatTag      = WAVE_FORMAT_PCM;
        f.nChannels       = WORD(channels);
        f.nSamplesPerSec  = DWORD(sampleRate);
        f.wBitsPerSample  = 16;
        f.nBlockAlign     = WORD(m_blockAlign);
        f.nAvgBytesPerSec = f.nSamplesPerSec * f.nBlockAlign;
        DSBUFFERDESC d;
        std::memset(&d, 0, sizeof(d));
        d.dwSize        = sizeof(d);
        d.dwFlags       = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME
                          | DSBCAPS_CTRLPAN | DSBCAPS_CTRLFREQUENCY;
        d.dwBufferBytes = (f.nAvgBytesPerSec * 6 / 10 + 1023) & ~1023u;
        d.lpwfxFormat   = &f;
        if (FAILED(m_ds->CreateSoundBuffer(&d, &m_buffer, 0)))
        {
            close();
            return false;
        }
        m_size = d.dwBufferBytes;
        m_ahead = (f.nAvgBytesPerSec * 8 / 100) / m_blockAlign * m_blockAlign;   // 80 ms
        // Silence everywhere, then the first 80 ms of the movie at the start.
        void* p1; DWORD b1; void* p2; DWORD b2;
        if (SUCCEEDED(m_buffer->Lock(0, m_size, &p1, &b1, &p2, &b2, 0)))
        {
            std::memset(p1, 0, b1);
            if (p2) std::memset(p2, 0, b2);
            m_buffer->Unlock(p1, b1, p2, b2);
        }
        m_writePos = 0;
        write(m_ahead);
        m_quit = false;
        m_thread = std::thread([this] { run(); });
        return true;
    }

    void close() override
    {
        m_quit = true;
        if (m_thread.joinable())
            m_thread.join();
        if (m_buffer) { m_buffer->Stop(); m_buffer->Release(); m_buffer = 0; }
        if (m_ds) { m_ds->Release(); m_ds = 0; }
    }

    void setRunning(bool run) override
    {
        if (!m_buffer || run == m_running)
            return;
        m_running = run;
        if (run)
            m_buffer->Play(0, 0, DSBPLAY_LOOPING);
        else
            m_buffer->Stop();
    }

    unsigned long long playedFrames() override { return m_played.load() / m_blockAlign; }

private:
    // Fills `bytes` from the write position on with pulled movie audio (silence on underrun).
    void write(DWORD bytes)
    {
        if (!bytes)
            return;
        const size_t frames = bytes / m_blockAlign;
        m_float.resize(frames * size_t(m_channels));
        m_pcm.resize(m_float.size());
        m_source->pull(m_float.data(), frames);
        for (size_t i = 0; i < m_float.size(); ++i)
        {
            float v = m_float[i] * 32768.0f;
            v = v > 32767.0f ? 32767.0f : (v < -32768.0f ? -32768.0f : v);
            m_pcm[i] = short(v < 0.0f ? v - 0.5f : v + 0.5f);
        }
        void* p1; DWORD b1; void* p2; DWORD b2;
        if (FAILED(m_buffer->Lock(m_writePos, DWORD(frames * m_blockAlign), &p1, &b1, &p2, &b2, 0)))
            return;
        std::memcpy(p1, m_pcm.data(), b1);
        if (p2)
            std::memcpy(p2, reinterpret_cast<const char*>(m_pcm.data()) + b1, b2);
        m_buffer->Unlock(p1, b1, p2, b2);
        m_writePos = (m_writePos + DWORD(frames * m_blockAlign)) % m_size;
        m_written += frames * m_blockAlign;
    }

    void run()
    {
        DWORD lastPlay = 0;
        while (!m_quit)
        {
            DWORD play = 0, cursor = 0;
            if (SUCCEEDED(m_buffer->GetCurrentPosition(&play, &cursor)))
            {
                if (m_running)
                    m_played += (play + m_size - lastPlay) % m_size;
                lastPlay = play;
                const unsigned long long played = m_played.load();
                if (m_written < played)
                {
                    // The feeder fell behind the cursor (a long thread stall): write on from the
                    // cursor; the skipped stretch played whatever the buffer held.
                    m_writePos = play;
                    m_written = played;
                }
                // Bytes written ahead of the play cursor; top them up to m_ahead.
                const unsigned long long queued = m_written - played;
                if (queued < m_ahead)
                    write(DWORD(m_ahead - queued) / m_blockAlign * m_blockAlign);
            }
            Sleep(5);
        }
    }

    IDirectSound8*                  m_ds = 0;
    IDirectSoundBuffer*             m_buffer = 0;
    BinkPortAudioSource*            m_source = 0;
    int                             m_channels = 2;
    unsigned                        m_blockAlign = 4;
    DWORD                           m_size = 0, m_ahead = 0, m_writePos = 0;
    unsigned long long              m_written = 0;    // bytes written since open
    std::atomic<bool>               m_running{false};
    std::atomic<bool>               m_quit{false};
    std::atomic<unsigned long long> m_played{0};
    std::thread                     m_thread;
    std::vector<float>              m_float;
    std::vector<short>              m_pcm;
};

} // namespace

BinkPortAudioOutput* binkPortCreateSystemAudioOutput()
{
    return new DirectSoundOutput();
}

} // namespace renddx9abi

#endif // BF2_BINK_PORT && _WIN32
