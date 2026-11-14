// [TEMP] A/V sync analysis for bink_harness play runs (Code/BF2/External/ffmpeg-bink/README.md).
//
//   bink_av <run dir> <ref.s16> <channels> <rate> <fps_num> <fps_den> [--rows]
//
// <run dir>/heard.s16 is what the ideal card played (interleaved s16), video.csv says which frame
// was shown when and how many frames the card had played at that moment. ref.s16 is the movie's
// track 0 decoded straight through (bink_harness decodeaudio). The heard stream is aligned
// against the reference in 1024-frame windows every 25 ms; at each shown frame the audible
// source time (aligned reference position) is compared with the frame's own time (k-1)/fps.
// Output: sync error statistics (audio minus video, ms; positive = sound ahead), the heard
// stream's silent and unmatched stretches, and alignment jumps (where the sound skipped).
//
// Build: clang++ -std=c++17 -O2 tools/bink/bink_av.cpp -o bink_av

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static std::vector<short> load(const std::string& p)
{
    std::vector<short> v;
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) { std::fprintf(stderr, "cannot read %s\n", p.c_str()); std::exit(2); }
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    v.resize(size_t(n) / 2);
    if (std::fread(v.data(), 2, v.size(), f) != v.size()) v.clear();
    std::fclose(f);
    return v;
}

