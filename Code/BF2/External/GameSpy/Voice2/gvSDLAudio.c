/* [CHOSEN, not recovered] Native Linux Voice2 audio, issue #36.
 * SDL owns device conversion/threads. The game thread pumps PCM through the SDK's existing
 * custom device, keeping its codec, source mixing, thresholds and packet clocks unchanged.
 * No GameSpy codec or caller callback runs on an SDL audio thread.
 */
#include "gvSDLAudio.h"
#include "gvCustomDevice.h"
#include "gvCodec.h"
#include <SDL3/SDL.h>
#include <stdio.h>

typedef struct GVISDLData
{
    GVIDevice *device;
    GVIDevice *codec;
    struct GVISDLData *next;
    SDL_AudioStream *stream;
    GVSample *samples;
    GVByte *packet;
    int sampleRate;
    int samplesPerFrame;
    int frameBytes;
    int encodedBytes;
    int packetCapacity;
    GVBool failed;
} GVISDLData;

static GVISDLData *GVIDevices;
static GVBool GVIInitialized;
enum { GVI_DEFAULT_PLAYBACK = -1, GVI_DEFAULT_CAPTURE = -2 };

static void gviSDLFailure(GVISDLData *data, const char *operation)
{
    if(!data || !data->failed)
    {
        fprintf(stderr, "Voice2 SDL: %s failed: %s\n", operation, SDL_GetError());
        fflush(stderr);
    }
    if(data)
        data->failed = GVTrue;
}

static GVBool gviSDLFormatValid(GVISDLData *data)
{
    if(data->sampleRate != GVISampleRate || data->samplesPerFrame != GVISamplesPerFrame
       || GVIEncodedFrameSize <= 0 || GVIEncodedFrameSize > data->packetCapacity)
    {
        SDL_SetError("close voice devices before changing PCM frame size or sample rate");
        gviSDLFailure(data, "codec format check");
    }
    else
        data->encodedBytes = GVIEncodedFrameSize;
    return !data->failed;
}

static void gviSDLFilter(GVDevice codec, GVSample *samples, GVFrameStamp stamp, GVDeviceType type)
{
    GVISDLData *data;
    for(data = GVIDevices; data; data = data->next)
        if(data->codec == codec)
        {
            gvFilterCallback filter = type == GV_CAPTURE ? data->device->m_captureFilterCallback
                                                         : data->device->m_playbackFilterCallback;
            if(filter)
                filter(data->device, samples, stamp);
            return;
        }
}
static void gviSDLCaptureFilter(GVDevice codec, GVSample *samples, GVFrameStamp stamp)
{ gviSDLFilter(codec, samples, stamp, GV_CAPTURE); }
static void gviSDLPlaybackFilter(GVDevice codec, GVSample *samples, GVFrameStamp stamp)
{ gviSDLFilter(codec, samples, stamp, GV_PLAYBACK); }

static void gviSDLFree(GVDevice device)
{
    GVISDLData *data = (GVISDLData *)device->m_data;
    GVISDLData **link = &GVIDevices;
    while(*link && *link != data)
        link = &(*link)->next;
    if(*link)
        *link = data->next;
    if(data->stream)
        SDL_DestroyAudioStream(data->stream);
    if(data->codec)
        gvFreeDevice(data->codec);
    gsifree(data->samples);
    gsifree(data->packet);
    gviFreeDevice(device);
}

static GVBool gviSDLStart(GVDevice device, GVDeviceType type)
{
    GVISDLData *data = (GVISDLData *)device->m_data;
    if(!type || (type & ~device->m_types) || !gviSDLFormatValid(data))
        return GVFalse;
    if(gvIsDeviceStarted(data->codec, type))
        return GVTrue;
    if(!SDL_ResumeAudioStreamDevice(data->stream))
    {
        gviSDLFailure(data, "start device");
        return GVFalse;
    }
    return gvStartDevice(data->codec, type);
}

static void gviSDLStop(GVDevice device, GVDeviceType type)
{
    GVISDLData *data = (GVISDLData *)device->m_data;
    if(!(type & device->m_types))
        return;
    if(!SDL_PauseAudioStreamDevice(data->stream) || !SDL_ClearAudioStream(data->stream))
        gviSDLFailure(data, "stop device");
    gvStopDevice(data->codec, type & device->m_types);
}

static GVBool gviSDLStarted(GVDevice device, GVDeviceType type)
{
    GVISDLData *data = (GVISDLData *)device->m_data;
    return !data->failed && type && !(type & ~device->m_types)
           && gvIsDeviceStarted(data->codec, type);
}

