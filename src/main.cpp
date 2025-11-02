#include <iostream>
#include <fmt/core.h>
#include <CLI/CLI.hpp> // CLI11 main header
#include "http_client.hpp"
#include "config.hpp"
#include "checksum.hpp"

int main(int argc, char *argv[])
{
    // Quick check for --version flag before full parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            fmt::print("Download Manager v1.0\n");
            fmt::print("Built with:\n");
            fmt::print("  - libcurl: HTTP/HTTPS support\n");
            fmt::print("  - CLI11: Command-line parsing\n");
            fmt::print("  - fmt: Modern string formatting\n");
            return 0;
        }
    }

    // Create CLI11 app
    CLI::App app{"Download Manager v1.0 - Multi-threaded file downloader"};

    // Configuration struct to be populated
    DownloadConfig config;

    // ====================================================================
    // DEFINE ARGUMENTS
    // ====================================================================

    // Required positional argument: URL
    app.add_option("URL", config.url, "HTTP/HTTPS URL to download")
        ->required()
        ->check([](const std::string &url) -> std::string {
            // Custom validator: check if URL starts with http:// or https://
            if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
                return "";  // Empty string = valid
            }
            return "URL must start with http:// or https://";
        });

    // Required positional argument: DESTINATION
    app.add_option("DESTINATION", config.destination, "Local file path to save")
        ->required();

    // Optional flag: --retry-count (or --max-retries)
    app.add_option("-r,--retry-count,--max-retries", config.maxRetries,
                   "Maximum retry attempts for transient errors")
        ->check(CLI::Range(0, 10)) // Built-in validator: 0-10 range
        ->default_val(3);

    // Optional flag: --timeout
    app.add_option("-t,--timeout", config.timeoutSeconds,
                   "Timeout in seconds for download")
        ->check(CLI::PositiveNumber) // Built-in validator: must be positive
        ->default_val(300);

    // Optional flag: --checksum
    app.add_option("-c,--checksum", config.expectedChecksum,
                   "Expected checksum in format 'algorithm:hexhash' (e.g., sha256:abc123...)")
        ->check([](const std::string &cs) -> std::string {
            if (cs.empty()) return "";
            try {
                ChecksumVerifier::parseChecksum(cs);
                return ""; // Valid
            } catch (const std::exception &e) {
                return std::string("Invalid checksum format: ") + e.what();
            }
        });

    // Optional flag: --version (for help display only, actual handling is done above)
    app.add_flag("-v,--version", config.showVersion, "Display version information");

    // ====================================================================
    // PARSE ARGUMENTS
    // ====================================================================

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        // CLI11 automatically generates beautiful help/error messages
        return app.exit(e);
    }

    // ====================================================================
    // DISPLAY CONFIGURATION
    // ====================================================================

    fmt::print("Download Manager v1.0\n");
    fmt::print("====================================\n\n");

    fmt::print("Configuration:\n");
    fmt::print("  URL:         {}\n", config.url);
    fmt::print("  Destination: {}\n", config.destination);
    fmt::print("  Max Retries: {}\n", config.maxRetries);
    fmt::print("  Timeout:     {}s\n", config.timeoutSeconds);
    if (config.expectedChecksum) {
        fmt::print("  Checksum:    {}\n", config.expectedChecksum.value());
    }
    fmt::print("\n");

    // ====================================================================
    // PERFORM DOWNLOAD
    // ====================================================================

    try
    {
        // Create HTTP client (RAII ensures cleanup)
        HttpClient client;

        // Apply configuration
        client.setMaxRetries(config.maxRetries);

        fmt::print("Starting download...\n\n");

        // Perform download with configured timeout
        if (client.downloadFile(config.url, config.destination, config.timeoutSeconds))
        {
            fmt::print("✓ Download completed successfully");

            // Show retry count if there were any retries
            int retryCount = client.getRetryCount();
            if (retryCount > 0)
            {
                fmt::print(" (after {} {})", retryCount, retryCount == 1 ? "retry" : "retries");
            }
            fmt::print("!\n");

            // Verify checksum if provided
            if (config.expectedChecksum)
            {
                fmt::print("\n");
                fmt::print("Verifying checksum...\n");
                try
                {
                    bool isValid = ChecksumVerifier::verify(
                        config.destination,
                        config.expectedChecksum.value());

                    if (isValid)
                    {
                        fmt::print("✓ Checksum verification passed!\n");
                    }
                    else
                    {
                        fmt::print(stderr, "✗ Checksum verification FAILED!\n");
                        fmt::print(stderr, "  Expected: {}\n", config.expectedChecksum.value());
                        fmt::print(stderr, "  File may be corrupted or incomplete.\n");
                        
                        // Move file to quarantine
                        std::filesystem::path quarantinePath = 
                            std::filesystem::path(config.destination).parent_path() / "quarantine";
                        std::filesystem::create_directories(quarantinePath);
                        
                        std::filesystem::path quarantineFile = 
                            quarantinePath / std::filesystem::path(config.destination).filename();
                        
                        std::filesystem::rename(config.destination, quarantineFile);
                        fmt::print(stderr, "  File moved to: {}\n", quarantineFile.string());
                        
                        return 1;
                    }
                }
                catch (const std::exception &e)
                {
                    fmt::print(stderr, "✗ Checksum verification error: {}\n", e.what());
                    return 1;
                }
            }

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