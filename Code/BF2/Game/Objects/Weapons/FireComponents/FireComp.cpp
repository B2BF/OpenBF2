// Shared firing component and template behavior.

#include "../../FireCompTemplate.h"
#include "FireComp.h"
#include "SingleFireComp.h"

#include "../GenericFireArm.h"
#include "../../GenericFireArmTemplate.h"
#include "../../GrapplingHookRopeTemplate.h"
#include "../../ICameraObject.h"
#include "../../IObject.h"
#include "../../IObjectTemplate.h"
#include "../../IPlayer.h"
#include "../../ObjectLayoutAccess.h"
#include "../../Projectiles/GenericProjectile.h"
#include "../../Projectiles/GenericProjectileTemplate.h"
#include "../../TemplateLeaves.h"

#include "../../../IGame.h"
#include "../../../IGameLogic.h"
#include "../../../Utility/Vars.h"

#include "../../../../Core/EventManager.h"
#include "../../../../Core/System.h"
#include "../../../../Physics/HeightmapCluster.h"
#include "../../../../Physics/IPhysicsNode.h"
#include "../../../../Scene/Object/ObjectManager.h"
#include "../../../../Scene/Object/PlayerManager.h"
#include "../../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

#include <cfloat>
#include <cmath>
#include <cstdio>                // [TEMP] fire-mode toggle tracer
#include <cstdlib>               // [TEMP] fire-mode toggle tracer
#include <cstring>
#include <map>
#include <sstream>

#include "../../../../Core/Debug.h"
#include "../../../../Core/Ids.h"
#include "../../../../IO/Stream/IStream.h"

namespace dice {
namespace hfe {
namespace world {

// [TEMP] [CHOSEN, not recovered] fire-mode toggle tracer, armed by BF2_TRACE_FIRERATE=1.
// Remove with the fire-mode investigation. [RETIRE-WHEN: firerate-toggle-diagnosed]
namespace {
bool fireRateTraceArmed()
{
    static int armed = -1;
    if (armed < 0)
        armed = std::getenv("BF2_TRACE_FIRERATE") ? 1 : 0;
    return armed != 0;
}
} // namespace


// FireCompTemplate


// The two unknown dwords before m_fireRates and the two natural padding bytes after the camera-DOF
// flags are deliberately not initialized: neither shipped constructor writes them.  The generic
// RandomDistribution default is value=1, so this class changes only that value to its observed
// fireStartDelay default of zero while retaining distribution=0, variation=0, relative=false and
// the retail 0x759DC150 random seed.
FireCompTemplate::FireCompTemplate()
    : m_fireStartDelay()
    , m_fireLaunchDelay(0.0f)
    , m_fireLaunchDelayTimes30(0)
    , m_fireRates()
    , m_roundsPerMinute(600)
    , m_fireInCameraDof(false)
    , m_fireInCameraDofLocked(false)
    , m_batchSize(1)
    , m_burstSize(3)
    , m_fireInput(io::PIFire)
    , m_altFireInput(io::PINone)
    , m_ignoreFireButton(io::PINone)
    , m_projectileStartPosition{0.0f, 0.0f, 0.0f}
    , m_projectileStartRotation{0.0f, 0.0f, 0.0f}
    , m_maxProjectilesInWorld(0)
{
    m_fireStartDelay.value = 0.0f;

    // The ELF really calls the out-of-line setter and then performs the same cache calculation a
    // second time.  Preserve that redundant ordering; the PE optimizer folds it to one division.
    setRoundsPerMinute(600);
    m_timePerRound = 1800 / m_roundsPerMinute;
}

// D1/D0 at ELF 0x8266820/0x8266880 and PE 0x5A2570/0x5A2670.  The hand-written body is empty;
// compiler-generated teardown destroys/deallocates the std::vector<FireRate> member.
FireCompTemplate::~FireCompTemplate()
{
}

// PE 0x5BF9C0.  There is intentionally no zero guard: RPM==0 takes the shipped
// integer divide fault.
void FireCompTemplate::setRoundsPerMinute(int roundsPerMinute)
{
    m_roundsPerMinute = roundsPerMinute;
    m_timePerRound = 1800 / roundsPerMinute;
}

// PE 0x5BF9E0.  Both ports truncate toward zero after multiplying seconds by 30.
void FireCompTemplate::setFireLaunchDelay(float delay)
{
    m_fireLaunchDelay = delay;
    m_fireLaunchDelayTimes30 = static_cast<int>(delay * 30.0f);
}

unsigned int FireCompTemplate::getClassID() const
{
    return CID_FireCompTemplate;
}

unsigned int FireCompTemplate::getInterfaceID()
{
    return IID_IWeaponFireCompTemplate;
}

// this base compares the constant directly; it does not dispatch through
// getClassID (and therefore does not identify a leaf as FireCompTemplate through this method).
void* FireCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_FireCompTemplate)
        return this;
    return 0;
}

// Three five-byte hooks at ELF 0x8262810/0x8262820/0x8262830.
void FireCompTemplate::objectCreated(IObject*)       {}
void FireCompTemplate::objectDestroyed(IObject*)     {}
void FireCompTemplate::setTemplate(IObjectTemplate*) {}

// (239 B), PE 0x5A25C0 (116 B).  FireCompTemplate is an abstract virtual type even
// though the slot has a real body.  DEBUGTYPE_6 is process-fatal; the structural null return after
// the Debug temporary's destructor is nevertheless present in both shipped images.
IObjectComponent* FireCompTemplate::createComponent()
{
    Debug(DEBUGTYPE_6, "Game/Objects/Weapons/FireComponents/FireComp.h", 210, "")
        << "Virtual type";
    return 0;
}

// (2,409 B), PE 0x5C00E0 (1,071 B).  The emitted property order, default gates,
// nested camera-DOF lock gate, slash-separated vector spelling and single final write are the same
// in both ports.  Note the deliberately narrow fireStartDelay gate: variation/relative alone do
// not cause a line to be emitted; retail tests only distribution and value.
bool FireCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;

