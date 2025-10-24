# Design: Multithreaded Download Manager

## Architecture Overview

### High-Level Components

```
┌─────────────────────────────────────────────────────────────┐
│                     Terminal UI (FTXUI)                     │
│  [Progress Bars] [Download List] [Controls] [Stats]         │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                  Download Manager (Orchestrator)             │
│  - Queue management    - State persistence                   │
│  - Global throttling   - Event notifications                 │
└────────────┬────────────────────────────┬───────────────────┘
             │                            │
             ▼                            ▼
┌────────────────────────┐    ┌──────────────────────────────┐
│    Thread Pool         │    │   Notification Service       │
│  - Worker threads      │    │  - Email sender (mailio)     │
│  - Work queue          │    │  - Event queue               │
│  - Load balancing      │    └──────────────────────────────┘
└────────┬───────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│              Download Task (per file)                        │
│  - HTTP client (libcurl)  - Chunk manager                    │
│  - Resume logic           - Integrity checker                │
│  - Bandwidth limiter      - Progress tracker                 │
└─────────────────────────────────────────────────────────────┘
```

### Design Principles

1. **Separation of Concerns**: UI, orchestration, and download logic are distinct layers
2. **Thread Safety**: All shared state protected by appropriate synchronization primitives
3. **Resource Management**: RAII for all resources (threads, files, network connections)
4. **Testability**: Components have clear interfaces and can be unit tested
5. **Progressive Enhancement**: Core features work before adding polish

## Core Components Deep Dive

### 1. Download Task

**Responsibility**: Execute a single file download with resume capability

**Key Classes**:
```cpp
class DownloadTask {
public:
    enum class State { Queued, Downloading, Paused, Completed, Failed };

    DownloadTask(std::string url, std::filesystem::path dest);

    void start();
    void pause();
    void resume();
    void cancel();

    State getState() const;
    ProgressInfo getProgress() const;

private:
    void downloadChunk(size_t offset, size_t length);
    void verifyIntegrity();

    std::string url_;
    std::filesystem::path destination_;
    std::atomic<State> state_;
    std::atomic<size_t> bytesDownloaded_;
    std::mutex stateMutex_;

    BandwidthLimiter rateLimiter_;
    CurlHandle curlHandle_;
};
```

**Learning Points**:
- **Atomic state management**: Why `std::atomic<State>` for state but `std::mutex` for operations?
- **Range requests**: How HTTP `Range: bytes=offset-end` enables resumption
- **Write callbacks**: libcurl's `CURLOPT_WRITEFUNCTION` for streaming data to disk
- **RAII patterns**: Ensuring file handles close even during exceptions

**Performance Hints**:
- **Chunk size**: 256KB-1MB chunks balance memory vs syscall overhead
- **Buffer reuse**: Single pre-allocated buffer per thread avoids malloc/free
- **Direct I/O**: Consider `O_DIRECT` flag for large files (Linux)

### 2. Thread Pool

**Responsibility**: Manage worker threads and distribute download tasks

**Key Classes**:
```cpp
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    template<typename Func>
    std::future<void> enqueue(Func&& task);

    void shutdown();
    size_t getActiveWorkers() const;

private:
    void workerThread();

    std::vector<std::thread> workers_;
    ThreadSafeQueue<std::function<void()>> taskQueue_;
    std::atomic<bool> shutdown_;
    std::condition_variable cv_;
    std::mutex queueMutex_;
};
```

**Learning Points**:
- **Producer-consumer pattern**: How condition variables coordinate work distribution
- **Graceful shutdown**: Ensuring all tasks complete before thread destruction
- **Future/Promise**: Returning results from async operations
- **Move semantics**: Efficiently transferring task ownership into queue

**Performance Hints**:
- **Thread count**: Start with `std::thread::hardware_concurrency()`, tune based on I/O wait
- **Queue lock contention**: Consider lock-free queue (advanced) or batch dequeue
- **False sharing**: Align atomic counters to cache line boundaries (64 bytes)

### 3. Bandwidth Throttler

**Responsibility**: Limit download speed per-task and globally

**Key Classes**:
```cpp
class BandwidthLimiter {
public:
    explicit BandwidthLimiter(size_t bytesPerSecond);

    // Blocks until 'bytes' can be consumed without exceeding rate
    void consume(size_t bytes);

    void setLimit(size_t bytesPerSecond);
    size_t getCurrentRate() const;

private:
    std::mutex mutex_;
    size_t limit_;  // bytes per second
    std::chrono::steady_clock::time_point lastRefill_;
    double tokensAvailable_;
};
```

**Algorithm**: Token Bucket
- Tokens accumulate at target rate
- Each downloaded byte consumes a token
- If bucket empty, sleep until tokens available

