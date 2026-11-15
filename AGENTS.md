# BF2Rebuilt agent instructions

This is the canonical policy for every coding agent working in this repository. Tool-specific
instruction files must import or point here instead of copying these rules. If guidance conflicts,
the user's current request wins, followed by this file, then the subsystem documentation.

## Code Review Rules

For code review and regression investigation, also read [docs/reviewing.md](docs/reviewing.md).
This file remains the canonical policy. Review alone does not authorize source changes;
explicit user authorization to implement fixes still applies.

- **Report concrete regressions.** Review the PR diff and affected callers for memory/lifetime
  errors, initialization and teardown failures, malformed-input handling, and broken builds.
  Explain the reachable trigger and consequence; distinguish pre-existing defects from changes
  introduced by the PR. Legacy style, raw pointers, lack of Asio, and opportunities to adopt
  newer C++ are not defects by themselves.
- **Preserve the applicable compatibility contract.** Check ABI, protocols, serialized data,
  and reconstructed behavior for each affected image and architecture. Respect the documented,
  issue-backed native ABI and editor exceptions; do not restore retired compatibility adapters
  or infer unknown signatures and layouts. State missing binary evidence and continue checks
  that do not depend on it.
- **Account for parallel upstream work.** Where accessible, inspect relevant B2BF/OpenBF2 PRs
  and branches at their current SHAs. Identify semantic conflicts and existing fixes with
  evidence; upstream code is not automatically proof of correctness. State unavailable context
  and continue the scoped review instead of assuming that another branch was checked.

## Agent prerequisites and human ownership

- Before implementing behavior as recovered engine code, the agent must have **IDA or Ghidra
  connected and accessible**, with the relevant original Battlefield 2 1.50 binaries loaded. Verify
  the image and architecture, and inspect its instructions, symbols and cross-references directly.
  Having only source comments, old transcripts or a plausible implementation is insufficient.
- If the required tool, original image or evidence is unavailable, stop the affected recovery work
  and report what is missing. Do not guess signatures, layouts or behavior. Documentation, build
  maintenance and issue-backed intentional changes can proceed independently when they do not
  depend on the missing evidence. For non-retail work, follow the approach already discussed in
  the linked issue and inspect the original only where needed to establish affected behavior or
  compatibility contracts.
- **Agents must not create or submit pull requests, or reply to reviewer questions.** Prepare local
  changes and validation results for the human author instead.
- The human author must fully understand every submitted code change, including its behavior,
  evidence, compatibility constraints and validation. The human creates the pull request, answers
  review questions and takes responsibility for the submission; agent assistance does not transfer
  that responsibility.

## Mission

Reconstruct Battlefield 2 1.50 accurately enough to run a stable dedicated server and let
an unmodified retail client connect, load, join, spawn, play, disconnect, reconnect, and survive a
map reload.

The project also includes the client, native server ports and a level editor. Validate the
production paths and compatibility contracts affected by each change; compilation and harness
success alone do not establish working gameplay.

## Intentional fixes and improvements

- Fidelity is the default for reconstructed code. A non-retail fix, improvement or feature is
  allowed only when a linked issue already contains discussion of the proposed approach before
  implementation begins. This workflow also applies to editor-only features and fixes.
- AI assistance is allowed for these changes. Agents may implement and validate the issue-discussed
  design; the human author retains the submission and review responsibilities above.
- Link the issue in the change description or documentation, and in source comments where needed
  to explain a deliberate departure from retail behavior. Identify intentional policy as
  `[CHOSEN, not recovered]`; never describe a new design as recovered behavior.
- Discuss affected ABI, protocol and file-format contracts in the issue when the approach changes
  them. Verify original behavior where needed to understand those contracts. An intentional change
  is not permission to guess an unknown signature, layout or recovered implementation.

### Editor

The BF2 Editor's code is in `Code/Tools/BF2Editor/`; public setup and design guidance lives in
`docs/getting-started.md`,
`docs/building.md` and `docs/architecture.md`. It builds with the `bf2_editor*` CMake targets.

- **Target the tree's current engine** (1.50), not the 2006 engine the retail editor was built on.
- **Retail-editor parity is not required** for editor-only code: the editor application, its UI, its
  editing modules and its engine-side editor layer. Use a modern stack within the issue-discussed
  approach. The retail editor is a behaviour and file-format reference, never a layout or ABI
  to reproduce.
