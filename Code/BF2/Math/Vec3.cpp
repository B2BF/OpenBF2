// Shared vectors, basis construction, coordinate helpers, and text serialization.

#include "Vec3.h"
#include <cmath>        // makeOrthonormalBasis
#include <istream>      // the console Vec3 text form below
#include <ostream>
#include <ios>

namespace dice {
namespace hfe {

namespace {
// [CHOSEN, not recovered] Explicit Init objects package the three original TUs in this merged file.
// static init: ELF 0x367450 / 0x3674B0 / 0x3674E0; engine 0x83F8170 / 0x83F81C0 / 0x83F81D0.
std::ios_base::Init boxStreamInit;
// static init: ELF 0x36C860 / 0x36C8C0 / 0x36C8F0; engine 0x83FB9E0 / 0x83FBA30 / 0x83FBA40.
std::ios_base::Init coordinateStreamInit;
// static init: ELF 0x49C8D0 / 0x49C930 / 0x49C960; engine 0x83B9DD0 / 0x83B9E20 / 0x83B9E30.
std::ios_base::Init basisStreamInit;
}

namespace Math {


// [CHOSEN, not recovered] Stable zero-vector storage for reference-returning accessors.

namespace {
const Vec3 s_zero = { 0.0f, 0.0f, 0.0f };
const Vec3 s_identity = { 1.0f, 1.0f, 1.0f };
}

const Vec3& Vec3::getZero()
{
    return s_zero;
}


// Basis construction uses FLT_EPSILON for each normalization.

namespace {

const float BASIS_EPS = 1.19209290e-7f;                  // FLT_EPSILON, as the body spells it

// The inlined "normalise unless it already is" step. False = degenerate; the caller zeroes.
bool normaliseIfNeeded(Vec3& v)
{
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lenSq - 1.0f) < BASIS_EPS)
        return true;                                     // already unit - untouched
    if (std::fabs(lenSq) < BASIS_EPS)
        return false;
    const float inv = 1.0f / std::sqrt(lenSq);
    v.x *= inv; v.y *= inv; v.z *= inv;
    return true;
}

void zero(Vec3& v) { v.x = 0.0f; v.y = 0.0f; v.z = 0.0f; }

void cross(Vec3& out, const Vec3& a, const Vec3& b)
{
    out.x = a.y * b.z - a.z * b.y;
    out.y = a.z * b.x - a.x * b.z;
    out.z = a.x * b.y - a.y * b.x;
}

void snapSmall(Vec3& v)
{
    if (std::fabs(v.x) < BASIS_EPS) v.x = 0.0f;
    if (std::fabs(v.y) < BASIS_EPS) v.y = 0.0f;
    if (std::fabs(v.z) < BASIS_EPS) v.z = 0.0f;
}

} // namespace

int Vec3::makeOrthonormalBasis(Vec3& forward, Vec3& up)
{
    if (!normaliseIfNeeded(forward)) { zero(forward); return -20003; }
    if (!normaliseIfNeeded(up))      { zero(up);      return -20003; }

    cross(*this, forward, up);
    if (!normaliseIfNeeded(*this))   { zero(*this);   return -20003; }

    // The one step that does not abandon the basis on failure - see the header.
    cross(up, *this, forward);
    if (!normaliseIfNeeded(up))
        zero(up);

    cross(forward, up, *this);
    if (!normaliseIfNeeded(forward)) { zero(forward); return -20003; }

    snapSmall(*this);
    snapSmall(forward);
    snapSmall(up);
    return 0;
}

const Vec3& Vec3::getIdentity()
{
    return s_identity;
}

// [CHOSEN] Stream operators for slash-separated vector coordinates.
std::istream& operator>>(std::istream& in, Vec3& v)
{
    float f = 0.0f;
    char sep = 0;
    in >> f;                // istream::operator>>(float&)
    in >> sep;              // one plain char - the '/' (any single char accepted)
    v.x = f;
    in >> f;
    in >> sep;
    v.y = f;
    in >> f;                // no separator after the last component
    v.z = f;
    return in;
}

std::ostream& operator<<(std::ostream& out, const Vec3& v)
{
    out << v.x << '/' << v.y << '/' << v.z;    // default precision, '/' separators
    return out;
}

// The Vec2 form: float >> char >> float / x '/' y (the hoistMinMax pair, ConsoleClass1259).
std::istream& operator>>(std::istream& in, Vec2& v)
{
    float f = 0.0f;
    char sep = 0;
    in >> f;
    in >> sep;
    v.x = f;
    in >> f;
    v.y = f;
    return in;
}

std::ostream& operator<<(std::ostream& out, const Vec2& v)
{
    out << v.x << '/' << v.y;
    return out;
}

// Parses/prints four slash-separated components; separators are consumed without validation.
std::istream& operator>>(std::istream& in, Vec4& v)
{
    float f = 0.0f;
    char sep = 0;
    in >> f;
    in >> sep;
    v.x = f;
    in >> f;
    in >> sep;
    v.y = f;
    in >> f;
    in >> sep;
    v.z = f;
    in >> f;                // no separator after the last component
    v.w = f;
    return in;
}

std::ostream& operator<<(std::ostream& out, const Vec4& v)
{
    out << v.x << '/' << v.y << '/' << v.z << '/' << v.w;
    return out;
}

}
// Projects x/z into Vec2, discarding height.