static void gviSDLSetVolume(GVDevice device, GVDeviceType type, GVScalar volume)
{ gvSetDeviceVolume(((GVISDLData *)device->m_data)->codec, type, volume); }
static GVScalar gviSDLGetVolume(GVDevice device, GVDeviceType type)
{ return gvGetDeviceVolume(((GVISDLData *)device->m_data)->codec, type); }
static void gviSDLSetThreshold(GVDevice device, GVScalar threshold)
{ gvSetCaptureThreshold(((GVISDLData *)device->m_data)->codec, threshold); }
static GVScalar gviSDLGetThreshold(GVDevice device)
{ return gvGetCaptureThreshold(((GVISDLData *)device->m_data)->codec); }

static int gviSDLAvailable(GVDevice device)
{
    GVISDLData *data = (GVISDLData *)device->m_data;
    int available;
    if(!gviSDLStarted(device, GV_CAPTURE) || !gviSDLFormatValid(data))
        return 0;
    available = SDL_GetAudioStreamAvailable(data->stream);
    if(available < 0)
    {
        gviSDLFailure(data, "query capture");
        return 0;
    }
    /* Bound stale capture to one second, advancing the codec clock for discarded frames. */
    while(available > data->sampleRate * (int)sizeof(GVSample))
    {
        GVFrameStamp stamp;
        int length = 0;
        if(SDL_GetAudioStreamData(data->stream, data->samples, data->frameBytes) != data->frameBytes)
        {
            gviSDLFailure(data, "drain capture");
            return 0;
        }
        gviSetCustomCaptureAudio(data->codec, NULL, data->samplesPerFrame, NULL, &length, &stamp, NULL);
        available -= data->frameBytes;
    }
    return (available / data->frameBytes) * data->encodedBytes;
}

static GVBool gviSDLCapture(GVDevice device, GVByte *packet, int *length, GVFrameStamp *stamp, GVScalar *volume)
{
    GVISDLData *data = (GVISDLData *)device->m_data;
    const int capacity = *length;
    int encoded;
    GVBool result;
    *length = 0;
    if(volume)
        *volume = 0;
    if(!gviSDLAvailable(device) || capacity < data->encodedBytes)
        return GVFalse;
    if(SDL_GetAudioStreamData(data->stream, data->samples, data->frameBytes) != data->frameBytes)
    {
        gviSDLFailure(data, "read capture");
        return GVFalse;
    }
    /* The custom-device SDK requires raw-frame capacity even for compressed codecs. */
    encoded = max(data->frameBytes, data->encodedBytes);
    result = gviSetCustomCaptureAudio(data->codec, data->samples, data->samplesPerFrame,
                                     data->packet, &encoded, stamp, volume);
    if(result)
    {
        memcpy(packet, data->packet, (size_t)encoded);
        *length = encoded;
    }
    return result;
}

static void gviSDLPlay(GVDevice device, const GVByte *packet, int length, GVSource source, GVFrameStamp stamp, GVBool mute)
{
    GVISDLData *data = (GVISDLData *)device->m_data;
    if(gviSDLStarted(device, GV_PLAYBACK) && gviSDLFormatValid(data))
        gvPlayPacket(data->codec, packet, length, source, stamp, mute);
}
static GVBool gviSDLTalking(GVDevice device, GVSource source)
{ return gvIsSourceTalking(((GVISDLData *)device->m_data)->codec, source); }
static int gviSDLListTalking(GVDevice device, GVSource sources[], int maximum)
{ return gvListTalkingSources(((GVISDLData *)device->m_data)->codec, sources, maximum); }

GVBool gviHardwareStartup(void)
{
    if(GVIInitialized)
        return GVTrue;
    if(!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        gviSDLFailure(NULL, "initialize audio");
        return GVFalse;
    }
    GVIInitialized = GVTrue;
    return GVTrue;
}

void gviHardwareCleanup(void)
{
    while(GVIDevices)
        gviSDLFree(GVIDevices->device);
    if(GVIInitialized)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    GVIInitialized = GVFalse;
}

void gviHardwarePrepareCodecChange(void)
{
    GVISDLData *data;
    for(data = GVIDevices; data; data = data->next)
    {
        /* The SDK frees source decoder data through the current codec's callbacks. Clear
         * sources before changing those callbacks, and before its pending-frame pool resets.
         * Same-rate voice-quality switches can then reuse the existing SDL PCM stream. */
        if(data->device->m_types & GV_PLAYBACK)
        {
            const GVBool playing = gvIsDeviceStarted(data->codec, GV_PLAYBACK);
            gvStopDevice(data->codec, GV_PLAYBACK);
            if(playing)
                gvStartDevice(data->codec, GV_PLAYBACK);
        }
        if(!SDL_ClearAudioStream(data->stream))
            gviSDLFailure(data, "clear audio for codec change");
    }
}

