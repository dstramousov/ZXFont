# Third-party font data

## ZX Spectrum Unicode 8x8

`fonts/zx-spectrum-unicode-8x8.psf` and its embedded byte representation
`src/spectrum_unicode_8x8.hpp` are generated from the FontForge SFD outlines
of [JFSebastian/zx-spectrum-unicode-font](https://github.com/jfsebastian/zx-spectrum-unicode-font).

- Author: Jorge Ferrer (JFSebastian).
- Project license: Unlicense (public domain dedication).
- The upstream README describes derivation from Darko Stanicic's Spectrum
  TrueType font, released as "healthware". That older provenance should be
  reviewed before distributing a commercial product based on this asset.
- Source revision, rasterization method, PSF2 hash and regeneration commands:
  see [fonts/SOURCE.md](../fonts/SOURCE.md).

ZXFont does **not** include the legacy mixed IBM PC / Go Mono bitmaps used
in earlier versions. Those font headers and their licensing notes were
removed in v0.0.8.
