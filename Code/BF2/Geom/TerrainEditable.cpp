// Terrain editing, rendering, saving and heightmap-modified event handling.
// [CHOSEN, not recovered] BF2_IMAGE_EDITOR initializes the legacy grid and skips water drawing
// without a culler; game images retain their existing behavior.
#ifdef BF2_IMAGE_RENDDX9

#include "TerrainEditable.h"
#include "QuadPatchEditable.h"
#include "SurroundingPatchEditable.h"
#include "QuadPatch.h"
#include "Terrain.h"
#include "TerrainLightSampling.h"             // createLightSampler's TerrainLightSampler
#include "XpLightManager.h"                   // LightManager::getSinglePointColor (+0x1F0)
#include "StaticMesh.h"                       // staticMeshPackVec4 (sub_1008DD80)
#include "UndergrowthSystem.h"                // g_UndergrowthSystem, slot +0x54
#include "Overgrowth/OverGrowth.h"            // OverGrowth::exportToTerrainTexture (0x100C8F40)

#include "../Core/EventManager.h"             // eventManager's methods, ECEditor
#include "../Math/Frustum.h"
#include "../Math/Mat4.h"
#include "../Physics/HeightmapCluster.h"
#include "../Physics/Heightmap.h"
#include "../RendDX9/RendDX9Base.h"           // the effect / state / buffer / texture seams
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"        // renddx9abi::logf - dice's debug stream
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"     // classManager, fileManager, eventManager
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"

#if BF2_PLUGIN_ABI_NATIVE
#include "HeightmapEventNode.h"               // the native host's event node
#endif

#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_shaderManager;       // dword_1023B97C - ShaderManager.cpp
extern void* g_raShaderConstants;   // dword_1023DDBC - RenderQueues.cpp
extern void* g_OverGrowth;          // dword_10258F38 - BF2Render.cpp, the level OverGrowth

// BufferManager.cpp / StateManager.cpp, declared per TU as every other client does.
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);  // sub_10011FF0 on the global
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);                                     // --ref, sub_10014510, free
void  bufferManagerRemoveVertexBuffer(void* wrapper);
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  indexBufferRelease(void* wrapper);                                      // --ref, sub_10014300, free
void  bufferManagerRemoveIndexBuffer(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  indexBufferBind(void* wrapper);
void  vertexDeclarationBind(void* wrapper);
unsigned int vertexBufferStride(void* wrapper);
unsigned int vertexBufferVertexCount(void* wrapper);
} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;                                          // ModuleGeom.cpp, dword_1024B914

namespace {

// dword_1021AE18 / dword_1021AE1C (Core/Ids.h IID_ITerrain / IID_ITerrainEditable).
const unsigned int kIID_ITerrain = 820004u;
// ECEditor event 100, "ECEditorEEHeightmapModified" - registered by init, answered by handleEvent.
// The editable heightmaps post it (Geom/HeightmapEventNode.h).
const unsigned int kEventHeightmapModified = 100u;

// Process-wide gate: only the first updated terrain rebuilds its low-detail maps.
unsigned char g_reloadLowDetailMapsPending = 1;

// off_10239754 - the technique names drawPatches selects by +0x780. Nine entries and NO bound
// check in the shipped draw (0x1010A3E5 indexes the table with the raw technique): a technique past 8
// reads the .data word after the table (0x28800000) as a string pointer.
const char* const kEditorTechniques[9] =
{
    "t0",                      // 0
    "EditorGrid",              // 1  TEXLAYER1 = the grid texture
    "EditorTopoGrid",          // 2  TEXLAYER1 = the grid3 texture
    "EditorDetailTextured",    // 3  drawn as 7 then 3
    "EditorUndergrowth",       // 4
    "EditorOvergrowth",        // 5
    "EditorOvergrowthShadow",  // 6
    "EditorDetailBasePass",    // 7
    "EditorMaterialmap",       // 8
};

// word_1021B4C8 - the declaration init creates into +0x2A4: the 16-byte base-grid stream (xz, uv),
// the per-patch height stream and the per-patch normal stream (decoded from the image bytes).
const D3DVERTEXELEMENT9 kDeclTerrainEditable[] =
{
    { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 1, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
    { 2, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    D3DDECL_END()
};

inline dx9rend::ShaderParameter* param(void* handle)
{
    return static_cast<dx9rend::ShaderParameter*>(handle);
}

inline ::dice::hfe::world::IHeightmapCluster* heightmapClusterByName()
{
    IBase* const found = ::dice::hfe::classManager->getSingleton(
        ::renddx9abi::HostString("HeightmapCluster"));
    return static_cast< ::dice::hfe::world::IHeightmapCluster*>(static_cast<void*>(found));
}

// std::operator!=(const std::string&, const std::string&) - vc71::string publishes no comparison.
inline bool stringsDiffer(const vc71::string& a, const vc71::string& b)
{
    return a.size() != b.size() || std::memcmp(a.c_str(), b.c_str(), a.size()) != 0;
}

// The texture-owner release every body here open-codes: `if (t && !--t->refCount) { sub_10014E20(t);
// sub_1005F360(t); }` (Terrain.cpp's terrainReleaseTexture) - no eviction, no store to the holder.
inline void releaseOwnerTexture(void* texture)
{
    terrainReleaseTexture(texture);
}


// The +0x2B0 multimap<float, QuadPatchEditable*> update() sorts the visible patches into (the
// constructor's first map, sub_100E1760, 0x18-byte nodes; Terrain.h). VC7.1 _Tree, transcribed from
// TerrainEditable's own instantiation: insert 0x1010E550, _Insert 0x1010E3D0, _Buynode 0x1010E340 with
// the node constructor 0x100492F0, and the image-shared rotations _Lrotate 0x100AE2F0 / _Rrotate
// (both test _Isnil at +0x15, i.e. this node size). The red-black fixup is the shipped one.

struct PatchQueueNode
{
    PatchQueueNode*    left;        // +0x00
    PatchQueueNode*    parent;      // +0x04
    PatchQueueNode*    right;       // +0x08
    float              key;         // +0x0C  the squared camera distance
    QuadPatchEditable* value;       // +0x10
    unsigned char      color;       // +0x14  0 red, 1 black
    unsigned char      isNil;       // +0x15
    unsigned char      pad[2];
};
static_assert(sizeof(PatchQueueNode) == 0x18, "_Buynode (0x1010E340) allocates 0x18");

inline PatchQueueNode* queueHead(const vc71::map_layout& map)
{
    return reinterpret_cast<PatchQueueNode*>(map.head);
}

// sub_100E1800 - _Erase: the right subtree recursively, then down the left spine, freeing nodes only.
void eraseQueueSubtree(PatchQueueNode* node)
{
    while (!node->isNil)
    {
        eraseQueueSubtree(node->right);
        PatchQueueNode* const left = node->left;
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

// update()'s inlined clear (0x1010C67A..0x1010C6AE): _Erase(root), then the head relinked in the
// shipped store order - _Parent, _Mysize, _Left, _Right.
void clearPatchQueue(vc71::map_layout& map)
{
    PatchQueueNode* node = queueHead(map)->parent;
    while (!node->isNil)
    {
        eraseQueueSubtree(node->right);
        PatchQueueNode* const left = node->left;
        ::renddx9abi::engineFree(node);
        node = left;
    }
    PatchQueueNode* const head = queueHead(map);
    head->parent = head;
    map.size     = 0;
    head->left   = head;
    head->right  = head;
}

// 0x100AE2F0 - _Lrotate.
void rotateLeft(vc71::map_layout& map, PatchQueueNode* where)
{
    PatchQueueNode* const pivot = where->right;
    where->right = pivot->left;
    if (!pivot->left->isNil)
        pivot->left->parent = where;
    pivot->parent = where->parent;
    PatchQueueNode* const head = queueHead(map);
    if (where == head->parent)
        head->parent = pivot;
    else if (where == where->parent->left)
        where->parent->left = pivot;
    else
        where->parent->right = pivot;
    pivot->left   = where;
    where->parent = pivot;
}

// 0x1008A260 - _Rrotate.
void rotateRight(vc71::map_layout& map, PatchQueueNode* where)
{
    PatchQueueNode* const pivot = where->left;
    where->left = pivot->right;
    if (!pivot->right->isNil)
        pivot->right->parent = where;
    pivot->parent = where->parent;
    PatchQueueNode* const head = queueHead(map);
    if (where == head->parent)
        head->parent = pivot;
    else if (where == where->parent->right)
        where->parent->right = pivot;
    else
        where->parent->left = pivot;
    pivot->right  = where;
    where->parent = pivot;
}

// 0x1010E550 + multimap::insert. The descent goes left only when key < node key (the
// listing's `fcomp; test ah, 5; jp right`): equal keys, larger keys and NaN go right, so ties keep
// insertion order. The answered iterator pair is dropped by update().
void insertPatchQueue(vc71::map_layout& map, float key, QuadPatchEditable* value)
{
    PatchQueueNode* const head = queueHead(map);
    PatchQueueNode* where = head;
    PatchQueueNode* cur   = head->parent;
    bool addLeft = true;
    while (!cur->isNil)
    {
        where = cur;
        if (key < cur->key)
        {
            cur = cur->left;
            addLeft = true;
        }
        else
        {
            cur = cur->right;
            addLeft = false;
        }
    }

    if (map.size >= 0x1FFFFFFEu)
        throw std::length_error("map/set<T> too long");

    // _Buynode: engine heap, then the node constructor - left = head, parent = where, right = head,
    // the pair, red, not nil. A failed allocation is not tested: _Insert dereferences the null.
    PatchQueueNode* const node = static_cast<PatchQueueNode*>(::renddx9abi::engineAlloc(0x18));
    if (node)
    {
        node->left   = head;
        node->right  = head;
        node->parent = where;
        node->key    = key;
        node->value  = value;
        node->color  = 0;
        node->isNil  = 0;
    }
    ++map.size;

    if (where == head)
    {
        head->parent = node;
        head->left   = node;
        head->right  = node;
    }
    else if (addLeft)
    {
        where->left = node;
        if (where == head->left)
            head->left = node;
    }
    else
    {
        where->right = node;
        if (where == head->right)
            head->right = node;
    }

    for (PatchQueueNode* p = node; p->parent->color == 0; )
    {
        PatchQueueNode* const parent = p->parent;
        PatchQueueNode* const grand  = parent->parent;
        if (parent == grand->left)
        {
            PatchQueueNode* const uncle = grand->right;
            if (uncle->color == 0)
            {
                parent->color = 1;
                uncle->color  = 1;
                grand->color  = 0;
                p = grand;
            }
            else
            {
                if (p == parent->right)
                {
                    p = parent;
                    rotateLeft(map, p);
                }
                p->parent->color = 1;
                p->parent->parent->color = 0;
                rotateRight(map, p->parent->parent);
            }
        }
        else
        {
            PatchQueueNode* const uncle = grand->left;
            if (uncle->color == 0)
            {
                parent->color = 1;
                uncle->color  = 1;
                grand->color  = 0;
                p = grand;
            }
            else
            {
                if (p == parent->left)
                {
                    p = parent;
                    rotateRight(map, p);
                }
                p->parent->color = 1;
                p->parent->parent->color = 0;
                rotateLeft(map, p->parent->parent);
            }
        }
    }
    head->parent->color = 1;
}

// The in-order walk drawPatches inlines three times (0x1010A3A0.. / 0x1010A44F.. / 0x1010A54E..):
// each patch's slot +0x04 draw with 0, the iterator advanced by the VC7.1 _Inc, the end re-read from
// the map every step.
void drawPatchQueue(const vc71::map_layout& map)
{
    PatchQueueNode* node = queueHead(map)->left;
    if (node == queueHead(map))
        return;
    do
    {
        node->value->draw(0);                                                  // QuadPatchEditable slot +0x04
        if (!node->isNil)
        {
            if (node->right->isNil)
            {
                PatchQueueNode* p = node->parent;
                while (!p->isNil && node == p->right)
                {
                    node = p;
                    p = p->parent;
                }
                node = p;
            }
            else
            {
                node = node->right;
                for (PatchQueueNode* l = node->left; !l->isNil; l = l->left)
                    node = l;
            }
        }
    }
    while (node != queueHead(map));
}

// EventManager +0x34 registerEvent(category, id, name). The typed interface spells the native
// std::string; the retail host's slot takes its own VC7.1 string, so the compatibility flavour reaches
// it by the slot with the flavour's string type - the same seam NametagManager uses for ISwiffHost +0xA8
// (RW-266). The answer is dropped, as init drops it.
void registerHeightmapModifiedEvent()
{
#if BF2_PLUGIN_ABI_NATIVE
    ::dice::hfe::eventManager->registerEvent(ECEditor, kEventHeightmapModified,
                                             std::string("ECEditorEEHeightmapModified"));
#else
    typedef bool (__thiscall* RegisterEventFn)(void*, int, unsigned int, const vc71::string&);
    void* const manager = ::dice::hfe::eventManager;
    const vc71::string name("ECEditorEEHeightmapModified");
    reinterpret_cast<RegisterEventFn>((*reinterpret_cast<void***>(manager))[0x34 / 4])(
        manager, static_cast<int>(ECEditor), kEventHeightmapModified, name);
#endif
}

// The changed rectangle (x0, z0, x1, z1) of a heightmap-modified event. handleEvent reads it from the
// handler's user-data word, which the EventManager sets to the node itself for typed nodes. Retail:
// RendDX9's own accessor 0x101C8450 (`lea eax, [ecx+30h]`) over the retail host's 0x40-byte node; the
// native host posts the tree's geom::HeightmapEventNode, read through its accessor.
inline const float* heightmapEventArea(void* data)
{
#if BF2_PLUGIN_ABI_NATIVE
    const HeightmapEventNode* const node =
        static_cast<const HeightmapEventNode*>(static_cast<EventNode*>(data));
    return &node->getAffectedArea().x;
#else
    return reinterpret_cast<const float*>(static_cast<const unsigned char*>(data) + 0x30);
#endif
}

// R2

// The lightmap-generation pass's walk of the +0x2B0 queue (0x1010A765..0x1010A77E): each patch's
// 0x1019E340 in key order, the iterator advanced by the image's out-of-line _Inc (sub_100A54F0 - the
// same steps drawPatchQueue inlines), the end re-read every step.
void drawPatchQueueForLightmapGeneration(const vc71::map_layout& map)
{
    PatchQueueNode* node = queueHead(map)->left;
    if (node == queueHead(map))
        return;
    do
    {
        node->value->drawForLightmapGeneration();
        if (!node->isNil)
        {
            if (node->right->isNil)
            {
                PatchQueueNode* p = node->parent;
                while (!p->isNil && node == p->right)
                {
                    node = p;
                    p = p->parent;
                }
                node = p;
            }
            else
            {
                node = node->right;
                for (PatchQueueNode* l = node->left; !l->isNil; l = l->left)
                    node = l;
            }
        }
    }
    while (node != queueHead(map));
}

// sub_10074AB0 - the image's printf-into-a-std::string helper (QuadPatchEditable.cpp carries the same
// transcription): _vsnprintf into a 0xFFF-byte window, the shipped warning when it does not fit.
void formatString(vc71::string& out, const char* format, ...)
{
    char buffer[0x1000];
    va_list args;
    va_start(args, format);
    const int written = _vsnprintf(buffer, 0xFFF, format, args);
    va_end(args);
    if (written < 0)
        std::snprintf(buffer, sizeof(buffer), "%s",
                      "WARNING: The original message was too long for formatString(). \n");
    else
        buffer[written] = 0;
    out = buffer;
}

// sub_100751A0 -> everything before the path's last separator (the larger signed
// rfind of '\\' and '/'), then `separator` appended; empty when there is none. The import slots call
// it with '/' and never read the answer.
vc71::string pathDirectoryOf(const vc71::string& path, char separator)
{
    int last = -1;
    const char* const text = path.c_str();
    for (unsigned int i = 0; i < path.size(); ++i)
        if (text[i] == '\\' || text[i] == '/')
            last = static_cast<int>(i);
    if (last == -1)
        return vc71::string();
    vc71::string out(text, static_cast<unsigned int>(last));
    const char tail[2] = { separator, 0 };
    out += tail;
    return out;
}

} // anonymous namespace


// TerrainDetailTextureEditable - the 0x78-byte record (table 0x1021B780).

// The constructor TerrainEditable::init inlines (0x1010ADC6..0x1010ADF2): the plain record's
// constructor (0x100D8C40), the two strings, then +0x64..+0x74 in order.
TerrainDetailTextureEditable::TerrainDetailTextureEditable()
{
    colorTexture = 0;                                                         // +0x64
    editableInt68 = 0;                                                        // +0x68
    colorTextureTiling.x = 1.0f;                                              // +0x6C
    colorTextureTiling.y = 1.0f;                                              // +0x70
    lowDetailType = 0;                                                        // +0x74
}

// (the deleting form; the free is the caller's here). The colour texture's owner
// reference only - no eviction, no store - then the strings and the plain record's destructor.
TerrainDetailTextureEditable::~TerrainDetailTextureEditable()
{
    releaseOwnerTexture(colorTexture);
}

// (`retn 1Ch`: the name by value). Only on a changed name: the main texture's reference
// dropped (the normal / side / side-normal textures are overwritten without a release), the name
// stored, the textures fetched again. The side pair falls back to the main pair - with NO added
// reference - when "<name>_side" does not exist.
void TerrainDetailTextureEditable::setName(vc71::string newName)
{
    if (!stringsDiffer(name, newName))
        return;
    releaseOwnerTexture(texture);
    name = newName;
    texture = dx9rend::textureManagerGetTexture(newName);

    vc71::string normal(newName);
    normal += "_normal";
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(normal)))
    {
        vc71::string again(newName);
        again += "_normal";
        normalTexture = dx9rend::textureManagerGetTexture(again);
    }

    vc71::string side(newName);
    side += "_side";
    if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(side)))
    {
        vc71::string again(newName);
        again += "_side";
        sideTexture = dx9rend::textureManagerGetTexture(again);

        vc71::string sideNormal(newName);
        sideNormal += "_sideNormal";
        if (::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(sideNormal)))
        {
            vc71::string again2(newName);
            again2 += "_sideNormal";
            sideNormalTexture = dx9rend::textureManagerGetTexture(again2);
        }
    }
    else
    {
        sideTexture = texture;
        sideNormalTexture = normalTexture;
    }
}

