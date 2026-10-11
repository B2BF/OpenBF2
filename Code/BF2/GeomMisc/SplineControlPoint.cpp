// Spline control-point rendering and editor state.


#ifdef BF2_IMAGE_RENDDX9

#include "SplineControlPoint.h"

#include "../Math/Mat4.h"
#include "../RendMisc/IRenderView.h"
#include "../RendDX9/RendDX9Base.h"        // d3d9.h, renderDevice, the effect and draw seams
#include "../RendDX9/ShaderParameter.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_shaderManager;       // dword_1023B97C - ShaderManager.cpp
// Named after the D3D call, not the shipped push order - see StateManager.cpp.
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex);                  // sub_10005820
}

namespace geom {

namespace {

// 0x1024B59C..0x1024B5A8 - the shared sphere and its effect handles.
ID3DXMesh*                  s_sphere = 0;           // dword_1024B59C
dx9rend::Effect*            s_effect = 0;           // dword_1024B5A0
dx9rend::ShaderParameter*   s_worldViewProjection = 0;   // dword_1024B5A4
dx9rend::ShaderParameter*   s_diffuseColor = 0;     // dword_1024B5A8

// Postmultiplies by a scale matrix, including translation; preserve product order for floating-point behavior.
void postScale(Mat4& matrix, const Vec3& s)
{
    float* const m = matrix.m;
    m[0] = s.x * m[0];
    m[4] = m[4] * s.x;
    m[8] = s.x * m[8];
    m[12] = m[12] * s.x;
    m[1] = s.y * m[1];
    m[5] = s.y * m[5];
    m[9] = s.y * m[9];
    m[13] = s.y * m[13];
    m[2] = s.z * m[2];
    m[6] = m[6] * s.z;
    m[10] = m[10] * s.z;
    m[14] = m[14] * s.z;
}

// Every construction after the first, and every copy: one reference on each shared handle, in the
// shipped order - the effect (+0x00), the two parameters (+0x04), the mesh (IUnknown::AddRef).
void addSharedReferences()
{
    dx9rend::effectAddRef(s_effect);
    s_worldViewProjection->addRef();
    s_diffuseColor->addRef();
    s_sphere->AddRef();
}

} // anonymous namespace

// Initializes point position, radius, and timing, preserving repeated assignments. The trailing byte remains unset.
SplineControlPoint::SplineControlPoint(const Vec3& position)
    : m_position(position)
    , m_radius(0.3f)
    , m_type(2)
    , m_speed(60.0f)
    , m_direction(0)
    , m_sound("")
    , m_delay(0.0f)
{
    m_speed = 60.0f;
    m_direction = 0;
    m_sound = "";
    m_delay = 0.0f;

    if (!s_sphere)
    {
        // The first point creates the sphere: D3DXCreateSphere(device, 1.0f, 5, 5) into a local,
        // cloned into the static as managed write-only XYZ (options 0x660, FVF 2), the local
        // released; then the effect and its two parameters. No result is tested (shipped).
        IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice());
        ID3DXMesh* source = 0;
        D3DXCreateSphere(device, 1.0f, 5u, 5u, &source, 0);
        source->CloneMeshFVF(0x660u, D3DFVF_XYZ, device, &s_sphere);
        source->Release();
        s_effect = static_cast<dx9rend::Effect*>(
            dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "Shaders/SplineShader"));
        s_worldViewProjection =
            dx9rend::effectGetShaderParameterTyped(s_effect, vc71::string("WorldViewProjection"));
        s_diffuseColor =
            dx9rend::effectGetShaderParameterTyped(s_effect, vc71::string("DiffuseColor"));
    }
    else
    {
        addSharedReferences();
    }
}

// Copies fields and retains shared handles. The trailing byte remains unset; shared mesh storage must be initialized.
SplineControlPoint::SplineControlPoint(const SplineControlPoint& other)
    : m_position(other.m_position)
    , m_radius(other.m_radius)
    , m_type(other.m_type)
    , m_speed(other.m_speed)
    , m_direction(other.m_direction)
    , m_sound(other.m_sound)
    , m_delay(other.m_delay)
{
    m_speed = other.m_speed;
    m_direction = other.m_direction;
    m_delay = other.m_delay;
    m_sound = other.m_sound;
    addSharedReferences();
}

// release DiffuseColor, WorldViewProjection and the effect (+0x08 each, no null tests),
// then the mesh: a null static is skipped, and the static is nulled only when Release answers 0.
// Then the string.
SplineControlPoint::~SplineControlPoint()
{
    s_diffuseColor->release();
    s_worldViewProjection->release();
    dx9rend::effectRelease(s_effect);
    if (s_sphere && !s_sphere->Release())
        s_sphere = 0;
}

// Draws control points in green, white or dark blue for types 0, 1 or 2.
// Type 3 is hidden; other types retain the previous color.
void SplineControlPoint::draw()
{
    if (m_type == 3)
        return;

    Vec4 colour;
    if (m_type == 2)
    {
        colour.x = 0.0f;
        colour.y = 0.0f;
        colour.z = 0.5f;
        colour.w = 1.0f;
        s_diffuseColor->setValue(&colour, 0);                               // +0x18
    }
    else if (m_type == 1)
    {
        colour.x = 1.0f;
        colour.y = 1.0f;
        colour.z = 1.0f;
        colour.w = 1.0f;
        s_diffuseColor->setValue(&colour, 0);
    }
    else if (m_type == 0)
    {
        colour.y = 1.0f;
        colour.w = 1.0f;
        colour.x = 0.2f;
        colour.z = 0.2f;
        s_diffuseColor->setValue(&colour, 0);
    }

    IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice());
    IDirect3DVertexBuffer9* vertices = 0;
    IDirect3DIndexBuffer9* indices = 0;
    s_sphere->GetVertexBuffer(&vertices);                                    // +0x34
    s_sphere->GetIndexBuffer(&indices);                                      // +0x38
    device->SetStreamSource(0, vertices, 0, 12);                             // +0x190
    device->SetIndices(indices);                                             // +0x1A0
    vertices->Release();
    indices->Release();

    dx9rend::effectSetTechniqueByName(s_effect, vc71::string("controlpoint"));
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();    // renderer +0x48

    Vec3 scale;
    scale.x = m_radius;
    scale.y = m_radius;
    scale.z = m_radius;
    Mat4 world = Mat4::getIdentity();
    postScale(world, scale);
    world.m[12] = world.m[12] + m_position.x;
    world.m[13] = world.m[13] + m_position.y;
    world.m[14] = world.m[14] + m_position.z;

    const Mat4* const viewMatrix = view->getInverseViewMatrixPtr();         // +0x68
    Mat4 copy = world;
    world.fullMult(copy, *viewMatrix);
    const Mat4* const projection = view->getProjectionMatrixPtr();           // +0x6C
    Mat4 copy2 = world;
    world.fullMult(copy2, *projection);
    s_worldViewProjection->setValue(&world, 0);

    dx9rend::effectBegin(s_effect);                                          // sub_1001D780
    dx9rend::effectBeginPass(s_effect, 0);                                   // sub_1001DBB0
    const unsigned int vertexCount = s_sphere->GetNumVertices();
    const unsigned int faceCount = s_sphere->GetNumFaces();
    dx9rend::rendererDrawIndexedPrimitive(faceCount, 4u, 0, 0u, vertexCount, 0u);
    dx9rend::effectEnd(s_effect);                                            // sub_1001D7E0
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
