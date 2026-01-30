# LibRaw CMake Build Configuration
# Cross-platform build for Windows, macOS, and Linux

cmake_minimum_required(VERSION 3.16)

project(LibRaw
    VERSION 0.21.3
    DESCRIPTION "Library for reading RAW files from digital cameras"
    LANGUAGES CXX C
)

# Build options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(LIBRAW_ENABLE_ZLIB "Enable zlib support for deflate DNG" ON)
option(LIBRAW_ENABLE_JPEG "Enable JPEG support for thumbnails" ON)
option(LIBRAW_ENABLE_LCMS2 "Enable LCMS2 color management" ON)
option(LIBRAW_ENABLE_JASPER "Enable JasPer JPEG-2000 support" ON)
option(LIBRAW_ENABLE_X3FTOOLS "Enable Sigma X3F support" ON)
option(LIBRAW_ENABLE_6BY9RPI "Enable Raspberry Pi camera support" OFF)

# C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Source files
set(LIBRAW_SOURCES
    # Core
    src/libraw_c_api.cpp
    src/libraw_datastream.cpp

    # Decoders
    src/decoders/canon_600.cpp
    src/decoders/crx.cpp
    src/decoders/decoders_dcraw.cpp
    src/decoders/decoders_libraw.cpp
    src/decoders/decoders_libraw_dcrdefs.cpp
    src/decoders/dng.cpp
    src/decoders/fp_dng.cpp
    src/decoders/fuji_compressed.cpp
    src/decoders/generic.cpp
    src/decoders/kodak_decoders.cpp
    src/decoders/load_mfbacks.cpp
    src/decoders/olympus14.cpp
    src/decoders/pana8.cpp
    src/decoders/smal.cpp
    src/decoders/sonycc.cpp
    src/decoders/unpack.cpp
    src/decoders/unpack_thumb.cpp

    # Decompressors
    src/decompressors/losslessjpeg.cpp

    # Demosaic
    src/demosaic/aahd_demosaic.cpp
    src/demosaic/ahd_demosaic.cpp
    src/demosaic/dcb_demosaic.cpp
    src/demosaic/dht_demosaic.cpp
    src/demosaic/misc_demosaic.cpp
    src/demosaic/xtrans_demosaic.cpp

    # Integration (stubs for optional SDKs)
    src/integration/dngsdk_glue.cpp
    src/integration/rawspeed_glue.cpp

    # Metadata
    src/metadata/adobepano.cpp
    src/metadata/canon.cpp
    src/metadata/ciff.cpp
    src/metadata/cr3_parser.cpp
    src/metadata/epson.cpp
    src/metadata/exif_gps.cpp
    src/metadata/fuji.cpp
    src/metadata/hasselblad_model.cpp
    src/metadata/identify.cpp
    src/metadata/identify_tools.cpp
    src/metadata/kodak.cpp
    src/metadata/leica.cpp
    src/metadata/makernotes.cpp
    src/metadata/mediumformat.cpp
    src/metadata/minolta.cpp
    src/metadata/misc_parsers.cpp
    src/metadata/nikon.cpp
    src/metadata/normalize_model.cpp
    src/metadata/olympus.cpp
    src/metadata/p1.cpp
    src/metadata/pentax.cpp
    src/metadata/samsung.cpp
    src/metadata/sony.cpp
    src/metadata/tiff.cpp

    # Postprocessing (excluding *_ph.cpp placeholder files which are for building without postprocessing)
    src/postprocessing/aspect_ratio.cpp
    src/postprocessing/dcraw_process.cpp
    src/postprocessing/mem_image.cpp
    src/postprocessing/postprocessing_aux.cpp
    # src/postprocessing/postprocessing_ph.cpp  # Placeholder - excluded
    src/postprocessing/postprocessing_utils.cpp
    src/postprocessing/postprocessing_utils_dcrdefs.cpp

    # Preprocessing (excluding *_ph.cpp placeholder files)
    src/preprocessing/ext_preprocess.cpp
    # src/preprocessing/preprocessing_ph.cpp  # Placeholder - excluded
    src/preprocessing/raw2image.cpp
    src/preprocessing/subtract_black.cpp

    # Tables
    src/tables/cameralist.cpp
    src/tables/colorconst.cpp
    src/tables/colordata.cpp
    src/tables/wblists.cpp

    # Utils
    src/utils/curves.cpp
    src/utils/decoder_info.cpp
    src/utils/init_close_utils.cpp
    src/utils/open.cpp
    src/utils/phaseone_processing.cpp
    src/utils/read_utils.cpp
    src/utils/thumb_utils.cpp
    src/utils/utils_dcraw.cpp
    src/utils/utils_libraw.cpp

    # Write (excluding *_ph.cpp placeholder files)
    src/write/apply_profile.cpp
    src/write/file_write.cpp
    src/write/tiff_writer.cpp
    # src/write/write_ph.cpp  # Placeholder - excluded

    # X3F (Sigma)
    src/x3f/x3f_parse_process.cpp
    src/x3f/x3f_utils_patched.cpp
)

