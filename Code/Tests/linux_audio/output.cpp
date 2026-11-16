// [TEMP] [CHOSEN, not recovered] Manual playback-only connection probe, issue #36.
// Queues silence to the real default output. No recording device is opened.
#include <SDL3/SDL.h>
#include <cstdio>

int main()
{
    if(!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        std::fprintf(stderr, "SDL audio initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    const SDL_AudioSpec format = { SDL_AUDIO_F32, 2, 48000 };
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, NULL, NULL);
    if(!stream)
    {
        std::fprintf(stderr, "SDL playback open failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return 1;
    }
    std::printf("SDL playback backend: %s\n", SDL_GetCurrentAudioDriver());
    const float silence[9600] = {};
    const bool started = SDL_PutAudioStreamData(stream, silence, sizeof(silence))
                         && SDL_ResumeAudioStreamDevice(stream);
    SDL_Delay(250);
    const int remaining = SDL_GetAudioStreamQueued(stream);
    std::printf("Silent playback: queued %zu bytes, remaining %d bytes\n", sizeof(silence), remaining);
    SDL_DestroyAudioStream(stream);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return started && remaining == 0 ? 0 : 1;
}