    if (m_roundsPerMinute != 600)
        script << "ObjectTemplate.fire.roundsPerMinute " << m_roundsPerMinute << std::endl;

    if (m_fireStartDelay.distribution != CRD_NONE || m_fireStartDelay.value != 0.0f)
        script << "ObjectTemplate.fire.fireStartDelay " << m_fireStartDelay << std::endl;

    if (m_fireLaunchDelay != 0.0f)
        script << "ObjectTemplate.fire.fireLaunchDelay " << m_fireLaunchDelay << std::endl;

    if (m_fireInput != io::PIFire)
        script << "ObjectTemplate.fire.fireInput " << m_fireInput << std::endl;

    if (m_altFireInput != io::PINone)
        script << "ObjectTemplate.fire.altFireInput " << m_altFireInput << std::endl;

    for (unsigned int i = 0; i < m_fireRates.size(); ++i)
        script << "ObjectTemplate.fire.addFireRate " << m_fireRates[i] << std::endl;

    if (m_burstSize != 3)
        script << "ObjectTemplate.fire.burstSize " << m_burstSize << std::endl;

    // Retail suppresses a Vec3 only when every absolute component is <= FLT_EPSILON.  Spelling
    // this as the negated conjunction also preserves its NaN behavior (a NaN causes emission).
    if (!(std::fabs(m_projectileStartPosition.x) <= FLT_EPSILON
          && std::fabs(m_projectileStartPosition.y) <= FLT_EPSILON
          && std::fabs(m_projectileStartPosition.z) <= FLT_EPSILON))
    {
        script << "ObjectTemplate.fire.projectileStartPosition "
               << m_projectileStartPosition.x << '/'
               << m_projectileStartPosition.y << '/'
               << m_projectileStartPosition.z << std::endl;
    }

    if (!(std::fabs(m_projectileStartRotation.x) <= FLT_EPSILON
          && std::fabs(m_projectileStartRotation.y) <= FLT_EPSILON
          && std::fabs(m_projectileStartRotation.z) <= FLT_EPSILON))
    {
        script << "ObjectTemplate.fire.projectileStartRotation "
               << m_projectileStartRotation.x << '/'
               << m_projectileStartRotation.y << '/'
               << m_projectileStartRotation.z << std::endl;
    }

    if (m_fireInCameraDof)
    {
        script << "ObjectTemplate.fire.fireInCameraDof " << m_fireInCameraDof << std::endl;
        if (m_fireInCameraDofLocked)
        {
            script << "ObjectTemplate.fire.fireInCameraDofLocked "
                   << m_fireInCameraDofLocked << std::endl;
        }
    }

    if (m_batchSize != 1)
        script << "ObjectTemplate.fire.batchSize " << m_batchSize << std::endl;

    if (m_ignoreFireButton != io::PINone)
        script << "ObjectTemplate.fire.ignoreFireButton " << m_ignoreFireButton << std::endl;