int main(int argc, char** argv)
{
    if (argc < 7) { std::fprintf(stderr, "usage: bink_av <run> <ref.s16> <ch> <rate> <fpsnum> <fpsden> [--rows]\n"); return 2; }
    const std::string run = argv[1];
    const int C = std::atoi(argv[3]);
    const double rate = std::atof(argv[4]);
    const double period = std::atof(argv[6]) / std::atof(argv[5]);
    const bool rows = argc > 7 && std::string(argv[7]) == "--rows";
    std::vector<short> heard = load(run + "/heard.s16"), ref = load(argv[2]);
    const long H = long(heard.size()) / C, R = long(ref.size()) / C;
    const int W = 1024;
    const long step = long(rate * 0.025);

    // Mono mix for alignment.
    std::vector<float> h(H), r(R);
    for (long i = 0; i < H; ++i) { float s = 0; for (int c = 0; c < C; ++c) s += heard[i * C + c]; h[i] = s / C; }
    for (long i = 0; i < R; ++i) { float s = 0; for (int c = 0; c < C; ++c) s += ref[i * C + c]; r[i] = s / C; }

    struct Pt { long hp; long off; double err; };   // ref index = hp + off
    std::vector<Pt> pts;
    long prev = 0;
    bool havePrev = false;
    long silentWins = 0, unmatched = 0, total = 0;
    std::vector<std::pair<long, long>> silentRuns;   // [start,end) heard frames, all |x|<=2
    auto search = [&](long hp, long lo, long hi, double e0, long& best, double& bestErr) {
        bestErr = 1e30;
        for (long o = lo; o <= hi; ++o)
        {
            long rp = hp + o;
            if (rp < 0 || rp + W > R) continue;
            double e = 0;
            for (int k = 0; k < W; ++k) { double d = h[hp + k] - r[rp + k]; e += d * d; if (e > bestErr * e0) break; }
            e /= e0;
            if (e < bestErr) { bestErr = e; best = o; }
        }
    };
    for (long hp = 0; hp + W <= H; hp += step)
    {
        ++total;
        double e0 = 0;
        int peak = 0;
        for (int k = 0; k < W; ++k) { e0 += double(h[hp + k]) * h[hp + k]; peak = std::max(peak, int(std::fabs(h[hp + k]))); }
        if (peak <= 64) { ++silentWins; continue; }   // too quiet to align
        long best = 0; double err = 1e30;
        if (havePrev) search(hp, prev - long(rate * 0.05), prev + long(rate * 0.05), e0, best, err);
        if (err > 0.02)
        {
            // wide search, coarse then fine
            long lo = havePrev ? prev - long(rate * 4) : -hp, hi = havePrev ? prev + long(rate * 4) : R - hp - W;
            double ce = 1e30; long cb = 0;
            for (long o = lo; o <= hi; o += 8)
            {
                long rp = hp + o;
                if (rp < 0 || rp + W > R) continue;
                double e = 0;
                for (int k = 0; k < W; k += 4) { double d = h[hp + k] - r[rp + k]; e += d * d; if (e * 4 > ce * e0) break; }
                e = e * 4 / e0;
                if (e < ce) { ce = e; cb = o; }
            }
            search(hp, cb - 8, cb + 8, e0, best, err);
        }
        if (err > 0.02) { ++unmatched; continue; }
        pts.push_back({hp, best, err});
        prev = best;
        havePrev = true;
    }
    // silent runs in the heard stream (exact digital silence, |x| <= 2), >= 20 ms
    {
        long start = -1;
        for (long i = 0; i <= H; ++i)
        {
            bool q = i < H && std::fabs(h[i]) <= 2;
            if (q && start < 0) start = i;
            if (!q && start >= 0) { if (i - start >= long(rate * 0.02)) silentRuns.push_back({start, i}); start = -1; }
        }
    }
    std::printf("heard %.3f s, ref %.3f s; windows %ld: aligned %zu, quiet %ld, unmatched %ld\n",
                H / rate, R / rate, total, pts.size(), silentWins, unmatched);
    for (auto& s : silentRuns)
        std::printf("  silent heard %.3f-%.3f s (%.0f ms)\n", s.first / rate, s.second / rate, (s.second - s.first) * 1000.0 / rate);
    for (size_t i = 1; i < pts.size(); ++i)
        if (std::labs(pts[i].off - pts[i - 1].off) > long(rate * 0.002))
            std::printf("  jump at heard %.3f s: source offset %+.1f -> %+.1f ms\n", pts[i].hp / rate,
                        pts[i - 1].off * 1000.0 / rate, pts[i].off * 1000.0 / rate);
    if (pts.empty()) return 0;
    {
        // Sample-exact comparison over the aligned windows (both channels).
        double sig = 0, noise = 0; long n = 0, exact = 0; int maxd = 0;
        for (const Pt& q : pts)
            for (int k = 0; k < W; ++k)
                for (int c = 0; c < C; ++c)
                {
                    const int a = heard[(q.hp + k) * C + c], b = ref[(q.hp + q.off + k) * C + c];
                    sig += double(b) * b; noise += double(a - b) * (a - b); ++n;
                    exact += a == b; maxd = std::max(maxd, std::abs(a - b));
                }
        std::printf("content over aligned windows: SNR %.1f dB, max |d| %d, identical samples %.2f%%\n",
                    noise > 0 ? 10 * std::log10(sig / noise) : INFINITY, maxd, 100.0 * exact / n);
    }

    // Sync at each shown frame.
    FILE* f = std::fopen((run + "/video.csv").c_str(), "r");
    char line[512];
    std::vector<double> errs;
    double emin = 1e9, emax = -1e9;
    while (std::fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == 't') continue;
        double t; unsigned before, after; char d[32], w[32]; unsigned long long hf; char ev[64] = "";
        // t,before,after,doframe,waits,heard,event
        std::vector<std::string> p;
        std::string s(line);
        size_t a = 0;
        for (;;) { size_t b = s.find(',', a); p.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a)); if (b == std::string::npos) break; a = b + 1; }
        if (p.size() < 7 || p[3].empty()) continue;
        (void)d; (void)w; (void)ev;
        t = std::atof(p[0].c_str()); before = unsigned(std::atoi(p[1].c_str())); after = unsigned(std::atoi(p[2].c_str()));
        hf = std::strtoull(p[5].c_str(), 0, 10);
        (void)after;
        // nearest alignment point at or before hf (within 0.2 s)
        auto it = std::upper_bound(pts.begin(), pts.end(), long(hf), [](long v, const Pt& q) { return v < q.hp; });
        if (it == pts.begin()) continue;
        --it;
        if (long(hf) - it->hp > long(rate * 0.2)) continue;
        const double audio = (double(hf) + it->off) / rate;
        const double video = (before - 1) * period;
        const double e = (audio - video) * 1000.0;
        errs.push_back(e);
        emin = std::min(emin, e); emax = std::max(emax, e);
        if (rows) std::printf("   t=%.1f frame %u audio %.3f video %.3f err %+.1f ms\n", t, before, audio, video, e);
    }
    std::fclose(f);
    if (errs.empty()) { std::printf("no frame rows matched\n"); return 0; }
    std::vector<double> s = errs;
    std::sort(s.begin(), s.end());
    double mean = 0; for (double e : s) mean += e; mean /= s.size();
    std::printf("sync (audio - video) over %zu frames: mean %+.1f ms, median %+.1f, p5 %+.1f, p95 %+.1f, min %+.1f, max %+.1f\n",
                s.size(), mean, s[s.size() / 2], s[s.size() / 20], s[s.size() * 19 / 20], emin, emax);
    return 0;
}
