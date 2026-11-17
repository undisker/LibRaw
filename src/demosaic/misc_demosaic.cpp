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

void LibRaw::pre_interpolate()
{
  ushort(*img)[4];
  int row, col, c;
  RUN_CALLBACK(LIBRAW_PROGRESS_PRE_INTERPOLATE, 0, 2);
  if (shrink)
  {
    if (half_size)
    {
      height = iheight;
      width = iwidth;
      if (filters == 9)
      {
        /* Find the all-green half-size phase from the CFA layout: scanning
           pixel values for R == B == 0 picks a wrong phase when the top-left
           corner is dark and noisy. */
        int grow = -1, gcol = -1;
        for (int r = 0; r < 3 && grow < 0; r++)
          for (int k = 0; k < 3 && grow < 0; k++)
            if (fcol(2 * r, 2 * k) == 1 && fcol(2 * r, 2 * k + 1) == 1 &&
                fcol(2 * r + 1, 2 * k) == 1 && fcol(2 * r + 1, 2 * k + 1) == 1)
            {
              grow = r;
              gcol = k ? k : 3; /* the fill below reads img[-1] */
            }
        if (grow >= 0)
        {
          row = grow;
          col = gcol;
          goto break2;
        }
        for (row = 0; row < 3; row++)
          for (col = 1; col < 4; col++)
            if (!(image[row * width + col][0] | image[row * width + col][2]))
              goto break2;
      break2:
        for (; row < height; row += 3)
          for (col = (col - 1) % 3 + 1; col < width - 1; col += 3)
          {
            img = image + row * width + col;
            for (c = 0; c < 3; c += 2)
              img[0][c] = (img[-1][c] + img[1][c]) >> 1;
          }
      }
    }
    else
    {
      int extra = filters ? (filters == 9 ? 6 : 2) : 0;
      img = (ushort(*)[4])calloc((height+extra), (width+extra) * sizeof *img);
      for (row = 0; row < height; row++)
        for (col = 0; col < width; col++)
        {
          c = fcol(row, col);
          img[row * width + col][c] =
              image[(row >> 1) * iwidth + (col >> 1)][c];
        }
      free(image);
      image = img;
      shrink = 0;
    }
  }
  if (filters > 1000 && colors == 3)
  {
    mix_green = four_color_rgb ^ half_size;
    if (four_color_rgb | half_size)
      colors++;
    else
    {
      const int grow0 = FC(1, 0) >> 1;
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(row, col) schedule(static)
#endif
      for (row = grow0; row < height; row += 2)
        for (col = FC(row, 1) & 1; col < width; col += 2)
          image[row * width + col][1] = image[row * width + col][3];
      filters &= ~((filters & 0x55555555U) << 1);
    }
  }
  if (half_size)
    filters = 0;
  RUN_CALLBACK(LIBRAW_PROGRESS_PRE_INTERPOLATE, 1, 2);
}

void LibRaw::border_interpolate(int border)
{
  unsigned row, col, y, x, f, c, sum[8];

  for (row = 0; row < height; row++)
    for (col = 0; col < width; col++)
    {
      if (col == (unsigned)border && row >= (unsigned)border && row < (unsigned)(height - border))
        col = width - border;
      memset(sum, 0, sizeof sum);
      for (y = row - 1; y != row + 2; y++)
        for (x = col - 1; x != col + 2; x++)
          if (y < height && x < width)
          {
            f = fcol(y, x);
            sum[f] += image[y * width + x][f];
            sum[f + 4]++;
          }
      f = fcol(row, col);
      FORC(unsigned(colors)) if (c != f && sum[c + 4]) image[row * width + col][c] =
          sum[c] / sum[c + 4];
    }
}

