// Manages decal templates, pooled instances and per-frame submission queues.
// Ground and object decals share the pool; update and draw also run on menu frames.

#ifdef BF2_IMAGE_RENDDX9

#include "DecalManager.h"
#include "DecalTemplate.h"

#include "../../RendDX9/RendDX9Base.h"                // readBoolSetting, readStringSettingDefault
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"
#include "../../RendMisc/IRenderView.h"
#include "../../RendMisc/IRenderer.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"             // logf
#include "../../../RendDX9Abi/HostFileAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"         // RD9_IID_IBASE
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/PluginEntry.h"          // fileManager

#include <cstdio>
#include <cstdlib>
#include <list>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_rendererBase;                                               // dword_1023B970
}

namespace Rend {
// [CHOSEN, not recovered] Copies the private RenderView class into caller-owned storage.
// Remove this adapter when RenderView is declared in a shared header.
dx9rend::IRenderView* renderViewCopyConstruct(void* storage, const dx9rend::IRenderView& source);
}

namespace geom {

// From ModuleGeom's getClassList (sub_100518A0(821018, "dice.hfe.geom.DecalManager", factory)) and
// its getSingletonList entry { "DecalManager", &g_DecalManager, iid 820018, classId 821018, flags 2 }.
// queryInterface tests iid 1 (dword_102088DC) and 820018 (dword_1021AE44).
extern const unsigned int CID_DecalManager  = 821018u;
extern const unsigned int IID_IDecalManager = 820018u;

namespace {

// unk_1025BEBC - a file-scope std::string built by the static initialiser 0x101D37C0 (destroyed by
// the atexit entry 0x101D96B0). loadAtlas and generateTexture prefix it.
const std::string s_atlasDirectory("Objects/Effects/Decals/Textures/Atlas/");

// The shipped code inlines MSVCP71's operator+ - copy-construct, operator+=, copy out. These carry
// the same steps (UndergrowthSystemLoad.cpp's precedent).
std::string concat(const std::string& left, const char* right)
{
    std::string result(left);
    result += right;
    return result;
}

std::string concat(const std::string& left, const std::string& right)
{
    std::string result(left);
    result += right;
    return result;
}

dx9rend::IRenderer* renderer()
{
    return static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase);
}

// The atlas's table slot +0x08 (0x10017410: `--refCount; if 0 { dtor; free }`) - the destructor
// dispatches it (0x101250A8..0x101250AA). The table is TextureAtlas.cpp's own (both flavours); its
// release has no published seam yet. [CHOSEN, not recovered] - retire with a typed
// `textureAtlasRelease` in TextureAtlas.h.
int releaseAtlas(dx9rend::TextureAtlas* atlas)
{
    typedef int (__thiscall* ReleaseFn)(void*);
    return reinterpret_cast<ReleaseFn>(static_cast<void**>(atlas->vptr)[0x08 / 4])(atlas);
}

} // anonymous namespace


// Initializes decal resources and pool, then derives fade times from their defaults.
// The listener's render-path field and padding remain untouched.

DecalManager::DecalManager()
    : m_refCount(1)
    , m_timeToLive(20.0f)
    , m_fadeOutAtPercentageOfTimeToLive(0.85f)
    , m_lastDeltaTime(0.0f)
    , m_byte24(0)
    , m_clients()
    , m_activeInstances()
    , m_templates()
    , m_deleteQueue()
    , m_removeQueue()
    , m_templatesByIndex()
    , m_map78()
    , m_map84()
    , m_renderer()
    , m_freeList(0)
    , m_poolSize(0)
    , m_templateCount(0)
    , m_dword29C(0x400)
    , m_dword2A0(0x400)
    , m_clearing(false)
    , m_drawStats(false)
    , m_enabled(true)
    , m_fadeDistance(50.0f)
    , m_startFadeDistance(30.0f)
    , m_minimumAlpha(0.05f)
    , m_startFadePercentageOfFadeDistance(0.85f)
    , m_decalNormalOffset(0.01f)
    , m_decalGeomClipAngle(60.0f)
    , m_atlas(0)
{
    void* const storage = ::renddx9abi::engineAlloc(
        static_cast<unsigned int>(sizeof(dx9rend::TextureAtlas)));         // sub_1005F320(0x14)
    m_atlas = storage ? dx9rend::textureAtlasCtor(storage) : 0;
    initPool();
    recomputeFades();                                                      // inline, 0x101253C3..
}


