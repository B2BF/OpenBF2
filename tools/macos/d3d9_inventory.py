#!/usr/bin/env python3
"""[CHOSEN, not recovered] Lexical D3D9 seam inventory; no SDK or compiler needed.

Run from any directory. All paths and ordering are repository relative. This scans
source, including inactive platform/editor arms, rather than claiming runtime reach.
Unknown receivers remain visible; it is deliberately not a C++ type checker.
"""

import argparse
import bisect
import collections
import csv
import hashlib
import json
from pathlib import Path
import random
import re
import time

ROOT = Path(__file__).resolve().parents[2]
ROOTS = ('Code/BF2/RendDX9/', 'Code/BF2/Geom/', 'Code/BF2/GeomMisc/',
         'Code/BF2/RendMisc/', 'Code/RendDX9Abi/')
EXTENSIONS = {'.cpp', '.cxx', '.cc', '.c', '.h', '.hpp', '.inl', '.inc'}
# Source-local SDK prefixes whose contracts are declared in the scanned sources.
ALIASES = {'SplineD3dxMesh': 'ID3DXMesh', 'RendD3dxMesh': 'ID3DXMesh',
           'LightmapD3dxMesh': 'ID3DXMesh', 'LodD3DXMesh': 'ID3DXMesh',
           'LodD3DXBuffer': 'ID3DXBuffer', 'ProbeD3DXBuffer': 'ID3DXBuffer',
           'RenderToSurface': 'ID3DXRenderToSurface', 'IHemiD3DXEffect': 'ID3DXEffect'}
TYPE = r'(?:IDirect3D\w*9(?:Ex)?|ID3DX\w+|IUnknown|LPDIRECT3D\w+|LPD3DX\w+|' + '|'.join(ALIASES) + ')'
TYPE_RE = re.compile(r'\b' + TYPE + r'\b')
MENTION = re.compile(r'\b(?:IDirect3D\w*|ID3DX\w*|LPDIRECT3D\w*|LPD3DX\w*|D3D[A-Z_0-9]\w*|Direct3DCreate9)\b')
# Full D3D9 interface vocabulary, including inherited resource/IUnknown methods.
# This only selects candidates; a method name never establishes a receiver type.
METHODS = set('''QueryInterface AddRef Release RegisterSoftwareDevice GetAdapterCount
GetAdapterIdentifier GetAdapterModeCount EnumAdapterModes GetAdapterDisplayMode
CheckDeviceType CheckDeviceFormat CheckDeviceMultiSampleType CheckDepthStencilMatch
CheckDeviceFormatConversion GetDeviceCaps GetAdapterMonitor CreateDevice
TestCooperativeLevel GetAvailableTextureMem EvictManagedResources GetDirect3D
GetDisplayMode GetCreationParameters SetCursorProperties SetCursorPosition ShowCursor
CreateAdditionalSwapChain GetSwapChain GetNumberOfSwapChains Reset Present GetBackBuffer
GetRasterStatus SetDialogBoxMode SetGammaRamp GetGammaRamp CreateTexture
CreateVolumeTexture CreateCubeTexture CreateVertexBuffer CreateIndexBuffer CreateRenderTarget
CreateDepthStencilSurface UpdateSurface UpdateTexture GetRenderTargetData GetFrontBufferData
StretchRect ColorFill CreateOffscreenPlainSurface SetRenderTarget GetRenderTarget
SetDepthStencilSurface GetDepthStencilSurface BeginScene EndScene Clear SetTransform
GetTransform MultiplyTransform SetViewport GetViewport SetMaterial GetMaterial SetLight
GetLight LightEnable GetLightEnable SetClipPlane GetClipPlane SetRenderState GetRenderState
CreateStateBlock BeginStateBlock EndStateBlock SetClipStatus GetClipStatus GetTexture
SetTexture GetTextureStageState SetTextureStageState GetSamplerState SetSamplerState
ValidateDevice SetPaletteEntries GetPaletteEntries SetCurrentTexturePalette
GetCurrentTexturePalette SetScissorRect GetScissorRect SetSoftwareVertexProcessing
GetSoftwareVertexProcessing SetNPatchMode GetNPatchMode DrawPrimitive DrawIndexedPrimitive
DrawPrimitiveUP DrawIndexedPrimitiveUP ProcessVertices CreateVertexDeclaration
SetVertexDeclaration GetVertexDeclaration SetFVF GetFVF CreateVertexShader SetVertexShader
GetVertexShader SetVertexShaderConstantF GetVertexShaderConstantF SetVertexShaderConstantI
GetVertexShaderConstantI SetVertexShaderConstantB GetVertexShaderConstantB SetStreamSource
GetStreamSource SetStreamSourceFreq GetStreamSourceFreq SetIndices GetIndices
CreatePixelShader SetPixelShader GetPixelShader SetPixelShaderConstantF GetPixelShaderConstantF
SetPixelShaderConstantI GetPixelShaderConstantI SetPixelShaderConstantB GetPixelShaderConstantB
DrawRectPatch DrawTriPatch DeletePatch CreateQuery GetDevice SetPrivateData GetPrivateData
FreePrivateData SetPriority GetPriority PreLoad GetType SetLOD GetLOD GetLevelCount
SetAutoGenFilterType GetAutoGenFilterType GenerateMipSubLevels GetLevelDesc GetSurfaceLevel
LockRect UnlockRect AddDirtyRect GetCubeMapSurface GetVolumeLevel LockBox UnlockBox
AddDirtyBox GetContainer GetDesc Lock Unlock GetDeclaration GetFunction GetDataSize
Issue GetData Capture Apply GetPresentParameters GetBufferPointer GetBufferSize
GetNumFaces GetNumVertices GetFVF GetDeclaration GetNumBytesPerVertex GetOptions
CloneMeshFVF CloneMesh GetVertexBuffer GetIndexBuffer LockVertexBuffer UnlockVertexBuffer
LockIndexBuffer UnlockIndexBuffer GetAttributeTable ConvertPointRepsToAdjacency
ConvertAdjacencyToPointReps GenerateAdjacency UpdateSemantics DrawSubset Optimize
OptimizeInplace SetAttributeTable GetDevice GetDesc GetDC ReleaseDC OnLostDevice
OnResetDevice Begin End Flush Draw DrawTextA DrawTextW PreloadCharacters PreloadGlyphs
PreloadTextA PreloadTextW GetSprite GetTransform SetTransform SetWorldViewRH SetWorldViewLH
GetParameter GetParameterByName GetParameterBySemantic GetParameterElement GetTechnique
GetTechniqueByName GetTechniqueDesc GetParameterDesc GetAnnotationByName GetBool GetInt
GetIntArray GetFloat SetValue GetValue SetBool SetBoolArray SetInt SetIntArray SetFloat
SetFloatArray SetVector SetVectorArray SetMatrix SetMatrixArray SetMatrixPointerArray
SetTexture SetArrayRange SetTechnique ValidateTechnique FindNextValidTechnique IsParameterUsed
BeginPass EndPass CommitChanges SetStateManager GetStateManager CloneEffect GetPool
GetPass GetPassByName GetPassDesc GetFunctionDesc GetString SetString GetVector GetMatrix
GetVectorArray GetFloatArray GetMatrixArray GetTexture GetAnnotation GetAnnotationByName
GetParameterByName GetParameterBySemantic SetMatrixTranspose SetMatrixTransposeArray
SetMatrixTransposePointerArray GetMatrixTranspose GetMatrixTransposeArray
BeginParameterBlock EndParameterBlock ApplyParameterBlock DeleteParameterBlock'''.split())
CONSTANT = re.compile(r'\b(?:D3D[A-Z0-9]+_\w+|D3DX(?:SHADER|FX|MESH|FILTER|IFF)_\w+)\b')
LEX = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|R"(?P<delim>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delim)"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
TOKEN = re.compile(r'[A-Za-z_]\w*|0[xX][\da-fA-F]+[uUlL]*|\d+(?:\.\d*)?[uUfFlL]*|->|::|[^\s]')


