#include "zxfont/zxfont.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <algorithm>
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

    // Spectrum A has top ink in row 1, columns 2..5.
    require(pixels[offset(2, 1, stride) + 0] == 10);
    require(pixels[offset(2, 1, stride) + 1] == 20);
    require(pixels[offset(2, 1, stride) + 2] == 30);
    require(pixels[offset(2, 1, stride) + 3] == 255);
    require(pixels[offset(0, 0, stride)] == 0);
    require(pixels[offset(2, 9, stride) + 3] == 255); // 'B' on next line.
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
    const auto index = offset(0, 1, 8 * 4);
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
    const auto glyph_pixel = offset(2, 1, 8 * 4);
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
        const auto first = offset(2, 1, 24 * 4);
        const auto second = offset(10, 1, 24 * 4);
        require(pixels[first] == expected.r);
        require(pixels[first + 1] == expected.g);
        require(pixels[first + 2] == expected.b);
        require(pixels[second] == 13);
        require(pixels[second + 1] == 14);
        require(pixels[second + 2] == 15);
        require(pixels[offset(18, 1, 24 * 4) + 3] == 0);
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

std::vector<std::uint8_t> makePsf2(std::uint32_t glyph_height = 8) {
    std::vector<std::uint8_t> bytes;
    appendLe32(bytes, 0x864AB572);  // magic
    appendLe32(bytes, 0);           // version
    appendLe32(bytes, 32);          // header size
    appendLe32(bytes, 1);           // Unicode table flag
    appendLe32(bytes, 2);           // glyph count
    appendLe32(bytes, glyph_height * 2);  // 9 pixels wide, 2 bytes/row
    appendLe32(bytes, glyph_height);      // height
    appendLe32(bytes, 9);           // width
    for (std::uint32_t i = 0; i < glyph_height; ++i) {
        bytes.push_back(0x80); // leftmost pixel
        bytes.push_back(0x80); // ninth pixel
    }
    for (std::uint32_t i = 0; i < glyph_height; ++i) {
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
    require(pixels[offset(2, 1, 27 * 4)] == 255);  // built-in A unchanged
    require(pixels[offset(9, 0, 27 * 4) + 1] == 255);  // PSF2 glyph, col 0
    require(pixels[offset(17, 0, 27 * 4) + 1] == 255); // PSF2 glyph, col 8
    require(pixels[offset(19, 0, 27 * 4) + 1] == 255); // Cyrillic glyph, col 1
    require(pixels[offset(1, 8, 27 * 4) + 3] == 255);

    // Exercise two different glyph heights and widths within the same frame.
    // A queued command captures its selected font even after another switch.
    writeFont(makePsf2(12));
    const auto tall_font = zx::Font::loadPsf2(path);
    require(tall_font.glyphWidth() == 9 && tall_font.glyphHeight() == 12);
    zx::Context mixed(32, 24);
    mixed.text(0, 0, "A");
    mixed.setFont(tall_font);
    mixed.text(8, 0, "A\nA");
    mixed.setFont(zx::Font{});
    mixed.text(18, 0, "А");
    std::array<std::uint8_t, 32 * 24 * 4> combined{};
    mixed.render({combined, 32, 24, 32 * 4});
    require(combined[offset(2, 1, 32 * 4) + 3] == 255);
    require(combined[offset(8, 0, 32 * 4) + 3] == 255);
    require(combined[offset(8, 12, 32 * 4) + 3] == 255);
    require(combined[offset(20, 1, 32 * 4) + 3] == 255);

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

void testCompactFontAndFallback() {
    // The selected 6x8 font must be distinct from the built-in 8x8 font.
    const auto compact_path = std::filesystem::path(ZXFONT_TEST_COMPACT_PATH);
    const zx::Font compact = zx::Font::loadPsf2(compact_path);
    require(compact.glyphWidth() == 6 && compact.glyphHeight() == 8);
    for (const auto cp : U"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          U"abcdefghijklmnopqrstuvwxyz"
                          U"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
                          U"абвгдеёжзийклмнопрстуфхцчшщъыьэюя"
                          U"ҐґЄєІіЇї0123456789┌┐└┘─│█░▒▓") {
        if (cp != 0) {
            require(compact.hasGlyph(cp));
        }
    }

    zx::Context ui(20, 8);
    std::string error = "previous error";
    require(ui.setFont(compact_path, &error));
    require(error.empty());
    ui.text(0, 0, "Ж", zx::Color{11, 22, 33, 255});

    // An unavailable font must return to embedded 8x8, while earlier commands
    // still reference the original 6x8 font. No exception must escape.
    const auto missing = compact_path.parent_path() / "font_does_not_exist.psf";
    require(!ui.setFont(missing, &error));
    require(!error.empty());
    ui.text(6, 0, "Ж", zx::Color{44, 55, 66, 255});
    std::array<std::uint8_t, 20 * 8 * 4> pixels{};
    ui.render({pixels, 20, 8, 20 * 4});
    // Compact Ж at y=1: #.#.#.  Built-in Ж at y=1: #..#..#.
    require(pixels[offset(0, 1, 20 * 4)] == 11);
    require(pixels[offset(2, 1, 20 * 4)] == 11);
    require(pixels[offset(6, 1, 20 * 4)] == 44);
    require(pixels[offset(9, 1, 20 * 4)] == 44);
    require(pixels[offset(5, 1, 20 * 4)] == 0);

    // A file that exists but is corrupt is also a recoverable font error.
    const auto corrupt_path = std::filesystem::current_path() /
                              "zxfont_test_corrupt_fallback.psf";
    struct RemoveFile {
        std::filesystem::path path;
        ~RemoveFile() { std::error_code ec; std::filesystem::remove(path, ec); }
    } cleanup{corrupt_path};
    {
        std::ofstream out(corrupt_path, std::ios::binary | std::ios::trunc);
        require(static_cast<bool>(out));
        out << "corrupt font data";
        out.close();
        require(static_cast<bool>(out));
    }
    require(!ui.setFont(corrupt_path, &error));
    require(!error.empty());
    ui.clear();
    ui.text(0, 0, "Ё", zx::Color{1, 2, 3, 255});
    pixels.fill(0);
    ui.render({pixels, 20, 8, 20 * 4});
    require(pixels[offset(2, 0, 20 * 4)] == 1);

    // A valid load after an earlier failure must clear the diagnostic.
    require(ui.setFont(compact_path, &error));
    require(error.empty());
    ui.clear();
    ui.text(0, 0, "Ж");
    pixels.fill(0);
    ui.render({pixels, 20, 8, 20 * 4});
    require(pixels[offset(2, 1, 20 * 4) + 3] == 255);
    // Direct loading remains strict for applications that prefer exceptions.
    require(throws<std::runtime_error>([&] {
        (void)zx::Font::loadPsf2(missing);
    }));
}

void testSpectrumUnicodeFont() {
    const zx::Font builtin;
    const zx::Font spectrum = zx::Font::loadPsf2(ZXFONT_TEST_FONT_PATH);
    require(builtin.glyphWidth() == 8 && builtin.glyphHeight() == 8);
    require(spectrum.glyphWidth() == 8 && spectrum.glyphHeight() == 8);
    require(spectrum.hasGlyph(U' '));
    require(!spectrum.hasGlyph(U'🎯'));
    for (const auto cp : U"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          U"abcdefghijklmnopqrstuvwxyz"
                          U"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
                          U"абвгдеёжзийклмнопрстуфхцчшщъыьэюя"
                          U"ҐґЄєІіЇї┌┐└┘─│█░▒▓←↑→↓") {
        if (cp != 0) {
            require(spectrum.hasGlyph(cp) && builtin.hasGlyph(cp));
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
    require(builtin.hasGlyph(U'█'));

    // Built-in and file-loaded fonts must be pixel-identical for the same text.
    const std::string phrase = "АБВГДЁЖЩ Її Ґґ ┌─┐█ 012345 ABCdef";
    zx::Context compared(384, 16);
    compared.text(0, 0, phrase);
    std::vector<std::uint8_t> embedded_pixels(384 * 16 * 4);
    compared.render({embedded_pixels, 384, 16, 384 * 4});
    compared.clear();
    compared.setFont(spectrum);
    compared.text(0, 0, phrase);
    std::vector<std::uint8_t> external_pixels(384 * 16 * 4);
    compared.render({external_pixels, 384, 16, 384 * 4});
    require(embedded_pixels == external_pixels);

    // Multiple Font objects remain attached to their own drawing commands.
    zx::Context ui(24, 16);
    ui.text(0, 0, "A");
    ui.setFont(spectrum);
    ui.text(8, 0, "А");
    ui.setFont(builtin);
    ui.text(16, 0, "Ё");
    std::array<std::uint8_t, 24 * 16 * 4> pixels{};
    ui.render({pixels, 24, 16, 24 * 4});
    require(pixels[offset(2, 1, 24 * 4) + 3] == 255);
    require(pixels[offset(10, 1, 24 * 4) + 3] == 255);
    require(pixels[offset(18, 0, 24 * 4) + 3] == 255);
}

void testLetterSpacing() {
    constexpr std::size_t kWidth = 72;
    constexpr std::size_t kHeight = 40;
    constexpr std::size_t kStride = kWidth * 4;
    auto render = [](const zx::Context& ui) {
        std::array<std::uint8_t, kWidth * kHeight * 4> pixels{};
        ui.render({pixels, kWidth, kHeight, kStride});
        return pixels;
    };

    // Zero extra spacing remains pixel-identical to the pre-existing API.
    for (std::uint32_t gap : {0u, 1u, 2u}) {
        zx::Context ui(kWidth, kHeight);
        ui.setLetterSpacing(gap);
        ui.text(2, 1, "AB", zx::ColorIndex{10});

        zx::Context reference(kWidth, kHeight);
        reference.text(2, 1, "A", zx::ColorIndex{10});
        reference.text(10 + static_cast<int>(gap), 1, "B", zx::ColorIndex{10});
        require(render(ui) == render(reference));
    }

    // Spacing applies after a space, but never before a newline or at the
    // end of a line. LF still advances exactly one glyph cell vertically.
    zx::Context spaced(kWidth, kHeight);
    spaced.setLetterSpacing(2);
    spaced.text(2, 1, "A B\nBA");
    zx::Context reference(kWidth, kHeight);
    reference.text(2, 1, "A");
    reference.text(22, 1, "B"); // A + ' ' with 8px advance + 2px gap each.
    reference.text(2, 9, "B");
    reference.text(12, 9, "A");
    require(render(spaced) == render(reference));

    // Later spacing changes must not move already queued commands.
    zx::Context snapshot(kWidth, kHeight);
    snapshot.text(2, 1, "AB");
    snapshot.setLetterSpacing(1);
    snapshot.text(2, 12, "AB");
    snapshot.setLetterSpacing(2);
    snapshot.text(2, 23, "AB");
    zx::Context expected(kWidth, kHeight);
    for (int row = 0; row < 3; ++row) {
        const int y = 1 + row * 11;
        expected.text(2, y, "A");
        expected.text(10 + row, y, "B");
    }
    require(render(snapshot) == render(expected));

    // Arbitrary unsigned spacing must be clipped safely (no wraparound).
    zx::Context huge(kWidth, kHeight);
    huge.setLetterSpacing(UINT32_MAX);
    huge.text(2, 1, "AB");
    zx::Context first_only(kWidth, kHeight);
    first_only.text(2, 1, "A");
    require(render(huge) == render(first_only));

    // The same API applies to an external 6x8 Cyrillic font.
    zx::Font compact = zx::Font::loadPsf2(ZXFONT_TEST_COMPACT_PATH);
    require(compact.glyphWidth() == 6 && compact.glyphHeight() == 8);
    zx::Context compact_ui(kWidth, kHeight);
    compact_ui.setFont(compact);
    compact_ui.setLetterSpacing(1);
    compact_ui.text(2, 1, "ЯЖ");
    zx::Context compact_ref(kWidth, kHeight);
    compact_ref.setFont(compact);
    compact_ref.text(2, 1, "Я");
    compact_ref.text(9, 1, "Ж");
    require(render(compact_ui) == render(compact_ref));
}

void testTextLayout() {
    zx::Context ui(96, 64);
    const auto empty = ui.measureText("");
    require(empty.lines == 0 && empty.width == 0 && empty.height == 0);
    const auto single = ui.measureText("Привет");
    require(single.width == 48 && single.height == 8 && single.lines == 1);
    const auto manual = ui.measureText("AB\n\nCD");
    require(manual.width == 16 && manual.height == 24 && manual.lines == 3);
    const auto wrapped = ui.measureText("AB CD EFGH", 32);
    require(wrapped.width == 32 && wrapped.height == 24 && wrapped.lines == 3);
    const auto hard = ui.measureText("ABCDE", 16);
    require(hard.width == 16 && hard.height == 24 && hard.lines == 3);
    require(throws<std::invalid_argument>([&] { (void)ui.measureText("A", 7); }));
    require(throws<std::invalid_argument>([&] { (void)ui.measureText("\xD0", 32); }));
    require(throws<std::invalid_argument>([&] { ui.textBox(0, 0, 0, "A"); }));
    require(throws<std::invalid_argument>([&] { ui.textBox(0, 0, 16, "\t"); }));

    auto render = [](const zx::Context& context) {
        std::array<std::uint8_t, 96 * 64 * 4> pixels{};
        context.render({pixels, 96, 64, 96 * 4});
        return pixels;
    };
    ui.textBox(0, 0, 32, "AB CD EFGH", zx::ColorIndex{12});
    zx::Context expected(96, 64);
    expected.text(0, 0, "AB", zx::ColorIndex{12});
    expected.text(0, 8, "CD", zx::ColorIndex{12});
    expected.text(0, 16, "EFGH", zx::ColorIndex{12});
    require(render(ui) == render(expected));

    ui.clear();
    ui.setFont(zx::Font::loadPsf2(ZXFONT_TEST_COMPACT_PATH));
    ui.setLetterSpacing(1);
    const auto compact = ui.measureText("АБ ВГ", 20);
    require(compact.lines == 2 && compact.width == 13 && compact.height == 16);
    ui.textBox(0, 0, 20, "АБ ВГ");
    zx::Context compact_ref(96, 64);
    compact_ref.setFont(zx::Font::loadPsf2(ZXFONT_TEST_COMPACT_PATH));
    compact_ref.setLetterSpacing(1);
    compact_ref.text(0, 0, "АБ");
    compact_ref.text(0, 8, "ВГ");
    require(render(ui) == render(compact_ref));

    // Text submitted before changing the selected font must keep its layout.
    ui.setFont(zx::Font{});
    ui.setLetterSpacing(0);
    require(render(ui) == render(compact_ref));
    require(throws<std::invalid_argument>([&] { (void)ui.measureText("A", 5); }));
}


void testTextReveal() {
    constexpr std::size_t width = 96;
    constexpr std::size_t height = 48;
    auto render = [](const zx::Context& ui) {
        std::array<std::uint8_t, width * height * 4> pixels{};
        ui.render({pixels, width, height, width * 4});
        return pixels;
    };

    zx::Context ui(width, height);
    zx::RevealOptions options;
    require(options.charsPerSecond == 30.0);
    require(options.cursorEnabled && options.cursorGapPx == 1);
    zx::TextReveal typed;
    require(typed.finished() && typed.visibleCharacters() == 0);

    options.charsPerSecond = 10.0;
    options.cursorEnabled = false;
    typed.setOptions(options);
    typed.setText(ui, "Привет", 48);
    require(typed.metrics().lines == 1 && typed.metrics().width == 48);
    require(!typed.finished() && typed.visibleCharacters() == 0);
    typed.update(0.25);
    require(typed.visibleCharacters() == 2);
    typed.draw(ui, 0, 0, zx::ColorIndex{12});
    zx::Context expected(width, height);
    expected.text(0, 0, "Пр", zx::ColorIndex{12});
    require(render(ui) == render(expected));

    // Changing speed preserves the revealed prefix and fractional progress.
    options.charsPerSecond = 20.0;
    typed.setOptions(options);
    typed.update(0.025);
    require(typed.visibleCharacters() == 3);
    ui.clear();
    typed.draw(ui, 0, 0, zx::ColorIndex{12});
    expected.clear();
    expected.text(0, 0, "При", zx::ColorIndex{12});
    require(render(ui) == render(expected));
    typed.finish();
    require(typed.finished() && typed.visibleCharacters() == 6);
    ui.clear();
    typed.draw(ui, 0, 0, zx::ColorIndex{12});
    expected.clear();
    expected.text(0, 0, "Привет", zx::ColorIndex{12});
    require(render(ui) == render(expected));
    typed.reset();
    require(!typed.finished() && typed.visibleCharacters() == 0);

    // Full layout is fixed before revealing; a wrapped line's next cursor
    // is drawn at the real start of the second line, not after the first.
    zx::Context compact_ui(width, height);
    compact_ui.setFont(zx::Font::loadPsf2(ZXFONT_TEST_COMPACT_PATH));
    compact_ui.setLetterSpacing(1);
    options.charsPerSecond = 2;
    options.cursorEnabled = true;
    options.cursorLead = 1;
    options.cursorGapPx = 2;
    options.cursorBlink = false;
    typed.setOptions(options);
    typed.setText(compact_ui, "АБ ВГ", 20);
    require(typed.metrics().lines == 2 && typed.metrics().height == 16);
    typed.update(1.0);
    require(typed.visibleCharacters() == 2);
    typed.draw(compact_ui, 4, 4, zx::ColorIndex{12});
    auto actual = render(compact_ui);
    // Cursor on new line occupies 6x8, no extra gap at x=4.
    auto at = offset(4, 12, width * 4);
    auto cursor_color = zx::paletteColor(zx::Palette::MonoGreen, {10});
    require(actual[at] == cursor_color.r &&
            actual[at + 1] == cursor_color.g && actual[at + 3] == 255);
    require(actual[offset(11, 12, width * 4) + 3] == 0);
    // The prepared glyph widths are independent of later font changes.
    compact_ui.clear();
    compact_ui.setFont(zx::Font{});
    compact_ui.setLetterSpacing(0);
    typed.finish();
    options.cursorAfterComplete = false;
    typed.setOptions(options);
    typed.draw(compact_ui, 4, 4, zx::ColorIndex{12});
    zx::Context ref(width, height);
    ref.setFont(zx::Font::loadPsf2(ZXFONT_TEST_COMPACT_PATH));
    ref.setLetterSpacing(1);
    ref.text(4, 4, "АБ\nВГ", zx::ColorIndex{12});
    require(render(compact_ui) == render(ref));

    // A cursor is a filled rectangle colored by the active palette.
    zx::Context cursor_ui(width, height);
    options.cursorAfterComplete = true;
    options.cursorLead = 0.4;
    options.cursorGapPx = 3;
    options.charsPerSecond = 10;
    typed.setOptions(options);
    typed.setText(cursor_ui, "AB", 48);
    typed.update(0.05);
    typed.draw(cursor_ui, 0, 0);
    require(render(cursor_ui)[offset(0, 0, width * 4) + 3] == 0);
    typed.update(0.02); // Last 40% of interval: cursor now visible.
    cursor_ui.clear();
    typed.draw(cursor_ui, 0, 0);
    require(render(cursor_ui)[offset(0, 0, width * 4) + 3] == 255);
    typed.update(0.03); // The first glyph is now visible.
    require(typed.visibleCharacters() == 1);
    cursor_ui.clear();
    typed.draw(cursor_ui, 0, 0);
    require(render(cursor_ui)[offset(11, 0, width * 4) + 3] == 0);
    // Lead threshold not reached, cursor temporarily disappears.
    typed.update(0.07);
    cursor_ui.clear();
    typed.draw(cursor_ui, 0, 0);
    require(render(cursor_ui)[offset(11, 0, width * 4) + 3] == 255);
    cursor_ui.clear();
    cursor_ui.setPalette(zx::Palette::VGA);
    typed.draw(cursor_ui, 0, 0);
    actual = render(cursor_ui);
    require(actual[offset(11, 0, width * 4) + 0] ==
            zx::paletteColor(zx::Palette::VGA, {10}).r);

    // Blinking and post-completion cursor visibility are independently set.
    options.cursorBlink = true;
    options.cursorLead = 1;
    typed.setOptions(options);
    cursor_ui.clear();
    typed.update(0.51);
    typed.draw(cursor_ui, 0, 0);
    // Already finished; blink is in the off phase.
    require(render(cursor_ui)[offset(19, 0, width * 4) + 3] == 0);
    options.cursorAfterComplete = false;
    typed.setOptions(options);
    cursor_ui.clear();
    typed.draw(cursor_ui, 0, 0);
    expected.clear();
    expected.text(0, 0, "AB");
    expected.setPalette(zx::Palette::VGA);
    require(render(cursor_ui) == render(expected));

    // The trailing cursor wraps after a completely filled last line.
    zx::Context full_line(width, height);
    zx::RevealOptions end_options;
    end_options.charsPerSecond = 0;
    end_options.cursorGapPx = 5;
    end_options.cursorLead = 1;
    zx::TextReveal completed(end_options);
    completed.setText(full_line, "AB", 16);
    require(completed.finished());
    completed.draw(full_line, 0, 0);
    const auto full_pixels = render(full_line);
    require(full_pixels[offset(0, 8, width * 4) + 3] == 255);
    require(full_pixels[offset(16, 0, width * 4) + 3] == 0);

    // Gap is skipped when the extra pixels would cross the measured box.
    end_options.charsPerSecond = 10;
    completed.setOptions(end_options);
    completed.setText(full_line, "AB", 16);
    completed.update(0.1);
    full_line.clear();
    completed.draw(full_line, 0, 0);
    const auto no_gap = render(full_line);
    require(no_gap[offset(8, 0, width * 4) + 3] == 255);

    // Zero CPS is instant; invalid options/deltas do not change state.
    options.charsPerSecond = 0;
    typed.setOptions(options);
    typed.setText(cursor_ui, "Кая", 24);
    require(typed.finished() && typed.visibleCharacters() == 3);
    options.cursorLead = 1.01;
    require(throws<std::invalid_argument>([&] { typed.setOptions(options); }));
    require(typed.options().cursorLead == 1);
    options.cursorLead = 0.4;
    options.charsPerSecond = -1;
    require(throws<std::invalid_argument>([&] { typed.setOptions(options); }));
    options.charsPerSecond = std::numeric_limits<double>::quiet_NaN();
    require(throws<std::invalid_argument>([&] { typed.setOptions(options); }));
    options.charsPerSecond = 10;
    options.cursorColor.value = 16;
    require(throws<std::invalid_argument>([&] { typed.setOptions(options); }));
    require(throws<std::invalid_argument>([&] { typed.update(-1); }));
    require(throws<std::invalid_argument>([&] {
        typed.update(std::numeric_limits<double>::infinity());
    }));
    require(throws<std::invalid_argument>([&] {
        typed.setText(cursor_ui, "A", 0);
    }));
    require(throws<std::invalid_argument>([&] {
        typed.setText(cursor_ui, "\xD0", 24);
    }));
    require(typed.finished() && typed.visibleCharacters() == 3);
}

void testPseudographics() {
    constexpr std::size_t width = 80;
    constexpr std::size_t height = 56;
    auto render = [](const zx::Context& ui) {
        std::array<std::uint8_t, width * height * 4> pixels{};
        ui.render({pixels, width, height, width * 4});
        return pixels;
    };

    // Intersections are chosen automatically for both orientations, even
    // when lines are submitted before their surrounding box.
    zx::Context single(width, height);
    single.line(24, 8, 32, zx::Axis::Vertical, zx::Border::Single);
    single.line(8, 16, 40, zx::Axis::Horizontal, zx::Border::Single);
    single.box(8, 8, 40, 32, zx::Border::Single);
    zx::Context manual(width, height);
    manual.text(8, 8, "┌─┬─┐\n├─┼─┤\n│ │ │\n└─┴─┘");
    require(render(single) == render(manual));

    zx::Context dbl(width, height);
    dbl.box(8, 8, 40, 32, zx::Border::Double);
    dbl.line(8, 16, 40, zx::Axis::Horizontal, zx::Border::Double);
    dbl.line(24, 8, 32, zx::Axis::Vertical, zx::Border::Double);
    // Double borders must not inherit the upstream U+2551 one-pixel shift.
    // Top/bottom corners and all junctions use vertical columns 2 and 4.
    // The middle straight segments must occupy exactly the same columns.
    const auto double_pixels = render(dbl);
    auto ink = [&](std::size_t x, std::size_t y) {
        return double_pixels[offset(x, y, width * 4) + 3] != 0;
    };
    // Straight vertical cells have exactly two aligned columns.
    for (std::size_t y = 24; y < 32; ++y) {
        require(ink(10, y) && ink(12, y));
        require(ink(42, y) && ink(44, y));
        require(!ink(11, y) && !ink(13, y));
        require(!ink(43, y) && !ink(45, y));
    }
    // Both rails continue across cell seams into corners and junctions.
    for (std::size_t y : {15U, 16U, 23U, 24U, 31U, 32U}) {
        require(ink(10, y) && ink(12, y));
        require(ink(42, y) && ink(44, y));
    }
    // An interior vertical stroke stays aligned through a top T,
    // an intersection and a bottom T (including all cell seams).
    for (std::size_t y : {15U, 16U, 23U, 24U, 31U, 32U}) {
        require(ink(26, y) && ink(28, y));
        require(!ink(27, y) && !ink(29, y));
    }
    // The two horizontal channels tile through junction cells.
    for (std::size_t x : {15U, 16U, 23U, 24U, 31U, 32U, 39U, 40U}) {
        require(ink(x, 10) && ink(x, 12));
    }
    // No double-border pixels may leak into an untouched interior cell.
    for (std::size_t y = 25; y < 32; ++y) {
        for (std::size_t x = 17; x < 24; ++x) {
            require(!ink(x, y));
        }
    }
    // Drawing the same box using the compact font changes no geometry.
    zx::Context compact_double(width, height);
    compact_double.setFont(zx::Font::loadPsf2(ZXFONT_TEST_COMPACT_PATH));
    compact_double.setLetterSpacing(2);
    compact_double.box(8, 8, 40, 32, zx::Border::Double);
    compact_double.line(8, 16, 40, zx::Axis::Horizontal, zx::Border::Double);
    compact_double.line(24, 8, 32, zx::Axis::Vertical, zx::Border::Double);
    require(render(compact_double) == double_pixels);

    zx::Context block(width, height);
    block.box(8, 8, 24, 24, zx::Border::Block);
    zx::Context expected_block(width, height);
    expected_block.text(8, 8, "███\n█ █\n███");
    require(render(block) == render(expected_block));

    // Frame glyphs always use the built-in 8x8 font, even when external
    // 6x8 text is active with nonzero letter spacing.
    zx::Context compact(width, height);
    compact.setFont(zx::Font::loadPsf2(ZXFONT_TEST_COMPACT_PATH));
    compact.setLetterSpacing(2);
    compact.box(8, 8, 40, 32, zx::Border::Single);
    compact.line(8, 16, 40, zx::Axis::Horizontal, zx::Border::Single);
    zx::Context classic(width, height);
    classic.box(8, 8, 40, 32, zx::Border::Single);
    classic.line(8, 16, 40, zx::Axis::Horizontal, zx::Border::Single);
    require(render(compact) == render(classic));

    // Indexed border and fill are resolved at render time; RGB colors are not.
    zx::Context recolor(width, height);
    recolor.box(8, 8, 40, 32, zx::Border::Single,
                zx::ColorIndex{10}, zx::ColorIndex{1});
    recolor.text(16, 16, "A", zx::Color{130, 90, 20, 255});
    auto sample = [&](zx::Palette palette) {
        recolor.setPalette(palette);
        const auto pixels = render(recolor);
        const auto stroke = zx::paletteColor(palette, zx::ColorIndex{10});
        const auto fill = zx::paletteColor(palette, zx::ColorIndex{1});
        const auto p_stroke = offset(18, 11, width * 4); // top border
        const auto p_fill = offset(36, 24, width * 4); // internal background
        const auto p_text = offset(18, 17, width * 4); // A's top row
        require(pixels[p_stroke] == stroke.r &&
                pixels[p_stroke + 1] == stroke.g &&
                pixels[p_stroke + 2] == stroke.b);
        require(pixels[p_fill] == fill.r &&
                pixels[p_fill + 1] == fill.g &&
                pixels[p_fill + 2] == fill.b);
        require(pixels[p_text] == 130 && pixels[p_text + 1] == 90 &&
                pixels[p_text + 2] == 20);
    };
    sample(zx::Palette::MonoGreen);
    sample(zx::Palette::VGA);

    // Two crossing translucent strokes must paint a shared glyph only once.
    zx::Context translucent(width, height);
    translucent.line(24, 8, 32, zx::Axis::Vertical, zx::Border::Single,
                     zx::Color{200, 10, 10, 128});
    translucent.line(8, 16, 40, zx::Axis::Horizontal, zx::Border::Single,
                     zx::Color{200, 10, 10, 128});
    const auto transparent_pixels = render(translucent);
    require(transparent_pixels[offset(27, 19, width * 4) + 3] == 128);

    require(throws<std::invalid_argument>([&] {
        single.box(0, 0, 8, 16);
    }));
    require(throws<std::invalid_argument>([&] {
        single.box(0, 0, 24, 9);
    }));
    require(throws<std::invalid_argument>([&] {
        single.line(0, 0, 0, zx::Axis::Horizontal);
    }));
    require(throws<std::invalid_argument>([&] {
        single.line(0, 0, 9, zx::Axis::Horizontal);
    }));
    require(throws<std::invalid_argument>([&] {
        single.line(0, 0, 16, static_cast<zx::Axis>(255));
    }));
    require(throws<std::invalid_argument>([&] {
        single.box(0, 0, 16, 16, static_cast<zx::Border>(255));
    }));
    require(throws<std::out_of_range>([&] {
        single.box(0, 0, 16, 16, zx::Border::Single,
                   zx::ColorIndex{16});
    }));
    require(throws<std::out_of_range>([&] {
        single.box(0, 0, 16, 16, zx::Border::Single,
                   zx::ColorIndex{1}, zx::ColorIndex{16});
    }));

    // Large or off-screen primitives must be clipped without unbounded loops.
    zx::Context clipped(width, height);
    clipped.box(-16, -16, 40, 40, zx::Border::Single);
    clipped.line(-100000, 16, 1000016, zx::Axis::Horizontal,
                 zx::Border::Single);
    const auto output = render(clipped);
    require(output.size() == width * height * 4);
    clipped.clear();
    const auto empty = render(clipped);
    require(std::all_of(empty.begin(), empty.end(), [](auto b) { return b == 0; }));
}


std::vector<std::uint8_t> renderPanelTest(const zx::Context& ui) {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(ui.width()) * ui.height() * 4, 0);
    ui.render({pixels, ui.width(), ui.height(), ui.width() * 4});
    return pixels;
}

void testPanels() {
    constexpr std::uint32_t w = 120, h = 88;
    const zx::Color border{10, 30, 50, 255};
    const zx::Color title{70, 90, 110, 255};
    const zx::Color body{220, 140, 30, 255};
    zx::Context bare(w, h);
    const auto area = bare.panel(8, 8, 80, 64, "TITLE", {}, border, title);
    require(area.x == 16 && area.y == 32 &&
            area.width == 64 && area.height == 32);
    const auto baseline = renderPanelTest(bare);

    zx::Context with_body(w, h);
    const auto body_area = with_body.panel(8, 8, 80, 64, "TITLE", {}, border, title);
    with_body.panelText(body_area,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ 1234567890 ABCDEFGHIJKLMNOPQRSTUVWXYZ", body);
    const auto output = renderPanelTest(with_body);
    bool ink_inside = false;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto i = offset(x, y, w * 4);
            const bool inside = x >= static_cast<std::uint32_t>(area.x) &&
                x < static_cast<std::uint32_t>(area.x) + area.width &&
                y >= static_cast<std::uint32_t>(area.y) &&
                y < static_cast<std::uint32_t>(area.y) + area.height;
            const bool changed = !std::equal(output.begin() + i, output.begin() + i + 4,
                                             baseline.begin() + i);
            if (inside) ink_inside |= changed;
            else require(!changed);
        }
    }
    require(ink_inside);

    // Different text alignments must match equivalent manually positioned glyphs.
    zx::Context centered(w, h), expected(w, h);
    const zx::PanelArea place{16, 16, 64, 24};
    centered.panelText(place, "AB", body, zx::TextAlign::Center);
    expected.text(40, 16, "AB", body);
    require(renderPanelTest(centered) == renderPanelTest(expected));
    centered.clear();
    expected.clear();
    centered.panelText(place, "AB", body, zx::TextAlign::Right);
    expected.text(64, 16, "AB", body);
    require(renderPanelTest(centered) == renderPanelTest(expected));

    // Title is clipped, too, even when wider than the panel.
    zx::Context long_title(w, h), short_title(w, h);
    (void)long_title.panel(8, 8, 80, 64, "A VERY LONG TITLE THAT DOES NOT FIT");
    (void)short_title.panel(8, 8, 80, 64, "A VERY L");
    require(renderPanelTest(long_title) == renderPanelTest(short_title));

    zx::PanelOptions with_padding;
    with_padding.padding = 4;
    with_padding.titleGap = 2;
    const auto padded = bare.panel(8, 8, 80, 64, "X", with_padding);
    require(padded.x == 20 && padded.y == 30 &&
            padded.width == 56 && padded.height == 30);

    // Small interior height clips individual glyph pixels, not only whole lines.
    zx::Context partial(w, h);
    partial.panelText({16, 16, 8, 4}, "W", body);
    const auto clipped = renderPanelTest(partial);
    zx::Context full(w, h);
    full.text(16, 16, "W", body);
    const auto full_pixels = renderPanelTest(full);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto i = offset(x, y, w * 4);
            if (y >= 16 && y < 20 && x >= 16 && x < 24) {
                require(clipped[i] == full_pixels[i]);
            } else {
                require(clipped[i + 3] == 0);
            }
        }
    }

    // Reveal cursor and text must both respect a panel's clipping rectangle.
    zx::Context revealed(w, h);
    zx::TextReveal reveal;
    reveal.setText(revealed, "HELLO WORLD", 16);
    reveal.finish();
    reveal.draw(revealed, zx::PanelArea{16, 16, 16, 8}, body);
    const auto reveal_pixels = renderPanelTest(revealed);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto i = offset(x, y, w * 4);
            if (x < 16 || x >= 32 || y < 16 || y >= 24) {
                require(reveal_pixels[i + 3] == 0);
            }
        }
    }

    // Indexed colors remain dynamic; selected font changes do not affect the frame.
    zx::Context indexed(w, h);
    auto index_area = indexed.panel(8, 8, 80, 64, "Кая", {},
        zx::ColorIndex{10}, zx::ColorIndex{15}, zx::ColorIndex{0});
    indexed.panelText(index_area, "Привет", zx::ColorIndex{12});
    indexed.setPalette(zx::Palette::VGA);
    const auto vga = renderPanelTest(indexed);
    indexed.setPalette(zx::Palette::MonoAmber);
    require(vga != renderPanelTest(indexed));

    zx::Context fonts(w, h);
    (void)fonts.panel(8, 8, 80, 64, "TITLE");
    const auto default_pixels = renderPanelTest(fonts);
    fonts.setFont(zx::Font::loadPsf2(ZXFONT_TEST_COMPACT_PATH));
    require(renderPanelTest(fonts) == default_pixels);

    // Validation must fail before queuing a partial frame.
    zx::Context invalid(w, h);
    require(throws<std::invalid_argument>([&] {
        (void)invalid.panel(0, 0, 24, 24, "TITLE");
    }));
    require(throws<std::invalid_argument>([&] {
        (void)invalid.panel(0, 0, 80, 64, "T\nA");
    }));
    require(throws<std::invalid_argument>([&] {
        (void)invalid.panel(0, 0, 80, 64, "T", {.border = static_cast<zx::Border>(255)});
    }));
    require(throws<std::invalid_argument>([&] {
        invalid.panelText({0, 0, 4, 8}, "A");
    }));
    require(throws<std::invalid_argument>([&] {
        invalid.panelText({0, 0, 32, 8}, "A", zx::Color{}, static_cast<zx::TextAlign>(255));
    }));
    require(throws<std::out_of_range>([&] {
        (void)invalid.panel(0, 0, 80, 64, "T", {},
                            zx::ColorIndex{16}, zx::ColorIndex{15});
    }));
    require(renderPanelTest(invalid) == std::vector<std::uint8_t>(w * h * 4, 0));
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
    testCompactFontAndFallback();
    testLetterSpacing();
    testTextLayout();
    testTextReveal();
    testPseudographics();
    testPanels();
}
