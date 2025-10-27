### Generate dependencies + configure + build
```bash
# One-time: let Conan detect your compiler profile
conan profile detect

# 1) Install deps and generate toolchain files into ./build
conan install . --output-folder=build --build=missing

# 2) Configure CMake (Release build is typical)
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# 3) Build
cmake --build build -j
```

### One-time (create a Debug build dir)
```bash
conan profile detect
conan install . --output-folder=build-debug --build=missing -s build_type=Debug
cmake -S . -B build-debug -DCMAKE_TOOLCHAIN_FILE=build-debug/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
```

### When do I re-run other steps?


Just source changes: only cmake --build ....

CMakeLists.txt changed (add files/flags): just cmake --build ... — CMake will auto-reconfigure when needed.

Dependencies changed (conanfile.txt/profile): run conan install ... again (to the same build dir), then reconfigure:
```bash
conan install . --output-folder=build-debug --build=missing -s build_type=Debug
cmake -S . -B build-debug -DCMAKE_TOOLCHAIN_FILE=build-debug/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
```

### Debugging + breakpoints
#### Option A: Terminal (gdb / lldb)
Make sure you built Debug (as above).

Start the debugger:

```bash
gdb ./build-debug/download_manager
```

Inside the debugger:

```bash
break main                  # or: break src/main.cpp:5
run
next                        # step over
step                        # step into
print someVar               # inspect variables
continue
quit
```

#### Option B: VS Code (easier)
Install extensions: CMake Tools and C/C++ (by Microsoft).

Open your project folder in VS Code.

Bottom status bar (CMake Tools): choose your kit (GCC/Clang or MinGW) and set Debug config.

Click Build (or Ctrl+Shift+B).

Set breakpoints in the editor → press F5 to debug.

If VS Code asks, pick C++ (GDB/LLDB).

Minimal launch.json if you want one manually (Linux shown):
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug download_manager (gdb)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build-debug/download_manager",
      "args": [],
      "cwd": "${workspaceFolder}",
      "MIMode": "gdb",
      "miDebuggerPath": "gdb",
      "externalConsole": false
    }
  ]
}
```

### Pro tips
Keep two build dirs: build-debug for everyday work, build-release for final testing:

```bash
# Release (faster, few symbols)
conan install . --output-folder=build-release --build=missing -s build_type=Release
cmake -S . -B build-release -DCMAKE_TOOLCHAIN_FILE=build-release/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j8
```

If builds are slow, install Ninja and configure with -G Ninja (faster parallel builds).