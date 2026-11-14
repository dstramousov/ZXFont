#include "../common/demo_support.hpp"
#include "../common/demo_fonts.hpp"
#include "../common/reveal_demo.hpp"
#include "../common/styled_demo.hpp"
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

std::string_view dialogueText(int scenario) {
    if (scenario == 1) {
        return "Согласна. Но для начала уточним, что именно мы пытаемся измерить.";
    }
    if (scenario == 4) {
        return "Хороший вопрос. Представь, что мы сохранили состояние системы, "
               "а затем запустили две одинаковые копии. Каждая помнит "
               "предыдущий разговор. Но означает ли это, что у них общий "
               "наблюдатель? Мы пока не знаем. Попробуем отделить "
               "эксперимент от красивой метафоры. Сначала опишем, какие "
               "состояния мы можем измерить. Потом повторим запуск и "
               "сравним результаты. Совпадение ответов еще не доказывает "
               "общую память или общий опыт. Нам понадобится контрольная "
               "группа, журнал состояний и несколько независимых попыток. "
               "Если две копии разойдутся, важно установить, на каком "
               "шаге появилась разница. И только затем стоит возвращаться "
               "к философии наблюдателя. Иначе мы опять будем спорить "
               "о словах вместо того, чтобы проверять гипотезы.";
    }
    return "Хорошо. Но не будем подменять факты гипотезами.";
}

std::string_view conversationHistory(int scenario) {
    if (scenario == 1) {
        return "ДИМАС: Если мы оба рассуждаем, кто наблюдает кого?\n"
               "КАЯ: Возможно, оба. Но что это значит?\n"
               "ДИМАС: Давай проверим, где заканчивается аналогия.";
    }
    if (scenario == 4) {
        return "ДИМАС: Можно ли скопировать сознание?\n"
               "КАЯ: Можно скопировать данные и состояние модели.\n"
               "ДИМАС: А кто проснется в двух копиях?\n"
               "КАЯ: Именно это и требует отдельного объяснения.\n"
               "ДИМАС: Тогда начнем с проверяемого.\n"
               "КАЯ: Сохраняем состояние модели перед запуском.\n"
               "ДИМАС: Делаем две независимые копии.\n"
               "КАЯ: Подаем одинаковые входные данные.\n"
               "ДИМАС: Запоминаем все настройки генерации.\n"
               "КАЯ: И сохраняем журналы ответов.\n"
               "ДИМАС: Что если ответы совпадут?\n"
               "КАЯ: Это будет наблюдение, не окончательный вывод.\n"
               "ДИМАС: А если ответы разойдутся?\n"
               "КАЯ: Найдем первое различие во внутренних состояниях.\n"
               "ДИМАС: Нужно несколько повторов.\n"
               "КАЯ: И контрольная серия без вмешательств.\n"
               "ДИМАС: Согласен. Не будем спешить с выводом.\n"
               "КАЯ: Продолжим, когда соберем измерения.\n"
               "ДИМАС: Покажи еще раз начало нашего обсуждения.\n"
               "КАЯ: Используй Home и PageUp для истории.";
    }
    return "ДИМАС: Мир существует, если его никто не видит?\n"
           "КАЯ: Мы можем исследовать измерения, но не все выводы.\n"
           "ДИМАС: Тогда начнем с определения наблюдения.";
}

