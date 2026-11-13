#include "../examples/common/demo_support.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>

int main() {
    demo::Frame frame(64, 48);
    constexpr zx::Color background{5, 10, 15, 255};
    frame.clear(background);
    frame.rect(0, 0, 4, 4, {20, 30, 40, 255});
    frame.rect(0, 0, 4, 4, background);

    zx::Context ui(64, 48);
    demo::box(ui, 1, 1, 6, 4, {255, 255, 255, 255});
    ui.render(frame.surface());

    bool saw_text = false;
    const auto& pixels = frame.pixels();
    for (std::size_t y = 0; y < 48; ++y) {
        for (std::size_t x = 0; x < 64; ++x) {
            const auto offset = (y * 64 + x) * 4;
            const bool changed = pixels[offset] != background.r ||
                                 pixels[offset + 1] != background.g ||
                                 pixels[offset + 2] != background.b;
            if (changed && (x < 8 || x >= 56 || y < 8 || y >= 40)) {
                std::cerr << "Text escaped its expected box bounds\n";
                return 1;
            }
            if (pixels[offset + 3] != 255) {
                std::cerr << "Unexpected alpha value\n";
                return 1;
            }
            saw_text |= changed;
        }
    }
    if (!saw_text) {
        std::cerr << "No glyph pixels rendered\n";
        return 1;
    }
    return 0;
}
