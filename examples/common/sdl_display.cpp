#include "sdl_display.hpp"
#include "frame_layout.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace demo {
namespace {

std::runtime_error sdlError(const char* operation) {
    return std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

}  // namespace

SdlDisplay::SdlDisplay(const char* title, int width, int height, int scale,
                       bool fullscreen)
    : width_(width), height_(height), fullscreen_(fullscreen) {
    if (width <= 0 || height <= 0 || scale <= 0) {
        throw std::invalid_argument("Invalid SDL display dimensions");
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw sdlError("SDL_Init");
    }
    try {
        const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE |
                                      (fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
        if (!SDL_CreateWindowAndRenderer(title, width * scale, height * scale,
                                         flags, &window_, &renderer_)) {
            throw sdlError("SDL_CreateWindowAndRenderer");
        }
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!texture_) {
            throw sdlError("SDL_CreateTexture");
        }
        if (!SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST)) {
            throw sdlError("SDL_SetTextureScaleMode");
        }
    } catch (...) {
        release();
        SDL_Quit();
        throw;
    }
}

SdlDisplay::~SdlDisplay() {
    release();
    SDL_Quit();
}

void SdlDisplay::release() noexcept {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

void SdlDisplay::present(const std::vector<std::uint8_t>& rgba) {
    const auto expected = static_cast<std::size_t>(width_) * height_ * 4;
    if (rgba.size() != expected) {
        throw std::invalid_argument("Invalid RGBA buffer length");
    }
    if (!SDL_UpdateTexture(texture_, nullptr, rgba.data(), width_ * 4)) {
        throw sdlError("SDL_UpdateTexture");
    }
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer_, &output_width, &output_height)) {
        throw sdlError("SDL_GetRenderOutputSize");
    }
    if (output_width <= 0 || output_height <= 0) {
        // Minimized window; no visible output is available yet.
        return;
    }
    // Compute a fresh letterbox on every frame: fullscreen, DPI changes, and
    // window resizing must never crop the logical framebuffer.
    const auto viewport = fitViewport(width_, height_, output_width, output_height);
    const SDL_FRect destination{viewport.x, viewport.y,
                                viewport.width, viewport.height};
    if (!SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255) ||
        !SDL_RenderClear(renderer_) ||
        !SDL_RenderTexture(renderer_, texture_, nullptr, &destination) ||
        !SDL_RenderPresent(renderer_)) {
        throw sdlError("SDL_Render");
    }
}

void SdlDisplay::toggleFullscreen() {
    if (!SDL_SetWindowFullscreen(window_, !fullscreen_)) {
        throw sdlError("SDL_SetWindowFullscreen");
    }
    fullscreen_ = !fullscreen_;
}

}  // namespace demo
