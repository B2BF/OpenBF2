// Weapon lifecycle and component delegation through requestFire, doFire, and postFire.
// Preserve scheduled firing and tick-stamped prediction order; optional components retain their defined defaults.
// Unimplemented reachable behavior stops at a named [FRONTIER].
// [RETIRE-WHEN: unrecovered-factories-registered] Register remaining factories only once their required bodies exist.

#include "GenericFireArm.h"
#include "../Game/TriggerManager.h"
#include "FireComponents/FireComp.h"
#include "AmmoComponents/AmmoComp.h"           // IWeaponAmmoComp - the typed ammo delegates
#include "AnimationComponents/AnimationComp.h"
#include "RecoilComponents/WeaponBasedRecoilComp.h"
#include "ZoomComponents/ZoomComp.h"

#include "../../../AI/AIInterface/AIInstance.h"
#include "../GenericFireArmTemplate.h"
#include "../Game/Soldier.h"
#include "../TemplateLeaves.h"
#include "../GameLogicComp/IRadioGLComp.h"
#include "../IItemContainerObject.h"
#include "../IObject.h"
#include "../IWeaponHudComp.h"                 // disableWeapon's +0x40 setLockState on the HUD comp
#include "../ObjectLayoutAccess.h"
#include "../IPlayer.h"
#include "../Game/RotationalBundle.h"
#include "../Projectiles/GenericProjectile.h"
#include "../../IGameLogic.h"
#include "../../Utility/Vars.h"
#include "../../../Animation/BoneAnimation/AnimationSystem.h"
#include "../../../Core/Debug.h"
#include "../../../Core/EventManager.h"
#include "../../../Core/Ids.h"
#include "../Base/SimpleObjectTemplate.h"                    // m_aiTemplate, queryImplementation
#include "../../../AI/AIInterface/BFKnowledge/WeaponTemplateManager.h"  // the AI weapon lookup
#include "../../../AI/AIInterface/BFKnowledge/WeaponTemplate.h"         // createWeapon
#include "../../../AI/AIInterface/BFKnowledge/IWeapon.h"                // the component it builds
#include "../../../Core/StringUtil.h"
#include "../../../Core/System.h"                                     // WorldPref::getTickCount
#include "../../../IO/Console/IConsoleSaveable.h"
#include "../../../IO/Input/PlayerInput.h"
#include "../../../Math/Mat4.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Physics/SoldierResponse.h"
#include "../Projectiles/ProjectileCompTemplates.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../../Scene/Object/PlayerScoreData.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <vector>

