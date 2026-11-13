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
