// Code/Tools/BF2Editor/Framework/TextureEncode.cpp - see TextureEncode.h. Editor-only code.
#include "Framework/EditorConfig.h"
#include "Framework/TextureEncode.h"

#include <cmath>
#include <cstring>

namespace bf2editor
{

namespace
{

void putU32(std::string& out, unsigned value)
{
    out.append(reinterpret_cast<const char*>(&value), 4);
}

// nvdxt's header: the linear size always set, the mip count and the complex caps only with a chain.
std::string ddsHeader(int width, int height, bool dxt1, unsigned linearSize, int mipLevels)
{
    std::string out("DDS ");
    putU32(out, 124);
    putU32(out, 0x1007u | 0x80000u | (mipLevels ? 0x20000u : 0u));
    putU32(out, static_cast<unsigned>(height));
    putU32(out, static_cast<unsigned>(width));
    putU32(out, linearSize);
    putU32(out, 0);                                     // depth
    putU32(out, static_cast<unsigned>(mipLevels));
    for (int i = 0; i < 11; ++i)
        putU32(out, 0);
    putU32(out, 32);
    putU32(out, dxt1 ? 0x4u : 0x40u);
    putU32(out, dxt1 ? 0x31545844u : 0u);               // "DXT1"
    putU32(out, dxt1 ? 0u : 16u);
    putU32(out, dxt1 ? 0u : 0xF800u);
    putU32(out, dxt1 ? 0u : 0x07E0u);
    putU32(out, dxt1 ? 0u : 0x001Fu);
    putU32(out, 0);
    putU32(out, mipLevels ? 0x401008u : 0x1000u);
    for (int i = 0; i < 4; ++i)
        putU32(out, 0);
    return out;
}

int clampInt(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
}

unsigned short pack565(const float c[3])
{
    const int r = clampInt(static_cast<int>(c[0] * 31.0f / 255.0f + 0.5f), 0, 31);
    const int g = clampInt(static_cast<int>(c[1] * 63.0f / 255.0f + 0.5f), 0, 63);
    const int b = clampInt(static_cast<int>(c[2] * 31.0f / 255.0f + 0.5f), 0, 31);
    return static_cast<unsigned short>((r << 11) | (g << 5) | b);
}

// The 8-bit channels a decoder expands a 565 colour to.
void expand565(unsigned short c, float out[3])
{
    const int r = (c >> 11) & 31;
    const int g = (c >> 5) & 63;
    const int b = c & 31;
    out[0] = static_cast<float>((r << 3) | (r >> 2));
    out[1] = static_cast<float>((g << 2) | (g >> 4));
    out[2] = static_cast<float>((b << 3) | (b >> 2));
}

// The four-colour palette of endpoints c0 > c1 and each texel's nearest entry; returns the squared error.
float fitIndices(const float px[16][3], unsigned short c0, unsigned short c1, unsigned char indices[16])
{
    float palette[4][3];
    expand565(c0, palette[0]);
    expand565(c1, palette[1]);
    for (int k = 0; k < 3; ++k)
    {
        palette[2][k] = (2.0f * palette[0][k] + palette[1][k]) / 3.0f;
        palette[3][k] = (palette[0][k] + 2.0f * palette[1][k]) / 3.0f;
    }
    float error = 0.0f;
    for (int i = 0; i < 16; ++i)
    {
        float best = 1e30f;
        for (int e = 0; e < 4; ++e)
        {
            float d = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                const float t = px[i][k] - palette[e][k];
                d += t * t;
            }
            if (d < best)
            {
                best = d;
                indices[i] = static_cast<unsigned char>(e);
            }
        }
        error += best;
    }
    return error;
}

// Endpoints that best reproduce the texels for the given indices (least squares, four-colour weights).
bool refineEndpoints(const float px[16][3], const unsigned char indices[16], float a[3], float b[3])
{
    static const float kWeight[4] = { 1.0f, 0.0f, 2.0f / 3.0f, 1.0f / 3.0f };
    float aa = 0.0f, ab = 0.0f, bb = 0.0f;
    float ap[3] = { 0.0f, 0.0f, 0.0f }, bp[3] = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 16; ++i)
    {
        const float w = kWeight[indices[i]];
        const float v = 1.0f - w;
        aa += w * w;
        ab += w * v;
        bb += v * v;
        for (int k = 0; k < 3; ++k)
        {
            ap[k] += w * px[i][k];
            bp[k] += v * px[i][k];
        }
    }
    const float determinant = aa * bb - ab * ab;
    if (std::fabs(determinant) < 1e-6f)
        return false;
    for (int k = 0; k < 3; ++k)
    {
        a[k] = (ap[k] * bb - bp[k] * ab) / determinant;
        b[k] = (bp[k] * aa - ap[k] * ab) / determinant;
    }
    return true;
}

