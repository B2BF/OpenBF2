// [TEMP] Test scaffolding - not recovered source. The FileStream and TestFileManager below model
// only plain-file access for this check.
//
// Checks the DemoRecorder playback read path through the production constructor, with no game
// content, network or renderer, and the DemoSpectatorCamera forwarders on the paths that need no
// world.
//
// usage: bf2_demo_playback_read            synthetic demo written with ZStream, checked exactly
//        bf2_demo_playback_read <file>     reads a real .bf2demo and prints its frame statistics

#include "../BF2/Game/DemoRecorder/DemoRecorder.h"
#include "../BF2/Game/DemoRecorder/DemoHeader.h"
#include "../BF2/Game/DemoRecorder/DemoSpectatorCamera.h"
#include "../BF2/Core/System.h"
#include "../BF2/IO/BitStream.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/IO/Stream/IStream.h"
#include "../BF2/IO/Stream/ZStream.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace dice::hfe;

// The engine global the read branch opens through. Globals.cpp is not part of this check.
namespace dice { namespace hfe { namespace io { FileManager* fileManager = 0; } } }

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok)
    {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// A minimal IStream over stdio. Reads report one available byte until the file is exhausted.
class FileStream : public io::IStream
{
public:
    explicit FileStream(std::FILE* f) : m_ref(1), m_file(f) {}
    void addRef() { ++m_ref; }
    int getRef() { return m_ref; }
    int release()
    {
        const int r = --m_ref;
        if (!r)
        {
            std::fclose(m_file);
            delete this;
        }
        return r;
    }
    void* queryInterface(unsigned int) const { return 0; }
    unsigned int read(void* b, unsigned int n) { return (unsigned int)std::fread(b, 1, n, m_file); }
    unsigned int write(const void* b, unsigned int n) { return (unsigned int)std::fwrite(b, 1, n, m_file); }
    int skip(unsigned int n) { return std::fseek(m_file, (long)n, SEEK_CUR) == 0; }
    int getAvailableBytes()
    {
        const int c = std::fgetc(m_file);
        if (c == EOF)
            return 0;
        std::ungetc(c, m_file);
        return 1;
    }
    io::IStream* clone() { return 0; }
    bool canRead() { return true; }
    bool canWrite() { return true; }

private:
    int m_ref;
    std::FILE* m_file;
};

// FileManager with the two calls playback setup makes redirected to plain files.
class TestFileManager : public io::FileManager
{
public:
    io::IStream* open(const std::string& name, io::FileMode mode, io::FileAccess access, unsigned int)
    {
        const bool reading = mode == io::FILEMODE_READ && access == 0;
        std::FILE* f = std::fopen(name.c_str(), reading ? "rb" : "wb");
        ++opens;
        return f ? new FileStream(f) : 0;
    }
    bool fileExists(const std::string& name)
    {
        std::FILE* f = std::fopen(name.c_str(), "rb");
        if (f)
            std::fclose(f);
        return f != 0;
    }
    int opens = 0;
};

struct Frame
{
    int tick;
    std::vector<unsigned char> data;
};

void writeInt(io::IStream* s, int v) { s->write(&v, 4); }

// Header + frames + one special packet + end marker, in the recorder's on-disk layout.
void writeSyntheticDemo(const std::string& path, const std::vector<Frame>& frames)
{
    std::FILE* f = std::fopen(path.c_str(), "wb");
    io::IStream* file = new FileStream(f);
    io::ZStream* z = new io::ZStream(file, io::ZStream::ZSTREAMMODE_WRITE);
    file->release();

    DemoHeader header;
    header.version = 3153;
    header.serverName = "check server";
    header.dateTime = "2026-10-21 12:00:00";
    header.mapName = "strike_at_karkand";
    header.writeTo(z);

    for (size_t i = 0; i < frames.size(); ++i)
    {
        if (i == 2)
        {
            writeInt(z, -2);                       // special packet marker
            writeInt(z, 0);                        // DemoRecorderSpecialPacket_InitialState
        }
        writeInt(z, frames[i].tick);
        writeInt(z, (int)frames[i].data.size());
        z->write(&frames[i].data[0], (unsigned int)frames[i].data.size());
    }
    writeInt(z, frames.back().tick);               // end of stream: a zero-length frame
    writeInt(z, 0);
    z->release();
}

std::string writeConfig(const std::string& demoPath)
{
    const std::string conPath = demoPath + ".con";
    std::FILE* f = std::fopen(conPath.c_str(), "w");
    // demoGetString keeps the rest of the line verbatim, quotes included, so the path is bare.
    std::fprintf(f, "demo.demoMode playback\ndemo.cameraMode user\ndemo.demoFile %s\n", demoPath.c_str());
    std::fclose(f);
    return conPath;
}

struct Delivery
{
    int worldTick;
    std::vector<unsigned char> data;
};

// Drives readData the way the engine does: deliver every packet the current tick allows, then
// advance one tick when the recorder answers with an empty buffer.
std::vector<Delivery> drain(DemoRecorder& rec, unsigned int* emptyCalls)
{
    std::vector<Delivery> out;
    unsigned char scratch[1];
    io::BitStream bs(scratch, 1);
    *emptyCalls = 0;
    for (;;)
    {
        const int tick = WorldPref::getTickCount();
        if (!rec.readData(tick, &bs))
            break;
        const unsigned int bytes = bs.m_highWaterBits / 8;
        if (bytes == 0)
        {
            ++*emptyCalls;
            WorldPref::advanceTickCount(1);
            continue;
        }
        Delivery d;
        d.worldTick = WorldPref::getTickCount();
        d.data.assign(rec.bufferData, rec.bufferData + bytes);
        out.push_back(d);
    }
    return out;
}

bool isIdentity(const Mat4& m)
{
    for (int i = 0; i < 16; ++i)
        if (m.m[i] != ((i % 5) == 0 ? 1.0f : 0.0f))
            return false;
    return true;
}

io::PlayerInput pressedAltFire()
{
    io::PlayerInput input;
    std::memset(&input, 0, sizeof(input));
    input.setInputIsMapped(io::PIAltFire, true);
    input.inputs[io::PIAltFire] = 1.0f;
    return input;
}

// The fresh rig, before the stream has synced the clock. Nothing here may reach the player
// manager, the template manager or the free camera, none of which this check links.
void checkRigBeforePlayback(DemoRecorder& rec)
{
    DemoSpectatorCamera* const rig = rec.demoSpectatorCamera;
    if (!rig)
        return;

    check(!rec.quit(), "a user-camera rig never reports the end of a track");
    check(rec.getCurrentDemoPlayerId() == -1, "no player followed yet");
    check(rec.getIsFreeFlying(), "the rig starts on the free camera");
    check(DemoSpectatorCamera::intToMode(1) == DemoSpectatorCamera::Mode_FollowPlayer
              && DemoSpectatorCamera::intToMode(0) == DemoSpectatorCamera::Mode_FreeCamera
              && DemoSpectatorCamera::intToMode(7) == DemoSpectatorCamera::Mode_FreeCamera,
          "intToMode maps 1 to follow and everything else to free");

    // No free camera and no followed player: the view stays the constructor's identity.
    Mat4 view;
    std::memset(&view, 0xCD, sizeof(view));
    float fov = 0.0f;
    rec.getPlaybackMatrixAndFOV(&view, &fov);
    check(fov == 1.0f, "the playback fov is always 1.0");
    check(isIdentity(view), "the view is the rig's identity before anything is followed");
    fov = 0.0f;
    rec.getPlaybackMatrixAndFOV(0, &fov);
    check(fov == 1.0f, "the fov is written even without a matrix");
    rec.getPlaybackMatrixAndFOV(&view, 0);
    check(isIdentity(view), "the matrix is written even without a fov");

    // Before the first timed frame the clock is not synced: the rig ignores the tick count.
    rec.updateTicks(3);
    check(rig->m_lastTicks == 0, "unsynced playback keeps the ticks away from the rig");

    // The input queue fills during playback and drains front first; with the gate set, the
    // rig never sees the samples (and so never builds its free camera).
    const io::PlayerInput input = pressedAltFire();
    rec.addPlayerInput(input);
    rec.addPlayerInput(input);
    check(rec.playerInputs.size() == 2, "playback queues every input sample");
    rec.inputSuppressed = true;
    rec.handlePlayerInput(0.1f);
    rec.inputSuppressed = false;
    check(rec.playerInputs.empty(), "handlePlayerInput drains the queue");
    check(!rig->m_inputArmed && rig->getIsFreeFlying(), "the gated rig saw no input");

    // A suppressed rig still records the interpolation for the next view request.
    rec.cameraSuppressed = true;
    rec.demoVisualUpdate(0.25f, 0.5f);
    rec.cameraSuppressed = false;
    check(rec.playbackInterpolation == 0.5f, "demoVisualUpdate keeps its second argument");
    check(rig->m_visualTime == 0.0f, "the suppressed rig got no visual update");
}

// Every forwarder with no rig, the dedicated recorder's situation.
void checkWithoutRig(DemoRecorder& rec)
{
    DemoSpectatorCamera* const rig = rec.demoSpectatorCamera;
    rec.demoSpectatorCamera = 0;

    Mat4 view;
    std::memset(&view, 0xCD, sizeof(view));
    float fov = -3.0f;
    rec.getPlaybackMatrixAndFOV(&view, &fov);
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&view);
    bool untouched = true;
    for (unsigned int i = 0; i < sizeof(view); ++i)
        untouched = untouched && bytes[i] == 0xCD;
    check(untouched && fov == -3.0f, "no rig leaves both view outputs untouched");
    rec.updateTicks(5);
    rec.addPlayerInput(pressedAltFire());
    rec.handlePlayerInput(0.1f);
    check(rec.playerInputs.empty(), "the queue still drains without a rig");
    check(!rec.quit(), "quit is false without a rig");
    check(!rec.nextCamera() && !rec.nextPlayer() && !rec.previousPlayer(),
          "camera switches fail without a rig");
    check(rec.getCurrentDemoPlayerId() == -1, "no followed player without a rig");
    check(!rec.getIsFreeFlying(), "not free flying without a rig");

    rec.demoSpectatorCamera = rig;
}