def mask(text, strings=False):
    def replace(m):
        if not strings and not m.group().startswith(('//', '/*')):
            return m.group()
        return re.sub(r'[^\n]', ' ', m.group())
    return LEX.sub(replace, text)


def canonical(name):
    if name in ALIASES:
        return ALIASES[name]
    if name.startswith('LPDIRECT3D'):
        stem = name[len('LPDIRECT3D'):]
        stems = ('Device', 'BaseTexture', 'Texture', 'CubeTexture', 'VolumeTexture',
                 'Volume', 'Surface', 'VertexBuffer', 'IndexBuffer', 'VertexDeclaration',
                 'VertexShader', 'PixelShader', 'Query', 'StateBlock', 'SwapChain', 'Resource')
        return 'IDirect3D' + next((s + '9' for s in stems if s.upper() + '9' == stem), stem)
    if name.startswith('LPD3DX'):
        return 'ID3DX' + name[6:].title()
    return name


def compact(text):
    return ' '.join(text.split())


class Source:
    def __init__(self, path, text=None):
        self.path = path.relative_to(ROOT).as_posix()
        self.text = text if text is not None else path.read_text(encoding='utf-8-sig', errors='replace')
        self.source_lines = self.text.splitlines()
        self.code = mask(self.text)
        self.clean = mask(self.text, strings=True)
        self.lines = [m.start() for m in re.finditer('\n', self.text)]
        self.tokens = list(TOKEN.finditer(self.clean))
        self.starts = [m.start() for m in self.tokens]
        self.t = [m.group() for m in self.tokens]
        self.pairs = {}
        stack = []
        for i, t in enumerate(self.t):
            if t in ('(', '[', '{'):
                stack.append(i)
            elif t in (')', ']', '}') and stack:
                if self.t[stack[-1]] == {')': '(', ']': '[', '}': '{'}[t]:
                    a = stack.pop()
                    self.pairs[a] = i
                    self.pairs[i] = a
        self.bindings = collections.defaultdict(list)
        self.returns = collections.defaultdict(set)
        # Save declaration positions and enclosing brace intervals. Parameters
        # immediately preceding a body belong to that body's interval.
        scopes = []
        self.scopes = []
        for i, t in enumerate(self.t):
            if t == '{':
                scopes.append(i)
            self.scopes.append(tuple(scopes))
            if t == '}' and scopes:
                scopes.pop()
        self.class_scopes = {}
        for m in re.finditer(r'\b(?:struct|class)\s+(\w+)(?:\s*:\s*[^;{]+)?\s*\{', self.clean):
            self.class_scopes[bisect.bisect_left(self.starts, m.end()-1)] = m[1]
        decl = re.compile(r'\b(' + TYPE + r')\b\s*((?:\*\s*|&\s*|const\s+)*)([A-Za-z_]\w*)')
        self.declarations = []
        for m in decl.finditer(self.clean):
            name, typ = m[3], canonical(m[1])
            if name in {'const', 'volatile'}:
                continue
            pos = bisect.bisect_left(self.starts, m.start())
            tail = self.clean[m.end():].lstrip()
            if tail.startswith('('):
                self.returns[name].add(typ)
            else:
                self.bindings[name].append((m.start(), typ, self.scopes[pos]))
            self.declarations.append((m.start(), typ, name, tail.startswith('(')))

    def line(self, pos):
        return bisect.bisect_left(self.lines, pos) + 1

    def span(self, a, b):
        return self.code[self.tokens[a].start():self.tokens[b].end()]

    def arguments(self, opening, closing):
        return self.code[self.tokens[opening].end():self.tokens[closing].start()]

    def receiver_start(self, end):
        """Walk a postfix receiver backwards, balancing calls, indexing and casts."""
        t = self.t
        start = end
        if t[end] in (')', ']') and end in self.pairs:
            start = self.pairs[end]
            if t[end] == ']' or (start and (re.match(r'^[A-Za-z_]\w*$', t[start-1]) or t[start-1] == '>')):
                start = self.receiver_start(start - 1)
        elif t[end] == '>':
            depth = 1
            start = end - 1
            while start >= 0:
                if t[start] == '>':
                    depth += 1
                elif t[start] == '<':
                    depth -= 1
                    if depth == 0:
                        start = max(0, start - 1)
                        break
                start -= 1
        while start >= 2 and t[start-1] in ('->', '.', '::'):
            start = self.receiver_start(start - 2)
        return max(0, start)

    def resolve(self, a, b, symbols, returns):
        ts = self.t[a:b+1]
        expr = self.span(a, b)
        explicit = {canonical(x.group()) for x in TYPE_RE.finditer(expr)}
        if len(explicit) == 1:
            return explicit.pop()
        # A receiver ending in a function call uses its declared return type,
        # never the type of one of the function's arguments.
        if self.t[b] == ')' and b in self.pairs:
            p = self.pairs[b]
            if p > a and self.t[p-1] != '>':
                types = self.returns.get(self.t[p-1], set()) or returns.get(self.t[p-1], set())
                return next(iter(types)) if len(types) == 1 else None
        # In arr[i], the receiver's declaration is arr, not the index i.
        base_tokens = ts
        if self.t[b] == ']' and b in self.pairs:
            base_tokens = self.t[a:self.pairs[b]]
        names = [x for x in base_tokens if re.match(r'^[A-Za-z_]\w*$', x)]
        if not names:
            return None
        name = names[-1]
        pos = self.tokens[b].start()
        scope = self.scopes[b]
        local = [(p, ty) for p, ty, sc in self.bindings.get(name, [])
                 if p < pos and scope[:len(sc)] == sc]
        if local and not any(x in ts for x in ('->', '.')):
            return max(local)[1]
        types = {ty for _, ty, _ in self.bindings.get(name, [])}
        if not types:
            types = symbols.get(name, set())
        return next(iter(types)) if len(types) == 1 else None