void vec3ToVec2XZ(const Vec3& in, Vec2& out)
{
    out.x = in.x;
    out.y = in.z;
}


// Maps Vec2 x/y to Vec3 x/z and explicitly clears height.

void vec2ToVec3XZ(const Vec2& in, Vec3& out)
{
    out.x = in.x;
    out.y = 0.0f;
    out.z = in.y;
}

// ELF libaidll 0x36AE40 (bf2 0x83FA260).
void vec3ToVec2XY(const Vec3& in, Vec2& out)
{
    out.x = in.x;
    out.y = in.y;
}

// ELF libaidll 0x36AE60 (bf2 0x83FA280): z cleared last.
void vec2ToVec3XY(const Vec2& in, Vec3& out)
{
    out.x = in.x;
    out.y = in.y;
    out.z = 0.0f;
}

// ELF libaidll 0x36AEE0 (bf2 0x83FA2E0).
void vec3ToVec2YZ(const Vec3& in, Vec2& out)
{
    out.x = in.y;
    out.y = in.z;
}

// ELF libaidll 0x36AF00 (bf2 0x83FA300): x cleared first.
void vec2ToVec3YZ(const Vec2& in, Vec3& out)
{
    out.x = 0.0f;
    out.y = in.x;
    out.z = in.y;
}

// ELF libaidll 0x36AFB0 (bf2 0x83FA380).
void addVec3ToVec2XZ(const Vec3& in, Vec2& out)
{
    out.x = in.x + out.x;
    out.y = in.z + out.y;
}

// ELF libaidll 0x36AFD0.
void subVec3ToVec2XZ(const Vec3& in, Vec2& out)
{
    out.x = out.x - in.x;
    out.y = out.y - in.z;
}

// ELF libaidll 0x36AFF0 (bf2 0x83FA3C0).
void addVec2ToVec3XZ(const Vec2& in, Vec3& out)
{
    out.x = in.x + out.x;
    out.z = in.y + out.z;
}

// ELF libaidll 0x36B010 (bf2 0x83FA3E0).
void subVec2ToVec3XZ(const Vec2& in, Vec3& out)
{
    out.x = out.x - in.x;
    out.z = out.z - in.y;
}

// ELF libaidll 0x36C1E0: out.y is cleared first; x' = x cos - z sin, z' = sin x + cos z.
void rotateXZ(const Vec3& in, Vec3& out, float radians)
{
    out.y = 0.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out.x = in.x * c - in.z * s;
    out.z = s * in.x + c * in.z;
}

// ELF 0x36BF10; engine ELF 0x83FB1D0. Float arithmetic is explicit in the amd-64 SSE body.
float distXZSqr(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return dx * dx + dz * dz;
}

// libaidll.so 0x36AF30 (the engine's Math TU, beside vec3ToVec2XZ).
void getNormal(const Vec2& in, Vec2& out)
{
    const float x = in.x;
    const float y = in.y;
    out.x = y;
    out.y = -x;
}

// libaidll.so 0x36AF70 (FireObject::isWithinRadius's caller): `mov 8(in),out.x`, then in.x with
// the sign bit flipped (`xor $0x80000000`) to out.y.
void getVec2NormalFromVec3XZ(const Vec3& in, Vec2& out)
{
    out.x = in.z;
    out.y = -in.x;
}

