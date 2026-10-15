
// TerrainEditable accessors, path construction, and shader technique selection.
// [INFERRED] source placement. These methods are shared with the game image.

#ifdef BF2_IMAGE_RENDDX9

#include "TerrainEditable.h"
#include "QuadPatchEditable.h"
#include "UndergrowthSystem.h"                // g_UndergrowthSystem, slot +0x54
#include "Overgrowth/OverGrowth.h"            // the whole-map overlay exports 0x100C8F40 / 0x100C9250

#include "../RendDX9/RendDX9Base.h"           // readStringSettingDefault, textureManagerGetTexture
#include "../../RendDX9Abi/PluginAbi.h"
#include <string>

#include <cstring>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_OverGrowth;          // dword_10258F38 - BF2Render.cpp, the level OverGrowth
} // namespace dx9rend

namespace geom {

namespace {

// dword_1021AEB0 - dice.hfe.geom.TerrainEditable, read as data by the reflection getter.
const unsigned int kTerrainEditableClassId = 821005u;

const unsigned int kNpos = 0xFFFFFFFFu;


// [CHOSEN, not recovered] VC7.1-compatible string operations absent from std::string.

// basic_string::rfind(const char*, size_type off, size_type count).
unsigned int stringRFind(const char* s, unsigned int size, const char* pattern,
                         unsigned int offset, unsigned int count)
{
    if (count == 0)
        return offset < size ? offset : size;
    if (count <= size)
    {
        const char* p = s + (offset < size - count ? offset : size - count);
        for (;; --p)
        {
            if (*p == *pattern && std::memcmp(p, pattern, count) == 0)
                return static_cast<unsigned int>(p - s);
            if (p == s)
                break;
        }
    }
    return kNpos;
}

// basic_string::find(const char*, size_type off, size_type count).
unsigned int stringFind(const char* s, unsigned int size, const char* pattern,
                        unsigned int offset, unsigned int count)
{
    if (count == 0 && offset <= size)
        return offset;
    if (offset < size && count <= size - offset)
    {
        for (unsigned int at = offset; at + count <= size; ++at)
            if (std::memcmp(s + at, pattern, count) == 0)
                return at;
    }
    return kNpos;
}

// basic_string::substr(0, count).
template <typename S>
std::string stringPrefix(const S& s, unsigned int count)
{
    const unsigned int size = static_cast<unsigned int>(s.size());
    return std::string(s.c_str(), count < size ? count : size);
}

// The base-name -> directory step the directory getters share: everything before the last "tx" (the
// 2-character string at 0x10221368); the whole name when there is none (substr(0, npos)).
template <typename S>
unsigned int lastTx(const S& s)
{
    return stringRFind(s.c_str(), static_cast<unsigned int>(s.size()), "tx", kNpos, 2);
}

// The editor-folder step the four editor base-name getters share: "<before>/Editor<folder...>" when
// `folder` occurs, else the name unchanged (find(folder, 0, length), then insert(at, "/Editor")).
std::string withEditorFolder(const std::string& name, const char* folder, unsigned int folderLength)
{
    const unsigned int at = stringFind(name.c_str(), name.size(), folder, 0, folderLength);
    if (at == kNpos)
        return name;
    std::string out(name.c_str(), at);
    out += "/Editor";
    out += std::string(name.c_str() + at, name.size() - at);
    return out;
}

} // anonymous namespace

// (primary slot +0x10).
unsigned int TerrainEditable::getReflectionTypeId()
{
    return kTerrainEditableClassId;
}

// (E+0x30) - the scale, then every patch's bounds (0x101084B0).
void TerrainEditable::setPrimaryWorldScale(const Vec3& scale)
{
    m_primaryWorldScale = scale;
    boundsAfterPrimaryScale();
}
const Vec3& TerrainEditable::getPrimaryWorldScale() const { return m_primaryWorldScale; }
void TerrainEditable::setSecondaryWorldScale(const Vec3& scale) { m_secondaryWorldScale = scale; }
const Vec3& TerrainEditable::getSecondaryWorldScale() const { return m_secondaryWorldScale; }

// Terrain +0x2C, which Terrain's loader reads as a raw word: the setter
// copies the argument's four bytes, the getter loads them as a float (`fld`).
void TerrainEditable::setUnresolved2C(float value)
{
    std::memcpy(&m_fileWord2C, &value, sizeof(value));
}
float TerrainEditable::getUnresolved2C() const
{
    float value;
    std::memcpy(&value, &m_fileWord2C, sizeof(value));
    return value;
}

void TerrainEditable::setPatchSize(int size) { m_patchSize = size; }
int  TerrainEditable::getPatchSize() const { return m_patchSize; }
void TerrainEditable::setSubdividePatches(bool subdivide) { m_subdividePatches = subdivide ? 1 : 0; }
bool TerrainEditable::getSubdividePatches() const { return m_subdividePatches != 0; }
void TerrainEditable::setPatchColormapSize(int size) { m_patchColormapSize = size; }
int  TerrainEditable::getPatchColormapSize() const { return m_patchColormapSize; }

// The four base names (Terrain +0x54 / +0x70 / +0x8C / +0xA8): each setter is `jmp string::operator=`,
// each getter `lea eax, [ecx - off]` - a reference to the member (the native flavour answers a copy;
// ITerrainEditable.h records why).
void TerrainEditable::setColormapBaseName(const TerrainEditableString& name)
{
    m_colormapBaseName = ::renddx9abi::fromHostString(name);
}
TerrainEditableNameResult TerrainEditable::getColormapBaseName() const
{
    return ::renddx9abi::toHostString(m_colormapBaseName);
}
void TerrainEditable::setLightmapBaseName(const TerrainEditableString& name)
{
    m_lightmapBaseName = ::renddx9abi::fromHostString(name);
}
TerrainEditableNameResult TerrainEditable::getLightmapBaseName() const
{
    return ::renddx9abi::toHostString(m_lightmapBaseName);
}
void TerrainEditable::setDetailmapBaseName(const TerrainEditableString& name)
{
    m_detailmapBaseName = ::renddx9abi::fromHostString(name);
}
TerrainEditableNameResult TerrainEditable::getDetailmapBaseName() const
{
    return ::renddx9abi::toHostString(m_detailmapBaseName);
}
void TerrainEditable::setLowDetailmapBaseName(const TerrainEditableString& name)
{
    m_lowDetailmapBaseName = ::renddx9abi::fromHostString(name);
}
TerrainEditableNameResult TerrainEditable::getLowDetailmapBaseName() const
{
    return ::renddx9abi::toHostString(m_lowDetailmapBaseName);
}


// The derived names. Each "directory" getter asks for its base name twice - once for the position of
// the last "tx", once for the substring - exactly as shipped; the editor base names copy the base
// name and insert "/Editor" before its folder.

std::string TerrainEditable::getColormapDirectory()                                                // P+0xEC 0x101A6080
{
    const unsigned int position = lastTx(getColormapBaseName());                                     // E+0x6C
    return stringPrefix(getColormapBaseName(), position);                                            // E+0x6C again
}

std::string TerrainEditable::getDetailmapDirectory()                                               // P+0xF0 0x101A61F0
{
    const unsigned int position = lastTx(getDetailmapBaseName());                                    // E+0x7C
    return stringPrefix(getDetailmapBaseName(), position);
}

// (non-virtual; R3's map writers call it).
std::string TerrainEditable::getLowDetailmapDirectory()
{
    const unsigned int position = lastTx(getLowDetailmapBaseName());                                 // E+0x84
    return stringPrefix(getLowDetailmapBaseName(), position);
}

std::string TerrainEditable::getEditorDetailmapBaseName()                                          // P+0xF4 0x101A63A0
{
    const std::string name(::renddx9abi::fromHostString(getDetailmapBaseName()));                     // E+0x7C
    return withEditorFolder(name, "/Detailmaps", 11);
}

std::string TerrainEditable::getEditorDetailmapDirectory()                                         // P+0xF8 0x101A6410
{
    unsigned int position;
    {
        const std::string first = getEditorDetailmapBaseName();                                     // P+0xF4, a temporary
        position = lastTx(first);
    }
    const std::string second = getEditorDetailmapBaseName();                                        // P+0xF4 again
    return stringPrefix(second, position);
}

std::string TerrainEditable::getEditorColormapBaseName()                                           // P+0xFC 0x101A6480
{
    const std::string name(::renddx9abi::fromHostString(getColormapBaseName()));                      // E+0x6C
    return withEditorFolder(name, "/Colormaps", 10);
}

std::string TerrainEditable::getEditorColormapDirectory()                                          // P+0x100 0x101A64F0
{
    unsigned int position;
    {
        const std::string first = getEditorColormapBaseName();                                      // P+0xFC
        position = lastTx(first);
    }
    const std::string second = getEditorColormapBaseName();
    return stringPrefix(second, position);
}

std::string TerrainEditable::getEditorLightmapBaseName()                                           // P+0x104 0x101A6160
{
    const std::string name(::renddx9abi::fromHostString(getLightmapBaseName()));                      // E+0x74
    return withEditorFolder(name, "/Lightmap", 9);
}

std::string TerrainEditable::getEditorLightmapDirectory()                                          // P+0x108 0x101A60F0
{
    unsigned int position;
    {
        const std::string first = getEditorLightmapBaseName();                                      // P+0x104
        position = lastTx(first);
    }
    const std::string second = getEditorLightmapBaseName();
    return stringPrefix(second, position);
}

std::string TerrainEditable::getEditorLowDetailmapBaseName()                                       // P+0x10C 0x101A62C0
{
    const std::string name(::renddx9abi::fromHostString(getLowDetailmapBaseName()));                  // E+0x84
    return withEditorFolder(name, "/LowDetailmaps", 14);
}

std::string TerrainEditable::getEditorLowDetailmapDirectory()                                      // P+0x110 0x101A6330
{
    unsigned int position;
    {
        const std::string first = getEditorLowDetailmapBaseName();                                  // P+0x10C
        position = lastTx(first);
    }
    const std::string second = getEditorLowDetailmapBaseName();
    return stringPrefix(second, position);
}


// (E+0x88) - the technique and its overlays: technique 4 builds the undergrowth overlays
// and pokes the UndergrowthSystem (+0x54, an empty slot in the plain system), 5 the overgrowth
// overlays and the OverGrowth export, 6 the overgrowth-shadow overlays and its export, 8 the material
// overlays; every other overlay set is released. The control flow is the shipped one, including the
// jump from the technique-5 arm past the overgrowth release. The two exports go through
// dword_10258F38 with no null test; one whose OverGrowth map is missing calls this slot again with 0
// (re-entrant), so the technique ends at 0 with every overlay released.

void TerrainEditable::setShaderTechnique(int technique)
{
    m_shaderTechnique = technique;
    if (technique == 4)
    {
        createUndergrowthOverlays();
        static_cast<UndergrowthSystem*>(g_UndergrowthSystem)->editorHook54();                        // unguarded
    }
    else
    {
        releaseUndergrowthOverlays();
        if (technique == 5)
        {
            createOvergrowthOverlays();
            static_cast<OverGrowth*>(dx9rend::g_OverGrowth)->exportToTerrainTexture();
            goto shadowAndMaterial;
        }
    }
    releaseOvergrowthOverlays();
    if (technique == 6)
    {
        createOvergrowthShadowOverlays();
        static_cast<OverGrowth*>(dx9rend::g_OverGrowth)->exportShadowmapToTerrainTexture();
        releaseMaterialOverlays();
        return;
    }
shadowAndMaterial:
    releaseOvergrowthShadowOverlays();
    if (technique != 8)
        releaseMaterialOverlays();
    else
        createMaterialOverlays();                                                                    // (R2: written - writes the heightmap's materials)
}

int TerrainEditable::getShaderTechnique() const { return m_shaderTechnique; }

// (E+0x90) - the setting "Editor_LevelSavePath" (default " ERROR ") plus "/". The body
// never reads `this`.
TerrainEditableString TerrainEditable::getLevelSavePath() const
{
    std::string path;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", " ERROR ", path);
    std::string withSlash(path);
    withSlash += "/";
    return ::renddx9abi::toHostString(withSlash);
}

// (E+0x94) - only a changed name: the record's main texture reference dropped (not
// nulled), the name stored straight into the record (+0x2C; NOT through its setName slot) and the
// texture fetched again. The other three textures are left alone. The layer is not bounded.
void TerrainEditable::setDetailTextureName(int layer, const TerrainEditableString& name)
{
    TerrainDetailTextureEditable* const record = static_cast<TerrainDetailTextureEditable*>(
        m_detailTextures.data()[layer]);
    const std::string newName(::renddx9abi::fromHostString(name));
    if (record->name.size() == newName.size()
        && std::memcmp(record->name.c_str(), newName.c_str(), newName.size()) == 0)
        return;
    terrainReleaseTexture(record->texture);
    record->name = newName;
    record->texture = dx9rend::textureManagerGetTexture(newName);
}

// the record's material byte (+0x14), read and written directly.
unsigned char TerrainEditable::getDetailMaterial(int layer) const
{
    return m_detailTextures.data()[layer]->materialId;
}
void TerrainEditable::setDetailMaterial(int layer, unsigned char material)
{
    m_detailTextures.data()[layer]->materialId = material;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
