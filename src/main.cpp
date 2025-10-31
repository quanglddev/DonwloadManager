#include <iostream>
#include <fmt/core.h>
#include "http_client.hpp"

int main(int argc, char *argv[])
{
    fmt::print("Download Manager v1.0\n");
    fmt::print("====================================\n\n");

    // Check command-line arguments
    if (argc != 3)
    {
        fmt::print(stderr, "Usage: {} <URL> <destination>\n", argv[0]);
        fmt::print(stderr, "Example: {} http://httpbin.org/bytes/1048576 test.bin\n", argv[0]);
        return 1;
    }

    std::string url = argv[1];
    std::string destination = argv[2];

    try
    {
        // Create HTTP client (RAII ensures cleanup)
        HttpClient client;

        fmt::print("Downloading: {}\n", url);
        fmt::print("Saving to: {}\n", destination);
        fmt::print("Starting download...\n\n");

        // Perform download
        if (client.downloadFile(url, destination))
        {
            fmt::print("✓ Download completed successfully");

            // Show retry count if there were any retries
            int retryCount = client.getRetryCount();
            if (retryCount > 0)
            {
                fmt::print(" (after {} {})", retryCount, retryCount == 1 ? "retry" : "retries");
            }
            fmt::print("!\n");

            return 0;
        }
        else
        {
            fmt::print(stderr, "✗ Download failed: {}\n", client.getLastError());
            return 1;
        }
    }
    catch (const std::exception &e)
    {
        fmt::print(stderr, "✗ Fatal error: {}\n", e.what());
        return 1;
    }
}