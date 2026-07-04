## 1. Goal
Implement the `perfm` command-line application described in `README.md:1-27` as a native C++ CLI using snake_case names, `.h` headers only, Allman brace placement, and 4-space indentation.

## 2. Approach
Use a CMake-based C++23 project with focused modules for CLI parsing, process execution, sampling, collectors, and output formatting. Enforce one project style from the first commit: snake_case filenames/symbols, header extension `.h`, Allman braces, and spaces-only 4-space indentation. Platform-specific APIs stay isolated behind shared collector interfaces because `README.md:24-27` requires Windows, Linux, and macOS support.

## 3. File Changes

### Modify
- `README.md` - Keep the existing user-facing contract from `README.md:4-22`, then add build/test instructions after `README.md:27`. Document unsupported GPU/VRAM and per-process network behavior.

### Create
- `CMakeLists.txt` - Top-level CMake project for `perfm`, C++23, executable/test targets, warning configuration, and platform-specific source selection.
- `cmake/compiler_warnings.cmake` - Shared warning flags for MSVC, Clang, and GCC.
- `.clang-format` - Enforces Allman braces, 4-space indentation, spaces instead of tabs, and C++ formatting defaults compatible with the requested style.
- `src/main.cpp` - Entrypoint that calls `parse_options`, launches the target process, runs `sample_process`, and writes formatted output.
- `src/cli/options.h` - Defines `output_mode`, `metric_kind`, and `options` for the documented flags in `README.md:12-22`.
- `src/cli/parser.h` - Declares `parse_result`, `parse_options`, and parse error helpers.
- `src/cli/parser.cpp` - Implements parsing for `--as-stdout`, `--as-csv[=<path>]`, `--as-md[=<path>]`, metric flags, `--freq=<time>`, and the `--` separator from `README.md:6`.
- `src/cli/duration_parser.h` - Declares `parse_duration` and default frequency helpers.
- `src/cli/duration_parser.cpp` - Parses frequency strings like `5s` and `1m` from `README.md:22` into `std::chrono::milliseconds`.
- `src/core/metric.h` - Defines `metric_kind`, `metric_value`, `metric_unit`, and unsupported/error states.
- `src/core/sample.h` - Defines `sample` as one timestamped set of metric values.
- `src/core/collector.h` - Defines the abstract `collector` interface with `start`, `sample`, and `stop` lifecycle functions.
- `src/core/sampler.h` - Declares `sample_process` and sampler configuration types.
- `src/core/sampler.cpp` - Implements periodic sampling until the child process exits.
- `src/process/child_process.h` - Cross-platform process API with `pid`, `is_running`, `exit_code`, `wait`, and termination behavior.
- `src/process/child_process.cpp` - Dispatches to platform-specific process implementations.
- `src/process/child_process_win.cpp` - Implements Windows process launching with `CreateProcessW`.
- `src/process/child_process_posix.cpp` - Implements Linux/macOS process launching with `fork`, `execvp`, and `waitpid`.
- `src/collectors/cpu_collector.h` - Declares CPU collector factory and CPU metric names for `README.md:15`.
- `src/collectors/cpu_collector_win.cpp` - Windows CPU collector using process times and processor counts.
- `src/collectors/cpu_collector_linux.cpp` - Linux CPU collector using `/proc/<pid>/stat`, `/proc/stat`, and clock ticks.
- `src/collectors/cpu_collector_macos.cpp` - macOS CPU collector using `proc_pidinfo` and Mach APIs.
- `src/collectors/memory_collector.h` - Declares memory collector factory and memory metric names for `README.md:16`.
- `src/collectors/memory_collector_win.cpp` - Windows memory collector using `GetProcessMemoryInfo` and global memory status.
- `src/collectors/memory_collector_linux.cpp` - Linux memory collector using `/proc/<pid>/status`, `/proc/<pid>/statm`, and `/proc/meminfo`.
- `src/collectors/memory_collector_macos.cpp` - macOS memory collector using `proc_pidinfo`, Mach task info, and host statistics.
- `src/collectors/io_collector.h` - Declares file/network I/O collector factories for `README.md:19-20`.
- `src/collectors/file_io_collector_win.cpp` - Windows file I/O collector using `GetProcessIoCounters`.
- `src/collectors/file_io_collector_linux.cpp` - Linux file I/O collector using `/proc/<pid>/io`.
- `src/collectors/file_io_collector_macos.cpp` - macOS file I/O collector using `proc_pid_rusage` where available.
- `src/collectors/network_collector_win.cpp` - Windows network collector returning explicit unsupported values initially.
- `src/collectors/network_collector_linux.cpp` - Linux network collector returning explicit unsupported values initially.
- `src/collectors/network_collector_macos.cpp` - macOS network collector returning explicit unsupported values initially.
- `src/collectors/gpu_collector.h` - Declares GPU/VRAM collector factory for `README.md:17-18`.
- `src/collectors/gpu_collector.cpp` - Implements provider discovery and unsupported fallback.
- `src/output/formatter.h` - Defines formatter interface and `format_samples` helpers for stdout, CSV, and Markdown output.
- `src/output/stdout_formatter.cpp` - Implements default stdout table output for `README.md:12`.
- `src/output/csv_formatter.cpp` - Implements CSV output and default `perfm.csv` path for `README.md:13`.
- `src/output/markdown_formatter.cpp` - Implements Markdown table output and default `perfm.md` path for `README.md:14`.
- `src/output/output_writer.h` - Handles stdout and file writes consistently.
- `tests/CMakeLists.txt` - Test target definitions integrated with CTest.
- `tests/duration_parser_tests.cpp` - Tests `parse_duration` behavior.
- `tests/parser_tests.cpp` - Tests documented CLI options and target argument parsing.
- `tests/formatter_tests.cpp` - Tests stdout, CSV, Markdown, unsupported metric rendering, and default paths.
- `tests/sampler_tests.cpp` - Tests sampler behavior using fake collectors and helper processes.
- `tests/helpers/sleeper.cpp` - Cross-platform helper executable for sampler/process tests.
- `.gitignore` - Ignores CMake build directories, binaries, generated outputs, and test artifacts.

