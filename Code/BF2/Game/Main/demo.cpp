// Demo runtime.
// Demo playback, capture and timing overrides. Timedemos skip ten warm-up frames;
// the HUD frame-count latch remains set after its first event. Capture can override
// clock advancement and simulation rate. Excess calculated ticks clamp to one.

#include "Demo.h"
#include "BF2Console.h"                     // setConsoleActive / outputToConsole
#include "../IGame.h"                       // IGame, game (0x8AD45A0)
#include "../IGameClient.h"
#include "../Utility/Vars.h"
#include "../DemoRecorder/DemoConfig.h"
#include "../DemoRecorder/DemoRecorder.h"
#include "../../Core/System.h"              // g_system, getWallClockTime (vptr+0x1C), advanceExactTime(uint)
#include "../../Core/StringUtil.h"          // getFixPathDelimiters, extractFileExt
#include "../../IO/TickCalculator.h"
#include "../../Scene/Object/PlayerManager.h"  // the real PlayerManager - replaces the local shim
#include "../../IO/File/FileManager.h"      // io::fileManager (0x8AC8C80), open(...); IFileSystem enums
#include "../../Core/IProfilerClient.h"     // profilerClient (ELF 0x8ABBCA8 / PE 0x9A5C6C) +0x2C pauseVTune
#include <sstream>                          // proven: __ioinit for this TU at 0x8AACC3D
#include <list>
#include <string>

