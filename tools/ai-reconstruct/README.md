# AI reconstruction tools

| Tool | Purpose |
| --- | --- |
| `build_ledger.py` | `docs/ai-reconstruct/ledger.csv`: one row per libaidll.so function, with its status. |
| `gen_console_bindings.py` | Console bindings (`AIModuleConsole.inc`) from libaidll.so. |
| `gen_static_init.py` | Verify static-init/teardown instruction shapes and recover matching source-level definitions, one directory per build. |
| `evidence.py` | Evidence cache plus per-function listings, decompiles and vtable dumps (see below). |
| `ghidra/DecompList.java`, `ghidra/ImportInfo.java` | Ghidra headless scripts that `evidence.py` runs. |

All tools are Python 3 standard library and run from a plain shell. They need `objdump` and
`c++filt` (llvm, as shipped with Xcode) and, for decompiles, Ghidra in `$GHIDRA_INSTALL_DIR`
(required for those commands). The binaries are read from
`$BF2_LINUX_SERVER_DIR/bin/{ia-32,amd-64}/libaidll.so` (the Linux 1.5 server; `--binary` or
`EVIDENCE_BIN_DIR` override that). None of them needs the rea MCP. The full CLI reference is in the `evidence.py`
docstring (`python3 tools/ai-reconstruct/evidence.py -h`).

## Ledger source discovery

`build_ledger.py` scans the aidll source manifest, AI headers, and all Math/Core/IO
`.cpp`, `.h` and `.inl` files, including shared inline bodies without a manifest entry.
Address comments keep their image exclusions; scanning a file does not itself mark a
function done. Original ELF FILE symbols and GCC TU-key path suffixes take precedence over neighbour inference for
unmatched rows such as DangerObject's initialization block.

For agent review without changing the coordinator's ledger:

    python3 tools/ai-reconstruct/build_ledger.py --ledger out/ledger.csv --summary out/ledger-summary.md --explain out/ledger.tsv
    python3 Code/Tests/test_ai_ledger.py

The selected ledger also supplies previous commit values when it already exists. Use a
fresh destination to compare scans of the current source independently of tracked status.
`file` names the current repository location: the original `VehicleOccupied.cpp` is
currently represented by `AI/AIUtils/VehicleOccupancy.cpp`; a source rename must update
the manifests too.

## Console binding generation

`gen_console_bindings.py` emits all 298 console classes in the AIModule translation unit. Its 17
manually inspected inline executors require matching instruction hashes, registration signatures,
argument counts, named field declarations and zero-offset base subobjects. A mismatch leaves the
binding omitted with a reason. These cases preserve discarded getters, the ineffective area-name
setter, unchecked side indices and repeated status stores. `AI.pause` resolves its two virtual
targets from the original ia-32 engine's BFTimer vtable and typeinfo; `--engine PATH` overrides that
image. No engine virtual slot is inferred from a header alone.

Use `--verify-slots BUILD_DIR` to compare the generated calls and complete available primary tables
against clang's native layouts, including the imported timer table. `--check` checks regeneration
without writing the output. `--ledger PATH` reads a local ledger snapshot when the coordinator owns
the tracked ledger; the binding generator itself never writes the ledger. The per-class JSON includes
the inline instruction hashes. Successful generation and linking do not establish runtime dispatch
or gameplay acceptance.

## Evidence cache

`out/ai-evidence/` is git-ignored and holds evidence for every ledger row with status `todo`,
`stop` or `skip-debug` (2875 rows at 26db6034). `ADDR` is the ia-32 ELF address, formatted `0x%X`:

- `ADDR.s`: the ia-32 listing in Intel syntax. GOT slots, GOT-relative data, rodata constants
  (decoded by operand width as float, double or long double, plus strings), jump tables and call
  targets are resolved and demangled. Each `call [reg+N]` is tagged with its vtable slot.
- `ADDR.c`: the ia-32 Ghidra decompile.
- `ADDR.amd64.s` and `ADDR.amd64.c`: the same function in the x86-64 build. It is found by mangled
  name; local static-init names are matched through their translation unit's `_GLOBAL__I_`; and
  names that differ only in `size_t`/`uint64` spelling are matched after merging those types.
  `index.csv` (column `amd64_match`) records which of these methods matched each row.
- `index.csv`: one row per cached function, with address, symbol, size, file, status, kind
  (function or static-init), the amd-64 address/size/match method, the file paths, and a decompile
  status for each image (`ok`, `fail: <message>`, `nofunc`, `missing`, `no-amd64-symbol`).
- `vtables/<class>.txt`: one file per `vtable for` symbol in the ia-32 image. Each line gives the
  word offset, the slot relative to the address point (`vptr+0xNN [index]`), the target address and
  the demangled target. Secondary vtables start a new group at their typeinfo word.
  `vtables/index.csv` maps each class to its file.
- `ghidra/<image>/libaidll.gpr`: the private Ghidra projects. `ghidra/<image>/jobN/` holds one copy
  per parallel decompile job.

Build or refresh the cache. Existing files are kept unless you pass `--force`; `--only 0xA,0xB`
limits the build to the listed rows.

    python3 tools/ai-reconstruct/evidence.py build                 # listings + decompiles + vtables
    python3 tools/ai-reconstruct/evidence.py build --no-decompile  # listings + vtables, ~35 s

