# Proposal: Implement Multithreaded Download Manager

## Overview

Build a production-grade, terminal-based download manager in C++17 that demonstrates modern C++ practices while providing hands-on learning opportunities in concurrent programming, network I/O, and system design.

## Learning Objectives

This project is designed as a **learning-first journey** through modern C++ development:

1. **Master C++17 concurrency primitives** - Implement multithreading manually using `std::thread`, `std::mutex`, `std::condition_variable`, and `std::atomic`
2. **Understand network programming** - Work with HTTP protocols, chunked transfers, range requests, and connection management
3. **Build robust systems** - Implement error handling, retry logic, state machines, and data integrity checks
4. **Design scalable architectures** - Create thread pools, queue management systems, and resource throttling
5. **Apply modern C++ idioms** - Use RAII, smart pointers, move semantics, and STL algorithms effectively

## Motivation

### Why This Project?

- **Practical complexity**: Download managers touch multiple domains (networking, concurrency, I/O, UI) without being overwhelming
- **Incremental learning**: Each feature builds on previous knowledge while introducing new concepts
- **Real-world applicability**: The patterns learned (producer-consumer, thread pools, resource management) apply broadly
- **Visible progress**: Every task produces tangible, testable functionality

### Learning Philosophy

- **Core features coded manually**: Threading, synchronization, and download logic written from scratch
- **Utilities via libraries**: Email notifications, TUI, and serialization use battle-tested libraries
- **Progressive complexity**: Start with single-threaded downloads, gradually add concurrency and advanced features
- **Performance mindfulness**: Each task includes hints about optimal approaches and trade-offs

## Scope

### What We're Building

A terminal-based application that can:
- Download multiple files concurrently with configurable parallelism
- Pause, resume, cancel individual downloads
- Survive network interruptions with automatic retry
- Throttle bandwidth per-file and globally
- Display real-time progress with a polished TUI
- Verify file integrity via checksums
- Send email notifications on completion
- Persist download state across application restarts

### What We're Learning

**Week 1-2: Foundations**
- CMake build systems and dependency management
- Basic HTTP downloads with libcurl
- File I/O and filesystem operations
- Simple terminal output

**Week 3-4: Concurrency Basics**
- Thread creation and management
- Mutexes and condition variables
- Thread-safe queues
- Basic thread pool implementation

**Week 5-6: Advanced Download Features**
- HTTP range requests for resumption
- Chunked downloads and reassembly
- Connection pooling
- Error handling and retry strategies

**Week 7-8: Performance & UX**
- Bandwidth throttling algorithms
- Advanced thread pool with work stealing
- Rich TUI with FTXUI
- State persistence with JSON serialization

**Week 9-10: Polish & Extension**
- Email notifications
- Comprehensive error recovery
- Performance profiling and optimization
- Documentation and testing

## Dependencies & Tooling

### Core Dependencies (Manual Implementation)
- **Threading**: `<thread>`, `<mutex>`, `<condition_variable>`, `<atomic>` (C++17 standard library)
- **Download Engine**: Custom implementation using libcurl C API (to understand low-level operations)

### Helper Libraries (Learning Focus: Integration)
- **libcurl**: HTTP/HTTPS protocol handling (industry standard, C++17 compatible)
- **FTXUI**: Terminal UI framework (modern, reactive, cross-platform)
- **nlohmann/json**: JSON serialization for state persistence (header-only, intuitive API)
- **mailio**: SMTP email notifications (C++17, cross-platform, TLS support)

### Build & Development Tools
- **CMake 3.20+**: Modern build configuration with FetchContent
- **vcpkg or Conan**: Dependency management (choose based on preference)
- **Catch2**: Unit testing framework (header-only option available)
- **clang-format**: Code formatting consistency

## Success Criteria

### Technical
- [ ] Successfully download 10+ files concurrently without corruption
- [ ] Resume interrupted downloads maintaining partial progress
- [ ] Bandwidth throttling accurate within 5% of target
- [ ] TUI responsive with <100ms input latency
- [ ] Zero memory leaks (verified with valgrind/ASAN)
- [ ] Graceful shutdown with state preservation

### Learning
- [ ] Understand thread synchronization patterns (producer-consumer, reader-writer)
- [ ] Explain memory models and atomic operations
- [ ] Debug race conditions and deadlocks
- [ ] Profile and optimize concurrent code
- [ ] Write thread-safe, exception-safe C++ code

## Risks & Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| C++17 concurrency learning curve | May block progress | Task breakdowns include detailed hints, example patterns, and debugging guides |
| libcurl API complexity | Could overwhelm early stages | Start with synchronous wrapper, gradually expose async features |
| Race conditions and deadlocks | Frustration, debugging time | Each concurrency task includes testing strategy and common pitfalls |
| Platform-specific issues (Windows) | Compatibility problems | Use cross-platform abstractions, test on MinGW/MSVC |

## Alternatives Considered

### Why Not C++20?
- Wider toolchain support, especially on Windows
- C++17 has sufficient concurrency features for learning
- Can upgrade later as a learning exercise

### Why Not Boost.Asio?
- Lower-level understanding comes from manual threading
- Asio's async model is a separate learning curve
- Can be added later for comparison

### Why Not Qt?
- Heavier dependency footprint
- Focuses on GUI patterns rather than modern C++
- FTXUI is lighter and terminal-focused

## Next Steps

1. **Review and approve** this proposal
2. **Read design.md** for architecture details and library integration plans
3. **Review tasks.md** for the day-by-day implementation roadmap
4. **Set up development environment** (CMake, compiler, vcpkg/Conan)
5. **Begin Day 1 task**: Project scaffolding and "Hello World" download

## Questions for Clarification

- [ ] Preferred dependency manager: vcpkg or Conan?
- [ ] Target platform priority: Windows (MinGW/MSVC), Linux, or both?
- [ ] Desired daily time investment (affects task granularity)?
- [ ] Interest in optional advanced topics (memory pools, custom allocators, coroutines)?

---

**Change ID**: `implement-multithreaded-download-manager`
**Status**: Proposed
**Estimated Duration**: 10 weeks (50 development days, ~2 hours/day)
**Complexity**: High (educational context makes it manageable)
