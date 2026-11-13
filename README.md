# ZXFont

ZXFont is a C++20 bitmap-text library intended for retro games, terminals and
character interfaces. Its core is independent of graphics/window libraries;
SDL3 is used only by two optional visual test applications.

**Current version: v0.0.5.** This milestone provides a compact `zx::Context`
API, embedded 8x8 ASCII/Cyrillic glyphs, strict UTF-8 decoding, PSF2 Unicode
font loading and RGBA8 framebuffer rendering. GUI widgets and palette
switching are not implemented yet; SDL3 display glue remains demo-only.

## Build and test

Requirements: CMake >= 3.21, Ninja, a C++20 compiler. The visual demos use
SDL3 >= 3.2. When SDL3 is not installed, the top-level CMake build downloads
a pinned SDL3 release (requires Git and network access on first configure).
The core library itself does not depend on SDL3.

```bash
./m                    # Configure and build Debug
./m test               # Run CTest tests
./build/zxfont_basic   # Write zxfont_demo.ppm to the current directory
./build/zxfont_kaya    # Full-screen mock terminal (F11 for windowed mode)
./build/zxfont_mode256 # Windowed game-host integration simulator
```

Other modes: `./m release`, `./m profile`, `./m rebuild`, `./m clean`.

### Visual test applications

Both demos are built by `./m` and linked against the same `ZXFont::zxfont`
static library. Each SDL3 application owns its window, events, and pixel
buffer. **ZXFont only draws text into the supplied RGBA8 buffer.** The demos
render ASCII frames with ZXFont until actual box primitives are added.

| Key | Kaya Terminal | Mode256 Simulator |
| --- | --- | --- |
| `Esc` | Exit | Exit |
| `F1` | Toggle help | Toggle help |
| `F5` | Change mock conversation | Change mock dialogue |
| `F11` | Toggle fullscreen | Toggle fullscreen |
| `Space` | — | Show/hide dialogue |
| `Left/Right` | — | Move game character |

The interactive SDL3 demos are enabled only when ZXFont is built as a
standalone project (or `ZXFONT_BUILD_DEMOS=ON` is specified explicitly).
When ZXFont is embedded via `add_subdirectory()`, no SDL3 is required.
To build the core and tests without SDL3, configure:

```bash
cmake -S . -B build -G Ninja -DZXFONT_BUILD_DEMOS=OFF
cmake --build build
```

To forbid downloading SDL3 when it isn't installed, use
`-DZXFONT_FETCH_SDL3=OFF`. SDL3 is never linked into the core library.
`F2`/`F3` are reserved for future palette/font switching, not implemented yet.

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
