// Code/Tools/BF2Editor/Framework/TerrainLowDetail.cpp - see TerrainLowDetail.h. Editor-only code.
#include "Framework/EditorConfig.h"
#include "Framework/TerrainLowDetail.h"

#include <cstdlib>

namespace bf2editor
{

DdsImage buildLowDetailMap(const DdsImage* const detail[2], const int types[6], int size)
{
    DdsImage out;
    out.width = size > 0 ? size : 0;
    out.height = out.width;
    out.pixels.assign(static_cast<size_t>(out.width) * out.width, 0u);
    for (int layer = 0; layer < 6; ++layer)
    {
        const DdsImage* const map = detail[layer / 3];
        if ((types[layer] != 1 && types[layer] != 2) || !map || map->width != out.width || map->height != out.height
            || map->pixels.size() < out.pixels.size())
            continue;
        const unsigned shiftIn = 8u * static_cast<unsigned>(layer % 3);
        const unsigned shiftOut = types[layer] == 2 ? 0u : 16u;
        for (int y = 0; y < out.height; ++y)
        {
            const unsigned* const source = &map->pixels[static_cast<size_t>(y) * map->width];
            unsigned* const target = &out.pixels[static_cast<size_t>(y) * out.width];
            for (int x = 0; x < out.width; ++x)
            {
                unsigned sum = ((source[x] >> shiftIn) & 255u) + ((target[x] >> shiftOut) & 255u);
                if (sum > 255u)
                    sum = 255u;
                target[x] = (sum << shiftOut) | (target[x] & ~(255u << shiftOut));
            }
        }
    }
    return out;
}

bool inferLowDetailTypes(const std::vector<LowDetailSample>& samples, int types[6], float& error)
{
    // The compared texels, eight bytes each: the six layers' weights, then the low-detail map's red and blue.
    std::vector<unsigned char> texels;
    for (size_t s = 0; s < samples.size(); ++s)
    {
        const LowDetailSample& sample = samples[s];
        const DdsImage* const low = sample.lowDetail;
        if (!low || low->width <= 0 || low->height <= 0 || (!sample.detail[0] && !sample.detail[1])
            || low->pixels.size() < static_cast<size_t>(low->width) * low->height)
            continue;
        DdsImage scaled[2];
        const DdsImage* maps[2] = { 0, 0 };
        for (int k = 0; k < 2; ++k)
        {
            const DdsImage* const map = sample.detail[k];
            if (!map || map->width <= 0 || map->height <= 0)
                continue;
            if (map->width == low->width && map->height == low->height)
            {
                maps[k] = map;
            }
            else
            {
                scaled[k] = map->scaled(low->width, low->height);
                maps[k] = &scaled[k];
            }
        }
        const int stepX = low->width > 64 ? low->width / 64 : 1;
        const int stepY = low->height > 64 ? low->height / 64 : 1;
        for (int y = 0; y < low->height; y += stepY)
        {
            for (int x = 0; x < low->width; x += stepX)
            {
                const size_t t = static_cast<size_t>(y) * low->width + x;
                for (int layer = 0; layer < 6; ++layer)
                {
                    const DdsImage* const map = maps[layer / 3];
                    texels.push_back(map ? static_cast<unsigned char>(map->pixels[t] >> (8 * (layer % 3))) : 0);
                }
                texels.push_back(static_cast<unsigned char>(low->pixels[t] >> 16));
                texels.push_back(static_cast<unsigned char>(low->pixels[t]));
            }
        }
    }
    const size_t count = texels.size() / 8;
    if (!count)
        return false;

    // Each channel's summed error for every set of layers added into it (bit l: layer l).
    double channelError[2][64];
    for (int set = 0; set < 64; ++set)
    {
        double red = 0.0, blue = 0.0;
        for (size_t i = 0; i < count; ++i)
        {
            const unsigned char* const t = &texels[i * 8];
            int sum = 0;
            for (int layer = 0; layer < 6; ++layer)
                if (set & (1 << layer))
                    sum += t[layer];
            if (sum > 255)
                sum = 255;
            red += std::abs(sum - static_cast<int>(t[6]));
            blue += std::abs(sum - static_cast<int>(t[7]));
        }
        channelError[0][set] = red;
        channelError[1][set] = blue;
    }
    const auto layersIn = [](int set) {
        int n = 0;
        for (int layer = 0; layer < 6; ++layer)
            n += (set >> layer) & 1;
        return n;
    };
    int bestRed = 0, bestBlue = 0;
    double best = channelError[0][0] + channelError[1][0];
    for (int red = 0; red < 64; ++red)
    {
        for (int blue = 0; blue < 64; ++blue)
        {
            if (red & blue)
                continue;
            const double e = channelError[0][red] + channelError[1][blue];
            if (e < best || (e == best && layersIn(red | blue) < layersIn(bestRed | bestBlue)))
            {
                best = e;
                bestRed = red;
                bestBlue = blue;
            }
        }
    }
    for (int layer = 0; layer < 6; ++layer)
        types[layer] = (bestRed & (1 << layer)) ? 1 : (bestBlue & (1 << layer)) ? 2 : 0;
    error = static_cast<float>(best / (2.0 * static_cast<double>(count)));
    return true;
}

} // namespace bf2editor
