// Light ownership, active-light culling and shadow-map rendering.
// [RETIRE-WHEN: the XpLightManager GPU resources, listener registration, XpLight coordinator and
// complete 189-slot operational surface are recovered]
// Replace remaining boundaries after their resource lifecycle and draw paths are validated.

#ifdef BF2_IMAGE_RENDDX9

#include "XpLightManager.h"
#include "HemiMapManager/HemiMapManager.h"
#include "MeshTemplate.h"                     // the geometry template's isWater (+0x50)
#include "StaticMesh.h"                       // staticMeshRootParent - RendDX9's getRootParent
#include "StaticMeshRenderer.h"               // G3: the StaticMesh shadow Effect (+0x44)
#include "Overgrowth/OverGrowth.h"            // G3: the level OverGrowth's +0x38 shadow query

#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../Math/Frustum.h"
#include "../RendMisc/IRenderView.h"
#include "../Game/Main/IVideoOptions.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"

// HfeAbi.h supplies the same four-slot IBase contract for the retail VC7.1 host ABI.  Reuse that
// declaration while importing the recovered engine class interfaces below; their virtual method
// order is the contract we need, while STL-bearing arguments are bridged explicitly at the few
// cross-compiler call sites.
#if !BF2_PLUGIN_ABI_NATIVE
#define CORE_IBASE_H
#define CORE_CLASSMANAGER_H
#endif
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Game/Objects/IPlayer.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../Game/Objects/Visual/LightSource.h"
#include "../Game/Objects/Visual/LightSourceTemplate.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Physics/ICollisionMesh.h"
#include "../Physics/HeightmapCluster.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectPredicator.h"
#include "../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../Scene/Object/PlayerManager.h"
#if !BF2_PLUGIN_ABI_NATIVE
#undef CORE_CLASSMANAGER_H
#undef CORE_IBASE_H
#endif
#if BF2_PLUGIN_ABI_NATIVE
#include "../Core/SettingsRepostitory.h"
#endif

#include <algorithm>
#include <cstddef>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>
#include <new>
#include <list>
#include <set>
#include <stdexcept>
#include <string.h>
#include <vector>

extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(
    IDirect3DSurface9* destination, const PALETTEENTRY* destinationPalette,
    const RECT* destinationRect, IDirect3DSurface9* source,
    const PALETTEENTRY* sourcePalette, const RECT* sourceRect,
    DWORD filter, D3DCOLOR colourKey);

// The modern SDK no longer ships D3DX9 import declarations.  lib.exe gives the plain .def export
// a cdecl-shaped thunk name, while this correct winapi declaration asks for the stdcall-decorated
// name.  Bridge those names without lying about the calling convention (eight arguments = @32).
#pragma comment(linker, "/alternatename:_D3DXLoadSurfaceFromSurface@32=_D3DXLoadSurfaceFromSurface")

// dice's DirectX SDK supplied this interface through d3dx9mesh.h.  The current Windows SDK does
// not, so retain the instruction-proven prefix used by LightManager::buildDebugGeometry rather
// than taking a machine-local June 2010 SDK dependency.  The prefix ends at GetIndexBuffer because
// no later ID3DXMesh slot is consumed here.
struct RendD3dxMesh : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE DrawSubset(DWORD attributeId) = 0;       // +0x0C
    virtual DWORD STDMETHODCALLTYPE GetNumFaces() = 0;                         // +0x10
    virtual DWORD STDMETHODCALLTYPE GetNumVertices() = 0;                      // +0x14
    virtual DWORD STDMETHODCALLTYPE GetFVF() = 0;                              // +0x18
    virtual HRESULT STDMETHODCALLTYPE GetDeclaration(D3DVERTEXELEMENT9*) = 0;  // +0x1C
    virtual DWORD STDMETHODCALLTYPE GetNumBytesPerVertex() = 0;                // +0x20
    virtual DWORD STDMETHODCALLTYPE GetOptions() = 0;                          // +0x24
    virtual HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**) = 0;       // +0x28
    virtual HRESULT STDMETHODCALLTYPE CloneMeshFVF(
        DWORD options, DWORD fvf, IDirect3DDevice9* device,
        RendD3dxMesh** clone) = 0;                                             // +0x2C
    virtual HRESULT STDMETHODCALLTYPE CloneMesh(
        DWORD options, const D3DVERTEXELEMENT9* declaration,
        IDirect3DDevice9* device, RendD3dxMesh** clone) = 0;                   // +0x30
    virtual HRESULT STDMETHODCALLTYPE GetVertexBuffer(IDirect3DVertexBuffer9**) = 0; // +0x34
    virtual HRESULT STDMETHODCALLTYPE GetIndexBuffer(IDirect3DIndexBuffer9**) = 0;   // +0x38
};

extern "C" HRESULT WINAPI D3DXCreateSphere(
    IDirect3DDevice9* device, float radius, unsigned int slices,
    unsigned int stacks, RendD3dxMesh** mesh, void** adjacency);
extern "C" HRESULT WINAPI D3DXCreateCylinder(
    IDirect3DDevice9* device, float radius1, float radius2, float length,
    unsigned int slices, unsigned int stacks, RendD3dxMesh** mesh, void** adjacency);
extern "C" HRESULT WINAPI D3DXCreateBox(
    IDirect3DDevice9* device, float width, float height, float depth,
    RendD3dxMesh** mesh, void** adjacency);

#pragma comment(linker, "/alternatename:_D3DXCreateSphere@24=_D3DXCreateSphere")
#pragma comment(linker, "/alternatename:_D3DXCreateCylinder@32=_D3DXCreateCylinder")
#pragma comment(linker, "/alternatename:_D3DXCreateBox@24=_D3DXCreateBox")

// [PLATFORM] RendDX9 uses D3DX view/projection builders; the dedicated ELF counterparts
// are empty. Mat4 and Vec3 supply the compatible float storage for these imports.
struct D3DXMATRIX;
struct D3DXVECTOR3;
extern "C" D3DXMATRIX* WINAPI D3DXMatrixLookAtLH(
    D3DXMATRIX* out, const D3DXVECTOR3* eye, const D3DXVECTOR3* at, const D3DXVECTOR3* up);
extern "C" D3DXMATRIX* WINAPI D3DXMatrixOrthoLH(
    D3DXMATRIX* out, float width, float height, float nearPlane, float farPlane);

#pragma comment(linker, "/alternatename:_D3DXMatrixLookAtLH@16=_D3DXMatrixLookAtLH")
#pragma comment(linker, "/alternatename:_D3DXMatrixOrthoLH@20=_D3DXMatrixOrthoLH")

namespace dice {
namespace hfe {

namespace world {
class IObject;
class LightSourceTemplate;
}

namespace dx9rend {
IRenderView* rendererGetRenderView();
bool rendererIsExpansionPack();
IVideoOptions* rendererGetVideoOptions();
extern void* g_rendererBase;
extern void* g_shaderManager;
// G3: BF2Render.cpp, dword_10258F38 - the level-scoped OverGrowth (created by the ECCore CEPreLoad
// arm, deleted and nulled by CEUnloadLevel). The directional pass calls its +0x38 unguarded.
extern void* g_OverGrowth;
}

namespace world {
// RendDX9 is a separate shipped image from BF2.exe. Both ABI builds now construct typed
// BundledMesh collision filters and need this non-pure base slot. PE1012C350 / ELF8163FC0
// are unconditional true; emit the recovered body locally as the original DLL did.
bool ObjectPredicator::predicateSurface(int)
{
    return true;
}
}

extern IBase* objectManager;
extern IBase* playerManager;

namespace geom {

extern LightManager* g_LightManager;
extern IBase* g_DebugRenderer;
// G3: ModuleGeom.cpp's singletons the quality-three arm reaches. dword_1024B8F8 is the
// StaticMeshRenderer (ModuleGeom's getSingletonList registers "StaticMeshRenderer" with that cell,
// SkinnedMesh.cpp's comment calling it the SkinnedMeshRenderer is stale);
// dword_1024B930 is the Terrain.
extern IBase* g_StaticMeshRenderer;
extern IBase* g_Terrain;

extern const unsigned int CID_LightManager  = 852417u;
extern const unsigned int IID_ILightManager = 0xB801DCF9u;

namespace {

static const unsigned int kLightManagerSize = 0x980u;      // RefFactory_geom_LightManager

extern const void* const kPrimaryVtable[189];
extern const void* const kDeviceLostVtable[3];
extern const void* const kRenderPathVtable[6];

template <typename T>
inline T& lmField(void* self, unsigned int offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(self) + offset);
}

template <typename T>
inline const T& lmField(const void* self, unsigned int offset)
{
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(self) + offset);
}

inline void lmSetVec3(void* self, unsigned int offset, float x, float y, float z)
{
    Vec3& value = lmField<Vec3>(self, offset);
    value.x = x;
    value.y = y;
    value.z = z;
}

inline void constructEmptyVector(void* self, unsigned int offset)
{
    vc71::vector_layout& vector = lmField<vc71::vector_layout>(self, offset);
    // VC7.1's empty allocator base is not written by the shipped constructor.
    vector.first = vector.last = vector.end = 0;
}

inline void reserveEmptyVector(vc71::vector_layout& vector,
                               unsigned int count, unsigned int elementSize)
{
    char* const storage = static_cast<char*>(::operator new(count * elementSize));
    vector.first = vector.last = storage;
    vector.end = storage + count * elementSize;
}

inline void setIdentityMatrix(void* self, unsigned int offset)
{
    lmField<Mat4>(self, offset) = Mat4::getIdentity();
}

void fillPowerLut(dx9rend::EngineTexture* texture, unsigned int count,
                  const Vec3& rgb, float exponent, bool /*save*/)
{
    dx9rend::TextureLockRecord* const lock =
        dx9rend::textureLock(texture, 0, 0x7FFFFFFFu);
    unsigned int* const pixels = static_cast<unsigned int*>(lock->pBits);
    const float scale = 1.0f / static_cast<float>(count - 1);
    for (unsigned int i = 0; i < count; ++i)
    {
        const float value = std::pow(static_cast<float>(i) * scale, exponent);
        const unsigned int red = static_cast<unsigned int>(255.0f * value * rgb.x);
        const unsigned int green = static_cast<unsigned int>(255.0f * value * rgb.y);
        const unsigned int blue = static_cast<unsigned int>(255.0f * value * rgb.z);
        pixels[i] = 0xFF000000u | ((red & 0xFFu) << 16)
                  | ((green & 0xFFu) << 8) | (blue & 0xFFu);
    }
    dx9rend::textureUnlock(texture, lock);
}

dx9rend::EngineTexture* createPowerLut(unsigned int count, const Vec3& rgb,
                                      float exponent, bool save)
{
    dx9rend::EngineTexture* const texture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            count, 1, D3DFMT_A8R8G8B8, 1, 0, 0));
    fillPowerLut(texture, count, rgb, exponent, save);
    return texture;
}

inline void normaliseCubeDirection(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) >= FLT_EPSILON)
    {
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        value.x *= inverseLength;
        value.y *= inverseLength;
        value.z *= inverseLength;
    }
}

inline unsigned int packNormalDirection(const Vec3& value)
{
    const int red = static_cast<int>(std::floor(value.x * 127.0f + 0.5f) + 128.0f);
    const int green = static_cast<int>(std::floor(value.y * 127.0f + 0.5f) + 128.0f);
    const int blue = static_cast<int>(std::floor(value.z * 127.0f + 0.5f) + 128.0f);
    return 0xFF000000u | (static_cast<unsigned int>(red) << 16)
         | (static_cast<unsigned int>(green) << 8) | static_cast<unsigned int>(blue);
}

// PE 0x101C3B00's lookup-type-three arm. It locks each cube surface directly and deliberately
// advances one dword per texel without consulting Pitch.
void fillNormalCubeMip(dx9rend::EngineTexture* texture, unsigned int mip,
                       unsigned int side)
{
    const float denominator = static_cast<float>(side - 1);
    for (unsigned int face = 0; face != 6; ++face)
    {
        IDirect3DSurface9* const surface =
            dx9rend::textureGetLevelSurface(texture, mip, face);
        D3DLOCKED_RECT locked;
        surface->LockRect(&locked, 0, 0);
        unsigned int* output = static_cast<unsigned int*>(locked.pBits);
        for (unsigned int y = 0; y != side; ++y)
        {
            const float v = static_cast<float>(y) / denominator * 2.0f - 1.0f;
            for (unsigned int x = 0; x != side; ++x)
            {
                const float u = static_cast<float>(x) / denominator * 2.0f - 1.0f;
                Vec3 direction;
                switch (face)
                {
                case 0: direction.x =  1.0f; direction.y = -v;   direction.z = -u;   break;
                case 1: direction.x = -1.0f; direction.y = -v;   direction.z =  u;   break;
                case 2: direction.x =  u;    direction.y =  1.0f; direction.z =  v;   break;
                case 3: direction.x =  u;    direction.y = -1.0f; direction.z = -v;   break;
                case 4: direction.x =  u;    direction.y = -v;   direction.z =  1.0f; break;
                default:direction.x = -u;    direction.y = -v;   direction.z = -1.0f; break;
                }
                normaliseCubeDirection(direction);
                *output++ = packNormalDirection(direction);
            }
        }
        surface->UnlockRect();
        surface->Release();
    }
}

// The ATI/PS1 combination receives one mip; every other device receives three.
dx9rend::EngineTexture* createNormalCubeTexture()
{
    const unsigned int mipLevels =
        dx9rend::deviceCapsIsAti() && dx9rend::deviceCapsPixelShaderMajor() == 1 ? 1u : 3u;
    dx9rend::EngineTexture* const texture =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateCube(
            256, D3DFMT_A8R8G8B8, mipLevels, 0, 0));
    fillNormalCubeMip(texture, 0, 256);
    if (mipLevels > 1)
    {
        fillNormalCubeMip(texture, 1, 128);
        fillNormalCubeMip(texture, 2, 64);
    }
    return texture;
}

void createSmallLightTextures(void* self)
{
    dx9rend::EngineTexture* const white =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            8, 8, D3DFMT_A8R8G8B8, 1, 0, 0));
    lmField<dx9rend::EngineTexture*>(self, 0x23C) = white;
    dx9rend::TextureLockRecord* lock = dx9rend::textureLock(white, 0, 0x7FFFFFFFu);
    ::memset(lock->pBits, 0xFF, (lock->bpp / 8) << 6);
    dx9rend::textureUnlock(white, lock);

    dx9rend::EngineTexture* const black =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            8, 8, D3DFMT_A8R8G8B8, 1, 0, 0));
    lmField<dx9rend::EngineTexture*>(self, 0x240) = black;
    lock = dx9rend::textureLock(black, 0, 0x7FFFFFFFu);
    ::memset(lock->pBits, 0, (lock->bpp / 8) << 6);
    dx9rend::textureUnlock(black, lock);

    dx9rend::EngineTexture* const framed =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            8, 8, D3DFMT_A8R8G8B8, 1, 0, 0));
    lmField<dx9rend::EngineTexture*>(self, 0x244) = framed;
    lock = dx9rend::textureLock(framed, 0, 0x7FFFFFFFu);
    ::memset(lock->pBits, 0xFF, (lock->bpp / 8) << 6);
    const unsigned int bytesPerPixel = static_cast<unsigned int>(lock->bpp / 8);
    unsigned char* const bits = static_cast<unsigned char*>(lock->pBits);
    for (unsigned int y = 2; y != 6; ++y)
    {
        for (unsigned int x = 2; x != 6; ++x)
            *reinterpret_cast<unsigned int*>(bits + (y * 8 + x) * bytesPerPixel) = 0xFF000000u;
    }
    dx9rend::textureUnlock(framed, lock);

    dx9rend::EngineTexture* const cube =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateCube(
            8, D3DFMT_X8R8G8B8, 1, 0, 0));
    lmField<dx9rend::EngineTexture*>(self, 0x248) = cube;
    IDirect3DSurface9* const source =
        dx9rend::textureGetLevelSurface(black, 0, 0x7FFFFFFFu);
    for (unsigned int face = 0; face < 6; ++face)
    {
        IDirect3DSurface9* const destination =
            dx9rend::textureGetLevelSurface(cube, 0, face);
        D3DXLoadSurfaceFromSurface(destination, 0, 0, source, 0, 0, 0xFFFFFFFFu, 0);
        destination->Release();
    }
    source->Release();
}

// PE allocator 0x100F5CF0 and its four tree owners.  These are not vc71::set_layout nodes: the
// payload is eight bytes and the colour/isNil bytes sit at +0x14/+0x15 in a 0x18-byte node.
struct ShadowTreeNode
{
    ShadowTreeNode* left;
    ShadowTreeNode* parent;
    ShadowTreeNode* right;
    unsigned int    key;
    void*           value;
    unsigned char   colour;
    unsigned char   isNil;
    unsigned char   pad16[2];
};

struct ShadowTree
{
    unsigned int    compareAllocator;
    ShadowTreeNode* head;
    unsigned int    size;
};

#if defined(_M_IX86)
static_assert(sizeof(ShadowTreeNode) == 0x18, "LightManager shadow-tree node layout drift");
static_assert(sizeof(ShadowTree) == 0x0C, "LightManager shadow-tree header layout drift");
#endif

ShadowTreeNode* createShadowTreeNode()
{
    ShadowTreeNode* const node = static_cast<ShadowTreeNode*>(
        ::renddx9abi::engineAlloc(sizeof(ShadowTreeNode)));
    // PE 0x100F5CF0 writes through the engine allocation without a null check.
    node->left = node->parent = node->right = 0;
    node->colour = 1;
    node->isNil = 0;
    return node;
}

void constructShadowTree(ShadowTree& tree)
{
    // PE's empty comparator/allocator occupies storage but its constructor emits no write.
    tree.head = createShadowTreeNode();
    tree.size = 0;
    tree.head->isNil = 1;
    tree.head->left = tree.head->parent = tree.head->right = tree.head;
}

void constructLightSet(vc71::set_layout& set)
{
    // LightManager's two inlined VC7.1 set constructors differ observably from the shared
    // compatibility facade: the empty comparator dword is untouched and allocation is
    // dereferenced unconditionally (PE 0x100F5BC0).
    vc71::set_node* const node = static_cast<vc71::set_node*>(
        ::renddx9abi::engineAlloc(sizeof(vc71::set_node)));
    node->left = node->parent = node->right = 0;
    node->color = 1;
    node->isNil = 0;
    node->isNil = 1;
    node->left = node->parent = node->right = node;
    set.head = node;
    set.size = 0;
}

void rotateShadowTreeLeft(ShadowTree& tree, ShadowTreeNode* node)
{
    ShadowTreeNode* const pivot = node->right;
    node->right = pivot->left;
    if (!pivot->left->isNil)
        pivot->left->parent = node;
    pivot->parent = node->parent;
    if (node == tree.head->parent)
        tree.head->parent = pivot;
    else if (node == node->parent->left)
        node->parent->left = pivot;
    else
        node->parent->right = pivot;
    pivot->left = node;
    node->parent = pivot;
}

void rotateShadowTreeRight(ShadowTree& tree, ShadowTreeNode* node)
{
    ShadowTreeNode* const pivot = node->left;
    node->left = pivot->right;
    if (!pivot->right->isNil)
        pivot->right->parent = node;
    pivot->parent = node->parent;
    if (node == tree.head->parent)
        tree.head->parent = pivot;
    else if (node == node->parent->right)
        node->parent->right = pivot;
    else
        node->parent->left = pivot;
    pivot->right = node;
    node->parent = pivot;
}

ShadowTreeNode* shadowTreeFindOrInsert(ShadowTree& tree, unsigned int key)
{
    ShadowTreeNode* const head = tree.head;
    ShadowTreeNode* parent = head;
    ShadowTreeNode* node = head->parent;
    bool insertLeft = true;

    while (!node->isNil)
    {
        parent = node;
        if (key < node->key)
        {
            insertLeft = true;
            node = node->left;
        }
        else if (node->key < key)
        {
            insertLeft = false;
            node = node->right;
        }
        else
        {
            return node;
        }
    }

    // PE 0x1015D000 carries the VC7.1 map/set maximum-size check before allocating a node.
    if (tree.size >= 0x1FFFFFFEu)
        throw std::length_error("map/set<T> too long");

    node = createShadowTreeNode();
    node->left = head;
    node->parent = parent;
    node->right = head;
    node->key = key;
    node->value = 0;
    node->colour = 0; // red; the head sentinel and final root are black (1)

    ++tree.size;
    if (parent == head)
    {
        head->parent = node;
        head->left = node;
        head->right = node;
    }
    else if (insertLeft)
    {
        parent->left = node;
        if (parent == head->left)
            head->left = node;
    }
    else
    {
        parent->right = node;
        if (parent == head->right)
            head->right = node;
    }

    // PE 0x1015D000 is the ordinary VC7.1 red/black insertion fix-up.  Keep its colour
    // convention (0 red, 1 black), rotations and root-parent sentinel relationship explicit.
    ShadowTreeNode* current = node;
    while (!current->parent->colour)
    {
        ShadowTreeNode* const parentNode = current->parent;
        ShadowTreeNode* const grandparent = parentNode->parent;
        if (parentNode == grandparent->left)
        {
            ShadowTreeNode* const uncle = grandparent->right;
            if (!uncle->colour)
            {
                parentNode->colour = 1;
                uncle->colour = 1;
                grandparent->colour = 0;
                current = grandparent;
            }
            else
            {
                if (current == parentNode->right)
                {
                    current = parentNode;
                    rotateShadowTreeLeft(tree, current);
                }
                current->parent->colour = 1;
                current->parent->parent->colour = 0;
                rotateShadowTreeRight(tree, current->parent->parent);
            }
        }
        else
        {
            ShadowTreeNode* const uncle = grandparent->left;
            if (!uncle->colour)
            {
                parentNode->colour = 1;
                uncle->colour = 1;
                grandparent->colour = 0;
                current = grandparent;
            }
            else
            {
                if (current == parentNode->left)
                {
                    current = parentNode;
                    rotateShadowTreeRight(tree, current);
                }
                current->parent->colour = 1;
                current->parent->parent->colour = 0;
                rotateShadowTreeLeft(tree, current->parent->parent);
            }
        }
    }
    head->parent->colour = 1;
    return node;
}

void freeShadowTreeBranch(ShadowTreeNode* node)
{
    while (!node->isNil)
    {
        freeShadowTreeBranch(node->right);
        ShadowTreeNode* const left = node->left;
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

void clearShadowTree(ShadowTree& tree)
{
    if (!tree.head)
        return;
    freeShadowTreeBranch(tree.head->parent);
    tree.head->left = tree.head->parent = tree.head->right = tree.head;
    tree.size = 0;
}

void releaseShadowTreeValues(ShadowTreeNode* node)
{
    if (node->isNil)
        return;
    releaseShadowTreeValues(node->left);
    if (node->value)
        dx9rend::effectRelease(static_cast<dx9rend::Effect*>(node->value));
    releaseShadowTreeValues(node->right);
}

void destroyShadowTree(ShadowTree& tree, bool releaseValues)
{
    if (!tree.head)
        return;
    if (releaseValues)
        // +0x964's values are Effect*. Retail walks them in key order and calls
        // release (+0x08) before it tears the nodes down.
        releaseShadowTreeValues(tree.head->parent);
    clearShadowTree(tree);
    ::renddx9abi::engineFree(tree.head);
    tree.head = 0;
    tree.size = 0;
}

void releaseSurface(IDirect3DSurface9*& surface)
{
    if (surface)
        surface->Release();
    surface = 0;
}

bool releaseShadowResources(void* self, bool result)
{
    dx9rend::textureReleaseRef(lmField<dx9rend::EngineTexture*>(self, 0x21C));
    dx9rend::textureReleaseRef(lmField<dx9rend::EngineTexture*>(self, 0x220));
    dx9rend::textureReleaseRef(lmField<dx9rend::EngineTexture*>(self, 0x224));
    releaseSurface(lmField<IDirect3DSurface9*>(self, 0x228));
    releaseSurface(lmField<IDirect3DSurface9*>(self, 0x22C));

    dx9rend::EngineTexture* const secondary =
        lmField<dx9rend::EngineTexture*>(self, 0x230);
    IDirect3DSurface9* const secondaryDepth =
        lmField<IDirect3DSurface9*>(self, 0x238);
    if (secondary && secondary->baseTexture && secondaryDepth)
    {
        // PE 0x100F21A0's alias-lifetime bracket is deliberately net-zero.  Preserve the exact
        // AddRef/AddRef/AddRef/Release/Release/Release sequence before owner releases below.
        secondary->baseTexture->AddRef();
        secondaryDepth->AddRef();
        secondary->baseTexture->AddRef();
        secondary->baseTexture->Release();
        secondary->baseTexture->Release();
        secondaryDepth->Release();
    }
    dx9rend::textureReleaseRef(lmField<dx9rend::EngineTexture*>(self, 0x230));
    dx9rend::textureReleaseRef(lmField<dx9rend::EngineTexture*>(self, 0x234));
    releaseSurface(lmField<IDirect3DSurface9*>(self, 0x238));

    for (unsigned int offset = 0x21C; offset <= 0x238; offset += 4)
        lmField<void*>(self, offset) = 0;
    return result;
}

void setRenderTargetTexture(unsigned int index, dx9rend::EngineTexture* texture)
{
    IDirect3DSurface9* surface = 0;
    if (texture)
        surface = dx9rend::textureGetLevelSurface(texture, 0, 0x7FFFFFFFu);
    else if (index == 0)
    {
        unsigned char* const renderer =
            static_cast<unsigned char*>(dx9rend::g_rendererBase);
        surface = *reinterpret_cast<IDirect3DSurface9**>(renderer + 0x0F4);
        if (!surface)
            surface = *reinterpret_cast<IDirect3DSurface9**>(renderer + 0x76C);
        if (surface)
            surface->AddRef();
    }
    dx9rend::stateManagerSetRenderTarget(index, surface);
    if (surface)
        surface->Release();
}

void setLightManagerDepthStencil(IDirect3DSurface9* surface)
{
    // PE 0x100057A0 substitutes Renderer +0x770 when passed null; this is not equivalent to
    // handing a literal null to StateManager +0x64.
    if (!surface)
        surface = *reinterpret_cast<IDirect3DSurface9**>(
            static_cast<unsigned char*>(dx9rend::g_rendererBase) + 0x770);
    dx9rend::stateManagerSetDepthStencilSurface(surface);
}

void restoreRendererTarget0()
{
    // PE 0x10028070 first restores Renderer +0x7D4's EngineTexture. Only a null texture takes
    // the generic 0x10005920 back-buffer fallback used by setRenderTargetTexture(0, 0).
    dx9rend::EngineTexture* const texture =
        *reinterpret_cast<dx9rend::EngineTexture**>(
            static_cast<unsigned char*>(dx9rend::g_rendererBase) + 0x7D4);
    setRenderTargetTexture(0, texture);
}

bool createShadowResourcesBody(void* self)
{
    if (!lmField<unsigned char>(self, 0x7EC))
        return false;

    releaseShadowResources(self, true);
    const int oldManaged = dx9rend::textureManagerSwapManaged(0);
    const int requested = lmField<int>(self, 0x204);
    // PE uses signed `jl`: negative configured sizes survive this clamp and are passed onward as
    // their raw dword bit pattern. Do not turn them into the unsigned minimum's 2048.
    const int clampedSize = requested < 2048 ? requested : 2048;
    const unsigned int primarySize = static_cast<unsigned int>(clampedSize);
    const unsigned int secondarySize = static_cast<unsigned int>(clampedSize);
    const unsigned int format = lmField<unsigned int>(self, 0x20C);
    const unsigned int usage = lmField<unsigned int>(self, 0x210);

    lmField<dx9rend::EngineTexture*>(self, 0x21C) =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            primarySize, primarySize, format, 1, usage, 0));
    if (!lmField<void*>(self, 0x21C))
        return releaseShadowResources(self, false);
    lmField<dx9rend::EngineTexture*>(self, 0x220) =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            primarySize, primarySize, format, 1, usage, 0));
    if (!lmField<void*>(self, 0x220))
        return releaseShadowResources(self, false);

    const unsigned int mode = lmField<unsigned int>(self, 0x910);
    if (mode != 0)
    {
        IDirect3DDevice9* const device = lmField<IDirect3DDevice9*>(self, 0x908);
        device->CreateDepthStencilSurface(
            primarySize, primarySize, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE,
            &lmField<IDirect3DSurface9*>(self, 0x228), 0);
        if (!lmField<void*>(self, 0x228))
            return releaseShadowResources(self, false);
        lmField<IDirect3DSurface9*>(self, 0x22C) =
            lmField<IDirect3DSurface9*>(self, 0x228);
        lmField<IDirect3DSurface9*>(self, 0x22C)->AddRef();
    }
    else
    {
        lmField<dx9rend::EngineTexture*>(self, 0x224) =
            static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
                primarySize, primarySize, 23, 1, D3DUSAGE_RENDERTARGET, 0));
        if (!lmField<void*>(self, 0x224))
            return releaseShadowResources(self, false);
        lmField<IDirect3DSurface9*>(self, 0x228) =
            dx9rend::textureGetLevelSurface(
                lmField<dx9rend::EngineTexture*>(self, 0x21C), 0, 0x7FFFFFFFu);
        lmField<IDirect3DSurface9*>(self, 0x22C) =
            dx9rend::textureGetLevelSurface(
                lmField<dx9rend::EngineTexture*>(self, 0x220), 0, 0x7FFFFFFFu);
    }

    lmField<dx9rend::EngineTexture*>(self, 0x230) =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            secondarySize, secondarySize, format, 1, usage, 0));
    if (!lmField<void*>(self, 0x230))
        return releaseShadowResources(self, false);

    if (mode == 0)
    {
        if (secondarySize == primarySize)
        {
            lmField<dx9rend::EngineTexture*>(self, 0x234) =
                lmField<dx9rend::EngineTexture*>(self, 0x224);
            dx9rend::textureAddRef(lmField<dx9rend::EngineTexture*>(self, 0x234));
        }
        else
        {
            lmField<dx9rend::EngineTexture*>(self, 0x234) =
                static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
                    primarySize, primarySize, 23, 1, D3DUSAGE_RENDERTARGET, 0));
            if (!lmField<void*>(self, 0x234))
                return releaseShadowResources(self, false);
        }
        lmField<IDirect3DSurface9*>(self, 0x238) =
            dx9rend::textureGetLevelSurface(
                lmField<dx9rend::EngineTexture*>(self, 0x230), 0, 0x7FFFFFFFu);
    }
    else if (secondarySize == primarySize)
    {
        lmField<IDirect3DSurface9*>(self, 0x238) =
            lmField<IDirect3DSurface9*>(self, 0x228);
        lmField<IDirect3DSurface9*>(self, 0x238)->AddRef();
    }
    else
    {
        IDirect3DDevice9* const device = lmField<IDirect3DDevice9*>(self, 0x908);
        device->CreateDepthStencilSurface(
            secondarySize, secondarySize, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE,
            &lmField<IDirect3DSurface9*>(self, 0x238), 0);
        if (!lmField<void*>(self, 0x238))
            return releaseShadowResources(self, false);
    }

    dx9rend::textureManagerSwapManaged(oldManaged);
    dx9rend::raShaderConstantsSetTexture(
        0x268, lmField<dx9rend::EngineTexture*>(self, 0x21C)->baseTexture);
    dx9rend::raShaderConstantsSetTexture(
        0x270, lmField<dx9rend::EngineTexture*>(self, 0x220)->baseTexture);
    void* const publishedManager = g_LightManager;
    dx9rend::raShaderConstantsSetTexture(
        0x278, lmField<dx9rend::EngineTexture*>(publishedManager, 0x230)->baseTexture);
    dx9rend::raShaderConstantsSetTexture(
        0x288, lmField<dx9rend::EngineTexture*>(publishedManager, 0x244)->baseTexture);

    // byte_102395B0 is initialised true and has no writer.  Every successful
    // resource rebuild binds and clears both shadow targets before restoring the back buffer.
    setLightManagerDepthStencil(
        lmField<IDirect3DSurface9*>(self, 0x228));
    setRenderTargetTexture(0, lmField<dx9rend::EngineTexture*>(
        self, mode != 0 ? 0x21C : 0x224));
    dx9rend::stateManagerClear(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                               0xFFFFFFFFu, 1.0f, 0);

    setRenderTargetTexture(0, lmField<dx9rend::EngineTexture*>(
        self, mode != 0 ? 0x230 : 0x234));
    setLightManagerDepthStencil(
        lmField<IDirect3DSurface9*>(self, 0x238));
    dx9rend::stateManagerClear(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                               0xFFFFFFFFu, 1.0f, 0);

    setLightManagerDepthStencil(0);
    setRenderTargetTexture(0, 0);
    dx9rend::flushRenderTargetsSeam();

    RECT& rectangle = lmField<RECT>(self, 0x78C);
    rectangle.left = rectangle.top = 0;
    rectangle.right = rectangle.bottom = requested;
    ::InflateRect(&rectangle, -3, -3);
    return true;
}

inline vc71::set_layout& lmOwnedLights(void* self)
{
    return lmField<vc71::set_layout>(self, 0x1B8);
}

inline vc71::set_layout& lmActiveLights(void* self)
{
    return lmField<vc71::set_layout>(self, 0x1C4);
}

inline unsigned int lmLightKey(world::LightSource* light)
{
    return reinterpret_cast<unsigned int>(light);
}

// Calls into BF2.exe use the recovered source interfaces themselves.  HostString is VC7.1's
// std::string layout in retail mode, but these callees receive it by reference, so the bridge only
// changes the static spelling of that reference; no current-CRT string operation touches it.
inline const std::string& hostStdString(const renddx9abi::HostString& value)
{
#if BF2_PLUGIN_ABI_NATIVE
    return value;
#else
    return *reinterpret_cast<const std::string*>(&value);
#endif
}

inline const world::LightSourceTemplate* lightSourceTemplatePointer(
    const world::LightSource* light)
{
#if BF2_PLUGIN_ABI_NATIVE
    return static_cast<const world::LightSourceTemplate*>(light->getTemplate());
#else
    return *reinterpret_cast<const world::LightSourceTemplate* const*>(
        reinterpret_cast<const unsigned char*>(light) + 0x144);
#endif
}

inline unsigned int lightSourceFlags(const world::LightSource* light)
{
#if BF2_PLUGIN_ABI_NATIVE
    return light->getFlags();
#else
    return *reinterpret_cast<const unsigned int*>(
        reinterpret_cast<const unsigned char*>(light) + 0x04);
#endif
}

inline const world::IObjectTemplate* objectTemplatePointer(const world::IObject* object)
{
#if BF2_PLUGIN_ABI_NATIVE
    return object->getTemplate();
#else
    return *reinterpret_cast<const world::IObjectTemplate* const*>(
        reinterpret_cast<const unsigned char*>(object) + 0x14);
#endif
}

#if !BF2_PLUGIN_ABI_NATIVE
inline std::vector<world::IObject*>& hostObjectVector(vc71::vector_layout& value)
{
    // A retail ObjectManager consumes the VC7.1 vector object at this address.  The rebuilt DLL
    // never evaluates a native std::vector operation through this reference; it only passes it on.
    return *reinterpret_cast<std::vector<world::IObject*>*>(&value);
}
#endif

template <typename T>
inline T* vectorBegin(vc71::vector_layout& vector)
{
    return reinterpret_cast<T*>(vector.first);
}

template <typename T>
inline const T* vectorBegin(const vc71::vector_layout& vector)
{
    return reinterpret_cast<const T*>(vector.first);
}

template <typename T>
inline unsigned int vectorSize(const vc71::vector_layout& vector)
{
    return vector.first
        ? static_cast<unsigned int>((vector.last - vector.first) / sizeof(T))
        : 0u;
}

inline void clearPointerVector(vc71::vector_layout& vector)
{
    // std::vector<T*>::clear destroys no payload and retains its allocation.
    vector.last = vector.first;
}

inline void destroyVectorStorage(vc71::vector_layout& vector)
{
    if (vector.first)
        ::operator delete(vector.first);
    vector.first = vector.last = vector.end = 0;
}

struct ShadowAtlasEntry
{
    int x;
    int y;
    int width;
    int height;
    int tier;
};

#if defined(_M_IX86)
static_assert(sizeof(ShadowAtlasEntry) == 0x14, "shadow-atlas entry layout drift");
#endif

// PE 0x10239350..0x10239580, consumed by 0x100F3160 in this exact order.
const ShadowAtlasEntry kShadowAtlas2048x1024[28] =
{
    {   0,    0, 1024, 1024, 0 }, { 1024,    0, 1024, 1024, 0 },
    {   0, 1024,  512,  512, 1 }, {  512, 1024,  512,  512, 1 },
    {   0, 1536,  512,  512, 1 }, {  512, 1536,  256,  256, 2 },
    { 768, 1536,  256,  256, 2 }, {  512, 1792,  256,  256, 2 },
    { 768, 1792,  256,  256, 2 }, { 1024, 1024,  256,  256, 2 },
    {1280, 1024,  256,  256, 2 }, { 1536, 1024,  256,  256, 2 },
    {1792, 1024,  256,  256, 2 }, { 1024, 1280,  256,  256, 2 },
    {1280, 1280,  256,  256, 2 }, { 1536, 1280,  256,  256, 2 },
    {1792, 1280,  256,  256, 2 }, { 1024, 1536,  256,  256, 2 },
    {1280, 1536,  256,  256, 2 }, { 1536, 1536,  256,  256, 2 },
    {1792, 1536,  256,  256, 2 }, { 1024, 1792,  256,  256, 2 },
    {1280, 1792,  256,  256, 2 }, { 1536, 1792,  256,  256, 2 },
    {1792, 1792,  128,  128, 3 }, { 1920, 1792,  128,  128, 3 },
    {1792, 1920,  128,  128, 3 }, { 1920, 1920,  128,  128, 3 }
};

// PE BSS the same 28 placements materialised as Win32 RECTs for the shadow renderer.
// The square fallback arm does not touch this table; only the two 28-entry atlas layouts do.
RECT g_shadowAtlasRectangles[28];
unsigned int g_shadowRecordOrdinal;

void publishShadowAtlasRectangle(unsigned int index, const ShadowAtlasEntry& entry)
{
    RECT& rectangle = g_shadowAtlasRectangles[index];
    rectangle.left = entry.x;
    rectangle.top = entry.y;
    rectangle.right = entry.x + entry.width;
    rectangle.bottom = entry.y + entry.height;
}

