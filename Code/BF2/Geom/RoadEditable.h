// Editable spline road: builds, draws and projects geometry onto terrain, then writes
// Roads/<name>_compiled.mesh and the CompiledRoads.con entry. [INFERRED] header placement.
// Keep the IRoadEditable, IConsoleSaveable and IEventListener base order.
// Spline and noise setters take no reference. Edge spline types are [INFERRED].
// Working vertices are 64 bytes; GPU/file vertices retain position, two UV sets and alpha
// in 32 bytes. File positions are relative to the bounding-sphere centre.

#ifdef BF2_IMAGE_RENDDX9

#ifndef BF2_GEOM_ROADEDITABLE_H
#define BF2_GEOM_ROADEDITABLE_H

#include "IRoadEditable.h"
#include "MeshTemplate.h"            // MeshBounds
#include "UndergrowthSystem.h"       // Vc71Vec<T>
#include "Vc71ValueList.h"
#include "../Game/IEventListener.h"
#include "../IO/Console/IConsoleSaveable.h"
#include "../Math/Vec3.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"     // renddx9abi::HostString

#include <stddef.h>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend { class Effect; class ShaderParameter; struct EngineTexture; }

namespace geom {


// RoadEditableVertex - [CHOSEN name] the 64-byte record of the tessellator (no symbol names it).
//   +0x00 position  +0x0C normal (the spline's; interpolated, never renormalised)  +0x18 uv0
//   +0x20 uv1  +0x28 byte "needs texture coordinates" (1 from every constructor; the texture pass's
//   triangle arm clears it)  +0x2C four floats, always 0 [UNRESOLVED]  +0x3C alpha (1.0f)
// The default constructor is the inline zero-fill every body opens with (flag 1, alpha 1.0f); the
// copy is memberwise (0x10140850).

struct RoadEditableVertex
{
    Vec3          position;     // +0x00
    Vec3          normal;       // +0x0C
    Vec2          uv0;          // +0x18
    Vec2          uv1;          // +0x20
    unsigned char needsUv;      // +0x28
    unsigned char pad29[3];
    float         unknown2C[4]; // +0x2C
    float         alpha;        // +0x3C

    RoadEditableVertex();
    RoadEditableVertex(const Vec3& samplePosition, const Vec3& sampleNormal);

    // (`retn 0Ch`, this = the result): position and normal through the Vec3 lerp
    // (0x10106AE0), both uv sets as a * (1 - t) + b * t, alpha as (b - a) * t + a. The flag and the
    // four floats are left as they were.
    void interpolate(const RoadEditableVertex& a, const RoadEditableVertex& b, float t);
    // the same without position and normal.
    void interpolateTexCoords(const RoadEditableVertex& a, const RoadEditableVertex& b, float t);
};

typedef std::vector<int>               RoadPolygon;
typedef Vc71ValueList<RoadPolygon>     RoadPolygonList;

class RoadEditable : public IRoadEditable,
                     public ::dice::hfe::io::IConsoleSaveable,
                     public IEventListener
{
public:
    RoadEditable();
    ~RoadEditable();

    // IBase (the primary copies; the IConsoleSaveable thunks are the compiler's)
    virtual void  addRef();                                                     // +0x00 (ICF)
    virtual int   getRef();                                                     // +0x04 (ICF)
    virtual int   release();                                                    // +0x08
    virtual void* queryInterface(unsigned int iid) const;                       // +0x0C

    // IRoad
    virtual void draw();
    virtual void updateChunkVisibility(const Frustum* frustum);
    virtual bool anyChunkVisible();

    // IRoadEditable
    virtual void setTemplate(IRoadTemplate* roadTemplate);
    virtual IRoadTemplate* getTemplate();
    virtual void setSpline(IEditorSpline* spline);
    virtual IEditorSpline* getSpline();
    virtual void setNoise(IPerlinNoise* noise);
    virtual IPerlinNoise* getNoise();
    virtual void setTextureScaleOffset(int layer, const Vec2& offset);
    virtual const Vec2& getTextureScaleOffset(int layer);
    virtual void setPriority(int priority);
    virtual int getPriority();
    virtual void setFadeDistAtStartLeft(Vec2 distance);
    virtual const Vec2& getFadeDistAtStartLeft();
    virtual void setFadeDistAtStartRight(Vec2 distance);
    virtual const Vec2& getFadeDistAtStartRight();
    virtual void setFadeDistAtEndLeft(Vec2 distance);
    virtual const Vec2& getFadeDistAtEndLeft();
    virtual void setFadeDistAtEndRight(Vec2 distance);
    virtual const Vec2& getFadeDistAtEndRight();
    virtual void setName(const ::renddx9abi::HostString& name);
    virtual ::renddx9abi::HostString getName();
    virtual void setGhost(bool ghost);
    virtual bool getGhost();
    virtual ::renddx9abi::HostString getCompiledMeshPath();
    virtual void invalidate();
    virtual void projectToTerrain();
    virtual bool compile(const ::renddx9abi::HostString& levelPath);
    virtual bool compileTemplates(const ::renddx9abi::HostString& path, RoadTemplateNameSet templates);
    virtual void buildGeometry(float width, float maxError, bool ghost);

    // io::IConsoleSaveable (+0x04)
    virtual bool makeScript(::dice::hfe::io::IStream* stream);                  // 0x1013B4B0 -> false

    // IEventListener (+0x08)
    virtual void handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data);

