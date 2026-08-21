# Shipping notes: `raw.dll` / `libraw.dylib` / `libraw.so`

Panvyo Viewer does not link LibRaw statically and does not use a distribution's
LibRaw package. It ships a shared library **built from this fork**, loaded at
run time by `src/uViewerLibRaw.pas` in the Viewer repository.

This file is the authoritative description of how that binary is produced, what
the Viewer requires from it, and which build switches change what the product
can actually open. Change the build recipe here and in
`.github/workflows/release.yml` together — never in only one of them.

---

## 1. Why we build our own

**Security.** The Viewer opens files of unknown provenance; upstream LibRaw's
own update policy says public snapshots "should not be considered sufficiently
reliable for processing files that are specially constructed for vulnerability
testing". Building from this fork is what puts the fork's checked-arithmetic
layer (`internal/libraw_safe_math.h` and the call sites listed in `README.md`)
into the shipped code. A distro package or a stock upstream build has none of
it, and a static link would bury the decoder inside `Viewer.exe`, where a
LibRaw CVE could only be fixed by re-shipping the whole application. As a
separate DLL, a decoder fix is a one-file patch.

**Performance.** LibRaw carries 61 `#pragma omp` parallel loops across the
decoders, demosaic and postprocessing. They are compiled out unless the build
enables OpenMP, and stock builds frequently do not. Measured on this fork
(4000×3000 synthetic Bayer frame, 4-core x86-64 Linux, `-O2`, full
`open_bayer` → `unpack` → `dcraw_process` → `dcraw_make_mem_image`):

| Demosaic quality | 1 thread | 4 threads | speed-up |
|---|---|---|---|
| 0 — linear   | 0.490 s | 0.312 s | 1.6× |
| 2 — PPG      | 0.506 s | 0.200 s | 2.5× |
| 3 — AHD      | 1.136 s | 0.396 s | 2.9× |

`dcraw_process` is on the Viewer's interactive path whenever the embedded
preview is unusable (`rdmFullOnly`, or `rdmAuto` on a file whose preview is
narrower than `RawMinPreviewWidth`), so this is user-visible latency, not a
batch-only concern.

Two build defaults exist purely to protect that: `CMakeLists.txt` forces
`CMAKE_BUILD_TYPE=Release` when the caller leaves it empty (single-config
generators otherwise compile at `-O0`), and `LIBRAW_ENABLE_OPENMP` defaults to
`ON`. Do not ship a binary built without checking both.

---

## 2. The ABI contract with the Viewer

`uViewerLibRaw.pas` resolves these by name with `GetProcedureAddress` and
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

    libraw_version         "0.22.0-patched-undisker" for our build
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
is a breaking ABI change for the Viewer and must be made in both repositories in
the same coordinated branch.

Naming is fixed by the Pascal constants and must not be changed by the build:

| Platform | File the Viewer loads | Produced by |
|---|---|---|
| Windows | `raw.dll`     | `OUTPUT_NAME "raw"` → `raw.dll` |
| macOS   | `libraw.dylib`| `OUTPUT_NAME "raw"` → `libraw.<ver>.dylib` + symlinks |
| Linux   | `libraw.so`   | `OUTPUT_NAME "raw"` → `libraw.so.<ver>` + symlinks |

On Windows only `DllDef`-marked entry points are exported. On ELF/Mach-O the
default visibility exports the whole C++ surface too (723 dynamic symbols in a
current Linux build); that is cosmetic, not a defect — do **not** add
`-fvisibility=hidden`, because LibRaw's `DllDef` expands to nothing outside
Windows and hiding symbols would strip the C API the Viewer needs.

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
binary is the non-reentrant variant; the Viewer uses one `LibRaw` handle per
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
payload; `installer/stage.py` in the Viewer repo lists them in
`WINDOWS_VCRUNTIME` and fails the build if any is missing, because without them
`raw.dll` fails to load and RAW support vanishes with no error message.

---

## 4. Optional dependencies — what each one actually buys

Every `LIBRAW_ENABLE_*` switch below is a *format* decision, not an
optimisation. Turning one off does not make LibRaw smaller in any way the user
benefits from; it removes files the Viewer can open.

