// Material implementation is attested to BundledMeshTemplate.cpp (100E9520/100E9840).
// [INFERRED] this split header/TU only separates the recovered material owner.
#ifndef BF2_GEOM_BUNDLEDMESHMATERIAL_H
#define BF2_GEOM_BUNDLEDMESHMATERIAL_H
#ifdef BF2_IMAGE_RENDDX9
#include "MeshTemplate.h"   // RaMaterialParameters
#include <cstddef>
#include <map>
#include <vector>
struct IDirect3DVertexDeclaration9;
namespace dice { namespace hfe {
namespace dx9rend { namespace Ra { struct RaVertexElementDesc; } }   // RendDX9/Ra/RaShader.h
namespace geom {
class BundledMeshTemplate;
class BundledMeshState;
// Retail allocation 0x1A0; the declaration map's 24-byte nodes come from 0x100493C0.
class BundledMeshMaterial
{
public:
    typedef std::vector<const ::dice::hfe::dx9rend::Ra::RaVertexElementDesc*> ElementList;
    typedef std::map<unsigned int, IDirect3DVertexDeclaration9*> DeclarationMap;

    BundledMeshMaterial(BundledMeshTemplate* templ, bool hasUv1, bool hasTangent);
    // The destructor body runs destroyMembers; destroy(flags) runs the destructor.
    ~BundledMeshMaterial();
    void destroyMembers();
    virtual void* destroy(unsigned char flags);
    virtual bool applyStreams(void* shader);
    virtual int getVertexCount();
    virtual int getBaseVertexIndex();
    virtual int getPrimitiveCount();
    virtual int getPrimitiveType();
    virtual int getVertexStart();
    virtual int getIndexOffset();
    virtual int getIndexStart();
    unsigned int m_dword04;                                  // +004
    BundledMeshTemplate* m_template;                          // +008
    BundledMeshState* m_state;                               // +00C
    // +010 Ra material block (RaMaterialParameters; 0x150 bytes on x86)
    alignas(RaMaterialParameters) unsigned char m_parameters[sizeof(RaMaterialParameters)];
    int m_kind;                                             // +160
    unsigned int m_vertexStart;                             // +164
    unsigned int m_indexStart;                              // +168
    unsigned int m_primitiveCount;                          // +16C
    unsigned int m_vertexCount;                             // +170
    unsigned int m_primitiveType;                           // +174
    void* m_vertexBuffer;                                   // +178 native D3D handle
    unsigned int m_vertexStride;                            // +17C
    void* m_indexBuffer;                                    // +180 native D3D handle
    ElementList m_elements;                                 // +184
    DeclarationMap m_declarations;                          // +194 shader build serial -> declaration
};
#if defined(_M_IX86)
static_assert(offsetof(BundledMeshMaterial,m_parameters)==0x10, "material parameters");
static_assert(offsetof(BundledMeshMaterial,m_kind)==0x160, "material kind");
#endif
// The material's Ra parameter block, typed.
inline RaMaterialParameters& bundledMeshMaterialParameters(BundledMeshMaterial* material)
{ return *reinterpret_cast<RaMaterialParameters*>(material->m_parameters); }
} } }
#endif
#endif