# Public headers
set(LIBRAW_HEADERS
    libraw/libraw.h
    libraw/libraw_alloc.h
    libraw/libraw_const.h
    libraw/libraw_datastream.h
    libraw/libraw_internal.h
    libraw/libraw_types.h
    libraw/libraw_version.h
)

# Create library
add_library(raw ${LIBRAW_SOURCES})

# Set target properties
set_target_properties(raw PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "raw"
    PUBLIC_HEADER "${LIBRAW_HEADERS}"
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(raw PRIVATE
        WIN32
        WIN64
        _WINDOWS
        UNICODE
        _UNICODE
        _ENABLE_EXTENDED_ALIGNED_STORAGE
        _CRT_SECURE_NO_WARNINGS
        LIBRAW_BUILDLIB
        LIBRAW_WIN32_CALLS
        LIBRAW_WIN32_UNICODEPATHS
        LIBRAW_WIN32_DLLDEFS
    )

    if(NOT BUILD_SHARED_LIBS)
        # Only define LIBRAW_NODLL for static builds - its presence (not value) disables DLL exports
        target_compile_definitions(raw PRIVATE LIBRAW_NODLL)
    endif()

    # Link Windows socket library
    target_link_libraries(raw PRIVATE ws2_32)
endif()

# macOS-specific settings
if(APPLE)
    target_compile_definitions(raw PRIVATE
        LIBRAW_NOTHREADS
    )
endif()

# Include directories
target_include_directories(raw
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

# Compile options
if(MSVC)
    target_compile_options(raw PRIVATE
        /W3
        /Zc:rvalueCast
        /Zc:inline
        /Zc:strictStrings
        /Zc:throwingNew
        /Zc:referenceBinding
        /Zc:__cplusplus
    )
else()
    target_compile_options(raw PRIVATE
        -Wall
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
endif()

# Find and link dependencies
set(LIBRAW_COMPILE_DEFS "")

# ZLib support (supports both zlib and zlib-ng)
# zlib-ng in compatibility mode produces zlib1.dll
if(LIBRAW_ENABLE_ZLIB)
    # First try zlib-ng in dependencies folder (preferred)
    set(ZLIB_NG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/zlib-ng")
    set(ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/zlib")

    # Check for zlib-ng first
    if(EXISTS "${ZLIB_NG_ROOT}")
        find_path(ZLIB_INCLUDE_DIR zlib.h
            PATHS ${ZLIB_NG_ROOT} ${ZLIB_NG_ROOT}/build
            PATH_SUFFIXES include
            NO_DEFAULT_PATH
        )
        find_library(ZLIB_LIBRARY
            NAMES zlib1 zlib z zlibstatic
            PATHS ${ZLIB_NG_ROOT}/build ${ZLIB_NG_ROOT}/build/Release ${ZLIB_NG_ROOT}/build/Debug
            PATH_SUFFIXES Release Debug lib bin
            NO_DEFAULT_PATH
        )
        if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
            target_include_directories(raw PRIVATE ${ZLIB_INCLUDE_DIR})
            target_link_libraries(raw PRIVATE ${ZLIB_LIBRARY})
            target_compile_definitions(raw PRIVATE USE_ZLIB)
            list(APPEND LIBRAW_COMPILE_DEFS USE_ZLIB)
            message(STATUS "LibRaw: zlib-ng support enabled (local)")
            message(STATUS "  zlib-ng include: ${ZLIB_INCLUDE_DIR}")
            message(STATUS "  zlib-ng library: ${ZLIB_LIBRARY}")
            set(ZLIB_CONFIGURED TRUE)
        endif()
    endif()

    # Fallback to regular zlib in dependencies folder
    if(NOT ZLIB_CONFIGURED AND EXISTS "${ZLIB_ROOT}")
        find_path(ZLIB_INCLUDE_DIR zlib.h
            PATHS ${ZLIB_ROOT} ${ZLIB_ROOT}/build
            PATH_SUFFIXES include
            NO_DEFAULT_PATH
        )
        find_library(ZLIB_LIBRARY
            NAMES zlib1 zlib z zlibd zlibstatic
            PATHS ${ZLIB_ROOT}/build ${ZLIB_ROOT}/build/Release ${ZLIB_ROOT}/build/Debug
            PATH_SUFFIXES Release Debug lib bin
            NO_DEFAULT_PATH
        )
        if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
            target_include_directories(raw PRIVATE ${ZLIB_INCLUDE_DIR})
            target_link_libraries(raw PRIVATE ${ZLIB_LIBRARY})
            target_compile_definitions(raw PRIVATE USE_ZLIB)
            list(APPEND LIBRAW_COMPILE_DEFS USE_ZLIB)
            message(STATUS "LibRaw: zlib support enabled (local)")
            message(STATUS "  zlib include: ${ZLIB_INCLUDE_DIR}")
            message(STATUS "  zlib library: ${ZLIB_LIBRARY}")
            set(ZLIB_CONFIGURED TRUE)
        endif()
    endif()

    # Fallback to system zlib
    if(NOT ZLIB_CONFIGURED)
        find_package(ZLIB QUIET)
        if(ZLIB_FOUND)
            target_link_libraries(raw PRIVATE ZLIB::ZLIB)
            target_compile_definitions(raw PRIVATE USE_ZLIB)
            list(APPEND LIBRAW_COMPILE_DEFS USE_ZLIB)
            message(STATUS "LibRaw: zlib support enabled (system)")
            set(ZLIB_CONFIGURED TRUE)
        endif()
    endif()

    if(NOT ZLIB_CONFIGURED)
        message(WARNING "LibRaw: zlib/zlib-ng not found, deflate DNG support disabled")
    endif()
endif()

# JPEG support (libjpeg/libjpeg-turbo)
if(LIBRAW_ENABLE_JPEG)
    find_package(JPEG QUIET)
    if(JPEG_FOUND)
        target_link_libraries(raw PRIVATE JPEG::JPEG)
        target_compile_definitions(raw PRIVATE USE_JPEG)
        list(APPEND LIBRAW_COMPILE_DEFS USE_JPEG)
        message(STATUS "LibRaw: JPEG support enabled")
    else()
        # Try to find libjpeg-turbo in dependencies folder
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/libjpeg-turbo")
            set(JPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/libjpeg-turbo")
            # libjpeg-turbo has jpeglib.h in src/ folder, jconfig.h in build/
            find_path(JPEG_INCLUDE_DIR jpeglib.h
                PATHS ${JPEG_ROOT} ${JPEG_ROOT}/build ${JPEG_ROOT}/src
                PATH_SUFFIXES include src
                NO_DEFAULT_PATH
            )
            find_path(JPEG_CONFIG_DIR jconfig.h
                PATHS ${JPEG_ROOT}/build ${JPEG_ROOT}
                PATH_SUFFIXES include
                NO_DEFAULT_PATH
            )
            find_library(JPEG_LIBRARY
                NAMES jpeg jpeg-static jpeg62
                PATHS ${JPEG_ROOT}/build ${JPEG_ROOT}/build/Release ${JPEG_ROOT}/build/Debug
                PATH_SUFFIXES Release Debug lib bin
                NO_DEFAULT_PATH
            )
            if(JPEG_INCLUDE_DIR AND JPEG_LIBRARY)
                target_include_directories(raw PRIVATE ${JPEG_INCLUDE_DIR})
                if(JPEG_CONFIG_DIR AND NOT JPEG_CONFIG_DIR STREQUAL JPEG_INCLUDE_DIR)
                    target_include_directories(raw PRIVATE ${JPEG_CONFIG_DIR})
                endif()
                target_link_libraries(raw PRIVATE ${JPEG_LIBRARY})
                target_compile_definitions(raw PRIVATE USE_JPEG)
                list(APPEND LIBRAW_COMPILE_DEFS USE_JPEG)
                message(STATUS "LibRaw: JPEG support enabled (local libjpeg-turbo)")
                message(STATUS "  JPEG include: ${JPEG_INCLUDE_DIR}")
                message(STATUS "  JPEG config: ${JPEG_CONFIG_DIR}")
                message(STATUS "  JPEG library: ${JPEG_LIBRARY}")
            else()
                message(WARNING "LibRaw: JPEG library not found, JPEG thumbnail support disabled")
                message(STATUS "  JPEG_INCLUDE_DIR: ${JPEG_INCLUDE_DIR}")
                message(STATUS "  JPEG_LIBRARY: ${JPEG_LIBRARY}")
            endif()
        else()
            message(WARNING "LibRaw: JPEG library not found, JPEG thumbnail support disabled")
        endif()
    endif()
endif()

# LCMS2 support
if(LIBRAW_ENABLE_LCMS2)
    find_package(LCMS2 QUIET)
    if(LCMS2_FOUND)
        target_link_libraries(raw PRIVATE ${LCMS2_LIBRARIES})
        target_include_directories(raw PRIVATE ${LCMS2_INCLUDE_DIRS})
        target_compile_definitions(raw PRIVATE USE_LCMS2)
        list(APPEND LIBRAW_COMPILE_DEFS USE_LCMS2)
        message(STATUS "LibRaw: LCMS2 color management enabled")
    else()
        # Try to find lcms2 in dependencies folder
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lcms2")
            set(LCMS2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lcms2")
            find_path(LCMS2_INCLUDE_DIR lcms2.h
                PATHS ${LCMS2_ROOT}
                PATH_SUFFIXES include
            )
            find_library(LCMS2_LIBRARY
                NAMES lcms2 lcms2_static
                PATHS ${LCMS2_ROOT}/build ${LCMS2_ROOT}/Lib/MS
                PATH_SUFFIXES Release Debug lib bin
            )
            if(LCMS2_INCLUDE_DIR AND LCMS2_LIBRARY)
                target_include_directories(raw PRIVATE ${LCMS2_INCLUDE_DIR})
                target_link_libraries(raw PRIVATE ${LCMS2_LIBRARY})
                target_compile_definitions(raw PRIVATE USE_LCMS2)
                list(APPEND LIBRAW_COMPILE_DEFS USE_LCMS2)
                message(STATUS "LibRaw: LCMS2 color management enabled (local)")
            else()
                message(STATUS "LibRaw: LCMS2 not found, color management disabled")
            endif()
        else()
            message(STATUS "LibRaw: LCMS2 not found, color management disabled")
        endif()
    endif()
endif()

# X3F Tools (Sigma RAW support)
if(LIBRAW_ENABLE_X3FTOOLS)
    target_compile_definitions(raw PRIVATE USE_X3FTOOLS)
    list(APPEND LIBRAW_COMPILE_DEFS USE_X3FTOOLS)
    message(STATUS "LibRaw: X3F (Sigma) support enabled")
endif()

# Raspberry Pi camera support
if(LIBRAW_ENABLE_6BY9RPI)
    target_compile_definitions(raw PRIVATE USE_6BY9RPI)
    list(APPEND LIBRAW_COMPILE_DEFS USE_6BY9RPI)
    message(STATUS "LibRaw: Raspberry Pi camera support enabled")
endif()

# Installation rules
include(GNUInstallDirs)

install(TARGETS raw
    EXPORT LibRawTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libraw
)

# Export package configuration
install(EXPORT LibRawTargets
    FILE LibRawTargets.cmake
    NAMESPACE LibRaw::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LibRaw
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LibRawConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibRawConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LibRawConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LibRaw
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/LibRawConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LibRawConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LibRaw
)

# Tests
option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "LibRaw: Tests enabled")
endif()

# Samples
option(BUILD_SAMPLES "Build sample programs" OFF)
if(BUILD_SAMPLES)
    # Simple dcraw sample
    add_executable(simple_dcraw samples/simple_dcraw.cpp)
    target_link_libraries(simple_dcraw PRIVATE raw)
    target_include_directories(simple_dcraw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Raw identify sample
    add_executable(raw_identify samples/raw-identify.cpp)
    target_link_libraries(raw_identify PRIVATE raw)
    target_include_directories(raw_identify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Memory image sample
    add_executable(mem_image_sample samples/mem_image_sample.cpp)
    target_link_libraries(mem_image_sample PRIVATE raw)
    target_include_directories(mem_image_sample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # 4 channels sample
    add_executable(4channels samples/4channels.cpp)
    target_link_libraries(4channels PRIVATE raw)
    target_include_directories(4channels PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # dcraw_emu sample
    add_executable(dcraw_emu samples/dcraw_emu.cpp)
    target_link_libraries(dcraw_emu PRIVATE raw)
    target_include_directories(dcraw_emu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # unprocessed_raw sample
    add_executable(unprocessed_raw samples/unprocessed_raw.cpp)
    target_link_libraries(unprocessed_raw PRIVATE raw)
    target_include_directories(unprocessed_raw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(WIN32)
        target_link_libraries(unprocessed_raw PRIVATE ws2_32)
    endif()

    message(STATUS "LibRaw: Samples enabled")
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "LibRaw ${PROJECT_VERSION} Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "  Features enabled: ${LIBRAW_COMPILE_DEFS}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")