**Learning Points**:
- **Token bucket vs leaky bucket**: Trade-offs in burstiness
- **Time handling**: Why `std::chrono::steady_clock` vs `system_clock`?
- **Floating point tokens**: Why not integer? (Precision at low rates)

**Performance Hints**:
- **Granularity**: Check throttle every 4-16KB to avoid excessive locking
- **Two-level throttling**: Per-task limit OR global limit (whichever stricter)
- **Adaptive rate**: Measure actual throughput vs limit, adjust sleep duration

### 4. State Persistence

**Responsibility**: Save/load download state for resume across app restarts

**Schema** (JSON via nlohmann/json):
```json
{
  "version": "1.0",
  "downloads": [
    {
      "id": "uuid-here",
      "url": "https://example.com/file.zip",
      "destination": "/path/to/file.zip",
      "state": "paused",
      "bytesDownloaded": 52428800,
      "totalBytes": 104857600,
      "checksum": "sha256:abc123...",
      "createdAt": "2025-10-23T10:30:00Z",
      "completedAt": null
    }
  ],
  "settings": {
    "maxConcurrentDownloads": 4,
    "globalBandwidthLimit": 1048576,
    "defaultChunkSize": 262144
  }
}
```

**Learning Points**:
- **Serialization**: Mapping C++ objects to JSON and back
- **Version migration**: Handling schema changes gracefully
- **Atomic writes**: Write to temp file, then rename (prevents corruption)

### 5. Terminal UI (FTXUI)

**Responsibility**: Render real-time download progress and handle user input

**Layout**:
```
┌─────────────────────────────────────────────────────────────┐
│ Download Manager v1.0               Active: 3/4    ↑/↓ ⏎    │
├─────────────────────────────────────────────────────────────┤
│ file1.zip                [▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░] 52% 2.1MB/s │
│ file2.tar.gz             [▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓] 100% ✓       │
│ file3.iso                [▓▓▓░░░░░░░░░░░░░░░░░] 15% 850KB/s │
│ file4.pdf                [Paused]                   0.0MB/s │
├─────────────────────────────────────────────────────────────┤
│ [A]dd  [P]ause  [R]esume  [C]ancel  [S]ettings  [Q]uit     │
├─────────────────────────────────────────────────────────────┤
│ Total: 1.2GB / 4.8GB     Rate: 3.5MB/s     ETA: 15m 24s    │
└─────────────────────────────────────────────────────────────┘
```

**FTXUI Integration**:
- **Component model**: Each download is a `Component` with its own state
- **Reactive updates**: UI polls download manager at 10Hz for smooth animation
- **Input handling**: Keyboard events mapped to download commands

**Learning Points**:
- **Event loop**: How TUI frameworks differ from GUI frameworks
- **Concurrency**: UI thread vs worker threads, thread-safe state access
- **Performance**: Minimizing redraws, diff-based rendering

## Library Integration Strategy

### libcurl (HTTP Client)

**Why**: De facto standard for HTTP in C/C++, battle-tested, feature-complete

**Integration Approach**:
1. **Phase 1 (Day 3-4)**: Synchronous wrapper for basic downloads
2. **Phase 2 (Day 15-16)**: Expose multi-handle API for concurrent connections
3. **Phase 3 (Day 25-26)**: Advanced features (connection pooling, HTTP/2)

**CMake FetchContent**:
```cmake
FetchContent_Declare(
  CURL
  GIT_REPOSITORY https://github.com/curl/curl.git
  GIT_TAG curl-8_5_0
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(CURL_DISABLE_TESTS ON CACHE BOOL "")
FetchContent_MakeAvailable(CURL)
target_link_libraries(download_manager PRIVATE CURL::libcurl)
```

**Learning Focus**: Understand curl callbacks, error codes, multi-handle API

### FTXUI (Terminal UI)

**Why**: Modern, reactive, cross-platform, excellent documentation

**Integration Approach**:
1. **Phase 1 (Day 5-6)**: Static layout with placeholder data
2. **Phase 2 (Day 20-21)**: Live updates with polling model
3. **Phase 3 (Day 35-36)**: Advanced features (mouse support, animations)

**CMake FetchContent**:
```cmake
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)
target_link_libraries(download_manager PRIVATE
  ftxui::screen ftxui::dom ftxui::component
)
```

**Learning Focus**: Component composition, event-driven UI, reactive patterns

### nlohmann/json (Serialization)

**Why**: Header-only, intuitive API, excellent C++ integration

**Integration Approach**:
1. **Phase 1 (Day 30)**: Basic save/load of download metadata
2. **Phase 2 (Day 40)**: Settings persistence, schema versioning

**CMake FetchContent**:
```cmake
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)
target_link_libraries(download_manager PRIVATE nlohmann_json::nlohmann_json)
```

**Learning Focus**: JSON serialization, RAII for file I/O

### mailio (SMTP Email)

**Why**: C++17, cross-platform, TLS support, active maintenance