    if (m_maxProjectilesInWorld != 0)
    {
        script << "ObjectTemplate.fire.maxProjectilesInWorld "
               << m_maxProjectilesInWorld << std::endl;
    }

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

// FireComp

namespace {

// TU statics at ELF 0x8265FC0 / 0x8266305.  The pointers, rather than copied values, are retained
// because the console variable repository may update either value while the game is running.
int* const localPrediction = Vars::getInt("local-prediction", 0);
int* const localPredictionConfirmTime = Vars::getInt("local-prediction-confirmtime", 30);

IPlayerControlObject* findPlayerControlObject(IObject* object)
{
    for (; object; object = object->getParent())
    {
        IPlayerControlObject* const pco = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
        if (pco)
            return pco;
    }
    return 0;
}

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

float unitRandom(Random& random)
{
    // The retail code drops seven low bits, forces the result odd, increments it, converts that
    // signed-positive integer to float, then multiplies by 2^-24.
    const unsigned int raw = ((random.getRaw() >> 7) | 1u) + 1u;
    return static_cast<float>(static_cast<int>(raw)) * 0.000000059604645f;
}

} // anonymous namespace

// ds:0x8ABAF24, defined in Core/Globals.cpp.  FireComp deliberately dereferences it without a
// null guard after a projectile has been created, exactly as both shipped ports do.
extern IBase* heightmapCluster;

// /0x8262460/0x8262470/0x8262480.  The interface destructor and its three permissive
// hooks are genuine emitted bodies, not temporary defaults.
IWeaponFireComp::~IWeaponFireComp() {}
void IWeaponFireComp::postFire(WeaponFireEvent&) {}
void IWeaponFireComp::fireComplete(unsigned int) {}
void IWeaponFireComp::postSwitchFireRate(FireRate) {}

// /0x8272BE0, 0x8279940/0x8279950 and 0x825CEA0/0x825CEB0.
// Target's two permissive hooks are emitted at 0x8279970/0x8279980; all other tail methods are
// pure in the interface vtables.
IWeaponSoundComp::~IWeaponSoundComp() {}
IWeaponTargetComp::~IWeaponTargetComp() {}
void IWeaponTargetComp::stoppedTracking() {}
void IWeaponTargetComp::postChangeTarget(IObject*) {}
IWeaponDeviationComp::~IWeaponDeviationComp() {}

// ELF C2/C1 0x8262B90/0x8262DE0, PE constructor cross-check at the contiguous 0x5BFxxx run.
FireComp::FireComp(FireCompTemplate* objectTemplate)
    : m_projectiles()
    , m_weaponState(0)
    , m_lastMagazineIndex(-1)
    , m_fireRate(static_cast<FireRate>(2))
    , m_fireAtTick(0)
    , m_reloadAtTick(0)
    , m_sequenceNumber(0)
    , m_maxShots(0)
    // m_fireData is deliberately untouched in both shipped constructors.
    , m_fireCompletionState(0)
    , m_lastFiredProjectileId(-1)
    , m_fireStartDelay(false)
    , m_fireStartDelayTicks(0)
    , m_postReloadState(0)
    , m_fireLaunchDelayPending(false)
    , m_signalFire(false)
    , m_signalComplete(false)
    , m_signalCompleteAtTick(0)
    , m_firedProjectiles()
    , m_random()
    , m_template(objectTemplate)
    , m_dummyShotsFired(0)
    , m_fireButtonLastHeldAtTick(0)
    , m_altFireButtonLastHeldAtTick(0)
    , m_allowEffects(true)
    , m_currentProjectile(0)
    , m_fireState(static_cast<FireState>(0))
{
    // WeaponComp has no emitted constructor of its own.  FireComp performs its one observed base
    // store and intentionally leaves the two object pointers untouched until setObject/init.
    m_refCount = 1;

    m_projectileOrigin = Mat4::getIdentity();
    m_lockedCameraTransform = Mat4::getIdentity();

    const std::vector<FireRate>& rates = objectTemplate->getFireRates();
    if (!rates.empty())
        m_fireRate = rates[0];

    // The constructor's final scalar store is the weapon state, after fire-rate selection.
    m_weaponState = 0;
}

// D1/D0 at ELF 0x8266530/0x82665C0.  Member and base teardown is compiler generated.
FireComp::~FireComp() {}

unsigned int FireComp::getInterfaceID() const { return IID_IWeaponFireComp; }
bool FireComp::isSelectable() const { return true; }
void FireComp::stoppedTracking() {}
void FireComp::postChangeTarget(IObject*) {}
void FireComp::stop() {}
void FireComp::exit() {}
void FireComp::projectileWasReset(IObject*) {}
void FireComp::disable() {}
bool FireComp::busy() { return false; }
void FireComp::pickup() {}
void FireComp::reset() {}
IObject* FireComp::getAlternateObject() const { return 0; }
int FireComp::fire(bool, int) { return 0; }
bool FireComp::ownsProjectile(IObject*) const { return false; }
bool FireComp::releaseProjectile(IObject*) { return false; }
bool FireComp::launchesProjectiles(int) const { return true; }
void FireComp::addProjectileReference(IObject*) {}
void FireComp::updateComponent(float, unsigned int) {}
void FireComp::setWeaponState(unsigned int) {}
unsigned int FireComp::getWeaponState() const { return 0; }

void FireComp::setAllowEffects(bool allow) { m_allowEffects = allow; }
std::vector<IObject*>* FireComp::getFiredProjectiles() { return &m_firedProjectiles; }
Vec3 FireComp::getStartPos() const { return Vec3{0.0f, 0.0f, 0.0f}; }
Vec3 FireComp::getEndPos() const { return Vec3{0.0f, 0.0f, 0.0f}; }

bool FireComp::getSignalFire()
{
    const bool result = m_signalFire;
    m_signalFire = false;
    return result;
}

bool FireComp::getSignalComplete()
{
    const bool result = m_signalComplete;
    m_signalComplete = false;
    return result;
}

unsigned int FireComp::getSignalCompleteAtTick() { return m_signalCompleteAtTick; }
FireRate FireComp::getFireRate() const { return m_fireRate; }
void FireComp::setFireRate(FireRate rate) { m_fireRate = rate; }

// / PE cross-check.  The second argument belongs to the first dword of the otherwise
// unresolved +0x34 fire-data aggregate; ExplosivesFireComp reads that same dword as the current
// magazine/charge count.  It is not the earlier +0x1C last-magazine-index member.
void FireComp::setMaxShots(int maximum, int magazineCount)
{
    m_maxShots = maximum;
    *reinterpret_cast<int*>(m_fireData) = magazineCount;
}

bool FireComp::getFireStartDelay() const { return m_fireStartDelay; }
void FireComp::setFireStartDelay(bool delay) { m_fireStartDelay = delay; }
unsigned int FireComp::getFireAtTick() const { return m_fireAtTick; }
void FireComp::setFireAtTick(unsigned int tick) { m_fireAtTick = tick; }

// The stored id is updated by setupProjectile, but the base accessor really is the constant -1 in
// both ports.  Derived components may expose the stored field through their own behavior.
int FireComp::getLastFiredProjectileId() const { return -1; }
int FireComp::getDummyShotsFired() const { return m_dummyShotsFired; }
bool FireComp::isFireLaunchDelay() const { return false; }
FireState FireComp::getFireState() const { return m_fireState; }
unsigned int FireComp::getSequenceNr() { return m_sequenceNumber; }
io::PlayerInputMap FireComp::getFireInput() const { return m_template->getFireInput(); }
io::PlayerInputMap FireComp::getAltFireInput() const { return m_template->getAltFireInput(); }
int FireComp::getRoundsPerMinute() const { return m_template->getRoundsPerMinute(); }
int FireComp::getBurstSize() const { return m_template->getBurstSize(); }

void FireComp::setFireState(FireState state) { m_fireState = state; }

bool FireComp::requestReload(bool)
{
    return m_fireState == static_cast<FireState>(0);
}

void FireComp::setCurrentProjectile(IObject*)
{
    // The pointer argument is ignored by retail; this member is a confirmation tick stamp.
    m_currentProjectile = 0;
}

void FireComp::enable()
{
    m_currentProjectile = 0;
    m_fireStartDelay = false;
}

bool FireComp::isWaitingForMissileConfirm() const
{
    const unsigned int deadline = m_currentProjectile
        + static_cast<unsigned int>(*localPredictionConfirmTime);
    return deadline > static_cast<unsigned int>(WorldPref::getTickCount());
}

void FireComp::setFirebuttonLastHeldAtTick(io::PlayerInputMap input, unsigned int tick)
{
    if (input == m_template->getFireInput())
        m_fireButtonLastHeldAtTick = tick;
    else if (input == m_template->getAltFireInput())
        m_altFireButtonLastHeldAtTick = tick;
}

bool FireComp::requestFire(bool released, bool)
{
    // [CHOSEN, not recovered]
    IPlayer* const player = firearm()->getPlayer();
    if (!player)
        return false;
    const io::PlayerInputMap ignored = m_template->getIgnoreFireButton();
    if (ignored != io::PINone && ignored == player->getCurrentFiringButton())
        return false;

    if (m_fireRate == static_cast<FireRate>(2))
    {
        m_fireLaunchDelayPending = !released;
    }
    else
    {
        if (released)
            return false;
        m_fireLaunchDelayPending = true;
    }
    return true;
}

void FireComp::fireComplete(unsigned int)
{
    // The player lookup is intentionally retained even though this base discards the result.
    firearm()->getPlayer();
    setFireState(static_cast<FireState>(0));

    if (m_fireRate == static_cast<FireRate>(1))
    {
        if (m_sequenceNumber >= static_cast<unsigned int>(m_template->getBurstSize() - 1))
        {
            m_sequenceNumber = 0;
        }
        else
        {
            ++m_sequenceNumber;
            m_fireLaunchDelayPending = false;
            m_signalFire = true;
        }
    }
    else if (m_fireRate == static_cast<FireRate>(2)
             && m_fireButtonLastHeldAtTick
                    == static_cast<unsigned int>(WorldPref::getTickCount())
             && m_template->getIgnoreFireButton() != m_template->getFireInput())
    {
        m_fireLaunchDelayPending = false;
        m_signalFire = true;
    }

    // [TEMP] auto/burst continuation tracer, armed by BF2_TRACE_FIRERATE=1. THE asymmetry under
    // test: the burst arm (rate 1) re-signals unconditionally, but the automatic arm (rate 2)
    // re-signals only when the fire button was stamped on this exact tick. If that equality never
    // holds, an automatic weapon fires one round per pull - indistinguishable from single.
    // [RETIRE-WHEN: firerate-toggle-diagnosed]
    if (fireRateTraceArmed() && static_cast<int>(m_fireRate) != 0)
    {
        std::fprintf(stderr,
                     "[FRTRACE] fireComplete rate=%d tick=%d heldAtTick=%u equal=%d"
                     " ignore=%d fireIn=%d signalFire=%d\n",
                     static_cast<int>(m_fireRate), WorldPref::getTickCount(),
                     m_fireButtonLastHeldAtTick,
                     m_fireButtonLastHeldAtTick
                         == static_cast<unsigned int>(WorldPref::getTickCount()) ? 1 : 0,
                     static_cast<int>(m_template->getIgnoreFireButton()),
                     static_cast<int>(m_template->getFireInput()),
                     m_signalFire ? 1 : 0);
        std::fflush(stderr);
    }

    m_fireStartDelay = false;
    m_signalComplete = true;
    m_signalCompleteAtTick = static_cast<unsigned int>(WorldPref::getTickCount());
}

void FireComp::fireStop(bool)
{
    m_fireStartDelay = false;
    if (m_fireState == static_cast<FireState>(1))
        m_fireState = static_cast<FireState>(0);
}

bool FireComp::preUpdate()
{
    if (!m_fireStartDelay)
        return false;

    if (m_fireAtTick + m_fireStartDelayTicks
        >= static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        return false;
    }

    m_fireLaunchDelayPending = false;
    m_fireStartDelay = false;
    setFireState(static_cast<FireState>(2));
    return true;
}

void FireComp::outOfAmmo(bool)
{
    m_sequenceNumber = static_cast<unsigned int>(m_template->getBurstSize() - 1);
}

void FireComp::reload(ReloadState state)
{
    if (state != static_cast<ReloadState>(2))
        m_reloadAtTick = static_cast<unsigned int>(WorldPref::getTickCount());
}

void FireComp::postReload()
{
    m_sequenceNumber = 0;
}

bool FireComp::isFireRateValid(FireRate rate)
{
    if (rate == m_fireRate)
        return true;

    const std::vector<FireRate>& rates = m_template->getFireRates();
    for (unsigned int i = 0; i < rates.size(); ++i)
    {
        if (rates[i] == rate)
            return true;
    }
    return false;
}

void FireComp::switchFireRate()
{
    const std::vector<FireRate>& rates = m_template->getFireRates();

    if (fireRateTraceArmed())
    {
        std::fprintf(stderr, "[FRTRACE] switchFireRate tick=%d cur=%d rates=%u [",
                     WorldPref::getTickCount(), static_cast<int>(m_fireRate),
                     static_cast<unsigned int>(rates.size()));
        for (unsigned int t = 0; t < rates.size(); ++t)
            std::fprintf(stderr, "%s%d", t ? "," : "", static_cast<int>(rates[t]));
        std::fprintf(stderr, "]");
    }

    if (rates.size() <= 1)
    {
        if (fireRateTraceArmed())
        {
            std::fprintf(stderr, " -> EARLY RETURN (size<=1)\n");
            std::fflush(stderr);
        }
        return;
    }

    // Retail deliberately does not break after finding the current entry.  Duplicated rates can
    // therefore advance more than once; preserve its index mutation and continuing scan.
    for (unsigned int i = 0; i < rates.size(); ++i)
    {
        if (rates[i] == m_fireRate)
        {
            ++i;
            if (i >= rates.size())
                i = 0;
            m_fireRate = rates[i];
        }
    }

    if (fireRateTraceArmed())
    {
        std::fprintf(stderr, " -> %d\n", static_cast<int>(m_fireRate));
        std::fflush(stderr);
    }
}

// .  Every camera child is visited and the last one wins; there is no early break.
void FireComp::initComponent(WeaponInitEvent&)
{
    if (!m_template->getFireInCameraDof() || !m_template->getFireInCameraDofLocked())
        return;

    IPlayerControlObject* const pco = findPlayerControlObject(firearm());
    if (!pco)
        return;

    IObject* const pcoObject = static_cast<IObject*>(pco->queryInterface(IID_IObject));
    for (IObject* child = pcoObject->getChild(); child; child = child->getSibling())
    {
        if (child->queryInterface(IID_ICameraObject))
        {
            Mat4 inverse;
            inverse.setInverse(firearm()->getAbsoluteTransformation());
            m_lockedCameraTransform.mult(child->getAbsoluteTransformation(), inverse);
        }
    }
}

// PE counterpart in the same FireComp run.  Destruction is asynchronous and the
// weak list is intentionally retained until ordinary member teardown.
void FireComp::destroy()
{
    for (std::list<WeakPtr<IObject> >::iterator it = m_projectiles.begin();
         it != m_projectiles.end(); ++it)
    {
        IObject* const projectile = it->get();
        if (projectile)
        {
            objectManager->postMessage(
                projectile,
                static_cast<TemplateMessage>(32),
                firearm()->m_template->m_keepProjectiles);
        }
    }
}

// PE 0x5BF6F0.  The IID gate is performed before the server/local-player tests.
bool FireComp::getUseLocalPrediction(const IObjectTemplate* objectTemplate) const
{
    IPlayer* const player = firearm()->getPlayerByForce();
    IProjectileObjectTemplate* const projectileTemplate =
        static_cast<IProjectileObjectTemplate*>(
            objectTemplate->queryInterface(IID_IProjectileObjectTemplate));

    return !gameLogic->isServer()
        && player == playerManager->getLocalHumanPlayer()
        && projectileTemplate
        && !objectTemplate->getNetworkableInfo().empty()
        && (static_cast<GenericProjectileTemplate*>(projectileTemplate)
                ->getLocalPredictOnClient()
            || *localPrediction == 1);
}


bool FireComp::ignoreNetworkedProjectileOnClient(const IObjectTemplate* objectTemplate) const
{
    if (!objectTemplate)
        return false;
    if (getUseLocalPrediction(objectTemplate))
        return false;
    if (objectTemplate->getNetworkableInfo().empty())
        return false;
    if (gameLogic->isServer())
        return false;
    return true;
}

// PE 0x5BFC10.  The local-prediction arm deliberately bypasses ObjectManager and
// performs createObject -> transform -> projectile flag -> init in that order.
IObject* FireComp::createProjectile(const IObjectTemplate* objectTemplate, const Mat4& transform)
{
    if (!objectTemplate)
        return 0;

    const bool useLocalPrediction = getUseLocalPrediction(objectTemplate);
    IObject* object = 0;

    if (!objectTemplate->getNetworkableInfo().empty())
    {
        if (useLocalPrediction)
        {
            object = objectTemplate->createObject();
            object->setAbsoluteTransformation(transform);

            IProjectileObject* const projectileFace = static_cast<IProjectileObject*>(
                object->queryInterface(IID_IProjectileObject));
            // GenericProjectile is the sole retail implementor.  Keep the IID gate, then use its
            // primary typed surface while the long secondary interface remains undeclared.
            static_cast<GenericProjectile*>(projectileFace)->setLocalPredict(true);
            object->init();

            IPhysicsNode* const node = static_cast<IPhysicsNode*>(
                object_detail::LayoutAccess::physicsNode(object));
            if (node)
                node->setIsAwake();

            firearm()->setLocalPredictedProjectile(object);
        }
        else
        {
            float zDegrees;
            float xDegrees;
            float yDegrees;
            getRotationZXY(transform, zDegrees, xDegrees, yDegrees);

            const Vec3 position = {transform.m[12], transform.m[13], transform.m[14]};
            const Vec3 rotation = {-yDegrees, -xDegrees, -zDegrees};
            object = gameLogic->spawnObject(objectTemplate, position, rotation, 1);
        }
    }
    else if (useLocalPrediction)
    {
        object = objectTemplate->createObject();
        object->setAbsoluteTransformation(transform);

        IProjectileObject* const projectileFace = static_cast<IProjectileObject*>(
            object->queryInterface(IID_IProjectileObject));
        static_cast<GenericProjectile*>(projectileFace)->setLocalPredict(true);
        object->init();

        IPhysicsNode* const node = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(object));
        if (node)
            node->setIsAwake();

        firearm()->setLocalPredictedProjectile(object);
    }
    else
    {
        object = objectManager->createObject(objectTemplate, 0, 0);
        object->setAbsoluteTransformation(transform);
    }

