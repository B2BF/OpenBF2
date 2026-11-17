// Decal appearance and texture-atlas coordinates. Texel getters truncate to signed integers.

#ifdef BF2_IMAGE_RENDDX9

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#endif
#include "DecalTemplate.h"
#include "../../Core/StringUtil.h"           // equalsIgnoreCase (sub_10075080)
#include "DecalManager.h"
#include "DecalObject.h"                                  // decal_template_state (below)

#include "../../RendDX9/RendDX9Base.h"                // textureManagerGetTexture
#include "../../RendDX9/Texture.h"                    // EngineTexture, textureDims, textureReleaseRef
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"  // TextureAtlasEntry
#include "../../GeomMisc/Debug.h"
#include "../../../RendDX9Abi/Frontier.h"             // logf
#include "../../../RendDX9Abi/HostFileAbi.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/PluginEntry.h"          // fileManager

#include <string.h>

namespace dice {
namespace hfe {
namespace geom {

// StaticMesh.cpp (published in StaticMesh.h) - sub_1008DD80: pack a Vec3 and a w into a static
// Vec4 and answer its address. Declared per TU, as MeshParticleMesh.cpp does.
const float* staticMeshPackVec4(const float v[3], float w);
// Terrain.cpp (published in Terrain.h) - sub_100E11B0: copy three floats into a guarded file static
// and answer its address.
const Vec3* terrainOriginScratch(const Vec3& in);

namespace {

// std::operator!=(const string&, const string&) (0x101AF9D0, MSVCP71's): the counted compare.
bool stringsDiffer(const DecalTemplateString& a, const DecalTemplateString& b)
{
    return a != b;
}

// `name + ".dds"` - the shipped code inlines MSVCP71's operator+: copy-construct, operator+=, copy
// out. The result is the host's string type because it goes straight to FileManager::fileExists.
::renddx9abi::HostString withDdsExtension(const DecalTemplateString& name)
{
    ::renddx9abi::HostString result(name);
    result += ".dds";
    return result;
}

// The SmartPtr<Texture> assignment (sub_1014DFD0 and its inlined twins in reset and the
// destructor): release the old texture - `if (!--ref) { sub_10014E20; sub_1005F360 }` - null the
// slot, store the new pointer without taking a reference.
void assignTexture(dx9rend::EngineTexture*& slot, dx9rend::EngineTexture* value)
{
    dx9rend::textureReleaseRef(slot);                      // null-tested, nulls the slot
    slot = value;
}

float unsignedToFloat(unsigned int value)
{
    return static_cast<float>(value);                      // `fild` + the 2^32 correction
}

} // anonymous namespace


// the constructor. The stores, in order: the vptr; +0x08..+0x14 (the
// colour) = 0; +0x2C..+0x38 = 0; the texture name default-constructed; +0x58 (the word index) = 0;
// the name copy-constructed from the argument; +0x78 / +0x7C = 0; +0xA0 / +0xA4 = 0; then reset().
// Everything else (+0x04, +0x18..+0x28, +0x80..+0x9C) is left to reset(); nothing is written
// twice here that the shipped constructor writes once.

// PE 0x101AFA70
DecalTemplate::DecalTemplate(const DecalTemplateString& name)
    : m_decalTextureName()
    , m_index(0)
    , m_name(name)
    , m_texture(0)
    , m_atlasEntry(0)
{
    m_color.x = 0.0f;
    m_color.y = 0.0f;
    m_color.z = 0.0f;
    m_color.w = 0.0f;
    m_vec2C.x = 0.0f;
    m_vec2C.y = 0.0f;
    m_uvOffset.x = 0.0f;
    m_uvOffset.y = 0.0f;
    m_uvExtent.x = 0.0f;
    m_uvExtent.y = 0.0f;
    reset();
}


// the destructor body, and +0xA0 (0x101AFBC0) is its scalar-deleting wrapper
// (`call 0x101AF7B0; if (flags & 1) sub_1005F360(this)`), which this compiler generates from the
// virtual destructor. The texture is released twice over: the body's `m_texture = 0` (the SmartPtr
// assignment - release, null), then the SmartPtr member's destructor, which tests the slot again and
// finds it null. Then ~name (+0x5C) and ~decalTextureName (+0x3C), reverse declaration order.
// The atlas entry (+0x7C) is borrowed from the manager's atlas and not released.

// PE 0x101AF7B0
DecalTemplate::~DecalTemplate()
{
    assignTexture(m_texture, 0);
    dx9rend::textureReleaseRef(m_texture);                  // the member's
                                                            // destructor: the slot is already null
}


// Restores defaults through the virtual setters while preserving the current atlas entry.

// PE 0x101AF6C0
void DecalTemplate::reset()
{
    assignTexture(m_texture, 0);
    m_texture = 0;                                          // the second store
    m_vec2C.x = 0.0f;                                       // +0x2C
    m_vec2C.y = 0.0f;                                       // +0x30
    m_randomSize = 0.0f;                                    // +0x1C
    m_randomRotation = 0.0f;                                // +0x24
    m_rotation = 0.0f;                                      // +0x28
    m_randomColorIntensity = 0.0f;                          // +0x04

    Vec3 white;
    white.x = 1.0f;
    white.y = 1.0f;
    white.z = 1.0f;
    setColor(white);                                        // vtable +0x20

    m_decalTextureName = "";                                // operator=(const char*) with unk_101DC333

    Vec2 unit;
    unit.x = 1.0f;
    unit.y = 1.0f;
    setUvExtent(unit);                                        // vtable +0x38
    setSize(1.0f);                                          // vtable +0x08
    setRandomSize(0.0f);                                    // vtable +0x10 (`push ebx`, ebx = 0)
    setIsStatic(false);                                     // vtable +0x28
    setUsedOnTransparentMaterials(false);                   // vtable +0x30

    m_animationRandomizedStartFrame = false;                // +0x80
    m_animationEnable = false;                              // +0x88
    m_animationFrameWidthRelative = 0.0f;                   // +0x8C
    m_animationFrameHeightRelative = 0.0f;                  // +0x90
    m_animationFrameCount = 0;                              // +0x94
    m_animationFrameCountX = 0;                             // +0x98
    m_frameCountXReciprocal = 0.0f;                         // +0x9C
    m_animationSpeed = 1.0f;                                // +0x84
}


// Updates UV offset and extent from the atlas entry and optional animation-frame scale.
// Uses virtual setUvExtent; absent atlas data spans the full texture.

// PE 0x101AF4F0
void DecalTemplate::updateUv()
{
    Vec2 extent;
    if (m_atlasEntry)
    {
        m_uvOffset.x = m_atlasEntry->uOffset;               // entry +0x08
        m_uvOffset.y = m_atlasEntry->vOffset;               // entry +0x0C
        if (m_animationEnable)
        {
            extent.x = m_animationFrameWidthRelative * m_atlasEntry->width;
            extent.y = m_atlasEntry->height * m_animationFrameHeightRelative;
        }
        else
        {
            extent.x = m_atlasEntry->width;
            extent.y = m_atlasEntry->height;
        }
    }
    else
    {
        m_uvOffset.x = 0.0f;
        m_uvOffset.y = 0.0f;
        if (m_animationEnable)
        {
            extent.x = m_animationFrameWidthRelative;
            extent.y = m_animationFrameHeightRelative;
        }
        else
        {
            extent.x = 1.0f;                                // flt_101DDDE4
            extent.y = 1.0f;
        }
    }
    setUvExtent(extent);                                      // vtable +0x38
}


// Resolves an atlas entry first, then a standalone DDS texture. Returns true when either
// resolves. Empty names leave state unchanged; other misses clear prior bindings and log.

// PE 0x101AF820
bool DecalTemplate::loadTexture(const DecalTemplateString& name)
{
    if (equalsIgnoreCase(std::string(name.c_str()), std::string("")))     // sub_10075080
        return false;

    assignTexture(m_texture, 0);

    DecalManager* const manager = static_cast<DecalManager*>(g_DecalManager);
    m_atlasEntry = manager->getAtlasEntry(::renddx9abi::fromHostString(name));
    if (m_atlasEntry)
        return true;

    // FileManager +0x1C fileExists - typed in both flavours (HostFileAbi.h; RW-266's TextureManager
    // row). `std::string(name) += ".dds"`, copy-constructed, passed by reference.
    const ::renddx9abi::HostString file = withDdsExtension(name);
    const bool exists = ::dice::hfe::fileManager->fileExists(file);          // dword_10244AC4 +0x1C
    if (exists)
    {
        assignTexture(m_texture, static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerGetTexture(::renddx9abi::fromHostString(name))));
        return m_texture != 0;
    }

    ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Geom\\DecalSystem\\DecalTemplate.cpp",
        107, "") << "File " << name << " not found!";
    return m_texture != 0;
}

// see the header: unreferenced in the shipped image.
void DecalTemplate::reloadTexture()
{
    loadTexture(m_decalTextureName);                       // sub_101AF820(&this->+0x3C)
    refresh();                                             // `jmp dword ptr [edx+4]`
}


// The table, slot by slot.


// +0x00, 0x101AF3C0 - `lea eax, [ecx+5Ch]`.
const DecalTemplateString& DecalTemplate::getName() const
{
    return m_name;
}

// +0x04, (85 B, retn) - 1 / animationFrameCountX when that count, as a float, compares
// greater than 0.0 (`fcom 0.0; test ah, 41h; jnz` - less, equal or unordered take the zero arm),
// else 0; then updateUv(). Every world::DecalTemplate setter in BF2.exe calls it after its set.
// PE 0x101AFA10
void DecalTemplate::refresh()
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] The unsigned count stays live in x87 until the reciprocal store.
    const bf2x87::Real count=bf2x87::wide(m_animationFrameCountX);
