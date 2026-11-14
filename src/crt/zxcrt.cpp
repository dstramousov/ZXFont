#include <zxcrt/zxcrt.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace zxcrt {

Viewport fitViewport(int sourceWidth, int sourceHeight,
                     int outputWidth, int outputHeight) {
    if (sourceWidth <= 0 || sourceHeight <= 0 || outputWidth <= 0 || outputHeight <= 0) {
        throw std::invalid_argument("CRT dimensions must be positive");
    }
    // Use 64-bit arithmetic to avoid overflow even with large source dimensions.
    const auto scaledWidth = static_cast<std::int64_t>(outputHeight) * sourceWidth;
    const auto scaledHeight = static_cast<std::int64_t>(outputWidth) * sourceHeight;
    int width = outputWidth;
    int height = outputHeight;
    if (scaledWidth <= scaledHeight) {
        width = static_cast<int>(scaledWidth / sourceHeight);
    } else {
        height = static_cast<int>(scaledHeight / sourceWidth);
    }
    width = width > 0 ? width : 1;
    height = height > 0 ? height : 1;
    return {(outputWidth - width) / 2, (outputHeight - height) / 2,
            width, height};
}

void validateSettings(const Settings& settings) {
    const auto inRange = [](float value, float limit) {
        return std::isfinite(value) && value >= 0.0f && value <= limit;
    };
    if (!inRange(settings.curvature, 0.3f) || !inRange(settings.glow, 1.0f) ||
        !inRange(settings.scanlines, 1.0f) || !inRange(settings.vignette, 1.0f)) {
        throw std::invalid_argument("CRT effects must have finite, in-range values");
    }
}

}  // namespace zxcrt
