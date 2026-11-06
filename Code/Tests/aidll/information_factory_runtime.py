"""[TEMP] InformationFactory, courier and recovered cover/physical/armament/weapon/unit outcomes.

The production TUs are linked whole, with the real InformationManager / WeaponInfoManager pools and
the real SCurve. Typed observers stand in for the engine object, its physical plug-in, the game
timer and the aerial height map; every slot the fixture does not choose aborts by name. The named
stops that remain in these TUs are driven one per mode and must report exactly and abort.
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import subprocess
import sys

CODE = Path(__file__).resolve().parents[2]
AI = 'AI/AIDLL/AI/'
BFK = 'AI/AIInterface/BFKnowledge/'
_spec = importlib.util.spec_from_file_location('information_fixture', Path(__file__).with_name('information_frontiers_runtime.py'))
_info = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_info)

OBJECT_CUSTOM = {
    'const Vec3& getPosition() const',
    'const Mat4& getTransformation() const',
    'int getSide() const',
    'IAIObjectPhysical* getPhysical() const',
    'bool isPrimary() const',
    'IAIObject* getSecondaryObject(int& iterator)',
    'IAIObject* getFirstSecondaryObject(int& iterator)',
    'unsigned int getInformation(int side)',
    'IAIObjectArmament* getArmament() const',
    'IAIObjectCover* getCover() const',
    'IAIObjectUnit* getUnit() const',
    'bool isCover() const',
}
PHYSICAL_CUSTOM = {
    'float getHealth()', 'float getMaxHealth() const', 'float getMass() const', 'float getVolume() const',
    'float getArmour(const Vec3& direction) const', 'float getBestArmour() const',
    'const LocalBoundingBox* getLocalBoundingBox() const', 'BoundingBox* getCurrentBoundingBox()',
    'float getRadius() const', 'Vec3 getSphereLocalOffset() const', 'float getSmallestRadius() const',
    'float getSubSpheres2d(std::vector<Vec2>& out) const', 'int getStrType() const',
    'bool isTouchingTerrain(Vec3& out) const', 'bool isInAir() const', 'bool isInWater() const',
    'int getMaterial() const', 'float getManDownTimeLeft() const', 'float getMaxManDownTime() const',
    'bool getManDown() const',
}
TIMER_CUSTOM = {'double getGameTime() const'}
COVER_CUSTOM = {'IPlanePolygon* getObjectOutline()'}
WEAPON_CUSTOM = {
    'const Mat4& getAbsoluteTransformation() const', 'const Vec3& getPosition() const',
    'const Mat4& getProjectileOrigin()', 'int getWeaponActivate() const',
    'int getWeaponFire() const', 'int getWeaponZoom() const', 'int getWeaponAltFire() const',
    'bool isZoomed() const', 'int getWeaponState() const', 'float getDrag() const',
    'float getGravityModifier() const', 'float getExitVelocity() const', 'float getMinRange() const',
    'float getMaxRange() const', 'float getOptimalRangePercentage() const',
    'SoldierPose getFiringPose() const', 'float getAllowedDeviation() const',
    'bool getFiresThroughTransparent() const', 'const int* getStrengths() const',
    'int getStrength(int index) const', 'float getExplosionRadius() const',
    'bool getIsThrown() const', 'bool useAimerOnly() const', 'int getFireRate() const',
    'int getAmmo() const', 'int getCurrentAmmo() const', 'int getAmmoInMag(int mag) const',
    'int getMaxAmmo() const', 'int getMaxAmmoInMag() const', 'int getNumberOfMags() const',
    'bool hasAutoReload() const', 'float getReloadTime() const', 'bool refillAmmo(float fraction)',
    'int getWeaponType() const', 'int getWeaponGuidingSystem() const',
    'void setTimeOfFiring(double time)', 'double getTimeOfFiring() const', 'bool getBurst() const',
    'bool getIndirect() const', 'float getHeat() const', 'bool hasHeat() const',
    'bool usesHeat() const', 'bool isReadyToFire() const', 'bool setFakeFire(bool fake)',
    'bool getFakeFire() const', 'float getSoundSphereRadius() const',
    'int getProjectileMaterial() const', 'float getReplenishRadius() const',
    'void setBotSkill(float skill, double since, float scale)',
    'world::IObject* getProjectile() const', 'bool setTarget(unsigned int targetId)',
    'bool hasTargetLocked() const', 'IAIObject* getLockedTarget() const',
    'const std::string& getName() const',
}
ARMAMENT_CUSTOM = {
    'int isAntiAircraft() const', 'int getActiveWeapon() const',
    'void setActiveWeapon(unsigned int index, int side)', 'IWeapon* getWeapon(unsigned int index) const',
    'void setLastFiredWeapon()', 'IWeapon* getLastFiredWeapon() const',
    'const std::vector<IWeapon*>& getWeapons() const',
    'int getWeaponIdFromActivationId(int activation, int fire) const',
    'bool rearm(IWeapon* weapon, float amount, int side)',
    'bool getProjectilePosition(Vec3& out) const', 'bool getProjectileSpeed(Vec3& out) const',
}
UNIT_CUSTOM = {
    'int getAmmo() const', 'int getMaxAmmo() const',
    'const int* getBattleStrengths() const', 'const int* getStrategicStrengths() const',
    'int getBattleStrength(int index) const', 'int getStrategicStrength(int index) const',
    'void setBattleStrength(int index, int value)', 'void setStrategicStrength(int index, int value)',
    'int getEquipmentType() const', 'bool useNoPathfindingToGetToObject() const',
    'bool hasExposedSoldier() const', 'bool getAlwaysTargetSoldier() const', 'bool isOccupied() const',
    'float getCullDistance() const', 'float getCullDistanceSqr() const', 'float getEntryRadius() const',
    'IAIObject* getOccupier() const', 'IAIObject* getOccupiedObject() const',
    'double getLastPlayerPossesionTime() const', 'void setLastPlayerPossesionTime(double time)',
    'int getTeamLock() const', 'bool mustBorrowArmament(IAIObject* vehicle) const',
    'int getOccupyingPlayerId() const', 'io::PlayerInputMap getSelectKey() const',
    'void setMissileLockStatus(int status)', 'int getMissileLockStatus() const',
    'const Vec3& getEntryPositionOfNonMobile() const',
}
SETTINGS_CUSTOM = {'int getNBattleStrengths()', 'int getNStrategicStrengths()'}
SOURCES = [AI + 'AIKnowledge/' + name for name in (
    'InformationFactory.cpp', 'InformationFactory.h', 'BFInformationCourier.cpp', 'BFInformationCourier.h',
    'InformationReal.cpp', 'InformationReal.h', 'IPIPhysicalReal.cpp', 'IPIPhysicalReal.h',
    'Information.cpp', 'IPICover.cpp', 'IPICover.h', 'IPIMobile.cpp', 'IPIMobile.h',
    'IPIMobileReal.cpp', 'IPIMobileReal.h', 'IPIMobileKnown.cpp', 'IPIMobileKnown.h',
    'IPIUnit.cpp', 'IPIUnit.h',
    'IPIControlInfo.h', 'IPIArmament.cpp', 'IPIArmament.h',
    'IPIArmamentReal.cpp', 'IPIArmamentReal.h', 'WeaponInfo.cpp', 'WeaponInfo.h',
    'WeaponInfoReal.cpp', 'WeaponInfoReal.h')]
SOURCES += [BFK + 'IWeapon.h', BFK + 'IAIObject.h', BFK + 'IAIObjectPlugIn.h',
            BFK + 'PlanePolygon.cpp', BFK + 'PlanePolygon.h',
            'Math/Vec3.cpp', 'Math/Vec3.h', AI + 'Main/AISettingsInterfaces.h',
            AI + 'Main/IAIPathfinding.cpp', AI + 'Main/IAIPathfindingSetup.cpp', AI + 'Main/IAIPathfinding.h']


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    evidence = []
    for relative in SOURCES:
        path = CODE / 'BF2' / relative
        text = path.read_text(encoding='utf-8')
        evidence.append({'source': path.as_posix(), 'source_sha256': hashlib.sha256(text.encode()).hexdigest()})
    (out / 'source.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    lines = ['// [TEMP] Generated typed observers; only the declared-custom slots are defined by hand.',
             'namespace dice { namespace hfe { namespace ai {']
    object_parts = [('IBase', 'Core/IBase.h'), ('IObjectComponent', 'Game/Objects/IObjectComponent.h'),
                    ('IAIObject', BFK + 'IAIObject.h')]
    _info.observer(lines, 'EngineObject', 'IAIObject', object_parts, OBJECT_CUSTOM,
                   ['    explicit EngineObject(const char* id);',
                    '    std::string m_id; Vec3 m_position; Mat4 m_transformation; int m_side; bool m_primary;',
                    '    IAIObjectPhysical* m_physical; IAIObjectArmament* m_armament;',
                    '    IAIObjectCover* m_cover; IAIObjectUnit* m_unit;',
                    '    std::vector<EngineObject*> m_secondaries;',
                    '    unsigned int m_handles[3];'])
    _info.observer(lines, 'EnginePhysical', 'IAIObjectPhysical',
                   [('IAIObjectPlugIn', BFK + 'IAIObjectPlugIn.h'), ('IAIObjectPhysical', BFK + 'IAIObjectPlugIn.h')],
                   PHYSICAL_CUSTOM,
                   ['    explicit EnginePhysical(const char* id) : IAIObjectPhysical(nullptr), m_id(id), m_health(50), m_maxHealth(200) {}',
                    '    std::string m_id; float m_health, m_maxHealth;'])
    _info.observer(lines, 'EngineCover', 'IAIObjectCover',
                   [('IAIObjectPlugIn', BFK + 'IAIObjectPlugIn.h'),
                    ('IAIObjectCover', BFK + 'IAIObjectPlugIn.h')], COVER_CUSTOM,
                   ['    EngineCover(IAIObject* owner, const char* id);',
                    '    std::string m_id; IPlanePolygon* m_lastOutline; unsigned int m_created;'])
    _info.observer(lines, 'TimerObserver', 'IBFTimer', [('IBFTimer', BFK + 'BFTimer.h')], TIMER_CUSTOM,
                   ['    double m_now = 0;'])
    _info.observer(lines, 'WeaponObserver', 'IWeapon',
                   [('IBase', 'Core/IBase.h'), ('IObjectComponent', 'Game/Objects/IObjectComponent.h'),
                    ('IWeapon', BFK + 'IWeapon.h')], WEAPON_CUSTOM,
                   ['    explicit WeaponObserver(const char* id);',
                    '    std::string m_id, m_name; Vec3 m_position; Mat4 m_absolute, m_projectileOrigin;',
                    '    int m_strengths[3]; IAIObject* m_lockedTarget;',
                    '    bool m_refillResult, m_targetResult; float m_lastRefill;',
                    '    double m_setFiringTime; unsigned int m_targetId; int m_ammo;'])
    _info.observer(lines, 'EngineArmament', 'IAIObjectArmament',
                   [('IAIObjectPlugIn', BFK + 'IAIObjectPlugIn.h'),
                    ('IAIObjectArmament', BFK + 'IAIObjectPlugIn.h')], ARMAMENT_CUSTOM,
                   ['    explicit EngineArmament(IAIObject* owner);',
                    '    std::vector<IWeapon*> m_weapons; IWeapon* m_lastFired;',
                    '    bool m_rearmResult; float m_lastRearmAmount; IWeapon* m_lastRearmWeapon;',
                    '    int m_lastRearmSide;'])
    _info.observer(lines, 'EngineUnit', 'IAIObjectUnit',
                   [('IAIObjectPlugIn', BFK + 'IAIObjectPlugIn.h'),
                    ('IAIObjectUnit', BFK + 'IAIObjectPlugIn.h')], UNIT_CUSTOM,
                   ['    explicit EngineUnit(IAIObject* owner);',
                    '    std::string m_id; int m_battle[6]; int m_strategic[2]; int m_maxAmmo;',
                    '    IAIObject* m_occupier; IAIObject* m_occupiedObject; bool m_occupied;',
                    '    int m_teamLock; int m_missileLock; bool m_borrow; Vec3 m_entryPosition;',
                    '    double m_possessionTime;'])
    _info.observer(lines, 'SettingsObserver', 'IAISettings',
                   [('IAISettings', AI + 'Main/AISettingsInterfaces.h')], SETTINGS_CUSTOM,
                   ['    SettingsObserver() : m_battleCount(3), m_strategicCount(2) {}',
                    '    int m_battleCount, m_strategicCount;'])
    # A base-sort record (getInformationSort stays the base's 3) with a traced destructor.
    _info.observer(lines, 'TracedInformation', 'Information', [('Information', AI + 'AIKnowledge/Information.h')],
                   set(), ['    TracedInformation(IAIObject* object, const char* id) : Information(object, 0.0), m_id(id) {}',
                           '    ~TracedInformation();', '    std::string m_id;',
                           '    IPIArmament* armament() const { return m_armament; }'], pure_only=True)
    lines.append('} } }')
    lines.append('namespace dice { namespace hfe { namespace world {')
    for method, declaration, pure in _info.methods('IObjectComponent', 'Game/Objects/IObjectComponent.h'):
        if not pure:
            qualified = declaration.replace(method + '(', 'IObjectComponent::' + method + '(', 1)
            lines.append(qualified + ' { unexpected("IObjectComponent::' + method + ' (base)"); }')
    lines.append('} } }')
    (out / 'factory_observers.inl').write_text('\n'.join(lines) + '\n', encoding='utf-8')


PASS_MODES = ['courier_pool', 'courier_free', 'courier_set', 'courier_armament', 'courier_weapons',
              'courier_polygon', 'factory_iface', 'factory_register', 'factory_real', 'factory_plugins',
              'factory_source', 'factory_update', 'real_getters', 'real_altitude', 'real_altitude_nan',
              'real_settime', 'physical_forwards', 'weapon_info', 'factory_weapon', 'armament_real',
              'unit_lifecycle', 'unit_forwards', 'unit_factory', 'cover_factory']
STOP_MODES = {
    'stop_known': 'dice::hfe::ai::InformationFactory::requestInformationKnown [InformationKnown]',
    'stop_weapon_known': 'dice::hfe::ai::InformationFactory::requestWeaponInfo [WeaponInfoKnown]',
    'stop_armament_known': 'dice::hfe::ai::InformationFactory::requestInformationPlugin [IPIArmamentKnown]',
    'stop_cover_physical': 'dice::hfe::PlanePolygon::PlanePolygon(BoundingBox*, const Mat4&)',
    'stop_physical_known': 'dice::hfe::ai::InformationFactory::requestInformationPlugin [IPIPhysicalKnown]',
}


def run(exe):
    summary = []
    check_count = 0
    for mode in PASS_MODES:
        p = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=15)
        output = p.stdout + p.stderr
        (exe.parent / (mode + '.log')).write_text(output, encoding='utf-8')
        if p.returncode != 0 or not output.startswith('PASS ' + mode + ' ') or 'UNEXPECTED' in output or 'FAIL' in output:
            raise RuntimeError((mode, p.returncode, output))
        check_count += int(re.search(r'checks=(\d+)', output).group(1))
        summary.append(output.strip()); print(summary[-1])
    for mode, name in STOP_MODES.items():
        p = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=15)
        output = p.stdout + p.stderr
        (exe.parent / (mode + '.log')).write_text(output, encoding='utf-8')
        expected = '[FRONTIER] ' + name + ' reached\n'
        if p.returncode != 3 or output != expected:
            raise RuntimeError((mode, p.returncode, output))
        summary.append('PASS ' + mode + ' exact diagnostic and abort'); print(summary[-1])
    summary.append('PASS information factory/courier: %d strict x86 cases / %d checks, %d named stops'
                   % (len(PASS_MODES), check_count, len(STOP_MODES)))
    (exe.parent / 'runtime.log').write_text('\n'.join(summary) + '\n', encoding='utf-8')
    print(summary[-1])


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('generate', 'run'):
        raise SystemExit('Usage: information_factory_runtime.py generate OUTPUT | run EXE')
    target = Path(sys.argv[2]).resolve()
    generate(target) if sys.argv[1] == 'generate' else run(target)