void LibRaw::lin_interpolate_loop(int *code, int size)
{
  int row;
  /* Rows are independent: a pixel only reads its neighbours' own CFA colour
     (never written here) and only writes its own missing colours. */
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) schedule(static)
#endif
  for (row = 1; row < height - 1; row++)
  {
    int col, *ip;
    ushort *pix;
    int *rowcode = code + (row % size) * 16 * 32;
    int cmod = 1 % size;
    for (col = 1; col < width - 1; col++)
    {
      int i;
      int sum[4];
      pix = image[row * width + col];
      ip = rowcode + cmod * 32;
      if (++cmod == size)
        cmod = 0;
      sum[0] = sum[1] = sum[2] = sum[3] = 0;
      for (i = *ip++; i--; ip += 3)
        sum[ip[2]] += pix[ip[0]] << ip[1];
      for (i = colors; --i; ip += 2)
        pix[ip[0]] = sum[ip[0]] * ip[1] >> 8;
    }
  }
}

void LibRaw::lin_interpolate()
{
  std::vector<int> code_buffer(16 * 16 * 32);
  int* code = &code_buffer[0], size = 16, *ip, sum[4];
  int f, c, x, y, row, col, shift, color;

  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 0, 3);

  if (filters == 9)
    size = 6;
  border_interpolate(1);
  for (row = 0; row < size; row++)
    for (col = 0; col < size; col++)
    {
      ip = code + (((row * 16) + col) * 32) + 1;
      f = fcol(row, col);
      memset(sum, 0, sizeof sum);
      for (y = -1; y <= 1; y++)
        for (x = -1; x <= 1; x++)
        {
          shift = (y == 0) + (x == 0);
          color = fcol(row + y + 48, col + x + 48);
          if (color == f)
            continue;
          *ip++ = (width * y + x) * 4 + color;
          *ip++ = shift;
          *ip++ = color;
          sum[color] += 1 << shift;
        }
      code[(row * 16 + col) * 32] = int((ip - (code + ((row * 16) + col) * 32)) / 3);
      FORCC
      if (c != f)
      {
        *ip++ = c;
        *ip++ = sum[c] > 0 ? 256 / sum[c] : 0;
      }
    }
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 1, 3);
  lin_interpolate_loop(code, size);
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 2, 3);
}

/*
   This algorithm is officially called:

   "Interpolation using a Threshold-based variable number of gradients"

   described in
   http://scien.stanford.edu/pages/labsite/1999/psych221/projects/99/tingchen/algodep/vargra.html

   I've extended the basic idea to work with non-Bayer filter arrays.
   Gradients are numbered clockwise from NW=0 to W=7.
 */
