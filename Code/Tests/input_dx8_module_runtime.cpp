// [TEMP] [CHOSEN, not recovered] Native module-list and factory boundary checks.
// [RETIRE-WHEN: INPUT-DX8-MODULE-FIXTURE] Replace bounded registration/factory
// checks with real complete client input initialization and shutdown.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "../BF2/Core/Modules.h"
#include "../BF2/Core/Ids.h"
#if !DEDICATED
#include "../BF2/IO/Input/DX8InputDeviceManager.h"
#include "../BF2/IO/Input/InputManager.h"
#endif
using namespace dice::hfe;
using namespace dice::hfe::io;
namespace dice { namespace hfe {
class BF2Render; BF2Render* g_bf2Render=nullptr;
namespace io { IBase* inputDeviceManager=nullptr; IBase* controlMapManager=nullptr; }
} }
static int checks,failures;
static void check(bool good,const char* label){++checks;if(!good){++failures;std::fprintf(stderr,"FAIL: %s\n",label);}}
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    auto* module=new InputDX8Module;IModule* api=module;
    auto* base=static_cast<IBase*>(api->queryInterface(IID_IBase));
    check(sizeof(InputDX8Module)==8&&api->getRef()==1,"native module storage and initial reference");
    check(api->queryInterface(IID_IBase)==api&&api->queryInterface(IID_IClassServer)==api&&
          api->queryInterface(IID_IModule)==api&&!api->queryInterface(0),"all three inherited interface identities");
    check(api->getRef()==1,"interface lookup does not retain");
    api->addRef();check(api->getRef()==2&&api->release()==1,"reference round trip");
    std::vector<ClassRegInfo> classes{{123,"before",nullptr}};
    api->getClassList(classes);
    check(classes[0].classId==123&&classes[0].name=="before"&&!classes[0].create,"class-list append preserves existing rows");
#if !DEDICATED
    check(classes.size()==3,"client appends exactly two rows");
    check(classes[1].classId==120508928u&&classes[1].name=="dice.hfe.io.InputDeviceManager.DX8"&&
          classes[1].create==&DX8InputDeviceManager::create,"DX8 row exact id/name/native factory");
    check(classes[2].classId==3139421003u&&classes[2].name=="dice.hfe.io.InputManager"&&
          classes[2].create==&InputManager::create,"InputManager row exact id/name/native factory");
    check(!classes[1].create(IID_IInputDeviceManager,base)&&!classes[2].create(IID_IInputManager,base),"both actual factories reject aggregation before construction");
    auto* created=classes[1].create(IID_IInputDeviceManager,nullptr);
    check(created!=nullptr,"registered DX8 factory constructs actual native owner");
    auto* deviceManager=static_cast<IInputDeviceManager*>(created->queryInterface(IID_IInputDeviceManager));
    unsigned mask=0;
    check(deviceManager&&deviceManager->getInputDevices()==0&&deviceManager->initDevices(&mask)&&mask==0,"actual empty-mask device initialization requires no hardware");
    check(created->release()==0,"actual DX8 owner releases normally");
    check(!classes[1].create(0,nullptr),"actual DX8 factory rejects unknown IID and destroys owner");
    api->getClassList(classes);
    check(classes.size()==5&&classes[3].name==classes[1].name&&classes[4].name==classes[2].name,"repeated list query appends same ordered rows");
#else
    check(classes.size()==1,"dedicated class list remains retail empty");
#endif
    IBase* sentinel=base;
    std::vector<SingletonRegInfo> singletons{{"before",&sentinel,17,19,23}};
    api->getSingletonList(singletons);
    check(singletons.size()==1&&singletons[0].name=="before"&&sentinel==base,"singleton list remains empty and preserves caller storage");
    check(api->init()&&api->close()&&api->init()&&api->close(),"init/close remain actual repeatable true leaves");
    check(api->release()==0,"final module release");
    std::printf("InputDX8Module D%d: %d/%d checks passed\n",DEDICATED,checks-failures,checks);
    return failures?1:0;
}
