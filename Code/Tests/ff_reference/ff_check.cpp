// [CHOSEN, not recovered] Checks the fixed-function vertex CPU model (Platform/Gfx/Metal/FFVertex.h)
// against ff_reference.txt, the real d3d9 runtime's ProcessVertices output for ff_cases.cpp
// (software vertex processing, so the GPU does not matter). The matrices come from the file, so
// libm differences in building them do not enter.
//   ff_check ff_reference.txt [--device DEVICE.txt]
// --device also compares a file written by macos_metal_device --ff-device (the port's
// ProcessVertices driven with the generator's own calls, ff_run.inc) block by block: vertex, clip,
// nocopy, offset. Components the runtime left at the sentinel (NaN, 0xdeadbeef) must stay unwritten.
#include "ff_setup.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct RefVertex { float screen[4]; unsigned diffuse, specular; float t[2][4]; };
float hexf(const std::string& s) { return std::strtof(s.c_str(), nullptr); }
struct RefCase {
    std::string name; std::vector<ffref::Mat> mats; std::vector<RefVertex> verts;
    std::map<std::string, std::vector<RefVertex>> blocks;   // vertex, clip, nocopy, offset
};
struct RefFile { std::vector<RefCase> cases; std::string device; int endCases = -1, endVerts = -1, variants = -1, errors = 0; };
bool parse(const char* path, RefFile& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        std::string tag; s >> tag;
        if (tag == "device") out.device = line.substr(7);
        else if (tag == "case") { RefCase c; int i; s >> i >> c.name; out.cases.push_back(c); }
        else if (tag == "matrix" && !out.cases.empty()) {
            std::string name, v; s >> name; ffref::Mat m{};
            for (float& f : m.m) { s >> v; f = hexf(v); }
            out.cases.back().mats.push_back(m);
        } else if ((tag == "vertex" || tag == "clip" || tag == "nocopy" || tag == "offset") && !out.cases.empty()) {
            RefVertex r{}; int i; std::string v; s >> i;
            for (float& f : r.screen) { s >> v; f = hexf(v); }
            s >> std::hex >> r.diffuse >> r.specular >> std::dec;
            for (auto& t : r.t) for (float& f : t) { s >> v; f = hexf(v); }
            out.cases.back().blocks[tag].push_back(r);
            if (tag == "vertex") out.cases.back().verts.push_back(r);
        } else if (tag == "variants") s >> out.variants;
        else if (tag == "end") s >> out.endCases >> out.endVerts;
        else if (tag == "error") { std::fprintf(stderr, "ff_check: %s has %s\n", path, line.c_str()); ++out.errors; }
    }
    return true;
}


