# SwiffPlayer Expat substitute

[CHOSEN, not recovered] Owner decision 2026-11-08: use official libexpat
1.95.0 as a documented substitute for DICE's modified Expat. Retail
`SwiffPlayer.dll` links a modified C++ parser whose exact source is no longer
available: James Clark's 1.x distribution site is gone and was not archived.
Retail differs from every published release inspected, including a 328-word
`namingBitmap` versus 320 and older xmlrole role numbering. The swf track identified it
against the published releases.
This dependency is not recovered DICE source or an exact retail match.

The known behavior difference is XML name-character classification: retail's
extra bitmap page 40 (words 320–327), referenced by the name page-table entry
for U+30xx, and differing name/name-start page-table entries admit additional
code points. The identification report's full BMP comparison found 385 additional
name-start classifications and 402 additional name classifications in retail;
none were unique to the official 1.95.0 tables. The substitute retains upstream
classification. Older role numbering also differs internally; no claim of
complete parser equivalence or an exhaustive behavioral-difference list is made.

## Source and license

- Source: <https://github.com/libexpat/libexpat/tree/R_1_95_0>
- Tag: `R_1_95_0` (official release 1.95.0).
- Archive: <https://github.com/libexpat/libexpat/archive/refs/tags/R_1_95_0.tar.gz>
- Archive SHA-256: `0d0ea80686a51330e34f541dde726c7bb4914fe0cbbd167fb8f724d160400846`.
- License: MIT (Expat), copyright the original authors, Thai Open Source
  Software Center Ltd and Clark Cooper. Preserve [COPYING](COPYING) and all
  source notices. The old C++ files elsewhere in the archive are not vendored.

Only `expat/COPYING` and the required `expat/lib/` sources/headers were copied.
All upstream files are unchanged. For each file below, `tar -xOf` from the
owner-supplied archive was piped through `shasum -a 256` and compared with the
read-only extracted tree before copying. The vendored copy was also checked.
No upstream test, documentation or build script was copied or executed.

| File | Archive SHA-256 | Extracted and vendored SHA-256 |
| --- | --- | --- |
| COPYING | `0e5a15440a08015d814b5fee7c1aac9f6c56ee6fc8e8288cba1eef5abd259bfc` | `0e5a15440a08015d814b5fee7c1aac9f6c56ee6fc8e8288cba1eef5abd259bfc` |
| xmlparse.c | `a0aa5a78b63dd74372332e491a57d464070952a09c286d0b24f276b373abbda9` | `a0aa5a78b63dd74372332e491a57d464070952a09c286d0b24f276b373abbda9` |
| xmltok.c | `bc9b77523e30046e2985ea000baccbbb58735c56824c8ca5b7ffe9d3f34d7ab1` | `bc9b77523e30046e2985ea000baccbbb58735c56824c8ca5b7ffe9d3f34d7ab1` |
| xmltok_impl.c | `035e5fde53aacfd38b4dd499ab5bc0d64eb8574b305e24177a44d3358cde481d` | `035e5fde53aacfd38b4dd499ab5bc0d64eb8574b305e24177a44d3358cde481d` |
| xmltok_ns.c | `a02dac0b291801fe427f7642e63e55cf319e047b19539e246bea7d10293d948c` | `a02dac0b291801fe427f7642e63e55cf319e047b19539e246bea7d10293d948c` |
| xmlrole.c | `744268a80b6f328afddc6d09142d56f8e4ecad686e755b81e8f5b14cc97a221a` | `744268a80b6f328afddc6d09142d56f8e4ecad686e755b81e8f5b14cc97a221a` |
| expat.h | `69aaf81d7f0b1ad7cfb99c70392421f52a6c2610c46b71f39d57cf43e9d152a5` | `69aaf81d7f0b1ad7cfb99c70392421f52a6c2610c46b71f39d57cf43e9d152a5` |
| xmlrole.h | `552324a96e741a748e29be322b48329c21427d19d0961ee69229bab608ca193b` | `552324a96e741a748e29be322b48329c21427d19d0961ee69229bab608ca193b` |
| xmltok.h | `0d436cbd3a3f313aeb7ab4601af3ae917f560abd24f70728625207d4250fae04` | `0d436cbd3a3f313aeb7ab4601af3ae917f560abd24f70728625207d4250fae04` |
| xmltok_impl.h | `577149bf7372ad62e8ad3e36ba82686c90efac0b694a19270c7b728999af7a39` | `577149bf7372ad62e8ad3e36ba82686c90efac0b694a19270c7b728999af7a39` |
| ascii.h | `00455fbd18eff9b9d444da082f9710189042c2b6d3907f3f863300aff9d1e484` | `00455fbd18eff9b9d444da082f9710189042c2b6d3907f3f863300aff9d1e484` |
| asciitab.h | `bfcf3aeff5a7ab136b54e86bd54dfe79ef610ebc792ff13a1dc0f0575e5912d4` | `bfcf3aeff5a7ab136b54e86bd54dfe79ef610ebc792ff13a1dc0f0575e5912d4` |
| iasciitab.h | `d8d1746408ac696427578ffd3491fb1714dec19bc58a39aca5cc142ba3114f92` | `d8d1746408ac696427578ffd3491fb1714dec19bc58a39aca5cc142ba3114f92` |
| latin1tab.h | `e46ab81b6bd03431b5878b8cc2db1098aa9eeaa8068a52f63fb2ec29a286f05e` | `e46ab81b6bd03431b5878b8cc2db1098aa9eeaa8068a52f63fb2ec29a286f05e` |
| nametab.h | `63a71bfd650c5a883aa630a042ec1636307773988ae648305c90c0036aa0335b` | `63a71bfd650c5a883aa630a042ec1636307773988ae648305c90c0036aa0335b` |
| utf8tab.h | `1e941e1bd2c5a94c8e4c269f3ec419c6ccbfa5008a186e3451d8f372429406b6` | `1e941e1bd2c5a94c8e4c269f3ec419c6ccbfa5008a186e3451d8f372429406b6` |

## OpenBF2 integration

`config.h` is a separate OpenBF2 file, [CHOSEN, not recovered]. The build adds
this directory to the parser include path so upstream's `<config.h>` resolves
it. It selects native byte order, standard memory functions and narrow UTF-8
callbacks, with namespace processing and parameter-entity/DTD support disabled
as observed in retail. No upstream file is force-edited. The three C translation
units are `xmlparse.c`, `xmltok.c` and `xmlrole.c`; `xmltok_impl.c` and
`xmltok_ns.c` are included fragments and must not be compiled independently.

CMake builds `bf2_expat_static` and links it to the macOS SwiffPlayer/client.
`Code/Build/swiffplayer.cmd` uses `/TC` and `lib.exe` for both MSVC x86/x64.
Every C compile selects `DEDICATED=0` and force-includes `Core/BuildConfig.h`.
The native C++ facade in `gameswf_xml.cpp` preserves the inspected create,
handler-storage and XML-load call sequence, adapting parser-owned state and
callbacks to the C API. It does not reproduce the retail parser layout or
30-slot virtual interface. All platforms use the recovered XML load behavior:
non-final input, status 0 only for parse result 1, parser release before onData,
and a String source argument even on parse failure. Existing S3 String/graph/VM
bridges remain chosen. The dependency does not enable the XML factories in the
action layer; publication and native teardown integration remain maintainer work.
