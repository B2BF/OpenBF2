// dice::hfe::DistributedLightRenderer, BF2.exe 0x46B5B0..0x46C690 (see the header).
//
// PE / ELF differences (the client is the reference; the ELF is the dedicated server's copy):
//   performTraceObject  the PE continues after "LightmapGeneration not found" (and then calls through
//                       the null generator) and, for a missing object, sends 0xF and still traces with
//                       a null active object before sending the trace's own result; the ELF returns
//                       after the diagnostic and after the 0xF.
//   update              the ELF writer table is ordered differently (queueText at +0x38, as the AI
//                       DebugRenderer notes); the calls are the same.
#include "DistributedLightRenderer.h"
#if !DEDICATED
#include "../../Core/ClassManager.h"
#include "../../Core/Debug.h"
#include "../../Core/Ids.h"
#include "../../Core/StringUtil.h"                    // formatString (4310D0)
#include "../../Geom/LightmapGeneration/ILightmapGeneration.h"
#include "../../Scene/Object/ObjectManager.h"         // world::objectManager (ds:A08F58)
#include "../Common/GameEvent.h"                      // PostRemoteEvent (53CFA0)
#include "../GameServer/GameServer.h"                 // sendDataBlock (4C57B0)
#include "../IEventListener.h"                        // ECNetwork
#include "../IGame.h"                                 // game (ds:99E348)
#include "../IGameServer.h"
#include "../Objects/IObject.h"
#include <cmath>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#define DLR_FILE "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Game\\DistributedLightRenderer\\DistributedLightRenderer.cpp"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace {

// The "DebugTextWriter" slots update uses (geom::DebugTextWriter, GeomMisc/DebugTextWriter.cpp),
// declared locally in table order as the AI DebugRenderer does. [INFERRED] interface name.
class DebugTextWriterView : public IBase
{
public:
    virtual bool setFont(const std::string&) = 0;                                       // +10
    virtual bool isEnabled() const = 0;                                                 // +14
    virtual void setEnabled(bool) = 0;                                                  // +18
    virtual unsigned int getColour() const = 0;                                         // +1C
    virtual void setColour(unsigned int colour) = 0;                                    // +20
    virtual void queueWorldGroupColoured(const Vec3&, unsigned int, const std::vector<std::string>&,
                                         int, int, int) = 0;                            // +24
    virtual void queueWorldTextColoured(const Vec3&, unsigned int, const std::string&, int) = 0; // +28
    virtual void queueGroupColoured(int, int, unsigned int, const std::vector<std::string>&,
                                    int, int, int) = 0;                                 // +2C
    virtual void queueTextColoured(int, int, unsigned int, const std::string&, int) = 0;// +30
    virtual void queueWorldGroup(const Vec3&, const std::vector<std::string>&, int, int, int) = 0; // +34
    virtual void queueWorldText(const Vec3&, const std::string&, int) = 0;              // +38
    virtual void queueGroup(int, int, const std::vector<std::string>&, int, int, int) = 0; // +3C
    virtual void queueText(int x, int y, const std::string& text, int alignment) = 0;  // +40
protected:
    ~DebugTextWriterView() {}
};

// The "Terrain" singleton's primary table up to the patch count (geom::Terrain, Geom/Terrain.h),
// declared locally so game code does not pull in the renderer's terrain header.
class TerrainView : public IBase
{
public:
    virtual unsigned int getReflectionTypeId() = 0;     // +10
    virtual void init() = 0;                            // +14
    virtual void load(const std::string&) = 0;          // +18
    virtual void slot07() = 0;                          // +1C
    virtual void setRenderStyle(int) = 0;               // +20
    virtual int  getRenderStyle() = 0;                  // +24
    virtual void update() = 0;                          // +28
    virtual void slot11() = 0;                          // +2C
    virtual int  getPatchCount() = 0;                   // +30, patches per side
protected:
    ~TerrainView() {}
};

void sendLightmapResult(unsigned int code)                // ECNetwork 0xE done / 0xF failed
{
    GameEvent* const event = new (std::nothrow) PostRemoteEvent(ECNetwork, code, false, 0, 0, 0.0f); // 0x54
    game->sendGameEvent(0xFFFFFFFFu, event);                                                       // +8C
}

} // namespace

// 40FF0E: an aggregate outer is refused; a failed interface query releases the new object.
IBase* DistributedLightRenderer::create(unsigned int iid, IBase* outer)
{
    if (outer) return nullptr;
    void* allocation = ::operator new(sizeof(DistributedLightRenderer));
    DistributedLightRenderer* renderer = allocation ? new (allocation) DistributedLightRenderer : nullptr;
    IBase* result = static_cast<IBase*>(renderer->queryInterface(iid));
    if (!result) renderer->release();
    return result;
}

