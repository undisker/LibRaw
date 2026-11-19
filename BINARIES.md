# Shipping notes: `raw.dll` / `libraw.dylib` / `libraw.so`

The host application does not link LibRaw statically and does not use a distribution's
LibRaw package. It ships a shared library **built from this fork**, loaded at
run time by the host application's loader.

This file is the authoritative description of how that binary is produced, what
the host application requires from it, and which build switches change what the product
can actually open. Change the build recipe here and in
`.github/workflows/release.yml` together — never in only one of them.

---

## 1. Why we build our own

**Security.** The host application opens files of unknown provenance; upstream LibRaw's
own update policy says public snapshots "should not be considered sufficiently
reliable for processing files that are specially constructed for vulnerability
testing". Building from this fork is what puts the fork's checked-arithmetic
layer (`internal/libraw_safe_math.h` and the call sites listed in `README.md`)
into the shipped code. A distro package or a stock upstream build has none of
it, and a static link would bury the decoder inside the application executable, where a
LibRaw CVE could only be fixed by re-shipping the whole application. As a
separate DLL, a decoder fix is a one-file patch.

**Performance.** LibRaw's decoders, demosaic and postprocessing are parallelised
with OpenMP. The pragmas are compiled out unless the build enables OpenMP, and
stock builds frequently do not; this fork additionally parallelises the
demosaics upstream leaves single-threaded (linear, VNG, DCB, AAHD) and trims the
AHD inner loops. Measured on this fork (6000×4000 synthetic Bayer frame, 4-core
x86-64 Linux, `-O2`, full `open_bayer` → `unpack` → `dcraw_process` →
`dcraw_make_mem_image`, 8-bit output, best of several runs on a shared VM - read
the ratios, not the absolute numbers):

| Demosaic quality | Aug-2026 build, 4 threads | this build, 4 threads | change |
|---|---|---|---|
| 0 — linear   | 430 ms  | 221 ms  | 1.9× |
| 1 — VNG      | 2691 ms | 820 ms  | 3.3× |
| 2 — PPG      | 250 ms  | 204 ms  | 1.2× |
| 3 — AHD (default) | 560 ms | 475 ms | 1.2× |
| 4 — DCB      | 3281 ms | 1319 ms | 2.5× |
| 11 — DHT     | 1172 ms | 1131 ms | — |
| 12 — AAHD    | 4704 ms | 1660 ms | 2.8× |
| 3, half-size | 89 ms   | 65 ms   | 1.4× |

AHD single-threaded went from 1717 ms to 1547 ms on the same frame, and scales
about 3× from one thread to four.

