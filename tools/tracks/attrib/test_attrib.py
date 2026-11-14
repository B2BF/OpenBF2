"""Regression checks for evidence boundaries, not implementation-shaped coverage tests.

    python3 -m unittest discover -s tools/tracks/attrib -p 'test_*.py'
"""
import unittest
import struct
import tempfile
from pathlib import Path
from unittest.mock import patch

from classify import Matcher


def fixture(strings1, strings2):
    retail, native, sources = {}, {}, []
    for i, strings in enumerate(strings1):
        a = 0x10001000 + i * 0x100
        retail[hex(a)] = dict(address=a, name="FUN_%x" % a, strings=strings, apis=[],
                              n=30, imports=[], calls=[], floats=[], consts=[], callers=[], insns=[])
    for i, strings in enumerate(strings2):
        name = "Owner::method%d()" % i
        sources.append(dict(file="test.cpp", name=name, short=name, params=0,
                            comment="", body="{ perform(); }", stopping=False, line=1))
        native[str(i)] = dict(name=name, source_file="test.cpp", source=i, strings=strings,
                              n=30, apis=[], calls=[], floats=[], consts=[])
    data = dict(retail=dict(functions=retail, vtables={}),
                native=dict(functions=native, symbols={}, tables=[]), sources=sources)
    ledger = [dict(image="RendDX9.dll", address=hex(a), status="todo", symbol=f["name"], file="")
              for a, f in ((int(k, 16), f) for k, f in retail.items())]
    return Matcher(data, ledger, [])


class PairingBoundaries(unittest.TestCase):
    def test_independent_unique_literals(self):
        m = fixture([["terrain.bmp", "Unable to lock terrain"]],
                    [["terrain.bmp", "Unable to lock terrain"]])
        self.assertEqual(m.anchor_pass(), 1)
        self.assertEqual(m.pairs[0x10001000]["confidence"], "high")

    def test_conflicting_anchors_do_not_choose_a_winner(self):
        m = fixture([["terrain.bmp", "Unable to lock terrain"]],
                    [["terrain.bmp"], ["Unable to lock terrain"]])
        self.assertEqual(m.anchor_pass(), 0)
        self.assertFalse(m.pairs)

    def test_shared_error_message_is_not_identity(self):
        m = fixture([["Unable to lock terrain"], ["Unable to lock terrain"]],
                    [["Unable to lock terrain"]])
        self.assertEqual(m.anchor_pass(), 0)

    def test_stopping_definition_does_not_count_as_matched(self):
        m = fixture([["terrain.bmp"]], [["terrain.bmp"]])
        m.sources[0]["stopping"] = True
        self.assertEqual(m.anchor_pass(), 0)

    def test_conflicting_debug_file_vetoes_pair(self):
        m = fixture([["terrain.bmp"]], [["terrain.bmp"]])
        m.rmap[0x10001000] = dict(confidence="high", file_basis="site", file="Geom/Other.cpp")
        self.assertEqual(m.anchor_pass(), 0)

    def test_itanium_destructor_positions_are_not_adapter_slots(self):
        m = fixture([[], [], [], []], [[], [], [], []])
        for i in range(3):
            m.pairs[0x10001000 + i * 0x100] = dict(native=str(i), confidence="cited", seed=True)
        m.refresh_reverse()
        m.data["native"]["symbols"] = {"sym%d" % i: [str(i)] for i in range(4)}
        m.data["retail"]["vtables"] = {"0x10200000": [0x10001000 + i * 0x100 for i in range(4)]}
        m.data["native"]["tables"] = [dict(name="vtable for Owner", slots=[[i, "sym%d" % i]
                                                                          for i in range(4)])]
        self.assertEqual(m.vtable_pass(), 0)
        m.data["native"]["tables"][0]["name"] = "explicitAdapterVtable"
        self.assertEqual(m.vtable_pass(), 1)

    def test_cpp_table_requires_a_paired_owner_relocation(self):
        m = fixture([[], [], [], [], []], [[], [], [], [], []])
        for i in (0, 1, 2, 4):
            m.pairs[0x10001000 + i * 0x100] = dict(native=str(i), confidence="cited", seed=True)
        m.refresh_reverse()
        m.data["native"]["symbols"] = {"sym%d" % i: [str(i)] for i in range(4)}
        m.data["retail"]["vtables"] = {"0x10200000": [0x10001000 + i * 0x100 for i in range(4)]}
        m.data["native"]["tables"] = [dict(object="test.o", name="vtable for Owner",
                                          slots=[[i + 3, "sym%d" % i] for i in range(4)])]
        m.F[0x10001400]["insns"] = [[0x10001400, "mov", "dword ptr [ecx], 0x10200000"]]
        self.assertEqual(m.cpp_vtable_pass(), 0)
        m.G["4"]["vtrefs"] = ["vtable for Owner"]
        self.assertEqual(m.cpp_vtable_pass(), 1)

    def test_cpp_table_rejects_conflicting_ordinary_slot_deltas(self):
        m = fixture([[], [], [], [], []], [[], [], [], [], []])
        for i in (0, 1, 2, 4):
            m.pairs[0x10001000 + i * 0x100] = dict(native=str(i), confidence="cited", seed=True)
        m.refresh_reverse()
        m.data["native"]["symbols"] = {"sym%d" % i: [str(i)] for i in range(4)}
        m.data["retail"]["vtables"] = {"0x10200000": [0x10001000 + i * 0x100 for i in range(4)]}
        m.data["native"]["tables"] = [dict(object="test.o", name="vtable for Owner",
                                          slots=[[3, "sym0"], [4, "sym1"], [6, "sym2"], [5, "sym3"]])]
        m.F[0x10001400]["insns"] = [[0x10001400, "mov", "dword ptr [ecx], 0x10200000"]]
        m.G["4"]["vtrefs"] = ["vtable for Owner"]
        self.assertEqual(m.cpp_vtable_pass(), 0)


