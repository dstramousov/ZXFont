# ZXFont Core API (v0.0.16)

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

## Measuring and wrapping text (v0.0.11)

`Context::measureText(text, max_width)` computes the pixel extent and line
count using the **currently selected font and letter spacing**. `max_width=0`
(default) respects explicit newlines without automatic wrapping. A positive
width wraps on ASCII spaces; words longer than the width split at glyph
boundaries. Explicit empty lines are preserved. Empty input returns all zeroes.
A width smaller than one glyph is rejected. Measurements are independent of
canvas size; the renderer still clips to the canvas.

`textBox(x, y, max_width, text, color)` queues exactly the layout returned by
`measureText(text, max_width)`, with either RGBA or `ColorIndex`. The text box
has a horizontal limit but no implicit vertical clipping; callers should use
`metrics.height` to place subsequent UI items and avoid collisions. Its font
and spacing are snapshotted like regular `text()` commands.

```cpp
ui.setLetterSpacing(1);
const auto metrics = ui.measureText("Привет, Кая! Как дела?", 120);
ui.textBox(8, 120, 120, "Привет, Кая! Как дела?", zx::ColorIndex{12});
// metrics.width <= 120, metrics.height is a multiple of glyphHeight().
```

The word separator is U+0020 SPACE. No hyphenation, tab expansion, Unicode
line-break algorithm or shaping is provided in this version.

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

## Pseudographics (v0.0.12)

ZXFont queues `box()` and `line()` commands in the same `Context` as `text()`
and `textBox()`. All coordinates and dimensions are **pixels**, not character
cells. Border glyphs are always rendered with the **embedded Spectrum Unicode
8x8**; selecting a 6x8 external font or changing letter spacing does not
move or deform frame geometry.

```cpp
ui.box(8, 128, 240, 56, zx::Border::Single,
       zx::ColorIndex{12}, zx::ColorIndex{0}); // Optional filled background.
ui.line(8, 152, 240, zx::Axis::Horizontal,
        zx::Border::Single, zx::ColorIndex{12});
ui.text(24, 136, "КАЯ:", zx::ColorIndex{15});
ui.textBox(24, 160, 208, "Привет, Димас!", zx::ColorIndex{12});
```

- `Border::Single` uses `┌─┐│└┘├┤┬┴┼`; `Border::Double` uses `╔═╗║╚╝╠╣╦╩╬`;
  `Border::Block` uses full `█` cells. The double-vertical drawing mask
  is corrected from the upstream font: it occupies pixel columns 2 and 4,
  matching the double corners and junctions instead of shifting by one pixel.
- `line(x,y,length,Axis::Horizontal/Vertical,style,color)` draws horizontal
  or vertical segments, and `box(x,y,width,height,style,color,optional_fill)`
  draws a framed rectangle. Both take either explicit RGBA or `ColorIndex`.
- Automatic junctions are chosen where 8x8 cell positions and the border
  style match. For example, a horizontal line spanning the side cells of
  a single-line box automatically draws `├` and `┤` endpoints. Crossings
  become `┼` / `╬`. Unlike lines with different styles, matching strokes
  are resolved together. A shared cell is blended only once at the color
  of the last contributing primitive.
- Width and height must be multiples of 8 and at least 16 px. Line lengths
  must be positive multiples of 8. Invalid sizes, border styles, axes,
  and color indices throw. Drawing clips against the logical canvas,
  including boxes with negative origins.
- Optional filled boxes can be opaque or translucent and are drawn in normal
  command order, before that box's border. Enqueue text after the box to
  display it over the fill. Use coordinate offsets (e.g. +8 pixels) for
  padding; content layout and widget behaviors are intentionally not part
  of this primitive.
- Palette-indexed borders and fills are resolved at render time, so F2
  recolors queued geometry just like indexed text. RGBA geometry is fixed.
- `clear()` drops text and geometry commands, not the host framebuffer.

SDL3 and any host-owned framebuffer remain outside the ZXFont core.

## Timed dialogue and cursor (v0.0.13)

`TextReveal` is an independent, frame-rate-independent component. Its default
constructor is ready to use (30 Unicode codepoints/second, enabled green
solid-block cursor, 1-pixel extra gap, 40% cursor lead). A zero speed means
instantaneous reveal. Each instance stores its own state and settings.

```cpp
zx::Context ui(256, 192);
zx::TextReveal line;
line.setText(ui, "Привет, Кая! Как дела?", 208);

// Once per frame; the host supplies measured elapsed seconds.
line.update(delta_seconds);
ui.clear();
ui.box(8, 128, 240, 56, zx::Border::Single, zx::ColorIndex{12});
line.draw(ui, 24, 152, zx::ColorIndex{15});
ui.render(host_framebuffer);

// To skip the reveal, call line.finish().
// To replay with the captured layout, call line.reset().
```

