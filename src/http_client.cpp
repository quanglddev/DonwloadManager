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
    // Convert to filesystem path for easier manipulation
    std::filesystem::path finalPath(destination);
    std::filesystem::path partPath = makePartPath(finalPath);

    // 1. Ensure destination directory exists
    if (!ensureDirectoryExists(finalPath))
    {
        return false; // Error already set in lastError_
    }

    // 2. Remove any existing .part file from previous failed attempt
    try
    {
        if (std::filesystem::exists(partPath))
        {
            std::filesystem::remove(partPath);
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        lastError_ = fmt::format("Failed to remove old .part file {}: {}",
                                 partPath.string(), e.what());
        return false;
    }

    // 3. Open .part file for writing (not final destination yet)
    std::ofstream outFile(partPath, std::ios::binary);
    if (!outFile)
    {
        lastError_ = fmt::format("Cannot open file for writing: {}", partPath.string());
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
    diskSpaceChecked_ = false;
    currentDestination_ = finalPath;

    // 7. Try to get content length for disk space check
    // Note: This is set BEFORE download starts via headers
    curl_easy_setopt(curl_.get(), CURLOPT_NOBODY, 1L); // HEAD request to get size
    CURLcode headRes = curl_easy_perform(curl_.get());

    curl_off_t contentLength = 0;
    
    if (headRes == CURLE_OK)
    {
        curl_easy_getinfo(curl_.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
    }
    
    // Check disk space if we know the size (from HEAD or we'll check in progress callback)
    if (contentLength > 0)
    {
        if (!checkDiskSpace(finalPath, contentLength))
        {
            outFile.close();
            std::filesystem::remove(partPath); // Clean up .part file
            return false;
        }
        diskSpaceChecked_ = true;  // Mark as checked
    }

    // Reset to actual download (not HEAD request)
    curl_easy_setopt(curl_.get(), CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl_.get(), CURLOPT_HTTPGET, 1L);

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
        // Leave .part file for potential resume in TASK-005
        return false;
    }

    // Check HTTP response code
    long httpCode = 0;
    curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode >= 400)
    {
        lastError_ = fmt::format("HTTP error {}: {}", httpCode, getHttpStatusText(httpCode));
        // Leave .part file for debugging
        return false;
    }

    // 8. Success! Rename .part to final filename (atomic operation)
    try
    {
        std::filesystem::rename(partPath, finalPath);
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        lastError_ = fmt::format("Download succeeded but failed to rename {} to {}: {}",
                                 partPath.string(), finalPath.string(), e.what());
        return false;
    }

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

    // If we haven't checked disk space yet and now know the total size, check it
    // Do this FIRST before throttling to ensure safety checks happen immediately
    if (!client->diskSpaceChecked_ && dltotal > 0)
    {
        if (!client->checkDiskSpace(client->currentDestination_, dltotal))
        {
            // Abort download by returning non-zero
            return 1;
        }
        client->diskSpaceChecked_ = true;
    }

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

// Ensure directory exists for file path
bool HttpClient::ensureDirectoryExists(const std::filesystem::path &filePath)
{
    try
    {
        // Get the parent directory of the file
        auto directory = filePath.parent_path();

        // If parent directory is empty (file in current dir), nothing to create
        if (directory.empty())
        {
            return true;
        }

        // Check if directory already exists
        if (std::filesystem::exists(directory))
        {
            return true;
        }

        // Create all parent directories (like mkdir -p)
        std::filesystem::create_directories(directory);
        return true;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        lastError_ = fmt::format("Failed to create directory for {}: {}",
                                 filePath.string(), e.what());
        return false;
    }
}

// Check if there's enough disk space
bool HttpClient::checkDiskSpace(const std::filesystem::path &filePath, curl_off_t requiredBytes)
{
    try
    {
        // If size is unknown (0 or negative), skip the check
        if (requiredBytes <= 0)
        {
            return true;
        }

        // Get the directory where file will be saved
        auto directory = filePath.parent_path();
        if (directory.empty())
        {
            directory = "."; // Current directory
        }

        // Query filesystem space information
        auto spaceInfo = std::filesystem::space(directory);

        // spaceInfo.available = bytes available to non-privileged process
        // Add 10% buffer to be safe (some filesystems reserve space)
        curl_off_t requiredWithBuffer = requiredBytes + (requiredBytes / 10);

        if (spaceInfo.available < static_cast<std::uintmax_t>(requiredWithBuffer))
        {
            lastError_ = fmt::format("Insufficient disk space: need {} (+ 10% buffer) but only {} available",
                                     formatBytes(requiredBytes),
                                     formatBytes(static_cast<curl_off_t>(spaceInfo.available)));
            return false;
        }

        return true;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        // If we can't check space, log warning but proceed
        // (some filesystems don't support space queries)
        fmt::print(stderr, "Warning: Unable to check disk space: {}\n", e.what());
        return true; // Optimistically proceed
    }
}

// Generate .part filename
std::filesystem::path HttpClient::makePartPath(const std::filesystem::path &destination) const
{
    // Simply append ".part" to the filename
    std::filesystem::path partPath = destination;
    partPath += ".part";
    return partPath;
}