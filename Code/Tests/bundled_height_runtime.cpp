// [TEMP] Collision/host spies around the unchanged production height and additive
// filter implementation. No game, renderer DLL, physics world or network is loaded.
#include <cstdio>
#include <cstring>
#include <cmath>
#include "../BF2/Geom/BundledMeshHeight.cpp"
using namespace dice::hfe;
using namespace dice::hfe::geom;
namespace {
int checks=0, failures=0, probe=0, roots=0, radii=0, playerQueries=0;
void check(bool condition,const char* message)
{ ++checks; if(!condition){++failures;std::fprintf(stderr,"FAIL: %s\n",message);} }
void* meshTable[62]={}; void* objectTable[100]={}; void* managerTable[100]={};
void* lightTable[189]={}; void* playerTable[120]={}; void* pmTable[40]={};
void* geomTable[62]={}; void* templateTable[60]={};
alignas(8) unsigned char meshBytes[sizeof(BundledMesh)]={};
alignas(8) unsigned char objectBytes[0x400]={}, otherBytes[0x400]={}, thirdBytes[0x400]={};
alignas(8) unsigned char lightBytes[0xB00]={}, templateBytes[sizeof(BundledMeshTemplate)]={};
BundledMesh* mesh=reinterpret_cast<BundledMesh*>(meshBytes);
world::IObject* object=reinterpret_cast<world::IObject*>(objectBytes);
world::IObject* other=reinterpret_cast<world::IObject*>(otherBytes);
world::IObject* third=reinterpret_cast<world::IObject*>(thirdBytes);
void** manager=managerTable; void** light=lightTable; void** player=playerTable;
void** pm=pmTable; void** geometry=geomTable;
Mat4 transform={}; const Mat4* parts[1]={&transform};
Vec2 fade={4,40}; float radiusValues[8]={3,3,3,3,3,3,3,3};
bool results[3]={}, additive=false, firstPerson=false;
world::IObject* vehicle=0; world::IPlayer* localPlayer=0;
bool expectPredicates=false; bool observedPredicates=false;
void* initialHit=0; void* lastHit=0; int beforePart=0;
Vec3 origins[3], directions[3]; unsigned masks[3];
unsigned int classId=0x870CA651u;
world::IObject* __fastcall rootCall(void*,void*) { ++roots; return object; }
float __fastcall radiusCall(void*,void*) { return radiusValues[radii++]; }
float __fastcall roofCall(void*,void*) { return 150.0f; }
const Vec2& __fastcall fadeCall(void*,void*) { return fade; }
world::IPlayer* __fastcall localCall(void*,void*) { ++playerQueries;return localPlayer; }
bool __fastcall firstCall(void*,void*) { return firstPerson; }
world::IObject* __fastcall vehicleCall(void*,void*) { return vehicle; }
MeshTemplate* __fastcall templateCall(void*,void*) { return reinterpret_cast<MeshTemplate*>(templateBytes); }
unsigned int __fastcall classCall(void*,void*) { return classId; }
bool __fastcall intersectLineCall(void*,void*,world::IObject*& hit,Vec3& point,Vec3& normal,int& part,
 const Vec3& origin,const Vec3& direction,world::ObjectPredicator* filter,bool a,bool b,bool c,bool d,unsigned int flags)
{
    const int index=probe++;
    if(additive)
    {
        initialHit=hit; beforePart=part;
        observedPredicates=filter->predicate(other);
        check(filter->getFlags()==0 && filter->performCulling(other) && filter->predicateSurface(7),"additive predicate companion slots");
        check(a&&!b&&c&&d&&flags==0,"additive trace flags");
        hit=third;part=37; point.y=19; return true;
    }
    origins[index]=origin;directions[index]=direction;
    masks[index]=(a?1:0)|(b?2:0)|(c?4:0)|(d?8:0)|flags;
    if(index==0)
    {
        initialHit=hit;
        observedPredicates=!filter->predicate(object)&&filter->predicate(other)
            && filter->getFlags()==0&&filter->performCulling(other)&&filter->predicateSurface(4);
        hit=third; point.y=4;
        transform.m[13]=25; // Production must reread the transform after the collision callback.
    }
    else if(index==1)
    {
        lastHit=hit;
        observedPredicates=observedPredicates&&filter->predicate(object)&&filter->getFlags()==0;
        fade.y=55; point.y=7;
    }
    else
    {
        observedPredicates=observedPredicates&&!filter->predicate(object)&&filter->predicate(other);
    }
    part=91;normal.y=3;
    return results[index];
}
void reset()
{
    probe=roots=radii=0;transform.m[13]=20;fade.y=40;initialHit=lastHit=reinterpret_cast<void*>(1);
    for(int i=0;i<8;++i)radiusValues[i]=3;
    results[0]=results[1]=results[2]=true; additive=false; observedPredicates=false;
}
}
namespace dice { namespace hfe {
IBase* objectManager=reinterpret_cast<IBase*>(&manager);
IBase* playerManager=reinterpret_cast<IBase*>(&pm);
namespace geom {
LightManager* g_LightManager=reinterpret_cast<LightManager*>(&light);
// [TEMP] Existing root helper's collision-world input is represented by cached roots.
void* staticMeshRootParent(void* value)
{ return value ? *reinterpret_cast<void**>(static_cast<unsigned char*>(value)+0x0C) : 0; }
}
namespace world { bool ObjectPredicator::predicateSurface(int){return true;} }
} }
int main()
{
    meshTable[0xA4/4]=reinterpret_cast<void*>(&rootCall);
    objectTable[0x90/4]=reinterpret_cast<void*>(&radiusCall);
    managerTable[0x68/4]=reinterpret_cast<void*>(&intersectLineCall);
    lightTable[0x2A0/4]=reinterpret_cast<void*>(&roofCall);lightTable[0x2C0/4]=reinterpret_cast<void*>(&fadeCall);
    pmTable[0x30/4]=reinterpret_cast<void*>(&localCall);
    playerTable[0x184/4]=reinterpret_cast<void*>(&firstCall);playerTable[0x44/4]=reinterpret_cast<void*>(&vehicleCall);
    geomTable[0x38/4]=reinterpret_cast<void*>(&templateCall);templateTable[0x10/4]=reinterpret_cast<void*>(&classCall);
    *reinterpret_cast<void***>(objectBytes)=objectTable;*reinterpret_cast<void***>(otherBytes)=objectTable;
    *reinterpret_cast<void***>(templateBytes)=templateTable;
    *reinterpret_cast<void**>(objectBytes+0x0C)=object;*reinterpret_cast<void**>(otherBytes+0x0C)=other;
    mesh->m_vptr=meshTable;mesh->m_partTransforms=parts;mesh->m_ownerObject=object;mesh->m_heightmapCluster=object;
    reset();mesh->m_heightmapCluster=0;mesh->updateHeightOverTerrain();
    check(probe==0&&roots==0,"missing heightmap early return");
    mesh->m_heightmapCluster=object;mesh->m_ownerObject=0;mesh->updateHeightOverTerrain();
    check(probe==0&&roots==0,"missing owner early return");mesh->m_ownerObject=object;
    reset();mesh->updateHeightOverTerrain();
    check(probe==3&&roots==4&&radii==3,"three probes with repeated root/radius calls");
    check(mesh->m_heightOverTerrain==6&&mesh->m_shadowSunHeight==18&&mesh->m_underRoof==1,"hit height normalization and sun distance");
    check(initialHit==0&&lastHit==third,"hit object resets once and persists between probes");
    check(directions[0].y==-12&&directions[1].y==-40&&directions[2].y==150,"three exact ray lengths");
    check(origins[0].y==20&&origins[1].y==25&&origins[2].y==25,"origin reread after callbacks");
    check(masks[0]==13&&masks[1]==12&&masks[2]==0&&observedPredicates,"probe flags and root/zero-mask predicates");
    reset();results[0]=results[1]=results[2]=false;mesh->updateHeightOverTerrain();
    check(mesh->m_heightOverTerrain==3&&mesh->m_shadowSunHeight==55&&mesh->m_underRoof==0,"miss fallback uses radius and retained fade reference");
    check(roots==5&&radii==4,"miss performs extra fresh root/radius query");
    reset();radiusValues[1]=30;radiusValues[2]=1;mesh->updateHeightOverTerrain();
    check(mesh->m_heightOverTerrain==0,"height lower clamp and minimum normalization radius");
    reset();radiusValues[2]=1;mesh->updateHeightOverTerrain();
    check(mesh->m_heightOverTerrain==9,"normalization divisor minimum is two");
    reset();radiusValues[1]=std::nanf("");mesh->updateHeightOverTerrain();
    check(std::isnan(mesh->m_heightOverTerrain),"height unordered value remains NaN");
    void* returned=0;Vec3 origin={1,2,3},direction={4,5,6};
    additive=true;probe=0;mesh->m_ownerObject=other;localPlayer=0;
    bundledMeshTraceAdditive(mesh,origin,direction,returned);
    check(!observedPredicates&&returned==third&&initialHit==0,"additive rejects own object and returns trace output");
    mesh->m_ownerObject=object;probe=0;
    bundledMeshTraceAdditive(mesh,origin,direction,returned);
    check(observedPredicates&&beforePart==37,"additive accepts other object and keeps static part scratch");
    localPlayer=reinterpret_cast<world::IPlayer*>(&player);firstPerson=true;vehicle=other;probe=0;
    bundledMeshTraceAdditive(mesh,origin,direction,returned);
    check(!observedPredicates,"additive rejects local first-person vehicle root");
    firstPerson=false;probe=0;world::object_detail::LayoutAccess::geometry(other)=reinterpret_cast<IBase*>(&geometry);
    reinterpret_cast<BundledMeshTemplate*>(templateBytes)->m_hasEnvironmentMap=1;
    bundledMeshTraceAdditive(mesh,origin,direction,returned);
    check(!observedPredicates,"additive rejects another environment-mapped BundledMesh");
    probe=0;classId=0x870CA650u;bundledMeshTraceAdditive(mesh,origin,direction,returned);
    check(observedPredicates,"additive permits different geometry template type");
    probe=0;classId=0x870CA651u;reinterpret_cast<BundledMeshTemplate*>(templateBytes)->m_hasEnvironmentMap=0;
    bundledMeshTraceAdditive(mesh,origin,direction,returned);
    check(observedPredicates,"additive permits BundledMesh without environment map");
    probe=0;world::object_detail::LayoutAccess::geometry(other)=reinterpret_cast<IBase*>(mesh);
    bundledMeshTraceAdditive(mesh,origin,direction,returned);
    check(observedPredicates,"additive permits own geometry without template query");
    std::printf("Bundled height/additive: %d/%d passed\n",checks-failures,checks);return failures?1:0;
}
