// Creates compiled-road geometry instances; mesh data is loaded per instance.


#ifdef BF2_IMAGE_RENDDX9

#include "RoadCompiledTemplate.h"
#include "RoadCompiled.h"
#include "Terrain.h"                     // TerrainRawReader - the raw reader family 0x10075DE0..0x100763A0

#include "../RendDX9/RendDX9Base.h"      // textureManagerGetTexture, resourceManagerIsReady
#include "../RendDX9/Texture.h"          // EngineTexture, textureDtor
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include <string>

#include <new>
#include <stdexcept>

namespace dice {
namespace hfe {
namespace geom {

// GeometryTemplate_RoadCompiled_reflectionTypeId, 0x1021AEC0: 8D 8A 0C 00 = 821901. getClassList and
// getClassId both read it as data.
extern const unsigned int CID_RoadCompiledTemplate = 821901u;

namespace {

// IID_IGeometryTemplate - dword_1021AE5C, 0x17701FFF. GeometryTemplateManager.cpp and MeshTemplate.cpp
// carry the same value as their own file constants; this file does the same rather than coupling.
const unsigned int kIID_IGeometryTemplate = 0x17701FFFu;

// The owner-side texture release both destructor arms open-code (0x1013AFF1 / 0x1013B012):
// `if (t && !--t->refCount) { sub_10014E20(t); sub_1005F360(t); }`. The slot is not cleared - the
// object is dying - and the TextureManager's cache entry is not touched.
void releaseOwnedTexture(dx9rend::EngineTexture* texture)
{
    if (texture && !--texture->refCount)
    {
        dx9rend::textureDtor(texture);
        ::renddx9abi::engineFree(texture);
    }
}

// sub_1007B220's shared `static const Vec3(1, 1, 1)` - decompressPosition's only source. MeshTemplate.cpp
// transcribes the same function-local static for its own table; the value is a constant.
const Vec3& unitScale()
{
    static Vec3 value = { 1.0f, 1.0f, 1.0f };
    return value;
}

} // anonymous namespace

// Initializes template resources and strings. Priority and the byte at +0x04 remain untouched.
RoadCompiledTemplate::RoadCompiledTemplate()
    : m_refCount(1)
    , m_loaded(0)
    , m_texture0(0)
    , m_texture1(0)
    , m_blendFactor(0.0f)
{
    m_dword08    = 0;
    m_fadeOff[0] = 0.0f;
    m_fadeOff[1] = 0.0f;
}

// the two textures released owner-side (texture0 first), then the members in reverse
// order: the path string, the name string and the instance list (0x1000EAD0).
RoadCompiledTemplate::~RoadCompiledTemplate()
{
    releaseOwnedTexture(m_texture0);
    releaseOwnedTexture(m_texture1);
}


// IBase.


// 0x10132630 - `add [ecx+0Ch], 1; mov eax, [ecx+0Ch]`. IBase declares it void; the count in eax is
// the shipped body's and no caller reads it.
void RoadCompiledTemplate::addRef()
{
    ++m_refCount;
}

// the image-wide `mov eax, [ecx+0Ch]` body.
int RoadCompiledTemplate::getRef()
{
    return m_refCount;
}

// the destructor body is called directly (the table has no deleting-destructor slot),
// then the block goes back to the engine heap.
int RoadCompiledTemplate::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        this->~RoadCompiledTemplate();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// IBase and IID_IGeometryTemplate answer with the object; everything else with null.
void* RoadCompiledTemplate::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == kIID_IGeometryTemplate)
        return const_cast<RoadCompiledTemplate*>(this);
    return 0;
}


// IGeometryTemplate - the slots this class owns.


unsigned int RoadCompiledTemplate::getClassId() const { return CID_RoadCompiledTemplate; }
void RoadCompiledTemplate::setName(const std::string& name) { m_name = name; }
const std::string& RoadCompiledTemplate::getName() const { return m_name; }
void RoadCompiledTemplate::setPath(const std::string& path) { m_path = path; }
const std::string& RoadCompiledTemplate::getPath() const { return m_path; }
bool RoadCompiledTemplate::isLoaded() const { return m_loaded != 0; }

// createGeometry. A new RoadCompiled (retail 0x194 bytes) joins the END of the instance list (the
// list holds the null too if the allocation failed); a template no one has asked for yet loads its
// `_compiled.dat` right here unless the ResourceManager is deferring loads (its +0x04 byte, sub_1000D950)
// - then GeometryTemplateManager::loadUsedTemplates loads it with the others. Either way the keep byte
// goes up, and the caller gets the IGeometry subobject (instance + 4), or null.
// The load is a virtual call (`call [eax+2Ch]`), and it is made on the keep byte, NOT on +0x10: a
//   template the manager already marked is never loaded here, loaded or not.
void* RoadCompiledTemplate::createInstance()
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(RoadCompiled));
    RoadCompiled* const instance = storage ? new (storage) RoadCompiled(this) : 0;
    m_instances.push_back(instance);

    if (!m_keepData && !dx9rend::resourceManagerIsReady())
        load();                                                              // +0x2C
    m_keepData = 1;

    return roadCompiledGeometry(instance);
}

