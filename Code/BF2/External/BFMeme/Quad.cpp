#include "Quad.h"
#include "Stream.h"
#if !defined(_MSC_VER) || !defined(_M_IX86)
#include "../../Math/PortableX87.h"
#endif
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
// PE 0x101C4F00: RendDX9's copy of this body (cosine and sine from fcos/fsin once, then the four
// corners in order); unreachable in 1.50 there: no code or data references.
// Each corner rotates about the supplied centre. The cosine and sine
// come from the x87 fcos/fsin pair, not the CRT, and both are evaluated once
// before the four corners; keep that or the shipped pixel positions move.
// Retail also writes the two offsets back into the corner before using them,
// so the x offset is rounded to float32 while the y offset stays on the x87
// stack; the named float locals below reproduce the rounding that matters.
void rotateQuad(Quad* quad, const float center[2], float radians)
{
#if defined(_WIN32)
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
#else
// [CHOSEN, not recovered] Preserve the S2.1 POSIX numeric arm (docs/macos-port.md).
    float cosine, sine;
    // [CHOSEN, not recovered] Preserve x87 float trig stores.
    cosine = bf2x87::store(bf2x87::cosine(bf2x87::wide(radians)));
    sine = bf2x87::store(bf2x87::sine(bf2x87::wide(radians)));
    for (unsigned int i = 0; i < 4; ++i) {
        const float dx = quad->corners[i].x - center[0];
        const float dy = quad->corners[i].y - center[1];
        quad->corners[i].x = dx * cosine + dy * sine + center[0];
        quad->corners[i].y = dy * cosine - dx * sine + center[1];
    }
#endif
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

// PE 0x101C4E50 (ecx = quad, out on the stack): the mean of the four corner positions, summed in
// corner order with a float store per addition, then scaled by 0.25.
void quadCenter(const Quad* quad, float out[3])
{
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    out[0] = quad->corners[0].x;
    out[1] = quad->corners[0].y;
    out[2] = quad->corners[0].z;
    for (unsigned int i = 1; i < 4; ++i) {
        out[0] = quad->corners[i].x + out[0];
        out[1] = quad->corners[i].y + out[1];
        out[2] = quad->corners[i].z + out[2];
    }
    out[0] = out[0] * 0.25f;
    out[1] = out[1] * 0.25f;
    out[2] = out[2] * 0.25f;
}

// PE 0x101C5060 (ecx = quad, margin on the stack): grows the quad outward by the margin: corner 0
// moves by (-mx, -my), corner 1 by (+mx, -my), corner 2 by (+mx, +my), corner 3 by (-mx, +my).
void expandQuad(Quad* quad, const float margin[2])
{
    quad->corners[0].x = -margin[0] + quad->corners[0].x;
    quad->corners[0].y = -margin[1] + quad->corners[0].y;
    quad->corners[1].x = quad->corners[1].x + margin[0];
    quad->corners[1].y = -margin[1] + quad->corners[1].y;
    quad->corners[2].x = quad->corners[2].x + margin[0];
    quad->corners[2].y = margin[1] + quad->corners[2].y;
    quad->corners[3].x = -margin[0] + quad->corners[3].x;
    quad->corners[3].y = margin[1] + quad->corners[3].y;
}

namespace {
// Exchanges the seven leading words (position and colour) of two corners; the eighth word of each
// corner is not touched.
void swapCorners(QuadCorner& a, QuadCorner& b)
{
    const QuadCorner saved = a;
    a.x = b.x;
    a.y = b.y;
    a.z = b.z;
    a.color = b.color;
    b.x = saved.x;
    b.y = saved.y;
    b.z = saved.z;
    b.color = saved.color;
}
}

// PE 0x101C50D0 (ecx = quad): corners 0 and 1, then 2 and 3, exchange position and colour.
void flipQuadHorizontal(Quad* quad)
{
    swapCorners(quad->corners[0], quad->corners[1]);
    swapCorners(quad->corners[2], quad->corners[3]);
}

// PE 0x101C5210 (ecx = quad): corners 0 and 3, then 1 and 2, exchange position and colour.
void flipQuadVertical(Quad* quad)
{
    swapCorners(quad->corners[0], quad->corners[3]);
    swapCorners(quad->corners[1], quad->corners[2]);
}

// PE 0x101C4BE0 (ecx = rect, bounds on the stack): false, leaving the rect alone, when the two
// rectangles do not overlap (each edge compared with `>=`, a NaN edge counts as no overlap);
// otherwise trims the rect to the bounds' left, top, right and bottom in that order and answers
// true. [CHOSEN, not recovered] the retail x87 keeps each delta unrounded on the stack; a double
// delta is used here because no 1.50 path executes the body.
bool clipRect(Rect* rect, const Rect* bounds)
{
    if (!(rect->x + rect->width >= bounds->x)) return false;
    if (!(rect->y + rect->height >= bounds->y)) return false;
    if (!(bounds->x + bounds->width >= rect->x)) return false;
    if (!(bounds->height + bounds->y >= rect->y)) return false;
    double delta = static_cast<double>(bounds->x) - rect->x;
    if (delta > 0.0) {
        rect->width = static_cast<float>(rect->width - delta);
        rect->x = bounds->x;
    }
    delta = static_cast<double>(bounds->y) - rect->y;
    if (delta > 0.0) {
        rect->height = static_cast<float>(rect->height - delta);
        rect->y = bounds->y;
    }
    delta = static_cast<double>(rect->x + rect->width) - (bounds->x + bounds->width);
    if (delta > 0.0)
        rect->width = static_cast<float>(rect->width - delta);
    delta = static_cast<double>(rect->y + rect->height) - (bounds->height + bounds->y);
    if (delta > 0.0)
        rect->height = static_cast<float>(rect->height - delta);
    return true;
}

// PE 0x101C4B30 (ecx = colour, stream on the stack): streams "Red", "Green", "Blue" and "Alpha"
// into the four floats, stopping at the first refusal.
bool streamColor(Color* color, Stream* stream)
{
    if (!stream->streamFloat(&color->r, "Red")) return false;
    if (!stream->streamFloat(&color->g, "Green")) return false;
    if (!stream->streamFloat(&color->b, "Blue")) return false;
    return stream->streamFloat(&color->a, "Alpha");
}

// PE 0x101C4CC0 (ecx = rect, stream on the stack): streams "X", "Y", "Width" and "Height" into the first four floats,
// stopping at the first refusal.
bool streamRect(Rect* rect, Stream* stream)
{
    if (!stream->streamFloat(&rect->x, "X")) return false;
    if (!stream->streamFloat(&rect->y, "Y")) return false;
    if (!stream->streamFloat(&rect->width, "Width")) return false;
    return stream->streamFloat(&rect->height, "Height");
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
