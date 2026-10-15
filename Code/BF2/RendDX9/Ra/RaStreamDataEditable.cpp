
// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\RendDX9\Ra\
//           RaStreamDataEditable.cpp" (RendDX9.dll, the sub_100490D0 diagnostic, line 69).
// Contents: dice::hfe::dx9rend::Ra::RaStreamDataEditable - see the header. Every body below is
//           read from RendDX9.dll 1.50; the stream-requirement, declaration-builder
//           and declaration-cache operations it performs are the ones RaVertexStreams.cpp already
//           recovered for StaticMeshMaterial::applyStreams, called the same way.

#ifdef BF2_IMAGE_RENDDX9

// Keep the VC7.1/x86 renderer carriers inside the same image boundary as their bodies.
// The native dedicated server neither constructs nor consumes this renderer-only class.
#include "RaStreamDataEditable.h"

#include "RaShader.h"
#include "../RendDX9Base.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include <string>

#include <cstring>

namespace dice {
namespace hfe {
namespace dx9rend {
namespace Ra {

namespace {

typedef std::vector<const RaVertexElementDesc*>               RaElementList;
typedef std::map<unsigned int, IDirect3DVertexDeclaration9*> RaDeclarationCache;

} // anonymous namespace

// sub_10048FF0 + sub_10048C70.  The element vector and the declaration cache (sub_100493C0's head
// node) are constructed empty before the body.
RaStreamDataEditable::RaStreamDataEditable(unsigned int vertexCapacity, unsigned int indexCapacity,
                                           unsigned int stride,
                                           const std::vector<const RaVertexElementDesc*>& elements,
                                           IDirect3DDevice9* device)
{
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
    raMaterialParametersConstruct(m_materialParameters);
    m_device = device;
    m_stride = stride;
    m_unresolved16C = 0;
    m_locked = 0;
    m_lockedVertices = 0;
    m_lockedIndices = 0;
    m_indexSize = 2;
    m_indexCount = 0;
    m_vertexCount = 0;
    m_vertexCapacity = vertexCapacity;
    m_indexCapacity = indexCapacity;
    // sub_10049B80 - m_elements.insert(end, elements.begin(), elements.end()).
    m_elements.insert(m_elements.end(), elements.begin(), elements.end());
    // sub_10048C70 - the two buffers, when a device exists.
    if (m_device)
    {
        m_device->CreateVertexBuffer(m_stride * m_vertexCapacity, D3DUSAGE_WRITEONLY, 0,
                                     D3DPOOL_MANAGED, &m_vertexBuffer, 0);   // device +0x68
        // sub_10047E70(1, vb, bytes, "", 0, 1) - the resource tracker, not reproduced.
        m_device->CreateIndexBuffer(m_indexSize * m_indexCapacity, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                    D3DPOOL_MANAGED, &m_indexBuffer, 0);     // device +0x6C
        // sub_10047E70(2, ib, bytes, "", &vb, 0) - the resource tracker, not reproduced.
    }
}

// sub_10048F00 - unlock if locked, release both buffers, drop the cache and the elements, destroy
// the material block.  Every teardown path (destroy, OverGrowthCellMesh::destroy) runs this and then
// frees the storage without a C++ destructor, so this ends the two containers' lifetimes itself.
void RaStreamDataEditable::destroyMembers()
{
    if (m_locked)
    {
        m_lockedVertices = 0;
        m_lockedIndices = 0;
        m_locked = 0;
        if (m_indexBuffer && m_vertexBuffer)
        {
            m_indexBuffer->Unlock();
            m_vertexBuffer->Unlock();
        }
    }
    if (m_vertexBuffer)
    {
        // sub_10047EE0(1, vb) - the resource tracker, not reproduced.
        m_vertexBuffer->Release();
    }
    if (m_indexBuffer)
    {
        // sub_10047EE0(2, ib) - the resource tracker, not reproduced.
        m_indexBuffer->Release();
    }
    m_declarations.~RaDeclarationCache();      // sub_10049900 over the nodes, then the head
    m_elements.~RaElementList();
    raMaterialParametersDestroy(m_materialParameters);
}

void* RaStreamDataEditable::destroy(unsigned char flags)
{
    destroyMembers();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}

// sub_100490D0 - the declaration for this shader, built once per build serial from the shader's
// required elements against this object's single stream; then stream 0, the declaration, the
// indices. A starving shader is logged and refused.
bool RaStreamDataEditable::applyStreams(void* shaderState)
{
    RaShader* const shader = static_cast<RaShader*>(shaderState);
    const unsigned int key = static_cast<unsigned int>(shader->m_buildSerial);        // +0x174
    RaDeclarationCache::iterator node = m_declarations.find(key);
    if (node == m_declarations.end())
    {
        RaStreamRequirements* const required =
            raStreamRequirementsCreate(shader->m_requiredVertexElements);              // sub_10049330 + sub_1004A0F0
        RaDeclarationBuilder builder;
        raDeclarationBuilderConstruct(builder);
        if (raStreamRequirementsConsume(required, m_elements))
            raDeclarationBuilderAddStream(builder, m_elements);
        if (raStreamRequirementsStarving(required))
        {
            char missing[1024];
            raStreamRequirementsDescribe(required, missing, sizeof(missing));
            ::renddx9abi::logf("RaStreamDataEditable.cpp:69 Starving streams in the shader %s, %s",
                               shader->m_name.c_str(), missing);
            raDeclarationBuilderDestroy(builder);
            raStreamRequirementsDestroy(required);
            return false;
        }
        IDirect3DDevice9* const device = raRendererDevice();                           // renderer +0x0C
        IDirect3DVertexDeclaration9*& inserted = m_declarations[key];
        inserted = raVertexDeclarationCached(raDeclarationBuilderFinish(builder), device);
        node = m_declarations.find(key);
        raDeclarationBuilderDestroy(builder);
        raStreamRequirementsDestroy(required);
    }
    stateManagerSetStreamSource(0, m_vertexBuffer, 0, m_stride);                       // +0x58
    stateManagerSetVertexDeclaration(node->second);                                     // +0x5C
    stateManagerSetIndices(m_indexBuffer);                                             // +0x54
    return true;
}

int RaStreamDataEditable::getVertexCount() const    { return static_cast<int>(m_vertexCount); }
int RaStreamDataEditable::getIndexCount() const     { return static_cast<int>(m_indexCount); }
int RaStreamDataEditable::getPrimitiveCount() const { return static_cast<int>(m_indexCount) / 3; }
int RaStreamDataEditable::getPrimitiveType() const  { return 4; }
int RaStreamDataEditable::getVertexStart() const    { return 0; }
int RaStreamDataEditable::getMinIndex() const       { return 0; }
int RaStreamDataEditable::getIndexStart() const     { return 0; }

// sub_10048DA0 - restart the fill and lock both buffers whole.
void RaStreamDataEditable::lock()
{
    const bool noIndices = m_indexBuffer == 0;
    m_locked = 1;
    m_vertexCount = 0;
    m_indexCount = 0;
    if (!noIndices && m_vertexBuffer)
    {
        m_vertexBuffer->Lock(0, 0, &m_lockedVertices, 0);                 // +0x2C
        m_indexBuffer->Lock(0, 0, &m_lockedIndices, 0);
    }
}


void RaStreamDataEditable::unlock()
{
    IDirect3DIndexBuffer9* const indices = m_indexBuffer;
    m_lockedVertices = 0;
    m_lockedIndices = 0;
    m_locked = 0;
    if (indices && m_vertexBuffer)
    {
        indices->Unlock();                                                // +0x30
        m_vertexBuffer->Unlock();
    }
}

// sub_10048E40 - copy the vertices after the ones already appended and the indices rebased by
// that count; false when either capacity would overflow.
bool RaStreamDataEditable::append(const void* vertices, int stride, int vertexCount,
                                  const unsigned short* indices, int indexCount)
{
    if (static_cast<unsigned int>(indexCount) + m_indexCount > m_indexCapacity)
        return false;
    const unsigned int base = m_vertexCount;
    if (base + static_cast<unsigned int>(vertexCount) > m_vertexCapacity)
        return false;
    if (m_indexBuffer && m_vertexBuffer)
    {
        ::memcpy(static_cast<unsigned char*>(m_lockedVertices) + stride * base, vertices,
                 static_cast<size_t>(vertexCount) * stride);
        unsigned short* const out = static_cast<unsigned short*>(m_lockedIndices);
        for (int i = 0; i < indexCount; ++i)
            out[i + m_indexCount] = static_cast<unsigned short>(static_cast<unsigned short>(m_vertexCount) + indices[i]);
    }
    m_vertexCount += static_cast<unsigned int>(vertexCount);
    m_indexCount += static_cast<unsigned int>(indexCount);
    return true;
}

} // namespace Ra
} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
