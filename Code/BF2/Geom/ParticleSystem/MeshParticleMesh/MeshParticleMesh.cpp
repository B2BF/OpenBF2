// Mesh-particle geometry, material selection and queued batch drawing.
// Shared age/alpha and transform arrays provide per-instance data to the renderer.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../../RendDX9Abi/ThunkAbi.h"
#include "MeshParticleMesh.h"
#include "MeshParticleMeshTemplate.h"
#include "../../BundledMeshStateManager.h"
#include "../../../RendDX9/RendDX9Base.h"
#include "../../../RendDX9/ShaderParameter.h"
#include "../../../RendMisc/IRenderer.h"
#include "../../../../RendDX9Abi/HfeAbi.h"
#include "../../../../RendDX9Abi/BfMemory.h"
#include "../../../../RendDX9Abi/Frontier.h"       // logf
#include "../ParticleRendererCallbacks.h"              // particleRendererList, the mesh renderer

#include <new>
#include <cstring>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_rendererBase;    // dword_1023B970 - declared per-TU, as SkinnedMesh.cpp does
extern IBase* g_Renderer;       // the Renderer console object; +0x69F is the particles switch
void vertexBufferBindStream(void* wrapper, unsigned int stream);
void indexBufferBind(void* wrapper);
void vertexDeclarationBind(void* wrapper);
// Named after the D3D call, NOT in the shipped push order - see StateManager.cpp.
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex);
}

namespace geom {

extern IBase* g_ParticleSystemManager;   // ModuleGeom.cpp, dword_1024B900

// Terrain.cpp (published in Terrain.h) - sub_100E11B0: copy three floats into a guarded file static
// and answer its address. IScaleable::getScale tail-jumps into it.
const Vec3* terrainOriginScratch(const Vec3& in);
// StaticMesh.cpp (published in StaticMesh.h) - sub_1008DD80: pack a Vec3 and a w into a static
// Vec4 and answer its address. IScaleable::setScale calls it with w = 1.0f.
const float* staticMeshPackVec4(const float v[3], float w);

namespace {

// ds:the AgeAndAlphaArray, 26 x Vec4. Zero-initialised: no static initialiser
// and no other code reference touches it; only .x/.y are ever written (setAgeAndAlpha).
Vec4 s_ageAndAlpha[26];

// All 26 part slots initially point to the first identity matrix.
#define BF2_MPM_IDENTITY { { 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f, \
                             0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f } }
Mat4 s_partIdentity[26] =
{
    BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY,
    BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY,
    BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY,
    BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY,
    BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY, BF2_MPM_IDENTITY,
    BF2_MPM_IDENTITY
};
#undef BF2_MPM_IDENTITY

// Bounds returned when no LOD is selected; initialized as empty and dirty.
MeshBounds s_emptyBounds =
{
    { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 1, { 0, 0, 0 },
    { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }
};

// A bundled state's first texture (state +0x28 -> [0]), unguarded.
template<class T>
void* bundledStateFirstTexture(const std::vector<T>& textures)
{
    return textures.data()[0];
}

// The shared helper both the constructor and refreshSelection inline (0x101B02A3.. / 0x101B0070).
// NO guards: an unloaded template's null tree faults here in retail too; the geom-count test is
// `sar 4` then an unsigned `jb 2`.
void selectFromTemplate(MeshParticleMesh* self)
{
    MeshParticleMeshTemplate* const t = self->m_template;
    if (static_cast<unsigned int>(t->m_geoms.size()) >= 2u)
        self->m_geomIndex = 1;

    const int geom = self->m_geomIndex;
    const int lod  = self->m_lodIndex;
    self->m_lodRecord    = t->m_geoms.data()[geom].data() + lod;
    self->m_lodMaterials = t->m_materials.data()[geom].data() + lod;
}

} // anonymous namespace


// Initializes geometry state. Fields +0x58 and +0x6C..+0x7B remain untouched;
// part-pointer allocation is not checked before initialization.