`setText(context, value, max_width)` validates UTF-8 and the current font's
glyph coverage, then captures a **complete, wrapped layout** including font and
letter spacing. It restarts the animation. Later font/spacing changes on the
context cannot move the queued text: call `setText` again to reflow in the
new font. The text reveal never creates a window or owns the framebuffer.

`RevealOptions` has sensible defaults and can be overridden per instance:

```cpp
zx::RevealOptions options;
options.charsPerSecond = 25;      // 0: instant
options.cursorEnabled = true;
options.cursorGapPx = 2;          // Extra pixel distance from preceding glyph
options.cursorLead = 0.5;        // Cursor visible in the last 50% of each interval
options.cursorBlink = false;
options.cursorAfterComplete = true;
options.cursorColor = zx::ColorIndex{10};
line.setOptions(options);         // Does not reset revealed text
```

The solid cursor is sized to the selected font's cell and uses the currently
selected palette. The lead fraction governs *when* the cursor appears before
each next glyph (0 means only after completion, 1 means throughout the
interval). The gap is extra to the current letter spacing and does not shift
text or change wrapping; it is omitted at the start of a wrapped line or
when the extra gap would cross the box boundary. After completing a completely
filled line, the cursor proceeds to the next row.
Blink toggles every 0.5 seconds when enabled. The cursor can remain after
the last glyph or disappear according to `cursorAfterComplete`. Explicit and
automatic line breaks are not delayed, while printable Unicode codepoints
(including spaces) consume one step. Layout normalization may discard spaces
at wrap boundaries before the timed sequence is prepared.

The caller passes a **measured elapsed time in seconds**, not a frame count.
`update()` rejects negative and non-finite time deltas. Options are validated
before being applied; `cursorLead` must be within [0, 1], color indices
within [0, 15], and speed finite and non-negative. To distinguish an invalid
external font from a missing glyph, use the established `Context::setFont`
and `Font::hasGlyph` APIs, respectively.

Both SDL3 demos use F6 (speed: 0/10/30/60/120 CPS), F7 (cursor gap 0..3 px),
F8 (cursor lead 25/50/100%), F9 (blink) and Enter (finish/replay). These are
demo controls, **not** core ZXFont hotkeys. Full frame CRT effects are outside
the scope of the library and were not added.


## Framed panels and bounded text (v0.0.14)

`Context::panel(x, y, width, height, title, options, borderColor,
 titleColor, optionalFill)` queues a box and an optional one-line title,
then returns `PanelArea {x, y, width, height}` for the remaining interior.
There are overloads for explicit `Color` and palette-indexed `ColorIndex`.

`PanelOptions` defaults to `Border::Single`, `padding=0`, `titleGap=8`,
`titleAlign=TextAlign::Left`. The fixed 8-pixel frame inset is always applied;
`padding` adds pixels on all sides beyond it. `titleGap` is used only if a
nonempty title is present, after one glyph-height reserved for the title.
The title does not take part in wrapping; excessively long titles are clipped.
A title containing LF is invalid. Small or misaligned panels throw before
adding any commands; dimensions must be multiples of 8 and leave room for
at least one text cell in the body, including after title placement.

`Context::panelText(area, text, color, align=TextAlign::Left)` wraps by the
same rules as `textBox()` and aligns *each wrapped line* within the supplied
`PanelArea`. Its RGBA and indexed overloads clip individual pixels at all
four sides, including partially visible glyphs. `TextAlign` supports `Left`,
`Center` and `Right`. Callers may also construct a `PanelArea` themselves.

`TextReveal::draw(context, area, color)` has two new overloads, with RGBA or
indexed text colors. Both the visible glyphs and the solid cursor are clipped
to the same bounds. For matching layout, call `setText(context, message,
area.width)` before starting the animation. Panel drawing does not alter the
selected font, spacing, or palette. It does not scroll content or manage input.

```cpp
zx::PanelOptions panel_options;
panel_options.padding = 0;
panel_options.titleGap = 8;
auto content = ui.panel(8, 128, 240, 56, "КАЯ:", panel_options,
                        zx::ColorIndex{12}, zx::ColorIndex{15},
                        zx::ColorIndex{0});

zx::TextReveal reply;
reply.setText(ui, "Привет, Димас!", content.width);
reply.update(elapsed_seconds);
reply.draw(ui, content, zx::ColorIndex{12});
```

## Scrolling long text (v0.0.15)

