// [TEMP] [CHOSEN, not recovered] Real OpenAL loader/device lifecycle with a test trace sink.
// CTest uses OpenAL Soft's null backend; this checks API integration, not audible output.
#include "../../BF2/Sound/SoundEngine/BfOpenAL.h"
#include "../../BF2/Sound/SoundEngine/Win32System.h"
#include "../../BF2/Sound/SoundEngine/SoundTrace.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dice { namespace hfe {
namespace {
class Trace : public ISoundTrace
{
public:
    void error(const char* text, const char*, int) override { std::fprintf(stderr, "%s\n", text); }
    void warning(const char* text, const char*, int) override { std::fprintf(stderr, "%s\n", text); }
    void info(const char* text, const char*, int) override { std::fprintf(stderr, "%s\n", text); }
    void assertFailed(const char* text, const char*, int) override { std::fprintf(stderr, "%s\n", text); std::abort(); }
    void debug(const char* text, const char*, int) override { std::fprintf(stderr, "%s\n", text); }
} trace;
}
ISoundTrace* g_soundTrace = &trace;
char* sndFormat(const char* format, ...)
{
    static char buffer[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}
}}

static void require(bool ok, const char* text)
{
    if(!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    using namespace dice::hfe;
    if(argc == 2 && std::strcmp(argv[1], "--unavailable") == 0)
    {
        require(!win32::hasSoundDevice(), "unavailable OpenAL device refused");
        std::puts("PASS: unavailable OpenAL output is reported");
        return 0;
    }
    require(win32::hasSoundDevice(), "OpenAL default output probe");
    const char* names[] = { "OpenAL32.dll", "BF2OpenAL.dll" };
    for(const char* name : names)
    {
        require(loadOpenAL(name), "Windows library name maps to native OpenAL");
        ALCdevice* device = alc_OpenDevice(NULL);
        require(device != NULL, "OpenAL device opens");
        ALCcontext* context = alc_CreateContext(device, NULL);
        require(context != NULL && alc_MakeContextCurrent(context), "OpenAL context activates");
        ALuint source = 0, buffer = 0;
        al_GenSources(1, &source);
        al_GenBuffers(1, &buffer);
        const short samples[800] = {};
        al_BufferData(buffer, AL_FORMAT_MONO16, samples, sizeof(samples), 8000);
        al_Sourcei(source, AL_BUFFER, buffer);
        al_SourcePlay(source);
        require(al_GetError() == 0, "OpenAL accepts a real source and sample buffer");
        al_SourceStop(source);
        al_DeleteSources(1, &source);
        al_DeleteBuffers(1, &buffer);
        alc_MakeContextCurrent(NULL);
        alc_DestroyContext(context);
        require(alc_CloseDevice(device), "OpenAL closes output");
        unloadOpenAL();
    }
    std::puts("PASS: native OpenAL load/device/context/buffer/unload");
}
