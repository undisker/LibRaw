# Licensing and provenance of this fork

What is in this repository, where each part came from, under which licence it
may be shipped, and which questions are still open. Written for the case this
fork exists for: a commercial, closed-source application (the host application) that
ships the library as a separate shared library.

This is an engineering record, not legal advice. The points marked **[lawyer]**
are the ones worth a lawyer's hour before a release.

---

## 1. What the library is made of

| Part | Origin | Licence |
|---|---|---|
| LibRaw itself | upstream `LibRaw/LibRaw`, merged through `70513b5` (snapshot 202611) | LGPL-2.1 **or** CDDL-1.0, at the recipient's choice |
| dcraw code inside LibRaw | Dave Coffin, the non-"RESTRICTED" parts only (upstream's statement in `COPYRIGHT`) | as LibRaw |
| DCB demosaic, FBDD denoise | Jacek Gozdz | BSD 3-clause |
| X3F (Foveon) reader | Roland Karlsson | BSD-style |
| Pieces of Adobe DNG SDK 1.4 | Adobe, as upstream uses them | MIT, per upstream's `COPYRIGHT` |
| Checked arithmetic, OpenMP work, CMake build, C API additions, headerless camera entries, ARRIRAW reader, `packed_lsbfirst_load_raw` | written for this fork (see `git log upstream/master..`) | offered under LibRaw's two licences |
| Sony lossless ARW tile decoder (`sony_ljpeg_load_raw_tiles`, `internal/losslessjpeg_lut.h`) | upstream pull request #886 by its author, not merged upstream when taken | LibRaw's two licences - upstream's `CONTRIBUTING.md`: "By submitting a patch or Pull Request, you explicitly agree that your contributed code can be distributed under both" |
| GoPro GPR, default: GoPro's VC-5 decoder | `gopro/gpr` at a pinned commit, directories `vc5_decoder`, `vc5_common`, `common`, compiled in **unmodified**; called from `src/decoders/vc5_opensdk.cpp` (this fork's glue, no GoPro code in it) | GoPro code: Apache-2.0 **or** MIT. See section 3 |
| GoPro GPR, optional: this fork's decoder, `src/decoders/vc5.cpp` | rewritten for this fork from the same GoPro decoder; **requires legal clarification**, off by default | as above for the GoPro-derived parts. See section 3 |
| JPEG XL DNG loader, `src/decoders/jxl_dng.cpp` | written for this fork; calls libjxl | LibRaw's two licences |

Linked at build time, shipped as separate shared libraries
(`cmake/build-deps.cmake` pins the versions):