A build from scratch takes about 20 minutes on an 8-core Mac. It imports both binaries into fresh
Ghidra projects (about 7 minutes, the two images in parallel), writes the listings (about 35 s), and
decompiles 2875 ia-32 and 2826 amd-64 functions (about 10 minutes, three headless sessions at a
time with about 200 functions per session, the large functions spread evenly over the sessions).
Known failures: the ia-32 decompile of the 113 KB `AIModule.cpp` static initialiser (0x3EEE90)
times out, and 6 amd-64 decompiles fail. `index.csv` lists both.
`evidence.py project [--from GPR] [--create]` creates or locates a project by itself. It first
reuses `out/ai-evidence/ghidra/<image>/`. Otherwise it copies an existing project from
`$GHIDRA_PROJECTS_DIR` (when set), or from `--from`, if that project holds the same binary (matched by MD5); the
original is never written to. As a last resort it imports the binary. Ghidra's settings and temp
files go to `out/ai-evidence/ghidra/{home,tmp}`, so a sandbox that only allows writes inside the
worktree still works. Rebuild after the ledger changes so that new open rows get their files.

## Using it as a recovery agent

    python3 tools/ai-reconstruct/evidence.py fn 'BBIdle::waitForPickup'   # listing + cache paths
    python3 tools/ai-reconstruct/evidence.py fn 0x2156c0 --att            # AT&T syntax
    python3 tools/ai-reconstruct/evidence.py --image amd-64 fn 0x278e60   # x86-64 listing
    python3 tools/ai-reconstruct/evidence.py fn 0x2156c0 --decompile      # decompile if not cached
    python3 tools/ai-reconstruct/evidence.py vtable BAPAMoveTo            # slot dump
    python3 tools/ai-reconstruct/evidence.py calls out/ai-evidence/0x2156C0.s   # call/argument summary
    python3 tools/ai-reconstruct/evidence.py strip-checkpointer out/ai-evidence/0x2156C0.c

Look up rows in `out/ai-evidence/index.csv` with `grep`, then read the `.s` file first and the
`.c` file second. To use a cache built in another worktree, set
`EVIDENCE_OUT=<that worktree>/out/ai-evidence`. Reading that cache needs no write access, but
`--decompile` and `build` write into it. All addresses are ELF addresses. Ghidra adds 0x10000 (ia-32) or 0x100000
(amd-64); the listing header prints both.

## Limits

- **The ia-32 listing is the source of truth.** Ghidra drops bool (byte) arguments, merges 64-bit
  masks, loses the stack after sret calls (hidden struct-return pointers), and misreads x87
  register stacks and `long double` returns. Use the `.c` file to find structure, then confirm
  every argument, constant and offset in the `.s` file.
- **The amd-64 build helps you understand a body only.** Register arguments decompile more
  cleanly, but its layouts, offsets, stack slots, vtable byte offsets and inlining decisions are
  not those of the 32-bit retail image. Addresses, layouts and offsets come from ia-32 only. 49
  rows have no amd-64 counterpart, for example templates inlined on x86-64 or anonymous-namespace
  names that differ.
- Register tracking in the listing is linear, not flow-sensitive. `ebx` stays the GOT base once
  `get_pc_thunk.bx` sets it up, and any other register loses its known value on its next write. A
  `vcall?` tag only reports a `call [reg+N]`; the receiver type is not known.
- A jump table's size comes from the nearest `cmp idx, N` before it. When no such compare is found
  (common on amd-64), entries are decoded as long as their targets stay inside the function, and
  the table is marked `(bound not found)`.
- Decompiles are of the auto-analysed program: no types, names or fixes from the rea project.

## Static initialization

`gen_static_init.py` reads every static-init, `_GLOBAL__I_` and `__tcf_` ledger row. It uses
`evidence.py`'s ELF reader and listings directly without writing the evidence cache. Its JSON
report includes each row's address, calls and referenced symbols, source placement, ordered
objects/constructor arguments where verified, and the reason an unsupported row remains open.

    python3 tools/ai-reconstruct/gen_static_init.py --report out/static-init.json
    python3 tools/ai-reconstruct/gen_static_init.py --apply --directory AIBotActionPlan
    cmake --build out/build/<tag> --target bf2_aidll bf2 bf2_vtable_audit -j2
    python3 Code/Tests/test_ai_static_init.py

Apply and validate one directory before starting the next. The script excludes the files
reserved in task g2-static. Existing ledger attribution is cross-checked against `_GLOBAL__I_`
keys and embedded source paths. Multiple original TUs mapped to one current file, absent TUs, unverified types, and unrecognized instruction shapes require
manual review; the script emits no guessed definitions and never changes manifests or the ledger.
Data-member keys are resolved against their namespace-scope source definitions too: they have
no function-ledger row, and adjacency can otherwise hide merged TUs. Report analysis completes
before source writes; a failed source/report write restores only the generator's own edits.

Supported source edits are `<iostream>` for a fully verified iostream-only triad and an ordered
`std::string` array with its fully verified reverse teardown. A string definition must match its
namespace, type, extent and every literal; a missing array is emitted only after a conservative
search for competing symbols. The Init include precedes source definitions. The compiler supplies
the init/teardown functions: no `__tcf_` or `_GLOBAL__I_` bodies are copied into C++.

Address comments use `build_ledger.py`'s `// static init: ELF ...` convention. A reported shape or
an existing ledger `done` status is not an additional recovery claim. The script leaves other
production frontiers in place. A green strict link and vtable audit validate compilation and
symbol resolution, not server startup, gameplay, or the remaining static objects' behavior.
