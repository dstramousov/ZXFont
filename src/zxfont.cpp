#include "zxfont/zxfont.hpp"

#include "utf8.hpp"

#include <algorithm>
#include <cmath>
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
               Color color, std::optional<PanelArea> clip = std::nullopt) noexcept {
    for (std::uint32_t row = 0; row < glyph_height; ++row) {
        const auto py = y + row;
        if (py < 0 || py >= canvas_height ||
            (clip && (py < clip->y ||
                      py >= static_cast<std::int64_t>(clip->y) + clip->height))) {
            continue;
        }
        const auto* row_bits = glyph + static_cast<std::size_t>(row) * bytes_per_row;
        for (std::uint32_t col = 0; col < glyph_width; ++col) {
            const auto px = x + col;
            if (px < 0 || px >= canvas_width ||
                (clip && (px < clip->x ||
                          px >= static_cast<std::int64_t>(clip->x) + clip->width)) ||
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
                   Color color, std::optional<PanelArea> clip = std::nullopt) noexcept {
    if (color.a == 0) return;
    const auto left = std::max<std::int64_t>(clip ? clip->x : 0, std::max<std::int64_t>(0, x));
    const auto top = std::max<std::int64_t>(clip ? clip->y : 0, std::max<std::int64_t>(0, y));
    const auto right = std::min<std::int64_t>(
        clip ? static_cast<std::int64_t>(clip->x) + clip->width : canvas_width,
        std::min<std::int64_t>(canvas_width, x + width));
    const auto bottom = std::min<std::int64_t>(
        clip ? static_cast<std::int64_t>(clip->y) + clip->height : canvas_height,
        std::min<std::int64_t>(canvas_height, y + height));
    for (auto py = top; py < bottom; ++py) {
        for (auto px = left; px < right; ++px) {
            const auto offset = static_cast<std::size_t>(py) * target.stride_bytes +
                                static_cast<std::size_t>(px) * kChannelsPerPixel;
            blendPixel(target.pixels.data() + offset, color);
        }
    }
}

void validateAlign(TextAlign align) {
    switch (align) {
        case TextAlign::Left:
        case TextAlign::Center:
        case TextAlign::Right: return;
    }
    throw std::invalid_argument("Unknown text alignment");
}

int checkedCoordinate(std::int64_t value) {
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
        throw std::overflow_error("Panel coordinate exceeds int range");
    }
    return static_cast<int>(value);
}

std::int64_t alignedX(const PanelArea& area, std::uint64_t line_width,
                      TextAlign align) noexcept {
    if (line_width >= area.width) return area.x;
    const auto leftover = static_cast<std::int64_t>(area.width - line_width);
    if (align == TextAlign::Center) return static_cast<std::int64_t>(area.x) + leftover / 2;
    if (align == TextAlign::Right) return static_cast<std::int64_t>(area.x) + leftover;
    return area.x;
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
                                    letter_spacing_, indexed, index, std::nullopt});
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
                                    font_, letter_spacing_, indexed, index, std::nullopt});
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

PanelArea Context::panel(int x, int y, std::uint32_t width,
                         std::uint32_t height, std::string_view title,
                         PanelOptions options, Color border_color,
                         Color title_color, std::optional<Color> fill) {
    return appendPanel(x, y, width, height, title, options, border_color,
                       title_color, false, ColorIndex{0}, ColorIndex{0},
                       fill, std::nullopt);
}

PanelArea Context::panel(int x, int y, std::uint32_t width,
                         std::uint32_t height, std::string_view title,
                         PanelOptions options, ColorIndex border_color,
                         ColorIndex title_color,
                         std::optional<ColorIndex> fill) {
    (void)paletteColor(palette_, border_color);
    (void)paletteColor(palette_, title_color);
    if (fill) (void)paletteColor(palette_, *fill);
    return appendPanel(x, y, width, height, title, options, Color{},
                       Color{}, true, border_color, title_color,
                       std::nullopt, fill);
}

