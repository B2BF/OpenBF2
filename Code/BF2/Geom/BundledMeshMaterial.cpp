// Material ctor/apply/dtor are attested to Geom/BundledMeshTemplate.cpp.
// [INFERRED] split TU, kept beside its recovered owner. BF2 1.50 PE only.
#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/ThunkAbi.h"
#include "BundledMeshMaterial.h"
#include "BundledMeshTemplate.h"
#include "../RendDX9/Ra/RaShader.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../../RendDX9Abi/Frontier.h"
#include <cstdlib>
#include <new>

namespace dice { namespace hfe { namespace geom {
namespace {
namespace Ra = ::dice::hfe::dx9rend::Ra;
void pushDescriptor(BundledMeshMaterial::ElementList& v, const Ra::RaVertexElementDesc* value)
{
    v.push_back(value);
}

// off_101E2428 is the real abstract material base: one deleting destructor and
// eight _purecall slots, read fresh alongside100ED400. These are retail pure slots.
extern "C" int __cdecl _purecall(void);
void* RD9_THUNK destroyedMaterialDestroy(void* self RD9_THUNK_EDX, unsigned char flags);
void* const kDestroyedMaterialTable[] = {
    reinterpret_cast<void*>(&destroyedMaterialDestroy),
    reinterpret_cast<void*>(&_purecall), reinterpret_cast<void*>(&_purecall),
    reinterpret_cast<void*>(&_purecall), reinterpret_cast<void*>(&_purecall),
    reinterpret_cast<void*>(&_purecall), reinterpret_cast<void*>(&_purecall),
    reinterpret_cast<void*>(&_purecall), reinterpret_cast<void*>(&_purecall)
};
void* RD9_THUNK destroyedMaterialDestroy(void* self RD9_THUNK_EDX, unsigned char flags)
{
    *static_cast<void* const**>(self) = kDestroyedMaterialTable;
    if (flags & 1) ::renddx9abi::engineFree(self);
    return self;
}
}

//Descriptor selection is ordered, and the two template virtual queries
// are unconditional. Missing tangent support logs the shipped diagnostic.
// The element list and the declaration map (head node 0x100493C0) are default-constructed empty.
BundledMeshMaterial::BundledMeshMaterial(BundledMeshTemplate* templ, bool hasUv1, bool hasTangent)
{
    m_dword04 = 0;
    m_template = templ;
    m_state = 0;
    Ra::raMaterialParametersConstruct(m_parameters);
    m_kind = 0;
    m_vertexStart = m_indexStart = m_primitiveCount = m_vertexCount = m_primitiveType = 0;
    m_vertexBuffer = m_indexBuffer = 0;
    m_vertexStride = 0;
    const bool compressed = m_template->getCompressVertexData();
    const bool normal16 = m_template->slot_0x84();
    pushDescriptor(m_elements, compressed ? &Ra::g_desc_1023ED9C : &Ra::g_desc_1023EF04);
    pushDescriptor(m_elements, compressed ? (normal16 ? &Ra::g_desc_1023EF54 : &Ra::g_desc_1023EE14)
                                         : &Ra::g_desc_1023EF7C);
    pushDescriptor(m_elements, &Ra::g_desc_1023EEDC);
    pushDescriptor(m_elements, compressed ? &Ra::g_desc_1023EDEC : &Ra::g_desc_1023ED4C);
    if (hasUv1) pushDescriptor(m_elements, compressed ? &Ra::g_desc_1023EE8C : &Ra::g_desc_1023F01C);
    if (hasTangent)
        pushDescriptor(m_elements, compressed ? (normal16 ? &Ra::g_desc_1023EDC4 : &Ra::g_desc_1023EEB4)
                                             : &Ra::g_desc_1023EFF4);
    else
        ::renddx9abi::logf("BundledMeshTemplate.cpp:111: tl: Material created _without_ tangent support. "
                          "Something must be wrong! (because we haven't implemented this yet!!)");
}

//100ED400, then100ED3D0's deleting flag. The state is released by the template,
// before invoking this body; the material destructor itself does not release it.
// The containers are emptied at their shipped points; their storage goes with their own
// destructors after this body.
BundledMeshMaterial::~BundledMeshMaterial()
{
    destroyMembers();
}
void BundledMeshMaterial::destroyMembers()
{
    m_declarations.clear();
    m_elements.clear();
    Ra::raMaterialParametersDestroy(m_parameters);
    *reinterpret_cast<void* const**>(this) = kDestroyedMaterialTable;
}
void* BundledMeshMaterial::destroy(unsigned char flags)
{
    this->~BundledMeshMaterial();
    if (flags & 1) ::renddx9abi::engineFree(this);
    return this;
}

//starvation refuses before inserting. Bind order is stream, declaration,
// indices. Declarations belong to the process-wide cache and are not released here.
bool BundledMeshMaterial::applyStreams(void* shaderState)
{
    Ra::RaShader* const shader = static_cast<Ra::RaShader*>(shaderState);
    const unsigned int key = static_cast<unsigned int>(shader->m_buildSerial);
    DeclarationMap::iterator node = m_declarations.find(key);
    if (node == m_declarations.end())
    {
        Ra::RaStreamRequirements* required = Ra::raStreamRequirementsCreate(shader->m_requiredVertexElements);
        Ra::RaDeclarationBuilder builder;
        Ra::raDeclarationBuilderConstruct(builder);
        if (Ra::raStreamRequirementsConsume(required, m_elements))
            Ra::raDeclarationBuilderAddStream(builder, m_elements);
        if (Ra::raStreamRequirementsStarving(required))
        {
            char missing[1024];
            Ra::raStreamRequirementsDescribe(required, missing, sizeof(missing));
            ::renddx9abi::logf("BundledMeshTemplate.cpp:137: Starving streams in the shader %s, %s",
                              shader->m_name.c_str(), missing);
            Ra::raDeclarationBuilderDestroy(builder);
            Ra::raStreamRequirementsDestroy(required);
            return false;
        }
        m_declarations[key] =
            Ra::raVertexDeclarationCached(Ra::raDeclarationBuilderFinish(builder), Ra::raRendererDevice());
        node = m_declarations.find(key);
        Ra::raDeclarationBuilderDestroy(builder);
        Ra::raStreamRequirementsDestroy(required);
    }
    dx9rend::stateManagerSetStreamSource(0, static_cast<IDirect3DVertexBuffer9*>(m_vertexBuffer), 0, m_vertexStride);
    dx9rend::stateManagerSetVertexDeclaration(node->second);
    dx9rend::stateManagerSetIndices(static_cast<IDirect3DIndexBuffer9*>(m_indexBuffer));
    return true;
}
int BundledMeshMaterial::getVertexCount() { return m_vertexCount; }
int BundledMeshMaterial::getBaseVertexIndex() { return -1; }
int BundledMeshMaterial::getPrimitiveCount() { return m_primitiveCount; }
int BundledMeshMaterial::getPrimitiveType() { return m_primitiveType; }
int BundledMeshMaterial::getVertexStart() { return m_vertexStart; }
int BundledMeshMaterial::getIndexOffset() { return 0; }
int BundledMeshMaterial::getIndexStart() { return m_indexStart; }

} } }
#endif
