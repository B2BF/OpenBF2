// [TEMP] [CHOSEN, not recovered]
// Tests real road templates, editable geometry, rendering, noise, and spline code against recording host seams.
// Checks stock highway serialization, draw order, Lehmer RNG, spline operations, and tessellation invariants.
// Tessellation checks cover chord error, the 2 m grid/diagonal, winding, terrain draping, texture mapping, and fades.
// No golden editor spline is available; stock-mesh comparison is separate. No game, DLL host, or editor is launched.

#include "../BF2/Geom/RoadTemplate.h"
#include "../BF2/Geom/RoadEditable.h"
#include "../BF2/Geom/RoadEditableRenderer.h"
#include "../BF2/Geom/Terrain.h"
#include "../BF2/Geom/HeightmapEventNode.h"
#include "../BF2/GeomMisc/PerlinNoise.h"
#include "../BF2/GeomMisc/EditorSpline.h"
#include "../BF2/Math/Mat4.h"
#include "../BF2/RendMisc/IRenderView.h"
#include "../BF2/RendDX9/RendDX9Base.h"
#include "../BF2/RendDX9/ShaderParameter.h"
#include "../RendDX9Abi/BfMemory.h"
#include "../RendDX9Abi/Frontier.h"
#include "../RendDX9Abi/HostFileAbi.h"
#include "../RendDX9Abi/PluginEntry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <sstream>
#include <string>
#include <vector>

using namespace dice::hfe;
using namespace dice::hfe::geom;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char* what)
{
    ++g_checks;
    if (!condition)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

bool closeTo(float a, float b, float tolerance)
{
    return std::fabs(a - b) <= tolerance;
}

std::string text(const renddx9abi::HostString& value)
{
    return std::string(value.c_str(), value.size());
}

std::vector<std::string> g_log;
int g_allocs = 0;
int g_frees = 0;


// FileManager + stream doubles (raw tables: +0x18 open; stream +0x08 release, +0x14 write, +0x30 seek).

struct FakeStream
{
    void** vptr;
    std::string path;
    int mode;
    int access;
    std::string bytes;
    int releases;
    int seeks;
    int seekOrigin;
    int seekOffset;
};
std::vector<FakeStream*> g_streams;
void* g_streamSlots[16] = {};

int __fastcall streamRelease(void* self, void*)
{
    FakeStream* const stream = static_cast<FakeStream*>(self);
    ++stream->releases;
    return 0;
}
unsigned int __fastcall streamWrite(void* self, void*, const void* data, unsigned int size)
{
    FakeStream* const stream = static_cast<FakeStream*>(self);
    stream->bytes.append(static_cast<const char*>(data), size);
    return size;
}
int __fastcall streamSeek(void* self, void*, int origin, int offset)
{
    FakeStream* const stream = static_cast<FakeStream*>(self);
    ++stream->seeks;
    stream->seekOrigin = origin;
    stream->seekOffset = offset;
    return 0;
}

struct FakeFileManager { void** vptr; } g_fileManagerFake;
void* g_fileManagerSlots[48] = {};

renddx9abi::HostStream* __fastcall fileOpen(void*, void*, const renddx9abi::HostString& path,
                                            renddx9abi::HostFileMode mode,
                                            renddx9abi::HostFileAccess access, unsigned int)
{
    FakeStream* const stream = new FakeStream();
    stream->vptr = g_streamSlots;
    stream->path = text(path);
    stream->mode = static_cast<int>(mode);
    stream->access = static_cast<int>(access);
    stream->releases = 0;
    stream->seeks = 0;
    stream->seekOrigin = -1;
    stream->seekOffset = -1;
    g_streams.push_back(stream);
    return reinterpret_cast<renddx9abi::HostStream*>(stream);
}


// HeightmapCluster double (+0x54 primary scale, +0x68 height at a Vec3, +0x8C ray intersection).
// height(x, z) = 0.5x + 0.25z; the ray hits wherever it crosses y = 0.

struct FakeCluster { void** vptr; } g_clusterFake;
void* g_clusterSlots[48] = {};
int g_scaleCalls = 0;
int g_heightCalls = 0;
Vec3 g_scale = { 1.0f, 1.0f, 1.0f };

const Vec3& __fastcall clusterPrimaryScale(void*, void*)
{
    ++g_scaleCalls;
    return g_scale;
}
float __fastcall clusterHeight(void*, void*, const Vec3& p)
{
    ++g_heightCalls;
    return 0.5f * p.x + 0.25f * p.z;
}
bool __fastcall clusterIntersect(void*, void*, const Vec3& from, const Vec3& to, Vec3& hit,
                                 float& fraction, Vec3& normal, unsigned char& material, bool twoSided)
{
    if (!twoSided || from.y == to.y)
        return false;
    const float f = from.y / (from.y - to.y);
    if (f < 0.0f || f > 1.0f)
        return false;
    hit.x = from.x + (to.x - from.x) * f;
    hit.y = 0.0f;
    hit.z = from.z + (to.z - from.z) * f;
    fraction = f;
    normal.x = 0.0f;
    normal.y = 1.0f;
    normal.z = 0.0f;
    material = 0;
    return true;
}

// Roads step 3: the cluster's queryInterface (+0x0C) answers the editable view (820003) - a double
// whose setHeightInWorldCoords (+0x34, MSVC: IBase 4 + the destructor + 8) records each write.
struct HeightWrite
{
    float x;
    float z;
    float height;
};
struct FakeEditableCluster { void** vptr; } g_editableFake;
void* g_editableSlots[32] = {};
std::vector<unsigned int> g_clusterQueries;
std::vector<HeightWrite> g_heightWrites;

void* __fastcall clusterQueryInterface(void*, void*, unsigned int iid)
{
    g_clusterQueries.push_back(iid);
    return iid == 820003u ? static_cast<void*>(&g_editableFake) : 0;
}
void __fastcall editableSetHeight(void*, void*, float x, float z, float height)
{
    HeightWrite write;
    write.x = x;
    write.z = z;
    write.height = height;
    g_heightWrites.push_back(write);
}


// Render view + renderer doubles. The view looks down -z from the origin: identity view, fov 90
// degrees, aspect 1, near 1, far 99, an 800 x 800 viewport.

struct FakeView { void** vptr; } g_viewFake;
void* g_viewSlots[40] = {};
float g_viewport[4] = { 0.0f, 0.0f, 800.0f, 800.0f };
Mat4 g_identity;
Mat4 g_projection;

float* __fastcall viewViewport(void*, void*) { return g_viewport; }
float __fastcall viewFov(void*, void*) { return 1.5707964f; }
float __fastcall viewNear(void*, void*) { return 1.0f; }
float __fastcall viewFar(void*, void*) { return 99.0f; }
float __fastcall viewAspect(void*, void*) { return 1.0f; }
Mat4* __fastcall viewInverse(void*, void*) { return &g_identity; }
Mat4* __fastcall viewProjection(void*, void*) { return &g_projection; }

struct FakeRenderer { void** vptr; } g_rendererFake;
void* g_rendererSlots[40] = {};
dx9rend::IRenderView* __fastcall rendererRenderView(void*, void*)
{
    return reinterpret_cast<dx9rend::IRenderView*>(&g_viewFake);
}


// ClassManager double (+0x30 createInstance, +0x48 getSingleton).

struct FakeClassManager { void** vptr; } g_classManagerFake;
void* g_classManagerSlots[40] = {};
std::vector<std::string> g_singletonRequests;
int g_createInstanceCalls = 0;

IBase* __fastcall cmCreateInstance(void*, void*, unsigned int classId, unsigned int iid, IBase* outer)
{
    ++g_createInstanceCalls;
    check(classId == 81033u && iid == 80033u && outer == 0,
          "makeOffsetCopy asks the ClassManager for (81033, 80033, 0)");
    return createEditorSpline(iid, outer);
}
IBase* __fastcall cmGetSingleton(void*, void*, const renddx9abi::HostString& name)
{
    const std::string value = text(name);
    g_singletonRequests.push_back(value);
    if (value == "HeightmapCluster")
        return reinterpret_cast<IBase*>(&g_clusterFake);
    if (value == "Renderer")
        return reinterpret_cast<IBase*>(&g_rendererFake);
    return 0;
}

// Roads step 3: +0x4C getSingletonAs(name, iid) - RoadEditable::compile asks for ("Terrain", 820004) and
// queries the answer for 820005; the fake terrain records the query and answers 0 (compile ignores it).
struct FakeTerrain { void** vptr; } g_terrainFake;
void* g_terrainSlots[8] = {};
std::vector<std::string> g_singletonAsRequests;
std::vector<unsigned int> g_terrainQueries;

void* __fastcall terrainQueryInterface(void*, void*, unsigned int iid)
{
    g_terrainQueries.push_back(iid);
    return 0;
}
void* __fastcall cmGetSingletonAs(void*, void*, const renddx9abi::HostString& name, unsigned int iid)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "|%u", iid);
    g_singletonAsRequests.push_back(text(name) + buffer);
    return text(name) == "Terrain" ? static_cast<void*>(&g_terrainFake) : 0;
}


// D3DX sphere double (COM, __stdcall): +0x04 AddRef, +0x08 Release, +0x10 GetNumFaces, +0x14
// GetNumVertices, +0x2C CloneMeshFVF.

struct FakeMesh
{
    void** vptr;
    int refs;
    bool clone;
};
void* g_meshSlots[16] = {};
int g_sphereCreates = 0;
int g_clones = 0;
int g_liveMeshes = 0;
unsigned long g_cloneOptions = 0;
unsigned long g_cloneFvf = 0;

unsigned long __stdcall meshAddRef(void* self)
{
    return static_cast<unsigned long>(++static_cast<FakeMesh*>(self)->refs);
}
unsigned long __stdcall meshRelease(void* self)
{
    FakeMesh* const mesh = static_cast<FakeMesh*>(self);
    const int refs = --mesh->refs;
    if (!refs)
    {
        --g_liveMeshes;
        delete mesh;
    }
    return static_cast<unsigned long>(refs);
}
FakeMesh* newMesh(bool clone)
{
    FakeMesh* const mesh = new FakeMesh();
    mesh->vptr = g_meshSlots;
    mesh->refs = 1;
    mesh->clone = clone;
    ++g_liveMeshes;
    return mesh;
}
long __stdcall meshClone(void*, unsigned long options, unsigned long fvf, void*, void** out)
{
    ++g_clones;
    g_cloneOptions = options;
    g_cloneFvf = fvf;
    *out = newMesh(true);
    return 0;
}


// Vertex buffer / draw / effect records.

struct FakeVertexBuffer
{
    unsigned int count;
    unsigned int stride;
    std::string name;
    unsigned int usage;
    std::vector<unsigned char> data;
    int releases;                    // roads step 3: the owner decrements it took
    int removes;                     // and the BufferManager removes
};
std::vector<FakeVertexBuffer*> g_vertexBuffers;
int g_vertexBufferReleases = 0;
std::vector<unsigned int> g_drawPrimitive;
int g_indexedDraws = 0;
std::vector<unsigned int> g_indexedDrawArgs;      // roads step 3: (prims, type, base, min, count, start)
int g_pushes = 0;
int g_pops = 0;
std::vector<std::string> g_events;
int g_effectAddRefs = 0;
int g_effectReleases = 0;
int g_paramReleases = 0;
std::vector<std::string> g_paramNames;
std::vector<std::string> g_effects;
std::vector<std::string> g_techniques;
std::vector<unsigned int> g_passes;               // roads step 3: effectBeginPass's pass
int g_effectToken = 0;
dx9rend::EngineTexture* const kScratch = reinterpret_cast<dx9rend::EngineTexture*>(0x5C5C);

// Roads step 3: what each shader parameter was handed. The value pointer is kept; a matrix handed to a
// WorldViewProjection handle is copied too, because the caller's matrix is a local.
struct ParamValue
{
    std::string name;
    const void* value;
    unsigned int count;
    float matrix[16];
};
struct ParamFloat
{
    std::string name;
    float value;
};
std::map<const dx9rend::ShaderParameter*, std::string> g_paramByPtr;
std::vector<ParamValue> g_paramValues;
std::vector<ParamFloat> g_paramFloats;


// Roads step 3: RoadEditable's further seams.

struct FakeIndexBuffer
{
    unsigned int count;
    unsigned int usage;
    unsigned int format;
    std::vector<unsigned short> data;
    int releases;
    int removes;
    int binds;
};
std::vector<FakeIndexBuffer*> g_indexBuffers;
std::vector<int> g_poolSwaps;                     // the value each bufferManagerSwapPool call was handed
int g_pool = 7;                                   // the pool the BufferManager double is in
std::vector<std::string> g_textureRequests;
std::vector<dx9rend::EngineTexture*> g_textureReleases;
int g_textureCount = 0;
std::vector<unsigned int> g_renderStates;         // (state, value) pairs
std::vector<std::string> g_settingRequests;
std::vector<unsigned int> g_formatQueries;
bool g_floatTargets = false;                      // deviceCapsSupportsTextureFormat's answer
unsigned char g_capsStorage[0x200] = {};          // D3DCAPS9 at offset 0; +0x5C MaxTextureHeight
unsigned char g_raStorage[0x200] = {};            // the Ra constants block (fog range +0xA8, colour +0xBC)
unsigned char g_rendererBaseStorage[0x100] = {};  // RendDX9Base; the device pointer at +0x0C

// The EventManager double (+0x10 / +0x14 the two postEvent overloads in MSVC's reversed order, +0x2C
// register, +0x30 unregister).
struct Registration
{
    int category;
    IEventListener* listener;
    int priority;
    bool add;
};
struct PostedNode
{
    int category;
    float delay;
    int type;
    unsigned int typeField;
    float area[4];
};
struct FakeEventManager { void** vptr; } g_eventManagerFake;
void* g_eventManagerSlots[20] = {};
std::vector<Registration> g_registrations;
std::vector<PostedNode> g_posted;
int g_wrongPostOverload = 0;

// D3DX bounds: what the doubles computed and were handed.
int g_boundingCalls = 0;
unsigned long g_boundingStride = 0;
unsigned long g_boundingCount = 0;
float g_boxMin[3] = {};
float g_boxMax[3] = {};
float g_sphereCentre[3] = {};
float g_sphereRadius = 0.0f;
std::vector<std::string> g_savedSurfaces;
int g_loadSurfaceCalls = 0;
unsigned long g_loadSurfaceFilter = 0;

// IDirect3DDevice9 (+0x90 CreateOffscreenPlainSurface, +0xAC Clear, +0x178 SetVertexShaderConstantF),
// its surfaces (+0x08 Release, +0x34 LockRect, +0x38 UnlockRect) and the ID3DXRenderToSurface
// (+0x08 Release, +0x14 BeginScene, +0x18 EndScene).
struct FakeSurface
{
    void** vptr;
    unsigned int width;
    unsigned int height;
    unsigned int format;
    unsigned int pool;
    int refs;
    int locks;
    int unlocks;
    unsigned long lockFlags;
    unsigned int pitch;
    std::vector<unsigned char> bits;
};
struct SurfaceRequest
{
    unsigned int width;
    unsigned int height;
    unsigned int format;
    unsigned int pool;
    bool ok;
};
struct FakeRts
{
    void** vptr;
    int refs;
    unsigned int width;
    unsigned int height;
    unsigned int format;
    int depthStencil;
    unsigned int depthFormat;
    int begins;
    int ends;
    void* beginSurface;
    const void* beginViewport;
    unsigned long endFilter;
};
struct FakeDevice { void** vptr; } g_deviceFake;
void* g_deviceSlots[119] = {};
void* g_surfaceSlots[17] = {};
void* g_rtsSlots[9] = {};
std::vector<FakeSurface*> g_surfaces;
std::vector<SurfaceRequest> g_surfaceRequests;
std::vector<FakeRts*> g_targets;
int g_surfaceFailures = 0;                        // CreateOffscreenPlainSurface fails while this counts down
bool g_surfaceAlwaysFails = false;
int g_rtsFailures = 0;
int g_beginFailures = 0;
std::vector<std::string> g_deviceCalls;
std::vector<float> g_shaderConstants;             // (register, x, y, z, w) per SetVertexShaderConstantF


// IRoadEditable double for the renderer.

std::vector<std::string> g_drawOrder;

// The renderer reads draw, getTemplate and get/setPriority; every other slot of the typed interface
// (roads step 3) is a stop the checks never reach.
[[noreturn]] void unusedSlot()
{
    std::fprintf(stderr, "FakeRoad: an unused slot was called\n");
    std::fflush(stderr);
    std::abort();
}

