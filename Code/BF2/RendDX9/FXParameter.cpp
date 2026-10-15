// Owns shader-parameter handles, types, dimensions, and value application.
// ShaderParameter wrappers refer to these owners, allowing names before effects exist.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9Abi/Frontier.h"
#include "ShaderParameter.h"
#include "Texture.h"                       // EngineTexture - the texture value unwrap

#include <new>

namespace dice {
namespace hfe {
namespace dx9rend {

// StateManager.cpp - the sparse-matrix arm's bone-palette upload (SM vslot +0x38).
void stateManagerSetVertexShaderConstantF(unsigned int reg, const float* data,
                                          unsigned int count);

// setD3DXParam's stack locals match D3DXPARAMETER_DESC field for field; StructMembers and Flags
// are the two it never reads.
#if defined(_M_IX86)
static_assert(sizeof(D3DXPARAMETER_DESC) == 44, "D3DXPARAMETER_DESC is 44 bytes on x86");
#endif


// Effect-owned reflected parameter with semantic, type and array metadata.
// The effect pointer is borrowed; setD3DXParam initializes the handle and description.

class FXParameterImpl final : public ParameterOwner
{
public:
    FXParameterImpl(void* effect, void* d3dxHandle);

    int  getRef() const { return m_refCount; }
    int  addRef() { return ++m_refCount; }
    int  release();
    void* getD3DXParam() const { return m_handle; }          // +0x0C,
    void setD3DXParam(void* handle);
    bool isParameterUsed() const;                            // +0x14,
    void setValue(const void* data, unsigned int count);
    void setBool(unsigned char value);
    void setInt(int value);
    void setFloat(float value);                            // +0x24,
    void setMatrixPointerArray(const void* const* matrices,
                               unsigned int count);         // +0x28,
    unsigned int getTypeCode() const { return m_typeCode; } // +0x2C,

private:
    int           m_refCount;     // +0x04
    void*         m_effect;       // +0x08
    void*         m_handle;       // +0x0C
    unsigned int  m_typeCode;     // +0x10
    std::string  m_semantic;     // +0x14 .. +0x2F
    unsigned int  m_bytes;        // +0x30
    unsigned int  m_bytesPerElem; // +0x34
    unsigned int  m_elements;     // +0x38
    unsigned int  m_rows;         // +0x3C
    unsigned int  m_columns;      // +0x40
    unsigned char m_sparseArray;  // +0x44
    unsigned char m_pad45[3];
    unsigned int  m_arrayStart;   // +0x48
};

// Effect.cpp - the owning effect's compiled ID3DXEffect, or null before bind.
void* effectD3dxHandle(void* effect);
bool effectIsParameterUsed(void* effect, void* parameter);

namespace {

void logUnknownDatatype(int line)
{
    ::renddx9abi::logf("FXParameter.cpp:%d Unknown datatype in FXParameterImpl::setD3DXParam()",
                       line);
}

} // anonymous namespace

// sub_100219A0, read in full.
FXParameterImpl::FXParameterImpl(void* effect, void* d3dxHandle)
    : m_refCount(1)
    , m_effect(effect)
    , m_handle(0)
    , m_typeCode(0x80000000u)   // the constructor's sentinel; setD3DXParam replaces it
    , m_bytes(0)
    , m_bytesPerElem(0)
    , m_elements(0)
    , m_rows(0)
    , m_columns(0)
    , m_sparseArray(0)
    , m_arrayStart(0)
{
    m_pad45[0] = 0; m_pad45[1] = 0; m_pad45[2] = 0;
    setD3DXParam(d3dxHandle);
}


// Releases this parameter and its semantic string at zero references.
// The effect pointer is borrowed; the effect owns the parameter, not vice versa.
int FXParameterImpl::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        m_semantic.~basic_string();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// +0x14, 0x100215C0. A null D3DX parameter is never used; otherwise the Effect checks it against
// the current technique (or answers true when no technique is selected).
bool FXParameterImpl::isParameterUsed() const
{
    return m_handle && effectIsParameterUsed(m_effect, m_handle);
}

void FXParameterImpl::setD3DXParam(void* handle)
{
    m_handle = handle;
    if (!handle)
        return;

    ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect));
    if (!fx)
        return;     // no compiled effect yet - the constructor's defaults stand

    D3DXPARAMETER_DESC desc;
    for (unsigned i = 0; i < sizeof(desc) / 4; ++i)
        reinterpret_cast<unsigned int*>(&desc)[i] = 0;

    fx->GetParameterDesc(::renddx9abi::d3dxHandle(handle), &desc);

    m_elements    = desc.Elements;
    m_rows        = desc.Rows;
    m_columns     = desc.Columns;
    m_bytes       = desc.Bytes;
    m_bytesPerElem = desc.Bytes / (desc.Elements ? desc.Elements : 1);   // clamped, not checked

    m_semantic    = desc.Semantic ? desc.Semantic : "";
    m_sparseArray = 0;
    m_arrayStart  = 0;

