// [TEMP] [CHOSEN, not recovered] Fixtures model only CPU cache/parameter/state contracts.
#include "Runtime.h"
#include "Wire.h"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>

using namespace bf2fx;
namespace fs=std::filesystem;
static unsigned checks=0;
static void check(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
template<class A,class B> static A bits(B b) {A a;static_assert(sizeof(a)==sizeof(b),"size");std::memcpy(&a,&b,sizeof(a));return a;}
struct Resolver:IncludeResolver {
    std::map<std::string,Bytes> files;
    unsigned opened=0,closed=0;
    const void* expectedParent=nullptr;
    bool open(uint32_t,const std::string& name,const void* parent,const void*& bytes,size_t& size) override {
        if(name=="inc.fx")check(parent==nullptr,"Top-level callback receives null parent");
        if(name=="child.fx")check(parent==expectedParent,"Nested callback receives original parent buffer");
        auto it=files.find(name);if(it==files.end())return false;
        bytes=it->second.data();size=it->second.size();++opened;if(name=="inc.fx")expectedParent=bytes;return true;
    }
    void close(const void*) override {++closed;}
};
static Bytes bytes(const std::string& s) {return Bytes(s.begin(),s.end());}
static Request makeRequest(const std::string& source,Resolver* resolver=nullptr,std::vector<Define> defines={}) {
    Request r;check(bool(capture(source.data(),source.size(),defines,0,std::string(64,'a'),"test-translator",resolver,r)),"Capture request");return r;
}
struct Observer:Callbacks {
    unsigned states=0,shaders=0,constantsCalls=0,samplers=0;
    bool valid=true;
    std::map<uint32_t,Value> stateValues;
    std::map<std::pair<uint32_t,uint32_t>,Value> indexedStates;
    std::map<uint32_t,unsigned> samplerSlots;
    std::map<uint32_t,uint32_t> shaderObjects;
    std::map<std::string,Registers> registers;
    std::shared_ptr<void> texture;
    void state(uint32_t id,uint32_t index,const Value& v) override {++states;stateValues[id]=v;indexedStates[{id,index}]=v;}
    void shader(uint32_t stage,const Shader* s) override {++shaders;if(s){check(!s->msl.empty(),"Selected shader exposes MSL");shaderObjects[stage]=s->object;}}
    void constants(const Shader& s,const Registers& r) override {++constantsCalls;registers[s.entry]=r;}
    void sampler(uint32_t,uint32_t slot,const Value&,const std::shared_ptr<void>& t) override {++samplers;++samplerSlots[slot];if(t)texture=t;}
    bool validate(const Pass&,const Description&) override {return valid;}
};
static Parameter parameter(const std::string& name,uint32_t type,uint32_t cls,uint32_t rows,uint32_t columns,uint32_t elements=0) {
    Parameter p;p.value.type={name,type,cls,rows,columns,elements,{}};p.value.words.resize(rows*4*std::max(1u,elements));return p;
}
static Description fixture() {
    Description d;d.parameters.push_back(parameter("Tint",Float,1,1,3,2));
    d.parameters.push_back(parameter("Transform",Float,3,2,3,2));
    d.parameters.push_back(parameter("Count",Int,0,1,1));d.parameters.push_back(parameter("Enabled",Bool,0,1,1));
    d.parameters.push_back(parameter("Map",Texture,4,0,0));
    Value annotation;annotation.type.name="UIName";annotation.type.type=String;annotation.text="Tint";d.parameters[0].annotations.push_back(annotation);
    d.techniques.push_back({"Main",{{"P0",{},{}}},{}});return d;
}
static void keyAndCache(const fs::path& scratch) {
    check(sha256(bytes("abc"))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 known answer");
    Resolver resolver;resolver.files["inc.fx"]=bytes("#include \"child.fx\"\n");resolver.files["child.fx"]=bytes("float x;\n");
    std::string source="// #include \"ignored\"\n#include \"inc.fx\"\n#if 0\n#include \"absent.fx\"\n#endif\n";
    auto a=makeRequest(source,&resolver,{{"B","2"},{"A","1"}});
    auto b=makeRequest(source,&resolver,{{"A","1"},{"B","2"}});
    check(a.includes.size()==3 && a.includes[1].parent==1,"Include ordering and parent identities");
    check(a.includes[2].found==0 && resolver.opened==resolver.closed,"Optional include and balanced Close");
    check(cacheKey(a)==cacheKey(b),"Macro order canonicalization");
    auto changed=a;changed.includes[1].bytes.push_back(' ');check(cacheKey(changed)!=cacheKey(a),"Transitive include changes key");
    changed=a;changed.flags=256;check(cacheKey(changed)!=cacheKey(a),"Flags change key");
    changed=a;changed.dllHash[0]='b';check(cacheKey(changed)!=cacheKey(a),"DLL changes key");
    changed=a;changed.translator+="2";check(cacheKey(changed)!=cacheKey(a),"Translator changes key");
    changed=a;changed.includes[0].kind=1;check(cacheKey(changed)!=cacheKey(a),"Include type changes key");
    changed=a;changed.source.push_back(' ');check(cacheKey(changed)!=cacheKey(a),"Root changes key");
    Request invalid;check(!capture(source.data(),source.size(),{{"A","1"},{"A","2"}},0,std::string(64,'a'),"test",&resolver,invalid),"Duplicate macro rejected");
    auto macro=std::string("#include DYNAMIC\n");check(!capture(macro.data(),macro.size(),{},0,std::string(64,'a'),"test",nullptr,invalid),"Macro include refused");
    Entry entry;auto cache=scratch/"cache";auto missing=load(cache.string(),a,entry);check(missing.error==Error::Miss,"Named cache miss");
    check(fs::is_regular_file(cache/"requests.txt"),"Miss requests list written");
    auto requestPath=cache/"requests"/(cacheKey(a)+".request");check(cacheKey(unenvelope<Request>(readFile(requestPath.string()),"request"))==cacheKey(a),"Replayable miss snapshot");
    entry.key=cacheKey(a);entry.fx2={1,9,255,254};entry.description=fixture();check(bool(publish(cache.string(),a,entry)),"Publish cache entry");
    Entry roundtrip;check(bool(load(cache.string(),a,roundtrip)),"Loader cache round trip");
    check(encode(entry)==encode(roundtrip),"Stable description serialization");
    auto binary=a;binary.source=entry.fx2;binary.defines.clear();binary.includes.clear();binary.flags|=0x800;
    check(bool(load(cache.string(),binary,roundtrip)) && roundtrip.fx2==entry.fx2,"Precompiled Ra FX2 runtime flags lookup");
    auto path=cache/"entries"/entry.key/"entry.bin";auto original=readFile(path.string());auto corrupt=original;corrupt.back()^=1;writeFile(path.string(),corrupt);
    check(load(cache.string(),a,roundtrip).error==Error::HashMismatch,"Artifact hash mismatch");
    corrupt=original;corrupt[9]=uint8_t(CacheVersion+1);writeFile(path.string(),corrupt);
    check(load(cache.string(),a,roundtrip).error==Error::VersionMismatch,"Version mismatch");writeFile(path.string(),original);
    auto same=entry;same.key=std::string(64,'b');check(publish(cache.string(),a,same).error==Error::HashMismatch,"Publish key mismatch");
    {   // Refusal records: load reports D3DX's HRESULTs and records no request.
        Request r=a;r.flags^=0x4;Entry refused;refused.key=cacheKey(r);refused.kind=RefusedEntry;
        check(publish(cache.string(),r,refused).error==Error::InvalidInput,"Refusal without HRESULT rejected");
        refused.compileResult=0x80004005u;refused.log="error X4502: test";
        check(bool(publish(cache.string(),r,refused)),"Publish refusal record");
        auto before=readFile((cache/"requests.txt").string());Entry got;auto result=load(cache.string(),r,got);
        check(result.error==Error::Refused && got.kind==RefusedEntry && got.compileResult==0x80004005u && got.log==refused.log,"Load refusal record");
        check(readFile((cache/"requests.txt").string())==before && !fs::exists(cache/"requests"/(refused.key+".request")),"Refusal records no request");
        Request q=a;q.flags^=0x8;Entry reflection;reflection.key=cacheKey(q);reflection.kind=ReflectionOnlyEntry;reflection.compileResult=0x80004005u;
        reflection.description=fixture();check(bool(publish(cache.string(),q,reflection)),"Publish reflection-only record");
        check(bool(load(cache.string(),q,got)) && got.kind==ReflectionOnlyEntry && got.fx2.empty(),"Load reflection-only record");
        auto alias=q;alias.source.clear();alias.source={1,9,255,254};alias.defines.clear();alias.includes.clear();
        check(!fs::exists(cache/"binary"/(cacheKey(alias)+".bin")),"Reflection record publishes no FX2 alias");
    }
    try {auto shortRecord=original;shortRecord.resize(13);unenvelope<Entry>(shortRecord,"entry");check(false,"Truncation rejected");}catch(const std::runtime_error&) {++checks;}
}
static void parameters() {
    Runtime r(fixture());auto tint=r.getParameterByName("Tint");auto element=r.getParameterElement(tint,1);
    check(bool(element) && !r.getParameterElement(tint,2),"Parameter elements bounded");
    check(r.getParameterByName("Tint[1]").element==1 && !r.getParameterByName("Tint[-1]"),"Indexed names");
    ParameterDesc desc;check(r.getParameterDesc(tint,desc) && desc.bytes==24 && desc.annotations==1,"Packed parameter descriptor");
    check(r.getParameterDesc(element,desc) && desc.bytes==12 && desc.type.elements==0,"Element descriptor");
    check(r.getAnnotation(tint,"UIName")->text=="Tint","Annotation by name");
    float array[]={1,2,3,4,5,6},out[6]{};check(r.setFloatArray(tint,array,6) && r.getFloatArray(tint,out,6) && !std::memcmp(array,out,sizeof(out)),"Float array packed/padded round trip");
    check(!r.setFloatArray(tint,array,7),"Array overflow rejected");
    check(r.setVector(element,{{7,8,9,10}}),"Vector element set");std::array<float,4> vector;
    check(r.getVector(element,vector) && vector[0]==7 && vector[2]==9,"Vector round trip");
    float vectors[]={2,3,4,99,5,6,7,99},vectorOutput[8];
    check(r.setVectorArray(tint,vectors,2) && r.getVectorArray(tint,vectorOutput,2) && vectorOutput[4]==5 && vectorOutput[6]==7,"Vector arrays use float4 API strides");
    auto count=r.getParameterByName("Count");int32_t i;float f;bool b;
    check(r.setFloat(count,3.75f) && r.getInt(count,i) && i==3 && r.getFloat(count,f) && f==3,"D3DX numeric conversion");
    auto enabled=r.getParameterByName("Enabled");check(r.setInt(enabled,-4) && r.getBool(enabled,b) && b,"Boolean normalization");
    int32_t booleanValue=-3,booleanOutput=0;
    check(r.setBoolArray(enabled,&booleanValue,1) && r.getBoolArray(enabled,&booleanOutput,1) && booleanOutput==1,"Boolean array conversion");
    int32_t integerValues[]={1,2,3,4,5,6},integerOutput[6]{};
    check(r.setIntArray(tint,integerValues,6) && r.getIntArray(tint,integerOutput,6) && !std::memcmp(integerValues,integerOutput,sizeof(integerValues)),"Integer array conversion through float storage");
    // D3DX9_25 (oracle): SetValue needs at least the parameter's bytes; a float3 element is 12.
    uint32_t raw[3]={0x40400000,0,0};check(!r.setValue(element,raw,4) && r.setValue(element,raw,12) && r.getFloat(element,f) && f==3,"Raw SetValue preserves bit patterns");
    uint32_t rawOutput[3]={};check(!r.getValue(element,rawOutput,4) && r.getValue(element,rawOutput,12) && rawOutput[0]==raw[0],"Raw GetValue round trip");
    auto matrix=r.getParameterByName("Transform");float m[32],result[32];for(unsigned j=0;j<32;++j)m[j]=float(j+1);
    check(r.setMatrixArray(matrix,m,2) && r.getMatrixArray(matrix,result,2) && result[4]==5 && result[18]==19,"Matrix arrays");
    Shader shader;shader.bindings.push_back({fixture().parameters[1].value.type,2,5,6});
    auto regs=r.mapConstants(shader);check(regs.f.size()==44 && regs.f[20]==1 && regs.f[21]==5 && regs.f[24]==2 && regs.f[33]==21,"Column-major CTAB register mapping");
    shader.uniforms={{0,5,6,0,""}};auto packed=r.packConstants(shader,regs);
    check(packed.size()==96 && decode<uint32_t>(Bytes(packed.begin(),packed.begin()+4))==bits<uint32_t>(1.0f),"Dense MSL register packing");
    check(r.setMatrix(matrix,m,true) && r.getMatrix(matrix,result,true) && result[1]==2 && result[4]==5,"Transpose matrix API");
    auto map=r.getParameterByName("Map");auto ptr=std::make_shared<int>(123);std::shared_ptr<void> returned;
    check(r.setTexture(map,ptr) && r.getTexture(map,returned) && returned==ptr,"Texture native ownership");
    check(!r.setTexture(count,ptr),"Wrong texture type rejected");
    Observer observer;check(r.getTechniqueByName("Main")==0 && r.findNextValidTechnique(-1,observer)==0,"Technique lookup and validation callback");
    observer.valid=false;check(r.findNextValidTechnique(-1,observer)==-1,"Device rejection is honored");
}
static void expressions() {
    Runtime runtime(fixture());Program p;p.inputs=1;p.temps=4;p.bindings.push_back({fixture().parameters[2].value.type,2,0,1});
    p.literals={bits<uint64_t>(2.0)};
    p.instructions.push_back({18,1,{{0,0,{}},{2,0,{}},{1,0,{}}}});
    check(runtime.setInt(runtime.getParameterByName("Count"),7),"Set expression input");
    check(runtime.evaluate(p).at(0)==14,"CPU preshader arithmetic and integer-to-float input");
    Value state;state.type.type=Int;state.expression={1};state.expressionType=0;state.program=p;
    check(runtime.evaluate(state).words[0]==14,"Typed dynamic state output");
    p.instructions={{29,4,{{2,0,{}},{0,0,{}},{1,0,{}}}}};
    auto scalar=runtime.evaluate(p);check(scalar[0]==9 && scalar[1]==2,"Scalar-op replication");
    // Runtime.h's current contract reports unsupported IR and throws Refused to direct callers.
    p.instructions[0].opcode=24;p.instructions[0].elements=1;
    bool refused=false;
    try {runtime.evaluate(p);} catch(const Refused& e) {
        refused=std::string(e.what()).find("Runtime::evaluate: Unsupported preshader opcode 24")!=std::string::npos;
    }
    check(refused,"Unsupported IR throws a qualified refusal");
}
// D3DX9_25 struct semantics, from tools/macos/d3dxfx/oracle (run-oracle.sh on structs.fx).
static Type member(const char* name,uint32_t type,uint32_t cls,uint32_t columns) {Type t;t.name=name;t.type=type;t.cls=cls;t.rows=1;t.columns=columns;return t;}
static Parameter structParameter(const char* name,uint32_t elements,std::vector<Type> members) {
    Parameter p;p.value.type.name=name;p.value.type.cls=5;p.value.type.rows=1;p.value.type.elements=elements;p.value.type.members=members;
    p.value.type.columns=uint32_t(members.size()*4);p.value.words.assign(members.size()*4*std::max(1u,elements),0);return p;
}
static void structs() {
    std::vector<Type> light={member("pos",Float,1,3),member("dir",Float,1,3),member("color",Float,1,4),member("specularColor",Float,1,4),member("attenuation",Float,0,1)};
    Description d;d.parameters.push_back(structParameter("Lights",2,light));d.parameters.push_back(structParameter("One",0,light));
    d.parameters.push_back(structParameter("Mix",0,{member("uv",Float,1,2),member("s",Float,0,1),member("n",Int,0,1),member("b",Bool,0,1)}));
    Parameter vec3;vec3.value.type=member("Vec3",Float,1,3);vec3.value.words.assign(4,0);d.parameters.push_back(vec3);
    Parameter vec4;vec4.value.type=member("Vec4",Float,1,4);vec4.value.words.assign(4,0);d.parameters.push_back(vec4);
    Shader vs;vs.object=1;vs.stage=VertexShader;Binding b;b.type.name="Lights";b.type.cls=5;b.type.rows=1;b.type.columns=15;b.type.elements=2;
    for(auto m:light){m.elements=1;b.type.members.push_back(m);}
    b.set=2;b.index=4;b.count=3;vs.bindings.push_back(b);
    Binding one=b;one.type.name="One";one.type.elements=1;one.index=10;one.count=5;vs.bindings.push_back(one);
    Binding wide=b;wide.index=20;wide.count=7;vs.bindings.push_back(wide);d.shaders.push_back(vs);
    Runtime r(d);float in[40],out[40];for(int i=0;i<40;++i)in[i]=float(i+1);
    auto lights=r.getParameterByName("Lights"),l0=r.getParameterElement(lights,0),l1=r.getParameterByName("Lights[1]");
    ParameterDesc desc;
    check(r.getParameterDesc(lights,desc) && desc.type.cls==5 && desc.type.type==0 && desc.type.rows==0 && desc.type.columns==0 &&
          desc.type.elements==2 && desc.type.members.size()==5 && desc.bytes==120,"Oracle: struct array descriptor");
    check(r.getParameterDesc(l0,desc) && desc.type.elements==0 && desc.bytes==60 && desc.type.name=="Lights","Oracle: struct element descriptor");
    check(r.getParameterDesc(r.getParameterMember(l0,0u),desc) && desc.type.name=="pos" && desc.type.cls==1 && desc.bytes==12 && desc.semantic.empty(),"Oracle: member descriptor");
    check(r.getParameterDesc(r.getParameterMember(l0,4u),desc) && desc.type.name=="attenuation" && desc.type.cls==0 && desc.bytes==4,"Oracle: last member descriptor");
    check(!r.getParameterMember(l0,5u) && !r.getParameterMember(lights,0u) && !r.getParameterMember(lights,"pos"),"Oracle: members need an element of a struct array");
    check(r.getParameterByName("Lights[0].pos") && r.getParameterMember(l0,"dir") && r.getParameterByName("One.pos"),"Oracle: member names");
    check(!r.getParameterByName("Lights.pos") && !r.getParameterByName("One[0]") && !r.getParameterByName("Lights[2]") &&
          !r.getParameterElement(r.getParameterByName("One"),0),"Oracle: invalid struct names");
    check(r.getParameterDesc(r.getParameterByName("Mix"),desc) && desc.bytes==20 && desc.type.members.size()==4,"Oracle: mixed struct descriptor");
    // Byte rules: at least the packed size, exactly that copied.
    check(r.setValue(lights,in,120) && r.getValue(lights,out,120),"Oracle: struct array SetValue/GetValue");
    for(int i=0;i<30;++i)check(out[i]==in[i],"Oracle: packed struct array round trip");
    check(r.setValue(lights,in+1,136) && r.getValue(lights,out,120) && out[0]==2 && out[29]==31,"Oracle: larger SetValue copies the parameter size");
    check(!r.setValue(lights,in+2,116) && r.getValue(lights,out,120) && out[0]==2,"Oracle: smaller SetValue fails unchanged");
    check(!r.getValue(lights,out,116) && r.getValue(lights,out,124),"Oracle: GetValue byte rule");
    check(r.setValue(l1,in+20,68) && r.getValue(lights,out,120) && out[14]==16 && out[15]==21 && out[29]==35,"Oracle: element SetValue with a 68-byte record");
    check(r.setValue(l0,in,60),"Oracle: element SetValue exact");
    auto pos=r.getParameterByName("Lights[0].pos");
    check(r.setValue(pos,in+30,16) && !r.setValue(pos,in+33,8),"Oracle: member SetValue byte rule");
    check(r.getValue(l0,out,60) && out[0]==31 && out[2]==33 && out[3]==4 && out[14]==15,"Oracle: member SetValue lands in the packed element");
    check(r.setVector(pos,{{101,102,103,104}}) && r.setFloat(r.getParameterByName("Lights[0].attenuation"),7.5f),"Oracle: member typed setters");
    float f[30]={};
    check(!r.setFloat(lights,9) && !r.setFloat(l0,9) && !r.getFloatArray(lights,f,30) && !r.setFloatArray(l0,in,15) && !r.setFloatArray(lights,in+10,4),
          "Oracle: typed setters refuse whole structs and struct elements");
    check(r.getValue(lights,out,120) && out[0]==101 && out[2]==103 && out[3]==4 && out[14]==7.5f && out[15]==21,"Oracle: state after member setters");
    std::array<float,4> v{};check(r.getVector(r.getParameterByName("Lights[1].color"),v) && v[0]==27 && v[3]==30,"Oracle: member GetVector");
    auto mix=r.getParameterByName("Mix");uint32_t w[5];int32_t n=0;float s=0;bool flag=false;
    check(r.setValue(mix,in,20) && r.getValue(mix,w,20),"Oracle: mixed struct SetValue");
    check(w[0]==0x3f800000u && w[1]==0x40000000u && w[2]==0x40400000u && w[3]==0x40800000u && w[4]==1,"Oracle: SetValue copies raw words and stores bool as 1");
    check(r.getInt(r.getParameterByName("Mix.n"),n) && n==1082130432 && r.getFloat(r.getParameterByName("Mix.s"),s) && s==3 &&
          r.getBool(r.getParameterByName("Mix.b"),flag) && flag,"Oracle: mixed member getters");
    check(r.setInt(r.getParameterByName("Mix.n"),5) && r.getInt(r.getParameterByName("Mix.n"),n) && n==5,"Oracle: int member setter");
    auto v3=r.getParameterByName("Vec3"),v4=r.getParameterByName("Vec4");
    check(r.setValue(v3,in,12) && r.setValue(v3,in+3,16) && !r.setValue(v3,in+6,8) && r.getValue(v3,out,12) && out[0]==4 && out[2]==6,"Oracle: vector SetValue byte rule");
    check(!r.getValue(v3,out,8) && r.getValue(v3,out,16) && !r.setValue(v4,in,0) && !r.setValue(v4,nullptr,0),"Oracle: vector GetValue and empty SetValue");
    // CTAB: one register per member per element; the register count trims trailing members.
    check(r.setValue(lights,in,120) && r.setValue(r.getParameterByName("One"),in+20,60),"Set struct constants");
    auto regs=r.mapConstants(d.shaders[0]);
    check(regs.f.at(16)==1 && regs.f.at(18)==3 && regs.f.at(20)==4 && regs.f.at(24)==7 && regs.f.at(27)==10,"Struct CTAB members start registers");
    check(regs.knownF.size()<=28 || !regs.knownF.at(28),"Partially used struct uploads only CTAB registers");
    check(regs.f.at(40)==21 && regs.f.at(56)==35,"Single struct: last member register");
    check(regs.f.at(80)==1 && regs.f.at(96)==15 && regs.f.at(100)==16 && regs.f.at(104)==19 && regs.f.at(106)==21,"Struct array: element 1 follows element 0's five registers");
    check(regs.knownF.at(107) && (regs.knownF.size()<=108 || !regs.knownF.at(108)),"Struct array: count trims element 1");
}
// D3DX9_25 matrix members in structs, from tools/macos/d3dxfx/oracle/run-matrix-oracle.sh.
static Type matrixMember(const char* name,uint32_t cls,uint32_t rows,uint32_t columns,uint32_t elements=0) {
    Type t;t.name=name;t.type=Float;t.cls=cls;t.rows=rows;t.columns=columns;t.elements=elements;return t;
}
static Parameter matrixStruct(const char* name,std::vector<Type> members) {
    Parameter p;p.value.type.name=name;p.value.type.cls=5;p.value.type.rows=1;p.value.type.members=members;size_t words=0;
    for(const auto& m:members)words+=size_t(m.rows)*4*std::max(1u,m.elements);
    p.value.type.columns=uint32_t(words);p.value.words.assign(words,0);return p;
}
static Binding ctab(const Parameter& p,std::vector<Type> members,uint32_t index,uint32_t count) {
    Binding b;b.type=p.value.type;b.type.elements=1;b.type.members=members;b.set=2;b.index=index;b.count=count;return b;
}
static void matrices() {
    // Effect descriptions: logical rows, class MATRIX_ROWS (both exporters); CTAB: the shader's class.
    Description d;
    d.parameters.push_back(matrixStruct("UserData",{matrixMember("uvMatrix",2,4,4,7)}));
    d.parameters.push_back(matrixStruct("Mixed",{matrixMember("a",2,3,3),matrixMember("b",2,4,3),matrixMember("v",1,1,2),matrixMember("r",2,4,4),matrixMember("c",2,3,4)}));
    d.parameters.push_back(matrixStruct("Static",{matrixMember("uvMatrix",2,4,4,7)}));
    Shader vs;vs.object=1;vs.stage=VertexShader;
    vs.bindings.push_back(ctab(d.parameters[0],{matrixMember("uvMatrix",3,4,4,7)},0,28));
    vs.bindings.push_back(ctab(d.parameters[1],{matrixMember("a",3,3,3,1),matrixMember("b",3,4,3,1),matrixMember("v",1,1,2,1),matrixMember("r",2,4,4,1),matrixMember("c",3,3,4,1)},28,14));
    vs.bindings.push_back(ctab(d.parameters[2],{matrixMember("uvMatrix",3,4,4,7)},42,11));
    d.shaders.push_back(vs);
    Runtime r(d);float in[512],out[512];for(int i=0;i<512;++i)in[i]=float(i+1);
    auto ud=r.getParameterByName("UserData"),uv=r.getParameterMember(ud,0u),uv2=r.getParameterElement(uv,2);
    ParameterDesc desc;
    check(r.getParameterDesc(uv,desc) && desc.type.cls==2 && desc.type.rows==4 && desc.type.columns==4 && desc.type.elements==7 && desc.bytes==448,"Matrix oracle: member descriptor");
    check(r.getParameterDesc(uv2,desc) && desc.type.elements==0 && desc.bytes==64,"Matrix oracle: member element descriptor");
    auto byName=r.getParameterByName("UserData.uvMatrix[2]");
    check(byName.member==uv2.member && byName.memberElement==2 && r.getParameterByName("UserData.uvMatrix").memberElement==UINT32_MAX,"Matrix oracle: member element names");
    check(!r.getParameterElement(uv,7) && !r.getParameterElement(uv2,0) && !r.getParameterByName("UserData.uvMatrix[7]"),"Matrix oracle: member element bounds");
    auto mixed=r.getParameterByName("Mixed");
    check(r.getParameterDesc(mixed,desc) && desc.bytes==204,"Matrix oracle: packed struct bytes");
    check(r.getParameterDesc(r.getParameterByName("Mixed.b"),desc) && desc.type.rows==4 && desc.type.columns==3 && desc.bytes==48,"Matrix oracle: float4x3 member");
    check(r.getParameterDesc(r.getParameterByName("Mixed.c"),desc) && desc.type.rows==3 && desc.type.columns==4 && desc.bytes==48,"Matrix oracle: float3x4 member");
    // Values: packed logical row-major per member; SetMatrix pads, SetMatrixTranspose transposes.
    check(r.setMatrix(uv2,in) && r.getValue(ud,out,448),"Matrix oracle: SetMatrix on member element");
    for(int i=0;i<16;++i)check(out[32+i]==float(i+1),"Matrix oracle: member element storage is row-major");
    check(r.setMatrix(uv2,in,true) && r.getValue(ud,out,448) && out[32]==1 && out[33]==5 && out[34]==9 && out[36]==2,"Matrix oracle: SetMatrixTranspose");
    check(r.setValue(ud,in,448) && r.setValue(ud,in,480) && !r.setValue(ud,in,440),"Matrix oracle: struct SetValue byte rule");
    float m16[16];check(r.getMatrix(uv2,m16) && m16[0]==33 && m16[15]==48 && r.getMatrix(uv2,m16,true) && m16[1]==37,"Matrix oracle: GetMatrix/GetMatrixTranspose");
    check(r.getMatrixArray(uv,out,7) && out[16]==17 && out[31]==32,"Matrix oracle: GetMatrixArray on an array member");
    check(r.setMatrixArray(uv,in+100,2) && r.getValue(ud,out,448) && out[0]==101 && out[31]==132 && out[32]==33,"Matrix oracle: SetMatrixArray on an array member");
    check(r.setValue(uv2,in+200,64) && !r.setValue(uv2,in,60) && r.setFloatArray(uv2,in+300,16) && r.getFloatArray(uv2,out,16) && out[0]==301,"Matrix oracle: element SetValue/SetFloatArray");
    check(r.setValue(mixed,in,400),"Matrix oracle: mixed struct SetValue");
    check(r.getMatrix(r.getParameterByName("Mixed.a"),m16) && m16[0]==1 && m16[2]==3 && m16[3]==0 && m16[4]==4 && m16[10]==9 && m16[12]==0,"Matrix oracle: float3x3 GetMatrix");
    check(r.getMatrix(r.getParameterByName("Mixed.b"),m16) && m16[0]==10 && m16[4]==13 && m16[12]==19 && m16[14]==21 && m16[15]==0,"Matrix oracle: float4x3 GetMatrix");
    check(!r.getMatrix(r.getParameterByName("Mixed.v"),m16),"Matrix oracle: GetMatrix refuses a vector");
    check(r.getMatrix(r.getParameterByName("Mixed.r"),m16) && m16[0]==24 && m16[15]==39 && r.getMatrix(r.getParameterByName("Mixed.c"),m16) && m16[0]==40 && m16[11]==51 && m16[12]==0,"Matrix oracle: row_major and float3x4 GetMatrix");
    // Constant table registers.
    check(r.setValue(ud,in,448) && r.setValue(r.getParameterByName("Static"),in,448),"Set matrix structs");
    auto regs=r.mapConstants(d.shaders[0]);
    auto reg=[&](size_t c,float x,float y,float z,float w) {return regs.f.at(c*4)==x && regs.f.at(c*4+1)==y && regs.f.at(c*4+2)==z && regs.f.at(c*4+3)==w;};
    check(reg(0,1,5,9,13) && reg(3,4,8,12,16) && reg(4,17,21,25,29) && reg(27,100,104,108,112),"Matrix oracle: column-major member array registers");
    check(reg(28,1,4,7,0) && reg(30,3,6,9,0) && reg(31,10,13,16,19) && reg(33,12,15,18,21),"Matrix oracle: float3x3/float4x3 column registers");
    check(reg(34,22,23,0,0) && reg(35,24,25,26,27) && reg(38,36,37,38,39),"Matrix oracle: vector and row_major registers");
    check(reg(39,40,44,48,0) && reg(41,42,46,50,0),"Matrix oracle: float3x4 columns trimmed by the struct count");
    check(reg(42,1,5,9,13) && reg(52,35,39,43,47) && regs.knownF.at(52*4) && (regs.knownF.size()<=53*4 || !regs.knownF.at(53*4)),"Matrix oracle: partially used array member");
    check(r.setMatrix(r.getParameterByName("Mixed.b"),in) && r.getValue(mixed,out,204) && out[9]==1 && out[12]==5 && out[20]==15 && out[21]==22,"Matrix oracle: SetMatrix on float4x3 packs rows of three");
}
static void corpus(const fs::path& cache) {
    bool swiff=false,ra=false,legacy=false;unsigned entries=0,passes=0,expressions=0,preshaders=0,structBindings=0,matrixBindings=0;
    for(const auto& directory:fs::directory_iterator(cache/"entries")) {
        auto key=directory.path().filename().string();auto request=unenvelope<Request>(readFile((cache/"requests"/(key+".request")).string()),"request");
        Entry entry;check(bool(load(cache.string(),request,entry)),"Load real effect");++entries;
        std::string source(request.source.begin(),request.source.end());
        swiff|=source.find("SwiffMenu.fx")!=std::string::npos;
        ra|=source.find("RaShader")!=std::string::npos;
        legacy|=source.find("Font.fx")!=std::string::npos;
        Runtime runtime(entry.description);Observer observer;
        if(runtime.getParameterByName("Columns")) {
            float matrix[16];ParameterDesc desc;
            auto columns=runtime.getParameterByName("Columns"),rows=runtime.getParameterByName("Rows");
            check(runtime.getParameterDesc(columns,desc) && desc.type.rows==2 && desc.type.columns==3 && desc.bytes==24,"D3DX compiler matrix descriptor oracle");
            check(runtime.getMatrix(columns,matrix) && matrix[0]==1 && matrix[2]==3 && matrix[4]==4 && matrix[6]==6,"D3DX compiler column-major default matrix oracle");
            check(runtime.getMatrix(rows,matrix) && matrix[0]==1 && matrix[2]==3 && matrix[4]==4 && matrix[6]==6,"D3DX compiler row-major default matrix oracle");
            auto* annotation=runtime.getAnnotation(runtime.getParameterByName("Tint"),"UIName");
            check(annotation && annotation->text=="Tint","Original compiler annotation round trip");
            auto bias=runtime.getParameterByName("Bias");check(runtime.setFloat(bias,0.5f),"Set preshader numerical input");
            for(const auto& shader:entry.description.shaders)if(!shader.preshader.instructions.empty()) {
                auto output=runtime.evaluate(shader.preshader);bool found=false;
                for(size_t i=0;i+2<output.size();++i)found|=output[i]==0.5f && output[i+1]==1.0f && output[i+2]==1.5f;
                check(found,"Embedded preshader numerical result matches Tint[0]*Bias");
            }
        }
        if(runtime.getParameterByName("Choice")) {
            auto h=runtime.getParameterByName("Choice");check(runtime.setInt(h,0),"Set first selector element");
            check(runtime.applyPass(0,0,observer),"Run selector pass");auto first=observer.shaderObjects[VertexShader];
            check(runtime.setInt(h,1) && runtime.applyPass(0,0,observer),"Update live selector element");
            check(observer.shaderObjects[VertexShader]!=first,"Selector preshader chooses a different shader");
        }
        for(const auto& p:entry.description.parameters) {
            auto h=runtime.getParameterByName(p.value.type.name);ParameterDesc desc;
            check(runtime.getParameterDesc(h,desc),"Real parameter descriptor");
        }
        auto map=runtime.getParameterByName("TexMap");auto texture=std::make_shared<int>(77);
        if(map)check(runtime.setTexture(map,texture),"Set real texture");
        for(uint32_t t=0;t<entry.description.techniques.size();++t)for(uint32_t p=0;p<entry.description.techniques[t].passes.size();++p) {
            check(runtime.applyPass(t,p,observer),"Real pass callback execution");++passes;
            for(const auto& s:entry.description.techniques[t].passes[p].states)if(!s.value.expression.empty()) {
                runtime.evaluate(s.value);++expressions;
                if(s.value.expressionType==0 && s.value.program.bindings.size()==1) {
                    auto h=runtime.getParameterByName(s.value.program.bindings[0].type.name);
                    auto* type=&s.value.program.bindings[0].type;
                    // Float inputs only: an int input (Ra's srcBlend) truncates 0.75 to 0 like D3DX.
                    if(type->rows==1 && type->columns==1 && type->type==Float) {
                        check(runtime.setFloat(h,0.0f),"Update real expression parameter");auto first=runtime.evaluate(s.value);
                        check(runtime.setFloat(h,0.75f),"Update real expression parameter again");auto second=runtime.evaluate(s.value);
                        check(first.words!=second.words,"Real expression observes live values");
                        if(type->name=="WaterCycleTime")check(bits<float>(second.words[0])==0.75f,"Ra WaterCycleTime numerical state result");
                    }
                }
            }
        }
        if(runtime.getParameterByName("WaterCycleTime")) {
            check(observer.samplerSlots.count(0) && observer.samplerSlots.count(1) && observer.samplerSlots.count(2) && observer.samplerSlots.count(3),"Ra sampler assignment indices preserved");
            check(observer.indexedStates.count({156,0}) && observer.indexedStates.count({156,3}),"Ra pixel constant assignment indices preserved");
            ParameterDesc desc;check(runtime.getParameterDesc(runtime.getParameterByName("Lights"),desc) && desc.bytes==60 && desc.type.rows==0 && desc.type.columns==0,"Ra struct descriptor matches D3DX compiler oracle");
        }
        if(runtime.getParameterByName("mOneBoneSkinning")) {
            auto h=runtime.getParameterByName("mOneBoneSkinning");ParameterDesc desc;
            check(runtime.getParameterDesc(h,desc) && desc.type.rows==4 && desc.type.columns==3 && desc.type.elements==26,"Real rectangular bone matrix array descriptor");
            float matrix[16];for(int i=0;i<16;++i)matrix[i]=float(i+1);
            check(runtime.setMatrix(runtime.getParameterElement(h,25),matrix),"Set last bone matrix array element");
            for(const auto& shader:entry.description.shaders)for(const auto& b:shader.bindings)if(b.type.name=="mOneBoneSkinning") {
                auto regs=runtime.mapConstants(shader);size_t base=(b.index+25*3)*4;
                check(regs.f.at(base)==1 && regs.f.at(base+1)==5 && regs.f.at(base+3)==13 && regs.f.at(base+4)==2,"Real rectangular bone array transposition and stride");
            }
        }
        if(auto ud=runtime.getParameterByName("UserData.uvMatrix")) {
            // RaShaderBM's UserData: SetValue(UserData, 7 matrices) then the CTAB columns.
            float m[112];for(int i=0;i<112;++i)m[i]=float(i+1);
            check(runtime.setValue(runtime.getParameterByName("UserData"),m,sizeof(m)),"Real UserData SetValue");
            float g[16];check(runtime.getMatrix(runtime.getParameterElement(ud,1),g) && g[0]==17 && g[15]==32,"Real UserData member element");
            for(const auto& shader:entry.description.shaders)if(!shader.selectorKind)for(const auto& b:shader.bindings)if(b.type.name=="UserData") {
                auto regs=runtime.mapConstants(shader);const size_t base=size_t(b.index)*4;
                check(regs.f.at(base)==1 && regs.f.at(base+1)==5 && regs.f.at(base+3)==13 && regs.f.at(base+4)==2,"Real UserData column registers");
                if(b.count>4)check(regs.f.at(base+16)==17,"Real UserData second matrix");
                ++matrixBindings;
            }
        }
        if(auto lights=runtime.getParameterByName("Lights")) {
            // RaShader's holder path: SetValue(Lights, RaLightRecord, 68) on Light Lights[1].
            ParameterDesc desc;float record[17];for(int i=0;i<17;++i)record[i]=float(i+1);
            check(runtime.getParameterDesc(lights,desc) && desc.type.cls==5 && desc.bytes==60*std::max(1u,desc.type.elements),"Real Lights descriptor");
            auto element=desc.type.elements?runtime.getParameterElement(lights,0):lights;
            check(runtime.setValue(element,record,68),"Real Lights 68-byte record");
            std::array<float,4> color{};check(runtime.getVector(runtime.getParameterMember(element,"color"),color) && color[0]==7 && color[3]==10,"Real Lights packed color member");
            for(const auto& shader:entry.description.shaders)if(!shader.selectorKind && !shader.msl.empty())for(const auto& b:shader.bindings)if(b.type.name=="Lights") {
                auto regs=runtime.mapConstants(shader);const size_t base=size_t(b.index)*4;
                check(regs.f.at(base)==1 && regs.f.at(base+2)==3,"Real Lights pos register");
                if(b.count>=2)check(regs.f.at(base+4)==4 && regs.f.at(base+6)==6,"Real Lights dir register");
                if(b.count>=3)check(regs.f.at(base+8)==7 && regs.f.at(base+11)==10,"Real Lights color register");
                if(b.count>=5)check(regs.f.at(base+16)==15,"Real Lights attenuation register");
                ++structBindings;
            }
        }
        if(map && observer.samplers)check(observer.texture==texture,"Real sampler resolves live native texture");
        if(runtime.getParameterByName("WorldView") && runtime.getParameterByName("DiffuseColor")) {
            float matrix[16];for(int i=0;i<16;++i)matrix[i]=float(i+1);
            check(runtime.setMatrix(runtime.getParameterByName("WorldView"),matrix),"Real matrix setter");
            check(runtime.setVector(runtime.getParameterByName("DiffuseColor"),{{0.1f,0.2f,0.3f,0.4f}}),"Real vector setter");
            for(const auto& shader:entry.description.shaders)if(!shader.selectorKind && !shader.msl.empty()) {
                auto regs=runtime.mapConstants(shader);
                if(!shader.preshader.instructions.empty())++preshaders;
                for(const auto& binding:shader.bindings)if(binding.type.name=="DiffuseColor")
                    check(std::abs(regs.f.at(binding.index*4)-0.1f)<1e-6f,"Real CTAB parameter-to-constant mapping");
            }
        }
        for(const auto& shader:entry.description.shaders)if(!shader.preshader.instructions.empty() && !runtime.getParameterByName("WorldView")) {
            if(shader.selectorKind==1)runtime.evaluate(shader.preshader);else runtime.mapConstants(shader);++preshaders;
        }
    }
    unsigned refusedRecords=0,reflections=0;
    if(fs::is_directory(cache/"refused"))for(const auto& file:fs::directory_iterator(cache/"refused")) {
        auto key=file.path().stem().string();auto request=unenvelope<Request>(readFile((cache/"requests"/(key+".request")).string()),"request");
        Entry entry;check(load(cache.string(),request,entry).error==Error::Refused && (entry.parseResult || entry.compileResult),"Real D3DX refusal record");++refusedRecords;
    }
    for(const auto& directory:fs::directory_iterator(cache/"entries")) {
        auto e=unenvelope<Entry>(readFile((directory.path()/"entry.bin").string()),"entry");if(e.kind!=ReflectionOnlyEntry)continue;
        Runtime runtime(e.description);auto h=runtime.getParameterByName("GenerateShaderPermutations");ParameterDesc desc;
        check(h && runtime.getParameterDesc(h,desc) && desc.type.type==String && desc.type.elements>0 && e.description.techniques.empty(),"Real .mfx reflection record");
        ++reflections;
    }
    std::cout<<"Records: "<<refusedRecords<<" refusals, "<<reflections<<" reflection-only\n";
    check(swiff && ra && legacy,"Real corpus contains SwiffMenu, Ra and legacy Font");
    check(expressions>0,"Real corpus expression coverage");
    std::cout<<"Real Lights CTAB bindings checked: "<<structBindings<<", UserData: "<<matrixBindings<<"\n";
    std::cout<<"Real corpus: "<<entries<<" entries, "<<passes<<" passes, "<<expressions<<" dynamic states, "<<preshaders<<" shader preshaders\n";
}
int main(int argc,char** argv) {
    try {
        if(argc<2)throw std::runtime_error("usage: macos_fx_runtime SCRATCH [REAL_CACHE]");fs::path scratch=argv[1];fs::create_directories(scratch);
        std::string pattern=(scratch/"run-XXXXXX").string();std::vector<char> path(pattern.begin(),pattern.end());path.push_back(0);
        if(!mkdtemp(path.data()))throw std::runtime_error("Cannot create fixture scratch directory");scratch=path.data();
        keyAndCache(scratch);parameters();expressions();structs();matrices();if(argc>2 && *argv[2])corpus(argv[2]);
        std::cout<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
