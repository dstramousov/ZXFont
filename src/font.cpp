#include "zxfont/zxfont.hpp"

#include "basic_ascii_8x8.hpp"
#include "basic_cyrillic_8x8.hpp"
#include "utf8.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace zx {
namespace {

constexpr std::uint32_t kPsf2Magic = 0x864AB572;
constexpr std::uint32_t kPsf2HeaderBytes = 32;
constexpr std::uint32_t kUnicodeTableFlag = 1;
constexpr std::uint32_t kMaxDimension = 256;
constexpr std::size_t kMaxFontBytes = 16 * 1024 * 1024;

std::uint8_t reverseBits(std::uint8_t bits) noexcept {
    bits = static_cast<std::uint8_t>(((bits & 0x55u) << 1) | ((bits & 0xAAu) >> 1));
    bits = static_cast<std::uint8_t>(((bits & 0x33u) << 2) | ((bits & 0xCCu) >> 2));
    return static_cast<std::uint8_t>((bits << 4) | (bits >> 4));
}

std::uint32_t readLe32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::invalid_argument("Truncated PSF2 header");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

}  // namespace

struct Font::Data {
    std::uint32_t width = 8;
    std::uint32_t height = 8;
    std::size_t row_bytes = 1;
    std::size_t glyph_size = 8;
    std::vector<std::uint8_t> glyphs;
    std::unordered_map<char32_t, std::size_t> indices;
};

// Define builtin construction through Font's own methods to keep Data private.
Font::Font() {
    static const auto builtin = [] {
        auto data = std::make_shared<Data>();
        constexpr std::size_t ascii_count = std::size(detail::kBasicAscii8x8);
        constexpr std::size_t cyrillic_count = std::size(detail::kBasicCyrillic8x8);
        data->glyphs.reserve((ascii_count + cyrillic_count) * 8);
        data->indices.reserve(ascii_count + cyrillic_count);
        for (std::size_t i = 0; i < ascii_count; ++i) {
            data->indices.emplace(static_cast<char32_t>(i + 0x20), i);
            for (auto row : detail::kBasicAscii8x8[i]) {
                data->glyphs.push_back(reverseBits(row));
            }
        }
        for (std::size_t i = 0; i < cyrillic_count; ++i) {
            const auto& glyph = detail::kBasicCyrillic8x8[i];
            data->indices.emplace(glyph.codepoint, ascii_count + i);
            for (auto row : glyph.rows) {
                data->glyphs.push_back(reverseBits(row));
            }
        }
        return std::shared_ptr<const Data>(std::move(data));
    }();
    data_ = builtin;
}

Font::Font(std::shared_ptr<const Data> data) noexcept : data_(std::move(data)) {}

std::uint32_t Font::glyphWidth() const noexcept { return data_->width; }
std::uint32_t Font::glyphHeight() const noexcept { return data_->height; }
std::size_t Font::bytesPerRow() const noexcept { return data_->row_bytes; }

bool Font::hasGlyph(char32_t codepoint) const noexcept {
    return data_->indices.contains(codepoint);
}

const std::uint8_t* Font::bitmap(char32_t codepoint) const noexcept {
    const auto found = data_->indices.find(codepoint);
    if (found == data_->indices.end()) {
        return nullptr;
    }
    return data_->glyphs.data() + found->second * data_->glyph_size;
}

