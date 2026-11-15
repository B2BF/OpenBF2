// Weapon object composed from fire, ammo, zoom, sound, recoil, deviation, animation and target components.
// Preserve base order: AnimatedBundle, IItemObject, IWeaponObject, IKitPart.
// Delegating getters retain nonzero missing-component defaults: fire input 0x40, mouse speed 1, water support true,
// and ammo -1 (versus 0 for an invalid magazine index). Construction precaches the template when requested.
// Header placement follows the attested GenericFireArm.cpp path.

#ifndef GAME_OBJECTS_WEAPONS_GENERICFIREARM_H
#define GAME_OBJECTS_WEAPONS_GENERICFIREARM_H

#include "../Game/AnimatedBundle.h"
#include "../Game/PlayerControlObject.h"      // dice::hfe::FireButtonState - the real 4-byte latch
#include "../IItemObject.h"
#include "../IWeaponObject.h"
#include "../IKitPart.h"
#include "../../../Core/WeakPtr.h"

#include <list>
#include <string>
#include <vector>

namespace dice {
namespace hfe {
class ArtilleryEvent;
namespace world {

class GenericFireArmTemplate;
class FireComp;
class ThrownFireComp;
class IWeaponComp;
class IWeaponAmmoComp;
class IWeaponAnimationComp;
class IWeaponDeviationComp;
class IWeaponFireComp;
class IWeaponRecoilComp;
class IWeaponSoundComp;
class IWeaponTargetComp;
class IWeaponZoomComp;
class RotationalBundle;
class ArtilleryHandler;
class Soldier;
class ZiplineTemplate;
enum FireState : int;

// The type name and 32-bit representation are attested by the
// setAlternateDisplayObject mangling.  Its three observed values are 0 (the explosives runtime),
// 1 (initial component assembly), and 2 (weapon shutdown); no enumerator names survive.
enum AlternateDisplayMode : int;

class GenericFireArm : public AnimatedBundle,
                       public IItemObject,
                       public IWeaponObject,
                       public IKitPart
{
    // FireComp's recovered projectile setup reads the firearm's random seed directly.  There is no
    // retail accessor; preserve the source-level friendship instead of inventing one.
    friend class FireComp;
    friend class ThrownFireComp;
    // AmmoComp::fireComplete writes m_pendingActiveWeapon directly (ELF 0x8258060) when a
    // spent weapon has to fall back to the soldier's previous one. Same situation, same
    // remedy: there is no retail setter to call.
    friend class AmmoComp;
    // Soldier::handleFrameUpdate writes the prior weapon here directly when a gas-mask/NVG deploy
    // animation completes. Retail has no setter for this write.
    friend class Soldier;
    friend class ArtilleryHandler;                   // server strike stamps/fire-arms the gun
    friend class ::dice::hfe::ArtilleryEvent;        // replicated strike mirrors the same sequence

public:
    explicit GenericFireArm(const GenericFireArmTemplate* objectTemplate);
    virtual ~GenericFireArm();

    // IBase. ONE set of overrides serves all four faces; the shipped tables reach them through
    // _ZThn316_ / _ZThn340_ / _ZThn344_ / _ZThn348_ thunk groups.
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;   // the flat chain

    // Object / AnimatedBundle lifecycle overrides
    virtual void  init();
    virtual void  disableCachedObject();
    virtual void  enableCachedObject();
    virtual void  destroy(bool force);
    virtual void  updateGeometry();
    virtual void  handleUpdate(float, unsigned int);
    virtual void  handleMessage(TemplateMessage, IPlayer*, bool);
    virtual void  setActiveAnimationSystem(int);
    virtual void* getActiveAnimationSystem();

    // empty as read. A firearm replicates through its soldier, not on its own,
    // so it deliberately installs no networkable component. This override previously sat on
    // GenericFireArmTemplate.cpp's FireArmStandIn; it belongs here. (KitStandIn precedent.)
    virtual void  initNetworkableComp();

    // IItemObject (+0x154)
    virtual void  handlePickup(IPlayer* player, IObject* container);
    virtual void  handleDrop(IPlayer* player, IObject* container);
    virtual void  setOwnerId(int ownerId);                             // empty
    virtual int   getOwnerId();
    virtual void  setItemType(int type);
    virtual int   getItemType() const;
    virtual bool  enableItem();
    virtual void  disableItem();
    // getItemIndex / isReadyToUse / getRank are shared with the other faces - declared below.

    // IKitPart (+0x15C)
    virtual int   getKitPartIndex();
    // getRank is IItemObject +0x38 AND IKitPart +0x10 - ONE 7-byte body (0x8256760) in both tables.
    virtual int   getRank() const;                                     // `return 0`

    // IWeaponObject (+0x158). Declaration order IS the shipped vtable order.
    virtual float getTimeToNotAllowChange();                         // 1.0f
    virtual FireRate getFireRate();
    virtual int   getFireAtTick() const;
    virtual bool  isFiring() const;
    virtual bool  isReadyToUseFire();
    virtual float getHeat() const;
    virtual void  setHeat(float);
    virtual bool  getHasHeat() const;
    virtual bool  getUsesHeat() const;
    virtual bool  getShowHudIcon() const;
    virtual io::PlayerInputMap getFireInput() const;
    virtual io::PlayerInputMap getAltFireInput() const;
    virtual bool  launchesProjectile(int) const;
    virtual void* getAbilityObjectList() const;
    virtual bool  getAbilityWorking() const;
    virtual float getAbility() const;
    virtual void  setAmmoInMag(unsigned int, int);
    virtual void  setFireRate(FireRate);
    virtual void  setZoom(int);
    virtual void  setWeaponState(unsigned int);
    virtual void  setStateSync(const WeaponState&);
    virtual IPlayer* getPlayer();
    virtual void  setPlayer(IPlayer*);
    virtual const Mat4& getProjectileOrigin();
    virtual void  detonateProjectiles();
    virtual bool  canBeRefilled();
    virtual bool  shouldReload() const;
    virtual unsigned int reloadAmmo(unsigned int, bool);
    virtual float reloadAmmoPercent(float, bool);
    virtual int   getTotalAmmo() const;
    virtual bool  isReadyToUse();
    virtual float getReloadTimeLeftInPercent() const;
    virtual float getTotalReloadTimeLeftInPercent() const;
    virtual int   getAmmoInMag(unsigned int) const;
    virtual int   getMaxNrOfMags() const;
    virtual void  setCurrentNrOfMags(int);
    virtual int   getCurrentNrOfMags() const;
    virtual int   getAmmoType();
    virtual int   getMagSize();
    virtual int   getMag0Size();
    virtual float getReloadTime() const;
    virtual bool  isReloading() const;
    virtual bool  getAutoReload() const;
    virtual bool  needsReload() const;
    virtual void  updateDeviation(io::PlayerInput, bool);
    virtual float getDeviation();
    virtual float getMinDeviation();
    virtual float getMaxDeviation();
    virtual void  setAIDeviation(float);
    virtual Vec3  deviatePosition(Vec3&, int);
    virtual void  setProjectileDeviationFixed(Vec3&, int);
    virtual bool  getHasRecoilForce();
    virtual const RandomDistribution& getRecoilForceLeftRight();
    virtual const RandomDistribution& getRecoilForceUp();
    virtual bool  getGoBackOnRecoil();
    virtual float getDevMod();
    virtual float getRecoilForceModifier() const;
    virtual const std::vector<float>& getRecoilVector() const;
    virtual float getPitchRecoil();
    virtual float getYawRecoil();
    virtual int   getPrevCameraId() const;
    virtual int   getChangeCameraId() const;
    virtual int   getZoom();
    virtual float getMouseSpeedMod();
    virtual float getZoomFactor(unsigned int) const;
    virtual bool  getNoisy() const;
    virtual void  setFlags(unsigned int);
    virtual int   getFlags() const;
    virtual IObject* getTargetObject() const;
    virtual bool  hasTargetLock() const;
    virtual int   getLastFiredProjectileId() const;
    virtual void  setTargetDirection(Vec3, Vec3);
    virtual int   getTargetSystemType() const;
    virtual float getMaxTargetDistance() const;
    virtual float getMaxTargetAngle() const;
    virtual bool  isAnimationReadyToUse();
    virtual bool  getUseScope();
    virtual void  projectileWasReset(IObject*);
    virtual void  addProjectileReference(IObject*);
    virtual bool  isSelectable() const;
    virtual bool  isUnselectable() const;
    virtual bool  getWorksInWater() const;
    virtual int   getItemIndex();
    virtual IObject* getProjectile() const;
    virtual void* getActiveWeaponGeometry() const;
    virtual int   getWeaponState() const;
    virtual bool  hasMissileConnection() const;
    virtual bool  isWaitingForMissileConfirm() const;
    virtual bool  ownsProjectile(IObject*) const;
    virtual void  releaseProjectile(IObject*);
    virtual int   getNetworkSafeItemIndex() const;
    virtual void  setNetworkSafeItemIndex(int);
    virtual void  enableWeapon();
    virtual void  disableWeapon();
    virtual bool  getDisableOnFire();
    virtual float getVelocity() const;
    virtual IObjectTemplate* getProjectileTemplate() const;
    virtual void  addNetworkableParts();
    virtual void  removeNetworkableParts();

    // non-virtual helpers reached by the bodies above
    bool  busy(bool exceptSelf);              // called by isReadyToUse (0x82566F0)
    bool  is1P();                             // called by enableWeapon
    void  coolWeapon(float seconds);          // called by enableWeapon's back-dated cooling
    void  setBlockRotBundleInput(bool);       // called by enableWeapon
    void  setAlternateDisplayObject(IObject*, AlternateDisplayMode);
    bool  busyExcept(IWeaponComp*, bool);
    unsigned int updateAcceptStateSyncAtTick(bool);
    void  internalHandleMessage(TemplateMessage, IPlayer*);
    bool  requestFire(bool, bool);
    void  doFire(FireState, bool);
    bool  checkAreaToFireZipline(Soldier*, const ZiplineTemplate*);
    void  postFire(FireState, int, int, int, std::vector<IObject*>*,
                   Vec3&, Vec3&);
    int   getMaxProjectilesToFire(int);
    bool  requestReload(bool);
    void  doReload();
    void  fireStop(io::PlayerInputMap);

    // Exact non-interface helpers used by the firearm's own primary surface.
    IPlayer* getPlayerByForce() const;
    int getWeaponClass() const;
    IObjectTemplate* getSecondaryProjectileTemplate() const;

    void toggleCamera(TemplateMessage);
    void setCamera(int);
    void setCurrentProjectile(IObject*);
    int getFireState();
    int getReloadState();
    int getBurstSize() const;
    void setTargetObject(IObject*);
    void processStateSyncedData();

    void setSyncedFlags(unsigned int);
    void setAsyncFlags(unsigned int);

    void initComponents();
    void resetFirearm();
    void setLocalPredictedProjectile(IObject*);
    IObject* getLocalPredictedProjectile();


private:
    // fields - every offset below is written by the recovered constructor at 0x824EEF0 and is
    // listed in ctor-store order where that is informative. Offsets are the ELF's; this rebuild
    // reaches them by name, so they are documentation, not a layout contract.
    /* +0x160 */ int   m_refCount;                 // ctor: 1
    /* +0x164 */ bool  m_byte164;                  // ctor: 0   [UNRESOLVED]
    /* +0x168 */ RotationalBundle* m_blockInputRotBundle; // cached name match; non-owning

    // the eleven component interfaces. The non-HUD weapon component families are typed and
    // their recovered concrete templates/runtimes are live. WeaponHud remains separately
    //      factory-gated under RW-6. [RETIRE-WHEN: unrecovered-factories-registered]
    /* +0x16C */ IWeaponFireComp* m_fireComp;       // adjusted secondary face returned by queryInterface
    /* +0x170 */ int    m_dword170;
    /* +0x174 */ IWeaponZoomComp* m_zoomComp;       // typed: PE/ELF zoom tails differ after dtor
    /* +0x178 */ IWeaponSoundComp* m_soundComp;
    /* +0x17C */ IWeaponAmmoComp* m_ammoComp;
    /* +0x180 */ IWeaponRecoilComp* m_recoilComp;
    /* +0x184 */ IWeaponTargetComp* m_targetComp;   // getFlags ORs its +0x90 in
    /* +0x188 */ IWeaponDeviationComp* m_deviationComp;
    /* +0x18C */ IWeaponAnimationComp* m_animationComp;
    /* +0x190 */ IObject* m_alternateDisplayObject;
    /* +0x194 */ IObject* m_activeDisplayObject;
    /* +0x198 */ IWeaponSoundComp* m_defaultSoundComp;
    /* +0x19C */ IWeaponAnimationComp* m_defaultAnimationComp;

    /* +0x1A0 */ float m_heat;                     // getHeat returns this unless the template
                                                   // inherits heat from the primary weapon
    /* +0x1A4 */ float m_float1A4;
    /* +0x1A8 */ int   m_heatSetTick;
    /* +0x1AC */ int   m_itemType;                 // ctor: copied from template +0x1C4
    /* +0x1B0 */ int   m_networkSafeItemIndex;     // ctor: -1  (NOT the same as the item index)
    /* +0x1B4 */ bool  m_itemEnabled;              // enableItem/disableItem/enableWeapon latch
    /* +0x1B8 */ int   m_deployDoneTick;           // enableWeapon: now + template->m_delayToUseTimes30
    /* +0x1BC */ int   m_lastCoolTick;             // enableWeapon back-dates cooling from this
    /* +0x1C0 */ bool  m_showHudIcon;
    /* +0x1C4 */ int   m_dword1C4;
    /* +0x1C8 */ float m_float1C8;                 // ctor 10.0f; enableWeapon re-arms it to 10.0f
    /* +0x1CC */ WeakPtr<IPlayer> m_player;        // IPlayer::getWeakPtr() (+0x1C) feeds this
    /* +0x1D0 */ const GenericFireArmTemplate* m_template;
    /* +0x1D4 */ bool  m_byte1D4;                  // enableWeapon clears it
    /* +0x1D8 */ unsigned int m_acceptStateSyncAtTick;
    // setStateSync's six-word pending state. The retail constructor deliberately does not
    // initialise these; m_byte1D4 is the validity gate that keeps them unread until populated.
    /* +0x1DC */ int          m_syncedAmmoInMag;
    /* +0x1E0 */ FireRate     m_syncedFireRate;
    /* +0x1E4 */ int          m_syncedCurrentNrOfMags;
    /* +0x1E8 */ int          m_syncedCamera;
    /* +0x1EC */ unsigned int m_syncedFlags;
    /* +0x1F0 */ float        m_syncedHeat;
    /* +0x1F4 */ IObject* m_pendingActiveWeapon;   // one-shot Soldier active-weapon reconciliation
    /* +0x1F8 */ FireButtonState m_fireButtons[3]; // ctor default-constructs exactly three
    // The destructor names this type outright: it calls
    // std::_List_base<dice::hfe::WeakPtr<dice::hfe::world::IObject> >::__clear()
    // so the projectile references are weak, not owning - a projectile may die independently.
    /* +0x204 */ std::list<WeakPtr<IObject> > m_projectiles;
    /* +0x208 */ bool  m_byte208;
    /* +0x20C */ int   m_enableTick;               // enableItem stamps WorldPref::getTickCount()
    /* +0x210 */ bool  m_byte210;                  // zipline placement HUD latch; doFire raises,
                                                   // handleUpdate clears after alt-fire ends
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_WEAPONS_GENERICFIREARM_H