    // Both annotations must be present; the original tests them together and reads neither unless
    // both are found.
    if (desc.Annotations == 2)
    {
        const D3DXHANDLE sparse = fx->GetAnnotationByName(::renddx9abi::d3dxHandle(handle),
                                                          "sparseArray");
        const D3DXHANDLE start  = fx->GetAnnotationByName(::renddx9abi::d3dxHandle(handle),
                                                          "arrayStart");
        if (sparse && start)
        {
            BOOL flag = 0;
            fx->GetBool(sparse, &flag);
            m_sparseArray = flag ? 1 : 0;

            INT base = 0;
            fx->GetInt(start, &base);
            m_arrayStart = static_cast<unsigned int>(base);
        }
    }

    // The class/type switch. Codes are the original's literals.
    switch (desc.Class)
    {
    case D3DXPC_SCALAR:
        m_typeCode = 256;
        if (desc.Type == D3DXPT_BOOL)       m_typeCode = 260;
        else if (desc.Type == D3DXPT_INT)   m_typeCode = 257;
        else if (desc.Type == D3DXPT_FLOAT) m_typeCode = 258;
        else logUnknownDatatype(93);
        break;
    case D3DXPC_VECTOR:
        m_typeCode = 512;
        if (desc.Type == D3DXPT_INT)        m_typeCode = 513;
        else if (desc.Type == D3DXPT_FLOAT) m_typeCode = 514;
        else logUnknownDatatype(114);
        break;
    case D3DXPC_MATRIX_COLUMNS:  // falls through to the MATRIX_ROWS arm after complaining
        ::renddx9abi::logf("FXParameter.cpp:121 D3DXPC_MATRIX_COLUMNS: Explicitly support this?");
        // fall through
    case D3DXPC_MATRIX_ROWS:
        m_typeCode = 1024;
        if (desc.Type == D3DXPT_INT)        m_typeCode = 1025;
        else if (desc.Type == D3DXPT_FLOAT) m_typeCode = 1026;
        else logUnknownDatatype(137);
        break;
    case D3DXPC_OBJECT:  // only one type is recognised, and the code is left unset otherwise
        if (desc.Type == D3DXPT_TEXTURE)
            m_typeCode = 2056;
        break;
    case D3DXPC_STRUCT:
        m_typeCode = 0x10000;
        break;
    default:
        ::renddx9abi::logf("FXParameter.cpp:150 Unknown dataclass in "
                           "FXParameterImpl::setD3DXParam()");
        break;
    }
}

namespace {

// unk_1023C6D0 - the sparse-matrix staging buffer, file-scope in the shipped image. Its extent is
// [INFERRED] from the .bss gap to the next named global (0x1023D08C), 2492 bytes = 51 matrices of
// 12 floats; nothing recovered establishes the declared size.
float s_sparseMatrixStaging[624];

} // anonymous namespace


// Uploads data according to reflected type; zero count uses the parameter's element count.
// Sparse matrices are transposed into three float4 rows per matrix for direct register upload.
// Requires a compiled owner effect.

void FXParameterImpl::setValue(const void* data, unsigned int count)
{
    ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect));
    const D3DXHANDLE handle = ::renddx9abi::d3dxHandle(m_handle);
    if (!count)
        count = m_elements;

    const unsigned int typeClass = m_typeCode & 0xFFFFFF00u;

    if (typeClass > 0x400u)
    {
        if (typeClass != 0x800u)
        {
            if (typeClass == 0x10000u)
            {
                fx->SetValue(handle, data, m_bytes);
                return;
            }
            ::renddx9abi::logf("FXParameter.cpp:357 Data type is unknown for parameter.");
            return;
        }
        // object/texture (2056): unwrap the EngineTexture (its D3D base texture at +0x08) and hand
        // D3DX the raw pointer - sub_10014930.
        IDirect3DBaseTexture9* d3dTexture = 0;
        if (data)
            d3dTexture = static_cast<const EngineTexture*>(data)->baseTexture;   // +0x08
        fx->SetTexture(handle, d3dTexture);
        return;
    }

    switch (typeClass)
    {
    case 0x400u:   // matrix
        if (m_typeCode & 4u)
        {
            ::renddx9abi::logf("FXParameter.cpp:295 Investigate more!");
            return;
        }
        if (m_typeCode & 1u)
        {
            ::renddx9abi::logf("FXParameter.cpp:299 Investigate more!");
            return;
        }
        if (m_typeCode & 2u)
        {
            if (m_elements)
            {
                if (m_sparseArray)
                {
                    const float* src = static_cast<const float*>(data);
                    float* dst = s_sparseMatrixStaging;
                    for (unsigned int i = 0; i < count; ++i, src += 16, dst += 12)
                    {
                        dst[0] = src[0];  dst[1] = src[4];  dst[2]  = src[8];  dst[3]  = src[12];
                        dst[4] = src[1];  dst[5] = src[5];  dst[6]  = src[9];  dst[7]  = src[13];
                        dst[8] = src[2];  dst[9] = src[6];  dst[10] = src[10]; dst[11] = src[14];
                    }
                    stateManagerSetVertexShaderConstantF(m_arrayStart, s_sparseMatrixStaging,
                                                         count * m_columns);
                    return;
                }
                fx->SetArrayRange(handle, 0, count);
                fx->SetMatrixArray(handle, static_cast<const D3DXMATRIX*>(data), count);
                return;
            }
            fx->SetMatrix(handle, static_cast<const D3DXMATRIX*>(data));
            return;
        }
        ::renddx9abi::logf("FXParameter.cpp:326 Unknown parameter type");
        return;

    case 0x100u:   // scalar
        if (m_typeCode & 4u)
        {
            if (m_elements)
            {
                fx->SetArrayRange(handle, 0, count);
                fx->SetBoolArray(handle, static_cast<const BOOL*>(data), count);
            }
            else
            {
                fx->SetBool(handle, *static_cast<const BOOL*>(data));
            }
            return;
        }
        if (m_typeCode & 1u)
        {
            if (m_elements)
            {
                fx->SetArrayRange(handle, 0, count);
                fx->SetIntArray(handle, static_cast<const INT*>(data), count);
            }
            else
            {
                fx->SetInt(handle, *static_cast<const INT*>(data));
            }
            return;
        }
        if (m_typeCode & 2u)
        {
            if (m_elements)
            {
                fx->SetArrayRange(handle, 0, count);
                fx->SetFloatArray(handle, static_cast<const FLOAT*>(data), count);
            }
            else
            {
                fx->SetFloat(handle, *static_cast<const FLOAT*>(data));
            }
            return;
        }
        ::renddx9abi::logf("FXParameter.cpp:256 Unknown parameter type");
        return;

    case 0x200u:   // vector
        if (m_typeCode & 4u)
        {
            ::renddx9abi::logf("FXParameter.cpp:267 Investigate more!");
            return;
        }
        if (m_typeCode & 1u)
        {
            ::renddx9abi::logf("FXParameter.cpp:271 Investigate more!");
            return;
        }
        if (m_typeCode & 2u)
        {
            if (m_elements)
            {
                fx->SetArrayRange(handle, 0, count);
                fx->SetVectorArray(handle, static_cast<const D3DXVECTOR4*>(data), count);
            }
            else
            {
                fx->SetVector(handle, static_cast<const D3DXVECTOR4*>(data));
            }
            return;
        }
        ::renddx9abi::logf("FXParameter.cpp:284 Unknown parameter type");
        return;

    default:
        ::renddx9abi::logf("FXParameter.cpp:357 Data type is unknown for parameter.");
        return;
    }
}

