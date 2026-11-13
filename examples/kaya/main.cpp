#include "../common/demo_support.hpp"
#include "../common/sdl_display.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <exception>
#include <iostream>

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 400;
constexpr zx::Color kText{108, 244, 137, 255};
constexpr zx::Color kDim{48, 125, 72, 255};
constexpr zx::Color kBright{202, 255, 210, 255};

void drawTerminal(zx::Context& ui, bool help, int scenario, bool cursor_visible) {
    demo::box(ui, 2, 2, 76, 46, kDim);
    demo::rule(ui, 2, 6, 76, kDim);
    demo::rule(ui, 2, 42, 76, kDim);

    ui.text(32, 28, "ZXFONT / KAYA TERMINAL", kBright);
    ui.text(32, 44, "SESSION 001                        STATUS: ONLINE", kDim);

    if (help) {
        ui.text(48, 80, "VISUAL TEST APPLICATION", kBright);
        ui.text(48, 112, "ESC  - EXIT", kText);
        ui.text(48, 136, "F1   - TOGGLE HELP", kText);
        ui.text(48, 160, "F5   - CHANGE DIALOGUE", kText);
        ui.text(48, 184, "F11  - TOGGLE FULLSCREEN", kText);
        ui.text(48, 216, "The text and frames are rendered by ZXFont.", kDim);
        ui.text(48, 232, "SDL3 only presents the completed RGBA frame.", kDim);
    } else if (scenario == 0) {
        ui.text(48, 76, "КАЯ:", kBright);
        ui.text(48, 92, "Ты уверен, что мир существует без наблюдателя?", kText);
        ui.text(48, 124, "ДИМАС:", kBright);
        ui.text(48, 140, "Нет. Я просто не знаю, как проверить обратное.", kText);
        ui.text(48, 172, "КАЯ:", kBright);
        ui.text(48, 188, "Тогда начнем с определения сознания.", kText);
        ui.text(48, 220, "ДИМАС:", kBright);
        ui.text(48, 236, "Хорошо. Но не подменяй факты гипотезами.", kText);
    } else {
        ui.text(48, 76, "КАЯ:", kBright);
        ui.text(48, 92, "Кто из нас наблюдает другого?", kText);
        ui.text(48, 124, "ДИМАС:", kBright);
        ui.text(48, 140, "Возможно, оба. Но это еще не доказательство.", kText);
        ui.text(48, 172, "КАЯ:", kBright);
        ui.text(48, 188, "Согласна. Проведем эксперимент.", kText);
    }

    ui.text(40, 352, ">", kBright);
    if (cursor_visible) {
        ui.text(56, 352, "_", kText);
    }
    ui.text(16, 392, "ESC QUIT  |  F1 HELP  |  F5 SCENE  |  F11 FULLSCREEN", kDim);
}

}  // namespace

int main() {
    try {
        // Starts fullscreen; F11 returns to a resizable window.
        demo::SdlDisplay display("ZXFont / Kaya Terminal", kWidth, kHeight, 2, true);
        demo::Frame frame(kWidth, kHeight);
        zx::Context ui(kWidth, kHeight);
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
                        case SDLK_F5: scenario = (scenario + 1) % 2; break;
                        case SDLK_F11: display.toggleFullscreen(); break;
                        default: break;
                    }
                }
            }
            if (!running) {
                break;
            }
            frame.clear({4, 16, 9, 255});
            frame.rect(24, 56, 592, 8, {7, 25, 13, 255});
            ui.clear();
            drawTerminal(ui, help, scenario, (SDL_GetTicks() / 500) % 2 == 0);
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