PanelArea Context::appendPanel(int x, int y, std::uint32_t width,
                               std::uint32_t height, std::string_view title,
                               PanelOptions options, Color border_color,
                               Color title_color, bool indexed,
                               ColorIndex border_index, ColorIndex title_index,
                               std::optional<Color> fill,
                               std::optional<ColorIndex> fill_index) {
    validateBorder(options.border);
    validateAlign(options.titleAlign);
    if (width < 16 || height < 16 || width % 8 != 0 || height % 8 != 0) {
        throw std::invalid_argument("Panel dimensions must be multiples of 8");
    }
    // Compute all geometry in 64 bits before mutating the command queue.
    const std::uint64_t inset = 8ull + options.padding;
    const auto text_width = static_cast<std::uint64_t>(width) > inset * 2
        ? static_cast<std::uint64_t>(width) - inset * 2 : 0;
    const auto text_height = static_cast<std::uint64_t>(height) > inset * 2
        ? static_cast<std::uint64_t>(height) - inset * 2 : 0;
    const auto glyph_width = font_.glyphWidth();
    const auto glyph_height = font_.glyphHeight();
    if (text_width < glyph_width || text_height < glyph_height) {
        throw std::invalid_argument("Panel too small for the selected font");
    }
    const auto title_text = decodeText(title, font_);
    if (title_text.find(U'\n') != std::u32string::npos) {
        throw std::invalid_argument("Panel title must occupy one line");
    }
    const std::uint64_t reserved = title.empty() ? 0ull
        : static_cast<std::uint64_t>(glyph_height) + options.titleGap;
    if (text_height < reserved + glyph_height) {
        throw std::invalid_argument("Panel too small for title and body");
    }
    const auto ax = checkedCoordinate(static_cast<std::int64_t>(x) + inset);
    const auto ay = checkedCoordinate(static_cast<std::int64_t>(y) + inset);
    const auto body_y = checkedCoordinate(static_cast<std::int64_t>(ay) + reserved);
    PanelArea title_area{ax, ay, static_cast<std::uint32_t>(text_width), glyph_height};
    PanelArea body_area{ax, body_y, static_cast<std::uint32_t>(text_width),
                        static_cast<std::uint32_t>(text_height - reserved)};
    const auto title_width = linePixels(title_text.size(), glyph_width, letter_spacing_);
    const auto tx = checkedCoordinate(alignedX(title_area, title_width, options.titleAlign));
    // Draw the panel first so that its optional fill remains behind title/body.
    if (indexed) box(x, y, width, height, options.border, border_index, fill_index);
    else box(x, y, width, height, options.border, border_color, fill);
    if (!title_text.empty()) {
        commands_.push_back(TextCommand{tx, ay, title_text, title_color,
            font_, letter_spacing_, indexed, title_index, title_area});
    }
    return body_area;
}

void Context::panelText(PanelArea area, std::string_view value,
                        Color color, TextAlign align) {
    appendPanelText(area, value, color, false, ColorIndex{0}, align);
}

void Context::panelText(PanelArea area, std::string_view value,
                        ColorIndex color, TextAlign align) {
    (void)paletteColor(palette_, color);
    appendPanelText(area, value, Color{}, true, color, align);
}

