/* -*- C++ -*-
 * File: batch_verify.cpp
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 *
 * LibRaw sample: batch-verify a folder of RAW files.
 *
 * Goal: make sure every supported-format sample in a directory opens
 * "consistently and fast":
 *   - consistently: each file decodes without error, and a second decode
 *     produces a byte-identical image (catches nondeterminism / data races,
 *     e.g. from the OpenMP postprocessing paths);
 *   - fast: per-file unpack/postprocess timing and Mpix/sec, with slow
 *     outliers flagged, grouped by detected format.
 *
 * Exit code is non-zero if any file fails to open/decode or is
 * non-deterministic, so it can be used in CI.
 *
LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "libraw/libraw.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#endif

// ---- portable millisecond timer -------------------------------------------
#ifdef _WIN32
static double now_ms()
{
  LARGE_INTEGER t, f;
  QueryPerformanceCounter(&t);
  QueryPerformanceFrequency(&f);
  return 1000.0 * (double)t.QuadPart / (double)f.QuadPart;
}
#else
static double now_ms()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}
#endif

// ---- portable directory scan ----------------------------------------------
static bool has_known_ext(const std::string &name)
{
  // Empty filter == accept everything; LibRaw decides what it can actually
  // open. This list just skips obvious non-RAW companions in sample folders.
  static const char *skip[] = {".txt", ".md",  ".json", ".xml", ".jpg",
                               ".jpeg", ".png", ".html", ".pdf", ".db",
                               ".ini", ".exe", ".dll",  ".lib", 0};
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  for (int i = 0; skip[i]; i++)
  {
    size_t L = strlen(skip[i]);
    if (lower.size() >= L && lower.compare(lower.size() - L, L, skip[i]) == 0)
      return false;
  }
  return true;
}

static void collect(const std::string &dir, bool recurse,
                    std::vector<std::string> &out)
{
#ifdef _WIN32
  std::string pat = dir + "\\*";
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pat.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE)
    return;
  do
  {
    std::string nm = fd.cFileName;
    if (nm == "." || nm == "..")
      continue;
    std::string full = dir + "\\" + nm;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
      if (recurse)
        collect(full, recurse, out);
    }
    else if (has_known_ext(nm))
      out.push_back(full);
  } while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR *d = opendir(dir.c_str());
  if (!d)
    return;
  struct dirent *e;
  while ((e = readdir(d)) != NULL)
  {
    std::string nm = e->d_name;
    if (nm == "." || nm == "..")
      continue;
    std::string full = dir + "/" + nm;
    struct stat st;
    if (stat(full.c_str(), &st) != 0)
      continue;
    if (S_ISDIR(st.st_mode))
    {
      if (recurse)
        collect(full, recurse, out);
    }
    else if (S_ISREG(st.st_mode) && has_known_ext(nm))
      out.push_back(full);
  }
  closedir(d);
#endif
}

// FNV-1a over the processed image bytes: a cheap content fingerprint used to
// confirm two independent decodes of the same file are byte-identical.
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

struct Result
{
  std::string file, fmt, camera;
  bool ok;
  bool deterministic;
  double unpack_ms, process_ms, mpix;
  std::string error;
};

// Decode one file once; fill checksum/dims. Returns LIBRAW_SUCCESS or error.
static int decode_once(const char *file, unsigned long long &checksum,
                       double &unpack_ms, double &process_ms, double &mpix,
                       std::string &fmt, std::string &camera, int &err)
{
  LibRaw R;
  int ret;
  if ((ret = R.open_file(file)) != LIBRAW_SUCCESS)
  {
    err = ret;
    return ret;
  }
  fmt = R.imgdata.idata.normalized_make[0]
            ? std::string(R.imgdata.idata.normalized_make) + " " +
                  R.imgdata.idata.normalized_model
            : "unknown";
  camera = std::string(R.imgdata.idata.make) + " " + R.imgdata.idata.model;

  double t0 = now_ms();
  if ((ret = R.unpack()) != LIBRAW_SUCCESS)
  {
    err = ret;
    return ret;
  }
  unpack_ms = now_ms() - t0;

  t0 = now_ms();
  if ((ret = R.dcraw_process()) != LIBRAW_SUCCESS)
  {
    err = ret;
    return ret;
  }
  process_ms = now_ms() - t0;

  libraw_processed_image_t *img = R.dcraw_make_mem_image(&ret);
  if (!img)
  {
    err = ret;
    return ret ? ret : LIBRAW_UNSPECIFIED_ERROR;
  }
  checksum = fnv1a((const unsigned char *)img->data, img->data_size);
  mpix = (img->width * (double)img->height) / 1e6;
  R.dcraw_clear_mem(img);
  return LIBRAW_SUCCESS;
}

int main(int argc, char **argv)
{
  bool recurse = false;
  double slow_threshold = 0.0; // msec; 0 => auto (3x median)
  std::vector<std::string> roots;

  for (int i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i], "-r"))
      recurse = true;
    else if (!strcmp(argv[i], "-t") && i + 1 < argc)
      slow_threshold = atof(argv[++i]);
    else
      roots.push_back(argv[i]);
  }

  if (roots.empty())
  {
    printf("LibRaw %s batch verifier (%d cameras supported)\n",
           LibRaw::version(), LibRaw::cameraCount());
    printf("Opens every file in a folder, checks each decodes consistently "
           "(twice, byte-identical) and reports speed.\n");
    printf("Usage: %s [-r] [-t slow_msec] <folder|file> [...]\n", argv[0]);
    printf("  -r          recurse into subdirectories\n");
    printf("  -t <msec>   flag files whose postprocess exceeds this "
           "(default: 3x median)\n");
    return 2;
  }

  std::vector<std::string> files;
  for (size_t i = 0; i < roots.size(); i++)
  {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(roots[i].c_str());
    bool isdir = (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    bool isdir = (stat(roots[i].c_str(), &st) == 0) && S_ISDIR(st.st_mode);
#endif
    if (isdir)
      collect(roots[i], recurse, files);
    else
      files.push_back(roots[i]);
  }
  std::sort(files.begin(), files.end());

  if (files.empty())
  {
    fprintf(stderr, "No files found.\n");
    return 2;
  }

  printf("Verifying %zu file(s)...\n\n", files.size());
  printf("%-40s %-22s %8s %9s %8s  %s\n", "FILE", "FORMAT", "unpack",
         "process", "Mpix/s", "STATUS");

  std::vector<Result> results;
  int failures = 0, nondet = 0;
  for (size_t i = 0; i < files.size(); i++)
  {
    Result r;
    r.file = files[i];
    r.ok = false;
    r.deterministic = true;
    r.unpack_ms = r.process_ms = r.mpix = 0;

    const char *base = r.file.c_str();
    for (const char *p = base; *p; p++)
      if (*p == '/' || *p == '\\')
        base = p + 1;

    unsigned long long c1 = 0, c2 = 0;
    int err = 0;
    double u1, p1, mp;
    std::string fmt, cam;
    int ret = decode_once(r.file.c_str(), c1, u1, p1, mp, fmt, cam, err);
    if (ret != LIBRAW_SUCCESS)
    {
      r.error = libraw_strerror(err);
      r.fmt = "-";
      printf("%-40.40s %-22s %8s %9s %8s  FAIL: %s\n", base, "-", "-", "-", "-",
             r.error.c_str());
      results.push_back(r);
      failures++;
      continue;
    }
    // second, independent decode to confirm determinism
    double u2, p2, mp2;
    std::string fmt2, cam2;
    decode_once(r.file.c_str(), c2, u2, p2, mp2, fmt2, cam2, err);

    r.ok = true;
    r.fmt = fmt;
    r.camera = cam;
    r.unpack_ms = u1;
    r.process_ms = p1;
    r.mpix = mp;
    r.deterministic = (c1 == c2);
    if (!r.deterministic)
      nondet++;

    double mpixsec = p1 > 0 ? mp * 1000.0 / p1 : 0;
    printf("%-40.40s %-22.22s %7.0fms %8.0fms %8.1f  %s\n", base, fmt.c_str(),
           u1, p1, mpixsec, r.deterministic ? "ok" : "NONDETERMINISTIC!");
    results.push_back(r);
  }

  // ---- slow-outlier detection (median of successful postprocess times) -----
  std::vector<double> times;
  for (size_t i = 0; i < results.size(); i++)
    if (results[i].ok)
      times.push_back(results[i].process_ms);
  double median = 0;
  if (!times.empty())
  {
    std::sort(times.begin(), times.end());
    median = times[times.size() / 2];
  }
  double thr = slow_threshold > 0 ? slow_threshold : median * 3.0;

  // ---- per-format consistency summary --------------------------------------
  std::map<std::string, int> fmt_ok, fmt_fail;
  for (size_t i = 0; i < results.size(); i++)
  {
    if (results[i].ok)
      fmt_ok[results[i].fmt]++;
    else
      fmt_fail[results[i].fmt]++;
  }

  printf("\n==== Per-format summary ====\n");
  std::map<std::string, int> seen;
  for (size_t i = 0; i < results.size(); i++)
    seen[results[i].ok ? results[i].fmt : std::string("(unopened)")] = 1;
  for (std::map<std::string, int>::iterator it = seen.begin(); it != seen.end();
       ++it)
    printf("  %-24s ok=%d fail=%d\n", it->first.c_str(),
           fmt_ok[it->first], fmt_fail[it->first]);

  printf("\n==== Slow files (process > %.0f ms) ====\n", thr);
  bool any_slow = false;
  for (size_t i = 0; i < results.size(); i++)
    if (results[i].ok && results[i].process_ms > thr)
    {
      printf("  %.0f ms  %s\n", results[i].process_ms, results[i].file.c_str());
      any_slow = true;
    }
  if (!any_slow)
    printf("  (none; median process time %.0f ms)\n", median);

  printf("\n==== Result ====\n");
  printf("  total=%zu  ok=%zu  failed=%d  nondeterministic=%d\n", files.size(),
         files.size() - failures, failures, nondet);

  return (failures || nondet) ? 1 : 0;
}
