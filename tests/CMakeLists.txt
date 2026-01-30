# LibRaw Tests CMakeLists.txt

# Test for security fixes
add_executable(test_security_fixes test_security_fixes.cpp)

# Include the internal headers
target_include_directories(test_security_fixes PRIVATE
    ${CMAKE_SOURCE_DIR}/internal
    ${CMAKE_SOURCE_DIR}/libraw
)

# Link math library on Unix
if(UNIX)
    target_link_libraries(test_security_fixes m)
endif()

# Add as a test
add_test(NAME SecurityFixes COMMAND test_security_fixes)

# Enable testing
enable_testing()
