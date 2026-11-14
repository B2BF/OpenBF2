#!/usr/bin/env python3
"""Unit tests for tools/tracks/marker_check.py and rend_ledger.py (python3 tools/tracks/test_ledger_tools.py).

Each test builds a throwaway Code/BF2 tree, points the tools' REPO at it and checks one fix:
qualified stop propagation, citations above macro invocations, and multi-address citations."""

import csv
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import marker_check as mc  # noqa: E402
import rend_ledger as rl  # noqa: E402

bl = mc.bl


class Tree(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.saved = (mc.REPO, bl.REPO, rl.REPO, rl.head_commit)
        mc.REPO = bl.REPO = rl.REPO = self.tmp.name
        rl.head_commit = lambda: "test"
        self.files = []

    def tearDown(self):
        mc.REPO, bl.REPO, rl.REPO, rl.head_commit = self.saved
        self.tmp.cleanup()

    def write(self, rel, text):
        path = os.path.join(self.tmp.name, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)
        if rel.endswith(mc.EXTS):
            self.files.append(rel)

    def run_ledger(self, addrs):
        export = os.path.join(self.tmp.name, "export")
        for sub, entries in (("RendDX9", addrs), ("BF2exe", [])):
            os.makedirs(os.path.join(export, sub))
            with open(os.path.join(export, sub, "functions.tsv"), "w") as f:
                f.write("entry\tsize\tthunk\tname\tnamespace\n")
                for a in entries:
                    f.write("%s\t16\t0\tFUN_%s\t\n" % (a, a.lower()))
        out = os.path.join(self.tmp.name, "out.csv")
        argv = sys.argv
        sys.argv = ["rend_ledger", "--export", export, "--ledger", out,
                    "--summary", os.path.join(self.tmp.name, "out.md")]
        try:
            rl.main()
        finally:
            sys.argv = argv
        with open(out, newline="") as f:
            return {r["address"]: r for r in csv.DictReader(f)}

    def stopping_names(self, rel_files=None):
        r = mc.analyse_ex(rel_files or self.files)
        return {"::".join(d["comps"]) for d in r["defs"] if id(d) in r["stopping"]}, r


class QualifiedPropagation(Tree):
    def test_unrelated_free_function_of_same_name_does_not_stop(self):
        self.write("Code/BF2/Geom/A.cpp", """
namespace n {
struct Reader { bool readU32(void* d) { return read(d, 4) == 4; } int read(void*, int); };
struct Other { void read() { std::abort(); } };
}
""")
        # Other::read is a thin stopper, but Reader::readU32 calls the member `read` of Reader
        names, _ = self.stopping_names()
        self.assertIn("Other::read", names)
        self.assertNotIn("Reader::readU32", names)

    def test_unresolved_bare_name_does_not_propagate(self):
        self.write("Code/BF2/Geom/A.cpp", """
void stopper() { std::abort(); }
struct K { void f() { stopper(); } };
struct L { void g() { setSize(1); } };
""")
        self.write("Code/BF2/Geom/B.cpp", """
namespace other { struct M { void setSize(int) { std::abort(); } }; }
""")
        names, _ = self.stopping_names()
        self.assertIn("K::f", names)          # unqualified call to a free stopper, same scope
        self.assertNotIn("L::g", names)       # `setSize` could be anything: no propagation

    def test_member_call_matches_own_class_then_base(self):
        self.write("Code/BF2/Geom/A.cpp", """
struct Base { void halt() { std::abort(); } };
struct Derived : public Base { void go() { halt(); } };
struct Unrelated { void go2() { halt(); } };
""")
        names, _ = self.stopping_names()
        self.assertIn("Derived::go", names)
        self.assertNotIn("Unrelated::go2", names)

    def test_own_class_member_shadows_a_stopping_free_function(self):
        self.write("Code/BF2/Geom/A.cpp", """
void setSize(int) { std::abort(); }
struct T { void setSize(int v) { m = v; } T() { setSize(1); } int m; };
""")
        names, _ = self.stopping_names()
        self.assertNotIn("T::T", names)

    def test_qualified_call_matches_path_suffix(self):
        self.write("Code/BF2/Geom/A.cpp", """
namespace a { void halt() { std::abort(); } }
namespace b { void halt() { } }
void viaA() { a::halt(); }
void viaB() { b::halt(); }
""")
        names, _ = self.stopping_names()
        self.assertIn("viaA", names)
        self.assertNotIn("viaB", names)

    def test_direct_stops_unchanged(self):
        self.write("Code/BF2/Geom/A.cpp", """
void a() { std::abort(); }
void b() { renddx9abi::frontier("x"); }
void c() { RD9_FRONTIER(); }
""")
        names, _ = self.stopping_names()
        self.assertEqual({"a", "b", "c"}, names)

    def test_previous_function_tail_is_not_a_helper_head(self):
        # a noreturn-looking tail right above must not make the next function a stop helper
        self.write("Code/BF2/Geom/A.cpp", """
struct B {
    void frontierHere() { std::fprintf(stderr, "[FRONTIER]"); std::abort(); }
    void run() override { x = 1; }
    int x;
};
struct C { void go() { run(); } void run() {} };
""")
        names, r = self.stopping_names()
        self.assertNotIn("run", r["stops"])
        self.assertNotIn("C::go", names)


class MacroCitations(Tree):
    def test_comment_above_macro_invocation_is_a_citation(self):
        self.write("Code/BF2/Geom/Con.cpp", """
#define BF2_CONSOLE_THING(name, fn) static int s_##name = 1;
// PE 0x10001000: the argument conversion.
BF2_CONSOLE_THING(alpha, f)
BF2_CONSOLE_THING(beta, f) // PE 0x10001010
// unrelated comment

BF2_CONSOLE_THING(gamma, f)
""")
        r = mc.analyse_ex(self.files)
        got = {c["label"]: (c["comment"].strip(), c["stops"]) for c in r["citations"]}
        self.assertIn("PE 0x10001000", got["alpha"][0])
        self.assertIn("PE 0x10001010", got["beta"][0])
        self.assertNotIn("PE", got["gamma"][0])
        self.assertFalse(any(s for _, s in got.values()))

    def test_ledger_row_done_and_stopping_macro_stops(self):
        self.write("Code/BF2/Geom/Con.cpp", """
#define BF2_CONSOLE_THING(name, fn) static int s_##name = 1;
// PE 0x10001000 / 0x10001010
BF2_CONSOLE_THING(alpha, f)
// PE 0x10001020
BF2_CONSOLE_THING(beta, RD9_FRONTIER("x"))
""")
        rows = self.run_ledger(["10001000", "10001010", "10001020", "10001030"])
        self.assertEqual("done", rows["0x10001000"]["status"])
        self.assertEqual("done", rows["0x10001010"]["status"])
        self.assertEqual("alpha", rows["0x10001000"]["symbol"])
        self.assertEqual("stop", rows["0x10001020"]["status"])
        self.assertEqual("todo", rows["0x10001030"]["status"])

    def test_definition_after_macro_invocations_does_not_absorb_their_citations(self):
        # macro invocations without a trailing ';' used to become the next definition's head, so
        # its comment block spanned (and multi-claimed) every citation above them
        self.write("Code/BF2/Geom/Con.cpp", """
#define BF2_CONSOLE_THING(name, fn) static int s_##name = 1;
static int s_before = 0;
// PE 0x10001000, 0x10001010: alpha constructor, converter.
BF2_CONSOLE_THING(alpha,
                  f)
// PE 0x10001020, 0x10001030
BF2_CONSOLE_THING(beta, f)

// a remark without an address
void after() { s_before = 1; }
// PE 0x10001040
void owned() { s_before = 2; }
""")
        rows = self.run_ledger(["10001000", "10001010", "10001020", "10001030", "10001040"])
        self.assertEqual("alpha", rows["0x10001010"]["symbol"])
        self.assertEqual("beta", rows["0x10001030"]["symbol"])
        self.assertEqual("owned", rows["0x10001040"]["symbol"])
        r = mc.analyse_ex(self.files)
        after = [d for d in r["defs"] if d["comps"] == ["after"]][0]
        self.assertNotIn("0x1000", after["comment"])
        self.assertEqual(11, after["line"])
        with open(os.path.join(self.tmp.name, "out.md")) as f:
            self.assertNotIn("after", f.read())                       # no conflict any more


class MultiAddress(Tree):
    FUNCS = {"RendDX9.dll": {a: None for a in range(0x10001000, 0x10001100, 0x10)}, "BF2.exe": {}}

    def test_citation_list_forms(self):
        f = self.FUNCS
        cl = lambda t: [a for _, a in rl.citation_list(t, f)]
        self.assertEqual([0x10001000, 0x10001010], cl("// PE 0x10001000, 0x10001010"))
        self.assertEqual([0x10001000, 0x10001010, 0x10001020], cl("// PE 0x10001000 / 0x10001010 / 0x10001020."))
        self.assertEqual([0x10001000, 0x10001010, 0x10001020],
                         cl("// PE 0x10001000 (also 0x10001010, 0x10001020)"))
        self.assertEqual([0x10001000, 0x10001010], cl("// PE 0x10001000: x\n// PE 0x10001010"))
        # prose after the address: nothing beyond the first-address rule
        self.assertEqual([], cl("// PE 0x10001000 is the callee of PE 0x10001010"))
        # not a function entry of the image: ignored
        self.assertEqual([0x10001000], cl("// PE 0x10001000, 0x10009990"))

    def test_definition_claims_all_addresses_and_conflicts_keep_first_address_winner(self):
        self.write("Code/BF2/Geom/Inl.cpp", """
// PE 0x10001000 (also 0x10001010)
int shared() { return 1; }
// PE 0x10001010
int owner() { return 2; }
// PE 0x10001020, 0x10001030
inline int twice() { return 3; }
""")
        rows = self.run_ledger(["10001000", "10001010", "10001020", "10001030", "10001040"])
        self.assertEqual("shared", rows["0x10001000"]["symbol"])
        self.assertEqual("owner", rows["0x10001010"]["symbol"])      # its own first address wins
        self.assertEqual("twice", rows["0x10001020"]["symbol"])
        self.assertEqual("twice", rows["0x10001030"]["symbol"])
        self.assertEqual("done", rows["0x10001030"]["status"])
        self.assertEqual("todo", rows["0x10001040"]["status"])
        with open(os.path.join(self.tmp.name, "out.md")) as f:
            self.assertIn("0x10001010", f.read())                     # the conflict is still reported

    def test_stop_status_applies_to_every_claimed_address(self):
        self.write("Code/BF2/Geom/Inl.cpp", """
// PE 0x10001000, 0x10001010
void both() { std::abort(); }
""")
        rows = self.run_ledger(["10001000", "10001010"])
        self.assertEqual({"stop"}, {rows[a]["status"] for a in rows})

    def test_memory_dll_sources_do_not_claim_renddx9_rows(self):
        # Code/BF2/Memory is Memory.dll; its PE addresses overlap RendDX9.dll's range.
        self.write("Code/BF2/Memory/Memory.cpp", """
// PE 0x10001000.
void memoryError() { }
""")
        self.write("Code/BF2/Geom/Own.cpp", """
// PE 0x10001010
void own() { }
""")
        rows = self.run_ledger(["10001000", "10001010"])
        self.assertEqual("todo", rows["0x10001000"]["status"])
        self.assertEqual("done", rows["0x10001010"]["status"])


if __name__ == "__main__":
    unittest.main()
