# OpenBF2 review agent

Use this guide for code review and regression investigation. [AGENTS.md](../AGENTS.md)
remains the canonical policy; this guide adds a review process, not a separate set of
reconstruction rules. Follow the user's requested scope and any explicit authorization
to implement fixes. A request to review alone means inspect, validate, and report.

Your priorities are correctness, evidence for reconstructed behavior, compatibility,
and alignment with parallel work in [B2BF/OpenBF2](https://github.com/B2BF/OpenBF2).
Evaluate concrete failures and integration risks. Legacy style, raw pointers, lack of
Asio, or lack of modern C++ abstractions do not establish a defect. Preserve readability
without proposing cosmetic rewrites, dependency migrations, or a newer language standard
as part of an ordinary review.

## Pull request scope and hosted reviews

For a pull request, review the diff against its target branch and follow affected callers,
implementations, and consumers as needed. Compare suspected regressions with the PR baseline;
keep pre-existing defects separate. The whole-fork inventory below applies to a requested
repository audit, not to every pull request.

Hosted reviewers must not assume access to the author's local IDA/Ghidra session, original
game binaries or content, sibling repositories, or current upstream branches. Use the
available repository evidence and connected read tools. State unavailable evidence and the
checks it prevents, then continue independent source review. Do not claim binary verification,
upstream coordination, or runtime validation that was not performed.

Keep durable review rules in the root `AGENTS.md` and any applicable nested `AGENTS.md` files;
this guide is reached through the root file's explicit link. Automatic review enablement and
triggers are configured separately in [Codex review settings](https://chatgpt.com/settings/code-review).
See the [GitHub review documentation](https://learn.chatgpt.com/docs/third-party/github).

## Establish the scope and baseline

1. Read `AGENTS.md` completely, then [development](development.md),
   [building](building.md), and the relevant [architecture](architecture.md) sections.
   Inspect `git status --short`, remotes, the selected branch, and its exact commit.
   Preserve existing tracked and untracked work; include it in the review only when
   requested, and distinguish it from committed changes.
2. Identify the target images and build configurations: dedicated server, client,
   individual plugins, editor, and native ports have different contracts and manifests.
   Follow affected callers, implementations, and consumers across those boundaries.
3. Record the fork SHA, upstream base SHA, merge base, and relevant PR/branch head SHAs.
   Refresh remote information without merging, rebasing, switching the shared checkout,
   or moving the user's branches. Inspect remote URLs before assuming an `upstream`
   remote exists or points to B2BF. If current upstream information is unavailable,
   state that limitation and continue the local review.
4. Use `gh` or another available read interface to inspect upstream PR changes, issue
   discussions, review comments, and CI. Inspect the fork's relevant work too. Resolve
   current heads at review time; a remembered PR number or stale conversation is not a
   reliable baseline. Reading these discussions does not authorize posting to them.
5. If no narrower scope was supplied, first inventory the fork's changes from its common
   ancestor with upstream. Prioritize changed production paths with memory, ABI,
   networking, lifecycle, or upstream-overlap risk. State which areas this pass covers;
   do not describe a sampled pass as a complete audit of the repository.

For each affected subsystem, keep a concise overlap record: files or interfaces,
relevant upstream PR/branch and SHA, the intended behavior on each side, and whether a
finding is already addressed or would conflict with that work. Review semantic conflicts
as well as textual ones: two patches may merge cleanly while disagreeing about ownership,
layout, event ordering, or protocol transitions. An upstream implementation is a candidate
to evaluate against the evidence and target contract, not automatic proof of correctness.

## Review the behavior and its evidence

- Trace ownership, allocation/release pairs, borrowed pointers, callbacks, container
  invalidation, and object lifetime across DLLs and asynchronous or threaded work.
- Check initialization and shutdown ordering, reentrancy, locking, thread completion,
  map unload/reload, disconnect/reconnect, and immediate listener reuse. Establish a
  reachable sequence before asserting a race or use-after-free.
- Check calling conventions, interface pointer adjustments, virtual dispatch, factory
  thunks, integer and pointer widths, packing, and the CRT/STL boundary for each image.
  Retail x86 layout evidence does not validate a native x64 or ARM64 layout.
- Check packet and file lengths, arithmetic, signedness, endianness, identifiers,
  serialization, partial reads/writes where applicable, and state transitions. Verify
  malformed-input behavior against the actual transport and caller contract.
- Check build manifests, image defines, registration, exports, and runtime staging.
  A missing body hidden by a harness override or diagnostic link is not a working
  production implementation.
- Treat console scripts, embedded Python, map content, renderer state, floating-point
  behavior, and editor output as compatibility boundaries when the reviewed path uses
  them. Require a concrete affected path rather than speculation about every subsystem.

Classify each observation as an introduced regression, pre-existing defect, evidenced
retail behavior, intentional project policy, unresolved reconstruction, or an unconfirmed
hypothesis. These categories are distinct from severity. An inherited defect can matter,
but silently changing it is not a reconstruction fix.

Recovery claims require the binary evidence specified in `AGENTS.md`. Record the relevant
original image/version/architecture and inspected function or call sites. Without the
required image or IDA/Ghidra connection, report the missing evidence and limit the claim;
continue independent source review, build analysis, and reproducible bug investigation.
Do not infer an unknown signature, layout, or body from a plausible implementation.

Read issue-backed intentional changes before evaluating fidelity. In particular, the
native host/plugin contracts documented in `architecture.md` deliberately differ from
retail DLL compatibility. Do not propose restoring retired VC7.1 adapters, old layouts,
or retail plugin mixing just because the original executable used them. Editor-only
code follows its documented design while preserving retail-compatible output formats.

Inspect `[FRONTIER]`, `[UNRESOLVED]`, `[TEMP]`, `[CHOSEN, not recovered]`, and
`[RETIRE-WHEN: ...]` boundaries and their callers. A documented frontier is not itself a
new regression. Never replace a diagnostic stop with fabricated success to improve a
test result. Check current source and fresh evidence when documentation disagrees;
report stale guidance rather than recreating retired scaffolding or inventing a policy.

## GameSpy and emulator integration

Inspect upstream endpoint selection, QR2 responses, authentication, server browsing, and
statistics work when those paths overlap the review. Do not replace OpenSpy or another
configured service globally merely to make a local test pass.

The [GameSpy emulator](https://github.com/kr4uzi/gamespy-emulator) can supply an additional
integration endpoint. Agreement between two reconstructed implementations is not proof
of retail compatibility. Compare relevant behavior with the matching GameSpy SDK,
original client/server evidence, and sanitized packet fixtures. Record the emulator
commit, endpoint configuration, and exercised scenarios. Attribute a failure to the
engine, emulator, configuration, or unresolved contract before proposing a correction.
Preserve the engine's networking architecture unless an explicitly authorized design
change requires otherwise. Use controlled test endpoints and keep captures free of
credentials and personal data.

## Validate without disrupting parallel work

Choose existing checks from `building.md` and `development.md` for the affected images.
Where practical, reproduce a suspected regression on the reviewed commit and compare
with its parent or a relevant upstream implementation using the same configuration.
Prefer focused tests of an observed contract or independent golden data; a test that
merely repeats the current implementation is weak evidence.

Use agent-owned outputs, content copies, profiles, and logs. A unique CMake `-B` directory
does not isolate every Windows client plugin output: follow `building.md` and serialize
builds that share staging paths. Preserve original binaries and reference installations.
Never use port 16567 or stop the owner's server. Configure an unused port explicitly and
keep settings, harnesses, and test clients consistent.

Use correctness-focused static diagnostics and sanitizers where the supported target can
run them. Supply the real image defines, compiler flags, and headers; do not alter the
toolchain or production source just to make a checker accept it. Disable automatic
fixes. Triage warnings against reachability, evidence, and intentional behavior.

Distinguish compile success, strict linking, diagnostic `/FORCE` linking, fixture success,
production startup, socket binding, map loading, joining, gameplay, and teardown. Exercise
only the production scenarios relevant to the claim, and report what remains untested.
Cross-compilation does not establish native execution. Missing tools, game content, or
binary-analysis access are limitations to record, not permission to invent results.

Parallel reviewers can divide upstream coordination, ABI/lifetimes, and protocol/lifecycle
analysis. Assign explicit boundaries and reconcile cross-subsystem findings centrally.
Keep inspection parallel where useful; serialize overlapping edits and shared builds.

## Deliver a review the author can act on

Lead with confirmed findings, ordered by severity. For each finding include:

- A concise title and classification, affected file/line, reviewed SHA, and target image.
- The concrete trigger, reachable call or state sequence, and user-visible consequence.
- Evidence: source reasoning, a reproducer or test result, original binary/packet evidence,
  or the applicable issue-backed contract. State confidence and unresolved assumptions.
- Relevant upstream work and whether the same problem or correction already exists there.
- The smallest justified correction and its compatibility implications, plus the checks
  needed to demonstrate it. Keep a proposed fix separate from a verified fix.

List unresolved questions and missing evidence separately from confirmed defects. End
with the reviewed scope, exact baselines, validation performed, unexercised paths, and
remaining relevant frontiers. If no actionable defect is found, say so without claiming
the code is bug-free. Do not pad the report with style preferences or a target number of
findings.

When the user authorizes fixes, retain this review discipline and the canonical rules for
intentional changes. Recheck file ownership and upstream overlap, make small scoped
patches, validate the affected contracts, and keep unrelated cleanup out of the change.
Follow `AGENTS.md` on commits, human-owned PR submission, and reviewer responses. Do not
request authorization again for work the user has already explicitly authorized.
