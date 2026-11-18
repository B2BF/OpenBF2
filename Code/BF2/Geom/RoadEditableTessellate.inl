// Road spline sampling, terrain-grid clipping, triangulation and texture interpolation.
// [CHOSEN, not recovered] included split of RoadEditable.cpp; never compile it separately.
// Preserve operand association and the observed float/double stores.

namespace dice {
namespace hfe {
namespace geom {

namespace {

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] Keep the x87 register values until the observed stores on native hosts.
typedef bf2x87::Real RoadScalar;
typedef bf2x87::Real RoadDouble;
#else
typedef float RoadScalar;
typedef double RoadDouble;
#endif

template <typename T, typename U>
bool roadLess(T a, U b)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] These sites use FCOM, including its invalid flag for quiet NaNs.
    return bf2x87::less(bf2x87::wideExact(a), bf2x87::wideExact(b));
#else
    return a < b;
#endif
}

template <typename T, typename U>
bool roadLessEqual(T a, U b)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] FCOM equality/range tests through the native arithmetic backend.
    return bf2x87::lessEqual(bf2x87::wideExact(a), bf2x87::wideExact(b));
#else
    return a <= b;
#endif
}

RoadScalar roadSquareRoot(RoadScalar value)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] The shared native FSQRT policy.
    return bf2x87::squareRoot(value);
#else
    return std::sqrt(value);
#endif
}

// Small vector arithmetic, in the shipped operand order.

Vec3 makeVec3(float x, float y, float z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

// The three association orders the shipped code adds a vector's squares in. Which one a site uses is
// read off its x87 sequence; each call site below names the one it was read with.
// sqrt((z*z + y*y) + x*x)
float lengthSumZYX(float x, float y, float z) { return std::sqrt((z * z + y * y) + x * x); }
// sqrt((x*x + z*z) + y*y)
float lengthSumXZY(float x, float y, float z) { return std::sqrt((x * x + z * z) + y * y); }
// sqrt((y*y + x*x) + z*z)
float lengthSumYXZ(float x, float y, float z) { return std::sqrt((y * y + x * x) + z * z); }
// sqrt(x*x + z*z) - two terms, so the order the shipped code adds them in does not matter.
float lengthXZ(float x, float z) { return std::sqrt(x * x + z * z); }

// (thiscall on `out`, `retn 0Ch`) - out = a, then each component (b - a) * t + a.
// PE 0x10106AE0
void lerpVec3(Vec3& out, const Vec3& a, const Vec3& b, float t)
{
    const Vec3 start = a;
    out = start;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Preserve the asymmetric FSTP spills before the three additions.
    const RoadScalar dx = RoadScalar(b.x) - out.x;
    const RoadScalar dy = RoadScalar(b.y) - start.y;
    const float dz = bf2x87::store(RoadScalar(b.z) - start.z);
    const float scaledX = bf2x87::store(dx * t);
    const RoadScalar scaledY = dy * t;
    const float scaledZ = bf2x87::store(RoadScalar(dz) * t);
    out.x = bf2x87::store(RoadScalar(scaledX) + out.x);
    out.y = bf2x87::store(scaledY + start.y);
    out.z = bf2x87::store(RoadScalar(scaledZ) + start.z);
#else
    out.x = (b.x - start.x) * t + start.x;
    out.y = (b.y - start.y) * t + start.y;
    out.z = (b.z - start.z) * t + start.z;
#endif
}

} // anonymous namespace


// RoadEditableVertex.

RoadEditableVertex::RoadEditableVertex()
    : needsUv(1)
    , alpha(1.0f)
{
    position = makeVec3(0.0f, 0.0f, 0.0f);
    normal = makeVec3(0.0f, 0.0f, 0.0f);
    uv0.x = 0.0f;
    uv0.y = 0.0f;
    uv1.x = 0.0f;
    uv1.y = 0.0f;
    pad29[0] = pad29[1] = pad29[2] = 0;                                      // the inline zero-fill writes
    unknown2C[0] = unknown2C[1] = unknown2C[2] = unknown2C[3] = 0.0f;        // the flag as a dword
}

// The tessellator's sample vertex (0x1013DBC4): position and normal from the spline, the rest as the
// default constructor leaves it.
RoadEditableVertex::RoadEditableVertex(const Vec3& samplePosition, const Vec3& sampleNormal)
    : needsUv(1)
    , alpha(1.0f)
{
    position = samplePosition;
    normal = sampleNormal;
    uv0.x = 0.0f;
    uv0.y = 0.0f;
    uv1.x = 0.0f;
    uv1.y = 0.0f;
    pad29[0] = pad29[1] = pad29[2] = 0;
    unknown2C[0] = unknown2C[1] = unknown2C[2] = unknown2C[3] = 0.0f;
}

// Interpolates position and normal, then the two texture layers and alpha.
// PE 0x1013B670
void RoadEditableVertex::interpolate(const RoadEditableVertex& a, const RoadEditableVertex& b, float t)
{
    lerpVec3(position, a.position, b.position, t);
    lerpVec3(normal, a.normal, b.normal, t);
    interpolateTexCoords(a, b, t);
}

// Blends the two texture layers and alpha, preserving the layer-specific float stores.
// PE 0x1013B760
void RoadEditableVertex::interpolateTexCoords(const RoadEditableVertex& a, const RoadEditableVertex& b,
                                              float t)
{
    uv0 = a.uv0;
    const RoadScalar w = RoadScalar(1.0f) - t;
    const RoadScalar leftU0 = w * uv0.x;
    uv0.x = static_cast<float>(leftU0);
    const float leftV0 = static_cast<float>(w * uv0.y);
    uv0.y = leftV0;
    const RoadScalar rightU0 = RoadScalar(t) * b.uv0.x;
    const float rightV0 = static_cast<float>(RoadScalar(t) * b.uv0.y);
    uv0.x = static_cast<float>(leftU0 + rightU0);
    uv0.y = static_cast<float>(RoadScalar(leftV0) + rightV0);

    uv1 = a.uv1;
    const float leftU1 = static_cast<float>(w * uv1.x);
    uv1.x = leftU1;
    const RoadScalar leftV1 = w * uv1.y;
    uv1.y = static_cast<float>(leftV1);
    const RoadScalar rightU1 = RoadScalar(t) * b.uv1.x;
    const float rightV1 = static_cast<float>(RoadScalar(t) * b.uv1.y);
    uv1.x = static_cast<float>(RoadScalar(leftU1) + rightU1);
    uv1.y = static_cast<float>(leftV1 + rightV1);
    alpha = static_cast<float>((RoadScalar(b.alpha) - a.alpha) * t + a.alpha);
}

namespace {


// The 2D tests. Every one reads x and z only.


// v = (c.z - a.z)(b.x - a.x) - (b.z - a.z)(c.x - a.x); 1 above 0.5, -1 below -0.5 (both
// double constants), else 0 - a NaN answers 0.
// PE 0x1013B370
int areaSign(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const RoadScalar v = (RoadScalar(c.z) - a.z) * (RoadScalar(b.x) - a.x)
                       - (RoadScalar(b.z) - a.z) * (RoadScalar(c.x) - a.x);
    if (roadLess(0.5, v))                                                     // dbl_1020D7D0
        return 1;
    if (roadLess(v, -0.5))                                                    // dbl_1021D798
        return -1;
    return 0;
}

// (`retn 18h`) - segment a-b against c-d. |denominator| below FLT_EPSILON answers false
// without writing s or t; otherwise both are written, and the answer is whether both lie strictly
// inside (0, 1) - a NaN passes every range test.
// PE 0x1013B3D0
bool segmentsIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float* s, float* t)
{
    const RoadScalar denominator = (RoadScalar(d.z) - c.z) * (RoadScalar(b.x) - a.x)
                                 - (RoadScalar(d.x) - c.x) * (RoadScalar(b.z) - a.z);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const RoadScalar magnitude = bf2x87::abs(denominator);
#else
    const RoadScalar magnitude = std::fabs(denominator);
#endif
    if (roadLess(magnitude, FLT_EPSILON))                                    // flt_101E05A0
        return false;
    const RoadScalar inverse = RoadScalar(1.0f) / denominator;
    *s = static_cast<float>(((RoadScalar(a.z) - c.z) * (RoadScalar(d.x) - c.x)
                           - (RoadScalar(d.z) - c.z) * (RoadScalar(a.x) - c.x)) * inverse);
    const RoadScalar tValue = ((RoadScalar(a.z) - c.z) * (RoadScalar(b.x) - a.x)
                             - (RoadScalar(b.z) - a.z) * (RoadScalar(a.x) - c.x)) * inverse;
    *t = static_cast<float>(tValue);
    if (roadLessEqual(tValue, 0.0f) || roadLessEqual(1.0f, tValue))
        return false;
    if (roadLessEqual(*s, 0.0f) || roadLessEqual(1.0f, *s))
        return false;
    return true;
}

// is c on segment a-b? Along x unless a.x == b.x (then along z); inclusive both ways.
// PE 0x1013BBD0
int between(const Vec3& a, const Vec3& b, const Vec3& c)
{
    if (a.x != b.x)
        return (a.x <= c.x && c.x <= b.x) || (a.x >= c.x && c.x >= b.x);
    return (a.z <= c.z && c.z <= b.z) || (a.z >= c.z && c.z >= b.z);
}

// (`retn 14h`) - the collinear case of segmentIntersection: '0' unless c is on the line
// a-b; then the first of c, d (on a-b) or a, b (on c-d) that lies on the other segment, copied to p,
// with 'e'.
// PE 0x1013BD10
char parallelIntersection(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, Vec3& p)
{
    if (areaSign(a, b, c) != 0)
        return '0';
    if (between(a, b, c))
    {
        p = c;
        return 'e';
    }
    if (between(a, b, d))
    {
        p = d;
        return 'e';
    }
    if (between(c, d, a))
    {
        p = a;
        return 'e';
    }
    if (between(c, d, b))
    {
        p = b;
        return 'e';
    }
    return '0';
}

