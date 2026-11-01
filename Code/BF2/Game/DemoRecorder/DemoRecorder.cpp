// Demo recording, distribution and playback support.
// Path attested by __FILE__; recovered from the symbolized dedicated ELF.
// Recorded payload sizes use the BitStream high-water mark, not its current write cursor.


#include "DemoRecorder.h"

#include "DemoHeader.h"
#include "DemoSpectatorCamera.h"
#include "../GameServer/ServerSettings.h"     // ds:0x8AAB7A0 - the header's server name
#include "../../Core/System.h"
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

// Opens a compressed demo stream. The compression wrapper retains its own reference, so the
// initial file-stream reference is released before returning it.
io::IStream* openCompressedDemoFile(const std::string& path, bool forWrite, bool secondFlag)
{
    io::IStream* const file = forWrite
        ? io::fileManager->open(path,
                                static_cast<io::FileMode>(secondFlag ? 0 : 1),
                                static_cast<io::FileAccess>(2),
                                0)
        : io::fileManager->open(path,
                                io::FILEMODE_READ,
                                static_cast<io::FileAccess>(0),
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
    inputSuppressed     = false;
    isOpenForRecordFlag = false;
    byte56              = false;
    cameraSuppressed    = false;

    demoHeader = new DemoHeader();                       // operator new(0x14)

    playbackInterpolation = 0.0f;
    playbackTickSynced  = false;
    frameHeaderPending  = false;
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

// Opens recording streams. Camera mode 1 also needs the spectator rig and camera
// track. [FRONTIER] Camera-track recording remains unrecovered.
bool DemoRecorder::initRecord()
{
    if (!openRecordStream(demoConfig.demoFile))
        return false;

    if (demoConfig.cameraMode == 1)
    {
        std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::initRecord camera arm reached "
                             "(camera-track recording is unrecovered)\n");
        std::fflush(stderr);
        std::abort();
    }
    return true;
}

// The rig is created before the stream opens and is kept on every failure path.
// [PLATFORM] BF2.exe streams a bad mode as int; the ELF's unsigned long is kept.
bool DemoRecorder::initPlayback()
{
    demoSpectatorCamera = new DemoSpectatorCamera(this);
    if (!openPlaybackStream(demoConfig.demoFile))
        return false;

    const int cameraMode = demoConfig.cameraMode;
    if (cameraMode == 0)
        return true;
    if (cameraMode == 1 || cameraMode == 2)
    {
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: demo-camera-track] Retire once
        // recordCameraToStream, playCameraFromStream and the demo-recordcam.con / demo-playcam.con
        // call are recovered.
        std::fprintf(stderr, "[FRONTIER] dice::hfe::DemoRecorder::initPlayback camera-track arm reached "
                             "(cameraMode %d)\n", cameraMode);
        std::fflush(stderr);
        std::abort();
    }
    Debug(DEBUGTYPE_6, "Game/DemoRecorder/DemoRecorder.cpp", 192, std::string(""))
        << "Invalid camera mode: " << static_cast<unsigned long>(cameraMode);
    return false;
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
    endOfStream        = false;
    playbackTickSynced = false;

    // The rig outlives the streams.
    if (demoSpectatorCamera)
    {
        demoSpectatorCamera->setPlaybackStream(0);
        demoSpectatorCamera->setRecordStream(0);
    }
}

// The rig is not null-tested: initPlayback creates it first.
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
    if (demoHeader->readFrom(zStreamFile))
    {
        demoSpectatorCamera->setPlaybackMode(false);
        demoSpectatorCamera->setPlaybackStream(0);
        return true;
    }
    close();
    return false;
}

bool DemoRecorder::readData(int tick, io::BitStream* stream)
{
    const bool read = internalReadData(tick, stream);
    if (!read && cameraStreamFile && demoConfig.syncCameraAndDemo)
    {
        cameraStreamFile->release();
        cameraStreamFile = 0;
        if (demoSpectatorCamera)
        {
            demoSpectatorCamera->setRecordStream(0);
            demoSpectatorCamera->setPlaybackStream(0);
        }
    }
    return read;
}

