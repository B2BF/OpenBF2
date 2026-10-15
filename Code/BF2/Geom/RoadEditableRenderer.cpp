// Draws editable roads from a copied queue; the queue is normally empty in the game.


#ifdef BF2_IMAGE_RENDDX9

#include "RoadEditableRenderer.h"
#include "RoadTemplate.h"                 // IRoadTemplate::getPriority (+0x4C)
#include "XpLightManager.h"               // lightManagerSetRenderTargetTexture (sub_10005920)

#include "../RendDX9/RendDX9Base.h"       // rendererGetHemiScratchTexture, stateManagerClear
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/ModuleCommon.h"

#include <new>

namespace dice {
namespace hfe {
namespace geom {

// dword_1021AEC8 - 821913, read as data by getClassList (0x100C8315).
extern const unsigned int CID_RoadEditableRenderer = 821913u;

namespace {

// dword_1021AE4C - 820020, the one id queryInterface tests after IBase's 1; read as data.
const unsigned int kIID_IRoadEditableRenderer = 820020u;

// drawAll's "no priority seen yet" sentinel for both minima.
const int kNoPriority = 999999999;

} // anonymous namespace

// vptr, refCount 1, the flag byte 0, the empty list, +0x18 = 0.
RoadEditableRenderer::RoadEditableRenderer()
    : m_refCount(1)
    , m_flag08(0)
    , m_mode(0)
{
}

void RoadEditableRenderer::addRef()
{
    ++m_refCount;                                                            // (shared)
}

int RoadEditableRenderer::getRef()
{
    return m_refCount;                                                       // (shared)
}

// 0x10144760: `--ref`; at zero the vptr is restored, the list destroyed (0x1000EAD0) and the block
// returned to the engine heap.
int RoadEditableRenderer::release()
{
    const int count = --m_refCount;
    if (!count)
    {
        this->~RoadEditableRenderer();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// 0x10144670: 1 answers `this`; otherwise the branch-free "820020 ? this : 0".
void* RoadEditableRenderer::queryInterface(unsigned int iid) const
{
    RoadEditableRenderer* const self = const_cast<RoadEditableRenderer*>(this);
    if (iid == RD9_IID_IBASE)
        return static_cast<IRoadEditableRenderer*>(self);
    return iid == kIID_IRoadEditableRenderer ? static_cast<IRoadEditableRenderer*>(self) : 0;
}

// find(begin, end, road); only when absent, push_back (_Buynode(head, head->prev) and
// the _Incsize test).
void RoadEditableRenderer::add(IRoadEditable* road)
{
    for (RoadEditableList::iterator node = m_roads.begin(); node != m_roads.end(); ++node)
    {
        if (*node == road)
            return;
    }
    m_roads.push_back(road);
}

// erase(find(begin, end, road)): the first match only; the shipped erase's own `node != head`
// guard makes an absent road a no-op.
void RoadEditableRenderer::remove(IRoadEditable* road)
{
    RoadEditableList::iterator node = m_roads.begin();
    while (node != m_roads.end() && *node != road)
        ++node;
    if (node != m_roads.end())
        m_roads.erase(node);
}

// Draws a copy of the road queue by template priority, then road priority within a template.
// Changes to the original queue during draw do not affect this pass. Clears the coverage
// mask before drawing. At least one remaining template priority must be below 999999999;
// otherwise winner selection leaves a null pointer. Priority calls remain repeated.
void RoadEditableRenderer::drawAll()
{
    RoadEditableList queue(m_roads);
    if (!queue.empty())
    {
        lightManagerSetRenderTargetTexture(0, dx9rend::rendererGetHemiScratchTexture());
        dx9rend::stateManagerClear(1u, 0x00FFFFFFu, 0.0f, 0u);
        lightManagerSetRenderTargetTexture(0, 0);
        do
        {
            // No winner leaves `best` singular and the draw below dereferences it, as shipped
            // (retail: a null node).
            RoadEditableList::iterator best = RoadEditableList::iterator();
            IRoadTemplate* bestTemplate = 0;
            int lowestTemplatePriority = kNoPriority;
            int lowestRoadPriority = kNoPriority;
            for (RoadEditableList::iterator node = queue.begin(); node != queue.end(); ++node)
            {
                IRoadEditable* const road = *node;
                IRoadTemplate* const roadTemplate = road->getTemplate();         // +0x20
                if (roadTemplate->getPriority() < lowestTemplatePriority)         // +0x4C
                {
                    lowestRoadPriority = road->getPriority();                     // +0x40
                    lowestTemplatePriority = roadTemplate->getPriority();
                    bestTemplate = roadTemplate;
                    best = node;
                }
                else if (roadTemplate == bestTemplate &&
                         road->getPriority() < lowestRoadPriority)
                {
                    lowestRoadPriority = road->getPriority();
                    best = node;
                }
            }
            (*best)->draw();                                                      // +0x10
            queue.erase(best);
        }
        while (!queue.empty());
    }
}

void RoadEditableRenderer::setFlag08(bool value)    { m_flag08 = value ? 1 : 0; }
bool RoadEditableRenderer::getFlag08()              { return m_flag08 != 0; }


// RefFactory refuse aggregation, the object (retail 0x1C bytes), construct, QI with NO null
// test on the object (shipped), release on refusal.

IBase* __fastcall createRoadEditableRenderer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(RoadEditableRenderer));
    RoadEditableRenderer* self = 0;
    if (storage)
        self = new (storage) RoadEditableRenderer();

    IBase* const result = static_cast<IBase*>(self->queryInterface(iid));    // [eax+0Ch]
    if (!result)
        self->release();                                                     // [eax+8]
    return result;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