    if (object && firearm()->getVelocity() > 150.0f)
        object->updateFlags(2, 1026);

    return object;
}

// .  The secondary face is converted back to its sole concrete implementation only
// after the caller's IID_IProjectileObject query succeeded; this avoids ABI-specific vtable slots.
void FireComp::setProjectileParameters(IProjectileObject* projectileFace,
                                       IPlayerControlObject* pco)
{
    GenericProjectile* const projectile = static_cast<GenericProjectile*>(projectileFace);
    IPlayer* const player = firearm()->getPlayer();

    projectile->setPlayer(player);
    if (player)
        projectile->setTeam(player->getTeam());

    projectile->setLauncherWeapon(firearm());
    if (!pco)
    {
        projectile->setLauncherVehicle(0);
    }
    else
    {
        projectile->setLauncherVehicle(
            static_cast<IObject*>(pco->queryInterface(IID_IObject)));
    }
}

// .  This const query updates a mutable cached matrix, as the shipped implementation
// does.  Several null quirks are intentional: a vehicle is assumed to expose a PCO, and camera
// fallbacks are dereferenced after their IID query without an additional guard.
const Mat4& FireComp::getProjectileOrigin() const
{
    IPlayer* const player = firearm()->getPlayerByForce();
    IObject* vehicle = 0;

    if (player)
    {
        vehicle = player->getVehicle();
        if (vehicle)
        {
            IPlayerControlObject* const vehiclePco = findPlayerControlObject(vehicle);
            if (vehiclePco->isChildSoldierActive())
                vehicle = vehiclePco->getOccupingObject();
        }
    }

    if (firearm()->m_byte208)
    {
        m_projectileOrigin = firearm()->getAbsoluteTransformation();
    }
    else if (!vehicle || vehicle->getTemplate()->getClassID() != CID_SoldierTemplate)
    {
        if (!m_template->getFireInCameraDof())
        {
            m_projectileOrigin = firearm()->getAbsoluteTransformation();
        }
        else if (m_template->getFireInCameraDofLocked())
        {
            m_projectileOrigin.mult(
                m_lockedCameraTransform, firearm()->getAbsoluteTransformation());
        }
        else
        {
            ICameraObject* const camera = player->getCamera();
            if (!camera || camera->getViewMode() != static_cast<CameraViewMode>(2))
            {
                // Retail asks the player for its camera a second time on this arm.
                ICameraObject* const currentCamera = player->getCamera();
                IObject* cameraObject = 0;
                if (currentCamera)
                {
                    cameraObject = static_cast<IObject*>(
                        currentCamera->queryInterface(IID_IObject));
                }
                m_projectileOrigin = cameraObject->getAbsoluteTransformation();
            }
            else
            {
                m_projectileOrigin = camera->getTransformation(0.0f);
            }
        }
    }
    else
    {
        IPlayerControlObject* const pco = findPlayerControlObject(firearm());
        if (!pco)
        {
            ICameraObject* const camera = player->getCamera();
            IObject* cameraObject = 0;
            if (camera)
                cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));
            m_projectileOrigin = cameraObject->getAbsoluteTransformation();
        }
        else
        {
            ICameraObject* const camera = pco->getActiveCamera();
            if (camera)
            {
                IObject* const cameraObject = static_cast<IObject*>(
                    camera->queryInterface(IID_IObject));
                if (cameraObject)
                    m_projectileOrigin = cameraObject->getAbsoluteTransformation();
            }
        }
    }

    normalizeAxis(m_projectileOrigin.m[8], m_projectileOrigin.m[9],
                  m_projectileOrigin.m[10]);
    normalizeAxis(m_projectileOrigin.m[4], m_projectileOrigin.m[5],
                  m_projectileOrigin.m[6]);
    normalizeAxis(m_projectileOrigin.m[0], m_projectileOrigin.m[1],
                  m_projectileOrigin.m[2]);

    const Vec3& offset = m_template->getProjectileStartPosition();
    m_projectileOrigin.m[12] += m_projectileOrigin.m[8] * offset.z;
    m_projectileOrigin.m[13] += m_projectileOrigin.m[9] * offset.z;
    m_projectileOrigin.m[14] += m_projectileOrigin.m[10] * offset.z;

    m_projectileOrigin.m[12] += m_projectileOrigin.m[4] * offset.y;
    m_projectileOrigin.m[13] += m_projectileOrigin.m[5] * offset.y;
    m_projectileOrigin.m[14] += m_projectileOrigin.m[6] * offset.y;

    m_projectileOrigin.m[12] += m_projectileOrigin.m[0] * offset.x;
    m_projectileOrigin.m[13] += m_projectileOrigin.m[1] * offset.x;
    m_projectileOrigin.m[14] += m_projectileOrigin.m[2] * offset.x;

    return m_projectileOrigin;
}

