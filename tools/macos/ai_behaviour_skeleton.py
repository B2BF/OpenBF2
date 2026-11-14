#!/usr/bin/env python3
"""[CHOSEN, not recovered] One-shot generator for the S0 behaviour-system skeleton.

Reads the Linux libaidll.so (1.5.3153, ia-32, symbols) through ai_behaviour_inventory.py and
writes one header and one .cpp per attested translation unit for every class of the
behaviour / action-plan system (docs/macos-ai-behaviour-plan.md, section 4 "S0"):

* classes, bases and virtual functions in ELF vtable slot order (typeinfo, vtable relocations);
* constructors, destructors and every other member function from the symbol table;
* return types from the base declaration (overrides), from HAND below, or from the ELF bodies
  (callee epilogue and call sites; see infer_ret). Those are marked [INFERRED] per line;
* every body a BF2_AI_FRONTIER stop; no data members except where HAND gives the layout.

Also writes tools/macos/ai_behaviour_vtables.json (the ELF slot lists for the vtable check).

After S0 the generated files belong to the packages; do not re-run this over owned files.
Usage: ai_behaviour_skeleton.py <libaidll.so> <workdir> [--write]
"""
import collections, contextlib, io, json, os, re, runpy, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AI = os.path.join(ROOT, 'Code/BF2/AI/AIDLL/AI')
LIB, WD = sys.argv[1], sys.argv[2]
WRITE = '--write' in sys.argv

sys.argv = ['inv', LIB, WD, os.path.join(WD, 'funcs.csv'), os.path.join(WD, 'classes.csv')]
os.environ['TREE'] = os.path.join(ROOT, 'Code/BF2')
with contextlib.redirect_stdout(io.StringIO()):
    g = runpy.run_path(os.path.join(ROOT, 'tools/macos/ai_behaviour_inventory.py'), run_name='inv')
funcs, vt, bases, news, body = g['funcs'], g['vt'], g['bases'], g['news'], g['body']
pkg, home, family, cls_funcs = g['pkg_of_class'], g['home'], g['family'], g['cls_funcs']
split_top, qualname, demangle, mangled_to_addr = g['split_top'], g['qualname'], g['demangle'], g['mangled_to_addr']
strong_n = g['strong_n']
body = {a: [re.sub(r'\s+#.*', '', t).strip() for t in ins] for a, ins in body.items()}

# ---------------------------------------------------------------- scope
EXISTING = {'UrgeCurve', 'UCConstant', 'UCLinear', 'UCXInverse', 'BAPContext', 'BotScriptVariable',
            'BAPEntry', 'CustomCurve', 'UrgencyGenerator'}
ENTRY_IN_TREE = set()   # recovered entries (BAPEntries split by S0); never regenerated
for fn in os.listdir(os.path.join(AI, 'AIPlanInterpreter')):
    if fn.startswith('Entry') and fn.endswith('.h'):
        t = open(os.path.join(AI, 'AIPlanInterpreter', fn)).read()
        if 'split out of BAPEntries' in t:
            ENTRY_IN_TREE |= set(re.findall(r'^class (Entry\w+) : public BAPEntry', t, re.M))
scope = sorted(c for c, p in pkg.items() if p != 'M-main' and c not in EXISTING and c not in ENTRY_IN_TREE
               and not c.startswith('(anonymous') and '<' not in c)   # templates: hand-written in S0
_extra = set()
for c in list(scope):
    for b in bases.get(c, []):
        while b and b not in scope and b not in EXISTING and b not in _extra and '<' not in b:
            _extra.add(b)
            bb = bases.get(b, []); b = bb[0] if bb else None
for b in _extra:
    der = [pkg[c] for c in scope if b in bases.get(c, []) and c in pkg]
    pkg[b] = min(der) if der else 'A-core'   # the earliest package that derives from it
scope = sorted(set(scope) | _extra)
NESTED = {c for c in scope if '::' in c}
TOP = [c for c in scope if c not in NESTED]

CONTROL_NS = {'BoatControl', 'CarControl', 'CommonControls', 'HeliControl', 'PlaneControl', 'TankControl'}
def directory(c):
    f = family(c)
    if f == 'entry' or c in CONTROL_NS or c in ('PlanInterpreter', 'BAPInterpreter', 'BAPDebugger', 'BotScriptDebugger',
                                          'BotScript', 'PrettyPrinter'):
        return 'AIPlanInterpreter'
    if f == 'pattern' or c == 'PlanGenerator': return 'AIBotBehaviourPattern'
    if f in ('behaviour', 'urge'): return 'AIBotBehaviour'
    return 'AIBotActionPlan'

def stem_of(c):
    ht = home.get(c, '')
    if strong_n[c] == 0 or ht in ('', '(no FILE symbol)', '?'):
        return c.split('<')[0]
    return ht[:-4]
# PrettyPrinter's TU sits in the AIBot block of the link order [INFERRED] directory.
FILE = {c: f'{directory(c)}/{stem_of(c)}' for c in TOP}
FILE['BotScript'] = 'AIPlanInterpreter/BotScript'
# free functions of the control TUs
FREE_TUS = {'InfanteryControl.cpp', 'MouseControl.cpp', 'ControlFunctions.cpp'}

