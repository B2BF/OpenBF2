// Path:     attested - Game/Objects/Weapons/FireComponents/MultiFireComp.cpp occurs in both the
// BF2 1.50 PE and ELF source maps.
// Contents: complete MultiFireComp runtime/template family recovered from bf2.i64
// (0x82679A0..0x826B21F) and cross-checked against bf2_w32ded.exe.

#include "MultiFireComp.h"

#include <cstddef>
#include <cfloat>
#include <cmath>
#include <sstream>
#include <string>

#include "../GenericFireArm.h"
#include "../../ICameraObject.h"
#include "../../IObject.h"
#include "../../IObjectComponent.h"
#include "../../IObjectTemplate.h"
#include "../../IPlayer.h"
#include "../../ObjectLayoutAccess.h"
#include "../../../IGame.h"
#include "../../../../Core/Debug.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/StringUtil.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Physics/IPhysicsNode.h"
#include "../../../../AI/AIInterface/BFKnowledge/BFCollisionHandler.h"
#include "../../../../AI/AIDLL/AI/Main/CommonSingletonServer.h"   // m_iBFCollisionHandler

namespace dice {
namespace hfe {
namespace world {

namespace {

void normalizeAxis(float& x, float& y, float& z)
{
    const float lengthSquared = x * x + y * y + z * z;
    if (std::fabs(lengthSquared - 1.0f) < FLT_EPSILON)
        return;

    if (std::fabs(lengthSquared) >= FLT_EPSILON)
    {
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        x *= inverseLength;
        y *= inverseLength;
        z *= inverseLength;
    }
    else
    {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
    }
}

} // anonymous namespace

// ELF C2/C1 0x82679A0/0x8267B40, PE 0x5A26E0. Retail deliberately leaves the requesting-fire
// byte (ELF +0x16C) and the dword at +0x174 untouched in the constructor.
MultiFireComp::MultiFireComp(MultiFireCompTemplate* objectTemplate)
    : FireComp(objectTemplate)
    , m_refCount(1)
    , m_unusedValues()
    , m_barrels()
    , m_fireCompleteAtTick(0)
    , m_pendingBarrel(-1)
    , m_barrelWaiting()
    , m_barrelFireAtTicks()
    , m_currentBarrel(0)
    , m_multiTemplate(objectTemplate)
    , m_mediumProjectileOrigin(Mat4::getIdentity())
{
}

MultiFireComp::~MultiFireComp() {}

void MultiFireComp::addRef() { ++m_refCount; }
int MultiFireComp::getRef() { return m_refCount; }

int MultiFireComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = m_refCount == 1;
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* MultiFireComp::queryInterface(unsigned int iid) const
{
    MultiFireComp* const self = const_cast<MultiFireComp*>(this);
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IWeaponComp || iid == IID_IWeaponFireComp)
        return static_cast<IWeaponFireComp*>(self);
    return 0;
}

void MultiFireComp::initComponent(WeaponInitEvent& event)
{
    FireComp::initComponent(event);
    createAllBarrels();
    m_barrelFireAtTicks.resize(m_barrels.size(), 0u);
    m_barrelWaiting.resize(m_barrels.size(), false);
}

// . The vector is not cleared: a second init would append a second copy, as shipped.
void MultiFireComp::createAllBarrels()
{
    for (IObject* child = firearm()->getChild(); child; child = child->getSibling())
    {
        for (std::list<std::string>::const_iterator it = m_multiTemplate->getBarrelNames().begin();
             it != m_multiTemplate->getBarrelNames().end(); ++it)
        {
            if (!compareIgnoreCase(child->getTemplate()->getName(), *it))
                m_barrels.push_back(child);
        }
    }
}

bool MultiFireComp::preUpdate()
{
    if (FireComp::preUpdate())
        return true;

    for (std::size_t i = 0; i < m_barrels.size(); ++i)
    {
        if (m_barrelWaiting[i]
            && m_barrelFireAtTicks[i]
                   + static_cast<unsigned int>(m_multiTemplate->getFireLaunchDelayInTicks())
               < static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            m_requestingFire = false;
            m_barrelWaiting[i] = false;
            m_pendingBarrel = static_cast<int>(i);
            setFireState(static_cast<FireState>(4));
            return true;
        }
    }
    return false;
}

void MultiFireComp::updateComponent(float, unsigned int)
{
    if (static_cast<int>(getFireState()) == 4
        && m_fireCompleteAtTick <= static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        fireComplete(m_fireCompleteAtTick);
    }
}

bool MultiFireComp::requestFire(bool released, bool)
{
    if (static_cast<int>(getFireRate()) == 2)
    {
        m_requestingFire = !released;
    }
    else
    {
        if (released)
            return false;
        m_requestingFire = true;
    }
    return true;
}

void MultiFireComp::outOfAmmo(bool alternate)
{
    FireComp::outOfAmmo(alternate);
}

// . The return value is not simply a shot count: delayed launches
// report the number of barrels which remain to be completed, while an immediate failed launch
// reports one (or all barrels on the simultaneous arm).
int MultiFireComp::fire(bool, int)
{
    m_signalFire = false;
    m_signalComplete = false;

    int result = 0;
    m_fireAtTick = static_cast<unsigned int>(WorldPref::getTickCount());
    m_fireCompleteAtTick =
        m_fireAtTick + static_cast<unsigned int>(m_multiTemplate->getTimePerRound());
    setFireState(static_cast<FireState>(2));
    m_firedProjectiles.clear();

    const int barrelCount = static_cast<int>(m_barrels.size());
    if (m_pendingBarrel != -1)
    {
        launchProjectile(m_pendingBarrel); // the return is intentionally discarded
        m_pendingBarrel = -1;
    }
    else if (barrelCount == 0)
    {
        const bool launched = launchProjectile(-1);
        if (m_multiTemplate->getFireLaunchDelayInTicks() == 0 || !launched)
            result = 1;
    }
    else if (barrelCount == 1)
    {
        const bool launched = launchProjectile(0);
        if (m_multiTemplate->getFireLaunchDelayInTicks() == 0 || !launched)
            result = 1;
    }
    else if (m_multiTemplate->getUseSequentialBarrels())
    {
        if (m_currentBarrel >= barrelCount)
            m_currentBarrel = 0;
        const bool launched = launchProjectile(m_currentBarrel);
        ++m_currentBarrel;
        if (m_multiTemplate->getFireLaunchDelayInTicks() == 0 || !launched)
            result = 1;
    }
    else if (m_maxShots == -1 || m_maxShots >= barrelCount)
    {
        for (int i = 0; i < barrelCount; ++i)
        {
            const bool launched = launchProjectile(i);
            if (m_multiTemplate->getFireLaunchDelayInTicks() == 0 || !launched)
                result = barrelCount;
        }
    }
    else if (m_maxShots > 0)
    {
        for (int i = 0; i < m_maxShots; ++i)
        {
            const bool launched = launchProjectile(i);
            if (m_multiTemplate->getFireLaunchDelayInTicks() == 0 || !launched)
                result = barrelCount;
        }
    }

    setFireState(static_cast<FireState>(4));
    if (result == 0 && m_multiTemplate->getFireLaunchDelay() != 0.0f)
        return m_multiTemplate->getUseSequentialBarrels() ? 1 : barrelCount;
    return result;
}

void MultiFireComp::enable()
{
    if (m_fireCompleteAtTick != 0)
    {
        const unsigned int currentTick = static_cast<unsigned int>(WorldPref::getTickCount());
        const unsigned int completionTick = m_fireCompleteAtTick;
        if (completionTick <= currentTick)
        {
            fireCompleteUpdateStates();
            setFireState(static_cast<FireState>(0));
            m_signalComplete = true;
            m_signalCompleteAtTick = completionTick;
        }
    }

    FireComp::enable();
    m_barrelFireAtTicks.resize(m_barrels.size(), 0u);
    m_barrelWaiting.resize(m_barrels.size(), false);
}

void MultiFireComp::fireCompleteUpdateStates()
{
    m_barrelFireAtTicks.resize(m_barrels.size(), 0u);
    m_barrelWaiting.resize(m_barrels.size(), false);
    m_fireCompleteAtTick = 0;
}

void MultiFireComp::fireComplete(unsigned int tick)
{
    FireComp::fireComplete(tick);
    fireCompleteUpdateStates();
}

void MultiFireComp::dummyLaunchProjectile(int index)
{
    const int barrelCount = static_cast<int>(m_barrels.size());
    if (m_multiTemplate->getUseDummyProjectiles() && barrelCount != 0 && index < barrelCount)
    {
        IObject* const barrel = index == -1 ? m_barrels[0] : m_barrels[index];
        barrel->updateFlags(0, 2);
    }
}

// . Delayed barrels first latch their request and return false; the
// later pending-barrel pass performs the real launch. The networkable-client dummy arm precedes
// ordinary barrel effects and projectile creation.
bool MultiFireComp::launchProjectile(int index)
{
    if (index == -1)
    {
        Debug(DEBUGTYPE_6,
              "Game/Objects/Weapons/FireComponents/MultiFireComp.cpp", 290, "")
            << "No barrels configured for this multifirearm";
        return false;
    }

    if (m_pendingBarrel == -1
        && !m_barrelWaiting[static_cast<std::size_t>(index)]
        && m_multiTemplate->getFireLaunchDelayInTicks() != 0)
    {
        m_barrelFireAtTicks[static_cast<std::size_t>(index)] = m_fireAtTick;
        m_barrelWaiting[static_cast<std::size_t>(index)] = true;

        bool allWaiting = true;
        for (std::size_t i = 0; i < m_barrels.size(); ++i)
        {
            if (!m_barrelWaiting[i])
            {
                allWaiting = false;
                break;
            }
        }
        if (allWaiting)
            setFireState(static_cast<FireState>(3));

        if (!m_barrels.empty())
        {
            m_barrels[static_cast<std::size_t>(index)]->handleMessage(
                static_cast<TemplateMessage>(0), firearm()->getPlayer(), false);
        }
        return false;
    }

    if (m_barrelWaiting[static_cast<std::size_t>(index)])
        return false;

    if (!firearm()->getTemplate()->getNetworkableInfo().empty()
        && game->queryInterface(IID_IGameClient))
    {
        dummyLaunchProjectile(index);
        return true;
    }

    if (m_multiTemplate->getFireLaunchDelayInTicks() == 0 && !m_barrels.empty())
    {
        m_barrels[static_cast<std::size_t>(index)]->handleMessage(
            static_cast<TemplateMessage>(0), firearm()->getPlayer(), false);
    }

    if (firearm()->getProjectileTemplate())
    {
        dummyLaunchProjectile(index);

        const Mat4& origin = getProjectileOrigin();
        IObject* const root = getRootParent(firearm());
        IPhysicsNode* const rootNode = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(root));

        Vec3 inherited = {0.0f, 0.0f, 0.0f};
        if (rootNode)
            inherited = rootNode->getPositionalSpeed();

        const float muzzleVelocity = firearm()->getVelocity();
        Vec3 velocity = {
            origin.m[8] * muzzleVelocity + inherited.x,
            origin.m[9] * muzzleVelocity + inherited.y,
            origin.m[10] * muzzleVelocity + inherited.z
        };

        // The second projectile-template query is observable in both shipped ports.
        if (!ignoreNetworkedProjectileOnClient(firearm()->getProjectileTemplate()))
        {
            IObject* const object = setupProjectile(origin, velocity);
            if (object)
            {
                IProjectileObject* const projectile = static_cast<IProjectileObject*>(
                    object->queryInterface(IID_IProjectileObject));
                if (ai::CommonSingletonServer::m_iBFCollisionHandler)
                    static_cast<ai::IBFCollisionHandler*>(ai::CommonSingletonServer::m_iBFCollisionHandler)
                        ->handleProjectileFire(projectile, object);
            }
        }
    }
    return true;
}

void MultiFireComp::postReload()
{
    if (m_multiTemplate->getUseDummyProjectiles())
    {
        for (std::size_t i = 0; i < m_barrels.size(); ++i)
            m_barrels[i]->updateFlags(2, 0);
    }
}

bool MultiFireComp::busy()
{
    return static_cast<int>(getFireState()) != 0;
}

void MultiFireComp::getProjectileOriginForBarrel(int index, Mat4& out) const
{
    if (m_multiTemplate->getFireInCameraDof() && firearm()->getPlayerByForce())
    {
        out = getProjectileOriginFromCamera();
    }
    else
    {
        IObject* const source = index == -1
            ? static_cast<IObject*>(firearm())
            : m_barrels[static_cast<std::size_t>(index)];
        out = source->getAbsoluteTransformation();
    }

    const Vec3& offset = m_multiTemplate->getProjectileStartPosition();
    if (!(std::fabs(offset.x) <= FLT_EPSILON
          && std::fabs(offset.y) <= FLT_EPSILON
          && std::fabs(offset.z) <= FLT_EPSILON))
    {
        const float x = out.m[0] * offset.x + out.m[4] * offset.y
            + out.m[8] * offset.z + out.m[12];
        const float y = out.m[1] * offset.x + out.m[5] * offset.y
            + out.m[9] * offset.z + out.m[13];
        const float z = out.m[2] * offset.x + out.m[6] * offset.y
            + out.m[10] * offset.z + out.m[14];
        out.m[12] = x;
        out.m[13] = y;
        out.m[14] = z;
    }
}

const Mat4& MultiFireComp::getProjectileOrigin() const
{
    int index = -1;
    const int barrelCount = static_cast<int>(m_barrels.size());
    if (m_pendingBarrel != -1)
    {
        index = m_pendingBarrel;
    }
    else if (barrelCount != 0)
    {
        if (barrelCount == 1)
            index = 0;
        else if (m_multiTemplate->getUseSequentialBarrels())
            index = m_currentBarrel < barrelCount ? m_currentBarrel : 0;
    }

    getProjectileOriginForBarrel(index, m_projectileOrigin);
    normalizeAxis(m_projectileOrigin.m[8], m_projectileOrigin.m[9], m_projectileOrigin.m[10]);
    normalizeAxis(m_projectileOrigin.m[4], m_projectileOrigin.m[5], m_projectileOrigin.m[6]);
    normalizeAxis(m_projectileOrigin.m[0], m_projectileOrigin.m[1], m_projectileOrigin.m[2]);
    return m_projectileOrigin;
}

const Mat4& MultiFireComp::getMediumProjectileOrigin()
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    const int barrelCount = static_cast<int>(m_barrels.size());
    for (int i = 0; i < barrelCount; ++i)
    {
        getProjectileOriginForBarrel(i, m_mediumProjectileOrigin);
        x += m_mediumProjectileOrigin.m[12];
        y += m_mediumProjectileOrigin.m[13];
        z += m_mediumProjectileOrigin.m[14];
    }

