# mcbetacpp

c++ port project aimed at minecraft beta 1.7.3 parity.

the codebase is being pushed forward toward beta 1.7.3 from an older beta baseline. beta 1.8 / adventure update and anything later are out of scope.

## building

you need cmake 3.14+ and a working c++ compiler. third-party code is vendored under `external/`, so the normal build is:

```bash
cmake -S . -B build
cmake --build build --config Debug --target McBetaCpp
```

cmake sends runtime output to `bin/`. with the debug command above, the executable ends up at `bin/Debug/McBetaCpp.exe` on the current windows build tree.

### alphaplace client

to build the alphaplace client variant, configure a separate build tree with `B173_TARGET_ALPHAPLACE` enabled. using a separate output suffix lets it live alongside the normal beta build:

```bash
cmake -S . -B build-alphaplace -DB173_TARGET_ALPHAPLACE=ON -DB173_OUTPUT_SUFFIX=-alphaplace
cmake --build build-alphaplace --config Debug --target McBetaCpp
```

this produces `McBetaCpp-alphaplace` instead of replacing the normal `McBetaCpp` executable. leave `B173_TARGET_ALPHAPLACE` off for the regular beta 1.7.3 client.

### optional build settings

all of these default off and keep the rendered output identical:

- `-DB173_ENABLE_IPO=ON` enables link-time optimization when the toolchain supports it.
- `-DB173_PGO=GENERATE` builds instrumented executables; run `McBetaCpp --stress <scenario> ...`, then reconfigure a second tree with `-DB173_PGO=USE -DB173_PGO_DIR=<same dir>`. a `USE` configure fails when the profile files are missing.
- `-DB173_USE_PCH=ON` and `-DB173_USE_UNITY=ON` speed up developer builds (cmake 3.16+).
- `-DB173_REGION_RENDERER=ON` stores terrain in per-region GPU buffers instead of one buffer per chunk.
- `-DB173_CACHE_CLOUDS=ON` uploads fancy-cloud geometry once per frame and draws it in both passes.
- `-DB173_FAST_LIGHT_ACCESS=ON` uses a scoped chunk accessor inside light updates.

`McBetaCppStress` accepts `--fancy 0|1`, `--ambient-occlusion 0|1`, `--anaglyph 0|1`, `--occlusion 0|1`, `--region-renderer 0|1`, `--cache-clouds 0|1`, `--state-hash`, `--chunk-log`, and `--frame-hash` for parity comparisons. Runtime resources are staged next to the executable, including configuration-specific directories such as `bin/Debug/resource`. Only changed resource files are copied; unrelated runtime files are left alone.

## renderer selection

Use `--backend NAME` when launching the game or stress tool:

| value | renderer |
| --- | --- |
| `compat` | original compatibility renderer, the default |
| `gl33` | OpenGL 3.3 Core |
| `gles2` | OpenGL ES 2.0 API |
| `vulkan` | Vulkan, when enabled in the build |
| `d3d12` | Direct3D 12 on Windows, when enabled in the build |

The renderer is selected before creating its graphics context or device. An unavailable renderer fails explicitly instead of silently selecting another one.

Both `--backend vulkan` and `--backend=vulkan` are accepted. The argument takes precedence over `B173_RENDERER`; without either, the default is `compat`. Game username, session and server arguments keep their existing order.

Vulkan and Direct3D 12 default off at build time. A Windows build with both enabled is:

```bash
cmake -S . -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DB173_RENDER_VULKAN=ON -DB173_RENDER_D3D12=ON
cmake --build build-native --target McBetaCpp McBetaCppStress
```

Vulkan builds need the Vulkan SDK and `glslangValidator`. Direct3D 12 builds need the Windows SDK. Neither SDK is required when its backend is disabled. On non-Windows systems, leave `B173_RENDER_D3D12` off. Keep the staged `resource` directory with the executable when moving the build.

For example, in PowerShell after that single-configuration build:

```powershell
.\bin\McBetaCpp.exe --backend vulkan
```

## renderer verification

Build the renderer regression and image-comparison tools with:

```bash
cmake --build build-native --target McBetaCppRendererSemantics McBetaCppLineGeometrySemantics McBetaCppRendererSmoke McBetaCppFrameCompare
```

`McBetaCppRendererSemantics` and `McBetaCppLineGeometrySemantics` check shared rendering rules. `McBetaCppRendererSmoke` uses the selected backend, opens its own test window, and exercises pixels, texture updates, resource deletion, supported occlusion queries, minimize/restore, and resizing. Set `B173_RENDER_VALIDATION=1` to enable available native validation and detailed GL checks. Vulkan synchronization checks can additionally be enabled with `VK_VALIDATION_VALIDATE_SYNC=1`.

`McBetaCppStress --help` lists deterministic terrain, sky, weather, GUI, item, entity, fog, and TextureFX scenes. Captures use a fresh `--output` directory:

```bash
./bin/McBetaCppStress --backend gl33 parity_transparent --view-distance 3 --warmup 60 --frames 3 --capture frame.png --output comparison-run --no-finish
./bin/McBetaCppFrameCompare reference.png candidate.png difference.png
```

The comparison tool reports differing pixels, maximum channel error, mean error, and the difference bounding box. Its exit status is 0 for an exact match, 1 for differing images, and 2 for an input/output failure. Differences are not automatically accepted as rasterization variance.

All four backends have been executed on Windows with an NVIDIA GeForce RTX 5070. The GLES backend was exercised through an ES 3.2 driver using its ES 2.0 path; ES 2.0-only hardware and non-Windows runtime behavior have not been tested here. Driver-dependent numeric and rasterization differences remain relative to compatibility rendering, so bit-identical output across drivers is not claimed. Detailed measurements are under `validation/`.

## resources

assets live under `resource/` so you can be lazy