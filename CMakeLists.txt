cmake_minimum_required(VERSION 3.15)
project(css_checker VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler flags for better warnings and debugging
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.cpp
    src/common.cpp
    src/css_parser.cpp
    src/file_scanner.cpp
    src/usage_detector.cpp
    src/report_generator.cpp
    src/cli_parser.cpp
)

# Header files
set(HEADERS
    include/css_parser.h
    include/file_scanner.h
    include/usage_detector.h
    include/report_generator.h
    include/cli_parser.h
    include/common.h
)

# Create executable
add_executable(css_checker ${SOURCES} ${HEADERS})

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(css_checker stdc++fs)
endif()

# Installation
install(TARGETS css_checker DESTINATION bin)

# Set output directory
set_target_properties(css_checker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

enable_testing()

add_executable(css_checker_tests
    tests/test_checker.cpp
    src/common.cpp
    src/css_parser.cpp
    src/file_scanner.cpp
    src/usage_detector.cpp
    src/report_generator.cpp
    src/cli_parser.cpp
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(css_checker_tests stdc++fs)
endif()

set_target_properties(css_checker_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME checker COMMAND css_checker_tests)
add_test(NAME cli_help COMMAND css_checker --help)
add_test(NAME cli_version COMMAND css_checker --version)