All of this is **bit-identical** to the previous build: the output hash of every
row above is unchanged, and a sweep over frame sizes from 22×22 up, four Bayer
phases, all seven qualities and 1–16 threads against the old single-threaded
result showed no difference. The rule that makes that possible, and that any
further parallelisation has to follow: a loop is only shared out between threads
when no iteration reads what another iteration of the same loop writes. Passes
that update the image in place and read already-updated neighbours
(`dcb_nyquist`, `dcb_pp`, `dcb_refinement`, the tail of `dcb_color_full`,
`fbdd_*`, AAHD's `hide_hots` and `refine_ihv_dirs`) depend on the scan order and
stay sequential on purpose.

The same comparison was repeated on twelve real files from raw.pixls.us (Canon
CR2 and CR3, Nikon NEF, Fuji X-Trans RAF, Panasonic 80 MP RW2, Olympus ORF,
Pentax PEF and DNG, four Sony ARW variants), 16-bit output, every quality, 3 and
4 threads: all 84 full-size results are bit-identical to the build before the
optimisations, with `dcraw_process` 2.3–2.9× faster for linear, VNG, DCB and
AAHD and 1.1–1.2× for PPG, AHD and DHT.

One result differs, and on purpose: **half-size X-Trans**. The old build is not
deterministic there - `copy_bayer` ran `schedule(dynamic)` with single-row
chunks, and with `half_size` two raw rows that store the same colour into the
same output cell could land in different threads, so the output changed from run
to run (upstream still does this). `copy_bayer` now uses an even chunk size,
which keeps such a row pair in one thread; the output is stable and equals the
single-threaded one. Keep the chunk size even.

**Sony lossless-compressed ARW.** `sony_ljpeg_load_raw` decodes its 512×512
lossless-JPEG tiles in parallel from memory with a table-driven Huffman decoder
(upstream PR #886, not merged upstream at the time of writing; carried here as a
cherry-pick). `unpack()` on the same machine:

| File | before | after, 4 threads | after, 1 thread |
|---|---|---|---|
| A7R V lossless L, 9728×6656 | 1694 ms | 156 ms | — |
| A1 lossless, 8704×6144      | 1542 ms | 155 ms | 556 ms |
| A7 IV lossless L, 7168×5120 | 940 ms  | 81 ms  | — |
| A7 IV APS-C lossless M      | 454 ms  | 43 ms  | 156 ms |

The raw buffer is bit-identical to the old decoder's on all six lossless-L /
crop-mode samples tried, at 1, 2, 4 and 7 threads. The new path only accepts
streams the old decoder reads without any warning and otherwise falls back to
it; seven deliberately damaged copies (truncated, bit-flipped, zeroed and
0xFF-filled blocks) gave the same output, the same error codes and the same
"data corrupted" messages as before. When upstream merges or reworks the PR,
expect a conflict in `src/decoders/decoders_libraw_dcrdefs.cpp` and take
upstream's version.

`dcraw_process` is on the host application's interactive path whenever the embedded
preview is unusable (`rdmFullOnly`, or `rdmAuto` on a file whose preview is
narrower than `RawMinPreviewWidth`), so this is user-visible latency, not a
batch-only concern.

Two build defaults exist purely to protect that: `CMakeLists.txt` forces
`CMAKE_BUILD_TYPE=Release` when the caller leaves it empty (single-config
generators otherwise compile at `-O0`), and `LIBRAW_ENABLE_OPENMP` defaults to
`ON`. Do not ship a binary built without checking both.

---

## 2. The ABI contract with the host application

The host application's loader resolves these by name with `GetProcedureAddress` and
degrades gracefully if any are missing — which means a mismatched build shows
up as "RAW files do not open" rather than a crash. All are `cdecl`.

Required (RAW decoding is disabled entirely without them):

    libraw_init  libraw_open_buffer  libraw_unpack  libraw_dcraw_process
    libraw_dcraw_make_mem_image  libraw_dcraw_clear_mem  libraw_close

Optional (each missing one silently disables one feature):

    libraw_unpack_thumb            embedded-preview fast path
    libraw_dcraw_make_mem_thumb    embedded-preview fast path
    libraw_set_demosaic            quality selection
    libraw_set_output_bps          16-bit output

Also always exported, and useful for diagnostics rather than decoding:

    libraw_version         "0.23.0-patched-undisker" for our build
    libraw_versionNumber
    libraw_capabilities    bitmask of what this DLL was compiled with

Fork-only, used by the Develop/processing code for direct sensor access:

    libraw_undisker_raw_image      libraw_undisker_raw_geometry
    libraw_undisker_levels         libraw_undisker_effective_levels
    libraw_undisker_cam_mul        libraw_undisker_filters
    libraw_undisker_xtrans

`TLibRawProcessedImage` in the Pascal unit is a `packed record` mirroring
`libraw_processed_image_t` — `Integer, Word, Word, Word, Word, LongWord`
followed by the pixel data. Any change to that struct in `libraw/libraw_types.h`
is a breaking ABI change for the host application and must be made in both repositories in
the same coordinated branch.

Naming is fixed by the Pascal constants and must not be changed by the build:

| Platform | File the host application loads | Produced by |
|---|---|---|
| Windows | `raw.dll`     | `OUTPUT_NAME "raw"` → `raw.dll` |
| macOS   | `libraw.dylib`| `OUTPUT_NAME "raw"` → `libraw.<ver>.dylib` + symlinks |
| Linux   | `libraw.so`   | `OUTPUT_NAME "raw"` → `libraw.so.<ver>` + symlinks |

On Windows only `DllDef`-marked entry points are exported. On ELF/Mach-O the
default visibility exports the whole C++ surface too (723 dynamic symbols in a
current Linux build); that is cosmetic, not a defect — do **not** add
`-fvisibility=hidden`, because LibRaw's `DllDef` expands to nothing outside
Windows and hiding symbols would strip the C API the host application needs.

---

## 3. Canonical build

The three `build_*.sh` / `build_windows.bat` scripts in this repository build
the dependencies from source into `dependencies/` first and are the recipe for a
**full-feature** binary. The CI in `.github/workflows/release.yml` builds a
**dependency-free** binary. Section 4 explains why that difference matters and
which one belongs in a release.

### Linux

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DLIBRAW_ENABLE_OPENMP=ON \
  -DLIBRAW_ENABLE_ZLIB=ON \
  -DLIBRAW_ENABLE_JPEG=ON \
  -DLIBRAW_ENABLE_LCMS2=ON \
  -DLIBRAW_ENABLE_X3FTOOLS=ON \
  -DBUILD_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Ship the **real file**, not the linker symlink: `build/bin/libraw.so.0.21.3`
copied to the payload as `libraw.so`. Copying the symlink itself produces a
dangling link in the installer. Set `RPATH` to `$ORIGIN` (`patchelf --set-rpath
'$ORIGIN' libraw.so`) so the library finds its own siblings next to the
executable instead of the system ones.

### macOS

Same flags plus `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` and
`MACOSX_DEPLOYMENT_TARGET=10.15`. Ship the resolved `libraw.<ver>.dylib` as
`libraw.dylib`, then `install_name_tool -id @rpath/libraw.dylib` on it and
verify every `otool -L` entry resolves inside the bundle.

Note that `CMakeLists.txt` defines `LIBRAW_NOTHREADS` on Apple, so the macOS
binary is the non-reentrant variant; the host application uses one `LibRaw` handle per
decode from a single thread at a time, which is what that variant supports.

### Windows

`build_windows.bat` (MSVC x64, Visual Studio 2022) builds zlib, libjpeg-turbo,
LCMS2 and JasPer into `dependencies/bin/`, then LibRaw against them, and copies
out `raw.dll`.

**OpenMP on Windows works with MSVC's default `/openmp`** and is enabled in
`release.yml` like every other platform. MSVC's `/openmp` is OpenMP 2.0, but
the codebase does not need more than that:

- `subtract_black.cpp` accumulates its maximum per thread and merges it in a
  `critical` section rather than using `reduction(max:)` (OpenMP 3.1). All
  three of those loops also perform the black subtraction itself, so this is
  what keeps the whole pass parallel on MSVC rather than only the reduction.
- `collapse(2)` in `dht_demosaic.cpp` is guarded upstream behind `_MSC_VER`.
  The fallback costs nothing — the outer loop runs over image height, which is
  thousands of iterations.
- Every omp-for loop variable is a signed `int`, as OpenMP 2.0 requires.

So do not reintroduce `-DLIBRAW_ENABLE_OPENMP=OFF`, and if you add a pragma
using an OpenMP 3.0+ feature, either guard it or express it in 2.0 terms.
`/openmp:llvm` (VS2019 16.9+, or `-DOpenMP_RUNTIME_MSVC=llvm` on CMake 3.30+)
is available if a future construct genuinely needs 3.1, but it ships
`libomp140.x86_64.dll` instead of the stock runtime, so prefer staying on 2.0.

Confirm the result rather than assuming it: `test_pipeline_consistency` prints
the thread count it ran with, and exists specifically to prove the parallel
paths stay bit-identical to the serial ones — including a dedicated section for
`subtract_black_internal`'s three branches.

The DLL imports the Visual C++ runtime (`vcruntime140.dll`,
`vcruntime140_1.dll`, `msvcp140.dll`) and, with OpenMP on, the OpenMP runtime
`vcomp140.dll`. All four ship in the VC++ redistributable and must be in the
payload; `installer/stage.py` in the host application's repository lists them in
`WINDOWS_VCRUNTIME` and fails the build if any is missing, because without them
`raw.dll` fails to load and RAW support vanishes with no error message.

---

## 4. Optional dependencies — what each one actually buys

Every `LIBRAW_ENABLE_*` switch below is a *format* decision, not an
optimisation. Turning one off does not make LibRaw smaller in any way the user
benefits from; it removes files the host application can open.

| Switch | Off means | Affects |
|---|---|---|
| `LIBRAW_ENABLE_ZLIB` | `deflate_dng_load_raw()` throws `LIBRAW_EXCEPTION_DECODE_RAW` | **DNG with Deflate/ZIP compression (TIFF compression 8)** — floating-point/HDR DNGs from Lightroom & ACR, and 8/16-bit integer ZIP DNGs. File opens, then fails to unpack. |
| `LIBRAW_ENABLE_JPEG` | `NO_JPEG` is defined; `identify()` sets `is_raw = 0` and raises `LIBRAW_WARN_NO_JPEGLIB` | **Lossy DNG (compression 34892)** and **Kodak JPEG RAW**. The file is rejected outright as "not a RAW file". |
| `LIBRAW_ENABLE_LCMS2` | no ICC output-profile support | Nothing the host application uses — it does its own colour management via `lcms2.dll`. |
| `LIBRAW_ENABLE_JASPER` | no JPEG-2000 in DNG | Rare. |
| `LIBRAW_ENABLE_X3FTOOLS` | Sigma `.x3f` unsupported | `.x3f` is in the host application's `RAW_EXTS`, so keep this **ON**. |
| `LIBRAW_ENABLE_6BY9RPI` | Raspberry Pi camera `.raw` (JPEG with the sensor dump appended) unsupported | **ON** by default since November 2026: no dependency, and all five Raspberry Pi samples in the raw.pixls.us corpus need it. |

Two things that are **not** affected by `LIBRAW_ENABLE_JPEG`, and are worth
knowing before someone "optimises" it away in the other direction:

- **Lossless-JPEG DNG (compression 7)** — by far the most common DNG flavour,
  including Adobe DNG Converter output and Apple ProRAW — uses LibRaw's own
  decoder in `src/decompressors/losslessjpeg.cpp`. No libjpeg involved.
- **Embedded previews.** `libraw_unpack_thumb` hands back the camera's JPEG
  bytes and the host application decodes them itself (`RawJpegDecoder`), so the
  preview fast path works in a `NO_JPEG` build. Only `T.tcolors` is guessed
  rather than parsed.

### How the release gets zlib and libjpeg-turbo

`cmake/build-deps.cmake` builds both from pinned sources into one prefix, with
the same single command on all three platforms:

```sh
cmake -DPREFIX="$PWD/deps" -DJOBS=4 [-DOSX_ARCHS="arm64;x86_64"] -P cmake/build-deps.cmake
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/deps" \
      -DLIBRAW_ENABLE_ZLIB=ON -DLIBRAW_ENABLE_JPEG=ON
```

Not vcpkg / Homebrew / apt, for three reasons found by trying them:

- **macOS.** The release is a universal binary. A Homebrew libjpeg-turbo is
  built for the host architecture only and cannot satisfy both slices. Built
  through the script it inherits `OSX_ARCHS` and comes out universal too.
- **Linux.** Distro *static* archives are not compiled `-fPIC`, so they cannot
  be linked into a shared library at all — `relocation R_X86_64_PC32 against
  'z_errmsg' can not be used when making a shared object`. Anything built here
  is position-independent.
- **One code path**, with versions pinned rather than whatever the runner image
  carries. To bump either, change the tag in `cmake/build-deps.cmake`.

Both are built **shared**, deliberately. Statically linking them saves nothing
worth having — LibRaw's own code grows only ~9.5 KB when both are enabled, so
the dependency *is* the size wherever it goes — and it costs a lot: a CVE in
either becomes a LibRaw rebuild instead of a one-file replacement, and the
application cannot share one copy with its other consumers (the host application
already ships `zlib1.dll` for libtiff and its own compression code).

zlib-ng is used in `ZLIB_COMPAT` mode: a drop-in `libz.so.1` / `zlib1.dll` with
the standard zlib ABI, faster at inflate — which *is* the deflate-DNG path.
libjpeg-turbo is built with `WITH_TURBOJPEG=OFF`, because LibRaw uses only the
libjpeg API and leaving the TurboJPEG API out avoids building and shipping a
second copy of the codec. The result is `libjpeg.so.62` / `jpeg62.dll` — note
that this is **not** the `turbojpeg.dll` the host application already ships, which
exports the `tj*` API and no `jpeg_*` symbols at all, so it cannot serve
LibRaw.

Licensing is unaffected by the link mode: zlib's licence asks for nothing in a
binary distribution, and libjpeg-turbo's (IJG + BSD-3-Clause + zlib) requires
the same acknowledgement whether it is linked statically or dynamically. Both
are noted in the host application's `installer/ATTRIBUTIONS.txt`.

---

## 5. Security posture of a shipped build

- Build from a **tagged commit of this fork**, never from a local working tree.
  `LIBRAW_VERSION_TAIL` is `patched-undisker`; `libraw_version()` in a shipped
  DLL must report it, which is the one cheap way to tell our binary from a
  system LibRaw that happened to get loaded first.
- Keep `tests/test_security_fixes.cpp` and `tests/test_pipeline_consistency.cpp`
  in the release job (`-DBUILD_TESTS=ON` + `ctest`). They are the regression
  net for the fork's checked arithmetic and for the OpenMP parallelisation
  respectively; a release that skips them is unverified.
- Re-run the upstream sync before each release (section 6). Most upstream
  commits between snapshots are decoder hardening, and shipping a stale decoder
  is the main avoidable risk in this component.
- **Memory ceiling.** `imgdata.rawparams.max_raw_memory_mb` defaults to 2048 MB
  (`LIBRAW_MAX_ALLOC_MB_DEFAULT`) and thumbnails to 512 MB
  (`LIBRAW_MAX_THUMBNAIL_MB`). Both are compile-time defaults; the C API exposes
  no setter, so the host application cannot currently lower them for untrusted input. If a
  tighter ceiling is wanted, either add a `libraw_set_max_raw_memory_mb` export
  to `src/libraw_c_api.cpp` (and to the Pascal loader as an optional symbol) or
  override `LIBRAW_MAX_ALLOC_MB_DEFAULT` at build time. Left as-is for now —
  recorded here so the choice is deliberate.
- Do not enable RawSpeed, the Adobe DNG SDK or the GoPro GPR SDK. They pull in
  large third-party codebases with their own CVE histories (the DNG SDK has had
  roughly monthly security releases since late 2025) and, for the DNG SDK, an
  uncapped indemnity to Adobe on commercial distribution. Nothing the host application
  opens needs them.
- GoPro `.GPR` (VC-5) is decoded by **GoPro's own open-source decoder**,
  compiled in from a pinned commit of `gopro/gpr` with a bounds-checking patch
  (`GoPro/gpr-vc5-hardening.patch`)
  (`LIBRAW_ENABLE_GPR_SDK`, ON; `libraw_capabilities()` bit 22). Only its
  `vc5_decoder`, `vc5_common` and `common` directories are used - not the
  Adobe DNG SDK copy in the same repository. GoPro holds patents on this kind
  of Bayer compression, the last running until April 2030, and its own code
  under its own Apache-2.0 licence is the conservative way to be covered.
  Unpatched, GoPro's decoder wrote past its buffers on damaged files; with the
  patch, fuzzing found nothing (`LICENSING.md` section 3 has the numbers). It
  is single-threaded and 5-8 times slower than the fork's decoder.
- This fork's own decoder, `src/decoders/vc5.cpp` (`LIBRAW_ENABLE_GPR`, OFF;
  bit 21), is bit-identical, faster and hardened, but **requires legal
  clarification** before it is shipped.
