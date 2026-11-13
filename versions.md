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
