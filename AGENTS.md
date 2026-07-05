## Project Notes

`perfm` is a CMake-based C++23 CLI for measuring a target process and its
subprocesses.

### Build and test

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The Windows build uses a multi-config generator, so include `-C Debug` when
running `ctest` against the existing `build` directory.

### Implementation conventions

- Keep metric names stable across output formats.
- Platform gaps should be represented with `unsupported` metric state rather
  than failing the whole run.
- Collector failures for unavailable process data should be localized to the
  affected metric where possible.
- Avoid adding external dependencies unless the benefit is clear and the build
  remains straightforward on Windows, Linux, and macOS.
- Generated build directories are ignored via `build*/`.
