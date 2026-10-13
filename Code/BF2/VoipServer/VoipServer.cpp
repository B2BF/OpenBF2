// BF2VoipServer facade: the instance registry and lifecycle the engine imports.
//
// Recovered from fh2 (CBF2VoipServer 0x840CB0-0x8411F0). The win32 image exports these members by
// decorated name (Code/VoipServerAbi/BF2VoipServer_w32ded.def). A mutex guards the instance vector
// and the running flag; every public entry takes it.

#include <cstdio>

#include "VoipServerApi.h"
#include "../Core/Mutex.h"
#include "../IO/Network/SocketManager.h"
#include "VoipDebug.h"
#include "VoipResult.h"
#include "VoipServerInstance.h"

namespace BF2VoipServer {

CBF2VoipServer::CBF2VoipServer()
    : mRunning(false)
    , mSocketManager(0)
    , mMutex()
    , mInstances()
    , mNextInstanceId(1)
{
}

// The socket manager is owned by Shutdown, which closes and deletes it; only the singleton is torn
// down here, matching the shipped destructor.
CBF2VoipServer::~CBF2VoipServer()
{
    CDebugStandard::Close();
}

bool CBF2VoipServer::IsRunning()
{
    return mRunning;
}

unsigned int CBF2VoipServer::getNumInstances()
{
    return static_cast<unsigned int>(mInstances.size());
}

CBF2VoipServerInstance* CBF2VoipServer::getInstance(unsigned int index)
{
    if (index < mInstances.size())
        return mInstances[index];
    return 0;
}

// Refuses a second start with -4. Local mode also allocates and initialises the socket manager.
VOIP::VRESULT CBF2VoipServer::Initialize(bool local)
{
    VOIP::VRESULT result;
    mMutex.lock();
    if (mRunning)
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, "Attempted to start running server");
        result = VOIP::VRESULT_INVALID_STATE;
    }
    else
    {
        mRunning = true;
        result = VOIP::VRESULT_SUCCESS;
        if (local)
        {
            mSocketManager = new dice::hfe::io::SocketManager();
            mSocketManager->init();
        }
    }
    mMutex.unlock();
    return result;
}

// Creates, starts and registers an instance. On success writes the new id to outId; before
// Initialize it reports VRESULT_INVALID_STATE and leaves outId at -1. A failed Start still reports
// success and leaves the instance unregistered (as shipped).
VOIP::VRESULT CBF2VoipServer::CreateInstance(unsigned int port, std::string password, int& outId)
{
    mMutex.lock();
    outId = -1;
    VOIP::VRESULT result = VOIP::VRESULT_SUCCESS;
    if (mRunning)
    {
        CBF2VoipServerInstance* instance = new CBF2VoipServerInstance();
        if (instance->Start(port, 55123, password, mNextInstanceId) == VOIP::VRESULT_SUCCESS)
        {
            mInstances.push_back(instance);
            outId = mNextInstanceId++;
            result = VOIP::VRESULT_SUCCESS;
        }
    }
    else
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR,
            "Attempted to create an instance while server has not been started");
        result = VOIP::VRESULT_INVALID_STATE;
    }
    mMutex.unlock();
    return result;
}

// Stops and destroys the instance whose id matches; VRESULT_FAILURE when none does.
VOIP::VRESULT CBF2VoipServer::ReleaseInstance(int id)
{
    mMutex.lock();
    VOIP::VRESULT result = VOIP::VRESULT_FAILURE;
    for (std::vector<CBF2VoipServerInstance*>::iterator it = mInstances.begin(); it != mInstances.end(); ++it)
    {
        if ((*it)->GetInstanceId() == id)
        {
            (*it)->Stop();
            delete *it;
            mInstances.erase(it);
            result = VOIP::VRESULT_SUCCESS;
            break;
        }
    }
    mMutex.unlock();
    return result;
}

// Stops and destroys every instance. Fails with -4 before Initialize.
VOIP::VRESULT CBF2VoipServer::ReleaseAllInstances()
{
    mMutex.lock();
    VOIP::VRESULT result;
    if (mRunning)
    {
        for (std::size_t i = 0; i < mInstances.size(); ++i)
        {
            mInstances[i]->Stop();
            delete mInstances[i];
        }
        mInstances.clear();
        result = VOIP::VRESULT_SUCCESS;
    }
    else
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR,
            "Attempted to release all instances while server has not been started");
        result = VOIP::VRESULT_INVALID_STATE;
    }
    mMutex.unlock();
    return result;
}

// Ticks every registered instance. Fails with -4 before Initialize.
VOIP::VRESULT CBF2VoipServer::Update()
{
    VOIP::VRESULT result;
    mMutex.lock();
    if (mRunning)
    {
        for (std::size_t i = 0; i < mInstances.size(); ++i)
            mInstances[i]->Update();
        result = VOIP::VRESULT_SUCCESS;
    }
    else
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR,
            "Attempted to update server all without starting");
        result = VOIP::VRESULT_INVALID_STATE;
    }
    mMutex.unlock();
    return result;
}

// Releases every instance and, in local mode, the socket manager. Fails with -4 when not running.
VOIP::VRESULT CBF2VoipServer::Shutdown()
{
    VOIP::VRESULT result;
    mMutex.lock();
    bool stopped = !mRunning;
    mNextInstanceId = 1;
    if (stopped)
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, "Attempted to shudown server not running");
        result = VOIP::VRESULT_INVALID_STATE;
    }
    else
    {
        ReleaseAllInstances();
        if (mSocketManager)
        {
            mSocketManager->close();
            delete mSocketManager;
            mSocketManager = 0;
        }
        mRunning = false;
        result = VOIP::VRESULT_SUCCESS;
    }
    mMutex.unlock();
    return result;
}

} // namespace BF2VoipServer
