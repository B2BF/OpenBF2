"""[TEMP] Whole-production IPICover/PlanePolygon fixture with typed chosen observers."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import struct
import subprocess
import sys

HERE = Path(__file__).resolve().parent
CODE = HERE.parents[1]
BF2 = CODE / 'BF2'
AI = 'AI/AIDLL/AI/'
INTERFACE = 'AI/AIInterface/BFKnowledge/'
HELPER = HERE / 'information_frontiers_runtime.py'
spec = importlib.util.spec_from_file_location('ipicover_observer_helpers', HELPER)
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)

SOURCES = [AI + 'AIKnowledge/IPICover.cpp', AI + 'AIKnowledge/IPICover.h',
           AI + 'AIKnowledge/Information.h', AI + 'AIKnowledge/InformationPlugIn.h',
           AI + 'Main/CommonSingletonServer.h', INTERFACE + 'IAIObject.h',
           INTERFACE + 'IAIObjectPlugIn.h', INTERFACE + 'BFTimer.h',
           INTERFACE + 'PlanePolygon.cpp', INTERFACE + 'PlanePolygon.h',
           'Math/Vec3.cpp', 'Math/Vec3.h', 'Math/Mat4.h',
           'Core/IBase.h', 'Game/Objects/IObjectComponent.h']

NORMAL = ['constructor', 'constructor_capture', 'null_outline', 'replacement',
          'replacement_null', 'timer_replacement', 'timer_virtual', 'forwards',
          'fresh_owner', 'direction_finite', 'direction_zero', 'direction_nan']
CRASH = ['null_information', 'null_object', 'null_cover', 'null_physical', 'null_timer']
FRONTIER = '[FRONTIER] dice::hfe::PlanePolygon::PlanePolygon(BoundingBox*, const Mat4&) reached'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    lines = ['// [TEMP] Typed observers from unchanged production declarations; unused slots abort.',
             'namespace dice { namespace hfe { namespace ai {']
    observer = helper.observer
    observer(lines, 'ObservedObject', 'IAIObject',
             [('IBase', 'Core/IBase.h'), ('IObjectComponent', 'Game/Objects/IObjectComponent.h'),
              ('IAIObject', INTERFACE + 'IAIObject.h')],
             {'bool isCover() const', 'IAIObjectCover* getCover() const',
              'IAIObjectPhysical* getPhysical() const'},
             ['explicit ObservedObject(const char* label) : label(label) {}',
              'const char* label; bool coverFlag = true;',
              'IAIObjectCover* cover = nullptr; IAIObjectPhysical* physical = nullptr;',
              'void (*onIsCover)() = nullptr; void (*onGetCover)() = nullptr;'])
    observer(lines, 'ObservedInformation', 'Information',
             [('Information', AI + 'AIKnowledge/Information.h')], {'Mat4& getTransformation()'},
             ['explicit ObservedInformation(IAIObject* object) : Information(object, 0.0) {}',
              'void owner(IAIObject* object) { m_object = object; }', 'Mat4 transform = {};'],
             pure_only=True)
    cover_custom = {decl for _, decl, _ in helper.methods('IAIObjectCover', INTERFACE + 'IAIObjectPlugIn.h')
                    if decl != 'IPlanePolygon* getObjectOutline(Mat4& transformation)'}
    observer(lines, 'ObservedCover', 'IAIObjectCover',
             [('IAIObjectPlugIn', INTERFACE + 'IAIObjectPlugIn.h'),
              ('IAIObjectCover', INTERFACE + 'IAIObjectPlugIn.h')], cover_custom,
             ['explicit ObservedCover(const char* label) : IAIObjectCover(nullptr), label(label) {}',
              'const char* label; IPlanePolygon* outline = nullptr; IPlanePolygon* local = nullptr;',
              'Vec3 vertex = {1.0f,2.0f,3.0f}, center = {4.0f,5.0f,6.0f}, normal = {7.0f,8.0f,9.0f};',
              'Vec3 corner = {-1.0f,-2.0f,-3.0f};', 'void (*onOutline)() = nullptr;'])
    observer(lines, 'ObservedPhysical', 'IAIObjectPhysical',
             [('IAIObjectPlugIn', INTERFACE + 'IAIObjectPlugIn.h'),
              ('IAIObjectPhysical', INTERFACE + 'IAIObjectPlugIn.h')],
             {'float getVolume() const', 'BoundingBox* getCurrentBoundingBox()'},
             ['ObservedPhysical() : IAIObjectPhysical(nullptr) {}'])
    observer(lines, 'ObservedTimer', 'IBFTimer', [('IBFTimer', INTERFACE + 'BFTimer.h')],
             {'double getGameTime() const'},
             ['double now = 0.0; const char* label = "timer"; void (*callback)() = nullptr;'])
    lines.append('} } }')
    # Out-of-line defaults owned by an unlinked base TU; never reached in this fixture.
    lines.append('namespace dice { namespace hfe { namespace world {')
    for name, decl, pure in helper.methods('IObjectComponent', 'Game/Objects/IObjectComponent.h'):
        if not pure:
            lines.append(decl.replace(name + '(', 'IObjectComponent::' + name + '(', 1)
                         + ' { unexpected("IObjectComponent base"); }')
    lines.append('} } }')
    (out / 'ipicover_observers.inl').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    manifest = {'production': {p: sha(BF2 / p) for p in SOURCES},
                'fixture': {p.name: sha(p) for p in [HERE / ('ipicover_runtime' + s)
                                                   for s in ('.cpp', '.py', '.cmd')] + [HELPER]},
                'generated': sha(out / 'ipicover_observers.inl')}
    (out / 'source_hashes.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


def run(exe):
    out = exe.parent
    image = exe.read_bytes()
    offset = struct.unpack_from('<I', image, 0x3c)[0]
    if image[:2] != b'MZ' or image[offset:offset+4] != b'PE\0\0' or struct.unpack_from('<H', image, offset+4)[0] != 0x14c:
        raise RuntimeError('not an x86 PE executable')
    lines, checks, records = [], 0, []
    for mode in NORMAL + CRASH + ['physical_frontier']:
        p = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=20)
        output = p.stdout + p.stderr
        code = p.returncode & 0xffffffff
        (out / (mode + '.log')).write_text('exit=0x%08X\n%s' % (code, output), encoding='utf-8')
        if mode in NORMAL:
            match = re.fullmatch(r'PASS ' + mode + r' checks=(\d+)\n', output)
            if p.returncode or not match:
                raise RuntimeError((mode, code, output))
            count = int(match.group(1))
            checks += count
            line = output.strip()
        elif mode in CRASH:
            if code != 0xc0000005 or output != 'READY ' + mode + '\n':
                raise RuntimeError((mode, code, output))
            count = 0
            line = 'PASS ' + mode + ' expected access violation 0xC0000005'
        else:
            expected = 'READY physical_frontier\nTRACE object.isCover,object.physical,boundingBox,transformation\n' + FRONTIER + '\n'
            if p.returncode != 3 or output != expected:
                raise RuntimeError((mode, code, output))
            count = 0
            line = 'PASS physical_frontier exact production diagnostic / exit 3'
        records.append({'mode': mode, 'exit': code, 'checks': count})
        lines.append(line)
        print(line)
    lines.append('PASS IPICover: %d strict x86 cases / %d checks / %d expected crashes / 1 named frontier' %
                 (len(records), checks, len(CRASH)))
    print(lines[-1])
    (out / 'runtime.log').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    (out / 'results.json').write_text(json.dumps({'passed': len(records), 'checks': checks,
        'expected_crashes': len(CRASH), 'named_frontiers': 1, 'strict_link': True, 'force': False,
        'exe_sha256': sha(exe), 'cases': records}, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('generate', 'run'):
        raise SystemExit('generate OUTPUT | run EXE')
    target = Path(sys.argv[2]).resolve()
    generate(target) if sys.argv[1] == 'generate' else run(target)