- **Code shared with the game images defaults to retail fidelity**, including
  editor-facing code that ships in retail game images (RendDX9's `TerrainEditable` and lightmap
  generation, `HeightmapEditable`). Editor-only needs in shared TUs go behind
  `BF2_IMAGE_EDITOR`. Deliberate game behavior changes follow the intentional-change workflow.
- **Editor output must stay loadable by the retail game.** File formats and `.con` semantics are the
  compatibility contract; prove them with golden files from the retail editor.
- **Source control is an interface**, and `NullSourceControl` is registered by default. No Perforce
  dependency.
- Unfinished editor features fail visibly (log and refuse); they never report a success they did
  not achieve.
- Editor play-testing starts a local server, so the port-16567 rule below applies.

## Mandatory preflight

Before editing:

1. Read this file completely.
2. Run `git status --short`. This is a shared, usually dirty worktree; assume every existing change
   belongs to the user or another agent.
3. Read `docs/development.md`, `docs/building.md` and the relevant parts of `docs/architecture.md`.
   Identify the target image, its source manifest, and the current implementation before editing.
4. Search the touched subsystem for `[FRONTIER]`, `[UNRESOLVED]`, `[TEMP]`, `[CHOSEN`, and
   `[RETIRE-WHEN:`. Inspect each affected boundary, its callers and its removal condition before
   changing it. An unverified boundary stays in place.
5. Check whether another agent is modifying the same files. Partition work or coordinate before
   editing overlapping code.
6. For recovered engine implementation, verify the IDA/Ghidra connection and relevant original
   image before editing. Missing evidence is a recovery blocker, never a reason to infer a body.
7. For an intentional non-retail change, read the linked issue and confirm that it already
   discusses the approach. If that discussion is missing, stop the affected implementation and
   report the missing prerequisite.

Current source and freshly inspected PE/ELF instructions take precedence over old comments or
link counts. Verify source placement against original `__FILE__` strings or translation-unit
symbols before choosing or moving a recovered file; label unattested placement `[INFERRED]`.

## Fidelity rules

- **Editor-only code is exempt from retail parity** (see *Editor* above). The intentional-change
  workflow applies to non-retail work; the rules below govern reconstruction claims.
- **A recovered body goes in only after its instructions have been read.** A plausible
  implementation is not recovered source.
- Cross-check the symbolized Linux ELF and Win32 PE whenever possible. Use mangled names for
  parameter types and constness; use callers and explicit return materialization for return types.
- Preserve shipped ordering, repeated virtual calls, null-guard behavior, signedness, side effects,
  observable quirks, and documented defects. Do not silently “improve” retail behavior.
- In a recovery change, add guards only when they exist in the retail PE/ELF and preserve observed
  defects. Bug, exploit and crash fixes that intentionally change retail behavior require the
  linked issue discussion described above; distinguish the fix from the recovered baseline.
- Public signatures, inheritance, virtual declaration order, object ownership, serialized layouts,
  bit widths, event ids, and packet state transitions are ABI/protocol contracts. Reconstruction
  changes require direct evidence. Intentional contract changes require the issue-discussed
  approach and compatibility impact to be documented. Update every affected override/caller
  atomically.
- Prefer typed C++ virtual calls. Itanium commonly emits two destructor slots while MSVC emits one;
  fixed raw vtable offsets often work on only one port. If a raw seam is unavoidable, isolate it,
  record both PE and ELF evidence, and make genuine ABI differences explicit.
- The retail layout reference is **x86/32-bit**. Never validate a retail layout with an x64 build;
  native ports need their own layout checks.
- Retail libstdc++ and MSVC STL layouts differ, and the current compiler's STL may differ from both.
  Never copy an STL-containing class size or member offset across builds without evidence.
- An attested `__FILE__` or anonymous-namespace path overrides namespace-based placement guesses.
- Preserve source-level STL operations; do not reproduce decompiler-emitted allocator/refcount
  internals.
- Do not reverse third-party code under `Code/BF2/External` when the matching upstream source is
  available.

Use the repository's evidence vocabulary consistently:

- `[INFERRED]`: a name, path, ordering, or interpretation that fits but is not directly attested.
- `[UNRESOLVED]`: an observed call/value whose identity is still unknown; retain raw evidence.
- `[PLATFORM]`: behavior genuinely differs between PE and ELF, with both sides recorded.
- `[CHOSEN, not recovered]`: intentional scaffolding or functional policy, not a fidelity claim.
- `[FRONTIER]`: known declaration or narrow named seam whose real body remains unwritten.
- `[TEMP]`: test/harness-only fixture.
- `[RETIRE-WHEN: tag]`: load-bearing debt; the accompanying comment must state the implementation
  and validation conditions required to remove it.

