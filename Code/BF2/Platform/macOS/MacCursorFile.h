// [CHOSEN, not recovered] macOS port: decoder for the Windows cursor files the menu loads.
//
// Retail SwiffHost::getCursor (0x488960) loads menu/external/flashmenu/cursor/cursor_arrow.ani and
// cursor_busy.ani with LoadCursorFromFileA (IAT 0x87F85C) and cursor_keybinding.ani with
// LoadImageA(GetModuleHandleA(0), path, IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE | LR_SHARED)
// (IAT 0x87F860). AppKit reads neither .ani nor .cur, so the port decodes them itself: an animated
// cursor (RIFF "ACON": anih, optional rate and seq, LIST "fram" of icon chunks) or a single .cur /
// .ico image. Each frame becomes non-premultiplied RGBA, top row first, with its hotspot.
// Header-only and free of AppKit, so a standalone check can include it. Nothing here reads the
// file system; CocoaHost.mm reads the user's own game file and hands the bytes over.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace dice {
namespace hfe {
namespace macos {

struct CursorFrame
{
    int width = 0;
    int height = 0;
    int hotX = 0;                       // from the top-left corner, in frame pixels
    int hotY = 0;
    std::vector<std::uint8_t> rgba;     // width * height * 4, non-premultiplied, top row first;
                                        // empty when the frame is PNG-compressed (see png)
    std::vector<std::uint8_t> png;      // a Vista-style PNG frame, decoded by the caller
};

struct CursorAnimation
{
    std::vector<CursorFrame> frames;
    std::vector<int> steps;             // frame index per display step
    std::vector<int> jiffies;           // display time per step, in 1/60 s (always >= 1)
};

namespace cursorfile_detail {

inline std::uint16_t u16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
inline std::uint32_t u32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

const int kMaxDimension = 256;
const std::size_t kMaxFrames = 1024;

// A BITMAPINFOHEADER DIB as icon resources store it: the XOR image, then the 1 bpp AND mask, both
// bottom-up, with the header height doubled. Supports BI_RGB at 1, 4, 8, 24 and 32 bpp.
inline bool decodeDib(const std::uint8_t* p, std::size_t n, CursorFrame& f)
{
    if (n < 40)
        return false;
    const std::uint32_t headerSize = u32(p);
    const std::int32_t width = static_cast<std::int32_t>(u32(p + 4));
    const std::int32_t height2 = static_cast<std::int32_t>(u32(p + 8));
    const int bpp = u16(p + 14);
    const std::uint32_t compression = u32(p + 16);
    std::uint32_t colours = u32(p + 32);
    if (headerSize < 40 || headerSize > n || width <= 0 || width > kMaxDimension ||
        height2 <= 0 || (height2 & 1) || height2 / 2 > kMaxDimension || compression != 0)
        return false;
    const int height = height2 / 2;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)
        return false;
    std::size_t paletteEntries = 0;
    if (bpp <= 8)
    {
        if (colours == 0 || colours > (1u << bpp))
            colours = 1u << bpp;
        paletteEntries = colours;
    }
    const std::size_t paletteOffset = headerSize;
    const std::size_t xorOffset = paletteOffset + paletteEntries * 4;
    const std::size_t xorStride = ((static_cast<std::size_t>(width) * bpp + 31) / 32) * 4;
    const std::size_t andOffset = xorOffset + xorStride * height;
    const std::size_t andStride = ((static_cast<std::size_t>(width) + 31) / 32) * 4;
    if (andOffset > n)
        return false;
    // Some writers omit the mask of a 32 bpp image; the alpha channel alone then decides.
    const bool hasMask = andOffset + andStride * height <= n;
    if (!hasMask && bpp != 32)
        return false;

