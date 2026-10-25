// Texture loading, format/mip selection and named texture/atlas caches.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../Core/EventManager.h"
#if BF2_PLUGIN_ABI_NATIVE
#include "../Menu/BF2HudManager.h"    // atlasCacheLoadAtlases: getAtlasList (+0x374), typed (RW-266)
#endif
#include "RendDX9Base.h"
#include "Texture.h"
#include "TextureAtlas/TextureAtlas.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <string.h>
#include <list>
#include <new>


// D3DXGetImageInfoFromFileInMemory - statically imported from d3dx9_25.dll exactly as the shipped
// image imports it (the call at 0x10018842 goes through the import thunk). Same decoration dance as
// D3DXCreateEffect: the DLL exports the undecorated name, the compiler references the __stdcall
// decorated one, and /alternatename bridges them. Three 4-byte parameters -> @12.

// D3DXIMAGE_INFO is seven dwords - Width, Height, Depth, MipLevels, Format, ResourceType,
// ImageFileFormat - confirmed by the 28-byte stack slot and the `rep movsd` of 7 at 0x1001885B.

struct D3DXImageInfo
{
    unsigned int width;          // +0x00
    unsigned int height;         // +0x04
    unsigned int depth;          // +0x08
    unsigned int mipLevels;      // +0x0C
    unsigned int format;         // +0x10  D3DFORMAT
    unsigned int resourceType;   // +0x14  D3DRESOURCETYPE: 3 = texture, 4 = volume, 5 = cube
    unsigned int imageFileFormat;// +0x18  D3DXIMAGE_FILEFORMAT: 2 = TGA, 4 = DDS
};

extern "C" long __stdcall D3DXGetImageInfoFromFileInMemory(const void* srcData,
                                                           unsigned int srcDataSize,
                                                           D3DXImageInfo* srcInfo);
#pragma comment(linker, "/alternatename:_D3DXGetImageInfoFromFileInMemory@12=_D3DXGetImageInfoFromFileInMemory")

// D3DXSaveTextureToFileA - textureManagerSaveTexture's (sub_10017F90) call, through the import thunk
// (IAT 0x101DB4BC). Four 4-byte parameters -> @16; the same bridge as above.
extern "C" long __stdcall D3DXSaveTextureToFileA(const char* destinationFile, unsigned int fileFormat,
                                                 IDirect3DBaseTexture9* source,
                                                 const PALETTEENTRY* sourcePalette);
#pragma comment(linker, "/alternatename:_D3DXSaveTextureToFileA@16=_D3DXSaveTextureToFileA")

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_rendererBase;      // dword_1023B970 - RendDX9Init.cpp
extern void* g_resourceManager;   // dword_1023B988 - ResourceManager.cpp

// dword_1023B974. VideoOptions::apply and the step-16 sweep both load it directly.
void* g_textureManager = 0;

// dword_10249EF0 - the shared menu-atlas cache, lazily created by Meme::Texture::setTexture and
// held by every Meme texture that follows.
static void* g_atlasCache = 0;


// The shipped path/string helpers. All four bodies read in full; they live at 0x10074xxx/0x10075xxx
// in an unattested utility TU, so they are defined here (their heaviest user) and declared in
// RendDX9Base.h for the atlas and Meme translation units.


// sub_10074D10 - lowercase IN place: five hardcoded Latin-1 maps, then CRT tolower. The special
// cases exist because MSVCR71's tolower under the "C" locale leaves high-bit characters alone:
//     0xC5 Å -> 0xE5 å      0xC4 Ä -> 0xE4 ä      0xD6 Ö -> 0xF6 ö
//     0xDC Ü -> 0xFC ü      0xC9 É -> 0xE9 é
void pathToLower(vc71::string& s)
{
    char* p = const_cast<char*>(s.c_str());
    const unsigned int n = s.size();
    for (unsigned int i = 0; i < n; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(p[i]);
        if      (c == 0xC5) p[i] = static_cast<char>(0xE5);
        else if (c == 0xC4) p[i] = static_cast<char>(0xE4);
        else if (c == 0xD6) p[i] = static_cast<char>(0xF6);
        else if (c == 0xDC) p[i] = static_cast<char>(0xFC);
        else if (c == 0xC9) p[i] = static_cast<char>(0xE9);
        else                p[i] = static_cast<char>(::tolower(c));
    }
}

// The substring builder the two helpers below need - see RendDX9Base.h for why it exists.
void pathAssignSubstring(vc71::string& out, const char* s, unsigned int len)
{
    char* tmp = static_cast<char*>(::renddx9abi::engineAlloc(len + 1));
    if (!tmp)
    {
        out = "";
        return;
    }
    ::memcpy(tmp, s, len);
    tmp[len] = '\0';
    out = tmp;
    ::renddx9abi::engineFree(tmp);
}

// sub_10075730 (and its thunk sub_100751C0) - every '/' OR '\\' becomes `with`.
void pathReplaceSeparators(vc71::string& s, char with)
{
    char* p = const_cast<char*>(s.c_str());
    const unsigned int n = s.size();
    for (unsigned int i = 0; i < n; ++i)
        if (p[i] == '/' || p[i] == '\\')
            p[i] = with;
}

// sub_100753D0 -> sub_10075A50 - normalise separators to '/', then cut at the last '.'. A name with
// no '.' passes through whole (separators still normalised).
void pathStripExtension(vc71::string& out, const vc71::string& in)
{
    vc71::string work(in);
    pathReplaceSeparators(work, '/');

    const char* s = work.c_str();
    int dot = -1;
    for (int i = static_cast<int>(work.size()) - 1; i >= 0; --i)
        if (s[i] == '.') { dot = i; break; }

    if (dot < 0)
        out = work;
    else
        pathAssignSubstring(out, s, static_cast<unsigned int>(dot));
}

// sub_10075180 -> sub_10075580 - the substring after the last '.'. Empty when there is no '.', and
// empty when the character before the last '.' is also '.' (the ".." path-segment guard, with the
// shipped `pos > 1` bound reproduced as-is).
void pathGetExtension(vc71::string& out, const vc71::string& in)
{
    const char* s = in.c_str();
    int dot = -1;
    for (int i = static_cast<int>(in.size()) - 1; i >= 0; --i)
        if (s[i] == '.') { dot = i; break; }

    if (dot < 0 || (dot > 1 && s[dot - 1] == '.'))
    {
        out = "";
        return;
    }
    pathAssignSubstring(out, s + dot + 1, in.size() - static_cast<unsigned int>(dot) - 1);
}

namespace {

// FileManager vslot +0x1C - `unsigned char exists(const std::string&)`, read from the two probes in
// Distinct from the stream's +0x1C (getAvailableBytes) - different objects.
// in both flavours through HostFileAbi.h, whose two forms both put
// fileExists at +0x1C (the native IFileManager, and the retail mirror with HostString carriers).
bool fileManagerExists(const vc71::string& name)
{
    if (!::dice::hfe::fileManager)
        return false;
    renddx9abi::HostString hostName = renddx9abi::makeHostString(name);
    return ::dice::hfe::fileManager->fileExists(hostName);                         // +0x1C
}

// One entry of the mip table. Eight bytes: a flag, three of padding, and a value.
struct MipClassRecord
{
    unsigned char enabled;   // +0
    unsigned char pad[3];
    int           value;     // +4
};

static_assert(sizeof(MipClassRecord) == 8, "the constructor strides this array by 8");

} // anonymous namespace

// Reference-counted texture cache and device-loss listener.
// Owns a reference to the D3D device. Per-class mip settings control subsequent loads.
// The managed-texture setting is passed as the D3DPOOL value; originLabel and loadingFileName
// describe the load in progress, and fileSuffix selects an optional named DDS variant.

