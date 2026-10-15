// Packs cell LOD vertices and submits batched vegetation streams.
// Rebuild cells before drawing so both leaf and trunk shader pointers are valid.
#ifdef BF2_IMAGE_RENDDX9
#include "OverGrowth.h"
#include "../StaticMesh.h"                        // staticMeshPackVec4 - sub_1008DD80
#include "../StaticMeshTemplate.h"                // StaticMeshTemplate / StaticMeshMaterial / ShaderMaterial
#include "../Terrain.h"                           // terrainSkyLightDirection - the host LightSource +0x17C
#include "../XpLightManager.h"                    // g_LightManager's typed slots
#include "../../Math/Mat4.h"
#include "../../Math/Vec3.h"
#include "../../RendMisc/IRenderView.h"
#include "../../RendDX9/RendDX9Base.h"            // rendererGetRenderViewDup, pathToLower, pathStripExtension
#include "../../RendDX9/Ra/RaShader.h"            // RenderQueueEntry, RaObjectParameters, the queue seams
#include "../../RendDX9/Ra/RaStreamDataEditable.h"
#include "../../RendDX9/Texture.h"                // EngineTexture::name (+0x58)
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"  // textureAtlasFind - sub_100156F0
#include <string>
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "OverGrowthCell.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <vector>

// appendGeometry calls two d3dx9_25 imports, D3DXMatrixInverse and D3DXMatrixTranspose (IAT
// 0x101DB46C / 0x101DB468); their D3DXMATRIX* is just 16 floats here.