namespace dice {
namespace hfe {

namespace ai {
// This singleton is null in the production no-bot server.  Its concrete IBFCollisionHandler
// interface is aidll-owned and intentionally not fabricated in the engine reconstruction.
extern IBase* g_bfCollisionHandler;
}

namespace world {

// ds:0x8ABAF24 (ELF) / dword_899500 (PE), published by Core/Globals.cpp.
extern IBase* heightmapCluster;

namespace {

// [TEMP] [CHOSEN, not recovered] - the agents.md frontier stop: name the method on stderr,
// flush, and never return. It never fabricates a default and lets execution continue.
[[noreturn]] void gfaFrontier(const char* method)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::world::GenericFireArm::%s reached\n", method);
    std::fflush(stderr);
    std::abort();
}

const float kTicksPerSecond   = 30.0f;   // flt_888B174 - the fixed timestep
const float kRedeployInterval =  2.0f;   // flt_888FFE4 - enableItem's deploy retry gap, in seconds
// ds:0x8AAC680.  Like the two other recovered readers, no writer exists in the dedicated server.
const bool g_damageDebugLocalPlayer = false;

// The three GenericFireArm.cpp TU statics recovered from both shipped ports. The reload factor is
// consumed by the specialized Ammo reload branch; the other two drive updateAcceptStateSyncAtTick.
int* const g_weaponDelayTicks        = Vars::getInt("wp-delay-ticks", 15);
int* const g_weaponDelayReloadFactor = Vars::getInt("wp-delay-reloadfactor", 1);
int* const g_weaponDelayPingFactor   = Vars::getInt("wp-delay-pingfactor", 2);
const std::vector<float> g_emptyRecoilVector;

} // namespace

// THE constructor - 0x824EEF0, 500 B, read end to end.
// Every store below is in the shipped write set. The tail is the pre-cache cascade that
// GenericFireArmTemplate.cpp predicted, which is why createObject no longer needs its interim hook.
GenericFireArm::GenericFireArm(const GenericFireArmTemplate* objectTemplate)
    : AnimatedBundle(objectTemplate)
    , m_refCount(1)
    , m_byte164(false)
    , m_blockInputRotBundle(0)
    , m_fireComp(0)
    , m_dword170(0)
    , m_zoomComp(0)
    , m_soundComp(0)
    , m_ammoComp(0)
    , m_recoilComp(0)
    , m_targetComp(0)
    , m_deviationComp(0)
    , m_animationComp(0)
    , m_alternateDisplayObject(0)
    , m_activeDisplayObject(0)
    , m_defaultSoundComp(0)
    , m_defaultAnimationComp(0)
    , m_heat(0.0f)
    , m_float1A4(0.0f)
    , m_heatSetTick(0)
    , m_itemType(objectTemplate->m_itemType)          // ctor copies template +0x1C4
    , m_networkSafeItemIndex(-1)                      // NOT 0 - and NOT the item index
    , m_itemEnabled(false)
    , m_deployDoneTick(0)
    , m_lastCoolTick(0)
    , m_showHudIcon(false)
    , m_dword1C4(0)
    , m_float1C8(10.0f)
    , m_template(objectTemplate)
    , m_byte1D4(false)
    , m_acceptStateSyncAtTick(0)
    , m_pendingActiveWeapon(0)
    , m_byte208(false)
    , m_enableTick(0)
    , m_byte210(false)
{
    // m_fireButtons[3] default-construct here - the ELF runs FireButtonState::FireButtonState()
    // exactly three times over +0x1F8/+0x1FC/+0x200. m_projectiles and m_player likewise.
    updateFlags(0x802, 0);

    // THE attested cascade site *  0x824F0C3 / 0x824F0DF.
    if (ObjectTemplate::getIsPreCaching())
        m_template->preCache();                       // template vslot +0x6C
}

// (D2) / 0x824F190 (D1) / 0x824F230 (D0), 154/154/161 B. The body is empty at source
// level: every instruction in it is compiler-generated teardown - the vptr resets, the
// std::_List_base<WeakPtr<IObject> >::__clear() + deallocate pair for m_projectiles, the WeakPtr
// release for m_player, then ~AnimatedBundle. Nothing hand-written to reproduce.
GenericFireArm::~GenericFireArm()
{
}

// queryInterface - 0x82565D0, 116 B. A flat if-else chain; it does NOT chain to the base's. The
// cast idiom mirrors AnimatedBundle::queryInterface so each face lands on its real sub-object.
void* GenericFireArm::queryInterface(unsigned int iid) const
{
    GenericFireArm* self = const_cast<GenericFireArm*>(this);
    if (iid == IID_IBase || iid == IID_IObject)
        return static_cast<IObject*>(self);                             // this + 0
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);                // this + 0x080
    if (iid == IID_IAnimatedBundle)
        return static_cast<IAnimatedBundle*>(self);                     // this + 0x13C
    if (iid == IID_IItemObject)
        return static_cast<IItemObject*>(self);                         // this + 0x154
    if (iid == IID_IWeaponObject)
        return static_cast<IWeaponObject*>(self);                       // this + 0x158
    if (iid == IID_IKitPart)
        return static_cast<IKitPart*>(self);                            // this + 0x15C
    return 0;
}

void GenericFireArm::addRef()  { ++m_refCount; }
int  GenericFireArm::getRef()  { return m_refCount; }
int  GenericFireArm::release()
{
    // / PE counterpart: the three extra faces share this counter.
    if (--m_refCount == 0)
    {
        delete this;
        return 0;
    }
    return m_refCount;
}

// object/cache lifecycle - recovered as one unit. Retail clears all ten non-owning component
// faces before a pooled object sleeps and re-queries them in the same order when it wakes. The
// inherited Bundle arms would otherwise retain dangling weapon-component pointers.

void GenericFireArm::initComponents()
{
    WeaponInitEvent event = { m_template, 0 };

    if (m_fireComp)
    {
        event.roundsPerMinute = m_fireComp->getRoundsPerMinute();
        m_fireComp->initComponent(event);

        IObject* alternate = m_fireComp->getAlternateObject();
        if (alternate)
        {
            m_alternateDisplayObject = alternate;
            setAlternateDisplayObject(
                static_cast<IObject*>(queryInterface(IID_IObject)),
                static_cast<AlternateDisplayMode>(1));
        }
    }

    if (m_ammoComp)
        m_ammoComp->initComponent(event);

    if (m_soundComp)
    {
        m_soundComp->initComponent(event);
        if (m_alternateDisplayObject)
        {
            IWeaponSoundComp* alternateSound = static_cast<IWeaponSoundComp*>(
                m_alternateDisplayObject->queryComponent(IID_IWeaponSoundComp,
                                                         IID_IWeaponSoundComp));
            event.firearmTemplate = static_cast<const GenericFireArmTemplate*>(
                m_alternateDisplayObject->getTemplate());
            if (alternateSound)
            {
                alternateSound->initComponent(event);
                alternateSound->setAlternateParent(this);
            }
        }
    }

    // Retail deliberately keeps an alternate object's template in `event` for these final two
    // calls.  Do not restore the primary template after the sound arm.
    if (m_targetComp)
        m_targetComp->initComponent(event);
    if (m_recoilComp)
        m_recoilComp->initComponent(event);
}

// . The AI weapon creation arm is bots-only. The production no-bot arm caches the
// eight interfaces, preserves the duplicate Sound/Animation defaults, then initializes Bundle.
void GenericFireArm::init()
{
    // Looks up the AI weapon only in a bots game, then constructs it only on a hit.
    // Preserve the temporary name copy and its destruction before testing the lookup result.
    if (hfe::ai::g_aiInstance && hfe::ai::aiInstance()->isBotsGame())
    {
        const SimpleObjectTemplate* const simpleTemplate =
            static_cast<const SimpleObjectTemplate*>(
                const_cast<GenericFireArmTemplate*>(m_template)
                    ->queryImplementation(CID_SimpleObjectTemplate));   // +0x50
        // The component map takes its own reference; release the creation reference after attaching.
        // [CHOSEN, not recovered] The template is held const, requiring a cast for queryImplementation.
        if (ai::WeaponTemplate* const weaponTemplate =
                ai::WeaponTemplateManager::m_instance->getTemplate(
                    std::string(simpleTemplate->m_aiTemplate)))
        {
            ai::IWeapon* const weapon = weaponTemplate->createWeapon(this);
            setComponent(ai::IID_IWeapon, weapon);
            weapon->release();
        }
    }

    m_fireComp = static_cast<IWeaponFireComp*>(
        queryComponent(IID_IWeaponFireComp, IID_IWeaponFireComp));
    m_ammoComp = static_cast<IWeaponAmmoComp*>(
        queryComponent(IID_IWeaponAmmoComp, IID_IWeaponAmmoComp));
    m_deviationComp = static_cast<IWeaponDeviationComp*>(
        queryComponent(IID_IWeaponDeviationComp, IID_IWeaponDeviationComp));
    m_recoilComp = static_cast<IWeaponRecoilComp*>(
        queryComponent(IID_IWeaponRecoilComp, IID_IWeaponRecoilComp));
    m_soundComp = static_cast<IWeaponSoundComp*>(
        queryComponent(IID_IWeaponSoundComp, IID_IWeaponSoundComp));
    m_zoomComp = static_cast<IWeaponZoomComp*>(
        queryComponent(IID_IWeaponZoomComp, IID_IWeaponZoomComp));
    m_targetComp = static_cast<IWeaponTargetComp*>(
        queryComponent(IID_IWeaponTargetComp, IID_IWeaponTargetComp));
    m_animationComp = static_cast<IWeaponAnimationComp*>(
        queryComponent(IID_IWeaponAnimationComp, IID_IWeaponAnimationComp));
    m_defaultSoundComp = m_soundComp;
    m_defaultAnimationComp = m_animationComp;

    Bundle::init();
    initComponents();

    // gates only on template +493. The dispatcher owns its disabled-item guard.
    if (m_template->m_canInternalHandleMessage)
        internalHandleMessage(static_cast<TemplateMessage>(8), 0);

    // IObjectComponent::init (+0x14) on the AI weapon attached above, with this firearm (ELF
    // ..0x824F436, PE 0x58639F..0x5863B6): WeaponObject::init binds the firearm's
    // IWeaponObject face and, once per AI template and never while precaching, measures it.
    if (IObjectComponent* const weapon = static_cast<IObjectComponent*>(
            queryComponent(ai::IID_IWeapon, ai::IID_IWeapon)))
        weapon->init(this);
}

void GenericFireArm::setLocalPredictedProjectile(IObject* projectile)
{
    if (projectile)
        m_projectiles.push_front(projectile->getWeakPtr());
}

IObject* GenericFireArm::getLocalPredictedProjectile()
{
    while (!m_projectiles.empty())
    {
        IObject* projectile = m_projectiles.front().get();
        m_projectiles.pop_front();
        if (projectile)
            return projectile;
    }
    return 0;
}

void GenericFireArm::resetFirearm()
{
    IObject* projectile = 0;
    while ((projectile = getLocalPredictedProjectile()) != 0)
        objectManager->destroyObject(projectile);

    if (m_fireComp)
        m_fireComp->stoppedTracking();
    if (m_targetComp)
        m_targetComp->reset();
}

void GenericFireArm::destroy(bool force)
{
    resetFirearm();
    SimpleObject::destroy(force);
}

void GenericFireArm::disableCachedObject()
{
    resetFirearm();
    SimpleObject::disableCachedObject();

    m_deviationComp = 0;
    m_recoilComp = 0;
    m_soundComp = 0;
    m_zoomComp = 0;
    m_fireComp = 0;
    m_ammoComp = 0;
    m_targetComp = 0;
    m_animationComp = 0;
    m_defaultSoundComp = 0;
    m_defaultAnimationComp = 0;
}

// (493 B) / dedicated PE 0x583DA0 (341 B) - both and in agreement.
void GenericFireArm::enableCachedObject()
{
    Bundle::enableCachedObject();

    // Recreates the cached firearm's AI weapon only when the bots-game template lookup succeeds.
    // Preserve the temporary name copy and init's [CHOSEN] const_cast.
    if (hfe::ai::g_aiInstance && hfe::ai::aiInstance()->isBotsGame())
    {
        const SimpleObjectTemplate* const simpleTemplate =
            static_cast<const SimpleObjectTemplate*>(
                const_cast<GenericFireArmTemplate*>(m_template)
                    ->queryImplementation(CID_SimpleObjectTemplate));   // +0x50

        if (ai::WeaponTemplate* const weaponTemplate =
                ai::WeaponTemplateManager::m_instance->getTemplate(
                    std::string(simpleTemplate->m_aiTemplate)))
        {
            ai::IWeapon* const weapon = weaponTemplate->createWeapon(this);
            setComponent(ai::IID_IWeapon, weapon);
            weapon->release();
        }
    }

    m_fireComp = static_cast<IWeaponFireComp*>(
        queryComponent(IID_IWeaponFireComp, IID_IWeaponFireComp));
    m_ammoComp = static_cast<IWeaponAmmoComp*>(
        queryComponent(IID_IWeaponAmmoComp, IID_IWeaponAmmoComp));
    m_deviationComp = static_cast<IWeaponDeviationComp*>(
        queryComponent(IID_IWeaponDeviationComp, IID_IWeaponDeviationComp));
    m_recoilComp = static_cast<IWeaponRecoilComp*>(
        queryComponent(IID_IWeaponRecoilComp, IID_IWeaponRecoilComp));
    m_soundComp = static_cast<IWeaponSoundComp*>(
        queryComponent(IID_IWeaponSoundComp, IID_IWeaponSoundComp));
    m_zoomComp = static_cast<IWeaponZoomComp*>(
        queryComponent(IID_IWeaponZoomComp, IID_IWeaponZoomComp));
    m_targetComp = static_cast<IWeaponTargetComp*>(
        queryComponent(IID_IWeaponTargetComp, IID_IWeaponTargetComp));
    m_animationComp = static_cast<IWeaponAnimationComp*>(
        queryComponent(IID_IWeaponAnimationComp, IID_IWeaponAnimationComp));
    m_defaultSoundComp = m_soundComp;
    m_defaultAnimationComp = m_animationComp;

    initComponents();
    // `init(this)` (IObjectComponent +0x14) on the IID_IWeapon component (ELF 0x824F767, PE
    // ) - WeaponObject::init, as at the end of init() above.
    if (IObjectComponent* const weapon = static_cast<IObjectComponent*>(
            queryComponent(ai::IID_IWeapon, ai::IID_IWeapon)))
        weapon->init(this);
}

void GenericFireArm::setActiveAnimationSystem(int index)
{
    m_activeAnimationSystemIndex = index;
    if (m_animationComp)
        m_animationComp->setActiveAnimationSystem(m_animationSystems[index]);
    if (m_activeDisplayObject && m_animationComp != m_defaultAnimationComp)
    {
        IAnimatedBundle* const animated = static_cast<IAnimatedBundle*>(
            m_alternateDisplayObject->queryInterface(IID_IAnimatedBundle));
        animated->setActiveAnimationSystem(index);
        m_animationComp->setActiveAnimationSystem(
            static_cast<anim::IAnimationSystem*>(animated->getActiveAnimationSystem()));
    }
}

void* GenericFireArm::getActiveAnimationSystem()
{
    if (!m_activeDisplayObject || m_animationComp == m_defaultAnimationComp)
        return m_animationSystems[m_activeAnimationSystemIndex];
    IAnimatedBundle* const animated = static_cast<IAnimatedBundle*>(
        m_alternateDisplayObject->queryInterface(IID_IAnimatedBundle));
    return animated->getActiveAnimationSystem();
}

void GenericFireArm::updateGeometry()
{
    if (m_geometry && m_activeDisplayObject)
    {
        // / client PE 0x5E0610: suppress the original mesh while an
        // alternate display object supplies it. Both ABIs call geometry +0x50;
        // this is the whole-object transform, not the adjacent per-part slot.
#if defined(_MSC_VER) && defined(_M_IX86)
        typedef void (__thiscall* SetTransformation)(void*, const Mat4&);
#else
        typedef void (*SetTransformation)(void*, const Mat4&);
#endif
        reinterpret_cast<SetTransformation>(
            (*static_cast<void***>(m_geometry))[0x50 / 4])(
                m_geometry, Mat4::getZero());
    }
    else
    {
        Object::updateGeometry();
    }
}

// (2,476 B).  The complete Ammo tail is cross-checked against the PE call sequence:
// magazine limits/state are sampled before Fire preUpdate; Ammo updates before Fire and again
// after Recoil; reload-complete notifications run Fire/Ammo/Animation/Sound/Radio in that order;
// and a queued reload takes the Fire/Zoom/Target vote before doReload.  Animation remains an
// independent named boundary; it is not required by DefaultAmmoComp itself.
void GenericFireArm::handleUpdate(float deltaTime, unsigned int flags)
{
    if (flags & 4u)
    {
        if (m_recoilComp)
            m_recoilComp->updateComponent(deltaTime, flags);
        return;
    }

    if (m_template->m_canInternalHandleMessage)
        internalHandleMessage(static_cast<TemplateMessage>(8), 0);

    IPlayer* player = getPlayer();
    if (player && m_byte210 && player == playerManager->getLocalHumanPlayer())
    {
        if (player->getCurrentFiringButton() != getAltFireInput()
            || player->getIsManDown()
            || !player->getIsAlive())
        {
            eventManager->postEvent(ECHud, 31, this, 0.0f);
            m_byte210 = false;
        }
    }

    if (m_itemEnabled)
    {
        if (player)
        {
            static const io::PlayerInputMap firingButtons[3] = {
                io::PIFire, io::PIAltFire, io::PIFlareFire
            };
            for (int i = 0; i != 3; ++i)
            {
                player->setCurrentFiringButton(firingButtons[i]);
                internalHandleMessage(
                    static_cast<TemplateMessage>(m_fireButtons[i].handleUpdate()), player);
            }
        }

        if (m_byte1D4
            && static_cast<unsigned int>(WorldPref::getTickCount()) > m_acceptStateSyncAtTick
            && !busy(false))
        {
            processStateSyncedData();
        }
    }

    if (m_pendingActiveWeapon)
    {
        if (getPlayer())
        {
            IObject* closest = 0;
            for (IObject* object = this; object; object = object->getParent())
            {
                if (object->getNetworkable())
                {
                    closest = object;
                    break;
                }
            }

            if (closest && closest->getTemplate()->getClassID() == CID_SoldierTemplate)
            {
                IPlayerControlObject* pco = static_cast<IPlayerControlObject*>(
                    closest->queryInterface(IID_IPlayerControlObject));
                // Unchecked in retail after the Soldier class-id gate.
                pco->setActiveWeapon(m_pendingActiveWeapon, 0);
            }
        }
        m_pendingActiveWeapon = 0;
        return;
    }

    IPlayerControlObject* pco = 0;
    for (IObject* object = this; object && !pco; object = object->getParent())
    {
        pco = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
    }

    if (!busy(false))
    {
        const bool weaponIsActive = pco && pco->getOccupingPlayer()
            && (pco->getActiveWeapon(0) == this || pco->getActiveWeapon(1) == this);
        if (!weaponIsActive)
            m_float1C8 -= deltaTime;
    }

    if (m_float1C8 < 0.0f)
    {
        updateFlags(0, 0x800);
        if (m_soundComp)
            m_soundComp->fireStop(false);
    }

    if (!m_itemEnabled)
        return;

    bool canFire = true;
    int ammoInMag = 0;
    int magazines = 0;
    FireState fireState = static_cast<FireState>(0);

    if (m_ammoComp)
    {
        ammoInMag = getAmmoInMag(0);
        magazines = getCurrentNrOfMags();
        (void)m_ammoComp->getIsReloading();
    }

    if (m_fireComp)
    {
        m_fireComp->setMaxShots(ammoInMag, magazines);
        (void)m_fireComp->getFireRate();
        fireState = m_fireComp->getFireState();
    }

    if (m_zoomComp)
        canFire = m_zoomComp->allowTargetSeek();

    if (m_fireComp && m_fireComp->preUpdate())
        doFire(fireState, false);

    if (m_animationComp)
    {
        m_animationComp->updateOnZooming(getZoom());
        m_animationComp->updateOnFiring(fireState);
    }

    if (getFireState() == 0)
        coolWeapon(deltaTime);

    if (m_ammoComp)
    {
        m_ammoComp->updateComponent(deltaTime, flags);

        if (m_ammoComp->getSignalReloadComplete())
        {
            if (m_fireComp)      m_fireComp->postReload();
            m_ammoComp->postReload();
            if (m_animationComp) m_animationComp->postReload();
            if (m_soundComp)     m_soundComp->postReload();
            if (m_radioGLComponent)
                static_cast<IRadioGLComp*>(m_radioGLComponent)->setUseMessageEnabled(true);
        }

        if (m_ammoComp->getSignalReload()
            && (!m_fireComp || m_fireComp->requestReload(false))
            && (!m_zoomComp || m_zoomComp->requestReload(false))
            && (!m_targetComp || m_targetComp->requestReload(false)))
        {
            doReload();
            updateAcceptStateSyncAtTick(true);
        }
    }

    if (m_fireComp)
    {
        m_fireComp->updateComponent(deltaTime, flags);

        if (m_fireComp->getSignalComplete())
        {
            const unsigned int completedAt = m_fireComp->getSignalCompleteAtTick();
            if (m_ammoComp)      m_ammoComp->fireComplete(completedAt);
            if (m_zoomComp)      m_zoomComp->fireComplete(completedAt);
            if (m_animationComp) m_animationComp->fireComplete(completedAt);
        }

        if (m_fireComp->getSignalFire() && !busy(false))
        {
            if (m_ammoComp && m_ammoComp->requestFire(false, false))
                doFire(fireState, true);
            else
                m_fireComp->outOfAmmo(true);
            updateAcceptStateSyncAtTick(false);
        }
    }

    if (m_zoomComp)
    {
        m_zoomComp->updateComponent(deltaTime, flags);
        if (m_zoomComp->getSignalToggle())
            toggleCamera(static_cast<TemplateMessage>(7));

        const int zoom = getZoom();
        if (m_recoilComp)
            m_recoilComp->setZoom(zoom);
        if (m_deviationComp)
            m_deviationComp->setZoom(zoom);
        if (m_targetComp)
            m_targetComp->setZoom(zoom);
    }
    if (m_recoilComp)
        m_recoilComp->updateComponent(deltaTime, flags);
    if (m_ammoComp)
        m_ammoComp->updateComponent(deltaTime, flags);
    if (m_targetComp)
    {
        m_targetComp->setAllowTargetSeek(canFire);
        m_targetComp->updateComponent(deltaTime, flags);
    }
    if (m_animationComp)
    {
        m_animationComp->updateComponent(deltaTime, flags);
        if (m_animationComp->getSignalDelayShiftStarted() && m_soundComp)
            m_soundComp->reload(static_cast<ReloadState>(7));
    }

    (void)canFire;
    SimpleObject::handleUpdate(deltaTime, flags);
    (void)getAbsoluteTransformation();
}

// empty as read, and that is the point: a firearm has no networkable component of
// its own. It reaches the wire through the soldier that carries it (SoldierNetworkable's item
// slots), so installing one here would mint a second, competing ghost for the same object.
void GenericFireArm::initNetworkableComp()
{
}

// THE IItemObject face

// . THE body that clears "Unknown part in kit!".
// The retail body inlines a WeakPtr assignment (retain incoming, release old, store, release temp);
// agents.md says preserve the source-level smart-pointer operation, so it is written as one
// assignment here - WeakPtr::operator= already performs those steps in that order.
void GenericFireArm::handlePickup(IPlayer* player, IObject* container)
{
    m_player = player ? player->getWeakPtr() : WeakPtr<IPlayer>();

    const int index = getItemIndex();                       // primary vslot +0x1DC, as compiled

    IItemContainerObject* itemContainer = 0;
    if (container)
        itemContainer = static_cast<IItemContainerObject*>(
            container->queryInterface(IID_IItemContainerObject));

    // NOT null-checked, exactly as shipped. * This is the same dice idiom Soldier.cpp
    // documents four times over (isWalking, getYawRecoil, getPitchRecoil, updateSoldierSpawnPoint):
    // the queryInterface result is used raw. Reproduced rather than "fixed".
    itemContainer->addItem(static_cast<IObject*>(this), index);          // +0x10

    if (m_fireComp)
        m_fireComp->pickup();
}

// . The four recovered typed component notifications precede stowing and container
// removal; an omitted component preserves the shipped null branch.
void GenericFireArm::handleDrop(IPlayer*, IObject* container)
{
    if (m_deviationComp) m_deviationComp->setAIDeviation(0.0f);
    if (m_fireComp)      m_fireComp->stop();
    if (m_ammoComp)      m_ammoComp->stop();
    if (m_soundComp)     m_soundComp->stop();

    disableItem();
    const int index = getItemIndex();

    IItemContainerObject* itemContainer = 0;
    if (container)
        itemContainer = static_cast<IItemContainerObject*>(
            container->queryInterface(IID_IItemContainerObject));

    // Unchecked in retail, like handlePickup's addItem arm.
    itemContainer->removeItem(index);
    m_player = WeakPtr<IPlayer>();
}

// . * genuinely empty AS shipped. * A weapon does not store an owner id; see
// getOwnerId below, which derives it from the player instead. Not a stub - this IS the body.
void GenericFireArm::setOwnerId(int)
{
}

// . Derived, not stored. getPlayer() is called twice, as compiled.
int GenericFireArm::getOwnerId()
{
    if (!getPlayer())                                       // primary vslot +0x1C0
        return -1;
    return getPlayer()->getId();                            // IPlayer +0x54 = getId, fetch #2
}

void GenericFireArm::setItemType(int type) { m_itemType = type; }
int  GenericFireArm::getItemType() const   { return m_itemType; }

bool GenericFireArm::isReadyToUse() { return !busy(false); }

// . Draw / take out. Returns whether the weapon actually came up.
bool GenericFireArm::enableItem()
{
    if (m_itemEnabled)
        return false;
    if (!m_fireComp)
        return false;
    if (!m_fireComp->isSelectable())
        return false;

    if (m_fireComp->preEnable())
    {
        enableWeapon();
        // Written again after enableWeapon sets it: the repeated store is present in both ports.
        m_itemEnabled = true;
        return true;
    }

    const int elapsedTicks = WorldPref::getTickCount() - m_enableTick;
    if (m_soundComp
        && static_cast<float>(elapsedTicks) / kTicksPerSecond > kRedeployInterval)
    {
        m_soundComp->disable();
        m_soundComp->enable();
        m_enableTick = WorldPref::getTickCount();
    }
    return false;
}

// . Stow / put away.
void GenericFireArm::disableItem()
{
    if (!m_itemEnabled)
        return;
    m_pendingActiveWeapon = 0;
    disableWeapon();                                         // primary vslot +0x294
    m_itemEnabled = false;
}

// THE IKitPart face - and getRank, which serves IItemObject +0x38 and IKitPart +0x10 from ONE body.

// . A constant, not a field read.
int GenericFireArm::getRank() const { return 0; }

// and 0x8253A60, 20 B - TWO slots, ONE template field.
// m_template is NOT null-checked in either, as shipped.
int GenericFireArm::getItemIndex()     { return m_template->m_itemIndex; }
int GenericFireArm::getKitPartIndex()  { return m_template->m_itemIndex; }

// THE IWeaponObject face - the recovered slots.

// . * THE NULL default IS 0x40 == io::PINone, NOT 0.
// 0 is PIYaw, a real axis. Soldier::addWeapon sorts weapons into its three slots by this value, so
// returning 0 here would make every component-less weapon claim it fires on the yaw axis.
io::PlayerInputMap GenericFireArm::getFireInput() const
{
    if (!m_fireComp)
        return io::PINone;
    return m_fireComp->getFireInput();
}

FireRate GenericFireArm::getFireRate()
{
    if (!m_fireComp)
        return static_cast<FireRate>(0);
    return m_fireComp->getFireRate();
}

int GenericFireArm::getWeaponState() const
{
    if (!m_fireComp)
        return 0;
    return static_cast<int>(m_fireComp->getWeaponState());
}

bool GenericFireArm::canBeRefilled()
{
    if (!m_ammoComp)
        return false;
    return m_ammoComp->canBeRefilled();
}

int GenericFireArm::getMagSize()
{
    if (!m_ammoComp)
        return 0;
    return m_ammoComp->getMagSize();
}

int GenericFireArm::getMaxNrOfMags() const
{
    if (!m_ammoComp)
        return 0;
    return m_ammoComp->getMaxNrOfMags();
}

int GenericFireArm::getCurrentNrOfMags() const
{
    if (!m_ammoComp)
        return 0;
    return m_ammoComp->getMagVectorSize();
}

// . * TWO different defaults IN ONE function. * ecx is seeded to -1 and that is
// what a missing ammo component returns; the out-of-range-index path re-zeroes it and returns 0.
// A negative index is clamped to 0 first (cmovs), not rejected.
int GenericFireArm::getAmmoInMag(unsigned int mag) const
{
    if (!m_ammoComp)
        return -1;

    // declared unsigned, compared signed. The mangling says `j` (unsigned int) but the body
    // tests the argument for < 0 and clamps with a cmovs, so a caller passing -1 reads magazine 0.
    int index = static_cast<int>(mag);
    if (index < 0)
        index = 0;

    const std::vector<int>* const ammo = m_ammoComp->getAmmo();
    if (static_cast<unsigned int>(index) < ammo->size())
        return (*ammo)[index];
    return 0;
}

// . With no ammo component the input percent is returned unchanged, not 0.
float GenericFireArm::reloadAmmoPercent(float percent, bool full)
{
    if (!m_ammoComp)
        return percent;
    return m_ammoComp->refillPercent(percent, full);
}

int GenericFireArm::getZoom()
{
    if (!m_zoomComp)
        return 0;
    return m_zoomComp->getCameraNr();
}

// . * THE NULL default IS 1.0f (fld1), NOT 0. * Soldier::getMouseSpeedMod
// multiplies by this, so a 0 here would freeze mouse look for every weapon in the game.
float GenericFireArm::getMouseSpeedMod()
{
    if (!m_zoomComp)
        return 1.0f;
    return m_zoomComp->getMouseSpeedMod();
}

// . * defaults TO true * and only a template can say otherwise.
bool GenericFireArm::getWorksInWater() const
{
    if (!m_template)
        return true;
    return m_template->m_worksInWater;
}

// . Bit 0 is "firing"; then TWO different components each OR in their own value
// shifted left BY ONE, into the same bits. Reproduced as compiled - this is not a tidy bitfield.
int GenericFireArm::getFlags() const
{
    int flags = 0;
    if (m_fireComp)
    {
        if (m_fireComp->getFireStartDelay())
            flags |= 1;
        flags |= static_cast<int>(m_fireComp->getWeaponState() << 1);
    }
    if (m_targetComp)
        flags |= static_cast<int>(m_targetComp->getWeaponState() << 1);
    return flags;
}

bool GenericFireArm::isSelectable() const
{
    if (!m_template->m_isSelectable)                         // template +0x1EC, unchecked as shipped
        return false;
    if (m_fireComp && !m_fireComp->isSelectable())
        return false;
    if (!m_ammoComp)
        return true;
    if (m_ammoComp->isSelectable())
        return true;
    return m_fireComp && !m_fireComp->launchesProjectiles(-1);
}

// . Unselectable unless the fire component reports 1..3. The component slot is
// called twice, as compiled.
bool GenericFireArm::isUnselectable() const
{
    if (!m_fireComp)
        return true;
    if (static_cast<int>(m_fireComp->getFireState()) <= 0)
        return true;
    return static_cast<int>(m_fireComp->getFireState()) > 3;
}

// . * ALT-fire shares THE primary weapon'S heat pool. * When the template sets
// m_inheritHeatFromPrimary the weapon does NOT read its own m_heat: it walks UP the parent chain to
// the first IPlayerControlObject and returns that object's weapon 0 heat.
float GenericFireArm::getHeat() const
{
    if (!m_template->m_inheritHeatFromPrimary)               // template +0x1BC
        return m_heat;

    IObject* parent = m_parent;
    IPlayerControlObject* playerControlObject = 0;
    while (parent)
    {
        playerControlObject = static_cast<IPlayerControlObject*>(
            parent->queryInterface(IID_IPlayerControlObject));
        if (playerControlObject)
            break;
        parent = parent->getParent();
    }

    if (playerControlObject)
    {
        IObject* primary = playerControlObject->getActiveWeapon(0);
        if (primary)
            return static_cast<GenericFireArm*>(primary)->getHeat();
    }
    return 0.0f;
}

// . Notifies the old owner component only when the player actually changes.
void GenericFireArm::setPlayer(IPlayer* player)
{
    if (m_soundComp && player != m_player.get())
        m_soundComp->stop();
    m_player = player ? player->getWeakPtr() : WeakPtr<IPlayer>();
}

// . AI owners remain visible while the item is stowed; human owners do not.
IPlayer* GenericFireArm::getPlayer()
{
    IPlayer* player = m_player.get();
    if (player && player->getIsAIPlayer())
        return player;
    return m_itemEnabled ? player : 0;
}

// . Unlike getPlayer(), this bypasses the enabled-state policy.
IPlayer* GenericFireArm::getPlayerByForce() const
{
    return m_player.get();
}

// . Input only reaches the firearm locally while it is enabled. Message 6 is
// translated through the two-tick latch belonging to the owner's current firing button before the
// private dispatcher sees it.
void GenericFireArm::handleMessage(TemplateMessage message, IPlayer*, bool remote)
{
    if (remote || !m_itemEnabled)
        return;

    IPlayer* player = getPlayer();
    if (player && message == static_cast<TemplateMessage>(6))
    {
        switch (player->getCurrentFiringButton())
        {
        case io::PIFire:
            message = static_cast<TemplateMessage>(m_fireButtons[0].translateMessage(6));
            break;
        case io::PIAltFire:
            message = static_cast<TemplateMessage>(m_fireButtons[1].translateMessage(6));
            break;
        case io::PIFlareFire:
            message = static_cast<TemplateMessage>(m_fireButtons[2].translateMessage(6));
            break;
        default:
            break;
        }
    }

    internalHandleMessage(message, player);
}

// (1,302 B), cross-checked against the PE dispatcher.  The infinite-ammo busy arm
// deliberately plays the fail message only while the shared damage-debug byte is clear.  Manual
// reload refuses state 2, takes the complete component vote, starts reload, then stamps the reload
// state-sync delay.
// [TEMP] [CHOSEN, not recovered] fire-mode toggle tracer, armed by BF2_TRACE_FIRERATE=1.
// Remove with the fire-mode investigation. [RETIRE-WHEN: firerate-toggle-diagnosed]
namespace {
bool gfaFireRateTraceArmed()
{
    static int armed = -1;
    if (armed < 0)
        armed = std::getenv("BF2_TRACE_FIRERATE") ? 1 : 0;
    return armed != 0;
}
} // namespace

void GenericFireArm::internalHandleMessage(TemplateMessage message, IPlayer* player)
{
    const int messageValue = static_cast<int>(message);

    const bool frTrace = messageValue == 11 && gfaFireRateTraceArmed();
    if (frTrace)
    {
        std::fprintf(stderr,
                     "[FRTRACE] TM11 tick=%d weapon=%s itemEnabled=%d fireComp=%d\n",
                     WorldPref::getTickCount(),
                     m_template ? m_template->getName().c_str() : "?",
                     m_itemEnabled ? 1 : 0, m_fireComp ? 1 : 0);
        std::fflush(stderr);
    }

    if (!m_itemEnabled || messageValue == 71)
        return;

    // Messages 44..53 bypass all weapon state work and go straight to Bundle.
    if (messageValue >= 44 && messageValue <= 53)
    {
        Bundle::handleMessage(message, player, false);
        return;
    }

    io::PlayerInputMap firingButton = io::PINone;
    if (player)
    {
        firingButton = player->getCurrentFiringButton();
        IObject* vehicle = player->getVehicle();
        if (vehicle
            && vehicle->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate)
        {
            IPlayerControlObject* pco = 0;
            for (IObject* object = vehicle; object && !pco; object = object->getParent())
            {
                pco = static_cast<IPlayerControlObject*>(
                    object->queryInterface(IID_IPlayerControlObject));
            }
            if (pco && firingButton != io::PINone
                && pco->checkSpecialToggleLastUpdate(firingButton))
                return;
        }
    }

    updateFlags(0x800, 0);
    m_float1C8 = 10.0f;

    FireRate fireRate = static_cast<FireRate>(0);
    if (m_fireComp)
    {
        fireRate = m_fireComp->getFireRate();
        (void)m_fireComp->getFireState();
        if (messageValue == 7 || messageValue == 8)
        {
            m_fireComp->setFirebuttonLastHeldAtTick(
                firingButton, static_cast<unsigned int>(WorldPref::getTickCount()));
        }
    }

    if (busy(false))
    {
        if (frTrace)
        {
            std::fprintf(stderr,
                         "[FRTRACE] TM11 DROPPED by busy(): deployDone=%u tick=%d ammo=%d fire=%d"
                         " zoom=%d anim=%d heat=%d\n",
                         static_cast<unsigned int>(m_deployDoneTick), WorldPref::getTickCount(),
                         m_ammoComp ? (m_ammoComp->busy() ? 1 : 0) : -1,
                         m_fireComp ? (m_fireComp->busy() ? 1 : 0) : -1,
                         m_zoomComp ? (m_zoomComp->busy() ? 1 : 0) : -1,
                         m_animationComp ? (m_animationComp->busy() ? 1 : 0) : -1,
                         getHasHeat() ? 1 : 0);
            std::fflush(stderr);
        }

        if (m_fireComp && (messageValue == 9 || getHasHeat()))
        {
            if (firingButton == m_fireComp->getFireInput()
                || firingButton == m_fireComp->getAltFireInput())
                fireStop(firingButton);
        }

        if (m_ammoComp && messageValue == 7
            && (m_ammoComp->getMag0Size() == -1
                || m_ammoComp->getMaxNrOfMags() == -1)
            && !g_damageDebugLocalPlayer
            && m_radioGLComponent)
        {
            static_cast<IRadioGLComp*>(m_radioGLComponent)->playFailMessage(true);
        }
        return;
    }

    switch (messageValue)
    {
    case 3:
        if (m_deviationComp)
            m_deviationComp->setAIDeviation(0.0f);
        setZoom(0);
        break;

    case 7:
    case 8:
        m_byte164 = true;
        if (m_fireComp)
        {
            if ((m_fireComp->getFireInput() == firingButton || m_byte208)
                && requestFire(messageValue == 8, false))
            {
                doFire(m_fireComp->getFireState(), false);
            }

            if (m_fireComp && m_fireComp->getAltFireInput() == firingButton
                && requestFire(messageValue == 8, true))
            {
                doFire(m_fireComp->getFireState(), false);
            }
        }
        if (m_zoomComp && m_zoomComp->canZoom()
            && m_zoomComp->getInput() == firingButton)
        {
            toggleCamera(message);
        }
        updateAcceptStateSyncAtTick(false);
        break;

    case 9:
        m_byte164 = false;
        fireStop(firingButton);
        updateAcceptStateSyncAtTick(false);
        break;

    case 10:
        if (m_ammoComp
            && static_cast<int>(m_ammoComp->getReloadState()) != 2
            && requestReload(true))
        {
            doReload();
        }
        updateAcceptStateSyncAtTick(true);
        break;

    case 11:
        if (m_fireComp)
            m_fireComp->switchFireRate();
        if (m_animationComp)
            m_animationComp->postSwitchFireRate(fireRate);
        if (m_soundComp)
            m_soundComp->postSwitchFireRate(fireRate);
        updateAcceptStateSyncAtTick(false);
        break;

    case 12:
    case 66:
        updateAcceptStateSyncAtTick(false);
        break;

    case 27:
        if (m_fireComp)
            m_fireComp->exit();
        break;

    case 28:
        if (m_zoomComp)
            m_zoomComp->changeViewMode();
        break;

    default:
        break;
    }

    Bundle::handleMessage(message, player, false);
}

// .  Components vote in the fixed Ammo/Fire/Zoom/Target/Animation order.  A failed
// Ammo vote has its own projectile-stop/out-of-ammo notification arm.  Retail only rejects the
// accumulated vote when that failed Ammo request was for a projectile-launching Fire component;
// otherwise evaluation resumes at Fire with the result still true.
bool GenericFireArm::requestFire(bool continuing, bool alternate)
{
    bool accepted = true;
    if (m_ammoComp && !m_ammoComp->requestFire(continuing, alternate))
    {
        if (m_fireComp && m_fireComp->launchesProjectiles(alternate ? 1 : 0))
        {
            fireStop(alternate ? m_fireComp->getAltFireInput() : m_fireComp->getFireInput());
            if (m_soundComp)
                m_soundComp->outOfAmmo(!continuing);
            if (m_radioGLComponent)
            {
                static_cast<IRadioGLComp*>(m_radioGLComponent)->playFailMessage(!continuing);
            }
            accepted = false;
        }
    }

    if (m_fireComp && !m_fireComp->requestFire(continuing, alternate))
        accepted = false;
    else if (m_zoomComp && !m_zoomComp->requestFire(continuing, alternate))
        accepted = false;
    else if (m_targetComp && !m_targetComp->requestFire(continuing, alternate))
        accepted = false;
    else if (m_animationComp && !m_animationComp->requestFire(continuing, alternate))
        accepted = false;

    Soldier* soldier = 0;
    if (m_parent->getTemplate()->getClassID() == CID_SoldierTemplate)
        soldier = static_cast<Soldier*>(m_parent);

    bool canFire = true;
    if (soldier)
        canFire = soldier->getCanFireWeapon();
    if (m_radioGLComponent && canFire)
    {
        static_cast<IRadioGLComp*>(m_radioGLComponent)->playUseMessage(!continuing);
    }
    return accepted;
}

// (1,196 B) / PE 0x584A80 (917 B). The secondary-projectile Soldier arm is the
// Special Forces zipline placement path. Its repeated parent/player/position calls and the split
// between notification 79 and placement-result notifications 80/81 are present in both ports.
void GenericFireArm::doFire(FireState requestedState, bool followup)
{
    int fireCount = 0;
    int dummyShots = 0;
    unsigned int sequence = 0;
    std::vector<IObject*>* firedProjectiles = 0;
    Vec3 start = Vec3::getZero();
    Vec3 end = Vec3::getZero();
    bool allowEffects = true;

    IObjectTemplate* secondary = getSecondaryProjectileTemplate();
    Soldier* soldier = 0;
    // Retail dereferences the parent unconditionally here; a firearm is an assembled bundle child.
    if (m_parent->getTemplate()->getClassID() == CID_SoldierTemplate)
        soldier = static_cast<Soldier*>(m_parent);

    // Retail asks the parent's template for its class id a second time instead of testing the
    // cached Soldier pointer. Keep that repeated virtual call and its ordering.
    if (secondary &&
        m_parent->getTemplate()->getClassID() == CID_SoldierTemplate)
    {
        const ZiplineTemplate* const ziplineTemplate =
            static_cast<const ZiplineTemplate*>(secondary);

        // getPlayer() is intentionally repeated on the live Fire-component arm.
        if (!m_fireComp || !getPlayer() ||
            getPlayer()->getCurrentFiringButton() != getAltFireInput())
        {
            if (!checkAreaToFireZipline(soldier, ziplineTemplate))
            {
                eventManager->postEvent(static_cast<EventCategory>(2), 79u, this, 0.0f);
                return;
            }
        }
        else
        {
            if (static_cast<int>(requestedState) == 0)
            {
                Vec3 endpoint = Vec3::getZero();
                if (ziplineTemplate->checkForRopeCreation(
                        getPlayer(), m_fireComp->getProjectileOrigin(),
                        soldier->getAbsolutePosition(), endpoint, true, 0) &&
                    checkAreaToFireZipline(soldier, ziplineTemplate))
                {
                    eventManager->postEvent(static_cast<EventCategory>(2), 81u, this, 0.0f);
                }
                else
                {
                    eventManager->postEvent(static_cast<EventCategory>(2), 80u, this, 0.0f);
                }
                m_byte210 = true;
            }
            return;
        }
    }

    const int requestedStateValue = static_cast<int>(requestedState);
    if (soldier && !soldier->getCanFireWeapon()
        && requestedStateValue != 2 && requestedStateValue != 3)
    {
        if (m_soundComp)
            m_soundComp->fireStop(false);
        return;
    }

    if (m_targetComp)
        (void)m_targetComp->getTargetObject();
    if (m_zoomComp)
        allowEffects = m_zoomComp->allowEffects();

    int maxShots = 99;
    int magazines = 99;
    if (m_ammoComp)
    {
        const int ammoInMag = getAmmoInMag(0);
        if (ammoInMag != -1)
            maxShots = ammoInMag;
        const int currentMags = getCurrentNrOfMags();
        if (currentMags != -1)
            magazines = currentMags;
    }

    if (m_fireComp)
    {
        m_fireComp->setMaxShots(maxShots, magazines);
        m_fireComp->setAllowEffects(allowEffects);
        fireCount = m_fireComp->fire(followup, -1);
        dummyShots = m_fireComp->getDummyShotsFired();
        firedProjectiles = m_fireComp->getFiredProjectiles();
        sequence = m_fireComp->getSequenceNr();
        start = m_fireComp->getStartPos();
        end = m_fireComp->getEndPos();

        if (fireCount > 0 && !followup && m_soundComp
            && m_soundComp->getNoisy() && triggerManager)
        {
            triggerManager->handleEvent(TriggerType_Weapon, this, -1);
        }
    }

    postFire(requestedState, fireCount, dummyShots, static_cast<int>(sequence),
             firedProjectiles, start, end);
}

// (950 B) / PE 0x5847C0 (701 B). This is the local placement gate used before
// ordinary fire and again after checkForRopeCreation. The response-physics bytes are sampled in
// current/previous order; the terrain path deliberately performs three position virtual calls.
bool GenericFireArm::checkAreaToFireZipline(Soldier* soldier,
                                             const ZiplineTemplate* ziplineTemplate)
{
    SoldierResponsePhysics* responsePhysics = 0;
    if (soldier)
    {
        responsePhysics = static_cast<SoldierResponsePhysics*>(
            object_detail::LayoutAccess::responsePhysics(soldier));
    }

    const bool currentGroundContact = responsePhysics ? responsePhysics->m_byteFA : false;
    const bool previousGroundContact = responsePhysics ? responsePhysics->m_byteF9 : false;
    if (!currentGroundContact &&
        (!previousGroundContact ||
         (m_fireComp && m_fireComp->isFireLaunchDelay())))
    {
        return false;
    }

    if (soldier->m_byte398)
    {
        // The x and z argument evaluations make two independent virtual calls, then a third call
        // supplies y after the terrain result has been materialised.
        const float terrainHeight =
            static_cast<IHeightmapCluster*>(heightmapCluster)->getHeightInWorldCoords(
                soldier->getAbsolutePosition().x,
                soldier->getAbsolutePosition().z);
        if (soldier->getAbsolutePosition().y - terrainHeight > 1.5f)
            return false;
    }

    // Neither port null-checks the player here, and both repeat GenericFireArm::getPlayer().
    IObject* const defaultVehicle = getPlayer()->getDefaultVehicle();
    if (defaultVehicle != getPlayer()->getVehicle())
        return false;

    if (m_fireComp &&
        (soldier->getPose() == SoldierPose_Prone ||
         soldier->getPose() == SoldierPose_Crouch) &&
        ziplineTemplate->checkForVerticalObstruction(soldier))
    {
        return false;
    }

    ObjectFlagPredicator downwardPredicator(0x100u);
    IObject* hitObject = 0;
    Vec3 hitPoint = Vec3::getZero();
    Vec3 hitNormal = Vec3::getZero();
    int hitPart; // retail leaves this output word uninitialised; intersectLine supplies it
    const Vec3 downward = { 0.0f, -2.0f, 0.0f };
    objectManager->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, soldier->getAbsolutePosition(), downward,
        &downwardPredicator, true, false, false, true, CID_SoldierTemplate);
    if (hitObject &&
        hitObject->getTemplate()->getClassID() == CID_TriggerableTargetTemplate)
    {
        return false;
    }