// (`retn 1Ch`) - O'Rourke's SegSegInt on x/z: '1' proper crossing, 'v' a vertex of one
// segment on the other, '0' none, 'e' collinear overlap, '?' none of those decided. s (along a-b)
// and t (along c-d) are stored as doubles; the denominator and inverse also have qword spills.
// The first product uses the retained inverse, and the second reloads its double spill.
// The point is lerp(a, b, float(s)).
// The collinear arm returns from parallelIntersection without writing s or t (shipped).
// PE 0x1013C420
char segmentIntersection(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, Vec3& p,
                         double& s, double& t)
{
    char code = '?';
    const RoadDouble denominator = (((RoadDouble(a.z) - b.z) * c.x + (RoadDouble(c.z) - d.z) * b.x)
                                   + (RoadDouble(b.z) - a.z) * d.x) + (RoadDouble(d.z) - c.z) * a.x;
    const double storedDenominator = static_cast<double>(denominator);
    if (denominator == 0.0)
        return parallelIntersection(a, b, c, d, p);

    const RoadDouble numerator = ((RoadDouble(c.z) - a.z) * d.x + (RoadDouble(a.z) - d.z) * c.x)
                               + (RoadDouble(d.z) - c.z) * a.x;
    if (numerator == 0.0 || numerator == denominator)
        code = 'v';
    const RoadDouble inverse = RoadDouble(1.0) / denominator;
    const double storedInverse = static_cast<double>(inverse);
    s = static_cast<double>(inverse * numerator);

    const RoadDouble numerator2 = -(((RoadDouble(b.z) - a.z) * c.x + (RoadDouble(a.z) - c.z) * b.x)
                                  + (RoadDouble(c.z) - b.z) * a.x);
    if (numerator2 == 0.0 || numerator2 == storedDenominator)
        code = 'v';
    const RoadDouble tValue = numerator2 * storedInverse;
    t = static_cast<double>(tValue);

    if (roadLess(0.0, s) && roadLess(s, 1.0) && roadLess(0.0, tValue) && roadLess(tValue, 1.0))
        code = '1';
    else if (roadLess(s, 0.0) || roadLess(1.0, s) || roadLess(tValue, 0.0) || roadLess(1.0, tValue))
        code = '0';

    lerpVec3(p, a, b, static_cast<float>(s));
    return code;
}

// v = (c.x - a.x)(b.z - a.z) - (c.z - a.z)(b.x - a.x); false only when v < 0 (a NaN is
// "left").
// PE 0x101406E0
bool leftOn(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const RoadScalar v = (RoadScalar(c.x) - a.x) * (RoadScalar(b.z) - a.z)
                       - (RoadScalar(c.z) - a.z) * (RoadScalar(b.x) - a.x);
    return !roadLess(v, 0.0f);
}

// the two edge tests a-b and b-c, then leftOn(c, a, p).
// PE 0x10140730
bool pointInTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& p)
{
    if (roadLess((RoadScalar(b.z) - a.z) * (RoadScalar(p.x) - a.x)
                 - (RoadScalar(p.z) - a.z) * (RoadScalar(b.x) - a.x), 0.0f))
        return false;
    if (roadLess((RoadScalar(c.z) - b.z) * (RoadScalar(p.x) - b.x)
                 - (RoadScalar(p.z) - b.z) * (RoadScalar(c.x) - b.x), 0.0f))
        return false;
    return leftOn(c, a, p);
}

// the leftOn value's sign into `side`: -1 below 0, 1 above, 0 otherwise (NaN too).
// PE 0x101407D0
void orient(const Vec3& a, const Vec3& b, const Vec3& c, int& side)
{
    const RoadScalar v = (RoadScalar(c.x) - a.x) * (RoadScalar(b.z) - a.z)
                       - (RoadScalar(c.z) - a.z) * (RoadScalar(b.x) - a.x);
    if (roadLess(v, 0.0f))
        side = -1;
    else if (roadLess(0.0f, v))
        side = 1;
    else
        side = 0;
}


// Interpolates target texture coordinates in the x/z triangle; returns false outside it.
// Out-of-range ratios repeat the edge calculation but still use the original result.
// Shipped defect: a parallel edge leaves the interpolation parameter unwritten.
// PE 0x10140BD0
bool interpolateInTriangle(const RoadEditableVertex& a, const RoadEditableVertex& b,
                           const RoadEditableVertex& c, RoadEditableVertex& target)
{
    const RoadScalar winding = ((RoadScalar(a.position.z) * c.position.x - RoadScalar(a.position.x) * c.position.z)
                               + (RoadScalar(a.position.x) * b.position.z - RoadScalar(a.position.z) * b.position.x))
                              + (RoadScalar(b.position.x) * c.position.z - RoadScalar(c.position.x) * b.position.z);
    RoadEditableVertex p0;
    RoadEditableVertex p1;
    RoadEditableVertex p2;
    p0 = a;
    if (roadLess(0.0f, winding))                     // `test ah, 41h; jnz` - zero and NaN take b, c
    {
        p1 = c;
        p2 = b;
    }
    else
    {
        p1 = b;
        p2 = c;
    }
    if (!pointInTriangle(p0.position, p1.position, p2.position, target.position))
        return false;

    Vec3 point = makeVec3(0.0f, 0.0f, 0.0f);
    double s;
    double t;
    segmentIntersection(p2.position, p1.position, p0.position, target.position, point, s, t);
    RoadEditableVertex edge;
    edge.interpolate(p2, p1, static_cast<float>(s));
    const RoadScalar targetX = RoadScalar(target.position.x) - p0.position.x;
    const RoadScalar targetZ = RoadScalar(target.position.z) - p0.position.z;
    const float edgeX = edge.position.x - p0.position.x;
    const float edgeZ = edge.position.z - p0.position.z;
    const RoadScalar quotient = roadSquareRoot(targetZ * targetZ + targetX * targetX)
                              / roadSquareRoot(RoadScalar(edgeZ) * edgeZ + RoadScalar(edgeX) * edgeX);
    const float ratio = static_cast<float>(quotient);
    if (roadLess(quotient, 0.0) || roadLess(1.0, ratio))
    {
        point = makeVec3(0.0f, 0.0f, 0.0f);
        segmentIntersection(p2.position, p1.position, p0.position, target.position, point, s, t);
        RoadEditableVertex unused;                   // written, never read
        unused.interpolate(p2, p1, static_cast<float>(s));
    }
    target.interpolateTexCoords(p0, edge, ratio);
    return true;
}


// Ear clipping.


// the shoelace sum over the polygon's x/z, (z_i * x_prev - z_prev * x_i) added in order
// (the shipped loop is unrolled by four; the additions stay sequential), times 0.5f.
// PE 0x10141560
RoadScalar polygonArea(const RoadPolygon& polygon, const std::vector<RoadEditableVertex>& vertices)
{
    const unsigned int count = static_cast<unsigned int>(polygon.size());
    RoadScalar area = RoadScalar(0.0f);
    unsigned int previous = count - 1;
    for (unsigned int i = 0; i < count; ++i)
    {
        const Vec3& current = vertices[polygon[i]].position;
        const Vec3& before = vertices[polygon[previous]].position;
        area = area + (RoadScalar(current.z) * before.x - RoadScalar(before.z) * current.x);
        previous = i;
    }
    return area * 0.5f;                                                      // flt_101DD4F0
}

// (`retn 8`) - a positive area (NaN and zero are left alone) reverses the polygon: a copy,
// clear() (VC7.1: the block freed), then the copy pushed back from its end.
// PE 0x10142C00
void ensureClockwise(RoadPolygon& polygon, const std::vector<RoadEditableVertex>& vertices)
{
    if (!roadLess(0.0f, polygonArea(polygon, vertices)))
        return;
    const RoadPolygon copy(polygon);
    polygon.clear();
    for (int i = static_cast<int>(copy.size()) - 1; i >= 0; --i)
        polygon.push_back(copy[static_cast<unsigned int>(i)]);
}

// (`retn 14h`) - a, b, c are vertex indices. Not an ear when the corner turns the wrong
// way ((b.z - a.z)(c.x - a.x) - (c.z - a.z)(b.x - a.x) < 0) or when any other live polygon vertex
// (not -1, not a, b or c) lies in the triangle.
// PE 0x1013C630
bool isEar(const RoadPolygon& polygon, int a, int b, int c, const std::vector<RoadEditableVertex>& vertices)
{
    const Vec3& pa = vertices[a].position;
    const Vec3& pb = vertices[b].position;
    const Vec3& pc = vertices[c].position;
    if (roadLess((RoadScalar(pb.z) - pa.z) * (RoadScalar(pc.x) - pa.x)
                 - (RoadScalar(pc.z) - pa.z) * (RoadScalar(pb.x) - pa.x), 0.0f))
        return false;
    for (unsigned int i = 0; i < polygon.size(); ++i)
    {
        const int index = polygon[i];
        if (index == -1 || index == a || index == b || index == c)
            continue;
        if (pointInTriangle(pa, pb, pc, vertices[index].position))
            return false;
    }
    return true;
}

// (`retn 14h`) - r = (b.x - c.x)(a.z - c.z) - (b.z - c.z)(a.x - c.x): positive pushes
// c, a, b; otherwise (NaN too) b, a, c.
// PE 0x1013C710
void emitTriangle(int a, int b, int c, const std::vector<RoadEditableVertex>& vertices,
                  std::vector<int>& indices)
{
    const Vec3& pa = vertices[a].position;
    const Vec3& pb = vertices[b].position;
    const Vec3& pc = vertices[c].position;
    const RoadScalar r = (RoadScalar(pb.x) - pc.x) * (RoadScalar(pa.z) - pc.z)
                       - (RoadScalar(pb.z) - pc.z) * (RoadScalar(pa.x) - pc.x);
    if (roadLess(0.0f, r))
    {
        indices.push_back(c);
        indices.push_back(a);
        indices.push_back(b);
    }
    else
    {
        indices.push_back(b);
        indices.push_back(a);
        indices.push_back(c);
    }
}

// (`retn 0Ch`) - ear clipping in place: a clipped corner is overwritten with -1.
//   * fewer than three entries logs line 501 and returns; exactly three is one triangle;
//   * otherwise the polygon is made clockwise, then each pass takes, for the first live entry i in
//     turn, i and the next two live entries (circularly, unsigned %); the first ear is emitted and its
//     middle entry removed; a pass that finds no ear emits the last triple it tried and removes its
//     middle anyway. One entry goes per pass until two remain.
// PE 0x1013C7C0
void earClip(RoadPolygon& polygon, const std::vector<RoadEditableVertex>& vertices, std::vector<int>& indices)
{
    if (polygon.size() < 3)
    {
        const std::string expression("");
        const std::string file("C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\RoadEditable.cpp");
        ::renddx9abi::Debug(DebugTypeError, file, 501, expression)
            << "TBM: npoly with less then 3 vertex!";
        return;
    }
    if (polygon.size() == 3)
    {
        emitTriangle(polygon[0], polygon[1], polygon[2], vertices, indices);
        return;
    }
    ensureClockwise(polygon, vertices);
    if (polygon.empty())
        return;
    unsigned int remaining = static_cast<unsigned int>(polygon.size());
    if (remaining <= 2)
        return;

    unsigned int candidate[3];                       // set by every pass while three live entries remain
    do
    {
        const unsigned int count = static_cast<unsigned int>(polygon.size());
        unsigned int i;
        for (i = 0; i < polygon.size(); ++i)
        {
            if (polygon[i] == -1)
                continue;
            candidate[0] = i;
            unsigned int found = 1;
            unsigned int next = i + 1;
            do
            {
                const unsigned int k = next % count;
                ++next;
                if (polygon[k] != -1)
                    candidate[found++] = k;
            }
            while (found < 3);
            if (isEar(polygon, polygon[candidate[0]], polygon[candidate[1]], polygon[candidate[2]],
                      vertices))
            {
                emitTriangle(polygon[candidate[0]], polygon[candidate[1]], polygon[candidate[2]],
                             vertices, indices);
                polygon[candidate[1]] = -1;
                break;
            }
        }
        if (i == polygon.size())
        {
            emitTriangle(polygon[candidate[0]], polygon[candidate[1]], polygon[candidate[2]],
                         vertices, indices);
            polygon[candidate[1]] = -1;
        }
        --remaining;
    }
    while (remaining > 2);
}


