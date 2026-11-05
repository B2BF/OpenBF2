"""[TEMP] Exact member caller, typed observers, real information pool, strict x86."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import subprocess
import sys

CODE = Path(__file__).resolve().parents[2]
AI = 'AI/AIDLL/AI/'
spec = importlib.util.spec_from_file_location('typed_declarations', Path(__file__).with_name('sai_states_runtime.py'))
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
NORMAL = [
    'no_bot', 'timer_equal', 'timer_below', 'timer_nan', 'previous_nan',
    'timer_extended', 'displacement_clear', 'displacement_nan', 'displacement_dx_spill',
    'invalid_position', 'null_player', 'dead_player', 'mobile_false', 'capture_formation',
    'fresh_index', 'membership_clear_timing', 'equipment_firing', 'equipment_zero',
    'equipment_stale', 'equipment_wrong_flags', 'equipment_null_plugin', 'equipment_manager_swap',
    'equipment_fresh_bot', 'mission_dead', 'mission_down', 'mission_leader_dead',
    'mission_leader_down', 'mission_fresh_player', 'mission_fresh_squad', 'within_formation',
    'waypoint_stop', 'artillery_stop', 'equipment_stop', 'mission_self_stop', 'mission_leader_stop']
REAL = ['real_position_null', 'real_position_callback', 'real_position_stop']
STOPS = {
    'waypoint_stop': 'SquadMember::update (WaypointList position query)',
    'artillery_stop': 'SquadMember::update (SquadManager::isInArtilleryDangerZone)',
    'equipment_stop': 'SquadMember::update (IPIArmament selected weapon)',
    'equipment_manager_swap': 'SquadMember::update (IPIArmament selected weapon)',
    'mission_self_stop': 'SquadMember::update (SquadCoordinator::acceptMission)',
    'mission_leader_stop': 'SquadMember::update (SquadCoordinator::acceptMission)',
    'real_position_stop': 'SquadMember::updateMemberPos (IAIObject information handle)'}


def generate(out):
    records = []
    path = CODE / 'BF2' / AI / 'AIBot/SquadMember.cpp'
    source = path.read_text(encoding='utf-8')
    for method in ('memberPathUpdateDue', 'memberPathDisplaced'):
        found = re.findall(r'^static bool ' + method + r'\([^\n]*\)\s*\{[\s\S]*?^}', source, re.M)
        if len(found) != 1: raise RuntimeError(('exact local numeric helper', method))
        records.append(dict(source=path.as_posix(),owner='',method=method,body=found[0],
                            source_sha256=hashlib.sha256(source.encode()).hexdigest(),body_sha256=hashlib.sha256(found[0].encode()).hexdigest()))
    methods = [('SquadMember', name) for name in ('update', 'getPosition', 'setHavePath', 'getPlayer', 'updateMemberPos')]
    methods += [('SquadCoordinator', 'getSquadFormation')]
    for owner, method in methods:
        path = CODE / 'BF2' / AI / 'AIBot' / (owner + '.cpp')
        source = path.read_text(encoding='utf-8')
        found = re.findall(r'^[^\n{};]+ ' + owner + '::' + method + r'\([^\n]*\)\s*\{[\s\S]*?^}', source, re.M)
        # The original trivial getPlayer leaf is one line, still extracted exactly.
        if method == 'getPlayer':
            found = re.findall(r'^IAIPlayer\* SquadMember::getPlayer\(\) \{[^\n]*\}', source, re.M)
        if len(found) != 1:
            raise RuntimeError(('unique actual definition', owner, method, len(found)))
        records.append(dict(source=path.as_posix(), owner=owner, method=method, body=found[0],
                            source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                            body_sha256=hashlib.sha256(found[0].encode()).hexdigest()))
    out.mkdir(parents=True, exist_ok=True)
    actual = ['// [TEMP] Exact production definitions; no statement transformations.',
              '#include "' + Path(__file__).with_suffix('.h').as_posix() + '"',
              'namespace dice { namespace hfe { namespace ai {']
    for record in records:
        if record['method'] == 'updateMemberPos':
            actual += ['#if defined(REAL_POSITION)', record['body'], '#endif']
        else:
            actual.append(record['body'])
    actual.append('} } }')
    (out / 'actual.cpp').write_text('\n\n'.join(actual), encoding='utf-8')
    (out / 'source.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
    mocks = ['// [TEMP] Typed virtual receivers; every unused call terminates.',
             'namespace dice { namespace hfe { namespace ai {']
    specs = [
        ('IAIPlayer', 'AI/AIInterface/BFKnowledge/AIPlayer.h', 'ObservedPlayer',
         {'getIsAlive': 'return playerValue(identity,0)!=0;', 'isManDown': 'return playerValue(identity,1)!=0;',
          'getCurrentVehicle': 'return playerValue(identity,2) ? reinterpret_cast<IAIObject*>(1) : nullptr;'}, ''),
        ('Bot', AI+'AIBot/BotMain.h', 'ObservedBot',
         {'isFiring': 'return botValue(identity,0)!=0;', 'getCurrentEquipment': 'return botValue(identity,1);',
          'getWaypoints': 'botValue(identity,2); return nullptr;'}, ''),
        ('Information', AI+'AIKnowledge/Information.h', 'ObservedInformation', {},
         'ObservedInformation():Information(nullptr,0.0) {}')]
    for base, header, name, bodies, constructor in specs:
        mocks += ['class '+name+' : public '+base+' { public: int identity=0;', constructor]
        for method, decl in helpers.methods(base, header):
            mocks.append(decl+' override { '+bodies.get(method, 'unexpected("'+base+'::'+method+'");')+' }')
        mocks.append('};')
    mocks.append('} } }')
    (out/'mocks.inl').write_text('\n'.join(mocks),encoding='utf-8')
    paths = [Path(__file__).with_suffix(s) for s in ('.h','.cpp','.py','.cmd')]
    paths.append(Path(__file__).with_name('sai_states_runtime.py'))
    (out/'fixture_sources.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},indent=2),encoding='utf-8')
    paths = [CODE/'BF2'/p for p in (
        AI+'Main/InformationManager.cpp', AI+'Main/InformationManager.h', 'Core/SpHandlePool.h',
        AI+'AIKnowledge/Information.h', AI+'AIBot/SquadMember.h', AI+'AIBot/SquadCoordinator.h',
        AI+'AIBot/SquadFormation.h', AI+'AIBot/BotMain.h', 'AI/AIInterface/BFKnowledge/AIPlayer.h')]
    (out/'whole_sources.json').write_text(json.dumps({p.as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},indent=2),encoding='utf-8')


def run(out):
    results=[]
    checks=0
    stops=0
    for real,modes in [(False,NORMAL),(True,REAL)]:
        for mode in modes:
            proc=subprocess.run([str(out/('real_position.exe' if real else 'member_update_runtime.exe')),mode],capture_output=True,text=True,timeout=15)
            output=proc.stdout+proc.stderr
            (out/(mode+'.log')).write_text(output,encoding='utf-8')
            if mode in STOPS:
                expected='VERIFIED '+mode+'\n[FRONTIER] dice::hfe::ai::'+STOPS[mode]+' reached\n'
                if proc.returncode!=3 or output!=expected: raise RuntimeError((mode,proc.returncode,output))
                result='PASS '+mode+' exact checked trace/diagnostic/abort 3'; stops+=1
            else:
                match=re.fullmatch(r'PASS '+mode+r' ([1-9][0-9]*) checks\n',output)
                if proc.returncode!=0 or not match: raise RuntimeError((mode,proc.returncode,output))
                checks+=int(match.group(1)); result='PASS '+mode+' '+match.group(1)+' checks'
            results.append(result);print(result)
    results.append('PASS member update: '+str(len(results))+' strict x86 cases, '+str(checks)+' normal checks, '+str(stops)+' exact abort traces')
    (out/'runtime.log').write_text('\n'.join(results)+'\n',encoding='utf-8');print(results[-1])


if __name__=='__main__':
    if len(sys.argv)!=3 or sys.argv[1] not in ('generate','run'): raise SystemExit('generate OUT | run OUT')
    target=Path(sys.argv[2]).resolve()
    generate(target) if sys.argv[1]=='generate' else run(target)
