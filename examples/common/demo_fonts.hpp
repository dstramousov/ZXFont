#ifndef ZXFONT_EXAMPLES_DEMO_FONTS_HPP
#define ZXFONT_EXAMPLES_DEMO_FONTS_HPP

#include <SDL3/SDL.h>

#include <filesystem>

namespace demo {

// The compact font is a runtime asset copied alongside the executables.
// A missing asset is handled by Context::setFont(path) with built-in fallback.
inline std::filesystem::path compactFontPath() {
    const char* base_path = SDL_GetBasePath();
    return base_path ? std::filesystem::path(base_path) / "fonts" /
                           "zx-compact-airy-6x8.psf"
                     : std::filesystem::path{};
}

inline const char* fontName(bool compact) noexcept {
    return compact ? "COMPACT AIRY 6X8" : "SPECTRUM 8X8";
}

}  // namespace demo

#endif  // ZXFONT_EXAMPLES_DEMO_FONTS_HPP
