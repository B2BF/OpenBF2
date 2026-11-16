// Path: attested by RendDX9.dll __FILE__ at 0x101E1940 (lines 81, 453, 495, 574, 713).
// Reads .mfx metadata and emits .fx permutations, recovered from RendDX9.dll.
// [CHOSEN, not recovered] std::regex replaces PME; native STL stores private state;
// Native compile-error dialogs report the error and select Cancel.

#ifdef BF2_IMAGE_RENDDX9

#include "RaShaderPermutator.h"

#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../../../RendDX9Abi/EngineFile.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../RendDX9Base.h"
#include "../../Core/StringUtil.h"          // extractFileName (sub_10075190)
#include "../../RendMisc/IRenderer.h"
#include "../../IO/File/FileChangeManager.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>

namespace dice {
namespace hfe {
extern IBase* fileChangeManager;
namespace dx9rend {

// RendDX9Init.cpp - the DeviceCaps block and its pixel-shader major reader.
extern void* g_deviceCaps;
int deviceCapsPixelShaderMajor();

// BF2Render.cpp - 0x1023D098. sub_10029D00 tests it before asking the Renderer for isExpansionPack.
extern IBase* g_Renderer;

// ShaderManager.cpp - dword_1023B97C. generate() takes the compiler's Flags from its +0x84 and its
// ID3DXInclude from its +4, both through the accessors that file publishes.
extern void* g_shaderManager;

// Effect.cpp - sub_10032F20, the same body and the same key table. readSource() runs it on the
// .mfx exactly as the .fx loader runs it on a shader.
void deobfuscateShader(unsigned char* buf, unsigned int size);

namespace Ra {

// dword_1023D954 - Ra/EffectManager.cpp. shaderMacros() reads one byte of it (+0x40).
extern void* g_effectManager;
unsigned char effectManagerFlag40();   // Ra/EffectManager.cpp - that +0x40 byte


// Generated shader files live in EffectManager's case-insensitive memory store.

bool effectManagerAddMemoryFile(const std::string& name, const std::string& contents);
void effectManagerRemoveMemoryFile(const std::string& name);

namespace {


// The literal numbers the shipped FileManager calls pass. sub_1003EAB0 opens the .mfx with
// (2, 0, 0) - the same read pair every recovered call in this image uses - and sub_1003D080 opens
// the generated .fx with (0, 2, 0), which IO/File/IFileSystem.h records as the write pair
// (FileManager::open takes the write path when the mode is not 2 and the access is 1 or 2).

const int kFileModeRead    = 2;
const int kFileAccessRead  = 0;
const int kFileModeWrite   = 0;
const int kFileAccessWrite = 2;


// std::string helpers. Every one of these is a body in the shipped image; the PE address is on the
// line. They keep the shipped operation sequences rather than the toolset's own string members.


// VC7.1's std::string::operator< - the ordering every inlined comparison in this class lands on.
int vc71Compare(const std::string& a, const std::string& b)
{
    const unsigned int na = static_cast<unsigned int>(a.size());   // .mfx names and tokens
    const unsigned int nb = static_cast<unsigned int>(b.size());
    const unsigned int n  = na < nb ? na : nb;
    if (n)
    {
        const int r = ::memcmp(a.c_str(), b.c_str(), n);
        if (r)
            return r;
    }
    if (na < nb) return -1;
    if (na > nb) return 1;
    return 0;
}

bool vc71Equal(const std::string& a, const std::string& b)
{
    return vc71Compare(a, b) == 0;
}

std::string vc71FromStd(const std::string& s)
{
    return std::string(s.c_str(), static_cast<unsigned int>(s.size()));
}

std::string stdFromVc71(const std::string& s)
{
    return std::string(s.c_str(), s.size());
}

// sub_10074BE0 / sub_10074D10, via the 0x10074E40 / 0x10074E80 copy wrappers.
// NOT plain toupper/tolower: five Scandinavian code points are mapped explicitly before the CRT
// is consulted, and the CRT is consulted only when none of them matched. The five pairs are
// aring/adiaeresis/odiaeresis/udiaeresis/eacute in the latin-1 code page, and they are in the
// image because dice is Swedish. Reproduced with the shipped byte values.
// PE 0x10074E40: copy, upper-case the copy in place (0x10074BE0, inlined here), return it.
std::string vc71ToUpper(const std::string& s)
{
    std::string out = stdFromVc71(s);
    for (std::string::size_type i = 0; i < out.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(out[i]);
        if      (c == 0xE5) out[i] = static_cast<char>(0xC5);
        else if (c == 0xE4) out[i] = static_cast<char>(0xC4);
        else if (c == 0xF6) out[i] = static_cast<char>(0xD6);
        else if (c == 0xFC) out[i] = static_cast<char>(0xDC);
        else if (c == 0xE9) out[i] = static_cast<char>(0xC9);
        else                out[i] = static_cast<char>(::toupper(static_cast<char>(c)));
    }
    return vc71FromStd(out);
}

std::string vc71ToLower(const std::string& s)
{
    std::string out = stdFromVc71(s);
    for (std::string::size_type i = 0; i < out.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(out[i]);
        if      (c == 0xC5) out[i] = static_cast<char>(0xE5);
        else if (c == 0xC4) out[i] = static_cast<char>(0xE4);
        else if (c == 0xD6) out[i] = static_cast<char>(0xF6);
        else if (c == 0xDC) out[i] = static_cast<char>(0xFC);
        else if (c == 0xC9) out[i] = static_cast<char>(0xE9);
        else                out[i] = static_cast<char>(::tolower(static_cast<char>(c)));
    }
    return vc71FromStd(out);
}

// sub_10075600 (reached through the 12-byte forwarder PE 0x10075190, listing read: calls 0x10075600 and returns the hidden result). rfind both separators, take
// whichever is later, and keep everything after it; the whole string when neither is present.
std::string vc71BaseName(const std::string& s)
{
    const std::string t = stdFromVc71(s);
    const std::string::size_type back  = t.rfind('\\');
    const std::string::size_type fwd   = t.rfind('/');
    std::string::size_type cut = back;
    if (fwd != std::string::npos && (cut == std::string::npos || fwd > cut))
        cut = fwd;
    if (cut == std::string::npos)
        return s;
    return vc71FromStd(t.substr(cut + 1));
}

} // anonymous namespace

// PE 0x1003C440 - drop every '_' (95) and '|' (124). "_base_|_detail_" -> "basedetail", which is
// what the .mfx's own comment calls "the permutationname - "_"". One shared copy (RendDX9_r has a
// single one as well); EffectManager.cpp's raGenerateShaderByCleanName calls it too.
std::string cleanPermutationName(const std::string& s)
{
    std::string out;
    const char* p = s.c_str();
    for (unsigned int i = 0; i < s.size(); ++i)
    {
        if (p[i] != 95 && p[i] != 124)
            out.push_back(p[i]);
    }
    return vc71FromStd(out);
}

namespace {

// sub_1003C4D0 - split at the first '|'. `rest` is consumed in place; the token is returned. With
// no '|' left the whole of `rest` is the token and `rest` becomes empty, which is what terminates
// every caller's loop (they test the token's size).
std::string splitFirstToken(std::string& rest)
{
    const std::string t = stdFromVc71(rest);
    const std::string::size_type bar = t.find('|');
    if (bar == std::string::npos)
    {
        const std::string token = rest;
        rest = "";
        return token;
    }
    const std::string token = vc71FromStd(t.substr(0, bar));
    rest = vc71FromStd(t.substr(bar + 1));
    return token;
}

// sub_10075000 - _stricmp on the two c_str()s. Used only by bitFromName.
bool vc71EqualNoCase(const std::string& a, const std::string& b)
{
    return ::_stricmp(a.c_str(), b.c_str()) == 0;
}

// sub_1003C870, RaShaderPermutator.cpp:713. `bitNames` is a NUL-terminated array of names; the
// answer is 1 << <index>, or 0 with a diagnostic when the name is not in it.
int bitFromName(const std::string& name, const char* const* bitNames)
{
    int shift = 0;
    if (bitNames && *bitNames)
    {
        for (const char* const* it = bitNames; *it; ++it, ++shift)
        {
            if (vc71EqualNoCase(name, std::string(*it)))
                return 1 << shift;
        }
    }

    ::renddx9abi::logf("RaShaderPermutator.cpp:713: bitName not found %s", name.c_str());
    return 0;
}



// Prefixes unqualified paths with the active mod; a missing renderer selects xpack.
// Uses shared scratch storage and is not reentrant.

void applyModPrefix(std::string& path)
{
    static std::string s_scratch;           // unk_1023D488

    if (::strstr(path.c_str(), "mods/"))
        return;

    bool xpack = true;
    if (g_Renderer)
    {
        xpack = static_cast<IRenderer*>(g_Renderer)->isExpansionPack();
    }

    s_scratch = xpack ? "mods/xpack/" : "mods/bf2/";
    s_scratch += path;
    path = s_scratch;
}


// The FileManager's getFileInfo (+0x38) and the 64-bit last-write time it fills. Both flavours of
// the plugin ABI are handled here so no body below has to know which one it is compiled for.

// The offsets are not guessed. Code/BF2/IO/File/IFileSystem.h records the PE record as 64 bytes
// with the last-write filetime's low dword at +56 and its high dword at +60, and both shipped
// readers in this file agree: sub_1003EAB0 copies `info[56]`/`info[60]` into the object's +0x48 /
// +0x4C, and sub_1003D080 compares the same pair against them.


bool engineGetFileTime(const std::string& path, unsigned int& low, unsigned int& high)
{
    low = 0;
    high = 0;
    if (!::dice::hfe::fileManager)
        return false;

    ::dice::hfe::io::FileInfo info;
    const ::renddx9abi::HostString name = ::renddx9abi::makeHostString(path);
    if (!::dice::hfe::fileManager->getFileInfo(name, info))
        return false;

    low  = info.lastWriteTimeLow;
    high = info.lastWriteTimeHigh;
    return true;
}



// Effect-compiler reads shared by readStringArrays and readDeprecationList.

D3DXHANDLE fxGetParameterByName(ID3DXEffectCompiler* fx, D3DXHANDLE parent, const char* name)
{
    return fx->GetParameterByName(parent, name);
}

// Only D3DXPARAMETER_DESC::Elements (+24) is read by the shipped bodies.
bool fxGetElementCount(ID3DXEffectCompiler* fx, D3DXHANDLE handle, unsigned int& elements)
{
    D3DXPARAMETER_DESC desc;
    ::memset(&desc, 0, sizeof(desc));
    if (fx->GetParameterDesc(handle, &desc) != 0)
        return false;
    elements = desc.Elements;
    return elements != 0;
}

const char* fxGetElementString(ID3DXEffectCompiler* fx, D3DXHANDLE handle, unsigned int index)
{
    const D3DXHANDLE element = fx->GetParameterElement(handle, index);
    const char* text = 0;
    fx->GetString(element, &text);
    return text;
}


// Lazily uses d3dx9_24.dll to avoid compiler recursion in the April 2005 D3DX version.
// The failure diagnostic retains its original debug-DLL spelling.

#if defined(__APPLE__) || defined(BF2_LINUX_CLIENT)
// [CHOSEN, not recovered] Native compiler entry; no Win32 DLL recursion workaround.
// PE 0x1003C610; platform D3DX entry selection below.
long createEffectCompiler(const char* srcData, unsigned int srcDataLen,
    const D3DXMACRO* defines, LPD3DXINCLUDE include, unsigned long flags,
    ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut)
{
    return D3DXCreateEffectCompiler(srcData, srcDataLen, defines, include,
        static_cast<DWORD>(flags), compilerOut, parseErrorsOut);
}
#else
typedef HRESULT (WINAPI* D3dxCreateEffectCompilerFn)(LPCSTR, UINT, CONST D3DXMACRO*,
                                                     LPD3DXINCLUDE, DWORD,
                                                     LPD3DXEFFECTCOMPILER*, LPD3DXBUFFER*);

HMODULE s_d3dxModule = 0;                       // dword_1023E108
D3dxCreateEffectCompilerFn s_createEffectCompiler = 0;   // dword_1023E10C

// PE 0x1003C610
long createEffectCompiler(const char* srcData, unsigned int srcDataLen,
                          const D3DXMACRO* defines, LPD3DXINCLUDE include, unsigned long flags,
                          ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut)
{
    if (!s_d3dxModule)
    {
        s_d3dxModule = ::LoadLibraryA("d3dx9_24.dll");
        if (s_d3dxModule)
        {
            s_createEffectCompiler = reinterpret_cast<D3dxCreateEffectCompilerFn>(
                ::GetProcAddress(s_d3dxModule, "D3DXCreateEffectCompiler"));
        }
        if (!s_d3dxModule || !s_createEffectCompiler)
        {
            ::renddx9abi::logf("RaShaderPermutator.cpp:574: Failed to load D3DX9d_24.dll, we need "
                               "this to avoid the April 2005 DxSDK bug(recursive call on parsing "
                               "some .fx files (skinnedmesh.fx)");
            return static_cast<long>(0x80004005);   // E_FAIL, as the shipped body returns
        }
    }

    return s_createEffectCompiler(srcData, srcDataLen, defines, include, flags,
                                  compilerOut, parseErrorsOut);
}
#endif


// The three text passes' patterns, transcribed verbatim from the image. Each is constructed with
// PME's "mg" flags - multiline plus global - and each is line-anchored, which is what lets the
// substitution below apply them to one line at a time. See the [CHOSEN] note at the top of the
// file.

const char* const kIncludePattern = "^#include\\s+\"(.*.d?fx)\".*";
const char* const kLiteralPattern =
    "^const\\s+(?:(?:bool)|(?:int)|(?:dword))\\s+([a-zA-Z0-9_]+)\\s+:\\s+LITERAL.*";
const char* const kDefinePattern  = "^//#define\\s+([a-zA-Z0-9_]+)\\s+([a-zA-Z0-9_]+).*";

// One line of `text` starting at `from`. `end` is where the pattern is allowed to reach (the CR of
// a CRLF pair is excluded so the trailing `.*` does not have to eat it); `rawEnd` is the byte
// before the newline, which is what a splice must cut at; `next` is the start of the following
// line. This stands in for PME's internal cursor: PME::match advances through the buffer and
// PME::getMarkers reports the match's byte span, which sub_1003C6F0 uses to splice.
bool nextLine(const char* text, unsigned int size, unsigned int from,
              unsigned int& begin, unsigned int& end, unsigned int& rawEnd, unsigned int& next)
{
    if (from >= size)
        return false;
    begin  = from;
    rawEnd = from;
    while (rawEnd < size && text[rawEnd] != '\n' && text[rawEnd] != '\0')
        ++rawEnd;
    next = (rawEnd < size) ? rawEnd + 1 : size;
    end = rawEnd;
    while (end > begin && text[end - 1] == '\r')
        --end;
    return true;
}

std::ostream& operator<<(std::ostream& os, const std::string& s)
{
    os.write(s.c_str(), static_cast<std::streamsize>(s.size()));
    return os;
}

} // anonymous namespace

// The file-local vc71ToUpper above, published for DiceEffect.cpp (declared in DiceEffect.h), whose
// PE 0x1001A510 upper-cases its copy through the same 0x10074BE0.
std::string vc71UpperCopy(const std::string& s)
{
    return vc71ToUpper(s);
}


// ShippedStringLess - the ordering declared in the header.

bool ShippedStringLess::operator()(const std::string& a, const std::string& b) const
{
    return vc71Compare(a, b) < 0;
}


// Initializes an empty MetaFx with its path stem, extension and output mode.
// Source reading supplies file timestamps; the file watch takes a counted reference.

MetaFx::MetaFx(const std::string& name, const std::string& extension, bool writeGeneratedFiles)
    : m_refCount(1)
    , m_name(name)
    , m_extension(extension)
    , m_source(0)
    , m_sourceSize(0)
    , m_writeGeneratedFiles(writeGeneratedFiles)
{
    // The watch name the shipped constructor builds, exactly as it builds it: `name + "." +
    // extension`, where "." is asc_101E1AD0 and the second operand is read out of the disassembly
    // (the decompiler prints the operator+= with a hidden argument).
    std::string watchName(m_name);
    watchName += ".";
    watchName += m_extension;


    // PE 0x1003E247..0x1003E265. The resolved return name is discarded.
    static_cast<io::IFileChangeManager*>(::dice::hfe::fileChangeManager)->addListener(
        reinterpret_cast<io::IFileChangeListener*>(this), watchName, 0);
}


// Destroys member resources in reverse order. The source buffer is intentionally not freed.

// PE 0x1003E290. Member destructors retain the complete reverse-container order.
MetaFx::~MetaFx()
{
    static_cast<io::IFileChangeManager*>(::dice::hfe::fileChangeManager)->removeListener(
        reinterpret_cast<io::IFileChangeListener*>(this));
    // shipped defect: m_source is not freed.
}


// The vtable, in slot order.


// +0x00, the shared IBase body, `++*(this+1)`.
void MetaFx::addRef()
{
    ++m_refCount;
}

// +0x04, the shared IBase body, `return *(this+1)`.
int MetaFx::getRef()
{
    return m_refCount;
}

// +0x08
//     v = --refCount; if (!v) { ~MetaFx(); bf_free(this); } return v;
// The decrement happens first and the returned value is the NEW count, including on the path
// that frees - the shipped body materialises it out of edi before the free, so reading it after
// the object is gone is not a defect here.
// PE 0x10041A20.
int MetaFx::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~MetaFx();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// PE 0x10041990: IBase and IFileChangeListener share the primary face.
void* MetaFx::queryInterface(unsigned int iid) const
{
    return iid == 1 || iid == io::IID_IFileChangeListener
        ? const_cast<MetaFx*>(this) : 0;
}

// +0x10, 0x1003EF10 - `retn 8`. The body is the parse loop and nothing else; both arguments are
// pushed by the caller and read by neither the shipped body nor this one.

// PE 0x1003EF10. FileChangeManager's typed call discards the result.
void MetaFx::onFileChanged(const std::string&, unsigned int)
{
    parse();
}


//     do { sub_1003EAB0(this); result = sub_1003E3B0(this); } while (!result);

bool MetaFx::parse()
{
    bool done = false;
    do
    {
        readSource();
        done = generate();
    }
    while (!done);
    return done;
}


// Replaces the source buffer, expands includes and gathers literal names and defines.
// Returns false if opening fails. The stored length includes the trailing NUL.
// Applies mod-path adjustment only with a renderer; deobfuscation defaults to enabled.

// PE 0x1003EAB0
bool MetaFx::readSource()
{
    if (m_source)
    {
        ::renddx9abi::engineFree(m_source);
        m_source     = 0;
        m_sourceSize = 0;
    }

    m_structs.clear();                       // sub_100419C0(&m_structs, 0)

    if (g_Renderer)
    {
        // FileManager +0x90, stripModPath - returned BY value, assigned over the name.
        const ::renddx9abi::HostString stripped =
            ::dice::hfe::fileManager->stripModPath(::renddx9abi::makeHostString(m_name));
        ::renddx9abi::assignFromHostString(m_name, stripped);
        applyModPrefix(m_name);
    }

    std::string fullName(m_name);
    fullName += ".";
    fullName += m_extension;

    ::renddx9abi::EngineStream file;
    if (!file.open(fullName, 2, 0, 0))
    {
        // RaShaderPermutator.cpp:81. The shipped diagnostic goes through the unread debug-stream
        // family (sub_10074300 severity 6 / sub_10074690); this image renders every recovered
        // diagnostic through its own log instead - see Texture.cpp:484 for the precedent.
        ::renddx9abi::logf("RaShaderPermutator.cpp:81: Failed to open dice effect: %s",
                           fullName.c_str());
        return false;
    }

    // The caller compiles even after a load failure; the later compile diagnostic stops it.
    // Build the metadata path separately, preserving the original evaluation order.
    std::string infoName(m_name);
    infoName += ".";
    infoName += m_extension;

    unsigned int low = 0;
    unsigned int high = 0;
    if (engineGetFileTime(infoName, low, high))
    {
        m_fileTimeLow  = low;
        m_fileTimeHigh = high;
    }
    else
    {
        m_fileTimeLow  = 0;
        m_fileTimeHigh = 0;
    }

    if (m_source)                            // the redundant second free - see the note
    {
        ::renddx9abi::engineFree(m_source);
        m_source = 0;
    }

    const int size = file.getSize();
    m_sourceSize = static_cast<unsigned int>(size);
    m_source     = static_cast<char*>(::renddx9abi::engineAlloc(m_sourceSize + 1));
    // Shipped defect: allocation failure is unchecked before reading into the buffer.
    file.read(m_source, m_sourceSize);
    file.close();

    if (::dice::hfe::dx9rend::readBoolSetting("GSLoadObfuscated", true))
        deobfuscateShader(reinterpret_cast<unsigned char*>(m_source), m_sourceSize);

    m_source[m_sourceSize] = 0;
    ++m_sourceSize;                          // the stored length now counts the terminator


    // Expands includes through ShaderManager's mounted-file resolver before parsing.

    expandIncludes();

    // sub_1003CF90 then sub_1003CC50, in that order.
    collectLiterals();
    collectDefines();

    return true;
}


// sub_1003C6F0 - 383 bytes. See the block comment above for the shape.

void MetaFx::expandIncludes()
{
    void* const include = ::dice::hfe::dx9rend::shaderManagerInclude(g_shaderManager);
    if (!include || !m_source)
        return;

    typedef long (__stdcall* IncludeOpenFn)(void*, int, const char*, const void*,
                                            const void**, unsigned int*);
    typedef long (__stdcall* IncludeCloseFn)(void*, const void*);

    void** const vtbl = *reinterpret_cast<void***>(include);

    const std::regex pattern(kIncludePattern);

    for (;;)
    {
        // PME::match over the whole buffer, then PME::getMarkers for the matched span. The line
        // walk is this file's stand-in for PME's cursor; see the [CHOSEN] note at the top.
        unsigned int cursor = 0;
        unsigned int begin = 0, end = 0, rawEnd = 0, next = 0;
        std::string  fileName;
        bool         matched = false;
        unsigned int matchBegin = 0;
        unsigned int matchEnd   = 0;

        const char* const text = m_source;
        while (nextLine(m_source, m_sourceSize, cursor, begin, end, rawEnd, next))
        {
            std::cmatch m;
            if (std::regex_search(text + begin, text + end, m, pattern))
            {
                fileName   = m[1].str();
                matchBegin = begin;
                matchEnd   = rawEnd;     // the splice cuts at the newline, as getMarkers does
                matched    = true;
                break;
            }
            cursor = next;
        }

        if (!matched)
            return;

        const void*  data  = 0;
        unsigned int bytes = 0;
        reinterpret_cast<IncludeOpenFn>(vtbl[0])(include, 0x7FFFFFFF, fileName.c_str(),
                                                 m_source, &data, &bytes);

        const unsigned int head    = matchBegin;
        const unsigned int tailAt  = matchEnd;
        const unsigned int tailLen = m_sourceSize - tailAt;
        const unsigned int total   = bytes + head + tailLen;

        char* const grown = static_cast<char*>(::renddx9abi::engineAlloc(total));
        if (!grown)
        {
            reinterpret_cast<IncludeCloseFn>(vtbl[1])(include, data);
            return;
        }

        ::memcpy(grown, m_source, head);
        if (bytes && data)
            ::memcpy(grown + head, data, bytes);
        ::memcpy(grown + head + bytes, m_source + tailAt, tailLen);

        ::renddx9abi::engineFree(m_source);
        m_source     = grown;
        m_sourceSize = total;

        reinterpret_cast<IncludeCloseFn>(vtbl[1])(include, data);
    }
}


// Collects literal names and seeds uppercase defines with zero.
// collectDefines subsequently clears those defaults; preserve this ordering.

// PE 0x1003CF90
void MetaFx::collectLiterals()
{
    m_literalNames.clear();
    if (!m_source)
        return;

    const std::regex pattern(kLiteralPattern);
    const char* const text = m_source;

    unsigned int cursor = 0;
    unsigned int begin = 0, end = 0, rawEnd = 0, next = 0;
    while (nextLine(m_source, m_sourceSize, cursor, begin, end, rawEnd, next))
    {
        std::cmatch m;
        if (std::regex_search(text + begin, text + end, m, pattern))
        {
            const std::string name = vc71FromStd(m[1].str());
            m_literalNames.push_back(name);
            m_defines[vc71ToUpper(name)] = std::string("0");
        }
        cursor = next;
    }
}


// sub_1003CC50 - 213 bytes, `retn 8`.

//   defines.clear();                                   <- the erase-subtree plus the four header
//                                                         stores that relink it empty
//   while (pattern matches) defines[group1] = group2;

// RaShaderSTM.mfx carries no `//#define` line, so on the stock path this pass leaves the map empty.

// PE 0x1003CC50
void MetaFx::collectDefines()
{
    m_defines.clear();
    if (!m_source)
        return;

    const std::regex pattern(kDefinePattern);
    const char* const text = m_source;

    unsigned int cursor = 0;
    unsigned int begin = 0, end = 0, rawEnd = 0, next = 0;
    while (nextLine(m_source, m_sourceSize, cursor, begin, end, rawEnd, next))
    {
        std::cmatch m;
        if (std::regex_search(text + begin, text + end, m, pattern))
            m_defines[vc71FromStd(m[1].str())] = vc71FromStd(m[2].str());
        cursor = next;
    }
}


// Returns a cached macro array with a full null pair terminator.
// Its first build requires device caps and EffectManager to select forced shader model 1.4.

// PE 0x1003CA30
const void* MetaFx::shaderMacros()
{
    if (!m_macros.empty())
        return &m_macros[0];

    const bool forceOneFour = (deviceCapsPixelShaderMajor() < 2) || (effectManagerFlag40() != 0);

    m_macros.push_back("_FORCE_1_4_SHADERS_");
    m_macros.push_back(forceOneFour ? "1" : "0");
    m_macros.push_back(0);
    m_macros.push_back(0);

    return &m_macros[0];
}

// compile .mfx metadata, enumerate permutations/modifiers, then read fallbacks.
// GSDeferredShaderPermutation postpones generation; its default is false.
// PE 0x1003E3B0
bool MetaFx::generate()
{
    shaderMacros();

    ID3DXEffectCompiler* compiler = 0;
    ID3DXBuffer*         errors   = 0;

    // m_macros holds the {Name, Definition} pairs flat, null pair last.
    const long hr = createEffectCompiler(
        m_source, m_sourceSize,
        reinterpret_cast<const D3DXMACRO*>(m_macros.empty() ? 0 : &m_macros[0]),
        0,
        ::dice::hfe::dx9rend::shaderManagerEffectFlags(g_shaderManager),
        &compiler, &errors);

    if (hr != 0)
    {
        std::string caption("Failed to load Effect: ");
        caption += extractFileName(m_name);                           // sub_10075190

        // shipped defect: a failed compile can dereference a null diagnostic buffer.
        const char* const text = static_cast<const char*>(errors->GetBufferPointer());
#if defined(_WIN32)
        const bool retry = ::MessageBoxA(0, text, caption.c_str(), 0x50015) == 4;
        errors->Release();
        if (retry)
            return false; // parse() rereads the source before generating again.
        // shipped defect: Cancel continues into metadata reflection, even for a null compiler.
#else
        // [CHOSEN, not recovered] No Win32 modal UI: select the shipped Cancel arm.
        // PE 0x1003E453..0x1003E481 continues after Cancel, even with a null compiler.
        ::fprintf(stderr, "[CHOSEN, not recovered] dice::hfe::dx9rend::Ra::MetaFx::generate "
                          "compiler dialog: Cancel\n%s\n%s\n",
                  caption.c_str(), text);
        ::fflush(stderr);
        errors->Release();
#endif
    }

    MetaFxStruct generateStructs = { "GenerateStructs", {}, {} };
    readStringArrays(compiler, generateStructs, std::string(""));

    for (MetaFxStringArray::const_iterator it = generateStructs.values.begin();
         it != generateStructs.values.end(); ++it)
    {
        MetaFxStruct element = { *it, {}, {} };
        m_structs.push_back(element);          // sub_10041800, 56-byte stride
    }

    // <struct>Base, plus every literal's annotation of the same name
    for (std::vector<MetaFxStruct>::iterator it = m_structs.begin(); it != m_structs.end(); ++it)
        readStringArrays(compiler, *it, std::string("Base"));

    // the two generation lists
    MetaFxStruct permutations = { "GenerateShaderPermutations", {}, {} };
    readStringArrays(compiler, permutations, std::string(""));

    MetaFxStruct modifiers = { "GenerateShaderModifiers", {}, {} };
    readStringArrays(compiler, modifiers, std::string(""));

    // Without a setting, build every permutation immediately.
    const bool deferred =
        ::dice::hfe::dx9rend::readBoolSetting("GSDeferredShaderPermutation", false);

    m_generatedNames.resize(0, std::string());    // sub_1001D650

    for (MetaFxStringArray::const_iterator p = permutations.values.begin();
         p != permutations.values.end(); ++p)
    {
        m_generatedNames.push_back(*p);
        if (!deferred)
            generateShader(*p);

        for (MetaFxStringArray::const_iterator m = modifiers.values.begin();
             m != modifiers.values.end(); ++m)
        {
            // unk_101E1AE8 is the one-character string "|".
            std::string combined(*p);
            combined += "|";
            combined += *m;
            m_generatedNames.push_back(combined);

            if (!deferred)
            {
                // Rebuild the generated name for this call.
                std::string again(*p);
                again += "|";
                again += *m;
                generateShader(again);
            }
        }
    }

    readDeprecationList(compiler);
    compiler->Release();

    return true;
}


// Appends common string-array values and per-literal annotations to element.
// Annotation lookup uses the element name even when the literal parameter lookup is null.

void MetaFx::readStringArrays(void* effectCompiler, MetaFxStruct& element,
                              const std::string& suffix)
{
    ID3DXEffectCompiler* const fx = static_cast<ID3DXEffectCompiler*>(effectCompiler);

    std::string lookup(element.name);
    lookup += suffix;

    const D3DXHANDLE handle = fxGetParameterByName(fx, 0, lookup.c_str());
    if (handle)
    {
        unsigned int elements = 0;
        if (fxGetElementCount(fx, handle, elements))
        {
            for (unsigned int i = 0; i < elements; ++i)
            {
                const char* const text = fxGetElementString(fx, handle, i);
                element.values.push_back(std::string(text ? text : ""));
            }
        }
    }

    for (MetaFxStringArray::const_iterator it = m_literalNames.begin();
         it != m_literalNames.end(); ++it)
    {
        const D3DXHANDLE parameter  = fxGetParameterByName(fx, 0, it->c_str());
        const D3DXHANDLE annotation = fx->GetAnnotationByName(parameter, element.name.c_str());
        if (!annotation)
            continue;

        unsigned int elements = 0;
        if (!fxGetElementCount(fx, annotation, elements))
            continue;

        for (unsigned int j = 0; j < elements; ++j)
        {
            const char* const text = fxGetElementString(fx, annotation, j);
            element.annotations[*it].push_back(std::string(text ? text : ""));
        }
    }
}


// Reads flattened deprecation groups, using empty strings as row separators.
// Creates a first row even for an empty array and retains a trailing empty row.
// PE 0x1003DFE0 (listing read).
void MetaFx::readDeprecationList(void* effectCompiler)
{
    ID3DXEffectCompiler* const fx = static_cast<ID3DXEffectCompiler*>(effectCompiler);

    m_deprecation.clear();                      // sub_10040EA0(&deprecation, 0, vector<string>())

    const D3DXHANDLE handle = fxGetParameterByName(fx, 0, "DeprecationList");
    if (!handle)
        return;

    D3DXPARAMETER_DESC desc;
    ::memset(&desc, 0, sizeof(desc));
    fx->GetParameterDesc(handle, &desc);

    m_deprecation.push_back(MetaFxStringArray());

    // The shipped body does NOT test the GetParameterDesc HRESULT here, only the element count -
    // unlike readStringArrays, which tests both. Kept.
    std::vector<MetaFxStringArray>::size_type row = 0;
    for (unsigned int i = 0; i < desc.Elements; ++i)
    {
        const char* const text = fxGetElementString(fx, handle, i);
        if (text && ::strlen(text))
        {
            m_deprecation[row].push_back(std::string(text));
        }
        else
        {
            m_deprecation.push_back(MetaFxStringArray());
            ++row;
        }
    }
}


// Clears literal flags and enables the names listed in permutation.
// Unknown names log an error; map keys come from the registered literal list.

// PE 0x1003CD30
bool MetaFx::setPermutation(const std::string& permutation)
{
    for (MetaFxStringArray::const_iterator it = m_literalNames.begin();
         it != m_literalNames.end(); ++it)
    {
        m_literalValues[*it] = 0;
    }

    std::string rest(permutation);
    for (;;)
    {
        const std::string token = splitFirstToken(rest);
        if (token.size() == 0)
            break;

        MetaFxStringArray::const_iterator found = m_literalNames.begin();
        for (; found != m_literalNames.end(); ++found)
        {
            if (vc71Equal(*found, token))
                break;
        }

        if (found == m_literalNames.end())
        {
            ::renddx9abi::logf("RaShaderPermutator.cpp:495: Literal %snot found!!", token.c_str());
        }
        else
        {
            m_literalValues[*found] = 1;
        }
    }

    return true;
}

// emit one .fx permutation to disk or the effect manager's memory files.
// Keep first-seen member order: it assigns texture-set indices used by the vertex layout.
// Literal values are visited twice with operator[]; retain insertion of missing zero values.
// A newer generated file is left untouched when writing to disk.
bool MetaFx::generateShader(const std::string& permutation)
{
    std::string outName(m_name);
    outName += cleanPermutationName(permutation);

    std::string outFile(outName);
    outFile += ".fx";

    if (m_writeGeneratedFiles)
    {
        unsigned int low = 0;
        unsigned int high = 0;
        if (engineGetFileTime(outFile, low, high))
        {
            // 64-bit compare, high dword first, exactly as the shipped body does it.
            if (high > m_fileTimeHigh || (high == m_fileTimeHigh && low > m_fileTimeLow))
                return false;
        }
    }

    setPermutation(permutation);

    MetaFxStringArray texSets;
    std::ostringstream out;

    out << "//" << std::endl;
    {
        std::string title(m_name);
        title += cleanPermutationName(permutation);
        out << "// AutoGenerated shader file " << title << std::endl;
    }
    out << "//" << std::endl << std::endl;
    out << "// Shader setting " << std::endl;

    for (MetaFxStringArray::const_iterator it = m_literalNames.begin();
         it != m_literalNames.end(); ++it)
    {
        out << "#define " << vc71ToUpper(*it) << " " << m_literalValues[*it] << std::endl;
    }

    for (MetaFxStringArray::const_iterator it = m_literalNames.begin();
         it != m_literalNames.end(); ++it)
    {
        out << "static const dword " << vc71ToLower(*it) << " = " << m_literalValues[*it]
            << ";" << std::endl;
    }

    for (std::vector<MetaFxStruct>::const_iterator s = m_structs.begin();
         s != m_structs.end(); ++s)
    {
        MetaFxNameSet     seen;
        MetaFxStringArray members;

        // The element's own members first - `<name>Base[]`.
        for (MetaFxStringArray::const_iterator v = s->values.begin(); v != s->values.end(); ++v)
        {
            if (seen.find(*v) == seen.end())
            {
                seen.insert(*v);
                members.push_back(*v);
            }
        }

        // Then each enabled literal's annotation of this element's name.
        for (MetaFxStringArray::const_iterator l = m_literalNames.begin();
             l != m_literalNames.end(); ++l)
        {
            if (m_literalValues[*l] == 0)
                continue;

            const MetaFxAnnotationMap::const_iterator a = s->annotations.find(*l);
            if (a == s->annotations.end())
                continue;

            for (MetaFxStringArray::const_iterator v = a->second.begin();
                 v != a->second.end(); ++v)
            {
                if (seen.find(*v) == seen.end())
                {
                    seen.insert(*v);
                    members.push_back(*v);
                }
            }
        }

        if (members.empty())
            continue;

        out << std::endl << "string " << s->name << "[] = {" << std::endl;

        for (MetaFxStringArray::const_iterator v = members.begin(); v != members.end(); ++v)
        {
            if (v != members.begin())
                out << "," << std::endl;

            out << "\t\"" << *v << "\"";

            // find("2D") anywhere, then the first character must be 'T'; the name kept is
            // substr(1, size - 3), i.e. everything between the leading 'T' and the trailing "2D".
            const std::string member = stdFromVc71(*v);
            if (member.find("2D") != std::string::npos && !member.empty() && member[0] == 'T')
            {
                std::string generated("Tex");
                generated += vc71FromStd(member.substr(1, member.size() - 3));
                generated += "Ind";
                texSets.push_back(generated);
            }
        }

        out << std::endl << "};" << std::endl;
    }

    out << "#define NUM_LIGHTS 1 " << std::endl;
    out << "#define NUM_TEXSETS " << static_cast<unsigned int>(texSets.size()) << std::endl;

    for (unsigned int i = 0; i < texSets.size(); ++i)
        out << "#define " << texSets[i] << " " << i << std::endl;

    {
        std::string include("shaders/");
        include += extractFileName(m_name);       // sub_10075190 -> sub_10075600
        include += ".fx\"";
        out << std::endl << "#include \"" << include << std::endl;
    }

    const std::string   text  = out.str();
    const std::string  body  = vc71FromStd(text);

    if (m_writeGeneratedFiles)
    {
        ::renddx9abi::HostStream* const file = ::dice::hfe::fileManager->open(
            ::renddx9abi::makeHostString(outFile),
            static_cast<::renddx9abi::HostFileMode>(kFileModeWrite),
            static_cast<::renddx9abi::HostFileAccess>(kFileAccessWrite), 0);

        if (!file)
        {
            ::renddx9abi::logf("RaShaderPermutator.cpp:453: Failed to open %s for writing",
                               outFile.c_str());
            return false;
        }

        file->write(body.c_str(), static_cast<unsigned int>(body.size()));   // IStream +0x14; one .fx
        file->release();                          // IStream +0x08
    }
    else
    {
        effectManagerAddMemoryFile(outFile, body);
    }

    return true;
}


// Rebuilds bit-mask keys and cleaned names for each generated permutation.
// Cleans individual literal tokens before bit lookup and the full name before insertion.
// PE 0x1003CAC0 (the shipped body keeps the two working strings outside the loop and assigns them).
bool MetaFx::buildPermutationBits(std::map<unsigned int, std::string>& out,
                                  const char* const* bitNames)
{
    out.clear();

    for (MetaFxStringArray::const_iterator g = m_generatedNames.begin();
         g != m_generatedNames.end(); ++g)
    {
        int bits = 0;

        std::string rest(*g);
        for (;;)
        {
            const std::string token = splitFirstToken(rest);
            if (token.size() == 0)
                break;
            bits |= bitFromName(cleanPermutationName(token), bitNames);
        }

        // `insert`, NOT `operator[]`: the shipped body passes a fully built {bits, name} pair to
        // std::map::insert, which keeps an existing entry for a duplicate key. Several of the 190
        // generated names reduce to the same bit set, so which name survives depends on this, and
        // `operator[]` would let the last one win instead of the first.
        out.insert(std::make_pair(static_cast<unsigned int>(bits), cleanPermutationName(*g)));
    }

    return true;
}


// Removes every generated name from memory storage, including disk-only outputs.
// Missing entries are tolerated by the store.

// PE 0x1003C960
void MetaFx::dropGeneratedFiles()
{
    for (MetaFxStringArray::const_iterator g = m_generatedNames.begin();
         g != m_generatedNames.end(); ++g)
    {
        std::string path(m_name);
        path += cleanPermutationName(*g);
        path += ".fx";
        effectManagerRemoveMemoryFile(path);
    }
}


// Cross-unit construction helpers use the host size of MetaFx, whose STL layout varies.
unsigned int metaFxObjectSize()
{
    return static_cast<unsigned int>(sizeof(MetaFx));
}

MetaFx* metaFxConstruct(void* storage,
                        const std::string& name,
                        const std::string& extension,
                        bool writeGeneratedFiles)
{
    return new (storage) MetaFx(name, extension, writeGeneratedFiles);
}

void metaFxParse(MetaFx* self)
{
    self->parse();
}

} // namespace Ra
} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
