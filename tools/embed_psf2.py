#!/usr/bin/env python3
"""Embed a PSF2 asset as a self-contained C++ byte array."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> None:
    """Generate the bundled font header from the canonical PSF2 file."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    data = arguments.font.read_bytes()
    if data[:4] != bytes((0x72, 0xB5, 0x4A, 0x86)):
        parser.error("Input is not a PSF2 font")
    rows = [
        "    " + ", ".join(f"0x{value:02X}" for value in data[pos:pos + 12]) + ","
        for pos in range(0, len(data), 12)
    ]
    output = (
        "#ifndef ZXFONT_SPECTRUM_UNICODE_8X8_HPP\n"
        "#define ZXFONT_SPECTRUM_UNICODE_8X8_HPP\n\n"
        "#include <array>\n#include <cstdint>\n\n"
        "namespace zx::detail {\n\n"
        "// Generated from fonts/zx-spectrum-unicode-8x8.psf.\n"
        "// Rebuild with: python3 tools/embed_psf2.py fonts/zx-spectrum-unicode-8x8.psf src/spectrum_unicode_8x8.hpp\n"
        f"inline constexpr std::array<std::uint8_t, {len(data)}> kSpectrumUnicode8x8Psf2 = {{\n"
        + "\n".join(rows)
        + "\n};\n\n}  // namespace zx::detail\n\n"
        "#endif  // ZXFONT_SPECTRUM_UNICODE_8X8_HPP\n"
    )
    arguments.output.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
