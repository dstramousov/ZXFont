# ZXFont

ZXFont is an experimental C++20 bitmap-font and character-UI library. The
core is independent of windowing libraries; SDL3 is a planned optional backend.

**Current version: v0.0.3.** This milestone implements only a compact drawing
API and a built-in 8x8 printable-ASCII font rendered into a caller-owned RGBA8
pixel buffer. It has no window, SDL3 dependency, UTF-8 or UI widgets yet.

## Build and test

Requirements: CMake >= 3.21, Ninja, a C++20 compiler.

```bash
./m               # Configure and build Debug
./m test          # Build and run CTest tests
./build/zxfont_basic  # Write zxfont_demo.ppm to the working directory
```

For other build modes: `./m release`, `./m profile`, `./m rebuild`, `./m clean`.

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
ui.render({pixels, 256, 192, 256 * 4});
```

See [docs/API.md](docs/API.md) for ownership, alpha and clipping rules.

## Development scripts

Scripts resolve the repository root independently of the current directory.

| Command | Purpose |
| --- | --- |
| `./a` | Package current source as `../ZXFont_vX.Y.Z_current.zip`. |
| `./appr` | Review changes, commit with the latest `versions.md` entry and push. |
| `./m` | Configure and build Debug with CMake/Ninja. |
| `./m release` | Configure and build Release. |
| `./m profile` | Configure and build RelWithDebInfo. |
| `./m rebuild` | Remove `build/` and rebuild Debug. |
| `./m clean` | Remove `build/`. |
| `./m test` | Build and run CTest tests. |

Other requirements for tooling: Bash, Git, and `zip`.

## Patch-based workflow

1. Supply the current `ZXFont_vX.Y.Z_current.zip` as the source of truth.
2. Discuss one focused change and the next patch version.
3. From the repository root, apply the delivered patch:

   ```bash
   git apply --check ~/Downloads/ZXFont_vX.Y.Z.patch
   git apply ~/Downloads/ZXFont_vX.Y.Z.patch
   ```

4. Validate with `./m` and `./m test`; approve the commit with `./appr`.
5. Create the next current-source archive with `./a`.

Every patch increments the final version component by one and updates
`versions.md` and the CMake project version.
