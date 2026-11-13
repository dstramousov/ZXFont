#include "zxfont/zxfont.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <string>
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
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "\xD0"); }));

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


void testUnicodeAndSnapshot() {
    zx::Font builtin;
    require(builtin.glyphWidth() == 8 && builtin.glyphHeight() == 8);
    for (char32_t cp : U"Привет, Кая! Її Єє Ґґ Йё") {
        if (cp != 0) {
            require(builtin.hasGlyph(cp));
        }
    }
    require(!builtin.hasGlyph(U'🎯'));

    zx::Context ui(96, 24);
    ui.text(0, 0, "Привет, Кая!", {0, 255, 0, 255});
    ui.text(0, 8, "Її Єє Ґґ Йё", {255, 255, 255, 255});
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "🎯"); }));
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "\xC0\x80"); }));
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "\xED\xA0\x80"); }));
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "\xF4\x90\x80\x80"); }));
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "\xE2\x28\xA1"); }));
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "\r"); }));

    std::vector<std::uint8_t> pixels(96 * 24 * 4);
    ui.render({pixels, 96, 24, 96 * 4});
    auto count_opaque = [](const auto& image, std::size_t first_row,
                           std::size_t last_row) {
        std::size_t count = 0;
        for (std::size_t y = first_row; y < last_row; ++y) {
            for (std::size_t x = 0; x < 96; ++x) {
                count += image[offset(x, y, 96 * 4) + 3] != 0;
            }
        }
        return count;
    };
    require(count_opaque(pixels, 0, 8) > 10);
    require(count_opaque(pixels, 8, 16) > 10);
}

void testPalettes() {
    const auto green = zx::paletteColor(zx::Palette::MonoGreen, {10});
    const auto amber = zx::paletteColor(zx::Palette::MonoAmber, {10});
    const auto spectrum = zx::paletteColor(zx::Palette::Spectrum, {10});
    const auto vga = zx::paletteColor(zx::Palette::VGA, {10});
    require(green.g > green.r && green.g > green.b);
    require(amber.r > amber.g && amber.g > amber.b);
    require(spectrum.r == 255 && spectrum.g == 0 && spectrum.b == 0);
    require(vga.r == 85 && vga.g == 255 && vga.b == 85);
    for (auto palette : {zx::Palette::MonoGreen, zx::Palette::MonoAmber,
                         zx::Palette::Spectrum, zx::Palette::VGA}) {
        for (std::uint8_t i = 0; i < 16; ++i) {
            require(zx::paletteColor(palette, {i}).a == 255);
        }
    }
    require(throws<std::out_of_range>([] {
        (void)zx::paletteColor(zx::Palette::VGA, {16});
    }));

    zx::Context ui(24, 8);
    ui.text(0, 0, "A", zx::ColorIndex{10});
    ui.text(8, 0, "A", zx::Color{13, 14, 15, 255});
    require(throws<std::out_of_range>([&] {
        ui.text(16, 0, "A", zx::ColorIndex{16});
    }));
    require(throws<std::invalid_argument>([&] {
        ui.setPalette(static_cast<zx::Palette>(255));
    }));
    // The same queued commands must render in different palettes, without
    // rewriting text or changing the explicit-RGBA command.
    auto check = [&](zx::Palette palette) {
        std::array<std::uint8_t, 24 * 8 * 4> pixels{};
        ui.setPalette(palette);
        ui.render({pixels, 24, 8, 24 * 4});
        const auto expected = zx::paletteColor(palette, {10});
        const auto first = offset(2, 0, 24 * 4);
        const auto second = offset(10, 0, 24 * 4);
        require(pixels[first] == expected.r);
        require(pixels[first + 1] == expected.g);
        require(pixels[first + 2] == expected.b);
        require(pixels[second] == 13);
        require(pixels[second + 1] == 14);
        require(pixels[second + 2] == 15);
        require(pixels[offset(18, 0, 24 * 4) + 3] == 0);
    };
    check(zx::Palette::MonoGreen);
    check(zx::Palette::MonoAmber);
    check(zx::Palette::Spectrum);
    check(zx::Palette::VGA);
}

void appendLe32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::vector<std::uint8_t> makePsf2() {
    std::vector<std::uint8_t> bytes;
    appendLe32(bytes, 0x864AB572);  // magic
    appendLe32(bytes, 0);           // version
    appendLe32(bytes, 32);          // header size
    appendLe32(bytes, 1);           // Unicode table flag
    appendLe32(bytes, 2);           // glyph count
    appendLe32(bytes, 16);          // bytes per glyph: 9x8, 2 bytes/row
    appendLe32(bytes, 8);           // height
    appendLe32(bytes, 9);           // width
    for (int i = 0; i < 8; ++i) {
        bytes.push_back(0x80); // leftmost pixel
        bytes.push_back(0x80); // ninth pixel
    }
    for (int i = 0; i < 8; ++i) {
        bytes.push_back(0x40);
        bytes.push_back(0x00);
    }
    bytes.insert(bytes.end(), {'A', 'B', 0xFF, 0xD0, 0x96, 0xFF});
    return bytes;
}