## No silent stubs

- Never add a production `return true`, empty body, zero object, or fake interface merely to make a
  link or drive pass. Leave a named unresolved frontier so the missing behavior remains visible.
- Whenever a known-signature frontier needs a temporary callable body before its retail body is
  recovered, its first executable action must emit and flush a diagnostic containing
  `[FRONTIER]` and the fully qualified method name (for example,
  `std::fprintf(stderr, "[FRONTIER] dice::hfe::Class::method reached\n"); std::fflush(stderr);`).
  After the diagnostic, preserve an unmistakable stop such as `std::abort()`; never return a
  fabricated success/default value and let execution continue. Mark the body `[TEMP]` and
  `[CHOSEN, not recovered]`, and add its retirement condition. Prefer `stderr` for early-boot
  frontiers because `BF2Log` may not be initialized yet. If the signature/ABI is not known well
  enough to define a safe callable body, keep the symbol declaration-only and document the caller
  boundary instead of inventing a signature merely to add logging.
  **Unimplemented-slot exception (owner decision 2026-11-02, libaidll reconstruction):** a
  virtual slot that is `__cxa_pure_virtual` in every vtable of every class that has it, in
  every inspected image, has no body anywhere. Reaching it in retail aborts with "pure virtual method
  called". The same applies to a class member that no instruction reads or writes. Its name and
  source-level type cannot be recovered, but its behaviour is fully determined.
  - Such a slot or member may be declared with a `[CHOSEN, not recovered]` name, so that the
    surrounding recovered code can be completed.
  - The declaration must reproduce what the instructions prove:
    - the slot position in both ELF vtables;
    - the receiver and argument registers or stack slots each call site prepares;
    - whether any caller reads a result (if none does, declare `void`; if a caller tests `eax`/`al`,
      declare the narrowest integer type the test proves);
    - for a member, the size and alignment implied by the neighbouring offsets in each image.
  - Keep the slot pure (`= 0`); never give it a body. Every call site stays exactly as the listing
    shows.
  - Cite the vtable addresses and call-site addresses beside the declaration.

  This covers only slots and members with no body and no access. A slot that has an override
  anywhere, or a member that is accessed anywhere, keeps the ordinary evidence rules. The matching
  build (`MATCHING_BUILD_PLAN.md`) re-checks every such declaration at its call sites.

  **Unobservable-type exception (owner decision 2026-11-05, RendDX9 reconstruction):** a body may be
  recovered with a `[CHOSEN, not recovered]` type or name when all of the following hold:
  - no inspected image (retail PE, `_r`, editor, Linux ELF) has a caller that reaches it, or every
    reachable path only ever sees empty data, so its behaviour is fully determined;
  - the instructions of every image have been read and the body reproduces everything they prove
    (reads, writes, offsets, call order, slot dispatch, return register);
  - only a name or a type that no instruction can expose is missing. Examples: the element type of a
    container that is never filled, or the name of a property pair that nothing calls.

  Mark each chosen piece `[CHOSEN, not recovered]` beside it, with the evidence (addresses in every
  image, and why the choice cannot be observed). Prefer the choice the instructions point to, such as
  float for an `st0` return. This replaces a [FRONTIER] stop only in that situation; a body that any
  path reaches with real data keeps the ordinary evidence rules. First applied to: LightManager +0xBC/+0xC0
  (PE 0x1015C130/0x1015C140), GeometryTemplateManager +0x2C/+0x30 (PE 0x1015D940/0x1015D430) and
  ObjectDrawer::flushDeferredGeometry (PE 0x100F8C00).
- Test fixtures belong under `Code/Tests`, must be labeled as scaffolding, and may
  only model the specific condition the test needs.
- When a production implementation lands, remove any Probe/harness override that would shadow it.
- `/FORCE` is a diagnostic instrument, not proof of link completeness. A force-linked smoke proves
  only the path it actually executed.
- An unresolved-symbol report is a work list. Compare symbol sets, not just totals: recovering one
  body may honestly expose deeper dependencies without lowering the count.
