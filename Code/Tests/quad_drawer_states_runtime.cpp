// [TEMP] [CHOSEN, not recovered] x86 renderer state-hook fixture. Real Effect
// constructor, native metadata map/vector, annotation parser and state helpers
// execute. D3DX, StateManager and declaration allocation are recording providers
// only; actual GPU coverage is separate in effect_stock_d3d_runtime.cpp.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "../BF2/GeomMisc/MenuRend/IQuadDrawer.h"
#include "../RendDX9Abi/HfeAbi.h"
#define private public
#include "../BF2/RendDX9/Effect.cpp"
#undef private

namespace {
int checks=0,failures=0;
void check(bool value,const char* label)
{ ++checks;if(!value){++failures;std::fprintf(stderr,"FAIL: %s\n",label);} }
std::vector<std::string> events;
// A COM-shaped stand-in for ID3DXEffect: the vtable pointer first, then recorder state. The x86
// table entries are __stdcall functions taking the object first, as the SDK interface's methods are.
struct FakeD3dx
{
    void** table;
    unsigned int passes;
    void* handle;
};
ID3DXEffect* asEffect(FakeD3dx* fake) { return reinterpret_cast<ID3DXEffect*>(fake); }
D3DXHANDLE asHandle(void* handle) { return static_cast<D3DXHANDLE>(handle); }
// [TEMP] D3DX reflection outputs for recorded technique/annotation cases only.
struct TechniqueSeed
{
    const char* name;
    unsigned passes;
    unsigned annotations;
    long verdict;
    std::vector<int> declaration;
    bool hasRestore;
    int restore;
};
std::vector<TechniqueSeed*> seeds;
int settingsReads=0, declarationCreates=0, declarationReleases=0;
unsigned declarationCount=0;
void* declarationResult=reinterpret_cast<void*>(0x333);
std::vector<D3DVERTEXELEMENT9> recordedDeclaration;
void* __stdcall getTechnique(void*,unsigned i) { return i<seeds.size() ? seeds[i]:nullptr; }
void* __stdcall getTechniqueByName(void*,const char* name)
{
    events.push_back(std::string("find:")+name);
    for(auto seed : seeds) if(std::strcmp(name,seed->name)==0) return seed;
    return nullptr;
}
long __stdcall getTechniqueDesc(void*,void* handle,void* output)
{
    auto seed=static_cast<TechniqueSeed*>(handle);
    auto desc=static_cast<D3DXTECHNIQUE_DESC*>(output);
    desc->Name=seed->name;desc->Passes=seed->passes;desc->Annotations=seed->annotations;
    events.push_back(std::string("describe:")+seed->name);
    return -1; // The production caller must use outputs despite a failed HRESULT.
}
long __stdcall validate(void*,void* handle)
{
    auto seed=static_cast<TechniqueSeed*>(handle);
    events.push_back(std::string("validate:")+seed->name);
    return seed->verdict;
}
void* __stdcall annotation(void*,void* handle,const char* name)
{
    auto seed=static_cast<TechniqueSeed*>(handle);
    events.push_back(std::string("annotation:")+name);
    if(std::strcmp(name,"Restore")==0) return seed->hasRestore ? &seed->restore:nullptr;
    return seed->declaration.empty() ? nullptr:&seed->declaration;
}
long __stdcall parameterDesc(void*,void* handle,void* output)
{
    auto values=static_cast<std::vector<int>*>(handle);
    auto desc=static_cast<D3DXPARAMETER_DESC*>(output);
    desc->Elements=static_cast<unsigned>(values->size());
    return -1;
}
long __stdcall getInts(void*,void* handle,int* output,unsigned count)
{
    auto values=static_cast<std::vector<int>*>(handle);
    check(count==values->size(),"annotation int count forwarded unchanged");
    std::memcpy(output,values->data(),count*sizeof(int));
    declarationCount=count>>2;
    return -1;
}
long __stdcall getBool(void*,void* handle,int* output) { *output=*static_cast<int*>(handle);return -1; }
long __stdcall setTechnique(void* self,void* handle)
{
    static_cast<FakeD3dx*>(self)->handle=handle;
    bool isState=handle==reinterpret_cast<void*>(0x222);
    for(auto seed : seeds) if(handle==seed && std::strstr(seed->name,"_States")) isState=true;
    events.push_back(isState ? "state":"original");
    return -1; // the recovered hooks ignore every HRESULT
}
long __stdcall begin(void* self,unsigned* passes,unsigned flags)
{ check(flags==7,"state Begin flags");*passes=static_cast<FakeD3dx*>(self)->passes;events.push_back("begin");return -1; }
long __stdcall beginPass(void*,unsigned index)
{ events.push_back(index==0 ? "pass0":"pass1");return -1; }
long __stdcall endPass(void*) { events.push_back("endPass");return -1; }
long __stdcall end(void*) { events.push_back("end");return -1; }
struct DrawerRecorder : dice::hfe::geom::IQuadDrawer
{
    int refs=1;
    void addRef() override { ++refs; }
    int getRef() override { return refs; }
    int release() override { return --refs; }
    void* queryInterface(unsigned id) const override { return id==80031 ? const_cast<DrawerRecorder*>(this):nullptr; }
    void flush() override { events.push_back("flush"); }
    void beginFrame() override { events.push_back("beginFrame"); }
    void resetState() override { events.push_back("resetState"); }
    void endFrame() override { events.push_back("endFrame"); }
};
}
namespace dice { namespace hfe { namespace dx9rend {
// [TEMP] Only observe the exact push/pop callbacks required by these hooks.
void stateManagerPush() { events.push_back("push"); }
void stateManagerPop() { events.push_back("pop"); }
void* g_shaderManager=nullptr;
bool readBoolSetting(const char* name,bool fallback)
{ ++settingsReads;check(std::strcmp(name,"GSMaxValidation")==0 && !fallback,"validation setting name/default");return false; }
void* vertexDeclarationCreate(const void* elements)
{
    ++declarationCreates;
    auto data=static_cast<const D3DVERTEXELEMENT9*>(elements);
    recordedDeclaration.assign(data,data+declarationCount);
    events.push_back("createDeclaration");return declarationResult;
}
void vertexDeclarationRelease(void* wrapper)
{ ++declarationReleases;check(wrapper==reinterpret_cast<void*>(0x333),"owned declaration released"); }
void vertexDeclarationBind(void* wrapper)
{ check(wrapper==reinterpret_cast<void*>(0x333),"begin binds recovered declaration");events.push_back("bindDeclaration"); }
void shaderManagerSetCurrentEffect(void* effect) { events.push_back(effect ? "current":"clearCurrent"); }
} } }

