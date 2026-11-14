#ifndef ZXFONT_ZXFONT_HPP
#define ZXFONT_ZXFONT_HPP

#include <cstddef>
#include <utility>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

namespace zx {

/** @brief Straight-alpha RGBA color with 8-bit channels. */
struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

/** @brief Built-in 16-color palette, independent of glyph bitmaps. */
enum class Palette { MonoGreen, MonoAmber, Spectrum, VGA };

/** @brief Color index (0..15) resolved from the active palette at render time. */
struct ColorIndex {
    std::uint8_t value;
};

/** @brief Built-in 8x8 box-drawing styles independent of the text font. */
enum class Border { Single, Double, Block };

/** @brief Orientation of a pseudographic line. */
enum class Axis { Horizontal, Vertical };

/**
 * @brief Resolve a palette entry to an RGBA color.
 *
 * @throws std::out_of_range If the index is outside [0, 15].
 * @throws std::invalid_argument If the palette identifier is unknown.
 */
[[nodiscard]] Color paletteColor(Palette palette, ColorIndex color);

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
 * Copies share glyph data. The default font embeds ZX Spectrum Unicode
 * 8x8 (667 mappings); external PSF2 fonts require a Unicode table.
 */
class Font {
public:
    /** @brief Construct the built-in ZX Spectrum Unicode 8x8 font. */
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
    [[nodiscard]] static std::shared_ptr<const Data> parsePsf2(
        std::span<const std::uint8_t> bytes);
    [[nodiscard]] const std::uint8_t* bitmap(char32_t codepoint) const noexcept;
    [[nodiscard]] std::size_t bytesPerRow() const noexcept;

    friend class Context;
};

/** @brief Pixel dimensions and line count of laid-out UTF-8 text. */
struct TextMetrics {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t lines = 0;
};

/** @brief Defaults for timed text reveal and its solid block cursor. */
struct RevealOptions {
    double charsPerSecond = 30.0;  // Zero reveals all text immediately.
    bool cursorEnabled = true;
    std::uint32_t cursorGapPx = 1;
    double cursorLead = 0.4;  // Fraction of the interval before the next glyph.
    bool cursorBlink = false;
    bool cursorAfterComplete = true;
    ColorIndex cursorColor{10};
};

class TextReveal;

/**
 * @brief Stores text drawing commands for a logical pixel-sized canvas.
 *
 * This context does not own a framebuffer or an application window. Rendering
 * draws text and pseudographic primitives into an existing RGBA8 surface.
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
     * @brief Set extra horizontal pixels between neighboring glyphs.
     *
     * Spacing applies between all printable glyphs, including spaces, but
     * never after the final glyph of a line or across LF. Each queued text
     * command captures the current spacing independently of later changes.
     * The default is zero (the historical monospace advance).
     */
    void setLetterSpacing(std::uint32_t pixels) noexcept;

    /**
     * @brief Select an external PSF2 font with built-in fallback.
     *
     * On a missing, unreadable, or invalid file, selects the embedded ZX
     * Spectrum Unicode 8x8 font and returns false. Previously queued commands
     * retain their fonts. The optional error string receives a diagnostic on
     * failure and is cleared on success. Allocation failures still propagate.
     *
     * @return True if the external font was loaded, false if fallback was used.
     */
    [[nodiscard]] bool setFont(const std::filesystem::path& path,
                               std::string* error = nullptr);

    /**
     * @brief Select a palette for indexed text commands.
     *
     * Previously queued indexed commands are recolored on the next render.
     * Commands with explicit RGBA colors are never affected.
     */
    void setPalette(Palette palette);

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

    /**
     * @brief Queue palette-indexed UTF-8 text, resolved on every render.
     *
     * @throws std::out_of_range If the index is outside [0, 15].
     * @throws std::invalid_argument For invalid UTF-8 or missing glyphs.
     */
    void text(int x, int y, std::string_view value, ColorIndex color);

    /**
     * @brief Measure UTF-8 text using the current font and letter spacing.
     *
     * A zero max_width means no automatic wrapping (explicit LF is honored).
     * A positive max_width wraps at spaces and splits oversized words. Spaces
     * at wrap boundaries are discarded; explicit LF preserves empty lines.
     * Empty text has zero width, height and lines.
     *
     * @throws std::invalid_argument For invalid text or too-narrow max_width.
     * @throws std::overflow_error If the measured dimensions exceed uint32_t.
     */
    [[nodiscard]] TextMetrics measureText(std::string_view value,
                                          std::uint32_t max_width = 0) const;

    /**
     * @brief Queue wrapped text inside a pixel-width text box.
     *
     * Uses the same layout as measureText(value, max_width). x/y are the top
     * left of the first line. Text is clipped to the Context canvas, but not
     * vertically clipped to an implicit box height.
     */
    void textBox(int x, int y, std::uint32_t max_width,
                 std::string_view value, Color color = {});

    /** @brief Palette-indexed overload of textBox. */
    void textBox(int x, int y, std::uint32_t max_width,
                 std::string_view value, ColorIndex color);

