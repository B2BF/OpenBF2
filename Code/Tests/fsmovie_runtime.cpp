// [TEMP] [CHOSEN, not recovered] Actual FSMoviePlayer load/draw/destruction with
// recording Video, shader, buffer and renderer collaborators. ShaderParameter's
// unchanged methods are extracted mechanically from Effect.cpp by the runner.
// This checks ownership/dispatch/order, not Bink decoding, GPU output or startup.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
#include "../BF2/GeomMisc/MenuRend/FSMoviePlayer.cpp"
#include "../BF2/Game/Main/IFSMoviePlayer.h"
static_assert(sizeof(void*)==4,"native Win32 movie boundary fixture");
using namespace dice::hfe;
using namespace dice::hfe::dx9rend;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* label) {
    ++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",label);std::fflush(stdout);
}
[[noreturn]] void cold() { std::fprintf(stderr,"Unexpected movie fixture collaborator\n");std::abort(); }
std::vector<std::string> calls;
void record(const char* name) { calls.push_back(name); }
void sequence(std::initializer_list<const char*> expected,const char* label) {
    bool same=calls.size()==expected.size();unsigned i=0;
    for(const char* name:expected) { if(i>=calls.size() || calls[i]!=name)same=false;++i; }
    check(same,label);
    if(!same) { std::printf("observed:");for(const auto& c:calls)std::printf(" %s",c.c_str());std::puts(""); }
}
enum Failure { None,NoEffect,NoTransform,NoTexture,OpenFailure,LockFailure } failure=None;
bool sceneReady=true,unlocks=true;
int movieDone=0,pool=7,effectRefs=0,textureBinds=0,transformWrites=0;
std::string expectedPath,receivedPath;
unsigned char vertexBytes[96];
int effectToken,declToken,bufferToken,textureToken,shaderToken;
Effect* effect() { return reinterpret_cast<Effect*>(&effectToken); }
struct Owner : ParameterOwner {
    const char* name;int refs=0;
    explicit Owner(const char* n):name(n) {}
    int getRef() const override { return refs; }
    int addRef() override { return ++refs; }
    int release() override { record(name);check(refs==1,"parameter owner released exactly once");return --refs; }
    void* getD3DXParam() const override { return const_cast<Owner*>(this); }
    void setD3DXParam(void*) override { cold(); }
    bool isParameterUsed() const override { cold(); }
    void setValue(const void* data,unsigned int count) override {
        record("identity");++transformWrites;
        const float* m=static_cast<const float*>(data);bool identity=count==0;
        for(unsigned i=0;i<16;++i)identity &= m[i]==(i%5==0?1.0f:0.0f);
        check(identity,"real parameter forwards identity matrix with count zero");
    }
    void setBool(unsigned char) override { cold(); }
    void setInt(int) override { cold(); }
    void setFloat(float) override { cold(); }
    void setMatrixPointerArray(const void* const*,unsigned int) override { cold(); }
    unsigned int getTypeCode() const override { cold(); }
} transformOwner("release_transform"),textureOwner("release_texture");
struct Resource : IBase {
    int refs=0;bool video=false,ownsTarget=false;
    void addRef() override { ++refs; }
    int getRef() override { return refs; }
    int release() override;
    void* queryInterface(unsigned int) const override { cold(); }
} video,target;
int Resource::release() {
    record(video?"release_video":"release_target");
    check(refs>0,"movie resource reference is live when released");
    --refs;
    if(video && !refs && ownsTarget) { ownsTarget=false;target.release(); }
    return refs;
}
// Only the observed ClassManager by-name call is reachable. All other slots abort.
// MSVC groups/reverses the createInstance overloads; actual load assembly emitted
// by this runner verifies the native string overload uses +0x2C.
void* classSlots[32];
struct ClassFactory { void** table; } classFactory={classSlots};
IBase* __fastcall createVideo(void* self,void*,std::string name,unsigned int iid,IBase* outer) {
    record("create_video");
    check(self==&classFactory && name=="dice.hfe.geom.Video" && iid==0xb801dd04u && !outer,
          "native factory receives exact class name, IID and null outer");
    video.refs=1;return &video;
}
// Narrow COM-shaped ID3DXEffect collaborator: its sole reachable method has the
// real stdcall signature. The rest abort instead of permitting arbitrary calls.
void* fxSlots[80];struct Fx { void** table; } fx={fxSlots};
long __stdcall setTexture(void* self,void* handle,void* texture) {
    record(texture?"bind_texture":"unbind_texture");++textureBinds;
    check(self==&fx && handle==&textureOwner && (!texture || texture==&textureToken),
          "D3DX receives parameter handle and the target's actual texture");
    return static_cast<long>(0x80004005u); // Retail ignores this HRESULT.
}
void reset(Failure f=None) {
    check(transformOwner.refs==0 && textureOwner.refs==0 && video.refs==0 && target.refs==0 && effectRefs==0,
          "previous case released all resources");
    failure=f;sceneReady=true;unlocks=true;movieDone=0;pool=7;textureBinds=transformWrites=0;
    receivedPath.clear();std::memset(vertexBytes,0xcd,sizeof(vertexBytes));calls.clear();
}
IFSMoviePlayer* player() {
    return reinterpret_cast<IFSMoviePlayer*>(geom::createFSMoviePlayer(80047u,0));
}
void releaseFailure(Failure f,std::initializer_list<const char*> expected) {
    reset(f);IFSMoviePlayer* p=player();
    check(!p->load(expectedPath),"selected missing resource/open/lock refuses load");
    check(transformOwner.refs==(f>=NoTexture?1:0) && textureOwner.refs==(f>=OpenFailure?1:0),
          "load refusal retains already acquired parameter references");
    if(f==OpenFailure) {
        sequence({"effect","transform","texture","create_video","init_sound","open","shutdown_sound"},
                 "failed open invokes sound shutdown without frame or geometry creation");
    }
    if(f==LockFailure)check(pool==7,"buffer lock failure still restores the previous pool");
    calls.clear();p->release();sequence(expected,"partial destruction releases only acquired resources in order");
}
}
namespace dice { namespace hfe {
IClassManager* classManager=reinterpret_cast<IClassManager*>(&classFactory);
namespace geom {
extern const unsigned int IID_IVideo=0xb801dd04u;
void videoInitSound(void* p) { check(p==&video,"sound initialization receives Video");record("init_sound"); }
void videoShutdownSound(void* p) { check(p==&video,"failed-open sound shutdown receives Video");record("shutdown_sound"); }
bool videoOpen(void* p,const char* path) {
    check(p==&video,"open receives Video");record("open");receivedPath=path;
    if(failure==OpenFailure)return false;
    target.refs=1;video.ownsTarget=true;return true;
}
void* videoAcquireFrame(void* p) {
    check(p==&video && video.ownsTarget,"frame acquisition follows successful open");
    record("acquire_frame");target.addRef();return &target;
}
void* videoTargetD3DTexture(void* p) { check(p==&target,"texture query receives acquired target");return &textureToken; }
void videoDoFrame(void* p) { check(p==&video,"decode receives Video");record("decode"); }
int videoIsDone(void* p) { check(p==&video,"completion query receives Video");return movieDone; }
}
namespace dx9rend {
void* g_shaderManager=&shaderToken;
void* shaderManagerGetEffect(void* manager,const char* path) {
    record("effect");check(manager==&shaderToken && std::strcmp(path,"mods/xpack/Shaders/LoadingScreen")==0,
                           "stock LoadingScreen effect is requested");
    if(failure==NoEffect)return 0;++effectRefs;return effect();
}
ShaderParameter* effectGetShaderParameterTyped(Effect* e,const vc71::string& name) {
    check(e==effect(),"parameter lookup uses acquired effect");
    if(std::strcmp(name.c_str(),"TRANSFORM")==0) {
        record("transform");return failure==NoTransform?0:new ShaderParameter(&transformOwner);
    }
    if(std::strcmp(name.c_str(),"TEXTURE")==0) {
        record("texture");return failure==NoTexture?0:new ShaderParameter(&textureOwner);
    }
    cold();
}
int effectRelease(Effect* e) { check(e==effect() && effectRefs==1,"effect reference released exactly once");record("release_effect");return --effectRefs; }
void* effectD3dxHandle(void* e) { check(e==effect(),"D3DX lookup uses acquired effect");return &fx; }
void* vertexDeclarationCreate(const void* p) {
    record("declaration");
    const unsigned char golden[]={0,0,0,0,2,0,0,0,0,0,12,0,4,0,10,0,0,0,16,0,1,0,5,0,255,0,0,0,17,0,0,0};
    check(std::memcmp(p,golden,sizeof(golden))==0,"movie declaration matches position/color/UV wire bytes");return &declToken;
}
void vertexDeclarationRelease(void* p) { check(p==&declToken,"release exact declaration");record("release_declaration"); }
int bufferManagerSwapPool(int next) { record(next==1?"pool_managed":"pool_restore");int previous=pool;pool=next;return previous; }
void* bufferManagerCreateVertexBuffer(unsigned int count,unsigned int stride,unsigned int usage,const char* name) {
    record("buffer");check(pool==1 && count==4 && stride==24 && usage==8 && std::strcmp(name,"VB")==0,
                           "quad buffer has four 24-byte vertices, usage 8 and managed pool");return &bufferToken;
}
void* vertexBufferLock(void* p,unsigned int offset,unsigned int count,unsigned int flags) {
    record("lock");check(p==&bufferToken && offset==0 && count==0 && flags==0 && pool==7,
                         "whole buffer locks after restoring original pool");return failure==LockFailure?0:vertexBytes;
}
bool vertexBufferUnlock(void* p) { check(p==&bufferToken,"unlock exact buffer");record("unlock");return unlocks; }
void vertexBufferRelease(void* p) { check(p==&bufferToken,"release exact buffer");record("release_buffer"); }
void vertexBufferBindStream(void* p,unsigned int stream) { check(p==&bufferToken && stream==0,"quad binds stream zero");record("stream"); }
void vertexDeclarationBind(void* p) { check(p==&declToken,"bind exact declaration");record("bind_declaration"); }
bool rendererBeginScene(const unsigned int* rgb,bool clear) { check(clear && rgb[0]==255 && rgb[1]==0 && rgb[2]==0,"movie begins with recovered red clear");record("begin");return sceneReady; }
bool rendererEndScene() { record("end");return false; }
bool rendererPresent(unsigned int mode) { check(mode==2,"movie presents with mode two");record("present");return false; }
void rendererDrawPrimitive(unsigned int count,unsigned int type,unsigned int start) { check(count==2 && type==6 && start==0,"draw is two triangle-fan primitives from vertex zero");record("draw"); }
void stateManagerPush() { record("push"); }
void stateManagerPop() { record("pop"); }
unsigned int effectBegin(void* p) { check(p==effect(),"begin exact effect");record("effect_begin");return 0; }
void effectBeginPass(void* p,unsigned int pass) { check(p==effect() && pass==0,"draw always selects effect pass zero");record("pass"); }
void effectEnd(void* p) { check(p==effect(),"end exact effect");record("effect_end"); }
}
} }
int main() {
    for(void*& slot:classSlots)slot=reinterpret_cast<void*>(&cold);
    classSlots[0x2c/4]=reinterpret_cast<void*>(&createVideo);
    for(void*& slot:fxSlots)slot=reinterpret_cast<void*>(&cold);
    fxSlots[0xd0/4]=reinterpret_cast<void*>(&setTexture);
    video.video=true;
    expectedPath="Movies/"+std::string(80,'a')+".bik";
    check(geom::createFSMoviePlayer(80047u,reinterpret_cast<IBase*>(1))==0,"factory refuses aggregation");
    check(geom::createFSMoviePlayer(123u,0)==0,"factory refuses unsupported IID");
    releaseFailure(NoEffect,{});
    releaseFailure(NoTransform,{"release_effect"});
    releaseFailure(NoTexture,{"release_transform","release_effect"});
    releaseFailure(OpenFailure,{"release_transform","release_texture","release_effect","release_video"});
    releaseFailure(LockFailure,{"release_transform","release_texture","release_effect","release_target","release_video","release_target","release_declaration","release_buffer"});
    reset();IFSMoviePlayer* p=player();unlocks=false;
    const std::string callerPath=expectedPath;
    check(p->load(callerPath),"native virtual by-value string load succeeds even when unlock returns false");
    check(callerPath==expectedPath && receivedPath==expectedPath,"long native path crosses the boundary without modifying its caller");
    sequence({"effect","transform","texture","create_video","init_sound","open","acquire_frame","declaration","pool_managed","buffer","pool_restore","lock","unlock"},"successful load acquires resources in recovered order");
    struct Vertex { float x,y,z;unsigned colour;float u,v; };
    const Vertex expected[]={ {-1,-1,0,0xffffffffu,0,1},{1,-1,0,0xffffffffu,1,1},
                              {1,1,0,0xffffffffu,1,0},{-1,1,0,0xffffffffu,0,0} };
    check(sizeof(expected)==sizeof(vertexBytes) && std::memcmp(vertexBytes,expected,sizeof(expected))==0,
          "quad uses full clip-space extent, white colour and exact unscaled 0-1 UVs");
    check(target.refs==2 && video.refs==1,"movie holds its own target reference alongside Video");
    check(!p->finished(),"zero video completion result means unfinished");movieDone=7;
    check(p->finished(),"nonzero video completion result means finished");
    calls.clear();sceneReady=false;p->drawFrame();
    sequence({"begin","present"},"failed scene begin still presents without decoding or drawing");
    check(textureBinds==0 && transformWrites==0,"failed begin does not access frame or shader state");
    calls.clear();sceneReady=true;p->drawFrame();
    sequence({"begin","push","identity","decode","bind_texture","stream","bind_declaration","effect_begin","pass","draw","effect_end","pop","unbind_texture","end","present"},
             "actual draw decodes, binds, draws and clears texture before end/present");
    check(textureBinds==2 && transformWrites==1,"one frame binds and unbinds exactly once");
    calls.clear();p->release();
    sequence({"release_transform","release_texture","release_effect","release_target","release_video","release_target","release_declaration","release_buffer"},
             "terminal release drops handles then target, Video and quad resources");
    reset();std::printf("FSMoviePlayer: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
