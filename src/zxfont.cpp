#include "zxfont/zxfont.hpp"

#include "utf8.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_map>
#include <variant>
#include <functional>
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

std::u32string decodeText(std::string_view value, const Font& font) {
    std::u32string decoded;
    decoded.reserve(value.size());
    std::size_t pos = 0;
    while (pos < value.size()) {
        const auto cp = detail::readCodepoint(value, pos);
        if (cp == U'\n' || (cp >= 0x20 && cp != 0x7F && font.hasGlyph(cp))) {
            decoded.push_back(cp);
        } else {
            throw std::invalid_argument("Control character or missing font glyph");
        }
    }
    return decoded;
}

std::uint64_t linePixels(std::size_t count, std::uint32_t glyph_width,
                         std::uint32_t spacing) {
    if (count == 0) return 0;
    return static_cast<std::uint64_t>(count) * glyph_width +
           static_cast<std::uint64_t>(count - 1) * spacing;
}

struct LayoutResult {
    std::u32string text;
    TextMetrics metrics;
};

LayoutResult layoutText(const std::u32string& input, const Font& font,
                        std::uint32_t spacing, std::uint32_t max_width) {
    const auto gw = font.glyphWidth();
    const auto gh = font.glyphHeight();
    if (max_width != 0 && max_width < gw) {
        throw std::invalid_argument("Text box narrower than glyph width");
    }
    if (input.empty()) return {};

    std::vector<std::u32string> lines;
    std::u32string line;
    auto emit = [&] { lines.push_back(std::move(line)); line.clear(); };
    auto fits = [&](std::size_t count) {
        return max_width == 0 || linePixels(count, gw, spacing) <= max_width;
    };

    std::size_t i = 0;
    while (i < input.size()) {
        if (input[i] == U'\n') {
            emit();
            ++i;
            continue;
        }
        // Preserve consecutive spaces within lines, but strip those at wraps.
        if (input[i] == U' ') {
            const auto begin = i;
            while (i < input.size() && input[i] == U' ') ++i;
            if (!line.empty()) {
                const auto amount = i - begin;
                if (fits(line.size() + amount)) {
                    line.append(amount, U' ');
                } else {
                    emit();
                }
            }
            continue;
        }
        const auto begin = i;
        while (i < input.size() && input[i] != U' ' && input[i] != U'\n') ++i;
        const auto word = input.substr(begin, i - begin);
        if (!line.empty() && !fits(line.size() + word.size())) {
            // Drop pending trailing separators on wrapped lines.
            while (!line.empty() && line.back() == U' ') line.pop_back();
            if (!line.empty()) emit();
        }
        for (const auto cp : word) {
            if (!fits(line.size() + 1)) {
                while (!line.empty() && line.back() == U' ') line.pop_back();
                if (!line.empty()) emit();
            }
            line.push_back(cp);
        }
    }
    emit();

    LayoutResult result;
    result.metrics.lines = lines.size();
    std::uint64_t widest = 0;
    for (std::size_t n = 0; n < lines.size(); ++n) {
        widest = std::max(widest, linePixels(lines[n].size(), gw, spacing));
        if (n != 0) result.text.push_back(U'\n');
        result.text += lines[n];
    }
    const std::uint64_t height = static_cast<std::uint64_t>(lines.size()) * gh;
    if (widest > std::numeric_limits<std::uint32_t>::max() ||
        height > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("Text metrics exceed uint32_t");
    }
    result.metrics.width = static_cast<std::uint32_t>(widest);
    result.metrics.height = static_cast<std::uint32_t>(height);
    return result;
}

// Box drawing uses the embedded font's glyphs on a fixed 8x8 grid.
constexpr std::int64_t kBorderCell = 8;
constexpr std::uint8_t kUp = 1;
constexpr std::uint8_t kRight = 2;
constexpr std::uint8_t kDown = 4;
constexpr std::uint8_t kLeft = 8;

void validateBorder(Border border) {
    switch (border) {
        case Border::Single:
        case Border::Double:
        case Border::Block: return;
    }
    throw std::invalid_argument("Unknown border style");
}

void validateAxis(Axis axis) {
    switch (axis) {
        case Axis::Horizontal:
        case Axis::Vertical: return;
    }
    throw std::invalid_argument("Unknown line axis");
}

// Signed floor division and ceil division by a positive divisor.
std::int64_t floorDiv(std::int64_t value, std::int64_t divisor) noexcept {
    const auto quotient = value / divisor;
    return quotient - (value % divisor < 0 ? 1 : 0);
}

struct VisibleCells {
    std::int64_t first;
    std::int64_t last;
};