// libaidll.so 0x36C2E0.
bool lineIntersect(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d, float& t, float& s)
{
    const float denominator = (b.x - a.x) * (d.y - c.y) - (b.y - a.y) * (d.x - c.x);
    if (denominator == 0.0f)
        return false;
    t = ((a.y - c.y) * (d.x - c.x) - (d.y - c.y) * (a.x - c.x)) / denominator;
    s = ((a.y - c.y) * (b.x - a.x) - (a.x - c.x) * (b.y - a.y)) / denominator;
    return true;
}

// ELF libaidll 0x36B0F0; engine bf2 0x83FA4C0. The temporary output is stored before the
// origin is reread, so output aliasing a retains the shipped double-add behavior.
bool lineIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, Vec3& output)
{
    const Vec3 first = {b.x - a.x, b.y - a.y, b.z - a.z};
    Vec3 firstUnit = first;
    if (!Math::normaliseIfNeeded(firstUnit))
        Math::zero(firstUnit);
    const Vec3 second = {d.x - c.x, d.y - c.y, d.z - c.z};
    Vec3 secondUnit = second;
    if (!Math::normaliseIfNeeded(secondUnit))
        Math::zero(secondUnit);
    const float dot = firstUnit.x * secondUnit.x + firstUnit.y * secondUnit.y +
                      firstUnit.z * secondUnit.z;
    if (std::fabs(std::fabs(dot) - 1.0f) < 0.001f)
        return false;
    const Vec3 offset = {c.x - a.x, c.y - a.y, c.z - a.z};
    Vec3 cross;
    Math::cross(cross, first, second);
    Vec3 offsetCross;
    Math::cross(offsetCross, offset, second);
    const float denominator = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;
    if (denominator == 0.0f)
    {
        if (offsetCross.x * offsetCross.x + offsetCross.y * offsetCross.y +
            offsetCross.z * offsetCross.z != 0.0f)
            return false;
        output = a;
        return true;
    }
    const float parameter = (cross.x * offsetCross.x + cross.y * offsetCross.y +
                             cross.z * offsetCross.z) / denominator;
    output.x = first.x * parameter;
    output.y = first.y * parameter;
    output.z = first.z * parameter;
    output.x += a.x;
    output.y += a.y;
    output.z += a.z;
    return true;
}

// ELF libaidll 0x36B6C0; engine bf2 0x83FAA20.
bool linePointOnSegment(const Vec3& point, const Vec3& a, const Vec3& b)
{
    const Vec3 direction = {b.x - a.x, b.y - a.y, b.z - a.z};
    const Vec3 offset = {point.x - a.x, point.y - a.y, point.z - a.z};
    Vec3 unitDirection = direction;
    if (!Math::normaliseIfNeeded(unitDirection))
        Math::zero(unitDirection);
    Vec3 unitOffset = offset;
    if (!Math::normaliseIfNeeded(unitOffset))
        Math::zero(unitOffset);
    if (std::fabs(std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z)) <
        0.001f)
        return true;
    const float dot = unitOffset.x * unitDirection.x + unitOffset.y * unitDirection.y +
                      unitOffset.z * unitDirection.z;
    if (std::fabs(dot - 1.0f) > 0.001f)
        return false;
    const float segmentLength = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                          direction.z * direction.z);
    const float offsetLength = std::sqrt(offset.x * offset.x + offset.y * offset.y +
                                         offset.z * offset.z);
    const float difference = segmentLength - offsetLength;
    if (std::fabs(difference) > 0.001f && difference < 0.0f)
        return false;
    const float scale = segmentLength / offsetLength;
    const Vec3 projected = {offset.x * scale, offset.y * scale, offset.z * scale};
    const Vec3 error = {projected.x - direction.x, projected.y - direction.y,
                         projected.z - direction.z};
    return std::fabs(error.x * error.x + error.y * error.y + error.z * error.z) < 0.001f;
}

// ELF libaidll 0x36B610; engine bf2 0x83FA980.
bool lineSegmentIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, Vec3& output)
{
    // shipped defect: a failed line calculation still tests the unchanged output.
    lineIntersect(a, b, c, d, output);
    return linePointOnSegment(output, a, b) && linePointOnSegment(output, c, d);
}

