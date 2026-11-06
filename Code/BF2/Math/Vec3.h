// Vector types and arithmetic. [INFERRED] Source placement.

#ifndef MATH_VEC3_H     // [INFERRED] guard style
#define MATH_VEC3_H

#include <iosfwd>

namespace dice {
namespace hfe {
namespace Math {

struct Vec3
{
    float x, y, z;

    // Returns a reference to the shared zero vector.
    static const Vec3& getZero();
    static const Vec3& getIdentity();


    // Builds an orthonormal frame in place: this receives the third axis, and forward/up are rewritten.
    // Returns zero or -20003 for a degenerate vector. A later degenerate up vector is zeroed without failure.
    int makeOrthonormalBasis(Vec3& forward, Vec3& up);
};


// Parses three components separated by any single non-whitespace character; prints x/y/z.
// [CHOSEN] Stream operators initialize failed component reads to zero rather than retaining undefined stack data.

std::istream& operator>>(std::istream& in, Vec3& v);
std::ostream& operator<<(std::ostream& out, const Vec3& v);

// [CHOSEN] Stream operators for slash-separated Vec2 coordinates.
struct Vec2;
std::istream& operator>>(std::istream& in, Vec2& v);
std::ostream& operator<<(std::ostream& out, const Vec2& v);

// Parses or prints slash-separated Vec4 components. [CHOSEN] Header packaging.
struct Vec4;
std::istream& operator>>(std::istream& in, Vec4& v);
std::ostream& operator<<(std::ostream& out, const Vec4& v);

// Four-component float vector. [CHOSEN, not recovered] Field names and component representation.
struct Vec4
{
    float x, y, z, w;
};

struct Vec2
{
    float x, y;
};

// Colour plus intensity in one token group - four numbers, not three. Console metadata declares
// this type for `color`, `speccolor` and the sun colour properties.
struct Vec3ColorFloatIntensity
{
    float r, g, b;
    float intensity;
};

} // namespace Math

// The console API and several struct layouts use the unqualified spelling for the same types.
typedef Math::Vec3 Vec3;
typedef Math::Vec2 Vec2;
typedef Math::Vec4 Vec4;


// Projects between Vec3 and the XZ plane through output parameters. Y is discarded or restored.
// [INFERRED] Void return contracts; preserve the output-parameter signatures.

void vec3ToVec2XZ(const Vec3& in, Vec2& out);
void vec2ToVec3XZ(const Vec2& in, Vec3& out);
// The XY and YZ planes the same way: libaidll 0x36AE40 / 0x36AE60 / 0x36AEE0 / 0x36AF00
// (bf2 0x83FA260 / 0x83FA280 / 0x83FA2E0 / 0x83FA300). The restoring direction clears the
// dropped component.
void vec3ToVec2XY(const Vec3& in, Vec2& out);
void vec2ToVec3XY(const Vec2& in, Vec3& out);
void vec3ToVec2YZ(const Vec3& in, Vec2& out);
void vec2ToVec3YZ(const Vec2& in, Vec3& out);
// libaidll 0x36AFB0 / bf2 0x83FA380: out.x += in.x, out.y += in.z.
void addVec3ToVec2XZ(const Vec3& in, Vec2& out);
// libaidll 0x36AFD0: out.x -= in.x, out.y -= in.z.
void subVec3ToVec2XZ(const Vec3& in, Vec2& out);
// libaidll 0x36AFF0 / bf2 0x83FA3C0: out.x += in.x, out.z += in.y; y untouched.
void addVec2ToVec3XZ(const Vec2& in, Vec3& out);
// libaidll 0x36B010 / bf2 0x83FA3E0: out.x -= in.x, out.z -= in.y; y untouched.
void subVec2ToVec3XZ(const Vec2& in, Vec3& out);
// The position variants update the same XZ components and leave height untouched.
void addVec2ToPos3XZ(const Vec2& in, Vec3& out);
void subVec2ToPos3XZ(const Vec2& in, Vec3& out);
// Writes the indicated projection of to - from; inputs and output may alias.
void getVectorToXZ(const Vec3& from, const Vec3& to, Vec2& out);
void getVectorToXY(const Vec3& from, const Vec3& to, Vec2& out);
void getVectorToYZ(const Vec3& from, const Vec3& to, Vec2& out);
void flipVec3XZ(Vec3& value);
// Rotates a projected vector, clearing the omitted component before reading the input.
// In-place rotation retains the shipped sequential-write behavior.
void rotateXY(const Vec3& in, Vec3& out, float radians);
void rotateYZ(const Vec3& in, Vec3& out, float radians);
// ELF libaidll 0x36C1E0 (added by ai-bot): `in` turned by `radians` about Y into `out` (y = 0).
void rotateXZ(const Vec3& in, Vec3& out, float radians);
// Squared distance in the XZ plane; ignores y. The x87 image can retain excess precision.
float distXZSqr(const Vec3& a, const Vec3& b);
float distXZ(const Vec3& a, const Vec3& b);
float distXY(const Vec3& a, const Vec3& b);
float distXYSqr(const Vec3& a, const Vec3& b);
float distYZ(const Vec3& a, const Vec3& b);
float distYZSqr(const Vec3& a, const Vec3& b);
float magnitudeXY(const Vec3& value);
float magnitudeXYSqr(const Vec3& value);
float magnitudeXZ(const Vec3& value);
float magnitudeXZSqr(const Vec3& value);
float magnitudeYZ(const Vec3& value);
float magnitudeYZSqr(const Vec3& value);
// The left-hand normal of a 2D vector: (y, -x). Reads both components before writing.
void getNormal(const Vec2& in, Vec2& out);
// The XZ projection's left-hand normal in one step: (z, -x). libaidll.so 0x36AF70.
void getVec2NormalFromVec3XZ(const Vec3& in, Vec2& out);
// Intersects the infinite lines a-b and c-d. False only when they are parallel (zero
// denominator); otherwise t and s are the parameters along a-b and c-d.
bool lineIntersect(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d, float& t, float& s);
// Finds the point on a-b selected by the cross-product construction for the two infinite lines.
// Near-parallel normalized directions are refused; failure leaves output untouched.
bool lineIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, Vec3& output);
// Tests whether the point lies along a-b, with the shipped direction and distance tolerances.
bool linePointOnSegment(const Vec3& point, const Vec3& a, const Vec3& b);
// Intersects segments using the shipped unchecked infinite-line result. The output overload
// retains its previous output when that calculation refuses the lines.
bool lineSegmentIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d);
bool lineSegmentIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, Vec3& output);
// True when c and d lie on the line through a and b (the shipped direction tolerance).
bool allPointsOnSameLine(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d);
// True when `point` (XZ) lies on the inner side of every edge of the polygon (XZ), i.e. no
// edge normal (getNormal of b - a) has a negative dot product with point - a. libaidll.so
// 0x36C650 (Vec3) and 0x36C790 (Vec2).
bool pointWithinPolygon2d(const Vec3& point, Vec3* polygon, int count);
bool pointWithinPolygon2d(const Vec2& point, Vec2* polygon, int count);

