/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 *

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

int LibRaw::subtract_black()
{
  adjust_bl();
  return subtract_black_internal();
}

int LibRaw::subtract_black_internal()
{
  CHECK_ORDER_LOW(LIBRAW_PROGRESS_RAW2_IMAGE);

  try
  {
    if (!is_phaseone_compressed() &&
        (C.cblack[0] || C.cblack[1] || C.cblack[2] || C.cblack[3] ||
         (C.cblack[4] && C.cblack[5])))
    {
      int cblk[4], i;
      for (i = 0; i < 4; i++)
        cblk[i] = C.cblack[i];

      int size = S.iheight * S.iwidth;
      int dmax = 0;
      if (C.cblack[4] && C.cblack[5])
      {
// reduction(max:) needs OpenMP 3.1+ (201107) and MSVC's default /openmp is
// 2.0, so accumulate the maximum per thread and merge it in a critical section
// instead. This loop performs the black subtraction as well as the maximum, so
// guarding the pragma out entirely would serialise the whole pass; the form
// below stays parallel on every compiler that has OpenMP at all.
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel
#endif
        {
          int tmax = 0;
#if defined(LIBRAW_USE_OPENMP)
#pragma omp for
#endif
          for (int q = 0; q < size; q++)
          {
            for (unsigned c = 0; c < 4; c++)
            {
              int val = imgdata.image[q][c];
              val -= C.cblack[6 + q / S.iwidth % C.cblack[4] * C.cblack[5] +
                              q % S.iwidth % C.cblack[5]];
              val -= cblk[c];
              imgdata.image[q][c] = CLIP(val);
              if (tmax < val) tmax = val;
            }
          }
#if defined(LIBRAW_USE_OPENMP)
#pragma omp critical(subtract_black_dmax)
#endif
          if (dmax < tmax) dmax = tmax;
        }
      }
      else
      {
// Per-thread maximum merged under critical - see the note above.
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel
#endif
        {
          int tmax = 0;
#if defined(LIBRAW_USE_OPENMP)
#pragma omp for
#endif
          for (int q = 0; q < size; q++)
          {
            for (unsigned c = 0; c < 4; c++)
            {
              int val = imgdata.image[q][c];
              val -= cblk[c];
              imgdata.image[q][c] = CLIP(val);
              if (tmax < val) tmax = val;
            }
          }
#if defined(LIBRAW_USE_OPENMP)
#pragma omp critical(subtract_black_dmax)
#endif
          if (dmax < tmax) dmax = tmax;
        }
      }
      C.data_maximum = dmax & 0xffff;
      C.maximum -= C.black;
      ZERO(C.cblack); // Yeah, we used cblack[6+] values too!
      C.black = 0;
    }
    else
    {
      // Nothing to Do, maximum is already calculated, black level is 0, so no
      // change only calculate channel maximum;
      ushort *p = (ushort *)imgdata.image;
      int dmax = 0;
      const int n = S.iheight * S.iwidth * 4;
// Per-thread maximum merged under critical - see the note above.
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel
#endif
      {
        int tmax = 0;
#if defined(LIBRAW_USE_OPENMP)
#pragma omp for
#endif
        for (int idx = 0; idx < n; idx++)
          if (tmax < p[idx])
            tmax = p[idx];
#if defined(LIBRAW_USE_OPENMP)
#pragma omp critical(subtract_black_dmax)
#endif
        if (dmax < tmax) dmax = tmax;
      }
      C.data_maximum = dmax;
    }
    return 0;
  }
  catch (const LibRaw_exceptions& err)
  {
    EXCEPTION_HANDLER(err);
  }
}