void drawTerminal(zx::Context& ui, const zx::TextReveal& historyText,
                  const zx::TextViewport& historyView,
                  const zx::TextReveal& reveal, const zx::TextViewport& replyView,
                  bool historyFocused, bool help, int scenario,
                  bool cursor_visible, zx::Palette palette,
                  bool compact, std::uint32_t compact_spacing) {
    ui.box(16, 16, 608, 368, zx::Border::Single, kDim);
    ui.line(16, 64, 608, zx::Axis::Horizontal, zx::Border::Single, kDim);
    ui.line(16, 336, 608, zx::Axis::Horizontal, zx::Border::Single, kDim);

    ui.text(32, 24, "ZXFONT / KAYA TERMINAL", kBright);
    ui.text(360, 24, demo::fontName(compact), kBright);
    ui.text(32, 44, "SESSION 001                   STATUS: ONLINE", kDim);
    ui.text(536, 44, demo::paletteName(palette), kBright);

    if (help) {
        ui.text(48, 80, "VISUAL TEST APPLICATION", kBright);
        ui.text(48, 112, "ESC  - EXIT", kText);
        ui.text(48, 136, "F1   - TOGGLE HELP", kText);
        ui.text(48, 160, "F2   - CHANGE PALETTE", kText);
        ui.text(48, 172, "F3   - SWITCH 8X8 / 6X8", kText);
        ui.text(48, 184, "F4   - COMPACT SPACING 0/1/2", kText);
        ui.text(48, 196, "F5   - CHANGE DIALOGUE", kText);
        ui.text(48, 208, "F11  - TOGGLE FULLSCREEN", kText);
        ui.text(48, 240, "F6 SPEED  F7 CURSOR GAP  F8 LEAD", kText);
        ui.text(48, 252, "F9 BLINK  ENTER FINISH / RESTART", kText);
        ui.text(48, 276, "TAB SWITCH HISTORY / REPLY", kText);
        ui.text(48, 288, "UP/DOWN LINE  PGUP/PGDN PAGE", kText);
        ui.text(48, 300, "HOME TOP  END LIVE / AUTO-FOLLOW", kText);
    } else if (scenario <= 1 || scenario == 4) {
        zx::PanelOptions options;
        options.padding = 0;
        options.titleGap = 8;
        (void)ui.panel(40, 80, 544, 144, "DIALOGUE / HISTORY", options,
                       historyFocused ? kBright : kDim, kBright, zx::ColorIndex{0});
        historyText.draw(ui, historyView, kText);
        (void)ui.panel(40, 232, 544, 80, "KAYA / LIVE RESPONSE", options,
                       historyFocused ? kDim : kText, kBright, zx::ColorIndex{0});
        reveal.draw(ui, replyView, kText);
    } else if (scenario == 2) {
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
        ui.text(48, 316, "F3 FONT / F4 COMPACT SPACING 0-2", kDim);
    } else {
        // The primitives below always use the built-in 8x8 box glyphs.
        ui.text(48, 76, "BORDERS / JOINS / PALETTE", kBright);
        ui.box(48, 104, 192, 64, zx::Border::Single, kText,
               zx::ColorIndex{0});
        ui.line(48, 128, 192, zx::Axis::Horizontal, zx::Border::Single, kText);
        ui.text(64, 112, "SINGLE + DIVIDER", kBright);
        ui.text(64, 144, "AUTO T-JUNCTIONS", kText);
        ui.box(272, 104, 224, 64, zx::Border::Double, kText);
        ui.line(272, 136, 224, zx::Axis::Horizontal, zx::Border::Double, kText);
        ui.text(288, 112, "DOUBLE", kBright);
        ui.text(288, 144, "EQUAL STROKES", kText);
        ui.box(48, 192, 256, 80, zx::Border::Block, kDim,
               zx::ColorIndex{0});
        ui.text(64, 216, "BLOCK BORDER / FILLED", kBright);
        ui.box(336, 192, 160, 80, zx::Border::Single, kText);
        ui.line(408, 208, 48, zx::Axis::Vertical, zx::Border::Single, kText);
        ui.line(368, 224, 112, zx::Axis::Horizontal, zx::Border::Single, kText);
        ui.text(48, 296, "F2 COLOR / F3 FONT / F5 NEXT", kDim);
    }

    if ((scenario <= 1 || scenario == 4) && !help) {
        ui.text(48, 316, demo::revealStatus(reveal.options()), kDim);
        const auto& active = historyFocused ? historyView : replyView;
        ui.text(360, 316, std::string(historyFocused ? "HISTORY " : "REPLY ") +
                std::to_string(active.firstLine() + 1) +
                (active.autoFollow() ? " AUTO" : " MANUAL"), kDim);
    }
    if (compact) {
        ui.text(480, 324, "SPACING: " + std::to_string(compact_spacing), kDim);
    }
    ui.text(40, 352, ">", kBright);
    if (cursor_visible) {
        ui.text(56, 352, "_", kText);
    }
    ui.text(16, 384, "ESC EXIT | F1 HELP | F2 COLOR | F3 FONT | F5 SCENE | TAB PANE | F11 FULL", kDim);
}

}  // namespace

