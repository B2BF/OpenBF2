"""[TEMP] Strict real-navmesh query fixture; no production edits.
The shared navmesh_loader fixture supplies only File/game/debug collaborators.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import struct
import zipfile
CODE = Path(__file__).resolve().parents[2]
BF2 = CODE / 'BF2'
AI = 'AI/AIDLL/AI/'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def decls(class_name, indent=''):
    path = BF2 / AI / 'Main/AIPathfinding.h'
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(encoding='utf-8'), flags=re.S)
    found = re.findall(r'^'+indent+r'class '+class_name+r'[^\{]*\{(.*?)^'+indent+r'};', source, re.M|re.S)
    if len(found) != 1: raise RuntimeError(('class',class_name,len(found)))
    body = re.sub(r'^    class \w+[^\{]*\{.*?^    };','',found[0],flags=re.M|re.S)
    for sig in re.findall(r'virtual\s+([^;{}]+);',body):
        sig=' '.join(sig.split())
        name=re.search(r'(~?\w+)\s*\(',sig).group(1)
        if not name.startswith('~'): yield name,sig
def prepare_data(out):
    data=out/'data'/'AIPathFinding'; data.mkdir(parents=True,exist_ok=True)
    def node(children=(0,0,0,0), leaf=1, count=1, first=0):
        return struct.pack('<4IBBHI',*children,leaf,count,0,first)
    def write(name, polygons, nodes=None, order=None, bounds=(-4,-4,4,4), size=8):
        # Horizontal XZ quads, counter-clockwise as viewed in XZ; normal is stored independently.
        verts=[]; ps=[]; indices=[]
        for spec in polygons:
            x0,z0,x1,z1,y,material,ny=spec
            first=len(indices); v=len(verts)
            verts += [(x0,y,z0),(x0,y,z1),(x1,y,z1),(x1,y,z0)]
            indices += [v,v+1,v+2,v+3]
            ps.append(struct.pack('<BBHIfff',4,material,0,first,0,ny,0))
        if order is None: order=list(range(len(polygons)))
        if nodes is None: nodes=[node(count=len(order))]
        arrays=[b''.join(nodes), b''.join(ps),struct.pack('<'+'I'*len(order),*order),
                b''.join(struct.pack('<fff',*v) for v in verts),
                struct.pack('<'+'I'*len(indices),*indices),struct.pack('<'+'I'*len(indices),*([0xffffffff]*len(indices)))]
        sizes=[24,20,4,12,4,4]
        raw=struct.pack('<iifffff',1,3,*bounds,size)
        for a,s in zip(arrays,sizes): raw+=struct.pack('<i',len(a)//s)+a
        (data/(name+'.qtr')).write_bytes(raw)
        (data/(name+'.clb')).write_bytes(struct.pack('<5i',5,0,0,0,0))
    square=(-3,-3,3,3,0,1,1)
    write('one',[square])
    write('empty',[],nodes=[])
    write('empty_leaf',[])
    write('blocked',[(-3,-3,3,3,0,0,1)])
    write('downward',[(-3,-3,3,3,0,1,-1)])
    write('zero_normal',[(-3,-3,3,3,0,1,0)])
    write('edge',[(-4,-4,4,4,0,1,1)])
    write('closest',[(-3,-3,3,3,-1,1,1),square,(-3,-3,3,3,0.5,1,1)])
    write('tie',[square,square],order=[1,0])
    write('filter',[(-3,-3,3,3,0,0,1),(-3,-3,3,3,0,1,-1),(-3,-3,3,3,-0.5,1,1)])
    write('sample_target',[(4.5,-1,5.5,1,0,1,1)],bounds=(0,-2,10,2),size=10)
    quads=[(-4,-4,0,0,0,1,1),(-4,0,0,4,0,1,1),(0,-4,4,0,0,1,1),(0,0,4,4,0,1,1)]
    write('quadrants',quads,nodes=[node((1,2,3,4),0,0),*[node(first=i) for i in range(4)]])
    write('missing_child',quads,nodes=[node((1,0,3,4),0,0),*[node(first=i) for i in range(4)]])
    write('limit',[(-3,-3,3,3,-10000,1,1)])
    # Copy stock inputs without modifying the reference installation.
    stock=Path(r'D:/B2BF Launcher/Battlefield2/mods/bf2/Levels/Dalian_plant/server.zip')
    with zipfile.ZipFile(stock) as archive:
        for ext in ('qtr','clb'):
            name=next(n for n in archive.namelist() if n.lower()=='aipathfinding/infantry.'+ext)
            (data/('stock.'+ext)).write_bytes(archive.read(name))
    raw=(data/'stock.qtr').read_bytes(); offset=28; arrays=[]
    for size in (24,20,4,12,4,4):
        count=struct.unpack_from('<I',raw,offset)[0]; offset+=4
        arrays.append(raw[offset:offset+size*count]); offset+=size*count
    if offset!=len(raw): raise RuntimeError('stock parser did not consume qtr')
    vertices=list(struct.iter_unpack('<fff',arrays[3])); indices=list(struct.iter_unpack('<I',arrays[4]))
    samples=[]
    for count,material,cluster,first,nx,ny,nz in struct.iter_unpack('<BBHIfff',arrays[1]):
        if count<3 or material==0 or ny<0.2: continue
        points=[vertices[indices[first+i][0]] for i in range(count)]
        samples.append(tuple(sum(p[j] for p in points)/count for j in range(3)))
        if len(samples)==32: break
    if len(samples)!=32: raise RuntimeError('stock samples')
    (out/'stock_samples.inl').write_text('const Vec3 samples[]={\n'+',\n'.join(
        '{'+','.join(format(v,'.17e')+'f' for v in point)+'}' for point in samples)+'\n};\n')
    cases=['empty_manager','empty_tree','empty_leaf','leaf_success','bounds','materials',
           'normal_filter','height_above','height_below','closest','tie','filtered_nearest',
           'quadrants','quadrant_boundary','missing_child','island_iteration','first_island','known_reuse',
           'alias_outputs','outer_invalid','outer_routing','outer_reload','closest_valid_polygon',
           'plane','distance_limit','stock']
    (out/'cases.json').write_text(json.dumps(cases,indent=2))
def generate(out):
    out.mkdir(parents=True,exist_ok=True)
    prepare_data(out)
    path=BF2/AI/'Main/AIPathfinding.cpp'
    source=path.read_text(encoding='utf-8')
    patterns=[
        r'^AIPathfinding::MapContainer::MapContainer\([^\n]+\)[\s\S]*?^}',
        r'^AIPathfinding::MapContainer::~MapContainer\(\)\s*\{[\s\S]*?^}',
        r'^bool AIPathfinding::MapContainer::getPolygon\(const Vec3&[^,]*,\s*unsigned int&[^,]*,\s*unsigned int&[^,]*,\s*MaterialManager\s*\*[^,)]*\) const\s*\{[\s\S]*?^}',
        r'^bool AIPathfinding::MapContainer::findClosestValidPolygon\([\s\S]*?^}',
        r'^bool AIPathfinding::getPolygon\(const Vec3&[^,]*,\s*int[^,]*,\s*unsigned int&[^,]*,\s*unsigned int&[^,]*,\s*int[^,)]*\) const\s*\{[\s\S]*?^}',
        r'^int AIPathfinding::getNumberOfMaps\(\) const\s*\{[\s\S]*?^}']
    records=[]
    for pattern in patterns:
        found=re.findall(pattern,source,re.M)
        if len(found)!=1: raise RuntimeError(('exact extraction',pattern,len(found)))
        body=found[0]
        records.append(dict(source=path.as_posix(),source_sha256=sha(path),body=body,
                            body_sha256=hashlib.sha256(body.encode()).hexdigest()))
    (out/'actual.cpp').write_text('// [TEMP] Exact production extracts; no substitutions.\n#include "'
        +Path(__file__).with_suffix('.h').as_posix()+'"\nnamespace dice { namespace hfe { namespace ai {\n'
        +'\n\n'.join(r['body'] for r in records)+'\n} } }\n',encoding='utf-8')
    (out/'source.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
    mocks=['// [TEMP] Unexercised setup/outer-query virtuals abort.','namespace dice { namespace hfe { namespace ai {']
    for owner,name,indent in [('AIPathfinding','AIPathfinding',''),('AIPathfinding::MapContainer','MapContainer','    ')]:
        for method,sig in decls(name,indent):
            if owner=='AIPathfinding' and (method=='getNumberOfMaps' or
               sig=='bool getPolygon(const Vec3&, int, unsigned int&, unsigned int&, int) const'):
                continue
            if owner=='AIPathfinding::MapContainer' and method=='findClosestValidPolygon':
                continue
            definition=re.sub(r'\b'+method+r'(?=\s*\()',owner+'::'+method,sig,count=1)
            mocks.append(definition+' { unexpected("unused '+owner+'::'+method+'"); }')
    mocks.append('} } }')
    (out/'unused.inl').write_text('\n'.join(mocks),encoding='utf-8')
    settings=(BF2/AI/'Main/AISettingsInterfaces.h').read_text(encoding='utf-8')
    settings=re.sub(r'/\*.*?\*/|//[^\n]*','',settings,flags=re.S)
    body=re.search(r'^class IAISettings\s*\{(.*?)^};',settings,re.M|re.S).group(1)
    observer=['// [TEMP] Typed settings observer; only the material-table lookup is exercised.',
              'class QuerySettings : public dice::hfe::ai::IAISettings { public:',
              'mutable int calls=0; mutable int vehicle=-999; MaterialManager* result=nullptr;',
              'AIPathfinding::MapContainer* replacement=nullptr; AIPathfinding* path=nullptr;',
              'MaterialManager* getVehicleMaterialManager(int id) const override;']
    for sig in re.findall(r'virtual\s+([^;{}]+);',body):
        sig=re.sub(r'\s*=\s*0$','',' '.join(sig.split()))
        method=re.search(r'(~?\w+)\s*\(',sig).group(1)
        if method.startswith('~') or method=='getVehicleMaterialManager': continue
        observer.append(sig+' override { unexpected("unused settings '+method+'"); }')
    observer.append('};')
    (out/'settings.inl').write_text('\n'.join(observer),encoding='utf-8')
    whole=[AI+'AIPathFinding/NavMesh/'+n+'.cpp' for n in ('QuadTreeManager','QuadTree','MapProperties')]
    node=AI+'AIPathFinding/NavMesh/QuadTreeNode.cpp'
    if (BF2/node).exists(): whole.append(node)
    whole += [AI+'AIPathFinding/Cluster/ClusterGraph.cpp',AI+'AIPathFinding/Material/MaterialManager.cpp',
              AI+'Main/IAIPathfinding.cpp',AI+'Main/IAIPathfindingSetup.cpp','Math/Mat4Inverse.cpp','Math/Plane3.cpp','Math/Vec3.cpp',
              'Core/Debug.cpp','Core/StringUtil.cpp']
    headers=[AI+'Main/AIPathfinding.h',AI+'Main/IAIPathfinding.h',AI+'Main/AISettingsInterfaces.h',
             AI+'AIPathFinding/NavMesh/QuadTree.h',AI+'AIPathFinding/NavMesh/QuadTreeNode.h',
             AI+'AIPathFinding/NavMesh/QuadTreeManager.h',AI+'AIPathFinding/NavMesh/MapProperties.h',
             AI+'AIPathFinding/Material/MaterialManager.h','Math/Mat4.h','Math/Plane3.h']
    (out/'whole_sources.json').write_text(json.dumps({p:sha(BF2/p) for p in whole},indent=2))
    (out/'header_context.json').write_text(json.dumps({p:sha(BF2/p) for p in headers},indent=2))
    fixtures=[Path(__file__).with_suffix(s) for s in ('.h','.cpp','.py','.cmd')]
    fixtures.append(Path(__file__).with_name('navmesh_loader_runtime.cpp'))
    (out/'fixture_sources.json').write_text(json.dumps({p.name:sha(p) for p in fixtures},indent=2))
def run(exe,out):
    cases=json.loads((out/'cases.json').read_text())
    summary=[];checks=0
    for mode in cases:
        result=subprocess.run([str(exe),mode,str(out)],capture_output=True,text=True,timeout=30)
        output=result.stdout+result.stderr
        (out/(mode+'.log')).write_text(output,encoding='utf-8')
        match=re.search(r'^PASS '+re.escape(mode)+r' ([1-9][0-9]*) checks\n\Z',output,re.M)
        if result.returncode!=0 or not match or 'FAIL' in output or 'UNEXPECTED' in output:
            raise RuntimeError((mode,result.returncode,output))
        checks+=int(match.group(1));summary.append(match.group(0).strip());print(summary[-1])
    summary.append(f'PASS map polygon: {len(cases)} strict x86 cases / {checks} checks')
    (out/'runtime.log').write_text('\n'.join(summary)+'\n',encoding='utf-8');print(summary[-1])
if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='generate': generate(Path(sys.argv[2]).resolve())
    elif len(sys.argv)==4 and sys.argv[1]=='run': run(Path(sys.argv[2]).resolve(),Path(sys.argv[3]).resolve())
    else: raise SystemExit('generate OUT | run EXE OUT')