// The atlas vector is destroyed and rebuilt, rather than merely cleared.  The
// 1024x512 arm halves only the rectangle coordinates/sizes; the tier survives unchanged. Both
// 28-entry arms also publish the parallel global rect table at PE 0x10259EE0.
void rebuildShadowAtlas(void* self)
{
    vc71::vector_layout& atlas = lmField<vc71::vector_layout>(self, 0x7BC);
    destroyVectorStorage(atlas);
    vc71::vectorRef<ShadowAtlasEntry> output(&atlas);
    const int width = lmField<int>(self, 0x204);
    const int height = lmField<int>(self, 0x208);

    if (width == 2048 && height == 1024)
    {
        for (unsigned int i = 0; i < 28; ++i)
        {
            output.push_back(kShadowAtlas2048x1024[i]);
            publishShadowAtlasRectangle(i, kShadowAtlas2048x1024[i]);
        }
    }
    else if (width == 1024 && height == 512)
    {
        for (unsigned int i = 0; i < 28; ++i)
        {
            ShadowAtlasEntry entry = kShadowAtlas2048x1024[i];
            entry.x /= 2;
            entry.y /= 2;
            entry.width /= 2;
            entry.height /= 2;
            output.push_back(entry);
            publishShadowAtlasRectangle(i, entry);
        }
    }
    else if (width == height)
    {
        // Retail performs this division without a zero guard.
        const int divisor = 2048 / width;
        ShadowAtlasEntry entry = kShadowAtlas2048x1024[0];
        entry.x /= divisor;
        entry.y /= divisor;
        entry.width /= divisor;
        entry.height /= divisor;
        output.push_back(entry);
    }
}

void* getEffectParameter(void* effect, const char* semantic)
{
    vc71::string name(semantic);
    return dx9rend::effectGetShaderParameter(
        static_cast<dx9rend::Effect*>(effect), name);
}

dx9rend::EngineTexture* loadManagerTexture(const char* path)
{
    vc71::string name(path);
    return static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(name));
}

void bindAndClearShadowTarget(void* self)
{
    // byte_10260A08 is false in the shipped data image.  Performance flag 3 is the live bypass.
    if (dx9rend::rendererPerformanceFlag(3))
        return;

    setLightManagerDepthStencil(
        lmField<IDirect3DSurface9*>(self, 0x238));
    if (lmField<unsigned int>(self, 0x910) != 0)
    {
        setRenderTargetTexture(0, lmField<dx9rend::EngineTexture*>(self, 0x230));
        dx9rend::stateManagerClear(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                   0xFFFFFFFFu, 1.0f, 0);
    }
    else
    {
        setRenderTargetTexture(0, lmField<dx9rend::EngineTexture*>(self, 0x234));
        dx9rend::stateManagerClear(D3DCLEAR_ZBUFFER, 0xFFFFFFFFu, 1.0f, 0);
        dx9rend::stateManagerSetRenderState(195, 0);
    }
}

inline float dot3(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float planeDistance(const Plane3& plane, const Vec3& point)
{
    return dot3(plane.normal, point) + plane.d;
}

inline void* objectGeometry(world::IObject* object)
{
#if BF2_PLUGIN_ABI_NATIVE
    return world::object_detail::LayoutAccess::geometry(object);
#else
    return *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(object) + 0x38);
#endif
}

inline const Vec3& objectAbsolutePosition(world::IObject* object)
{
    return object->getAbsolutePosition();
}

inline float objectVisualRadius(world::IObject* object)
{
    return object->getVisualBoundingRadius();
}

// Accepts enabled shadow casters with the required template flag, excluding water.
// When accept is clear, also excludes the local player's first-person vehicle root.
bool shadowCasterPredicateBody(world::IObject* object, unsigned char accept)
{
    void* const geometry = objectGeometry(object);
    if (geometry)
    {
        typedef void* (__fastcall* GetTemplateFn)(void*, void*);
        void** const vtbl = *reinterpret_cast<void***>(geometry);
        void* const geometryTemplate =
            reinterpret_cast<GetTemplateFn>(vtbl[0x38 / sizeof(void*)])(geometry, 0);
        if (static_cast<MeshTemplate*>(geometryTemplate)->isWater())              // +0x50
            return false;
    }

    const world::IObjectTemplate* const objectTemplate = objectTemplatePointer(object);
#if BF2_PLUGIN_ABI_NATIVE
    const unsigned int objectFlags = object->getFlags();
#else
    const unsigned int objectFlags = *reinterpret_cast<const unsigned int*>(
        reinterpret_cast<const unsigned char*>(object) + 0x04);
#endif
    if ((objectTemplate->getFlags() & 0x800u) == 0)                               // +0x44
        return false;
    if ((objectFlags & 2u) == 0)
        return false;
    if (accept != 0)
        return true;

    world::IObject* const root =
        static_cast<world::IObject*>(staticMeshRootParent(object));
    if (!root)
        return true;
    world::IPlayer* const player =
        reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer();   // +0x30
    if (!player)
        return true;
    if (!player->hasFirstPersonView())                                            // PE +0x184
        return true;
    return player->getVehicle() != root;                                          // PE +0x44
}

#if BF2_PLUGIN_ABI_NATIVE
class ShadowCasterPredicator : public world::ObjectPredicator
{
public:
    ShadowCasterPredicator() : m_accept(1) {}
    virtual bool predicate(world::IObject* object)
    {
        return shadowCasterPredicateBody(object, m_accept);
    }
    virtual unsigned int getFlags() const { return 0; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }

private:
    unsigned char m_accept;
    unsigned char m_pad05[3];
};
#else
struct ShadowCasterPredicator
{
    const void**  table;
    unsigned char accept;
    unsigned char pad05[3];
};

bool __fastcall shadowPredicate(void* self, void*, world::IObject* object)
{
    return shadowCasterPredicateBody(
        object, static_cast<ShadowCasterPredicator*>(self)->accept);
}
unsigned int __fastcall shadowPredicateFlags(void*, void*) { return 0; }
bool __fastcall shadowPredicateCull(void*, void*, world::IObject*) { return true; }
bool __fastcall shadowPredicateSurface(void*, void*, int) { return true; }
const void* const kShadowCasterPredicatorVtable[4] =
{
    reinterpret_cast<void*>(&shadowPredicate),
    reinterpret_cast<void*>(&shadowPredicateFlags),
    reinterpret_cast<void*>(&shadowPredicateCull),
    reinterpret_cast<void*>(&shadowPredicateSurface)
};
#endif

void constructShadowCasterPredicator(ShadowCasterPredicator& predicate)
{
#if BF2_PLUGIN_ABI_NATIVE
    (void)predicate; // the native C++ constructor already installed the exact four-slot table
#else
    predicate.table = const_cast<const void**>(kShadowCasterPredicatorVtable);
    predicate.accept = 1;
    predicate.pad05[0] = predicate.pad05[1] = predicate.pad05[2] = 0;
#endif
}

struct ShadowSweepClassifier
{
    std::vector<const Plane3*> away;
    std::vector<const Plane3*> toward;
    Vec3 direction;
};

void constructShadowSweepClassifier(ShadowSweepClassifier& classifier,
                                    const Frustum& frustum, const Vec3& direction)
{
    classifier.direction = direction;
    for (unsigned int i = 0; i < 6; ++i)
    {
        const Plane3* const plane = &frustum.m_planes[i];
        // 0x1016C573..0x1016C597, re-the dot sums
        // (z*z' + y*y') + x*x' (float temporaries - 24-bit x87), and `fcomp 0.0f; test ah,5; jp`
        // sends every plane that is NOT strictly behind - ">= 0" AND unordered (a NaN dot) - to the
        // second vector (`toward`, +0x10); only a strictly negative dot goes to `away` (+0x04). The
        // earlier `dot3(...) >= 0.0f` summed (x + y) + z and sent a NaN to `away`.
        const float zy = plane->normal.z * direction.z + plane->normal.y * direction.y;
        const float dot = zy + plane->normal.x * direction.x;
        if (!(dot < 0.0f))
            classifier.toward.push_back(plane);
        else
            classifier.away.push_back(plane);
    }
}

bool shadowSweepAccepts(const ShadowSweepClassifier& classifier, world::IObject* object)
{
    const float radius = objectVisualRadius(object);
    const Vec3& centre = objectAbsolutePosition(object);

    for (std::vector<const Plane3*>::const_iterator it = classifier.toward.begin();
         it != classifier.toward.end(); ++it)
    {
        const float d = planeDistance(**it, centre);
        if (d > radius)
            return false;
        if (d > -radius)
            return true;
    }

    for (std::vector<const Plane3*>::const_iterator it = classifier.away.begin();
         it != classifier.away.end(); ++it)
    {
        const Plane3& plane = **it;
        if (planeDistance(plane, centre) < radius)
            continue;

        Vec3 end;
        end.x = centre.x + classifier.direction.x;
        end.y = centre.y + classifier.direction.y;
        end.z = centre.z + classifier.direction.z;
        const Vec3 delta = { end.x - centre.x, end.y - centre.y, end.z - centre.z };
        const float denominator = dot3(plane.normal, delta);
        const float t = -planeDistance(plane, centre) / denominator;
        Vec3 intersection;
        intersection.x = centre.x + delta.x * t;
        intersection.y = centre.y + delta.y * t;
        intersection.z = centre.z + delta.z * t;

        for (std::vector<const Plane3*>::const_iterator toward = classifier.toward.begin();
             toward != classifier.toward.end(); ++toward)
        {
            if (planeDistance(**toward, intersection) > radius)
                return false;
        }
    }
    return true;
}

void filterShadowCandidates(const ShadowSweepClassifier& classifier,
                            vc71::vector_layout& candidates)
{
    world::IObject** current = vectorBegin<world::IObject*>(candidates);
    world::IObject** end = reinterpret_cast<world::IObject**>(candidates.last);
    while (current != end)
    {
        if (shadowSweepAccepts(classifier, *current))
        {
            ++current;
            continue;
        }
        ::memmove(current, current + 1,
                  static_cast<unsigned int>(reinterpret_cast<unsigned char*>(end)
                                           - reinterpret_cast<unsigned char*>(current + 1)));
        --end;
        candidates.last -= sizeof(world::IObject*);
    }
}


// Normalizes in place unless already within FLT_EPSILON of unit length.
// Degenerate vectors become zero.
void shadowNormalizeInPlace(Vec3& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return;
    if (std::fabs(lengthSquared) < FLT_EPSILON)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    v.x *= inverse;
    v.y *= inverse;
    v.z *= inverse;
}

// sub_100C8820 - re-orthonormalise a basis matrix in place, keeping its y row: rows 1 and 2 are
// normalised, then row 0 = row1 x row2 and row 2 = row0 x row1, each component a two-product
// difference in the shipped operand order. The w column is not touched.
void orthonormalizeShadowBasis(Mat4& basis)
{
    Vec3& row0 = *reinterpret_cast<Vec3*>(&basis.m[0]);
    Vec3& row1 = *reinterpret_cast<Vec3*>(&basis.m[4]);
    Vec3& row2 = *reinterpret_cast<Vec3*>(&basis.m[8]);
    shadowNormalizeInPlace(row1);
    shadowNormalizeInPlace(row2);
    row0.x = row2.z * row1.y - row2.y * row1.z;
    row0.y = row1.z * row2.x - row2.z * row1.x;
    row0.z = row2.y * row1.x - row1.y * row2.x;
    row2.x = row1.z * row0.y - row1.y * row0.z;
    row2.y = row1.x * row0.z - row1.z * row0.x;
    row2.z = row1.y * row0.x - row1.x * row0.y;
}

// The directional pass's light basis (0x10164E7B..0x10164F96 for the shadow frustum, and again at
// 0x1016548D..0x10165593 for the caster frustum): identity, the sky light's direction as the z row,
// the y row = direction x (1, 0, 0) inlined against the zero constant the `* 0.0f`
// products are real multiplies (they differ from 0 for an infinite or NaN direction, and carry the
// sign of zero) - and the orthonormalisation. LightSource +0x17C is asked twice, once per row.
void buildShadowLightBasis(world::LightSource* sky, Mat4& basis)
{
    basis = Mat4::getIdentity();
    {
        const Vec3& direction = sky->getDirection();                       // +0x17C
        basis.m[8] = direction.x;
        basis.m[9] = direction.y;
        basis.m[10] = direction.z;
    }
    {
        const Vec3& direction = sky->getDirection();                       // +0x17C, again
        const float xTimesZero = direction.x * 0.0f;                       // [ebp-0x10], a float
        basis.m[4] = direction.y * 0.0f - direction.z * 0.0f;
        basis.m[5] = direction.z - xTimesZero;
        basis.m[6] = xTimesZero - direction.y;
    }
    orthonormalizeShadowBasis(basis);
}

// One plane of the five fits (see the block comment above). 0x1016AFA0 alone sums corner 0's dot
// as (x + z) + y; every other corner of every fit sums (z + y) + x, and the re-seat's d is
// -((y + z) + x) - the PE's order (g3v_core). The re-seat
// also stores the normal back unchanged (`plane = Plane3(normal, corner)`), which is not repeated.
void fitShadowPlaneToCorners(Plane3& plane, const Vec3* corners, bool firstCornerXzy)
{
    const Vec3 normal = plane.normal;
    float maximum = -FLT_MAX;
    float maximumPlusD = -FLT_MAX;
    int best = -1;
    for (int i = 0; i < 8; ++i)
    {
        const Vec3& corner = corners[i];
        const float dot = (i == 0 && firstCornerXzy)
            ? normal.x * corner.x + corner.z * normal.z + normal.y * corner.y
            : corner.z * normal.z + corner.y * normal.y + normal.x * corner.x;
        if (dot > maximum)
        {
            best = i;
            maximumPlusD = dot + plane.d;
            maximum = dot;
        }
    }
    if (maximumPlusD >= 0.0f)                                              // `test ah, 1; jne`
    {
        const Vec3& corner = corners[best];
        plane.d = -(normal.y * corner.y + normal.z * corner.z + normal.x * corner.x);
    }
}

// sqrt((dz*dz + dy*dy) + dx*dx) with d = a - b - the shipped association of every extent below.
float shadowCornerDistance(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dz * dz + dy * dy + dx * dx);
}

// The extents tail every fit shares: the corners from the planes (0x10079E50, RendDX9's copy of
// Frustum::recalcCorners), width |c2 - c0| and height |c1 - c0| (the all-plane fits only), far
// |c4 - c0| - near, and the transformation re-seated at the near face's centre (c0 + c3) / 2 and
// re-applied through Frustum::transform (copy, then reSetup - setOrtho for these).
void refitShadowFrustumExtents(Frustum& frustum, bool widthAndHeight)
{
    frustum.recalcCorners();
    const Vec3* const corner = frustum.m_corners;
    if (widthAndHeight)
    {
        frustum.m_dword1D4 = shadowCornerDistance(corner[2], corner[0]);   // +0x1D4 width
        frustum.m_dword1D8 = shadowCornerDistance(corner[1], corner[0]);   // +0x1D8 height
    }
    frustum.m_farDist = shadowCornerDistance(corner[4], corner[0]) - frustum.m_nearDist;
    Mat4 transformation = frustum.m_transformation;
    transformation.m[12] = (corner[0].x + corner[3].x) * 0.5f;
    transformation.m[13] = (corner[3].y + corner[0].y) * 0.5f;
    transformation.m[14] = (corner[3].z + corner[0].z) * 0.5f;
    frustum.transform(transformation);
}

// sub_1016AB10 / sub_1016AFA0 / sub_1016B3C0 - all six planes, then width, height and far.
void fitShadowFrustumAllPlanes(Frustum& frustum, const Vec3* corners, bool firstCornerXzy)
{
    for (unsigned int plane = 0; plane < 6; ++plane)
        fitShadowPlaneToCorners(frustum.m_planes[plane], corners, firstCornerXzy);
    refitShadowFrustumExtents(frustum, true);
}

// sub_1016B660 / sub_1016B820 - the near plane (4) only, pulled back toward the light; width and
// height stay, far is re-derived.
void fitShadowFrustumNearPlane(Frustum& frustum, const Vec3* corners)
{
    fitShadowPlaneToCorners(frustum.m_planes[4], corners, false);
    refitShadowFrustumExtents(frustum, false);
}

// sub_100E7180 - an axis-aligned box's corner i: x from max when bit 2, y when bit 1, z when bit 0
// (an index past 7 answers zero; nothing here asks one). 0x1016B3C0 / 0x1016B660 ask it twice per
// corner and once more for the re-seat; the answers cannot differ, so the eight are taken once.
void shadowAabbCorners(const LocalBoundingBox& box, Vec3* corners)
{
    for (unsigned int i = 0; i < 8; ++i)
    {
        corners[i].x = (i & 4u) ? box.m_max.x : box.m_min.x;
        corners[i].y = (i & 2u) ? box.m_max.y : box.m_min.y;
        corners[i].z = (i & 1u) ? box.m_max.z : box.m_min.z;
    }
}

// sub_1016B3C0 - a frustum fitted, all six planes, to an axis-aligned box.
void fitShadowFrustumToAabb(Frustum& frustum, const LocalBoundingBox& box)
{
    Vec3 corners[8];
    shadowAabbCorners(box, corners);
    fitShadowFrustumAllPlanes(frustum, corners, false);
}

// sub_1016B660 - a frustum's near plane pulled back over an axis-aligned box.
void fitShadowFrustumNearPlaneToAabb(Frustum& frustum, const LocalBoundingBox& box)
{
    Vec3 corners[8];
    shadowAabbCorners(box, corners);
    fitShadowFrustumNearPlane(frustum, corners);
}

// sub_100CE120 / sub_101C6750 / sub_101C67F0 / sub_101C6C70 / sub_101C6E10 - the pass's 96-byte
// eight-corner object box. setBounds lays a box's min/max out in the fixed order (x bit 2, y bit 1,
// z bit 0); transform pushes every corner through the full matrix with the shipped per-corner
// operand order - corner 0's y and x rows associate differently from corners 1..7 (BundledMeshDraw
// .cpp's worldHeight reads 0x101C67F0 the same way; StaticMesh.cpp's StaticMeshBoundsCorners keeps
// corner 0's order for all eight); maximum()/minimum() replace a component only on a strict, ordered
// compare, so a NaN corner never wins.
struct ShadowBoxCorners
{
    Vec3 corner[8];

    // The zeroing half of 0x100CE120; the directional record zeroes the same 24 floats inline
    // (0x10165BD7..0x10165CBD) before it picks an arm.
    ShadowBoxCorners()
    {
        for (unsigned int i = 0; i < 8; ++i)
            corner[i].x = corner[i].y = corner[i].z = 0.0f;
    }

    // zero, then the two setters.
    ShadowBoxCorners(const LocalBoundingBox& bounds, const Mat4& world)
    {
        for (unsigned int i = 0; i < 8; ++i)
            corner[i].x = corner[i].y = corner[i].z = 0.0f;
        setBounds(bounds);
        transform(world);
    }


    void setBounds(const LocalBoundingBox& bounds)
    {
        for (unsigned int i = 0; i < 8; ++i)
        {
            corner[i].x = (i & 4u) ? bounds.m_max.x : bounds.m_min.x;
            corner[i].y = (i & 2u) ? bounds.m_max.y : bounds.m_min.y;
            corner[i].z = (i & 1u) ? bounds.m_max.z : bounds.m_min.z;
        }
    }

    // all three rows are computed from the untouched corner before any is stored.
    void transform(const Mat4& world)
    {
        const float* const m = world.m;
        for (unsigned int i = 0; i < 8; ++i)
        {
            const float x = corner[i].x;
            const float y = corner[i].y;
            const float z = corner[i].z;
            const float tz = z * m[10] + x * m[2] + y * m[6] + m[14];
            const float ty = i == 0 ? x * m[1] + y * m[5] + z * m[9] + m[13]
                                    : x * m[1] + z * m[9] + y * m[5] + m[13];
            const float tx = i == 0 ? z * m[8] + x * m[0] + y * m[4] + m[12]
                                    : y * m[4] + z * m[8] + x * m[0] + m[12];
            corner[i].x = tx;
            corner[i].y = ty;
            corner[i].z = tz;
        }
    }

    // 0x101C6C70 - `test ah, 5; jp`: a component moves only when it is strictly below the corner's.
    Vec3 maximum() const
    {
        Vec3 result = corner[0];
        for (unsigned int i = 1; i < 8; ++i)
        {
            if (result.x < corner[i].x) result.x = corner[i].x;
            if (result.y < corner[i].y) result.y = corner[i].y;
            if (result.z < corner[i].z) result.z = corner[i].z;
        }
        return result;
    }

    // 0x101C6E10 - `test ah, 41h; jne`: a component moves only when it is strictly above.
    Vec3 minimum() const
    {
        Vec3 result = corner[0];
        for (unsigned int i = 1; i < 8; ++i)
        {
            if (result.x > corner[i].x) result.x = corner[i].x;
            if (result.y > corner[i].y) result.y = corner[i].y;
            if (result.z > corner[i].z) result.z = corner[i].z;
        }
        return result;
    }
};

#if defined(_M_IX86)
static_assert(sizeof(ShadowBoxCorners) == 0x60, "the pass's object box is 96 bytes (0x100CE120)");
#endif

// sub_101C5EB0 on a LocalBoundingBox. MeshTemplate.cpp's meshBoundsFinalize IS that body, written
// against MeshBounds - the same 60-byte record under its RendDX9 name; the layouts are asserted equal.
void finalizeShadowBounds(LocalBoundingBox& bounds)
{
    meshBoundsFinalize(*reinterpret_cast<MeshBounds*>(&bounds));
}

#if defined(_M_IX86)
static_assert(sizeof(LocalBoundingBox) == sizeof(MeshBounds), "LocalBoundingBox is MeshBounds");
static_assert(offsetof(LocalBoundingBox, m_max) == offsetof(MeshBounds, max), "max");
static_assert(offsetof(LocalBoundingBox, m_derivedDataDirty) == offsetof(MeshBounds, dirty),
              "dirty");
static_assert(offsetof(LocalBoundingBox, m_centre) == offsetof(MeshBounds, center), "centre");
#endif

// RendDX9 this image's Frustum view matrix (the ELF body is empty [PLATFORM]):
// D3DXMatrixLookAtLH from the transformation's translation (+0x1A4), towards that point plus its
// z row (+0x194, summed into float stores), with its y row (+0x184) as up.
void shadowFrustumViewMatrix(const Frustum& frustum, Mat4& output)
{
    const Mat4& t = frustum.m_transformation;
    const Vec3 at = { t.m[8] + t.m[12], t.m[9] + t.m[13], t.m[10] + t.m[14] };
    ::D3DXMatrixLookAtLH(reinterpret_cast<D3DXMATRIX*>(&output),
                         reinterpret_cast<const D3DXVECTOR3*>(&t.m[12]),
                         reinterpret_cast<const D3DXVECTOR3*>(&at),
                         reinterpret_cast<const D3DXVECTOR3*>(&t.m[4]));
}

// RendDX9 this image's Frustum ortho projection (the ELF body is empty [PLATFORM]):
// D3DXMatrixOrthoLH(width +0x1D4, height +0x1D8, near +0x1DC, far +0x1E0).
void shadowFrustumOrthoProjection(const Frustum& frustum, Mat4& output)
{
    ::D3DXMatrixOrthoLH(reinterpret_cast<D3DXMATRIX*>(&output), frustum.m_dword1D4,
                        frustum.m_dword1D8, frustum.m_nearDist, frustum.m_farDist);
}

// The directional pass's pull toward the light (0x10166419..0x101664B9 for the followers,
// 0x10166A66..0x10166AFD per record in the quality-three map): the translation moves by d along the
// near plane's outward normal (planes[4] at +0x40, which setOrtho makes the negated z row, i.e.
// toward the light) and the far distance grows by d, stored straight into +0x1E0 before
// Frustum::transform (0x1007AAD0) re-runs the ortho set-up.
void shiftShadowFrustumTowardLight(Frustum& frustum, float distance)
{
    Mat4 transformation = frustum.m_transformation;
    const Vec3 normal = frustum.m_planes[4].normal;
    const float x = normal.x * distance;
    const float y = normal.y * distance;                                   // [ebp-0x1C]
    const float z = normal.z * distance;                                   // [ebp-0x18]
    transformation.m[12] = transformation.m[12] + x;
    transformation.m[13] = y + transformation.m[13];
    transformation.m[14] = z + transformation.m[14];
    frustum.m_farDist = frustum.m_farDist + distance;
    frustum.transform(transformation);
}

// Gathers directional-shadow candidates using the camera frustum and light basis.
// Caller-owned basis and sweep classifier remain valid through the entire shadow pass.
void buildShadowCandidateSet(void* self, Mat4& lightBasis, ShadowSweepClassifier& classifier)
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();   // renderer +0x48
    const Mat4& viewMatrix = *view->getViewMatrixPtr();                     // view +0x64
    const Vec3 cameraCentre =
        { viewMatrix.m[12], viewMatrix.m[13], viewMatrix.m[14] };
    world::LightSource* const sky = lmField<world::LightSource*>(self, 0x14);
    (void)sky->getAbsoluteDirection();                                      // +0x180, discarded

    Frustum& cameraFrustum = lmField<Frustum>(self, 0x460);
    cameraFrustum = *view->getFrustumPtr();                                 // +0x74,
    cameraFrustum.setFarDist(lmField<float>(self, 0x214));

    buildShadowLightBasis(sky, lightBasis);
    lightBasis.m[12] = lightBasis.m[12] + lmField<float>(self, 0x604);
    lightBasis.m[13] = lightBasis.m[13] + lmField<float>(self, 0x608);
    lightBasis.m[14] = lightBasis.m[14] + lmField<float>(self, 0x60C);

    Frustum& shadowFrustum = lmField<Frustum>(self, 0x278);
    shadowFrustum.setOrtho(1.0f, 1.0f, 0.01f, 1.0f, lightBasis);
    fitShadowFrustumAllPlanes(shadowFrustum, cameraFrustum.m_corners, false);

    // with the debug byte +0x788 set (constructor 0; its only writer is the
    // setter 0x1015C170 at vtable +0xCC, whose only caller is the retail console binding
    // `shadowManager.shadowShowFrustums` - executor 0x100C1D4F, name 0x10215CD4 (g3v_arm)) the shadow
    // frustum, then the camera frustum pulled back half a unit along its z row, are handed to the
    // DebugRenderer's +0x28. DebugRenderer has no recovered interface. The rebuilt client registers
    // no such binding and +0xCC is lm_unrecovered, so this and the two stops below cannot fire today.
    if (lmField<unsigned char>(self, 0x788))
    {
        RD9_FRONTIER_AT("dice::hfe::geom::LightManager::renderDirectionalShadowMaps "
                        "(DebugRenderer::drawFrustum - the +0x788 shadow/camera frustum draw)",
                        g_DebugRenderer, 0x28);
        std::abort();
    }

    vc71::vector_layout& candidates = lmField<vc71::vector_layout>(self, 0x914);
    LocalBoundingBox bounds;                                                // [ebp-0x2B8]
    constructShadowSweepClassifier(classifier, cameraFrustum,
                                   sky->getDirection());
    ShadowCasterPredicator predicateStorage;                                // PE: static 0x1023A120
    constructShadowCasterPredicator(predicateStorage);

#if BF2_PLUGIN_ABI_NATIVE
    std::vector<world::IObject*> hostObjects;
    world::ObjectManager* const manager =
        reinterpret_cast<world::ObjectManager*>(objectManager);
    if (lmField<unsigned char>(self, 0x170))
        manager->getDynamicObjectsFromGridRend(
            cameraCentre, lmField<float>(self, 0x214), hostObjects, &bounds,
            &predicateStorage, 2, false);
    else
        manager->getDynamicObjectsFromGridRend(
            cameraFrustum, hostObjects, &bounds, &predicateStorage, 2, false);
    for (std::vector<world::IObject*>::const_iterator it = hostObjects.begin();
         it != hostObjects.end(); ++it)
    {
        vc71::vectorRef<world::IObject*> out(&candidates);
        out.push_back(*it);
    }
#else
    world::ObjectManager* const manager =
        reinterpret_cast<world::ObjectManager*>(objectManager);
    world::ObjectPredicator* const typedPredicate =
        reinterpret_cast<world::ObjectPredicator*>(&predicateStorage);
    if (lmField<unsigned char>(self, 0x170))
        manager->getDynamicObjectsFromGridRend(
            cameraCentre, lmField<float>(self, 0x214), hostObjectVector(candidates),
            &bounds, typedPredicate, 2, false);
    else
        manager->getDynamicObjectsFromGridRend(
            cameraFrustum, hostObjectVector(candidates), &bounds,
            typedPredicate, 2, false);
#endif

    {
        // [TEMP] One-shot diagnostic in the style of LightManager::update's recovery line: the
        // size of the first candidate set the shadow-caster predicate produced, so a live run can
        // prove the shadow pass reached the predicate and show what it kept.
        static bool s_candidatesReported = false;
        if (!s_candidatesReported)
        {
            s_candidatesReported = true;
            ::renddx9abi::logf("LightManager: shadow-caster predicate live - the first candidate "
                               "set holds %u object(s)", vectorSize<world::IObject*>(candidates));
        }
    }

    // only the sphere arm filters, with the classifier built above. The
    // [TEMP] report sits between the query and the filter, where the old body had it.
    if (lmField<unsigned char>(self, 0x170))
        filterShadowCandidates(classifier, candidates);
}

struct XpLight;

struct XpCuller
{
    const void** vtable;
    XpLight*     light;
};

struct XpSpotCuller
{
    const void** vtable;
    XpLight*     light;
    Frustum*     frustum;
};

// PE Terrain's heap query object (ctor 0x100E3AE0) and its 0x110-byte quadtree node.  Names are
// [INFERRED]; every offset below and the two traversal contracts are instruction-proven from
// 0x100E6C70/0x100E6CB0 and 0x100E4370/0x100E4460.
struct TerrainPatchQueryNode
{
    void*                 payload;          // +0x00
    unsigned char         pad04[0x18];
    TerrainPatchQueryNode* children[4];      // +0x1C
    unsigned char         pad2C[0x24];
    LocalBoundingBox      bounds;            // +0x50
    unsigned char         pad8C[0x64];
    unsigned int          hasChildren;       // +0xF0
    unsigned char         active;            // +0xF4
    unsigned char         padF5[0x1B];
};

struct TerrainPatchQueryLayout
{
    unsigned char          pad00[8];
    TerrainPatchQueryNode* root;             // +0x08
    unsigned char          pad0C[0x38];
    vc71::vector_layout    results;          // +0x44
    const Frustum*         frustum;           // +0x54
    void*                  field58;
    const Vec3*            centre;            // +0x5C
    float                  radius;            // +0x60
    float                  radiusSquared;     // +0x64
    unsigned char          pad68[0x14];
};

// Both vectors retain their VC7.1 four-pointer layout even in the
// native plugin flavour because they are members of the renderer-owned XpLight object, not native
// host objects. Only an ObjectManager call crossing the DLL boundary needs flavour conversion.
struct XpLight
{
    /* +0x00 */ vc71::vector_layout records;       // elements are 0x498-byte render records
    /* +0x10 */ unsigned char       releaseWarning;
    /* +0x11 */ unsigned char       pad11[3];
    /* +0x14 */ vc71::vector_layout candidates;    // world::IObject*
    /* +0x24 */ world::LightSource* source;
    /* +0x28 */ unsigned char       active;
    /* +0x29 */ unsigned char       policy;
    /* +0x2A */ unsigned char       pad2A[2];
    /* +0x2C */ float               cameraDistance;
    /* +0x30 */ int                 category;       // 0 dynamic, 1 static
    /* +0x34 */ XpCuller*           culler;
};

// This record crosses the VC7.1 vector boundary, so
// its physical layout and its deep-copy ownership are both significant.  In
// particular, the caster set at +0x488 cannot be copied as twelve POD bytes:
// vector growth and the +0x7CC -> +0x7DC range append each clone its nodes.
struct XpRenderRecordLayout
{
    /* +0x000 */ float            atlasScaleOffset[4];
    /* +0x010 */ Frustum          frustum;
    /* +0x1F8 */ Mat4             matrices[9];
    /* +0x438 */ float            atlasBounds[4];
    /* +0x448 */ unsigned char    flags448[8];
    /* +0x450 */ void*            geometry;
    /* +0x454 */ world::LightSource* source;
    /* +0x458 */ unsigned char    pad458[0x30];
    /* +0x488 */ vc71::set_layout embeddedTree;
    /* +0x494 */ unsigned char    tail494[4];

    XpRenderRecordLayout()
        : frustum(), geometry(0), source(0)
    {
        ::memset(atlasScaleOffset, 0, sizeof(atlasScaleOffset));
        for (unsigned int i = 0; i < 9; ++i)
            matrices[i] = Mat4::getIdentity();
        ::memset(atlasBounds, 0, sizeof(atlasBounds));
        ::memset(flags448, 0, sizeof(flags448));
        ::memset(pad458, 0, sizeof(pad458));
        embeddedTree.construct();
        ::memset(tail494, 0, sizeof(tail494));
    }

    XpRenderRecordLayout(const XpRenderRecordLayout& other)
        : frustum(other.frustum), geometry(0), source(0)
    {
        // Frustum has no owned allocation.  Copying the instruction-proven
        // prefix byte-for-byte preserves all nine matrices and opaque flags.
        ::memcpy(this, &other, offsetof(XpRenderRecordLayout, embeddedTree));
        embeddedTree.construct();
        for (vc71::set_node* node = other.embeddedTree.begin();
             node != other.embeddedTree.end(); node = vc71::set_layout::next(node))
            embeddedTree.insert(node->key);
        ::memcpy(tail494, other.tail494, sizeof(tail494));
    }

    ~XpRenderRecordLayout()
    {
        embeddedTree.destroy();
    }

private:
    XpRenderRecordLayout& operator=(const XpRenderRecordLayout&);
};

#if defined(_M_IX86)
static_assert(sizeof(XpLight) == 0x38, "retail XpLight layout drift");
static_assert(sizeof(XpCuller) == 0x08, "retail Point culler layout drift");
static_assert(sizeof(XpSpotCuller) == 0x0C, "retail Spot culler layout drift");
static_assert(sizeof(XpRenderRecordLayout) == 0x498, "retail Xp render-record layout drift");
static_assert(offsetof(XpRenderRecordLayout, frustum) == 0x10,
              "Xp render-record Frustum offset drift");
static_assert(offsetof(XpRenderRecordLayout, matrices) == 0x1F8,
              "Xp render-record matrix offset drift");
static_assert(offsetof(XpRenderRecordLayout, atlasBounds) == 0x438,
              "Xp render-record atlas bounds offset drift");
static_assert(offsetof(XpRenderRecordLayout, geometry) == 0x450,
              "Xp render-record geometry offset drift");
static_assert(offsetof(XpRenderRecordLayout, source) == 0x454,
              "Xp render-record source offset drift");
static_assert(offsetof(XpRenderRecordLayout, embeddedTree) == 0x488,
              "Xp render-record caster-set offset drift");
static_assert(sizeof(TerrainPatchQueryNode) == 0x110, "terrain-query node layout drift");
static_assert(offsetof(TerrainPatchQueryNode, children) == 0x1C, "terrain children offset drift");
static_assert(offsetof(TerrainPatchQueryNode, bounds) == 0x50, "terrain bounds offset drift");
static_assert(offsetof(TerrainPatchQueryNode, hasChildren) == 0xF0,
              "terrain branch flag offset drift");
static_assert(offsetof(TerrainPatchQueryNode, active) == 0xF4,
              "terrain active flag offset drift");
static_assert(sizeof(TerrainPatchQueryLayout) == 0x7C, "terrain-query layout drift");
static_assert(offsetof(TerrainPatchQueryLayout, root) == 0x08, "terrain root offset drift");
static_assert(offsetof(TerrainPatchQueryLayout, results) == 0x44, "terrain results offset drift");
static_assert(offsetof(TerrainPatchQueryLayout, frustum) == 0x54,
              "terrain frustum offset drift");
static_assert(offsetof(TerrainPatchQueryLayout, centre) == 0x5C,
              "terrain centre offset drift");
static_assert(offsetof(TerrainPatchQueryLayout, radius) == 0x60, "terrain radius offset drift");
static_assert(offsetof(TerrainPatchQueryLayout, radiusSquared) == 0x64,
              "terrain radius-squared offset drift");
#endif

struct ShadowProjectionLayout
{
    /* +0x000 */ Mat4    transform;
    /* +0x040 */ float   current[6];
    /* +0x058 */ float   saved[6];
    /* +0x070 */ float   fallback[6];
    /* +0x088 */ Plane3  axisZ;
    /* +0x098 */ Plane3  axisY;
    /* +0x0A8 */ Plane3  axisX;
    /* +0x0B8 */ unsigned char perspectiveAdjust;
    /* +0x0B9 */ unsigned char dirty;
    /* +0x0BA */ unsigned char padBA[2];
    /* +0x0BC */ Frustum cachedFrustum;

    ShadowProjectionLayout(const Mat4& value, bool adjust)
        : transform(Mat4::getIdentity()), perspectiveAdjust(adjust ? 1 : 0), dirty(1),
          cachedFrustum()
    {
        ::memset(current, 0, sizeof(current));
        ::memset(saved, 0, sizeof(saved));
        ::memset(fallback, 0, sizeof(fallback));
        ::memset(&axisZ, 0, sizeof(axisZ));
        ::memset(&axisY, 0, sizeof(axisY));
        ::memset(&axisX, 0, sizeof(axisX));
        padBA[0] = padBA[1] = 0;
        setAngularLimits(0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 1.0f, fallback);
        setTransform(value);
    }

    static float clampValue(float value, float minimum, float maximum)
    {
        return value < minimum ? minimum : (value > maximum ? maximum : value);
    }

    static void setAngularLimits(float left, float right, float bottom, float top,
                                 float nearDistance, float farDistance, float* destination)
    {
        destination[0] = std::sin(left);
        destination[1] = std::sin(right);
        destination[2] = std::sin(bottom);
        destination[3] = std::sin(top);
        destination[4] = nearDistance * nearDistance;
        destination[5] = farDistance * farDistance;
    }

    static Vec3 normalized(const Vec3& value)
    {
        const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (lengthSquared <= FLT_EPSILON)
        {
            const Vec3 zero = { 0.0f, 0.0f, 0.0f };
            return zero;
        }
        const float inverse = 1.0f / std::sqrt(lengthSquared);
        const Vec3 result = { value.x * inverse, value.y * inverse, value.z * inverse };
        return result;
    }

    void setTransform(const Mat4& value)
    {
        Plane3 z = { { 0.0f, 0.0f, 1.0f }, 0.0f };
        Plane3 y = { { 0.0f, 1.0f, 0.0f }, 0.0f };
        Plane3 x = { { 1.0f, 0.0f, 0.0f }, 0.0f };
        axisZ = Plane3::transform(z, value);
        axisY = Plane3::transform(y, value);
        axisX = Plane3::transform(x, value);
        axisZ.normal = normalized(axisZ.normal);
        axisY.normal = normalized(axisY.normal);
        axisX.normal = normalized(axisX.normal);
        transform = value;
        current[0] = 2.0f;
        current[1] = -2.0f;
        current[2] = 2.0f;
        current[3] = -2.0f;
        current[4] = FLT_MAX;
        current[5] = 0.0f;
        dirty = 1;
    }

    void setPerspectiveLimits(float left, float right, float bottom, float top,
                              float nearDistance, float farDistance)
    {
        // PE 0x101BABD0 evaluates sqrt(2*a*a)*.75 separately for every signed angle.
        const float scale = std::sqrt(2.0f) * 0.75f;
        setAngularLimits(left * scale, right * scale, bottom * scale, top * scale,
                         nearDistance, farDistance, saved);
    }

