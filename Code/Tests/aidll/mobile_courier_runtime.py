"""[TEMP] Strict x86 runtime driver for IPIMobile and BFInformationCourier.

The fixture links the production translation units whole.  It generates typed engine/pathfinding/
timer/settings observers whose unused virtual slots abort, records the exact source revision used,
and drives each behavior in its own process.  Crash modes document shipped missing-null-guard
behavior without adding a production guard or a source shim.
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

_spec = importlib.util.spec_from_file_location(
    'information_fixture', Path(__file__).with_name('information_frontiers_runtime.py'))
_info = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_info)

OBJECT_CUSTOM = {
    'IAIObjectMobile* getMobile() const',
    'IAIObjectUnit* getUnit() const',
    'bool isOfType(InformationType type) const',
    'unsigned int getInformation(int side)',
    'const Vec3& getPosition() const',
    'const Mat4& getTransformation() const',
    'int getSide() const',
}
MOBILE_CUSTOM = {
    'const Vec3& getSpeed()', 'const Vec3& getRotationalSpeed()',
    'float getSpeedMagnitude()', 'int getVehicleNumber() const', 'float getMaxSpeed() const',
    'float getTurnRadius() const', 'float getDrag() const', 'float getCoverSearchRadius() const',
    'float getLodHeight() const', 'bool isTurnable() const', 'float getSoundSphereRadius() const',
    'float getSoundSphereRadiusMin() const', 'float getSoundSphereRadiusMax() const',
    'float getHearingProbabilityMax() const', 'float getHearingProbabilityMin() const',
    'float getAvoidCollisionLookAhead() const', 'IAIObject* getSector() const',
    'int getLastPassedPortal() const', 'float getSprintStamina() const',
}
UNIT_CUSTOM = {'int getEquipmentType() const'}
PATH_CUSTOM = {
    'int getTraceMapIndex() const',
    'bool getPolygon(const Vec3&, int, unsigned int&, unsigned int&, unsigned int, unsigned int, int) const',
    'bool getMaterial(int, unsigned int, unsigned int, unsigned char&) const',
}
TIMER_CUSTOM = {'double getGameTime() const'}

SOURCES = [AI + 'AIKnowledge/' + name for name in (
    'BFInformationCourier.cpp', 'BFInformationCourier.h', 'InformationFactory.cpp',
    'InformationFactory.h', 'Information.cpp', 'Information.h', 'InformationReal.cpp',
    'InformationReal.h', 'InformationManager.cpp', 'InformationManager.h', 'IPIMobile.cpp',
    'IPIMobile.h', 'IPIMobileReal.cpp', 'IPIMobileReal.h', 'IPIMobileKnown.cpp',
    'IPIMobileKnown.h')]
SOURCES += [AI + 'Main/IAIPathfinding.cpp', AI + 'Main/IAIPathfindingSetup.cpp', AI + 'Main/IAIPathfinding.h',
            AI + 'Main/AISettingsInterfaces.h', BFK + 'IAIObject.h',
            BFK + 'IAIObjectPlugIn.h', BFK + 'BFTimer.h']
FIXTURE_FILES = [
    Path(__file__).resolve(),
    Path(__file__).with_name('mobile_courier_runtime.cpp').resolve(),
    Path(__file__).with_name('mobile_courier_runtime.cmd').resolve(),
    Path(__file__).with_name('information_frontiers_runtime.py').resolve(),
]


def generate(out: Path) -> None:
    out.mkdir(parents=True, exist_ok=True)
    evidence = []
    frontier_functions = []
    for relative in SOURCES:
        path = CODE / 'BF2' / relative
        text = path.read_text(encoding='utf-8')
        evidence.append({'source': path.as_posix(),
                         'source_sha256': hashlib.sha256(text.encode()).hexdigest()})
        if relative.endswith('BFInformationCourier.cpp'):
            if 'BFInformationCourier::saveCurrentPolygon [IPIMobile position records]' in text:
                frontier_functions.append('BFInformationCourier::saveCurrentPolygon')
            if 'BFInformationCourier::updateCurrentPolygon"' in text:
                frontier_functions.append('BFInformationCourier::updateCurrentPolygon')
        if relative.endswith('InformationFactory.cpp'):
            if 'requestInformationPlugin [IPIMobileReal]' in text:
                frontier_functions.append('InformationFactory mobile real arm')
            if 'requestInformationPlugin [IPIMobileKnown]' in text:
                frontier_functions.append('InformationFactory mobile known arm')
    for path in FIXTURE_FILES:
        data = path.read_bytes()
        evidence.append({'fixture': path.as_posix(),
                         'source_sha256': hashlib.sha256(data).hexdigest()})
    (out / 'source.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')

    lines = [
        '// [TEMP] Generated typed observers. Hand-written definitions exist only for the slots',
        '// intentionally observed by mobile_courier_runtime.cpp; every other virtual aborts.',
        'namespace dice { namespace hfe { namespace ai {'
    ]
    object_parts = [('IBase', 'Core/IBase.h'),
                    ('IObjectComponent', 'Game/Objects/IObjectComponent.h'),
                    ('IAIObject', BFK + 'IAIObject.h')]
    _info.observer(lines, 'EngineObject', 'IAIObject', object_parts, OBJECT_CUSTOM,
                   ['    explicit EngineObject(const char* id);',
                    '    std::string m_id; Vec3 m_position; Mat4 m_transform;',
                    '    int m_side; bool m_mobileType; IAIObjectMobile* m_mobile;',
                    '    IAIObjectUnit* m_unit; unsigned int m_handles[3];'])
    _info.observer(lines, 'EngineMobile', 'IAIObjectMobile',
                   [('IAIObjectPlugIn', BFK + 'IAIObjectPlugIn.h'),
                    ('IAIObjectMobile', BFK + 'IAIObjectPlugIn.h')], MOBILE_CUSTOM,
                   ['    explicit EngineMobile(IAIObject* owner, const char* id);',
                    '    std::string m_id; Vec3 m_speed, m_rotationalSpeed;',
                    '    int m_vehicleNumber; IAIObject* m_sector; int m_lastPortal;',
                    '    IBFTimer* m_timerOnSpeed;'])
    _info.observer(lines, 'EngineUnit', 'IAIObjectUnit',
                   [('IAIObjectPlugIn', BFK + 'IAIObjectPlugIn.h'),
                    ('IAIObjectUnit', BFK + 'IAIObjectPlugIn.h')], UNIT_CUSTOM,
                   ['    explicit EngineUnit(IAIObject* owner) : IAIObjectUnit(owner), m_equipment(0) {}',
                    '    int m_equipment;'])
    _info.observer(lines, 'PathObserver', 'IAIPathfinding',
                   [('IAIPathfinding', AI + 'Main/IAIPathfinding.h')], PATH_CUSTOM,
                   ['    PathObserver();',
                    '    int m_traceMap;'])
    _info.observer(lines, 'TimerObserver', 'IBFTimer',
                   [('IBFTimer', BFK + 'BFTimer.h')], TIMER_CUSTOM,
                   ['    explicit TimerObserver(const char* id = "timer")'
                    ' : m_id(id), m_now(0.0), m_speedOnNow(nullptr), m_speedValue() {}',
                    '    std::string m_id; double m_now;',
                    '    Vec3* m_speedOnNow; Vec3 m_speedValue;'])
    _info.observer(lines, 'SettingsObserver', 'IAISettings',
                   [('IAISettings', AI + 'Main/AISettingsInterfaces.h')], set(),
                   ['    SettingsObserver() {}'])
    lines.append('} } }')
    lines.append('namespace dice { namespace hfe { namespace world {')
    for method, declaration, pure in _info.methods(
            'IObjectComponent', 'Game/Objects/IObjectComponent.h'):
        if not pure:
            qualified = declaration.replace(
                method + '(', 'IObjectComponent::' + method + '(', 1)
            lines.append(qualified + ' { unexpected("IObjectComponent::' + method + ' (base)"); }')
    lines.append('} } }')
    (out / 'mobile_observers.inl').write_text('\n'.join(lines) + '\n', encoding='utf-8')

    if frontier_functions:
        state = ('Expected temporary runtime failure: production still contains named stops for '\
                 + ', '.join(frontier_functions) + '.')
    else:
        state = 'Production prerequisites are present.'
    readme = f"""# Mobile/courier strict x86 validation