    std::vector<IObject*> nearbyObjects;
    ObjectFlagPredicator nearbyPredicator(0x200u);
    const Vec3 centre = soldier->getAbsolutePosition();
    const float radius = ziplineTemplate->m_entryRadius;
    objectManager->getObjectsFromGrid(centre, radius, nearbyObjects, &nearbyPredicator);

    for (std::vector<IObject*>::const_iterator it = nearbyObjects.begin();
         it != nearbyObjects.end(); ++it)
    {
        IObject* const object = *it;
        if (object->getTemplate()->getClassID() == CID_ZiplineTemplate)
        {
            const Vec3& position = object->getAbsolutePosition();
            const float x = position.x - centre.x;
            const float y = position.y - centre.y;
            const float z = position.z - centre.z;
            if (radius >= std::sqrt(x * x + y * y + z * z))
                return false;
        }
    }
    return true;
}

// .  The complete dispatcher.  WeaponFireEvent is the exact 0x3C-byte POD passed
// to every component's common +0x38 slot.  Retail deliberately leaves its padding and the sound-
// only maximum word untouched until those arms need them.
void GenericFireArm::postFire(FireState requestedState,
                              int fireCount,
                              int dummyShots,
                              int sequence,
                              std::vector<IObject*>* firedProjectiles,
                              Vec3& start,
                              Vec3& end)
{
    WeaponFireEvent event;
    event.fireCount = fireCount;
    event.sequenceNumber = sequence;
    event.requestedStateAboveTwo = static_cast<int>(requestedState) > 2;
    event.componentState = false;
    event.start = start;
    event.end = end;
    event.fireRate = static_cast<FireRate>(0);
    event.fireStrength = 1.0f;

    int currentState = 0;
    if (m_fireComp)
    {
        event.fireRate = m_fireComp->getFireRate();
        currentState = static_cast<int>(m_fireComp->getFireState());
        event.fireStrength = m_fireComp->getFireStrength();
    }

    bool alternateStateFour = false;
    if (currentState == 4)
    {
        alternateStateFour = static_cast<int>(requestedState) != 3;

        if (!g_damageDebugLocalPlayer)
            m_heat += m_template->m_heatAddWhenFire;

        if (m_heat > 1.0f && !m_template->m_inheritHeatFromPrimary)
        {
            m_heat = m_template->m_overheatPenalty;
            if (m_soundComp)
                m_soundComp->fireStop(false);
        }
        m_float1A4 = m_heat;
        m_heatSetTick = WorldPref::getTickCount();

        if (firedProjectiles)
        {
            const unsigned int size = static_cast<unsigned int>(firedProjectiles->size());
            const unsigned int dummyStart = size - static_cast<unsigned int>(dummyShots);
            for (unsigned int i = 0; i < size; ++i)
            {
                const bool dummy = i >= dummyStart;
                IObject* projectileObject = (*firedProjectiles)[i];
                event.projectileIndex = static_cast<int>(i);
                event.dummyProjectile = dummy;
                event.projectile = projectileObject;

                if (m_deviationComp)
                    m_deviationComp->postFire(event);

                IProjectileObject* projectileFace = projectileObject
                    ? static_cast<IProjectileObject*>(
                        projectileObject->queryInterface(IID_IProjectileObject))
                    : 0;
                if (projectileFace)
                {
                    // GenericProjectile is the sole retail IProjectileObject implementation.
                    static_cast<GenericProjectile*>(projectileFace)->setCountHit(!dummy);
                }

                if (!dummy)
                {
                    if (m_recoilComp)
                        m_recoilComp->postFire(event);
                    if (m_targetComp)
                        m_targetComp->postFire(event);
                    if (m_zoomComp)
                        m_zoomComp->postFire(event);
                    if (ai::g_bfCollisionHandler && projectileObject)
                        gfaFrontier("postFire (AIDLL IBFCollisionHandler fire report)");
                }
            }

            // The repeated owner lookup is shipped behavior.
            if (getPlayer())
            {
                IPlayer* owner = getPlayer();
                PlayerScoreData* score = owner->getScoreData();
                if (score)
                {
                    score->addBulletsFired(
                        m_template->getId(),
                        static_cast<int>(size) - dummyShots);
                }
            }
        }
    }

    if (currentState > 1 && m_ammoComp)
        m_ammoComp->postFire(event);

    if ((currentState == 3 || (alternateStateFour && currentState == 4))
        )
    {
        event.componentState = static_cast<unsigned int>(currentState - 2) <= 1u;
        if (m_animationComp)
        {
            int ammoLeft = 0;
            if (m_ammoComp)
                ammoLeft = *m_ammoComp->getAmmo()->begin();
            m_animationComp->setAmmoLeft(ammoLeft);
            m_animationComp->postFire(event);
        }
    }

    const bool soundFallback = static_cast<int>(requestedState) == 0
                            && (currentState == 2 || currentState == 4);
    if (!m_soundComp || (fireCount <= 0 && !soundFallback))
        return;

    // The fallback changes only the count used by the limiting helper.  event.fireCount retains
    // the caller's original value, exactly as the ELF stack writes show.
    int soundFireCount = fireCount;
    if (soundFallback && soundFireCount == 0)
        soundFireCount = 1;

    event.maxProjectilesToFire = getMaxProjectilesToFire(soundFireCount);
    const int maxSounds = m_soundComp->getMaxSoundsPerBurst();
    if (maxSounds >= 0)
    {
        if (sequence >= maxSounds)
            return;
        if (maxSounds <= event.maxProjectilesToFire)
            event.maxProjectilesToFire = maxSounds;
    }
    event.componentState = currentState == 2;
    m_soundComp->postFire(event);
}

