// Engine-side half of the BF2/aidll singleton refresh handshake. The updater interface is shared
// across the DLL boundary; declaring its real virtual destructor lets each compiler place update
// at its shipped native slot (ELF +0x08, PE +0x04).

#ifndef AI_AIINTERFACE_SINGLETONSERVER_H
#define AI_AIINTERFACE_SINGLETONSERVER_H

namespace dice {
namespace hfe {
namespace ai {

class IAIEventCourier;

class ISingletonServerUpdater
{
public:
    ISingletonServerUpdater();
    virtual ~ISingletonServerUpdater();
    virtual void update() = 0;
};

class BFSingletonServer
{
public:
    static void updateStaticMethods();
    static void updateStaticMethodsInternal();
    static void clearAllStaticMethods();

    static void*              m_iaiObjectUtils;
    static void*              m_iDebugRenderer;
    static void*              m_iDPAIObjectCreator;
    static void*              m_iInformationFactory;
    static IAIEventCourier*   m_iaiEventCourier;
    static void*              m_iaiPlayerCourier;
};

// Caches renderer interfaces from the normal singleton registry.
// Dedicated builds may have no renderer; callers must allow absent interfaces.
class RenderSingletonServer
{
public:
    static void updateStaticMethods();

    static void* m_debugLineDrawer;             // ds: "DebugLineDrawer"
    static void* m_debugCircleDrawer;           // ds: "DebugCircleDrawer"
    static void* m_debugTextWriter;             // ds: "DebugTextWriter"
    static void* m_debugRenderer;               // ds: "DebugRenderer"
    static void* m_iDebugRenderer;              // ds: "IDebugRenderer"
};

class BFSingletonServerUpdater : public ISingletonServerUpdater
{
public:
    BFSingletonServerUpdater();
    virtual ~BFSingletonServerUpdater();
    virtual void update();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_SINGLETONSERVER_H
