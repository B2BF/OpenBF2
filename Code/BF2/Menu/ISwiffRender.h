// Rendering interface exposed to the menu player plugin.
#ifndef MENU_ISWIFFRENDER_H
#define MENU_ISWIFFRENDER_H
#include "../Core/IBase.h"
#include <stdint.h>
#include <string>
#if !DEDICATED
namespace dice { namespace hfe { namespace ui {

struct ISwiffRender : public IBase
{
    virtual int   init() = 0;                                                              // +0x10
    virtual int   shutdown() = 0;                                                          // +0x14
    virtual int   slot6() = 0;                                                             // +0x18
    virtual int   uploadAlpha(unsigned int w, unsigned int h, const void* data) = 0;       //  7 +0x1C
    virtual int   slot8(int a) = 0;                                                        // +0x20
    virtual int   uploadRgba(unsigned int* desc) = 0;                                      //  9 +0x24
    virtual int   loadTexture(const char* name, unsigned int* outW, unsigned int* outH) = 0;   // 10 +0x28
    virtual int   releaseTexture(int id) = 0;                                              // +0x2C
    virtual void  beginDisplay(int bgUnused, int vpX, int vpY, int vpWidth, int vpHeight,
                               float xMin, float xMax, float yMin, float yMax) = 0;        // 12 +0x30
    virtual void  endDisplay() = 0;                                                        // 13 +0x34
    virtual void  setTransform(const float* m2x3) = 0;                                     // 14 +0x38
    virtual void  setCxform(const float* cx) = 0;                                          // +0x3C
    virtual int   recordStat(int value) = 0;                                               // +0x40
    virtual void  drawVertices(const short* verts, unsigned int count) = 0;                // 17 +0x44
    virtual void  drawLineStrip(const short* verts, unsigned int count) = 0;               // 18 +0x48
    virtual int   fillStyleDisable(int index) = 0;                                         // +0x4C
    virtual void  fillStyleColor(int index, unsigned int rgba) = 0;                        // 20 +0x50
    virtual void  setTexture(int index, int texId, int w, int h,
                             const float* matrix, int clamp) = 0;                          // 21 +0x54
    virtual void  lineStyleDisable() = 0;                                                  // +0x58
    virtual int   lineStyleColor(unsigned int rgba) = 0;                                   // 23 +0x5C
    virtual void  lineStyleWidth(unsigned int widthAsDword) = 0;                           // 24 +0x60
    virtual void  nullsub25(const void* a, int b, const void* c, const void* d,
                            unsigned int e) = 0;                                           // 25 +0x64  0x10152DE0 retn 14h
    virtual void* createFont(const char* name, int size) = 0;                              // +0x68
    virtual void  releaseFontHandle(void* font) = 0;                                       // +0x6C
    virtual int   fontForward16(void* font, int arg) = 0;                                  // +0x70
    virtual int   destroyFont(void* font) = 0;                                             // +0x74
    virtual void  drawGlyphBitmap(void* font, const float* m2x3, unsigned int rgba,
                                  int unused) = 0;                                         // 30 +0x78
    virtual char  fontGetExtent(void* font, int arg, float* outW, float* outH) = 0;        // +0x7C
    virtual char  fontForward32(void* font, int arg, float x, float y) = 0;                // +0x80
    virtual char  fontForward40(void* font, int a, int b) = 0;                             // +0x84
    virtual char  fontForward44(void* font, float x1, float y1, float x2, float y2) = 0;   // +0x88
    virtual int   fontForward48(void* font) = 0;                                           // +0x8C
    virtual int   fontSetSize(void* font, float size, int flag) = 0;                       // +0x90
    virtual int   fontSetField104(void* font, int value) = 0;                              // +0x94
    virtual void  nullsub38(int a) = 0;                                                    // 38 +0x98  0x101531A0 retn 4
    virtual void  beginMask() = 0;                                                         // 39 +0x9C
    virtual void  endMask() = 0;                                                           // 40 +0xA0
    virtual void  disableMask() = 0;                                                       // 41 +0xA4
    virtual void  setMovieWidth(unsigned int w) = 0;                                       // +0xA8
    virtual void  setMovieHeight(int h) = 0;                                               // +0xAC
    virtual void  setScale(float sx, float sy) = 0;                                        // +0xB0
    virtual void  setBackgroundColor(unsigned int rgb) = 0;                                // +0xB4
    virtual void  setMouseState(int x, int y, int buttons) = 0;                            // +0xB8
    virtual int   slot47(int a) = 0;                                                       // +0xBC
    virtual void  nullsub48(int a) = 0;                                                    // 48 +0xC0  0x10153380 retn 4
    virtual char  slot49(intptr_t a) = 0;                                                  // +0xC4
    virtual char  slot50(intptr_t a) = 0;                                                  // +0xC8
    virtual int   beginFrame() = 0;                                                        // 51 +0xCC  BF2.exe 0x4A2C26's call
    virtual int   endFrame() = 0;                                                          // 52 +0xD0  MainMenuLogic::endFrame
    virtual void* setTextureTechnique(const std::string& name, int index) = 0;            // 53 +0xD4
    virtual int   updateTime(unsigned int t, float dt) = 0;                                // +0xD8
    virtual void* getTextureRegistry() = 0;                                                // +0xDC
    virtual int   uploadTexture(const std::string& name, unsigned int* outW,
                                unsigned int* outH) = 0;                                   // 56 +0xE0  SwiffHost 0x488C20 (-1 = failure)
    virtual unsigned int* getDriverVersion(unsigned int* out) = 0;                         // 57 +0xE4  SwiffHost 0x488EA0
    virtual char  isNvidia() = 0;                                                          // 58 +0xE8  0x10153470  adapter vendor 0x10DE
    virtual char  getFlag() = 0;                                                           // +0xEC
    virtual char  setFlag(char v) = 0;                                                     // +0xF0
    virtual char  preloadCredits() = 0;                                                    // 61 +0xF4
    virtual int   creditsCount() = 0;                                                      // +0xF8
    virtual std::string getCreditsName(unsigned int index) = 0;                            // 63 +0xFC  (hidden-pointer return)
    virtual std::string getRandomCreditsName() = 0;                                        // +0x100
    virtual char  getWasDeviceLost() = 0;                                                  // +0x104
    virtual void  clearWasDeviceLost() = 0;                                                // +0x108
};

} } }
#endif
#endif