    void copyCurrentToSaved(const ShadowProjectionLayout& other)
    {
        ::memcpy(saved, other.current, sizeof(saved));
    }

    void restoreAll()
    {
        ::memcpy(current, saved, sizeof(current));
        dirty = 1;
    }

    void restoreSelected(bool left, bool right, bool bottom, bool top,
                         bool nearDistance, bool farDistance)
    {
        const bool flags[6] = { left, right, bottom, top, nearDistance, farDistance };
        for (unsigned int i = 0; i < 6; ++i)
        {
            if (flags[i])
                current[i] = saved[i];
        }
        dirty = 1;
    }

    bool includePoint(const Vec3& point)
    {
        Vec3 delta = { point.x - transform.m[12], point.y - transform.m[13],
                       point.z - transform.m[14] };
        Vec3 horizontal = {
            delta.x - axisY.normal.x * dot3(delta, axisY.normal),
            delta.y - axisY.normal.y * dot3(delta, axisY.normal),
            delta.z - axisY.normal.z * dot3(delta, axisY.normal)
        };
        Vec3 vertical = {
            delta.x - axisX.normal.x * dot3(delta, axisX.normal),
            delta.y - axisX.normal.y * dot3(delta, axisX.normal),
            delta.z - axisX.normal.z * dot3(delta, axisX.normal)
        };

        const float distanceSquared = dot3(delta, delta);
        bool inside = distanceSquared >= saved[4] && distanceSquared <= saved[5];
        const float clampedDistance = clampValue(distanceSquared, saved[4], saved[5]);
        float nearScale = 1.0f;
        if (perspectiveAdjust)
        {
            float maximum = saved[0];
            maximum = (std::max)(maximum, saved[1]);
            maximum = (std::max)(maximum, saved[2]);
            maximum = (std::max)(maximum, saved[3]);
            nearScale = maximum * maximum;
        }
        current[4] = (std::min)(current[4], clampedDistance * nearScale);
        current[5] = (std::max)(current[5], clampedDistance);

        horizontal = normalized(horizontal);
        vertical = normalized(vertical);
        float horizontalValue = dot3(horizontal, axisX.normal);
        float verticalValue = dot3(vertical, axisY.normal);
        if (horizontalValue < saved[0] || horizontalValue > saved[1]
            || verticalValue < saved[2] || verticalValue > saved[3])
            inside = false;
        horizontalValue = clampValue(horizontalValue, saved[0], saved[1]);
        verticalValue = clampValue(verticalValue, saved[2], saved[3]);
        current[0] = (std::min)(current[0], horizontalValue);
        current[1] = (std::max)(current[1], horizontalValue);
        current[2] = (std::min)(current[2], verticalValue);
        current[3] = (std::max)(current[3], verticalValue);
        dirty = 1;
        return inside;
    }

    void sanitizedBounds(float* output) const
    {
        ::memcpy(output, current, sizeof(current));
        if (output[0] > 1.0f)  output[0] = fallback[0];
        if (output[1] < -1.0f) output[1] = fallback[1];
        if (output[2] > 1.0f)  output[2] = fallback[2];
        if (output[3] < -1.0f) output[3] = fallback[3];
        if (output[4] >= FLT_MAX) output[4] = fallback[4];
        if (output[5] < output[4])
        {
            const float adjustedFar = std::sqrt(fallback[5]) + std::sqrt(output[4]);
            output[5] = adjustedFar * adjustedFar;
        }
    }

    void getExtents(float* output) const
    {
        float bounds[6];
        sanitizedBounds(bounds);
        const float a0 = std::acos(clampValue(bounds[0], -1.0f, 1.0f));
        const float a1 = std::acos(clampValue(bounds[1], -1.0f, 1.0f));
        const float a2 = std::acos(clampValue(bounds[2], -1.0f, 1.0f));
        const float a3 = std::acos(clampValue(bounds[3], -1.0f, 1.0f));
        output[0] = std::fabs(a1 - a0);
        output[1] = std::fabs(a3 - a2);
        output[2] = std::sqrt(bounds[4]);
        output[3] = std::sqrt(bounds[5]);
        if (!perspectiveAdjust)
            output[2] *= std::cos((std::max)(output[0], output[1]) * 0.5f);
        output[2] = (std::min)(output[2], 1.0f);
        output[3] += 3.0f;
    }

    static Mat4 yawPitchRoll(float yaw, float pitch, float roll)
    {
        const float sy = std::sin(yaw),   cy = std::cos(yaw);
        const float sp = std::sin(pitch), cp = std::cos(pitch);
        const float sr = std::sin(roll),  cr = std::cos(roll);
        Mat4 result = Mat4::getIdentity();
        result.m[0]  = sr * sp * sy + cr * cy;
        result.m[1]  = sr * cp;
        result.m[2]  = sr * sp * cy - cr * sy;
        result.m[4]  = cr * sp * sy - sr * cy;
        result.m[5]  = cr * cp;
        result.m[6]  = cr * sp * cy + sr * sy;
        result.m[8]  = cp * sy;
        result.m[9]  = -sp;
        result.m[10] = cp * cy;
        return result;
    }

    const Frustum& getFrustum()
    {
        if (!dirty)
            return cachedFrustum;
        float bounds[6];
        sanitizedBounds(bounds);
        const float left = std::acos(clampValue(bounds[0], -1.0f, 1.0f));
        const float right = std::acos(clampValue(bounds[1], -1.0f, 1.0f));
        const float bottom = std::acos(clampValue(bounds[2], -1.0f, 1.0f));
        const float top = std::acos(clampValue(bounds[3], -1.0f, 1.0f));
        float extents[4];
        getExtents(extents);
        const float yaw = 1.5707963267948966f - (left + right) * 0.5f;
        const float pitch = (bottom + top) * 0.5f - 1.5707963267948966f;
        const Mat4 rotation = yawPitchRoll(yaw, pitch, 0.0f);
        const Mat4 composed = rotation * transform;
        float aspect = 1.0f;
        if (extents[0] > FLT_EPSILON)
            aspect = extents[1] / extents[0];
        if (extents[1] > FLT_EPSILON)
            cachedFrustum.setPerspective(extents[1], aspect, extents[2], extents[3], composed);
        else
            cachedFrustum.setOrtho(1.0f, 1.0f, extents[2], extents[3], composed);
        dirty = 0;
        return cachedFrustum;
    }
};

struct XpTemporaryRecord
{
    XpRenderRecordLayout record;
    ShadowProjectionLayout projection;

    XpTemporaryRecord(const Mat4& transform, bool adjust)
        : record(), projection(transform, adjust)
    {
    }
};

#if defined(_M_IX86)
static_assert(sizeof(ShadowProjectionLayout) == 0x2A4,
              "retail ShadowProjection layout drift");
static_assert(offsetof(ShadowProjectionLayout, cachedFrustum) == 0xBC,
              "ShadowProjection Frustum offset drift");
static_assert(sizeof(XpTemporaryRecord) == 0x73C,
              "retail temporary shadow-record layout drift");
#endif

struct XpCoordinator
{
    std::map<world::LightSource*, XpLight*> bySource;
    std::vector<XpLight*> iterationOrder;
    std::set<XpLight*> staticLights;
    std::set<XpLight*> dynamicLights;
    std::vector<XpLight*> staticFrame;
    std::vector<XpLight*> dynamicFrame;
    std::multimap<void*, world::LightSource*> geometryPairs;
};

XpCoordinator g_xpCoordinator;

inline LightManager* xpLightManager()
{
    return g_LightManager;
}

inline float squaredDistance(const Vec3& a, const Vec3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

inline Vec3 matrixTranslation(const Mat4& matrix)
{
    Vec3 value;
    value.x = matrix.m[12];
    value.y = matrix.m[13];
    value.z = matrix.m[14];
    return value;
}

inline float xpEffectiveRadius(const XpLight* light)
{
    float radius = light->source->getVisualBoundingRadius();
    if (lightSourceTemplatePointer(light->source)->getIsDynamic())
        radius *= xpLightManager()->getDynamicLightRangeScale();
    return radius;
}

void appendCandidate(vc71::vector_layout& destination, world::IObject* object)
{
    vc71::vectorRef<world::IObject*> out(&destination);
    out.push_back(object);
}

void pointQueryObjects(const Vec3& centre, float radius, vc71::vector_layout& destination)
{
#if BF2_PLUGIN_ABI_NATIVE
    std::vector<world::IObject*> hostObjects;
    reinterpret_cast<world::ObjectManager*>(objectManager)->getObjectsFromGrid(
        centre, radius, hostObjects, 0);
    for (std::vector<world::IObject*>::const_iterator it = hostObjects.begin();
         it != hostObjects.end(); ++it)
        appendCandidate(destination, *it);
#else
    reinterpret_cast<world::ObjectManager*>(objectManager)->getObjectsFromGrid(
        centre, radius, hostObjectVector(destination), 0);
#endif
}

void spotQueryObjects(const Frustum& frustum, vc71::vector_layout& destination)
{
#if BF2_PLUGIN_ABI_NATIVE
    std::vector<world::IObject*> hostObjects;
    reinterpret_cast<world::ObjectManager*>(objectManager)->getObjectsFromGrid(
        frustum, hostObjects, 0, false, 0.0f, 0.0f);
    for (std::vector<world::IObject*>::const_iterator it = hostObjects.begin();
         it != hostObjects.end(); ++it)
        appendCandidate(destination, *it);
#else
    reinterpret_cast<world::ObjectManager*>(objectManager)->getObjectsFromGrid(
        frustum, hostObjectVector(destination), 0, false, 0.0f, 0.0f);
#endif
}

bool localControlledObject(world::IObject*& controlled)
{
    world::IPlayer* const player =
        reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer();
    if (!player)
    {
        controlled = 0;
        return false;
    }
    controlled = player->getVehicle();
    return true;
}

void pointCollect(XpCuller* culler, double, vc71::vector_layout& destination, int)
{
    const Vec3& position = culler->light->source->getAbsolutePosition();
    const float radius = xpEffectiveRadius(culler->light);
    pointQueryObjects(position, radius, destination);

    world::IObject* controlled = 0;
    if (!localControlledObject(controlled))
        return;

    const Vec3& controlledPosition = controlled->getAbsolutePosition();
    const Vec3& sourcePosition = culler->light->source->getAbsolutePosition();
    if (std::sqrt(squaredDistance(controlledPosition, sourcePosition)) < radius)
        appendCandidate(destination, controlled);
}

void spotCollect(XpSpotCuller* culler, double, vc71::vector_layout& destination, int)
{
    spotQueryObjects(*culler->frustum, destination);
}

bool pointVisible(XpCuller* culler, const Frustum& camera)
{
    return camera.inside(culler->light->source->getAbsolutePosition(),
                         culler->light->source->getVisualBoundingRadius());
}

bool spotVisible(XpSpotCuller* culler, const Frustum& camera)
{
    // PE 0x101BD0C0 builds a BoundingBox (eight Vec3 corners) from the spotlight Frustum's local
    // box, transforms all eight by that Frustum's matrix, and accepts unless all corners lie
    // outside one camera plane. This is conservative intersection, not containment.
    Vec3 corners[8];
    const LocalBoundingBox& local = culler->frustum->m_boundingBox;
    const Mat4& transform = culler->frustum->m_transformation;
    for (int i = 0; i < 8; ++i)
    {
        Vec3 point;
        point.x = (i & 1) ? local.m_max.x : local.m_min.x;
        point.y = (i & 2) ? local.m_max.y : local.m_min.y;
        point.z = (i & 4) ? local.m_max.z : local.m_min.z;
        corners[i].x = transform.m[0] * point.x + transform.m[4] * point.y
                     + transform.m[8] * point.z + transform.m[12];
        corners[i].y = transform.m[1] * point.x + transform.m[5] * point.y
                     + transform.m[9] * point.z + transform.m[13];
        corners[i].z = transform.m[2] * point.x + transform.m[6] * point.y
                     + transform.m[10] * point.z + transform.m[14];
    }

    for (int planeIndex = 0; planeIndex < 6; ++planeIndex)
    {
        const Plane3& plane = camera.m_planes[planeIndex];
        bool allOutside = true;
        for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
        {
            const Vec3& point = corners[cornerIndex];
            const float distance = plane.normal.x * point.x
                                 + plane.normal.y * point.y
                                 + plane.normal.z * point.z + plane.d;
            if (distance <= 0.0f)
            {
                allOutside = false;
                break;
            }
        }
        if (allOutside)
            return false;
    }
    return true;
}

void terrainQuerySphereWalk(TerrainPatchQueryLayout* query, TerrainPatchQueryNode* node)
{
    if (node->hasChildren)
    {
        for (unsigned int i = 0; i < 4; ++i)
        {
            TerrainPatchQueryNode* const child = node->children[i];
            if (child->active)
                terrainQuerySphereWalk(query, child);
        }
        return;
    }

    // The shipped leaf body asks the dirty byte twice; calcRadius clears it on the first call.
    if (node->bounds.m_derivedDataDirty)
        node->bounds.calcRadius();
    const float nodeRadius = node->bounds.m_radiusFromCentre;
    if (node->bounds.m_derivedDataDirty)
        node->bounds.calcRadius();

    const float totalRadius = nodeRadius + query->radius;
    if (totalRadius > std::sqrt(squaredDistance(*query->centre, node->bounds.m_centre))
        && node->payload)
    {
        vc71::vectorRef<TerrainPatchQueryNode*> output(&query->results);
        output.push_back(node);
    }
}

void terrainQueryFrustumWalk(TerrainPatchQueryLayout* query, TerrainPatchQueryNode* node)
{
    if (node->hasChildren)
    {
        for (unsigned int i = 0; i < 4; ++i)
        {
            TerrainPatchQueryNode* const child = node->children[i];
            if (child->active)
                terrainQueryFrustumWalk(query, child);
        }
        return;
    }

    if (node->bounds.m_derivedDataDirty)
        node->bounds.calcRadius();
    const float nodeRadius = node->bounds.m_radiusFromCentre;
    if (node->bounds.m_derivedDataDirty)
        node->bounds.calcRadius();
    if (query->frustum->inside(node->bounds.m_centre, nodeRadius) && node->payload)
    {
        vc71::vectorRef<TerrainPatchQueryNode*> output(&query->results);
        output.push_back(node);
    }
}

void terrainQuerySphere(TerrainPatchQueryLayout* query, const Vec3* centre, float radius)
{
    clearPointerVector(query->results);
    query->centre = centre;
    query->radius = radius;
    query->radiusSquared = radius * radius;
    if (query->root->active)
        terrainQuerySphereWalk(query, query->root);
}

void terrainQueryFrustum(TerrainPatchQueryLayout* query, const Frustum* frustum)
{
    clearPointerVector(query->results);
    query->frustum = frustum;
    if (query->root->active)
        terrainQueryFrustumWalk(query, query->root);
}

void __fastcall pointCollectSlot(void* self, void*, double time,
                                 vc71::vector_layout* destination, int flag)
{
    pointCollect(static_cast<XpCuller*>(self), time, *destination, flag);
}
void __fastcall spotCollectSlot(void* self, void*, double time,
                                vc71::vector_layout* destination, int flag)
{
    spotCollect(static_cast<XpSpotCuller*>(self), time, *destination, flag);
}
bool __fastcall pointVisibleSlot(void* self, void*, const Frustum* camera)
{
    return pointVisible(static_cast<XpCuller*>(self), *camera);
}
bool __fastcall spotVisibleSlot(void* self, void*, const Frustum* camera)
{
    return spotVisible(static_cast<XpSpotCuller*>(self), *camera);
}
// Both retail +0x08 bodies are `xor al,al; ret 4`: one opaque stack argument is consumed even
// though the result is always false.  Keep that argument so an indirect call cannot unbalance esp.
bool __fastcall cullerFalseSlot(void*, void*, void*) { return false; }
void __fastcall pointSecondCullSlot(void* self, void*, TerrainPatchQueryLayout* query)
{
    XpCuller* const culler = static_cast<XpCuller*>(self);
    const Vec3& position = culler->light->source->getAbsolutePosition();
    terrainQuerySphere(query, &position,
                       culler->light->source->getVisualBoundingRadius());
}
void __fastcall spotSecondCullSlot(void* self, void*, TerrainPatchQueryLayout* query)
{
    terrainQueryFrustum(query, static_cast<XpSpotCuller*>(self)->frustum);
}
void __fastcall pointFillRecordSlot(void*, void*, unsigned char* record)
{
    *reinterpret_cast<float*>(record + 0x3C) = 1.0f;
    *reinterpret_cast<float*>(record + 0x40) = 1.0f;
}
void __fastcall spotFillRecordSlot(void* self, void*, unsigned char* record)
{
    XpSpotCuller* const culler = static_cast<XpSpotCuller*>(self);
    const float degreesToRadians = 0.01745329251994329577f;
    *reinterpret_cast<float*>(record + 0x3C) =
        std::cos(culler->light->source->getConeAngle1() * degreesToRadians);
    *reinterpret_cast<float*>(record + 0x40) =
        std::cos(culler->light->source->getConeAngle2() * degreesToRadians);
}
void __fastcall pointDebugSlot(void*, void*) {}
void __fastcall spotDebugSlot(void* self, void*)
{
    XpSpotCuller* const culler = static_cast<XpSpotCuller*>(self);
    (void)culler;
    // [FRONTIER] DebugRenderer has no recovered source interface.  The PE caller proves +0x28,
    // but declaring unnamed prefix slots would merely disguise another positional dispatch.
    RD9_FRONTIER_AT("dice::hfe::geom::LightManager::spotDebug "
                    "(DebugRenderer::drawFrustum)", g_DebugRenderer, 0x28);
    std::abort();
}

const void* const kPointCullerVtable[6] =
{
    reinterpret_cast<void*>(&pointCollectSlot),
    reinterpret_cast<void*>(&pointVisibleSlot),
    reinterpret_cast<void*>(&cullerFalseSlot),
    reinterpret_cast<void*>(&pointSecondCullSlot),
    reinterpret_cast<void*>(&pointFillRecordSlot),
    reinterpret_cast<void*>(&pointDebugSlot)
};

const void* const kSpotCullerVtable[6] =
{
    reinterpret_cast<void*>(&spotCollectSlot),
    reinterpret_cast<void*>(&spotVisibleSlot),
    reinterpret_cast<void*>(&cullerFalseSlot),
    reinterpret_cast<void*>(&spotSecondCullSlot),
    reinterpret_cast<void*>(&spotFillRecordSlot),
    reinterpret_cast<void*>(&spotDebugSlot)
};

void computeXpPolicy(XpLight* light)
{
    light->policy = 1;
    if (!dx9rend::rendererIsExpansionPack())
    {
        light->policy = 0;
        return;
    }

    const world::LightSourceTemplate* const objectTemplate =
        lightSourceTemplatePointer(light->source);
    const bool flag800 = (objectTemplate->getFlags() & 0x800u) != 0;
    if (((!flag800 && objectTemplate->getIsDynamic())
         || (flag800 && !objectTemplate->getIsDynamic()))
        && dx9rend::rendererGetVideoOptions()->getQuality(4) < 3)
        light->policy = 0;
}

void constructXpLight(XpLight* light, world::LightSource* source)
{
    ::memset(light, 0, sizeof(*light));
    light->source = source;
    light->cameraDistance = 1000000.0f; // exact 0x49742400 constructor constant
    const world::LightSourceTemplate* const sourceTemplate =
        lightSourceTemplatePointer(source);
    light->category = sourceTemplate->getIsDynamic() ? 0 : 1;
    source->addRef();

    const int type = sourceTemplate->getLightTypeEnum();
    if (type == 1)
    {
        XpCuller* const culler = static_cast<XpCuller*>(
            ::renddx9abi::engineAlloc(sizeof(XpCuller)));
        culler->vtable = const_cast<const void**>(kPointCullerVtable);
        culler->light = light;
        light->culler = culler;
    }
    else if (type == 2)
    {
        XpSpotCuller* const culler = static_cast<XpSpotCuller*>(
            ::renddx9abi::engineAlloc(sizeof(XpSpotCuller)));
        culler->vtable = const_cast<const void**>(kSpotCullerVtable);
        culler->light = light;
        culler->frustum = static_cast<Frustum*>(::renddx9abi::engineAlloc(sizeof(Frustum)));
        new (culler->frustum) Frustum;
        culler->frustum->setPerspective(
            sourceTemplate->getConeAngle1(), 1.0f, 0.0f, 100.0f,
            source->getAbsoluteTransformation());
        light->culler = reinterpret_cast<XpCuller*>(culler);
    }

    computeXpPolicy(light);
}

void destroyXpLight(XpLight* light)
{
    // Shipped order is significant. In particular, the Spot adapter is raw-freed without running
    // its destructor, leaking the heap Frustum at +8; reproduce that defect.
    light->source->release();
    ::renddx9abi::engineFree(light->culler);
    if (light->releaseWarning)
        ::renddx9abi::logf("XpLight::~XpLight: Need someway of releasing the lightsource.");
    destroyVectorStorage(light->candidates);

    XpRenderRecordLayout* const records = vectorBegin<XpRenderRecordLayout>(light->records);
    const unsigned int recordCount = vectorSize<XpRenderRecordLayout>(light->records);
    for (unsigned int i = 0; i < recordCount; ++i)
        records[i].~XpRenderRecordLayout();
    destroyVectorStorage(light->records);
}

void registerWithCollector(XpLight* light)
{
    if (light->category == 0)
    {
        g_xpCoordinator.dynamicLights.insert(light);
        return;
    }
    if (light->category == 1)
    {
        if ((lightSourceFlags(light->source) & 1u) == 0
            || light->source->getEnabled())
        {
            g_xpCoordinator.staticLights.insert(light);
            light->source->setEnabled(false);
        }
        return;
    }

    ::renddx9abi::logf("XpLightManager::addLight: invalid XpLight category %d", light->category);
}

void unregisterFromCollector(XpLight* light)
{
    if (light->category == 0)
        g_xpCoordinator.dynamicLights.erase(light);
    else if (light->category == 1)
        g_xpCoordinator.staticLights.erase(light);
    else
        ::renddx9abi::logf("XpLightManager::removeLight: invalid XpLight category %d",
                           light->category);
}

// PE 0x100EF2B0, reached by manager +0x94 for Point/Spot lights. Eligibility, duplicate handling,
// 0x38-byte construction, collector classification and map publication preserve shipped order.
void lmRegisterXpLight(world::LightSource* light)
{
    const world::LightSourceTemplate* const objectTemplate =
        lightSourceTemplatePointer(light);

    if (!objectTemplate->getEnabled() && !objectTemplate->getIsDynamic())
        return;

    const int firstType = objectTemplate->getLightTypeEnum();
    if (firstType != 1 && objectTemplate->getLightTypeEnum() != 2)
        return;

    if ((objectTemplate->getFlags() & 0x800u) == 0 && !objectTemplate->getIsDynamic())
        return;

    if (!dx9rend::rendererIsExpansionPack() && !objectTemplate->getIsDynamic())
        return;

    if (g_xpCoordinator.bySource.find(light) != g_xpCoordinator.bySource.end())
        return;

    XpLight* const xp = static_cast<XpLight*>(::renddx9abi::engineAlloc(sizeof(XpLight)));
    constructXpLight(xp, light);
    registerWithCollector(xp);
    g_xpCoordinator.bySource.insert(std::make_pair(light, xp));
    g_xpCoordinator.iterationOrder.push_back(xp);
}

void lmRemoveXpLight(world::LightSource* light)
{
    std::map<world::LightSource*, XpLight*>::iterator found =
        g_xpCoordinator.bySource.find(light);
    if (found == g_xpCoordinator.bySource.end())
        return;

    XpLight* const xp = found->second;
    g_xpCoordinator.bySource.erase(found);
    unregisterFromCollector(xp);
    std::vector<XpLight*>::iterator ordered =
        std::find(g_xpCoordinator.iterationOrder.begin(),
                  g_xpCoordinator.iterationOrder.end(), xp);
    if (ordered != g_xpCoordinator.iterationOrder.end())
        g_xpCoordinator.iterationOrder.erase(ordered);
    destroyXpLight(xp);
    ::renddx9abi::engineFree(xp);
}

void updateXpLight(XpLight* light)
{
    // PE 0x1018FE10 deliberately exempts the shipped default class value (1) from the disabled
    // source early-out. Static XpLights are therefore still considered after their registration
    // path invokes source->setEnabled(false).
    if ((lightSourceFlags(light->source) & 1u) != 0
        && !light->source->getEnabled()
        && light->category != 1)
    {
        light->active = 0;
        return;
    }

    bool visible = true;
    if (dx9rend::g_rendererBase)
    {
        dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();
        const Vec3 camera = matrixTranslation(*view->getViewMatrixPtr());
        const Vec3 source = matrixTranslation(
            light->source->getAbsoluteTransformation());
        light->cameraDistance = std::sqrt(squaredDistance(camera, source));
        visible = light->cameraDistance <= 100.0f; // PE global 0x1023A6F8 default
        if (visible)
        {
            const Frustum& cameraFrustum = *view->getFrustumPtr();
            const int type = lightSourceTemplatePointer(light->source)->getLightTypeEnum();
            if (type == 1)
                visible = pointVisible(light->culler, cameraFrustum);
            else if (type == 2)
                visible = spotVisible(reinterpret_cast<XpSpotCuller*>(light->culler),
                                      cameraFrustum);
        }
    }

    // PE BSS byte 0x102637E0 can force an otherwise rejected light active. Its shipped default is
    // false and no recovered console/property writer has yet been identified.
    static const bool forceVisible = false;
    if (!visible && forceVisible)
        visible = true;
    light->active = visible ? 1 : 0;
}

void clearXpRecords(XpLight* light)
{
    XpRenderRecordLayout* const records = vectorBegin<XpRenderRecordLayout>(light->records);
    const unsigned int recordCount = vectorSize<XpRenderRecordLayout>(light->records);
    for (unsigned int i = 0; i < recordCount; ++i)
        records[i].~XpRenderRecordLayout();
    light->records.last = light->records.first;
}

void prepareXpCandidates(XpLight* light, double time, int flag)
{
    clearPointerVector(light->candidates);
    clearXpRecords(light);
    if (light->active)
    {
        const int type = lightSourceTemplatePointer(light->source)->getLightTypeEnum();
        if (type == 1)
            pointCollect(light->culler, time, light->candidates, flag);
        else if (type == 2)
            spotCollect(reinterpret_cast<XpSpotCuller*>(light->culler),
                        time, light->candidates, flag);
    }
    if (vectorSize<world::IObject*>(light->candidates) == 0)
        light->active = 0;
}

bool nearerXpLight(const XpLight* lhs, const XpLight* rhs)
{
    return lhs->cameraDistance < rhs->cameraDistance;
}

void updateStaticCollector(double time)
{
    g_xpCoordinator.staticFrame.clear();
    for (std::set<XpLight*>::const_iterator it = g_xpCoordinator.staticLights.begin();
         it != g_xpCoordinator.staticLights.end(); ++it)
    {
        if ((*it)->active)
            g_xpCoordinator.staticFrame.push_back(*it);
    }
    std::sort(g_xpCoordinator.staticFrame.begin(), g_xpCoordinator.staticFrame.end(),
              nearerXpLight);
    while (g_xpCoordinator.staticFrame.size() > 3u) // PE 0x1023A750 default
        g_xpCoordinator.staticFrame.pop_back();
    for (std::vector<XpLight*>::iterator it = g_xpCoordinator.staticFrame.begin();
         it != g_xpCoordinator.staticFrame.end(); ++it)
        prepareXpCandidates(*it, time, 1);
}

void updateDynamicCollector(double time)
{
    g_xpCoordinator.dynamicFrame.clear();
    for (std::set<XpLight*>::const_iterator it = g_xpCoordinator.dynamicLights.begin();
         it != g_xpCoordinator.dynamicLights.end(); ++it)
    {
        if ((*it)->active)
            g_xpCoordinator.dynamicFrame.push_back(*it);
    }
    std::sort(g_xpCoordinator.dynamicFrame.begin(), g_xpCoordinator.dynamicFrame.end(),
              nearerXpLight);
    while (g_xpCoordinator.dynamicFrame.size() > 3u) // PE 0x1023A7AC default
    {
        g_xpCoordinator.dynamicFrame.back()->active = 0;
        g_xpCoordinator.dynamicFrame.pop_back();
    }
    for (std::vector<XpLight*>::iterator it = g_xpCoordinator.dynamicFrame.begin();
         it != g_xpCoordinator.dynamicFrame.end(); ++it)
        prepareXpCandidates(*it, time, 1);
}

void* candidateGeometry(world::IObject* candidate)
{
#if BF2_PLUGIN_ABI_NATIVE
    return world::object_detail::LayoutAccess::geometry(candidate);
#else
    return *reinterpret_cast<void**>(
        reinterpret_cast<unsigned char*>(candidate) + 0x38);
#endif
}

static const unsigned int kBundledMeshReflectionType = 0x870CA651u;
static const unsigned int kSkinnedMeshReflectionType = 0x870CA652u;
static const unsigned int kStaticMeshReflectionType  = 0x870CA653u;


// Geometry instance dispatch uses fixed primary-table offsets until a shared typed view exists.
// All supported mesh classes must keep identical arity at these callee-cleanup slots.

inline void* geometrySlot(void* geometry, unsigned int offset)
{
    return (*reinterpret_cast<void***>(geometry))[offset / sizeof(void*)];
}

inline IGeometryTemplate* geometryTemplate(void* geometry)
{
    typedef void* (__fastcall* GetTemplateFn)(void*, void*);
    return static_cast<IGeometryTemplate*>(
        reinterpret_cast<GetTemplateFn>(geometrySlot(geometry, 0x38))(geometry, 0));
}

// The PE never caches this pair: every caller asks +0x38 and then the template's +0x10 again.
inline unsigned int geometryReflectionType(void* geometry)
{
    return geometryTemplate(geometry)->getClassId();                     // template +0x10
}

inline const LocalBoundingBox& geometryLocalBounds(void* geometry)
{
    typedef const void* (__fastcall* GetLocalBoundsFn)(void*, void*);
    return *static_cast<const LocalBoundingBox*>(
        reinterpret_cast<GetLocalBoundsFn>(geometrySlot(geometry, 0x40))(geometry, 0));
}

inline bool geometryAcceptsXpShadow(void* geometry)
{
    typedef bool (__fastcall* AcceptsFn)(void*, void*);
    return reinterpret_cast<AcceptsFn>(geometrySlot(geometry, 0xA8))(geometry, 0);
}

inline const Mat4& candidateAbsoluteTransformation(world::IObject* candidate)
{
    return candidate->getAbsoluteTransformation();
}

inline unsigned int candidateFlags(world::IObject* candidate)
{
#if BF2_PLUGIN_ABI_NATIVE
    return candidate->getFlags();
#else
    return *reinterpret_cast<const unsigned int*>(
        reinterpret_cast<const unsigned char*>(candidate) + 0x04);
#endif
}

ShadowTreeNode* shadowTreeFind(ShadowTree& tree, unsigned int key)
{
    ShadowTreeNode* const head = tree.head;
    ShadowTreeNode* candidate = head;
    ShadowTreeNode* node = head->parent;
    while (!node->isNil)
    {
        if (node->key < key)
            node = node->right;
        else
        {
            candidate = node;
            node = node->left;
        }
    }
    if (candidate == head || key < candidate->key)
        return head;
    return candidate;
}

bool supportedShadowGeometry(unsigned int type)
{
    return type == kStaticMeshReflectionType
        || type == kSkinnedMeshReflectionType
        || type == kBundledMeshReflectionType;
}

bool xpShadowPredicate(void* manager, world::LightSource* source,
                       world::IObject* candidate, bool point)
{
    const unsigned int flags = candidateFlags(candidate);
    if ((flags & 2u) == 0 || (flags & 0x0A000000u) != 0)
        return false;
    void* const geometry = candidateGeometry(candidate);
    if (!geometry)
        return false;
    const unsigned int type = geometryReflectionType(geometry);
    if (!supportedShadowGeometry(type))
        return false;
    ShadowTree& effects = lmField<ShadowTree>(manager, 0x964);
    if (shadowTreeFind(effects, type) == effects.head)
        return false;

    // PE globals 0x1023A09C/0x1023A0A0 both ship as mode 2. Point adds the
    // manager +0xC8 feature test; Spot deliberately does not.
    if (point && !lmField<unsigned char>(manager, 0x268))
        return false;
    const bool dynamicSource = lightSourceTemplatePointer(source)->getIsDynamic();
    if (type == kStaticMeshReflectionType)
        return dynamicSource;
    return geometryAcceptsXpShadow(geometry);
}

Vec3 transformPoint(const Mat4& matrix, const Vec3& point)
{
    Vec3 result;
    result.x = matrix.m[0] * point.x + matrix.m[4] * point.y
             + matrix.m[8] * point.z + matrix.m[12];
    result.y = matrix.m[1] * point.x + matrix.m[5] * point.y
             + matrix.m[9] * point.z + matrix.m[13];
    result.z = matrix.m[2] * point.x + matrix.m[6] * point.y
             + matrix.m[10] * point.z + matrix.m[14];
    return result;
}

bool fitProjectionToCandidate(ShadowProjectionLayout& projection,
                              world::IObject* candidate)
{
    void* const geometry = candidateGeometry(candidate);
    if (!geometry)
        return false;

    // PE 0x101613A0 extends only the lower world-Y side by one unit: move
    // the object transform down one and raise the copied local max-Y by one.
    Mat4 transform = candidateAbsoluteTransformation(candidate);
    transform.m[13] -= 1.0f;
    LocalBoundingBox box = geometryLocalBounds(geometry);
    box.m_max.y += 1.0f;
    box.m_derivedDataDirty = true;

    bool inside = true;
    for (unsigned int i = 0; i < 8; ++i)
    {
        const Vec3 local = {
            (i & 1) ? box.m_max.x : box.m_min.x,
            (i & 2) ? box.m_max.y : box.m_min.y,
            (i & 4) ? box.m_max.z : box.m_min.z
        };
        const Vec3 world = transformPoint(transform, local);
        // Retail uses bitwise-and assignment and therefore visits all corners.
        inside = projection.includePoint(world) & inside;
    }
    return inside;
}

void attachShadowCandidate(XpTemporaryRecord& record, world::IObject* candidate)
{
    (void)fitProjectionToCandidate(record.projection, candidate);
    record.record.embeddedTree.insert(reinterpret_cast<unsigned int>(candidate));
}

struct ShadowBuildContext
{
    world::LightSource* source;
    std::list<XpTemporaryRecord> records;
    float farDistance;

    ShadowBuildContext(world::LightSource* value, float distance)
        : source(value), records(), farDistance(distance)
    {
    }
};

void appendPointProjection(ShadowBuildContext& context, const Mat4& transform,
                           float left, float right, float bottom, float top,
                           bool restoreSides)
{
    context.records.emplace_back(transform, false);
    XpTemporaryRecord& record = context.records.back();
    ShadowProjectionLayout limits(transform, false);
    limits.setPerspectiveLimits(left, right, bottom, top, 0.1f, context.farDistance);
    limits.restoreAll();
    record.projection.copyCurrentToSaved(limits);
    record.projection.restoreSelected(false, false, false, false, true, false);
    if (restoreSides)
        record.projection.restoreSelected(true, true, true, false, true, false);
}

void buildPointFan(void* manager, ShadowBuildContext& context,
                   const vc71::vector_layout& candidates)
{
    static const float degreesToRadians = 0.017444445f;
    static const float azimuthDegrees = 90.0f;
    static const float verticalDegrees = 90.0f;
    const float azimuth = azimuthDegrees * degreesToRadians;
    const float vertical = verticalDegrees * degreesToRadians;

    Mat4 base = Mat4::getIdentity();
    const Vec3& sourcePosition = context.source->getAbsolutePosition();
    base.m[12] += sourcePosition.x;
    base.m[13] += sourcePosition.y;
    base.m[14] += sourcePosition.z;
    Mat4 yaw;
    setRotateYDeg(yaw, azimuthDegrees);
    const int sideCount = static_cast<int>(360.0f / azimuthDegrees);
    for (int i = 0; i < sideCount; ++i)
    {
        const Mat4 previous = base;
        base.mult(previous, yaw);
        appendPointProjection(context, base,
                              -azimuth * 0.5f, azimuth * 0.5f,
                              -vertical * 0.5f, vertical * 0.5f, true);
    }

    base = Mat4::getIdentity();
    base.m[12] += sourcePosition.x;
    base.m[13] += sourcePosition.y;
    base.m[14] += sourcePosition.z;
    Mat4 pitch;
    setRotateXDeg(pitch, 90.0f);
    const Mat4 untranslated = base;
    base.mult(untranslated, pitch);
    const float cap = 3.1400001f - vertical;
    appendPointProjection(context, base, -cap * 0.5f, cap * 0.5f,
                          -cap * 0.5f, cap * 0.5f, false);

    world::IObject* const* const begin = vectorBegin<world::IObject*>(candidates);
    const unsigned int count = vectorSize<world::IObject*>(candidates);
    for (unsigned int i = 0; i < count; ++i)
    {
        if (!xpShadowPredicate(manager, context.source, begin[i], true))
            continue;
        for (std::list<XpTemporaryRecord>::iterator record = context.records.begin();
             record != context.records.end(); ++record)
            attachShadowCandidate(*record, begin[i]);
    }
}

float scoreProjection(const XpTemporaryRecord* record, world::IObject* candidate,
                      float& growth)
{
    if (!record)
        return FLT_MAX;
    ShadowProjectionLayout projection(record->projection);
    float oldExtents[4];
    projection.getExtents(oldExtents);
    if (!fitProjectionToCandidate(projection, candidate))
        return FLT_MAX;
    float newExtents[4];
    projection.getExtents(newExtents);
    const float horizontalGrowth = (std::max)(newExtents[0] - oldExtents[0], 0.0f);
    growth = horizontalGrowth;
    // PE 0x1023A0A4 ships as selector 4: minimize final horizontal span.
    return newExtents[0];
}

XpTemporaryRecord* appendAdaptiveProjection(ShadowBuildContext& context,
                                            world::IObject* candidate)
{
    const Vec3 source = matrixTranslation(
        context.source->getAbsoluteTransformation());
    const Vec3 target = matrixTranslation(candidateAbsoluteTransformation(candidate));
    const Vec3 delta = { target.x - source.x, target.y - source.y, target.z - source.z };
    Vec3 up = { -delta.z, 0.0f, delta.x };
    if (dot3(delta, delta) <= FLT_EPSILON)
    {
        up.x = 0.0f;
        up.y = -delta.z;
        up.z = delta.y;
    }
    up = ShadowProjectionLayout::normalized(up);
    Mat4 look;
    calcLookAtMatrix(look, source, target, up);

    context.records.emplace_back(look, false);
    XpTemporaryRecord& record = context.records.back();
    ShadowProjectionLayout limits(look, false);
    limits.setPerspectiveLimits(-0.78500003f, 0.78500003f,
                                -0.78500003f, 0.78500003f,
                                0.1f, context.farDistance);
    limits.restoreAll();
    record.projection.copyCurrentToSaved(limits);
    return &record;
}

void buildAdaptivePoint(void* manager, ShadowBuildContext& context,
                        const vc71::vector_layout& candidates)
{
    world::IObject* const* const begin = vectorBegin<world::IObject*>(candidates);
    const unsigned int count = vectorSize<world::IObject*>(candidates);
    for (unsigned int i = 0; i < count; ++i)
    {
        world::IObject* const candidate = begin[i];
        if (!xpShadowPredicate(manager, context.source, candidate, true))
            continue;

        XpTemporaryRecord* best = 0;
        float bestScore = FLT_MAX;
        float bestGrowth = FLT_MAX;
        for (std::list<XpTemporaryRecord>::iterator it = context.records.begin();
             it != context.records.end(); ++it)
        {
            float growth = 0.0f;
            const float score = scoreProjection(&*it, candidate, growth);
            if (score < bestScore)
            {
                best = &*it;
                bestScore = score;
                bestGrowth = growth;
            }
        }

        // PE selector byte 0x10260A14 is loader-zero.  With the stock scoring
        // mode this adds a record when horizontal growth exceeds pi/4.
        const bool shouldAdd = context.records.size() < 5u && bestGrowth > 0.0f
            && (bestScore >= FLT_MAX || bestScore > 0.7853981633974483f);
        if (shouldAdd)
        {
            XpTemporaryRecord* const added = appendAdaptiveProjection(context, candidate);
            float ignoredGrowth = 0.0f;
            const float newScore = scoreProjection(added, candidate, ignoredGrowth);
            if (newScore < bestGrowth || !best)
                best = added;
            else
                context.records.pop_back();
        }

        if (!dx9rend::rendererPerformanceFlag(2) && best)
            attachShadowCandidate(*best, candidate);
    }
}

void buildSpotProjection(void* manager, ShadowBuildContext& context,
                         const vc71::vector_layout& candidates)
{
    world::IObject* const* const begin = vectorBegin<world::IObject*>(candidates);
    const unsigned int count = vectorSize<world::IObject*>(candidates);
    for (unsigned int i = 0; i < count; ++i)
    {
        world::IObject* const candidate = begin[i];
        if (!xpShadowPredicate(manager, context.source, candidate, false))
            continue;
        if (context.records.empty())
        {
            const Mat4& transform = context.source->getAbsoluteTransformation();
            context.records.emplace_back(transform, false);
            XpTemporaryRecord& record = context.records.back();
            ShadowProjectionLayout limits(transform, false);
            const float angle = context.source->getConeAngleRads2();
            limits.setPerspectiveLimits(-angle, angle, -angle, angle,
                                        0.1f, context.farDistance);
            limits.restoreAll();
            record.projection.copyCurrentToSaved(limits);
        }
        attachShadowCandidate(context.records.front(), candidate);
    }
}

void makeFrustumViewMatrix(const Frustum& frustum, Mat4& output)
{
    const Mat4& transform = frustum.m_transformation;
    const Vec3 eye = { transform.m[12], transform.m[13], transform.m[14] };
    Vec3 z = { transform.m[8], transform.m[9], transform.m[10] };
    Vec3 up = { transform.m[4], transform.m[5], transform.m[6] };
    z = ShadowProjectionLayout::normalized(z);
    Vec3 x = { up.y * z.z - up.z * z.y,
               up.z * z.x - up.x * z.z,
               up.x * z.y - up.y * z.x };
    x = ShadowProjectionLayout::normalized(x);
    Vec3 y = { z.y * x.z - z.z * x.y,
               z.z * x.x - z.x * x.z,
               z.x * x.y - z.y * x.x };
    output = Mat4::getIdentity();
    output.m[0] = x.x; output.m[1] = y.x; output.m[2] = z.x;
    output.m[4] = x.y; output.m[5] = y.y; output.m[6] = z.y;
    output.m[8] = x.z; output.m[9] = y.z; output.m[10] = z.z;
    output.m[12] = -dot3(x, eye);
    output.m[13] = -dot3(y, eye);
    output.m[14] = -dot3(z, eye);
}

void makeFrustumPerspectiveMatrix(const Frustum& frustum, Mat4& output)
{
    ::memset(&output, 0, sizeof(output));
    const float yScale = 1.0f / std::tan(frustum.m_fov * 0.5f);
    const float d3dAspect = 1.0f / frustum.m_aspect;
    output.m[0] = yScale / d3dAspect;
    output.m[5] = yScale;
    output.m[10] = frustum.m_farDist / (frustum.m_farDist - frustum.m_nearDist);
    output.m[11] = 1.0f;
    output.m[14] = -frustum.m_nearDist * frustum.m_farDist
                 / (frustum.m_farDist - frustum.m_nearDist);
}

void materializeShadowRecords(XpLight* light, ShadowBuildContext& context)
{
    for (std::list<XpTemporaryRecord>::iterator it = context.records.begin();
         it != context.records.end(); ++it)
    {
        if (it->record.embeddedTree.size == 0)
            continue;
        ShadowProjectionLayout projection(it->projection.transform, false);
        projection.copyCurrentToSaved(it->projection);
        // PE data dword 0x1023A0AC ships as policy 1: restore all fitted bounds.
        projection.restoreAll();
        float extents[4];
        projection.getExtents(extents);
        if (extents[0] <= FLT_EPSILON || extents[1] <= FLT_EPSILON
            || extents[3] - extents[2] <= FLT_EPSILON)
        {
            projection.restoreAll();
            projection.getExtents(extents);
        }
        if (extents[0] <= FLT_EPSILON || extents[1] <= FLT_EPSILON
            || extents[3] - extents[2] <= FLT_EPSILON)
            continue;

        XpRenderRecordLayout record;
        record.source = light->source;
        for (vc71::set_node* node = it->record.embeddedTree.begin();
             node != it->record.embeddedTree.end(); node = vc71::set_layout::next(node))
            record.embeddedTree.insert(node->key);
        record.frustum = projection.getFrustum();
        makeFrustumViewMatrix(record.frustum, record.matrices[1]);
        makeFrustumPerspectiveMatrix(record.frustum, record.matrices[2]);
        record.tail494[1] = 1;
        if (light->policy)
        {
            vc71::vectorRef<XpRenderRecordLayout> destination(&light->records);
            destination.push_back(record);
        }
    }
}

void buildPointLightRecords(void* manager, XpLight* light, bool adaptive)
{
    if (lightSourceTemplatePointer(light->source)->getLightTypeEnum() != 1)
        return;
    ShadowBuildContext context(light->source, xpEffectiveRadius(light) + 3.0f);
    if (adaptive)
        buildAdaptivePoint(manager, context, light->candidates);
    else
        buildPointFan(manager, context, light->candidates);
    materializeShadowRecords(light, context);
}

void buildSpotLightRecords(void* manager, XpLight* light)
{
    if (lightSourceTemplatePointer(light->source)->getLightTypeEnum() != 2)
        return;
    // PE 0x10167580 calls the source +0x90 radius slot directly here.  Unlike
    // the point builder it does not apply dynamicLightRangeScale.
    ShadowBuildContext context(light->source,
                               light->source->getVisualBoundingRadius() + 3.0f);
    buildSpotProjection(manager, context, light->candidates);
    materializeShadowRecords(light, context);
}

void updateXpCoordinator(double time)
{
    for (std::vector<XpLight*>::iterator it = g_xpCoordinator.iterationOrder.begin();
         it != g_xpCoordinator.iterationOrder.end(); ++it)
        updateXpLight(*it);
    updateStaticCollector(time);
    updateDynamicCollector(time);
}

void buildXpGeometryPairs()
{
    for (std::vector<XpLight*>::const_iterator it = g_xpCoordinator.iterationOrder.begin();
         it != g_xpCoordinator.iterationOrder.end(); ++it)
    {
        XpLight* const light = *it;
        if (!light->active)
            continue;
        world::IObject* const* const begin =
            vectorBegin<world::IObject*>(light->candidates);
        const unsigned int count = vectorSize<world::IObject*>(light->candidates);
        for (unsigned int i = 0; i < count; ++i)
            g_xpCoordinator.geometryPairs.insert(
                std::make_pair(candidateGeometry(begin[i]), light->source));
    }
}

void clearShadowRecordVector(vc71::vector_layout& vector)
{
    XpRenderRecordLayout* const records = vectorBegin<XpRenderRecordLayout>(vector);
    const unsigned int count = vectorSize<XpRenderRecordLayout>(vector);
    for (unsigned int i = 0; i < count; ++i)
        records[i].~XpRenderRecordLayout();
    vector.last = vector.first;
}

void destroyShadowRecordVector(vc71::vector_layout& vector);

inline world::IObject* objectParent(world::IObject* object)
{
    // IObject's parent is an inline field in both shipped images.  The client PE
    // places it at +0x28 and the shadow pass reads it directly.
    // (RW-266): ObjectLayoutAccess names it, at the same offset in both flavours.
    return object ? world::object_detail::LayoutAccess::parent(object) : 0;
}

inline void* localHumanPlayerRaw()
{
    return reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer();
}

inline world::IObject* playerVehicleRaw(void* player)
{
    return static_cast<world::IPlayer*>(player)->getVehicle();
}

inline bool playerHasFirstPersonViewRaw(void* player)
{
    return static_cast<world::IPlayer*>(player)->hasFirstPersonView();
}

inline void* queryPlayerControlObject(world::IObject* object)
{
    return object->queryInterface(50373u);
}

void* findPlayerControlObject(world::IObject* object)
{
    while (object)
    {
        void* const control = queryPlayerControlObject(object);
        if (control)
            return control;
        object = objectParent(object);
    }
    return 0;
}

inline world::IObject* controlOccupyingObject(void* control)
{
    return static_cast<world::IPlayerControlObject*>(control)->getOccupingObject();
}

inline bool controlChildSoldierActive(void* control)
{
    return static_cast<world::IPlayerControlObject*>(control)->isChildSoldierActive();
}

// Reads the host's weapon vector for first-person shadow exclusions.
// Requires a valid control object; native and VC7.1 hosts have different vector layouts.
inline world::IObject* const* controlFirstPersonObjectsEnd(void* control)
{
    void* const weapons = static_cast<world::IPlayerControlObject*>(control)->getWeapons();
#if BF2_PLUGIN_ABI_NATIVE
    std::vector<world::IObject*>& host = *static_cast<std::vector<world::IObject*>*>(weapons);
    return host.data() + host.size();
#else
    return reinterpret_cast<world::IObject* const*>(
        static_cast<const vc71::vector_layout*>(weapons)->last);
#endif
}

inline world::IObject* const* controlFirstPersonObjectsBegin(void* control)
{
    void* const weapons = static_cast<world::IPlayerControlObject*>(control)->getWeapons();
#if BF2_PLUGIN_ABI_NATIVE
    return static_cast<std::vector<world::IObject*>*>(weapons)->data();
#else
    return reinterpret_cast<world::IObject* const*>(
        static_cast<const vc71::vector_layout*>(weapons)->first);
#endif
}

// +0xA0, no arguments: the geometry's owner object (StaticMesh answers the shared const-0 body).
inline world::IObject* geometryObject(void* geometry)
{
    typedef void* (__fastcall* OwnerFn)(void*, void*);
    return static_cast<world::IObject*>(
        reinterpret_cast<OwnerFn>(geometrySlot(geometry, 0xA0))(geometry, 0));
}

bool pointCasterPassesLocalView(void* localPlayer, world::IObject* candidate,
                                void* geometry, unsigned int reflectionType)
{
    // PE data dword 0x1023A0C4 ships as mode 1.  A missing local player or a
    // value below one rejects the caster rather than disabling only the 1P
    // exclusions.
    static const int localFilterMode = 1;
    if (!localPlayer || localFilterMode < 1)
        return false;

    world::IObject* const vehicle = playerVehicleRaw(localPlayer);
    void* const control = findPlayerControlObject(vehicle);
    world::IObject* const geometryOwner = geometryObject(geometry);

    if (localFilterMode == 1)
    {
        if (playerHasFirstPersonViewRaw(localPlayer) && control
            && controlChildSoldierActive(control)
            && objectParent(candidate) == controlOccupyingObject(control))
            return false;
        if (playerHasFirstPersonViewRaw(localPlayer) && geometryOwner
            && playerVehicleRaw(localPlayer) == objectParent(geometryOwner))
            return false;
        if (playerHasFirstPersonViewRaw(localPlayer)
            && playerVehicleRaw(localPlayer) == candidate)
            return false;
    }
    else if (localFilterMode == 2
             && playerHasFirstPersonViewRaw(localPlayer) && control
             && controlChildSoldierActive(control)
             && controlOccupyingObject(control) == geometryOwner)
    {
        return false;
    }

    if (reflectionType != kStaticMeshReflectionType)
    {
        // Preserve the three shipped virtual calls and its missing null guard.
        world::IObject* const* const originalEnd = controlFirstPersonObjectsEnd(control);
        world::IObject* const* const end = controlFirstPersonObjectsEnd(control);
        world::IObject* const* cursor = controlFirstPersonObjectsBegin(control);
        while (cursor != end && *cursor != candidate)
            ++cursor;
        if (playerHasFirstPersonViewRaw(localPlayer) && cursor != originalEnd)
            return false;
    }

    if (playerHasFirstPersonViewRaw(localPlayer) && (candidateFlags(candidate) & 2u) == 0)
        return false;
    return true;
}

void assignPointShadowAtlas(void* manager, XpRenderRecordLayout& record)
{
    // Point/spot records use an equal-tile square atlas rather
    // than the directional 28-entry placement table.  There is intentionally
    // no zero/divisibility guard around the retail integer division.
    const int atlasSize = lmField<int>(manager, 0x204);
    const int tileSize = lmField<int>(manager, 0x208);
    const int columns = atlasSize / tileSize;
    const unsigned int ordinal = g_shadowRecordOrdinal++;
    const int x = tileSize * static_cast<int>(ordinal % columns);
    const int y = tileSize * static_cast<int>(ordinal / columns);

    D3DVIEWPORT9 viewport;
    viewport.X = static_cast<DWORD>(x + 3);
    viewport.Y = static_cast<DWORD>(y + 3);
    viewport.Width = static_cast<DWORD>(tileSize - 6);
    viewport.Height = static_cast<DWORD>(tileSize - 6);
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    IDirect3DDevice9* const device =
        lmField<IDirect3DDevice9*>(manager, 0x908);
    device->SetViewport(&viewport);
    RECT scissor = { x + 3, y + 3, x + tileSize - 3, y + tileSize - 3 };
    device->SetScissorRect(&scissor);

    const float inverseAtlas = 1.0f / static_cast<float>(atlasSize);
    const float scale = static_cast<float>(tileSize) * inverseAtlas;
    const float u = static_cast<float>(x) * inverseAtlas;
    const float v = static_cast<float>(y) * inverseAtlas;
    record.atlasScaleOffset[0] = u;
    record.atlasScaleOffset[1] = v;
    record.atlasScaleOffset[2] = scale;
    record.atlasScaleOffset[3] = scale;
    record.matrices[8] = Mat4::getIdentity();
    record.matrices[8].m[0] = scale;
    record.matrices[8].m[5] = scale;
    record.matrices[8].m[12] = u;
    record.matrices[8].m[13] = v;
    record.atlasBounds[0] = u;
    record.atlasBounds[1] = v;
    record.atlasBounds[2] = u + scale;
    record.atlasBounds[3] = v + scale;
}

// +0x20, the shadow-map draw, as the point/spot path calls it (0x10167462..0x10167479):
// `push 0; push 0; push &rec+10h; push &rec+278h; push &rec+238h; push rec`. So the six stack
// arguments are the record, its matrices[1] (+0x238) and matrices[2] (+0x278), its frustum (+0x10),
// and the two zeros the directional arm fills with (secondPass, reflectionChanged) instead.
inline void geometryDrawShadow(void* geometry, XpRenderRecordLayout* record,
                               int secondPass, int effectChanged)
{
    typedef void (__fastcall* DrawShadowMapFn)(void*, void*, void*, const void*, const void*,
                                               const void*, unsigned int, unsigned int);
    reinterpret_cast<DrawShadowMapFn>(geometrySlot(geometry, 0x20))(
        geometry, 0, record, &record->matrices[1], &record->matrices[2], &record->frustum,
        static_cast<unsigned int>(secondPass), static_cast<unsigned int>(effectChanged));
}

void submitShadowCaster(void* manager, XpRenderRecordLayout& record,
                        world::IObject* candidate, void* localPlayer,
                        bool applyPointFilter)
{
    void* const geometry = candidateGeometry(candidate);
    if (!geometry)
        return;

    // The PE asks the geometry for its template and reflection id twice.
    const unsigned int filterType = geometryReflectionType(geometry);
    const unsigned int effectType = geometryReflectionType(geometry);
    if (applyPointFilter
        && !pointCasterPassesLocalView(localPlayer, candidate, geometry, filterType))
        return;

    ShadowTree& effects = lmField<ShadowTree>(manager, 0x964);
    ShadowTreeNode* const effectNode = shadowTreeFind(effects, effectType);
    if (effectNode == effects.head)
        return;

    if (applyPointFilter)
    {
        // The loader-zero debug bytes bypassing this cull and filtering dynamic
        // sources are deliberately absent here; the stock arm performs the
        // Frustum test and only performance flag 4 suppresses the draw.
        if (!record.frustum.inside(objectAbsolutePosition(candidate),
                                   objectVisualRadius(candidate))
            || dx9rend::rendererPerformanceFlag(4))
            return;
    }

    dx9rend::stateManagerPush();
    dx9rend::effectBegin(effectNode->value);
    dx9rend::effectBeginPass(effectNode->value, 1);
    geometryDrawShadow(geometry, &record, 0, 0);
    dx9rend::effectEnd(effectNode->value);
    dx9rend::stateManagerPop();
}

void renderXpLightRecords(void* manager, XpLight* light, void* localPlayer,
                          bool applyPointFilter)
{
    XpRenderRecordLayout* const records =
        vectorBegin<XpRenderRecordLayout>(light->records);
    const unsigned int count = vectorSize<XpRenderRecordLayout>(light->records);
    for (unsigned int i = 0; i < count; ++i)
    {
        XpRenderRecordLayout& record = records[i];
        record.flags448[0] = 0;
        assignPointShadowAtlas(manager, record);
        for (vc71::set_node* node = record.embeddedTree.begin();
             node != record.embeddedTree.end(); node = vc71::set_layout::next(node))
        {
            submitShadowCaster(manager, record,
                reinterpret_cast<world::IObject*>(node->key), localPlayer,
                applyPointFilter);
        }
    }
}

void renderPointShadowMaps(void* manager)
{
    // PE 0x10166DB0 stock selector 2: dynamic lights use the five-face fan,
    // static lights use the adaptive builder, and submission orders static
    // records before dynamic records.  +0x7DC remains empty in this mode.
    clearShadowRecordVector(lmField<vc71::vector_layout>(manager, 0x7DC));
    if (!dx9rend::rendererPerformanceFlag(0))
    {
        for (std::vector<XpLight*>::iterator it = g_xpCoordinator.dynamicFrame.begin();
             it != g_xpCoordinator.dynamicFrame.end(); ++it)
            buildPointLightRecords(manager, *it, false);
    }
    for (std::vector<XpLight*>::iterator it = g_xpCoordinator.staticFrame.begin();
         it != g_xpCoordinator.staticFrame.end(); ++it)
        buildPointLightRecords(manager, *it, true);

    if (dx9rend::rendererPerformanceFlag(5))
        destroyShadowRecordVector(lmField<vc71::vector_layout>(manager, 0x7DC));

    void* const localPlayer = localHumanPlayerRaw();
    for (std::vector<XpLight*>::iterator it = g_xpCoordinator.staticFrame.begin();
         it != g_xpCoordinator.staticFrame.end(); ++it)
        renderXpLightRecords(manager, *it, localPlayer, true);
    for (std::vector<XpLight*>::iterator it = g_xpCoordinator.dynamicFrame.begin();
         it != g_xpCoordinator.dynamicFrame.end(); ++it)
        renderXpLightRecords(manager, *it, localPlayer, true);

    if (dx9rend::rendererPerformanceFlag(6))
        destroyShadowRecordVector(lmField<vc71::vector_layout>(manager, 0x7DC));
    clearShadowTree(lmField<ShadowTree>(manager, 0x8CC));
}

void appendShadowRecords(vc71::vector_layout& destination,
                         const vc71::vector_layout& source)
{
    vc71::vectorRef<XpRenderRecordLayout> output(&destination);
    const XpRenderRecordLayout* const records =
        vectorBegin<XpRenderRecordLayout>(source);
    const unsigned int count = vectorSize<XpRenderRecordLayout>(source);
    for (unsigned int i = 0; i < count; ++i)
        output.push_back(records[i]);
}

void renderSpotShadowMaps(void* manager)
{
    // PE 0x10167580 is gated by signed BSS dword 0x10260A74 <= 0; its
    // loader-zero stock value enters this arm.
    static const int skipSpotPass = 0;
    if (skipSpotPass > 0)
        return;

    vc71::vector_layout& spotRecords =
        lmField<vc71::vector_layout>(manager, 0x7CC);
    clearShadowRecordVector(spotRecords);
    if (!g_xpCoordinator.dynamicFrame.empty())
    {
        // Retail never advances the selected iterator: it builds the first
        // dynamic XpLight three times, regardless of the vector's length.
        XpLight* const first = g_xpCoordinator.dynamicFrame.front();
        for (int remaining = 3; remaining > 0; --remaining)
            buildSpotLightRecords(manager, first);
    }

    (void)localHumanPlayerRaw(); // shipped side-effect call; result is discarded
    XpRenderRecordLayout* const records =
        vectorBegin<XpRenderRecordLayout>(spotRecords);
    const unsigned int count = vectorSize<XpRenderRecordLayout>(spotRecords);
    for (unsigned int i = 0; i < count; ++i)
    {
        records[i].flags448[0] = 0;
        assignPointShadowAtlas(manager, records[i]);
        for (vc71::set_node* node = records[i].embeddedTree.begin();
             node != records[i].embeddedTree.end(); node = vc71::set_layout::next(node))
        {
            submitShadowCaster(manager, records[i],
                reinterpret_cast<world::IObject*>(node->key), 0, false);
        }
    }

    // In the observed stock path +0x7CC has no producer, so this appends an
    // empty range.  Keep the range operation rather than synthesising spot
    // records into the manager vector.
    appendShadowRecords(lmField<vc71::vector_layout>(manager, 0x7DC), spotRecords);
}

// The directional path below is PE 0x10164DC0 and its TSM fitter at 0x101626B0.  Keep the
// projective multiply local: Mat4::mult is deliberately the affine fast path and therefore cannot
// represent the trapezoid projection's m[3] term.
Mat4 multiplyShadowMatrices(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 result;
    for (unsigned int row = 0; row < 4; ++row)
    {
        for (unsigned int column = 0; column < 4; ++column)
        {
            result.m[row * 4 + column] =
                lhs.m[row * 4 + 0] * rhs.m[0 * 4 + column]
              + lhs.m[row * 4 + 1] * rhs.m[1 * 4 + column]
              + lhs.m[row * 4 + 2] * rhs.m[2 * 4 + column]
              + lhs.m[row * 4 + 3] * rhs.m[3 * 4 + column];
        }
    }
    return result;
}

Vec3 transformShadowCoord(const Vec3& point, const Mat4& matrix)
{
    const float w = point.x * matrix.m[3] + point.y * matrix.m[7]
                  + point.z * matrix.m[11] + matrix.m[15];
    const float inverseW = 1.0f / w;
    const Vec3 result = {
        (point.x * matrix.m[0] + point.y * matrix.m[4]
          + point.z * matrix.m[8] + matrix.m[12]) * inverseW,
        (point.x * matrix.m[1] + point.y * matrix.m[5]
          + point.z * matrix.m[9] + matrix.m[13]) * inverseW,
        (point.x * matrix.m[2] + point.y * matrix.m[6]
          + point.z * matrix.m[10] + matrix.m[14]) * inverseW
    };
    return result;
}

Vec3 transformShadowNormal(const Vec3& value, const Mat4& matrix)
{
    const Vec3 result = {
        value.x * matrix.m[0] + value.y * matrix.m[4] + value.z * matrix.m[8],
        value.x * matrix.m[1] + value.y * matrix.m[5] + value.z * matrix.m[9],
        value.x * matrix.m[2] + value.y * matrix.m[6] + value.z * matrix.m[10]
    };
    return result;
}

struct ShadowAabb
{
    Vec3 minimum;
    Vec3 maximum;
};

ShadowAabb emptyShadowAabb(float extent)
{
    ShadowAabb result;
    result.minimum.x = result.minimum.y = result.minimum.z = extent;
    result.maximum.x = result.maximum.y = result.maximum.z = -extent;
    return result;
}

void expandShadowAabb(ShadowAabb& bounds, const Vec3& point)
{
    bounds.minimum.x = (std::min)(bounds.minimum.x, point.x);
    bounds.minimum.y = (std::min)(bounds.minimum.y, point.y);
    bounds.minimum.z = (std::min)(bounds.minimum.z, point.z);
    bounds.maximum.x = (std::max)(bounds.maximum.x, point.x);
    bounds.maximum.y = (std::max)(bounds.maximum.y, point.y);
    bounds.maximum.z = (std::max)(bounds.maximum.z, point.z);
}

Vec3 shadowAabbCorner(const ShadowAabb& bounds, unsigned int index)
{
    const Vec3 result = {
        (index & 1u) ? bounds.maximum.x : bounds.minimum.x,
        (index & 2u) ? bounds.maximum.y : bounds.minimum.y,
        (index & 4u) ? bounds.maximum.z : bounds.minimum.z
    };
    return result;
}

Vec3 localBoxCorner(const LocalBoundingBox& bounds, unsigned int index)
{
    const Vec3 result = {
        (index & 1u) ? bounds.m_max.x : bounds.m_min.x,
        (index & 2u) ? bounds.m_max.y : bounds.m_min.y,
        (index & 4u) ? bounds.m_max.z : bounds.m_min.z
    };
    return result;
}

inline const Mat4& objectAbsoluteTransformation(world::IObject* object)
{
    return object->getAbsoluteTransformation();
}

inline bool objectHasCollisionLod(world::IObject* object, int lod)
{
    return object->hasCollisionLod(lod);
}

inline void* objectCollisionMesh(world::IObject* object)
{
#if BF2_PLUGIN_ABI_NATIVE
    return world::object_detail::LayoutAccess::collisionMesh(object);
#else
    return *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(object) + 0x40);
#endif
}