    // the TU's own members (non-virtual in the image; thiscall on the road)
    // (`retn 14h`) - spline + template (+ noise) -> 64-byte vertices and triangle indices;
    // answers the triangle count. `maxError` is not read (the body asks the template again).
    int  tessellate(std::vector<RoadEditableVertex>& vertices, std::vector<int>& indices,
                    float width, float maxError, bool ghost);
    // (`retn 8`) - dice every pending polygon on the 2 m terrain grid, ear-clip the pieces.
    void splitAndTriangulate(std::vector<RoadEditableVertex>& vertices, std::vector<int>& indices);
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor flavour only: both buffers released and removed from the buffer manager, the
    // counts zeroed (invalidate and the destructor; see RoadEditable.cpp).
    void releaseBuffersForEditor();
#endif

    // data, in shipped order (the three vptrs precede)
    int                         m_refCount;                     // +0x0C
    IRoadTemplate*              m_template;                     // +0x10
    IEditorSpline*              m_spline;                       // +0x14
    IEditorSpline*              m_edge0;                        // +0x18  [INFERRED] type, never set
    IEditorSpline*              m_edge1;                        // +0x1C
    IPerlinNoise*               m_noise;                        // +0x20
    Vc71Vec<Vec2>               m_textureScaleOffsets;          // +0x24
    int                         m_vertexCount;                  // +0x34
    int                         m_triangleCount;                // +0x38  not written by the ctor
    void*                       m_vertexBuffer;                 // +0x3C
    void*                       m_indexBuffer;                  // +0x40
    dx9rend::EngineTexture*     m_texture0;                     // +0x44
    dx9rend::EngineTexture*     m_texture1;                     // +0x48
    dx9rend::Effect*            m_effect;                       // +0x4C
    dx9rend::ShaderParameter*   m_paramWorldViewProjection;     // +0x50
    dx9rend::ShaderParameter*   m_paramViewPos;                 // +0x54
    dx9rend::ShaderParameter*   m_paramTexLayer0;               // +0x58
    dx9rend::ShaderParameter*   m_paramTexLayer1;               // +0x5C
    dx9rend::ShaderParameter*   m_paramBlendFactor;             // +0x60
    dx9rend::ShaderParameter*   m_paramMaterial;                // +0x64
    dx9rend::ShaderParameter*   m_paramFogRange;                // +0x68
    dx9rend::ShaderParameter*   m_paramFogColor;                // +0x6C
    Vec3                        m_centre;                       // +0x70
    float                       m_radius;                       // +0x7C
    MeshBounds                  m_bounds;                       // +0x80
    ::renddx9abi::HostString    m_name;                         // +0xBC
    RoadPolygonList             m_pendingPolygons;              // +0xD8
    RoadPolygonList             m_readyPolygons;                // +0xE4
    unsigned char               m_ghost;                        // +0xF0
    unsigned char               m_padF1[3];
    int                         m_priority;                     // +0xF4
    Vec2                        m_fadeDistAtStartLeft;          // +0xF8
    Vec2                        m_fadeDistAtStartRight;         // +0x100
    Vec2                        m_fadeDistAtEndLeft;            // +0x108
    Vec2                        m_fadeDistAtEndRight;           // +0x110

private:
    RoadEditable(const RoadEditable&);
    RoadEditable& operator=(const RoadEditable&);
};

