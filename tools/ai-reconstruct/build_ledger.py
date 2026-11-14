#!/usr/bin/env python3
"""Generate the AI DLL function ledger (docs/ai-reconstruct/ledger.csv) and its summary.

One row per function defined in the Linux AI image (libaidll.so 1.5.3153, ia-32, with symbols).
Python 3 standard library only; shells out to `objdump -t` (symbols, sizes, sections), `c++filt -n`
(demangling) and `git rev-parse` (commit column). Re-run after every merge:

    python3 tools/ai-reconstruct/build_ledger.py [--binary PATH] [--ledger PATH --summary PATH]

Columns (the first six are the START.md contract; the last two are additions):
    address        ELF virtual address, hex (0x...). The ELF address is canonical; Ghidra shows ELF+0x10000.
    size           symbol size in bytes (decimal).
    symbol         demangled name. Aliases at the same address are joined with " | " (one row per address).
    file           repo-relative source file where the body lives or should live (blank for CRT startup).
    status         todo | done | stop | skip-debug | skip-lib (see below).
    commit         short HEAD hash when the row was last marked done (kept from the previous ledger when
                   the row was already done there), blank otherwise.
    file_inferred  1 when `file` is a guess (class not present in the tree, TU neighbour, static init), 0
                   when it comes from a matched body or from where the class's methods are defined/declared.
    mangled        raw symbol name(s), " | "-joined like `symbol`.

File attribution of rows without a matched body: the class's definition/declaration file; for a
TU's static-init block (the run of adjacent static rows ending in _GLOBAL__I_) the file of the key
symbol of _GLOBAL__I_ when it is a matched body in a .cpp, with original FILE symbols and embedded
GCC key paths taking precedence over that key's current placement; else the majority file of the TU's own
strong (non-weak, non-anonymous-namespace, non-header) functions between the previous block and this
one (the image lays a TU's functions out before its static-init block), else the key class's .cpp;
a lone static row (a function-local static's __tcf) goes with the function before it; an
anonymous-namespace symbol whose namespace is named after its file (_GLOBAL__N_<dir>_<file>.cpp...)
goes to that file; everything else to the nearest attributed neighbour.

Function set: every FUNC symbol in .text/.init/.fini (`objdump -t`, flag F). `nm` letters t/T/W also
list 652 local `.Lnnnn` NOTYPE labels in .text; those are not functions and are excluded.

Status rules (first that applies):
  skip-lib    not BF2 source: CRT/glibc startup (_init, _fini, __do_global_dtors_aux, frame_dummy,
              __do_global_ctors_aux, __i686.get_pc_thunk.*), compiler thunks ("non-virtual thunk to",
              "virtual thunk to", "covariant return thunk to"), and everything whose qualified name is in
              std:: or __gnu_cxx:: (libstdc++ / STL template instantiations, also over BF2 types: the
              source-level STL operation is preserved, the instantiation is not reconstructed).
              NOT skip-lib: __static_initialization_and_destruction_*, __tcf_*, _GLOBAL__I_* (a BF2 TU's
              static init/teardown; mapped to that TU's file, status todo unless matched).
  done        a source body matched (see matching) and the body text contains neither
              BF2_AI_FRONTIER nor "[FRONTIER]".
  stop        a matched body contains BF2_AI_FRONTIER / "[FRONTIER]", or no body matched but a
              declaration whose own comment carries [FRONTIER] matched by name.
  skip-debug  not done (todo or stop; stops are counted in the summary), and the class is a debug-drawing class: AIRenderUtil, PrettyPrinter, BAPDebugger,
              DrawPrimitive and the DrawPrimitives package (DPManager, DP*, IDP*), or the matched/mapped
              file is under AI/DrawPrimitives/ or AIUtils/DebugRendering/.
  todo        everything else.

Matching (source scan: the .cpp files of the bf2_aidll manifest in CMakeLists.txt, all .cpp/.h/.inl
files under Code/BF2/{Math,Core,IO}, every header under Code/BF2/AI, and the headers beside
manifest .cpp files; function bodies found by a brace-aware
scan with comments/strings blanked):
  A. Address: hex addresses in the comments attached to a definition (the contiguous comment block above
     it, comments inside its declarator, the comment on its opening line) are looked up as ELF addresses
     and as Ghidra addresses (A - 0x10000). An address counts only when the symbol there agrees by name
     (same function name and same innermost class, template arguments ignored). A match covers the
     symbol's whole demangled-signature group (C1/C2, D0/D1/D2 share one source body). Addresses after
     '+', or tagged PE/bf2/vtable/GOT/rodata/.data/.bss are ignored.
  B. Exact qualified name: when A did not match, the definition's class chain + name is compared with the
     binary's (namespaces dropped). A unique group matches; overloads are separated by parameter count;
     anything still ambiguous is not matched and is listed in the summary.
  C. Documented static initialisation: a comment block in a manifest .cpp that contains
     "static init" / "static initialisation" and names the TU's static-init rows (static init,
     __tcf_N, _GLOBAL__I_) by ELF address marks those rows done in that file. Short form for a new
     file: `// static init: ELF 0x... (__tcf_0 0x..., _GLOBAL__I 0x...)`.
  D. Generated console bindings: docs/ai-reconstruct/console-bindings.json (written by
     tools/ai-reconstruct/gen_console_bindings.py) lists every ConsoleClass with its member functions
     and teardown functions; the emitted classes' rows are done in AIModule.cpp.
  Name-only matches whose body or comment says "[CHOSEN, not recovered]", or whose comment (not body)
  says [FRONTIER], are left todo and listed. The first address after "ELF" in a definition's comment that names a different function
  is listed as ambiguous.
"""

import argparse
import collections
import csv
import json
import os
import re
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BINARY = (os.path.join(os.environ["BF2_LINUX_SERVER_DIR"], "bin", "ia-32", "libaidll.so")
                  if os.environ.get("BF2_LINUX_SERVER_DIR") else None)
LEDGER = os.path.join(REPO, "docs", "ai-reconstruct", "ledger.csv")
SUMMARY = os.path.join(REPO, "docs", "ai-reconstruct", "ledger-summary.md")
CONSOLE_REPORT = os.path.join(REPO, "docs", "ai-reconstruct", "console-bindings.json")
GHIDRA_DELTA = 0x10000
AI_ROOT = "Code/BF2/AI/AIDLL/AI/"

