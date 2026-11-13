#include "../examples/common/demo_support.hpp"
#include "../examples/common/frame_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <cmath>

int main() {
    // Fullscreen and resizing must keep every edge of the logical frame visible.
    for (const auto& [out_w, out_h] : {
             std::pair{1920, 1080}, std::pair{1280, 800},
             std::pair{300, 600}, std::pair{256, 160}}) {
        const auto v = demo::fitViewport(640, 400, out_w, out_h);
        if (v.x < 0 || v.y < 0 || v.width <= 0 || v.height <= 0 ||
            v.x + v.width > static_cast<float>(out_w) + 0.1f ||
            v.y + v.height > static_cast<float>(out_h) + 0.1f ||
            std::fabs(v.width / v.height - 1.6f) > 0.001f) {
            std::cerr << "Viewport clips or stretches its content\n";
            return 1;
        }
    }
    // Integer scaling is preferred whenever the frame fits at native size.
    {
        const auto fit = demo::fitViewport(640, 400, 1920, 1080);
        if (fit.width != 1280 || fit.height != 800 ||
            fit.x != 320 || fit.y != 140) {
            std::cerr << "Noninteger scaling in pixel-perfect mode\n";
            return 1;
        }
        const auto small = demo::fitViewport(640, 400, 320, 200);
        if (small.width != 320 || small.height != 200) {
            std::cerr << "Fractional fallback does not fit small window\n";
            return 1;
        }
    }
    try {
        (void)demo::fitViewport(640, 400, 0, 600);
        return 1;
    } catch (const std::invalid_argument&) {}

    if (demo::nextPalette(zx::Palette::MonoGreen) != zx::Palette::MonoAmber ||
        demo::nextPalette(zx::Palette::MonoAmber) != zx::Palette::Spectrum ||
        demo::nextPalette(zx::Palette::Spectrum) != zx::Palette::VGA ||
        demo::nextPalette(zx::Palette::VGA) != zx::Palette::MonoGreen) {
        return 1;
    }

    demo::Frame frame(64, 48);
    constexpr zx::Color background{5, 10, 15, 255};
    frame.clear(background);
    frame.rect(0, 0, 4, 4, {20, 30, 40, 255});
    frame.rect(0, 0, 4, 4, background);

    zx::Context ui(64, 48);
    demo::box(ui, 1, 1, 6, 4, zx::Color{255, 255, 255, 255});
    demo::box(ui, 1, 1, 6, 4, zx::ColorIndex{10});
    ui.setPalette(zx::Palette::VGA);
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