int syntheticCheck()
{
    char tmp[512];
    const char* dir = std::getenv("TEMP");
    std::snprintf(tmp, sizeof(tmp), "%s\\bf2_demo_playback_read.bf2demo", dir ? dir : ".");
    const std::string path(tmp);

    std::vector<Frame> frames;
    const int ticks[] = { -1, -1, 1000, 1000, 1001, 1005 };
    for (int i = 0; i < 6; ++i)
    {
        Frame fr;
        fr.tick = ticks[i];
        for (int b = 0; b < 3 + i * 2; ++b)
            fr.data.push_back((unsigned char)(i * 16 + b));
        frames.push_back(fr);
    }
    writeSyntheticDemo(path, frames);
    std::fprintf(stderr, "[check] synthetic demo written: %s\n", path.c_str());
    const std::string con = writeConfig(path);

    WorldPref::resetTickCount(7);
    DemoRecorder rec(con);
    std::fprintf(stderr, "[check] recorder constructed\n");
    check(rec.renderMode, "initPlayback succeeded");
    check(rec.isOpenForPlayback(), "stream is open for playback");
    check(rec.demoSpectatorCamera != 0, "initPlayback created the spectator rig");
    check(rec.getHeader()->version == 3153, "header version");
    check(rec.getHeader()->serverName == "check server", "header server name");
    check(rec.getHeader()->mapName == "strike_at_karkand", "header map name");
    checkRigBeforePlayback(rec);

    unsigned int empty = 0;
    const std::vector<Delivery> got = drain(rec, &empty);
    check(got.size() == frames.size(), "every packet delivered");
    for (size_t i = 0; i < got.size() && i < frames.size(); ++i)
    {
        check(got[i].data == frames[i].data, "packet payload matches");
        if (frames[i].tick >= 0)
            check(got[i].worldTick == frames[i].tick, "packet delivered on its tick");
    }
    // Initial-state packets are delivered while the world tick is still the caller's.
    if (got.size() >= 2)
        check(got[0].worldTick == 7 && got[1].worldTick == 7, "initial state delivered immediately");
    // 1000 is synced, 1001 needs one empty answer, 1005 four more.
    check(empty == 5, "empty answers while waiting for future ticks");
    check(rec.endOfStream, "end of stream latched");
    check(!rec.readData(WorldPref::getTickCount(), 0), "reads after the end fail without touching the stream");
    check(rec.renderType == 0, "InitialState leaves a non-render playback alone");
    check(rec.demoSpectatorCamera->m_playbackMode == false, "rig left in user mode");

    // Once internalReadData has synced the world clock, the rig keeps every tick count.
    rec.updateTicks(3);
    check(rec.demoSpectatorCamera->m_lastTicks == 3, "synced playback hands the ticks to the rig");
    rec.updateTicks(0);
    check(rec.demoSpectatorCamera->m_lastTicks == 0, "a zero tick count is kept too");

    checkWithoutRig(rec);

    // restartPlayback reopens from the start on a fresh rig; the old one is leaked as shipped.
    const DemoSpectatorCamera* const oldRig = rec.demoSpectatorCamera;
    rec.playbackPlaying = false;
    rec.restartPlayback();
    check(rec.renderMode && rec.isOpenForPlayback(), "restartPlayback reopened the stream");
    check(rec.playbackPlaying, "restartPlayback sets playbackPlaying");
    check(rec.demoSpectatorCamera != 0 && rec.demoSpectatorCamera != oldRig,
          "restartPlayback built a new rig");
    check(!rec.playbackTickSynced && !rec.endOfStream, "restartPlayback cleared the stream state");
    unsigned int emptyAgain = 0;
    const std::vector<Delivery> again = drain(rec, &emptyAgain);
    check(again.size() == frames.size(), "every packet delivered again after the restart");
    for (size_t i = 0; i < again.size() && i < frames.size(); ++i)
        check(again[i].data == frames[i].data, "restarted packet payload matches");
    check(emptyAgain == empty, "the restarted stream waits on the same ticks");

    std::remove(con.c_str());
    std::remove(path.c_str());
    return 0;
}

