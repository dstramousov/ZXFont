#ifndef ZXCRT_ZXCRT_HPP
#define ZXCRT_ZXCRT_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace zxcrt {

/** @brief A rectangle in physical output pixels, with bottom-left origin. */
struct Viewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

/**
 * @brief Fit an image inside the output while preserving its aspect ratio.
 * @throws std::invalid_argument if any dimension is not positive.
 */
[[nodiscard]] Viewport fitViewport(int sourceWidth, int sourceHeight,
                                   int outputWidth, int outputHeight);

/** @brief Independent CRT postprocessing settings, with restrained defaults. */
struct Settings {
    bool enabled = true;
    float curvature = 0.055f;  ///< Barrel distortion, range [0, 0.3].
    float glow = 0.20f;        ///< Blurred phosphor halo, range [0, 1].
    float scanlines = 0.14f;   ///< Scanline darkening, range [0, 1].
    float vignette = 0.12f;    ///< Edge darkening, range [0, 1].
};

/**
 * @brief Validate a CRT settings value.
 * @throws std::invalid_argument for nonfinite or out-of-range settings.
 */
void validateSettings(const Settings& settings);

/** @brief Generic platform OpenGL function pointer (not an SDL type). */
using GLProcedure = void (*)();

/** @brief Callback that resolves OpenGL entry points for the current context. */
using GLProcLoader = GLProcedure (*)(const char* name);

/**
 * @brief OpenGL 3.3 CRT renderer for a caller-owned RGBA8 image.
 *
 * A compatible OpenGL 3.3 core context must be current on the calling thread
 * throughout construction, rendering, and destruction. The caller owns the
 * context and presents/swaps the result. Renderer draws into the currently
 * bound framebuffer, clears it to black, and letterboxes the image. No SDL
 * headers, windows, files, or ZXFont objects are required by this library.
 *
 * The renderer preserves its modified OpenGL state but does not own or swap
 * the window. Do not share a Renderer between different GL contexts.
 */
class Renderer {
public:
    /** @brief Compile the embedded shaders and allocate GL resources. */
    explicit Renderer(GLProcLoader loader);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /** @brief Replace effects without recreating the OpenGL resources. */
    void setSettings(const Settings& settings);

    /** @brief Return the active settings. */
    [[nodiscard]] const Settings& settings() const noexcept;

    /**
     * @brief Upload a top-left-origin RGBA8 image and draw it into the current GL framebuffer.
     *
     * @param pixels Exactly sourceWidth * sourceHeight * 4 bytes, packed RGBA8.
     * @param sourceWidth Image width in pixels (>0).
     * @param sourceHeight Image height in pixels (>0).
     * @param outputWidth Current framebuffer width in physical pixels (>0).
     * @param outputHeight Current framebuffer height in physical pixels (>0).
     * @throws std::invalid_argument for invalid dimensions or byte count.
     * @throws std::runtime_error for OpenGL initialization failures.
     */
    void render(std::span<const std::uint8_t> pixels, int sourceWidth,
                int sourceHeight, int outputWidth, int outputHeight);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Settings settings_{};
};

}  // namespace zxcrt

#endif  // ZXCRT_ZXCRT_HPP
