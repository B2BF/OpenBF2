# Development guide

## Before changing code

Read [AGENTS.md](../AGENTS.md), inspect `git status --short`, and identify the image and
subsystem you are changing. The worktree may contain work from other contributors; preserve
it and agree on file ownership before editing overlapping code.

Search the affected source for evidence markers and inspect the current implementation.
Check the boundary's callers and the conditions attached to temporary code. Current source
and freshly inspected binary instructions take precedence over old comments. If evidence
required for recovery or a compatibility contract is missing, stop that part of the work
and state what is needed.

## Agent assistance and contribution ownership

An agent implementing recovered engine behavior must have IDA or Ghidra connected and
accessible, with the relevant original Battlefield 2 1.50 binaries loaded. Confirm the
image and architecture and inspect instructions, symbols and cross-references before
writing the implementation. If the tool or required binary evidence is unavailable,
stop recovery work; do not fill the gap with guesses. Independent documentation, build
maintenance and intentional non-retail work can proceed when they do not depend on that
evidence. For intentional changes, use the approach already discussed in the linked issue
and inspect the original where needed to establish affected behavior or compatibility.

Agents must not create or submit pull requests, and must not reply to reviewer questions.
They can prepare local changes and validation results for a human to review. The human
author must fully understand every code change they submit, including its behavior,
applicable binary evidence, the issue-discussed design, compatibility constraints and tests.
The human submits the pull request, answers reviewers and owns the result.

## Choose the basis for the change

Reconstruction defaults to the shipped behavior. A non-retail fix, improvement or feature
is allowed only when a linked issue already contains discussion of the proposed approach
before implementation begins. An issue title or a link added after the work is complete
does not satisfy this requirement. AI may assist with implementation and validation of
the discussed approach under the human ownership rules above.

Link the issue in the change description or documentation. Add the link beside the code
when a future reader needs it to understand a deliberate departure from retail behavior.
Use `[CHOSEN, not recovered]` for intentional policy and describe the new behavior plainly;
never present an improvement as a recovered implementation. Discuss compatibility impact
in the issue when changing an ABI, protocol or file-format contract, and verify original
behavior wherever the design depends on it. An issue discussion does not establish an
unknown signature or layout.

Editor-only code may use a modern design without retail-editor parity. Its features and
fixes follow the same issue-discussion workflow. Shared game code defaults to the recovery
rules, and editor output must remain loadable by the retail game.

## Recover behavior accurately

Read the original instructions before implementing a recovered function. Cross-check the
symbolized Linux ELF and Windows PE where available. Mangled names establish parameter
types and constness; callers and return-value use help establish return types. Keep the
observed ordering, ownership, side effects, signedness, repeated virtual calls, and missing
guards in a recovery change. Intentional fixes to those behaviors follow the linked issue's
discussed approach. A plausible rewrite is not evidence of the shipped implementation.

Public signatures, inheritance, virtual order, class layout, serialized widths, event IDs,
and protocol transitions are compatibility contracts. Verify retail layouts with MSVC x86;
64-bit native builds need separate layout checks. STL layout and destructor slots differ
across compilers, so use typed C++ interfaces and source-level container operations rather
than copying decompiler internals or raw vtable offsets.

Place a recovered file at its attested path when one exists. An original `__FILE__` string
or translation-unit symbol takes precedence over a placement inferred from a namespace.
Record unverified placement as `[INFERRED]`. Use upstream sources for third-party libraries
under `External/`.

## Comments and evidence

Use short API comments to explain what a function or class does. Describe meaningful
arguments and return values, including null handling and failure results where applicable.
Add a brief remark for ownership, required call order, an ABI constraint or a remaining
boundary when the caller needs that information. Avoid restating the signature or narrating
the implementation.

For example:

```cpp
/// Tests whether this area applies to the player's team and current vehicle.
/// @param player Player to test; may be null.
/// @return False for a null player, no controllable vehicle, or a filter mismatch.
bool checkIfActiveToPlayer(world::IPlayer* player);
```

Do not use decorative separators, emoji, all-caps emphasis, completion dates, byte counts,
address headings or investigation stories. Keep necessary layout offsets next to the fields
or ABI checks they describe, and preserve license and attribution notices. Guides should
state their contracts and limitations directly.

Keep these markers when they affect how the code should be interpreted:

