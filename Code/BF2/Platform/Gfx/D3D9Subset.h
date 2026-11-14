// [CHOSEN, not recovered] Public D3D9 API subset for the native graphics seam.
// Declarations only; no SDK implementation or vendor header is embedded here.
#ifndef BF2_GFX_D3D9SUBSET_H
#define BF2_GFX_D3D9SUBSET_H

#include "../../Core/PlatformCompat.h"
#include <cstdint>
#include <cstddef>
#if defined(_WIN32)
#include <windows.h>
#include <unknwn.h>
#else
using HRESULT = std::int32_t;
using BOOL = std::int32_t;
using LONG = std::int32_t;
using DWORD = std::uint32_t;
using ULONG = std::uint32_t;
using UINT = unsigned int;
using INT = int;
using WORD = std::uint16_t;
using BYTE = std::uint8_t;
using FLOAT = float;
using HANDLE = void*;
using HWND = void*;
using HMONITOR = void*;
using HDC = void*;
using LPCSTR = const char*;
using LPSTR = char*;
using LPCVOID = const void*;
using LPVOID = void*;
struct RECT { LONG left, top, right, bottom; };
struct POINT { LONG x, y; };
struct GUID { std::uint32_t Data1; std::uint16_t Data2, Data3; std::uint8_t Data4[8]; };
using REFIID = const GUID&;
using REFGUID = const GUID&;
union LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; }; std::int64_t QuadPart; };
struct RGNDATA;
struct PALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; };
#define WINAPI __stdcall
#define STDMETHODCALLTYPE __stdcall
#define TRUE 1
#define FALSE 0
#define SUCCEEDED(hr) (static_cast<HRESULT>(hr) >= 0)
#define FAILED(hr) (static_cast<HRESULT>(hr) < 0)
#define S_OK static_cast<HRESULT>(0)
#define S_FALSE static_cast<HRESULT>(1)
#define E_NOINTERFACE static_cast<HRESULT>(0x80004002u)
#define E_OUTOFMEMORY static_cast<HRESULT>(0x8007000eu)
#define E_FAIL static_cast<HRESULT>(0x80004005u)
#define E_NOTIMPL static_cast<HRESULT>(0x80004001u)
struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;
};
#endif

using D3DCOLOR = DWORD;
#define D3D_OK S_OK
#define D3D_SDK_VERSION 32
#define D3DERR_DEVICELOST static_cast<HRESULT>(0x88760868u)
#define D3DERR_DEVICENOTRESET static_cast<HRESULT>(0x88760869u)
#define D3DERR_NOTAVAILABLE static_cast<HRESULT>(0x8876086au)
#define D3DERR_OUTOFVIDEOMEMORY static_cast<HRESULT>(0x8876017cu)
#define D3DERR_INVALIDCALL static_cast<HRESULT>(0x8876086cu)
#define D3DERR_NOTFOUND static_cast<HRESULT>(0x88760866u)
#define D3DERR_MOREDATA static_cast<HRESULT>(0x88760867u)
#define D3DERR_DRIVERINTERNALERROR static_cast<HRESULT>(0x88760827u)
#define BF2_D3D_FOURCC(a,b,c,d) (DWORD(BYTE(a)) | (DWORD(BYTE(b)) << 8) | (DWORD(BYTE(c)) << 16) | (DWORD(BYTE(d)) << 24))

enum D3DFORMAT {
    D3DFMT_UNKNOWN=0, D3DFMT_R8G8B8=20, D3DFMT_A8R8G8B8=21, D3DFMT_X8R8G8B8=22,
    D3DFMT_R5G6B5=23, D3DFMT_X1R5G5B5=24, D3DFMT_A1R5G5B5=25, D3DFMT_A4R4G4B4=26,
    D3DFMT_R3G3B2=27, D3DFMT_A8=28, D3DFMT_A8R3G3B2=29, D3DFMT_X4R4G4B4=30,
    D3DFMT_A2B10G10R10=31, D3DFMT_A8B8G8R8=32, D3DFMT_X8B8G8R8=33,
    D3DFMT_G16R16=34, D3DFMT_A2R10G10B10=35, D3DFMT_A16B16G16R16=36,
    D3DFMT_A8P8=40, D3DFMT_P8=41, D3DFMT_L8=50, D3DFMT_A8L8=51, D3DFMT_A4L4=52,
    D3DFMT_V8U8=60, D3DFMT_L6V5U5=61, D3DFMT_X8L8V8U8=62, D3DFMT_Q8W8V8U8=63,
    D3DFMT_V16U16=64, D3DFMT_A2W10V10U10=67,
    D3DFMT_UYVY=BF2_D3D_FOURCC('U','Y','V','Y'), D3DFMT_YUY2=BF2_D3D_FOURCC('Y','U','Y','2'),
    D3DFMT_R8G8_B8G8=BF2_D3D_FOURCC('R','G','B','G'), D3DFMT_G8R8_G8B8=BF2_D3D_FOURCC('G','R','G','B'),
    D3DFMT_DXT1=BF2_D3D_FOURCC('D','X','T','1'), D3DFMT_DXT2=BF2_D3D_FOURCC('D','X','T','2'),
    D3DFMT_DXT3=BF2_D3D_FOURCC('D','X','T','3'), D3DFMT_DXT4=BF2_D3D_FOURCC('D','X','T','4'),
    D3DFMT_DXT5=BF2_D3D_FOURCC('D','X','T','5'), D3DFMT_D32=71, D3DFMT_D24S8=75,
    D3DFMT_D24X8=77, D3DFMT_D16=80, D3DFMT_L16=81, D3DFMT_VERTEXDATA=100,
    D3DFMT_INDEX16=101, D3DFMT_INDEX32=102, D3DFMT_Q16W16V16U16=110,
    D3DFMT_R16F=111, D3DFMT_G16R16F=112, D3DFMT_A16B16G16R16F=113,
    D3DFMT_R32F=114, D3DFMT_G32R32F=115, D3DFMT_A32B32G32R32F=116, D3DFMT_CxV8U8=117,
    D3DFMT_FORCE_DWORD=0x7fffffff
};
enum D3DDEVTYPE { D3DDEVTYPE_HAL=1, D3DDEVTYPE_REF=2, D3DDEVTYPE_SW=3 };
enum D3DRESOURCETYPE { D3DRTYPE_SURFACE=1, D3DRTYPE_VOLUME=2, D3DRTYPE_TEXTURE=3,
    D3DRTYPE_VOLUMETEXTURE=4, D3DRTYPE_CUBETEXTURE=5, D3DRTYPE_VERTEXBUFFER=6, D3DRTYPE_INDEXBUFFER=7 };
