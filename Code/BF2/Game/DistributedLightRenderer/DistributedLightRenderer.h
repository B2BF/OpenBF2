// dice::hfe::DistributedLightRenderer, the "DistributedLightRenderer" singleton (factory
// "dice.bf2.DistributedLightRenderer", CID 121009, IID 120009). BF2.exe 0x46B5B0..0x46C690, table
// 0x89FB40, 0x38 bytes; the bf2 ELF carries the same class with its symbols (0x80C15A0..0x80C4300).
//
// Distributed lightmap generation: a server queues work items (one per traceable object, or one
// per terrain patch) and hands one to each connected lightmap-rendering client as data block 4;
// the client traces it through the renderer's "LightmapGeneration" and answers with ECNetwork
// 0xE (done) or 0xF (failed). In the shipped client the rendering-node switch is never set (its
// command-line option 35 is not registered), so only the server half could run in retail.
#ifndef GAME_DISTRIBUTEDLIGHTRENDERER_DISTRIBUTEDLIGHTRENDERER_H
#define GAME_DISTRIBUTEDLIGHTRENDERER_DISTRIBUTEDLIGHTRENDERER_H

#include "../../Core/IBase.h"
#include <deque>
#include <map>
#include <set>

#if !DEDICATED
namespace dice {
namespace hfe {
namespace world { class IObject; }

// One queued job, 0xC bytes (46C1E0): an object, or a terrain patch when the object is null.
struct DLRWorkItem
{
    world::IObject* object;        // +00
    int             terrainPatch;  // +04, -1 for objects
    unsigned int    id;            // +08, the renderer's running job counter
};

// Data block 4 as assignWorkItem sends it (0x248 bytes). Only the tail is written.
struct DLRWorkItemBlock
{
    unsigned char unused[0x240];   // +000, not written in retail ([CHOSEN] zeroed here)
    unsigned int  id;              // +240, the object's id, or the terrain patch
    bool          isTerrain;       // +244
    unsigned char pad[3];
};

// [INFERRED] interface name and placement; the slot names are the ELF's.
class IDistributedLightRenderer : public IBase
{
public:
    virtual void traceAllObjects() = 0;                             // +10
    virtual void traceTerrain() = 0;                                // +14
    virtual void addRenderResource(int resource) = 0;               // +18
    virtual void removeRenderResource(int resource) = 0;            // +1C
    virtual void workItemFinished(int resource) = 0;                // +20
    virtual void performTraceObject(void* data, unsigned int size) = 0; // +24
    virtual void update() = 0;                                      // +28
protected:
    ~IDistributedLightRenderer() {}
};

class DistributedLightRenderer : public IDistributedLightRenderer
{
public:
    static IBase* create(unsigned int iid, IBase* outer);           // 40FF0E

    DistributedLightRenderer();                                     // 46C350
    virtual ~DistributedLightRenderer();                            // 46C120 (+2C deleting 46C3C0)

    void  addRef() override;                                        // 6D0970
    int   getRef() override;                                        // 68AED0
    int   release() override;                                       // 46C1C0
    void* queryInterface(unsigned int iid) const override;          // 46C190

    void traceAllObjects() override;                                // 46C580
    void traceTerrain() override;                                   // 46C3F0
    void addRenderResource(int resource) override;                  // 46BDC0
    void removeRenderResource(int resource) override;               // 46C250
    void workItemFinished(int resource) override;                   // 46BE80
    void performTraceObject(void* data, unsigned int size) override;// 46B5B0
    void update() override;                                         // 46BED0

private:
    friend struct DistributedLightRendererFixture;
    void addObjectToJobQueue(world::IObject* object, int terrainPatch); // 46C1E0
    void assignWorkItem(int resource, DLRWorkItem* item);               // 46BA30

    int                           m_refCount;       // +04
    unsigned int                  m_nextJobId;      // +08
    std::deque<DLRWorkItem*>      m_jobs;           // +0C
    std::map<int, DLRWorkItem*>   m_resources;      // +20, the job each client is working on
    std::set<int>                 m_idleResources;  // +2C
};

} // namespace hfe
} // namespace dice
#endif
#endif