// Returns a tolerance-adjusted polygon extremum on the even grid.
// Requires a nonempty polygon. Minima round down; maxima round up. Ties and NaN replace
// minima, while maxima change only on a strict increase.
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] Native equivalent of the qword libm argument, FISTP and 32-bit wrap.
int roadGridRound(float value, float tolerance, bool maximum)
{
    const RoadScalar adjusted = maximum ? RoadScalar(value) - tolerance : RoadScalar(value) + tolerance;
    const double input = bf2x87::storeDouble(adjusted);
    const int n = bf2x87::toIntTruncate(RoadScalar(maximum ? std::ceil(input) : std::floor(input)));
    const int odd = static_cast<unsigned int>(n) & 1u;
    return bf2x87::lowWord(static_cast<long long>(n) - odd + (maximum && odd ? 2 : 0));
}
#endif

int roadGridStep(int value, int step)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Preserve the ADD/SUB wrap on the native compiler.
    return bf2x87::lowWord(static_cast<long long>(value) + step);
#else
    return value + step;
#endif
}

float roadGridOffset(int value, float offset)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] FILD, add/subtract, then the first float store.
    return bf2x87::store(RoadScalar(value) + offset);
#else
    return static_cast<float>(value) + offset;
#endif
}

// PE 0x10141370
int extremeMinZ(RoadPolygon polygon, const std::vector<RoadEditableVertex>& vertices, float tolerance)
{
    float minimum = vertices[polygon[0]].position.z;
    for (unsigned int i = 1; i < polygon.size(); ++i)
    {
        const float z = vertices[polygon[i]].position.z;
        if (!roadLess(minimum, z))
            minimum = z;
    }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return roadGridRound(minimum, tolerance, false);
#else
    const int n = static_cast<int>(std::floor(static_cast<double>(minimum + tolerance)));
    return n - std::abs(n) % 2;
#endif
}

// the maximum x, even upward.
// PE 0x10141410
int extremeMaxX(RoadPolygon polygon, const std::vector<RoadEditableVertex>& vertices, float tolerance)
{
    float maximum = vertices[polygon[0]].position.x;
    for (unsigned int i = 1; i < polygon.size(); ++i)
    {
        const float x = vertices[polygon[i]].position.x;
        if (roadLess(maximum, x))
            maximum = x;
    }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return roadGridRound(maximum, tolerance, true);
#else
    int n = static_cast<int>(std::ceil(static_cast<double>(maximum - tolerance)));
    const int odd = std::abs(n) % 2;
    if (odd)
        n = n - odd + 2;
    return n;
#endif
}

// the maximum z, even upward.
// PE 0x101414B0
int extremeMaxZ(RoadPolygon polygon, const std::vector<RoadEditableVertex>& vertices, float tolerance)
{
    float maximum = vertices[polygon[0]].position.z;
    for (unsigned int i = 1; i < polygon.size(); ++i)
    {
        const float z = vertices[polygon[i]].position.z;
        if (roadLess(maximum, z))
            maximum = z;
    }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return roadGridRound(maximum, tolerance, true);
#else
    int n = static_cast<int>(std::ceil(static_cast<double>(maximum - tolerance)));
    const int odd = std::abs(n) % 2;
    if (odd)
        n = n - odd + 2;
    return n;
#endif
}

// the minimum x, even downward.
// PE 0x101428D0
int extremeMinX(RoadPolygon polygon, const std::vector<RoadEditableVertex>& vertices, float tolerance)
{
    float minimum = vertices[polygon[0]].position.x;
    for (unsigned int i = 1; i < polygon.size(); ++i)
    {
        const float x = vertices[polygon[i]].position.x;
        if (!roadLess(minimum, x))
            minimum = x;
    }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return roadGridRound(minimum, tolerance, false);
#else
    const int n = static_cast<int>(std::floor(static_cast<double>(minimum + tolerance)));
    return n - std::abs(n) % 2;
#endif
}

// The cut classifyPolygon proposes (splitAndTriangulate's 12-byte local): a line x = cut.x (axis 0)
// or z = cut.z (axis 1).
struct PolygonCut
{
    float x;     // +0
    float z;     // +4
    int   axis;  // +8
};

// (`retn 0Ch`) - with 1 cm of tolerance: more than one 2 m cell across in x cuts at the
// first grid line inside (minX + 2, axis 0); else more than one in z (minZ + 2, axis 1); else a
// single cell - false.
// PE 0x1013C9A0
bool classifyPolygon(const RoadPolygon& polygon, PolygonCut& cut, const std::vector<RoadEditableVertex>& vertices)
{
    const int minX = extremeMinX(polygon, vertices, 0.01f);
    const int maxXLessTwo = roadGridStep(extremeMaxX(polygon, vertices, 0.01f), -2);
    const int minZ = extremeMinZ(polygon, vertices, 0.01f);
    const int maxZ = extremeMaxZ(polygon, vertices, 0.01f);
    if (minX != maxXLessTwo)
    {
        cut.x = static_cast<float>(roadGridStep(minX, 2));
        cut.z = 0.0f;
        cut.axis = 0;
        return true;
    }
    if (minZ != roadGridStep(maxZ, -2))
    {
        cut.x = 0.0f;
        cut.axis = 1;
        cut.z = static_cast<float>(roadGridStep(minZ, 2));
        return true;
    }
    return false;
}

bool containsIndex(const std::vector<int>& values, int value)
{
    for (unsigned int i = 0; i < values.size(); ++i)
    {
        if (values[i] == value)
            return true;
    }
    return false;
}

RoadScalar roadDistanceXZ(const Vec3& a, const Vec3& b)
{
    const RoadScalar x = RoadScalar(a.x) - b.x;
    const RoadScalar z = RoadScalar(a.z) - b.z;
    return roadSquareRoot(z * z + x * x);
}

RoadScalar roadDistance3(const Vec3& a, const Vec3& b, unsigned int order, bool spillDifferences)
{
    RoadScalar x = RoadScalar(a.x) - b.x;
    RoadScalar y = RoadScalar(a.y) - b.y;
    RoadScalar z = RoadScalar(a.z) - b.z;
    if (spillDifferences)
    {
        x = RoadScalar(static_cast<float>(x));
        y = RoadScalar(static_cast<float>(y));
        z = RoadScalar(static_cast<float>(z));
    }
    if (order == 2)
        return roadSquareRoot((y * y + x * x) + z * z);
    return roadSquareRoot(order == 1 ? (x * x + z * z) + y * y : (z * z + y * y) + x * x);
}

float roadProjectionRatio(const Vec3& origin, const Vec3& point, const Vec3& edge)
{
    const RoadScalar numerator = roadDistanceXZ(point, origin);
    const float x = edge.x - origin.x;
    const float z = edge.z - origin.z;
    return static_cast<float>(numerator / roadSquareRoot(RoadScalar(z) * z + RoadScalar(x) * x));
}

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] Native storage for the tessellator's register values and float spills.
struct RoadDeviation
{
    float x, y, z;
    RoadScalar denominatorX, denominatorY, denominatorZ;
};

RoadScalar roadLength(RoadScalar x, RoadScalar y, RoadScalar z, unsigned int order)
{
    if (order == 1)
        return roadSquareRoot((x * x + z * z) + y * y);
    if (order == 2)
        return roadSquareRoot((y * y + x * x) + z * z);
    return roadSquareRoot((z * z + y * y) + x * x);
}

RoadDeviation roadDeviation(const Vec3& anchor, const Vec3& sample, const Vec3& ahead, bool right)
{
    const RoadScalar rawDx = RoadScalar(anchor.x) - sample.x;
    const RoadScalar dx = right ? rawDx : RoadScalar(static_cast<float>(rawDx));
    const float dy = anchor.y - sample.y;
    const float dz = anchor.z - sample.z;
    const RoadScalar ex = RoadScalar(ahead.x) - anchor.x;
    const float ey = ahead.y - anchor.y;
    const RoadScalar ez = RoadScalar(ahead.z) - anchor.z;
    const float storedEx = static_cast<float>(ex);
    RoadDeviation result;
    result.x = static_cast<float>(RoadScalar(ey) * dz - ez * dy);
    result.y = static_cast<float>(ez * dx - RoadScalar(dz) * storedEx);
    result.z = static_cast<float>(RoadScalar(dy) * storedEx - RoadScalar(ey) * dx);
    result.denominatorX = RoadScalar(storedEx);
    result.denominatorY = ey;
    result.denominatorZ = right ? RoadScalar(static_cast<float>(ez)) : ez;
    return result;
}

float roadDeviationError(const RoadDeviation& value, unsigned int order)
{
    return static_cast<float>(roadLength(value.x, value.y, value.z, order)
                              / roadLength(value.denominatorX, value.denominatorY, value.denominatorZ, order));
}
#endif


// Splits polygon across line a-b, appending intersection vertices and output pieces.
// Returns false for no crossings or an odd crossing count. Requires at least three
// vertices and finite coordinates; endpoint hits use a one-centimetre tolerance.