class TextureManager
{
public:
    explicit TextureManager(IDirect3DDevice9* device);

    bool init();     // sub_10017BA0, RendDX9Init.cpp:272

    // The loader makes its own name copy; the binary entry point takes the string by value.
    EngineTexture* getTexture(const vc71::string& name);

    // TextureManager_loadTextureFromFile, 0x10018580.
    EngineTexture* loadTextureFromFile(const vc71::string& name, EngineTexture* tex,
                                       bool skipChangeWatch, bool useFallback);

    // sub_10017E70 - resolve `name` to an openable path: as-is if it has an extension and exists,
    // else "<base>.dds", else "<base>" bare, else empty.
    void findTexturePath(vc71::string& out, const vc71::string& name);

    // The accessor family the Texture constructor and the consumers use.
    int           defaultMipClass() const        { return m_defaultMipClass; }
    int           swapDefaultMipClass(int v)     { const int o = m_defaultMipClass; m_defaultMipClass = v; return o; }
    int           managedFlag() const            { return m_managedTextures; }
    int           swapManaged(int v)             { const int o = m_managedTextures; m_managedTextures = v; return o; }
    unsigned char recreateMode() const           { return m_recreateFromStoredParams; }
    const vc71::string& originLabel() const      { return m_originLabel; }

    // The two walks of sub_10018FD0 - see textureManagerSweep for the orchestration and the two
    // ECCore load-progress events posted between them.
    unsigned sweepPass1();   // destroy cache-only textures, count the non-resident survivors
    void     sweepPass2();   // re-upload every survivor that is still not resident

    // sub_10018C50 - re-upload one texture (or, with tex null, look it up by name first).
    bool reupload(const vc71::string& name, EngineTexture* tex);
    void refreshFileSuffix();

    // sub_10018550 - the code-created-texture entry (a thunk onto the 2D create with depth 1).
    // Meme::Texture's blank render targets and its onDeviceReset re-creation both come through it.
    EngineTexture* createEmpty2D(unsigned int w, unsigned int h, unsigned int format,
                                 unsigned int mips, unsigned int usage, EngineTexture* existing)
    {
        return create2DOrVolumeTexture(w, h, 1, format, mips, usage, existing);
    }

    EngineTexture* createEmptyCube(unsigned int side, unsigned int format,
                                   unsigned int mips, unsigned int usage,
                                   EngineTexture* existing)
    {
        return createCubeTexture(side, format, mips, usage, existing);
    }

    // sub_10017E40 - the third swap-setter: TM+0x24, the recreate-from-stored-params byte every
    // new Texture snapshots. Meme's createTexture swaps 1 in so its render targets are marked for
    // device-reset re-creation.
    unsigned char swapRecreateMode(unsigned char v)
    {
        const unsigned char old = m_recreateFromStoredParams;
        m_recreateFromStoredParams = v;
        return old;
    }

    // GetRenderTargetData(source, destination) on the manager's device; a failure is logged and
    // answered false.
    bool copyRenderTargetToSurface(IDirect3DSurface9* source, IDirect3DSurface9* destination)
    {
        if (m_device->GetRenderTargetData(source, destination) >= 0)
            return true;
        ::renddx9abi::logf("TextureManager.cpp:594: Failed to copy pRenderTarget to pDestSurface.");
        return false;
    }

    // sub_100191C0 - evict BY value: find the cache node holding `tex`, release the cache's ref
    // (destroying the texture if that was the last), erase the node. Meme's release paths call it
    // when their own decrement left other holders.
    void evictFromCache(EngineTexture* tex)
    {
        for (vc71::map_node* node = vc71::mapFirst(m_textures); node;
             node = vc71::mapNext(m_textures, node))
        {
            if (vc71::mapNodeValue(node) == tex)
            {
                if (!--tex->refCount)
                {
                    textureDtor(tex);
                    ::renddx9abi::engineFree(tex);
                }
                vc71::mapErase(m_textures, node);
                return;
            }
        }
    }

    unsigned cacheSize() const { return m_textures.size; }

    void addRef() { ++m_refCount; }
    int  getRef() const { return m_refCount; }

    // Device listener methods preserve cached wrappers across reset.
    void onDeviceLost();
    void onDeviceReset();
    void onDeviceDestroy();

private:
    EngineTexture* create2DOrVolumeTexture(unsigned int w, unsigned int h, unsigned int depth,
                                           unsigned int format, unsigned int mips,
                                           unsigned int usage, EngineTexture* tex);
    EngineTexture* createCubeTexture(unsigned int w, unsigned int format, unsigned int mips,
                                     unsigned int usage, EngineTexture* tex);
    bool seekPastHeaderAndSkippedMips(::renddx9abi::EngineStream& stream,
                                      D3DXImageInfo& info, int mipSkip);

    void*             m_vptr;             // +0x00
    void*             m_listenerVptr;     // +0x04
    int               m_refCount;         // +0x08
    IDirect3DDevice9* m_device;           // +0x0C
    vc71::map_layout  m_textures;         // +0x10
    int               m_smallestMipSide;  // +0x1C
    int               m_managedTextures;  // +0x20
    unsigned char     m_recreateFromStoredParams; // +0x24
    unsigned char     m_pad25[3];
    MipClassRecord    m_mipClasses[14];   // +0x28 .. +0x97
    int               m_defaultMipClass;  // +0x98
    vc71::string      m_originLabel;      // +0x9C  "Created from code." at rest
    unsigned char     m_detailedInfo;     // +0xB8  GSLoadedTextureDetailedInfo
    unsigned char     m_padB9[3];
    vc71::string      m_fileSuffix;       // +0xBC  the "<base>_<suffix>.dds" probe
    unsigned char     m_byteD8;           // +0xD8, pending file-suffix refresh
    unsigned char     m_padD9[3];
    vc71::string      m_loadingFileName;  // +0xDC  the file mid-load
};

static_assert(sizeof(TextureManager) == 0xF8,
              "RendDX9Init.cpp:267 allocates 248 - see the bf_malloc at 0x1000B4E9");

namespace {


// The two vtables. MSVC will not let a free function be __thiscall, so each slot is __fastcall with
// an unused EDX - the same convention for slots whose arguments are all stack-passed after `this`.


void __fastcall tm_addRef(void* self, void*) { static_cast<TextureManager*>(self)->addRef(); }
int  __fastcall tm_getRef(void* self, void*) { return static_cast<TextureManager*>(self)->getRef(); }

// +0x0C, 0x10025930. A comdat-folded body shared with SkinnedMeshRenderer, which is why the
// database names it after that class - the same three instructions serve both. It accepts exactly
// one interface id: `return iid != 1 ? 0 : this`, so IID_IBase and nothing else.
void* __fastcall tm_queryInterface(void* self, void*, unsigned int iid)
{
    return iid == RD9_IID_IBASE ? self : 0;
}

#define RD9_TM_SLOT(name, qualified, slotOffset)                                    \
    void __fastcall name(void* self, void*)                                         \
    {                                                                               \
        RD9_FRONTIER_AT("dice::hfe::dx9rend::TextureManager::" qualified,            \
                        self, slotOffset);                                          \
    }

RD9_TM_SLOT(tm_release,   "release (0x1001A020)",              0x08)
RD9_TM_SLOT(tm_slot10,    "slot_0x10 (0x10018F10)",            0x10)
RD9_TM_SLOT(tm_slot14,    "slot_0x14 (0x1001A040)",            0x14)

#undef RD9_TM_SLOT

// Recovers the texture manager from its listener subobject at +0x04.
inline TextureManager* tmFromListener(void* listener)
{
    return reinterpret_cast<TextureManager*>(static_cast<unsigned char*>(listener) - 4);
}

void __fastcall tm_onDeviceLost(void* listener, void*)   { tmFromListener(listener)->onDeviceLost(); }
void __fastcall tm_onDeviceReset(void* listener, void*)  { tmFromListener(listener)->onDeviceReset(); }
void __fastcall tm_listenerSlot08(void* listener, void*) { tmFromListener(listener)->onDeviceDestroy(); }

void* const kTextureManagerVtable[6] =
{
    reinterpret_cast<void*>(&tm_addRef),          // +0x00
    reinterpret_cast<void*>(&tm_getRef),          // +0x04
    reinterpret_cast<void*>(&tm_release),         // +0x08
    reinterpret_cast<void*>(&tm_queryInterface),  // +0x0C
    reinterpret_cast<void*>(&tm_slot10),          // +0x10
    reinterpret_cast<void*>(&tm_slot14)           // +0x14
};

void* const kTextureManagerListenerVtable[3] =
{
    reinterpret_cast<void*>(&tm_onDeviceLost),    // +0x00
    reinterpret_cast<void*>(&tm_onDeviceReset),   // +0x04
    reinterpret_cast<void*>(&tm_listenerSlot08)   // +0x08
};

IDirect3DDevice9* rendererDevice()
{
    RendDX9Base* b = reinterpret_cast<RendDX9Base*>(g_rendererBase);
    return b ? b->device : 0;
}

// sub_1000D950 on dword_1023B988 - the byte at RM+0x04. See ResourceManager.cpp for the polarity.
bool resourceLoadingDeferred()
{
    return resourceManagerIsReady();
}

} // anonymous namespace


