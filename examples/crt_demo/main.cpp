#include <zxcrt/zxcrt.hpp>
#include <zxfont/zxfont.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 400;

zxcrt::GLProcedure loadGL(const char* name) {
    return reinterpret_cast<zxcrt::GLProcedure>(SDL_GL_GetProcAddress(name));
}

std::runtime_error sdlError(const char* operation) {
    return std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

void pixel(std::vector<std::uint8_t>& image, int x, int y,
           std::array<std::uint8_t, 3> rgb) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    const std::size_t offset = (static_cast<std::size_t>(y) * kWidth + x) * 4;
    image[offset] = rgb[0];
    image[offset + 1] = rgb[1];
    image[offset + 2] = rgb[2];
    image[offset + 3] = 255;
}

std::vector<std::uint8_t> makeTestCard(bool amber) {
    std::vector<std::uint8_t> image(static_cast<std::size_t>(kWidth) * kHeight * 4);
    const auto foreground = amber ? std::array<std::uint8_t, 3>{246, 177, 64}
                                  : std::array<std::uint8_t, 3>{71, 231, 112};
    const auto subdued = amber ? std::array<std::uint8_t, 3>{58, 40, 21}
                               : std::array<std::uint8_t, 3>{19, 62, 36};
    // Pixel-perfect diagnostic grid: distortion is visible at every crossing.
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            pixel(image, x, y, {2, 4, 3});
            if (x % 32 == 0 || y % 32 == 0) pixel(image, x, y, subdued);
        }
    }
    for (int x = 0; x < kWidth; ++x) {
        pixel(image, x, kHeight / 2, foreground);
    }
    for (int y = 0; y < kHeight; ++y) {
        pixel(image, kWidth / 2, y, foreground);
    }

    zx::Context text(kWidth, kHeight);
    text.setPalette(amber ? zx::Palette::MonoAmber : zx::Palette::MonoGreen);
    constexpr zx::ColorIndex bright{15};
    constexpr zx::ColorIndex normal{12};
    text.box(16, 16, 608, 368, zx::Border::Double, bright);
    text.text(48, 40, "ZXCRT  /  PHOSPHOR MONITOR TEST", bright);
    text.text(48, 64, "CURVATURE    GLOW    SCANLINES    VIGNETTE", normal);
    text.line(32, 88, 576, zx::Axis::Horizontal, zx::Border::Single, normal);
    text.text(48, 112, "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789", bright);
    text.text(48, 136, "abcdefghijklmnopqrstuvwxyz !?.,:;+-=*", normal);
    text.text(48, 160, "ПРИВЕТ, КАЯ!  УКРАЇНСЬКА", bright);
    text.text(48, 216, "STRAIGHT GRID + COLOR REFERENCE", normal);
    // Red, green, blue, grayscale: a CRT effect must not force monochrome.
    const std::array<std::array<std::uint8_t, 3>, 6> bars{{
        {{240, 40, 40}}, {{40, 220, 60}}, {{60, 95, 240}},
        {{230, 210, 50}}, {{180, 90, 200}}, {{240, 240, 240}}}};
    for (std::size_t index = 0; index < bars.size(); ++index) {
        const int beginX = 48 + static_cast<int>(index) * 88;
        for (int y = 256; y < 280; ++y) {
            for (int x = beginX; x < beginX + 80; ++x) {
                pixel(image, x, y, bars[index]);
            }
        }
    }
    text.text(48, 312, "F1 ON/OFF  F2 CURVE  F3 GLOW  F4 SCAN", bright);
    text.text(48, 336, "F5 VIGNETTE  F6 AMBER  F11 FULL  ESC", normal);
    text.render({std::span<std::uint8_t>(image), kWidth, kHeight, kWidth * 4});
    return image;
}

float nextLevel(float value, float step, float maximum) {
    return value + step > maximum + 0.0001f ? 0.0f : value + step;
}

std::string title(const zxcrt::Settings& settings, bool amber) {
    return "ZXCRT v0.0.18 | " + std::string(settings.enabled ? "ON" : "OFF") +
           " | curvature " + std::to_string(settings.curvature).substr(0, 4) +
           " | glow " + std::to_string(settings.glow).substr(0, 4) +
           " | scan " + std::to_string(settings.scanlines).substr(0, 4) +
           " | vignette " + std::to_string(settings.vignette).substr(0, 4) +
           (amber ? " | AMBER" : " | GREEN");
}

}  // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "CRT demo: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    try {
        if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) ||
            !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
            throw sdlError("SDL_GL_SetAttribute");
        }
        window = SDL_CreateWindow("ZXCRT test", 960, 600,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window) throw sdlError("SDL_CreateWindow");
        glContext = SDL_GL_CreateContext(window);
        if (!glContext) throw sdlError("SDL_GL_CreateContext");
        if (!SDL_GL_SetSwapInterval(1)) {
            std::cerr << "CRT demo: VSync unavailable: " << SDL_GetError() << '\n';
        }
        {
            zxcrt::Renderer crt(loadGL);
            bool amber = false;
            auto image = makeTestCard(amber);
            auto settings = crt.settings();
            SDL_SetWindowTitle(window, title(settings, amber).c_str());
            bool running = true;
            bool fullscreen = false;
            while (running) {
                SDL_Event event;
                bool changed = false;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_QUIT) running = false;
                    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
                    switch (event.key.key) {
                        case SDLK_ESCAPE: running = false; break;
                        case SDLK_F1: settings.enabled = !settings.enabled; changed = true; break;
                        case SDLK_F2:
                            settings.curvature = nextLevel(settings.curvature, 0.025f, 0.3f);
                            changed = true;
                            break;
                        case SDLK_F3:
                            settings.glow = nextLevel(settings.glow, 0.1f, 1.0f);
                            changed = true;
                            break;
                        case SDLK_F4:
                            settings.scanlines = nextLevel(settings.scanlines, 0.1f, 1.0f);
                            changed = true;
                            break;
                        case SDLK_F5:
                            settings.vignette = nextLevel(settings.vignette, 0.1f, 1.0f);
                            changed = true;
                            break;
                        case SDLK_F6:
                            amber = !amber;
                            image = makeTestCard(amber);
                            changed = true;
                            break;
                        case SDLK_F11:
                            if (SDL_SetWindowFullscreen(window, !fullscreen)) {
                                fullscreen = !fullscreen;
                            } else {
                                std::cerr << SDL_GetError() << '\n';
                            }
                            break;
                        default: break;
                    }
                }
                if (!running) break;
                if (changed) {
                    crt.setSettings(settings);
                    SDL_SetWindowTitle(window, title(settings, amber).c_str());
                }
                int outputWidth = 0;
                int outputHeight = 0;
                if (!SDL_GetWindowSizeInPixels(window, &outputWidth, &outputHeight)) {
                    throw sdlError("SDL_GetWindowSizeInPixels");
                }
                if (outputWidth > 0 && outputHeight > 0) {
                    crt.render(image, kWidth, kHeight, outputWidth, outputHeight);
                    if (!SDL_GL_SwapWindow(window)) throw sdlError("SDL_GL_SwapWindow");
                } else {
                    SDL_Delay(16);
                }
            }
        }  // Destroy CRT's GL resources before destroying the SDL context.
    } catch (const std::exception& error) {
        std::cerr << "CRT demo: " << error.what() << '\n';
        if (glContext) SDL_GL_DestroyContext(glContext);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (glContext) SDL_GL_DestroyContext(glContext);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