// .  `relative` means a second random sign choice in retail; the final clamp
// consequently turns its negative half into a zero-tick delay rather than taking an absolute value.
int FireComp::getFireStartDelayInTicks()
{
    int seed = firearm()->m_dword1C4;
    if (!seed)
        seed = WorldPref::getTickCount();
    m_random.m_seed = (static_cast<unsigned int>(seed) + 3323u) & 0x7FFFFFFFu;

    const RandomDistribution& distribution = m_template->getFireStartDelay();
    float sample = 0.0f;

    switch (distribution.distribution)
    {
    case CRD_NONE:
        sample = distribution.value;
        break;
    case CRD_UNIFORM:
        sample = distribution.value
            + (distribution.variation - distribution.value) * unitRandom(m_random);
        break;
    case CRD_EXPONENTIAL:
        sample = static_cast<float>(std::log(unitRandom(m_random)))
            * -distribution.value;
        break;
    case CRD_NORMAL:
        sample = distribution.value + m_random.getNormal() * distribution.variation;
        break;
    default:
        sample = 0.0f;
        break;
    }

    if (distribution.relative && unitRandom(m_random) <= 0.5f)
        sample = -sample;

    float nonNegative = 0.0f;
    if (nonNegative <= sample)
        nonNegative = sample;
    return static_cast<int>(nonNegative * 30.0f);
}

