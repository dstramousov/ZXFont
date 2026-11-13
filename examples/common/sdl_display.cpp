#include "sdl_display.hpp"

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
        // Fullscreen terminals fill the display; windowed game pixels use
        // integer scaling to keep their pixel grid sharp.
        const auto presentation = fullscreen ? SDL_LOGICAL_PRESENTATION_LETTERBOX
                                             : SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
        if (!SDL_SetRenderLogicalPresentation(renderer_, width, height,
                                               presentation)) {
            throw sdlError("SDL_SetRenderLogicalPresentation");
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
    if (!SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255) ||
        !SDL_RenderClear(renderer_) ||
        !SDL_RenderTexture(renderer_, texture_, nullptr, nullptr) ||
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
