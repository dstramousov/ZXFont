#ifndef ZXFONT_INTERNAL_UTF8_HPP
#define ZXFONT_INTERNAL_UTF8_HPP

#include <cstddef>
#include <string_view>

namespace zx::detail {

// Decode one Unicode scalar value and advance pos; reject malformed UTF-8.
char32_t readCodepoint(std::string_view text, std::size_t& pos);

}  // namespace zx::detail

#endif  // ZXFONT_INTERNAL_UTF8_HPP
