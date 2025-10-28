#include "http_client.hpp"
#include <fstream>
#include <iostream>
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

    // Perform the download
    CURLcode res = curl_easy_perform(curl_.get());

    // Close file before checking result (ensures data is flushed)
    outFile.close();

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