| Marker | Meaning |
| --- | --- |
| `[INFERRED]` | A name, path, or interpretation fits the evidence but is not directly attested. |
| `[UNRESOLVED]` | An observed call or value is still unidentified; retain the raw evidence. |
| `[PLATFORM]` | The original Windows and Linux implementations demonstrably differ. |
| `[CHOSEN, not recovered]` | An intentional project decision or scaffold. |
| `[FRONTIER]` | A known boundary whose real implementation is missing. |
| `[TEMP]` | A test or harness fixture. |
| `[RETIRE-WHEN: tag]` | Temporary debt with its implementation and validation conditions stated beside the marker. |

A known-signature frontier normally logs its qualified method name to stderr, flushes,
and aborts. Keep an unknown-signature boundary declaration-only or stop at a proven caller.
Never invent a successful return, empty production implementation, or fake interface to
make progress appear complete. The narrowly scoped exceptions are documented in
[AGENTS.md](../AGENTS.md); do not generalize them to other seams.

## Validation

Choose checks that exercise the changed contract. The usual Windows gates, run from the
repository root, are:

```powershell
# One translation unit, then the full syntax/semantic gate when needed.
cmd /c Code\Tests\syntaxcheck.cmd Code\BF2\path\File.cpp
cmd /c Code\Tests\syntaxcheck.cmd

# Focused object/physics runtime checks and boot-harness compile check.
cmd /c Code\Tests\runtime_pair.cmd
cmd /c Code\Build\_bmcheck.cmd

# Actual client-event dispatch and network-status calls with typed observers, on x86 and x64.
cmd /c Code\Tests\client_event_dispatch_runtime.cmd out\client_dispatch_local

# Full compile and diagnostic links in an isolated output directory.
$env:BF2_BUILD_OUT = "$PWD\Code\Build\obj_local_review"
try {
    cmd /c Code\Build\build.cmd
} finally {
    Remove-Item Env:BF2_BUILD_OUT
}
```

Choose a unique `BF2_BUILD_OUT`: the full build cleans its output directory. The batch
helpers resolve `VsDevCmd.bat` through `BF2_VSDEVCMD` or `vswhere`; see the
[Windows prerequisites](building.md#windows-prerequisites).
The client-dispatch fixture also takes an isolated output directory. It checks secondary
interface pointers, forwarded arguments, and null-client/connection handling without sockets
or game content; it does not exercise a production connection.
Every compile of engine code must set its image define and force-include `BuildConfig.h`.
New translation units must enter both CMake's image manifests and the applicable full
syntax gate; do not replace an image manifest with an unrestricted object glob.

For a shared header, interface, virtual signature, or layout change, run the full MSVC x86
syntax gate and an isolated all-translation-unit build. Shared headers must also pass the
Windows x64 strict link and Linux build. Test the client image when its compilation or
behavior is affected. Editor framework changes have `bf2_editor_run_checks`; engine-backed
editor changes need the relevant engine and file-format checks too.

A static archive proves compilation. A strict link proves symbol resolution. `/FORCE`
diagnostic images can contain unresolved calls that fail only when executed. A bounded
harness proves only its selected paths; direct `loadPath` calls, injected maps, synthetic
players, and Python fixtures are not production startup acceptance.

Server runtime claims use the real `BF2::init -> BF2Engine::startGame -> GameServer` path
with representative content and configuration. Test the affected loading, ticks, client sessions,
unload/reload, shutdown, receive-thread join and immediate rebind behavior. Stability results
must state the run duration. Report the executable, arguments, environment flags, ports,
branches exercised and remaining frontiers. Never use port 16567 for an agent-run or automated
test or stop another session to obtain a port. Keep rebuilt binaries separate from the original
executables, DLLs and reference installation.

For networking changes, exercise the affected packet paths and rejection of malformed or
unknown input. Registration alone does not establish working dispatch or playable behavior.
Use original server/client packet evidence and binary instructions for recovered protocol
transitions; identify intentional changes through their linked issue. Keep packet captures
and golden fixtures free of credentials and personal data.

## Finishing a change

Before opening or updating a PR, use the [local CI command](local-ci.md) to verify
the committed revision. It runs the Linux workflow locally and reports other
platforms and production signing as unverified. An optional fast pre-push guard
checks saved results without starting builds.

Update the affected source comments and engineer guide when behavior or evidence changes.
State each temporary boundary's removal conditions beside its marker, and remove it only
after the required implementation and tests are complete. Remove harness overrides that
would hide a new production implementation. Keep generated build logs and extraction scratch
files in your own build directory.

A useful review records the behavior changed, its PE/ELF evidence for recovery or the linked
issue and intentional design change, tests and outcomes, unexercised paths, and remaining
named frontiers relevant to the change. Distinguish startup, listening, loading, joining,
and playable behavior. Do not stage or commit shared work unless asked.
An agent reports its local changes and verification results; the human author handles
pull-request submission and review discussion.
