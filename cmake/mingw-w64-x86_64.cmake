# CMake toolchain file: cross-build Windows x64 binaries with MinGW-w64 from
# Linux (or build natively under MSYS2's MinGW shell).
#
#   cmake -DPREFIX="$PWD/deps-win64" -DTOOLCHAIN="$PWD/cmake/mingw-w64-x86_64.cmake" \
#         -P cmake/build-deps.cmake
#   cmake -S . -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
#         -DCMAKE_PREFIX_PATH="$PWD/deps-win64" ...
#
# See BINARIES.md section 3, "Windows with MinGW-w64".
#
# The compiler is the "posix" thread-model variant where one exists: GCC's
# OpenMP runtime (libgomp) is written against POSIX threads, which MinGW-w64
# provides through winpthreads.
#
# The toolchain's own runtime is linked STATICALLY into every DLL and EXE:
# libgcc, libstdc++, libgomp and winpthreads. Left dynamic, a binary imports
# libgcc_s_seh-1.dll, libstdc++-6.dll, libgomp-1.dll and libwinpthread-1.dll,
# and fails to load - silently, from the application's side - wherever one of
# them is not beside it. libgcc and libstdc++ have -static-* switches;
# libgomp and winpthreads do not, and -fopenmp adds a plain -lgomp -lpthread,
# for which the linker prefers the import library (.dll.a) over the archive
# (.a) in the same directory. So the two archives are copied into a directory
# of their own that holds no import libraries, and that directory is searched
# first.
#
# Licences of what is linked in: libgcc, libstdc++ and libgomp are GPL-3.0
# with the GCC Runtime Library Exception, which allows combining them with
# code under any licence. winpthreads is MIT (expat), with two headers under
# the 3-clause BSD licence of Lockless Inc.; its notice belongs in the
# shipping application's third-party notices.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT MINGW_TRIPLET)
    set(MINGW_TRIPLET x86_64-w64-mingw32)
endif()

find_program(MINGW_GCC NAMES ${MINGW_TRIPLET}-gcc-posix ${MINGW_TRIPLET}-gcc REQUIRED)
find_program(MINGW_GXX NAMES ${MINGW_TRIPLET}-g++-posix ${MINGW_TRIPLET}-g++ REQUIRED)
find_program(MINGW_WINDRES NAMES ${MINGW_TRIPLET}-windres windres)
set(CMAKE_C_COMPILER   "${MINGW_GCC}")
set(CMAKE_CXX_COMPILER "${MINGW_GXX}")
if(MINGW_WINDRES)
    set(CMAKE_RC_COMPILER "${MINGW_WINDRES}")
endif()

set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_TRIPLET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# The static-runtime directory (see above), one per build tree.
set(_mw_static "${CMAKE_BINARY_DIR}/mingw-static-runtime")
if(NOT EXISTS "${_mw_static}/libwinpthread.a")
    file(MAKE_DIRECTORY "${_mw_static}")
    foreach(_mw_lib libgomp.a libwinpthread.a)
        execute_process(COMMAND "${MINGW_GCC}" -print-file-name=${_mw_lib}
                        OUTPUT_VARIABLE _mw_path OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(IS_ABSOLUTE "${_mw_path}" AND EXISTS "${_mw_path}")
            file(COPY "${_mw_path}" DESTINATION "${_mw_static}")
        endif()
    endforeach()
    if(EXISTS "${_mw_static}/libwinpthread.a")
        # -fopenmp asks for -lpthread; on MinGW-w64 that is winpthreads.
        file(COPY_FILE "${_mw_static}/libwinpthread.a" "${_mw_static}/libpthread.a")
    endif()
endif()

set(CMAKE_SHARED_LINKER_FLAGS_INIT "-L${_mw_static} -static-libgcc -static-libstdc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-L${_mw_static} -static-libgcc -static-libstdc++")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-L${_mw_static} -static-libgcc -static-libstdc++")

# FindOpenMP records libgomp by full path - the import library - and hands it
# to everything that links OpenMP::OpenMP_CXX, which bypasses the directory
# above (a target linking raw without -fopenmp of its own would then import
# libgomp-1.dll). Point it at the archive, with winpthreads after it.
if(EXISTS "${_mw_static}/libgomp.a")
    set(OpenMP_gomp_LIBRARY "${_mw_static}/libgomp.a;${_mw_static}/libwinpthread.a"
        CACHE STRING "libgomp, linked statically (cmake/mingw-w64-x86_64.cmake)")
endif()