## 4. Implementation Steps

### Task 1: Establish project scaffold and style rules
1. Create `CMakeLists.txt` with target names `perfm`, `perfm_tests`, C++23, snake_case source paths, and `.h` header paths only.
2. Create `cmake/compiler_warnings.cmake` and include it from `CMakeLists.txt`.
3. Create `.clang-format` with `BreakBeforeBraces: Allman`, `IndentWidth: 4`, `TabWidth: 4`, and `UseTab: Never`.
4. Create `tests/CMakeLists.txt` and `tests/helpers/sleeper.cpp`, wiring CTest without CamelCase test filenames.
5. Create `.gitignore` with build and generated output patterns.

### Task 2: Implement snake_case CLI parsing with `.h` headers
1. Create `src/cli/options.h` with symbols `options`, `output_mode`, `metric_kind`, `target_path`, `target_args`, `output_path`, and `sample_frequency`.
2. Create `src/cli/duration_parser.h` and `src/cli/duration_parser.cpp` with `parse_duration`, `default_sample_frequency`, and error values using snake_case names.
3. Create `src/cli/parser.h` and `src/cli/parser.cpp` with `parse_result`, `parse_options`, and `format_usage`.
4. Format all C++ files with Allman braces and 4-space indentation.
5. Add `tests/duration_parser_tests.cpp` and `tests/parser_tests.cpp` with snake_case test case names covering `README.md:12-22`.

### Task 3: Implement process execution and sampling core
1. Create `src/process/child_process.h`, `src/process/child_process.cpp`, `src/process/child_process_win.cpp`, and `src/process/child_process_posix.cpp` with type/function names such as `child_process`, `launch_child_process`, `is_running`, and `exit_code`.
2. Create `src/core/metric.h` and `src/core/sample.h` with `metric_value`, `metric_unit`, `metric_state`, and `sample`.
3. Create `src/core/collector.h`, `src/core/sampler.h`, and `src/core/sampler.cpp` with `collector`, `collector_list`, `sample_process`, and `sampler_config`.
4. Add `tests/sampler_tests.cpp` to verify `start`, `sample`, `stop`, cadence, and elapsed time behavior.
5. Run formatting after these files are created so braces and indentation match the requested style.

### Task 4: Implement CPU and memory collectors
1. Create `src/collectors/cpu_collector.h` and platform implementations using factory function `make_cpu_collector`.
2. Implement CPU metrics with snake_case names such as `cpu_percent`, `cpu_percent_per_logical_core`, and `cpu_percent_per_physical_core`.
3. Create `src/collectors/memory_collector.h` and platform implementations using factory function `make_memory_collector`.
4. Implement memory metrics with names such as `memory_total_bytes`, `memory_resident_bytes`, and `memory_virtual_bytes`.
5. Keep all collector declarations in `.h` files and all implementation files in `.cpp` files.

### Task 5: Implement file, network, GPU, and VRAM collectors
1. Create `src/collectors/io_collector.h` and file I/O implementations using `make_file_io_collector`.
2. Implement file metrics with names such as `file_read_bytes` and `file_write_bytes`.
3. Create network collector implementations using `make_network_collector` and return unsupported values for unavailable per-process network counters.
4. Create `src/collectors/gpu_collector.h` and `src/collectors/gpu_collector.cpp` using `make_gpu_collector`, with metrics `gpu_percent` and `gpu_vram_bytes`.
5. Keep unsupported metric handling explicit in shared metric types so output stays consistent.

