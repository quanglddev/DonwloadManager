# Tasks: Multithreaded Download Manager Implementation

> **Learning Philosophy**: Each task is designed as a self-contained day's work (~2-3 hours) with clear goals, hints, and validation criteria. Tasks build progressively, introducing new concepts while reinforcing previous knowledge.

---

## Phase 1: Foundations (Days 1-10)

### TASK-001: Project Scaffolding and Build System
**Priority**: P0 (Blocker)
**Estimated Time**: 2 hours
**Complexity**: Low

**Goal**: Set up CMake project with dependency management and create "Hello World" application.

**Acceptance Criteria**:
- [x] CMakeLists.txt configures C++17 standard
- [x] vcpkg or Conan configured for dependency management
- [x] Project builds without warnings
- [x] Executable runs and prints "Download Manager v1.0"
- [x] Build works on Windows (MinGW64) and/or Linux

**Approach**:
1. Create `CMakeLists.txt` with C++17 requirements
2. Configure vcpkg/Conan (choose one based on ecosystem familiarity)
3. Set up directory structure: `src/`, `include/`, `tests/`
4. Write minimal `main.cpp` with version output
5. Verify build with `cmake -B build && cmake --build build`

**Learning Focus**:
- CMake basics: `project()`, `add_executable()`, `target_link_libraries()`
- Dependency management fundamentals
- Cross-platform build considerations

**Hints**:
- Use `FetchContent` in CMake for fetching libraries (simpler than vcpkg for learning)
- Set `CMAKE_EXPORT_COMPILE_COMMANDS=ON` for editor integration (VS Code, CLion)
- **Performance Note**: Debug builds are slow; use `CMAKE_BUILD_TYPE=Release` for testing download speeds

**Validation**:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/download_manager  # Should print version
```

---

### TASK-002: Integrate libcurl - Synchronous Download
**Priority**: P0 (Blocker)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Download a single file using libcurl's easy interface.

**Acceptance Criteria**:
- [x] libcurl integrated via CMake FetchContent
- [x] Download a test file (e.g., `http://httpbin.org/bytes/1048576` - 1MB)
- [x] File saved to disk with correct size
- [x] HTTP errors (404, 500) are detected and reported
- [x] HTTPS URLs work with certificate validation

**Approach**:
1. Add libcurl to `CMakeLists.txt` using FetchContent
2. Create `HttpClient` class wrapping `CURL*` handle (RAII pattern)
3. Implement `downloadFile(url, destination)` using `curl_easy_*` functions
4. Set up write callback: `CURLOPT_WRITEFUNCTION` to stream data to file
5. Test with various URLs and error conditions

**Learning Focus**:
- RAII: Use constructor/destructor for `curl_easy_init()` / `curl_easy_cleanup()`
- Callbacks: How C-style callbacks work with C++ member functions (static trampolines)
- Error handling: Check `CURLcode` return values

**Hints**:
- Use `std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>` for automatic cleanup
- Write callback signature: `size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)`
- **Performance Note**: Default libcurl buffer size is 16KB; optimal chunk size is 256KB-1MB for large files

**Validation**:
```bash
./download_manager https://httpbin.org/bytes/1048576 test.bin
ls -lh test.bin  # Should show ~1MB file
```

**Troubleshooting**:
- If HTTPS fails: `curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L)`
- If download is slow: Check if you're in Debug mode (use Release)

---

### TASK-003: Progress Tracking and Display
**Priority**: P0 (Blocker)
**Estimated Time**: 2.5 hours
**Complexity**: Low-Medium

**Goal**: Display real-time download progress in the terminal (plain text, no TUI yet).

**Acceptance Criteria**:
- [x] Progress displayed as percentage and bytes downloaded
- [x] Download speed calculated in KB/s or MB/s
- [x] Progress updates every second
- [x] ETA calculated based on current speed
- [x] Progress uses libcurl's built-in progress callback

**Approach**:
1. Set `CURLOPT_XFERINFOFUNCTION` callback for progress updates
2. In callback, calculate: percentage, speed (bytes/second), ETA
3. Print progress to stdout using `\r` (carriage return) for in-place updates
4. Use `std::chrono` for timing calculations
5. Format output: `[=====     ] 52% | 52.3MB / 100MB | 2.1MB/s | ETA: 23s`

**Learning Focus**:
- `std::chrono`: `steady_clock`, `time_point`, `duration`
- Callback context: Passing `this` pointer via `CURLOPT_XFERINFODATA`
- Formatting: `std::setprecision`, `std::fixed` for clean output

**Hints**:
- Progress callback signature: `int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)`
- Use a 1-second sliding window for speed calculation (avoids spikes)
- **Performance Note**: Progress callbacks are called frequently (~100Hz); keep them lightweight (no I/O)

**Validation**:
```bash
./download_manager https://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso ubuntu.iso
# Should show updating progress bar
```

---

### TASK-004: File I/O and Filesystem Operations
**Priority**: P0 (Blocker)
**Estimated Time**: 2 hours
**Complexity**: Low

**Goal**: Robust file handling with `<filesystem>`, create partial files, and handle edge cases.

**Acceptance Criteria**:
- [x] Check if destination directory exists; create if needed
- [x] Check available disk space before download
- [x] Create `.part` file for partial downloads
- [x] Rename `.part` to final filename on completion
- [x] Handle file permission errors gracefully

**Approach**:
1. Use `std::filesystem::path` for cross-platform path handling
2. Implement `ensureDirectoryExists(path)` using `std::filesystem::create_directories()`
3. Implement `checkDiskSpace(path, requiredBytes)` using `std::filesystem::space()`
4. Modify download to write to `destination + ".part"`
5. On success, `std::filesystem::rename(src, dst)`

**Learning Focus**:
- `<filesystem>`: Modern C++17 path and file operations
- Error handling: `std::filesystem::filesystem_error` exceptions
- RAII for file handles: `std::ofstream` with RAII guarantees closure

**Hints**:
- Always check `std::filesystem::space(path).available` before large downloads
- Use binary mode for file streams: `std::ofstream(path, std::ios::binary)`
- **Performance Note**: `std::ofstream` buffers writes; explicit `flush()` ensures data reaches disk

**Validation**:
```cpp
// Test edge cases
downloadFile("http://httpbin.org/bytes/1024", "/invalid/path/file.bin");  // Should fail gracefully
downloadFile("http://httpbin.org/bytes/999999999999", "huge.bin");  // Should check disk space
```

---

### TASK-005: HTTP Range Requests for Resumption
**Priority**: P0 (Blocker)
**Estimated Time**: 3 hours
**Complexity**: Medium-High

**Goal**: Implement resumable downloads using HTTP `Range` headers.

**Acceptance Criteria**:
- [x] Detect existing `.part` file and its size
- [x] Send `Range: bytes=N-` header to resume from byte N
- [x] Append new data to existing `.part` file
- [x] Handle servers that don't support ranges (restart download)
- [x] Verify final file size matches `Content-Length`

**Approach**:
1. Check if `destination.part` exists; get file size with `std::filesystem::file_size()`
2. Set libcurl range: `curl_easy_setopt(curl, CURLOPT_RANGE, "N-")`
3. Open file in append mode: `std::ofstream(path, std::ios::binary | std::ios::app)`
4. Check HTTP response code: 206 (Partial Content) = resumable, 200 = server doesn't support ranges
5. If 200, delete `.part` and restart from 0

**Learning Focus**:
- HTTP protocol: `Range` requests, `Content-Range` responses, status codes 200 vs 206
- File modes: Append vs truncate
- Conditional logic: Fallback strategies when features aren't available

**Hints**:
- Get response code: `curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code)`
- Get `Content-Length`: `curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size)`
- **Performance Note**: Always resume if possible; redownloading wastes bandwidth and time
- **Critical Thinking**: What happens if the server file changed between downloads? How could you detect this? (Hint: `ETag` or `Last-Modified` headers)

**Validation**:
```bash
# Start download, then kill it mid-way (Ctrl+C)
./download_manager http://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso ubuntu.iso
# Resume - should continue from where it stopped
./download_manager http://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso ubuntu.iso
```

---

### TASK-006: Basic Error Handling and Retry Logic
**Priority**: P1 (High)
**Estimated Time**: 2.5 hours
**Complexity**: Medium

**Goal**: Handle transient network errors with exponential backoff retry.

**Acceptance Criteria**:
- [x] Distinguish transient errors (timeout, connection reset) from permanent errors (404, 403)
- [x] Retry transient errors up to 3 times with delays: 1s, 2s, 4s
- [x] Log each retry attempt with reason
- [x] Fail permanently after max retries exceeded
- [x] User-friendly error messages for common failures

**Approach**:
1. Create `enum class ErrorType { Transient, Permanent, Unknown }`
2. Implement `classifyError(CURLcode)` to categorize errors
3. Wrap download logic in retry loop with exponential backoff
4. Use `std::this_thread::sleep_for(std::chrono::seconds(1 << attempt))`
5. Log to stderr with timestamps

**Learning Focus**:
- Error categorization: Which errors are worth retrying?
- Exponential backoff: `2^attempt` pattern prevents server overload
- Logging: Timestamped messages for debugging