def row(src, i, interface, method, args, resolved, kind, receiver=''):
    return dict(file=src.path, line=src.line(src.tokens[i].start()), interface=interface,
                method=method, args_summary=compact(args), resolved='yes' if resolved else 'no',
                scope='renderer-tree' if src.path.startswith(ROOTS) else 'outside',
                kind=kind, receiver=compact(receiver))


def scan(src, symbols, returns):
    rows = []
    t, pairs = src.t, src.pairs
    for i, tok in enumerate(t):
        if i+1 >= len(t) or t[i+1] != '(' or i+1 not in pairs:
            continue
        close = pairs[i+1]
        args = src.arguments(i+1, close)
        if i and t[i-1] in ('->', '.'):
            a = src.receiver_start(i-2)
            typ = src.resolve(a, i-2, symbols, returns)
            if tok in METHODS or (typ and tok[:1].isupper()) or (typ == 'ID3DXEffect' and tok == 'onLostDevice'):
                method = 'OnLostDevice' if tok == 'onLostDevice' else tok
                rows.append(row(src, i, typ or 'unresolved', method, args, bool(typ),
                                'member', src.span(a, i-2)))
        elif re.match(r'^D3DX[A-Z][A-Za-z0-9_]*$', tok) or tok == 'Direct3DCreate9':
            # Upper-case value types are constructors, not D3DX helper functions.
            if tok.isupper():
                continue
            prev = t[i-1] if i else ''
            prev2 = t[i-2] if i > 1 else ''
            if prev in {'__stdcall', 'WINAPI', 'HRESULT', 'void', 'float', 'BOOL', 'UINT', '*', '&'}:
                continue
            if prev == '::' and prev2 in {'dx9rend', 'dice'}:
                continue
            rows.append(row(src, i, 'D3DX free' if tok.startswith('D3DX') else 'D3D9 free',
                            tok, args, True, 'free'))
    # D3DX effect adapters use opaque void* receivers; the helper's contract is
    # explicit in Code/RendDX9Abi/D3dxEffect.h. Preserve the slot in the CSV.
    pat = re.compile(r'\bd3dxSlot\s*<([^;{}]*?)>\s*\(')
    offsets = {0x04: 'AddRef', 0x08: 'Release', 0x10: 'GetParameterDesc',
               0x14: 'GetTechniqueDesc', 0x20: 'GetParameter', 0x24: 'GetParameterByName',
               0xF4: 'FindNextValidTechnique', 0xF8: 'IsParameterUsed'}
    for m in pat.finditer(src.clean):
        i = bisect.bisect_left(src.starts, m.end()-1)
        end = pairs.get(i)
        if end is None or end+1 >= len(t) or t[end+1] != '(' or end+1 not in pairs:
            continue
        finish = pairs[end+1]
        callargs = src.arguments(end+1, finish)
        dispatch = compact(src.arguments(i, end))
        fn = re.sub(r'^(?:::)?(?:renddx9abi::)?\s*', '', m[1].strip())
        interface = 'ID3DXEffect'
        method = re.sub(r'Fn$', '', re.sub(r'^(D3dx|RaFx|Fx|RaBuf|Buf)', '', fn))
        resolved = fn.endswith('Fn') and method != 'Fn'
        if fn.startswith(('RaBuf', 'Buf')):
            interface = 'IUnknown' if method == 'Release' else 'ID3DXBuffer'
            method = {'GetPointer': 'GetBufferPointer', 'GetSize': 'GetBufferSize'}.get(method, method)
        elif fn == 'RaFxCompileEffectFn':
            interface = 'ID3DXEffectCompiler'
        elif fn.startswith('Fx') and src.path.endswith('/RaShaderPermutator.cpp'):
            # MetaFx::generate passes D3DXCreateEffectCompiler's result into
            # readStringArrays/readDeprecationList and these local Fx helpers.
            # Its error object is the separate ID3DXBuffer out parameter.
            interface = 'ID3DXBuffer' if fn == 'FxReleaseFn' and callargs.strip() == 'errors' else 'ID3DXEffectCompiler'
        elif fn.startswith(('unsigned int ', 'void* ')):
            interface = 'ID3DXBuffer'
            method = 'GetBufferSize' if 'unsigned int' in fn else 'GetBufferPointer'
            resolved = True
        elif not fn.startswith(('D3dx', 'RaFx', 'Fx')):
            val = re.search(r',\s*(0x[0-9a-fA-F]+|\d+)\s*$', dispatch)
            method = offsets.get(int(val[1], 0), 'slot:' + (val[1] if val else 'dynamic')) if val else 'slot:dynamic'
            resolved = not method.startswith('slot:')
        rows.append(row(src, i, interface, method,
                        callargs + ' [dispatch: ' + dispatch + ']', resolved, 'effect-slot'))
    # Raw immediate effect COM calls outside d3dxSlot. The explicit D3dx Fn
    # name or known 0x104 slot plus fx/d3dx receiver identifies these source seams.
    pat = re.compile(r'reinterpret_cast\s*<\s*(?:::)?(?:renddx9abi::)?\s*(\w+)\s*>\s*\(')
    for m in pat.finditer(src.clean):
        i = bisect.bisect_left(src.starts, m.end()-1)
        end = pairs.get(i)
        if end is None or end+1 >= len(t) or t[end+1] != '(' or end+1 not in pairs:
            continue
        dispatch = compact(src.arguments(i, end))
        finish = pairs[end+1]
        args = src.arguments(end+1, finish)
        interface = 'ID3DXEffect'
        if m[1].startswith('D3dx') and m[1].endswith('Fn'):
            method = m[1][4:-2]
        elif m[1].startswith('EffectSet') and m[1].endswith('Fn'):
            method = m[1][6:-2]
        elif m[1] in {'IncludeOpenFn', 'IncludeCloseFn'}:
            interface = 'ID3DXInclude'
            method = m[1][7:-2]
        elif '0x104' in dispatch and re.match(r'\s*(?:fx|d3dx)\b', args):
            method = 'CommitChanges'
        else:
            continue
        rows.append(row(src, i, interface, method, args + ' [dispatch: ' + dispatch + ']',
                        True, 'effect-raw'))
    # D3DXCreateEffectCompiler is loaded dynamically in two source TUs.
    for m in re.finditer(r'\b(?:Ra)?D3dx(Create\w+)Fn\s+(\w+)\s*=', src.clean):
        for call in re.finditer(r'\b' + re.escape(m[2]) + r'\s*\(', src.clean):
            i = bisect.bisect_left(src.starts, call.end()-1)
            if i in pairs:
                rows.append(row(src, i, 'D3DX free', 'D3DX' + m[1], src.arguments(i, pairs[i]),
                                True, 'dynamic-free', m[2]))
    return rows


