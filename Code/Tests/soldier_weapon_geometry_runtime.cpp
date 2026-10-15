// [TEMP] [CHOSEN, not recovered] observers around unchanged production weapon-geometry blocks.
// The fixture does not construct a Soldier, load a mesh or render a frame.
#define private public
#include <string>
#undef private
#include "../BF2/Game/Objects/GeometryPartAccess.h"
#include "../BF2/Game/Objects/IWeaponObject.h"
#include "../BF2/Animation/BoneAnimation/Skeleton.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace dice::hfe;
using namespace dice::hfe::world;
void applyWeaponGeometryFixture(int&, void*, IWeaponObject*, dice::anim::Skeleton*, const Mat4*);
void gatherWeaponBonesFixture(void*, std::vector<int>&);
static_assert(sizeof(void*) == 4, "x86 fixture only");
namespace {
int checks, failures, ammo, parts, lod, lodCount, setLodValue, geomIndex, published;
int* changingMode;
const Mat4* pointers[32];
int indexes[32];
char order[128]; int orderSize;
void note(char c) { order[orderSize++] = c; order[orderSize] = 0; }
void check(bool ok, const char* name) {
    ++checks; if (!ok) ++failures; std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
}
struct Receiver { void** table; } geometry, weapon, objectTemplate;
void* templateResult;
alignas(std::string) unsigned char nameBytes[sizeof(std::string)] = {};
std::string& name = *reinterpret_cast<std::string*>(nameBytes);
int __fastcall getAmmo(void*, void*) { note('A'); return ammo; }
int __fastcall getParts(void*, void*) { note('E'); return parts; }
int __fastcall getLod(void*, void*) { note('L'); return lod; }
int __fastcall getLodCount(void*, void*) { note('C'); return lodCount; }
void* __fastcall getTemplate(void*, void*) { note('T'); return templateResult; }
const std::string& __fastcall getName(void*, void*) { note('N'); return name; }
void __fastcall setLod(void*, void*, int n) { note('D'); setLodValue = n; }
void __fastcall setGeom(void*, void*, int n) { note('S'); geomIndex = n; }
void __fastcall setPart(void*, void*, const Mat4* transform, int part) {
    note('P'); pointers[published] = transform; indexes[published++] = part;
    if (changingMode) *changingMode = 1;
}
void reset() {
    orderSize=0; order[0]=0; published=0; setLodValue=-99; changingMode=0;
    ammo=0; parts=3; lod=0; lodCount=3; templateResult=&objectTemplate;
    std::memset(nameBytes, 0, sizeof(nameBytes));
    std::memcpy(name.m_bx.buf, "insgr_rpg", 10); name.m_size=9; name.m_res=15;
}
}
int main() {
    void* geometryTable[0x84/4] = {}; geometry.table=geometryTable;
    void* weaponTable[0x88/4] = {}; weapon.table=weaponTable;
    void* templateTable[0x1C/4] = {}; objectTemplate.table=templateTable;
    geometryTable[0x5C/4]=reinterpret_cast<void*>(&getParts);
    geometryTable[0x7C/4]=reinterpret_cast<void*>(&getLod);
    geometryTable[0x80/4]=reinterpret_cast<void*>(&getLodCount);
    geometryTable[0x38/4]=reinterpret_cast<void*>(&getTemplate);
    geometryTable[0x78/4]=reinterpret_cast<void*>(&setLod);
    geometryTable[0x6C/4]=reinterpret_cast<void*>(&setGeom);
    geometryTable[0x4C/4]=reinterpret_cast<void*>(&setPart);
    weaponTable[0x84/4]=reinterpret_cast<void*>(&getAmmo);
    templateTable[0x18/4]=reinterpret_cast<void*>(&getName);
    alignas(dice::anim::Skeleton) unsigned char storage[sizeof(dice::anim::Skeleton)] = {};
    dice::anim::Skeleton* skeleton = reinterpret_cast<dice::anim::Skeleton*>(storage);
    Mat4 palette[96]; skeleton->m_transforms=palette;
    const Mat4 local = {{0,-1,0,0, 1,0,0,0, 0,0,1,0, 1,2,3,1}};
    const Mat4 world = {{2,0,0,0, 0,3,0,0, 0,0,4,0, 10,20,30,1}};
    const Mat4 expected = {{0,-3,0,0, 2,0,0,0, 0,0,4,0, 12,26,42,1}};
    auto run = [&](int& mode) { applyWeaponGeometryFixture(mode, &geometry,
        reinterpret_cast<IWeaponObject*>(&weapon), skeleton, &world); };
    for (Mat4& m : palette) m=local;
    reset(); int mode=0; run(mode);
    check(std::strcmp(order,"SALCTTNDEPP")==0, "empty RPG preserves all getter, setter and publication calls");
    check(setLodValue==2 && geomIndex==0, "empty RPG selects lod2 after first-person geom");
    check(published==2 && pointers[0]==&palette[55] && pointers[1]==&palette[56], "part pointers retain skeleton storage starting after bone54");
    check(std::memcmp(&palette[55], &expected, sizeof(Mat4))==0, "weapon bones become world space in shipped multiplication order");
    check(std::memcmp(&palette[54], &local, sizeof(Mat4))==0 && indexes[0]==1 && indexes[1]==2, "attachment part0 stays untouched");
    reset(); ammo=1; parts=1; run(mode);
    check(std::strcmp(order,"SAE")==0 && published==0, "loaded weapon short-circuits empty-RPG queries and part0");
    reset(); lod=1; parts=0; run(mode);
    check(std::strcmp(order,"SALE")==0, "hidden lod1 short-circuits template and lod-count queries");
    reset(); lodCount=2; parts=0; run(mode);
    check(std::strcmp(order,"SALCE")==0, "two-lod mesh skips empty-RPG template path");
    reset(); templateResult=0; parts=0; run(mode);
    check(std::strcmp(order,"SALCTE")==0, "null template skips second getter and name");
    reset(); name.m_bx.buf[0]='x'; parts=0; run(mode);
    check(setLodValue==-99, "other geometry names retain selected lod");
    reset(); ammo=1; parts=20; mode=0; run(mode);
    check(published==15 && indexes[14]==15, "first-person palette publication caps at16 including part0");
    reset(); ammo=1; parts=20; mode=1; run(mode);
    check(published==7 && pointers[0]==&palette[65], "third-person publication starts at64 and caps at8");
    reset(); ammo=1; parts=3; mode=0; changingMode=&mode; run(mode);
    check(pointers[0]==&palette[55] && pointers[1]==&palette[66], "character mode is reread after geometry callbacks");
    reset(); ammo=1; parts=-1; mode=1; run(mode);
    check(published==7, "part count is compared unsigned before the cap");
    reset(); std::vector<int> bones(1, 4); parts=20; gatherWeaponBonesFixture(&geometry,bones);
    check(bones.size()==9 && bones.front()==4 && bones[1]==64 && bones.back()==71, "used-bone arm appends64..71 with cap8");
    reset(); parts=0; bones.clear(); gatherWeaponBonesFixture(&geometry,bones);
    check(bones.empty(), "zero parts contribute no weapon bones");
    reset(); parts=3;
    for(int part=1;part<geometry_part::count(&geometry);++part)
        geometry_part::setTransform(&geometry,&Mat4::getZero(),part);
    check(std::strcmp(order,"EPEPE")==0 && pointers[0]==&Mat4::getZero() && indexes[1]==2,
          "armor seams preserve repeated count and stable zero-transform pointer");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
