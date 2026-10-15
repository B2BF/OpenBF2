// [TEMP] Minimal engine collaborators around the unchanged extracted bind body.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#if defined(_M_IX86)
#include <string>
#endif
struct IBase { void** table; };
int checks=0,failures=0,flags=0,names=0,queries=0,binds=0;
void check(bool result,const char* text)
{++checks;if(!result){++failures;std::fprintf(stderr,"FAIL: %s\n",text);}}
std::string expected;
char targetStorage;
void* target=&targetStorage;
void** slots=0;IBase object;
struct Manager
{
 IBase* available;
 IBase* getSingleton(const std::string& name)
 {++queries;check(flags==1&&name=="EnvMapManager","flag update precedes exact singleton lookup");return available;}
} manager;
Manager* classManager=&manager;
struct EnvMapTemplate
{
 void* m_envMap;
 std::string name;
 void updateFlags(unsigned int set,unsigned int clear){check(set==1&&clear==0,"bind updates exact flags");flags=1;}
 const std::string& getName(){++names;return name;}
 void templateInitialize();
};
#include "envmap_bind_body.inc"
#if defined(_M_IX86)
void* __fastcall bind(void*,void*,const std::string& name)
#else
void* bind(void*,const std::string& name)
#endif
{
 ++binds;
 check(name.size()==expected.size()&&std::memcmp(name.c_str(),expected.data(),expected.size())==0,"name argument preserves all bytes");
 return target;
}
#if defined(_M_IX86)
void* __fastcall wrongSlot(void*,void*,const std::string&)
#else
void* wrongSlot(void*,const std::string&)
#endif
{
 std::fprintf(stderr,"FAIL: EnvMapTemplate called a manager slot other than ordinal 7\n");
 std::fflush(stderr);
 std::exit(1);
}
int main()
{
 void* table[8];
 for(int i=0;i<8;++i)table[i]=reinterpret_cast<void*>(&wrongSlot);
 table[7]=reinterpret_cast<void*>(&bind);object.table=table;
 EnvMapTemplate value;value.m_envMap=reinterpret_cast<void*>(7);value.name="ignored";
 manager.available=0;value.templateInitialize();
 check(value.m_envMap==reinterpret_cast<void*>(7)&&names==0&&binds==0,"missing manager preserves cached pointer and avoids name query");
 manager.available=&object;
 const std::string inputs[]={"", "small", std::string(15,'a'),std::string(16,'b'),std::string(100,'c'),std::string("in\0line",7)};
 for(const auto& input:inputs)
 {
  expected=input;value.name=input;value.m_envMap=0;flags=0;value.templateInitialize();
  check(value.m_envMap==target,"manager return cached without refcount operation");
 }
 check(names==6&&binds==6&&queries==7,"one virtual name read and one template call per successful bind");
#ifdef _WIN64
 check(reinterpret_cast<std::uintptr_t>(target)>UINT32_MAX,"manager return preserves a real pointer above 4 GiB");
#endif
 std::printf("EnvMapTemplate bind: %d/%d passed\n",checks-failures,checks);return failures?1:0;
}