// .  Burst/single/automatic select different sound-projectile limits.  The first
// Fire call is intentionally unguarded: this helper is only entered from postFire's live Sound
// arm, and retail assumes that such a weapon also owns a Fire component.
int GenericFireArm::getMaxProjectilesToFire(int fireCount)
{
    int result = 0;
    const int burstSize = m_fireComp->getBurstSize();
    FireRate fireRate = static_cast<FireRate>(0);
    if (m_fireComp)
        fireRate = m_fireComp->getFireRate();

    if (m_ammoComp)
    {
        std::vector<int>* ammo = m_ammoComp->getAmmo();
        const int magSize = m_ammoComp->getMagSize();
        if (ammo && !ammo->empty())
        {
            const int loaded = (*ammo)[0];
            const int total = static_cast<int>(static_cast<unsigned int>(loaded)
                                             + static_cast<unsigned int>(fireCount));
            const int rate = static_cast<int>(fireRate);
            if (rate == 1)
                result = burstSize;
            else if (rate <= 1)
            {
                if (rate == 0)
                    result = 1;
            }
            else if (rate == 2)
            {
                result = INT_MAX;
                if (total >= 0)
                    result = magSize;
            }

            if (total < fireCount)
                return result;
            if (loaded >= 0 && total <= result)
                result = total;
        }
    }
    return result;
}

