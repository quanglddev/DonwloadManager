#include "http_client.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <fmt/core.h>

HttpClient::HttpClient() : curl_(curl_easy_init(), curl_easy_cleanup)
{

    if (!curl_)
    {
        lastError_ = "Failed to initialized CURL (out of memory or library error)";
        throw std::runtime_error(lastError_);
    }

    // Set a user-agent (some servers block requests without one)
    curl_easy_setopt(curl_.get(), CURLOPT_USERAGENT, "DownloadManager/1.90");
}

// Destructor: unique_ptr handles cleanup automatically
HttpClient::~HttpClient() = default;

// Static callback: libcurl calls this with chunks of downloaded data
size_t HttpClient::writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    // Calculate total bytes in this chunk
    size_t totalSize = size * nmemb;

    // userdata is our std::ofstream* (we pass it in downloadFile)
    auto *outFile = static_cast<std::ofstream *>(userdata);

    // Write chunk to file
    outFile->write(ptr, totalSize);

    // Return bytes written (libcurl checks this matches totalSize)
    // If we return 0 or a different value, libcurl aborts the transfer
    return totalSize;
}

bool HttpClient::downloadFile(const std::string &url, const std::string &destination)
{
    // Open output file in binary mode
    std::ofstream outFile(destination, std::ios::binary);
    if (!outFile)
    {
        lastError_ = fmt::format("Cannot open file for writing: {}", destination);
        return false;
    }

    // Configure CURL for this request

    // 1. Set the URL
    curl_easy_setopt(curl_.get(), CURLOPT_URL, url.c_str());

    // 2. Set write callback and pass file stream as context
    curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &outFile);

    // 3. HTTPS settings (CRITICAL for security)
    curl_easy_setopt(curl_.get(), CURLOPT_SSL_VERIFYPEER, 1L); // Verify server certificate
    curl_easy_setopt(curl_.get(), CURLOPT_SSL_VERIFYHOST, 2L); // Verify hostname matches cert

    // 4. Follow HTTP redirects (e.g., http://example.com -> https://example.com)
    curl_easy_setopt(curl_.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_.get(), CURLOPT_MAXREDIRS, 5L); // Limit redirect chain

    // 5. Set timeout to prevent hanging forever
    curl_easy_setopt(curl_.get(), CURLOPT_TIMEOUT, 300L);       // 5 minutes max
    curl_easy_setopt(curl_.get(), CURLOPT_CONNECTTIMEOUT, 30L); // 30s to establish connection

    // 6. Enable progress tracking
    curl_easy_setopt(curl_.get(), CURLOPT_NOPROGRESS, 0L); // Enable progress meter
    curl_easy_setopt(curl_.get(), CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl_.get(), CURLOPT_XFERINFODATA, this);

    startTime_ = std::chrono::steady_clock::now();
    lastDownloaded_ = 0;
    lastProgressTime_ = startTime_;

    // Perform the download
    CURLcode res = curl_easy_perform(curl_.get());

    // Close file before checking result (ensures data is flushed)
    outFile.close();

    // Print newline after progress bar
    fmt::print("\n");

    // Check for errors
    if (res != CURLE_OK)
    {
        lastError_ = fmt::format("Download failed: {}", curl_easy_strerror(res));
        return false;
    }

    // Check HTTP response code
    long httpCode = 0;
    curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode >= 400)
    {
        lastError_ = fmt::format("HTTP error {}: {}", httpCode, getHttpStatusText(httpCode));
        return false;
    }

    // Success!
    return true;
}

