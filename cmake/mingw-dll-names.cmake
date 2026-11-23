# Injected into the dependency builds by build-deps.cmake (as
# CMAKE_PROJECT_INCLUDE) when they are built with MinGW-w64, so their DLLs get
# the names an MSVC build gives them and the application ships:
#
#   zlib-ng (compat)  libzlib1.dll    -> zlib1.dll
#   libjpeg-turbo     libjpeg-62.dll  -> jpeg62.dll
#
# raw.dll records the DLL name it was linked against, so the name must be
# right when the dependency is built; renaming the file afterwards breaks the
# import. Only the DLL is renamed - the import libraries keep their usual
# names (libzlib.dll.a, libjpeg.dll.a), which is what FindZLIB and FindJPEG
# look for.
#
# The targets do not exist yet when this file runs (right after project()),
# so the change is deferred to the end of the top-level directory.

if(MINGW AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    if(CMAKE_VERSION VERSION_LESS 3.19)
        message(FATAL_ERROR "mingw-dll-names.cmake needs CMake 3.19 or newer (cmake_language DEFER)")
    endif()
    function(_libraw_mingw_dll_names)
        if(TARGET zlib-ng)
            get_target_property(_type zlib-ng TYPE)
            if(_type STREQUAL "SHARED_LIBRARY")
                set_target_properties(zlib-ng PROPERTIES PREFIX "")
            endif()
        endif()
        if(TARGET jpeg)
            get_target_property(_type jpeg TYPE)
            if(_type STREQUAL "SHARED_LIBRARY")
                set_target_properties(jpeg PROPERTIES
                    PREFIX "" SUFFIX ".dll" RUNTIME_OUTPUT_NAME "jpeg62")
            endif()
        endif()
    endfunction()
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _libraw_mingw_dll_names)
endif()
