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
| GoPro GPR decoder, `src/decoders/vc5.cpp` | rewritten for this fork from GoPro's reference decoder, `gopro/gpr` (`vc5_decoder`, `vc5_common`) | GoPro code: Apache-2.0 **or** MIT. See section 3 |
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
`vc5.cpp` is a derivative of that code and carries GoPro's notice.

The file header records which licence is relied on: **Apache-2.0**, because it
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

### What that means

There is a real, time-limited patent question, and it is the same for every
way of decoding GPR - GoPro's SDK, rawspeed, or this file. The argument for
being licensed is that `vc5.cpp` is a derivative of code GoPro itself released
under Apache-2.0 with a patent grant. The argument against is that it is a
rewrite, which GoPro might call a "custom implementation".

So the decoder is a build option and it is **off by default**:

```sh
cmake ... -DLIBRAW_ENABLE_GPR=ON     # USE_GPR_VC5; libraw_capabilities() bit 21
```

Ways to close the question, cheapest first:

1. Ask GoPro in writing whether a decoder derived from `gopro/gpr` under
   Apache-2.0 is covered. An answer either way settles it.
2. Have a patent attorney read the live claims of the 8,014,597 family against
   a decode-only implementation, and check counterparts in the countries the
   product is sold in. **[lawyer]**
3. Leave the option off until the family has expired. Seven members are
   active; six of them end in March-July 2027, the parent in April 2030.

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
- [ ] `LIBRAW_ENABLE_GPR` decided. If ON: GoPro's notice and the Apache-2.0
      text are in the attribution list.
- [ ] No Adobe DNG SDK, GoPro GPR SDK or RawSpeed in the build
      (`libraw_capabilities()` bits 0, 1, 2, 8 are clear).
