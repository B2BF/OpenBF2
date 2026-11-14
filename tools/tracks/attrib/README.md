# Renderer functional attribution

Run from the repository root with Python 3.9+ and Xcode's `objdump`/`c++filt`.
All binary analysis is read-only. Keep the private archive, listings, caches and
scratch outputs under `out/attrib/`; do not analyze a live build archive.

```sh
export REND_EVIDENCE_ROOT=/path/to/rend-track
mkdir -p out/attrib
ATTRIB_BUILD=out/build/attrib2  # completed build of this checkout's current sources
cp "$ATTRIB_BUILD/libbf2_renddx9_static.a" out/attrib/
python3 tools/tracks/attrib/extract.py
python3 tools/tracks/attrib/classify.py
python3 -m unittest discover -s tools/tracks/attrib -p 'test_*.py'
python3 tools/tracks/rend_ledger.py --attribution \
  --export "$REND_EVIDENCE_ROOT/out/rend-ghidra" \
  --ledger out/attrib/ledger.csv --summary out/attrib/ledger-summary.md
```

`extract.py --help` documents path overrides. `REND_GAME_DIR` overrides the read-only
retail installation. The evidence root supplies `out/rend-evidence/RendDX9.s` and
`out/rend-ghidra/RendDX9/functions.tsv`. The local checkout supplies the ledger,
debug-build matches and current sources. The archive snapshot must correspond to
those sources; hashes and the source commit are recorded for review. No downloaded
packages, live disassembler connection or source edits are needed for classification.

For round 10, import the **rebuilt** PE i386 RendDX9.dll and its matching local
MSVC PDB into a private Ghidra 12.1.4 project. This is same architecture, not the
same compiler: the supplied build uses MSVC 2022 while retail used VC7.1.

```sh
OUR_X86="$REND_EVIDENCE_ROOT/out/rend-evidence/our-x86"
GHIDRA="$GHIDRA_INSTALL_DIR/support/analyzeHeadless"
mkdir -p out/attrib/ghidra out/attrib/java-home
_JAVA_OPTIONS="-Duser.home=$PWD/out/attrib/java-home" "$GHIDRA" \
  out/attrib/ghidra attrib2 -import "$OUR_X86/RendDX9.dll" \
  -scriptPath tools/tracks/attrib \
  -preScript ImportPdb.java "$OUR_X86/RendDX9.pdb" \
  -postScript ExportPdb.java out/attrib/our-x86 -max-cpu 2 \
  -log out/attrib/ghidra.log -scriptlog out/attrib/scripts.log
python3 tools/tracks/attrib/x86_extract.py \
  --image "$OUR_X86/RendDX9.dll" --pdb "$OUR_X86/RendDX9.pdb"
python3 tools/tracks/attrib/classify.py --x86-features out/attrib/x86-features.json
python3 tools/tracks/attrib/x86_audit.py
```

The export contains function names, prototypes, sizes, body ranges, imported symbol
aliases and source file/line maps. `x86_extract.py` reads the same strings, imports,
direct calls, floats, constants and tables as the retail extractor. A unique
repository definition needs both its PDB name and its PDB file/line evidence;
basenames and an inlined callee's line alone do not establish ownership. Folded
aliases and ambiguous overloads remain unresolved. Input hashes bind the x86 cache
to the baseline features, image, PDB, listing and Ghidra exports.
Incremental-link thunks are resolved only when Ghidra marks the entry as a thunk
and its actual instruction is a direct jump. Raw instructions and the resolution
map are retained; no register-indirect target is invented. Documented source-file
splits explain original-TU filename differences without supplying identity votes.
Likewise, a direct call through an exact one-jump IAT thunk records the imported
API on the caller. The thunk itself is excluded from business-body API uniqueness
counts on both images. APIs are never propagated through arbitrary helpers.