// the destructor: flushDeletes, clear, the atlas released through its table
// (unguarded - a failed atlas allocation faults here, as shipped), then the members in reverse
// declaration order: the renderer (0x101AA4F0), the maps +0x84 / +0x78 / +0x6C, the vectors +0x5C /
// +0x4C, the maps +0x40 / +0x34 / +0x28 - the std containers' own destructors.

DecalManager::~DecalManager()
{
    flushDeletes();
    clear();
    releaseAtlas(m_atlas);                                                 // [ecx] +0x08
}

// IBase

// the image's shared `+0x0C` counter bodies.
void DecalManager::addRef()
{
    ++m_refCount;
}

int DecalManager::getRef()
{
    return m_refCount;
}

// the decremented count is cached before the object is destroyed and freed
// (sub_1005F360), and returned.
int DecalManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~DecalManager();                                             // DecalManager_dtor
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// this for iid 1 or 820018, else 0 (branch-free `setnz; sub; and`).
void* DecalManager::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE)
        return const_cast<DecalManager*>(this);
    return iid == IID_IDecalManager ? const_cast<DecalManager*>(this) : 0;
}

// IDecalManager

// +0x10, nullsub_59 (0x101240F0, `retn`).
void DecalManager::reserved10()
{
}


// +0x14, createClient(owner): 0 while decals are disabled (+0x2A6);
// else a 0x44-byte DecalObject (operator new, D1's constructor 0x101AC7B0 with the owner), inserted
// into the client set as (object, object) - also when the allocation failed and the object is 0.

void* DecalManager::createClient(void* owner)
{
    if (!m_enabled)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(
        static_cast<unsigned int>(sizeof(DecalObject)));                   // sub_1005F320(0x44)
    DecalObject* const object =
        storage ? new (storage) DecalObject(static_cast<world::IObject*>(owner)) : 0;
    m_clients.insert(DecalObjectMap::value_type(object, object));
    return object;
}

// +0x18, the deferred delete (DecalObject slot +0x28 calls it with
// itself): queued unless clear() is running; update's flushDeletes destroys it.
void DecalManager::enqueue(void* decalObject)
{
    if (!m_clearing)                                                       // +0x2A4
        m_deleteQueue.push_back(static_cast<DecalObject*>(decalObject));
}


// +0x1C, the template for `name`, created on first use:
//   find (the _stricmp map, sub_10036390) -> the existing template;
//   else, at 0x200 templates or more, the debug stream reports "To many decalManager templates"
//   (line 159) and the creation goes ON; a new DecalTemplate(name) (0xA8 bytes) is filed under the
//   name (operator[]), takes the running count as its index (+0x58, written through the new
//   pointer with no null test), the count is incremented, and the index map files it too.

IDecalTemplate* DecalManager::addTemplate(const DecalTemplateString& name)
{
    // The key is this image's own string; fromHostString converts the host string once here.
    const std::string& key = ::renddx9abi::fromHostString(name);

    const DecalTemplateMap::iterator found = m_templates.find(key);
    if (found != m_templates.end())
        return found->second;                                              // node +0x28

    if (m_templateCount >= 0x200)
    {
        // sub_10074300(6, __FILE__, 159, "") << "To many decalManager templates"; sub_10074690.
        // [CHOSEN, not recovered] rendering through logf, as everywhere in this image.
        ::renddx9abi::logf("DecalManager.cpp:159 To many decalManager templates");
    }

    DecalTemplate* const created = new DecalTemplate(name);                // 0xA8, sub_101AFA70
    m_templates[key] = created;
    created->setIndex(m_templateCount);                                    // +0x58
    ++m_templateCount;
    m_templatesByIndex[created->getIndex()] = created;
    return created;
}