vc71::string TerrainDetailTextureEditable::getName()
{
    return name;
}

// the same shape over the colour texture (+0x48 / +0x64).
void TerrainDetailTextureEditable::setColorTextureName(vc71::string newName)
{
    if (!stringsDiffer(colorTextureName, newName))
        return;
    releaseOwnerTexture(colorTexture);
    colorTextureName = newName;
    colorTexture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(newName));
}

vc71::string TerrainDetailTextureEditable::getColorTextureName()
{
    return colorTextureName;
}

void TerrainDetailTextureEditable::setEditableInt68(int value) { editableInt68 = value; }
int  TerrainDetailTextureEditable::getEditableInt68() { return editableInt68; }
void TerrainDetailTextureEditable::setColorTextureTiling(Vec2 tiling) { colorTextureTiling = tiling; }
Vec2 TerrainDetailTextureEditable::getColorTextureTiling() { return colorTextureTiling; }
void TerrainDetailTextureEditable::setLowDetailType(int type) { lowDetailType = type; }
int  TerrainDetailTextureEditable::getLowDetailType() { return lowDetailType; }


// TerrainEditable - construction, teardown, IBase

// Terrain's constructor, the seven vptrs, then the members in declaration order; the
// eight surrounding slots are zeroed in the body, after the strings, and the five nvdxt option
// strings assigned last. +0x7B0 / +0x7B4 are not initialised (the frame constants compute them on
// the first frame, because the +0x7A8 cache starts at 0 and the distance at 75).
TerrainEditable::TerrainEditable()
    : m_gridTexture(0),
      m_grid3Texture(0),
      m_shaderTechnique(0),
      m_editablePatches(0),
      m_lodSwitchDistanceCache(0.0f),
      m_lodSwitchDistance(75.0f),
      m_drawDetailLayerAsColor(-1),
      m_surroundingTerrainEngineMode(0),
      m_useCompDetailMaps(0),
      m_roadSamplingSize(4),
      m_showHeightmap(0)
{
    for (int i = 0; i < 8; ++i)
        m_editableSurrounding[i] = 0;
    m_colormapSettings               = "-dxt1c -Kaiser -sharpenMethod SharpenMedium";
    m_lowDetailmapSettings           = "-dxt1c -prescale 256 256 -Box";
    m_detailmapSettings              = "-u565 -prescale 256 256 -Box";
    m_surroundingColormapSettings    = "-dxt1c -Kaiser -sharpenMethod SharpenMedium";
    m_surroundingLowDetailmapSettings = "-dxt1c -prescale 256 256 -Box";
}

// Releases grid textures and event registrations before base destruction.
// The base may read uninitialized shader handles/detail counts; editable patches and records leak.
TerrainEditable::~TerrainEditable()
{
    releaseOwnerTexture(m_grid3Texture);                                      // +0x77C
    releaseOwnerTexture(m_gridTexture);                                       // +0x778
    ::dice::hfe::eventManager->unregisterEvent(ECEditor, kEventHeightmapModified);           // +0x38
    ::dice::hfe::eventManager->unregisterEventHandler(ECEditor, static_cast<IEventListener*>(this));   // +0x30
}

void TerrainEditable::addRef()  { Terrain::addRef(); }                         // (ICF with Terrain's)
int  TerrainEditable::getRef()  { return Terrain::getRef(); }                  // (ICF)
int  TerrainEditable::release() { return Terrain::release(); }                 // 0x1010DE30 `jmp Terrain_release`

// The subobject answers are the compiler's null-tested casts; the plain IBase id 1 is
// tested last.
void* TerrainEditable::queryInterface(unsigned int iid) const
{
    TerrainEditable* const self = const_cast<TerrainEditable*>(this);
    if (iid == kIID_ITerrain)
        return self;
    switch (iid)
    {
    case kIID_ITerrainEditable:
        return static_cast<ITerrainEditable*>(self);                          // +0x768
    case kIID_IConsoleSaveable_TE:
        return static_cast< ::dice::hfe::io::IConsoleSaveable*>(self);         // +0x76C
    case kIID_ILightSampleable_TE:
        return static_cast<ILightSampleable*>(self);                          // +0x774
    default:
        break;
    }
    return iid == 1u ? self : 0;
}

// (primary slot +0xB0): the destructor body, the free on bit 0, `this` answered.
void* TerrainEditable::deletingDestructor(int flags)
{
    this->~TerrainEditable();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}


// init - 0x1010ADA0 (primary slot +0x14). Replaces Terrain::init; does not call it.

void TerrainEditable::init()
{
    // Six editable detail records into +0x338 (the push_back's fast path and _Insert_n 0x100E1940
    // both append; vectorPushBackIn is the tree's growth policy, RW-74).
    for (int count = 6; count != 0; --count)
    {
        void* const storage = ::renddx9abi::engineAlloc(0x78);
        TerrainDetailTexture* const record =
            storage ? new (storage) TerrainDetailTextureEditable() : 0;
        vc71::vectorPushBackIn(m_detailTextures, 4u, &record);
    }

    ::dice::hfe::eventManager->registerEventHandler(ECEditor, static_cast<IEventListener*>(this), 0);   // +0x2C
    registerHeightmapModifiedEvent();                                         // +0x34 (8, 100, ...)

    m_effect = dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "Shaders/TerrainEditorShader");
    // The effect is re-read for every lookup, as shipped.
#define TE_PARAM(member, name) \
    member = dx9rend::effectGetShaderParameter(static_cast<dx9rend::Effect*>(m_effect), vc71::string(name))
    TE_PARAM(m_hScaleTransXZ, "SCALETRANSXZ");                                // +0x1B8
    TE_PARAM(m_hScaleTransY,  "SCALETRANSY");                                 // +0x1BC
    TE_PARAM(m_hMatViewProj,  "matVIEWPROJ");                                 // +0x1D0
    TE_PARAM(m_hTexLayer[0],  "TEXLAYER0");                                   // +0x1D8
    TE_PARAM(m_hTexLayer[1],  "TEXLAYER1");
    TE_PARAM(m_hTexLayer[2],  "TEXLAYER2");
    TE_PARAM(m_hTexLayer[3],  "TEXLAYER3");
    TE_PARAM(m_hTexLayer[4],  "TEXLAYER4");
    TE_PARAM(m_hTexLayer[5],  "TEXLAYER5");
    TE_PARAM(m_hTexLayer[6],  "TEXLAYER6");                                   // +0x1F0
    m_hComponentSelector = 0;                                                 // +0x1FC
    m_hTexProjOffset     = 0;                                                 // +0x200
    m_hCameraPos         = 0;                                                 // +0x208
    TE_PARAM(m_hSunColor,          "SUNCOLOR");                               // +0x20C
    TE_PARAM(m_hGIColor,           "GICOLOR");                                // +0x210
    TE_PARAM(m_unresolved27C,      "POINTCOLOR");                             // +0x27C - this class's pointcolor
    TE_PARAM(m_hWaterHeight,       "WaterHeight");                            // +0x280
    TE_PARAM(m_hTerrainWaterColor, "TerrainWaterColor");                      // +0x288
    TE_PARAM(m_hFogRange,          "fogRange");                               // +0x220
    TE_PARAM(m_hFogColor,          "fogColor");                               // +0x228
#undef TE_PARAM
    m_hMorphDeltaSelector = 0;                                                // +0x23C
    m_hNearFarMorphLimits = 0;                                                // +0x244 (the frame constants look it up by name)
    m_hFarTexTiling       = 0;                                                // +0x248
    m_hNearTexTiling      = 0;                                                // +0x250
    m_hLightViewProj      = 0;                                                // +0x254
    m_hViewportMap        = 0;                                                // +0x25C
    m_hStTransXZ          = 0;                                                // +0x260
    m_hPointLight         = 0;                                                // +0x264
    m_hSpotLight          = 0;                                                // +0x268

    m_gridTexture  = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerGetTexture(vc71::string("BF2Editor/Content/Terrain/Grid")));
    m_grid3Texture = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerGetTexture(vc71::string("BF2Editor/Content/Terrain/Grid3")));

    // The level's low-detail texture, else the common default (the settings default " FEEEEL " is
    // the shipped one).
    vc71::string levelPath;
    dx9rend::readStringSettingDefault("GLGameLevelPath", " FEEEEL ", levelPath);
    vc71::string lowDetailPath(levelPath);
    lowDetailPath += "/lowDetailTexture.dds";
    if (!::dice::hfe::fileManager->fileExists(::renddx9abi::makeHostString(lowDetailPath)))
        lowDetailPath = "common/terrain/textures/Default.dds";
    m_lowDetailTexture = dx9rend::textureManagerGetTexture(lowDetailPath);

    m_surroundingDeclaration = dx9rend::vertexDeclarationCreate(kDeclTerrainEditable);   // +0x2A4

    // The heightmap cluster - no test on it, its primary heightmap or the scales, as shipped.
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    m_primaryHeightmap = cluster->getPrimaryHeightmap();                      // +0x40
    m_primaryHeightmap->getSize(m_heightmapSizeX, m_heightmapSizeZ);          // +0x2C
    m_primaryWorldScale = m_primaryHeightmap->getScale();                     // +0x54
    m_secondaryWorldScale = cluster->getSecondaryScale();                     // +0x58

    // The patch grid: patchCount^2 slots, filled row by row from (-n/2, -n/2). `n / -2` .. `n / 2`
    // truncates for an odd count (the last row and column are never built), and init runs on a failed
    // allocation's null (QuadPatchEditable::init is not guarded) - both as shipped.
    m_patchCount = m_heightmapSizeX / m_patchSize;
    m_editablePatches = static_cast<QuadPatchEditable**>(
        ::renddx9abi::engineAlloc(static_cast<unsigned int>(4 * m_patchCount * m_patchCount)));
    int next = 0;
    for (int z = m_patchCount / -2; z < m_patchCount / 2; ++z)
    {
        for (int x = m_patchCount / -2; x < m_patchCount / 2; ++x)
        {
            void* const storage = ::renddx9abi::engineAlloc(0x32C);
            QuadPatchEditable* const patch = storage ? new (storage) QuadPatchEditable(this) : 0;
            patch->init(x, z);
            m_editablePatches[next] = patch;
            ++next;
        }
    }

    m_flag2AD = 0;
    m_flag2AC = 0;
    buildLegacyBaseGridVertexTable();                                         // primary +0xD0: this class's grid VB
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor-only region 1 (TerrainEditable.cpp header): Terrain's own +0xD0
    // body builds the packed legacy table (+0xD4) that createLodIndexBuffer's optimiser reads - the
    // 2006 editor calls it here, between its VB builder and the index buffer.
    Terrain::buildLegacyBaseGridVertexTable();
