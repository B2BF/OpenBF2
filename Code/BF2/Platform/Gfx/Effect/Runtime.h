// [CHOSEN, not recovered] CPU parameter and expression execution; G4 owns the D3DX/Metal facade.
#pragma once
#include "Cache.h"
#include <array>
#include <stdexcept>

namespace bf2fx {
// A parameter, one of its elements, or one member of a struct parameter/element (D3DX9_25
// allows members of a non-array struct or of one element of a struct array), or one element
// of an array member (UserData.uvMatrix[2]).
struct Handle {
    uint32_t parameter = UINT32_MAX, element = UINT32_MAX, member = UINT32_MAX, memberElement = UINT32_MAX;
    explicit operator bool() const { return parameter!=UINT32_MAX; }
};
struct ParameterDesc {
    Type type;
    std::string semantic;
    uint32_t bytes = 0, annotations = 0, flags = 0;
};
struct Registers {
    std::vector<float> f;
    std::vector<int32_t> i;
    std::vector<uint8_t> b;
    // Set components only; G4 overlays these on device registers set by indexed pass states.
    std::vector<uint8_t> knownF,knownI,knownB;
};
class Callbacks {
public:
    virtual ~Callbacks() = default;
    // These IDs are FX2/MojoShader IDs. G4 must map them explicitly to its device states.
    virtual void state(uint32_t id,uint32_t index,const Value& value) = 0;
    virtual void shader(uint32_t stage,const Shader* shader) = 0;
    virtual void constants(const Shader& shader,const Registers& registers) = 0;
    virtual void sampler(uint32_t stage,uint32_t slot,const Value& sampler,
                         const std::shared_ptr<void>& texture) = 0;
    virtual bool validate(const Pass& pass,const Description& effect) = 0;
};
// [CHOSEN, not recovered] Effect data outside the executable subset (malformed or unsupported
// IR; no shipped effect selects any) is refused: refuse()
// logs once (Unsupported.h) and throws Refused. The public entry points catch it and fail like
// an ordinary caller error (bad handles, sizes or technique indices return false); a refused
// construction leaves valid() false and D3DXCreateEffect fails. evaluate, mapConstants and
// packConstants may throw Refused to direct callers.
struct Refused : std::runtime_error { using std::runtime_error::runtime_error; };
class Runtime {
public:
    explicit Runtime(Description description);
    bool valid() const { return valid_; }
    const Description& description() const { return data_; }
    // Native deferred builds: the shader records arrive after construction (same objects).
    void setShaders(std::vector<Shader> shaders) { data_.shaders = std::move(shaders); }
    Handle getParameterByName(const std::string& name) const;
    Handle getParameterElement(Handle parameter,uint32_t index) const;
    // Struct members: D3DX's GetParameter/GetParameterByName with a struct parent handle.
    Handle getParameterMember(Handle parent,uint32_t index) const;
    Handle getParameterMember(Handle parent,const std::string& name) const;
    bool getParameterDesc(Handle parameter,ParameterDesc& desc) const;
    const Value* getAnnotation(Handle parameter,const std::string& name) const;
    const Value* getAnnotation(Handle parameter,uint32_t index) const;
    int getTechniqueByName(const std::string& name) const;
    int findNextValidTechnique(int previous,Callbacks& callbacks) const;
    bool setFloat(Handle,float); bool getFloat(Handle,float&) const;
    bool setInt(Handle,int32_t); bool getInt(Handle,int32_t&) const;
    bool setBool(Handle,bool); bool getBool(Handle,bool&) const;
    bool setFloatArray(Handle,const float*,size_t); bool getFloatArray(Handle,float*,size_t) const;
    bool setIntArray(Handle,const int32_t*,size_t); bool getIntArray(Handle,int32_t*,size_t) const;
    bool setBoolArray(Handle,const int32_t*,size_t); bool getBoolArray(Handle,int32_t*,size_t) const;
    bool setVector(Handle,const std::array<float,4>&); bool getVector(Handle,std::array<float,4>&) const;
    bool setVectorArray(Handle,const float*,size_t); bool getVectorArray(Handle,float*,size_t) const;
    // Matrices at the API are D3DX row-major 4x4. Parameter dimensions select the used subset.
    bool setMatrix(Handle,const float*,bool transpose=false); bool getMatrix(Handle,float*,bool transpose=false) const;
    bool setMatrixArray(Handle,const float*,size_t,bool transpose=false);
    bool getMatrixArray(Handle,float*,size_t,bool transpose=false) const;
    bool setValue(Handle,const void*,size_t); bool getValue(Handle,void*,size_t) const;
    // Set/GetValue follow D3DX9_25: at least the parameter's packed bytes, exactly those copied;
    // structs use D3DX's packed member layout (a float3 is 12 bytes) and bools store 0/1.
    // The shared owner is supplied by G4; a raw D3D texture pointer is never truncated into FX2 words.
    bool setTexture(Handle,std::shared_ptr<void>); bool getTexture(Handle,std::shared_ptr<void>&) const;
    bool applyPass(uint32_t technique,uint32_t pass,Callbacks&) const;
    Value evaluate(const Value&) const;
    std::vector<float> evaluate(const Program&,std::vector<float> output={}) const;
    Registers mapConstants(const Shader&) const;
    // Packs MojoShader's dense MSL buffer(0) float4/int4/bool arrays, in reflection order.
    Bytes packConstants(const Shader&,const Registers&) const;
private:
    bool valid_ = true;
    Description data_;
    std::vector<Words> values_;
    std::vector<std::vector<std::shared_ptr<void>>> textures_;
    const Value* value(Handle) const;
    struct Component { size_t word; uint32_t type; };
    std::vector<Component> components(Handle) const;
    Type viewType(Handle) const;
    bool setNumeric(Handle,uint32_t,const void*,size_t);
    bool getNumeric(Handle,uint32_t,void*,size_t) const;
    const Shader* resolveShader(uint32_t object,uint32_t depth=0) const;
    void applySampler(const Value&,uint32_t stage,uint32_t slot,Callbacks&) const;
};
[[noreturn]] void refuse(const std::string& method,const std::string& detail);
} // namespace bf2fx
