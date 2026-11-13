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
}