class FakeRoad : public IRoadEditable
{
public:
    FakeRoad(const char* name, IRoadTemplate* roadTemplate, int priority)
        : m_name(name), m_template(roadTemplate), m_priority(priority) {}
    void  addRef() override {}
    int   getRef() override { return 1; }
    int   release() override { return 1; }
    void* queryInterface(unsigned int) const override { return 0; }
    void draw() override { g_drawOrder.push_back(m_name); }
    void updateChunkVisibility(const Frustum*) override { unusedSlot(); }
    bool anyChunkVisible() override { return true; }
    void setTemplate(IRoadTemplate*) override { unusedSlot(); }
    IRoadTemplate* getTemplate() override { return m_template; }
    void setSpline(IEditorSpline*) override { unusedSlot(); }
    IEditorSpline* getSpline() override { unusedSlot(); }
    void setNoise(IPerlinNoise*) override { unusedSlot(); }
    IPerlinNoise* getNoise() override { unusedSlot(); }
    void setTextureScaleOffset(int, const Vec2&) override { unusedSlot(); }
    const Vec2& getTextureScaleOffset(int) override { unusedSlot(); }
    void setPriority(int priority) override { m_priority = priority; }
    int getPriority() override { return m_priority; }
    void setFadeDistAtStartLeft(Vec2) override { unusedSlot(); }
    const Vec2& getFadeDistAtStartLeft() override { unusedSlot(); }
    void setFadeDistAtStartRight(Vec2) override { unusedSlot(); }
    const Vec2& getFadeDistAtStartRight() override { unusedSlot(); }
    void setFadeDistAtEndLeft(Vec2) override { unusedSlot(); }
    const Vec2& getFadeDistAtEndLeft() override { unusedSlot(); }
    void setFadeDistAtEndRight(Vec2) override { unusedSlot(); }
    const Vec2& getFadeDistAtEndRight() override { unusedSlot(); }
    void setName(const renddx9abi::HostString&) override { unusedSlot(); }
    renddx9abi::HostString getName() override { unusedSlot(); }
    void setGhost(bool) override { unusedSlot(); }
    bool getGhost() override { unusedSlot(); }
    renddx9abi::HostString getCompiledMeshPath() override { unusedSlot(); }
    void invalidate() override { unusedSlot(); }
    void projectToTerrain() override { unusedSlot(); }
    bool compile(const renddx9abi::HostString&) override { unusedSlot(); }
    bool compileTemplates(const renddx9abi::HostString&, RoadTemplateNameSet) override { unusedSlot(); }
    void buildGeometry(float, float, bool) override { unusedSlot(); }

private:
    const char* m_name;
    IRoadTemplate* m_template;
    int m_priority;
};

// An independent Lehmer step: x' = a*x mod (2^31 - 1) in 64-bit arithmetic.
int lehmer64(int x, long long factor)
{
    return static_cast<int>((static_cast<long long>(x) * factor) % 0x7FFFFFFFLL);
}

} // anonymous namespace


// The link doubles the five TUs call.

namespace renddx9abi {
void* engineAlloc(unsigned int size) { ++g_allocs; return std::malloc(size); }
void  engineFree(void* p) { if (p) ++g_frees; std::free(p); }
void logf(const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    g_log.push_back(buffer);
}
}

namespace dice {
namespace hfe {
IClassManager* classManager = 0;
::renddx9abi::HostFileManager* fileManager = 0;
EventManager* eventManager = 0;                                              // roads step 3

namespace dx9rend {
void* g_bufferManager = reinterpret_cast<void*>(0xB0F);
void* g_shaderManager = reinterpret_cast<void*>(0x5AD);

void* createVertexBuffer(void*, unsigned int count, unsigned int stride, const std::string& name,
                         unsigned int usage)
{
    FakeVertexBuffer* const buffer = new FakeVertexBuffer();
    buffer->count = count;
    buffer->stride = stride;
    buffer->name = name.c_str();
    buffer->usage = usage;
    buffer->data.assign(count * stride + 1u, 0xCD);
    g_vertexBuffers.push_back(buffer);
    return buffer;
}
void* vertexBufferLock(void* wrapper, unsigned int, unsigned int, unsigned int)
{
    return &static_cast<FakeVertexBuffer*>(wrapper)->data[0];
}
bool vertexBufferUnlock(void*) { return true; }
void vertexBufferBindStream(void*, unsigned int stream) { g_events.push_back(stream ? "bind?" : "bind0"); }
void vertexBufferRelease(void* wrapper)
{
    ++g_vertexBufferReleases;
    ++static_cast<FakeVertexBuffer*>(wrapper)->releases;
}
void rendererDrawPrimitive(unsigned int primCount, unsigned int type, unsigned int startVertex)
{
    g_drawPrimitive.push_back(primCount);
    g_drawPrimitive.push_back(type);
    g_drawPrimitive.push_back(startVertex);
}
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices, unsigned int startIndex)
{
    ++g_indexedDraws;
    g_indexedDrawArgs.push_back(primCount);
    g_indexedDrawArgs.push_back(type);
    g_indexedDrawArgs.push_back(static_cast<unsigned int>(baseVertex));
    g_indexedDrawArgs.push_back(minIndex);
    g_indexedDrawArgs.push_back(numVertices);
    g_indexedDrawArgs.push_back(startIndex);
}
void* shaderManagerGetEffect(void*, const char* rawPath)
{
    g_effects.push_back(rawPath);
    return &g_effectToken;
}
ShaderParameter* effectGetShaderParameterTyped(Effect*, const std::string& name)
{
    g_paramNames.push_back(name.c_str());
    ShaderParameter* const parameter = new ShaderParameter(0);
    g_paramByPtr[parameter] = name.c_str();
    return parameter;
}
void effectAddRef(Effect*) { ++g_effectAddRefs; }
int  effectRelease(Effect*) { ++g_effectReleases; return 1; }
bool effectSetTechniqueByName(void*, const std::string& name)
{
    g_techniques.push_back(name.c_str());
    return true;
}
unsigned int effectBegin(void*) { g_events.push_back("begin"); return 1; }
void effectBeginPass(void*, unsigned int pass)
{
    g_events.push_back("pass");
    g_passes.push_back(pass);
}
void effectEnd(void*) { g_events.push_back("end"); }

// roads step 3
void* g_rendererBase = g_rendererBaseStorage;
void* g_deviceCaps = g_capsStorage;
void* g_raShaderConstants = g_raStorage;
// The x86 mapping of RenderQueues.cpp's accessor: the retail offset into the constants block.
unsigned char* raShaderConstantsAt(unsigned int retailByteOffset)
{
    return static_cast<unsigned char*>(g_raShaderConstants) + retailByteOffset;
}

int bufferManagerSwapPool(int value)
{
    g_poolSwaps.push_back(value);
    const int previous = g_pool;
    g_pool = value;
    return previous;
}
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage, unsigned int format)
{
    FakeIndexBuffer* const buffer = new FakeIndexBuffer();
    buffer->count = count;
    buffer->usage = usage;
    buffer->format = format;
    buffer->data.assign(count + 1u, 0xCDCDu);
    g_indexBuffers.push_back(buffer);
    return buffer;
}
void* indexBufferLock(void* wrapper, unsigned int, unsigned int, unsigned int)
{
    return &static_cast<FakeIndexBuffer*>(wrapper)->data[0];
}
bool indexBufferUnlock(void*) { return true; }
void indexBufferBind(void* wrapper)
{
    ++static_cast<FakeIndexBuffer*>(wrapper)->binds;
    g_events.push_back("ibind");
}
void indexBufferRelease(void* wrapper) { ++static_cast<FakeIndexBuffer*>(wrapper)->releases; }
void bufferManagerRemoveVertexBuffer(void* wrapper) { ++static_cast<FakeVertexBuffer*>(wrapper)->removes; }
void bufferManagerRemoveIndexBuffer(void* wrapper) { ++static_cast<FakeIndexBuffer*>(wrapper)->removes; }

void* textureManagerGetTexture(const std::string& name)
{
    g_textureRequests.push_back(name.c_str());
    ++g_textureCount;
    return reinterpret_cast<void*>(static_cast<size_t>(0x7E000 + 0x10 * g_textureCount));
}
void textureReleaseRef(EngineTexture*& tex)
{
    g_textureReleases.push_back(tex);
    tex = 0;
}
void stateManagerSetRenderState(unsigned int state, unsigned int value)
{
    g_renderStates.push_back(state);
    g_renderStates.push_back(value);
}
void readStringSettingDefault(const char* name, const char* defaultValue, std::string& out)
{
    g_settingRequests.push_back(std::string(name) + "|" + defaultValue);
    if (std::strcmp(name, "Editor_LevelSavePath") == 0)
        out = "C:\\Save";
    else
        out = defaultValue;
}
bool deviceCapsSupportsTextureFormat(unsigned int format)
{
    g_formatQueries.push_back(format);
    return g_floatTargets;
}
void* renderDevice() { return reinterpret_cast<void*>(0xDE7); }
IRenderView* rendererGetRenderView() { return reinterpret_cast<IRenderView*>(&g_viewFake); }
EngineTexture* rendererGetHemiScratchTexture() { return kScratch; }
void stateManagerClear(unsigned int flags, unsigned int colour, float depth, unsigned int stencil)
{
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "clear %u %08X %g %u", flags, colour, depth, stencil);
    g_events.push_back(buffer);
}
void stateManagerPush() { ++g_pushes; }
void stateManagerPop() { ++g_pops; }

// ShaderParameter's members are Effect.cpp's; the fixture links none of that, so the parameter handle
// is this narrow double: a count and a value log.
ShaderParameter::ShaderParameter(ParameterOwner* owner) : m_refCount(1), m_owner(owner) {}
ShaderParameter::~ShaderParameter() {}
int ShaderParameter::getRef() const { return m_refCount; }
int ShaderParameter::addRef() { return ++m_refCount; }
int ShaderParameter::release() { ++g_paramReleases; return --m_refCount; }
void* ShaderParameter::getD3DXParam() const { return 0; }
void ShaderParameter::setD3DXParam(void*) {}
bool ShaderParameter::isParameterUsed() const { return true; }
void ShaderParameter::setValue(const void* value, unsigned int count)
{
    g_events.push_back(count ? "setValue?" : "setValue");
    ParamValue record;
    record.name = g_paramByPtr[this];
    record.value = value;
    record.count = count;
    std::memset(record.matrix, 0, sizeof(record.matrix));
    if (record.name == "WorldViewProjection")
        std::memcpy(record.matrix, value, sizeof(record.matrix));
    g_paramValues.push_back(record);
}
void ShaderParameter::setBool(unsigned char) {}
void ShaderParameter::setInt(int) {}
void ShaderParameter::setFloat(float value)
{
    ParamFloat record;
    record.name = g_paramByPtr[this];
    record.value = value;
    g_paramFloats.push_back(record);
}
void ShaderParameter::setMatrixPointerArray(const void* const*, unsigned int) {}
unsigned int ShaderParameter::getTypeCode() const { return 0; }
} // namespace dx9rend

namespace geom {
void lightManagerSetRenderTargetTexture(unsigned int index, dx9rend::EngineTexture* texture)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "target %u %s", index,
                  texture == kScratch ? "scratch" : (texture ? "other" : "null"));
    g_events.push_back(buffer);
}

// Terrain.cpp's raw reader open / close (sub_100763A0 / sub_10075E00), the same stores, for the
// writer twins RoadTemplate::compile uses.
void TerrainRawReader::open(const std::string& path, int mode, int access, int)
{
    m_stream = 0;
    m_stream = ::dice::hfe::fileManager->open(::renddx9abi::makeHostString(path),
                                             static_cast< ::renddx9abi::HostFileMode>(mode),
                                             static_cast< ::renddx9abi::HostFileAccess>(access), 0);
}
void TerrainRawReader::close()
{
    if (m_stream)
    {
        m_stream->release();
        m_stream = 0;
    }
}

// roads step 3: ModuleGeom's queue singleton and RoadCompiledRenderer's depth-bias pair
IBase* g_RoadEditableRenderer = 0;
unsigned int roadCompiledRendererDepthBiasBits() { return 0x3A83126Fu; }               // 0.001f
unsigned int roadCompiledRendererSlopeScaleDepthBiasBits() { return 0x3F000000u; }     // 0.5f
} // namespace geom
} // namespace hfe
} // namespace dice


// Roads step 3: the EventManager, device, surface, render-to-surface and D3DX doubles.

namespace {

bool __fastcall emRegister(void*, void*, int category, IEventListener* listener, int priority)
{
    Registration record;
    record.category = category;
    record.listener = listener;
    record.priority = priority;
    record.add = true;
    g_registrations.push_back(record);
    return true;
}
bool __fastcall emUnregister(void*, void*, int category, IEventListener* listener)
{
    Registration record;
    record.category = category;
    record.listener = listener;
    record.priority = 0;
    record.add = false;
    g_registrations.push_back(record);
    return true;
}
// The node overload. The node is read and released here, as the real manager would deliver it.
unsigned int __fastcall emPostNode(void*, void*, int category, EventNode* node, float delay)
{
    PostedNode record;
    record.category = category;
    record.delay = delay;
    geom::HeightmapEventNode* const heightmapNode = static_cast<geom::HeightmapEventNode*>(node);
    record.type = node->getType();
    record.typeField = static_cast<unsigned int>(record.type);
    const Vec4& area = heightmapNode->getAffectedArea();
    record.area[0] = area.x;
    record.area[1] = area.y;
    record.area[2] = area.z;
    record.area[3] = area.w;
    node->release();
    g_posted.push_back(record);
    return 1;
}
unsigned int __fastcall emPostId(void*, void*, int, unsigned int, void*, float)
{
    ++g_wrongPostOverload;
    return 0;
}

unsigned long __stdcall surfaceRelease(void* self)
{
    return static_cast<unsigned long>(--static_cast<FakeSurface*>(self)->refs);
}
long __stdcall surfaceLockRect(void* self, D3DLOCKED_RECT* locked, const RECT*, unsigned long flags)
{
    FakeSurface* const surface = static_cast<FakeSurface*>(self);
    ++surface->locks;
    surface->lockFlags = flags;
    locked->Pitch = static_cast<INT>(surface->pitch);
    locked->pBits = &surface->bits[0];
    return 0;
}
long __stdcall surfaceUnlockRect(void* self)
{
    ++static_cast<FakeSurface*>(self)->unlocks;
    return 0;
}

// Every surface carries a known pattern in its pitched layout (the pitch is the width's 4 bytes per
// texel plus 64 bytes of padding): texel k of the whole block - byte offset 4k - has green
// (37k + 11) & 0xFF and alpha 0x80 for every third k (k % 3 == 1), else 0; the padding cells have alpha
// 0x40. So a reader that strides by the width, not the pitch, meets padding - which is what the
// shipped projection does.
void fillSurface(FakeSurface& surface)
{
    surface.pitch = surface.width * 4u + 64u;
    surface.bits.assign(static_cast<size_t>(surface.pitch) * surface.height + 4u, 0);
    for (size_t b = 0; b + 4 <= surface.bits.size(); b += 4)
    {
        const size_t k = b / 4;
        const bool padding = (b % surface.pitch) >= surface.width * 4u;
        surface.bits[b + 1] = static_cast<unsigned char>((37u * k + 11u) & 0xFFu);
        surface.bits[b + 3] = padding ? 0x40 : static_cast<unsigned char>(k % 3 == 1 ? 0x80 : 0x00);
    }
}

long __stdcall deviceCreateOffscreenPlainSurface(void*, unsigned int width, unsigned int height,
                                                 unsigned int format, unsigned int pool, void** out, void*)
{
    SurfaceRequest request;
    request.width = width;
    request.height = height;
    request.format = format;
    request.pool = pool;
    request.ok = !(g_surfaceAlwaysFails || g_surfaceFailures > 0);
    g_surfaceRequests.push_back(request);
    if (!request.ok)
    {
        if (g_surfaceFailures > 0)
            --g_surfaceFailures;
        return static_cast<long>(0x8876086Cu);                               // D3DERR_INVALIDCALL
    }
    FakeSurface* const surface = new FakeSurface();
    surface->vptr = g_surfaceSlots;
    surface->width = width;
    surface->height = height;
    surface->format = format;
    surface->pool = pool;
    surface->refs = 1;
    fillSurface(*surface);
    g_surfaces.push_back(surface);
    *out = surface;
    return 0;
}
long __stdcall deviceClear(void*, unsigned long count, const void* rects, unsigned long flags,
                           unsigned long colour, float z, unsigned long stencil)
{
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "clear %lu %s %lu %08lX %g %lu", count, rects ? "rects" : "0", flags,
                  colour, z, stencil);
    g_deviceCalls.push_back(buffer);
    return 0;
}
long __stdcall deviceSetVertexShaderConstantF(void*, unsigned int start, const float* data, unsigned int count)
{
    for (unsigned int i = 0; i < count; ++i)
    {
        g_shaderConstants.push_back(static_cast<float>(start + i));
        for (int c = 0; c < 4; ++c)
            g_shaderConstants.push_back(data[i * 4 + c]);
    }
    return 0;
}

unsigned long __stdcall rtsRelease(void* self)
{
    return static_cast<unsigned long>(--static_cast<FakeRts*>(self)->refs);
}
long __stdcall rtsBeginScene(void* self, void* surface, const void* viewport)
{
    FakeRts* const target = static_cast<FakeRts*>(self);
    ++target->begins;
    target->beginSurface = surface;
    target->beginViewport = viewport;
    if (g_beginFailures > 0)
    {
        --g_beginFailures;
        return static_cast<long>(0x80004005u);                               // E_FAIL
    }
    return 0;
}
long __stdcall rtsEndScene(void* self, unsigned long mipFilter)
{
    FakeRts* const target = static_cast<FakeRts*>(self);
    ++target->ends;
    target->endFilter = mipFilter;
    return 0;
}

} // anonymous namespace