This directory is generated by `Code/Tests/aidll/mobile_courier_runtime.cmd`. It links the real
mobile, courier, information-factory, information-pool and pathfinding-interface translation units.
Typed observers abort on every unused virtual. No production replacement body is linked.

{state}

The suite covers x86 layouts, ValidPos field ordering, base material routing, Real and Known mobile
behavior (including the PE timed-estimate component roundings and timer-receiver capture), courier
save/restore and polygon-update branches, the retail Boolean second map argument, both destination
mobiles, factory construction, and subprocess-only missing-null-guard behavior.
`source.json` binds the result to the exact input files. `runtime.log` is written only after every
ordinary and expected-crash case passes.
"""
    (out / 'README.md').write_text(readme, encoding='utf-8')


PASS_MODES = [
    'layout', 'base_material', 'base_positions', 'real', 'real_rounding_order',
    'known', 'known_rounding',
    'save_branches', 'save_restore', 'update_branches', 'update_polygon',
    'update_second_failure', 'factory_mobile',
]
CRASH_MODES = [
    'crash_update_null_mobile', 'crash_update_null_unit',
    'crash_known_stale_real', 'crash_factory_stale_target',
]


def run(exe: Path) -> None:
    summary = []
    checks = 0
    for mode in PASS_MODES:
        process = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=20)
        output = process.stdout + process.stderr
        (exe.parent / (mode + '.log')).write_text(output, encoding='utf-8')
        if (process.returncode != 0 or not output.startswith('PASS ' + mode + ' ')
                or 'UNEXPECTED' in output or 'FAIL' in output or '[FRONTIER]' in output):
            raise RuntimeError((mode, process.returncode, output))
        checks += int(re.search(r'checks=(\d+)', output).group(1))
        summary.append(output.strip())
        print(summary[-1])
    for mode in CRASH_MODES:
        process = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=20)
        output = process.stdout + process.stderr
        (exe.parent / (mode + '.log')).write_text(output, encoding='utf-8')
        if (process.returncode == 0 or 'REACHED-AFTER' in output or '[FRONTIER]' in output
                or 'UNEXPECTED' in output or 'FAIL' in output):
            raise RuntimeError((mode, process.returncode, output))
        line = 'PASS ' + mode + ' preserved missing guard (subprocess failed before sentinel)'
        summary.append(line)
        print(line)
    summary.append('PASS mobile/courier: %d strict x86 cases / %d checks, %d guard crashes'
                   % (len(PASS_MODES), checks, len(CRASH_MODES)))
    (exe.parent / 'runtime.log').write_text('\n'.join(summary) + '\n', encoding='utf-8')
    with (exe.parent / 'README.md').open('a', encoding='utf-8') as readme:
        readme.write('\nValidation complete: ' + summary[-1] + '.\n')
    print(summary[-1])


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('generate', 'run'):
        raise SystemExit('Usage: mobile_courier_runtime.py generate OUTPUT | run EXE')
    target = Path(sys.argv[2]).resolve()
    generate(target) if sys.argv[1] == 'generate' else run(target)
