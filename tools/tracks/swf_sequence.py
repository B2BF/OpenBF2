#!/usr/bin/env python3
"""Round-two alignment helpers; [CHOSEN, not recovered] offline analysis policy.

Source and decompiler text stay in memory. Only identifiers, addresses, numeric
scores and hashes may leave this module. A sequence score is a naming proposal,
never proof of body equivalence. No upstream text is executed.
"""

from collections import Counter, defaultdict
import hashlib
import math
import re


def ordered_alignment(left, right, score, gap=-3.0):
    """Global gapped alignment with deterministic ties (gap before weak match).

    Returns index pairs and the total score. Both sides can have gaps; a retail
    gap does not establish a DICE addition, nor does a source gap prove inlining.
    """
    n, m = len(left), len(right)
    trace = [bytearray(m + 1) for _ in range(n + 1)]
    previous = [j * gap for j in range(m + 1)]
    for j in range(1, m + 1):
        trace[0][j] = 2
    for i in range(1, n + 1):
        current = [i * gap] + [0.0] * m
        trace[i][0] = 1
        for j in range(1, m + 1):
            choices = [(previous[j] + gap, 1), (current[j - 1] + gap, 2),
                       (previous[j - 1] + score(left[i - 1], right[j - 1]), 3)]
            best, step = max(choices, key=lambda pair: (pair[0], -pair[1]))
            current[j], trace[i][j] = best, step
        previous = current
    result, i, j = [], n, m
    while i or j:
        step = trace[i][j]
        if step == 3:
            result.append((i - 1, j - 1))
            i, j = i - 1, j - 1
        elif step == 1:
            i -= 1
        else:
            j -= 1
    return list(reversed(result)), previous[m]


def number_constants(text):
    """Numeric features only; skip comments and quoted data before tokenizing."""
    text = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  ' ', text, flags=re.S)
    result = set()
    for match in re.finditer(r'(?<![\w.])(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?)(?![\w.])', text):
        token = match[0]
        try:
            value = float(token) if '.' in token else int(token, 0) if token.startswith('0x') else int(token)
        except ValueError:
            continue
        # Small constants and pointers are weak or compiler-layout features.
        if abs(value) > 4 and not 0x10000000 <= value < 0x10100000:
            result.add(value)
    return result