VisibleCells visibleCells(std::int64_t origin, std::uint32_t count,
                          std::uint32_t limit) noexcept {
    // A cell intersects the canvas when its last pixel is >= 0 and
    // its first pixel is < limit, including partially clipped cells.
    const auto first = std::max<std::int64_t>(
        0, -floorDiv(7 + origin, kBorderCell));
    const auto last = std::min<std::int64_t>(
        static_cast<std::int64_t>(count) - 1,
        floorDiv(static_cast<std::int64_t>(limit) - 1 - origin, kBorderCell));
    return {first, last};
}

template <typename Callback>
void eachBoxCell(std::int64_t x, std::int64_t y,
                 std::uint32_t width, std::uint32_t height,
                 std::uint32_t canvas_width, std::uint32_t canvas_height,
                 Callback&& callback) {
    const auto columns = static_cast<std::int64_t>(width) / kBorderCell;
    const auto rows = static_cast<std::int64_t>(height) / kBorderCell;
    const auto visible_x = visibleCells(x, static_cast<std::uint32_t>(columns),
                                         canvas_width);
    const auto visible_y = visibleCells(y, static_cast<std::uint32_t>(rows),
                                         canvas_height);
    if (visible_x.first > visible_x.last || visible_y.first > visible_y.last) {
        return;
    }
    for (auto row = visible_y.first; row <= visible_y.last; ++row) {
        if (row != 0 && row != rows - 1) {
            // Interior rows can only contain the two vertical sides.
            if (visible_x.first == 0) {
                callback(x, y + row * kBorderCell, kUp | kDown);
            }
            if (visible_x.first <= columns - 1 &&
                visible_x.last >= columns - 1) {
                callback(x + (columns - 1) * kBorderCell,
                         y + row * kBorderCell, kUp | kDown);
            }
            continue;
        }
        for (auto col = visible_x.first; col <= visible_x.last; ++col) {
            std::uint8_t mask = 0;
            mask |= col == 0 ? kRight : col == columns - 1 ? kLeft
                            : kLeft | kRight;
            if (col == 0 || col == columns - 1) {
                mask |= row == 0 ? kDown : kUp;
            }
            callback(x + col * kBorderCell, y + row * kBorderCell, mask);
        }
    }
}

template <typename Callback>
void eachLineCell(std::int64_t x, std::int64_t y,
                  std::uint32_t length, Axis axis,
                  std::uint32_t canvas_width, std::uint32_t canvas_height,
                  Callback&& callback) {
    const auto count = length / kBorderCell;
    if (axis == Axis::Horizontal) {
        if (y + 7 < 0 || y >= canvas_height) return;
        const auto visible = visibleCells(x, count, canvas_width);
        for (auto i = visible.first; i <= visible.last; ++i) {
            callback(x + i * kBorderCell, y, i,
                     static_cast<std::int64_t>(count));
        }
    } else {
        if (x + 7 < 0 || x >= canvas_width) return;
        const auto visible = visibleCells(y, count, canvas_height);
        for (auto i = visible.first; i <= visible.last; ++i) {
            callback(x, y + i * kBorderCell, i,
                     static_cast<std::int64_t>(count));
        }
    }
}

struct StrokeKey {
    std::int64_t x;
    std::int64_t y;
    Border border;
    bool operator==(const StrokeKey&) const = default;
};

struct StrokeHash {
    std::size_t operator()(const StrokeKey& key) const noexcept {
        std::size_t h = std::hash<std::int64_t>{}(key.x);
        h ^= std::hash<std::int64_t>{}(key.y) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(static_cast<int>(key.border)) + 0x9e3779b9u +
             (h << 6) + (h >> 2);
        return h;
    }
};

struct StrokeCell {
    std::uint8_t mask = 0;
    std::size_t owner = 0;
};
using Strokes = std::unordered_map<StrokeKey, StrokeCell, StrokeHash>;

void addStroke(Strokes& strokes, std::int64_t x, std::int64_t y,
               Border border, std::uint8_t mask, std::size_t owner) {
    auto& cell = strokes[{x, y, border}];
    cell.mask |= mask;
    cell.owner = std::max(cell.owner, owner);
}

// The source font's U+2551 uses columns 3 and 5, while its double
// corners, tees and crosses use columns 2 and 4. A frame assembled from
// those glyphs therefore shifts by one pixel at every vertical join.
// Keep this corrected border-only mask independent of the text glyph.
constexpr std::array<std::uint8_t, 8> kAlignedDoubleVertical = {
    0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28
};