**Integration Approach**:
1. **Phase 1 (Day 45)**: Basic email sending on download completion
2. **Phase 2 (Day 46)**: HTML templates, error notifications

**CMake** (via vcpkg or manual):
```bash
vcpkg install mailio
```

**Learning Focus**: SMTP protocol basics, async notification patterns

## Threading Model

### Worker Thread Lifecycle

```
[Start] → [Wait for Task] → [Acquire Task] → [Download Chunks] → [Update Progress]
              ↑                                      │
              └──────────────────────────────────────┘
                        (loop until shutdown)
```

### Synchronization Points

| Shared Resource | Protection Mechanism | Access Pattern |
|-----------------|---------------------|----------------|
| Task queue | `std::mutex + std::condition_variable` | Producer: main thread; Consumer: workers |
| Download state (per-task) | `std::atomic` for state, `std::mutex` for operations | Writers: worker thread; Readers: UI thread |
| Global bandwidth counter | `std::atomic` or `std::mutex` | Writers: all workers; Readers: UI thread |
| File writing | `std::mutex` per file | Exclusive writer (single worker per file) |

### Concurrency Patterns

1. **Producer-Consumer**: Main thread produces tasks, workers consume
2. **Reader-Writer**: UI reads state frequently, workers write occasionally
3. **Barrier**: Shutdown waits for all workers to finish current tasks

## Error Handling Strategy

### Error Categories

1. **Network Errors** (retryable):
   - Connection timeout → Exponential backoff retry (1s, 2s, 4s, 8s)
   - DNS failure → Retry with different resolver
   - HTTP 5xx → Retry up to 3 times

2. **File Errors** (mostly fatal):
   - Disk full → Pause all downloads, notify user
   - Permission denied → Mark task failed, continue others
   - Write error → Retry once, then fail

3. **Protocol Errors**:
   - HTTP 404 → Immediately fail
   - HTTP 403 → Fail with authentication hint
   - HTTP 416 (range not satisfiable) → Restart from beginning

### Retry Logic

```cpp
template<typename Func>
auto retryWithBackoff(Func&& func, int maxRetries = 3) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            return func();
        } catch (const NetworkException& e) {
            if (attempt == maxRetries - 1) throw;
            std::this_thread::sleep_for(
                std::chrono::seconds(1 << attempt)  // 1s, 2s, 4s
            );
        }
    }
}
```

## Performance Optimization Roadmap

### Initial Implementation (Correctness First)
- Single global mutex for all shared state
- Simple sleep-based throttling
- Synchronous I/O

### Phase 1 Optimization (Week 5-6)
- Per-task mutexes (reduce contention)
- Token bucket throttling
- Buffered I/O

### Phase 2 Optimization (Week 9-10, Optional)
- Lock-free queue (if profiling shows queue contention)
- Memory pool for buffers
- I/O thread separation (IOCP on Windows, io_uring on Linux)

## Testing Strategy

### Unit Tests (Catch2)
- `BandwidthLimiter`: Verify rate enforcement within ±5%
- `ThreadPool`: Queue operations, shutdown behavior
- `DownloadTask`: State transitions, pause/resume logic

### Integration Tests
- Download real files (use httpbin.org for testing)
- Simulate network failures (iptables/firewall rules)
- Corrupt files mid-download, verify recovery

### Manual Testing
- Extended soak test: 100+ files over 24 hours
- Bandwidth verification: tcpdump + wireshark analysis
- Cross-platform: Windows (MinGW + MSVC), Linux (GCC + Clang)

## Platform Considerations

### Windows (MinGW64)
- **Thread creation**: Use `<thread>`, avoid Windows API directly
- **File paths**: Use `std::filesystem`, handle `\` separators
- **Networking**: libcurl handles Winsock initialization

### Linux
- **Signal handling**: Ignore SIGPIPE (broken pipes during download)
- **File descriptors**: Check `ulimit -n`, may need to increase

### Cross-Platform
- **Endianness**: Network byte order for checksums
- **Line endings**: Handle `\r\n` vs `\n` in HTTP headers

## Security Considerations

1. **HTTPS validation**: Enable `CURLOPT_SSL_VERIFYPEER`
2. **Path traversal**: Sanitize filenames from URLs
3. **Disk space**: Check available space before download
4. **Resource limits**: Cap max memory (buffer pool size), max connections

## Future Extensions (Post-Learning)

- **Protocols**: FTP, BitTorrent, Metalink
- **Advanced resume**: Multi-part downloads (download different ranges in parallel)
- **Scheduling**: Cron-like scheduled downloads
- **Scripting**: Lua/Python integration for automation
- **GUI**: Migrate to Qt/wxWidgets

---

This design balances **learning depth** (manual threading, explicit synchronization) with **practical utility** (using proven libraries for non-core features). Each component is sized for incremental implementation while teaching fundamental C++ concepts.