int HttpClient::progressCallback(void *clientp,
                                 curl_off_t dltotal,
                                 curl_off_t dlnow,
                                 curl_off_t ultotal,
                                 curl_off_t ulnow)
{
    // Suppress unused parameter warnings
    (void)ultotal;
    (void)ulnow;

    // clientp is our HttpClient* (we pass it in downloadFile)
    auto *client = static_cast<HttpClient *>(clientp);

    // Throttle updates to once per 200ms to avoid terminal flooding
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   now - client->lastProgressTime_)
                                   .count();

    // Only update if 200ms have passed (unless download is complete)
    bool isComplete = (dltotal > 0 && dlnow >= dltotal);
    if (timeSinceLastUpdate < 200 && !isComplete)
    {
        return 0; // Skip this update
    }

    // Update last progress time
    client->lastProgressTime_ = now;

    // If we don't know the total size, show basic progress
    if (dltotal == 0)
    {
        fmt::print("\rDownloaded: {} | Speed: calculating...", client->formatBytes(dlnow));
        std::cout.flush();
        return 0;
    }

    double percentage = (static_cast<double>(dlnow) / dltotal) * 100.0;

    // Calculate elapsed time (reuse 'now' from throttling check above)
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - client->startTime_).count();

    // Calculate download speed (bytes per second)
    double speed = (elapsed > 0) ? static_cast<double>(dlnow) / elapsed : 0.0;

    // Calculate ETA (Estimated Time of Arrival)
    long eta = (speed > 0) ? static_cast<long>((dltotal - dlnow) / speed) : 0;

    // Create progress bar (50 characters wide)
    int barWidth = 50;
    int filled = static_cast<int>((percentage / 100.0) * barWidth);
    std::string bar = "[";
    for (int i = 0; i < barWidth; ++i)
    {
        if (i < filled)
        {
            bar += "=";
        }
        else if (i == filled)
        {
            bar += ">";
        }
        else
        {
            bar += " ";
        }
    }
    bar += "]";

    // Format speed (KB/s or MB/s)
    std::string speedStr;
    if (speed >= 1024 * 1024)
    {
        speedStr = fmt::format("{:.2f} MB/s", speed / (1024.0 * 1024.0));
    }
    else if (speed >= 1024)
    {
        speedStr = fmt::format("{:.2f} KB/s", speed / 1024.0);
    }
    else
    {
        speedStr = fmt::format("{:.0f} B/s", speed);
    }

    // Print progress (using \r for in-place update)
    // \033[K clears from cursor to end of line (removes artifacts)
    fmt::print("\r{} {:.1f}% | {} / {} | {} | ETA: {}\033[K",
               bar,
               percentage,
               client->formatBytes(dlnow),
               client->formatBytes(dltotal),
               speedStr,
               client->formatDuration(eta));
    std::cout.flush();

    // Return 0 to continue download (non-zero would abort)
    return 0;
}

// Format bytes into human-readable string
std::string HttpClient::formatBytes(curl_off_t bytes) const
{
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;

    if (bytes >= GB)
    {
        return fmt::format("{:.2f} GB", bytes / GB);
    }
    else if (bytes >= MB)
    {
        return fmt::format("{:.2f} MB", bytes / MB);
    }
    else if (bytes >= KB)
    {
        return fmt::format("{:.2f} KB", bytes / KB);
    }
    else
    {
        return fmt::format("{} B", bytes);
    }
}

// Format duration into human-readable string
std::string HttpClient::formatDuration(long seconds) const
{
    if (seconds < 0)
    {
        return "unknown";
    }
    else if (seconds < 60)
    {
        return fmt::format("{}s", seconds);
    }
    else if (seconds < 3600)
    {
        long minutes = seconds / 60;
        long secs = seconds % 60;
        return fmt::format("{}m {}s", minutes, secs);
    }
    else
    {
        long hours = seconds / 3600;
        long minutes = (seconds % 3600) / 60;
        return fmt::format("{}h {}m", hours, minutes);
    }
}

// Helper: Get human-readable HTTP status text
std::string HttpClient::getHttpStatusText(long code) const
{
    switch (code)
    {
    case 200:
        return "OK";
    case 206:
        return "Partial Content";
    case 301:
        return "Moved Permanently";
    case 302:
        return "Found";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 500:
        return "Internal Server Error";
    case 502:
        return "Bad Gateway";
    case 503:
        return "Service Unavailable";
    default:
        return "Unknown Status";
    }
}