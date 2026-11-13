# ZXFont

ZXFont is a C++20 bitmap-text library intended for retro games, terminals and
character interfaces. Its core is independent of graphics/window libraries;
SDL3 is used only by two optional visual test applications.

**Current version: v0.0.10.** This milestone provides a compact `zx::Context`
API, embedded ZX Spectrum Unicode 8x8 glyphs, strict UTF-8 decoding, PSF2 Unicode
font loading, RGBA8 framebuffer rendering and four switchable 16-color
palettes. GUI widgets are not implemented yet; SDL3 display glue remains demo-only.

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
| `F2` | Switch color palette | Switch overlay color palette |
| `F3` | Switch Spectrum 8×8 / Compact Airy 6×8 | Switch Spectrum 8×8 / Compact Airy 6×8 |
| `F4` | Cycle Compact Airy letter spacing 0/1/2 px | Cycle Compact Airy letter spacing 0/1/2 px |
| `F5` | Change conversation / view font matrix | Change dialogue / view font matrix |
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
F2 cycles Green, Amber, ZX, VGA. F3 switches between the **embedded**
Spectrum Unicode 8x8 and the **external** Compact Airy 6x8. F5 shows
font diagnostic scenes in both applications. The PSF2 assets are copied to
`build/fonts/` when demos or tests are configured. Border glyphs remain on
an independent 8x8 grid while dialogue text switches to 6x8. Compact
text uses 1 px extra space by default; F4 cycles through 0, 1, 2 px when
Compact is selected. Spectrum 8x8 retains its original 0 px spacing.
If the compact file cannot be opened or validated, the demo keeps the built-in
font and reports the problem to stderr instead of terminating.
Pixel-perfect output uses whole-number scaling whenever the canvas fits the
window; smaller windows use a fractional fallback to avoid cropping.

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

Render palette-indexed text independently of the selected font:

```cpp
ui.setPalette(zx::Palette::Spectrum);
ui.text(8, 56, "Spectrum red", zx::ColorIndex{10});
ui.setPalette(zx::Palette::VGA); // The already queued text becomes VGA green.
```

Direct RGBA text colors remain unchanged on palette switches. In both visual
applications F2 switches through Green, Amber, ZX Spectrum and VGA. Kaya's
entire screen uses the active palette; Mode256's game graphics remain owned by
the host and only the ZXFont overlay changes palette. Fullscreen and resized
windows use a centered aspect-preserving viewport, without cropping.

Change the font without changing the rendering API:

```cpp
std::string error;
if (!ui.setFont("fonts/zx-compact-airy-6x8.psf", &error)) {
    // The selected font is now the embedded Spectrum 8x8.
    logWarning(error);
}
ui.text(8, 40, "Кая");
```

`zx::Font{}` now embeds the **ZX Spectrum Unicode 8x8** font with 667
Unicode-mapped glyphs. It works without external files. The identical PSF2
file `fonts/zx-spectrum-unicode-8x8.psf` remains available for reproducibility.
`fonts/zx-compact-airy-6x8.psf` is the user-selected optional compact font.
Both contain 667 Unicode mappings (see [font provenance](fonts/SOURCE.md)).
The older mixed ASCII/Cyrillic font has been removed. PSF2 fonts must have
Unicode mappings and include all requested glyphs. On an absent or invalid external font,
`Context::setFont(path, &error)` automatically selects the built-in 8x8 and
returns `false`; `Font::loadPsf2()` remains strict and throws. Missing
individual Unicode glyphs are still errors, not silently substituted.
`Context::setLetterSpacing(pixels)` changes only the space between adjacent
glyphs in subsequently queued text. It does not alter glyph bitmaps, does
not add space at line ends, and preserves already queued commands. The
default is 0 px for backward compatibility.
Different fonts can be mixed **within the same frame** by calling `setFont()` between `text()` commands; commands keep
their selected font and glyph dimensions. No extra API is needed. See
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
