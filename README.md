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

`McBetaCppStress` accepts `--region-renderer 0|1`, `--cache-clouds 0|1`, `--state-hash`, `--chunk-log`, and `--frame-hash` for parity comparisons. `bin/resource` is refreshed from `resource/` only when files change; other files placed in `bin/resource` are left alone.

### verification

`McBetaCpp --findings-smoke` runs the block/item audit regressions and glass geometry/GPU checks. It requires a working OpenGL context. The existing `--block-smoke`, `--network-smoke`, and `--terrain-storage-smoke` commands cover the broader block, protocol, and terrain-storage behavior.

The `glass` stress scenario renders isolated, paired, and grouped glass, then adds and removes a neighbor. `--origin` shifts the fixture across chunk boundaries; `--ao` controls ambient occlusion independently of `--fancy`. Supply a fresh output directory:

```bash
McBetaCppStress glass --view-distance 3 --warmup 60 --frames 6 --origin 15 --fancy 1 --ao 0 --region-renderer 1 --output glass-check --capture glass.png
```

## resources

assets live under `resource/` so you can be lazy