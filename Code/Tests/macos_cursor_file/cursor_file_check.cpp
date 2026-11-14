// [TEMP] [CHOSEN, not recovered] Checks Platform/macOS/MacCursorFile.h on synthetic cursor files:
// a 32 bpp alpha .cur, a 1 bpp masked .cur, a 4 bpp .ico, an animated RIFF ACON with rate and
// seq chunks, and truncated or foreign data. Everything is generated here.
#include "../../BF2/Platform/macOS/MacCursorFile.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace dice::hfe::macos;

namespace {
void require(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}

typedef std::vector<std::uint8_t> Bytes;
void put16(Bytes& b, unsigned v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); }
void put32(Bytes& b, unsigned v) { put16(b, v & 0xFFFF); put16(b, v >> 16); }
void putId(Bytes& b, const char* id) { b.insert(b.end(), id, id + 4); }

// A DIB with a doubled height. pixel(x, y) answers BGRA (bpp 32/24) or a palette index.
template <class Pixel, class Mask>
Bytes dib(int w, int h, int bpp, const Bytes& palette, Pixel pixel, Mask mask)
{
    Bytes b;
    put32(b, 40); put32(b, w); put32(b, h * 2); put16(b, 1); put16(b, bpp);
    put32(b, 0); put32(b, 0); put32(b, 0); put32(b, 0);
    put32(b, bpp <= 8 ? static_cast<unsigned>(palette.size() / 4) : 0); put32(b, 0);
    b.insert(b.end(), palette.begin(), palette.end());
    const size_t xorStride = ((w * bpp + 31) / 32) * 4;
    for (int y = h - 1; y >= 0; --y) {                      // bottom-up
        Bytes row(xorStride, 0);
        for (int x = 0; x < w; ++x) {
            const unsigned v = pixel(x, y);
            if (bpp == 32) for (int i = 0; i < 4; ++i) row[x * 4 + i] = (v >> (8 * i)) & 0xFF;
            else if (bpp == 24) for (int i = 0; i < 3; ++i) row[x * 3 + i] = (v >> (8 * i)) & 0xFF;
            else if (bpp == 4) row[x / 2] |= (v & 0xF) << ((x & 1) ? 0 : 4);
            else if (bpp == 1) row[x / 8] |= (v & 1) << (7 - (x & 7));
        }
        b.insert(b.end(), row.begin(), row.end());
    }
    const size_t andStride = ((w + 31) / 32) * 4;
    for (int y = h - 1; y >= 0; --y) {
        Bytes row(andStride, 0);
        for (int x = 0; x < w; ++x)
            if (mask(x, y)) row[x / 8] |= 1 << (7 - (x & 7));
        b.insert(b.end(), row.begin(), row.end());
    }
    return b;
}

Bytes iconDir(int type, int w, int h, int hotX, int hotY, const Bytes& image)
{
    Bytes b;
    put16(b, 0); put16(b, type); put16(b, 1);
    b.push_back(w & 0xFF); b.push_back(h & 0xFF); b.push_back(0); b.push_back(0);
    put16(b, hotX); put16(b, hotY); put32(b, static_cast<unsigned>(image.size())); put32(b, 22);
    b.insert(b.end(), image.begin(), image.end());
    return b;
}

void chunk(Bytes& b, const char* id, const Bytes& body)
{
    putId(b, id); put32(b, static_cast<unsigned>(body.size()));
    b.insert(b.end(), body.begin(), body.end());
    if (body.size() & 1) b.push_back(0);
}

const std::uint8_t* px(const CursorFrame& f, int x, int y) { return &f.rgba[(y * f.width + x) * 4]; }
}

