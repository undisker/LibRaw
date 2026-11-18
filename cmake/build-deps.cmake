# Build LibRaw's optional dependencies from pinned sources into one prefix.
#
#   cmake [-DPREFIX=<dir>] [-DOSX_ARCHS=<archs>] [-DJOBS=<n>] -P cmake/build-deps.cmake
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=<dir> -DLIBRAW_ENABLE_ZLIB=ON -DLIBRAW_ENABLE_JPEG=ON -DLIBRAW_ENABLE_JXL=ON
#
# Run with `cmake -P`, so the same one command works on Windows, macOS and
# Linux - no vcpkg, Homebrew and apt recipes to keep in agreement, and the
# versions are pinned rather than whatever the runner image happens to carry.
#
# Why build them at all instead of using the system copies:
#
#   * macOS. The release is a universal binary. A Homebrew libjpeg-turbo is
#     built for the host architecture only and cannot satisfy both slices;
#     built here it inherits OSX_ARCHS and comes out universal too.
#   * Linux. Distro *static* archives are not compiled -fPIC and cannot be
#     linked into a shared library at all ("relocation R_X86_64_PC32 against
#     `z_errmsg` can not be used when making a shared object"). Everything
#     built here is position-independent.
#   * Windows. There is no system copy to find.
#
# All are built SHARED on purpose. Separate libraries keep a CVE in any of them
# a single-file replacement rather than a LibRaw rebuild, and let the
# application share one copy with its other consumers - the host application already
# ships zlib1.dll for libtiff and its own compression code. See BINARIES.md.
#
# To bump a version, change the tag below; nothing else needs touching.

cmake_minimum_required(VERSION 3.16)

# zlib-ng in compat mode is ABI-compatible with zlib and faster at inflate,
# which is the deflate-DNG path.
set(ZLIB_NG_REPO "https://github.com/zlib-ng/zlib-ng.git")
set(ZLIB_NG_TAG  "2.3.3")

set(JPEG_REPO "https://github.com/libjpeg-turbo/libjpeg-turbo.git")
set(JPEG_TAG  "3.1.4")

# libjxl decodes JPEG XL compressed DNG (DNG 1.7). It needs three of its
# submodules - highway, brotli and a colour management library; the others
# are test data and tools. Little CMS is used rather than the default skcms:
# skcms is the one submodule not hosted on GitHub, and a build that depends on
# a second host fails wherever only GitHub is reachable.
set(JXL_REPO "https://github.com/libjxl/libjxl.git")
set(JXL_TAG  "v0.11.1")
set(JXL_SUBMODULES third_party/highway third_party/brotli third_party/lcms)

# GoPro's GPR SDK, for its VC-5 decoder only (source/lib/vc5_decoder,
# vc5_common, common). It is not built here: LibRaw's own CMakeLists compiles
# those three directories, with the hardening patch below and no other change,
# into the library (LIBRAW_ENABLE_GPR_SDK)
# and finds the checkout at <prefix>/_src/gpr. The repository has no release
# tags, so a commit is pinned.
set(GPR_REPO   "https://github.com/gopro/gpr.git")
set(GPR_COMMIT "446c736a38fb14f51343605c0780d347dc602f89")   # 2026-05-05

if(NOT DEFINED PREFIX OR PREFIX STREQUAL "")
    set(PREFIX "${CMAKE_CURRENT_LIST_DIR}/../dependencies/prefix")
endif()
get_filename_component(PREFIX "${PREFIX}" ABSOLUTE)

if(NOT DEFINED JOBS OR JOBS STREQUAL "")
    include(ProcessorCount)
    ProcessorCount(JOBS)
    if(JOBS EQUAL 0)
        set(JOBS 2)
    endif()
endif()

set(WORK "${PREFIX}/_src")
file(MAKE_DIRECTORY "${WORK}")

find_package(Git REQUIRED)

# Clone <repo> at <tag> into <dir>, or leave an existing checkout alone.
# Further arguments name the submodules to fetch as well (at the commits the
# tag records).
function(fetch_dep aName aRepo aTag aDir)
    if(EXISTS "${aDir}/.git")
        message(STATUS "build-deps: ${aName} already present at ${aDir}")
        return()
    endif()
    message(STATUS "build-deps: cloning ${aName} ${aTag}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" clone --depth 1 --branch "${aTag}" "${aRepo}" "${aDir}"
        RESULT_VARIABLE iRc
    )
    if(NOT iRc EQUAL 0)
        message(FATAL_ERROR "build-deps: cloning ${aName} failed (${iRc})")
    endif()
    if(ARGN)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${aDir}" submodule update --init --depth 1 ${ARGN}
            RESULT_VARIABLE iRc
        )
        if(NOT iRc EQUAL 0)
            message(FATAL_ERROR "build-deps: fetching the submodules of ${aName} failed (${iRc})")
        endif()
    endif()
endfunction()

# Fetch <repo> at exactly <commit> into <dir> (for repositories without tags).
function(fetch_commit aName aRepo aCommit aDir)
    if(EXISTS "${aDir}/.git")
        message(STATUS "build-deps: ${aName} already present at ${aDir}")
        return()
    endif()
    message(STATUS "build-deps: fetching ${aName} ${aCommit}")
    file(MAKE_DIRECTORY "${aDir}")
    foreach(iCmd "init;-q" "remote;add;origin;${aRepo}" "fetch;-q;--depth;1;origin;${aCommit}" "checkout;-q;FETCH_HEAD")
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${aDir}" ${iCmd} RESULT_VARIABLE iRc)
        if(NOT iRc EQUAL 0)
            message(FATAL_ERROR "build-deps: fetching ${aName} failed at 'git ${iCmd}' (${iRc})")
        endif()
    endforeach()