int main()
{
    using namespace dice::hfe::dx9rend;
    void* table[0x110/4]{};
    table[0xE8/4]=reinterpret_cast<void*>(&setTechnique);
    table[0xFC/4]=reinterpret_cast<void*>(&begin);
    table[0x100/4]=reinterpret_cast<void*>(&beginPass);
    table[0x108/4]=reinterpret_cast<void*>(&endPass);
    table[0x10C/4]=reinterpret_cast<void*>(&end);
    FakeD3dx fake{table,0,nullptr};
    // Real constructor. Explicit process lifetime: Effect::release/destructor
    // remains an unrelated renderer frontier, and is not shadowed or invoked.
    Effect* effect=new Effect;
    effect->m_d3dxEffect=asEffect(&fake);
    EffectTechnique* recordObject=new EffectTechnique;
    EffectTechnique* state=new EffectTechnique;
    auto record=reinterpret_cast<unsigned char*>(recordObject);
    recordObject->handle=reinterpret_cast<void*>(0x111);
    state->handle=reinterpret_cast<void*>(0x222);
    effectBeginTechniqueStateRecord(nullptr,record);
    effectEndTechniqueStateRecord(nullptr,record);
    check(events.empty(),"null state link returns before touching Effect");
    recordObject->states=state;
    for(unsigned int passes : {0u,1u,2u,0x80000000u})
    {
        fake.passes=passes;events.clear();
        effectBeginTechniqueStateRecord(effect,record);
        std::vector<std::string> expected{"state","begin"};
        if(passes){expected.push_back("push");expected.push_back("pass0");expected.push_back("endPass");}
        expected.push_back("end");expected.push_back("original");
        check(events==expected && fake.handle==reinterpret_cast<void*>(0x111),
              "begin preserves pass threshold, state push order and technique restoration");
        events.clear();effectEndTechniqueStateRecord(effect,record);
        expected={"state","begin"};
        if(passes>1){expected.push_back("pass1");expected.push_back("endPass");expected.push_back("pop");}
        expected.push_back("end");expected.push_back("original");
        check(events==expected && fake.handle==reinterpret_cast<void*>(0x111),
              "end preserves unsigned pass threshold, state pop order and technique restoration");
    }
    events.clear();
    effectBeginTechniqueStates(effect,std::string("Menu"));
    effectEndTechniqueStates(effect,std::string("Menu"));
    check(events.empty(),"named helpers miss in member Effect technique map");
    EffectTechnique& memberRecord=effect->techniqueMap()["Menu"];
    memberRecord.handle=reinterpret_cast<void*>(0x111);
    memberRecord.states=state;
    fake.passes=2;events.clear();
    effectBeginTechniqueStates(effect,std::string("Menu"));
    effectEndTechniqueStates(effect,std::string("Menu"));
    check(events==std::vector<std::string>{"state","begin","push","pass0","endPass","end","original",
          "state","begin","pass1","endPass","pop","end","original"},
          "named helper hit uses member technique record and both state passes");
    events.clear();effectBeginTechniqueStates(effect,std::string("menu"));
    check(events.empty(),"technique lookup remains case-sensitive");
    DrawerRecorder recorder;dice::hfe::geom::IQuadDrawer* drawer=&recorder;
    check(drawer->getRef()==1 && drawer->queryInterface(80031)==drawer,"IQuadDrawer IBase prefix");
    drawer->addRef();check(drawer->release()==1,"IQuadDrawer reference quartet");
    drawer->beginFrame();drawer->resetState();drawer->flush();drawer->endFrame();
    check(events==std::vector<std::string>{"beginFrame","resetState","flush","endFrame"},
          "all four IQuadDrawer suffix slots dispatch in client order");

    // All previous 25 state/table checks above are retained. Reflection below
    // now supplies the production-created records used by the same hooks.
    table[0x10/4]=reinterpret_cast<void*>(&parameterDesc);
    table[0x14/4]=reinterpret_cast<void*>(&getTechniqueDesc);
    table[0x30/4]=reinterpret_cast<void*>(&getTechnique);
    table[0x34/4]=reinterpret_cast<void*>(&getTechniqueByName);
    table[0x4C/4]=reinterpret_cast<void*>(&annotation);
    table[0x5C/4]=reinterpret_cast<void*>(&getBool);
    table[0x74/4]=reinterpret_cast<void*>(&getInts);
    table[0xF0/4]=reinterpret_cast<void*>(&validate);
    Effect* reflected=new Effect;reflected->m_d3dxEffect=asEffect(&fake);
    TechniqueSeed menu{"Menu",5,23,0,{},false,0};
    TechniqueSeed states{"Menu_States",2,91,0,{},true,7};
    TechniqueSeed quad{"QuadWithTexture",1,47,0,
        {0,2,0,0, 0,3,10,0, 0,1,5,0, 0,1,5,1, 255,17,0,0},false,0};
    seeds={&states,&quad,&menu};events.clear();
    check(reflected->validateTechniques()==2,"reflection enumeration and states postpass complete");
    auto menuRecord=static_cast<EffectTechnique*>(effectFindTechniqueRecord(reflected,"Menu"));
    auto quadRecord=static_cast<EffectTechnique*>(effectFindTechniqueRecord(reflected,"QuadWithTexture"));
    check(menuRecord && menuRecord->name=="Menu" && menuRecord->handle==&menu && menuRecord->validated,
          "native record retains actual name, handle and validated flag");
    check(menuRecord->passes==5 && quadRecord->passes==1,"pass count is descriptor word1, not annotations word2");
    check(menuRecord->states && menuRecord->states->name=="Menu_States" && menuRecord->states->handle==&states,
          "queued state technique links after base enumeration");
    check(menuRecord->states->restore==7 && menuRecord->states->validated && menuRecord->states->passes==0 &&
          menuRecord->states->declaration==nullptr,"state Restore is int-width; pass and declaration keep constructor defaults");
    check(recordedDeclaration.size()==5 && recordedDeclaration[0].Offset==0 &&
          recordedDeclaration[1].Offset==12 && recordedDeclaration[2].Offset==28 && recordedDeclaration[3].Offset==36 &&
          recordedDeclaration[4].Stream==255 && recordedDeclaration[4].Offset==0,
          "stock quad declaration offsets and terminator");
    check(quadRecord->declaration==reinterpret_cast<void*>(0x333) && declarationCreates==1,
          "reflection stores declaration wrapper despite failed annotation HRESULTs");
    auto descPosition=std::find(events.begin(),events.end(),"describe:QuadWithTexture");
    auto validatePosition=std::find(events.begin(),events.end(),"validate:QuadWithTexture");
    auto createPosition=std::find(events.begin(),events.end(),"createDeclaration");
    check(descPosition<validatePosition && validatePosition<createPosition,"descriptor then validation then declaration creation");
    events.clear();effectSetTechniqueByName(reflected,std::string("QuadWithTexture"));
    check(reflected->m_currentTechnique==quadRecord && std::strcmp(reflected->m_string38.c_str(),"QuadWithTexture")==0,
          "name selection uses native record and copies its real name");
    fake.passes=1;events.clear();check(effectBegin(reflected)==1,"begin returns D3DX pass count");
    check(events==std::vector<std::string>{"begin","bindDeclaration","current"},"real begin binds produced declaration before publishing current effect");
    events.clear();effectSetTechniqueRecord(reflected,menuRecord);
    check(reflected->m_currentTechnique==menuRecord && fake.handle==&menu,"opaque record round-trip selects original handle");
    fake.passes=2;events.clear();
    effectBeginTechniqueStates(reflected,std::string("Menu"));
    effectEndTechniqueStates(reflected,std::string("Menu"));
    check(events==std::vector<std::string>{"state","begin","push","pass0","endPass","end","original",
          "state","begin","pass1","endPass","pop","end","original"},
          "production reflection state link reaches both actual frame helpers");

    TechniqueSeed rejected{"Rejected",8,2,1,{},false,0};
    seeds.push_back(&rejected);events.clear();
    check(reflected->validateTechnique("Rejected")==1,"positive nonzero validation HRESULT is failure");
    auto rejectedRecord=static_cast<EffectTechnique*>(effectFindTechniqueRecord(reflected,"Rejected"));
    check(rejectedRecord->validated && rejectedRecord->name=="Rejected" && !rejectedRecord->handle && rejectedRecord->passes==0,
          "failed first validation retains validated=true but constructor payload");
    rejected.verdict=-1;check(reflected->validateTechnique("Rejected")==1,"negative validation HRESULT also fails");
    rejected.verdict=0;check(reflected->validateTechnique("Rejected")==2,"subsequent exact-zero validation succeeds");
    rejected.verdict=1;check(reflected->validateTechnique("Rejected")==1 && rejectedRecord->handle==&rejected && rejectedRecord->passes==8,
          "revalidation failure retains old handle/pass payload");
    check(reflected->validateTechnique("Missing")==0,"missing technique has non-retry verdict0");
    auto missing=static_cast<EffectTechnique*>(effectFindTechniqueRecord(reflected,"Missing"));
    check(missing && missing->validated && missing->name=="Missing","missing technique still creates named validated record");
    TechniqueSeed doubled{"A_States_B_States",1,0,0,{},false,0};seeds.push_back(&doubled);
    const auto queued=reflected->m_stringVec.size();reflected->validateTechnique(doubled.name);
    check(reflected->m_stringVec.size()==queued,"only the first substring match can qualify as the suffix");
    reflected->validateTechnique("Menu_States");
    check(reflected->m_stringVec.size()==queued+1,"repeated states validation appends without deduplication");
    auto linked=menuRecord->states;states.hasRestore=false;
    check(reflected->linkStateTechnique("Menu_States")==2 && menuRecord->states==linked && linked->restore==0,
          "state relink reuses owned record and missing Restore defaults to zero");
    const auto mapSize=reflected->techniqueMap().size();
    check(reflected->linkStateTechnique("Absent_States")==0 && reflected->techniqueMap().size()==mapSize,
          "missing state technique returns before touching base cache");
    TechniqueSeed orphan{"Orphan_States",2,0,0,{},false,0};seeds.push_back(&orphan);events.clear();
    check(reflected->linkStateTechnique(orphan.name)==2,"missing base is validated on demand even when lookup fails");
    auto orphanBase=static_cast<EffectTechnique*>(effectFindTechniqueRecord(reflected,"Orphan"));
    check(orphanBase && orphanBase->validated && !orphanBase->handle && orphanBase->states->handle==&orphan,
          "state link follows failed missing-base validation without invented guard");
    check(settingsReads==1,"GSMaxValidation is sampled only once across Effects and retries");

    // Real bf2/xpack QuadCache annotation differences from the retained shader.
    quad.declaration={0,3,0,0, 0,3,10,0, 0,1,5,0, 0,1,5,1, 255,17,0,0};
    void* cacheOutput=nullptr;reflected->readDeclaration(asHandle(&quad),&cacheOutput);
    check(recordedDeclaration[2].Offset==32 && recordedDeclaration[3].Offset==40,"bf2 QuadCache FLOAT4 color layout");
    quad.declaration[5]=4;reflected->readDeclaration(asHandle(&quad),&cacheOutput);
    check(recordedDeclaration[2].Offset==20 && recordedDeclaration[3].Offset==28,"xpack QuadCache packed D3DCOLOR layout");
    // Adversarial finite annotations verify stream comparison before truncation.
    quad.declaration={0,3,0,0, 0,1,5,0, 256,0x101,0x105,0x102, 256,2,5,0,
                      1,4,10,0, 1,17,0,0, 255,17,0,0};
    void* output=reinterpret_cast<void*>(0x444);const int releasesBefore=declarationReleases;
    check(reflected->readDeclaration(asHandle(&quad),&output),"valid annotation returns true");
    check(recordedDeclaration[2].Stream==0 && recordedDeclaration[2].Offset==0 &&
          recordedDeclaration[2].Type==1 && recordedDeclaration[2].Usage==5 && recordedDeclaration[2].UsageIndex==2,
          "full stream change resets offset even when truncated stream is equal");
    check(recordedDeclaration[3].Offset==8 && recordedDeclaration[4].Offset==0 && recordedDeclaration[5].Offset==4,
          "offset accumulation follows type sizes and actual stream transitions");
    check(declarationReleases==releasesBefore,"annotation reader overwrites output without releasing previous wrapper");
    declarationResult=nullptr;check(reflected->readDeclaration(asHandle(&quad),&output) && !output,"null wrapper creation still reports annotation success");
    quad.declaration.clear();output=reinterpret_cast<void*>(0x444);
    check(!reflected->readDeclaration(asHandle(&quad),&output) && !output,"missing Declaration clears output then returns false");
    const int widths[]={4,8,12,16,4,4,4,8,4,4,8,4,8,4,4,4,8,0};
    for(unsigned i=0;i<18;++i) check(vertexDeclarationTypeSize(i)==widths[i],"complete declaration type-size table");
    check(vertexDeclarationTypeSize(18)==0 && vertexDeclarationTypeSize(0xffffffff)==0,"unknown declaration types return zero after diagnostic");
    // Release the independent temporary record tree through the REAL record dtor.
    auto lifetime=new EffectTechnique;lifetime->declaration=reinterpret_cast<void*>(0x333);
    lifetime->states=new EffectTechnique;lifetime->states->declaration=reinterpret_cast<void*>(0x333);
    const int oldReleases=declarationReleases;delete lifetime;
    check(declarationReleases==oldReleases+2,"record destructor releases its declaration and recursively owns state record");
    std::printf("quad_drawer_states_runtime: %d checks, %d failures\n",checks,failures);
    return failures ? 1:0;
}