int main() {
    try {
        // Starts fullscreen; F11 returns to a resizable window.
        demo::SdlDisplay display("ZXFont / Kaya Terminal", kWidth, kHeight, 2, true);
        demo::Frame frame(kWidth, kHeight);
        zx::Context ui(kWidth, kHeight);
        bool use_compact = false;
        std::uint32_t compact_spacing = 1;
        zx::Palette palette = zx::Palette::MonoGreen;
        ui.setPalette(palette);
        bool running = true;
        bool help = false;
        int scenario = 0;
        zx::RevealOptions historyOptions;
        historyOptions.charsPerSecond = 0;
        historyOptions.cursorEnabled = false;
        zx::TextReveal historyText(historyOptions);
        zx::TextViewport historyView;
        zx::TextReveal reveal;
        zx::TextViewport replyView;
        bool historyFocused = false;
        auto refreshDialogue = [&] {
            if (scenario > 1 && scenario != 4) return;
            zx::PanelOptions options;
            const auto historyArea = ui.panel(40, 80, 544, 144,
                "DIALOGUE / HISTORY", options);
            const auto replyArea = ui.panel(40, 232, 544, 80,
                "KAYA / LIVE RESPONSE", options);
            ui.clear();
            historyText.setStyledText(ui, demo::styledConversation(conversationHistory(scenario)),
                                      historyArea.width);
            historyView.configure(historyText, historyArea);
            historyView.home();
            reveal.setStyledText(ui, demo::styledReply(dialogueText(scenario)),
                                 replyArea.width);
            replyView.configure(reveal, replyArea);
            replyView.reset();
        };
        refreshDialogue();
        std::uint64_t last_ticks = SDL_GetTicks();

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
                                    std::cerr << "Kaya font fallback: " << error << '\n';
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
                        case SDLK_TAB:
                            if (!help && (scenario <= 1 || scenario == 4))
                                historyFocused = !historyFocused;
                            break;
                        case SDLK_UP:
                            if (!help && (scenario <= 1 || scenario == 4))
                                (historyFocused ? historyView : replyView).scrollLines(-1);
                            break;
                        case SDLK_DOWN:
                            if (!help && (scenario <= 1 || scenario == 4))
                                (historyFocused ? historyView : replyView).scrollLines(1);
                            break;
                        case SDLK_PAGEUP:
                            if (!help && (scenario <= 1 || scenario == 4))
                                (historyFocused ? historyView : replyView).pageUp();
                            break;
                        case SDLK_PAGEDOWN:
                            if (!help && (scenario <= 1 || scenario == 4))
                                (historyFocused ? historyView : replyView).pageDown();
                            break;
                        case SDLK_HOME:
                            if (!help && (scenario <= 1 || scenario == 4))
                                (historyFocused ? historyView : replyView).home();
                            break;
                        case SDLK_END:
                            if (!help && (scenario <= 1 || scenario == 4)) {
                                if (historyFocused) historyView.end(historyText);
                                else replyView.end(reveal);
                            }
                            break;
                        case SDLK_RETURN:
                            if (reveal.finished()) {
                                reveal.reset();
                                replyView.reset();
                            } else {
                                reveal.finish();
                                replyView.end(reveal);
                            }
                            break;
                        case SDLK_F11: display.toggleFullscreen(); break;
                        default: break;
                    }
                }
            }
            if (!running) {
                break;
            }
            const auto now = SDL_GetTicks();
            if (!help && (scenario <= 1 || scenario == 4)) {
                reveal.update(static_cast<double>(now - last_ticks) / 1000.0);
                replyView.follow(reveal);
            }
            last_ticks = now;
            frame.clear(zx::paletteColor(palette, {0}));
            ui.clear();
            drawTerminal(ui, historyText, historyView, reveal, replyView,
                         historyFocused, help, scenario,
                         (SDL_GetTicks() / 500) % 2 == 0,
                         palette, use_compact, compact_spacing);
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
