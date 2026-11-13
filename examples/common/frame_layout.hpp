#ifndef ZXFONT_EXAMPLES_FRAME_LAYOUT_HPP
#define ZXFONT_EXAMPLES_FRAME_LAYOUT_HPP

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace demo {

struct Viewport {
    float x;
    float y;
    float width;
    float height;
};

// Fit the entire logical image inside the renderer's actual output extent.
// A single uniform scale prevents cropping after fullscreen or window resize.
inline Viewport fitViewport(int logical_width, int logical_height,
                            int output_width, int output_height) {
    if (logical_width <= 0 || logical_height <= 0 ||
        output_width <= 0 || output_height <= 0) {
        throw std::invalid_argument("Invalid viewport dimensions");
    }
    const double scale = std::min(static_cast<double>(output_width) / logical_width,
                                  static_cast<double>(output_height) / logical_height);
    // Preserve whole physical pixels whenever the output can hold a 1x frame.
    // Only use a fractional scale when the window is smaller than the canvas.
    const double effective_scale = scale >= 1.0 ? std::floor(scale) : scale;
    const float width = static_cast<float>(logical_width * effective_scale);
    const float height = static_cast<float>(logical_height * effective_scale);
    return {(static_cast<float>(output_width) - width) * 0.5f,
            (static_cast<float>(output_height) - height) * 0.5f,
            width, height};
}

}  // namespace demo

#endif  // ZXFONT_EXAMPLES_FRAME_LAYOUT_HPP
