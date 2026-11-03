// Ambient effect-area activation, template settings and area registration.

#include "AmbientEffectArea.h"
#include "../../../IO/Stream/IStream.h"
#include <sstream>
#include "AmbientEffectAreaManager.h"

#include "../../../Core/Ids.h"
#include "../IObject.h"
#include "../../../Math/Vec3.h"
#include "../../../Core/Random.h"
#include "../../../Core/System.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../GameEvents/AmbientEffectAreaEvent.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"
#include "../IPlayer.h"
#include "../ObjectLayoutAccess.h"
#include "../Fx/EffectBundle.h"
#include "../Fx/EffectBundleTemplate.h"
#include "../../../Core/Debug.h"
#include "../../Main/Game.h"                                            // isDedicated (440510), E3
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h" // the linked-template lookup

#include <cmath>

namespace dice {
namespace hfe {
extern IGameLogic* gameLogic;
namespace world {
extern IBase* heightmapCluster;

// The TU's id counter (no ELF symbol; ds:0x8AACDE8): post-incremented by every ctor, reset by the
// manager's removeAll. Starts at 0 - .bss.
int ambientEffectAreaNextId = 0;

// ELF C1 8164320 / PE 5895D0: default-construct both indexes, first singleton wins.
AmbientEffectAreaManager::AmbientEffectAreaManager()
{
    if (!ambientEffectAreaManager)
        ambientEffectAreaManager = this;
}

// ELF D1 8164490 / PE 5890F0: relinquish the singleton only if it is ours.
// Member teardown frees the indexes, never their objects, and does not reset ids.
AmbientEffectAreaManager::~AmbientEffectAreaManager()
{
    if (ambientEffectAreaManager == this)
        ambientEffectAreaManager = 0;
}

// ELF 8164560 / PE 588EF0: object destruction precedes this in GameLogic::unload.
void AmbientEffectAreaManager::removeAll()
{
    m_areas.clear();
    m_areasById.clear();
    ambientEffectAreaNextId = 0;
}

// ELF 81645F0; PE inlined in AmbientEffectArea::init at 589799..5897BF.
// The id assignment runs even when the pointer was already in the set.
void AmbientEffectAreaManager::addAmbientEffectArea(AmbientEffectArea* area)
{
    m_areas.insert(area);
    m_areasById[area->m_id] = area;
}

// ELF 81646B0 / PE 588F90. Retail requires a registered pointer: erase(find())
// has no end check. It collects every matching map iterator before erasing any;
// removal is by pointer, not by the object's current id.
void AmbientEffectAreaManager::removeAmbientEffectArea(AmbientEffectArea* area)
{
    m_areas.erase(m_areas.find(area));
    typedef std::map<int, AmbientEffectArea*>::iterator AreaIterator;
    std::list<AreaIterator> remove;
    for (AreaIterator it = m_areasById.begin(); it != m_areasById.end(); ++it)
        if (it->second == area)
            remove.push_back(it);
    for (std::list<AreaIterator>::iterator it = remove.begin(); it != remove.end(); ++it)
        m_areasById.erase(*it);
}

// ELF 8164830 / PE 589160: append queried IObject interfaces in set order.
// The result describes the entire output vector, including entries already there.
bool AmbientEffectAreaManager::getAreasWithTriggerId(int triggerId, std::vector<IObject*>& objects)
{
    for (std::set<AmbientEffectArea*>::iterator it = m_areas.begin(); it != m_areas.end(); ++it)
        if ((*it)->getTriggerId() == triggerId)
            objects.push_back(static_cast<IObject*>((*it)->queryInterface(IID_IObject)));
    return !objects.empty();
}

// ELF 81648E0 / PE 588F50: a signed key lookup that does not insert on a miss.
AmbientEffectArea* AmbientEffectAreaManager::getAreaFromId(int id)
{
    std::map<int, AmbientEffectArea*>::iterator it = m_areasById.find(id);
    return it == m_areasById.end() ? 0 : it->second;
}

// AmbientEffectAreaTemplate

// (C2) / 0x8165D80 (C1), 143 B: BundleTemplate, the four vptrs, then refcount 1,
// radius 0, triggerId -1, numNetworkedEffects 0, the +0x188 byte 0, projectOnHeightmap true, the
// empty string, attachToEventObject true (the ctor's own store order).
AmbientEffectAreaTemplate::AmbientEffectAreaTemplate()
{
    m_refCount = 1;                                  // +0x17C
    m_radius = 0.0f;                                 // +0x180
    m_triggerId = -1;                                // +0x184
    m_playAtEventPosition = false;                   // +0x188
    m_numNetworkedEffects = 0;                       // +0x18C
    // m_linkedEffectContainer (+0x190): default-constructed empty - the ELF binds the shared
    // empty _Rep (0x8990C4C) with an atomic increment, which is what an empty std::string is.
    m_projectOnHeightmap = true;                     // +0x194
    m_attachToEventObject = true;                    // +0x195
}

// (D1) / 112 B (D0): vptr restores, the string's _Rep release, BundleTemplate's dtor.
AmbientEffectAreaTemplate::~AmbientEffectAreaTemplate() {}

unsigned int AmbientEffectAreaTemplate::getClassID() const { return CID_AmbientEffectAreaTemplate; }

// the one-cid hop into Bundle's chain.
void* AmbientEffectAreaTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_AmbientEffectAreaTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

// new(0x158) AmbientEffectArea(this).
IObject* AmbientEffectAreaTemplate::createObject() const
{
    return new AmbientEffectArea(this);
}

// The re-implemented quartet - refcount at +0x17C (TemplateLeaves.cpp's BF2_LEAF_IBASE_IMPL shape);
// queryInterface is the standard bundle-leaf table, which BundleTemplate's own answers.
void AmbientEffectAreaTemplate::addRef()  { ++m_refCount; }
int  AmbientEffectAreaTemplate::getRef()  { return m_refCount; }
int  AmbientEffectAreaTemplate::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}
void* AmbientEffectAreaTemplate::queryInterface(unsigned int iid) const { return BundleTemplate::queryInterface(iid); }

// AmbientEffectArea

// Shared effect randomization draws the relative sign before the remaining placement values.
namespace {
// [INFERRED helper] The inlined Random draw in ELF 8164D50 and PE 5897D0 (client 43FFA0 on
// this+0x168, then `(raw | 0x80) >> 7) + 1` - the same value).
// Keep the existing scalar member representation; write back after every draw.
unsigned int ambientRandom(int& seed)
{
    Random random;
    random.m_seed = static_cast<unsigned int>(seed);
    const unsigned int raw = random.getRaw();
    seed = static_cast<int>(random.m_seed);
    return ((raw >> 7) | 1u) + 1u;
}

// PE 588B30 -> 588AD0/5807E0/5889E0/588A10 (client 5E74B0 -> 5E7450). The process-global Random is
// used, never the seed embedded in the distribution. PE draws the relative sign first;
// ELF 81658B8..5BE9 samples the distribution, materializes float, then draws its sign.
double ambientUnitRandom()
{
    return static_cast<double>((static_cast<int>(Rand.getRaw() | 0x80u) >> 7) + 1)
        * (1.0 / 16777216.0);
}
double ambientLifetimeSample(const RandomDistribution& distribution)
{
#if defined(_WIN32)
    const double sign = distribution.relative && ambientUnitRandom() <= 0.5 ? -1.0 : 1.0;
#endif
    double value;
    switch (distribution.distribution)
    {
    case CRD_NONE: value = distribution.value; break;
    case CRD_UNIFORM:
        value = distribution.value + (double(distribution.variation) - distribution.value) * ambientUnitRandom();
        break;
    case CRD_EXPONENTIAL: value = -double(distribution.value) * std::log(ambientUnitRandom()); break;
    case CRD_NORMAL: value = distribution.value + double(Rand.getNormal()) * distribution.variation; break;
    default: value = 0; break;
    }
#if defined(_WIN32)
    return value * sign;
#else
    if (distribution.relative)
    {
        value = static_cast<float>(value);
        if (ambientUnitRandom() <= 0.5) value = -value;
    }
    return value;
#endif
}
}

// (C2) / 0x81649D0 (C1), 153 B, read in full: Bundle, the two vptrs, refcount 1, the
// template, the list head (allocated), +0x148 = 0x759DC150, +0x150/+0x154 = 0, and the id from the
// TU counter (read, then incremented).
AmbientEffectArea::AmbientEffectArea(const AmbientEffectAreaTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    m_refCount = 1;                                  // +0x13C
    m_template = objectTemplate;                     // +0x140
    // m_effects (+0x144): the SGI head node the ELF allocates is the empty list.
    m_lastRandom = 0x759DC150;                       // +0x148
    m_effectContainer = 0;                           // +0x150
    m_effectTemplate = 0;                            // +0x154
    m_id = ambientEffectAreaNextId++;                // +0x14C <- ds:0x8AACDE8, then ++

    // On clients, resolves the linked effect container or template and initializes its child objects.
    // Lifetime sampling is retained even when its result is discarded because it advances the shared random state.
    if (isDedicated())
        return;
    static const char* const file = "Game/Objects/Game/AmbientEffectArea.cpp";
    if (m_template->m_linkedEffectContainer.empty())                          // template +0x254
    {
        Debug(DebugTypeError, file, 180, "") << "AmbientEffectArea has no linked effect."
            << " Area:" << m_template->getName();
        return;
    }
    m_effectTemplate = objectTemplateManager->getTemplate(m_template->m_linkedEffectContainer);  // PE +0x34
    if (!m_effectTemplate)
    {
        Debug(DebugTypeError, file, 173, "") << "Could not instantiate linked effect template."
            << " Area:" << m_template->getName()
            << " Linked template name: " << m_template->m_linkedEffectContainer;
        return;
    }
    if (m_effectTemplate->getClassID() != CID_BundleTemplate)                // ds:91C1D8
    {
        if (m_effectTemplate->getClassID() == CID_EffectBundleTemplate)      // ds:91C1E4
        {
            ambientLifetimeSample(
                static_cast<const EffectBundleTemplate*>(m_effectTemplate)->m_dist2E0);  // PE +0x390
            return;
        }
        Debug(DebugTypeError, file, 167, "")
            << "AmbientEffectArea linkedEffectContainer must be of type BundleTemplate or "
               "EffectBundleTemplate."
            << " Area:" << m_template->getName();
        return;
    }
    IObject* const container = m_effectTemplate->createObject();              // +0x50
    if (!container)
    {
        // This one names the area through the object's template pointer (+0x14), and " Area: "
        // keeps its trailing space - a different literal (ds:8EDDB4) from the others' " Area:".
        Debug(DebugTypeError2, file, 155, "") << "Failed to create effectcontainer of non-effectbundle type."
            << " Area: " << object_detail::LayoutAccess::objectTemplate(this)->getName();
        return;
    }
    container->init();                                                        // +0x1C
    m_effectContainer = container;
    addChild(container);
    for (IObject* child = container->getChild(); child; child = child->getSibling())   // +0xEC +0x2C
    {
        const IObjectTemplate* const childTemplate = object_detail::LayoutAccess::objectTemplate(child);
        if (childTemplate->getClassID() == CID_EffectBundleTemplate)
            ambientLifetimeSample(static_cast<const EffectBundleTemplate*>(childTemplate)->m_dist2E0);
    }
}

// (D1) / 0x8164C10 (D0), 202 / 210 B: vptr restores; for every WeakPtr in m_effects whose
// target is still alive, target->destroy(true) (slot 13, +0x34); clear the list; `if (manager)
// removeAmbientEffectArea(this)`; clear again and free the head node; Bundle's dtor (D0 adds
// operator delete). The list is cleared twice in the ELF - once before the manager call, once
// after - which is idempotent; the member's own teardown covers both.
AmbientEffectArea::~AmbientEffectArea()
{
    for (std::list<WeakPtr<IObject> >::iterator it = m_effects.begin(); it != m_effects.end(); ++it)
    {
        IObject* effect = it->get();
        if (effect)
            effect->destroy(true);
    }
    m_effects.clear();
    if (ambientEffectAreaManager)
        ambientEffectAreaManager->removeAmbientEffectArea(this);
    m_effects.clear();
}

// The quartet - refcount at +0x13C, the ControlPoint arrangement.
void AmbientEffectArea::addRef()  { ++m_refCount; }
int  AmbientEffectArea::getRef()  { return m_refCount; }
int  AmbientEffectArea::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}

// , three cases: IBase / IObject -> this; IConsoleSaveable -> the +0x80 sub-object; else 0.
void* AmbientEffectArea::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)   return const_cast<AmbientEffectArea*>(this);
    if (iid == IID_IObject) return const_cast<AmbientEffectArea*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<AmbientEffectArea*>(this));   // this + 0x80
    return 0;
}