- Do not register a factory, event maker, or vtable-bearing class while required virtual bodies are
  missing. Retire the matching marker only when the whole boundary is safe.
  **Client startup exception:** advance the reconstructed client in retail
  order even while later HUD dependencies remain frontiers. Replace broad publication stops
  with diagnostic-and-abort bodies at actual use when signatures are known. Unknown-signature
  boundaries must stop at their known caller; never invent virtual slots or return success.
  This permits diagnostic startup progress, not a claim that the published subsystem is complete.
  **LocalProfileManager slot exception:** the native rebuilt client may
  reserve the attested primary +0x30 and +0x34 positions with explicitly chosen void() declarations
  whose bodies first log their qualified `[FRONTIER]`, flush stderr, and abort. These two
  placeholders permit later proven methods to retain their original virtual positions; they do
  not claim recovered signatures and must never return, forward arguments, or bridge a retail DLL.
  Retire them when matching type evidence permits the real declarations and bodies. This exception
  covers only these two positions, not other unknown signatures or skipped startup dependencies.

  **voipStopTest frontier exception:** the frontier inside
  `as_sound_voipStopTest` (the Bf2VoipClient arm of PE 10069620) REPORTS AND CONTINUES instead of
  aborting, and answers the movie with the Boolean `false` that the shipped body itself publishes on
  its own refusal path. Preserve this narrowly scoped continuation behavior.
  It stays honest on four counts, and any future exception of this shape should meet the same bar:
  it names itself fully on every report, to both the log and stderr; it carries a hit count and is
  rate-limited rather than silenced; the value it returns is one the SHIPPED body can also produce
  in a refusal, never a fabricated success; and the surrounding body is written as far as the
  evidence honestly reaches, so the frontier covers only what is genuinely unrecovered. This
  exception covers only this call site. Every other frontier keeps its stop.

  **Sound-seam exception:** a frontier whose ONLY unrecovered effect is
  AUDIBLE - starting or stopping a named engine sound - REPORTS AND CONTINUES instead of aborting.
  This covers the calls into the still-unidentified sound owner at `ds:A23DC0` (+0x2C starts a named
  sound, +0x34 stops one) and any future seam of the same shape. It meets the same four-point bar as
  the `voipStopTest` exception above: the report names its caller fully every time and carries the
  sound it would have driven; each call site keeps its own hit count and is rate-limited, never
  silenced; NO value is fabricated, because every shipped call site discards the result, so
  continuing returns nothing at all; and the surrounding body is written as far as the evidence
  honestly reaches, so the seam covers only the sound call. No sound seam remains (the shared
  helper `Core/SoundSeamFrontier.h` was removed with its last caller); a future sound seam re-adds
  that helper with this rule. It reports to stderr only, deliberately, because
  `Debug(DebugTypeError, ...)` aborts in this tree and `BF2Log` is not guaranteed to be up on the
  HUD paths that reach these seams. The cost is real and is the point: a sound retail would start or
  stop keeps doing whatever it was doing. This exception covers sound seams ONLY - every other
  frontier keeps its stop.

## Working in the shared tree

- Preserve all unrelated modified and untracked files. Never use `git reset --hard`, broad checkout,
  clean, or mass deletion. Do not stage or commit unless the user asks.
- Use small, scoped patches. Avoid broad formatting, line-ending churn, and drive-by cleanup.
- Search with `rg`/`rg --files`. Use `apply_patch` for hand edits.
- Never overwrite the original `bf2_w32ded.exe`, its `.i64`, original DLLs, or the reference game
  directory. Put rebuilt binaries in a separate output directory.
- **Port 16567 is reserved for the owner's testing sessions.** Every agent-run server test,
  including test flights, gameplay tests, boot smokes, soaks, and shutdown/rebind probes, must use
  an explicitly configured, unused port other than 16567. Using 16567 can kick the owner from an
  active session. Keep the test configuration's `sv.serverPort`, harness `BF2_PORT`, and any test-client target
  consistent; never rely on the default port or stop/restart the owner's server to free it.
- Full builds clean their output directory. During concurrent work, set `BF2_BUILD_OUT` to a unique
  agent-owned directory; never reuse or delete another agent's build directory or trust shared logs
  that another build may be rewriting.
- The repository contains multiple shipped images with distinct source manifests. AIDLL-only
  translation units do not belong in the engine image. Do not replace the
  manifest with a recursive object glob or link `Core/DllModule.cpp` into `bf2_w32ded`.
- Do not leave ad-hoc extraction dumps, manifests, or link logs in `Code/BF2`. Put temporary analysis
  under an agent-owned build/temp directory and retain only intentional evidence artifacts.
- Every compile line that touches `Code/BF2` states which image it builds: `/DDEDICATED=1` for
  `bf2_w32ded.exe` and every server-side gate, `/DDEDICATED=0` for `BF2.exe` and the client-side DLL
  images, always with `/FI` on `Code/BF2/Core/BuildConfig.h`, which refuses to compile without the
  define. Client-only code goes under `#if !DEDICATED` or into the client-only file manifest;
  behaviour the retail client switches at run time stays a real `if (!isDedicated())`.