void Context::appendPanelText(PanelArea area, std::string_view value,
                              Color color, bool indexed, ColorIndex index,
                              TextAlign align) {
    validateAlign(align);
    if (area.width < font_.glyphWidth() || area.height == 0) {
        throw std::invalid_argument("Panel interior is too small for text");
    }
    const auto layout = layoutText(decodeText(value, font_), font_,
                                   letter_spacing_, area.width);
    // Prepare all commands before appending for strong exception safety.
    std::vector<TextCommand> prepared;
    std::size_t start = 0;
    std::uint64_t row = 0;
    while (start < layout.text.size()) {
        const auto end = layout.text.find(U'\n', start);
        const auto stop = end == std::u32string::npos ? layout.text.size() : end;
        const auto pos_y = static_cast<std::int64_t>(area.y) +
                           static_cast<std::int64_t>(row) * font_.glyphHeight();
        if (pos_y >= static_cast<std::int64_t>(area.y) + area.height) break;
        const auto count = stop - start;
        const auto pos_x = alignedX(area, linePixels(count, font_.glyphWidth(),
                                                    letter_spacing_), align);
        prepared.push_back(TextCommand{checkedCoordinate(pos_x),
            checkedCoordinate(pos_y), layout.text.substr(start, count), color,
            font_, letter_spacing_, indexed, index, area});
        if (end == std::u32string::npos) break;
        start = end + 1;
        ++row;
    }
    commands_.reserve(commands_.size() + prepared.size());
    for (auto& cmd : prepared) commands_.push_back(std::move(cmd));
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
        } else if (const auto* cursor = std::get_if<CursorCommand>(&command)) {
            fillRectangle(target, width_, height_, cursor->x, cursor->y,
                          cursor->width, cursor->height,
                          paletteColor(palette_, cursor->color), cursor->clip);
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
                              glyph_height, text.font.bytesPerRow(), color,
                              text.clip);
                }
                advance(x, glyph_width);
                if (i + 1 < text.value.size() && text.value[i + 1] != U'\n') {
                    advance(x, text.letter_spacing);
                }
            }
        }
    }
}

namespace {

void validateRevealOptions(const RevealOptions& value) {
    if (!std::isfinite(value.charsPerSecond) || value.charsPerSecond < 0 ||
        !std::isfinite(value.cursorLead) || value.cursorLead < 0 ||
        value.cursorLead > 1 || value.cursorColor.value > 15) {
        throw std::invalid_argument("Invalid reveal options");
    }
}

}  // namespace

TextReveal::TextReveal(RevealOptions options) : options_(options) {
    validateRevealOptions(options_);
}

void TextReveal::setText(const Context& context, std::string_view value,
                         std::uint32_t max_width) {
    if (max_width == 0) {
        throw std::invalid_argument("Reveal width must be positive");
    }
    const auto decoded = decodeText(value, context.font_);
    auto layout = layoutText(decoded, context.font_, context.letter_spacing_,
                             max_width);
    std::vector<std::pair<std::int64_t, std::int64_t>> positions;
    positions.reserve(layout.text.size() + 1);
    std::int64_t px = 0;
    std::int64_t py = 0;
    const auto gw = context.font_.glyphWidth();
    const auto gh = context.font_.glyphHeight();
    for (std::size_t i = 0; i < layout.text.size(); ++i) {
        if (layout.text[i] == U'\n') {
            px = 0;
            advance(py, gh);
            continue;
        }
        positions.emplace_back(px, py);
        advance(px, gw);
        if (i + 1 < layout.text.size() && layout.text[i + 1] != U'\n') {
            advance(px, context.letter_spacing_);
        }
    }
    positions.emplace_back(px, py);

    // Commit only after complete validation and allocation succeeded.
    laid_out_ = std::move(layout.text);
    metrics_ = layout.metrics;
    positions_ = std::move(positions);
    font_ = context.font_;
    spacing_ = context.letter_spacing_;
    max_width_ = max_width;
    reset();
    if (options_.charsPerSecond == 0) finish();
}

void TextReveal::setOptions(RevealOptions options) {
    validateRevealOptions(options);
    options_ = options;
    if (options_.charsPerSecond == 0) finish();
}

const RevealOptions& TextReveal::options() const noexcept { return options_; }

