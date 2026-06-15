# Test Samples & Pipeline Verification

This document explains how to verify that LibRaw opens all supported formats
**consistently and fast**, and which sample files to gather for full
decoder/demosaic coverage.

There are two layers of testing:

1. **File-free CI test** (`PipelineConsistency`) — needs no RAW files, runs in
   CI, and proves the parallel pipeline is correct.
2. **Real-format spot check** (`batch_verify`) — point it at a local folder of
   real RAWs to confirm every decoder opens cleanly and quickly.

---

## 1. File-free CI test (no downloads, no licensing)

`tests/test_pipeline_consistency.cpp` feeds a deterministic synthetic Bayer
frame through the full pipeline via `open_bayer()`, so it requires **no external
files**. For demosaic qualities 0 (bilinear), 3 (AHD) and 11 (DHT) it asserts:

- **determinism** — two decodes are byte-identical;
- **thread-invariance** — single-threaded output == multi-threaded output, i.e.
  the OpenMP postprocessing/demosaic paths contain no output-changing data race.

```sh
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected:

```
[ OK ] q=0  deterministic & thread-invariant  checksum=...
[ OK ] q=3  deterministic & thread-invariant  checksum=...
[ OK ] q=11 deterministic & thread-invariant  checksum=...
100% tests passed
```

---

## 2. Real-format batch verification

Build the samples, then point `batch_verify` at a folder of RAW files:

```sh
cmake -S . -B build -DBUILD_SAMPLES=ON
cmake --build build
build/bin/batch_verify -r /path/to/raw_samples
```

For each file it reports unpack/postprocess timing and Mpix/sec, decodes each
file **twice** and fingerprints the result to confirm consistency, prints a
per-format ok/fail summary, flags slow outliers, and **returns non-zero** if any
file fails to decode or is non-deterministic (so it is CI-usable too).

Options: `-r` recurse into subdirectories, `-t <msec>` custom slow threshold
(default: 3x median postprocess time).

---

## 3. Where to get test files

Use **<https://raw.pixls.us>** — a community RAW database intended for exactly
this. Its samples are **CC0** (public domain), so they are safe to test with and
to redistribute. (Editing-practice packs elsewhere are usually licensed for
personal practice only and are *not* redistributable.)

You do **not** need to commit any RAW files to this repository; keep the corpus
in a local folder and run `batch_verify` against it.

### Minimal complete set — one file per major decoder

| Ext | Example camera | Decoder | Path exercised |
|-----|----------------|---------|----------------|
| `.CR2` | Canon EOS 5D Mk III / 80D | lossless JPEG | classic Bayer |
| `.CR3` | Canon EOS R / RP / M50 | `crx.cpp` | parallel decode |
| `.NEF` | Nikon Z6/Z7 (+ Z8/Z9 for High-Efficiency) | `decoders_dcraw` | compressed + HE codec |
| `.ARW` | Sony A7 III / A7R IV | `sonycc.cpp` | lossless-compressed |
| `.RAF` | Fuji X-T3 / X-T4 | `fuji_compressed.cpp` | parallel decode + X-Trans |
| `.RW2` | Panasonic G9 / S1 | `pana8.cpp` | parallel decode |
| `.ORF` | Olympus E-M1 / OM-1 | `olympus14.cpp` | 12/14-bit |
| `.PEF` | Pentax K-1 / K-3 | `decoders_dcraw` | Bayer |
| `.IIQ` | Phase One IQ / P-series | `load_mfbacks.cpp` | medium format |
| `.DNG` (x3) | uncompressed, lossless-JPEG, and a float/HDR-merge DNG | `dng.cpp` / `fp_dng.cpp` | the 3 DNG sub-paths |
| `.X3F` | Sigma sd Quattro / DP Merrill | X3F tools | Foveon (no Bayer demosaic) |
| `.DCR`/`.KDC` | Kodak DCS Pro / EasyShare | `kodak_decoders.cpp` | Kodak |

### Branch coverage (a normal 3-color Bayer file misses these)

| Need | Example | Why |
|------|---------|-----|
| 4-color sensor | Sony DSC-F828 (`.SR2`, RGBE) | `colors == 4` branch of `convert_to_rgb_loop` |
| Monochrome | Leica M Monochrom (`.DNG`) | `colors == 1` |
| Pixel-shift / multi-shot | Pentax K-1, Sony `.ARQ`, Olympus high-res | `shot_select` |
| One large file (>=45 MP) | any high-MP CR3/RAF/RW2 above | makes the parallel speedup measurable |

### Rarer decoders (optional, grab if present)

`.SRW` Samsung · `.3FR`/`.FFF` Hasselblad · `.MRW` Minolta · `.ERF` Epson ·
`.MOS` Leaf · `.MEF` Mamiya · `.GPR` GoPro · `.CRW` old Canon (CIFF) ·
`.RWL` Leica.

---

## 4. Spot-checking demosaic paths

The demosaic quality is independent of format, so vary `-q` on a single Bayer
file to exercise each interpolator:

```sh
build/bin/postprocessing_benchmark -q 0  file.CR2   # bilinear
build/bin/postprocessing_benchmark -q 3  file.CR2   # AHD  (parallel)
build/bin/postprocessing_benchmark -q 11 file.CR2   # DHT  (parallel)
build/bin/postprocessing_benchmark -q 12 file.CR2   # AAHD (serial)
```

---

## 5. Measuring the OpenMP speedup

Build twice and compare. Output is bit-identical between the two; only the speed
differs:

```sh
cmake -S . -B build_omp   -DBUILD_SAMPLES=ON                          # OpenMP on (default)
cmake -S . -B build_noomp -DBUILD_SAMPLES=ON -DLIBRAW_ENABLE_OPENMP=OFF
cmake --build build_omp   && cmake --build build_noomp
build_noomp/bin/batch_verify file.CR2
build_omp/bin/batch_verify   file.CR2
```
