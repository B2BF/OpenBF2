// Path: [INFERRED] header beside RaShaderPermutator.cpp, attested at RendDX9 0x101E1940.
// MetaFx parses .mfx data and generates shader permutations; class name is [INFERRED].
// Recovered from RendDX9.dll; native STL storage does not reproduce the retail 0xC0 layout.
// Use accessors, never retail byte offsets. File-watch argument types remain [UNRESOLVED].

#ifndef BF2_RENDDX9_RA_RASHADERPERMUTATOR_H
#define BF2_RENDDX9_RA_RASHADERPERMUTATOR_H

#ifdef BF2_IMAGE_RENDDX9

// HostFileAbi.h is what supplies dice::hfe::IBase in both plugin-ABI flavours: the retail flavour
// gets it from HfeAbi.h, the native one from Code/BF2/Core/IBase.h via IFileSystem.h.
#include "../../../RendDX9Abi/HostFileAbi.h"
#include <string>

#include <map>
#include <set>
#include <vector>

namespace dice {
namespace hfe {
namespace dx9rend {
namespace Ra {


// The containers below need an ordering, and the shipped ones use the ordinary byte-wise `operator<`
// that VC7.1's std::string supplies - RendDX9's inlined comparisons all land on
// `std::operator<<char>(std::string const&, std::string const&)`, which is lexicographic strcmp
// order on the characters. This comparator is that ordering and nothing more.
// [CHOSEN, not recovered] as a type - the ordering it implements is recovered.

struct ShippedStringLess
{
    bool operator()(const std::string& a, const std::string& b) const;
};

typedef std::vector<std::string>                          MetaFxStringArray;
typedef std::map<std::string, MetaFxStringArray, ShippedStringLess> MetaFxAnnotationMap;
typedef std::map<std::string, std::string, ShippedStringLess>      MetaFxDefineMap;
typedef std::map<std::string, unsigned long, ShippedStringLess>     MetaFxLiteralMap;
typedef std::set<std::string, ShippedStringLess>                    MetaFxNameSet;


// Generated-struct definition: base members plus feature-specific literal annotations.

struct MetaFxStruct
{
    std::string        name;
    MetaFxStringArray   values;
    MetaFxAnnotationMap annotations;
};

// Parsed .mfx owned by EffectManager's cache. Allocate sizeof(MetaFx) on the engine heap;
// release() frees through engineFree. Native STL changes size from retail's 0xC0.
// Constructor name is the normalized path without extension; extension is supplied separately.
class MetaFx : public IBase
{
public:
    // 0x1003E110. `name` is the path with its extension stripped and its separators normalised;
    // `extension` is what was stripped ("mfx"); `writeGeneratedFiles` is the loader's
    // `settingByte == 0` - see the loader note in the .cpp.
    MetaFx(const std::string& name, const std::string& extension, bool writeGeneratedFiles);

    // Non-virtual and public: the shipped object has no destructor slot (IBase has
    // none either) and release() is what reaches it.
    ~MetaFx();

    // the shipped vtable, in slot order
    virtual void  addRef();                                   // +0x00
    virtual int   getRef();                                   // +0x04
    virtual int   release();                                  // +0x08
    virtual void* queryInterface(unsigned int iid) const;     // +0x0C
    // +0x10  0x1003EF10. The FileChangeManager notification. Both arguments are ignored by the
    // shipped body; their types are [UNRESOLVED] and only the arity (`retn 8`) is read, so they
    // are declared as opaque pointers and never dereferenced.
    virtual bool  onFileChanged(void* unknown1, void* unknown2);

    // the parse
    // 0x1003EEF0 - `do { readSource(); done = generate(); } while (!done);`
    bool parse();

    // readers, for the consumers that must not index this object by offset
    const std::string&      getName() const             { return m_name; }
    const std::string&      getExtension() const        { return m_extension; }
    const MetaFxStringArray& getGeneratedNames() const   { return m_generatedNames; }
    const std::vector<MetaFxStringArray>& getDeprecationList() const { return m_deprecation; }
    const std::vector<MetaFxStruct>&      getStructs() const         { return m_structs; }
    const MetaFxLiteralMap&  getLiteralValues() const    { return m_literalValues; }
    const MetaFxStringArray& getLiteralNames() const     { return m_literalNames; }

    // Clears every literal to 0, then sets the ones named in `permutation` (a
    // '|'-separated list) to 1. Public because sub_1003D080 is not its only caller.
    bool setPermutation(const std::string& permutation);

    // Rebuilds out as bit mask -> cleaned permutation name using NUL-terminated bitNames.
    // Duplicate masks collapse on insertion and traversal remains numerically ordered.
    bool buildPermutationBits(std::map<unsigned int, std::string>& out,
                              const char* const* bitNames);

    // Drops every generated .fx from the EffectManager's in-memory file store.
    void dropGeneratedFiles();

    // Generates ONE permutation's .fx. Public for the same reason setPermutation is.
    bool generateShader(const std::string& permutation);

private:
    MetaFx(const MetaFx&);
    MetaFx& operator=(const MetaFx&);

    bool readSource();    // the first half of the parse loop
    bool generate();      // the second half

    // The three text passes readSource() drives, in the order it drives them. All three are
    // file-scope helpers in the shipped object (they take the PME as an argument and `this` in
    // ecx); they are members here because every one of them writes this object's containers.
    void expandIncludes();    // splice every  #include "...fx"  in place
    void collectLiterals();   // the  const dword <name> : literal  pass
    void collectDefines();    // the  //#define <name> <value>     pass

    // Fills `element` from the compiled effect: `<element.name><suffix>[]` into element.values,
    // and each literal's annotation of the same name into element.annotations. 0x1003DE10.
    void readStringArrays(void* effectCompiler, MetaFxStruct& element, const std::string& suffix);

    // reads DeprecationList[] into m_deprecation.
    void readDeprecationList(void* effectCompiler);

    // builds the D3DXMACRO array once and caches it in m_macros.
    const void* shaderMacros();

    // members, in the shipped order; the comment is the shipped offset
    int                            m_refCount;        // +0x04
    std::string                   m_name;            // +0x08
    std::string                   m_extension;       // +0x24
    char*                          m_source;          // +0x40
    unsigned int                   m_sourceSize;      // +0x44
    unsigned int                   m_fileTimeLow;     // +0x48  the ctor does NOT initialise
    unsigned int                   m_fileTimeHigh;    // +0x4C  these two; readSource always does
    MetaFxDefineMap                m_defines;         // +0x50
    MetaFxStringArray              m_literalNames;    // +0x5C
    MetaFxLiteralMap               m_literalValues;   // +0x6C
    std::vector<const char*>       m_macros;          // +0x78
    std::vector<MetaFxStruct>      m_structs;         // +0x88
    MetaFxStringArray              m_generatedNames;  // +0x98
    std::vector<MetaFxStringArray> m_deprecation;     // +0xA8
    bool                           m_writeGeneratedFiles; // +0xB8
};

} // namespace Ra
} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9

#endif // BF2_RENDDX9_RA_RASHADERPERMUTATOR_H