void storeBlock(unsigned short c0, unsigned short c1, const unsigned char indices[16], unsigned char out[8])
{
    unsigned bits = 0;
    for (int i = 0; i < 16; ++i)
        bits |= static_cast<unsigned>(indices[i] & 3) << (2 * i);
    std::memcpy(out, &c0, 2);
    std::memcpy(out + 2, &c1, 2);
    std::memcpy(out + 4, &bits, 4);
}

// One 4 x 4 block of texels (0xAARRGGBB, rows top to bottom) as DXT1 colour.
void encodeBlock(const unsigned texels[16], unsigned char out[8])
{
    float px[16][3];
    float mean[3] = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 16; ++i)
    {
        px[i][0] = static_cast<float>((texels[i] >> 16) & 0xFF);
        px[i][1] = static_cast<float>((texels[i] >> 8) & 0xFF);
        px[i][2] = static_cast<float>(texels[i] & 0xFF);
        for (int k = 0; k < 3; ++k)
            mean[k] += px[i][k];
    }
    for (int k = 0; k < 3; ++k)
        mean[k] /= 16.0f;

    // The principal axis of the block's colours (power iteration on the covariance).
    float cov[3][3] = { { 0.0f } };
    for (int i = 0; i < 16; ++i)
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                cov[r][c] += (px[i][r] - mean[r]) * (px[i][c] - mean[c]);
    float axis[3] = { 1.0f, 1.0f, 1.0f };
    for (int iteration = 0; iteration < 8; ++iteration)
    {
        float next[3];
        for (int r = 0; r < 3; ++r)
            next[r] = cov[r][0] * axis[0] + cov[r][1] * axis[1] + cov[r][2] * axis[2];
        const float length = std::sqrt(next[0] * next[0] + next[1] * next[1] + next[2] * next[2]);
        if (length < 1e-6f)
            break;
        for (int r = 0; r < 3; ++r)
            axis[r] = next[r] / length;
    }
    float low = 0.0f, high = 0.0f;
    for (int i = 0; i < 16; ++i)
    {
        const float t = (px[i][0] - mean[0]) * axis[0] + (px[i][1] - mean[1]) * axis[1] + (px[i][2] - mean[2]) * axis[2];
        low = t < low ? t : low;
        high = t > high ? t : high;
    }
    float a[3], b[3];
    for (int k = 0; k < 3; ++k)
    {
        a[k] = mean[k] + axis[k] * high;
        b[k] = mean[k] + axis[k] * low;
    }

    unsigned short bestC0 = 0, bestC1 = 0;
    unsigned char bestIndices[16] = { 0 };
    float bestError = 1e30f;
    for (int pass = 0; pass < 3; ++pass)
    {
        unsigned short c0 = pack565(a);
        unsigned short c1 = pack565(b);
        if (c0 < c1)
        {
            const unsigned short t = c0;
            c0 = c1;
            c1 = t;
        }
        unsigned char indices[16];
        float error;
        if (c0 == c1)
        {
            // One colour: three-colour mode would make index 3 transparent, so every texel takes index 0.
            std::memset(indices, 0, sizeof(indices));
            float colour[3];
            expand565(c0, colour);
            error = 0.0f;
            for (int i = 0; i < 16; ++i)
                for (int k = 0; k < 3; ++k)
                    error += (px[i][k] - colour[k]) * (px[i][k] - colour[k]);
        }
        else
        {
            error = fitIndices(px, c0, c1, indices);
        }
        if (error < bestError)
        {
            bestError = error;
            bestC0 = c0;
            bestC1 = c1;
            std::memcpy(bestIndices, indices, sizeof(indices));
        }
        // The next pass takes the endpoints that best fit these indices: a for index 0, b for index 1.
        if (c0 == c1 || !refineEndpoints(px, indices, a, b))
            break;
    }
    storeBlock(bestC0, bestC1, bestIndices, out);
}

