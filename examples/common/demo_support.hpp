#ifndef ZXFONT_EXAMPLES_DEMO_SUPPORT_HPP
#define ZXFONT_EXAMPLES_DEMO_SUPPORT_HPP

#include "zxfont/zxfont.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace demo {

// The host owns this buffer. ZXFont only overlays text onto its RGBA pixels.
class Frame {
public:
    Frame(int width, int height)
        : width_(width), height_(height),
          pixels_(static_cast<std::size_t>(width) * height * 4, 0) {}

    void clear(zx::Color color) noexcept {
        for (std::size_t index = 0; index < pixels_.size(); index += 4) {
            pixels_[index] = color.r;
            pixels_[index + 1] = color.g;
            pixels_[index + 2] = color.b;
            pixels_[index + 3] = color.a;
        }
    }

    void rect(int x, int y, int width, int height, zx::Color color) noexcept {
        if (width <= 0 || height <= 0) {
            return;
        }
        const int x0 = std::clamp(x, 0, width_);
        const int y0 = std::clamp(y, 0, height_);
        const int x1 = std::clamp(x + width, 0, width_);
        const int y1 = std::clamp(y + height, 0, height_);
        for (int py = y0; py < y1; ++py) {
            for (int px = x0; px < x1; ++px) {
                const auto offset = (static_cast<std::size_t>(py) * width_ + px) * 4;
                pixels_[offset] = color.r;
                pixels_[offset + 1] = color.g;
                pixels_[offset + 2] = color.b;
                pixels_[offset + 3] = color.a;
            }
        }
    }

    [[nodiscard]] zx::Surface surface() noexcept {
        return {pixels_, static_cast<std::uint32_t>(width_),
                static_cast<std::uint32_t>(height_),
                static_cast<std::size_t>(width_) * 4};
    }

    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const noexcept {
        return pixels_;
    }

private:
    int width_;
    int height_;
    std::vector<std::uint8_t> pixels_;
};

// Until the public box primitive exists, ASCII frames exercise Context::text.
inline void rule(zx::Context& ui, int column, int row, int columns,
                 zx::Color color) {
    if (columns < 2) {
        return;
    }
    ui.text(column * 8, row * 8,
            "+" + std::string(static_cast<std::size_t>(columns - 2), '-') + "+",
            color);
}

inline void box(zx::Context& ui, int column, int row, int columns, int rows,
                zx::Color color) {
    if (columns < 2 || rows < 2) {
        return;
    }
    rule(ui, column, row, columns, color);
    for (int r = row + 1; r < row + rows - 1; ++r) {
        ui.text(column * 8, r * 8, "|", color);
        ui.text((column + columns - 1) * 8, r * 8, "|", color);
    }
    rule(ui, column, row + rows - 1, columns, color);
}

}  // namespace demo

#endif  // ZXFONT_EXAMPLES_DEMO_SUPPORT_HPP
