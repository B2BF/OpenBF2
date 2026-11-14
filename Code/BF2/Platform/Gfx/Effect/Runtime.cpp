// [CHOSEN, not recovered] Port implementation using the local MojoShader FX2/CTAB contract.
#if defined(__APPLE__)
#include "Runtime.h"
#include "../Unsupported.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace bf2fx {
[[noreturn]] void refuse(const std::string& method,const std::string& detail) {
    // Extend executable IR only with real-effect fixtures and numerical/state-update checks;
    // never default unsupported work to zero (Runtime.h: the operation fails instead).
    bf2gfx::unsupported("bf2fx::"+method+": "+detail);
    throw Refused(method+": "+detail);
}
template<class A,class B> static A bits(B b) { static_assert(sizeof(A)==sizeof(B),"bit width"); A a;std::memcpy(&a,&b,sizeof(a));return a; }
static bool numeric(const Type& t) {return t.type>=Bool && t.type<=Float && t.cls<=3;}
static bool texture(const Type& t) {return t.type>=Texture && t.type<Sampler;}
static uint32_t elements(const Type& t) {return std::max(1u,t.elements);}
static double number(uint32_t word,uint32_t type) {return type==Float?double(bits<float>(word)):type==Bool?double(word!=0):double(bits<int32_t>(word));}
static int32_t integer(double v) {
    // [CHOSEN, not recovered] D3DX numeric setters use x86 truncation for nonfinite/out-of-range inputs.
    return !std::isfinite(v) || v<INT32_MIN || v>=2147483648.0 ? INT32_MIN:int32_t(v);
}
static uint32_t word(double v,uint32_t type) {return type==Float?bits<uint32_t>(float(v)):type==Bool?uint32_t(v!=0):bits<uint32_t>(integer(v));}
// [CHOSEN, not recovered] Struct storage: per element, each member in order, each logical member
// row padded to four words (a matrix member is logical row-major, class MATRIX_ROWS, as both
// exporters describe it and as D3DX9_25's member descriptors report it). Struct-in-struct and
// object members stop by name until a real effect needs them.
static bool structType(const Type& t) {return t.cls==5;}
static void checkStruct(const Type& t) {
    for(const auto& m:t.members) {
        if(structType(m))refuse("Runtime::components","Nested struct member: "+t.name+"."+m.name);
        if(!numeric(m))refuse("Runtime::components","Non-numeric struct member: "+t.name+"."+m.name);
        if(m.cls==3 || m.rows<1 || m.rows>4 || m.columns<1 || m.columns>4)
            refuse("Runtime::components","Struct member matrix not in logical-row form: "+t.name+"."+m.name);
    }
}
static size_t memberWords(const Type& m) {return size_t(m.rows)*4*elements(m);}
static size_t structWords(const Type& t) {size_t n=0;for(const auto& m:t.members)n+=memberWords(m);return n;}
Runtime::Runtime(Description description):data_(std::move(description)) {
    for(const auto& p:data_.parameters) {
        values_.push_back(p.value.words); textures_.emplace_back(texture(p.value.type)?elements(p.value.type):0);
        if(structType(p.value.type)) try {
            checkStruct(p.value.type);
            if(p.value.words.size()!=structWords(p.value.type)*elements(p.value.type))
                refuse("Runtime::Runtime","Struct storage differs from its member layout: "+p.value.type.name);
        } catch(const Refused&) {valid_=false;}
    }
}
const Value* Runtime::value(Handle h) const {
    if(h.parameter>=data_.parameters.size()) return nullptr;
    const auto& v=data_.parameters[h.parameter].value;
    if(h.element!=UINT32_MAX && !(v.type.elements && h.element<v.type.elements)) return nullptr;
    if(h.member!=UINT32_MAX) {
        // Members exist on a non-array struct or on one element of a struct array.
        if(!structType(v.type) || h.member>=v.type.members.size() || (v.type.elements && h.element==UINT32_MAX)) return nullptr;
        if(h.memberElement!=UINT32_MAX && h.memberElement>=v.type.members[h.member].elements) return nullptr;
    } else if(h.memberElement!=UINT32_MAX) return nullptr;
    return &v;
}
Type Runtime::viewType(Handle h) const {
    auto* v=value(h); if(!v) return {};
    if(h.member!=UINT32_MAX) {Type m=v->type.members[h.member];if(h.memberElement!=UINT32_MAX)m.elements=0;return m;}
    Type t=v->type; if(h.element!=UINT32_MAX) t.elements=0; return t;
}
Handle Runtime::getParameterByName(const std::string& name) const {
    // name[index] and name[index].member / name.member, as D3DX9_25 resolves them.
    auto dot=name.find('.'); std::string head=name.substr(0,dot);
    auto bracket=head.find('['); auto base=head.substr(0,bracket);
    for(uint32_t i=0;i<data_.parameters.size();++i) if(data_.parameters[i].value.type.name==base) {
        Handle h{i,UINT32_MAX};
        if(bracket!=std::string::npos) {
            if(head.back()!=']') return {};
            auto digits=head.substr(bracket+1,head.size()-bracket-2);
            if(digits.empty() || digits.find_first_not_of("0123456789")!=std::string::npos || digits.size()>9) return {};
            h=getParameterElement(h,uint32_t(std::stoul(digits))); if(!h) return {};
        }
        if(dot==std::string::npos) return h;
        auto member=name.substr(dot+1);
        // Struct-in-struct members are refused at construction (checkStruct), so a nested
        // name resolves nothing, as D3DX answers for a name the effect lacks.
        if(member.find('.')!=std::string::npos) return {};
        auto mb=member.find('[');
        Handle m=getParameterMember(h,member.substr(0,mb));
        if(mb==std::string::npos || !m) return m;
        if(member.back()!=']') return {};
        auto digits=member.substr(mb+1,member.size()-mb-2);
        if(digits.empty() || digits.find_first_not_of("0123456789")!=std::string::npos || digits.size()>9) return {};
        return getParameterElement(m,uint32_t(std::stoul(digits)));
    } return {};
}
Handle Runtime::getParameterElement(Handle h,uint32_t index) const {
    auto* v=value(h); if(!v) return {};
    if(h.member!=UINT32_MAX) {
        // An array member's element (D3DX9_25: GetParameterElement(UserData.uvMatrix, i)).
        Handle e=h; e.memberElement=index;
        return h.memberElement==UINT32_MAX && index<v->type.members[h.member].elements?e:Handle{};
    }
    return h.element==UINT32_MAX && index<v->type.elements?Handle{h.parameter,index}:Handle{};
}
Handle Runtime::getParameterMember(Handle h,uint32_t index) const {
    if(h.member!=UINT32_MAX) return {};
    Handle m{h.parameter,h.element,index,UINT32_MAX}; return value(m)?m:Handle{};
}
Handle Runtime::getParameterMember(Handle h,const std::string& name) const {
    auto* v=value(h); if(!v || h.member!=UINT32_MAX || !structType(v->type)) return {};
    for(uint32_t i=0;i<v->type.members.size();++i) if(v->type.members[i].name==name) return getParameterMember(h,i);
    return {};
}
// Storage words in D3DX's packed value order, with each component's numeric type.
std::vector<Runtime::Component> Runtime::components(Handle h) const try {
    auto* v=value(h);
    if(!v) return {};
    const auto& t=v->type;std::vector<Component> result;
    uint32_t first=h.element==UINT32_MAX?0:h.element,last=h.element==UINT32_MAX?elements(t):first+1;
    if(structType(t)) {
        checkStruct(t);
        const size_t stride=structWords(t);
        for(uint32_t e=first;e<last;++e) {
            size_t offset=e*stride;
            for(uint32_t m=0;m<t.members.size();++m) {
                const auto& mt=t.members[m];
                if(h.member==UINT32_MAX || h.member==m)
                    for(uint32_t me=0;me<elements(mt);++me) if(h.memberElement==UINT32_MAX || h.memberElement==me)
                        for(uint32_t r=0;r<mt.rows;++r) for(uint32_t c=0;c<mt.columns;++c)
                            result.push_back({offset+(me*mt.rows+r)*4+c,mt.type});
                offset+=memberWords(mt);
            }
        }
    } else {
        if(!numeric(t)) return {};
        for(uint32_t e=first;e<last;++e) for(uint32_t r=0;r<t.rows;++r) for(uint32_t c=0;c<t.columns;++c)
            result.push_back({(e*t.rows+r)*4+c,t.type});
    }
    if(!result.empty() && result.back().word>=values_[h.parameter].size()) refuse("Runtime::components","Malformed numeric parameter storage");
    return result;
} catch(const Refused&) {return {};} // The typed accessors and Set/GetValue then fail.
static size_t parameterBytes(const Type& t) {
    if(numeric(t))return size_t(t.rows)*t.columns*4*elements(t);
    if(t.cls==5) {size_t count=0;for(const auto& member:t.members)count+=parameterBytes(member);return count*elements(t);}
    if(t.type>=Sampler && t.type<=14)return 0;
    return sizeof(void*)*elements(t);
}
bool Runtime::getParameterDesc(Handle h,ParameterDesc& desc) const {
    auto* v=value(h); if(!v) return false;
    desc.type=viewType(h);
    if(desc.type.cls==5)desc.type.rows=desc.type.columns=0;
    const bool member=h.member!=UINT32_MAX;
    desc.semantic=member?std::string():v->semantic;desc.flags=member?0:data_.parameters[h.parameter].flags;
    desc.annotations=member?0:uint32_t(data_.parameters[h.parameter].annotations.size());
    desc.bytes=uint32_t(parameterBytes(desc.type));
    return true;
}
const Value* Runtime::getAnnotation(Handle h,const std::string& name) const {
    if(!value(h) || h.member!=UINT32_MAX) return nullptr;for(const auto& v:data_.parameters[h.parameter].annotations) if(v.type.name==name) return &v;return nullptr;
}
const Value* Runtime::getAnnotation(Handle h,uint32_t index) const {
    if(!value(h) || h.member!=UINT32_MAX || index>=data_.parameters[h.parameter].annotations.size()) return nullptr;
    return &data_.parameters[h.parameter].annotations[index];
}
int Runtime::getTechniqueByName(const std::string& name) const {
    for(size_t i=0;i<data_.techniques.size();++i) if(data_.techniques[i].name==name) return int(i);return -1;
}
int Runtime::findNextValidTechnique(int previous,Callbacks& callbacks) const {
    if(previous < -1 || previous>=int(data_.techniques.size())) return -1;
    for(size_t i=size_t(previous+1);i<data_.techniques.size();++i) {
        bool valid=!data_.techniques[i].passes.empty();
        for(const auto& p:data_.techniques[i].passes) if(!callbacks.validate(p,data_)) {valid=false;break;}
        if(valid) return int(i);
    } return -1;
}
// Typed scalar/array/vector/matrix access. D3DX9_25 refuses it on a whole struct or struct
// element (only members and Set/GetValue reach struct storage).
bool Runtime::setNumeric(Handle h,uint32_t type,const void* data,size_t n) {
    if(!numeric(viewType(h))) return false;
    auto indices=components(h);if(indices.empty() || n>indices.size() || (!data && n)) return false;
    auto* p=static_cast<const uint8_t*>(data);auto& dst=values_[h.parameter];
    for(size_t i=0;i<n;++i) {uint32_t w;std::memcpy(&w,p+i*4,4);dst[indices[i].word]=word(number(w,type),indices[i].type);}return true;
}
bool Runtime::getNumeric(Handle h,uint32_t type,void* data,size_t n) const {
    if(!numeric(viewType(h))) return false;
    auto indices=components(h);if(indices.empty() || n>indices.size() || (!data && n)) return false;
    auto* p=static_cast<uint8_t*>(data);const auto& src=values_[h.parameter];
    for(size_t i=0;i<n;++i) {auto w=word(number(src[indices[i].word],indices[i].type),type);std::memcpy(p+i*4,&w,4);}return true;
}
bool Runtime::setFloat(Handle h,float v) {return setNumeric(h,Float,&v,1);}
bool Runtime::getFloat(Handle h,float& v) const {return getNumeric(h,Float,&v,1);}
bool Runtime::setInt(Handle h,int32_t v) {return setNumeric(h,Int,&v,1);}
bool Runtime::getInt(Handle h,int32_t& v) const {return getNumeric(h,Int,&v,1);}
bool Runtime::setBool(Handle h,bool v) {int32_t i=v;return setNumeric(h,Bool,&i,1);}
bool Runtime::getBool(Handle h,bool& v) const {int32_t i;if(!getNumeric(h,Bool,&i,1))return false;v=i!=0;return true;}
bool Runtime::setFloatArray(Handle h,const float* p,size_t n) {return setNumeric(h,Float,p,n);}
bool Runtime::getFloatArray(Handle h,float* p,size_t n) const {return getNumeric(h,Float,p,n);}
bool Runtime::setIntArray(Handle h,const int32_t* p,size_t n) {return setNumeric(h,Int,p,n);}
bool Runtime::getIntArray(Handle h,int32_t* p,size_t n) const {return getNumeric(h,Int,p,n);}
bool Runtime::setBoolArray(Handle h,const int32_t* p,size_t n) {return setNumeric(h,Bool,p,n);}
bool Runtime::getBoolArray(Handle h,int32_t* p,size_t n) const {return getNumeric(h,Bool,p,n);}
bool Runtime::setVector(Handle h,const std::array<float,4>& v) {
    auto t=viewType(h);return value(h) && t.rows==1 && t.columns<=4 && setNumeric(h,Float,v.data(),t.columns);
}
bool Runtime::getVector(Handle h,std::array<float,4>& v) const {
    auto t=viewType(h);if(!value(h) || t.rows!=1 || t.columns>4) return false;v={};return getNumeric(h,Float,v.data(),t.columns);
}
bool Runtime::setVectorArray(Handle h,const float* p,size_t n) {
    auto t=viewType(h);if(!value(h) || !p || !numeric(t) || t.rows!=1 || t.columns>4 || n>elements(t))return false;
    std::vector<float> packed;for(size_t i=0;i<n;++i)for(uint32_t c=0;c<t.columns;++c)packed.push_back(p[i*4+c]);
    return setNumeric(h,Float,packed.data(),packed.size());
}
bool Runtime::getVectorArray(Handle h,float* p,size_t n) const {
    auto t=viewType(h);if(!value(h) || !p || !numeric(t) || t.rows!=1 || t.columns>4 || n>elements(t))return false;
    std::vector<float> packed(n*t.columns);if(!getNumeric(h,Float,packed.data(),packed.size()))return false;
    std::fill(p,p+n*4,0);for(size_t i=0;i<n;++i)for(uint32_t c=0;c<t.columns;++c)p[i*4+c]=packed[i*t.columns+c];return true;
}
bool Runtime::setMatrix(Handle h,const float* m,bool transpose) {return setMatrixArray(h,m,1,transpose);}
bool Runtime::getMatrix(Handle h,float* m,bool transpose) const {return getMatrixArray(h,m,1,transpose);}
bool Runtime::setMatrixArray(Handle h,const float* m,size_t n,bool transpose) {
    auto t=viewType(h);if(!value(h) || !m || !numeric(t) || t.cls<2 || t.rows>4 || t.columns>4) return false;
    if(n>elements(t)) return false;
    std::vector<float> packed;for(size_t e=0;e<n;++e) for(uint32_t r=0;r<t.rows;++r) for(uint32_t c=0;c<t.columns;++c)
        packed.push_back(m[e*16+(transpose?c*4+r:r*4+c)]);
    return setNumeric(h,Float,packed.data(),packed.size());
}
bool Runtime::getMatrixArray(Handle h,float* m,size_t n,bool transpose) const {
    auto t=viewType(h);if(!value(h) || !m || !numeric(t) || t.cls<2 || t.rows>4 || t.columns>4) return false;
    if(n>elements(t)) return false;
    std::vector<float> packed(n*t.rows*t.columns);if(!getNumeric(h,Float,packed.data(),packed.size()))return false;
    std::fill(m,m+n*16,0);size_t i=0;
    for(size_t e=0;e<n;++e) for(uint32_t r=0;r<t.rows;++r) for(uint32_t c=0;c<t.columns;++c)
        m[e*16+(transpose?c*4+r:r*4+c)]=packed[i++];return true;
}
bool Runtime::setValue(Handle h,const void* data,size_t bytes) {
    auto indices=components(h);if(indices.empty() || !data || bytes<indices.size()*4)return false;
    for(size_t i=0;i<indices.size();++i) {
        uint32_t w;std::memcpy(&w,static_cast<const uint8_t*>(data)+i*4,4);
        values_[h.parameter][indices[i].word]=indices[i].type==Bool?uint32_t(w!=0):w;
    }return true;
}
bool Runtime::getValue(Handle h,void* data,size_t bytes) const {
    auto indices=components(h);if(indices.empty() || !data || bytes<indices.size()*4)return false;
    for(size_t i=0;i<indices.size();++i)std::memcpy(static_cast<uint8_t*>(data)+i*4,&values_[h.parameter][indices[i].word],4);return true;
}
bool Runtime::setTexture(Handle h,std::shared_ptr<void> ptr) {
    auto* v=value(h);if(!v || !texture(v->type))return false;textures_[h.parameter][h.element==UINT32_MAX?0:h.element]=std::move(ptr);return true;
}
bool Runtime::getTexture(Handle h,std::shared_ptr<void>& ptr) const {
    auto* v=value(h);if(!v || !texture(v->type))return false;ptr=textures_[h.parameter][h.element==UINT32_MAX?0:h.element];return true;
}

