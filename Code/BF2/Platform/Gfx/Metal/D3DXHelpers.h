// [CHOSEN, not recovered] CPU contracts for the native D3DX helpers.
#pragma once
#include "../D3DX9Subset.h"
#include <vector>
#include <cstddef>
#include <cstdint>

namespace bf2d3dx {
using Bytes = std::vector<unsigned char>;
struct Color { double r=0,g=0,b=0,a=1; double& operator[](unsigned i) { return i==0?r:i==1?g:i==2?b:a; }
    double operator[](unsigned i) const { return i==0?r:i==1?g:i==2?b:a; } };
struct Image {
    D3DXIMAGE_INFO info={};
    D3DFORMAT sourceFormat=D3DFMT_UNKNOWN; // P8 inputs expand in data, retain their metadata here.
    // DDS order: face, mip, depth slice. Raster images have one subresource.
    std::vector<Bytes> data;
};
[[noreturn]] void stop(const char* name);
std::uint16_t half(float);
float unhalf(std::uint16_t);
unsigned pixelBytes(D3DFORMAT);
unsigned blockSize(D3DFORMAT);
unsigned rowBytes(D3DFORMAT,unsigned width);
std::size_t imageBytes(D3DFORMAT,unsigned width,unsigned height,unsigned depth=1);
Color decode(D3DFORMAT,const unsigned char*);
void encode(D3DFORMAT,unsigned char*,const Color&);
bool convert(const void* src,D3DFORMAT sf,unsigned sw,unsigned sh,unsigned sp,
    void* dst,D3DFORMAT df,unsigned dw,unsigned dh,unsigned dp,DWORD filter,D3DCOLOR key=0);
bool parse(const void*,std::size_t,Image&);
// DDS/TGA/BMP/PNG metadata reads fixed headers only (TextureManager's 124-byte
// probe). JPEG needs the actual complete buffer for its variable-length headers.
bool metadata(const void*,std::size_t,D3DXIMAGE_INFO&);
bool serialize(const Image&,D3DXIMAGE_FILEFORMAT,Bytes&);
struct MeshVertex { D3DXVECTOR3 position,normal; };
struct Geometry { std::vector<MeshVertex> vertices; std::vector<DWORD> indices; };
bool sphereGeometry(float radius,unsigned slices,unsigned stacks,Geometry&);
bool boxGeometry(float width,float height,float depth,Geometry&);
bool cylinderGeometry(float r1,float r2,float length,unsigned slices,unsigned stacks,Geometry&);
// Packed resource access, including staging for non-lockable render targets.
// Linux requires a lockable volume; public D3D9 has no volume readback operation.
HRESULT surfaceBytes(IDirect3DSurface9*,Bytes&,bool upload);
HRESULT textureBytes(IDirect3DBaseTexture9*,unsigned face,unsigned level,Bytes&,bool upload);
}
