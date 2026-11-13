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

## ZX Compact Airy 6×8 (v0.0.9)

The `zx-compact-airy-6x8.psf` font is the **user-selected right-hand prototype**
from the comparison made for ZXFont, not the discarded *Dense* variant.
It is derived from the above Spectrum Unicode PSF2 using five occupied bitmap
columns plus one spacing column, with hand-corrected Russian/Ukrainian Cyrillic
and digits. The result contains the same 667 Unicode mappings and remains
monochrome (no TTF engine or runtime converter). Some special line-drawing
glyphs may need manual refinement at 6x8 in a later typography iteration.

Rebuild using only the Python 3 standard library:

```bash
python3 tools/make_compact_airy.py fonts/zx-spectrum-unicode-8x8.psf fonts/zx-compact-airy-6x8.psf
```

SHA256: `e82b08d2561fcbdd02106b610eeaef610497fb160d59ac737a30233c866fdd9a`

The Spectrum 8x8 source license/provenance noted above also applies to this
derivative. ZXFont embeds only the Spectrum font; Compact Airy remains a
separate optional PSF2 file for safe loading and replacement.