    // Retail has no empty-vector guard: zero barrels produce NaNs through 0 * (1 / 0).
    const float inverseCount = 1.0f / static_cast<float>(barrelCount);
    m_mediumProjectileOrigin.m[12] = x * inverseCount;
    m_mediumProjectileOrigin.m[13] = y * inverseCount;
    m_mediumProjectileOrigin.m[14] = z * inverseCount;

    normalizeAxis(m_mediumProjectileOrigin.m[8], m_mediumProjectileOrigin.m[9],
                  m_mediumProjectileOrigin.m[10]);
    normalizeAxis(m_mediumProjectileOrigin.m[4], m_mediumProjectileOrigin.m[5],
                  m_mediumProjectileOrigin.m[6]);
    normalizeAxis(m_mediumProjectileOrigin.m[0], m_mediumProjectileOrigin.m[1],
                  m_mediumProjectileOrigin.m[2]);
    return m_mediumProjectileOrigin;
}

const Mat4& MultiFireComp::getProjectileOriginFromCamera() const
{
    IPlayer* const player = firearm()->getPlayerByForce();
    ICameraObject* const camera = player->getCamera();
    IObject* cameraObject = 0;
    if (camera)
        cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));
    return cameraObject->getAbsoluteTransformation();
}

bool MultiFireComp::launchesProjectiles(int) const { return true; }