static void markKnown(uint32_t set,size_t first,size_t extent,Registers& regs);
static void store(uint32_t set,size_t dst,double n,Registers& regs);
// [CHOSEN, not recovered] HLSL SM2/SM3 struct constants (D3DX9_25 oracle, matrix-oracle.c): each
// member starts a register; per member element a vector/scalar takes one register, a
// column-major matrix (CTAB class 3) one register per column holding that column's rows, a
// row_major matrix (class 2) one register per row; elements follow each other, and the CTAB
// register count trims unused trailing registers (D3DX uploads only those).
static void putStructBinding(const Binding& binding,const Value& v,const Words& words,Registers& regs) {
    const auto& t=binding.type;
    if(binding.set!=2) refuse("Runtime::mapConstants","Struct CTAB binding outside the float registers: "+t.name);
    if(v.type.members.size()!=t.members.size()) refuse("Runtime::mapConstants","Struct parameter/CTAB member mismatch: "+t.name);
    checkStruct(v.type);
    for(size_t m=0;m<t.members.size();++m) {
        const auto& c=t.members[m]; const auto& p=v.type.members[m];
        if(c.name!=p.name || c.cls>3 || c.rows!=p.rows || c.columns!=p.columns || std::max(1u,c.elements)!=elements(p))
            refuse("Runtime::mapConstants","Struct CTAB member layout: "+t.name+"."+c.name);
    }
    const size_t first=binding.index,extent=binding.count,stride=structWords(v.type);
    markKnown(binding.set,first*4,extent*4,regs);
    size_t reg=first;
    for(uint32_t e=0;e<elements(t) && reg<first+extent;++e) {
        size_t offset=e*stride;
        for(size_t m=0;m<t.members.size() && reg<first+extent;++m) {
            const auto& mt=v.type.members[m];
            const bool columns=t.members[m].cls==3; // registers are columns; otherwise rows
            const uint32_t vectors=columns?mt.columns:mt.rows,width=columns?mt.rows:mt.columns;
            for(uint32_t me=0;me<elements(mt);++me) for(uint32_t j=0;j<vectors && reg<first+extent;++j,++reg)
                for(uint32_t k=0;k<width;++k) {
                    const size_t src=offset+(me*mt.rows+(columns?k:j))*4+(columns?j:k);
                    if(src>=words.size()) refuse("Runtime::mapConstants","Struct storage shorter than its CTAB: "+t.name);
                    store(binding.set,reg*4+k,number(words[src],mt.type),regs);
                }
            offset+=memberWords(mt);
        }
    }
}
static void markKnown(uint32_t set,size_t first,size_t extent,Registers& regs) {
    if(first+extent>65536) refuse("Runtime::mapConstants","Register bound exceeded");
    auto mark=[&](auto& r,auto& known) {r.resize(std::max(r.size(),first+extent));known.resize(r.size());std::fill(known.begin()+first,known.begin()+first+extent,1);};
    if(set==2) mark(regs.f,regs.knownF); else if(set==1) mark(regs.i,regs.knownI); else if(set==0) mark(regs.b,regs.knownB);
    else refuse("Runtime::mapConstants","Unknown register set");
}
static void store(uint32_t set,size_t dst,double n,Registers& regs) {
    if(set==2) regs.f[dst]=float(n); else if(set==1) regs.i[dst]=integer(n); else regs.b[dst]=n!=0;
}
static void putBinding(const Binding& binding,const Value& v,const Words& words,Registers& regs) {
    const auto& t=binding.type;
    if(structType(v.type) && structType(t)) return putStructBinding(binding,v,words,regs);
    if(!numeric(v.type) || !numeric(t)) refuse("Runtime::mapConstants","Non-numeric CTAB binding: "+t.name);
    bool columns=t.cls==3;
    uint32_t vectors=(t.cls==2 || t.cls==3)?(columns?t.columns:t.rows):t.rows;
    uint32_t width=columns?t.rows:t.columns;
    size_t first=binding.set==0?binding.index:size_t(binding.index)*4;
    size_t extent=binding.set==0?binding.count:size_t(binding.count)*4;
    if(first+extent>65536) refuse("Runtime::mapConstants","Register bound exceeded");
    if(binding.set==2) {regs.f.resize(std::max(regs.f.size(),first+extent));regs.knownF.resize(regs.f.size());std::fill(regs.knownF.begin()+first,regs.knownF.begin()+first+extent,1);}
    else if(binding.set==1) {regs.i.resize(std::max(regs.i.size(),first+extent));regs.knownI.resize(regs.i.size());std::fill(regs.knownI.begin()+first,regs.knownI.begin()+first+extent,1);}
    else if(binding.set==0) {regs.b.resize(std::max(regs.b.size(),first+extent));regs.knownB.resize(regs.b.size());std::fill(regs.knownB.begin()+first,regs.knownB.begin()+first+extent,1);}
    else refuse("Runtime::mapConstants","Unknown register set");
    for(uint32_t e=0;e<elements(t);++e) for(uint32_t r=0;r<vectors;++r) for(uint32_t c=0;c<width;++c) {
        size_t dst=first+(binding.set==0?(e*vectors+r)*width+c:(e*vectors+r)*4+c);
        size_t src=(e*v.type.rows+(columns?c:r))*4+(columns?r:c);
        if(dst>=first+extent) continue;
        if(src>=words.size()) refuse("Runtime::mapConstants","Parameter/CTAB shape mismatch: "+t.name);
        double n=number(words[src],v.type.type);
        if(binding.set==2) regs.f[dst]=float(n);else if(binding.set==1)regs.i[dst]=integer(n);else regs.b[dst]=n!=0;
    }
}
Registers Runtime::mapConstants(const Shader& shader) const {
    Registers regs;
    for(const auto& b:shader.bindings) if(b.set!=3) {
        auto h=getParameterByName(b.type.name);if(!h)refuse("Runtime::mapConstants","Unknown parameter: "+b.type.name);
        putBinding(b,*value(h),values_[h.parameter],regs);
    }
    if(!shader.preshader.instructions.empty()) {
        regs.f=evaluate(shader.preshader,std::move(regs.f));regs.knownF.resize(regs.f.size());
        for(const auto& inst:shader.preshader.instructions) {
            const auto& op=inst.operands.back();if(op.kind==1)std::fill(regs.knownF.begin()+op.index,regs.knownF.begin()+op.index+inst.elements,1);
        }
    }
    for(const auto& c:shader.constants) {
        auto kind=c.value.type.type;size_t index=kind==Bool?c.id:size_t(c.id)*4;
        if(index>65536)refuse("Runtime::mapConstants","Constant bound exceeded");
        if(kind==Float) {regs.f.resize(std::max(regs.f.size(),index+4));regs.knownF.resize(regs.f.size());for(size_t i=0;i<4;++i){regs.f[index+i]=bits<float>(c.value.words.at(i));regs.knownF[index+i]=1;}}
        else if(kind==Int) {regs.i.resize(std::max(regs.i.size(),index+4));regs.knownI.resize(regs.i.size());for(size_t i=0;i<4;++i){regs.i[index+i]=bits<int32_t>(c.value.words.at(i));regs.knownI[index+i]=1;}}
        else {regs.b.resize(std::max(regs.b.size(),index+1));regs.knownB.resize(regs.b.size());regs.b[index]=c.value.words.at(0)!=0;regs.knownB[index]=1;}
    } return regs;
}
Bytes Runtime::packConstants(const Shader& shader,const Registers& regs) const {
    Bytes packed;size_t alignment=1;
    for(uint32_t kind=0;kind<3;++kind)for(const auto& u:shader.uniforms)if(u.kind==kind && !u.constant) {
        size_t width=kind==2?1:16;alignment=std::max(alignment,width);
        while(packed.size()%width)packed.push_back(0);
        for(uint32_t r=0;r<u.count;++r) {
            size_t index=size_t(u.index)+r;
            if(kind==2) {
                if(index>=regs.b.size() || index>=regs.knownB.size() || !regs.knownB[index])refuse("Runtime::packConstants","G4 must supply the current device bool register");
                packed.push_back(regs.b[index]);continue;
            }
            for(uint32_t c=0;c<4;++c) {
                size_t component=index*4+c;uint32_t w=0;
                if(kind==0) {if(component>=regs.f.size() || component>=regs.knownF.size() || !regs.knownF[component])refuse("Runtime::packConstants","G4 must supply the current device float register");w=bits<uint32_t>(regs.f[component]);}
                if(kind==1) {if(component>=regs.i.size() || component>=regs.knownI.size() || !regs.knownI[component])refuse("Runtime::packConstants","G4 must supply the current device int register");w=bits<uint32_t>(regs.i[component]);}
                for(unsigned b=0;b<4;++b)packed.push_back(uint8_t(w>>(b*8)));
            }
        }
    }
    while(packed.size()%alignment)packed.push_back(0);return packed;
}

