# Task: ask GoPro about the patent licence for the fork's VC-5 decoder

**Status:** open - not sent yet.

**Goal:** a written answer from GoPro on whether the patent licence in
section 3 of the Apache License 2.0, under which GoPro publishes the GPR SDK,
covers `src/decoders/vc5.cpp` - a decoder derived from GoPro's
`vc5_decoder` / `vc5_common` and rewritten in C++ for LibRaw.

**Why it matters:** with a "yes", the fork's own decoder can replace GoPro's
patched one as the default (`-DLIBRAW_ENABLE_GPR=ON`,
`-DLIBRAW_ENABLE_GPR_SDK=OFF`): same output, 5-8 times faster, multithreaded.
Background: `LICENSING.md`, section 3.

**Scope of the question:** the LibRaw fork only, as an open-source project.
Keep any downstream product out of the letter. Note that the fork's public
documentation (`README.md`, `BINARIES.md`, `LICENSING.md`) currently names the
application it is built for; decide before sending whether that should stay.

## Where to send it

No licensing or legal contact for the GPR SDK was found in the repository.
Candidates, most direct first - check each is current before using it:

1. An issue in `github.com/gopro/gpr` - public, and the repository answers
   rarely (several issues have had no reply for years).
2. GoPro's general legal / intellectual property contact on gopro.com.
3. GoPro's open-source contact, if the repository's `CONTRIBUTING.md` or
   `AUTHORS.md` names one.

A public issue puts the patent question on record; a private letter does not.
For a definitive answer, the legal contact is the better addressee.

## What counts as an answer

- **Yes, covered:** switch the defaults, record the answer (date, sender,
  wording) in `LICENSING.md` section 3, keep the letter.
- **No, or a licence is needed:** keep GoPro's patched decoder as the default
  and record the answer.
- **No reply:** nothing changes. Revisit when the patents lapse - six of the
  seven active US patents end between March and July 2027, the last
  (US 8,014,597) on 2030-04-07.

## Draft

> **Subject:** Patent licence scope for a decoder derived from the GPR SDK
> (Apache-2.0, section 3)
>
> Dear GoPro team,
>
> I maintain an open-source fork of LibRaw, the raw image decoding library
> (https://github.com/undisker/LibRaw, LGPL-2.1 / CDDL-1.0). The fork reads
> GoPro GPR files in two ways:
>
> 1. by compiling the VC-5 decoder of your GPR SDK (`source/lib/vc5_decoder`,
>    `vc5_common`, `common`, commit 446c736) into the library, with a small
>    patch that only adds bounds checks for damaged files; and
> 2. by a decoder of its own, `src/decoders/vc5.cpp`, which is derived from
>    the same GPR SDK code and rewritten in C++ (bounded readers,
>    multithreading). It decodes raw Bayer data only, with no encoding, and
>    its output is bit-identical to your reference decoder.
>
> The GPR SDK is licensed under "Apache License 2.0 or MIT, at your option",
> and the fork takes it under Apache-2.0. Section 3 of that licence grants a
> patent licence for "the Work". I would like to confirm how GoPro reads it:
>
> - Does the patent licence of section 3 cover the second decoder - a
>   derivative work of the GPR SDK that decodes VC-5 raw data - as it covers
>   the SDK code itself?
> - If not, is a separate licence needed for a decode-only software
>   implementation distributed as open source, and on what terms?
>
> A short written confirmation either way would let the project choose the
> right default. Thank you for publishing the SDK.
>
> Kind regards,
> [name]
> [contact]
