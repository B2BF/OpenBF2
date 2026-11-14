// [CHOSEN, not recovered] Native effect builder for the macOS port. See FxBuild.h.
#if defined(__APPLE__)
#include "FxBuild.h"
#include "FxStates.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>

namespace bf2fx {
using namespace src;

namespace {
std::string slice(const std::string& text,size_t b,size_t e) { return text.substr(b,e-b); }
}

namespace {
// d3dx9 accepts these as function names; vkd3d-shader reads them as keywords.
bool reservedName(const std::string& name) {
    static const char* names[]={"vertexshader","pixelshader","texture","sampler","technique","pass","compile",
        "string","stateblock","geometryshader","computeshader","domainshader","hullshader","blendstate",
        "rasterizerstate","depthstencilstate","samplerstate","vector","matrix","point","line","triangle",nullptr};
    std::string l=lower(name);
    for(int i=0;names[i];++i) if(l==names[i]) return true;
    return false;
}
std::string compilerName(const std::string& name) { return reservedName(name)?"bf2fx_fn_"+name:name; }
}

std::string compilerText(const Effect& fx,const std::string& text,uint32_t stage) {
    std::string out=shaderText(text,fx);
    struct Edit { size_t begin,end; std::string with; };
    std::vector<Edit> edits;
    const char* prefix=stage==VertexShader?"vs":"ps";
    for(const auto& r:fx.reservations)
        edits.push_back({r.begin,r.end,lower(r.profile).compare(0,2,prefix)==0?"register("+r.reg+")":""});
    for(const auto& f:fx.functions)
        if(reservedName(f.name)) edits.push_back({f.nameBegin,f.nameBegin+f.name.size(),compilerName(f.name)});
    std::sort(edits.begin(),edits.end(),[](const Edit& a,const Edit& b){return a.begin>b.begin;});
    for(const auto& e:edits) {
        std::string with=e.with;
        // A dropped reservation leaves its ':' behind; replace it with a harmless semantic-free form.
        if(with.empty()) {
            size_t colon=out.rfind(':',e.begin);
            if(colon!=std::string::npos) {out.replace(colon,e.end-colon,std::string(e.end-colon,' '));continue;}
        }
        out.replace(e.begin,e.end-e.begin,with);
    }
    return out;
}

bool shaderJob(const Effect& fx,const std::string& text,const Expr& value,uint32_t stage,ShaderJob& job,std::string& error) {
    job=ShaderJob();job.stage=stage;
    if(value.kind==Expr::Asm) {job.asmText=value.text;job.key="asm|"+value.text;return true;}
    if(value.kind!=Expr::Compile) {error="Not a shader compile expression";return false;}
    job.profile=value.text;job.function=value.args.at(0)->text;
    const Function* fn=fx.findFunction(job.function);
    if(!fn || !fn->hasBody) {error="Unknown shader function "+job.function;return false;}
    std::vector<std::string> args;
    for(size_t i=1;i<value.args.size();++i) args.push_back(slice(text,value.args[i]->begin,value.args[i]->end));
    job.key=job.profile+"|"+job.function;for(const auto& a:args) job.key+="|"+a;
    size_t uniforms=0;for(const auto& p:fn->params) if(p.uniform()) ++uniforms;
    std::string callee=compilerName(job.function);
    if(args.empty() && !uniforms) {job.entry=callee;return true;}
    if(args.size()>uniforms) {error="Too many compile arguments for "+job.function;return false;}
    // Uniform parameters take the compile arguments in order (then their defaults); the
    // varying parameters and return semantic are forwarded through a generated entry point.
    std::string params,call;size_t next=0;
    for(const auto& p:fn->params) {
        if(!call.empty()) call+=", ";
        if(p.uniform()) {
            if(next<args.size()) call+="("+args[next++]+")";
            else if(p.defaultValue) call+="("+slice(text,p.defaultValue->begin,p.defaultValue->end)+")";
            else {error="Missing uniform argument "+p.name+" for "+job.function;return false;}
        } else {
            if(!params.empty()) params+=", ";
            params+=slice(text,p.begin,p.end);call+=p.name;
        }
    }
    job.entry="bf2fx_entry";
    // Return type text: from the declaration start up to the function name.
    std::string retType=slice(text,fn->begin,fn->nameBegin);
    for(const char* m:{"inline ","static "}) {size_t at;while((at=retType.find(m))!=std::string::npos) retType.erase(at,std::strlen(m));}
    bool isVoid=fn->returns.base==Base::Void;
    job.wrapper="\n"+retType+" "+job.entry+"("+params+")"+(fn->semantic.empty()?"":" : "+fn->semantic)+
        "\n{\n    "+(isVoid?"":"return ")+callee+"("+call+");\n}\n";
    return true;
}

namespace {
struct BuildError : std::runtime_error { using std::runtime_error::runtime_error; };

// MojoShader symbol types (EffectData.h numbering).
uint32_t symbolType(Base b) {
    switch(b) {
        case Base::Bool: return 1; case Base::Int: case Base::Uint: return 2;
        case Base::Half: case Base::Float: case Base::Double: return 3;
        case Base::String: return 4; case Base::Texture: return 5; case Base::Texture1D: return 6;
        case Base::Texture2D: return 7; case Base::Texture3D: return 8; case Base::TextureCube: return 9;
        case Base::Sampler: return 10; case Base::Sampler1D: return 11; case Base::Sampler2D: return 12;
        case Base::Sampler3D: return 13; case Base::SamplerCube: return 14;
        case Base::PixelShader: return 15; case Base::VertexShader: return 16;
        default: return 0;
    }
}

struct Constant {
    std::vector<double> v;
    bool isFloat = false;
};

class Builder {
public:
    Builder(const Effect& fx,const std::string& text,ShaderBackend& be,BuildReport* report)
        :fx_(fx),text_(text),be_(be),report_(report) {}
    Description run() {
        d_.objects.push_back(""); // Object 0 is never referenced.
        // d3dx9 numbers objects in source order, with techniques between the parameters
        // they follow; the parameter and technique lists keep their own order.
        struct Item { size_t begin; const Variable* g; const src::Technique* t; };
        std::vector<Item> items;
        for(const auto& g:fx_.globals) items.push_back({g.begin,&g,nullptr});
        for(const auto& t:fx_.techniques) items.push_back({t.begin,nullptr,&t});
        std::stable_sort(items.begin(),items.end(),[](const Item& a,const Item& b){return a.begin<b.begin;});
        for(const auto& it:items) {
            if(it.g) {
                const Variable& g=*it.g;
                if(g.has("static")) continue;
                index_[g.name]=d_.parameters.size();
                globals_.push_back(&g);
                d_.parameters.push_back(parameter(g));
                if(g.has("shared")) d_.parameters.back().flags|=1u; // D3DX_PARAMETER_SHARED: pooled across effects
                continue;
            }
            const src::Technique& t=*it.t;
            bf2fx::Technique out;out.name=t.name;out.annotations=annotations(t.annotations);
            for(const auto& p:t.passes) {
                bf2fx::Pass pass;pass.name=p.name;pass.annotations=annotations(p.annotations);
                for(const auto& st:p.states) {
                    State state=passState(st);
                    // A state assigned twice in one pass keeps only its last assignment.
                    pass.states.erase(std::remove_if(pass.states.begin(),pass.states.end(),
                        [&](const State& x){return x.id==state.id && x.index==state.index;}),pass.states.end());
                    pass.states.push_back(std::move(state));
                }
                out.passes.push_back(std::move(pass));
            }
            d_.techniques.push_back(std::move(out));
        }
        return std::move(d_);
    }
private:
    const Effect& fx_;
    const std::string& text_;
    ShaderBackend& be_;
    BuildReport* report_;
    std::string shaderText_[2]; // Pixel, vertex.
    Description d_;
    std::map<std::string,size_t> index_;
    std::vector<const Variable*> globals_;
    std::map<std::string,Bytes> compiled_; // Job key -> bytecode (same job, several objects).

