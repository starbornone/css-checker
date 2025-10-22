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

# Find required packages
find_package(Threads REQUIRED)

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

# Link libraries
target_link_libraries(css_checker Threads::Threads)

# Installation
install(TARGETS css_checker DESTINATION bin)

# Set output directory
set_target_properties(css_checker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)