MeshParticleMesh::MeshParticleMesh(MeshParticleMeshTemplate* objectTemplate)
    : GeometryInstanceData(g_meshParticleMeshVtable)
{
    m_scaleableVptr = g_meshParticleMeshScaleableVtable;   // +0x30,
    m_refCount      = 1;                                   // +0x34
    m_partTransforms = 0;                                  // +0x38
    m_dword3C       = 0;                                   // +0x3C
    m_fov           = -1.0f;                               // +0x40
    m_lodRecord     = 0;                                   // +0x44
    m_lodMaterials  = 0;                                   // +0x48
    m_geomIndex     = 0;                                   // +0x4C
    m_lodIndex      = 0;                                   // +0x50
    m_lodExplicit   = 0;                                   // +0x54
    m_scale.x = 1.0f;                                      // +0x5C .. +0x68
    m_scale.y = 1.0f;
    m_scale.z = 1.0f;
    m_scale.w = 1.0f;
    m_alpha         = 1.0f;                                // +0x80
    m_firstPerson   = 0;                                   // +0x7C
    m_alternateView = 0;                                   // +0x7D
    m_template      = objectTemplate;                      // +0x84

    // 26 part-transform pointers (retail 0x68 bytes).
    m_partTransforms = static_cast<const Mat4**>(::renddx9abi::engineAlloc(0x68u / 4u * kPsmPointerSize));
    for (unsigned int offset = 0; offset < 0x68u; offset += 4)
        m_partTransforms[offset / 4] = &s_partIdentity[0];

    if (m_template->m_loaded)                              // [edi+4Ch]
        selectFromTemplate(this);
}


// Unregisters the instance if its template is still alive.
// Instances created from an already-loaded template may not be in the live set.

MeshParticleMesh::~MeshParticleMesh()
{
    m_vptr          = g_meshParticleMeshVtable;            // not observable; kept for the reader
    m_scaleableVptr = g_meshParticleMeshScaleableVtable;
    if (m_partTransforms)
        ::renddx9abi::engineFree(m_partTransforms);
    if (m_template)
        m_template->m_instances.erase(this);                // sub_10129110(&this)
    // off_1020A660 is restored and m_name destroyed by the base - not observable.
}

MeshParticleMesh* meshParticleMeshConstruct(void* storage, MeshParticleMeshTemplate* objectTemplate)
{
    return new (storage) MeshParticleMesh(objectTemplate);
}

// +0x34
void* meshParticleMeshDestroyFlags(void* instance, unsigned char flags)
{
    static_cast<MeshParticleMesh*>(instance)->~MeshParticleMesh();
    if (flags & 1)
        ::renddx9abi::engineFree(instance);
    return instance;
}

// sub_101B0070, 92 bytes - the template's post-load walk. The constructor's tail without the
// loaded test (the template has just set it).
void MeshParticleMesh::refreshSelection()
{
    selectFromTemplate(this);
}

void meshParticleMeshRefreshSelection(MeshParticleMesh* instance)
{
    instance->refreshSelection();
}


// The IBase trio - StaticMesh's own bodies (count at +0x34), shared through the table. addRef and
// release return the count.

int MeshParticleMesh::addRef()
{
    return ++m_refCount;
}

int MeshParticleMesh::getRef() const
{
    return m_refCount;
}

int MeshParticleMesh::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        typedef void* (__thiscall* DestroyFn)(void*, unsigned int);
        reinterpret_cast<DestroyFn>(m_vptr[0x34 / 4])(this, 1u);   // [eax+34h] (1)
    }
    return remaining;
}

// four ids, compared in this order. The IScaleable arm keeps MSVC's `this` null test.
void* MeshParticleMesh::queryInterface(unsigned int iid)
{
    if (iid == 1u                                          // dword_102088DC, IBase
        || iid == 0x492FE0FEu                              // dword_1021AE58, IID_IGeometry
        || iid == 0xADCBF4FCu)                             // dword_1021AE64, IID_IRenderable
        return this;
    if (iid == 0xEBC881FAu && this)                        // dword_1021AE68, IID_IScaleable
        return &m_scaleableVptr;                           // the IScaleable subobject (+0x30)
    return 0;
}

void  MeshParticleMesh::setFov(float value) { m_fov = value; }            // +0x10
float MeshParticleMesh::getFov() const      { return m_fov; }             // +0x14


// Queues the instance for MeshParticlesRenderer::drawExtras; arguments are ignored.
// Requires a live template, renderer, and particle-system manager.

void MeshParticleMesh::queueDraw(void*, void*, int)
{
    if (!m_template->m_loaded)                             // [eax+4Ch]
        return;
    if (!dx9rend::rendererRenderSwitch(0x69F))                             // particles switch
        return;
    meshParticlesRendererQueueMesh(
        particleSystemManagerMeshParticlesRenderer(g_ParticleSystemManager), this);
}