inline const LocalBoundingBox* collisionBoundingBox(void* collision, int lod)
{
    return static_cast<world::ICollisionMesh*>(collision)->getBoundingBox(lod);
}

// a null geometry is false; otherwise the template's +0x9C (no args,
// AL). The base body is `xor al,al`; BundledMeshTemplate answers its +0x199 byte (0x100EEA00).
bool geometryUsesShadowSphere(void* geometry)
{
    if (!geometry)
        return false;
    return geometryTemplate(geometry)->slot_0x9C();                     // template +0x9C
}

// +0xAC, one argument: the renderer's frame ordinal (renderer +0x54) - the geometry's
// shadow-caster frame stamp (StaticMesh +0x350, SkinnedMesh +0x338, BundledMesh +0x2B0).
// The callees' own return values differ (a void setter, a returning setter); every PE call
// site discards them, so the dispatch is declared void.
inline void geometryPrepareShadow(void* geometry, unsigned int rendererToken)
{
    typedef void (__fastcall* StampFn)(void*, void*, unsigned int);
    reinterpret_cast<StampFn>(geometrySlot(geometry, 0xAC))(geometry, 0, rendererToken);
}

// +0x24, one argument: the shadow record this geometry draws into (StaticMesh +0x34C, SkinnedMesh
// +0x33C, BundledMesh +0x2B4 - the "caster" the shadow draws read when no light is passed).
inline void geometryAttachShadowRecord(void* geometry, XpRenderRecordLayout* record)
{
    typedef void (__fastcall* AttachFn)(void*, void*, void*);
    reinterpret_cast<AttachFn>(geometrySlot(geometry, 0x24))(geometry, 0, record);
}

// +0xA4, no arguments: the geometry's root object (memoised by +0xF8 / refreshed in the call).
inline world::IObject* geometryFinalizerObject(void* geometry)
{
    typedef void* (__fastcall* RootFn)(void*, void*);
    return static_cast<world::IObject*>(
        reinterpret_cast<RootFn>(geometrySlot(geometry, 0xA4))(geometry, 0));
}

inline unsigned int rendererShadowToken()
{
    return dx9rend::rendererFrameOrdinal();
}

// G3, confirmed against the PE: every quality-three test in 0x10164DC0
// (0x101665CE, 0x101666D2, 0x1016676C and 0x10166782) is `mov ecx, ds:1023D098; call [eax+0ACh]` -
// g_Renderer's +0xAC, Renderer::isExpansionPack (the byte at Renderer +0x764, written only
// by setExpansionPack 0x10028310 when the shader archive is selected). The seam below reaches the
// same object through g_rendererBase (0x1023B970), which the Renderer's base constructor points at
// the same instance (RendDX9Init.cpp) - the same slot on the same object.
inline bool rendererSuppressesHighQualityShadows()
{
    return dx9rend::rendererIsExpansionPack();
}

bool shadowFloatHasExponent(float value)
{
    unsigned int bits;
    ::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7F800000u) != 0;
}

Mat4 makeShadowOrthoOffCenter(float left, float right, float bottom, float top,
                              float nearDistance, float farDistance)
{
    Mat4 result = Mat4::getZero();
    result.m[0] = 2.0f / (right - left);
    result.m[5] = 2.0f / (top - bottom);
    result.m[10] = 1.0f / (farDistance - nearDistance);
    result.m[12] = (left + right) / (left - right);
    result.m[13] = (top + bottom) / (bottom - top);
    result.m[14] = nearDistance / (nearDistance - farDistance);
    result.m[15] = 1.0f;
    return result;
}

void transformShadowPoints(Vec3* points, unsigned int count, const Mat4& matrix)
{
    for (unsigned int i = 0; i < count; ++i)
        points[i] = transformShadowCoord(points[i], matrix);
}

ShadowAabb shadowBoundsForPoints(const Vec3* points, unsigned int count, float extent)
{
    ShadowAabb result = emptyShadowAabb(extent);
    for (unsigned int i = 0; i < count; ++i)
        expandShadowAabb(result, points[i]);
    return result;
}

void collectDirectionalCasterBounds(void* manager, const Mat4& inverseView,
                                    std::vector<ShadowAabb>& output)
{
    world::IObject* const* const first =
        vectorBegin<world::IObject*>(lmField<vc71::vector_layout>(manager, 0x914));
    const unsigned int count =
        vectorSize<world::IObject*>(lmField<vc71::vector_layout>(manager, 0x914));
    std::vector<world::IObject*> ordered(first, first + count);
    std::sort(ordered.begin(), ordered.end(),
        [](world::IObject* lhs, world::IObject* rhs) {
            return reinterpret_cast<unsigned int>(lhs)
                 < reinterpret_cast<unsigned int>(rhs);
        });
    ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());

    static float sphereScale = std::sqrt(2.0f);
    for (std::vector<world::IObject*>::const_iterator it = ordered.begin();
         it != ordered.end(); ++it)
    {
        world::IObject* const object = *it;
        void* const geometry = objectGeometry(object);
        // m_flags bit the shipped byte test `[obj+7] & 4`, typed through
        // ObjectLayoutAccess (IObject's retail-aligned prefix, both flavours; RW-266).
        const bool sphereArm = geometryUsesShadowSphere(geometry)
            || ((world::object_detail::LayoutAccess::flags(object) & 0x04000000u) != 0);
        if (sphereArm)
        {
            const Vec3& centre = objectAbsolutePosition(object);
            const float radius = objectVisualRadius(object) * sphereScale;
            Vec3 low = { centre.x - radius, centre.y - radius, centre.z - radius };
            Vec3 high = { centre.x + radius, centre.y + radius, centre.z + radius };
            ShadowAabb box = emptyShadowAabb(3.3e33f);
            expandShadowAabb(box, transformShadowCoord(low, inverseView));
            expandShadowAabb(box, transformShadowCoord(high, inverseView));
            output.push_back(box);
            continue;
        }

        if (!objectHasCollisionLod(object, 0))
            continue;
        void* const collision = objectCollisionMesh(object);
        const LocalBoundingBox* const local = collisionBoundingBox(collision, 0);
        const Mat4 objectToView = multiplyShadowMatrices(
            objectAbsoluteTransformation(object), inverseView);
        ShadowAabb box = emptyShadowAabb(3.3e33f);
        for (unsigned int corner = 0; corner < 8; ++corner)
            expandShadowAabb(box, transformShadowCoord(
                localBoxCorner(*local, corner), objectToView));
        output.push_back(box);
    }
}

// Fits the directional shadow matrix using the sun direction; requires a live sun.
// [UNVERIFIED] Receiver fitting currently transforms camera corners rather than deriving
// the receiver frustum from the camera projection; this difference needs validation.
Mat4 calculateDirectionalShadowMatrix(void* manager)
{
    const Vec3 sunWorld =
        lmField<world::LightSource*>(manager, 0x840)->getDirection();       // 0x101653C7, sun +0x17C
    dx9rend::IRenderView* const view = lmField<dx9rend::IRenderView*>(manager, 0x90C);
    const Mat4 inverseView = *view->getInverseViewMatrixPtr();

    std::vector<ShadowAabb> casterBoxes;
    collectDirectionalCasterBounds(manager, inverseView, casterBoxes);

    Vec3 receiver[8];
    const Frustum& camera = lmField<Frustum>(manager, 0x460);
    for (unsigned int i = 0; i < 8; ++i)
        receiver[i] = transformShadowCoord(camera.m_corners[i], inverseView);

    const Vec3 light = transformShadowNormal(sunWorld, inverseView);
    Vec3 axis = { -light.y, light.x, 0.0f };
    const float axisLength = std::sqrt(axis.x * axis.x + axis.y * axis.y);
    axis.x /= axisLength;
    axis.y /= axisLength;

    Mat4 lightBasis = Mat4::getZero();
    lightBasis.m[0] = axis.x;
    lightBasis.m[1] = axis.z * light.y - light.z * axis.y;
    lightBasis.m[2] = -light.x;
    lightBasis.m[4] = axis.y;
    lightBasis.m[5] = light.z * axis.x - axis.z * light.x;
    lightBasis.m[6] = -light.y;
    lightBasis.m[8] = axis.z;
    lightBasis.m[9] = axis.y * light.x - light.y * axis.x;
    lightBasis.m[10] = -light.z;
    lightBasis.m[15] = 1.0f;

    transformShadowPoints(receiver, 8, lightBasis);
    ShadowAabb receiverBounds = shadowBoundsForPoints(receiver, 8, 1.0e33f);
    ShadowAabb casterBounds = emptyShadowAabb(1.0e33f);
    for (std::vector<ShadowAabb>::const_iterator it = casterBoxes.begin();
         it != casterBoxes.end(); ++it)
    {
        for (unsigned int corner = 0; corner < 8; ++corner)
            expandShadowAabb(casterBounds, transformShadowCoord(
                shadowAabbCorner(*it, corner), lightBasis));
    }

    float nearDistance = (std::min)(casterBounds.minimum.z, receiverBounds.minimum.z);
    float farDistance = (std::max)(casterBounds.maximum.z, receiverBounds.maximum.z);
    if (nearDistance <= 0.0f)
    {
        Mat4 translate = Mat4::getIdentity();
        translate.m[14] = 1.0f - nearDistance;
        lightBasis = multiplyShadowMatrices(lightBasis, translate);
        farDistance = farDistance - nearDistance + 1.0f;
        nearDistance = 1.0f;
        transformShadowPoints(receiver, 8, translate);
        receiverBounds = shadowBoundsForPoints(receiver, 8, 1.0e33f);
    }

    const Mat4 ortho = makeShadowOrthoOffCenter(
        receiverBounds.minimum.x, receiverBounds.maximum.x,
        receiverBounds.minimum.y, receiverBounds.maximum.y,
        nearDistance, farDistance);
    transformShadowPoints(receiver, 8, ortho);

    Vec3 nearCentre = { 0.0f, 0.0f, 0.0f };
    Vec3 farCentre = { 0.0f, 0.0f, 0.0f };
    for (unsigned int i = 0; i < 4; ++i)
    {
        nearCentre.x += receiver[i].x * 0.25f;
        nearCentre.y += receiver[i].y * 0.25f;
        nearCentre.z += receiver[i].z * 0.25f;
        farCentre.x += receiver[i + 4].x * 0.25f;
        farCentre.y += receiver[i + 4].y * 0.25f;
        farCentre.z += receiver[i + 4].z * 0.25f;
    }
    const Vec3 middle = {
        (nearCentre.x + farCentre.x) * 0.5f,
        (nearCentre.y + farCentre.y) * 0.5f,
        (nearCentre.z + farCentre.z) * 0.5f
    };
    const Vec3 delta = {
        nearCentre.x - middle.x, nearCentre.y - middle.y, nearCentre.z - middle.z
    };
    const float inverseLength = 1.0f / std::sqrt(delta.x * delta.x + delta.y * delta.y);
    Mat4 centre = Mat4::getIdentity();
    centre.m[12] = -middle.x;
    centre.m[13] = -middle.y;
    Mat4 rotate = Mat4::getIdentity();
    rotate.m[0] = delta.x * inverseLength;
    rotate.m[1] = -(delta.y * inverseLength);
    rotate.m[4] = delta.y * inverseLength;
    rotate.m[5] = rotate.m[0];
    Mat4 fitted = multiplyShadowMatrices(centre, rotate);
    transformShadowPoints(receiver, 8, fitted);
    ShadowAabb fittedBounds = shadowBoundsForPoints(receiver, 8, 1.0e33f);

    const float scaleX = 1.0f / (std::max)(
        std::fabs(fittedBounds.minimum.x), std::fabs(fittedBounds.maximum.x));
    const float scaleY = 1.0f / (std::max)(
        std::fabs(fittedBounds.minimum.y), std::fabs(fittedBounds.maximum.y));
    Mat4 scale = Mat4::getIdentity();
    scale.m[0] = scaleX;
    scale.m[5] = scaleY;
    fitted = multiplyShadowMatrices(fitted, scale);
    fittedBounds.minimum.x *= scaleX;
    fittedBounds.maximum.x *= scaleX;
    fittedBounds.minimum.y *= scaleY;
    fittedBounds.maximum.y *= scaleY;

    const float width = fittedBounds.maximum.x - fittedBounds.minimum.x;
    const float focusWidth = lmField<float>(manager, 0x814) * width;
    const float q = width * focusWidth * 0.39999998f
                  / (width * 1.6f - (focusWidth + focusWidth));
    const float projectionX = fittedBounds.maximum.x + q;
    float maximumSlope = -1.0e32f;
    float minimumSlope = 1.0e32f;
    for (unsigned int i = 0; i < 8; ++i)
    {
        const float numerator = scaleY * receiver[i].y;
        const float denominator = scaleX * receiver[i].x - projectionX;
        if (!shadowFloatHasExponent(numerator) || !shadowFloatHasExponent(denominator))
            continue;
        const float slope = numerator / denominator;
        maximumSlope = (std::max)(maximumSlope, slope);
        if (slope <= minimumSlope)
            minimumSlope = slope;
    }

    Mat4 flip = Mat4::getIdentity();
    flip.m[0] = -1.0f;
    flip.m[12] = projectionX;
    fitted = multiplyShadowMatrices(fitted, flip);

    const float shearValue = (std::fabs(minimumSlope) + maximumSlope) * 0.5f
                           - maximumSlope;
    maximumSlope += shearValue;
    Mat4 shear = Mat4::getIdentity();
    shear.m[1] = shearValue;
    fitted = multiplyShadowMatrices(fitted, shear);

    const float p = q + width;
    const float denominator = p - q;
    Mat4 projective = Mat4::getZero();
    projective.m[0] = p / denominator;
    projective.m[3] = 1.0f;
    projective.m[5] = 1.0f / maximumSlope;
    projective.m[10] = 1.0f
        / (((receiverBounds.maximum.z - receiverBounds.minimum.z)
           / (fittedBounds.maximum.y - fittedBounds.minimum.y)) * maximumSlope);
    projective.m[14] = -(p * q / denominator);
    fitted = multiplyShadowMatrices(fitted, projective);
    Mat4 remap = Mat4::getIdentity();
    remap.m[0] = 2.0f;
    remap.m[12] = -1.0f;
    fitted = multiplyShadowMatrices(fitted, remap);

    Mat4 shadow = multiplyShadowMatrices(ortho, fitted);
    shadow = multiplyShadowMatrices(lightBasis, shadow);

    // Stock byte 0x1023A0B0 crops the fitted projection to the transformed caster bounds.
    ShadowAabb crop = emptyShadowAabb(1.0e33f);
    for (std::vector<ShadowAabb>::const_iterator it = casterBoxes.begin();
         it != casterBoxes.end(); ++it)
    {
        for (unsigned int corner = 0; corner < 8; ++corner)
            expandShadowAabb(crop, transformShadowCoord(
                shadowAabbCorner(*it, corner), shadow));
    }
    if (crop.maximum.x > 1.0f) crop.maximum.x = 1.0f;
    if (crop.minimum.x <= -1.0f) crop.minimum.x = -1.0f;
    if (crop.maximum.y > 1.0f) crop.maximum.y = 1.0f;
    if (crop.minimum.y <= -1.0f) crop.minimum.y = -1.0f;
    const float cropWidth = crop.maximum.x - crop.minimum.x;
    const float cropHeight = crop.maximum.y - crop.minimum.y;
    if (shadowFloatHasExponent(cropWidth) && shadowFloatHasExponent(cropHeight))
    {
        Mat4 cropMatrix = Mat4::getIdentity();
        cropMatrix.m[0] = 2.0f / cropWidth;
        cropMatrix.m[5] = 2.0f / cropHeight;
        cropMatrix.m[12] = -(crop.minimum.x + crop.maximum.x) / cropWidth;
        cropMatrix.m[13] = -(crop.minimum.y + crop.maximum.y) / cropHeight;
        shadow = multiplyShadowMatrices(shadow, cropMatrix);
    }
    return multiplyShadowMatrices(inverseView, shadow);
}

