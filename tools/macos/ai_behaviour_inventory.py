#!/usr/bin/env python3
"""Inventory of the behaviour / action-plan system in the Linux libaidll.so (1.5.3153, ia-32).

Reads the shipped ELF (symbols are present) and writes, per function: address, size,
instruction count, binding, class, translation unit (from the FILE symbols and the link
order), family and proposed package. Also prints per-class aggregates (vtable, ctor/dtor,
operator-new size, base class from the typeinfo) and a reachability pass (rapid type
analysis over direct calls, constructor calls and vtables of constructed classes).

Usage: ai_behaviour_inventory.py <libaidll.so> <workdir> <functions.csv> [classes.csv]
<workdir> caches `objdump -t/-d/-R` output (created on first run; use out/re).
Env: TREE=Code/BF2 adds the tree-status column; EXT=1 lists outside calls of the minimum set;
VT=ClassA,ClassB dumps vtables slot by slot. See docs/macos-ai-behaviour-plan.md.
Planning tool only: no engine code depends on it.
"""
import bisect, collections, csv, os, re, struct, subprocess, sys

LIB, WD, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
OUTC = sys.argv[4] if len(sys.argv) > 4 else None
os.makedirs(WD, exist_ok=True)

def cached(name, args):
    p = os.path.join(WD, name)
    if not os.path.exists(p):
        with open(p, 'w') as f:
            subprocess.run(args, stdout=f, check=True)
    return p

symtab = cached('symtab.txt', ['objdump', '-t', LIB])
dis = cached('dis.txt', ['objdump', '-d', '--no-show-raw-insn', LIB])
rels = cached('relocs.txt', ['objdump', '-R', LIB])

# ---------------------------------------------------------------- symbols and TUs
SYM = re.compile(r'^([0-9a-f]{8}) (.{7}) (\S+)\t([0-9a-f]{8}) (.*)$')
funcs = {}        # addr -> dict
objs = {}         # mangled -> (addr, size)
byaddr_obj = {}
tus = []          # (end, name)
tu_first = {}     # name -> mangled name of the TU's first global symbol (from _GLOBAL__I_<sym>)
cur, curmax = None, None
def close_tu():
    if cur and curmax:
        tus.append((curmax, cur))
for line in open(symtab):
    m = SYM.match(line.rstrip('\n'))
    if not m: continue
    a, fl, sec, sz, name = int(m[1], 16), m[2], m[3], int(m[4], 16), m[5]
    if 'df' in fl and sec == '*ABS*':
        close_tu(); cur, curmax = name, None; continue
    if sec == '.text' and 'F' in fl:
        if fl[0] == 'l' and cur:
            curmax = max(curmax or 0, a + sz)
            if name.startswith('_GLOBAL__I_'): tu_first[cur] = name[len('_GLOBAL__I_'):]
        bind = 'l' if fl[0] == 'l' else 'w' if fl[1] == 'w' else 'g'
        if a not in funcs or bind == 'g':
            funcs[a] = dict(addr=a, size=sz, bind=bind, mangled=name)
    elif 'O' in fl:
        objs[name] = (a, sz); byaddr_obj[a] = name
close_tu()
tus.sort()
tu_ends = [e for e, _ in tus]
tu_start = {}
def tu_of(a):
    i = bisect.bisect_right(tu_ends, a)
    if i >= len(tus): return '?'
    st = tu_start.get(tus[i][1])
    # a strong function before the TU's first symbol sits in a file without a FILE symbol
    if st is not None and a < st: return '(no FILE symbol)'
    return tus[i][1]

def demangle(names):
    r = subprocess.run(['c++filt', '-n'], input='\n'.join(names), capture_output=True, text=True)
    return r.stdout.split('\n')[:len(names)]

addrs = sorted(funcs)
_m2a = {funcs[a]['mangled']: a for a in addrs}
for n, first in tu_first.items():
    if first in _m2a: tu_start[n] = _m2a[first]
for f, d in zip(addrs, demangle([funcs[a]['mangled'] for a in addrs])):
    funcs[f]['dem'] = d

