// Demo recording, distribution and playback support.
// Path attested by __FILE__; recovered from the symbolized dedicated ELF.
// Recorded payload sizes use the BitStream high-water mark, not its current write cursor.


#include "DemoRecorder.h"

#include "DemoHeader.h"
#include "DemoSpectatorCamera.h"
#include "../GameServer/ServerSettings.h"     // ds:0x8AAB7A0 - the header's server name
#include "../IGame.h"                         // game->getCurrentMapName() - the header's map name
#include "../../Core/Debug.h"                 // init()'s two fatal diagnostics, lines 123 and 133
#include "../../Core/SystemInfo.h"            // systemInfo()->getDateTimeString() - vptr +0x0C
#include "../../Core/System.h"                // WorldPref::resetTickCount / getTickCount - the playback clock
#include "../../IO/Console/Console.h"         // io::mainConsole->run - the demo camera .con files
#include "../../IO/BitStream.h"               // m_highWaterBits / getBuffer - the packet payload
#include "../../IO/File/FileManager.h"        // io::fileManager->open - vptr +0x18
#include "../../IO/Stream/IStream.h"          // write +0x14, release +0x08
#include "../../IO/Stream/ZStream.h"          // the deflate wrapper every .bf2demo is written through


namespace dice {
namespace hfe {

namespace {

// openCompressedDemoFile - ELF 0x814F780, 184 B.
// Opens a compressed demo stream. The compression wrapper retains its own reference,
// so the initial file-stream reference is released before returning it. Reading opens with
// mode 2 / access 0 whatever secondFlag says; writing picks mode 0 or 1 from it, access 2.
io::IStream* openCompressedDemoFile(const std::string& path, bool forWrite, bool secondFlag)
{
    io::IStream* const file = forWrite
        ? io::fileManager->open(path, static_cast<io::FileMode>(secondFlag ? 0 : 1),
                                static_cast<io::FileAccess>(2), 0)
        : io::fileManager->open(path, static_cast<io::FileMode>(2),
                                static_cast<io::FileAccess>(0), 0);
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
    tickResynced        = false;
    frameHeaderRead     = false;
    frameTick           = 0;
    frameSize           = 0;
    bufferCapacity      = 0x40000;
    bufferData          = 0;
    demoSpectatorCamera = 0;
    endOfStream         = false;
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

namespace {
// The eight empty arguments and the ignored result of io::Console::run, as every .con runner passes.
void runDemoCameraConfig(const char* file)
{
    io::mainConsole->run(file, "", "", "", "", "", "", "", "", io::Console::ignoredString_);   // vptr +0x28
}
} // namespace

// initRecord - ELF 0x814FDF0, 847 B.
// Opens the packet stream; camera mode 1 also records the spectator camera to cameraFile and
// runs Settings/demo-recordcam.con.
bool DemoRecorder::initRecord()
{
    if (!openRecordStream(demoConfig.demoFile))
        return false;

    if (demoConfig.cameraMode == 1)
    {
        demoSpectatorCamera = new DemoSpectatorCamera(this);         // operator new(0xA4)
        if (!recordCameraToStream(demoConfig.cameraFile))
            return false;
        runDemoCameraConfig("Settings/demo-recordcam.con");
    }
    return true;
}

// initPlayback - ELF 0x8150140, 1429 B.
// Always builds the spectator camera, then opens the packet stream. Camera mode 0 plays the
// demo with the free/follow camera; 1 records that camera to cameraFile while playing
// (demo-recordcam.con); 2 plays a recorded camera track (demo-playcam.con). Line 192 reports
// any other mode, as a DEBUGTYPE_6 abort.
bool DemoRecorder::initPlayback()
{
    demoSpectatorCamera = new DemoSpectatorCamera(this);             // operator new(0xA4)
    if (!openPlaybackStream(demoConfig.demoFile))
        return false;

    switch (demoConfig.cameraMode)
    {
    case 0:
        return true;
    case 1:
        if (!recordCameraToStream(demoConfig.cameraFile))
            return false;
        runDemoCameraConfig("Settings/demo-recordcam.con");
        return true;
    case 2:
        if (!playCameraFromStream(demoConfig.cameraFile))
            return false;
        runDemoCameraConfig("Settings/demo-playcam.con");
        return true;
    default:
        Debug(DEBUGTYPE_6, "Game/DemoRecorder/DemoRecorder.cpp", 192, "")
            << "Invalid camera mode: " << static_cast<int>(demoConfig.cameraMode);
        return false;
    }
}

// initRender - ELF 0x81506E0, 849 B / PE 0x54BE60. Movie render (demo mode 2): the demo plays
// through a recorded camera track; renderType 1 waits for the initial-state packet
// (handleSpecialPacket arms 2) and startRenderMovie turns it into 3, rendering to file.
bool DemoRecorder::initRender()
{
    demoSpectatorCamera = new DemoSpectatorCamera(this);             // operator new(0xA4)
    renderType = 1;
    if (!openPlaybackStream(demoConfig.demoFile))
        return false;
    if (!playCameraFromStream(demoConfig.cameraFile))
        return false;
    runDemoCameraConfig("Settings/demo-playcam.con");
    return true;
}

// initRenderSound - ELF 0x8150A40, 833 B / PE 0x54C000. Sound render (demo mode 3): as initRender
// without the movie state; as shipped it runs the record-camera config.
bool DemoRecorder::initRenderSound()
{
    demoSpectatorCamera = new DemoSpectatorCamera(this);             // operator new(0xA4)
    if (!openPlaybackStream(demoConfig.demoFile))
        return false;
    if (!playCameraFromStream(demoConfig.cameraFile))
        return false;
    runDemoCameraConfig("Settings/demo-recordcam.con");
    return true;
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

// openPlaybackStream - ELF 0x8151270, 189 B.
// Refuses to reopen. The 0x40000-byte read buffer is allocated once and kept across restarts.
// A file whose header does not read closes again. The camera is told it plays a demo (not a
// camera track) with no camera stream yet. No null test on the camera: initPlayback made it.
bool DemoRecorder::openPlaybackStream(const std::string& path)
{
    if (zStreamFile)
        return false;
    if (!bufferData)
    {
        bufferData = new unsigned char[bufferCapacity];
        if (!bufferData)
            return false;
    }
    zStreamFile = openCompressedDemoFile(path, false, false);
    if (!zStreamFile)
        return false;

    isOpenForRecordFlag = false;
    if (!demoHeader->readFrom(zStreamFile))
    {
        close();
        return false;
    }
    demoSpectatorCamera->setPlaybackMode(false);
    demoSpectatorCamera->setPlaybackStream(0);
    return true;
}

// recordCameraToStream - ELF 0x8151330, 360 B.
// The camera track is written through the same compressed-file wrapper, refusing an existing
// file (the open's secondFlag); line 355 reports the failure.
bool DemoRecorder::recordCameraToStream(const std::string& path)
{
    cameraStreamFile = openCompressedDemoFile(path, true, false);
    if (!cameraStreamFile)
    {
        Debug(DEBUGTYPE_6, "Game/DemoRecorder/DemoRecorder.cpp", 355, "")
            << "Failed to open camera file " << path << " for writing. It probably exist already.";
        return false;
    }
    byte56           = true;
    cameraSuppressed = false;
    demoSpectatorCamera->setPlaybackStream(0);
    demoSpectatorCamera->setRecordStream(cameraStreamFile);
    return true;
}

// playCameraFromStream - ELF 0x81514A0, 106 B.
bool DemoRecorder::playCameraFromStream(const std::string& path)
{
    cameraStreamFile = openCompressedDemoFile(path, false, false);
    if (!cameraStreamFile)
        return false;
    byte56           = false;
    cameraSuppressed = true;
    demoSpectatorCamera->setPlaybackMode(true);
    demoSpectatorCamera->setPlaybackStream(cameraStreamFile);
    return true;
}

// readData - ELF 0x81515C0, 132 B / PE 0x54C3B0.
// One packet for the given tick. When the packet stream runs dry and the config ties the camera
// to it (DemoConfig +0x2C, syncCameraAndDemo), the camera track is dropped as well.
bool DemoRecorder::readData(int tick, io::BitStream* stream)
{
    const bool read = internalReadData(tick, stream);
    if (!read && cameraStreamFile && demoConfig.syncCameraAndDemo)
    {
        cameraStreamFile->release();                              // vptr +0x08
        cameraStreamFile = 0;
        if (demoSpectatorCamera)
        {
            demoSpectatorCamera->setRecordStream(0);
            demoSpectatorCamera->setPlaybackStream(0);
        }
    }
    return read;
}

// internalReadData - ELF 0x8151650, 301 B.
// Frames are (tick, size, payload); tick -2 frames are special packets and are handled inline.
// The first data frame, or one more than 100 ticks ahead of the caller, resets the world tick to
// the frame's; a frame still in the future hands back an empty packet. A short payload read ends
// the stream. A tick of -1 is never compared and plays at once.
bool DemoRecorder::internalReadData(int tick, io::BitStream* stream)
{
    if (!zStreamFile || endOfStream)
        return false;

    if (!frameHeaderRead)
    {
        for (;;)
        {
            if (!readNextFrameHeader())
            {
                endOfStream = true;
                return false;
            }
            if (frameTick != -2)
                break;
            handleSpecialPacket(static_cast<DemoRecorderSpecialPacket>(frameSize));
        }
    }

    if (frameTick != -1)
    {
        if (!tickResynced || frameTick - tick > 100)
        {
            WorldPref::resetTickCount(static_cast<unsigned int>(frameTick));
            tick         = WorldPref::getTickCount();
            tickResynced = true;
        }
        if (frameTick > tick)
        {
            stream->setBuffer(bufferData, 0);
            stream->resetPositions();
            stream->setReadMode();
            return true;
        }
    }

    const unsigned int got = zStreamFile->read(bufferData, frameSize);   // vptr +0x10
    if (got != frameSize)
    {
        endOfStream = true;
        return false;
    }
    stream->setBuffer(bufferData, got);
    stream->resetPositions();
    stream->setReadMode();
    frameHeaderRead = false;
    return true;
}

// readNextFrameHeader - ELF 0x81519A0, 140 B.
// A frame larger than the read buffer is refused (and left unread); otherwise the header counts
// as read while the stream still has bytes.
bool DemoRecorder::readNextFrameHeader()
{
    if (!zStreamFile->getAvailableBytes())                            // vptr +0x1C
        return false;
    zStreamFile->read(&frameTick, 4);
    zStreamFile->read(&frameSize, 4);
    if (frameSize > bufferCapacity)
    {
        frameHeaderRead = false;
        return false;
    }
    frameHeaderRead = zStreamFile->getAvailableBytes() != 0;
    return frameHeaderRead;
}

// handleSpecialPacket - ELF 0x8151A30, 335 B.
// The initial-state marker arms a pending movie render (1 -> 2); line 563 reports anything else.
void DemoRecorder::handleSpecialPacket(DemoRecorderSpecialPacket packet)
{
    if (packet == DemoRecorderSpecialPacket_InitialState)
    {
        if (renderType == 1)
            renderType = 2;
        return;
    }
    Debug(DEBUGTYPE_6, "Game/DemoRecorder/DemoRecorder.cpp", 563, "")
        << "Invalid special packet " << static_cast<int>(packet);
}

// restartPlayback - ELF 0x8151D70, 64 B.
// Reopens from the start through init(). As shipped, init() allocates a fresh DemoHeader and a
// fresh camera without freeing the old ones; preserved.
void DemoRecorder::restartPlayback()
{
    if (!isOpenForPlayback())
        return;
    close();
    init(demoConfig);
    playbackPlaying = true;
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
    endOfStream  = false;
    tickResynced = false;

    if (demoSpectatorCamera)
    {
        demoSpectatorCamera->setPlaybackStream(0);
        demoSpectatorCamera->setRecordStream(0);
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


// ~DemoRecorder - ELF 0x8150EF0, 342 B: close, then the camera, the read buffer and the header.
DemoRecorder::~DemoRecorder()
{
    close();
    delete demoSpectatorCamera;
    delete[] bufferData;
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

// The DemoSpectatorCamera forwarders - ELF 0x8151B90..0x8151E80, BF2.exe 0x54BBE0..0x54BD01.
// Each answers false / -1 without a camera, except demoVisualUpdate, which (as shipped) calls
// the camera unless a camera track plays.

bool DemoRecorder::quit() const
{
    return demoSpectatorCamera ? demoSpectatorCamera->getEOF() : false;
}

void DemoRecorder::updateTicks(int ticks)
{
    if (demoSpectatorCamera)
        demoSpectatorCamera->updateTicks(ticks);
}

// handlePlayerInput - ELF 0x8151C40: drains the queued inputs in order.
void DemoRecorder::handlePlayerInput(float deltaTime)
{
    while (!playerInputs.empty())
    {
        internalHandlePlayerInput(playerInputs.front(), deltaTime);
        playerInputs.pop_front();
    }
}

void DemoRecorder::internalHandlePlayerInput(const io::PlayerInput& input, float deltaTime)
{
    if (demoSpectatorCamera && !byte54)
        demoSpectatorCamera->handlePlayerInput(input, deltaTime);
}

// addPlayerInput - ELF 0x8151BF0: inputs are only queued while a demo plays.
void DemoRecorder::addPlayerInput(const io::PlayerInput& input)
{
    if (isOpenForPlayback())
        playerInputs.push_back(input);
}

// demoVisualUpdate - ELF 0x8151BB0 / PE 0x54BBE0. b is kept for getPlaybackMatrixAndFOV.
void DemoRecorder::demoVisualUpdate(float a, float b)
{
    if (!cameraSuppressed)
        demoSpectatorCamera->demoVisualUpdate(a);
    visualUpdateB = b;
}

// getPlaybackMatrixAndFOV - ELF 0x8151D00 / PE 0x54BC20.
void DemoRecorder::getPlaybackMatrixAndFOV(Mat4* matrix, float* fov)
{
    if (demoSpectatorCamera)
        demoSpectatorCamera->getPlaybackMatrixAndFOV(matrix, fov, visualUpdateB);
}

bool DemoRecorder::nextCamera()               // PE 0x54BC70
{
    return demoSpectatorCamera ? demoSpectatorCamera->nextCamera() : false;
}

bool DemoRecorder::nextPlayer()               // PE 0x54BC90
{
    return demoSpectatorCamera ? demoSpectatorCamera->nextPlayer() : false;
}

bool DemoRecorder::previousPlayer()           // PE 0x54BCB0
{
    return demoSpectatorCamera ? demoSpectatorCamera->previousPlayer() : false;
}

int DemoRecorder::getCurrentDemoPlayerId()    // PE 0x54BCD0
{
    return demoSpectatorCamera ? demoSpectatorCamera->getCurrentDemoPlayerId() : -1;
}

bool DemoRecorder::getIsFreeFlying()          // PE 0x54BCF0
{
    return demoSpectatorCamera ? demoSpectatorCamera->getIsFreeFlying() : false;
}

} // namespace hfe
} // namespace dice
