#pragma once

#include <string>
#include <optional> // C++17 feature for optional values

/**
 * Configuration for the download manager.
 * Populated by CLI11 argument parser from command-line arguments.
 */
struct DownloadConfig
{
    // Required parameters
    std::string url;
    std::string destination;

    // Optional parameters with sensible defaults
    int maxRetries = 3;       // Default: 3 retries (from TASK-006)
    int timeoutSeconds = 300; // Default: 5 minutes (300 seconds)

    // Checksum verification (optional)
    std::optional<std::string> expectedChecksum; // Format: "sha256:abc123..."

    // Flags
    bool showVersion = false; // Display version and exit
};