def sequence_proposals(sa, sources, funcs, strings, matches, by_name, decomp):
    """Align between monotone same-TU anchors; report inversions explicitly.

    Anchor inversions split islands. COMDAT methods are excluded from the TU
    source sequence. Strong proposals need independent literal/callee evidence;
    the remainder stay candidates even if the DP selected a unique path.
    """
    grouped = defaultdict(list)
    for key, source in sorted(sources.items()):
        if source.out_of_line:
            grouped[source.file].append(key)
    used = {value[0] for value in matches.values()}
    graph = {key: sa.resolved_calls(source, sources, by_name) for key, source in sources.items()}
    branchless = {}
    for key, source in sources.items():
        code, _ = sa.BL.blank_code(source.body)
        branchless[key] = not source.calls and not re.search(r'\b(if|for|while|switch)\b|\?', code)
    known_calls = {a: {matches[c][0] for c in f['callees'] if c in matches} for a, f in funcs.items()}
    snums = {key: number_constants(s.body) for key, s in sources.items()}
    rnums = {a: number_constants(text) for a, text in decomp.items()}
    rstrings = {a: {strings[p] for p in f['strings'] if p in strings} for a, f in funcs.items()}
    literal_use = Counter(value for s in sources.values() for value in s.strings)
    order_audit, proposals = [], []
    for file, keys in sorted(grouped.items()):
        keys.sort(key=lambda key: (sources[key].line, key))
        positions = {key: i for i, key in enumerate(keys)}
        anchors = sorted((a, positions[k]) for a, (k, _, _) in matches.items() if k in positions)
        if len(anchors) < 2:
            continue
        ratios = [funcs[a]['ninstr'] / max(sources[keys[i]].tokens, 1) for a, i in anchors
                  if funcs[a]['ninstr'] >= 10 and sources[keys[i]].tokens >= 10]
        ratio = sorted(ratios)[len(ratios) // 2] if ratios else 0.45

        def features(key, a):
            source, f = sources[key], funcs[a]
            shared_literals = {v for v in source.strings & rstrings[a] if len(v) >= 4}
            literal_weight = sum(1 / literal_use[v] for v in sorted(shared_literals))
            shared_calls = graph[key] & known_calls[a]
            size_fit = abs(math.log2(max(f['ninstr'], 1) / max(source.tokens * ratio, 1)))
            count_fit = abs(len(source.calls) - len(f['callees'])) / max(len(source.calls), len(f['callees']), 1)
            constants = snums[key] & rnums.get(a, set())
            score = (min(literal_weight, 3) * 9 + min(len(shared_calls), 4) * 4
                     + min(len(constants), 3) * 0.6 + 1.5 - min(size_fit, 5) * 2 - count_fit)
            if branchless[key] and f.get('branch_ops', 0):
                score -= 5
            return score, len(shared_literals), len(shared_calls), len(constants), size_fit

        for (first, lo), (last, hi) in zip(anchors, anchors[1:]):
            inversion = hi <= lo
            order_audit.append(dict(file=file, first=f'0x{first:08X}', last=f'0x{last:08X}',
                                    first_line=sources[keys[lo]].line, last_line=sources[keys[hi]].line,
                                    decision='inversion:split-island' if inversion else 'monotone:align-gaps'))
            if inversion:
                continue
            left = [k for k in keys[lo + 1:hi] if k not in used]
            right = [a for a in sorted(funcs) if first < a < last and a not in matches
                     and not funcs[a].get('alignment_rejected')]
            if not left or not right:
                continue
            matrix = {(k, a): features(k, a) for k in left for a in right}
            pairs, _ = ordered_alignment(left, right, lambda k, a: matrix[k, a][0])
            for i, j in pairs:
                key, a = left[i], right[j]
                score, nl, nc, nn, distance = matrix[key, a]
                # Distinct evidence is needed beyond order/size. Margin is local
                # competition, not a probability or an equivalence confidence.
                alternate = max([matrix[k, a][0] for k in left if k != key] + [-100])
                margin = score - alternate
                strong = score >= 10 and margin >= 3 and (nl >= 1 or nc >= 2)
                proposals.append(dict(address=f'0x{a:08X}', key=key, symbol=sources[key].name,
                                      file=file, score=f'{score:.3f}', margin=f'{margin:.3f}',
                                      literals=nl, callees=nc, constants=nn, size_distance=f'{distance:.3f}',
                                      first=f'0x{first:08X}', last=f'0x{last:08X}',
                                      decision='supported-name-proposal' if strong else 'candidate-only'))
    # A function may be inside overlapping TU envelopes. Conflicting proposals
    # remain visible and cannot seed graph propagation.
    offered = defaultdict(set)
    for row in proposals:
        if row['decision'] == 'supported-name-proposal':
            offered[int(row['address'], 16)].add(row['key'])
    for row in proposals:
        a = int(row['address'], 16)
        if row['decision'] == 'supported-name-proposal' and len(offered[a]) == 1 and a not in matches:
            matches[a] = (row['key'], 'sequence:literal-or-two-callees:' + row['first'] + '-' + row['last'], 'medium')
        elif len(offered[a]) > 1:
            row['decision'] = 'conflict:overlapping-TU-islands'
    return sorted(proposals, key=lambda r: (r['address'], r['file'], r['key'])), order_audit


def decompile_shape(text, mask_numbers=True):
    """Canonical text shape for grouping, not a semantic-equivalence predicate.

    Keep variable identity, casts, signs, operations and call repetition. Masked
    constants and addresses can change behavior: a shared hash alone never
    assigns a name/classification. Review rules must validate each variant.
    """
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
    text = tokens.sub(lambda m: '' if m[0].startswith(('//', '/*')) else m[0], text)
    text = text[text.find('{'):] if '{' in text else text
    aliases = {}
    def rename(m):
        token = m[0]
        family = 'call' if token.startswith('FUN_') else 'data' if re.match(r'_?(?:DAT|PTR)_', token) else 'var'
        pair = family, token
        if pair not in aliases:
            aliases[pair] = family + str(sum(k[0] == family for k in aliases))
        return aliases[pair]
    parts = re.split(r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')', text)
    for i in range(0, len(parts), 2):
        code = re.sub(r'\b(?:FUN_[0-9a-fA-F]+|_?DAT_[\w]+|PTR_[\w]+|LAB_[\w]+|'
                      r'param_[\w]+|local_[\w]+|[a-z]+Var\d+)\b', rename, parts[i])
        if mask_numbers:
            code = re.sub(r'\b(?:0x[0-9a-fA-F]+|\d+)\b', 'N', code)
        parts[i] = re.sub(r'\s+', '', code)
    return ''.join(parts)


def shape_id(text, mask_numbers=True):
    return hashlib.sha256(decompile_shape(text, mask_numbers).encode()).hexdigest()[:20]


def shape_groups(decomp):
    groups = defaultdict(list)
    for a, text in sorted(decomp.items()):
        groups[shape_id(text)].append(a)
    return groups


def reviewed_vtables(sa, path, vtables, funcs, rows, sources=None, decomp=None):
    """Expand reviewed primary tables, leaving unknown slots explicit.

    Each manifest row fixes a table's extent and method order. Shared inherited
    addresses retain existing identities. vtwrites identify constructor or
    destructor candidates; writes alone cannot distinguish the two.
    """
    tables = {v['addr']: v for v in vtables}
    at = {int(r['address'], 16): r for r in rows}
    audit = []
    for layout in sa.read_csv(path):
        table = tables[int(layout['vtable'], 16)]
        methods = layout['methods'].split(';')
        if len(methods) != int(layout['slots']) or len(methods) != len(table['slots']):
            raise ValueError('reviewed vtable extent changed: ' + layout['vtable'])
        writers = [a for a, f in sorted(funcs.items()) if table['addr'] in f['vtwrites']]
        for slot, (a, method) in enumerate(zip(table['slots'], methods)):
            row = at.get(a)
            symbol = layout['class'] + '::' + method if method != '-' else ''
            audit.append(dict(vtable=layout['vtable'], slot=slot, address=f'0x{a:08X}',
                              symbol=symbol, decision='reviewed-name;body-separate' if symbol else 'unresolved-slot',
                              writers=';'.join(f'0x{x:08X}' for x in writers)))
            if row and symbol and row['symbol'].startswith('FUN_'):
                row.update(symbol=symbol, file=layout['file'], file_inferred=1,
                           confidence='medium', anchor='reviewed-vtable:' + layout['vtable'] + ':' + str(slot),
                           reason=layout['reason'] + ';name-only;body-not-compared')
            if (row and symbol and row['match'] == 'unknown' and 'review' not in row['anchor'].split(';')
                    and sources and decomp and a in decomp):
                source_defs = [s for s in sources.values() if s.name.endswith('::' + method)
                               and not s.out_of_line]
                # Only trivial emitted definitions: do not turn a vtable name
                # into an unchanged claim about a substantial body.
                body = decompile_shape(decomp[a], False)
                simple_return = re.fullmatch(r'\{return(?:;|(?:0|1|0xffffffff);)\}', body)
                accessor = (funcs[a]['ninstr'] <= 3 and re.fullmatch(
                    r'\{return(?:\*var\d+|\*\((?:undefined[1248]|int|float|byte)\*\)'
                    r'\(var\d+\+(?:0x[0-9a-f]+|\d+)\)|var\d+\+(?:0x[0-9a-f]+|\d+));\}', body))
                compatible = []
                for s in source_defs:
                    code, _ = sa.BL.blank_code(s.body)
                    code = re.sub(r'\bassert\s*\([^;]*\)\s*;', '', code)
                    clean = re.sub(r'\s+', '', code)
                    clean = clean.replace('NULL', '0').replace('false', '0').replace('true', '1')
                    clean = clean.replace('STOP', '1')
                    calls = [call for call in s.calls if call != 'assert']
                    if simple_return and not calls and clean == body:
                        compatible.append(s)
                    elif accessor and not calls and re.fullmatch(r'\{return(?:m_\w+|this);\}', clean):
                        compatible.append(s)
                if compatible:
                    row.update(match='inline', confidence='medium',
                               reason=layout['reason'] + ';emitted-trivial-inline-definition;not-body-equivalence')
    return audit


def template_guard(gate, text, f):
    """Validate the non-shape features required by a reviewed template family."""
    calls = set(f['callees'])
    if f['vtwrites'] or f['strings']:
        return False
    if gate == 'allocation':
        return bool(calls) and calls <= {0x1006DB60, 0x1006DB80, 0x1006DBA0, 0x1006DBB0, 0x100130A0}
    if gate == 'deleting-destructor':
        return bool(calls & {0x1006DBA0, 0x1006DBB0}) and bool(re.search(r'& 1\)', text))
    if gate == 'empty-entry':
        return bool(re.search(r'== -2\s*;', text))
    if gate == 'hash':
        return '-2' in text or '0xfffffffe' in text
    if gate == 'hash-expand':
        return '* 3' in text and '* 2' in text and '0x10' in text
    if gate == 'array-access':
        return not calls and '*(int *)this' in text
    if gate == 'refcount':
        return '*piVar1 = *piVar1 + ' in text or bool(calls & {0x1006F290, 0x1006F2A0})
    if gate == 'iterator':
        return not calls
    if gate == 'array-grow':
        return bool(calls - {0x1006DBA0, 0x1006DBB0})
    raise ValueError('unknown template guard: ' + gate)


def template_instances(sa, path, decomp, funcs, rows):
    """Classify guarded instances of manually compared template shapes.

    Type arguments stay T/K/V. Numeric size is an observed stride, not a recovered
    C++ type. A different hash/callee/constant contract rejects the instance.
    """
    at = {int(r['address'], 16): r for r in rows}
    groups = shape_groups(decomp)
    audit = []
    for rule in sa.read_csv(path):
        exemplar = int(rule['exemplar'], 16)
        if exemplar not in decomp:
            raise ValueError('template exemplar unavailable: ' + rule['exemplar'])
        digest = shape_id(decomp[exemplar])
        for a in groups[digest]:
            if a not in at:
                continue
            row, text = at[a], decomp[a]
            accepted = template_guard(rule['gate'], text, funcs[a])
            # Preserve constants as an audit fingerprint, never as copied body text.
            exact = shape_id(text, False)
            multipliers = [int(x, 0) for x in re.findall(r'\* (0x[0-9a-f]+|\d+)(?![\w])', text)]
            stride = next((v for v in multipliers if 4 <= v <= 512), None)
            symbol = rule['family'] + (' sz=' + str(stride) if stride else '') + '>::' + rule['operation']
            audit.append(dict(address=row['address'], exemplar=rule['exemplar'], shape=digest,
                              variant=exact, symbol=symbol, decision='guarded-inline-instance' if accepted else 'rejected-variant'))
            if accepted and row['match'] == 'unknown' and 'review' not in row['anchor'].split(';'):
                row.update(symbol=symbol if row['symbol'].startswith('FUN_') else row['symbol'],
                           file=rule['file'], base_rev='release-2004-05-12-approx', match='inline',
                           confidence='medium', file_inferred=1,
                           anchor=(row['anchor'] + ';template:' + rule['exemplar']).strip(';'),
                           reason='[INFERRED]:template-family;type-arguments-open;' + rule['reason'])
    return sorted(audit, key=lambda r: (r['address'], r['exemplar']))


def structural_templates(decomp, funcs, rows):
    """Recognize container operations with explicit field/control-flow contracts.

    This expands the exact-shape families across Ghidra's type/cast variants.
    The result is an inferred template role with unknown type arguments, never
    an unchanged classification. Simple constructors/accessors need a nearby
    corroborated container instance; size alone supplies no identity.
    """
    at = {int(r['address'], 16): r for r in rows}
    audit = []
    alloc = {0x1006DB60, 0x1006DB80, 0x1006DBA0, 0x1006DBB0, 0x100130A0}
    for _ in range(3):
        additions = []
        for a, r in sorted(at.items()):
            if r['match'] != 'unknown' or a not in decomp or 'review' in r['anchor'].split(';'):
                continue
            f, text = funcs[a], decomp[a]
            if f['vtwrites'] or f['strings']:
                continue
            shape = decompile_shape(text, False)
            calls = set(f['callees'])
            named = [at[c]['symbol'] for c in sorted(calls) if c in at]
            family, operation, gate = '', '', ''
            file = 'Code/BF2/External/base/container.h'
            # Reserve has only allocator effects and writes the capacity field.
            if (calls and calls <= alloc and '*(int *)((int)this + 8) = param_1;' in text
                    and '*(int *)((int)this + 4) = param_1;' not in text
                    and ('FUN_100130a0' in text or 'FUN_100130A0' in text)):
                family, operation, gate = 'array<T', 'reserve', 'capacity-eight;allocator-only;realloc-old-and-new-sizes'
            elif ('*(int *)((int)this + 4) = param_1;' in text
                  and '>> 2' in text and '>> 1' in text
                  and '((int)this + 8)' in text and calls
                  and not re.search(r'\bswitch\b', text)):
                family, operation, gate = 'array<T', 'resize', 'size-four;capacity-eight;quarter-growth;half-capacity-shrink'
            elif ('0xfffffffe' in text or '!= -2' in text or '== -2' in text):
                if 'log2(' in text and calls & alloc and 'FUN_100130a0' not in text:
                    family, operation, gate = 'hash<K;V', 'set_raw_capacity', 'power-two-capacity;minus-two-empty;allocate-and-rehash'
                elif ('**(int **)this = **(int **)this + 1;' in text
                      and '&' in text and ('0xffffffff' in text or '-1' in text)):
                    family, operation, gate = 'hash<K;V', 'add', 'entry-count-increment;minus-two-empty;masked-coalesced-chain'
                elif ('return 0xffffffff;' in text and '&' in text and 'return' in text
                      and not re.search(r'\*[^;\n]*= 0xfffffffe;', text)):
                    family, operation, gate = 'hash<K;V', 'find_index', 'minus-one-miss;minus-two-empty;mask-and-chain-search'
                elif (calls and calls <= alloc and 'while' in text
                      and ('= 0xfffffffe;' in text)):
                    family, operation, gate = 'hash<K;V', 'clear_or_destruct', 'walk-live-minus-two-entries;allocator-only-release'
            # Explicit calls to a classified operation supply identity evidence.
            if not family and len(calls) == 1:
                target = at.get(next(iter(calls)))
                if target and target['symbol'].startswith('array<') and target['symbol'].endswith('::resize'):
                    if re.fullmatch(r'\{call0\(var0,0\);return;\}', shape):
                        family, operation, gate = 'array<T', 'clear', 'single-known-array-resize-with-zero'
                    elif re.fullmatch(r'\{call0\(var0,\*\(int\*\)\(\(int\)var0\+4\)\+-1\);return;\}', shape):
                        family, operation, gate = 'array<T', 'pop_back', 'known-array-resize-size-minus-one'
                if any(n.startswith('hash<') and n.endswith('::find_index') for n in named):
                    if 'if (param_2 != ' in text and 'return CONCAT31' in text and '0xffffff00' in text:
                        family, operation, gate = 'hash<K;V', 'get', 'known-find-index;optional-output;Boolean-result'
            if not family:
                near = [row for b, row in sorted(at.items()) if 0 < abs(a - b) <= 64
                        and row['symbol'].startswith('array<') and row['match'] == 'inline']
                if near and shape == '{*var0=0;var0[1]=0;var0[2]=0;return;}':
                    family, operation, gate = 'array<T', 'array', 'zero-buffer-size-capacity;adjacent-array-instance'
                elif near and shape == '{return*(undefined4*)(var0+4);}':
                    family, operation, gate = 'array<T', 'size', 'size-field-four;adjacent-array-instance'
            if family:
                multipliers = [int(x, 0) for x in re.findall(r'\* (0x[0-9a-f]+|\d+)(?![\w])', text)]
                stride = next((n for n in multipliers if 4 <= n <= 512), None)
                if family.startswith('array<') and operation == 'reserve' and not stride:
                    shifts = re.findall(r'param_1 << ([1-6])', text)
                    stride = 1 << int(shifts[0]) if shifts else 1
                symbol = family + (' sz=' + str(stride) if stride else '') + '>::' + operation
                additions.append((a, symbol, file, gate))
        if not additions:
            break
        for a, symbol, file, gate in additions:
            r = at[a]
            audit.append(dict(address=r['address'], symbol=symbol, rule=gate, decision='inferred-inline-role'))
            r.update(symbol=symbol if r['symbol'].startswith('FUN_') else r['symbol'], file=file,
                     match='inline', base_rev='release-2004-05-12-approx', confidence='medium', file_inferred=1,
                     anchor=(r['anchor'] + ';template-structure').strip(';'),
                     reason='[INFERRED]:template-role;type-arguments-open;' + gate)
    return sorted(audit, key=lambda r: r['address'])


def callback_bindings(decomp, rows):
    """Identify native callbacks only from an explicit value/key/binding triple.

    The source-level function/class name can remain unknown; the registered
    ActionScript member is an honest identifier for planning. Never accept a
    nearby string without the subsequent set-member call.
    """
    at = {int(r['address'], 16): r for r in rows}
    audit, proposals = [], defaultdict(set)
    for a, text in sorted(decomp.items()):
        pending, key = None, None
        for line in text.splitlines():
            m = re.search(r'=\s*(?:CONCAT44\([^,]+,\s*)?FUN_([0-9a-fA-F]{8})\)?\s*;', line)
            if m:
                pending, key = int(m[1], 16), None
            m = re.search(r'FUN_10007680\([^,]+,"([A-Za-z_]\w*)"\);', line, re.I)
            if m:
                key = m[1] if pending else None
            binding = (re.search(r'FUN_(?:1009a2a0|1000af80)\(', line, re.I)
                       or re.search(r'\+ 0x20\)\)', line))
            if pending and key and binding:
                symbol = f'gameswf::ASCallback<registrar=0x{a:08X}>::{key}'
                proposals[pending].add(symbol)
                audit.append(dict(address=f'0x{pending:08X}', registrar=f'0x{a:08X}', member=key,
                                  decision='explicit-native-value-key-binding;name-only'))
                pending, key = None, None
    for a, names in sorted(proposals.items()):
        r = at.get(a)
        if r and len(names) == 1 and r['symbol'].startswith('FUN_') and 'review' not in r['anchor'].split(';'):
            r.update(symbol=next(iter(names)), confidence='high',
                     anchor=(r['anchor'] + ';native-member-binding').strip(';'),
                     reason='[INFERRED]:source-function-name;registered-callback-identity;body-uncompared')
    return audit


CHANGE_REASONS = ('extra-call', 'missing-call', 'literal', 'branch', 'signature', 'dice-member')


def change_reason(row):
    """Planning tag from an explicit reviewed difference, not a new body claim."""
    if row['match'] != 'changed':
        return ''
    reason = row['reason']
    prefix = reason.split(':', 1)[0]
    if prefix in CHANGE_REASONS:
        return prefix
    for fragment, code in [('constructor-now-receives', 'signature'), ('omits-', 'missing-call'),
                           ('movie-add-init-action', 'dice-member'), ('adds-tag62', 'extra-call'),
                           ('adds-prototype', 'dice-member'), ('event-spelling', 'literal'),
                           ('GetAsyncKeyState', 'extra-call'), ('adds-hexadecimal', 'branch'),
                           ('adds-Boolean', 'branch'), ('colon-and-dot', 'branch')]:
        if fragment in reason:
            return code
    raise ValueError('changed row has no reviewed reason code: ' + row['address'])


def pilot_scope(rows, funcs, slots):
    """Inventory-backed pilot closure, including shared virtual dispatch targets.

    Core includes the entire button TU, root and sprite virtual slots, and named
    event/action helpers. Include each core routine's direct B callees, plus the
    interpreter explicitly. Virtual dispatch is represented by the reviewed slot
    maps; this is not a claim that direct-call exports form a complete call graph.
    """
    at = {int(r['address'], 16): r for r in rows}
    reasons = defaultdict(set)
    for a in at:
        if 0x100A1BE0 <= a <= 0x100A3B70:
            reasons[a].add('button-TU')
    for slot in slots:
        if slot['vtable'] in ('0x100DF780', '0x100E0C70', '0x100E2F38'):
            reasons[int(slot['address'], 16)].add('virtual:' + slot['vtable'] + ':' + str(slot['slot']))
    for a in (0x10073250, 0x10073560, 0x10077910, 0x1007AED0, 0x1007F310, 0x10080010,
              0x10081480, 0x10081620, 0x10081830, 0x10081870, 0x10081B10,
              0x10081CF0, 0x1008BA90, 0x1008E990, 0x1008EAA0, 0x100925A0, 0x100961E0):
        reasons[a].add('mouse-event-action-core')
    core = sorted(reasons)
    for a in core:
        for child in funcs.get(a, {}).get('callees', []):
            if child in at:
                reasons[child].add('direct-callee:' + f'0x{a:08X}')
    return [dict(address=at[a]['address'], size=at[a]['size'], ninstr=at[a]['ninstr'],
                 symbol=at[a]['symbol'], file=at[a]['file'], match=at[a]['match'],
                 confidence=at[a]['confidence'], change_reason=at[a].get('change_reason', ''),
                 scope=';'.join(sorted(why)),
                 open_item='identity/body-review' if at[a]['match'] == 'unknown' else 'S2-instruction-and-signature-validation')
            for a, why in sorted(reasons.items()) if a in at]