// +0x20, nullsub_60 (0x10124100, `retn 4`).
void DecalManager::reserved20(void* argument)
{
}


// +0x24, addInstance(object): an object already flagged active (+0x04)
// is reported (line 274) and handled again; the flag is set; an object already in the active set is
// reported (line 283); then (object, object) is inserted - a duplicate insert changes nothing.

void DecalManager::addInstance(void* instance)
{
    DecalObject* const object = static_cast<DecalObject*>(instance);
    if (object->activeFlag())
        ::renddx9abi::logf("DecalManager.cpp:274 Trying to add instance twice");

    object->activeFlag() = true;
    if (m_activeInstances.find(object) != m_activeInstances.end())
        ::renddx9abi::logf("DecalManager.cpp:283 Trying to add already activeInstances");

    m_activeInstances.insert(DecalObjectMap::value_type(object, object));
}

// +0x28, the active flag cleared and the object queued; update's
// flushRemovals takes it out of the set. The DecalObject destructor (0x101AEC80) calls this too.
void DecalManager::removeInstance(void* decalObject)
{
    DecalObject* const object = static_cast<DecalObject*>(decalObject);
    object->activeFlag() = false;
    m_removeQueue.push_back(object);                                       // inline / sub_10125EA0
}

// +0x2C, relink every pool record into the free list, in order, and point
// the list at the first. It does NOT free the records' vertex blocks or touch any DecalObject.
// unsigned: poolSize - 1 is the loop bound, so a pool of 1 skips the loop and a pool of 0 would
// run it ~2^32 times - neither happens (initPool sets 2048).
void DecalManager::resetPool()
{
    for (unsigned int i = 0; i < m_poolSize - 1u; ++i)
        m_pool[i].next = &m_pool[i + 1];
    m_pool[m_poolSize - 1u].next = 0;                                      // [pool + 88n - 88]
    m_freeList = m_pool;                                                   // +0x290
}


// Flushes pending deletes and removals, updates active decals, then flushes new removals.
// The renderer view lookup is retained even though its result is unused.

void DecalManager::update(float deltaTime)
{
    flushDeletes();
    flushRemovals();
    renderer()->getRenderViewDup();                                        // +0x50, answer unused
    m_lastDeltaTime = deltaTime;                                           // +0x20
    for (DecalObjectMap::iterator it = m_activeInstances.begin(); it != m_activeInstances.end();
         ++it)
        it->second->update(deltaTime);               // [value] +0x24
    flushRemovals();
}


// Setters return void; getters return float in st(0) (RW-153).
// Setters recompute the dependent fade values after updating their inputs.


// +0x34, timeToLive (+0x10).
void DecalManager::setTimeToLive(float distance)
{
    m_timeToLive = distance;
    recomputeFades();
}

// +0x38, 0x101241D0 - `fld [ecx+10h]`.
float DecalManager::getTimeToLive() const
{
    return m_timeToLive;
}

// +0x3C, fadeDistance (+0x2B0).
void DecalManager::setFadeDistance(float distance)
{
    m_fadeDistance = distance;
    recomputeFades();
}

// +0x40, 0x10124270 - `fld [ecx+2B0h]`.
float DecalManager::getFadeDistance() const
{
    return m_fadeDistance;
}

// Recomputes fades without reading percentage; the stored percentage remains unchanged.
void DecalManager::setStartFadePercentageOfFadeDistance(float percentage)
{
    (void)percentage;                                                      // never read - shipped
    recomputeFades();
}

