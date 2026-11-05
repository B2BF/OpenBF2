"""[TEMP] Exact bot-scheduler step bodies (2026-09-28) with typed observers at every collaborator.

BotManager::actionExecutePlan, actionDecisionMaking, actionPathfinding, actionHearing and
actionSensing, BotNavigation::isRunning, AIPathfinding::updatePathfinding (over the real header
PathfindingEngine::update / AStarResource::update templates) and BotMain::updatePlayerAction /
resetAllControls are extracted verbatim, each file-local speed helper with its own TU, into two
generated translation units. The real InformationManager pool, io::Input<PlayerInputMap,64>::setInput
(IO/Input/PlayerInput.cpp) and Core/Debug.cpp are linked; retail's Debug(Error) paths run through
the real Debug with a fixture callback. Every other collaborator is a typed observer generated from
the real headers; any call nobody expects terminates. Lifetimes are chosen scaffolding, not recovery.
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
spec = importlib.util.spec_from_file_location('typed_methods', Path(__file__).with_name('spawn_metadata_runtime.py'))
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)

NORMAL = ['empty_roster', 'count_1_false', 'count_1_true', 'count_2', 'count_3_false', 'count_3_true',
          'count_4', 'count_5_false', 'count_5_true', 'skip_arms', 'no_information',
          'execute_armed', 'execute_unarmed', 'out_of_budget', 'deadline_equal', 'deadline_nan',
          'queue_planned', 'queue_planned_equal', 'queue_default_plan', 'queue_unplanned',
          'queue_unplanned_equal', 'ladder_mapped', 'ladder_unmapped', 'two_bots_budget_split',
          'pending_no_player', 'pending_slow_on_foot', 'pending_equal_on_foot', 'pending_fast',
          'pending_slow_not_on_foot', 'pending_no_mobile', 'pending_in_water', 'pending_airborne',
          'pending_nan_speed', 'pending_witness', 'cursor_wrap',
          'stall_clear', 'stall_coast', 'stall_coast_unmapped', 'stall_at_limit', 'stall_nan',
          'stall_witness',
          'dm_empty', 'dm_interleave', 'dm_spare_negative', 'dm_skips', 'dm_zero_slice', 'dm_deadline',
          'pf_empty_roster', 'pf_serve', 'pf_skips', 'pf_time_out', 'pf_sweep',
          'se_take_two', 'se_drop_invalid', 'se_deadline', 'se_zero_weight', 'he_empty',
          'ap_idle', 'ap_polygon_spent']
STOPS = {'pending_vehicle_stop': 'BotManager::actionExecutePlan (IAIObject::getPrimaryObject)',
         'stall_over_limit_stop': 'BotMain::resetAllControls',
         'stall_slow_stop': 'BotMain::resetAllControls',
         'stall_no_mobile_stop': 'BotMain::resetAllControls',
         'he_resume_stop': 'BotManager::actionHearing (IAIObject hearing query)',
         'ap_cluster_stop': 'AStarResource::update',
         'ap_polygon_stop': 'AStarResource::update'}
# Retail's own Debug(Error) paths: the fixture's type-6 callback prints this line, then ~Debug aborts.
DEBUG_STOPS = {'dm_light_stop': 'DEBUG 6 Main BotManager.cpp 1666 Unhandled lod.',
               'dm_light_planned_stop': 'DEBUG 6 Main BotManager.cpp 1690 Unhandled lod.'}


def extract(relative, owner, names, static_helpers):
    path = CODE / 'BF2' / (AI + relative)
    source = path.read_text(encoding='utf-8')
    records = []
    for name in static_helpers + names:
        static = name in static_helpers
        qualified = name if static else owner + '::' + name
        # File-local helpers are anchored on `static` so a call site cannot match.
        prefix = r'^static [^\n;{}]+ ' if static else r'^[^\n;{}]+ '
        found = re.findall(prefix + qualified + r'\([^\n]*\)\s*(?:\{[^{}\n]*\}|\{[\s\S]*?^})', source, re.M)
        if len(found) != 1:
            raise RuntimeError(('unique body', relative, qualified, len(found)))
        records.append(dict(source=path.as_posix(), owner=owner, method=name, body=found[0],
                            source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                            body_sha256=hashlib.sha256(found[0].encode()).hexdigest()))
    return records


def methods_flat(name, relative):
    """helpers.methods, minus nested class bodies (AIPathfinding holds MapContainer's virtuals)."""
    text = (CODE / 'BF2' / relative).read_text(encoding='utf-8')
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    classes = re.findall(r'^class ' + name + r'(?:\s*:\s*[^\{]+)?\s*\{(.*?)^};', text, re.M | re.S)
    if len(classes) != 1:
        raise RuntimeError('Expected one class ' + name)
    body = re.sub(r'^[ \t]+class \w+[^{;]*\{.*?^[ \t]+\};', '', classes[0], flags=re.M | re.S)
    for original in re.findall(r'virtual\s+([^;{}]+);', body):
        pure = bool(re.search(r'=\s*0\s*$', original))
        declaration = re.sub(r'\s*=\s*0$', '', ' '.join(original.split()))
        method = re.search(r'(~?\w+)\s*\(', declaration).group(1)
        if not method.startswith('~'):
            yield method, declaration, pure


def unit(records):
    text = ['// [TEMP] Exact untouched production definitions.',
            '#include "' + Path(__file__).with_suffix('.h').as_posix() + '"',
            'namespace dice { namespace hfe { namespace ai {']
    text += [r['body'] for r in records]
    text.append('} } }')
    return '\n\n'.join(text)


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    manager = extract('Main/BotManager.cpp', 'BotManager',
                      ['actionExecutePlan', 'actionDecisionMaking', 'actionPathfinding', 'actionHearing',
                       'actionSensing'], ['speedSquaredAtMost'])
    manager += extract('AIBot/BotNavigation.cpp', 'BotNavigation', ['isRunning'], [])
    manager += extract('Main/AIPathfinding.cpp', 'AIPathfinding', ['updatePathfinding'], [])
    main = extract('AIBot/BotMain.cpp', 'BotMain', ['updatePlayerAction', 'resetAllControls'],
                   ['speedSquaredAtMost'])
    (out / 'actual_manager.cpp').write_text(unit(manager), encoding='utf-8')
    (out / 'actual_main.cpp').write_text(unit(main), encoding='utf-8')
    (out / 'source.json').write_text(json.dumps(manager + main, indent=2), encoding='utf-8')

    mocks = ['// [TEMP] Only named collaborators observe; every other virtual terminates.',
             'namespace dice { namespace hfe { namespace ai {']
    # Out-of-line definitions the chosen lifetimes need (vtables), all terminating.
    skip = {('BotMain', 'updatePlayerAction'), ('AIPathfinding', 'updatePathfinding')}
    for base, header in [('BotManager', AI + 'Main/BotManager.h'), ('BotMain', AI + 'AIBot/BotMain.h'),
                         ('AIPathfinding', AI + 'Main/AIPathfinding.h')]:
        for name, decl, pure in methods_flat(base, header):
            if pure or (base, name) in skip:
                continue
            signature = re.sub(r'\b' + name + r'(?=\s*\()', base + '::' + name, decl, count=1)
            mocks.append(signature + ' { unexpected("' + base + '::' + name + '"); }')
    observed = [
        ('Bot', AI + 'AIBot/BotMain.h', 'ObservedBot', {
            'isDead': 'return botIsDead(*this);',
            'isDeactivated': 'return botIsDeactivated(*this);',
            'getSquadMember': 'return botSquadMember(*this);',
            'getCurrentEquipment': 'return botEquipment(*this);',
            'getPlayer': 'return botPlayer(*this);',
            'getBotNavigation': 'return botNavigation(*this);',
            'rearm': 'botRearm(*this, i);',
            'planExecution': 'botPlanExecution(*this, slice);',
            'hasPlan': 'return botHasPlan(*this);',
            'hasDefaultPlan': 'return botHasDefaultPlan(*this);',
            'updatePlayerAction': 'botUpdatePlayerAction(*this, stalled);',
            'getLodLevel': 'return botLod(*this);',
            'decisionMaking': 'return botDecide(*this, slice);',
            'see': 'botSee(*this, dt);',
            'updateSensing': 'botSense(*this, dt);',
            'updatePathfinding': 'botPath(*this, a, b);'},
         'io::PlayerInput* input() { return m_playerInput; } void setInput(io::PlayerInput* p) { m_playerInput = p; }'),
        ('IAIPlayer', 'AI/AIInterface/BFKnowledge/AIPlayer.h', 'ObservedPlayer', {
            'getName': 'return playerName(*this);',
            'getCurrentVehicle': 'return playerCurrentVehicle(*this);',
            'getDefaultVehicle': 'return playerDefaultVehicle(*this);',
            'isClimbingLadder': 'return playerClimbing(*this);'}, ''),
        ('IPIMobile', AI + 'AIKnowledge/IPIMobile.h', 'ObservedMobile', {
            'getSpeed': 'return mobileSpeed(*this);'}, 'Vec3 speed{};'),
        ('IPIPhysical', AI + 'AIKnowledge/IPIPhysical.h', 'ObservedPhysical', {
            'isInAir': 'return physicalInAir(*this);',
            'isInWater': 'return physicalInWater(*this);'}, ''),
        ('IBFTimer', 'AI/AIInterface/BFKnowledge/BFTimer.h', 'ObservedTimer', {
            'getUpdateTime': 'return timerUpdate();',
            'getTimeIncrease': 'return timerIncrease();',
            'getGameTime': 'return timerGame();'}, ''),
        ('IAIGameEffector', 'AI/AIInterface/BFKnowledge/BFGameEffector.h', 'ObservedEffector', {
            'respawnBot': 'return effectorRespawn(player);'}, ''),
        ('IAIPathfinding', AI + 'Main/IAIPathfinding.h', 'ObservedPathfinder', {
            'updatePathfinding': 'pathfinderUpdate(budget);'}, ''),
        ('IAIObjectEnvironment', 'AI/AIInterface/BFKnowledge/BFObjectEnvironment.h', 'ObservedEnvironment', {
            'getWorldPlayerOccupiedObjectsWithIds': 'environmentOccupied(out);'}, ''),
        ('Information', AI + 'AIKnowledge/Information.h', 'ObservedInformation', {},
         'ObservedInformation() : Information(nullptr, 0.0) {}')]
    for base, header, owner, bodies, extra in observed:
        mocks.append('class ' + owner + ' : public ' + base + ' { public: int identity = 0; ' + extra)
        seen = set()
        for name, decl, pure in helpers.methods(base, header):
            if not pure:
                continue
            if (base, name) == ('IAIPathfinding', 'updatePathfinding'):
                decl = decl.replace('updatePathfinding(double)', 'updatePathfinding(double budget)')
            seen.add(name)
            mocks.append('    ' + decl + ' override { ' +
                         bodies.get(name, 'unexpected("' + base + '::' + name + '");') + ' }')
        missing = set(bodies) - seen
        if missing:
            raise RuntimeError(('observed methods not pure in', base, sorted(missing)))
        mocks.append('};')
    mocks.append('} } }')
    (out / 'mocks.inl').write_text('\n'.join(mocks), encoding='utf-8')
    paths = [Path(__file__).with_suffix(s) for s in ('.h', '.cpp', '.py', '.cmd')]
    paths.append(Path(__file__).with_name('spawn_metadata_runtime.py'))
    (out / 'fixture_sources.json').write_text(
        json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}, indent=2), encoding='utf-8')
    whole = [CODE / 'BF2' / p for p in (AI + 'Main/InformationManager.cpp', 'Core/SpHandlePool.h',
                                        'IO/Input/PlayerInput.cpp', 'IO/Input/Input.h', 'Core/Debug.cpp')]
    (out / 'whole_sources.json').write_text(
        json.dumps({p.as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in whole}, indent=2),
        encoding='utf-8')