    [[noreturn]] void fail(const std::string& m,int line=0) const {
        throw BuildError(line?("line "+std::to_string(line)+": "+m):m);
    }
    std::string source(const Expr& e) const { return slice(text_,e.begin,e.end); }
    uint32_t object(const std::string& name="") { d_.objects.push_back(name);return uint32_t(d_.objects.size()-1); }

    const Variable* global(const std::string& name) const {
        for(const auto& g:fx_.globals) if(g.name==name) return &g;
        return nullptr;
    }
    uint32_t arraySize(const Variable& v) const {
        if(v.unsizedArray) {
            if(!v.init || v.init->kind!=Expr::List) fail("Unsized array without initializer list: "+v.name,v.line);
            if(v.type.base==Base::String) return uint32_t(strings(*v.init).size());
            // D3DX9 accepts unsized shader-object arrays whose initializer is a list of compile
            // expressions (xpack RaShaderSTM.fx `VertexShader vsStaticMesh_Array[] = { compile
            // vs_1_1 f(false), compile vs_1_1 f(true) };`): one element per list entry.
            if(v.type.base==Base::VertexShader || v.type.base==Base::PixelShader)
                return uint32_t(v.init->args.size());
            // d3dx9 sizes an unsized numeric array by its flattened initializer.
            Constant c;if(!eval(*v.init,c,nullptr)) fail("Non-constant initializer: "+v.name,v.line);
            uint32_t per=v.type.numeric()?v.type.rows*v.type.cols:1;
            return uint32_t(c.v.size()/std::max(1u,per));
        }
        if(!v.arraySize) return 0;
        Constant c;if(!eval(*v.arraySize,c,nullptr) || c.v.size()!=1) fail("Non-constant array size: "+v.name,v.line);
        return uint32_t(c.v[0]);
    }
    // String initializers, nested lists flattened (d3dx9 sizes string arrays that way).
    static std::vector<std::string> strings(const Expr& e) {
        std::vector<std::string> out;
        if(e.kind==Expr::List) {for(const auto& a:e.args) {auto x=strings(*a);out.insert(out.end(),x.begin(),x.end());}}
        else out.push_back(e.kind==Expr::String?e.text:"");
        return out;
    }
    Type type(const TypeSpec& t,const std::string& name,uint32_t elements) const {
        Type out;out.name=name;out.type=symbolType(t.base);out.elements=elements;
        if(t.numeric()) {
            out.rows=t.rows;out.columns=t.cols;out.cls=t.matrix?2:t.vector?1:0;
        } else if(t.base==Base::Struct) {
            const StructDef* s=fx_.findStruct(t.structName);
            if(!s) fail("Unknown struct "+t.structName);
            out.cls=5;out.rows=1;
            for(const auto& m:s->members) {
                uint32_t e=0;
                if(m.arraySize) {Constant c;if(!eval(*m.arraySize,c,nullptr) || c.v.size()!=1) fail("Non-constant member array");e=uint32_t(c.v[0]);}
                out.members.push_back(type(m.type,m.name,e));
                out.columns+=words(out.members.back());
            }
        } else out.cls=4;
        return out;
    }
    // FX2 value size: numeric rows padded to four components.
    static uint32_t words(const Type& t) {
        uint32_t n=std::max(1u,t.elements);
        if(t.cls==5) {uint32_t sum=0;for(const auto& m:t.members) sum+=words(m);return n*sum;}
        if(t.cls==4) return n;
        return n*t.rows*4;
    }
    static uint32_t word(double v,uint32_t type) {
        if(type==3) {float f=float(v);uint32_t w;std::memcpy(&w,&f,4);return w;}
        if(type==1) return v!=0?1u:0u;
        return uint32_t(int64_t(v)); // 0xffffffff literals stay unsigned.
    }
    Words numericWords(const Type& t,const Expr* init,int line) const {
        Words w(words(t),0);
        if(!init) return w;
        if(t.cls==5) fail("Struct initializers are not supported",line);
        Constant c;if(!eval(*init,c,nullptr)) fail("Non-constant initializer",line);
        uint32_t n=std::max(1u,t.elements),per=t.rows*t.columns;
        if(c.v.size()==1) c.v=std::vector<double>(size_t(n)*per,double(c.v[0]));
        size_t k=0;
        for(uint32_t e=0;e<n;++e) for(uint32_t r=0;r<t.rows;++r) for(uint32_t col=0;col<t.columns;++col)
            if(k<c.v.size()) w[(e*t.rows+r)*4+col]=word(c.v[k++],t.type);
        return w;
    }