unsigned packColor(const float* c) {
    auto b = [](float v) { v = v < 0 ? 0 : v > 1 ? 1 : v; return unsigned(std::lround(v * 255.0f)); };
    return (b(c[3]) << 24) | (b(c[0]) << 16) | (b(c[1]) << 8) | b(c[2]);
}
bool colorClose(unsigned a, unsigned b) {
    for (int s = 0; s < 32; s += 8) if (std::abs(int((a >> s) & 255) - int((b >> s) & 255)) > 1) return false;
    return true;
}
bool close(float a, float b, float rel) {
    if (std::isnan(b)) return true;   // the runtime did not write this component (sentinel)
    if (std::isnan(a)) return false;
    return std::fabs(a - b) <= rel * std::fmax(1.0f, std::fmax(std::fabs(a), std::fabs(b)));
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: ff_check ff_reference.txt [--device DEVICE.txt]\n"); return 2; }
    const char* devicePath = nullptr;
    for (int i = 2; i + 1 < argc; ++i) if (!std::strcmp(argv[i], "--device")) devicePath = argv[i + 1];
    RefFile file;
    if (!parse(argv[1], file)) { std::fprintf(stderr, "ff_check: cannot read %s\n", argv[1]); return 2; }
    std::vector<RefCase>& ref = file.cases;
    const std::string& device = file.device;
    const int endCases = file.endCases, endVerts = file.endVerts, errors = file.errors;
    int count = 0, vcount = 0;
    const ffref::Case* cases = ffref::cases(count);
    const ffref::Vertex* verts = ffref::vertices(vcount);
    if (errors || endCases != ffref::kCaseCount || endVerts != ffref::kCaseCount * ffref::kVertexCount ||
        int(ref.size()) != count || count != ffref::kCaseCount) {
        std::fprintf(stderr, "ff_check: reference incomplete (end %d %d, %zu cases, table %d)\n", endCases, endVerts, ref.size(), count);
        return 1;
    }
    std::printf("ff_check: reference device %s\n", device.c_str());
    int failed = 0, checked = 0, skipped = 0;
    for (int ci = 0; ci < count; ++ci) {
        const ffref::Case& c = cases[ci];
        const RefCase& r = ref[ci];
        if (r.name != c.name || r.mats.size() != 8 || int(r.verts.size()) != vcount) {
            std::printf("FAIL %s: reference case mismatch\n", c.name); ++failed; continue;
        }
        bf2ff::Key k;
        static bf2ff::Constants K;
        ffref::setup(c, r.mats.data(), k, K);
        bool ok = true;
        for (int vi = 0; vi < vcount; ++vi) {
            const ffref::Vertex& v = verts[vi];
            bf2ff::VertexIn x;
            ffref::vertexIn(c, v, x);
            bf2ff::VertexOut o;
            bf2ff::process(k, K, x, o);
            // ProcessVertices writes the vertex fog factor into the specular alpha
            float spec[4] = {o.specular[0], o.specular[1], o.specular[2], k.fog ? o.fog : o.specular[3]};
            const RefVertex& e = r.verts[vi];
            std::string why;
            // x, y in pixels: 1/1000 px absolute (float rounding on ~800 px values); z, rhw relative
            for (int j = 0; j < 2; ++j) if (!(std::isnan(e.screen[j]) || std::fabs(o.screen[j] - e.screen[j]) <= 1e-3f)) why += " screen";
            for (int j = 2; j < 4; ++j) if (!close(o.screen[j], e.screen[j], 2e-5f)) why += " screen";
            if (!colorClose(packColor(o.diffuse), e.diffuse)) why += " diffuse";
            if (!colorClose(packColor(spec), e.specular)) why += " specular";
            // ProcessVertices writes all four components of each set, but only the ones D3D9 defines
            // carry results; the rest hold runtime scratch (e.g. 49.99999 left in tex0.zw by the
            // projected case before it, though the destination is NaN-filled before every case):
            // a count from TEXTURETRANSFORMFLAGS, else 3 for camera-space texgen, 2 for the sphere
            // map, the input set's components for pass-through.
            for (int t = 0; t < 2; ++t) {
                const unsigned gen = c.tci[t] >> 16, count = c.ttff[t] & 0xFF;
                const int defined = count >= 1 && count <= 4 ? int(count) : gen == 4 ? 2 : gen ? 3 : 2;
                for (int j = 0; j < defined; ++j)
                    if (!close(o.tex[t][j], e.t[t][j], 2e-5f)) { why += " tex" + std::to_string(t); break; }
            }
            if (!why.empty()) {
                if (ok) std::printf("FAIL %s\n", c.name);
                ok = false;
                std::printf("  vertex %d:%s\n    got  %g %g %g %g %08x %08x | %g %g %g %g | %g %g %g %g\n"
                            "    want %g %g %g %g %08x %08x | %g %g %g %g | %g %g %g %g\n", vi, why.c_str(),
                            o.screen[0], o.screen[1], o.screen[2], o.screen[3], packColor(o.diffuse), packColor(spec),
                            o.tex[0][0], o.tex[0][1], o.tex[0][2], o.tex[0][3], o.tex[1][0], o.tex[1][1], o.tex[1][2], o.tex[1][3],
                            e.screen[0], e.screen[1], e.screen[2], e.screen[3], e.diffuse, e.specular,
                            e.t[0][0], e.t[0][1], e.t[0][2], e.t[0][3], e.t[1][0], e.t[1][1], e.t[1][2], e.t[1][3]);
            }
        }
        ++checked;
        if (ok) std::printf("ok   %s\n", c.name); else ++failed;
    }
    std::printf("ff_check: %d checked, %d failed\n", checked, failed);
    if (devicePath) {
        RefFile dev;
        if (!parse(devicePath, dev) || dev.errors || dev.cases.size() != ref.size()) {
            std::fprintf(stderr, "ff_check: device file %s unreadable or incomplete\n", devicePath);
            return 1;
        }
        int blocks = 0, blockFails = 0;
        for (int ci = 0; ci < count; ++ci) {
            const ffref::Case& c = cases[ci];
            for (const auto& b : ref[ci].blocks) {
                const auto it = dev.cases[ci].blocks.find(b.first);
                ++blocks;
                if (it == dev.cases[ci].blocks.end() || it->second.size() != b.second.size()) {
                    std::printf("FAIL device %s %s: block missing\n", c.name, b.first.c_str()); ++blockFails; continue;
                }
                bool ok = true;
                for (std::size_t vi = 0; vi < b.second.size(); ++vi) {
                    const RefVertex& e = b.second[vi]; const RefVertex& g = it->second[vi];
                    std::string why;
                    auto flt = [&](float got, float want, float tol, const char* what) {
                        if (std::isnan(want)) { if (!std::isnan(got)) why += std::string(" ") + what + "(written)"; return; }
                        if (std::isnan(got) || std::fabs(got - want) > tol * std::fmax(1.0f, std::fabs(want))) why += std::string(" ") + what;
                    };
                    for (int j = 0; j < 2; ++j) { if (std::isnan(e.screen[j])) flt(g.screen[j], e.screen[j], 0, "screen");
                                                  else if (std::isnan(g.screen[j]) || std::fabs(g.screen[j] - e.screen[j]) > 1e-3f) why += " screen"; }
                    for (int j = 2; j < 4; ++j) flt(g.screen[j], e.screen[j], 2e-5f, "screen");
                    auto col = [&](unsigned got, unsigned want, const char* what) {
                        if (want == 0xdeadbeefu) { if (got != want) why += std::string(" ") + what + "(written)"; return; }
                        if (!colorClose(got, want)) why += std::string(" ") + what;
                    };
                    col(g.diffuse, e.diffuse, "diffuse"); col(g.specular, e.specular, "specular");
                    for (int t = 0; t < 2; ++t) {
                        const unsigned gen = c.tci[t] >> 16, cnt = c.ttff[t] & 0xFF;
                        const int defined = cnt >= 1 && cnt <= 4 ? int(cnt) : gen == 4 ? 2 : gen ? 3 : 2;
                        for (int j = 0; j < 4; ++j) {
                            if (std::isnan(e.t[t][j])) { if (!std::isnan(g.t[t][j])) { why += " tex" + std::to_string(t) + "(written)"; break; } continue; }
                            if (j < defined && (std::isnan(g.t[t][j]) || std::fabs(g.t[t][j] - e.t[t][j]) > 2e-5f * std::fmax(1.0f, std::fabs(e.t[t][j])))) { why += " tex" + std::to_string(t); break; }
                        }
                    }
                    if (!why.empty()) {
                        if (ok) std::printf("FAIL device %s %s\n", c.name, b.first.c_str());
                        ok = false;
                        std::printf("  vertex %zu:%s\n    got  %g %g %g %g %08x %08x | %g %g %g %g | %g %g %g %g\n"
                                    "    want %g %g %g %g %08x %08x | %g %g %g %g | %g %g %g %g\n", vi, why.c_str(),
                                    g.screen[0], g.screen[1], g.screen[2], g.screen[3], g.diffuse, g.specular,
                                    g.t[0][0], g.t[0][1], g.t[0][2], g.t[0][3], g.t[1][0], g.t[1][1], g.t[1][2], g.t[1][3],
                                    e.screen[0], e.screen[1], e.screen[2], e.screen[3], e.diffuse, e.specular,
                                    e.t[0][0], e.t[0][1], e.t[0][2], e.t[0][3], e.t[1][0], e.t[1][1], e.t[1][2], e.t[1][3]);
                    }
                }
                if (!ok) ++blockFails;
            }
        }
        std::printf("ff_check: device %s: %d blocks, %d failed\n", dev.device.c_str(), blocks, blockFails);
        if (blockFails) failed += blockFails;
    }
    return failed ? 1 : 0;
}