namespace dice {
namespace hfe {

// Shared world-clock declaration; retain the namespace form used by its definition.
namespace WorldPref { int getTickCount(); }                 // direct call, no args

// The TU-local PlayerManagerLike shim that used to be here is gone - the real header
// is included above and all four sites now agree on the type. getLocalHumanPlayer is vptr+0x30 and
// returns IPlayer*, not the [CHOSEN] void* the shim guessed; it is used only as a boolean here
// (null on a dedicated server), so the call site is unaffected.
namespace world {
extern PlayerManager* playerManager;   // ds:0x8B1B928
}

// [INFERRED] Timedemo statistics. Original i386 layout is 0x30 bytes with
// four-byte-aligned doubles.
struct TimeDemoStats
{
    int               frameCount;       // +0x00  starts at -10: ten warm-up frames
    double            totalTime;        // +0x04
    double            shortestTime;     // +0x0C  init DBL_MAX
    int               shortestFrame;    // +0x14
    double            longestTime;      // +0x18  init -1.0
    int               longestFrame;     // +0x20
    std::list<double> frameTimes;       // +0x24  one pointer (GCC 3.x)
    double            frameStartTime;   // +0x28
};

// TU statics - .bss order matches declaration order (0x8AACC38, 0x8AACC3C, then __ioinit 0x8AACC3D).
namespace {
    // attested name: _ZN35_GLOBAL__N_Game_Main_demo.cppBjtJ5a18g_demo_useSlomoCamE @ 0x8AACC38.
    // A pointer to the console variable's float, resolved once at static-init.
    float* g_demo_useSlomoCam = Vars::getFloat("demo-slomo-cam", 0);
}
// Plain file-static (its local symbol was stripped; the anonymous-namespace one above survived
// because those are global symbols). [INFERRED name].  never cleared - see the file header.
static bool g_hudFrameSeen = false;                                     // byte @ 0x8AACC3C


Demo::Demo()
{
    tickCalculator = 0;
    demoRecorder   = 0;
    timeDemoStats  = 0;
    isPlayback     = false;
}

Demo::~Demo()                                   //  (D2) / 0x81415D0 (D1); no D0 - non-virtual
{
    killDemoRecorder();                         // tail-jmp at 0x81415C0 (D2) / 0x81415E0 (D1)
}

bool Demo::playDemo(IGameClient* client, const DemoConfig& config)
{
    killDemoRecorder();
    tickCalculator = new TickCalculator(10, 30.0f);         // operator new(0x20), 0x8141608..1B
    demoRecorder   = new DemoRecorder(config);              // operator new(0x88), 0x814162B..
    if (!demoRecorder)                                      // the binary tests it; kept
        return false;
    if (!demoRecorder->renderMode)                          // byte +0x00
    {
        killDemoRecorder();
        return false;
    }
    if (demoRecorder->demoConfig.timeDemo)                  // config+0x2D
    {
        timeDemoStats = new TimeDemoStats;                  // operator new(0x30); init inlined in binary:
        timeDemoStats->frameCount     = -10;                //   TEN warm-up frames
        timeDemoStats->totalTime      = 0.0;
        timeDemoStats->shortestTime   = 1.7976931348623157e308;     // DBL_MAX
        timeDemoStats->shortestFrame  = 0;
        timeDemoStats->longestTime    = -1.0;               //   dwords {0, 0xBFF00000}
        timeDemoStats->longestFrame   = 0;
        timeDemoStats->frameStartTime = 0.0;
    }
    isPlayback = true;                                      // +0x10 - only writer
    // Hand playback to the game client.
    return client->playDemo(demoRecorder);
}

bool Demo::playDemo(IGameClient* client, const std::string& configFile)
{
    DemoConfig cfg;
    if (!loadDemoConfig(&cfg, configFile))
        return false;
    return playDemo(client, cfg);
}

void Demo::recordDemo(IGame* game_, const std::string& name, BF2Console* console)
{
    if (demoRecorder)
    {
        console->outputToConsole("already recording\n");    // verbatim incl. '\n'
        return;
    }
    DemoConfig cfg;
    cfg.demoMode = DemoConfig::RECORD;                      // +0x00, writes 1
    cfg.demoFile = getFixPathDelimiters(name, '/');         // +0x08, delimiter literal 47
    std::string base = cfg.demoFile;
    if (extractFileExt(name).empty())                       // ext of the original argument
    {
        base = "Demos/" + cfg.demoFile;                     // capital D, verbatim
        cfg.demoFile = base + ".bf2demo";
    }
    if (world::playerManager->getLocalHumanPlayer())        // vptr+0x30 (0x814192E); null on a ded. server
    {
        cfg.cameraFile = base + "-def.bf2cam";              // +0x0C
        cfg.cameraMode = DemoConfig::CAMERA_RECORD;         // +0x04, writes 1 - record the camera too
    }
    tickCalculator = new TickCalculator(10, 30.0f);
    demoRecorder   = new DemoRecorder(cfg);
    if (!demoRecorder)
        return;
    if (!demoRecorder->renderMode)
    {
        killDemoRecorder();
        return;
    }
    game_->recordDemo(demoRecorder);                        // vptr+0xD8 (0x8141A10), attested slot
    // note: isPlayback stays false and timeDemoStats stays null - recording never timedemos.
}

void Demo::stop(IGame* game_)
{
    if (!demoRecorder)
        return;
    if (!isPlayback)                                        // recordings only
    {
        game_->stopRecording();                             // vptr+0xDC (0x8141C2F), attested slot
        demoRecorder->writeEndOfStream(WorldPref::getTickCount());
    }
    killDemoRecorder();                                     // tail-called in the binary
}

void Demo::addPlayerInput(const io::PlayerInput& input)
{
    if (demoRecorder)
        demoRecorder->addPlayerInput(input);                // tail call
}

void Demo::beginMainLoop(BF2Render* render, BF2Console* console)
{
    if (!demoRecorder)
        return;
    if (demoRecorder->startRenderMovie())
    {
        // [UNRESOLVED] Movie-target start/stop interface remains omitted here; the two
        // operations take DemoConfig and bracket demo rendering.
        (void)render;
        console->setConsoleActive(false);
    }
    if (!demoRecorder || !timeDemoStats)                    // re-tests, 0x8141CAC/0x8141CB6
        return;
    timeDemoStats->frameStartTime = g_system->getWallClockTime();
    int stopFrame = demoRecorder->demoConfig.vtuneFrame;    // config+0x34, 0x8141CD3
    if (stopFrame < 0)                                      // signed (js), 0x8141CD8
        return;
    if (timeDemoStats->frameCount != stopFrame)
        return;
    if (demoRecorder->demoConfig.vtuneRendering)            //   config+0x38
        demoRecorder->byte84 = true;                        //   [UNRESOLVED meaning - NOT quit();
                                                            //   endMainLoop clears it under the same test]
    else
        profilerClient->pauseVTune(false);                  // 0x8141CF0: profilerClient +0x2C(false)
}

bool Demo::endMainLoop(BF2Render* render, bool quit)        // return [CHOSEN bool]
{
    if (!demoRecorder)
        return false;
    if (timeDemoStats)
    {
        int stopFrame = demoRecorder->demoConfig.vtuneFrame;
        if (stopFrame >= 0 && timeDemoStats->frameCount == stopFrame)   // /7E
        {
            if (demoRecorder->demoConfig.vtuneRendering)    //   config+0x38
                demoRecorder->byte84 = false;               // the begin/end asymmetry, literal
            else
                profilerClient->pauseVTune(false);          // 0x8142EB3: the same slot, false again
        }
        if (timeDemoStats->frameCount >= 0)                 // warm-up frames not recorded
        {
            double ft = g_system->getWallClockTime() - timeDemoStats->frameStartTime;   // /DA0
            timeDemoStats->totalTime += ft;
            if (timeDemoStats->shortestTime > ft)           //  (fucomip/jbe)
            {
                timeDemoStats->shortestTime  = ft;
                timeDemoStats->shortestFrame = timeDemoStats->frameCount;
            }
            if (ft > timeDemoStats->longestTime)
            {
                timeDemoStats->longestTime  = ft;
                timeDemoStats->longestFrame = timeDemoStats->frameCount;
            }
            timeDemoStats->frameTimes.insert(timeDemoStats->frameTimes.end(), ft);
        }
        if (g_hudFrameSeen)                                 // and never cleared
            ++timeDemoStats->frameCount;
    }
    if (demoRecorder->quit())                               // camera-track EOF
    {
        // [UNRESOLVED] movie-target stop, the +0x64 pair of beginMainLoop's +0x60 (0x8141E4C):
        //     if (render && render->movieTarget) render->movieTarget->stopMovie();
        (void)render;
        quit = true;
    }
    if (quit)
    {
        if (demoRecorder && timeDemoStats)                  // ..6B
        {
            g_system->getWallClockTime();                   // result discarded (fstp st); kept
                                                            //   verbatim (the call may not be side-effect free)
            double avg = timeDemoStats->totalTime / timeDemoStats->frameCount;  //  fild/fdivr

            std::ostringstream report;                      // the ios/stringbuf storm in the binary,
                                                            // written as the C++ that produces it
            report << std::endl;                            // leading blank line - first op is endl
            report << "------------------------------------------------------------------------------" << std::endl;
            report << "T I M E D E M O - R E P O R T" << std::endl;         // verbatim, 0x8893286
            report << "------------------------------------------------------------------------------" << std::endl;
            report << "Demo: "   << demoRecorder->demoConfig.demoFile   << std::endl;
            report << "Camera: " << demoRecorder->demoConfig.cameraFile << std::endl;
            report << "Target Frame Rate: " << demoRecorder->demoConfig.renderFps << std::endl;
            report << "Resolution: " << demoRecorder->demoConfig.renderWidth
                   << "x"            << demoRecorder->demoConfig.renderHeight << std::endl;
            report << "Field of View: " << demoRecorder->demoConfig.renderFieldOfView << std::endl; // float, 0x814227D
            report << "-" << std::endl;
            report << "Total Frame Time: " << timeDemoStats->totalTime << " seconds" << std::endl;
            report << "Number of Frames: " << timeDemoStats->frameCount << std::endl;
            report << "Average Frame Rate: " << 1.0 / avg << std::endl;     // dbl 1.0
            report << "Average Frame Time: " << avg << std::endl;
            report << "Shortest Frame Time: " << timeDemoStats->shortestTime
                   << " (frame #" << timeDemoStats->shortestFrame << ")" << std::endl;
            report << "Longest Frame Time: " << timeDemoStats->longestTime
                   << " (frame #" << timeDemoStats->longestFrame << ")" << std::endl;
            report << "------------------------------------------------------------------------------" << std::endl;

            if (io::IStream* f = openFileForWriting("timedemo_report.txt"))
            {
                std::string s = report.str();
                f->write(s.c_str(), (unsigned int)s.length()); // vptr+0x14, attested
                f->release();                                  // vptr+0x08, attested
            }
            if (io::IStream* f = openFileForWriting("timedemo_frametimes.csv"))
            {
                {
                    std::ostringstream head;
                    head << "frameId;frameTime;frameRate;" << std::endl;    // verbatim, trailing ';'
                    std::string s = head.str();
                    f->write(s.c_str(), (unsigned int)s.length());
                }
                int frameId = 0;
                for (std::list<double>::iterator it = timeDemoStats->frameTimes.begin();
                     it != timeDemoStats->frameTimes.end(); ++it)
                {
                    std::ostringstream line;                // fresh stream per row - in the binary
                    line << frameId++ << ";" << *it << ";" << 1.0 / *it;
                    line << std::endl;
                    std::string s = line.str();
                    f->write(s.c_str(), (unsigned int)s.length());
                }
                f->release();
            }
        }
        killDemoRecorder();
    }
    return quit;
}

void Demo::visualUpdate(float a, float b, float c, int useSlomo)
{
    if (demoRecorder
        && (demoRecorder->isRenderingToFile()
            || (demoRecorder && demoRecorder->isOpenForPlayback())))    // duplicate test is in the binary
    {
        if (*g_demo_useSlomoCam <= 0.0f || !useSlomo)
            a = b;
        demoRecorder->demoVisualUpdate(a, c);
    }
}

void Demo::updateTicks(float* timeStep, int* tickCount, int* playbackTicks, float* playbackStep,
                       const TickCalculator* engineCalc)
{
    if (!demoRecorder)
        return;
    bool renderingSound = demoRecorder->isRenderingSound(); // before the clock read
    double now = g_system->getWallClockTime();
    int ownTicks = tickCalculator->compute(now);            // the demo'S calculator
    if (renderingSound)
    {
        *tickCount = 1;
        *timeStep  = 0.033333335f;                          // bit-exact 0x3D088889
    }
    int t = *tickCount;                                     // in-out from the engine
    if (t > engineCalc->maxTicksPerFrame)                   // the engine calc's +0x04
    {
        *tickCount = 1;                                     // clamp to ONE, not to max
        t = 1;
    }
    demoRecorder->updateTicks(t);
    if (demoRecorder->isOpenForPlayback())
    {
        *playbackTicks = ownTicks;
        *playbackStep  = tickCalculator->tickLength;
    }
}

void Demo::handlePlayerInput(int simulationRate)
{
    if (!demoRecorder)
        return;
    if (demoRecorder->isRenderingToFile()
        || demoRecorder->isRenderingSound()
        || demoRecorder->isOpenForPlayback())
    {
        float t = tickCalculator->tickLength;
        if (*g_demo_useSlomoCam > 0.0f && simulationRate > 0)   // strict >, float deref; 0x814306B/8C
            t = t * (float(simulationRate) / 100.0f);       // ..A0
        demoRecorder->handlePlayerInput(t);                 // tail call, 0x8143085
    }
}

bool Demo::allowRendering() const
{
    return !demoRecorder || !demoRecorder->isRenderingSound();
}

bool Demo::allowLockFps() const
{
    return !demoRecorder
        || (!demoRecorder->isRenderingToFile()
            && !demoRecorder->demoConfig.timeDemo);         // config+0x2D, 0x8143132
}

double Demo::overridePreferredFrameTime(double preferred)   // double return proven
{
    if (!demoRecorder || !demoRecorder->isRenderingSound())
        return preferred;                                   // unchanged
    game->setSimulationRate(100);                           // vptr+0xE4 (0x814317F), attested slot
    return 0.03333333333333333;                             // double 1/30 (dbl_8892F10)
}

bool Demo::overrideAdvanceTime()
{
    if (!demoRecorder)
        return false;
    if (demoRecorder->isRenderingToFile()
        || demoRecorder->demoConfig.timeDemo)               // recorder+0x31 = config+0x2D, 0x81431C1
    {
        g_system->advanceExactTime(1000000u / (unsigned)demoRecorder->demoConfig.renderFps);
                                                            // /CE - unsigned div, microseconds
        return true;
    }
    if (demoRecorder->isRenderingSound())
    {
        g_system->advanceExactTime(0x8235u);                // 33333 us
        return true;
    }
    return false;
}

void Demo::killDemoRecorder()
{
    if (demoRecorder)
        delete demoRecorder;                                // ~DemoRecorder + operator delete
    demoRecorder = 0;
    delete timeDemoStats;                                   // inlined list clear + operator delete in binary
    timeDemoStats = 0;
    delete tickCalculator;                                  // the binary emits a bare operator delete
    tickCalculator = 0;                                     //   (no dtor - TickCalculator is trivial -
                                                            //   and no null test); `delete` of the now
                                                            //   complete trivial type emits the same
    isPlayback = false;
}

io::IStream* Demo::openFileForWriting(const char* name)
{
    // io::fileManager (0x8AC8C80), virtual open; raw args observed: (string, 0, 2, 0).
    // [CHOSEN] the enum spellings - IFileSystem.h leaves the enumerator names unresolved.
    return io::fileManager->open(std::string(name),
                                 (io::IFileSystem::FileMode)0,
                                 (io::IFileSystem::FileAccess)2, 0);
}

void Demo::handleEvent(EventCategory category, unsigned int id, EventNode*, void*)
{
    if (category == (EventCategory)2 && id == 8)            // ECHud == 2 (BF2Engine.cpp); id 8 [UNRESOLVED name]
        g_hudFrameSeen = true;                              // never cleared, see file header
}

// Both frame-loop call sites guard demoRecorder. startRenderMovie is implemented, while
// DemoRecorder::quit still diagnoses and aborts at the unrecovered client-playback boundary.

} // namespace hfe
} // namespace dice
