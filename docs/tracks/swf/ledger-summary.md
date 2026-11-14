# SwiffPlayer S1 ledger

[CHOSEN, not recovered] analysis tooling. S1 classifications remain independent of subsequent recovery. Reviewed completion overrides preserve identifiers and status (1916 done); commit fields remain blank.

Owner dispositions 2026-11-09 (`swf-hashsize`): 22 capacity rows are done for
the retail client's normal 24-bit/nearest mode; 5 GC/collector rows retain
reference-counting ownership because no functionality is missing and 13
numeric/CRT rows retain standard math/native CRT without x87 emulation, all
18 `[CHOSEN, not recovered]`; 104 category-(d) rows are "unattributed, no
recorded reference", with final status `unattributed`, no recovery or
unreachability claim. The capacity table and limits are in `Code/BF2/External/base/hash_capacity.h`.

| Status | Rows |
|---|---:|
| done | 1916 |
| chosen | 78 |
| unattributed | 104 |
| todo | 67 |
| stop | 3 |
| Total | 2168 |

## Coverage and classification

**Round-two 85% acceptance target: NOT MET.** At least 1840/2164 B rows must be both named and classified. Current: 823 (38.0%); 1017 more are needed. Unknown must fall below 15%.

| Class | B rows | Share |
|---|---:|---:|
| unchanged | 54 | 2.5% |
| changed | 65 | 3.0% |
| new | 10 | 0.5% |
| inline | 695 | 32.1% |
| unknown | 1340 | 61.9% |

Named: 1988; classified: 824; both: 823. E bookkeeping: 4 rows, counted separately.

Naming and body classification are independent. Unchanged requires an explicit source/decompile comparison of structure, calls and literals under the release configuration; it is not a matching-build claim. Changed records an observed difference, with a primary reason code. New means no r586 function counterpart for the reviewed role. Inline covers emitted templates, defaults and compiler veneers; inferred template roles leave type arguments open and do not assert unchanged behavior. Unknown is retained when the available evidence is insufficient.

Template symbols identify an operation and an observed stride where available. They are not recovered template arguments or original DICE symbol spellings. Header placement is a definition location, not a recovered originating TU. All original source-path placement remains [INFERRED]; base/ is the approximate May 2004 release.

## Reviewed sample

255 purposive, same-analyst source/decompile comparisons; 12 initial naming/classification errors corrected (4.7%). This is a self-audit, not an independent random sample or a population error bound. Round one found two false anchors (String constructor and ref_counted::drop_ref). Round two corrected four reversed char/wchar_t overload labels and withdrew six unsupported names in the altered character root-slot block. Unresolved comparisons remain unknown. Rule exemplars are part of this review set; automatically propagated instances are not counted as hand reviews.

| Instruction bucket | B rows | Reviews | Unknown |
|---|---:|---:|---:|
| 1-10 | 872 | 94 | 513 |
| 11-30 | 590 | 67 | 395 |
| 31-100 | 550 | 74 | 334 |
| 101-300 | 124 | 15 | 73 |
| 301-1000 | 25 | 3 | 22 |
| >1000 | 3 | 1 | 3 |

Additional reviewed export entries outside the original inventory keys: 0x1006DD90. They are excluded from the bucket counts above.

## Method results

Link-order audit: 105 adjacent out-of-line anchor pairs, 24 inversions. Gapped DP emitted 70 proposals; 0 meet the literal/callee and local-margin threshold. Others remain candidates. Class-body definitions are excluded from this source sequence. A retail gap does not prove DICE-new code; a source gap does not prove inlining.

Reviewed vtable maps cover 5 primary tables and 234 slots; 32 slots remain explicit gaps. The export has 238 candidates, of which 73 contain B entries. Secondary/split tables and DICE-added slots preclude count-only class assignment. vtwrites are constructor/destructor candidates, not proof of which.

Exact-shape template audit: 345 instance/rule pairs, 0 rejected by guards. Structural field/control contracts added 211 inferred roles. Explicit native-value/key/set-member triples identify 68 bindings. Neither shape hashes nor member bindings imply unchanged bodies.

Manual sweep remains incomplete: 432 unknown rows exceed 30 instructions. s1-open.csv orders this remaining work by size. The 85% target must not be met by assigning names from order or size alone.

## Per-TU attribution and retail ranges

Rows include inferred neighbourhood attribution. Ranges below are min/max row-start envelopes, not claims that every intervening COMDAT belongs to the TU. Inline headers and unplaced code are reported separately. s1-order.csv records each tested anchor island.