// sub_101B04C0 - `add ecx, 1D80h`, then the fast push when size < capacity, else _Insert_n
// (sub_100D7560, engine heap via sub_1005F320): std::vector::push_back.
void meshParticlesRendererQueueMesh(unsigned char* renderer, MeshParticleMesh* mesh)
{
    meshParticlesRendererMeshes(renderer).push_back(mesh);
}

// +0x2C, 0x101AFD00 - `mov eax,[ecx+44h]; mov eax,[eax+40h]`. No lod guard.
int MeshParticleMesh::getGeometryType() const
{
    return m_lodRecord->renderKind;
}

// +0x38, 0x10145640 - `mov eax,[ecx+84h]` (a body comdat-shared with RoadTemplate and a road
// render-path listener).
MeshParticleMeshTemplate* MeshParticleMesh::getTemplate() const
{
    return m_template;
}

// +0x40
const void* MeshParticleMesh::getLocalBounds() const
{
    if (m_lodRecord)
        return &m_lodRecord->bounds;                       // lod + 4
    return &s_emptyBounds;                                 // ds:1023AFD0
}

// +0x44, finalise a dirty bounds first (the byte at lod +0x24), then its radius.
// No lod answers flt_101DDE64 = 0.0f.
float MeshParticleMesh::getBoundingRadius() const
{
    if (!m_lodRecord)
        return 0.0f;
    MeshBounds& bounds = m_lodRecord->bounds;
    if (bounds.dirty)
        meshBoundsFinalize(bounds);
    return bounds.radius;                                  // [esi+1Ch]
}

// +0x48, part 0. The block pointer is not tested.
const Mat4* MeshParticleMesh::getGeometryTransformation() const
{
    return m_partTransforms[0];
}

// +0x4C, 0x101AFD80 - `m_partTransforms[part] = value`. No range, loaded or null test: the
// renderer indexes 0 .. partCount-1, and partCount comes from the file.
void MeshParticleMesh::setPartTransformation(const Mat4* value, int part)
{
    m_partTransforms[part] = value;
}

// +0x50, through THE table: `call [eax+4Ch]` with (value, 0).
void MeshParticleMesh::setGeometryTransformation(const Mat4* value)
{
    typedef void (__thiscall* SetPartFn)(void*, const Mat4*, int);
    reinterpret_cast<SetPartFn>(m_vptr[0x4C / 4])(this, value, 0);
}

// Selects geom and the current LOD without checking their range or loaded state.
void MeshParticleMesh::setGeomIndex(int geom)
{
    m_geomIndex = geom;
    MeshParticleMeshTemplate* const t = m_template;
    m_lodRecord    = t->m_geoms.data()[geom].data() + m_lodIndex;
    m_lodMaterials = t->m_materials.data()[geom].data() + m_lodIndex;
}

int MeshParticleMesh::getGeomIndex() const { return m_geomIndex; }        // +0x70

// +0x74, how many geoms (16-byte records, `sar 4`).
int MeshParticleMesh::getGeomCount() const
{
    const MeshParticleMeshTemplate* const t = m_template;
    return static_cast<int>(t->m_geoms.size());
}

// +0x78, 0x101B0120. -1 = back to lod 0 and clear the explicit byte; otherwise store and raise it.
// Both arms re-point the selections from the current geom. No range test.
void MeshParticleMesh::setLodIndex(int lod)
{
    MeshParticleMeshTemplate* const t = m_template;
    if (lod == -1)
    {
        m_lodIndex    = 0;
        m_lodExplicit = 0;
        m_lodRecord    = t->m_geoms.data()[m_geomIndex].data();
        m_lodMaterials = t->m_materials.data()[m_geomIndex].data();
        return;
    }
    m_lodIndex    = lod;
    m_lodExplicit = 1;
    m_lodRecord    = t->m_geoms.data()[m_geomIndex].data() + lod;
    m_lodMaterials = t->m_materials.data()[m_geomIndex].data() + lod;
}

int MeshParticleMesh::getLodIndex() const { return m_lodIndex; }          // +0x7C