### Task 6: Implement output modes
1. Create `src/output/formatter.h`, `src/output/output_writer.h`, `src/output/stdout_formatter.cpp`, `src/output/csv_formatter.cpp`, and `src/output/markdown_formatter.cpp` with functions `format_stdout`, `format_csv`, `format_markdown`, and `write_output`.
2. Implement `--as-stdout` from `README.md:12` as default stdout table output.
3. Implement `--as-csv[=<path>]` from `README.md:13`, using `perfm.csv` when no path is supplied.
4. Implement `--as-md[=<path>]` from `README.md:14`, using `perfm.md` when no path is supplied.
5. Add `tests/formatter_tests.cpp` with snake_case test names for column selection, escaping, Markdown table syntax, unsupported values, and default output paths.

### Task 7: Wire executable and documentation
1. Implement `src/main.cpp` with snake_case calls: `parse_options`, `launch_child_process`, `make_selected_collectors`, `sample_process`, and `write_output`.
2. Update `README.md` after `README.md:27` with CMake build/test commands and example invocations.
3. Document unsupported GPU/VRAM and per-process network behavior in `README.md`.
4. Ensure all new C++ code is formatted with Allman braces and 4-space indentation before review.

## 5. Acceptance Criteria
1. All created source, header, and test filenames use snake_case, except required conventional names `CMakeLists.txt` and `README.md`.
2. All project headers use `.h`; no `.hpp` or `.hxx` files are created or referenced.
3. Public C++ symbols use snake_case, including `options`, `parse_options`, `parse_duration`, `child_process`, `launch_child_process`, `collector`, `sample_process`, `make_cpu_collector`, `make_memory_collector`, `make_file_io_collector`, `make_network_collector`, `make_gpu_collector`, `format_csv`, and `write_output`.
4. C++ brace placement follows Allman style: function, class, namespace, control-flow, and enum braces open on their own line.
5. C++ indentation uses spaces only, with 4 spaces per indentation level and no tab characters.
6. Running `perfm --time -- <program>` launches `<program>`, waits for it to exit, and prints elapsed time greater than or equal to the target runtime.
7. Running `perfm --as-stdout --time -- <program>` writes measurement rows to stdout without requiring an output path.
8. Running `perfm --as-csv --time -- <program>` creates `perfm.csv` with a header row and at least one data row.
9. Running `perfm --as-md --time -- <program>` creates `perfm.md` containing a valid Markdown table.
10. Running `perfm --freq=1s --time -- <program that runs at least 2200ms>` produces at least two timestamped samples.
11. Running `perfm --freq=0s -- <program>` exits nonzero and identifies `--freq` as invalid.
12. Arguments after `--` are passed unchanged to the target executable, including arguments beginning with `--`.
13. GPU/VRAM and network metrics render as `unsupported` where no provider exists instead of crashing.
14. The CTest suite passes with parser, duration, formatter, sampler, and process tests enabled.

## 6. Verification Steps
1. Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`.
2. Build: `cmake --build build --config Debug`.
3. Run tests: `ctest --test-dir build --output-on-failure`.
4. Check header extensions: list repository files and verify no new file ends in `.hpp` or `.hxx`.
5. Check filenames: list repository files and verify no new source/test file uses CamelCase.
6. Check tab usage: search new C++ and CMake files for tab characters and verify none are present.
7. Check formatting: run `clang-format --dry-run --Werror` on `src/**/*.cpp`, `src/**/*.h`, and `tests/**/*.cpp` if `clang-format` is available.
8. Manual Windows stdout check: `build\Debug\perfm.exe --time --freq=1s -- powershell -NoProfile -Command "Start-Sleep -Seconds 2"`.
9. Manual CSV check: `build\Debug\perfm.exe --as-csv=sample.csv --cpu --mem --time -- powershell -NoProfile -Command "Start-Sleep -Seconds 1"`.
10. Manual Markdown check: `build\Debug\perfm.exe --as-md=sample.md --file --network --gpu --vmem -- powershell -NoProfile -Command "Start-Sleep -Seconds 1"`.

## 7. Risks & Mitigations
1. Enforcing snake_case for C++ types and `.h` headers may conflict with some common C++ project conventions. Mitigation: apply the user-requested convention uniformly and encode it in `.clang-format` plus review checks.
2. Allman style and 4-space indentation can drift during manual edits. Mitigation: add `.clang-format` and require the dry-run formatting check in verification.
3. GPU/VRAM metrics from `README.md:17-18` are not consistently portable. Mitigation: keep `make_gpu_collector` provider-based and render unsupported values explicitly.
4. Per-process network I/O from `README.md:20` is not reliably available without heavier tracing or permissions. Mitigation: keep the collector API and output columns, but initially return unsupported values.
5. Windows and POSIX process argument handling differ. Mitigation: isolate launch logic in `child_process_win.cpp` and `child_process_posix.cpp`, and test that arguments after `--` are preserved.