# ---------------------------------------------------------------- hand knowledge (S0 evidence)
TYPES_H = 'AIBotActionPlan/BAPTypes'
HAND_RET = {
    # BotScript 0x4B77D8 / BAPStatement 0x4BA240: BAPInterpreter::execute 0x2A99D0 tests the al result
    'execute(Bot*, bool&, bool&)': 'bool',
    'getControls()': 'unsigned long long',   # 0x1CDB10: edx:eax = +0xC:+0x8
    'getId() const': 'BAPStatementType',     # streamed by operator<<(ostream&, BAPStatementType) 0x3C7D90
    'hasDefault()': 'bool',
    'debug(BotScriptDebugger::DebugLevel, std::string&)': 'void',   # 0x1F8210 assigns "Standard debug."
    'streamSelf(std::ostream&) const': 'void',
    'evaluate(Bot*)': 'bool',
    'getConditionType() const': 'BAPConditionType',   # operator<<(ostream&, BAPConditionType) 0x3C7AE0
    'createPlan(Bot*)': 'BotScript*',        # the plan BotMain::setPlan stores
    'generatePlan(Bot*)': 'BotScript*',
    'isInitiated(Bot*)': 'bool',
    'calculateUrgency(Bot*, bool, float)': 'float',
    'isAplicable(Information*, Bot*)': 'bool',
    'getUrgency(Bot*)': 'float', 'getActiveUrgency(Bot*)': 'float', 'updateActiveUrgency(Bot*)': 'float',
    'hasChangedTarget(Bot*)': 'bool', 'quotientChanged(Bot*, float, float)': 'bool',
    'hasChanged(Bot*, float, float)': 'bool', 'getChosenGenerator()': 'UrgencyGenerator*',
    'reset(Bot*)': 'void',
    'execute(BotScript*, Bot*, bool&, bool&)': 'bool', 'getTargetId()': 'int',
}
# Base layouts frozen in S0 (constructor stores; addresses in the comments).
HAND_MEMBERS = {
    'BAPStatement': '''public:
    static int news;        // BSS 0x4CE268; every BAPStatement constructor increments it
    static int destroys;    // BSS 0x4CE26C; ~BAPStatement 0x1F83B0 increments it

protected:
    /* +0x04 */ void* m_parent;                 // = 0 in every constructor; [UNRESOLVED] type and name
    /* +0x08 */ unsigned long long m_controls;  // = 0; getControls 0x1CDB10 returns it (the ctor `y` argument)''',
    'BAPCondition': '''protected:
    // [UNRESOLVED] type: BAPFlowCEnd's constructor (0x1E3230) stores the owning statement here,
    // BAPConSharedCondition's (0x1E9D80) and BAPBoleanOp's the owning condition.
    /* +0x04 */ void* m_owner;''',
    'BotBehaviour': '''protected:
    // BotBehaviour(float*, UrgeCurve*, int) 0x225250 and BotBehaviour(float*, UrgeCurve*) 0x2259D0.
    // +0x04/+0x05 are bytes the BB* constructors store (behaviour index, queue group) [INFERRED].
    /* +0x04 */ unsigned char m_index;
    /* +0x05 */ unsigned char m_queue;
    /* +0x08 */ void* m_unknown8;               // = 0 [UNRESOLVED]
    /* +0x0C */ void* m_unknownC;               // = 0 [UNRESOLVED]
    /* +0x10 */ float* m_urgency;               // new float[count]; getUrgency 0x225D70 indexes it by bot slot +0xE8
    /* +0x14 */ float* m_modifiers;             // constructor argument 1 (the behaviour-modifier row)
    /* +0x18 */ bool* m_flags;                  // new bool[count] [UNRESOLVED] meaning
    /* +0x1C */ float* m_activeUrgency;         // new float[count], each 1e-5f; updateActiveUrgency 0x225DD0 writes it
    /* +0x20 */ UrgeCurve* m_urgeCurve;         // constructor argument 2''',
    'BBPPattern': '''protected:
    // BBPPattern() 0x296FD0: new[] sized by CommonSingletonServer::m_iaiSettings slot 0 [UNRESOLVED] element type.
    /* +0x04 */ bool* m_initiated;''',
}
HAND_DOC = {
    'BotScript': 'Root of every plan node. Vtable 0x4B77D8: execute (pure), D1, D0. Inline in the image.',
    'BAPStatement': 'Plan statement base. Vtable 0x4BA240. Layout from BAPFlowCEnd(int, EndType, BAPCondition*) 0x1E3230.',
    'BAPAction': 'Action base. Vtable 0x4B8000; no members of its own (inline in the image).',
    'BAPCondition': 'Condition base. Vtable 0x4BA1C8: evaluate, getConditionType, streamSelf, D1, D0.',
    'BotBehaviour': 'Behaviour base. Vtable 0x4BB580 (13 words).',
    'BBPPattern': 'Behaviour-pattern base. Vtable 0x4BC400.',
    'PlanGenerator': 'Pattern interface. Vtable 0x4BC420: D1, D0, four pure slots. Inline in the image.',
    'PlanInterpreter': 'Interpreter interface. Vtable 0x4BCA60: D1, D0, execute (pure). Inline in the image.',
}
INLINE_DTOR = {'BotScript', 'BAPAction', 'BAPCombiner', 'BAPBoleanOp', 'PlanGenerator', 'PlanInterpreter',
               'BBPGotoWaypoint', 'BotScriptDebugger'}