class RuntimeBoundaries(unittest.TestCase):
    def rules(self, strings, imports=(), calls=(), insns=()):
        from rules import RuntimeRules
        r = RuntimeRules.__new__(RuntimeRules)
        a = 0x10001000
        r.F = {a: dict(strings=list(strings), imports=list(imports), calls=list(calls),
                       insns=list(insns), n=len(insns), callers=[])}
        r.L = {a: dict(status="todo", symbol="FUN_10001000")}
        r.index, r.result, r.free, r.alloc = {}, {}, set(), set()
        r.import_target = lambda target: "msvcr71.dll!_CxxThrowException"
        return r, a

    def test_standard_library_use_does_not_exclude_game_method(self):
        r, a = self.rules(["atlas", "list<T> too long"], calls=[0x101C44C2])
        r.stl()
        self.assertNotIn(a, r.result)

    def test_string_import_is_not_an_stl_template_identity(self):
        r, a = self.rules([], imports=["msvcp71.dll!basic_string_destructor"])
        r.stl()
        self.assertNotIn(a, r.result)

    def test_diagnostic_and_throw_identify_bounded_container_kernel(self):
        r, a = self.rules(["list<T> too long"], calls=[0x101C44C2])
        r.stl()
        self.assertEqual(r.result[a]["classification"], "stl")

    def test_virtual_business_call_vetoes_container_kernel(self):
        r, a = self.rules(["list<T> too long"], calls=[0x101C44C2],
                          insns=[[0x10001000, "call", "dword ptr [eax + 0x24]"]])
        r.stl()
        self.assertNotIn(a, r.result)

    def test_optimized_initterm_arguments_in_data_section(self):
        from rules import RuntimeRules
        r = RuntimeRules.__new__(RuntimeRules)
        a = 0x10001000
        lo, hi = 0x10236000, 0x10236008
        r.A = [1, 2, 3]
        r.M = ["mov", "push", "call"]
        r.O = ["dword ptr [esp], 0x10236008", "0x10236000", "0x101c48ba"]
        r.import_target = lambda target: "msvcr71.dll!_initterm"
        r.imports, r.index = {}, {a: 0}
        r.result, r.static_initializers = {}, {}
        r.eh_handlers, r.eh_actions, r.eh_catches = {}, {}, {}
        r.F, r.L = {a: dict(insns=[])}, {a: dict(status="todo")}
        class Memory:
            def u32(self, address):
                return {lo: a, lo + 4: 0}[address]
            def in_sec(self, address, name):
                return name == ".data" and lo <= address < hi
        r.pe = Memory()
        r.crt_init()
        self.assertEqual(r.result[a]["classification"], "compiler")