enum D3DPOOL { D3DPOOL_DEFAULT=0, D3DPOOL_MANAGED=1, D3DPOOL_SYSTEMMEM=2, D3DPOOL_SCRATCH=3 };
enum D3DMULTISAMPLE_TYPE { D3DMULTISAMPLE_NONE=0, D3DMULTISAMPLE_NONMASKABLE=1,
    D3DMULTISAMPLE_2_SAMPLES=2, D3DMULTISAMPLE_4_SAMPLES=4, D3DMULTISAMPLE_8_SAMPLES=8 };
enum D3DSWAPEFFECT { D3DSWAPEFFECT_DISCARD=1, D3DSWAPEFFECT_FLIP=2, D3DSWAPEFFECT_COPY=3 };
enum D3DBACKBUFFER_TYPE { D3DBACKBUFFER_TYPE_MONO=0, D3DBACKBUFFER_TYPE_LEFT=1, D3DBACKBUFFER_TYPE_RIGHT=2 };
enum D3DTEXTUREFILTERTYPE { D3DTEXF_NONE=0, D3DTEXF_POINT=1, D3DTEXF_LINEAR=2, D3DTEXF_ANISOTROPIC=3 };
enum D3DCUBEMAP_FACES { D3DCUBEMAP_FACE_POSITIVE_X=0, D3DCUBEMAP_FACE_NEGATIVE_X=1,
    D3DCUBEMAP_FACE_POSITIVE_Y=2, D3DCUBEMAP_FACE_NEGATIVE_Y=3,
    D3DCUBEMAP_FACE_POSITIVE_Z=4, D3DCUBEMAP_FACE_NEGATIVE_Z=5 };
enum D3DPRIMITIVETYPE { D3DPT_POINTLIST=1, D3DPT_LINELIST=2, D3DPT_LINESTRIP=3,
    D3DPT_TRIANGLELIST=4, D3DPT_TRIANGLESTRIP=5, D3DPT_TRIANGLEFAN=6 };
enum D3DQUERYTYPE { D3DQUERYTYPE_EVENT=8, D3DQUERYTYPE_OCCLUSION=9 };
enum D3DRENDERSTATETYPE { D3DRS_ZENABLE=7, D3DRS_CULLMODE=22, D3DRS_ZFUNC=23,
    D3DRS_STENCILENABLE=52, D3DRS_STENCILFAIL=53, D3DRS_STENCILZFAIL=54,
    D3DRS_STENCILPASS=55, D3DRS_STENCILFUNC=56, D3DRS_STENCILREF=57,
    D3DRS_MULTISAMPLEANTIALIAS=161, D3DRS_SCISSORTESTENABLE=174 };
enum D3DTEXTURESTAGESTATETYPE { D3DTSS_TEXTURETRANSFORMFLAGS=24 };
enum D3DSAMPLERSTATETYPE { D3DSAMP_ADDRESSU=1, D3DSAMP_ADDRESSV=2, D3DSAMP_ADDRESSW=3,
    D3DSAMP_BORDERCOLOR=4, D3DSAMP_MAGFILTER=5, D3DSAMP_MINFILTER=6,
    D3DSAMP_MIPFILTER=7, D3DSAMP_MIPMAPLODBIAS=8, D3DSAMP_MAXMIPLEVEL=9,
    D3DSAMP_MAXANISOTROPY=10, D3DSAMP_SRGBTEXTURE=11, D3DSAMP_ELEMENTINDEX=12, D3DSAMP_DMAPOFFSET=13 };
enum D3DTRANSFORMSTATETYPE { D3DTS_VIEW=2, D3DTS_PROJECTION=3, D3DTS_TEXTURE0=16, D3DTS_WORLD=256 };
enum D3DSTATEBLOCKTYPE { D3DSBT_ALL=1, D3DSBT_PIXELSTATE=2, D3DSBT_VERTEXSTATE=3 };
enum D3DCMPFUNC { D3DCMP_NEVER=1, D3DCMP_LESS=2, D3DCMP_EQUAL=3,
    D3DCMP_LESSEQUAL=4, D3DCMP_GREATER=5, D3DCMP_NOTEQUAL=6, D3DCMP_GREATEREQUAL=7, D3DCMP_ALWAYS=8 };