#endif
    // Outside the editor build, optimizeLodIndices reads a table this class never builds.
    createLodIndexBuffer();
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor-only region 1, continued: the 2006 editor frees the table once
    // the index buffer exists.
    ::renddx9abi::engineFree(m_legacyBaseGridTable);
    m_legacyBaseGridTable = 0;
#endif
    buildSurroundingGeometry();                                               // primary +0xCC
    setupWater();                                                             // primary +0xD4 (Terrain's)
}


// The per-frame path

// (primary slot +0x28): the pending layer imports, the one-shot low-detail rebuild, then
// the visible patches sorted by squared camera distance into the +0x2B0 multimap, then Terrain's
// water constants (slot +0xE0). The patch count is re-read on every pass, as shipped.
void TerrainEditable::update()
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
    {
        QuadPatchEditable* const patch = m_editablePatches[i];
        if (patch->m_importPending)                                           // +0x329
            patch->importAllLayers();
    }
    if (g_reloadLowDetailMapsPending)
    {
        reloadLowDetailMaps();
        g_reloadLowDetailMapsPending = 0;
    }

    clearPatchQueue(m_map2B0);

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();      // Renderer +0x48
    Frustum* const frustum = view->getFrustumPtr();                           // +0x74
    const Vec3& camera = *reinterpret_cast<const Vec3*>(&view->getViewMatrixPtr()->m[12]);   // +0x64, row 3
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
    {
        QuadPatchEditable* const patch = m_editablePatches[i];
        if (patch->isVisible(frustum))
            insertPatchQueue(m_map2B0, patch->getSquaredDistance(camera), patch);
    }
    updateWaterConstants();                                                   // primary +0xE0
}

// (primary slot +0x2C, Terrain's shared `ret`): the editor draw. Reached through the
// render-path listener's +0x0C (0x101076D0), which renderWorldPassBegin (+0xB8) calls with 0.
void TerrainEditable::slot11_noop()
{
    dx9rend::stateManagerPush();
    update();                                                                 // primary +0x28
    applyEditorFrameConstants();
    if (m_drawMain)                                                           // +0x358
        drawPatches();
    if (m_flag359)                                                            // +0x359
        drawEditorSurrounding();
    drawEditorWater();
    dx9rend::stateManagerPop();
}

// the per-frame shader constants. The morph limits are recomputed only when the LOD
// switch distance changed (`fucompp; test ah, 44h; jnp`: a NaN distance recomputes every frame).
void TerrainEditable::applyEditorFrameConstants()
{
    dx9rend::vertexDeclarationBind(m_surroundingDeclaration);                // +0x2A4
    const float distance = m_lodSwitchDistance;
    if (m_lodSwitchDistanceCache != distance)
    {
        m_lodSwitchDistanceCache = m_lodSwitchDistance;
        const float far0  = (distance * 0.015625f - 1.0f) * 64.0f + 22.56f;  // flt_1021A14C / 101DDDE4 / 1021BB20 / 1021BB1C
        const float near0 = 0.4f * far0;                                      // flt_1021BB18, stored as a float
        const float scale = 1.0f / (far0 - near0);
        m_morphLimitScale  = scale;                                           // +0x7B0
        m_morphLimitOffset = near0 * scale;                                   // +0x7B4
    }
    dx9rend::Effect* const fx = static_cast<dx9rend::Effect*>(m_effect);
    {
        float limits[2] = { m_morphLimitScale, m_morphLimitOffset };
        dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("NEARFARMORPHLIMITS"))->setValue(limits, 0);
    }
    unsigned char* const constants = static_cast<unsigned char*>(dx9rend::g_raShaderConstants);
    param(m_hFogRange)->setValue(constants + 0xA8, 0);                        // FogRange holder
    param(m_hFogColor)->setValue(constants + 0xBC, 0);                        // FogColor holder
    param(m_hSunColor)->setValue(&m_sunColor, 0);                             // +0x2E4
    param(m_hGIColor)->setValue(&m_giColor, 0);                               // +0x2F0

    // The view-projection: +0x6C's answer is pushed first and +0x68's second, so the product is
    // fullMult(+0x68, +0x6C) - the same pair Terrain::applyFrameConstants multiplies.
    Mat4 viewProj = Mat4::getIdentity();
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();   // Renderer +0x50
    {
        const Mat4* const projection = view->getProjectionMatrixPtr();        // +0x6C
        const Mat4* const inverseView = view->getInverseViewMatrixPtr();      // +0x68
        viewProj.fullMult(*inverseView, *projection);
    }
    param(m_hMatViewProj)->setValue(&viewProj, 0);                            // +0x1D0

    {
        const float* const packed = staticMeshPackVec4(&g_LightManager->getSinglePointColor().x, 1.0f);
        float pointColor[4] = { packed[0], packed[1], packed[2], packed[3] };
        param(m_unresolved27C)->setValue(pointColor, 0);                      // +0x27C pointcolor
    }

    float seaLevel = 0.0f;
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    if (cluster)
        seaLevel = cluster->getSeaWaterLevel();                               // +0x60
    param(m_hWaterHeight)->setValue(&seaLevel, 0);                            // +0x280

    {
        const float* const packed = staticMeshPackVec4(&m_terrainWaterColor.x, 1.0f);
        float waterColor[4] = { packed[0], packed[1], packed[2], packed[3] };
        dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("TerrainWaterColor"))->setValue(waterColor, 0);
    }

    // camerapos: the camera row as a Vec4, w = 1, then w = |y - terrain height under the camera| when
    // the cluster cache (+0x2DC) can be filled (no "+ 1" here, unlike Terrain's).
    float cameraPos[4];
    {
        const float* const packed = staticMeshPackVec4(&view->getViewMatrixPtr()->m[12], 1.0f);   // +0x64 row 3
        cameraPos[0] = packed[0];
        cameraPos[1] = packed[1];
        cameraPos[2] = packed[2];
        cameraPos[3] = packed[3];
    }
    if (!m_heightmapCluster)
        m_heightmapCluster = heightmapClusterByName();
    if (m_heightmapCluster)
    {
        const Vec3* const origin = terrainOriginScratch(*reinterpret_cast<const Vec3*>(cameraPos));
        const float height = m_heightmapCluster->getHeightInWorldCoords(*origin);                    // +0x68
        const float above = std::fabs(cameraPos[1] - height);
        m_cameraHeightAboveTerrain = above;                                   // +0x2E0
        cameraPos[3] = above;
    }
    dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("CAMERAPOS"))->setValue(cameraPos, 0);
}

// the patch draw. The technique is re-read after the stream bind; technique 3 draws the
// queue twice (as 7, "EditorDetailBasePass", then as 3 with the declaration re-bound).
void TerrainEditable::drawPatches()
{
    {
        float scaleTransY[4] = { m_primaryWorldScale.y, 1.0f, 0.0f, 0.0f };
        param(m_hScaleTransY)->setValue(scaleTransY, 0);                      // +0x1BC
    }
    dx9rend::Effect* const fx = static_cast<dx9rend::Effect*>(m_effect);
    {
        const float invSize = 1.0f / static_cast<float>(m_patchSize);
        float texScale[4];
        texScale[3] = 0.0f;
        texScale[0] = invSize;
        texScale[1] = m_primaryWorldScale.y * 400.0f * 1.5378700e-05f;        // flt_10219F94 / flt_10219F90
        texScale[2] = invSize;
        dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("TEXSCALE"))->setValue(texScale, 0);
    }
    {
        float farTiling[4];
        farTiling[0] = m_farSideTiling.x;
        farTiling[1] = m_farSideTiling.y;
        farTiling[2] = m_lowQualityB ? m_farTopTilingLow : m_farTopTilingHi;
        farTiling[3] = m_farYOffset;
        dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("FARTEXTILING"))->setValue(farTiling, 0);
    }
    if (m_shaderTechnique == 1)
        dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("TEXLAYER1"))->setValue(m_gridTexture, 0);
    if (m_shaderTechnique == 2)
        dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("TEXLAYER1"))->setValue(m_grid3Texture, 0);
    dx9rend::vertexBufferBindStream(m_baseGridVertexBuffer, 0);              // +0xD0

    const int technique = m_shaderTechnique;
    if (technique == 3)
    {
        m_shaderTechnique = 7;
        dx9rend::effectSetTechniqueByName(m_effect, vc71::string(kEditorTechniques[7]));
        dx9rend::effectBegin(m_effect);
        dx9rend::effectBeginPass(m_effect, 0);
        drawPatchQueue(m_map2B0);
        dx9rend::effectEnd(m_effect);
        dx9rend::vertexDeclarationBind(m_surroundingDeclaration);            // +0x2A4
        m_shaderTechnique = 3;
        dx9rend::effectSetTechniqueByName(m_effect, vc71::string(kEditorTechniques[3]));
        dx9rend::effectBegin(m_effect);
        dx9rend::effectBeginPass(m_effect, 0);
        drawPatchQueue(m_map2B0);
    }
    else
    {
        // No bound on the technique (see kEditorTechniques).
        dx9rend::effectSetTechniqueByName(m_effect, vc71::string(kEditorTechniques[technique]));
        dx9rend::effectBegin(m_effect);
        // The pass by the renderer's render mode (sub_10009F00, asked up to twice).
        if (dx9rend::rendererGetHemiRenderMode() == 1)
            dx9rend::effectBeginPass(m_effect, 1);
        else if (dx9rend::rendererGetHemiRenderMode() == 4)
            dx9rend::effectBeginPass(m_effect, 2);
        else
            dx9rend::effectBeginPass(m_effect, 0);
        drawPatchQueue(m_map2B0);
    }
    dx9rend::effectEnd(m_effect);
}

// the surrounding ring under "SurroundingEditorTerrain". No beginPass here: the ring
// patch's own draw (0x101A20F0, R2) is the one that opens a pass. In engine mode the corner slots
// (0, 2, 4, 6) draw over the corner buffers and the side slots over the side buffers; otherwise all
// eight over the side buffers.
void TerrainEditable::drawEditorSurrounding()
{
    {
        float scaleTransY[4] = { m_secondaryWorldScale.y, 1.0f, 0.0f, 0.0f };
        param(m_hScaleTransY)->setValue(scaleTransY, 0);
    }
    dx9rend::Effect* const fx = static_cast<dx9rend::Effect*>(m_effect);
    {
        float texScale[4];
        texScale[3] = 0.0f;
        texScale[0] = 1.0f / static_cast<float>(m_patchSize) / m_secondaryWorldScale.x;
        texScale[1] = m_secondaryWorldScale.y * 400.0f * 1.5378700e-05f;
        texScale[2] = texScale[0];
        dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("TEXSCALE"))->setValue(texScale, 0);
    }
    {
        const float count = static_cast<float>(m_patchCount);
        const float quarter = m_secondaryWorldScale.x * 0.25f;
        float farTiling[4];
        farTiling[1] = m_farSideTiling.y;
        farTiling[0] = count * m_farSideTiling.x / quarter;
        farTiling[2] = (m_lowQualityB ? count * m_farTopTilingLow : count * m_farTopTilingHi) / quarter;
        farTiling[3] = m_farYOffset;
        dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("FARTEXTILING"))->setValue(farTiling, 0);
    }
    dx9rend::effectSetTechniqueByName(m_effect, vc71::string("SurroundingEditorTerrain"));
    dx9rend::effectBegin(m_effect);
    dx9rend::effectGetShaderParameterBorrowedTyped(fx, vc71::string("TEXLAYER3"))->setValue(m_lowDetailTexture, 0);
    if (m_surroundingTerrainEngineMode)
    {
        if (m_cornerVertexBuffer && m_cornerIndexBuffer)                      // +0xE4 / +0xEC
        {
            dx9rend::vertexBufferBindStream(m_cornerVertexBuffer, 0);
            dx9rend::indexBufferBind(m_cornerIndexBuffer);
            for (int slot = 0; slot < 8; slot += 2)
                if (m_editableSurrounding[slot])
                    m_editableSurrounding[slot]->draw(0);
        }
        if (m_sideVertexBuffer && m_sideIndexBuffer)                          // +0xE0 / +0xE8
        {
            dx9rend::vertexBufferBindStream(m_sideVertexBuffer, 0);
            dx9rend::indexBufferBind(m_sideIndexBuffer);
            for (int slot = 1; slot < 8; slot += 2)
                if (m_editableSurrounding[slot])
                    m_editableSurrounding[slot]->draw(0);
        }
    }
    else if (m_sideVertexBuffer && m_sideIndexBuffer)
    {
        dx9rend::vertexBufferBindStream(m_sideVertexBuffer, 0);
        dx9rend::indexBufferBind(m_sideIndexBuffer);
        for (int slot = 0; slot < 8; ++slot)
            if (m_editableSurrounding[slot])
                m_editableSurrounding[slot]->draw(0);
    }
    dx9rend::effectEnd(m_effect);
}