class SecondaryBoundaries(unittest.TestCase):
    def console_table(self, delta):
        fields = [("getObjectName", "mov", "eax, dword ptr [ecx + 0x8]"),
                  ("getFunctionName", "mov", "eax, dword ptr [ecx + 0xc]"),
                  ("getType", "mov", "eax, dword ptr [ecx + 0x4]"),
                  ("getAccess", "mov", "eax, dword ptr [ecx + 0x10]"),
                  ("getCategory", "mov", "al, byte ptr [ecx + 0x14]")]
        functions, slots, retail = {}, {}, []
        for i, (name, m, o) in enumerate(fields):
            a = 0x10001000 + i * 0x10
            functions[a] = dict(insns=[(a, m, o), (a + 3, "ret", "")])
            retail.append(a)
            slots[i + delta] = dict(name="Owner::" + name + "() const")
        return retail, slots, functions

    def test_console_delta_is_measured_instead_of_assumed(self):
        from secondary import console_slot_mapping
        retail, slots, functions = self.console_table(7)
        self.assertEqual(console_slot_mapping(retail, slots, functions, 12)[0], 7)

    def test_one_contradicting_metadata_getter_blocks_table_mapping(self):
        from secondary import console_slot_mapping
        retail, slots, functions = self.console_table(3)
        slots[20] = slots.pop(6)
        self.assertIsNone(console_slot_mapping(retail, slots, functions, 8))

    def test_inlining_requires_a_retail_call_to_the_helper(self):
        from secondary import inlined
        m = fixture([[], ["unique shader literal"]], [["unique shader literal"]])
        m.pairs[0x10001000] = dict(native="0", seed=True, confidence="cited")
        m.refresh_reverse()
        self.assertEqual(inlined(m.data, m), {})
        m.F[0x10001100]["callers"] = [0x10001000]
        result = inlined(m.data, m)
        self.assertEqual(result[0x10001100]["classification"], "inlined")

    def test_neighbour_file_is_not_editor_reachability(self):
        from secondary import editor
        m = fixture([[]], [[]])
        m.ledger[0]["file"] = "Code/BF2/Geom/TerrainEditable.cpp"
        rmap = {0x10001000: dict(confidence="high", file_basis="neighbour",
                               file="Geom/TerrainEditable.cpp", addr_r="0x10100000", method="order")}
        self.assertEqual(editor(m.data, m.ledger, rmap, m), {})
        rmap[0x10001000]["file_basis"] = "site"
        self.assertEqual(editor(m.data, m.ledger, rmap, m)[0x10001000]["classification"], "editor")