// 0x10165F3C..0x10165F67: +0x954 cleared (0x10032330(0, 0)), then ObjectManager::addChildrenRend
// (+0x88) collects the record root's children into it and grows the record's bounds over them - the
// bounds the near-plane fit then reads (0x10165F75).
void collectDirectionalChildren(void* manager, world::IObject* root, LocalBoundingBox& bounds)
{
    vc71::vector_layout& destination = lmField<vc71::vector_layout>(manager, 0x954);
    clearPointerVector(destination);
#if BF2_PLUGIN_ABI_NATIVE
    std::vector<world::IObject*> hostObjects;
    reinterpret_cast<world::ObjectManager*>(objectManager)->addChildrenRend(
        root, hostObjects, &bounds, 0);
    vc71::vectorRef<world::IObject*> output(&destination);
    for (std::vector<world::IObject*>::const_iterator it = hostObjects.begin();
         it != hostObjects.end(); ++it)
        output.push_back(*it);
#else
    // MSVC prepends the later object overload inside the addChildrenRend overload group.
    reinterpret_cast<world::ObjectManager*>(objectManager)->addChildrenRend(
        root, hostObjectVector(destination), &bounds, 0);
#endif
}

// IGeometry +0x20 - StaticMesh::drawShadowMap (PE 0x100FD580, ).  The shipped
// two-pass loop (PE 0x10164DC0) calls it with four zero words - the first of them the caster, so
// there is none and the instance uses its own +0x34C record when that registration is this frame's
// - and the two pass booleans
// `pass != 0` and `reflection type changed`, none of which the StaticMesh body reads.  Reached raw
// exactly as the gather reaches +0x44 / +0x48; this tree's StaticMesh publishes a raw table.
inline void geometryDrawDirectionalBatch(void* geometry, int secondPass,
                                         int reflectionChanged)
{
    typedef void (__fastcall* DrawShadowMapFn)(void*, void*, void*, unsigned int, unsigned int,
                                               unsigned int, unsigned int, unsigned int);
    void** const vtbl = *reinterpret_cast<void***>(geometry);
    reinterpret_cast<DrawShadowMapFn>(vtbl[0x20 / sizeof(void*)])(
        geometry, 0, 0, 0u, 0u, 0u,
        static_cast<unsigned int>(secondPass != 0),
        static_cast<unsigned int>(reflectionChanged != 0));
}

// Submits directional geometry, switching effects on class changes.
// [DEVIATION] Class-ID sorting replaces the original non-ordering inequality predicate;
// draw order and effect transition counts can differ, although depth output agrees.
void submitDirectionalGeometry(void* manager)
{
    vc71::vector_layout& geometryVector = lmField<vc71::vector_layout>(manager, 0x924);
    void** const first = vectorBegin<void*>(geometryVector);
    void** const last = reinterpret_cast<void**>(geometryVector.last);
    if (first != last)
    {
        std::sort(first, last, [](void* lhs, void* rhs) {
            return geometryReflectionType(lhs) < geometryReflectionType(rhs);
        });
    }

    dx9rend::stateManagerPush();
    unsigned int currentType = 0;
    unsigned int currentKey = 0;
    unsigned int begunPass = 0;
    void* currentEffect = 0;
    for (unsigned int pass = 0; pass < 2; ++pass)
    {
        for (void** cursor = first; cursor != last; ++cursor)
        {
            void* const geometry = *cursor;
            const unsigned int reflectionType = geometryReflectionType(geometry);
            const bool changed = currentType != reflectionType;
            if (changed)
            {
                if (currentType != 0)
                    dx9rend::effectEnd(currentEffect);
                currentKey = reflectionType;
                ShadowTree& effects = lmField<ShadowTree>(manager, 0x964);
                ShadowTreeNode* const node = shadowTreeFind(effects, currentKey);
                if (node == effects.head || !node->value)
                {
                    RD9_FRONTIER_AT("dice::hfe::geom::LightManager::renderDirectionalShadowMaps "
                                    "(mesh renderer shadow-effect registration)",
                                    manager, 0x19C);
                    std::abort();
                }
                currentEffect = node->value;
                dx9rend::effectBegin(currentEffect);
                dx9rend::effectBeginPass(currentEffect, pass);
            }
            if (begunPass != pass)
            {
                ShadowTree& effects = lmField<ShadowTree>(manager, 0x964);
                ShadowTreeNode* const node = shadowTreeFind(effects, reflectionType);
                if (node == effects.head || !node->value)
                {
                    RD9_FRONTIER_AT("dice::hfe::geom::LightManager::renderDirectionalShadowMaps "
                                    "(mesh renderer shadow-effect registration)",
                                    manager, 0x19C);
                    std::abort();
                }
                dx9rend::effectBeginPass(node->value, pass);
                begunPass = pass;
            }
            geometryDrawDirectionalBatch(geometry, pass != 0, changed ? 1 : 0);
            currentType = reflectionType;
        }
    }
    if (currentType != 0)
        dx9rend::effectEnd(currentEffect);
    dx9rend::stateManagerPop();
}

bool destroyableShadowPredicateBody(world::IObject* object)
{
#if BF2_PLUGIN_ABI_NATIVE
    world::IObjectTemplate* const objectTemplate = const_cast<world::IObjectTemplate*>(
        object->getTemplate());
    return objectTemplate->queryImplementation(50384u) != 0;
#else
    world::IObjectTemplate* const objectTemplate = const_cast<world::IObjectTemplate*>(
        objectTemplatePointer(object));
    return objectTemplate->queryImplementation(50384u) != 0;
#endif
}

#if BF2_PLUGIN_ABI_NATIVE
class DestroyableShadowPredicator : public world::ObjectPredicator
{
public:
    virtual bool predicate(world::IObject* object)
    {
        return destroyableShadowPredicateBody(object);
    }
    virtual unsigned int getFlags() const { return 0; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }
};
#else
struct DestroyableShadowPredicator
{
    const void** table;
};
bool __fastcall destroyableShadowPredicate(void*, void*, world::IObject* object)
{
    return destroyableShadowPredicateBody(object);
}
unsigned int __fastcall destroyableShadowFlags(void*, void*) { return 0; }
bool __fastcall destroyableShadowCull(void*, void*, world::IObject*) { return true; }
bool __fastcall destroyableShadowSurface(void*, void*, int) { return true; }
const void* const kDestroyableShadowVtable[4] = {
    reinterpret_cast<void*>(&destroyableShadowPredicate),
    reinterpret_cast<void*>(&destroyableShadowFlags),
    reinterpret_cast<void*>(&destroyableShadowCull),
    reinterpret_cast<void*>(&destroyableShadowSurface)
};
#endif

void constructDestroyableShadowPredicator(DestroyableShadowPredicator& predicate)
{
#if BF2_PLUGIN_ABI_NATIVE
    (void)predicate;
#else
    predicate.table = const_cast<const void**>(kDestroyableShadowVtable);
#endif
}

// 0x101664BE..0x10166500 (the followers) and 0x10166B02..0x10166B47 (per record in the
// quality-three map): +0x934 cleared (0x10032330(0, 0)), then ObjectManager's static grid (+0x7C,
// no predicate) and dynamic grid (+0x84) through the static "destroyable" predicator 0x1023A130
// ({vptr 0x1021EC58, 0}; slot 0, 0x10169A70, is the object template's +0x4C implementation test) -
// both with null bounds, type 2 and `true`. The sweep filter belongs to the followers alone; the
// quality-three map queries without it.
void queryStaticShadowCasters(void* manager, const Frustum& frustum)
{
    vc71::vector_layout& destination = lmField<vc71::vector_layout>(manager, 0x934);
    clearPointerVector(destination);
    DestroyableShadowPredicator predicate;
    constructDestroyableShadowPredicator(predicate);
#if BF2_PLUGIN_ABI_NATIVE
    std::vector<world::IObject*> hostObjects;
    world::ObjectManager* const objects =
        reinterpret_cast<world::ObjectManager*>(objectManager);
    objects->getStaticObjectsFromGridRend(frustum, hostObjects, 0, 0, 2, true);
    objects->getDynamicObjectsFromGridRend(frustum, hostObjects, 0, &predicate, 2, true);
    vc71::vectorRef<world::IObject*> output(&destination);
    for (std::vector<world::IObject*>::const_iterator it = hostObjects.begin();
         it != hostObjects.end(); ++it)
        output.push_back(*it);
#else
    world::ObjectManager* const objects =
        reinterpret_cast<world::ObjectManager*>(objectManager);
    objects->getStaticObjectsFromGridRend(
        frustum, hostObjectVector(destination), 0, 0, 2, true);
    objects->getDynamicObjectsFromGridRend(
        frustum, hostObjectVector(destination), 0,
        reinterpret_cast<world::ObjectPredicator*>(&predicate), 2, true);
#endif
}

// 0x101652CF..0x10165310 (after the TSM target is bound) and 0x101669A2..0x10166A60 (per record in
// the quality-three map): the scissor alone - {0, 0, size, size} inset by three pixels through
// InflateRect, then device +0x12C. No viewport.
void setShadowInsetScissor(void* manager)
{
    const int size = lmField<int>(manager, 0x204);
    RECT rectangle = { 0, 0, size, size };
    ::InflateRect(&rectangle, -3, -3);
    lmField<IDirect3DDevice9*>(manager, 0x908)->SetScissorRect(&rectangle);
}

// the full-size viewport (0, 0, size, size, 0, 1) and the full-size scissor,
// once, after the quality-three collection and before the static map. (G3: the old `inset` flag
// is gone - its one `true` caller was the TSM bind, which the PE scissors without a viewport.)
void setShadowViewportAndScissor(void* manager)
{
    const int size = lmField<int>(manager, 0x204);
    D3DVIEWPORT9 viewport;
    viewport.X = 0;
    viewport.Y = 0;
    viewport.Width = static_cast<DWORD>(size);
    viewport.Height = static_cast<DWORD>(size);
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    IDirect3DDevice9* const device = lmField<IDirect3DDevice9*>(manager, 0x908);
    device->SetViewport(&viewport);
    RECT rectangle = { 0, 0, size, size };
    device->SetScissorRect(&rectangle);
}

// the TSM target. G3: the PE sets NO viewport here, only the inset
// scissor; the old body also set a full-size viewport.
void bindDirectionalShadowTarget(void* manager)
{
    setLightManagerDepthStencil(lmField<IDirect3DSurface9*>(manager, 0x228));
    if (lmField<unsigned int>(manager, 0x910) != 0)
    {
        setRenderTargetTexture(0,
            lmField<dx9rend::EngineTexture*>(manager, 0x21C));
        dx9rend::stateManagerClear(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                   0xFFFFFFFFu, 1.0f, 0);
    }
    else
    {
        setRenderTargetTexture(0,
            lmField<dx9rend::EngineTexture*>(manager, 0x224));
        dx9rend::stateManagerClear(D3DCLEAR_ZBUFFER, 0xFFFFFFFFu, 1.0f, 0);
        dx9rend::stateManagerSetRenderState(195,
            lmField<unsigned int>(manager, 0x82C));
        dx9rend::stateManagerSetRenderState(175,
            lmField<unsigned int>(manager, 0x830));
    }
    setShadowInsetScissor(manager);
}

void appendDirectionalGeometry(void* manager, void* geometry)
{
    vc71::vectorRef<void*> output(&lmField<vc71::vector_layout>(manager, 0x924));
    output.push_back(geometry);
}

// Fits the caster frustum to candidate geometry from a unit light-space orthographic box.
// Sphere casters use radius*sqrt(2) bounds; others require collision LOD zero.
void buildDirectionalCasterFrustum(void* manager, Frustum& casterFrustum)
{
    Mat4 basis;                                                            // [ebp-0xA8]
    buildShadowLightBasis(lmField<world::LightSource*>(manager, 0x14), basis);
    vc71::vector_layout& candidates = lmField<vc71::vector_layout>(manager, 0x914);
    const Vec3& first =
        objectAbsolutePosition(*vectorBegin<world::IObject*>(candidates)); // [0x918][0] +0x68
    basis.m[12] = basis.m[12] + first.x;
    basis.m[13] = basis.m[13] + first.y;
    basis.m[14] = basis.m[14] + first.z;
    casterFrustum.setOrtho(1.0f, 1.0f, 0.01f, 1.0f, basis);

    for (world::IObject** it = vectorBegin<world::IObject*>(candidates);
         it != reinterpret_cast<world::IObject**>(candidates.last); ++it)
    {
        world::IObject* const object = *it;
        void* const geometry = objectGeometry(object);                     // [object+0x38]
        if (!geometry)
            continue;
        if (!geometryTemplate(geometry)->slot_0x94()                       // template +0x94
            && (candidateFlags(object) & 0x04000000u) == 0)                // `test byte [esi+7], 4`
        {
            if (!objectHasCollisionLod(object, 0))                         // +0xC8(0), `cmp al, 1`
                continue;
            void* const collision = objectCollisionMesh(object);           // +0x40, read first
            const Mat4& transform = objectAbsoluteTransformation(object);  // +0x80
            const LocalBoundingBox* const local = collisionBoundingBox(collision, 0);   // +0x54(0)
            const ShadowBoxCorners box(*local, transform);
            fitShadowFrustumAllPlanes(casterFrustum, box.corner, true);
            continue;
        }

        // The scale is a guarded static (0x10260A30, guard 0x10260A34 bit 0)
        // - sqrt of the double 2.0, stored as a float.
        static const float s_sphereScale = static_cast<float>(std::sqrt(2.0));
        const Vec3& centre = objectAbsolutePosition(object);               // +0x68, first
        const float radius = objectVisualRadius(object) * s_sphereScale;   // +0x90, [ebp-8]
        LocalBoundingBox sphere;                                           // [ebp-0x60]
        sphere.m_max.x = radius + centre.x;
        sphere.m_max.y = radius + centre.y;
        sphere.m_max.z = radius + centre.z;
        sphere.m_min.x = centre.x - radius;
        sphere.m_min.y = centre.y - radius;
        sphere.m_min.z = centre.z - radius;
        fitShadowFrustumToAabb(casterFrustum, sphere);
    }
}

// PE BSS dword 0x10260A2C. Exactly ONE reference in the whole image - the read at and no
// writer, so it is always the loader's zero. Kept, like StaticMesh.cpp's g_staticMeshShadowDrawMode,
// as the mutable file-static the switch below reads. [INFERRED] name.
int g_directionalCasterFrustumMode;

// the switch on that dword: debug nudges of the caster frustum by the
// follower distance d (+0x16C) along its near-plane normal. 1: +(n d / 2) / 100 and far + d / 100;
// 2: +n d; 3: -n d and near - d; 4: -n d and far + d. With the shipped zero no arm runs.
void applyDirectionalCasterFrustumMode(void* manager, Frustum& casterFrustum)
{
    const int mode = g_directionalCasterFrustumMode;
    if (mode != 1 && mode != 2 && mode != 3 && mode != 4)
        return;

    Mat4 transformation = casterFrustum.m_transformation;                  // [ebp-0x64], 0x10030DD0
    const float distance = lmField<float>(manager, 0x16C);
    const Vec3 normal = casterFrustum.m_planes[4].normal;                  // +0x40
    const float x = normal.x * distance;
    const float y = normal.y * distance;                                   // [ebp-0x1C]
    const float z = normal.z * distance;                                   // [ebp-0x18]
    if (mode == 1)
    {
        const float halfZ = z * 0.5f;                                      // [ebp-0x18] again
        const float offsetX = x * 0.5f * 0.01f;                            // [ebp-0x234]
        const float offsetY = y * 0.5f * 0.01f;
        const float offsetZ = halfZ * 0.01f;
        transformation.m[12] = transformation.m[12] + offsetX;
        transformation.m[13] = offsetY + transformation.m[13];
        transformation.m[14] = offsetZ + transformation.m[14];
        casterFrustum.m_farDist = distance * 0.01f + casterFrustum.m_farDist;
    }
    else if (mode == 2)
    {
        transformation.m[12] = transformation.m[12] + x;
        transformation.m[13] = y + transformation.m[13];
        transformation.m[14] = z + transformation.m[14];
    }
    else
    {
        transformation.m[12] = transformation.m[12] - x;
        transformation.m[13] = transformation.m[13] - y;
        transformation.m[14] = transformation.m[14] - z;
        if (mode == 3)
            casterFrustum.m_nearDist = casterFrustum.m_nearDist - distance;
        else
            casterFrustum.m_farDist = casterFrustum.m_farDist + distance;
    }
    casterFrustum.transform(transformation);
}

// 0x10165951..0x10165AC9, and again after the collection (0x10166798..0x10166910): the caster
// frustum's view (0x10079E00, D3DXMatrixLookAtLH) times its ortho projection (0x10079760,
// D3DXMatrixOrthoLH) through Mat4::fullMult (0x10077A30), both temporaries seeded with identity.
void directionalCasterViewProjection(const Frustum& casterFrustum, Mat4& output)
{
    Mat4 view = Mat4::getIdentity();
    Mat4 projection = Mat4::getIdentity();
    shadowFrustumViewMatrix(casterFrustum, view);
    shadowFrustumOrthoProjection(casterFrustum, projection);
    output.fullMult(view, projection);
}

// Appends a directional-shadow record for an object with geometry and gathers its children.
// First-person records can remain partially initialized when their draw flag is disabled.
// Fits the light frustum to grown bounds and excludes seated first-person geometry from drawing.
void buildDirectionalRecord(void* manager, world::IObject* object, Mat4& lightBasis,
                            const Mat4& casterViewProjection, const Mat4& shadowMatrix)
{
    void* const rootGeometry = objectGeometry(object);                     // [ebp-4]
    if (!rootGeometry)
        return;

    vc71::vector_layout& records = lmField<vc71::vector_layout>(manager, 0x79C);
    {
        vc71::vectorRef<XpRenderRecordLayout> output(&records);
        output.push_back(XpRenderRecordLayout());
    }
    XpRenderRecordLayout* const record =
        reinterpret_cast<XpRenderRecordLayout*>(records.last) - 1;
    record->geometry = rootGeometry;
    record->source = lmField<world::LightSource*>(manager, 0x14);
    record->flags448[0] = 0;
    void* const player = localHumanPlayerRaw();                            // [ebp-8], per candidate
    if (player)
        record->flags448[0] = (playerHasFirstPersonViewRaw(player)
                               && playerVehicleRaw(player) == object) ? 1 : 0;
    if (record->flags448[0] && !lmField<unsigned char>(manager, 0x7ED))
        return;

    geometryPrepareShadow(rootGeometry, rendererShadowToken());            // +0xAC

    ShadowBoxCorners box;                                                  // [ebp-0x18C], zeroed
    void* const collision = objectCollisionMesh(object);                   // [ebp-0xC]
    if (collision && static_cast<world::ICollisionMesh*>(collision)->hasLod(0))   // +0x28(0) == 1
    {
        box.setBounds(*collisionBoundingBox(collision, 0));                // +0x54
        box.transform(objectAbsoluteTransformation(object));               // +0x80,
    }
    else
    {
        LocalBoundingBox sphere;                                           // [ebp-0x60]
        const float radius = objectVisualRadius(object);                   // +0x90
        const Vec3& centre = objectAbsolutePosition(object);               // +0x68
        const float maximumX = radius + centre.x;                          // [ebp-0x20]
        const float maximumY = radius + centre.y;                          // [ebp-0x1C]
        const float maximumZ = radius + centre.z;                          // [ebp-0x18]
        const float radiusAgain = objectVisualRadius(object);              // +0x90, again
        const Vec3& centreAgain = objectAbsolutePosition(object);          // +0x68, again
        sphere.m_min.x = centreAgain.x - radiusAgain;
        sphere.m_min.y = centreAgain.y - radiusAgain;
        sphere.m_min.z = centreAgain.z - radiusAgain;
        sphere.m_max.x = maximumX;
        sphere.m_max.y = maximumY;
        sphere.m_max.z = maximumZ;
        box.setBounds(sphere);
    }

    LocalBoundingBox bounds;                                               // [ebp-0xA4]
    const Vec3 maximum = box.maximum();                                    // 0x101C6C70, first
    const Vec3 minimum = box.minimum();
    bounds.m_min = minimum;
    bounds.m_max = maximum;
    bounds.m_derivedDataDirty = true;
    finalizeShadowBounds(bounds);

    lightBasis.m[12] = bounds.m_centre.x;
    lightBasis.m[13] = bounds.m_centre.y;
    lightBasis.m[14] = bounds.m_centre.z;
    record->frustum.setOrtho(0.1f, 0.1f, 0.01f, 0.1f, lightBasis);
    fitShadowFrustumToAabb(record->frustum, bounds);
    finalizeShadowBounds(bounds);                                          // 0x101C5EB0, again
    collectDirectionalChildren(manager, object, bounds);
    fitShadowFrustumNearPlaneToAabb(record->frustum, bounds);
    record->matrices[1] = casterViewProjection;                            // +0x238
    record->matrices[2] = shadowMatrix;                                    // +0x278

    vc71::vector_layout& children = lmField<vc71::vector_layout>(manager, 0x954);
    for (world::IObject** it = vectorBegin<world::IObject*>(children);
         it != reinterpret_cast<world::IObject**>(children.last); ++it)
    {
        void* const geometry = objectGeometry(*it);                        // [ebp-0x68]
        if (!geometry || !supportedShadowGeometry(geometryReflectionType(geometry)))
            continue;
        void* const control = player
            ? findPlayerControlObject(playerVehicleRaw(player)) : 0;       // [ebp-4]
        world::IObject* const owner = geometryObject(geometry);            // +0xA0, [ebp-0x14]
        bool drawn = !record->flags448[0];
        if (drawn && player)
        {
            if (playerHasFirstPersonViewRaw(player) && owner)
            {
                world::IObject* const ownerParent = objectParent(owner);   // [owner+0x28]
                if (playerVehicleRaw(player) == ownerParent)
                    drawn = false;
            }
            if (drawn && playerHasFirstPersonViewRaw(player) && control
                && controlChildSoldierActive(control)                      // PCO +0x140
                && controlOccupyingObject(control) == owner)               // PCO +0x14
                drawn = false;
        }
        geometryAttachShadowRecord(geometry, record);                      // +0x24
        if (drawn)
            appendDirectionalGeometry(manager, geometry);
    }
}

// Queries followers using the pulled-back shadow frustum and the pass's camera classifier.
// Attaches geometry to the first shadow record and extends the caster near plane over
// collision bounds. shifted remains available for quality-three collection.
void attachDirectionalFollowers(void* manager, Frustum& shifted, Frustum& casterFrustum,
                                const ShadowSweepClassifier& classifier)
{
    vc71::vector_layout& records = lmField<vc71::vector_layout>(manager, 0x79C);
    XpRenderRecordLayout* const firstRecord =
        vectorBegin<XpRenderRecordLayout>(records);                        // [ebp-0x24]
    shiftShadowFrustumTowardLight(shifted, lmField<float>(manager, 0x16C));
    queryStaticShadowCasters(manager, shifted);
    vc71::vector_layout& followers = lmField<vc71::vector_layout>(manager, 0x934);
    if (lmField<unsigned char>(manager, 0x170))
        filterShadowCandidates(classifier, followers);

    for (world::IObject** it = vectorBegin<world::IObject*>(followers);
         it != reinterpret_cast<world::IObject**>(followers.last); ++it)
    {
        world::IObject* const object = *it;                                // [ebp-0xC]
        void* const geometry = objectGeometry(object);
        if (!geometry)
            continue;
        geometryAttachShadowRecord(geometry, firstRecord);                 // +0x24
        geometryPrepareShadow(geometry, rendererShadowToken());            // +0xAC
        void* const collision = objectCollisionMesh(object);               // +0x40
        if (collision && static_cast<world::ICollisionMesh*>(collision)->hasLod(0))   // +0x28(0)
        {
            const Mat4& transform = objectAbsoluteTransformation(object);  // +0x80, first
            const LocalBoundingBox* const local = collisionBoundingBox(collision, 0);   // +0x54(0)
            const ShadowBoxCorners followerBox(*local, transform);
            fitShadowFrustumNearPlane(casterFrustum, followerBox.corner);
        }
    }
}

// dword_1024B930, the Terrain: two direct calls the quality-three arm makes, both to bodies that are
// empty in RendDX9 1.50 - 0x100D8530 is `retn 0Ch` (the records, the pulled-back light frustum, the
// caster frustum) and 0x100D8540 is `retn 4` (the first record). Neither touches the terrain, so a
// level without one is harmless here. Recovered empty bodies, not stubs; [INFERRED] names.
void terrainCollectShadowGeometry(IBase* /*terrain*/, vc71::vector_layout* /*records*/,
                                  const Frustum* /*shifted*/, Frustum* /*casterFrustum*/)
{
}

void terrainDrawShadowMap(IBase* /*terrain*/, XpRenderRecordLayout* /*firstRecord*/)
{
}


// Returns the static-mesh renderer's borrowed shadow effect, or null if unavailable.
// The quality-3 shadow pass selects this effect independently of expansion settings.
inline dx9rend::Effect* staticMeshShadowEffect()
{
    return static_cast<StaticMeshRenderer*>(
        static_cast<void*>(g_StaticMeshRenderer))->m_shadowEffect;
}

// +0x20 as the quality-three map calls it (0x10166CA7..0x10166CBC): the record, pointers to the
// record's pulled-back frustum's view and ortho matrices, then three zeros. StaticMesh's body reads
// only the record (+0x1F8 = matrices[0], just written; +0x278; +0x495); the six-argument arity is
// the slot's everywhere (StaticMesh +0x20 is `retn 18h`).
inline void geometryDrawOccluderShadow(void* geometry, XpRenderRecordLayout* record,
                                       const Mat4* view, const Mat4* projection)
{
    typedef void (__fastcall* DrawShadowMapFn)(void*, void*, void*, const void*, const void*,
                                               unsigned int, unsigned int, unsigned int);
    reinterpret_cast<DrawShadowMapFn>(geometrySlot(geometry, 0x20))(
        geometry, 0, record, view, projection, 0u, 0u, 0u);
}

// +0x20 for the OverGrowth instances (0x10166D23..0x10166D32): the first record, then five zeros.
inline void geometryDrawRecordShadow(void* geometry, XpRenderRecordLayout* record)
{
    typedef void (__fastcall* DrawShadowMapFn)(void*, void*, void*, unsigned int, unsigned int,
                                               unsigned int, unsigned int, unsigned int);
    reinterpret_cast<DrawShadowMapFn>(geometrySlot(geometry, 0x20))(
        geometry, 0, record, 0u, 0u, 0u, 0u, 0u);
}

// the first half.
void collectHighQualityShadowCasters(void* manager, Frustum& shifted, Frustum& casterFrustum,
                                     unsigned int shadowQuality)
{
    if (!lmField<unsigned char>(manager, 0x268))
        return;
    if (shadowQuality < 3u)                                                // `jb`
        return;
    if (rendererSuppressesHighQualityShadows())
        return;
    vc71::vector_layout& records = lmField<vc71::vector_layout>(manager, 0x79C);
    // dword_10258F38 is dereferenced with no null test (0x101665E0), as shipped: the ECCore
    // CEPreLoad arm creates it before any level renders and CEUnloadLevel nulls it.
    static_cast<OverGrowth*>(dx9rend::g_OverGrowth)->collectShadowGeometry(
        reinterpret_cast<OverGrowthShadowVolumes*>(&records),
        reinterpret_cast<OverGrowthShadowBounds*>(&shifted),
        reinterpret_cast<OverGrowthShadowState*>(&casterFrustum),
        reinterpret_cast<OverGrowthGeometryVector*>(
            &lmField<vc71::vector_layout>(manager, 0x970)));
    terrainCollectShadowGeometry(g_Terrain, &records, &shifted, &casterFrustum);
}

// the second half: the static-occluder map.
void renderHighQualityShadowMap(void* manager, const Frustum& casterFrustum,
                                Mat4& casterViewProjection, unsigned int shadowQuality)
{
    if (!lmField<unsigned char>(manager, 0x268))
        return;

    // the second target, bound and cleared.
    if (shadowQuality >= 3u && !rendererSuppressesHighQualityShadows())
    {
        if (lmField<unsigned int>(manager, 0x910) == 0)
        {
            setLightManagerDepthStencil(lmField<IDirect3DSurface9*>(manager, 0x22C));
            dx9rend::stateManagerSetRenderState(195, 0);                   // D3DRS_DEPTHBIAS
            dx9rend::stateManagerSetRenderState(175, 0);                   // D3DRS_SLOPESCALEDEPTHBIAS
        }
        else
        {
            setRenderTargetTexture(0, lmField<dx9rend::EngineTexture*>(manager, 0x220));
        }
        dx9rend::stateManagerClear(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFFFFFFFu, 1.0f, 0);
    }

    // 0x10166752..0x10166792: +0x268 is read again and isExpansionPack is asked twice more.
    if (!lmField<unsigned char>(manager, 0x268) || shadowQuality < 3u
        || rendererSuppressesHighQualityShadows() || rendererSuppressesHighQualityShadows())
        return;

    directionalCasterViewProjection(casterFrustum, casterViewProjection);  // the grown frustum
    dx9rend::stateManagerPush();
    // Always "DrawShadowMap" - never the "DrawShadowMapNV" the
    // StaticMeshRenderer's constructor selects on nvidia - and never restored: after the first
    // quality-three frame the StaticMesh Effect keeps this technique for the TSM pass too. The
    // selection's result is not tested.
    dx9rend::effectSetTechniqueByName(staticMeshShadowEffect(), vc71::string("DrawShadowMap"));
    dx9rend::effectBegin(staticMeshShadowEffect());
    dx9rend::effectBeginPass(staticMeshShadowEffect(), 1);

    vc71::vector_layout& records = lmField<vc71::vector_layout>(manager, 0x79C);
    vc71::vector_layout& occluders = lmField<vc71::vector_layout>(manager, 0x934);
    for (XpRenderRecordLayout* record = vectorBegin<XpRenderRecordLayout>(records);
         record != reinterpret_cast<XpRenderRecordLayout*>(records.last); ++record)
    {
        record->matrices[0] = casterViewProjection;                        // +0x1F8
        setShadowInsetScissor(manager);
        Frustum shifted(record->frustum);                                  // [ebp-0x688], 0x100F58A0
        shiftShadowFrustumTowardLight(shifted, lmField<float>(manager, 0x16C));
        queryStaticShadowCasters(manager, shifted);                        // no sweep filter here
        Mat4 view = Mat4::getIdentity();                                   // [ebp-0xA8]
        Mat4 projection = Mat4::getIdentity();                             // [ebp-0x64]
        shadowFrustumViewMatrix(shifted, view);
        shadowFrustumOrthoProjection(shifted, projection);
        for (world::IObject** it = vectorBegin<world::IObject*>(occluders);
             it != reinterpret_cast<world::IObject**>(occluders.last); ++it)
        {
            void* const geometry = objectGeometry(*it);                    // [ebp-0xC]
            if (!geometry)
                continue;
            if (geometryReflectionType(geometry) != kStaticMeshReflectionType)   // ds:1021AF04
                continue;
            geometryDrawOccluderShadow(geometry, record, &view, &projection);
        }
    }

    // with records, the OverGrowth instances against the first record, then
    // the Terrain's (empty) draw hook.
    if (records.first && vectorSize<XpRenderRecordLayout>(records) != 0)
    {
        XpRenderRecordLayout* const firstRecord = vectorBegin<XpRenderRecordLayout>(records);
        vc71::vector_layout& overGrowthGeometry = lmField<vc71::vector_layout>(manager, 0x970);
        for (void** it = vectorBegin<void*>(overGrowthGeometry);
             it != reinterpret_cast<void**>(overGrowthGeometry.last); ++it)
            geometryDrawRecordShadow(*it, firstRecord);
        terrainDrawShadowMap(g_Terrain, firstRecord);
    }
    dx9rend::effectEnd(staticMeshShadowEffect());
    dx9rend::stateManagerPop();
}


// Builds directional-shadow candidates, transforms, records, and TSM draws, then attaches
// followers and renders the quality-three static map. Resets viewport/scissor afterward.

void renderDirectionalShadowMaps(void* manager)
{
    // renderer +0x74, then VideoOptions +0x04 with 4 - asked once, kept in
    // [ebp-0xAC] for the four quality-three tests. Every test is unsigned (`jb`).
    const unsigned int shadowQuality =
        static_cast<unsigned int>(dx9rend::rendererGetVideoOptions()->getQuality(4));

    // five `resize(0, 0)`s - 0x10032330 on the object vectors +0x914 /
    // +0x934, 0x10168B50 on the geometry vectors +0x924 / +0x944 / +0x970.
    clearPointerVector(lmField<vc71::vector_layout>(manager, 0x914));
    clearPointerVector(lmField<vc71::vector_layout>(manager, 0x924));
    clearPointerVector(lmField<vc71::vector_layout>(manager, 0x934));
    clearPointerVector(lmField<vc71::vector_layout>(manager, 0x944));
    clearPointerVector(lmField<vc71::vector_layout>(manager, 0x970));

    Mat4 lightBasis;                                                       // [ebp-0x228]
    ShadowSweepClassifier classifier;                                      // [ebp-0x27C]
    buildShadowCandidateSet(manager, lightBasis, classifier);

    // the records `resize(0, Record())`. no
    // candidate, no pass - the classifier's two vectors are released on the way out.
    clearShadowRecordVector(lmField<vc71::vector_layout>(manager, 0x79C));
    vc71::vector_layout& candidates = lmField<vc71::vector_layout>(manager, 0x914);
    if (vectorSize<world::IObject*>(candidates) == 0)
        return;

    bindDirectionalShadowTarget(manager);

    Frustum casterFrustum;                                                 // [ebp-0x4A0], 0x10030F30
    Mat4 casterViewProjection = Mat4::getIdentity();                       // [ebp-0xEC]
    // the TSM fitter's inputs and the fitter, 0x101626B0.
    const Mat4 shadowMatrix = calculateDirectionalShadowMatrix(manager);

    buildDirectionalCasterFrustum(manager, casterFrustum);
    // the caster frustum, with the debug byte +0x788 set, to DebugRenderer
    // +0x28 (colour 0). Reached only through shadowManager.shadowShowFrustums (see the first stop).
    if (lmField<unsigned char>(manager, 0x788))
    {
        RD9_FRONTIER_AT("dice::hfe::geom::LightManager::renderDirectionalShadowMaps "
                        "(DebugRenderer::drawFrustum - the +0x788 caster frustum draw)",
                        g_DebugRenderer, 0x28);
        std::abort();
    }
    applyDirectionalCasterFrustumMode(manager, casterFrustum);             // dead, see above
    directionalCasterViewProjection(casterFrustum, casterViewProjection);

    // a record per candidate with a geometry; the end is re-read.
    for (world::IObject** it = vectorBegin<world::IObject*>(candidates);
         it != reinterpret_cast<world::IObject**>(candidates.last); ++it)
        buildDirectionalRecord(manager, *it, lightBasis, casterViewProjection, shadowMatrix);

    submitDirectionalGeometry(manager);

    // only with records - a null or empty vector resumes at 0x10166621.
    // So without records (every candidate lacked a geometry) the followers and the OverGrowth
    // query are skipped but the static map below still binds, clears and begins its Effect.
    vc71::vector_layout& records = lmField<vc71::vector_layout>(manager, 0x79C);
    if (records.first && vectorSize<XpRenderRecordLayout>(records) != 0)
    {
        Frustum shifted(lmField<Frustum>(manager, 0x278));                 // [ebp-0x688], 0x100F58A0
        attachDirectionalFollowers(manager, shifted, casterFrustum, classifier);
        collectHighQualityShadowCasters(manager, shifted, casterFrustum, shadowQuality);
    }

    // the grown caster frustum, with +0x788 set, to DebugRenderer +0x28
    // (colour 0xFF0000). Reached only through shadowManager.shadowShowFrustums (see the first stop).
    if (lmField<unsigned char>(manager, 0x788))
    {
        RD9_FRONTIER_AT("dice::hfe::geom::LightManager::renderDirectionalShadowMaps "
                        "(DebugRenderer::drawFrustum - the +0x788 grown caster frustum draw)",
                        g_DebugRenderer, 0x28);
        std::abort();
    }

    setShadowViewportAndScissor(manager);
    renderHighQualityShadowMap(manager, casterFrustum, casterViewProjection, shadowQuality);
}