- DNG with JPEG XL compression (TIFF compression 52546 - Samsung Expert RAW,
  recent Adobe DNG Converter output) is decoded through libjxl
  (`LIBRAW_ENABLE_JXL`, ON; `libraw_capabilities()` bit 20). The loader calls
  libjxl directly, not the DNG SDK. Limits: integer samples only, and a file
  that combines Row/ColumnInterleaveFactor with more than one tile is refused -
  no such file was available to establish how the two interact.
- Licences, provenance and the attribution list a release needs are in
  `LICENSING.md`.

---

## 6. Keeping in sync with upstream

```sh
git remote add upstream https://github.com/LibRaw/LibRaw.git   # once
git fetch upstream master
git log --oneline HEAD..upstream/master        # what is new
git merge upstream/master
```

Conflicts land almost exclusively where the fork replaced raw arithmetic with
checked arithmetic. The rule is: **keep the fork's checked version, then apply
whatever upstream added on top of it** (a new limit, a wider integer type).
Never resolve by taking upstream's side wholesale — that silently drops a fork
patch. The three conflicts in the June-2026 sync are written up in that merge
commit as worked examples, including one case where upstream's version is
outright wrong for us (it dropped the `size = iheight * iwidth` assignment in
`wavelet_denoise`, leaving `size` uninitialised in the OpenMP variant this fork
builds).

