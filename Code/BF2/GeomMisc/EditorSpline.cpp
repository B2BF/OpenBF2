// Editable spline geometry, picking and offset generation.
// Picking helpers return shared static storage and are not reentrant.
// Preserve float operation order; Hermite offsets accept an unrounded double parameter.


#ifdef BF2_IMAGE_RENDDX9

#include "EditorSpline.h"

#include "../Math/Mat4.h"
#include "../Physics/HeightmapCluster.h"      // world::IHeightmapCluster (+0x54, +0x68, +0x8C)
#include "../RendMisc/IRenderer.h"
#include "../RendMisc/IRenderView.h"
#include "../RendDX9/RendDX9Base.h"           // rendererGetRenderView, the effect / state / draw seams
#include "../RendDX9/ShaderParameter.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"        // renddx9abi::logf - dice's debug stream
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/PluginEntry.h"     // dice::hfe::classManager

#include <cfloat>
#include <cmath>
#include <cstring>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_bufferManager;       // dword_1023B978 - BufferManager.cpp
extern void* g_shaderManager;       // dword_1023B97C - ShaderManager.cpp

// BufferManager.cpp / StateManager.cpp, declared per TU as every other client does.
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const std::string& name, unsigned int usage);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);                                  // sub_10014630
bool  vertexBufferUnlock(void* wrapper);                                     // sub_10014690
void  vertexBufferBindStream(void* wrapper, unsigned int stream);            // sub_100146C0
void  vertexBufferRelease(void* wrapper);                                    // --ref, sub_10014510, free
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);                       // sub_10005870
} // namespace dx9rend