# ---------------------------------------------------------------- type rendering
def norm(t):
    t = t.strip()
    t = t.replace('dice::hfe::ai::', '').replace('dice::hfe::', '')
    for _ in range(4):
        t = re.sub(r'std::(list|vector)<(.+?), std::allocator<\2>\s*>', r'std::\1<\2>', t)
    t = re.sub(r'std::map<(.+?), (.+), std::less<\1>, std::allocator<std::pair<\1 const, \2>\s*>\s*>', r'std::map<\1, \2>', t)
    if 'Bot::C4PlacementType' in t:
        t = t.replace('Bot::C4PlacementType', 'int /* [UNRESOLVED] Bot::C4PlacementType */')
    t = re.sub(r'((?:[\w:]|<[^<>]*>)+) const([&*])', r'const \1\2', t)
    t = t.replace('>>', '> >')
    return t

def parse_sig(dem):
    if 'operator<<' in dem:
        q, al, const = parse_sig(dem.replace('operator<<', 'operatorLSHIFT'))
        return q.replace('operatorLSHIFT', 'operator<<'), al, const
    q = qualname(dem)
    d = re.sub(r'^(non-)?virtual thunk to ', '', dem)
    i = len(q)
    while i < len(d) and d[i] != '(':
        i += 1
    depth, j = 0, i
    for j in range(i, len(d)):
        if d[j] == '(': depth += 1
        elif d[j] == ')':
            depth -= 1
            if depth == 0: break
    args = d[i + 1:j]
    const = d[j + 1:].strip().startswith('const')
    al = [norm(a) for a in split_top(args, ', ') if a and a != 'void']
    return q, al, const

def argkey(name, al, const):
    return f"{name}({', '.join(al)})" + (' const' if const else '')

# ---------------------------------------------------------------- return types from the ELF
CALLERS = collections.defaultdict(list)    # mangled -> [(caller addr, index)]
CALLRE = re.compile(r'^calll\s+0x[0-9a-f]+ <(.+?)(?:@plt)?>')
for a, ins in body.items():
    for i, t in enumerate(ins):
        m = CALLRE.match(t)
        if m: CALLERS[m[1]].append((a, i))
EPI = re.compile(r'^(popl|leave|nop|xchg|movl\s+%ebp, %esp|addl\s+\$0x[0-9a-f]+, %esp|leal\s+\(%e[sd]i(,%eiz)?\), %e[sd]i'
                 r'|movl\s+-?0x[0-9a-f]+\(%ebp\), %e(bx|si|di)|movl\s+0x[0-9a-f]+\(%esp\), %e(bx|si|di))')
def callee_kind(addr):
    ins = body.get(addr, [])
    kinds = []
    for r, t in enumerate(ins):
        if not t.startswith('retl'): continue
        if '$0x4' in t: return 'sret'
        k = 'void'
        j = r - 1
        while j >= 0 and EPI.match(ins[j]): j -= 1
        for t2 in ins[max(0, j - 3):j + 1][::-1]:
            mn = t2.split()[0]
            if mn.startswith('f') and not mn.startswith(('fstp', 'fnstcw', 'fldcw', 'fist')):
                k = 'double' if re.match(r'^f\w*l$', mn) and mn not in ('fmul', 'fild', 'fildl') else 'float'
                if mn in ('fildl',): k = 'float'
                break
            if re.match(r'^fstp\s+%st\([1-7]\)$', t2):
                k = 'float'; break       # pops into st(1): the result stays in st(0)
            if mn.startswith('fstp'): break
            if re.search(r', %al$', t2) or mn.startswith('set') or (mn == 'movzbl' and t2.endswith('%eax')):
                k = 'bool'; break
            if re.search(r', %eax$', t2) or mn == 'xorl' and '%eax, %eax' in t2:
                k = 'int'; break
            if mn.startswith(('j', 'call')): break
        kinds.append(k)
    for k in ('double', 'float', 'bool', 'int'):
        if k in kinds: return k
    return 'void' if kinds else '?'

def caller_kind(mangled):
    out = collections.Counter()
    for a, i in CALLERS.get(mangled, [])[:40]:
        ins = body[a]
        if i + 1 >= len(ins): continue
        n = ins[i + 1]
        mn = n.split()[0]
        if re.match(r'^(testb|cmpb)\s.*%al', n) or re.match(r'^movb\s+%al', n) or re.match(r'^movzbl\s+%al', n):
            out['bool'] += 1
        elif mn.startswith(('fstp', 'fsts', 'fxch', 'fmul', 'fadd', 'fsub', 'fdiv', 'fucom', 'fcom', 'fld %st', 'fchs', 'fabs')):
            out['float'] += 1
        elif re.match(r'^movl\s+-?(0x[0-9a-f]+)?\(%eax\)', n):
            out['ptr'] += 1
        elif re.match(r'^(movl\s+%eax|testl\s+%eax, %eax|cmpl\s+.*%eax|addl\s+%eax|imull|leal\s+.*%eax)', n):
            out['int'] += 1
    return out