enum D3DCULL { D3DCULL_NONE=1, D3DCULL_CW=2, D3DCULL_CCW=3 };
enum D3DSTENCILOP { D3DSTENCILOP_KEEP=1, D3DSTENCILOP_ZERO=2, D3DSTENCILOP_REPLACE=3 };
enum D3DDECLTYPE { D3DDECLTYPE_FLOAT1=0, D3DDECLTYPE_FLOAT2=1, D3DDECLTYPE_FLOAT3=2,
    D3DDECLTYPE_FLOAT4=3, D3DDECLTYPE_D3DCOLOR=4, D3DDECLTYPE_UBYTE4=5,
    D3DDECLTYPE_SHORT2=6, D3DDECLTYPE_SHORT4=7, D3DDECLTYPE_UNUSED=17 };
enum D3DDECLMETHOD { D3DDECLMETHOD_DEFAULT=0 };
enum D3DDECLUSAGE { D3DDECLUSAGE_POSITION=0, D3DDECLUSAGE_BLENDWEIGHT=1,
    D3DDECLUSAGE_BLENDINDICES=2, D3DDECLUSAGE_NORMAL=3, D3DDECLUSAGE_PSIZE=4,
    D3DDECLUSAGE_TEXCOORD=5, D3DDECLUSAGE_TANGENT=6, D3DDECLUSAGE_BINORMAL=7,
    D3DDECLUSAGE_TESSFACTOR=8, D3DDECLUSAGE_POSITIONT=9, D3DDECLUSAGE_COLOR=10 };
constexpr DWORD D3DCLEAR_TARGET=1, D3DCLEAR_ZBUFFER=2, D3DCLEAR_STENCIL=4;
constexpr DWORD D3DUSAGE_RENDERTARGET=1, D3DUSAGE_DEPTHSTENCIL=2, D3DUSAGE_WRITEONLY=8, D3DUSAGE_DYNAMIC=0x200;
constexpr DWORD D3DLOCK_READONLY=0x10, D3DLOCK_NOOVERWRITE=0x1000, D3DLOCK_DISCARD=0x2000;
constexpr DWORD D3DISSUE_END=1, D3DGETDATA_FLUSH=1, D3DFVF_XYZ=2;
// D3DFVF (d3d9types.h), for SetFVF's declaration mapping (Metal/FixedFunctionDraw.inc)
constexpr DWORD D3DFVF_POSITION_MASK=0x400E, D3DFVF_XYZRHW=0x004, D3DFVF_XYZB1=0x006, D3DFVF_XYZB2=0x008,
    D3DFVF_XYZB3=0x00a, D3DFVF_XYZB4=0x00c, D3DFVF_XYZB5=0x00e, D3DFVF_XYZW=0x4002, D3DFVF_NORMAL=0x010,
    D3DFVF_PSIZE=0x020, D3DFVF_DIFFUSE=0x040, D3DFVF_SPECULAR=0x080, D3DFVF_TEXCOUNT_MASK=0xf00,
    D3DFVF_TEXCOUNT_SHIFT=8, D3DFVF_TEX1=0x100, D3DFVF_TEX2=0x200, D3DFVF_LASTBETA_UBYTE4=0x1000, D3DFVF_LASTBETA_D3DCOLOR=0x8000;
constexpr DWORD D3DPRESENT_INTERVAL_ONE=1, D3DPRESENT_INTERVAL_IMMEDIATE=0x80000000u;
constexpr DWORD D3DCAPS2_DYNAMICTEXTURES=0x20000000, D3DPTEXTURECAPS_POW2=2, D3DPTEXTURECAPS_NONPOW2CONDITIONAL=0x100;