int main()
{
    // 32 bpp with alpha: alpha wins over the (all-opaque) mask; rows come out top first.
    {
        Bytes image = dib(8, 4, 32, Bytes(),
                          [](int x, int y) { return (y == 0 ? 0x80u : 0xFFu) << 24 | 0x0000FFu * (x == 1); },
                          [](int, int) { return false; });
        Bytes file = iconDir(2, 8, 4, 3, 2, image);
        CursorAnimation a;
        require(decodeCursorFile(file.data(), file.size(), a), "32 bpp .cur decodes");
        require(a.frames.size() == 1 && a.steps.size() == 1, "single-frame cursor has one step");
        const CursorFrame& f = a.frames[0];
        require(f.width == 8 && f.height == 4 && f.hotX == 3 && f.hotY == 2, ".cur size and hotspot");
        require(px(f, 1, 0)[2] == 0xFF && px(f, 1, 0)[0] == 0 && px(f, 1, 0)[3] == 0x80, "BGRA to RGBA, top row");
        require(px(f, 0, 3)[3] == 0xFF, "bottom row alpha");
    }
    // 1 bpp with AND mask: transparent, opaque white, and an inverting pixel drawn black.
    {
        Bytes palette = { 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0 };
        Bytes image = dib(32, 32, 1, palette,
                          [](int x, int) { return x >= 16 ? 1u : 0u; },
                          [](int x, int y) { return y >= 16 && (x < 8 || x >= 24); });
        Bytes file = iconDir(2, 32, 32, 0, 0, image);
        CursorAnimation a;
        require(decodeCursorFile(file.data(), file.size(), a), "1 bpp .cur decodes");
        const CursorFrame& f = a.frames[0];
        require(px(f, 0, 0)[3] == 0xFF && px(f, 0, 0)[0] == 0, "AND 0 XOR 0: opaque black");
        require(px(f, 20, 0)[3] == 0xFF && px(f, 20, 0)[0] == 0xFF, "AND 0 XOR 1: opaque white");
        require(px(f, 0, 20)[3] == 0, "AND 1 XOR 0: transparent");
        require(px(f, 30, 20)[3] == 0xFF && px(f, 30, 20)[0] == 0, "AND 1 XOR 1: inverting pixel shown black");
    }
    // 4 bpp .ico: the hotspot is the centre.
    {
        Bytes palette(16 * 4, 0);
        palette[5 * 4 + 1] = 0xCC;                                      // index 5: green
        Bytes image = dib(16, 16, 4, palette, [](int x, int) { return x == 3 ? 5u : 0u; },
                          [](int, int) { return false; });
        Bytes file = iconDir(1, 16, 16, 1, 4, image);                  // planes 1, 4 bpp
        CursorAnimation a;
        require(decodeCursorFile(file.data(), file.size(), a), "4 bpp .ico decodes");
        require(a.frames[0].hotX == 8 && a.frames[0].hotY == 8, ".ico hotspot is the centre");
        require(px(a.frames[0], 3, 7)[1] == 0xCC && px(a.frames[0], 2, 7)[1] == 0, "4 bpp palette nibbles");
    }
    // Animated: three frames, five steps through seq, per-step rates, one odd-sized pad chunk.
    Bytes ani;
    {
        Bytes frames;
        putId(frames, "fram");
        for (int i = 0; i < 3; ++i) {
            const unsigned colour = 0xFF000000u | (0x40u * (i + 1));
            Bytes image = dib(4, 4, 32, Bytes(), [colour](int, int) { return colour; },
                              [](int, int) { return false; });
            chunk(frames, "icon", iconDir(2, 4, 4, i, 1, image));
        }
        Bytes anih;
        put32(anih, 36); put32(anih, 3); put32(anih, 5); put32(anih, 4); put32(anih, 4);
        put32(anih, 32); put32(anih, 1); put32(anih, 10); put32(anih, 3);   // AF_ICON | AF_SEQUENCE
        Bytes rate, seq;
        const unsigned rates[5] = { 200, 4, 0, 4, 4 }, order[5] = { 0, 1, 2, 1, 7 };
        for (int i = 0; i < 5; ++i) { put32(rate, rates[i]); put32(seq, order[i]); }
        Bytes body;
        putId(body, "ACON");
        chunk(body, "anih", anih);
        chunk(body, "INFx", Bytes(3, 'x'));                            // unknown, odd: padded
        chunk(body, "rate", rate);
        chunk(body, "seq ", seq);
        chunk(body, "LIST", frames);
        putId(ani, "RIFF"); put32(ani, static_cast<unsigned>(body.size()));
        ani.insert(ani.end(), body.begin(), body.end());
        CursorAnimation a;
        require(decodeCursorFile(ani.data(), ani.size(), a), "RIFF ACON decodes");
        require(a.frames.size() == 3 && a.steps.size() == 5, "frame and step counts");
        require(a.steps[2] == 2 && a.steps[3] == 1, "seq maps steps to frames");
        require(a.steps[4] == 0, "an out-of-range seq entry shows frame 0");
        require(a.jiffies[0] == 200 && a.jiffies[1] == 4 && a.jiffies[2] == 1, "rates, a zero rate clamped");
        require(a.frames[2].hotX == 2 && a.frames[2].hotY == 1, "per-frame hotspot");
        require(px(a.frames[1], 0, 0)[2] == 0x80, "frame pixels (blue)");
    }
    // Without AF_SEQUENCE the seq chunk is ignored and anih's rate is the default.
    {
        Bytes copy = ani;
        const size_t flags = 12 + 8 + 32;
        copy[flags] = 1;
        CursorAnimation a;
        require(decodeCursorFile(copy.data(), copy.size(), a), "ACON without AF_SEQUENCE decodes");
        require(a.steps[3] == 0 && a.steps[4] == 1, "steps loop over the frames in order");
    }
    // Truncations at every length and foreign data fail cleanly.
    for (size_t n = 0; n < ani.size(); ++n) {
        CursorAnimation a;
        if (decodeCursorFile(ani.data(), n, a))
            require(!a.frames.empty() && a.steps.size() == a.jiffies.size(), "a partial file decodes consistently");
        else
            require(a.frames.empty() && a.steps.empty(), "a failed decode leaves nothing");
    }
    {
        const std::string text = "RIFF....WAVEfmt ";
        CursorAnimation a;
        require(!decodeCursorFile(reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), a), "a WAVE file is not a cursor");
        require(!decodeCursorFile(nullptr, 0, a), "no data");
    }
    std::printf("cursor_file_check: OK\n");
    return 0;
}
