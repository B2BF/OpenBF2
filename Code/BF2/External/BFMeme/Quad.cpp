#include "Quad.h"
#if !DEDICATED
#include <cmath>
#include <cstddef>
namespace dice { namespace meme {
Quad::Quad()
    : clip(nullptr), blend(1), state08(7), state0c(8),
      state10(-1), state14(-1), state18(0), texture2(0)
{
    for (unsigned int i = 0; i < 8; ++i) state20[i] = 0;
    for (unsigned int i = 0; i < 4; ++i)
        corners[i].color = Color{1.0f, 1.0f, 1.0f, 1.0f};
    // Retail leaves every position and QuadCorner::unused word untouched.
}

Quad::Quad(const Rect* bounds, const Rect* clipping)
    : clip(clipping), blend(1), state08(7), state0c(8),
      state10(-1), state14(-1), state18(0), texture2(0)
{
    for (unsigned int i = 0; i < 8; ++i) state20[i] = 0;
    for (unsigned int i = 0; i < 4; ++i)
        corners[i].color = Color{1.0f, 1.0f, 1.0f, 1.0f};
    const double x = bounds->x;
    const double right = x + bounds->width;
    const double y = bounds->y;
    const float bottom = static_cast<float>(y + bounds->height);
    const float z = bounds->z;
    corners[0].x = static_cast<float>(x);
    corners[0].y = static_cast<float>(y);
    corners[0].z = z;
    corners[1].z = z;
    corners[1].x = static_cast<float>(right);
    corners[1].y = static_cast<float>(y);
    corners[2].y = bottom;
    corners[2].z = z;
    corners[2].x = static_cast<float>(right);
    corners[3].y = bottom;
    corners[3].z = z;
    corners[3].x = static_cast<float>(x);
}

Quad::Quad(const Rect* bounds, const Rect* clipping, const Color* color)
    : clip(clipping), blend(1), state08(7), state0c(8),
      state10(-1), state14(-1), state18(0), texture2(0)
{
    for (unsigned int i = 0; i < 8; ++i) state20[i] = 0;
    for (unsigned int i = 0; i < 4; ++i)
        corners[i].color = Color{1.0f, 1.0f, 1.0f, 1.0f};
    const double x = bounds->x;
    const double right = x + bounds->width;
    const double y = bounds->y;
    const float bottom = static_cast<float>(y + bounds->height);
    const float z = bounds->z;
    corners[0].x = static_cast<float>(x);
    corners[0].y = static_cast<float>(y);
    corners[0].z = z;
    corners[0].color = *color;
    corners[1].x = static_cast<float>(right);
    corners[1].y = static_cast<float>(y);
    corners[1].z = z;
    corners[1].color = *color;
    corners[2].x = static_cast<float>(right);
    corners[2].y = bottom;
    corners[2].z = z;
    corners[2].color = *color;
    corners[3].x = static_cast<float>(x);
    corners[3].y = bottom;
    corners[3].z = z;
    corners[3].color = *color;
}
// Each corner rotates about the supplied centre. The cosine and sine
// come from the x87 fcos/fsin pair, not the CRT, and both are evaluated once
// before the four corners; keep that or the shipped pixel positions move.
// Retail also writes the two offsets back into the corner before using them,
// so the x offset is rounded to float32 while the y offset stays on the x87
// stack; the named float locals below reproduce the rounding that matters.
void rotateQuad(Quad* quad, const float center[2], float radians)
{
    float cosine, sine;
#if defined(_M_IX86)
    __asm {
        fld radians
        fcos
        fstp cosine
        fld radians
        fsin
        fstp sine
    }
#else
    // [PLATFORM] x64: no x87 fcos/fsin. The CRT double results rounded to float match the
    // x87 ones except in rare last-bit cases; |radians| >= 2^63 (left unreduced by x87) differs.
    cosine = static_cast<float>(std::cos(static_cast<double>(radians)));
    sine = static_cast<float>(std::sin(static_cast<double>(radians)));
#endif
    for (unsigned int i = 0; i < 4; ++i) {
        const float dx = quad->corners[i].x - center[0];
        const float dy = quad->corners[i].y - center[1];
        quad->corners[i].x = dx * cosine + dy * sine + center[0];
        quad->corners[i].y = dy * cosine - dx * sine + center[1];
    }
}

// Overwrites all four corner colors; no alpha or range handling.
void colorQuad(Quad* quad, const Color* color)
{
    for (unsigned int i = 0; i < 4; ++i) quad->corners[i].color = *color;
}

// Offsets every corner; the fourth position word is left alone.
void translateQuad(Quad* quad, float dx, float dy, float dz)
{
    for (unsigned int i = 0; i < 4; ++i) {
        quad->corners[i].x += dx;
        quad->corners[i].y += dy;
        quad->corners[i].z += dz;
    }
}

// Repositions an existing quad without disturbing its render state or
// colours. The first corner's z is copied to the other three corners; its own
// z is left untouched.
void setQuadBounds(Quad* quad, float x, float y, float width, float height)
{
    const float z = quad->corners[0].z;
    quad->corners[0].x = x;
    quad->corners[0].y = y;
    quad->corners[0].z = z;
    quad->corners[1].x = x + width;
    quad->corners[1].y = y;
    quad->corners[1].z = z;
    quad->corners[2].x = x + width;
    quad->corners[2].y = y + height;
    quad->corners[2].z = z;
    quad->corners[3].x = x;
    quad->corners[3].y = y + height;
    quad->corners[3].z = z;
}

static_assert(sizeof(QuadCorner) == 32, "retail quad corner stride");
#if defined(_M_IX86)
static_assert(sizeof(Quad) == 192, "retail QuadDrawer request size");
#else
// RendDX9's QuadDrawRequest (GeomMisc/MenuRend/QuadDrawer.cpp) declares an 8-byte clip pointer,
// a pointer texture2 and 0x20 bytes before the corners; with the pointer-sized handle both
// sides place texture2 at +0x20 and the corners at +0x48 on x64.
static_assert(offsetof(Quad, texture2) == 0x20 && offsetof(Quad, corners) == 0x48
              && sizeof(Quad) == 0xC8, "x64 QuadDrawer request layout");
#endif
} }
#endif
