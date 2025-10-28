#pragma once

#include <string>
#include <memory>
#include <curl/curl.h>

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
     * Get human-readable HTTP status text for a status code.
     * 
     * @param code HTTP status code (e.g., 200, 404, 500)
     * @return Descriptive text for the status code
     */
    std::string getHttpStatusText(long code) const;
};