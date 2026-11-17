/* -*- C++ -*-
 * Copyright 2019-2026 LibRaw LLC (info@libraw.org)
 *
 LibRaw uses code from dcraw.c -- Dave Coffin's raw photo decoder,
 dcraw.c is copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net.
 LibRaw do not use RESTRICTED code from dcraw.c

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/dcraw_defs.h"

/*
   Adaptive Homogeneity-Directed interpolation is based on
   the work of Keigo Hirakawa, Thomas Parks, and Paul Lee.
 */

#ifdef LIBRAW_NOTHREADS
/* File scope rather than local to cielab(), so that the AHD tile loop can
   read the same tables directly. */
static float ahd_cbrt_nothreads[0x10000], ahd_xyz_cam_nothreads[3][4];
#endif

void LibRaw::cielab(ushort rgb[3], short lab[3])
{
  int c, i, j, k;
  float r, xyz[3];
#ifdef LIBRAW_NOTHREADS
#define cbrt ahd_cbrt_nothreads
#define xyz_cam ahd_xyz_cam_nothreads
#else
#define cbrt tls->ahd_data.cbrt
#define xyz_cam tls->ahd_data.xyz_cam
#endif

  if (!rgb)
  {
#ifndef LIBRAW_NOTHREADS
    if (cbrt[0] < -1.0f)
#endif
      for (i = 0; i < 0x10000; i++)
      {
        r = i / 65535.0f;
        cbrt[i] =
            r > 0.008856f ? pow(r, 1.f / 3.0f) : 7.787f * r + 16.f / 116.0f;
      }
    for (i = 0; i < 3; i++)
      for (j = 0; j < colors; j++)
        for (xyz_cam[i][j] = float( k = 0); k < 3; k++)
          xyz_cam[i][j] += float(LibRaw_constants::xyz_rgb[i][k] * rgb_cam[k][j] /
                           LibRaw_constants::d65_white[i]);
    return;
  }
  xyz[0] = xyz[1] = xyz[2] = 0.5;
  FORCC
  {
    xyz[0] += xyz_cam[0][c] * rgb[c];
    xyz[1] += xyz_cam[1][c] * rgb[c];
    xyz[2] += xyz_cam[2][c] * rgb[c];
  }
  xyz[0] = cbrt[CLIP((int)xyz[0])];
  xyz[1] = cbrt[CLIP((int)xyz[1])];
  xyz[2] = cbrt[CLIP((int)xyz[2])];
  lab[0] = short(64 * (116 * xyz[1] - 16));
  lab[1] = short(64 * 500 * (xyz[0] - xyz[1]));
  lab[2] = short(64 * 200 * (xyz[1] - xyz[2]));
#undef cbrt
#undef xyz_cam
}

/* Same arithmetic as LibRaw::cielab(), for the per-pixel AHD loop: the
   tables are passed in so the caller can look them up once per tile instead
   of once per pixel, and the call can be inlined. */
static inline void ahd_cielab_px(const ushort rgb[3], short lab[3],
                                 const float *cbrt_tbl,
                                 const float (*xyz_cam_tbl)[4], int ncolors)
{
  float xyz[3];
  xyz[0] = xyz[1] = xyz[2] = 0.5;
  for (int c = 0; c < ncolors; c++)
  {
    xyz[0] += xyz_cam_tbl[0][c] * rgb[c];
    xyz[1] += xyz_cam_tbl[1][c] * rgb[c];
    xyz[2] += xyz_cam_tbl[2][c] * rgb[c];
  }
  xyz[0] = cbrt_tbl[CLIP((int)xyz[0])];
  xyz[1] = cbrt_tbl[CLIP((int)xyz[1])];
  xyz[2] = cbrt_tbl[CLIP((int)xyz[2])];
  lab[0] = short(64 * (116 * xyz[1] - 16));
  lab[1] = short(64 * 500 * (xyz[0] - xyz[1]));
  lab[2] = short(64 * 200 * (xyz[1] - xyz[2]));
}

