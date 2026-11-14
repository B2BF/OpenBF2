// [CHOSEN, not recovered] G4 shader objects over offline-translated FX2 shader records.
// Included only in Device.cpp after Resources.inc.
static const GUID iidVertexShader={0xefc5557e,0x6265,0x4613,{0x8a,0x94,0x43,0x85,0x78,0x89,0xeb,0x36}};
static const GUID iidPixelShader={0x6d3bdbdc,0x5b02,0x4415,{0xb8,0x52,0xce,0x5e,0x8b,0xcc,0xb2,0x89}};
thread_local std::shared_ptr<const bf2fx::Shader> offeredShader;

// Per-draw D3D9 state that changes a fragment program (see DeviceDraw.inc).
//  - fog: D3DRS_FOGENABLE after a ps_1_x/ps_2_x shader or the fixed-function pixel stages.
//    0 off; 1 vertex fog from the vertex shader's oFog; 2 vertex fog with oFog unwritten (the
//    factor is 0, fully fogged: Wine visual.c fog_with_shader_test, Windows rows); 3 table fog
//    (D3DRS_FOGTABLEMODE EXP/EXP2/LINEAR) from the fragment depth.
//  - samplers: ps_1_x bytecode declares no sampler dimension; D3D9 samples whatever texture is
//    bound. MojoShader types every such sampler 2D, so a slot bound to a cube or volume texture
//    is retyped for the draw (slot -> 1 cube, 2 volume).
struct FragmentVariant {
    unsigned fog=0;
    std::vector<std::pair<unsigned,unsigned>> samplers;
    // D3DSAMP_MIPMAPLODBIAS per pixel sampler (non-zero only): Metal sampler states carry no LOD
    // bias, so the shader's samples take it as MSL bias() (lodBiasSamples).
    std::vector<std::pair<unsigned,float>> bias;
    // D3DRS_SHADEMODE = D3DSHADE_FLAT: the colour inputs (COLOR0/COLOR1) take the provoking
    // (first) vertex's value, as D3D9 flat shading does for oD0/oD1 and color-semantic inputs.
    bool flat=false;
    std::string key() const {
        std::string k=std::to_string(fog)+(flat?"F":"");
        for(const auto& s:samplers) k+=":"+std::to_string(s.first)+"="+std::to_string(s.second);
        char b[48];
        for(const auto& s:bias) { std::snprintf(b,sizeof(b),"|b%u=%a",s.first,double(s.second)); k+=b; }
        return k;
    }
};
// The argument list of a call starting at `open` (just after '('): returns the index of the
// matching ')' or npos.
static std::size_t closingParen(const std::string& t,std::size_t open) {
    int depth=1;
    for(std::size_t i=open;i<t.size();++i) {
        if(t[i]=='(') ++depth;
        else if(t[i]==')' && --depth==0) return i;
    }
    return std::string::npos;
}
// The MSL literal of a bias value (exact: hexadecimal float).
static std::string biasLiteral(float v) { char b[48]; std::snprintf(b,sizeof(b),"%a",double(v)); return b; }
// D3D9 D3DSAMP_MIPMAPLODBIAS: the bias is added to the level of detail of every sample of the
// sampler (texld/texldp and the ps_1_x tex forms; a texldb shader bias and a texldl explicit LOD
// add to it, as the sampler bias adds to both in the reference rasterizer and GL). `sample(sN,
// coord)` becomes `sample(sN, coord, bias(B))`; Metal clamps the biased LOD to the sampler's mip
// range as D3D9 clamps it to MAXMIPLEVEL..last level. [UNRESOLVED] texldd (gradient2d) cannot
// take a bias in MSL; MojoShader emits no gradient sample for BF2's effects.
static bool lodBiasSamples(std::string& msl,unsigned slot,float value,std::string& error) {
    const std::string n=std::to_string(slot), call="s"+n+"_texture.sample(s"+n+", ";
    for(std::size_t at=msl.find(call);at!=std::string::npos;at=msl.find(call,at+1)) {
        const std::size_t a=at+call.size(), e=closingParen(msl,a);
        if(e==std::string::npos) { error="unterminated sample of sampler "+n; return false; }
        const std::string arg=msl.substr(a,e-a);
        std::string patched;
        const std::size_t b=arg.find(", bias(");
        if(b!=std::string::npos) patched=arg.substr(0,b+7)+biasLiteral(value)+" + "+arg.substr(b+7);
        else if(arg.find(", level(")!=std::string::npos) { const std::size_t l=arg.find(", level("); patched=arg.substr(0,l+8)+biasLiteral(value)+" + "+arg.substr(l+8); }
        else if(arg.find(", gradient")!=std::string::npos) patched=arg;
        else patched=arg+", bias("+biasLiteral(value)+")";
        msl.replace(a,e-a,patched);
    }
    return true;
}
// Retypes MojoShader's ps_1_x 2D sampler `sN` to a cube (kind 1) or volume (kind 2) texture.
// SM1 samples come in two forms: `sN_texture.sample(sN, X.xy)` and, for texld with _dw/_dz,
// `sN_texture.sample(sN, X.xy / X.w)`; both gain the third coordinate. Any other use of the
// sampler (texbem, texm3x2tex) is 2D-only and refused by name.
static bool retypeSampler(std::string& msl,unsigned slot,unsigned kind,std::string& error) {
    const std::string n=std::to_string(slot), tex="s"+n+"_texture";
    const std::string decl="texture2d<float> "+tex+" [[texture("+n+")]]";
    const std::size_t d=msl.find(decl);
    if(d==std::string::npos) { error="ps_1_x sampler "+n+" is not a 2D texture argument"; return false; }
    msl.replace(d,std::strlen("texture2d"),kind==1?"texturecube":"texture3d");
    const std::string call=tex+".sample(s"+n+", ";
    std::size_t uses=0;
    for(std::size_t at=msl.find(tex);at!=std::string::npos;at=msl.find(tex,at+1)) ++uses;
    std::size_t samples=0;
    for(std::size_t at=msl.find(call);at!=std::string::npos;at=msl.find(call,at+1)) {
        const std::size_t a=at+call.size(), e=closingParen(msl,a);
        if(e==std::string::npos) { error="unterminated sample of sampler "+n; return false; }
        std::string arg=msl.substr(a,e-a);
        const std::size_t div=arg.find(" / ");
        std::string coord=div==std::string::npos?arg:arg.substr(0,div);
        if(coord.size()<4 || coord.compare(coord.size()-3,3,".xy")) {
            error="ps_1_x sample form \""+arg+"\" on a "+(kind==1?"cube":"volume")+" texture"; return false;
        }
        coord+="z";
        arg=coord+(div==std::string::npos?"":arg.substr(div));
        msl.replace(a,e-a,arg); ++samples;
    }
    if(uses!=samples+1) { error="ps_1_x sampler "+n+" used outside a plain sample on a "+(kind==1?"cube":"volume")+" texture"; return false; }
    return true;
}
// Patches MojoShader's MSL for D3D9 conventions:
//  - vertex: D3D9 samples pixel centres at integer window coordinates, Metal at +0.5. The clip
//    position is moved by +0.5 pixel right/down and remapped from the D3D viewport to the
//    clamped Metal viewport: xy = xy * bf2_fixup.zw + bf2_fixup.xy * w (see DeviceDraw.inc).
//    D3D9 and Metal share y-up NDC, a top-left window origin and 0..w clip depth. The position
//    output is [[invariant]] (D3D9 multipass depth invariance, DrawBridge.inc). Below vs_3_0
//    the colour outputs are saturated, as D3D9 clamps oD0/oD1.
//  - fragment: the variant (fog, ps_1_x sampler types), then D3DRS_ALPHATEST* as a discard on
//    the 8-bit colour-0 alpha (bf2_alpha = {func, ref}). Fog blends colour 0's rgb with
//    D3DRS_FOGCOLOR by the factor f: rgb = mix(fogColor, rgb, saturate(f)); alpha is unchanged.
//    bf2_fog[0] = fog colour, bf2_fog[1] = {table mode, end, 1/(end-start), density}.
static bool patchMsl(const std::string& source,bool vertex,std::string& out,std::string& error,
                     const FragmentVariant* variant=nullptr,bool clampColors=false) {
    std::string msl=source;
    if(variant) for(const auto& s:variant->samplers) if(!retypeSampler(msl,s.first,s.second,error)) return false;
    if(variant) for(const auto& s:variant->bias) if(!lodBiasSamples(msl,s.first,s.second,error)) return false;
    if(variant && variant->flat && !vertex)
        for(const char* user:{"[[user(color0)]]","[[user(color1)]]"})
            for(std::size_t at=msl.find(user);at!=std::string::npos;at=msl.find(user,at+1))
                msl.replace(at,std::strlen(user),std::string(user).substr(0,std::strlen(user)-2)+", flat]]");
    const std::string keyword=vertex?"vertex ":"fragment ";
    std::size_t sig=msl.rfind("\n"+keyword); if(sig==std::string::npos) { error="no "+keyword+"entry"; return false; }
    std::size_t close=msl.find("\n) {",sig);
    if(close==std::string::npos) { error="unrecognized entry signature"; return false; }
    auto memberBefore=[&](const std::string& attribute)->std::string {
        const std::size_t at=msl.find(attribute); if(at==std::string::npos) return std::string();
        std::size_t end=msl.rfind(' ',at); if(end==std::string::npos) return std::string();
        while(end && msl[end-1]==' ') --end;
        std::size_t begin=end; while(begin && (std::isalnum((unsigned char)msl[begin-1]) || msl[begin-1]=='_')) --begin;
        return msl.substr(begin,end-begin);
    };
    std::string arguments=vertex?"constant float4 &bf2_fixup [[buffer(20)]]":"constant float4 &bf2_alpha [[buffer(1)]]";
    std::string fogCode;
    const unsigned fog=variant?variant->fog:0;
    if(!vertex && fog) {
        const std::string color=memberBefore("[[color(0)]]");
        if(color.empty()) { error="fog on a pixel shader without colour 0"; return false; }
        if(msl.find("[[user(fog)]]")!=std::string::npos) { error="pixel shader reads the fog register"; return false; }
        std::string factor="0.0";
        if(fog==1 || fog==3) {
            // A fragment input that already holds [[position]] (the fixed-function stage's
            // placeholder) is reused; MojoShader's ps_1_x/ps_2_x inputs never contain one.
            const std::string existing=fog==3?memberBefore("[[position]]"):std::string();
            const std::string field=fog==1?"bf2_fog_in":existing.empty()?"bf2_fog_pos":existing;
            const std::string member=fog==1?"float4 bf2_fog_in [[user(fog)]];":existing.empty()?"float4 bf2_fog_pos [[position]];":"";
            const std::size_t in=msl.find("[[stage_in]]",sig);
            std::string param;
            if(in!=std::string::npos && in<close) {
                std::size_t e=in; while(e && msl[e-1]==' ') --e;
                std::size_t b=e; while(b && (std::isalnum((unsigned char)msl[b-1]) || msl[b-1]=='_')) --b;
                param=msl.substr(b,e-b);
                std::size_t te=b; while(te && msl[te-1]==' ') --te;
                std::size_t tb=te; while(tb && (std::isalnum((unsigned char)msl[tb-1]) || msl[tb-1]=='_')) --tb;
                const std::string type=msl.substr(tb,te-tb);
                const std::size_t st=msl.find("struct "+type);
                const std::size_t brace=st==std::string::npos?st:msl.find('{',st);
                if(brace==std::string::npos || brace>sig) { error="fragment input struct "+type; return false; }
                if(!member.empty()) msl.insert(brace+1,"\n\t"+member);
            } else {
                param="bf2_fog_stage";
                msl.insert(sig+1,"struct bf2_fog_stage_in {\n\t"+member+"\n};\n");
                arguments+=",\n\tbf2_fog_stage_in bf2_fog_stage [[stage_in]]";
            }
            const std::string v=param+"."+field;
            if(fog==1) factor=v+".x";
            else factor="(bf2_fog[1].x == 3.0 ? (bf2_fog[1].y - "+v+".z) * bf2_fog[1].z : "
                        "bf2_fog[1].x == 1.0 ? exp(-bf2_fog[1].w * "+v+".z) : "
                        "exp(-(bf2_fog[1].w * "+v+".z) * (bf2_fog[1].w * "+v+".z)))";
        }
        fogCode="output."+color+".rgb = mix(bf2_fog[0].rgb, output."+color+".rgb, saturate(float("+factor+")));\n\t";
        arguments+=",\n\tconstant float4 *bf2_fog [[buffer(3)]]";
    }
    sig=msl.rfind("\n"+keyword); close=msl.find("\n) {",sig);
    const std::size_t ret=msl.rfind("return output;");
    std::string code;
    if(vertex) {
        const std::string pos=memberBefore("[[position]]");
        if(pos.empty() || ret==std::string::npos) { error="vertex shader writes no position"; return false; }
        code="output."+pos+".xy = output."+pos+".xy * bf2_fixup.zw + bf2_fixup.xy * output."+pos+".w;\n\t";
        // vs_1_1..vs_2_x colour outputs oD0/oD1 are clamped to [0,1] before interpolation
        // (the pixel shader's v0/v1 are 0..1); MojoShader writes them unclamped.
        if(clampColors) for(const char* user:{"[[user(color0)]]","[[user(color1)]]"}) {
            const std::string m=memberBefore(user);
            if(!m.empty()) code+="output."+m+" = saturate(output."+m+");\n\t";
        }
    } else {
        const std::string color=memberBefore("[[color(0)]]");
        if(!color.empty() && ret!=std::string::npos)
            code=fogCode+"{ const uint bf2_f = uint(bf2_alpha.x); if (bf2_f != 0u) {\n"
                "\t\tconst float bf2_a = rint(saturate(output."+color+".a) * 255.0), bf2_r = bf2_alpha.y; bool bf2_pass;\n"
                "\t\tswitch (bf2_f) { case 1u: bf2_pass = false; break; case 2u: bf2_pass = bf2_a < bf2_r; break;\n"
                "\t\tcase 3u: bf2_pass = bf2_a == bf2_r; break; case 4u: bf2_pass = bf2_a <= bf2_r; break;\n"
                "\t\tcase 5u: bf2_pass = bf2_a > bf2_r; break; case 6u: bf2_pass = bf2_a != bf2_r; break;\n"
                "\t\tcase 7u: bf2_pass = bf2_a >= bf2_r; break; default: bf2_pass = true; break; }\n"
                "\t\tif (!bf2_pass) discard_fragment(); } }\n\t";
    }
    if(!code.empty()) msl.insert(ret,code);
    // The signature line ends in "(" when the entry has no other arguments.
    msl.insert(close,(msl[close-1]=='(' ? "\n\t" : ",\n\t")+arguments);
    out="#include <metal_stdlib>\n"+msl;
    // D3D9's position invariance across passes (DrawBridge.inc compileProgram).
    if(vertex && !std::getenv("BF2_METAL_NO_INVARIANCE")) { // [TEMP] c-shadows A/B switch
        const std::size_t at=out.find("[[position]]");
        if(at!=std::string::npos) out.replace(at,12,"[[position, invariant]]");
    }
    return true;
}
static bool patchMsl(const bf2fx::Shader& s,bool vertex,std::string& out,std::string& error) {
    return patchMsl(s.msl,vertex,out,error,nullptr,vertex && s.major<3);
}
template<class I> class ShaderObject final : public I, public Life {
public:
    std::shared_ptr<const bf2fx::Shader> record;
    bf2metal::Program* program=nullptr;
    std::map<std::string,bf2metal::Program*> variants; // fragment variants by FragmentVariant::key
    const bool vertex;
    ShaderObject(IDirect3DDevice9* d,bf2metal::Frame* f,bool v,std::shared_ptr<const bf2fx::Shader> r)
        : Life(d,f,v?"IDirect3DVertexShader9":"IDirect3DPixelShader9"),record(std::move(r)),vertex(v) {}
    ~ShaderObject() override {
        if(program) bf2metal::destroyProgram(program);
        for(auto& v:variants) bf2metal::destroyProgram(v.second);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return add(); }
    ULONG STDMETHODCALLTYPE Release() override { return release(); }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override { return device(out); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        if(!sameIID(id,iidUnknown) && !sameIID(id,vertex?iidVertexShader:iidPixelShader)) return E_NOINTERFACE;
        *out=static_cast<I*>(this); add(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFunction(void* data,UINT* size) override {
        if(!size) return D3DERR_INVALIDCALL;
        const UINT n=UINT(record->bytecode.size());
        if(!data) { *size=n; return D3D_OK; }
        if(*size<n) return D3DERR_INVALIDCALL;
        std::memcpy(data,record->bytecode.data(),n); *size=n; return D3D_OK;
    }
};
using VertexShaderObject=ShaderObject<IDirect3DVertexShader9>;
using PixelShaderObject=ShaderObject<IDirect3DPixelShader9>;
} // namespace

namespace bf2metal {
ShaderOffer::ShaderOffer(std::shared_ptr<const bf2fx::Shader> record) { offeredShader=std::move(record); }
ShaderOffer::~ShaderOffer() { offeredShader.reset(); }
std::shared_ptr<const bf2fx::Shader> ShaderOffer::take(const std::uint32_t* tokens,std::size_t bytes) {
    auto r=offeredShader;
    if(!r || !tokens || r->bytecode.size()!=bytes || std::memcmp(r->bytecode.data(),tokens,bytes)) return nullptr;
    offeredShader.reset(); return r;
}
bool drawTraceEnabled() {
    static const bool on=[]{const char* e=std::getenv("BF2_METAL_DRAW_TRACE"); return e && *e=='1';}();
    return on;
}
static thread_local std::string traceLabel;
void setDrawTraceLabel(const char* label) { traceLabel=label?label:""; }
const char* drawTraceLabel() { return traceLabel.empty()?"(no effect)":traceLabel.c_str(); }
std::size_t shaderLength(const std::uint32_t* t) {
    if(!t) return 0;
    const std::uint32_t version=t[0];
    if((version>>16)!=0xfffe && (version>>16)!=0xffff) return 0;
    const unsigned major=(version>>8)&0xff;
    constexpr std::size_t limit=1u<<20; // tokens
    for(std::size_t i=1;i<limit;) {
        const std::uint32_t token=t[i];
        if(token==0x0000ffffu) return (i+1)*4;
        const std::uint32_t op=token&0xffff;
        if(op==0xfffe) { i+=1+((token>>16)&0x7fff); continue; }          // comment
        if(major>=2) { i+=1+((token>>24)&0xf); continue; }               // SM2+ instruction length
        if(op==0x51) { i+=6; continue; }                                  // SM1 def: dst + four literals
        ++i; while(i<limit && (t[i]&0x80000000u)) ++i;                    // SM1 parameter tokens
    }
    return 0;
}
}
namespace {
