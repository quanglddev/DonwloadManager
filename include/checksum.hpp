#pragma once

#include <string>
#include <vector>
#include <filesystem>

/**
 * File integrity verification using cryptographic hashes.
 * Supports SHA-256 (extensible to MD5, SHA-1 later).
 */
class ChecksumVerifier
{
public:
    /**
     * Supported hash algorithms.
     */
    enum class Algorithm
    {
        SHA256,
        MD5, // For future implementation
        SHA1 // For future implementation
    };

    /**
     * Compute SHA-256 hash of a file.
     * Reads file in chunks to avoid loading entire file into memory.
     *
     * @param filePath Path to file to hash
     * @return Hex-encoded hash string (64 characters for SHA-256)
     * @throws std::runtime_error if file cannot be read
     */
    static std::string computeSHA256(const std::filesystem::path &filePath);

    /**
     * Verify a file matches an expected checksum.
     *
     * @param filePath Path to file to verify
     * @param expectedChecksum Expected hash in format "algorithm:hexhash"
     *                         Example: "sha256:abc123..."
     * @return true if checksums match, false otherwise
     * @throws std::runtime_error if format is invalid or algorithm unsupported
     */
    static bool verify(const std::filesystem::path &filePath,
                       const std::string &expectedChecksum);

    /**
     * Parse checksum string into algorithm and hash.
     * Format: "algorithm:hexhash"
     *
     * @param checksumStr Input string (e.g., "sha256:abc123...")
     * @return Pair of (algorithm, hex hash)
     * @throws std::runtime_error if format is invalid
     */
    static std::pair<Algorithm, std::string> parseChecksum(const std::string &checksumStr);

private:
    /**
     * Convert binary data to hex string.
     * Example: {0x01, 0xFF} → "01ff"
     */
    static std::string toHex(const std::vector<unsigned char> &data);

    /**
     * Convert hex string to lowercase and remove whitespace.
     * Makes comparison case-insensitive.
     */
    static std::string normalizeHex(const std::string &hex);

    // Chunk size for file reading (1 MB)
    static constexpr size_t CHUNK_SIZE = 1024 * 1024;
};