// Initializes mip policies, pinning UI class 1 to full resolution regardless of global mip skip.

TextureManager::TextureManager(IDirect3DDevice9* device)
    : m_vptr(const_cast<void**>(kTextureManagerVtable))
    , m_listenerVptr(const_cast<void**>(kTextureManagerListenerVtable))
    , m_refCount(1)
    , m_device(0)
    , m_smallestMipSide(4)
    , m_managedTextures(0)
    , m_recreateFromStoredParams(0)
    , m_defaultMipClass(0)
    , m_detailedInfo(0)
    , m_byteD8(0)
{
    m_pad25[0] = 0; m_pad25[1] = 0; m_pad25[2] = 0;
    m_padB9[0] = 0; m_padB9[1] = 0; m_padB9[2] = 0;
    m_padD9[0] = 0; m_padD9[1] = 0; m_padD9[2] = 0;

    m_textures.construct();          // sub_10019920 - the same 0x30 node as the GTM's maps

    for (int i = 0; i < 14; ++i)
    {
        m_mipClasses[i].enabled = 1;
        m_mipClasses[i].pad[0]  = 0;
        m_mipClasses[i].pad[1]  = 0;
        m_mipClasses[i].pad[2]  = 0;
        m_mipClasses[i].value   = 0;
    }

    // The device is AddRef'd before being stored, and the store is the shipped order.
    if (device)
        device->AddRef();
    m_device = device;

    m_detailedInfo    = readBoolSetting("GSLoadedTextureDetailedInfo", false) ? 1 : 0;
    m_managedTextures = readBoolSetting("GSManagedTextures", true) ? 1 : 0;

    // +0x30 - record 1's enabled flag, cleared after the loop set it. See the note above: class 1
    // is the UI class and its mip skip is pinned at zero by this store.
    m_mipClasses[1].enabled = 0;

    m_smallestMipSide = 4;
    m_originLabel = "Created from code.";

    ::renddx9abi::logf("TextureManager: built at 0x%08X - device AddRef'd, detailedInfo=%d "
                       "managedTextures=%d, 14 mip-class records (record 1, the UI class, pinned "
                       "off), empty texture cache",
                       ::renddx9abi::addr(this), m_detailedInfo, m_managedTextures);
}


// Loads GSSmallestMipSide, defaulting to 8, and returns true.
// This intentionally replaces the constructor's temporary default of 4.

bool TextureManager::init()
{
    m_smallestMipSide = readIntSetting("GSSmallestMipSide", 8);
    ::renddx9abi::logf("TextureManager::init: GSSmallestMipSide=%d (the constructor's 4 is "
                       "overwritten here on every boot)", m_smallestMipSide);
    return true;
}


// sub_10017E70 - findTexturePath, a member whose body touches only globals. In order:

//   1. if the name HAS an extension (sub_10075180) and exists as-is -> done
//      (when it has one and does NOT exist, the extension is stripped for the probes below)
//   2. try "<base>" + each of the TWO entries of off_10238058 - ".dds", then "" - first hit wins
//   3. nothing found -> empty string

void TextureManager::findTexturePath(vc71::string& out, const vc71::string& name)
{
    out = "";

    vc71::string base(name);

    vc71::string ext;
    pathGetExtension(ext, name);
    if (!ext.empty())
    {
        if (fileManagerExists(name))
        {
            out = name;
            return;
        }
        vc71::string stripped;
        pathStripExtension(stripped, name);
        base = stripped;
    }

    static const char* const kProbeExtensions[2] = { ".dds", "" };
    for (int i = 0; i < 2; ++i)
    {
        vc71::string candidate(base);
        candidate += kProbeExtensions[i];
        if (fileManagerExists(candidate))
        {
            out = candidate;
            return;
        }
    }
}


// Seeks past TGA/DDS headers and skipped top mips, updating info dimensions and mip count.
// Minimum side and remaining-mip limits stop skipping. Unsupported formats return false
// without moving the current stream position.

bool TextureManager::seekPastHeaderAndSkippedMips(::renddx9abi::EngineStream& stream,
                                                  D3DXImageInfo& info, int mipSkip)
{
    if (info.imageFileFormat == 2)                         // TGA
    {
        const int bpp = bitsPerPixel(info.format);
        int offset = 18;
        if (info.width >= static_cast<unsigned int>(m_smallestMipSide))
        {
            int remaining = mipSkip;
            for (;;)
            {
                const int next = remaining - 1;
                if (!remaining)
                    break;
                if (info.mipLevels <= 1)
                    break;
                offset += static_cast<int>(bpp * info.width * info.height) >> 3;
                const unsigned int w = info.width >> 1;
                info.height >>= 1;
                info.width = w;
                --info.mipLevels;
                if (w < static_cast<unsigned int>(m_smallestMipSide))
                    break;
                remaining = next;
            }
        }
        stream.seek(0, offset);
        return true;
    }

    if (info.imageFileFormat == 4)                         // DDS
    {
        const int bpp = bitsPerPixel(info.format);
        int offset = 128;

        if (info.resourceType == 3)                        // 2D only
        {
            const unsigned int fmt = info.format;
            if (fmt == 827611204 || fmt == 861165636 || fmt == 894720068)   // DXT1 / DXT3 / DXT5
            {
                const unsigned int mips = info.mipLevels;
                int w = static_cast<int>(info.width);
                int h = static_cast<int>(info.height);
                const int lastW = (static_cast<int>(info.width)  >> (mips - 1) <= 0)
                                      ? 0 : static_cast<int>(info.width)  >> (mips - 1);
                const int lastH = (static_cast<int>(info.height) >> (mips - 1) <= 0)
                                      ? 0 : static_cast<int>(info.height) >> (mips - 1);
                if (mips > 1)
                {
                    do
                    {
                        w >>= 1;
                        h >>= 1;
                        if ((!w && !h) || (w < lastW && h < lastH))
                            break;
                        const int smaller = (w >= h) ? h : w;
                        if (smaller < m_smallestMipSide)
                            --info.mipLevels;
                    }
                    while (info.mipLevels > 1);
                }
            }

            if (mipSkip)
            {
                int remaining = mipSkip;
                do
                {
                    --remaining;
                    if (info.mipLevels <= 1)
                        break;
                    const unsigned int w = info.width;
                    if (w == 1)
                        break;
                    const unsigned int h = info.height;
                    if (h == 1)
                        break;
                    const unsigned int smallest = static_cast<unsigned int>(m_smallestMipSide);
                    if ((w >> 1) < smallest)
                        break;
                    if ((h >> 1) < smallest)
                        break;
                    offset += static_cast<int>(bpp * w * h) >> 3;
                    info.width  = w >> 1;
                    info.height = h >> 1;
                    --info.mipLevels;
                }
                while (remaining);
            }
        }

        stream.seek(0, offset);
        return true;
    }

    return false;
}


