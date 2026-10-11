// The installed DirectX SDK's D3DX9 interface for RendDX9.dll and TextureAtlasBuilder.dll.
//
// Both shipped DLLs import d3dx9_25.dll, the D3DX of the April 2005 DirectX SDK
// (D3DX_SDK_VERSION 25). Code/Build/dxsdk.cmd locates that SDK, appends its Include directory
// after the Windows SDK and selects Lib\x86 or Lib\x64, so the same declarations and the same
// D3DX generation serve 32- and 64-bit builds. Include this header only from translation units
// built through that helper; the engine images compile without any DirectX SDK.
//
// The Windows SDK supplies <d3d9.h>; it is included first so d3dx9.h's own "d3d9.h" is skipped by
// the include guard and both renderer DLLs keep one D3D9 declaration set.

#ifndef RENDDX9ABI_D3DX9SDK_H
#define RENDDX9ABI_D3DX9SDK_H

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>

#if !defined(D3DX_SDK_VERSION) || D3DX_SDK_VERSION != 25
#error "RendDX9 builds against the April 2005 DirectX SDK (D3DX_SDK_VERSION 25); see Code/Build/dxsdk.cmd"
#endif

namespace renddx9abi {

// Engine records carry D3DX parameter and technique handles as void*; D3DX takes them as
// D3DXHANDLE (const char*). These two conversions keep that boundary in one place.
inline D3DXHANDLE d3dxHandle(const void* handle)
{
    return static_cast<D3DXHANDLE>(handle);
}

inline void* d3dxHandleValue(D3DXHANDLE handle)
{
    return const_cast<char*>(handle);
}

} // namespace renddx9abi

#endif // RENDDX9ABI_D3DX9SDK_H
