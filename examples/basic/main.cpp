#include "zxfont/zxfont.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

int main(int argc, char** argv) {
    constexpr std::uint32_t width = 256;
    constexpr std::uint32_t height = 192;
    const char* output = argc > 1 ? argv[1] : "zxfont_demo.ppm";

    std::vector<std::uint8_t> framebuffer(width * height * 4);
    for (std::size_t index = 0; index < framebuffer.size(); index += 4) {
        framebuffer[index + 0] = 0;
        framebuffer[index + 1] = 22;
        framebuffer[index + 2] = 8;
        framebuffer[index + 3] = 255;
    }

    zx::Context ui(width, height);
    ui.text(8, 8, "HELLO ZXFONT", {25, 240, 100, 255});
    ui.text(8, 32, "Pixel buffer / C++20", {90, 190, 125, 255});
    ui.text(8, 48, "0123456789 !?.,:;", {90, 190, 125, 255});
    ui.render({std::span<std::uint8_t>(framebuffer), width, height, width * 4});

    std::ofstream file(output, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open output file: " << output << '\n';
        return 1;
    }
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index < framebuffer.size(); index += 4) {
        file.write(reinterpret_cast<const char*>(&framebuffer[index]), 3);
    }
    if (!file) {
        std::cerr << "Failed to write output file: " << output << '\n';
        return 1;
    }
    std::cout << "Wrote " << output << " (" << width << 'x' << height << ")\n";
}
