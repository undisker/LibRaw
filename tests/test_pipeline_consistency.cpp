/* -*- C++ -*-
 * tests/test_pipeline_consistency.cpp
 *
 * File-free regression test for the postprocessing/demosaic pipeline.
 *
 * Feeds a deterministic synthetic Bayer frame through the full LibRaw
 * pipeline (subtract_black -> scale_colors -> demosaic -> convert_to_rgb)
 * using open_bayer(), so it needs NO external RAW files and is safe to run
 * in CI.
 *
 * It verifies two properties that the OpenMP parallelization must preserve:
 *   1. Determinism      - decoding the same frame twice is byte-identical.
 *   2. Thread-invariance - decoding with 1 thread and with N threads produces
 *                          byte-identical output, i.e. the parallel paths
 *                          (thread-local histogram merge, per-thread max merge,
 *                          per-pixel/per-row writes) contain no data race that
 *                          changes the result.
 *
 * Part 2 covers subtract_black_internal() on its own. dcraw_process() reaches
 * it only when inline subtraction is off (see `subtract_inline` there), which
 * a plain open_bayer() frame never triggers - so the full-pipeline pass above
 * cannot exercise it, and calling raw2image() + subtract_black() directly is
 * the only way to reach all three of its branches:
 *   BLACK_NONE  - cblack all zero            -> plain channel-maximum scan
 *   BLACK_FLAT  - cblack[0..3] set           -> per-channel subtraction
 *   BLACK_TABLE - cblack[4],[5],[6..] set    -> per-pixel cblack table
 * Each branch accumulates its own maximum across threads, so data_maximum is
 * compared as well as the pixel data.
 *
 * Exit code 0 = pass, non-zero = a regression.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libraw/libraw.h"

#ifdef _OPENMP
#include <omp.h>
#endif

static unsigned long long fnv1a(const unsigned char *p, size_t n)
{
  unsigned long long h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; i++)
  {
    h ^= p[i];
    h *= 1099511628211ULL;
  }
  return h;
}

// Decode the synthetic frame once at quality q; return FNV-1a of the 16-bit
// processed image, or 0 on pipeline error (with *ok cleared).
static unsigned long long decode(const ushort *bayer, int W, int H, int q,
                                 int *ok)
{
  LibRaw R;
  R.imgdata.params.user_qual = q;
  R.imgdata.params.no_auto_bright = 1; // keep output independent of histogram
  R.imgdata.params.output_bps = 16;

  size_t bytes = (size_t)W * H * sizeof(ushort);
  int ret = R.open_bayer((unsigned char *)bayer, (unsigned)bytes, W, H, 0, 0, 0,
                         0, 0, /*RGGB*/ 0x94, 0, 0, 0);
  if (ret == LIBRAW_SUCCESS)
    ret = R.unpack();
  if (ret == LIBRAW_SUCCESS)
    ret = R.dcraw_process();
  if (ret != LIBRAW_SUCCESS)
  {
    fprintf(stderr, "  pipeline error (q=%d): %s\n", q, libraw_strerror(ret));
    *ok = 0;
    return 0;
  }
  libraw_processed_image_t *img = R.dcraw_make_mem_image(&ret);
  if (!img)
  {
    fprintf(stderr, "  make_mem_image error (q=%d): %s\n", q,
            libraw_strerror(ret));
    *ok = 0;
    return 0;
  }
  unsigned long long c = fnv1a(img->data, img->data_size);
  R.dcraw_clear_mem(img);
  *ok = 1;
  return c;
}

// Which branch of subtract_black_internal() to exercise.
enum BlackMode
{
  BLACK_NONE = 0,  // cblack all zero    -> plain channel-maximum scan
  BLACK_FLAT = 1,  // cblack[0..3]       -> per-channel subtraction
  BLACK_TABLE = 2, // cblack[4],[5],[6+] -> per-pixel cblack table
  BLACK_MODES = 3
};

