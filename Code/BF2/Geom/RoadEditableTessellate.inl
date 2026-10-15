// Road spline sampling, terrain-grid clipping, triangulation and texture interpolation.
// [CHOSEN, not recovered] included split of RoadEditable.cpp; never compile it separately.
// Preserve float association/24-bit precision and the explicit double-precision sort keys.

namespace dice {
namespace hfe {
namespace geom {

namespace {


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
void lerpVec3(Vec3& out, const Vec3& a, const Vec3& b, float t)
{
    const Vec3 start = a;
    out = start;
    out.x = (b.x - start.x) * t + start.x;
    out.y = (b.y - start.y) * t + start.y;
    out.z = (b.z - start.z) * t + start.z;
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

// w = 1 - t once; each uv component is w*a + t*b (the product of a and w is formed first).
void RoadEditableVertex::interpolate(const RoadEditableVertex& a, const RoadEditableVertex& b, float t)
{
    lerpVec3(position, a.position, b.position, t);
    lerpVec3(normal, a.normal, b.normal, t);
    interpolateTexCoords(a, b, t);
}


void RoadEditableVertex::interpolateTexCoords(const RoadEditableVertex& a, const RoadEditableVertex& b,
                                              float t)
{
    const float w = 1.0f - t;                                                // flt_101DDDE4 - t
    uv0.x = w * a.uv0.x + t * b.uv0.x;
    uv0.y = w * a.uv0.y + t * b.uv0.y;
    uv1.x = w * a.uv1.x + t * b.uv1.x;
    uv1.y = w * a.uv1.y + t * b.uv1.y;
    alpha = (b.alpha - a.alpha) * t + a.alpha;
}

namespace {


// The 2D tests. Every one reads x and z only.


// v = (c.z - a.z)(b.x - a.x) - (b.z - a.z)(c.x - a.x); 1 above 0.5, -1 below -0.5 (both
// double constants), else 0 - a NaN answers 0.
int areaSign(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const float v = (c.z - a.z) * (b.x - a.x) - (b.z - a.z) * (c.x - a.x);
    if (v > 0.5)                                                             // dbl_1020D7D0
        return 1;
    if (v < -0.5)                                                            // dbl_1021D798
        return -1;
    return 0;
}

// (`retn 18h`) - segment a-b against c-d. |denominator| below FLT_EPSILON answers false
// without writing s or t; otherwise both are written, and the answer is whether both lie strictly
// inside (0, 1) - a NaN passes every range test.
bool segmentsIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float* s, float* t)
{
    const float denominator = (d.z - c.z) * (b.x - a.x) - (d.x - c.x) * (b.z - a.z);
    if (std::fabs(denominator) < FLT_EPSILON)                                // flt_101E05A0
        return false;
    const float inverse = 1.0f / denominator;
    *s = ((a.z - c.z) * (d.x - c.x) - (d.z - c.z) * (a.x - c.x)) * inverse;
    const float tValue = inverse * ((a.z - c.z) * (b.x - a.x) - (b.z - a.z) * (a.x - c.x));
    *t = tValue;
    if (tValue <= 0.0f || tValue >= 1.0f)
        return false;
    if (*s <= 0.0f || *s >= 1.0f)
        return false;
    return true;
}

// is c on segment a-b? Along x unless a.x == b.x (then along z); inclusive both ways.
int between(const Vec3& a, const Vec3& b, const Vec3& c)
{
    if (a.x != b.x)
        return (a.x <= c.x && c.x <= b.x) || (a.x >= c.x && c.x >= b.x);
    return (a.z <= c.z && c.z <= b.z) || (a.z >= c.z && c.z >= b.z);
}

// (`retn 14h`) - the collinear case of segmentIntersection: '0' unless c is on the line
// a-b; then the first of c, d (on a-b) or a, b (on c-d) that lies on the other segment, copied to p,
// with 'e'.
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
// and t (along c-d) are stored as doubles but carry float precision - the inverse is formed at 24
// bits (`fld 1.0; fdiv`) and kept as a float (`fst` to a dword). The point is lerp(a, b, s).
// The collinear arm returns from parallelIntersection without writing s or t (shipped).
char segmentIntersection(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, Vec3& p,
                         double& s, double& t)
{
    char code = '?';
    const float denominator = (((a.z - b.z) * c.x + (c.z - d.z) * b.x) + (b.z - a.z) * d.x)
                            + (d.z - c.z) * a.x;
    if (denominator == 0.0)
        return parallelIntersection(a, b, c, d, p);

    const float numerator = ((c.z - a.z) * d.x + (a.z - d.z) * c.x) + (d.z - c.z) * a.x;
    if (numerator == 0.0 || numerator == denominator)
        code = 'v';
    const float inverse = 1.0f / denominator;                                // dbl_101E0260 / denominator
    s = static_cast<double>(inverse * numerator);

    const float numerator2 = -(((b.z - a.z) * c.x + (a.z - c.z) * b.x) + (c.z - b.z) * a.x);
    if (numerator2 == 0.0 || numerator2 == denominator)
        code = 'v';
    const float tValue = numerator2 * inverse;
    t = static_cast<double>(tValue);

    if (0.0 < s && s < 1.0 && 0.0 < tValue && tValue < 1.0)
        code = '1';
    else if (s < 0.0 || s > 1.0 || tValue < 0.0 || tValue > 1.0)
        code = '0';

    lerpVec3(p, a, b, static_cast<float>(s));
    return code;
}

// v = (c.x - a.x)(b.z - a.z) - (c.z - a.z)(b.x - a.x); false only when v < 0 (a NaN is
// "left").
bool leftOn(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const float v = (c.x - a.x) * (b.z - a.z) - (c.z - a.z) * (b.x - a.x);
    return !(v < 0.0f);
}

// the two edge tests a-b and b-c, then leftOn(c, a, p).
bool pointInTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& p)
{
    if ((b.z - a.z) * (p.x - a.x) - (p.z - a.z) * (b.x - a.x) < 0.0f)
        return false;
    if ((c.z - b.z) * (p.x - b.x) - (p.z - b.z) * (c.x - b.x) < 0.0f)
        return false;
    return leftOn(c, a, p);
}

// the leftOn value's sign into `side`: -1 below 0, 1 above, 0 otherwise (NaN too).
void orient(const Vec3& a, const Vec3& b, const Vec3& c, int& side)
{
    const float v = (c.x - a.x) * (b.z - a.z) - (c.z - a.z) * (b.x - a.x);
    if (v < 0.0f)
        side = -1;
    else if (v > 0.0f)
        side = 1;
    else
        side = 0;
}


// Interpolates target texture coordinates in the x/z triangle; returns false outside it.
// Out-of-range ratios repeat the edge calculation but still use the original result.
// [CHOSEN] A degenerate parallel intersection uses zero for the unwritten edge parameter.

bool interpolateInTriangle(const RoadEditableVertex& a, const RoadEditableVertex& b,
                           const RoadEditableVertex& c, RoadEditableVertex& target)
{
    const float winding = ((a.position.z * c.position.x - a.position.x * c.position.z)
                         + (a.position.x * b.position.z - a.position.z * b.position.x))
                        + (b.position.x * c.position.z - c.position.x * b.position.z);
    RoadEditableVertex p0;
    RoadEditableVertex p1;
    RoadEditableVertex p2;
    p0 = a;
    if (winding > 0.0f)                              // `test ah, 41h; jnz` - zero and NaN take b, c
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
    double s = 0.0;
    double t = 0.0;
    segmentIntersection(p2.position, p1.position, p0.position, target.position, point, s, t);
    RoadEditableVertex edge;
    edge.interpolate(p2, p1, static_cast<float>(s));
    const float ratio = lengthXZ(target.position.x - p0.position.x, target.position.z - p0.position.z)
                      / lengthXZ(edge.position.x - p0.position.x, edge.position.z - p0.position.z);
    if (ratio < 0.0 || ratio > 1.0)
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
float polygonArea(const RoadPolygon& polygon, const std::vector<RoadEditableVertex>& vertices)
{
    const unsigned int count = static_cast<unsigned int>(polygon.size());
    float area = 0.0f;
    unsigned int previous = count - 1;
    for (unsigned int i = 0; i < count; ++i)
    {
        const Vec3& current = vertices[polygon[i]].position;
        const Vec3& before = vertices[polygon[previous]].position;
        area = area + (current.z * before.x - before.z * current.x);
        previous = i;
    }
    return area * 0.5f;                                                      // flt_101DD4F0
}

// (`retn 8`) - a positive area (NaN and zero are left alone) reverses the polygon: a copy,
// clear() (VC7.1: the block freed), then the copy pushed back from its end.
void ensureClockwise(RoadPolygon& polygon, const std::vector<RoadEditableVertex>& vertices)
{
    if (!(polygonArea(polygon, vertices) > 0.0f))
        return;
    const RoadPolygon copy(polygon);
    polygon.clear();
    for (int i = static_cast<int>(copy.size()) - 1; i >= 0; --i)
        polygon.push_back(copy[static_cast<unsigned int>(i)]);
}

// (`retn 14h`) - a, b, c are vertex indices. Not an ear when the corner turns the wrong
// way ((b.z - a.z)(c.x - a.x) - (c.z - a.z)(b.x - a.x) < 0) or when any other live polygon vertex
// (not -1, not a, b or c) lies in the triangle.
bool isEar(const RoadPolygon& polygon, int a, int b, int c, const std::vector<RoadEditableVertex>& vertices)
{
    const Vec3& pa = vertices[a].position;
    const Vec3& pb = vertices[b].position;
    const Vec3& pc = vertices[c].position;
    if ((pb.z - pa.z) * (pc.x - pa.x) - (pc.z - pa.z) * (pb.x - pa.x) < 0.0f)
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
void emitTriangle(int a, int b, int c, const std::vector<RoadEditableVertex>& vertices,
                  std::vector<int>& indices)
{
    const Vec3& pa = vertices[a].position;
    const Vec3& pb = vertices[b].position;
    const Vec3& pc = vertices[c].position;
    const float r = (pb.x - pc.x) * (pa.z - pc.z) - (pb.z - pc.z) * (pa.x - pc.x);
    if (r > 0.0f)
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
void earClip(RoadPolygon& polygon, const std::vector<RoadEditableVertex>& vertices, std::vector<int>& indices)
{
    if (polygon.size() < 3)
    {
        ::renddx9abi::logf("RoadEditable.cpp:501: TBM: npoly with less then 3 vertex!");
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
int extremeMinZ(RoadPolygon polygon, const std::vector<RoadEditableVertex>& vertices, float tolerance)
{
    float minimum = vertices[polygon[0]].position.z;
    for (unsigned int i = 1; i < polygon.size(); ++i)
    {
        const float z = vertices[polygon[i]].position.z;
        if (!(minimum < z))
            minimum = z;
    }
    const int n = static_cast<int>(std::floor(static_cast<double>(minimum + tolerance)));
    return n - std::abs(n) % 2;
}

// the maximum x, even upward.
int extremeMaxX(RoadPolygon polygon, const std::vector<RoadEditableVertex>& vertices, float tolerance)
{
    float maximum = vertices[polygon[0]].position.x;
    for (unsigned int i = 1; i < polygon.size(); ++i)
    {
        const float x = vertices[polygon[i]].position.x;
        if (maximum < x)
            maximum = x;
    }
    int n = static_cast<int>(std::ceil(static_cast<double>(maximum - tolerance)));
    const int odd = std::abs(n) % 2;
    if (odd)
        n = n - odd + 2;
    return n;
}

// the maximum z, even upward.
int extremeMaxZ(RoadPolygon polygon, const std::vector<RoadEditableVertex>& vertices, float tolerance)
{
    float maximum = vertices[polygon[0]].position.z;
    for (unsigned int i = 1; i < polygon.size(); ++i)
    {
        const float z = vertices[polygon[i]].position.z;
        if (maximum < z)
            maximum = z;
    }
    int n = static_cast<int>(std::ceil(static_cast<double>(maximum - tolerance)));
    const int odd = std::abs(n) % 2;
    if (odd)
        n = n - odd + 2;
    return n;
}

// the minimum x, even downward.
int extremeMinX(RoadPolygon polygon, const std::vector<RoadEditableVertex>& vertices, float tolerance)
{
    float minimum = vertices[polygon[0]].position.x;
    for (unsigned int i = 1; i < polygon.size(); ++i)
    {
        const float x = vertices[polygon[i]].position.x;
        if (!(minimum < x))
            minimum = x;
    }
    const int n = static_cast<int>(std::floor(static_cast<double>(minimum + tolerance)));
    return n - std::abs(n) % 2;
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
bool classifyPolygon(const RoadPolygon& polygon, PolygonCut& cut, const std::vector<RoadEditableVertex>& vertices)
{
    const int minX = extremeMinX(polygon, vertices, 0.01f);
    const int maxXLessTwo = extremeMaxX(polygon, vertices, 0.01f) - 2;
    const int minZ = extremeMinZ(polygon, vertices, 0.01f);
    const int maxZ = extremeMaxZ(polygon, vertices, 0.01f);
    if (minX != maxXLessTwo)
    {
        cut.x = static_cast<float>(minX + 2);
        cut.z = 0.0f;
        cut.axis = 0;
        return true;
    }
    if (minZ != maxZ - 2)
    {
        cut.x = 0.0f;
        cut.axis = 1;
        cut.z = static_cast<float>(minZ + 2);
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


// Splits polygon across line a-b, appending intersection vertices and output pieces.
// Returns false for no crossings or an odd crossing count. Requires at least three
// vertices and finite coordinates; endpoint hits use a one-centimetre tolerance.

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

    double s = 0.0;
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
            if (lengthXZ(crossing.position.x - to.position.x, crossing.position.z - to.position.z) < 0.01)
            {
                t = 1.0;                                 // the end vertex is hit - tested below
            }
            else if (lengthXZ(crossing.position.x - from.position.x,
                              crossing.position.z - from.position.z) < 0.01)
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

        // |t - 1| at 24 bits (`fsubr` against the double 1.0); a NaN skips.
        if (!(std::fabs(static_cast<float>(t - 1.0)) < 0.01))
            continue;

        const float abX = b.x - a.x;
        const float abZ = b.z - a.z;
        const float sideOfStart = (from.position.x - a.x) * abZ - abX * (from.position.z - a.z);
        int sideBefore;
        if (sideOfStart < 0.0f)
            sideBefore = -1;
        else if (sideOfStart > 0.0f)
            sideBefore = 1;
        else
            continue;                                    // the start vertex is on the line (or NaN)

        Vec3 after = makeVec3(0.0f, 0.0f, 0.0f);
        unsigned int k = (i + 2) % count;
        if (k != i)
        {
            for (;;)
            {
                after = vertices[polygon[k]].position;
                const float side = (after.x - a.x) * abZ - abX * (after.z - a.z);
                if (side < 0.0f)
                {
                    sideAfter = -1;
                    break;
                }
                if (side > 0.0f)
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
            if (crossingParameter[m] != -1.0 && crossingParameter[m] < best)
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
// [CHOSEN] Uninitialized compatibility fallback indices and intersection values start at zero.

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
                        lengthXZ(v.position.x - l0.position.x, v.position.z - l0.position.z)
                        / lengthXZ(l1.position.x - l0.position.x, l1.position.z - l0.position.z));
                    v.needsUv = 0;
                    break;
                }
                orient(l1.position, r1.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(l1, r1,
                        lengthXZ(v.position.x - l1.position.x, v.position.z - l1.position.z)
                        / lengthXZ(r1.position.x - l1.position.x, r1.position.z - l1.position.z));
                    v.needsUv = 0;
                    break;
                }
                orient(r1.position, r0.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(r1, r0,
                        lengthXZ(v.position.x - r1.position.x, v.position.z - r1.position.z)
                        / lengthXZ(r0.position.x - r1.position.x, r0.position.z - r1.position.z));
                    v.needsUv = 0;
                    break;
                }
                orient(r0.position, l0.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(r0, l0,
                        lengthXZ(v.position.x - r0.position.x, v.position.z - r0.position.z)
                        / lengthXZ(l0.position.x - r0.position.x, l0.position.z - r0.position.z));
                    v.needsUv = 0;
                    break;
                }
                orient(l0.position, r1.position, v.position, side);
                if (side == 0)
                {
                    v.interpolateTexCoords(l0, r1,
                        lengthXZ(v.position.x - l0.position.x, v.position.z - l0.position.z)
                        / lengthXZ(r1.position.x - l0.position.x, r1.position.z - l0.position.z));
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
        unsigned int nearest = 0;                    // shipped: the third column's address (unset)
        int side = static_cast<int>(start);          // shipped: the `start` argument slot
        float s = 0.0f;                              // shipped: an unwritten stack word
        float t = 0.0f;
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
                const float toFirst = lengthSumXZY(p1.x - v.position.x, p1.y - v.position.y,
                                                   p1.z - v.position.z);
                const float toSecond = lengthSumXZY(p2.x - v.position.x, p2.y - v.position.y,
                                                    p2.z - v.position.z);
                if (toFirst < best)
                {
                    nearest = i;
                    best = toFirst;
                    side = 1;
                }
                if (toSecond < best)
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
                const float toPrevious = lengthSumZYX(previous.position.x - v.position.x,
                                                      previous.position.y - v.position.y,
                                                      previous.position.z - v.position.z);
                const float toFollowing = lengthSumZYX(following.position.x - v.position.x,
                                                       following.position.y - v.position.y,
                                                       following.position.z - v.position.z);
                neighbour = (toFollowing > toPrevious) ? nearest - 1 : nearest + 1;
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
            const float ratio = lengthXZ(v.position.x - closest.position.x, v.position.z - closest.position.z)
                              / lengthXZ(hit.position.x - closest.position.x, hit.position.z - closest.position.z);
            v.interpolateTexCoords(closest, hit, ratio);
        }
        return;
    }

    if (mode == 2)
    {
        unsigned int nearest = start;                // shipped: the `start` argument slot
        int side = mode;                             // shipped: the `mode` argument slot
        float s = 0.0f;                              // shipped: an unwritten stack word
        float t = 0.0f;
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
                const float toFirst = lengthSumXZY(p1.x - v.position.x, p1.y - v.position.y,
                                                   p1.z - v.position.z);
                const float toSecond = lengthSumXZY(p2.x - v.position.x, p2.y - v.position.y,
                                                    p2.z - v.position.z);
                const float toThird = lengthSumXZY(p2.x - v.position.x, p2.y - v.position.y,
                                                   p2.z - v.position.z);   // [shipped] the centre again
                if (toFirst < best)
                {
                    nearest = i;
                    best = toFirst;
                    side = 1;
                }
                if (toSecond < best)
                {
                    nearest = i;
                    best = toSecond;
                    side = 21;
                    if (!(toFirst < toThird))
                        side = 23;
                }
                if (toThird < best)
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
                const float toPrevious = lengthSumXZY(previous.position.x - v.position.x,
                                                      previous.position.y - v.position.y,
                                                      previous.position.z - v.position.z);
                const float toFollowing = lengthSumXZY(following.position.x - v.position.x,
                                                       following.position.y - v.position.y,
                                                       following.position.z - v.position.z);
                neighbour = (toFollowing > toPrevious) ? nearest - 1 : nearest + 1;
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
            const float ratio = lengthXZ(v.position.x - closest.position.x, v.position.z - closest.position.z)
                              / lengthXZ(hit.position.x - closest.position.x, hit.position.z - closest.position.z);
            v.interpolateTexCoords(closest, hit, ratio);
        }
    }
}

// The fade ramps' clamps, each exactly as its arm compares (NaN results noted per form).
// First pass: t < 1 ? (0 < t ? t : 0) : 1 - a NaN becomes 1.
float fadeRamp(float t)
{
    if (!(t < 1.0f))
        return 1.0f;
    return (0.0f < t) ? t : 0.0f;
}

// Second pass: min(min(t, 1), current) against 0 - a NaN t becomes 1, a NaN current is replaced.
float fadeRampEnd(float t, float current)
{
    if (!(t < 1.0f))
        t = 1.0f;
    if (current < t)
        t = current;
    return (0.0f < t) ? t : 0.0f;
}

} // anonymous namespace


// (`retn 8`) - drain the pending list: a polygon of three or more vertices that spans more
// than one 2 m cell is cut at the first grid line inside (classifyPolygon; the cut line runs 1 m past
// the polygon's even-rounded extent) and its pieces go back on the pending list; a single cell is cut
// along its diagonal (from (minX - 1, maxZ + 1) to (maxX + 1, minZ - 1), extents taken with 1 cm of
// tolerance) and the pieces go to the ready list; a polygon the cut misses goes to the ready list
// whole. Then every ready polygon of three or more vertices is ear-clipped into `indices`. Polygons
// with fewer vertices are dropped from both lists unprocessed.

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
                    lineStart = makeVec3(cut.x, 0.0f, static_cast<float>(maxZ) + 1.0f);
                    const int minZ = extremeMinZ(polygon, vertices, 0.0f);
                    lineEnd = makeVec3(cut.x, 0.0f, static_cast<float>(minZ) - 1.0f);
                }
                else
                {
                    const int maxX = extremeMaxX(polygon, vertices, 0.0f);
                    lineStart = makeVec3(static_cast<float>(maxX) + 1.0f, 0.0f, cut.z);
                    const int minX = extremeMinX(polygon, vertices, 0.0f);
                    lineEnd = makeVec3(static_cast<float>(minX) - 1.0f, 0.0f, cut.z);
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
                const Vec3 lineStart = makeVec3(static_cast<float>(minX) - 1.0f, 0.0f,
                                                static_cast<float>(maxZ) + 1.0f);
                const int minZ = extremeMinZ(polygon, vertices, 0.01f);
                const int maxX = extremeMaxX(polygon, vertices, 0.01f);
                const Vec3 lineEnd = makeVec3(static_cast<float>(maxX) + 1.0f, 0.0f,
                                              static_cast<float>(minZ) - 1.0f);
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
    const float scaleV1 = offset1.y + scale1.y;
    const float inverseWidth = 1.0f / width;
    const float alongScale0 = scaleU0 * inverseWidth / scaleV0;
    const float alongScale1 = inverseWidth * scaleU1 / scaleV1;
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
                if (m_template->getMaxTesselatedError(0) < leftError)        // +0x34
                {
                    keep = true;
                    break;
                }
                const float rightError = lengthSumZYX(c2x, c2y, c2z) / lengthSumZYX(fx, fy, fz);
                if (m_template->getMaxTesselatedError(0) < rightError || i == leftSamples.size() - 1)
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
                const float noiseX = static_cast<float>(i) * 0.1f;           // flt_101DD25C
                leftVertex.position.x = m_noise->sample(noiseX, 0.0f, 0.0f, 0.0f) + leftVertex.position.x;
                rightVertex.position.x = m_noise->sample(noiseX + 1000.0f, 0.0f, 0.0f, 0.0f)
                                       + rightVertex.position.x;
            }
            const float dx = rightVertex.position.x - secondAnchor.position.x;
            const float dy = rightVertex.position.y - secondAnchor.position.y;
            const float dz = rightVertex.position.z - secondAnchor.position.z;
            rightVertex.uv0.x = inverseScaleU0;
            rightVertex.uv1.x = inverseScaleU1;
            leftVertex.uv0.x = 0.0f;
            leftVertex.uv1.x = 0.0f;
            const float rowLength = lengthSumZYX(dx, dy, dz);
            const float v0 = alongScale0 * rowLength + secondAnchor.uv0.y;
            rightVertex.uv0.y = v0;
            const float v1 = alongScale1 * rowLength + secondAnchor.uv1.y;
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

        float firstDistance = 0.0f;
        float secondDistance = 0.0f;
        for (unsigned int k = 1; k < firstColumn.size(); ++k)
        {
            const Vec3& p = firstColumn[k].position;
            const Vec3& q = firstColumn[k - 1].position;
            firstDistance = firstDistance + lengthSumZYX(p.x - q.x, p.y - q.y, p.z - q.z);
            const Vec3& r = secondColumn[k].position;
            const Vec3& u = secondColumn[k - 1].position;
            secondDistance = secondDistance + lengthSumZYX(r.x - u.x, r.y - u.y, r.z - u.z);
            if (0.0f != m_fadeDistAtStartLeft.y - m_fadeDistAtStartLeft.x)
            {
                firstColumn[k].alpha = fadeRamp((firstDistance - m_fadeDistAtStartLeft.x)
                    / (m_fadeDistAtStartLeft.y - m_fadeDistAtStartLeft.x));
            }
            if (0.0f != m_fadeDistAtStartRight.y - m_fadeDistAtStartRight.x)
            {
                secondColumn[k].alpha = fadeRamp((secondDistance - m_fadeDistAtStartRight.x)
                    / (m_fadeDistAtStartRight.y - m_fadeDistAtStartRight.x));
            }
        }
        const float firstTotal = firstDistance;
        const float secondTotal = secondDistance;
        float firstRun = 0.0f;
        float secondRun = 0.0f;
        for (unsigned int k = 1; k < firstColumn.size(); ++k)
        {
            const Vec3& p = firstColumn[k].position;
            const Vec3& q = firstColumn[k - 1].position;
            firstRun = firstRun + lengthSumXZY(p.x - q.x, p.y - q.y, p.z - q.z);
            const Vec3& r = secondColumn[k].position;
            const Vec3& u = secondColumn[k - 1].position;
            secondRun = secondRun + lengthSumXZY(r.x - u.x, r.y - u.y, r.z - u.z);
            if (0.0f != m_fadeDistAtEndLeft.y - m_fadeDistAtEndLeft.x)
            {
                firstColumn[k].alpha = fadeRampEnd(((firstTotal - firstRun) - m_fadeDistAtEndLeft.x)
                    / (m_fadeDistAtEndLeft.y - m_fadeDistAtEndLeft.x), firstColumn[k].alpha);
            }
            if (0.0f != m_fadeDistAtEndRight.y - m_fadeDistAtEndRight.x)
            {
                secondColumn[k].alpha = fadeRampEnd(((secondTotal - secondRun) - m_fadeDistAtEndRight.x)
                    / (m_fadeDistAtEndRight.y - m_fadeDistAtEndRight.x), secondColumn[k].alpha);
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
        secondAnchor.position = makeVec3((thirdAnchor.position.x + firstAnchor.position.x) * 0.5f,
                                         (thirdAnchor.position.y + firstAnchor.position.y) * 0.5f,
                                         (thirdAnchor.position.z + firstAnchor.position.z) * 0.5f);
        firstAnchor.uv1.x = 0.0f;
        firstAnchor.uv1.y = 0.0f;
        firstAnchor.uv0.x = 0.0f;
        firstAnchor.uv0.y = 0.0f;
        thirdAnchor.uv0.y = 0.0f;
        secondAnchor.uv0.y = 0.0f;
        const float inverseScaleU0 = 1.0f / scaleU0;
        if (mirror0)
        {
            secondAnchor.uv0.x = inverseScaleU0;
            thirdAnchor.uv0.x = 0.0f;
        }
        else
        {
            secondAnchor.uv0.x = 0.5f * inverseScaleU0;
            thirdAnchor.uv0.x = inverseScaleU0;
        }
        const float inverseScaleU1 = 1.0f / scaleU1;
        thirdAnchor.uv1.y = 0.0f;
        secondAnchor.uv1.y = 0.0f;
        if (mirror1)
        {
            secondAnchor.uv1.x = inverseScaleU1;
            thirdAnchor.uv1.x = 0.0f;
        }
        else
        {
            secondAnchor.uv1.x = 0.5f * inverseScaleU1;
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
                if (m_template->getMaxTesselatedError(0) < leftError)
                {
                    keep = true;
                    break;
                }
                const float rightError = lengthSumXZY(c2x, c2y, c2z) / lengthSumXZY(fx, fy, fz);
                if (m_template->getMaxTesselatedError(0) < rightError || i == sampleCount - 1)
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
                const float noiseX = static_cast<float>(i) * 0.1f;
                leftVertex.position.x = m_noise->sample(noiseX, 0.0f, 0.0f, 0.0f) + leftVertex.position.x;
                rightVertex.position.x = m_noise->sample(noiseX + 1000.0f, 0.0f, 0.0f, 0.0f)
                                       + rightVertex.position.x;
            }
            RoadEditableVertex centreVertex = leftVertex;
            centreVertex.position = makeVec3((rightVertex.position.x + leftVertex.position.x) * 0.5f,
                                             (rightVertex.position.y + leftVertex.position.y) * 0.5f,
                                             (rightVertex.position.z + leftVertex.position.z) * 0.5f);
            const float rightLength = lengthSumZYX(rightVertex.position.x - thirdAnchor.position.x,
                                                   rightVertex.position.y - thirdAnchor.position.y,
                                                   rightVertex.position.z - thirdAnchor.position.z);
            const float leftLength = lengthSumZYX(leftVertex.position.x - firstAnchor.position.x,
                                                  leftVertex.position.y - firstAnchor.position.y,
                                                  leftVertex.position.z - firstAnchor.position.z);
            leftVertex.uv0.x = 0.0f;
            const float rowLength = (leftLength + rightLength) * 0.5f;
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
            const float v0 = alongScale0 * rowLength + secondAnchor.uv0.y;
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
            const float v1 = rowLength * alongScale1 + secondAnchor.uv1.y;
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

        float firstDistance = 0.0f;
        float centreDistance = 0.0f;
        for (unsigned int k = 1; k < firstColumn.size(); ++k)
        {
            const Vec3& p = firstColumn[k].position;
            const Vec3& q = firstColumn[k - 1].position;
            firstDistance = firstDistance + lengthSumZYX(p.x - q.x, p.y - q.y, p.z - q.z);
            const Vec3& r = secondColumn[k].position;
            const Vec3& u = secondColumn[k - 1].position;
            centreDistance = centreDistance + lengthSumZYX(r.x - u.x, r.y - u.y, r.z - u.z);
            if (0.0f != m_fadeDistAtStartLeft.y - m_fadeDistAtStartLeft.x)
            {
                firstColumn[k].alpha = fadeRamp((firstDistance - m_fadeDistAtStartLeft.x)
                    / (m_fadeDistAtStartLeft.y - m_fadeDistAtStartLeft.x));
            }
            if (0.0f != m_fadeDistAtStartRight.y - m_fadeDistAtStartRight.x)
            {
                thirdColumn[k].alpha = fadeRamp((centreDistance - m_fadeDistAtStartRight.x)
                    / (m_fadeDistAtStartRight.y - m_fadeDistAtStartRight.x));   // [shipped] the centre's run
            }
        }
        const float firstTotal = firstDistance;
        const float centreTotal = centreDistance;
        float firstRun = 0.0f;
        float centreRun = 0.0f;
        for (unsigned int k = 1; k < firstColumn.size(); ++k)
        {
            const Vec3& p = firstColumn[k].position;
            const Vec3& q = firstColumn[k - 1].position;
            firstRun = firstRun + lengthSumZYX(p.x - q.x, p.y - q.y, p.z - q.z);
            const Vec3& r = secondColumn[k].position;
            const Vec3& u = secondColumn[k - 1].position;
            centreRun = centreRun + lengthSumYXZ(r.x - u.x, r.y - u.y, r.z - u.z);
            if (0.0f != m_fadeDistAtEndLeft.y - m_fadeDistAtEndLeft.x)
            {
                firstColumn[k].alpha = fadeRampEnd(((firstTotal - firstRun) - m_fadeDistAtEndLeft.x)
                    / (m_fadeDistAtEndLeft.y - m_fadeDistAtEndLeft.x), firstColumn[k].alpha);
            }
            if (0.0f != m_fadeDistAtEndRight.y - m_fadeDistAtEndRight.x)
            {
                thirdColumn[k].alpha = fadeRampEnd(((centreTotal - centreRun) - m_fadeDistAtEndRight.x)
                    / (m_fadeDistAtEndRight.y - m_fadeDistAtEndRight.x), thirdColumn[k].alpha);
            }
            secondColumn[k].alpha = (firstColumn[k].alpha + thirdColumn[k].alpha) * 0.5f;
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
