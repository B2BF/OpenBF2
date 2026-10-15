// [TEMP] [CHOSEN, not recovered] Focused x86 ABI fixture; not part of any shipped image.
// Verifies the recovered 18-entry manager declaration and the evidence-scoped instance/template
// dispatch views used by Player::handleGasClouds. No renderer, particle pool, or live effect runs.

#include "../BF2/Geom/ParticleSystem/ParticleSystemManager.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <type_traits>

using dice::hfe::IBase;
using dice::hfe::Vec3;
using dice::hfe::geom::IParticleSystemInstance;
using dice::hfe::geom::IParticleSystemManager;
using dice::hfe::geom::IParticleSystemTemplate;
using dice::hfe::geom::IParticleSystemTemplateType0;
using dice::hfe::geom::IParticleSystemTemplateType1;
using dice::hfe::geom::ParticleSystemInstanceList;
using dice::hfe::geom::ParticleSystemName;
using dice::hfe::geom::ParticleSystemSettings;

namespace {

int checks;
int failures;

#define CHECK(expression)                                                                  \
    do                                                                                     \
    {                                                                                      \
        ++checks;                                                                          \
        if (!(expression))                                                                 \
        {                                                                                  \
            ++failures;                                                                    \
            std::printf("FAIL line %d: %s\n", __LINE__, #expression);                    \
        }                                                                                  \
    } while (0)

class ManagerOrderProbe : public IParticleSystemManager
{
public:
    ManagerOrderProbe() : mark(0), pointerMark(0), intMark(0), boolMarkA(false), boolMarkB(false)
    {
        std::memset(&instances, 0, sizeof(instances));
    }

    virtual ~ManagerOrderProbe() { ++destructorCalls; }

    virtual void addRef() { mark = 0; }
    virtual int getRef() { mark = 1; return 101; }
    virtual int release() { mark = 2; return 102; }
    virtual void* queryInterface(unsigned int iid) const
    {
        mark = 3;
        return iid == 103 ? const_cast<ManagerOrderProbe*>(this) : 0;
    }
    virtual void reserved_reacquireWindManager() { mark = 4; }
    virtual IParticleSystemInstance* createInstance(IParticleSystemTemplate* value)
    {
        mark = 5; pointerMark = value;
        return reinterpret_cast<IParticleSystemInstance*>(0x105u);
    }
    virtual void removeInstance(IParticleSystemInstance* value) { mark = 6; pointerMark = value; }
    virtual IParticleSystemTemplate* createParticleSystem(int type, const ParticleSystemName& name)
    {
        mark = 7; intMark = type; pointerMark = const_cast<ParticleSystemName*>(&name);
        return reinterpret_cast<IParticleSystemTemplate*>(0x107u);
    }
    virtual void removeParticleSystem(IParticleSystemTemplate* value)
    {
        mark = 8; pointerMark = value;
    }
    virtual void reserved_releaseAllInstances() { mark = 9; }
    virtual void update(float value) { mark = value == 10.25f ? 10 : -10; }
    virtual void onPathDraw(bool firstPerson, bool secondPass)
    {
        mark = 11; boolMarkA = firstPerson; boolMarkB = secondPass;
    }
    virtual void resetStats() { mark = 12; }
    virtual void togglePause() { mark = 13; }
    virtual ParticleSystemInstanceList* getInstances() { mark = 14; return &instances; }
    virtual void applySettings(const ParticleSystemSettings* value)
    {
        mark = 15; pointerMark = const_cast<ParticleSystemSettings*>(value);
    }
    virtual void reserved_clearInternalVector() { mark = 16; }

    mutable int mark;
    void* pointerMark;
    int intMark;
    bool boolMarkA;
    bool boolMarkB;
    ParticleSystemInstanceList instances;
    static int destructorCalls;
};

int ManagerOrderProbe::destructorCalls = 0;

void testManagerVirtualOrder()
{
#if defined(_M_IX86)
    ManagerOrderProbe probe;
    IBase* const base = &probe;
    IParticleSystemManager* const manager = &probe;
    CHECK(reinterpret_cast<void*>(base) == reinterpret_cast<void*>(manager));

    void** const table = *reinterpret_cast<void***>(&probe);
    typedef void (__thiscall *VoidFn)(ManagerOrderProbe*);
    typedef int (__thiscall *IntFn)(ManagerOrderProbe*);
    typedef void* (__thiscall *QueryFn)(ManagerOrderProbe*, unsigned int);
    typedef IParticleSystemInstance* (__thiscall *CreateInstanceFn)(
        ManagerOrderProbe*, IParticleSystemTemplate*);
    typedef void (__thiscall *RemoveInstanceFn)(ManagerOrderProbe*, IParticleSystemInstance*);
    typedef IParticleSystemTemplate* (__thiscall *CreateTemplateFn)(
        ManagerOrderProbe*, int, const ParticleSystemName&);
    typedef void (__thiscall *RemoveTemplateFn)(ManagerOrderProbe*, IParticleSystemTemplate*);
    typedef void (__thiscall *UpdateFn)(ManagerOrderProbe*, float);
    typedef void (__thiscall *DrawFn)(ManagerOrderProbe*, bool, bool);
    typedef ParticleSystemInstanceList* (__thiscall *InstancesFn)(ManagerOrderProbe*);
    typedef void (__thiscall *SettingsFn)(ManagerOrderProbe*, const ParticleSystemSettings*);
    typedef void (__thiscall *DeletingDtorFn)(ManagerOrderProbe*, unsigned int);

    reinterpret_cast<VoidFn>(table[0])(&probe);
    CHECK(probe.mark == 0);
    CHECK(reinterpret_cast<IntFn>(table[1])(&probe) == 101 && probe.mark == 1);
    CHECK(reinterpret_cast<IntFn>(table[2])(&probe) == 102 && probe.mark == 2);
    CHECK(reinterpret_cast<QueryFn>(table[3])(&probe, 103) == &probe && probe.mark == 3);
    reinterpret_cast<VoidFn>(table[4])(&probe);
    CHECK(probe.mark == 4);

    IParticleSystemTemplate* const templateValue =
        reinterpret_cast<IParticleSystemTemplate*>(0x51515151u);
    CHECK(reinterpret_cast<CreateInstanceFn>(table[5])(&probe, templateValue) ==
              reinterpret_cast<IParticleSystemInstance*>(0x105u) &&
          probe.mark == 5 && probe.pointerMark == templateValue);

    IParticleSystemInstance* const instanceValue =
        reinterpret_cast<IParticleSystemInstance*>(0x61616161u);
    reinterpret_cast<RemoveInstanceFn>(table[6])(&probe, instanceValue);
    CHECK(probe.mark == 6 && probe.pointerMark == instanceValue);

    alignas(ParticleSystemName) unsigned char nameStorage[sizeof(ParticleSystemName)] = {};
    const ParticleSystemName& name = *reinterpret_cast<const ParticleSystemName*>(nameStorage);
    CHECK(reinterpret_cast<CreateTemplateFn>(table[7])(&probe, 4, name) ==
              reinterpret_cast<IParticleSystemTemplate*>(0x107u) &&
          probe.mark == 7 && probe.intMark == 4 && probe.pointerMark == &name);
    reinterpret_cast<RemoveTemplateFn>(table[8])(&probe, templateValue);
    CHECK(probe.mark == 8 && probe.pointerMark == templateValue);

    reinterpret_cast<VoidFn>(table[9])(&probe);
    CHECK(probe.mark == 9);
    reinterpret_cast<UpdateFn>(table[10])(&probe, 10.25f);
    CHECK(probe.mark == 10);
    reinterpret_cast<DrawFn>(table[11])(&probe, true, false);
    CHECK(probe.mark == 11 && probe.boolMarkA && !probe.boolMarkB);
    reinterpret_cast<VoidFn>(table[12])(&probe);
    CHECK(probe.mark == 12);
    reinterpret_cast<VoidFn>(table[13])(&probe);
    CHECK(probe.mark == 13);
    CHECK(reinterpret_cast<InstancesFn>(table[14])(&probe) == &probe.instances && probe.mark == 14);

    ParticleSystemSettings settings = {};
    reinterpret_cast<SettingsFn>(table[15])(&probe, &settings);
    CHECK(probe.mark == 15 && probe.pointerMark == &settings);
    reinterpret_cast<VoidFn>(table[16])(&probe);
    CHECK(probe.mark == 16);

    const int before = ManagerOrderProbe::destructorCalls;
    ManagerOrderProbe* const heapProbe = new ManagerOrderProbe;
    void** const heapTable = *reinterpret_cast<void***>(heapProbe);
    reinterpret_cast<DeletingDtorFn>(heapTable[17])(heapProbe, 1u);
    CHECK(ManagerOrderProbe::destructorCalls == before + 1);
#else
    CHECK(false && "ParticleSystemManager ABI fixture must run as x86");
#endif
}

struct RawView
{
    void** table;
    int mark;
};

RawView* rawTemplate;
Vec3 expectedPosition = { 3.0f, 4.0f, 5.0f };

IParticleSystemTemplate* __fastcall instanceTemplate(void*, void*)
{
    return reinterpret_cast<IParticleSystemTemplate*>(rawTemplate);
}
const Vec3* __fastcall instancePosition(void*, void*) { return &expectedPosition; }
float __fastcall instanceRadius(void*, void*) { return 17.5f; }
void __fastcall instancePause(void* self, void*, bool paused)
{
    static_cast<RawView*>(self)->mark = paused ? 18 : -18;
}

void __fastcall templateRefresh(void* self, void*) { static_cast<RawView*>(self)->mark = 8; }
void __fastcall templateEmitPrio(void* self, void*, int value)
{
    static_cast<RawView*>(self)->mark = value == 10 ? 0x10 : -0x10;
}
void __fastcall templateLodDistance(void* self, void*, int value)
{
    static_cast<RawView*>(self)->mark = value == 18 ? 0x18 : -0x18;
}
void __fastcall templateQuality(void* self, void*, int value)
{
    static_cast<RawView*>(self)->mark = value == 20 ? 0x20 : -0x20;
}
void __fastcall templateRadius(void* self, void*, Vec3 value)
{
    static_cast<RawView*>(self)->mark = value.x == 7.0f ? 0x28 : -0x28;
}
void __fastcall templateEmitter(void* self, void*, int value)
{
    static_cast<RawView*>(self)->mark = value == 40 ? 0x40 : -0x40;
}
void __fastcall templateParticle(void* self, void*, int value)
{
    static_cast<RawView*>(self)->mark = value == 18 ? 0x1C8 : -0x1C8;
}
void __fastcall templateBlend(void* self, void*, int value)
{
    static_cast<RawView*>(self)->mark = value == 19 ? 0x1E0 : -0x1E0;
}
void __fastcall templateDrawOrder(void* self, void*, int value)
{
    static_cast<RawView*>(self)->mark = value == 17 ? 0x1D0 : -0x1D0;
}
int __fastcall templateGasType(void*, void*) { return 1; }
float __fastcall templateBlur(void*, void*) { return 1.38f; }
float __fastcall templateHold(void*, void*) { return 1.40f; }
float __fastcall templateMin(void*, void*) { return 1.48f; }
float __fastcall templateMax(void*, void*) { return 1.50f; }
float __fastcall templateRadiusTime(void*, void*) { return 1.58f; }
float __fastcall templateDamage(void*, void*) { return 1.60f; }

void testEvidenceScopedViews()
{
#if defined(_M_IX86)
    void* templateTable[0x1E4 / 4] = {};
    RawView templateObject = { templateTable, 0 };
    rawTemplate = &templateObject;
    templateTable[0x08 / 4] = reinterpret_cast<void*>(&templateRefresh);
    templateTable[0x10 / 4] = reinterpret_cast<void*>(&templateEmitPrio);
    templateTable[0x18 / 4] = reinterpret_cast<void*>(&templateLodDistance);
    templateTable[0x20 / 4] = reinterpret_cast<void*>(&templateQuality);
    templateTable[0x28 / 4] = reinterpret_cast<void*>(&templateRadius);
    templateTable[0x40 / 4] = reinterpret_cast<void*>(&templateEmitter);
    templateTable[0x130 / 4] = reinterpret_cast<void*>(&templateGasType);
    templateTable[0x138 / 4] = reinterpret_cast<void*>(&templateBlur);
    templateTable[0x140 / 4] = reinterpret_cast<void*>(&templateHold);
    templateTable[0x148 / 4] = reinterpret_cast<void*>(&templateMin);
    templateTable[0x150 / 4] = reinterpret_cast<void*>(&templateMax);
    templateTable[0x158 / 4] = reinterpret_cast<void*>(&templateRadiusTime);
    templateTable[0x160 / 4] = reinterpret_cast<void*>(&templateDamage);
    templateTable[0x1C8 / 4] = reinterpret_cast<void*>(&templateParticle);
    templateTable[0x1D0 / 4] = reinterpret_cast<void*>(&templateDrawOrder);
    templateTable[0x1E0 / 4] = reinterpret_cast<void*>(&templateBlend);

    IParticleSystemTemplate* const systemTemplate =
        reinterpret_cast<IParticleSystemTemplate*>(&templateObject);
    IParticleSystemTemplateType0* const spriteTemplate =
        reinterpret_cast<IParticleSystemTemplateType0*>(&templateObject);
    IParticleSystemTemplateType1* const meshTemplate =
        reinterpret_cast<IParticleSystemTemplateType1*>(&templateObject);
    systemTemplate->refreshDerivedState();
    CHECK(templateObject.mark == 8);
    systemTemplate->setEmitPrio(10);
    CHECK(templateObject.mark == 0x10);
    systemTemplate->setLodDistance(18);
    CHECK(templateObject.mark == 0x18);
    systemTemplate->setQualityLevel(20);
    CHECK(templateObject.mark == 0x20);
    const Vec3 radius = { 7.0f, 0.0f, 0.0f };
    systemTemplate->setEmitRadius(radius);
    CHECK(templateObject.mark == 0x28);
    systemTemplate->setEmitterType(40);
    CHECK(templateObject.mark == 0x40);
    spriteTemplate->setParticleType(18);
    CHECK(templateObject.mark == 0x1C8);
    meshTemplate->setBlendMode(19);
    CHECK(templateObject.mark == 0x1E0);
    spriteTemplate->setDrawOrder(17);
    CHECK(templateObject.mark == 0x1D0);
    CHECK(systemTemplate->getGasCloudType() == 1);
    CHECK(systemTemplate->getBlurryVisionTime() == 1.38f);
    CHECK(systemTemplate->getBlurryVisionHoldTime() == 1.40f);
    CHECK(systemTemplate->getMinGasCloudRadius() == 1.48f);
    CHECK(systemTemplate->getMaxGasCloudRadius() == 1.50f);
    CHECK(systemTemplate->getGasCloudRadiusTime() == 1.58f);
    CHECK(systemTemplate->getGasCloudDamage() == 1.60f);

    void* instanceTable[0xB0 / 4] = {};
    RawView instanceObject = { instanceTable, 0 };
    instanceTable[0x18 / 4] = reinterpret_cast<void*>(&instancePause);
    instanceTable[0x8C / 4] = reinterpret_cast<void*>(&instanceTemplate);
    instanceTable[0xA0 / 4] = reinterpret_cast<void*>(&instancePosition);
    instanceTable[0xAC / 4] = reinterpret_cast<void*>(&instanceRadius);
    IParticleSystemInstance* const instance =
        reinterpret_cast<IParticleSystemInstance*>(&instanceObject);
    instance->setPaused(true);
    CHECK(instanceObject.mark == 18);
    CHECK(instance->getTemplate() == systemTemplate);
    CHECK(&instance->getPosition() == &expectedPosition);
    CHECK(instance->getGasCloudRadius() == 17.5f);
#else
    CHECK(false && "particle object view fixture must run as x86");
#endif
}

} // anonymous namespace

int main()
{
    static_assert(sizeof(void*) == 4, "ParticleSystemManager ABI fixture is x86-only");
    static_assert(sizeof(IParticleSystemManager) == 4,
                  "the manager interface must remain one primary vptr");
    static_assert(std::is_base_of<IBase, IParticleSystemManager>::value,
                  "the manager must retain the IBase quartet");
    static_assert(std::is_same<ParticleSystemInstanceList,
                               std::vector<dice::hfe::geom::IParticleSystemInstance*> >::value,
                  "the manager exposes the shared std::vector of instances (issue #7)");
    static_assert(sizeof(ParticleSystemSettings) == 60,
                  "the quality profile width is part of the recovered call ABI");

    testManagerVirtualOrder();
    testEvidenceScopedViews();
    std::printf("ParticleSystemManager ABI: %d/%d checks passed (%s)\n",
                checks - failures, checks,
#if !defined(BF2_IMAGE_RENDDX9)
                "engine"
#else
                "RendDX9 native"
#endif
    );
    return failures ? 1 : 0;
}