static const char *black_name(int m)
{
  return m == BLACK_NONE ? "none" : (m == BLACK_FLAT ? "flat" : "table");
}

// Run open_bayer -> unpack -> raw2image -> subtract_black with the requested
// black-level configuration. Returns 1 on success, filling *sum with an FNV-1a
// of the whole image buffer and *dmax with color.data_maximum (the value the
// per-thread max merge produces).
static int black_scan(const ushort *bayer, int W, int H, int mode,
                      unsigned long long *sum, unsigned *dmax)
{
  LibRaw R;
  size_t bytes = (size_t)W * H * sizeof(ushort);
  int ret = R.open_bayer((unsigned char *)bayer, (unsigned)bytes, W, H, 0, 0, 0,
                         0, 0, /*RGGB*/ 0x94, 0, 0, 0);
  if (ret == LIBRAW_SUCCESS)
    ret = R.unpack();
  if (ret == LIBRAW_SUCCESS)
    ret = R.raw2image(); // copies to imgdata.image WITHOUT subtracting black
  if (ret != LIBRAW_SUCCESS)
  {
    fprintf(stderr, "  black_scan setup error (%s): %s\n", black_name(mode),
            libraw_strerror(ret));
    return 0;
  }

  if (mode == BLACK_FLAT)
  {
    R.imgdata.color.cblack[0] = 64;
    R.imgdata.color.cblack[1] = 48;
    R.imgdata.color.cblack[2] = 72;
    R.imgdata.color.cblack[3] = 48;
  }
  else if (mode == BLACK_TABLE)
  {
    // A 4x4 table on purpose: adjust_bl() folds 1x1 and 2x2 tables into
    // cblack[0..3] and clears cblack[4]/[5], which would silently turn this
    // case back into BLACK_FLAT and leave the per-pixel branch untested.
    R.imgdata.color.cblack[4] = 4;
    R.imgdata.color.cblack[5] = 4;
    for (int i = 0; i < 16; i++)
      R.imgdata.color.cblack[6 + i] = (unsigned)(16 + (i * 7) % 64);
  }

  R.subtract_black();

  size_t px = (size_t)R.imgdata.sizes.iheight * R.imgdata.sizes.iwidth;
  *sum = fnv1a((const unsigned char *)R.imgdata.image,
               px * 4 * sizeof(ushort));
  *dmax = R.imgdata.color.data_maximum;
  return 1;
}

