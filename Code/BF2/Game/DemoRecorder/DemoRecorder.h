// Demo recording and playback state. Camera-track modes and rendering retain [FRONTIER] stops.
// This nonpolymorphic class contains STL members; its ELF size must not be imposed on MSVC or
// native-pointer builds.
// [PLATFORM] In BF2.exe, fields +0x4C..+0x7B sit 0x78 bytes later and fields from +0x80 sit 0x80
// later. [INFERRED] cause: the larger MSVC std::string and std::list members.

#ifndef GAME_DEMORECORDER_DEMORECORDER_H     // [INFERRED] guard style
#define GAME_DEMORECORDER_DEMORECORDER_H

#include <list>
#include <string>

#include "../../Math/Mat4.h"      // getPlaybackMatrixAndFOV's out parameter

#include "DemoConfig.h"                      // the embedded 0x40-byte config record
#include "../../IO/Input/PlayerInput.h"      // io::PlayerInput - the list at +0x7C holds it BY value

namespace dice {
namespace hfe {

namespace io { class BitStream; class IStream; }

class DemoHeader;

// Created by initPlayback; a recording dedicated server has none.
class DemoSpectatorCamera;

enum DemoRecorderSpecialPacket
{
    DemoRecorderSpecialPacket_InitialState = 0
};

class DemoRecorder
{
public:
    // construction
    // Both forms end in init(). The config form is what the server takes: Demo::recordDemo builds a
    // DemoConfig with demoMode = 1 and hands it over (ELF 0x8141810 / PE 0x421B00's neighbour).
    explicit DemoRecorder(const DemoConfig& config);
    explicit DemoRecorder(const std::string& configFile);     // loadConfig, then init
    ~DemoRecorder();

    static void loadConfig(DemoConfig* out, const std::string& file);   // a 9 B forwarder

    // the four modes init() dispatches on
    void init(const DemoConfig& config);
    bool initRecord();
    bool initPlayback();                                      // camera tracks [FRONTIER]
    bool initRender();                                        //          [FRONTIER]
    bool initRenderSound();                                   //          [FRONTIER]

    // the record path. ALL
    bool openRecordStream(const std::string& path);
    void writeData(int tick, io::BitStream* stream);
    void writeFakePacketData(io::BitStream* stream, int tick);
    void writeSpecialPacket(DemoRecorderSpecialPacket packet);
    void writeEndOfStream(int tick);
    void close();

    // the playback path
    bool openPlaybackStream(const std::string& path);
    bool readData(int tick, io::BitStream* stream);
    bool internalReadData(int tick, io::BitStream* stream);
    bool readNextFrameHeader();
    void handleSpecialPacket(DemoRecorderSpecialPacket packet);

    const DemoHeader* getHeader() const;
    void setHeader(const DemoHeader& header);
    void stopCameraRecording();

    // pure field tests. All four are disasm-exact and touch no camera.
    bool isOpenForRecord() const;
    bool isOpenForPlayback() const;
    bool isRenderingSound() const;
    bool isRenderingToFile() const;
    bool startRenderMovie();

    // the DemoSpectatorCamera forwarders
    void updateTicks(int ticks);
    void demoVisualUpdate(float time, float interpolation);
    void addPlayerInput(const io::PlayerInput& input);
    void handlePlayerInput(float deltaTime);
    void internalHandlePlayerInput(const io::PlayerInput& input, float deltaTime);
    void getPlaybackMatrixAndFOV(Mat4* out, float* fov);
    bool quit() const;
    void restartPlayback();
    bool nextCamera();
    bool nextPlayer();
    bool previousPlayer();
    int  getCurrentDemoPlayerId();
    bool getIsFreeFlying();

    // the 0x88 record. Names without a comment are [INFERRED] from their only writer.
    // demo.cpp reads renderMode, demoConfig and byte84 by these exact names.
    /* +0x00 */ bool         renderMode;          // [IDB name] the init-success flag, despite it
    /* +0x04 */ DemoConfig   demoConfig;          // embedded by value, 0x40 in the ELF
    /* +0x44 */ std::string  string44;            // [UNRESOLVED] ctor leaves it empty
    /* +0x48 */ std::string  string48;            // [UNRESOLVED] ctor leaves it empty
    /* +0x4C */ io::IStream* zStreamFile;         // Packet stream: a ZStream over a file.
    /* +0x50 */ io::IStream* cameraStreamFile;    // the separate .bf2cam track
    /* +0x54 */ bool         inputSuppressed;
    /* +0x55 */ bool         isOpenForRecordFlag; // set by openRecordStream, tested by both is*()
    /* +0x56 */ bool         byte56;              // [UNRESOLVED] init() zeroes it
    /* +0x57 */ bool         cameraSuppressed;    // gates demoVisualUpdate
    /* +0x58 */ DemoHeader*  demoHeader;          // init() allocates one (operator new(0x14))
    /* +0x5C */ bool         playbackTickSynced;
    /* +0x5D */ bool         frameHeaderPending;
    /* +0x60 */ int          frameTick;           // [INFERRED] -1 initial state, -2 special packet
    /* +0x64 */ int          frameSize;           // [INFERRED] or the special packet id
    /* +0x68 */ unsigned int bufferCapacity;      // init() = 0x40000
    /* +0x6C */ unsigned char* bufferData;        // init() zeroes it; the playback read buffer
    /* +0x70 */ DemoSpectatorCamera* demoSpectatorCamera;
    /* +0x74 */ bool         endOfStream;
    /* +0x78 */ float        playbackInterpolation;
    /* +0x7C */ std::list<io::PlayerInput> playerInputs;     // drained by handlePlayerInput
    /* +0x80 */ int          renderType;          // 0/1/2/3; startRenderMovie moves 2 -> 3
    /* +0x84 */ bool         byte84;              // [UNRESOLVED] - see beginMainLoop/endMainLoop
    /* +0x85 */ bool         playbackPlaying;     // [IDB name]

private:
    DemoRecorder(const DemoRecorder&);                        // not in the binary; not generated
    DemoRecorder& operator=(const DemoRecorder&);
};

} // namespace hfe
} // namespace dice

#endif // GAME_DEMORECORDER_DEMORECORDER_H
