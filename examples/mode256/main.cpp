#include "../common/demo_support.hpp"
#include "../common/demo_fonts.hpp"
#include "../common/sdl_display.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <exception>
#include <iostream>
#include <initializer_list>
#include <string>

namespace {

constexpr int kWidth = 256;
constexpr int kHeight = 192;
constexpr zx::ColorIndex kHud{15};
constexpr zx::ColorIndex kPanel{12};

void drawWorld(demo::Frame& frame, int hero_x) {
    // All of this scenery belongs to the host application, not ZXFont.
    frame.clear({26, 37, 65, 255});
    for (int star = 0; star < 34; ++star) {
        const int x = (star * 47 + 23) % kWidth;
        const int y = (star * 29 + 11) % 100;
        frame.rect(x, y, 1, 1, {126, 150, 174, 255});
    }
    frame.rect(0, 85, 256, 40, {43, 64, 74, 255});
    for (int ridge = 0; ridge < 256; ridge += 16) {
        const int height = 8 + (ridge * 7 % 23);
        frame.rect(ridge, 85 - height, 16, height, {43, 64, 74, 255});
    }
    for (int y = 120; y < kHeight; y += 16) {
        for (int x = 0; x < kWidth; x += 16) {
            const bool checker = ((x + y) / 16) % 2 == 0;
            frame.rect(x, y, 16, 16, checker ? zx::Color{63, 91, 72, 255}
                                                  : zx::Color{54, 80, 66, 255});
        }
    }
    for (int x : {28, 208}) {
        frame.rect(x + 8, 100, 8, 36, {86, 58, 45, 255});
        frame.rect(x, 86, 24, 24, {25, 66, 47, 255});
        frame.rect(x + 4, 78, 16, 16, {31, 87, 58, 255});
    }

    // Two simple characters and their shadows, owned by the simulated game.
    frame.rect(hero_x - 3, 135, 14, 3, {25, 41, 42, 255});
    frame.rect(hero_x, 110, 8, 8, {223, 187, 144, 255});
    frame.rect(hero_x - 2, 118, 12, 12, {67, 148, 185, 255});
    frame.rect(hero_x, 130, 3, 6, {33, 41, 65, 255});
    frame.rect(hero_x + 5, 130, 3, 6, {33, 41, 65, 255});
    frame.rect(174, 135, 14, 3, {25, 41, 42, 255});
    frame.rect(176, 110, 8, 8, {223, 178, 158, 255});
    frame.rect(174, 118, 12, 12, {157, 94, 169, 255});
    frame.rect(176, 130, 3, 6, {33, 41, 65, 255});
    frame.rect(181, 130, 3, 6, {33, 41, 65, 255});
}

void drawUi(zx::Context& ui, zx::Context& frames, bool help,
            bool dialog, int scenario, zx::Palette palette, bool compact) {
    ui.text(8, 8, "HP: 100  LVL: 03", kHud);
    ui.text(176, 8, demo::paletteName(palette), kHud);
    if (help) {
        demo::box(frames, 1, 16, 30, 7, kPanel);
        ui.text(24, 136, "F1 HELP  F5 NEXT", kPanel);
        ui.text(24, 144, "SPACE HIDE DIALOG", kPanel);
        ui.text(24, 152, "F2 COLOR F3 FONT", kPanel);
        ui.text(24, 160, "F11 FULLSCREEN", kPanel);
        return;
    }
    if (!dialog) {
        return;
    }
    if (scenario == 3) {
        // Full-size typography diagnostics, confined to this 256x192 host.
        demo::box(frames, 1, 4, 30, 19, kPanel);
        ui.text(16, 40, compact ? "COMPACT AIRY / 6X8" : "SPECTRUM / 8X8", kHud);
        ui.text(16, 56, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", kPanel);
        ui.text(16, 64, "abcdefghijklmnopqrstuvwxyz", kPanel);
        ui.text(16, 80, "АБВГДЕЁЖЗИЙКЛМНОПР", kPanel);
        ui.text(16, 88, "СТУФХЦЧШЩЪЫЬЭЮЯ", kPanel);
        ui.text(16, 104, "абвгдеёжзийклмнопр", kPanel);
        ui.text(16, 112, "стуфхцчшщъыьэюя", kPanel);
        ui.text(16, 128, "Ґґ Єє Іі Її", kPanel);
        ui.text(16, 144, "ШЩЖЫЙФ  0OО 1lІ", kPanel);
        ui.text(16, 160, "┌────┬────┐ ░▒▓█", kHud);
        return;
    }
    demo::box(frames, 1, 16, 30, 7, kPanel);
    if (scenario == 0) {
        ui.text(24, 136, "КАЯ:", kHud);
        ui.text(24, 152, compact ? "Ты уверен, что мир реален" :
                                   "Ты уверен, что мир", kPanel);
        ui.text(24, 160, compact ? "без наблюдателя?" :
                                   "реален без наблюдателя?", kPanel);
    } else if (scenario == 1) {
        ui.text(24, 136, "ДИМАС:", kHud);
        ui.text(24, 152, compact ? "Я не знаю, но готов проверить" :
                                   "Я не знаю. Но готов", kPanel);
        ui.text(24, 160, compact ? "Это можно проверить." :
                                   "это проверить.", kPanel);
    } else {
        ui.text(24, 136, "КАЯ:", kHud);
        ui.text(24, 152, "Главное - не путать", kPanel);
        ui.text(24, 160, "гипотезу с фактом.", kPanel);
    }
}

}  // namespace

int main() {
    try {
        demo::SdlDisplay display("ZXFont / Mode256 Simulator", kWidth, kHeight,
                                 3, false);
        demo::Frame frame(kWidth, kHeight);
        zx::Context ui(kWidth, kHeight);
        zx::Context frames(kWidth, kHeight); // Fixed 8x8 border grid.
        bool use_compact = false;
        zx::Palette palette = zx::Palette::MonoGreen;
        ui.setPalette(palette);
        frames.setPalette(palette);
        bool running = true;
        bool help = false;
        bool dialog = true;
        int scenario = 0;
        int hero_x = 74;

        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    switch (event.key.key) {
                        case SDLK_ESCAPE: running = false; break;
                        case SDLK_F1: help = !help; break;
                        case SDLK_F2:
                            palette = demo::nextPalette(palette);
                            ui.setPalette(palette);
                            frames.setPalette(palette);
                            break;
                        case SDLK_F3:
                            if (use_compact) {
                                ui.setFont(zx::Font{});
                                use_compact = false;
                            } else {
                                std::string error;
                                use_compact = ui.setFont(demo::compactFontPath(), &error);
                                if (!use_compact) {
                                    std::cerr << "Mode256 font fallback: " << error << '\n';
                                }
                            }
                            break;
                        case SDLK_F5: scenario = (scenario + 1) % 4; break;
                        case SDLK_F11: display.toggleFullscreen(); break;
                        case SDLK_SPACE: dialog = !dialog; break;
                        default: break;
                    }
                }
            }
            if (!running) {
                break;
            }
            const bool left = SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT];
            const bool right = SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT];
            hero_x = std::clamp(hero_x + static_cast<int>(right) -
                                static_cast<int>(left), 8, 157);

            drawWorld(frame, hero_x);
            // Overlay ZXFont onto the game's already-rendered RGBA framebuffer.
            if (help || dialog) {
                if (!help && scenario == 3) {
                    frame.rect(8, 32, 240, 152,
                               zx::paletteColor(palette, {0}));
                } else {
                    frame.rect(8, 128, 240, 56,
                               zx::paletteColor(palette, {0}));
                }
            }
            frames.clear();
            ui.clear();
            drawUi(ui, frames, help, dialog, scenario, palette, use_compact);
            frames.render(frame.surface());
            ui.render(frame.surface());
            display.present(frame.pixels());
            SDL_Delay(16);
        }
    } catch (const std::exception& error) {
        std::cerr << "Mode256 Simulator: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