// .  The secondary flag is derived by asking for the alternate input a second time;
// getFireState is an observable but otherwise discarded call before the stop cascade.
void GenericFireArm::fireStop(io::PlayerInputMap input)
{
    if (!m_fireComp
        || (input != m_fireComp->getFireInput() && input != m_fireComp->getAltFireInput()))
        return;

    const bool alternate = input == m_fireComp->getAltFireInput();
    (void)m_fireComp->getFireState();
    m_fireComp->fireStop(alternate);
    if (m_ammoComp)      m_ammoComp->fireStop(alternate);
    if (m_soundComp)     m_soundComp->fireStop(alternate);
    if (m_targetComp)    m_targetComp->fireStop(alternate);
    if (m_animationComp) m_animationComp->fireStop(alternate);
    if (m_radioGLComponent)
        static_cast<IRadioGLComp*>(m_radioGLComponent)->setUseMessageEnabled(true);
}

// (267 B).  A rejecting Ammo vote still publishes its current ReloadState to the
// animation component before returning false; no other component receives that rejected state.
bool GenericFireArm::requestReload(bool requested)
{
    if (!m_ammoComp)
        return false;
    if (m_fireComp && !m_fireComp->requestReload(requested))
        return false;
    if (m_zoomComp && !m_zoomComp->requestReload(requested))
        return false;
    if (m_targetComp && !m_targetComp->requestReload(requested))
        return false;
    if (m_ammoComp->requestReload(requested))
        return true;

    const ReloadState state = m_ammoComp->getReloadState();
    if (m_animationComp)
        m_animationComp->reload(state);
    return false;
}

