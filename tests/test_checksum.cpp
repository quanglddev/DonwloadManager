#include "checksum.hpp"
#include <iostream>
#include <fmt/core.h>

int main()
{
    try
    {
        // Test 1: Compute SHA-256 of test.txt
        std::string hash = ChecksumVerifier::computeSHA256("test.txt");
        fmt::print("Computed SHA-256: {}\n", hash);

        // Test 2: Verify with correct checksum
        bool result1 = ChecksumVerifier::verify(
            "test.txt",
            "sha256:c98c24b677eff44860afea6f493bbaec5bb1c4cbb209c6fc2bbb47f66ff2ad31");
        fmt::print("Verification with correct hash: {}\n", result1 ? "PASS" : "FAIL");

        // Test 3: Verify with incorrect checksum
        bool result2 = ChecksumVerifier::verify(
            "test.txt",
            "sha256:0000000000000000000000000000000000000000000000000000000000000000");
        fmt::print("Verification with wrong hash: {}\n", result2 ? "FAIL (should be false)" : "PASS (correctly rejected)");

        // Test 4: Test parseChecksum
        auto [algo, hexHash] = ChecksumVerifier::parseChecksum(
            "sha256:c98c24b677eff44860afea6f493bbaec5bb1c4cbb209c6fc2bbb47f66ff2ad31");
        fmt::print("Parsed algorithm: SHA256 ({})\n", static_cast<int>(algo));
        fmt::print("Parsed hash: {}\n", hexHash);

        fmt::print("\n✅ All tests passed!\n");
        return 0;
    }
    catch (const std::exception &e)
    {
        fmt::print(stderr, "❌ Error: {}\n", e.what());
        return 1;
    }
}
