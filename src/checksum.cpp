#include "checksum.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <fmt/core.h>

// OpenSSL headers for SHA-256
#include <openssl/evp.h>
#include <openssl/sha.h>

std::string ChecksumVerifier::computeSHA256(const std::filesystem::path &filePath)
{
    // Step 1: Open file in binary mode
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error(
            fmt::format("Cannot open file for checksum: {}", filePath.string()));
    }

    // Step 2: Initialize OpenSSL SHA-256 context
    // EVP = "Envelope" API (high-level cryptography interface)
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (!context)
    {
        throw std::runtime_error("Failed to create OpenSSL context");
    }

    // RAII wrapper to ensure context is freed even if exception occurs
    auto contextDeleter = [](EVP_MD_CTX *ctx)
    {
        if (ctx)
            EVP_MD_CTX_free(ctx);
    };
    std::unique_ptr<EVP_MD_CTX, decltype(contextDeleter)> contextGuard(context, contextDeleter);

    // Step 3: Initialize digest operation for SHA-256
    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1)
    {
        throw std::runtime_error("Failed to initialize SHA-256 digest");
    }

    // Step 4: Read file in chunks and update digest
    constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1 MB chunks for efficiency
    std::vector<char> buffer(BUFFER_SIZE);

    while (file.read(buffer.data(), BUFFER_SIZE) || file.gcount() > 0)
    {
        size_t bytesRead = static_cast<size_t>(file.gcount());
        if (EVP_DigestUpdate(context, buffer.data(), bytesRead) != 1)
        {
            throw std::runtime_error("Failed to update SHA-256 digest");
        }
    }

    // Step 5: Finalize the hash
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLength = 0;

    if (EVP_DigestFinal_ex(context, hash, &hashLength) != 1)
    {
        throw std::runtime_error("Failed to finalize SHA-256 digest");
    }

    // Step 6: Convert binary hash to hexadecimal string
    std::vector<unsigned char> hashVector(hash, hash + hashLength);
    return toHex(hashVector);
}

bool ChecksumVerifier::verify(const std::filesystem::path &filePath,
                               const std::string &expectedChecksum)
{
    // Parse the checksum string to get algorithm and hash
    auto [algorithm, expectedHash] = parseChecksum(expectedChecksum);

    // Only SHA-256 is implemented for now
    if (algorithm != Algorithm::SHA256)
    {
        throw std::runtime_error("Only SHA-256 is currently supported");
    }

    // Compute the actual checksum
    std::string actualChecksum = computeSHA256(filePath);

    // Normalize both checksums (lowercase, no whitespace)
    std::string normalizedExpected = normalizeHex(expectedHash);
    std::string normalizedActual = normalizeHex(actualChecksum);

    return normalizedExpected == normalizedActual;
}

std::pair<ChecksumVerifier::Algorithm, std::string>
ChecksumVerifier::parseChecksum(const std::string &checksumString)
{
    // Expected format: "algorithm:hexhash"
    // Example: "sha256:abc123..."

    size_t colonPos = checksumString.find(':');
    if (colonPos == std::string::npos)
    {
        throw std::runtime_error(
            "Invalid checksum format. Expected 'algorithm:hexhash'");
    }

    std::string algorithmStr = checksumString.substr(0, colonPos);
    std::string hexHash = checksumString.substr(colonPos + 1);

    // Convert algorithm string to lowercase
    std::transform(algorithmStr.begin(), algorithmStr.end(),
                   algorithmStr.begin(), ::tolower);

    Algorithm algorithm;
    if (algorithmStr == "sha256")
    {
        algorithm = Algorithm::SHA256;
    }
    else if (algorithmStr == "md5")
    {
        algorithm = Algorithm::MD5;
    }
    else if (algorithmStr == "sha1")
    {
        algorithm = Algorithm::SHA1;
    }
    else
    {
        throw std::runtime_error(
            fmt::format("Unsupported algorithm: '{}'", algorithmStr));
    }

    // Normalize the hex string
    std::string normalizedHex = normalizeHex(hexHash);

    // Validate length based on algorithm
    size_t expectedLength;
    if (algorithm == Algorithm::SHA256)
    {
        expectedLength = 64; // 256 bits / 4 bits per hex digit
    }
    else if (algorithm == Algorithm::MD5)
    {
        expectedLength = 32; // 128 bits / 4
    }
    else if (algorithm == Algorithm::SHA1)
    {
        expectedLength = 40; // 160 bits / 4
    }
    else
    {
        expectedLength = 0; // Should never happen
    }

    if (normalizedHex.length() != expectedLength)
    {
        throw std::runtime_error(
            fmt::format("Invalid {} hash length. Expected {} hex characters, got {}",
                        algorithmStr, expectedLength, normalizedHex.length()));
    }

    return {algorithm, normalizedHex};
}

std::string ChecksumVerifier::toHex(const std::vector<unsigned char> &data)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (unsigned char byte : data)
    {
        oss << std::setw(2) << static_cast<unsigned int>(byte);
    }

    return oss.str();
}

std::string ChecksumVerifier::normalizeHex(const std::string &hex)
{
    std::string result;
    result.reserve(hex.length());

    for (char ch : hex)
    {
        // Skip whitespace and common separators
        if (std::isspace(ch) || ch == ':' || ch == '-')
        {
            continue;
        }

        // Keep only hex digits
        if (std::isxdigit(ch))
        {
            result += std::tolower(ch);
        }
        else
        {
            throw std::runtime_error(
                fmt::format("Invalid character in checksum: '{}'", ch));
        }
    }

    return result;
}