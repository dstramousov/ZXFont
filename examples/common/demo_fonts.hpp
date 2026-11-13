#ifndef ZXFONT_EXAMPLES_DEMO_FONTS_HPP
#define ZXFONT_EXAMPLES_DEMO_FONTS_HPP

#include "zxfont/zxfont.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <stdexcept>

namespace demo {

// The demo font is copied next to the executables by CMake. Neither the
// host's current working directory nor its source checkout is required.
inline zx::Font loadSpectrumFont() {
    const char* base_path = SDL_GetBasePath();
    if (!base_path) {
        throw std::runtime_error("SDL_GetBasePath failed");
    }
    return zx::Font::loadPsf2(std::filesystem::path(base_path) / "fonts" /
                              "zx-spectrum-unicode-8x8.psf");
}

inline const char* fontName(bool external) noexcept {
    return external ? "ZX UNICODE / PSF2" : "ZX UNICODE / BUILTIN";
}

}  // namespace demo

#endif  // ZXFONT_EXAMPLES_DEMO_FONTS_HPP