char32_t borderGlyph(Border border, std::uint8_t mask) noexcept {
    if (border == Border::Block) return U'█';
    const bool dbl = border == Border::Double;
    switch (mask) {
        case kUp | kRight: return dbl ? U'╚' : U'└';
        case kUp | kLeft: return dbl ? U'╝' : U'┘';
        case kDown | kRight: return dbl ? U'╔' : U'┌';
        case kDown | kLeft: return dbl ? U'╗' : U'┐';
        case kLeft | kRight: return dbl ? U'═' : U'─';
        case kUp | kDown: return dbl ? U'║' : U'│';
        case kUp | kLeft | kRight: return dbl ? U'╩' : U'┴';
        case kDown | kLeft | kRight: return dbl ? U'╦' : U'┬';
        case kUp | kDown | kRight: return dbl ? U'╠' : U'├';
        case kUp | kDown | kLeft: return dbl ? U'╣' : U'┤';
        case kUp | kDown | kLeft | kRight: return dbl ? U'╬' : U'┼';
        default: return (mask & (kLeft | kRight)) != 0
                            ? (dbl ? U'═' : U'─') : (dbl ? U'║' : U'│');
    }
}

void fillRectangle(Surface target, std::uint32_t canvas_width,
                   std::uint32_t canvas_height, std::int64_t x, std::int64_t y,
                   std::uint32_t width, std::uint32_t height,
                   Color color) noexcept {
    if (color.a == 0) return;
    const auto left = std::max<std::int64_t>(0, x);
    const auto top = std::max<std::int64_t>(0, y);
    const auto right = std::min<std::int64_t>(canvas_width, x + width);
    const auto bottom = std::min<std::int64_t>(canvas_height, y + height);
    for (auto py = top; py < bottom; ++py) {
        for (auto px = left; px < right; ++px) {
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

void Context::setFont(Font font) noexcept { font_ = std::move(font); }

void Context::setLetterSpacing(std::uint32_t pixels) noexcept {
    letter_spacing_ = pixels;
}

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
    auto decoded = decodeText(value, font_);
    commands_.push_back(TextCommand{x, y, std::move(decoded), color, font_,
                                    letter_spacing_, indexed, index});
}

TextMetrics Context::measureText(std::string_view value,
                                 std::uint32_t max_width) const {
    return layoutText(decodeText(value, font_), font_, letter_spacing_,
                      max_width).metrics;
}

void Context::textBox(int x, int y, std::uint32_t max_width,
                      std::string_view value, Color color) {
    appendBox(x, y, max_width, value, color, false, ColorIndex{0});
}

void Context::textBox(int x, int y, std::uint32_t max_width,
                      std::string_view value, ColorIndex color) {
    (void)paletteColor(palette_, color);
    appendBox(x, y, max_width, value, Color{}, true, color);
}

void Context::appendBox(int x, int y, std::uint32_t max_width,
                        std::string_view value, Color color, bool indexed,
                        ColorIndex index) {
    if (max_width == 0) {
        throw std::invalid_argument("Text box width must be positive");
    }
    auto laid_out = layoutText(decodeText(value, font_), font_,
                               letter_spacing_, max_width);
    commands_.push_back(TextCommand{x, y, std::move(laid_out.text), color,
                                    font_, letter_spacing_, indexed, index});
}

void Context::box(int x, int y, std::uint32_t width, std::uint32_t height,
                  Border border, Color color, std::optional<Color> fill) {
    validateBorder(border);
    if (width < 16 || height < 16 || width % 8 != 0 || height % 8 != 0) {
        throw std::invalid_argument("Box size must be >=16 and divisible by 8");
    }
    commands_.emplace_back(BoxCommand{x, y, width, height, border, color,
                                      fill, false, {0}, std::nullopt});
}

void Context::box(int x, int y, std::uint32_t width, std::uint32_t height,
                  Border border, ColorIndex color,
                  std::optional<ColorIndex> fill) {
    validateBorder(border);
    (void)paletteColor(palette_, color);
    if (fill) (void)paletteColor(palette_, *fill);
    if (width < 16 || height < 16 || width % 8 != 0 || height % 8 != 0) {
        throw std::invalid_argument("Box size must be >=16 and divisible by 8");
    }
    commands_.emplace_back(BoxCommand{x, y, width, height, border, {},
                                      std::nullopt, true, color, fill});
}

void Context::line(int x, int y, std::uint32_t length, Axis axis,
                   Border border, Color color) {
    validateBorder(border);
    validateAxis(axis);
    if (length < 8 || length % 8 != 0) {
        throw std::invalid_argument("Line length must be positive and divisible by 8");
    }
    commands_.emplace_back(LineCommand{x, y, length, axis, border, color,
                                       false, {0}});
}

void Context::line(int x, int y, std::uint32_t length, Axis axis,
                   Border border, ColorIndex color) {
    validateBorder(border);
    validateAxis(axis);
    (void)paletteColor(palette_, color);
    if (length < 8 || length % 8 != 0) {
        throw std::invalid_argument("Line length must be positive and divisible by 8");
    }
    commands_.emplace_back(LineCommand{x, y, length, axis, border, {},
                                       true, color});
}

void Context::clear() noexcept { commands_.clear(); }

void Context::render(Surface target) const {
    validateSurface(target, width_, height_);

    // Collect connections once per render, independently of text fonts.
    // First add boxes, so line endpoints can join their vertical/horizontal
    // sides as tees rather than continuing outside the border.
    Strokes strokes;
    for (std::size_t index = 0; index < commands_.size(); ++index) {
        const auto& command = commands_[index];
        if (const auto* box = std::get_if<BoxCommand>(&command)) {
            eachBoxCell(box->x, box->y, box->width, box->height,
                        width_, height_, [&](auto x, auto y, auto mask) {
                            addStroke(strokes, x, y, box->border, mask, index);
                        });
        }
    }
    for (std::size_t index = 0; index < commands_.size(); ++index) {
        const auto& command = commands_[index];
        if (const auto* line = std::get_if<LineCommand>(&command)) {
            eachLineCell(line->x, line->y, line->length, line->axis,
                         width_, height_, [&](auto x, auto y, auto cell_index, auto count) {
                std::uint8_t mask = line->axis == Axis::Horizontal
                                        ? kLeft | kRight : kUp | kDown;
                if (count > 1) {
                    const auto found = strokes.find({x, y, line->border});
                    const auto existing = found == strokes.end()
                                              ? std::uint8_t{0} : found->second.mask;
                    if (line->axis == Axis::Horizontal &&
                        (existing & (kUp | kDown)) != 0) {
                        if (cell_index == 0) mask = kRight;
                        if (cell_index == count - 1) mask = kLeft;
                    } else if (line->axis == Axis::Vertical &&
                               (existing & (kLeft | kRight)) != 0) {
                        if (cell_index == 0) mask = kDown;
                        if (cell_index == count - 1) mask = kUp;
                    }
                }
                addStroke(strokes, x, y, line->border, mask, index);
            });
        }
    }

    const Font graphic_font;  // Embedded 8x8 Spectrum Unicode.
    auto drawStroke = [&](std::int64_t x, std::int64_t y, Border border,
                          Color color, std::size_t index) {
        const auto it = strokes.find({x, y, border});
        // The last contributing primitive paints the joined glyph once.
        // This avoids applying semi-transparent stroke alpha multiple times.
        if (it == strokes.end() || it->second.owner != index) return;
        const auto* glyph = border == Border::Double &&
                                    it->second.mask == (kUp | kDown)
                                ? kAlignedDoubleVertical.data()
                                : graphic_font.bitmap(borderGlyph(border, it->second.mask));
        drawGlyph(target, width_, height_, x, y, glyph, 8, 8,
                  graphic_font.bytesPerRow(), color);
    };

    for (std::size_t index = 0; index < commands_.size(); ++index) {
        const auto& command = commands_[index];
        if (const auto* box = std::get_if<BoxCommand>(&command)) {
            if (box->indexed ? box->fill_index.has_value()
                             : box->fill.has_value()) {
                const auto fill = box->indexed
                                      ? paletteColor(palette_, *box->fill_index)
                                      : *box->fill;
                fillRectangle(target, width_, height_, box->x, box->y,
                              box->width, box->height, fill);
            }
            const auto color = box->indexed ? paletteColor(palette_, box->index)
                                            : box->color;
            eachBoxCell(box->x, box->y, box->width, box->height,
                        width_, height_, [&](auto x, auto y, auto) {
                            drawStroke(x, y, box->border, color, index);
                        });
        } else if (const auto* line = std::get_if<LineCommand>(&command)) {
            const auto color = line->indexed ? paletteColor(palette_, line->index)
                                             : line->color;
            eachLineCell(line->x, line->y, line->length, line->axis,
                         width_, height_, [&](auto x, auto y, auto, auto) {
                drawStroke(x, y, line->border, color, index);
            });
        } else {
            const auto& text = std::get<TextCommand>(command);
            std::int64_t x = text.x;
            std::int64_t y = text.y;
            const auto color = text.indexed ? paletteColor(palette_, text.index)
                                            : text.color;
            const auto glyph_width = text.font.glyphWidth();
            const auto glyph_height = text.font.glyphHeight();
            for (std::size_t i = 0; i < text.value.size(); ++i) {
                const auto codepoint = text.value[i];
                if (codepoint == U'\n') {
                    x = text.x;
                    advance(y, glyph_height);
                    continue;
                }
                if (x < width_ && x > -static_cast<std::int64_t>(glyph_width) &&
                    y < height_ && y > -static_cast<std::int64_t>(glyph_height)) {
                    drawGlyph(target, width_, height_, x, y,
                              text.font.bitmap(codepoint), glyph_width,
                              glyph_height, text.font.bytesPerRow(), color);
                }
                advance(x, glyph_width);
                if (i + 1 < text.value.size() && text.value[i + 1] != U'\n') {
                    advance(x, text.letter_spacing);
                }
            }
        }
    }
}

}  // namespace zx
