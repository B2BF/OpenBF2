// Demo recording, distribution and playback support.
// Path attested by __FILE__; recovered from the symbolized dedicated ELF.
// Recorded payload sizes use the BitStream high-water mark, not its current write cursor.


#include "DemoRecorder.h"

#include "DemoHeader.h"
#include "../GameServer/ServerSettings.h"     // ds:0x8AAB7A0 - the header's server name
#include "../IGame.h"                         // game->getCurrentMapName() - the header's map name
#include "../../Core/Debug.h"                 // init()'s two fatal diagnostics, lines 123 and 133
#include "../../Core/SystemInfo.h"            // systemInfo()->getDateTimeString() - vptr +0x0C
#include "../../IO/BitStream.h"               // m_highWaterBits / getBuffer - the packet payload
#include "../../IO/File/FileManager.h"        // io::fileManager->open - vptr +0x18
#include "../../IO/Stream/IStream.h"          // write +0x14, release +0x08
#include "../../IO/Stream/ZStream.h"          // the deflate wrapper every .bf2demo is written through

#include <cstdio>                             // fprintf / fflush - the [FRONTIER] stops
#include <cstdlib>                            // abort

namespace dice {
namespace hfe {

namespace {

// Opens a compressed demo stream. The compression wrapper retains its own reference,
// so the initial file-stream reference is released before returning it. The playback
// branch remains a separate frontier.
io::IStream* openCompressedDemoFile(const std::string& path, bool forWrite, bool secondFlag)
{
    if (!forWrite)
    {
        std::fprintf(stderr, "[FRONTIER] dice::hfe::(anon)::openCompressedDemoFile playback branch "
                             "reached (ELF 0x814F810)\n");
        std::fflush(stderr);
        std::abort();
    }

    io::IStream* const file = io::fileManager->open(
        path,
        static_cast<io::FileMode>(secondFlag ? 0 : 1),
        static_cast<io::FileAccess>(2),
        0);
    if (!file)
        return 0;

    // The ZStream mode is the inverse of forWrite: true -> 0 (ZSTREAMMODE_WRITE).
    io::ZStream* const z = new io::ZStream(file, forWrite ? io::ZStream::ZSTREAMMODE_WRITE
                                                          : io::ZStream::ZSTREAMMODE_READ);
    file->release();                                     // the ZStream holds the only ref now
    return z;
}

} // namespace

// construction

//  (C1) / 0x814F990 (C2), 101 B; PE 0x4CB8A0, 81 B. THE server'S form.
// The members are default-constructed and init() does every assignment, so the body is one call.
DemoRecorder::DemoRecorder(const DemoConfig& config)
{
    init(config);
}

//  (C1) / 0x814F840 (C2), 186 B. The console form: load a .con config by name, then
// init from it. The temporary DemoConfig is constructed, filled and destroyed around the init call.
DemoRecorder::DemoRecorder(const std::string& configFile)
{
    DemoConfig config;
    loadConfig(&config, configFile);
    init(config);
}

// a tail-jump straight into DemoConfig.cpp's loader.
void DemoRecorder::loadConfig(DemoConfig* out, const std::string& file)
{
    loadDemoConfig(out, file);
}

// init - ELF 0x814FA70, 881 B / PE 0x4CB5D0, 506 B.
// Zero the record, copy the config in, allocate the header, then dispatch on demoMode. The two
// Debug(6, ...) sites attest this file at lines 123 and 133; DEBUGTYPE_6 aborts after emitting,
// which is why neither arm needs a return value.
void DemoRecorder::init(const DemoConfig& config)
{
    // byte84 is written twice in retail - once here and once after renderType. Preserved.
    byte84          = false;
    playbackPlaying = false;
    renderMode      = false;

    demoConfig = config;                                 // the implicit member-by-member assignment

    zStreamFile         = 0;
    cameraStreamFile    = 0;
    byte54              = false;
    isOpenForRecordFlag = false;
    byte56              = false;
    cameraSuppressed    = false;

    demoHeader = new DemoHeader();                       // operator new(0x14)

    visualUpdateB       = 0.0f;
    byte5C              = false;
    byte5D              = false;
    dword60             = 0;
    dword64             = 0;
    bufferCapacity      = 0x40000;
    bufferData          = 0;
    demoSpectatorCamera = 0;
    byte74              = false;
    renderType          = 0;
    byte84              = false;

    if (!demoConfig.sanityCheck())
    {
        Debug(DEBUGTYPE_6, "Game/DemoRecorder/DemoRecorder.cpp", 123, __FUNCTION__)
            << "Invalid demo configuration.";
        return;
    }

    bool inited;
    switch (demoConfig.demoMode)
    {
    case 0:  inited = initPlayback();    break;
    case 1:  inited = initRecord();      break;
    case 2:  inited = initRender();      break;
    case 3:  inited = initRenderSound(); break;
    default:
        Debug(DEBUGTYPE_6, "Game/DemoRecorder/DemoRecorder.cpp", 133, __FUNCTION__)
            << "Invalid demo mode" << static_cast<int>(demoConfig.demoMode);
        return;
    }
    renderMode = inited;
}

// Opens recording streams. Camera mode 1 also needs the spectator rig and camera
// track. [FRONTIER] That client-only rig remains unrecovered.
bool DemoRecorder::initRecord()
{
    if (!openRecordStream(demoConfig.demoFile))
        return false;

    if (demoConfig.cameraMode == 1)
    {
        std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::initRecord camera arm reached "
                             "(DemoSpectatorCamera is unrecovered; ELF 0x814FE2C)\n");
        std::fflush(stderr);
        std::abort();
    }
    return true;
}

bool DemoRecorder::initPlayback()
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::initPlayback reached\n");
    std::fflush(stderr);
    std::abort();
}

