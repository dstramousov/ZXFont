#include "zxfont/zxfont.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("ZXFont test assertion failed");
    }
}

template <class Exception, class Callback>
bool throws(Callback&& callback) {
    try {
        callback();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

std::size_t offset(std::size_t x, std::size_t y, std::size_t stride) {
    return y * stride + x * 4;
}

void testCanvasAndText() {
    zx::Context ui(16, 16);
    require(ui.width() == 16 && ui.height() == 16);
    require(throws<std::invalid_argument>([] { zx::Context invalid(0, 12); }));
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "\t"); }));
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "\xD0\x9A"); }));

    constexpr std::size_t stride = 16 * 4 + 8;
    std::vector<std::uint8_t> pixels(stride * 16, 0);
    ui.text(0, 0, "A\nB", {10, 20, 30, 255});
    ui.render({pixels, 16, 16, stride});

    // 'A': first row 0x0C, so x=2 and x=3 are lit.
    require(pixels[offset(2, 0, stride) + 0] == 10);
    require(pixels[offset(2, 0, stride) + 1] == 20);
    require(pixels[offset(2, 0, stride) + 2] == 30);
    require(pixels[offset(2, 0, stride) + 3] == 255);
    require(pixels[offset(0, 0, stride)] == 0);
    require(pixels[offset(2, 8, stride) + 3] == 255); // 'B' on next line.
    require(pixels[16 * 4] == 0); // Row padding is preserved.

    ui.clear();
    std::vector<std::uint8_t> empty(pixels.size());
    ui.render({empty, 16, 16, stride});
    for (auto byte : empty) {
        require(byte == 0);
    }
}

void testClippingAndBlending() {
    zx::Context ui(8, 8);
    std::array<std::uint8_t, 8 * 8 * 4> pixels{};
    for (std::size_t index = 0; index < pixels.size(); index += 4) {
        pixels[index + 2] = 100;
        pixels[index + 3] = 255;
    }
    ui.text(-2, 0, "A", {100, 0, 0, 128});
    ui.text(999, 999, "Z");
    ui.render({pixels, 8, 8, 8 * 4});
    const auto index = offset(0, 0, 8 * 4);
    require(pixels[index + 0] == 50);
    require(pixels[index + 1] == 0);
    require(pixels[index + 2] == 50);
    require(pixels[index + 3] == 255);
    require(pixels[offset(7, 7, 8 * 4) + 2] == 100);

    ui.clear();
    ui.text(0, 0, "A", {255, 255, 255, 0});
    const auto before = pixels;
    ui.render({pixels, 8, 8, 8 * 4});
    require(pixels == before);

    // Blending over a transparent destination retains straight RGB.
    ui.clear();
    pixels.fill(0);
    ui.text(0, 0, "A", {120, 70, 30, 128});
    ui.render({pixels, 8, 8, 8 * 4});
    const auto glyph_pixel = offset(2, 0, 8 * 4);
    require(pixels[glyph_pixel + 0] == 120);
    require(pixels[glyph_pixel + 1] == 70);
    require(pixels[glyph_pixel + 2] == 30);
    require(pixels[glyph_pixel + 3] == 128);
}

void testSurfaceValidation() {
    zx::Context ui(8, 8);
    std::vector<std::uint8_t> pixels(8 * 8 * 4);
    require(throws<std::invalid_argument>([&] {
        ui.render({pixels, 7, 8, 7 * 4});
    }));
    require(throws<std::invalid_argument>([&] {
        ui.render({pixels, 8, 8, 8 * 4 - 1});
    }));
    require(throws<std::invalid_argument>([&] {
        ui.render({std::span<std::uint8_t>(pixels).first(50), 8, 8, 8 * 4});
    }));
}

}  // namespace

int main() {
    testCanvasAndText();
    testClippingAndBlending();
    testSurfaceValidation();
}
