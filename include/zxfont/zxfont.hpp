#ifndef ZXFONT_ZXFONT_HPP
#define ZXFONT_ZXFONT_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace zx {

/** @brief Straight-alpha RGBA color with 8-bit channels. */
struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

/**
 * @brief Non-owning, top-to-bottom, left-to-right RGBA8 pixel surface.
 *
 * Every row begins stride_bytes after the previous row. The span must contain
 * the bytes required by the visible rectangle, including intervening padding.
 */
struct Surface {
    std::span<std::uint8_t> pixels;
    std::uint32_t width;
    std::uint32_t height;
    std::size_t stride_bytes;
};

/**
 * @brief Immutable, shareable monospace bitmap font.
 *
 * Copies share glyph data. The default font contains ASCII and common
 * Cyrillic glyphs; external PSF2 fonts require a Unicode character table.
 */
class Font {
public:
    /** @brief Construct the built-in 8x8 ASCII/Cyrillic font. */
    Font();

    /**
     * @brief Load a PSF2 font with a Unicode mapping table from disk.
     *
     * Accepts glyphs up to 256x256 and files up to 16 MiB. PSF2 character
     * sequences are skipped because ZXFont draws individual codepoints only.
     *
     * @throws std::runtime_error If the file cannot be read.
     * @throws std::invalid_argument If the font data is malformed or unsupported.
     */
    [[nodiscard]] static Font loadPsf2(const std::filesystem::path& path);

    /** @brief Glyph cell width in pixels. */
    [[nodiscard]] std::uint32_t glyphWidth() const noexcept;

    /** @brief Glyph cell height in pixels. */
    [[nodiscard]] std::uint32_t glyphHeight() const noexcept;

    /** @brief Return whether the font contains a Unicode codepoint. */
    [[nodiscard]] bool hasGlyph(char32_t codepoint) const noexcept;

private:
    struct Data;
    std::shared_ptr<const Data> data_;

    explicit Font(std::shared_ptr<const Data> data) noexcept;
    [[nodiscard]] const std::uint8_t* bitmap(char32_t codepoint) const noexcept;
    [[nodiscard]] std::size_t bytesPerRow() const noexcept;

    friend class Context;
};

/**
 * @brief Stores text drawing commands for a logical pixel-sized canvas.
 *
 * This context does not own a framebuffer or an application window. Rendering
 * draws only foreground glyph pixels into an existing RGBA8 surface.
 */
class Context {
public:
    /** @brief Create a logical canvas; width and height must be nonzero. */
    Context(std::uint32_t width, std::uint32_t height);

    /** @brief Logical canvas width in pixels. */
    [[nodiscard]] std::uint32_t width() const noexcept;

    /** @brief Logical canvas height in pixels. */
    [[nodiscard]] std::uint32_t height() const noexcept;

    /**
     * @brief Select a font for subsequently queued text commands.
     *
     * Previously queued commands keep their own fonts, so changing fonts
     * never changes the appearance of already queued text.
     */
    void setFont(Font font) noexcept;

    /**
     * @brief Add UTF-8 text at pixel coordinates, using the selected font.
     *
     * LF resets x and advances y by the active font's glyph height. Only
     * glyphs in the selected font can be drawn; no implicit substitutions.
     *
     * @throws std::invalid_argument For invalid UTF-8, unsupported glyphs,
     *                               or control characters other than LF.
     */
    void text(int x, int y, std::string_view value, Color color = {});

    /** @brief Remove all queued drawing commands without changing any pixels. */
    void clear() noexcept;

    /**
     * @brief Draw queued glyphs onto the caller's RGBA8 surface.
     *
     * Pixels outside glyphs are not modified. Coordinates are clipped against
     * the logical canvas. Semi-transparent colors use source-over blending.
     * The surface must be at least the logical canvas size.
     *
     * @throws std::invalid_argument If the surface description is invalid.
     * @throws std::overflow_error If the surface byte extent overflows size_t.
     */
    void render(Surface target) const;

private:
    struct TextCommand {
        int x;
        int y;
        std::u32string value;
        Color color;
        Font font;
    };

    std::uint32_t width_;
    std::uint32_t height_;
    Font font_;
    std::vector<TextCommand> commands_;
};

}  // namespace zx

#endif  // ZXFONT_ZXFONT_HPP
