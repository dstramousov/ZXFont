# ZX Spectrum Unicode 8×8 font

This directory contains `zx-spectrum-unicode-8x8.psf`, a PSF2 font with
667 Unicode-mapped glyphs, generated offline from the **Fore** outlines in
`build/ZXSpectrum.sfd` in the project below:

- Project: https://github.com/jfsebastian/zx-spectrum-unicode-font
- Author: Jorge Ferrer (JFSebastian).
- Source revision (master): `67e9c4d40d8a77ffcecf3c7eef6fde8ba977ce7f`.
- Font project license: Unlicense (public domain dedication).
- Original inspiration: Darko Stanicic's ZX Spectrum TrueType font, described
  by the upstream author as "healthware". That earlier provenance should be
  reviewed before distributing a commercial product based on this asset.

The PSF2 output has exactly one bit per pixel, **8×8**, with no antialiasing
or interpolation. `tools/sfd_to_psf2.py` reads the pinned FontForge SFD source,
uses even-odd polygon filling, and samples pixel centers on its original
100-units-per-pixel grid, starting at baseline ascent 700.

Rebuild (requires downloading the upstream SFD, *not* required by consumers):

```bash
python3 tools/sfd_to_psf2.py ZXSpectrum.sfd fonts/zx-spectrum-unicode-8x8.psf
```

Expected SHA256 of the resulting PSF2:
`e679311e14a3aa85c64617577419376cfc828a56afc917ffcf91d8ece370eb98`

**PSF2 is runtime data, not a TTF dependency.** All demo font loading uses
`zx::Font::loadPsf2()`; the ZXFont core API and library remain unchanged.

The default `zx::Font{}` uses the embedded representation of this PSF2
asset, rather than the old mixed bitmap font. To regenerate its checked-in
C++ source from the exact PSF2 file (Python 3.12+, build-time only):

```bash
python3 tools/embed_psf2.py fonts/zx-spectrum-unicode-8x8.psf src/spectrum_unicode_8x8.hpp
```

The embedded bytes and external file share the same PSF2 parser, so coverage
and rasterized glyphs must match exactly. Both are tested in `zxfont_tests`.