// Strict UTF-8 decoder shared by font tables and Context text validation.
namespace detail {

char32_t readCodepoint(std::string_view text, std::size_t& pos) {
    if (pos >= text.size()) {
        throw std::invalid_argument("Truncated UTF-8 sequence");
    }
    const auto first = static_cast<std::uint8_t>(text[pos++]);
    if (first < 0x80) {
        return first;
    }
    std::size_t continuations = 0;
    char32_t codepoint = 0;
    char32_t minimum = 0;
    if (first >= 0xC2 && first <= 0xDF) {
        continuations = 1;
        codepoint = first & 0x1F;
        minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
        continuations = 2;
        codepoint = first & 0x0F;
        minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
        continuations = 3;
        codepoint = first & 0x07;
        minimum = 0x10000;
    } else {
        throw std::invalid_argument("Invalid UTF-8 leading byte");
    }
    if (text.size() - pos < continuations) {
        throw std::invalid_argument("Truncated UTF-8 sequence");
    }
    for (std::size_t i = 0; i < continuations; ++i) {
        const auto byte = static_cast<std::uint8_t>(text[pos++]);
        if ((byte & 0xC0) != 0x80) {
            throw std::invalid_argument("Invalid UTF-8 continuation byte");
        }
        codepoint = static_cast<char32_t>((codepoint << 6) | (byte & 0x3F));
    }
    if (codepoint < minimum || (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
        codepoint > 0x10FFFF) {
        throw std::invalid_argument("Invalid UTF-8 codepoint");
    }
    return codepoint;
}

}  // namespace detail

Font Font::loadPsf2(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Cannot open PSF2 font");
    }
    const auto end = stream.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kMaxFontBytes) {
        throw std::invalid_argument("PSF2 font exceeds size limit");
    }
    const auto size = static_cast<std::size_t>(end);
    std::vector<std::uint8_t> bytes(size);
    stream.seekg(0);
    if (size != 0 && !stream.read(reinterpret_cast<char*>(bytes.data()),
                                  static_cast<std::streamsize>(size))) {
        throw std::runtime_error("Cannot read PSF2 font");
    }
    if (size < kPsf2HeaderBytes || readLe32(bytes, 0) != kPsf2Magic ||
        readLe32(bytes, 4) != 0) {
        throw std::invalid_argument("Unsupported PSF2 header");
    }
    const auto header_size = readLe32(bytes, 8);
    const auto flags = readLe32(bytes, 12);
    const auto glyph_count = readLe32(bytes, 16);
    const auto char_size = readLe32(bytes, 20);
    const auto height = readLe32(bytes, 24);
    const auto width = readLe32(bytes, 28);
    if (header_size < kPsf2HeaderBytes || header_size > size ||
        (flags & ~kUnicodeTableFlag) != 0 || (flags & kUnicodeTableFlag) == 0 ||
        glyph_count == 0 || width == 0 || height == 0 ||
        width > kMaxDimension || height > kMaxDimension) {
        throw std::invalid_argument("Unsupported PSF2 geometry or missing Unicode table");
    }
    const auto row_bytes = (static_cast<std::size_t>(width) + 7) / 8;
    const auto minimum_size = row_bytes * height;
    if (char_size < minimum_size || glyph_count > (size - header_size) / char_size) {
        throw std::invalid_argument("Truncated PSF2 glyph bitmap data");
    }
    const auto table_offset = header_size + static_cast<std::size_t>(glyph_count) * char_size;
    if (table_offset >= size) {
        throw std::invalid_argument("Missing PSF2 Unicode table");
    }

    auto data = std::make_shared<Data>();
    data->width = width;
    data->height = height;
    data->row_bytes = row_bytes;
    data->glyph_size = minimum_size;
    data->glyphs.resize(static_cast<std::size_t>(glyph_count) * minimum_size);
    for (std::size_t i = 0; i < glyph_count; ++i) {
        const auto* source = bytes.data() + header_size + i * char_size;
        std::copy_n(source, minimum_size,
                    data->glyphs.begin() + static_cast<std::ptrdiff_t>(i * minimum_size));
    }

    std::size_t pos = table_offset;
    for (std::size_t glyph = 0; glyph < glyph_count; ++glyph) {
        bool in_sequence = false;
        bool terminated = false;
        while (pos < size) {
            const auto marker = bytes[pos];
            if (marker == 0xFF) {
                ++pos;
                terminated = true;
                break;
            }
            if (marker == 0xFE) {
                ++pos;
                in_sequence = true;
                continue;
            }
            const auto remainder = std::string_view(
                reinterpret_cast<const char*>(bytes.data() + pos), size - pos);
            std::size_t consumed = 0;
            const auto codepoint = detail::readCodepoint(remainder, consumed);
            pos += consumed;
            if (!in_sequence) {
                data->indices.try_emplace(codepoint, glyph);
            }
        }
        if (!terminated) {
            throw std::invalid_argument("Unterminated PSF2 Unicode table entry");
        }
    }
    if (pos != size || data->indices.empty()) {
        throw std::invalid_argument("Invalid PSF2 Unicode table extent");
    }
    return Font(std::move(data));
}

}  // namespace zx