// +0x80, the current geom's lod count (72-byte records, signed division). The geom
// tree itself is indexed unguarded.
int MeshParticleMesh::getLodCount() const
{
    const std::vector<MeshParticleMeshLod>& lods = m_template->m_geoms.data()[m_geomIndex];
    return static_cast<int>(lods.size());
}

// +0xB4, 0x101B0440 - `[ecx+58h] = arg` (a body comdat-shared with the emitter descriptor's slot
// 14). [UNRESOLVED] meaning; nothing in this image reads +0x58 of this class.
void MeshParticleMesh::setDword58(unsigned int value)
{
    m_dword58 = value;
}

unsigned char MeshParticleMesh::getFirstPerson() const    { return m_firstPerson; }
void MeshParticleMesh::setFirstPerson(unsigned char v)    { m_firstPerson = v; }
unsigned char MeshParticleMesh::getAlternateView() const  { return m_alternateView; }
void MeshParticleMesh::setAlternateView(unsigned char v)  { m_alternateView = v; }

// +0xF8, 0x101AFC60 - `retn 0Ch`: three arguments, nothing done. The table's first slot past the
// 62-entry IGeometry shape the Bundled/Skinned tables end at. [UNRESOLVED] name.
void MeshParticleMesh::slotF8(unsigned int, unsigned int, unsigned int)
{
}

// +0xFC, 0x10188350 - `[ecx+80h] = arg` (comdat-shared with the trail descriptor's slot 97).
void MeshParticleMesh::setAlpha(float value)
{
    m_alpha = value;
}


// IScaleable (+0x30). +0x10, pack (scale, 1.0f) through sub_1008DD80 and copy the four
// floats to +0x5C. (Retail stores y, x, z, w in that order - not observable.)
// +0x14, 0x101AFD70: `add ecx, 2Ch; jmp sub_100E11B0` - a copy of the first three floats of +0x5C
// into sub_100E11B0's shared file static, whose address is returned. Every caller of that static
// (Terrain's loaders included) overwrites the same three floats.

void MeshParticleMesh::setScale(const Vec3& scale)
{
    const float* const packed = staticMeshPackVec4(&scale.x, 1.0f);
    m_scale.x = packed[0];
    m_scale.y = packed[1];
    m_scale.z = packed[2];
    m_scale.w = packed[3];
}

const Vec3& MeshParticleMesh::getScale() const
{
    return *terrainOriginScratch(*reinterpret_cast<const Vec3*>(&m_scale));
}


// 0x101AFBF0, 31 bytes, `retn 0Ch` - (age, alpha) into AgeAndAlphaArray[index]. No bounds test.

void MeshParticleMesh::setAgeAndAlpha(float age, float alpha, int index)
{
    s_ageAndAlpha[index].x = age;                          // dword_10266258[index * 16]
    s_ageAndAlpha[index].y = alpha;                        // dword_1026625C[index * 16]
}


// Draws batched mesh particles with the selected blend technique.
// Alpha-mode-1 materials are skipped in both passes. Invalid blend modes retain the prior
// technique; material, state and first-texture pointers must be valid.

