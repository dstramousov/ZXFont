#!/usr/bin/env python3
"""Convert the pinned ZX Spectrum Unicode FontForge source to 8x8 PSF2."""

from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

FONT_SOURCE_MAX_BYTES = 16 * 1024 * 1024
FONT_DIMENSION = 8
PIXEL_UNITS = 100
ASCENT_UNITS = 700
GLYPH_PATTERN = re.compile(r"^StartChar: ([^\n]+)\n(.*?)^EndChar$", re.MULTILINE | re.DOTALL)
OUTLINE_PATTERN = re.compile(r"^Fore\nSplineSet\n(.*?)^EndSplineSet$", re.MULTILINE | re.DOTALL)
POINT_PATTERN = re.compile(r"\s*(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+([ml])\s+\d+\s*")
ENCODING_PATTERN = re.compile(r"^Encoding: (-?\d+)\s+", re.MULTILINE)


def inside_contour(contour: list[tuple[float, float]], x: float, y: float) -> bool:
    """Return whether a pixel center lies inside a polygon (even-odd fill)."""
    if len(contour) < 3:
        raise ValueError("Open or degenerate contour")
    result = False
    for index, point in enumerate(contour):
        previous = contour[index - 1]
        if (point[1] > y) != (previous[1] > y):
            crossing = (previous[0] - point[0]) * (y - point[1]) / (
                previous[1] - point[1]
            ) + point[0]
            if x < crossing:
                result = not result
    return result


def parse_contours(text: str) -> list[list[tuple[float, float]]]:
    """Parse the rectilinear m/l outlines of the pinned FontForge source."""
    contours: list[list[tuple[float, float]]] = []
    current: list[tuple[float, float]] = []
    for line in text.splitlines():
        match = POINT_PATTERN.fullmatch(line)
        if match is None:
            raise ValueError(f"Unsupported outline operation: {line!r}")
        if match.group(3) == "m":
            if current:
                contours.append(current)
            current = []
        elif not current:
            raise ValueError("Outline starts without a move operation")
        current.append((float(match.group(1)), float(match.group(2))))
    if current:
        contours.append(current)
    return contours


def bitmap_from_contours(contours: list[list[tuple[float, float]]]) -> bytes:
    """Sample glyph outlines at the centers of the original 8x8 grid."""
    rows = []
    for row in range(FONT_DIMENSION):
        bits = 0
        y = ASCENT_UNITS - (row + 0.5) * PIXEL_UNITS
        for col in range(FONT_DIMENSION):
            x = (col + 0.5) * PIXEL_UNITS
            filled = False
            for contour in contours:
                if inside_contour(contour, x, y):
                    filled = not filled
            if filled:
                bits |= 1 << (7 - col)
        rows.append(bits)
    return bytes(rows)


def convert(source: Path, destination: Path) -> int:
    """Convert SFD glyph outlines to an 8x8 PSF2 font with Unicode mappings."""
    if source.stat().st_size > FONT_SOURCE_MAX_BYTES:
        raise ValueError("Font source exceeds size limit")
    text = source.read_text(encoding="utf-8")
    if not text.startswith("SplineFontDB:"):
        raise ValueError("Expected FontForge SFD source")
    glyphs: dict[int, bytes] = {}
    for match in GLYPH_PATTERN.finditer(text):
        name, body = match.groups()
        encoded = ENCODING_PATTERN.search(body)
        if encoded is None:
            raise ValueError(f"Missing encoding: {name}")
        codepoint = int(encoded.group(1))
        if not 0 <= codepoint <= 0x10FFFF or 0xD800 <= codepoint <= 0xDFFF:
            raise ValueError(f"Invalid Unicode codepoint: {name}")
        if codepoint in glyphs:
            raise ValueError(f"Duplicate Unicode codepoint: {name}")
        outline = OUTLINE_PATTERN.search(body)
        glyphs[codepoint] = bitmap_from_contours(
            parse_contours(outline.group(1).strip()) if outline else []
        )
    if not glyphs:
        raise ValueError("No glyphs found")
    ordered = sorted(glyphs.items())
    header = struct.pack(
        "<8I", 0x864AB572, 0, 32, 1, len(ordered), 8, 8, 8
    )
    binary = header + b"".join(bitmap for _, bitmap in ordered)
    binary += b"".join(chr(cp).encode("utf-8") + b"\xff" for cp, _ in ordered)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(binary)
    return len(ordered)


def main() -> None:
    """Regenerate the offline PSF2 asset from an explicitly supplied SFD file."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Original ZXSpectrum.sfd")
    parser.add_argument("destination", type=Path, help="Output PSF2 file")
    args = parser.parse_args()
    print(f"Converted {convert(args.source, args.destination)} Unicode glyphs")


if __name__ == "__main__":
    main()