    f.width = width;
    f.height = height;
    f.rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
    bool anyAlpha = false;
    for (int y = 0; y < height; ++y)
    {
        const std::uint8_t* row = p + xorOffset + xorStride * (height - 1 - y);
        std::uint8_t* out = &f.rgba[static_cast<std::size_t>(y) * width * 4];
        for (int x = 0; x < width; ++x, out += 4)
        {
            std::uint8_t b = 0, g = 0, r = 0, a = 255;
            if (bpp == 32)
            {
                b = row[x * 4]; g = row[x * 4 + 1]; r = row[x * 4 + 2]; a = row[x * 4 + 3];
                if (a) anyAlpha = true;
            }
            else if (bpp == 24)
            {
                b = row[x * 3]; g = row[x * 3 + 1]; r = row[x * 3 + 2];
            }
            else
            {
                unsigned index;
                if (bpp == 8)      index = row[x];
                else if (bpp == 4) index = (row[x / 2] >> ((x & 1) ? 0 : 4)) & 0x0F;
                else               index = (row[x / 8] >> (7 - (x & 7))) & 1;
                if (index >= paletteEntries)
                    index = 0;
                const std::uint8_t* c = p + paletteOffset + index * 4;
                b = c[0]; g = c[1]; r = c[2];
            }
            out[0] = r; out[1] = g; out[2] = b; out[3] = a;
        }
    }
    if (bpp == 32 && anyAlpha)
        return true;                    // per-pixel alpha; the mask is ignored, as Windows does
    if (!hasMask)
    {
        for (std::size_t i = 3; i < f.rgba.size(); i += 4)
            f.rgba[i] = 255;            // no alpha and no mask: opaque
        return true;
    }
    for (int y = 0; y < height; ++y)
    {
        const std::uint8_t* row = p + andOffset + andStride * (height - 1 - y);
        std::uint8_t* out = &f.rgba[static_cast<std::size_t>(y) * width * 4];
        for (int x = 0; x < width; ++x, out += 4)
        {
            const bool transparent = (row[x / 8] >> (7 - (x & 7))) & 1;
            if (!transparent)
                out[3] = 255;
            else if (out[0] | out[1] | out[2])
            {
                // AND 1 with a non-black XOR pixel inverts the screen on Windows; AppKit cannot
                // invert, so such pixels are drawn opaque black.
                out[0] = out[1] = out[2] = 0;
                out[3] = 255;
            }
            else
                out[3] = 0;
        }
    }
    return true;
}

// An ICONDIR file (.ico type 1, .cur type 2). The largest image wins, then the deepest.
inline bool decodeIconDir(const std::uint8_t* p, std::size_t n, CursorFrame& f)
{
    if (n < 6 || u16(p) != 0)
        return false;
    const int type = u16(p + 2);
    const std::size_t count = u16(p + 4);
    if ((type != 1 && type != 2) || count == 0 || 6 + count * 16 > n)
        return false;
    std::size_t best = count;
    long bestScore = -1;
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint8_t* e = p + 6 + i * 16;
        const int w = e[0] ? e[0] : 256;
        const int h = e[1] ? e[1] : 256;
        const int depth = type == 1 ? u16(e + 6) : 0;
        const long score = static_cast<long>(w) * h * 64 + depth;
        if (score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    const std::uint8_t* e = p + 6 + best * 16;
    const std::uint32_t size = u32(e + 8);
    const std::uint32_t offset = u32(e + 12);
    if (offset >= n || size > n - offset || size < 8)
        return false;
    const std::uint8_t* image = p + offset;
    static const std::uint8_t pngSignature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (std::memcmp(image, pngSignature, 8) == 0)
    {
        f.png.assign(image, image + size);
        f.width = e[0] ? e[0] : 256;
        f.height = e[1] ? e[1] : 256;
    }
    else if (!decodeDib(image, size, f))
        return false;
    if (type == 2)
    {
        f.hotX = u16(e + 4);            // a cursor's entry keeps its hotspot in planes/bitcount
        f.hotY = u16(e + 6);
    }
    else
    {
        f.hotX = f.width / 2;           // an icon used as a cursor: the centre
        f.hotY = f.height / 2;
    }
    if (f.hotX >= f.width)  f.hotX = f.width - 1;
    if (f.hotY >= f.height) f.hotY = f.height - 1;
    return true;
}

inline bool decodeAcon(const std::uint8_t* p, std::size_t n, CursorAnimation& out)
{
    const std::size_t riffSize = u32(p + 4);
    const std::size_t end = riffSize + 8 < n ? riffSize + 8 : n;
    std::uint32_t frameCount = 0, stepCount = 0, defaultRate = 0, flags = 0;
    bool haveHeader = false;
    std::vector<int> rates, sequence;
    std::size_t at = 12;
    while (at + 8 <= end)
    {
        const std::uint8_t* id = p + at;
        const std::size_t size = u32(p + at + 4);
        const std::size_t body = at + 8;
        if (size > end - body)
            return false;
        if (std::memcmp(id, "anih", 4) == 0 && size >= 36)
        {
            frameCount = u32(p + body + 4);
            stepCount = u32(p + body + 8);
            defaultRate = u32(p + body + 28);
            flags = u32(p + body + 32);
            haveHeader = true;
        }
        else if (std::memcmp(id, "rate", 4) == 0 || std::memcmp(id, "seq ", 4) == 0)
        {
            std::vector<int>& list = id[0] == 'r' ? rates : sequence;
            list.clear();
            for (std::size_t i = 0; i + 4 <= size && list.size() < kMaxFrames; i += 4)
                list.push_back(static_cast<int>(u32(p + body + i)));
        }
        else if (std::memcmp(id, "LIST", 4) == 0 && size >= 4 && std::memcmp(p + body, "fram", 4) == 0)
        {
            std::size_t sub = body + 4;
            const std::size_t listEnd = body + size;
            while (sub + 8 <= listEnd)
            {
                const std::size_t subSize = u32(p + sub + 4);
                if (subSize > listEnd - (sub + 8))
                    return false;
                if (std::memcmp(p + sub, "icon", 4) == 0)
                {
                    if (out.frames.size() >= kMaxFrames)
                        return false;
                    CursorFrame f;
                    if (!decodeIconDir(p + sub + 8, subSize, f))
                        return false;
                    out.frames.push_back(f);
                }
                sub += 8 + subSize + (subSize & 1);
            }
        }
        at = body + size + (size & 1);
    }
    // AF_ICON (bit 0) marks icon/cursor frames; raw-DIB frames are not supported.
    if (!haveHeader || !(flags & 1) || out.frames.empty())
        return false;
    if (frameCount == 0 || frameCount > out.frames.size())
        frameCount = static_cast<std::uint32_t>(out.frames.size());
    out.frames.resize(frameCount);
    if (stepCount == 0)
        stepCount = (flags & 2) && !sequence.empty() ? static_cast<std::uint32_t>(sequence.size()) : frameCount;
    if (stepCount > kMaxFrames)
        stepCount = kMaxFrames;
    for (std::uint32_t i = 0; i < stepCount; ++i)
    {
        int frame = (flags & 2) && i < sequence.size() ? sequence[i] : static_cast<int>(i % frameCount);
        if (frame < 0 || static_cast<std::uint32_t>(frame) >= frameCount)
            frame = 0;
        int rate = i < rates.size() ? rates[i] : static_cast<int>(defaultRate);
        out.steps.push_back(frame);
        out.jiffies.push_back(rate > 0 ? rate : 1);
    }
    return true;
}

} // namespace cursorfile_detail

// Decodes a .ani, .cur or .ico file image. False (and out cleared) when the data is not one.
inline bool decodeCursorFile(const std::uint8_t* data, std::size_t size, CursorAnimation& out)
{
    out = CursorAnimation();
    bool ok = false;
    if (data && size >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "ACON", 4) == 0)
        ok = cursorfile_detail::decodeAcon(data, size, out);
    else if (data)
    {
        CursorFrame f;
        if (cursorfile_detail::decodeIconDir(data, size, f))
        {
            out.frames.push_back(f);
            out.steps.push_back(0);
            out.jiffies.push_back(1);
            ok = true;
        }
    }
    if (!ok)
        out = CursorAnimation();
    return ok;
}

} // namespace macos
} // namespace hfe
} // namespace dice