The optional x86 pass keeps the earlier rules and exclusions. It first seeds from
cited/round-9 source identities, then pairs unique literals/imports and unique
offset/slot/retn/constant signatures, followed by call-graph and table propagation.
Uniqueness checks include x86 functions without source associations. Stack offsets,
scaled indexing and absolute addresses do not become field signatures. Offsets,
slots and return cleanup count as one evidence family. Graph observations count as
one family; high confidence needs two independent agreeing families or two distinct
unique literals/imports. Conflicts and source-file contradictions veto acceptance.
Explicit return-cleanup contradictions and retail leaves misclaimed as multi-call
bodies also veto pairings. A single literal needs corroboration that it belongs to
the full body rather than a wrapper. Weak round-9 pairs remain classified as before
but are not new graph seeds; cited seeds with these instruction contradictions are
also withheld. None of this rewrites the input ledger or prior classifications.
PDB identities whose source body stops or remains partial are reported separately
without upgrading them to complete matches. This pass makes no ABI/layout claims.
`x86_audit.py` withholds every fifth sorted source anchor and reports predictions
against those prior identities in `out/attrib/x86-holdout.json`. An incorrect
prediction fails this calibration check. Unresolved anchors are allowed; this is
not a total-coverage test or an independent validation of the original seeds.
An alias counts separately only when both PDB bodies contain exactly the same
instructions and operands and contain no direct calls. The two definitions remain
visible in the audit rather than being silently declared the same C++ identity.
Use `x86_extract.py --reuse-instructions` to refresh source associations after a
baseline refresh; image, PDB, listing and all export hashes must still agree.

The feature cache includes both PE i386 and Mach-O x86_64 features: exact strings,
named imports/API calls, ordered direct calls, float/integer constants, indirect jump
counts, PE vtables and native pointer tables. Mach-O relocation addends resolve local
literal references; branch relocations identify callees. All executable sections,
including `__StaticInit`, participate. Duplicate archive member names keep separate
identities. The brace-aware source parser associates native symbols with definitions
and identifies stopping/partial bodies. Generated console classes use the macro
invocation's closing-line `__LINE__`, checked against the actual source.

Explicit MSVC-order adapter arrays use paired slots. Genuine C++ tables require a
paired owner's table references, at least three agreeing ordinary-method anchors,
a measured slot delta and equal full table lengths. Console tables independently
measure their delta from at least four exact retail metadata getters and the native
getter names, after tying the class through its unique command literal, CRT caller
and installed native vtable. No destructor shift is assumed. Undefined renderer
vtable references to host methods require a unique non-stopping source definition.

PE body sizes are Ghidra address counts, not end offsets. Reachable instruction edges
bound getters and preserve multi-return functions and known switch arms. Entries that
lost synchronization in the linear listing are decoded again at their exact address.
An unresolved indirect jump retains the bounded listing and is flagged in evidence.

The CSV contains exactly the input ledger's RendDX9 `todo` addresses. The classifier
does not rewrite the input ledger, its statuses or engine source. Newly detected
source citations are `cited`; complete uncited definitions are `matched`.
Compiler/CRT/STL rules take precedence over apparent function matches, then complete
definitions, inlining, editor scope and missing. Missing is a conservative, low
confidence review queue; it does not prove that the source body is unwritten.

Each full evidence cell is JSON: anchors, strings, imports, raw direct caller callsites,
callees/tail calls, indirect calls/jumps, constants, `_r` file evidence, inferred-file
basis, incomplete-source candidates and CFG limitations. The report gives counts
and bytes, every missing address grouped by probable file, the 50 largest, named
source frontiers and denominator gaps. Extra vtable-only, CRT-table and EH entries
must be reconciled before claiming a total-image recovery percentage.
The committed CSV keeps only `anchors` and `probable_file`; the complete evidence
is written to `out/attrib/attribution-evidence.csv`.

Each rerun is deterministic for fixed inputs. Delete `native.s` and its
`.archive-sha256` sidecar when changing the snapshot; mismatched snapshots are
rejected. The source parser cache is invalidated by a hash over the scanned sources.
Binary evidence and source bodies are cached only under `out/`. Classification
refuses changed source/image/listing/map/ledger inputs. CSV/report publication is
atomic, using scratch files under `out/attrib/`. The ledger command's output overrides
above keep its regeneration out of the original documents; without `--attribution`,
its status logic and columns are unchanged.
