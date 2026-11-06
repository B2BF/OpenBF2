"""[TEMP] Verbatim kit helper/caller/box helper; real headers and typed observers."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import subprocess
import sys

_spec = importlib.util.spec_from_file_location('primary_helpers', Path(__file__).with_name('bot_spawner_runtime.py'))
_helpers = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_helpers)
CODE = Path(__file__).resolve().parents[2]

# [TEMP] Independent table of inputs and expected choices, not a scoring implementation.
# Record tuple: side, strength, kit, health bits. Defaults are non-kit / full health.
def record(side,strength,kit=13,health=0x3f800000):
    return (side,strength,kit,health)
E=record(1,0)
F=record(2,0)
V=record(1,1)
D=record(2,1,13,0x3effffff)
SCORING_CASES={}
def case(name,records,available,result,draw=0xffffffff,scoring_draws=0,fallback=False,planned=None,**flags):
    SCORING_CASES['policy_score_'+name]=dict(records=records,available=available,result=result,draw=draw,
        scoring_draws=scoring_draws,fallback=fallback,planned=planned or {},**flags)
case('vehicle',[V],[6],6)
case('vehicle_equal',[V,record(2,1)],[6],2,fallback=True)
case('vehicle_plan_blocks',[V],[6],2,fallback=True,planned={20:(-1,6)})
case('vehicle_live_bin_blocks',[V,record(2,0,6)],[6],2,fallback=True)
case('vehicle_kit13',[V,F],[6],6)
case('vehicle_missing_to_infantry',[V,E],[0],0,scoring_draws=1)
case('vehicle_missing_to_repair',[V,V,D],[4],4)
case('infantry_equal',[E,E,E,F,F],[0,2,3,4],2,fallback=True)
case('infantry_above',[E,E,F],[0,2,3,4],0,scoring_draws=1)
for label,raw,equal,above in [('85',0xd9999a00,2,0),('55',0x8ccccd00,3,2),('25',0x40000000,4,3)]:
    case('threshold'+label+'_below',[E],[0,2,3,4],equal,raw-1,1)
    case('threshold'+label+'_equal',[E],[0,2,3,4],equal,raw,1)
    case('threshold'+label+'_above',[E],[0,2,3,4],above,raw+1,1)
case('missing0',[E],[2,3,4],2,scoring_draws=1)
case('missing02',[E],[3,4],3,scoring_draws=1)
case('missing023',[E],[4],4,scoring_draws=1)
case('missing_all',[E],[],2,scoring_draws=1,fallback=True)
case('infantry_priority_over_repair_bin',[E,E,record(2,0,4),D],[4],4,0,1)
case('repair_below',[D],[4],4)
case('repair_equal',[record(2,1,13,0x3f000000)],[4],2,fallback=True)
case('repair_above',[record(2,1,13,0x3f000001)],[4],2,fallback=True)
case('repair_nan',[record(2,1,13,0x7fc00000)],[4],2,fallback=True)
case('repair_plan_blocks',[D],[4],2,fallback=True,planned={20:(-1,4)})
case('repair_live_bin_blocks',[D,record(2,0,4)],[4],2,fallback=True)
case('repair_missing',[D],[],2,fallback=True)
case('map_mismatch',[V],[6],6,planned={40:(-1,6)})
case('map_self',[V],[6],6,planned={10:(-1,6)})
case('map_null',[V],[6],6,planned={30:(-1,6)})
case('map_copy_bin',[V],[6],2,fallback=True,planned={10:(1,4),20:(2,6)},mutate_source=True)
case('map_area_capture',[V],[6],2,fallback=True,planned={20:(1,6)},mutate_area=True)
case('shared_no_squad',[],[],2,fallback=True,no_squad=True)
case('shared_roster',[],[],2,fallback=True,roster=True)
case('shared_assigned_stop',[],[],None,fallback=True,assigned=True)
case('borrowed_vector',[V],[],6,replace_templates=True)
case('fresh_random',[E],[0,2,3,4],0,scoring_draws=1,fresh_random=True)
case('refill_negative',[E],[0,2,3,4],2,0xa0000000,1,refill=-1)
case('refill_captured_vector',[E],[0,2,3,4],3,scoring_draws=1,refill=624,refill_mutation=True)


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    path = CODE/'BF2/AI/AIDLL/AI/AIStrategicMap/BotSpawner.cpp'
    source = path.read_text(encoding='utf-8')
    evidence = []
    for signature in [r'KitType BotSpawner::GetRandomKit\([^\n]*\)',
                      r'int BotSpawner::findSuitableKit\([^\{]*']:
        found = re.findall('^'+signature+r'\s*\{[\s\S]*?^}', source, re.M)
        if len(found) != 1:
            raise RuntimeError(('exact body', signature, len(found)))
        evidence.append(dict(source=path.as_posix(), source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                             body=found[0], body_sha256=hashlib.sha256(found[0].encode()).hexdigest()))
    actual = ['// [TEMP] Verbatim statements, including the actual remaining policy stop.',
              '#include "'+Path(__file__).with_suffix('.h').as_posix()+'"',
              'namespace dice { namespace hfe { namespace ai {']
    actual += [e['body'] for e in evidence]
    path = CODE/'BF2/Math/Vec3.cpp'
    source = path.read_text(encoding='utf-8')
    found = re.findall(r'^Vec2& getBoxMaxCorner\([^\n]*\)\s*\{[\s\S]*?^}', source, re.M)
    if len(found) != 1:
        raise RuntimeError(('exact box helper',len(found)))
    evidence.append(dict(source=path.as_posix(), source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                         body=found[0], body_sha256=hashlib.sha256(found[0].encode()).hexdigest()))
    actual += ['}',found[0],'} }']
    (out/'random_kit_actual.cpp').write_text('\n\n'.join(actual), encoding='utf-8')
    (out/'source.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    whole_paths=['BF2/AI/AIDLL/AI/Main/InformationManager.cpp',
                 'BF2/AI/AIDLL/AI/Main/InformationManager.h','BF2/Core/SpHandlePool.h']
    (out/'whole_sources.json').write_text(json.dumps([
        dict(source=(CODE/relative).as_posix(),sha256=hashlib.sha256((CODE/relative).read_bytes()).hexdigest())
        for relative in whole_paths],indent=2),encoding='utf-8')
    lines = ['// [TEMP] Typed observers; unexpected virtual calls stop.',
             'namespace dice { namespace hfe { namespace ai {']
    specs = [
        ('Bot','AI/AIDLL/AI/AIBot/BotMain.h','ObservedBot',{'getSquadMember','getPlayerId','getPlayer','getBotId','getSpawnToStrategicAreaId'},'int identity=0;',
         'SquadMember* getSquadMember() override { return observeMember(); }\n'
         'int getPlayerId() const override { return observePlayerId(); }\n'
         'IAIPlayer* getPlayer() const override { return observePlayer(); }\n'
         'unsigned getBotId() const override { return observeBotId(identity); }\n'
         'int getSpawnToStrategicAreaId() override { return observeSpawnArea(identity); }'),
        ('IAIPlayer','AI/AIInterface/BFKnowledge/AIPlayer.h','ObservedPlayer',{'getSpawnKit'},
         'int identity=0;', 'int getSpawnKit() const override { return observeKit(identity); }'),
        ('AIStrategicArea','AI/AIDLL/AI/AIStrategicEntity/AIStrategicArea.h','ObservedObject',set(),'',
         'ObservedObject():AIStrategicArea("fixture",0,0.0f) {}\n'
         'const Vec2& getPosition(int side) const override { return observePosition(this,side); }'),
        ('IAIInformationEnvironment','AI/AIDLL/AI/AIKnowledge/BFInformationEnvironment.h','ObservedEnvironment',
         {'getObjectsWithinRectangle'},'int identity=0;',
         'void getObjectsWithinRectangle(std::vector<unsigned>& result,const Vec2& lower,const Vec2& upper,'
         'int side,bool world,IAIInformationGridPredicate* predicate) const override '
         '{ observeRectangle(identity,result,lower,upper,side,world,predicate); }'),
        ('Information','AI/AIDLL/AI/AIKnowledge/Information.h','ObservedInformation',{'getSide'},'',
         'ObservedInformation():Information(nullptr,0.0) {}\n'
         'int getSide() const override { return observeInformationSide(this); }'),
        ('IPIPhysical','AI/AIDLL/AI/AIKnowledge/IPIPhysical.h','ObservedPhysical',{'getStrengthType','getHealth'},
         'int identity=0;',
         'int getStrengthType() override { return observeStrength(identity); }\n'
         'float getHealth() const override { return observeHealth(identity); }'),
        ('IAIMain','AI/AIDLL/AI/Main/AIMain.h','ObservedMain',{'getSAI'},'int identity=0;',
         'SAI* getSAI(int side) override { return observeSAI(identity,side); }'),
        ('IBotManager','AI/AIDLL/AI/Main/IBotManager.h','ObservedBotManager',{'getBotFromId'},'int identity=0;',
         'Bot* getBotFromId(unsigned id) const override { return observeBotLookup(identity,id); }'),
        ('IKitTemplate','AI/AIInterface/BFKnowledge/KitTemplate.h','ObservedTemplate',{'getType'},
         'int identity=0;', 'KitType getType() const override { return observeType(identity); }'),
        ('IKitTemplateManager','AI/AIInterface/BFKnowledge/KitTemplateManager.h','ObservedManager',{'getAllTemplates'},
         'int identity=0;', 'const std::vector<IKitTemplate*>& getAllTemplates() const override { return observeTemplates(identity); }')]
    for base, relative, name, excluded, fields, custom in specs:
        lines.append('class '+name+' : public '+base+' { public: '+fields)
        for method, declaration, pure in _helpers.methods(base,relative):
            if method not in excluded:
                lines.append(declaration+' override { unexpected("'+base+'::'+method+'"); }')
        lines += [custom,'};']
    for base in ['AIStrategicObject','AIStrategicArea']:
        for method,declaration,pure in _helpers.methods(base,'AI/AIDLL/AI/AIStrategicEntity/'+base+'.h'):
            if not pure:
                signature=re.sub(r'\b'+method+r'\s*\(', base+'::'+method+'(',declaration,count=1)
                lines.append(signature+' { unexpected("'+base+'::'+method+'"); }')
    for method,declaration,pure in _helpers.methods('SAI','AI/AIDLL/AI/AIStrategic/SAI.h'):
        if method != 'getDeadBots':
            signature=re.sub(r'\b'+method+r'\s*\(', 'SAI::'+method+'(',declaration,count=1)
            lines.append(signature+' { unexpected("SAI::'+method+'"); }')
    lines.append('} } }')
    (out/'random_kit_mocks.inl').write_text('\n'.join(lines),encoding='utf-8')
    setup=[]
    for name,spec in SCORING_CASES.items():
        setup+=['if(mode=="'+name+'") {',
                'expectedObjectResult='+str(spec['result'] or 0)+';',
                'expectedFallback='+str(spec['fallback']).lower()+';',
                'expectedScoreDraws='+str(spec['scoring_draws'])+';',
                'scoreRaw='+str(spec['draw'])+'u;',
                'scoringTemplates.assign(13,nullptr);']
        setup += ['scoringTemplates['+str(i)+']=all['+str(i)+'];' for i in spec['available']]
        for i,(side,strength,kit,health) in enumerate(spec['records']):
            setup += ['records['+str(i)+'].side='+str(side)+'; records['+str(i)+'].strength='+str(strength)+'; '
                      'records['+str(i)+'].kit='+str(kit)+'; records['+str(i)+'].health=fromBits('+str(health)+'u); '
                      'scanHandles.push_back(handles['+str(i)+']);']
        for key,(first,second) in spec['planned'].items():
            setup+=['(*sourceMaps[0])['+str(key)+']=std::make_pair('+str(first)+','+str(second)+');']
        if spec.get('roster'): setup+=['squadA->m_squadMembers={rosterMembers[0],rosterMembers[1],rosterMembers[2]}; expectedRosterReads=3;']
        setup+=['}']
    (out/'scoring_setup.inl').write_text('\n'.join(setup),encoding='utf-8')
    (out/'scoring_cases.json').write_text(json.dumps(SCORING_CASES,indent=2),encoding='utf-8')
    fixture_paths=[Path(__file__).with_suffix(suffix) for suffix in ('.cpp','.h','.py','.cmd')]
    (out/'fixture_sources.json').write_text(json.dumps([
        dict(source=path.as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest()) for path in fixture_paths
    ],indent=2),encoding='utf-8')


MODES = ['first','middle','last','single','duplicates','excluded','reload','second_type',
         'live_end','manager_swap','refill','negative_index',
         'policy_missing_medic','policy_existing_medic','policy_fresh_member',
         'policy_manager_capture','policy_template_swap','policy_retained_squad','policy_retained_player',
         'policy_fallback_no_squad','policy_fallback_leader','policy_fallback_other_kit','policy_fallback_wide_kit',
         'policy_negative_count','policy_roster','policy_cached_count','policy_live_members',
         'policy_retained_roster_player','policy_fallback_templates','policy_retained_roster_squad',
         'policy_assigned_empty_stop','policy_assigned_roster_stop',
         'policy_object_no_squad_stop','policy_object_leader_stop','policy_object_medic',
         'policy_object_populated_stop','policy_object_reversed_stop','policy_object_rounding_stop',
         'policy_object_cached_centre_environment_stop','policy_object_alias_position_stop',
         'policy_object_side_one_stop','policy_object_nonfinite_stop',
         'policy_object_scan_invalid_stop','policy_object_scan_zero_manager_stop','policy_object_scan_unoccupied_stop',
         'policy_object_scan_enemy_types_stop','policy_object_scan_other_types_stop',
         'policy_object_scan_kit13_stop','policy_object_scan_health_values_stop',
         'policy_object_scan_retained_physical_stop','policy_object_scan_live_end_stop',
         'policy_object_scan_manager_swap_stop','policy_object_scan_main_swap_stop',
         'policy_object_scan_sai_capture_stop','policy_object_scan_map_populated_stop',
         'policy_object_map_copy_stop','policy_object_map_manager_stop','policy_object_map_dynamic_id_stop']
MODES+=list(SCORING_CASES)


def run(exe):
    summary = []
    for mode in MODES:
        case_name=mode[:-5] if mode.startswith('policy_object_') and mode.endswith('_stop') else mode
        spec=SCORING_CASES.get(mode)
        p = subprocess.run([str(exe),case_name],capture_output=True,text=True,timeout=15)
        output = p.stdout+p.stderr
        (exe.parent/(case_name+'.log')).write_text(output,encoding='utf-8')
        if mode.endswith('_stop') or spec:
            events = ['member1','templates0','type0:1','type0:2','templates0','player_id','lookupA']
            no_squad=mode=='policy_object_no_squad_stop' or (spec and spec.get('no_squad'))
            if not no_squad:
                events+=['member2','leader2']
            if mode in ('policy_assigned_empty_stop','policy_assigned_roster_stop'):
                events+=['count']
                if mode=='policy_assigned_roster_stop':
                    for index in range(3):
                        events+=['index'+str(index),'roster_player'+str(index),'kit'+str(10+index)]
                events+=['assigned']
            if mode.startswith('policy_object_') or spec:
                environment=1 if mode=='policy_object_cached_centre_environment_stop' else 0
                size=2 if mode in ('policy_object_populated_stop','policy_object_scan_zero_manager_stop',
                                  'policy_object_scan_manager_swap_stop') else 0
                if mode=='policy_object_scan_invalid_stop': size=4
                if mode in ('policy_object_scan_enemy_types_stop','policy_object_scan_other_types_stop'): size=8
                if mode=='policy_object_scan_health_values_stop': size=5
                if mode in ('policy_object_scan_unoccupied_stop','policy_object_scan_kit13_stop',
                            'policy_object_scan_retained_physical_stop','policy_object_scan_main_swap_stop',
                            'policy_object_scan_live_end_stop'): size=1
                if spec: size=len(spec['records'])
                events+=['position','rectangle'+str(environment)+':size'+str(size)]
                ids=[]
                if mode=='policy_object_scan_unoccupied_stop': events+=['occupied0']
                if mode in ('policy_object_scan_enemy_types_stop','policy_object_scan_other_types_stop'): ids=list(range(8))
                if mode=='policy_object_scan_health_values_stop': ids=list(range(5))
                if mode in ('policy_object_scan_kit13_stop','policy_object_scan_retained_physical_stop',
                            'policy_object_scan_main_swap_stop'): ids=[0]
                if mode=='policy_object_scan_live_end_stop': ids=[0,1]
                if mode=='policy_object_scan_manager_swap_stop': ids=[0,2]
                if spec: ids=list(range(len(spec['records'])))
                for i in ids:
                    events+=['occupied'+str(i),'side'+str(i),'strength'+str(i)]
                    infantry=mode in ('policy_object_scan_kit13_stop','policy_object_scan_retained_physical_stop',
                                      'policy_object_scan_main_swap_stop') or (mode=='policy_object_scan_other_types_stop' and i==0)
                    if spec: infantry=spec['records'][i][0]!=1 and spec['records'][i][1]==0
                    if infantry: events+=['informationkit'+str(i)]
                    vehicle_health=spec and spec['records'][i][0]!=1 and spec['records'][i][1] in (1,2,4)
                    if infantry or vehicle_health or mode=='policy_object_scan_health_values_stop' or (mode=='policy_object_scan_other_types_stop' and i in (1,2,3)):
                        events+=['health'+str(i)]
                map_size=6 if mode=='policy_object_scan_map_populated_stop' else (2 if mode.startswith('policy_object_map_') else 0)
                if mode=='policy_object_map_manager_stop': map_size=3
                if spec: map_size=len(spec['planned'])
                events+=['sai'+('1' if mode=='policy_object_scan_main_swap_stop' else '0'),'dead0:size'+str(map_size)]
                if mode=='policy_object_scan_map_populated_stop':
                    events+=['botid1','botlookup0:4294967295','spawnarea1','botid2',
                             'botid3','botlookup0:20','spawnarea1','botid4','botlookup0:30',
                             'botid5','botlookup0:40','spawnarea2','botid6','botlookup0:2147483647','spawnarea1']
                if mode=='policy_object_map_copy_stop': events+=['botid1','botid2','botlookup0:20','spawnarea1']
                if mode=='policy_object_map_manager_stop': events+=['botid1','botid2','botlookup1:20','spawnarea1','botid3','botlookup0:30']
                if mode=='policy_object_map_dynamic_id_stop': events+=['botid1','botid2']
                if spec:
                    for i,key in enumerate(sorted(spec['planned'])):
                        events+=['botid'+str(i+1)]
                        if key!=10:
                            events+=['botlookup0:'+str(key & 0xffffffff)]
                            if key!=30: events+=['spawnarea'+('2' if key==40 else '1')]
                    if 'refill' in spec: events+=['refill']
                fallback=spec['fallback'] if spec else mode not in ('policy_object_scan_enemy_types_stop','policy_object_scan_health_values_stop')
                if fallback:
                    if not no_squad:
                        events+=['count']
                        if spec and spec.get('roster'):
                            for i in range(3): events+=['index'+str(i),'roster_player'+str(i),'kit'+str(10+i)]
                        events+=['assigned']
                    if not (spec and spec.get('assigned')):
                        events+=['member2' if no_squad else 'member3','templates0','type1:1','type1:2','type2:1','type2:2']
            expected = ''.join('TRACE '+event+'\n' for event in events)
            stops=mode.startswith('policy_assigned_') or (spec and spec.get('assigned'))
            if stops:
                expected+='[FRONTIER] dice::hfe::ai::BotSpawner::findSuitableKit [assigned-squad equipment] reached\n'
            else:
                expected+='PASS '+case_name+'\n'
            if p.returncode != (3 if stops else 0) or output != expected:
                raise RuntimeError((mode,p.returncode,output))
        elif p.returncode != 0 or output != 'PASS '+case_name+'\n':
            raise RuntimeError((mode,p.returncode,output))
        summary.append('PASS '+case_name)
        print(summary[-1])
    summary.append('PASS random kit helper and medic/roster/equipment policy: '+str(len(MODES))+' strict x86 cases')
    (exe.parent/'runtime.log').write_text('\n'.join(summary)+'\n',encoding='utf-8')
    print(summary[-1])


if __name__ == '__main__':
    if len(sys.argv)!=3 or sys.argv[1] not in ('generate','run'):
        raise SystemExit('Usage: random_kit_runtime.py generate OUTPUT | run EXE')
    p=Path(sys.argv[2]).resolve()
    generate(p) if sys.argv[1]=='generate' else run(p)
