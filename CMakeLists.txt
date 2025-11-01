cmake_minimum_required(VERSION 3.16)
project(DownloadManager VERSION 1.0.0 LANGUAGES CXX)

# Export compile commands for VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++17 standard, no compiler extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Strict warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find packages installed by Conan
find_package(fmt REQUIRED)
find_package(CURL REQUIRED)
find_package(CLI11 REQUIRED)

# Main executable
add_executable(download_manager
    src/main.cpp
    src/http_client.cpp
)

target_include_directories(download_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(download_manager PRIVATE
    fmt::fmt
    CURL::libcurl
    CLI11::CLI11
)
