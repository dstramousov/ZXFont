# Third-party bitmap font data

`src/basic_ascii_8x8.hpp` contains the printable Basic Latin glyphs from
`dhepper/font8x8`, `font8x8_basic.h`:

- Source: https://github.com/dhepper/font8x8
- Author: Daniel Hepper, based on Marcel Sondaar / IBM PC bitmap fonts.
- Source license: **Public Domain** (as stated by the upstream author).

The glyphs are temporary built-in test data, not a representation of the
original ZX Spectrum ROM font. They can later be replaced or supplemented by
user-defined fonts without changing the drawing API.

## Go Mono: built-in Cyrillic 8x8 bitmaps

- Source: https://go.googlesource.com/image/+/master/font/gofont/ttfs/
- Original work: Go Mono, Copyright (c) 2016 Bigelow & Holmes Inc.
- License: BSD 3-Clause (not the ZXFont library's code license).
- The 76 bundled Cyrillic bitmaps in `src/basic_cyrillic_8x8.hpp` are
  8x8 monochrome rasterizations of glyphs from Go Mono, produced as part
  of ZXFont development; the original font file is not included.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of Google Inc. nor the names of its contributors may be
   used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
