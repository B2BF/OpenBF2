// [TEMP] [CHOSEN, not recovered] Includes the complete actual manager, constructors and draw
// helpers through the update fixture. Renderer/effect/device boundaries are isolated models.
// This does not instantiate concrete particles, render pixels or claim production map acceptance.
#define BF2_PSM_DRAW_FIXTURE
#define main particleUpdateFixtureMain
#include "particle_update_runtime.cpp"
#undef main
#include <csignal>

namespace {
using namespace dice::hfe::dx9rend;
std::vector<std::string> drawCalls;
void* rendererTable[0xDC / 4] = {};
void* rendererObject = rendererTable;
void* drawViewTable[0x9C / 4] = {};
void* primaryViewObject = drawViewTable;
void* alternateViewObject = drawViewTable;
void* waterViewObject = drawViewTable;
void* videoTable[0x3C / 4] = {};
void* videoObject = videoTable;
unsigned char raStorage[0x1E4] = {};
Mat4 waterMatrix = {};
float primaryFar = 2222.0f, alternateFar = 3333.0f;
float videoScale = 0.5f, expectedDistance = 250.0f;
int globalDistance = 500, waterReads = 0;
bool waterFirst = false, waterSecond = false;
// The per-frame draw counters inside the embedded renderers (retail manager +0x1E78/+0x1E7C,
// +0x8B14, +0x7C70/+0x7C74, +0x851C/+0x8520), addressed from each renderer span: the native
// manager's containers ahead of the spans are not retail-sized, the spans keep their layout.
const unsigned int kRendererCounters = 7;
unsigned int& rendererCounter(ParticleSystemManager* manager, unsigned int index)
{
    unsigned char* const spans[kRendererCounters] = {
        manager->m_particleSystemRenderer, manager->m_particleSystemRenderer,
        manager->m_nsaParticleSystemRenderer,
        manager->m_meshParticlesRenderer, manager->m_meshParticlesRenderer,
        manager->m_trailRenderer, manager->m_trailRenderer};
    const unsigned int offsets[kRendererCounters] = {0x1D70,0x1D74,0x51C,0x1D40,0x1D44,0x850,0x854};
    return *reinterpret_cast<unsigned int*>(spans[index] + offsets[index]);
}
float& ra(unsigned int offset) { return *reinterpret_cast<float*>(raStorage + offset); }
unsigned int& word(unsigned char* renderer, unsigned int offset)
{ return *reinterpret_cast<unsigned int*>(renderer + offset); }

void __fastcall forbidden(void*, void*) { std::abort(); }
IRenderView* __fastcall primaryRead(void* receiver, void*)
{
    CHECK(receiver == &rendererObject);
    for (unsigned int i = 0; i < kRendererCounters; ++i) CHECK(rendererCounter(currentManager, i) == 0);
    drawCalls.push_back("primary"); return reinterpret_cast<IRenderView*>(&primaryViewObject);
}
IRenderView* __fastcall alternateRead(void* receiver, void*)
{
    CHECK(receiver == &rendererObject && primaryFar == 2222.0f);
    // These flags are reset by selectors in the first flush, after its far-clip restore.
    CHECK(currentManager->m_particleSystemRenderer[0x1D78] == 0);
    CHECK(currentManager->m_meshParticlesRenderer[0x1D48] == 0);
    drawCalls.push_back("alternate"); return reinterpret_cast<IRenderView*>(&alternateViewObject);
}
IRenderView* __fastcall waterRead(void* receiver, void*)
{
    CHECK(receiver == &rendererObject);
    waterMatrix.m[13] = (waterReads++ == 0 ? waterFirst : waterSecond) ? -10.0f : 10.0f;
    drawCalls.push_back("water"); return reinterpret_cast<IRenderView*>(&waterViewObject);
}
Mat4* __fastcall drawMatrixRead(void* receiver, void*)
{ CHECK(receiver == &waterViewObject); drawCalls.push_back("waterMatrix"); return &waterMatrix; }
IVideoOptions* __fastcall videoRead(void* receiver, void*)
{ CHECK(receiver == &rendererObject); drawCalls.push_back("video"); return reinterpret_cast<IVideoOptions*>(&videoObject); }
float __fastcall scaleRead(void* receiver, void*)
{ CHECK(receiver == &videoObject); drawCalls.push_back("scale"); return videoScale; }
float __fastcall farRead(void* receiver, void*)
{
    CHECK(receiver == &primaryViewObject || receiver == &alternateViewObject);
    drawCalls.push_back(receiver == &primaryViewObject ? "farGet0" : "farGet1");
    return receiver == &primaryViewObject ? primaryFar : alternateFar;
}
void __fastcall farWrite(void* receiver, void*, float value)
{
    CHECK(receiver == &primaryViewObject || receiver == &alternateViewObject);
    float& target = receiver == &primaryViewObject ? primaryFar : alternateFar;
    const float original = receiver == &primaryViewObject ? 2222.0f : 3333.0f;
    CHECK(bits(value) == bits(expectedDistance) || bits(value) == bits(original));
    target = value;
    drawCalls.push_back(receiver == &primaryViewObject ? "farSet0" : "farSet1");
}
float __fastcall drawLightRead(void*, void*)
{
    CHECK(bits(currentManager->m_effectiveViewDistance) == bits(expectedDistance));
    drawCalls.push_back("light"); return 0.5f;
}
IBase* __fastcall constructorSingleton(void*, void*, const renddx9abi::HostString& name)
{
    CHECK(name == "WindManager");
    particle_renderer_fixture::constructionEvents.push_back("wind");
    return reinterpret_cast<IBase*>(0x71717171u);
}

void installDrawCollaborators()
{
    for (void*& p : rendererTable) p = reinterpret_cast<void*>(&forbidden);
    for (void*& p : drawViewTable) p = reinterpret_cast<void*>(&forbidden);
    for (void*& p : videoTable) p = reinterpret_cast<void*>(&forbidden);
    rendererTable[0x48/4] = reinterpret_cast<void*>(&waterRead);
    rendererTable[0x50/4] = reinterpret_cast<void*>(&primaryRead);
    rendererTable[0x74/4] = reinterpret_cast<void*>(&videoRead);
    rendererTable[0xD4/4] = reinterpret_cast<void*>(&alternateRead);
    drawViewTable[0x38/4] = reinterpret_cast<void*>(&farWrite);
    drawViewTable[0x3C/4] = reinterpret_cast<void*>(&farRead);
    drawViewTable[0x64/4] = reinterpret_cast<void*>(&drawMatrixRead);
    videoTable[0x28/4] = reinterpret_cast<void*>(&scaleRead);
    g_rendererBase = &rendererObject; g_raShaderConstants = raStorage; ra(0x1BC) = 0.0f;
}

void verifyConstructor(ParticleSystemManager& manager, unsigned short previousControl)
{
    using namespace particle_renderer_fixture;
    unsigned short afterControl;
    __asm { fnstcw afterControl }
    CHECK(afterControl == previousControl);
    const char* expectedNames[] = {
        "declaration","effect:mods/xpack/shaders/Particles",
        "parameter:ViewMat","parameter:ProjMat","parameter:HemiMapInfo","parameter:HemiShadowAltitude",
        "parameter:Texture0","parameter:Texture1","parameter:TemplateParameters","parameter:fogRange",
        "parameter:fogColor","parameter:WaterFogRange","parameter:WaterFogColor","parameter:CameraToWater",
        "parameter:EffectSunColor","parameter:EffectShadowColor",
        "effect:mods/xpack/shaders/Trail","declaration",
        "parameter:ViewMat","parameter:ProjMat","parameter:HemiMapInfo","parameter:HemiShadowAltitude",
        "parameter:Texture0","parameter:Texture1","parameter:EyePos","parameter:TemplateParameters",
        "parameter:FresnelOffset","parameter:fogRange","parameter:fogColor","parameter:EffectSunColor",
        "parameter:EffectShadowColor","listener",
        "declaration","effect:mods/xpack/shaders/NonScreenAlignedParticles",
        "parameter:ViewMat","parameter:ProjMat","parameter:HemiMapInfo","parameter:HemiShadowAltitude",
        "parameter:Texture0","parameter:Texture1","parameter:TemplateParameters","parameter:fogRange",
        "parameter:fogColor","parameter:EffectSunColor","parameter:EffectShadowColor","atlas","wind"};
    CHECK(constructionEvents == std::vector<std::string>(expectedNames,expectedNames + 47));
    CHECK(declarations.size() == 3 && listeners.size() == 1);
    CHECK(listeners[0] == reinterpret_cast<DeviceLostResetListener*>(manager.m_trailRenderer));
    CHECK(listenerPriorities[0] == 4);
    // Engine-heap blocks only: the primed pointer lists now take their storage from the shared CRT
    // through std::vector (checked below), and the atlas block is the native TextureAtlas.
    const unsigned int expectedAllocations[] = {0x460,0x800,0x460,0x800,
                                                static_cast<unsigned int>(sizeof(TextureAtlas))};
    CHECK(allocationSizes == std::vector<unsigned int>(expectedAllocations,expectedAllocations + 5));
    CHECK(declarations[0].size() == 9 && declarations[1].size() == 7 && declarations[2].size() == 9);
    const unsigned short ordinaryOffsets[] = {0,12,20,32,40,48,52,60};
    const unsigned short trailOffsets[] = {0,12,24,36,52,60};
    const unsigned short nsaOffsets[] = {0,12,20,32,44,52,60,68};
    const unsigned short* offsets[] = {ordinaryOffsets,trailOffsets,nsaOffsets};
    for (unsigned int kind = 0; kind < 3; ++kind) {
        for (unsigned int i = 0; i + 1 < declarations[kind].size(); ++i) {
            CHECK(declarations[kind][i].Stream == 0 && declarations[kind][i].Offset == offsets[kind][i]);
            CHECK(declarations[kind][i].Method == 0);
        }
        CHECK(declarations[kind].back().Stream == 0xFF && declarations[kind].back().Type == 17);
    }
    std::vector<unsigned char*>* vectorOwners[] = {
        &particleRendererList<unsigned char*>(manager.m_particleSystemRenderer, 8),
        &particleRendererList<unsigned char*>(manager.m_trailRenderer, 8),
        &particleRendererList<unsigned char*>(manager.m_nsaParticleSystemRenderer, 12)};
    for (std::vector<unsigned char*>* owner : vectorOwners) {
        // resize(30, nullptr) then resize(0): empty, capacity 30, the storage still zeroed.
        CHECK(owner->empty() && owner->capacity() == 30);
        const unsigned char* const storage = reinterpret_cast<const unsigned char*>(owner->data());
        for (unsigned int i = 0; i < 30 * sizeof(void*); ++i) CHECK(storage[i] == 0);
    }
    const unsigned int recordsOffsets[] = {0x1DC8,0x564}, scratchOffsets[] = {0x1DC0,0x55C};
    unsigned char* scratchOwners[] = {manager.m_particleSystemRenderer,manager.m_nsaParticleSystemRenderer};
    for (unsigned int kind = 0; kind < 2; ++kind) {
        const unsigned char* records = *reinterpret_cast<unsigned char**>(scratchOwners[kind]+recordsOffsets[kind]);
        const unsigned char* scratch = *reinterpret_cast<unsigned char**>(scratchOwners[kind]+scratchOffsets[kind]);
        CHECK(records[0] == 0 && records[0x45F] == 0);
        CHECK(scratch[0] == 0xA5 && scratch[0x7FF] == 0xA5);
    }
    const short* trig = reinterpret_cast<short*>(manager.m_particleSystemRenderer+0x1E14);
    CHECK(trig[0] == 0 && trig[1] == 32767);
    CHECK(trig[1024*2] > 32760 && trig[1024*2+1] == 0);
    CHECK(trig[2048*2] == 0 && trig[2048*2+1] < -32760);
    CHECK(trig[3072*2] < -32760 && trig[3072*2+1] == 0);
    CHECK(*reinterpret_cast<int*>(manager.m_particleSystemRenderer+4) == -1);
    CHECK(*reinterpret_cast<int*>(manager.m_meshParticlesRenderer) == -1);
    CHECK(*reinterpret_cast<int*>(manager.m_trailRenderer+4) == -1);
    CHECK(*reinterpret_cast<int*>(manager.m_nsaParticleSystemRenderer+8) == -1);
    CHECK(manager.m_particleSystemRenderer[0x1D9C] == 1 && manager.m_meshParticlesRenderer[0x1D6C] == 1);
    CHECK(manager.m_trailRenderer[0x884] == 1 && manager.m_nsaParticleSystemRenderer[0x544] == 1);
    // PE byte stores and the in-place lists do not blanket-initialize adjacent padding. A list's
    // std::vector fills the first twelve bytes of its 16-byte span; the fourth word stays as is.
    CHECK(manager.m_particleSystemRenderer[1] == 0xA5 && manager.m_particleSystemRenderer[0x14] == 0xA5);
    CHECK(manager.m_meshParticlesRenderer[0x1D3D] == 0xA5 && manager.m_trailRenderer[0x84D] == 0xA5);
    CHECK(manager.m_nsaParticleSystemRenderer[1] == 0xA5 && manager.m_nsaParticleSystemRenderer[0x18] == 0xA5);
}

void verifySelectorsAndEmptyExits()
{
    // Indexed loads occur before -1 tests in PE. Give each table a real preceding element.
    unsigned char ordinary[0x5E18] = {}, nsa[0x5A0] = {}, mesh[0x1D90] = {}, trail[0x914] = {};
    unsigned char entry[0xC0] = {}, lower[0xC0] = {};
    unsigned char* entries[] = {0,entry,lower};
    *reinterpret_cast<unsigned char***>(ordinary+0x18) = entries+1;
    *reinterpret_cast<unsigned char***>(ordinary+0x1C) = entries+1;
    *reinterpret_cast<unsigned char***>(nsa+0x1C) = entries+1;
    *reinterpret_cast<unsigned char***>(mesh+4) = entries+1;
    word(entry,0x14) = word(entry,0x94) = word(entry,0xBC) = 0xFFFFFFFFu;
    word(lower,0x14) = word(lower,0x94) = word(lower,0xBC) = 9;
    CHECK(psmSelectParticleRenderer(ordinary,0xFFFFFFFFu) && ordinary[0x1D78] == 1);
    CHECK(word(ordinary,0x1D7C) == 0xFFFFFFFFu);
    word(ordinary,4) = 1;
    CHECK(!psmSelectParticleRenderer(ordinary,10));
    word(ordinary,4) = 0xFFFFFFFFu;
    word(ordinary,0x1824) = word(ordinary,0x1828) = 1;
    word(ordinary,0x24) = 1; word(ordinary,0xC24) = 0;
    CHECK(psmSelectParticleRenderer(ordinary,10) && ordinary[0x1D78] == 0 && ordinary[0x1D79] == 1);
    word(ordinary,0x1824) = word(ordinary,0x1828) = 0;
    CHECK(!psmSelectParticleRenderer(ordinary,0));
    CHECK(psmSelectNsaParticleRenderer(nsa,0xFFFFFFFFu) && nsa[0x520] == 1);
    word(nsa,8) = 0xFFFFFFFFu;
    CHECK(psmSelectNsaParticleRenderer(nsa,0) && nsa[0x520] == 1);
    CHECK(!psmDrawNsaParticleRenderer(nsa,0) && nsa[0x520] == 1); // disabled, retains flag
    word(mesh,0x180C) = word(mesh,0x1810) = 1;
    word(mesh,0xC) = 1; word(mesh,0xC0C) = 0;
    CHECK(psmSelectMeshParticleRenderer(mesh,10) && mesh[0x1D48] == 0 && mesh[0x1D49] == 1);
    word(mesh,0x180C) = word(mesh,0x1810) = 0;
    CHECK(!psmSelectMeshParticleRenderer(mesh,10));
    word(trail,0x320) = 1;
    CHECK(psmSelectTrailRenderer(trail,91) && word(trail,0x85C) == 91);
    word(trail,0x320) = 0;
    CHECK(psmSelectTrailRenderer(trail,0) && trail[0x858] == 1);
    CHECK(!psmDrawTrailRenderer(trail,0) && trail[0x858] == 1);
    CHECK(!psmDrawParticleRenderer(ordinary,0));
    CHECK(!psmDrawMeshParticleRenderer(mesh,0,false) && !psmDrawMeshParticleExtras(mesh,true));
    ParticleSystemInstanceList empty = {};
    const unsigned char savedNsa = nsa[0x520], savedTrail = trail[0x858];
    psmPrepareNsaParticleRenderer(nsa,0,&empty,2.0f,false);
    psmPrepareTrailRenderer(trail,0,&empty,2.0f);
    CHECK(nsa[0x520] == savedNsa && trail[0x858] == savedTrail);
}

void expectedAbort(int)
{
    std::fputs("EXPECTED active particle frontier stop\n",stdout); std::fflush(stdout); std::_Exit(86);
}
void verifyTrailParticles(int fault);
int runNegative(const char* value)
{
    const int kind = std::atoi(value);
    unsigned char renderer[0x5E18] = {};
    IParticleSystemInstance* item = reinterpret_cast<IParticleSystemInstance*>(0xCCCCCCC1u);
    ParticleSystemInstanceList instances(1, item);
    std::signal(SIGABRT,&expectedAbort);
    switch (kind) {
    case 1: renderer[0x544]=1; psmPrepareNsaParticleRenderer(renderer,0,&instances,0.25f,false); break;
    // Mesh-renderer empty/disabled exits are covered by the positive checks.
    case 3: installDrawCollaborators(); verifyTrailParticles(3); break;
    case 5: renderer[0x544]=renderer[0x520]=1; psmDrawNsaParticleRenderer(renderer,0); break;
    case 7: installDrawCollaborators(); verifyTrailParticles(7); break;
    case 9:
        particle_renderer_fixture::allowConstruction = true;
        particle_renderer_fixture::failDeclaration = true;
        expectedConstructorFrontierSelf = renderer;
        psmConstructParticleRenderer(renderer); break;
    default: return 2;
    }
    return 3;
}

void verifyDraw(ParticleSystemManager& manager, bool firstPerson, bool secondPass,
                bool submergedFirst, bool submergedSecond)
{
    currentManager = &manager;
    waterFirst = submergedFirst; waterSecond = submergedSecond; waterReads = 0;
    drawCalls.clear(); ra(0x1E0) = 17.0f;
    const float previousDt = manager.m_deltaTime;
    for (unsigned int i = 0; i < kRendererCounters; ++i) rendererCounter(&manager, i) = 99;
    manager.m_particleSystemRenderer[0x1D78] = manager.m_particleSystemRenderer[0x1D79] = 9;
    manager.m_meshParticlesRenderer[0x1D48] = manager.m_meshParticlesRenderer[0x1D49] = 9;
    reinterpret_cast<IParticleSystemManager*>(&manager)->onPathDraw(firstPerson, secondPass);
    const char* first[] = {"primary","setting","video","scale","water","waterMatrix",
                          "light","light","farGet0","farSet0","farSet0"};
    std::vector<std::string> expected(first, first + 11);
    if (firstPerson) {
        const char* extra[] = {"alternate","water","waterMatrix","light","light","farGet1","farSet1","farSet1"};
        expected.insert(expected.end(), extra, extra + 8);
    }
    CHECK(drawCalls == expected);
    CHECK(waterReads == (firstPerson ? 2 : 1));
    CHECK(primaryFar == 2222.0f && alternateFar == 3333.0f);
    CHECK(bits(manager.m_effectiveViewDistance) == bits(expectedDistance));
    CHECK(bits(manager.m_deltaTime) == bits(previousDt) && manager.m_poolsInitialized == 1);
    CHECK(ra(0x1E0) == (firstPerson ? 17.0f : secondPass != submergedFirst ? -1.0f : 1.0f));
    for (unsigned int i = 0; i < kRendererCounters; ++i) CHECK(rendererCounter(&manager, i) == 0);
    CHECK(manager.m_particleSystemRenderer[0x1D78] == 0 && manager.m_particleSystemRenderer[0x1D79] == 0);
    CHECK(manager.m_meshParticlesRenderer[0x1D48] == 0 && manager.m_meshParticlesRenderer[0x1D49] == 0);
    CHECK(manager.m_instances.empty());
    CHECK(manager.m_pendingClassification.empty());
    CHECK(manager.m_pendingDestroy.empty());
    for (unsigned int i = 0; i < 10; ++i)
        CHECK(manager.m_renderBuckets[i].empty());
    CHECK(psmReclassification(&manager).empty());
}

LightManager* replacementLight;
int firstLightCalls=0, secondLightCalls=0, refreshCalls=0;
float __fastcall replacingLight(void*,void*)
{
    ++firstLightCalls;
    currentManager->m_cachedLightValue=8.0f;
    g_LightManager=replacementLight;
    return 5.0f;
}
float __fastcall replacementLightRead(void*,void*) { ++secondLightCalls; return 9.0f; }
void __fastcall templateRefresh(void*,void*) { ++refreshCalls; }

void verifyPrecisionAndCallbacks(ParticleSystemManager& manager)
{
    unsigned short originalControl;
    __asm { fnstcw originalControl }
    for (unsigned int pc=0x200;pc<=0x300;pc+=0x100) {
        unsigned short control=static_cast<unsigned short>((originalControl & ~0x300u) | pc);
        __asm { fldcw control }
        expectedDistance=manager.m_effectiveViewDistance=200.0f;
        manager.m_lodScales[0]=0.1f;
        manager.m_nearRatio=0.3f;
        psmRecomputeDerivedSettings(&manager);
        CHECK(bits(manager.m_nearDistances[0])==0x40C00001u); // 6.000000476837158f
        manager.m_nearRatio=0.9f;
        psmRecomputeDerivedSettings(&manager);
        CHECK(bits(manager.m_inverseNearRanges[0])==0x3EFFFFFEu); // .4999999403953552f
        globalDistance=500; videoScale=1.0001720190048218f;
        manager.m_distanceFactor=1.5254164934158325f;
        expectedDistance=pc==0x300 ? 762.83941650390625f : 762.8394775390625f;
        verifyDraw(manager,false,false,false,false);
        unsigned short after;
        __asm { fnstcw after }
        CHECK(after==control);
    }
    __asm { fldcw originalControl }

    LightManager* savedLight=g_LightManager;
    void* firstTable[0x274/4]={}; void* secondTable[0x274/4]={};
    firstTable[0x270/4]=reinterpret_cast<void*>(&replacingLight);
    secondTable[0x270/4]=reinterpret_cast<void*>(&replacementLightRead);
    void* firstObject=firstTable; void* secondObject=secondTable;
    replacementLight=reinterpret_cast<LightManager*>(&secondObject);
    void* templateTable[3]={0,0,reinterpret_cast<void*>(&templateRefresh)};
    void* templateObject=templateTable;
    g_templatesByName["testCallback"] = reinterpret_cast<IParticleSystemTemplate*>(&templateObject);
    currentManager=&manager; manager.m_cachedLightValue=5.0f;
    g_LightManager=reinterpret_cast<LightManager*>(&firstObject);
    psmRecomputeDerivedSettings(&manager);
    CHECK(firstLightCalls==1 && secondLightCalls==1);
    CHECK(manager.m_cachedLightValue==9.0f && refreshCalls==0);
    CHECK(g_LightManager==replacementLight);
    g_templatesByName.clear(); g_LightManager=savedLight;
}
}

