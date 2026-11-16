// [CHOSEN, not recovered] Portable D3DX image codecs; Linux port: issue #36.
#if (defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))) && defined(BF2_IMAGE_RENDDX9)
#include "D3DXHelpers.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <zlib.h>
#if defined(__APPLE__)
#include <ImageIO/ImageIO.h>
#include <CoreGraphics/CoreGraphics.h>
#else
#include <cstdio>
#include <cstdlib>
#include <csetjmp>
#include <memory>
#include <jpeglib.h>
#include <png.h>
#endif
namespace bf2d3dx {
static unsigned u16(const unsigned char* p) { return p[0]|(unsigned(p[1])<<8); }
static unsigned u32(const unsigned char* p) { return u16(p)|(u16(p+2)<<16); }
static unsigned be32(const unsigned char* p) { return (unsigned(p[0])<<24)|(unsigned(p[1])<<16)|(unsigned(p[2])<<8)|p[3]; }
static void set32(Bytes& b,unsigned offset,unsigned v) { for(unsigned i=0;i<4;++i) b[offset+i]=v>>(i*8); }
struct PixelFormat { D3DFORMAT format; unsigned flags,bits,r,g,b,a; };
static const PixelFormat formats[]={
    {D3DFMT_R8G8B8,0x40,24,0xff0000,0xff00,0xff,0},
    {D3DFMT_A8R8G8B8,0x41,32,0xff0000,0xff00,0xff,0xff000000},
    {D3DFMT_X8R8G8B8,0x40,32,0xff0000,0xff00,0xff,0},
    {D3DFMT_A8B8G8R8,0x41,32,0xff,0xff00,0xff0000,0xff000000},
    {D3DFMT_X8B8G8R8,0x40,32,0xff,0xff00,0xff0000,0},
    {D3DFMT_R5G6B5,0x40,16,0xf800,0x7e0,0x1f,0},
    {D3DFMT_A1R5G5B5,0x41,16,0x7c00,0x3e0,0x1f,0x8000},
    {D3DFMT_X1R5G5B5,0x40,16,0x7c00,0x3e0,0x1f,0},
    {D3DFMT_A4R4G4B4,0x41,16,0xf00,0xf0,0xf,0xf000},
    {D3DFMT_X4R4G4B4,0x40,16,0xf00,0xf0,0xf,0},
    {D3DFMT_A2R10G10B10,0x41,32,0x3ff00000,0xffc00,0x3ff,0xc0000000},
    {D3DFMT_A2B10G10R10,0x41,32,0x3ff,0xffc00,0x3ff00000,0xc0000000},
    {D3DFMT_G16R16,0x40,32,0xffff,0xffff0000,0,0},
    {D3DFMT_L8,0x20000,8,0xff,0,0,0},
    {D3DFMT_L16,0x20000,16,0xffff,0,0,0},
    {D3DFMT_A8L8,0x20001,16,0xff,0,0,0xff00},
    {D3DFMT_A8,2,8,0,0,0,0xff}
};
static bool dimensions(unsigned w,unsigned h,unsigned d=1) {
    return w && h && d && w<=16384 && h<=16384 && d<=2048;
}
static unsigned mipCount(unsigned w,unsigned h,unsigned d) { unsigned n=1; while((w|h|d)>1) { w>>=1; h>>=1; d>>=1; ++n; } return n; }
static bool completeJPEG(const unsigned char* p,std::size_t size) {
    if(size<4 || p[0]!=0xff || p[1]!=0xd8) return false;
    std::size_t at=2; bool scan=false;
    while(at<size) {
        if(scan) { while(at<size && p[at]!=0xff) ++at; }
        if(at==size || p[at++]!=0xff) return false;
        while(at<size && p[at]==0xff) ++at;
        if(at==size) return false;
        const unsigned marker=p[at++];
        if(scan && (!marker || (marker>=0xd0 && marker<=0xd7))) continue;
        if(marker==0xd9) return true; // Trailing application data is permitted.
        if(marker==0xd8 || !marker || size-at<2) return false;
        const unsigned length=unsigned(p[at])*256+p[at+1];
        if(length<2 || length>size-at) return false;
        at+=length; scan=marker==0xda;
    }
    return false;
}
#if defined(__APPLE__)
static CGImageSourceRef jpegSource(const unsigned char* p,std::size_t size) {
    if(!completeJPEG(p,size) || size>std::size_t(std::numeric_limits<CFIndex>::max())) return nullptr;
    CFDataRef data=CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,p,CFIndex(size),kCFAllocatorNull);
    if(!data) return nullptr;
    CGImageSourceRef source=CGImageSourceCreateWithData(data,nullptr); CFRelease(data);
    if(source && (!CGImageSourceGetType(source) || !CFEqual(CGImageSourceGetType(source),CFSTR("public.jpeg")) ||
                  CGImageSourceGetStatus(source)!=kCGImageStatusComplete)) { CFRelease(source); source=nullptr; }
    return source;
}
static bool jpegInfo(CGImageSourceRef source,D3DXIMAGE_INFO& info) {
    if(!source) return false;
    CFDictionaryRef properties=CGImageSourceCopyPropertiesAtIndex(source,0,nullptr);
    if(!properties) return false;
    int w=0,h=0;
    auto width=static_cast<CFNumberRef>(CFDictionaryGetValue(properties,kCGImagePropertyPixelWidth));
    auto height=static_cast<CFNumberRef>(CFDictionaryGetValue(properties,kCGImagePropertyPixelHeight));
    const bool ok=width && height && CFNumberGetValue(width,kCFNumberIntType,&w) &&
        CFNumberGetValue(height,kCFNumberIntType,&h) && w>0 && h>0 && dimensions(w,h);
    const auto model=CFDictionaryGetValue(properties,kCGImagePropertyColorModel);
    const bool gray=model && CFEqual(model,kCGImagePropertyColorModelGray);
    CFRelease(properties);
    if(ok) info={unsigned(w),unsigned(h),1,1,gray?D3DFMT_L8:D3DFMT_X8R8G8B8,D3DRTYPE_TEXTURE,D3DXIFF_JPG};
    return ok;
}
static bool jpeg(const unsigned char* p,std::size_t size,Image& out) {
    CGImageSourceRef source=jpegSource(p,size);
    if(!source) return false;
    if(!jpegInfo(source,out.info)) { CFRelease(source); return false; }
    CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,nullptr);
    const bool complete=CGImageSourceGetStatusAtIndex(source,0)==kCGImageStatusComplete;
    CFRelease(source);
    if(!image) return false;
    const unsigned w=out.info.Width,h=out.info.Height;
    if(!complete || CGImageGetWidth(image)!=w || CGImageGetHeight(image)!=h) { CGImageRelease(image); return false; }
    Bytes pixels(std::size_t(w)*h*4);
    // Device RGB keeps stored JPEG colour values; do not apply EXIF orientation or
    // an embedded display profile. D3DX loads texture rows in file order, not photo orientation.
    CGColorSpaceRef rgb=CGColorSpaceCreateDeviceRGB();
    const auto model=CGColorSpaceGetModel(CGImageGetColorSpace(image));
    CGColorSpaceRef plain=model==kCGColorSpaceModelMonochrome?CGColorSpaceCreateDeviceGray():
        model==kCGColorSpaceModelCMYK?CGColorSpaceCreateDeviceCMYK():CGColorSpaceCreateDeviceRGB();
    CGImageRef untagged=CGImageCreateCopyWithColorSpace(image,plain); CGColorSpaceRelease(plain);
    CGContextRef context=CGBitmapContextCreate(pixels.data(),w,h,8,w*4,rgb,
        kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst);
    CGColorSpaceRelease(rgb);
    if(!context || !untagged) { if(context) CGContextRelease(context); if(untagged) CGImageRelease(untagged); CGImageRelease(image); return false; }
    CGContextSetBlendMode(context,kCGBlendModeCopy);
    CGContextSetInterpolationQuality(context,kCGInterpolationNone);
    CGContextDrawImage(context,CGRectMake(0,0,w,h),untagged);
    CGContextRelease(context); CGImageRelease(untagged); CGImageRelease(image);
    for(std::size_t i=3;i<pixels.size();i+=4) pixels[i]=255;
    if(out.info.Format==D3DFMT_L8) {
        Bytes luminance(std::size_t(w)*h);
        for(std::size_t i=0;i<luminance.size();++i) luminance[i]=pixels[i*4+2];
        pixels=std::move(luminance);
    }
    out.data.push_back(std::move(pixels)); return true;
}
#else
struct JPEGError {
    jpeg_error_mgr base;
    std::jmp_buf jump;
    bool warning=false;
};
static void jpegFailure(j_common_ptr codec) {
    std::longjmp(reinterpret_cast<JPEGError*>(codec->err)->jump,1);
}
static void jpegMessage(j_common_ptr codec,int level) {
    if(level<0) reinterpret_cast<JPEGError*>(codec->err)->warning=true;
}
struct JPEGReader {
    jpeg_decompress_struct codec={};
    JPEGError error={};
    Bytes row;
    ~JPEGReader() { if(codec.mem) jpeg_destroy_decompress(&codec); }
};
// Codec state lives on the heap so libjpeg's longjmp cannot invalidate changed
// automatic variables. Owning C++ objects exist before the setjmp boundary.
static bool readJPEG(const unsigned char* p,std::size_t size,D3DXIMAGE_INFO& info,Bytes* pixels) {
    if(!completeJPEG(p,size) || size>std::numeric_limits<unsigned long>::max()) return false;
    std::unique_ptr<JPEGReader> state(new JPEGReader);
    auto& row=state->row;
    auto& c=state->codec;
    c.err=jpeg_std_error(&state->error.base);
    state->error.base.error_exit=jpegFailure; state->error.base.emit_message=jpegMessage;
    if(setjmp(state->error.jump)) return false;
    jpeg_create_decompress(&c);
    jpeg_mem_src(&c,p,static_cast<unsigned long>(size));
    if(jpeg_read_header(&c,TRUE)!=JPEG_HEADER_OK || !dimensions(c.image_width,c.image_height)) return false;
    const bool gray=c.jpeg_color_space==JCS_GRAYSCALE;
    const bool cmyk=c.jpeg_color_space==JCS_CMYK || c.jpeg_color_space==JCS_YCCK;
    info={c.image_width,c.image_height,1,1,gray?D3DFMT_L8:D3DFMT_X8R8G8B8,D3DRTYPE_TEXTURE,D3DXIFF_JPG};
    if(!pixels) return !state->error.warning;
    c.out_color_space=gray?JCS_GRAYSCALE:cmyk?JCS_CMYK:JCS_RGB;
    if(!jpeg_start_decompress(&c)) return false;
    row.resize(std::size_t(c.output_width)*c.output_components);
    pixels->resize(std::size_t(info.Width)*info.Height*(gray?1:4));
    while(c.output_scanline<c.output_height) {
        const unsigned y=c.output_scanline; JSAMPROW next=row.data();
        if(jpeg_read_scanlines(&c,&next,1)!=1) return false;
        auto* dst=pixels->data()+std::size_t(y)*info.Width*(gray?1:4);
        if(gray) std::memcpy(dst,row.data(),info.Width);
        else for(unsigned x=0;x<info.Width;++x) {
            const auto* src=row.data()+x*c.output_components;
            for(unsigned channel=0;channel<3;++channel) {
                unsigned value=src[2-channel];
                if(cmyk) value=c.saw_Adobe_marker?(value*src[3]+127)/255:((255-value)*(255-src[3])+127)/255;
                dst[x*4+channel]=static_cast<unsigned char>(value);
            }
            dst[x*4+3]=255;
        }
    }
    return jpeg_finish_decompress(&c) && !state->error.warning;
}
static bool jpeg(const unsigned char* p,std::size_t size,Image& out) {
    Bytes pixels;
    if(!readJPEG(p,size,out.info,&pixels)) return false;
    out.data.push_back(std::move(pixels)); return true;
}
#endif
bool metadata(const void* bytes,std::size_t size,D3DXIMAGE_INFO& info) {
    if(!bytes || size<18) return false;
    const auto* p=static_cast<const unsigned char*>(bytes); D3DXIMAGE_INFO i={};
    i.Depth=i.MipLevels=1; i.ResourceType=D3DRTYPE_TEXTURE;
    if(p[0]==0xff && p[1]==0xd8) {
#if defined(__APPLE__)
        CGImageSourceRef source=jpegSource(p,size); const bool ok=jpegInfo(source,i);
        if(source) CFRelease(source); if(!ok) return false;
#else
        if(!readJPEG(p,size,i,nullptr)) return false;
#endif
    } else if(size>=124 && !std::memcmp(p,"DDS ",4)) {
        if(u32(p+4)!=124 || u32(p+76)!=32) return false;
        i.Width=u32(p+16); i.Height=u32(p+12); i.MipLevels=std::max(1u,u32(p+28)); i.ImageFileFormat=D3DXIFF_DDS;
        unsigned caps=u32(p+112),flags=u32(p+80);
        if(caps&0x200000) { i.Depth=u32(p+24); i.ResourceType=D3DRTYPE_VOLUMETEXTURE; }
        if(caps&0x200) { if((caps&0xfc00)!=0xfc00 || (caps&0x200000) || i.Width!=i.Height) return false; i.ResourceType=D3DRTYPE_CUBETEXTURE; }
        if((flags&0x20) && (u32(p+88)==8 || u32(p+88)==16)) i.Format=u32(p+88)==8?D3DFMT_P8:D3DFMT_A8P8;
        else if(flags&4) { auto f=static_cast<D3DFORMAT>(u32(p+84)); if(pixelBytes(f) && (blockSize(f)>1 || f==D3DFMT_A16B16G16R16 || f>=D3DFMT_R16F)) i.Format=f; }
        else for(const auto& f:formats) if((flags&0x20043)==f.flags && u32(p+88)==f.bits && u32(p+92)==f.r && u32(p+96)==f.g && u32(p+100)==f.b && u32(p+104)==f.a) { i.Format=f.format; break; }
    } else if(size>=33 && !std::memcmp(p,"\x89PNG\r\n\x1a\n",8)) {
        if(be32(p+8)!=13 || std::memcmp(p+12,"IHDR",4)) return false;
        i.Width=be32(p+16); i.Height=be32(p+20); i.Format=D3DFMT_A8R8G8B8; i.ImageFileFormat=D3DXIFF_PNG;
    } else if(size>=54 && p[0]=='B' && p[1]=='M') {
        i.Width=u32(p+18); unsigned h=u32(p+22); i.Height=h&0x80000000u?0u-h:h; i.Format=D3DFMT_X8R8G8B8; i.ImageFileFormat=D3DXIFF_BMP;
    } else {
        unsigned type=p[2],bits=p[16],b=type==1 || type==9?p[7]:bits;
        if(type!=1 && type!=2 && type!=3 && type!=9 && type!=10 && type!=11) return false;
        i.Width=u16(p+12); i.Height=u16(p+14); i.ImageFileFormat=D3DXIFF_TGA;
        if(type==3 || type==11) i.Format=bits==8?D3DFMT_L8:bits==16?D3DFMT_A8L8:D3DFMT_UNKNOWN;
        else i.Format=b==32?D3DFMT_A8R8G8B8:b==24?D3DFMT_R8G8B8:b==16?((type==2 || type==10) && !(p[17]&15)?D3DFMT_X1R5G5B5:D3DFMT_A1R5G5B5):D3DFMT_UNKNOWN;
    }
    if(!dimensions(i.Width,i.Height,i.Depth) || i.Format==D3DFMT_UNKNOWN || i.MipLevels>mipCount(i.Width,i.Height,i.Depth)) return false;
    info=i; return true;
}
static bool dds(const unsigned char* p,std::size_t size,Image& out) {
    if(size<128 || u32(p+4)!=124 || u32(p+76)!=32) return false;
    auto& i=out.info; i.Width=u32(p+16); i.Height=u32(p+12); i.Depth=1; i.MipLevels=std::max(1u,u32(p+28)); i.ImageFileFormat=D3DXIFF_DDS; i.ResourceType=D3DRTYPE_TEXTURE;
    const unsigned caps=u32(p+112);
    if(caps&0x200000) { i.ResourceType=D3DRTYPE_VOLUMETEXTURE; i.Depth=u32(p+24); }
    if(caps&0x200) { if((caps&0xfc00)!=0xfc00 || (caps&0x200000) || i.Width!=i.Height) return false; i.ResourceType=D3DRTYPE_CUBETEXTURE; }
    if(!dimensions(i.Width,i.Height,i.Depth) || i.MipLevels>mipCount(i.Width,i.Height,i.Depth)) return false;
    const unsigned flags=u32(p+80);
    const bool indexed=(flags&0x20)!=0; const unsigned indexBytes=u32(p+88)/8;
    if(indexed && indexBytes!=1 && indexBytes!=2) return false;
    i.Format=D3DFMT_UNKNOWN;
    if(indexed) i.Format=D3DFMT_A8R8G8B8;
    else if(flags&4) { const auto f=static_cast<D3DFORMAT>(u32(p+84)); if(pixelBytes(f) && (blockSize(f)>1 || f==D3DFMT_A16B16G16R16 || f>=D3DFMT_R16F)) i.Format=f; }
    else for(const auto& f:formats) if((flags&0x20043)==f.flags && u32(p+88)==f.bits && u32(p+92)==f.r && u32(p+96)==f.g && u32(p+100)==f.b && u32(p+104)==f.a) { i.Format=f.format; break; }
    if(i.Format==D3DFMT_UNKNOWN) return false;
    std::size_t offset=128;
    // Legacy DDS palette entries are RGBA bytes (PALETTEENTRY), not D3DCOLOR.
    // Expand to a G3b color format instead of publishing a P8 Metal resource.
    const unsigned char* palette=nullptr;
    if(indexed) { if(size-offset<1024) return false; palette=p+offset; offset+=1024; }
    const unsigned faces=i.ResourceType==D3DRTYPE_CUBETEXTURE?6:1;
    // DDS uncompressed rows can be tight or DWORD-aligned. Honor the explicit
    // top pitch and infer DWORD alignment for lower levels only when attested.
    const unsigned topRow=indexed?i.Width*indexBytes:rowBytes(i.Format,i.Width),pitch=u32(p+20);
    const bool pitched=blockSize(i.Format)==1 && (u32(p+8)&8);
    if(pitched && pitch<topRow) return false;
    const bool aligned=pitched && pitch!=topRow && pitch==((topRow+3)&~3u);
    for(unsigned face=0;face<faces;++face) for(unsigned l=0;l<i.MipLevels;++l) {
        const unsigned w=std::max(1u,i.Width>>l),h=std::max(1u,i.Height>>l),d=std::max(1u,i.Depth>>l),row=rowBytes(i.Format,w),rows=(h+blockSize(i.Format)-1)/blockSize(i.Format);
        const unsigned packedRow=indexed?w*indexBytes:row;
        const unsigned fileRow=pitched && l==0?pitch:aligned?((packedRow+3)&~3u):packedRow;
        const std::size_t n=std::size_t(fileRow)*rows*d;
        if(n>size-offset) return false;
        Bytes bytes(std::size_t(row)*rows*d);
        for(std::size_t y=0;y<std::size_t(rows)*d;++y) {
            if(!indexed) std::memcpy(bytes.data()+y*row,p+offset+y*fileRow,row);
            else for(unsigned x=0;x<w;++x) {
                const auto* index=p+offset+y*fileRow+x*indexBytes; const auto* color=palette+index[0]*4; auto* dest=bytes.data()+y*row+x*4;
                dest[0]=color[2]; dest[1]=color[1]; dest[2]=color[0]; dest[3]=indexBytes==2?index[1]:color[3];
            }
        }
        out.data.push_back(std::move(bytes)); offset+=n;
    }
    return true;
}
static bool tga(const unsigned char* p,std::size_t size,Image& out) {
    if(size<18) return false;
    const unsigned type=p[2],bits=p[16],w=u16(p+12),h=u16(p+14),entryBits=p[7],entryBytes=(entryBits+7)/8;
    const bool indexed=type==1 || type==9,gray=type==3 || type==11,rle=type>=9;
    if(!dimensions(w,h) || !(type==1 || type==2 || type==3 || type==9 || type==10 || type==11) || (p[17]&0xc0)) return false;
    if((indexed && (p[1]!=1 || bits!=8 || (entryBits!=16 && entryBits!=24 && entryBits!=32))) ||
       (!indexed && p[1]) || (gray && bits!=8 && bits!=16) || (!gray && !indexed && bits!=16 && bits!=24 && bits!=32)) return false;
    std::size_t offset=18u+p[0]; const unsigned paletteStart=u16(p+3),paletteCount=u16(p+5),bpp=bits/8;
    if(offset>size || std::size_t(paletteCount)*entryBytes>size-offset) return false;
    const auto* palette=p+offset; offset+=std::size_t(paletteCount)*entryBytes;
    const D3DFORMAT fmt=gray?(bits==8?D3DFMT_L8:D3DFMT_A8L8):indexed?(entryBits==32?D3DFMT_A8R8G8B8:entryBits==24?D3DFMT_R8G8B8:D3DFMT_A1R5G5B5):bits==32?D3DFMT_A8R8G8B8:bits==24?D3DFMT_R8G8B8:(p[17]&15)?D3DFMT_A1R5G5B5:D3DFMT_X1R5G5B5;
    const unsigned outBytes=pixelBytes(fmt); Bytes data(imageBytes(fmt,w,h));
    std::size_t n=0,total=std::size_t(w)*h;
    auto emit=[&](const unsigned char* pixel) {
        unsigned x=n%w,y=n/w; if(p[17]&16) x=w-1-x; if(!(p[17]&32)) y=h-1-y;
        if(indexed) { const unsigned ix=pixel[0]; if(ix<paletteStart || ix-paletteStart>=paletteCount) return false; pixel=palette+(ix-paletteStart)*entryBytes; }
        std::memcpy(data.data()+(std::size_t(y)*w+x)*outBytes,pixel,outBytes); ++n; return true;
    };
    while(n<total) {
        unsigned count=1; bool repeat=false;
        if(rle) { if(offset==size) return false; unsigned header=p[offset++]; count=(header&127)+1; repeat=header&128; }
        if(count>total-n || std::size_t(repeat?1:count)*bpp>size-offset) return false;
        for(unsigned j=0;j<count;++j) if(!emit(p+offset+(repeat?0:j*bpp))) return false;
        offset+=std::size_t(repeat?1:count)*bpp;
    }
    out.info={w,h,1,1,fmt,D3DRTYPE_TEXTURE,D3DXIFF_TGA}; out.data.push_back(std::move(data)); return true;
}
static bool bmp(const unsigned char* p,std::size_t size,Image& out) {
    if(size<54 || u32(p+14)<40 || u32(p+14)>size-14 || u16(p+26)!=1 || u32(p+30)!=0) return false;
    const unsigned w=u32(p+18),rawh=u32(p+22),h=rawh&0x80000000u?0u-rawh:rawh,bits=u16(p+28),offset=u32(p+10);
    if(!dimensions(w,h) || (bits!=24 && bits!=32 && bits!=8)) return false;
    const std::size_t row=((std::size_t(w)*bits+31)/32)*4;
    if(offset>size || row*h>size-offset) return false;
    const unsigned count=u32(p+46)?u32(p+46):256,paletteOffset=14+u32(p+14);
    if(bits==8 && (count>256 || std::size_t(paletteOffset)+count*4>offset)) return false;
    Bytes data(std::size_t(w)*h*4);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const auto* pixel=p+offset+row*((rawh&0x80000000u)?y:h-y-1)+x*(bits/8);
        if(bits==8) { if(*pixel>=count) return false; pixel=p+paletteOffset+*pixel*4; }
        auto* dst=data.data()+(std::size_t(y)*w+x)*4; std::memcpy(dst,pixel,3); dst[3]=255;
    }
    out.info={w,h,1,1,D3DFMT_X8R8G8B8,D3DRTYPE_TEXTURE,D3DXIFF_BMP}; out.data.push_back(std::move(data)); return true;
}
static bool png(const unsigned char* p,std::size_t size,Image& out) {
    if(size<33 || be32(p+8)!=13 || std::memcmp(p+12,"IHDR",4)) return false;
    const unsigned w=be32(p+16),h=be32(p+20),depth=p[24],type=p[25];
    const unsigned channels=type==0 || type==3?1:type==2?3:type==4?2:type==6?4:0;
    if(!dimensions(w,h) || !channels || p[26] || p[27] || p[28] || (depth!=8 && depth!=16 && depth!=1 && depth!=2 && depth!=4) ||
        (depth<8 && type!=0 && type!=3) || (type==3 && depth==16)) return false;
    Bytes compressed,palette,trans; bool end=false; std::size_t offset=8;
    while(offset+12<=size) {
        const unsigned n=be32(p+offset); if(n>size-offset-12) return false;
        const auto* name=p+offset+4; const auto* data=name+4;
        if(crc32(0,name,n+4)!=be32(data+n)) return false;
        if(!std::memcmp(name,"IDAT",4)) compressed.insert(compressed.end(),data,data+n);
        else if(!std::memcmp(name,"PLTE",4)) palette.assign(data,data+n);
        else if(!std::memcmp(name,"tRNS",4)) trans.assign(data,data+n);
        else if(!std::memcmp(name,"IEND",4)) { if(n) return false; end=true; break; }
        else if(std::memcmp(name,"IHDR",4) && !(name[0]&32)) return false;
        offset+=n+12;
    }
    if(!end || compressed.empty() || (type==3 && (palette.empty() || palette.size()%3 || palette.size()>768))) return false;
    const std::size_t row=(std::size_t(w)*channels*depth+7)/8,bpp=std::max(1u,(channels*depth+7)/8);
    Bytes raw((row+1)*h); uLongf length=raw.size();
    if(uncompress(raw.data(),&length,compressed.data(),compressed.size())!=Z_OK || length!=raw.size()) return false;
    Bytes scan(row*h),data(std::size_t(w)*h*4);
    auto paeth=[](int a,int b,int c) { int q=a+b-c,pa=std::abs(q-a),pb=std::abs(q-b),pc=std::abs(q-c); return pa<=pb && pa<=pc?a:pb<=pc?b:c; };
    for(unsigned y=0;y<h;++y) {
        const unsigned filter=raw[y*(row+1)]; if(filter>4) return false;
        for(std::size_t x=0;x<row;++x) { int a=x>=bpp?scan[y*row+x-bpp]:0,b=y?scan[(y-1)*row+x]:0,c=y && x>=bpp?scan[(y-1)*row+x-bpp]:0;
            scan[y*row+x]=raw[y*(row+1)+x+1]+(filter==1?a:filter==2?b:filter==3?(a+b)/2:filter==4?paeth(a,b,c):0); }
        for(unsigned x=0;x<w;++x) {
            auto sample=[&](unsigned k) { unsigned bit=(x*channels+k)*depth; const auto* s=scan.data()+y*row+bit/8;
                return depth==16?unsigned(s[0])*256+s[1]:depth==8?unsigned(*s):unsigned((*s>>(8-depth-bit%8))&((1<<depth)-1)); };
            Color c; const double max=depth==16?65535:(1u<<depth)-1;
            if(type==3) { unsigned ix=sample(0); if(ix>=palette.size()/3) return false; c={palette[ix*3]/255.,palette[ix*3+1]/255.,palette[ix*3+2]/255.,ix<trans.size()?trans[ix]/255.:1}; }
            else if(type==0 || type==4) { c.r=c.g=c.b=sample(0)/max; if(type==4) c.a=sample(1)/max; else if(trans.size()==2 && sample(0)==(unsigned(trans[0])*256+trans[1])) c.a=0; }
            else { c.r=sample(0)/max; c.g=sample(1)/max; c.b=sample(2)/max; if(type==6) c.a=sample(3)/max;
                else if(trans.size()==6 && sample(0)==unsigned(trans[0])*256+trans[1] && sample(1)==unsigned(trans[2])*256+trans[3] && sample(2)==unsigned(trans[4])*256+trans[5]) c.a=0; }
            encode(D3DFMT_A8R8G8B8,data.data()+(std::size_t(y)*w+x)*4,c);
        }
    }
    out.info={w,h,1,1,D3DFMT_A8R8G8B8,D3DRTYPE_TEXTURE,D3DXIFF_PNG}; out.data.push_back(std::move(data)); return true;
}
bool parse(const void* bytes,std::size_t size,Image& out) {
    if(!bytes) return false; Image temp; const auto* p=static_cast<const unsigned char*>(bytes); bool ok;
    if(size>=4 && !std::memcmp(p,"DDS ",4)) ok=dds(p,size,temp);
    else if(size>=8 && !std::memcmp(p,"\x89PNG\r\n\x1a\n",8)) ok=png(p,size,temp);
    else if(size>=2 && p[0]=='B' && p[1]=='M') ok=bmp(p,size,temp);
    else if(size>=2 && p[0]==0xff && p[1]==0xd8) ok=jpeg(p,size,temp);
    else ok=tga(p,size,temp);
    if(ok) { D3DXIMAGE_INFO source; temp.sourceFormat=metadata(bytes,size,source)?source.Format:temp.info.Format; out=std::move(temp); } return ok;
}
// [CHOSEN, not recovered] PNG and JPEG output via system codecs, 8 bits per channel, untagged
// device RGB (the stored values, no colour-profile conversion). A source format with alpha bits
// keeps them in the PNG; JPEG has no alpha. D3DX9's own encoder settings (JPEG quality, PNG
// chunks) are not reproduced: only the file format is the contract.
#if !defined(__APPLE__)
struct JPEGWriter {
    jpeg_compress_struct codec={};
    JPEGError error={};
    unsigned char* bytes=nullptr;
    unsigned long size=0;
    ~JPEGWriter() { if(codec.mem) jpeg_destroy_compress(&codec); std::free(bytes); }
};
static bool encodeLinuxRaster(const Bytes& rgba,unsigned width,unsigned height,bool alpha,D3DXIMAGE_FILEFORMAT format,Bytes& out) {
    Bytes rgb;
    const Bytes* source=&rgba;
    if(!alpha) {
        rgb.resize(std::size_t(width)*height*3);
        for(std::size_t i=0;i<std::size_t(width)*height;++i) std::memcpy(rgb.data()+i*3,rgba.data()+i*4,3);
        source=&rgb;
    }
    if(format==D3DXIFF_PNG) {
        png_image image={}; image.version=PNG_IMAGE_VERSION;
        image.width=width; image.height=height; image.format=alpha?PNG_FORMAT_RGBA:PNG_FORMAT_RGB;
        png_alloc_size_t size=0;
        if(!png_image_write_to_memory(&image,nullptr,&size,0,source->data(),0,nullptr)) { png_image_free(&image); return false; }
        out.resize(size);
        const bool ok=png_image_write_to_memory(&image,out.data(),&size,0,source->data(),0,nullptr)!=0;
        png_image_free(&image); if(ok) out.resize(size); return ok;
    }
    std::unique_ptr<JPEGWriter> state(new JPEGWriter);
    auto& c=state->codec;
    c.err=jpeg_std_error(&state->error.base);
    state->error.base.error_exit=jpegFailure; state->error.base.emit_message=jpegMessage;
    if(setjmp(state->error.jump)) return false;
    jpeg_create_compress(&c); jpeg_mem_dest(&c,&state->bytes,&state->size);
    c.image_width=width; c.image_height=height; c.input_components=3; c.in_color_space=JCS_RGB;
    jpeg_set_defaults(&c); jpeg_set_quality(&c,90,TRUE); jpeg_start_compress(&c,TRUE);
    while(c.next_scanline<c.image_height) {
        JSAMPROW row=const_cast<JSAMPROW>(source->data()+std::size_t(c.next_scanline)*width*3);
        if(jpeg_write_scanlines(&c,&row,1)!=1) return false;
    }
    jpeg_finish_compress(&c);
    if(state->error.warning) return false;
    out.assign(state->bytes,state->bytes+state->size); return true;
}
#endif
static bool encodeRaster(const Image& im,D3DXIMAGE_FILEFORMAT format,Bytes& out) {
    const auto& i=im.info;
    if(i.ResourceType!=D3DRTYPE_TEXTURE || i.Depth!=1 || im.data.empty() ||
        im.data[0].size()!=imageBytes(i.Format,i.Width,i.Height)) return false;
    D3DFORMAT alphaProbe=i.Format; bool alpha=false;
    for(const auto& f:formats) if(f.format==alphaProbe) { alpha=f.a!=0; break; }
    if(alphaProbe==D3DFMT_A16B16G16R16 || alphaProbe==D3DFMT_A16B16G16R16F || alphaProbe==D3DFMT_A32B32G32R32F ||
        alphaProbe==D3DFMT_DXT3 || alphaProbe==D3DFMT_DXT5) alpha=true;
    alpha=alpha && format==D3DXIFF_PNG;
    Bytes rgba(std::size_t(i.Width)*i.Height*4);
    if(!convert(im.data[0].data(),i.Format,i.Width,i.Height,rowBytes(i.Format,i.Width),rgba.data(),D3DFMT_A8B8G8R8,
        i.Width,i.Height,i.Width*4,D3DX_FILTER_NONE)) return false;
#if defined(__APPLE__)
    CGColorSpaceRef rgb=CGColorSpaceCreateDeviceRGB();
    CFDataRef pixels=CFDataCreate(kCFAllocatorDefault,rgba.data(),CFIndex(rgba.size()));
    CGDataProviderRef provider=pixels?CGDataProviderCreateWithCFData(pixels):nullptr;
    CGImageRef image=provider&&rgb?CGImageCreate(i.Width,i.Height,8,32,i.Width*4,rgb,
        kCGBitmapByteOrderDefault|(alpha?kCGImageAlphaLast:kCGImageAlphaNoneSkipLast),provider,nullptr,false,
        kCGRenderingIntentDefault):nullptr;
    if(provider) CGDataProviderRelease(provider); if(pixels) CFRelease(pixels); if(rgb) CGColorSpaceRelease(rgb);
    if(!image) return false;
    CFMutableDataRef data=CFDataCreateMutable(kCFAllocatorDefault,0);
    CGImageDestinationRef destination=data?CGImageDestinationCreateWithData(data,
        format==D3DXIFF_PNG?CFSTR("public.png"):CFSTR("public.jpeg"),1,nullptr):nullptr;
    bool ok=false;
    if(destination) {
        CGImageDestinationAddImage(destination,image,nullptr);
        ok=CGImageDestinationFinalize(destination);
        CFRelease(destination);
    }
    CGImageRelease(image);
    if(ok) { const auto* p=CFDataGetBytePtr(data); out.assign(p,p+CFDataGetLength(data)); }
    if(data) CFRelease(data);
    return ok;
#else
    return encodeLinuxRaster(rgba,i.Width,i.Height,alpha,format,out);
#endif
}
bool serialize(const Image& im,D3DXIMAGE_FILEFORMAT format,Bytes& out) {
    const auto& i=im.info;
    if(!dimensions(i.Width,i.Height,i.Depth) || !i.MipLevels || i.MipLevels>mipCount(i.Width,i.Height,i.Depth) || !pixelBytes(i.Format)) return false;
    if(i.ResourceType!=D3DRTYPE_TEXTURE && i.ResourceType!=D3DRTYPE_CUBETEXTURE && i.ResourceType!=D3DRTYPE_VOLUMETEXTURE) return false;
    if(i.ResourceType!=D3DRTYPE_VOLUMETEXTURE && i.Depth!=1) return false;
    if(i.ResourceType==D3DRTYPE_CUBETEXTURE && i.Width!=i.Height) return false;
    const unsigned faces=i.ResourceType==D3DRTYPE_CUBETEXTURE?6:1;
    if(im.data.size()!=faces*i.MipLevels) return false;
    if(format==D3DXIFF_TGA) {
        if(i.Width>65535 || i.Height>65535 || i.Depth!=1 || faces!=1 || im.data[0].size()!=imageBytes(i.Format,i.Width,i.Height)) return false;
        out.assign(18+std::size_t(i.Width)*i.Height*4,0); out[2]=2; out[12]=i.Width; out[13]=i.Width>>8; out[14]=i.Height; out[15]=i.Height>>8; out[16]=32; out[17]=0x28;
        return convert(im.data[0].data(),i.Format,i.Width,i.Height,rowBytes(i.Format,i.Width),out.data()+18,D3DFMT_A8R8G8B8,i.Width,i.Height,i.Width*4,D3DX_FILTER_NONE);
    }
    if(format==D3DXIFF_PNG || format==D3DXIFF_JPG) return faces==1 && i.MipLevels>=1 && encodeRaster(im,format,out);
    if(format!=D3DXIFF_DDS) stop("bf2d3dx::serialize unsupported output format");
    out.assign(128,0); std::memcpy(out.data(),"DDS ",4); set32(out,4,124);
    set32(out,8,0x1007|(blockSize(i.Format)>1?0x80000:8)|(i.MipLevels>1?0x20000:0)|(i.Depth>1?0x800000:0));
    set32(out,12,i.Height); set32(out,16,i.Width); set32(out,20,blockSize(i.Format)>1?unsigned(imageBytes(i.Format,i.Width,i.Height)):rowBytes(i.Format,i.Width));
    set32(out,24,i.ResourceType==D3DRTYPE_VOLUMETEXTURE?i.Depth:0); set32(out,28,i.MipLevels); set32(out,76,32);
    bool found=false;
    for(const auto& f:formats) if(f.format==i.Format) { set32(out,80,f.flags); set32(out,88,f.bits); set32(out,92,f.r); set32(out,96,f.g); set32(out,100,f.b); set32(out,104,f.a); found=true; break; }
    if(!found) { set32(out,80,4); set32(out,84,i.Format); }
    set32(out,108,0x1000|(i.MipLevels>1?0x400008:0)|((faces==6 || i.Depth>1)?8:0)); set32(out,112,faces==6?0xfe00:i.ResourceType==D3DRTYPE_VOLUMETEXTURE?0x200000:0);
    for(unsigned face=0;face<faces;++face) for(unsigned l=0;l<i.MipLevels;++l) {
        const auto& bytes=im.data[face*i.MipLevels+l];
        if(bytes.size()!=imageBytes(i.Format,std::max(1u,i.Width>>l),std::max(1u,i.Height>>l),std::max(1u,i.Depth>>l))) return false;
        out.insert(out.end(),bytes.begin(),bytes.end());
    }
    return true;
}
}
HRESULT WINAPI D3DXGetImageInfoFromFileInMemory(LPCVOID p,UINT n,D3DXIMAGE_INFO* out) {
    if(!out) return D3DERR_INVALIDCALL;
    return bf2d3dx::metadata(p,n,*out)?D3D_OK:static_cast<HRESULT>(0x88760b59u);
}
#endif
