# Speex 1.0.5: local changes

## Provenance

- Source: the official Xiph.Org release tarball `speex-1.0.5.tar.gz` from
  `https://downloads.xiph.org/releases/speex/`, downloaded 2026-11-13.
- SHA-256 `fce83e97a795d87db84118ec45cea42691474949264e6978bc5a25482ea983b5`, matching Xiph's published
  `SHA256SUMS.txt` in the same directory.
- Licence: BSD-style, see `COPYING`.
- Why this version: it is the version the GameSpy Voice SDK pins (Voice2 `changelog.txt`, 2.07.02:
  "Fixed all VC6 projects to use Speex 1.0.5"; `Voice2/libspeex/libspeex.vcxproj` lists
  `..\speex-1.0.5\libspeex\*.c`). It is also the version BF2.exe links statically: every libspeex
  1.0.5 data table found in BF2.exe matches byte for byte (20 of 21 tables). That includes `cdbk_nb`
  at file offset 0x57B8B0, the `exc_*`, `gain_cdbk_*`, `hexc_*`, `high_lsp_cdbk*` and `cdbk_nb_*`
  codebooks, and the sub-band QMF filters `h0`/`h1`. The 21st table is explained below.

## Source changes

None. Every file is byte-identical to the tarball. `LOCAL_CHANGES.md` is the only added file.

## Build

- The OpenBF2 build compiles only the libspeex codec files GameSpy Voice2 needs (see the CMake and
  MSVC lists). The tarball's own build scripts (`configure`, `Makefile.*`, `win32/`) are kept as
  shipped and never run.
- **No `SLOW_TRIG` define.** In 1.0.5, `math_approx.c` compiles `cos_sin[102]` and `speex_cos()`
  only under `#ifdef SLOW_TRIG`, and `nb_celp.c` maps `cos` to `speex_cos` only under the same
  guard. Nothing in the tarball or in GameSpy's project defines it.
- Retail is the same default build. BF2.exe has no `cos_sin` table, and its Speex code calls CRT
  `cos` as inline x87 `fcos` (MSVC `/Oi` intrinsic), at 12 sites in 0x738000..0x752000. Two examples:
  - `0x73D021` is `nb_encoder_init`'s analysis window, `.54 - .46*cos(M_PI*i/part1)`. Its constants
    are 0.54 at `0x921AD8`, 0.46 at `0x921AE0` and pi at `0x921AE8`, stored to `st->window[i]`.
  - `0x73EAB9` is `nb_encode`: `lsp_enforce_margin(..., 0.05f)` at `0x73EA9F`, then
    `interp_lsp[i] = cos(interp_lsp[i])` in place.

## Standard-math note

Native builds call the platform libm `cos` (SSE2 on x64, NEON/scalar on arm64), while retail uses
x87 `fcos`. The two can differ in the last bits of a float (window coefficients and the
interpolated LSPs). This follows the owner's standard-math policy for numeric/CRT edge rows
(2026-11-09): CRT behaviour is kept, not emulated. The Speex
bitstream format is unaffected, so frames still interoperate with retail clients and servers.