// +0x48, 0x101242B0 - `fld [ecx+2BCh]`.
float DecalManager::getStartFadePercentageOfFadeDistance() const
{
    return m_startFadePercentageOfFadeDistance;
}

// +0x4C, fadeOutAtPercentageOfTimeToLive (+0x1C).
void DecalManager::setFadeOutAtPercentageOfTimeToLive(float ratio)
{
    m_fadeOutAtPercentageOfTimeToLive = ratio;
    recomputeFades();
}

// +0x50, 0x10124220 - `fld [ecx+1Ch]`.
float DecalManager::getFadeOutAtPercentageOfTimeToLive() const
{
    return m_fadeOutAtPercentageOfTimeToLive;
}

// +0x54, nullsub_62 (0x10124150, `retn`).
void DecalManager::reserved54()
{
}

// dx9rend::RenderPathListener

// +0x00 nullsub_20 (`retn 4`) - unlike SkyDome's, it does NOT store the path: +0x08 is never
// written by anything.
void DecalManager::onRenderPathActivated(int path)
{
}

// +0x04 nullsub_63 (`retn`).
void DecalManager::onRenderPathDeactivated()
{
}

// +0x08 nullsub_64 (`retn 4`).
void DecalManager::onRenderPathChanged(int path)
{
}


// +0x0C, the per-frame decal draw:
// collect over a stack copy of the renderer's view (+0x48), then render with the renderer's view
// fetched again. The two halves are the unreferenced 0x10124570 and 0x10124110, inlined here.

void DecalManager::onRenderPathNotification(int unused)
{
    collectDecals(*renderer()->getRenderView());                           // +0x48, 0x10124570 body
    renderDecals(renderer()->getRenderView());                             // +0x48, 0x10124110 body
}

// +0x10, 0x101A9A10 - `mov eax, [ecx+4]` on the subobject: +0x08 - never written (see the header).
int DecalManager::getCurrentRenderPath() const
{
    return m_renderPath;
}

// +0x14, std::string("DecalRenderer client").
std::string DecalManager::getRenderPathListenerName() const
{
    return std::string("DecalRenderer client");
}

// non-virtual

// 0x10124110 - `add ecx, 94h; jmp sub_101AA670`: the renderer's draw with the given view. No
// reference in the image; its body is the listener's second half.
void DecalManager::renderDecals(dx9rend::IRenderView* view)
{
    m_renderer.render(view);
}

// (nullsub_9, `retn 4`) - the console's `drawStats` setter (0x100BC19F). empty: the
// console can read drawStats (+0x2A5) but never change it. Shipped.
void DecalManager::setDrawStats(bool enable)
{
}

// the derivation every setter inlines:
//     fadeOutTime       = fadeOutAtPercentage * timeToLive       (+0x14 = +0x1C * +0x10)
//     fadeOutDuration   = timeToLive - fadeOutTime               (+0x18)
//     startFadeDistance = startFadePercentage * fadeDistance     (+0x2B4 = +0x2BC * +0x2B0)
void DecalManager::recomputeFades()
{
    m_fadeOutTime = m_fadeOutAtPercentageOfTimeToLive * m_timeToLive;
    m_fadeOutDuration = m_timeToLive - m_fadeOutTime;
    m_startFadeDistance = m_startFadePercentageOfFadeDistance * m_fadeDistance;
}

// the DecalAtlas entry for a texture name: 0 when the atlas holds no
// entry at all (its map size, `cmp [eax+10h], 0; ja`), else the atlas's lookup of name + ".dds".
// The atlas pointer is not tested. Caller: DecalTemplate::loadTexture (0x101AF89D).
dx9rend::TextureAtlasEntry* DecalManager::getAtlasEntry(const std::string& textureName)
{
    if (dx9rend::textureAtlasEntryCount(m_atlas) == 0)
        return 0;
    const std::string key = concat(textureName, ".dds");
    return dx9rend::textureAtlasFind(m_atlas, key);
}