// ELF libaidll 0x36B5B0; engine bf2 0x83FA930.
bool lineSegmentIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
{
    Vec3 output = {};
    return lineSegmentIntersect(a, b, c, d, output);
}

// libaidll.so 0x36C650. Each pass projects the edge's endpoints and the point (three
// vec3ToVec2XZ calls, point last), takes getNormal of the edge inline, and rejects on
// `0 > dot` (ja: an unordered dot does not reject). An empty polygon is inside.
bool pointWithinPolygon2d(const Vec3& point, Vec3* polygon, int count)
{
    for (int i = 0; i < count;)
    {
        Vec2 a = { 0.0f, 0.0f };
        vec3ToVec2XZ(polygon[i], a);
        ++i;
        Vec2 b = { 0.0f, 0.0f };
        vec3ToVec2XZ(polygon[i % count], b);
        Vec2 edge;
        edge.x = b.x - a.x;
        edge.y = b.y - a.y;
        Vec2 normal;
        getNormal(edge, normal);
        Vec2 p = { 0.0f, 0.0f };
        vec3ToVec2XZ(point, p);
        const float dot = (p.x - a.x) * normal.x + (p.y - a.y) * normal.y;
        if (0.0f > dot)
            return false;
    }
    return true;
}

// libaidll.so 0x36C790. The same test on 2D vertices; the point is read once.
bool pointWithinPolygon2d(const Vec2& point, Vec2* polygon, int count)
{
    for (int i = 0; i < count;)
    {
        const Vec2& a = polygon[i];
        ++i;
        const Vec2& b = polygon[i % count];
        Vec2 edge;
        edge.x = b.x - a.x;
        edge.y = b.y - a.y;
        Vec2 normal;
        getNormal(edge, normal);
        const float dot = (point.x - a.x) * normal.x + (point.y - a.y) * normal.y;
        if (0.0f > dot)
            return false;
    }
    return true;
}

// Computes (centre - minimum) + centre in double precision before each float store.
// Stores x before reading y; output may alias either input. Returns output.
Vec2& getBoxMaxCorner(const Vec2& minimum, const Vec2& centre, Vec2& output)
{
    output.x = static_cast<float>((static_cast<double>(centre.x) - minimum.x) + centre.x);
    output.y = static_cast<float>((static_cast<double>(centre.y) - minimum.y) + centre.y);
    return output;
}

// The four `ja` tests: unordered comparisons do not reject.
bool insideCentreBox(const Vec2& point, const Vec2& minimum, const Vec2& centre)
{
    if (minimum.x > point.x || minimum.y > point.y)
        return false;
    Vec2 maximum;
    maximum.x = 0.0f;
    maximum.y = 0.0f;
    getBoxMaxCorner(minimum, centre, maximum);
    return !(point.x > maximum.x) && !(point.y > maximum.y);
}

// libaidll.so 0x366D30. The four `ja` tests: unordered comparisons do not reject.
bool insideCornerBox(const Vec2& point, const Vec2& minimum, const Vec2& maximum)
{
    if (minimum.x > point.x || minimum.y > point.y || point.x > maximum.x)
        return false;
    return !(point.y > maximum.y);
}

// libaidll.so 0x366DB0.
void getBoxCentre(const Vec2& minimum, Vec2& centre, const Vec2& maximum)
{
    centre.x = (maximum.x + minimum.x) * 0.5f;
    centre.y = (maximum.y + minimum.y) * 0.5f;
}