void MeshParticleMesh::draw(int blendMode, bool transparentPass)
{
    MeshParticleMeshEffectState& shared = g_meshParticleMeshEffectState;

    const std::vector<MeshParticleMeshMaterial>* const materials = m_lodMaterials;   // [edi+48h]
    const unsigned int materialCount = static_cast<unsigned int>(materials->size());

    dx9rend::vertexDeclarationBind(m_template->m_vertexDeclaration);  // +0xA0, sub_100147C0
    dx9rend::vertexBufferBindStream(m_template->m_vertexBuffer, 0);   // +0x94, sub_100146C0
    dx9rend::indexBufferBind(m_template->m_indexBuffer);              // +0x9C, sub_100144A0
    static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderViewDup();   // discarded

    for (unsigned int i = 0; i < materialCount; ++i)
    {
        const MeshParticleMeshMaterial& material = m_lodMaterials->data()[i];
        BundledMeshState* const state = material.state;               // ebx
        const int alphaMode = material.alphaMode;

        if (alphaMode == 1)
        {
            if (!transparentPass)
                continue;
            ::renddx9abi::logf("MeshParticleMesh.cpp:99: Transparent material not supported for "
                               "meshpariclemesh %s",
                               m_template->getName().c_str());        // template +0x18
            continue;
        }
        if (transparentPass)
            continue;

        if (blendMode == 1)
            dx9rend::effectSetTechniqueByName(shared.effect, std::string("Additive"));
        else if (blendMode == 2)
            dx9rend::effectSetTechniqueByName(shared.effect, std::string("DiffuseWithZWrite"));
        else if (blendMode == 0)
            dx9rend::effectSetTechniqueByName(shared.effect, std::string("Diffuse"));

        dx9rend::effectBegin(shared.effect);

        shared.oneBoneSkinning->setMatrixPointerArray(
            reinterpret_cast<const void* const*>(m_partTransforms),
            m_lodRecord->partCount);                                  // [eax+28h]

        dx9rend::effectGetShaderParameterBorrowedTyped(
            shared.effect, std::string("AgeAndAlphaArray"))
                ->setValue(s_ageAndAlpha, m_lodRecord->partCount);    // sub_1001E3A0, [edx+18h]

        dx9rend::effectGetShaderParameterBorrowedTyped(
            shared.effect, std::string("GlobalScale"))
                ->setValue(&m_scale, 0);                              // +0x5C

        shared.texture0->setValue(bundledStateFirstTexture(state->m_textures), 0);   // state +0x28 -> [0]

        dx9rend::effectBeginPass(shared.effect, 0);

        // sub_10005820 pushed as (prim, verts, type, base, 0, start); named here after the D3D call.
        dx9rend::rendererDrawIndexedPrimitive(material.primitiveCount,
                                              m_template->m_primitiveType,
                                              static_cast<int>(material.vertexStart),
                                              0u,
                                              material.vertexCount,
                                              material.indexStart);

        dx9rend::effectEnd(shared.effect);
    }
}


// THE TWO tables. Shared bodies are reproduced once each, with the `retn N` their arity is read from:
//   0x100E13B0 retn 4 (empty)       0x100E1480 retn 8 (empty)     0x10083AB0 retn 0Ch (empty)
//   0x10082D20 retn 18h (empty)     0x10037D40 retn (empty)       0x100ED3B0 -> 0 (no args)
//   0x10088060 -> 0 (1 arg)         0x10083AF0 -> false           0x1005DA00 -> false (1 arg)
//   0x10082D40 -> -1                0x10082D50 -> 7               0x10082D30 m_name = arg
// arity IS ABI: these are callee-cleanup __thiscall bodies, so a wrong argument count here
// corrupts the caller's stack.