// Creates or recreates a 2D/volume wrapper, using stored parameters for recreation.
// OOM permanently switches to managed storage before retry; repeated OOM reaches the
// unchecked attach path. Other creation failures return null.

EngineTexture* TextureManager::create2DOrVolumeTexture(unsigned int w, unsigned int h,
                                                       unsigned int depth, unsigned int format,
                                                       unsigned int mips, unsigned int usage,
                                                       EngineTexture* tex)
{
    IDirect3DTexture9*       tex2D  = 0;
    IDirect3DVolumeTexture9* texVol = 0;
    HRESULT hr;

    for (;;)
    {
        tex2D = 0;
        texVol = 0;

        if (tex && tex->recreateFromStored)
        {
            // The device-reset arm: every parameter from the wrapper, usage from the caller.
            if (tex->depth <= 1)
                hr = m_device->CreateTexture(tex->width, tex->height, tex->mipLevels, usage,
                                             static_cast<D3DFORMAT>(tex->format),
                                             static_cast<D3DPOOL>(tex->pool), &tex2D, 0);
            else
                hr = m_device->CreateVolumeTexture(tex->width, tex->height, tex->depth,
                                                   tex->mipLevels, usage,
                                                   static_cast<D3DFORMAT>(tex->format),
                                                   static_cast<D3DPOOL>(tex->pool), &texVol, 0);
        }
        else
        {
            if (static_cast<int>(depth) <= 1)
                hr = m_device->CreateTexture(w, h, mips, usage, static_cast<D3DFORMAT>(format),
                                             static_cast<D3DPOOL>(m_managedTextures), &tex2D, 0);
            else
                hr = m_device->CreateVolumeTexture(w, h, depth, mips, usage,
                                                   static_cast<D3DFORMAT>(format),
                                                   static_cast<D3DPOOL>(m_managedTextures),
                                                   &texVol, 0);
        }

        if (SUCCEEDED(hr))
            break;

        if (hr != D3DERR_OUTOFVIDEOMEMORY && hr != E_OUTOFMEMORY)
        {
            // The non-OOM failure arm. Only R8G8B8 gets a diagnostic; everything else fails silent.
            if (format == 20)
                ::renddx9abi::logf("TextureManager.cpp:460: Texture %s is a 24-bit texture, this "
                                   "format is not supported. Please fix content.",
                                   m_originLabel.c_str());
            return 0;
        }

        if (m_managedTextures == 1)
        {
            // TextureManager.cpp:453 - "Failed to create dx texture even after going managed."
            ::renddx9abi::logf("TextureManager.cpp:453: Failed to create dx texture even after "
                               "going managed: hr=0x%08X", static_cast<unsigned int>(hr));
            break;   // fall through to the attach block with tex2D == texVol == 0, as shipped
        }

        m_managedTextures = 1;
        if (tex)
            tex->pool = 1;
        // and retry
    }

    if (!tex)
    {
        void* storage = ::renddx9abi::engineAlloc(0x7C);
        tex = storage ? textureCtor(storage) : 0;
    }

    unsigned int desc[7];
    desc[0] = w; desc[1] = h; desc[2] = depth; desc[3] = mips; desc[4] = format;
    desc[6] = 0x7FFFFFFF;

    IDirect3DBaseTexture9* base = 0;
    if (tex2D)
    {
        desc[5] = 3;                                       // D3DRTYPE_TEXTURE
        tex2D->QueryInterface(kIID_BaseTexture9, reinterpret_cast<void**>(&base));
        textureAttach(tex, base, desc);

        D3DSURFACE_DESC sd;
        tex2D->GetLevelDesc(0, &sd);
        if (sd.Pool == D3DPOOL_MANAGED && tex->priority >= 15)
            base->SetPriority(static_cast<DWORD>(tex->priority));

        tex2D->Release();
        base->Release();
        return tex;
    }

    // the volume arm - and, after a double-OOM give-up, the arm the shipped body reaches with
    // texVol NULL, where this QI dereferences it. See the function comment.
    desc[5] = 4;                                           // D3DRTYPE_VOLUMETEXTURE
    texVol->QueryInterface(kIID_BaseTexture9, reinterpret_cast<void**>(&base));
    textureAttach(tex, base, desc);
    texVol->Release();
    base->Release();
    return tex;
}


// TextureManager_createCubeTexture, read in full. Simpler than the 2D path: no OOM
// retry, no recreate arm, no SetPriority.
//     CreateCubeTexture(EdgeLength, Levels, Usage, Format, m_managedTextures, &cube, 0) - slot 25
//     failure: TextureManager.cpp:535 "Failed to create dx texture." and return 0
//     success: desc = {W, W, 1, Mips, Format, 5, 0x7FFFFFFF}, QI + attach, release both, return

EngineTexture* TextureManager::createCubeTexture(unsigned int w, unsigned int format,
                                                 unsigned int mips, unsigned int usage,
                                                 EngineTexture* tex)
{
    if (!tex)
    {
        void* storage = ::renddx9abi::engineAlloc(0x7C);
        tex = storage ? textureCtor(storage) : 0;
    }

    IDirect3DCubeTexture9* cube = 0;
    const HRESULT hr = m_device->CreateCubeTexture(w, mips, usage,
                                                   static_cast<D3DFORMAT>(format),
                                                   static_cast<D3DPOOL>(m_managedTextures),
                                                   &cube, 0);
    if (FAILED(hr))
    {
        ::renddx9abi::logf("TextureManager.cpp:535: Failed to create dx texture. hr=0x%08X",
                           static_cast<unsigned int>(hr));
        return 0;
    }

    unsigned int desc[7];
    desc[0] = w; desc[1] = w; desc[2] = 1; desc[3] = mips; desc[4] = format;
    desc[5] = 5;                                           // D3DRTYPE_CUBETEXTURE
    desc[6] = 0x7FFFFFFF;

    IDirect3DBaseTexture9* base = 0;
    cube->QueryInterface(kIID_BaseTexture9, reinterpret_cast<void**>(&base));
    textureAttach(tex, base, desc);
    cube->Release();
    base->Release();
    return tex;
}


// Loads a named texture, optionally replacing tex and using the missing-texture fallback.
// Searches global then level-local paths; without fallback, a missing path returns null.
// The selected file must remain available while loading. tex can supply its pool/mip policy.
// [FRONTIER] TextureManager file-change watching remains withheld.
// [RETIRE-WHEN: RW-78] Implement and verify the notification/reload slots before registering
// this owner with FileChangeManager.

