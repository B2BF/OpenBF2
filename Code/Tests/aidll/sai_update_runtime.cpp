// [TEMP] [CHOSEN, not recovered] Exact production SAI::update with isolated callees.
// Real headers and native C++ containers/lifetimes; no production SAI construction,
// deny sweep, strategic planning or complete AI tick is claimed by this fixture.
// The separate deny fixture tests the production sweep. distributeArtillery is real.
#include "sai_update_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "MSVC x86 fixture only");
[[noreturn]] static void unexpected(const char* method)
{
    std::fprintf(stderr, "UNEXPECTED SAI collaborator: %s\n", method);
    std::fflush(stderr);
    std::abort();
}
static void require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}
static std::string calls, mode;
static void trace(const std::string& value) { if (!calls.empty()) calls += ','; calls += value; }
static int initialState = 0, initialBuffer = 0;
static bool initialSuppression, strategicResult = true, ownedResult = true;
static bool* suppression;
static int* state;
static int* buffer;
static float* timestamp;
static std::vector<SAI::BotInfo*>* bots;
static float stateArrays[4][44];
static float* wantedFirst = stateArrays[1];
static float* wantedSecond = stateArrays[3];
static bool mutateBuffer, mutateFlag, mutateTimer, mutateSettings, mutateTimestamp;
static bool replaceBot, appendBot;
static IBFTimer* alternateTimer;
static IAISettings* alternateSettings;
static SAI::BotInfo* replacementBot;
static SAI::BotInfo* appendedBot;
static std::vector<double> updateTimes;
static double gameTime = 10.0, increase = 0.25;
static float frequency = 1.0f;
static unsigned updates, increases;

static std::string at(const std::string& event) { return event + "@" + std::to_string(*state); }
static double observeUpdate(int identity)
{
    require(identity == 0, "cached timer must receive every update-time call");
    require(updates < updateTimes.size(), "unexpected extra update-time read");
    trace(at("t" + std::to_string(updates)));
    if (updates == 0 && mutateTimer) CommonSingletonServer::m_iBFTimer = alternateTimer;
    return updateTimes[updates++];
}
static double observeIncrease(int identity)
{
    require(identity == 0, "BotInfo uses cached timer");
    trace(at("dt" + std::to_string(increases)));
    if (increases++ == 0)
    {
        if (replaceBot) (*bots)[0] = replacementBot;
        if (appendBot) bots->push_back(appendedBot); // reserved capacity keeps iterators valid
    }
    return increase;
}
static double observeGame(int identity)
{
    require(identity == 0, "frequency gate uses cached timer");
    trace(at("game"));
    if (mutateSettings) CommonSingletonServer::m_iaiSettings = alternateSettings;
    if (mutateTimestamp) *timestamp = 9.0f;
    return gameTime;
}
static float observeFrequency(int identity)
{
    require(identity == 0, "PE retains settings pointer from before game-time callback");
    trace(at("frequency"));
    return frequency;
}

#include "sai_update_mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [TEMP] These lifetimes initialize only the real fields exercised by update.
// Native containers are genuinely constructed/destructed; no byte-offset access.
ISAI::~ISAI() {}
SAI::SAI(int side, int, int, Vec2, Vec2)
    : m_int18C(initialState), m_float190(0.0f), m_side(side),
      m_flag1D1(initialSuppression), m_stateA(stateArrays[0]), m_stateB(stateArrays[1]),
      m_stateC(stateArrays[2]), m_stateD(stateArrays[3]), m_int1F4(initialBuffer)
{
    state = &m_int18C; buffer = &m_int1F4; timestamp = &m_float190;
    suppression = &m_flag1D1; bots = &m_bots;
}
SAI::~SAI() {}
SAI::BotInfo::BotInfo(Bot* bot)
    : m_bot(bot), m_currentlyOrderedSquare(-1), m_lastOrderedSquare(-1), m_currentlyOccupiedSquare(-1), m_float10(0.0f), m_double14(0.0) {}
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}
IAISettings::IAISettings() {}
IAISettings::~IAISettings() {}
void* CommonSingletonServer::m_iBFTimer = nullptr;
void* CommonSingletonServer::m_iaiSettings = nullptr;

void SAI::updateStrengths() { trace(at("strengths")); }
void SAI::clearStates(float* first, float* second)
{
    trace(at("clear"));
    require(first == wantedFirst && second == wantedSecond, "clear uses boolean-negated selector and correct pair");
    require(m_int1F4 == initialBuffer, "clear does not publish next buffer");
}
void SAI::updateStates(float* first, float* second)
{
    trace(at("states"));
    require(first == wantedFirst && second == wantedSecond, "update uses boolean-negated selector and correct pair");
    require(m_int1F4 == initialBuffer, "updateStates sees old buffer selector");
    if (mutateBuffer) m_int1F4 = 7;
}
void SAI::prepareUpdate() { trace(at("prepare")); }
bool SAI::updateStrategicObjects() { trace(at("strategic")); return strategicResult; }
void SAI::chooseStrategy() { trace(at("choose")); if (mutateFlag) m_flag1D1 = true; }
void SAI::modifyStrategicObjectsTemperatures() { trace(at("temperatures")); }
void SAI::updateBotPositions() { trace(at("positions")); }
void SAI::updateDistribution() { trace(at("distribution")); }
void SAI::distributeResources() { trace(at("resources")); }
void SAI::distributeDefence() { trace(at("defence")); }
void SAI::distributeAttack() { trace(at("attack")); }
void SAI::distributeWithNoAreas() { trace(at("noareas")); }
} } }