// the water: the culler's visible leaves within the far distance of the camera, then
// QuadPatch::drawWater over them (Terrain's patch draw, which consumes each patch's entries). It reads
// the global terrain (dword_1024B930), not `this`.
void TerrainEditable::drawEditorWater()
{
    if (!m_flag35A)                                                           // +0x35A
        return;
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor-only region 2 (TerrainEditable.cpp header): this class never
    // builds a culler; shipped 1.50 hands the null +0x3BC to gatherVisibleSphere on the first frame.
    if (!terrain()->m_culler)
        return;
#endif
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();      // Renderer +0x48
    const Mat4* const viewMatrix = view->getViewMatrixPtr();                  // +0x64
    TerrainCuller* const culler = terrain()->m_culler;                        // +0x3BC
    const float radius = view->getFrustumPtr()->getFarDist();                 // +0x74, sub_100797B0
    culler->gatherVisibleSphere(&viewMatrix->m[12], radius);
    vc71::vector_layout* const list = terrain()->m_culler->gatheredPatches(); // re-read
    TerrainCullerNode** it = reinterpret_cast<TerrainCullerNode**>(list->first);
    while (it != reinterpret_cast<TerrainCullerNode**>(list->last))
        (*it)->patch->drawWater(it, list, m_materialParameters, m_objectParameters);   // sub_1018D800, advances `it`
}

// (primary slot +0xB8): the render-path listener's +0x0C with 0 - which draws.
void TerrainEditable::renderWorldPassBegin()
{
    static_cast<dx9rend::RenderPathListener*>(this)->onRenderPathNotification(0);
}

void TerrainEditable::renderWorldPassEnd() {}                                 // 0x10037D40, the shared `ret`

// (render-path listener slot +0x0C, `retn 4`): primary slot +0x2C, the editor draw.
void TerrainEditable::onRenderPathNotification(int)
{
    slot11_noop();
}

// update()'s one-shot: a copy of the colormap base name that nothing reads (E+0x6C),
// then every patch's low-detail map rebuilt.
void TerrainEditable::reloadLowDetailMaps()
{
    const vc71::string unused(::renddx9abi::toVc71String(getColormapBaseName()));
    (void)unused;
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->rebuildLowDetailMap();
}


// Terrain primary slots over the editable patches

float TerrainEditable::getPatchField9C(int index) { return m_editablePatches[index]->minHeight; }
float TerrainEditable::getPatchField98(int index) { return m_editablePatches[index]->maxHeight; }
void* TerrainEditable::getPatchColormap(int index) { return m_editablePatches[index]->colormap; }
void* TerrainEditable::getPatchLightmap(int index) { return m_editablePatches[index]->lightmap; }
bool  TerrainEditable::getPatchHasNoDetail(int index)
{
    return m_editablePatches[index]->hasNoDetailLayers != 0;
}

// (primary slot +0xD0) - this class's base grid: (patchSize + 1)^2 vertices of (x, z, u, v)
// floats into +0xD0, u = x / patchSize, v = z / patchSize. The lock is not tested.
void TerrainEditable::buildLegacyBaseGridVertexTable()
{
    const int side = m_patchSize + 1;
    m_baseGridVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        static_cast<unsigned int>(side * side), 16u, 8u, "TerrainEditable");
    float* const vertices = static_cast<float*>(dx9rend::vertexBufferLock(m_baseGridVertexBuffer, 0, 0, 0));
    for (int row = 0; row < m_patchSize + 1; ++row)
    {
        const float z = static_cast<float>(row);
        for (int col = 0; col < m_patchSize + 1; ++col)
        {
            const float x = static_cast<float>(col);
            float* const v = vertices + 4 * (col + row * (m_patchSize + 1));
            v[0] = x;
            v[1] = z;
            v[2] = x / static_cast<float>(m_patchSize);
            v[3] = z / static_cast<float>(m_patchSize);
        }
    }
    dx9rend::vertexBufferUnlock(m_baseGridVertexBuffer);
}

// (primary slot +0xCC) - the ring's shared geometry for the editor: 16-byte (x, z, u, v)
// vertices and 32-bit indices (format 102). Without engine mode one side grid of unit steps; in
// engine mode the three-strip side grid and the five-block corner grid Terrain's own body builds.
void TerrainEditable::buildSurroundingGeometry()
{
    if (m_sideVertexBuffer)
    {
        dx9rend::bufferManagerRemoveVertexBuffer(m_sideVertexBuffer);
        dx9rend::vertexBufferRelease(m_sideVertexBuffer);
        m_sideVertexBuffer = 0;
    }
    if (m_cornerVertexBuffer)
    {
        dx9rend::bufferManagerRemoveVertexBuffer(m_cornerVertexBuffer);
        dx9rend::vertexBufferRelease(m_cornerVertexBuffer);
        m_cornerVertexBuffer = 0;
    }
    if (m_sideIndexBuffer)
    {
        dx9rend::bufferManagerRemoveIndexBuffer(m_sideIndexBuffer);
        dx9rend::indexBufferRelease(m_sideIndexBuffer);
        m_sideIndexBuffer = 0;
    }
    if (m_cornerIndexBuffer)
    {
        dx9rend::bufferManagerRemoveIndexBuffer(m_cornerIndexBuffer);
        dx9rend::indexBufferRelease(m_cornerIndexBuffer);
        m_cornerIndexBuffer = 0;
    }

    const int span = static_cast<int>(
        static_cast<float>(m_heightmapSizeX - 1) * m_primaryWorldScale.x / m_secondaryWorldScale.x);
    vc71::vector_layout vertices = { 0, 0, 0, 0 };
    vc71::vector_layout indices  = { 0, 0, 0, 0 };
    // The generator's ninth argument (-1 at every call here) is not modelled by the tree's
    // generateSurroundingGrid (Terrain.h); nothing in it reads that word.
    if (!m_surroundingTerrainEngineMode)
    {
        generateSurroundingGrid(0, 0, span, span, 1, 1, indices, vertices);
    }
    else
    {
        generateSurroundingGrid(0, 0,        span, span / 4, 1, 1, indices, vertices);
        generateSurroundingGrid(0, span / 4, span, span / 2, 2, 2, indices, vertices);
        generateSurroundingGrid(0, span / 2, span, span,     4, 4, indices, vertices);
    }

    m_sideVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        vc71::vectorSizeIn(vertices, 16u), 16u, 8u, "TerrainEditable");
    {
        unsigned char* const dst = static_cast<unsigned char*>(dx9rend::vertexBufferLock(m_sideVertexBuffer, 0, 0, 0));
        for (unsigned int i = 0; vertices.first && i < vc71::vectorSizeIn(vertices, 16u); ++i)
            std::memcpy(dst + 16 * i, vertices.first + 16 * i, 16);
        dx9rend::vertexBufferUnlock(m_sideVertexBuffer);
    }
    m_sideVertexCount = vc71::vectorSizeIn(vertices, 16u);                    // +0xFC

    m_sideIndexBuffer = dx9rend::bufferManagerCreateIndexBuffer(vc71::vectorSizeIn(indices, 4u), 8u, 102u);
    {
        unsigned int* const dst = static_cast<unsigned int*>(dx9rend::indexBufferLock(m_sideIndexBuffer, 0, 0, 0));
        for (unsigned int i = 0; indices.first && i < vc71::vectorSizeIn(indices, 4u); ++i)
            dst[i] = reinterpret_cast<const unsigned int*>(indices.first)[i];
        dx9rend::indexBufferUnlock(m_sideIndexBuffer);
    }
    m_sideIndexCount = vc71::vectorSizeIn(indices, 4u);                       // +0xF4

    if (m_surroundingTerrainEngineMode)
    {
        if (vertices.first)
            ::renddx9abi::engineFree(vertices.first);
        vertices.first = 0; vertices.last = 0; vertices.end = 0;
        if (indices.first)
            ::renddx9abi::engineFree(indices.first);
        indices.first = 0; indices.last = 0; indices.end = 0;

        const int quarter = span / 4;
        const int half    = span / 2;
        generateSurroundingGrid(0,       0,       quarter, quarter, 1, 1, indices, vertices);
        generateSurroundingGrid(quarter, 0,       half,    quarter, 2, 2, indices, vertices);
        generateSurroundingGrid(0,       quarter, half,    half,    2, 2, indices, vertices);
        generateSurroundingGrid(half,    0,       span,    half,    4, 4, indices, vertices);
        generateSurroundingGrid(0,       half,    span,    span,    4, 4, indices, vertices);

        m_cornerVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
            vc71::vectorSizeIn(vertices, 16u), 16u, 8u, "TerrainEditable");
        {
            unsigned char* const dst = static_cast<unsigned char*>(dx9rend::vertexBufferLock(m_cornerVertexBuffer, 0, 0, 0));
            for (unsigned int i = 0; vertices.first && i < vc71::vectorSizeIn(vertices, 16u); ++i)
                std::memcpy(dst + 16 * i, vertices.first + 16 * i, 16);
            dx9rend::vertexBufferUnlock(m_cornerVertexBuffer);
        }
        m_cornerVertexCount = vc71::vectorSizeIn(vertices, 16u);              // +0x100

        m_cornerIndexBuffer = dx9rend::bufferManagerCreateIndexBuffer(vc71::vectorSizeIn(indices, 4u), 8u, 102u);
        {
            unsigned int* const dst = static_cast<unsigned int*>(dx9rend::indexBufferLock(m_cornerIndexBuffer, 0, 0, 0));
            for (unsigned int i = 0; indices.first && i < vc71::vectorSizeIn(indices, 4u); ++i)
                dst[i] = reinterpret_cast<const unsigned int*>(indices.first)[i];
            dx9rend::indexBufferUnlock(m_cornerIndexBuffer);
        }
        m_cornerIndexCount = vc71::vectorSizeIn(indices, 4u);                 // +0xF8
    }

    if (indices.first)
        ::renddx9abi::engineFree(indices.first);
    indices.first = 0; indices.last = 0; indices.end = 0;
    if (vertices.first)
        ::renddx9abi::engineFree(vertices.first);
}


// The heightmap-modified handler (IEventListener, +0x770) and its two rebuilds

// (`retn 10h`). Event 100 only: the patches and ring patches under the changed rectangle
// rebuilt, then the terrain's height range recomputed from every patch - max seeded with FLT_MIN
// (0x00800000), min with FLT_MAX. The compares are the listing's: `max < h` raises the maximum and
// `!(min < h)` lowers the minimum, so a NaN height never moves the maximum but always lands in the
// minimum.
void TerrainEditable::handleEvent(EventCategory, unsigned int id, EventNode*, void* data)
{
    if (id != kEventHeightmapModified)
        return;
    rebuildPatchesInRect(heightmapEventArea(data));
    rebuildSurroundingInRect(heightmapEventArea(data));
    m_maxHeight = FLT_MIN;
    m_minHeight = FLT_MAX;
    if (m_patchCount <= 0)
        return;
    const int stride = m_patchCount;
    for (int z = 0; z < m_patchCount; ++z)
    {
        for (int x = 0; x < m_patchCount; ++x)
        {
            const QuadPatchEditable* const patch = m_editablePatches[z * stride + x];
            if (m_maxHeight < patch->maxHeight)
                m_maxHeight = patch->maxHeight;
            if (!(m_minHeight < patch->minHeight))
                m_minHeight = patch->minHeight;
        }
    }
}

// every patch whose cell overlaps the rectangle (x0, z0, x1, z1) gets its bounds and its
// vertex buffers rebuilt. The rectangle is world metres about the terrain centre; the cell range is
// clamped to [0, patchCount - 1] and the index tested again before use.
void TerrainEditable::rebuildPatchesInRect(const float* rect)
{
    const float half = static_cast<float>(m_heightmapSizeX - 1) * m_primaryWorldScale.x * 0.5f;
    const float inv  = 1.0f / (static_cast<float>(m_patchSize) * m_primaryWorldScale.x);
    int x0 = static_cast<int>((half + rect[0]) * inv);
    if (x0 <= 0)
        x0 = 0;
    const int last = m_patchCount - 1;
    int x1 = static_cast<int>((half + rect[2]) * inv);
    if (last < x1)
        x1 = last;
    int z1 = static_cast<int>((half + rect[3]) * inv);
    if (last < z1)
        z1 = m_patchCount - 1;
    int z0 = static_cast<int>((half + rect[1]) * inv);
    if (z0 <= 0)
        z0 = 0;
    for (int z = z0; z <= z1; ++z)
    {
        for (int x = x0; x <= x1; ++x)
        {
            const int index = x + z * m_patchCount;
            if (index >= 0 && index < m_patchCount * m_patchCount)
            {
                m_editablePatches[index]->computeBounds();
                m_editablePatches[index]->rebuildVertexBuffers();
            }
        }
    }
}

// every ring patch with a corner of the rectangle strictly inside its placement.
void TerrainEditable::rebuildSurroundingInRect(const float* rect)
{
    for (int slot = 0; slot < 8; ++slot)
    {
        SurroundingPatchEditable* const patch = m_editableSurrounding[slot];
        if (!patch)
            continue;
        const float minX = static_cast<float>(patch->originX);
        const double maxX = static_cast<double>(patch->endX);
        const double minZ = static_cast<double>(patch->originZ);
        const double maxZ = static_cast<double>(patch->endZ);
        if ((rect[0] > minX && maxX > rect[0] && minZ < rect[1] && maxZ > rect[1])
            || (rect[2] > minX && maxX > rect[2] && minZ < rect[1] && maxZ > rect[1])
            || (rect[0] > minX && maxX > rect[0] && minZ < rect[3] && maxZ > rect[3])
            || (rect[2] > minX && maxX > rect[2] && minZ < rect[3] && maxZ > rect[3]))
        {
            patch->computeBounds();                                           // (R2)
            patch->buildVertexBuffers();                                      // (R2)
        }
    }
}

// setPrimaryWorldScale's follow-up: every patch's bounds.
void TerrainEditable::boundsAfterPrimaryScale()
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->computeBounds();
}


// The technique overlays (0x10107F30 .. 0x10108160): z outer, x inner, patches[z * n + x], the count
// re-read on every pass.

void TerrainEditable::createUndergrowthOverlays()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[x + z * m_patchCount]->createUndergrowthOverlay();
}

void TerrainEditable::releaseUndergrowthOverlays()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[x + z * m_patchCount]->releaseUndergrowthOverlay();
}

void TerrainEditable::createOvergrowthOverlays()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[x + z * m_patchCount]->createOvergrowthOverlay();
}