// (174 B).  Retail assumes Ammo is non-null.  Ammo begins first, its resulting state
// is sampled once, then propagated Fire -> Zoom -> Sound -> Animation -> Target.
void GenericFireArm::doReload()
{
    m_ammoComp->reload();
    const ReloadState state = m_ammoComp->getReloadState();
    if (m_fireComp)      m_fireComp->reload(state);
    if (m_zoomComp)      m_zoomComp->reload(state);
    if (m_soundComp)     m_soundComp->reload(state);
    if (m_animationComp) m_animationComp->reload(state);
    if (m_targetComp)    m_targetComp->reload(state);
}

// . Draws the weapon: re-arms the 10.0f timer, stamps the deploy-done tick from
// the template's cached tick count, back-dates cooling over the time the weapon was stowed, then
// enables each attached component IN this fixed order - ammo, target, fire, sound, zoom, animation.
void GenericFireArm::enableWeapon()
{
    if (m_itemEnabled)
        return;

    updateFlags(0x800, 1);
    m_float1C8 = 10.0f;
    m_deployDoneTick = WorldPref::getTickCount() + m_template->m_delayToUseTimes30;   // template +0x1A0
    m_byte1D4 = false;

    const int stowedTicks = WorldPref::getTickCount() - m_lastCoolTick;
    if (stowedTicks > 0)
        coolWeapon(static_cast<float>(stowedTicks) / kTicksPerSecond);

    if (m_ammoComp)      m_ammoComp->enable();
    if (m_targetComp)    m_targetComp->enable();
    if (m_fireComp)      m_fireComp->enable();
    if (m_soundComp)     m_soundComp->enable();
    if (m_zoomComp)      m_zoomComp->enable();
    if (m_animationComp) m_animationComp->enable();

    // compare(), not empty() - the shipped call is std::string::compare(char const*) against a
    // zero-length literal. "If the template names a rot-bundle input, block it."
    if (m_template->m_blockInputOnRotBundle.compare("") != 0)
        setBlockRotBundleInput(true);

    m_itemEnabled = true;

    if (is1P())
        eventManager->postEvent(ECHud, 31, this, 0.0f);
}

// Disables an enabled weapon and its components. Returns immediately for weapons never enabled.
void GenericFireArm::disableWeapon()
{
    if (!m_itemEnabled)
        return;

    if (m_alternateDisplayObject)
        setAlternateDisplayObject(
            static_cast<IObject*>(queryInterface(IID_IObject)),
            static_cast<AlternateDisplayMode>(2));

    for (int i = 0; i != 3; ++i)
        m_fireButtons[i].reset();

    if (m_animationSystems[0])
        static_cast<anim::IAnimationSystem*>(m_animationSystems[0])->disable(true);
    if (m_animationSystems[1])
        static_cast<anim::IAnimationSystem*>(m_animationSystems[1])->disable(true);
    if (m_animationComp)
        m_animationComp->disable();

    if (!m_parent || m_parent->getTemplate()->getClassID() == CID_SoldierTemplate)
        updateFlags(1, 0);

    if (m_soundComp)  m_soundComp->disable();
    if (m_targetComp) m_targetComp->disable();
    if (m_fireComp)   m_fireComp->disable();
    if (m_ammoComp)   m_ammoComp->disable();
    if (m_zoomComp)   m_zoomComp->disable();
    if (m_recoilComp) m_recoilComp->disable();

    // Clears the returned IObject face's weapon-HUD lock during teardown.
    // Keep both nested null tests and the setLockState(0) call.
    IObject* const objectFace = static_cast<IObject*>(queryInterface(IID_IObject));
    if (objectFace)
    {
        // Through the tree's existing layout accessor because the field is protected on IObject
        // and this reads it off the returned face rather than off `this`; the same bridge
        // VehicleHudCompTemplate.cpp uses for the identical field.
        IObjectComponent* const hudComp =
            object_detail::LayoutAccess::weaponHudComponent(objectFace);         // +0x5C
        if (hudComp)
            static_cast<IWeaponHudComp*>(hudComp)->setLockState(
                static_cast<WarningState>(0));                                   // +0x40
    }

    if (m_template->m_blockInputOnRotBundle.compare("") != 0)
        setBlockRotBundleInput(false);

    m_lastCoolTick = WorldPref::getTickCount();
    m_itemEnabled = false;

    if (is1P())
        eventManager->postEvent(ECHud, 31, this, 0.0f);
}

// .  This is the shared display-object switch used during component assembly,
// explosives detonator transitions, and shutdown.  The apparently redundant virtual queries and
// the asymmetric sound/animation mode tests are present in the shipped instructions.
void GenericFireArm::setAlternateDisplayObject(IObject* object, AlternateDisplayMode mode)
{
    IObject* self = static_cast<IObject*>(queryInterface(IID_IObject));
    const int modeValue = static_cast<int>(mode);

    if (object == self)
    {
        if (!m_alternateDisplayObject)
            return;

        updateFlags(2, 0);
        m_alternateDisplayObject->updateFlags(1, 2);

        if (m_soundComp && m_soundComp != m_defaultSoundComp)
        {
            if (modeValue != 1)
                m_soundComp->disable();
            m_soundComp = m_defaultSoundComp;
            if (modeValue == 0)
                m_soundComp->enable();
        }

        if (m_animationComp && m_animationComp != m_defaultAnimationComp)
        {
            if (modeValue != 1)
                m_animationComp->disable();
            m_animationComp = m_defaultAnimationComp;
            if (modeValue != 1)
                m_animationComp->enable();
        }

        IAnimatedBundle* alternateAnimated = static_cast<IAnimatedBundle*>(
            m_alternateDisplayObject->queryInterface(IID_IAnimatedBundle));
        anim::IAnimationSystem* alternateAnimationSystem =
            static_cast<anim::IAnimationSystem*>(alternateAnimated->getActiveAnimationSystem());
        alternateAnimationSystem->disable(true);
        m_activeDisplayObject = 0;
        return;
    }

    updateFlags(2, 0);
    object->updateFlags(2, 1);

    IWeaponSoundComp* alternateSound = static_cast<IWeaponSoundComp*>(
        object->queryComponent(IID_IWeaponSoundComp, IID_IWeaponSoundComp));
    if (alternateSound)
    {
        if (modeValue != 1 && m_soundComp)
            m_soundComp->disable();
        m_soundComp = alternateSound;
        if (modeValue == 0)
            alternateSound->enable();
    }

    IWeaponAnimationComp* alternateAnimation = static_cast<IWeaponAnimationComp*>(
        object->queryComponent(IID_IWeaponAnimationComp, IID_IWeaponAnimationComp));
    if (alternateAnimation)
    {
        if (modeValue != 1 && m_animationComp)
            m_animationComp->disable();
        m_animationComp = alternateAnimation;
        if (modeValue != 1)
            alternateAnimation->enable();
    }

    IAnimatedBundle* animated = static_cast<IAnimatedBundle*>(
        queryInterface(IID_IAnimatedBundle));
    anim::IAnimationSystem* animationSystem =
        static_cast<anim::IAnimationSystem*>(animated->getActiveAnimationSystem());
    animationSystem->disable(true);
    m_activeDisplayObject = object;
}

// . The complete PlayerInput forwarding arm needs IWeaponDeviationComp; a missing
// component is a genuine no-op.
void GenericFireArm::updateDeviation(io::PlayerInput input, bool movementState)
{
    if (m_deviationComp)
        m_deviationComp->updateDeviation(input, movementState, getFireState() != 0);
}

// . Notify Fire first, then Target; no component means no work.
void GenericFireArm::projectileWasReset(IObject* projectile)
{
    if (m_fireComp)
        m_fireComp->projectileWasReset(projectile);
    if (m_targetComp)
        m_targetComp->projectileWasReset(projectile);
}

bool GenericFireArm::busy(bool exceptSelf)
{
    return busyExcept(0, exceptSelf);
}

// . Every component uses its shared busy slot +0x6C. Until IWeaponComp exists as
// a dispatchable interface, preserve the exclusion/null ordering and stop only on a live delegate.
bool GenericFireArm::busyExcept(IWeaponComp* excluded, bool exceptSelf)
{
    if (!exceptSelf
        && static_cast<unsigned int>(m_deployDoneTick)
           > static_cast<unsigned int>(WorldPref::getTickCount()))
        return true;

    if (m_ammoComp && m_ammoComp != excluded && m_ammoComp->busy())
        return true;
    if (m_fireComp && static_cast<IWeaponComp*>(m_fireComp) != excluded && m_fireComp->busy())
        return true;
    if (m_zoomComp
        && static_cast<IWeaponComp*>(m_zoomComp) != excluded
        && m_zoomComp->busy())
        return true;
    if (m_animationComp && m_animationComp != excluded && m_animationComp->busy())
        return true;

    if (excluded)
        return false;
    return getHasHeat();
}

// . Preserve the repeated virtual owner lookup.
bool GenericFireArm::is1P()
{
    if (!getPlayer())
        return false;
    return getPlayer()->hasFirstPersonView();
}

// . The float argument is genuinely unused in both shipped images: cooling is
// calculated from the stamped tick delta. The zero-extension before `fild qword` makes it unsigned.
void GenericFireArm::coolWeapon(float)
{
    const unsigned int elapsedTicks =
        static_cast<unsigned int>(WorldPref::getTickCount())
        - static_cast<unsigned int>(m_heatSetTick);
    m_heat = m_float1A4
           - (static_cast<float>(elapsedTicks) / kTicksPerSecond) * m_template->m_coolDownPerSec;
    if (m_heat < 0.0f)
        m_heat = 0.0f;

    if (gameLogic->isServer())
    {
        m_float1A4 = m_heat;
        m_heatSetTick = WorldPref::getTickCount();
    }
}