The November-2026 sync added a fourth kind: `src/decoders/crx.cpp` conflicted
over its whole length because the fork stores it with LF line endings and
upstream with CRLF. Do not resolve that by hand - strip the `\r` from the base
and upstream stages and run `git merge-file` on the three normalised files; the
real differences then merge cleanly.

Upstream's struct layouts are not frozen between snapshots (that sync moved
offsets in `libraw_colordata_t` and `libraw_raw_unpack_params_t`). The host application
is insulated because it goes through functions and `libraw_processed_image_t`
only; keep it that way, and add an accessor to `src/libraw_c_api.cpp` rather
than mirroring another struct in Pascal.

After merging:

1. Add any new `src/**/*.cpp` to `LIBRAW_SOURCES` in `CMakeLists.txt`. Upstream
   maintains the autotools/MSVC makefiles but not our CMake list, so a new
   decoder will otherwise link out silently. (`src/decoders/sony_arw6.cpp` and
   `src/decoders/nikonhe.cpp` were added this way.)
2. Rebuild and run `ctest`.
3. Check the host application's symbols are all still exported:
   `nm -D --defined-only build/bin/libraw.so | grep libraw_` — or `dumpbin
   /exports raw.dll` on Windows.

---

## 7. Pre-ship checklist

- [ ] Built from a tagged commit of this fork, `CMAKE_BUILD_TYPE=Release`.
- [ ] `ctest` green (`SecurityFixes`, `PipelineConsistency`).
- [ ] OpenMP on for every platform (`test_pipeline_consistency` prints the
      thread count it used); Windows payload carries `vcomp140.dll`.
- [ ] zlib and JPEG on (`libraw_capabilities()` returns bits 6 and 7); the
      dependency runtime is packaged beside the library.
- [ ] Every symbol in section 2 present in the built binary.
- [ ] Correct file name (`raw.dll` / `libraw.dylib` / `libraw.so`), real file
      rather than a symlink, `RPATH=$ORIGIN` / `@rpath` applied.
- [ ] Windows: VC++ runtime DLLs staged alongside.
- [ ] `libraw_version()` reports `…-patched-undisker`.
- [ ] `installer/ATTRIBUTIONS.txt` in the host application lists LibRaw and every
      dependency actually linked into this build.
