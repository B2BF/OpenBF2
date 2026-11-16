// [CHOSEN, not recovered] The macOS client's table of statically linked plugin images; see
// StaticPlugins.h. Built only into the macOS bf2_client executable.

#include "StaticPlugins.h"

#include <string.h>
#include <strings.h>

namespace dice {
namespace hfe {
class IClassManager;
} // namespace hfe
} // namespace dice

namespace dice {
namespace hfe {
// The renamed plugin entry pairs (CMakeLists.txt, bf2_renddx9_static / bf2_swiffplayer_static):
// each image's extern "C" initDll/deinitDll is compiled under an image-specific name so that the
// three images can share one link.
#if defined(BF2_STATIC_RENDDX9)
extern "C" bool bf2_renddx9_initDll(IClassManager* classManager);
extern "C" void bf2_renddx9_deinitDll();
#endif
#if defined(BF2_STATIC_SWIFFPLAYER)
extern "C" bool bf2_swiffplayer_initDll(IClassManager* classManager);
extern "C" void bf2_swiffplayer_deinitDll();
extern "C" void bf2_swiffplayer_reloadImage();   // SwiffPlayerModule.cpp
#endif
} // namespace hfe
} // namespace dice

#if defined(BF2_STATIC_BF2AUDIO)
class ISoundEngine;
extern "C" ISoundEngine* getSoundEngine();     // BF2Audio's two exports keep their names
extern "C" void shutdownSoundEngine();
#endif

namespace bf2staticplugins {
namespace {

struct Export
{
    const char* name;
    void* address;
};

struct Image
{
    const char* stem;
    const Export* exports;
    void (*reload)();   // rebuilds the image's static module table; null: freed only at shutdown
    bool deinitOnFree;  // FreeLibraryBf calls the image's deinitDll (see unloadsWithDeinit)
};

#if defined(BF2_STATIC_RENDDX9)
const Export kRendDX9Exports[] = {
    { "initDll",   reinterpret_cast<void*>(&dice::hfe::bf2_renddx9_initDll) },
    { "deinitDll", reinterpret_cast<void*>(&dice::hfe::bf2_renddx9_deinitDll) },
    { 0, 0 },
};
#endif

#if defined(BF2_STATIC_SWIFFPLAYER)
const Export kSwiffPlayerExports[] = {
    { "initDll",   reinterpret_cast<void*>(&dice::hfe::bf2_swiffplayer_initDll) },
    { "deinitDll", reinterpret_cast<void*>(&dice::hfe::bf2_swiffplayer_deinitDll) },
    { 0, 0 },
};
#endif

#if defined(BF2_STATIC_BF2AUDIO)
const Export kBF2AudioExports[] = {
    { "getSoundEngine",      reinterpret_cast<void*>(&getSoundEngine) },
    { "shutdownSoundEngine", reinterpret_cast<void*>(&shutdownSoundEngine) },
    { 0, 0 },
};
#endif

// An image that is not linked in has no row, so the loader reports it missing exactly as a
// failed dlopen would; the null row keeps the array non-empty.
const Image kImages[] = {
#if defined(BF2_STATIC_RENDDX9)
#if defined(__APPLE__) || defined(BF2_LINUX_CLIENT)
    // [CHOSEN, not recovered] BF2.exe frees RendDX9.dll once, at engine shutdown: shutdownEngine
    // (0x408AB0) deletes BF2Render after the window, and ~BF2Render (0x4548B0) passes the handle
    // to FreeLibraryBf (0x54CD80: deinitDll, then FreeLibrary). The linked image gets the same
    // deinitDll at that call; it is never loaded again, so it needs no reload.
    { "RendDX9",     kRendDX9Exports, 0, true },
#else
    { "RendDX9",     kRendDX9Exports, 0, false },
#endif
#endif
#if defined(BF2_STATIC_SWIFFPLAYER)
    { "SwiffPlayer", kSwiffPlayerExports, &dice::hfe::bf2_swiffplayer_reloadImage, true },
#endif
#if defined(BF2_STATIC_BF2AUDIO)
    { "BF2Audio",    kBF2AudioExports, 0, false },
#endif
    { 0, 0, 0, false },
};

const unsigned kImageCount = sizeof(kImages) / sizeof(kImages[0]) - 1;

// Per image: released by FreeLibrary and not yet handed out again.
bool s_released[sizeof(kImages) / sizeof(kImages[0])];

int indexOf(const void* handle)
{
    for (unsigned i = 0; i < kImageCount; ++i)
        if (handle == &kImages[i])
            return static_cast<int>(i);
    return -1;
}

} // namespace

void* find(const char* stem)
{
    for (unsigned i = 0; i < kImageCount; ++i)
        if (::strcasecmp(kImages[i].stem, stem) == 0)
        {
            // The "load" of a released image: what a fresh DLL mapping would give initDll.
            if (s_released[i] && kImages[i].reload)
                kImages[i].reload();
            s_released[i] = false;
            return const_cast<Image*>(&kImages[i]);
        }
    return 0;
}

bool unloadsWithDeinit(const void* handle)
{
    const int i = indexOf(handle);
    return i >= 0 && kImages[i].deinitOnFree;
}

void release(const void* handle)
{
    const int i = indexOf(handle);
    if (i >= 0)
        s_released[i] = true;
}

bool isStaticHandle(const void* handle)
{
    for (unsigned i = 0; i < kImageCount; ++i)
        if (handle == &kImages[i])
            return true;
    return false;
}

void* symbol(const void* handle, const char* name)
{
    const Image* image = static_cast<const Image*>(handle);
    for (const Export* e = image->exports; e->name; ++e)
        if (::strcmp(e->name, name) == 0)
            return e->address;
    return 0;
}

} // namespace bf2staticplugins