struct D3DVECTOR { float x,y,z; };
struct D3DCOLORVALUE { float r,g,b,a; };
struct D3DMATRIX {
    union { struct { float _11,_12,_13,_14,_21,_22,_23,_24,_31,_32,_33,_34,_41,_42,_43,_44; }; float m[4][4]; };
};
struct D3DRECT { LONG x1,y1,x2,y2; };
struct D3DBOX { UINT Left,Top,Right,Bottom,Front,Back; };
struct D3DVIEWPORT9 { DWORD X,Y,Width,Height; float MinZ,MaxZ; };
struct D3DLOCKED_RECT { INT Pitch; void* pBits; };
struct D3DLOCKED_BOX { INT RowPitch,SlicePitch; void* pBits; };
struct D3DDISPLAYMODE { UINT Width,Height,RefreshRate; D3DFORMAT Format; };
struct D3DPRESENT_PARAMETERS {
    UINT BackBufferWidth,BackBufferHeight; D3DFORMAT BackBufferFormat; UINT BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; D3DSWAPEFFECT SwapEffect;
    HWND hDeviceWindow; BOOL Windowed,EnableAutoDepthStencil; D3DFORMAT AutoDepthStencilFormat;
    DWORD Flags; UINT FullScreen_RefreshRateInHz,PresentationInterval;
};
// Win32 x86 packs the driver-version union to four bytes; Win64/native LP64
// retains eight-byte alignment. Keep the existing x86 1100-byte renderer contract.
#if defined(_WIN32) && !defined(_WIN64)
#pragma pack(push,4)
#endif
struct D3DADAPTER_IDENTIFIER9 {
    char Driver[512],Description[512],DeviceName[32]; LARGE_INTEGER DriverVersion;
    DWORD VendorId,DeviceId,SubSysId,Revision; GUID DeviceIdentifier; DWORD WHQLLevel;
};
#if defined(_WIN32) && !defined(_WIN64)
#pragma pack(pop)
#endif
struct D3DSURFACE_DESC { D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; UINT Width,Height; };
struct D3DVOLUME_DESC { D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool; UINT Width,Height,Depth; };
struct D3DVERTEXBUFFER_DESC { D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool; UINT Size; DWORD FVF; };
struct D3DINDEXBUFFER_DESC { D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool; UINT Size; };
struct D3DVERTEXELEMENT9 { WORD Stream,Offset; BYTE Type,Method,Usage,UsageIndex; };
#define D3DDECL_END() {0xff,0,D3DDECLTYPE_UNUSED,0,0,0}
struct D3DDEVICE_CREATION_PARAMETERS { UINT AdapterOrdinal; D3DDEVTYPE DeviceType; HWND hFocusWindow; DWORD BehaviorFlags; };
struct D3DRASTER_STATUS { BOOL InVBlank; UINT ScanLine; };
struct D3DGAMMARAMP { WORD red[256],green[256],blue[256]; };
struct D3DMATERIAL9 { D3DCOLORVALUE Diffuse,Ambient,Specular,Emissive; float Power; };
struct D3DLIGHT9;
struct D3DCLIPSTATUS9 { DWORD ClipUnion,ClipIntersection; };
struct D3DRECTPATCH_INFO;
struct D3DTRIPATCH_INFO;
struct D3DVSHADERCAPS2_0 { DWORD Caps; INT DynamicFlowControlDepth,NumTemps,StaticFlowControlDepth; };
struct D3DPSHADERCAPS2_0 { DWORD Caps; INT DynamicFlowControlDepth,NumTemps,StaticFlowControlDepth,NumInstructionSlots; };
struct D3DCAPS9 {
    D3DDEVTYPE DeviceType; UINT AdapterOrdinal;
    DWORD Caps,Caps2,Caps3,PresentationIntervals,CursorCaps,DevCaps,PrimitiveMiscCaps,RasterCaps,
        ZCmpCaps,SrcBlendCaps,DestBlendCaps,AlphaCmpCaps,ShadeCaps,TextureCaps,TextureFilterCaps,
        CubeTextureFilterCaps,VolumeTextureFilterCaps,TextureAddressCaps,VolumeTextureAddressCaps,
        LineCaps,MaxTextureWidth,MaxTextureHeight,MaxVolumeExtent,MaxTextureRepeat,MaxTextureAspectRatio,MaxAnisotropy;
    float MaxVertexW,GuardBandLeft,GuardBandTop,GuardBandRight,GuardBandBottom,ExtentsAdjust;
    DWORD StencilCaps,FVFCaps,TextureOpCaps,MaxTextureBlendStages,MaxSimultaneousTextures,
        VertexProcessingCaps,MaxActiveLights,MaxUserClipPlanes,MaxVertexBlendMatrices,MaxVertexBlendMatrixIndex;
    float MaxPointSize;
    DWORD MaxPrimitiveCount,MaxVertexIndex,MaxStreams,MaxStreamStride,VertexShaderVersion,MaxVertexShaderConst,PixelShaderVersion;
    float PixelShader1xMaxValue; DWORD DevCaps2; float MaxNpatchTessellationLevel;
    DWORD Reserved5; UINT MasterAdapterOrdinal,AdapterOrdinalInGroup,NumberOfAdaptersInGroup;
    DWORD DeclTypes,NumSimultaneousRTs,StretchRectFilterCaps;
    D3DVSHADERCAPS2_0 VS20Caps; D3DPSHADERCAPS2_0 PS20Caps;
    DWORD VertexTextureFilterCaps,MaxVShaderInstructionsExecuted,MaxPShaderInstructionsExecuted,
        MaxVertexShader30InstructionSlots,MaxPixelShader30InstructionSlots;
};

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DResource9;
struct IDirect3DBaseTexture9;
struct IDirect3DTexture9;
struct IDirect3DVolumeTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3DSurface9;
struct IDirect3DVolume9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DSwapChain9;
struct IDirect3DVertexDeclaration9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DQuery9;
struct IDirect3DStateBlock9;