// 46C350. +08 is not written in retail (the job counter starts with whatever the heap held);
// [CHOSEN, not recovered] it starts at 0 here.
DistributedLightRenderer::DistributedLightRenderer() : m_refCount(1), m_nextJobId(0) {}

// 46C120: the containers are cleared; the queued and assigned work items are not freed.
DistributedLightRenderer::~DistributedLightRenderer() {}

void DistributedLightRenderer::addRef() { ++m_refCount; }
int DistributedLightRenderer::getRef() { return m_refCount; }
int DistributedLightRenderer::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0) delete this;                     // +2C(1)
    return remaining;
}

void* DistributedLightRenderer::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IDistributedLightRenderer)
        return const_cast<DistributedLightRenderer*>(this);
    return nullptr;
}

// 46C1E0: objects carry patch -1; the id is the running counter.
void DistributedLightRenderer::addObjectToJobQueue(world::IObject* object, int terrainPatch)
{
    DLRWorkItem* const item = new (std::nothrow) DLRWorkItem();              // 0xC, zeroed
    item->object = object;
    item->id = m_nextJobId++;
    item->terrainPatch = object ? -1 : terrainPatch;
    m_jobs.push_back(item);
}

// 46C580: every registered object that is alive, not mobile, has geometry within 10000 of the
// origin, and whose geometry answers ILightSampleable. Walks a copy of the registry.
void DistributedLightRenderer::traceAllObjects()
{
    const std::map<unsigned int, world::IObject*> objects(world::objectManager->getAllRegisteredObjects()); // +B4
    for (std::map<unsigned int, world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it) {
        world::IObject* const object = it->second;
        const unsigned int flags = object->m_flags;
        if ((flags & world::IObject::OFHasMobilePhysics) || !(flags & world::IObject::OFIsAlive) || !object->m_geometry)
            continue;
        const Vec3& position = object->getAbsolutePosition();                // +68
        const float distance = std::sqrt(position.x * position.x + position.y * position.y + position.z * position.z);
        if (!(distance <= 10000.0f))                                         // 89FBB0
            continue;
        IBase* const geometry = static_cast<IBase*>(object->m_geometry);
        if (geometry && geometry->queryInterface(geom::IID_ILightSampleable))
            addObjectToJobQueue(object, 0);
    }
}

// 46C3F0: one job per terrain patch (the count is per side). The patch count is read through the
// singleton itself, not the editable interface it is checked for.
void DistributedLightRenderer::traceTerrain()
{
    IBase* const terrain = classManager->getSingleton(std::string("Terrain"));        // +48
    if (!terrain) {
        Debug(DEBUGTYPE_6, DLR_FILE, 0x5A, "") << "no terrain found";
        return;
    }
    if (!terrain->queryInterface(geom::IID_ITerrainEditable)) {
        Debug(DEBUGTYPE_6, DLR_FILE, 0x60, "") << "IID_ITerrainEditable not found for terrain";
        return;
    }
    TerrainView* const view = static_cast<TerrainView*>(terrain);
    for (unsigned int patch = 0; patch < static_cast<unsigned int>(view->getPatchCount() * view->getPatchCount()); ++patch)
        addObjectToJobQueue(nullptr, static_cast<int>(patch));
}

// 46BDC0: a known client is reported, then (re)inserted idle with no job.
void DistributedLightRenderer::addRenderResource(int resource)
{
    if (m_resources.find(resource) != m_resources.end())
        Debug(DEBUGTYPE_6, DLR_FILE, 0x91, "") << "Client already registred as an resource";
    m_resources.insert(std::make_pair(resource, static_cast<DLRWorkItem*>(nullptr)));
    m_idleResources.insert(resource);
}

// 46C250: a departing client's job goes back to the end of the queue.
void DistributedLightRenderer::removeRenderResource(int resource)
{
    std::map<int, DLRWorkItem*>::iterator found = m_resources.find(resource);
    if (found != m_resources.end()) {
        if (found->second)
            m_jobs.push_back(found->second);
        m_resources.erase(found);
    }
    std::set<int>::iterator idle = m_idleResources.find(resource);
    if (idle != m_idleResources.end())
        m_idleResources.erase(idle);
}

