# ZXFont Core API (v0.0.10)

A compact bitmap text API. The application owns its framebuffer and the window.
ZXFont does not depend on SDL3 or create display resources.

```cpp
std::vector<std::uint8_t> rgba(256 * 192 * 4, 0);
zx::Context ui(256, 192);
ui.text(8, 8, "HELLO ZXFONT", {0, 255, 0, 255});
ui.text(8, 24, "Привет, Кая!");
ui.render({rgba, 256, 192, 256 * 4});
ui.clear(); // Remove queued commands, not framebuffer pixels.
```

Include `<zxfont/zxfont.hpp>` and link `ZXFont::zxfont`.

## Fonts

The default `zx::Font{}` embeds **ZX Spectrum Unicode 8x8**, including
Latin, Russian/Ukrainian Cyrillic and box-drawing glyphs (667 Unicode
mappings). Applications need **no external font file** for the default.
The old mixed ASCII/Cyrillic bitmaps are no longer bundled.

```cpp
zx::Font console = zx::Font::loadPsf2("my_console_font.psf");
ui.setFont(console);
ui.text(8, 40, "Кая", {255, 200, 100, 255});
```

- `Font::loadPsf2(path)` loads a **PSF2** bitmap font with a Unicode table.
  PSF1 fonts, PSF2 fonts without Unicode tables, and glyph sequences
  (composed character ligatures) are not supported.
- `Font::glyphWidth()`, `glyphHeight()`, `hasGlyph(codepoint)` query coverage.
- Glyph dimensions are 1..256 pixels per axis; font files are limited to 16 MiB.
- Fonts are immutable, and copies share ownership of the underlying bitmaps.
- `Context::setLetterSpacing(pixels)` selects extra horizontal pixels between
  neighboring glyphs for subsequently queued commands. Default: 0. Spacing
  includes ordinary space glyphs, excludes the end of each line, and does not
  affect vertical LF advances. Already queued commands retain their spacing;
  the bitmaps themselves do not change. The parameter is an unsigned pixel
  count; large values safely move later glyphs outside the clipped canvas.
- `Context::setFont(font)` affects subsequent text commands only. Previously
  queued text keeps the font it was originally submitted with. This already
  permits **different glyph dimensions within one frame** with no new API.
  Newline advances by the height of the font captured for each command.
- `Context::setFont(path, &error)` attempts to select an external PSF2 file.
  On a missing, unreadable, or malformed file it selects the built-in Spectrum
  Unicode 8x8, returns `false`, and optionally describes the failure in `error`.
  On success it returns `true` and clears `error`. This does not alter queued
  commands (each retains its original font). Out-of-memory errors still throw.
  `Font::loadPsf2()` remains a strict, throwing loader for callers who want it.
- When a glyph is missing from the selected font, `text()` throws instead of
  drawing an arbitrary substitute. Call `font.hasGlyph()` to query coverage.
- Glyphs are monospace, top-to-bottom. All characters in a font use the same
  advance width/height. Neither kerning nor TTF/OTF shaping is supported.

## ZX Spectrum Unicode 8x8 font (v0.0.8)

The default font is the ZX Spectrum Unicode 8x8 design. It is compiled into
`libzxfont.a` using `src/spectrum_unicode_8x8.hpp`, generated from
`fonts/zx-spectrum-unicode-8x8.psf`. It has the **same pixels and mappings**
as that PSF2 file, which remains a reference asset. For consistent font
coverage, always supply a Unicode-mapped PSF2 font when loading additional
font families. No TTF rasterizer is needed at runtime.

### Several fonts in one UI

```cpp
zx::Font classic; // Embedded Spectrum Unicode 8x8.
std::string error;
ui.setFont(classic);
ui.text(8, 8, "STATUS", zx::ColorIndex{15});
if (!ui.setFont("fonts/zx-compact-airy-6x8.psf", &error)) {
    // ZXFont already selected embedded Spectrum 8x8 as fallback.
    logWarning(error);
}
ui.text(8, 32, "Текст другим шрифтом", zx::ColorIndex{12});
ui.setFont(classic);
ui.text(8, 64, "> ", zx::ColorIndex{15});
```

Font sizes/advances can vary from command to command. The default built-in
font is still 8x8. Compact Airy is an optional 6x8 external PSF2 asset
(667 Unicode mappings). It is not automatically loaded by the library; the
calling application chooses the path. No glyph-by-glyph fallback is performed.
The SDL3 demos exercise F3 switches, loading failures, and fixed 8x8 borders.
F4 cycles 0 / 1 / 2 px spacing for Compact Airy (initially 1); the built-in
Spectrum font keeps 0 px spacing in the demos.
Display scaling remains the host's responsibility.

## Palette-indexed colors

RGBA colors are still accepted and stay exactly as supplied. Indexed colors
reference one of 16 entries from the current palette:

```cpp
ui.setPalette(zx::Palette::MonoGreen);
ui.text(8, 8, "KAYA", zx::ColorIndex{10});
ui.text(8, 24, "fixed RGBA", zx::Color{200, 10, 10, 255});
ui.setPalette(zx::Palette::MonoAmber);  // Recolors previously queued indexed text.
ui.render(surface);
const zx::Color background = zx::paletteColor(zx::Palette::MonoAmber, {0});
```

`Palette` presets are `MonoGreen`, `MonoAmber`, `Spectrum`, and `VGA`.
Entries use indices 0..15; Spectrum and VGA entries follow their respective
standard 16-color index ordering. `paletteColor()` and indexed `text()` reject
indices outside 0..15. `setPalette()` rejects unknown palette identifiers.

Changing the active palette affects indexed text **at render time**, without
re-queuing it, changing its position, or modifying glyph bitmaps. Explicit
RGBA text is not recolored. The host still owns its background and any
non-ZXFont graphics: use `paletteColor()` when it wants matching colors.

## Text behavior

- `Context(width,height)` specifies a logical canvas in **pixels**.
- `text(x,y,utf8,color)` queues a command using pixel coordinates. UTF-8 is
  checked strictly: malformed, overlong, surrogate, and out-of-range encodings
  are rejected with `std::invalid_argument`.
- `\n` is the only supported control character: it resets x and advances y by
  the active font's height. `\r` and `\t` are rejected.
- No automatic wrapping, bidirectional shaping, normalization or font fallback.
- `clear()` drops commands without touching any framebuffer pixels.
- `render(surface)` replays commands onto the caller-owned RGBA8 byte buffer.
  Glyph backgrounds are transparent; source-over blending, stride and clipping
  rules are unchanged from v0.0.3.
- `Surface` has a byte span, width, height, and row stride in **bytes**. The
  span must cover all visible rows including intermediate row padding.
- Invalid surface descriptors throw; rendering requires live writable memory.

For Mode256, the host still owns its framebuffer and presents the frame.
Direct writes to an indexed 8-bit framebuffer are not supported yet. The
indexed adapter will be designed against Mode256's actual buffer contract.

## SDL3 visual integration examples (v0.0.9)

`zxfont_kaya` uses ZXFont for its entire mock terminal. `zxfont_mode256`
composes ZXFont text on top of a separately rendered 256x192 game framebuffer.
Both programs own and present their RGBA8 buffers using SDL3, which is **not**
part of the `ZXFont::zxfont` interface. See `examples/` and `README.md`.
F2 cycles the four palettes; F3 switches embedded ZX Spectrum Unicode 8x8
against external Compact Airy 6x8; F5 shows glyph diagnostic scenes.
F4 changes Compact Airy inter-glyph spacing; glyph dimensions remain 6x8.
The SDL3 adapter preserves aspect ratio when resized or fullscreen.
