// Adapted from arceuss/a126cpp portable src/tools/stress/StressMain.cpp.
#include "tools/stress/StressHarness.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

static void stressUsage()
{
	std::cout <<
		"Usage: McBetaCppStress [options] <scenario|all> [--parameter value]...\n"
		"  --frames N          Measured frames, 0 uses scenario default\n"
		"  --warmup N          Warm-up frames, default 60\n"
		"  --tick-interval N   Frames per fixed game tick, default 3\n"
		"  --sample-every N    State sample interval in frames, default 100\n"
		"  --view-distance N   0=far, 1=normal, 2=short, 3=tiny, default 0\n"
		"  --fancy 0|1         Fancy graphics and ambient occlusion, default 1\n"
		"  --no-finish         Do not glFinish each measured/warm-up frame\n"
		"  --anaglyph 0|1      Anaglyph 3D render path, default 0\n"
		"  --region-renderer 0|1  Per-chunk VBOs or pooled region storage\n"
		"  --cache-clouds 0|1  Reuse fancy-cloud geometry between its two passes\n"
		"  --frame-hash        Per-frame RGBA SHA-256 to <scenario>-frames.csv (parity only)\n"
		"  --state-hash        Per-tick canonical SHA-256 state+light digests to <scenario>-state.csv\n"
		"                      (parity runs only; invalidates frame timings)\n"
		"  --chunk-log         Ordered chunk rebuild/publish log with canonical mesh\n"
		"                      SHA-256 per publish to <scenario>-chunks.csv (parity runs only)\n"
		"  --output directory Fresh isolated output/game directory, default stress-results\n"
		"  --log filename     Report basename inside output directory\n"
		"  --capture filename Final framebuffer PNG basename inside output directory\n"
		"  --seed N           Signed 64-bit world seed, default 1234567\n"
		"Scenarios and parameters:\n"
		"  idle, spin [--rate degrees-per-tick], daycycle [--step world-ticks]\n"
		"  walk [--radius blocks], travel [--speed blocks-per-tick --distance blocks]\n"
		"  farlands [--axis x|z --sign +|- --shift blocks --speed blocks-per-tick --distance blocks]\n"
		"  building [--size blocks --floors N --torch_spacing blocks]\n"
		"  lighting [--count N --period ticks --width blocks --depth blocks]\n"
		"  fluids [--size blocks --spacing blocks], tnt [--count N --period ticks]\n"
		"  mobs [--count N], entities [--count N], cave [--width blocks --depth blocks]\n"
		"  clouds (interpolated cloud-boundary crossings and changing daylight)\n"
		"all runs each offline renderer scenario once with normal lighting and a fresh level.\n"
		"Only --seed is accepted as a scenario parameter for all. Options may be mixed.\n"
		"Real hidden OpenGL 2.1, no pacing. No backend/null sink, fullbright, server,\n"
		"authentication, saved-world loading, or audio-only scenarios.\n"
		"Existing output directories are rejected. Exit: 0 complete, 1 runtime failure, 2 usage error.\n";
}

static void stressTerminate()
{
	const std::exception_ptr current = std::current_exception();
	if (current)
	{
		try { std::rethrow_exception(current); }
		catch (const std::exception &error) { std::cerr << "stress: terminate: " << error.what() << '\n'; }
		catch (...) { std::cerr << "stress: terminate: non-standard exception\n"; }
	}
	else
		std::cerr << "stress: terminate without an active exception\n";
	std::abort();
}

namespace stress
{
// argv[0] is the program name; arguments start at argv[1], matching main().
int runCommandLine(int argc, char *argv[])
{
	std::set_terminate(&stressTerminate);
	stress::Options options;
	try
	{
		for (int i = 1; i < argc; ++i)
		{
			const std::string arg = argv[i];
			if (arg == "--help" || arg == "-h")
			{
				stressUsage();
				return 0;
			}
			if (arg == "--no-finish")
			{
				options.finishEachFrame = false;
				continue;
			}
			if (arg == "--state-hash")
			{
				options.stateHash = true;
				continue;
			}
			if (arg == "--frame-hash")
			{
				options.frameHash = true;
				continue;
			}
			if (arg == "--chunk-log")
			{
				options.chunkLog = true;
				continue;
			}
			if (arg == "--backend" || arg == "--null-sink" || arg == "--no-lighting" ||
				arg == "--no-occlusion" || arg == "--server" || arg == "--user" || arg == "--world")
				throw std::invalid_argument("Unsupported option in the Beta renderer tool: " + arg);
			if (arg.compare(0, 2, "--") != 0)
			{
				if (!options.scenario.empty())
					throw std::invalid_argument("Unexpected argument: " + arg);
				options.scenario = arg;
				continue;
			}
			if (++i >= argc)
				throw std::invalid_argument("Missing value for " + arg);
			const std::string value = argv[i];
			if (arg == "--output") options.outputDirectory = value;
			else if (arg == "--log") options.logPath = value;
			else if (arg == "--capture") options.capturePath = value;
			else if (arg == "--frames" || arg == "--warmup" || arg == "--tick-interval" ||
				arg == "--sample-every" || arg == "--view-distance" || arg == "--fancy" ||
				arg == "--anaglyph" || arg == "--region-renderer" || arg == "--cache-clouds")
			{
				const long_t number = stress::parseInteger(value);
				if (number < 0 || number > 1000000)
					throw std::invalid_argument("Out of range: " + arg);
				const int parsed = static_cast<int>(number);
				if (arg == "--frames") options.frames = parsed;
				if (arg == "--warmup") options.warmupFrames = parsed;
				if (arg == "--tick-interval") options.tickInterval = parsed;
				if (arg == "--sample-every") options.sampleEvery = parsed;
				if (arg == "--view-distance") options.viewDistance = parsed;
				if (arg == "--fancy") options.fancyGraphics = parsed;
				if (arg == "--anaglyph") options.anaglyph = parsed;
				if (arg == "--region-renderer") options.regionRenderer = parsed;
				if (arg == "--cache-clouds") options.cacheClouds = parsed;
			}
			else
				options.params.values[arg.substr(2)] = value;
		}
		stress::validateOptions(options);
	}
	catch (const std::exception &error)
	{
		std::cerr << "stress: " << error.what() << '\n';
		stressUsage();
		return 2;
	}
	return stress::run(options);
}
}

// The PGO training build embeds this translation unit in the game executable
// (B173_PGO_STRESS_EMBED); the game's own main dispatches --stress to
// stress::runCommandLine, so only the standalone tool defines main here.
#ifndef B173_PGO_STRESS_EMBED
int main(int argc, char *argv[])
{
	return stress::runCommandLine(argc, argv);
}
#endif