void LibRaw::vng_interpolate()
{
  static const signed char *cp,
      terms[] =
          {-2, -2, +0,   -1, 0,  0x01, -2, -2, +0,   +0, 1,  0x01, -2, -1, -1,
           +0, 0,  0x01, -2, -1, +0,   -1, 0,  0x02, -2, -1, +0,   +0, 0,  0x03,
           -2, -1, +0,   +1, 1,  0x01, -2, +0, +0,   -1, 0,  0x06, -2, +0, +0,
           +0, 1,  0x02, -2, +0, +0,   +1, 0,  0x03, -2, +1, -1,   +0, 0,  0x04,
           -2, +1, +0,   -1, 1,  0x04, -2, +1, +0,   +0, 0,  0x06, -2, +1, +0,
           +1, 0,  0x02, -2, +2, +0,   +0, 1,  0x04, -2, +2, +0,   +1, 0,  0x04,
           -1, -2, -1,   +0, 0,  -128, -1, -2, +0,   -1, 0,  0x01, -1, -2, +1,
           -1, 0,  0x01, -1, -2, +1,   +0, 1,  0x01, -1, -1, -1,   +1, 0,  -120,
           -1, -1, +1,   -2, 0,  0x40, -1, -1, +1,   -1, 0,  0x22, -1, -1, +1,
           +0, 0,  0x33, -1, -1, +1,   +1, 1,  0x11, -1, +0, -1,   +2, 0,  0x08,
           -1, +0, +0,   -1, 0,  0x44, -1, +0, +0,   +1, 0,  0x11, -1, +0, +1,
           -2, 1,  0x40, -1, +0, +1,   -1, 0,  0x66, -1, +0, +1,   +0, 1,  0x22,
           -1, +0, +1,   +1, 0,  0x33, -1, +0, +1,   +2, 1,  0x10, -1, +1, +1,
           -1, 1,  0x44, -1, +1, +1,   +0, 0,  0x66, -1, +1, +1,   +1, 0,  0x22,
           -1, +1, +1,   +2, 0,  0x10, -1, +2, +0,   +1, 0,  0x04, -1, +2, +1,
           +0, 1,  0x04, -1, +2, +1,   +1, 0,  0x04, +0, -2, +0,   +0, 1,  -128,
           +0, -1, +0,   +1, 1,  -120, +0, -1, +1,   -2, 0,  0x40, +0, -1, +1,
           +0, 0,  0x11, +0, -1, +2,   -2, 0,  0x40, +0, -1, +2,   -1, 0,  0x20,
           +0, -1, +2,   +0, 0,  0x30, +0, -1, +2,   +1, 1,  0x10, +0, +0, +0,
           +2, 1,  0x08, +0, +0, +2,   -2, 1,  0x40, +0, +0, +2,   -1, 0,  0x60,
           +0, +0, +2,   +0, 1,  0x20, +0, +0, +2,   +1, 0,  0x30, +0, +0, +2,
           +2, 1,  0x10, +0, +1, +1,   +0, 0,  0x44, +0, +1, +1,   +2, 0,  0x10,
           +0, +1, +2,   -1, 1,  0x40, +0, +1, +2,   +0, 0,  0x60, +0, +1, +2,
           +1, 0,  0x20, +0, +1, +2,   +2, 0,  0x10, +1, -2, +1,   +0, 0,  -128,
           +1, -1, +1,   +1, 0,  -120, +1, +0, +1,   +2, 0,  0x08, +1, +0, +2,
           -1, 0,  0x40, +1, +0, +2,   +1, 0,  0x10},
      chood[] = {-1, -1, -1, 0, -1, +1, 0, +1, +1, +1, +1, 0, +1, -1, 0, -1};
  ushort(*brow[5])[4];
  int prow = 8, pcol = 2, *ip, *code[16][16];
  int row, col, x, y, x1, x2, y1, y2, t, weight, grads, color, diag;
  int g;

  if (width < 8 || height < 8) return;  // skip interploation on too small images

  lin_interpolate();

  if (filters == 1)
    prow = pcol = 16;
  if (filters == 9)
    prow = pcol = 6;
  ip = (int *)calloc(prow * pcol, 1280);
  for (row = 0; row < prow; row++) /* Precalculate for VNG */
    for (col = 0; col < pcol; col++)
    {
      code[row][col] = ip;
      for (cp = terms, t = 0; t < 64; t++)
      {
        y1 = *cp++;
        x1 = *cp++;
        y2 = *cp++;
        x2 = *cp++;
        weight = *cp++;
        grads = *cp++;
        color = fcol(row + y1 + 144, col + x1 + 144);
        if (fcol(row + y2 + 144, col + x2 + 144) != color)
          continue;
        diag = (fcol(row, col + 1) == color && fcol(row + 1, col) == color) ? 2
                                                                            : 1;
        if (abs(y1 - y2) == diag && abs(x1 - x2) == diag)
          continue;
        *ip++ = (y1 * width + x1) * 4 + color;
        *ip++ = (y2 * width + x2) * 4 + color;
        *ip++ = weight;
        for (g = 0; g < 8; g++)
          if (grads & 1 << g)
            *ip++ = g;
        *ip++ = -1;
      }
      *ip++ = INT_MAX;
      for (cp = chood, g = 0; g < 8; g++)
      {
        y = *cp++;
        x = *cp++;
        *ip++ = (y * width + x) * 4;
        color = fcol(row, col);
        if (fcol(row + y + 144, col + x + 144) != color &&
            fcol(row + y * 2 + 144, col + x * 2 + 144) == color)
          *ip++ = (y * width + x) * 8 + color;
        else
          *ip++ = 0;
      }
    }
  /* Do VNG interpolation.
     Every output row is a pure function of the lin_interpolate()d rows
     row-2..row+2: a finished row is only stored back two rows later, after
     the last stencil that needs its old content. That makes the row range
     splittable between threads, as long as the first two and the last two
     rows of each thread's range are held back until every thread is done
     (the neighbouring ranges still read them). */
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 1, ((height - 3) / 256) + 1);
  const int rfirst = 2, rlast = height - 2;
  int maxthr = 1;