// PE 0x10143490
bool clipPolygon(const Vec3& a, const Vec3& b, std::vector<RoadPolygon>& pieces, const RoadPolygon& polygon,
                 std::vector<RoadEditableVertex>& vertices, bool verbose)
{
    if (verbose)
    {
        for (unsigned int i = 0; i < polygon.size(); ++i)
        {
            // [shipped] empty - the release build kept the loop and dropped its body
        }
    }

    std::vector<int>                crossedEdges;      // edge index of each crossing
    std::vector<int>                crossingVertex;    // the polygon vertex it lands on, or -1
    std::vector<double>             crossingParameter; // s along a-b
    std::vector<int>                crossingNew;       // index into newVertices, or -1
    std::vector<RoadEditableVertex> newVertices;

    double s;
    int sideAfter = 0;
    for (unsigned int i = 0; i < polygon.size(); ++i)
    {
        const unsigned int count = static_cast<unsigned int>(polygon.size());
        double t = -1.0;                                                     // dbl_1021D9A8
        const unsigned int next = i + 1;
        Vec3 point = makeVec3(0.0f, 0.0f, 0.0f);
        const RoadEditableVertex& from = vertices[polygon[i]];
        const RoadEditableVertex& to = vertices[polygon[next % count]];

        if (segmentIntersection(a, b, from.position, to.position, point, s, t) == '1')
        {
            RoadEditableVertex crossing;
            crossing.interpolate(from, to, static_cast<float>(t));
            if (roadLess(roadDistanceXZ(crossing.position, to.position), 0.01))
            {
                t = 1.0;                                 // the end vertex is hit - tested below
            }
            else if (roadLess(roadDistanceXZ(crossing.position, from.position), 0.01))
            {
                crossingVertex.push_back(polygon[i]);
                crossingNew.push_back(-1);
                crossedEdges.push_back(static_cast<int>(i));
                crossingParameter.push_back(s);
                continue;
            }
            else
            {
                newVertices.push_back(crossing);
                crossingVertex.push_back(-1);
                crossingNew.push_back(static_cast<int>(newVertices.size()) - 1);
                crossingParameter.push_back(s);
                crossedEdges.push_back(static_cast<int>(i));
                continue;
            }
        }

        // The qword parameter stays in the register through subtraction, FABS and comparison.
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        if (!roadLess(bf2x87::abs(RoadDouble(t) - 1.0), 0.01))
#else
        if (!(std::fabs(t - 1.0) < 0.01))
#endif
            continue;

        const RoadScalar wideAbX = RoadScalar(b.x) - a.x;
        const RoadScalar wideAbZ = RoadScalar(b.z) - a.z;
        const RoadScalar sideOfStart = (RoadScalar(from.position.x) - a.x) * wideAbZ
                                    - wideAbX * (RoadScalar(from.position.z) - a.z);
        int sideBefore;
        if (roadLess(sideOfStart, 0.0f))
            sideBefore = -1;
        else if (roadLess(0.0f, sideOfStart))
            sideBefore = 1;
        else
            continue;                                    // the start vertex is on the line (or NaN)

        const float abX = static_cast<float>(wideAbX);
        const float abZ = static_cast<float>(wideAbZ);
        Vec3 after = makeVec3(0.0f, 0.0f, 0.0f);
        unsigned int k = (i + 2) % count;
        if (k != i)
        {
            for (;;)
            {
                after = vertices[polygon[k]].position;
                const RoadScalar side = (RoadScalar(after.x) - a.x) * abZ
                                      - RoadScalar(abX) * (RoadScalar(after.z) - a.z);
                if (roadLess(side, 0.0f))
                {
                    sideAfter = -1;
                    break;
                }
                if (roadLess(0.0f, side))
                {
                    sideAfter = 1;
                    break;
                }
                sideAfter = 0;
                k = (k + 1) % static_cast<unsigned int>(polygon.size());
                if (k == i)
                    break;
            }
        }
        if (sideBefore == sideAfter)
        {
            i = next;                                    // and the loop's ++i: the next edge is skipped
            continue;
        }
        double s2;
        double t2;
        if (segmentIntersection(a, b, to.position, after, point, s2, t2) == '1')
            continue;
        crossingVertex.push_back(polygon[next % static_cast<unsigned int>(polygon.size())]);
        crossingNew.push_back(-1);
        crossedEdges.push_back(static_cast<int>(i));
        crossingParameter.push_back(s);
        i = next;
    }

    if (crossedEdges.size() == 0)
        return false;
    if (crossedEdges.size() & 1)
        return false;

    unsigned int j;
    for (j = 0; j < crossingNew.size(); ++j)
    {
        if (crossingNew[j] == -1)
            continue;
        crossingVertex[j] = static_cast<int>(vertices.size());
        vertices.push_back(newVertices[static_cast<unsigned int>(crossingNew[j])]);
    }

    std::vector<int> sortedEdges;
    std::vector<int> sortedVertices;
    unsigned int smallest = j;                           // the previous loop's counter
    for (unsigned int row = 0; row < crossingParameter.size(); ++row)
    {
        double best = 1000.0;                                                // dbl_101E01D8
        for (unsigned int m = 0; m < crossingParameter.size(); ++m)
        {
            if (crossingParameter[m] != -1.0 && roadLess(crossingParameter[m], best))
            {
                smallest = m;
                best = crossingParameter[m];
            }
        }
        sortedEdges.push_back(crossedEdges[smallest]);
        sortedVertices.push_back(crossingVertex[smallest]);
        crossingParameter[smallest] = -1.0;
    }

    std::vector<int>  piece;
    std::vector<bool> used;
    for (unsigned int m = 0; m < polygon.size(); ++m)
        used.push_back(false);
    for (;;)
    {
        int start = -1;
        for (unsigned int m = 0; m < used.size(); ++m)
        {
            if (!used[m])
            {
                start = static_cast<int>(m);
                break;
            }
        }
        if (start == -1)
            break;

        piece.clear();
        unsigned int current = static_cast<unsigned int>(start);
        do
        {
            if (!containsIndex(piece, polygon[current]))
            {
                piece.push_back(polygon[current]);
                used[current] = true;
            }
            for (unsigned int e = 0; e < sortedEdges.size(); e += 2)
            {
                unsigned int matched;
                unsigned int partner;
                if (sortedEdges[e] == static_cast<int>(current))
                {
                    matched = e;
                    partner = e + 1;
                }
                else if (sortedEdges[e + 1] == static_cast<int>(current))
                {
                    matched = e + 1;
                    partner = e;
                }
                else
                {
                    continue;
                }
                if (!containsIndex(piece, sortedVertices[matched]))
                    piece.push_back(sortedVertices[matched]);
                if (!containsIndex(piece, sortedVertices[partner]))
                    piece.push_back(sortedVertices[partner]);
                current = static_cast<unsigned int>(sortedEdges[partner]);
                for (unsigned int m = 0; m < polygon.size(); ++m)
                {
                    if (polygon[m] == sortedVertices[matched] || polygon[m] == sortedVertices[partner])
                        used[m] = true;
                }
                break;
            }
            current = (current + 1) % static_cast<unsigned int>(polygon.size());
        }
        while (current != static_cast<unsigned int>(start));
        pieces.push_back(piece);
    }
    return true;
}


// Assigns coordinates to vertices from start onward using the road columns and mode.
// Mode 0 clears resolved flags; modes 1 and 2 retain them. Requires nonempty columns
// and finite coordinates. Mode 2 intentionally reuses the centre-column distance.
// Shipped defect: failed parallel intersections leave the interpolation parameter unwritten.
// PE 0x10141690
void assignTexCoords(const std::vector<RoadEditableVertex>& first, const std::vector<RoadEditableVertex>& second,
                     const std::vector<RoadEditableVertex>& third, std::vector<RoadEditableVertex>& vertices,
                     unsigned int start, int mode)
{
    if (mode == 0)
    {
        for (unsigned int k = start; k < vertices.size(); ++k)
        {
            RoadEditableVertex& v = vertices[k];
            if (!v.needsUv)
                continue;
            for (unsigned int i = 1; i < first.size(); ++i)
            {
                const RoadEditableVertex& l0 = first[i - 1];
                const RoadEditableVertex& l1 = first[i];
                const RoadEditableVertex& r1 = second[i];
                const RoadEditableVertex& r0 = second[i - 1];
                if (interpolateInTriangle(l0, l1, r1, v) || interpolateInTriangle(r0, l0, r1, v))
                {
                    v.needsUv = 0;
                    break;
                }
                int side;
                orient(l0.position, l1.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(l0, l1,
                        roadProjectionRatio(l0.position, v.position, l1.position));
                    v.needsUv = 0;
                    break;
                }
                orient(l1.position, r1.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(l1, r1,
                        roadProjectionRatio(l1.position, v.position, r1.position));
                    v.needsUv = 0;
                    break;
                }
                orient(r1.position, r0.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(r1, r0,
                        roadProjectionRatio(r1.position, v.position, r0.position));
                    v.needsUv = 0;
                    break;
                }
                orient(r0.position, l0.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(r0, l0,
                        roadProjectionRatio(r0.position, v.position, l0.position));
                    v.needsUv = 0;
                    break;
                }
                orient(l0.position, r1.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(l0, r1,
                        roadProjectionRatio(l0.position, v.position, r1.position));
                    v.needsUv = 0;
                    break;
                }
            }
            if (v.needsUv)
            {
                for (unsigned int i = 2; i < first.size(); ++i)
                {
                    if (interpolateInTriangle(first[i - 2], first[i], second[i], v)
                        || interpolateInTriangle(second[i - 2], first[i - 2], second[i], v))
                    {
                        v.needsUv = 0;
                        break;
                    }
                }
            }
        }
        return;
    }

    if (mode == 1)
    {
        // Shipped defect: until a nearer sample is found, this argument slot holds the third column pointer.
        unsigned int nearest = static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(&third));
        int side = static_cast<int>(start);          // shipped: the `start` argument slot
        float s;
        float t;
        for (unsigned int k = start; k < vertices.size(); ++k)
        {
            RoadEditableVertex& v = vertices[k];
            if (!v.needsUv)
                continue;
            float best = FLT_MAX;                                            // flt_10208A04
            for (unsigned int i = 0; i < first.size(); ++i)
            {
                const Vec3& p1 = first[i].position;
                const Vec3& p2 = second[i].position;
                const RoadScalar firstDistance = roadDistance3(p1, v.position, true, false);
                const float toFirst = static_cast<float>(firstDistance);
                const float toSecond = static_cast<float>(roadDistance3(p2, v.position, true, false));
                if (roadLess(firstDistance, best))
                {
                    nearest = i;
                    best = toFirst;
                    side = 1;
                }
                if (roadLess(toSecond, best))
                {
                    nearest = i;
                    best = toSecond;
                    side = 2;
                }
            }

            unsigned int neighbour;
            if (nearest == 0)
            {
                neighbour = 1;
            }
            else if (nearest == first.size() - 1)
            {
                neighbour = static_cast<unsigned int>(first.size()) - 2;
            }
            else
            {
                RoadEditableVertex previous;
                RoadEditableVertex following;
                if (side == 1)
                {
                    previous = first[nearest - 1];
                    following = first[nearest + 1];
                }
                else
                {
                    previous = second[nearest - 1];
                    following = second[nearest + 1];
                }
                const RoadScalar toPrevious = roadDistance3(previous.position, v.position, false, false);
                const RoadScalar toFollowing = roadDistance3(following.position, v.position, false, true);
                neighbour = roadLess(toPrevious, toFollowing) ? nearest - 1 : nearest + 1;
            }

            RoadEditableVertex closest;
            RoadEditableVertex across;
            const std::vector<RoadEditableVertex>* column;
            if (side == 1)
            {
                closest = first[nearest];
                across = second[nearest];
                column = &first;
            }
            else
            {
                closest = second[nearest];
                across = first[nearest];
                column = &second;
            }
            RoadEditableVertex along;
            along = (*column)[neighbour];
            segmentsIntersect(along.position, across.position, closest.position, v.position, &s, &t);
            RoadEditableVertex hit;
            hit.interpolate(along, across, s);
            const float ratio = roadProjectionRatio(closest.position, v.position, hit.position);
            v.interpolateTexCoords(closest, hit, ratio);
        }
        return;
    }

    if (mode == 2)
    {
        unsigned int nearest = start;                // shipped: the `start` argument slot
        int side = mode;                             // shipped: the `mode` argument slot
        float s;
        float t;
        for (unsigned int k = start; k < vertices.size(); ++k)
        {
            RoadEditableVertex& v = vertices[k];
            if (!v.needsUv)
                continue;
            float best = FLT_MAX;
            for (unsigned int i = 0; i < first.size(); ++i)
            {
                const Vec3& p1 = first[i].position;
                const Vec3& p2 = second[i].position;
                const RoadScalar firstDistance = roadDistance3(p1, v.position, true, false);
                const float toFirst = static_cast<float>(firstDistance);
                const float toSecond = static_cast<float>(roadDistance3(p2, v.position, true, false));
                const float toThird = static_cast<float>(roadDistance3(p2, v.position, true, false)); // shipped defect: centre again
                if (roadLess(firstDistance, best))
                {
                    nearest = i;
                    best = toFirst;
                    side = 1;
                }
                if (roadLess(toSecond, best))
                {
                    nearest = i;
                    best = toSecond;
                    side = 21;
                    if (!roadLess(toFirst, toThird))
                        side = 23;
                }
                if (roadLess(toThird, best))
                {
                    nearest = i;
                    best = toThird;
                    side = 3;
                }
            }

            unsigned int neighbour;
            if (nearest == 0)
            {
                neighbour = 1;
            }
            else if (nearest == first.size() - 1)
            {
                neighbour = static_cast<unsigned int>(first.size()) - 2;
            }
            else
            {
                RoadEditableVertex previous;
                RoadEditableVertex following;
                if (side == 1)
                {
                    previous = first[nearest - 1];
                    following = first[nearest + 1];
                }
                else
                {
                    const std::vector<RoadEditableVertex>& column =
                        (side == 21 || side == 23) ? second : third;
                    previous = column[nearest - 1];
                    following = column[nearest + 1];
                }
                const RoadScalar toPrevious = roadDistance3(previous.position, v.position, true, false);
                const RoadScalar toFollowing = roadDistance3(following.position, v.position, true, true);
                neighbour = roadLess(toPrevious, toFollowing) ? nearest - 1 : nearest + 1;
            }

            RoadEditableVertex closest;
            RoadEditableVertex across;
            const std::vector<RoadEditableVertex>* column;
            if (side == 1)
            {
                closest = first[nearest];
                across = second[nearest];
                column = &first;
            }
            else if (side == 21)
            {
                closest = second[nearest];
                across = first[nearest];
                column = &second;
            }
            else if (side == 23)
            {
                closest = second[nearest];
                across = third[nearest];
                column = &second;
            }
            else
            {
                closest = third[nearest];
                across = second[nearest];
                column = &third;
            }
            RoadEditableVertex along;
            along = (*column)[neighbour];
            segmentsIntersect(along.position, across.position, closest.position, v.position, &s, &t);
            RoadEditableVertex hit;
            hit.interpolate(along, across, s);
            const float ratio = roadProjectionRatio(closest.position, v.position, hit.position);
            v.interpolateTexCoords(closest, hit, ratio);
        }
    }
}

