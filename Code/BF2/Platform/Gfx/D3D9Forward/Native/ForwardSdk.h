// [CHOSEN, not recovered] DXVK Native boundary, https://github.com/kr4uzi/OpenBF2/issues/36.
// Only this TU sees the complete native SDK. Engine TUs retain the reduced interfaces.
#pragma once
#include "../../../../Core/PlatformCompat.h"
#include <d3d9.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

// MinGW's public header spells this enum value as a macro, unlike our subset.
static constexpr D3DTRANSFORMSTATETYPE bf2SdkWorld = D3DTS_WORLD;
#undef D3DTS_WORLD
static constexpr D3DTRANSFORMSTATETYPE D3DTS_WORLD = bf2SdkWorld;

#include "ForwardMacroValues.inc"
#include "ForwardUndef.inc"
#include "ForwardRename.inc"
#define BF2_GFX_EXTERNAL_PLATFORM_TYPES 1
namespace sub {
#include "../../D3D9Subset.h"
}
#undef BF2_GFX_EXTERNAL_PLATFORM_TYPES
#include "ForwardUnrename.inc"
#include "ForwardNames.inc"

static_assert(sizeof(HRESULT) == 4 && sizeof(DWORD) == 4 && sizeof(ULONG) == 4,
              "DXVK uses fixed-width D3D values on both x86-64 and ARM64");
static_assert(sizeof(HWND) == sizeof(void*), "DXVK's SDL3 HWND holds an SDL_Window pointer");