#else
    const double count=static_cast<double>(m_animationFrameCountX);
#endif
    if (count > 0.0f)
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        m_frameCountXReciprocal=static_cast<float>(bf2x87::wide(1)/count);
#else
        m_frameCountXReciprocal=static_cast<float>(1.0/count);
#endif
    else
        m_frameCountXReciprocal = 0.0f;
    updateUv();
}

void  DecalTemplate::setSize(float size)                { m_size = size; }              // +0x08
float DecalTemplate::getSize() const                    { return m_size; }              // +0x0C
void  DecalTemplate::setRandomSize(float randomSize)    { m_randomSize = randomSize; }  // +0x10
float DecalTemplate::getRandomSize() const              { return m_randomSize; }        // +0x14

// +0x18 / +0x1C, the +0x04 store/load, comdat-folded with other classes'
// identical bodies (IDA names the store RenderPathListener_storeRenderPathAt0x4).
void  DecalTemplate::setRandomColorIntensity(float intensity) { m_randomColorIntensity = intensity; }
float DecalTemplate::getRandomColorIntensity() const    { return m_randomColorIntensity; }

// PE 0x101AF490
// +0x20, the colour packed with w = 1.0 through sub_1008DD80's shared
// static Vec4, then copied out of it: +0x08 = x, +0x0C = y, +0x10 = z, +0x14 = 1.0.
void DecalTemplate::setColor(const Vec3& color)
{
    const float* const packed = staticMeshPackVec4(&color.x, 1.0f);
    m_color.x = packed[0];
    m_color.y = packed[1];
    m_color.z = packed[2];
    m_color.w = packed[3];
}