// +0x1C, straight through to ID3DXBaseEffect::SetBool.
void FXParameterImpl::setBool(unsigned char value)
{
    ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect));
    fx->SetBool(::renddx9abi::d3dxHandle(m_handle), value);
}

// +0x20, straight through to ID3DXBaseEffect::SetInt.
void FXParameterImpl::setInt(int value)
{
    ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect));
    fx->SetInt(::renddx9abi::d3dxHandle(m_handle), value);
}

// +0x24, straight through to ID3DXBaseEffect::SetFloat.
void FXParameterImpl::setFloat(float value)
{
    ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect));
    fx->SetFloat(::renddx9abi::d3dxHandle(m_handle), value);
}

// +0x28, 0x10021640. This overload accepts pointers to matrices, unlike +0x18's contiguous
// matrix-array arm. The sparse path repacks the first three transposed rows exactly like +0x18;
// the ordinary path selects the range before calling D3DX SetMatrixPointerArray (+0xA8).
void FXParameterImpl::setMatrixPointerArray(const void* const* matrices, unsigned int count)
{
    if (m_sparseArray)
    {
        float* dst = s_sparseMatrixStaging;
        for (unsigned int i = 0; i < count; ++i, dst += 12)
        {
            const float* const src = static_cast<const float*>(matrices[i]);
            dst[0] = src[0];  dst[1] = src[4];  dst[2]  = src[8];  dst[3]  = src[12];
            dst[4] = src[1];  dst[5] = src[5];  dst[6]  = src[9];  dst[7]  = src[13];
            dst[8] = src[2];  dst[9] = src[6];  dst[10] = src[10]; dst[11] = src[14];
        }
        stateManagerSetVertexShaderConstantF(m_arrayStart, s_sparseMatrixStaging,
                                             count * m_columns);
        return;
    }

    ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect));
    const D3DXHANDLE handle = ::renddx9abi::d3dxHandle(m_handle);
    fx->SetArrayRange(handle, 0, count);
    // The SDK spells the array as CONST D3DXMATRIX**; D3DX only reads through it.
    fx->SetMatrixPointerArray(
        handle, const_cast<const D3DXMATRIX**>(reinterpret_cast<const D3DXMATRIX* const*>(matrices)),
        count);
}

ParameterOwner* createFXParameter(void* effect, void* d3dxHandle)
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(FXParameterImpl));
    if (!storage)
        return 0;
    return new (storage) FXParameterImpl(effect, d3dxHandle);
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