// ELF 0x366F60 (libaidll.so), engine ELF 0x83F7CC0: preserve the early axis collapses.
bool getBoxIntersections(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d,
                         Vec2& outMinimum, Vec2& outMaximum)
{
    float minimum;
    float maximum;
    float collapsed = 0.0f;
    bool collapse = false;
    if (d.x > b.x)
    {
        if (c.x > b.x)
            return false;
        maximum = b.x;
        if (b.x == c.x || c.x > a.x)
            minimum = c.x;
        else
        {
            minimum = a.x;
            collapse = true;
            collapsed = b.y > d.y ? d.y : c.y;
        }
    }
    else if (b.x == d.x)
    {
        minimum = c.x > a.x ? c.x : a.x;
        maximum = d.x;
    }
    else
    {
        if (a.x > d.x)
            return false;
        maximum = d.x;
        if (a.x == d.x || !(c.x > a.x))
            minimum = a.x;
        else
        {
            minimum = c.x;
            collapse = true;
            collapsed = d.y > b.y ? b.y : a.y;
        }
    }
    outMinimum.x = minimum;
    outMaximum.x = maximum;
    if (collapse)
    {
        outMinimum.y = collapsed;
        outMaximum.y = collapsed;
        return true;
    }

    if (d.y > b.y)
    {
        if (c.y > b.y)
            return false;
        maximum = b.y;
        if (b.y == c.y || c.y > a.y)
            minimum = c.y;
        else
        {
            minimum = a.y;
            collapse = true;
            collapsed = b.x > d.x ? d.x : c.x;
        }
    }
    else if (b.y == d.y)
    {
        minimum = c.y > a.y ? c.y : a.y;
        maximum = d.y;
    }
    else
    {
        if (a.y > d.y)
            return false;
        maximum = d.y;
        if (a.y == d.y || !(c.y > a.y))
            minimum = a.y;
        else
        {
            minimum = c.y;
            collapse = true;
            collapsed = d.x > b.x ? b.x : a.x;
        }
    }
    outMinimum.y = minimum;
    outMaximum.y = maximum;
    if (collapse)
    {
        outMinimum.x = collapsed;
        outMaximum.x = collapsed;
    }
    return true;
}

// libaidll.so 0x3671E0. The line is direction . q + offset = 0 with direction = (normal.y,
// -normal.x); atMinimum / atMaximum are its values at the two corners. Each range test is written
// so that a NaN parameter fails it, as the image's `jb` tests do.
void getIntersection(const Vec2& minimum, const Vec2& maximum, const Vec2& normal,
                     const Vec2& point, Vec2& out)
{
    const float width = maximum.x - minimum.x;
    const float height = maximum.y - minimum.y;
    if (width == 0.0f || height == 0.0f)
    {
        out = point;
        return;
    }
    Vec2 direction;
    direction.x = normal.y;
    direction.y = -normal.x;
    const float offset = -(direction.x * point.x + direction.y * point.y);
    const float atMinimum = direction.y * minimum.y + minimum.x * direction.x + offset;
    const float atMaximum = maximum.x * direction.x + direction.y * maximum.y + offset;
    if (normal.y > 0.0f)
    {
        const float t = atMaximum / (normal.y * width);
        if (t >= 0.0f && t <= 1.0f)
        {
            out.y = maximum.y;
            out.x = maximum.x - t * width;
            return;
        }
    }
    else if (normal.y < 0.0f)
    {
        const float t = -atMinimum / (normal.y * width);
        if (t >= 0.0f && t <= 1.0f)
        {
            out.y = minimum.y;
            out.x = t * width + minimum.x;
            return;
        }
    }
    if (normal.x < 0.0f)
    {
        const float t = -atMinimum / (height * direction.y);
        if (!(t >= 0.0f && t <= 1.0f))
            return;
        out.x = minimum.x;
        out.y = minimum.y + t * height;
    }
    else if (normal.x > 0.0f)
    {
        const float t = atMaximum / (height * direction.y);
        if (!(t >= 0.0f && t <= 1.0f))
            return;
        out.x = maximum.x;
        out.y = maximum.y - t * height;
    }
}

// libaidll.so 0x36C470 (recovered by ai-w1-b-entries for the mouse-turret entries). The cosine is
// clamped twice in the image (same value); the angle to the left normal gives the magnitude, and
// a negative dot product folds it onto the far half circle.
float getSignedDiffAngleToDir(const Vec2& a, const Vec2& b)
{
    Vec2 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    getNormal(a, normal);
    float c = b.x * normal.x + b.y * normal.y;
    if (!(-1.0f <= c))
        c = -1.0f;
    else if (1.0f < c)
        c = 1.0f;
    float angle = 1.57079637f - std::acos(c);
    if (b.x * a.x + b.y * a.y < 0.0f)
    {
        float sign = 1.0f;
        if (angle <= 0.0f)
            sign = (angle < 0.0f) ? -1.0f : 0.0f;
        angle = sign * 3.14159274f - angle;
    }
    return angle;
}

