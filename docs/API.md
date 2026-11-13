# ZXFont Core API (v0.0.5; unchanged from v0.0.4)

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

The default `zx::Font{}` uses 8x8 bitmap glyphs for printable ASCII,
Russian Cyrillic, and selected Ukrainian/Belarusian letters. It is bundled
in the library; no external font files are required for these glyphs.

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
- `Context::setFont(font)` affects subsequent text commands only. Previously
  queued text keeps the font it was originally submitted with.
- When a glyph is missing from the selected font, `text()` throws instead of
  drawing an arbitrary substitute. Call `font.hasGlyph()` to query coverage.
- Glyphs are monospace, top-to-bottom. All characters in a font use the same
  advance width/height. Neither kerning nor TTF/OTF shaping is supported.

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

## SDL3 visual integration examples (v0.0.5)

`zxfont_kaya` uses ZXFont for its entire mock terminal. `zxfont_mode256`
composes ZXFont text on top of a separately rendered 256x192 game framebuffer.
Both programs own and present their RGBA8 buffers using SDL3, which is **not**
part of the `ZXFont::zxfont` interface. See `examples/` and `README.md`.
The library API is unchanged from v0.0.4.
