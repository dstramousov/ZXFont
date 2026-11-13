# ZXFont

ZXFont is a C++20 bitmap-text library intended for retro games, terminals and
character interfaces. Its core is independent of graphics/window libraries;
SDL3 is a planned optional backend.

**Current version: v0.0.4.** This milestone provides a compact `zx::Context`
API, embedded 8x8 ASCII/Cyrillic glyphs, strict UTF-8 decoding, PSF2 Unicode
font loading and RGBA8 framebuffer rendering. GUI widgets, palettes and SDL3
rendering have not yet been implemented.

## Build and test

Requirements: CMake >= 3.21, Ninja, a C++20 compiler.

```bash
./m                    # Configure and build Debug
./m test               # Run CTest tests
./build/zxfont_basic   # Write zxfont_demo.ppm to the current directory
```

Other modes: `./m release`, `./m profile`, `./m rebuild`, `./m clean`.

## Integrating in another CMake project

```cmake
add_subdirectory(path/to/ZXFont)
target_link_libraries(my_app PRIVATE ZXFont::zxfont)
```

```cpp
#include <zxfont/zxfont.hpp>

std::vector<std::uint8_t> pixels(256 * 192 * 4, 0);
zx::Context ui(256, 192);
ui.text(8, 8, "HELLO ZXFONT", {0, 255, 0, 255});
ui.text(8, 24, "Привет, Кая!");
ui.render({pixels, 256, 192, 256 * 4});
```

Change the font without changing the rendering API:

```cpp
zx::Font font = zx::Font::loadPsf2("my_font.psf");
ui.setFont(font);
ui.text(8, 40, "Кая");
```

PSF2 fonts must have Unicode mappings and include all requested glyphs. See
[docs/API.md](docs/API.md) for validation, text and glyph behavior, and
[docs/THIRD_PARTY.md](docs/THIRD_PARTY.md) for bitmap attribution.

## Development scripts

Scripts resolve the repository root independently of the working directory.

| Command | Purpose |
| --- | --- |
| `./a` | Package source as `../ZXFont_vX.Y.Z_current.zip`. |
| `./appr` | Review, commit using the newest `versions.md` entry and push. |
| `./m` | Configure and build Debug with CMake/Ninja. |
| `./m release` | Configure and build Release. |
| `./m profile` | Build RelWithDebInfo. |
| `./m rebuild` | Remove `build/` and rebuild Debug. |
| `./m clean` | Remove `build/`. |
| `./m test` | Build and run CTest tests. |

Other requirements for tooling: Bash, Git and `zip`.

## Patch-based workflow

1. Supply the current `ZXFont_vX.Y.Z_current.zip` as the source of truth.
2. Agree on one focused change and the next patch version.
3. From the repository root, apply the delivered patch:

   ```bash
   git apply --check ~/Downloads/ZXFont_vX.Y.Z.patch
   git apply ~/Downloads/ZXFont_vX.Y.Z.patch
   ```

4. Validate with `./m` and `./m test`; approve using `./appr`.
5. Create the next source archive using `./a`.

Every patch increments the final version component by one and updates
`versions.md` and the CMake project version.
