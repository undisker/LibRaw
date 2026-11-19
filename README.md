# LibRaw - Patched Version (undisker)

**Version: 0.23.0-patched-undisker** (tracking upstream LibRaw `master`, snapshot 202611, ABI/shlib current `27`)

This is a security-patched fork of LibRaw with safety improvements and build system
enhancements, kept in sync with the upstream [LibRaw/LibRaw](https://github.com/LibRaw/LibRaw)
`master` branch.

The host application ships a shared library built from this fork. **[BINARIES.md](BINARIES.md)**
is the authoritative description of how that `raw.dll` / `libraw.dylib` / `libraw.so` is
produced, which symbols the host application requires, what each `LIBRAW_ENABLE_*` switch costs in
supported formats, and the procedure for the next upstream sync.

## Formats added in this fork (November 2026)

A run over the whole raw.pixls.us corpus (2020 files) is the basis; see the commit
messages for how each layout was established.

- **GoPro GPR** (HERO5-HERO11, Fusion): native VC-5 decoder, `src/decoders/vc5.cpp`, no
  GoPro or Adobe SDK. Bit-identical to GoPro's reference decoder on all 17 corpus files;
  40 wavelet bands decode in parallel (4000x3000 in about 60 ms on 4 cores). **It
  requires legal clarification (patents) and is OFF by default**
  (`LIBRAW_ENABLE_GPR`). The default build decodes GPR with GoPro's own open-source
  VC-5 decoder instead (`LIBRAW_ENABLE_GPR_SDK`), with a bounds-checking patch for
  damaged files: same output, slower - see [LICENSING.md](LICENSING.md).
- **DNG with JPEG XL compression** (DNG 1.7; Samsung Expert RAW): through libjxl,
  `LIBRAW_ENABLE_JXL`, without the Adobe DNG SDK.
- **ARRIRAW** `.ari` (header version 3).
- **Headerless dumps**: Canon PowerShot G7 X and IXY 220F (CHDK), Paralenz Dive Camera,
  SJCAM SJ6 LEGEND full frame, Xiro Xplorer V, ImBack ImB35mm.
- **Raspberry Pi** `.raw`: `LIBRAW_ENABLE_6BY9RPI` is now ON by default.
- **Sony lossless ARW**: tile-parallel decoding (upstream PR #886, carried as a cherry-pick).

Of the 2020 corpus files, 2014 unpack in the default configuration (1997 with both GPR
decoders off); the rest are two Casio QV `.cam`, one Light L16 `.lri` and three
files that are not raw data at all.

Licences and provenance of everything in this fork: [LICENSING.md](LICENSING.md).

## Latest Upstream Sync (November 2026)

Merged up to upstream `master` at **70513b5** (PR #893, public snapshot 202611), 42 commits
past the previous sync point. Three conflicts: the version header (upstream numbers, fork
tail), `crx.cpp` (line endings only - the fork stores it LF, upstream CRLF; the checked
allocation sizes are kept) and `wavelet_denoise` (upstream restored the `size` assignment;
the fork's checked variant is kept).

Adopted in this sync:

- **New decoder**: Nikon HE / HE\* (`src/decoders/nikonhe.cpp`, added to the CMake list).
- **Hardening**: CR3/CRX output and plane buffer limits, 64-bit offsets and clamped
  quantisation-table indices; X3F size and offset checks; TIFF strip-size arithmetic in
  64 bits; CIFF 32/64-bit conversion; lossless-DNG `tiff_samples` check; Sony YCbCr
  decode-cache index mask; Panasonic 8 shift mask; `getreal` rejects NaN/Inf; DNG tone
  curve initialised before `OpcodeList2`; Kodak body-name overrun.
- **Correctness**: X-Trans interpolation now reads original pixel values (not ones already
  adjusted by the previous tile) and finds the half-size green site from the CFA pattern;
  Canon R50V metadata; better Hasselblad model parsing; Nikon Z5 II colour matrix.
- **API/ABI**: version 0.23.0, shlib current 27, new `LIBRAW_ABI_VERSION`;
  `libraw_colordata_t`, `libraw_raw_unpack_params_t` and the Canon makernotes struct each
  gained a field, so **struct offsets moved**. The host application is unaffected - it only calls
  functions and mirrors `libraw_processed_image_t`, which did not change - but any caller
  that reads `libraw_data_t` fields by offset must be rebuilt against the new headers.

Performance work done on top of this sync (output bit-identical, see
[BINARIES.md](BINARIES.md) section 1):

- Linear, VNG, DCB and AAHD demosaic were single-threaded; they now run in parallel.
- AHD (the default): cheaper homogeneity map and direction merge, CIELab conversion
  inlined into the tile loop.
- `pre_interpolate` green copy parallelised; `copy_bayer` no longer calls `fcol()` per pixel.

## Previous Upstream Sync (August 2026)

Merged up to upstream `master` at **df226ea** (PR #846), 35 commits past the previous
sync point. Conflicts were confined to the three files where this fork replaced raw
arithmetic with checked arithmetic; in each case the fork's checked version was kept and
the upstream addition applied on top. See the merge commit for the worked resolutions -
including `wavelet_denoise`, where upstream's current code leaves `size` uninitialised in
the OpenMP variant that this fork builds.

Adopted in this sync:

- **New decoder**: Sony ARW6 CRAW HQ (`src/decoders/sony_arw6.cpp`) - tiled streams,
  stream version 1, non-16-aligned heights, Adobe-matching highlight handling, adjusted
  black/white/linear_max points.
- **New cameras**: Sony ILCE-7M5, Canon R50V, Sony A1 II model-string cleanup, Olympus
  metadata rework.
- **Hardening**: read buffers zeroed before `fread`; FP DNG tile-index and `ifd->bps`
  checks plus a `convertFloatToInt` overflow guard; `open_bayer()` input validation and a
  minimum-size floor in `vng_interpolate`; crx plane size checked against
  `rawparams.max_raw_memory_mb` and an int32 overflow guard; Olympus 14-bit `wbits`
  validation; PPM16 thumbnails checked against `LIBRAW_MAX_THUMBNAIL_MB`;
  `trimSpaces`/`removeExcessiveSpaces` empty-string handling.

## Previous Upstream Sync (June 2026)

Merged up to **PR #799** (Hasselblad X2D II 100C support), reviewed with a comparative
audit (no regressions; all fork-specific protections retained). Highlights adopted from
upstream:

- **Security / hardening**:
  - Stack-memory / previous-image metadata exposure fix in `tiff.cpp` (reported by DMSAN)
  - TALOS CVE fixes 2330, 2331, 2358, 2359, 2363, 2364 — integer-overflow and EOF hardening
    across the X3F decoder, FP/deflated-DNG loader, CR3 parser, Panasonic enc8, Nikon
    12-bit and Sony YCC paths
  - `wavelet_denoise`: allocation size capped below 4 GB
  - `unpack_thumb`: thumbnail size validated before allocation; allocation failures now
    surface via the throwing `LibRaw::malloc`/`calloc` allocators
- **Camera support**: Canon EOS Kiss M2, Sony A1 Mark II, Hasselblad X2D II 100C, plus
  crop / colour-matrix corrections.

## Fork-specific Changes

### Security Fixes

- **Safe Math Library** (`internal/libraw_safe_math.h`):
  - Integer overflow protection for multiplication, addition
  - Division by zero prevention with safe defaults
  - Safe array index clamping and bounds checking
  - Safe bit shift operations
  - Allocation size validation

- **Division by Zero Fixes**:
  - `kodak.cpp`: Kodak_DCR_WBtags, KodakIllumMatrix
  - `decoders_dcraw.cpp`: cr2_slice calculation

- **Overflow Protection**:
  - Sony YCC decoder tile calculations (`sonycc.cpp`)
  - Thumbnail allocation sizes (`thumb_utils.cpp`)
  - File seek offset calculations (INT64 casts)

These fork patches coexist with the upstream fixes above and are exercised by the security
test suite in `tests/test_security_fixes.cpp` (62 checks).

### Build System

- CMake configuration with automatic dependency detection
- OpenMP enabled by default (every Bayer demosaic is parallel; ~3x on AHD, 4 cores) and
  `CMAKE_BUILD_TYPE` defaulted to `Release`, so a plain `cmake ..` cannot accidentally
  produce an unoptimised, single-threaded library
- Support for **zlib-ng** (preferred, produces `zlib1.dll`)
- Optional dependencies: zlib-ng, libjpeg-turbo, lcms2, jasper - see
  [BINARIES.md](BINARIES.md) section 4 for what each one costs when disabled
- Per-platform scripts (`build_windows.bat`, `build_linux.sh`, `build_macos.sh`) and a
  release workflow that builds and tests on all three platforms

### Fork-only C API

`libraw_undisker_raw_image`, `_raw_geometry`, `_levels`, `_effective_levels`,
`_cam_mul`, `_filters`, `_xtrans` give a C caller direct access to the undemosaiced CFA
buffer, its geometry, the effective black/white levels and the Bayer / X-Trans 6x6
pattern - what the host application's processing pipeline needs and what the stock C API does not
expose.

---

# LibRaw (Original)
## Library for reading and processing of RAW digicam images

 The LibRaw library provides a simple and unified interface for extracting out of RAW files generated by digital photo cameras the following:

* RAW data (_pixel values)_
* Metadata necessary for processing RAW (_geometry, CFA / Bayer pattern, black level, white balance, etc.)_
* Embedded preview / thumbnail.

The library is intended for use with programs that work with RAW files, such as:

* RAW viewers
* RAW converters
* RAW data analyzers
* Software for stitching and compositing multi-frame aggregates like panoramas and image stacks.

Using the LibRaw library allows one to focus on the substantive part of processing the data contained in RAW files, without getting distracted by the wide variety of RAW file and metadata formats, compression algorithms, etc.

The library's development is focused on:

* Support for new cameras and formats
* Improving extraction of metadata necessary for RAW processing
* Providing an interface for reading other types of metadata.

Additionally, the LibRaw library offers some basic RAW conversion, intended for cases when such conversion is not the main function of the LibRaw-using application _(for example: a viewer for 500+ graphic file formats, including RAW)._ These methods are inherited from the Dave Coffin's dcraw.c utility _(see below the "Project history" section);_ their further development is not currently planned, because we do not consider production-quality rendering to be in the scope of LibRaw's functionality _(the methods are retained for compatibility with prior versions and for rapid-fire testing of RAW support and other aspects)._

## Licensing


The LibRaw library is distributed free of charge and with open-source code subject to two licenses:

1\. GNU LESSER GENERAL PUBLIC LICENSE version 2.1

2\. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0

To use the LibRaw library in an application, you can choose the license that better suits your needs.

If you modify/add/improve the LibRaw source code, then your patches can only be included into the library's official source code if you agree to it being distributed under both of the above licenses.

We do not guarantee that the licensing will not change in future versions of LibRaw.

## Update policy


### Major/minor releases

* Major releases _(for example, 0.20)_ are published once every year and a half to three years.
* When the first public beta version of a major release is published, the list of supported cameras and formats is frozen; we try (but do not guarantee) to freeze the API/ABI as well.
* The public major release contains only that code which has been sufficiently tested on a wide user base (including in our commercial products). Something very new may not be included in it (but it will probably be included in a public snapshot, see below).
* Minor updates _(0.20.1, 0.20.2)_ do not change the API/ABI and new cameras are not added; generally they are just bugfixes.
* Bugfixes are published as soon as possible in the [public GitHub repository](https://github.com/LibRaw/LibRaw).
* Minor version increments: these are generally published if a serious error has been fixed, one that potentially affects many library users (for example, a possible stack overflow).

### Public snapshots

* Public snapshots are published every 9-18 months in the [public GitHub repository](https://github.com/LibRaw/LibRaw).
* These versions contain support for new cameras that was added after the previous major release. The API/ABI of public snapshots is not frozen, and may change.
* Public snapshots are always tested on a fairly large user base, and may be considered suitable for use in programs that work with files with known origins (that is, recorded directly by users' digital cameras). However, public snapshots should not be considered sufficiently reliable for processing files that are specially constructed for vulnerability testing; that is, they should not be used in public services that allow for anonymous processing of files of unknown provenance

## Support and feedback

* You can submit a bug report in one of the following ways:
    * [Issues](https://github.com/LibRaw/LibRaw/issues) on GitHub
    * The libraw.org [forum](https://www.libraw.org/forum)
    * [Feedback form](https://www.libraw.org/contact) on libraw.org
    * E-mail us at [info@libraw.org](mailto:info@libraw.org)
* Questions can be asked on the forum, through the feedback form, and via the email indicated above.  
    Please do not use Issues on GitHub for questions; it is intended specifically for problem reports.
* We prefer to process additions to the source code via the [Pull Requests](https://github.com/LibRaw/LibRaw/pulls) on GitHub.

We cannot guarantee any response to requests submitted via the above methods. If you need a guaranteed response for a bug report and/or for extending technical support, please use the Extended Support option.

## Project history


The LibRaw project was launched in 2008, based on the [dcraw.c](https://www.dechifro.org/dcraw/) (Dave Coffin) utility, with its goals being to:

* Convert dcraw.c into a library that could be used by other programs, while also somewhat improving the structure of dcraw.c (removing global variables, making it thread-safe, etc.)
* Import changes made to dcraw.c
* Improve/expand the extraction of metadata.

Unfortunately, support for dcraw.c was reduced to a minimum in 2015, and completely ceased in 2018. Since then, support for new cameras (including support for new formats) has been done entirely by the LibRaw team.

The code that implements rendering of RAW to RGB is inherited from dcraw.c, without major changes (though a set of changes was implemented to speed up and parallelize this code).

In the early-to-mid 2010s, we gladly expanded post-processing implemented in LibRaw, and readily accepted the input of outside developers (new/improved methods for debayerization/demosaicing, systematic noise reduction etc.).

Experience has shown that the LibRaw team cannot support this added code on their own, while contributors do not provide support and development of the code they suggest and it ends up abandoned. As such, the majority of the modifications that are implemented by people outside the LibRaw team was moved to the [LibRaw-demosaic-pack-...](https://github.com/LibRaw?tab=repositories), where they remain in their original form (but these projects are still present in our GitHub).

Thus, we arrive at our current status, described in the first part of this text.

## Copyrights and Acknowledgements

Copyright (C) 2008-2026 LibRaw LLC

LibRaw uses code from Dave Coffin's dcraw.c utility (without RESTRICTED/GPL2 code):

Copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net

LibRaw uses DCB demosaic code by Jaceck Gozdz distributed under BSD license:

Copyright (C) 2010, Jacek Gozdz (mailto:cuniek@kft.umcs.lublin.pl)

LibRaw uses Roland Karlsson's X3F tools source code, licensed under BSD license:

Copyright (c) 2010, Roland Karlsson (roland@proxel.se)