// All slots preceding the last used operation are present. No virtual destructors:
// COM lifetime is Release(), including on the native Itanium C++ ABI.
#define BF2_D3D_METHOD(ret,name,args) virtual ret STDMETHODCALLTYPE name args = 0
struct IDirect3D9 : IUnknown {
    BF2_D3D_METHOD(HRESULT,RegisterSoftwareDevice,(void*));
    BF2_D3D_METHOD(UINT,GetAdapterCount,());
    BF2_D3D_METHOD(HRESULT,GetAdapterIdentifier,(UINT,DWORD,D3DADAPTER_IDENTIFIER9*));
    BF2_D3D_METHOD(UINT,GetAdapterModeCount,(UINT,D3DFORMAT));
    BF2_D3D_METHOD(HRESULT,EnumAdapterModes,(UINT,D3DFORMAT,UINT,D3DDISPLAYMODE*));
    BF2_D3D_METHOD(HRESULT,GetAdapterDisplayMode,(UINT,D3DDISPLAYMODE*));
    BF2_D3D_METHOD(HRESULT,CheckDeviceType,(UINT,D3DDEVTYPE,D3DFORMAT,D3DFORMAT,BOOL));
    BF2_D3D_METHOD(HRESULT,CheckDeviceFormat,(UINT,D3DDEVTYPE,D3DFORMAT,DWORD,D3DRESOURCETYPE,D3DFORMAT));
    BF2_D3D_METHOD(HRESULT,CheckDeviceMultiSampleType,(UINT,D3DDEVTYPE,D3DFORMAT,BOOL,D3DMULTISAMPLE_TYPE,DWORD*));
    BF2_D3D_METHOD(HRESULT,CheckDepthStencilMatch,(UINT,D3DDEVTYPE,D3DFORMAT,D3DFORMAT,D3DFORMAT));
    BF2_D3D_METHOD(HRESULT,CheckDeviceFormatConversion,(UINT,D3DDEVTYPE,D3DFORMAT,D3DFORMAT));
    BF2_D3D_METHOD(HRESULT,GetDeviceCaps,(UINT,D3DDEVTYPE,D3DCAPS9*));
    BF2_D3D_METHOD(HMONITOR,GetAdapterMonitor,(UINT));
    BF2_D3D_METHOD(HRESULT,CreateDevice,(UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**));
};
struct IDirect3DResource9 : IUnknown {
    BF2_D3D_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3D_METHOD(HRESULT,SetPrivateData,(REFGUID,const void*,DWORD,DWORD));
    BF2_D3D_METHOD(HRESULT,GetPrivateData,(REFGUID,void*,DWORD*));
    BF2_D3D_METHOD(HRESULT,FreePrivateData,(REFGUID));
    BF2_D3D_METHOD(DWORD,SetPriority,(DWORD));
    BF2_D3D_METHOD(DWORD,GetPriority,());
    BF2_D3D_METHOD(void,PreLoad,());
    BF2_D3D_METHOD(D3DRESOURCETYPE,GetType,());
};
struct IDirect3DBaseTexture9 : IDirect3DResource9 {
    BF2_D3D_METHOD(DWORD,SetLOD,(DWORD));
    BF2_D3D_METHOD(DWORD,GetLOD,());
    BF2_D3D_METHOD(DWORD,GetLevelCount,());
    BF2_D3D_METHOD(HRESULT,SetAutoGenFilterType,(D3DTEXTUREFILTERTYPE));
    BF2_D3D_METHOD(D3DTEXTUREFILTERTYPE,GetAutoGenFilterType,());
    BF2_D3D_METHOD(void,GenerateMipSubLevels,());
};
struct IDirect3DTexture9 : IDirect3DBaseTexture9 {
    BF2_D3D_METHOD(HRESULT,GetLevelDesc,(UINT,D3DSURFACE_DESC*));
    BF2_D3D_METHOD(HRESULT,GetSurfaceLevel,(UINT,IDirect3DSurface9**));
    BF2_D3D_METHOD(HRESULT,LockRect,(UINT,D3DLOCKED_RECT*,const RECT*,DWORD));
    BF2_D3D_METHOD(HRESULT,UnlockRect,(UINT));
};
struct IDirect3DCubeTexture9 : IDirect3DBaseTexture9 {
    BF2_D3D_METHOD(HRESULT,GetLevelDesc,(UINT,D3DSURFACE_DESC*));
    BF2_D3D_METHOD(HRESULT,GetCubeMapSurface,(D3DCUBEMAP_FACES,UINT,IDirect3DSurface9**));
    BF2_D3D_METHOD(HRESULT,LockRect,(D3DCUBEMAP_FACES,UINT,D3DLOCKED_RECT*,const RECT*,DWORD));
    BF2_D3D_METHOD(HRESULT,UnlockRect,(D3DCUBEMAP_FACES,UINT));
};
struct IDirect3DVolumeTexture9 : IDirect3DBaseTexture9 {
    BF2_D3D_METHOD(HRESULT,GetLevelDesc,(UINT,D3DVOLUME_DESC*));
    BF2_D3D_METHOD(HRESULT,GetVolumeLevel,(UINT,IDirect3DVolume9**));
    BF2_D3D_METHOD(HRESULT,LockBox,(UINT,D3DLOCKED_BOX*,const D3DBOX*,DWORD));
    BF2_D3D_METHOD(HRESULT,UnlockBox,(UINT));
};
struct IDirect3DSurface9 : IDirect3DResource9 {
    BF2_D3D_METHOD(HRESULT,GetContainer,(REFIID,void**));
    BF2_D3D_METHOD(HRESULT,GetDesc,(D3DSURFACE_DESC*));
    BF2_D3D_METHOD(HRESULT,LockRect,(D3DLOCKED_RECT*,const RECT*,DWORD));
    BF2_D3D_METHOD(HRESULT,UnlockRect,());
};
struct IDirect3DVertexBuffer9 : IDirect3DResource9 {
    BF2_D3D_METHOD(HRESULT,Lock,(UINT,UINT,void**,DWORD));
    BF2_D3D_METHOD(HRESULT,Unlock,());
    BF2_D3D_METHOD(HRESULT,GetDesc,(D3DVERTEXBUFFER_DESC*));
};
struct IDirect3DIndexBuffer9 : IDirect3DResource9 {
    BF2_D3D_METHOD(HRESULT,Lock,(UINT,UINT,void**,DWORD));
    BF2_D3D_METHOD(HRESULT,Unlock,());
    BF2_D3D_METHOD(HRESULT,GetDesc,(D3DINDEXBUFFER_DESC*));
};
struct IDirect3DSwapChain9 : IUnknown {
    BF2_D3D_METHOD(HRESULT,Present,(const RECT*,const RECT*,HWND,const RGNDATA*,DWORD));
    BF2_D3D_METHOD(HRESULT,GetFrontBufferData,(IDirect3DSurface9*));
    BF2_D3D_METHOD(HRESULT,GetBackBuffer,(UINT,D3DBACKBUFFER_TYPE,IDirect3DSurface9**));
};
struct IDirect3DVertexDeclaration9 : IUnknown {
    BF2_D3D_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3D_METHOD(HRESULT,GetDeclaration,(D3DVERTEXELEMENT9*,UINT*));
};
struct IDirect3DVertexShader9 : IUnknown {
    BF2_D3D_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3D_METHOD(HRESULT,GetFunction,(void*,UINT*));
};
struct IDirect3DPixelShader9 : IUnknown {
    BF2_D3D_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3D_METHOD(HRESULT,GetFunction,(void*,UINT*));
};
struct IDirect3DQuery9 : IUnknown {
    BF2_D3D_METHOD(HRESULT,GetDevice,(IDirect3DDevice9**));
    BF2_D3D_METHOD(D3DQUERYTYPE,GetType,());
    BF2_D3D_METHOD(DWORD,GetDataSize,());
    BF2_D3D_METHOD(HRESULT,Issue,(DWORD));
    BF2_D3D_METHOD(HRESULT,GetData,(void*,DWORD,DWORD));
};
struct IDirect3DDevice9 : IUnknown {
    BF2_D3D_METHOD(HRESULT,TestCooperativeLevel,());
    BF2_D3D_METHOD(UINT,GetAvailableTextureMem,());
    BF2_D3D_METHOD(HRESULT,EvictManagedResources,());
    BF2_D3D_METHOD(HRESULT,GetDirect3D,(IDirect3D9**));
    BF2_D3D_METHOD(HRESULT,GetDeviceCaps,(D3DCAPS9*));
    BF2_D3D_METHOD(HRESULT,GetDisplayMode,(UINT,D3DDISPLAYMODE*));
    BF2_D3D_METHOD(HRESULT,GetCreationParameters,(D3DDEVICE_CREATION_PARAMETERS*));
    BF2_D3D_METHOD(HRESULT,SetCursorProperties,(UINT,UINT,IDirect3DSurface9*));
    BF2_D3D_METHOD(void,SetCursorPosition,(int,int,DWORD));
    BF2_D3D_METHOD(BOOL,ShowCursor,(BOOL));
    BF2_D3D_METHOD(HRESULT,CreateAdditionalSwapChain,(D3DPRESENT_PARAMETERS*,IDirect3DSwapChain9**));
    BF2_D3D_METHOD(HRESULT,GetSwapChain,(UINT,IDirect3DSwapChain9**));
    BF2_D3D_METHOD(UINT,GetNumberOfSwapChains,());
    BF2_D3D_METHOD(HRESULT,Reset,(D3DPRESENT_PARAMETERS*));
    BF2_D3D_METHOD(HRESULT,Present,(const RECT*,const RECT*,HWND,const RGNDATA*));
    BF2_D3D_METHOD(HRESULT,GetBackBuffer,(UINT,UINT,D3DBACKBUFFER_TYPE,IDirect3DSurface9**));
    BF2_D3D_METHOD(HRESULT,GetRasterStatus,(UINT,D3DRASTER_STATUS*));
    BF2_D3D_METHOD(HRESULT,SetDialogBoxMode,(BOOL));
    BF2_D3D_METHOD(void,SetGammaRamp,(UINT,DWORD,const D3DGAMMARAMP*));
    BF2_D3D_METHOD(void,GetGammaRamp,(UINT,D3DGAMMARAMP*));
    BF2_D3D_METHOD(HRESULT,CreateTexture,(UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**,HANDLE*));
    BF2_D3D_METHOD(HRESULT,CreateVolumeTexture,(UINT,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DVolumeTexture9**,HANDLE*));
    BF2_D3D_METHOD(HRESULT,CreateCubeTexture,(UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DCubeTexture9**,HANDLE*));
    BF2_D3D_METHOD(HRESULT,CreateVertexBuffer,(UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9**,HANDLE*));
    BF2_D3D_METHOD(HRESULT,CreateIndexBuffer,(UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DIndexBuffer9**,HANDLE*));
    BF2_D3D_METHOD(HRESULT,CreateRenderTarget,(UINT,UINT,D3DFORMAT,D3DMULTISAMPLE_TYPE,DWORD,BOOL,IDirect3DSurface9**,HANDLE*));
    BF2_D3D_METHOD(HRESULT,CreateDepthStencilSurface,(UINT,UINT,D3DFORMAT,D3DMULTISAMPLE_TYPE,DWORD,BOOL,IDirect3DSurface9**,HANDLE*));
    BF2_D3D_METHOD(HRESULT,UpdateSurface,(IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,const POINT*));
    BF2_D3D_METHOD(HRESULT,UpdateTexture,(IDirect3DBaseTexture9*,IDirect3DBaseTexture9*));
    BF2_D3D_METHOD(HRESULT,GetRenderTargetData,(IDirect3DSurface9*,IDirect3DSurface9*));
    BF2_D3D_METHOD(HRESULT,GetFrontBufferData,(UINT,IDirect3DSurface9*));
    BF2_D3D_METHOD(HRESULT,StretchRect,(IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,const RECT*,D3DTEXTUREFILTERTYPE));
    BF2_D3D_METHOD(HRESULT,ColorFill,(IDirect3DSurface9*,const RECT*,D3DCOLOR));
    BF2_D3D_METHOD(HRESULT,CreateOffscreenPlainSurface,(UINT,UINT,D3DFORMAT,D3DPOOL,IDirect3DSurface9**,HANDLE*));
    BF2_D3D_METHOD(HRESULT,SetRenderTarget,(DWORD,IDirect3DSurface9*));
    BF2_D3D_METHOD(HRESULT,GetRenderTarget,(DWORD,IDirect3DSurface9**));
    BF2_D3D_METHOD(HRESULT,SetDepthStencilSurface,(IDirect3DSurface9*));
    BF2_D3D_METHOD(HRESULT,GetDepthStencilSurface,(IDirect3DSurface9**));
    BF2_D3D_METHOD(HRESULT,BeginScene,());
    BF2_D3D_METHOD(HRESULT,EndScene,());
    BF2_D3D_METHOD(HRESULT,Clear,(DWORD,const D3DRECT*,DWORD,D3DCOLOR,float,DWORD));
    BF2_D3D_METHOD(HRESULT,SetTransform,(D3DTRANSFORMSTATETYPE,const D3DMATRIX*));
    BF2_D3D_METHOD(HRESULT,GetTransform,(D3DTRANSFORMSTATETYPE,D3DMATRIX*));
    BF2_D3D_METHOD(HRESULT,MultiplyTransform,(D3DTRANSFORMSTATETYPE,const D3DMATRIX*));
    BF2_D3D_METHOD(HRESULT,SetViewport,(const D3DVIEWPORT9*));
    BF2_D3D_METHOD(HRESULT,GetViewport,(D3DVIEWPORT9*));
    BF2_D3D_METHOD(HRESULT,SetMaterial,(const D3DMATERIAL9*));
    BF2_D3D_METHOD(HRESULT,GetMaterial,(D3DMATERIAL9*));
    BF2_D3D_METHOD(HRESULT,SetLight,(DWORD,const D3DLIGHT9*));
    BF2_D3D_METHOD(HRESULT,GetLight,(DWORD,D3DLIGHT9*));
    BF2_D3D_METHOD(HRESULT,LightEnable,(DWORD,BOOL));
    BF2_D3D_METHOD(HRESULT,GetLightEnable,(DWORD,BOOL*));
    BF2_D3D_METHOD(HRESULT,SetClipPlane,(DWORD,const float*));
    BF2_D3D_METHOD(HRESULT,GetClipPlane,(DWORD,float*));
    BF2_D3D_METHOD(HRESULT,SetRenderState,(D3DRENDERSTATETYPE,DWORD));
    BF2_D3D_METHOD(HRESULT,GetRenderState,(D3DRENDERSTATETYPE,DWORD*));
    BF2_D3D_METHOD(HRESULT,CreateStateBlock,(D3DSTATEBLOCKTYPE,IDirect3DStateBlock9**));
    BF2_D3D_METHOD(HRESULT,BeginStateBlock,());
    BF2_D3D_METHOD(HRESULT,EndStateBlock,(IDirect3DStateBlock9**));
    BF2_D3D_METHOD(HRESULT,SetClipStatus,(const D3DCLIPSTATUS9*));
    BF2_D3D_METHOD(HRESULT,GetClipStatus,(D3DCLIPSTATUS9*));
    BF2_D3D_METHOD(HRESULT,GetTexture,(DWORD,IDirect3DBaseTexture9**));
    BF2_D3D_METHOD(HRESULT,SetTexture,(DWORD,IDirect3DBaseTexture9*));
    BF2_D3D_METHOD(HRESULT,GetTextureStageState,(DWORD,D3DTEXTURESTAGESTATETYPE,DWORD*));
    BF2_D3D_METHOD(HRESULT,SetTextureStageState,(DWORD,D3DTEXTURESTAGESTATETYPE,DWORD));
    BF2_D3D_METHOD(HRESULT,GetSamplerState,(DWORD,D3DSAMPLERSTATETYPE,DWORD*));
    BF2_D3D_METHOD(HRESULT,SetSamplerState,(DWORD,D3DSAMPLERSTATETYPE,DWORD));
    BF2_D3D_METHOD(HRESULT,ValidateDevice,(DWORD*));
    BF2_D3D_METHOD(HRESULT,SetPaletteEntries,(UINT,const PALETTEENTRY*));
    BF2_D3D_METHOD(HRESULT,GetPaletteEntries,(UINT,PALETTEENTRY*));
    BF2_D3D_METHOD(HRESULT,SetCurrentTexturePalette,(UINT));
    BF2_D3D_METHOD(HRESULT,GetCurrentTexturePalette,(UINT*));
    BF2_D3D_METHOD(HRESULT,SetScissorRect,(const RECT*));
    BF2_D3D_METHOD(HRESULT,GetScissorRect,(RECT*));
    BF2_D3D_METHOD(HRESULT,SetSoftwareVertexProcessing,(BOOL));
    BF2_D3D_METHOD(BOOL,GetSoftwareVertexProcessing,());
    BF2_D3D_METHOD(HRESULT,SetNPatchMode,(float));
    BF2_D3D_METHOD(float,GetNPatchMode,());
    BF2_D3D_METHOD(HRESULT,DrawPrimitive,(D3DPRIMITIVETYPE,UINT,UINT));
    BF2_D3D_METHOD(HRESULT,DrawIndexedPrimitive,(D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT));
    BF2_D3D_METHOD(HRESULT,DrawPrimitiveUP,(D3DPRIMITIVETYPE,UINT,const void*,UINT));
    BF2_D3D_METHOD(HRESULT,DrawIndexedPrimitiveUP,(D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT));
    BF2_D3D_METHOD(HRESULT,ProcessVertices,(UINT,UINT,UINT,IDirect3DVertexBuffer9*,IDirect3DVertexDeclaration9*,DWORD));
    BF2_D3D_METHOD(HRESULT,CreateVertexDeclaration,(const D3DVERTEXELEMENT9*,IDirect3DVertexDeclaration9**));
    BF2_D3D_METHOD(HRESULT,SetVertexDeclaration,(IDirect3DVertexDeclaration9*));
    BF2_D3D_METHOD(HRESULT,GetVertexDeclaration,(IDirect3DVertexDeclaration9**));
    BF2_D3D_METHOD(HRESULT,SetFVF,(DWORD));
    BF2_D3D_METHOD(HRESULT,GetFVF,(DWORD*));
    BF2_D3D_METHOD(HRESULT,CreateVertexShader,(const DWORD*,IDirect3DVertexShader9**));
    BF2_D3D_METHOD(HRESULT,SetVertexShader,(IDirect3DVertexShader9*));
    BF2_D3D_METHOD(HRESULT,GetVertexShader,(IDirect3DVertexShader9**));
    BF2_D3D_METHOD(HRESULT,SetVertexShaderConstantF,(UINT,const float*,UINT));
    BF2_D3D_METHOD(HRESULT,GetVertexShaderConstantF,(UINT,float*,UINT));
    BF2_D3D_METHOD(HRESULT,SetVertexShaderConstantI,(UINT,const int*,UINT));
    BF2_D3D_METHOD(HRESULT,GetVertexShaderConstantI,(UINT,int*,UINT));
    BF2_D3D_METHOD(HRESULT,SetVertexShaderConstantB,(UINT,const BOOL*,UINT));
    BF2_D3D_METHOD(HRESULT,GetVertexShaderConstantB,(UINT,BOOL*,UINT));
    BF2_D3D_METHOD(HRESULT,SetStreamSource,(UINT,IDirect3DVertexBuffer9*,UINT,UINT));
    BF2_D3D_METHOD(HRESULT,GetStreamSource,(UINT,IDirect3DVertexBuffer9**,UINT*,UINT*));
    BF2_D3D_METHOD(HRESULT,SetStreamSourceFreq,(UINT,UINT));
    BF2_D3D_METHOD(HRESULT,GetStreamSourceFreq,(UINT,UINT*));
    BF2_D3D_METHOD(HRESULT,SetIndices,(IDirect3DIndexBuffer9*));
    BF2_D3D_METHOD(HRESULT,GetIndices,(IDirect3DIndexBuffer9**));
    BF2_D3D_METHOD(HRESULT,CreatePixelShader,(const DWORD*,IDirect3DPixelShader9**));
    BF2_D3D_METHOD(HRESULT,SetPixelShader,(IDirect3DPixelShader9*));
    BF2_D3D_METHOD(HRESULT,GetPixelShader,(IDirect3DPixelShader9**));
    BF2_D3D_METHOD(HRESULT,SetPixelShaderConstantF,(UINT,const float*,UINT));
    BF2_D3D_METHOD(HRESULT,GetPixelShaderConstantF,(UINT,float*,UINT));
    BF2_D3D_METHOD(HRESULT,SetPixelShaderConstantI,(UINT,const int*,UINT));
    BF2_D3D_METHOD(HRESULT,GetPixelShaderConstantI,(UINT,int*,UINT));
    BF2_D3D_METHOD(HRESULT,SetPixelShaderConstantB,(UINT,const BOOL*,UINT));
    BF2_D3D_METHOD(HRESULT,GetPixelShaderConstantB,(UINT,BOOL*,UINT));
    BF2_D3D_METHOD(HRESULT,DrawRectPatch,(UINT,const float*,const D3DRECTPATCH_INFO*));
    BF2_D3D_METHOD(HRESULT,DrawTriPatch,(UINT,const float*,const D3DTRIPATCH_INFO*));
    BF2_D3D_METHOD(HRESULT,DeletePatch,(UINT));
    BF2_D3D_METHOD(HRESULT,CreateQuery,(D3DQUERYTYPE,IDirect3DQuery9**));
};
#undef BF2_D3D_METHOD

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion);

static_assert(sizeof(HRESULT)==4 && sizeof(DWORD)==4 && sizeof(BOOL)==4, "D3D scalar widths");
static_assert(sizeof(GUID)==16 && sizeof(RECT)==16, "D3D Win32 value layouts");
static_assert(sizeof(D3DVERTEXELEMENT9)==8 && sizeof(D3DSURFACE_DESC)==32, "D3D resource layouts");
static_assert(sizeof(D3DMATRIX)==64 && sizeof(D3DCAPS9)==304, "D3D matrix and caps layouts");
#endif