namespace dice { namespace hfe { namespace dx9rend {
int readIntSetting(const char* name, int fallback)
{ CHECK(std::strcmp(name,"GSViewDistance") == 0 && fallback == 500); drawCalls.push_back("setting"); return globalDistance; }
IVideoOptions* rendererGetVideoOptions() { std::abort(); }
IRenderView* rendererGetRenderViewDup()
{ observe("view"); return reinterpret_cast<IRenderView*>(&viewObject); }
TextureAtlas* textureAtlasCtor(void* storage)
{
    particle_renderer_fixture::requireConstruction();
    particle_renderer_fixture::constructionEvents.push_back("atlas");
    return static_cast<TextureAtlas*>(storage);
}
} } }

#include "particle_active_draw_fixture.inl"
#include "particle_nsa_draw_fixture.inl"
#include "particle_trail_draw_fixture.inl"

int main(int argc, char** arguments)
{
    if (argc == 3 && std::strcmp(arguments[1],"active") == 0) return runNegative(arguments[2]);
    // Retain the previous actual pre-load -> update regression in this combined fixture.
    char arg0[] = "particle_frame_runtime"; char* argv[] = {arg0,0};
    if (particleUpdateFixtureMain(1,argv)) return 1;
    installDrawCollaborators();
    void* lightTable[0x274/4] = {};
    lightTable[0x270/4] = reinterpret_cast<void*>(&drawLightRead);
    void* lightObject = lightTable;
    g_LightManager = reinterpret_cast<LightManager*>(&lightObject);
    void* classTable[0x4C/4] = {};
    classTable[0x48/4] = reinterpret_cast<void*>(&constructorSingleton);
    void* classObject = classTable; classManager = reinterpret_cast<IClassManager*>(&classObject);

    ParticleSystemManager manager = {};
    std::memset(manager.m_particleSystemRenderer,0xA5,sizeof(manager.m_particleSystemRenderer));
    std::memset(manager.m_meshParticlesRenderer,0xA5,sizeof(manager.m_meshParticlesRenderer));
    std::memset(manager.m_trailRenderer,0xA5,sizeof(manager.m_trailRenderer));
    std::memset(manager.m_nsaParticleSystemRenderer,0xA5,sizeof(manager.m_nsaParticleSystemRenderer));
    currentManager = &manager;
    particle_renderer_fixture::allowConstruction = true;
    unsigned short controlBefore;
    __asm { fnstcw controlBefore }
    constructParticleSystemManager(&manager);
    verifyConstructor(manager,controlBefore);
    CHECK(manager.m_poolsInitialized == 0);
    static ManagerImage before;
    capture(before, manager);
    drawCalls.clear();
    reinterpret_cast<IParticleSystemManager*>(&manager)->onPathDraw(true,true);
    CHECK(drawCalls.empty() && std::memcmp(&manager,before.bytes,sizeof(before.bytes)) == 0);

    manager.m_settingsApplied = 1;
    manager.m_type0PoolCount = manager.m_type1PoolCount = manager.m_type2PoolCount = manager.m_type4PoolCount = 2;
    manager.m_effectiveViewDistance = expectedDistance;
    particleSystemManagerPreLoadLevel(reinterpret_cast<IParticleSystemManager*>(&manager));
    CHECK(manager.m_poolsInitialized == 1);
    // The registered listener reaches actual TrailRenderer reset and fills all three real
    // fixture index buffers. Lost/destroy are shipped nullsubs and leave the storage untouched.
    for (unsigned int i=3;i<6;++i) std::fill(buffers[i]->indices.begin(),buffers[i]->indices.end(),0xEEEE);
    particle_renderer_fixture::listeners[0]->onDeviceLost();
    CHECK(buffers[3]->indices[0] == 0xEEEE);
    particle_renderer_fixture::listeners[0]->onDeviceReset();
    CHECK(buffers[3]->indices[0] == 0 && buffers[4]->indices[0] == 0 && buffers[5]->indices[0] == 0);
    particle_renderer_fixture::listeners[0]->onDeviceDestroy();
    CHECK(buffers[3]->indices[0] == 0);
    for (int second = 0; second < 2; ++second)
        for (int under = 0; under < 2; ++under)
            verifyDraw(manager,false,second != 0,under != 0,false);
    verifyDraw(manager,true,false,false,true);
    verifyDraw(manager,true,true,true,false);
    globalDistance = 100; expectedDistance = 200.0f;
    verifyDraw(manager,false,false,false,false);
    globalDistance = 400;
    verifyDraw(manager,false,false,false,false);
    globalDistance = -100;
    verifyDraw(manager,false,false,false,false);
    globalDistance = 500; videoScale = std::numeric_limits<float>::quiet_NaN();
    verifyDraw(manager,false,false,false,false);
    videoScale = std::numeric_limits<float>::infinity(); expectedDistance = videoScale;
    verifyDraw(manager,false,false,false,false);
    videoScale = 0.75f; globalDistance = 800; manager.m_distanceFactor = 0.75f; expectedDistance = 450.0f;
    verifyDraw(manager,false,false,false,false);
    verifySelectorsAndEmptyExits();
    verifyPrecisionAndCallbacks(manager);
    verifyActiveParticles();
    verifyNsaParticles();
    verifyTrailParticles(0);
    cleanup();
    std::printf("Particle draw and construction: %d/%d checks passed\n", checks-failures,checks);
    return failures ? 1 : 0;
}