**Hints**:
- Transient: `CURLE_OPERATION_TIMEDOUT`, `CURLE_COULDNT_CONNECT`, `CURLE_RECV_ERROR`
- Permanent: `CURLE_HTTP_RETURNED_ERROR` (check if 4xx vs 5xx)
- **Performance Note**: Immediate retries can exacerbate server issues; always delay
- **Critical Thinking**: Should 5xx errors be retried? (Yes, usually temporary server issues)

**Validation**:
```bash
# Test with invalid URL (permanent failure)
./download_manager http://httpbin.org/status/404 test.bin  # Should fail immediately

# Test with timeout (transient, requires simulated network issue)
# Simulate: Use firewall/iptables to drop packets temporarily
```

---

### TASK-007: Command-Line Argument Parsing
**Priority**: P1 (High)
**Estimated Time**: 2 hours
**Complexity**: Low

**Goal**: Parse CLI arguments for URL, destination, and options.

**Acceptance Criteria**:
- [ ] Accept URL and destination as positional arguments
- [ ] Support optional flags: `--retry-count`, `--timeout`, `--output`
- [ ] Display help message with `--help` or `-h`
- [ ] Validate arguments (URL format, writable destination)
- [ ] Set defaults for optional parameters

**Approach**:
1. Use a simple argument parser (manual or library like CLI11/argparse)
2. Implement `parseArgs(int argc, char* argv[])` returning a config struct
3. Validate URL format with regex or basic checks (starts with `http`)
4. Validate destination is writable
5. Print usage message on errors

**Learning Focus**:
- Argument parsing patterns
- Validation: Fail fast with clear messages
- User experience: Helpful error messages

**Hints**:
- For simplicity, start with manual parsing: `if (strcmp(argv[i], "--help") == 0)`
- Later, refactor to use a library (CLI11 is header-only and modern)
- **Performance Note**: Argument parsing is one-time; prioritize clarity over speed

**Validation**:
```bash
./download_manager --help  # Shows usage
./download_manager http://example.com/file.zip output.zip --retry-count 5
./download_manager invalid_url output.zip  # Error: Invalid URL format
```

---

### TASK-008: Checksum Verification (SHA-256)
**Priority**: P1 (High)
**Estimated Time**: 2.5 hours
**Complexity**: Medium

**Goal**: Verify downloaded file integrity using SHA-256 checksums.

**Acceptance Criteria**:
- [ ] Compute SHA-256 hash of downloaded file
- [ ] Compare with expected hash (provided via CLI or sidecar file)
- [ ] Report verification success/failure
- [ ] Quarantine corrupted files to separate directory
- [ ] Support other hash algorithms (MD5, SHA-1) as optional

**Approach**:
1. Use OpenSSL (often available with libcurl) or standalone SHA-256 implementation
2. Read file in chunks (1MB) to compute hash (avoids loading entire file into memory)
3. Compare computed hash with expected (case-insensitive)
4. Move corrupted files to `./quarantine/` directory
5. Add CLI flag `--checksum sha256:abc123...`

**Learning Focus**:
- Streaming algorithms: Process data incrementally to save memory
- Cryptographic hashing basics
- Hex encoding/decoding

**Hints**:
- OpenSSL example: `EVP_DigestInit()`, `EVP_DigestUpdate()`, `EVP_DigestFinal()`
- Read file in 1MB chunks: `std::ifstream::read(buffer, 1024*1024)`
- **Performance Note**: SHA-256 on modern CPUs: ~500 MB/s; checksum adds ~2s per 1GB file
- **Critical Thinking**: When is checksum verification most important? (Large files, unreliable sources)

**Validation**:
```bash
# Download a file with known checksum
./download_manager http://httpbin.org/bytes/1024 test.bin --checksum sha256:<expected>
# Should report "Checksum verified successfully"

# Corrupt the file and verify again
echo "corrupted" >> test.bin
./download_manager http://httpbin.org/bytes/1024 test.bin --checksum sha256:<expected>
# Should detect corruption
```

---

### TASK-009: Configuration File Support
**Priority**: P2 (Medium)
**Estimated Time**: 2 hours
**Complexity**: Low-Medium

**Goal**: Load default settings from a configuration file (INI or JSON).

**Acceptance Criteria**:
- [ ] Support config file at `~/.config/download_manager/config.json`
- [ ] Load defaults: max retries, timeout, default download directory
- [ ] CLI arguments override config file settings
- [ ] Create default config on first run
- [ ] Validate config file syntax

**Approach**:
1. Use nlohmann/json library (add to CMake)
2. Implement `loadConfig(path)` returning a `Config` struct
3. Merge config file settings with CLI arguments (CLI takes precedence)
4. Use `std::filesystem` to locate config directory
5. Write default config if missing

**Learning Focus**:
- JSON parsing in C++ with nlohmann/json
- Configuration precedence: Defaults < Config File < CLI
- Cross-platform paths: `~/.config` on Linux, `%APPDATA%` on Windows

**Hints**:
- nlohmann/json is header-only, very intuitive: `json j = json::parse(file)`
- Use `std::getenv("HOME")` or `std::getenv("APPDATA")` for user directories
- **Performance Note**: Config loading is one-time; validation is more important than speed

**Validation**:
```bash
# First run creates default config
./download_manager http://example.com/file.zip output.zip
cat ~/.config/download_manager/config.json
# Edit config, run again - settings should apply
```

---

### TASK-010: Logging Infrastructure
**Priority**: P2 (Medium)
**Estimated Time**: 2 hours
**Complexity**: Low

**Goal**: Implement structured logging with log levels and file output.

**Acceptance Criteria**:
- [ ] Log levels: DEBUG, INFO, WARN, ERROR
- [ ] Timestamped log messages
- [ ] Log to both stderr and file (`~/.config/download_manager/download.log`)
- [ ] Configurable log level via CLI or config
- [ ] Thread-safe logging (preparation for multithreading)

**Approach**:
1. Create `Logger` singleton class
2. Implement log macros: `LOG_INFO("message")`, `LOG_ERROR("message")`
3. Format: `[2025-10-23 14:30:15] [INFO] message`
4. Write to file using `std::ofstream` (opened in append mode)
5. Protect with `std::mutex` for thread safety

**Learning Focus**:
- Singleton pattern in C++
- Variadic templates for log macros
- Thread-safe I/O with mutexes

**Hints**:
- Use `std::lock_guard<std::mutex>` to automatically lock/unlock
- Format timestamps with `std::put_time()`
- **Performance Note**: Logging to disk is slow; buffer writes or use async logging for high-frequency logs
- **Critical Thinking**: Should we log every progress update? (No, it would fill disk quickly)

**Validation**:
```cpp
LOG_INFO("Download started: {}", url);
LOG_ERROR("Failed to connect: {}", curl_easy_strerror(code));
// Check log file has formatted entries
```

---

## Phase 2: Concurrency Fundamentals (Days 11-20)

### TASK-011: Thread Pool - Basic Implementation
**Priority**: P0 (Blocker)
**Estimated Time**: 4 hours
**Complexity**: High

**Goal**: Implement a thread pool with fixed number of worker threads and a task queue.

**Acceptance Criteria**:
- [ ] Create `ThreadPool` class with N worker threads
- [ ] Implement `enqueue(task)` method to submit tasks
- [ ] Workers pick tasks from queue and execute them
- [ ] Thread-safe queue using `std::mutex` and `std::condition_variable`
- [ ] Graceful shutdown that waits for tasks to complete

**Approach**:
1. Create `ThreadPool(size_t numThreads)` constructor spawning workers
2. Each worker runs: `while (!shutdown_) { task = queue.pop(); task(); }`
3. Implement thread-safe queue with mutex + condition variable
4. `enqueue()` locks mutex, pushes task, notifies one worker
5. `~ThreadPool()` sets `shutdown_`, notifies all, joins threads

**Learning Focus**:
- `std::thread`: Creation, joining, detaching
- `std::mutex` and `std::lock_guard`: Protecting shared data
- `std::condition_variable`: Worker synchronization (wait/notify)
- Producer-consumer pattern

**Hints**:
- Use `std::function<void()>` for task type (type-erased callable)
- Worker loop: `cv_.wait(lock, [this]{ return !queue_.empty() || shutdown_; })`
- **Performance Note**: Thread creation is expensive (~1ms each); create pool once
- **Critical Thinking**: What's the optimal thread count? (Start with `std::thread::hardware_concurrency()`, but I/O-bound tasks benefit from more threads than CPU cores)

**Validation**:
```cpp
ThreadPool pool(4);
for (int i = 0; i < 10; ++i) {
    pool.enqueue([i] {
        std::cout << "Task " << i << " on thread " << std::this_thread::get_id() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });
}
// Destructor waits for all tasks to complete
```

**Troubleshooting**:
- Deadlock? Check lock ordering
- Tasks not running? Verify `notify_one()` is called after unlocking
- Crashes? Ensure threads are joined before destroying queue

---

### TASK-012: Thread Pool - Future/Promise Integration
**Priority**: P1 (High)
**Estimated Time**: 2.5 hours
**Complexity**: Medium-High

**Goal**: Return `std::future` from `enqueue()` to retrieve task results.

**Acceptance Criteria**:
- [ ] `enqueue()` returns `std::future<ReturnType>`
- [ ] Tasks can return values or throw exceptions
- [ ] Calling `future.get()` retrieves result or rethrows exception
- [ ] Support for `void` return type

**Approach**:
1. Use `std::packaged_task<ReturnType()>` to wrap task
2. Get `std::future` from packaged_task before moving into queue
3. Queue stores `std::function<void()>` (wraps packaged_task)
4. Worker executes function, which runs packaged_task and sets promise