// Delivers the next packet once the world tick reaches it; a packet still ahead returns true with
// an empty buffer. The first timed packet, or one more than 100 ticks ahead, moves the world tick
// to the stream.
// [PLATFORM] The tick distance is unsigned as in the ELF; BF2.exe subtracts signed, which gives
// the same value.
bool DemoRecorder::internalReadData(int tick, io::BitStream* stream)
{
    if (!zStreamFile || endOfStream)
        return false;

    if (!frameHeaderPending)
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
        const int ahead = static_cast<int>(static_cast<unsigned int>(frameTick) -
                                           static_cast<unsigned int>(tick));
        if (!playbackTickSynced || 100 < ahead)
        {
            WorldPref::resetTickCount(static_cast<unsigned int>(frameTick));
            tick = WorldPref::getTickCount();
            playbackTickSynced = true;
        }
        if (tick < frameTick)
        {
            stream->setBuffer(bufferData, 0);
            stream->m_readBitPos      = 0;
            stream->m_bitPos          = 0;
            stream->m_readWriteSwitch = 1;
            return true;
        }
    }

    const unsigned int got = zStreamFile->read(bufferData, static_cast<unsigned int>(frameSize));
    if (got == static_cast<unsigned int>(frameSize))
    {
        stream->setBuffer(bufferData, got);
        stream->m_readBitPos      = 0;
        stream->m_bitPos          = 0;
        stream->m_readWriteSwitch = 1;
        frameHeaderPending = false;
        return true;
    }
    endOfStream = true;
    return false;
}

bool DemoRecorder::readNextFrameHeader()
{
    if (!zStreamFile->getAvailableBytes())
        return false;
    zStreamFile->read(&frameTick, 4);
    zStreamFile->read(&frameSize, 4);
    if (bufferCapacity < static_cast<unsigned int>(frameSize))
    {
        frameHeaderPending = false;
        return false;
    }
    frameHeaderPending = zStreamFile->getAvailableBytes() != 0;
    return frameHeaderPending;
}

void DemoRecorder::handleSpecialPacket(DemoRecorderSpecialPacket packet)
{
    if (packet == DemoRecorderSpecialPacket_InitialState)
    {
        if (renderType == 1)
            renderType = 2;
        return;
    }
    Debug(DEBUGTYPE_6, "Game/DemoRecorder/DemoRecorder.cpp", 563, std::string(""))
        << "Invalid special packet " << static_cast<unsigned long>(packet);
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
    close();
}


DemoRecorder::~DemoRecorder()
{
    close();
    delete demoSpectatorCamera;
    if (bufferData)
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

// The DemoSpectatorCamera forwarders. A recording dedicated server has no rig.

void DemoRecorder::updateTicks(int ticks)
{
    if (demoSpectatorCamera)
        demoSpectatorCamera->updateTicks(ticks);
}

// The rig is not null-tested, as shipped.
void DemoRecorder::demoVisualUpdate(float time, float interpolation)
{
    if (!cameraSuppressed)
        demoSpectatorCamera->demoVisualUpdate(time);
    playbackInterpolation = interpolation;
}

void DemoRecorder::addPlayerInput(const io::PlayerInput& input)
{
    if (isOpenForPlayback())
        playerInputs.insert(playerInputs.end(), input);
}

// The shipped loop recounts the list on every pass.
void DemoRecorder::handlePlayerInput(float deltaTime)
{
    while (playerInputs.size() != 0)
    {
        internalHandlePlayerInput(playerInputs.front(), deltaTime);
        playerInputs.erase(playerInputs.begin());
    }
}

void DemoRecorder::internalHandlePlayerInput(const io::PlayerInput& input, float deltaTime)
{
    if (demoSpectatorCamera && !inputSuppressed)
        demoSpectatorCamera->handlePlayerInput(input, deltaTime);
}

void DemoRecorder::getPlaybackMatrixAndFOV(Mat4* out, float* fov)
{
    if (demoSpectatorCamera)
        demoSpectatorCamera->getPlaybackMatrixAndFOV(out, fov, playbackInterpolation);
}

bool DemoRecorder::quit() const
{
    bool result = false;
    if (demoSpectatorCamera)
        result = demoSpectatorCamera->getEOF();
    return result;
}

// As shipped, init() overwrites the rig, header and packet buffer without freeing them, so each
// restart leaks the old ones and leaves the old free camera in the world.
void DemoRecorder::restartPlayback()
{
    if (isOpenForPlayback())
    {
        close();
        init(demoConfig);
        playbackPlaying = true;
    }
}

bool DemoRecorder::nextCamera()
{
    if (!demoSpectatorCamera)
        return false;
    return demoSpectatorCamera->nextCamera();
}

bool DemoRecorder::nextPlayer()
{
    if (!demoSpectatorCamera)
        return false;
    return demoSpectatorCamera->nextPlayer();
}

bool DemoRecorder::previousPlayer()
{
    if (!demoSpectatorCamera)
        return false;
    return demoSpectatorCamera->previousPlayer();
}

int DemoRecorder::getCurrentDemoPlayerId()
{
    if (!demoSpectatorCamera)
        return -1;
    return demoSpectatorCamera->getCurrentDemoPlayerId();
}

bool DemoRecorder::getIsFreeFlying()
{
    if (!demoSpectatorCamera)
        return false;
    return demoSpectatorCamera->getIsFreeFlying();
}

} // namespace hfe
} // namespace dice