BOOLNAME = re.compile(r'^(is|has|can|should|was|did|are|need|check|evaluate|allow|use[A-Z]|get(Is|Has|Can))')
def infer_ret(f, name):
    k = callee_kind(f['addr'])
    ck = caller_kind(f['mangled'])
    ev = f'ELF {k}' + (f', callers {dict(ck)}' if ck else '')
    if k == 'sret':
        return ('Vec3' if re.search(r'(Pos|Position|Point|Dir|Direction|Coord|Vec|Target|Centre|Center)', name)
                else 'std::string' if re.search(r'(Name|String|Text)', name) else 'Vec3'), ev + ' (returned through a hidden pointer)'
    if k in ('float', 'double'): return k, ev
    if k == 'bool' or (k == 'int' and (BOOLNAME.match(name) or ck['bool'] > ck['int'] + ck['ptr'])):
        return 'bool', ev
    if k == 'int':
        if ck['ptr'] > 0: return 'void*', ev + ' [UNRESOLVED] pointee'
        return 'int', ev
    if k == 'void' and ck['float']: return 'float', ev
    return 'void', ev

# ---------------------------------------------------------------- per-class model
dem_of = {f['mangled']: f['dem'] for f in funcs.values()}
_rest = sorted({x for v in vt.values() for x in v['slots'] if x and x.startswith('_Z') and x not in dem_of})
dem_of.update(zip(_rest, demangle(_rest)))
def D(m):
    if m not in dem_of: dem_of[m] = demangle([m])[0]
    return dem_of[m]

def prim_slots(c):
    s = vt[c]['slots']; out = []
    for x in s:
        if x is None or x.startswith(('0x', '_ZTI')): break
        out.append(x)
    return out
def sec_slots(c):
    s = vt[c]['slots']
    for i, x in enumerate(s):
        if x is None or x.startswith(('0x', '_ZTI')): return [y for y in s[i + 2:] if y and not y.startswith(('0x', '_ZTI'))]
    return []
def primary_base(c):
    b = bases.get(c, [])
    return b[0] if b else None
def ancestors(c):
    out = []
    while True:
        b = primary_base(c)
        if not b: return out
        out.append(b); c = b

DECLS = {}   # class -> {key: ret}
def ret_from_bases(c, key):
    for b in [c] + ancestors(c) + bases.get(c, [])[1:]:
        if key in DECLS.get(b, {}): return DECLS[b][key]
    if key in HAND_RET: return HAND_RET[key]
    nm = key.split('(')[0]
    for k2, r in HAND_RET.items():
        if k2.split('(')[0] == nm and k2 == key: return r
    return None

unresolved_slots = []
def model(c):
    m = dict(name=c, ctors=[], dtor=None, virt=[], plain=[], doc=HAND_DOC.get(c, ''), dtor_slot=None)
    fl = [funcs[a] for a in cls_funcs.get(c, [])]
    seen = set()
    vslots = prim_slots(c) if c in vt else []
    nbase = len(prim_slots(primary_base(c))) if primary_base(c) in vt else 0
    short = c.split('::')[-1].split('<')[0]
    dtor_virtual = any(re.search(r'D[01]Ev$', s or '') and s.startswith('_ZN') for s in vslots) or \
        any(re.search(r'D[01]Ev$', s or '') for s in (sec_slots(c) if c in vt else []))
    if c in vt and nbase:
        dtor_virtual = True
    for i, s in enumerate(vslots):
        if s is None: continue
        if s == '__cxa_pure_virtual':
            if i < nbase:
                bs = prim_slots(primary_base(c))[i]
                if bs and bs != '__cxa_pure_virtual' and not re.search(r'D[01]Ev$', bs):
                    # a concrete base slot re-declared pure here (BAPCombiner's getControls)
                    q, al, const = parse_sig(D(bs)); name = q.split('::')[-1]
                    key = argkey(name, al, const)
                    if key not in seen:
                        seen.add(key)
                        m['virt'].append(dict(name=name, args=al, const=const, pure=True, slot=i, key=key,
                                              addr=None, new=False))
                continue
            # name the new pure slot from a derived override
            found = None
            for d in vt:
                if d != c and c in ancestors(d):
                    q = prim_slots(d)
                    if i < len(q) and q[i] not in (None, '__cxa_pure_virtual') and q[i].startswith('_Z'):
                        found = q[i]; break
            if not found:
                unresolved_slots.append((c, i)); continue
            q, al, const = parse_sig(D(found)); name = q.split('::')[-1]
            key = argkey(name, al, const)
            if key in seen: continue
            seen.add(key)
            m['virt'].append(dict(name=name, args=al, const=const, pure=True, slot=i, key=key, addr=None))
            continue
        dm = D(s)
        q, al, const = parse_sig(dm)
        owner = '::'.join(split_top(q)[:-1]).replace('dice::hfe::ai::', '')
        name = q.split('::')[-1]
        if name.startswith('~'):
            if m['dtor_slot'] is None and not primary_base(c): m['dtor_slot'] = i
            continue
        if owner != c: continue
        key = argkey(name, al, const)
        if key in seen: continue
        seen.add(key)
        m['virt'].append(dict(name=name, args=al, const=const, pure=False, slot=i, key=key,
                              addr=mangled_to_addr.get(s), new=i >= nbase))
    for s in (sec_slots(c) if c in vt else []):
        dm = D(s)
        if not dm.startswith('non-virtual thunk to'): continue
        q, al, const = parse_sig(dm)
        owner = '::'.join(split_top(q)[:-1]).replace('dice::hfe::ai::', '')
        name = q.split('::')[-1]
        if owner != c or name.startswith('~'): continue
        key = argkey(name, al, const)
        if key in seen: continue
        seen.add(key)
        m['virt'].append(dict(name=name, args=al, const=const, pure=False, slot=None, key=key,
                              addr=mangled_to_addr.get(s.replace('_ZThn', '')), new=False, secondary=True))
    for f in sorted(fl, key=lambda f: f['addr']):
        dm = f['dem']
        if 'thunk to' in dm: continue
        q, al, const = parse_sig(dm)
        name = q.split('::')[-1]
        mg = f['mangled']
        if name == short or name == short.split('<')[0]:
            if re.search(r'C2E', mg) and any(re.search(r'C1E', funcs[a]['mangled']) and parse_sig(funcs[a]['dem'])[1] == al for a in cls_funcs[c]):
                continue
            if any(x['args'] == al for x in m['ctors']): continue
            m['ctors'].append(dict(args=al, addr=f['addr']))
            continue
        if name.startswith('~'):
            if m['dtor'] is None or re.search(r'D1Ev', mg): m['dtor'] = dict(addr=f['addr'], virtual=dtor_virtual)
            continue
        key = argkey(name, al, const)
        if key in seen: continue
        seen.add(key)
        m['plain'].append(dict(name=name, args=al, const=const, key=key, addr=f['addr'], f=f))
    if m['dtor'] is None and dtor_virtual:
        m['dtor'] = dict(addr=None, virtual=True)
    return m