EngineTexture* TextureManager::loadTextureFromFile(const vc71::string& name, EngineTexture* tex,
                                                   bool skipChangeWatch, bool useFallback)
{
    vc71::string path;
    findTexturePath(path, name);

    vc71::string prefixedName(name);

    if (path.empty())
    {
        vc71::string level, levelDir;
        readStringSetting("GSLoadLevel", level);
        readStringSetting("GSLevelDirectory", levelDir);

        vc71::string prefix(levelDir);
        prefix += level;
        prefix += "\\";

        vc71::string prefixed(prefix);
        prefixed += prefixedName;
        prefixedName = prefixed;

        findTexturePath(path, prefixedName);

        if (path.empty())
        {
            if (!useFallback)
                return 0;

            // TextureManager.cpp:761, severity 6.
            ::renddx9abi::logf("TextureManager.cpp:761: Texture not found: %s", name.c_str());
            path = "common/textures/wantu.dds";
        }
    }

    if (!skipChangeWatch)
    {
        // fileChangeManager->vslot+0x10(sret, this, path, 8) - see the function comment. Withheld.
    }

    m_loadingFileName = prefixedName;

    ::renddx9abi::EngineStream stream;
    stream.open(path);   // shipped: no null check on the returned stream

    D3DXImageInfo info;
    ::memset(&info, 0, sizeof(info));
    {
        void* probe = ::renddx9abi::engineAlloc(0x7C);
        stream.read(probe, 0x7C);
        D3DXGetImageInfoFromFileInMemory(probe, static_cast<unsigned int>(stream.getSize()), &info);
        ::renddx9abi::engineFree(probe);
    }

    const D3DXImageInfo originalInfo = info;               // the rep-movsd copy at 0x1001885B keeps
                                                           // the original for the +0x0C stamp

    int mipSkip = 0;
    {
        int cls;
        if (tex && (cls = tex->mipClassIndex, m_mipClasses[cls].enabled))
            mipSkip = m_mipClasses[cls].value;
        else if (cls = m_defaultMipClass, m_mipClasses[cls].enabled)
            mipSkip = m_mipClasses[cls].value;
    }
    seekPastHeaderAndSkippedMips(stream, info, mipSkip);   // return value ignored, as shipped

    int savedManaged = 0;
    if (tex)
    {
        savedManaged = m_managedTextures;
        m_managedTextures = tex->pool;
    }

    EngineTexture* result = 0;

    if (info.resourceType == 3)
    {
        const unsigned int fmt = info.format;
        result = create2DOrVolumeTexture(info.width, info.height, 1, fmt, info.mipLevels, 0, tex);
        const int bpp = bitsPerPixel(fmt);
        for (unsigned int mip = 0; mip < info.mipLevels; ++mip)
        {
            int w = static_cast<int>(info.width >> mip);  if (w < 1) w = 1;
            int h = static_cast<int>(info.height >> mip); if (h < 1) h = 1;
            if (w >= m_smallestMipSide && h >= m_smallestMipSide)
            {
                TextureLockRecord* rec = textureLock(result, mip, 0x7FFFFFFF);
                stream.read(rec->pBits, static_cast<unsigned int>(bpp * w * h / 8));
                textureUnlock(result, rec);
            }
        }
    }
    else if (info.resourceType == 4)
    {
        const unsigned int fmt = info.format;
        result = create2DOrVolumeTexture(info.width, info.height, info.depth, fmt,
                                         info.mipLevels, 0, tex);
        const int bpp = bitsPerPixel(fmt);
        for (unsigned int mip = 0; mip < info.mipLevels; ++mip)
        {
            int w = static_cast<int>(info.width >> mip);  if (w < 1) w = 1;
            int h = static_cast<int>(info.height >> mip); if (h < 1) h = 1;
            int d = static_cast<int>(info.depth >> mip);  if (d < 1) d = 1;
            TextureLockRecord* rec = textureLock(result, mip, 0x7FFFFFFF);
            stream.read(rec->pBits, static_cast<unsigned int>(bpp * w * h * d / 8));
            textureUnlock(result, rec);
        }
    }
    else
    {
        const unsigned int originalMips = info.mipLevels;
        if (deviceCapsPixelShaderMajor() < 2)              // sub_100010D0 on dword_1023B980
            info.mipLevels = 1;
        const unsigned int fmt = info.format;
        result = createCubeTexture(info.width, fmt, info.mipLevels, 0, tex);
        const int bpp = bitsPerPixel(fmt);
        for (int face = 0; face < 6; ++face)
        {
            for (unsigned int mip = 0; mip < originalMips; ++mip)
            {
                unsigned int edge = info.width >> mip;
                if (static_cast<int>(edge) < 4)
                    edge = 4;
                const int bytes = static_cast<int>(bpp * edge * edge) / 8;
                if (mip < info.mipLevels)
                {
                    TextureLockRecord* rec = textureLock(result, mip, static_cast<unsigned int>(face));
                    stream.read(rec->pBits, static_cast<unsigned int>(bytes));
                    textureUnlock(result, rec);
                }
                else
                {
                    stream.seek(1, bytes);                 // vslot +0x30 mode 1 - relative
                }
            }
        }
    }

    if (tex)
        m_managedTextures = savedManaged;

    stream.close();                                        // vslot +0x08

    textureSetFileInfo(result, &originalInfo);             // sub_10014820 - the PRE-skip info
    m_loadingFileName = "";                                // string::clear

    ::renddx9abi::logf("TextureManager: loaded \"%s\" -> 0x%08X  %ux%ux%u fmt=%u type=%u mips=%u "
                       "(file had %u) pool=%d skip=%d",
                       path.c_str(), ::renddx9abi::addr(result),
                       info.width, info.height, info.depth, info.format, info.resourceType,
                       info.mipLevels, originalInfo.mipLevels,
                       result ? result->pool : -1, mipSkip);
    return result;
}


// Returns an owned texture reference using a normalized, extension-free cache key.
// May substitute the configured suffix. Resource initialization defers file loading until
// the cache sweep; new wrappers retain separate cache and caller references.

EngineTexture* TextureManager::getTexture(const vc71::string& nameIn)
{
    vc71::string name(nameIn);                             // the by-value parameter

    {
        const char* s = name.c_str();
        const char* hit = ::strstr(s, "@@ABSOLUTE@@");
        if (hit)
        {
            vc71::string tail(hit + 12);
            name = tail;                                   // erase(0, pos + 12)
        }
    }

    vc71::string key;
    pathStripExtension(key, name);                         // sub_100753D0 (separators + strip)
    pathToLower(key);

    vc71::map_node* node = vc71::mapFind(m_textures, key.c_str());
    if (node)
    {
        EngineTexture* cached =
            static_cast<EngineTexture*>(vc71::mapNodeValue(node));
        textureAddRef(cached);                             // vslot +0x08 on the hit
        return cached;
    }

    vc71::string loadName(key);
    bool usedSuffix = false;

    if (!m_fileSuffix.empty())                             // the +0xD0 _Mysize test
    {
        vc71::string probe;
        pathStripExtension(probe, name);
        probe += "_";
        probe += m_fileSuffix;
        probe += ".dds";

        vc71::string resolved;
        findTexturePath(resolved, probe);
        if (!resolved.empty())
        {
            loadName   = probe;
            usedSuffix = true;
        }
    }

    m_originLabel = name;

    void* storage = ::renddx9abi::engineAlloc(0x7C);
    EngineTexture* tex = storage ? textureCtor(storage) : 0;

    vc71::map_node* inserted = vc71::mapInsert(m_textures, 0x30, key.c_str());
    if (inserted)
        vc71::mapNodeValueRef(inserted) = tex;

    textureAddRef(tex);
    tex->usedSuffixedFile = usedSuffix ? 1 : 0;
    tex->priority = resourceManagerDefaultPriority();      // RM+0x10

    if (!resourceLoadingDeferred())                        // sub_1000D950 - RM+0x04 clear
    {
        tex = loadTextureFromFile(loadName, tex, false, true);
        // nullsub_1(tex) - a no-op in the original too
        m_originLabel = "Created from code.";
    }

    return tex;
}


// Removes textures held only by the cache and counts remaining nonresident textures.
// Advances to the successor before erasing each node; pass 2 loads deferred file data.

