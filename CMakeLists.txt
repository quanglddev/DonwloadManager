cmake_minimum_required(VERSION 3.16)

# 1) Name the project and set the C++ standard
project(download_manager LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # enforce pure C++17, no compiler-specific extensions

# 2) Add the executable
add_executable(download_manager src/main.cpp)

# 3) Warnings: different flags for GCC/Clang vs MSVC
if(MSVC)
    target_compile_options(download_manager PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(download_manager PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 4) Find and link dependencies provided by Conan (fmt here)
#    Conan generates config files that make this find_package work.
find_package(fmt CONFIG REQUIRED)
target_link_libraries(download_manager PRIVATE fmt::fmt)
