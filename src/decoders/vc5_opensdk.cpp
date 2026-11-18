/* -*- C++ -*-
 * File: vc5_opensdk.cpp
 * Written for the undisker LibRaw fork, 2026; not part of upstream LibRaw.
 *
   GoPro GPR (VC-5 in DNG, TIFF Compression=9) decoded by GoPro's own
   open-source decoder: the vc5_decoder and vc5_common libraries of the GPR
   SDK (https://github.com/gopro/gpr), compiled into the library with one
   patch that adds bounds checks (GoPro/gpr-vc5-hardening.patch) and no other
   change.

   Built when USE_GPR_OPENSDK is defined (CMake: LIBRAW_ENABLE_GPR_SDK, the
   default). This file contains no GoPro code; it only finds the VC-5 stream
   in the DNG, calls vc5_decoder_process() and stores the result.

   This is not upstream's USE_GPRSDK route: that one goes through GoPro's
   gpr_read_image.cpp and needs the Adobe DNG SDK. Nothing of Adobe's is used
   here.

   Why GoPro's code rather than this fork's own decoder (vc5.cpp): GoPro
   licenses these sources under Apache-2.0 (or MIT), and the patent licence in
   Apache-2.0 section 3 is given for GoPro's contribution as GoPro published
   it. Running that code is the use the grant is written for. See
   LICENSING.md, section 3.

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#ifdef USE_GPR_OPENSDK
/* GoPro's headers first: LibRaw's internal headers define macros named
   width, height, curve, ... that would rewrite their declarations. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
extern "C"
{
#include "vc5_decoder.h"
}
#endif

#include "../../internal/dcraw_defs.h"

#ifdef USE_GPR_OPENSDK

#include <vector>
#include <stdlib.h>

namespace
{
/* GoPro's decoder allocates through two plain callbacks. They are routed
   through a per-call ledger, for two things the decoder does not do itself:
   everything it leaves allocated when it stops on an error is released
   afterwards, and the total it may hold is limited. The callbacks carry no
   context argument, hence the thread-local pointer. */
struct gpr_opensdk_ledger
{
  struct block
  {
    void *ptr;
    size_t size;
  };
  std::vector<block> blocks;
  size_t held, limit;
  gpr_opensdk_ledger(size_t aLimit) : held(0), limit(aLimit) {}
  ~gpr_opensdk_ledger()
  {
    for (size_t i = 0; i < blocks.size(); i++)
      ::free(blocks[i].ptr);
  }
  void *take(size_t size)
  {
    if (size > limit || held > limit - size)
      return NULL;
    void *p = ::malloc(size ? size : 1);
    if (!p)
      return NULL;
    try
    {
      block b = {p, size};
      blocks.push_back(b);
    }
    catch (...)
    {
      ::free(p);
      return NULL;
    }
    held += size;
    return p;
  }
  // Remove a block from the ledger; false if it is not one of ours
  bool forget(void *p)
  {
    for (size_t i = blocks.size(); i-- > 0;)
      if (blocks[i].ptr == p)
      {
        held -= blocks[i].size;
        blocks.erase(blocks.begin() + i);
        return true;
      }
    return false;
  }
};

#if defined(_MSC_VER)
__declspec(thread) gpr_opensdk_ledger *gpr_opensdk_current = NULL;
#else
__thread gpr_opensdk_ledger *gpr_opensdk_current = NULL;
#endif

void *gpr_opensdk_alloc(size_t size) { return gpr_opensdk_current ? gpr_opensdk_current->take(size) : NULL; }
void gpr_opensdk_free(void *p)
{
  // Only what the ledger handed out is freed, and only once
  if (p && gpr_opensdk_current && gpr_opensdk_current->forget(p))
    ::free(p);
}

inline unsigned gpr_be16(const uchar *p) { return (unsigned(p[0]) << 8) | p[1]; }

/* Image size announced by the VC-5 stream, read without GoPro's code.

   A VC-5 stream is a start marker followed by big-endian (tag, value) pairs
   of 16 bits each; a negative tag is the optional form of the same tag.
   GoPro's decoder sizes its buffers from ImageWidth (20) and ImageHeight (21)
   alone, so they are checked against the DNG before it is given the stream.
   Chunk payloads are skipped by their length, as the syntax defines; the
   scan stops at the first codeblock, which follows the header. */
bool gpr_opensdk_stream_size(const uchar *data, size_t size, unsigned &wide, unsigned &high)
{
  if (size < 8 || data[0] != 'V' || data[1] != 'C' || data[2] != '-' || data[3] != '5')
    return false;
  bool have_w = false, have_h = false;
  size_t pos = 4;
  for (int segments = 0; segments < 4096 && pos + 4 <= size; segments++)
  {
    int tag = int(short(gpr_be16(data + pos)));
    const unsigned value = gpr_be16(data + pos + 2);
    pos += 4;
    if (tag < 0)
      tag = -tag;
    if (tag & 0x2000) // large chunk: 24-bit length in 32-bit words
    {
      if ((tag & 0x6000) == 0x6000) // a codeblock: the header is over
        break;
      const size_t words = (size_t(tag & 0xff) << 16) | value;
      if (words > (size - pos) / 4)
        return false;
      pos += words * 4;
    }
    else if (tag & 0x4000) // small chunk: 16-bit length in 32-bit words
    {
      if (size_t(value) > (size - pos) / 4)
        return false;
      pos += size_t(value) * 4;
    }
    else if (tag == 20)
    {
      wide = value;
      have_w = true;
    }
    else if (tag == 21)
    {
      high = value;
      have_h = true;
    }
    if (have_w && have_h)
      return true;
  }
  return false;
}
} // namespace

