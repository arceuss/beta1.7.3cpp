#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Full FIPS 180-4 SHA-256 for the stress tool's deterministic state digests.
// Streaming interface so multi-megabyte chunk arrays hash without copies.
namespace stress
{
class Sha256
{
public:
	Sha256();

	void update(const void *data, std::size_t length);
	// Finalizes and returns the 64-character lowercase hex digest. The object
	// must not be updated afterwards; construct a fresh hasher per digest.
	std::string finishHex();

private:
	void processBlock(const std::uint8_t *block);

	std::uint32_t state[8];
	std::uint8_t buffer[64];
	std::size_t buffered;
	std::uint64_t totalBytes;
};
}