    // Constant folding of literals, keywords, constructors and static constants.
    bool eval(const Expr& e,Constant& out,const StateInfo* state) const {
        out=Constant();
        switch(e.kind) {
            case Expr::Number: out.v={e.number};out.isFloat=!e.integer;return true;
            case Expr::Bool: out.v={e.number};return true;
            case Expr::Null: out.v={0};return true;
            case Expr::Ident: {
                uint32_t kw;
                if(state && stateKeyword(*state,e.text,kw)) {out.v={double(kw)};return true;}
                const Variable* g=global(e.text);
                if(g && g->has("static") && g->init) {
                    if(!eval(*g->init,out,nullptr)) return false;
                    out.isFloat=g->type.base==Base::Float || g->type.base==Base::Half || g->type.base==Base::Double;
                    return true;
                }
                return false;
            }
            case Expr::Unary: {
                if(!eval(*e.args[0],out,state)) return false;
                for(auto& x:out.v) x=e.text=="-"?-x:e.text=="!"?double(x==0):e.text=="~"?double(~int64_t(x)):x;
                return true;
            }
            case Expr::Binary: {
                Constant a,b;if(!eval(*e.args[0],a,state) || !eval(*e.args[1],b,state)) return false;
                size_t n=std::max(a.v.size(),b.v.size());
                if(a.v.size()==1) a.v=std::vector<double>(n,double(a.v[0]));
                if(b.v.size()==1) b.v=std::vector<double>(n,double(b.v[0]));
                if(a.v.size()!=b.v.size()) return false;
                out.isFloat=a.isFloat || b.isFloat;out.v.resize(n);
                const std::string& op=e.text;
                for(size_t i=0;i<n;++i) {
                    double x=a.v[i],y=b.v[i],r=0;
                    if(op=="+") r=x+y; else if(op=="-") r=x-y; else if(op=="*") r=x*y;
                    else if(op=="/") {if(y==0) return false;r=out.isFloat?x/y:double(int64_t(x)/int64_t(y));}
                    else if(op=="%") {if(y==0) return false;r=double(int64_t(x)%int64_t(y));}
                    else if(op=="|") r=double(int64_t(x)|int64_t(y)); else if(op=="&") r=double(int64_t(x)&int64_t(y));
                    else if(op=="^") r=double(int64_t(x)^int64_t(y));
                    else if(op=="<") r=x<y; else if(op==">") r=x>y; else if(op=="<=") r=x<=y; else if(op==">=") r=x>=y;
                    else if(op=="==") r=x==y; else if(op=="!=") r=x!=y;
                    else if(op=="&&") r=x!=0 && y!=0; else if(op=="||") r=x!=0 || y!=0;
                    else return false;
                    out.v[i]=r;
                }
                if(op=="<" || op==">" || op=="<=" || op==">=" || op=="==" || op=="!=" || op=="&&" || op=="||") out.isFloat=false;
                return true;
            }
            case Expr::Ternary: {
                Constant c;if(!eval(*e.args[0],c,state) || c.v.size()!=1) return false;
                return eval(*e.args[c.v[0]!=0?1:2],out,state);
            }
            case Expr::Construct: case Expr::List: {
                for(const auto& a:e.args) {Constant c;if(!eval(*a,c,state)) return false;out.v.insert(out.v.end(),c.v.begin(),c.v.end());out.isFloat|=c.isFloat;}
                if(e.kind==Expr::Construct) out.isFloat=e.type.base==Base::Float || e.type.base==Base::Half || e.type.base==Base::Double;
                if(e.kind==Expr::Construct && e.type.numeric() && out.v.size()==1) out.v=std::vector<double>(size_t(e.type.rows)*e.type.cols,double(out.v[0]));
                return true;
            }
            case Expr::Call: {
                std::vector<Constant> a(e.args.size());
                for(size_t i=0;i<e.args.size();++i) if(!eval(*e.args[i],a[i],state)) return false;
                const std::string& f=e.text;
                auto unary=[&](double(*fn)(double))->bool {
                    if(a.size()!=1) return false;out=a[0];for(auto& x:out.v) x=fn(x);out.isFloat=true;return true;
                };
                if(f=="sqrt") return unary([](double x){return std::sqrt(x);});
                if(f=="rsqrt") return unary([](double x){return 1/std::sqrt(x);});
                if(f=="abs") return unary([](double x){return std::fabs(x);});
                if(f=="sin") return unary([](double x){return std::sin(x);});
                if(f=="cos") return unary([](double x){return std::cos(x);});
                if(f=="exp") return unary([](double x){return std::exp(x);});
                if(f=="log") return unary([](double x){return std::log(x);});
                if(f=="radians") return unary([](double x){return x*3.14159265358979323846/180;});
                if((f=="min" || f=="max" || f=="pow") && a.size()==2 && a[0].v.size()==a[1].v.size()) {
                    out=a[0];out.isFloat=a[0].isFloat || a[1].isFloat || f=="pow";
                    for(size_t i=0;i<out.v.size();++i) out.v[i]=f=="min"?std::min(a[0].v[i],a[1].v[i]):f=="max"?std::max(a[0].v[i],a[1].v[i]):std::pow(a[0].v[i],a[1].v[i]);
                    return true;
                }
                return false;
            }
            case Expr::Cast: {
                if(!eval(*e.args[0],out,state)) return false;
                bool f=e.type.base==Base::Float || e.type.base==Base::Half || e.type.base==Base::Double;
                if(!f) for(auto& x:out.v) x=e.type.base==Base::Bool?double(x!=0):double(int64_t(x));
                out.isFloat=f;
                if(e.type.numeric() && out.v.size()==1) out.v=std::vector<double>(size_t(e.type.rows)*e.type.cols,double(out.v[0]));
                return true;
            }
            default: return false;
        }
    }

