// [TEMP] Synthetic byte stream and texture cache for the real state manager.
// No game, renderer device, /FORCE, or production singleton registration.
#include "../BF2/Geom/BundledMeshStateManager.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>
#include <map>
#include <vector>

using namespace dice::hfe;
using namespace dice::hfe::geom;
namespace {
int checks=0,obsoleteLogs=0,destroyed=0,virtualRetains=0;
#define CHECK(c) do{++checks;if(!(c)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
std::map<std::string,dx9rend::EngineTexture*> textures;
std::vector<std::string> loads;
unsigned char rendererBytes[0x800]={};
void __fastcall retainTexture(dx9rend::EngineTexture* t,void*){++virtualRetains;++t->refCount;}
void* textureTable[3]={0,0,reinterpret_cast<void*>(retainTexture)};
struct Bytes : renddx9abi::HostNonlinearStream
{
    std::vector<unsigned char> data;unsigned int cursor=0;
    void number(unsigned int n){const unsigned char* p=reinterpret_cast<const unsigned char*>(&n);data.insert(data.end(),p,p+4);}
    void string(const char* s){number(static_cast<unsigned int>(std::strlen(s)));data.insert(data.end(),s,s+std::strlen(s));}
    Bytes(const char* material,std::initializer_list<const char*> maps)
    {string(material);number(static_cast<unsigned int>(maps.size()));for(auto s:maps)string(s);}
    void addRef()override{CHECK(false);}int getRef()override{CHECK(false);return 0;}
    int release()override{CHECK(false);return 0;}void* queryInterface(unsigned int)const override{CHECK(false);return 0;}
    unsigned int read(void* out,unsigned int n)override
    {CHECK(cursor+n<=data.size());if(n)std::memcpy(out,data.data()+cursor,n);cursor+=n;return n;}
    unsigned int write(const void*,unsigned int)override{CHECK(false);return 0;}
    int skip(unsigned int)override{CHECK(false);return 0;}
    int getAvailableBytes()override{CHECK(false);return 0;}
    io::IStream* clone()override{CHECK(false);return 0;}
    bool canRead()override{return true;}bool canWrite()override{return false;}
    int seek(io::SeekOrigin,int)override{CHECK(false);return 0;}
    int getPosition()override{return cursor;}int getSize()override{return static_cast<int>(data.size());}
};
BundledMeshState* read(Bytes& bytes,bool skip=false,bool arg4=false)
{
    BundledMeshState* result=g_bundledMeshStateManager->read(reinterpret_cast<void*>(0xBAD),std::string("Weapons/Rifle/"),&bytes,arg4,skip);
    CHECK(bytes.cursor==bytes.data.size());return result;
}
template<class T> unsigned int count(const std::vector<T>& v){return static_cast<unsigned int>(v.size());}
dx9rend::EngineTexture* tex(BundledMeshState* s,unsigned int i){return s->m_textures[i];}
}
namespace renddx9abi {
void* engineAlloc(unsigned int size){return std::malloc(size);}void engineFree(void* p){std::free(p);}
void logf(const char* format,...){CHECK(std::strstr(format,"obsolete")!=0);++obsoleteLogs;}
}
namespace dice { namespace hfe { namespace dx9rend {
IBase* g_Renderer=reinterpret_cast<IBase*>(rendererBytes);
void pathToLower(std::string& value)
{
    std::string lower(value.c_str());for(char& c:lower)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    value=lower.c_str();
}
void* textureManagerGetTexture(const std::string& name)
{
    const std::string key(name.c_str());loads.push_back(key);
    if(key.find("missing")!=std::string::npos)return 0;
    if(key.find("transient")!=std::string::npos)
    {EngineTexture* t=new EngineTexture();t->vptr=textureTable;t->refCount=1;return t;}
    EngineTexture*& t=textures[key];
    if(!t){t=new EngineTexture();t->vptr=textureTable;t->refCount=1;}
    ++t->refCount;return t; // one cache reference plus the returned owning reference
}
void textureDtor(EngineTexture* texture){++destroyed;texture->~EngineTexture();}
} } }
int main()
{
    CHECK(!g_bundledMeshStateManager);bundledMeshStateManagerRetain();
    BundledMeshStateManager* manager=g_bundledMeshStateManager;
    CHECK(manager && manager->getRef()==1 && manager->getEnabled() && manager->m_enabled9);
    CHECK(manager->queryInterface(1)==manager && !manager->queryInterface(2));
    CHECK(std::strcmp(manager->m_texturePrefix.c_str(),"Textures/")==0);
    bundledMeshStateManagerRetain();CHECK(manager->getRef()==2);
    bundledMeshStateManagerRelease();CHECK(manager->getRef()==1);
    manager->setEnabled(false);CHECK(!manager->getEnabled());manager->setEnabled(true);

    Bytes all("ANIMATED_EnvMap_ColorMapGloss_alpha_test_alpha_one_Cockpit_NoHemiLight",
              {"rifle_c.dds","rifle_b.dds","effects/e.dds","SpecularLUT_pow36"});
    BundledMeshState* first=read(all);
    CHECK(first->getRef()==1 && first->queryInterface(1)==first && !first->queryInterface(99));
    CHECK(first->m_animated && first->m_envMap && first->m_colorMapGloss && first->m_alphaTest);
    CHECK(first->m_alphaOne && first->m_cockpit && first->m_noHemiLight);
    CHECK(first->m_normalMap && first->m_extraMap && !first->m_feature39);
    CHECK(count(first->m_textures)==4 && count(manager->m_cache)==1);
    CHECK(loads[0]=="Weapons/Rifle/Textures/rifle_c.dds");
    CHECK(loads[2]=="effects/e.dds");
    CHECK(loads[3]=="Weapons/Rifle/Textures/@@ABSOLUTE@@Common/Textures/SpecularLUT_pow36.dds");
    CHECK(tex(first,0)->refCount==2 && virtualRetains==4);
    Bytes identical("ANIMATED_EnvMap_ColorMapGloss_alpha_test_alpha_one_Cockpit_NoHemiLight",
              {"rifle_c.dds","rifle_b.dds","effects/e.dds","SpecularLUT_pow36"});
    BundledMeshState* same=read(identical,true); // skipped textures are a distinct cache identity
    CHECK(same!=first && count(manager->m_cache)==2 && !tex(same,0));
    CHECK(!same->m_normalMap && !same->m_extraMap && count(same->m_textures)==4);
    CHECK(same->release()==0 && count(manager->m_cache)==1);
    Bytes identicalLoaded("ANIMATED_EnvMap_ColorMapGloss_alpha_test_alpha_one_Cockpit_NoHemiLight",
              {"rifle_c.dds","rifle_b.dds","effects/e.dds","SpecularLUT_pow36"});
    same=read(identicalLoaded,false,true);
    CHECK(same==first && first->getRef()==2 && tex(first,0)->refCount==2 && virtualRetains==4);
    first->m_noHemiLight=0;
    Bytes identityIgnoresFlags("ANIMATED_EnvMap_ColorMapGloss_alpha_test_alpha_one_Cockpit_NoHemiLight",
              {"rifle_c.dds","rifle_b.dds","effects/e.dds","SpecularLUT_pow36"});
    CHECK(read(identityIgnoresFlags)==first && !first->m_noHemiLight);
    CHECK(first->release()==2 && first->release()==1);

    Bytes differentCase("animated_EnvMap_ColorMapGloss_alpha_test_alpha_one_Cockpit_NoHemiLight",
              {"rifle_c.dds","rifle_b.dds","effects/e.dds","SpecularLUT_pow36"});
    BundledMeshState* second=read(differentCase);CHECK(second!=first && second->m_animated);
    CHECK(tex(first,0)->refCount==3);CHECK(second->release()==0 && tex(first,0)->refCount==2);

    *reinterpret_cast<int*>(rendererBytes+0x6CC)=1;loads.clear();
    Bytes quality("alpha_test",{"base.dds","normal_b.dds","extra.dds"});
    BundledMeshState* q=read(quality);
    CHECK(loads.size()==2 && !tex(q,1) && !q->m_normalMap && q->m_extraMap);
    CHECK(q->m_feature39 && !q->m_alphaTest);q->release();
    loads.clear();Bytes empty("EnvMap",{"","ignored.dds"});
    BundledMeshState* e=read(empty,true);
    CHECK(loads.size()==1 && loads[0]=="Weapons/Rifle/Textures/default");
    CHECK(tex(e,0) && !tex(e,1) && count(e->m_textures)==2 && e->m_envMap);
    e->release();

    *reinterpret_cast<int*>(rendererBytes+0x6CC)=0;loads.clear();
    Bytes obsolete("Base",{"default_sdgbmfbf_color_lut"});
    BundledMeshState* o=read(obsolete);
    CHECK(obsoleteLogs==1 && loads.size()==1);
    CHECK(loads[0]=="Weapons/Rifle/Textures/@@ABSOLUTE@@Common/Textures/SpecularLUT_pow36.dds");o->release();
    Bytes missing("Base",{"missing.dds","missing_b.dds"});BundledMeshState* m=read(missing);
    CHECK(!tex(m,0) && !tex(m,1) && m->m_normalMap && !m->m_feature39);m->release();
    Bytes shortName("Base",{"base.dds","abcde"});BundledMeshState* s=read(shortName);
    CHECK(s->m_normalMap);s->release(); // shipped unsigned size()-6/npos quirk
    Bytes noMaps("EnvMap",{});BundledMeshState* n=read(noMaps);
    CHECK(count(n->m_textures)==0 && n->m_feature39 && n->m_envMap);n->release();

    manager->m_cache.push_back(first);
    CHECK(count(manager->m_cache)==2);CHECK(first->release()==0 && count(manager->m_cache)==0);
    for(const auto& item:textures)CHECK(item.second->refCount==1);
    CHECK(destroyed==0);bundledMeshStateManagerRelease();CHECK(!g_bundledMeshStateManager);
    bundledMeshStateManagerRetain();Bytes transient("Base",{"transient.dds"});
    BundledMeshState* transientState=read(transient);CHECK(tex(transientState,0)->refCount==1);
    transientState->release();CHECK(destroyed==1 && count(g_bundledMeshStateManager->m_cache)==0);
    bundledMeshStateManagerRelease();CHECK(!g_bundledMeshStateManager);
    for(const auto& item:textures)delete item.second;
    textures.clear();
    std::printf("BundledMesh state: %d checks passed (strict x86 fixture).\n",checks);
}
#include "renderer_accessor_fixture_support.h"   // RendDX9Base.h Renderer accessors (x86 identity)