Mat4 shadowClipBias()
{
    Mat4 result = Mat4::getIdentity();
    result.m[0] = 0.5f;
    result.m[5] = -0.5f;
    result.m[12] = 0.5f;
    result.m[13] = 0.5f;
    return result;
}

void rebuildRecordAtlasMatrix(XpRenderRecordLayout& record)
{
    record.matrices[8] = Mat4::getIdentity();
    record.matrices[8].m[0] = record.atlasScaleOffset[2];
    record.matrices[8].m[5] = record.atlasScaleOffset[3];
    record.matrices[8].m[12] = record.atlasScaleOffset[0];
    record.matrices[8].m[13] = record.atlasScaleOffset[1];
}

// The two object-transform dispatches in the directional arm are deliberately
// repeated; retail does not cache the first result.
void finalizeShadowRecords(void* manager)
{
    const Mat4 bias = shadowClipBias();
    vc71::vector_layout& directional = lmField<vc71::vector_layout>(manager, 0x79C);
    XpRenderRecordLayout* records = vectorBegin<XpRenderRecordLayout>(directional);
    unsigned int count = vectorSize<XpRenderRecordLayout>(directional);
    for (unsigned int i = 0; i < count; ++i)
    {
        XpRenderRecordLayout& record = records[i];
        rebuildRecordAtlasMatrix(record);
        record.matrices[4] = multiplyShadowMatrices(record.matrices[1], bias);
        record.matrices[5] = multiplyShadowMatrices(record.matrices[0], bias);
        world::IObject* const firstObject = geometryFinalizerObject(record.geometry);
        record.matrices[3] = multiplyShadowMatrices(
            objectAbsoluteTransformation(firstObject), record.matrices[4]);
        record.matrices[7] = multiplyShadowMatrices(record.matrices[2], bias);
        world::IObject* const secondObject = geometryFinalizerObject(record.geometry);
        record.matrices[6] = multiplyShadowMatrices(
            objectAbsoluteTransformation(secondObject), record.matrices[7]);
        record.atlasBounds[0] = 0.0f;
        record.atlasBounds[1] = 0.0f;
        record.atlasBounds[2] = 1.0f;
        record.atlasBounds[3] = 1.0f;
    }

    vc71::vector_layout& secondary = lmField<vc71::vector_layout>(manager, 0x7AC);
    records = vectorBegin<XpRenderRecordLayout>(secondary);
    count = vectorSize<XpRenderRecordLayout>(secondary);
    for (unsigned int i = 0; i < count; ++i)
    {
        XpRenderRecordLayout& record = records[i];
        rebuildRecordAtlasMatrix(record);
        record.matrices[4] = multiplyShadowMatrices(
            record.matrices[1], record.matrices[2]);
        // PE dword 0x1023A098 ships as one and has no writer.
        record.matrices[4] = multiplyShadowMatrices(record.matrices[4], bias);
        record.matrices[4] = multiplyShadowMatrices(
            record.matrices[4], record.matrices[8]);
        record.atlasBounds[0] = record.atlasScaleOffset[0];
        record.atlasBounds[1] = record.atlasScaleOffset[1];
        record.atlasBounds[2] = record.atlasScaleOffset[0] + record.atlasScaleOffset[2];
        record.atlasBounds[3] = record.atlasScaleOffset[1] + record.atlasScaleOffset[3];
    }
}

void destroyShadowRecordVector(vc71::vector_layout& vector)
{
    clearShadowRecordVector(vector);
    destroyVectorStorage(vector);
}

void destroyLightManager(void* self)
{
    // Restore all three most-derived tables before touching any owned state.
    lmField<const void**>(self, 0x00) = const_cast<const void**>(kPrimaryVtable);
    lmField<const void**>(self, 0x04) = const_cast<const void**>(kDeviceLostVtable);
    lmField<const void**>(self, 0x08) = const_cast<const void**>(kRenderPathVtable);

    if (lmField<void*>(self, 0x190))
        ::renddx9abi::engineFree(lmField<void*>(self, 0x190));

    ShadowTree& effects = lmField<ShadowTree>(self, 0x964);
    releaseShadowTreeValues(effects.head->parent);

    static const unsigned int textureOffsets[] =
    {
        0x24C, 0x23C, 0x240, 0x244, 0x248, 0x8BC, 0x8B8
    };
    for (unsigned int i = 0; i < sizeof(textureOffsets) / sizeof(textureOffsets[0]); ++i)
        dx9rend::textureReleaseRef(
            lmField<dx9rend::EngineTexture*>(self, textureOffsets[i]));

    static const unsigned int parameterOffsets[] =
    {
        0x8B4, 0x8B0, 0x8AC, 0x8A8, 0x8A4, 0x8A0, 0x89C,
        0x898, 0x894, 0x874, 0x878, 0x88C, 0x890, 0x870,
        0x888, 0x884, 0x880, 0x87C, 0x86C, 0x868, 0x864,
        0x858, 0x860, 0x85C, 0x854, 0x84C, 0x848, 0x850
    };
    for (unsigned int i = 0; i < sizeof(parameterOffsets) / sizeof(parameterOffsets[0]); ++i)
    {
        dx9rend::ShaderParameter*& parameter =
            lmField<dx9rend::ShaderParameter*>(self, parameterOffsets[i]);
        if (parameter)
            parameter->release();
        parameter = 0;
    }
    dx9rend::Effect*& effect = lmField<dx9rend::Effect*>(self, 0x844);
    if (effect)
        dx9rend::effectRelease(effect);
    effect = 0;

    for (unsigned int offset = 0x254; offset <= 0x260; offset += 4)
        dx9rend::textureReleaseRef(lmField<dx9rend::EngineTexture*>(self, offset));
    lmField<void*>(self, 0x250) = 0;

    releaseShadowResources(self, true);
    if (lmField<world::LightSource*>(self, 0x14))
        lmField<world::LightSource*>(self, 0x14)->release();

    static const unsigned int objectOffsets[] = { 0x8F8, 0x8FC, 0x8D8, 0x8DC, 0x8E8, 0x8EC };
    for (unsigned int i = 0; i < sizeof(objectOffsets) / sizeof(objectOffsets[0]); ++i)
    {
        IUnknown*& object = lmField<IUnknown*>(self, objectOffsets[i]);
        if (object)
            object->Release();
        object = 0;
    }

    lmField<IDirect3DDevice9*>(self, 0x908)->Release();
    dx9rend::unregisterDeviceLostResetListener(
        reinterpret_cast<dx9rend::DeviceLostResetListener*>(
            static_cast<unsigned char*>(self) + 4));
    dx9rend::unregisterRenderPathListener(
        reinterpret_cast<dx9rend::RenderPathListener*>(
            static_cast<unsigned char*>(self) + 8));

    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x970));
    destroyShadowTree(effects, false);
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x954));
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x944));
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x934));
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x924));
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x914));
    destroyShadowTree(lmField<ShadowTree>(self, 0x8CC), false);
    destroyShadowTree(lmField<ShadowTree>(self, 0x8C0), false);
    destroyShadowRecordVector(lmField<vc71::vector_layout>(self, 0x7DC));
    destroyShadowRecordVector(lmField<vc71::vector_layout>(self, 0x7CC));
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x7BC));
    destroyShadowRecordVector(lmField<vc71::vector_layout>(self, 0x7AC));
    destroyShadowRecordVector(lmField<vc71::vector_layout>(self, 0x79C));
    destroyShadowTree(lmField<ShadowTree>(self, 0x1F0), false);
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x1E0));
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x1D0));
    lmActiveLights(self).destroy();
    lmOwnedLights(self).destroy();
    lmField<void*>(self, 0x17C) = 0;
    destroyVectorStorage(lmField<vc71::vector_layout>(self, 0x180));
}

// Every unrecovered primary slot has its own instantiated stopping body.  Its arguments are not
// declared: with the frontier trap armed it never returns, which is the only stack-safe behavior
// for an as-yet unknown __thiscall signature.  The byte offset is still carried into the diagnostic.
template <unsigned int Slot>
void __fastcall lm_unrecovered(void* self, void*)
{
    RD9_FRONTIER_AT("dice::hfe::geom::LightManager::unrecoveredVirtual", self, Slot);
    std::abort();
}

int __fastcall lm_addRef(void* self, void*)
{
    return ++lmField<int>(self, 0x10);
}

int __fastcall lm_getRef(void* self, void*)
{
    return lmField<int>(self, 0x10);
}

int __fastcall lm_release(void* self, void*)
{
    const int remaining = --lmField<int>(self, 0x10);
    if (remaining == 0)
    {
        destroyLightManager(self);
        ::renddx9abi::engineFree(self);
    }
    return remaining;
}

void* __fastcall lm_queryInterface(void* self, void*, unsigned int iid)
{
    if (iid == RD9_IID_IBASE || iid == IID_ILightManager)
        return self;
    return 0;
}

// LightManager::init calls this on every invocation, even after the sunlight at
// +0x840 has already been created.  D3DX return values are deliberately ignored: retail proceeds
// directly through every returned mesh pointer and therefore has no failure recovery arm here.
void buildLightDebugGeometry(void* self)
{
    IDirect3DDevice9* const device = lmField<IDirect3DDevice9*>(self, 0x908);

    RendD3dxMesh* sphereSource = 0;
    RendD3dxMesh* sphere = 0;
    D3DXCreateSphere(device, 1.0f, 20, 20, &sphereSource, 0);
    sphereSource->CloneMeshFVF(0x660u, D3DFVF_XYZ, device, &sphere);

    RendD3dxMesh* cylinderSource = 0;
    RendD3dxMesh* cylinder = 0;
    D3DXCreateCylinder(device, 0.0f, 1.0f, 1.0f, 20, 20, &cylinderSource, 0);
    cylinderSource->CloneMeshFVF(0x660u, D3DFVF_XYZ, device, &cylinder);

    RendD3dxMesh* boxSource = 0;
    RendD3dxMesh* box = 0;
    D3DXCreateBox(device, 1.0f, 1.0f, 1.0f, &boxSource, 0);
    boxSource->CloneMeshFVF(0x660u, D3DFVF_XYZ, device, &box);

    if (lmField<IDirect3DVertexBuffer9*>(self, 0x8F8))
        lmField<IDirect3DVertexBuffer9*>(self, 0x8F8)->Release();
    if (lmField<IDirect3DIndexBuffer9*>(self, 0x8FC))
        lmField<IDirect3DIndexBuffer9*>(self, 0x8FC)->Release();
    if (lmField<IDirect3DVertexBuffer9*>(self, 0x8D8))
        lmField<IDirect3DVertexBuffer9*>(self, 0x8D8)->Release();
    if (lmField<IDirect3DIndexBuffer9*>(self, 0x8DC))
        lmField<IDirect3DIndexBuffer9*>(self, 0x8DC)->Release();
    if (lmField<IDirect3DVertexBuffer9*>(self, 0x8E8))
        lmField<IDirect3DVertexBuffer9*>(self, 0x8E8)->Release();
    if (lmField<IDirect3DIndexBuffer9*>(self, 0x8EC))
        lmField<IDirect3DIndexBuffer9*>(self, 0x8EC)->Release();

    box->GetVertexBuffer(&lmField<IDirect3DVertexBuffer9*>(self, 0x8F8));
    box->GetIndexBuffer(&lmField<IDirect3DIndexBuffer9*>(self, 0x8FC));
    cylinder->GetVertexBuffer(&lmField<IDirect3DVertexBuffer9*>(self, 0x8E8));
    cylinder->GetIndexBuffer(&lmField<IDirect3DIndexBuffer9*>(self, 0x8EC));
    sphere->GetVertexBuffer(&lmField<IDirect3DVertexBuffer9*>(self, 0x8D8));
    sphere->GetIndexBuffer(&lmField<IDirect3DIndexBuffer9*>(self, 0x8DC));

    lmField<unsigned int>(self, 0x900) = box->GetNumVertices();
    lmField<unsigned int>(self, 0x904) = box->GetNumFaces();
    lmField<unsigned int>(self, 0x8F0) = cylinder->GetNumVertices();
    lmField<unsigned int>(self, 0x8F4) = cylinder->GetNumFaces();
    lmField<unsigned int>(self, 0x8E0) = sphere->GetNumVertices();
    lmField<unsigned int>(self, 0x8E4) = sphere->GetNumFaces();

    boxSource->Release();
    box->Release();
    cylinderSource->Release();
    cylinder->Release();
    sphereSource->Release();
    sphere->Release();
}

// [PLATFORM] Adapts the renderer's internal VC7.1 vector<bool> storage at +0x17C.
// Growth uses 1.5x capacity or the requested size; shrinking retains capacity and clears spare bits.
// No external STL object is reinterpreted as this layout.
class SimpleShadowMapIOStorage
{
public:
    explicit SimpleShadowMapIOStorage(void* manager)
        : count(lmField<unsigned int>(manager, 0x17C)),
          words(lmField<vc71::vector_layout>(manager, 0x180)) {}

    void set(unsigned int index, bool value)
    {
        unsigned int& word = reinterpret_cast<unsigned int*>(words.first)[index >> 5];
        const unsigned int mask = 1u << (index & 31u);
        if (value) word |= mask;
        else word &= ~mask;
    }

    void resize(unsigned int size, bool value)
    {
        const unsigned int oldSize = count;
        if (size == oldSize)
            return;
        const unsigned int wordCount = (size + 31u) >> 5;
        const unsigned int oldWords = vc71::vectorSizeIn(words, sizeof(unsigned int));
        const unsigned int capacity = words.first
            ? static_cast<unsigned int>(words.end - words.first) / sizeof(unsigned int) : 0u;
        if (wordCount > capacity)
        {
            if (wordCount > 0x3FFFFFFFu)
                throw std::length_error("vector<T> too long");
            unsigned int grown = capacity <= 0x3FFFFFFFu - (capacity >> 1)
                ? capacity + (capacity >> 1) : 0u;
            if (grown < wordCount) grown = wordCount;
            char* const block = static_cast<char*>(::operator new(grown * 4u));
            if (oldWords) memcpy(block, words.first, oldWords * 4u);
            if (words.first) ::operator delete(words.first);
            words.first = block;
            words.end = block + grown * 4u;
        }
        if (wordCount > oldWords)
            std::fill(reinterpret_cast<unsigned int*>(words.first) + oldWords,
                      reinterpret_cast<unsigned int*>(words.first) + wordCount, 0u);
        words.last = words.first ? words.first + wordCount * 4u : 0;
        count = size;
        if (size > oldSize)
        {
            for (unsigned int i = oldSize; i < size; ++i) set(i, value);
        }
        else if (size & 31u)
        {
            reinterpret_cast<unsigned int*>(words.first)[wordCount - 1u] &=
                (1u << (size & 31u)) - 1u;
        }
    }

    void fill(bool value)
    {
        // PE 100AED90 fills only [begin,end), leaving the unused high bits alone.
        for (unsigned int i = 0; i < count; ++i) set(i, value);
    }

private:
    unsigned int& count;
    vc71::vector_layout& words;
};

const renddx9abi::HostString& simpleShadowStringSetting(
    const renddx9abi::HostString& key, const renddx9abi::HostString& fallback)
{
#if BF2_PLUGIN_ABI_NATIVE
    return settings->stringGet(key, fallback);
#else
    // [PLATFORM] PE settings +0x40 returns a reference to a VC7.1 string. The native branch uses
    // the recovered SettingsRepostitory signature; only this legacy carrier needs an adapter.
    typedef const renddx9abi::HostString& (__thiscall* GetString)(
        void*, const renddx9abi::HostString&, const renddx9abi::HostString&);
    return reinterpret_cast<GetString>((*reinterpret_cast<void***>(settings))[0x40 / 4])(
        settings, key, fallback);
#endif
}

renddx9abi::HostString simpleShadowLevelPath()
{
    const renddx9abi::HostString levelDefault("");
    const renddx9abi::HostString levelKey("GSLoadLevel");
    const renddx9abi::HostString directoryDefault("");
    const renddx9abi::HostString directoryKey("GSLevelDirectory");
    const renddx9abi::HostString& level = simpleShadowStringSetting(levelKey, levelDefault);
    const renddx9abi::HostString& directory = simpleShadowStringSetting(directoryKey, directoryDefault);
    renddx9abi::HostString path(directory);
    path += level;
    return path;
}

void loadSimpleShadowMapsBody(void* self)
{
    if (lmField<unsigned char*>(self, 0x190))
        ::renddx9abi::engineFree(lmField<unsigned char*>(self, 0x190));

    world::IHeightmapCluster* const cluster = static_cast<world::IHeightmapCluster*>(
        static_cast<void*>(classManager->getSingleton(renddx9abi::HostString("HeightmapCluster"))));
    // IHeightmapCluster PE +0x2C / ELF +0x30 is getHeightmapSize, not getWorldSize.
    const int size = cluster->getHeightmapSize() / 2;
    lmField<int>(self, 0x19C) = (size + 8) / 2;
    const int rowOrigin = (size + 2) / 2;
    lmField<int>(self, 0x1A4) = rowOrigin;
    lmField<int>(self, 0x198) = size + 8;
    lmField<int>(self, 0x1A8) = (size + 8) / 4;
    lmField<int>(self, 0x1A0) = size + 2;
    lmField<float>(self, 0x194) = static_cast<float>(size);
    lmField<int>(self, 0x1AC) = ((size + 8) / 4) / 2;
    lmField<int>(self, 0x1B0) = rowOrigin;
    const unsigned int packedBytes = lmField<int>(self, 0x1A0) * lmField<int>(self, 0x1A8);
    lmField<unsigned char*>(self, 0x190) =
        static_cast<unsigned char*>(::renddx9abi::engineAlloc(packedBytes));
    memset(lmField<unsigned char*>(self, 0x190), 0xFF, packedBytes);

    const renddx9abi::HostString levelPath = simpleShadowLevelPath();
    renddx9abi::HostStream* stream;
    {
        renddx9abi::HostString path(levelPath);
        path += "/SimpleShadowmap.raw";
        stream = fileManager->open(path, static_cast<renddx9abi::HostFileMode>(2),
                                   static_cast<renddx9abi::HostFileAccess>(0), 0);
    }
    // Retail's missing-main-file return leaves the previous IO bit vector untouched. In
    // particular, the first absent main file does not manufacture IO storage for the sampler.
    if (!stream) return;

    const unsigned int imageBytes = size * size;
    unsigned char* source = static_cast<unsigned char*>(::renddx9abi::engineAlloc(imageBytes));
    stream->read(source, imageBytes);
    stream->release();
    for (int y = 0; y < size; ++y)
    {
        const unsigned char* row = source + (size - 1 - y) * size;
        for (int x = 0; x < size; ++x)
        {
            // PE uses x87 precision up to floor(double): do not round the product to float.
            // The multiplier's exact stored float is 0x3C40C0C1, i.e. float(3/255).
            const unsigned int value = static_cast<unsigned int>(std::floor(
                static_cast<double>(row[x]) * static_cast<double>(3.0f / 255.0f) + 0.5));
            const int packedX = x + 4;
            const unsigned int shift = 2u * (packedX & 3);
            unsigned char& destination = lmField<unsigned char*>(self, 0x190)[
                (y + 1) * lmField<int>(self, 0x1A8) + (packedX >> 2)];
            destination = static_cast<unsigned char>(
                (destination & ~(3u << shift)) | ((value & 3u) << shift));
        }
    }
    ::renddx9abi::engineFree(source);

    SimpleShadowMapIOStorage io(self);
    io.resize(lmField<int>(self, 0x198) * lmField<int>(self, 0x1A0), false);
    io.fill(true);
    {
        renddx9abi::HostString path(levelPath);
        path += "/SimpleShadowmapIO.raw";
        stream = fileManager->open(path, static_cast<renddx9abi::HostFileMode>(2),
                                   static_cast<renddx9abi::HostFileAccess>(0), 0);
    }
    if (!stream) return;

    source = static_cast<unsigned char*>(::renddx9abi::engineAlloc(imageBytes));
    stream->read(source, imageBytes);
    stream->release();
    for (int y = 0; y < size; ++y)
    {
        const unsigned char* row = source + (size - 1 - y) * size;
        for (int x = 0; x < size; ++x)
            io.set((y + 1) * lmField<int>(self, 0x198) + x + 4, row[x] > 127);
    }
    ::renddx9abi::engineFree(source);
}

// PE 0x100F1F60, reached by the zero-argument `Lightmanager.init` console object.  The debug mesh
// rebuild above is unconditional; only creation of the special sunlight is guarded by +0x840.
void initializeLightManagerBody(void* self)
{
    buildLightDebugGeometry(self);
    if (lmField<world::LightSource*>(self, 0x840))
        return;

    world::ObjectTemplateManager* const templateManager =
        static_cast<world::ObjectTemplateManager*>(classManager->getSingleton(
            renddx9abi::HostString("ObjectTemplateManager")));
    if (!templateManager)
        return;

    const renddx9abi::HostString sunlightName("sunlight");
    world::LightSourceTemplate* objectTemplate =
        static_cast<world::LightSourceTemplate*>(
            templateManager->getTemplate(hostStdString(sunlightName), 38087u));
    if (!objectTemplate)
    {
        // PE constructs the object name before the class name and therefore destroys the class
        // string first after createTemplate returns.
        const renddx9abi::HostString objectName("sunlight");
        const renddx9abi::HostString className("LightSource");
        objectTemplate = static_cast<world::LightSourceTemplate*>(
            templateManager->createTemplate(
                hostStdString(className), hostStdString(objectName)));
    }
    if (!objectTemplate)
        return;

    {
        const renddx9abi::HostString directional("Directional");
        objectTemplate->setLightType(hostStdString(directional));
    }
    objectTemplate->setCastsDynamicShadow(false);
    objectTemplate->setCastsStaticShadow(true);
    objectTemplate->setFov(8.0f);
    objectTemplate->setSupportsPerPixel(true);

    world::LightSource* const light =
        static_cast<world::LightSource*>(objectTemplate->createObject());
    lmField<world::LightSource*>(self, 0x840) = light;
    light->init();
    {
        const renddx9abi::HostString name("sun");
        light->setName(hostStdString(name));
    }
    light->setAsSkyLight();
    const Vec3 color = { 0.8f, 0.8f, 0.8f };
    light->setColor(color);
    const Vec3 direction = { 0.0f, -0.7f, -0.7f };
    light->setDirection(direction);
}


// Tests required and forbidden light properties, including the allowed light-type mask.
// Requires a light with a template. Typed calls account for the two-slot PE/ELF
// difference in the light-template interface.

bool lightMatchesMask(world::LightSource* light, unsigned int required, unsigned int forbidden)
{
    const world::LightSourceTemplate* const t = lightSourceTemplatePointer(light);

    if ((required  & 0x1u) && !light->getEnabled()) return false;
    if ((forbidden & 0x1u) &&  light->getEnabled()) return false;
    if ((required  & 0x2u) &&  light->getEnabled()) return false;
    if ((forbidden & 0x2u) && !light->getEnabled()) return false;

    if ((required  & 0x8u) && !t->getIsDynamic()) return false;
    if ((forbidden & 0x8u) &&  t->getIsDynamic()) return false;
    if ((required  & 0x4u) &&  t->getIsDynamic()) return false;
    if ((forbidden & 0x4u) && !t->getIsDynamic()) return false;

    if ((required  & 0x20u) && !t->getCastsDynamicShadow()) return false;
    if ((forbidden & 0x20u) &&  t->getCastsDynamicShadow()) return false;
    if ((required  & 0x10u) && !t->getCastsStaticShadow()) return false;
    if ((forbidden & 0x10u) &&  t->getCastsStaticShadow()) return false;

    if ((required  & 0x80u) && !t->getSelfLights()) return false;
    if ((forbidden & 0x80u) &&  t->getSelfLights()) return false;
    if ((required  & 0x40u) && !t->getSelfShadows()) return false;
    if ((forbidden & 0x40u) &&  t->getSelfShadows()) return false;

    const int type = t->getLightTypeEnum();
    if ((required & 0x3C00u) != 0
        && ((required & 0x400u)  == 0 || type != 0)
        && ((required & 0x800u)  == 0 || type != 1)
        && ((required & 0x1000u) == 0 || type != 2)
        && ((required & 0x2000u) == 0 || type != 3))
        return false;
    if ((forbidden & 0x3C00u) != 0
        && ((forbidden & 0x400u)  == 0 || type != 0)
        && ((forbidden & 0x800u)  == 0 || type != 1)
        && ((forbidden & 0x1000u) == 0 || type != 2)
        && ((forbidden & 0x2000u) == 0 || type != 3))
        return false;

    // The template header declares these two getters non-const (they are read-only bodies; the
    // ELF mangles them without the const qualifier), so the const pointer is cast for them alone.
    world::LightSourceTemplate* const mutableTemplate =
        const_cast<world::LightSourceTemplate*>(t);
    if ((required  & 0x4000u) && !mutableTemplate->getSupportsGlow()) return false;
    if ((forbidden & 0x4000u) &&  mutableTemplate->getSupportsGlow()) return false;
    if ((required  & 0x8000u) && !mutableTemplate->getSupportsEmitter()) return false;
    if ((forbidden & 0x8000u) &&  mutableTemplate->getSupportsEmitter()) return false;
    if ((required  & 0x10000u) && !t->getAffectLightmappedObjects()) return false;
    if ((forbidden & 0x10000u) &&  t->getAffectLightmappedObjects()) return false;
    return true;
}


// +0x14, sub_100F2E60 - every owned light that passes the masks, appended to `out` in set order.
// It walks the owned set at +0x1B8 (the head read is `[ecx+1BCh]`), not the active one:
// enabled-ness is a mask bit, not a set membership.

void __fastcall lm_getLightsByMask(void* self, void*, vc71::vector_layout* out,
                                   unsigned int required, unsigned int forbidden)
{
    vc71::set_layout& owned = lmOwnedLights(self);
    vc71::vectorRef<world::LightSource*> lights(out);
    for (vc71::set_node* node = owned.begin(); node != owned.end();
         node = vc71::set_layout::next(node))
    {
        world::LightSource* const light = reinterpret_cast<world::LightSource*>(node->key);
        if (lightMatchesMask(light, required, forbidden))
            lights.push_back(light);
    }
}


// Appends this frame's visible lights matching required/forbidden masks.
// Required 0x7FFFFFFF instead selects enabled non-directional emitter lights and ignores forbidden.

void __fastcall lm_getVisibleLightsByMask(void* self, void*, vc71::vector_layout* out,
                                          unsigned int required, unsigned int forbidden)
{
    vc71::vector_layout& scratch = lmField<vc71::vector_layout>(self, 0x1D0);
    vc71::vectorRef<world::LightSource*> lights(out);
    if (required == 0x7FFFFFFFu)
    {
        for (world::LightSource** it = reinterpret_cast<world::LightSource**>(scratch.first);
             it != reinterpret_cast<world::LightSource**>(scratch.last); ++it)
        {
            world::LightSource* const light = *it;
            const world::LightSourceTemplate* const t = lightSourceTemplatePointer(light);
            // getSupportsEmitter is declared non-const on the template (lightMatchesMask's note).
            if (t->getLightTypeEnum() != 0 && light->getEnabled()
                && const_cast<world::LightSourceTemplate*>(t)->getSupportsEmitter())
                lights.push_back(light);
        }
        return;
    }
    for (world::LightSource** it = reinterpret_cast<world::LightSource**>(scratch.first);
         it != reinterpret_cast<world::LightSource**>(scratch.last); ++it)
    {
        world::LightSource* const light = *it;
        if (lightMatchesMask(light, required, forbidden))
            lights.push_back(light);
    }
}


// First gather snapshots enabled static point lights into engine-heap clones and registers
// originals with the coordinator. A repeated initialization restores settings from those clones.

LightManager*        g_snapshotManager = 0;                       // dword_10264114
vc71::vector_layout  g_snapshotOriginals = { 0, 0, 0, 0 };        // unk_10264118
vc71::vector_layout  g_snapshotClones    = { 0, 0, 0, 0 };        // unk_10264128
bool                 g_snapshotRegistering = false;               // byte_10259BE4
bool                 g_gatherFirstCall = true;                    // byte_102395B4, .data 1

// sub_101949A0 - clone a light: its template's createObject, then copySettings from the original.
world::LightSource* cloneLightSettings(world::LightSource* light)
{
    world::LightSource* const clone = static_cast<world::LightSource*>(
        lightSourceTemplatePointer(light)->createObject());        // ObjectTemplate +0x50
    clone->copySettings(*light);                                     // LightSource +0x1E8
    return clone;
}


void collectSnapshotLights(vc71::vector_layout& lights)
{
    if (g_snapshotOriginals.first
        && vc71::vectorSizeIn(g_snapshotOriginals, sizeof(void*)) != 0)
    {
        // The restore branch.
        const unsigned int count = vc71::vectorSizeIn(g_snapshotOriginals, sizeof(void*));
        vc71::vectorReserveIn(lights, sizeof(void*), count);
        world::LightSource** const originals =
            reinterpret_cast<world::LightSource**>(g_snapshotOriginals.first);
        for (unsigned int i = 0; i < count; ++i)
            vc71::vectorPushBackIn(lights, sizeof(void*), &originals[i]);

        world::LightSource** const clones =
            reinterpret_cast<world::LightSource**>(g_snapshotClones.first);
        const unsigned int cloneCount = vc71::vectorSizeIn(g_snapshotClones, sizeof(void*));
        world::LightSource** const first = reinterpret_cast<world::LightSource**>(lights.first);
        world::LightSource** const last  = reinterpret_cast<world::LightSource**>(lights.last);
        unsigned int c = 0;
        for (world::LightSource** it = first; it != last && c != cloneCount; ++it, ++c)
            (*it)->copySettings(*clones[c]);
        return;
    }

    const unsigned int before = lights.first ? vc71::vectorSizeIn(lights, sizeof(void*)) : 0u;
    g_snapshotManager->getLightsByMask(lights, 0x805u, 0u);                    // +0x14

    world::LightSource** const first =
        reinterpret_cast<world::LightSource**>(lights.first) + before;
    world::LightSource** const last = reinterpret_cast<world::LightSource**>(lights.last);
    const unsigned int added = static_cast<unsigned int>(last - first);

    vc71::vectorReserveIn(g_snapshotOriginals, sizeof(void*), added);
    vc71::vectorReserveIn(g_snapshotClones, sizeof(void*), added);
    for (world::LightSource** it = first; it != last; ++it)
        vc71::vectorPushBackIn(g_snapshotOriginals, sizeof(void*), it);
    for (world::LightSource** it = first; it != last; ++it)
    {
        world::LightSource* const clone = cloneLightSettings(*it);
        vc71::vectorPushBackIn(g_snapshotClones, sizeof(void*), &clone);
    }
}


void snapshotAndRegisterStaticPointLights()
{
    vc71::vector_layout lights = { 0, 0, 0, 0 };
    collectSnapshotLights(lights);

    g_snapshotRegistering = true;
    for (world::LightSource** it = reinterpret_cast<world::LightSource**>(lights.first);
         it != reinterpret_cast<world::LightSource**>(lights.last); ++it)
        lmRegisterXpLight(*it);
    g_snapshotRegistering = false;

    if (lights.first)
        ::renddx9abi::engineFree(lights.first);
}

// The ray-test predicator the gather builds on its stack: sub_101C7E90 with mask 0x102, i.e.
// OFIsVisible | OFHasCollision, and 0x101C7EB0's rule that every bit of the mask must be present.
// Same two-flavour shape as ShadowCasterPredicator above, for the same reason.
#if BF2_PLUGIN_ABI_NATIVE
class LightOcclusionPredicator : public world::ObjectPredicator
{
public:
    LightOcclusionPredicator() : m_mask(0x102u) {}
    virtual bool predicate(world::IObject* object)
    {
        return (m_mask & object->getFlags()) == m_mask;
    }
    virtual unsigned int getFlags() const { return m_mask; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }

private:
    unsigned int m_mask;
};

inline world::ObjectPredicator* lightOcclusionPredicatorPointer(LightOcclusionPredicator& p)
{
    return &p;
}
#else
struct LightOcclusionPredicator
{
    const void** table;
    unsigned int mask;
};

bool __fastcall lightOcclusionPredicate(void* self, void*, world::IObject* object)
{
    const unsigned int mask = static_cast<LightOcclusionPredicator*>(self)->mask;
    return (mask & object->getFlags()) == mask;
}
unsigned int __fastcall lightOcclusionPredicateFlags(void* self, void*)
{ return static_cast<LightOcclusionPredicator*>(self)->mask; }
bool __fastcall lightOcclusionPredicateCull(void*, void*, world::IObject*) { return true; }
bool __fastcall lightOcclusionPredicateSurface(void*, void*, int) { return true; }
const void* const kLightOcclusionPredicatorVtable[4] =
{
    reinterpret_cast<void*>(&lightOcclusionPredicate),
    reinterpret_cast<void*>(&lightOcclusionPredicateFlags),
    reinterpret_cast<void*>(&lightOcclusionPredicateCull),
    reinterpret_cast<void*>(&lightOcclusionPredicateSurface)
};

inline world::ObjectPredicator* lightOcclusionPredicatorPointer(LightOcclusionPredicator& p)
{
    p.table = const_cast<const void**>(kLightOcclusionPredicatorVtable);
    p.mask  = 0x102u;
    return reinterpret_cast<world::ObjectPredicator*>(&p);
}
#endif


// Appends nearest eligible lights until out reaches maxLights.
// Candidates must strictly overlap the query sphere and pass both masks. Optional
// occlusion casts from the light toward position; hits inside radius remain accepted.

void __fastcall lm_gatherLights(void* self, void*, const Vec3* position, float radius,
                                vc71::vector_layout* out, unsigned int maxLights,
                                unsigned int required, unsigned int forbidden,
                                unsigned char occlusionTest)
{
    if (g_gatherFirstCall)
    {
        g_snapshotManager = static_cast<LightManager*>(self);
        snapshotAndRegisterStaticPointLights();
        g_gatherFirstCall = false;
    }

    (void)dx9rend::rendererGetRenderView()->getViewMatrixPtr();

    std::vector<std::pair<float, world::LightSource*> > candidates;
    vc71::vector_layout& scratch = lmField<vc71::vector_layout>(self, 0x1D0);
    world::LightSource** const first = reinterpret_cast<world::LightSource**>(scratch.first);
    world::LightSource** const last  = reinterpret_cast<world::LightSource**>(scratch.last);
    for (world::LightSource** it = first; it != last; ++it)
    {
        world::LightSource* const light = *it;
        const float reach = light->getVisualBoundingRadius() + radius;
        const Vec3& p = light->getAbsolutePosition();
        const float dx = position->x - p.x;
        const float dy = position->y - p.y;
        const float dz = position->z - p.z;
        const float distanceSq = dz * dz + dy * dy + dx * dx;
        if (distanceSq < reach * reach && lightMatchesMask(light, required, forbidden))
            candidates.push_back(std::make_pair(distanceSq, light));
    }

    std::sort(candidates.begin(), candidates.end());

    const float radiusSq = radius * radius;
    world::ObjectManager* const manager = reinterpret_cast<world::ObjectManager*>(objectManager);
    for (std::vector<std::pair<float, world::LightSource*> >::iterator c = candidates.begin();
         c != candidates.end(); ++c)
    {
        if (vc71::vectorSizeIn(*out, sizeof(void*)) >= maxLights)
            break;

        if (occlusionTest)
        {
            const Vec3& p = c->second->getAbsolutePosition();
            const Vec3 delta = { position->x - p.x, position->y - p.y, position->z - p.z };
            LightOcclusionPredicator predicate;                                 // sub_101C7E90(0x102)
            world::IObject* hitObject = 0;
            Vec3 hitPoint  = { 0.0f, 0.0f, 0.0f };
            Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
            int  hitPart;
            if (manager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, p, delta,
                                       lightOcclusionPredicatorPointer(predicate),
                                       true, true, true, false, 0u))            // ObjectManager +0x68
            {
                const float hx = hitPoint.x - position->x;
                const float hy = hitPoint.y - position->y;
                const float hz = hitPoint.z - position->z;
                const float hitSq = hz * hz + hy * hy + hx * hx;
                if (hitSq > radiusSq)
                    continue;
            }
        }

        vc71::vectorPushBackIn(*out, sizeof(void*), &c->second);
    }
}


// +0x20, sub_100F1750 - gatherLights at a geometry instance.  ret 0x18.  The instance's transform
// (IGeometry +0x48, the +0x50 setter's stored pointer) supplies the position from its translation
// row; its bounding radius (IGeometry +0x44) the radius.  Both are reached by raw slot because the
// geometry instance interface is not modelled as a type in this tree (see ObjectDrawer.cpp); the
// call order - transform first, radius second - is the shipped one.

void __fastcall lm_gatherLightsForGeometry(void* self, void*, void* geometry,
                                           vc71::vector_layout* out, unsigned int maxLights,
                                           unsigned int required, unsigned int forbidden,
                                           int occlusionTest)
{
    typedef const Mat4* (__fastcall* TransformationFn)(void*, void*);
    typedef float (__fastcall* BoundingRadiusFn)(void*, void*);

    void** const vtbl = *reinterpret_cast<void***>(geometry);
    const Mat4* const transform =
        reinterpret_cast<TransformationFn>(vtbl[0x48 / sizeof(void*)])(geometry, 0);
    const float radius =
        reinterpret_cast<BoundingRadiusFn>(vtbl[0x44 / sizeof(void*)])(geometry, 0);
    const Vec3* const position = reinterpret_cast<const Vec3*>(&transform->m[12]);

    static_cast<LightManager*>(self)->gatherLights(*position, radius, *out, maxLights,
                                                   required, forbidden, occlusionTest != 0);
}

// +0x280, PE 0x1015BF40 and +0x2D8, the two one-instruction getters the StaticMesh
// shadow draw dispatches.  ; the fields are the ones the constructor seeds.
int __fastcall lm_getStaticMeshShadowLod(void* self, void*)
{
    return static_cast<int>(lmField<unsigned int>(self, 0x154));
}

int __fastcall lm_getShadowMergeLodLimit(void* self, void*)
{
    return static_cast<int>(lmField<unsigned int>(self, 0x820));
}

// +0x10, PE 0x10160D30 -> 0x100F2D00. Retail clears the scratch vector, refreshes/copies the active
// RenderView's Frustum, applies the configured cull distance, and walks the active set in pointer
// order. Each accepted light is appended to the VC7.1 scratch vector at +0x1D0.
void __fastcall lm_update(void* self, void*, double time)
{
    (void)time;

    vc71::vector_layout& scratch = lmField<vc71::vector_layout>(self, 0x1D0);
    // std::vector<LightSource*>::clear retains capacity; only the destructor frees the buffer.
    scratch.last = scratch.first;

    LightManager* const lights = static_cast<LightManager*>(self);
    if (lights->getLightCullDistance() == 0.0f)
        return;

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();
    Frustum localFrustum(*view->getFrustumPtr());
    localFrustum.setFarDist(lights->getLightCullDistance());

    vc71::set_layout& active = lmActiveLights(self);
    vc71::vectorRef<world::LightSource*> visible(&scratch);
    for (vc71::set_node* node = active.begin(); node != active.end();
         node = vc71::set_layout::next(node))
    {
        world::LightSource* const light =
            reinterpret_cast<world::LightSource*>(node->key);
        const float radius = light->getVisualBoundingRadius();
        const Vec3& position = light->getAbsolutePosition();
        if (localFrustum.inside(position, radius))
            visible.push_back(light);
    }

    static bool noted = false;
    if (!noted)
    {
        noted = true;
        ::renddx9abi::logf("LightManager::update: active-light cull path recovered; scratch reset, "
                           "RenderView frustum refresh and ordered sphere tests completed");
    }
}