| Library | Licence | Needed for |
|---|---|---|
| zlib-ng | zlib | Deflate DNG |
| libjpeg-turbo | IJG + BSD 3-clause + zlib | lossy DNG, Kodak JPEG raw |
| libjxl | BSD 3-clause (with Google's additional patent grant, `PATENTS`) | JPEG XL DNG |
| highway (inside libjxl) | Apache-2.0 or BSD 3-clause | |
| brotli (inside libjxl) | MIT | |
| Little CMS 2 (inside libjxl) | MIT | |

Not used, deliberately: the Adobe DNG SDK, the GoPro GPR SDK, RawSpeed. The
reasons are in `BINARIES.md` section 5; the short version for the DNG SDK is an
uncapped indemnity to Adobe on commercial distribution and a monthly stream of
security releases.

No sample images are stored in the repository. The test corpus
(raw.pixls.us) is CC0 and is downloaded when a test run needs it.

### Provenance checks that were actually run

- `vc5.cpp` against rawspeed's `VC5Decompressor` (LGPL): of 776 significant
  source lines one is shared, and it is `#pragma omp for schedule(static)`.
  No rawspeed code was used; rawspeed was not needed for the port.
- `vc5.cpp` against GoPro's sources: no line is copied verbatim; identifiers
  (tag names, table names) and two data tables follow GoPro's code, which is
  what "derived from" means here.
- The headerless camera entries are measurements of the sample files (file
  size, row stride by autocorrelation, CFA phase by rendering) - facts, not
  code taken from elsewhere.

### A note on authorship

Much of the fork-only code was written with an AI coding assistant (the
`Co-Authored-By` lines in `git log` say which commits). That introduces no
third-party licence. Whether AI-assisted code is itself protected by copyright
varies by jurisdiction and is unsettled; it matters only if the fork's own code
ever had to be *enforced* against someone, not for the right to ship it.

---

## 2. Shipping LibRaw inside a commercial product

The recipient of LibRaw picks one of the two licences. Both allow this use; the
obligations differ.

**LGPL-2.1** (what `BINARIES.md` assumes):

- The library must stay replaceable by the user. Shipping it as a separate
  `raw.dll` / `libraw.dylib` / `libraw.so`, loaded at run time, is the standard
  way to meet that.
- The source of the library *as shipped* - this fork, at the tagged commit the
  binary was built from - must be available to recipients. The fork is a public
  repository, which covers it; the release must name the tag.
- Ship the LGPL text and say prominently that the product uses LibRaw.
- Section 6 requires that the product's own licence terms permit "modification
  of the work for the customer's own use and reverse engineering for debugging
  such modifications". A EULA with a blanket no-reverse-engineering clause needs
  a carve-out for this. **[lawyer]**

**CDDL-1.0** (the alternative):

- File-level copyleft: the source of the LibRaw files, modified or not, must be
  available; the executable may be distributed under different terms, and
  section 3.6 explicitly allows a "Larger Work" combining covered code with
  code under other licences.
- No relinking requirement and no reverse-engineering clause.
- It also sidesteps the question of Apache-2.0 code inside an LGPL-2.1 library
  (section 3).

Either way, the attribution list of the product must name LibRaw, the parts in
the first table with their own notices (DCB/FBDD, X3F, Adobe DNG SDK pieces),
every linked library, and - when the GPR decoder is built in - GoPro.

---

## 3. GoPro GPR (VC-5): copyright is clear, patents are not

### Copyright

GoPro publishes its decoder under "Apache License 2.0 or MIT, at your option".
Both decoders are that code or derive from it: the default build compiles
GoPro's files as they are, and `vc5.cpp` is a derivative that carries GoPro's
notice.

For `vc5.cpp` the file header records which licence is relied on: **Apache-2.0**, because it
is the one that contains a patent licence (section 3 of the licence); MIT is not
given up. Apache-2.0 section 4 then requires: give recipients the licence text,
keep GoPro's notices, and state prominently that the file was changed - the
header does. `gopro/gpr` has no `NOTICE` file, so there is nothing further to
propagate.

Open point: the FSF regards Apache-2.0 as incompatible with GPLv2, and
LGPL-2.1 has the same "no further restrictions" rule. rawspeed (LGPL) includes
GoPro's dual-licensed table in exactly this way, and the dual offer lets a
recipient take the copyright permission from MIT, but nobody has ruled on it.
Taking LibRaw under CDDL instead removes the question. **[lawyer]**

### Patents

What was found (November 2026). Items marked *unverified* come from search
result excerpts only - the hosts themselves could not be reached - and must be
re-read at the source before anyone relies on them.

- **GoPro holds patents on exactly this.** The family "Compression and decoding
  of single sensor color image data" (inventor David Newman; CineForm, then
  Woodman Labs, now GoPro) was filed on 2007-03-22. Read on Google Patents on
  2026-11-18 (not the patent office's own register, and US only - foreign
  counterparts were not checked):

  | Patent | Granted | Status | Expires |
  |---|---|---|---|
  | US 8,014,597 (parent) | 2011-09-06 | Active | **2030-04-07** (term adjusted) |
  | US 8,345,969 | 2013-01-01 | Expired, fees not paid | - |
  | US 8,538,143 | 2013-09-17 | Expired, fees not paid | - |
  | US 8,644,629 | 2014-02-04 | Expired, fees not paid | - |
  | US 8,718,390 | 2014-05-06 | Expired, fees not paid | - |
  | US 8,879,861 | 2014-11-04 | Active | 2027-03-22 |
  | US 9,025,896 | 2015-05-05 | Active | 2027-03-22 |
  | US 9,438,799 | 2016-09-06 | Active | 2027-03-22 |
  | US 9,736,384 | 2017-08-15 | Expired, fees not paid | - |
  | US 10,212,352 | 2019-02-19 | Active | 2027-03-22 |
  | US 10,574,898 | 2020-02-25 | Active | 2027-03-22 |
  | US 11,297,337 | 2022-04-05 | Active | 2027-07-15 (term adjusted) |

  The last to expire is the **parent, in April 2030**, not 2027. Its claim 12
  is on the decoding side: an apparatus that retrieves data compressed by
  separating the interleaved colour primaries into independent channels and
  encoding each, decodes the channels, and combines them "into the full color
  display memory". Claim 22 of US 11,297,337 covers instructions that
  reconstruct the per-colour image portions "based on at least one color
  differencing operation" and generate a reduced-resolution image. Whether a
  decoder that only reproduces the Bayer mosaic falls under either is a
  claim-construction question. **[lawyer]**