IS_BASE = {b for c in scope for b in bases.get(c, [])}
MODELS = {}
order = sorted(TOP, key=lambda c: len(ancestors(c)) if c in vt or bases.get(c) else 0)
for c in order + sorted(NESTED):
    MODELS[c] = model(c)
# return types: bases first
for c in order + sorted(NESTED):
    m = MODELS[c]; DECLS.setdefault(c, {})
    for v in m['virt']:
        r = ret_from_bases(c, v['key']) if not v.get('new', True) or v['pure'] else HAND_RET.get(v['key'])
        ev = 'from the base declaration' if r else ''
        if not r:
            if v['addr'] is not None:
                r, ev = infer_ret(funcs[v['addr']], v['name']); ev = '[INFERRED] ' + ev
            else:
                # pure: take a derived override's evidence
                r, ev = 'void', '[UNRESOLVED] pure slot, no body'
                for d in vt:
                    if d != c and c in ancestors(d):
                        q = prim_slots(d)
                        if v['slot'] is not None and v['slot'] < len(q) and q[v['slot']] in mangled_to_addr:
                            r, ev = infer_ret(funcs[mangled_to_addr[q[v['slot']]]], v['name'])
                            ev = f'[INFERRED] from {d}: ' + ev; break
        v['ret'], v['ev'] = r, ev
        DECLS[c][v['key']] = r
    for p in m['plain']:
        r = HAND_RET.get(p['key'])
        if r: p['ret'], p['ev'] = r, ''
        else:
            p['ret'], ev = infer_ret(p['f'], p['name']); p['ev'] = '[INFERRED] ' + ev
        DECLS[c][p['key']] = p['ret']

# free functions in the control TUs
FREE = collections.defaultdict(list)
for a in sorted(funcs):
    f = funcs[a]
    if f['tu'] in FREE_TUS and f['cls'] == '' and f['bind'] != 'l' and f['dem'].startswith('dice::hfe::ai::'):
        q, al, const = parse_sig(f['dem'])
        r, ev = infer_ret(f, q.split('::')[-1])
        FREE[f['tu'][:-4]].append(dict(name=q.split('::')[-1], args=al, ret=r, ev='[INFERRED] ' + ev, addr=a))

# ---------------------------------------------------------------- dependencies
KNOWN_FWD_AI = {'Bot', 'BotMain', 'Information', 'WeaponInfo', 'IPIControlInfo', 'IPIMobile', 'IPIPhysical',
                'IPIArmament', 'IPIUnit', 'AIStrategicObject', 'CommunicationAgent', 'IAITerrain', 'BotScriptVariable',
                'BAPExpression', 'AIRadioMessageInfo', 'UrgeCurve', 'UrgencyGenerator', 'BAPEntry', 'BAPContext', 'PrettySpecialInfo',
                'BotBehaviour'}
INCLUDE_FOR = {
    'UrgencyGenerator': 'AIBot/UrgencyMerger.h', 'BotScriptVariable': 'AIPlanInterpreter/BAPContext.h',
    'BAPEntry': 'AIPlanInterpreter/BAPEntry.h', 'UrgeCurve': 'AIBotBehaviour/UrgeCurve.h',
}
ENUM_HDR = {'SoldierPose': '../../../AIInterface/BFKnowledge/SoldierPose.h',
            'WeaponGuidingSystem': '../../../AIInterface/BFKnowledge/WeaponTemplate.h',
            'io::PlayerInputMap': '../../../../IO/Input/Input.h'}
TYPES_IN_TYPES_H = {'BAPStatementType', 'BAPConditionType', 'BotLodLevel', 'AvoidSide', 'AIRadioMessageType',
                    'AIRadioChannelType'}
IDENT = re.compile(r'\b((?:io::|world::)?[A-Z]\w*(?:::\w+)?)\b')

def file_of(c):
    return FILE.get(c.split('::')[0]) if c.split('::')[0] in FILE else None

def all_types(m):
    s = []
    for v in m['virt'] + m['plain']:
        s += v['args'] + [v['ret']]
    for k in m['ctors']: s += k['args']
    return s