    Value annotation(const Variable& a) {
        Value v;v.type=type(a.type,a.name,arraySize(a));
        if(a.type.base==Base::String) {
            if(!a.init || (a.init->kind!=Expr::String && a.init->kind!=Expr::List)) fail("String annotation without a string literal: "+a.name,a.line);
            auto items=strings(*a.init); // String arrays (.mfx metadata): one object per string.
            for(const auto& it:items) v.words.push_back(object(it));
            v.text=items.empty()?"":items[0];
        } else if(a.type.numeric()) v.words=numericWords(v.type,a.init.get(),a.line);
        else fail("Unsupported annotation type: "+a.name,a.line);
        return v;
    }
    std::vector<Value> annotations(const std::vector<Variable>& in) {
        std::vector<Value> out;for(const auto& a:in) out.push_back(annotation(a));return out;
    }

    Parameter parameter(const Variable& g) {
        Parameter p;Value& v=p.value;
        v.type=type(g.type,g.name,arraySize(g));v.semantic=g.semantic;
        Base b=g.type.base;
        if(g.type.numeric() || b==Base::Struct) v.words=numericWords(v.type,g.init.get(),g.line);
        else if(b==Base::String) {
            // One string object per element; Value::text is the first (as the cache exporter reads it).
            std::vector<std::string> items=g.init?strings(*g.init):std::vector<std::string>{""};
            for(const auto& it:items) v.words.push_back(object(it));
            v.text=items.empty()?"":items[0];
        } else if(b>=Base::Texture && b<=Base::TextureCube) {
            for(uint32_t i=0;i<std::max(1u,v.type.elements);++i) v.words.push_back(object());
        } else if(b>=Base::Sampler && b<=Base::SamplerCube) {
            if(g.init) {
                if(g.init->kind!=Expr::SamplerState) fail("Unsupported sampler initializer: "+g.name,g.line);
                for(const auto& st:g.init->states) {
                    const StateInfo* info=samplerState(st.name);
                    if(!info) fail("Unknown sampler state "+st.name,st.line);
                    v.stateIds.push_back(info->id);
                    v.states.push_back(stateValue(*info,*st.value,st.line,true));
                }
            }
        } else if(b==Base::VertexShader || b==Base::PixelShader) {
            uint32_t stage=b==Base::VertexShader?VertexShader:PixelShader;
            std::vector<const Expr*> items;
            if(g.init && g.init->kind==Expr::List) for(const auto& a:g.init->args) items.push_back(a.get());
            else if(g.init) items.push_back(g.init.get());
            for(const auto* e:items) v.words.push_back(shaderObject(stage,*e,g.line));
        } else fail("Unsupported parameter type: "+g.name,g.line);
        p.annotations=annotations(g.annotations);
        return p;
    }

