"""[TEMP] Exact maneuver definitions; native scopes; explicit dispatcher-only observer variant."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import subprocess
import sys

CODE=Path(__file__).resolve().parents[2]
AI='AI/AIDLL/AI/'
spec=importlib.util.spec_from_file_location('native_methods',Path(__file__).with_name('spawn_metadata_runtime.py'))
helpers=importlib.util.module_from_spec(spec);spec.loader.exec_module(helpers)
NORMAL=['empty','human','dead','nonfiring','flag10','flag20','flag80','other_flag',
        'invalid_position','one','two','continue_after_two','range_equal','range_below','range_above','range_nan',
        'spill_x','spill_y','spill_z','sum_extended','cached_count','relocated_vector','retained_member','captured_bot',
        'manager_swap','lowword_zero','stale_nonfiring','lookup_before_fire','fresh_position',
        'mission_full_width','mission_after_scan','mission_stop','alternate_stop',
        'dispatch_empty','dispatch_humans','dispatch_mixed','dispatch_duplicate',
        'dispatch_append','dispatch_clear','dispatch_replace','fire_leaf']
OBSERVERS=['observe_zero','observe_one','observe_two','observe_negative','observe_large',
           'observe_retained_member','observe_fresh_bot']
STOPS={'mission_stop':'SquadCoordinator::chooseSquadManeuver (terrain altitude and flank policy)',
       'mission_after_scan':'SquadCoordinator::chooseSquadManeuver (terrain altitude and flank policy)',
       'alternate_stop':'SquadCoordinator::updateSquadManeuver (squadManeuverAlternateFire)',
       'observe_one':'SquadCoordinator::updateSquadManeuver (squadManeuverAlternateFire)',
       'observe_two':'SquadCoordinator::updateSquadManeuver (squadManeuverStaticFlank)'}

def generate(out):
    out.mkdir(parents=True,exist_ok=True)
    specs=[('SquadCoordinator',n) for n in ('maneuverMemberWithinRange','chooseSquadManeuver','updateSquadManeuver')]
    specs += [('SquadMember',n) for n in ('getBot','isBot','getPlayer','getPosition')]
    specs += [('BotMain',n) for n in ('setAllowFire','isFireAllowed')]
    records=[]
    for owner,name in specs:
        path=CODE/'BF2'/AI/'AIBot'/(owner+'.cpp');source=path.read_text(encoding='utf-8')
        pattern=(r'^static bool '+name if name=='maneuverMemberWithinRange' else r'^[^\n{};]+ '+owner+'::'+name)
        pattern+=r'\([^\n]*\)\s*\{[\s\S]*?^}'
        if name in ('isBot','getPlayer'):pattern=r'^[^\n{};]+ '+owner+'::'+name+r'\([^\n]*\) \{[^\n]*\}'
        found=re.findall(pattern,source,re.M)
        if len(found)!=1:raise RuntimeError(('unique exact body',owner,name,len(found)))
        records.append(dict(owner=owner,method=name,source=path.as_posix(),source_sha256=hashlib.sha256(source.encode()).hexdigest(),body=found[0],body_sha256=hashlib.sha256(found[0].encode()).hexdigest()))
    actual=['// [TEMP] Untouched body extracts; tested definitions excluded from observers.',
            '#include "'+Path(__file__).with_suffix('.h').as_posix()+'"',
            'namespace dice { namespace hfe { namespace ai {']
    for r in records:
        if r['method'] in ('chooseSquadManeuver','isBot','getBot'):
            actual+=['#if !defined(DISPATCH_OBSERVERS)',r['body'],'#endif']
        else:actual.append(r['body'])
    actual.append('} } }');(out/'actual.cpp').write_text('\n\n'.join(actual),encoding='utf-8')
    (out/'source.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
    mocks=['// [TEMP] Only selected methods observe; unused virtuals terminate.',
           'namespace dice { namespace hfe { namespace ai {']
    for owner in ('Bot','BotMain'):
        for name,decl,pure in helpers.methods(owner,AI+'AIBot/BotMain.h'):
            if pure or (owner=='BotMain' and name in ('setAllowFire','isFireAllowed')):continue
            signature=re.sub(r'\b'+name+r'(?=\s*\()',owner+'::'+name,decl,count=1)
            mocks.append(signature+' { unexpected("'+owner+'::'+name+'"); }')
    specs=[('Bot',AI+'AIBot/BotMain.h','ObservedBot',{
        'getCurrentEquipment':'return observeBot(identity,0);','isFiring':'return observeBot(identity,1)!=0;',
        'setAllowFire':'observeAllow(identity,b);'},''),
        ('IAIPlayer','AI/AIInterface/BFKnowledge/AIPlayer.h','ObservedPlayer',{'getIsAlive':'return observeAlive(identity);'},''),
        ('Information',AI+'AIKnowledge/Information.h','ObservedInformation',{},'ObservedInformation():Information(nullptr,0) {}')]
    for base,header,owner,bodies,ctor in specs:
        mocks+=['class '+owner+' : public '+base+' { public: int identity=0;',ctor]
        for name,decl,pure in helpers.methods(base,header):
            mocks.append(decl+' override { '+bodies.get(name,'unexpected("'+base+'::'+name+'");')+' }')
        mocks.append('};')
    mocks.append('} } }');(out/'mocks.inl').write_text('\n'.join(mocks),encoding='utf-8')
    paths=[Path(__file__).with_suffix(s) for s in ('.h','.cpp','.py','.cmd')]+[Path(__file__).with_name('spawn_metadata_runtime.py')]
    (out/'fixture_sources.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},indent=2),encoding='utf-8')
    paths=[CODE/'BF2'/p for p in (AI+'Main/InformationManager.cpp',AI+'Main/InformationManager.h','Core/SpHandlePool.h',AI+'AIKnowledge/Information.h',AI+'AIBot/SquadCoordinator.h',AI+'AIBot/SquadMember.h',AI+'AIBot/BotMain.h','AI/AIInterface/BFKnowledge/AIPlayer.h')]
    (out/'whole_sources.json').write_text(json.dumps({p.as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},indent=2),encoding='utf-8')

def run(out):
    results=[];checks=0;stops=0
    for observer,modes in [(False,NORMAL),(True,OBSERVERS)]:
        for mode in modes:
            exe=out/('dispatcher_observers.exe' if observer else 'squad_maneuver_runtime.exe')
            proc=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=15)
            output=proc.stdout+proc.stderr;(out/(mode+'.log')).write_text(output,encoding='utf-8')
            if mode in STOPS:
                expected='VERIFIED '+mode+'\n[FRONTIER] dice::hfe::ai::'+STOPS[mode]+' reached\n'
                if proc.returncode!=3 or output!=expected:raise RuntimeError((mode,proc.returncode,output))
                result='PASS '+mode+' exact checked trace/diagnostic/abort 3';stops+=1
            else:
                match=re.fullmatch(r'PASS '+mode+r' ([1-9][0-9]*) checks\n',output)
                if proc.returncode!=0 or not match:raise RuntimeError((mode,proc.returncode,output))
                checks+=int(match.group(1));result='PASS '+mode+' '+match.group(1)+' checks'
            results.append(result);print(result)
    results.append(f'PASS squad maneuver: {len(results)} strict x86 cases, {checks} normal checks, {stops} exact abort traces')
    (out/'runtime.log').write_text('\n'.join(results)+'\n',encoding='utf-8');print(results[-1])

if __name__=='__main__':
    if len(sys.argv)!=3 or sys.argv[1] not in ('generate','run'):raise SystemExit('generate OUT | run OUT')
    out=Path(sys.argv[2]).resolve();generate(out) if sys.argv[1]=='generate' else run(out)
