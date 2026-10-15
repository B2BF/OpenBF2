// [CHOSEN, not recovered] The display-mode records BF2.exe and RendDX9.dll exchange: the host's
// display-mode carrier (hWndBf2TempSlt) and the mode list Renderer +0x3C returns. Both images are
// built with one toolset and share its STL (issue #7), so both use std::vector.

#ifndef RENDDX9ABI_DISPLAYMODEABI_H
#define RENDDX9ABI_DISPLAYMODEABI_H

#include "PluginAbi.h"

#include <new>
#include <vector>

namespace renddx9abi {

struct DisplayModeMultiSample
{
    unsigned int type;
    unsigned int quality;
};

static_assert(sizeof(DisplayModeMultiSample) == 8,
              "the display-mode multisample record is two dwords");

// One enumerated display mode: the record Renderer +0x3C returns a std::vector of (retail: the
// 40-byte record sub_1000CEE0 builds; sub_100091E0 copies it and sub_10009510 frees it). The
// scalars are copied by BF2.exe's 0x4547F0, which skips the pad byte.
struct DisplayModeRecord
{
    /* +0x00 */ unsigned short width;
    /* +0x02 */ unsigned short height;
    /* +0x04 */ unsigned short refresh;
    /* +0x06 */ unsigned char  bpp;
    /* +0x07 */ unsigned char  pad;
    /* +0x08 */ std::vector<unsigned int>           depthFormats;  // CheckDeviceFormat results
    /* +0x18 */ std::vector<DisplayModeMultiSample> multiSample;   // CheckDeviceMultiSampleType

    DisplayModeRecord() : width(0), height(0), refresh(0), bpp(0), pad(0) {}
};


// The host's display-mode carrier (hWndBf2TempSlt). Both images compile this definition with /MD and
// _ITERATOR_DEBUG_LEVEL=0, as enforced by PluginAbi.h, so its std::vectors cross directly.
struct HostDisplayMode
{
    unsigned char fullscreen;
    unsigned char pad01[3];
    unsigned short width;
    unsigned short height;
    unsigned short refreshHz;
    unsigned char bpp;
    unsigned char pad0B;
    std::vector<unsigned int> depthFormats;
    std::vector<DisplayModeMultiSample> multiSamples;
    unsigned char vsync;
    unsigned char pad25[3];
    std::vector<void*> windows;

    HostDisplayMode()
        : fullscreen(0), width(0), height(0), refreshHz(0), bpp(0), vsync(0)
    {
        pad01[0] = pad01[1] = pad01[2] = 0;
        pad0B = 0;
        pad25[0] = pad25[1] = pad25[2] = 0;
    }
};



inline const unsigned int* hostDepthFirst(const HostDisplayMode& mode)
{
    return mode.depthFormats.empty() ? 0 : &mode.depthFormats[0];
}

inline const unsigned int* hostDepthLast(const HostDisplayMode& mode)
{
    return mode.depthFormats.empty() ? 0 : (&mode.depthFormats[0] + mode.depthFormats.size());
}

inline const DisplayModeMultiSample* hostMultiSampleFirst(const HostDisplayMode& mode)
{
    return mode.multiSamples.empty() ? 0 : &mode.multiSamples[0];
}

inline const DisplayModeMultiSample* hostMultiSampleLast(const HostDisplayMode& mode)
{
    return mode.multiSamples.empty() ? 0 : (&mode.multiSamples[0] + mode.multiSamples.size());
}

inline void* const* hostWindowFirst(const HostDisplayMode& mode)
{
    return mode.windows.empty() ? 0 : &mode.windows[0];
}

inline void* const* hostWindowLast(const HostDisplayMode& mode)
{
    return mode.windows.empty() ? 0 : (&mode.windows[0] + mode.windows.size());
}

} // namespace renddx9abi

#endif // RENDDX9ABI_DISPLAYMODEABI_H