def rel(fromdir, target):
    return os.path.relpath(os.path.join(AI, target), os.path.join(AI, fromdir))

# ---------------------------------------------------------------- emit
def decl_line(v, cls, virtual):
    args = ', '.join(v['args'])
    pre = 'virtual ' if virtual else ''
    tail = ' const' if v['const'] else ''
    pure = ' = 0' if v.get('pure') else ''
    at = f"0x{v['addr']:X}" if v.get('addr') is not None else 'no body'
    note = f"{at}" + (f", slot +0x{4 * v['slot']:02X}" if v.get('slot') is not None else '') + \
        (f"; {v['ev']}" if v.get('ev') else '')
    return f"    {pre}{v['ret']} {v['name']}({args}){tail}{pure};   // {note}"

def class_text(c, inner=''):
    m = MODELS[c]
    short = c.split('::')[-1]
    bl = bases.get(c, [])
    head = f"class {short}" + (' : ' + ', '.join('public ' + b for b in bl) if bl else '')
    lines = []
    sz = sorted(news.get(c, ()))
    info = []
    if c in vt: info.append(f"vtable 0x{vt[c]['addr']:X} ({(vt[c]['size'] - 8) // 4} words)")
    if sz: info.append('operator new ' + '/'.join(f'0x{x:X}' for x in sz))
    info.append(f"package {pkg.get(c, '?')}")
    if m['doc']: lines.append(f"// {m['doc']}")
    lines.append(f"// ELF: {', '.join(info)}.")
    if c not in HAND_MEMBERS:
        lines.append("// [UNRESOLVED] data members: the owning package names them from the constructors.")
    lines.append(head)
    lines.append('{')
    lines.append('public:')
    if inner: lines.append(inner)
    for k in m['ctors']:
        lines.append(f"    {'explicit ' if len(k['args']) == 1 else ''}{short}({', '.join(k['args'])});   // 0x{k['addr']:X}")
    vs = sorted(m['virt'], key=lambda v: (v.get('slot') is None, v.get('slot') or 0))
    dline = None
    if m['dtor'] is not None:
        if c in INLINE_DTOR:
            dline = f"    {'virtual ' if m['dtor']['virtual'] else ''}~{short}() {{}}   // weak in the image (inline)"
        else:
            at = f"0x{m['dtor']['addr']:X}" if m['dtor']['addr'] else 'no out-of-line body'
            dline = f"    {'virtual ' if m['dtor']['virtual'] else ''}~{short}();   // {at}"
    ds = m['dtor_slot']
    if dline and not (ds and any((v.get('slot') or 0) < ds for v in vs if v.get('slot') is not None)):
        lines.append(dline); dline = None
    if vs: lines.append('')
    for v in vs:
        if dline and v.get('slot') is not None and v['slot'] > ds:
            lines.append(dline + f' slots +0x{4 * ds:02X}/+0x{4 * ds + 4:02X}'); dline = None
        lines.append(decl_line(v, c, True))
    if dline: lines.append(dline + (f' slots +0x{4 * ds:02X}/+0x{4 * ds + 4:02X}' if ds else ''))
    if m['plain']: lines.append('')
    for p in m['plain']: lines.append(decl_line(p, c, False))
    if needs_default_ctor(c):
        lines.append('')
        lines.append('protected:')
        lines.append(f'    // [TEMP] [CHOSEN, not recovered] S0 only: lets the generated derived constructor stubs compile;')
        lines.append(f'    // it stops by name. [RETIRE-WHEN: ai-behaviour-skeleton-ctors] every derived constructor is recovered.')
        lines.append(f'    {short}();')
    if c in HAND_MEMBERS:
        lines.append('')
        lines.append(HAND_MEMBERS[c])
    lines.append('};')
    return '\n'.join(lines)

def needs_default_ctor(c):
    m = MODELS[c]
    return c in IS_BASE and m['ctors'] and not any(not k['args'] for k in m['ctors'])

def nested_text(outer):
    out = []
    for n in sorted(NESTED):
        if n.split('::')[0] == outer:
            t = class_text(n)
            out.append('\n'.join('    ' + l if l else l for l in t.split('\n')))
    return '\n\n'.join(out)

NESTED_TYPES = collections.defaultdict(set)   # outer -> nested names used in signatures
for c in MODELS:
    for t in all_types(MODELS[c]):
        for x in IDENT.findall(t):
            if '::' in x and not x.startswith(('io::', 'world::', 'std::')):
                o, n = x.split('::', 1)
                if o in MODELS and o + '::' + n not in NESTED:
                    NESTED_TYPES[o].add(n)
NESTED_ENUM = {'EndType', 'ValueId', 'Type', 'TransformType', 'DebugLevel', 'DebugPlace'}

GUARD = lambda p: 'AI_AIDLL_AI_' + re.sub(r'\W', '_', p.upper()) + '_H'
HEADER_NOTE = '''// [CHOSEN, not recovered] S0 skeleton, generated by tools/macos/ai_behaviour_skeleton.py from libaidll.so.
// Virtual functions are in ELF vtable slot order; the vtable check (tools/macos/ai_behaviour_vtables.py)
// compares them with the image. Return types marked [INFERRED] come from the ELF bodies and call sites;
// the owning package verifies them. Owner: see docs/macos-ai-behaviour-plan.md (package column).
// [RETIRE-WHEN: ai-behaviour-<package>] the package recovers and validates every body in this file.'''

