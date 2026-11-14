#!/usr/bin/env python3
"""Synthetic text/metadata fixtures for S1 tooling; no retail code or strings."""

import importlib.util
from pathlib import Path
import random
import sys
import tempfile
import unittest


def load(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + ".py"))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


sa = load("swf_align")
sl = load("swf_ledger")


class ParserTests(unittest.TestCase):
    def test_literals_comments_escapes_and_concatenation(self):
        text = r'''// "ignored"
          emit("hello" /* separator */ "there\n", "brace }", "a\\b");
          /* "ignored twice" */ char c = '\"';'''
        self.assertEqual(sa.literals(text), {"hellothere\n", "brace }", "a\\b"})

    def test_qualified_overloads_and_nested_braces(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "sample.cpp"
            p.write_text('namespace demo { struct Item { virtual void step() { if (1) { ping(); } } }; '
                         'int calc(int n) { emit("fixture"); return n; } '
                         'int calc(float n) { return 2; } }')
            funcs, layouts = sa.parse_sources(d, "fixture/")
            names = [s.name for s in funcs.values()]
            self.assertIn("demo::Item::step", names)
            self.assertEqual(names.count("demo::calc"), 2)
            self.assertEqual(layouts["Item"]["virtuals"], ["step"])
            self.assertEqual(next(s for s in funcs.values() if s.name.endswith("step")).calls, ["ping"])

    def test_macro_is_text_not_execution(self):
        source = ('#define MAKE(name) void wrap_##name() { name(); }\n'
                  'MAKE(alpha);\nMAKE(beta);\n')
        funcs = sa.macro_functions(source, "fixture/sample.cpp")
        self.assertEqual({s.name for s in funcs.values()}, {"wrap_alpha", "wrap_beta"})
        self.assertEqual(sorted(s.calls for s in funcs.values()), [["alpha"], ["beta"]])

    def test_virtual_order_and_members(self):
        layouts = sa.class_layouts('struct Parent { virtual void a() = 0; int m_data; '
                                  'virtual void b() { if (1) { call(); } } };', "fixture.h")
        self.assertEqual(layouts["Parent"]["virtuals"], ["a", "b"])
        self.assertEqual(layouts["Parent"]["members"], ["m_data"])

    def test_tags_follow_arguments_not_assignment_order(self):
        text = ('local_a = FUN_12345678; local_b = (code *)0x2; '
                'FUN_10071e30(&table,(uint *)&local_b,(uint *)&local_a); '
                'local_b = FUN_23456789; local_a = (code *)0x4; '
                'FUN_10071e30(&table,(uint *)&local_a,(uint *)&local_b);')
        self.assertEqual(sa.retail_tags(text), {2: 0x12345678, 4: 0x23456789})


class AlignmentTests(unittest.TestCase):
    def source(self, key, calls=(), strings=()):
        return sa.Source(key, key, "fixture.cpp", 1, set(strings), list(calls), 10, "{}")

    def test_ambiguous_strings_do_not_anchor(self):
        sources = {k: self.source(k, strings=["shared literal"]) for k in ("first", "second")}
        funcs = {10: {"strings": [20]}}
        matches, _, _ = sa.align(sources, funcs, {20: "shared literal"}, [], {})
        self.assertFalse(matches)

    def test_competing_unique_literals_reject_tie(self):
        sources = {"first": self.source("first", strings=["first literal"]),
                   "second": self.source("second", strings=["second literal"])}
        funcs = {10: {"strings": [20, 30]}}
        matches, _, conflicts = sa.align(sources, funcs, {20: "first literal", 30: "second literal"}, [], {})
        self.assertFalse(matches)
        self.assertEqual(len(conflicts), 2)

    def test_graph_requires_two_anchors(self):
        sources = {k: self.source(k, calls=["child"] if k != "child" else [])
                   for k in ("a", "b", "child")}
        funcs = {10: {"callees": [30], "callers": []},
                 20: {"callees": [30], "callers": []},
                 30: {"callees": [], "callers": [10, 20]}}
        by_name = {k: [k] for k in sources}
        matches = {10: ("a", "fixture", "high")}
        sa.propagate(sources, funcs, matches, by_name)
        self.assertNotIn(30, matches)
        matches[20] = ("b", "fixture", "high")
        sa.propagate(sources, funcs, matches, by_name)
        self.assertEqual(matches[30][0], "child")

    def test_boundary_retains_missing_keys_and_extras(self):
        inv = [{"retail_addr": "0x1006DB10"}, {"retail_addr": "0x1006DB20"}]
        funcs = {0x1006DB10: {}, 0x1006DB30: {}}
        notes, extra = sa.boundary_notes(inv, funcs, {0x1006DB20: (5, "jmp", "0x10070000")})
        self.assertIn("jump-thunk", notes[0x1006DB20])
        self.assertIn("ghidra-extra", notes[0x1006DB20])
        self.assertEqual(extra, [0x1006DB30])

    def test_alignment_never_implies_unchanged(self):
        inv = [{"retail_addr": "0x1006DB10", "bytes": "16", "instructions": "4"}]
        funcs = {0x1006DB10: {"size": 4}}
        sources = {"f": self.source("f")}
        row = sa.make_rows(inv, funcs, {}, {0x1006DB10: ("f", "fixture", "high")}, sources, {}, {}, [])[0]
        self.assertEqual(row["match"], "unknown")

    def test_review_rejects_literal_donor_before_propagation(self):
        matches = {10: ("dispatcher", "string:fixture", "high")}
        sa.apply_review_names(matches, [{"address": "0xA", "match": "new", "symbol": "FUN_A",
                                        "confidence": "high"}], {10: {}}, {})
        self.assertNotIn(10, matches)

    def test_reviewed_unknown_rejects_name_and_graph_seed(self):
        funcs = {10: {}}
        matches = {10: ('bad_name', 'fixture', 'high')}
        sa.apply_review_names(matches, [dict(address='0xA', match='unknown', symbol='FUN_0000000A',
                                              confidence='high')], funcs, {})
        self.assertFalse(matches)
        self.assertTrue(funcs[10]['alignment_rejected'])

    def test_accessor_cannot_be_a_conditional_destructor(self):
        source = self.source("accessor")
        source.body = "{ return m_items[index]; }"
        self.assertFalse(sa.graph_shape_agrees(source, {"branch_ops": 1}))
        self.assertTrue(sa.graph_shape_agrees(source, {"branch_ops": 0}))

    def test_vtable_inserted_slot_breaks_bracket(self):
        sources = {k: self.source("demo::Node::" + k) for k in ("a", "middle", "z")}
        layouts = {"Node": {"bases": [], "virtuals": ["a", "middle", "z"]}}
        funcs = {n: {} for n in (10, 20, 30, 40)}
        matches = {10: ("a", "fixture", "high"), 40: ("z", "fixture", "high")}
        sa.vtable_anchors(layouts, sources, funcs, [{"addr": 100, "slots": [10, 20, 30, 40]}], matches)
        self.assertEqual(len(matches), 2)
        sa.vtable_anchors(layouts, sources, funcs, [{"addr": 100, "slots": [10, 20, 40]}], matches)
        self.assertEqual(matches[20][0], "middle")


class LedgerTests(unittest.TestCase):
    def test_determinism_and_contract(self):
        rows = [{"address": f"0x{x:08X}", "size": 3, "symbol": f"FUN_{x:08X}", "file": "",
                 "match": "unknown"} for x in (0x10070020, 0x10070010, 0x10070030)]
        expected = sl.ledger_rows(rows)
        random.Random(42).shuffle(rows)
        self.assertEqual(sl.ledger_rows(rows), expected)
        self.assertEqual(sl.FIELDS[:7], ["image", "address", "size", "symbol", "file", "status", "commit"])
        self.assertTrue(all(r["status"] == "todo" and r["commit"] == "" for r in expected))
        with tempfile.TemporaryDirectory() as d:
            a, b = Path(d) / "a.csv", Path(d) / "b.csv"
            sa.write_csv(a, sl.FIELDS, expected)
            sa.write_csv(b, sl.FIELDS, sl.ledger_rows(rows))
            self.assertEqual(a.read_bytes(), b.read_bytes())

    def test_duplicate_rejected(self):
        with self.assertRaises(ValueError):
            sl.ledger_rows([{"address": "0x10070010", "match": "unknown"}] * 2)

    def test_completion_override_preserves_classification_and_commit(self):
        rows = sl.ledger_rows([dict(address="0x10070010", match="unknown",
                                    symbol="FUN_10070010", file="", reason="S1")])
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "overrides.csv"
            self.assertEqual(sl.apply_completion_overrides(rows, path), rows)
            sa.write_csv(path, ["address", "symbol", "file", "status", "reason"], [
                dict(address="0x10070010", symbol="demo::step", file="fixture.cpp",
                     status="done", reason="S2")])
            result = sl.apply_completion_overrides(rows, path)[0]
        self.assertEqual((result["symbol"], result["file"], result["status"]),
                         ("demo::step", "fixture.cpp", "done"))
        self.assertEqual((result["match"], result["commit"], result["reason"]),
                         ("unknown", "", "S1;S2"))

    def test_invalid_completion_overrides_rejected(self):
        row = dict(address="0x10070010", symbol="demo::step", file="fixture.cpp", status="done")
        variants = [[row, row], [{**row, "address": "0x10070020"}],
                    [{**row, "status": "todo"}], [{**row, "symbol": ""}], [{**row, "file": ""}]]
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "overrides.csv"
            for overrides in variants:
                with self.subTest(overrides=overrides):
                    sa.write_csv(path, list(row), overrides)
                    rows = sl.ledger_rows([dict(address="0x10070010", match="unknown", reason="S1")])
                    with self.assertRaises(ValueError):
                        sl.apply_completion_overrides(rows, path)

    def test_non_recovery_dispositions_preserve_unattributed_rows(self):
        original = dict(address="0x10070010", match="unknown", symbol="FUN_10070010",
                        file="", reason="S1")
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "overrides.csv"
            for status in ("chosen", "unattributed"):
                override = {**original, "status": status, "reason": "owner disposition"}
                sa.write_csv(path, list(override), [override])
                row = sl.apply_completion_overrides(sl.ledger_rows([original]), path)[0]
                self.assertEqual((row["status"], row["symbol"], row["file"], row["match"], row["commit"]),
                                 (status, "FUN_10070010", "", "unknown", ""))
            for changes in ({"symbol": "invented::owner"}, {"file": "invented.cpp"}, {"reason": ""}):
                override = {**original, "status": "unattributed", "reason": "owner disposition", **changes}
                sa.write_csv(path, list(override), [override])
                with self.assertRaises(ValueError):
                    sl.apply_completion_overrides(sl.ledger_rows([original]), path)

    def test_buckets_match_probe_edges(self):
        self.assertEqual([sl.bucket(x) for x in (10, 11, 30, 31, 100, 101, 300, 301, 1000, 1001)],
                         ["1-10", "11-30", "11-30", "31-100", "31-100", "101-300", "101-300",
                          "301-1000", "301-1000", ">1000"])

    def test_tu_ranges_do_not_bridge_gaps(self):
        rows = [{"address": str(i), "file": f} for i, f in enumerate(["a.cpp", "", "a.cpp"])]
        self.assertEqual(len(sl.tu_runs(rows)), 3)


class RoundTwoTests(unittest.TestCase):
    def test_class_body_is_not_out_of_line_sequence(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / 'fixture.cpp').write_text(
                'namespace demo { struct Node { void local() {} }; void Node::remote() {} void free_fn() {} }')
            sources, _ = sa.parse_sources(d, 'fixture/')
            flags = {s.name: s.out_of_line for s in sources.values()}
            self.assertFalse(flags['demo::Node::local'])
            self.assertTrue(flags['demo::Node::remote'])
            self.assertTrue(flags['demo::free_fn'])

    def test_sequence_gaps_both_sides(self):
        left, right = ['a', 'removed', 'b'], ['a', 'added', 'b']
        pairs, score = sa.sequence.ordered_alignment(left, right, lambda a, b: 5 if a == b else -20)
        self.assertEqual(pairs, [(0, 0), (2, 2)])
        self.assertEqual(score, 4)

    def test_sequence_ties_are_deterministic(self):
        expected = sa.sequence.ordered_alignment([1, 2], [3, 4], lambda a, b: -6)
        self.assertEqual(expected, ([], -12))

    def test_inverted_anchors_split_island(self):
        sources = {key: sa.Source(key, key, 'fixture.cpp', line, set(), [], 10, '{}', True)
                   for key, line in [('early', 1), ('middle', 2), ('late', 3)]}
        funcs = {10: dict(callees=[], strings=[], ninstr=4), 20: dict(callees=[], strings=[], ninstr=4)}
        matches = {10: ('late', 'fixture', 'high'), 20: ('early', 'fixture', 'high')}
        proposals, audit = sa.sequence.sequence_proposals(sa, sources, funcs, {}, matches, {}, {})
        self.assertEqual(proposals, [])
        self.assertEqual(audit[0]['decision'], 'inversion:split-island')

    def test_shape_preserves_variable_and_call_identity(self):
        shape = sa.sequence.decompile_shape
        a = '{ local_a = FUN_12345678(param_1); FUN_12345678(local_a); }'
        b = '{ local_b = FUN_87654321(param_9); FUN_87654321(local_b); }'
        c = '{ local_b = FUN_87654321(param_9); FUN_11111111(local_b); }'
        self.assertEqual(shape(a), shape(b))
        self.assertNotEqual(shape(a), shape(c))
        self.assertNotEqual(shape('{ x = x + 1; }'), shape('{ x = x - 1; }'))
        self.assertNotEqual(shape('{ return 1; }', False), shape('{ return 2; }', False))
        self.assertNotEqual(shape('{ emit("fixture 1"); }'), shape('{ emit("fixture 2"); }'))
        self.assertEqual(shape('{ emit("url//fixture"); /* comment */ }'),
                         shape('{ emit("url//fixture"); }'))

    def test_template_guard_rejects_wrong_allocator_and_sentinel(self):
        f = dict(callees=[0x1234], vtwrites=[], strings=[])
        self.assertFalse(sa.sequence.template_guard('allocation', '{}', f))
        self.assertFalse(sa.sequence.template_guard('empty-entry', '{ return *p == -1; }', f))
        self.assertTrue(sa.sequence.template_guard('empty-entry', '{ return *p == -2; }', f))

    def test_constructor_needs_nearby_container(self):
        a = 0x10070000
        row = dict(address=hex(a), symbol='FUN_fixture', match='unknown', anchor='')
        sa.sequence.structural_templates({a: 'void f() { *param_1 = 0; param_1[1] = 0; param_1[2] = 0; return; }'},
                                         {a: dict(callees=[], vtwrites=[], strings=[])}, [row])
        self.assertEqual(row['match'], 'unknown')

    def test_callback_requires_binding_and_rejects_conflict(self):
        a, callback = 0x10070000, 0x10070010
        row = dict(address=hex(callback), symbol='FUN_fixture', anchor='', match='unknown')
        text = 'local_a = FUN_10070010;\nFUN_10007680(local_b,"fixture");\n'
        sa.sequence.callback_bindings({a: text}, [row])
        self.assertEqual(row['symbol'], 'FUN_fixture')
        sa.sequence.callback_bindings({a: text + 'FUN_1009a2a0(this,local_b,local_c);'}, [row])
        self.assertIn('::fixture', row['symbol'])
        self.assertEqual(row['match'], 'unknown')
        row = dict(address=hex(callback), symbol='FUN_fixture', anchor='', match='unknown')
        binding = text + 'FUN_1009a2a0(this,local_b,local_c);'
        sa.sequence.callback_bindings({a: binding, a + 1: binding.replace('fixture', 'second')}, [row])
        self.assertEqual(row['symbol'], 'FUN_fixture')

    def test_reviewed_unknown_is_not_reclassified(self):
        a = 0x10070000
        row = dict(address=hex(a), symbol='FUN_fixture', match='unknown', anchor='string;review')
        body = ('void f() { *(int *)((int)this + 8) = param_1; '
                'FUN_100130a0(this,param_1,1); return; }')
        sa.sequence.structural_templates({a: body}, {a: dict(callees=[0x100130A0], vtwrites=[], strings=[])}, [row])
        self.assertEqual(row['match'], 'unknown')

    def test_vtable_extent_is_checked(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'layout.csv'
            sa.write_csv(path, ['vtable', 'class', 'file', 'slots', 'methods', 'reason'],
                         [dict(vtable='0x64', **{'class': 'fixture::Node'}, file='fixture.cpp',
                               slots=2, methods='first;last', reason='fixture')])
            with self.assertRaises(ValueError):
                sa.sequence.reviewed_vtables(sa, path, [dict(addr=100, slots=[10])], {}, [])

    def test_vtable_default_must_agree(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'layout.csv'
            sa.write_csv(path, ['vtable', 'class', 'file', 'slots', 'methods', 'reason'],
                         [dict(vtable='0x64', **{'class': 'fixture::Node'}, file='fixture.cpp',
                               slots=1, methods='enabled', reason='fixture')])
            row = dict(address='0xA', symbol='FUN_fixture', anchor='', match='unknown')
            source = sa.Source('enabled', 'fixture::Node::enabled', 'fixture.cpp', 1,
                               set(), [], 4, '{ return false; }')
            sa.sequence.reviewed_vtables(sa, path, [dict(addr=100, slots=[10])],
                                         {10: dict(vtwrites=[], ninstr=2)}, [row], {'enabled': source},
                                         {10: 'void f() { return 1; }'})
            self.assertEqual(row['match'], 'unknown')

    def test_changed_requires_explicit_reason(self):
        with self.assertRaises(ValueError):
            sa.sequence.change_reason(dict(address='fixture', match='changed', reason='no evidence'))
        self.assertEqual(sa.sequence.change_reason(dict(address='fixture', match='changed',
                                                        reason='signature:fixture')), 'signature')

    def test_pilot_includes_virtuals_and_direct_dependencies_once(self):
        core, child = 0x100A1BE0, 0x10070000
        rows = [dict(address=hex(a), size=4, ninstr=2, symbol='FUN_fixture', file='',
                     match='unknown', confidence='low') for a in [core, child]]
        scope = sa.sequence.pilot_scope(rows, {core: dict(callees=[child, 0x1234])},
                                        [dict(address=hex(child), vtable='0x100E0C70', slot=4)])
        self.assertEqual({r['address'] for r in scope}, {hex(core), hex(child)})
        self.assertEqual(len(scope), 2)
        self.assertIn('virtual:', next(r['scope'] for r in scope if r['address'] == hex(child)))


if __name__ == "__main__":
    unittest.main(verbosity=2)
