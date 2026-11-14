#include "../common/demo_support.hpp"
#include "../common/demo_fonts.hpp"
#include "../common/reveal_demo.hpp"
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

std::string_view dialogueText(int scenario) {
    switch (scenario) {
        case 1: return "Я не знаю. Но готов это проверить.";
        case 2: return "Главное - не путать гипотезу с фактом.";
        default: return "Ты уверен, что мир реален без наблюдателя?";
    }
}

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

void drawUi(zx::Context& ui, const zx::TextReveal& reveal, bool help,
            bool dialog, int scenario, zx::Palette palette, bool compact,
            std::uint32_t compact_spacing) {
    ui.text(8, 8, "HP: 100  LVL: 03", kHud);
    ui.text(176, 8, demo::paletteName(palette), kHud);
    if (scenario <= 2 && !help) {
        ui.text(8, 20, demo::revealStatus(reveal.options()), kHud);
    }
    if (help) {
        ui.box(8, 128, 240, 56, zx::Border::Single, kPanel,
               zx::ColorIndex{0});
        ui.text(24, 136, "F1 HELP F5 NEXT F11 FULL", kPanel);
        ui.text(24, 144, "F2 COLOR F3 FONT F4 SPACE", kPanel);
        ui.text(24, 152, "F6 SPEED F7 GAP F8 LEAD", kPanel);
        ui.text(24, 160, "F9 BLINK ENTER SKIP/REPLAY", kPanel);
        ui.text(24, 168, "SPACE HIDE DIALOG", kPanel);
        return;
    }
    if (!dialog) {
        return;
    }
    if (scenario == 3) {
        // Full-size typography diagnostics, confined to this 256x192 host.
        ui.box(8, 32, 240, 152, zx::Border::Single, kPanel,
               zx::ColorIndex{0});
        ui.text(16, 40, compact ? "COMPACT AIRY / 6X8" : "SPECTRUM / 8X8", kHud);
        if (compact) {
            ui.text(168, 40, "GAP " + std::to_string(compact_spacing), kHud);
        }
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
    if (scenario == 4) {
        ui.box(8, 128, 240, 56, zx::Border::Double, kPanel,
               zx::ColorIndex{0});
        ui.line(8, 152, 240, zx::Axis::Horizontal,
                zx::Border::Double, kPanel);
        ui.text(24, 136, "DOUBLE BORDER", kHud);
        ui.text(24, 160, "F3 FONT / F5 NEXT", kPanel);
        return;
    }
    ui.box(8, 128, 240, 56, zx::Border::Single, kPanel,
           zx::ColorIndex{0});
    if (scenario == 0) {
        ui.text(24, 136, "КАЯ:", kHud);
        reveal.draw(ui, 24, 152, kPanel);
    } else if (scenario == 1) {
        ui.text(24, 136, "ДИМАС:", kHud);
        reveal.draw(ui, 24, 152, kPanel);
    } else {
        ui.text(24, 136, "КАЯ:", kHud);
        reveal.draw(ui, 24, 152, kPanel);
    }
}

}  // namespace

int main() {
    try {
        demo::SdlDisplay display("ZXFont / Mode256 Simulator", kWidth, kHeight,
                                 3, false);
        demo::Frame frame(kWidth, kHeight);
        zx::Context ui(kWidth, kHeight);
        bool use_compact = false;
        std::uint32_t compact_spacing = 1;
        zx::Palette palette = zx::Palette::MonoGreen;
        ui.setPalette(palette);
        bool running = true;
        bool help = false;
        bool dialog = true;
        int scenario = 0;
        int hero_x = 74;
        zx::TextReveal reveal;
        reveal.setText(ui, dialogueText(scenario), 208);
        std::uint64_t last_ticks = SDL_GetTicks();
        auto refreshDialogue = [&] {
            if (scenario <= 2) reveal.setText(ui, dialogueText(scenario), 208);
        };

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
                            break;
                        case SDLK_F3:
                            if (use_compact) {
                                ui.setFont(zx::Font{});
                                ui.setLetterSpacing(0);
                                use_compact = false;
                            } else {
                                std::string error;
                                use_compact = ui.setFont(demo::compactFontPath(), &error);
                                ui.setLetterSpacing(use_compact ? compact_spacing : 0);
                                if (!use_compact) {
                                    std::cerr << "Mode256 font fallback: " << error << '\n';
                                }
                            }
                            refreshDialogue();
                            break;
                        case SDLK_F4:
                            if (use_compact) {
                                compact_spacing = (compact_spacing + 1) % 3;
                                ui.setLetterSpacing(compact_spacing);
                                refreshDialogue();
                            }
                            break;
                        case SDLK_F5:
                            scenario = (scenario + 1) % 5;
                            refreshDialogue();
                            break;
                        case SDLK_F6: {
                            auto opts = reveal.options();
                            opts.charsPerSecond = demo::nextTypingSpeed(opts.charsPerSecond);
                            reveal.setOptions(opts);
                            break;
                        }
                        case SDLK_F7: {
                            auto opts = reveal.options();
                            opts.cursorGapPx = (opts.cursorGapPx + 1) % 4;
                            reveal.setOptions(opts);
                            break;
                        }
                        case SDLK_F8: {
                            auto opts = reveal.options();
                            opts.cursorLead = demo::nextCursorLead(opts.cursorLead);
                            reveal.setOptions(opts);
                            break;
                        }
                        case SDLK_F9: {
                            auto opts = reveal.options();
                            opts.cursorBlink = !opts.cursorBlink;
                            reveal.setOptions(opts);
                            break;
                        }
                        case SDLK_RETURN:
                            if (reveal.finished()) reveal.reset();
                            else reveal.finish();
                            break;
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

            const auto now = SDL_GetTicks();
            if (!help && dialog && scenario <= 2) {
                reveal.update(static_cast<double>(now - last_ticks) / 1000.0);
            }
            last_ticks = now;
            drawWorld(frame, hero_x);
            // Compose ZXFont directly over the host-owned game framebuffer.
            ui.clear();
            drawUi(ui, reveal, help, dialog, scenario, palette, use_compact,
                   compact_spacing);
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