def run(out):
    results, checks, stops = [], 0, 0
    for mode in NORMAL + list(STOPS) + list(DEBUG_STOPS):
        proc = subprocess.run([str(out / 'plan_executor_runtime.exe'), mode], capture_output=True, text=True,
                              timeout=15)
        output = proc.stdout + proc.stderr
        (out / (mode + '.log')).write_text(output, encoding='utf-8')
        if mode in STOPS or mode in DEBUG_STOPS:
            tail = ('[FRONTIER] dice::hfe::ai::' + STOPS[mode] + ' reached') if mode in STOPS else DEBUG_STOPS[mode]
            expected = 'VERIFIED ' + mode + '\n' + tail + '\n'
            if proc.returncode != 3 or output != expected:
                raise RuntimeError((mode, proc.returncode, output))
            result = 'PASS ' + mode + ' exact checked state/diagnostic/abort 3'
            stops += 1
        else:
            match = re.fullmatch(r'PASS ' + mode + r' ([1-9][0-9]*) checks\n', output)
            if proc.returncode != 0 or not match:
                raise RuntimeError((mode, proc.returncode, output))
            checks += int(match.group(1))
            result = output.strip()
        results.append(result)
        print(result)
    results.append('PASS bot scheduler steps: ' + str(len(results)) + ' strict x86 cases, ' + str(checks) +
                   ' normal checks, ' + str(stops) + ' exact abort traces')
    (out / 'runtime.log').write_text('\n'.join(results) + '\n', encoding='utf-8')
    print(results[-1])


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('generate', 'run'):
        raise SystemExit('Usage: plan_executor_runtime.py generate OUT | run OUT')
    target = Path(sys.argv[2]).resolve()
    generate(target) if sys.argv[1] == 'generate' else run(target)
