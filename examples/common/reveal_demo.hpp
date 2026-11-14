#ifndef ZXFONT_EXAMPLES_REVEAL_DEMO_HPP
#define ZXFONT_EXAMPLES_REVEAL_DEMO_HPP

#include "zxfont/zxfont.hpp"

#include <array>
#include <string>

namespace demo {

inline double nextTypingSpeed(double current) noexcept {
    constexpr std::array<double, 5> speeds{0, 10, 30, 60, 120};
    for (std::size_t i = 0; i < speeds.size(); ++i) {
        if (speeds[i] == current) return speeds[(i + 1) % speeds.size()];
    }
    return 30;
}

inline double nextCursorLead(double current) noexcept {
    if (current < 0.45) return 0.5;
    if (current < 0.75) return 1.0;
    return 0.25;
}

inline std::string revealStatus(const zx::RevealOptions& options) {
    return "CPS:" + std::to_string(static_cast<int>(options.charsPerSecond)) +
           " GAP:" + std::to_string(options.cursorGapPx) +
           " LEAD:" + std::to_string(static_cast<int>(options.cursorLead * 100)) + "%" +
           (options.cursorBlink ? " BLINK" : " SOLID");
}

}  // namespace demo

#endif  // ZXFONT_EXAMPLES_REVEAL_DEMO_HPP