// . Cache the first case-insensitive template-name match under the root and make
// that RotationalBundle swallow this weapon owner's input while the weapon is active.
void GenericFireArm::setBlockRotBundleInput(bool enabled)
{
    if (!m_blockInputRotBundle)
    {
        std::vector<IObject*> children;
        getChilds(getRootParent(this), children, 0, 0);
        for (std::vector<IObject*>::iterator it = children.begin(); it != children.end(); ++it)
        {
            IObject* child = *it;
            if (!compareIgnoreCase(child->getTemplate()->getName(),
                                   m_template->m_blockInputOnRotBundle))
            {
                m_blockInputRotBundle = static_cast<RotationalBundle*>(child);
                break;
            }
        }

        if (!m_blockInputRotBundle)
        {
            IObject* root = getRootParent(this);
            Debug(DEBUGTYPE_6, "Game/Objects/Weapons/GenericFireArm.cpp", 1572)
                << "Template " << m_template->m_blockInputOnRotBundle
                << " not found in object hierarchy. RootObject:"
                << root->getTemplate()->getName();
        }
    }

    if (m_blockInputRotBundle)
        m_blockInputRotBundle->m_inputOwner = enabled ? m_player.get() : 0;
}

// , 310 B.  This is the reconciliation horizon used by the
// firearm and SingleFireComp.  Ping and the two component accept delays are seconds; retail
// converts their sum to 30 Hz ticks by truncation, clamps to wp-delay-ticks, never moves the
// existing horizon backwards, and snaps an implausible (>10 s) horizon back to the minimum.
unsigned int GenericFireArm::updateAcceptStateSyncAtTick(bool includeDelays)
{
    if (*g_weaponDelayTicks <= 0)
    {
        m_acceptStateSyncAtTick = 0;
        return 0;
    }

    float pingDelay = 0.0f;
    if (includeDelays && m_player.get())
    {
        const unsigned int weightedPing =
            static_cast<unsigned int>(*g_weaponDelayPingFactor) * m_player.get()->getPing();
        pingDelay = static_cast<float>(weightedPing) / 1000.0f;
    }

    float componentDelay = 0.0f;
    if (includeDelays)
    {
        if (m_ammoComp)
            componentDelay = m_ammoComp->getAcceptDelay(true);
        if (m_fireComp)
            componentDelay += m_fireComp->getAcceptDelay(true);
    }

    int delayTicks = static_cast<int>((pingDelay + componentDelay) * kTicksPerSecond);
    if (*g_weaponDelayTicks >= delayTicks)
        delayTicks = *g_weaponDelayTicks;

    unsigned int next = static_cast<unsigned int>(WorldPref::getTickCount())
                      + static_cast<unsigned int>(delayTicks);
    if (m_acceptStateSyncAtTick >= next)
        next = m_acceptStateSyncAtTick;
    m_acceptStateSyncAtTick = next;

    if (m_acceptStateSyncAtTick - static_cast<unsigned int>(WorldPref::getTickCount()) > 300u)
    {
        m_acceptStateSyncAtTick = static_cast<unsigned int>(*g_weaponDelayTicks)
                                + static_cast<unsigned int>(WorldPref::getTickCount());
    }
    return m_acceptStateSyncAtTick;
}

float GenericFireArm::getTimeToNotAllowChange() { return 1.0f; }       // fld1

int GenericFireArm::getFireAtTick() const
{
    return m_fireComp ? static_cast<int>(m_fireComp->getFireAtTick()) : 0;
}

bool GenericFireArm::isFiring() const
{
    return m_fireComp && static_cast<int>(m_fireComp->getFireState()) > 0;
}

bool GenericFireArm::isReadyToUseFire() { return !busy(false); }
bool GenericFireArm::getHasHeat() const  { return getHeat() >= 1.0f; }
bool GenericFireArm::getUsesHeat() const { return m_template->m_heatAddWhenFire > 0.0f; }
bool GenericFireArm::getShowHudIcon() const { return m_showHudIcon; }

io::PlayerInputMap GenericFireArm::getAltFireInput() const
{
    return m_fireComp ? m_fireComp->getAltFireInput() : io::PINone;
}

// / 0x8252B60 / 0x8252B70. The three ability slots all come off the ammo component -
// ReplenishingAmmoComp is the only implementor; every other ammo comp inherits IWeaponComp's
// permissive defaults, so these read 0/false/0.0 for a normal weapon without a null test firing.
void* GenericFireArm::getAbilityObjectList() const
{
    return m_ammoComp ? m_ammoComp->getAbilityObjectList() : 0;
}

bool GenericFireArm::getAbilityWorking() const
{
    return m_ammoComp ? m_ammoComp->getAbilityWorking() : false;
}

float GenericFireArm::getAbility() const
{
    return m_ammoComp ? m_ammoComp->getAbility() : 0.0f;
}

// . The null Fire arm logs and returns a freshly allocated identity matrix by
// const reference. The leak is retail behavior (the pointer is never retained or freed here).
const Mat4& GenericFireArm::getProjectileOrigin()
{
    if (m_fireComp)
        return m_fireComp->getProjectileOrigin();

    Debug(DEBUGTYPE_6, "Game/Objects/Weapons/GenericFireArm.cpp", 2458) << "Output dummy";
    return *new Mat4(Mat4::getIdentity());
}

bool GenericFireArm::shouldReload() const
{
    return m_ammoComp ? m_ammoComp->shouldReload() : false;
}

int GenericFireArm::getTotalAmmo() const
{
    const int mags = getCurrentNrOfMags();
    if (mags == -1)
        return -1;

    int total = 0;
    for (unsigned int i = 0; i < static_cast<unsigned int>(mags); ++i)
    {
        const int ammo = getAmmoInMag(i);
        if (ammo == -1)
            return -1;
        total += ammo;
    }
    return total;
}

float GenericFireArm::getReloadTimeLeftInPercent() const
{
    return m_ammoComp ? m_ammoComp->getReloadTimeLeftInPercent() : 0.0f;
}

float GenericFireArm::getTotalReloadTimeLeftInPercent() const
{
    return m_ammoComp ? m_ammoComp->getTotalReloadTimeLeftInPercent() : 0.0f;
}

int GenericFireArm::getAmmoType() { return 0; }

int GenericFireArm::getMag0Size()
{
    return m_ammoComp ? m_ammoComp->getMag0Size() : 0;
}

float GenericFireArm::getReloadTime() const
{
    return m_ammoComp ? m_ammoComp->getReloadTime() : 0.0f;
}

bool GenericFireArm::isReloading() const
{
    return m_ammoComp ? m_ammoComp->getIsReloading() : false;
}

// THE NULL default IS true, not false - a weapon with no ammo component behaves as if it
// auto-reloads, which keeps a bottomless turret out of the manual-reload paths.
bool GenericFireArm::getAutoReload() const
{
    return m_ammoComp ? m_ammoComp->getAutoReload() : true;
}

bool GenericFireArm::needsReload() const
{
    return m_ammoComp ? m_ammoComp->needsReload() : false;
}

float GenericFireArm::getDeviation()
{
    return m_deviationComp ? m_deviationComp->getDeviation() : 0.0f;
}

float GenericFireArm::getMinDeviation()
{
    return m_deviationComp ? m_deviationComp->getMinDeviation() : 0.0f;
}

float GenericFireArm::getMaxDeviation()
{
    return m_deviationComp ? m_deviationComp->getMaxDeviation() : 0.0f;
}

bool GenericFireArm::getHasRecoilForce()
{
    if (!m_recoilComp)
        return false;
    return static_cast<IWeaponRecoilComp*>(
        m_recoilComp->queryInterface(IID_IWeaponRecoilComp))->getHasRandomRecoilForce();
}

// These return references to 0x14-byte RandomDistribution objects, not floating-point scalars.
// Retail deliberately reports an error and leaks a fresh default distribution when no recoil
// component exists. Preserve that oddity exactly.
const RandomDistribution& GenericFireArm::getRecoilForceLeftRight()
{
    if (m_recoilComp)
        return static_cast<IWeaponRecoilComp*>(
            m_recoilComp->queryInterface(IID_IWeaponRecoilComp))
            ->getRandomRecoilForceLeftRight();

    Debug(DEBUGTYPE_6, "Game/Objects/Weapons/GenericFireArm.cpp", 2257)
        << "Just avoiding warning temporarily";
    return *new RandomDistribution();
}

const RandomDistribution& GenericFireArm::getRecoilForceUp()
{
    if (m_recoilComp)
        return static_cast<IWeaponRecoilComp*>(
            m_recoilComp->queryInterface(IID_IWeaponRecoilComp))
            ->getRandomRecoilForceUp();

    Debug(DEBUGTYPE_6, "Game/Objects/Weapons/GenericFireArm.cpp", 2267)
        << "Just avoiding warning temporarily";
    return *new RandomDistribution();
}

bool GenericFireArm::getGoBackOnRecoil()
{
    if (!m_recoilComp)
        return false;
    return static_cast<IWeaponRecoilComp*>(
        m_recoilComp->queryInterface(IID_IWeaponRecoilComp))->getGoBackOnRecoil();
}

float GenericFireArm::getDevMod() { return 1.0f; }

float GenericFireArm::getRecoilForceModifier() const
{
    return m_recoilComp ? m_recoilComp->getRecoilForceModifier() : 1.0f;
}

const std::vector<float>& GenericFireArm::getRecoilVector() const
{
    return m_recoilComp ? m_recoilComp->getRecoilVector() : g_emptyRecoilVector;
}

float GenericFireArm::getPitchRecoil()
{
    return m_recoilComp ? m_recoilComp->getPitchRecoil() : 0.0f;
}

float GenericFireArm::getYawRecoil()
{
    return m_recoilComp ? m_recoilComp->getYawRecoil() : 0.0f;
}

int GenericFireArm::getPrevCameraId() const
{
    return m_zoomComp ? m_zoomComp->getPrevCameraId() : 0;
}

int GenericFireArm::getChangeCameraId() const
{
    return m_zoomComp ? m_zoomComp->getChangeCameraId() : -1;
}

bool GenericFireArm::getNoisy() const
{
    return m_soundComp ? m_soundComp->getNoisy() : false;
}

IObject* GenericFireArm::getTargetObject() const
{
    return m_targetComp ? m_targetComp->getTargetObject() : 0;
}

bool GenericFireArm::hasTargetLock() const
{
    return m_targetComp ? m_targetComp->hasTargetLock() : false;
}

int GenericFireArm::getLastFiredProjectileId() const
{
    return m_fireComp ? m_fireComp->getLastFiredProjectileId() : -1;
}

int GenericFireArm::getTargetSystemType() const
{
    return m_targetComp ? m_targetComp->getTargetSystem() : 0;
}

float GenericFireArm::getMaxTargetDistance() const
{
    return m_targetComp ? m_targetComp->getMaxTargetDistance() : 0.0f;
}

// . Ordinary projectile templates take the zero arm.
// getComponentFromClassID (ELF +0x60 / PE +0x5C) finds the attached seek template;
// its radians-to-degrees max-angle accessor supplies the result.
float GenericFireArm::getMaxTargetAngle() const
{
    IObjectTemplate* projectile = getProjectileTemplate();
    if (!projectile)
        return 0.0f;

    SeekClosestTargetCompTemplate* seek = static_cast<SeekClosestTargetCompTemplate*>(
        projectile->getComponentFromClassID(CID_SeekClosestTargetCompTemplate));
    if (seek)
        return seek->getMaxAngleLock();
    return 0.0f;
}

bool GenericFireArm::isAnimationReadyToUse() { return true; }
bool GenericFireArm::getUseScope() { return false; }