## Validation

Use MSVC x86, C++14, permissive mode. Validate in proportion to the change:

```powershell
# One real-header translation unit
cmd /c Code\Tests\syntaxcheck.cmd Code\BF2\path\File.cpp

# Full syntax/semantic gate
cmd /c Code\Tests\syntaxcheck.cmd

# Focused recovered Object/physics runtime suite
cmd /c Code\Tests\runtime_pair.cmd

# Boot harness compile check
cmd /c Code\Build\_bmcheck.cmd

# Isolated full compile and diagnostic links (choose a unique directory)
$env:BF2_BUILD_OUT = "$PWD\Code\Build\obj_agent_name"
cmd /c Code\Build\build.cmd
Remove-Item Env:BF2_BUILD_OUT
```

The native Linux x86-64 port (`bf2` + `libdice_py.so`) builds with
GCC/Clang through the `linux-x64` preset; from this Windows checkout use the WSL distro with an
agent-owned build directory outside `/mnt/d`:

```bash
cmake --preset linux-x64 -B ~/bf2-linux-<agent>      # needs cmake, ninja, g++, libncurses-dev
cmake --build ~/bf2-linux-<agent> --target bf2 dice_py_check
```

A change to a shared header must keep all three toolchains green: the MSVC gates above, the
`msvc-x64` strict link, and the Linux build. Recovered Linux-only bodies follow the same evidence
rules: read the POSIX arm of a `#if defined(_WIN32)` seam from the ELF. Intentional port changes
follow the linked issue's discussed approach and must not be presented as recovered behavior.

For new `.cpp` files, ensure both recursive build discovery and the explicit full syntax gate include
them. After changing a widely included header, interface, virtual signature, or layout, run the full
syntax gate and an isolated all-TU build—not only the local translation unit.

Use the CMake targets documented in `docs/building.md` when appropriate. Keep in mind that a no-drive boot
smoke does not exercise `GameServer::startServer`, socket binding, map loading, packet dispatch, or a
client join. Report exactly which environment flags and branches a run exercised.

For server runtime claims, use the actual `BF2::init -> BF2Engine::startGame -> GameServer`
path and representative content. A fixture, injected map list or direct `loadPath` call proves
only its selected boundary. Exercise the changed lifecycle: loading, ticks, client sessions,
unload/reload, shutdown, receive-thread join and immediate port reuse as applicable. Report the
duration and configuration of stability runs. A factory registration or unresolved vtable proves
neither working dispatch nor gameplay.

For networking changes, test the affected packet paths and rejection of malformed or unknown
input. Recover retail protocol transitions from original server/client packet evidence and binary
instructions; intentional changes follow the issue-discussed design. Keep packet captures and
golden fixtures free of credentials and personal data.

## Documentation and reporting

- New engineer guides live in `docs/` and explain the build, architecture and contribution process.
  State contracts, limitations and safety requirements directly so each guide is usable on its own.
- Write normal API comments for classes and functions: purpose, meaningful inputs, return value,
  and caller-visible preconditions or side effects. Add short remarks for ownership, ordering,
  ABI constraints or live frontiers only when a reader needs them. Do not repeat the signature.
- Do not put decorative banners, emoji, all-caps emphasis, completion dates, byte counts,
  disassembly addresses or investigation narratives in API comments. Keep necessary layout
  offsets beside the fields or ABI checks they explain. Preserve licenses and attribution notices.
- When behavior or evidence changes, update the affected source contract and relevant engineer
  guide. Keep each temporary boundary's removal condition beside its `[RETIRE-WHEN: tag]` marker;
  remove it only after the required implementation and validation are complete.
- A change report must state: files changed, recovered behavior and its PE/ELF evidence or the
  linked issue and intentional design change, tests run and their outcomes, paths not exercised,
  and every remaining named frontier relevant to the change.
- Distinguish clearly among compile success, diagnostic-link status, `/FORCE` smoke reach, production
  startup, socket-listener reach, loading-screen reach, joined-player reach, and playable-client
  reach. Never collapse them into “the server works.”

## Agent-tool compatibility

`AGENTS.md` is the single source of truth for Codex and other tools that recognize the convention.
`CLAUDE.md` and `GEMINI.md` import it for their native clients. For Aider, start it with
`--read AGENTS.md`. Do not add duplicated Copilot/Cursor/Windsurf/Cline rule copies; update this file
instead so all agents receive the same priorities.