    uint32_t shaderObject(uint32_t stage,const Expr& e,int line) {
        if(e.kind==Expr::Null) return 0;
        ShaderJob job;std::string error;
        if(!shaderJob(fx_,text_,e,stage,job,error)) fail(error,line);
        uint32_t id=object();
        Shader s;s.object=id;s.stage=stage;s.entry="ShaderFunction"+std::to_string(id);
        if(report_) ++report_->shaders;
        auto cached=compiled_.find(job.key);
        bool ok=true;
        if(cached!=compiled_.end()) s.bytecode=cached->second;
        else {
            std::string& t=shaderText_[stage==VertexShader?1:0];
            if(t.empty()) t=compilerText(fx_,text_,stage);
            if(be_.compile(job,t,s.bytecode,error)) compiled_[job.key]=s.bytecode;
            else ok=false;
        }
        if(!ok && stage==VertexShader && report_ && report_->higherVertexProfile && job.profile.rfind("vs_",0)==0 &&
           job.profile!="vs_3_0" && (error.find("E5017")!=std::string::npos || error.find("E9015")!=std::string::npos)) {
            // BuildReport::higherVertexProfile: the requested profile's lowering is the gap, not
            // the source; compile it as vs_3_0 and keep reporting the requested version.
            ShaderJob high=job;high.profile="vs_3_0";high.key="vs_3_0|"+job.key;
            std::string error3;Bytes code3;
            std::string& t=shaderText_[1];
            if(t.empty()) t=compilerText(fx_,text_,stage);
            if(be_.compile(high,t,code3,error3)) {
                s.bytecode=std::move(code3);compiled_[job.key]=s.bytecode;ok=true;++report_->higherVertexProfileUses;
            } else error+=" (vs_3_0: "+error3+")";
        }
        if(ok && !be_.reflect(s,{},error)) ok=false;
        if(ok && job.profile.size()==6 && job.profile[2]=='_' && job.profile[4]=='_' && s.major==3 && job.profile!="vs_3_0" && stage==VertexShader) {
            // Reported version: the requested profile (see BuildReport::higherVertexProfile).
            s.major=uint32_t(job.profile[3]-'0');
            s.minor=job.profile[5]=='x'?1u:uint32_t(job.profile[5]-'0');
        }
        if(!ok) {
            std::string what=(job.function.empty()?std::string("asm"):job.profile+" "+job.function)+": "+error;
            if(!report_ || !report_->tolerateShaderFailures) fail(what,line);
            report_->shaderFailures.push_back(what);
            s.bytecode.clear();s.msl.clear();
        } else if(report_) ++report_->compiled;
        d_.shaders.push_back(std::move(s));
        return id;
    }