// +0x24/+0x28, PE 0x1015BB90/0x1015BBC0.  Assignment owns one reference.  The same-pointer case
// deliberately adds another reference without releasing the old one; that retail leak is visible.
void __fastcall lm_setSkyLight(void* self, void*, world::LightSource* light)
{
    world::LightSource*& current = lmField<world::LightSource*>(self, 0x14);
    if (current && current != light)
        current->release();
    current = light;
    light->addRef();
}

world::LightSource* __fastcall lm_getSkyLight(void* self, void*)
{
    return lmField<world::LightSource*>(self, 0x14);
}

// +0x8C, PE 0x100F2C70.  addRef is unconditional after std::set::insert, including when a
// duplicate key is rejected (the shipped duplicate-registration leak).
void __fastcall lm_addLight(void* self, void*, world::LightSource* light)
{
    lmOwnedLights(self).insert(lmLightKey(light));
    light->addRef();
}

// +0x90, PE 0x100F2B40.  The special pointer is non-owning. Sky ownership and owning-set
// ownership are independent, so a sky light found in the set is released twice in this call.
void __fastcall lm_removeLight(void* self, void*, world::LightSource* light)
{
    world::LightSource*& special = lmField<world::LightSource*>(self, 0x840);
    if (special == light)
        special = 0;

    LightManager* const manager = static_cast<LightManager*>(self);
    if (manager->getSkyLight() == light)
    {
        light->release();
        lmField<world::LightSource*>(self, 0x14) = 0;
    }

    vc71::set_layout& owned = lmOwnedLights(self);
    vc71::set_node* const node = owned.find(lmLightKey(light));
    if (node != owned.end())
    {
        reinterpret_cast<world::LightSource*>(node->key)->release();
        owned.erase(node);
    }
}

// +0x94, PE 0x100F2CA0.  Point/Spot lights visit the XpLight coordinator before every light is
// inserted into the non-owning active set.  The pointer at +0x840 is explicitly excluded.
void __fastcall lm_enableLight(void* self, void*, world::LightSource* light)
{
    if (lmField<world::LightSource*>(self, 0x840) == light)
        return;

    const world::LightSourceTemplate* const objectTemplate =
        lightSourceTemplatePointer(light);
    const int lightType = objectTemplate->getLightTypeEnum();
    if (lightType == 1 || lightType == 2)
        lmRegisterXpLight(light);

    lmActiveLights(self).insert(lmLightKey(light));
}

// +0x98, PE 0x100F2BB0.  Coordinator removal precedes erasing the active-set node and happens only
// when that node was present.
void __fastcall lm_disableLight(void* self, void*, world::LightSource* light)
{
    if (lmField<world::LightSource*>(self, 0x840) == light)
        return;

    vc71::set_layout& active = lmActiveLights(self);
    vc71::set_node* const node = active.find(lmLightKey(light));
    if (node != active.end())
    {
        lmRemoveXpLight(light);
        active.erase(node);
    }
}

// +0x54..+0x70 are exact setter/getter pairs over +0x13C..+0x148.
void __fastcall lm_setLightCullDistance(void* self, void*, float value)
{
    lmField<float>(self, 0x13C) = value;
}
float __fastcall lm_getLightCullDistance(void* self, void*)
{
    const float value = lmField<float>(self, 0x13C);
    return value > 0.0f ? value : 0.0f;
}
void __fastcall lm_setMaxLightsPerBundledMesh(void* self, void*, int value)
{
    lmField<int>(self, 0x140) = value;
}
int __fastcall lm_getMaxLightsPerBundledMesh(void* self, void*)
{
    const int value = lmField<int>(self, 0x140);
    return value > 0 ? value : 0;
}
void __fastcall lm_setMaxLightsPerStaticMesh(void* self, void*, int value)
{
    lmField<int>(self, 0x144) = value;
}
int __fastcall lm_getMaxLightsPerStaticMesh(void* self, void*)
{
    const int value = lmField<int>(self, 0x144);
    return value > 0 ? value : 0;
}
void __fastcall lm_setMaxLightsPerSkinnedMesh(void* self, void*, int value)
{
    lmField<int>(self, 0x148) = value;
}
int __fastcall lm_getMaxLightsPerSkinnedMesh(void* self, void*)
{
    const int value = lmField<int>(self, 0x148);
    return value > 0 ? value : 0;
}
void __fastcall lm_setMaxLightsPerTerrainPatch(void* self, void*, int value)
{
    lmField<int>(self, 0x14C) = value;
}
int __fastcall lm_getMaxLightsPerTerrainPatch(void* self, void*)
{
    const int value = lmField<int>(self, 0x14C);
    return value > 0 ? value : 0;
}

// Primary +0x7C/+0x80, PE 0x1015BF00/0x1015BF10. Unlike the neighbouring quality
// accessors, these preserve the stored float without clamping. The getter returns it in ST(0).
void __fastcall lm_setSkinnedMeshShaderLodDistance(void* self, void*, float value)
{
    lmField<float>(self, 0x1B4) = value;
}
float __fastcall lm_getSkinnedMeshShaderLodDistance(void* self, void*)
{
    return lmField<float>(self, 0x1B4);
}
// Primary +0x84/+0x88: the getter returns the field's address; the setter reads and stores the
// second component before the first.
const Vec2& __fastcall lm_getLightMapAmbientAlphaCutoffRange(void* self, void*)
{
    return lmField<Vec2>(self, 0x1FC);
}
void __fastcall lm_setLightMapAmbientAlphaCutoffRange(void* self, void*, const Vec2& range)
{
    const float second = range.y;
    const float first = range.x;
    lmField<float>(self, 0x200) = second;
    lmField<float>(self, 0x1FC) = first;
}

// Simple-shadow settings and CPU map sampling. The packed map and bit-vector are populated by
// loadSimpleShadowMaps; retail has no null/bounds fallback in these texel readers.
// PE 0x1015BE10/20, table 0x1021AAF0 +0x284/+0x288; ordinary unclamped integer field.
void __fastcall lm_setBundledMeshShadowLod(void* self, void*, int lod)
{
    lmField<int>(self, 0x158) = lod;
}
int __fastcall lm_getBundledMeshShadowLod(void* self, void*)
{
    return lmField<int>(self, 0x158);
}
// PE 0x1015BE70/80, +0x29C/+0x2A0. The upward ray length, default150 in the constructor.
void __fastcall lm_setShadowRoofRayLength(void* self, void*, float length)
{
    lmField<float>(self, 0x164) = length;
}
float __fastcall lm_getShadowRoofRayLength(void* self, void*)
{
    return lmField<float>(self, 0x164);
}
// PE 0x1015BE30/40, +0x28C/+0x290: the skinned-mesh shadow LOD is an unclamped integer.
void __fastcall lm_setSkinnedMeshShadowLod(void* self, void*, int lod)
{
    lmField<int>(self, 0x15C) = lod;
}
int __fastcall lm_getSkinnedMeshShadowLod(void* self, void*)
{
    return lmField<int>(self, 0x15C);
}
// PE 0x1015BE90/A0, +0x2A4/+0x2A8: unclamped exponent.
void __fastcall lm_setShadowSunScalePower(void* self, void*, float power)
{
    lmField<float>(self, 0x168) = power;
}
float __fastcall lm_getShadowSunScalePower(void* self, void*)
{
    return lmField<float>(self, 0x168);
}
// PE 0x1015CB40 writes the second component first; 0x1015BEE0 returns the field's address.
void __fastcall lm_setShadowSunHeightOverFade(void* self, void*, const Vec2& range)
{
    const float first = range.x;
    lmField<float>(self, 0x178) = range.y;
    lmField<float>(self, 0x174) = first;
}
const Vec2& __fastcall lm_getShadowSunHeightOverFade(void* self, void*)
{
    return lmField<Vec2>(self, 0x174);
}

// PE 0x100F1F10, +0x2D0. Four two-bit cells per byte, low bits first. The arithmetic
// right shift of negative x is significant: the map origin is added before that byte offset.
float __fastcall lm_sampleSimpleShadowMapTexel(void* self, void*, int x, int z)
{
    const unsigned int row = (static_cast<unsigned int>(z) + lmField<unsigned int>(self, 0x1B0))
                          * lmField<unsigned int>(self, 0x1A8)
                          + lmField<unsigned int>(self, 0x1AC);
    const int byteIndex = static_cast<int>(row + static_cast<unsigned int>(x >> 2));
    const unsigned char packed = lmField<unsigned char*>(self, 0x190)[byteIndex];
    return static_cast<float>((packed >> (2 * (x & 3))) & 3) * 0.3333333432674408f;
}

// PE 0x100F2C10, +0x2CC. The IO map is a bit vector: compute the wrapped 32-bit bit index,
// then use a logical shift for its word address. The only results are the shipped 0 and 1.
float __fastcall lm_sampleSimpleShadowMapIOTexel(void* self, void*, int x, int z)
{
    const unsigned int bit =
        (static_cast<unsigned int>(z) + lmField<unsigned int>(self, 0x1A4))
        * lmField<unsigned int>(self, 0x198) + lmField<unsigned int>(self, 0x19C)
        + static_cast<unsigned int>(x);
    const unsigned int word = lmField<unsigned int*>(self, 0x184)[bit >> 5];
    return (word & (1u << (bit & 31u))) != 0 ? 1.0f : 0.0f;
}

// PE 0x100F1C70 / 0x100F19D0 differ only in the virtual texel slot (+0x2D0 / +0x2CC).
// Keep their cell-centre offset, floor (not truncation), four-query order and float spills.
template <bool IoMap>
float sampleSimpleShadowMapFiltered(void* self, float x, float z)
{
    const float limit = lmField<float>(self, 0x194);
    if (x < -limit)
        x = -limit;
    else if (x > limit)
        x = limit;
    if (z < -limit)
        z = -limit;
    else if (z > limit)
        z = limit;
    x = x * 0.5f - 0.5f;
    z = z * 0.5f - 0.5f;

    LightManager* const manager = static_cast<LightManager*>(self);
    const auto query = [manager](int cellX, int cellZ) -> float {
        return IoMap ? manager->sampleSimpleShadowMapIOTexel(cellX, cellZ)
                     : manager->sampleSimpleShadowMapTexel(cellX, cellZ);
    };
    const float lowerX = static_cast<float>(std::floor(static_cast<double>(x)));
    const float a = query(static_cast<int>(lowerX),
                         static_cast<int>(std::floor(static_cast<double>(z))));
    const float nextX = x + 1.0f;
    const float upperX = static_cast<float>(std::floor(static_cast<double>(x) + 1.0));
    const float b = query(static_cast<int>(upperX),
                         static_cast<int>(std::floor(static_cast<double>(z))));
    const float lowerXAgain = static_cast<float>(std::floor(static_cast<double>(x)));
    const float nextZ = z + 1.0f;
    const float c = query(static_cast<int>(lowerXAgain),
                         static_cast<int>(std::floor(static_cast<double>(nextZ))));
    const float upperXAgain = static_cast<float>(std::floor(static_cast<double>(nextX)));
    const float d = query(static_cast<int>(upperXAgain),
                         static_cast<int>(std::floor(static_cast<double>(nextZ))));
    x = static_cast<float>(static_cast<double>(x) - std::floor(static_cast<double>(x)));
    const double fractionZ = static_cast<double>(z) - std::floor(static_cast<double>(z));
    const double inverseX = 1.0 - x;
    return static_cast<float>((a * inverseX + static_cast<double>(x) * b) * (1.0 - fractionZ)
                             + (inverseX * c + static_cast<double>(x) * d) * fractionZ);
}

// +0xEC, PE 0x1015CEC0. Enabling publishes the byte before resource creation, then replaces it with
// the creation result. Disabling clears the three render-record vectors before invoking the device-
// lost listener on the secondary this pointer.
void __fastcall lm_setDynamicShadowsEnabled(void* self, void*, bool enabled)
{
    unsigned char& current = lmField<unsigned char>(self, 0x7EC);
    if (enabled)
    {
        if (!current)
        {
            current = 1;
            current = createShadowResourcesBody(self) ? 1 : 0;
        }
        return;
    }

    if (current)
    {
        clearShadowRecordVector(lmField<vc71::vector_layout>(self, 0x79C));
        clearShadowRecordVector(lmField<vc71::vector_layout>(self, 0x7CC));
        clearShadowRecordVector(lmField<vc71::vector_layout>(self, 0x7DC));
        current = 0;
        releaseShadowResources(self, true);
    }
}
bool __fastcall lm_getDynamicShadowsEnabled(void* self, void*)
{
    return lmField<unsigned char>(self, 0x7EC) != 0;
}

void __fastcall lm_setShadowIntensityBias(void* self, void*, float value)
{
    lmField<float>(self, 0x26C) = value;
    lmField<dx9rend::ShaderParameter*>(self, 0x894)->setFloat(value);
}
float __fastcall lm_getShadowIntensityBias(void* self, void*)
{
    return lmField<float>(self, 0x26C);
}
void __fastcall lm_setLightMapIntensityBias(void* self, void*, float value)
{
    lmField<float>(self, 0x270) = value;
    lmField<dx9rend::ShaderParameter*>(self, 0x898)->setFloat(value);
}
float __fastcall lm_getLightMapIntensityBias(void* self, void*)
{
    return lmField<float>(self, 0x270);
}

void __fastcall lm_setShadowUpdatesEnabled(void* self, void*, bool enabled)
{
    lmField<unsigned char>(self, 0x7F9) = enabled ? 1 : 0;
}
bool __fastcall lm_getShadowUpdatesEnabled(void* self, void*)
{
    return lmField<unsigned char>(self, 0x7F9) != 0;
}
void __fastcall lm_setShadowUseSweepTest(void* self, void*, bool enabled)
{
    lmField<unsigned char>(self, 0x170) = enabled ? 1 : 0;
}
bool __fastcall lm_getShadowUseSweepTest(void* self, void*)
{
    return lmField<unsigned char>(self, 0x170) != 0;
}

// Console property shadowTSMFocus, primary +0x180/+0x184, PE
// The directional TSM fitter reads this through the
// getter rather than loading +0x814 itself.
void __fastcall lm_setShadowTSMFocus(void* self, void*, float value)
{
    lmField<float>(self, 0x814) = value;
}
float __fastcall lm_getShadowTSMFocus(void* self, void*)
{
    return lmField<float>(self, 0x814);
}

// +0xC4/+0xC8, PE 0x1015C150/0x1015C160.  The public spelling has not yet
// been attested, but the bodies and their use by the stock point-shadow
// predicate are exact: they write/read the feature byte at +0x268.
void __fastcall lm_setShadowStaticFeature(void* self, void*, bool enabled)
{
    lmField<unsigned char>(self, 0x268) = enabled ? 1 : 0;
}
bool __fastcall lm_getShadowStaticFeature(void* self, void*)
{
    return lmField<unsigned char>(self, 0x268) != 0;
}

// +0x19C, PE 0x101679A0.  Profiler counters and the loader-zero diagnostic flush are nonfunctional
// instrumentation; the renderer state, target, directional/point/spot and finalizer ordering is
// otherwise the shipped transaction.
void __fastcall lm_renderShadowMaps(void* self, void*)
{
    if (!lmField<void*>(self, 0x14)
        || !lmField<unsigned char>(self, 0x7EC)
        || !lmField<unsigned char>(self, 0x7F9))
        return;

    lmField<void*>(self, 0x90C) = dx9rend::rendererGetRenderView();
    dx9rend::stateManagerPush();
    dx9rend::stateManagerSetRenderState(161, 0);
    // ONE call, the candidate half now runs inside it (G3).
    if (!dx9rend::rendererPerformanceFlag(20))
        renderDirectionalShadowMaps(self);
    g_shadowRecordOrdinal = 0;

    bindAndClearShadowTarget(self);
    renderPointShadowMaps(self);
    renderSpotShadowMaps(self);

    setLightManagerDepthStencil(0);
    restoreRendererTarget0();
    dx9rend::stateManagerPop();
    finalizeShadowRecords(self);
}

// The stock Sky.con color surface.  Each setter is the same three-dword copy and each getter
// returns a reference to the in-object Vec3.  The distinct vtable slots and storage offsets are
// instruction-proven in the tiny PE bodies at 0x1015C5F0..0x1015C770 and
// Keeping the implementation templated does not merge ABI entries: MSVC
// still emits one __fastcall thunk per instantiated field offset.
template <unsigned int Offset>
void __fastcall lm_setVec3(void* self, void*, const Vec3* value)
{
    lmField<Vec3>(self, Offset) = *value;
}

template <unsigned int Offset>
const Vec3* __fastcall lm_getVec3(void* self, void*)
{
    return &lmField<Vec3>(self, Offset);
}

inline bool vec3EpsilonEqual(const Vec3& a, const Vec3& b)
{
    // PE sub_10037DC0, called by setSunSpecColor before its coarser regeneration threshold.
    return std::fabs(a.x - b.x) <= FLT_EPSILON
        && std::fabs(a.y - b.y) <= FLT_EPSILON
        && std::fabs(a.z - b.z) <= FLT_EPSILON;
}

// PE 0x1015CB80/0x1015CC40, primary +0x1A0/+0x1A4. The manager interface takes and returns Vec3
// by value; the explicit output pointer below is the physical MSVC x86 sret parameter.
void __fastcall lm_setSunDirection(void* self, void*, Vec3 value)
{
    world::LightSource* const sun = lmField<world::LightSource*>(self, 0x840);
    sun->setDirection(value);
    dx9rend::textureReleaseRef(lmField<dx9rend::EngineTexture*>(self, 0x24C));
    dx9rend::EngineTexture* const normalCube = createNormalCubeTexture();
    lmField<dx9rend::EngineTexture*>(self, 0x24C) = normalCube;
    dx9rend::raShaderConstantsSetEngineTexture(0x2B8, normalCube);
}

Vec3* __fastcall lm_getSunDirection(void* self, void*, Vec3* out)
{
    *out = lmField<world::LightSource*>(self, 0x840)->getDirection();
    return out;
}

// +0x1A8 / +0x1AC
void __fastcall lm_setSunColor(void* self, void*, Vec3 value)
{
    lmField<world::LightSource*>(self, 0x840)->setColor(value);
}

Vec3* __fastcall lm_getSunColor(void* self, void*, Vec3* out)
{
    *out = lmField<world::LightSource*>(self, 0x840)->getColor();
    return out;
}

// PE 0x1015CCA0/0x1015CDD0, primary +0x1B0/+0x1B4. Exact epsilon equality skips every effect.
// Otherwise a squared delta >= 0.0025 (and unordered/NaN) regenerates one member of the four-LUT
// ring before the live LightSource receives the new specular colour.
void __fastcall lm_setSunSpecColor(void* self, void*, Vec3 value)
{
    world::LightSource* const sun = lmField<world::LightSource*>(self, 0x840);
    bool regenerate = lmField<dx9rend::EngineTexture*>(self, 0x250) == 0;
    if (!regenerate)
    {
        if (vec3EpsilonEqual(sun->getSpecColor(), value))
            return;

        // Retail dispatches getSpecColor a second time for this coarser test.
        const Vec3& current = sun->getSpecColor();
        const float dx = current.x - value.x;
        const float dy = current.y - value.y;
        const float dz = current.z - value.z;
        regenerate = !(dx * dx + dy * dy + dz * dz < 0.0025f);
    }

    if (regenerate)
    {
        const unsigned int index = lmField<unsigned int>(self, 0x264);
        dx9rend::EngineTexture* const lut =
            lmField<dx9rend::EngineTexture*>(self, 0x254 + index * 4);
        lmField<dx9rend::EngineTexture*>(self, 0x250) = lut;
        fillPowerLut(lut, 256, value, 64.0f, false);
        lmField<unsigned int>(self, 0x264) = (index + 1) & 3;
        dx9rend::raShaderConstantsSetEngineTexture(0x2C0, lut);
    }

    sun->setSpecColor(value);
}

Vec3* __fastcall lm_getSunSpecColor(void* self, void*, Vec3* out)
{
    *out = lmField<world::LightSource*>(self, 0x840)->getSpecColor();
    return out;
}

// PE 0x1015BF80/0x1015BFB0, primary +0x1B8/+0x1BC. This is Object flag bit zero, not
// LightSource::setEnabled: true clears the disabled bit and false sets it.
void __fastcall lm_setSunEnabled(void* self, void*, bool enabled)
{
    world::LightSource* const sun = lmField<world::LightSource*>(self, 0x840);
    if (enabled)
        sun->updateFlags(0, 1);
    else
        sun->updateFlags(1, 0);
}

bool __fastcall lm_getSunEnabled(void* self, void*)
{
    return (lightSourceFlags(lmField<world::LightSource*>(self, 0x840)) & 1u) == 0;
}

// Console property defaultEffectLightAffectionFactor: +0x26C setter, +0x270 getter.
void __fastcall lm_setDefaultEffectFactor(void* self, void*, float value)
{
    lmField<float>(self, 0x138) = value;
}
float __fastcall lm_getDefaultEffectFactor(void* self, void*)
{
    return lmField<float>(self, 0x138);
}

// Console property dynamicLightRangeScale: +0x2DC setter, +0x2E0 getter. PE 0x1015C5A0 and
// 0x1015C5B0 are direct stores/loads at +0x824; the constructor writes 1.0f.
void __fastcall lm_setDynamicLightRangeScale(void* self, void*, float value)
{
    lmField<float>(self, 0x824) = value;
}
float __fastcall lm_getDynamicLightRangeScale(void* self, void*)
{
    return lmField<float>(self, 0x824);
}

// Console property occludedSkyNightIntensity: +0x2E4 setter, +0x2E8 getter. PE 0x1015C5C0 is
// exactly an x87 load from +0x828; zero-fill supplies the shipped constructor default 0.0f.
void __fastcall lm_setOccludedSkyNightIntensity(void* self, void*, float value)
{
    lmField<float>(self, 0x828) = value;
}
float __fastcall lm_getOccludedSkyNightIntensity(void* self, void*)
{
    return lmField<float>(self, 0x828);
}

// PE 0x1015CE30, primary vtable +0x2F0. Mesh renderers publish the shadow Effect associated
// with their GeometryTemplate reflection type. Replacement releases the previous map value,
// stores the new pointer, then unconditionally retains it; retail has no null guard here.
void __fastcall lm_registerShadowEffect(void* self, void*, int reflectionTypeId,
                                        dx9rend::Effect* effect)
{
    ShadowTreeNode* const node = shadowTreeFindOrInsert(
        lmField<ShadowTree>(self, 0x964), static_cast<unsigned int>(reflectionTypeId));
    if (node->value)
        dx9rend::effectRelease(static_cast<dx9rend::Effect*>(node->value));
    node->value = effect;
    dx9rend::effectAddRef(effect);
}

} // anonymous namespace

// The two renderer-target helpers above, published for the terrain draws (Geom/TerrainDraw.cpp):
// sub_10005920 with a texture argument, and sub_10028070.
void lightManagerSetRenderTargetTexture(unsigned int index, dx9rend::EngineTexture* texture)
{
    setRenderTargetTexture(index, texture);
}

void lightManagerRestoreRendererTarget0()
{
    restoreRendererTarget0();
}

void lightManagerSetDepthStencil(IDirect3DSurface9* surface)
{
    setLightManagerDepthStencil(surface);
}

// Source-level LightManager implementation. The public type deliberately has ordinary virtual
// methods: BF2.exe callers (notably LightSource) dispatch through the recovered primary interface,
// while RendDX9 code can use the concrete type directly. The HostAbi adapters below exist only
// because 89 still-unknown signatures require the explicit 189-entry table to remain the DLL
// boundary; no source caller indexes that table.
void LightManager::addRef()
{
    (void)lm_addRef(this, 0);
}

int LightManager::getRef()
{
    return lm_getRef(this, 0);
}

int LightManager::release()
{
    return lm_release(this, 0);
}

void* LightManager::queryInterface(unsigned int iid) const
{
    return lm_queryInterface(const_cast<LightManager*>(this), 0, iid);
}

void LightManager::update(double time)
{
    lm_update(this, 0, time);
}

void LightManager::getLightsByMask(vc71::vector_layout& out, unsigned int required,
                                   unsigned int forbidden)
{
    lm_getLightsByMask(this, 0, &out, required, forbidden);
}

void LightManager::getVisibleLightsByMask(vc71::vector_layout& out, unsigned int required,
                                          unsigned int forbidden)
{
    lm_getVisibleLightsByMask(this, 0, &out, required, forbidden);
}

void LightManager::gatherLights(const Vec3& position, float radius, vc71::vector_layout& out,
                                unsigned int maxLights, unsigned int required,
                                unsigned int forbidden, bool occlusionTest)
{
    lm_gatherLights(this, 0, &position, radius, &out, maxLights, required, forbidden,
                    occlusionTest ? 1u : 0u);
}

void LightManager::gatherLightsForGeometry(void* geometry, vc71::vector_layout& out,
                                           unsigned int maxLights, unsigned int required,
                                           unsigned int forbidden, bool occlusionTest)
{
    lm_gatherLightsForGeometry(this, 0, geometry, &out, maxLights, required, forbidden,
                               occlusionTest ? 1 : 0);
}

int LightManager::getStaticMeshShadowLod() const
{
    return lm_getStaticMeshShadowLod(const_cast<LightManager*>(this), 0);
}

int LightManager::getShadowMergeLodLimit() const
{
    return lm_getShadowMergeLodLimit(const_cast<LightManager*>(this), 0);
}

void LightManager::setSkyLight(world::LightSource* light)
{
    lm_setSkyLight(this, 0, light);
}

world::LightSource* LightManager::getSkyLight() const
{
    return lm_getSkyLight(const_cast<LightManager*>(this), 0);
}

#define BF2_LIGHT_DEFINE_COLOR_PROPERTY(Name, Offset)                              \
    void LightManager::set##Name(const Vec3& color)                                \
    {                                                                               \
        lm_setVec3<Offset>(this, 0, &color);                                        \
    }                                                                               \
    const Vec3& LightManager::get##Name() const                                     \
    {                                                                               \
        return *lm_getVec3<Offset>(const_cast<LightManager*>(this), 0);             \
    }