def split_top(s, sep='::'):
    out, depth, i, start = [], 0, 0, 0
    while i < len(s):
        c = s[i]
        if c == '<': depth += 1
        elif c == '>': depth -= 1
        elif depth == 0 and s.startswith(sep, i):
            out.append(s[start:i]); i += len(sep); start = i; continue
        i += 1
    out.append(s[start:]); return out

def qualname(dem):
    d = re.sub(r'^(non-)?virtual thunk to ', '', dem)
    d = d.replace('operator()', 'operator@@').replace('(anonymous namespace)', '{anon}')
    depth = 0
    for i, c in enumerate(d):
        if c == '<': depth += 1
        elif c == '>': depth -= 1
        elif c == '(' and depth == 0:
            d = d[:i]; break
    return d.replace('operator@@', 'operator()').replace('{anon}', '(anonymous namespace)').strip()

PFX = 'dice::hfe::ai::'
def class_of(dem):
    q = qualname(dem)
    parts = split_top(q)
    if len(parts) < 2: return '', parts[-1]
    c = '::'.join(parts[:-1])
    if c == PFX[:-2]: c = ''          # namespace-scope function in dice::hfe::ai
    if c.startswith(PFX): c = c[len(PFX):]
    return c, parts[-1]

# GCC 3 places a file's .gnu.linkonce.t (weak/inline) sections after its own .text, i.e. after
# its static initialiser. A weak function that precedes the next file's first strong function
# therefore belongs to the previous file.
strong = sorted(a for a in addrs if funcs[a]['bind'] == 'g')
def tu_of_weak(a):
    i = bisect.bisect_right(tu_ends, a)
    if i == 0: return tu_of(a)
    j = bisect.bisect_left(strong, tu_ends[i - 1])
    if j < len(strong) and a < strong[j]: return tus[i - 1][1]
    return tu_of(a)
for a in addrs:
    f = funcs[a]
    f['cls'], f['meth'] = class_of(f['dem'])
    f['tu'] = tu_of(a) if f['bind'] != 'w' else tu_of_weak(a)

# ---------------------------------------------------------------- disassembly
HDR = re.compile(r'^([0-9a-f]{8}) <(.+)>:$')
INS = re.compile(r'^\s+([0-9a-f]+):\s+(\S.*)$')
CALL = re.compile(r'^(calll|jmp|jmpl)\s+0x([0-9a-f]+)(?: <(.+?)>)?')
body = collections.defaultdict(list)
cura = None
for line in open(dis):
    m = HDR.match(line.rstrip('\n'))
    if m:
        a = int(m[1], 16)
        # local labels (.Lnnn, jump-table targets) inside a function keep the current function
        if a in funcs: cura = a
        elif not (cura is not None and a < cura + funcs[cura]['size']): cura = None
        continue
    if cura is None: continue
    m = INS.match(line.rstrip('\n'))
    if m: body[cura].append(m[2])
for a in addrs:
    funcs[a]['insns'] = len(body.get(a, []))

# plt name -> mangled
def tgt_mangled(tok):
    if tok is None: return None
    tok = re.sub(r'\+0x[0-9a-f]+$', '', tok)
    return tok[:-4] if tok.endswith('@plt') else tok
mangled_to_addr = {}
for a in addrs:
    mangled_to_addr.setdefault(funcs[a]['mangled'], a)

# ---------------------------------------------------------------- relocations / data
rel32, relrel = {}, set()
for line in open(rels):
    p = line.split()
    if len(p) >= 2 and re.match(r'^[0-9a-f]{8}$', p[0]):
        if p[1] == 'R_386_32' and len(p) >= 3: rel32[int(p[0], 16)] = p[2]
        elif p[1] == 'R_386_RELATIVE': relrel.add(int(p[0], 16))
raw = open(LIB, 'rb').read()
ph = subprocess.run(['objdump', '-p', LIB], capture_output=True, text=True).stdout
loads = [(int(o, 16), int(v, 16), int(s, 16)) for o, v, s in re.findall(
    r'LOAD off\s+0x([0-9a-f]+) vaddr 0x([0-9a-f]+) paddr 0x[0-9a-f]+ align[^\n]*\n\s*filesz 0x([0-9a-f]+)', ph)]
def word(va):
    for off, v, sz in loads:
        if v <= va < v + sz: return struct.unpack_from('<I', raw, off + va - v)[0]
    return None