    // A parameter reference as written: name, <name>, (name), name[i] / (name[i]).
    bool reference(const Expr& e,std::string& name,const Expr*& index) const {
        index=nullptr;
        if(e.kind==Expr::Ident) {name=e.text;return index_.count(name)!=0;}
        if(e.kind==Expr::Reference) {name=e.text;if(!e.args.empty()) index=e.args[0].get();return index_.count(name)!=0;}
        if(e.kind==Expr::Index && e.args[0]->kind==Expr::Ident) {name=e.args[0]->text;index=e.args[1].get();return index_.count(name)!=0;}
        return false;
    }

    Value stateValue(const StateInfo& info,const Expr& e,int line,bool sampler) {
        Value v;
        if(info.value==StateValue::VertexShader || info.value==StateValue::PixelShader) {
            uint32_t stage=info.value==StateValue::VertexShader?VertexShader:PixelShader;
            if(e.kind==Expr::Null || (e.kind==Expr::Number && e.number==0)) {v.type.type=Int;v.type.cls=2;v.type.rows=v.type.columns=1;v.words={0,0,0,0};return v;}
            v.type.type=stage;v.type.cls=4;
            if(e.kind==Expr::Compile || e.kind==Expr::Asm) {v.words={shaderObject(stage,e,line)};return v;}
            std::string name;const Expr* index;
            if(!reference(e,name,index)) fail("Unsupported shader assignment: "+source(e),line);
            Constant c;uint32_t element=0;
            uint32_t id=object();
            Shader s;s.object=id;s.stage=stage;s.selectorKind=2;s.selectorParameter=uint32_t(index_.at(name));
            if(index && !(eval(*index,c,nullptr) && c.v.size()==1)) {
                // D3DX9: an array indexed by a parameter (xpack RaShaderSTM.fx
                // `(vsStaticMesh_AccumBuffer_Array[ PointSun ])`, TerrainShader_Hi.fx
                // `[NightTimeMode]`, `[ShadowCount]`) is a shader selector whose index the effect
                // evaluates when the pass is applied (preshader): mov out0, <parameter>.
                std::string iname;const Expr* inner;
                if(!reference(*index,iname,inner) || inner) fail("Unsupported shader array index: "+source(e),line);
                const Variable& ig=*globals_[index_.at(iname)];
                if(!ig.type.numeric() || ig.type.matrix || ig.type.vector || ig.arraySize)
                    fail("Unsupported shader array index type: "+source(e),line);
                Binding b;b.type=type(ig.type,iname,1);b.set=2;b.index=0;b.count=1;
                Instruction mov;mov.opcode=1;mov.elements=1;mov.operands.resize(2);mov.operands[0].kind=0;mov.operands[1].kind=1;
                s.preshader.inputs=1;s.preshader.bindings={b};s.preshader.instructions={mov};
                s.selectorKind=1;
            } else if(index) element=uint32_t(c.v[0]);
            s.selectorElement=element;
            d_.shaders.push_back(std::move(s));
            v.words={id};return v;
        }
        if(info.value==StateValue::Texture) {
            std::string name;const Expr* index;
            if(e.kind==Expr::Null) {v.type.type=Texture;v.type.cls=4;v.words={0};return v;}
            if(!reference(e,name,index) || index) fail("Unsupported texture assignment: "+source(e),line);
            v.type.type=Texture;v.type.cls=4;v.words={object(name)};return v;
        }
        if(info.value==StateValue::Sampler) {
            std::string name;const Expr* index;
            if(!reference(e,name,index) || index) fail("Unsupported sampler assignment: "+source(e),line);
            const Variable& g=*globals_[index_.at(name)];
            v.type=type(g.type,"",0);v.expressionType=1;v.reference=name;
            v.expression.assign(name.begin(),name.end());v.expression.push_back(0);
            return v;
        }
        Constant c;
        if(eval(e,c,&info)) {
            // [CHOSEN, not recovered] Shader-constant states (Vertex/PixelShaderConstant*, ids
            // 148..163) take a whole register: an immediate vector (TerrainShader_Low.fx
            // "PixelShaderConstantF[1] = vec4(0.5,0.5,0.5,1)") is stored as a 1xN vector value; the
            // runtime sizes the upload from the value's words (16 bytes a register).
            if(c.v.size()>1 && c.v.size()<=4 && info.id>=148 && info.id<=163) {
                v.type.type=c.isFloat?Float:Int;v.type.cls=1;v.type.rows=1;v.type.columns=uint32_t(c.v.size());
                v.words.clear();
                for(double x:c.v) v.words.push_back(word(x,v.type.type));
                return v;
            }
            if(c.v.size()!=1) fail("Vector constant for a scalar state: "+source(e),line);
            v.type.type=c.isFloat?Float:Int;v.type.cls=2;v.type.rows=v.type.columns=1;
            v.words={word(c.v[0],v.type.type),0,0,0};
            return v;
        }
        // State expression: a parameter reference, evaluated by the runtime (mov out, input).
        std::string name;const Expr* index;
        if(!reference(e,name,index) || index) fail("Unsupported state expression: "+source(e),line);
        const Variable& g=*globals_[index_.at(name)];
        if(!g.type.numeric() || g.type.matrix || g.arraySize) fail("Unsupported state expression type: "+source(e),line);
        uint32_t n=g.type.vector?g.type.cols:1;
        Binding b;b.type=type(g.type,name,1);b.set=2;b.index=0;b.count=1; // CTAB-style: elements 1.
        Instruction mov;mov.opcode=1;mov.elements=n;mov.operands.resize(2);mov.operands[0].kind=0;mov.operands[1].kind=1;
        v.program.inputs=1;v.program.bindings={b};v.program.instructions={mov};
        v.type.type=symbolType(g.type.base);v.type.cls=2;v.type.rows=n;v.type.columns=1;
        v.expressionType=0;
        std::string payload="BF2FX-native-expression:"+source(e);
        v.expression.assign(payload.begin(),payload.end());
        (void)sampler;
        return v;
    }

    State passState(const StateAssign& st) {
        const StateInfo* info=passState(st.name);
        if(!info) fail("Unknown state "+st.name,st.line);
        State out;out.id=info->id;
        if(st.index) {
            Constant c;if(!eval(*st.index,c,nullptr) || c.v.size()!=1) fail("Non-constant state index: "+st.name,st.line);
            out.index=uint32_t(c.v[0]);
            if(info->indexCount<2) fail("State is not indexed: "+st.name,st.line);
        }
        out.value=stateValue(*info,*st.value,st.line,false);
        return out;
    }
    static const StateInfo* passState(const std::string& n) { return bf2fx::passState(n); }
};
} // namespace

bool build(const std::string& text,ShaderBackend& backend,Description& out,std::string& error,BuildReport* report) {
    Effect fx;Error perr;
    if(!parse(text,fx,perr)) {error="parse: line "+std::to_string(perr.line)+": "+perr.message;return false;}
    if(fx.techniques.empty() && (!report || !report->allowNoTechniques)) {error="There were no techniques";return false;}
    try {
        Builder b(fx,text,backend,report);out=b.run();
    } catch(const BuildError& e) {error=e.what();return false;}
    return true;
}
} // namespace bf2fx
#endif
