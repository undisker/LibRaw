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
 *                          (thread-local histogram merge, reduction(max),
 *                          per-pixel/per-row writes) contain no data race that
 *                          changes the result.
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

  free(bayer);

  printf("\n%s (%d/%d quality modes passed)\n",
         failures ? "PIPELINE CONSISTENCY TEST FAILED" : "ALL CHECKS PASSED",
         (int)(sizeof(quals) / sizeof(quals[0])) - failures,
         (int)(sizeof(quals) / sizeof(quals[0])));
  return failures ? 1 : 0;
}
