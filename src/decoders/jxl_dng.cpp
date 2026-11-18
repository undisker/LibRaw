/* -*- C++ -*-
 * File: jxl_dng.cpp
 * Written for the undisker LibRaw fork, 2026; not part of upstream LibRaw.
 *
   DNG with JPEG XL compression (DNG 1.7, TIFF Compression=52546), decoded
   with libjxl directly. Upstream LibRaw reads these files only through the
   Adobe DNG SDK, which this fork does not use.

   Built when USE_JXL is defined (CMake: LIBRAW_ENABLE_JXL); without it the
   loader reports the format as unsupported, as before.

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/dcraw_defs.h"

#ifndef USE_JXL

void LibRaw::jxl_dng_load_raw()
{
  // Needs libjxl (USE_JXL) or, in upstream's configuration, the Adobe DNG SDK
  throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;
}

#else

#include <jxl/decode.h>
#include <jxl/version.h>
/* Integer samples of less than 16 bits: libjxl 0.8 and later can hand them
   out at the bit depth of the codestream. Older versions only stretch them to
   the full range of the output type, with rounding that is not documented, so
   there the image is taken as float and converted back - exact for integer
   data, at the price of a buffer twice the size. */
#if JPEGXL_NUMERIC_VERSION >= JPEGXL_COMPUTE_NUMERIC_VERSION(0, 8, 0)
#define LIBRAW_JXL_NATIVE_DEPTH 1
#else
#define LIBRAW_JXL_NATIVE_DEPTH 0
#endif
#ifdef USE_JXL_THREADS
#include <jxl/thread_parallel_runner.h>
#endif
#include <algorithm>
#include <vector>

namespace
{
struct jxl_dng_tile
{
  INT64 offset, size;
  unsigned row, col;   // position in the stored image
  unsigned wide, high; // nominal size of the tile / strip
};

// Owns the libjxl objects, so that an exception cannot leak them
struct jxl_dng_decoder
{
  JxlDecoder *dec;
  void *runner;
  jxl_dng_decoder() : dec(JxlDecoderCreate(NULL)), runner(NULL)
  {
#ifdef USE_JXL_THREADS
    size_t nthreads = JxlThreadParallelRunnerDefaultNumWorkerThreads();
#ifdef LIBRAW_USE_OPENMP
    const int omp_threads = omp_get_max_threads();
    if (omp_threads >= 1)
      nthreads = size_t(omp_threads);
#endif
    if (nthreads > 1)
      runner = JxlThreadParallelRunnerCreate(NULL, nthreads);
#endif
  }
  ~jxl_dng_decoder()
  {
    if (dec)
      JxlDecoderDestroy(dec);
#ifdef USE_JXL_THREADS
    if (runner)
      JxlThreadParallelRunnerDestroy(runner);
#endif
  }
  // Start on a new codestream
  bool reset()
  {
    if (!dec)
      return false;
    JxlDecoderReset(dec);
#ifdef USE_JXL_THREADS
    if (runner && JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner) != JXL_DEC_SUCCESS)
      return false;
#endif
    return JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) == JXL_DEC_SUCCESS;
  }
};

/* Row/ColumnInterleaveFactor: the stored rows (columns) are the rows 0, f,
   2f, ... of the image, then the rows 1, f+1, ..., and so on. Returns, for
   each stored index, the index it has in the image. */
static void jxl_dng_interleave_map(std::vector<unsigned> &map, unsigned count, unsigned factor)
{
  map.resize(count);
  unsigned stored = 0;
  for (unsigned field = 0; field < factor; field++)
    for (unsigned pos = field; pos < count; pos += factor)
      map[stored++] = pos;
}
} // namespace