// load "<atlas directory><fileName>" into the atlas; false for an empty
// name. Caller: initPool.
bool DecalManager::loadAtlas(const std::string& fileName)
{
    if (fileName.size() == 0)
        return false;
    const std::string path = concat(s_atlasDirectory, fileName);          // unk_1025BEBC + name
    return dx9rend::textureAtlasLoadTAI(m_atlas, path);
}


// the 2048-record pool: poolSize = 0x800 (stored first), `new
// DecalRecord[2048]` (0x2C000 bytes, sub_1005F340, the seven-store constructor inlined; no cookie),
// the records chained in order with the last one's link 0, the free list at the first; then, when
// the host's "GSUseEffectTextureAtlas" setting is true (SettingsRepostitory +0x50, default false),
// loadAtlas("DecalAtlas.tai").

void DecalManager::initPool()
{
    m_poolSize = 0x800;
    m_pool = new DecalRecord[2048];
    for (unsigned int i = 0; i < m_poolSize - 1u; ++i)
        m_pool[i].next = &m_pool[i + 1];
    m_pool[m_poolSize - 1u].next = 0;
    m_freeList = m_pool;

    if (dx9rend::readBoolSetting("GSUseEffectTextureAtlas", false))       // +0x50
        loadAtlas(std::string("DecalAtlas.tai"));
}

// the listener's first half: a stack copy of `view`
// (Rend::RenderView's copy constructor, 0x10125960), and the renderer's collect over it and the
// active set.
void DecalManager::collectDecals(dx9rend::IRenderView& view)
{
    union
    {
        double        alignment;
        unsigned char bytes[dx9rend::kRenderViewObjectBytes];             // [ebp-430h], 1072 in retail
    } storage;
    dx9rend::IRenderView* const copy = Rend::renderViewCopyConstruct(storage.bytes, view);
    m_renderer.collect(*copy, m_activeInstances);
}


// the deferred deletes: for each queued object, in queue order (the end
// re-read every step): removeInstance (this object's +0x28, a virtual call) when it is still active;
// then, when the client set holds it, erase it there and destroy the object (its +0x00 with 1).
// An object the client set does not hold is NOT destroyed (it leaks) - shipped. The queue is then
// resized to 0 (VC7.1 `resize(0)` - the buffer is kept).

void DecalManager::flushDeletes()
{
    for (std::vector<DecalObject*>::iterator it = m_deleteQueue.begin(); it != m_deleteQueue.end();
         ++it)
    {
        DecalObject* const object = *it;
        if (object->activeFlag())
            removeInstance(object);                                        // [this] +0x28
        const DecalObjectMap::iterator found = m_clients.find(object);
        if (found != m_clients.end())
        {
            m_clients.erase(found);
            delete object;                         // [object] +0x00 (1)
        }
    }
    m_deleteQueue.resize(0);                                               // sub_101262A0(0, 0)
}

// the deferred removals: each queued object erased from the active set
// when present, then the queue resized to 0.
void DecalManager::flushRemovals()
{
    for (std::vector<DecalObject*>::iterator it = m_removeQueue.begin(); it != m_removeQueue.end();
         ++it)
    {
        const DecalObjectMap::iterator found = m_activeInstances.find(*it);
        if (found != m_activeInstances.end())
            m_activeInstances.erase(found);
    }
    m_removeQueue.resize(0);                                               // sub_101262A0(0, 0)
}


// drop everything; called only by the destructor. clearing = 1; every
// template destroyed (+0xA0 with 1), every client destroyed (+0x00 with 1 - whose destructor calls
// removeInstance, which still queues); the delete queue resized to 0; the maps +0x40, +0x6C, +0x28,
// +0x34 and +0x78 cleared, in that order (+0x84 and the remove queue are left); templateCount = 0;
// the renderer's empty reset (0x101A9AA0); the pool `delete[]`d (sub_1005F370) - the pool pointer
// is NOT cleared; clearing = 0.