bool DemoRecorder::initRender()
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::initRender reached\n");
    std::fflush(stderr);
    std::abort();
}

bool DemoRecorder::initRenderSound()
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::initRenderSound reached\n");
    std::fflush(stderr);
    std::abort();
}

// openRecordStream - ELF 0x8151160, 271 B / PE 0x4CAE10, 249 B.
// Refuses to reopen, then stamps the header and writes it as the file's first bytes. Version 3153
// is a literal in both ports.
bool DemoRecorder::openRecordStream(const std::string& path)
{
    if (zStreamFile)
        return false;

    zStreamFile = openCompressedDemoFile(path, true, false);
    if (!zStreamFile)
        return false;

    isOpenForRecordFlag = true;

    demoHeader->version    = 3153;
    demoHeader->serverName = serverSettings->m_serverName;
    demoHeader->dateTime   = systemInfo()->getDateTimeString();        // vptr +0x0C
    demoHeader->mapName    = game->getCurrentMapName();                // vptr +0xF0

    demoHeader->writeTo(zStreamFile);
    return true;
}

// Writes nonempty packets; writeFakePacketData also permits empty packets.
void DemoRecorder::writeData(int tick, io::BitStream* stream)
{
    const unsigned char* const buffer = stream->getBuffer();
    const unsigned int bits = stream->m_highWaterBits;
    unsigned int bytes = bits >> 3;
    if ((bits & 7) != 0)
        ++bytes;
    if (!bytes)
        return;

    zStreamFile->write(&tick, 4);
    zStreamFile->write(&bytes, 4);
    zStreamFile->write(buffer, bytes);
}

//  / PE 0x4CA5F0, 97 B. Identical to writeData except that it writes the frame
// even when the byte count is zero - the caller (flushDemoPackets) has already made that test.
// No null guard on zStreamFile here, unlike writeEndOfStream. Preserved as shipped.
void DemoRecorder::writeFakePacketData(io::BitStream* stream, int tick)
{
    const unsigned char* const buffer = stream->getBuffer();
    const unsigned int bits = stream->m_highWaterBits;
    unsigned int bytes = bits >> 3;
    if ((bits & 7) != 0)
        ++bytes;

    zStreamFile->write(&tick, 4);
    zStreamFile->write(&bytes, 4);
    zStreamFile->write(buffer, bytes);
}

//  / PE 0x4CA660, 61 B. Tick -2 is the special-packet marker.
// No null guard on zStreamFile. Preserved as shipped.
void DemoRecorder::writeSpecialPacket(DemoRecorderSpecialPacket packet)
{
    const int marker = -2;
    zStreamFile->write(&marker, 4);

    const int id = static_cast<int>(packet);
    zStreamFile->write(&id, 4);
}