namespace geom {

// dword_1020AEFC - 81033, read as data by getClassList (0x1007DE80) and by the offset copy's
// createInstance (0x1009D555).
extern const unsigned int CID_EditorSpline = 81033u;

namespace {

// dword_1020AE70 - 80033, the id queryInterface tests after IBase's 1 and the offset copy asks for.
const unsigned int kIID_IEditorSpline = 80033u;

// flt_1020CE48 - the pick ray's length; flt_1020CE4C - the sample iterator's t step.
const float kPickRayLength   = 300.0f;
const float kSampleStep      = 0.01f;

// the "HeightmapCluster" singleton, cached by the first construction and never cleared
// (census §8: it goes stale across a level change).
world::IHeightmapCluster* s_heightmapCluster = 0;


// Vector helpers - the shipped bodies inline all of these; the operand order is theirs.

Vec3 makeVec3(float x, float y, float z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

Vec3 subtract(const Vec3& a, const Vec3& b)
{
    return makeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

// (a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x) - every cross in the file.
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return makeVec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// v * s + base, component-wise (the ribbon edges: `fld n.x; fmul h; fadd base.x`).
Vec3 scaleAdd(const Vec3& v, float s, const Vec3& base)
{
    return makeVec3(v.x * s + base.x, v.y * s + base.y, v.z * s + base.z);
}

// Normalizes in place; near-unit vectors remain unchanged and near-zero vectors become zero.
// Returns -20001 for a near-zero vector.
int normalizeInPlace(Vec3& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return 0;
    if (std::fabs(lengthSquared) < FLT_EPSILON)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return -20001;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    v.x = inverse * v.x;
    v.y = inverse * v.y;
    v.z = inverse * v.z;
    return 0;
}

// Evaluates the Catmull–Rom segment from p1 to p2 with half-scaled tangents m0 and m1.
Vec3 catmullRomPoint(const Vec3& p1, const Vec3& p2, const Vec3& m0, const Vec3& m1, double t)
{
    const float t2 = static_cast<float>(t * t);
    const float t3 = static_cast<float>(static_cast<double>(t2) * t);
    const float twoT3 = t3 + t3;
    const float threeT2 = t2 * 3.0f;                                         // flt_1020CE50
    const float h00 = (twoT3 - threeT2) + 1.0f;
    const float h01 = threeT2 - twoT3;
    const float h10 = static_cast<float>(static_cast<double>(t3 - (t2 + t2)) + t);
    const float h11 = t3 - t2;
    return makeVec3(((p2.x * h01 + p1.x * h00) + m0.x * h10 * 0.5f) + m1.x * h11 * 0.5f,
                    ((p2.y * h01 + p1.y * h00) + m0.y * h10 * 0.5f) + m1.y * h11 * 0.5f,
                    ((p2.z * h01 + p1.z * h00) + m0.z * h10 * 0.5f) + m1.z * h11 * 0.5f);
}


// The pick ray, 0x101C30B0..0x101C3620 [INFERRED placement - see the file note].

struct PickRay
{
    Vec3 origin;
    Vec3 direction;
};

dx9rend::IRenderView* s_pickView = 0;                                       // dword_10267D0C

// PE 0x10079610 (92 B) - transformPoint; z and y are formed before x is stored (out may alias in).
void transformPoint(const Mat4& m, const Vec3& in, Vec3& out)
{
    const float z = m.m[2] * in.x + m.m[10] * in.z + m.m[6] * in.y + m.m[14];
    const float y = m.m[1] * in.x + m.m[9] * in.z + m.m[5] * in.y + m.m[13];
    out.x = m.m[8] * in.z + m.m[4] * in.y + in.x * m.m[0] + m.m[12];
    out.y = y;
    out.z = z;
}

// 0x101C30B0 (`retn 4`) - the origin through transformPoint (in place), the direction through the
// 3x3 part: Z = (m10*dz + m6*dy) + m2*dx, Y likewise, X = (m8*dz + m4*dy) + dx*m0.
void transformRay(PickRay& ray, const Mat4& m)
{
    transformPoint(m, ray.origin, ray.origin);
    const float dx = ray.direction.x;
    const float dy = ray.direction.y;
    const float dz = ray.direction.z;
    const float z = (m.m[10] * dz + m.m[6] * dy) + m.m[2] * dx;
    const float y = (m.m[9] * dz + m.m[5] * dy) + m.m[1] * dx;
    const float x = (m.m[8] * dz + m.m[4] * dy) + dx * m.m[0];
    ray.direction.x = x;
    ray.direction.y = y;
    ray.direction.z = z;
}

// pixels to [-1, 1]: x = sx / (w * 0.5) - 1, y = 1 - sy / (h * 0.5). The viewport is
// asked for twice; the first answer supplies the height, the second the width.
const Vec2* screenToNdc(const Vec2* screen)
{
    static Vec2 result = { 0.0f, 0.0f };
    const float* const forHeight = s_pickView->getViewport();                // +0x14
    const float* const forWidth = s_pickView->getViewport();
    const float y = 1.0f - screen->y / (forHeight[3] * 0.5f);
    const float x = screen->x / (forWidth[2] * 0.5f) - 1.0f;
    result.x = x;
    result.y = y;
    return &result;
}

// onto the z = 1 view plane: tan(fov / 2) scales both, x also divided by the aspect.
// fptan's result is not rounded by the precision control; each product is.
const Vec2* ndcToViewPlane(const Vec2* ndc)
{
    static Vec2 result = { 0.0f, 0.0f };
    const double tanHalf = std::tan(static_cast<double>(s_pickView->getFov() * 0.5f));   // +0x20
    const float y = static_cast<float>(tanHalf * static_cast<double>(ndc->y));
    const float xUnscaled = static_cast<float>(tanHalf * static_cast<double>(ndc->x));
    const float aspect = s_pickView->getAspect();                           // +0x44
    result.y = y;
    result.x = xUnscaled / aspect;
    return &result;
}

// the view-space ray from the near plane: origin (n*px, n*py, n), direction
// (F*px - n*px, F*py - n*py, F - n) with F = far + near (sic - the far point is at far + near).
const PickRay* viewPlaneToViewRay(const Vec2* point)
{
    static PickRay result = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    const float nearPlane = s_pickView->getNearPlane();                     // +0x34
    const float farSum = s_pickView->getFarPlane() + nearPlane;             // +0x3C
    const float nearX = nearPlane * point->x;
    const float nearY = nearPlane * point->y;
    const float farX = farSum * point->x;
    const float farY = farSum * point->y;
    result.origin.z = nearPlane;
    result.direction.x = farX - nearX;
    result.direction.y = farY - nearY;
    result.direction.z = farSum - nearPlane;
    result.origin.x = nearX;
    result.origin.y = nearY;
    return &result;
}

// into the world: the view's +0x68 matrix copied twice and inverted (0x10078270), the
// ray copied and transformed by the inverse.
const PickRay* viewRayToWorld(const PickRay* ray)
{
    static PickRay result = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    Mat4 inverse = *s_pickView->getInverseViewMatrixPtr();                   // +0x68
    const Mat4 source = inverse;
    fullInvert(source, inverse);
    result = *ray;
    transformRay(result, inverse);
    return &result;
}

// Returns a world pick ray through the screen point, using the renderer's view when none is supplied.
// The returned pointer borrows static storage overwritten on the next call.
const PickRay* pickRayFromScreen(const Vec2* screen, dx9rend::IRenderView* view)
{
    static PickRay result = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    if (!view)
    {
        IBase* const renderer = classManager->getSingleton(::renddx9abi::HostString("Renderer"));
        view = static_cast<dx9rend::IRenderer*>(renderer)->getRenderView();
    }
    s_pickView = view;
    result = *viewRayToWorld(viewPlaneToViewRay(ndcToViewPlane(screenToNdc(screen))));
    return &result;
}

// Tests whether the forward ray passes within radius of centre. A NaN distance misses.
bool pickRayHitsSphere(const PickRay& ray, const Vec3& centre, float radius)
{
    float dx = centre.x - ray.origin.x;
    float dy = centre.y - ray.origin.y;
    float dz = centre.z - ray.origin.z;
    const float projection = (dz * ray.direction.z + dy * ray.direction.y) + dx * ray.direction.x;
    if (!(projection <= 0.0f))
    {
        const Vec3& d = ray.direction;
        const float scale = projection / ((d.x * d.x + d.y * d.y) + d.z * d.z);
        dx = dx - scale * d.x;
        dy = dy - scale * d.y;
        dz = dz - scale * d.z;
    }
    const float distanceSquared = (dx * dx + dz * dz) + dy * dy;
    return radius * radius >= distanceSquared;
}

} // anonymous namespace


// Construction, destruction, identity.


// Creates an empty spline and its rendering effect, caching the heightmap singleton on first use.
// Unused fields remain uninitialized.
EditorSpline::EditorSpline(int order)
    : m_refCount(1)
    , m_stepSize(3.0f)
    , m_order(order)
    , m_snapFlags(1)
    , m_dirty(0)
    , m_rebuildBuffer(0)
    , m_vertexBuffer(0)
    , m_lineWidth(0.05f)
    , m_iteratorState(2)
    , m_endState(2)
{
    m_up = makeVec3(0.0f, 1.0f, 0.0f);
    m_color.x = 1.0f;
    m_color.y = 0.0f;
    m_color.z = 0.0f;
    m_color.w = 0.5f;
    for (int i = 0; i < 4; ++i)
        m_segmentPoint[i] = makeVec3(0.0f, 0.0f, 0.0f);
    m_lastSample = makeVec3(0.0f, 0.0f, 0.0f);
    m_previousSample = makeVec3(0.0f, 0.0f, 0.0f);

    m_effect = static_cast<dx9rend::Effect*>(
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "Shaders/SplineShader"));
    m_paramWorldViewProjection =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("WorldViewProjection"));
    m_paramDiffuseColor =
        dx9rend::effectGetShaderParameterTyped(m_effect, std::string("DiffuseColor"));

