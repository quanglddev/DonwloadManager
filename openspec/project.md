# Project Context

## Purpose
A production-grade, terminal-based multithreaded download manager built in modern C++17. This is a **learning-first project** designed to master:
- Modern C++ concurrency primitives (threads, mutexes, condition variables, atomics)
- Network programming with HTTP protocols, range requests, and connection management
- Robust system design with error handling, retry logic, and state persistence
- Thread pool architectures, resource throttling, and performance optimization
- RAII, smart pointers, move semantics, and STL best practices

The application will download multiple files concurrently, handle network interruptions gracefully, persist state across restarts, and provide a polished terminal UI with real-time progress tracking.

## Tech Stack
- **Language**: C++17 (standard library concurrency, no Boost)
- **Build System**: CMake 3.16+ with modern targets
- **Dependency Manager**: Conan 2.x
- **HTTP Library**: libcurl 8.x (for network operations)
- **Terminal UI**: FTXUI (planned for Phase 3)
- **JSON Serialization**: nlohmann/json (planned for state persistence)
- **Email Notifications**: mailio (planned for optional feature)
- **Testing**: Catch2 (planned)
- **Compiler Support**: GCC/Clang (Linux), MinGW64/MSVC (Windows)
- **Formatting**: clang-format (planned)

## Project Conventions