- **SMPTE.** VC-5 is SMPTE ST 2073. SMPTE's policy allows declarations on RAND
  terms, not only royalty-free ones. A patent statement by GoPro for VC-5 is
  listed at SMPTE (2012); its content could not be read. *Unverified.* Nothing
  official calls VC-5 royalty-free.
- **GoPro's own account** (David Newman, comments under his 2017 post on
  open-sourcing CineForm, *unverified*): the non-raw modes are "completely
  patent free"; "the Apache 2.0 license grants patent rights including RAW" for
  software using the SDK; hardware and "custom implementations" of raw are
  pointed to VC-5 licensing.
- **The Apache-2.0 patent licence** covers claims "necessarily infringed by
  their Contribution(s) alone or by combination of their Contribution(s) with
  the Work". The licence text speaks of "the Work"; whether that follows the
  code into a rewritten derivative is the mainstream reading but is not spelled
  out. **[lawyer]**
- **Practice.** darktable has shipped rawspeed's own VC-5 decoder since 2018
  with no reported complaint. Adobe decodes GPR and is a GoPro licensee.
  ImageMagick does not read GPR at all (its raw path is LibRaw). Fedora's
  FFmpeg build does not enable the CineForm *video* decoder, which means "not
  cleared", not "found encumbered". No patent pool includes VC-5 and no
  litigation was found.

### Copyright licence is not patent licence

MIT and Apache-2.0 both let anyone copy GoPro's code into a product - that is
the *copyright* permission and it is not in doubt. A patent is a separate
right: it covers the *method*, whoever wrote the code that performs it. MIT
says nothing about patents. Apache-2.0 does give a patent licence, but for
"their Contribution(s)" - GoPro's code as GoPro contributed it. So:

- running **GoPro's own code** is the case the grant is written for, and it is
  what GoPro's own engineer describes as covered ("direct using of the
  ... SDK software");
- a **rewrite** performs the same patented method with different code. Whether
  the grant follows the method into new code is the open question.

### The two decoders

| | GoPro's decoder | this fork's decoder |
|---|---|---|
| CMake option | `LIBRAW_ENABLE_GPR_SDK`, **ON** | `LIBRAW_ENABLE_GPR`, OFF |
| Macro | `USE_GPR_OPENSDK` | `USE_GPR_VC5` |
| `libraw_capabilities()` | bit 22 | bit 21 |
| Code | GoPro's files, unmodified | `vc5.cpp`, a rewrite |
| Patent position | the use Apache-2.0 section 3 is written for | **requires legal clarification** |
| Output | identical on all 17 corpus files | identical |
| Speed, 4000x3000, 4 cores | about 390 ms | about 50 ms |
| Hostile input | **memory errors, see below** | none found in 7280 corrupted files |

If both options are on, GoPro's decoder is the one compiled in.

Not to be confused with upstream's `USE_GPRSDK`: that route goes through
GoPro's `gpr_read_image.cpp` and needs the Adobe DNG SDK. Neither is used
here; the copy of the Adobe DNG SDK that sits in GoPro's repository is not
compiled.

### Robustness of GoPro's decoder - read before shipping