IObject* GenericFireArm::getProjectile() const
{
    return m_targetComp ? m_targetComp->getProjectile() : 0;
}

void* GenericFireArm::getActiveWeaponGeometry() const
{
    if (m_activeDisplayObject)
        return object_detail::LayoutAccess::geometry(m_activeDisplayObject);
    return m_geometry;
}

bool GenericFireArm::hasMissileConnection() const
{
    return m_targetComp ? m_targetComp->hasMissileConnection() : false;
}

bool GenericFireArm::isWaitingForMissileConfirm() const
{
    return m_fireComp && m_fireComp->isWaitingForMissileConfirm();
}

bool GenericFireArm::getDisableOnFire() { return false; }

float GenericFireArm::getVelocity() const
{
    if (m_targetComp)
    {
        const float dynamicVelocity = m_targetComp->getDynamicVelocity();
        if (dynamicVelocity != -1.0f)
            return dynamicVelocity;
    }
    return m_template->m_velocity;
}

IObjectTemplate* GenericFireArm::getProjectileTemplate() const
{
    return const_cast<GenericFireArmTemplate*>(m_template)->getProjectileTemplate();
}

IObjectTemplate* GenericFireArm::getSecondaryProjectileTemplate() const
{
    return const_cast<GenericFireArmTemplate*>(m_template)->getSecondaryProjectileTemplate();
}

int GenericFireArm::getWeaponClass() const { return m_template->m_weaponClass; }

void GenericFireArm::detonateProjectiles() {}                            // empty
void GenericFireArm::addNetworkableParts() {}                            // empty
void GenericFireArm::removeNetworkableParts() {}                         // empty

void GenericFireArm::setHeat(float heat)
{
    m_heat = heat;
    m_float1A4 = heat;
    m_heatSetTick = WorldPref::getTickCount();
}
// Delegating setters below preserve the shipped null-component no-op and use the recovered typed
// component interfaces on their live arms.
// . A negative index IS clamped TO 0, as in getAmmoInMag, and an out-of-range one is
// silently dropped. getAmmo() IS called twice on the success path in the shipped body - once to
// bounds-check and once to store. Reproduced; a real double dispatch, not a transcription slip.
void GenericFireArm::setAmmoInMag(unsigned int mag, int rounds)
{
    int index = static_cast<int>(mag);
    if (index < 0)
        index = 0;

    if (!m_ammoComp)
        return;

    if (static_cast<unsigned int>(index) >= m_ammoComp->getAmmo()->size())
        return;

    (*m_ammoComp->getAmmo())[index] = rounds;

    if (m_animationComp)
        m_animationComp->setAmmoLeft(rounds);
}

void GenericFireArm::setFireRate(FireRate rate)
{
    if (m_fireComp)
        m_fireComp->setFireRate(rate);
}

void GenericFireArm::setZoom(int camera)
{
    if (m_zoomComp)
        m_zoomComp->setCameraNr(camera);
}

void GenericFireArm::setWeaponState(unsigned int state)
{
    if (m_fireComp)
        m_fireComp->setWeaponState(state);
}

// . Validate the magazine counts by both bound and sign class, then stage all six
// words. A missing Fire component accepts the state; a present one first validates the fire rate.
void GenericFireArm::setStateSync(const WeaponState& state)
{
    const int maxMagsForBound = getMaxNrOfMags();
    if (state.currentNrOfMags > maxMagsForBound)
        return;
    const int maxMagsForSign = getMaxNrOfMags();
    if (!((state.currentNrOfMags < 0 && maxMagsForSign < 0)
          || (state.currentNrOfMags > 0 && maxMagsForSign > 0)
          || state.currentNrOfMags == 0 || maxMagsForSign == 0))
        return;

    const int magSizeForBound = getMagSize();
    if (state.ammoInMag > magSizeForBound)
        return;
    const int magSizeForSign = getMagSize();
    if (!((state.ammoInMag < 0 && magSizeForSign < 0)
          || (state.ammoInMag > 0 && magSizeForSign > 0)
          || state.ammoInMag == 0 || magSizeForSign == 0))
        return;

    if (m_fireComp && !m_fireComp->isFireRateValid(state.fireRate))
        return;

    m_syncedAmmoInMag = state.ammoInMag;
    m_syncedFireRate = state.fireRate;
    m_syncedCurrentNrOfMags = state.currentNrOfMags;
    m_syncedCamera = state.camera;
    m_syncedFlags = state.flags;
    m_byte1D4 = true;
    m_syncedHeat = state.heat;
    setHeat(m_syncedHeat);
    setAsyncFlags(m_syncedFlags);
}

// returns the refillAbsolute leftover, or the input amount with no component.
// SupplyDepot (ELF 0x81B9D00 / PE 0x5721E0) consumes EAX as unsigned when subtracting stock;
// that caller closes the formerly guessed void return without changing the virtual slot order.
unsigned int GenericFireArm::reloadAmmo(unsigned int amount, bool testOnly)
{
    if (m_ammoComp)
        return m_ammoComp->refillAbsolute(amount, testOnly);
    return amount;
}

// . IT resizes THE magazine vector directly, filling new elements with magSize and
// truncating from the end - the network path forcing a client magazine count to the server value.
// THE sign-agreement guard is the interesting part: the requested count and getMaxNrOfMags() must
// agree in sign (or one of them be zero), so a template with infinite magazines (-1) accepts only
// -1 or 0, and a finite one accepts only non-negative counts. A mismatch is dropped silently.
// THE count <= 0 ARM keeps exactly ONE magazine, not zero - the loaded one is an element.
void GenericFireArm::setCurrentNrOfMags(int count)
{
    if (!m_ammoComp)
        return;

    const int maxMags = getMaxNrOfMags();
    if (count > maxMags)
        return;

    const bool signsAgree = (count < 0 && maxMags < 0) ||
                            (count > 0 && maxMags > 0) ||
                            count == 0 || maxMags == 0;
    if (!signsAgree)
        return;

    std::vector<int>* const ammo = m_ammoComp->getAmmo();
    const int fill = m_ammoComp->getMagSize();
    const int size = static_cast<int>(ammo->size());

    if (count <= 0)
    {
        if (size <= 1)
            ammo->insert(ammo->end(), static_cast<std::size_t>(1 - size), fill);
        else
            ammo->erase(ammo->begin() + 1, ammo->end());
    }
    else
    {
        if (count >= size)
            ammo->insert(ammo->end(), static_cast<std::size_t>(count - size), fill);
        else
            ammo->erase(ammo->begin() + count, ammo->end());
    }
}

void GenericFireArm::setAIDeviation(float deviation)
{
    if (m_deviationComp)
        m_deviationComp->setAIDeviation(deviation);
}

Vec3 GenericFireArm::deviatePosition(Vec3& position, int projectileIndex)
{
    if (m_deviationComp)
        return m_deviationComp->deviatePosition(position, projectileIndex);

    return Vec3::getZero();
}

void GenericFireArm::setProjectileDeviationFixed(Vec3& direction, int projectileIndex)
{
    if (m_deviationComp)
        m_deviationComp->setProjectileDeviationFixed(direction, projectileIndex);
}

// / 0x8252850. Flags 0 and 1 go to Fire; the asynchronous lock flag goes to Target.
void GenericFireArm::setSyncedFlags(unsigned int flags)
{
    if (m_fireComp)
    {
        m_fireComp->setFireStartDelay((flags & 1u) != 0);
        m_fireComp->setWeaponState((flags & 2u) >> 1);
    }
}

void GenericFireArm::setAsyncFlags(unsigned int flags)
{
    if (m_targetComp)
        m_targetComp->setWeaponState((flags & 2u) >> 1);
}

void GenericFireArm::setFlags(unsigned int flags)
{
    setSyncedFlags(flags);
    setAsyncFlags(flags);
}

void GenericFireArm::setTargetDirection(Vec3 from, Vec3 to)
{
    if (m_targetComp)
        m_targetComp->setTargetDirection(from, to);
}

void GenericFireArm::addProjectileReference(IObject* projectile)
{
    if (m_fireComp)
        m_fireComp->addProjectileReference(projectile);
}

void GenericFireArm::releaseProjectile(IObject* projectile)
{
    if (m_fireComp)
        m_fireComp->releaseProjectile(projectile);
}
// . * THE -1 THE constructor plants IS A sentinel, NOT A value. * Until
// PlayerControlObject::addWeapon assigns a real slot index, this falls back to the template's item
// index - which is why the ctor seeds -1 rather than 0.
int GenericFireArm::getNetworkSafeItemIndex() const
{
    if (m_networkSafeItemIndex != -1)
        return m_networkSafeItemIndex;
    return m_template->m_itemIndex;
}

// . PlayerControlObject::addWeapon is the writer - it stores the weapon's index
// within m_weapons at the moment it is added.
void GenericFireArm::setNetworkSafeItemIndex(int index) { m_networkSafeItemIndex = index; }
bool GenericFireArm::launchesProjectile(int magazine) const
{
    return m_fireComp && m_fireComp->launchesProjectiles(magazine);
}

float GenericFireArm::getZoomFactor(unsigned int index) const
{
    return m_zoomComp ? m_zoomComp->getZoomFactor(index) : -1.0f;
}

// / 0x8250E10. A missing Zoom component makes both operations empty. The typed Zoom
// compare/change ordering and Sound notification tail are recovered.
void GenericFireArm::toggleCamera(TemplateMessage message)
{
    if (!m_zoomComp)
        return;

    const int previous = m_zoomComp->getCameraNr();
    m_zoomComp->toggle(message);
    if (previous != m_zoomComp->getCameraNr() && m_soundComp)
        m_soundComp->toggleCamera();
}

void GenericFireArm::setCamera(int camera)
{
    if (!m_zoomComp || camera == m_zoomComp->getCameraNr())
        return;

    m_zoomComp->setCameraNr(camera);
    if (m_soundComp)
        m_soundComp->toggleCamera();
}

// . Target receives the authoritative projectile first, Fire second.
void GenericFireArm::setCurrentProjectile(IObject* projectile)
{
    if (m_targetComp)
        m_targetComp->setCurrentProjectile(projectile);
    if (m_fireComp)
        m_fireComp->setCurrentProjectile(projectile);
}

int GenericFireArm::getFireState()
{
    return m_fireComp ? static_cast<int>(m_fireComp->getFireState()) : 0;
}

int GenericFireArm::getReloadState()
{
    if (m_ammoComp)
        return static_cast<int>(m_ammoComp->getReloadState());
    return 0;
}

int GenericFireArm::getBurstSize() const
{
    return m_fireComp ? m_fireComp->getBurstSize() : 0;
}

void GenericFireArm::setTargetObject(IObject* target)
{
    if (m_targetComp)
        m_targetComp->setTargetObject(target, true, getRootParent(target));
}

// . Apply the staged values in retail order, then lower the validity gate. All
// componentless setter arms are genuine no-ops, so this also completes today's sync-consumption
// path instead of leaving m_byte1D4 latched forever.
void GenericFireArm::processStateSyncedData()
{
    setCurrentNrOfMags(m_syncedCurrentNrOfMags);
    setAmmoInMag(0, m_syncedAmmoInMag);
    if (m_fireComp)
        m_fireComp->setFireRate(m_syncedFireRate);
    setSyncedFlags(m_syncedFlags);
    setCamera(m_syncedCamera);
    m_byte1D4 = false;
}

bool GenericFireArm::ownsProjectile(IObject* projectile) const
{
    return m_fireComp && m_fireComp->ownsProjectile(projectile);
}

} // namespace world
} // namespace hfe
} // namespace dice