files = collections.defaultdict(list)
for c in TOP: files[FILE[c]].append(c)
for tu in FREE: files[f'AIPlanInterpreter/{tu}'] += []

out = {}
def emit_header(path, classes):
    d = os.path.dirname(path)
    inc, fwd_ai, fwd_hfe, fwd_ns = set(), set(), set(), collections.defaultdict(set)
    std = set()
    here = set(classes)
    for c in classes:
        m = MODELS[c]
        for b in bases.get(c, []):
            if b in here: continue
            if b in INCLUDE_FOR: inc.add(rel(d, INCLUDE_FOR[b]))
            elif b in FILE: inc.add(rel(d, FILE[b] + '.h'))
        types = all_types(m)
        for n in NESTED:
            if n.split('::')[0] == c: types += all_types(MODELS[n])
        for t in types:
            if 'std::string' in t: std.add('<string>')
            if 'std::ostream' in t: std.add('<iosfwd>')
            if 'std::list' in t: std.add('<list>')
            if 'std::map' in t: std.add('<map>'); std.add('<utility>')
            for x in IDENT.findall(re.sub(r'std::\w+', '', t)):
                if x in ENUM_HDR: inc.add(ENUM_HDR[x]); continue
                if x in ('BAPVariable', 'BAPConOpLessThan'):
                    inc.add(rel(d, f'AIBotActionPlan/{x}.h')); continue
                if x in TYPES_IN_TYPES_H or x in ('BAPStatementType', 'BAPConditionType'):
                    inc.add(rel(d, TYPES_H + '.h')); continue
                if x.startswith('io::'): fwd_ns['io'].add(x[4:]); continue
                if x.startswith('world::'): fwd_ns['world'].add(x[7:]); continue
                if x in ('Vec3', 'Vec2'): inc.add(rel(d, '../../../Math/Vec3.h')); continue
                if x == 'Mat4': fwd_hfe.add(x); continue
                o = x.split('::')[0]
                if '::' in x:
                    if o in here: continue
                    if o in FILE: inc.add(rel(d, FILE[o] + '.h'))
                    continue
                if x in here: continue
                if x in MODELS or x in KNOWN_FWD_AI or x in FILE: fwd_ai.add(x)
    ordered = []
    def visit(c):
        if c in ordered or c not in here: return
        for b in bases.get(c, []): visit(b)
        ordered.append(c)
    for c in sorted(classes, key=lambda c: vt[c]['addr'] if c in vt else 0): visit(c)
    L = [HEADER_NOTE, '', f'#ifndef {GUARD(path)}', f'#define {GUARD(path)}', '']
    for s in sorted(std): L.append(f'#include {s}')
    for s in sorted(inc): L.append(f'#include "{s}"')
    L += ['', 'namespace dice {', 'namespace hfe {', '']
    for x in sorted(fwd_hfe): L.append(f'struct {x};')
    for ns, xs in sorted(fwd_ns.items()):
        L.append(f'namespace {ns} {{ ' + ' '.join(f'class {x};' for x in sorted(xs)) + ' }')
    L += ['namespace ai {', '']
    for x in sorted(fwd_ai - here):
        L.append(f'{"struct" if x in ("PrettySpecialInfo", "AIRadioMessageInfo") else "class"} {x};')
    L.append('')
    for c in ordered:
        if c in CONTROL_NS:
            L.append(control_ns_text(c)); L.append(''); continue
        inner = []
        for n in sorted(NESTED_TYPES.get(c, ())):
            inner.append(f'    enum {n} : int;   // [UNRESOLVED] enumerators' if n in NESTED_ENUM
                         else f'    struct {n};   // [UNRESOLVED] layout')
        nt = nested_text(c)
        if nt: inner.append(nt)
        L.append(class_text(c, '\n'.join(inner))); L.append('')
    L += ['} // namespace ai', '} // namespace hfe', '} // namespace dice', '', f'#endif // {GUARD(path)}', '']
    out[path + '.h'] = '\n'.join(L)

def control_ns_text(c):
    m = MODELS[c]
    L = [f'// [INFERRED] a namespace: no vtable, no constructor, and the first argument is never `this`.',
         f'namespace {c}', '{']
    for p in m['plain']:
        L.append(decl_line(dict(p, const=False), c, False))
    L.append('}')
    return '\n'.join(L)

def stub(ret, qual, args, const, addr):
    a = ', '.join(args)
    return (f"// ELF 0x{addr:X}\n" if addr else '') + \
        f"{ret + ' ' if ret is not None else ''}{qual}({a}){' const' if const else ''}\n{{\n    BF2_AI_FRONTIER(\"dice::hfe::ai::{qual}\");\n}}\n"

