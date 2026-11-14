#!/usr/bin/env python3
"""Generator regression checks; binary-backed cases skip when the reference ELF is absent."""

import csv
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/ai-reconstruct"))
import evidence
import gen_static_init as gen


class SourceDefinitions(unittest.TestCase):
    obj = dict(symbol="dice::hfe::ai::infoTypes", type="std::string", count=2,
               arguments=["Fire", "MoveTo"])
    prefix = '#include "header.h"\nnamespace dice { namespace hfe { namespace ai {\n'
    source = prefix + 'std::string infoTypes[2] = { "Fire", "MoveTo" };\n}}}\n'

    def test_exact_type_value_extent_and_namespace(self):
        self.assertIsNotNone(gen.string_definition(self.source, self.obj))
        for before, after in [('"Fire", "MoveTo"', '"MoveTo", "Fire"'),
                              ("[2]", "[3]"), ("std::string", "OtherString"),
                              ("std::string", "const std::string"),
                              ("namespace ai", "namespace other"),
                              ('"MoveTo"', '"Other"')]:
            with self.subTest(after=after):
                self.assertIsNone(gen.string_definition(self.source.replace(before, after), self.obj))

    def test_function_local_is_not_namespace_scope(self):
        local = self.prefix + 'void f() { std::string infoTypes[2] = { "Fire", "MoveTo" }; }\n}}}\n'
        self.assertIsNone(gen.string_definition(local, self.obj))

    def test_preceding_object_requires_order_review(self):
        source = self.source.replace('std::string infoTypes', 'Other other;\nstd::string infoTypes')
        self.assertIsNone(gen.string_definition(source, self.obj))

    def test_missing_definition_emitted_once_before_other_definitions(self):
        source = self.prefix + 'Other other;\n}}}\n'
        edited, reason = gen.string_edit(source, self.obj, "target.cpp", {"target.cpp": source})
        self.assertEqual(reason, "added string-array definition")
        self.assertIsNotNone(gen.string_definition(edited, self.obj))
        self.assertLess(edited.index("infoTypes"), edited.index("Other other"))
        self.assertEqual(gen.string_edit(edited, self.obj, "target.cpp", {})[0], edited)

    def test_other_tu_blocks_emission(self):
        edited, reason = gen.string_edit(self.prefix + '}}}\n', self.obj, "target.cpp",
                                         {"other.cpp": self.source})
        self.assertIsNone(edited)
        self.assertIn("another TU", reason)

    def test_wrong_existing_type_cannot_create_duplicate_definition(self):
        source = self.source.replace("std::string", "OtherString")
        edited, reason = gen.string_edit(source, self.obj, "target.cpp", {})
        self.assertIsNone(edited)
        self.assertIn("differs in type", reason)

    def test_extern_declaration_allows_missing_definition(self):
        source = self.prefix + 'extern std::string infoTypes[2];\n}}}\n'
        edited, reason = gen.string_edit(source, self.obj, "target.cpp", {})
        self.assertEqual(reason, "added string-array definition")
        self.assertIsNotNone(gen.string_definition(edited, self.obj))

    def test_iostream_precedes_static_objects(self):
        edited, _ = gen.include_edit(self.source, "// static init: ELF 0x12345\n")
        self.assertLess(edited.index("#include <iostream>"), edited.index("namespace dice"))
        self.assertIsNone(gen.include_edit("Other other;\n#include <iostream>\n", "// x\n")[0])

    def test_lone_teardown_is_not_attached_to_next_tu(self):
        rows = [dict(symbol="__tcf_0"), dict(symbol="SomeClass::method()"),
                dict(symbol="__static_initialization_and_destruction_0(int, int)"),
                dict(symbol="__tcf_0"), dict(symbol="_GLOBAL__I_key")]
        self.assertEqual([len(g) for g in gen.static_groups(rows)], [1, 3])

    def test_missing_attested_tu_does_not_fall_back_to_neighbor(self):
        group = [dict(mangled='_GLOBAL__I__GLOBAL__N_AI_AIDLL_AI_Missing.cppHash',
                      file='neighbor.cpp')]
        target, reason = gen.source_target(group, [], ['neighbor.cpp'])
        self.assertEqual(target, '')
        self.assertIn('absent', reason)

    def test_competing_data_definitions_do_not_fall_back_to_neighbor(self):
        group = [dict(mangled='_GLOBAL__I_key', file='neighbor.cpp')]
        target, reason = gen.source_target(group, [], ['neighbor.cpp'],
                                         {'key': {'one.cpp', 'two.cpp'}})
        self.assertEqual(target, '')
        self.assertIn('competing definitions', reason)