#if defined(_M_IX86)
static_assert(sizeof(RoadEditableVertex) == 0x40, "the tessellator's vertex is 64 bytes");
static_assert(offsetof(RoadEditableVertex, uv0) == 0x18, "uv0 at +0x18");
static_assert(offsetof(RoadEditableVertex, needsUv) == 0x28, "flag byte at +0x28");
static_assert(offsetof(RoadEditableVertex, alpha) == 0x3C, "alpha at +0x3C");
#endif

// The retail flavour's layout is the shipped one. In the native flavour HostString is the host's
// std::string, so the name takes that type's size instead - the object never crosses the boundary by
// layout, only through its slots.
#if defined(_M_IX86) && !BF2_PLUGIN_ABI_NATIVE
static_assert(offsetof(RoadEditable, m_refCount) == 0x0C, "refCount at +0x0C");
static_assert(offsetof(RoadEditable, m_template) == 0x10, "template at +0x10");
static_assert(offsetof(RoadEditable, m_noise) == 0x20, "noise at +0x20");
static_assert(offsetof(RoadEditable, m_textureScaleOffsets) == 0x24, "scale offsets at +0x24");
static_assert(offsetof(RoadEditable, m_vertexCount) == 0x34, "vertex count at +0x34");
static_assert(offsetof(RoadEditable, m_vertexBuffer) == 0x3C, "vertex buffer at +0x3C");
static_assert(offsetof(RoadEditable, m_texture0) == 0x44, "texture 0 at +0x44");
static_assert(offsetof(RoadEditable, m_effect) == 0x4C, "effect at +0x4C");
static_assert(offsetof(RoadEditable, m_paramFogColor) == 0x6C, "fogColor handle at +0x6C");
static_assert(offsetof(RoadEditable, m_centre) == 0x70, "sphere centre at +0x70");
static_assert(offsetof(RoadEditable, m_radius) == 0x7C, "sphere radius at +0x7C");
static_assert(offsetof(RoadEditable, m_bounds) == 0x80, "bounds at +0x80");
static_assert(offsetof(RoadEditable, m_name) == 0xBC, "name at +0xBC");
static_assert(offsetof(RoadEditable, m_pendingPolygons) == 0xD8, "pending polygons at +0xD8");
static_assert(offsetof(RoadEditable, m_readyPolygons) == 0xE4, "ready polygons at +0xE4");
static_assert(offsetof(RoadEditable, m_ghost) == 0xF0, "ghost byte at +0xF0");
static_assert(offsetof(RoadEditable, m_priority) == 0xF4, "priority at +0xF4");
static_assert(offsetof(RoadEditable, m_fadeDistAtStartLeft) == 0xF8, "fade start-left at +0xF8");
static_assert(offsetof(RoadEditable, m_fadeDistAtEndRight) == 0x110, "fade end-right at +0x110");
static_assert(sizeof(RoadEditable) == 0x118, "the factory 0x100B0A1B allocates 0x118");
#endif

// RefFactory published by ModuleGeom::getClassList.
IBase* __fastcall createRoadEditable(unsigned int iid, IBase* outer);

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_ROADEDITABLE_H
#endif // BF2_IMAGE_RENDDX9
