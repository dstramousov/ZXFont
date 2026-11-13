#include "zxfont/zxfont.hpp"

#include "utf8.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace zx {
namespace {

constexpr std::size_t kChannelsPerPixel = 4;

void validateSurface(const Surface& target, std::uint32_t required_width,
                     std::uint32_t required_height) {
    if (target.width < required_width || target.height < required_height ||
        target.width == 0 || target.height == 0) {
        throw std::invalid_argument("Surface dimensions are smaller than the canvas");
    }

    const auto max = std::numeric_limits<std::size_t>::max();
    if (target.width > max / kChannelsPerPixel) {
        throw std::overflow_error("Surface row width overflow");
    }

    const auto visible_row_bytes = static_cast<std::size_t>(target.width) *
                                   kChannelsPerPixel;
    if (target.stride_bytes < visible_row_bytes) {
        throw std::invalid_argument("Surface stride is smaller than its pixel width");
    }

    const auto prior_rows = static_cast<std::size_t>(target.height) - 1;
    if (prior_rows != 0 &&
        target.stride_bytes > (max - visible_row_bytes) / prior_rows) {
        throw std::overflow_error("Surface byte extent overflow");
    }

    const auto required_bytes = prior_rows * target.stride_bytes + visible_row_bytes;
    if (target.pixels.size() < required_bytes) {
        throw std::invalid_argument("Surface pixel span is too small");
    }
}

void blendPixel(std::uint8_t* destination, Color source) noexcept {
    if (source.a == 0) {
        return;
    }
    if (source.a == 255) {
        destination[0] = source.r;
        destination[1] = source.g;
        destination[2] = source.b;
        destination[3] = 255;
        return;
    }

    const std::uint32_t alpha = source.a;
    const std::uint32_t dest_alpha = destination[3];
    const std::uint32_t inv_alpha = 255 - alpha;
    const std::uint32_t dest_weight = (dest_alpha * inv_alpha + 127) / 255;
    const std::uint32_t out_alpha = alpha + dest_weight;

    const std::uint8_t src_channels[] = {source.r, source.g, source.b};
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const std::uint32_t weighted_src = src_channels[channel] * alpha;
        const std::uint32_t weighted_dst =
            (destination[channel] * dest_alpha * inv_alpha + 127) / 255;
        destination[channel] = static_cast<std::uint8_t>(
            (weighted_src + weighted_dst + out_alpha / 2) / out_alpha);
    }
    destination[3] = static_cast<std::uint8_t>(out_alpha);
}

void drawGlyph(Surface target, std::uint32_t canvas_width,
               std::uint32_t canvas_height, std::int64_t x, std::int64_t y,
               const std::uint8_t* glyph, std::uint32_t glyph_width,
               std::uint32_t glyph_height, std::size_t bytes_per_row,
               Color color) noexcept {
    for (std::uint32_t row = 0; row < glyph_height; ++row) {
        const auto py = y + row;
        if (py < 0 || py >= canvas_height) {
            continue;
        }
        const auto* row_bits = glyph + static_cast<std::size_t>(row) * bytes_per_row;
        for (std::uint32_t col = 0; col < glyph_width; ++col) {
            const auto px = x + col;
            if (px < 0 || px >= canvas_width ||
                (row_bits[col / 8] & (0x80u >> (col % 8))) == 0) {
                continue;
            }
            const auto offset = static_cast<std::size_t>(py) * target.stride_bytes +
                                static_cast<std::size_t>(px) * kChannelsPerPixel;
            blendPixel(target.pixels.data() + offset, color);
        }
    }
}

// Avoid overflowing signed pixel positions when a huge command is clipped.
void advance(std::int64_t& position, std::uint32_t delta) noexcept {
    const auto max = std::numeric_limits<std::int64_t>::max();
    if (position > max - delta) {
        position = max;
    } else {
        position += delta;
    }
}

}  // namespace

Context::Context(std::uint32_t width, std::uint32_t height)
    : width_(width), height_(height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Context dimensions must be nonzero");
    }
}

std::uint32_t Context::width() const noexcept { return width_; }
std::uint32_t Context::height() const noexcept { return height_; }

void Context::setFont(Font font) noexcept { font_ = std::move(font); }

bool Context::setFont(const std::filesystem::path& path, std::string* error) {
    try {
        Font loaded = Font::loadPsf2(path);
        setFont(std::move(loaded));
        if (error) {
            error->clear();
        }
        return true;
    } catch (const std::invalid_argument& failure) {
        setFont(Font{});
        if (error) {
            *error = failure.what();
        }
    } catch (const std::runtime_error& failure) {
        setFont(Font{});
        if (error) {
            *error = failure.what();
        }
    }
    return false;
}

void Context::setPalette(Palette palette) {
    // Validate before changing the selected palette.
    (void)paletteColor(palette, ColorIndex{0});
    palette_ = palette;
}

void Context::text(int x, int y, std::string_view value, Color color) {
    appendText(x, y, value, color, false, ColorIndex{0});
}

void Context::text(int x, int y, std::string_view value, ColorIndex color) {
    (void)paletteColor(palette_, color);
    appendText(x, y, value, Color{}, true, color);
}

void Context::appendText(int x, int y, std::string_view value, Color color,
                         bool indexed, ColorIndex index) {
    std::u32string decoded;
    decoded.reserve(value.size());
    std::size_t pos = 0;
    while (pos < value.size()) {
        const auto codepoint = detail::readCodepoint(value, pos);
        if (codepoint == U'\n' || (codepoint >= 0x20 && codepoint != 0x7F &&
                                     font_.hasGlyph(codepoint))) {
            decoded.push_back(codepoint);
        } else {
            throw std::invalid_argument("Control character or missing font glyph");
        }
    }
    commands_.push_back(TextCommand{x, y, std::move(decoded), color, font_,
                                    indexed, index});
}

void Context::clear() noexcept { commands_.clear(); }

void Context::render(Surface target) const {
    validateSurface(target, width_, height_);
    for (const auto& command : commands_) {
        std::int64_t x = command.x;
        std::int64_t y = command.y;
        const auto color = command.indexed ? paletteColor(palette_, command.index)
                                           : command.color;
        const auto glyph_width = command.font.glyphWidth();
        const auto glyph_height = command.font.glyphHeight();
        for (const auto codepoint : command.value) {
            if (codepoint == U'\n') {
                x = command.x;
                advance(y, glyph_height);
                continue;
            }
            if (x < width_ && x + glyph_width > 0 &&
                y < height_ && y + glyph_height > 0) {
                drawGlyph(target, width_, height_, x, y,
                          command.font.bitmap(codepoint), glyph_width,
                          glyph_height, command.font.bytesPerRow(), color);
            }
            advance(x, glyph_width);
        }
    }
}

}  // namespace zx
