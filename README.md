# ZXFont

ZXFont is a planned C++20 bitmap-font and character-UI library. The core will
be independent of windowing libraries; SDL3 will be an optional backend.

The repository currently contains only its development scripts. No CMake
project or font-rendering library has been implemented yet.

## Development scripts

Run these scripts from any directory; they resolve the repository root
independently of the current working directory.

| Command | Purpose |
| --- | --- |
| `./a` | Package the current source tree as `../ZXFont_vX.Y.Z_current.zip`. |
| `./appr` | Review changes, commit with the latest `versions.md` entry, and push. |
| `./m` | Configure and build Debug with CMake and Ninja (once CMake exists). |
| `./m release` | Configure and build Release. |
| `./m profile` | Configure and build RelWithDebInfo. |
| `./m rebuild` | Remove `build/` and rebuild Debug. |
| `./m clean` | Remove `build/`. |
| `./m test` | Build and run configured CTest tests. |

Requirements for these scripts: Bash, Git, `zip`, and (for builds) CMake + Ninja.

## Patch-based workflow

1. Supply the current `ZXFont_vX.Y.Z_current.zip` as the source of truth.
2. Discuss one focused change and the next patch version.
3. Apply the delivered patch from the repository root:

   ```bash
   git apply --check ~/Downloads/ZXFont_vX.Y.Z.patch
   git apply ~/Downloads/ZXFont_vX.Y.Z.patch
   ```

4. Validate the change and use `./appr` to review, commit, and push it.
5. Use `./a` to create the next current-source archive.

Every patch increments the final version component by one and updates
`versions.md`. Until a CMake project exists, that file is the version source.