**Learning Focus**:
- `std::future` / `std::promise`: Async result communication
- `std::packaged_task`: Wrapping callable with future
- Move semantics: Transferring ownership into queue

**Hints**:
- Template signature: `template<typename Func> auto enqueue(Func&& f) -> std::future<decltype(f())>`
- Use `std::make_shared<std::packaged_task<...>>` to share ownership
- **Performance Note**: `future.get()` blocks until result is ready; use for synchronization
- **Critical Thinking**: When should you use futures vs callbacks? (Futures for synchronous waits, callbacks for async notifications)

**Validation**:
```cpp
ThreadPool pool(2);
auto future = pool.enqueue([] { return 42; });
int result = future.get();  // Blocks until task completes
assert(result == 42);
```

---

### TASK-013: Download Task Abstraction
**Priority**: P0 (Blocker)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Create `DownloadTask` class encapsulating a single download with state management.

**Acceptance Criteria**:
- [ ] `DownloadTask` stores URL, destination, progress, state (queued/downloading/paused/completed/failed)
- [ ] State transitions are thread-safe
- [ ] Progress is atomically readable
- [ ] Task can be paused/resumed/canceled
- [ ] Task integrates with `HttpClient` from earlier tasks

**Approach**:
1. Create `DownloadTask` class with state enum and atomic state variable
2. Use `std::atomic<State>` for lock-free state reads
3. Use `std::mutex` for state transitions (multi-step operations)
4. Implement `start()`, `pause()`, `resume()`, `cancel()` methods
5. Store progress in `std::atomic<size_t> bytesDownloaded_`