// Template

MultiFireCompTemplate::MultiFireCompTemplate()
    : FireCompTemplate()
    , m_refCount(1)
    , m_useDummyProjectiles(false)
    , m_barrelNames()
    , m_useSequentialBarrels(true)
{
}

MultiFireCompTemplate::~MultiFireCompTemplate() {}

void MultiFireCompTemplate::addRef() { ++m_refCount; }
int MultiFireCompTemplate::getRef() { return m_refCount; }

int MultiFireCompTemplate::release()
{
    const int remaining = m_refCount - 1;
    const bool last = m_refCount == 1;
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* MultiFireCompTemplate::queryInterface(unsigned int iid) const
{
    MultiFireCompTemplate* const self = const_cast<MultiFireCompTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent || iid == IID_IWeaponCompTemplate)
        return static_cast<IWeaponCompTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int MultiFireCompTemplate::getClassID() const
{
    return CID_MultiFireCompTemplate;
}

IObjectComponent* MultiFireCompTemplate::createComponent()
{
    return new MultiFireComp(this);
}

void* MultiFireCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_MultiFireCompTemplate || classId == CID_FireCompTemplate)
        return this;
    return 0;
}

void MultiFireCompTemplate::addBarrelName(const std::string& name)
{
    for (std::list<std::string>::const_iterator it = m_barrelNames.begin();
         it != m_barrelNames.end(); ++it)
    {
        if (!compareIgnoreCase(name, *it))
            return;
    }
    m_barrelNames.push_back(name);
}