CRT_NAMES = {"_init", "_fini", "__do_global_dtors_aux", "frame_dummy", "__do_global_ctors_aux"}
THUNK_PREFIXES = ("non-virtual thunk to ", "virtual thunk to ", "covariant return thunk to ")
LIB_NAMESPACES = ("std", "__gnu_cxx")
DEBUG_CLASSES = {"AIRenderUtil", "PrettyPrinter", "BAPDebugger", "DrawPrimitive", "DPManager"}
DEBUG_CLASS_RE = re.compile(r"^I?DP[A-Z]")
DEBUG_DIRS = ("/DrawPrimitives/", "/AIUtils/DebugRendering/")
KNOWN_NAMESPACES = {"dice", "hfe", "ai", "io", "std", "__gnu_cxx", "{anon}", "world"}
KEYWORDS = {"if", "for", "while", "switch", "catch", "return", "sizeof", "do", "else", "defined",
            "decltype", "alignof", "static_assert", "new", "delete", "throw", "typeid"}
CHOSEN = "[CHOSEN, not recovered]"


# ----------------------------------------------------------------------------------------------
# Binary side
# ----------------------------------------------------------------------------------------------

SYM_RE = re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\t([0-9a-f]{8}) (?:\.hidden )?(.+)$")


def load_symbols(binary):
    if not binary:
        raise SystemExit("build_ledger.py: pass --binary or set BF2_LINUX_SERVER_DIR")
    out = subprocess.run(["objdump", "-t", binary], check=True, capture_output=True, text=True).stdout
    syms = []
    tu = None
    for line in out.splitlines():
        m = SYM_RE.match(line)
        if not m:
            continue
        addr, flags, section, size, name = m.groups()
        if section == "*ABS*" and "df" in flags:
            tu = name.strip()
            continue
        if "F" not in flags or section not in (".text", ".init", ".fini"):
            continue
        syms.append({"addr": int(addr, 16), "size": int(size, 16), "mangled": name.strip(),
                     "bind": flags[0], "tu": tu if flags[0] == "l" else None})
    names = [s["mangled"] for s in syms]
    dem = subprocess.run(["c++filt", "-n"], input="\n".join(names) + "\n", check=True,
                         capture_output=True, text=True).stdout.splitlines()
    if len(dem) != len(syms):
        sys.exit("c++filt returned %d lines for %d symbols" % (len(dem), len(syms)))
    for s, d in zip(syms, dem):
        s["demangled"] = d.strip()
    return syms


OPERATOR_RE = re.compile(
    r"operator\s*(\(\)|\[\]|new\[\]|delete\[\]|new\b|delete\b|->\*?|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||"
    r"\+\+|--|[-+*/%^&|~!=<>,]=?|\s+[A-Za-z_][\w:<>*& ]*?(?=\())")