// The fade ramps' clamps, each exactly as its arm compares (NaN results noted per form).
// First pass: t < 1 ? (0 < t ? t : 0) : 1 - a NaN becomes 1.
float fadeRamp(RoadScalar t)
{
    if (!roadLess(t, 1.0f))
        return 1.0f;
    const float stored = static_cast<float>(t);
    return roadLess(0.0f, stored) ? stored : 0.0f;
}

// Second pass: min(min(t, 1), current) against 0 - a NaN t becomes 1, a NaN current is replaced.
float fadeRampEnd(RoadScalar t, float current)
{
    if (!roadLess(t, 1.0f))
        t = RoadScalar(1.0f);
    if (roadLess(current, t))
        t = RoadScalar(current);
    return roadLess(0.0f, t) ? static_cast<float>(t) : 0.0f;
}

} // anonymous namespace


// Dead tessellator helpers: unreachable in 1.50, no code or data references (no export, no table, no
// caller beyond the pair noted below). They keep their external linkage so the unused-function warning
// stays quiet; nothing calls them. The thiscall-on-the-road bodies take the road's pending list
// (+0xD8; the only road field they read) by reference [CHOSEN, not recovered].

// (thiscall on `out`, `retn 0Ch` through the target) - `push ebp; mov ebp, esp; pop ebp; jmp` to the
// Vec3 lerp, the same three arguments untouched.
// PE 0x1013B750
void roadLerpVec3Forward(Vec3& out, const Vec3& a, const Vec3& b, float t)
{
    lerpVec3(out, a, b, t);
}

// (stdcall, `retn 0Ch`) - areaSign(a, b, c) == 0.
// PE 0x1013BC60
bool roadAreaSignIsZero(const Vec3& a, const Vec3& b, const Vec3& c)
{
    return areaSign(a, b, c) == 0;
}

// (thiscall, `retn 8`) - every element equal to oldIndex in every pending polygon becomes newIndex.
// Only the snapping helper below calls it.
// PE 0x1013C5D0
void roadReplacePendingIndex(RoadPolygonList& pending, int oldIndex, int newIndex)
{
    for (RoadPolygonList::iterator node = pending.begin(); node != pending.end(); ++node)
    {
        RoadPolygon& polygon = *node;
        for (unsigned int i = 0; !polygon.empty() && i < polygon.size(); ++i)
        {
            if (polygon[i] == oldIndex)
                polygon[i] = newIndex;
        }
    }
}

// (`retn 20h`) - the edge polygon[k] -> polygon[(k + 1) % n] against the segment a-b. Writes
// index = -1 and the first ratio `s` (double) before it decides anything. A second ratio within
// 1e-4 of 1 means the lines meet at the far end: answers whether the other end points of the edge lie
// on opposite sides of a-b (nothing is appended). Otherwise, when the second ratio lies in
// [0.0001, 1), appends the point lerp(edge start, edge end, ratio) as a fresh vertex to `out`,
// writes its index to `index` and answers true. polygon.size() == 0 divides by zero (shipped).
// PE 0x1013CA80
bool roadEdgeCrossing(const Vec3& a, const Vec3& b, const RoadPolygon& polygon, int k,
                      const std::vector<RoadEditableVertex>& vertices, double& s, int& index,
                      std::vector<RoadEditableVertex>& out)
{
    const unsigned int count = static_cast<unsigned int>(polygon.size());
    const unsigned int next = static_cast<unsigned int>(k + 1) % count;
    const float ax = a.x;
    const float az = a.z;
    const float bx = b.x;
    const RoadScalar dzAB = RoadScalar(b.z) - az;
    const float dzABStored = static_cast<float>(dzAB);
    const Vec3& c = vertices[polygon[k]].position;
    const Vec3& d = vertices[polygon[next]].position;
    const float cx = c.x;
    const float cz = c.z;
    const float dx = d.x;
    const float dz = d.z;

    index = -1;
    const RoadScalar t1Register = (RoadScalar(dz) - cz) * ax;
    const double t1 = static_cast<double>(t1Register);
    const RoadScalar t2 = (RoadScalar(cz) - dz) * bx;
    const RoadScalar t3 = t2 + (RoadScalar(az) - b.z) * cx;
    const RoadScalar t4 = t3 + RoadScalar(dx) * dzAB;
    const RoadScalar denominator = t1Register + t4;
    const float u = static_cast<float>(RoadScalar(cz) - az);
    const RoadScalar inverse = RoadScalar(1.0) / denominator;
    const RoadScalar sValue = (((RoadScalar(az) - dz) * cx + RoadScalar(u) * dx) + t1) * inverse;
    s = static_cast<double>(sValue);
    const RoadScalar x1 = RoadScalar(cz) - b.z;
    const RoadScalar y1 = x1 * ax + (RoadScalar(az) - cz) * bx;
    const RoadScalar y2 = y1 + RoadScalar(cx) * dzAB;
    const RoadScalar ratio = (-y2) * inverse;

    if (roadLess(0.9999, ratio) && roadLess(ratio, 1.0001))
    {
        const float q = static_cast<float>(RoadScalar(bx) - ax);
        const float r = static_cast<float>(RoadScalar(dzABStored) * (RoadScalar(cx) - ax)
                                           - RoadScalar(u) * q);
        if (roadLess(RoadScalar(std::fabs(r)), 0.0001))
            return false;
        // The shipped source compares two constants here (a dword 0 and |qword 0| < 1e-4), so the
        // scan below always runs.
        RoadScalar side = 0.0f;
        const double zeroConstant = 0.0;
        if (roadLess(RoadScalar(std::fabs(zeroConstant)), 0.0001))
        {
            unsigned int j = static_cast<unsigned int>(k) + 2;
            do
            {
                const unsigned int size = static_cast<unsigned int>(polygon.size());
                const Vec3& e = vertices[polygon[j % size]].position;
                ++j;
                side = (RoadScalar(e.x) - ax) * dzABStored - (RoadScalar(e.z) - az) * q;
            } while (roadLess(RoadScalar(std::fabs(static_cast<double>(side))), 0.0001));
        }
        if (roadLess(0.0f, r) && roadLess(0.0f, side))
            return false;
        if (roadLess(r, 0.0f) && roadLess(side, 0.0f))
            return false;
        return true;
    }

    if (roadLess(ratio, 0.0001) || roadLessEqual(1.0, ratio))
        return false;

    RoadEditableVertex crossing;
    lerpVec3(crossing.position, c, d, static_cast<float>(ratio));
    crossing.uv0.x = 0.0f;                          // the inline vertex constructor's stores after the call
    crossing.uv0.y = 0.0f;
    crossing.uv1.x = 0.0f;
    crossing.uv1.y = 0.0f;
    index = static_cast<int>(out.size());
    out.push_back(crossing);
    return true;
}