// libaidll.so 0x36C570 (listing). As getSignedDiffAngleToDir with `normal` supplied; the dot product
// is clamped only when it is ordered and outside [-1, 1], and the flip applies unless b.a >= 0.
float getSignedDiffAngleToDirWithNormal(const Vec2& a, const Vec2& normal, const Vec2& b)
{
    float c = b.y * normal.y + b.x * normal.x;
    if (-1.0f > c)
        c = -1.0f;
    else if (c > 1.0f)
        c = 1.0f;
    float angle = 1.57079637f - std::acos(c);
    if (!(b.y * a.y + b.x * a.x >= 0.0f))
    {
        float sign = 1.0f;
        if (!(angle > 0.0f))
            sign = (0.0f > angle) ? -1.0f : 0.0f;
        angle = sign * 3.14159274f - angle;
    }
    return angle;
}

// libaidll.so 0x36C3B0
float getSignedDiffAngleToDirXZ(const Vec3& a, const Vec3& b)
{
    Vec2 a2;
    a2.x = a.x;
    a2.y = a.z;
    Vec2 b2;
    b2.x = b.x;
    b2.y = b.z;
    return getSignedDiffAngleToDir(a2, b2);
}

// libaidll.so 0x36C410
float getSignedDiffAngleToDirYZ(const Vec3& a, const Vec3& b)
{
    Vec2 a2;
    a2.x = a.y;
    a2.y = a.z;
    Vec2 b2;
    b2.x = b.y;
    b2.y = b.z;
    return getSignedDiffAngleToDir(a2, b2);
}

// ELF 0x36B030; engine ELF 0x83FA400.
void addVec2ToPos3XZ(const Vec2& in, Vec3& out)
{
    out.x = in.x + out.x;
    out.z = in.y + out.z;
}

// ELF 0x36B050; engine ELF 0x83FA420.
void subVec2ToPos3XZ(const Vec2& in, Vec3& out)
{
    out.x = out.x - in.x;
    out.z = out.z - in.y;
}

// ELF 0x36B070; engine ELF 0x83FA440.
void getVectorToXZ(const Vec3& from, const Vec3& to, Vec2& out)
{
    out.x = to.x - from.x;
    out.y = to.z - from.z;
}

// ELF 0x36B090; engine ELF 0x83FA460.
void getVectorToXY(const Vec3& from, const Vec3& to, Vec2& out)
{
    out.x = to.x - from.x;
    out.y = to.y - from.y;
}

// ELF 0x36B0B0; engine ELF 0x83FA480.
void getVectorToYZ(const Vec3& from, const Vec3& to, Vec2& out)
{
    out.x = to.y - from.y;
    out.y = to.z - from.z;
}

// ELF 0x36B0D0; engine ELF 0x83FA4A0.
void flipVec3XZ(Vec3& value)
{
    const float x = value.x;
    const float z = value.z;
    value.z = x;
    value.x = z;
}

// ELF 0x36BEC0; engine ELF 0x83FB190.
float distXZ(const Vec3& a, const Vec3& b)
{
    return std::sqrt(distXZSqr(a, b));
}

// ELF 0x36BF40; engine ELF 0x83FB200.
float distXY(const Vec3& a, const Vec3& b)
{
    return std::sqrt(distXYSqr(a, b));
}

// ELF 0x36BF90; engine ELF 0x83FB240.
float distXYSqr(const Vec3& a, const Vec3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return x * x + y * y;
}

// ELF 0x36BFC0; engine ELF 0x83FB270.
float distYZ(const Vec3& a, const Vec3& b)
{
    return std::sqrt(distYZSqr(a, b));
}

// ELF 0x36C010; engine ELF 0x83FB2B0.
float distYZSqr(const Vec3& a, const Vec3& b)
{
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return y * y + z * z;
}

// ELF 0x36C040; engine ELF 0x83FB2E0.
float magnitudeXY(const Vec3& value)
{
    return std::sqrt(magnitudeXYSqr(value));
}

// ELF 0x36C080; engine ELF 0x83FB310.
float magnitudeXYSqr(const Vec3& value)
{
    return value.x * value.x + value.y * value.y;
}

// ELF 0x36C0A0; engine ELF 0x83FB330.
float magnitudeXZ(const Vec3& value)
{
    return std::sqrt(magnitudeXZSqr(value));
}

// ELF 0x36C0E0; engine ELF 0x83FB360.
float magnitudeXZSqr(const Vec3& value)
{
    return value.x * value.x + value.z * value.z;
}

// ELF 0x36C100; engine ELF 0x83FB380.
float magnitudeYZ(const Vec3& value)
{
    return std::sqrt(magnitudeYZSqr(value));
}