void LibRaw::ahd_interpolate_green_h_and_v(
    int top, int left, ushort (*out_rgb)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3])
{
  int row, col;
  int c, val;
  ushort(*pix)[4];
  const int rowlimit = MIN(top + LIBRAW_AHD_TILE, height - 2);
  const int collimit = MIN(left + LIBRAW_AHD_TILE, width - 2);

  for (row = top; row < rowlimit; row++)
  {
    col = left + (FC(row, left) & 1);
    for (c = FC(row, col); col < collimit; col += 2)
    {
      pix = image + row * width + col;
      val =
          ((pix[-1][1] + pix[0][c] + pix[1][1]) * 2 - pix[-2][c] - pix[2][c]) >>
          2;
      out_rgb[0][row - top][col - left][1] = ULIM(val, pix[-1][1], pix[1][1]);
      val = ((pix[-width][1] + pix[0][c] + pix[width][1]) * 2 -
             pix[-2 * width][c] - pix[2 * width][c]) >>
            2;
      out_rgb[1][row - top][col - left][1] =
          ULIM(val, pix[-width][1], pix[width][1]);
    }
  }
}
void LibRaw::ahd_interpolate_r_and_b_in_rgb_and_convert_to_cielab(
    int top, int left, ushort (*inout_rgb)[LIBRAW_AHD_TILE][3],
    short (*out_lab)[LIBRAW_AHD_TILE][3])
{
  unsigned row, col;
  int c, val;
  ushort(*pix)[4];
  ushort(*rix)[3];
  short(*lix)[3];
  const unsigned num_pix_per_row = 4 * width;
  const unsigned rowlimit = MIN(top + LIBRAW_AHD_TILE - 1, height - 3);
  const unsigned collimit = MIN(left + LIBRAW_AHD_TILE - 1, width - 3);
  ushort *pix_above;
  ushort *pix_below;
  int t1, t2;
  /* rgb has 3 components per pixel here, so never read more than that */
  const int ncolors = colors < 3 ? colors : 3;
#ifdef LIBRAW_NOTHREADS
  const float *cbrt_tbl = ahd_cbrt_nothreads;
  const float (*xyz_cam_tbl)[4] = ahd_xyz_cam_nothreads;
#else
  const float *cbrt_tbl = tls->ahd_data.cbrt;
  const float (*xyz_cam_tbl)[4] = tls->ahd_data.xyz_cam;
#endif

  for (row = top + 1; row < rowlimit; row++)
  {
    pix = image + row * width + left;
    rix = &inout_rgb[row - top][0];
    lix = &out_lab[row - top][0];

    for (col = left + 1; col < collimit; col++)
    {
      pix++;
      pix_above = &pix[0][0] - num_pix_per_row;
      pix_below = &pix[0][0] + num_pix_per_row;
      rix++;
      lix++;

      c = 2 - FC(row, col);

      if (c == 1)
      {
        c = FC(row + 1, col);
        t1 = 2 - c;
        val = pix[0][1] +
              ((pix[-1][t1] + pix[1][t1] - rix[-1][1] - rix[1][1]) >> 1);
        rix[0][t1] = CLIP(val);
        val =
            pix[0][1] + ((pix_above[c] + pix_below[c] -
                          rix[-LIBRAW_AHD_TILE][1] - rix[LIBRAW_AHD_TILE][1]) >>
                         1);
      }
      else
      {
        t1 = -4 + c; /* -4+c: pixel of color c to the left */
        t2 = 4 + c;  /* 4+c: pixel of color c to the right */
        val = rix[0][1] +
              ((pix_above[t1] + pix_above[t2] + pix_below[t1] + pix_below[t2] -
                rix[-LIBRAW_AHD_TILE - 1][1] - rix[-LIBRAW_AHD_TILE + 1][1] -
                rix[+LIBRAW_AHD_TILE - 1][1] - rix[+LIBRAW_AHD_TILE + 1][1] +
                1) >>
               2);
      }
      rix[0][c] = CLIP(val);
      c = FC(row, col);
      rix[0][c] = pix[0][c];
      ahd_cielab_px(rix[0], lix[0], cbrt_tbl, xyz_cam_tbl, ncolors);
    }
  }
}
void LibRaw::ahd_interpolate_r_and_b_and_convert_to_cielab(
    int top, int left, ushort (*inout_rgb)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3],
    short (*out_lab)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3])
{
  int direction;
  for (direction = 0; direction < 2; direction++)
  {
    ahd_interpolate_r_and_b_in_rgb_and_convert_to_cielab(
        top, left, inout_rgb[direction], out_lab[direction]);
  }
}

/* L and a/b distance between two CIELab pixels. The squares are taken in
   unsigned arithmetic: two shorts can differ by 65535, whose square does not
   fit an int. */
static inline void ahd_lab_diff(const short *p, const short *q, unsigned &ld,
                                unsigned &abd)
{
  const int dl = p[0] - q[0];
  const unsigned da = unsigned(p[1] - q[1]), db = unsigned(p[2] - q[2]);
  ld = unsigned(dl < 0 ? -dl : dl);
  abd = da * da + db * db;
}