// PE 0x5C0E00.  The ignored-network arm still counts as a launched round and adds a
// null entry to the fired-projectile vector, matching the client prediction placeholder protocol.
bool FireComp::launchProjectile(int index)
{
    (void)index;

    IPlayer* const player = firearm()->getPlayerByForce();
    const Mat4& origin = getProjectileOrigin();
    const float muzzleVelocity = firearm()->getVelocity();
    Vec3 velocity = {
        origin.m[8] * muzzleVelocity,
        origin.m[9] * muzzleVelocity,
        origin.m[10] * muzzleVelocity
    };

    IObject* const root = getRootParent(firearm());
    IPhysicsNode* const rootNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(root));
    if (rootNode)
    {
        const Vec3& inherited = rootNode->getAbsolutePositionalSpeed();
        velocity.x += inherited.x;
        velocity.y += inherited.y;
        velocity.z += inherited.z;
    }

    bool launched = false;
    m_dummyShotsFired = 0;
    for (int shot = 0; shot < m_template->getBatchSize(); ++shot)
    {
        IObjectTemplate* const objectTemplate = firearm()->getProjectileTemplate();
        if (ignoreNetworkedProjectileOnClient(objectTemplate))
        {
            m_firedProjectiles.push_back(0);
        }
        else
        {
            IObject* const object = setupProjectile(origin, velocity);
            if (!object)
                continue;

            IProjectileObject* const projectileFace = static_cast<IProjectileObject*>(
                object->queryInterface(IID_IProjectileObject));
            if (projectileFace
                && static_cast<GenericProjectile*>(projectileFace)->getLocalPredict())
            {
                m_currentProjectile = static_cast<unsigned int>(WorldPref::getTickCount());
            }

            if (object->queryInterface(IID_IPlayerControlObject) && gameLogic->isServer())
            {
                void* const aiObject = object_detail::LayoutAccess::aiObject(object);
                if (aiObject)
                    eventManager->postEvent(ECAI, 19, aiObject, 0.0f);
                game->enterVehicle(player, object, false);
            }
        }
        launched = true;
    }

    if (m_allowEffects)
    {
        for (IObject* child = firearm()->getChild(); child; child = child->getSibling())
            child->handleMessage(static_cast<TemplateMessage>(0), player, false);
    }

    if (launched && m_template->getBatchSize() > 0)
        m_dummyShotsFired = m_template->getBatchSize() - 1;
    return launched;
}