| File | B rows | Named | Unknown | First | Last | Order inversions |
|---|---:|---:|---:|---|---|---:|
| (unplaced) | 107 | 7 | 100 | 0x1006DB10 | 0x100AA540 | 0 |
| Code/BF2/External/base/container.cpp | 7 | 7 | 6 | 0x1006E3B0 | 0x1006EBC0 | 0 |
| Code/BF2/External/base/container.h | 763 | 719 | 251 | 0x1006E510 | 0x100AA0A0 | 0 |
| Code/BF2/External/base/smart_ptr.h | 94 | 94 | 42 | 0x1006EC50 | 0x100A7BD0 | 0 |
| Code/BF2/External/base/tu_config.h | 5 | 5 | 1 | 0x1006DB40 | 0x1006DBB0 | 0 |
| Code/BF2/External/base/tu_file.cpp | 31 | 31 | 0 | 0x1006DCB0 | 0x1006E380 | 1 |
| Code/BF2/External/base/tu_file.h | 21 | 21 | 20 | 0x1006DC10 | 0x100A4F40 | 0 |
| Code/BF2/External/base/utf8.cpp | 2 | 2 | 1 | 0x1006E6A0 | 0x1006E980 | 0 |
| Code/BF2/External/base/utility.h | 8 | 8 | 2 | 0x1006DC90 | 0x100A4F20 | 0 |
| Code/BF2/External/gameswf/gameswf.h | 28 | 28 | 27 | 0x1006EB70 | 0x100A4850 | 0 |
| Code/BF2/External/gameswf/gameswf_action.cpp | 182 | 174 | 145 | 0x1006FB10 | 0x100A9BE0 | 8 |
| Code/BF2/External/gameswf/gameswf_action.h | 34 | 34 | 33 | 0x1006ECB0 | 0x100A8CC0 | 0 |
| Code/BF2/External/gameswf/gameswf_button.cpp | 26 | 26 | 5 | 0x100A1BE0 | 0x100A3CB0 | 3 |
| Code/BF2/External/gameswf/gameswf_button.h | 10 | 10 | 6 | 0x100A1C70 | 0x100A2840 | 0 |
| Code/BF2/External/gameswf/gameswf_dlist.cpp | 14 | 14 | 14 | 0x100A7800 | 0x100A82A0 | 0 |
| Code/BF2/External/gameswf/gameswf_dlist.h | 6 | 6 | 6 | 0x10071500 | 0x100A7EB0 | 0 |
| Code/BF2/External/gameswf/gameswf_font.cpp | 15 | 15 | 13 | 0x100A01A0 | 0x100A1BB0 | 2 |
| Code/BF2/External/gameswf/gameswf_font.h | 14 | 14 | 14 | 0x10084D60 | 0x100A5BB0 | 0 |
| Code/BF2/External/gameswf/gameswf_fontlib.cpp | 35 | 35 | 35 | 0x100A47F0 | 0x100A7480 | 1 |
| Code/BF2/External/gameswf/gameswf_impl.cpp | 311 | 291 | 205 | 0x1006F0E0 | 0x10084B20 | 7 |
| Code/BF2/External/gameswf/gameswf_impl.h | 119 | 119 | 102 | 0x1006ED60 | 0x100A7740 | 0 |
| Code/BF2/External/gameswf/gameswf_log.cpp | 3 | 3 | 1 | 0x1009AF40 | 0x1009AF90 | 0 |
| Code/BF2/External/gameswf/gameswf_morph2.cpp | 5 | 5 | 4 | 0x1009F5D0 | 0x1009FDB0 | 0 |
| Code/BF2/External/gameswf/gameswf_profile.h | 2 | 2 | 2 | 0x10084CF0 | 0x10084D10 | 0 |
| Code/BF2/External/gameswf/gameswf_render.cpp | 39 | 39 | 38 | 0x1009A3F0 | 0x1009AE90 | 0 |
| Code/BF2/External/gameswf/gameswf_shape.cpp | 62 | 60 | 61 | 0x100963C0 | 0x10099670 | 0 |
| Code/BF2/External/gameswf/gameswf_shape.h | 12 | 12 | 11 | 0x10075BD0 | 0x1009F5A0 | 0 |
| Code/BF2/External/gameswf/gameswf_sound.cpp | 11 | 11 | 10 | 0x1009E8F0 | 0x1009F530 | 0 |
| Code/BF2/External/gameswf/gameswf_stream.cpp | 21 | 19 | 20 | 0x1009D210 | 0x1009D6A0 | 1 |
| Code/BF2/External/gameswf/gameswf_stream.h | 1 | 1 | 1 | 0x1009F5B0 | 0x1009F5B0 | 0 |
| Code/BF2/External/gameswf/gameswf_string.cpp | 20 | 20 | 18 | 0x1008D670 | 0x1009C140 | 0 |
| Code/BF2/External/gameswf/gameswf_styles.cpp | 14 | 14 | 14 | 0x100A3D10 | 0x100A4750 | 0 |
| Code/BF2/External/gameswf/gameswf_styles.h | 9 | 9 | 9 | 0x10086550 | 0x100A3DA0 | 0 |
| Code/BF2/External/gameswf/gameswf_tesselate.h | 1 | 1 | 1 | 0x10096410 | 0x10096410 | 0 |
| Code/BF2/External/gameswf/gameswf_text.cpp | 63 | 63 | 58 | 0x10072300 | 0x10089D50 | 1 |
| Code/BF2/External/gameswf/gameswf_timers.cpp | 7 | 7 | 6 | 0x10075E30 | 0x1007E210 | 0 |
| Code/BF2/External/gameswf/gameswf_types.cpp | 39 | 39 | 36 | 0x1009D800 | 0x1009E7B0 | 0 |
| Code/BF2/External/gameswf/gameswf_types.h | 2 | 2 | 2 | 0x1006EBD0 | 0x10084D40 | 0 |
| Code/BF2/External/gameswf/gameswf_xml.cpp | 21 | 21 | 20 | 0x1008B200 | 0x100A93E0 | 0 |