std::string dxt1Level(const DdsImage& image)
{
    const int blocksX = (image.width + 3) / 4;
    const int blocksY = (image.height + 3) / 4;
    std::string out(static_cast<size_t>(blocksX) * blocksY * 8, '\0');
    for (int by = 0; by < blocksY; ++by)
    {
        for (int bx = 0; bx < blocksX; ++bx)
        {
            unsigned texels[16];
            for (int y = 0; y < 4; ++y)
            {
                const int sy = by * 4 + y < image.height ? by * 4 + y : image.height - 1;
                for (int x = 0; x < 4; ++x)
                {
                    const int sx = bx * 4 + x < image.width ? bx * 4 + x : image.width - 1;
                    texels[y * 4 + x] = image.pixels[static_cast<size_t>(sy) * image.width + sx];
                }
            }
            encodeBlock(texels, reinterpret_cast<unsigned char*>(&out[(static_cast<size_t>(by) * blocksX + bx) * 8]));
        }
    }
    return out;
}

} // namespace

DdsImage halveBox(const DdsImage& image)
{
    DdsImage out;
    out.width = image.width > 1 ? image.width / 2 : 1;
    out.height = image.height > 1 ? image.height / 2 : 1;
    out.pixels.resize(static_cast<size_t>(out.width) * out.height);
    for (int y = 0; y < out.height; ++y)
    {
        for (int x = 0; x < out.width; ++x)
        {
            const int x0 = image.width > 1 ? 2 * x : 0;
            const int y0 = image.height > 1 ? 2 * y : 0;
            const int x1 = image.width > 1 ? x0 + 1 : x0;
            const int y1 = image.height > 1 ? y0 + 1 : y0;
            const unsigned t[4] = { image.pixels[static_cast<size_t>(y0) * image.width + x0],
                                    image.pixels[static_cast<size_t>(y0) * image.width + x1],
                                    image.pixels[static_cast<size_t>(y1) * image.width + x0],
                                    image.pixels[static_cast<size_t>(y1) * image.width + x1] };
            unsigned value = 0;
            for (int shift = 0; shift < 32; shift += 8)
            {
                unsigned sum = 0;
                for (int k = 0; k < 4; ++k)
                    sum += (t[k] >> shift) & 0xFF;
                value |= ((sum + 2) / 4) << shift;
            }
            out.pixels[static_cast<size_t>(y) * out.width + x] = value;
        }
    }
    return out;
}

std::string encodeDxt1(const DdsImage& image, bool mipmaps)
{
    int levels = 1;
    if (mipmaps)
        for (int w = image.width, h = image.height; w > 1 || h > 1; w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1)
            ++levels;
    std::string out = ddsHeader(image.width, image.height, true,
                                static_cast<unsigned>(((image.width + 3) / 4) * ((image.height + 3) / 4) * 8),
                                mipmaps ? levels : 0);
    DdsImage level = image;
    for (int i = 0; i < levels; ++i)
    {
        out += dxt1Level(level);
        if (i + 1 < levels)
            level = halveBox(level);
    }
    return out;
}

std::string encodeR5G6B5(const DdsImage& image)
{
    std::string out = ddsHeader(image.width, image.height, false, static_cast<unsigned>(image.width * image.height * 2), 0);
    out.reserve(out.size() + image.pixels.size() * 2);
    for (size_t i = 0; i < image.pixels.size(); ++i)
    {
        const unsigned p = image.pixels[i];
        const float c[3] = { static_cast<float>((p >> 16) & 0xFF), static_cast<float>((p >> 8) & 0xFF),
                             static_cast<float>(p & 0xFF) };
        const unsigned short v = pack565(c);
        out.append(reinterpret_cast<const char*>(&v), 2);
    }
    return out;
}

} // namespace bf2editor