// (thiscall, `retn 8`) - snaps the point to millimetres in x and z (y is kept), drops every vertex
// within 1 mm of it (their indices become -1 in the pending polygons, the survivors are renumbered
// into a compacted copy), appends the point itself under index -1's replacement, then removes
// consecutive duplicate indices from each pending polygon. The compacted vertex vector is a local
// that is destroyed at the end (`vertices` is never written; shipped).
// Shipped defect, restored as a bounds check [CHOSEN, not recovered; memory-safety exception]: the
// duplicate-removal pass compares the next index with element (m - 1) of the half-built copy, which
// lies past its end once an index has been skipped; here such an out-of-range comparison counts as
// "different".
// PE 0x1013CDE0
void roadSnapPoint(RoadPolygonList& pending, RoadEditableVertex& point,
                   const std::vector<RoadEditableVertex>& vertices)
{
    const float y = point.position.y;
    point.position.x = static_cast<float>(RoadScalar(point.position.x) * 1000.0f);
    point.position.y = static_cast<float>(RoadScalar(point.position.y) * 1000.0f);
    point.position.z = static_cast<float>(RoadScalar(point.position.z) * 1000.0f);
    point.position.x = static_cast<float>(RoadScalar(static_cast<int>(point.position.x)) * 0.001f);
    point.position.z = static_cast<float>(RoadScalar(static_cast<int>(point.position.z)) * 0.001f);
    point.position.y = y;

    std::vector<RoadEditableVertex> snapped;
    for (unsigned int i = 0; !vertices.empty() && i < vertices.size(); ++i)
    {
        const RoadEditableVertex& v = vertices[i];
        const RoadScalar deltaX = RoadScalar(v.position.x) - point.position.x;
        const RoadScalar deltaZ = RoadScalar(v.position.z) - point.position.z;
        const RoadScalar distance = roadSquareRoot(deltaX * deltaX + deltaZ * deltaZ);
        if (roadLess(0.001, distance))
        {
            const int newIndex = static_cast<int>(snapped.size());
            snapped.push_back(v);
            if (newIndex != static_cast<int>(i))
                roadReplacePendingIndex(pending, static_cast<int>(i), newIndex);
        }
        else
        {
            roadReplacePendingIndex(pending, static_cast<int>(i), -1);
        }
    }
    const int pointIndex = static_cast<int>(snapped.size());
    snapped.push_back(point);
    roadReplacePendingIndex(pending, -1, pointIndex);

    for (RoadPolygonList::iterator node = pending.begin(); node != pending.end(); ++node)
    {
        RoadPolygon& polygon = *node;
        for (unsigned int j = 1; !polygon.empty() && j < polygon.size(); ++j)
        {
            if (polygon[j - 1] != polygon[j])
                continue;
            RoadPolygon compact;
            compact.push_back(polygon[0]);
            for (unsigned int m = 1; !polygon.empty() && m < polygon.size(); ++m)
            {
                const bool same = (m - 1 < compact.size()) && compact[m - 1] == polygon[m];
                if (!same)
                    compact.push_back(polygon[m]);
            }
            polygon = compact;
            break;
        }
    }
}


// (`retn 8`) - drain the pending list: a polygon of three or more vertices that spans more
// than one 2 m cell is cut at the first grid line inside (classifyPolygon; the cut line runs 1 m past
// the polygon's even-rounded extent) and its pieces go back on the pending list; a single cell is cut
// along its diagonal (from (minX - 1, maxZ + 1) to (maxX + 1, minZ - 1), extents taken with 1 cm of
// tolerance) and the pieces go to the ready list; a polygon the cut misses goes to the ready list
// whole. Then every ready polygon of three or more vertices is ear-clipped into `indices`. Polygons
// with fewer vertices are dropped from both lists unprocessed.

// PE 0x1013D560
void RoadEditable::splitAndTriangulate(std::vector<RoadEditableVertex>& vertices, std::vector<int>& indices)
{
    PolygonCut cut;
    cut.x = 0.0f;
    cut.z = 0.0f;                                    // the axis word is not initialised (shipped)
    while (!m_pendingPolygons.empty())
    {
        RoadPolygon& polygon = m_pendingPolygons.front();
        if (polygon.size() >= 3)
        {
            if (classifyPolygon(polygon, cut, vertices))
            {
                Vec3 lineStart;
                Vec3 lineEnd;
                if (cut.axis == 0)
                {
                    const int maxZ = extremeMaxZ(polygon, vertices, 0.0f);
                    lineStart = makeVec3(cut.x, 0.0f, roadGridOffset(maxZ, 1.0f));
                    const int minZ = extremeMinZ(polygon, vertices, 0.0f);
                    lineEnd = makeVec3(cut.x, 0.0f, roadGridOffset(minZ, -1.0f));
                }
                else
                {
                    const int maxX = extremeMaxX(polygon, vertices, 0.0f);
                    lineStart = makeVec3(roadGridOffset(maxX, 1.0f), 0.0f, cut.z);
                    const int minX = extremeMinX(polygon, vertices, 0.0f);
                    lineEnd = makeVec3(roadGridOffset(minX, -1.0f), 0.0f, cut.z);
                }
                std::vector<RoadPolygon> pieces;
                if (clipPolygon(lineStart, lineEnd, pieces, polygon, vertices, false))
                {
                    for (unsigned int k = 0; k < pieces.size(); ++k)
                        m_pendingPolygons.push_back(pieces[k]);
                }
                else
                {
                    m_readyPolygons.push_back(polygon);
                }
            }
            else
            {
                const int maxZ = extremeMaxZ(polygon, vertices, 0.01f);
                const int minX = extremeMinX(polygon, vertices, 0.01f);
                const Vec3 lineStart = makeVec3(roadGridOffset(minX, -1.0f), 0.0f,
                                                roadGridOffset(maxZ, 1.0f));
                const int minZ = extremeMinZ(polygon, vertices, 0.01f);
                const int maxX = extremeMaxX(polygon, vertices, 0.01f);
                const Vec3 lineEnd = makeVec3(roadGridOffset(maxX, 1.0f), 0.0f,
                                              roadGridOffset(minZ, -1.0f));
                std::vector<RoadPolygon> pieces;
                if (clipPolygon(lineStart, lineEnd, pieces, polygon, vertices, false))
                {
                    for (unsigned int k = 0; k < pieces.size(); ++k)
                        m_readyPolygons.push_back(pieces[k]);
                }
                else
                {
                    m_readyPolygons.push_back(polygon);
                }
            }
        }
        m_pendingPolygons.pop_front();
    }

    while (!m_readyPolygons.empty())
    {
        RoadPolygon& polygon = m_readyPolygons.front();
        if (polygon.size() >= 3)
            earClip(polygon, vertices, indices);
        m_readyPolygons.pop_front();
    }
}