#ifdef LIBRAW_USE_OPENMP
  maxthr = omp_get_max_threads();
  if (maxthr < 1)
    maxthr = 1;
#endif
  brow[4] = (ushort(*)[4])calloc((size_t)maxthr * width * 5, sizeof **brow);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel default(shared) num_threads(maxthr)
#endif
  {
    int tid = 0, nthr = 1;
#ifdef LIBRAW_USE_OPENMP
    tid = omp_get_thread_num();
    nthr = omp_get_num_threads();
#endif
    ushort(*tb)[4] = brow[4] + (size_t)tid * width * 5;
    ushort(*head[2])[4] = {tb, tb + width};
    ushort(*trow[4])[4] = {tb + 2 * width, tb + 3 * width, tb + 4 * width, 0};
    const int rs = rfirst + int(INT64(rlast - rfirst) * tid / nthr);
    const int re = rfirst + int(INT64(rlast - rfirst) * (tid + 1) / nthr);
    const size_t rowbytes = (width - 4) * sizeof *image;
    int nhead = 0;
    int trw, tcol, tgval[8], tgmin, tgmax, tsum[4], tg, tdiff, tthold, tnum,
        tcolor, tt, tc, *tip;
    ushort *tpix;

    for (trw = rs; trw < re; trw++)
    {
      for (tcol = 2; tcol < width - 2; tcol++)
      {
        tpix = image[trw * width + tcol];
        tip = code[trw % prow][tcol % pcol];
        memset(tgval, 0, sizeof tgval);
        while ((tg = tip[0]) != INT_MAX)
        { /* Calculate gradients */
          tdiff = ABS(tpix[tg] - tpix[tip[1]]) << tip[2];
          tgval[tip[3]] += tdiff;
          tip += 5;
          if ((tg = tip[-1]) == -1)
            continue;
          tgval[tg] += tdiff;
          while ((tg = *tip++) != -1)
            tgval[tg] += tdiff;
        }
        tip++;
        tgmin = tgmax = tgval[0]; /* Choose a threshold */
        for (tg = 1; tg < 8; tg++)
        {
          if (tgmin > tgval[tg])
            tgmin = tgval[tg];
          if (tgmax < tgval[tg])
            tgmax = tgval[tg];
        }
        if (tgmax == 0)
        {
          memcpy(trow[2][tcol], tpix, sizeof *image);
          continue;
        }
        tthold = tgmin + (tgmax >> 1);
        memset(tsum, 0, sizeof tsum);
        tcolor = fcol(trw, tcol);
        for (tnum = tg = 0; tg < 8; tg++, tip += 2)
        { /* Average the neighbors */
          if (tgval[tg] <= tthold)
          {
            for (tc = 0; tc < colors; tc++)
              if (tc == tcolor && tip[1])
                tsum[tc] += (tpix[tc] + tpix[tip[1]]) >> 1;
              else
                tsum[tc] += tpix[tip[0] + tc];
            tnum++;
          }
        }
        for (tc = 0; tc < colors; tc++)
        { /* Save to buffer */
          tt = tpix[tcolor];
          if (tc != tcolor)
            tt += (tsum[tc] - tsum[tcolor]) / tnum;
          trow[2][tcol][tc] = CLIP(tt);
        }
      }
      if (trw - 2 >= rs) /* Store the row finished two rows ago */
      {
        if (trw - 2 < rs + 2)
          memcpy(head[nhead++] + 2, trow[0] + 2, rowbytes);
        else
          memcpy(image[(trw - 2) * width + 2], trow[0] + 2, rowbytes);
      }
      for (tg = 0; tg < 4; tg++)
        trow[(tg - 1) & 3] = trow[tg];
    }
#ifdef LIBRAW_USE_OPENMP
#pragma omp barrier
#endif
    for (tg = 0; tg < nhead; tg++)
      memcpy(image[(rs + tg) * width + 2], head[tg] + 2, rowbytes);
    if (re - 2 >= rs)
      memcpy(image[(re - 2) * width + 2], trow[0] + 2, rowbytes);
    if (re - 1 >= rs)
      memcpy(image[(re - 1) * width + 2], trow[1] + 2, rowbytes);
  }
  free(brow[4]);
  free(code[0][0]);
}