void TerrainEditable::releaseOvergrowthOverlays()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[x + z * m_patchCount]->releaseOvergrowthOverlay();
}

void TerrainEditable::createOvergrowthShadowOverlays()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[x + z * m_patchCount]->createOvergrowthShadowOverlay();
}

void TerrainEditable::releaseOvergrowthShadowOverlays()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[x + z * m_patchCount]->releaseOvergrowthShadowOverlay();
}

void TerrainEditable::createMaterialOverlays()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[x + z * m_patchCount]->createMaterialOverlay(); // (R2)
}

void TerrainEditable::releaseMaterialOverlays()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[x + z * m_patchCount]->releaseMaterialOverlay();
}


// ITerrainEditable (+0x768) - the TerrainEditable.cpp bodies (the GetSets half is in
// TerrainEditableGetSets.cpp)

void  TerrainEditable::setLodSwitchDistance(float distance) { m_lodSwitchDistance = distance; }
float TerrainEditable::getLodSwitchDistance() const { return m_lodSwitchDistance; }

// the level-wide low-detail texture: the old owner reference dropped (not nulled), the
// named file when it exists, else the common default.
void TerrainEditable::setLowDetailTexture(const TerrainEditableString& path)
{
    releaseOwnerTexture(m_lowDetailTexture);
    if (::dice::hfe::fileManager->fileExists(path))
        m_lowDetailTexture = dx9rend::textureManagerGetTexture(::renddx9abi::toVc71String(path));
    else
        m_lowDetailTexture = dx9rend::textureManagerGetTexture(vc71::string("common/terrain/textures/Default.dds"));
}

void TerrainEditable::setLowDetailmapSize(int size) { m_lowDetailmapSize = size; }
int  TerrainEditable::getLowDetailmapSize() const { return m_lowDetailmapSize; }

// Accepts layer 6 (`<= 6`), one past the six records - the shipped bound. Anything
// larger logs and answers the first record.
TerrainDetailTextureBase* TerrainEditable::getDetailTexture(unsigned int layer)
{
    if (layer <= 6)
        return reinterpret_cast<TerrainDetailTexture**>(m_detailTextures.first)[layer];
    ::renddx9abi::logf("[TerrainEditable.cpp:2041] TBM: layer index out of range!");
    return reinterpret_cast<TerrainDetailTexture**>(m_detailTextures.first)[0];
}

// (`retn 1Ch`) - the shipped body only logs (the binding still exists).
void TerrainEditable::importAllDetailLayers(TerrainEditableString)
{
    ::renddx9abi::logf("[TerrainEditable.cpp:1843] TBM: dont use this functions!");
}

// every patch's debugTest, which is an empty body (nullsub_12) in 1.50.
void TerrainEditable::debugTest(int value)
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->debugTest(value);
}

void TerrainEditable::setDrawDetailLayerAsColor(int layer) { m_drawDetailLayerAsColor = layer; }

PatchDetailLayers* TerrainEditable::getPatchMergedDetailLayers(int patchIndex) const
{
    return &m_editablePatches[patchIndex]->m_mergedLayers;
}
void* TerrainEditable::getPatchUndergrowthOverlay(int patchIndex) const
{
    return m_editablePatches[patchIndex]->m_undergrowthOverlay;
}
void* TerrainEditable::getPatchOvergrowthOverlay(int patchIndex) const
{
    return m_editablePatches[patchIndex]->m_overgrowthOverlay;
}
void* TerrainEditable::getPatchOvergrowthShadowOverlay(int patchIndex) const
{
    return m_editablePatches[patchIndex]->m_overgrowthShadowOverlay;
}

bool TerrainEditable::getSurroundingTerrainEngineMode() const { return m_surroundingTerrainEngineMode != 0; }

// 0x1010DDC0 - `jmp [vtbl+14h]`: burnAllColorMaps (E+0x14, R3).
void TerrainEditable::burnColorMaps()
{
    burnAllColorMaps();
}

// the byte is written only when it changes.
void TerrainEditable::setUseCompDetailMaps(bool use)
{
    const unsigned char value = use ? 1 : 0;
    if (value != m_useCompDetailMaps)
        m_useCompDetailMaps = value;
}
bool TerrainEditable::getUseCompDetailMaps() const { return m_useCompDetailMaps != 0; }

void TerrainEditable::setTerrainWaterColor(const Vec3& colour) { m_terrainWaterColor = colour; }
const Vec3& TerrainEditable::getTerrainWaterColor() const { return m_terrainWaterColor; }

// The nvdxt option strings (0x1010DCC0 .. 0x1010DD90): by value in, a reference out.
void TerrainEditable::setColormapSettings(TerrainEditableString value) { m_colormapSettings = value; }
const TerrainEditableString& TerrainEditable::getColormapSettings() const { return m_colormapSettings; }
void TerrainEditable::setLowDetailmapSettings(TerrainEditableString value) { m_lowDetailmapSettings = value; }
const TerrainEditableString& TerrainEditable::getLowDetailmapSettings() const { return m_lowDetailmapSettings; }
void TerrainEditable::setDetailmapSettings(TerrainEditableString value) { m_detailmapSettings = value; }
const TerrainEditableString& TerrainEditable::getDetailmapSettings() const { return m_detailmapSettings; }
void TerrainEditable::setSurroundingColormapSettings(TerrainEditableString value) { m_surroundingColormapSettings = value; }
const TerrainEditableString& TerrainEditable::getSurroundingColormapSettings() const { return m_surroundingColormapSettings; }
void TerrainEditable::setSurroundingLowDetailmapSettings(TerrainEditableString value) { m_surroundingLowDetailmapSettings = value; }
const TerrainEditableString& TerrainEditable::getSurroundingLowDetailmapSettings() const { return m_surroundingLowDetailmapSettings; }


// io::IConsoleSaveable (+0x76C), ILightSampleable (+0x774)

bool TerrainEditable::makeScript(::dice::hfe::io::IStream*) { return false; }

bool TerrainEditable::isLightSampleable() { return true; }

// release the sampler through its IBase slot +0x08 (the TerrainLightSampler's last block was written
// by its storeResults).
bool TerrainEditable::destroyLightSampler(ILightSampling* sampler)
{
    if (!sampler)
        return false;
    sampler->release();
    return true;
}


// Paints inner detail weights and matching ring colors using a quartered radius.
// Does nothing while compressed detail maps are active.
void TerrainEditable::paintDetail(float x, float z, float radius, int strength, int layer)
{
    const float reach = radius * 0.25f;
    if (m_useCompDetailMaps)
        return;
    heightmapClusterByName()->getPrimaryHeightmap();                          // +0x40, answer dropped
    const int type = static_cast<TerrainDetailTextureEditable*>(
        reinterpret_cast<TerrainDetailTexture**>(m_detailTextures.first)[layer])->lowDetailType;   // +0x74
    unsigned int colour = 0xFFu;
    if (type == 1)
        colour = 0xFFu;
    else if (type == 0)
        colour = 0u;
    else if (type == 2)
        colour = 0xFF0000u;
    for (int slot = 0; slot < 8; ++slot)
        if (m_editableSurrounding[slot])
            m_editableSurrounding[slot]->paintDetail(x, z, reach, strength, colour);
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->paintDetail(x, z, reach, strength, layer);
}

// E+0xA8 0x10107930 (`retn 14h`) - the same gate and quarter radius; the cluster's primary heightmap
// asked for and dropped; every ring patch (0x101A2E80) and patch (0x1019FF40) painted with the colour.
void TerrainEditable::paintColor(float x, float z, float radius, int strength, unsigned int color)
{
    const float reach = radius * 0.25f;
    if (m_useCompDetailMaps)
        return;
    heightmapClusterByName()->getPrimaryHeightmap();                          // +0x40, answer dropped
    for (int slot = 0; slot < 8; ++slot)
        if (m_editableSurrounding[slot])
            m_editableSurrounding[slot]->paintColor(x, z, reach, strength, color);
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->paintColor(x, z, reach, strength, color);
}

// E+0xAC 0x1010A930 (`retn 14h`) - the same gate and quarter radius, then nothing unless the layer's
// record has a colour texture (+0x64); no cluster lookup. Ring patches (0x101A5AB0), then patches

void TerrainEditable::paintColorFromTexture(float x, float z, float radius, int strength, int layer)
{
    const float reach = radius * 0.25f;
    if (m_useCompDetailMaps)
        return;
    const TerrainDetailTextureEditable* const record = static_cast<const TerrainDetailTextureEditable*>(
        reinterpret_cast<TerrainDetailTexture**>(m_detailTextures.first)[layer]);
    if (!record->colorTexture)
        return;
    for (int slot = 0; slot < 8; ++slot)
        if (m_editableSurrounding[slot])
            m_editableSurrounding[slot]->paintColorFromTexture(x, z, reach, strength, layer);
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->paintColorFromTexture(x, z, reach, strength, layer);
}

// P+0x114 0x101076E0 (`retn 10h`) - nothing while the compressed detail maps are in use. The point and
// the quartered radius into primary-heightmap units (getScale +0x54 asked three times: x, z, then x for
// the radius), the patches whose cells (ceil(radius) either side, `fidiv` by the patch size, centred
// by half the count, chopped) cover the brush, each tested 0 <= index < count^2 and handed the point
// relative to it. The per-patch body (0x1019B780) is empty in 1.50 - the walk has no effect.
void TerrainEditable::brushHeightmapPatches(float x, float z, float radius, float strength)
{
    if (m_useCompDetailMaps)
        return;
    ::dice::hfe::world::IHeightmap* const heightmap = heightmapClusterByName()->getPrimaryHeightmap();
    x = x / heightmap->getScale().x;
    z = z / heightmap->getScale().z;
    radius = radius * 0.25f / heightmap->getScale().x;
    int sizeX;
    int sizeZ;
    heightmap->getSize(sizeX, sizeZ);                                         // +0x2C
    const int count = sizeX / m_patchSize;
    const int half = count / 2;
    const float halfF = static_cast<float>(half);
    const int x0 = static_cast<int>((x - static_cast<float>(std::ceil(static_cast<double>(radius))))
                                    / static_cast<float>(m_patchSize) + halfF);
    const int x1 = static_cast<int>((static_cast<float>(std::ceil(static_cast<double>(radius))) + x)
                                    / static_cast<float>(m_patchSize) + halfF);
    const int z0 = static_cast<int>((z - static_cast<float>(std::ceil(static_cast<double>(radius))))
                                    / static_cast<float>(m_patchSize) + halfF);
    const int z1 = static_cast<int>((static_cast<float>(std::ceil(static_cast<double>(radius))) + z)
                                    / static_cast<float>(m_patchSize) + halfF);
    if (z0 > z1)
        return;
    int index = z0 * count + x0;
    int zRelative = z0 - half;
    for (int rows = z1 - z0 + 1; rows; --rows, index += count, ++zRelative)
    {
        if (x0 > x1)
            continue;
        int cell = index;
        int xRelative = x0 - half;
        for (int columns = x1 - x0 + 1; columns; --columns, ++cell, ++xRelative)
        {
            if (cell >= 0 && cell < count * count)
                m_editablePatches[cell]->brushHeightmap(
                    x - static_cast<float>(xRelative * m_patchSize),
                    z - static_cast<float>(zRelative * m_patchSize), radius, strength);
        }
    }
}

// layers and colormaps
void TerrainEditable::clearDetailLayer(int layer)                             // E+0xC0 0x10107D20
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->clearDetailLayer(layer);
}

void TerrainEditable::clearAllDetailLayers()                                  // E+0xC4 0x10107D80
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->clearAllDetailLayers();
}

void TerrainEditable::clearColorMapEdits()                                    // E+0xD4 0x10107DD0
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->clearColormapEdits();
}

// E+0xD8 every patch's edit copy loaded back (0x1019EF80), then every ring patch's
// alpha-weighted commit (0x101A4370).
void TerrainEditable::commitColorMapEdits()
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->commitColormapEdit();
    for (int slot = 0; slot < 8; ++slot)
        if (m_editableSurrounding[slot])
            m_editableSurrounding[slot]->commitColormapEdit();
}

void TerrainEditable::commitDetailLayers()                                    // E+0xC8 0x10107E90
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->commitAllLayers();
}

void TerrainEditable::revertDetailLayers()                                    // E+0xCC 0x10107EE0
{
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->revertAllLayers();
}

// the imports
// E+0xB4 0x1010A9E0 (`retn 1Ch`) - the import directory is the TGA's path without its extension
// (sub_100753D0: separators to '/', cut at the last dot); the path's directory with '/' is computed
// and never read (sub_100751A0). The directory is created (CreateDirectoryA, the answer ignored), the
// TGA split into 512 x 512 "tx" tiles inside it by tgasplit (0x1010A0B0, with "<directory>\\"), then
// every patch imports its tile (0x1019E480).
void TerrainEditable::importBaseDetailLayers(TerrainEditableString tga)
{
    const vc71::string file(::renddx9abi::toVc71String(tga));
    vc71::string directory;
    dx9rend::pathStripExtension(directory, file);
    const vc71::string unusedDirectory = pathDirectoryOf(file, '/');
    (void)unusedDirectory;
    ::CreateDirectoryA(directory.c_str(), 0);
    {
        vc71::string tileDirectory(directory);
        tileDirectory += "\\";
        tgasplit(file, tileDirectory);
    }
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->importBaseLayers(directory);
}

// E+0xB8 0x1010AAD0 (`retn 20h`) - the same split, then every patch imports the layer from its tile

void TerrainEditable::importDetailLayer(int layer, TerrainEditableString tga)
{
    const vc71::string file(::renddx9abi::toVc71String(tga));
    vc71::string directory;
    dx9rend::pathStripExtension(directory, file);
    const vc71::string unusedDirectory = pathDirectoryOf(file, '/');
    (void)unusedDirectory;
    ::CreateDirectoryA(directory.c_str(), 0);
    {
        vc71::string tileDirectory(directory);
        tileDirectory += "\\";
        tgasplit(file, tileDirectory);
    }
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->importLayerTile(layer, directory);
}