// Bundle::init, then `if (manager) manager->addAmbientEffectArea(this)` - the
// manager is constructed by GameLogic. Object.create's init() call lands here.
void AmbientEffectArea::init()
{
    Bundle::init();
    if (ambientEffectAreaManager)
        ambientEffectAreaManager->addAmbientEffectArea(this);
}

// the bare forward to the base the vtable already had there (Bundle has no destroy).
void AmbientEffectArea::destroy(bool force) { SimpleObject::destroy(force); }

// only message 0 reaches the virtual trigger; anything else is dropped.
void AmbientEffectArea::handleAmbientMessage(TemplateMessage message, IPlayer* player, const Vec3& position)
{
    if (message == 0)
        trigger(player, position);                   // slot 96 (+0x180)
}

// 20 / 28 / 20 B: the template's triggerId (+0x184), numNetworkedEffects > 0 (+0x18C), radius (+0x180).
int   AmbientEffectArea::getTriggerId() const { return m_template->getTriggerId(); }
bool  AmbientEffectArea::isNetworked() const  { return m_template->getNumNetworkedEffects() > 0; }
float AmbientEffectArea::getRadius() const    { return m_template->getRadius(); }

// Selects and triggers a linked effect, sending at most one network event and then playing locally on clients.
// Client container selection excludes child zero; a one-child container plays nothing.
void AmbientEffectArea::trigger(IPlayer* player, const Vec3& position)
{
    if (m_template->m_numNetworkedEffects > 0 && !gameLogic->isServer())
        return;

    int effectIndex = 0;
    if (m_effectTemplate && m_effectTemplate->getClassID() == CID_EffectBundleTemplate)
    {
        // index 0: the linked template is the effect itself.
    }
    else if (m_effectContainer && m_effectTemplate->getClassID() == CID_BundleTemplate)
    {
        // The second class-id call has no template null guard.
        std::vector<IObject*> children;
        world::getChilds(m_effectContainer, children, 0, 0);
        m_lastRandom = static_cast<int>(((WorldPref::getTickCount() & 0x3FFu) + 92993u) & 0x7FFFFFFFu);
        float upper;
        if (isDedicated())
        {
            // Retail still traverses the container although the dedicated bound uses the
            // configured count.
            upper = static_cast<float>(m_template->m_numNetworkedEffects);
        }
        else
        {
            // Client 5E9063: the child count (size_t, so converted unsigned), and ONE child returns.
            const std::size_t count = children.size();
            if (count == 1)
                return;
            upper = static_cast<float>(count);
        }
        // 440030(&m_lastRandom, 1.0f, upper) on the client - the same draw as the ELF's inline one.
        const double chosen = 1.0 + (double(upper) - 1.0) * ambientRandom(m_lastRandom) * (1.0 / 16777216.0);
#if defined(_WIN32)
        effectIndex = static_cast<int>(chosen);
#else
        // [PLATFORM] ELF materializes a float before cvttss2si; PE converts the x87 result.
        effectIndex = static_cast<int>(static_cast<float>(chosen));
#endif
    }
    else if (!isDedicated())
    {
        // Client 5E937F: the container and template names are gathered first, then the Debug, then
        // the area's position and name - the order playEffect's own diagnostic uses.
        const std::string containerName = m_effectContainer
            ? object_detail::LayoutAccess::objectTemplate(m_effectContainer)->getName() : std::string();
        const std::string templateName = m_effectTemplate ? m_effectTemplate->getName() : std::string();
        Debug diagnostic(DebugTypeError2, "Game/Objects/Game/AmbientEffectArea.cpp", 288, "");
        const Vec3& areaPosition = getAbsolutePosition();
        const std::string& areaName = m_template->getName();
        diagnostic << "Error in AmbientEffectArea." << " Area:" << areaName << " Position:"
            << areaPosition << " Template:" << templateName << " Container:" << containerName
            << " TriggerId:" << m_template->m_triggerId;
        return;
    }

    IObject* eventObject = 0;
    Vec3 relative = {0.0f, 0.0f, 0.0f};
    if (player && m_template->m_playAtEventPosition)
    {
        IObject* vehicle = player->getVehicle();
        if (vehicle)
        {
            // [PLATFORM] PE evaluates the area's reference first; ELF reverses the calls.
#if defined(_WIN32)
            const Vec3& areaPosition = getAbsolutePosition();
            const Vec3& vehiclePosition = vehicle->getAbsolutePosition();
#else
            const Vec3& vehiclePosition = vehicle->getAbsolutePosition();
            const Vec3& areaPosition = getAbsolutePosition();
#endif
            relative = Vec3{vehiclePosition.x - areaPosition.x,
                vehiclePosition.y - areaPosition.y, vehiclePosition.z - areaPosition.z};
        }
        if (m_template->m_attachToEventObject)
        {
            eventObject = vehicle;
            relative = Vec3{0.0f, 0.0f, 0.0f};
        }
    }
    else if (m_template->m_playAtEventPosition)
    {
        const Vec3& areaPosition = getAbsolutePosition();
        relative = Vec3{position.x - areaPosition.x,
            position.y - areaPosition.y, position.z - areaPosition.z};
    }
    else
    {
        const float x = static_cast<float>(ambientRandom(m_lastRandom) * (1.0 / 8388608.0) - 1.0);
        const float clamped = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
#if defined(_WIN32)
        // [PLATFORM] PE uses _CIacos/fsin, then stores both chord endpoints as floats.
        const float chord = static_cast<float>(std::sin(std::acos(double(clamped))));
#else
        const float chord = std::sin(std::acos(clamped));
#endif
        const double z = double(ambientRandom(m_lastRandom)) * (double(chord) - double(-chord))
            * (1.0 / 16777216.0) - chord;
        relative.x = x * m_template->m_radius;
        relative.z = static_cast<float>(z * m_template->m_radius);
    }

    if (m_template->m_projectOnHeightmap)
    {
        // Three virtual position reads are present in both ports. PE retains the first
        // reference until after the second call; ELF computes z before that call.
#if defined(_WIN32)
        const Vec3& zPosition = getAbsolutePosition();
        const Vec3& xPosition = getAbsolutePosition();
        const float z = relative.z + zPosition.z;
        const float x = relative.x + xPosition.x;
#else
        const float z = getAbsolutePosition().z + relative.z;
        const float x = getAbsolutePosition().x + relative.x;
#endif
        const float height = static_cast<IHeightmapCluster*>(heightmapCluster)->getHeightInWorldCoords(x, z);
        relative.y = height - getAbsolutePosition().y;
    }

    if (isNetworked() && gameLogic->isServer())
    {
        unsigned short objectNetId = 0;
        if (eventObject)
        {
            io::INetworkable* networkable = eventObject->getNetworkable();
            if (networkable)
                objectNetId = networkable->m_networkableId;
        }
        game->sendGameEventToAllConnected(new AmbientEffectAreaEvent(this, effectIndex, relative, objectNetId));
    }
    // Client 5E9371: the local playback, after any send - a listen server plays its own too.
    if (!isDedicated())
        playEffect(relative, effectIndex, eventObject);
}
// Creates and initializes the selected effect and tracks its lifetime through a weak reference.
// The original capacity path leaks a newly created object when the live list already exceeds ten.
void AmbientEffectArea::playEffect(Vec3& position, int effectIndex, IObject* eventObject)
{
    const IObjectTemplate* selected;
    if (m_effectTemplate && m_effectTemplate->getClassID() == CID_EffectBundleTemplate)
        selected = m_effectTemplate;
    else if (m_effectContainer && m_effectTemplate->getClassID() == CID_BundleTemplate)
    {
        std::vector<IObject*> children;
        world::getChilds(m_effectContainer, children, 0, 0);
        selected = object_detail::LayoutAccess::objectTemplate(children[effectIndex]);
    }
    else
    {
        // Retail has no null guard for m_effectTemplate in the container condition above.
        const std::string containerName = m_effectContainer
            ? object_detail::LayoutAccess::objectTemplate(m_effectContainer)->getName() : std::string();
        const std::string templateName = m_effectTemplate ? m_effectTemplate->getName() : std::string();
#if !defined(_WIN32)
        const std::string& areaName = m_template->getName();
#endif
        Debug diagnostic(DebugTypeError2, "Game/Objects/Game/AmbientEffectArea.cpp", 380, "");
#if defined(_WIN32)
        const Vec3& areaPosition = getAbsolutePosition();
        const std::string& areaName = m_template->getName();
#endif
        diagnostic << "Error in AmbientEffectArea." << " Area:" << areaName << " Position:";
#if !defined(_WIN32)
        const Vec3& areaPosition = getAbsolutePosition();
#endif
        diagnostic << areaPosition << " Template:" << templateName << " Container:" << containerName
            << " TriggerId:" << m_template->m_triggerId;
        return;
    }
    if (!eventObject) eventObject = this;
    EffectBundle* effect = static_cast<EffectBundle*>(selected->createObject());
    if (!effect) return;
    for (std::list<WeakPtr<IObject> >::iterator i = m_effects.begin(); i != m_effects.end(); )
    {
        if (!i->get()) i = m_effects.erase(i);
        else ++i;
    }
    if (m_effects.size() > 10) return;
    m_effects.push_back(effect->getWeakPtr());
    effect->overRideTimeToLive(true);
    effect->init();
    const EffectBundleTemplate* t = static_cast<const EffectBundleTemplate*>(
        object_detail::LayoutAccess::objectTemplate(effect));
    const double lifetime = ambientLifetimeSample(t->m_dist2E0);
    if (lifetime == -1.0 || lifetime == 30.0)
        effect->destroy(true);
    else
    {
        eventObject->addChild(effect);
        effect->setRelativePosition(position);
        effect->handleMessage(static_cast<TemplateMessage>(0), 0, false);
    }
}

// Serializes the area template; attachToEventObject is intentionally omitted.
bool AmbientEffectAreaTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    if (m_radius != 0.0f) out << "ObjectTemplate.radius " << m_radius << std::endl;
    out << "ObjectTemplate.triggerId " << m_triggerId << std::endl;
    if (m_playAtEventPosition) out << "ObjectTemplate.playAtEventPosition " << m_playAtEventPosition << std::endl;
    if (m_numNetworkedEffects > 0) out << "ObjectTemplate.numNetworkedEffects " << m_numNetworkedEffects << std::endl;
    if (!m_projectOnHeightmap) out << "ObjectTemplate.projectOnHeightmap " << m_projectOnHeightmap << std::endl;
    if (!m_linkedEffectContainer.empty()) out << "ObjectTemplate.linkedEffectContainer " << m_linkedEffectContainer << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

} // namespace world
} // namespace hfe
} // namespace dice
