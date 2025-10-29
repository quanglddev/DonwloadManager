#pragma once

#include <string>
#include <memory>
#include <curl/curl.h>
#include <chrono>
#include <filesystem>

/**
 * HTTP client for downloading files using libcurl.
 * Uses RAII to manage CURL handle lifecycle.
 */
class HttpClient
{
public:
    HttpClient();
    ~HttpClient();

    // Delete copy operations (CURL handles aren't copyable)
    HttpClient(const HttpClient &) = delete;
    HttpClient &operator=(const HttpClient &) = delete;

    // Move operations (allow transferring ownership)
    HttpClient(HttpClient &&) noexcept = default;
    HttpClient &operator=(HttpClient &&) noexcept = default;

    /**
     * Download file from URL to destination path
     *
     * @param url HTTP/HTTPS URL to download
     * @param destination Local file path to save
     * @return true on success, false on failure
     */
    bool downloadFile(const std::string &url, const std::string &destination);

    /**
     * Get detailed error message from last operation.
     */
    std::string getLastError() const { return lastError_; }

private:
    // CURL handle with custom deleter (RAII pattern)
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_;

    // Last error message
    std::string lastError_;

    /**
     * Static callback for libcurl to write downloaded data.
     * libcurl is C library, so callbacks must be static or free functions.
     *
     * @param ptr Pointer to downloaded data chunk
     * @param size Size of each element (usually 1)
     * @param nmemb Number of elements
     * @param userdata User-provided pointer (we pass std::ofstream*)
     * @return Number of bytes written (size * nmemb on success)
     */
    static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

    /**
     * Static progress callback for libcurl.
     * Called periodically during download to report progress.
     *
     * @param clientp User data pointer (we pass 'this')
     * @param dltotal Total bytes to download (0 if unknown)
     * @param dlnow Bytes downloaded so far
     * @param ultotal Total bytes to upload (not used)
     * @param ulnow Bytes uploaded so far (not used)
     * @return 0 to continue, non-zero to abort
     */
    static int progressCallback(void *clientp,
                                curl_off_t dltotal,
                                curl_off_t dlnow,
                                curl_off_t ultotal,
                                curl_off_t ulnow);

    /**
     * Format bytes into human-readable string (e.g., "52.3 MB")
     *
     * @param bytes Number of bytes
     * @return Formatted string
     */
    std::string formatBytes(curl_off_t bytes) const;

    /**
     * Format duration into human-readable string (e.g., "2m 30s")
     *
     * @param seconds Duration in seconds
     * @return Formatted string
     */
    std::string formatDuration(long seconds) const;

    /**
     * Get human-readable HTTP status text for a status code.
     *
     * @param code HTTP status code (e.g., 200, 404, 500)
     * @return Descriptive text for the status code
     */
    std::string getHttpStatusText(long code) const;

    /**
     * Ensure the directory for a file path exists, creating it if needed.
     *
     * @param filePath Path to the file (directory will be extracted)
     * @return true if directory exists or was created successfully
     */
    bool ensureDirectoryExists(const std::filesystem::path &filePath);

    /**
     * Check if there's enough disk space for a download.
     *
     * @param filePath Path where file will be saved
     * @param requiredBytes Number of bytes needed
     * @return true if enough space is available
     */
    bool checkDiskSpace(const std::filesystem::path &filePath, curl_off_t requiredBytes);

    /**
     * Generate the .part filename for a destination path.
     *
     * @param destination Final destination path
     * @return Path with .part extension added
     */
    std::filesystem::path makePartPath(const std::filesystem::path &destination) const;

    std::chrono::steady_clock::time_point startTime_;
    curl_off_t lastDownloaded_ = 0;
    std::chrono::steady_clock::time_point lastProgressTime_;
    
    // Track if we've checked disk space (to do it once in progress callback if HEAD failed)
    bool diskSpaceChecked_ = false;
    std::filesystem::path currentDestination_;
};