def write_csv(path, rows, fields):
    with path.open('w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows({k: r.get(k, '') for k in fields} for r in rows)


def table(headers, rows):
    def cell(v):
        return str(v).replace('|', '\\|').replace('\n', ' ')
    return '\n'.join(['| ' + ' | '.join(headers) + ' |',
                      '| ' + ' | '.join('---' for _ in headers) + ' |'] +
                     ['| ' + ' | '.join(cell(v) for v in r) + ' |' for r in rows])


def counts(rows, key):
    return sorted(collections.Counter(key(r) for r in rows).items(), key=lambda x: (-x[1], x[0]))


def group(r):
    name = r['method']
    it = r['interface']
    if 'Effect' in it or 'Effect' in name:
        return 'effect'
    if re.search(r'Font|Sprite', it + name):
        return 'font/sprite'
    if re.search(r'Mesh|Adjacency|VertexBuffer|IndexBuffer|NumFaces|NumVertices|Optimize', it + name) and it.startswith('ID3DX') or re.match(r'D3DX(?:CreateBox|CreateSphere|CreateCylinder|Intersect|Compute|Clean|Valid|Weld|Simplify|Tessellate)', name):
        return 'mesh'
    if re.match(r'D3DX(?:Matrix|Vec|Quaternion|Plane|Color|SH|Float|Fresnel)', name):
        return 'math'
    if re.search(r'Texture|Surface|Volume|ImageInfo', name):
        return 'texture loading'
    return 'other'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path, default=ROOT / 'out/inventory')
    ap.add_argument('--report', type=Path, default=ROOT / 'docs/macos-d3d9-inventory.md')
    opts = ap.parse_args()
    start = time.monotonic()
    sources = []
    for path in sorted((ROOT / 'Code').rglob('*')):
        if not path.is_file() or path.suffix.lower() not in EXTENSIONS:
            continue
        rel = path.relative_to(ROOT).as_posix()
        # Do not count generated build copies as original source.
        if any(part in {'out', 'CMakeFiles', '__pycache__'} or part.startswith(('obj_', 'bin_')) for part in path.parts):
            continue
        text = path.read_text(encoding='utf-8-sig', errors='replace')
        if rel.startswith(ROOTS) or (MENTION.search(text) and MENTION.search(mask(text, strings=True))):
            sources.append(Source(path))
    symbols, returns = collections.defaultdict(set), collections.defaultdict(set)
    for src in sources:
        for name, values in src.bindings.items():
            symbols[name].update(ty for _, ty, _ in values)
        for name, values in src.returns.items():
            returns[name].update(values)
    rows = sorted([r for src in sources for r in scan(src, symbols, returns)],
                  key=lambda r: (r['file'], r['line'], r['method'], r['args_summary']))
    opts.out.mkdir(parents=True, exist_ok=True)
    fields = ['file', 'line', 'interface', 'method', 'args_summary', 'resolved', 'scope', 'kind', 'receiver']
    d3dx = [r for r in rows if r['interface'].startswith(('ID3DX', 'D3DX')) or r['method'].startswith('D3DX') or r['kind'].startswith('effect-')]
    d3d = [r for r in rows if r not in d3dx]
    write_csv(opts.out / 'd3d9_calls.csv', d3d, fields)
    write_csv(opts.out / 'd3dx_calls.csv', d3dx, fields)
    # Retain literal state IDs/flags on the engine's existing forwarding API as
    # supplementary evidence; these are not added to the COM call totals.
    wrappers = []
    wrapper_name = re.compile(r'^(?:stateManager(?:Set|Clear|Draw)\w*|rendererDraw\w*|(?:vertex|index)Buffer(?:Lock|Unlock)|texture(?:Lock|Unlock)\w*)$')
    for src in sources:
        for i, tok in enumerate(src.t[:-1]):
            if not wrapper_name.match(tok) or src.t[i+1] != '(' or i+1 not in src.pairs:
                continue
            # Exclude declarations/definitions: a type immediately before the
            # unqualified name, or a primitive-typed parameter list, is not a call.
            prev = src.t[i-1] if i else ''
            end = src.pairs[i+1]
            args = src.arguments(i+1, end)
            if prev in {'void', 'bool', 'int', '*'} or re.match(r'\s*(?:unsigned|int|void|const)\b', args):
                continue
            wrappers.append(row(src, i, 'engine forwarding helper', tok, args, True, 'wrapper'))
    write_csv(opts.out / 'd3d9_wrapper_calls.csv', wrappers, fields)
    enums = []
    crossings = []
    for src in sources:
        scope = 'renderer-tree' if src.path.startswith(ROOTS) else 'outside'
        for m in CONSTANT.finditer(src.clean):
            line = src.line(m.start())
            enums.append(dict(file=src.path, line=line, token=m.group(), scope=scope,
                              source=compact(src.source_lines[line-1])))
        for pos, typ, name, is_function in src.declarations:
            line = src.line(pos)
            crossings.append(dict(file=src.path, line=line, interface=typ, name=name,
                                  kind='return/interface' if is_function else 'typed storage/parameter',
                                  scope=scope, source=compact(src.source_lines[line-1])))
        # Opaque fields, containers and byte-offset storage are also crossings;
        # retain annotation evidence rather than silently treating void* as typed.
        for num, line in enumerate(src.source_lines, 1):
            if re.search(r'\b(?:void\s*\*|vc71::\w+|unsigned\s+int)', line) and re.search(r'\b(?:IDirect3D\w+|ID3DX\w+)\b', line) and '//' in line and not line.lstrip().startswith('//'):
                crossings.append(dict(file=src.path, line=num, interface='opaque; see annotation',
                                      name='', kind='annotated opaque storage', scope=scope, source=compact(line)))
        for m in re.finditer(r'(?:reinterpret_cast|static_cast)\s*<\s*(' + TYPE + r')\s*\*\s*\*', src.clean):
            line = src.line(m.start())
            crossings.append(dict(file=src.path, line=line, interface=canonical(m[1]), name='',
                                  kind='raw pointer overlay/cast', scope=scope,
                                  source=compact(src.source_lines[line-1])))
        for m in re.finditer(r'\bvoid\s*\*\s*(?:const\s+)?(\w+)(?:\s*\[[^\]]*\])?\s*;', src.clean):
            i = bisect.bisect_left(src.starts, m.start())
            sc = src.scopes[i]
            if sc and sc[-1] in src.class_scopes:
                line = src.line(m.start())
                crossings.append(dict(file=src.path, line=line, interface='opaque; requires owner review',
                                      name=src.class_scopes[sc[-1]] + '::' + m[1],
                                      kind='opaque member candidate', scope=scope,
                                      source=compact(src.source_lines[line-1])))
    crossings.sort(key=lambda r: (r['file'], r['line'], r['kind']))
    write_csv(opts.out / 'd3d9_constants.csv', enums, ['file', 'line', 'token', 'scope', 'source'])
    write_csv(opts.out / 'd3d9_crossings.csv', crossings,
              ['file', 'line', 'interface', 'name', 'kind', 'scope', 'source'])
    inside = [r for r in rows if r['scope'] == 'renderer-tree']
    outside = [r for r in rows if r['scope'] == 'outside']
    builder = [r for r in inside if '/TextureAtlasBuilder/' in r['file']]
    unresolved = [r for r in inside if r['resolved'] == 'no']
    member_keys = collections.Counter((r['file'], r['line'], r['method'])
                                      for r in inside if r['kind'] == 'member')
    syntactic_members = collections.Counter()
    slot_count = 0
    for src in sources:
        if not src.path.startswith(ROOTS):
            continue
        slot_count += len(re.findall(r'\bd3dxSlot\s*<', src.clean))
        for call in re.finditer(r'(?:->|\.)\s*([A-Z]\w*)\s*\(', src.clean):
            syntactic_members[(src.path, src.line(call.start(1)), call[1])] += 1
    unmatched = sorted((syntactic_members - member_keys).elements())
    # [CHOSEN, not recovered] PR #55 retired this hand-built import library in
    # favor of the installed SDK. Missing historical input disables this check,
    # not the source inventory; never report an empty manifest as verified coverage.
    import_manifest = ROOT / 'Code/RendDX9Abi/D3dx9_25.def'
    imports = set(re.findall(r'^\s+(D3DX\w+)\s*$', import_manifest.read_text(), re.M)) if import_manifest.is_file() else set()
    missing_imports = sorted(imports - {r['method'] for r in inside})
    coverage = dict(unmatched_capitalized_member_calls=unmatched,
                    d3dx_slot_syntax=slot_count,
                    d3dx_slot_rows=sum(r['kind'] == 'effect-slot' for r in inside),
                    d3dx_import_manifest_available=import_manifest.is_file(),
                    d3dx_import_names=len(imports) if import_manifest.is_file() else None,
                    imports_without_call_sites=missing_imports if import_manifest.is_file() else None)
    (opts.out / 'coverage.json').write_text(json.dumps(coverage, indent=2, sort_keys=True) + '\n')
    sample = random.Random(1509).sample(inside, min(20, len(inside)))
    write_csv(opts.out / 'spot_checks.csv', sample, fields)
    srcmap = {s.path: s for s in sources}
    excerpts = []
    for r in sample:
        lines = srcmap[r['file']].text.splitlines()
        lo, hi = max(0, r['line'] - 3), min(len(lines), r['line'] + 4)
        excerpts += [f"{r['file']}:{r['line']} {r['interface']}::{r['method']}"]
        excerpts += [f'{i+1}: {lines[i]}' for i in range(lo, hi)] + ['']
    (opts.out / 'spot_checks.txt').write_text('\n'.join(excerpts), encoding='utf-8')
    manifest = {'roots': ROOTS, 'files': {s.path: hashlib.sha256(s.text.encode()).hexdigest() for s in sources},
                'renderer_calls': len(inside), 'outside_calls': len(outside),
                'unresolved_renderer': len(unresolved), 'sample_seed': 1509}
    (opts.out / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    report = ['# D3D9 and D3DX source inventory', '',
              '[CHOSEN, not recovered] Read-only planning tool for the owner-requested macOS port. '
              'This inventories current source, not recovered binary behavior or executed paths.', '',
              'Reproduce from the repository root: `python3 tools/macos/d3d9_inventory.py`. '
              'Python 3 standard library only; no SDK, compiler, game content or network required. '
              'Outputs are deterministic (no timestamps); `out/inventory/manifest.json` records input hashes. '
              'CSV arguments preserve the full whitespace-normalized source expression, including numeric '
              'literals, casts and enum names. Additional CSV columns preserve receiver, dispatch kind and scope.', '',
              '## Scope and interpretation', '',
              'Requested trees: ' + ', '.join('`' + p + '`' for p in ROOTS) + '. '
              'Headers/inline bodies and inactive Windows/editor arms are included once per lexical site. '
              'TextureAtlasBuilder is included by the requested directory scope but is a separate DLL; '
              'its sites must not be mistaken for current RendDX9.dll imports. '
              'Image manifests: `Code/Build/renddx9.cmd`, `Code/Build/textureatlasbuilder.cmd`, '
              'and `BF2_RENDDX9_STATIC_SOURCES` in `CMakeLists.txt`. Other files under `Code/` naming '
              'D3D/D3DX types are scanned and reported separately; these include editor, tests and vendors.', '',
              'The scanner strips comments/strings for matching, balances parentheses/indexing, recognizes '
              'typed declarations, explicit casts, unambiguous member names and function returns. '
              'Unambiguous cross-file declarations are lexical evidence, not compiler-verified overload '
              'resolution. No interface is guessed solely from a method name. Candidate COM-method names '
              'with unknown receivers are retained as `unresolved` (some may be engine methods). '
              '`IUnknown` calls preserve the declared type; they do not establish which D3D resource is held. '
              'D3DX slot adapters and immediate named/raw CommitChanges dispatches are included. '
              'Dynamic slot dispatch is retained unresolved; template fan-out is counted at its physical '
              'dispatch body, not once per instantiation. This is a lexical inventory, not proof that '
              'arbitrary aliasing/macros/raw vtables contain no additional GPU calls.', '',
              f'**Requested-tree totals: {len(inside)} calls; '
              f'{sum(r in d3d for r in inside)} D3D9/candidates, '
              f'{sum(r in d3dx for r in inside)} D3DX; {len(unresolved)} unresolved.** '
              f'Outside scope: {len(outside)} calls/candidates. Scanned {len(sources)} files.', '',
              table(['Image partition within requested trees', 'Sites'],
                    [('RendDX9 source/header/inline sites', len(inside)-len(builder)),
                     ('TextureAtlasBuilder sibling DLL', len(builder))]), '',
              '## Totals by interface', '', table(['Interface', 'Sites'], counts(inside, lambda r: r['interface'])), '',
              'No direct calls were found on `IDirect3DStateBlock9`, `IDirect3DVertexShader9`, '
              '`IDirect3DVolume9`, `ID3DXFont` or `ID3DXSprite`. Shader creation/binding and '
              'generic IUnknown releases still belong to the seam; zero direct calls does not '
              'mean a resource type is absent.', '',
              '## Totals by method name', '', table(['Method', 'Sites'], counts(inside, lambda r: r['method'])), '',
              '## Totals by interface and method', '', table(['Interface::method', 'Sites'], counts(inside, lambda r: r['interface'] + '::' + r['method'])), '',
              '## State, format and other seam constants', '',
              'Counts below are code-token occurrences in the requested trees, including tables, declarations '
              'and non-COM wrapper calls. They are not runtime state-change counts. Each occurrence and '
              'source line is in `d3d9_constants.csv`; numeric/variable arguments remain in call CSVs. '
              'Do not drop numeric state IDs or vendor FOURCC values merely because they have no enum spelling. '
              'The supplementary `d3d9_wrapper_calls.csv` preserves arguments at the existing engine '
              'state/draw/lock forwarding helpers (excluded from COM totals), where many numeric state '
              'IDs, flags and primitive types originate.', '']
    for title, prefix in [('Render states', 'D3DRS_'), ('Formats', 'D3DFMT_'),
                          ('Sampler states', 'D3DSAMP_'), ('Texture-stage states', 'D3DTSS_'),
                          ('Pools', 'D3DPOOL_'), ('Usage', 'D3DUSAGE_'), ('Locks', 'D3DLOCK_'),
                          ('Primitive types', 'D3DPT_'), ('Declaration types', 'D3DDECLTYPE_'),
                          ('Declaration usages', 'D3DDECLUSAGE_')]:
        values = [e for e in enums if e['scope'] == 'renderer-tree' and e['token'].startswith(prefix)]
        report += ['### ' + title, '', table(['Token', 'Occurrences'], counts(values, lambda e: e['token'])), '']
    report += ['## D3DX groups', '']
    for category in ('texture loading', 'math', 'mesh', 'effect', 'font/sprite', 'other'):
        values = [r for r in inside if r in d3dx and group(r) == category]
        report += [f'### {category} ({len(values)} sites)', '', table(['Method', 'Sites'], counts(values, lambda r: r['interface'] + '::' + r['method'])), '']
    report += ['## Top files', '', table(['File', 'Sites'], counts(inside, lambda r: r['file'])[:25]), '',
               '## Raw pointer crossings', '',
               'These declarations are migration boundaries, not a claim that every one is exported. '
               'The exhaustive `d3d9_crossings.csv` includes parameters, returns, storage, annotated '
               'opaque fields, all void-pointer member candidates and pointer overlays. Header interfaces are visible beyond their owning TU; '
               'private TU structs and offset views still require a consistent resource representation. '
               'Opaque candidates include engine pointers and vtables: they require owner review and '
               'are not all GPU resources. Integer-encoded pointer storage cannot be exhaustively typed lexically.', '',
               'The main shared contracts are `EngineTexture::baseTexture` and '
               '`TextureLockRecord::staging` in `RendDX9/Texture.h`, `RendDX9Base::d3d9/device` '
               'and typed state/buffer helper parameters in `RendDX9/RendDX9Base.h`, and the '
               '`RaStreamDataEditable` vertex/index buffer fields. TU-private crossings include '
               '`VertexBuffer::buffer`, `IndexBuffer::buffer`, vertex declarations, queries, '
               'render/depth surfaces, and the opaque compiled-effect handles. `StateManager::streamVB`, '
               '`indices`, `vertexDecl`, `vertexShader` and `pixelShader` are void-pointer resource '
               'mirrors; its texture `StateCell` values carry raw texture references through integer '
               'words (see `blockCopyTextureRef` and the reset/pop walks). A Windows backend must '
               'preserve those reference operations before any Metal representation change. '
               '`IRenderer` and `IRenderView` have unresolved positional slots; their declarations '
               'alone do not prove an absence of additional opaque crossings. '
               '`RendDX9Base.h::renderDevice()` also returns the device as `void*`; '
               'this opaque helper return must migrate with its typed/cast callers.', '',
               table(['Location', 'Type', 'Name / kind', 'Source'],
                     [(f"{r['file']}:{r['line']}", r['interface'], r['name'] or r['kind'], r['source'])
                      for r in crossings if r['scope'] == 'renderer-tree' and r['kind'] != 'opaque member candidate']), '',
               'Additional untyped member candidates (including the state mirrors named above) '
               'are listed in `d3d9_crossings.csv` with kind `opaque member candidate`.', '',
               '## Additional source files outside the requested trees', '',
               table(['File', 'Calls/candidates', 'Role'],
                     [(s.path, sum(r['file'] == s.path for r in outside),
                       'vendor' if '/External/' in s.path else 'test/tool' if s.path.startswith(('Code/Tests/', 'Code/Build/', 'Code/Tools/')) else 'engine/shared')
                      for s in sources if not s.path.startswith(ROOTS)]), '',
               'Outside counts are excluded from all tables above; filter `scope=outside` in the two call CSVs.', '',
               '## Proposed seam groups (proposal)', '',
               'Preserve each listed call and its arguments 1:1 in a Windows D3D9 backend first. '
               'The proposed grouping is organizational; it does not authorize removing inherited COM '
               'operations, changing lifetimes, coalescing calls, changing flags, or replacing effect '
               'behavior. Metal comes after the Windows wrapper is validated.', '']
    proposed = collections.defaultdict(set)
    for r in inside:
        name, it = r['method'], r['interface']
        if it.startswith('ID3DX') or it == 'D3DX free':
            cat = 'effects' if 'Effect' in it or 'Effect' in name else 'D3DX helpers (' + group(r) + ')'
        elif re.search(r'Lock|Unlock', name):
            cat = 'lock/unlock'
        elif re.search(r'RenderTarget|DepthStencil|BackBuffer|SwapChain|Surface|StretchRect|ColorFill|Viewport|Scissor|Clear', name):
            cat = 'render targets'
        elif 'Declaration' in name or 'FVF' in name:
            cat = 'declarations'
        elif re.search(r'Draw|BeginScene|EndScene|Present', name):
            cat = 'draws'
        elif 'Query' in it or 'Query' in name or name in {'Issue', 'GetData'}:
            cat = 'queries'
        elif name.startswith(('Set', 'Get')) and re.search(r'State|Shader|Stream|Indices|Texture|Transform|Clip|Light|Material', name):
            cat = 'state'
        else:
            cat = 'resources, lifetime and device lifecycle'
        proposed[cat].add(it + '::' + name)
    report += [table(['Group', 'Exact observed methods'], [(k, ', '.join(sorted(v))) for k, v in sorted(proposed.items())]), '',
               '## Self-check and limitations', '',
               f'Independent syntax cross-check: {len(unmatched)} unmatched capitalized member-call '
               f'sites; {slot_count} raw `d3dxSlot` occurrences versus '
               f'{coverage["d3dx_slot_rows"]} dispatch rows. ' +
               (f'{len(imports)} names in `D3dx9_25.def`, with {len(missing_imports)} lacking source call sites. '
                if import_manifest.is_file() else
                'D3DX import-manifest check unavailable: `D3dx9_25.def` was retired in favor of the SDK import library. ') +
               'Details are in `coverage.json`. These checks catch extraction omissions but do not '
               'establish the type of opaque receivers.', '',
               f'{len(unresolved)} requested-tree sites remain unresolved. '
               'Ten deterministic examples (or all when fewer):', '',
               table(['Location', 'Receiver', 'Method', 'Arguments'],
                     [(f"{r['file']}:{r['line']}", r['receiver'], r['method'], r['args_summary']) for r in unresolved[:10]]), '',
               'Twenty rows are selected without replacement with Python `random.Random(1509)` '
               'from the sorted requested-tree rows. `spot_checks.csv` holds them and '
               '`spot_checks.txt` holds numbered source excerpts. The review result is recorded in '
               '`docs/macos-r0-d3d9-inventory.md`; sampling checks precision, not exhaustive recall. '
               'Manual review of the source snapshot at `df2262e`: all 20 rows matched their location, '
               'receiver interface/method and arguments. That review applies to this baseline only; '
               're-run it when inputs change.', '',
               table(['Location', 'Interface::method', 'Resolved'],
                     [(f"{r['file']}:{r['line']}", r['interface'] + '::' + r['method'], r['resolved']) for r in sample]), '',
               'No engine source was changed. No binary recovery, renderer build, GPU execution, '
               'Windows wrapper validation, server test or Metal rendering is claimed.', '']
    opts.report.parent.mkdir(parents=True, exist_ok=True)
    opts.report.write_text('\n'.join(report), encoding='utf-8')
    print(json.dumps(dict(renderer=len(inside), d3d9=sum(r in d3d for r in inside),
                          d3dx=sum(r in d3dx for r in inside), unresolved=len(unresolved),
                          outside=len(outside), seconds=round(time.monotonic()-start, 3)), sort_keys=True))


if __name__ == '__main__':
    main()