unsigned TextureManager::sweepPass1()
{
    unsigned notResident = 0;

    vc71::map_node* node = vc71::mapFirst(m_textures);
    while (node)
    {
        vc71::map_node* next = vc71::mapNext(m_textures, node);
        EngineTexture*  tex  = static_cast<EngineTexture*>(vc71::mapNodeValue(node));

        if (tex->refCount == 1)                            // the vslot +0x00 getRef call
        {
            if (!--tex->refCount)
            {
                textureDtor(tex);
                ::renddx9abi::engineFree(tex);
            }
            vc71::mapErase(m_textures, node);              // sub_100436D0, after the advance
        }
        else if (!tex->baseTexture)
        {
            ++notResident;
        }
        node = next;
    }
    return notResident;
}

// Pass 2: every survivor still without a D3D texture gets re-uploaded through sub_10018C50, keyed
// by the node's own cache key (the shipped call passes node+12).
void TextureManager::sweepPass2()
{
    for (vc71::map_node* node = vc71::mapFirst(m_textures); node;
         node = vc71::mapNext(m_textures, node))
    {
        EngineTexture* tex = static_cast<EngineTexture*>(vc71::mapNodeValue(node));
        if (!tex->baseTexture)
        {
            const vc71::string* key = reinterpret_cast<const vc71::string*>(node->value);
            reupload(*key, tex);
        }
    }
}


// Reloads a supplied texture or resolves its normalized name in the cache.
// Cache lookup can retry without the mod prefix; absolute-path misses fail.
// Preserves the existing file watch and allows fallback textures during reload.

bool TextureManager::reupload(const vc71::string& name, EngineTexture* tex)
{
    vc71::string key;
    pathStripExtension(key, name);
    pathToLower(key);

    if (!tex)
    {
        vc71::map_node* node = vc71::mapFind(m_textures, key.c_str());
        if (!node)
        {
            vc71::string modDir;
            readStringSetting("GSModDirectory", modDir);   // shipped default: "(unknown)"
            if (modDir.empty())
                modDir = "(unknown)";

            if (::strchr(key.c_str(), ':'))                // an absolute path - fail
                return false;
            if (!::strstr(key.c_str(), modDir.c_str()))
                return false;
            // the shipped strip is BY length, not by match position: substr(modDirLen, ...) -
            // it assumes the hit was a prefix and drops that many leading characters wherever the
            // match actually sat. Reproduced as-is.
            const unsigned int len = (modDir.size() < key.size()) ? modDir.size() : key.size();
            vc71::string stripped;
            pathAssignSubstring(stripped, key.c_str() + len, key.size() - len);
            key = stripped;

            node = vc71::mapFind(m_textures, key.c_str());
            if (!node)
                return false;
        }
        tex = static_cast<EngineTexture*>(vc71::mapNodeValue(node));
    }

    m_originLabel = name;

    vc71::string loadName(name);
    if (tex->usedSuffixedFile && !m_fileSuffix.empty())
    {
        vc71::string base;
        pathStripExtension(base, loadName);
        base += "_";
        base += m_fileSuffix;
        base += ".dds";
        loadName = base;
    }

    loadTextureFromFile(loadName, tex, true, true);
    m_originLabel = "Created from code.";
    return true;
}


// Releases cached default-pool D3D textures while retaining wrappers; managed textures survive.
// Code-created textures belong to their owners' listeners. Cached pointers must be nonnull.
void TextureManager::onDeviceLost()
{
    for (vc71::map_node* node = vc71::mapFirst(m_textures); node;
         node = vc71::mapNext(m_textures, node))
    {
        EngineTexture* const tex = static_cast<EngineTexture*>(vc71::mapNodeValue(node));
        if (tex->pool == 0)
            textureReleaseD3DResource(tex);
    }
}

// for every cached pool-0 texture, re-upload it from disk BY ITS cache KEY -
// `sub_10018C50(this, &node->key, 0)`: the null texture argument makes reupload look the wrapper up
// again by name (the node's own key, already normalised, so the find hits the same wrapper) and
// reload into it with skipChangeWatch = true / useFallback = true. The pool field is what the reload
// uses again, so the texture comes back in the default pool it was created in.
void TextureManager::onDeviceReset()
{
    for (vc71::map_node* node = vc71::mapFirst(m_textures); node;
         node = vc71::mapNext(m_textures, node))
    {
        EngineTexture* const tex = static_cast<EngineTexture*>(vc71::mapNodeValue(node));
        if (tex->pool == 0)
        {
            const vc71::string* key = reinterpret_cast<const vc71::string*>(node->value);
            reupload(*key, 0);                             // sub_10018C50(key, 0)
        }
    }
}

// Drops cache references during manager destruction and notifies surviving wrappers.
// Nodes are left with dangling values until manager teardown; do not reuse the cache afterward.
void TextureManager::onDeviceDestroy()
{
    for (vc71::map_node* node = vc71::mapFirst(m_textures); node;
         node = vc71::mapNext(m_textures, node))
    {
        EngineTexture* const tex = static_cast<EngineTexture*>(vc71::mapNodeValue(node));
        const int remaining = --tex->refCount;
        if (!remaining)
        {
            textureDtor(tex);
            ::renddx9abi::engineFree(tex);
        }
        if (remaining > 0)
        {
            EngineTexture* const survivor =
                static_cast<EngineTexture*>(vc71::mapNodeValue(node));   // re-read, as shipped
            typedef void (__fastcall* SlotFn)(void*, void*);
            reinterpret_cast<SlotFn>((*reinterpret_cast<void***>(survivor))[1])(survivor, 0);
        }
    }
}

// the expression initializeD3DSubsystems registers at 0x1000B5F7..0x1000B60C:
// `dword_1023B974 ? dword_1023B974 + 4 : 0` (the listener subobject). The null arm is shipped.
void* textureManagerDeviceLostListener()
{
    return g_textureManager ? static_cast<unsigned char*>(g_textureManager) + 4 : 0;
}

// PE 0x100196B0, reached through 0x10019890 after ResourceManager clears its defer flag.
// A failed suffixed probe reloads the unsuffixed file only when this wrapper used a suffix.
void TextureManager::refreshFileSuffix()
{
    if (!m_byteD8)
        return;
    for (vc71::map_node* node = vc71::mapFirst(m_textures); node;
         node = vc71::mapNext(m_textures, node))
    {
        EngineTexture* const texture = static_cast<EngineTexture*>(vc71::mapNodeValue(node));
        if (!texture)
            continue;
        const vc71::string name(vc71::mapNodeKey(node));
        TextureManager* const published = static_cast<TextureManager*>(g_textureManager);
        if (!m_fileSuffix.empty())
        {
            vc71::string suffixed;
            pathStripExtension(suffixed, name);
            suffixed += "_";
            suffixed += m_fileSuffix;
            suffixed += ".dds";
            if (published->loadTextureFromFile(suffixed, texture, false, false))
                texture->usedSuffixedFile = 1;
            else if (texture->usedSuffixedFile)
                published->loadTextureFromFile(name, texture, false, true);
        }
        else if (texture->usedSuffixedFile)
            published->loadTextureFromFile(name, texture, false, true);
    }
    m_byteD8 = 0;
}

void textureManagerRefreshFileSuffix()
{
    static_cast<TextureManager*>(g_textureManager)->refreshFileSuffix();
}


// Lazy menu-atlas cache holding parsed atlases and deduplicated paths.
// Its host HUD list is copied before load callbacks can mutate it.


