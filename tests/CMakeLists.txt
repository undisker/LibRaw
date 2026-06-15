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

# File-free pipeline consistency test: proves the OpenMP postprocessing/demosaic
# paths are deterministic and produce the same output single- vs multi-threaded.
# Links the library and feeds it a synthetic frame via open_bayer().
add_executable(test_pipeline_consistency test_pipeline_consistency.cpp)
target_include_directories(test_pipeline_consistency PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(test_pipeline_consistency raw)
add_test(NAME PipelineConsistency COMMAND test_pipeline_consistency)

# Enable testing
enable_testing()