void LibRaw::ahd_interpolate_build_homogeneity_map(
    int top, int left, short (*lab)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3],
    char (*out_homogeneity_map)[LIBRAW_AHD_TILE][2])
{
  int row, col;
  int tr, tc;
  unsigned ldiff[2][4], abdiff[2][4], leps, abeps;
  const int rowlimit = MIN(top + LIBRAW_AHD_TILE - 2, height - 4);
  const int collimit = MIN(left + LIBRAW_AHD_TILE - 2, width - 4);
  /* A distance is symmetric, so every pair of neighbours is measured once:
     the "right" distance of a pixel is the "left" one of the next pixel, and
     its "below" distance - kept here per column and per interpolation
     direction - is the "above" one of the pixel under it. */
  unsigned below_l[2][LIBRAW_AHD_TILE], below_ab[2][LIBRAW_AHD_TILE];

  memset(out_homogeneity_map, 0, 2 * LIBRAW_AHD_TILE * LIBRAW_AHD_TILE);

  for (row = top + 2; row < rowlimit; row++)
  {
    tr = row - top;
    char(*hmap)[2] = &out_homogeneity_map[tr][2];
    const short(*lh)[3] = &lab[0][tr][2]; /* horizontally interpolated */
    const short(*lv)[3] = &lab[1][tr][2]; /* vertically interpolated */
    const bool first_row = (row == top + 2);

    /* indices: 0 left, 1 right, 2 above, 3 below */
    ahd_lab_diff(lh[0], lh[-1], ldiff[0][1], abdiff[0][1]);
    ahd_lab_diff(lv[0], lv[-1], ldiff[1][1], abdiff[1][1]);

    for (col = left + 2, tc = 2; col < collimit; col++, tc++, hmap++, lh++, lv++)
    {
      ldiff[0][0] = ldiff[0][1];
      abdiff[0][0] = abdiff[0][1];
      ldiff[1][0] = ldiff[1][1];
      abdiff[1][0] = abdiff[1][1];
      ahd_lab_diff(lh[0], lh[1], ldiff[0][1], abdiff[0][1]);
      ahd_lab_diff(lv[0], lv[1], ldiff[1][1], abdiff[1][1]);
      if (first_row)
      {
        ahd_lab_diff(lh[0], lh[-LIBRAW_AHD_TILE], ldiff[0][2], abdiff[0][2]);
        ahd_lab_diff(lv[0], lv[-LIBRAW_AHD_TILE], ldiff[1][2], abdiff[1][2]);
      }
      else
      {
        ldiff[0][2] = below_l[0][tc];
        abdiff[0][2] = below_ab[0][tc];
        ldiff[1][2] = below_l[1][tc];
        abdiff[1][2] = below_ab[1][tc];
      }
      ahd_lab_diff(lh[0], lh[LIBRAW_AHD_TILE], ldiff[0][3], abdiff[0][3]);
      ahd_lab_diff(lv[0], lv[LIBRAW_AHD_TILE], ldiff[1][3], abdiff[1][3]);
      below_l[0][tc] = ldiff[0][3];
      below_ab[0][tc] = abdiff[0][3];
      below_l[1][tc] = ldiff[1][3];
      below_ab[1][tc] = abdiff[1][3];

      leps = MIN(MAX(ldiff[0][0], ldiff[0][1]), MAX(ldiff[1][2], ldiff[1][3]));
      abeps =
          MIN(MAX(abdiff[0][0], abdiff[0][1]), MAX(abdiff[1][2], abdiff[1][3]));

      /* '&' on purpose: both tests are cheap, a branch per test is not */
      hmap[0][0] = char(((ldiff[0][0] <= leps) & (abdiff[0][0] <= abeps)) +
                        ((ldiff[0][1] <= leps) & (abdiff[0][1] <= abeps)) +
                        ((ldiff[0][2] <= leps) & (abdiff[0][2] <= abeps)) +
                        ((ldiff[0][3] <= leps) & (abdiff[0][3] <= abeps)));
      hmap[0][1] = char(((ldiff[1][0] <= leps) & (abdiff[1][0] <= abeps)) +
                        ((ldiff[1][1] <= leps) & (abdiff[1][1] <= abeps)) +
                        ((ldiff[1][2] <= leps) & (abdiff[1][2] <= abeps)) +
                        ((ldiff[1][3] <= leps) & (abdiff[1][3] <= abeps)));
    }
  }
}
void LibRaw::ahd_interpolate_combine_homogeneous_pixels(
    int top, int left, ushort (*rgb)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3],
    char (*homogeneity_map)[LIBRAW_AHD_TILE][2])
{
  int row, col;
  int tr;
  int direction;
  int c;
  const int rowlimit = MIN(top + LIBRAW_AHD_TILE - 3, height - 5);
  const int collimit = MIN(left + LIBRAW_AHD_TILE - 3, width - 5);

  ushort(*pix)[4];
  ushort(*rix[2])[3];

  for (row = top + 3; row < rowlimit; row++)
  {
    tr = row - top;
    pix = &image[row * width + left + 2];
    for (direction = 0; direction < 2; direction++)
    {
      rix[direction] = &rgb[direction][tr][2];
    }
    /* The 3x3 homogeneity sum is kept as three column sums that slide along
       the row: one new column per pixel instead of nine cells. */
    const char(*hm_up)[2] = homogeneity_map[tr - 1];
    const char(*hm_mid)[2] = homogeneity_map[tr];
    const char(*hm_down)[2] = homogeneity_map[tr + 1];
    int colsum[2][3];
    for (direction = 0; direction < 2; direction++)
    {
      colsum[direction][1] =
          hm_up[2][direction] + hm_mid[2][direction] + hm_down[2][direction];
      colsum[direction][2] =
          hm_up[3][direction] + hm_mid[3][direction] + hm_down[3][direction];
    }

    for (col = left + 3; col < collimit; col++)
    {
      const int tc = col - left;
      int hm[2];
      pix++;
      for (direction = 0; direction < 2; direction++)
      {
        rix[direction]++;
        colsum[direction][0] = colsum[direction][1];
        colsum[direction][1] = colsum[direction][2];
        colsum[direction][2] = hm_up[tc + 1][direction] +
                               hm_mid[tc + 1][direction] +
                               hm_down[tc + 1][direction];
        hm[direction] = colsum[direction][0] + colsum[direction][1] +
                        colsum[direction][2];
      }
      if (hm[0] != hm[1])
      {
        memcpy(pix[0], rix[hm[1] > hm[0]][0], 3 * sizeof(ushort));
      }
      else
      {
        FORC3 { pix[0][c] = (rix[0][0][c] + rix[1][0][c]) >> 1; }
      }
    }
  }
}
void LibRaw::ahd_interpolate()
{
    int terminate_flag = 0;
    cielab(0, 0);
    border_interpolate(5);

#ifdef LIBRAW_USE_OPENMP
    int buffer_count = omp_get_max_threads();
#else
    int buffer_count = 1;
#endif

    size_t buffer_size = 26 * LIBRAW_AHD_TILE * LIBRAW_AHD_TILE; /* 1664 kB */
    char** buffers = malloc_omp_buffers(buffer_count, buffer_size);

#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for schedule(dynamic) default(none) shared(terminate_flag) firstprivate(buffers)
#endif
    for (int top = 2; top < height - 5; top += LIBRAW_AHD_TILE - 6)
    {
#ifdef LIBRAW_USE_OPENMP
        if (0 == omp_get_thread_num())
#endif
            if (callbacks.progress_cb)
            {
                int rr = (*callbacks.progress_cb)(callbacks.progresscb_data,
                    LIBRAW_PROGRESS_INTERPOLATE,
                    top - 2, height - 7);
                if (rr)
                    terminate_flag = 1;
            }

#if defined(LIBRAW_USE_OPENMP)
        char* buffer = buffers[omp_get_thread_num()];
#else
        char* buffer = buffers[0];
#endif

        ushort(*rgb)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3];
        short(*lab)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3];
        char(*homo)[LIBRAW_AHD_TILE][2];

        rgb = (ushort(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3])buffer;
        lab = (short(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3])(
            buffer + 12 * LIBRAW_AHD_TILE * LIBRAW_AHD_TILE);
        homo = (char(*)[LIBRAW_AHD_TILE][2])(buffer + 24 * LIBRAW_AHD_TILE *
            LIBRAW_AHD_TILE);

        for (int left = 2; !terminate_flag && (left < width - 5);
            left += LIBRAW_AHD_TILE - 6)
        {
            ahd_interpolate_green_h_and_v(top, left, rgb);
            ahd_interpolate_r_and_b_and_convert_to_cielab(top, left, rgb, lab);
            ahd_interpolate_build_homogeneity_map(top, left, lab, homo);
            ahd_interpolate_combine_homogeneous_pixels(top, left, rgb, homo);
        }
    }

    free_omp_buffers(buffers, buffer_count);

    if (terminate_flag)
        throw LIBRAW_EXCEPTION_CANCELLED_BY_CALLBACK;
}