// (3,657 B), PE 0x5C0770 (1,666 B).  This is the common projectile
// construction path shared by the ordinary, spawned, thrown and explosive fire components.  Its
// repeated interface walks and template queries are intentional: both shipped ports preserve
// them, along with the first-empty-seat weak entry and the tracer's second transform store.
IObject* FireComp::setupProjectile(const Mat4& transform, Vec3& velocity)
{
    IPlayer* const player = firearm()->getPlayer();

    IObjectTemplate* const secondaryTemplate =
        firearm()->getSecondaryProjectileTemplate();
    if (secondaryTemplate)
    {
        static_cast<ZiplineTemplate*>(secondaryTemplate)->createZiplineObject(
            player, const_cast<Mat4&>(transform), firearm());
    }

    IObjectTemplate* const primaryTemplate = firearm()->getProjectileTemplate();
    if (primaryTemplate
        && primaryTemplate->getClassID() == CID_GrapplingHookRopeTemplate)
    {
        // This arm exists in FireComp even though the value belongs to its SingleFireComp leaf.
        // Retail passes the tick dword's bits as a float; there is no integer-to-float conversion.
        const unsigned int completionBits =
            static_cast<SingleFireComp*>(this)->m_fireCompleteAtTick;
        float completionValue;
        std::memcpy(&completionValue, &completionBits, sizeof(completionValue));

        GrapplingHookRope* const rope =
            static_cast<GrapplingHookRopeTemplate*>(primaryTemplate)
                ->createGrapplingHookObject(
                    player, const_cast<Mat4&>(transform), completionValue);
        // The completed GrapplingHookRope leaf returns its primary IObject address in both ports,
        // so no pointer adjustment is present at this boundary.
        return reinterpret_cast<IObject*>(rope);
    }

    // Retail asks the firearm for its primary template again after the class-id probe.
    IObject* const object = createProjectile(firearm()->getProjectileTemplate(), transform);
    if (!object)
        return object;

    IProjectileObject* const projectileFace = static_cast<IProjectileObject*>(
        object->queryInterface(IID_IProjectileObject));
    if (projectileFace)
    {
        // GenericProjectile is the sole retail implementation.  Preserve the IID/null gate before
        // crossing back to its primary typed surface; the long secondary interface is unrecovered.
        GenericProjectile* const projectile =
            static_cast<GenericProjectile*>(projectileFace);

        m_firedProjectiles.push_back(object);
        IObject* const rootParent = getRootParent(firearm());

        const Vec3& position = object->getAbsolutePosition();
        const float aboveWater = position.y
            - static_cast<IHeightmapCluster*>(heightmapCluster)
                ->getHeightInWorldCoords(position.x, position.z);
        if (aboveWater < 0.0f)
        {
            // Both ports fetch and copy the transformation here even though the copied value is
            // not consulted afterwards.  Keep the virtual access and its observable ordering.
            const Mat4 underwaterTransform = object->getAbsoluteTransformation();
            (void)underwaterTransform;

            const float speed = std::sqrt(
                velocity.x * velocity.x
                + velocity.y * velocity.y
                + velocity.z * velocity.z);
            const float underwaterSpeed =
                projectile->getWaterVelocityDecreaseMod() * speed;

            normalizeAxis(velocity.x, velocity.y, velocity.z);
            velocity.x *= underwaterSpeed;
            velocity.y *= underwaterSpeed;
            velocity.z *= underwaterSpeed;
            projectile->setUnderWater(-aboveWater);
        }

        // The first PCO walk is deliberately unguarded before getTeam().  A second, independent
        // walk supplies setProjectileParameters after activation.
        IPlayerControlObject* const teamPco = findPlayerControlObject(firearm());
        projectile->setTeam(teamPco->getTeam());
        projectile->activate();
        setProjectileParameters(
            projectileFace, findPlayerControlObject(firearm()));

        IPhysicsNode* const projectileNode = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(object));
        if (projectileNode)
        {
            projectileNode->setIsAwake();
            projectileNode->setPositionalSpeed(velocity);
        }
        projectile->initOnLaunch();

        IPlayerControlObject* const rootPco = findPlayerControlObject(rootParent);
        if (rootPco)
        {
            std::map<int, IPlayerControlObject*>* const pcos = rootPco->getPcos();
            std::list<WeakPtr<IPlayer> > assistPlayers;

            bool firstSeat = true;
            for (std::map<int, IPlayerControlObject*>::iterator it = pcos->begin();
                 it != pcos->end(); ++it)
            {
                IPlayer* const occupant = it->second->getOccupingPlayer();
                if (firstSeat && !occupant)
                    assistPlayers.push_back(WeakPtr<IPlayer>());

                if (occupant && occupant != player)
                    assistPlayers.push_back(occupant->getWeakPtr());

                firstSeat = false;
            }
            projectile->setAssistPlayers(assistPlayers);
        }

        m_lastFiredProjectileId = static_cast<int>(object->getObjectId());

        if (projectile->hasTracer())
        {
            --m_postReloadState;
            if (m_postReloadState <= 0)
            {
                m_postReloadState = projectile->getTracerInterval();

                Mat4 tracerTransform = transform;
                IObject* const tracerObject =
                    createProjectile(firearm()->getProjectileTemplate(), tracerTransform);
                IProjectileObject* const tracerFace = static_cast<IProjectileObject*>(
                    tracerObject->queryInterface(IID_IProjectileObject));

                // There are intentionally no null guards on the tracer object or its projectile
                // face.  createProjectile already stored this transform; retail stores it again.
                tracerObject->setAbsoluteTransformation(tracerTransform);
                GenericProjectile* const tracer =
                    static_cast<GenericProjectile*>(tracerFace);
                tracer->turnIntoTracer(false);
                tracer->activate();
                setProjectileParameters(
                    tracerFace, findPlayerControlObject(firearm()));
                projectile->setAssociatedTracer(tracerFace);

                IPhysicsNode* const tracerNode = static_cast<IPhysicsNode*>(
                    object_detail::LayoutAccess::physicsNode(tracerObject));
                if (tracerNode)
                {
                    tracerNode->setIsAwake();
                    tracerNode->setPositionalSpeed(velocity);
                }

                // Retail asks for the interval again rather than reusing the stored value.
                if (projectileNode)
                {
                    if (projectile->getTracerInterval() <= 1)
                    {
                        projectileNode->setPositionalSpeed(velocity);
                    }
                    else
                    {
                        const Vec3 slowedVelocity = {
                            velocity.x * 0.75f,
                            velocity.y * 0.75f,
                            velocity.z * 0.75f
                        };
                        projectileNode->setPositionalSpeed(slowedVelocity);
                    }
                }
                tracer->initOnLaunch();
            }
        }
    }

    // The prediction query precedes the max-projectile test, including when the configured maximum
    // is zero.  The primary template lookup is repeated as it is in both shipped implementations.
    if (!getUseLocalPrediction(firearm()->getProjectileTemplate())
        && m_template->getMaxProjectilesInWorld() > 0)
    {
        for (std::list<WeakPtr<IObject> >::iterator it = m_projectiles.begin();
             it != m_projectiles.end();)
        {
            if (!it->get())
                it = m_projectiles.erase(it);
            else
                ++it;
        }

        while (static_cast<int>(m_projectiles.size())
               >= m_template->getMaxProjectilesInWorld())
        {
            game->destroyObject(m_projectiles.front().get());
            m_projectiles.pop_front();
        }
    }

    IProjectileObjectTemplate* projectileTemplate = 0;
    IObjectTemplate* const lifetimeTemplate = firearm()->getProjectileTemplate();
    if (lifetimeTemplate)
    {
        projectileTemplate = static_cast<IProjectileObjectTemplate*>(
            lifetimeTemplate->queryInterface(IID_IProjectileObjectTemplate));
    }

    float timeToLive = 0.0f;
    if (projectileTemplate)
    {
        // Preserve the repeated virtual getTimeToLive calls emitted by retail.
        if (projectileTemplate->getTimeToLive().distribution == CRD_NONE)
        {
            timeToLive = projectileTemplate->getTimeToLive().value + 1.0f;
        }
        else if (projectileTemplate->getTimeToLive().distribution == CRD_UNIFORM)
        {
            timeToLive = projectileTemplate->getTimeToLive().variation + 1.0f;
        }
        else
        {
            Debug(DEBUGTYPE_6,
                  "Game/Objects/Weapons/FireComponents/FireComp.cpp", 457, "")
                << "Indefinitive timeToliveParam";
        }
    }
    else
    {
        timeToLive = 9999.0f;
    }

    if (m_template->getMaxProjectilesInWorld() > 0 || timeToLive > 10.0f)
        m_projectiles.push_back(object->getWeakPtr());

    return object;
}

} // namespace world
} // namespace hfe
} // namespace dice
