// [CHOSEN, not recovered] Native gameswf-to-renderer transport. Known calls use
// the shared interface so C++ selects their slots and register conventions.
#ifndef BF2_SWIFF_NATIVE_RENDER_CALLS_H
#define BF2_SWIFF_NATIVE_RENDER_CALLS_H
#if !defined(_MSC_VER)
#include "../BF2/Menu/ISwiffRender.h"
#include <string>
namespace dice { namespace hfe { namespace ui { namespace native_render {
inline ISwiffRender* view(void* object) { return static_cast<ISwiffRender*>(object); }

// The five former argument-mapping stops (slots 8, 32, 33, 34, 53) follow the
// shipped SwiffPlayer.dll forwarders and RendDX9.dll SwiffRender bodies.
inline int rcCreateEmpty(void* rc) { return view(rc)->slot6(); }
inline int rcUploadAlpha(void* rc, unsigned w, unsigned h, const void* data) { return view(rc)->uploadAlpha(w,h,data); }
// 0x10005B56: `push edi` (the image::rgb*) then call +0x20; RendDX9 0x101529D0 `or eax,-1; ret 4`.
inline int rcCreateRgb(void* rc, void* image) { return view(rc)->slot8(image); }
inline int rcUploadRgba(void* rc, void* image) { return view(rc)->uploadRgba(static_cast<unsigned*>(image)); }
inline int rcLoadTexture(void* rc, const char* name, int* w, int* h)
{ return view(rc)->loadTexture(name, reinterpret_cast<unsigned*>(w), reinterpret_cast<unsigned*>(h)); }
inline void rcReleaseTexture(void* rc, int id) { view(rc)->releaseTexture(id); }
inline void rcBeginDisplay(void* rc, int bg, int x, int y, int w, int h, float x0, float x1, float y0, float y1)
{ view(rc)->beginDisplay(bg,x,y,w,h,x0,x1,y0,y1); }
inline void rcEndDisplay(void* rc) { view(rc)->endDisplay(); }
inline void rcSetTransform(void* rc, const float* m) { view(rc)->setTransform(m); }
inline void rcSetCxform(void* rc, const float* m) { view(rc)->setCxform(m); }
inline void rcDrawVertices(void* rc, const void* vertices, int count)
{ view(rc)->drawVertices(static_cast<const short*>(vertices), static_cast<unsigned>(count)); }
inline void rcDrawLineStrip(void* rc, const void* vertices, int count)
{ view(rc)->drawLineStrip(static_cast<const short*>(vertices), static_cast<unsigned>(count)); }
inline void rcFillStyleDisable(void* rc, int side) { view(rc)->fillStyleDisable(side); }
inline void rcFillStyleColor(void* rc, int side, unsigned rgba) { view(rc)->fillStyleColor(side,rgba); }
inline void rcSetTexture(void* rc, int side, int id, int w, int h, const float* m, int wrap)
{ view(rc)->setTexture(side,id,w,h,m,wrap); }
inline void rcLineStyleDisable(void* rc) { view(rc)->lineStyleDisable(); }
inline void rcLineStyleColor(void* rc, unsigned rgba) { view(rc)->lineStyleColor(rgba); }
inline void rcLineStyleWidth(void* rc, unsigned width) { view(rc)->lineStyleWidth(width); }
inline void rcDrawBitmapNull(void* rc, const void* m, int id, const void* coordinates, const void* uv, unsigned rgba)
{ view(rc)->nullsub25(m,id,coordinates,uv,rgba); }
inline void rcBeginMask(void* rc) { view(rc)->beginMask(); }
inline void rcEndMask(void* rc) { view(rc)->endMask(); }
inline void rcDisableMask(void* rc) { view(rc)->disableMask(); }
inline void* rcTextCreate(void* rc, const char* name, int size) { return view(rc)->createFont(name,size); }
inline void rcTextRelease(void* rc, void* line) { view(rc)->releaseFontHandle(line); }
inline void rcTextSet(void* rc, void* line, const wchar_t* text) { view(rc)->fontForward16(line,text); }
inline void rcTextDetach(void* rc, void* line) { view(rc)->destroyFont(line); }
inline void rcTextDraw(void* rc, void* line, const void* m, unsigned color, int flags)
{ view(rc)->drawGlyphBitmap(line,static_cast<const float*>(m),color,flags); }
inline char rcTextExtent(void* rc, void* line, int count, float* w, float* h) { return view(rc)->fontGetExtent(line,count,w,h); }
// 0x10005E20 -> 0x10152FF0: (line, int* outPos, float x, float y); x/y are fld/fmul'd.
inline char rcTextHit(void* rc, void* line, int* pos, float x, float y) { return view(rc)->fontForward32(line,pos,x,y); }
// 0x10005E50 -> 0x10153030 -> font 0x10152080: two int* outs written; AL = selection exists.
inline int rcTextSelection(void* rc, void* line, int* a, int* b) { return view(rc)->fontForward40(line,a,b); }
// 0x10005E90 (jmp) -> 0x10153060: four floats, each fld/fmul'd by the scale.
inline char rcTextSetSelection(void* rc, void* line, float x1, float y1, float x2, float y2)
{ return view(rc)->fontForward44(line,x1,y1,x2,y2); }
inline void rcTextBlur(void* rc, void* line) { view(rc)->fontForward48(line); }
inline void rcTextSetSize(void* rc, void* line, float size, int flags) { view(rc)->fontSetSize(line,size,flags); }
inline void rcTextCaret(void* rc, void* line, int position) { view(rc)->fontSetField104(line,position); }
inline void rcSetMovieWidth(void* rc, unsigned w) { view(rc)->setMovieWidth(w); }
inline void rcSetMovieHeight(void* rc, int h) { view(rc)->setMovieHeight(h); }
inline void rcSetScale(void* rc, float x, float y) { view(rc)->setScale(x,y); }
inline void rcSetBackgroundColor(void* rc, unsigned rgb) { view(rc)->setBackgroundColor(rgb); }
inline void rcSetMouseState(void* rc, int x, int y, int buttons) { view(rc)->setMouseState(x,y,buttons); }
inline char rcCapability0(void* rc, intptr_t value) { return view(rc)->slot49(value); }
inline char rcCapability1(void* rc, intptr_t value) { return view(rc)->slot50(value); }
// 0x10005F50 (jmp +0xD4) -> 0x10154E60: std::map<std::string,int> at +0x3D8 keyed by the string.
inline void rcSetPairState(void* rc, const std::string& name, int index) { view(rc)->setTextureTechnique(name,index); }
inline void rcUpdateTime(void* rc, unsigned t, float delta) { view(rc)->updateTime(t,delta); }
} } } }
#endif
#endif
