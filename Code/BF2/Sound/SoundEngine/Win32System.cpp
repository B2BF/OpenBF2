// Audio DLL platform services and process entry point.

// Audio DLL process entry point.


// BF2Audio.dll only. Without BF2_IMAGE_BF2AUDIO this file compiles to nothing: the recursive
// sweeps that build BF2.exe and bf2_w32ded.exe (CMake's bf2_client_objects glob, build.cmd's
// all-TU census) must not carry the DLL's engine: its DllMain, its two exports and its own null
// Sound/SoundTemplate pair would collide with the host's, which loads this image at run time.
// Same convention as
// Code/BF2/RendDX9/BF2Render.cpp's BF2_IMAGE_RENDDX9 guard. Build with Code\Build\bf2audio.cmd.
#if defined(BF2_IMAGE_BF2AUDIO)
#include "Win32System.h"
#include "SoundTrace.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>     // waveformatex, which dsound.h needs and WIN32_LEAN_AND_MEAN leaves out
#include <dsound.h>

namespace dice {
namespace hfe {
namespace win32 {

void* g_dllInstance = 0;        // ds:10048E68

void* loadLibrary(const char* fileName)              { return (void*) ::LoadLibraryA(fileName); }
void  freeLibrary(void* module)                       { if (module) ::FreeLibrary((HMODULE) module); }
void* getProcAddress(void* module, const char* name) { return module ? (void*) ::GetProcAddress((HMODULE) module, name) : 0; }

void  outputDebugString(const char* text)             { ::OutputDebugStringA(text); }

void  sleep(unsigned int milliseconds)                { ::Sleep(milliseconds); }
long  exchange(volatile long* target, long value)     { return ::InterlockedExchange(target, value); }

namespace {

DWORD WINAPI threadStart(LPVOID parameter)
{
    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    Thread* t = (Thread*) parameter;
    t->entry(t);
    return 0;
}

} // namespace

// The handle CreateThread returns is stored after the object is fully written,
// but the thread may already be running `entry` by then - the entry must not touch `handle`.
Thread* createThread(void (__cdecl* entry)(Thread*), void* argument)
{
    Thread* t = new Thread;
    t->entry = entry;
    t->argument = argument;
    t->stopRequested = 0;
    t->handle = (void*) ::CreateThread(0, 0, threadStart, t, 0, &t->id);
    return t;
}

// Raise the stop flag, wait forever for the thread, close, delete.
void joinAndDeleteThread(Thread* t)
{
    if (t)
    {
        ::InterlockedIncrement(&t->stopRequested);
        ::WaitForSingleObject((HANDLE) t->handle, INFINITE);
        ::CloseHandle((HANDLE) t->handle);
        delete t;
    }
}

bool  threadShouldStop(const Thread* t) { return t->stopRequested != 0; }
void* threadArgument(const Thread* t)   { return t->argument; }

namespace {

// Ignores the primary-device entry with null GUID; logs and marks named devices.
BOOL CALLBACK enumCallback(LPGUID guid, LPCSTR description, LPCSTR module, LPVOID context)
{
    if (!guid)
        return TRUE;
    if (!module)      module = "unknown";
    if (!description) description = "unknown";
    g_soundTrace->info(sndFormat("Found sound device %s (%s)", description, module),
                       "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\Win32System.cpp", 141);
    *(unsigned int*) context |= 1u;
    return FALSE;   // false stops the enumeration after the first named device. As shipped.
}

} // namespace

bool hasSoundDevice()
{
    unsigned int context = 0;
    const HRESULT hr = ::DirectSoundEnumerateA(enumCallback, &context);
    if (hr < 0)
    {
        g_soundTrace->warning(sndFormat("Enumerating sound devices failed (0x%08x). Sound will be disabled.", hr),
                              "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\Win32System.cpp", 152);
    }
    return context != 0;
}

} // namespace win32
} // namespace hfe
}
// Records the DLL instance.
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void) reason; (void) reserved;
    dice::hfe::win32::g_dllInstance = (void*) instance;
    return TRUE;
}

#else // !_WIN32

// [CHOSEN, not recovered] Native POSIX arm of the image's platform services (macOS port, package
// S2.5; docs/macos-port.md, "Audio (S2.5)"). The image is linked statically into the client, so
// there is no DllMain and g_dllInstance stays null (it has no reader). Each wrapper keeps the
// contract its Win32 body documents in Win32System.h.

