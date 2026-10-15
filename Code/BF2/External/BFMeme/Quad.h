// [INFERRED] shared BFMeme drawing value type and path. Binary layout matches
// client constructors7732D0/7733D0 and RendDX9 QuadDrawer's192-byte request.
#ifndef BFMEME_QUAD_H
#define BFMEME_QUAD_H
#include "Style.h"
#if !DEDICATED
#include <cstdint>
namespace dice { namespace meme {
struct Color { float r, g, b, a; };
struct QuadCorner
{
    float x, y, z;
    Color color;
    float unused; // The two constructors do not write this final word.
};
struct Quad
{
    Quad();
    Quad(const Rect* bounds, const Rect* clip);
    Quad(const Rect* bounds, const Rect* clip, const Color* color);

    const Rect* clip;                           // +00; renderer uses first4floats
    unsigned char blend;                        // +04
    unsigned char padding05[3];                 // untouched
    int state08, state0c, state10, state14;      // +08..+14
    unsigned char state18;                      // +18
    unsigned char padding19[3];                 // untouched
    TextureHandle texture2;                     // +1C, texture handle; RendDX9 reads an IMemeTexture*
    std::uint32_t state20[8];                   // +20..+3C, all zeroed
    QuadCorner corners[4];                      // +40,+60,+80,+A0
};

// Shared drawing helpers, names and placement [INFERRED]. Both are free
// functions taking the Quad in ECX; every caller in this image is a menu/HUD
// node (Bf2MenuMapNode774xxx, Bf2PictureNode7ACxxx, Bf2CompassNode7CFxxx,
// Bf2ObjectMarkerNode7CCxxx).
void rotateQuad(Quad*, const float center[2], float radians);
void colorQuad(Quad*, const Color*);
void translateQuad(Quad*, float dx, float dy, float dz);
void setQuadBounds(Quad*, float x, float y, float width, float height);
} }
#endif
#endif