extern "C" long __stdcall D3DXCreateRenderToSurface(void* device, unsigned int width, unsigned int height,
                                                    unsigned int format, int depthStencil,
                                                    unsigned int depthFormat, void** out)
{
    check(device == &g_deviceFake, "D3DXCreateRenderToSurface is handed the renderer's device");
    if (g_rtsFailures > 0)
    {
        --g_rtsFailures;
        return static_cast<long>(0x80004005u);
    }
    FakeRts* const target = new FakeRts();
    target->vptr = g_rtsSlots;
    target->refs = 1;
    target->width = width;
    target->height = height;
    target->format = format;
    target->depthStencil = depthStencil;
    target->depthFormat = depthFormat;
    g_targets.push_back(target);
    *out = target;
    return 0;
}

extern "C" long __stdcall D3DXLoadSurfaceFromSurface(void* destination, const void* destinationPalette,
                                                     const RECT* destinationRect, void* source,
                                                     const void* sourcePalette, const RECT* sourceRect,
                                                     unsigned long filter, unsigned long colourKey)
{
    ++g_loadSurfaceCalls;
    g_loadSurfaceFilter = filter;
    check(destination && source && destination != source && !destinationPalette && !destinationRect &&
          !sourcePalette && !sourceRect && !colourKey,
          "D3DXLoadSurfaceFromSurface(heights, 0, 0, rendering, 0, 0, filter, 0)");
    return 0;
}

extern "C" long __stdcall D3DXSaveSurfaceToFileA(const char* file, unsigned int format, void* surface,
                                                 const void* palette, const RECT* rect)
{
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "%s|%u|%s", file, format, (surface && !palette && !rect) ? "ok" : "?");
    g_savedSurfaces.push_back(buffer);                                       // nothing is written
    return 0;
}

