"""[TEMP] Strict x86 concrete-area caller with real helpers and typed collaborators."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import subprocess
import sys

CODE=Path(__file__).resolve().parents[2]
AI='AI/AIDLL/AI/'
spec=importlib.util.spec_from_file_location('declarations',Path(__file__).with_name('sai_states_runtime.py'))
helper=importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
MODES='cp_owned cp_hostile cp_neutral cp_lost_neutral cp_takeover cp_takeover_equal cp_takeover_nan owner_changes_id free_empty defence side_two curve_half curve_high geometry_rounding cached_dependencies invalid no_unit unoccupied neither friend flagged_friend unit_reload enemy flagged_enemy battle_pointer_capture repeated_side secondary_enemy secondary_flagged_friend wide_sum live_end queries manager_carry'.split()
MODES += 'helper_zero helper_index mask_one mask_two mask_wrap free_friend free_enemy free_flagged_friend free_flagged_enemy free_friend_disabled free_enemy_disabled free_secondary_friend free_secondary_enemy free_contested fractional_need negative_ratio nan_ratio weighted_product'.split()
MODES += 'counter_non_control counter_control counter_fresh'.split()

def generate(out):
    specs=[
        (AI+'AIStrategicEntity/AIStrategicArea.cpp',r'void AIStrategicArea::update\([^\n]+\)'),
        (AI+'AIStrategicEntity/AIStrategicArea.cpp',r'float AIStrategicArea::getControlPointTakeOverCounter\(\)'),
        (AI+'AIStrategic/SAI.cpp',r'EnemyStatistics::EnemyStatistics\(unsigned int size\)'),
        (AI+'AIStrategic/SAI.cpp',r'EnemyStatistics::~EnemyStatistics\(\)'),
        (AI+'AIStrategic/SAI.cpp',r'void EnemyStatistics::sample\([^\n]+\)'),
        (AI+'AIStrategic/SAI.cpp',r'SampleHelper::SampleHelper\([^\n]+\)'),
        (AI+'AIStrategic/SAI.cpp',r'void SampleHelper::clear\([^\n]+\)'),
        (AI+'AIStrategic/SAI.cpp',r'void SampleHelper::addSample\([^\n]+\)'),
        (AI+'AIStrategic/SAI.cpp',r'bool SAI::isFriend\(int side\) const'),
        (AI+'AIStrategic/SAI.cpp',r'bool SAI::isEnemy\(int side\) const')]
    out.mkdir(parents=True,exist_ok=True)
    evidence=[]
    for relative,signature in specs:
        path=CODE/'BF2'/relative
        source=path.read_text(encoding='utf-8')
        matches=re.findall('^'+signature+r'\s*(?::[\s\S]*?)?\{[\s\S]*?^}',source,re.M)
        if len(matches)!=1 or 'BF2_AI_FRONTIER' in matches[0]:
            raise RuntimeError('Expected actual body: '+signature)
        body=matches[0]
        evidence.append(dict(source=path.as_posix(),source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                             body_sha256=hashlib.sha256(body.encode()).hexdigest(),body=body))
    (out/'actual.cpp').write_text('// [TEMP] Exact complete production definitions.\n#include "'
        +Path(__file__).with_suffix('.h').as_posix()+'"\n#include <cmath>\nnamespace dice { namespace hfe { namespace ai {\n'
        +'\n\n'.join(e['body'] for e in evidence)+'\n} } }\n',encoding='utf-8')
    (out/'source.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')
    lines=['// [TEMP] Chosen typed observers; unused virtuals abort.','namespace dice { namespace hfe { namespace ai {']
    classes=[
        ('Information',AI+'AIKnowledge/Information.h','ObservedInformation',{'getSecurity','getSide'},
         'int identity; ObservedInformation() : Information(nullptr,0.0), identity(constructing) {}',
         'float getSecurity() override { return security(identity); }\n'
         'float getSecurity(double) override { unexpected("timed security"); }\n'
         'int getSide() const override { return informationSide(identity); }'),
        ('IPIPhysical',AI+'AIKnowledge/IPIPhysical.h','ObservedPhysical',{'getStrengthType'},'int identity = 0;',
         'int getStrengthType() override { return category(identity); }'),
        ('ISAI',AI+'AIStrategic/SAI.h','ObservedSAI',{'isFriend','isEnemy'},'',
         'bool isFriend(int side) const override { return relation(side,true); }\n'
         'bool isEnemy(int side) const override { return relation(side,false); }'),
        ('IAIGame','AI/AIInterface/BFKnowledge/BFGame.h','ObservedGame',{'getOwnerOfControlPoint','getControlPointTakeOverCounter'},'int identity = 0;',
         'int getOwnerOfControlPoint(unsigned int id) const override { return owner(identity,id); }\n'
         'float getControlPointTakeOverCounter(unsigned int id) const override { return gameCounter(identity,id); }'),
        ('IAIInformationEnvironment',AI+'AIKnowledge/BFInformationEnvironment.h','ObservedEnvironment',{'getObjectsWithinRectangle'},'int identity = 0;',
         'void getObjectsWithinRectangle(std::vector<unsigned int>& out, const Vec2& first, const Vec2& last, int side, bool world, IAIInformationGridPredicate* predicate) const override { query(identity,out,first,last,side,world,predicate); }'),
        ('AIStrategicArea',AI+'AIStrategicEntity/AIStrategicArea.h','ObservedArea',{'update','getControlPointTakeOverCounter','hasObjectTypeFlag'},
         'ObservedArea() : AIStrategicArea("fixture", unsigned(-1), 1.0f) {}',
         'float getControlPointTakeOverCounter() override { return takeover(); }\n'
         'bool hasObjectTypeFlag(StrategicObjectType type) const override { return objectFlag(type); }')]
    for base,relative,name,excluded,fields,custom in classes:
        lines.append('class '+name+' : public '+base+' { public: '+fields)
        for method,decl in helper.methods(base,relative):
            if method in excluded:
                # Only the observed rectangle overload is retained; other overloads still abort.
                if base!='IAIInformationEnvironment' or 'const Vec2&' in decl and decl.count('const Vec2&')==2:
                    continue
            lines.append(decl+' override { unexpected("'+base+'::'+method+'"); }')
        lines += [custom,'};']
    for base,relative in [('SAI',AI+'AIStrategic/SAI.h'),('AIStrategicArea',AI+'AIStrategicEntity/AIStrategicArea.h'),
                          ('AIStrategicObject',AI+'AIStrategicEntity/AIStrategicObject.h')]:
        source=re.sub(r'//[^\n]*','',(CODE/'BF2'/relative).read_text(encoding='utf-8'))
        for method,decl in helper.methods(base,relative):
            if base=='AIStrategicArea' and method in ('update','getControlPointTakeOverCounter'): continue
            if base=='SAI' and method in ('isFriend','isEnemy'): continue
            original=re.search(r'virtual\s+[^;]*\b'+method+r'\s*\([^;]*;',source)
            if base=='AIStrategicObject' and original and re.search(r'=\s*0\s*;',original.group(0)): continue
            signature=re.sub(r'\b'+method+r'(?=\s*\()',base+'::'+method,decl,count=1)
            lines.append(signature+' { unexpected("'+base+'::'+method+'"); }')
    lines.append('} } }')
    (out/'mocks.inl').write_text('\n'.join(lines),encoding='utf-8')
    whole=[AI+'AIUtils/CustomCurve.cpp',AI+'AIUtils/SCurve.cpp',AI+'AIUtils/SCurve.h',AI+'AIUtils/CustomCurve.h',
           AI+'Main/InformationManager.cpp',AI+'Main/InformationManager.h','Core/SpHandlePool.h']
    (out/'whole_sources.json').write_text(json.dumps({p:hashlib.sha256((CODE/'BF2'/p).read_bytes()).hexdigest() for p in whole},indent=2))
    (out/'fixture_sources.json').write_text(json.dumps({Path(__file__).with_suffix(s).name:hashlib.sha256(Path(__file__).with_suffix(s).read_bytes()).hexdigest() for s in ('.cpp','.h','.py','.cmd')},indent=2))
    (out/'cases.json').write_text(json.dumps(MODES,indent=2))

def run(exe):
    modes=json.loads((exe.parent/'cases.json').read_text())
    summary=[]
    for mode in modes:
        result=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=15)
        output=result.stdout+result.stderr
        (exe.parent/(mode+'.log')).write_text(output,encoding='utf-8')
        if result.returncode!=0 or output!='PASS '+mode+'\n': raise RuntimeError((mode,result.returncode,output))
        summary.append('PASS '+mode); print(summary[-1])
    summary.append('PASS concrete area update: '+str(len(modes))+' strict x86 cases')
    (exe.parent/'runtime.log').write_text('\n'.join(summary)+'\n',encoding='utf-8'); print(summary[-1])

if __name__=='__main__':
    if len(sys.argv)!=3 or sys.argv[1] not in ('generate','run'): raise SystemExit('generate OUT | run EXE')
    path=Path(sys.argv[2]).resolve()
    generate(path) if sys.argv[1]=='generate' else run(path)