endfunction()

# Apply <patch> to the checkout in <dir>, unless it is already applied.
function(apply_patch aName aDir aPatch)
    get_filename_component(iPatch "${aPatch}" ABSOLUTE)
    if(NOT EXISTS "${iPatch}")
        message(WARNING "build-deps: patch for ${aName} not found: ${iPatch}")
        return()
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${aDir}" apply --check "${iPatch}"
                    RESULT_VARIABLE iRc OUTPUT_QUIET ERROR_QUIET)
    if(iRc EQUAL 0)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${aDir}" apply "${iPatch}" RESULT_VARIABLE iRc)
        if(NOT iRc EQUAL 0)
            message(FATAL_ERROR "build-deps: applying the ${aName} patch failed (${iRc})")
        endif()
        message(STATUS "build-deps: ${aName} patched")
        return()
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${aDir}" apply --reverse --check "${iPatch}"
                    RESULT_VARIABLE iRc OUTPUT_QUIET ERROR_QUIET)
    if(iRc EQUAL 0)
        message(STATUS "build-deps: ${aName} patch already applied")
    else()
        message(FATAL_ERROR "build-deps: the ${aName} patch neither applies nor is applied - "
                            "the checkout in ${aDir} is not the pinned commit")
    endif()
endfunction()

# Configure, build and install one dependency into PREFIX.
function(build_dep aName aSrc)
    set(iArgs ${ARGN})
    if(OSX_ARCHS)
        list(APPEND iArgs "-DCMAKE_OSX_ARCHITECTURES=${OSX_ARCHS}")
    endif()
    message(STATUS "build-deps: building ${aName}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${aSrc}" -B "${aSrc}/build"
                -DCMAKE_BUILD_TYPE=Release
                "-DCMAKE_INSTALL_PREFIX=${PREFIX}"
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                ${iArgs}
        RESULT_VARIABLE iRc
    )
    if(NOT iRc EQUAL 0)
        message(FATAL_ERROR "build-deps: configuring ${aName} failed (${iRc})")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${aSrc}/build" --config Release --parallel ${JOBS}
        RESULT_VARIABLE iRc
    )
    if(NOT iRc EQUAL 0)
        message(FATAL_ERROR "build-deps: building ${aName} failed (${iRc})")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install "${aSrc}/build" --config Release
        RESULT_VARIABLE iRc
    )
    if(NOT iRc EQUAL 0)
        message(FATAL_ERROR "build-deps: installing ${aName} failed (${iRc})")
    endif()
endfunction()

fetch_dep(zlib-ng "${ZLIB_NG_REPO}" "${ZLIB_NG_TAG}" "${WORK}/zlib-ng")
build_dep(zlib-ng "${WORK}/zlib-ng"
    -DZLIB_COMPAT=ON            # drop-in zlib ABI: libz.so.1 / zlib1.dll
    -DBUILD_SHARED_LIBS=ON
    -DZLIB_ENABLE_TESTS=OFF
    -DZLIBNG_ENABLE_TESTS=OFF
    -DWITH_GTEST=OFF
)

# libjpeg-turbo refuses add_subdirectory()/FetchContent by design, so it is
# built standalone here rather than pulled into the LibRaw build.
fetch_dep(libjpeg-turbo "${JPEG_REPO}" "${JPEG_TAG}" "${WORK}/libjpeg-turbo")
build_dep(libjpeg-turbo "${WORK}/libjpeg-turbo"
    -DENABLE_SHARED=ON
    -DENABLE_STATIC=OFF
    # LibRaw uses the libjpeg API only; leaving the TurboJPEG API out avoids
    # building and shipping a second copy of the codec.
    -DWITH_TURBOJPEG=OFF
)

# Decoder only: no tools, no tests, no encoder front ends, no JPEG transcoding.
fetch_dep(libjxl "${JXL_REPO}" "${JXL_TAG}" "${WORK}/libjxl" ${JXL_SUBMODULES})
build_dep(libjxl "${WORK}/libjxl"
    -DBUILD_SHARED_LIBS=ON
    -DBUILD_TESTING=OFF
    -DJPEGXL_ENABLE_TOOLS=OFF
    -DJPEGXL_ENABLE_DOXYGEN=OFF
    -DJPEGXL_ENABLE_MANPAGES=OFF
    -DJPEGXL_ENABLE_BENCHMARK=OFF
    -DJPEGXL_ENABLE_EXAMPLES=OFF
    -DJPEGXL_ENABLE_JNI=OFF
    -DJPEGXL_ENABLE_SJPEG=OFF
    -DJPEGXL_ENABLE_OPENEXR=OFF
    -DJPEGXL_ENABLE_JPEGLI=OFF
    -DJPEGXL_ENABLE_TRANSCODE_JPEG=OFF
    -DJPEGXL_ENABLE_SKCMS=OFF           # Little CMS, see above
    -DJPEGXL_BUNDLE_LIBPNG=OFF
    -DJPEGXL_FORCE_SYSTEM_BROTLI=OFF
    -DJPEGXL_FORCE_SYSTEM_HWY=OFF
)

fetch_commit(gpr "${GPR_REPO}" "${GPR_COMMIT}" "${WORK}/gpr")
# GoPro's decoder trusts its input (heap overflows on damaged files). The
# patch adds bounds checks and changes nothing else; LibRaw's CMakeLists warns
# when it compiles an unpatched checkout.
apply_patch(gpr "${WORK}/gpr" "${CMAKE_CURRENT_LIST_DIR}/../GoPro/gpr-vc5-hardening.patch")

message(STATUS "build-deps: done, prefix = ${PREFIX}")
