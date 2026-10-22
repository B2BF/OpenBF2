// BF2VoipServer public facade.
//
// The voice server ships as its own server-side image: BF2VoipServer_w32ded.dll on Windows
// (statically linked into fh2 on Linux). The engine reaches it only through CBF2VoipServer; the
// win32 image exports these members by decorated name. The module source layout is [INFERRED] -
// no module __FILE__ string survives (see voip-server/recon.md).
//
// Field offsets beside the members are from the recovered x86-64 fh2 ELF. The x86 PE differs and
// is verified with the x86 build, never with x64.

#ifndef BF2_VOIPSERVER_VOIPSERVERAPI_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPSERVERAPI_H

#include <string>
#include <vector>

#include "../Core/Mutex.h"
#include "VoipResult.h"

namespace dice { namespace hfe { namespace io { class SocketManager; } } }

namespace BF2VoipServer {

class CBF2VoipServerInstance;

// Facade and instance registry, plus the module lifecycle the engine imports. Not polymorphic, so
// the interface is its exported member set rather than a vtable.
class CBF2VoipServer
{
public:
    CBF2VoipServer();
    ~CBF2VoipServer();

    // Starts the facade. Local mode also allocates the socket manager. A second start fails with
    // VRESULT_INVALID_STATE ("Attempted to start running server").
    VOIP::VRESULT Initialize(bool local);

    // Creates, starts and registers one voice-server instance on port using password. On success
    // writes the new instance id to outId. Before Initialize it fails with
    // VRESULT_INVALID_STATE and leaves outId at -1.
    VOIP::VRESULT CreateInstance(unsigned int port, std::string password, int& outId);

    // Ticks every registered instance in order.
    VOIP::VRESULT Update();

    // Releases every instance and, in local mode, the socket manager. Fails with
    // VRESULT_INVALID_STATE when not running.
    VOIP::VRESULT Shutdown();

    bool IsRunning();

    unsigned int getNumInstances();

    // Returns the instance at the given registry position, or null when out of range.
    CBF2VoipServerInstance* getInstance(unsigned int index);

    // Stops and destroys the instance whose id matches; VRESULT_FAILURE when none does.
    VOIP::VRESULT ReleaseInstance(int id);

    // Stops and destroys every instance. Fails with VRESULT_INVALID_STATE when not running.
    VOIP::VRESULT ReleaseAllInstances();

private:
    bool mRunning;                                          // +0x00
    dice::hfe::io::SocketManager* mSocketManager;           // +0x08  local mode only
    dice::hfe::Mutex mMutex;                                // +0x10
    std::vector<CBF2VoipServerInstance*> mInstances;        // +0x18
    unsigned int mNextInstanceId;                           // +0x30  ctor sets 1
};

} // namespace BF2VoipServer

#endif // BF2_VOIPSERVER_VOIPSERVERAPI_H
