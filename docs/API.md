# ZXFont Core API (v0.0.3)

The API is intentionally small and independent of the display backend.

```cpp
std::vector<std::uint8_t> rgba(256 * 192 * 4, 0);
zx::Context ui(256, 192);          // logical dimensions, in pixels
ui.text(8, 8, "HELLO ZXFONT", {0, 255, 0, 255});
ui.render({rgba, 256, 192, 256 * 4});
ui.clear();                         // remove draw commands, not framebuffer pixels
```

Include `<zxfont/zxfont.hpp>`, link against `ZXFont::zxfont`.

- `Context(width, height)`: non-owning logical canvas description, no SDL/window.
- `text(x, y, value, color)`: queue text at pixel coordinates. Font is fixed 8x8.
- `render(surface)`: replay queued commands onto supplied RGBA8 bytes.
- `clear()`: remove queued commands before composing the next frame.
- `width()`, `height()`: logical canvas dimensions.
- `Surface`: span of bytes, dimensions, and stride in **bytes**. It must contain
  enough storage for all visible rows, including row padding.
- All glyphs have transparent backgrounds. Render does **not** clear the surface.
- RGBA channels are straight-alpha, source-over composited on glyph pixels.
- Text supports printable ASCII (`0x20..0x7e`) and LF (`\n`) only in this patch.
  UTF-8, Cyrillic, font loading, color palettes, windows, and SDL3 are not yet
  implemented. Unsupported input throws `std::invalid_argument`.
- Surface validation throws for bad descriptions. Valid descriptors must refer
  to live writable memory throughout the render call.

For Mode256, the host renderer remains responsible for its own framebuffer,
converting indexed pixels to RGBA if necessary, and presenting the final image.
The indexed-color adapter is intentionally deferred until the actual Mode256
framebuffer contract has been inspected.
