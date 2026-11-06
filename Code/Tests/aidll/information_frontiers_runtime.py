"""[TEMP] Information.cpp, whole: the forwards, plug-ins, booking and getHandle - exact outcomes.

Until 2026-09-29 this fixture proved four named frontier stops (isPrimary, getPrimaryObject,
getFirstSecondaryObject, getKitType). Those bodies are written now, so it drives the whole
production TU against typed observers instead: every IAIObject / plug-in slot the TU is expected to
call is observed and traced, and every other slot aborts. The recovered IPIUnit TU is linked whole;
its constructor and mustBorrowArmament path use typed timer, settings, physical and unit observers.
The file name is kept so the suite's evidence folders stay continuous.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

CODE = Path(__file__).resolve().parents[2]
AI = 'AI/AIDLL/AI/'


def methods(name, relative):
    """bot_spawner_runtime.methods, plus inline bodies: `virtual ~X() {}` has no ';' of its own, so
    the shared helper's split swallows the declaration after it (IAIObject::lockInit)."""
    source = (CODE / 'BF2' / relative).read_text(encoding='utf-8')
    found = re.findall(r'^class ' + name + r'(?:\s*:\s*[^\{]+)?\s*\{(.*?)^};', source, re.M | re.S)
    if len(found) != 1:
        raise RuntimeError('Expected one real class: ' + name)
    body = re.sub(r'/\*.*?\*/|//[^\n]*', '', found[0], flags=re.S)
    body = re.sub(r'\)(\s*const)?\s*\{[^{}]*\}', lambda m: ')' + (m.group(1) or '') + ';', body)
    for original in re.findall(r'virtual\s+([^;]+);', body):
        pure = bool(re.search(r'=\s*0\s*$', original))
        declaration = re.sub(r'\s*=\s*0$', '', ' '.join(original.split()))
        method = re.search(r'(~?\w+)\s*\(', declaration).group(1)
        if not method.startswith('~'):
            yield method, declaration, pure


class _Helpers:
    methods = staticmethod(methods)


_helpers = _Helpers()

# Declarations (as _helpers.methods normalises them) whose bodies the fixture .cpp writes itself.
OBJECT_CUSTOM = {
    'const std::string& getName() const',
    'const std::string& getAIName() const',
    'const std::string& getObjectTypeName() const',
    'unsigned int getInformation(int side)',
    'unsigned int getObjectId() const',
    'SoldierPose getSoldierPose() const',
    'int getKitType() const',
    'bool isPrimary() const',
    'IAIObject* getPrimaryObject()',
    'IAIObject* getSecondaryObject(int& iterator)',
    'IAIObject* getFirstSecondaryObject(int& iterator)',
    'int getFirstSecondaryObjectIterator() const',
    'bool getObjectSpawnerHolding() const',
    'IAIObject* getObjectSpawner() const',
    'bool isRootObject() const',
    'int getPlayerId() const',
    'IAIObjectUnit* getUnit() const',
    'bool isCover() const',
    'IAIObjectCover* getCover() const',
}
PHYSICAL_CUSTOM = {'void setTime(double time)', 'float getHealth() const', 'bool getManDown() const'}
MOBILE_CUSTOM = {'void setTime(double time)'}
TIMER_CUSTOM = {'double getGameTime() const'}
SETTINGS_CUSTOM = {'int getNBattleStrengths()', 'int getNStrategicStrengths()'}
UNIT_CUSTOM = {
    'int getMaxAmmo() const', 'const int* getBattleStrengths() const',
    'const int* getStrategicStrengths() const',
    'bool mustBorrowArmament(IAIObject* vehicle) const',
}
SOURCES = [AI + 'AIKnowledge/Information.cpp', AI + 'AIKnowledge/Information.h',
           AI + 'AIKnowledge/InformationPlugIn.h', AI + 'AIKnowledge/IPIControlInfo.h',
           AI + 'AIKnowledge/IPIUnit.cpp', AI + 'AIKnowledge/IPIUnit.h',
           AI + 'AIKnowledge/IPICover.cpp', AI + 'AIKnowledge/IPICover.h',
           AI + 'AIKnowledge/IPIMobile.cpp', AI + 'AIKnowledge/IPIMobile.h',
           'AI/AIInterface/BFKnowledge/PlanePolygon.cpp',
           'AI/AIInterface/BFKnowledge/PlanePolygon.h', 'Math/Vec3.cpp',
           AI + 'AIUtils/WeaponInfoManager.cpp',
           AI + 'AIUtils/CustomCurve.cpp', AI + 'AIUtils/SCurve.cpp',
           AI + 'Main/AISettingsInterfaces.h',
           AI + 'Main/IAIPathfinding.h',
           'AI/AIInterface/BFKnowledge/BFTimer.h',
           'AI/AIInterface/BFKnowledge/IAIObject.h',
           'AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h']