// E+0xD0 0x1010ABC0 (`retn 1Ch`) - the same split, then every patch imports its colormap tile

void TerrainEditable::importColorMap(TerrainEditableString tga)
{
    const vc71::string file(::renddx9abi::toVc71String(tga));
    vc71::string directory;
    dx9rend::pathStripExtension(directory, file);
    const vc71::string unusedDirectory = pathDirectoryOf(file, '/');
    (void)unusedDirectory;
    ::CreateDirectoryA(directory.c_str(), 0);
    {
        vc71::string tileDirectory(directory);
        tileDirectory += "\\";
        tgasplit(file, tileDirectory);
    }
    for (int z = 0; z < m_patchCount; ++z)
        for (int x = 0; x < m_patchCount; ++x)
            m_editablePatches[m_patchCount * z + x]->importColormapTile(directory);
}

// the external tools
// (`retn 38h`) - "<executable> <arguments>" run by CreateProcessA with no window
// (CREATE_NO_WINDOW, nothing inherited, the caller's directory), then polled: while it is still active
// the Esc key (GetAsyncKeyState(VK_ESCAPE), the sign bit) terminates it with 0x80000003, else a 5 ms
// wait. Both handles are closed. On a failed start the error is formatted (FormatMessageA, else the
// number) into an 80-byte buffer by sprintf - unbounded, as shipped - and never reported.
void TerrainEditable::runTool(vc71::string executable, vc71::string arguments)
{
    STARTUPINFOA startup;
    std::memset(&startup, 0, sizeof(startup));
    PROCESS_INFORMATION process;
    std::memset(&process, 0, sizeof(process));
    startup.cb = 68;
    vc71::string command;
    formatString(command, "%s %s", executable.c_str(), arguments.c_str());
    if (::CreateProcessA(0, const_cast<char*>(command.c_str()), 0, 0, FALSE, 0x08000000u, 0, 0,
                         &startup, &process))
    {
        DWORD exitCode = 0;
        ::GetExitCodeProcess(process.hProcess, &exitCode);
        while (exitCode == 259u)                                              // STILL_ACTIVE
        {
            if (static_cast<unsigned int>(static_cast<int>(::GetAsyncKeyState(27))) >> 16)
            {
                exitCode = 0;
                ::TerminateProcess(process.hProcess, 0x80000003u);
                ::CloseHandle(process.hProcess);
                ::CloseHandle(process.hThread);
                return;
            }
            ::WaitForSingleObject(process.hProcess, 5);
            ::GetExitCodeProcess(process.hProcess, &exitCode);
        }
        ::CloseHandle(process.hProcess);
        ::CloseHandle(process.hThread);
        return;
    }
    const DWORD error = ::GetLastError();
    char message[80];
    LPSTR text = 0;
    if (::FormatMessageA(0x1300u, 0, ::GetLastError(), 0x400u, reinterpret_cast<LPSTR>(&text), 0, 0))
    {
        std::sprintf(message, "GetLastError returned %s\n", text);
        ::LocalFree(text);
    }
    else
    {
        std::sprintf(message, "GetLastError returned %u\n", static_cast<unsigned int>(error));
    }
}

// (`retn 38h`) - "tgasplit <tga> -w512 -h512 -b -s -R0 -G0 -B0 <outputDirectory> tx".
void TerrainEditable::tgasplit(vc71::string tga, vc71::string outputDirectory)
{
    vc71::string command;
    formatString(command, "%s -w512 -h512 -b -s -R0 -G0 -B0 %s tx", tga.c_str(), outputDirectory.c_str());
    runTool(vc71::string("tgasplit"), command);
}

// E+0xF4 0x1010A130 (`retn 54h`) - "nvdxt -file <file> <options> -outdir <outputDirectory>".
void TerrainEditable::runNvdxt(TerrainEditableString file, TerrainEditableString outputDirectory,
                               TerrainEditableString options)
{
    vc71::string command;
    {
        vc71::string formatted;
        formatString(formatted, "-file %s %s -outdir %s",
                     ::renddx9abi::toVc71String(file).c_str(),
                     ::renddx9abi::toVc71String(options).c_str(),
                     ::renddx9abi::toVc71String(outputDirectory).c_str());
        command = formatted;
    }
    runTool(vc71::string("nvdxt"), command);
}

// The converters (`retn 38h`): E+0xF4 (virtual) with the file, the directory and their option string.
void TerrainEditable::convertToDxt1(TerrainEditableString file, TerrainEditableString outputDirectory)       // E+0xF8 0x10108210
{
    runNvdxt(file, outputDirectory, TerrainEditableString("-dxt1c"));
}
void TerrainEditable::makeUncompDDS(TerrainEditableString file, TerrainEditableString outputDirectory)       // E+0xFC 0x10108270
{
    runNvdxt(file, outputDirectory, TerrainEditableString("-u8888"));
}
// E+0x100 the slot 1.50 inserted: the same body as E+0xFC (the same "-u8888", 0x1021B76C).
void TerrainEditable::makeUncompDDSAgain(TerrainEditableString file, TerrainEditableString outputDirectory)
{
    runNvdxt(file, outputDirectory, TerrainEditableString("-u8888"));
}
void TerrainEditable::convertToU565(TerrainEditableString file, TerrainEditableString outputDirectory)       // E+0x104 0x10108330
{
    runNvdxt(file, outputDirectory, TerrainEditableString("-u565"));
}

// the surrounding patches
// E+0x10C 0x101084F0 (`retn 4`). The guard is `slot >= 0 || slot <= 7` - always true, so the slot is
// not bounded (shipped). An empty slot gets a 0x2FC-byte ring patch and its init(slot) - init runs
// on a failed allocation's null, as shipped.
void TerrainEditable::createSurroundingPatch(int slot)
{
    if (slot >= 0 || slot <= 7)
    {
        if (!m_editableSurrounding[slot])
        {
            void* const storage = ::renddx9abi::engineAlloc(0x2FC);
            SurroundingPatchEditable* const patch = storage ? new (storage) SurroundingPatchEditable(this) : 0;
            patch->init(slot);
            m_editableSurrounding[slot] = patch;
        }
    }
}

// E+0x108 0x10108560 (`retn 4`) - the same (unbounded) guard; a ring patch is destroyed through its
// slot 0 with 1 (the body, then the engine heap's free) and the slot nulled.
void TerrainEditable::removeSurroundingPatch(int slot)
{
    if (slot >= 0 || slot <= 7)
    {
        SurroundingPatch* const ring = m_editableSurrounding[slot];
        if (ring)
        {
            ring->~SurroundingPatch();                                        // virtual: ~SurroundingPatchEditable
            ::renddx9abi::engineFree(ring);
            m_editableSurrounding[slot] = 0;
        }
    }
}

void TerrainEditable::createSurroundingAllPatchs()                            // E+0x114 0x10108590
{
    for (int slot = 0; slot < 8; ++slot)
        createSurroundingPatch(slot);                                         // E+0x10C (virtual)
}

void TerrainEditable::removeSurroundingAllPatchs()                            // E+0x110 0x101085B0
{
    for (int slot = 0; slot < 8; ++slot)
        removeSurroundingPatch(slot);                                         // E+0x108 (virtual)
}

// E+0x118 0x101085D0 (`retn 4`) - only on a change: the byte stored, the ring's shared geometry rebuilt
// (primary +0xCC), then every ring patch re-placed (engine mode: SurroundingPatch's which
// does not set the +0x48 step count the vertex writer reads; editor mode: 0x101A2610), its bounds and
// buffers rebuilt and its textures re-oriented (0x101A4FF0).
void TerrainEditable::setSurroundingTerrainEngineMode(bool engineMode)
{
    const unsigned char value = engineMode ? 1 : 0;
    if (m_surroundingTerrainEngineMode == value)
        return;
    m_surroundingTerrainEngineMode = value;
    buildSurroundingGeometry();                                               // primary +0xCC
    for (int slot = 0; slot < 8; ++slot)
    {
        SurroundingPatchEditable* const patch = m_editableSurrounding[slot];
        if (!patch)
            continue;
        if (m_surroundingTerrainEngineMode)
            patch->setupPlacement();
        else
            patch->setupEditorPlacement();
        patch->computeBounds();
        patch->buildVertexBuffers();
        patch->onModeSwitch();
    }
}

// E+0x120 every patch's bounds, vertex buffers and lightmap (0x1019E160 / 0x1019DF30 /
// 0x1019F350), then every ring patch's (0x101A4150 / 0x101A3D70 / 0x101A4520).
void TerrainEditable::rebuildTerrain()
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
    {
        m_editablePatches[i]->computeBounds();
        m_editablePatches[i]->rebuildVertexBuffers();
        m_editablePatches[i]->reloadLightmap();
    }
    for (int slot = 0; slot < 8; ++slot)
    {
        if (m_editableSurrounding[slot])
        {
            m_editableSurrounding[slot]->computeBounds();
            m_editableSurrounding[slot]->buildVertexBuffers();
            m_editableSurrounding[slot]->reloadLightmap();
        }
    }
}

// the material map and the road passes
// E+0x20 every patch's material map (0x101A0F30), then the road masks at the patch size
// again (each of which also writes the road materials and leaves its mask alive).
void TerrainEditable::buildMaterialMap()
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->buildMaterialMap();
    renderRoadMasks(m_patchSize);
}

// (`retn 4`) - every patch's road mask (0x101A0680) at `size`.
void TerrainEditable::renderRoadMasks(int size)
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->renderRoadMask(size);
}

// P+0xE4 every patch's roads burnt into the undergrowth (0x101A0C20).
void TerrainEditable::burnRoadsInUndergrowth()
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->burnRoadsInUndergrowth();
}

// P+0xE8 the same, for the patches whose square (origin +0x78 / +0x7C, side
// (int)(patchSize * scale.x)) contains the camera strictly (each compare NaN-rejecting). The camera is
// the duplicate view's (Renderer +0x50) matrix row 3, packed with w = 1 (sub_1008DD80).
void TerrainEditable::burnRoadsInUndergrowthNeerest()
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();   // Renderer +0x50
    const float* const packed = staticMeshPackVec4(&view->getViewMatrixPtr()->m[12], 1.0f);
    const float cameraX = packed[0];
    const float cameraZ = packed[2];
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
    {
        QuadPatchEditable* const patch = m_editablePatches[i];
        const float dx = cameraX - static_cast<float>(patch->scaledX);
        const float dz = cameraZ - static_cast<float>(patch->scaledZ);
        const int side = static_cast<int>(static_cast<float>(m_patchSize) * m_primaryWorldScale.x);
        const float sideF = static_cast<float>(side);
        if (dx > 0.0f && dx < sideF && dz > 0.0f && dz < sideF)
            patch->burnRoadsInUndergrowth();
    }
}

// Draws queued terrain and ring patches for lightmap generation in editor mode.
// Requires an effect with the lightmapGeneration technique; restores the state block.
void TerrainEditable::renderForLightmapGeneration()
{
    if (m_surroundingTerrainEngineMode)
        return;
    update();                                                                 // primary +0x28
    dx9rend::stateManagerPush();
    dx9rend::effectSetTechniqueByName(m_effect, vc71::string("lightmapGeneration"));
    dx9rend::effectBegin(m_effect);
    Mat4 viewProj = Mat4::getIdentity();
    {
        dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();  // Renderer +0x48
        const Mat4* const projection = view->getProjectionMatrixPtr();        // +0x6C
        const Mat4* const inverseView = view->getInverseViewMatrixPtr();      // +0x68
        viewProj.fullMult(*inverseView, *projection);
    }
    param(m_hMatViewProj)->setValue(&viewProj, 0);                            // +0x1D0
    if (m_drawMain)                                                           // +0x358
    {
        dx9rend::effectBeginPass(m_effect, 0);
        float scaleTransY[4] = { m_primaryWorldScale.y, 1.0f, 0.0f, 0.0f };
        param(m_hScaleTransY)->setValue(scaleTransY, 0);                      // +0x1BC
        dx9rend::vertexBufferBindStream(m_baseGridVertexBuffer, 0);          // +0xD0
        dx9rend::vertexDeclarationBind(m_surroundingDeclaration);            // +0x2A4
        drawPatchQueueForLightmapGeneration(m_map2B0);
    }
    if (m_flag359)                                                            // +0x359
    {
        dx9rend::effectBeginPass(m_effect, 1);
        float scaleTransY[4] = { m_secondaryWorldScale.y, 1.0f, 0.0f, 0.0f };
        param(m_hScaleTransY)->setValue(scaleTransY, 0);
        if (m_sideVertexBuffer && m_sideIndexBuffer)                          // +0xE0 / +0xE8
        {
            dx9rend::vertexBufferBindStream(m_sideVertexBuffer, 0);
            dx9rend::indexBufferBind(m_sideIndexBuffer);
            for (int slot = 0; slot < 8; ++slot)
                if (m_editableSurrounding[slot])
                    m_editableSurrounding[slot]->drawForLightmapGeneration();
        }
    }
    dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerPop();
}


// R3 - saving, read in RendDX9.dll 1.50 ("RendDX9.dll (2).i64"; Hex-Rays for the shape,
// the listing for every float compare, push order and receiver). Every path is
// getLevelSavePath() ("<Editor_LevelSavePath>/") + a level-relative name; nothing here creates a
// folder, and only the Terrain.con write reports a failure.


// E+0x14 every patch's colormap burnt (0x1019C630), then every ring patch's (0x101A5060),
// each handed a copy of the colormap base name (E+0x6C) - which the burns read only on their
// not-edited arm. burnColorMaps (E+0x124) jumps here.
void TerrainEditable::burnAllColorMaps()
{
    const vc71::string colormapBase(::renddx9abi::toVc71String(getColormapBaseName()));   // E+0x6C
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->burnColormap(colormapBase);
    for (int slot = 0; slot < 8; ++slot)
        if (m_editableSurrounding[slot])
            m_editableSurrounding[slot]->burnColormap(colormapBase);
}

