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

### optional build settings

all of these default off and keep the rendered output identical:

- `-DB173_ENABLE_IPO=ON` enables link-time optimization when the toolchain supports it.
- `-DB173_PGO=GENERATE` builds instrumented executables; run `McBetaCpp --stress <scenario> ...`, then reconfigure a second tree with `-DB173_PGO=USE -DB173_PGO_DIR=<same dir>`. a `USE` configure fails when the profile files are missing.
- `-DB173_USE_PCH=ON` and `-DB173_USE_UNITY=ON` speed up developer builds (cmake 3.16+).
- `-DB173_REGION_RENDERER=ON` stores terrain in per-region GPU buffers instead of one buffer per chunk.
- `-DB173_CACHE_CLOUDS=ON` uploads fancy-cloud geometry once per frame and draws it in both passes.
- `-DB173_FAST_LIGHT_ACCESS=ON` uses a scoped chunk accessor inside light updates.

`McBetaCppStress` accepts `--region-renderer 0|1`, `--cache-clouds 0|1`, `--state-hash`, `--chunk-log`, and `--frame-hash` for parity comparisons. `bin/resource` is refreshed from `resource/` only when files change; other files placed in `bin/resource` are left alone.

## resources

assets live under `resource/` so you can be lazy