void TextReveal::update(double elapsed_seconds) {
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0) {
        throw std::invalid_argument("Reveal time delta must be finite and nonnegative");
    }
    // Blink is driven by caller time too, regardless of typing progress.
    constexpr double kBlinkPeriod = 1.0;
    blink_phase_ = std::fmod(blink_phase_ +
                            std::fmod(elapsed_seconds, kBlinkPeriod),
                            kBlinkPeriod);
    if (finished()) return;
    if (options_.charsPerSecond == 0) {
        finish();
        return;
    }
    const double progressed = elapsed_seconds * options_.charsPerSecond;
    const auto remaining = positions_.size() - 1 - visible_;
    if (!std::isfinite(progressed) ||
        progressed >= static_cast<double>(remaining) - fractional_) {
        finish();
        return;
    }
    const double accumulated = fractional_ + progressed;
    const auto whole = static_cast<std::size_t>(std::floor(accumulated));
    visible_ += whole;
    fractional_ = accumulated - static_cast<double>(whole);
}

void TextReveal::reset() noexcept {
    visible_ = 0;
    fractional_ = 0.0;
    blink_phase_ = 0.0;
}

void TextReveal::finish() noexcept {
    visible_ = positions_.empty() ? 0 : positions_.size() - 1;
    fractional_ = 0.0;
}

bool TextReveal::finished() const noexcept {
    return positions_.empty() || visible_ >= positions_.size() - 1;
}

std::size_t TextReveal::visibleCharacters() const noexcept { return visible_; }
TextMetrics TextReveal::metrics() const noexcept { return metrics_; }

void TextReveal::draw(Context& context, int x, int y, Color color) const {
    queue(context, x, y, color, false, ColorIndex{0});
}

void TextReveal::draw(Context& context, int x, int y, ColorIndex color) const {
    (void)paletteColor(context.palette_, color);
    queue(context, x, y, Color{}, true, color);
}

void TextReveal::draw(Context& context, PanelArea area, Color color) const {
    queue(context, area.x, area.y, color, false, ColorIndex{0}, area);
}

void TextReveal::draw(Context& context, PanelArea area, ColorIndex color) const {
    (void)paletteColor(context.palette_, color);
    queue(context, area.x, area.y, Color{}, true, color, area);
}

void TextReveal::queue(Context& context, int x, int y, Color color,
                       bool indexed, ColorIndex index,
                       std::optional<PanelArea> clip) const {
    if (positions_.empty()) return;
    std::u32string text;
    text.reserve(laid_out_.size());
    std::size_t copied = 0;
    for (char32_t cp : laid_out_) {
        if (cp == U'\n') {
            if (copied < visible_) text.push_back(cp);
        } else if (copied < visible_) {
            text.push_back(cp);
            ++copied;
        } else {
            break;
        }
    }
    if (!text.empty()) {
        context.commands_.push_back(Context::TextCommand{
            x, y, std::move(text), color, font_, spacing_, indexed, index, clip});
    }
    if (!options_.cursorEnabled ||
        (finished() && !options_.cursorAfterComplete)) return;
    if (options_.cursorBlink && blink_phase_ >= 0.5) return;
    if (!finished() && fractional_ < 1.0 - options_.cursorLead) return;
    const auto [at_x, at_y] = positions_[visible_];
    std::int64_t cursor_x = at_x;
    std::int64_t cursor_y = at_y;
    const auto gw = font_.glyphWidth();
    const bool start_of_line = visible_ == 0 ||
        (visible_ > 0 && at_y != positions_[visible_ - 1].second);
    if (finished() && cursor_x + gw > max_width_ && cursor_x != 0) {
        // A completed line exactly fills the box: continue on the next row.
        cursor_x = 0;
        advance(cursor_y, font_.glyphHeight());
    } else if (!start_of_line &&
               cursor_x + gw + options_.cursorGapPx <= max_width_) {
        // The requested gap must never push the block beyond the text box.
        advance(cursor_x, options_.cursorGapPx);
    }
    context.commands_.push_back(Context::CursorCommand{
        static_cast<std::int64_t>(x) + cursor_x,
        static_cast<std::int64_t>(y) + cursor_y,
        gw, font_.glyphHeight(), options_.cursorColor, clip});
}

}  // namespace zx