namespace dice { namespace hfe {

namespace dx9rend {
extern void* g_OverGrowth;         // BF2Render.cpp - dword_10258F38, the level OverGrowth
}

namespace geom {

extern LightManager* g_LightManager;   // ModuleGeom.cpp - dword_1024B914

namespace {

// flt_10219554 (0x42DA7259) = 109.22334 = 32767/300 - a cell-relative position to SHORT4; its inverse,
// the immediate 0x3C16012C = 0.009155553, is the draw's decode scale. flt_10219558 (0x46FFFE00) = 32767
// and flt_10219550 (0x42FF0000) = 127.5. Each literal below is the float's exact value.
const float kPositionEncode = 109.22333526611328f;
const float kPositionDecode = 0.009155552834272385f;
const float kShortScale     = 32767.0f;
const float kNormalScale    = 127.5f;

// The material block (RaStreamDataEditable +0x0C) holders the draw writes: PosUnpack's value at
// block +0x128 (holder +0x124), and the Ra shader the block keeps at +0x14C (rebuildCells stores
// "LeafOG" / "TrunkOG" there; the StaticMesh draw's staticMeshMaterialShader and TerrainDraw's
// water read the same word - raMaterialShaderSlot).
inline RaMaterialParameters& streamMaterial(OverGrowthCellMesh* stream)
{
    return *reinterpret_cast<RaMaterialParameters*>(stream->m_materialParameters);
}

// One vertex of the cell streams - 16 bytes, the stride appendGeometry hands append (`push 10h`) and the
// cell constructor gives both streams. Element descriptors (OverGrowthCells.cpp, 100D0C90): position SHORT4,
// normal D3DCOLOR, texcoord SHORT2.
struct OverGrowthCellVertex
{
    short         position[4];     // +0x00 x, y, z = (world - cell centre) * 109.22334; w = shadow * 32767
    unsigned char normal[4];       // +0x08 D3DCOLOR bytes: [0] z, [1] y, [2] x = (n + 1) * 127.5;
                                   //       [3] (var_17D) is never written by the shipped body
    short         texCoord[2];     // +0x0C u, v = (clamp(t, -1, 1) * atlas scale + atlas offset) * 32767
};

// The vertex format: every architecture (append and the stream constructor pass the 16 literally).
static_assert(sizeof(OverGrowthCellVertex) == 16, "the cell streams are built with stride 16");
#if defined(_M_IX86)
static_assert(offsetof(dx9rend::Ra::RaStreamDataEditable, m_materialParameters) == 0x0C,
              "the material block sits at stream +0x0C");
static_assert(offsetof(dx9rend::Ra::RaObjectParameters, worldViewProjection) == 0x090,
              "stream +0x234 is the WorldViewProjection holder (its value at +0x238)");
static_assert(offsetof(dx9rend::Ra::RaObjectParameters, objectSpaceCamPos) == 0x1B4,
              "stream +0x358 is the ObjectSpaceCamPos holder");
static_assert(offsetof(dx9rend::RenderQueueEntry, lighting) == 0x008, "entry lighting block");
static_assert(offsetof(dx9rend::Ra::RaLightingParameters, overGrowthAmbient) == 0x04C,
              "entry +0x58 is the OverGrowthAmbient value");
#endif

// __ftol2 - truncation toward zero into 64 bits; each caller keeps the low word (ax) or byte (al).
inline long long ftol2(float value)
{
    return static_cast<long long>(value);
}

// PE 0x1000ED40 (34 B), read in full: `fcomi high; fcmovnb` then `fcomi low; fcmovb` - the high bound
// first; an unordered value comes back as `low`.
float clampFloat(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// PE 0x100778B0 (88 B), read in full - a Mat4 member: columns 0, 1 and 2 of the 3x3 scaled by s.x, s.y and
// s.z (m0, m4, m8 by x; m1, m5, m9 by y; m2, m6, m10 by z); the translation row and column 3 untouched.
void mat4ScaleColumns(Mat4& matrix, const Vec3& s)
{
    float* const m = matrix.m;
    m[0] = s.x * m[0];
    m[4] = m[4] * s.x;
    m[8] = s.x * m[8];
    m[1] = s.y * m[1];
    m[5] = s.y * m[5];
    m[9] = s.y * m[9];
    m[2] = s.z * m[2];
    m[6] = m[6] * s.z;
    m[10] = m[10] * s.z;
}

// PE 0x10079610 (92 B), read in full - transformPoint (SkinnedMesh.cpp keeps the same body file-local).
// z and y are formed before x is stored because out may alias in.
void transformPoint(const Mat4& m, const float in[3], float out[3])
{
    const float z = m.m[2] * in[0] + m.m[10] * in[2] + m.m[6] * in[1] + m.m[14];
    const float y = m.m[1] * in[0] + m.m[9] * in[2] + m.m[5] * in[1] + m.m[13];
    out[0] = m.m[8] * in[2] + m.m[4] * in[1] + in[0] * m.m[0] + m.m[12];
    out[1] = y;
    out[2] = z;
}

// PE 0x1007C440 (83 B), read in full - transformVector: the same body without the translation row.
void transformVector(const Mat4& m, const float in[3], float out[3])
{
    const float z = m.m[2] * in[0] + m.m[10] * in[2] + m.m[6] * in[1];
    const float y = m.m[1] * in[0] + m.m[9] * in[2] + m.m[5] * in[1];
    out[0] = m.m[8] * in[2] + m.m[4] * in[1] + in[0] * m.m[0];
    out[1] = y;
    out[2] = z;
}

// PE 0x100777A0 (135 B), read in full - normalise in place: |v|^2 = (x*x + y*y) + z*z kept as a float;
// within FLT_EPSILON (flt_10208958) of 1 it is left alone; below FLT_EPSILON it is zeroed (answering
// -20001, discarded by appendGeometry); otherwise each component is scaled by 1 / sqrt(|v|^2).
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

// std::string::find(s, 0, count) != npos, over the counted characters (MSVCP71's find, 0x100D1F6E).
bool containsCounted(const std::string& text, const char* needle, unsigned int count)
{
    const char* const data = text.c_str();
    const unsigned int size = text.size();
    for (unsigned int i = 0; i + count <= size; ++i)
    {
        if (std::memcmp(data + i, needle, count) == 0)
            return true;
    }
    return false;
}

// The cell centre both bodies derive: (corner.x + half, corner.y, corner.z + half), half = edge * 0.5
// (100D1D46..100D1D88 and 100D1057..100D1089, the same association: half + corner).
Vec3 cellCentre(const OverGrowthCell* cell)
{
    const float half = cell->size * 0.5f;
    Vec3 centre;
    centre.y = cell->corner.y;
    centre.x = half + cell->corner.x;
    centre.z = half + cell->corner.z;
    return centre;
}


// One of draw's two submissions (0x100D1111..0x100D126C for the leaf stream, 0x100D126F..0x100D13CB for
// the trunk stream - the second a copy of the first with the two LightManager colour slots swapped).
// Nothing happens unless the stream holds vertices (+0x08) AND primitives (+0x10), both signed `jle`.
void submitCellStream(OverGrowthCellMesh* stream, const Mat4& world, const Vec3& cameraLocal,
                      world::LightSource* sky, bool trunk)
{
    if (stream->getVertexCount() <= 0)                                           // +0x08
        return;
    if (stream->getPrimitiveCount() <= 0)                                        // +0x10
        return;

    dx9rend::RenderQueueEntry* const entry = dx9rend::renderEntryPoolAcquire();
    const Mat4* const viewProjection =
        dx9rend::rendererGetRenderViewDup()->getViewProjectionMatrixPtr();       // renderer +0x50, view +0x84
    dx9rend::Ra::RaObjectParameters& op =
        *reinterpret_cast<dx9rend::Ra::RaObjectParameters*>(stream->m_objectParameters);
    reinterpret_cast<Mat4*>(op.worldViewProjection.value)->fullMult(world, *viewProjection);
    op.objectSpaceCamPos.value[0] = cameraLocal.x;                              // x, y, z
    op.objectSpaceCamPos.value[1] = cameraLocal.y;                              //   (w keeps the block's 1)
    op.objectSpaceCamPos.value[2] = cameraLocal.z;
    {
        float* const posUnpack = streamMaterial(stream).posUnpack.value;        // block +0x128
        posUnpack[0] = kPositionDecode;                                          // 0x10031150 with the
        posUnpack[1] = kPositionDecode;                                          // stack Vec4 (s, s, s, 1)
        posUnpack[2] = kPositionDecode;
        posUnpack[3] = 1.0f;
    }
    if (sky)
    {
        dx9rend::Ra::RaLightRecord* const record =
            static_cast<dx9rend::Ra::RaLightRecord*>(entry->lightArray.lights.value);   // entry +0x128
        const float* const direction = terrainSkyLightDirection(sky);           // host +0x17C
        record->direction[1] = direction[1];
        record->direction[2] = direction[2];
        record->direction[0] = direction[0];
        const Vec3& sun = trunk ? g_LightManager->getStaticSunColor()           // +0x1D8
                                : g_LightManager->getTreeSunColor();            // +0x1C8
        record->color[0] = sun.x;
        record->color[1] = sun.y;
        record->color[2] = sun.z;
    }
    {
        const Vec3& ambient = trunk ? g_LightManager->getStaticSkyColor()       // +0x1E8
                                    : g_LightManager->getTreeAmbientColor();    // +0x40
        const float* const packed = staticMeshPackVec4(&ambient.x, 1.0f);
        float* const value = entry->lighting.overGrowthAmbient.value;            // entry +0x58, 0x1000BDD0
        value[0] = packed[0];
        value[1] = packed[1];
        value[2] = packed[2];
        value[3] = packed[3];
    }
    entry->material = stream;                                                    // entry +0x000
    entry->materialParameters = stream->m_materialParameters;                    // entry +0x004 = stream + 0x0C
    entry->objectParameters = stream->m_objectParameters;                        // entry +0x118 = stream + 0x1A4
    entry->shader = static_cast<dx9rend::Ra::RaShader*>(
        raMaterialShaderSlot(streamMaterial(stream)));                           // entry +0x138 = [stream + 0x158]
    // The PE inlines index 0 of sub_10041A70: the shader's own queue (+0x04) through its add slot (+0x04).
    dx9rend::renderQueueEnqueue(entry, false, false, false);
}

} // namespace


// Packs LOD geometry into this cell's trunk/leaf streams using transform, scale and shadow.
// geometryTemplate, its textures and destination streams must be initialized; unused is ignored.
// Atlas misses use full-range UVs. Normals use the scaled world basis and are normalized.
// Returns true even when there are no materials; append results are discarded.

bool OverGrowthCell::appendGeometry(void* geometryTemplate, const Mat4* transform, float scale,
                                    int unused, float shadow)
{
    (void)unused;                                                                // arg_C is never read
    Mat4 world = *transform;
    {
        Vec3 scaling;
        scaling.x = scale;
        scaling.y = scale;
        scaling.z = scale;
        mat4ScaleColumns(world, scaling);
    }
    static std::vector<OverGrowthCellVertex> s_vertices;                         // unk_10258F24
    static std::vector<unsigned short> s_indices;                                // unk_10258F14
    {
        Mat4 inverse;                                                            // [esp+0x158]
        Mat4 inverseTranspose;                                                   // [esp+0x198]
        ::D3DXMatrixInverse(reinterpret_cast<D3DXMATRIX*>(&inverse), 0,
                            reinterpret_cast<const D3DXMATRIX*>(&world));
        ::D3DXMatrixTranspose(reinterpret_cast<D3DXMATRIX*>(&inverseTranspose),
                              reinterpret_cast<const D3DXMATRIX*>(&inverse));
    }

    // The geom-0 / lod-0 materials (StaticMeshTemplate.h's first-lod accessors: m_materials.first ->
    // [0].first -> [0]), read at 0x100D1BE2..0x100D1BEB and again per material (0x100D1C4D..0x100D1C59,
    // 0x100D1D49..0x100D1D60).
    const StaticMeshTemplate* const t = static_cast<const StaticMeshTemplate*>(geometryTemplate);
    unsigned int remaining = staticMeshFirstLodMaterialCount(t);
    if (!remaining)                                                              // `test ebx, ebx; jbe`
        return true;

    const short shadowWord = static_cast<short>(ftol2(shadow * kShortScale));
    OverGrowthCellVertex vertex;                                                 // [esp+0x50] - one slot
    for (unsigned int materialIndex = 0; remaining != 0; ++materialIndex, --remaining)
    {
        const std::string unusedName;                                           // var_9C
        const StaticMeshMaterial* material = staticMeshFirstLodMaterial(t, materialIndex);
        const std::string& textureName =
            static_cast<const dx9rend::EngineTexture*>(material->m_shaderMaterial->textures[1])->name;

        std::string ddsName;                                                    // var_130
        {
            std::string stem;                                                   // var_D4
            dx9rend::pathStripExtension(stem, textureName);
            std::string suffixed(stem);                                         // var_B8
            suffixed += ".dds";
            ddsName = suffixed;
        }
        s_vertices.resize(0);                                                    // (0, Vertex())
        s_indices.resize(0);                                                     // (0, 0)

        dx9rend::TextureAtlas* const atlas =
            static_cast<OverGrowth*>(dx9rend::g_OverGrowth)->m_atlas;            // the global, +0xFC
        const dx9rend::TextureAtlasEntry* const found =
            atlas ? dx9rend::textureAtlasFind(atlas, ddsName) : 0;
        float scaleU = 1.0f;
        float scaleV = 1.0f;
        float offsetU = 0.0f;
        float offsetV = 0.0f;
        if (found)
        {
            scaleU = found->width;
            scaleV = found->height;
            offsetU = found->uOffset;
            offsetV = found->vOffset;
        }

        const Vec3 centre = cellCentre(this);
        material = staticMeshFirstLodMaterial(t, materialIndex);
        for (int index = 0; index < static_cast<int>(material->m_vegetationVertexCount); ++index)
        {
            const float* const source = reinterpret_cast<const float*>(
                static_cast<const unsigned char*>(material->m_vegetationVertices) + index * 32);

            Vec3 point;
            point.x = 0.0f;
            point.y = 0.0f;
            point.z = 0.0f;
            transformPoint(world, source, &point.x);
            const float dx = point.x - centre.x;
            const float dy = point.y - centre.y;
            const float dz = point.z - centre.z;
            const float scaledX = dx * kPositionEncode;
            const float scaledY = dy * kPositionEncode;
            const float scaledZ = dz * kPositionEncode;
            vertex.position[0] = static_cast<short>(ftol2(scaledX));
            vertex.position[1] = static_cast<short>(ftol2(scaledY));
            vertex.position[2] = static_cast<short>(ftol2(scaledZ));
            vertex.position[3] = shadowWord;

            vertex.texCoord[0] = static_cast<short>(ftol2(
                (clampFloat(source[6], -1.0f, 1.0f) * scaleU + offsetU) * kShortScale));   // +0x18
            vertex.texCoord[1] = static_cast<short>(ftol2(
                (clampFloat(source[7], -1.0f, 1.0f) * scaleV + offsetV) * kShortScale));   // +0x1C

            Vec3 normal;
            normal.x = 0.0f;
            normal.y = 0.0f;
            normal.z = 0.0f;
            transformVector(world, source + 3, &normal.x);                       // +0x0C
            normalizeInPlace(normal);
            vertex.normal[2] = static_cast<unsigned char>(ftol2((normal.x + 1.0f) * kNormalScale));
            vertex.normal[1] = static_cast<unsigned char>(ftol2((normal.y + 1.0f) * kNormalScale));
            vertex.normal[0] = static_cast<unsigned char>(ftol2((normal.z + 1.0f) * kNormalScale));
            s_vertices.push_back(vertex);
        }

        std::string lowered(textureName);                                       // a copy,
        dx9rend::pathToLower(lowered);                                           //   lowered (0x10074D10)
        const bool tiled = containsCounted(lowered, "tile_", 5);
        const unsigned int vertexCount = static_cast<unsigned int>(s_vertices.size());
        const void* const vertices = s_vertices.data();                          // &*(begin() + 0), 0x100D2020
        OverGrowthCellMesh* const stream = tiled ? trunk : leaf;
        stream->append(vertices, 16, static_cast<int>(vertexCount), material->m_vegetationIndices,
                       static_cast<int>(material->m_vegetationIndexCount));      // +0x2C, answer dropped
    }
    return true;
}


// Submits leaf and trunk streams using the view; position and fade are unused.
// Reuses a static transform and pooled lighting records, so the call is not reentrant.
// Lighting fields not written here retain their previous pooled values.

void OverGrowthCell::draw(IRenderView* view, const Vec3* position, float fade)
{
    (void)position;                                                              // arg_4 never read
    (void)fade;                                                                  // arg_8 never read
    const Vec3 centre = cellCentre(this);

    static Mat4 s_world = Mat4::getIdentity();                                   // dword_10258ED0
    s_world.m[12] = centre.x;
    s_world.m[0] = kPositionDecode;
    s_world.m[5] = kPositionDecode;
    s_world.m[10] = kPositionDecode;
    s_world.m[13] = centre.y;
    s_world.m[14] = centre.z;

    Vec3 cameraLocal;
    {
        const Mat4* const camera = view->getViewMatrixPtr();                     // +0x64
        cameraLocal.z = camera->m[14] - centre.z;
        cameraLocal.y = camera->m[13] - centre.y;
        cameraLocal.x = camera->m[12] - centre.x;
    }
    world::LightSource* const sky = g_LightManager->getSkyLight();              // +0x28

    submitCellStream(leaf, s_world, cameraLocal, sky, false);
    submitCellStream(trunk, s_world, cameraLocal, sky, true);
}

} } }
#endif