/*
   Patterned Pixel Grouping Interpolation by Alain Desbiolles
*/
void LibRaw::ppg_interpolate()
{
  int dir[5] = {1, width, -1, -width, 1};
  int row, col, diff[2], guess[2], c, d, i;
  ushort(*pix)[4];

  border_interpolate(3);

  /*  Fill in the green layer with gradients and pattern recognition: */
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 0, 3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c,  \
                                                 i, pix) schedule(static)
#endif
  for (row = 3; row < height - 3; row++)
    for (col = 3 + (FC(row, 3) & 1), c = FC(row, col); col < width - 3;
         col += 2)
    {
      pix = image + row * width + col;
      for (i = 0; i < 2; i++)
      {
        d = dir[i];
        guess[i] = (pix[-d][1] + pix[0][c] + pix[d][1]) * 2 - pix[-2 * d][c] -
                   pix[2 * d][c];
        diff[i] =
            (ABS(pix[-2 * d][c] - pix[0][c]) + ABS(pix[2 * d][c] - pix[0][c]) +
             ABS(pix[-d][1] - pix[d][1])) *
                3 +
            (ABS(pix[3 * d][1] - pix[d][1]) +
             ABS(pix[-3 * d][1] - pix[-d][1])) *
                2;
      }
      d = dir[i = diff[0] > diff[1]];
      pix[0][1] = ULIM(guess[i] >> 2, pix[d][1], pix[-d][1]);
    }
  /*  Calculate red and blue for each green pixel:		*/
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 1, 3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c,  \
                                                 i, pix) schedule(static)
#endif
  for (row = 1; row < height - 1; row++)
    for (col = 1 + (FC(row, 2) & 1), c = FC(row, col + 1); col < width - 1;
         col += 2)
    {
      pix = image + row * width + col;
      for (i = 0; i < 2; c = 2 - c, i++)
      {
        d = dir[i];
        pix[0][c] = CLIP(
            (pix[-d][c] + pix[d][c] + 2 * pix[0][1] - pix[-d][1] - pix[d][1]) >>
            1);
      }
    }
  /*  Calculate blue for red pixels and vice versa:		*/
  RUN_CALLBACK(LIBRAW_PROGRESS_INTERPOLATE, 2, 3);
#ifdef LIBRAW_USE_OPENMP
#pragma omp parallel for default(shared) private(guess, diff, row, col, d, c,  \
                                                 i, pix) schedule(static)
#endif
  for (row = 1; row < height - 1; row++)
    for (col = 1 + (FC(row, 1) & 1), c = 2 - FC(row, col); col < width - 1;
         col += 2)
    {
      pix = image + row * width + col;
      for (i = 0; i < 2; i++)
      {
        d = dir[i] + dir[i+1];
        diff[i] = ABS(pix[-d][c] - pix[d][c]) + ABS(pix[-d][1] - pix[0][1]) +
                  ABS(pix[d][1] - pix[0][1]);
        guess[i] =
            pix[-d][c] + pix[d][c] + 2 * pix[0][1] - pix[-d][1] - pix[d][1];
      }
      if (diff[0] != diff[1])
        pix[0][c] = CLIP(guess[diff[0] > diff[1]] >> 1);
      else
        pix[0][c] = CLIP((guess[0] + guess[1]) >> 2);
    }
}
