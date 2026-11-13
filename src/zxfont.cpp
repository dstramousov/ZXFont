#include "zxfont/zxfont.hpp"

#include "basic_ascii_8x8.hpp"

#include <limits>
#include <stdexcept>

namespace zx {
namespace {

constexpr std::size_t kChannelsPerPixel = 4;
constexpr std::int64_t kGlyphWidth = 8;
constexpr std::int64_t kGlyphHeight = 8;

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
               unsigned char glyph, Color color) noexcept {
    for (std::int64_t row = 0; row < kGlyphHeight; ++row) {
        const auto py = y + row;
        if (py < 0 || py >= canvas_height) {
            continue;
        }
        const auto bits = detail::kBasicAscii8x8[glyph - 32][row];
        for (std::int64_t col = 0; col < kGlyphWidth; ++col) {
            const auto px = x + col;
            if (px < 0 || px >= canvas_width || (bits & (1u << col)) == 0) {
                continue;
            }
            const auto offset = static_cast<std::size_t>(py) * target.stride_bytes +
                                static_cast<std::size_t>(px) * kChannelsPerPixel;
            blendPixel(target.pixels.data() + offset, color);
        }
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

void Context::text(int x, int y, std::string_view value, Color color) {
    for (const auto ch : value) {
        const auto byte = static_cast<unsigned char>(ch);
        if ((byte < 32 || byte > 126) && byte != '\n') {
            throw std::invalid_argument("Only printable ASCII and LF are supported");
        }
    }
    commands_.push_back(TextCommand{x, y, std::string(value), color});
}

void Context::clear() noexcept { commands_.clear(); }

void Context::render(Surface target) const {
    validateSurface(target, width_, height_);
    for (const auto& command : commands_) {
        std::int64_t x = command.x;
        std::int64_t y = command.y;
        for (const auto ch : command.value) {
            if (ch == '\n') {
                x = command.x;
                y += kGlyphHeight;
                continue;
            }
            if (x < width_ && x + kGlyphWidth > 0 &&
                y < height_ && y + kGlyphHeight > 0) {
                drawGlyph(target, width_, height_, x, y,
                          static_cast<unsigned char>(ch), command.color);
            }
            x += kGlyphWidth;
        }
    }
}

}  // namespace zx