namespace {

MeshParticleMesh* self(void* p) { return static_cast<MeshParticleMesh*>(p); }
MeshParticleMesh* fromScaleable(void* p)
{
    return reinterpret_cast<MeshParticleMesh*>(
        static_cast<unsigned char*>(p) - offsetof(MeshParticleMesh, m_scaleableVptr));   // `sub ecx, 30h`
}

int   RD9_THUNK mpm_addRef(void* s RD9_THUNK_EDX)                   { return self(s)->addRef(); }
int   RD9_THUNK mpm_getRef(void* s RD9_THUNK_EDX)                   { return self(s)->getRef(); }
int   RD9_THUNK mpm_release(void* s RD9_THUNK_EDX)                  { return self(s)->release(); }
void* RD9_THUNK mpm_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{ return self(s)->queryInterface(iid); }
void  RD9_THUNK mpm_setFov(void* s RD9_THUNK_EDX, float v)          { self(s)->setFov(v); }
float RD9_THUNK mpm_getFov(void* s RD9_THUNK_EDX)                   { return self(s)->getFov(); }
void  RD9_THUNK mpm_queueDraw(void* s RD9_THUNK_EDX, void* a, void* b, int c)
{ self(s)->queueDraw(a, b, c); }
int   RD9_THUNK mpm_getGeometryType(void* s RD9_THUNK_EDX)          { return self(s)->getGeometryType(); }
void* RD9_THUNK mpm_destroy(void* s RD9_THUNK_EDX, unsigned int flags)
{ return meshParticleMeshDestroyFlags(s, static_cast<unsigned char>(flags)); }
void* RD9_THUNK mpm_getTemplate(void* s RD9_THUNK_EDX)              { return self(s)->getTemplate(); }
const void* RD9_THUNK mpm_getLocalBounds(void* s RD9_THUNK_EDX)     { return self(s)->getLocalBounds(); }
float RD9_THUNK mpm_getBoundingRadius(void* s RD9_THUNK_EDX)        { return self(s)->getBoundingRadius(); }
const Mat4* RD9_THUNK mpm_getGeometryTransformation(void* s RD9_THUNK_EDX)
{ return self(s)->getGeometryTransformation(); }
void  RD9_THUNK mpm_setPartTransformation(void* s RD9_THUNK_EDX, const Mat4* v, int part)
{ self(s)->setPartTransformation(v, part); }
void  RD9_THUNK mpm_setGeometryTransformation(void* s RD9_THUNK_EDX, const Mat4* v)
{ self(s)->setGeometryTransformation(v); }
void  RD9_THUNK mpm_setName(void* s RD9_THUNK_EDX, const std::string& name) { self(s)->m_name = name; }
void  RD9_THUNK mpm_setGeomIndex(void* s RD9_THUNK_EDX, int g)      { self(s)->setGeomIndex(g); }
int   RD9_THUNK mpm_getGeomIndex(void* s RD9_THUNK_EDX)             { return self(s)->getGeomIndex(); }
int   RD9_THUNK mpm_getGeomCount(void* s RD9_THUNK_EDX)             { return self(s)->getGeomCount(); }
void  RD9_THUNK mpm_setLodIndex(void* s RD9_THUNK_EDX, int l)       { self(s)->setLodIndex(l); }
int   RD9_THUNK mpm_getLodIndex(void* s RD9_THUNK_EDX)              { return self(s)->getLodIndex(); }
int   RD9_THUNK mpm_getLodCount(void* s RD9_THUNK_EDX)              { return self(s)->getLodCount(); }
void  RD9_THUNK mpm_setDword58(void* s RD9_THUNK_EDX, unsigned int v) { self(s)->setDword58(v); }
unsigned char RD9_THUNK mpm_getFirstPerson(void* s RD9_THUNK_EDX)   { return self(s)->getFirstPerson(); }
void  RD9_THUNK mpm_setFirstPerson(void* s RD9_THUNK_EDX, unsigned int v)
{ self(s)->setFirstPerson(static_cast<unsigned char>(v)); }  // `mov al, [ebp+8]` - the low byte
unsigned char RD9_THUNK mpm_getAlternateView(void* s RD9_THUNK_EDX) { return self(s)->getAlternateView(); }
void  RD9_THUNK mpm_setAlternateView(void* s RD9_THUNK_EDX, unsigned int v)
{ self(s)->setAlternateView(static_cast<unsigned char>(v)); }
void  RD9_THUNK mpm_slotF8(void* s RD9_THUNK_EDX, unsigned int a, unsigned int b, unsigned int c)
{ self(s)->slotF8(a, b, c); }
void  RD9_THUNK mpm_setAlpha(void* s RD9_THUNK_EDX, float v)        { self(s)->setAlpha(v); }

// The shared bodies.
void  RD9_THUNK mpm_empty0(void* RD9_THUNK_EDX) {}
void  RD9_THUNK mpm_empty1(void* RD9_THUNK_EDX, unsigned int) {}
void  RD9_THUNK mpm_empty2(void* RD9_THUNK_EDX, unsigned int, unsigned int) {}
void  RD9_THUNK mpm_empty3(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) {}
void  RD9_THUNK mpm_empty6(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int,
                            unsigned int, unsigned int, unsigned int) {}
unsigned int RD9_THUNK mpm_zero0(void* RD9_THUNK_EDX) { return 0; }
unsigned int RD9_THUNK mpm_zero1(void* RD9_THUNK_EDX, unsigned int) { return 0; }
bool  RD9_THUNK mpm_false0(void* RD9_THUNK_EDX) { return false; }
bool  RD9_THUNK mpm_false1(void* RD9_THUNK_EDX, unsigned int) { return false; }
int   RD9_THUNK mpm_minusOne(void* RD9_THUNK_EDX) { return -1; }
int   RD9_THUNK mpm_seven(void* RD9_THUNK_EDX) { return 7; }

// IScaleable - four adjustor thunks (`sub ecx, 30h; jmp`) and the two bodies.
int   RD9_THUNK mpms_addRef(void* s RD9_THUNK_EDX)                  { return fromScaleable(s)->addRef(); }
int   RD9_THUNK mpms_getRef(void* s RD9_THUNK_EDX)                  { return fromScaleable(s)->getRef(); }
int   RD9_THUNK mpms_release(void* s RD9_THUNK_EDX)                 { return fromScaleable(s)->release(); }
void* RD9_THUNK mpms_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{ return fromScaleable(s)->queryInterface(iid); }
void  RD9_THUNK mpms_setScale(void* s RD9_THUNK_EDX, const Vec3& v) { fromScaleable(s)->setScale(v); }
const Vec3& RD9_THUNK mpms_getScale(void* s RD9_THUNK_EDX)          { return fromScaleable(s)->getScale(); }

} // anonymous namespace