// Computes (centre - minimum) + centre, storing x before reading y.
// Output may alias either input. [INFERRED] Reference return.
Vec2& getBoxMaxCorner(const Vec2& minimum, const Vec2& centre, Vec2& output);
// AI image 0x366CA0: point within [minimum, getBoxMaxCorner(minimum, centre)], bounds inclusive.
bool insideCentreBox(const Vec2& point, const Vec2& minimum, const Vec2& centre);
// libaidll.so 0x366D30: point within [minimum, maximum], bounds inclusive; NaN does not reject.
bool insideCornerBox(const Vec2& point, const Vec2& minimum, const Vec2& maximum);
// libaidll.so 0x366DB0: the centre of the box minimum..maximum. Note the argument order.
void getBoxCentre(const Vec2& minimum, Vec2& centre, const Vec2& maximum);
// Intersects ordered axis-aligned line segments. Axis indices must be 0 or 1.
// Writes the first point and sets overlap false for a crossing; a collinear overlap writes both.
// Failure leaves the outputs untouched.
int getLineIntersections(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d,
                          Vec2& outFirst, Vec2& outSecond, bool& overlap, int firstAxis, int secondAxis);
// Finds the shipped box-overlap segment. May collapse the other axis and return early;
// a false result can leave the output X coordinates already written.
bool getBoxIntersections(const Vec2& firstMinimum, const Vec2& firstMaximum,
                         const Vec2& secondMinimum, const Vec2& secondMaximum,
                         Vec2& outMinimum, Vec2& outMaximum);
// libaidll.so 0x3671E0: where the line through `point` along (normal.y, -normal.x) meets an edge
// of the box minimum..maximum (y edges first). A flat box yields `point`; `out` is left untouched
// when no edge qualifies. Not the same body as ai::getIntersection (ControlFunctions).
void getIntersection(const Vec2& minimum, const Vec2& maximum, const Vec2& normal,
                     const Vec2& point, Vec2& out);
// libaidll.so 0x36C470: signed angle (radians, -pi..pi) from direction a to direction b, both unit.
float getSignedDiffAngleToDir(const Vec2& a, const Vec2& b);
// libaidll.so 0x36C570 (bf2 0x83FB750): the same angle with the caller's left-hand normal of `a`
// (getNormal); the clamp passes NaN through.
float getSignedDiffAngleToDirWithNormal(const Vec2& a, const Vec2& normal, const Vec2& b);
// libaidll.so 0x36C3B0 / 0x36C410: the same for the XZ and YZ projections of two Vec3.
float getSignedDiffAngleToDirXZ(const Vec3& a, const Vec3& b);
float getSignedDiffAngleToDirYZ(const Vec3& a, const Vec3& b);

} // namespace hfe
} // namespace dice

#endif // MATH_VEC3_H