void LibRaw::vc5_dng_load_raw()
{
  if (!dng_version || tiff_samples != 1)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
  /* Bayer data only: unpack() allocates raw_image for CFA images */
  if (!raw_image || !filters || !raw_width || !raw_height)
    throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;
  /* GPR stores the whole image as one VC-5 stream in a single tile (or strip) */
  if (tile_width < INT_MAX || tile_length < INT_MAX)
    throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;

  const INT64 memory_limit = INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024);
  const INT64 fsize = ifp->size();
  const INT64 offset = data_offset;
  if (offset < 0 || offset >= fsize)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
  INT64 csize = fsize - offset;
  if (data_size > 0 && INT64(data_size) < csize)
    csize = INT64(data_size);
  if (csize < 8)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
  // GoPro's decoder works in buffers of roughly four times the raw image
  const INT64 raw_bytes = INT64(raw_width) * INT64(raw_height) * INT64(sizeof(ushort));
  if (csize > memory_limit || raw_bytes * 4 > memory_limit)
    throw LIBRAW_EXCEPTION_TOOBIG;

  /* The stream, followed by zero padding: GoPro's bit reader does not check
     the end of its input, and the padding keeps a read past the end of a
     damaged stream inside this buffer. This is a second line of defence;
     the first is the hardening patch the SDK sources are built with
     (GoPro/gpr-vc5-hardening.patch, LICENSING.md section 3). */
  const size_t padding = 1 << 16;
  std::vector<uchar> stream;
  try
  {
    stream.resize(size_t(csize) + padding);
  }
  catch (const std::bad_alloc &)
  {
    throw LIBRAW_EXCEPTION_ALLOC;
  }
  fseek(ifp, offset, SEEK_SET);
  if (INT64(fread(stream.data(), 1, size_t(csize), ifp)) != csize)
    throw LIBRAW_EXCEPTION_IO_EOF;

  unsigned swide = 0, shigh = 0;
  if (!gpr_opensdk_stream_size(stream.data(), size_t(csize), swide, shigh) || swide != raw_width ||
      shigh != raw_height)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  /* As GoPro's own DNG reader does: 14 bit data for RGGB sensors, 12 bit for
     the others, which it takes to be GBRG. (LibRaw may number the second
     green as colour 3.) */
  const bool rggb = FC(0, 0) == 0 && (FC(0, 1) & 1) && (FC(1, 0) & 1) && FC(1, 1) == 2;

  vc5_decoder_parameters params;
  vc5_decoder_parameters_set_default(&params);
  params.mem_alloc = gpr_opensdk_alloc;
  params.mem_free = gpr_opensdk_free;
  params.pixel_format = rggb ? VC5_DECODER_PIXEL_FORMAT_RGGB_14 : VC5_DECODER_PIXEL_FORMAT_GBRG_12;

  gpr_buffer in = {stream.data(), size_t(csize)};
  gpr_buffer out = {NULL, 0};
  checkCancel();

  /* Everything from here to the end of the block is plain C code and
     non-throwing C++; the ledger releases whatever the decoder still holds
     when it goes out of scope. */
  bool decoded = false;
  {
    const size_t ledger_limit = size_t(memory_limit);
    gpr_opensdk_ledger ledger(ledger_limit);
    gpr_opensdk_current = &ledger;
    const CODEC_ERROR err = vc5_decoder_process(&params, &in, &out, NULL);
    gpr_opensdk_current = NULL;

    // The decoder writes complete 2x2 cells only
    const size_t dwide = raw_width & ~1u, dhigh = raw_height & ~1u;
    /* The result must be a block the decoder obtained through the ledger
       and large enough for the image; anything else is not trusted. */
    size_t out_block = 0;
    for (size_t i = 0; i < ledger.blocks.size(); i++)
      if (ledger.blocks[i].ptr == out.buffer)
        out_block = ledger.blocks[i].size;
    if (err == CODEC_ERROR_OKAY && out.buffer && dwide && dhigh && out.size <= out_block &&
        out.size >= dwide * dhigh * sizeof(ushort))
    {
      const size_t stride = out.size / sizeof(ushort) / dhigh >= size_t(raw_width) ? size_t(raw_width) : dwide;
      const ushort *src = (const ushort *)out.buffer;
      for (unsigned row = 0; row < raw_height; row++)
        for (unsigned col = 0; col < raw_width; col++)
          RAW(row, col) = (row < dhigh && col < dwide) ? curve[src[size_t(row) * stride + col]] : 0;
      decoded = true;
    }
  }
  if (!decoded)
    throw LIBRAW_EXCEPTION_IO_CORRUPT;
}

#endif /* USE_GPR_OPENSDK */
