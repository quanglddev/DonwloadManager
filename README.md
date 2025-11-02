#### Install Dependencies with Conan
```bash
# Create build directory
mkdir -p build/Release
cd build/Release

# Install dependencies for Release build
conan install ../.. --output-folder=. --build=missing -s build_type=Release

# Go back to project root
cd ../..
```


#### Build the Project
```bash
# Configure with Conan toolchain (from project root)
cmake -B build/Release \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake

# Build
cmake --build build

# Verify executable exists
ls -lh build/bin/download_manager
```

#### After putting new stuff in conan
```bash
conan install . --output-folder=build/Release --build=missing --settings=build_type=Release
```