std::vector<float> Runtime::evaluate(const Program& program,std::vector<float> output) const {
    if(program.inputs>65536 || program.temps>65536)refuse("Runtime::evaluate","Preshader register limit");
    Registers inputs;inputs.f.resize(program.inputs*4);
    for(const auto& b:program.bindings) {
        auto h=getParameterByName(b.type.name);if(!h)refuse("Runtime::evaluate","Unknown preshader input: "+b.type.name);
        putBinding(b,*value(h),values_[h.parameter],inputs);
    }
    std::vector<double> temps(program.temps);
    // Opcode/operand enums follow MojoShader ad5dff8; upstream arithmetic is the reference.
    for(const auto& inst:program.instructions) {
        if(inst.elements<1 || inst.elements>4 || inst.operands.size()<2 || inst.operands.size()>4)
            refuse("Runtime::evaluate","Malformed preshader instruction");
        bool scalar=inst.opcode>=25;double src[3][4]{};uint32_t n=inst.elements;
        auto read=[&](const Operand& op,uint32_t lane)->double {
            size_t i=size_t(op.index)+lane;
            if(i>65536)refuse("Runtime::evaluate","Operand register bound");
            if(!op.arrays.empty()) {
                if(op.kind!=0)refuse("Runtime::evaluate","Indirect non-input operand");
                size_t base=(op.index>>4)*4+((op.index>>2)&3);
                if(base>=inputs.f.size())refuse("Runtime::evaluate","Indirect input bound");
                int32_t ix=bits<int32_t>(inputs.f[base]);
                for(auto a:op.arrays) {int64_t addr=int64_t(a)+ix;if(addr<0 || size_t(addr)>=inputs.f.size())refuse("Runtime::evaluate","Indirect array bound");ix=bits<int32_t>(inputs.f[size_t(addr)]);}
                return ix;
            }
            if(op.kind==0) {if(i>=inputs.f.size())refuse("Runtime::evaluate","Input bound");return inputs.f[i];}
            if(op.kind==1) {if(i>=output.size())output.resize(i+1);return output[i];}
            if(op.kind==2) {if(i>=program.literals.size())refuse("Runtime::evaluate","Literal bound");return bits<double>(program.literals[i]);}
            if(op.kind==3) {if(i>=temps.size())refuse("Runtime::evaluate","Temporary bound");return temps[i];}
            refuse("Runtime::evaluate","Unknown operand");
        };
        for(size_t s=0;s+1<inst.operands.size();++s) for(uint32_t j=0;j<n;++j)src[s][j]=read(inst.operands[s],scalar && s==0?0:j);
        double dst[4]{};
        for(uint32_t j=0;j<n;++j) {double a=src[0][j],b=src[1][j],c=src[2][j];
            switch(inst.opcode) {
                case 1:dst[j]=a;break;case 2:dst[j]=-a;break;case 3:dst[j]=1/a;break;
                case 4:dst[j]=a-std::floor(a);break;case 5:dst[j]=std::exp(a);break;case 6:dst[j]=std::log(a);break;
                case 7:dst[j]=1/std::sqrt(a);break;case 8:dst[j]=std::sin(a);break;case 9:dst[j]=std::cos(a);break;
                case 10:dst[j]=std::asin(a);break;case 11:dst[j]=std::acos(a);break;case 12:dst[j]=std::atan(a);break;
                case 13:case 25:dst[j]=a<b?a:b;break;case 14:case 26:dst[j]=a>b?a:b;break;
                case 15:case 27:dst[j]=a<b;break;case 16:case 28:dst[j]=a>=b;break;
                case 17:case 29:dst[j]=a+b;break;case 18:case 30:dst[j]=a*b;break;
                case 19:case 31:dst[j]=std::atan2(a,b);break;case 20:case 32:dst[j]=a/b;break;
                case 21:dst[j]=a>=0?b:c;break;
                case 22:dst[j]=a!=0?b:c;break; // MOVC: a non-zero selector picks the second operand
                case 23:case 33:for(uint32_t k=0;k<n;++k)dst[j]+=src[0][k]*src[1][k];break; // DOT (scalar form: lane 0 broadcast)
                default:refuse("Runtime::evaluate","Unsupported preshader opcode "+std::to_string(inst.opcode));
            }
        }
        const auto& target=inst.operands.back();size_t end=size_t(target.index)+n;
        if(end>65536 || !target.arrays.empty())refuse("Runtime::evaluate","Output register bound");
        if(target.kind==1) {if(output.size()<end)output.resize(end);for(uint32_t j=0;j<n;++j)output[target.index+j]=float(dst[j]);}
        else if(target.kind==3) {if(temps.size()<end)refuse("Runtime::evaluate","Temporary output bound");for(uint32_t j=0;j<n;++j)temps[target.index+j]=dst[j];}
        else refuse("Runtime::evaluate","Invalid preshader destination");
    }return output;
}
Value Runtime::evaluate(const Value& value) const {
    if(value.expression.empty()) return value;
    if(value.expressionType==1) {
        auto h=getParameterByName(value.reference);if(!h)refuse("Runtime::evaluate","Unknown state reference: "+value.reference);
        Value v=*this->value(h);v.words=values_[h.parameter];return v;
    }
    if(value.expressionType!=0)refuse("Runtime::evaluate","Unsupported state expression type");
    if(value.program.instructions.empty())refuse("Runtime::evaluate","Missing state expression IR");
    auto output=evaluate(value.program);Value v=value;v.expression.clear();v.program={};v.words.assign(std::max<size_t>(4,output.size()),0);
    for(size_t i=0;i<output.size();++i)v.words[i]=word(output[i],value.type.type);return v;
}
const Shader* Runtime::resolveShader(uint32_t object,uint32_t depth) const {
    if(depth>16)refuse("Runtime::resolveShader","Selector recursion");
    auto it=std::find_if(data_.shaders.begin(),data_.shaders.end(),[&](const Shader& s){return s.object==object;});
    if(it==data_.shaders.end())refuse("Runtime::resolveShader","Missing shader object "+std::to_string(object));
    if(!it->selectorKind)return it->bytecode.empty() && it->msl.empty()?nullptr:&*it;
    if(it->selectorParameter>=values_.size())refuse("Runtime::resolveShader","Bad selector parameter");
    uint32_t element=it->selectorElement;
    if(it->selectorKind==1) {
        auto result=evaluate(it->preshader);if(result.empty() || !std::isfinite(result[0]) || result[0]<0 || result[0]>=values_[it->selectorParameter].size())
            refuse("Runtime::resolveShader","Shader selector out of range");element=uint32_t(result[0]);
    }
    if(element>=values_[it->selectorParameter].size())refuse("Runtime::resolveShader","Shader element out of range");
    return resolveShader(values_[it->selectorParameter][element],depth+1);
}
void Runtime::applySampler(const Value& original,uint32_t stage,uint32_t slot,Callbacks& callbacks) const {
    Value sampler=evaluate(original);if(sampler.type.type<Sampler || sampler.type.type>14)refuse("Runtime::applySampler","Non-sampler reference");
    std::shared_ptr<void> ptr;
    for(size_t i=0;i<sampler.states.size();++i) {
        sampler.states[i]=evaluate(sampler.states[i]);
        if(sampler.stateIds.at(i)==4) {
            const auto& v=sampler.states[i];Handle h;
            if(!v.reference.empty())h=getParameterByName(v.reference);
            else if(!v.words.empty() && v.words[0]<data_.objects.size())h=getParameterByName(data_.objects[v.words[0]]);
            if(h) {if(!getTexture(h,ptr))refuse("Runtime::applySampler","Texture reference has wrong type");}
            else if(!v.words.empty() && v.words[0]!=0)refuse("Runtime::applySampler","Unknown texture object");
        }
    }callbacks.sampler(stage,slot,sampler,ptr);
}
bool Runtime::applyPass(uint32_t technique,uint32_t pass,Callbacks& callbacks) const try {
    if(technique>=data_.techniques.size() || pass>=data_.techniques[technique].passes.size())return false;
    const auto& p=data_.techniques[technique].passes[pass];
    std::vector<const Shader*> shaders;
    for(const auto& state:p.states) {
        auto v=evaluate(state.value);
        if(state.id==146 || state.id==147) {
            const Shader* shader=nullptr;
            if(v.type.cls==4 && !v.words.empty())shader=resolveShader(v.words[0]);
            else if(v.words.empty() || v.words[0]!=0)refuse("Runtime::applyPass","Invalid shader assignment");
            callbacks.shader(state.id==146?VertexShader:PixelShader,shader);if(shader)shaders.push_back(shader);
        } else if(state.id==178) applySampler(v,PixelShader,state.index,callbacks);
        else callbacks.state(state.id,state.index,v);
    }
    for(auto* shader:shaders) {
        callbacks.constants(*shader,mapConstants(*shader));
        for(const auto& b:shader->bindings) if(b.set==3) {
            auto h=getParameterByName(b.type.name);if(!h)refuse("Runtime::applyPass","Unknown sampler: "+b.type.name);
            applySampler(*value(h),shader->stage,b.index,callbacks);
        }
    }return true;
} catch(const Refused&) {return false;} // BeginPass/CommitChanges answer D3DERR_INVALIDCALL.
} // namespace bf2fx
#endif