// D3DX's definitions (Wine's d3dx9_36 reads the same): the box is the component-wise extremes; the
// sphere's centre is the mean position and its radius the largest distance from it.
extern "C" long __stdcall D3DXComputeBoundingBox(const float* first, unsigned long count, unsigned long stride,
                                                 float* minimum, float* maximum)
{
    ++g_boundingCalls;
    g_boundingStride = stride;
    g_boundingCount = count;
    for (int c = 0; c < 3; ++c)
    {
        minimum[c] = FLT_MAX;
        maximum[c] = -FLT_MAX;
    }
    for (unsigned long i = 0; i < count; ++i)
    {
        const float* const p = reinterpret_cast<const float*>(reinterpret_cast<const char*>(first) + i * stride);
        for (int c = 0; c < 3; ++c)
        {
            minimum[c] = (std::min)(minimum[c], p[c]);
            maximum[c] = (std::max)(maximum[c], p[c]);
        }
    }
    std::memcpy(g_boxMin, minimum, sizeof(g_boxMin));
    std::memcpy(g_boxMax, maximum, sizeof(g_boxMax));
    return 0;
}
extern "C" long __stdcall D3DXComputeBoundingSphere(const float* first, unsigned long count, unsigned long stride,
                                                    float* centre, float* radius)
{
    float sum[3] = { 0.0f, 0.0f, 0.0f };
    for (unsigned long i = 0; i < count; ++i)
    {
        const float* const p = reinterpret_cast<const float*>(reinterpret_cast<const char*>(first) + i * stride);
        for (int c = 0; c < 3; ++c)
            sum[c] += p[c];
    }
    for (int c = 0; c < 3; ++c)
        centre[c] = sum[c] * (1.0f / static_cast<float>(count));
    float largest = 0.0f;
    for (unsigned long i = 0; i < count; ++i)
    {
        const float* const p = reinterpret_cast<const float*>(reinterpret_cast<const char*>(first) + i * stride);
        const float dx = p[0] - centre[0];
        const float dy = p[1] - centre[1];
        const float dz = p[2] - centre[2];
        largest = (std::max)(largest, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    *radius = largest;
    std::memcpy(g_sphereCentre, centre, sizeof(g_sphereCentre));
    g_sphereRadius = largest;
    return 0;
}

extern "C" long __stdcall D3DXCreateSphere(void*, float radius, unsigned int slices,
                                           unsigned int stacks, void** mesh, void** adjacency)
{
    ++g_sphereCreates;
    check(radius == 1.0f && slices == 5 && stacks == 5 && !adjacency,
          "the control-point sphere is D3DXCreateSphere(device, 1, 5, 5, &mesh, 0)");
    *mesh = newMesh(false);
    return 0;
}


// The checks.

namespace {

void installDoubles()
{
    g_streamSlots[0x08 / 4] = reinterpret_cast<void*>(&streamRelease);
    g_streamSlots[0x14 / 4] = reinterpret_cast<void*>(&streamWrite);
    g_fileManagerFake.vptr = g_fileManagerSlots;
    g_fileManagerSlots[0x18 / 4] = reinterpret_cast<void*>(&fileOpen);
    dice::hfe::fileManager = reinterpret_cast<renddx9abi::HostFileManager*>(&g_fileManagerFake);

    g_clusterFake.vptr = g_clusterSlots;
    g_clusterSlots[0x54 / 4] = reinterpret_cast<void*>(&clusterPrimaryScale);
    g_clusterSlots[0x68 / 4] = reinterpret_cast<void*>(&clusterHeight);
    g_clusterSlots[0x8C / 4] = reinterpret_cast<void*>(&clusterIntersect);

    g_viewFake.vptr = g_viewSlots;
    g_viewSlots[0x14 / 4] = reinterpret_cast<void*>(&viewViewport);
    g_viewSlots[0x20 / 4] = reinterpret_cast<void*>(&viewFov);
    g_viewSlots[0x34 / 4] = reinterpret_cast<void*>(&viewNear);
    g_viewSlots[0x3C / 4] = reinterpret_cast<void*>(&viewFar);
    g_viewSlots[0x44 / 4] = reinterpret_cast<void*>(&viewAspect);
    g_viewSlots[0x68 / 4] = reinterpret_cast<void*>(&viewInverse);
    g_viewSlots[0x6C / 4] = reinterpret_cast<void*>(&viewProjection);
    g_identity = Mat4::getIdentity();
    g_projection = Mat4::getIdentity();

    g_rendererFake.vptr = g_rendererSlots;
    g_rendererSlots[0x48 / 4] = reinterpret_cast<void*>(&rendererRenderView);

    g_classManagerFake.vptr = g_classManagerSlots;
    g_classManagerSlots[0x30 / 4] = reinterpret_cast<void*>(&cmCreateInstance);
    g_classManagerSlots[0x48 / 4] = reinterpret_cast<void*>(&cmGetSingleton);
    dice::hfe::classManager = reinterpret_cast<IClassManager*>(&g_classManagerFake);

    g_meshSlots[0x04 / 4] = reinterpret_cast<void*>(&meshAddRef);
    g_meshSlots[0x08 / 4] = reinterpret_cast<void*>(&meshRelease);
    g_meshSlots[0x2C / 4] = reinterpret_cast<void*>(&meshClone);

    // roads step 3
    g_streamSlots[0x30 / 4] = reinterpret_cast<void*>(&streamSeek);
    g_clusterSlots[0x0C / 4] = reinterpret_cast<void*>(&clusterQueryInterface);
    g_editableFake.vptr = g_editableSlots;
    g_editableSlots[0x34 / 4] = reinterpret_cast<void*>(&editableSetHeight);
    g_classManagerSlots[0x4C / 4] = reinterpret_cast<void*>(&cmGetSingletonAs);
    g_terrainFake.vptr = g_terrainSlots;
    g_terrainSlots[0x0C / 4] = reinterpret_cast<void*>(&terrainQueryInterface);

    g_eventManagerFake.vptr = g_eventManagerSlots;
    g_eventManagerSlots[0x10 / 4] = reinterpret_cast<void*>(&emPostId);
    g_eventManagerSlots[0x14 / 4] = reinterpret_cast<void*>(&emPostNode);
    g_eventManagerSlots[0x2C / 4] = reinterpret_cast<void*>(&emRegister);
    g_eventManagerSlots[0x30 / 4] = reinterpret_cast<void*>(&emUnregister);
    dice::hfe::eventManager = reinterpret_cast<EventManager*>(&g_eventManagerFake);

    g_deviceFake.vptr = g_deviceSlots;
    g_deviceSlots[36] = reinterpret_cast<void*>(&deviceCreateOffscreenPlainSurface);   // +0x90
    g_deviceSlots[43] = reinterpret_cast<void*>(&deviceClear);                         // +0xAC
    g_deviceSlots[94] = reinterpret_cast<void*>(&deviceSetVertexShaderConstantF);      // +0x178
    g_surfaceSlots[2] = reinterpret_cast<void*>(&surfaceRelease);
    g_surfaceSlots[13] = reinterpret_cast<void*>(&surfaceLockRect);
    g_surfaceSlots[14] = reinterpret_cast<void*>(&surfaceUnlockRect);
    g_rtsSlots[2] = reinterpret_cast<void*>(&rtsRelease);
    g_rtsSlots[5] = reinterpret_cast<void*>(&rtsBeginScene);
    g_rtsSlots[6] = reinterpret_cast<void*>(&rtsEndScene);
    void* const device = &g_deviceFake;
    static_assert(offsetof(dx9rend::RendDX9Base, device) == 0x0C, "RendDX9Base::device at +0x0C");
    std::memcpy(g_rendererBaseStorage + 0x0C, &device, sizeof(device));
    const int maxTextureHeight = 4096;
    std::memcpy(g_capsStorage + 0x5C, &maxTextureHeight, sizeof(maxTextureHeight));
}

IRoadTemplate* newTemplate()
{
    return static_cast<IRoadTemplate*>(createRoadTemplate(820008u, 0));
}

void roadTemplateChecks()
{
    check(createRoadTemplate(820008u, reinterpret_cast<IBase*>(1)) == 0, "RoadTemplate refuses aggregation");
    const int allocsBefore = g_allocs;
    IRoadTemplate* const road = newTemplate();
    check(road != 0, "RoadTemplate answers 820008");
    check(road->queryInterface(1u) == road && road->queryInterface(820009u) == 0,
          "RoadTemplate QI: 1 and 820008 only");
    check(static_cast<IBase*>(createRoadTemplate(1234u, 0)) == 0, "a refused iid answers 0");

    // The constructor's state (0x1013A700).
    check(text(road->getTextureFile(0)).empty() && text(road->getTextureFile(1)).empty(),
          "two empty texture files");
    check(road->getTextureScale(0).x == 1.0f && road->getTextureScale(1).y == 1.0f, "scales (1, 1)");
    check(!road->getMirror(0) && !road->getMirror(1), "mirrors false");
    check(road->getWidth() == 1.0f && road->getBlendFactor() == 1.0f, "width 1, blend 1");
    check(road->getMaxTesselatedError(0) == 0.25f, "one max error, 0.25");
    check(road->getFadeOffDistance().x == 200.0f && road->getFadeOffDistance().y == 300.0f,
          "fade-off (200, 300)");
    check(road->getPriority() == 0 && road->getMaterial() == 0 && text(road->getName()).empty(),
          "priority 0, material 0, no name");
    check(road->getUnknown5C() == 0 && road->getUnknown60() == 0.0f, "+0x5C / +0x60 zero");

    // Round trips (the highway template's own values).
    road->setName(renddx9abi::HostString("highway"));
    road->setWidth(10.0f);
    road->setMaxTesselatedError(0, 0.0f);
    road->setBlendFactor(0.85f);
    road->setPriority(1);
    road->setMaterial(7);
    road->setTextureFile(0, renddx9abi::HostString("objects\\roads\\textures\\tarmac_mirror"));
    road->setMirror(0u, true);
    Vec2 scale = { 1.0f, 1.0f };
    road->setTextureScale(0, scale);
    road->setTextureFile(1, renddx9abi::HostString("objects\\roads\\textures\\tarmac_layer1_lowtiling"));
    road->setMirror(1u, false);
    scale.y = 0.5f;
    road->setTextureScale(1, scale);
    road->setUnknown5C(42);
    road->setUnknown60(4.5f);
    check(text(road->getName()) == "highway" && road->getWidth() == 10.0f, "name / width round trip");
    check(road->getMaxTesselatedError(0) == 0.0f && road->getBlendFactor() == 0.85f, "error / blend");
    check(road->getPriority() == 1 && road->getMaterial() == 7, "priority / material");
    check(road->getMirror(0u) && !road->getMirror(1u), "mirror bits");
    check(road->getTextureScale(1).y == 0.5f && road->getTextureScale(0).y == 1.0f, "scales");
    check(road->getUnknown5C() == 42 && road->getUnknown60() == 4.5f, "+0x5C / +0x60 round trip");
    Vec2 fade = { 150.0f, 250.0f };
    road->setFadeOffDistance(fade);
    check(road->getFadeOffDistance().y == 250.0f, "fade round trip");
    fade.x = 200.0f;
    fade.y = 300.0f;
    road->setFadeOffDistance(fade);

    // compile - the stock highway pair (Objects_client.zip Roads/Splines/highway_compiled.*).
    g_streams.clear();
    check(road->compile(renddx9abi::HostString("Objects\\Roads\\Splines")), "compile answers true");
    check(g_streams.size() == 2, "compile opens two files");
    if (g_streams.size() == 2)
    {
        const FakeStream& con = *g_streams[0];
        const FakeStream& dat = *g_streams[1];
        check(con.path == "Objects\\Roads\\Splines\\highway_compiled.con" && con.mode == 0 && con.access == 2,
              "the .con is opened first, (path\\name_compiled.con, 0, 2)");
        check(dat.path == "Objects\\Roads\\Splines\\highway_compiled.dat" && dat.mode == 0 && dat.access == 2,
              "then the .dat, (0, 2)");
        check(con.bytes == "GeometryTemplate.create RoadCompiled highway\n"
                           "ObjectTemplate.create SimpleObject highway\n"
                           "ObjectTemplate.geometry highway\n",
              "the .con text is the stock file's (LF)");
        static const unsigned char stockDat[100] = {
            'o','b','j','e','c','t','s','\\','r','o','a','d','s','\\','t','e','x','t','u','r','e','s','\\',
            't','a','r','m','a','c','_','m','i','r','r','o','r','\n',
            'o','b','j','e','c','t','s','\\','r','o','a','d','s','\\','t','e','x','t','u','r','e','s','\\',
            't','a','r','m','a','c','_','l','a','y','e','r','1','_','l','o','w','t','i','l','i','n','g','\n',
            0x9A, 0x99, 0x59, 0x3F, 0x00, 0x00, 0x48, 0x43, 0x00, 0x00, 0x96, 0x43, 0x01, 0x00, 0x00, 0x00 };
        check(dat.bytes.size() == 100 && std::memcmp(dat.bytes.data(), stockDat, 100) == 0,
              "the .dat is the stock 100-byte highway_compiled.dat, byte for byte");
        check(con.releases == 1 && dat.releases == 1, "both streams released once");
    }

    // Release: the destructor and the block.
    const int freesBefore = g_frees;
    check(road->release() == 0, "release answers the new count");
    check(g_frees > freesBefore, "the template's memory goes back to the heap");
    check(g_allocs > allocsBefore, "the template came from the engine heap");
}

void rendererChecks()
{
    IRoadEditableRenderer* const renderer =
        static_cast<IRoadEditableRenderer*>(createRoadEditableRenderer(820020u, 0));
    check(renderer != 0 && renderer->queryInterface(1u) == renderer, "RoadEditableRenderer answers 820020 / 1");
    check(!renderer->getFlag08(), "flag byte starts 0");
    renderer->setFlag08(true);
    check(renderer->getFlag08(), "flag byte round trip");

    // An empty queue: no clear, no draw.
    g_events.clear();
    g_drawOrder.clear();
    renderer->drawAll();
    check(g_events.empty() && g_drawOrder.empty(), "an empty queue clears and draws nothing");

    IRoadTemplate* const a = newTemplate();
    IRoadTemplate* const b = newTemplate();
    a->setPriority(2);
    b->setPriority(1);
    FakeRoad r1("r1", a, 5), r2("r2", b, 9), r3("r3", b, 3), r4("r4", a, 1);
    renderer->add(&r1);
    renderer->add(&r2);
    renderer->add(&r3);
    renderer->add(&r4);
    renderer->add(&r2);                                                      // a duplicate is ignored
    renderer->drawAll();
    check(g_drawOrder.size() == 4 && g_drawOrder[0] == "r3" && g_drawOrder[1] == "r2" &&
          g_drawOrder[2] == "r4" && g_drawOrder[3] == "r1",
          "drawAll: lowest template priority first, then lowest road priority within that template");
    check(g_events.size() == 3 && g_events[0] == "target 0 scratch" && g_events[1] == "clear 1 00FFFFFF 0 0" &&
          g_events[2] == "target 0 null", "the hemi scratch target is cleared to 0x00FFFFFF once, first");

    // The queue was a copy: the renderer still holds all four.
    g_events.clear();
    g_drawOrder.clear();
    renderer->remove(&r2);
    renderer->remove(&r2);                                                   // absent: nothing
    renderer->drawAll();
    check(g_drawOrder.size() == 3 && g_drawOrder[0] == "r3" && g_drawOrder[1] == "r4" &&
          g_drawOrder[2] == "r1", "remove takes one road; the list survived the previous drawAll");

    // Equal template priority on two templates: the first met wins the scan.
    g_drawOrder.clear();
    b->setPriority(2);
    renderer->drawAll();
    check(g_drawOrder.size() == 3 && g_drawOrder[0] == "r4" && g_drawOrder[1] == "r1" && g_drawOrder[2] == "r3",
          "a priority tie between templates keeps list order; the winner's template then collects its lower road");

    renderer->remove(&r1);
    renderer->remove(&r3);
    renderer->remove(&r4);
    check(renderer->release() == 0, "the renderer releases");
    a->release();
    b->release();
}

void perlinChecks()
{
    IPerlinNoise* const noise = static_cast<IPerlinNoise*>(createPerlinNoise(80035u, 0));
    check(noise != 0 && noise->queryInterface(1u) == noise, "PerlinNoise answers 80035 / 1");
    io::IConsoleSaveable* const saveable =
        static_cast<io::IConsoleSaveable*>(noise->queryInterface(0x3709A4FFu));
    check(reinterpret_cast<char*>(saveable) == reinterpret_cast<char*>(noise) + 4,
          "0x3709A4FF answers the second subobject (+4)");
    check(!saveable->makeScript(0), "makeScript answers false");
    check(noise->getDimension() == 0 && noise->getSmoothing() == 0 && noise->getInterpolation() == 2 &&
          noise->getOctaves() == 1 && noise->getPersistence() == 0.25f, "noise defaults");
    check(noise->getOutputRange().x == -1.0f && noise->getOutputRange().y == 1.0f, "range (-1, 1)");

    noise->setSeed(1234);
    noise->setInterpolation(0);
    Vec2 range = { -0.15f, 0.15f };
    noise->setOutputRange(range);
    check(noise->getSeed() == 1234 && noise->getInterpolation() == 0 && noise->getOutputRange().y == 0.15f,
          "noise round trips");

    // One octave, linear, integer x: the sample is the base value at x - an independent Lehmer step.
    for (int x = -3; x <= 3; ++x)
    {
        int state = (1234 + x) & 0x7FFFFFFF;
        state = lehmer64(lehmer64(state, 24112), 26143);
        const float r = static_cast<float>(((state | 0x80) >> 7) + 1);
        const float expected = r * (0.15f - -0.15f) * 5.9604644775390625e-08f + -0.15f;
        check(noise->sample(static_cast<float>(x), 0.0f, 0.0f, 0.0f) == expected,
              "base value at integer x = the 24-bit draw of lcgrand(seed + x) scaled into the range");
    }

    // Three octaves, persistence 0.2 (the retail LevelEditor's road noise): deterministic and bounded.
    noise->setOctaves(3);
    noise->setPersistence(0.2f);
    IPerlinNoise* const twin = static_cast<IPerlinNoise*>(createPerlinNoise(80035u, 0));
    twin->setSeed(1234);
    twin->setInterpolation(0);
    twin->setOutputRange(range);
    twin->setOctaves(3);
    twin->setPersistence(0.2f);
    bool same = true;
    bool bounded = true;
    for (int i = 0; i < 200; ++i)
    {
        const float x = -10.0f + 0.137f * static_cast<float>(i);
        const float v = noise->sample(x, 0.0f, 0.0f, 0.0f);
        same = same && v == twin->sample(x, 0.0f, 0.0f, 0.0f);
        if (x >= 0.0f)
            bounded = bounded && std::fabs(v) <= 0.15f * 1.24f + 1e-5f;
    }
    check(same, "two noises with one seed agree sample for sample");
    check(bounded, "for x >= 0 linear three-octave samples stay within range x (1 + 0.2 + 0.04)");
    {
        // Below zero the integer part truncates (fistp in chop mode), so the fraction is negative and
        // the linear blend extrapolates past base(0) (shipped): one octave at x = -0.5 is
        // 1.5 * base(0) - 0.5 * base(1).
        IPerlinNoise* const one = static_cast<IPerlinNoise*>(createPerlinNoise(80035u, 0));
        one->setSeed(1234);
        one->setInterpolation(0);
        one->setOutputRange(range);
        float base[2];
        for (int n = 0; n < 2; ++n)
        {
            int state = (1234 + n) & 0x7FFFFFFF;
            state = lehmer64(lehmer64(state, 24112), 26143);
            const float r = static_cast<float>(((state | 0x80) >> 7) + 1);
            base[n] = r * (0.15f - -0.15f) * 5.9604644775390625e-08f + -0.15f;
        }
        const float a = base[1] * -0.5f;
        check(one->sample(-0.5f, 0.0f, 0.0f, 0.0f) == (1.0f - -0.5f) * base[0] + a,
              "negative x truncates toward zero and extrapolates (shipped)");
        one->release();
    }
    for (int mode = 1; mode <= 2; ++mode)
    {
        noise->setInterpolation(mode);
        twin->setInterpolation(mode);
        bool agree = true;
        for (int i = 0; i < 50; ++i)
        {
            const float x = 0.31f * static_cast<float>(i);
            agree = agree && noise->sample(x, 0.0f, 0.0f, 0.0f) == twin->sample(x, 0.0f, 0.0f, 0.0f);
        }
        check(agree, mode == 1 ? "cosine interpolation is deterministic" : "cubic interpolation is deterministic");
    }
    noise->setSmoothing(1);
    twin->setSmoothing(1);
    check(noise->sample(2.5f, 0.0f, 0.0f, 0.0f) == twin->sample(2.5f, 0.0f, 0.0f, 0.0f),
          "the smoothed base sampler is deterministic");

    g_log.clear();
    noise->setDimension(1);
    check(noise->sample(1.0f, 0.0f, 0.0f, 0.0f) == 0.0f && g_log.size() == 1 &&
          g_log[0] == "PerlinNoise.cpp:106: Not implemented", "a non-1D noise logs line 106 and answers 0");
    noise->setDimension(0);
    noise->setSmoothing(5);
    noise->setOctaves(1);
    noise->setInterpolation(0);
    g_log.clear();
    check(noise->sample(1.0f, 0.0f, 0.0f, 0.0f) == 0.0f && !g_log.empty() &&
          g_log[0] == "PerlinNoise.cpp:151: Unknown param", "an unknown smoothing logs line 151");
    noise->setSmoothing(0);
    noise->setInterpolation(7);
    g_log.clear();
    check(noise->sample(1.5f, 0.0f, 0.0f, 0.0f) == 0.0f && g_log.size() == 1 &&
          g_log[0] == "PerlinNoise.cpp:175: Unknown param", "an unknown interpolation logs line 175");
    check(noise->release() == 0 && twin->release() == 0, "both noises release");
}

float length(const Vec3& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

void splineChecks()
{
    IEditorSpline* const spline = static_cast<IEditorSpline*>(createEditorSpline(80033u, 0));
    check(spline != 0 && spline->queryInterface(1u) == spline && spline->queryInterface(80034u) == 0,
          "EditorSpline answers 80033 / 1");
    check(g_effects.size() == 1 && g_effects[0] == "Shaders/SplineShader", "the spline effect");
    check(g_paramNames.size() == 2 && g_paramNames[0] == "WorldViewProjection" &&
          g_paramNames[1] == "DiffuseColor", "its two parameters, in order");
    check(g_singletonRequests.size() == 1 && g_singletonRequests[0] == "HeightmapCluster",
          "the first spline caches the HeightmapCluster singleton");
    check(spline->getOrder() == 1 && spline->getStepSize() == 3.0f && spline->getLineWidth() == 0.05f &&
          spline->getSnapFlags() == 1 && spline->getControlPointCount() == 0, "spline defaults");
    check(spline->getColor().x == 1.0f && spline->getColor().w == 0.5f, "colour (1, 0, 0, 0.5)");
    check(text(spline->getName()).empty(), "no name");

    g_log.clear();
    spline->setOrder(2);
    check(g_log.size() == 1 && g_log[0] == "EditorSpline.cpp:834: Implement first order on request...",
          "setOrder only logs line 834");
    check(spline->getOrder() == 1, "and does not store");

    // Points, without and with snapping.
    spline->setSnapFlags(0);
    const float xs[4] = { 0.0f, 10.0f, 20.0f, 30.0f };
    const float zs[4] = { 0.0f, 0.0f, 10.0f, 10.0f };
    for (int i = 0; i < 4; ++i)
    {
        Vec3 p = { xs[i], 5.0f, zs[i] };
        spline->addControlPoint(p);
    }
    check(g_sphereCreates == 1 && g_clones == 1 && g_cloneOptions == 0x660 && g_cloneFvf == 2,
          "the first point makes the shared sphere (clone 0x660, XYZ)");
    check(spline->getControlPointCount() == 4, "four points");
    SplineControlPoint* const third = spline->getControlPoint(2);
    check(third->m_position.x == 20.0f && third->m_position.y == 5.0f && third->m_position.z == 10.0f,
          "points keep their positions when bit 0 is clear");
    check(third->m_radius == 0.3f && third->m_type == 2 && third->m_speed == 60.0f && third->m_direction == 0 &&
          third->m_delay == 0.0f && std::strlen(third->m_sound.c_str()) == 0, "point defaults");
    spline->setSnapFlags(1);
    Vec3 snapped = { 40.0f, 99.0f, 10.0f };
    spline->addControlPoint(snapped);
    check(spline->getControlPoint(4)->m_position.y == 22.5f, "bit 0 snaps an added point (0.5x + 0.25z)");
    Vec3 moved = { 1.0f, 2.0f, 3.0f };
    spline->setControlPointPosition(0, moved);
    check(spline->getControlPoint(0)->m_position.y == 2.0f, "bit 1 clear: a moved point keeps its y");
    spline->setSnapFlags(2);
    spline->setControlPointPosition(0, moved);
    check(spline->getControlPoint(0)->m_position.y == 1.25f, "bit 1 set: a moved point snaps");
    moved.x = 0.0f;
    moved.y = 5.0f;
    moved.z = 0.0f;
    spline->setSnapFlags(0);
    spline->setControlPointPosition(0, moved);

    // Per-point slots and the last-point wrappers.
    spline->setControlPointSpeed(1, 12.5f);
    spline->setControlPointDelay(1, 0.75f);
    spline->setControlPointDirection(1, -1);
    spline->setControlPointSound(1, renddx9abi::HostString("horn"));
    spline->setControlPointFlag(1, true);
    check(spline->getControlPointSpeed(1) == 12.5f && spline->getControlPointDelay(1) == 0.75f &&
          spline->getControlPointDirection(1) == -1 && text(spline->getControlPointSound(1)) == "horn" &&
          spline->getControlPoint(1)->m_flag3C == 1, "per-point round trips");
    spline->setLastControlPointSpeed(7.0f);
    spline->setLastControlPointDelay(2.0f);
    spline->setLastControlPointDirection(3);
    spline->setLastControlPointSound(renddx9abi::HostString("boom"));
    spline->setLastControlPointFlag(true);
    check(spline->getControlPointSpeed(4) == 7.0f && spline->getControlPointDelay(4) == 2.0f &&
          spline->getControlPointDirection(4) == 3 && text(spline->getControlPointSound(4)) == "boom" &&
          spline->getControlPoint(4)->m_flag3C == 1, "the last-point wrappers address count - 1");

    // Types.
    spline->setAllControlPointTypes(false);
    bool hidden = true;
    for (int i = 0; i < 5; ++i)
        hidden = hidden && spline->getControlPointType(i) == 3;
    check(hidden, "setAllControlPointTypes(false) = type 3");
    spline->setControlPointType(1, 0);
    spline->resetControlPointTypes();
    bool reset = true;
    for (int i = 0; i < 5; ++i)
        reset = reset && spline->getControlPointType(i) == 2;
    check(reset, "resetControlPointTypes puts every point back to 2");
    spline->setAllControlPointTypes(true);
    check(spline->getControlPointType(3) == 2, "setAllControlPointTypes(true) = type 2");

    // The iterator: the first answer is the first point, then samples come one behind.
    int segments = 0;
    int samples = 0;
    bool firstAtPointZero = false;
    bool unitNormals = true;
    Vec3 position;
    Vec3 normal;
    while (spline->nextSegment() && segments < 10)
    {
        ++segments;
        while (spline->nextSample(&position, &normal) && samples < 10000)
        {
            if (!samples)
                firstAtPointZero = position.x == 0.0f && position.y == 5.0f && position.z == 0.0f;
            unitNormals = unitNormals && closeTo(length(normal), 1.0f, 1e-4f) && normal.y == 0.0f;
            ++samples;
        }
    }
    check(segments == 5, "nextSegment walks five calls on five points (the last marks the end)");
    check(firstAtPointZero, "the first sample handed out is the first point (t = 0)");
    check(unitNormals, "every sample normal is unit and horizontal");
    check(samples >= 390 && samples <= 410, "about 100 samples per segment");

    // The offset copy - every point moved 2 m sideways.
    const int createsBefore = g_createInstanceCalls;
    IEditorSpline* const edge = spline->makeOffsetCopy(2.0f);
    check(g_createInstanceCalls == createsBefore + 1 && edge != 0, "the copy comes from the ClassManager");
    check(edge->getControlPointCount() == 5 && edge->getLineWidth() == spline->getLineWidth() &&
          edge->getStepSize() == spline->getStepSize(), "the copy has the points and the settings");
    // The side vector is (unit tangent) x up and is NOT renormalised (shipped), so a point on a slope
    // moves offset x cos(slope). Points 0..2 sit on flat chords (2 m); point 3 leaves along (10, 8.75)
    // and point 4 arrives along (5, 8.75) - the tangents m0 / 2 and m1 / 2 at t = 0 and t = 1.
    const float expected[5] = { 2.0f, 2.0f, 2.0f, 2.0f * 10.0f / std::sqrt(10.0f * 10.0f + 8.75f * 8.75f),
                                2.0f * 5.0f / std::sqrt(5.0f * 5.0f + 8.75f * 8.75f) };
    bool offsetOk = true;
    for (int i = 0; i < 5; ++i)
    {
        const Vec3 a = spline->getControlPoint(i)->m_position;
        const Vec3 b = edge->getControlPoint(i)->m_position;
        const Vec3 d = { b.x - a.x, b.y - a.y, b.z - a.z };
        offsetOk = offsetOk && closeTo(length(d), expected[i], i < 3 ? 1e-3f : 1e-2f) && d.y == 0.0f;
    }
    check(offsetOk, "each copied point moves offset x the tangent's horizontal share, never vertically");
    const Vec3 firstShift = { edge->getControlPoint(0)->m_position.x - 0.0f, 0.0f,
                              edge->getControlPoint(0)->m_position.z - 0.0f };
    check(closeTo(firstShift.x, 0.0f, 1e-2f) && closeTo(firstShift.z, 2.0f, 1e-2f),
          "along +x the side is dir x up = +z");
    check(edge->release() == 0, "the copy releases");

    // Picking: the ray through the viewport centre looks down -z from the origin at y = 0, so move the
    // view so it looks down instead - a view whose inverse maps view -z onto world -y.
    Mat4 lookDown = Mat4::getIdentity();
    lookDown.m[5] = 0.0f;
    lookDown.m[6] = 1.0f;                                                    // up = world +z
    lookDown.m[9] = -1.0f;                                                   // forward = world -y
    lookDown.m[10] = 0.0f;
    lookDown.m[13] = 50.0f;
    // The spline inverts the view's +0x68 matrix; hand it the inverse of the camera frame.
    Mat4 cameraFrame = lookDown;
    fullInvert(cameraFrame, g_identity);
    const int before = spline->getControlPointCount();
    Vec2 centre = { 400.0f, 400.0f };
    spline->setSnapFlags(0);
    spline->pickAndAddControlPoint(centre);
    check(spline->getControlPointCount() == before + 1, "pickAndAddControlPoint adds on a terrain hit");
    const Vec3 picked = spline->getControlPoint(before)->m_position;
    check(closeTo(picked.x, 0.0f, 1e-3f) && picked.y == 0.0f && closeTo(picked.z, 0.0f, 1e-3f),
          "the centre pixel of a camera 50 m up, looking down, hits (0, 0, 0)");
    check(std::find(g_singletonRequests.begin(), g_singletonRequests.end(), std::string("Renderer")) !=
          g_singletonRequests.end(), "the pick asks the Renderer singleton for its view");
    spline->pickAndMoveControlPoint(0, centre);
    check(spline->getControlPoint(0)->m_position.y == 0.0f, "pickAndMoveControlPoint moves point 0 to the hit");
    g_identity = Mat4::getIdentity();

    // Nearest: a point on the view axis in front of the camera, radius 0.3.
    Vec3 inFront = { 0.0f, 0.0f, 10.0f };                                    // the view looks down +z
    spline->addControlPoint(inFront);
    const int nearest = spline->findNearestControlPoint(centre);
    check(nearest == spline->getControlPointCount() - 1, "findNearestControlPoint finds the point on the ray");
    Vec2 corner = { 0.0f, 0.0f };
    check(spline->findNearestControlPoint(corner) == -1, "a corner pixel finds none");

    // The ribbon: hide the spheres (type 3 draws nothing), then draw.
    spline->clearControlPoints();
    check(spline->getControlPointCount() == 0, "clearControlPoints");
    for (int i = 0; i < 4; ++i)
    {
        Vec3 p = { xs[i], 5.0f, zs[i] };
        spline->addControlPoint(p);
    }
    spline->setAllControlPointTypes(false);
    g_events.clear();
    g_drawPrimitive.clear();
    const size_t buffersBefore = g_vertexBuffers.size();
    spline->draw();
    check(g_pushes == 1 && g_pops == 1, "draw is one state bracket");
    check(g_vertexBuffers.size() == buffersBefore + 1, "the dirty spline rebuilt one buffer");
    if (g_vertexBuffers.size() == buffersBefore + 1)
    {
        const FakeVertexBuffer& vb = *g_vertexBuffers.back();
        check(vb.stride == 24 && vb.name == "EditorSpline" && vb.usage == 8, "(count, 24, \"EditorSpline\", 8)");
        check(vb.count >= 4 && vb.count % 2 == 0, "an even strip: left/right pairs");
        const float* const v = reinterpret_cast<const float*>(&vb.data[0]);
        bool widthOk = true;
        bool normalOk = true;
        for (unsigned int i = 0; i + 1 < vb.count; i += 2)
        {
            const float* left = v + i * 6;
            const float* right = v + (i + 1) * 6;
            const Vec3 across = { right[0] - left[0], right[1] - left[1], right[2] - left[2] };
            widthOk = widthOk && closeTo(length(across), 0.05f, 1e-4f);
            const Vec3 n = { left[3], left[4], left[5] };
            normalOk = normalOk && closeTo(length(n), 1.0f, 1e-4f) && left[3] == right[3] && left[5] == right[5];
        }
        check(widthOk, "every left/right pair is the 0.05 m ribbon width apart");
        check(normalOk, "each pair carries one unit normal");
        check(g_drawPrimitive.size() == 3 && g_drawPrimitive[0] == vb.count - 2 && g_drawPrimitive[1] == 5 &&
              g_drawPrimitive[2] == 0, "a triangle strip of (vertices - 2)");
    }
    check(g_techniques.size() >= 1 && g_techniques.back() == "spline", "technique \"spline\"");
    check(g_indexedDraws == 0, "hidden points draw no sphere");
    g_drawPrimitive.clear();
    spline->draw();
    check(g_vertexBuffers.size() == buffersBefore + 1, "a clean spline does not rebuild");

    // Tear down: the points' shared sphere goes when the last point does.
    const int releasesBefore = g_paramReleases;
    check(spline->release() == 0, "the spline releases");
    check(g_paramReleases > releasesBefore + 2, "the spline and its points release their parameters");
    check(g_liveMeshes == 0, "the shared sphere is freed with the last point");
    check(g_vertexBufferReleases == 0, "the ribbon buffer is never released by the destructor (shipped leak)");
}


// Roads step 3: RoadEditable.


// The vertex buffer's 32-byte record (buildGeometry): position, uv0, uv1, alpha.
struct Record32
{
    float px, py, pz;
    float u0, v0;
    float u1, v1;
    float alpha;
};

std::vector<Record32> records(const void* buffer)
{
    const FakeVertexBuffer& vb = *static_cast<const FakeVertexBuffer*>(buffer);
    std::vector<Record32> out(vb.count);
    if (vb.count)
        std::memcpy(&out[0], &vb.data[0], vb.count * sizeof(Record32));
    return out;
}

std::vector<unsigned short> indexData(const void* buffer)
{
    const FakeIndexBuffer& ib = *static_cast<const FakeIndexBuffer*>(buffer);
    return std::vector<unsigned short>(ib.data.begin(), ib.data.begin() + ib.count);
}

Vec3 positionOf(const Record32& r)
{
    Vec3 v;
    v.x = r.px;
    v.y = r.py;
    v.z = r.pz;
    return v;
}

bool same(const Vec3& a, const Vec3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

// The order the two-column texture walk adds a row's squares in: sqrt((z*z + y*y) + x*x).
float lengthZYX(float x, float y, float z)
{
    return std::sqrt((z * z + y * y) + x * x);
}

// The tessellator's walk over one offset copy, through the same public IEditorSpline calls:
// makeOffsetCopy, a restart (+0x6C), then segments 1 .. count - 3, every sample (+0x70) kept.
std::vector<Vec3> edgeSamples(IEditorSpline* spline, float offset)
{
    IEditorSpline* const copy = spline->makeOffsetCopy(offset);
    std::vector<Vec3> samples;
    Vec3 p;
    Vec3 n;
    copy->nextSegment();
    for (int segment = 1; segment < copy->getControlPointCount() - 2; ++segment)
    {
        copy->nextSegment();
        while (copy->nextSample(&p, &n))
            samples.push_back(p);
    }
    copy->release();
    return samples;
}

// The distance from p to the line through a and b, in double.
double lineDistance(const Vec3& a, const Vec3& b, const Vec3& p)
{
    const double ex = double(b.x) - a.x;
    const double ey = double(b.y) - a.y;
    const double ez = double(b.z) - a.z;
    const double dx = double(a.x) - p.x;
    const double dy = double(a.y) - p.y;
    const double dz = double(a.z) - p.z;
    const double cx = ey * dz - ez * dy;
    const double cy = ez * dx - dz * ex;
    const double cz = dy * ex - ey * dx;
    return std::sqrt(cx * cx + cy * cy + cz * cz) / std::sqrt(ex * ex + ey * ey + ez * ez);
}

// Independent statements about a triangle list, in x/z.
struct MeshStats
{
    bool   indicesValid;
    int    triangles;
    double area;               // the sum of the triangles' areas
    int    counterClockwise;   // orient(p, q, r) > 1e-4 (emitTriangle never emits one)
    int    outsideCell;        // not inside one 2 m cell of the even grid (1 cm tolerance)
    int    acrossDiagonal;     // inside a cell but on both sides of its x + z = gx + gz + 2 diagonal
};

MeshStats analyse(const std::vector<Record32>& v, const std::vector<unsigned short>& idx)
{
    MeshStats s;
    s.indicesValid = idx.size() % 3 == 0;
    s.triangles = static_cast<int>(idx.size() / 3);
    s.area = 0.0;
    s.counterClockwise = 0;
    s.outsideCell = 0;
    s.acrossDiagonal = 0;
    for (size_t t = 0; t + 2 < idx.size(); t += 3)
    {
        if (idx[t] >= v.size() || idx[t + 1] >= v.size() || idx[t + 2] >= v.size())
        {
            s.indicesValid = false;
            continue;
        }
        const Record32& p = v[idx[t]];
        const Record32& q = v[idx[t + 1]];
        const Record32& r = v[idx[t + 2]];
        const double orient = (double(q.px) - p.px) * (double(r.pz) - p.pz)
                            - (double(q.pz) - p.pz) * (double(r.px) - p.px);
        s.area += std::fabs(orient) * 0.5;
        if (orient > 1e-4)
            ++s.counterClockwise;
        const double xs[3] = { p.px, q.px, r.px };
        const double zs[3] = { p.pz, q.pz, r.pz };
        const double minX = (std::min)(xs[0], (std::min)(xs[1], xs[2]));
        const double maxX = (std::max)(xs[0], (std::max)(xs[1], xs[2]));
        const double minZ = (std::min)(zs[0], (std::min)(zs[1], zs[2]));
        const double maxZ = (std::max)(zs[0], (std::max)(zs[1], zs[2]));
        const double gx = 2.0 * std::floor((minX + 0.0101) / 2.0);
        const double gz = 2.0 * std::floor((minZ + 0.0101) / 2.0);
        if (maxX > gx + 2.0101 || maxZ > gz + 2.0101)
        {
            ++s.outsideCell;
            continue;
        }
        double lo = 1e30;
        double hi = -1e30;
        for (int k = 0; k < 3; ++k)
        {
            const double d = xs[k] + zs[k] - (gx + gz + 2.0);
            lo = (std::min)(lo, d);
            hi = (std::max)(hi, d);
        }
        if (!(hi <= 0.002 || lo >= -0.002))
            ++s.acrossDiagonal;
    }
    return s;
}

const ParamValue* lastValue(const char* name)
{
    for (size_t i = g_paramValues.size(); i-- > 0;)
    {
        if (g_paramValues[i].name == name)
            return &g_paramValues[i];
    }
    return 0;
}

const ParamFloat* lastFloat(const char* name)
{
    for (size_t i = g_paramFloats.size(); i-- > 0;)
    {
        if (g_paramFloats[i].name == name)
            return &g_paramFloats[i];
    }
    return 0;
}

IRoadEditableRenderer* g_queue = 0;

IRoadEditable* newRoad()
{
    return static_cast<IRoadEditable*>(createRoadEditable(820011u, 0));
}

RoadEditable* concrete(IRoadEditable* road)
{
    return static_cast<RoadEditable*>(road);
}

IEditorSpline* newSpline(const float (*points)[3], int count)
{
    IEditorSpline* const spline = static_cast<IEditorSpline*>(createEditorSpline(80033u, 0));
    spline->setSnapFlags(0);
    for (int i = 0; i < count; ++i)
    {
        Vec3 p = { points[i][0], points[i][1], points[i][2] };
        spline->addControlPoint(p);
    }
    return spline;
}

// The highway-like template the geometry checks use: width 10, max error 0.25 (the constructor's),
// layer 1 scaled (1, 0.5), blend 0.85, material 7; layer 0 mirrors on request (three columns).
IRoadTemplate* newHighway(bool mirror0)
{
    IRoadTemplate* const roadTemplate = newTemplate();
    roadTemplate->setName(renddx9abi::HostString("highway"));
    roadTemplate->setWidth(10.0f);
    roadTemplate->setTextureFile(0, renddx9abi::HostString("roads\\tarmac"));
    roadTemplate->setTextureFile(1, renddx9abi::HostString("roads\\detail"));
    Vec2 scale = { 1.0f, 0.5f };
    roadTemplate->setTextureScale(1, scale);
    roadTemplate->setBlendFactor(0.85f);
    roadTemplate->setMaterial(7);
    roadTemplate->setMirror(0u, mirror0);
    return roadTemplate;
}

const float kStraight[4][3] = { { 0.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f }, { 20.0f, 0.0f, 0.0f },
                                { 30.0f, 0.0f, 0.0f } };


// The object: factory, QI, constructor state, slot round trips, compileTemplates, destructor.

void roadEditableObjectChecks()
{
    // The queue singleton: the destructor and draw read it untested (census hazard 7), so it exists
    // before the first road does.
    g_queue = static_cast<IRoadEditableRenderer*>(createRoadEditableRenderer(820020u, 0));
    geom::g_RoadEditableRenderer = g_queue;

    const int allocsBefore = g_allocs;
    check(createRoadEditable(820011u, reinterpret_cast<IBase*>(1)) == 0 && g_allocs == allocsBefore,
          "RoadEditable refuses aggregation before allocating");

    g_registrations.clear();
    const int freesBefore = g_frees;
    const int paramReleasesBefore = g_paramReleases;
    const int effectReleasesBefore = g_effectReleases;
    check(createRoadEditable(820012u, 0) == 0, "an unknown iid answers 0");
    check(g_registrations.size() == 2 && g_registrations[0].add && !g_registrations[1].add &&
          g_registrations[0].listener == g_registrations[1].listener && g_frees > freesBefore,
          "... and the refused road is released at once: handler registered, unregistered, block freed");
    check(g_paramReleases == paramReleasesBefore + 8 && g_effectReleases == effectReleasesBefore + 1,
          "the destructor releases all eight handles and the effect");

    g_registrations.clear();
    g_effects.clear();
    g_paramNames.clear();
    IRoadEditable* const road = newRoad();
    RoadEditable* const self = concrete(road);
    check(road != 0, "RoadEditable answers 820011");
    check(road->queryInterface(1u) == road && road->queryInterface(820010u) == road &&
          road->queryInterface(820011u) == road, "QI: 1, IRoad 820010 and IRoadEditable 820011 answer this");
    io::IConsoleSaveable* const saveable =
        static_cast<io::IConsoleSaveable*>(road->queryInterface(0x3709A4FFu));
    check(reinterpret_cast<char*>(saveable) == reinterpret_cast<char*>(road) + 4,
          "0x3709A4FF answers the IConsoleSaveable subobject (+4)");
    check(road->queryInterface(820012u) == 0 && road->queryInterface(820020u) == 0, "anything else answers 0");
    check(!saveable->makeScript(0), "makeScript answers false (the LevelEditor writes Roads.con)");
    IEventListener* const listener = g_registrations.empty() ? 0 : g_registrations[0].listener;
    check(g_registrations.size() == 1 && g_registrations[0].add && g_registrations[0].category == 8 &&
          reinterpret_cast<char*>(listener) == reinterpret_cast<char*>(road) + 8 &&
          g_registrations[0].priority == 0,
          "the constructor registers its +8 listener for ECEditor (8) at priority 0");
    check(g_effects.size() == 1 && g_effects[0] == "Shaders/Road", "the effect \"Shaders/Road\"");
    static const char* const kNames[8] = { "WorldViewProjection", "ViewPos", "TEXLAYER0", "TEXLAYER1",
                                           "BlendFactor", "Material", "fogRange", "fogColor" };
    bool namesOk = g_paramNames.size() == 8;
    for (int i = 0; namesOk && i < 8; ++i)
        namesOk = g_paramNames[static_cast<size_t>(i)] == kNames[i];
    check(namesOk, "its eight handles, in the constructor's order");
    check(!road->getTemplate() && !road->getSpline() && !road->getNoise() && road->getPriority() == 0 &&
          road->getGhost() && text(road->getName()).empty(),
          "no template, spline or noise; priority 0; a ghost; no name");
    check(road->getFadeDistAtStartLeft().x == 0.0f && road->getFadeDistAtStartLeft().y == 3.0f &&
          road->getFadeDistAtStartRight().x == 0.0f && road->getFadeDistAtStartRight().y == 3.0f &&
          road->getFadeDistAtEndLeft().x == 0.0f && road->getFadeDistAtEndLeft().y == 3.0f &&
          road->getFadeDistAtEndRight().x == 0.0f && road->getFadeDistAtEndRight().y == 3.0f,
          "every fade distance (0, 3)");
    check(road->getTextureScaleOffset(0).x == 0.0f && road->getTextureScaleOffset(0).y == 0.0f &&
          road->getTextureScaleOffset(1).x == 0.0f && road->getTextureScaleOffset(1).y == 0.0f,
          "two (0, 0) texture scale offsets");
    check(self->m_vertexCount == 0 && !self->m_vertexBuffer && !self->m_indexBuffer && self->m_radius == 0.0f &&
          self->m_bounds.dirty == 1 && !self->m_texture0 && !self->m_texture1,
          "no geometry, no textures; the bounds marked dirty");
    check(road->anyChunkVisible(), "anyChunkVisible answers true");
    road->updateChunkVisibility(0);

    // Round trips.
    road->setPriority(4);
    Vec2 fade = { 1.0f, 6.0f };
    road->setFadeDistAtStartLeft(fade);
    fade.x = 2.0f;
    fade.y = 7.0f;
    road->setFadeDistAtStartRight(fade);
    fade.x = 3.0f;
    fade.y = 8.0f;
    road->setFadeDistAtEndLeft(fade);
    fade.x = 4.0f;
    fade.y = 9.0f;
    road->setFadeDistAtEndRight(fade);
    check(road->getPriority() == 4 && self->m_priority == 4, "priority round trip (+0xF4)");
    check(&road->getFadeDistAtStartLeft() == &self->m_fadeDistAtStartLeft &&
          road->getFadeDistAtStartLeft().y == 6.0f &&
          &road->getFadeDistAtStartRight() == &self->m_fadeDistAtStartRight &&
          road->getFadeDistAtStartRight().x == 2.0f &&
          &road->getFadeDistAtEndLeft() == &self->m_fadeDistAtEndLeft && road->getFadeDistAtEndLeft().y == 8.0f &&
          &road->getFadeDistAtEndRight() == &self->m_fadeDistAtEndRight && road->getFadeDistAtEndRight().x == 4.0f,
          "fade round trips; each getter answers the member's address");
    Vec2 offset = { 0.5f, 0.25f };
    road->setTextureScaleOffset(1, offset);
    check(road->getTextureScaleOffset(1).x == 0.5f && road->getTextureScaleOffset(1).y == 0.25f &&
          road->getTextureScaleOffset(0).x == 0.0f, "texture scale offset round trip (layer 1 only)");
    road->setName(renddx9abi::HostString("bridge01"));
    check(text(road->getName()) == "bridge01", "name round trip");
    check(text(road->getCompiledMeshPath()) == "\\Roads\\bridge01_compiled.mesh",
          "getCompiledMeshPath = \\Roads\\<name>_compiled.mesh");
    road->setGhost(true);
    check(road->getGhost(), "setGhost to the current state does nothing");

    // Nothing to draw, compile or rebuild yet.
    const int pushes = g_pushes;
    road->draw();
    check(g_pushes == pushes, "draw does nothing without vertices");
    g_streams.clear();
    check(!road->compile(renddx9abi::HostString("Levels\\Test")) && g_streams.empty(),
          "compile answers false without vertices and opens nothing");
    road->invalidate();
    check(!self->m_vertexBuffer && self->m_vertexCount == 0, "invalidate without a spline or template builds nothing");

    // compileTemplates is a stub that answers true; the by-value set is the callee's to destroy.
    {
        RoadTemplateNameSet templates;
        templates.insert(renddx9abi::HostString("highway"));
        templates.insert(renddx9abi::HostString("dirt"));
        check(road->compileTemplates(renddx9abi::HostString("Levels\\Test"), templates),
              "compileTemplates answers true");
    }

    // Destruction: off the queue, the handler unregistered, handles and effect released, block freed.
    g_queue->add(road);
    g_registrations.clear();
    const int params = g_paramReleases;
    const int effects = g_effectReleases;
    const int frees = g_frees;
    check(road->release() == 0, "release answers 0");
    check(g_registrations.size() == 1 && !g_registrations[0].add && g_registrations[0].category == 8 &&
          g_registrations[0].listener == listener, "the destructor unregisters the +8 listener from ECEditor");
    check(g_paramReleases == params + 8 && g_effectReleases == effects + 1 && g_frees > frees,
          "... releases the eight handles and the effect, and frees the block");
    g_drawPrimitive.clear();
    g_indexedDrawArgs.clear();
    g_queue->drawAll();
    check(g_drawPrimitive.empty() && g_indexedDrawArgs.empty(), "the destructor took the road off the queue (+0x14)");
}


// A straight road along +x: ghost, draw, final, compile, the shipped buffer hazards, the events.

void straightRoadChecks()
{
    IRoadTemplate* const highway = newHighway(false);
    IEditorSpline* const spline = newSpline(kStraight, 4);
    IRoadEditable* const road = newRoad();
    RoadEditable* const self = concrete(road);
    IEventListener* const listener = g_registrations.back().listener;
    road->setName(renddx9abi::HostString("bridge01"));
    road->setPriority(4);

    const size_t buffers = g_vertexBuffers.size();
    road->setSpline(spline);
    check(road->getSpline() == spline && g_vertexBuffers.size() == buffers,
          "a spline alone builds nothing (invalidate wants both)");
    g_textureRequests.clear();
    g_textureReleases.clear();
    g_poolSwaps.clear();
    road->setTemplate(highway);
    check(road->getTemplate() == highway, "setTemplate stores the template");
    check(g_textureReleases.size() == 2 && !g_textureReleases[0] && !g_textureReleases[1] &&
          g_textureRequests.size() == 2 && g_textureRequests[0] == "roads\\tarmac" &&
          g_textureRequests[1] == "roads\\detail" && self->m_texture0 && self->m_texture1 &&
          self->m_texture0 != self->m_texture1,
          "setTemplate releases both texture slots, then loads the template's two files by name");
    check(g_vertexBuffers.size() == buffers + 1 && self->m_vertexBuffer == g_vertexBuffers.back(),
          "... then invalidate builds the road - a ghost, the constructor's state");
    check(g_poolSwaps.size() == 2 && g_poolSwaps[0] == 1 && g_poolSwaps[1] == 7,
          "the buffers are made inside a BufferManager pool swap to 1, restored after");

    // the two-column ghost: a strip of right, left per kept row
    const FakeVertexBuffer& ghost = *static_cast<FakeVertexBuffer*>(self->m_vertexBuffer);
    check(ghost.count == 4 && ghost.stride == 32 && ghost.name == "RoadEditable" && ghost.usage == 0,
          "the vertex buffer is (vertices, 32, \"RoadEditable\", 0)");
    check(!self->m_indexBuffer && self->m_vertexCount == 4 && self->m_triangleCount == 2,
          "a straight two-column ghost keeps two rows: four vertices, two strip triangles, no index buffer");
    const std::vector<Vec3> left = edgeSamples(spline, 5.0f);
    const std::vector<Vec3> right = edgeSamples(spline, -5.0f);
    const std::vector<Record32> g = records(self->m_vertexBuffer);
    check(left.size() > 50 && left.size() == right.size(), "the walk hands out matching edge samples");
    check(same(positionOf(g[0]), right.front()) && same(positionOf(g[1]), left.front()) &&
          same(positionOf(g[2]), right.back()) && same(positionOf(g[3]), left.back()),
          "the rows are the first and the last sample (a straight edge never exceeds the error); right, left");
    check(left.front().x == 10.0f && closeTo(left.front().z, 5.0f, 1e-5f) && closeTo(right.front().z, -5.0f, 1e-5f),
          "the walk starts at the second control point; left is the +width/2 copy (+z for a road along +x)");
    const float rowLength = lengthZYX(g[2].px - g[0].px, g[2].py - g[0].py, g[2].pz - g[0].pz);
    check(g[1].u0 == 0.0f && g[1].v0 == 0.0f && g[0].u0 == 1.0f && g[0].v0 == 0.0f && g[0].u1 == 1.0f &&
          g[1].u1 == 0.0f, "row 0: u 0 on the left, 1 / scale on the right; v 0");
    check(g[2].v0 == 0.1f * rowLength && g[3].v0 == g[2].v0 && g[2].v1 == 0.2f * rowLength && g[3].v1 == g[2].v1,
          "v = scale.x / width / scale.y x the right edge's run, copied to the left (layer 1: 1 / 10 / 0.5)");
    check(g[0].alpha == 0.0f && g[1].alpha == 0.0f && g[2].alpha == 0.0f && g[3].alpha == 0.0f,
          "default fades (0, 3): both rows of a two-row road are transparent (the start and the end ramp)");
    check(g_boundingStride == 64 && g_boundingCount == 4, "D3DX bounds the 64-byte working vertices");
    check(self->m_bounds.min.x == g_boxMin[0] && self->m_bounds.min.z == g_boxMin[2] &&
          self->m_bounds.max.x == g_boxMax[0] && self->m_bounds.max.z == g_boxMax[2] && self->m_bounds.dirty == 1 &&
          self->m_centre.x == g_sphereCentre[0] && self->m_centre.z == g_sphereCentre[2] &&
          self->m_radius == g_sphereRadius,
          "the box goes to the bounds (+0x80 / +0x8C, dirty 1), the sphere to +0x70 / +0x7C");

    // draw, through the queue
    g_queue->add(road);
    Mat4 inverseView = Mat4::getIdentity();
    inverseView.m[12] = 1.0f;
    inverseView.m[13] = 2.0f;
    inverseView.m[14] = 3.0f;
    Mat4 projection = Mat4::getIdentity();
    projection.m[0] = 2.0f;
    projection.m[5] = 3.0f;
    projection.m[10] = 4.0f;
    projection.m[11] = 1.0f;
    g_identity = inverseView;
    g_projection = projection;
    g_events.clear();
    g_drawPrimitive.clear();
    g_indexedDrawArgs.clear();
    g_renderStates.clear();
    g_paramValues.clear();
    g_paramFloats.clear();
    g_techniques.clear();
    g_passes.clear();
    int pushes = g_pushes;
    int pops = g_pops;
    g_queue->drawAll();
    check(g_pushes == pushes + 1 && g_pops == pops + 1, "the queue draws the road inside one state bracket");
    static const unsigned int kStates[8] = { 195u, 0x3A83126Fu, 175u, 0x3F000000u, 195u, 0u, 175u, 0u };
    check(g_renderStates.size() == 8 && std::equal(kStates, kStates + 8, g_renderStates.begin()),
          "depth bias (195) and slope-scale bias (175) from RoadCompiledRenderer, both back to 0 after");
    Mat4 expected = Mat4::getIdentity();
    expected.fullMult(inverseView, projection);
    const ParamValue* const wvp = lastValue("WorldViewProjection");
    check(wvp && wvp->count == 0 && std::memcmp(wvp->matrix, &expected, 64) == 0,
          "WorldViewProjection = inverse view x projection");
    const ParamValue* const viewPos = lastValue("ViewPos");
    check(viewPos && viewPos->value == &g_identity.m[12], "ViewPos = the inverse view's translation row");
    const ParamValue* const layer0 = lastValue("TEXLAYER0");
    const ParamValue* const layer1 = lastValue("TEXLAYER1");
    check(layer0 && layer1 && layer0->value == self->m_texture0 && layer1->value == self->m_texture1,
          "TEXLAYER0 / TEXLAYER1 = the template's two textures");
    const ParamFloat* const blend = lastFloat("BlendFactor");
    const ParamFloat* const material = lastFloat("Material");
    check(blend && material && blend->value == 0.85f && material->value == static_cast<float>(7) * 0.0039215689f,
          "BlendFactor = the template's; Material = material / 255");
    const ParamValue* const fogRange = lastValue("fogRange");
    const ParamValue* const fogColor = lastValue("fogColor");
    check(fogRange && fogColor && fogRange->value == g_raStorage + 0xA8 && fogColor->value == g_raStorage + 0xBC,
          "the Ra constants' fog range (+0xA8) and colour (+0xBC)");
    check(!g_techniques.empty() && g_techniques.back() == "roadeditable" && g_passes.size() == 1 && g_passes[0] == 0,
          "technique \"roadeditable\", pass = the queue's mode word (0)");
    check(g_drawPrimitive.size() == 3 && g_drawPrimitive[0] == 2 && g_drawPrimitive[1] == 5 &&
          g_drawPrimitive[2] == 0 && g_indexedDrawArgs.empty(),
          "no index buffer: DrawPrimitive(triangles, STRIP, 0)");

    RoadEditableRenderer* const queueObject = static_cast<RoadEditableRenderer*>(g_queue);
    queueObject->m_mode = 1;
    g_passes.clear();
    road->draw();
    check(g_passes.size() == 1 && g_passes[0] == 1, "the road-mask pass (mode 1) draws a material in 1..255, pass 1");
    highway->setMaterial(0);
    g_passes.clear();
    road->draw();
    highway->setMaterial(256);
    road->draw();
    check(g_passes.empty(), "but neither material 0 nor 256");
    highway->setMaterial(7);
    queueObject->m_mode = 0;
    g_identity = Mat4::getIdentity();
    g_projection = Mat4::getIdentity();

    // setGhost(false): the final road - diced, ear-clipped, textured, draped
    FakeVertexBuffer* const ghostBuffer = static_cast<FakeVertexBuffer*>(self->m_vertexBuffer);
    g_log.clear();
    road->setGhost(false);
    check(!road->getGhost() && self->m_vertexBuffer != ghostBuffer, "setGhost(false) rebuilds");
    check(ghostBuffer->releases == 1 && ghostBuffer->removes == 1,
          "the old vertex buffer: its owner reference dropped and removed from the BufferManager");
    FakeIndexBuffer* const finalIndices = static_cast<FakeIndexBuffer*>(self->m_indexBuffer);
    check(finalIndices && finalIndices->usage == 0 && finalIndices->format == 101 &&
          finalIndices->count == static_cast<unsigned int>(self->m_triangleCount) * 3u,
          "an index buffer (indices, 0, INDEX16) of triangles x 3");
    const std::vector<Record32> f = records(self->m_vertexBuffer);
    const std::vector<unsigned short> fi = indexData(self->m_indexBuffer);
    check(static_cast<int>(f.size()) == self->m_vertexCount && self->m_triangleCount > 20,
          "the vertex count; a diced road has many triangles");
    const MeshStats st = analyse(f, fi);
    const double quad = 10.0 * (double(left.back().x) - 10.0);
    check(st.indicesValid, "every index names a vertex");
    check(std::fabs(st.area - quad) <= 1e-3 * quad, "the triangles tile the road's quad (x/z area)");
    check(st.counterClockwise == 0, "every triangle winds one way (emitTriangle: orient(p, q, r) <= 0)");
    check(st.outsideCell == 0, "every triangle lies inside one 2 m cell of the even grid");
    check(st.acrossDiagonal == 0, "... and on one side of its cell's x + z diagonal");
    bool draped = true;
    bool uvOk = true;
    bool alphaOk = true;
    for (size_t i = 0; i < f.size(); ++i)
    {
        draped = draped && f[i].py == 0.5f * f[i].px + 0.25f * f[i].pz;
        const float u = (5.0f - f[i].pz) / 10.0f;
        const float v = 0.1f * (f[i].px - 10.0f);
        uvOk = uvOk && closeTo(f[i].u0, u, 2e-5f) && closeTo(f[i].u1, u, 2e-5f) && closeTo(f[i].v0, v, 1e-4f) &&
               closeTo(f[i].v1, 2.0f * v, 2e-4f);
        alphaOk = alphaOk && f[i].alpha == 0.0f;
    }
    check(draped, "every vertex is set on the terrain (HeightmapCluster +0x68; 0.5x + 0.25z here)");
    check(uvOk, "every vertex, the dicing's included, carries the columns' affine texture map");
    check(alphaOk, "and the two rows' alpha (0)");
    check(g_log.empty(), "no piece fell below three vertices (no line-501 log)");

    // invalidate, then the rebuild it runs: the vertex buffer is dropped twice (shipped)
    FakeVertexBuffer* const finalBuffer = static_cast<FakeVertexBuffer*>(self->m_vertexBuffer);
    road->invalidate();
    check(finalBuffer->releases == 2 && finalBuffer->removes == 1,
          "⚠ invalidate drops the vertex buffer's owner reference and keeps the pointer; the rebuild drops it "
          "again, then removes it (census hazard: a double release)");
    check(finalIndices->releases == 1 && finalIndices->removes == 0,
          "the index buffer is dropped once, cleared, and left in the BufferManager's set");
    const std::vector<Record32> again = records(self->m_vertexBuffer);
    const std::vector<unsigned short> againIndices = indexData(self->m_indexBuffer);
    check(self->m_vertexBuffer != finalBuffer && again.size() == f.size() && againIndices.size() == fi.size() &&
          std::memcmp(&again[0], &f[0], f.size() * sizeof(Record32)) == 0 &&
          std::equal(fi.begin(), fi.end(), againIndices.begin()),
          "the rebuild is new buffers with the same bytes (the tessellation is deterministic)");

    // a texture scale offset joins the template's scale, at the next invalidate
    size_t made = g_vertexBuffers.size();
    Vec2 offset0 = { 1.0f, 0.0f };
    road->setTextureScaleOffset(0, offset0);
    check(g_vertexBuffers.size() == made, "setTextureScaleOffset does not rebuild");
    road->invalidate();
    const std::vector<Record32> scaled = records(self->m_vertexBuffer);
    bool offsetOk = !scaled.empty();
    for (size_t i = 0; i < scaled.size(); ++i)
    {
        offsetOk = offsetOk && closeTo(scaled[i].u0, 0.5f * (5.0f - scaled[i].pz) / 10.0f, 2e-5f) &&
                   closeTo(scaled[i].v0, 0.2f * (scaled[i].px - 10.0f), 2e-4f) &&
                   closeTo(scaled[i].u1, (5.0f - scaled[i].pz) / 10.0f, 2e-5f);
    }
    check(offsetOk, "scale = offset + template scale: layer 0's u spans 1 / 2 and its v doubles; layer 1 unchanged");
    offset0.x = 0.0f;
    road->setTextureScaleOffset(0, offset0);
    road->invalidate();

    // compile
    g_streams.clear();
    g_settingRequests.clear();
    g_singletonAsRequests.clear();
    g_terrainQueries.clear();
    g_log.clear();
    made = g_vertexBuffers.size();
    check(road->compile(renddx9abi::HostString("Levels\\Test")), "compile answers true");
    check(g_vertexBuffers.size() == made, "a final road is not rebuilt by compile");
    check(g_settingRequests.size() == 1 && g_settingRequests[0] == "Editor_LevelSavePath| ERROR ",
          "the save path is the Editor_LevelSavePath setting (default \" ERROR \")");
    check(g_singletonAsRequests.size() == 1 && g_singletonAsRequests[0] == "Terrain|820004" &&
          g_terrainQueries.size() == 1 && g_terrainQueries[0] == 820005u,
          "the Terrain singleton as ITerrain (+0x4C), queried for ITerrainEditable - both answers unused");
    check(g_streams.size() == 2, "compile opens two files");
    if (g_streams.size() == 2)
    {
        const FakeStream& con = *g_streams[0];
        const FakeStream& mesh = *g_streams[1];
        check(con.path == "C:\\Save\\Levels\\Test\\CompiledRoads.con" && con.mode == 3 && con.access == 2,
              "<save path>\\<level>\\CompiledRoads.con, mode 3");
        check(con.seeks == 1 && con.seekOrigin == 2 && con.seekOffset == 0,
              "... seeked to its end (+0x30 (2, 0)): compile appends");
        std::ostringstream expectedText;
        expectedText << "object.create highway\nobject.geometry.loadMesh Levels\\Test\\Roads\\bridge01_compiled.mesh\n"
                     << "object.absoluteposition " << self->m_centre.x << '/' << self->m_centre.y << '/'
                     << self->m_centre.z << "\n\n";
        check(con.bytes == expectedText.str(),
              "object.create <template>, object.geometry.loadMesh <level>\\Roads\\<name>_compiled.mesh, "
              "object.absoluteposition <centre>, a blank line");
        check(con.releases == 1 && mesh.releases == 1, "both streams released once");
        check(mesh.path == "Levels\\Test\\Roads\\bridge01_compiled.mesh" && mesh.mode == 0 && mesh.access == 2,
              "the mesh is <level> + getCompiledMeshPath(), mode 0 (no save-path prefix)");
        const std::vector<Record32> c = records(self->m_vertexBuffer);
        const std::vector<unsigned short> ci = indexData(self->m_indexBuffer);
        const size_t size = 52u + 32u * c.size() + 4u + 2u * ci.size();
        check(mesh.bytes.size() == size,
              "52 header bytes, 32 per vertex, the index count and 2 per index - and NO chunk table");
        if (mesh.bytes.size() == size)
        {
            const char* const p = mesh.bytes.data();
            unsigned int version = 0;
            float header[10];
            int priority = 0;
            int vertices = 0;
            std::memcpy(&version, p, 4);
            std::memcpy(header, p + 4, 40);
            std::memcpy(&priority, p + 44, 4);
            std::memcpy(&vertices, p + 48, 4);
            check(version == 0x10004u, "version 1.4 (u16 minor 4, u16 major 1)");
            check(header[0] == self->m_centre.x && header[1] == self->m_centre.y && header[2] == self->m_centre.z &&
                  header[3] == self->m_radius && header[4] == self->m_bounds.min.x &&
                  header[6] == self->m_bounds.min.z && header[7] == self->m_bounds.max.x &&
                  header[9] == self->m_bounds.max.z, "then the sphere centre, radius, the box min and max");
            check(priority == 4 && vertices == static_cast<int>(c.size()),
                  "the road's priority as the order word, then the vertex count");
            bool vertexOk = true;
            for (size_t i = 0; i < c.size(); ++i)
            {
                float v[8];
                std::memcpy(v, p + 52 + 32 * i, 32);
                vertexOk = vertexOk && v[0] == c[i].px - self->m_centre.x && v[1] == c[i].py - self->m_centre.y &&
                           v[2] == c[i].pz - self->m_centre.z && v[3] == c[i].u0 && v[4] == c[i].v0 &&
                           v[5] == c[i].u1 && v[6] == c[i].v1 && v[7] == c[i].alpha;
            }
            check(vertexOk, "each vertex read back from the buffer: position - centre, uv0, uv1, alpha");
            unsigned int indexCount = 0;
            std::memcpy(&indexCount, p + 52 + 32 * c.size(), 4);
            check(indexCount == ci.size() &&
                  std::memcmp(p + 56 + 32 * c.size(), &ci[0], 2 * ci.size()) == 0,
                  "the index count (triangles x 3), then every index as a u16");
        }
    }
    check(g_log.empty(), "no line-2548 log for an indexed road");

    // A ghost compiles as a final road and is a ghost again after.
    road->setGhost(true);
    check(!self->m_indexBuffer, "a two-column ghost again: no index buffer");
    g_streams.clear();
    made = g_vertexBuffers.size();
    check(road->compile(renddx9abi::HostString("Levels\\Test")), "a ghost road compiles");
    check(g_vertexBuffers.size() == made + 2 && road->getGhost() && !self->m_indexBuffer,
          "... rebuilt final before writing, and as a ghost after");
    if (g_streams.size() == 2)
    {
        int vertices = 0;
        std::memcpy(&vertices, g_streams[1]->bytes.data() + 48, 4);
        check(vertices == static_cast<int>(f.size()), "the written mesh is the final road");
    }
    road->setGhost(false);

    // the editor's events, through the registered listener
    made = g_vertexBuffers.size();
    listener->handleEvent(ECEditor, 200u, 0, spline);
    check(g_vertexBuffers.size() == made + 1, "200 for its spline: invalidate (a rebuild)");
    listener->handleEvent(ECEditor, 200u, 0, highway);
    listener->handleEvent(ECEditor, 300u, 0, spline);
    listener->handleEvent(ECEditor, 302u, 0, spline);
    listener->handleEvent(ECEditor, 304u, 0, spline);
    listener->handleEvent(ECEditor, 999u, 0, road);
    check(g_vertexBuffers.size() == made + 1 && !road->getGhost(), "another sender, or an unknown id, does nothing");
    listener->handleEvent(ECEditor, 300u, 0, highway);
    listener->handleEvent(ECEditor, 302u, 0, road);
    listener->handleEvent(ECEditor, 303u, 0, road);
    check(g_vertexBuffers.size() == made + 4, "300 for its template, 302 and 303 for the road: invalidate");
    listener->handleEvent(ECEditor, 305u, 0, road);
    check(road->getGhost() && !self->m_indexBuffer, "305 for the road: setGhost(true)");
    listener->handleEvent(ECEditor, 304u, 0, road);
    check(!road->getGhost() && self->m_indexBuffer, "304 for the road: setGhost(false)");
    g_posted.clear();
    made = g_vertexBuffers.size();
    listener->handleEvent(ECEditor, 206u, 0, spline);
    check(g_posted.size() == 1 && g_vertexBuffers.size() == made + 2,
          "206 for its spline: projectToTerrain (a footprint buffer, the heightmap event), then invalidate");
    g_textureReleases.clear();
    const int vertexCount = self->m_vertexCount;
    FakeVertexBuffer* const kept = static_cast<FakeVertexBuffer*>(self->m_vertexBuffer);
    listener->handleEvent(ECEditor, 301u, 0, highway);
    check(!road->getTemplate() && g_textureReleases.empty() && self->m_vertexCount == vertexCount &&
          self->m_vertexBuffer == kept && kept->releases == 1 && !self->m_indexBuffer,
          "301 for its template: setTemplate(0) - the textures kept; ⚠ the vertex count and the released "
          "vertex buffer left for draw (census hazard 2: draw would read the null template)");

    // destruction
    g_textureReleases.clear();
    dx9rend::EngineTexture* const texture0 = self->m_texture0;
    dx9rend::EngineTexture* const texture1 = self->m_texture1;
    check(road->release() == 0, "the road releases");
    check(kept->releases == 2 && kept->removes == 0,
          "⚠ the destructor drops the vertex buffer again and never removes it from the BufferManager");
    check(g_textureReleases.size() == 2 && g_textureReleases[0] == texture0 && g_textureReleases[1] == texture1,
          "the destructor releases both textures");
    check(spline->release() == 0 && highway->release() == 0, "the spline and the template release");
}


// Three columns (a mirrored layer): the ghost's twelve-index rows, the final road's two halves.

void mirroredRoadChecks()
{
    IRoadTemplate* const highway = newHighway(true);
    IEditorSpline* const spline = newSpline(kStraight, 4);
    IRoadEditable* const road = newRoad();
    RoadEditable* const self = concrete(road);
    road->setSpline(spline);
    road->setTemplate(highway);
    const std::vector<Record32> g = records(self->m_vertexBuffer);
    check(g.size() == 6 && self->m_triangleCount == 4 && self->m_indexBuffer,
          "a mirrored layer makes three columns: rows of left, centre, right; 4 x ((6 - 2) / 3) triangles; "
          "an index buffer even for a ghost");
    static const unsigned short kPattern[12] = { 0, 3, 1, 1, 3, 4, 1, 4, 2, 2, 4, 5 };
    const std::vector<unsigned short> gi = self->m_indexBuffer ? indexData(self->m_indexBuffer)
                                                               : std::vector<unsigned short>();
    check(gi.size() == 12 && std::equal(kPattern, kPattern + 12, gi.begin()),
          "twelve indices per row pair: (L0 L1 C0) (C0 L1 C1) (C0 C1 R0) (R0 C1 R1)");
    if (g.size() == 6)
    {
        check(g[1].px == (g[2].px + g[0].px) * 0.5f && g[1].pz == (g[2].pz + g[0].pz) * 0.5f,
              "the centre is (right + left) * 0.5");
        check(g[0].u0 == 0.0f && g[1].u0 == 1.0f && g[2].u0 == 0.0f && g[0].u1 == 0.0f && g[1].u1 == 0.5f &&
              g[2].u1 == 1.0f, "u across: the mirrored layer 0 / 1 / 0, the other 0 / 0.5 / 1");
        check(analyse(g, gi).counterClockwise == 0, "the ghost list winds like the final road");
    }

    road->setGhost(false);
    const std::vector<Record32> f = records(self->m_vertexBuffer);
    const std::vector<unsigned short> fi = indexData(self->m_indexBuffer);
    const MeshStats st = analyse(f, fi);
    const float x1 = g.size() == 6 ? g[3].px : 0.0f;
    const double quad = 10.0 * (double(x1) - 10.0);
    check(st.indicesValid && st.triangles == self->m_triangleCount, "every index names a vertex");
    check(std::fabs(st.area - quad) <= 1e-3 * quad, "the two halves tile the road's quad");
    check(st.counterClockwise == 0 && st.outsideCell == 0 && st.acrossDiagonal == 0,
          "one winding; every triangle in one even 2 m cell, on one side of its diagonal");
    // Texture coordinates. The first mode-0 pass walks the left half's quads, and its edge tests
    // (orient == 0) test the edge's whole line, so a right-half vertex the dicing put on a row line
    // (x = 10 or x = x1) is taken by the left quad first (shipped, preserved): on the first row line it
    // gets the left quad's values at its distance from the centre (u0 right by symmetry, u1 the mirror
    // image's); on the last it is extrapolated from the left corner (u0 = 1 + |z| / 5, u1 right).
    bool draped = true;
    bool uvOk = true;
    int rowLineVertices = 0;
    for (size_t i = 0; i < f.size(); ++i)
    {
        const Record32& v = f[i];
        draped = draped && v.py == 0.5f * v.px + 0.25f * v.pz;
        const float a = std::fabs(v.pz) / 5.0f;
        float u0 = 1.0f - a;
        float u1 = (5.0f - v.pz) / 10.0f;
        if (static_cast<unsigned int>(i) >= 6u && v.pz < -1e-6f)
        {
            if (v.px == 10.0f)
            {
                u1 = 0.5f * (1.0f - a);
                ++rowLineVertices;
            }
            else if (v.px == x1)
            {
                u0 = 1.0f + a;
                ++rowLineVertices;
            }
        }
        uvOk = uvOk && closeTo(v.u0, u0, 2e-5f) && closeTo(v.u1, u1, 2e-5f) &&
               closeTo(v.v0, 0.1f * (v.px - 10.0f), 1e-4f) && closeTo(v.v1, 0.2f * (v.px - 10.0f), 2e-4f);
    }
    check(draped, "the final road is draped");
    check(uvOk && rowLineVertices > 0,
          "the halves' texture map, with the shipped first-pass capture of the right half's row-line vertices");
    check(road->release() == 0 && spline->release() == 0 && highway->release() == 0, "the objects release");
}


// A curved road: the chord-error walk, the texture walk and the fade ramps against independent
// statements; the final road's tiling; the noise.

void curvedRoadChecks()
{
    // A quarter circle of radius 30 about the origin, walked from 0 to 90 degrees (the first and the
    // last control point only shape the ends).
    float arc[6][3];
    for (int i = 0; i < 6; ++i)
    {
        const double angle = (-30.0 + 30.0 * i) * 3.14159265358979323846 / 180.0;
        arc[i][0] = static_cast<float>(30.0 * std::cos(angle));
        arc[i][1] = 0.0f;
        arc[i][2] = static_cast<float>(30.0 * std::sin(angle));
    }
    IRoadTemplate* const highway = newHighway(false);
    IEditorSpline* const spline = newSpline(arc, 6);
    IRoadEditable* const road = newRoad();
    RoadEditable* const self = concrete(road);
    road->setSpline(spline);
    road->setTemplate(highway);

    const std::vector<Record32> g = records(self->m_vertexBuffer);
    const std::vector<Vec3> l = edgeSamples(spline, 5.0f);
    const std::vector<Vec3> r = edgeSamples(spline, -5.0f);
    const size_t rows = g.size() / 2;
    std::vector<size_t> kept;
    bool mapped = rows >= 4 && g.size() % 2 == 0 && l.size() == r.size();
    for (size_t k = 0; mapped && k < rows; ++k)
    {
        size_t i = kept.empty() ? 0 : kept.back() + 1;
        while (i < l.size() && !(same(l[i], positionOf(g[2 * k + 1])) && same(r[i], positionOf(g[2 * k]))))
            ++i;
        mapped = i < l.size();
        if (mapped)
            kept.push_back(i);
    }
    check(mapped && kept.front() == 0 && kept.back() == l.size() - 1,
          "every kept row is one sample pair, from the first sample to the last");
    const double maxError = 0.25;
    bool within = mapped;
    bool reasoned = mapped;
    for (size_t k = 1; mapped && k < kept.size(); ++k)
    {
        const size_t a = kept[k - 1];
        const size_t b = kept[k];
        for (size_t j = a + 1; j < b; ++j)
        {
            within = within && lineDistance(l[a], l[b], l[j]) <= maxError + 1e-4 &&
                     lineDistance(r[a], r[b], r[j]) <= maxError + 1e-4;
        }
        if (b + 1 < l.size())
        {
            bool exceeds = false;
            for (size_t j = a + 1; j <= b; ++j)
            {
                exceeds = exceeds || lineDistance(l[a], l[b + 1], l[j]) > maxError - 1e-4 ||
                          lineDistance(r[a], r[b + 1], r[j]) > maxError - 1e-4;
            }
            reasoned = reasoned && exceeds;
        }
    }
    check(within, "every skipped sample lies within the max error of the chord between its kept rows, both edges");
    check(reasoned, "and each kept row is the last before the chord to the next sample would exceed it");

    bool uOk = true;
    bool vOk = true;
    float v0 = 0.0f;
    float v1 = 0.0f;
    for (size_t k = 0; k < rows; ++k)
    {
        const Record32& lv = g[2 * k + 1];
        const Record32& rv = g[2 * k];
        uOk = uOk && lv.u0 == 0.0f && lv.u1 == 0.0f && rv.u0 == 1.0f && rv.u1 == 1.0f;
        if (k)
        {
            const Record32& previous = g[2 * k - 2];
            const float run = lengthZYX(rv.px - previous.px, rv.py - previous.py, rv.pz - previous.pz);
            v0 = 0.1f * run + v0;
            v1 = 0.2f * run + v1;
        }
        vOk = vOk && rv.v0 == v0 && lv.v0 == v0 && rv.v1 == v1 && lv.v1 == v1;
    }
    check(uOk && vOk, "u is 0 / 1 / scale across; v adds along-scale x each row's right-edge run, on both columns");

    // The ramps: alpha_k = clamp(min(d_k / 3, (D - d_k) / 3), 0, 1) along each column, 0 on row 0.
    bool alphaOk = true;
    for (int column = 0; column < 2; ++column)
    {
        std::vector<double> distance(1, 0.0);
        for (size_t k = 1; k < rows; ++k)
        {
            const Record32& p = g[2 * k + (column ? 0 : 1)];
            const Record32& q = g[2 * k - 2 + (column ? 0 : 1)];
            const double dx = double(p.px) - q.px;
            const double dy = double(p.py) - q.py;
            const double dz = double(p.pz) - q.pz;
            distance.push_back(distance.back() + std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        const double total = distance.back();
        for (size_t k = 0; k < rows; ++k)
        {
            const double ramp = k ? (std::max)(0.0, (std::min)(1.0, (std::min)(distance[k] / 3.0,
                                                                             (total - distance[k]) / 3.0)))
                                  : 0.0;
            alphaOk = alphaOk && std::fabs(g[2 * k + (column ? 0 : 1)].alpha - ramp) <= 1e-4;
        }
    }
    check(alphaOk, "alpha ramps 0 -> 1 over the first 3 m and back to 0 over the last 3 m of each edge");

    road->setGhost(false);
    const std::vector<Record32> f = records(self->m_vertexBuffer);
    const std::vector<unsigned short> fi = indexData(self->m_indexBuffer);
    const MeshStats st = analyse(f, fi);
    double outline = 0.0;                                // left rows, then the right rows back
    std::vector<Vec3> ring;
    for (size_t k = 0; k < rows; ++k)
        ring.push_back(positionOf(g[2 * k + 1]));
    for (size_t k = rows; k-- > 0;)
        ring.push_back(positionOf(g[2 * k]));
    for (size_t k = 0; k < ring.size(); ++k)
    {
        const Vec3& a = ring[k];
        const Vec3& b = ring[(k + 1) % ring.size()];
        outline += double(a.x) * b.z - double(b.x) * a.z;
    }
    outline = std::fabs(outline) * 0.5;
    check(st.indicesValid && std::fabs(st.area - outline) <= 1e-3 * outline,
          "the curved road's triangles tile its outline");
    check(st.counterClockwise == 0 && st.outsideCell == 0 && st.acrossDiagonal == 0,
          "one winding; every triangle in one even 2 m cell, on one side of its diagonal");
    // Allow small range overshoot from interpolation rounding and boundary fallback extrapolation.
    // A vertex outside its quad may choose the nearer neighbouring column and intersect beyond that segment.
    bool draped = true;
    bool ranges = true;
    int extrapolated = 0;
    float worst = 0.0f;
    for (size_t i = 0; i < f.size(); ++i)
    {
        draped = draped && f[i].py == 0.5f * f[i].px + 0.25f * f[i].pz;
        ranges = ranges && f[i].alpha >= -1e-5f && f[i].alpha <= 1.00001f && f[i].u0 >= -0.1f && f[i].u0 <= 1.1f &&
                 f[i].v0 >= -1e-3f && f[i].v0 <= v0 + 1e-3f;
        const float excess = (std::max)(f[i].u0 - 1.0f, -f[i].u0);
        if (excess > 1e-4f)
        {
            ++extrapolated;
            worst = (std::max)(worst, excess);
        }
    }
    check(draped && ranges,
          "draped; alpha in [0, 1] to float rounding; u and v inside the columns' ranges up to mode 1's extrapolation");
    std::printf("  info: %d curved-road vertex(es) carry a u0 outside [0, 1], by at most %.4g (mode 1, shipped)\n",
                extrapolated, worst);

    // The noise moves x only: kept row i's left vertex by sample(i * 0.1), its right one by
    // sample(i * 0.1 + 1000); row 0 is not moved.
    IPerlinNoise* const noise = static_cast<IPerlinNoise*>(createPerlinNoise(80035u, 0));
    noise->setSeed(1234);
    noise->setInterpolation(0);
    Vec2 range = { -0.15f, 0.15f };
    noise->setOutputRange(range);
    noise->setOctaves(3);
    noise->setPersistence(0.2f);
    road->setGhost(true);
    road->setNoise(noise);
    check(road->getNoise() == noise, "noise round trip");
    const std::vector<Record32> n = records(self->m_vertexBuffer);
    bool noiseOk = n.size() >= 4 && same(positionOf(n[1]), l[0]) && same(positionOf(n[0]), r[0]);
    bool moved = false;
    for (size_t k = 1; noiseOk && k < n.size() / 2; ++k)
    {
        const Record32& lv = n[2 * k + 1];
        const Record32& rv = n[2 * k];
        size_t i = 1;
        while (i < l.size() && l[i].z != lv.pz)
            ++i;
        noiseOk = i < l.size() && r[i].z == rv.pz && l[i].y == lv.py && r[i].y == rv.py &&
                  lv.px == noise->sample(static_cast<float>(i) * 0.1f, 0.0f, 0.0f, 0.0f) + l[i].x &&
                  rv.px == noise->sample(static_cast<float>(i) * 0.1f + 1000.0f, 0.0f, 0.0f, 0.0f) + r[i].x;
        moved = moved || lv.px != l[i].x;
    }
    check(noiseOk && moved, "the noise moves x only: left by sample(i * 0.1), right by sample(i * 0.1 + 1000)");
    check(road->release() == 0 && spline->release() == 0 && highway->release() == 0 && noise->release() == 0,
          "the objects release");
}


// projectToTerrain: the 8-bit target, the float target, the size retries and the shipped give-up.

void clearProjectionRecords()
{
    g_surfaceRequests.clear();
    g_targets.clear();
    g_deviceCalls.clear();
    g_shaderConstants.clear();
    g_heightWrites.clear();
    g_clusterQueries.clear();
    g_posted.clear();
    g_savedSurfaces.clear();
    g_formatQueries.clear();
    g_loadSurfaceCalls = 0;
    g_techniques.clear();
    g_passes.clear();
    g_drawPrimitive.clear();
    g_indexedDrawArgs.clear();
}

void footprintExtent(const FakeVertexBuffer& foot, float* minimum, float* maximum)
{
    for (int c = 0; c < 3; ++c)
    {
        minimum[c] = FLT_MAX;
        maximum[c] = -FLT_MAX;
    }
    const float* const p = reinterpret_cast<const float*>(&foot.data[0]);
    for (unsigned int i = 0; i < foot.count; ++i)
    {
        for (int c = 0; c < 3; ++c)
        {
            if (!(minimum[c] < p[3 * i + c]))
                minimum[c] = p[3 * i + c];
            if (maximum[c] < p[3 * i + c])
                maximum[c] = p[3 * i + c];
        }
    }
}

void projectionChecks()
{
    // y = 0.5x - 7: the footprint climbs from -2 to about 3, so the heights cross the clamp's 0.
    const float sloped[4][3] = { { 0.0f, -7.0f, 0.0f }, { 10.0f, -2.0f, 0.0f }, { 20.0f, 3.0f, 0.0f },
                                 { 30.0f, 8.0f, 0.0f } };
    IRoadTemplate* const highway = newHighway(false);
    IEditorSpline* const spline = newSpline(sloped, 4);
    IRoadEditable* const road = newRoad();
    road->setTemplate(highway);
    road->setSpline(spline);

    clearProjectionRecords();
    size_t buffers = g_vertexBuffers.size();
    int pushes = g_pushes;
    int pops = g_pops;
    road->projectToTerrain();
    check(g_clusterQueries.size() == 1 && g_clusterQueries[0] == 820003u,
          "the HeightmapCluster singleton, queried for its editable view (820003)");
    check(g_formatQueries.size() == 1 && g_formatQueries[0] == 115u, "the device is asked for G32R32F (115) targets");
    check(g_vertexBuffers.size() == buffers + 1, "one footprint buffer");
    const FakeVertexBuffer& foot = *g_vertexBuffers.back();
    check(foot.stride == 12 && foot.usage == 8 && foot.name == "RoadEditable" && foot.count == 4,
          "(vertices, 12, \"RoadEditable\", 8): positions of a ghost footprint (two rows here)");
    check(foot.releases == 1 && foot.removes == 1, "released and removed after the draw");
    float minimum[3];
    float maximum[3];
    footprintExtent(foot, minimum, maximum);
    // The offset copy's side vector is the unit 3D tangent x up, not renormalised (EditorSpline, shipped):
    // on this 1 : 2 slope each edge moves half the width x cos(slope) = 6 x 2 / sqrt(5) - to the 1e-4 the
    // tangent's finite difference (t and t + 0.001, float) allows.
    const float slopeHalf = static_cast<float>(6.0 * 2.0 / std::sqrt(5.0));
    check(closeTo(minimum[2], -slopeHalf, 1e-3f) && closeTo(maximum[2], slopeHalf, 1e-3f) && minimum[1] == -2.0f,
          "the footprint is the template width + 2 m wide; its heights are the spline's (a ghost)");
    check(g_surfaceRequests.size() == 2 && g_surfaceRequests[0].width == 4096u && g_surfaceRequests[0].height == 4096u &&
          g_surfaceRequests[0].format == 21u && g_surfaceRequests[0].pool == 2u,
          "the target: MaxTextureHeight (4096) square, A8R8G8B8 (21), system memory");
    check(g_targets.size() == 1 && g_targets[0]->width == 4096u && g_targets[0]->format == 21u &&
          g_targets[0]->depthStencil == 0 && g_targets[0]->depthFormat == 0u,
          "D3DXCreateRenderToSurface(device, size, size, format, FALSE, D3DFMT_UNKNOWN)");
    if (g_targets.size() == 1 && g_surfaces.size() >= 2)
    {
        check(g_targets[0]->begins == 1 && g_targets[0]->beginSurface == g_surfaces[g_surfaces.size() - 2] &&
              !g_targets[0]->beginViewport && g_targets[0]->ends == 1 && g_targets[0]->endFilter == 0,
              "BeginScene(rendering, 0) ... EndScene(0)");
    }
    check(g_deviceCalls.size() == 1 && g_deviceCalls[0] == "clear 0 0 1 FFFF0000 0 0",
          "Clear(0, 0, TARGET, 0xFFFF0000, 0, 0)");
    check(!g_techniques.empty() && g_techniques.back() == "projectroad" && g_passes.size() == 1 && g_passes[0] == 0,
          "technique \"projectroad\", pass 0");
    const float spanX = maximum[0] - minimum[0];
    const float spanZ = maximum[2] - minimum[2];
    const float spanY = maximum[1] - minimum[1];
    const float kConstants[15] = { 0.0f, minimum[0], minimum[2], 0.1f, 2.0f,
                                   1.0f, 2.0f / spanX, 2.0f / spanZ, 0.0f, 1.0f,
                                   2.0f, minimum[1], 1.0f / spanY, 0.0f, 1.0f };
    check(g_shaderConstants.size() == 15 && std::equal(kConstants, kConstants + 15, g_shaderConstants.begin()),
          "on the device: c0 = (minX, minZ, 0.1, 2), c1 = (2 / spanX, 2 / spanZ, 0, 1), c2 = (minY, 1 / spanY, 0, 1)");
    check(g_drawPrimitive.size() == 3 && g_drawPrimitive[0] == 2 && g_drawPrimitive[1] == 5 && g_indexedDrawArgs.empty(),
          "the footprint drawn as a strip (a two-column ghost has no indices)");
    if (g_surfaces.size() >= 2 && !g_targets.empty())
    {
        const FakeSurface& rendering = *g_surfaces[g_surfaces.size() - 2];
        const FakeSurface& heights = *g_surfaces.back();
        const int x0 = static_cast<int>(std::floor(static_cast<double>(minimum[0])));
        const int z0 = static_cast<int>(std::floor(static_cast<double>(minimum[2])));
        const int x1 = static_cast<int>(std::ceil(static_cast<double>(maximum[0])));
        const int z1 = static_cast<int>(std::ceil(static_cast<double>(maximum[2])));
        const int columns = x1 - x0;
        const int rows = z1 - z0;
        check(heights.width == static_cast<unsigned int>(columns) && heights.height == static_cast<unsigned int>(rows) &&
              heights.format == 21u && heights.pool == 2u,
              "the heights surface: (ceil maxX - floor minX) x (ceil maxZ - floor minZ), the target's format");
        check(g_loadSurfaceCalls == 1 && g_loadSurfaceFilter == 2, "the rendering copied in with the point filter");
        check(rendering.refs == 0 && g_targets[0]->refs == 0 && heights.refs == 0,
              "the rendering, the render-to-surface and the heights surface are released");
        check(heights.locks == 1 && heights.unlocks == 1 && heights.lockFlags == 0x10,
              "the heights surface is locked read-only, once");
        std::vector<HeightWrite> expectedWrites;
        int pitchStrideWrites = 0;
        for (int row = 0; row < rows; ++row)
        {
            for (int i = 0; i < columns; ++i)
            {
                const unsigned char* const texel = &heights.bits[static_cast<size_t>(4 * (i + row * columns))];
                if (texel[3] == 0)
                {
                    float height = static_cast<float>(static_cast<int>(texel[1])) * 0.0039215689f * spanY
                                 + minimum[1] - 0.1f;
                    if (height >= 65535.0f)
                        height = 65535.0f;
                    if (!(height >= 0.0f))
                        height = 0.0f;
                    HeightWrite write;
                    write.x = static_cast<float>(x0 + i);
                    write.z = static_cast<float>(z1 - row);
                    write.height = height;
                    expectedWrites.push_back(write);
                }
                if (heights.bits[static_cast<size_t>(row) * heights.pitch + 4u * static_cast<unsigned int>(i) + 3u] == 0)
                    ++pitchStrideWrites;
            }
        }
        bool writesOk = g_heightWrites.size() == expectedWrites.size();
        bool zero = false;
        bool positive = false;
        for (size_t i = 0; writesOk && i < expectedWrites.size(); ++i)
        {
            writesOk = g_heightWrites[i].x == expectedWrites[i].x && g_heightWrites[i].z == expectedWrites[i].z &&
                       g_heightWrites[i].height == expectedWrites[i].height;
            zero = zero || expectedWrites[i].height == 0.0f;
            positive = positive || expectedWrites[i].height > 0.0f;
        }
        check(writesOk && !expectedWrites.empty(),
              "each texel with alpha 0 sets clamp(green / 255 x spanY + minY - 0.1, 0, 65535) at "
              "(x0 + column, z1 - row), rows from ceil maxZ down (IHeightmapClusterEditable +0x34)");
        check(zero && positive, "both sides of the clamp's 0 are met");
        check(static_cast<int>(expectedWrites.size()) != pitchStrideWrites,
              "⚠ the texel address strides by the width, not the locked pitch (shipped)");
        check(g_posted.size() == 1 && g_posted[0].category == 8 && g_posted[0].delay == 0.0f &&
              g_posted[0].type == 100 && g_posted[0].typeField == 100u &&
              g_posted[0].area[0] == static_cast<float>(x0) && g_posted[0].area[1] == static_cast<float>(z0) &&
              g_posted[0].area[2] == static_cast<float>(x1) && g_posted[0].area[3] == static_cast<float>(z1),
              "a HeightmapEventNode(100) for (floor minX, floor minZ, ceil maxX, ceil maxZ), posted under ECEditor");
    }
    check(g_wrongPostOverload == 0, "through the node overload of postEvent (+0x14)");
    check(g_pushes == pushes + 1 && g_pops == pops + 1, "inside one state bracket");
    check(g_savedSurfaces.empty(), "an 8-bit target is not saved");

    // The float target.
    g_floatTargets = true;
    clearProjectionRecords();
    road->projectToTerrain();
    g_floatTargets = false;
    bool floatFormats = !g_surfaceRequests.empty();
    for (size_t i = 0; i < g_surfaceRequests.size(); ++i)
        floatFormats = floatFormats && g_surfaceRequests[i].format == 115u;
    check(floatFormats, "a device that renders G32R32F gets float surfaces");
    check(g_savedSurfaces.size() == 1 && g_savedSurfaces[0] == "c:\\tbmTest.dds|4|ok",
          "⚠ the float rendering is saved to c:\\tbmTest.dds as DDS (census hazard 2; recorded here, never written)");
    check(g_heightWrites.empty() && g_posted.size() == 1,
          "⚠ and no height is written from a float target (shipped) - the event still goes");

    // Size retries.
    clearProjectionRecords();
    g_surfaceFailures = 2;
    road->projectToTerrain();
    check(g_surfaceRequests.size() == 4 && g_surfaceRequests[0].width == 4096u && !g_surfaceRequests[0].ok &&
          g_surfaceRequests[1].width == 2048u && !g_surfaceRequests[1].ok && g_surfaceRequests[2].width == 1024u &&
          g_surfaceRequests[2].ok && g_posted.size() == 1,
          "a failed surface halves the size and tries again");
    clearProjectionRecords();
    g_rtsFailures = 1;
    road->projectToTerrain();
    check(g_surfaceRequests.size() == 3 && g_surfaceRequests[1].width == 2048u && g_targets.size() == 1 &&
          g_surfaces[g_surfaces.size() - 3]->refs == 0 && g_posted.size() == 1,
          "a failed render-to-surface releases the surface and halves");
    clearProjectionRecords();
    g_beginFailures = 1;
    road->projectToTerrain();
    check(g_targets.size() == 2 && g_targets[0]->refs == 0 && g_surfaces[g_surfaces.size() - 3]->refs == 0 &&
          g_surfaceRequests[1].width == 2048u && g_posted.size() == 1,
          "a failed BeginScene releases the surface and the target and halves");
    const int smallCaps = 512;
    std::memcpy(g_capsStorage + 0x5C, &smallCaps, sizeof(smallCaps));
    clearProjectionRecords();
    road->projectToTerrain();
    check(!g_surfaceRequests.empty() && g_surfaceRequests[0].width == 1024u,
          "a MaxTextureHeight below 1024 still starts at 1024");
    const int largeCaps = 4096;
    std::memcpy(g_capsStorage + 0x5C, &largeCaps, sizeof(largeCaps));

    // No size works: the shipped give-up.
    clearProjectionRecords();
    g_surfaceAlwaysFails = true;
    buffers = g_vertexBuffers.size();
    pushes = g_pushes;
    pops = g_pops;
    road->projectToTerrain();
    g_surfaceAlwaysFails = false;
    check(g_surfaceRequests.size() == 6 && g_surfaceRequests[5].width == 128u,
          "no surface from 4096 down to 128: six sizes, then the loop gives up");
    check(g_vertexBuffers.size() == buffers + 1 && g_vertexBuffers.back()->releases == 0 &&
          g_vertexBuffers.back()->removes == 0 && g_pushes == pushes + 1 && g_pops == pops && g_posted.empty(),
          "⚠ ... holding the footprint buffer, with the state bracket left open and no event (shipped)");

    // A noise widens the footprint by its output range.
    IPerlinNoise* const noise = static_cast<IPerlinNoise*>(createPerlinNoise(80035u, 0));
    Vec2 range = { -0.15f, 0.15f };
    noise->setOutputRange(range);
    road->setNoise(noise);
    clearProjectionRecords();
    road->projectToTerrain();
    footprintExtent(*g_vertexBuffers.back(), minimum, maximum);
    const float noiseHalf = static_cast<float>(6.15 * 2.0 / std::sqrt(5.0));
    check(closeTo(minimum[2], -noiseHalf, 1e-3f) && closeTo(maximum[2], noiseHalf, 1e-3f),
          "with a noise the footprint is width + 2 + (range high - low) wide");

    // Fewer than four control points: neither build nor projection does anything.
    const float three[3][3] = { { 0.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f }, { 20.0f, 0.0f, 0.0f } };
    IEditorSpline* const short3 = newSpline(three, 3);
    IRoadEditable* const stub = newRoad();
    stub->setTemplate(highway);
    stub->setSpline(short3);
    buffers = g_vertexBuffers.size();
    clearProjectionRecords();
    stub->projectToTerrain();
    check(concrete(stub)->m_vertexCount == 0 && !concrete(stub)->m_vertexBuffer && g_vertexBuffers.size() == buffers &&
          g_clusterQueries.empty(), "below four control points neither buildGeometry nor projectToTerrain does anything");

    check(stub->release() == 0 && short3->release() == 0 && road->release() == 0 && spline->release() == 0 &&
          highway->release() == 0 && noise->release() == 0, "the objects release");
    check(g_liveMeshes == 0, "every control-point sphere went with its spline");
}

} // anonymous namespace


int main()
{
    installDoubles();
    roadTemplateChecks();
    rendererChecks();
    perlinChecks();
    splineChecks();
    roadEditableObjectChecks();
    straightRoadChecks();
    mirroredRoadChecks();
    curvedRoadChecks();
    projectionChecks();
    std::printf("road objects fixture: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