    if (!s_heightmapCluster)
    {
        IBase* const cluster =
            classManager->getSingleton(::renddx9abi::HostString("HeightmapCluster"));   // +0x48
        s_heightmapCluster = static_cast<world::IHeightmapCluster*>(static_cast<void*>(cluster));
    }
}

// Releases required effect parameters and effect before member teardown. The original leaves the ribbon buffer allocated.
EditorSpline::~EditorSpline()
{
    m_paramDiffuseColor->release();
    m_paramWorldViewProjection->release();
    dx9rend::effectRelease(m_effect);
}

// +0xB0 (0x1009EEE0, `retn 4`) - the deleting destructor; answers `this`.
void* EditorSpline::destroy(unsigned int flags)
{
    this->~EditorSpline();
    if (flags & 1u)
        ::renddx9abi::engineFree(this);
    return this;
}

void EditorSpline::addRef()
{
    ++m_refCount;                                                            // 0x1000CB20 (shared)
}

int EditorSpline::getRef()
{
    return m_refCount;                                                       // 0x101A9A10 (shared)
}

// 0x1009EEA0: `--ref`; at zero the deleting destructor through the table (+0xB0, flags 1).
int EditorSpline::release()
{
    const int count = --m_refCount;
    if (!count)
        destroy(1u);
    return count;
}

// 0x1009EE50: 1 answers `this`; otherwise the branch-free "80033 ? this : 0".
void* EditorSpline::queryInterface(unsigned int iid) const
{
    EditorSpline* const self = const_cast<EditorSpline*>(this);
    if (iid == RD9_IID_IBASE)
        return static_cast<IEditorSpline*>(self);
    return iid == kIID_IEditorSpline ? static_cast<IEditorSpline*>(self) : 0;
}

int EditorSpline::pointCount() const
{
    const SplineControlPoint* const first = m_points.data();
    return first ? static_cast<int>(m_points.size()) : 0;
}

SplineControlPoint& EditorSpline::pointAt(int index)
{
    return m_points.data()[index];
}


// The small slots.


void EditorSpline::slot_0x14()
{
    // 0x1009C010 - `retn`.
}

// the ribbon width. +0x31 is set only when the width crosses zero (0 -> positive or
// positive -> 0); +0x30 always.
void EditorSpline::setLineWidth(float width)
{
    if ((m_lineWidth == 0.0f && width > 0.0f) || (m_lineWidth > 0.0f && width == 0.0f))
        m_rebuildBuffer = 1;
    m_dirty = 1;
    m_lineWidth = width;
}

float EditorSpline::getLineWidth()                  { return m_lineWidth; }

// the step; both dirty bytes.
void EditorSpline::setStepSize(float step)
{
    m_stepSize = step;
    m_rebuildBuffer = 1;
    m_dirty = 1;
}

float EditorSpline::getStepSize()                   { return m_stepSize; }

// a Vec4 copy; nothing is marked dirty (the colour is a shader constant).
void EditorSpline::setColor(const Vec4& color)
{
    m_color.x = color.x;
    m_color.y = color.y;
    m_color.z = color.z;
    m_color.w = color.w;
}

const Vec4& EditorSpline::getColor()                { return m_color; }
void EditorSpline::setSnapFlags(int flags)          { m_snapFlags = flags; }
int  EditorSpline::getSnapFlags()                   { return m_snapFlags; }
int  EditorSpline::getOrder()                       { return m_order; }

// 0x1009C0F0 (`retn 4`) - the argument is never read: the only thing this slot does is say so
// (EditorSpline.cpp:834).
void EditorSpline::setOrder(int /*order*/)
{
    ::renddx9abi::logf("EditorSpline.cpp:834: Implement first order on request...");
}


int EditorSpline::getControlPointCount()
{
    return pointCount();
}

// the record's address, unchecked.
SplineControlPoint* EditorSpline::getControlPoint(int index)
{
    return &pointAt(index);
}

// a copy of the name into the caller's return slot.
::renddx9abi::HostString EditorSpline::getName()
{
    return ::renddx9abi::toHostString(m_name);
}

// The per-point accessors, 0x1009CEE0..0x1009CF70 and 0x1009D010..0x1009D070 - one field each, no
// range check, no dirty byte.
void  EditorSpline::setControlPointSpeed(int index, float speed)      { pointAt(index).m_speed = speed; }
void  EditorSpline::setControlPointDelay(int index, float delay)      { pointAt(index).m_delay = delay; }
void  EditorSpline::setControlPointDirection(int index, int direction){ pointAt(index).m_direction = direction; }
void  EditorSpline::setControlPointFlag(int index, bool flag)         { pointAt(index).m_flag3C = flag ? 1 : 0; }
float EditorSpline::getControlPointSpeed(int index)                   { return pointAt(index).m_speed; }
float EditorSpline::getControlPointDelay(int index)                   { return pointAt(index).m_delay; }
int   EditorSpline::getControlPointDirection(int index)               { return pointAt(index).m_direction; }
void  EditorSpline::setControlPointType(int index, int type)          { pointAt(index).m_type = type; }
int   EditorSpline::getControlPointType(int index)                    { return pointAt(index).m_type; }

// 0x1009CF40 (`retn 20h`) - the sound arrives by value and is assigned to the record's string.
void EditorSpline::setControlPointSound(int index, ::renddx9abi::HostString sound)
{
    ::renddx9abi::assignFromHostString(pointAt(index).m_sound, sound);
}

// a copy of the record's string into the caller's return slot.
::renddx9abi::HostString EditorSpline::getControlPointSound(int index)
{
    return ::renddx9abi::toHostString(pointAt(index).m_sound);
}

// Updates the last point through virtual setters. Requires a nonempty spline; empty input is not guarded.
void EditorSpline::setLastControlPointSpeed(float speed)           { setControlPointSpeed(pointCount() - 1, speed); }
void EditorSpline::setLastControlPointDelay(float delay)           { setControlPointDelay(pointCount() - 1, delay); }
void EditorSpline::setLastControlPointDirection(int direction)     { setControlPointDirection(pointCount() - 1, direction); }
void EditorSpline::setLastControlPointFlag(bool flag)              { setControlPointFlag(pointCount() - 1, flag); }

// 0x1009C7F0 (`retn 1Ch`): the count is taken, the by-value string copied for the callee, and the
// wrapper's own copy destroyed after the call.
void EditorSpline::setLastControlPointSound(::renddx9abi::HostString sound)
{
    const int last = pointCount() - 1;
    setControlPointSound(last, sound);
}


// Editing.


// Appends a control point, optionally snapping height to terrain, and marks geometry dirty.
void EditorSpline::addControlPoint(const Vec3& position)
{
    Vec3 placed = position;
    if (m_snapFlags & 1)
        placed.y = s_heightmapCluster->getHeightInWorldCoords(position);
    {
        const SplineControlPoint point(placed);
        m_points.push_back(point);
    }
    m_rebuildBuffer = 1;
    m_dirty = 1;
}

// the points' clear (every destructor); both dirty bytes.
void EditorSpline::clearControlPoints()
{
    m_points.clear();
    m_rebuildBuffer = 1;
    m_dirty = 1;
}

// 0x1009CF90 (`retn 8`) - x and z copied; y from the terrain under the record (its new x/z) when snap
// bit 1 is set, else the argument's; both dirty bytes.
void EditorSpline::setControlPointPosition(int index, const Vec3& position)
{
    pointAt(index).m_position.x = position.x;
    pointAt(index).m_position.z = position.z;
    if (m_snapFlags & 2)
    {
        SplineControlPoint& point = pointAt(index);
        point.m_position.y = s_heightmapCluster->getHeightInWorldCoords(point.m_position);
    }
    else
    {
        pointAt(index).m_position.y = position.y;
    }
    m_rebuildBuffer = 1;
    m_dirty = 1;
}

// 0x1009C950 (`retn 4`) - every point's type to 2 (visible) or 3 (hidden) through +0x60; the count is
// taken once.
void EditorSpline::setAllControlPointTypes(bool visible)
{
    const int count = pointCount();
    const int type = visible ? 2 : 3;
    for (int i = 0; i < count; ++i)
        setControlPointType(i, type);
}

// every point whose type is not 2 back to 2, through +0x60; the count is taken once.
void EditorSpline::resetControlPointTypes()
{
    const int count = pointCount();
    for (int i = 0; i < count; ++i)
    {
        if (pointAt(i).m_type != 2)
            setControlPointType(i, 2);
    }
}

// Casts a normalized 300-metre, two-sided terrain ray and adds or moves a point on hit.
void EditorSpline::pickAndAddControlPoint(const Vec2& screen)
{
    const PickRay* const ray = pickRayFromScreen(&screen, 0);
    const Vec3 origin = ray->origin;
    Vec3 direction = ray->direction;
    normalizeInPlace(direction);
    const Vec3 end = scaleAdd(direction, kPickRayLength, origin);
    Vec3 hit = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
    float fraction;
    unsigned char material;
    if (s_heightmapCluster->intersectRayInWorldCoords(origin, end, hit, fraction, normal, material,
                                                      true))
        addControlPoint(hit);
}

void EditorSpline::pickAndMoveControlPoint(int index, const Vec2& screen)
{
    const PickRay* const ray = pickRayFromScreen(&screen, 0);
    const Vec3 origin = ray->origin;
    Vec3 direction = ray->direction;
    normalizeInPlace(direction);
    const Vec3 end = scaleAdd(direction, kPickRayLength, origin);
    Vec3 hit = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
    float fraction;
    unsigned char material;
    if (s_heightmapCluster->intersectRayInWorldCoords(origin, end, hit, fraction, normal, material,
                                                      true))
        setControlPointPosition(index, hit);
}

// Returns the hit control point nearest in view-space depth, or -1. Requeries the view matrix per hit.
int EditorSpline::findNearestControlPoint(const Vec2& screen)
{
    int best = -1;
    float bestDepth = FLT_MAX;
    const PickRay ray = *pickRayFromScreen(&screen, 0);
    for (unsigned int i = 0; m_points.data() && i < static_cast<unsigned int>(pointCount()); ++i)
    {
        const SplineControlPoint& point = pointAt(static_cast<int>(i));
        if (!pickRayHitsSphere(ray, point.m_position, point.m_radius))
            continue;
        const Vec3 position = pointAt(static_cast<int>(i)).m_position;
        const Mat4* const m = dx9rend::rendererGetRenderView()->getInverseViewMatrixPtr();
        const float depth = ((position.z * m->m[10] + position.y * m->m[6]) + position.x * m->m[2])
                          + m->m[14];
        if (best == -1 || depth < bestDepth)
        {
            best = static_cast<int>(i);
            bestDepth = depth;
        }
    }
    return best;
}


// Copies spline settings and control points, applying a lateral offset when requested.
// A nonzero offset requires at least three points; the class factory must return a valid spline.

IEditorSpline* EditorSpline::makeOffsetCopy(float offset)
{
    EditorSpline* const copy = static_cast<EditorSpline*>(static_cast<IEditorSpline*>(
        classManager->createInstance(CID_EditorSpline, kIID_IEditorSpline, 0)));
    copy->m_stepSize = m_stepSize;
    copy->m_dirty = 1;
    copy->m_rebuildBuffer = 1;
    copy->m_snapFlags = m_snapFlags;
    copy->m_lineWidth = m_lineWidth;
    copy->m_color.x = m_color.x;
    copy->m_color.y = m_color.y;
    copy->m_color.z = m_color.z;
    copy->m_color.w = m_color.w;
    for (unsigned int i = 0; m_points.data() + i != m_points.data() + m_points.size(); ++i)
        copy->m_points.push_back(m_points[i]);

    if (!(offset == 0.0f))
    {
        for (int i = 0; m_points.data() && static_cast<unsigned int>(i) <
                                              static_cast<unsigned int>(pointCount()); ++i)
        {
            const int count = pointCount();
            double t = 0.0;                                                  // dbl_101E0068
            Vec3 p0 = pointAt(i ? i - 1 : 0).m_position;
            Vec3 p1 = pointAt(i).m_position;
            Vec3 p2 = makeVec3(0.0f, 0.0f, 0.0f);
            Vec3 p3 = makeVec3(0.0f, 0.0f, 0.0f);
            if (static_cast<unsigned int>(i) < static_cast<unsigned int>(count - 2))
            {
                p2 = pointAt(i + 1).m_position;
                p3 = pointAt(i + 2).m_position;
            }
            else if (static_cast<unsigned int>(i) < static_cast<unsigned int>(count - 1))
            {
                p2 = pointAt(i + 1).m_position;
                p3 = pointAt(i + 1).m_position;
            }
            if (i == count - 1)
            {
                p0 = pointAt(i - 2).m_position;
                p2 = pointAt(i).m_position;
                p3 = pointAt(i).m_position;
                p1 = pointAt(i - 1).m_position;
                t = 0.999;                                                   // dbl_1020CF38
            }
            const Vec3 m0 = subtract(p2, p0);
            const Vec3 m1 = subtract(p3, p1);
            const Vec3 s0 = catmullRomPoint(p1, p2, m0, m1, t);
            t = static_cast<float>(t + 0.001);                               // dbl_1020CF30, rounded
            const Vec3 s1 = catmullRomPoint(p1, p2, m0, m1, t);

            Vec3 direction = subtract(s1, s0);
            normalizeInPlace(direction);
            const Vec3 side = cross(direction, m_up);
            Vec3& target = copy->pointAt(i).m_position;
            target.x = side.x * offset + target.x;
            target.y = side.y * offset + target.y;
            target.z = side.z * offset + target.z;
        }
    }
    return copy;
}


// The iterator RoadEditable walks: nextSegment (0x1009D230) steps the segment, nextSample (0x1009C9A0)
// the samples along it - t advances by 0.01, and the ribbon-style side normal comes with each point.


// state 2 restarts at segment 0 (clearing t, the sample count and the end state) with
// p0 = pts[0]; state 0 moves to the next segment with p0 = pts[segment - 1]; state 1 turns into 2 and
// answers false; any other state keeps p0. Then p1 = pts[segment] and, by the count: p2 = pts[s+1],
// p3 = pts[s+2] (s < count-2), or p2 = p3 = pts[s+1] (s < count-1), or the end - state 1 and end
// state 1, with p2/p3 unchanged. Every other answer is true.
bool EditorSpline::nextSegment()
{
    switch (m_iteratorState)
    {
    case 0:
        m_segment += 1;
        m_segmentPoint[0] = pointAt(m_segment - 1).m_position;
        break;
    case 1:
        m_iteratorState = 2;
        return false;
    case 2:
        m_segment = 0;
        m_iteratorState = 0;
        m_t = 0.0f;
        m_samples = 0;
        m_endState = 0;
        m_segmentPoint[0] = pointAt(0).m_position;
        break;
    default:
        break;
    }

    m_segmentPoint[1] = pointAt(m_segment).m_position;
    if (m_segment < pointCount() - 2)
    {
        m_segmentPoint[2] = pointAt(m_segment + 1).m_position;
        m_segmentPoint[3] = pointAt(m_segment + 2).m_position;
    }
    else if (m_segment < pointCount() - 1)
    {
        m_segmentPoint[2] = pointAt(m_segment + 1).m_position;
        m_segmentPoint[3] = pointAt(m_segment + 1).m_position;
    }
    else
    {
        m_iteratorState = 1;
        m_endState = 1;
    }
    return true;
}

// Writes the next sampled position and normal, including smoothing when enabled.
// Returns false after the final sample.
bool EditorSpline::nextSample(Vec3* position, Vec3* normal)
{
    if (m_endState == 0)
    {
        do
        {
            // `fcomp 1.0; test ah, 1; jz` - out only when t >= 1 (a NaN counts as below 1).
            const float t = m_t;
            if (t >= 1.0f)
            {
                m_t = t - 1.0f;
                break;
            }
            const Vec3 m0 = subtract(m_segmentPoint[2], m_segmentPoint[0]);
            const Vec3 m1 = subtract(m_segmentPoint[3], m_segmentPoint[1]);
            Vec3 sample = catmullRomPoint(m_segmentPoint[1], m_segmentPoint[2], m0, m1, t);
            if (m_snapFlags & 4)
                sample.y = s_heightmapCluster->getHeightInWorldCoords(sample);

            if (m_samples > 1)
            {
                Vec3 previousNormal = cross(subtract(m_lastSample, m_previousSample), m_up);
                Vec3 nextNormal = cross(subtract(sample, m_lastSample), m_up);
                normalizeInPlace(previousNormal);
                normalizeInPlace(nextNormal);
                Vec3 averaged = makeVec3(nextNormal.x + previousNormal.x,
                                         nextNormal.y + previousNormal.y,
                                         nextNormal.z + previousNormal.z);
                normalizeInPlace(averaged);
                if (m_samples == 2)
                {
                    *position = m_previousSample;
                    *normal = previousNormal;
                    m_samples += 1;
                    return true;
                }
                *position = m_lastSample;
                *normal = averaged;
            }

            m_t = m_t + kSampleStep;
            m_samples += 1;
            m_previousSample = m_lastSample;
            m_lastSample = sample;
            if (m_samples > 2)
                return true;
        }
        while (m_endState == 0);
    }

    if (m_endState != 1)
        return false;
    Vec3 endNormal = cross(subtract(m_lastSample, m_previousSample), m_up);
    normalizeInPlace(endNormal);
    *position = m_lastSample;
    *normal = endNormal;
    m_endState = 2;
    return true;
}


// Drawing.


// inside a state bracket: rebuild when dirty; with a buffer, technique "spline", pass 0,
// stream 0, WorldViewProjection = (+0x68 matrix) x (+0x6C matrix) of the renderer view (the +0x6C
// pointer is asked for first), DiffuseColor = the colour, and a triangle strip of (vertices - 2)
// primitives when that is positive; then every point's sphere.
void EditorSpline::draw()
{
    dx9rend::stateManagerPush();                                             // sub_100261A0
    if (m_dirty)
        rebuild();
    if (m_vertexBuffer)
    {
        dx9rend::effectSetTechniqueByName(m_effect, std::string("spline"));
        dx9rend::effectBegin(m_effect);                                      // sub_1001D780
        dx9rend::effectBeginPass(m_effect, 0);                               // sub_1001DBB0
        dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);                  // sub_100146C0
        dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView(); // renderer +0x48
        Mat4 worldViewProjection = Mat4::getIdentity();
        const Mat4* const projection = view->getProjectionMatrixPtr();      // +0x6C
        const Mat4* const viewMatrix = view->getInverseViewMatrixPtr();     // +0x68
        worldViewProjection.fullMult(*viewMatrix, *projection);
        m_paramWorldViewProjection->setValue(&worldViewProjection, 0);      // +0x18
        m_paramDiffuseColor->setValue(&m_color, 0);
        const int primitives = static_cast<int>(m_vertexCount) - 2;
        if (primitives > 0)
            dx9rend::rendererDrawPrimitive(static_cast<unsigned int>(primitives), 5u, 0u);  // strip
        dx9rend::effectEnd(m_effect);                                        // sub_1001D7E0
    }
    for (unsigned int i = 0; m_points.data() + i != m_points.data() + m_points.size(); ++i)
        m_points[i].draw();
    dx9rend::stateManagerPop();                                              // sub_100261B0
}


