// Texture-atlas builder DLL interface. [INFERRED] header placement and method names.
// Preserve virtual order, export names and explicit __fastcall conventions.
// Objects are allocated and destroyed inside the DLL. Callback strings use that DLL's STL;
// callers that inspect them must match its ABI. Other arguments cross without CRT ownership.

#ifndef TEXTUREATLASBUILDER_ITEXTUREATLAS_H
#define TEXTUREATLASBUILDER_ITEXTUREATLAS_H

#include <string>
#include <vector>

struct IDirect3DDevice9;

// The DLL image exports the two interfaces; every other image (RendDX9, test hosts) imports them.
#if defined(__APPLE__) || defined(BF2_LINUX_CLIENT)
// [CHOSEN, not recovered] The native renderer embeds the builder without DLL attributes.
#  define TEXTUREATLAS_API
#elif defined(BF2_IMAGE_TEXTUREATLASBUILDER)
#  define TEXTUREATLAS_API __declspec(dllexport)
#else
#  define TEXTUREATLAS_API __declspec(dllimport)
#endif

namespace textureatlas {

// The error / warning sink. ECX = the message; the DLL's own defaults (nullsub_1 0x10002320 for the
// atlas, nullsub_2 0x10007460 for the configuration) and RendDX9's (0x100156D0 / 0x100156E0) are a
// bare `retn`.
typedef void (__fastcall* MessageFn)(const std::string& message);


// The packer's rectangle. TextureInfo carries one at +0x38, and Packer/Node read +0x08 (w) / +0x0C
// (h) and write +0x00 (x) / +0x04 (y) through a pointer to it (TAB 0x10001F70, 0x10001FB0). The
// default constructor's zeroing is the four stores in addTexture's local (0x10007EA8..0x10007EB6).

struct Rect
{
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}

    int x;      // +0x00
    int y;      // +0x04
    int w;      // +0x08
    int h;      // +0x0C
};


// Source texture metadata. Name separators are normalized; the load path is preserved.
// addTexture sets border/mip count before storing the record; the constructor leaves them unset.

struct TextureInfo
{
    std::string name;
    std::string path;
    Rect        rect;
    std::string format;
    bool        border;
    int         miplevels;
};


// One group of the configuration - and, in the atlas, one output page (0x64 bytes in the VC7.1
// image). The constructor is 0x10006A20 (`retn 48h`: two strings by value and four ints); the atlas's
// build() inlines it for every page it opens.

struct Group
{
    Group(std::string name_, int width_, int height_, std::string format_, int miplevels_, int border_)
        : name(name_), format(format_), x(0), y(0), width(width_), height(height_),
          miplevels(miplevels_), border(border_), index(-1)
    {
    }

    std::string              name;       // +0x00
    std::vector<TextureInfo> textures;   // +0x1C
    std::string              format;     // +0x2C ""  = one page per format run (split mode)
    int                      x;          // +0x48 always 0
    int                      y;          // +0x4C always 0
    int                      width;      // +0x50 0 = the atlas maximum; a page: its power-of-two extent
    int                      height;     // +0x54
    int                      miplevels;  // +0x58 <= 0 = the maximum over the group's textures
    int                      border;     // +0x5C texel inset of bordered textures (default 1)
    int                      index;      // +0x60 -1; a page: its number within the group
};

// The iterator the configuration's slots +0x28..+0x34 return through a hidden pointer (a VC7.1 vector
// iterator is a one-pointer class); +0x34 forwards to vector::erase (0x100081C0).
typedef std::vector<Group>::iterator GroupIter;


// ITextureAtlasConfiguration - 16 slots (vftable 0x1001B360). A .tac file, or groups and textures
// added one by one; the input path is prefixed to every texture, the output path is where the atlas
// writes its pages.

class TEXTUREATLAS_API ITextureAtlasConfiguration
{
public:
    virtual ~ITextureAtlasConfiguration();                                  // +0x00
    virtual bool load(const char* tacPath) = 0;                             // +0x04
    virtual void save(const char* tacPath) = 0;                             // +0x08
    virtual void setOutputPath(const char* directory) = 0;                  // +0x0C
    virtual void setInputPath(const char* directory) = 0;                   // +0x10
    virtual const char* getInputPath() = 0;                                 // +0x14
    virtual const char* getOutputPath() = 0;                                // +0x18
    virtual Group* selectGroup(const char* name) = 0;                       // +0x1C
    virtual Group* addGroup(const char* name, int width, int height,
                            const char* format, int miplevels, int border) = 0;   // +0x20
    virtual TextureInfo* addTexture(const char* path, bool border) = 0;     // +0x24
    virtual GroupIter findGroup(const char* name) = 0;                      // +0x28
    virtual GroupIter groupsBegin() = 0;                                    // +0x2C
    virtual GroupIter groupsEnd() = 0;                                      // +0x30
    virtual GroupIter removeGroup(GroupIter group) = 0;                     // +0x34
    virtual void setErrorCallback(MessageFn callback) = 0;                  // +0x38
    virtual void setWarningCallback(MessageFn callback) = 0;                // +0x3C

    // a new (0x58-byte) D3D9 configuration through the DLL's operator new.
    static ITextureAtlasConfiguration* __fastcall create();
};


// ITextureAtlas - 6 slots (vftable 0x1001B3A0). build() packs a configuration into pages; save()
// renders the pages to "<outputPath><page>.dds" and writes the .tai.

class TEXTUREATLAS_API ITextureAtlas
{
public:
    virtual ~ITextureAtlas();                                               // +0x00
    virtual void build(ITextureAtlasConfiguration* configuration) = 0;      // +0x04
    virtual bool save(const char* taiPath) = 0;                             // +0x08
    virtual void setSortMode(int mode) = 0;                                 // +0x0C
    virtual void setErrorCallback(MessageFn callback) = 0;                  // +0x10
    virtual void setWarningCallback(MessageFn callback) = 0;                // +0x14

    // a new (0x58-byte) D3D9 atlas; it AddRefs the device and sets the maximum page size
    // to 2048 x 2048. With a null device the maximum stays 0 x 0 and every texture is "too large".
    static ITextureAtlas* __fastcall create(IDirect3DDevice9* device);
};

} // namespace textureatlas

#endif // TEXTUREATLASBUILDER_ITEXTUREATLAS_H