class ExtractionBoundaries(unittest.TestCase):
    def test_static_initializer_code_is_not_dropped(self):
        from native import Object
        # Minimal test-only Mach-O with two code sections; no game bytes.
        section = lambda name, addr, offset: struct.pack("<16s16sQQIIIIIIII", name, b"__TEXT", addr,
                    1, offset, 0, 0, 0, 0x80000400, 0, 0, 0)
        strings = b"\0_plain\0__GLOBAL__sub_I_test.cpp\0"
        command_size = 72 + 160 + 24
        code_offset = 32 + command_size
        symbols_offset = code_offset + 2
        str_offset = symbols_offset + 32
        data = struct.pack("<IIIIIIII", 0xFEEDFACF, 0x01000007, 3, 1, 2, command_size, 0, 0)
        data += struct.pack("<II16sQQQQIIII", 0x19, 232, b"", 0, 2, code_offset, 2, 7, 7, 2, 0)
        data += section(b"__text", 0, code_offset) + section(b"__StaticInit", 1, code_offset + 1)
        data += struct.pack("<IIIIII", 2, 24, symbols_offset, 2, str_offset, len(strings))
        data += b"\xc3\xc3" + struct.pack("<IBBHQ", 1, 0xF, 1, 0, 0) + struct.pack("<IBBHQ", 8, 0xF, 2, 0, 1)
        obj = Object("test.o", data + strings)
        self.assertEqual(obj.entries, [0, 1])
        self.assertEqual({s["name"] for s in obj.texts}, {"__text", "__StaticInit"})

    def test_generated_class_requires_the_actual_macro_line(self):
        from extract import associate_console_macros
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "test.cpp").write_text("// fixture\nBF2_CONSOLE_PROPERTY_FN(example, bool, active, get, set, tag)\n")
            function = dict(source=None, source_file="test.cpp", name="Owner::ConsoleClass2::executeObjectMethod()",
                            mangled="symbol", call_names=[], calls=[], source_candidates=[])
            data, sources = dict(functions={"0": function}), []
            with patch("extract.REPO", root):
                associate_console_macros(data, sources)
            self.assertEqual(function["source"], 0)
            self.assertEqual(sources[0]["generated_macro"], "BF2_CONSOLE_PROPERTY_FN")
            self.assertEqual(sources[0]["comment"], "")
            function.update(source=None, name="Owner::ConsoleClass1::executeObjectMethod()")
            with patch("extract.REPO", root):
                associate_console_macros(data, sources)
            self.assertIsNone(function["source"])

    def test_multiline_macro_uses_closing_line_but_reports_invocation_start(self):
        from extract import associate_console_macros
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "test.cpp").write_text("// fixture\nBF2_CONSOLE_PROPERTY_FN(example, bool,\n  active, get, set, tag)\n")
            function = dict(source=None, source_file="test.cpp", name="Owner::ConsoleClass3::executeObjectMethod()",
                            mangled="symbol", call_names=[], calls=[], source_candidates=[])
            sources = []
            with patch("extract.REPO", root):
                associate_console_macros(dict(functions={"0": function}), sources)
            self.assertEqual(sources[0]["line"], 2)
            self.assertEqual(sources[0]["generated_class_line"], 3)

    def test_vtable_leaf_does_not_swallow_next_unlisted_function(self):
        from extract import reachable_instructions
        insns = [(0x1000, "mov", "eax, dword ptr [ecx + 0x10]"), (0x1003, "ret", ""),
                 (0x1004, "int3", ""), (0x1010, "mov", "eax, dword ptr [ecx + 0x14]"), (0x1013, "ret", "")]
        actual, complete = reachable_instructions(None, insns, 0x1000, 0x1020)
        self.assertTrue(complete)
        self.assertEqual(actual, insns[:2])

    def test_reachable_branches_keep_both_return_paths(self):
        from extract import reachable_instructions
        insns = [(0x1000, "test", "eax, eax"), (0x1002, "je", "0x1008"),
                 (0x1004, "mov", "eax, 0x1"), (0x1007, "ret", ""),
                 (0x1008, "xor", "eax, eax"), (0x100A, "ret", "")]
        actual, complete = reachable_instructions(None, insns, 0x1000, 0x1020)
        self.assertTrue(complete)
        self.assertEqual(actual, insns)


class LedgerCompatibility(unittest.TestCase):
    def test_attribution_does_not_mutate_existing_columns(self):
        from rend_ledger import apply_attribution
        original = [dict(image="RendDX9.dll", address="0x10001000", status=s, symbol="name", file="file")
                    for s in ("todo", "done", "chosen", "stop", "skip")]
        output = [dict(r) for r in original]
        apply_attribution(output, {"0x10001000": "stl"})
        for before, after in zip(original, output):
            self.assertEqual(before, {k: v for k, v in after.items() if k != "class"})
        self.assertEqual([r["class"] for r in output], ["stl", "cited", "cited", "cited", ""])

    def test_unclassified_new_todo_is_not_silently_excluded(self):
        from rend_ledger import apply_attribution
        with self.assertRaisesRegex(ValueError, "regenerate attribution"):
            apply_attribution([dict(image="RendDX9.dll", address="0x10001000", status="todo")], {})


if __name__ == "__main__":
    unittest.main()