void DecalManager::clear()
{
    m_clearing = true;
    for (DecalTemplateMap::iterator it = m_templates.begin(); it != m_templates.end(); ++it)
    {
        if (it->second)
            delete it->second;                                             // [t] +0xA0 (1)
    }
    for (DecalObjectMap::iterator it = m_clients.begin(); it != m_clients.end(); ++it)
    {
        if (it->second)
            delete it->second;                     // [o] +0x00 (1)
    }
    m_deleteQueue.resize(0);                                               // sub_101262A0(0, 0)
    m_templates.clear();
    m_templatesByIndex.clear();                                            // inline _Erase
    m_clients.clear();
    m_activeInstances.clear();
    m_map78.clear();
    m_templateCount = 0;
    m_renderer.reset();                                                    // nullsub_13
    delete[] m_pool;
    m_clearing = false;
}

// (no reference) - `return templatesByIndex[index]` - operator[], so an unknown index
// files a null entry.
DecalTemplate* DecalManager::getTemplateByIndex(unsigned short index)
{
    return m_templatesByIndex[index];
}


// Builds the decal atlas from named template textures beneath the editor save path.
// Returns false only for an empty list; downstream builder failures are not propagated.

bool DecalManager::generateTexture(const std::string& atlasName)
{
    std::list<std::string> files;
    for (DecalTemplateMap::iterator it = m_templates.begin(); it != m_templates.end(); ++it)
    {
        IDecalTemplate* const decalTemplate = it->second;                  // node +0x28
        if (decalTemplate->getDecalTextureName().size() > 0)               // +0x54
            files.push_back(concat(::renddx9abi::fromHostString(
                decalTemplate->getDecalTextureName()), ".dds"));           // +0x54 again
    }

    std::string modDirectory;
    dx9rend::readStringSettingDefault("GSModDirectory", "mods/bf2", modDirectory);        // +0x40
    std::string levelSavePath;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", modDirectory.c_str(),
                                      levelSavePath);                                      // +0x40

    const std::string taiPath =
        concat(concat(concat(concat(levelSavePath, "/"), s_atlasDirectory), atlasName), ".tai");
    ::renddx9abi::HostStream* const stream = ::dice::hfe::fileManager->open(
        ::renddx9abi::makeHostString(taiPath), static_cast<renddx9abi::HostFileMode>(0),
        static_cast<renddx9abi::HostFileAccess>(2), 0);                   // +0x18
    if (stream)
        stream->release();                                                 // +0x08

    return dx9rend::textureAtlasGenerate(m_atlas, levelSavePath, concat(s_atlasDirectory, atlasName),
                                         levelSavePath, files, false);
}


// RefFactory_geom_DecalManager, allocate 0x2CC (sub_1005F320), construct, ask for the
// interface, release on a mismatch. The constructed pointer is NOT tested before its
// queryInterface (0x100B0779 `mov eax, [esi]` with esi possibly 0) - a failed allocation faults, as
// shipped. No memset: the fields the constructor skips hold engine-heap bytes, as in retail.

IBase* __fastcall createDecalManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(static_cast<unsigned int>(sizeof(DecalManager)));
    DecalManager* const self = storage ? new (storage) DecalManager() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}


// Direct decal-manager state and pool access shared with DecalObject.

namespace decal_manager_state
{
namespace {
DecalManager* manager()
{
    return static_cast<DecalManager*>(static_cast<IDecalManager*>(g_DecalManager));
}
}

bool         decalsEnabled()                 { return manager()->isEnabled(); }            // +0x2A6
DecalRecord* allocateRecord()                { return manager()->popFreeRecord(); }        // +0x290
void         freeRecord(DecalRecord* record) { manager()->freeRecord(record); }
float        surfaceOffset()                 { return manager()->decalNormalOffset(); }    // +0x2C0
float        maxProjectionAngle()            { return manager()->decalGeomClipAngle(); }   // +0x2C4
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