class ObservedSAI : public SAI
{
public:
    ObservedSAI() : SAI(2, 0, 0, Vec2(), Vec2()) {}
    void updateRequestDenyTimers() override { trace(at("deny")); }
    void checkCommanderUseArtillery() override { trace(at("artillery")); }
    bool haveAnyOwnedCps(int side) override
    {
        require(side == 2, "owned-control-point query receives the SAI side unchanged");
        trace(at("owned")); return ownedResult;
    }
};

static void debugObserver(DebugType type, const std::string&, const std::string&,
                          int line, const std::string& expression, const std::string& text,
                          const std::string& fullFile, void*)
{
    require(type == DebugTypeError && line == 1044 && expression.empty(), "invalid state diagnostic metadata");
    require(fullFile == "AI/AIDLL/AI/AIStrategic/SAI.cpp", "invalid state diagnostic path");
    std::fprintf(stderr, "EXPECTED DEBUG %d %d %s\n", static_cast<int>(type), line, text.c_str());
    std::fflush(stderr); // real Debug::~Debug performs the abort after this callback returns
}
static void checkTrace(const std::string& expected)
{
    if (calls != expected)
    {
        std::fprintf(stderr, "FAIL trace\nexpected: %s\nactual:   %s\n", expected.c_str(), calls.c_str());
        std::exit(1);
    }
    require(updates == updateTimes.size(), "all expected timer reads consumed");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    require(argc == 2, "one fixture mode required");
    mode = argv[1];
    double budget = 1.0;
    int wantedState = 0, wantedBuffer = 0;
    std::string expected;
    updateTimes = {0.0, 100.0};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    bool invalid = mode.find("invalid_") == 0;
    if (mode.find("state_") == 0)
    {
        initialState = std::stoi(mode.substr(6));
        struct Stage { int state, next; const char* work; };
        const Stage stages[] = {
            {0,1,""}, {1,2,"strengths@1,"}, {2,3,"clear@2,"}, {3,4,"states@3,"},
            {4,6,"game@4,frequency@4,prepare@4,"}, {6,7,"strategic@6,artillery@6,"},
            {7,8,"choose@7,"}, {8,9,"temperatures@8,"}, {9,10,"positions@9,"},
            {10,11,"distribution@10,"}, {11,12,"resources@11,owned@11,"},
            {12,13,"defence@12,"}, {13,14,"attack@13,"}, {14,19,""},
            {15,19,"noareas@15,"}, {19,1,""}};
        bool found = false;
        for (const Stage& stage : stages) if (stage.state == initialState)
        {
            expected = "t0@" + std::to_string(initialState) + ",deny@" + std::to_string(initialState)
                + "," + stage.work + "t1@" + std::to_string(stage.next);
            wantedState = stage.next; found = true;
        }
        require(found, "known state case");
        wantedBuffer = initialState == 3 ? 1 : 0;
    }
    else if (mode == "budget_equal" || mode == "elapsed_nan")
    {
        updateTimes = {0.0, mode == "budget_equal" ? 0.95 : nan, 100.0};
        expected = "t0@0,deny@0,t1@1,strengths@1,t2@2"; wantedState = 2;
    }
    else if (mode == "budget_nan")
    {
        budget = nan; gameTime = 1.0;
        updateTimes = {0.0, 100.0, 100.0, 100.0, 100.0};
        expected = "t0@0,deny@0,t1@1,strengths@1,t2@2,clear@2,t3@3,states@3,t4@4,game@4,frequency@4";
        wantedState = 4; wantedBuffer = 1;
    }
    else if (mode == "frequency_equal" || mode == "frequency_nan" || mode == "game_nan")
    {
        initialState = 4; updateTimes = {0.0};
        if (mode == "frequency_equal") gameTime = 1.0;
        if (mode == "frequency_nan") frequency = std::numeric_limits<float>::quiet_NaN();
        if (mode == "game_nan") gameTime = nan;
        expected = "t0@4,deny@4,game@4,frequency@4"; wantedState = 4;
    }
    else if (mode == "strategic_false")
    {
        initialState = 6; strategicResult = false; updateTimes = {0.0};
        expected = "t0@6,deny@6,strategic@6"; wantedState = 6;
    }
    else if (mode == "owned_false")
    {
        initialState = 11; ownedResult = false; updateTimes = {0.0};
        expected = "t0@11,deny@11,resources@11,owned@11"; wantedState = 15;
    }
    else if (mode == "full_normal" || mode == "full_suppressed")
    {
        initialState = 19; initialSuppression = mode == "full_suppressed";
        updateTimes.assign(15, 0.0); updateTimes.back() = nan;
        expected = "t0@19,deny@19,t1@1,strengths@1,t2@2,clear@2,t3@3,states@3,t4@4,game@4,frequency@4,prepare@4,t5@6,";
        expected += initialSuppression ? "t6@7,t7@8," : "strategic@6,artillery@6,t6@7,choose@7,t7@8,";
        expected += "temperatures@8,t8@9,positions@9,t9@10,";
        expected += initialSuppression ? "t10@11,owned@11,t11@12,t12@13,t13@14,t14@19"
            : "distribution@10,t10@11,resources@11,owned@11,t11@12,defence@12,t12@13,attack@13,t13@14,t14@19";
        wantedState = 19; wantedBuffer = 1;
    }
    else if (mode == "flag_reread")
    {
        initialState = 7; mutateFlag = true; updateTimes.assign(8, 0.0);
        expected = "t0@7,deny@7,choose@7,t1@8,temperatures@8,t2@9,positions@9,t3@10,t4@11,owned@11,t5@12,t6@13,t7@14,t8@19";
        updateTimes.push_back(0.0); wantedState = 19;
    }
    else if (mode == "buffer_nonzero" || mode == "buffer_captured")
    {
        initialState = mode == "buffer_nonzero" ? 2 : 3;
        initialBuffer = mode == "buffer_nonzero" ? 7 : 0;
        wantedFirst = stateArrays[initialBuffer ? 0 : 1]; wantedSecond = stateArrays[initialBuffer ? 2 : 3];
        mutateBuffer = mode == "buffer_captured";
        expected = mode == "buffer_nonzero" ? "t0@2,deny@2,clear@2,t1@3" : "t0@3,deny@3,states@3,t1@4";
        wantedState = initialState + 1; wantedBuffer = mode == "buffer_nonzero" ? 7 : 1;
    }
    else if (mode == "cached_timer")
    {
        initialState = 4; mutateTimer = true;
        expected = "t0@4,deny@4,game@4,frequency@4,prepare@4,t1@6"; wantedState = 6;
    }
    else if (mode == "settings_captured" || mode == "timestamp_after_game")
    {
        initialState = 4; mutateSettings = mode == "settings_captured";
        mutateTimestamp = mode == "timestamp_after_game";
        if (mutateTimestamp)
        {
            updateTimes = {0.0}; expected = "t0@4,deny@4,game@4,frequency@4"; wantedState = 4;
        }
        else { expected = "t0@4,deny@4,game@4,frequency@4,prepare@4,t1@6"; wantedState = 6; }
    }
    else if (mode == "bot_pointer_reload" || mode == "bot_float_rounding" || mode == "bot_live_end")
    {
        budget = -1.0; // Even a negative budget does not bypass the prefix.
        updateTimes = {0.0, 0.0};
        replaceBot = mode == "bot_pointer_reload"; appendBot = mode == "bot_live_end";
        mutateTimer = true;
        increase = mode == "bot_float_rounding" ? std::ldexp(1.0, -24) + std::ldexp(1.0, -50) : 0.25;
        expected = appendBot ? "t0@0,deny@0,dt0@0,dt1@0,t1@1" : "t0@0,deny@0,dt0@0,t1@1";
        wantedState = 1;
    }
    else if (invalid)
    {
        initialState = std::stoi(mode.substr(8)); updateTimes = {0.0};
        setDebugCallback(DebugTypeError, debugObserver, nullptr);
    }
    else unexpected("unknown fixture mode");

    ObservedTimer primaryTimer, otherTimer;
    otherTimer.identity = 1; alternateTimer = &otherTimer;
    ObservedSettings primarySettings, otherSettings;
    otherSettings.identity = 1; alternateSettings = &otherSettings;
    CommonSingletonServer::m_iBFTimer = &primaryTimer;
    CommonSingletonServer::m_iaiSettings = &primarySettings;
    ObservedSAI subject;
    SAI::BotInfo original(nullptr), replacement(nullptr), appended(nullptr);
    replacementBot = &replacement; appendedBot = &appended;
    original.m_float10 = 1.0f; replacement.m_float10 = 2.0f; appended.m_float10 = 4.0f;
    if (mode.find("bot_") == 0)
    {
        bots->reserve(4); bots->push_back(&original); bots->push_back(nullptr);
    }
    subject.update(budget);
    if (invalid) { std::fprintf(stderr, "AFTER INVALID\n"); return 1; }
    checkTrace(expected);
    require(*state == wantedState, "stored next state");
    require(*buffer == wantedBuffer, "buffer publication and callback capture");
    if (replaceBot)
        require(original.m_float10 == 1.0f && replacement.m_float10 == 2.25f, "PE reloads BotInfo after callback");
    if (mode == "bot_float_rounding")
        require(original.m_float10 == std::nextafter(1.0f, 2.0f), "PE adds double interval before rounding to float");
    if (appendBot)
        require(original.m_float10 == 1.25f && appended.m_float10 == 4.25f && increases == 2,
                "current vector end is reread, null slot skipped and appended bot visited");
    std::printf("PASS %s\n", mode.c_str());
    return 0;
}