// E+0x18 every patch's edited colormap committed and saved (0x1019FE30), then every ring
// patch's (0x101A4CC0).
void TerrainEditable::saveEditedColorMaps()
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->saveEditedColormap();
    for (int slot = 0; slot < 8; ++slot)
        if (m_editableSurrounding[slot])
            m_editableSurrounding[slot]->saveEditedColormap();
}

// E+0x1C every patch's lightmap saved under the plain lightmap base (E+0x74, copied once;
// 0x1019D210). The ring patches' lightmaps are not saved.
void TerrainEditable::saveLightmaps()
{
    const vc71::string lightmapBase(::renddx9abi::toVc71String(getLightmapBaseName()));   // E+0x74
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->saveLightmap(lightmapBase);
}

// E+0x130 0x101086E0 / E+0x134 every patch's compressed detail maps made (0x101A0310) or
// removed (0x1019F120).
void TerrainEditable::makeCompDetailMaps()
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->makeCompDetailMaps();
}

void TerrainEditable::removeCompDetailMaps()
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        m_editablePatches[i]->removeCompDetailMaps();
}

// (`retn 4`) - save()'s flag-2 arm: every patch's two detail maps saved with an empty
// suffix (0x1019D090, a fresh "" each time; the argument is never read), the material map rebuilt
// (E+0x20 - it writes the primary heightmap's material grid), then every ring patch's low-detail
// map saved (0x101A2A90).
void TerrainEditable::saveEditorMaps(const vc71::string&)
{
    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
    {
        const vc71::string empty("");
        m_editablePatches[i]->saveDetailMaps(empty);
    }
    buildMaterialMap();                                                       // E+0x20 (virtual)
    for (int slot = 0; slot < 8; ++slot)
        if (m_editableSurrounding[slot])
            m_editableSurrounding[slot]->saveLowDetailMap();
}

// Saves selected terrain data: 4 raw data, 1 Terrain.con, 2 editor maps, 8 colormaps;
// zero selects all. Raw-data flag 0x10 controls burns; 0x20 is unused.
// Returns false only for a short Terrain.con write. The destination folder must exist.
// [CHOSEN] Native stream formatting may use two exponent digits; both forms are loadable.
bool TerrainEditable::save(const TerrainEditableString& path, int flags)
{
    bool written = true;
    vc71::string folder(::renddx9abi::toVc71String(path));
    if (folder.size())
    {
        const char last = folder.c_str()[folder.size() - 1];
        if (last != '/' && last != '\\')
            folder += "/";
    }

    if ((flags & 4) || !flags)
        writeTerrainData(folder, flags);

    if ((flags & 1) || !flags)
    {
        vc71::string conPath(::renddx9abi::toVc71String(getLevelSavePath())); // E+0x90
        conPath += folder;
        conPath += "Terrain.con";

        std::ostringstream text;
        text << "if v_arg1 == BF2Editor" << std::endl;
        text << "terrain.create TerrainEditable" << std::endl;
        text << "terrain.patchSize " << getPatchSize() << std::endl;                       // E+0x4C
        text << "terrain.subdividePatches " << getSubdividePatches() << std::endl;         // E+0x54, 0 / 1
        {
            const Vec3& scale = getPrimaryWorldScale();                                     // E+0x34
            text << "terrain.primaryWorldScale " << scale.x << '/' << scale.y << '/' << scale.z << std::endl;
        }
        {
            const Vec3& scale = getSecondaryWorldScale();                                   // E+0x3C
            text << "terrain.secondaryWorldScale " << scale.x << '/' << scale.y << '/' << scale.z << std::endl;
        }
        text << "terrain.patchColormapSize " << getPatchColormapSize() << std::endl;       // E+0x5C
        text << "terrain.lowDetailmapSize " << getLowDetailmapSize() << std::endl;         // E+0x64
        {
            TerrainEditableNameResult name = getDetailmapBaseName();                       // E+0x7C
            text << "terrain.detailmapBaseName \"";
            text.write(name.c_str(), static_cast<std::streamsize>(name.size()));
            text << "\"" << std::endl;
        }
        {
            TerrainEditableNameResult name = getLowDetailmapBaseName();                    // E+0x84
            text << "terrain.lowDetailmapBaseName \"";
            text.write(name.c_str(), static_cast<std::streamsize>(name.size()));
            text << "\"" << std::endl;
        }
        {
            TerrainEditableNameResult name = getColormapBaseName();                        // E+0x6C
            text << "terrain.colormapBaseName \"";
            text.write(name.c_str(), static_cast<std::streamsize>(name.size()));
            text << "\"" << std::endl;
        }
        {
            TerrainEditableNameResult name = getLightmapBaseName();                        // E+0x74
            text << "terrain.lightmapBaseName \"";
            text.write(name.c_str(), static_cast<std::streamsize>(name.size()));
            text << "\"" << std::endl;
        }
        {
            Vec2 storage;
            const Vec2* const tiling = getFarSideTiling(&storage);                          // P+0x54
            text << "terrain.farSideTiling " << tiling->x << '/' << tiling->y << std::endl;
        }
        text << "terrain.farTopTilingHi " << getFarTopTilingHi() << std::endl;             // P+0x5C
        text << "terrain.farTopTilingLow " << getFarTopTilingLow() << std::endl;           // P+0x64
        text << "terrain.farYOffset " << getFarYOffset() << std::endl;                     // P+0x6C
        {
            const Vec3& colour = getTerrainWaterColor();                                    // E+0x13C
            text << "terrain.terrainWaterColor " << colour.x << '/' << colour.y << '/' << colour.z << std::endl;
        }
        text << "terrain.init" << std::endl;
        text << std::endl;
        text << "else" << std::endl;
        text << "terrain.create Terrain" << std::endl;
        text << "terrain.load ";
        text.write(path.c_str(), static_cast<std::streamsize>(path.size()));
        text << "/terraindata.raw" << std::endl;
        text << "endIf" << std::endl;
        text << std::endl;

        ::renddx9abi::HostStream* const stream = ::dice::hfe::fileManager->open(
            ::renddx9abi::makeHostString(conPath), static_cast< ::renddx9abi::HostFileMode>(0),
            static_cast< ::renddx9abi::HostFileAccess>(2), 0);               // FileManager +0x18
        const std::string body = text.str();
        written = stream->write(body.c_str(), static_cast<unsigned int>(body.size()))
                  == static_cast<unsigned int>(body.size());                  // +0x14, unguarded
        stream->release();                                                    // +0x08
    }

    if ((flags & 2) || !flags)
    {
        const vc71::string empty("");
        saveEditorMaps(empty);
    }
    if ((flags & 8) || !flags)
        saveEditedColorMaps();                                                // E+0x18 (virtual)
    return written;
}

namespace {

// The lock / count / stride / write / unlock the raw writers inline for every vertex buffer they copy
// (0x1010CCB3.., 0x1010CDAE.., 0x1010CDE9.., 0x10108DBC..): the buffer locked whole, count (0x10014710)
// asked before the stride (0x10014700), count * stride raw bytes written, then unlocked. Unguarded.
void writeVertexBufferBytes(TerrainRawReader& raw, void* vertexBuffer)
{
    void* const bits = dx9rend::vertexBufferLock(vertexBuffer, 0, 0, 0);
    const unsigned int count = dx9rend::vertexBufferVertexCount(vertexBuffer);
    const unsigned int stride = dx9rend::vertexBufferStride(vertexBuffer);
    raw.write(bits, count * stride);
    dx9rend::vertexBufferUnlock(vertexBuffer);
}

} // anonymous namespace

// Writes version-0x1A terraindata.raw beneath the level save path; logs open failures.
// This compatibility writer omits blocks required by the 1.50 loader and its output
// is not loadable by that reader. Ring patches temporarily switch to engine mode.
void TerrainEditable::writeTerrainData(const vc71::string& folder, int flags)
{
    ITerrainEditable* const editable = static_cast<ITerrainEditable*>(this);   // the +0x768 subobject
    vc71::string name(::renddx9abi::toVc71String(getLevelSavePath()));        // E+0x90
    name += folder;
    name += "terraindata.raw";
    TerrainRawReader raw;
    raw.open(name, 0, 2, 0);
    if (!raw.isOpen())
    {
        ::renddx9abi::logf("[TerrainEditable.cpp:434] Failed to open file terraindata.raw!");
        return;
    }

    raw.writeU32(0x1001Au);
    raw.writeVec3(&m_primaryWorldScale);                                      // +0x14
    raw.writeVec3(&m_secondaryWorldScale);                                    // +0x20
    raw.writeU32(static_cast<unsigned int>(m_fileWord2C));                    // +0x2C through the f32 twin (the raw word)
    {
        float maxHeight = FLT_MIN;
        for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        {
            const float height = getPatchField98(i);                          // P+0x40
            if (!(height < maxHeight))
                maxHeight = height;
        }
        raw.writeF32(maxHeight);
        float minHeight = FLT_MAX;
        for (int i = 0; i < m_patchCount * m_patchCount; ++i)
        {
            const float height = getPatchField9C(i);                          // P+0x3C
            if (height < minHeight)
                minHeight = height;
        }
        raw.writeF32(minHeight);
    }
    raw.writeU32(static_cast<unsigned int>(m_patchSize));                     // +0x38
    raw.writeByte(m_subdividePatches);                                        // +0x3C, sub_10076240
    raw.writeU32(static_cast<unsigned int>(m_patchCount));                    // +0x40
    raw.writeU32(static_cast<unsigned int>(m_patchDetailmapSize));            // +0x44
    raw.writeU32(static_cast<unsigned int>(m_patchColormapSize));             // +0x48
    {
        vc71::string line(m_colormapBaseName);                                // +0x54
        line += "\n";
        raw.writeString(line);
    }
    {
        vc71::string line(m_detailmapBaseName);                               // +0x8C
        line += "\n";
        raw.writeString(line);
    }
    {
        vc71::string line(m_lowDetailmapBaseName);                            // +0xA8
        line += "\n";
        raw.writeString(line);
    }
    {
        vc71::string line(m_lightmapBaseName);                                // +0x70
        line += "\n";
        raw.writeString(line);
    }
    raw.writeVec2(&m_farSideTiling);                                          // +0x324
    raw.writeF32(m_farTopTilingHi);                                           // +0x330
    raw.writeF32(m_farTopTilingLow);                                          // +0x32C
    raw.writeF32(m_farYOffset);                                               // +0x334
    {
        Vec3 sun;
        sun.x = m_sunColor.x * 4.0f;                                          // flt_101E0630
        sun.y = m_sunColor.y * 4.0f;
        sun.z = m_sunColor.z * 4.0f;
        raw.writeVec3(&sun);
    }
    {
        Vec3 gi;
        gi.x = m_giColor.x + m_giColor.x;
        gi.y = m_giColor.y + m_giColor.y;
        gi.z = m_giColor.z + m_giColor.z;
        raw.writeVec3(&gi);
    }
    raw.writeVec3(&m_terrainWaterColor);                                      // +0xC4
    raw.writeU32(6u);
    for (int i = 0; i < 6; ++i)
    {
        const TerrainDetailTextureEditable* const record = static_cast<const TerrainDetailTextureEditable*>(
            reinterpret_cast<TerrainDetailTexture**>(m_detailTextures.first)[i]);
        vc71::string line(record->name);                                      // +0x2C
        line += "\n";
        raw.writeString(line);
        raw.writeByte(record->planeMap);                                      // +0x15, sub_100761C0
        raw.writeVec2(&record->sideTiling);                                   // +0x1C
        raw.writeF32(record->topTiling);                                      // +0x18
        raw.writeF32(record->yOffset);                                        // +0x24
        raw.writeByte(record->envMap);                                        // +0x28, sub_10076240
    }

    for (int i = 0; i < m_patchCount * m_patchCount; ++i)
    {
        QuadPatchEditable* const patch = m_editablePatches[i];
        raw.writeU32(static_cast<unsigned int>(patch->gridX));                // +0x68
        raw.writeU32(static_cast<unsigned int>(patch->gridZ));                // +0x6C
        raw.writeF32(getPatchField98(i));                                     // P+0x40
        raw.writeF32(getPatchField9C(i));                                     // P+0x3C
        raw.writeVec3(&patch->sphereCentre);                                  // +0xA0
        raw.writeF32(patch->sphereRadius);                                    // +0xAC
        writePatchHeights(raw, patch);
        writeVertexBufferBytes(raw, patch->normalVertexBuffer);               // +0x5C
        writePatchLayersAndCompile(raw, patch);
        if ((flags & 0x10) || !flags)
            patch->burnColormap(getColormapDirectory());                      // P+0xEC, 0x1019C630
    }

    const unsigned char savedEngineMode = m_surroundingTerrainEngineMode;     // +0x7BC
    for (int slot = 0; slot < 8; ++slot)
    {
        if (!m_editableSurrounding[slot])
            continue;
        editable->setSurroundingTerrainEngineMode(true);                      // E+0x118
        SurroundingPatchEditable* const ring = m_editableSurrounding[slot];
        raw.writeU32(static_cast<unsigned int>(ring->slot));                  // +0x84
        raw.writeF32(ring->fileMaxY);                                         // +0x68
        raw.writeF32(ring->fileMinY);                                         // +0x6C
        raw.writeVec3(&ring->boundsCenter);                                   // +0x70
        raw.writeF32(ring->boundsRadius);                                     // +0x7C
        writeVertexBufferBytes(raw, ring->xzVertexBuffer);                    // +0x1C - the FLOAT1 heights
        writeVertexBufferBytes(raw, ring->heightVertexBuffer);                // +0x20 - the normals
        ring->compileLowDetailMap();
        if ((flags & 0x10) || !flags)
            ring->burnColormap(getColormapDirectory());                       // P+0xEC, 0x101A5060
    }
    editable->setSurroundingTerrainEngineMode(savedEngineMode != 0);
    raw.writeU32(0xFFFFFFFFu);
    raw.close();
}