int main(void)
{
  const int W = 1200, H = 800; // small enough for CI, big enough to thread
  size_t n = (size_t)W * H;
  ushort *bayer = (ushort *)malloc(n * sizeof(ushort));
  if (!bayer)
  {
    fprintf(stderr, "alloc failed\n");
    return 2;
  }
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      bayer[(size_t)y * W + x] =
          (ushort)(((x * 7 + y * 13) & 0x3ff) + ((x ^ y) & 0xff) * 16 +
                   ((x / 5 + y / 3) & 0x7f) * 3);

#ifdef _OPENMP
  int maxthreads = omp_get_max_threads();
  printf("OpenMP enabled, max threads = %d\n", maxthreads);
#else
  int maxthreads = 1;
  printf("OpenMP not enabled (serial build)\n");
#endif

  const int quals[] = {0, 3, 11}; // bilinear, AHD, DHT (all parallel paths)
  int failures = 0;

  for (size_t i = 0; i < sizeof(quals) / sizeof(quals[0]); i++)
  {
    int q = quals[i];
    int ok = 1;

#ifdef _OPENMP
    omp_set_num_threads(1);
#endif
    unsigned long long serial = decode(bayer, W, H, q, &ok);

#ifdef _OPENMP
    omp_set_num_threads(maxthreads);
#endif
    unsigned long long par1 = decode(bayer, W, H, q, &ok);
    unsigned long long par2 = decode(bayer, W, H, q, &ok); // determinism

    if (!ok)
    {
      printf("[FAIL] q=%-2d pipeline error\n", q);
      failures++;
      continue;
    }
    if (par1 != par2)
    {
      printf("[FAIL] q=%-2d non-deterministic: %016llx != %016llx\n", q, par1,
             par2);
      failures++;
      continue;
    }
    if (serial != par1)
    {
      printf("[FAIL] q=%-2d threaded output differs from 1-thread: "
             "%016llx != %016llx (DATA RACE)\n",
             q, par1, serial);
      failures++;
      continue;
    }
    printf("[ OK ] q=%-2d deterministic & thread-invariant  checksum=%016llx\n",
           q, par1);
  }

  // ---- part 2: subtract_black_internal(), all three branches ----
  printf("\nsubtract_black branches:\n");
  unsigned long long bsum[BLACK_MODES];
  unsigned bdmax[BLACK_MODES];
  int bok = 1;

  for (int m = 0; m < BLACK_MODES; m++)
  {
    unsigned long long serial = 0, par1 = 0, par2 = 0;
    unsigned dser = 0, dpar1 = 0, dpar2 = 0;

#ifdef _OPENMP
    omp_set_num_threads(1);
#endif
    if (!black_scan(bayer, W, H, m, &serial, &dser))
    {
      printf("[FAIL] black=%-5s setup error\n", black_name(m));
      failures++;
      bok = 0;
      continue;
    }
#ifdef _OPENMP
    omp_set_num_threads(maxthreads);
#endif
    if (!black_scan(bayer, W, H, m, &par1, &dpar1) ||
        !black_scan(bayer, W, H, m, &par2, &dpar2))
    {
      printf("[FAIL] black=%-5s setup error\n", black_name(m));
      failures++;
      bok = 0;
      continue;
    }

    bsum[m] = par1;
    bdmax[m] = dpar1;

    if (par1 != par2 || dpar1 != dpar2)
    {
      printf("[FAIL] black=%-5s non-deterministic: %016llx/%u != %016llx/%u\n",
             black_name(m), par1, dpar1, par2, dpar2);
      failures++;
      continue;
    }
    if (serial != par1)
    {
      printf("[FAIL] black=%-5s threaded pixels differ from 1-thread: "
             "%016llx != %016llx (DATA RACE)\n",
             black_name(m), par1, serial);
      failures++;
      continue;
    }
    // A lost update in the per-thread max merge shows up here and nowhere else.
    if (dser != dpar1)
    {
      printf("[FAIL] black=%-5s data_maximum differs from 1-thread: "
             "%u != %u (LOST MAX UPDATE)\n",
             black_name(m), dpar1, dser);
      failures++;
      continue;
    }
    printf("[ OK ] black=%-5s deterministic & thread-invariant  "
           "checksum=%016llx dmax=%u\n",
           black_name(m), par1, dpar1);
  }

  // Guard against the whole section passing vacuously: if two branches produce
  // identical results, the black levels never took effect and nothing above
  // actually tested the code it claims to.
  if (bok)
  {
    if (bsum[BLACK_NONE] == bsum[BLACK_FLAT] ||
        bsum[BLACK_FLAT] == bsum[BLACK_TABLE] ||
        bsum[BLACK_NONE] == bsum[BLACK_TABLE])
    {
      printf("[FAIL] black branches produced identical output "
             "(%016llx/%016llx/%016llx) - the cblack settings did not take "
             "effect, so this section tested nothing\n",
             bsum[BLACK_NONE], bsum[BLACK_FLAT], bsum[BLACK_TABLE]);
      failures++;
    }
    else
      printf("[ OK ] the three black branches produce distinct output\n");
  }

  free(bayer);

  printf("\n%s (%d failure(s))\n",
         failures ? "PIPELINE CONSISTENCY TEST FAILED" : "ALL CHECKS PASSED",
         failures);
  return failures ? 1 : 0;
}