// PE 0x101AF4C0
// +0x24, the colour's xyz copied through
// sub_100E11B0's shared file static, then out of it into the return slot.
Vec3 DecalTemplate::getColor() const
{
    Vec3 xyz;
    xyz.x = m_color.x;
    xyz.y = m_color.y;
    xyz.z = m_color.z;
    return *terrainOriginScratch(xyz);                                     // sub_100E11B0(this+8)
}

// PE 0x101AFB10
void DecalTemplate::setIsStatic(bool isStatic)          { m_isStatic = isStatic; }      // +0x28
bool DecalTemplate::getIsStatic() const                 { return m_isStatic; }          // +0x2C
// PE 0x101AFB30
void DecalTemplate::setUsedOnTransparentMaterials(bool used)                             // +0x30
{
    m_usedOnTransparentMaterials = used;
}
bool DecalTemplate::getUsedOnTransparentMaterials() const                                // +0x34
{
    return m_usedOnTransparentMaterials;
}

// +0x38 / +0x3C, the Vec2 at +0xA0 (y through the FPU, x as a dword).
// PE 0x101AF450
void DecalTemplate::setUvExtent(const Vec2& value)
{
    m_uvExtent.y = value.y;
    m_uvExtent.x = value.x;
}

// PE 0x101AF470
Vec2 DecalTemplate::getUvExtent() const
{
    return m_uvExtent;
}