// loadResources. `<path><name>_compiled.dat` through the raw reader: two texture lines
// (TextureManager::getTexture, the line passed by value), the blend factor, the fade-off pair and the
// priority. The shipped body tests neither the open nor a single read.
// The file name is built as `(path + name) + "_compiled.dat"` - VC7.1's operator+ inlined twice
//   (0x1013AE49..0x1013AEAB); only the finished string is observable.
bool RoadCompiledTemplate::load()
{
    if (m_loaded)
        return false;

    std::string file(m_path);
    file += m_name;
    file += "_compiled.dat";

    TerrainRawReader reader;
    reader.open(file, 2, 0, 0);

    std::string line;
    reader.readLine(line, '\n');
    m_texture0 = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerGetTexture(line));
    reader.readLine(line, '\n');
    m_texture1 = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerGetTexture(line));
    reader.readU32(&m_blendFactor);                                          // sub_10075FA0 (f32)
    reader.readVec2(m_fadeOff);
    reader.readU32(&m_priority);
    reader.close();

    m_loaded = 1;
    return true;
}


// Default geometry-template operations for unsupported road-template features.

void RoadCompiledTemplate::setLodDistance(int, int, float) { }
float RoadCompiledTemplate::getLodDistance(int, int) { return -1.0f; }
void RoadCompiledTemplate::setMaterialValue(int, int, unsigned char, float) { }
float RoadCompiledTemplate::getMaterialValue(int, int, unsigned char) { return 15.0f; }
void RoadCompiledTemplate::slot_0x40(int, int, int, int) { }
float RoadCompiledTemplate::slot_0x44(int, int, int) { return 0.25f; }
void RoadCompiledTemplate::slot_0x48(int) { }
bool RoadCompiledTemplate::slot_0x4C() { return false; }
bool RoadCompiledTemplate::isWater() const { return false; }
unsigned int RoadCompiledTemplate::getMemoryUsage(int) { return 0; }
int RoadCompiledTemplate::getPrimitiveCount(int, int, int) { return 0; }
int RoadCompiledTemplate::getVertexCount(int, int, int) { return 0; }
void RoadCompiledTemplate::setDoNotGenerateLightmaps(bool) { }
bool RoadCompiledTemplate::getDoNotGenerateLightmaps() const { return false; }
void RoadCompiledTemplate::setLightmapOverride(bool) { }
bool RoadCompiledTemplate::getLightmapOverride() const { return false; }
void RoadCompiledTemplate::setLightmapSize(int, int, int) { }
int RoadCompiledTemplate::getLightmapSize(int, int) { return 0; }
void RoadCompiledTemplate::setCompressVertexData(bool) { }
bool RoadCompiledTemplate::getCompressVertexData() const { return true; }
void RoadCompiledTemplate::slot_0x80(int) { }
bool RoadCompiledTemplate::slot_0x84() { return false; }
void RoadCompiledTemplate::setMaxTextureRepeat(int) { }
int RoadCompiledTemplate::getMaxTextureRepeat() const { return 16; }
void RoadCompiledTemplate::slot_0x90(int) { }
bool RoadCompiledTemplate::slot_0x94() { return false; }
void RoadCompiledTemplate::slot_0x98(int) { }
bool RoadCompiledTemplate::slot_0x9C() { return false; }
int RoadCompiledTemplate::getMaterialCount(unsigned int) { return 0; }
int RoadCompiledTemplate::getLodPrimitiveCount(unsigned int) { return 0; }

// the shared unit vector into the caller's buffer (y, x, z - the shipped store order);
// the input is never read.
Vec3* RoadCompiledTemplate::decompressPosition(Vec3* out, const Vec3*)
{
    const Vec3& scale = unitScale();
    out->y = scale.y;
    out->x = scale.x;
    out->z = scale.z;
    return out;
}

// +0xAC is nullsub_15 (0x10037D40, `ret`) for this class - MeshTemplate's frame stamp is not here.
void RoadCompiledTemplate::releaseDeviceResources() { }


// RefFactory 0x100B09DC - `if (outer) return 0`, allocate the object (retail 0x70 bytes), construct,
// queryInterface(iid) through the NEW object without testing the allocation (a failed allocation
// faults on the QI, as shipped), and release it when the interface is refused.

IBase* __fastcall createRoadCompiledTemplate(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(RoadCompiledTemplate));
    RoadCompiledTemplate* self = 0;
    if (storage)
        self = new (storage) RoadCompiledTemplate();

    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));    // [eax+0Ch]
    if (!result)
        self->release();                                                     // [eax+8]
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
