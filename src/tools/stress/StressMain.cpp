// Adapted from arceuss/a126cpp portable src/tools/stress/StressMain.cpp.
#include "tools/stress/StressHarness.h"
#include "BetaGL.h"

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
		"  --backend NAME      compat, gl33, gles2, vulkan or d3d12; overrides B173_RENDERER\n"
		"  --frames N          Measured frames, 0 uses scenario default\n"
		"  --warmup N          Warm-up frames, default 60\n"
		"  --tick-interval N   Frames per fixed game tick, default 3\n"
		"  --sample-every N    State sample interval in frames, default 100\n"
		"  --view-distance N   0=far, 1=normal, 2=short, 3=tiny, default 0\n"
		"  --fancy 0|1         Fancy graphics and ambient occlusion, default 1\n"
		"  --ambient-occlusion 0|1  Override AO independently of fast/fancy graphics\n"
		"  --no-finish         Do not glFinish each measured/warm-up frame\n"
		"  --anaglyph 0|1      Anaglyph 3D render path, default 0\n"
		"  --occlusion 0|1     Exercise supported asynchronous occlusion queries\n"
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
		"  crops (all eight growth stages, four rows, ordinary world rendering)\n"
		"  clouds (interpolated cloud-boundary crossings and changing daylight)\n"
		"Renderer-parity scenes (fixed world, time, weather and camera; one scene each):\n"
		"  parity_terrain_above, parity_terrain_below (terrain and cube faces above/below)\n"
		"  parity_transparent (leaves, glass, ice, water, lava, fire)\n"
		"  parity_texturefx (water/lava still and flowing, fire, portal, compass, clock)\n"
		"  parity_clouds_below, parity_clouds_above\n"
		"  parity_sky_day, parity_sky_night, parity_sky_dawn (sun, moon, stars, sunrise fan;\n"
		"                      need --view-distance 0 or 1, the sky pass does not run above)\n"
		"  parity_particles (every particle kind, block-break and dispenser events)\n"
		"  parity_gui_hud, parity_gui_inventory (HUD, chat font, screen, 3D slot items)\n"
		"  parity_sign (standing and wall signs, font in world space)\n"
		"  parity_held_item [--item block|tool|flat|hand]\n"
		"  parity_break_outline [--target progress 0.05-0.95] (crack overlay plus outline)\n"
		"  parity_rain, parity_snow [--thunder 0|1 --search blocks] (biome-matched site)\n"
		"  parity_fog_underwater, parity_fog_lava (submerged exponential fog and overlay)\n"
		"  parity_fog_distance (linear fog with distance pillars; reports eye-radial mode)\n"
		"  parity_fog_nether (run built on the Hell dimension provider)\n"
		"Mobs, dropped items and other entities are covered by the mobs and entities\n"
		"scenarios; --fancy and --anaglyph select fast/fancy, ambient occlusion and anaglyph\n"
		"for every scene. Parity scenes drain the chunk queue before measuring.\n"
		"all runs each offline renderer scenario once with normal lighting and a fresh level.\n"
		"Only --seed is accepted as a scenario parameter for all. Options may be mixed.\n"
		"--backend or B173_RENDERER selects the renderer before\n"
		"device creation. Native backends must be enabled in the build. No null sink,\n"
		"fullbright, server, authentication, saved-world loading or audio-only scenarios.\n"
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
			if (BetaGL::consumeBackendArgument(i, argc, argv))
				continue;
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
			if (arg == "--null-sink" || arg == "--no-lighting" ||
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
				arg == "--sample-every" || arg == "--view-distance" || arg == "--fancy" || arg == "--ambient-occlusion" ||
				arg == "--anaglyph" || arg == "--occlusion" || arg == "--region-renderer" || arg == "--cache-clouds")
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
				if (arg == "--ambient-occlusion") options.ambientOcclusion = parsed;
				if (arg == "--anaglyph") options.anaglyph = parsed;
				if (arg == "--occlusion") options.occlusion = parsed;
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
