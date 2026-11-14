// [TEMP] [CHOSEN, not recovered] Native run of the self-authored __proto__ probe SWFs
// (tools/swf/proto_probe.py; spec docs/tracks/game/swf-proto-retail-probe.md), CTest
// bf2_swf_proto_probe_check. Each SWF must show exactly the squares retail 1.50 showed on Boot Camp
// (probe v2, 67788e01, evidence swf-proto-retail-20261113/v2-67788e01, kept outside the repository):
// greens 1, 4, 6, 7, 8, 9; reds 2, 3, 5; `done` every time; no fault. No game content is used.
#include "gameswf_impl.h"
#include "base/tu_file.h"
#include <cstdio>
#include <string>

using namespace gameswf;

namespace {
// Retail 1.50's colour per test: true = green (ok), false = red (bad).
const bool kRetailGreen[10] = { false, true, false, false, true, false, true, true, true, true };

unsigned checks, failures;
void check(bool value, const std::string& label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label.c_str()); }
}

// Loads one probe SWF, plays both frames and returns whether each named square is visible.
bool visible(movie* root, const std::string& name)
{
    as_value square, shown;
    if (!root->get_member(name.c_str(), &square) || !square.to_object()) return false;
    square.to_object()->get_member("_visible", &shown);
    return shown.to_bool();
}

void runProbe(const std::string& dir, const std::string& file, int only)
{
    const std::string path = dir + "/" + file;
    tu_file in(path.c_str(), "rb");
    check(!in.get_error(), file + ": opens");
    if (in.get_error()) return;
    movie_definition* def = create_movie_no_recurse(&in, DO_NOT_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
    check(def != NULL, file + ": parses");
    if (!def) return;
    movie_interface* inst = def->create_instance();
    inst->advance(0.0f);
    inst->advance(0.1f);
    inst->advance(0.1f);
    movie* root = inst->get_root_movie();
    check(visible(root, "done"), file + ": done");
    for (int test = 1; test <= 9; ++test) {
        const bool ran = only == 0 || only == test;
        const bool ok = visible(root, "ok" + std::to_string(test));
        const bool bad = visible(root, "bad" + std::to_string(test));
        const std::string label = file + ": test " + std::to_string(test);
        if (!ran) {
            check(!ok && !bad, label + " stays hidden when not built in");
            continue;
        }
        check(ok == kRetailGreen[test] && bad == !kRetailGreen[test],
              label + (kRetailGreen[test] ? " green as retail" : " red as retail"));
    }
    inst->drop_ref();
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s <proto_probe.py swf output dir>\n", argv[0]); return 2; }
    runProbe(argv[1], "probe_all.swf", 0);
    for (int test = 1; test <= 9; ++test)
        runProbe(argv[1], "probe" + std::to_string(test) + ".swf", test);
    std::printf("swf proto probe %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