void  DecalTemplate::setRandomRotation(float randomRotation) { m_randomRotation = randomRotation; } // +0x40
float DecalTemplate::getRandomRotation() const          { return m_randomRotation; }    // +0x44
// PE 0x101AFB60
void  DecalTemplate::setRotation(float rotation)        { m_rotation = rotation; }      // +0x48
float DecalTemplate::getRotation() const                { return m_rotation; }          // +0x4C

// +0x50, only a different name (MSVCP71 operator!=) is stored and
// loaded; a failed load puts "" back. The load is handed the argument, not the member (`push ebx`,
// ebx = the argument, 0x101AF9E6). The refresh (+0x04, virtual) runs either way.
// PE 0x101AF9C0
void DecalTemplate::setDecalTextureName(const DecalTemplateString& name)
{
    if (stringsDiffer(m_decalTextureName, name))
    {
        m_decalTextureName = name;
        if (!loadTexture(name))
            m_decalTextureName = "";
    }
    refresh();                                                            // `call dword ptr [eax+4]`
}

// +0x54, 0x101AF3D0 - `lea eax, [ecx+3Ch]`.
const DecalTemplateString& DecalTemplate::getDecalTextureName() const
{
    return m_decalTextureName;
}

// PE 0x101AFBA0
void DecalTemplate::setAnimationEnable(bool enable)     { m_animationEnable = enable; } // +0x58
bool DecalTemplate::getAnimationEnable() const          { return m_animationEnable; }   // +0x5C
// PE 0x101AFB80
void DecalTemplate::setAnimationRandomizedStartFrame(bool randomized)                    // +0x60
{
    m_animationRandomizedStartFrame = randomized;
}
bool DecalTemplate::getAnimationRandomizedStartFrame() const                             // +0x64
{
    return m_animationRandomizedStartFrame;
}
void  DecalTemplate::setAnimationSpeed(float speed)     { m_animationSpeed = speed; }   // +0x68
float DecalTemplate::getAnimationSpeed() const          { return m_animationSpeed; }    // +0x6C

// +0x70, with a texture bound: frameWidthRel = width / texture width
// (both unsigned, converted exactly). Without one: nothing.
// PE 0x101AF570
void DecalTemplate::setAnimationFrameWidth(unsigned int width)
{
    if (m_texture)
    {
        const unsigned int* const dims = dx9rend::textureDims(m_texture);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] Neither unsigned conversion spills before division.
        m_animationFrameWidthRelative=static_cast<float>(bf2x87::wide(width)/bf2x87::wide(dims[0]));
#else
        m_animationFrameWidthRelative=static_cast<float>(static_cast<double>(width)/static_cast<double>(dims[0]));
#endif
    }
}

// +0x74, (82 B, retn) - 0 without a texture; else (int)(texture width * frameWidthRel),
// truncated (the inline control-word switch to round-toward-zero around `fistp dword`).
// PE 0x101AF5B0
unsigned int DecalTemplate::getAnimationFrameWidth() const
{
    if (!m_texture)
        return 0;
    const unsigned int* const dims = dx9rend::textureDims(m_texture);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return static_cast<unsigned int>(bf2x87::toIntTruncate(bf2x87::wide(dims[0]) *
                                                      bf2x87::wide(m_animationFrameWidthRelative)));
#else
    return static_cast<unsigned int>(static_cast<int>(static_cast<double>(dims[0]) *
                                                      m_animationFrameWidthRelative));
#endif
}

// +0x78, the height twin; the argument is narrowed to a float in its own
// stack slot (`fstp [ebp+arg_0]`) before the division, which is what `(float)height` is here.
// PE 0x101AF610
void DecalTemplate::setAnimationFrameHeight(unsigned int height)
{
    if (m_texture)
    {
        const float h = unsignedToFloat(height);
        const unsigned int* const dims = dx9rend::textureDims(m_texture);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] Only the numerator spills to float in this setter.
        m_animationFrameHeightRelative=static_cast<float>(bf2x87::wide(h)/bf2x87::wide(dims[1]));
#else
        m_animationFrameHeightRelative=static_cast<float>(static_cast<double>(h)/static_cast<double>(dims[1]));
#endif
    }
}