// . A zero-length frame closes the stream.
// This one does guard on zStreamFile, and its shipped body returns an uninitialised value on the
// null path; the declared void return makes that unobservable, which is why void stays [CHOSEN].
void DemoRecorder::writeEndOfStream(int tick)
{
    if (!zStreamFile)
        return;

    zStreamFile->write(&tick, 4);

    const int zero = 0;
    zStreamFile->write(&zero, 4);
}

// teardown AND header access


void DemoRecorder::close()
{
    if (zStreamFile)
    {
        zStreamFile->release();                          // vptr +0x08
        zStreamFile = 0;
    }
    if (cameraStreamFile)
    {
        cameraStreamFile->release();
        cameraStreamFile = 0;
    }
    byte74 = false;
    byte5C = false;

    if (demoSpectatorCamera)
    {
        // Unreachable on a dedicated server - the pointer is only ever set by initRecord's camera
        // arm. setPlaybackStream(0) / setRecordStream(0) belong to the unrecovered camera class.
        std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::close camera arm reached\n");
        std::fflush(stderr);
        std::abort();
    }
}

const DemoHeader* DemoRecorder::getHeader() const
{
    return demoHeader;
}

void DemoRecorder::setHeader(const DemoHeader& header)
{
    *demoHeader = header;
}

void DemoRecorder::stopCameraRecording()
{
    byte56 = true;
}


DemoRecorder::~DemoRecorder()
{
    close();
    delete demoHeader;
}

bool DemoRecorder::isOpenForRecord() const
{
    return zStreamFile != 0 && isOpenForRecordFlag;
}

bool DemoRecorder::isOpenForPlayback() const
{
    return zStreamFile != 0 && !isOpenForRecordFlag;
}

bool DemoRecorder::isRenderingSound() const
{
    return demoConfig.demoMode == 3;
}

bool DemoRecorder::isRenderingToFile() const
{
    return renderType == 3;
}

bool DemoRecorder::startRenderMovie()
{
    if (renderType != 2)
        return false;
    renderType = 3;
    return true;
}

// THE DemoSpectatorCamera forwarders. Only client playback and camera-track recording own a camera,
// so a recording dedicated server calls quit and updateTicks every frame with a null camera, and
// both return without effect there. Every camera call stops by name rather than invent a call into
// an unrecovered 0xA4-byte class; the remaining forwarders are client playback only.

// Reports whether the playback camera track has ended. False without a camera.
bool DemoRecorder::quit() const
{
    if (!demoSpectatorCamera)
        return false;

    // [FRONTIER] return demoSpectatorCamera->getEOF(); - replace this stop with that call once
    // DemoSpectatorCamera is recovered. Unreachable until initRecord's camera arm or initPlayback
    // sets the pointer.
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoSpectatorCamera::getEOF reached "
                         "(from dice::hfe::DemoRecorder::quit)\n");
    std::fflush(stderr);
    std::abort();
}

// Advances the camera track by this frame's tick count. Does nothing without a camera.
void DemoRecorder::updateTicks(int ticks)
{
    if (!demoSpectatorCamera)
        return;

    // [FRONTIER] demoSpectatorCamera->updateTicks(ticks); - same retirement condition as quit.
    (void)ticks;
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoSpectatorCamera::updateTicks reached "
                         "(from dice::hfe::DemoRecorder::updateTicks)\n");
    std::fflush(stderr);
    std::abort();
}

void DemoRecorder::handlePlayerInput(float)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::handlePlayerInput reached\n");
    std::fflush(stderr);
    std::abort();
}

void DemoRecorder::addPlayerInput(const io::PlayerInput&)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::addPlayerInput reached\n");
    std::fflush(stderr);
    std::abort();
}

void DemoRecorder::demoVisualUpdate(float, float)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::demoVisualUpdate reached\n");
    std::fflush(stderr);
    std::abort();
}

// Returns without a spectator camera. Otherwise stops at the unresolved camera
// transform call, which takes output, flag, and visual update time.
void DemoRecorder::getCameraTransformation(Mat4&, int)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::getCameraTransformation reached\n");
    std::fflush(stderr);
    std::abort();
}

} // namespace hfe
} // namespace dice
