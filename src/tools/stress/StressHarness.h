#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "java/Type.h"

class Minecraft;
class Level;
class LocalPlayer;

// Adapted from arceuss/a126cpp portable src/tools/stress/StressHarness.h.
namespace stress
{
class Params
{
public:
	std::map<std::string, std::string> values;

	bool has(const std::string &key) const;
	int intOr(const std::string &key, int fallback) const;
	long_t longOr(const std::string &key, long_t fallback) const;
	double doubleOr(const std::string &key, double fallback) const;
	std::string stringOr(const std::string &key, const std::string &fallback) const;
};

struct Options
{
	std::string scenario;
	std::string outputDirectory = "stress-results";
	std::string logPath;
	std::string capturePath;
	int fancyGraphics = 1;
	int frames = 0;
	int tickInterval = 3;
	int warmupFrames = 60;
	int sampleEvery = 100;
	int viewDistance = 0;
	int anaglyph = 0;
	int regionRenderer = -1;
	int cacheClouds = -1;
	bool frameHash = false;
	bool finishEachFrame = true;
	// Opt-in per-tick SHA-256 state/light digests; costs real time per tick,
	// so it stays off unless a parity run asks for it.
	bool stateHash = false;
	// Opt-in ordered rebuild/publish log with canonical mesh SHA-256 per publish.
	bool chunkLog = false;
	Params params;
};

struct World
{
	Minecraft &minecraft;
	Level &level;
	LocalPlayer &player;
	long_t seed;
	int tickInterval;
};

class Scenario
{
public:
	virtual ~Scenario() = default;
	virtual const char *name() const = 0;
	virtual int defaultFrames() const = 0;
	virtual void setup(World &world, const Params &params) = 0;
	virtual bool settleBeforeMeasure() const { return true; }
	virtual void onTick(World &world, long_t tick) = 0;
	virtual void report(World &, std::vector<std::string> &) {}
};

std::unique_ptr<Scenario> makeScenario(const std::string &name);
std::vector<std::string> scenarioNames();
void pinPlayer(LocalPlayer &player, double x, double y, double z, float yRot, float xRot);
long_t parseInteger(const std::string &text);
void validateOptions(const Options &options);
int run(const Options &options);
// Full CLI entry (parsing, usage, terminate handler); used by the standalone
// stress executable and by the PGO-embedded --stress dispatch in the game.
int runCommandLine(int argc, char *argv[]);
}