// +0x7C, (83 B, retn).
// PE 0x101AF660
unsigned int DecalTemplate::getAnimationFrameHeight() const
{
    if (!m_texture)
        return 0;
    const unsigned int* const dims = dx9rend::textureDims(m_texture);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return static_cast<unsigned int>(bf2x87::toIntTruncate(bf2x87::wide(dims[1]) *
                                                      bf2x87::wide(m_animationFrameHeightRelative)));
#else
    return static_cast<unsigned int>(static_cast<int>(static_cast<double>(dims[1]) *
                                                      m_animationFrameHeightRelative));
#endif
}

void  DecalTemplate::setAnimationFrameWidthRelative(float width)    { m_animationFrameWidthRelative = width; }   // +0x80
float DecalTemplate::getAnimationFrameWidthRelative() const         { return m_animationFrameWidthRelative; }   // +0x84
void  DecalTemplate::setAnimationFrameHeightRelative(float height)  { m_animationFrameHeightRelative = height; } // +0x88
float DecalTemplate::getAnimationFrameHeightRelative() const        { return m_animationFrameHeightRelative; }  // +0x8C
void  DecalTemplate::setAnimationFrameCount(unsigned int count)     { m_animationFrameCount = count; }          // +0x90
unsigned int DecalTemplate::getAnimationFrameCount() const          { return m_animationFrameCount; }           // +0x94

// +0x98, store the count; the reciprocal is 1 / count for a non-zero
// count (`test eax, eax; jbe`), else 0. No refresh here - the BF2.exe setter calls +0x04 itself.
// PE 0x101AF3E0
void DecalTemplate::setAnimationFrameCountX(unsigned int count)
{
    m_animationFrameCountX = count;
    if (count != 0)
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] FILD's unsigned correction stays in x87 until the reciprocal store.
        m_frameCountXReciprocal = bf2x87::store(bf2x87::wide(1.0f) / bf2x87::wide(count));
#else
        m_frameCountXReciprocal = 1.0f / unsignedToFloat(count);          // fdivr 1.0
#endif
    else
        m_frameCountXReciprocal = 0.0f;
}

// Stores the argument dword at +0x9C (the 1/frameCountX cache); this setter has no caller.
// unreachable in 1.50: no code or data references
// PE 0x101AF430
void DecalTemplate::setFrameCountXReciprocal(float value)
{
    m_frameCountXReciprocal = value;
}

// +0x9C, 0x101AF420 - `mov eax, [ecx+98h]`.
unsigned int DecalTemplate::getAnimationFrameCountX() const
{
    return m_animationFrameCountX;
}


// Typed decal-template accessors account for native layout changes after string fields.

namespace decal_template_state
{
namespace {
const DecalTemplate* impl(const IDecalTemplate* decalTemplate)
{
    return static_cast<const DecalTemplate*>(decalTemplate);
}
}

const Vec4&    color(const IDecalTemplate* t)                   { return impl(t)->colorVec4(); }                // +0x08
bool           isStatic(const IDecalTemplate* t)                { return impl(t)->isStaticField(); }            // +0x20
const Vec2&    uvOffset(const IDecalTemplate* t)                { return impl(t)->uvOffset(); }                 // +0x34
unsigned short index(const IDecalTemplate* t)                   { return impl(t)->getIndex(); }                 // +0x58
// `+0x7C ? [+0x7C]+0x10 : +0x78` (0x101AE5AF..0x101AE5C3): the atlas page when DecalAtlas.tai holds the
// template's texture, else its own texture.
dx9rend::EngineTexture* renderTexture(const IDecalTemplate* t)
{
    const dx9rend::TextureAtlasEntry* const entry = impl(t)->getAtlasEntry();
    return entry ? entry->page : impl(t)->getTexture();
}
bool           animationRandomizedStartFrame(const IDecalTemplate* t) { return impl(t)->randomizedStartFrameField(); } // +0x80
bool           animationEnable(const IDecalTemplate* t)         { return impl(t)->animationEnableField(); }     // +0x88
unsigned int   animationFrameCount(const IDecalTemplate* t)     { return impl(t)->frameCountField(); }          // +0x94
unsigned int   animationFrameCountX(const IDecalTemplate* t)    { return impl(t)->frameCountXField(); }         // +0x98
float          frameCountXReciprocal(const IDecalTemplate* t)   { return impl(t)->frameCountXReciprocal(); }    // +0x9C
const Vec2&    uvExtent(const IDecalTemplate* t)                { return impl(t)->uvExtent(); }                 // +0xA0
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
