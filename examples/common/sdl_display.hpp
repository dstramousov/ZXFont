#ifndef ZXFONT_EXAMPLES_SDL_DISPLAY_HPP
#define ZXFONT_EXAMPLES_SDL_DISPLAY_HPP

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace demo {

// Owns an SDL3 window, renderer, and streaming texture for RGBA8 frames.
// This adapter is intentionally outside the public ZXFont library.
class SdlDisplay {
public:
    SdlDisplay(const char* title, int width, int height, int scale,
               bool fullscreen);
    ~SdlDisplay();

    SdlDisplay(const SdlDisplay&) = delete;
    SdlDisplay& operator=(const SdlDisplay&) = delete;

    void present(const std::vector<std::uint8_t>& rgba);
    void toggleFullscreen();

private:
    void release() noexcept;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int width_;
    int height_;
    bool fullscreen_;
};

}  // namespace demo

#endif  // ZXFONT_EXAMPLES_SDL_DISPLAY_HPP
