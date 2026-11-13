#include "zxfont/zxfont.hpp"

#include <array>
#include <stdexcept>

namespace zx {
namespace {

using Colors = std::array<Color, 16>;

// The indices for ZX Spectrum and VGA follow their native 16-color order.
constexpr Colors kMonoGreen{{
    {2, 9, 5}, {5, 20, 10}, {7, 32, 16}, {9, 45, 20},
    {11, 58, 25}, {13, 73, 33}, {17, 89, 41}, {19, 105, 48},
    {24, 122, 56}, {31, 139, 65}, {41, 163, 77}, {57, 184, 90},
    {78, 206, 106}, {105, 223, 130}, {147, 239, 166}, {205, 255, 211}
}};

constexpr Colors kMonoAmber{{
    {10, 7, 2}, {23, 13, 3}, {36, 20, 4}, {52, 28, 5},
    {67, 36, 5}, {83, 44, 6}, {99, 55, 8}, {117, 67, 11},
    {137, 79, 15}, {158, 94, 20}, {182, 112, 28}, {204, 132, 40},
    {224, 155, 57}, {240, 182, 84}, {250, 210, 123}, {255, 235, 179}
}};

constexpr Colors kSpectrum{{
    {0, 0, 0}, {0, 0, 205}, {205, 0, 0}, {205, 0, 205},
    {0, 205, 0}, {0, 205, 205}, {205, 205, 0}, {205, 205, 205},
    {0, 0, 0}, {0, 0, 255}, {255, 0, 0}, {255, 0, 255},
    {0, 255, 0}, {0, 255, 255}, {255, 255, 0}, {255, 255, 255}
}};

constexpr Colors kVga{{
    {0, 0, 0}, {0, 0, 170}, {0, 170, 0}, {0, 170, 170},
    {170, 0, 0}, {170, 0, 170}, {170, 85, 0}, {170, 170, 170},
    {85, 85, 85}, {85, 85, 255}, {85, 255, 85}, {85, 255, 255},
    {255, 85, 85}, {255, 85, 255}, {255, 255, 85}, {255, 255, 255}
}};

}  // namespace

Color paletteColor(Palette palette, ColorIndex color) {
    if (color.value >= 16) {
        throw std::out_of_range("Palette color index must be in [0, 15]");
    }
    switch (palette) {
        case Palette::MonoGreen: return kMonoGreen[color.value];
        case Palette::MonoAmber: return kMonoAmber[color.value];
        case Palette::Spectrum: return kSpectrum[color.value];
        case Palette::VGA: return kVga[color.value];
    }
    throw std::invalid_argument("Unknown palette");
}

}  // namespace zx
