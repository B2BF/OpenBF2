// Engine-side plugin loading and unloading. [INFERRED] Source placement.

#ifndef CORE_PLUGINDLL_H
#define CORE_PLUGINDLL_H

#if !defined(_WIN32)
// [PLATFORM] ELF wrappers over dlopen/dlsym/dlclose, defined in PluginDll.cpp.
void* LoadLibrary(const char* name);
void* GetProcAddress(void* handle, const char* name);
bool  FreeLibrary(void* handle);
#endif

namespace dice {
namespace hfe {

// [PLATFORM] The Win32 image was built with /Gr, so these free functions receive their first two
// arguments in ECX/EDX.  The symbolized ELF uses the platform's ordinary calling convention.
#if defined(_MSC_VER)
void* __fastcall LoadLibraryBf(const char* name);
bool  __fastcall FreeLibraryBf(void* handle, const char* diagnosticName);
#else
void* LoadLibraryBf(const char* name);
bool  FreeLibraryBf(void* handle, const char* diagnosticName);
#endif

} // namespace hfe
} // namespace dice

#endif // CORE_PLUGINDLL_H
