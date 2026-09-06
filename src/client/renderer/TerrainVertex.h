#pragma once

#include <cstddef>

#include "OpenGL.h"

// B173 - Central terrain vertex layout (report: compact terrain vertex format).
// The tesselator writes 32-byte slots; the terrain capture path repacks them to
// this layout before upload. Position 12 bytes, UV 8, RGBA 4, packed normal 4.
// Bytes 28-31 of the tesselator slot are never consumed by any terrain draw.
//
// Rollback flags (report rollback plan):
//   B173_COMPACT_TERRAIN_VERTEX=0 keeps the 32-byte capture layout.
//   B173_INDEXED_TERRAIN=0 keeps six explicit vertices per quad and glDrawArrays.
#ifndef B173_COMPACT_TERRAIN_VERTEX
#define B173_COMPACT_TERRAIN_VERTEX 1
#endif
#ifndef B173_INDEXED_TERRAIN
#define B173_INDEXED_TERRAIN 1
#endif

#if B173_COMPACT_TERRAIN_VERTEX
constexpr GLsizei TERRAIN_VERTEX_STRIDE = 28;
#else
constexpr GLsizei TERRAIN_VERTEX_STRIDE = 32;
#endif

constexpr std::size_t TERRAIN_POS_OFFSET = 0;
constexpr std::size_t TERRAIN_UV_OFFSET = 12;
constexpr std::size_t TERRAIN_COLOR_OFFSET = 20;
constexpr std::size_t TERRAIN_NORMAL_OFFSET = 24;

static_assert(TERRAIN_UV_OFFSET == TERRAIN_POS_OFFSET + 3 * sizeof(float), "position is three floats");
static_assert(TERRAIN_COLOR_OFFSET == TERRAIN_UV_OFFSET + 2 * sizeof(float), "uv is two floats");
static_assert(TERRAIN_NORMAL_OFFSET == TERRAIN_COLOR_OFFSET + 4, "color is four bytes");
static_assert(TERRAIN_NORMAL_OFFSET + 4 <= static_cast<std::size_t>(TERRAIN_VERTEX_STRIDE), "normal fits in stride");
