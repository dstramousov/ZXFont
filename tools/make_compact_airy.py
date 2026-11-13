#!/usr/bin/env python3
"""Generate the selected ZX Compact Airy 6x8 bitmap font from Spectrum PSF2."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

PSF2_MAGIC = 0x864AB572
HEADER_SIZE = 32
GLYPH_HEIGHT = 8
SOURCE_WIDTH = 8
TARGET_WIDTH = 6

# Hand-retouched shapes selected for legible Cyrillic at 6x8.
CYRILLIC_FIXES: dict[str, list[str]] = {
    "Ж": ["......", "#.#.#.", "#.#.#.", ".###..", "..#...", ".###..", "#.#.#.", "#.#.#."],
    "ж": ["......", "......", "#.#.#.", ".###..", "..#...", ".###..", "#.#.#.", "#.#.#."],
    "Ш": ["......", "#.#.#.", "#.#.#.", "#.#.#.", "#.#.#.", "#.#.#.", "#####.", "......"],
    "ш": ["......", "......", "#.#.#.", "#.#.#.", "#.#.#.", "#.#.#.", "#####.", "......"],
    "Щ": ["......", "#.#.#.", "#.#.#.", "#.#.#.", "#.#.#.", "#.#.#.", "######", ".....#"],
    "щ": ["......", "......", "#.#.#.", "#.#.#.", "#.#.#.", "#.#.#.", "######", ".....#"],
    "Ы": ["......", "#...#.", "#...#.", "###.#.", "#.#.#.", "#.#.#.", "###.#.", "......"],
    "ы": ["......", "......", "#...#.", "#...#.", "###.#.", "#.#.#.", "###.#.", "......"],
    "Ю": ["......", "#.###.", "#.#.#.", "###.#.", "#.#.#.", "#.#.#.", "#.###.", "......"],
    "ю": ["......", "......", "#.###.", "#.#.#.", "###.#.", "#.#.#.", "#.###.", "......"],
    "Й": [".#.#..", "..#...", "#...#.", "#..##.", "#.#.#.", "##..#.", "#...#.", "......"],
    "й": [".#.#..", "..#...", "#...#.", "#..##.", "#.#.#.", "##..#.", "#...#.", "......"],
    "Ё": [".#.#..", "......", "#####.", "#.....", "####..", "#.....", "#####.", "......"],
    "ё": [".#.#..", "......", ".###..", "#...#.", "#####.", "#.....", ".####.", "......"],
    "І": ["......", ".###..", "..#...", "..#...", "..#...", "..#...", ".###..", "......"],
    "і": ["......", "..#...", "......", ".##...", "..#...", "..#...", ".###..", "......"],
    "Ї": [".#.#..", "......", ".###..", "..#...", "..#...", "..#...", ".###..", "......"],
    "ї": [".#.#..", "......", ".##...", "..#...", "..#...", "..#...", ".###..", "......"],
    "Ґ": ["....#.", "#####.", "#.....", "#.....", "#.....", "#.....", "#.....", "......"],
    "ґ": ["....#.", "..###.", "#.....", "#.....", "#.....", "#.....", "#.....", "......"],
    "Є": ["......", ".###..", "#...#.", "#.....", "###...", "#.....", ".####.", "......"],
    "є": ["......", "......", ".###..", "#.....", "###...", "#.....", ".####.", "......"],
}

DIGIT_FIXES: dict[str, list[str]] = {
    "0": ["......", ".###..", "#...#.", "#..##.", "#.#.#.", "##..#.", ".###..", "......"],
    "1": ["......", "..#...", ".##...", "..#...", "..#...", "..#...", ".###..", "......"],
    "2": ["......", ".###..", "#...#.", "...#..", "..#...", ".#....", "#####.", "......"],
    "3": ["......", ".###..", "#...#.", "..##..", "....#.", "#...#.", ".###..", "......"],
    "4": ["......", "...#..", "..##..", ".#.#..", "#####.", "...#..", "...#..", "......"],
    "5": ["......", "#####.", "#.....", "####..", "....#.", "#...#.", ".###..", "......"],
    "6": ["......", ".###..", "#.....", "####..", "#...#.", "#...#.", ".###..", "......"],
    "7": ["......", "#####.", "....#.", "...#..", "..#...", ".#....", ".#....", "......"],
    "8": ["......", ".###..", "#...#.", ".###..", "#...#.", "#...#.", ".###..", "......"],
    "9": ["......", ".###..", "#...#.", "#...#.", ".####.", "....#.", ".###..", "......"],
}


def pattern_to_bytes(rows: list[str]) -> bytes:
    """Convert an 8-row pattern with six columns to PSF2 bitmap bytes."""
    if len(rows) != GLYPH_HEIGHT or any(len(row) != TARGET_WIDTH for row in rows):
        raise ValueError("Incorrect glyph correction dimensions")
    if any(c not in "#." for row in rows for c in row):
        raise ValueError("Invalid glyph correction pixel")
    return bytes(
        sum(1 << (7 - col) for col, value in enumerate(row) if value == "#")
        for row in rows
    )


def compress_glyph(rows: bytes) -> bytes:
    """Sample each occupied 8-pixel row into a 5-pixel drawing + one spacer."""
    pixels = [[(row >> (7 - col)) & 1 for col in range(SOURCE_WIDTH)] for row in rows]
    occupied = [x for y in range(GLYPH_HEIGHT) for x in range(SOURCE_WIDTH) if pixels[y][x]]
    if not occupied:
        return bytes(GLYPH_HEIGHT)
    lo = min(occupied)
    span = max(occupied) - lo + 1
    width = min(TARGET_WIDTH - 1, span)
    return bytes(
        sum(
            pixels[y][lo + min(span - 1, int((col + 0.5) * span / width))] << (7 - col)
            for col in range(width)
        )
        for y in range(GLYPH_HEIGHT)
    )


def parse_source(source: bytes) -> dict[int, bytes]:
    """Read canonical Unicode-mapped 8x8 PSF2 and return codepoint bitmaps."""
    if len(source) < HEADER_SIZE:
        raise ValueError("Truncated PSF2 header")
    magic, version, header_size, flags, count, char_size, height, width = struct.unpack_from(
        "<8I", source
    )
    if (magic, version, header_size, flags, char_size, height, width) != (
        PSF2_MAGIC, 0, HEADER_SIZE, 1, 8, GLYPH_HEIGHT, SOURCE_WIDTH
    ) or count == 0:
        raise ValueError("Expected Unicode-mapped Spectrum 8x8 PSF2")
    glyph_end = header_size + count * char_size
    if glyph_end >= len(source):
        raise ValueError("Missing PSF2 Unicode table")
    bitmaps = [source[header_size + i * char_size: header_size + (i + 1) * char_size]
               for i in range(count)]
    mapping: dict[int, bytes] = {}
    pos = glyph_end
    for glyph in bitmaps:
        try:
            end = source.index(0xFF, pos)
        except ValueError as error:
            raise ValueError("Unterminated PSF2 Unicode entry") from error
        data = source[pos:end]
        pos = end + 1
        try:
            codepoint = data.decode("utf-8")
        except UnicodeDecodeError as error:
            raise ValueError("Invalid UTF-8 in PSF2 Unicode table") from error
        if len(codepoint) != 1 or ord(codepoint) in mapping:
            raise ValueError("Expected one unique codepoint per PSF2 glyph")
        mapping[ord(codepoint)] = glyph
    if pos != len(source):
        raise ValueError("Unexpected data after Unicode table")
    return mapping


def convert(source: Path, destination: Path) -> int:
    """Generate the chosen compact prototype and return the glyph count."""
    source_glyphs = parse_source(source.read_bytes())
    generated = {cp: compress_glyph(rows) for cp, rows in source_glyphs.items()}
    for ch, rows in (CYRILLIC_FIXES | DIGIT_FIXES).items():
        if ord(ch) not in generated:
            raise ValueError(f"Missing source glyph for correction: {ch}")
        generated[ord(ch)] = pattern_to_bytes(rows)
    codes = sorted(generated)
    output = bytearray(struct.pack("<8I", PSF2_MAGIC, 0, HEADER_SIZE, 1,
                                   len(codes), GLYPH_HEIGHT, GLYPH_HEIGHT, TARGET_WIDTH))
    for cp in codes:
        output.extend(generated[cp])
    for cp in codes:
        output.extend(chr(cp).encode("utf-8"))
        output.append(0xFF)
    destination.write_bytes(output)
    return len(codes)


def main() -> None:
    """Parse CLI arguments and generate PSF2 from a given Spectrum font."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    options = parser.parse_args()
    count = convert(options.source, options.output)
    print(f"Wrote {count} glyphs to {options.output}")


if __name__ == "__main__":
    main()