class OriginalInstructions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        binary = Path(evidence.BIN_DIR or "/nonexistent") / "ia-32/libaidll.so"
        if not binary.exists():
            raise unittest.SkipTest("original ia-32 image unavailable")
        cls.image = evidence.Image("ia-32", str(binary))
        with open(ROOT / "docs/ai-reconstruct/ledger.csv") as ledger:
            cls.rows = list(csv.DictReader(ledger))
        cls.groups = gen.static_groups(cls.rows)

    def group(self, address):
        return next(g for g in self.groups if g[0]["address"] == address)

    def altered(self, group, row_number, before, after):
        original_insns = self.image.insns
        address = int(group[row_number]["address"], 16)
        def insns(start, end, *args, **kwargs):
            result = original_insns(start, end, *args, **kwargs)
            if start == address:
                changed = [(a, text.replace(before, after)) for a, text in result]
                self.assertNotEqual(changed, result)
                return changed
            return result
        with patch.object(self.image, "insns", side_effect=insns):
            return gen.classify(self.image, group)

    def test_shipped_iostream_register_variants(self):
        for address in ("0x235010", "0x1A1930", "0x1A27A0"):
            with self.subTest(address=address):
                self.assertEqual(gen.classify(self.image, self.group(address))["shape"], "ios-only")

    def test_string_array_has_literal_order_and_reverse_teardown(self):
        result = gen.classify(self.image, self.group("0x1F5740"))
        self.assertEqual(result["shape"], "ios+string-array")
        self.assertEqual(result["objects"][1]["arguments"], ["Fire", "DefuseMines", "MoveTo", "Avoid",
            "Idle", "Roam", "Scout", "TakeCover", "Change", "Default"])
        changed = self.altered(self.group("0x1F5740"), 2, "sub esi, 0x4", "sub esi, 0x8")
        self.assertEqual(changed["shape"], "unclassified")

    def test_inlined_stores_are_not_mistaken_for_iostream_only(self):
        for address in ("0x21AB30", "0x2247B0", "0x48A850"):
            result = gen.classify(self.image, self.group(address))
            self.assertNotEqual(result["shape"], "ios-only")
            self.assertTrue(result["writes"])

    def test_wrong_atexit_argument_is_rejected(self):
        result = self.altered(self.group("0x235010"), 0, "xor eax, eax", "mov eax, 0x1")
        self.assertEqual(result["shape"], "unclassified")

    def test_changed_guard_is_rejected(self):
        result = self.altered(self.group("0x235010"), 0, "[ebp + 0x8], 0x1", "[ebp + 0x8], 0x2")
        self.assertEqual(result["shape"], "unclassified")

    def test_different_destructor_receiver_is_rejected(self):
        result = self.altered(self.group("0x235010"), 1, "[ebx + 0x1b3b8]", "[ebx + 0x1b3b4]")
        self.assertEqual(result["shape"], "unclassified")

    def test_wrapper_priority_is_checked(self):
        result = self.altered(self.group("0x235010"), 2, "0xffff", "0xfffe")
        self.assertEqual(result["shape"], "unclassified")

    def test_console_teardown_vptr_and_symbol_are_checked(self):
        row = next(r for r in self.rows if r["address"] == "0x40A8D0")
        self.assertEqual(gen.console_teardown(self.image, row),
                         "dice::hfe::ai::(anonymous namespace)::consoleObject161")
        original_insns = self.image.insns
        def insns(start, end, *args, **kwargs):
            return [(a, text.replace("add eax, 0x8", "add eax, 0x4"))
                    for a, text in original_insns(start, end, *args, **kwargs)]
        with patch.object(self.image, "insns", side_effect=insns):
            self.assertIsNone(gen.console_teardown(self.image, row))

    def run_failed_transaction(self, failure):
        group = self.group("0x235010")
        scratch = ROOT / 'out/g2-static/test-workspaces'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            root = Path(directory)
            filename = group[0]["file"]
            path = root / filename
            path.parent.mkdir(parents=True)
            original = '#include "BBPC4.h"\nnamespace dice { namespace hfe { namespace ai {\n}}}\n'
            path.write_text(original)
            ledger = root / 'ledger.csv'
            with ledger.open('w') as stream:
                writer = csv.DictWriter(stream, fieldnames=group[0].keys())
                writer.writeheader()
                writer.writerows(group)
            report = root / 'report.json'
            args = SimpleNamespace(apply=True, directory='AIBotBehaviourPattern', ledger=ledger,
                                   binary=self.image.binary, report=report)
            with patch.object(gen, 'REPO', root), \
                    patch.object(gen.build_ledger, 'manifest_files', return_value=([filename], [])), \
                    patch.object(gen.evidence, 'Image', return_value=self.image):
                if failure == 'classification':
                    with patch.object(gen, 'row_shape', side_effect=RuntimeError('test report failure')):
                        with self.assertRaises(RuntimeError):
                            gen.run(args)
                else:
                    write = Path.write_text
                    def write_text(destination, text, *a, **kw):
                        if destination == report:
                            raise OSError('test report write failure')
                        return write(destination, text, *a, **kw)
                    with patch.object(Path, 'write_text', write_text):
                        with self.assertRaises(OSError):
                            gen.run(args)
            self.assertEqual(path.read_text(), original)
            self.assertFalse(report.exists())

    def test_report_analysis_failure_never_mutates_source(self):
        self.run_failed_transaction('classification')

    def test_report_write_failure_rolls_back_source(self):
        self.run_failed_transaction('write')

    def test_data_keys_detect_merged_interface_tus(self):
        groups = [self.group('0x3E6990'), self.group('0x3E6B40')]
        filename = gen.AI_ROOT + 'Main/IAIPathfinding.cpp'
        source = ('namespace dice { namespace hfe { namespace ai {\n'
                  'IAIPathfinding* IAIPathfinding::instance = 0;\n'
                  'IAIPathfindingSetup* IAIPathfindingSetup::instance = 0;\n}}}\n')
        keys = gen.data_key_files(self.image, groups, {filename: source}, [filename])
        targets = [gen.source_target(group, self.rows, [filename], keys)[0] for group in groups]
        self.assertEqual(targets, [filename, filename])

    def test_extern_data_key_is_not_a_definition(self):
        groups = [self.group('0x3E6990')]
        filename = gen.AI_ROOT + 'Main/IAIPathfinding.cpp'
        source = ('namespace dice { namespace hfe { namespace ai {\n'
                  'extern IAIPathfinding* IAIPathfinding::instance;\n}}}\n')
        keys = gen.data_key_files(self.image, groups, {filename: source}, [filename])
        self.assertFalse(keys)


if __name__ == "__main__":
    unittest.main()