**Learning Focus**:
- `std::atomic`: Lock-free programming basics
- State machines: Valid transitions (e.g., can't pause if already completed)
- Separation of concerns: Task logic vs HTTP logic

**Hints**:
- Use `std::atomic<State>` for reads, but lock mutex for transitions (compare-and-swap pattern)
- Check state before operations: `if (state_ != State::Downloading) return;`
- **Performance Note**: Atomic loads are very fast (~1 cycle); use liberally for progress reads
- **Critical Thinking**: Which operations need locks vs atomics? (Reads → atomic; multi-step transitions → mutex)

**Validation**:
```cpp
DownloadTask task("http://httpbin.org/bytes/1024", "test.bin");
task.start();
// While downloading...
task.pause();
assert(task.getState() == DownloadTask::State::Paused);
task.resume();
```

---

### TASK-014: Parallel Downloads - Multiple Tasks
**Priority**: P0 (Blocker)
**Estimated Time**: 3 hours
**Complexity**: Medium-High

**Goal**: Download multiple files concurrently using thread pool.

**Acceptance Criteria**:
- [ ] Download 5 files simultaneously
- [ ] Each download runs on a worker thread
- [ ] Progress for all downloads is tracked independently
- [ ] Downloads can be added while others are in progress
- [ ] Limit concurrent downloads (e.g., max 4 active)

**Approach**:
1. Create `DownloadManager` class managing a list of `DownloadTask`s
2. Maintain queue of tasks (pending) and set of active tasks
3. When thread becomes available, dequeue next task and submit to thread pool
4. On task completion, start next queued task
5. Implement `addDownload(url, dest)` method

**Learning Focus**:
- Work queue management
- Coordination between producer (adding tasks) and consumers (workers)
- Resource limits (max concurrent downloads)

**Hints**:
- Use a separate thread or timer to monitor completed tasks and start queued ones
- Track active count: `std::atomic<int> activeCount_`
- **Performance Note**: More concurrent downloads != faster overall; network and disk are bottlenecks
- **Critical Thinking**: What's the optimal concurrency for a 100Mbps connection downloading from a single server? (1-2 connections; more adds overhead without benefit. Multiple servers benefit from more connections)

**Validation**:
```cpp
DownloadManager manager(4);  // Max 4 concurrent
manager.addDownload("http://example.com/file1.zip", "file1.zip");
manager.addDownload("http://example.com/file2.zip", "file2.zip");
// ... add 10 downloads total
manager.start();
manager.waitForCompletion();
```

---

### TASK-015: Pause/Resume Across Threads
**Priority**: P1 (High)
**Estimated Time**: 2.5 hours
**Complexity**: Medium-High

**Goal**: Safely pause and resume downloads running on worker threads.

**Acceptance Criteria**:
- [ ] Pausing a download stops it within 5 seconds
- [ ] Paused downloads save progress to disk
- [ ] Resuming restarts download from saved position
- [ ] Multiple downloads can be paused/resumed independently
- [ ] No race conditions between pause signal and download loop

**Approach**:
1. Download loop checks `state_` periodically (every chunk)
2. `pause()` sets state to `Paused` and waits for loop to exit
3. Use `std::condition_variable` for main thread to wait for pause confirmation
4. `resume()` re-enqueues task to thread pool
5. Save `.part` file and offset on pause

**Learning Focus**:
- Cooperative cancellation: Polling a flag vs interrupting threads
- Synchronization between control thread and worker thread
- Wait-with-timeout patterns

**Hints**:
- Check state: `if (state_.load() == State::Paused) break;`
- Wait for pause: `cv_.wait_for(lock, 5s, [this]{ return state_ == State::Paused; })`
- **Performance Note**: Check state every chunk (256KB); checking more frequently adds overhead
- **Critical Thinking**: Why not forcibly kill the thread? (Resources leak, file corruption, no cleanup)

**Validation**:
```cpp
DownloadTask task("http://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso", "ubuntu.iso");
pool.enqueue([&task] { task.start(); });
std::this_thread::sleep_for(5s);
task.pause();  // Should complete within 5 seconds
task.resume();
```

---

### TASK-016: Thread-Safe Progress Reporting
**Priority**: P1 (High)
**Estimated Time**: 2 hours
**Complexity**: Medium

**Goal**: UI thread reads progress from worker threads without blocking or data races.

**Acceptance Criteria**:
- [ ] UI polls progress at 10Hz (every 100ms)
- [ ] Progress reads don't block download threads
- [ ] Progress is always consistent (no torn reads)
- [ ] Speed and ETA calculated in UI thread

**Approach**:
1. Store progress in `std::atomic<size_t> bytesDownloaded_`
2. UI thread calls `task.getProgress()` which returns atomic snapshot
3. UI thread calculates speed: `(current - previous) / elapsed_time`
4. Avoid mutexes in progress reads (use atomics only)

**Learning Focus**:
- Lock-free reads with atomics
- Separation of data producers (workers) and consumers (UI)
- Sampling and rate calculation

**Hints**:
- Use `std::atomic::load(std::memory_order_relaxed)` for progress (relaxed ordering is fine here)
- UI thread maintains previous progress and timestamp for speed calculation
- **Performance Note**: Atomic loads are nearly free; reading progress 10x/second is negligible overhead
- **Critical Thinking**: Why is `memory_order_relaxed` safe here? (Progress is monotonic; we don't need strict ordering)

**Validation**:
```cpp
// UI thread loop
while (!allCompleted) {
    for (auto& task : tasks) {
        auto progress = task.getProgress();  // Non-blocking
        std::cout << task.getFilename() << ": " << progress.percentage << "%\n";
    }
    std::this_thread::sleep_for(100ms);
}
```

---

### TASK-017: Deadlock Detection and Prevention
**Priority**: P2 (Medium)
**Estimated Time**: 2 hours
**Complexity**: Medium

**Goal**: Implement strategies to prevent and detect deadlocks.

**Acceptance Criteria**:
- [ ] All mutexes acquired in consistent order
- [ ] Use `std::lock()` for acquiring multiple mutexes atomically
- [ ] Timeout-based lock acquisition in debug mode
- [ ] Logging when lock contention occurs
- [ ] No deadlocks in stress tests (1000 pause/resume cycles)

**Approach**:
1. Document lock hierarchy (e.g., DownloadManager lock before DownloadTask lock)
2. Use `std::scoped_lock` when locking multiple mutexes
3. Add `std::timed_mutex` in debug builds with timeout warnings
4. Stress test: Rapidly pause/resume downloads from multiple threads
5. Use ThreadSanitizer (TSan) to detect race conditions

**Learning Focus**:
- Lock ordering principles
- `std::scoped_lock` for deadlock-free multi-lock acquisition
- Debugging tools: TSan, Helgrind

**Hints**:
- Lock order example: Always lock `DownloadManager::mutex_` before any `DownloadTask::mutex_`
- TSan: `cmake -DCMAKE_CXX_FLAGS=-fsanitize=thread`
- **Performance Note**: Lock contention shows up in profiling as high wait times
- **Critical Thinking**: Can atomics replace all mutexes? (No, multi-step operations need mutexes)

**Validation**:
```bash
# Compile with TSan
cmake -B build -DCMAKE_CXX_FLAGS=-fsanitize=thread
cmake --build build
./build/bin/download_manager_test  # Run stress tests
# TSan should report zero data races
```

---

### TASK-018: Exception Safety in Concurrent Code
**Priority**: P1 (High)
**Estimated Time**: 2.5 hours
**Complexity**: Medium-High

**Goal**: Ensure threads handle exceptions without crashing or leaking resources.

**Acceptance Criteria**:
- [ ] Exceptions in worker threads are caught and logged
- [ ] Worker threads don't terminate on exceptions
- [ ] Download state reflects error if exception occurs
- [ ] Resources (file handles, memory) are cleaned up via RAII
- [ ] `ThreadPool` survives task exceptions

**Approach**:
1. Wrap task execution in `try-catch` block in worker loop
2. Catch `std::exception` and log error message
3. Use RAII for all resources (files, mutexes, curl handles)
4. Set task state to `Failed` with error message
5. Test with artificially thrown exceptions

**Learning Focus**:
- Exception safety guarantees (basic, strong, no-throw)
- RAII: Destructors called even during stack unwinding
- Thread-local exception handling

**Hints**:
- Worker loop: `try { task(); } catch (const std::exception& e) { LOG_ERROR("Task failed: {}", e.what()); }`
- Use `std::unique_ptr` for automatic cleanup
- **Performance Note**: Exceptions are slow; use for errors, not control flow
- **Critical Thinking**: Should we retry tasks that throw exceptions? (Depends on error; network errors yes, logic errors no)

**Validation**:
```cpp
pool.enqueue([] {
    throw std::runtime_error("Test exception");
});
// Pool should log error and continue processing other tasks
pool.enqueue([] { std::cout << "This should still run\n"; });
```

---

### TASK-019: Cancellation and Cleanup
**Priority**: P1 (High)
**Estimated Time**: 2 hours
**Complexity**: Medium

**Goal**: Cleanly cancel downloads and clean up partial files.

**Acceptance Criteria**:
- [ ] `cancel()` stops download and deletes `.part` file
- [ ] Cancellation completes within 5 seconds
- [ ] Canceled downloads are removed from manager
- [ ] No zombie threads or leaked resources
- [ ] Can cancel all downloads with single command

**Approach**:
1. `cancel()` sets state to `Canceled` and signals worker thread
2. Worker checks state and exits loop early
3. On exit, cleanup code deletes `.part` file
4. `DownloadManager::cancelAll()` iterates and cancels each task
5. Use RAII to ensure cleanup even if exceptions occur

**Learning Focus**:
- Resource cleanup patterns
- Bulk operations (cancel all)
- Graceful shutdown strategies

**Hints**:
- Use `std::filesystem::remove()` to delete files
- Ensure `.part` deletion happens in destructor or `finally` equivalent
- **Performance Note**: Cancellation should be fast; avoid blocking operations
- **Critical Thinking**: Should we keep `.part` files after cancel? (Optional: let user decide via flag)

**Validation**:
```cpp
manager.addDownload("http://releases.ubuntu.com/.../ubuntu.iso", "ubuntu.iso");
manager.start();
std::this_thread::sleep_for(5s);
manager.cancel("ubuntu.iso");
// .part file should be deleted
assert(!std::filesystem::exists("ubuntu.iso.part"));
```

---

### TASK-020: Concurrency Stress Testing
**Priority**: P2 (Medium)
**Estimated Time**: 2.5 hours
**Complexity**: Medium

**Goal**: Validate thread safety with stress tests and sanitizers.

**Acceptance Criteria**:
- [ ] Download 100 files concurrently with frequent pause/resume
- [ ] No crashes, deadlocks, or data corruption
- [ ] Pass ThreadSanitizer (TSan) checks
- [ ] Pass AddressSanitizer (ASan) checks
- [ ] Measure and log lock contention

**Approach**:
1. Create test suite with Catch2 or Google Test
2. Implement stress test: 100 downloads, random pause/resume/cancel
3. Compile with TSan and run tests
4. Compile with ASan and run tests
5. Use profiler to identify lock contention hotspots

**Learning Focus**:
- Testing concurrent code
- Sanitizers: TSan (race conditions), ASan (memory errors)
- Profiling: `perf`, `gprof`, or platform-specific tools

**Hints**:
- TSan: `-fsanitize=thread`
- ASan: `-fsanitize=address`
- Stress test pattern: `for (int i = 0; i < 1000; ++i) { /* pause/resume random task */ }`
- **Performance Note**: Sanitizers add 2-5x slowdown; use in testing, not production
- **Critical Thinking**: How do you test for race conditions? (Stress tests, sanitizers, code review)

**Validation**:
```bash
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=thread"
cmake --build build
./build/bin/download_manager_test --stress
# Should complete without TSan warnings
```

---

## Phase 3: Advanced Features (Days 21-35)

### TASK-021: Bandwidth Throttling - Token Bucket Algorithm
**Priority**: P1 (High)
**Estimated Time**: 4 hours
**Complexity**: High

**Goal**: Implement token bucket algorithm for per-file bandwidth limiting.

**Acceptance Criteria**:
- [ ] Limit download speed to specified bytes/second
- [ ] Actual speed within ±5% of target over 10 seconds
- [ ] Support burst (2x rate) for short periods
- [ ] Thread-safe for concurrent downloads
- [ ] Minimal CPU overhead

**Approach**:
1. Create `BandwidthLimiter` class with token bucket
2. Tokens accumulate at `bytesPerSecond` rate
3. `consume(bytes)` blocks if insufficient tokens
4. Bucket capacity = 2 × bytesPerSecond (2-second burst)
5. Use `std::chrono::steady_clock` for timing

**Learning Focus**:
- Token bucket algorithm: refill rate, capacity, consumption
- Precise timing with `std::chrono`
- Blocking with calculated sleep durations

**Hints**:
- Tokens = `min(capacity, tokens + elapsed * rate)`
- If `tokens < bytes`, sleep for `(bytes - tokens) / rate` seconds
- **Performance Note**: Check throttle every 64-256KB to balance overhead vs accuracy
- **Critical Thinking**: Why token bucket over fixed-rate sleep? (Allows bursts, smoother traffic, better for bursty networks)

**Validation**:
```cpp
BandwidthLimiter limiter(1024 * 1024);  // 1 MB/s
auto start = std::chrono::steady_clock::now();
for (int i = 0; i < 10; ++i) {
    limiter.consume(1024 * 1024);  // Consume 1MB
}
auto elapsed = std::chrono::steady_clock::now() - start;
// Should take ~10 seconds
assert(elapsed >= 9s && elapsed <= 11s);
```

---

### TASK-022: Global Bandwidth Management
**Priority**: P1 (High)
**Estimated Time**: 3 hours
**Complexity**: Medium-High

**Goal**: Enforce global bandwidth limit across all downloads.

**Acceptance Criteria**:
- [ ] Total download speed respects global limit
- [ ] Bandwidth fairly distributed among active downloads
- [ ] Dynamic reallocation when downloads complete
- [ ] Per-file limits can coexist with global limit
- [ ] Accurate to ±5% over 10 seconds

**Approach**:
1. Create shared `GlobalBandwidthLimiter` instance
2. Each `DownloadTask` consumes from both per-file and global limiters
3. Enforcement: `min(perFileLimit, globalLimit / activeDownloads)`
4. Update allocation when downloads start/stop
5. Use mutex to protect active download count

**Learning Focus**:
- Shared resource management
- Fair allocation algorithms
- Coordination between multiple limiters

**Hints**:
- Global limiter is shared: `std::shared_ptr<BandwidthLimiter> globalLimiter_`
- Before consuming: `globalLimiter_->consume(bytes); perFileLimiter_->consume(bytes);`
- **Performance Note**: Coordinating global limit adds contention; use coarse-grained locking (every 256KB)
- **Critical Thinking**: How to handle priority? (Weighted fair sharing: high-priority downloads get larger share)

**Validation**:
```cpp
GlobalBandwidthLimiter global(5 * 1024 * 1024);  // 5 MB/s total
DownloadTask task1(..., global);
DownloadTask task2(..., global);
// Both download simultaneously; total should be ~5 MB/s
```

---

### TASK-023: FTXUI Integration - Static Layout
**Priority**: P0 (Blocker)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Create basic terminal UI with FTXUI showing download list.

**Acceptance Criteria**:
- [ ] Display list of downloads with filenames
- [ ] Show progress bars for each download
- [ ] Display download states (downloading, paused, completed)
- [ ] Layout fits in 80x24 terminal
- [ ] Renders without flicker

**Approach**:
1. Add FTXUI to CMake via FetchContent
2. Create main UI loop with FTXUI `ScreenInteractive`
3. Build component tree: Container with multiple rows (one per download)
4. Each row: filename, progress bar, percentage, speed
5. Render at 10fps

**Learning Focus**:
- FTXUI component model: `Element`, `Component`, `Renderer`
- Declarative UI: Describe what to show, not how to draw
- Layout primitives: `hbox`, `vbox`, `gauge`

**Hints**:
- Progress bar: `gauge(progress)` where `progress` is 0.0 to 1.0
- Layout: `vbox({ row1, row2, row3 })`
- **Performance Note**: FTXUI is efficient; 10fps is smooth for terminal UI
- **Critical Thinking**: Terminal vs GUI trade-offs? (Terminal: simpler, remote-friendly, lightweight)

**Validation**:
```cpp
auto screen = ScreenInteractive::FitComponent();
auto component = Renderer([&] {
    return vbox({
        text("Download Manager"),
        gauge(0.5) | border,
        text("file.zip: 50%")
    });
});
screen.Loop(component);
```

---

### TASK-024: FTXUI - Live Progress Updates
**Priority**: P0 (Blocker)
**Estimated Time**: 3 hours
**Complexity**: Medium-High

**Goal**: Update UI in real-time as downloads progress.

**Acceptance Criteria**:
- [ ] UI updates at 10fps
- [ ] Progress bars animate smoothly
- [ ] Speed and ETA update every second
- [ ] No race conditions between UI and download threads
- [ ] UI remains responsive

**Approach**:
1. UI thread polls `DownloadManager` for progress every 100ms
2. Use atomics for progress reads (no mutexes in hot path)
3. Calculate speed in UI thread: `(current - previous) / 0.1s`
4. Use FTXUI's `Loop()` with custom event loop
5. Post screen refresh events at 10Hz

**Learning Focus**:
- Polling vs event-driven UI updates
- Thread separation: UI thread vs download threads
- Rate calculation and smoothing

**Hints**:
- FTXUI loop: `screen.Loop(component, 100ms)` for 10fps
- Access download state: `auto progress = manager.getProgress(taskId);`
- **Performance Note**: Polling every 100ms is low overhead; progress reads are atomic
- **Critical Thinking**: Why polling instead of callbacks? (Simpler for learning; callbacks can be added later for efficiency)

**Validation**:
```cpp
// Start downloads in background
std::thread([&] { manager.start(); }).detach();
// UI loop
screen.Loop(component, 100ms);
```

---

### TASK-025: FTXUI - Keyboard Controls
**Priority**: P1 (High)
**Estimated Time**: 2.5 hours
**Complexity**: Medium

**Goal**: Add keyboard controls for pause, resume, cancel, and navigation.

**Acceptance Criteria**:
- [ ] Arrow keys navigate download list
- [ ] 'P' pauses selected download
- [ ] 'R' resumes selected download
- [ ] 'C' cancels selected download (with confirmation)
- [ ] 'A' prompts to add new download
- [ ] 'Q' quits application

**Approach**:
1. Use FTXUI's `CatchEvent()` to handle key presses
2. Maintain `selectedIndex` for current selection
3. Map keys to actions: `Event::ArrowDown` → `selectedIndex++`
4. Show confirmation dialog for destructive actions (cancel)
5. Input dialog for adding downloads

**Learning Focus**:
- Event handling in FTXUI
- Modal dialogs and input prompts
- State management (selected item)

**Hints**:
- Key handling: `component |= CatchEvent([&](Event e) { if (e == Event::Character('p')) { /*pause*/ } })`
- Confirmation dialog: `Modal(mainComponent, confirmDialog, showDialog)`
- **Performance Note**: Keyboard events are infrequent; no performance concerns
- **Critical Thinking**: Should actions be immediate or queued? (Pause/resume can be immediate; cancel should confirm)

**Validation**:
```cpp
// Navigate with arrow keys, select download, press 'P'
// Download should pause within 5 seconds
```

---

### TASK-026: State Persistence - Save/Load Downloads
**Priority**: P1 (High)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Save download state to JSON and restore on application restart.

**Acceptance Criteria**:
- [ ] Save state to `~/.config/download_manager/state.json`
- [ ] State saved every 30 seconds and on shutdown
- [ ] State includes: URL, destination, progress, state, checksum
- [ ] On startup, load state and resume downloads
- [ ] Atomic writes (temp file + rename)

**Approach**:
1. Extend `DownloadTask` with `toJson()` and `fromJson()` methods
2. `DownloadManager::saveState()` serializes all tasks to JSON
3. Write to `state.json.tmp`, then rename to `state.json`
4. On startup, `loadState()` deserializes and reconstructs tasks
5. Use timer thread to save every 30 seconds

**Learning Focus**:
- Serialization with nlohmann/json
- Atomic file writes (temp + rename pattern)
- Background timers with `std::thread`

**Hints**:
- JSON schema: `{"downloads": [{"url": "...", "destination": "...", "bytesDownloaded": 1024, ...}]}`
- Atomic write: `write(tmp) → sync(tmp) → rename(tmp, final)`
- **Performance Note**: JSON serialization is fast; saving every 30s is negligible
- **Critical Thinking**: What if power loss occurs mid-write? (Temp + rename ensures old state is never corrupted)

**Validation**:
```bash
# Start download, kill app (Ctrl+C), restart
./download_manager  # Should resume download from saved progress
```

---

### TASK-027: Auto-Retry on Network Disconnect
**Priority**: P1 (High)
**Estimated Time**: 3 hours
**Complexity**: Medium-High

**Goal**: Detect network failures and automatically retry downloads.

**Acceptance Criteria**:
- [ ] Detect network disconnection (connection refused, timeout)
- [ ] Pause all downloads when network is lost
- [ ] Periodically check for network recovery (every 10 seconds)
- [ ] Automatically resume downloads when network returns
- [ ] User notified of network status changes

**Approach**:
1. Classify errors: Network errors (retry) vs others (fail)
2. On network error, set `networkUnavailable` flag
3. Background thread pings a reliable host (e.g., `http://httpbin.org/get`) every 10s
4. When ping succeeds, clear flag and resume paused downloads
5. Show network status in UI

**Learning Focus**:
- Network error detection and classification
- Background monitoring threads
- Event-driven resumption

**Hints**:
- Network check: Simple HTTP GET to a known-good URL
- Retry logic: `if (networkUnavailable) { sleep(10s); continue; }`
- **Performance Note**: Network checks every 10s are low overhead
- **Critical Thinking**: How to distinguish network down from server down? (Ping multiple hosts; if all fail, likely local network issue)

**Validation**:
```bash
# Start download, disable network (airplane mode / ifconfig down), wait 10s
# Re-enable network, downloads should auto-resume within 10s
```

---

### TASK-028: Prevent File Corruption - Checksums and Locking
**Priority**: P1 (High)
**Estimated Time**: 2.5 hours
**Complexity**: Medium

**Goal**: Ensure file integrity with checksums and prevent concurrent writes.

**Acceptance Criteria**:
- [ ] Verify checksum after download completes
- [ ] Lock files to prevent multiple downloads to same destination
- [ ] Detect partial file corruption (checksum of `.part` file)
- [ ] Quarantine corrupted files
- [ ] Warn if download resuming from corrupted `.part` file

**Approach**:
1. Compute checksum of `.part` file on resume (optional, for paranoia)
2. Lock destination path in `DownloadManager` to prevent duplicate downloads
3. After download, verify full file checksum
4. If mismatch, move to `./quarantine/` and mark failed
5. Use `std::map<std::string, std::mutex>` for per-file locks

**Learning Focus**:
- File locking strategies
- Integrity verification points (in-progress, completion)
- Error recovery (quarantine vs delete vs retry)

**Hints**:
- Per-file lock: `std::unique_lock<std::mutex> lock(fileLocks_[destination])`
- Checksum during download: Update hash incrementally for efficiency
- **Performance Note**: Incremental hashing adds ~2% overhead; negligible
- **Critical Thinking**: Should we checksum `.part` files? (Only if resuming after long pause or from untrusted storage)

**Validation**:
```bash
# Corrupt a .part file manually
echo "garbage" >> file.zip.part
./download_manager http://example.com/file.zip file.zip  # Should detect corruption and restart
```

---

### TASK-029: Email Notifications - SMTP Integration
**Priority**: P2 (Medium)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Send email notifications on download completion using mailio.

**Acceptance Criteria**:
- [ ] Send email when download completes
- [ ] Email includes filename, size, duration, speed
- [ ] SMTP settings configurable (server, port, username, password)
- [ ] TLS/SSL support
- [ ] Test email functionality

**Approach**:
1. Add mailio library via vcpkg or CMake
2. Create `EmailNotifier` class with SMTP config
3. Implement `sendCompletionEmail(task)` method
4. Call from `DownloadManager` when task completes
5. Run email sending on background thread (non-blocking)

**Learning Focus**:
- SMTP protocol basics
- Library integration (mailio)
- Async notifications

**Hints**:
- mailio example: Create `message`, set `from`, `to`, `subject`, `content`, send via `smtp`
- SMTP config: `smtp.gmail.com:587` for Gmail (requires app password)
- **Performance Note**: Email sending is slow (1-5s); always run async
- **Critical Thinking**: Should we email on every download? (Allow user to configure; digest multiple completions)

**Validation**:
```cpp
EmailNotifier notifier("smtp.gmail.com", 587, "user@gmail.com", "password");
notifier.sendCompletionEmail(task);
// Check inbox for email
```

---

### TASK-030: Notification Queue - Async Email Delivery
**Priority**: P2 (Medium)
**Estimated Time**: 2.5 hours
**Complexity**: Medium

**Goal**: Queue notifications for async delivery to avoid blocking downloads.

**Acceptance Criteria**:
- [ ] Notifications queued when events occur
- [ ] Background thread processes queue
- [ ] Rate limiting (1 email per 5 seconds)
- [ ] Retry on SMTP failure
- [ ] Persist queue on shutdown

**Approach**:
1. Create `NotificationQueue` with thread-safe queue
2. Background worker thread dequeues and sends emails
3. Rate limit: Sleep 5 seconds between sends
4. On shutdown, save pending notifications to JSON
5. On startup, reload and send pending notifications

**Learning Focus**:
- Producer-consumer pattern (notifications)
- Rate limiting
- Persistent queues

**Hints**:
- Queue: `std::queue<Notification>` protected by `std::mutex`
- Worker loop: `while (true) { notif = queue.pop(); send(notif); sleep(5s); }`
- **Performance Note**: Email rate limiting prevents provider blocking
- **Critical Thinking**: What if emails fail repeatedly? (Exponential backoff, max retries, then discard)

**Validation**:
```cpp
// Complete 10 downloads rapidly
// Emails should be sent at ~1 per 5 seconds
```

---

### TASK-031: Settings UI Panel
**Priority**: P2 (Medium)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Add settings panel in TUI for configuring options.

**Acceptance Criteria**:
- [ ] 'S' key opens settings panel
- [ ] Configure: max concurrent downloads, global bandwidth, email settings
- [ ] Settings persist to config file
- [ ] Apply settings immediately (no restart required)
- [ ] Settings panel dismissible with Esc

**Approach**:
1. Create FTXUI modal dialog for settings
2. Show input fields for each setting
3. On save, update `DownloadManager` config
4. Write to `config.json` atomically
5. Use `Modal()` component for overlay

**Learning Focus**:
- Modal UI patterns
- Input validation and sanitization
- Live config updates

**Hints**:
- FTXUI input: `Input()` component for text fields
- Modal: `Modal(mainComponent, settingsComponent, showSettings)`
- **Performance Note**: Config updates are infrequent; no perf concerns
- **Critical Thinking**: Should all settings be hot-reloadable? (Most yes; some like thread count may require restart)

**Validation**:
```bash
# Press 'S', change max concurrent downloads from 4 to 8
# Active downloads should adjust to new limit within 5 seconds
```

---

### TASK-032: Enhanced Error Reporting
**Priority**: P2 (Medium)
**Estimated Time**: 2 hours
**Complexity**: Low-Medium

**Goal**: Improve error messages with actionable troubleshooting hints.

**Acceptance Criteria**:
- [ ] User-friendly error messages (no raw error codes)
- [ ] Suggest fixes for common errors (disk full, network down, invalid URL)
- [ ] Error log with timestamps and context
- [ ] UI shows last error for each download
- [ ] Errors persisted to `errors.log` file

**Approach**:
1. Map `CURLcode` to user-friendly messages
2. Add context: "Failed to download file.zip: Connection timeout (check your internet connection)"
3. Store last error in `DownloadTask`
4. Display in UI and log to file
5. Create `ErrorFormatter` utility class

**Learning Focus**:
- User experience: Error messages matter
- Error context: What, why, how to fix
- Logging best practices

**Hints**:
- Error map: `std::map<CURLcode, std::string> errorMessages_`
- Context: Include URL, filename, timestamp
- **Performance Note**: Error formatting is only on failures (rare); optimize for clarity
- **Critical Thinking**: Should errors be localized (i18n)? (Future enhancement; English is fine for now)

**Validation**:
```bash
./download_manager http://invalid.url.example file.bin
# Should show: "Failed: Could not resolve host (check URL and DNS)"
```

---

### TASK-033: Download History Tracking
**Priority**: P2 (Medium)
**Estimated Time**: 2 hours
**Complexity**: Low

**Goal**: Maintain history of completed downloads.

**Acceptance Criteria**:
- [ ] Record completed downloads with metadata
- [ ] History stored in `history.json` (max 1000 entries)
- [ ] View history in UI ('H' key)
- [ ] Filter history by date or filename
- [ ] Export history to CSV

**Approach**:
1. On download completion, append entry to history
2. History entry: `{url, destination, size, duration, completedAt}`
3. Load history on startup
4. UI panel to browse history
5. Limit to 1000 entries (FIFO eviction)

**Learning Focus**:
- Data persistence patterns
- UI for browsing lists
- Export formats (CSV)

**Hints**:
- Append to JSON array, then write atomically
- UI: Use FTXUI's `Menu()` component for scrollable list
- **Performance Note**: History appends are infrequent; batching not needed
- **Critical Thinking**: Should history include failed downloads? (Yes, for debugging)

**Validation**:
```bash
# Complete several downloads
# Press 'H' in UI, should show list of completed downloads
```

---

### TASK-034: Progress Animations and Visual Polish
**Priority**: P3 (Low)
**Estimated Time**: 2 hours
**Complexity**: Low

**Goal**: Add visual polish to TUI with animations and colors.

**Acceptance Criteria**:
- [ ] Smooth progress bar animations (not jumpy)
- [ ] Color-coded states (blue=downloading, green=completed, red=failed, yellow=paused)
- [ ] Spinner for "connecting" state
- [ ] Transitions (e.g., fade when download completes)
- [ ] Accessible in monochrome terminals

**Approach**:
1. Use FTXUI colors: `text("Downloading") | color(Color::Blue)`
2. Animated gauge: Interpolate progress for smooth animation
3. Spinner: Rotate through characters `|/-\`
4. Ensure symbols + text convey state (not just color)
5. Test in color and monochrome terminals

**Learning Focus**:
- UX: Visual feedback matters
- Accessibility: Color is not enough
- Animation techniques in terminal

**Hints**:
- Colors: `gauge(progress) | color(Color::Green)`
- Spinner: `text(spinner_chars[frame % 4])`
- **Performance Note**: Animations at 10fps are smooth; higher is unnecessary
- **Critical Thinking**: How much polish is worth it? (Diminishing returns; focus on clarity)

**Validation**:
```bash
# Run in color terminal, verify colors
# Run in monochrome terminal, verify symbols are clear
```

---

### TASK-035: Performance Profiling and Optimization
**Priority**: P2 (Medium)
**Estimated Time**: 3 hours
**Complexity**: Medium-High

**Goal**: Profile application and optimize hotspots.

**Acceptance Criteria**:
- [ ] Profile with `perf`, `gprof`, or Visual Studio Profiler
- [ ] Identify top 3 CPU hotspots
- [ ] Optimize at least one hotspot (e.g., reduce lock contention)
- [ ] Measure before/after performance
- [ ] Document optimizations in code comments

**Approach**:
1. Compile with profiling flags: `-pg` for gprof, or use `perf`
2. Run stress test: 100 concurrent downloads
3. Analyze profile: `gprof ./download_manager` or `perf report`
4. Optimize hotspots (e.g., reduce mutex scope, use atomics)
5. Re-profile and compare

**Learning Focus**:
- Profiling tools and workflows
- Identifying bottlenecks (CPU, I/O, locks)
- Optimization techniques

**Hints**:
- Common hotspots: Mutex contention, frequent allocations, checksum computation
- Quick wins: Reduce lock scope, use `std::string_view` instead of copying strings
- **Performance Note**: Profile in Release mode; Debug mode skews results
- **Critical Thinking**: When to optimize? (After correctness, when profiling shows issues)

**Validation**:
```bash
# Before optimization
perf stat ./download_manager_stress
# 100 downloads in 120 seconds

# After optimization
perf stat ./download_manager_stress
# 100 downloads in 100 seconds (20% faster)
```

---

## Phase 4: Polish and Extension (Days 36-50)

### TASK-036: Multi-Part Downloads (Advanced Resume)
**Priority**: P3 (Nice to Have)
**Estimated Time**: 4 hours
**Complexity**: High

**Goal**: Download different parts of a file in parallel for faster downloads.

**Acceptance Criteria**:
- [ ] Split large files into N chunks (e.g., 4)
- [ ] Download each chunk on a separate thread
- [ ] Reassemble chunks into final file
- [ ] Resume works for multi-part downloads
- [ ] Faster than single-threaded for large files

**Approach**:
1. Check if server supports ranges (`HEAD` request, check `Accept-Ranges`)
2. Split file into equal chunks: `[0-N], [N+1-2N], ...`
3. Create separate `DownloadTask` for each chunk
4. Write each chunk to separate `.part.0`, `.part.1` files
5. On completion, concatenate into final file

**Learning Focus**:
- Parallel I/O patterns
- File assembly techniques
- When parallelism helps (large files, high-latency connections)

**Hints**:
- Chunk size: `fileSize / numChunks`
- Range header: `bytes=0-1023`, `bytes=1024-2047`, etc.
- **Performance Note**: Benefits diminish beyond 4-8 chunks due to server limits
- **Critical Thinking**: When does multi-part help? (Large files, high bandwidth, low latency. Doesn't help for slow servers or small files)

**Validation**:
```bash
./download_manager http://releases.ubuntu.com/.../ubuntu.iso ubuntu.iso --parts 4
# Should be faster than single-threaded download
```

---

### TASK-037: Protocol Support - FTP
**Priority**: P3 (Nice to Have)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Add FTP protocol support using libcurl.

**Acceptance Criteria**:
- [ ] Download files from FTP URLs
- [ ] Support FTP authentication (username, password)
- [ ] Resume FTP downloads
- [ ] List FTP directory contents
- [ ] Passive mode support

**Approach**:
1. Detect FTP URLs: `ftp://`
2. Set libcurl options: `CURLOPT_URL`, `CURLOPT_USERPWD`
3. Use `CURLOPT_FTP_USE_EPSV` for passive mode
4. Resume works same as HTTP (range requests)
5. Directory listing: `CURLOPT_DIRLISTONLY`

**Learning Focus**:
- FTP protocol basics
- Libcurl's multi-protocol support
- Protocol-specific quirks

**Hints**:
- FTP resume: `CURLOPT_RESUME_FROM_LARGE`
- Auth: `curl_easy_setopt(curl, CURLOPT_USERPWD, "user:pass")`
- **Performance Note**: FTP is often slower than HTTP; single connection per file
- **Critical Thinking**: Why is FTP still used? (Legacy systems, simplicity, anonymity)

**Validation**:
```bash
./download_manager ftp://ftp.example.com/pub/file.tar.gz file.tar.gz --user anonymous:email@example.com
```

---

### TASK-038: Scheduling - Cron-Like Download Scheduling
**Priority**: P3 (Nice to Have)
**Estimated Time**: 3 hours
**Complexity**: Medium-High

**Goal**: Schedule downloads to start at specific times.

**Acceptance Criteria**:
- [ ] Schedule download for future time (e.g., "tonight at 2 AM")
- [ ] Recurring downloads (daily, weekly)
- [ ] Persistent schedules (survive restarts)
- [ ] UI to view and edit schedules
- [ ] Cancel scheduled downloads

**Approach**:
1. Create `Schedule` class with cron-like syntax or simple time spec
2. Background thread checks schedules every minute
3. When time matches, enqueue download
4. Persist schedules to `schedules.json`
5. UI panel to add/edit schedules

**Learning Focus**:
- Time-based scheduling
- Cron syntax (simplified version)
- Background timers

**Hints**:
- Simple syntax: `"2025-10-24 02:00"` or `"daily 02:00"`
- Check: `if (now >= scheduledTime) { enqueueDownload(); }`
- **Performance Note**: Checking every minute is low overhead
- **Critical Thinking**: Timezones? (Use UTC internally, display in local time)

**Validation**:
```bash
./download_manager --schedule "2025-10-24 02:00" http://example.com/file.zip file.zip
# Wait until 2 AM, download should start automatically
```

---

### TASK-039: Scripting Integration - Lua/Python API
**Priority**: P3 (Nice to Have)
**Estimated Time**: 4 hours
**Complexity**: High

**Goal**: Expose download manager functionality via scripting API.

**Acceptance Criteria**:
- [ ] Embed Lua or Python interpreter
- [ ] API to add, pause, resume, cancel downloads
- [ ] Scripts can register callbacks (on completion, on error)
- [ ] Load scripts from `~/.config/download_manager/scripts/`
- [ ] Example scripts provided

**Approach**:
1. Choose language: Lua (lightweight) or Python (powerful)
2. Embed interpreter (LuaJIT or pybind11)
3. Expose C++ API to scripts
4. Implement callbacks via script functions
5. Provide example: Auto-download from RSS feed

**Learning Focus**:
- Embedding scripting languages
- C++ ↔ script interop
- Callback mechanisms

**Hints**:
- Lua: Use `sol2` library for C++ bindings
- Python: Use `pybind11` for bindings
- **Performance Note**: Script overhead is minimal for infrequent operations
- **Critical Thinking**: Why add scripting? (Flexibility, user customization, automation)

**Validation**:
```lua
-- script.lua
dm.add_download("http://example.com/file.zip", "file.zip")
dm.on_complete(function(task)
    print("Downloaded: " .. task.filename)
end)
```

---

### TASK-040: GUI Option - Qt/wxWidgets Port
**Priority**: P3 (Nice to Have)
**Estimated Time**: 6+ hours
**Complexity**: Very High

**Goal**: Create optional GUI frontend using Qt or wxWidgets.

**Acceptance Criteria**:
- [ ] GUI shows download list with progress bars
- [ ] Buttons for add, pause, resume, cancel
- [ ] Settings dialog
- [ ] System tray integration (minimize to tray)
- [ ] Notifications via native OS (Windows, macOS, Linux)

**Approach**:
1. Separate core logic into library (`libdownload_manager`)
2. Create Qt/wxWidgets frontend
3. Use Qt's `QProgressBar`, `QListWidget` for UI
4. System tray: `QSystemTrayIcon`
5. Native notifications: Qt's `QNotification` or platform APIs

**Learning Focus**:
- Separation of UI and logic (MVC pattern)
- GUI framework basics (Qt or wxWidgets)
- Cross-platform GUI challenges

**Hints**:
- Start with minimal GUI: Just download list and progress
- Reuse all backend logic from TUI version
- **Performance Note**: GUI adds overhead; not suitable for headless servers
- **Critical Thinking**: When GUI vs TUI? (GUI for desktop users, TUI for servers/SSH)

**Validation**:
```bash
# Build GUI version
cmake -B build -DBUILD_GUI=ON
./build/bin/download_manager_gui
# Should show graphical window with download list
```

---

### TASK-041: Plugin System Architecture
**Priority**: P3 (Nice to Have)
**Estimated Time**: 4 hours
**Complexity**: High

**Goal**: Design plugin system for extensibility.

**Acceptance Criteria**:
- [ ] Load plugins from shared libraries (.so, .dll)
- [ ] Plugin API for custom protocols, notifiers, UI themes
- [ ] Plugins registered at runtime
- [ ] Example plugin: Custom protocol handler
- [ ] Plugin manifest with metadata

**Approach**:
1. Define plugin interface: `IPlugin` base class
2. Load shared libraries with `dlopen()` (Linux) or `LoadLibrary()` (Windows)
3. Query plugin for capabilities and register hooks
4. Example: Custom protocol plugin implementing `IProtocolHandler`
5. Manifest: JSON with plugin name, version, dependencies

**Learning Focus**:
- Dynamic library loading
- Plugin architecture patterns
- Interface-based design

**Hints**:
- Plugin entry point: `extern "C" IPlugin* create_plugin()`
- Use `std::unique_ptr` for plugin lifetime management
- **Performance Note**: Plugin loading is one-time; focus on API design
- **Critical Thinking**: Security? (Only load signed plugins, or user explicitly allows)

**Validation**:
```cpp
// custom_protocol_plugin.cpp
class MyProtocolHandler : public IProtocolHandler {
    void download(const std::string& url, const std::string& dest) override {
        // Custom download logic
    }
};
extern "C" IPlugin* create_plugin() { return new MyProtocolHandler(); }
```

---

### TASK-042: Memory Profiling and Leak Detection
**Priority**: P2 (Medium)
**Estimated Time**: 2.5 hours
**Complexity**: Medium

**Goal**: Ensure no memory leaks using Valgrind or ASAN.

**Acceptance Criteria**:
- [ ] Valgrind reports zero leaks after full run
- [ ] ASAN reports zero leaks during stress tests
- [ ] Memory usage stable during long-running downloads
- [ ] Profile memory usage over 24-hour run
- [ ] Document maximum memory footprint

**Approach**:
1. Compile with ASAN: `-fsanitize=address`
2. Run stress test with ASAN
3. Run with Valgrind: `valgrind --leak-check=full ./download_manager`
4. Fix any leaks (likely in libcurl usage or thread cleanup)
5. Long-term run: Monitor RSS with `ps` or `top`

**Learning Focus**:
- Memory leak detection tools
- RAII and leak prevention
- Memory profiling

**Hints**:
- ASAN: Immediate feedback on use-after-free, leaks
- Valgrind: More thorough but slower
- **Performance Note**: ASAN adds 2x memory overhead; use in testing only
- **Critical Thinking**: Acceptable memory usage? (<100MB for 100 downloads is reasonable)

**Validation**:
```bash
cmake -B build -DCMAKE_CXX_FLAGS=-fsanitize=address
cmake --build build
./build/bin/download_manager_test
# ASAN should report: "0 leaks detected"
```

---

### TASK-043: Cross-Platform Testing - Windows + Linux
**Priority**: P1 (High)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Ensure application works on both Windows and Linux.

**Acceptance Criteria**:
- [ ] Builds on Windows (MinGW64, MSVC)
- [ ] Builds on Linux (GCC, Clang)
- [ ] All tests pass on both platforms
- [ ] File paths handled correctly (/ vs \)
- [ ] Network operations work on both platforms

**Approach**:
1. Test build on Windows with MinGW64 and MSVC
2. Test build on Linux with GCC and Clang
3. Use `std::filesystem` for path handling (cross-platform)
4. Run full test suite on both platforms
5. Fix platform-specific issues (e.g., Winsock initialization)

**Learning Focus**:
- Cross-platform C++ development
- Platform-specific quirks
- Conditional compilation (`#ifdef _WIN32`)

**Hints**:
- Use `std::filesystem::path` for all paths (handles / and \ automatically)
- libcurl handles Winsock initialization internally
- **Performance Note**: No perf difference between platforms for I/O-bound tasks
- **Critical Thinking**: Should we support macOS? (Yes, libcurl and FTXUI are cross-platform)

**Validation**:
```bash
# Linux
cmake -B build && cmake --build build && ./build/bin/download_manager_test

# Windows (MinGW64)
cmake -B build -G "MinGW Makefiles" && cmake --build build && ./build/bin/download_manager_test.exe
```

---

### TASK-044: Documentation - User Guide
**Priority**: P2 (Medium)
**Estimated Time**: 3 hours
**Complexity**: Low

**Goal**: Write comprehensive user guide and README.

**Acceptance Criteria**:
- [ ] README.md with project overview, build instructions, usage examples
- [ ] USER_GUIDE.md with detailed feature documentation
- [ ] Screenshots of TUI
- [ ] Troubleshooting section
- [ ] API documentation (if scripting supported)

**Approach**:
1. Write README.md: Overview, features, quick start, build instructions
2. Write USER_GUIDE.md: Detailed usage, keyboard shortcuts, settings
3. Capture screenshots of TUI (use `asciinema` for terminal recording)
4. Document common issues and solutions
5. Use Markdown for formatting

**Learning Focus**:
- Technical writing
- Documentation best practices
- User-focused explanations

**Hints**:
- Structure: Purpose → Features → Installation → Usage → Troubleshooting
- Screenshots: `asciinema rec demo.cast` then convert to GIF
- **Performance Note**: N/A (documentation)
- **Critical Thinking**: Who is the audience? (Developers learning C++, end users wanting download tool)

**Validation**:
```bash
# README should allow new user to build and run with zero prior knowledge
# Follow your own README from scratch on fresh VM
```

---

### TASK-045: Documentation - Developer Guide
**Priority**: P2 (Medium)
**Estimated Time**: 3 hours
**Complexity**: Low

**Goal**: Document architecture and code for future contributors.

**Acceptance Criteria**:
- [ ] ARCHITECTURE.md describing system design
- [ ] Code comments on complex functions
- [ ] Doxygen-style comments for public APIs
- [ ] CONTRIBUTING.md for new developers
- [ ] Diagrams (architecture, threading model)

**Approach**:
1. Write ARCHITECTURE.md: High-level design, component interactions
2. Add comments to complex algorithms (token bucket, thread pool)
3. Use Doxygen comments: `/// @brief`, `/// @param`, `/// @return`
4. Write CONTRIBUTING.md: Code style, PR process, testing requirements
5. Create diagrams with PlantUML or Mermaid

**Learning Focus**:
- Code documentation practices
- Architectural documentation
- Onboarding new contributors

**Hints**:
- Doxygen: `/// @brief Downloads a file from URL to destination`
- Diagrams: Use Mermaid (supported by GitHub) for simple diagrams
- **Performance Note**: N/A (documentation)
- **Critical Thinking**: What would you want to know if you were new to this codebase?

**Validation**:
```bash
# Generate Doxygen docs
doxygen Doxyfile
# Open html/index.html, verify API docs are clear
```

---

### TASK-046: Unit Test Suite - Core Components
**Priority**: P1 (High)
**Estimated Time**: 4 hours
**Complexity**: Medium-High

**Goal**: Write unit tests for core components using Catch2 or Google Test.

**Acceptance Criteria**:
- [ ] Tests for `ThreadPool`: Enqueue, shutdown, future/promise
- [ ] Tests for `BandwidthLimiter`: Rate enforcement, token bucket
- [ ] Tests for `DownloadTask`: State transitions, pause/resume
- [ ] Tests for `HttpClient`: Basic download, resume, errors
- [ ] 80%+ code coverage

**Approach**:
1. Add Catch2 or Google Test to CMake
2. Create test directory: `tests/`
3. Write test cases for each component
4. Use mocks for network I/O (e.g., httpbin.org or local server)
5. Measure coverage with `lcov` or `gcov`

**Learning Focus**:
- Unit testing in C++
- Mocking network I/O
- Test-driven development principles

**Hints**:
- Catch2 test: `TEST_CASE("ThreadPool enqueue") { ... }`
- Mock server: Use httpbin.org or `python -m http.server`
- **Performance Note**: Tests should be fast (<1s each); use small files
- **Critical Thinking**: What makes a good test? (Fast, isolated, repeatable, clear)

**Validation**:
```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
./build/bin/download_manager_test
# All tests should pass
```

---

### TASK-047: Integration Tests - End-to-End Scenarios
**Priority**: P1 (High)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Write integration tests for complete workflows.

**Acceptance Criteria**:
- [ ] Test: Download multiple files concurrently
- [ ] Test: Pause and resume download
- [ ] Test: Cancel download
- [ ] Test: Network failure and auto-retry
- [ ] Test: State persistence across restarts

**Approach**:
1. Create integration test suite in `tests/integration/`
2. Use real HTTP servers (httpbin.org, local Python server)
3. Simulate failures (disconnect network, corrupt files)
4. Test full workflows: Add → Download → Pause → Resume → Complete
5. Automate with test framework (Catch2 or Bash scripts)

**Learning Focus**:
- Integration testing strategies
- Simulating real-world conditions
- Test automation

**Hints**:
- Local server: `python3 -m http.server 8080`
- Simulate disconnect: Use `iptables` or firewall rules
- **Performance Note**: Integration tests are slower (network I/O); run less frequently
- **Critical Thinking**: What scenarios are most likely to break? (Network failures, race conditions)

**Validation**:
```bash
./build/bin/download_manager_integration_test
# All scenarios should pass
```

---

### TASK-048: Continuous Integration - GitHub Actions
**Priority**: P2 (Medium)
**Estimated Time**: 2.5 hours
**Complexity**: Medium

**Goal**: Set up CI pipeline to build and test on every commit.

**Acceptance Criteria**:
- [ ] GitHub Actions workflow for Linux and Windows
- [ ] Build on GCC, Clang, MSVC
- [ ] Run unit tests on each commit
- [ ] Run sanitizers (ASAN, TSan)
- [ ] Badge in README showing build status

**Approach**:
1. Create `.github/workflows/ci.yml`
2. Define matrix: OS (Linux, Windows), compiler (GCC, Clang, MSVC)
3. Steps: Checkout → Install deps → Build → Test
4. Add sanitizer builds: ASAN, TSan
5. Add badge to README: `![CI](badge-url)`

**Learning Focus**:
- CI/CD basics
- GitHub Actions syntax
- Multi-platform testing automation

**Hints**:
- Example workflow: `runs-on: [ubuntu-latest, windows-latest]`
- Sanitizers: Add separate job with `-fsanitize=address`
- **Performance Note**: CI builds take 5-10 minutes; optimize by caching dependencies
- **Critical Thinking**: What should CI catch? (Build failures, test regressions, platform-specific issues)

**Validation**:
```bash
# Push commit, check GitHub Actions tab
# All jobs should pass
```

---

### TASK-049: Release Packaging - Binaries and Installers
**Priority**: P2 (Medium)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Create release packages for distribution.

**Acceptance Criteria**:
- [ ] Build static binary (minimal dependencies)
- [ ] Package for Linux (.deb, .tar.gz)
- [ ] Package for Windows (.zip, installer)
- [ ] Include README, LICENSE, example config
- [ ] Automated release process

**Approach**:
1. Static build: Link libcurl and FTXUI statically
2. Linux: Use CPack to create .deb package
3. Windows: Create .zip with executable and DLLs, or use NSIS for installer
4. Include docs and example files in package
5. Automate with GitHub Actions release workflow

**Learning Focus**:
- Packaging and distribution
- Static vs dynamic linking
- Release automation

**Hints**:
- Static link: `set(BUILD_SHARED_LIBS OFF)` in CMake
- CPack: `include(CPack)` in CMakeLists.txt, then `cpack`
- **Performance Note**: Static binaries are larger but more portable
- **Critical Thinking**: Trade-offs of static vs dynamic linking? (Static: larger, portable; Dynamic: smaller, requires dependencies)

**Validation**:
```bash
# Build package
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build build
cpack
# Test package installation on fresh system
```

---

### TASK-050: Final Polish and Code Review
**Priority**: P1 (High)
**Estimated Time**: 3 hours
**Complexity**: Medium

**Goal**: Final review, cleanup, and polish before project completion.

**Acceptance Criteria**:
- [ ] Code review: Consistent style, no TODOs, clean logic
- [ ] Remove debug code and dead code
- [ ] Verify all tests pass
- [ ] Update version numbers and changelog
- [ ] Tag release: v1.0.0

**Approach**:
1. Code review: Read through all code, check for issues
2. Run `clang-format` on all files
3. Remove debug prints, commented code
4. Run full test suite on all platforms
5. Update CHANGELOG.md, tag release

**Learning Focus**:
- Code quality and maintainability
- Release management
- Reflection on learning journey

**Hints**:
- Use `clang-format` for consistent style
- Checklist: No warnings, all tests pass, docs complete
- **Performance Note**: N/A (final review)
- **Critical Thinking**: Reflect: What did you learn? What would you do differently? What's next?

**Validation**:
```bash
# Full checklist
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/bin/download_manager_test  # All pass
valgrind ./build/bin/download_manager  # No leaks
git tag -a v1.0.0 -m "Release 1.0.0"
git push --tags
```

---

## Summary

**Total Tasks**: 50
**Estimated Duration**: 50 days (~2-3 hours/day)
**Total Hours**: ~125-150 hours

**Learning Progression**:
1. **Foundations (Days 1-10)**: Build system, HTTP basics, file I/O, error handling
2. **Concurrency (Days 11-20)**: Thread pool, synchronization, parallel downloads
3. **Advanced Features (Days 21-35)**: Bandwidth control, TUI, state persistence, notifications
4. **Polish (Days 36-50)**: Optimization, testing, documentation, release

**Key Takeaways**:
- Each task is self-contained and achievable in one day
- Progressive complexity: Simple → Moderate → Complex
- Emphasis on learning: Hints, performance notes, critical thinking prompts
- Practical validation: Every task has concrete acceptance criteria
- Modern C++: C++17 features, RAII, smart pointers, STL algorithms
- Real-world skills: Concurrency, networking, testing, profiling, documentation

**Next Steps After Completion**:
- Share on GitHub with polished README
- Write blog post about your learning journey
- Extend with additional protocols (BitTorrent, Metalink)
- Contribute to open-source projects using skills learned
- Mentor others learning C++

---

**Congratulations on completing your C++ learning journey!** This project has equipped you with production-ready skills in systems programming, concurrency, and software engineering best practices.