def emit_cpp(path, classes):
    d = os.path.dirname(path)
    L = [HEADER_NOTE.replace('every body in this file', 'every body in this file'), '',
         f'#include "{os.path.basename(path)}.h"', '',
         f'#include "{rel(d, "Main/AIDllFrontier.h")}"', f'#include "{rel(d, "../../../Math/Vec3.h")}"']
    if any('Mat4' in t for c in classes for t in all_types(MODELS[c])):
        L.append(f'#include "{rel(d, "../../../Math/Mat4.h")}"')
    L += ['', '#include <string>', '', 'namespace dice {', 'namespace hfe {', 'namespace ai {', '']
    n = 0
    for c in classes:
        if c == 'BAPStatement':
            L += ['int BAPStatement::news = 0;', 'int BAPStatement::destroys = 0;', '']
        targets = [c] + sorted(x for x in NESTED if x.split('::')[0] == c)
        for t in targets:
            m = MODELS[t]; short = t.split('::')[-1]
            for k in m['ctors']:
                L.append(stub(None, f'{t}::{short}', k['args'], False, k['addr'])); n += 1
            if t in MODELS and needs_default_ctor(t):
                L.append(f'// [TEMP] S0 skeleton default constructor (see the header).\n{t}::{short}()\n{{\n'
                         f'    BF2_AI_FRONTIER("dice::hfe::ai::{t}::{short} (S0 skeleton default constructor: a derived constructor is not recovered)");\n}}\n'); n += 1
            if m['dtor'] is not None and t not in INLINE_DTOR:
                L.append(stub(None, f'{t}::~{short}', [], False, m['dtor']['addr'])); n += 1
            for v in sorted(m['virt'], key=lambda v: v.get('slot') or 0):
                if v.get('pure'): continue
                L.append(stub(v['ret'], f'{t}::{v["name"]}', v['args'], v['const'], v['addr'])); n += 1
            for p in m['plain']:
                L.append(stub(p['ret'], f'{t}::{p["name"]}', p['args'], p['const'], p['addr'])); n += 1
    L += ['} // namespace ai', '} // namespace hfe', '} // namespace dice', '']
    if n: out[path + '.cpp'] = '\n'.join(L)

for path, classes in sorted(files.items()):
    if classes: emit_header(path, classes); emit_cpp(path, classes)

# control TUs with free functions
for tu, fl in FREE.items():
    path = f'AIPlanInterpreter/{tu}'
    L = [HEADER_NOTE, '', f'#ifndef {GUARD(path)}', f'#define {GUARD(path)}', '',
         '#include "../../../AIInterface/BFKnowledge/SoldierPose.h"', '#include "../../../../Math/Vec3.h"', '',
         'namespace dice {', 'namespace hfe {', 'struct Mat4;',
         'namespace io { class PlayerInput; }', 'namespace ai {', '',
         'class Bot;', 'class BAPAMoveTo;', 'class IPIControlInfo;', 'class IPIMobile;', 'class IPIPhysical;',
         'class Information;', '']
    C = [HEADER_NOTE, '', f'#include "{tu}.h"', '', '#include "../Main/AIDllFrontier.h"',
         '#include "../../../../Math/Vec3.h"', '#include "../../../../Math/Mat4.h"', '',
         'namespace dice {', 'namespace hfe {', 'namespace ai {', '']
    for p in fl:
        L.append(f"{p['ret']} {p['name']}({', '.join(p['args'])});   // 0x{p['addr']:X}; {p['ev']}")
        C.append(stub(p['ret'], p['name'], p['args'], False, p['addr']).replace('dice::hfe::ai::' + p['name'], 'dice::hfe::ai::' + p['name']))
    L += ['', '} // namespace ai', '} // namespace hfe', '} // namespace dice', '', f'#endif // {GUARD(path)}', '']
    C += ['} // namespace ai', '} // namespace hfe', '} // namespace dice', '']
    out[path + '.h'] = '\n'.join(L); out[path + '.cpp'] = '\n'.join(C)

# ---------------------------------------------------------------- vtable manifest
man = {}
for c in sorted(set(scope) | ENTRY_IN_TREE):
    if c not in vt: continue
    ent = []
    for s in vt[c]['slots']:
        if s is None or s.startswith('0x') or s.startswith('_ZTI'): ent.append('-'); continue
        if s == '__cxa_pure_virtual': ent.append('pure'); continue
        dm = D(s)
        thunk = dm.startswith('non-virtual thunk to')
        q = qualname(re.sub(r'^(non-)?virtual thunk to ', '', dm)).replace('dice::hfe::ai::', '')
        if re.search(r'D1Ev$', s): q += ' [complete]'
        if re.search(r'D0Ev$', s): q += ' [deleting]'
        ent.append(('thunk ' if thunk else '') + q)
    man[c] = dict(vtable=f"0x{vt[c]['addr']:X}", slots=ent, bases=bases.get(c, []),
                  header=(FILE[c] + '.h') if c in FILE else ('AIPlanInterpreter/' + c + '.h' if c.startswith('Entry') else None))
man_text = json.dumps(dict(source='libaidll.so 1.5.3153 (ia-32)', unresolved_pure_slots=[f'{c}+0x{4*i:X}' for c, i in unresolved_slots],
                           classes=man), indent=1)

if os.environ.get('SHOW'):
    for p in os.environ['SHOW'].split(','): print(out.get(p, '(none)'))
print('classes', len(MODELS), 'files', len(out), 'unresolved pure slots', unresolved_slots)
if WRITE:
    for p, t in out.items():
        fp = os.path.join(AI, p)
        os.makedirs(os.path.dirname(fp), exist_ok=True)
        if os.path.exists(fp):
            print('exists, skipped:', p); continue
        open(fp, 'w').write(t)
    open(os.path.join(ROOT, 'tools/macos/ai_behaviour_vtables.json'), 'w').write(man_text + '\n')
    open(os.path.join(WD, 'skeleton_files.txt'), 'w').write('\n'.join(sorted(out)) + '\n')