// ELF 0x36C140; engine ELF 0x83FB3B0.
float magnitudeYZSqr(const Vec3& value)
{
    return value.y * value.y + value.z * value.z;
}

// ELF 0x36C160; engine ELF 0x83FB3D0.
void rotateXY(const Vec3& in, Vec3& out, float radians)
{
    out.z = 0.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out.x = in.x * c - in.y * s;
    out.y = s * in.x + c * in.y;
}

// ELF 0x36C260; engine ELF 0x83FB4B0.
void rotateYZ(const Vec3& in, Vec3& out, float radians)
{
    out.x = 0.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out.y = in.y * c - in.z * s;
    out.z = s * in.y + c * in.z;
}

namespace {

// [INFERRED] Packaging for Vec2's inlined component access; both components are attested floats.
float& boxComponent(Vec2& value, int axis)
{
    return *reinterpret_cast<float*>(reinterpret_cast<unsigned char*>(&value) + axis * sizeof(float));
}

const float& boxComponent(const Vec2& value, int axis)
{
    return *reinterpret_cast<const float*>(reinterpret_cast<const unsigned char*>(&value) + axis * sizeof(float));
}

// [INFERRED] Packaging of the inlined Vec3 normalization, dot product and subtraction.
void normaliseGeometry(Vec3& value)
{
    if (!Math::normaliseIfNeeded(value))
        Math::zero(value);
}

float geometryDot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 geometryDifference(const Vec3& to, const Vec3& from)
{
    Vec3 result = { to.x - from.x, to.y - from.y, to.z - from.z };
    return result;
}

} // anonymous namespace

// ELF 0x366DF0; engine ELF 0x83F7B50.
// [INFERRED] int return follows the full-width zero/one materialization in both images.
int getLineIntersections(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d,
                          Vec2& outFirst, Vec2& outSecond, bool& overlap, int firstAxis, int secondAxis)
{
    if (firstAxis != secondAxis)
    {
        const float coordinate = boxComponent(a, secondAxis);
        if (!(coordinate >= boxComponent(c, secondAxis)))
            return false;
        if (!(boxComponent(d, secondAxis) >= coordinate))
            return false;
        const float other = boxComponent(c, firstAxis);
        if (!(other >= boxComponent(a, firstAxis)))
            return false;
        if (!(boxComponent(b, firstAxis) >= other))
            return false;
        boxComponent(outFirst, firstAxis) = other;
        boxComponent(outFirst, secondAxis) = boxComponent(a, secondAxis);
        overlap = false;
        return true;
    }

    const int otherAxis = (firstAxis + 1) % 2;
    if (boxComponent(d, otherAxis) != boxComponent(c, otherAxis))
        return false;
    const float firstMinimum = boxComponent(a, firstAxis);
    const float secondMaximum = boxComponent(d, firstAxis);
    if (!(secondMaximum > firstMinimum))
        return false;
    const float secondMinimum = boxComponent(c, firstAxis);
    const float firstMaximum = boxComponent(b, firstAxis);
    if (secondMinimum > firstMaximum)
        return false;

    const Vec2& left = !(secondMinimum > firstMinimum) ? a : c;
    const Vec2& right = !(secondMaximum > firstMaximum) ? d : b;
    const float leftX = left.x;
    const float leftY = left.y;
    outFirst.y = leftY;
    outFirst.x = leftX;
    const float rightX = right.x;
    const float rightY = right.y;
    outSecond.x = rightX;
    outSecond.y = rightY;
    overlap = true;
    return true;
}

// ELF 0x36BB10; engine ELF 0x83FAE30.
bool allPointsOnSameLine(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
{
    Vec3 first = geometryDifference(b, a);
    normaliseGeometry(first);
    Vec3 current = geometryDifference(c, a);
    normaliseGeometry(current);
    for (int i = 0; i < 2; ++i)
    {
        const float error = std::fabs(geometryDot(first, current)) - 1.0f;
        if (geometryDot(first, first) > 0.001f && geometryDot(current, current) > 0.001f
            && std::fabs(error) > 0.001f)
            return false;
        // The final iteration repeats this construction and normalization before returning.
        current = geometryDifference(d, a);
        normaliseGeometry(current);
    }
    return true;
}


} // namespace hfe
} // namespace dice
