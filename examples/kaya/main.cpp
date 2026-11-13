#include "../common/demo_support.hpp"
#include "../common/demo_fonts.hpp"
#include "../common/sdl_display.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 400;
constexpr zx::ColorIndex kText{12};
constexpr zx::ColorIndex kDim{7};
constexpr zx::ColorIndex kBright{15};

void drawTerminal(zx::Context& ui, zx::Context& frames, bool help,
                  int scenario, bool cursor_visible, zx::Palette palette,
                  bool compact) {
    demo::box(frames, 2, 2, 76, 46, kDim);
    demo::rule(frames, 2, 6, 76, kDim);
    demo::rule(frames, 2, 42, 76, kDim);

    ui.text(32, 28, "ZXFONT / KAYA TERMINAL", kBright);
    ui.text(360, 28, demo::fontName(compact), kBright);
    ui.text(32, 44, "SESSION 001                        STATUS: ONLINE", kDim);
    ui.text(536, 44, demo::paletteName(palette), kBright);

    if (help) {
        ui.text(48, 80, "VISUAL TEST APPLICATION", kBright);
        ui.text(48, 112, "ESC  - EXIT", kText);
        ui.text(48, 136, "F1   - TOGGLE HELP", kText);
        ui.text(48, 160, "F2   - CHANGE PALETTE", kText);
        ui.text(48, 172, "F3   - SWITCH 8X8 / 6X8", kText);
        ui.text(48, 184, "F5   - CHANGE DIALOGUE", kText);
        ui.text(48, 208, "F11  - TOGGLE FULLSCREEN", kText);
        ui.text(48, 240, "The text and frames are rendered by ZXFont.", kDim);
        ui.text(48, 256, "SDL3 only presents the completed RGBA frame.", kDim);
    } else if (scenario == 0) {
        ui.text(48, 76, "КАЯ:", kBright);
        ui.text(48, 92, "Ты уверен, что мир существует без наблюдателя?", kText);
        ui.text(48, 124, "ДИМАС:", kBright);
        ui.text(48, 140, "Нет. Я просто не знаю, как проверить обратное.", kText);
        ui.text(48, 172, "КАЯ:", kBright);
        ui.text(48, 188, "Тогда начнем с определения сознания.", kText);
        ui.text(48, 220, "ДИМАС:", kBright);
        ui.text(48, 236, "Хорошо. Но не подменяй факты гипотезами.", kText);
    } else if (scenario == 1) {
        ui.text(48, 76, "КАЯ:", kBright);
        ui.text(48, 92, "Кто из нас наблюдает другого?", kText);
        ui.text(48, 124, "ДИМАС:", kBright);
        ui.text(48, 140, "Возможно, оба. Но это еще не доказательство.", kText);
        ui.text(48, 172, "КАЯ:", kBright);
        ui.text(48, 188, "Согласна. Проведем эксперимент.", kText);
    } else {
        // Same samples with either font: a visual regression chart.
        ui.text(48, 76, "FONT MATRIX / LATIN + CYRILLIC", kBright);
        ui.text(48, 96, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", kText);
        ui.text(48, 108, "abcdefghijklmnopqrstuvwxyz", kText);
        ui.text(48, 132, "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ", kText);
        ui.text(48, 144, "абвгдеёжзийклмнопрстуфхцчшщъыьэюя", kText);
        ui.text(48, 168, "Ґґ Єє Іі Її   ШЩЖЫЙФ   0OО 1lІ", kText);
        ui.text(48, 192, "Сегодня проверяем форму букв и ритм строки.", kText);
        ui.text(48, 204, "Українською: Ґанок, Єнот, Їжак, Ірій.", kText);
        ui.text(48, 228, "0123456789  !?.,:; +-*/()[]{}", kText);
        ui.text(48, 252, "┌──────────┬──────────┐", kText);
        ui.text(48, 264, "│ █ ░ ▒ ▓│ ← ↑ → ↓  │", kText);
        ui.text(48, 276, "└──────────┴──────────┘", kText);
        ui.text(48, 316, "F3: SPECTRUM 8X8 / COMPACT AIRY 6X8", kDim);
    }

    ui.text(40, 352, ">", kBright);
    if (cursor_visible) {
        ui.text(56, 352, "_", kText);
    }
    ui.text(16, 384, "ESC EXIT | F1 HELP | F2 COLOR | F3 FONT | F5 SCENE | F11 FULL", kDim);
}

}  // namespace

int main() {
    try {
        // Starts fullscreen; F11 returns to a resizable window.
        demo::SdlDisplay display("ZXFont / Kaya Terminal", kWidth, kHeight, 2, true);
        demo::Frame frame(kWidth, kHeight);
        zx::Context ui(kWidth, kHeight);
        zx::Context frames(kWidth, kHeight); // Borders stay aligned to 8x8 cells.
        bool use_compact = false;
        zx::Palette palette = zx::Palette::MonoGreen;
        ui.setPalette(palette);
        frames.setPalette(palette);
        bool running = true;
        bool help = false;
        int scenario = 0;

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
                                    std::cerr << "Kaya font fallback: " << error << '\n';
                                }
                            }
                            break;
                        case SDLK_F5: scenario = (scenario + 1) % 3; break;
                        case SDLK_F11: display.toggleFullscreen(); break;
                        default: break;
                    }
                }
            }
            if (!running) {
                break;
            }
            frame.clear(zx::paletteColor(palette, {0}));
            frame.rect(24, 56, 592, 8, zx::paletteColor(palette, {1}));
            frames.clear();
            ui.clear();
            drawTerminal(ui, frames, help, scenario, (SDL_GetTicks() / 500) % 2 == 0,
                         palette, use_compact);
            frames.render(frame.surface());
            ui.render(frame.surface());
            display.present(frame.pixels());
            SDL_Delay(16);
        }
    } catch (const std::exception& error) {
        std::cerr << "Kaya Terminal: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