// Builds road vertices and triangle indices from the spline, template and optional noise.
// Returns the triangle count, or zero with fewer than four control points. maxError is ignored;
// the template supplies the error limit. Ghost mode emits a strip or mirrored-column mesh.
// Requires sampled edges to be nonempty; the final error test retains an unchecked end read.
// Noise changes only x. Final geometry is split on the terrain grid and projected to its heightmap.
// PE 0x1013DAC0
int RoadEditable::tessellate(std::vector<RoadEditableVertex>& vertices, std::vector<int>& indices,
                             float width, float /*maxError - not read*/, bool ghost)
{
    const float halfWidth = width * 0.5f;                                    // flt_101DD4F0
    if (m_spline->getControlPointCount() < 4)                                // +0x54
        return 0;
    IEditorSpline* const leftSpline = m_spline->makeOffsetCopy(halfWidth);   // +0x10
    IEditorSpline* const rightSpline = m_spline->makeOffsetCopy(-halfWidth);

    Vec3 position = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
    std::vector<RoadEditableVertex> leftSamples;
    std::vector<RoadEditableVertex> rightSamples;

    leftSpline->nextSegment();                                               // +0x6C: restart
    for (int segment = 1; segment < leftSpline->getControlPointCount() - 2; ++segment)
    {
        leftSpline->nextSegment();
        while (leftSpline->nextSample(&position, &normal))                   // +0x70
            leftSamples.push_back(RoadEditableVertex(position, normal));
    }
    rightSpline->nextSegment();
    for (int segment = 1; segment < rightSpline->getControlPointCount() - 2; ++segment)
    {
        rightSpline->nextSegment();
        while (rightSpline->nextSample(&position, &normal))
            rightSamples.push_back(RoadEditableVertex(position, normal));
    }
    leftSpline->release();
    rightSpline->release();

    const Vec2& offset0 = m_textureScaleOffsets[0];
    const Vec2& scale0 = m_template->getTextureScale(0);                     // +0x1C
    const float scaleU0 = offset0.x + scale0.x;
    const float scaleV0 = scale0.y + offset0.y;
    const Vec2& offset1 = m_textureScaleOffsets[1];
    const Vec2& scale1 = m_template->getTextureScale(1);
    const float scaleU1 = scale1.x + offset1.x;
    const RoadScalar scaleV1 = RoadScalar(offset1.y) + scale1.y;
    const RoadScalar inverseWidth = RoadScalar(1.0f) / width;
    const float alongScale0 = static_cast<float>(RoadScalar(scaleU0) * inverseWidth / scaleV0);
    const float alongScale1 = static_cast<float>(inverseWidth * scaleU1 / scaleV1);
    const bool mirror0 = m_template->getMirror(0);                           // +0x24
    const bool mirror1 = m_template->getMirror(1);

    std::vector<RoadEditableVertex> firstColumn;     // left
    std::vector<RoadEditableVertex> secondColumn;    // right (two columns) / centre (three)
    std::vector<RoadEditableVertex> thirdColumn;     // right (three columns); empty otherwise
    RoadEditableVertex firstAnchor;
    RoadEditableVertex secondAnchor;

    if (!mirror0 && !mirror1)
    {
        // two columns
        firstAnchor = leftSamples[0];
        firstAnchor.uv1.x = 0.0f;
        firstAnchor.uv1.y = 0.0f;
        firstAnchor.uv0.x = 0.0f;
        firstAnchor.uv0.y = 0.0f;
        secondAnchor = rightSamples[0];
        secondAnchor.uv0.y = 0.0f;
        secondAnchor.uv1.y = 0.0f;
        const float inverseScaleU0 = 1.0f / scaleU0;
        secondAnchor.uv0.x = inverseScaleU0;
        const float inverseScaleU1 = 1.0f / scaleU1;
        secondAnchor.uv1.x = inverseScaleU1;
        firstColumn.push_back(firstAnchor);
        secondColumn.push_back(secondAnchor);

        const RoadEditableVertex* const left = leftSamples.data();
        const RoadEditableVertex* const right = rightSamples.data();
        unsigned int nextRow = 1;
        for (unsigned int i = 1; i < leftSamples.size(); ++i)
        {
            bool keep = false;
            for (unsigned int j = nextRow; j <= i; ++j)
            {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                const RoadDeviation leftDeviation = roadDeviation(firstAnchor.position, left[j].position,
                                                                   left[i + 1].position, false);
                const RoadDeviation rightDeviation = roadDeviation(secondAnchor.position, right[j].position,
                                                                    right[i + 1].position, true);
                const float leftError = roadDeviationError(leftDeviation, 0);
#else
                const Vec3& a = firstAnchor.position;
                const float d1x = a.x - left[j].position.x;
                const float d1y = a.y - left[j].position.y;
                const float d1z = a.z - left[j].position.z;
                const Vec3& ahead = left[i + 1].position;        // one past the end for the last i
                const float ex = ahead.x - a.x;
                const float ey = ahead.y - a.y;
                const float ez = ahead.z - a.z;
                const float c1x = ey * d1z - ez * d1y;
                const float c1y = ez * d1x - d1z * ex;
                const float c1z = d1y * ex - ey * d1x;

                const Vec3& b = secondAnchor.position;
                const float d2x = b.x - right[j].position.x;
                const float d2y = b.y - right[j].position.y;
                const float d2z = b.z - right[j].position.z;
                const Vec3& aheadRight = right[i + 1].position;
                const float fx = aheadRight.x - b.x;
                const float fy = aheadRight.y - b.y;
                const float fz = aheadRight.z - b.z;
                const float c2x = fy * d2z - fz * d2y;
                const float c2y = fz * d2x - d2z * fx;
                const float c2z = d2y * fx - fy * d2x;

                const float leftError = lengthSumZYX(c1x, c1y, c1z) / lengthSumZYX(ex, ey, ez);
#endif
                if (roadLess(m_template->getMaxTesselatedError(0), leftError)) // +0x34
                {
                    keep = true;
                    break;
                }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                const float rightError = roadDeviationError(rightDeviation, 0);
#else
                const float rightError = lengthSumZYX(c2x, c2y, c2z) / lengthSumZYX(fx, fy, fz);
#endif
                if (roadLess(m_template->getMaxTesselatedError(0), rightError) || i == leftSamples.size() - 1)
                {
                    keep = true;
                    break;
                }
            }
            if (!keep)
                continue;

            RoadEditableVertex leftVertex = leftSamples[i];
            RoadEditableVertex rightVertex = rightSamples[i];
            if (m_noise)
            {
                const float noiseX = static_cast<float>(RoadScalar(i) * 0.1f); // flt_101DD25C
                leftVertex.position.x = m_noise->sample(noiseX, 0.0f, 0.0f, 0.0f) + leftVertex.position.x;
                rightVertex.position.x = m_noise->sample(noiseX + 1000.0f, 0.0f, 0.0f, 0.0f)
                                       + rightVertex.position.x;
            }
            rightVertex.uv0.x = inverseScaleU0;
            rightVertex.uv1.x = inverseScaleU1;
            leftVertex.uv0.x = 0.0f;
            leftVertex.uv1.x = 0.0f;
            const RoadScalar rowLength = roadDistance3(rightVertex.position, secondAnchor.position, 0, false);
            const float v0 = static_cast<float>(RoadScalar(alongScale0) * rowLength + secondAnchor.uv0.y);
            rightVertex.uv0.y = v0;
            const float v1 = static_cast<float>(RoadScalar(alongScale1) * rowLength + secondAnchor.uv1.y);
            rightVertex.uv1.y = v1;
            leftVertex.uv1.y = v1;
            leftVertex.uv0.y = v0;
            firstColumn.push_back(leftVertex);
            secondColumn.push_back(rightVertex);
            firstAnchor = leftVertex;
            secondAnchor = rightVertex;
            nextRow = i + 1;
        }

        // the fade ramps
        if (m_fadeDistAtStartLeft.y != 0.0f)
            firstColumn[0].alpha = 0.0f;
        if (m_fadeDistAtStartRight.y != 0.0f)
            secondColumn[0].alpha = 0.0f;

        RoadScalar firstDistance = RoadScalar(0.0f);
        RoadScalar secondDistance = RoadScalar(0.0f);
        for (unsigned int k = 1; k < firstColumn.size(); ++k)
        {
            const Vec3& p = firstColumn[k].position;
            const Vec3& q = firstColumn[k - 1].position;
            firstDistance = firstDistance + roadDistance3(p, q, 0, false);
            const Vec3& r = secondColumn[k].position;
            const Vec3& u = secondColumn[k - 1].position;
            secondDistance = secondDistance + roadDistance3(r, u, 0, false);
            if (0.0f != RoadScalar(m_fadeDistAtStartLeft.y) - m_fadeDistAtStartLeft.x)
            {
                firstColumn[k].alpha = fadeRamp((firstDistance - m_fadeDistAtStartLeft.x)
                    / (RoadScalar(m_fadeDistAtStartLeft.y) - m_fadeDistAtStartLeft.x));
            }
            if (0.0f != RoadScalar(m_fadeDistAtStartRight.y) - m_fadeDistAtStartRight.x)
            {
                secondColumn[k].alpha = fadeRamp((secondDistance - m_fadeDistAtStartRight.x)
                    / (RoadScalar(m_fadeDistAtStartRight.y) - m_fadeDistAtStartRight.x));
            }
        }
        const RoadScalar firstTotal = firstDistance;
        const float secondTotal = static_cast<float>(secondDistance);
        RoadScalar firstRun = RoadScalar(0.0f);
        RoadScalar secondRun = RoadScalar(0.0f);
        for (unsigned int k = 1; k < firstColumn.size(); ++k)
        {
            const Vec3& p = firstColumn[k].position;
            const Vec3& q = firstColumn[k - 1].position;
            firstRun = firstRun + roadDistance3(p, q, 1, false);
            const Vec3& r = secondColumn[k].position;
            const Vec3& u = secondColumn[k - 1].position;
            secondRun = secondRun + roadDistance3(r, u, 1, false);
            if (0.0f != RoadScalar(m_fadeDistAtEndLeft.y) - m_fadeDistAtEndLeft.x)
            {
                firstColumn[k].alpha = fadeRampEnd(((firstTotal - firstRun) - m_fadeDistAtEndLeft.x)
                    / (RoadScalar(m_fadeDistAtEndLeft.y) - m_fadeDistAtEndLeft.x), firstColumn[k].alpha);
            }
            if (0.0f != RoadScalar(m_fadeDistAtEndRight.y) - m_fadeDistAtEndRight.x)
            {
                secondColumn[k].alpha = fadeRampEnd(((secondTotal - secondRun) - m_fadeDistAtEndRight.x)
                    / (RoadScalar(m_fadeDistAtEndRight.y) - m_fadeDistAtEndRight.x), secondColumn[k].alpha);
            }
        }

        if (ghost)
        {
            for (unsigned int k = 0; k < firstColumn.size(); ++k)
            {
                vertices.push_back(secondColumn[k]);
                vertices.push_back(firstColumn[k]);
            }
            return static_cast<int>(vertices.size()) - 2;
        }

        RoadPolygon outline;
        for (unsigned int k = 0; k < firstColumn.size(); ++k)
        {
            const int index = static_cast<int>(vertices.size());
            outline.push_back(index);
            vertices.push_back(firstColumn[k]);
        }
        for (unsigned int k = 0; k < secondColumn.size(); ++k)
        {
            const int index = static_cast<int>(vertices.size());
            outline.insert(outline.begin(), index);
            vertices.push_back(secondColumn[k]);
        }
        m_pendingPolygons.push_back(outline);
    }
    else
    {
        // three columns
        RoadEditableVertex thirdAnchor;
        firstAnchor = leftSamples[0];
        thirdAnchor = rightSamples[0];
        secondAnchor = firstAnchor;
        secondAnchor.position = makeVec3(static_cast<float>((RoadScalar(thirdAnchor.position.x) + firstAnchor.position.x) * 0.5f),
                                         (thirdAnchor.position.y + firstAnchor.position.y) * 0.5f,
                                         (thirdAnchor.position.z + firstAnchor.position.z) * 0.5f);
        firstAnchor.uv1.x = 0.0f;
        firstAnchor.uv1.y = 0.0f;
        firstAnchor.uv0.x = 0.0f;
        firstAnchor.uv0.y = 0.0f;
        thirdAnchor.uv0.y = 0.0f;
        secondAnchor.uv0.y = 0.0f;
        const RoadScalar rawInverseScaleU0 = RoadScalar(1.0f) / scaleU0;
        const float inverseScaleU0 = static_cast<float>(rawInverseScaleU0);
        if (mirror0)
        {
            secondAnchor.uv0.x = inverseScaleU0;
            thirdAnchor.uv0.x = 0.0f;
        }
        else
        {
            secondAnchor.uv0.x = static_cast<float>(RoadScalar(0.5f) * rawInverseScaleU0);
            thirdAnchor.uv0.x = inverseScaleU0;
        }
        const RoadScalar rawInverseScaleU1 = RoadScalar(1.0f) / scaleU1;
        const float inverseScaleU1 = static_cast<float>(rawInverseScaleU1);
        thirdAnchor.uv1.y = 0.0f;
        secondAnchor.uv1.y = 0.0f;
        if (mirror1)
        {
            secondAnchor.uv1.x = inverseScaleU1;
            thirdAnchor.uv1.x = 0.0f;
        }
        else
        {
            secondAnchor.uv1.x = static_cast<float>(RoadScalar(0.5f) * rawInverseScaleU1);
            thirdAnchor.uv1.x = inverseScaleU1;
        }
        firstColumn.push_back(firstAnchor);
        secondColumn.push_back(secondAnchor);
        thirdColumn.push_back(thirdAnchor);

        const RoadEditableVertex* const left = leftSamples.data();
        const RoadEditableVertex* const right = rightSamples.data();
        const unsigned int sampleCount = static_cast<unsigned int>(leftSamples.size());
        unsigned int nextRow = 1;
        for (unsigned int i = 1; i < sampleCount; ++i)
        {
            bool keep = false;
            for (unsigned int j = nextRow; j <= i; ++j)
            {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                const RoadDeviation leftDeviation = roadDeviation(firstAnchor.position, left[j].position,
                                                                   left[i + 1].position, false);
                const RoadDeviation rightDeviation = roadDeviation(thirdAnchor.position, right[j].position,
                                                                    right[i + 1].position, true);
                const float leftError = roadDeviationError(leftDeviation, 0);
#else
                const Vec3& a = firstAnchor.position;
                const float d1x = a.x - left[j].position.x;
                const float d1y = a.y - left[j].position.y;
                const float d1z = a.z - left[j].position.z;
                const Vec3& ahead = left[i + 1].position;        // one past the end for the last i
                const float ex = ahead.x - a.x;
                const float ey = ahead.y - a.y;
                const float ez = ahead.z - a.z;
                const float c1x = ey * d1z - ez * d1y;
                const float c1y = ez * d1x - d1z * ex;
                const float c1z = d1y * ex - ey * d1x;

                const Vec3& b = thirdAnchor.position;
                const float d2x = b.x - right[j].position.x;
                const float d2y = b.y - right[j].position.y;
                const float d2z = b.z - right[j].position.z;
                const Vec3& aheadRight = right[i + 1].position;
                const float fx = aheadRight.x - b.x;
                const float fy = aheadRight.y - b.y;
                const float fz = aheadRight.z - b.z;
                const float c2x = fy * d2z - fz * d2y;
                const float c2y = fz * d2x - d2z * fx;
                const float c2z = d2y * fx - fy * d2x;

                const float leftError = lengthSumZYX(c1x, c1y, c1z) / lengthSumZYX(ex, ey, ez);
#endif
                if (roadLess(m_template->getMaxTesselatedError(0), leftError))
                {
                    keep = true;
                    break;
                }
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                const float rightError = roadDeviationError(rightDeviation, 1);
#else
                const float rightError = lengthSumXZY(c2x, c2y, c2z) / lengthSumXZY(fx, fy, fz);
#endif
                if (roadLess(m_template->getMaxTesselatedError(0), rightError) || i == sampleCount - 1)
                {
                    keep = true;
                    break;
                }
            }
            if (!keep)
                continue;

            RoadEditableVertex leftVertex = leftSamples[i];
            RoadEditableVertex rightVertex = rightSamples[i];
            if (m_noise)
            {
                const float noiseX = static_cast<float>(RoadScalar(i) * 0.1f);
                leftVertex.position.x = m_noise->sample(noiseX, 0.0f, 0.0f, 0.0f) + leftVertex.position.x;
                rightVertex.position.x = m_noise->sample(noiseX + 1000.0f, 0.0f, 0.0f, 0.0f)
                                       + rightVertex.position.x;
            }
            RoadEditableVertex centreVertex = leftVertex;
            centreVertex.position = makeVec3(static_cast<float>((RoadScalar(rightVertex.position.x) + leftVertex.position.x) * 0.5f),
                                             (rightVertex.position.y + leftVertex.position.y) * 0.5f,
                                             (rightVertex.position.z + leftVertex.position.z) * 0.5f);
            const float rightLength = static_cast<float>(roadDistance3(rightVertex.position, thirdAnchor.position, 0, false));
            const RoadScalar leftLength = roadDistance3(leftVertex.position, firstAnchor.position, 0, false);
            leftVertex.uv0.x = 0.0f;
            const RoadScalar rowLength = (leftLength + rightLength) * 0.5f;
            if (mirror0)
            {
                centreVertex.uv0.x = inverseScaleU0;
                rightVertex.uv0.x = 0.0f;
            }
            else
            {
                rightVertex.uv0.x = inverseScaleU0;
                centreVertex.uv0.x = inverseScaleU0 * 0.5f;
            }
            const float v0 = static_cast<float>(RoadScalar(alongScale0) * rowLength + secondAnchor.uv0.y);
            centreVertex.uv0.y = v0;
            leftVertex.uv1.x = 0.0f;
            if (mirror1)
            {
                centreVertex.uv1.x = inverseScaleU1;
                rightVertex.uv1.x = 0.0f;
            }
            else
            {
                rightVertex.uv1.x = inverseScaleU1;
                centreVertex.uv1.x = inverseScaleU1 * 0.5f;
            }
            const float v1 = static_cast<float>(rowLength * alongScale1 + secondAnchor.uv1.y);
            rightVertex.uv0.y = v0;
            leftVertex.uv0.y = v0;
            centreVertex.uv1.y = v1;
            rightVertex.uv1.y = v1;
            leftVertex.uv1.y = v1;
            firstColumn.push_back(leftVertex);
            secondColumn.push_back(centreVertex);
            thirdColumn.push_back(rightVertex);
            firstAnchor = leftVertex;
            secondAnchor = centreVertex;
            thirdAnchor = rightVertex;
            nextRow = i + 1;
        }

        // the fade ramps
        if (m_fadeDistAtStartLeft.y != 0.0f)
            firstColumn[0].alpha = 0.0f;
        if (m_fadeDistAtStartLeft.y != 0.0f || m_fadeDistAtStartRight.y != 0.0f)
            secondColumn[0].alpha = 0.0f;
        if (m_fadeDistAtStartRight.y != 0.0f)
            thirdColumn[0].alpha = 0.0f;

        RoadScalar firstDistance = RoadScalar(0.0f);
        RoadScalar centreDistance = RoadScalar(0.0f);
        for (unsigned int k = 1; k < firstColumn.size(); ++k)
        {
            const Vec3& p = firstColumn[k].position;
            const Vec3& q = firstColumn[k - 1].position;
            firstDistance = firstDistance + roadDistance3(p, q, 0, false);
            const Vec3& r = secondColumn[k].position;
            const Vec3& u = secondColumn[k - 1].position;
            centreDistance = centreDistance + roadDistance3(r, u, 0, false);
            if (0.0f != RoadScalar(m_fadeDistAtStartLeft.y) - m_fadeDistAtStartLeft.x)
            {
                firstColumn[k].alpha = fadeRamp((firstDistance - m_fadeDistAtStartLeft.x)
                    / (RoadScalar(m_fadeDistAtStartLeft.y) - m_fadeDistAtStartLeft.x));
            }
            if (0.0f != RoadScalar(m_fadeDistAtStartRight.y) - m_fadeDistAtStartRight.x)
            {
                thirdColumn[k].alpha = fadeRamp((centreDistance - m_fadeDistAtStartRight.x)
                    / (RoadScalar(m_fadeDistAtStartRight.y) - m_fadeDistAtStartRight.x));   // [shipped] the centre's run
            }
        }
        const RoadScalar firstTotal = firstDistance;
        const float centreTotal = static_cast<float>(centreDistance);
        RoadScalar firstRun = RoadScalar(0.0f);
        RoadScalar centreRun = RoadScalar(0.0f);
        for (unsigned int k = 1; k < firstColumn.size(); ++k)
        {
            const Vec3& p = firstColumn[k].position;
            const Vec3& q = firstColumn[k - 1].position;
            firstRun = firstRun + roadDistance3(p, q, 0, false);
            const Vec3& r = secondColumn[k].position;
            const Vec3& u = secondColumn[k - 1].position;
            centreRun = centreRun + roadDistance3(r, u, 2, false);
            if (0.0f != RoadScalar(m_fadeDistAtEndLeft.y) - m_fadeDistAtEndLeft.x)
            {
                firstColumn[k].alpha = fadeRampEnd(((firstTotal - firstRun) - m_fadeDistAtEndLeft.x)
                    / (RoadScalar(m_fadeDistAtEndLeft.y) - m_fadeDistAtEndLeft.x), firstColumn[k].alpha);
            }
            if (0.0f != RoadScalar(m_fadeDistAtEndRight.y) - m_fadeDistAtEndRight.x)
            {
                thirdColumn[k].alpha = fadeRampEnd(((centreTotal - centreRun) - m_fadeDistAtEndRight.x)
                    / (RoadScalar(m_fadeDistAtEndRight.y) - m_fadeDistAtEndRight.x), thirdColumn[k].alpha);
            }
            secondColumn[k].alpha = static_cast<float>((RoadScalar(firstColumn[k].alpha) + thirdColumn[k].alpha) * 0.5f);
        }

        if (ghost)
        {
            for (unsigned int k = 0; k < firstColumn.size(); ++k)
            {
                vertices.push_back(firstColumn[k]);
                vertices.push_back(secondColumn[k]);
                vertices.push_back(thirdColumn[k]);
            }
            int centre = 1;
            for (unsigned int row = 0; row < firstColumn.size() - 1; ++row, centre += 3)
            {
                indices.push_back(centre - 1);
                indices.push_back(centre + 2);
                indices.push_back(centre);
                indices.push_back(centre);
                indices.push_back(centre + 2);
                indices.push_back(centre + 3);
                indices.push_back(centre);
                indices.push_back(centre + 3);
                indices.push_back(centre + 1);
                indices.push_back(centre + 1);
                indices.push_back(centre + 3);
                indices.push_back(centre + 4);
            }
            return static_cast<int>(4u * ((static_cast<unsigned int>(vertices.size()) - 2u) / 3u));
        }

        RoadPolygon leftHalf;
        RoadPolygon rightHalf;
        for (unsigned int k = 0; k < firstColumn.size(); ++k)
        {
            const int index = static_cast<int>(vertices.size());
            leftHalf.push_back(index);
            vertices.push_back(firstColumn[k]);
        }
        for (unsigned int k = 0; k < secondColumn.size(); ++k)
        {
            const int index = static_cast<int>(vertices.size());
            leftHalf.insert(leftHalf.begin(), index);
            const int sameIndex = static_cast<int>(vertices.size());
            rightHalf.push_back(sameIndex);
            vertices.push_back(secondColumn[k]);
        }
        for (unsigned int k = 0; k < thirdColumn.size(); ++k)
        {
            const int index = static_cast<int>(vertices.size());
            rightHalf.insert(rightHalf.begin(), index);
            vertices.push_back(thirdColumn[k]);
        }
        m_pendingPolygons.push_back(leftHalf);
        m_pendingPolygons.push_back(rightHalf);
    }

    // final road: dice, triangulate, texture, drape
    const unsigned int start = static_cast<unsigned int>(vertices.size());
    splitAndTriangulate(vertices, indices);
    assignTexCoords(firstColumn, secondColumn, thirdColumn, vertices, start, 0);
    if (mirror0 || mirror1)
        assignTexCoords(secondColumn, thirdColumn, firstColumn, vertices, start, 0);
    assignTexCoords(firstColumn, secondColumn, thirdColumn, vertices, start, (mirror0 || mirror1) ? 2 : 1);

    world::IHeightmapCluster* const cluster = static_cast<world::IHeightmapCluster*>(static_cast<void*>(
        ::dice::hfe::classManager->getSingleton(::renddx9abi::HostString("HeightmapCluster"))));   // +0x48
    for (unsigned int k = 0; k < vertices.size(); ++k)
        vertices[k].position.y = cluster->getHeightInWorldCoords(vertices[k].position);            // +0x68
    return static_cast<int>(static_cast<unsigned int>(indices.size()) / 3u);
}

} // namespace geom
} // namespace hfe
} // namespace dice
