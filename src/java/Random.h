#pragma once

#include "java/Type.h"

// Java evaluates operands and call arguments left to right; C++ leaves the
// order of two calls inside one expression unspecified, and MSVC's PGO/LTCG
// codegen was observed flipping it. Never draw twice inside one expression:
// hoist each draw into a local in Java source order first.
class Random
{
private:
	ulong_t seed;
	bool haveNextNextGaussian = false;
	double nextNextGaussian = 0.0;
public:
	Random();
	Random(long_t set_seed);

	// State accessors for the deterministic stress-tool digest; no behavior change.
	ulong_t rawState() const { return seed; }
	bool hasPendingGaussian() const { return haveNextNextGaussian; }
	double pendingGaussian() const { return nextNextGaussian; }

	// Stress-tool repeatability: when enabled (before any construction), every
	// default-constructed Random draws from a deterministic sequence instead of
	// the wall clock. Never enabled by the game itself.
	static void enableDeterministicDefaultSeeds(long_t base);

	void setSeed(long_t set_seed);

	int_t next(int_t bits);

	bool nextBoolean();

	int_t nextInt();
	int_t nextInt(int_t bound);

	long_t nextLong();

	float nextFloat();
	double nextDouble();
	double nextGaussian();
};