BF2_LIGHT_DEFINE_COLOR_PROPERTY(SkyColor, 0x018u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(AmbientColor, 0x03Cu)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(TreeAmbientColor, 0x048u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(TreeSunColor, 0x024u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(TreeSkyColor, 0x030u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(StaticSunColor, 0x054u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(StaticSpecularColor, 0x06Cu)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(StaticSkyColor, 0x060u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(SinglePointColor, 0x078u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(DynamicPointColor, 0x084u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(EffectSunColor, 0x120u)
BF2_LIGHT_DEFINE_COLOR_PROPERTY(EffectShadowColor, 0x12Cu)

#undef BF2_LIGHT_DEFINE_COLOR_PROPERTY

// +0x4C/+0x50, PE 0x1015BD70/0x1015BD80. Both retail bodies ignore LightManager `this` and
// forward through the typed HemiMapManager singleton without a null guard. These are real
// LightManager member bodies; neither side of the call indexes a vtable manually.
void LightManager::setHemiLerpBias(float bias)
{
    g_HemiMapManager->setHemiLerpBias(bias);
}

float LightManager::getHemiLerpBias() const
{
    return g_HemiMapManager->getHemiLerpBias();
}

namespace {

// SettingsRepostitory +0x10, `const int& intGet(const std::string&, const int&)`, exactly as PE
// 0x1015CA20 makes it: `mov ecx, dword_10244A90` with no null test, and the answer dereferenced
// (`fild dword ptr [eax]`). [PLATFORM] carrier split as simpleShadowStringSetting's above: the native
// flavour calls the recovered SettingsRepostitory declaration, the retail flavour its +0x10 slot with
// the VC7.1 HostString key.
const int& groundHemiIntSetting(const renddx9abi::HostString& key, const int& fallback)
{
#if BF2_PLUGIN_ABI_NATIVE
    return settings->intGet(key, fallback);
#else
    typedef const int& (__thiscall* GetInt)(void*, const renddx9abi::HostString&, const int&);
    return reinterpret_cast<GetInt>((*reinterpret_cast<void***>(settings))[0x10 / 4])(
        settings, key, fallback);
#endif
}

} // anonymous namespace

// +0x44, PE 0x1015C9E0 (112 B, `retn 4`), `this` is never read - the
// body works on the published HemiMapManager (`mov ecx, g_HemiMapManager`, no null test), like the
// +0x4C/+0x50 bias pair above. In shipped order: the default 2048 stored, the key "GLSWorldSizeX"
// built, the origin zeroed, SettingsRepostitory +0x10 asked for the world size (GLSWorldSizeX - the
// heightmap size GameLogic::loadInitFile publishes, per the editor's lighting census §5.7), its int
// converted with `fild`, then setBaseHemiMap(name, 0/0/0, size, 500.0f) - the same falloff the
// setBaseHemiMap binding supplies for a missing fourth argument.
void LightManager::setGroundHemi(const LightManagerHostString& name)
{
    const int defaultWorldSize = 2048;
    const renddx9abi::HostString key("GLSWorldSizeX");
    Vec3 origin;
    origin.x = 0.0f;
    origin.y = 0.0f;
    origin.z = 0.0f;
    const float range = static_cast<float>(groundHemiIntSetting(key, defaultWorldSize));
    g_HemiMapManager->setBaseHemiMap(name, origin, range, 500.0f);
}

// +0x48, PE 0x1015CA50 (68 B), The guarded function-local static
// empty string (dword_102605D8 bit 0, atexit 0x101D9990) is constructed on the first call, before the
// HemiMapManager's +0x18 getBaseHemiMap is asked; a base map answers its name (+0x18), none the
// static. `this` is never read.
const vc71::string& LightManager::getGroundHemi() const
{
    static const vc71::string s_noGroundHemi;
    const HemiMap* const baseMap = g_HemiMapManager->getBaseHemiMap();
    if (baseMap)
        return baseMap->name;
    return s_noGroundHemi;
}

void LightManager::setLightCullDistance(float distance)
{
    lm_setLightCullDistance(this, 0, distance);
}

float LightManager::getLightCullDistance() const
{
    return lm_getLightCullDistance(const_cast<LightManager*>(this), 0);
}

void LightManager::setMaxLightsPerBundledMesh(int quality)
{
    lm_setMaxLightsPerBundledMesh(this, 0, quality);
}

int LightManager::getMaxLightsPerBundledMesh() const
{
    return lm_getMaxLightsPerBundledMesh(const_cast<LightManager*>(this), 0);
}

void LightManager::setMaxLightsPerStaticMesh(int quality)
{
    lm_setMaxLightsPerStaticMesh(this, 0, quality);
}

int LightManager::getMaxLightsPerStaticMesh() const
{
    return lm_getMaxLightsPerStaticMesh(const_cast<LightManager*>(this), 0);
}

void LightManager::setMaxLightsPerSkinnedMesh(int quality)
{
    lm_setMaxLightsPerSkinnedMesh(this, 0, quality);
}

int LightManager::getMaxLightsPerSkinnedMesh() const
{
    return lm_getMaxLightsPerSkinnedMesh(const_cast<LightManager*>(this), 0);
}

void LightManager::setMaxLightsPerTerrainPatch(int quality)
{
    lm_setMaxLightsPerTerrainPatch(this, 0, quality);
}

int LightManager::getMaxLightsPerTerrainPatch() const
{
    return lm_getMaxLightsPerTerrainPatch(const_cast<LightManager*>(this), 0);
}

void LightManager::setSkinnedMeshShaderLodDistance(float distance)
{
    lm_setSkinnedMeshShaderLodDistance(this, 0, distance);
}

float LightManager::getSkinnedMeshShaderLodDistance() const
{
    return lm_getSkinnedMeshShaderLodDistance(const_cast<LightManager*>(this), 0);
}

const Vec2& LightManager::getLightMapAmbientAlphaCutoffRange() const
{
    return lm_getLightMapAmbientAlphaCutoffRange(const_cast<LightManager*>(this), 0);
}

void LightManager::setLightMapAmbientAlphaCutoffRange(const Vec2& range)
{
    lm_setLightMapAmbientAlphaCutoffRange(this, 0, range);
}

void LightManager::setBundledMeshShadowLod(int lod)
{
    lm_setBundledMeshShadowLod(this, 0, lod);
}
int LightManager::getBundledMeshShadowLod() const
{
    return lm_getBundledMeshShadowLod(const_cast<LightManager*>(this), 0);
}
void LightManager::setShadowRoofRayLength(float length)
{
    lm_setShadowRoofRayLength(this, 0, length);
}
float LightManager::getShadowRoofRayLength() const
{
    return lm_getShadowRoofRayLength(const_cast<LightManager*>(this), 0);
}
void LightManager::setSkinnedMeshShadowLod(int lod)
{
    lm_setSkinnedMeshShadowLod(this, 0, lod);
}
int LightManager::getSkinnedMeshShadowLod() const
{
    return lm_getSkinnedMeshShadowLod(const_cast<LightManager*>(this), 0);
}
void LightManager::setShadowSunScalePower(float power)
{
    lm_setShadowSunScalePower(this, 0, power);
}
float LightManager::getShadowSunScalePower() const
{
    return lm_getShadowSunScalePower(const_cast<LightManager*>(this), 0);
}
void LightManager::setShadowSunHeightOverFade(const Vec2& range)
{
    lm_setShadowSunHeightOverFade(this, 0, range);
}
const Vec2& LightManager::getShadowSunHeightOverFade() const
{
    return lm_getShadowSunHeightOverFade(const_cast<LightManager*>(this), 0);
}
float LightManager::sampleSimpleShadowMap(float x, float z) const
{
    return sampleSimpleShadowMapFiltered<false>(const_cast<LightManager*>(this), x, z);
}
float LightManager::sampleSimpleShadowMapIO(float x, float z) const
{
    return sampleSimpleShadowMapFiltered<true>(const_cast<LightManager*>(this), x, z);
}
float LightManager::sampleSimpleShadowMapIOTexel(int x, int z) const
{
    return lm_sampleSimpleShadowMapIOTexel(const_cast<LightManager*>(this), 0, x, z);
}
float LightManager::sampleSimpleShadowMapTexel(int x, int z) const
{
    return lm_sampleSimpleShadowMapTexel(const_cast<LightManager*>(this), 0, x, z);
}

void LightManager::addLight(world::LightSource* light)
{
    lm_addLight(this, 0, light);
}

void LightManager::removeLight(world::LightSource* light)
{
    lm_removeLight(this, 0, light);
}

void LightManager::enableLight(world::LightSource* light)
{
    lm_enableLight(this, 0, light);
}

void LightManager::disableLight(world::LightSource* light)
{
    lm_disableLight(this, 0, light);
}

void LightManager::setShadowStaticFeature(bool enabled)
{
    lm_setShadowStaticFeature(this, 0, enabled);
}

bool LightManager::getShadowStaticFeature() const
{
    return lm_getShadowStaticFeature(const_cast<LightManager*>(this), 0);
}

void LightManager::setShadowIntensityBias(float value)
{
    lm_setShadowIntensityBias(this, 0, value);
}

float LightManager::getShadowIntensityBias() const
{
    return lm_getShadowIntensityBias(const_cast<LightManager*>(this), 0);
}

void LightManager::setLightMapIntensityBias(float value)
{
    lm_setLightMapIntensityBias(this, 0, value);
}

float LightManager::getLightMapIntensityBias() const
{
    return lm_getLightMapIntensityBias(const_cast<LightManager*>(this), 0);
}

void LightManager::setDynamicShadowsEnabled(bool enabled)
{
    lm_setDynamicShadowsEnabled(this, 0, enabled);
}

bool LightManager::getDynamicShadowsEnabled() const
{
    return lm_getDynamicShadowsEnabled(const_cast<LightManager*>(this), 0);
}

void LightManager::setShadowUpdatesEnabled(bool enabled)
{
    lm_setShadowUpdatesEnabled(this, 0, enabled);
}

bool LightManager::getShadowUpdatesEnabled() const
{
    return lm_getShadowUpdatesEnabled(const_cast<LightManager*>(this), 0);
}

void LightManager::setShadowTSMFocus(float value)
{
    lm_setShadowTSMFocus(this, 0, value);
}

float LightManager::getShadowTSMFocus() const
{
    return lm_getShadowTSMFocus(const_cast<LightManager*>(this), 0);
}

void LightManager::renderShadowMaps()
{
    lm_renderShadowMaps(this, 0);
}

void LightManager::setSunDirection(Vec3 direction)
{
    lm_setSunDirection(this, 0, direction);
}

Vec3 LightManager::getSunDirection() const
{
    Vec3 result;
    lm_getSunDirection(const_cast<LightManager*>(this), 0, &result);
    return result;
}

void LightManager::setSunColor(Vec3 color)
{
    lm_setSunColor(this, 0, color);
}

Vec3 LightManager::getSunColor() const
{
    Vec3 result;
    lm_getSunColor(const_cast<LightManager*>(this), 0, &result);
    return result;
}

void LightManager::setSunSpecColor(Vec3 color)
{
    lm_setSunSpecColor(this, 0, color);
}

Vec3 LightManager::getSunSpecColor() const
{
    Vec3 result;
    lm_getSunSpecColor(const_cast<LightManager*>(this), 0, &result);
    return result;
}

void LightManager::setSunEnabled(bool enabled)
{
    lm_setSunEnabled(this, 0, enabled);
}

bool LightManager::getSunEnabled() const
{
    return lm_getSunEnabled(const_cast<LightManager*>(this), 0);
}

void LightManager::setDefaultEffectLightAffectionFactor(float factor)
{
    lm_setDefaultEffectFactor(this, 0, factor);
}

float LightManager::getDefaultEffectLightAffectionFactor() const
{
    return lm_getDefaultEffectFactor(const_cast<LightManager*>(this), 0);
}

void LightManager::setShadowUseSweepTest(bool enabled)
{
    lm_setShadowUseSweepTest(this, 0, enabled);
}

bool LightManager::getShadowUseSweepTest() const
{
    return lm_getShadowUseSweepTest(const_cast<LightManager*>(this), 0);
}

void LightManager::setDynamicLightRangeScale(float scale)
{
    lm_setDynamicLightRangeScale(this, 0, scale);
}

float LightManager::getDynamicLightRangeScale() const
{
    return lm_getDynamicLightRangeScale(const_cast<LightManager*>(this), 0);
}

void LightManager::setOccludedSkyNightIntensity(float intensity)
{
    lm_setOccludedSkyNightIntensity(this, 0, intensity);
}

float LightManager::getOccludedSkyNightIntensity() const
{
    return lm_getOccludedSkyNightIntensity(const_cast<LightManager*>(this), 0);
}

void LightManager::registerShadowEffect(int reflectionTypeId, dx9rend::Effect* effect)
{
    lm_registerShadowEffect(this, 0, reflectionTypeId, effect);
}

// The explicit host table requires plain x86 entry points. Keeping these adapters nested under
// LightManager makes the boundary unmistakable; every adapter immediately enters the corresponding
// qualified member body and none performs a vtable lookup.
struct LightManager::HostAbi
{
    static LightManager& object(void* self) { return *static_cast<LightManager*>(self); }

    static void __fastcall addRef(void* self, void*) { object(self).LightManager::addRef(); }
    static int __fastcall getRef(void* self, void*) { return object(self).LightManager::getRef(); }
    static int __fastcall release(void* self, void*) { return object(self).LightManager::release(); }
    static void* __fastcall queryInterface(void* self, void*, unsigned int iid)
    { return object(self).LightManager::queryInterface(iid); }
    static void __fastcall update(void* self, void*, double time)
    { object(self).LightManager::update(time); }
    static void __fastcall getLightsByMask(void* self, void*, vc71::vector_layout* out,
                                           unsigned int required, unsigned int forbidden)
    { object(self).LightManager::getLightsByMask(*out, required, forbidden); }
    static void __fastcall getVisibleLightsByMask(void* self, void*, vc71::vector_layout* out,
                                                  unsigned int required, unsigned int forbidden)
    { object(self).LightManager::getVisibleLightsByMask(*out, required, forbidden); }
    static int __fastcall getStaticMeshShadowLod(void* self, void*)
    { return object(self).LightManager::getStaticMeshShadowLod(); }
    static void __fastcall setBundledMeshShadowLod(void* self, void*, int lod)
    { object(self).LightManager::setBundledMeshShadowLod(lod); }
    static int __fastcall getBundledMeshShadowLod(void* self, void*)
    { return object(self).LightManager::getBundledMeshShadowLod(); }
    static int __fastcall getShadowMergeLodLimit(void* self, void*)
    { return object(self).LightManager::getShadowMergeLodLimit(); }
    static void __fastcall gatherLights(void* self, void*, const Vec3* position, float radius,
                                        vc71::vector_layout* out, unsigned int maxLights,
                                        unsigned int required, unsigned int forbidden,
                                        unsigned char occlusionTest)
    { object(self).LightManager::gatherLights(*position, radius, *out, maxLights, required,
                                              forbidden, occlusionTest != 0); }
    static void __fastcall gatherLightsForGeometry(void* self, void*, void* geometry,
                                                   vc71::vector_layout* out,
                                                   unsigned int maxLights, unsigned int required,
                                                   unsigned int forbidden, int occlusionTest)
    { object(self).LightManager::gatherLightsForGeometry(geometry, *out, maxLights, required,
                                                         forbidden, occlusionTest != 0); }
    static void __fastcall setSkyLight(void* self, void*, world::LightSource* light)
    { object(self).LightManager::setSkyLight(light); }
    static world::LightSource* __fastcall getSkyLight(void* self, void*)
    { return object(self).LightManager::getSkyLight(); }

#define BF2_LIGHT_HOST_COLOR(Name)                                                \
    static void __fastcall set##Name(void* self, void*, const Vec3* color)         \
    { object(self).LightManager::set##Name(*color); }                              \
    static const Vec3* __fastcall get##Name(void* self, void*)                     \
    { return &object(self).LightManager::get##Name(); }

    BF2_LIGHT_HOST_COLOR(SkyColor)
    BF2_LIGHT_HOST_COLOR(AmbientColor)
    BF2_LIGHT_HOST_COLOR(TreeAmbientColor)
    BF2_LIGHT_HOST_COLOR(TreeSunColor)
    BF2_LIGHT_HOST_COLOR(TreeSkyColor)
    BF2_LIGHT_HOST_COLOR(StaticSunColor)
    BF2_LIGHT_HOST_COLOR(StaticSpecularColor)
    BF2_LIGHT_HOST_COLOR(StaticSkyColor)
    BF2_LIGHT_HOST_COLOR(SinglePointColor)
    BF2_LIGHT_HOST_COLOR(DynamicPointColor)
    BF2_LIGHT_HOST_COLOR(EffectSunColor)
    BF2_LIGHT_HOST_COLOR(EffectShadowColor)

#undef BF2_LIGHT_HOST_COLOR

    static void __fastcall setGroundHemi(void* self, void*, const LightManagerHostString* name)
    { object(self).LightManager::setGroundHemi(*name); }
    static const vc71::string* __fastcall getGroundHemi(void* self, void*)
    { return &object(self).LightManager::getGroundHemi(); }
    static void __fastcall setHemiLerpBias(void* self, void*, float bias)
    { object(self).LightManager::setHemiLerpBias(bias); }
    static float __fastcall getHemiLerpBias(void* self, void*)
    { return object(self).LightManager::getHemiLerpBias(); }
    static void __fastcall setLightCullDistance(void* self, void*, float value)
    { object(self).LightManager::setLightCullDistance(value); }
    static float __fastcall getLightCullDistance(void* self, void*)
    { return object(self).LightManager::getLightCullDistance(); }

#define BF2_LIGHT_HOST_INT_PROPERTY(Name)                                         \
    static void __fastcall set##Name(void* self, void*, int value)                 \
    { object(self).LightManager::set##Name(value); }                                \
    static int __fastcall get##Name(void* self, void*)                              \
    { return object(self).LightManager::get##Name(); }

    BF2_LIGHT_HOST_INT_PROPERTY(MaxLightsPerBundledMesh)
    BF2_LIGHT_HOST_INT_PROPERTY(MaxLightsPerStaticMesh)
    BF2_LIGHT_HOST_INT_PROPERTY(MaxLightsPerSkinnedMesh)
    BF2_LIGHT_HOST_INT_PROPERTY(MaxLightsPerTerrainPatch)
    BF2_LIGHT_HOST_INT_PROPERTY(SkinnedMeshShadowLod)

#undef BF2_LIGHT_HOST_INT_PROPERTY

    static void __fastcall addLight(void* self, void*, world::LightSource* light)
    { object(self).LightManager::addLight(light); }
    static void __fastcall removeLight(void* self, void*, world::LightSource* light)
    { object(self).LightManager::removeLight(light); }
    static void __fastcall enableLight(void* self, void*, world::LightSource* light)
    { object(self).LightManager::enableLight(light); }
    static void __fastcall disableLight(void* self, void*, world::LightSource* light)
    { object(self).LightManager::disableLight(light); }

#define BF2_LIGHT_HOST_BOOL_PROPERTY(Name)                                        \
    static void __fastcall set##Name(void* self, void*, bool value)                \
    { object(self).LightManager::set##Name(value); }                                \
    static bool __fastcall get##Name(void* self, void*)                             \
    { return object(self).LightManager::get##Name(); }

    BF2_LIGHT_HOST_BOOL_PROPERTY(ShadowStaticFeature)
    BF2_LIGHT_HOST_BOOL_PROPERTY(DynamicShadowsEnabled)
    BF2_LIGHT_HOST_BOOL_PROPERTY(ShadowUpdatesEnabled)
    BF2_LIGHT_HOST_BOOL_PROPERTY(ShadowUseSweepTest)

#undef BF2_LIGHT_HOST_BOOL_PROPERTY

#define BF2_LIGHT_HOST_FLOAT_PROPERTY(Name)                                       \
    static void __fastcall set##Name(void* self, void*, float value)               \
    { object(self).LightManager::set##Name(value); }                                \
    static float __fastcall get##Name(void* self, void*)                            \
    { return object(self).LightManager::get##Name(); }

    BF2_LIGHT_HOST_FLOAT_PROPERTY(SkinnedMeshShaderLodDistance)
    BF2_LIGHT_HOST_FLOAT_PROPERTY(ShadowRoofRayLength)
    BF2_LIGHT_HOST_FLOAT_PROPERTY(ShadowSunScalePower)
    BF2_LIGHT_HOST_FLOAT_PROPERTY(ShadowIntensityBias)
    BF2_LIGHT_HOST_FLOAT_PROPERTY(LightMapIntensityBias)
    BF2_LIGHT_HOST_FLOAT_PROPERTY(ShadowTSMFocus)
    BF2_LIGHT_HOST_FLOAT_PROPERTY(DefaultEffectLightAffectionFactor)
    BF2_LIGHT_HOST_FLOAT_PROPERTY(DynamicLightRangeScale)
    BF2_LIGHT_HOST_FLOAT_PROPERTY(OccludedSkyNightIntensity)

#undef BF2_LIGHT_HOST_FLOAT_PROPERTY

    static const Vec2* __fastcall getLightMapAmbientAlphaCutoffRange(void* self, void*)
    { return &object(self).LightManager::getLightMapAmbientAlphaCutoffRange(); }
    static void __fastcall setLightMapAmbientAlphaCutoffRange(void* self, void*, const Vec2* range)
    { object(self).LightManager::setLightMapAmbientAlphaCutoffRange(*range); }
    static void __fastcall setShadowSunHeightOverFade(void* self, void*, const Vec2* range)
    { object(self).LightManager::setShadowSunHeightOverFade(*range); }
    static const Vec2* __fastcall getShadowSunHeightOverFade(void* self, void*)
    { return &object(self).LightManager::getShadowSunHeightOverFade(); }
    static float __fastcall sampleSimpleShadowMap(void* self, void*, float x, float z)
    { return object(self).LightManager::sampleSimpleShadowMap(x, z); }
    static float __fastcall sampleSimpleShadowMapIO(void* self, void*, float x, float z)
    { return object(self).LightManager::sampleSimpleShadowMapIO(x, z); }
    static float __fastcall sampleSimpleShadowMapIOTexel(void* self, void*, int x, int z)
    { return object(self).LightManager::sampleSimpleShadowMapIOTexel(x, z); }
    static float __fastcall sampleSimpleShadowMapTexel(void* self, void*, int x, int z)
    { return object(self).LightManager::sampleSimpleShadowMapTexel(x, z); }

    static void __fastcall renderShadowMaps(void* self, void*)
    { object(self).LightManager::renderShadowMaps(); }
    static void __fastcall setSunDirection(void* self, void*, Vec3 value)
    { object(self).LightManager::setSunDirection(value); }
    static Vec3* __fastcall getSunDirection(void* self, void*, Vec3* out)
    { *out = object(self).LightManager::getSunDirection(); return out; }
    static void __fastcall setSunColor(void* self, void*, Vec3 value)
    { object(self).LightManager::setSunColor(value); }
    static Vec3* __fastcall getSunColor(void* self, void*, Vec3* out)
    { *out = object(self).LightManager::getSunColor(); return out; }
    static void __fastcall setSunSpecColor(void* self, void*, Vec3 value)
    { object(self).LightManager::setSunSpecColor(value); }
    static Vec3* __fastcall getSunSpecColor(void* self, void*, Vec3* out)
    { *out = object(self).LightManager::getSunSpecColor(); return out; }
    static void __fastcall setSunEnabled(void* self, void*, bool enabled)
    { object(self).LightManager::setSunEnabled(enabled); }
    static bool __fastcall getSunEnabled(void* self, void*)
    { return object(self).LightManager::getSunEnabled(); }
    static void __fastcall registerShadowEffect(
        void* self, void*, int reflectionTypeId, dx9rend::Effect* effect)
    { object(self).LightManager::registerShadowEffect(reflectionTypeId, effect); }
    static void __fastcall loadSimpleShadowMaps(void* self, void*)
    { object(self).LightManager::loadSimpleShadowMaps(); }
};

namespace {

void __fastcall lm_deviceLost(void* self, void*)
{
    releaseShadowResources(static_cast<unsigned char*>(self) - 4, true);
}
void __fastcall lm_deviceReset(void* self, void*)
{
    createShadowResourcesBody(static_cast<unsigned char*>(self) - 4);
}
void __fastcall lm_deviceNull(void*, void*) {}

void __fastcall lm_renderPathActivated(void* self, void*, int path)
{
    void* const base = static_cast<unsigned char*>(self) - 8;
    rebuildShadowAtlas(base);
    if (path == 2)
    {
        void* const effect = dx9rend::shaderManagerGetEffect(
            dx9rend::g_shaderManager, "Shaders/LightManager_r3x0");
        lmField<void*>(base, 0x844) = effect;
        struct ParameterSpec { unsigned int offset; const char* semantic; };
        static const ParameterSpec parameters[] =
        {
            {0x848, "STENCILFUNC"}, {0x84C, "STENCILREF"}, {0x850, "STENCILPASS"},
            {0x854, "PARABOLOIDZVALUES"}, {0x85C, "LIGHTDIR"},
            {0x858, "LIGHTWORLDPOS"}, {0x860, "LIGHTPOS"}, {0x864, "LIGHTCOL"},
            {0x868, "LIGHTATTENUATIONRANGE"}, {0x86C, "LIGHTATTENUATIONRANGEINV"},
            {0x870, "PROJECTORMASK"}, {0x874, "EYEPOS"}, {0x878, "EYEDOF"},
            {0x87C, "VIEWPROJ"}, {0x880, "WORLDVIEWPROJ"},
            {0x884, "LIGHTVIEWPROJ"}, {0x888, "LIGHTOCCLUDERVIEWPROJ"},
            {0x88C, "VIEWPORTMAP"}, {0x890, "VIEWPORTMAP2"},
            {0x894, "SHADOWINTENSITYBIAS"}, {0x898, "LIGHTMAPINTENSITYBIAS"},
            {0x89C, "TEXLAYER0"}, {0x8A0, "TEXLAYER1"}, {0x8A4, "TEXLAYER2"},
            {0x8A8, "TEXLAYER3"}, {0x8AC, "TEXLAYER4"}, {0x8B0, "TEXLAYER5"},
            {0x8B4, "TEXLAYER6"}
        };
        for (unsigned int i = 0; i < sizeof(parameters) / sizeof(parameters[0]); ++i)
            lmField<void*>(base, parameters[i].offset) =
                getEffectParameter(effect, parameters[i].semantic);
        lm_setShadowIntensityBias(base, 0, 0.0f);
        lm_setLightMapIntensityBias(base, 0, 0.0f);
    }
    lmField<dx9rend::EngineTexture*>(base, 0x8B8) =
        loadManagerTexture("Common/Textures/RadialAtt2");
    lmField<dx9rend::EngineTexture*>(base, 0x8BC) =
        loadManagerTexture("Common/Textures/ConicalAtt");
}
void __fastcall lm_renderPathNull04(void*, void*) {}
void __fastcall lm_renderPathNull08(void*, void*, int) {}
void __fastcall lm_renderPathNull0C(void*, void*, void*) {}
void* __fastcall lm_getRenderPath(void* self, void*)
{
    // PE 0x101A9A10 returns secondaryThis[1].  The render-path listener subobject starts at
    // base+0x08, so this is the pointer at base+0x0C (zero in the partial constructor).
    return lmField<void*>(self, 0x04);
}
renddx9abi::HostString* __fastcall lm_getRenderPathName(
    void*, void*, renddx9abi::HostString* out)
{
    // PE 0x100F7910 constructs this exact string into the caller's MSVC x86 sret storage and
    // returns it in EAX (`retn 4`). HostString selects the native or VC7.1 representation at the
    // plugin boundary without changing that physical call shape.
    new (out) renddx9abi::HostString("aLightManager");
    return out;
}

const void* const kDeviceLostVtable[3] =
{
    reinterpret_cast<void*>(&lm_deviceLost),
    reinterpret_cast<void*>(&lm_deviceReset),
    reinterpret_cast<void*>(&lm_deviceNull)
};

const void* const kRenderPathVtable[6] =
{
    reinterpret_cast<void*>(&lm_renderPathActivated),
    reinterpret_cast<void*>(&lm_renderPathNull04),
    reinterpret_cast<void*>(&lm_renderPathNull08),
    reinterpret_cast<void*>(&lm_renderPathNull0C),
    reinterpret_cast<void*>(&lm_getRenderPath),
    reinterpret_cast<void*>(&lm_getRenderPathName)
};

// PE vtable 0x1021AAF0: 189 entries, +0x000..+0x2F0.
const void* const kPrimaryVtable[189] =
{
    reinterpret_cast<void*>(&LightManager::HostAbi::addRef), // +0x000
    reinterpret_cast<void*>(&LightManager::HostAbi::getRef), // +0x004
    reinterpret_cast<void*>(&LightManager::HostAbi::release), // +0x008
    reinterpret_cast<void*>(&LightManager::HostAbi::queryInterface), // +0x00C
    reinterpret_cast<void*>(&LightManager::HostAbi::update), // +0x010
    reinterpret_cast<void*>(&LightManager::HostAbi::getLightsByMask), // +0x014
    reinterpret_cast<void*>(&LightManager::HostAbi::getVisibleLightsByMask), // +0x018
    reinterpret_cast<void*>(&LightManager::HostAbi::gatherLights), // +0x01C
    reinterpret_cast<void*>(&LightManager::HostAbi::gatherLightsForGeometry), // +0x020
    reinterpret_cast<void*>(&LightManager::HostAbi::setSkyLight), // +0x024
    reinterpret_cast<void*>(&LightManager::HostAbi::getSkyLight), // +0x028
    reinterpret_cast<void*>(&LightManager::HostAbi::setSkyColor), // +0x02C
    reinterpret_cast<void*>(&LightManager::HostAbi::getSkyColor), // +0x030
    reinterpret_cast<void*>(&LightManager::HostAbi::setAmbientColor), // +0x034
    reinterpret_cast<void*>(&LightManager::HostAbi::getAmbientColor), // +0x038
    reinterpret_cast<void*>(&LightManager::HostAbi::setTreeAmbientColor), // +0x03C
    reinterpret_cast<void*>(&LightManager::HostAbi::getTreeAmbientColor), // +0x040
    reinterpret_cast<void*>(&LightManager::HostAbi::setGroundHemi), // +0x044
    reinterpret_cast<void*>(&LightManager::HostAbi::getGroundHemi), // +0x048
    reinterpret_cast<void*>(&LightManager::HostAbi::setHemiLerpBias), // +0x04C
    reinterpret_cast<void*>(&LightManager::HostAbi::getHemiLerpBias), // +0x050
    reinterpret_cast<void*>(&LightManager::HostAbi::setLightCullDistance), // +0x054
    reinterpret_cast<void*>(&LightManager::HostAbi::getLightCullDistance), // +0x058
    reinterpret_cast<void*>(&LightManager::HostAbi::setMaxLightsPerBundledMesh), // +0x05C
    reinterpret_cast<void*>(&LightManager::HostAbi::getMaxLightsPerBundledMesh), // +0x060
    reinterpret_cast<void*>(&LightManager::HostAbi::setMaxLightsPerStaticMesh), // +0x064
    reinterpret_cast<void*>(&LightManager::HostAbi::getMaxLightsPerStaticMesh), // +0x068
    reinterpret_cast<void*>(&LightManager::HostAbi::setMaxLightsPerSkinnedMesh), // +0x06C
    reinterpret_cast<void*>(&LightManager::HostAbi::getMaxLightsPerSkinnedMesh), // +0x070
    reinterpret_cast<void*>(&LightManager::HostAbi::setMaxLightsPerTerrainPatch), // +0x074
    reinterpret_cast<void*>(&LightManager::HostAbi::getMaxLightsPerTerrainPatch), // +0x078
    reinterpret_cast<void*>(&LightManager::HostAbi::setSkinnedMeshShaderLodDistance), // +0x07C
    reinterpret_cast<void*>(&LightManager::HostAbi::getSkinnedMeshShaderLodDistance), // +0x080
    reinterpret_cast<void*>(&LightManager::HostAbi::getLightMapAmbientAlphaCutoffRange), // +0x084
    reinterpret_cast<void*>(&LightManager::HostAbi::setLightMapAmbientAlphaCutoffRange), // +0x088
    reinterpret_cast<void*>(&LightManager::HostAbi::addLight), // +0x08C
    reinterpret_cast<void*>(&LightManager::HostAbi::removeLight), // +0x090
    reinterpret_cast<void*>(&LightManager::HostAbi::enableLight), // +0x094
    reinterpret_cast<void*>(&LightManager::HostAbi::disableLight), // +0x098
    reinterpret_cast<void*>(&lm_unrecovered<0x09Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0A0u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0A4u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0A8u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0ACu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0B0u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0B4u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0B8u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0BCu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0C0u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setShadowStaticFeature), // +0x0C4
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowStaticFeature), // +0x0C8
    reinterpret_cast<void*>(&lm_unrecovered<0x0CCu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0D0u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setShadowIntensityBias), // +0x0D4
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowIntensityBias), // +0x0D8
    reinterpret_cast<void*>(&LightManager::HostAbi::setLightMapIntensityBias), // +0x0DC
    reinterpret_cast<void*>(&LightManager::HostAbi::getLightMapIntensityBias), // +0x0E0
    reinterpret_cast<void*>(&lm_unrecovered<0x0E4u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0E8u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setDynamicShadowsEnabled), // +0x0EC
    reinterpret_cast<void*>(&LightManager::HostAbi::getDynamicShadowsEnabled), // +0x0F0
    reinterpret_cast<void*>(&lm_unrecovered<0x0F4u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0F8u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x0FCu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x100u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x104u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x108u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x10Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x110u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x114u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x118u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x11Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x120u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setShadowUpdatesEnabled), // +0x124
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowUpdatesEnabled), // +0x128
    reinterpret_cast<void*>(&lm_unrecovered<0x12Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x130u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x134u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x138u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x13Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x140u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x144u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x148u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x14Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x150u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x154u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x158u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x15Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x160u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x164u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x168u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x16Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x170u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x174u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x178u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x17Cu>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setShadowTSMFocus), // +0x180
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowTSMFocus), // +0x184
    reinterpret_cast<void*>(&lm_unrecovered<0x188u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x18Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x190u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x194u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x198u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::renderShadowMaps), // +0x19C
    reinterpret_cast<void*>(&LightManager::HostAbi::setSunDirection), // +0x1A0
    reinterpret_cast<void*>(&LightManager::HostAbi::getSunDirection), // +0x1A4
    reinterpret_cast<void*>(&LightManager::HostAbi::setSunColor), // +0x1A8
    reinterpret_cast<void*>(&LightManager::HostAbi::getSunColor), // +0x1AC
    reinterpret_cast<void*>(&LightManager::HostAbi::setSunSpecColor), // +0x1B0
    reinterpret_cast<void*>(&LightManager::HostAbi::getSunSpecColor), // +0x1B4
    reinterpret_cast<void*>(&LightManager::HostAbi::setSunEnabled), // +0x1B8
    reinterpret_cast<void*>(&LightManager::HostAbi::getSunEnabled), // +0x1BC
    reinterpret_cast<void*>(&lm_unrecovered<0x1C0u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setTreeSunColor), // +0x1C4
    reinterpret_cast<void*>(&LightManager::HostAbi::getTreeSunColor), // +0x1C8
    reinterpret_cast<void*>(&LightManager::HostAbi::setTreeSkyColor), // +0x1CC
    reinterpret_cast<void*>(&LightManager::HostAbi::getTreeSkyColor), // +0x1D0
    reinterpret_cast<void*>(&LightManager::HostAbi::setStaticSunColor), // +0x1D4
    reinterpret_cast<void*>(&LightManager::HostAbi::getStaticSunColor), // +0x1D8
    reinterpret_cast<void*>(&LightManager::HostAbi::setStaticSpecularColor), // +0x1DC
    reinterpret_cast<void*>(&LightManager::HostAbi::getStaticSpecularColor), // +0x1E0
    reinterpret_cast<void*>(&LightManager::HostAbi::setStaticSkyColor), // +0x1E4
    reinterpret_cast<void*>(&LightManager::HostAbi::getStaticSkyColor), // +0x1E8
    reinterpret_cast<void*>(&LightManager::HostAbi::setSinglePointColor), // +0x1EC
    reinterpret_cast<void*>(&LightManager::HostAbi::getSinglePointColor), // +0x1F0
    reinterpret_cast<void*>(&LightManager::HostAbi::setDynamicPointColor), // +0x1F4
    reinterpret_cast<void*>(&LightManager::HostAbi::getDynamicPointColor), // +0x1F8
    reinterpret_cast<void*>(&lm_unrecovered<0x1FCu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x200u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x204u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x208u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x20Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x210u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x214u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x218u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x21Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x220u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x224u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x228u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x22Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x230u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x234u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x238u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x23Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x240u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x244u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x248u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x24Cu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x250u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x254u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x258u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setEffectSunColor), // +0x25C
    reinterpret_cast<void*>(&LightManager::HostAbi::getEffectSunColor), // +0x260
    reinterpret_cast<void*>(&LightManager::HostAbi::setEffectShadowColor), // +0x264
    reinterpret_cast<void*>(&LightManager::HostAbi::getEffectShadowColor), // +0x268
    reinterpret_cast<void*>(&LightManager::HostAbi::setDefaultEffectLightAffectionFactor), // +0x26C
    reinterpret_cast<void*>(&LightManager::HostAbi::getDefaultEffectLightAffectionFactor), // +0x270
    reinterpret_cast<void*>(&lm_unrecovered<0x274u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x278u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x27Cu>),
    reinterpret_cast<void*>(&LightManager::HostAbi::getStaticMeshShadowLod), // +0x280
    reinterpret_cast<void*>(&LightManager::HostAbi::setBundledMeshShadowLod), // +0x284
    reinterpret_cast<void*>(&LightManager::HostAbi::getBundledMeshShadowLod), // +0x288
    reinterpret_cast<void*>(&LightManager::HostAbi::setSkinnedMeshShadowLod), // +0x28C
    reinterpret_cast<void*>(&LightManager::HostAbi::getSkinnedMeshShadowLod), // +0x290
    reinterpret_cast<void*>(&lm_unrecovered<0x294u>),
    reinterpret_cast<void*>(&lm_unrecovered<0x298u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setShadowRoofRayLength), // +0x29C
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowRoofRayLength), // +0x2A0
    reinterpret_cast<void*>(&LightManager::HostAbi::setShadowSunScalePower), // +0x2A4
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowSunScalePower), // +0x2A8
    reinterpret_cast<void*>(&lm_unrecovered<0x2ACu>),
    reinterpret_cast<void*>(&lm_unrecovered<0x2B0u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::setShadowUseSweepTest), // +0x2B4
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowUseSweepTest), // +0x2B8
    reinterpret_cast<void*>(&LightManager::HostAbi::setShadowSunHeightOverFade), // +0x2BC
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowSunHeightOverFade), // +0x2C0
    reinterpret_cast<void*>(&LightManager::HostAbi::sampleSimpleShadowMap), // +0x2C4
    reinterpret_cast<void*>(&LightManager::HostAbi::sampleSimpleShadowMapIO), // +0x2C8
    reinterpret_cast<void*>(&LightManager::HostAbi::sampleSimpleShadowMapIOTexel), // +0x2CC
    reinterpret_cast<void*>(&LightManager::HostAbi::sampleSimpleShadowMapTexel), // +0x2D0
    reinterpret_cast<void*>(&lm_unrecovered<0x2D4u>),
    reinterpret_cast<void*>(&LightManager::HostAbi::getShadowMergeLodLimit), // +0x2D8
    reinterpret_cast<void*>(&LightManager::HostAbi::setDynamicLightRangeScale), // +0x2DC
    reinterpret_cast<void*>(&LightManager::HostAbi::getDynamicLightRangeScale), // +0x2E0
    reinterpret_cast<void*>(&LightManager::HostAbi::setOccludedSkyNightIntensity), // +0x2E4
    reinterpret_cast<void*>(&LightManager::HostAbi::getOccludedSkyNightIntensity), // +0x2E8
    reinterpret_cast<void*>(&LightManager::HostAbi::loadSimpleShadowMaps), // +0x2EC
    reinterpret_cast<void*>(&LightManager::HostAbi::registerShadowEffect), // +0x2F0
};

void constructLightManager(void* self)
{
    // PE 0x100F4770 performs explicit member construction; it does not blanket-zero the allocation.
    lmField<const void**>(self, 0x00) = const_cast<const void**>(kPrimaryVtable);
    lmField<const void**>(self, 0x04) = const_cast<const void**>(kDeviceLostVtable);
    lmField<const void**>(self, 0x08) = const_cast<const void**>(kRenderPathVtable);
    lmField<int>(self, 0x10) = 1;
    lmField<void*>(self, 0x14) = 0;

    lmField<float>(self, 0x18) = 0.4f;
    lmField<float>(self, 0x1C) = 0.4f;
    lmField<float>(self, 0x20) = 0.7f;
    for (unsigned int offset = 0x24; offset <= 0x38; offset += 4)
        lmField<float>(self, offset) = 1.0f;
    for (unsigned int offset = 0x3C; offset <= 0x44; offset += 4)
        lmField<float>(self, offset) = 0.3f;
    for (unsigned int offset = 0x48; offset <= 0x50; offset += 4)
        lmField<unsigned int>(self, offset) = 0;
    lmField<float>(self, 0x54) = 1.0f;
    lmField<float>(self, 0x58) = 0.5f;
    lmField<float>(self, 0x5C) = 0.2f;
    lmField<float>(self, 0x60) = 0.3f;
    lmField<float>(self, 0x64) = 1.0f;
    lmField<float>(self, 0x68) = 0.5f;
    for (unsigned int offset = 0x6C; offset <= 0x7C; offset += 4)
        lmField<float>(self, offset) = 1.0f;
    lmField<float>(self, 0x80) = 0.8f;
    for (unsigned int offset = 0x84; offset <= 0x8C; offset += 4)
        lmField<float>(self, offset) = 1.0f;
    for (unsigned int offset = 0x90; offset <= 0x11C; offset += 4)
        lmField<unsigned int>(self, offset) = 0;
    for (unsigned int offset = 0x120; offset <= 0x134; offset += 4)
        lmField<float>(self, offset) = 1.0f;

    lmField<float>(self, 0x138) = 1.0f;
    lmField<float>(self, 0x13C) = 300.0f;
    lmField<int>(self, 0x140) = 3;
    lmField<int>(self, 0x144) = 3;
    lmField<int>(self, 0x148) = 3;
    lmField<int>(self, 0x14C) = 7;
    lmField<unsigned int>(self, 0x150) = 1;
    lmField<unsigned int>(self, 0x154) = 1;
    lmField<unsigned int>(self, 0x158) = 0;
    lmField<unsigned int>(self, 0x15C) = 0;
    lmField<int>(self, 0x160) = -1;
    lmField<float>(self, 0x164) = 150.0f;
    lmField<float>(self, 0x168) = 1.0f;
    lmField<float>(self, 0x16C) = 200.0f;
    lmField<unsigned char>(self, 0x170) = 1;
    lmField<float>(self, 0x174) = 3.0f;
    lmField<float>(self, 0x178) = 6.0f;
    lmField<void*>(self, 0x17C) = 0;
    constructEmptyVector(self, 0x180);
    for (unsigned int offset = 0x190; offset <= 0x1B0; offset += 4)
        lmField<unsigned int>(self, offset) = 0;
    lmField<float>(self, 0x1B4) = 10.0f;

    // The two VC7.1 std::set<LightSource*> facades own distinct sentinels. Retail dereferences the
    // allocation results unconditionally; do not introduce an OOM recovery arm here.
    constructLightSet(lmOwnedLights(self));
    constructLightSet(lmActiveLights(self));
    constructEmptyVector(self, 0x1D0);
    constructEmptyVector(self, 0x1E0);
    constructShadowTree(lmField<ShadowTree>(self, 0x1F0));
    lmField<float>(self, 0x1FC) = 0.1f;
    lmField<float>(self, 0x200) = 0.5f;
    lmField<unsigned int>(self, 0x204) = 1024;
    lmField<unsigned int>(self, 0x208) = 256;
    lmField<float>(self, 0x214) = 40.0f;
    lmField<float>(self, 0x218) = 40.0f;
    for (unsigned int offset = 0x21C; offset <= 0x238; offset += 4)
        lmField<void*>(self, offset) = 0;
    lmField<void*>(self, 0x24C) = 0;
    lmField<void*>(self, 0x250) = 0;
    lmField<unsigned int>(self, 0x264) = 0;
    lmField<unsigned char>(self, 0x268) = 1;
    lmField<unsigned int>(self, 0x274) = 0;

    new (static_cast<unsigned char*>(self) + 0x278) Frustum();
    new (static_cast<unsigned char*>(self) + 0x460) Frustum();
    setIdentityMatrix(self, 0x648);
    setIdentityMatrix(self, 0x688);
    setIdentityMatrix(self, 0x6C8);
    setIdentityMatrix(self, 0x708);
    setIdentityMatrix(self, 0x748);
    lmField<unsigned char>(self, 0x788) = 0;
    constructEmptyVector(self, 0x79C);
    constructEmptyVector(self, 0x7AC);
    constructEmptyVector(self, 0x7BC);
    constructEmptyVector(self, 0x7CC);
    constructEmptyVector(self, 0x7DC);

    lmField<unsigned char>(self, 0x7EC) = 1;
    lmField<unsigned char>(self, 0x7ED) = 1;
    lmField<unsigned char>(self, 0x7EE) = 1;
    lmField<unsigned char>(self, 0x7EF) = 0;
    lmField<float>(self, 0x7F0) = 0.2f;
    lmField<float>(self, 0x7F4) = 1.0f;
    lmField<unsigned char>(self, 0x7F8) = 1;
    lmField<unsigned char>(self, 0x7F9) = 1;
    lmField<float>(self, 0x7FC) = 1.0f;
    lmField<unsigned char>(self, 0x800) = 0;
    lmField<unsigned char>(self, 0x801) = 0;
    lmField<unsigned char>(self, 0x802) = 1;
    lmField<unsigned char>(self, 0x803) = 1;
    lmField<unsigned int>(self, 0x804) = 0;
    lmField<unsigned int>(self, 0x808) = 0;
    lmField<float>(self, 0x80C) = 1.5f;
    lmField<float>(self, 0x810) = 60.0f;
    lmField<float>(self, 0x814) = 0.52f;
    lmField<float>(self, 0x818) = -0.6f;
    lmField<unsigned char>(self, 0x81C) = 1;
    lmField<unsigned int>(self, 0x820) = 16;
    lmField<float>(self, 0x824) = 1.0f;
    lmField<float>(self, 0x828) = 0.0f;
    lmField<float>(self, 0x82C) = 0.005f;
    lmField<float>(self, 0x830) = 1.0f;
    lmField<double>(self, 0x838) = 1.0;
    for (unsigned int offset = 0x840; offset <= 0x8BC; offset += 4)
        lmField<void*>(self, offset) = 0;

    constructShadowTree(lmField<ShadowTree>(self, 0x8C0));
    constructShadowTree(lmField<ShadowTree>(self, 0x8CC));
    for (unsigned int offset = 0x8D8; offset <= 0x904; offset += 4)
        lmField<void*>(self, offset) = 0;
    lmField<void*>(self, 0x90C) = 0;
    lmField<unsigned int>(self, 0x910) = 2;
    constructEmptyVector(self, 0x914);
    constructEmptyVector(self, 0x924);
    constructEmptyVector(self, 0x934);
    constructEmptyVector(self, 0x944);
    constructEmptyVector(self, 0x954);
    constructShadowTree(lmField<ShadowTree>(self, 0x964));
    constructEmptyVector(self, 0x970);

    IDirect3DDevice9* const device = *reinterpret_cast<IDirect3DDevice9**>(
        static_cast<unsigned char*>(dx9rend::g_rendererBase) + 0x0C);
    lmField<IDirect3DDevice9*>(self, 0x908) = device;
    device->AddRef();

    const Mat4 identity = Mat4::getIdentity();
    lmField<Frustum>(self, 0x278).setOrtho(1.0f, 1.0f, 0.0f, 1.0f, identity);
    setIdentityMatrix(self, 0x708);

    if (dx9rend::deviceCapsIsNvidia())
    {
        lmField<unsigned int>(self, 0x910) = 0;
        lmField<unsigned int>(self, 0x210) = D3DUSAGE_DEPTHSTENCIL;
        lmField<unsigned int>(self, 0x20C) = D3DFMT_D24X8;
    }
    else
    {
        lmField<unsigned int>(self, 0x210) = D3DUSAGE_RENDERTARGET;
        const unsigned int formats[] = { 111u, 114u, 112u };
        for (unsigned int i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i)
        {
            if (!dx9rend::deviceCapsSupportsTextureFormat(formats[i]))
                continue;
            lmField<unsigned int>(self, 0x20C) = formats[i];
            lmField<unsigned int>(self, 0x910) = 1;
            break;
        }
    }

    const Vec3 white = { 1.0f, 1.0f, 1.0f };
    lmField<dx9rend::EngineTexture*>(self, 0x254) = createPowerLut(256, white, 64.0f, false);
    lmField<dx9rend::EngineTexture*>(self, 0x258) = createPowerLut(256, white, 64.0f, false);
    lmField<dx9rend::EngineTexture*>(self, 0x25C) = createPowerLut(256, white, 64.0f, false);
    lmField<dx9rend::EngineTexture*>(self, 0x260) = createPowerLut(256, white, 64.0f, false);
    lmField<unsigned char>(self, 0x7EC) =
        lmField<unsigned int>(self, 0x910) != 2 ? 1 : 0;

    dx9rend::registerDeviceLostResetListener(
        reinterpret_cast<dx9rend::DeviceLostResetListener*>(
            static_cast<unsigned char*>(self) + 4), 4);
    dx9rend::registerRenderPathListener(
        reinterpret_cast<dx9rend::RenderPathListener*>(
            static_cast<unsigned char*>(self) + 8));
    createSmallLightTextures(self);
    reserveEmptyVector(lmField<vc71::vector_layout>(self, 0x79C), 150, 0x498);

    ::renddx9abi::logf("LightManager: built at 0x%08X (retail size 0x980); lifecycle, shadow "
                       "resources and outer shadow transaction recovered",
                       ::renddx9abi::addr(self));
}

} // anonymous namespace

bool createShadowResources(LightManager* manager)
{
    return createShadowResourcesBody(manager);
}

void initializeLightManager(LightManager* manager)
{
    initializeLightManagerBody(manager);
}

void LightManager::loadSimpleShadowMaps()
{
    loadSimpleShadowMapsBody(this);
}

void prepareLightFrame(double time)
{
    // reset transient grouping state, update all wrappers and both capped
    // collectors, emit geometry->LightSource pairs into coordinator +0xA4, then call 0x10193F40
    // on the separate helper at +0xB0. Full PE xref/raw inspection proves the helper's own +0x04
    // multimap has no reachable producer on this path; the finalizer observes size zero and only
    // clears its dirty byte. It does not consume or clear the +0xA4 pairs built here.
    g_xpCoordinator.geometryPairs.clear();
    updateXpCoordinator(time);
    buildXpGeometryPairs();
}

IBase* __fastcall createLightManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    LightManager* const self = static_cast<LightManager*>(
        ::renddx9abi::engineAlloc(kLightManagerSize));
    if (self)
        constructLightManager(self);
    void* const itf = self->LightManager::queryInterface(iid);
    if (!itf)
        self->LightManager::release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
