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