namespace {

// 10017AB0, called by100175A5 before the first atlas loader callback. This is
// a local owning copy, never a published STL layout. The retail host view is
// decoded through its existing boundary type; native mode uses its real STL.
// Source operation std::list<std::string> copy; helper name/path split inferred.
std::list<renddx9abi::HostString> copyAtlasNames(const renddx9abi::HostStringList& source)
{
#if BF2_PLUGIN_ABI_NATIVE
    return source;
#else
    std::list<renddx9abi::HostString> result;
    for (renddx9abi::HostStringListNode* node = source.head->next;
         node != source.head; node = node->next)
        result.push_back(node->value);
    return result;
#endif
}

struct AtlasNameNode                                       // the VC7.1 list node: links + string
{
    AtlasNameNode* next;
    AtlasNameNode* prev;
    vc71::string   name;
};

struct AtlasCache
{
    void*          vptr;         // +0x00
    int            refCount;     // +0x04
    void*          vecAlloc;     // +0x08  unwritten by the shipped ctor
    TextureAtlas** vecFirst;     // +0x0C
    TextureAtlas** vecLast;      // +0x10
    TextureAtlas** vecEnd;       // +0x14
    unsigned char  hasAtlases;   // +0x18
    unsigned char  pad19[3];
    void*          listAlloc;    // +0x1C
    AtlasNameNode* listHead;     // +0x20  self-linked sentinel
    unsigned int   listSize;     // +0x24
    void*          hudManager;   // +0x28
};

static_assert(sizeof(AtlasCache) == 0x2C, "sub_10088EA0 allocates 0x2C for it");

void __fastcall ac_addRef(void* self, void*) { ++static_cast<AtlasCache*>(self)->refCount; }
int  __fastcall ac_getRef(void* self, void*) { return static_cast<AtlasCache*>(self)->refCount; }

void __fastcall ac_slot08(void* self, void*)
{
    RD9_FRONTIER_AT("dice::hfe::dx9rend::AtlasCache::slot_0x08 (0x10017B50, unread)", self, 0x08);
}

void* __fastcall ac_queryInterface(void* self, void*, unsigned int iid)
{
    return iid == RD9_IID_IBASE ? self : 0;
}

void* const kAtlasCacheVtable[4] =
{
    reinterpret_cast<void*>(&ac_addRef),          // +0x00  addRef first
    reinterpret_cast<void*>(&ac_getRef),          // +0x04
    reinterpret_cast<void*>(&ac_slot08),          // +0x08
    reinterpret_cast<void*>(&ac_queryInterface)   // +0x0C
};

// sub_10017470, read in full: vptr, refcount, the empty vector, the flag, and the list head
// (sub_1000E410 - a self-linked sentinel node).
AtlasCache* atlasCacheCtor(void* storage)
{
    AtlasCache* c = static_cast<AtlasCache*>(storage);
    c->vptr       = const_cast<void**>(kAtlasCacheVtable);
    c->refCount   = 1;
    c->vecFirst   = 0;
    c->vecLast    = 0;
    c->vecEnd     = 0;
    c->hasAtlases = 0;

    AtlasNameNode* head =
        static_cast<AtlasNameNode*>(::renddx9abi::engineAlloc(sizeof(AtlasNameNode)));
    if (head)
    {
        head->next = head;
        head->prev = head;
        // the head's string is never constructed and never read - the same deliberate
        // non-construction Vc71Map.h records for the map head's key.
    }
    c->listHead = head;
    c->listSize = 0;
    return c;
}

void atlasCacheVectorPushBack(AtlasCache* c, TextureAtlas* atlas)
{
    if (c->vecLast != c->vecEnd)
    {
        *c->vecLast++ = atlas;
        return;
    }
    const unsigned int count = static_cast<unsigned int>(c->vecLast - c->vecFirst);
    const unsigned int grow  = count ? count * 2 : 1;
    TextureAtlas** fresh =
        static_cast<TextureAtlas**>(::renddx9abi::engineAlloc(grow * sizeof(TextureAtlas*)));
    for (unsigned int i = 0; i < count; ++i)
        fresh[i] = c->vecFirst[i];
    if (c->vecFirst)
        ::renddx9abi::engineFree(c->vecFirst);
    c->vecFirst = fresh;
    c->vecLast  = fresh + count;
    c->vecEnd   = fresh + grow;
    *c->vecLast++ = atlas;
}

} // anonymous namespace

bool atlasCacheExists()
{
    return g_atlasCache != 0;
}

void* atlasCacheEnsure()
{
    if (!g_atlasCache)
    {
        void* storage = ::renddx9abi::engineAlloc(sizeof(AtlasCache));
        g_atlasCache = storage ? atlasCacheCtor(storage) : 0;
    }
    return g_atlasCache;
}

void atlasCacheAddRef()
{
    if (g_atlasCache)
        ++static_cast<AtlasCache*>(g_atlasCache)->refCount;   // vslot +0x00
}

bool atlasCacheHasAtlases()
{
    return g_atlasCache && static_cast<AtlasCache*>(g_atlasCache)->hasAtlases != 0;
}


// Loads unique atlas names from a snapshot of the HUD list at texture priority 15.
// Returns whether any atlas parsed. Additions during callbacks wait for the next load;
// failed atlas loads retain their allocation. Requires class and HUD managers.


bool atlasCacheLoadAtlases(const vc71::string& prefix)
{
    AtlasCache* c = static_cast<AtlasCache*>(atlasCacheEnsure());
    if (!c)
        return false;

    const int savedPriority = resourceManagerSwapDefaultPriority(15);

    c->hudManager = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("HudManager"));

    bool anyParsed = false;

    {
        // hudManager->vslot+0x374() -> the atlas-name list. Typed in the native flavour (RW-266):
        // BF2HudManager::getAtlasList is its primary table's slot 221 (+0x374), and HostStringList
        // IS its std::list<std::string> there. The retail-ABI flavour keeps the raw slot.
#if BF2_PLUGIN_ABI_NATIVE
        const renddx9abi::HostStringList& names =
            static_cast<hud::BF2HudManager*>(c->hudManager)->getAtlasList();
#else
        void** vtbl = *reinterpret_cast<void***>(c->hudManager);
        typedef const renddx9abi::HostStringList& (__thiscall* GetAtlasNamesFn)(void*);
        const renddx9abi::HostStringList& names =
            reinterpret_cast<GetAtlasNamesFn>(vtbl[0x374 / 4])(c->hudManager);
#endif
        const std::list<renddx9abi::HostString> snapshot = copyAtlasNames(names);
        for (const renddx9abi::HostString& atlasName : snapshot)
            {
                vc71::string full(prefix);
                full += atlasName.c_str();

                // the dedup walk over the list at +0x20
                bool seen = false;
                for (AtlasNameNode* d = c->listHead->next; d != c->listHead; d = d->next)
                {
                    if (d->name.size() == full.size() &&
                        ::memcmp(d->name.c_str(), full.c_str(), full.size()) == 0)
                        seen = true;
                }
                if (seen)
                    continue;

                // push_back onto the dedup list (insert before the sentinel)
                AtlasNameNode* fresh =
                    static_cast<AtlasNameNode*>(::renddx9abi::engineAlloc(sizeof(AtlasNameNode)));
                if (fresh)
                {
                    new (&fresh->name) vc71::string(full);
                    fresh->next = c->listHead;
                    fresh->prev = c->listHead->prev;
                    c->listHead->prev->next = fresh;
                    c->listHead->prev       = fresh;
                    ++c->listSize;
                }

                void* storage = ::renddx9abi::engineAlloc(0x14);
                TextureAtlas* atlas = storage ? textureAtlasCtor(storage) : 0;
                if (atlas && textureAtlasLoadTAI(atlas, full))
                {
                    atlasCacheVectorPushBack(c, atlas);
                    anyParsed     = true;
                    c->hasAtlases = 1;
                }
                // a failed parse leaks the atlas object, exactly as the shipped body does
            }
    }

    resourceManagerSwapDefaultPriority(savedPriority);
    return anyParsed;
}

