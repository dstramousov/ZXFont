# Versions

## v0.0.0 -> v0.0.1

- init repo

## v0.0.1 -> v0.0.2

- Adapt source archive, build and approval scripts for standalone ZXFont.
- Make development commands independent of the caller working directory.
- Validate version transitions before committing and handle first pushes.
- Document the patch-based workflow and ignore generated artifacts.

## v0.0.2 -> v0.0.3

- Bootstrap the C++20 ZXFont static library and CMake integration target.
- Add a small Context API that renders 8x8 ASCII text into caller-owned RGBA8 buffers.
- Implement clipping, stride validation, alpha blending and basic error handling.
- Add a no-window PPM demo, headless tests, and API/font provenance documentation.

## v0.0.3 -> v0.0.4

- Separate shareable bitmap Font data from the text renderer without changing the existing text/render API.
- Decode UTF-8 strictly and support built-in 8x8 Cyrillic characters alongside ASCII.
- Add external PSF2 Unicode font loading and per-command font snapshots via Context::setFont.
- Extend tests for Cyrillic, font switching, non-8-bit widths, malformed UTF-8 and invalid font files.
- Update the headless demonstration, API documentation and font provenance.

## v0.0.4 -> v0.0.5

- Add a full-screen SDL3 Kaya Terminal visual test with mock Cyrillic conversations.
- Add a windowed Mode256-style host that draws its own pixel world and overlays ZXFont text.
- Keep SDL3 outside the ZXFont core; provide automatic pinned SDL3 retrieval for standalone builds.
- Document visual demo controls, build options, and the framebuffer ownership boundary.

## v0.0.5 -> v0.0.6

- Add four built-in 16-color palettes: Mono Green, Mono Amber, ZX Spectrum, and VGA.
- Keep explicit RGBA text unchanged; add indexed text recolored at render time via Context::setPalette.
- Enable F2 palette cycling in Kaya Terminal and Mode256 Simulator without changing fonts or game-world pixels.
- Fit SDL3 output to the physical window/fullscreen size with a letterboxed viewport to prevent clipping.
- Add palette, recoloring, and viewport regression tests; document the new API and controls.

## v0.0.6 -> v0.0.7

- Add a consistent ZX Spectrum Unicode 8x8 PSF2 font with 667 mappings, including Russian/Ukrainian letters and box drawing.
- Preserve the legacy built-in font for A/B visual comparisons; F3 switches fonts in both SDL3 demos.
- Add F5 typography diagnostic screens to Kaya Terminal and Mode256 Simulator.
- Prefer integer pixel scaling in windowed and fullscreen output, falling back to fractional downscaling only for undersized windows.
- Document upstream font provenance and a reproducible offline SFD-to-PSF2 converter; test glyph shapes and pixel-perfect viewports.

## v0.0.7 -> v0.0.8

- Remove the previous mixed IBM PC / Go Mono 8x8 built-in font and its obsolete license text.
- Embed ZX Spectrum Unicode 8x8 as the sole default font, with no external runtime file needed.
- Use one validated PSF2 decoder for embedded glyphs and external fonts; add reproducible embedding tool.
- Keep the minimal `Context::setFont()` / `text()` API and test independent font snapshots, dimensions and built-in/PSF2 pixel equivalence.
- Repurpose F3 in both SDL3 demos to compare identical embedded/PSF2 font sources; retain F5 typography diagnostics.
- Update documentation and asset provenance without adding font families prematurely.

## v0.0.8 -> v0.0.9

- Add user-selected ZX Compact Airy 6x8 as an optional PSF2 font; do not include rejected Dense variant.
- Retain embedded Spectrum Unicode 8x8 as default and automatic fallback on missing, unreadable or invalid external fonts.
- Introduce compact `Context::setFont(path, &error)` returning load status without changing the existing explicit `Font` API.
- Switch F3 between distinct 8x8 and 6x8 fonts in both SDL3 demos while preserving 8x8 border geometry.
- Add focused font coverage, failure/fallback, bitmap and mixed-font regression tests plus reproducible font generator.
- Document font source, fallback behavior and updated visual controls.

## v0.0.9 -> v0.0.10

- Add `Context::setLetterSpacing(uint32_t)` with 0 px default and per-text-command snapshots.
- Apply spacing only between adjacent glyphs, preserving LF positions, font bitmaps, and existing palette behavior.
- Give Compact Airy 6x8 a 1 px default gap in both SDL3 demos; F4 cycles 0 / 1 / 2 px.
- Preserve 0 px spacing for embedded Spectrum 8x8 and independent 8x8 border rendering.
- Add regression tests and document the new control without introducing layout or UI dependencies.