def ptr_target(va):
    """Mangled name a data pointer at va refers to (relocated)."""
    if va in rel32: return rel32[va]
    w = word(va)
    if w is None: return None
    if w in funcs: return funcs[w]['mangled']
    if w in byaddr_obj: return byaddr_obj[w]
    return None if w == 0 else f'0x{w:x}'

def dem1(m):
    return demangle([m])[0] if m else m

# vtables: _ZTV<cls> -> slot list from offset 8 (primary only; MI secondaries appended)
vt = {}
vtnames = [n for n in objs if n.startswith('_ZTV')]
for n, d in zip(vtnames, demangle(vtnames)):
    c = d.replace('vtable for ', '')
    c = c[len(PFX):] if c.startswith(PFX) else c
    a, sz = objs[n]
    slots = [ptr_target(a + 8 + 4 * i) for i in range((sz - 8) // 4)]
    vt[c] = dict(addr=a, size=sz, slots=slots)
# typeinfo bases
bases = {}
tinames = [n for n in objs if n.startswith('_ZTI')]
for n, d in zip(tinames, demangle(tinames)):
    c = d.replace('typeinfo for ', '')
    c = c[len(PFX):] if c.startswith(PFX) else c
    a, sz = objs[n]
    kind = rel32.get(a, '')
    b = []
    if 'si_class' in kind:
        b = [ptr_target(a + 8)]
    elif 'vmi_class' in kind:
        nb = word(a + 12) or 0
        b = [ptr_target(a + 16 + 8 * i) for i in range(min(nb, 8))]
    bases[c] = [x for x in b if x]
allb = sorted({x for v in bases.values() for x in v if x.startswith('_ZTI')})
bdm = dict(zip(allb, demangle(allb)))
for c in bases:
    bases[c] = [bdm.get(x, x).replace('typeinfo for ', '').replace(PFX, '') for x in bases[c]]

# ---------------------------------------------------------------- calls, new-sizes, ctor edges
IMM = re.compile(r'^movl\s+\$0x([0-9a-f]+), \(%esp\)')
calls = collections.defaultdict(set)    # addr -> set(mangled)
news = collections.defaultdict(set)     # cls -> sizes
ctor_edges = collections.defaultdict(set)  # addr -> classes constructed
def is_ctor(m):
    return bool(re.search(r'C[12]E', m or '')) and (m or '').startswith('_ZN')
for a in addrs:
    ins = body.get(a, [])
    for i, t in enumerate(ins):
        m = CALL.match(t)
        if not m: continue
        tg = tgt_mangled(m[3])
        if not tg or (m[1] != 'calll' and '@plt' not in (m[3] or '')): continue
        calls[a].add(tg)
        if tg == '_Znwj':
            size = None
            for j in range(max(0, i - 4), i):
                mm = IMM.match(ins[j]);
                if mm: size = int(mm[1], 16)
            for j in range(i + 1, min(len(ins), i + 20)):
                mm = CALL.match(ins[j])
                if mm:
                    t2 = tgt_mangled(mm[3])
                    if is_ctor(t2):
                        ta = mangled_to_addr.get(t2)
                        c = funcs[ta]['cls'] if ta is not None else class_of(dem1(t2))[0]
                        if size: news[c].add(size)
                        ctor_edges[a].add(c)
                        break
        elif is_ctor(tg):
            ta = mangled_to_addr.get(tg)
            c = funcs[ta]['cls'] if ta is not None else None
            if c: ctor_edges[a].add(c)

# ---------------------------------------------------------------- families
names = [n for _, n in tus]
def tu_range(first, last):
    i, j = names.index(first), names.index(last)
    return set(names[i:j + 1])
BEHAV_TUS = tu_range('BAPAAimAt.cpp', 'TankControl.cpp') | {'BotBehaviourManager.cpp', 'PrettyPrinter.cpp'}

CORE = {'BotScript', 'BotScriptVariable', 'BAPContext', 'BAPStatement', 'BAPCondition', 'BAPAction',
        'BAPCombiner', 'BAPBoleanOp', 'PlanInterpreter', 'BAPInterpreter', 'PlanGenerator', 'BAPEntry',
        'BAPConAnd', 'BAPConOr', 'BAPConNot', 'BAPConTrue', 'BAPConFalse', 'BAPConSharedCondition',
        'BAPConIteration', 'BAPConTime', 'BAPFlowControl'}
DEBUG = {'BAPDebugger', 'BotScriptDebugger', 'PrettyPrinter', 'PrettySpecialInfo'}
def family(c):
    b = c.split('<')[0].split('::')[0] if not c.startswith('(anonymous') else c
    if not c: return None
    if c.startswith(('std::', '__gnu')): return None
    if b.startswith('BAPVariable') or b in CORE: return 'core'
    if b in DEBUG or c.startswith('PrettyPrinter'): return 'debug'
    if b.startswith(('BAPFlow', 'BAPCombiner')): return 'flow'
    if b.startswith(('BAPCon', 'BAPCCon')): return 'condition'
    if b.startswith(('BAPWrapper', 'BAPInfoWrapper', 'BAPIW', 'BAPIC')): return 'wrapper'
    if b.startswith('BAPA'): return 'action'
    if b.startswith('BBP'): return 'pattern'
    if 'BBCreateFunction' in c or c == 'BotBehaviourManager': return 'factory'
    if b.startswith('BB') or b == 'BotBehaviour': return 'behaviour'
    if b.startswith(('UC', 'UrgeCurve', 'VehicleUrgencyCalculator')): return 'urge'
    if b.startswith('Entry'): return 'entry'
    if b.endswith('Control') or b in ('CommonControls', 'ControlFunctions'): return 'control'
    return None

for a in addrs:
    f = funcs[a]
    fam = family(f['cls'])
    if fam is None and f['tu'] in BEHAV_TUS:
        if re.match(r'^(__tcf_|__static_initialization|_GLOBAL__)', f['mangled']): fam = 'tuinit'
        elif f['cls'].startswith(('std::', '__gnu')) or f['dem'].startswith(('std::', '__gnu')): fam = 'std'
        elif f['cls'].startswith('dice::hfe::'): fam = 'shared'
        elif f['cls'] == '': fam = 'free'
        else: fam = 'other'
    f['fam'] = fam

# home TU per class: majority TU of its strong (non-weak) definitions
cls_funcs = collections.defaultdict(list)
for a in addrs: cls_funcs[funcs[a]['cls']].append(a)
home = {}
for c, l in cls_funcs.items():
    cnt = collections.Counter(funcs[a]['tu'] for a in l if funcs[a]['bind'] == 'g') or \
          collections.Counter(funcs[a]['tu'] for a in l)
    home[c] = cnt.most_common(1)[0][0]

# ---------------------------------------------------------------- content: .ai -> classes
GOT = 0x4B30C4
def cstr(va):
    for off, v, sz in loads:
        if v <= va < v + sz:
            return raw[off + va - v: off + va - v + 96].split(b'\0')[0].decode('latin1')
pattern_map = {}     # content pattern name -> class (BBCreateFunction compare chains)
create_bb = {}       # create function suffix -> BB class constructed
LEA = re.compile(r'leal\s+-0x([0-9a-f]+)\(%ebx\)')
for a in addrs:
    f = funcs[a]
    m = re.search(r'BBCreateFunction(\w+)::operator\(\)', f['dem'])
    if not m: continue
    key, ins, last = m[1], body.get(a, []), None
    for i, t in enumerate(ins):
        if 'compareEPKc' in t:
            for j in range(i - 1, max(0, i - 6), -1):
                mm = LEA.search(ins[j])
                if mm: last = cstr(GOT - int(mm[1], 16)); break
        mm = CALL.match(t)
        if mm and is_ctor(tgt_mangled(mm[3])):
            ta = mangled_to_addr.get(tgt_mangled(mm[3]))
            c = funcs[ta]['cls'] if ta is not None else None
            if c and c.startswith('BBP') and last: pattern_map[last] = c; last = None
            elif c and c.startswith('BB'): create_bb[key] = c

if not os.environ.get('BF2_LINUX_SERVER_DIR'):
    sys.exit('ai_behaviour_inventory.py: set BF2_LINUX_SERVER_DIR to the Linux 1.5 server installation')
ai = os.path.join(os.environ['BF2_LINUX_SERVER_DIR'], 'mods/bf2/ai/aibehaviours.ai')
veh_bb = collections.defaultdict(set); veh_entries = collections.defaultdict(set)
for line in open(ai, errors='replace'):
    p = line.split()
    if len(p) >= 5 and p[0].lower() == 'aisettings.setvehiclebehaviour':
        bb = create_bb.get(p[3][2:], p[3]); veh_bb[p[1]].add(bb)
        if p[4] in pattern_map: veh_bb[p[1]].add(pattern_map[p[4]])
    if len(p) >= 3 and p[0].lower() == 'aisettings.addinterpreterentry':
        veh_entries[p[1]].add('Entry' + p[2])

# ---------------------------------------------------------------- reachability (RTA)
def reach(seed_fn, seed_classes):
    seen, inst, work = set(), set(), []
    def addf(a):
        if a is not None and a not in seen: seen.add(a); work.append(a)
    def addc(c):
        if c in inst: return
        inst.add(c)
        for s in (vt.get(c, {}).get('slots') or []):
            if s: addf(mangled_to_addr.get(s))
        for b in bases.get(c, []): addc(b)
        for a in cls_funcs.get(c, ()):        # constructors and destructors
            if re.search(r'[CD][012]E', funcs[a]['mangled']): addf(a)
    for a in addrs:
        if any(re.search(p, funcs[a]['dem']) for p in seed_fn): addf(a)
    for c in seed_classes: addc(c)
    while work:
        a = work.pop()
        for tg in calls.get(a, ()): addf(mangled_to_addr.get(tg))
        for c in ctor_edges.get(a, ()): addc(c)
    return seen, inst

TICK = [r'^dice::hfe::ai::BotMain::(planExecution|decisionMaking|prePlanExecution|postPlanExecution)\(',
        r'^dice::hfe::ai::BAPInterpreter::']   # load time: AISettings::addInterpreterEntry/activateDefaultInterpreter
ALWAYS = {'BAPInterpreter', 'UCConstant', 'UCLinear', 'UCXInverse'}
MIN = {'BBIdle', 'BBMoveTo', 'BBFireInfantery', 'BBPIdleInfantery', 'BBPGotoWaypointSoldier', 'BBPFireInfantery'}
r_min, i_min = reach(TICK, ALWAYS | MIN | veh_entries['Infantery'])
r_mov, i_mov = reach(TICK, ALWAYS | (MIN - {'BBFireInfantery', 'BBPFireInfantery'}) | veh_entries['Infantery'])
r_inf, i_inf = reach(TICK, ALWAYS | veh_bb['Infantery'] | veh_entries['Infantery'])
allc = set().union(*veh_bb.values()) | set().union(*veh_entries.values())
r_all, i_all = reach(TICK + [r'BotBehaviourManager::'], ALWAYS | allc)
r_sen, i_sen = reach([r'^dice::hfe::ai::SensingAgent'], set())
for a in addrs:
    f = funcs[a]
    f['reach'] = ('min' if a in r_min else 'infantry' if a in r_inf else 'vehicle' if a in r_all
                  else 'sensing' if a in r_sen else '')

RANK = {'min': 4, 'infantry': 3, 'vehicle': 2, 'sensing': 1, '': 0}
def cls_reach(c):
    if c in i_min: return 'min'
    if c in i_inf: return 'infantry'
    if c in i_all: return 'vehicle'
    if c in i_sen: return 'sensing'
    if c not in vt:   # static-only classes (the *Control helpers): best function-level reach
        return max((funcs[a]['reach'] for a in cls_funcs.get(c, ())), key=lambda r: RANK[r], default='')
    return ''

# ---------------------------------------------------------------- packages
def package_of(c, fam):
    r = cls_reach(c)
    if fam == 'debug': return 'P7-deferred'
    if fam in ('core', 'factory', 'urge') or c == 'BotBehaviour' or c.startswith(('BAPFlow', 'BAPCombiner')) \
            or c in ('BBPPattern', 'PlanGenerator'):
        return 'P1-core'
    if fam in ('entry', 'control'):
        if c in i_min and not any(v in c for v in ('Tank', 'Car', 'Boat', 'Plane', 'Heli', 'KeyTurret')):
            return 'P2-entries'
        return 'P6-vehicles' if r else 'P7-deferred'
    if fam in ('behaviour', 'pattern'):
        if r == 'min' or c in ('BBFire', 'BBPFire', 'BBPIdle', 'BBPGotoWaypoint') or c.startswith('BBPFireInfantery'):
            return 'P4-patterns'
        return {'infantry': 'P5-infantry', 'vehicle': 'P6-vehicles'}.get(r, 'P7-deferred')
    if fam in ('action', 'condition', 'wrapper'):
        return {'min': 'P3-actions', 'sensing': 'P3-actions', 'infantry': 'P5-infantry',
                'vehicle': 'P6-vehicles'}.get(r, 'P7-deferred')
    return 'P1-core'
FINAL = {'P1-core': 'A-core', 'P2-entries': 'B-entries', 'P3-actions': 'C-actions', 'P4-patterns': 'D-behaviours',
         'P5-infantry': 'F-infantry', 'P6-vehicles': 'G-vehicles', 'P7-deferred': 'H-deferred'}
strong_n = collections.Counter(funcs[a]['cls'] for a in addrs if funcs[a]['bind'] == 'g')
pkg_of_class = {}
for c in cls_funcs:
    fam = family(c)
    if not fam: continue
    p = FINAL[package_of(c, fam)]
    if c.startswith('BBPFireInfantery'): p = 'E-fire-pattern'
    if c in ('BBPIdle2d', 'BBPIdle3d'): p = 'D-behaviours'      # BBPIdleInfantery::createPlan calls their init
    if fam == 'factory': p = 'M-main'                             # Main/BotBehaviourManager.cpp (ai-main)
    elif strong_n[c] == 0 and fam in ('core', 'urge', 'debug', 'pattern', 'condition', 'entry'): p = 'S0-skeleton'
    pkg_of_class[c] = p
# std / tuinit / free functions follow the dominant class of their TU
tu_pkg = collections.defaultdict(collections.Counter)
for a in addrs:
    f = funcs[a]
    if f['cls'] in pkg_of_class: tu_pkg[f['tu']][pkg_of_class[f['cls']]] += 1
for a in addrs:
    f = funcs[a]
    if not f['fam']: f['pkg'] = ''; continue
    if f['cls'] in pkg_of_class: f['pkg'] = pkg_of_class[f['cls']]
    elif f['tu'] in ('InfanteryControl.cpp', 'MouseControl.cpp', 'ControlFunctions.cpp'): f['pkg'] = 'B-entries'
    elif f['tu'] == 'VehicleUrgencyCalculator.cpp': f['pkg'] = 'A-core'
    elif f['tu'] == 'BotBehaviourManager.cpp': f['pkg'] = 'M-main'
    else: f['pkg'] = tu_pkg[f['tu']].most_common(1)[0][0] if tu_pkg[f['tu']] else 'A-core'

# ---------------------------------------------------------------- tree status
defined, frontier = set(), set()
if os.environ.get('TREE'):
    DEF = re.compile(r'^[^\s/#][^;]*?\b(\w+)::(~?\w+|operator\(\))\s*\(', re.M)
    FR = re.compile(r'BF2_AI_FRONTIER\("dice::hfe::ai::(?:\(anonymous namespace\)::)?(\w+)::(~?\w+|operator\(\))')
    for root, _, fs in os.walk(os.environ['TREE']):
        for fn in fs:
            if fn.endswith(('.cpp', '.h')):
                t = open(os.path.join(root, fn), errors='replace').read()
                defined |= {f'{a}::{b}' for a, b in DEF.findall(t)}
                defined |= {f'::{b}' for b in re.findall(r'^[A-Za-z_][\w\s\*&:<>,]*?\b(\w+)\s*\([^;]*$', t, re.M)}
                frontier |= {f'{a}::{b}' for a, b in FR.findall(t)}
for a in addrs:
    f = funcs[a]
    k = f"{f['cls'].split('::')[-1]}::{f['meth']}" if f['cls'] not in ('', 'dice::hfe', 'dice::hfe::io', 'std') else f"::{f['meth']}"
    f['tree'] = 'frontier' if k in frontier else 'defined' if k in defined else ''

# ---------------------------------------------------------------- phase
# load: runs while aibehaviours.ai is read (create functions construct every content behaviour
# and pattern, AISettings registers interpreter entries); tick: reached from the bot tick with the
# minimum behaviour set; urgency: behaviour methods every bot of that vehicle evaluates each
# decision tick; plan: pattern createPlan bodies and what only they reach.
for a in addrs:
    f = funcs[a]
    c, m = f['cls'], f['mangled']
    isctor = bool(re.search(r'[CD][012]E', m)) and c.split('::')[-1] in f['dem']
    content = cls_reach(c) in ('min', 'infantry', 'vehicle')
    fam = f['fam']
    if isctor and content and fam in ('behaviour', 'pattern', 'entry', 'urge', 'core', 'factory'):
        f['phase'] = 'load'
    elif fam == 'factory' or re.search(r'BAPInterpreter::(addEntry|removeEntry|activateEntry)', f['dem']):
        f['phase'] = 'load'
    elif f['reach'] == 'min':
        f['phase'] = 'tick'
    elif fam == 'behaviour' and content:
        f['phase'] = 'urgency'
    elif f['reach'] in ('infantry', 'vehicle'):
        f['phase'] = 'plan'
    else:
        f['phase'] = ''

# ---------------------------------------------------------------- output
sel = [funcs[a] for a in addrs if funcs[a]['fam']]
with open(OUT, 'w', newline='') as fh:
    w = csv.writer(fh)
    w.writerow(['elf_addr', 'size', 'insns', 'bind', 'tu', 'class', 'family', 'package', 'reach', 'min_tier', 'phase', 'tree', 'function'])
    for f in sel:
        w.writerow([f'0x{f["addr"]:06X}', f['size'], f['insns'], f['bind'], f['tu'], f['cls'], f['fam'],
                    f['pkg'], f['reach'], ('stand-move' if f['addr'] in r_mov else 'shoot') if f['reach'] == 'min' else '', f['phase'], f['tree'], f['dem']])

agg = {}
for f in sel:
    if f['fam'] in ('std', 'tuinit', 'free', 'shared', 'other'): continue
    g = agg.setdefault(f['cls'], dict(n=0, ins=0, strong=0, ctor=[], dtor=[], callees=set(), lo=f['addr']))
    g['strong'] += f['bind'] == 'g'
    g['n'] += 1; g['ins'] += f['insns']; g['lo'] = min(g['lo'], f['addr'])
    if re.search(r'C[12]E', f['mangled']): g['ctor'].append(f'0x{f["addr"]:X}')
    if re.search(r'D[012]Ev', f['mangled']): g['dtor'].append(f'0x{f["addr"]:X}')
    for tg in calls.get(f['addr'], ()):
        ta = mangled_to_addr.get(tg)
        if ta is not None and funcs[ta]['cls'] and funcs[ta]['cls'] != f['cls']:
            g['callees'].add(funcs[ta]['cls'])
    for c in ctor_edges.get(f['addr'], ()):
        if c != f['cls']: g['callees'].add(c)
inbeh = set(agg)
tree = os.environ.get('TREE')
tree_classes = set()
if tree:
    for root, _, fs in os.walk(tree):
        for fn in fs:
            if fn.endswith('.h'):
                for m in re.finditer(r'^\s*(?:class|struct)\s+(\w+)\s*(?::|\{)', open(os.path.join(root, fn), errors='replace').read(), re.M):
                    tree_classes.add(m[1])
if OUTC:
    with open(OUTC, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['class', 'family', 'package', 'reach', 'functions', 'strong', 'insns', 'home_tu', 'proposed_file', 'base', 'vtable',
                    'vt_words', 'new_size', 'ctors', 'dtors', 'uses_behaviour_classes', 'uses_tree_classes',
                    'uses_other_classes'])
        for k, g in sorted(agg.items(), key=lambda kv: kv[1]['lo']):
            v = vt.get(k)
            beh = sorted(c for c in g['callees'] if c in inbeh)
            rest = [c for c in g['callees'] if c not in inbeh and not c.startswith(('std::', '__gnu'))]
            intree = sorted(c for c in rest if c.split('::')[-1] in tree_classes or c in tree_classes)
            oth = sorted(c for c in rest if c not in intree)
            ht = home.get(k, '')
            pf = ('Main/BotBehaviourManager.cpp') if k.startswith('(anonymous') else \
                 (k.split('<')[0] + '.h (inline)') if g['strong'] == 0 else \
                 (k.split('::')[-1] + '.cpp') if ht == '(no FILE symbol)' else ht
            fam = family(k)
            d = ('AIPlanInterpreter/' if fam in ('entry', 'control') or k in ('PlanInterpreter', 'BAPInterpreter',
                     'BAPDebugger', 'BotScriptDebugger', 'BAPEntry') else
                 'AIBotBehaviourPattern/' if fam == 'pattern' or k == 'PlanGenerator' else
                 'AIBotBehaviour/' if fam in ('behaviour', 'urge') else
                 'AIBotActionPlan/' if fam in ('action', 'condition', 'wrapper', 'flow', 'core') else '')
            if k in ('BAPContext',): d = 'AIPlanInterpreter/'
            if k.startswith('BBPFireInfantery::'): pf = 'BBPFireInfantery.cpp'
            if not pf.startswith('Main/'): pf = d + pf
            w.writerow([k, family(k), pkg_of_class.get(k, ''), cls_reach(k), g['n'], g['strong'], g['ins'], ht, pf,
                        '|'.join(bases.get(k, [])), f'0x{v["addr"]:X}' if v else '',
                        ((v['size'] - 8) // 4) if v else '', '|'.join(f'0x{s:X}' for s in sorted(news.get(k, ()))),
                        ' '.join(g['ctor'][:4]), ' '.join(g['dtor'][:3]), ' '.join(beh), ' '.join(intree), ' '.join(oth)])

tot = collections.Counter(); toti = collections.Counter(); totc = collections.defaultdict(set)
for f in sel:
    tot[f['pkg']] += 1; toti[f['pkg']] += f['insns']
    if f['cls'] in agg: totc[f['pkg']].add(f['cls'])
print('selected functions', len(sel), 'insns', sum(f['insns'] for f in sel))
for k in sorted(tot): print(f'{k:14s} {len(totc[k]):4d} classes {tot[k]:5d} fns {toti[k]:7d} insns')
fc = collections.Counter(f['fam'] for f in sel); fi = collections.Counter()
for f in sel: fi[f['fam']] += f['insns']
for k in sorted(fc): print(f'fam {k:10s} {fc[k]:5d} {fi[k]:7d}')
for k in ('min', 'infantry', 'vehicle', 'sensing'):
    s = [f for f in sel if f['reach'] == k]; print('reach', k, len(s), sum(f['insns'] for f in s))
print('pattern_map', pattern_map)
print('create_bb', create_bb)
print('infantry content', sorted(veh_bb['Infantery']))
print('sensing-instantiated', ' '.join(sorted(c for c in i_sen if c in inbeh)))
print('min-instantiated', ' '.join(sorted(c for c in i_min if c in inbeh)))

if os.environ.get('VT'):
    for c in os.environ['VT'].split(','):
        v = vt.get(c)
        if not v: print(c, 'no vtable'); continue
        sl = v['slots']; ds = demangle([s or '0' for s in sl])
        print(f'== {c} vtable 0x{v["addr"]:X} words {len(sl)} bases {bases.get(c)} new {sorted(news.get(c, ()))}')
        for i, d in enumerate(ds): print(f'  +0x{4*i:02X} {d[:150]}')

# external functions (outside the selection) the minimum set calls, with their tree status
if os.environ.get('EXT'):
    selset = {f['addr'] for f in sel}
    ext = collections.defaultdict(set)
    for a in r_min:
        if a not in selset: continue
        for tg in calls.get(a, ()):
            ta = mangled_to_addr.get(tg)
            if ta is not None and ta not in selset and funcs[ta]['cls']:
                ext[(funcs[ta]['tu'], funcs[ta]['cls'])].add((funcs[ta]['meth'], funcs[ta]['tree'], ta))
    for (tu, c), ms in sorted(ext.items()):
        st = collections.Counter(t or 'absent' for _, t, _ in ms)
        print(f'EXT {tu:28s} {c:28s} n={len(ms):3d} {dict(st)} missing: ' +
              ' '.join(f'{m}@0x{a:X}' for m, t, a in sorted(ms) if t != 'defined')[:400])