#define BF2_MPM_ENTRY(fn) reinterpret_cast<void*>(&fn)

// The 64-slot table includes a three-argument no-op at +0xF8 and the setter at +0xFC.
void* const g_meshParticleMeshVtable[64] =
{
    BF2_MPM_ENTRY(mpm_addRef),                     // +0x00  0x10103C00  shared (StaticMesh)
    BF2_MPM_ENTRY(mpm_getRef),                     // +0x04  0x10103C10  shared
    BF2_MPM_ENTRY(mpm_release),                    // +0x08  0x10103C20  shared
    BF2_MPM_ENTRY(mpm_queryInterface),             // +0x0C  0x101B03A0  own
    BF2_MPM_ENTRY(mpm_setFov),                     // +0x10  0x101B0490  own
    BF2_MPM_ENTRY(mpm_getFov),                     // +0x14  0x101B04A0  own
    BF2_MPM_ENTRY(mpm_queueDraw),                  // +0x18  0x101B0370  own
    BF2_MPM_ENTRY(mpm_empty2),                     // +0x1C  0x100E1480  shared (silhouette)
    BF2_MPM_ENTRY(mpm_empty6),                     // +0x20  0x10082D20  shared (shadow map)
    BF2_MPM_ENTRY(mpm_empty1),                     // +0x24  0x100E13B0  shared
    BF2_MPM_ENTRY(mpm_zero0),                      // +0x28  0x100ED3B0  shared
    BF2_MPM_ENTRY(mpm_getGeometryType),            // +0x2C  0x101AFD00  own
    BF2_MPM_ENTRY(mpm_false1),                     // +0x30  0x1005DA00  shared
    BF2_MPM_ENTRY(mpm_destroy),                    // +0x34  0x101B0520  own
    BF2_MPM_ENTRY(mpm_getTemplate),                // +0x38  0x10145640  shared (comdat)
    BF2_MPM_ENTRY(mpm_zero1),                      // +0x3C  0x10088060  shared
    BF2_MPM_ENTRY(mpm_getLocalBounds),             // +0x40  0x101B03E0  own
    BF2_MPM_ENTRY(mpm_getBoundingRadius),          // +0x44  0x101B0400  own
    BF2_MPM_ENTRY(mpm_getGeometryTransformation),  // +0x48  0x101AFC10  own
    BF2_MPM_ENTRY(mpm_setPartTransformation),      // +0x4C  0x101AFD80  own
    BF2_MPM_ENTRY(mpm_setGeometryTransformation),  // +0x50  0x101AFC20  own
    BF2_MPM_ENTRY(mpm_empty2),                     // +0x54  0x100E1480  shared
    BF2_MPM_ENTRY(mpm_empty1),                     // +0x58  0x100E13B0  shared
    BF2_MPM_ENTRY(mpm_zero0),                      // +0x5C  0x100ED3B0  shared
    BF2_MPM_ENTRY(mpm_empty2),                     // +0x60  0x100E1480  shared
    BF2_MPM_ENTRY(mpm_empty1),                     // +0x64  0x100E13B0  shared
    BF2_MPM_ENTRY(mpm_setName),                    // +0x68  0x10082D30  shared
    BF2_MPM_ENTRY(mpm_setGeomIndex),               // +0x6C  0x101B00D0  own
    BF2_MPM_ENTRY(mpm_getGeomIndex),               // +0x70  0x101AFC40  own
    BF2_MPM_ENTRY(mpm_getGeomCount),               // +0x74  0x101AFD10  own
    BF2_MPM_ENTRY(mpm_setLodIndex),                // +0x78  0x101B0120  own
    BF2_MPM_ENTRY(mpm_getLodIndex),                // +0x7C  0x101B0430  own
    BF2_MPM_ENTRY(mpm_getLodCount),                // +0x80  0x101B01B0  own
    BF2_MPM_ENTRY(mpm_empty1),                     // +0x84  0x100E13B0  shared
    BF2_MPM_ENTRY(mpm_minusOne),                   // +0x88  0x10082D40  shared
    BF2_MPM_ENTRY(mpm_zero0),                      // +0x8C  0x100ED3B0  shared (material count 0!)
    BF2_MPM_ENTRY(mpm_empty3),                     // +0x90  0x10083AB0  shared
    BF2_MPM_ENTRY(mpm_empty2),                     // +0x94  0x100E1480  shared
    BF2_MPM_ENTRY(mpm_empty2),                     // +0x98  0x100E1480  shared
    BF2_MPM_ENTRY(mpm_empty1),                     // +0x9C  0x100E13B0  shared (owner: ignored)
    BF2_MPM_ENTRY(mpm_zero0),                      // +0xA0  0x100ED3B0  shared
    BF2_MPM_ENTRY(mpm_zero0),                      // +0xA4  0x100ED3B0  shared (root: none)
    BF2_MPM_ENTRY(mpm_false0),                     // +0xA8  0x10083AF0  shared (no shadow)
    BF2_MPM_ENTRY(mpm_empty1),                     // +0xAC  0x100E13B0  shared
    BF2_MPM_ENTRY(mpm_zero0),                      // +0xB0  0x100ED3B0  shared
    BF2_MPM_ENTRY(mpm_setDword58),                 // +0xB4  0x101B0440  shared (comdat)
    BF2_MPM_ENTRY(mpm_empty1),                     // +0xB8  0x100E13B0  shared
    BF2_MPM_ENTRY(mpm_false0),                     // +0xBC  0x10083AF0  shared
    BF2_MPM_ENTRY(mpm_empty2),                     // +0xC0  0x100E1480  shared
    BF2_MPM_ENTRY(mpm_zero0),                      // +0xC4  0x100ED3B0  shared
    BF2_MPM_ENTRY(mpm_zero1),                      // +0xC8  0x10088060  shared
    BF2_MPM_ENTRY(mpm_zero0),                      // +0xCC  0x100ED3B0  shared (no next geometry)
    BF2_MPM_ENTRY(mpm_getFirstPerson),             // +0xD0  0x101B0450  own
    BF2_MPM_ENTRY(mpm_setFirstPerson),             // +0xD4  0x101B0460  own
    BF2_MPM_ENTRY(mpm_getAlternateView),           // +0xD8  0x101B0470  own
    BF2_MPM_ENTRY(mpm_setAlternateView),           // +0xDC  0x101B0480  own
    BF2_MPM_ENTRY(mpm_empty1),                     // +0xE0  0x100E13B0  shared
    BF2_MPM_ENTRY(mpm_seven),                      // +0xE4  0x10082D50  shared
    BF2_MPM_ENTRY(mpm_zero0),                      // +0xE8  0x100ED3B0  shared
    BF2_MPM_ENTRY(mpm_zero1),                      // +0xEC  0x10088060  shared
    BF2_MPM_ENTRY(mpm_empty0),                     // +0xF0  0x10037D40  shared (NO frame stamp)
    BF2_MPM_ENTRY(mpm_empty1),                     // +0xF4  0x100E13B0  shared (fade ignored)
    BF2_MPM_ENTRY(mpm_slotF8),                     // +0xF8  0x101AFC60  own, retn 0Ch
    BF2_MPM_ENTRY(mpm_setAlpha)                    // +0xFC  0x10188350  shared (comdat)
};

// IScaleable, six slots, zero-terminated in the image (0x10221B8C = 0).
void* const g_meshParticleMeshScaleableVtable[6] =
{
    BF2_MPM_ENTRY(mpms_addRef),                    // +0x00  0x10103C50  thunk (StaticMesh's)
    BF2_MPM_ENTRY(mpms_getRef),                    // +0x04  0x10103C60  thunk
    BF2_MPM_ENTRY(mpms_release),                   // +0x08  0x10103C70  thunk
    BF2_MPM_ENTRY(mpms_queryInterface),            // +0x0C  0x101B04B0  thunk onto 0x101B03A0
    BF2_MPM_ENTRY(mpms_setScale),                  // +0x10
    BF2_MPM_ENTRY(mpms_getScale)                   // +0x14
};

#undef BF2_MPM_ENTRY

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