void gviHardwareThink(void)
{
    GVISDLData *data = GVIDevices;
    while(data)
    {
        GVISDLData *next = data->next;
        if(gviSDLFormatValid(data) && gviSDLStarted(data->device, GV_PLAYBACK))
        {
            /* Keep 100 ms queued, below the SDK's 200 ms playback buffer. */
            int queued = SDL_GetAudioStreamQueued(data->stream);
            const int target = data->sampleRate * (int)sizeof(GVSample) / 10;
            if(queued < 0)
                gviSDLFailure(data, "query playback");
            while(!data->failed && queued < target)
            {
                gviGetCustomPlaybackAudio(data->codec, data->samples, data->samplesPerFrame);
                if(!SDL_PutAudioStreamData(data->stream, data->samples, data->frameBytes))
                    gviSDLFailure(data, "queue playback");
                queued += data->frameBytes;
            }
        }
        else if(gviSDLStarted(data->device, GV_CAPTURE))
            gviSDLAvailable(data->device);
        if(data->failed)
            gviDeviceUnplugged(data->device);
        data = next;
    }
}

int gviHardwareListDevices(GVDeviceInfo devices[], int maximum, GVDeviceType types)
{
    int result = 0;
    int direction;
    if(!GVIInitialized || !devices || maximum <= 0)
        return 0;
    for(direction = 0; direction < 2 && result < maximum; ++direction)
    {
        int count = 0;
        const GVDeviceType type = direction ? GV_CAPTURE : GV_PLAYBACK;
        SDL_AudioDeviceID *ids;
        if(!(types & type))
            continue;
        ids = direction ? SDL_GetAudioRecordingDevices(&count) : SDL_GetAudioPlaybackDevices(&count);
        if(!ids)
            gviSDLFailure(NULL, "enumerate devices");
        else if(count > 0)
        {
            GVDeviceInfo *info = &devices[result++];
            memset(info, 0, sizeof(*info));
            info->m_id = direction ? GVI_DEFAULT_CAPTURE : GVI_DEFAULT_PLAYBACK;
            _tcsncpy(info->m_name, direction ? _T("Default system microphone") : _T("Default system playback"), GV_DEVICE_NAME_LEN - 1);
            info->m_deviceType = info->m_defaultDevice = type;
            info->m_hardwareType = GVHardwareSDL;
        }
        SDL_free(ids);
    }
    return result;
}

GVDevice gviHardwareNewDevice(GVDeviceID id, GVDeviceType type)
{
    GVIDevice *device;
    GVISDLData *data;
    SDL_AudioSpec spec;
    if(!GVIInitialized || GVISamplesPerFrame <= 0 || GVIEncodedFrameSize <= 0 || GVISampleRate <= 0
       || !((id == GVI_DEFAULT_PLAYBACK && type == GV_PLAYBACK)
            || (id == GVI_DEFAULT_CAPTURE && type == GV_CAPTURE)))
        return NULL;
    device = gviNewDevice(id, GVHardwareSDL, type, sizeof(GVISDLData));
    if(!device)
        return NULL;
    data = (GVISDLData *)device->m_data;
    data->device = device;
    data->sampleRate = GVISampleRate;
    data->samplesPerFrame = GVISamplesPerFrame;
    data->frameBytes = GVIBytesPerFrame;
    data->encodedBytes = GVIEncodedFrameSize;
    data->packetCapacity = max(data->frameBytes, data->encodedBytes);
    data->codec = gviCustomNewDevice(type);
    data->samples = (GVSample *)gsimalloc((unsigned int)data->frameBytes);
    data->packet = (GVByte *)gsimalloc((unsigned int)data->packetCapacity);
    spec.format = SDL_AUDIO_S16;
    spec.channels = 1;
    spec.freq = data->sampleRate;
    data->stream = SDL_OpenAudioDeviceStream(type == GV_CAPTURE ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                                              : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                            &spec, NULL, NULL);
    if(!data->codec || !data->samples || !data->packet || !data->stream)
    {
        gviSDLFailure(NULL, "create device");
        gviSDLFree(device);
        return NULL;
    }
    data->codec->m_captureFilterCallback = gviSDLCaptureFilter;
    data->codec->m_playbackFilterCallback = gviSDLPlaybackFilter;
    device->m_methods.m_freeDevice = gviSDLFree;
    device->m_methods.m_startDevice = gviSDLStart;
    device->m_methods.m_stopDevice = gviSDLStop;
    device->m_methods.m_isDeviceStarted = gviSDLStarted;
    device->m_methods.m_setDeviceVolume = gviSDLSetVolume;
    device->m_methods.m_getDeviceVolume = gviSDLGetVolume;
    device->m_methods.m_setCaptureThreshold = gviSDLSetThreshold;
    device->m_methods.m_getCaptureThreshold = gviSDLGetThreshold;
    device->m_methods.m_capturePacket = gviSDLCapture;
    device->m_methods.m_getAvailableCaptureBytes = gviSDLAvailable;
    device->m_methods.m_playPacket = gviSDLPlay;
    device->m_methods.m_isSourceTalking = gviSDLTalking;
    device->m_methods.m_listTalkingSources = gviSDLListTalking;
    data->next = GVIDevices;
    GVIDevices = data;
    return device;
}