# IObjectComponent is declared inside namespace world; its parameter types need the qualifier the
# ai namespace writes (IAIObject.h itself says `world::IObject*`).
WORLD_TYPES = [('IObject*', 'world::IObject*'), ('IPlayer*', 'world::IPlayer*'),
               ('TemplateMessage', 'world::TemplateMessage')]


def observer(lines, cls, base, parts, custom, extra, pure_only=False):
    seen = set()
    lines.append('class ' + cls + ' : public ' + base + ' { public:')
    lines.extend(extra)
    for owner, relative in parts:
        for method, declaration, pure in _helpers.methods(owner, relative):
            if owner == 'IObjectComponent':
                for bare, qualified in WORLD_TYPES:
                    declaration = declaration.replace(bare, qualified)
            if declaration in seen or (pure_only and not pure):
                continue
            seen.add(declaration)
            if declaration in custom:
                lines.append('    ' + declaration + ' override;')
            else:
                lines.append('    ' + declaration + ' override { unexpected("' + owner + '::' + method + '"); }')
    missing = custom - seen
    if missing:
        raise RuntimeError(('custom declarations not found', cls, sorted(missing)))
    lines.append('};')


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    evidence = []
    for relative in SOURCES:
        path = CODE / 'BF2' / relative
        text = path.read_text(encoding='utf-8')
        if 'BF2_AI_FRONTIER' in text and relative.endswith('Information.cpp'):
            raise RuntimeError('Information.cpp still carries a frontier')
        evidence.append({'source': path.as_posix(), 'source_sha256': hashlib.sha256(text.encode()).hexdigest()})
    (out / 'source.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')

    lines = ['// [TEMP] Generated typed observers. Only the slots the fixture declares custom are',
             '// defined in information_frontiers_runtime.cpp; every other slot aborts by name.',
             'namespace dice { namespace hfe { namespace ai {']
    lines.append('class EngineUnit;')
    object_parts = [('IBase', 'Core/IBase.h'), ('IObjectComponent', 'Game/Objects/IObjectComponent.h'),
                    ('IAIObject', 'AI/AIInterface/BFKnowledge/IAIObject.h')]
    observer(lines, 'ObservedObject', 'IAIObject', object_parts, OBJECT_CUSTOM,
             ['    explicit ObservedObject(const char* id);',
              '    ~ObservedObject();',
              '    std::string m_id, m_name, m_aiName, m_typeName;',
              '    unsigned int m_handles[3];',
              '    ObservedObject* m_primary; ObservedObject* m_secondary; ObservedObject* m_spawner;',
              '    bool m_isPrimary; EngineUnit* m_unit;'])
    # Pure slots only: the base's own getInformationSort (3) is under test.
    observer(lines, 'NativeInformation', 'Information', [('Information', AI + 'AIKnowledge/Information.h')],
             set(), ['    explicit NativeInformation(IAIObject* object) : Information(object, 12.0) {}',
                     '    IPIArmament* armament() const { return m_armament; }',
                     '    IPIControlInfo* controlInfo() const { return m_controlInfo; }',
                     '    IPICover* cover() const { return m_cover; }',
                     '    double time() const { return m_time; }'],
             pure_only=True)
    observer(lines, 'PhysicalObserver', 'IPIPhysical', [('IPIPhysical', AI + 'AIKnowledge/IPIPhysical.h')],
             PHYSICAL_CUSTOM, ['    explicit PhysicalObserver(bool manDown) : m_manDown(manDown) {}',
                               '    ~PhysicalObserver();', '    bool m_manDown;'])
    observer(lines, 'MobileObserver', 'IPIMobile', [('IPIMobile', AI + 'AIKnowledge/IPIMobile.h')],
             MOBILE_CUSTOM, ['    MobileObserver() : IPIMobile(nullptr) {}', '    ~MobileObserver();'])
    observer(lines, 'TimerObserver', 'IBFTimer',
             [('IBFTimer', 'AI/AIInterface/BFKnowledge/BFTimer.h')], TIMER_CUSTOM,
             ['    double m_now = 1.0;'])
    observer(lines, 'SettingsObserver', 'IAISettings',
             [('IAISettings', AI + 'Main/AISettingsInterfaces.h')], SETTINGS_CUSTOM,
             ['    SettingsObserver() : m_battleCount(1), m_strategicCount(1) {}',
              '    int m_battleCount, m_strategicCount;'])
    observer(lines, 'EngineUnit', 'IAIObjectUnit',
             [('IAIObjectPlugIn', 'AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h'),
              ('IAIObjectUnit', 'AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h')], UNIT_CUSTOM,
             ['    explicit EngineUnit(IAIObject* owner);',
              '    int m_battle[1]; int m_strategic[1];'])
    # [TEMP] A native cover provider returning no owned outline. This allows CoverObserver to
    # run the real IPICover constructor/destructor while Information owns its observed lifetime.
    observer(lines, 'EmptyEngineCover', 'IAIObjectCover',
             [('IAIObjectPlugIn', 'AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h'),
              ('IAIObjectCover', 'AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h')],
             {'IPlanePolygon* getObjectOutline()'},
             ['    EmptyEngineCover() : IAIObjectCover(nullptr) {}'])
    lines.append('} } }')
    # The engine interface's non-pure defaults: nothing links world's own TU here, and each one
    # aborts if it is ever reached - they exist only because IObjectComponent's table names them.
    lines.append('namespace dice { namespace hfe { namespace world {')
    for method, declaration, pure in _helpers.methods('IObjectComponent', 'Game/Objects/IObjectComponent.h'):
        if pure:
            continue
        qualified = declaration.replace(method + '(', 'IObjectComponent::' + method + '(', 1)
        lines.append(qualified + ' { unexpected("IObjectComponent::' + method + ' (base)"); }')
    lines.append('} } }')
    (out / 'information_observers.inl').write_text('\n'.join(lines) + '\n', encoding='utf-8')


MODES = ['forwards', 'primary', 'secondary', 'spawner', 'handle', 'setters', 'remove', 'destroy',
         'borrow', 'settime_plain', 'settime_borrow', 'settime_mandown', 'booking', 'sort']


def run(exe):
    summary = []
    check_count = 0
    for mode in MODES:
        p = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=15)
        output = p.stdout + p.stderr
        (exe.parent / (mode + '.log')).write_text(output, encoding='utf-8')
        if p.returncode != 0 or not output.startswith('PASS ' + mode + ' ') or 'UNEXPECTED' in output or 'FAIL' in output:
            raise RuntimeError((mode, p.returncode, output))
        check_count += int(re.search(r'checks=(\d+)', output).group(1))
        summary.append(output.strip())
        print(summary[-1])
    summary.append('PASS Information: %d strict x86 cases / %d checks' % (len(MODES), check_count))
    (exe.parent / 'runtime.log').write_text('\n'.join(summary) + '\n', encoding='utf-8')
    print(summary[-1])


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('generate', 'run'):
        raise SystemExit('Usage: information_frontiers_runtime.py generate OUTPUT | run EXE')
    target = Path(sys.argv[2]).resolve()
    generate(target) if sys.argv[1] == 'generate' else run(target)