void LibRaw::jxl_dng_load_raw()
{
  if (tiff_samples < 1 || tiff_samples > 4 || !raw_width || !raw_height)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
  if (!raw_image && !image)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
  // Integer samples only; floating point DNG goes through a different pipeline
  if (tiff_bps < 1 || tiff_bps > 16 || tiff_sampleformat == 3)
    throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;

  const INT64 fsize = ifp->size();
  const INT64 mem_limit = INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024);

  // The IFD the raw data comes from: strip table and interleave factors
  const struct tiff_ifd_t *rawifd = 0;
  for (unsigned i = 0; i < tiff_nifds && i < LIBRAW_IFD_MAXCOUNT; i++)
    if (tiff_ifd[i].offset == data_offset && tiff_ifd[i].comp == 52546)
    {
      rawifd = &tiff_ifd[i];
      break;
    }
  const unsigned row_factor = rawifd && rawifd->row_interleave > 1 ? unsigned(rawifd->row_interleave) : 1;
  const unsigned col_factor = rawifd && rawifd->col_interleave > 1 ? unsigned(rawifd->col_interleave) : 1;
  if (row_factor > 8 || col_factor > 8)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  // Where the codestreams are
  std::vector<jxl_dng_tile> tiles;
  if (tile_length < INT_MAX)
  {
    if (tile_width < 1 || tile_length < 1 || tile_width > 0x10000 || tile_length > 0x10000)
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
    const unsigned tiles_x = (raw_width + tile_width - 1) / tile_width;
    const unsigned tiles_y = (raw_height + tile_length - 1) / tile_length;
    if (INT64(tiles_x) * INT64(tiles_y) > 0x10000)
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
    fseek(ifp, data_offset, SEEK_SET);
    for (unsigned ty = 0; ty < tiles_y; ty++)
      for (unsigned tx = 0; tx < tiles_x; tx++)
      {
        jxl_dng_tile t;
        t.offset = get4();
        t.size = 0;
        t.row = ty * tile_length;
        t.col = tx * tile_width;
        t.wide = tile_width;
        t.high = tile_length;
        tiles.push_back(t);
      }
  }
  else if (rawifd && rawifd->strip_offsets_count > 1 && rawifd->rows_per_strip > 0 &&
           rawifd->strip_offsets_count <= 0x10000 &&
           INT64(rawifd->strip_offsets_count) * INT64(rawifd->rows_per_strip) >= INT64(raw_height))
  {
    for (int s = 0; s < rawifd->strip_offsets_count; s++)
    {
      jxl_dng_tile t;
      t.offset = rawifd->strip_offsets[s];
      t.size = s < rawifd->strip_byte_counts_count ? rawifd->strip_byte_counts[s] : 0;
      t.row = unsigned(s) * unsigned(rawifd->rows_per_strip);
      t.col = 0;
      t.wide = raw_width;
      t.high = unsigned(rawifd->rows_per_strip);
      if (t.row < raw_height)
        tiles.push_back(t);
    }
  }
  else
  {
    jxl_dng_tile t;
    t.offset = data_offset;
    t.size = 0;
    t.row = t.col = 0;
    t.wide = raw_width;
    t.high = raw_height;
    tiles.push_back(t);
  }

  /* Interleaving is defined on the stored image. With a single tile or strip
     that is unambiguous; no file with several interleaved tiles was available
     to establish whether the factors then apply per tile or to the whole
     image, so that combination is refused rather than guessed. */
  if ((row_factor > 1 || col_factor > 1) && tiles.size() != 1)
    throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;

  // A codestream ends where the next one starts, or at the end of the file
  std::vector<INT64> sorted;
  for (size_t i = 0; i < tiles.size(); i++)
  {
    if (tiles[i].offset <= 0 || tiles[i].offset >= fsize)
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
    sorted.push_back(tiles[i].offset);
  }
  std::sort(sorted.begin(), sorted.end());
  for (size_t i = 0; i < tiles.size(); i++)
  {
    std::vector<INT64>::iterator nx = std::upper_bound(sorted.begin(), sorted.end(), tiles[i].offset);
    INT64 end = nx != sorted.end() ? *nx : fsize;
    if (tiles[i].size > 0 && tiles[i].offset + tiles[i].size < end)
      end = tiles[i].offset + tiles[i].size;
    tiles[i].size = end - tiles[i].offset;
    if (tiles[i].size <= 0 || tiles[i].size > mem_limit)
      throw LIBRAW_EXCEPTION_IO_CORRUPT;
  }

  std::vector<unsigned> rowmap, colmap;
  jxl_dng_interleave_map(rowmap, raw_height, row_factor);
  jxl_dng_interleave_map(colmap, raw_width, col_factor);

  int ss = shot_select;
  shot_select = libraw_internal_data.unpacker_data.dng_frames[LIM(ss, 0, (LIBRAW_IFD_MAXCOUNT * 2 - 1))] & 0xff;

  try
  {
    jxl_dng_decoder jxl;
    std::vector<uchar> data;
    std::vector<ushort> pixels;
#if LIBRAW_JXL_NATIVE_DEPTH
    const JxlPixelFormat format = {uint32_t(tiff_samples), JXL_TYPE_UINT16, JXL_NATIVE_ENDIAN, 0};
#else
    std::vector<float> fpixels;
    const JxlPixelFormat format = {uint32_t(tiff_samples), JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0};
#endif

    for (size_t ti = 0; ti < tiles.size(); ti++)
    {
      checkCancel();
      const jxl_dng_tile &t = tiles[ti];
      data.resize(size_t(t.size));
      fseek(ifp, t.offset, SEEK_SET);
      if (fread(data.data(), 1, data.size(), ifp) != data.size())
        throw LIBRAW_EXCEPTION_IO_EOF;

      if (!jxl.reset() || JxlDecoderSetInput(jxl.dec, data.data(), data.size()) != JXL_DEC_SUCCESS)
        throw LIBRAW_EXCEPTION_DECODE_RAW;
      JxlDecoderCloseInput(jxl.dec);

      JxlBasicInfo info;
      memset(&info, 0, sizeof(info));
      bool have_info = false, have_image = false;
      while (!have_image)
      {
        const JxlDecoderStatus status = JxlDecoderProcessInput(jxl.dec);
        if (status == JXL_DEC_BASIC_INFO)
        {
          if (JxlDecoderGetBasicInfo(jxl.dec, &info) != JXL_DEC_SUCCESS)
            throw LIBRAW_EXCEPTION_DECODE_RAW;
          // The codestream has to be what the DNG tags say it is
          if (!info.xsize || !info.ysize || info.xsize > t.wide || info.ysize > t.high ||
              info.num_color_channels + info.num_extra_channels != unsigned(tiff_samples) ||
              info.bits_per_sample < 1 || info.bits_per_sample > 16 || info.exponent_bits_per_sample)
            throw LIBRAW_EXCEPTION_IO_CORRUPT;
          if (INT64(info.xsize) * INT64(info.ysize) * INT64(tiff_samples) * 6LL > mem_limit)
            throw LIBRAW_EXCEPTION_TOOBIG;
          have_info = true;
        }
        else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER)
        {
          size_t bytes = 0;
          const size_t nsamples = size_t(info.xsize) * size_t(info.ysize) * size_t(tiff_samples);
          if (!have_info || JxlDecoderImageOutBufferSize(jxl.dec, &format, &bytes) != JXL_DEC_SUCCESS)
            throw LIBRAW_EXCEPTION_DECODE_RAW;
#if LIBRAW_JXL_NATIVE_DEPTH
          if (bytes != nsamples * sizeof(ushort))
            throw LIBRAW_EXCEPTION_DECODE_RAW;
          pixels.resize(nsamples);
          JxlBitDepth depth;
          depth.type = JXL_BIT_DEPTH_FROM_CODESTREAM;
          depth.bits_per_sample = 0;
          depth.exponent_bits_per_sample = 0;
          if (JxlDecoderSetImageOutBuffer(jxl.dec, &format, pixels.data(), bytes) != JXL_DEC_SUCCESS ||
              JxlDecoderSetImageOutBitDepth(jxl.dec, &depth) != JXL_DEC_SUCCESS)
            throw LIBRAW_EXCEPTION_DECODE_RAW;
#else
          if (bytes != nsamples * sizeof(float))
            throw LIBRAW_EXCEPTION_DECODE_RAW;
          fpixels.resize(nsamples);
          if (JxlDecoderSetImageOutBuffer(jxl.dec, &format, fpixels.data(), bytes) != JXL_DEC_SUCCESS)
            throw LIBRAW_EXCEPTION_DECODE_RAW;
#endif
        }
        else if (status == JXL_DEC_FULL_IMAGE)
          have_image = true;
        else // error, truncated input, or the stream ended without an image
          throw LIBRAW_EXCEPTION_DECODE_RAW;
      }
#if !LIBRAW_JXL_NATIVE_DEPTH
      {
        // float in 0..1 back to integer samples of the codestream's depth
        const float maxv = float((1u << info.bits_per_sample) - 1);
        pixels.resize(fpixels.size());
        for (size_t i = 0; i < fpixels.size(); i++)
        {
          const float v = fpixels[i] * maxv + 0.5f;
          pixels[i] = v > 0.f ? (v < maxv + 1.f ? ushort(v) : ushort(maxv)) : ushort(0); // NaN gives 0
        }
      }
#endif
      if (pixels.size() != size_t(info.xsize) * size_t(info.ysize) * size_t(tiff_samples))
        throw LIBRAW_EXCEPTION_DECODE_RAW;

      ushort *rp = pixels.data();
      for (unsigned row = 0; row < info.ysize; row++)
      {
        const unsigned srow = t.row + row;
        if (srow >= raw_height)
          break;
        const unsigned drow = rowmap[srow];
        for (unsigned col = 0; col < info.xsize; col++)
        {
          const unsigned scol = t.col + col;
          if (scol < raw_width)
            adobe_copy_pixel(drow, colmap[scol], &rp);
          else
            rp += tiff_samples;
        }
      }
    }
  }
  catch (const std::bad_alloc &)
  {
    shot_select = ss;
    throw LIBRAW_EXCEPTION_ALLOC;
  }
  catch (...)
  {
    shot_select = ss;
    throw;
  }
  shot_select = ss;
}

#endif
