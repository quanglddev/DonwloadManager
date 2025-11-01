#include "http_client.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

#include <fmt/core.h>
#include <thread>
#include <random>

HttpClient::HttpClient() : curl_(curl_easy_init(), curl_easy_cleanup)
{

    if (!curl_)
    {
        lastError_ = "Failed to initialized CURL (out of memory or library error)";
        throw std::runtime_error(lastError_);
    }

    // Set a user-agent (some servers block requests without one)
    curl_easy_setopt(curl_.get(), CURLOPT_USERAGENT, "DownloadManager/1.90");

    // Detect if stdout is a terminal to decide how we render the progress bar
    isTerminalOutput_ = ::isatty(fileno(stdout));
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
    outFile->write(ptr, static_cast<std::streamsize>(totalSize));
    if (!outFile->good())
    {
        return 0; // Abort transfer if write fails
    }

    // If we return 0 or a different value, libcurl aborts the transfer
    return totalSize;
}

bool HttpClient::downloadFile(const std::string &url, const std::string &destination, int timeoutSeconds)
{
    // Convert to filesystem path for easier manipulation
    std::filesystem::path finalPath(destination);
    std::filesystem::path partPath = makePartPath(finalPath);

    // Reset retry count for this download
    retryCount_ = 0;

    // 1. Ensure destination directory exists
    if (!ensureDirectoryExists(finalPath))
    {
        return false; // Error already set in lastError_
    }

    // 2. Check if .part file exists from previous download (for resume)
    resumeOffset_ = 0; // Default: start from beginning
    try
    {
        if (std::filesystem::exists(partPath))
        {
            // Get size of existing partial file
            resumeOffset_ = static_cast<curl_off_t>(std::filesystem::file_size(partPath));
            if (resumeOffset_ > 0)
            {
                fmt::print("Found existing partial download ({} already downloaded).\nAttempting to resume...\n",
                           formatBytes(resumeOffset_));
            }
            else
            {
                // Empty .part file, remove it and start fresh
                std::filesystem::remove(partPath);
            }
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        // If we can't read the .part file, remove it and start fresh
        fmt::print(stderr, "Warning: Could not check .part file ({}). Starting fresh download.\n", e.what());
        try
        {
            std::filesystem::remove(partPath);
        }
        catch (...)
        {
            // Ignore cleanup errors
        }
        resumeOffset_ = 0;
    }

    // 3. Open .part file for writing
    // If resuming (resumeOffset_ > 0), open in APPEND mode to continue writing
    // If starting fresh (resumeOffset_ == 0), open in TRUNCATE mode (default)
    std::ios::openmode fileMode = std::ios::binary;
    if (resumeOffset_ > 0)
    {
        fileMode |= std::ios::app; // Append mode: write at end of file
    }

    std::ofstream outFile(partPath, fileMode);
    if (!outFile)
    {
        lastError_ = fmt::format("Cannot open file for writing: {}", partPath.string());
        return false;
    }

    // 1. Set URL
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

    // 5. Set timeout to prevent hanging forever (configurable via CLI)
    curl_easy_setopt(curl_.get(), CURLOPT_TIMEOUT, static_cast<long>(timeoutSeconds));
    curl_easy_setopt(curl_.get(), CURLOPT_CONNECTTIMEOUT, 30L); // 30s to establish connection

    // 6. Enable progress tracking
    curl_easy_setopt(curl_.get(), CURLOPT_NOPROGRESS, 0L); // Enable progress meter
    curl_easy_setopt(curl_.get(), CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl_.get(), CURLOPT_XFERINFODATA, this);

    startTime_ = std::chrono::steady_clock::now();
    lastDownloaded_ = 0;
    lastProgressTime_ = startTime_;
    lastPrintedTime_ = startTime_;
    lastPrintedPercentage_ = -1.0;
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
        diskSpaceChecked_ = true; // Mark as checked
    }

    // Reset to actual download (not HEAD request)
    curl_easy_setopt(curl_.get(), CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl_.get(), CURLOPT_HTTPGET, 1L);

    // Configure resume if we have a partial file
    if (resumeOffset_ > 0)
    {
        // Tell libcurl to request bytes starting from resumeOffset_
        // Format: "bytes=N-" means "from byte N to end of file"
        curl_easy_setopt(curl_.get(), CURLOPT_RESUME_FROM_LARGE, resumeOffset_);
    }
    else
    {
        curl_easy_setopt(curl_.get(), CURLOPT_RESUME_FROM_LARGE, 0L);
    }

    // Perform the download with retry logic
    CURLcode res;
    int attemptCount = 0;
    bool shouldRetry = false;

    do
    {
        // Perform download attempt
        res = curl_easy_perform(curl_.get());

        // Close file after each attempt (ensures data is flushed)
        outFile.close();

        // Print newline after progress bar
        fmt::print("\n");

        // Check if download succeeded
        if (res == CURLE_OK)
        {
            break; // Success - exit retry loop
        }

        // Download failed - classify error
        long httpCode = 0;
        curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &httpCode);

        ErrorType errorType = classifyError(res, httpCode);
        attemptCount++;

        // Determine if we should retry
        shouldRetry = (errorType == ErrorType::Transient || errorType == ErrorType::Unknown) &&
                      (attemptCount < maxRetryAttempts_);

        if (shouldRetry)
        {
            // Calculate exponential backoff delay: 1s, 2s, 4s + jitter to prevent thundering herd
            int baseDelayMs = INITIAL_RETRY_DELAY_MS * (1 << (attemptCount - 1));
            // Add random jitter: ±20% variation
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(-20, 20);
            int jitterPercent = dis(gen);
            int delayMs = baseDelayMs + (baseDelayMs * jitterPercent / 100);

            fmt::print(stderr,
                       "Download failed (attempt {}/{}): {}\n"
                       "Retrying in {} seconds...\n",
                       attemptCount, maxRetryAttempts_,
                       curl_easy_strerror(res),
                       delayMs / 1000);

            // Wait before retry
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            // Reopen file in append mode for retry (resume from where we left off)
            outFile.open(partPath, std::ios::binary | std::ios::app);
            if (!outFile)
            {
                lastError_ = fmt::format("Cannot reopen file for retry: {}", partPath.string());
                return false;
            }

            // Update curl options for resumed transfer
            try
            {
                curl_off_t currentSize = std::filesystem::file_size(partPath);
                if (currentSize > resumeOffset_)
                {
                    // We made some progress, resume from current position
                    curl_easy_setopt(curl_.get(), CURLOPT_RESUME_FROM_LARGE, currentSize);
                }
            }
            catch (...)
            {
                // If we can't get file size, just retry from current offset
            }
        }
        else
        {
            // Permanent error or max retries exceeded
            if (errorType == ErrorType::Permanent)
            {
                lastError_ = fmt::format("Download failed permanently: {}", curl_easy_strerror(res));
            }
            else
            {
                lastError_ = fmt::format("Download failed after {} attempts: {}",
                                         attemptCount, curl_easy_strerror(res));
            }
            return false;
        }

    } while (shouldRetry);

    // Store retry count for statistics
    retryCount_ = attemptCount;

    // If we reach here after loop, download succeeded
    if (res != CURLE_OK)
    {
        return false;
    }

    // Continue with existing success logic (check HTTP code, verify file size, etc.)

    // Check HTTP response code
    long httpCode = 0;
    curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &httpCode);

    // Handle range request responses
    if (resumeOffset_ > 0 && httpCode == 200)
    {
        // Server sent status 200 instead of 206 = doesn't support ranges
        // This means it sent the ENTIRE file, ignoring our Range header
        fmt::print("\nServer doesn't support resume. Restarting download from beginning...\n");

        // Close and delete the .part file
        outFile.close();
        try
        {
            std::filesystem::remove(partPath);
        }
        catch (...)
        {
            // Ignore cleanup errors
        }

        // Retry download from scratch (no range request this time)
        resumeOffset_ = 0;
        return downloadFile(url, destination); // Recursive call
    }
    else if (resumeOffset_ > 0 && httpCode == 206)
    {
        // Success! Server supports ranges and sent partial content
        fmt::print("\nResume successful! Continued from byte {}.\n", resumeOffset_);
    }
    else if (httpCode >= 400)
    {
        lastError_ = fmt::format("HTTP error {}: {}", httpCode, getHttpStatusText(httpCode));
        // Leave .part file for debugging
        return false;
    }

    // 8. Verify file size (optional but recommended for integrity)
    try
    {
        curl_off_t finalSize = static_cast<curl_off_t>(std::filesystem::file_size(partPath));
        curl_off_t expectedSize = 0;
        curl_easy_getinfo(curl_.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &expectedSize);

        // Only check if server provided Content-Length
        if (expectedSize > 0)
        {
            curl_off_t totalExpected = (resumeOffset_ > 0) ? resumeOffset_ + expectedSize : expectedSize;

            if (finalSize != totalExpected)
            {
                lastError_ = fmt::format("File size mismatch: expected {} but got {}",
                                         formatBytes(totalExpected),
                                         formatBytes(finalSize));
                // Leave .part file for debugging
                return false;
            }
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        // If we can't verify size, log warning but continue
        fmt::print(stderr, "Warning: Could not verify file size: {}\n", e.what());
    }

    // 9. Success! Rename .part to final filename (atomic operation)
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

    auto now = std::chrono::steady_clock::now();
    auto timeSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now - client->startTime_)
                              .count();
    bool isComplete = (dltotal > 0 && dlnow >= dltotal);

    if (client->isTerminalOutput_)
    {
        auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - client->lastProgressTime_).count();

        // Throttle updates for terminal output
        if (!isComplete)
        {
            // Don't show progress in the first 500ms (prevents flashing for instant downloads)
            if (timeSinceStart < 500)
            {
                return 0;
            }

            // Update at most 5 times per second (200ms interval)
            if (timeSinceLastUpdate < 200)
            {
                return 0;
            }
        }

        client->lastProgressTime_ = now;
    }
    else
    {
        auto timeSinceLastPrint = std::chrono::duration_cast<std::chrono::milliseconds>(now - client->lastPrintedTime_).count();

        // For non-terminal output (e.g., piped to file), print less frequently
        if (!isComplete && timeSinceLastPrint < 1000)
        {
            return 0;
        }
    }

    // If we don't know the total size, show basic progress
    if (dltotal == 0)
    {
        // For resumed downloads, add the resume offset to show total progress
        curl_off_t totalDownloaded = dlnow + client->resumeOffset_;

        if (client->isTerminalOutput_)
        {
            fmt::print("\rDownloaded: {} | Speed: calculating...\033[K", client->formatBytes(totalDownloaded));
            std::fflush(stdout);
        }
        else
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - client->startTime_).count();
            fmt::print("Downloaded: {} | Elapsed: {}\n",
                       client->formatBytes(totalDownloaded),
                       client->formatDuration(static_cast<long>(elapsed)));
            client->lastPrintedTime_ = now;
        }
        return 0;
    }

    // For resumed downloads:
    // - dltotal = size of THIS request (remaining bytes)
    // - dlnow = bytes downloaded in THIS session
    // We need to add resumeOffset_ to show actual total progress
    curl_off_t totalDownloaded = dlnow + client->resumeOffset_;
    curl_off_t totalSize = dltotal + client->resumeOffset_;

    double percentage = (static_cast<double>(totalDownloaded) / totalSize) * 100.0;

    // Avoid over-printing in non-terminal environments
    if (!client->isTerminalOutput_ && !isComplete)
    {
        if (client->lastPrintedPercentage_ >= 0.0 && percentage < client->lastPrintedPercentage_ + 1.0)
        {
            return 0;
        }
    }

    // Calculate elapsed time (reuse 'now' from throttling check above)
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - client->startTime_).count();

    // Calculate download speed (bytes per second) - use dlnow for current session speed
    double speed = (elapsed > 0) ? static_cast<double>(dlnow) / elapsed : 0.0;

    // Calculate ETA - use remaining bytes (totalSize - totalDownloaded)
    long eta = (speed > 0) ? static_cast<long>((totalSize - totalDownloaded) / speed) : 0;

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

    if (client->isTerminalOutput_)
    {
        fmt::print("\r{} {:.1f}% | {} / {} | {} | ETA: {}\033[K",
                   bar,
                   percentage,
                   client->formatBytes(totalDownloaded),
                   client->formatBytes(totalSize),
                   speedStr,
                   client->formatDuration(eta));
        std::fflush(stdout);
        client->lastPrintedTime_ = now;
    }
    else
    {
        fmt::print("{} {:.1f}% | {} / {} | {} | ETA: {}\n",
                   bar,
                   percentage,
                   client->formatBytes(totalDownloaded),
                   client->formatBytes(totalSize),
                   speedStr,
                   client->formatDuration(eta));
        client->lastPrintedTime_ = now;
    }

    client->lastPrintedPercentage_ = percentage;

    // Return 0 to continue download (non-zero would abort)
    return 0;
}