void testExternalPsf2() {
    const auto path = std::filesystem::current_path() / "zxfont_test_external.psf";
    struct RemoveFile {
        std::filesystem::path path;
        ~RemoveFile() { std::error_code ec; std::filesystem::remove(path, ec); }
    } cleanup{path};

    auto writeFont = [&](const std::vector<std::uint8_t>& bytes) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        require(static_cast<bool>(file));
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        file.close();
        require(static_cast<bool>(file));
    };

    const auto valid = makePsf2();
    writeFont(valid);
    auto loaded = zx::Font::loadPsf2(path);
    require(loaded.glyphWidth() == 9 && loaded.glyphHeight() == 8);
    require(loaded.hasGlyph(U'A') && loaded.hasGlyph(U'B') && loaded.hasGlyph(U'Ж'));
    require(!loaded.hasGlyph(U'Z'));

    zx::Context ui(27, 16);
    ui.text(0, 0, "A", {255, 0, 0, 255});  // built-in font snapshot
    ui.setFont(loaded);
    ui.text(9, 0, "BЖ", {0, 255, 0, 255});
    ui.text(0, 8, "Ж");
    require(throws<std::invalid_argument>([&] { ui.text(0, 0, "Z"); }));
    std::vector<std::uint8_t> pixels(27 * 16 * 4);
    ui.render({pixels, 27, 16, 27 * 4});
    require(pixels[offset(2, 0, 27 * 4)] == 255);  // built-in A unchanged
    require(pixels[offset(9, 0, 27 * 4) + 1] == 255);  // PSF2 glyph, col 0
    require(pixels[offset(17, 0, 27 * 4) + 1] == 255); // PSF2 glyph, col 8
    require(pixels[offset(19, 0, 27 * 4) + 1] == 255); // Cyrillic glyph, col 1
    require(pixels[offset(1, 8, 27 * 4) + 3] == 255);

    auto invalid = valid;
    invalid[12] = 0;  // no Unicode table flag
    writeFont(invalid);
    require(throws<std::invalid_argument>([&] { (void)zx::Font::loadPsf2(path); }));
    invalid = valid;
    invalid.pop_back();  // missing final terminator
    writeFont(invalid);
    require(throws<std::invalid_argument>([&] { (void)zx::Font::loadPsf2(path); }));
    invalid = valid;
    invalid[24] = 0;  // zero height
    writeFont(invalid);
    require(throws<std::invalid_argument>([&] { (void)zx::Font::loadPsf2(path); }));
    invalid = valid;
    invalid[68] = 0xC0;  // invalid UTF-8 leading byte in Unicode table
    writeFont(invalid);
    require(throws<std::invalid_argument>([&] { (void)zx::Font::loadPsf2(path); }));
}

void testSpectrumUnicodeFont() {
    const zx::Font legacy;
    const zx::Font spectrum = zx::Font::loadPsf2(ZXFONT_TEST_FONT_PATH);
    require(spectrum.glyphWidth() == 8 && spectrum.glyphHeight() == 8);
    require(spectrum.hasGlyph(U' '));
    require(!spectrum.hasGlyph(U'🎯'));
    for (const auto cp : U"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          U"abcdefghijklmnopqrstuvwxyz"
                          U"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
                          U"абвгдеёжзийклмнопрстуфхцчшщъыьэюя"
                          U"ҐґЄєІіЇї┌┐└┘─│█░▒▓←↑→↓") {
        if (cp != 0) {
            require(spectrum.hasGlyph(cp));
        }
    }
    // Sample the exact PSF2 glyph rows. These values exercise the shape of
    // Cyrillic letters, not merely their presence in the Unicode map.
    auto rowsFor = [&](const char* text) {
        zx::Context ui(8, 8);
        ui.setFont(spectrum);
        ui.text(0, 0, text, zx::Color{255, 255, 255, 255});
        std::array<std::uint8_t, 8 * 8 * 4> pixels{};
        ui.render({pixels, 8, 8, 8 * 4});
        std::array<std::uint8_t, 8> rows{};
        for (std::size_t y = 0; y < 8; ++y) {
            for (std::size_t x = 0; x < 8; ++x) {
                if (pixels[offset(x, y, 8 * 4) + 3] != 0) {
                    rows[y] |= static_cast<std::uint8_t>(1u << (7 - x));
                }
            }
        }
        return rows;
    };
    require(rowsFor("А") == (std::array<std::uint8_t, 8>{
        0x00, 0x3C, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x00}));
    require(rowsFor("Ё") == (std::array<std::uint8_t, 8>{
        0x24, 0x00, 0x7E, 0x40, 0x7C, 0x40, 0x7E, 0x00}));
    require(rowsFor("█") == (std::array<std::uint8_t, 8>{
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}));
    require(!legacy.hasGlyph(U'█'));

    // Font switching does not retroactively rewrite queued glyphs.
    zx::Context ui(16, 8);
    ui.text(0, 0, "A");
    ui.setFont(spectrum);
    ui.text(8, 0, "А");
    std::array<std::uint8_t, 16 * 8 * 4> pixels{};
    ui.render({pixels, 16, 8, 16 * 4});
    require(pixels[offset(2, 0, 16 * 4) + 3] == 255);
    require(pixels[offset(10, 1, 16 * 4) + 3] == 255);
}

}  // namespace

int main() {
    testCanvasAndText();
    testClippingAndBlending();
    testSurfaceValidation();
    testUnicodeAndSnapshot();
    testPalettes();
    testExternalPsf2();
    testSpectrumUnicodeFont();
}
