#include <zxcrt/zxcrt.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

void check(bool condition) {
    if (!condition) throw std::runtime_error("ZXCRT test assertion failed");
}

template <typename Callable>
bool invalid(Callable call) {
    try { call(); }
    catch (const std::invalid_argument&) { return true; }
    return false;
}

}  // namespace

int main() {
    const zxcrt::Settings settings{};
    zxcrt::validateSettings(settings);
    check(settings.enabled);
    check(settings.curvature > 0.0f);
    check(settings.glow > 0.0f);
    check(settings.scanlines > 0.0f);
    check(settings.vignette > 0.0f);

    auto custom = settings;
    custom.enabled = false;
    custom.curvature = 0.0f;
    custom.glow = 1.0f;
    custom.scanlines = 1.0f;
    custom.vignette = 1.0f;
    zxcrt::validateSettings(custom);
    check(invalid([&] { auto s = custom; s.glow = -0.01f; zxcrt::validateSettings(s); }));
    check(invalid([&] { auto s = custom; s.scanlines = 1.01f; zxcrt::validateSettings(s); }));
    check(invalid([&] { auto s = custom; s.curvature = 0.31f; zxcrt::validateSettings(s); }));
    check(invalid([&] { auto s = custom; s.vignette = std::numeric_limits<float>::infinity();
                         zxcrt::validateSettings(s); }));
    check(invalid([&] { auto s = custom; s.glow = std::numeric_limits<float>::quiet_NaN();
                         zxcrt::validateSettings(s); }));

    const auto full = zxcrt::fitViewport(640, 400, 1280, 800);
    check(full.x == 0 && full.y == 0 && full.width == 1280 && full.height == 800);
    const auto letterbox = zxcrt::fitViewport(640, 400, 1920, 1080);
    check(letterbox.x == 96 && letterbox.y == 0 &&
           letterbox.width == 1728 && letterbox.height == 1080);
    const auto pillarbox = zxcrt::fitViewport(640, 400, 1000, 1000);
    check(pillarbox.x == 0 && pillarbox.y == 187 &&
           pillarbox.width == 1000 && pillarbox.height == 625);
    const auto tiny = zxcrt::fitViewport(640, 400, 1, 1);
    check(tiny.width == 1 && tiny.height == 1);
    check(invalid([] { (void)zxcrt::fitViewport(0, 400, 800, 600); }));
    check(invalid([] { (void)zxcrt::fitViewport(640, -1, 800, 600); }));
    check(invalid([] { (void)zxcrt::fitViewport(640, 400, 0, 600); }));
    check(invalid([] { (void)zxcrt::fitViewport(640, 400, 800, 0); }));
    // Force a 64-bit intermediate in aspect-ratio computations.
    const auto huge = zxcrt::fitViewport(100000, 100000, 2000000000, 2000000000);
    check(huge.width == 2000000000 && huge.height == 2000000000);
}