// Format bytes into human-readable string
std::string HttpClient::formatBytes(curl_off_t bytes) const
{
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;

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

// Classify error for retry logic
HttpClient::ErrorType HttpClient::classifyError(CURLcode code, long httpCode) const
{
    // First, check CURL-level errors (network, DNS, etc.)
    switch (code)
    {
    // Transient network errors - worth retrying
    case CURLE_OPERATION_TIMEDOUT:   // Server didn't respond in time
    case CURLE_COULDNT_RESOLVE_HOST: // DNS lookup failed (might be temporary)
    case CURLE_COULDNT_CONNECT:      // Connection refused (server might be restarting)
    case CURLE_PARTIAL_FILE:         // Transfer ended early (network interruption)
    case CURLE_RECV_ERROR:           // Error receiving data (network glitch)
    case CURLE_SEND_ERROR:           // Error sending data (network glitch)
    case CURLE_GOT_NOTHING:          // Server sent no data (might be overloaded)
        return ErrorType::Transient;

    // Permanent errors - retrying won't help
    case CURLE_URL_MALFORMAT:          // Invalid URL syntax
    case CURLE_UNSUPPORTED_PROTOCOL:   // Protocol not supported (http/https/ftp)
    case CURLE_FILE_COULDNT_READ_FILE: // Can't read local file
    case CURLE_OUT_OF_MEMORY:          // System resource exhaustion
    case CURLE_SSL_CERTPROBLEM:        // SSL certificate invalid
    case CURLE_SSL_CIPHER:             // SSL cipher negotiation failed
        return ErrorType::Permanent;

    // No CURL error - check HTTP status code
    case CURLE_OK:
        if (httpCode >= 400 && httpCode < 500)
        {
            // 4xx Client Errors - usually permanent
            // 404 Not Found, 403 Forbidden, 401 Unauthorized
            return ErrorType::Permanent;
        }
        else if (httpCode >= 500 && httpCode < 600)
        {
            // 5xx Server Errors - usually transient (server overload, temporary issues)
            // 500 Internal Server Error, 503 Service Unavailable
            return ErrorType::Transient;
        }
        return ErrorType::Unknown;

    // Unknown CURL error - be conservative and retry
    default:
        return ErrorType::Unknown;
    }
}