`TextViewport` is a small independent scroll controller for `TextReveal`. It
stores a clipped `PanelArea`, the current first row, the line height, and the
auto-follow setting. It **does not** re-decode or re-wrap the message on scroll.
`TextReveal::draw(context, viewport, color)` enqueues only the visible rows,
so hidden earlier pages are not processed on each rendered frame. Indexed
colors and cursor clipping behave exactly as in the non-scrolling overloads.

```cpp
zx::TextReveal reply;
reply.setText(ui, longAnswer, area.width);
zx::TextViewport viewport;
viewport.configure(reply, area);

// Each frame, before ui.render():
reply.update(deltaSeconds);
viewport.follow(reply);
reply.draw(ui, viewport, zx::ColorIndex{12});

// User navigation:
viewport.scrollLines(-1);  // Up one line and suspend auto-follow.
viewport.pageUp();         // Up by a whole page.
viewport.pageDown();       // Down by a whole page.
viewport.home();           // Beginning, auto-follow remains suspended.
viewport.end(reply);       // Latest revealed row, resume auto-follow.
```

- `configure(text, area)` requires `area.width` to match the width supplied to
  `TextReveal::setText()`, and a positive height; mismatched geometry throws
  `std::invalid_argument`. Call both again after changing the text, wrapping
  width, font, or letter spacing. Reconfiguring with new geometry keeps the
  current first line when possible.
- `reset()` moves to the first line and enables auto-follow; `home()` also
  moves to the first line but **disables** auto-follow.
- `scrollLines(delta)` accepts signed 64-bit line counts, clamps to the
  already revealed range, and disables following only when the position
  changes; zero and navigation blocked at an edge leave the state unchanged.
  The user cannot scroll ahead into unrevealed blank pages.
- `firstLine()` is zero-based; `pageLines()` reports completely visible rows
  (at least one). `autoFollow()` reports whether new characters can move the
  viewport. Partial bottom rows are pixel-clipped by the RGBA renderer.
- `TextReveal::caretLine()` returns the zero-based logical line where the next
  glyph would be printed; `lineHeight()` reports its captured font height.
- `end(reveal)` follows the **revealed** end, not the blank bottom of the full
  unrevealed message. While manually scrolled up, `follow(reveal)` has no
  effect. Moving with End explicitly resumes auto-follow.
- The core only handles layout, scroll state, and drawing. Input events,
  scrolling speed, and the presentation window remain the host's job.

Kaya uses `Tab` to choose the history or live response viewport, arrows for
single lines, PageUp/PageDown for pages, and Home/End. Mode256 binds its
PageUp/PageDown and Home/End to dialogue scrolling, leaving arrows to the
simulated game movement. Both demos supply long dialogue examples via F5.

## Rich text (v0.0.16)

`zx::StyledText` contains UTF-8 fragments with per-fragment `zx::TextStyle`. Fragment boundaries are not spaces or newlines; the same word-wrap algorithm processes the concatenated input. The font and letter spacing are snapped when a draw command or reveal layout is prepared.

```cpp
zx::StyledText message;
message.add("KAYA: ", zx::ColorIndex{15});
zx::TextStyle emphasis;
emphasis.foreground = zx::ColorIndex{10};
emphasis.background = zx::ColorIndex{1};
emphasis.underline = true;
message.add("important", emphasis);
message.add(" text", zx::Color{220, 200, 160, 255});

ui.styledText(8, 8, message);                 // Unwrapped, preserves spaces and LF.
ui.styledTextBox(8, 16, 180, message);        // Wrap at words, across spans.
ui.panelStyledText(area, message);           // Wrap, align, clip by panel.
reply.setStyledText(ui, message, area.width); // Timed reveal with per-glyph style.
reply.draw(ui, viewport);                    // Scrolled rendering preserves style.
```

- `StyledText::add(text, style/index/RGBA)` appends a fragment; `clear()`, `empty()` and `fragments()` manage/query it. `TextStyle::foreground` is a `TextColor` (`std::variant<Color, ColorIndex>`). `background` is an optional `TextColor`.
- `underline` draws a one-pixel line along the last glyph row; `inverse` swaps foreground/background. Missing background in inverse mode uses palette index 0.
- Backgrounds cover glyph cells, including spaces, and follow alpha blending. Indexed colors are resolved at `render()` time; explicit RGBA colors stay fixed.
- Unsupported glyphs, malformed UTF-8, and out-of-range indexed colors are rejected. Changing font requires new `setStyledText()`/`configure()` calls, just like plain text.
- Styles survive word wrapping and dropped separator spaces. `TextReveal::visibleCharacters()` still counts Unicode codepoints rather than UTF-8 bytes; viewport paints only visible rows. The `color` argument of `TextReveal::draw()` is ignored for a styled message (colors come from the fragments); its cursor still uses `RevealOptions::cursorColor`.
