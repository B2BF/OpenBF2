// [TEMP] Actual EnvMapObject/template bodies with narrow heap and manager spies.
// No original DLL, renderer device or game is loaded.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include "../BF2/RendMisc/EnvMapSystem/EnvMapObject.cpp"
using namespace dice::hfe;
using namespace dice::hfe::Rend;
namespace {
int checks=0,failures=0,allocations=0,removals=0,queues=0,diagnostics=0;
bool failAllocation=false,removalOrder=true,queueOrder=true;
EnvMapObject* active=0; EnvMapEntry* removed=0;
void* managerTable[15]={};void** manager=managerTable;
void check(bool pass,const char* text)
{++checks;if(!pass){++failures;std::fprintf(stderr,"FAIL: %s\n",text);}}
void __fastcall queue(void*,void*,EnvMapObject* value)
{++queues;queueOrder=queueOrder&&value==active&&value->m_entry==0&&value->m_count==0;}
template<class Return,class... Args> Return call(void* self,unsigned int offset,Args... args)
{
    typedef Return(__thiscall* Function)(void*,Args...);
    return reinterpret_cast<Function>((*static_cast<void***>(self))[offset/4])(self,args...);
}
}
namespace renddx9abi {
void* engineAlloc(unsigned int size){++allocations;return failAllocation?0:std::malloc(size);}
void engineFree(void* p){std::free(p);}
void logf(const char* format,...)
{
    ++diagnostics;char message[256];va_list args;va_start(args,format);
    std::vsnprintf(message,sizeof(message),format,args);va_end(args);
    check(std::strstr(message,"Exceeding maximum amout of envMaps 0")!=0,"retail limit diagnostic text");
}
}
namespace dice { namespace hfe { namespace Rend {
IBase* g_EnvMapManager=reinterpret_cast<IBase*>(&manager);
// [TEMP] The separately recovered manager collaborator; only these ctor stores
// and removeEntry observation are needed to exercise the object's real body.
EnvMapEntry::EnvMapEntry(){position.x=position.y=position.z=0;flag=false;texture=0;}
void EnvMapManager::removeEntry(EnvMapEntry* entry)
{
    ++removals;removed=entry;
    removalOrder=removalOrder&&active->m_entry==entry;
}
} } }
int main()
{
    managerTable[0x18/4]=reinterpret_cast<void*>(&queue);
    std::string name("env_fixture_name_longer_than_inline_buffer");
    alignas(EnvMapObjectTemplate) unsigned char templateBytes[sizeof(EnvMapObjectTemplate)];
    std::memset(templateBytes,0xA5,sizeof(templateBytes));
    EnvMapObjectTemplate* t=new(templateBytes) EnvMapObjectTemplate(name);
    check(call<float>(t,8)==1&&call<bool>(t,0x10),"template slot defaults size one/static true");
    const std::string& actual=call<const std::string&>(t,0);
    check(actual.size()==name.size()&&std::strcmp(actual.c_str(),name.c_str())==0,"template owns copied name");
    check(templateBytes[9]==0xA5&&templateBytes[10]==0xA5&&templateBytes[11]==0xA5,"template padding remains untouched");
    call<void>(t,4,-3.25f);call<void>(t,0xC,false);
    check(call<float>(t,8)==-3.25f&&!call<bool>(t,0x10),"template setters preserve unclamped values");
    alignas(EnvMapObject) unsigned char objectBytes[sizeof(EnvMapObject)];
    std::memset(objectBytes,0xA5,sizeof(objectBytes));
    active=new(objectBytes) EnvMapObject(t);
    check(active->m_template==t&&active->m_entry==0&&active->m_value10==1&&active->m_count==0&&active->m_limit==20,"object constructor fields");
    check(call<bool>(active,4)&&objectBytes[5]==0xA5&&objectBytes[7]==0xA5,"enabled default and untouched padding");
    call<void>(active,0,false);check(!call<bool>(active,4),"enabled setter/getter slots");
    Mat4 transform={};transform.m[12]=-1.2f;transform.m[13]=2.9f;transform.m[14]=7.1f;
    failAllocation=true;
    check(!call<bool,const Mat4&,EnvMapObjectTemplate*,void*>(active,8,transform,t,0)&&active->m_count==0&&active->m_entry==0,"allocation failure leaves no entry/count");
    failAllocation=false;
    check(call<bool,const Mat4&,EnvMapObjectTemplate*,void*>(active,8,transform,t,0),"disabled object still updates as shipped");
    EnvMapEntry* retained=active->m_entry;
    check(retained&&retained->position.x==-2&&retained->position.y==2&&retained->position.z==7&&!retained->flag&&retained->texture==0,"entry uses floored position and current argument template flag");
    check(call<unsigned int>(active,0x10)==1&&allocations==2,"count slot and one successful allocation");
    call<void>(t,0xC,true);transform.m[12]=-1.8f;
    call<bool,const Mat4&,EnvMapObjectTemplate*,void*>(active,8,transform,t,0);
    check(active->m_entry==retained&&retained->position.x==-2&&!retained->flag&&allocations==2,"existing entry stays allocated and retains original static flag");
    transform.m[14]=11.3f;call<bool,const Mat4&,EnvMapObjectTemplate*,void*>(active,8,transform,t,0);
    check(retained->position.z==11&&active->m_count==1,"existing position refresh leaves count unchanged");
    transform.m[12]=std::nanf("");call<bool,const Mat4&,EnvMapObjectTemplate*,void*>(active,8,transform,t,0);
    check(std::isnan(retained->position.x),"unordered position refuses epsilon equality");
    call<void>(active,0xC);
    check(removals==1&&removed==retained&&active->m_entry==0&&active->m_count==0&&removalOrder,"clear removes manager entry before resetting fields");
    call<void>(active,0xC);check(removals==2&&removed==0,"clear still calls manager with null entry");
    active->m_limit=0;
    check(!call<bool,const Mat4&,EnvMapObjectTemplate*,void*>(active,8,transform,t,0)&&diagnostics==1&&allocations==2,"unsigned allocation limit refuses creation");
    active->m_limit=20;active->m_entry=retained;active->m_count=1;
    call<void>(active,0x14);
    check(removals==3&&queues==1&&queueOrder&&removalOrder,"release clears then queues the same object");
    active->m_entry=retained;active->~EnvMapObject();
    check(removals==3,"destructor does not call clear or free the entry");
    std::free(retained);t->~EnvMapObjectTemplate();
    std::printf("EnvMapObject/template: %d/%d passed\n",checks-failures,checks);return failures?1:0;
}
