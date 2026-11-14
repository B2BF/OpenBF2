"""Evidence-boundary checks for PDB-backed x86 pairing."""
import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch

from test_attrib import fixture
from x86 import X86Matcher, signature


def xfixture(left, right):
    baseline = fixture(left, right)
    funcs = {}
    for i, g in enumerate(baseline.G.values()):
        b = 0x10201000 + i * 0x100
        funcs[hex(b)] = dict(g, address=b, insns=[], callers=[], imports=[])
    return X86Matcher(dict(functions=funcs, vtables={}), baseline)


class X86Boundaries(unittest.TestCase):
    def test_unique_literals_agree_independently(self):
        m = xfixture([["terrain.bmp", "Unable to lock terrain"]],
                     [["terrain.bmp", "Unable to lock terrain"]])
        self.assertEqual(m.anchors(), 1)
        self.assertEqual(m.pairs[0x10001000]["confidence"], "high")

    def test_conflicting_literals_remain_unpaired(self):
        m = xfixture([["terrain.bmp", "Unable to lock terrain"]],
                     [["terrain.bmp"], ["Unable to lock terrain"]])
        self.assertEqual(m.anchors(), 0)

    def test_one_literal_inside_wrapper_is_not_a_complete_body_identity(self):
        m = xfixture([["terrain.bmp"]], [["terrain.bmp"]])
        self.assertEqual(m.anchors(), 0)
        self.assertIn("wrapper/body", m.rejected[(0x10001000, 0x10201000)])

    def test_return_cleanup_contradiction_vetoes_literal_pair(self):
        m = xfixture([["terrain.bmp", "Unable to lock terrain"]],
                     [["terrain.bmp", "Unable to lock terrain"]])
        m.left[0x10001000]["returns"] = (4,)
        m.right[0x10201000]["returns"] = (8,)
        self.assertEqual(m.anchors(), 0)

    def test_uniqueness_includes_functions_without_source_associations(self):
        m = xfixture([["terrain.bmp"]], [["terrain.bmp"], ["terrain.bmp"]])
        m.G[0x10201100]["source"] = None
        m.usable.remove(0x10201100)
        self.assertEqual(m.anchors(), 0)

    def test_stack_and_scaled_index_are_not_object_offsets(self):
        f = dict(insns=[[0, "mov", "eax, dword ptr [esp + 0x38]"],
                        [1, "mov", "eax, dword ptr [ebp + 0x38]"],
                        [2, "mov", "eax, dword ptr [esi + 4*eax + 0x38]"],
                        [3, "mov", "eax, dword ptr [ecx + 0x38]"],
                        [4, "call", "dword ptr [eax + 0x5c]"],
                        [5, "ret", "0x8"]], floats=[])
        s = signature(f)
        self.assertEqual(s["fields"], (0x38,))
        self.assertEqual(s["slots"], (0x5c,))
        self.assertEqual(s["returns"], (8,))

    def test_layout_slot_retn_count_as_one_family(self):
        m = xfixture([[]], [[]])
        insns = [[0, "mov", "eax, dword ptr [ecx + 0x38]"],
                 [1, "mov", "eax, dword ptr [ecx + 0x40]"],
                 [2, "call", "dword ptr [eax + 0x5c]"], [3, "ret", "0x8"]]
        m.F[0x10001000]["insns"] = m.G[0x10201000]["insns"] = insns
        m.left[0x10001000] = m.right[0x10201000] = signature(dict(insns=insns, floats=[]))
        self.assertEqual(m.signatures(), 1)
        self.assertEqual(m.pairs[0x10001000]["confidence"], "medium")

    def test_frontier_identity_is_reported_without_completed_match(self):
        m = xfixture([["terrain.bmp", "Unable to lock terrain"]], [["terrain.bmp", "Unable to lock terrain"]])
        m.sources[0]["stopping"] = True
        self.assertEqual(m.anchors(), 1)
        r = dict(address="0x10001000", classification="missing", confidence="low",
                 our_definition="", evidence={})
        m.apply([r])
        self.assertEqual(r["classification"], "missing")
        self.assertTrue(r["evidence"]["known_source_frontiers"])
        self.assertFalse(m.reverse)

    def test_pdb_does_not_associate_a_basename_only(self):
        from x86_extract import source_path
        source_path.paths = {"code/bf2/geom/terrain.cpp": "Code/BF2/Geom/Terrain.cpp"}
        self.assertEqual(source_path("Terrain.cpp"), "")
        self.assertEqual(source_path(r"D:\src\Code\BF2\Geom\Terrain.cpp"), "Code/BF2/Geom/Terrain.cpp")

    def test_documented_split_explains_file_veto_but_does_not_vote(self):
        m = xfixture([["terrain.bmp"]], [["terrain.bmp"]])
        m.matcher.rmap[0x10001000] = dict(confidence="high", file_basis="site", file="Geom/Owner.cpp")
        m.G[0x10201000]["source_file"] = "Code/BF2/Geom/OwnerDraw.cpp"
        self.assertFalse(m.compatible(0x10001000, 0x10201000)[0])
        m.G[0x10201000]["documented_original_tus"] = ["Code/BF2/Geom/Owner.cpp"]
        self.assertTrue(m.compatible(0x10001000, 0x10201000)[0])
        self.assertFalse(m.votes)

    def test_folded_distinct_source_definitions_remain_ambiguous(self):
        from x86_extract import associate
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Code").mkdir()
            (root / "Code/Owner.cpp").write_text("// fixture\n")
            sources = [dict(file="Code/Owner.cpp", name="Owner::" + name,
                            line=line, body="{ return value; }", stopping=False)
                       for name, line in (("first", 10), ("second", 20))]
            funcs = {"0x1000": dict(analysis_end=0x1010)}
            exported = [dict(entry="0x1000", namespace="Owner", name="first",
                             prototype="int first()", symbol_source="IMPORTED")]
            symbols = [dict(address="0x1000", name=d["name"], source="IMPORTED") for d in sources]
            lines = [dict(address="0x1000", file="/C:/src/Code/Owner.cpp", line=str(d["line"]), length="4")
                     for d in sources]
            with patch("x86_extract.REPO", root):
                associate(funcs, exported, symbols, lines, sources)
            self.assertIsNone(funcs["0x1000"]["source"])
            self.assertEqual(funcs["0x1000"]["source_candidates"], [0, 1])

    def test_link_thunk_needs_an_attested_direct_jump(self):
        from x86_extract import link_thunks
        exported = [dict(entry=hex(a), thunk="1") for a in (0x1000, 0x1010, 0x1020)]
        with patch("x86_extract.asm.load", return_value=(
                [0x1000, 0x1010, 0x1020], ["jmp", "jmp", "add"],
                ["0x1010", "0x2000", "ecx, 0x4"])):
            thunks, resolve = link_thunks(Path("unused"), exported)
        self.assertEqual(resolve(0x1000), 0x2000)
        self.assertNotIn(0x1020, thunks)

    def test_import_resolution_does_not_propagate_through_business_helpers(self):
        from x86_extract import import_thunks
        leaf = dict(address=0x2000, insns=[[0x2000, "jmp", "dword ptr [0x3000]"]],
                    imports=["d3dx9_25.dll!D3DXCreateTexture"], apis=["D3DXCreateTexture"])
        helper = dict(address=0x2100, insns=[[0x2100, "call", "dword ptr [0x3000]"], [0x2106, "ret", ""]],
                      imports=["d3dx9_25.dll!D3DXCreateTexture"], apis=["D3DXCreateTexture"])
        caller = dict(address=0x1000, insns=[[0x1000, "call", "0x1010"], [0x1005, "call", "0x2100"]],
                      imports=[], apis=[])
        result = dict(functions={"0x1000": caller, "0x2000": leaf, "0x2100": helper},
                      link_thunks={"0x1010": 0x2000})
        import_thunks(result)
        self.assertEqual(caller["apis"], ["D3DXCreateTexture"])
        self.assertEqual(len(caller["resolved_import_calls"]), 1)
        self.assertEqual(caller["resolved_import_calls"][0]["thunk"], 0x2000)


if __name__ == "__main__":
    unittest.main()
