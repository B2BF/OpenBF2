// Direct3D atlas page creation and exported factories. Sources load from disk.
#if defined(BF2_IMAGE_TEXTUREATLASBUILDER)

#include "TextureAtlasD3D9.h"
#include "TextureFormat.h"
#include "../../../../RendDX9Abi/D3dx9Sdk.h"

#include <dxerr9.h>         // DXGetErrorString9A, from the SDK's DxErr9.lib
#include <cctype>

namespace textureatlas {


// export 12. `new` (the DLL's operator new: bf_malloc(0x58, 0) in the compatible
// flavour), the base constructor, then the leaf vptr (0x1001B888); a failed allocation returns null.

ITextureAtlasConfiguration* __fastcall ITextureAtlasConfiguration::create()
{
    return new TextureAtlasConfigurationD3D9();
}


// getTextureInfo(): the load path is inputDirectory + name (stored
// first), the name as given; D3DX reads the image's header from THE disk (D3DXGetImageInfoFromFileA:
// the game's zip archives are invisible to it - census finding 5). A failed read leaves both strings
// assigned and returns false. Width (+0x00), Height (+0x04), MipLevels (+0x0C) and Format (+0x10)
// are read out of the D3DXIMAGE_INFO stack copy (TAB 0x100082D0..0x100082FA).

bool TextureAtlasConfigurationD3D9::getTextureInfo(const std::string& inputDirectory,
                                                   const std::string& name, TextureInfo& info)
{
    info.path = inputDirectory + name;
    info.name = name;

    D3DXIMAGE_INFO imageInfo;
    if (FAILED(D3DXGetImageInfoFromFileA(info.path.c_str(), &imageInfo)))
        return false;

    info.rect.w = static_cast<int>(imageInfo.Width);
    info.rect.h = static_cast<int>(imageInfo.Height);
    info.format = formatToString(imageInfo.Format);
    info.miplevels = static_cast<int>(imageInfo.MipLevels);
    return true;
}

// (4 B each): the device-dependent limits.
int TextureAtlasD3D9::maxWidth()
{
    return m_maxWidth;
}

int TextureAtlasD3D9::maxHeight()
{
    return m_maxHeight;
}

namespace {

// the string BY value, upper-cased in place through MSVCR71's toupper
// with the char sign-extended (`movsx`), returned by value (through EBX: the DLL was built with /GL).
// end() is re-read on every pass - a hand-written loop, not std::transform. RendDX9 carries its own
// copy (0x10015650).
std::string toUpper(std::string s)
{
    for (std::string::iterator it = s.begin(); it != s.end(); ++it)
        *it = static_cast<char>(toupper(*it));
    return s;
}

} // anonymous namespace


// Renders atlas pages and saves DDS files; returns false for reported copy or save failures.
// Compatibility quirks remain: working textures are dereferenced before creation is checked,
// DXT save status is ignored, and the DXT error path leaks its texture.

bool TextureAtlasD3D9::buildTextures()
{
    for (std::vector<Group>::iterator page = m_pages.begin(); page != m_pages.end(); ++page)
    {
        int miplevels = page->miplevels;
        if (miplevels < 1)
            miplevels = 1;
        const D3DFORMAT format = stringToFormat(toUpper(page->format));

        IDirect3DTexture9* atlasTexture = 0;
        HRESULT hr = D3DXCreateTexture(m_device, page->width, page->height, miplevels, 0,
                                       isDXT(format) ? D3DFMT_A8R8G8B8 : format, D3DPOOL_SYSTEMMEM,
                                       &atlasTexture);
        const int levels = static_cast<int>(atlasTexture->GetLevelCount());
        if (FAILED(hr))
        {
            m_error(std::string("Could not create atlasTexture for group ") + page->name);
            return false;
        }

        for (std::vector<TextureInfo>::iterator tex = page->textures.begin();
             tex != page->textures.end(); ++tex)
        {
            IDirect3DTexture9* texture;
            if (FAILED(D3DXCreateTextureFromFileExA(m_device, tex->path.c_str(),
                                                    D3DX_DEFAULT, D3DX_DEFAULT,
                                                    0, 0, D3DFMT_UNKNOWN, D3DPOOL_SYSTEMMEM,
                                                    D3DX_FILTER_NONE, D3DX_DEFAULT,
                                                    0, 0, 0, &texture)))
            {
                m_error(std::string("Could not load texture: ") + tex->path);
                atlasTexture->Release();
                return false;
            }

            int x = tex->rect.x;
            int y = tex->rect.y;
            int w = tex->rect.w;
            int h = tex->rect.h;
            for (int level = 0; level < levels; ++level)
            {
                if (w == 0 || h == 0)
                    break;

                RECT rect;
                rect.left = x;
                rect.top = y;
                rect.right = x + w;
                rect.bottom = y + h;
                if (level == 0 && tex->border)
                {
                    rect.left += page->border;
                    rect.top += page->border;
                    rect.right -= page->border;
                    rect.bottom -= page->border;
                }

                IDirect3DSurface9* srcSurface = 0;
                IDirect3DSurface9* dstSurface = 0;
                const int srcLevel = level < static_cast<int>(texture->GetLevelCount()) - 1
                                         ? level
                                         : static_cast<int>(texture->GetLevelCount()) - 1;
                hr = texture->GetSurfaceLevel(srcLevel, &srcSurface);
                if (FAILED(hr))
                {
                    m_error(std::string(DXGetErrorString9A(hr)));
                    texture->Release();
                    atlasTexture->Release();
                    return false;
                }
                hr = atlasTexture->GetSurfaceLevel(level, &dstSurface);
                if (FAILED(hr))
                {
                    m_error(std::string(DXGetErrorString9A(hr)));
                    srcSurface->Release();
                    texture->Release();
                    atlasTexture->Release();
                    return false;
                }
                hr = D3DXLoadSurfaceFromSurface(dstSurface, 0, &rect, srcSurface, 0, 0,
                                                D3DX_FILTER_NONE, 0);
                if (FAILED(hr))
                {
                    m_error(std::string(DXGetErrorString9A(hr)));
                    dstSurface->Release();
                    srcSurface->Release();
                    texture->Release();
                    atlasTexture->Release();
                    return false;
                }
                if (srcSurface)
                {
                    srcSurface->Release();
                    srcSurface = 0;
                }
                if (dstSurface)
                    dstSurface->Release();

                x /= 2;
                y /= 2;
                w /= 2;
                h /= 2;
            }
            if (texture)
            {
                texture->Release();
                texture = 0;
            }
        }

        const std::string filename = page->name + ".dds";
        if (isDXT(format))
        {
            IDirect3DTexture9* dxtTexture;
            const HRESULT createResult = D3DXCreateTexture(m_device, page->width, page->height,
                                                           page->miplevels, 0, format,
                                                           D3DPOOL_SYSTEMMEM, &dxtTexture);
            IDirect3DSurface9* srcSurface = 0;
            IDirect3DSurface9* dstSurface = 0;
            for (int level = 0; level < levels; ++level)
            {
                atlasTexture->GetSurfaceLevel(level, &srcSurface);
                dxtTexture->GetSurfaceLevel(level, &dstSurface);
                D3DXLoadSurfaceFromSurface(dstSurface, 0, 0, srcSurface, 0, 0,
                                           D3DX_FILTER_NONE, 0);
                if (srcSurface)
                {
                    srcSurface->Release();
                    srcSurface = 0;
                }
                if (dstSurface)
                {
                    dstSurface->Release();
                    dstSurface = 0;
                }
            }

            const std::string path = m_outputPath + filename;
            D3DXSaveTextureToFileA(path.c_str(), D3DXIFF_DDS, dxtTexture, 0);
            if (FAILED(createResult))
            {
                m_error(std::string("Error saving texture \"") + path + "\"");
                atlasTexture->Release();
                return false;
            }
            if (dxtTexture)
            {
                dxtTexture->Release();
                dxtTexture = 0;
            }
        }
        else
        {
            const std::string path = m_outputPath + filename;
            if (FAILED(D3DXSaveTextureToFileA(path.c_str(), D3DXIFF_DDS, atlasTexture, 0)))
            {
                m_error(std::string("Error saving texture \"") + path + "\"");
                atlasTexture->Release();
                return false;
            }
        }

        if (atlasTexture)
        {
            atlasTexture->Release();
            atlasTexture = 0;
        }
    }
    return true;
}


// (out of line, `retn 4`; inlined into create) - the base constructor, the leaf vptr,
// 0 x 0, the device stored; a non-null device is AddRef'd (+0x04) and lifts the limit to 2048 x 2048.

TextureAtlasD3D9::TextureAtlasD3D9(IDirect3DDevice9* device)
    : m_maxWidth(0),
      m_maxHeight(0),
      m_device(device)
{
    if (device)
    {
        device->AddRef();
        m_maxWidth = 2048;
        m_maxHeight = 2048;
    }
}

// (inlined into the scalar deleting destructor 0x10008C10, the vftable's slot +0x00):
// Release (+0x08) the device when there is one, then the base destructor.
TextureAtlasD3D9::~TextureAtlasD3D9()
{
    if (m_device)
        m_device->Release();
}


// export 11. `new` (0x58), the constructor inlined; ECX is the device.

ITextureAtlas* __fastcall ITextureAtlas::create(IDirect3DDevice9* device)
{
    return new TextureAtlasD3D9(device);
}

} // namespace textureatlas

#endif // BF2_IMAGE_TEXTUREATLASBUILDER