// 46BE80: the client is idle again and its job is freed.
// [CHOSEN, not recovered] Retail does not test the lookup; an unknown client reads the map head.
// Here it is ignored.
void DistributedLightRenderer::workItemFinished(int resource)
{
    std::map<int, DLRWorkItem*>::iterator found = m_resources.find(resource);
    if (found == m_resources.end()) return;
    DLRWorkItem* const item = found->second;
    found->second = nullptr;
    m_idleResources.insert(found->first);
    ::operator delete(item);
}

// 46BA30: records the job and, on a server, sends it to the client as data block 4.
void DistributedLightRenderer::assignWorkItem(int resource, DLRWorkItem* item)
{
    std::map<int, DLRWorkItem*>::iterator found = m_resources.find(resource);
    if (found == m_resources.end()) {
        Debug(DEBUGTYPE_6, DLR_FILE, 0x134, "") << "Resource " << resource << " not found";
        // [CHOSEN, not recovered] Retail goes on to write the job into the map head.
        return;
    }
    if (found->second)
        Debug(DEBUGTYPE_6, DLR_FILE, 0x139, "") << "Job already assigned to resource " << resource;
    found->second = item;
    IGameServer* const server = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)); // +0C
    if (!server) return;
    DLRWorkItemBlock block;
    std::memset(&block, 0, sizeof(block));
    if (item->object) {
        block.id = item->object->m_objectId;                                 // +10
        block.isTerrain = false;
    } else {
        block.id = static_cast<unsigned int>(item->terrainPatch);
        block.isTerrain = true;
    }
    static_cast<GameServer*>(server)->sendDataBlock(resource, 4, &block, sizeof(block), true);
}

// 46B5B0, a lightmap-rendering client's half: trace the block's object (as the object manager's
// active object) or terrain patch, and report.
void DistributedLightRenderer::performTraceObject(void* data, unsigned int size)
{
    (void)size;
    geom::ILightmapGeneration* const generation = static_cast<geom::ILightmapGeneration*>(
        classManager->getSimpleSingleton(std::string("LightmapGeneration")));          // +58
    if (!generation)
        Debug(DEBUGTYPE_6, DLR_FILE, 0xD8, "") << "LightmapGeneration not found";
    const DLRWorkItemBlock* const block = static_cast<const DLRWorkItemBlock*>(data);
    bool traced;
    if (!block->isTerrain) {
        world::IObject* const object = world::objectManager->getRegisteredObject(static_cast<int>(block->id)); // +B0
        if (!object)
            sendLightmapResult(0xF);
        world::IObject* const previous = world::objectManager->getActiveObject();   // +90
        world::objectManager->setActiveObject(object);                              // +94
        traced = generation->traceActiveObject();                                   // +14
        world::objectManager->setActiveObject(previous);
    } else {
        traced = generation->traceTerrain(static_cast<int>(block->id));             // +1C
    }
    sendLightmapResult(traced ? 0xE : 0xF);
}

// 46BED0: hand queued jobs to idle clients, then the progress lines (server) or the node banner.
void DistributedLightRenderer::update()
{
    while (!m_jobs.empty()) {
        if (m_idleResources.empty())
            break;
        const int resource = *m_idleResources.begin();
        m_idleResources.erase(m_idleResources.begin());
        DLRWorkItem* const item = m_jobs.front();
        m_jobs.pop_front();
        assignWorkItem(resource, item);
    }
    DebugTextWriterView* const writer =
        static_cast<DebugTextWriterView*>(classManager->getSingleton(std::string("DebugTextWriter"))); // +48
    if (!writer)
        return;
    const unsigned int colour = writer->getColour();
    if (game->queryInterface(IID_IGameServer)) {
        writer->setColour(0xFFFFFFFFu);
        writer->queueText(20, 500, formatString("Distributed lightmapGeneration in progress..."), 0);
        writer->queueText(20, 510, formatString("Using %d nodes", static_cast<int>(m_resources.size())), 0);
        writer->queueText(20, 520, formatString("%d items enqued.", static_cast<int>(m_jobs.size())), 0);
        writer->queueText(20, 530, formatString("%d items in progress.",
            static_cast<int>(m_resources.size() - m_idleResources.size())), 0);
    } else {
        writer->setColour(0xAAFFAAFFu);
        writer->queueText(20, 500, formatString("Distributed lightmapGeneration in progress...connected to server"), 0);
    }
    writer->setColour(colour);
}

#if defined(_M_IX86)
static_assert(sizeof(DLRWorkItem) == 0xC, "DLRWorkItem layout");
static_assert(sizeof(DLRWorkItemBlock) == 0x248, "data block 4 layout");
#endif

} // namespace hfe
} // namespace dice
#endif
