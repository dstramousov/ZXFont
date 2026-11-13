#ifndef ZXFONT_ZXFONT_HPP
#define ZXFONT_ZXFONT_HPP

#include <cstddef>
#include <cstdint>
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
     * @brief Add a text command at pixel coordinates.
     *
     * Supports printable ASCII and LF. Newlines reset x and advance y by 8.
     * All other control bytes and non-ASCII input are rejected for now.
     * The built-in public-domain font has fixed 8x8 glyphs.
     *
     * @throws std::invalid_argument If text contains unsupported bytes.
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
        std::string value;
        Color color;
    };

    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<TextCommand> commands_;
};

}  // namespace zx

#endif  // ZXFONT_ZXFONT_HPP
