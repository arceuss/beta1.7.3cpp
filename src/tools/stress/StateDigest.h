#pragma once

#include <cstddef>
#include <string>

#include "java/Type.h"

class Level;

// Deterministic per-tick world digests for the stress tool (report L792-L808).
namespace stress
{
struct DigestResult
{
	std::string stateHex;   // full canonical world state
	std::string lightHex;   // lighting-only digest so light divergence is isolated
	int chunkCount = 0;     // loaded chunks covered by both digests
};

// Hashes every chunk held by the local ChunkCache (no loads triggered), in
// sorted (x, z) order, plus the world/entity/tile/RNG state enumerated by
// coverageStatement(); anything listed there as excluded is genuinely not hashed.
DigestResult digestLevel(Level &level);

// One-line honest declaration of what the digests do and do not cover.
const char *coverageStatement();
}
