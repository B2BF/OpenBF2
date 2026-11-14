// [CHOSEN, not recovered] bf2_bink_decode_test: CPU check of the macOS Bink port (RendDX9Abi/
// BinkFFmpeg.cpp over Code/BF2/External/ffmpeg-bink) against references the local ffmpeg CLI
// decodes from the read-only game content. No GPU, game engine or network. References and frames
// stay outside the repository (Code/BF2/External/ffmpeg-bink/README.md has the commands).
//
//   video <movie.bik> <frame (1-based)> <ref.yuv420p> [<ref.bgra>]
//       Plays frames 1..N through BinkWait-free BinkDoFrame/BinkNextFrame, then compares frame N's
//       YUV planes byte for byte and BinkCopyToBuffer's BGRx against ffmpeg's bgra (PSNR).
//       Grey movies (header flag 0x20000, the two menu movies): ffmpeg's bgra converts their
//       zero chroma planes to green, so the BGRx is checked against the neutral-grey expectation
//       instead (R = G = B, within 1 of BT.601 limited-range luma of the reference Y plane);
//       retail binkw32 shows these movies grey (Code/BF2/External/ffmpeg-bink/README.md).
//   loop <movie.bik>
//       Plays the movie twice through the FrameNum wrap and checks frame 1 decodes identically.
//   audio <movie.bik> <ref.f32le interleaved>
//       Decodes track 0 through the audio tap and reports SNR against ffmpeg's samples.

#include "../RendDX9Abi/BinkPort.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace renddx9abi;