#include <dlfcn.h>
#include <pthread.h>
#include <strings.h>
#include <time.h>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#if defined(__linux__) && defined(BF2_LINUX_CLIENT)
#include "BfOpenAL.h"
#endif
#if defined(__APPLE__)
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace dice {
namespace hfe {
namespace win32 {

void* g_dllInstance = 0;

namespace {

// The two OpenAL DLL names the provider loads (the system OpenAL32.dll and BF2's own
// BF2OpenAL.dll). macOS has one OpenAL, the system framework, and it opens the default output
// for every device name the provider rows pass, so both names load it.
const char* mapLibraryName(const char* fileName)
{
#if defined(__APPLE__)
    if (fileName && (::strcasecmp(fileName, "OpenAL32.dll") == 0 || ::strcasecmp(fileName, "BF2OpenAL.dll") == 0))
        return "/System/Library/Frameworks/OpenAL.framework/OpenAL";
#elif defined(__linux__) && defined(BF2_LINUX_CLIENT)
    // [CHOSEN, not recovered] Native Linux OpenAL Soft, issue #36.
    if (fileName && (::strcasecmp(fileName, "OpenAL32.dll") == 0 || ::strcasecmp(fileName, "BF2OpenAL.dll") == 0))
        return "libopenal.so.1";
#endif
    return fileName;
}

} // namespace

void* loadLibrary(const char* fileName)              { return ::dlopen(mapLibraryName(fileName), RTLD_NOW | RTLD_LOCAL); }
void  freeLibrary(void* module)                       { if (module) ::dlclose(module); }
void* getProcAddress(void* module, const char* name) { return module ? ::dlsym(module, name) : 0; }

// OutputDebugStringA has no POSIX channel; the text goes to stderr unchanged, unbuffered.
void  outputDebugString(const char* text)             { std::fputs(text, stderr); }

void sleep(unsigned int milliseconds)
{
    timespec request;
    request.tv_sec  = (time_t) (milliseconds / 1000u);
    request.tv_nsec = (long) (milliseconds % 1000u) * 1000000L;
    while (::nanosleep(&request, &request) != 0 && errno == EINTR)
    {
    }
}

long exchange(std::atomic<long>* target, long value) { return target->exchange(value); }

namespace {

// THREAD_PRIORITY_TIME_CRITICAL's counterpart: the user-interactive QoS class.
void* threadStart(void* parameter)
{
#if defined(__APPLE__)
    ::pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    Thread* t = (Thread*) parameter;
    t->entry(t);
    return 0;
}

} // namespace

// As on Win32, the handle is stored after the thread may already be running `entry`.
Thread* createThread(void (__cdecl* entry)(Thread*), void* argument)
{
    Thread* t = new Thread;
    t->entry = entry;
    t->argument = argument;
    t->stopRequested = 0;
    t->handle = 0;
    t->id = 0;
    pthread_t thread;
    if (::pthread_create(&thread, 0, threadStart, t) == 0)
    {
        t->handle = (void*) thread;
#if defined(__APPLE__)
        std::uint64_t id = 0;
        ::pthread_threadid_np(thread, &id);
        t->id = (unsigned long) id;
#endif
    }
    return t;
}

void joinAndDeleteThread(Thread* t)
{
    if (t)
    {
        t->stopRequested.fetch_add(1);
        if (t->handle)
            ::pthread_join((pthread_t) t->handle, 0);
        delete t;
    }
}

bool  threadShouldStop(const Thread* t) { return t->stopRequested.load() != 0; }
void* threadArgument(const Thread* t)   { return t->argument; }

// DirectSoundEnumerate's counterpart: the default output device. Named devices are logged as
// the Win32 enumeration logs them; a failed query logs the same warning and returns false.
bool hasSoundDevice()
{
#if defined(__APPLE__)
    AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultOutputDevice,
                                           kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain };
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    const OSStatus status = ::AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, 0, &size, &device);
    if (status != noErr)
    {
        g_soundTrace->warning(sndFormat("Enumerating sound devices failed (0x%08x). Sound will be disabled.", (unsigned int) status),
                              "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\Win32System.cpp", 152);
        return false;
    }
    if (device == kAudioObjectUnknown)
        return false;
    char description[256] = "unknown";
    CFStringRef name = 0;
    AudioObjectPropertyAddress nameAddress = { kAudioObjectPropertyName,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain };
    size = sizeof(name);
    if (::AudioObjectGetPropertyData(device, &nameAddress, 0, 0, &size, &name) == noErr && name)
    {
        if (!::CFStringGetCString(name, description, sizeof(description), kCFStringEncodingUTF8))
            std::snprintf(description, sizeof(description), "%s", "unknown");
        ::CFRelease(name);
    }
    g_soundTrace->info(sndFormat("Found sound device %s (%s)", description, "CoreAudio"),
                       "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\Win32System.cpp", 141);
    return true;
#elif defined(__linux__) && defined(BF2_LINUX_CLIENT)
    // [CHOSEN, not recovered] Probe the real output rather than infer device availability
    // from the presence of a library. This does not retain a device or change provider state.
    void* module = loadLibrary("OpenAL32.dll");
    typedef ALCdevice* (*OpenDevice)(const ALchar*);
    typedef ALboolean (*CloseDevice)(ALCdevice*);
    OpenDevice open = reinterpret_cast<OpenDevice>(getProcAddress(module, "alcOpenDevice"));
    CloseDevice close = reinterpret_cast<CloseDevice>(getProcAddress(module, "alcCloseDevice"));
    ALCdevice* device = open && close ? open(0) : 0;
    const bool found = device != 0;
    if (device)
        close(device);
    freeLibrary(module);
    if (found)
        g_soundTrace->info(sndFormat("Found sound device default (OpenAL Soft)"),
                           "Sound/SoundEngine/Win32System.cpp", 0);
    else
        g_soundTrace->warning(sndFormat("OpenAL default sound device could not be opened. Sound will be disabled."),
                              "Sound/SoundEngine/Win32System.cpp", 0);
    return found;
#else
    // Other POSIX ports have no device query yet; OpenAL's own device open decides.
    return true;
#endif
}

} // namespace win32
} // namespace hfe
} // namespace dice

#endif // _WIN32

#endif // BF2_IMAGE_BF2AUDIO
