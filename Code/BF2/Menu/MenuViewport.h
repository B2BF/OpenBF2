#pragma once

#include <cstdint>

namespace dice { namespace hfe { namespace ui {

// [CHOSEN, not recovered] The Flash menus occupy a centered 4:3 rectangle (#45).
// Coordinates and dimensions are in drawable pixels. The right and bottom edges are exclusive.
struct MenuViewport
{
    unsigned x, y, width, height;

    static MenuViewport fit(unsigned outputWidth, unsigned outputHeight)
    {
        unsigned w = outputWidth;
        unsigned h = outputHeight;
        if (std::uint64_t(w) * 3 > std::uint64_t(h) * 4)
            w = static_cast<unsigned>(std::uint64_t(h) * 4 / 3);
        else
            h = static_cast<unsigned>(std::uint64_t(w) * 3 / 4);
        return {(outputWidth - w) / 2, (outputHeight - h) / 2, w, h};
    }
};

} } }