// Writes patch heights and four LOD morph-delta channels with per-channel bounds.
// A zero neighbor average leaves delta zero. Min/max comparisons preserve their distinct
// NaN behavior; the temporary delta allocation is not checked.
void TerrainEditable::writePatchHeights(TerrainRawReader& raw, QuadPatchEditable* patch)
{
    writeVertexBufferBytes(raw, patch->heightVertexBuffer);                   // +0x54
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const float scaleX = m_primaryWorldScale.x;                               // +0x14
    const float scaleY = m_primaryWorldScale.y;                               // +0x18
    float minDelta[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
    float maxDelta[4] = { FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN };
    const unsigned int count = dx9rend::vertexBufferVertexCount(patch->heightVertexBuffer);
    float* const deltas = static_cast<float*>(::renddx9abi::engineAlloc(count << 4));   // operator new[] (sub_1005F340)
    if (deltas)
    {
        for (int k = 0; k < static_cast<int>(count); ++k)                     // the inlined Vec4 constructors: zero
        {
            deltas[4 * k + 0] = 0.0f;
            deltas[4 * k + 1] = 0.0f;
            deltas[4 * k + 2] = 0.0f;
            deltas[4 * k + 3] = 0.0f;
        }
    }

    for (int lod = 0; lod < 4; ++lod)
    {
        const unsigned int step = 1u << (lod + 1);
        const unsigned int half = step >> 1;
        const int halfI = static_cast<int>(half);
        for (int z = 0; z < m_patchSize + 1; ++z)
        {
            const unsigned int zPhase = static_cast<unsigned int>(z) % step;
            for (int x = 0; x < m_patchSize + 1; ++x)
            {
                float* const cell = deltas + 4 * ((m_patchSize + 1) * z + x);
                const int sampleX = patch->worldX + x;                         // +0x70
                const int sampleZ = patch->worldZ + z;                         // +0x74
                const unsigned int xPhase = static_cast<unsigned int>(x) % step;
                float average = 0.0f;
                if (xPhase == half)
                {
                    if (zPhase == 0)
                    {
                        const float worldZ = static_cast<float>(sampleZ) * scaleX;
                        const float inverse = 1.0f / scaleY;
                        const float h1 = cluster->getHeightInWorldCoords(static_cast<float>(sampleX - halfI) * scaleX, worldZ);
                        const float h2 = cluster->getHeightInWorldCoords(static_cast<float>(sampleX + halfI) * scaleX, worldZ);
                        average = (h2 + h1) * inverse;
                    }
                    else if (zPhase == half)
                    {
                        const float inverse = 1.0f / scaleY;
                        const float h1 = cluster->getHeightInWorldCoords(static_cast<float>(sampleX + halfI) * scaleX,
                                                                         static_cast<float>(sampleZ - halfI) * scaleX);
                        const float h2 = cluster->getHeightInWorldCoords(static_cast<float>(sampleX - halfI) * scaleX,
                                                                         static_cast<float>(sampleZ + halfI) * scaleX);
                        average = (h2 + h1) * inverse;
                    }
                }
                else if (zPhase == half && xPhase == 0)
                {
                    const float worldX = static_cast<float>(sampleX) * scaleX;
                    const float inverse = 1.0f / scaleY;
                    const float h1 = cluster->getHeightInWorldCoords(worldX, static_cast<float>(sampleZ - halfI) * scaleX);
                    const float h2 = cluster->getHeightInWorldCoords(worldX, static_cast<float>(sampleZ + halfI) * scaleX);
                    average = (h2 + h1) * inverse;
                }
                average = average * 0.5f;                                      // flt_101DD4F0
                float delta;
                if (average == 0.0f)
                    delta = average;
                else
                    delta = cluster->getHeightInWorldCoords(static_cast<float>(sampleX) * scaleX,
                                                            static_cast<float>(sampleZ) * scaleX) / scaleY - average;
                cell[lod] = delta;
                if (!(minDelta[lod] < delta))
                    minDelta[lod] = delta;
                if (maxDelta[lod] < delta)
                    maxDelta[lod] = delta;
            }
        }
    }

    raw.writeVec4(minDelta);
    raw.writeVec4(maxDelta);
    raw.write(deltas, dx9rend::vertexBufferVertexCount(patch->heightVertexBuffer) << 4);
    ::renddx9abi::engineFree(deltas);                                         // operator delete[] (sub_1005F370)
}

// Writes patch layer indices and compiles detail/low-detail textures.
// Import/update layers first: empty layers delete compiled maps without replacing them.
// Missing nvdxt is not detected by this compatibility path.
void TerrainEditable::writePatchLayersAndCompile(TerrainRawReader& raw, QuadPatchEditable* patch)
{
    ::dice::hfe::world::IHeightmapCluster* const cluster = heightmapClusterByName();
    const float seaLevel = cluster->getSeaWaterLevel();                       // +0x60
    const float scaleX = getPrimaryWorldScale().x;                            // E+0x34
    int layerIndexCount[6];
    for (int layer = 0; layer < 6; ++layer)
    {
        std::vector<unsigned short> indices;
        for (int z = 0; z < m_patchSize; ++z)
        {
            for (int x = 0; x < m_patchSize; ++x)
            {
                if (!patch->m_mergedLayers.getLayer(layer)->anyWeight(x, z))
                    continue;
                const float worldX = static_cast<float>(patch->gridX * m_patchSize + x) * scaleX;
                const float worldZ = static_cast<float>(patch->gridZ * m_patchSize + z) * scaleX;
                const float threshold = seaLevel - 3.0f;                       // flt_1020CE50
                const bool belowA = cluster->getHeightInWorldCoords(worldX, worldZ) < threshold;
                const float worldX1 = worldX + scaleX;
                const bool belowB = cluster->getHeightInWorldCoords(worldX1, worldZ) < threshold;
                const float worldZ1 = worldZ + scaleX;
                const bool belowC = cluster->getHeightInWorldCoords(worldX, worldZ1) < threshold;
                const bool belowD = cluster->getHeightInWorldCoords(worldX1, worldZ1) < threshold;
                if (!belowA || !belowB || !belowC)
                {
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * z + x));
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * z + x + 1));
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * (z + 1) + x));
                }
                if (!belowC || !belowB || !belowD)
                {
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * (z + 1) + x));
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * z + x + 1));
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * (z + 1) + x + 1));
                }
            }
        }
        const unsigned int count = static_cast<unsigned int>(indices.size());
        layerIndexCount[layer] = static_cast<int>(count);
        if (count)
        {
            raw.writeU32(0xEEEEEEEEu);
            raw.writeU32(static_cast<unsigned int>(layer));
            const Vec3 zero = { 0.0f, 0.0f, 0.0f };
            raw.writeVec3(&zero);
            raw.writeF32(0.0f);
            raw.writeU32(count);
            raw.writeU32(count / 3u);
            raw.writeU32(4u);
            raw.write(&indices[0], 2 * count);
        }
    }
    {
        std::vector<unsigned short> indices;
        for (int z = 0; z < m_patchSize; ++z)
        {
            for (int x = 0; x < m_patchSize; ++x)
            {
                const float worldX = static_cast<float>(patch->gridX * m_patchSize + x) * scaleX;
                const float worldZ = static_cast<float>(patch->gridZ * m_patchSize + z) * scaleX;
                const bool belowA = cluster->getHeightInWorldCoords(worldX, worldZ) < seaLevel;
                const float worldX1 = worldX + scaleX;
                const bool belowB = cluster->getHeightInWorldCoords(worldX1, worldZ) < seaLevel;
                const float worldZ1 = worldZ + scaleX;
                const bool belowC = cluster->getHeightInWorldCoords(worldX, worldZ1) < seaLevel;
                const bool belowD = cluster->getHeightInWorldCoords(worldX1, worldZ1) < seaLevel;
                if (belowA || belowB || belowC)
                {
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * z + x));
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * z + x + 1));
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * (z + 1) + x));
                }
                if (belowC || belowB || belowD)
                {
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * (z + 1) + x));
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * z + x + 1));
                    indices.push_back(static_cast<unsigned short>((m_patchSize + 1) * (z + 1) + x + 1));
                }
            }
        }
        const unsigned int count = static_cast<unsigned int>(indices.size());
        if (count)
        {
            raw.writeU32(0xEEEEEEEEu);
            raw.writeU32(6u);
            const Vec3 zero = { 0.0f, 0.0f, 0.0f };
            raw.writeVec3(&zero);
            raw.writeF32(0.0f);
            raw.writeU32(count);
            raw.writeU32(count / 3u);
            raw.writeU32(4u);
            raw.write(&indices[0], 2 * count);
        }
    }
    raw.writeU32(0xFFFFFFFFu);

    const int half = m_patchCount / 2;
    const int fileX = patch->gridX + half;
    const int fileZ = patch->gridZ + half;
    unsigned char hasNoDetail = 1;
    for (int map = 0; map < 2; ++map)
    {
        vc71::string name;
        formatString(name, "%02ix%02i_%i", fileX, fileZ, map + 1);
        {
            const vc71::string empty("");
            patch->saveDetailMaps(empty);
        }
        {
            TerrainEditableNameResult base = getDetailmapBaseName();          // E+0x7C
            vc71::string compiled(::renddx9abi::toVc71String(getLevelSavePath()));
            compiled += ::renddx9abi::toVc71String(base);
            compiled += name;
            compiled += ".dds";
            ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(compiled));   // +0x20
        }
        if (!layerIndexCount[3 * map] && !layerIndexCount[3 * map + 1] && !layerIndexCount[3 * map + 2])
            continue;
        hasNoDetail = 0;
        {
            // The argument blocks are built right to left: the options, the directory, the file.
            const TerrainEditableString options(m_detailmapSettings);         // +0x800
            vc71::string outputDirectory;
            {
                const vc71::string directory = getDetailmapDirectory();       // P+0xF0
                outputDirectory = ::renddx9abi::toVc71String(getLevelSavePath());
                outputDirectory += directory;
            }
            vc71::string file;
            {
                const vc71::string editorBase = getEditorDetailmapBaseName(); // P+0xF4
                file = ::renddx9abi::toVc71String(getLevelSavePath());
                file += editorBase;
                file += name;
                file += ".dds";
            }
            runNvdxt(::renddx9abi::toHostString(file), ::renddx9abi::toHostString(outputDirectory), options);   // E+0xF4
        }
        vc71::string target;
        {
            TerrainEditableNameResult base = getDetailmapBaseName();          // E+0x7C
            target = ::renddx9abi::toVc71String(getLevelSavePath());
            target += ::renddx9abi::toVc71String(base);
            target += name;
            target += ".dds";
        }
        vc71::string source;
        {
            TerrainEditableNameResult base = getDetailmapBaseName();
            source = ::renddx9abi::toVc71String(getLevelSavePath());
            source += ::renddx9abi::toVc71String(base);
            source += name;
            source += "_.dds";
        }
        ::dice::hfe::fileManager->moveFile(::renddx9abi::makeHostString(source),
                                           ::renddx9abi::makeHostString(target));   // +0x24
        {
            TerrainEditableNameResult base = getDetailmapBaseName();
            vc71::string leftover(::renddx9abi::toVc71String(getLevelSavePath()));
            leftover += ::renddx9abi::toVc71String(base);
            leftover += name;
            leftover += "_.dds";
            ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(leftover));
        }
    }

    vc71::string lowName;
    {
        TerrainEditableNameResult lowBase = getLowDetailmapBaseName();        // E+0x84
        const TerrainEditableString savePath = getLevelSavePath();            // E+0x90
        formatString(lowName, "%s%s%02ix%02i", savePath.c_str(), lowBase.c_str(), fileX, fileZ);
    }
    vc71::string lowOutputDirectory;
    {
        const vc71::string directory = getLowDetailmapDirectory();
        lowOutputDirectory = ::renddx9abi::toVc71String(getLevelSavePath());
        lowOutputDirectory += directory;
    }
    {
        vc71::string dds(lowName);
        dds += ".dds";
        ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(dds));
    }
    if (!hasNoDetail)
    {
        dx9rend::textureManagerSaveTexture(static_cast<dx9rend::EngineTexture*>(patch->lowDetailmap), lowName);
        {
            const TerrainEditableString options(m_lowDetailmapSettings);      // +0x7E4
            const vc71::string directory(lowOutputDirectory);
            vc71::string file(lowName);
            file += ".dds";
            runNvdxt(::renddx9abi::toHostString(file), ::renddx9abi::toHostString(directory), options);   // E+0xF4
        }
        {
            vc71::string input(lowName);
            input += ".dds";
            ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(input));
        }
        {
            vc71::string target(lowName);
            target += ".dds";
            vc71::string source(lowName);
            source += "_.dds";
            ::dice::hfe::fileManager->moveFile(::renddx9abi::makeHostString(source),
                                               ::renddx9abi::makeHostString(target));
        }
        {
            vc71::string leftover(lowName);
            leftover += "_.dds";
            ::dice::hfe::fileManager->deleteFile(::renddx9abi::makeHostString(leftover));
        }
    }
    patch->hasNoDetailLayers = hasNoDetail;                                   // +0x10C
}


// The ring patches leave engine mode first (E+0x118 with false); the sampler is made on the
// engine heap over the primary object and its +0x1C takes the argument - the allocation is not
// tested, as shipped. A failed init destroys and frees the sampler.

ILightSampling* TerrainEditable::createLightSampler(float resolutionModifier)
{
    setSurroundingTerrainEngineMode(false);                                   // E+0x118
    void* const storage = ::renddx9abi::engineAlloc(sizeof(TerrainLightSampler));
    TerrainLightSampler* const sampler = storage ? new (storage) TerrainLightSampler(this) : 0;
    sampler->m_resolutionModifier = resolutionModifier;                       // +0x1C
    if (sampler->init())
        return sampler;
    sampler->~TerrainLightSampler();
    ::renddx9abi::engineFree(sampler);
    return 0;
}


// The reflection factory - RefFactory_geom_TerrainEditable an outer object is refused,
// 0x854 bytes on the engine heap, the constructor, then queryInterface(iid) (not guarded against a
// failed allocation); a refused iid releases the object.

#if BF2_PLUGIN_ABI_NATIVE
static_assert(sizeof(TerrainEditable) <= 0x854, "the native layout must fit the shipped 0x854-byte allocation");
#endif

IBase* __fastcall createTerrainEditable(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(0x854);
    TerrainEditable* const self = storage ? new (storage) TerrainEditable() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