def strip_templates(s):
    out, depth, i = [], 0, 0
    while i < len(s):
        if s.startswith("operator", i):
            m = OPERATOR_RE.match(s, i)
            if m:
                if depth == 0:
                    out.append(m.group(0))
                i = m.end()
                continue
        c = s[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth = max(0, depth - 1)
        elif depth == 0:
            out.append(c)
        i += 1
    return "".join(out)


def split_top(s, sep):
    """Split s at sep when not inside <> or ()."""
    parts, depth, cur, i = [], 0, [], 0
    while i < len(s):
        if s.startswith("operator", i):
            m = OPERATOR_RE.match(s, i)
            if m:
                cur.append(m.group(0))
                i = m.end()
                continue
        c = s[i]
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        if depth == 0 and s.startswith(sep, i):
            parts.append("".join(cur))
            cur = []
            i += len(sep)
            continue
        cur.append(c)
        i += 1
    parts.append("".join(cur))
    return parts


def parse_demangled(d):
    """Return dict(kind, qual, params, nparams, sig) for a demangled function name."""
    info = {"thunk": False}
    for p in THUNK_PREFIXES:
        if d.startswith(p):
            info["thunk"] = True
            d = d[len(p):]
            break
    d = d.replace("(anonymous namespace)", "{anon}")
    # find the parameter list: first '(' at depth 0 that is not part of an operator name
    depth, i, start = 0, 0, -1
    while i < len(d):
        if d.startswith("operator", i):
            m = OPERATOR_RE.match(d, i)
            if m:
                i = m.end()
                continue
        c = d[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == "(" and depth == 0:
            start = i
            break
        i += 1
    if start < 0:
        name, params, tail = d, "", ""
    else:
        pd, j = 0, start
        while j < len(d):
            if d[j] == "(":
                pd += 1
            elif d[j] == ")":
                pd -= 1
                if pd == 0:
                    break
            j += 1
        name, params, tail = d[:start], d[start + 1:j], d[j + 1:].strip()
    # drop a return type: last depth-0 space-separated token (operators keep their spaces)
    toks = split_top(name.strip(), " ")
    qual = toks[-1] if toks else name
    if len(toks) > 1 and toks[-2].endswith("operator"):
        qual = toks[-2] + " " + toks[-1]
    # conversion operators: "Foo::operator bool" -> keep "operator bool"
    m = re.search(r"(\S*operator)\s+(.+)$", name.strip())
    if m and "operator" in qual and qual.endswith("operator"):
        qual = name.strip()
    comps = [c for c in split_top(qual, "::")]
    comps_nt = [strip_templates(c).strip() for c in comps]
    info.update({"qual": qual, "comps": comps_nt, "params": params, "cv": tail,
                 "nparams": count_params(params), "sig": qual + "(" + params + ")" + tail,
                 "types": norm_types(params, False), "const": "const" in tail})
    return info


def count_params(p):
    p = p.strip()
    if p in ("", "void"):
        return 0
    return len(split_top(p, ","))


TYPE_WORDS = {"int", "char", "short", "long", "float", "double", "bool", "void", "unsigned", "signed",
              "const", "volatile", "wchar_t"}


def norm_type(t, source):
    t = t.strip()
    if source:
        t = split_top(t, "=")[0].strip()          # default argument
        t = re.sub(r"\[[^\]]*\]\s*$", "*", t)     # array parameter
        t = re.sub(r"(\w+)\s*\[[^\]]*\]$", r"* \1", t)
        m = re.match(r"^(.*?[\w>*&\s])\s*\b([A-Za-z_]\w*)$", t)
        if m and m.group(2) not in TYPE_WORDS and re.search(r"[\w>*&]", m.group(1).strip() or ""):
            t = m.group(1)
    t = t.replace("std::basic_string<char, std::char_traits<char>, std::allocator<char>>", "std::string")
    t = re.sub(r"\b[A-Za-z_]\w*\s*::\s*", "", t)   # all qualifiers (namespaces and outer classes)
    t = re.sub(r"\s+", " ", t).strip()
    t = re.sub(r"\bunsigned\b(?!\s+(int|char|short|long))", "unsigned int", t)
    t = re.sub(r"\bsigned char\b", "signed char", t)
    t = re.sub(r"\blong int\b", "long", t)
    t = re.sub(r"\bshort int\b", "short", t)
    if "*" not in t and "&" not in t:
        t = re.sub(r"^const\s+|\s+const$", "", t)   # top-level const is not part of the signature
    t = re.sub(r"\s*([*&<>,])\s*", r"\1", t)
    t = re.sub(r"^(\w[\w<>,]*)\s+const([*&])", r"const \1\2", t)
    return t


def norm_types(p, source):
    p = p.strip()
    if p in ("", "void"):
        return ()
    return tuple(norm_type(x, source) for x in split_top(p, ","))


def scope_key(comps, namespaces):
    """(class chain without namespaces, function name)."""
    if not comps:
        return ("", "")
    fn = comps[-1]
    scope = [c for c in comps[:-1] if c not in namespaces]
    # drop leading namespace components only (a class named like a namespace stays in the middle)
    lead = 0
    for c in comps[:-1]:
        if c in namespaces:
            lead += 1
        else:
            break
    scope = comps[lead:-1]
    return ("::".join(scope), fn)


def norm_fn(fn):
    fn = re.sub(r"\s+", "", fn)
    return fn


# ----------------------------------------------------------------------------------------------
# Source side
# ----------------------------------------------------------------------------------------------

def manifest_files():
    with open(os.path.join(REPO, "CMakeLists.txt"), encoding="utf-8", errors="replace") as f:
        text = f.read()
    m = re.search(r"set\(bf2_aidll_sources(.*?)\n\)", text, re.S)
    files = set()
    # The source list moved to Code/Build/aidll_sources.txt (one path per line, relative to Code/BF2).
    manifest = os.path.join(REPO, "Code/Build/aidll_sources.txt")
    if os.path.exists(manifest):
        for line in open(manifest, encoding="utf-8"):
            line = line.split("#", 1)[0].strip()
            if line:
                files.add("Code/BF2/" + line)
        m = None
    if m:
        for line in m.group(1).splitlines():
            line = line.split("#", 1)[0]
            for f in re.findall(r"\$\{BF2_ROOT\}/([^\s\"]+)", line):
                files.add("Code/BF2/" + f)
    headers = set()
    for directory in ("AI", "Math", "Core", "IO"):
        for root, _, names in os.walk(os.path.join(REPO, "Code/BF2", directory)):
            for n in names:
                rel = os.path.relpath(os.path.join(root, n), REPO)
                if n.endswith((".h", ".inl")):
                    headers.add(rel)
                elif directory != "AI" and n.endswith(".cpp"):
                    files.add(rel)
    for f in list(files):
        h = os.path.splitext(f)[0] + ".h"
        if os.path.exists(os.path.join(REPO, h)):
            headers.add(h)
    return sorted(files), sorted(headers)


def blank_code(text):
    """Comments, string/char literals and preprocessor lines -> spaces (newlines kept).
    Returns (code, comment_spans)."""
    out = list(text)
    comments = []
    i, n = 0, len(text)
    line_start = True
    while i < n:
        c = text[i]
        if line_start:
            j = i
            while j < n and text[j] in " \t":
                j += 1
            if j < n and text[j] == "#":
                # preprocessor line with continuations
                k = j
                while k < n:
                    if text[k] == "\n" and text[k - 1] != "\\":
                        break
                    k += 1
                for q in range(j, k):
                    if out[q] != "\n":
                        out[q] = " "
                i = k
                line_start = True
                continue
            line_start = False
        if c == "\n":
            line_start = True
            i += 1
            continue
        if text.startswith("//", i):
            k = text.find("\n", i)
            k = n if k < 0 else k
            comments.append((i, k))
            for q in range(i, k):
                out[q] = " "
            i = k
            continue
        if text.startswith("/*", i):
            k = text.find("*/", i + 2)
            k = n if k < 0 else k + 2
            comments.append((i, k))
            for q in range(i, k):
                if out[q] != "\n":
                    out[q] = " "
            i = k
            continue
        if c == '"' or c == "'":
            # raw strings are not used in this tree
            k = i + 1
            while k < n and text[k] != c:
                if text[k] == "\\":
                    k += 1
                elif text[k] == "\n":
                    break
                k += 1
            for q in range(i + 1, min(k, n)):
                if out[q] != "\n":
                    out[q] = " "
            i = k + 1
            continue
        i += 1
    return "".join(out), comments


def apply_object_macros(text):
    """Substitute simple in-file `#define NAME Identifier` (e.g. BF2_CC_NAME ConsoleClass298) in the
    text that follows each define, so the parser sees the real class names."""
    lines = text.split("\n")
    active = {}
    out = []
    for line in lines:
        m = re.match(r"\s*#\s*define\s+([A-Z_][A-Z0-9_]*)\s+([A-Za-z_]\w*)\s*(//.*)?$", line)
        u = re.match(r"\s*#\s*undef\s+([A-Z_][A-Z0-9_]*)", line)
        if m:
            active[m.group(1)] = m.group(2)
            out.append(line)
            continue
        if u:
            active.pop(u.group(1), None)
            out.append(line)
            continue
        if active and not line.lstrip().startswith("#"):
            for k, v in active.items():
                if k in line:
                    line = re.sub(r"\b%s\b" % k, v, line)
        out.append(line)
    return "\n".join(out)


ACCESS_RE = re.compile(r"\b(public|private|protected)\s*:(?!:)")
QUAL_TAIL_RE = re.compile(r"(\s*(const|volatile|override|final|noexcept|throw\s*\([^)]*\)|&|&&))*\s*$")
NAME_BEFORE_PAREN_RE = re.compile(
    r"((?:::)?(?:[A-Za-z_]\w*\s*(?:<[^()]*?>)?\s*::\s*)*(?:~\s*[A-Za-z_]\w*|operator\s*(?:\(\)|\[\]|"
    r"new\[\]|delete\[\]|[^\s(]+|\s+[A-Za-z_][\w:<>*& ]*?)|[A-Za-z_]\w*))\s*$")


def find_matching(code, i, open_c, close_c):
    depth = 0
    n = len(code)
    while i < n:
        c = code[i]
        if c == open_c:
            depth += 1
        elif c == close_c:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return n - 1


def parse_head(head):
    """Classify the text before a '{'. Returns ('namespace', name) | ('class', name) |
    ('function', (qualname, params)) | ('other', None)."""
    h = ACCESS_RE.sub(" ", head)
    hs = h.strip()
    if not hs:
        return ("other", None)
    m = re.search(r"\bnamespace\s*([A-Za-z_]\w*)?\s*$", hs)
    if m:
        return ("namespace", m.group(1) or "{anon}")
    if re.search(r"\bextern\s*$", hs):
        return ("transparent", None)
    if re.search(r"\benum\b", hs) and "(" not in hs.split("enum")[-1]:
        return ("other", None)
    hc = re.sub(r"\btemplate\s*<[^{};]*?>", " ", hs)
    m = re.search(r"\b(class|struct|union)\s+(?:[A-Z_][A-Z0-9_]*\s+)?([A-Za-z_]\w*)\s*(?:<[^{};()]*>)?\s*"
                  r"(?:final\s*)?(?::[^{};()]*)?$", hc)
    if m and "(" not in hc[m.start():]:
        return ("class", m.group(2))
    if hs.endswith("=") or re.search(r"=\s*$", hs):
        return ("other", None)
    # cut a constructor initializer list: first single ':' at paren depth 0 after a ')'
    depth, seen_close, cut = 0, False, None
    i = 0
    while i < len(h):
        c = h[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                seen_close = True
        elif c == ":" and depth == 0 and seen_close:
            if h.startswith("::", i):
                i += 2
                continue
            if i > 0 and h[i - 1] == ":":
                i += 1
                continue
            cut = i
            break
        i += 1
    if cut is not None:
        h = h[:cut]
    h = h.rstrip()
    h = strip_template_prefix(h)
    is_const = bool(re.search(r"\)\s*const\b[^()]*$", h))
    h2 = QUAL_TAIL_RE.sub("", h)
    if not h2.endswith(")"):
        return ("other", None)
    # matching '(' of the final ')'
    depth, j = 0, len(h2) - 1
    while j >= 0:
        if h2[j] == ")":
            depth += 1
        elif h2[j] == "(":
            depth -= 1
            if depth == 0:
                break
        j -= 1
    if j < 0:
        return ("other", None)
    params = h2[j + 1:-1]
    before = h2[:j]
    m = NAME_BEFORE_PAREN_RE.search(before)
    if not m:
        return ("other", None)
    qual = re.sub(r"\s+", "", m.group(1)) if "operator" not in m.group(1) else m.group(1).strip()
    if "operator" not in qual and re.search(r"[*&]", qual):
        # a template return type (`BSTNode<T>* Tree<T>::get`) was swallowed by the lazy `<...>`
        qual = re.split(r"[*&]", qual)[-1]
    last = qual.split("::")[-1]
    if last in KEYWORDS or not last:
        return ("other", None)
    pre = before[:m.start()].strip()
    if re.search(r"(=|\breturn|[-+/%,(]|\bnew)\s*$", pre):
        return ("other", None)
    # a macro invocation such as FOO(x) { ... } with an all-caps name and no return type
    # (a qualified constructor such as BBC4::BBC4 or a destructor is not a macro)
    parts = qual.split("::")
    is_ctor = len(parts) >= 2 and parts[-1].lstrip("~") == re.sub(r"<.*", "", parts[-2])
    if re.fullmatch(r"[A-Z0-9_]+", last) and not pre and not is_ctor:
        return ("other", None)
    return ("function", (qual, params, is_const))


def strip_template_prefix(h):
    """Remove `template <...>` clauses (balanced angle brackets) from a declarator head."""
    while True:
        m = re.search(r"\btemplate\s*<", h)
        if not m:
            return h
        depth, j = 0, m.end() - 1
        while j < len(h):
            if h[j] == "<":
                depth += 1
            elif h[j] == ">":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        h = h[:m.start()] + " " * (j + 1 - m.start()) + h[j + 1:]


def comment_text(text, comments, a, b):
    parts = [text[s:e] for (s, e) in comments if s >= a and e <= b]
    return "\n".join(parts)


def line_of(starts, pos):
    lo, hi = 0, len(starts) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if starts[mid] <= pos:
            lo = mid
        else:
            hi = mid - 1
    return lo + 1


def attached_comment_start(text, line_starts, name_line):
    """Position of the first line of the comment block / declarator lines above `name_line`."""
    lines = text.split("\n")
    k = name_line - 2  # 0-based index of the line above
    while k >= 0:
        s = lines[k].strip()
        if not s:
            break
        if s.startswith("//") or s.startswith("/*") or s.startswith("*") or s.endswith("*/"):
            k -= 1
            continue
        if re.fullmatch(r"(template\s*<.*>|[\w:<>,*&\s]+)", s) and not s.endswith(";"):
            k -= 1
            continue
        break
    return line_starts[k + 1] if k + 1 < len(line_starts) else 0


def parse_file(rel):
    path = os.path.join(REPO, rel)
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            raw = f.read()
    except OSError:
        return [], [], []
    text = apply_object_macros(raw)
    code, comments = blank_code(text)
    line_starts = [0] + [i + 1 for i, c in enumerate(text) if c == "\n"]
    defs, decls, classes = [], [], []

    def walk(lo, hi, scope, tmpl=False):
        """scope: list of ('namespace'|'class', name)."""
        i = lo
        boundary = lo
        while i < hi:
            c = code[i]
            if c == ";":
                if scope and scope[-1][0] == "class" or True:
                    stmt = code[boundary:i]
                    record_decl(stmt, boundary, i, scope)
                boundary = i + 1
            elif c == "}":
                boundary = i + 1
            elif c == "{":
                head = code[boundary:i]
                kind, val = parse_head(head)
                end = find_matching(code, i, "{", "}")
                if kind == "namespace":
                    walk(i + 1, end, scope + [("namespace", val)], tmpl)
                elif kind == "transparent":
                    walk(i + 1, end, scope, tmpl)
                elif kind == "class":
                    classes.append((val, [n for t, n in scope if t == "class"], rel,
                                    line_of(line_starts, i)))
                    walk(i + 1, end, scope + [("class", val)],
                         tmpl or bool(re.search(r"\btemplate\s*<", head)))
                    # the declarator of `class X {...} name;` follows; skip to ';'
                elif kind == "function":
                    qual, params, is_const = val
                    hs = boundary + (len(head) - len(head.lstrip()))
                    name_pos = boundary + head.rfind(qual.split("::")[-1].split("<")[0])
                    name_line = line_of(line_starts, max(name_pos, hs))
                    cstart = attached_comment_start(text, line_starts, line_of(line_starts, hs))
                    cstart = min(cstart, hs)
                    brace_line_end = text.find("\n", i)
                    brace_line_end = len(text) if brace_line_end < 0 else brace_line_end
                    ctext = comment_text(text, comments, cstart, brace_line_end + 1)
                    body = text[i:end + 1]
                    comps = [strip_templates(p).strip() for p in split_top(qual, "::") if p]
                    chain = [n for t, n in scope if t == "class"]
                    defs.append({"file": rel, "line": line_of(line_starts, hs), "qual": qual,
                                 "comps": chain + comps, "params": params,
                                 "nparams": count_params(params), "comment": ctext, "body": body,
                                 "types": norm_types(params, True), "const": is_const,
                                 "template": tmpl or bool(re.search(r"\btemplate\s*<", head)),
                                 "namespaces": [n for t, n in scope if t == "namespace"]})
                i = end
                boundary = end + 1
            i += 1

    def record_decl(stmt, a, b, scope):
        s = ACCESS_RE.sub(" ", stmt).strip()
        if "(" not in s or s.startswith(("return", "typedef", "using", "friend")):
            return
        s2 = re.sub(r"=\s*0\s*$", "", s).strip()
        kind, val = parse_head(s2)
        if kind != "function":
            return
        qual, params, is_const = val
        cstart = attached_comment_start(text, line_starts, line_of(line_starts, a + len(stmt) - len(stmt.lstrip())))
        eol = text.find("\n", b)
        eol = len(text) if eol < 0 else eol
        ctext = comment_text(text, comments, min(cstart, b), eol + 1)
        chain = [n for t, n in scope if t == "class"]
        decls.append({"file": rel, "line": line_of(line_starts, a + len(stmt) - len(stmt.lstrip())),
                      "comps": chain + [strip_templates(p).strip() for p in split_top(qual, "::") if p],
                      "nparams": count_params(params), "comment": ctext,
                      "types": norm_types(params, True), "const": is_const})

    walk(0, len(code), [])
    return defs, decls, classes


# ----------------------------------------------------------------------------------------------
# Matching
# ----------------------------------------------------------------------------------------------

ADDR_RE = re.compile(r"(?<![+\w])0x([0-9A-Fa-f]{5,8})\b")
EXCLUDE_BEFORE_RE = re.compile(r"(PE|bf2|BF2\.exe|vtable|Vtable|GOT|rodata|\.data|\.bss|BSS|ds:|AIDLL\.dll)"
                               r"[^,;.()]{0,12}$")


def comment_addresses(ctext):
    """[(address, is_primary)] from a definition's comment; primary = first address after 'ELF'."""
    out = []
    primary_done = False
    for m in ADDR_RE.finditer(ctext):
        before = ctext[max(0, m.start() - 24):m.start()]
        if EXCLUDE_BEFORE_RE.search(before):
            continue
        v = int(m.group(1), 16)
        prim = False
        if not primary_done and re.search(r"ELF\s*:?\s*$", before):
            prim = True
            primary_done = True
        out.append((v, prim))
    return out


def names_agree(def_comps, sym_comps):
    """Same function name and same innermost class (template args ignored)."""
    if not def_comps or not sym_comps:
        return False
    df, sf = norm_fn(def_comps[-1]), norm_fn(sym_comps[-1])
    if df != sf:
        return False
    dc = [c for c in def_comps[:-1] if c not in KNOWN_NAMESPACES]
    sc = [c for c in sym_comps[:-1] if c not in KNOWN_NAMESPACES]
    if not dc and not sc:
        return True
    if not dc or not sc:
        return False
    return strip_templates(dc[-1]) == strip_templates(sc[-1])


STATIC_RE = re.compile(r"^(__static_initialization_and_destruction_\d+|__tcf_\d+)\b")


def is_static(r):
    return bool(STATIC_RE.match(r["symbol"])) or r["mangled"].startswith(("_GLOBAL__I_", "_GLOBAL__D_"))


def disambiguate(cands, d, groups):
    """Narrow overload candidates: parameter count, then normalized parameter types, then const."""
    if len(cands) <= 1:
        return cands
    c1 = {c for c in cands if groups[c][0]["info"]["nparams"] == d["nparams"]}
    if len(c1) <= 1:
        return c1
    c2 = {c for c in c1 if groups[c][0]["info"]["types"] == d["types"]}
    if len(c2) > 1:
        c3 = {c for c in c2 if groups[c][0]["info"]["const"] == d["const"]}
        if c3:
            c2 = c3
    return c2 if c2 else c1


def frontier_in(s):
    return "BF2_AI_FRONTIER" in s or "[FRONTIER]" in s


def same_linkage_scope(d, info):
    """A TU-local helper cannot satisfy a namespace-scope external function."""
    return ("{anon}" in d["namespaces"] or "{anon}" in d["comps"]) == ("{anon}" in info["comps"])


def embedded_tu_file(mangled):
    """GCC appends a flattened source path to otherwise colliding _GLOBAL__I_ keys.

    DangerObject.cpp has no strong out-of-line member, so neighbour voting incorrectly
    assigns its init triad to AIInformationGrid.cpp. Preserve this path evidence even
    when the destination has not yet been recovered. Underscore decoding is inferred.
    """
    m = re.search(r"(AI_AIDLL_AI_[A-Za-z0-9_]+\.cpp)", mangled)
    return "Code/BF2/" + m.group(1).replace("_", "/") if m else None


def main():
    global LEDGER, SUMMARY
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", default=DEFAULT_BINARY,
                    help="ia-32 libaidll.so (default: $BF2_LINUX_SERVER_DIR/bin/ia-32/libaidll.so)")
    ap.add_argument("--ledger", default=LEDGER, help="output ledger (also preserves its previous commits)")
    ap.add_argument("--summary", default=SUMMARY, help="output summary")
    ap.add_argument("--explain", help="also write a TSV (address, status, evidence, symbol) for checks")
    args = ap.parse_args()
    LEDGER, SUMMARY = args.ledger, args.summary

    head = subprocess.run(["git", "-C", REPO, "rev-parse", "--short", "HEAD"], capture_output=True,
                          text=True).stdout.strip()
    previous = {}
    if os.path.exists(LEDGER):
        with open(LEDGER, newline="") as f:
            for r in csv.DictReader(f):
                previous[r["address"]] = r

    syms = load_symbols(args.binary)
    by_addr = collections.OrderedDict()
    for s in sorted(syms, key=lambda s: (s["addr"], s["mangled"])):
        by_addr.setdefault(s["addr"], []).append(s)
    func_count = len(syms)

    cpps, headers = manifest_files()
    defs, decls, classes = [], [], []
    for rel in cpps + headers:
        d, dc, cl = parse_file(rel)
        defs += d
        decls += dc
        classes += cl
    namespaces = set(KNOWN_NAMESPACES)
    for d in defs:
        namespaces.update(d["namespaces"])

    # binary groups: one source function per demangled signature
    rows = []
    for addr, ss in by_addr.items():
        s0 = ss[0]
        info = parse_demangled(s0["demangled"])
        rows.append({"addr": addr, "size": max(s["size"] for s in ss),
                     "symbol": " | ".join(s["demangled"] for s in ss),
                     "mangled": " | ".join(s["mangled"] for s in ss), "info": info,
                     "bind": s0["bind"], "tu": s0["tu"], "status": None, "file": "", "inferred": 1, "why": ""})
    row_at = {r["addr"]: r for r in rows}
    groups = collections.defaultdict(list)
    for r in rows:
        if not r["info"]["thunk"]:
            groups[r["info"]["sig"]].append(r)
    by_key = collections.defaultdict(set)
    for sig, rs in groups.items():
        k = scope_key(rs[0]["info"]["comps"], namespaces)
        by_key[(k[0], norm_fn(k[1]))].add(sig)

    ambiguous = []
    conv = collections.Counter()
    matched = collections.defaultdict(list)  # sig -> [def]
    chosen_only = []

    for d in defs:
        addrs = comment_addresses(d["comment"])
        hit = set()
        for v, prim in addrs:
            agreed = False
            for cand, label in ((v, "ELF"), (v - GHIDRA_DELTA, "Ghidra")):
                r = row_at.get(cand)
                if (r and not r["info"]["thunk"] and same_linkage_scope(d, r["info"])
                        and names_agree(d["comps"], r["info"]["comps"])):
                    hit.add(r["info"]["sig"])
                    agreed = True
                    if prim:
                        conv[label] += 1
                    break
            if prim and not agreed:
                if 0x10000000 <= v < 0x11000000:
                    conv["PE"] += 1
                elif 0x08000000 <= v < 0x0A000000:
                    conv["bf2"] += 1
                elif v in row_at or (v - GHIDRA_DELTA) in row_at:
                    other = row_at.get(v) or row_at.get(v - GHIDRA_DELTA)
                    conv["mismatch"] += 1
                    ambiguous.append("%s:%d `%s` names 0x%X, which is `%s`" % (
                        d["file"], d["line"], d["qual"], v, other["symbol"][:90]))
                else:
                    conv["not-a-symbol"] += 1
        if hit:
            for sig in hit:
                matched[sig].append((d, "address"))
            continue
        k = scope_key(d["comps"], namespaces)
        cands = by_key.get((k[0], norm_fn(k[1])), set())
        cands = {c for c in cands if same_linkage_scope(d, groups[c][0]["info"])}
        if not cands:
            continue
        if d["template"] and len(cands) > 1:
            same = {c for c in cands if groups[c][0]["info"]["nparams"] == d["nparams"]}
            if same:
                for sig in same:
                    matched[sig].append((d, "template"))
                continue
        cands = disambiguate(cands, d, groups)
        if len(cands) == 1:
            sig = next(iter(cands))
            if not frontier_in(d["body"]) and (CHOSEN in d["body"] or CHOSEN in d["comment"]
                                               or "[FRONTIER]" in d["comment"]):
                chosen_only.append("%s:%d `%s`" % (d["file"], d["line"], d["qual"]))
                continue
            matched[sig].append((d, "name"))
        elif len(cands) > 1:
            ambiguous.append("%s:%d `%s` (%d params) matches %d overloads by name" % (
                d["file"], d["line"], d["qual"], d["nparams"], len(cands)))

    # declaration-only frontiers
    decl_frontier = set()
    for dc in decls:
        if "[FRONTIER]" not in dc["comment"]:
            continue
        k = scope_key(dc["comps"], namespaces)
        cands = by_key.get((k[0], norm_fn(k[1])), set())
        cands = disambiguate(cands, dc, groups)
        if len(cands) == 1:
            decl_frontier.add(next(iter(cands)))

    # class -> file maps
    class_files = collections.defaultdict(collections.Counter)
    for d in defs:
        k = scope_key(d["comps"], namespaces)[0]
        if k:
            class_files[k][d["file"]] += 1
            class_files[k.split("::")[0]][d["file"]] += 0  # register outer class
    class_decl = {}
    for name, chain, rel, line in classes:
        key = "::".join(chain + [name])
        cpp = os.path.splitext(rel)[0] + ".cpp"
        target = cpp if (rel.endswith(".h") and os.path.exists(os.path.join(REPO, cpp))) else rel
        class_decl.setdefault(key, (target, rel.endswith(".cpp") or target != rel))

    def class_file(key):
        for k in (key, key.split("::")[0]) if key else ():
            if k in class_files and class_files[k]:
                f, n = class_files[k].most_common(1)[0]
                if n > 0:
                    return f, 0
            if k in class_decl:
                f, real = class_decl[k]
                return f, 0 if real else 1
        return None, 1

    # statuses and files
    for sig, rs in groups.items():
        ms = matched.get(sig)
        if not ms:
            continue
        stop = any(frontier_in(d["body"]) for d, _ in ms)
        f = ms[0][0]["file"]
        for r in rs:
            r["status"] = "stop" if stop else "done"
            r["file"], r["inferred"] = f, 0
            r["why"] = "%s %s:%d" % (ms[0][1], ms[0][0]["file"], ms[0][0]["line"])

    for r in rows:
        info = r["info"]
        name = r["symbol"].split(" | ")[0]
        comps = info["comps"]
        key = scope_key(comps, namespaces)
        lib = (name in CRT_NAMES or name.startswith("__i686.get_pc_thunk") or info["thunk"]
               or (comps and comps[0] in LIB_NAMESPACES))
        if not r["file"] and "{anon}" not in comps:
            f, inf = class_file(key[0])
            if f:
                r["file"], r["inferred"] = f, inf
        if r["status"] in ("done", "stop"):
            pass
        elif lib:
            r["status"] = "skip-lib"
        elif info["sig"] in decl_frontier:
            r["status"] = "stop"
            r["why"] = "decl"
        if r["status"] in (None, "stop"):
            outer = key[0].split("::")[0] if key[0] else ""
            debug = (outer in DEBUG_CLASSES or DEBUG_CLASS_RE.match(outer or "")
                     or any(x in r["file"] for x in DEBUG_DIRS))
            if debug:
                r["debug_stop"] = r["status"] == "stop"
                r["status"] = "skip-debug"
        if r["status"] is None:
            r["status"] = "todo"

    # TU-based file inference for the rest: anonymous namespace / _GLOBAL__I_ keys, static init,
    # then the nearest neighbour with a known file
    mangled_row = {}
    for r in rows:
        for m in r["mangled"].split(" | "):
            mangled_row[m] = r

    # A TU key can identify a weak header member emitted far from its init block.
    # Only relocate unmatched rows: matched definitions always name their actual repo file.
    for g in rows:
        f = embedded_tu_file(g["mangled"]) if g["mangled"].startswith("_GLOBAL__I_") else None
        if f:
            key = g["mangled"][len("_GLOBAL__I_"):]
            for r in rows:
                if r["status"] == "todo" and key.startswith(r["mangled"]):
                    r["file"], r["inferred"] = f, 1
    def key_file(mkey):
        r = mangled_row.get(mkey)
        return r["file"] if r and r["file"] and not r["inferred"] else None

    def global_key(m):
        if m.startswith(("_GLOBAL__I_", "_GLOBAL__D_")):
            return m[len("_GLOBAL__I_"):]
        return None

    # GCC names an anonymous namespace after its file when the TU has no external symbol to
    # borrow ("_GLOBAL__N_Core_StringUtil.cppOckXud": the path with '/' as '_', then a hash)
    flat = {}
    for rel in cpps:
        if rel.startswith("Code/BF2/"):
            flat[rel[len("Code/BF2/"):].replace("/", "_")] = rel

    def anon_path_file(k):
        m = re.match(r"^(.*?\.cpp)", k or "")
        return flat.get(m.group(1)) if m else None

    def anon_key(m):
        km = re.search(r"(\d+)_GLOBAL__N_", m)
        if not km:
            return None
        n = int(km.group(1))
        start = km.end(1)
        return m[start:start + n][len("_GLOBAL__N_"):]

    # Static init / teardown blocks (__static_initialization_and_destruction_*, __tcf_*, then the
    # TU's _GLOBAL__I_): a TU's block follows its functions in address order, so the block belongs
    # to the TU of the functions between the previous block and this one. The _GLOBAL__I_ key
    # symbol wins when it names a function with a matched body; otherwise the majority file of
    # those functions (matched bodies first, inferred files second).
    global_rows = {}
    tu_files = collections.defaultdict(list)
    for rel in cpps:
        tu_files[os.path.basename(rel)].append(rel)
    # Original FILE spelling; keep the actual repository destination until it is renamed.
    tu_files["VehicleOccupied.cpp"] = tu_files.get("VehicleOccupancy.cpp", [])
    # The original DllModules.cpp initializer now lives beside AIModule's implementation.
    # Its key is a data symbol, so function-name matching cannot locate it. Never attribute
    # this block to the preceding unrelated FileManagerConsole functions.
    module_table_file = AI_ROOT + "Main/AIModule.cpp"
    if module_table_file in cpps:
        tu_files["DllModules.cpp"] = [module_table_file]

    def attribute_static_blocks():
        # a block is the run of adjacent static rows that ends in a _GLOBAL__I_; a static row
        # elsewhere (the teardown of a function-local static) goes with the function before it
        blocks, cur, lone = [], [], []
        last_fn = None
        for r in rows:
            if is_static(r):
                cur.append(r)
                if r["mangled"].startswith("_GLOBAL__I_"):
                    blocks.append(cur)
                    cur = []
            else:
                if cur:
                    lone.append((cur, last_fn))
                    cur = []
                if r["status"] != "skip-lib" and r["file"] and not r["file"].endswith((".h", ".inl")):
                    last_fn = r
        for blk, fn in lone:
            if fn is not None:
                for r in blk:
                    if not r["file"] or r["inferred"]:
                        r["file"], r["inferred"] = fn["file"], 1
                        r["why"] = r["why"] or "local-static teardown after %s" % fn["symbol"][:40]
        prev_end = -1
        for blk in blocks:
            g = blk[-1]
            start = blk[0]["addr"]
            # weak (COMDAT) and anonymous-namespace functions are emitted out of TU order; only the
            # TU's own strong functions locate it
            between = [r for r in rows if prev_end < r["addr"] < start and not is_static(r)
                       and r["status"] != "skip-lib" and r["file"] and r["bind"] in ("g", "l")
                       and not r["file"].endswith((".h", ".inl"))
                       and "{anon}" not in r["info"]["comps"]]
            strong = collections.Counter(r["file"] for r in between if not r["inferred"])
            weak = collections.Counter(r["file"] for r in between)
            if not weak:
                # only header-defined bodies (e.g. Cluster.h): their sibling .cpp is the TU
                weak = collections.Counter(
                    os.path.splitext(r["file"])[0] + ".cpp" for r in rows
                    if prev_end < r["addr"] < start and not is_static(r) and r["bind"] in ("g", "l")
                    and r["status"] != "skip-lib" and r["file"].endswith(".h")
                    and "{anon}" not in r["info"]["comps"])
            key = global_key(g["mangled"])
            embedded = embedded_tu_file(g["mangled"])
            originals = tu_files.get(g["tu"], [])
            original = originals[0] if len(originals) == 1 else None
            f = embedded or original or (key_file(key) if key else None)
            if f and f.endswith((".h", ".inl")):
                f = None            # a key defined in a header is an inline; it names no TU
            inferred = 1 if embedded else 0
            if not f:
                f = (strong or weak).most_common(1)[0][0] if (strong or weak) else None
                inferred = 1
            if not f and key:
                # nothing strong in between: the class the key symbol belongs to
                kr = mangled_row.get(key)
                if kr and kr["file"]:
                    f = os.path.splitext(kr["file"])[0] + ".cpp"
                    inferred = 1
            if f:
                for r in blk:
                    if not r["file"] or r["inferred"]:
                        r["file"], r["inferred"] = f, inferred
                        r["why"] = r["why"] or "static block after %s" % ("key" if not inferred else "neighbours")
            global_rows[key] = g
            prev_end = g["addr"]

    attribute_static_blocks()

    # anonymous-namespace symbols: the TU named by the namespace key
    for r in rows:
        k = anon_key(r["mangled"])
        if k and (not r["file"] or r["inferred"]) and r["status"] not in ("done", "stop"):
            f = anon_path_file(k)
            if not f:
                g = global_rows.get(k)
                f = g["file"] if g and g["file"] else key_file(k)
            if f:
                r["file"], r["inferred"] = f, 1
    known = [(r["addr"], r["file"]) for r in rows if r["file"] and not r["inferred"]]
    import bisect
    kaddrs = [a for a, _ in known]
    for r in rows:
        if r["file"]:
            continue
        i = bisect.bisect_left(kaddrs, r["addr"])
        prev = known[i - 1][1] if i > 0 else None
        nextf = known[i][1] if i < len(known) else None
        base = prev or nextf
        if not base:
            continue
        key = scope_key(r["info"]["comps"], namespaces)[0]
        outer = key.split("::")[0] if key else ""
        lib = r["status"] == "skip-lib"
        if (outer and not lib and "{anon}" not in r["info"]["comps"] and not is_static(r)
                and re.match(r"[A-Za-z_]\w*$", outer)):
            r["file"] = os.path.dirname(base) + "/" + outer + ".cpp"
        else:
            r["file"] = base
        r["inferred"] = 1
        if r["status"] == "todo" and any(x in r["file"] for x in DEBUG_DIRS):
            r["status"] = "skip-debug"
    # second pass: blocks whose functions only now have (neighbour-inferred) files
    attribute_static_blocks()

    # Documented static initialisation: a source comment that names a TU's static-init triad
    # ("static initialisation ... (ELF 0x..., __tcf_0 0x..., _GLOBAL__I 0x...)", or the short form
    # `// static init: ELF 0x...`) marks those rows done in that file. The triad is compiler output
    # of the statics the file declares (typically <iostream>'s ios_base::Init), so the comment is
    # the recovery claim.
    static_doc = 0
    for rel in cpps:
        text = open(os.path.join(REPO, rel), encoding="utf-8", errors="replace").read()
        for blockm in re.finditer(r"(?:^[ \t]*//.*\n)+", text, re.M):
            block = blockm.group(0)
            if not re.search(r"static init(?:iali[sz]ation)?\b", block):
                continue
            for hx in re.findall(r"0x[0-9A-Fa-f]{5,8}", block):
                r = row_at.get(int(hx, 16))
                if r and is_static(r) and r["status"] in ("todo", None):
                    r["status"], r["file"], r["inferred"] = "done", rel, 0
                    r["why"] = "static-init comment %s" % rel
                    static_doc += 1
    # Generated console bindings: tools/ai-reconstruct/gen_console_bindings.py lists every
    # ConsoleClass with its member functions and teardown functions; the emitted ones are done.
    console_done = 0
    if os.path.exists(CONSOLE_REPORT):
        rep = json.load(open(CONSOLE_REPORT))
        out = os.path.join(os.path.dirname(rep["output"]), "AIModule.cpp")
        for c in rep["classes"]:
            if c["verdict"] != "emit":
                continue
            for a in list(c["functions"].values()) + c.get("teardown", []):
                r = row_at.get(int(a, 16))
                if r and r["status"] in ("todo", None):
                    r["status"], r["file"], r["inferred"] = "done", out, 0
                    r["why"] = "generated %s ConsoleClass%d" % (rep["output"], c["line"])
                    console_done += 1
    print("static-init comments: %d rows done; generated console bindings: %d rows done"
          % (static_doc, console_done))

    # CRT startup and PIC thunks belong to no BF2 file
    for r in rows:
        n = r["symbol"]
        if n in CRT_NAMES or n.startswith("__i686.get_pc_thunk"):
            r["file"], r["inferred"] = "", 0

    # write the ledger
    os.makedirs(os.path.dirname(LEDGER) or ".", exist_ok=True)
    with open(LEDGER, "w", newline="") as f:
        w = csv.writer(f, quoting=csv.QUOTE_MINIMAL, lineterminator="\n")
        w.writerow(["address", "size", "symbol", "file", "status", "commit", "file_inferred", "mangled"])
        for r in rows:
            a = "0x%X" % r["addr"]
            commit = ""
            if r["status"] == "done":
                p = previous.get(a)
                commit = p["commit"] if p and p.get("status") == "done" and p.get("commit") else head
            w.writerow([a, r["size"], r["symbol"], r["file"], r["status"], commit, r["inferred"],
                        r["mangled"]])

    if args.explain:
        with open(args.explain, "w") as f:
            for r in rows:
                f.write("0x%X\t%d\t%s\t%s\t%s\n" % (r["addr"], r["size"], r["status"], r["why"],
                                                   r["symbol"][:160]))
    write_summary(rows, func_count, conv, ambiguous, chosen_only, namespaces, head)
    print("rows %d (function symbols %d), statuses %s" % (
        len(rows), func_count, dict(collections.Counter(r["status"] for r in rows))))


def write_summary(rows, func_count, conv, ambiguous, chosen_only, namespaces, head):
    st = collections.Counter(r["status"] for r in rows)
    sb = collections.Counter()
    for r in rows:
        sb[r["status"]] += r["size"]
    L = ["# AI DLL ledger summary", "",
         "Generated by `tools/ai-reconstruct/build_ledger.py` at %s from libaidll.so (%d function "
         "symbols, %d rows; one row per address)." % (head, func_count, len(rows)), "",
         "| status | functions | bytes |", "|---|---:|---:|"]
    for s in ("done", "stop", "todo", "skip-debug", "skip-lib"):
        L.append("| %s | %d | %d |" % (s, st[s], sb[s]))
    dstop = [r for r in rows if r.get("debug_stop")]
    L += ["", "skip-debug includes %d functions whose body is a [FRONTIER] stop (%d bytes); they abort "
          "if a debug path reaches them." % (len(dstop), sum(r["size"] for r in dstop))]
    L += ["", "Address convention of the first `ELF 0x...` in each definition comment: " +
          ", ".join("%s %d" % kv for kv in sorted(conv.items())) +
          " (ELF = matches the libaidll.so symbol by name; Ghidra = ELF+0x10000).", ""]

    def topdir(f):
        if f.startswith(AI_ROOT):
            rest = f[len(AI_ROOT):]
            return AI_ROOT + rest.split("/")[0] if "/" in rest else AI_ROOT + "(root)"
        return os.path.dirname(f) or "(unmapped)"

    def table(keyfn, title, limit=None, show_file=False):
        agg = collections.defaultdict(lambda: collections.Counter())
        files = collections.defaultdict(collections.Counter)
        for r in rows:
            if r["status"] not in ("todo", "stop", "done"):
                continue
            k = keyfn(r)
            agg[k][r["status"]] += 1
            if r["status"] in ("todo", "stop"):
                agg[k]["open_bytes"] += r["size"]
            files[k][r["file"]] += 1
        items = sorted(agg.items(), key=lambda kv: (-kv[1]["open_bytes"], kv[0]))
        if limit:
            items = items[:limit]
        L.append("## " + title)
        L.append("")
        hdr = "| %s | todo | stop | done | open bytes |%s" % ("key", " file |" if show_file else "")
        L.append(hdr)
        L.append("|---|---:|---:|---:|---:|" + ("---|" if show_file else ""))
        for k, c in items:
            extra = ""
            if show_file:
                f = files[k].most_common(1)[0][0]
                extra = " %s |" % f
            L.append("| %s | %d | %d | %d | %d |%s" % (k, c["todo"], c["stop"], c["done"],
                                                     c["open_bytes"], extra))
        L.append("")

    table(lambda r: topdir(r["file"]), "Per directory (todo/stop/done)")

    def cls(r):
        k = scope_key(r["info"]["comps"], namespaces)[0]
        k = k.split("::")[0] if k else "(free functions)"
        if k.startswith("ConsoleClass"):
            k = "ConsoleClass* (console bindings)"
        if is_static(r):
            k = "(static init/teardown)"
        return k

    table(cls, "Top 60 classes by open bytes (todo+stop)", 60, True)
    L.append("## Ambiguous or unmatched evidence (%d)" % len(ambiguous))
    L.append("")
    L += ["- " + a for a in ambiguous]
    L.append("")
    L.append("## Name-only matches left todo: body or comment says %s, or the comment says [FRONTIER] "
             "while the body has none (%d)" % (CHOSEN, len(chosen_only)))
    L.append("")
    L += ["- " + a for a in chosen_only]
    L.append("")
    os.makedirs(os.path.dirname(SUMMARY) or ".", exist_ok=True)
    with open(SUMMARY, "w") as f:
        f.write("\n".join(L))


if __name__ == "__main__":
    main()