TextureAtlasEntry* atlasCacheFind(const vc71::string& name)
{
    AtlasCache* c = static_cast<AtlasCache*>(g_atlasCache);
    if (!c || c->vecFirst == c->vecLast)
        return 0;

    // sub_10017440 - first atlas that answers. The walk is by direct call, not through a vtable.
    for (TextureAtlas** it = c->vecFirst; it != c->vecLast; ++it)
    {
        TextureAtlasEntry* entry = textureAtlasFind(*it, name);
        if (entry)
            return entry;
    }
    return 0;
}


// Releases the atlas cache and returns its remaining reference count.
// At zero, destroys atlas page references and containers and clears the cache singleton.

int atlasCacheRelease()
{
    AtlasCache* c = static_cast<AtlasCache*>(g_atlasCache);
    if (!c)
        return 0;

    const int remaining = --c->refCount;
    if (!remaining)
    {
        c->vptr = const_cast<void**>(kAtlasCacheVtable);   // the dtor's vptr restore

        for (TextureAtlas** it = c->vecFirst; it != c->vecLast; ++it)
        {
            if (*it)
            {
                textureAtlasDtor(*it);
                ::renddx9abi::engineFree(*it);
                *it = 0;
            }
        }

        if (c->listHead)
        {
            AtlasNameNode* n = c->listHead->next;
            while (n != c->listHead)
            {
                AtlasNameNode* next = n->next;
                n->name.~string();
                ::renddx9abi::engineFree(n);
                n = next;
            }
            ::renddx9abi::engineFree(c->listHead);
            c->listHead = 0;
        }
        c->listSize = 0;

        if (c->vecFirst)
            ::renddx9abi::engineFree(c->vecFirst);
        c->vecFirst = 0;
        c->vecLast  = 0;
        c->vecEnd   = 0;

        ::renddx9abi::engineFree(c);
        g_atlasCache = 0;
    }
    return remaining;
}


// The cross-TU seams (RendDX9Base.h). The shipped callers reach the manager as __thiscall members
// on dword_1023B974; these carry the identical behaviour for our other translation units.


void* textureManagerGetTexture(const vc71::string& name)
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    if (!tm)
        return 0;
    return tm->getTexture(name);
}

int textureManagerSwapDefaultMipClass(int value)
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    return tm ? tm->swapDefaultMipClass(value) : 0;
}

int textureManagerSwapManaged(int value)
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    return tm ? tm->swapManaged(value) : 0;
}

int textureManagerSwapRecreateMode(int value)
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    return tm ? tm->swapRecreateMode(static_cast<unsigned char>(value)) : 0;
}

void* textureManagerCreateEmpty(unsigned int w, unsigned int h, unsigned int format,
                                unsigned int mips, unsigned int usage, void* existing)
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    if (!tm)
        return 0;
    return tm->createEmpty2D(w, h, format, mips, usage,
                             static_cast<EngineTexture*>(existing));
}

void* textureManagerCreateCube(unsigned int side, unsigned int format,
                               unsigned int mips, unsigned int usage, void* existing)
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    if (!tm)
        return 0;
    return tm->createEmptyCube(side, format, mips, usage,
                               static_cast<EngineTexture*>(existing));
}

void textureManagerEvictFromCache(void* tex)
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    if (tm && tex)
        tm->evictFromCache(static_cast<EngineTexture*>(tex));
}

// A manager member in the image, reached through the global manager without a null test.
// A null `source` reads the renderer's current swap chain back buffer (the base+0xE0 vector's
// first entry at +0xE4, indexed by +0xF0 - RendDX9Init.cpp's swapChainVectorPushBack); the source
// surface is fetched before the destination's, and both references are released afterwards.
bool textureManagerCopyRenderTarget(EngineTexture* source, EngineTexture* destination,
                                    unsigned int level, unsigned int face)
{
    if (!destination)
        return false;
    TextureManager* const tm = static_cast<TextureManager*>(g_textureManager);
    IDirect3DSurface9* sourceSurface = 0;
    if (!source)
    {
        unsigned char* const base = static_cast<unsigned char*>(g_rendererBase);
        IDirect3DSwapChain9* const* const chains =
            *reinterpret_cast<IDirect3DSwapChain9* const* const*>(base + 0xE4);
        const unsigned int current = *reinterpret_cast<const unsigned int*>(base + 0xF0);
        chains[current]->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &sourceSurface);
    }
    else
        sourceSurface = textureGetLevelSurface(source, level, face);
    IDirect3DSurface9* const destinationSurface = textureGetLevelSurface(destination, level, face);
    const bool copied = tm->copyRenderTargetToSurface(sourceSurface, destinationSurface);
    if (sourceSurface)
        sourceSurface->Release();
    if (destinationSurface)
        destinationSurface->Release();
    return copied;
}


// Saves texture to name + ".dds" through the host write-path mapping.
// Requires a valid wrapper and returns true only for D3D_OK.

bool textureManagerSaveTexture(EngineTexture* texture, const vc71::string& name)
{
    vc71::string path(name);
    path += ".dds";
    ::renddx9abi::engineFilePrepareWrite(path);                              // FileManager +0x18 (3, 2, 0), release
    vc71::string writePath;
    ::renddx9abi::engineFileGetWritePath(path, writePath);                   // FileManager +0xA4
    return D3DXSaveTextureToFileA(writePath.c_str(), 4u, texture->baseTexture, 0) == 0;
}

// The four reads the Texture constructor makes (Texture.cpp).
int textureManagerDefaultMipClass()
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    return tm ? tm->defaultMipClass() : 0;
}

int textureManagerManagedFlag()
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    return tm ? tm->managedFlag() : 0;
}

unsigned char textureManagerRecreateMode()
{
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    return tm ? tm->recreateMode() : 0;
}

const vc71::string& textureManagerOriginLabel()
{
    static vc71::string empty;
    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    return tm ? tm->originLabel() : empty;
}


// When the resource manager is ready, drops cache-only textures and reloads survivors
// that are not resident. Loading-progress events bracket the reload pass.

namespace {

struct LoadProgressPayload
{
    int         current;
    float       total;
    const char* label;
};

void postLoadProgress(int current, const char* label)
{
    if (!::dice::hfe::eventManager)
        return;

    LoadProgressPayload payload;
    payload.current = current;
    payload.total   = 1000.0f;
    payload.label   = label;

    ::dice::hfe::eventManager->postEvent(
        ECCore, 4, &payload, 0.0f);
}

} // anonymous namespace

void textureManagerSweep()
{
    if (!resourceManagerIsReady())
    {
        ::renddx9abi::logf("TextureManager sweep: skipped - the ResourceManager flag is clear, "
                           "which is the original's early return");
        return;
    }

    TextureManager* tm = static_cast<TextureManager*>(g_textureManager);
    if (!tm)
        return;

    // The shipped order: pass 1, the "Textures" progress event with the count, pass 2, the
    // closing event. Pass 2 is where every load deferred behind the ResourceManager flag (the
    // step-6 fonts, the step-9 console overlay) finally reads its file.
    const unsigned notResident = tm->sweepPass1();

    postLoadProgress(static_cast<int>(notResident), "Textures");

    tm->sweepPass2();

    postLoadProgress(0, 0);

    ::renddx9abi::logf("TextureManager sweep: cache has %u entries after pass 1, %u re-uploaded "
                       "in pass 2; two ECCore id-4 load-progress events posted through "
                       "postEvent +0x10", tm->cacheSize(), notResident);
}

bool createTextureManager(IDirect3DDevice9* device)
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(TextureManager));
    if (!storage)
    {
        g_textureManager = 0;
        return false;
    }

    g_textureManager = new (storage) TextureManager(device);
    return true;
}

bool textureManagerInit()
{
    if (!g_textureManager)
        return false;
    return static_cast<TextureManager*>(g_textureManager)->init();
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