## DICE-new clusters

Only reviewed new rows count below. Intervening helpers and unchanged/changed functions do not inherit a cluster classification. Endpoints are first/last row starts.

| First | Last | File | New rows |
|---|---|---|---:|
| 0x1006F820 | 0x1006F820 | Code/BF2/External/gameswf/gameswf_impl.cpp | 1 |
| 0x10077910 | 0x10077910 | Code/BF2/External/gameswf/gameswf_timers.cpp | 1 |
| 0x10080010 | 0x10080010 | Code/BF2/External/gameswf/gameswf_impl.cpp | 1 |
| 0x1009BB10 | 0x1009BB10 | Code/BF2/External/gameswf/gameswf_string.cpp | 1 |
| 0x100A2010 | 0x100A3020 | Code/BF2/External/gameswf/gameswf_button.cpp | 4 |
| 0x100A8530 | 0x100A8530 | Code/BF2/External/gameswf/gameswf_xml.cpp | 1 |
| 0x100A95A0 | 0x100A95A0 | Code/BF2/External/gameswf/gameswf_action.cpp | 1 |

## Changed-reason counts

One primary observed reason per changed row; additional differences remain in reason.

| Reason | B rows |
|---|---:|
| extra-call | 23 |
| missing-call | 6 |
| literal | 4 |
| branch | 6 |
| signature | 8 |
| dice-member | 18 |

## Revised effort (PROBE section 5)

Applying the probe's rates to the measured split: (54 unchanged + 695 inline)/400 + (65 changed + 10 new)/150 = 2.37 agent-days. Charging 1340 unknown at 400/day versus 120/day gives **5.7-13.5 agent-days of function work**. Add the probe's 1-2 tooling days and 2-3 menu-validation days: **8.7-18.5 agent-days total**, excluding incomplete E coverage and separate vendoring. Inline functions are not free.

Per-function assumptions, using an eight-hour agent-day: 1.2 minutes for unchanged/inline, 3.2 minutes for changed/new, 1.2-4.0 minutes for unknown. Weighted function-work average: 1.3-3.0 minutes. These are planning rates, not measured recovery timings. Large VM/text/shape bodies can cost much more. S2 must calibrate them; the current split cannot justify a 65/35 extrapolation.

## Boundaries, provenance and S2

All 2164 B inventory keys plus 4 E rows are retained. size is the export body size where present; csv_size is the padded inventory extent; ninstr keeps the PROBE count. 0x10095609 is interpreter jump-table data, retained as an unknown inventory artifact. s1-boundaries.csv retains the 48 jump-thunk decisions and 28 extra Ghidra starts; those starts do not become additional B keys.

Boundary audit rows: 104; tree citations: 83. Citation entry checks are separate from function identity. The export contains 2142 B starts and only 79 E starts. s1-inputs.json fingerprints current input availability, decompile text, rules and tooling. S1 name alignment alone claims no recovered implementation. S2 uses a verified read-only Ghidra project copy; see s2-progress.md for its evidence, completed subset and validation limits.

s2-scope.csv includes the button TU (0x100A1BE0-0x100A3B70), root/sprite/button virtual targets, mouse/event/action helpers, their direct B callees and the interpreter. Shared inherited targets occur once, with all scope reasons. Unknown slot/body identities remain visible. Indirect dispatch and callback effects still require instruction review; the scope is an explicit pilot list, not a proven whole-program call-graph closure.

The retail sprite mouse pass snapshots child count, then reloads the display-list storage while iterating. S2 must examine callback-driven mutation and the quit dialog without silently introducing an empty-list guard. No runtime path was exercised.

## Reproduction

```sh
python3 -I tools/tracks/swf_align.py
python3 -I tools/tracks/swf_ledger.py
python3 -I tools/tracks/test_swf_ledger.py
python3 -I tools/tracks/swf_ledger.py --require-coverage
```

The optional coverage gate fails while fewer than 85% are both named and classified. Unit tests validate tooling contracts, not recovery fidelity or gameplay. No C++ builds, game runs, installs, network, commits or pushes. All production frontiers remain.