### Code Style
- **Standard**: Pure C++17, no compiler-specific extensions (`CMAKE_CXX_EXTENSIONS OFF`)
- **Naming Conventions**:
  - Classes: `PascalCase` (e.g., `HttpClient`, `ThreadPool`)
  - Functions/methods: `camelCase` (e.g., `downloadFile()`, `getLastError()`)
  - Member variables: `camelCase_` with trailing underscore (e.g., `curl_`, `lastError_`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_RETRIES`)
  - Files: `snake_case` (e.g., `http_client.hpp`, `thread_pool.cpp`)
- **Warnings**: Strict (`-Wall -Wextra -Wpedantic` on GCC/Clang, `/W4 /permissive-` on MSVC)
- **Header Guards**: `#pragma once` (modern, widely supported)
- **Includes**: Separate with blank lines: C++ standard library, third-party, project headers
- **Formatting**: 4-space indentation, 100-character line limit (to be enforced with clang-format)

### Architecture Patterns
- **RAII (Resource Acquisition Is Initialization)**: Core principle for all resource management
  - Use `std::unique_ptr` with custom deleters for C APIs (e.g., `CURL*`)
  - File handles managed by `std::fstream` (auto-close on destruction)
  - Locks always use RAII wrappers (`std::lock_guard`, `std::unique_lock`)
- **Dependency Injection**: Constructor injection for testability (e.g., passing file paths, config objects)
- **Single Responsibility**: Each class has one clear purpose (e.g., `HttpClient` only handles HTTP, not threading)
- **Manual Threading**: Implement concurrency primitives from scratch using C++17 standard library
  - Thread pools built with `std::thread`, not third-party abstractions
  - Synchronization via `std::mutex`, `std::condition_variable`, `std::atomic`
- **Progressive Complexity**: Start simple (single-threaded), add features incrementally
- **Performance-Conscious**: 
  - Release builds for performance testing (`CMAKE_BUILD_TYPE=Release`)
  - Separate debug builds with symbols (`build-debug/` directory)

### Testing Strategy
- **Unit Testing**: Catch2 framework (to be implemented in later phases)
- **Manual Validation**: Each task includes specific test commands with expected outputs
- **Test URLs**: Use `httpbin.org` for predictable HTTP testing scenarios
  - `/bytes/N`: Download N bytes of random data
  - `/status/CODE`: Test HTTP error handling (404, 500, etc.)
  - `/delay/N`: Test timeout and retry logic
- **Debug Workflow**:
  - Use `gdb`/`lldb` for debugging (or VS Code with CMake Tools extension)
  - Valgrind/AddressSanitizer for memory leak detection (planned)
- **Concurrency Testing**: Test race conditions, deadlocks, and thread safety as concurrency is introduced

### Git Workflow
- **Branch Strategy**: Feature branches merged to `main`
- **Commit Conventions**: Descriptive messages referencing task IDs
  - Example: `TASK-002: Integrate libcurl with synchronous download`
- **Build Directories**: `build/`, `build-debug/`, `build-release/` are gitignored
- **OpenSpec Integration**: All major changes tracked via OpenSpec proposals in `openspec/changes/`

## Domain Context

### Download Manager Fundamentals
- **HTTP Range Requests**: `Range: bytes=N-` header enables resumable downloads
  - Server responds with 206 (Partial Content) if supported, 200 (OK) if not
  - Check `Content-Range` response header for actual range served
- **File States**: Downloads progress through states: Queued → Downloading → Paused/Completed/Failed
- **Chunk Management**: Large files split into chunks (e.g., 1MB) for parallel downloading
- **Progress Tracking**: libcurl's `CURLOPT_XFERINFOFUNCTION` callback provides real-time progress
- **Bandwidth Throttling**: Control download speed using `CURLOPT_MAX_RECV_SPEED_LARGE`
- **Connection Pooling**: Reuse TCP connections for multiple requests (planned advanced feature)

### Network Error Handling
- **Transient Errors** (retry with backoff):
  - `CURLE_OPERATION_TIMEDOUT`, `CURLE_COULDNT_CONNECT`, `CURLE_RECV_ERROR`
  - HTTP 5xx errors (server-side issues)
- **Permanent Errors** (fail immediately):
  - `CURLE_HTTP_RETURNED_ERROR` with 4xx codes (client errors like 404, 403)
  - Invalid URLs, SSL certificate failures
- **Retry Strategy**: Exponential backoff (1s, 2s, 4s, 8s...) with configurable max attempts

### Performance Considerations
- **Debug vs Release**: Debug builds are 5-10x slower; always benchmark in Release mode
- **Optimal Chunk Sizes**: 
  - libcurl default: 16KB (suboptimal for large files)
  - Recommended: 256KB-1MB for high-throughput downloads
- **Progress Callback Frequency**: Called ~100Hz; keep callbacks lightweight (no I/O)
- **SHA-256 Throughput**: ~500 MB/s on modern CPUs; adds ~2s per 1GB file
- **Thread Pool Sizing**: Start with `std::thread::hardware_concurrency()`, tune based on I/O vs CPU workload

## Important Constraints

### Technical Constraints
- **C++17 Only**: No C++20 features (wider toolchain compatibility, especially Windows MinGW)
- **Cross-Platform**: Must build and run on Linux (GCC/Clang) and Windows (MinGW64/MSVC)
- **No Boost**: Use C++17 standard library for learning; Boost.Asio avoided to understand manual threading
- **Manual Concurrency**: Thread pools, synchronization primitives implemented from scratch (no third-party thread libraries)
- **Memory Safety**: Zero memory leaks verified with Valgrind/AddressSanitizer
- **Build Time**: FetchContent for dependencies may slow initial build; Conan preferred for faster rebuilds

### Learning Constraints
- **Incremental Complexity**: Each task ~2-3 hours, self-contained with clear acceptance criteria
- **Core Features Manual**: Threading, HTTP logic, download management coded by hand
- **Utilities via Libraries**: Email, TUI, JSON use battle-tested libraries (focus on integration, not reimplementation)
- **Performance Awareness**: Each task includes performance hints and trade-offs

### Operational Constraints
- **HTTPS Mandatory**: SSL certificate validation always enabled (`CURLOPT_SSL_VERIFYPEER=1`)
- **Disk Space Checks**: Verify available space before starting large downloads
- **Graceful Shutdown**: Handle Ctrl+C gracefully, persist download state
- **State Persistence**: Save `.part` files and state to survive application restarts

## External Dependencies

### Core Dependencies (via Conan)
- **fmt 10.2.1**: Fast, type-safe string formatting (alternative to `<iostream>`)
- **libcurl 8.x**: HTTP/HTTPS client library
  - Options: `with_ssl=openssl` (for HTTPS on Linux)
  - Minimal configuration: SSH, Brotli, Zstd, HTTP/2 disabled initially

### Planned Dependencies (Later Phases)
- **FTXUI**: Modern, reactive terminal UI framework (Phase 3: TUI)
- **nlohmann/json**: Header-only JSON library (Phase 4: State persistence)
- **mailio**: SMTP email client (Phase 5: Notifications)
- **Catch2**: Unit testing framework (Phase 2-3: Testing infrastructure)
- **spdlog** (optional): Structured logging alternative to custom logger

### System Dependencies
- **OpenSSL**: Required by libcurl for HTTPS support
  - Linux: `libssl-dev`, `ca-certificates`
  - Windows: Bundled by Conan
- **CMake 3.16+**: Build system
- **Conan 2.x**: Package manager
- **GCC 7+/Clang 5+/MSVC 2017+**: C++17 compiler support

### External Services (for Testing)
- **httpbin.org**: HTTP request/response testing service
  - `/bytes/N`: Generate N bytes of random data
  - `/status/CODE`: Return specific HTTP status codes
  - `/delay/N`: Delay response by N seconds
- **releases.ubuntu.com**: Real-world large files for testing resumable downloads
