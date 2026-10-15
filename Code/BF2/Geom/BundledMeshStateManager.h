// Path: [INFERRED] header beside the attested Geom/BundledMeshStateManager.cpp.
// RendDX9.dll 1.50 only; no concrete renderer counterpart exists in the dedicated ELF.
#ifndef GEOM_BUNDLEDMESHSTATEMANAGER_H
#define GEOM_BUNDLEDMESHSTATEMANAGER_H

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include <string>
#include <cstddef>
#include <vector>

namespace dice { namespace hfe {
namespace dx9rend { struct EngineTexture; }   // RendDX9/Texture.h
namespace geom {

// off_10220C94: IBase, then scalar deleting destructor. Retail allocation is 0x40.
// Field names are inferred from the feature tests in 0x10192BF0 and their draw consumers.
class BundledMeshState : public IBase
{
public:
    typedef std::vector<dx9rend::EngineTexture*> TextureList;

    BundledMeshState();
    ~BundledMeshState();                                     // 0x10192F50, nonvirtual
    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual void* destroy(unsigned char flags);

    void deriveFeatures();

    int m_refCount;                                         // +04
    std::string m_name;                                    // +08
    TextureList m_textures;                                 // +24
    unsigned char m_animated;                               // +34
    unsigned char m_normalMap;                              // +35, texture 1 ends in _b.dds
    unsigned char m_extraMap;                               // +36, extra non-SpecularLUT texture
    unsigned char m_envMap;                                 // +37
    unsigned char m_colorMapGloss;                          // +38
    unsigned char m_feature39;                              // +39, !colorMapGloss && !normalMap
    unsigned char m_alphaTest;                              // +3A, gated by colorMapGloss
    unsigned char m_alphaOne;                               // +3B
    unsigned char m_cockpit;                                // +3C
    unsigned char m_noHemiLight;                            // +3D
    unsigned char m_padding3E[2];                           // untouched by ctor
};

// off_10220C7C: the complete six-slot manager table, retail allocation 0x38.
class BundledMeshStateManager : public IBase
{
public:
    BundledMeshStateManager();
    ~BundledMeshStateManager();                             // 0x10192E50, nonvirtual
    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual void setEnabled(bool enabled);
    virtual bool getEnabled() const;

    // Wrapper 0x101935E0 -> body 0x10192FF0. The effect argument is never read.
    BundledMeshState* read(void* effect, const std::string& templatePath,
                          ::renddx9abi::HostNonlinearStream* stream,
                          bool arg4, bool skipTextures);
    BundledMeshState* find(const BundledMeshState& state) const;
    void remove(const BundledMeshState* state);

    int m_refCount;                                         // +04
    unsigned char m_enabled;                               // +08
    unsigned char m_enabled9;                              // +09, draw's second-pass gate
    unsigned char m_padding0A[2];                           // untouched by ctor
    std::vector<BundledMeshState*> m_cache;                 // +0C, non-owning State* cache
    // Explicit lifetime preserves the retail destructor's string-before-vector-buffer order.
    union { std::string m_texturePrefix; };                 // +1C
};

extern BundledMeshStateManager* g_bundledMeshStateManager;    // dword_102597F4
// Template ctor 0x100EA631 and dtor 0x100EA828 own the singleton references.
void bundledMeshStateManagerRetain();
void bundledMeshStateManagerRelease();

} } }
#endif
#endif