namespace {

std::vector<unsigned char> readFile(const char* path)
{
    std::vector<unsigned char> data;
    std::FILE* f = std::fopen(path, "rb");
    if (!f)
        return data;
    std::fseek(f, 0, SEEK_END);
    data.resize(size_t(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    if (std::fread(data.data(), 1, data.size(), f) != data.size())
        data.clear();
    std::fclose(f);
    return data;
}

double psnr(double mse) { return mse > 0 ? 10.0 * std::log10(255.0 * 255.0 / mse) : INFINITY; }

// Header video flag 0x20000 (luma-only file), dword at offset 36.
bool grayMovie(const char* movie)
{
    unsigned char hdr[40];
    std::FILE* f = std::fopen(movie, "rb");
    if (!f)
        return false;
    const bool ok = std::fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr);
    std::fclose(f);
    return ok && (hdr[38] & 0x02) != 0;
}

int videoTest(const char* movie, unsigned frame, const char* refYuv, const char* refBgra)
{
    HBINK b = BinkOpen(movie, 0);
    if (!b) { std::printf("FAIL: BinkOpen(%s)\n", movie); return 1; }
    BINKSUMMARY sum;
    BinkGetSummary(b, &sum);
    std::printf("movie %ux%u, %u frames, %u/%u fps\n", sum.Width, sum.Height, sum.TotalFrames,
                sum.FileFrameRate, sum.FileFrameRateDiv);
    if (frame < 1 || frame > sum.TotalFrames) { std::printf("FAIL: frame %u out of range\n", frame); return 1; }
    const unsigned w = sum.Width, h = sum.Height;
    std::vector<unsigned char> bgra(size_t(w) * h * 4);
    const auto t0 = std::chrono::steady_clock::now();
    unsigned decoded = 0;
    while (b->FrameNum != frame)
    {
        BinkDoFrame(b);
        BinkNextFrame(b);
        ++decoded;
    }
    BinkDoFrame(b);
    BinkCopyToBuffer(b, bgra.data(), int(w * 4), h, 0, 0, BINK_COPYALL | BINK_SURFACE_32BGRA);
    ++decoded;
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0).count();
    std::printf("decoded %u frames (+copy of the last), %.3f ms per frame\n", decoded, ms / decoded);

    FFBinkPicture pic;
    if (!binkPortPicture(b, &pic)) { std::printf("FAIL: no picture\n"); return 1; }
    const std::vector<unsigned char> ref = readFile(refYuv);
    const size_t ys = size_t(w) * h, cs = size_t((w + 1) / 2) * ((h + 1) / 2);
    if (ref.size() != ys + 2 * cs) { std::printf("FAIL: %s has %zu bytes, want %zu\n", refYuv, ref.size(), ys + 2 * cs); return 1; }
    int failures = 0;
    const char* names[3] = { "Y", "U", "V" };
    size_t off = 0;
    for (int p = 0; p < 3; ++p)
    {
        const unsigned pw = p ? (w + 1) / 2 : w, ph = p ? (h + 1) / 2 : h;
        size_t diff = 0;
        double se = 0;
        for (unsigned y = 0; y < ph; ++y)
            for (unsigned x = 0; x < pw; ++x)
            {
                const int a = pic.plane[p][size_t(y) * pic.linesize[p] + x];
                const int r = ref[off + size_t(y) * pw + x];
                if (a != r) { ++diff; se += double(a - r) * (a - r); }
            }
        off += size_t(pw) * ph;
        std::printf("frame %u plane %s: %zu of %u samples differ, PSNR %.2f dB\n", frame, names[p],
                    diff, pw * ph, psnr(se / (double(pw) * ph)));
        failures += diff != 0;
    }
    if (refBgra && grayMovie(movie))
    {
        std::printf("grey movie: ffmpeg's bgra is green here (zero chroma), not compared; checking "
                    "the neutral-grey expectation from the reference Y plane\n");
        size_t notGrey = 0;
        int maxd = 0;
        double se = 0;
        for (unsigned y = 0; y < h; ++y)
            for (unsigned x = 0; x < w; ++x)
            {
                const unsigned char* px = &bgra[(size_t(y) * w + x) * 4];
                notGrey += px[0] != px[1] || px[1] != px[2];
                double e = (int(ref[size_t(y) * w + x]) - 16) * 255.0 / 219.0;
                e = e < 0 ? 0 : (e > 255 ? 255 : e);
                const int d = int(px[1]) - int(std::lround(e));
                se += double(d) * d;
                maxd = std::max(maxd, std::abs(d));
            }
        std::printf("frame %u BGRx vs neutral grey: %zu non-grey pixels, max |diff| %d, PSNR %.2f dB\n",
                    frame, notGrey, maxd, psnr(se / (double(w) * h)));
        failures += notGrey != 0 || maxd > 1;
    }
    else if (refBgra)
    {
        const std::vector<unsigned char> rb = readFile(refBgra);
        if (rb.size() != size_t(w) * h * 4) { std::printf("FAIL: %s size\n", refBgra); return 1; }
        double se = 0;
        int maxd = 0;
        for (size_t i = 0; i < rb.size(); i += 4)
            for (int c = 0; c < 3; ++c)
            {
                const int d = int(bgra[i + c]) - int(rb[i + c]);
                se += double(d) * d;
                maxd = std::max(maxd, std::abs(d));
            }
        const double p = psnr(se / (double(w) * h * 3));
        std::printf("frame %u BGRx vs ffmpeg bgra: PSNR %.2f dB, max |diff| %d\n", frame, p, maxd);
        failures += p < 35.0;
    }
    BinkClose(b);
    std::printf("%s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}

int loopTest(const char* movie)
{
    HBINK b = BinkOpen(movie, 0);
    if (!b) { std::printf("FAIL: BinkOpen(%s)\n", movie); return 1; }
    FFBinkPicture pic;
    BinkDoFrame(b);
    binkPortPicture(b, &pic);
    std::vector<unsigned char> first(pic.plane[0], pic.plane[0] + size_t(pic.linesize[0]) * b->Height);
    unsigned steps = 0;
    do
    {
        BinkNextFrame(b);
        ++steps;
        BinkDoFrame(b);
    } while (b->FrameNum != 1);
    binkPortPicture(b, &pic);
    const bool same = std::memcmp(first.data(), pic.plane[0], first.size()) == 0;
    std::printf("wrapped to frame 1 after %u NextFrame calls (Frames %u); frame 1 Y %s\n", steps,
                b->Frames, same ? "identical" : "DIFFERENT");
    const bool ok = same && steps == b->Frames;
    BinkClose(b);
    std::printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

struct AudioCapture { std::vector<float> interleaved; int channels = 0, rate = 0; };

void tap(void* user, const float* const* planes, int channels, int samples, int rate)
{
    AudioCapture* c = static_cast<AudioCapture*>(user);
    c->channels = channels;
    c->rate = rate;
    for (int s = 0; s < samples; ++s)
        for (int ch = 0; ch < channels; ++ch)
            c->interleaved.push_back(planes[ch][s]);
}

int audioTest(const char* movie, const char* refPath)
{
    AudioCapture cap;
    binkPortSetAudioTap(&tap, &cap);
    HBINK b = BinkOpen(movie, 0);
    if (!b) { std::printf("FAIL: BinkOpen(%s)\n", movie); return 1; }
    for (unsigned i = 0; i < b->Frames; ++i)
    {
        BinkDoFrame(b);
        BinkNextFrame(b);
    }
    BinkClose(b);
    binkPortSetAudioTap(0, 0);
    const std::vector<unsigned char> raw = readFile(refPath);
    const size_t refCount = raw.size() / sizeof(float);
    const float* ref = reinterpret_cast<const float*>(raw.data());
    const size_t n = std::min(refCount, cap.interleaved.size());
    double ar = 0, aa = 0, rr = 0;
    for (size_t i = 0; i < n; ++i)
    {
        ar += double(cap.interleaved[i]) * ref[i];
        aa += double(cap.interleaved[i]) * cap.interleaved[i];
        rr += double(ref[i]) * ref[i];
    }
    const double gain = aa > 0 ? ar / aa : 0;
    double e1 = 0, eg = 0;
    float maxd = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double d1 = cap.interleaved[i] - ref[i];
        const double dg = gain * cap.interleaved[i] - ref[i];
        e1 += d1 * d1;
        eg += dg * dg;
        maxd = std::max(maxd, float(std::fabs(d1)));
    }
    const double snr1 = 10.0 * std::log10(rr / std::max(e1, 1e-30));
    const double snrg = 10.0 * std::log10(rr / std::max(eg, 1e-30));
    std::printf("audio: %d ch, %d Hz, port %zu samples, ffmpeg %zu samples\n", cap.channels, cap.rate,
                cap.interleaved.size(), refCount);
    std::printf("SNR %.2f dB (max |diff| %.3g); best-fit gain %.6f gives SNR %.2f dB\n", snr1, maxd,
                gain, snrg);
    const bool ok = snr1 > 60.0 && cap.interleaved.size() == refCount;
    std::printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 5 && !std::strcmp(argv[1], "video"))
        return videoTest(argv[2], unsigned(std::atoi(argv[3])), argv[4], argc > 5 ? argv[5] : 0);
    if (argc == 3 && !std::strcmp(argv[1], "loop"))
        return loopTest(argv[2]);
    if (argc == 4 && !std::strcmp(argv[1], "audio"))
        return audioTest(argv[2], argv[3]);
    std::fprintf(stderr, "usage: %s video <bik> <frame> <ref.yuv> [<ref.bgra>] | loop <bik> | "
                         "audio <bik> <ref.f32le>\n", argv[0]);
    return 2;
}
