#pragma once

#include "zxfont/zxfont.hpp"

#include <string_view>

namespace demo {

/** @brief Mark speaker names in a conversation without changing its UTF-8 text. */
inline zx::StyledText styledConversation(std::string_view message) {
    zx::StyledText result;
    while (!message.empty()) {
        const auto end = message.find('\n');
        const auto row = message.substr(0, end);
        const auto colon = row.find(':');
        if (colon != std::string_view::npos && colon <= 12) {
            zx::TextStyle speaker;
            speaker.foreground = zx::ColorIndex{15};
            speaker.underline = true;
            result.add(row.substr(0, colon + 1), speaker);
            result.add(row.substr(colon + 1), zx::ColorIndex{12});
        } else {
            result.add(row, zx::ColorIndex{12});
        }
        if (end == std::string_view::npos) break;
        result.add("\n", zx::ColorIndex{12});
        message.remove_prefix(end + 1);
    }
    return result;
}

/** @brief Mark one key phrase with full-cell Spectrum INVERSE. */
inline zx::StyledText styledReply(std::string_view message) {
    zx::StyledText result;
    // Each scene has a single emphasis point, useful for testing wraps.
    constexpr std::string_view markers[] = {
        "наблюдателя", "гипотезу", "измерить", "не знаем", "фактом"
    };
    for (const auto marker : markers) {
        const auto found = message.find(marker);
        if (found != std::string_view::npos) {
            result.add(message.substr(0, found), zx::ColorIndex{12});
            zx::TextStyle accent;
            accent.foreground = zx::ColorIndex{15};
            accent.background = zx::ColorIndex{1};
            accent.inverse = true;
            result.add(message.substr(found, marker.size()), accent);
            result.add(message.substr(found + marker.size()), zx::ColorIndex{12});
            return result;
        }
    }
    result.add(message, zx::ColorIndex{12});
    return result;
}

}  // namespace demo