## v0.0.10 -> v0.0.11

- Add UTF-8-aware pixel measurement and word wrapping through `measureText()` and `textBox()`.
- Respect selected font metrics, letter spacing, explicit newlines, and overlong words.
- Render dialogue text using the same layout path in Kaya Terminal and Mode256 Simulator.
- Add regression tests for wrapping, errors, per-command font snapshots, and metrics.

## v0.0.11 -> v0.0.12

- Correct double-border vertical pixel columns to match corners and junctions; add pixel-level seam regression checks.
- Add `Context::box()` and `Context::line()` for 8x8 pseudographics without introducing a GUI runtime.
- Support single, double, and solid-block borders; horizontal and vertical lines; automatic junction glyphs on the same grid.
- Add optional RGBA/palette-indexed panel fills, clipping, and per-command drawing order.
- Keep frame geometry on the embedded Spectrum Unicode 8x8 grid independently of selected 6x8 text and letter spacing.
- Move Kaya and Mode256 demos from hand-built text frames and separate border contexts to the shared ZXFont primitives.
- Add regression tests for junctions, fonts, colors, validation, and clipped geometry.

## v0.0.12 -> v0.0.13

- Add independent `TextReveal` with configurable speed, solid-block cursor lead/gap/blink, and instant mode.
- Keep one precomputed UTF-8 layout per message, retaining font and spacing for stable word wraps.
- Drive reveal progression from elapsed real time rather than frame count.
- Queue text and cursor through the existing Context RGBA renderer; no SDL3 dependency in core.
- Demonstrate live tuning and skipping the effect in both Kaya and Mode256, with time/layout regression tests.

## v0.0.13 -> v0.0.14

- Add bounded panels with configurable border, padding, single-line title and title alignment; return the usable content rectangle.
- Add wrapped panel text with per-line left/center/right alignment and exact pixel clipping, including partial glyph rows.
- Support drawing animated text and its cursor within the same clipping rectangle.
- Rework the Kaya dialogue into history and live-response panels and use panel layout in Mode256.
- Replace the root README with a concise Russian overview, screenshot placeholders, integration guide and public API reference; keep version history only here.
- Add panel geometry, alignment, clipping, palette and validation tests.

## v0.0.14 -> v0.0.15

- Add `TextViewport` with per-instance line/page scrolling, Home/End, and automatic follow of the current reveal position.
- Render only pre-laid-out visible rows; keep the wrapping and font snapshot fixed while scrolling.
- Suspend auto-follow on manual navigation and resume it with End; keep the animated cursor clipped to its viewport.
- Add independent history/reply navigation in Kaya (Tab switches the active panel) and page scrolling for Mode256 dialogue without changing movement controls.
- Extend pixel tests, long-history tests, the README API reference, and detailed API documentation.

## v0.0.15 -> v0.0.16

- Add `StyledText` fragments with independent indexed or RGBA foreground and optional background, underline and inverse video.
- Keep rich-text word wrapping continuous across fragment boundaries; preserve glyph styles through `TextReveal` and `TextViewport` without re-layout during scrolling.
- Support `Context::styledText()`, `styledTextBox()`, `panelStyledText()` and `TextReveal::setStyledText()`; leave plain-text API unchanged.
- Resolve indexed foreground/background at render time so palettes remain switchable; clip backgrounds and underlines to panels.
- Highlight dialogue speakers and key phrases in Kaya and Mode256; cover wrapping, blending, validation, cursor, palette and viewport behavior with tests.
- Update the root README API reference and `docs/API.md` without duplicating the version log in README.

## v0.0.16 -> v0.0.17

- Add `Context::setLineSpacing(pixels)` (default 0) and capture it independently per text command and prepared `TextReveal`; include it in metrics, wrapped lines, panel alignment/clipping, cursor advance, and viewport page geometry.
- Preserve Spectrum INVERSE video in styled text across the entire glyph cell including spaces; verify explicit and default PAPER colors, dynamic palettes, and 8x8 / 6x8 fonts with pixel-level tests.
- Showcase inverse highlighting in dialogue demos, and add F10 line spacing (0..3) without introducing a configuration file; update the README/API reference.
- Close the font/text rendering feature cycle. CRT postprocessing is a separate future module, not part of this release.
