// Shared texture-wrapper layout used by the manager, atlas and render consumers.
// Lifetime and device operations are provided by Texture.cpp.

#ifndef BF2_RENDDX9_TEXTURE_H
#define BF2_RENDDX9_TEXTURE_H

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

namespace dice {
namespace hfe {
namespace dx9rend {

// D3D texture wrapper with separate cache/caller references and a three-slot shared interface.
// Owners explicitly destroy/free on the final release; there is no virtual release slot.
// Creation parameters remain uninitialized until attach and must not be read beforehand.
// File information describes the original image; the attached description reflects mip skipping.
// The interface GUIDs below are local constants so consumers do not require dxguid.lib.
static const GUID kIID_VolumeTexture9 = { 0x2518526C, 0xE789, 0x4111, { 0xA7,0xB9,0x47,0xEF,0x32,0x8D,0x13,0xE6 } };
static const GUID kIID_CubeTexture9   = { 0xFFF32F81, 0xD953, 0x473A, { 0x92,0x23,0x93,0xD6,0x52,0xAB,0xA9,0x3F } };
static const GUID kIID_Texture9       = { 0x85C31227, 0x3DE5, 0x4F00, { 0x9B,0x3A,0xF1,0x1A,0xC3,0x8C,0x18,0xB5 } };
static const GUID kIID_BaseTexture9   = { 0x580CA87E, 0x1D3C, 0x4D54, { 0x99,0x1D,0xB7,0xD3,0xE3,0xC2,0x98,0xCE } };

struct EngineTexture
{
    void*                  vptr;          // +0x00
    int                    refCount;      // +0x04
    IDirect3DBaseTexture9* baseTexture;   // +0x08
    unsigned int           fileInfo[7];   // +0x0C  D3DXIMAGE_INFO: W,H,Depth,Mips,Format,ResType,FileFormat
    unsigned int           width;         // +0x28  the created desc starts here
    unsigned int           height;        // +0x2C
    unsigned int           depth;         // +0x30
    unsigned int           mipLevels;     // +0x34
    unsigned int           format;        // +0x38
    unsigned int           resourceType;  // +0x3C  3 = 2D, 4 = volume, 5 = cube
    unsigned int           face;          // +0x40  0x7FFFFFFF outside a cube-face context
    int                    lockCount;     // +0x44
    int                    mipClassIndex; // +0x48
    int                    pool;          // +0x4C  D3DPOOL - 1 managed, 0 default
    unsigned char          recreateFromStored; // +0x50
    unsigned char          byte51;        // +0x51
    unsigned char          pad52[2];      // +0x52  unwritten in the original
    int                    priority;      // +0x54
    std::string           name;          // +0x58
    unsigned int           totalBytes;    // +0x74
    unsigned char          usedSuffixedFile; // +0x78
    unsigned char          pad79[3];      // +0x79  unwritten in the original
};

// Every allocation site (retail: `push 7Ch` - getTexture 0x1001942C and the create paths) takes
// sizeof(EngineTexture) from the engine heap and constructs it with textureCtor; owners destroy it
// with textureDtor before engineFree.

// The 32-byte lock record textureLock returns and textureUnlock consumes (sub_10014BA0's `new 0x20`):
//     +0x00 w  +0x04 h  +0x08 max(d,1)  +0x0C bpp  +0x10 mip  +0x14 face  +0x18 pBits  +0x1C staging
struct TextureLockRecord
{
    unsigned int       width;      // +0x00
    unsigned int       height;     // +0x04
    unsigned int       depth;      // +0x08
    int                bpp;        // +0x0C
    unsigned int       mip;        // +0x10
    unsigned int       face;       // +0x14  0x7FFFFFFF = not a cube face
    void*              pBits;      // +0x18
    IDirect3DSurface9* staging;    // +0x1C  default-pool path only, else 0
};

// Retail x86 evidence only; textureLock allocates sizeof(TextureLockRecord) (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(TextureLockRecord) == 0x20, "sub_10014BA0 allocates 0x20");
#endif

// Texture.cpp - all bodies read in full from the addresses in their comments.
EngineTexture*     textureCtor(void* storage);
void               textureDtor(EngineTexture* tex);
bool               textureAttach(EngineTexture* tex, IDirect3DBaseTexture9* d3d,
                                 const unsigned int desc[7]);
TextureLockRecord* textureLock(EngineTexture* tex, unsigned int mip,
                               unsigned int face);
bool               textureUnlock(EngineTexture* tex, TextureLockRecord* rec);
void               textureSetFileInfo(EngineTexture* tex, const void* info28);
IDirect3DSurface9* textureGetLevelSurface(EngineTexture* tex, unsigned int mip,
                                          unsigned int face);
// sub_10014940 - `return this + 40`. The dims pair every consumer reads: [0] = W, [1] = H.
inline const unsigned int* textureDims(const EngineTexture* tex) { return &tex->width; }
// The shared vtable's slot +0x08 / explicit release-at-owner pattern, for other TUs.
void               textureAddRef(EngineTexture* tex);
void               textureReleaseRef(EngineTexture*& tex);   // if (!--ref) { dtor; engineFree; }, nulls the slot
int                textureReleaseD3DResource(EngineTexture* tex);  // sub_10014E70 - device-lost, keeps the wrapper

// RendDX9Types.cpp - sub_1000CD30, bits per pixel for a D3DFORMAT (4 for DXT1, 8 for DXT3/5, ...).
int bitsPerPixel(unsigned int d3dFormat);

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_RENDDX9_TEXTURE_H
