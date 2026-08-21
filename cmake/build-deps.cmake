# Build LibRaw's optional dependencies from pinned sources into one prefix.
#
#   cmake [-DPREFIX=<dir>] [-DOSX_ARCHS=<archs>] [-DJOBS=<n>] -P cmake/build-deps.cmake
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=<dir> -DLIBRAW_ENABLE_ZLIB=ON -DLIBRAW_ENABLE_JPEG=ON
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
# Both are built SHARED on purpose. Separate libraries keep a CVE in either one
# a single-file replacement rather than a LibRaw rebuild, and let the
# application share one copy with its other consumers - Panvyo Viewer already
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

message(STATUS "build-deps: done, prefix = ${PREFIX}")
