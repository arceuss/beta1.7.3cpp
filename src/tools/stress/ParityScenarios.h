#pragma once

#include <memory>
#include <string>
#include <vector>

#include "java/Type.h"
#include "tools/stress/StressHarness.h"

namespace stress
{
// Deterministic single-scene coverage for the portable renderer bring-up. Every
// entry below pins a fixed world, time of day, weather and camera so the same
// frame index can be compared between the OpenGL 2.1 oracle and a new backend.
//
// These are ordinary Scenario implementations driving the real game: world tiles,
// tile entities, entities, player inventory, screens, game-mode digging, weather
// and dimensions. Nothing here draws its own geometry or fabricates images.
namespace parity
{
// Registry order, appended to scenarioNames() and used by the "all" sweep.
const std::vector<std::string> &names();

// Nullptr when name is not a parity scene.
std::unique_ptr<Scenario> make(const std::string &name);

// Allowed --parameter keys for a parity scene, nullptr when name is not one.
// "seed" is accepted for every scenario by the runner itself.
const std::vector<std::string> *params(const std::string &name);

// Dimension id the run's Level must be built with (Dimension::Id_Normal for
// every scene except the Nether fog one).
int_t dimension(const std::string &name);

// Parity scenes need the chunk rebuild queue fully drained before the measured
// frames, so a frozen capture never depends on the per-frame chunk budget.
bool needsFullSettle(const std::string &name);

// Largest --view-distance at which the scene still renders every pass it exists
// for; LevelRenderer::renderSky only runs below 2, so the sky scenes cap there.
// 3 (the runner maximum) when the scene does not care.
int maxViewDistance(const std::string &name);
} // namespace parity
} // namespace stress