int realDemo(const std::string& demoPath)
{
    const std::string con = writeConfig(demoPath);   // demoPath + ".con" beside the demo

    WorldPref::resetTickCount(0);
    DemoRecorder rec(con);
    std::remove(con.c_str());
    if (!rec.renderMode)
    {
        std::printf("initPlayback failed for %s\n", demoPath.c_str());
        return 1;
    }
    const DemoHeader* h = rec.getHeader();
    std::printf("version     : %d\nserverName  : %s\ndateTime    : %s\nmapName     : %s\n",
                h->version, h->serverName.c_str(), h->dateTime.c_str(), h->mapName.c_str());

    unsigned int empty = 0;
    const std::vector<Delivery> got = drain(rec, &empty);
    unsigned long long bytes = 0;
    int firstTimed = -1;
    for (size_t i = 0; i < got.size(); ++i)
    {
        bytes += got[i].data.size();
        if (firstTimed < 0 && rec.playbackTickSynced && got[i].worldTick > 0)
            firstTimed = got[i].worldTick;
    }
    std::printf("packets     : %u (payload %llu bytes)\n", (unsigned)got.size(), bytes);
    std::printf("ticks       : first timed %d, last %d, waits %u\n", firstTimed,
                got.empty() ? -1 : got.back().worldTick, empty);
    std::printf("end latched : %s\n", rec.endOfStream ? "yes" : "no");
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    std::fprintf(stderr, "[check] start\n");
    TestFileManager fm;
    std::fprintf(stderr, "[check] file manager ready\n");
    io::fileManager = &fm;

    int rc = argc > 1 ? realDemo(argv[1]) : syntheticCheck();
    io::fileManager = 0;
    if (g_failures)
    {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    if (argc <= 1)
        std::printf("demo playback read check passed\n");
    return rc;
}