GoPro's decoder trusts its input. 687 corrupted copies of three small GPR
files, run through it under AddressSanitizer and UBSan (November 2026, commit
`446c736`):

| Result | Runs |
|---|---|
| heap buffer overflow, **write**, in `DecodeBandRuns` (decoder.c) | 6 |
| heap buffer overflow, read, in `GetByte` (stream.c) | 1 |
| global buffer overflow, read, in `SubbandWaveletIndex` (wavelet.c) | 1 |
| stack use after return in `AllocateChannelWavelets` (decoder.c) | 1 |
| memory leaked on an error path (up to about 2 MB per file) | 355 |

A valid file also triggers undefined behaviour of the harmless kind (left
shift of a negative value in raw.c). GoPro's issue tracker has had a report of
a crash on a malformed file open since January 2022.

The glue checks what it can without GoPro's code - stream size against the DNG
size, memory budget, zero padding after the stream so that a read past its end
stays in the buffer - but it cannot stop the write overflow, which happens
inside the entropy decoder. **For a product that opens files of unknown origin
this is a real exposure**, and it is the price of the cleaner patent position.
Ways to reduce it, none implemented yet:

1. Validate the stream first with this fork's bounded parser (structure and
   entropy coding, no image reconstruction) and hand GoPro's decoder only
   streams that pass. Fast, and it stops the overflows found; but it runs part
   of the rewrite, which brings a weaker form of the patent question back.
2. Carry a small hardening patch for GoPro's files (bounds checks at the four
   places above, frees on the error paths). Apache-2.0 allows modification;
   the code is then GoPro's with a patch rather than "unmodified".
3. Report the problems upstream and wait - the repository gets a commit a
   year.
4. Ship without GPR support (`-DLIBRAW_ENABLE_GPR_SDK=OFF`).

### What that means

There is a real patent question; the last patent runs to April 2030, and it
is the same for every way of decoding GPR. The default build answers it the
conservative way - GoPro's own code under GoPro's own licence - and pays for
that in speed and in robustness against hostile files. The fork's faster,
hardened decoder is there for the day the patent question is answered:

1. Ask GoPro in writing whether a decoder derived from `gopro/gpr` under
   Apache-2.0 is covered. An answer either way settles it.
2. Have a patent attorney read the live claims of the 8,014,597 family against
   a decode-only implementation, and check counterparts in the countries the
   product is sold in. **[lawyer]**
3. Wait for expiry: six of the seven active members end in March-July 2027,
   the parent in April 2030.

---

## 4. JPEG XL

libjxl is BSD 3-clause and ships a `PATENTS` file (read in the pinned tag,
v0.11.1): Google grants "a perpetual, worldwide, non-exclusive, no-charge,
royalty-free, irrevocable ... patent license" for its implementation. That the
format as a whole is royalty-free is the project's stated position and is
repeated here from general knowledge, not from a document read for this file.

---

## 5. Checklist for a release

- [ ] Built from a tagged commit of this fork; the tag is named in the product.
- [ ] Licence chosen for LibRaw (LGPL-2.1 or CDDL-1.0) and its text shipped.
- [ ] If LGPL: the EULA permits modification of the library and reverse
      engineering for debugging such modifications.
- [ ] Attribution list names: LibRaw; dcraw (Dave Coffin); DCB/FBDD (Jacek
      Gozdz); X3F tools (Roland Karlsson); Adobe DNG SDK 1.4 portions; zlib-ng;
      libjpeg-turbo (IJG notice: "this software is based in part on the work of
      the Independent JPEG Group"); libjxl, highway, brotli, Little CMS.
- [ ] GPR decided: GoPro's decoder (default), none, or - only after legal
      clarification - this fork's. With either decoder, GoPro's copyright
      notice and the Apache-2.0 text are in the attribution list.
- [ ] If GoPro's decoder ships: the robustness note in section 3 has been
      read and one of its mitigations chosen.
- [ ] No Adobe DNG SDK, GoPro GPR SDK or RawSpeed in the build
      (`libraw_capabilities()` bits 0, 1, 2, 8 are clear).