    /**
     * @brief Queue a pixel-positioned border and optional filled background.
     *
     * Width and height must be multiples of 8, each at least 16 pixels.
     * Borders use the embedded Spectrum Unicode 8x8 glyphs regardless of the
     * currently selected text font or its letter spacing. The optional fill
     * covers the complete rectangle; subsequent commands can draw on top.
     *
     * @throws std::invalid_argument For invalid dimensions or border style.
     */
    void box(int x, int y, std::uint32_t width, std::uint32_t height,
             Border border = Border::Single, Color color = {},
             std::optional<Color> fill = std::nullopt);

    /** @brief Palette-indexed box, recolored with setPalette(). */
    void box(int x, int y, std::uint32_t width, std::uint32_t height,
             Border border, ColorIndex color,
             std::optional<ColorIndex> fill = std::nullopt);

    /**
     * @brief Queue an 8-pixel-grid line, joining matching box borders.
     *
     * Length must be a positive multiple of 8. Intersection glyphs are
     * automatically chosen for crossing lines and compatible borders.
     * Shapes join when their 8x8 cell coordinates and border styles match.
     *
     * @throws std::invalid_argument For invalid length, axis or border style.
     */
    void line(int x, int y, std::uint32_t length, Axis axis,
              Border border = Border::Single, Color color = {});

    /** @brief Palette-indexed line, recolored with setPalette(). */
    void line(int x, int y, std::uint32_t length, Axis axis,
              Border border, ColorIndex color);

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
        std::uint32_t letter_spacing = 0;
        bool indexed = false;
        ColorIndex index{0};
    };

    struct BoxCommand {
        int x;
        int y;
        std::uint32_t width;
        std::uint32_t height;
        Border border;
        Color color;
        std::optional<Color> fill;
        bool indexed = false;
        ColorIndex index{0};
        std::optional<ColorIndex> fill_index;
    };

    struct CursorCommand {
        std::int64_t x;
        std::int64_t y;
        std::uint32_t width;
        std::uint32_t height;
        ColorIndex color;
    };

    struct LineCommand {
        int x;
        int y;
        std::uint32_t length;
        Axis axis;
        Border border;
        Color color;
        bool indexed = false;
        ColorIndex index{0};
    };

    void appendText(int x, int y, std::string_view value, Color color,
                    bool indexed, ColorIndex index);
    void appendBox(int x, int y, std::uint32_t max_width,
                   std::string_view value, Color color, bool indexed,
                   ColorIndex index);

    std::uint32_t width_;
    std::uint32_t height_;
    Font font_;
    std::uint32_t letter_spacing_ = 0;
    Palette palette_ = Palette::MonoGreen;
    std::vector<std::variant<TextCommand, BoxCommand, LineCommand, CursorCommand>> commands_;

    friend class TextReveal;
};


/**
 * @brief Time-driven UTF-8 text reveal independent of rendering and frame rate.
 *
 * setText() captures the current Context font, spacing, and fully wrapped
 * layout. update() advances printable Unicode codepoints, never UTF-8 bytes.
 * The caller owns time, the window, and the framebuffer. Each TextReveal has
 * independent progress and options; the default constructor is ready to use.
 */
class TextReveal {
public:
    /** @brief Create a reveal with validated options and no text. */
    explicit TextReveal(RevealOptions options = {});

    /**
     * @brief Prepare and reset a message using the current Context typography.
     *
     * The width must be nonzero. Changes to the Context font or spacing do not
     * affect this message until setText() is called again.
     * Invalid text and unsupported glyphs throw std::invalid_argument.
     */
    void setText(const Context& context, std::string_view value,
                 std::uint32_t max_width);

    /** @brief Set options without resetting already revealed characters. */
    void setOptions(RevealOptions options);

    /** @brief Return the current configuration. */
    [[nodiscard]] const RevealOptions& options() const noexcept;

    /**
     * @brief Advance by elapsed real time in seconds (not frames).
     *
     * Rejects negative, NaN, or infinite time deltas. Speed zero is instant.
     */
    void update(double elapsed_seconds);

    /** @brief Restart the prepared message without changing its layout. */
    void reset() noexcept;

    /** @brief Reveal the entire message immediately. */
    void finish() noexcept;

    /** @brief True when every printable glyph is visible. */
    [[nodiscard]] bool finished() const noexcept;

    /** @brief Number of revealed printable Unicode codepoints. */
    [[nodiscard]] std::size_t visibleCharacters() const noexcept;

    /** @brief Dimensions of the complete precomputed layout. */
    [[nodiscard]] TextMetrics metrics() const noexcept;

    /** @brief Queue visible text and an optional cursor with an RGBA text color. */
    void draw(Context& context, int x, int y, Color color = {}) const;

    /** @brief Queue visible text and an optional cursor with indexed text color. */
    void draw(Context& context, int x, int y, ColorIndex color) const;

private:
    void queue(Context& context, int x, int y, Color color,
               bool indexed, ColorIndex index) const;

    RevealOptions options_;
    Font font_;
    std::uint32_t spacing_ = 0;
    std::uint32_t max_width_ = 0;
    std::u32string laid_out_;
    TextMetrics metrics_;
    std::vector<std::pair<std::int64_t, std::int64_t>> positions_;
    std::size_t visible_ = 0;
    double fractional_ = 0.0;
    double blink_phase_ = 0.0;
};

}  // namespace zx

#endif  // ZXFONT_ZXFONT_HPP