// Rebuilds the ribbon vertex buffer from samples spaced by the configured step.
// Requires at least two control points. Unsupported spline orders produce an empty buffer and diagnostics.

void EditorSpline::rebuild()
{
    if (!m_points.data())
        return;
    const int count = pointCount();
    if (count < 2)
        return;

    if (m_order == 1)
    {
        // Computed and discarded, as shipped: the polyline's length modulo the step.
        float remainder = 0.0f;
        const float inverseStep = 1.0f / m_stepSize;
        for (int i = 0; i < count - 1; ++i)
        {
            const Vec3 d = subtract(pointAt(i).m_position, pointAt(i + 1).m_position);
            remainder = remainder + std::sqrt((d.x * d.x + d.z * d.z) + d.y * d.y);
            const int steps = static_cast<int>(inverseStep * remainder);
            remainder = remainder - static_cast<float>(steps) * m_stepSize;
        }
        (void)remainder;
    }
    else
    {
        ::renddx9abi::logf("EditorSpline.cpp:213: Implement first order on request...");
    }
    if (m_snapFlags & 8)
        s_heightmapCluster->getPrimaryScale();                               // +0x54, unused

    std::vector<Vec3> vertices;
    unsigned int vec3Count = 0;
    if (m_order == 1)
    {
        s_heightmapCluster->getPrimaryScale();                               // +0x54, unused
        const float halfWidth = m_lineWidth * 0.5f;
        float arc = 0.0f;
        int samples = 0;
        Vec3 previous = makeVec3(0.0f, 0.0f, 0.0f);                           // the sample before last
        Vec3 last = makeVec3(0.0f, 0.0f, 0.0f);

        for (int segment = 0;; ++segment)
        {
            Vec3 p2;
            Vec3 p3;
            if (segment < count - 2)
            {
                p2 = pointAt(segment + 1).m_position;
                p3 = pointAt(segment + 2).m_position;
            }
            else if (segment < count - 1)
            {
                p2 = pointAt(segment + 1).m_position;
                p3 = pointAt(segment + 1).m_position;
            }
            else
            {
                break;
            }
            const Vec3 p0 = segment ? pointAt(segment - 1).m_position : pointAt(0).m_position;
            const Vec3 p1 = pointAt(segment).m_position;

            const Vec3 chord = subtract(p1, p2);
            const float length = std::sqrt((chord.x * chord.x + chord.z * chord.z) + chord.y * chord.y);
            const float inverseLength = 1.0f / length;
            float t = inverseLength * arc;
            if (!(t > 1.0f))
            {
                const Vec3 m0 = subtract(p2, p0);
                const Vec3 m1 = subtract(p3, p1);
                do
                {
                    Vec3 sample = catmullRomPoint(p1, p2, m0, m1, t);
                    if (m_snapFlags & 4)
                        sample.y = s_heightmapCluster->getHeightInWorldCoords(sample);

                    if (samples > 1)
                    {
                        const Vec3 along = subtract(last, previous);
                        Vec3 previousNormal = cross(along, m_up);
                        Vec3 nextNormal = cross(subtract(sample, last), m_up);
                        normalizeInPlace(previousNormal);
                        normalizeInPlace(nextNormal);
                        Vec3 averaged = makeVec3(nextNormal.x + previousNormal.x,
                                                 nextNormal.y + previousNormal.y,
                                                 nextNormal.z + previousNormal.z);
                        normalizeInPlace(averaged);

                        if (samples == 2)
                        {
                            // The start cap, at the sample before last.
                            Vec3 left = scaleAdd(previousNormal, -halfWidth, previous);
                            Vec3 right = scaleAdd(previousNormal, halfWidth, previous);
                            if (m_snapFlags & 8)
                            {
                                left.y = s_heightmapCluster->getHeightInWorldCoords(left);
                                right.y = s_heightmapCluster->getHeightInWorldCoords(right);
                            }
                            Vec3 capNormal = cross(along, subtract(right, left));
                            normalizeInPlace(capNormal);
                            vertices.push_back(left);                        // sub_10095BB0
                            vertices.push_back(capNormal);
                            vertices.push_back(right);
                            vertices.push_back(capNormal);
                        }

                        Vec3 left = scaleAdd(averaged, -halfWidth, last);
                        Vec3 right = scaleAdd(averaged, halfWidth, last);
                        if (m_snapFlags & 8)
                        {
                            left.y = s_heightmapCluster->getHeightInWorldCoords(left);
                            right.y = s_heightmapCluster->getHeightInWorldCoords(right);
                        }
                        Vec3 edgeNormal = cross(subtract(sample, last), subtract(right, left));
                        normalizeInPlace(edgeNormal);
                        vertices.push_back(left);
                        vertices.push_back(edgeNormal);
                        vertices.push_back(right);
                        vertices.push_back(edgeNormal);
                    }

                    arc = arc + m_stepSize;
                    previous = last;
                    last = sample;
                    t = inverseLength * arc;
                    ++samples;
                }
                while (!(t > 1.0f));
            }
            arc = (t - 1.0f) * length;
        }

        // The end cap, at the last sample.
        const Vec3 along = subtract(last, previous);
        Vec3 endNormal = cross(along, m_up);
        normalizeInPlace(endNormal);
        Vec3 left = scaleAdd(endNormal, -halfWidth, last);
        Vec3 right = scaleAdd(endNormal, halfWidth, last);
        if (m_snapFlags & 8)
        {
            left.y = s_heightmapCluster->getHeightInWorldCoords(left);
            right.y = s_heightmapCluster->getHeightInWorldCoords(right);
        }
        Vec3 capNormal = cross(along, subtract(right, left));
        normalizeInPlace(capNormal);
        vertices.push_back(left);
        vertices.push_back(capNormal);
        vertices.push_back(right);
        vertices.push_back(capNormal);

        m_vertexCount = 0;
        vec3Count = static_cast<unsigned int>(vertices.size());
    }
    else
    {
        ::renddx9abi::logf("EditorSpline.cpp:427: Implement first order on request...");
    }

    const unsigned int vertexCount = vec3Count >> 1;
    m_vertexCount = vertexCount;
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    m_vertexBuffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, vertexCount, 24u,
                                                 std::string("EditorSpline"), 8u);
    m_rebuildBuffer = 0;
    void* const destination = dx9rend::vertexBufferLock(m_vertexBuffer, 0u, 0u, 0u);
    std::memcpy(destination, vertices.data(), vertexCount * 24u);
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
    m_dirty = 0;
}


// RefFactory 0x1007CEA0: refuse aggregation, the object (retail 0xD4 bytes), construct with order 1,
// QI with no null test on the object (shipped), release on refusal.

IBase* __fastcall createEditorSpline(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(EditorSpline));
    EditorSpline* self = 0;
    if (storage)
        self = new (storage) EditorSpline(1);

    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));    // [eax+0Ch]
    if (!result)
        self->release();                                                     // [edx+8]
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