| Switch | Off means | Affects |
|---|---|---|
| `LIBRAW_ENABLE_ZLIB` | `deflate_dng_load_raw()` throws `LIBRAW_EXCEPTION_DECODE_RAW` | **DNG with Deflate/ZIP compression (TIFF compression 8)** — floating-point/HDR DNGs from Lightroom & ACR, and 8/16-bit integer ZIP DNGs. File opens, then fails to unpack. |
| `LIBRAW_ENABLE_JPEG` | `NO_JPEG` is defined; `identify()` sets `is_raw = 0` and raises `LIBRAW_WARN_NO_JPEGLIB` | **Lossy DNG (compression 34892)** and **Kodak JPEG RAW**. The file is rejected outright as "not a RAW file". |
| `LIBRAW_ENABLE_LCMS2` | no ICC output-profile support | Nothing the Viewer uses — it does its own colour management via `lcms2.dll`. |
| `LIBRAW_ENABLE_JASPER` | no JPEG-2000 in DNG | Rare. |
| `LIBRAW_ENABLE_X3FTOOLS` | Sigma `.x3f` unsupported | `.x3f` is in the Viewer's `RAW_EXTS`, so keep this **ON**. |
| `LIBRAW_ENABLE_6BY9RPI` | Raspberry Pi camera format unsupported | Not a target; leave OFF. |

Two things that are **not** affected by `LIBRAW_ENABLE_JPEG`, and are worth
knowing before someone "optimises" it away in the other direction:

- **Lossless-JPEG DNG (compression 7)** — by far the most common DNG flavour,
  including Adobe DNG Converter output and Apple ProRAW — uses LibRaw's own
  decoder in `src/decompressors/losslessjpeg.cpp`. No libjpeg involved.
- **Embedded previews.** `libraw_unpack_thumb` hands back the camera's JPEG
  bytes and the Viewer decodes them itself (`RawJpegDecoder`), so the
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
application cannot share one copy with its other consumers (Panvyo Viewer
already ships `zlib1.dll` for libtiff and its own compression code).

zlib-ng is used in `ZLIB_COMPAT` mode: a drop-in `libz.so.1` / `zlib1.dll` with
the standard zlib ABI, faster at inflate — which *is* the deflate-DNG path.
libjpeg-turbo is built with `WITH_TURBOJPEG=OFF`, because LibRaw uses only the
libjpeg API and leaving the TurboJPEG API out avoids building and shipping a
second copy of the codec. The result is `libjpeg.so.62` / `jpeg62.dll` — note
that this is **not** the `turbojpeg.dll` the Viewer already ships, which
exports the `tj*` API and no `jpeg_*` symbols at all, so it cannot serve
LibRaw.

Licensing is unaffected by the link mode: zlib's licence asks for nothing in a
binary distribution, and libjpeg-turbo's (IJG + BSD-3-Clause + zlib) requires
the same acknowledgement whether it is linked statically or dynamically. Both
are noted in the Viewer's `installer/ATTRIBUTIONS.txt`.

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
  no setter, so the Viewer cannot currently lower them for untrusted input. If a
  tighter ceiling is wanted, either add a `libraw_set_max_raw_memory_mb` export
  to `src/libraw_c_api.cpp` (and to the Pascal loader as an optional symbol) or
  override `LIBRAW_MAX_ALLOC_MB_DEFAULT` at build time. Left as-is for now —
  recorded here so the choice is deliberate.
- Do not enable RawSpeed or the DNG SDK. Both pull in large third-party
  codebases with their own CVE histories and neither is needed for anything the
  Viewer opens.

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

After merging:

1. Add any new `src/**/*.cpp` to `LIBRAW_SOURCES` in `CMakeLists.txt`. Upstream
   maintains the autotools/MSVC makefiles but not our CMake list, so a new
   decoder will otherwise link out silently. (The June-2026 sync added
   `src/decoders/sony_arw6.cpp` this way.)
2. Rebuild and run `ctest`.
3. Check the Viewer's symbols are all still exported:
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
- [ ] `installer/ATTRIBUTIONS.txt` in the Viewer lists LibRaw and every
      dependency actually linked into this build.