void MultiFireCompTemplate::removeBarrelName(const std::string& name)
{
    for (std::list<std::string>::iterator it = m_barrelNames.begin();
         it != m_barrelNames.end(); ++it)
    {
        if (!compareIgnoreCase(name, *it))
        {
            m_barrelNames.erase(it);
            return;
        }
    }
}

bool MultiFireCompTemplate::isBarrel(const std::string& name)
{
    for (std::list<std::string>::const_iterator it = m_barrelNames.begin();
         it != m_barrelNames.end(); ++it)
    {
        if (!compareIgnoreCase(name, *it))
            return true;
    }
    return false;
}

// The launch delay is intentionally serialized a second time after FireCompTemplate's writer.
// useSequentialBarrels is never written by retail, despite being live runtime state.
bool MultiFireCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (!FireCompTemplate::makeScript(stream))
        return false;

    if (getFireLaunchDelay() != 0.0f)
        script << "ObjectTemplate.fire.fireLaunchDelay " << getFireLaunchDelay() << std::endl;
    if (m_useDummyProjectiles)
        script << "ObjectTemplate.fire.useDummyProjectiles 1" << std::endl;
    for (std::list<std::string>::const_iterator it = m_barrelNames.begin();
         it != m_barrelNames.end(); ++it)
    {
        script << "ObjectTemplate.fire.addBarrelName " << *it << std::endl;
    }

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
