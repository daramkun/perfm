## perfm
`perfm`(pronounce as `perfume`) is CLI application for performance measuring.

### usage
```bash
$ perfm [options] -- <application filename> [application arguments]
```

#### options
| option name | about |
|:-----------:|:-----:|
| --as-stdout | output as STDOUT (default) |
| --as-csv[=<path>] | output as csv file |
| --as-md[=<path>] | output as markdown table |
| --as-json[=<path>] | output as json file |
| --cpu | measure CPU usage percentage for the target process tree |
| --mem | measure memory usage (total, physical/virtual) |
| --gpu | measure GPU usage percentage |
| --vmem | measure GPU VRAM usage |
| --file | measure Filesystem I/O usage |
| --network | measure Network I/O usage |
| --time | measure total wall-clock runtime from process start to process exit, similar to `time <command>` |
| --freq=<time> | frequency for measuring (5s, 1m, ...) |
| --split-subproc | measure subprocesses as separate per-PID rows instead of aggregating them |
| --summary | output per-metric summary rows instead of raw samples |
| --stdout-graph | render Unicode block graphs for stdout output instead of the default table |
| --force-etw | on Windows, relaunch as administrator when ETW kernel tracing permissions are needed |

### support operating systems
- Windows
- Linux
- macOS

### build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### examples
```bash
perfm --time -- <application filename> [application arguments]
perfm --as-stdout --cpu --mem --time -- <application filename>
perfm --as-csv=sample.csv --cpu --mem --time -- <application filename>
perfm --as-md=sample.md --file --network --gpu --vmem -- <application filename>
perfm --as-json=sample.json --summary --cpu --mem --time -- <application filename>
perfm --as-stdout --stdout-graph --cpu --mem --network --time -- <application filename>
perfm --force-etw --as-stdout --stdout-graph --cpu --network -- <application filename>
```

When `--as-csv` is used without a path, output is written to `perfm.csv`. When
`--as-md` is used without a path, output is written to `perfm.md`. When
`--as-json` is used without a path, output is written to `perfm.json`.

`--time` records the target application's total elapsed wall-clock runtime. When
it is combined with sampled metrics such as `--cpu` or `--mem`, the
`elapsed_time_ms` metric is emitted on the final sample row.

`--stdout-graph` is only available with `--as-stdout`, and cannot be combined
with `--summary`. It renders Unicode block sparklines, so use a terminal with
Unicode block support such as Windows Terminal, common Linux terminals, or
Terminal.app on macOS.

On Windows, `--force-etw` relaunches `perfm` through UAC with the same command
line when the current process is not already elevated. This is useful for ETW
kernel tracing metrics such as Windows per-core CPU usage and per-process
network attribution. If the UAC prompt is cancelled, the run fails before the
target application is launched.

By default, process metrics are aggregated for the target process and its
currently running subprocesses. Use `--split-subproc` to emit separate per-PID
rows for subprocesses observed at each sample point.

### current metric support notes
GPU usage and GPU VRAM metrics are implemented on Windows using Performance
Counters. They are sampled for the target process and its currently running
subprocesses, or as separate per-PID rows when `--split-subproc` is used.

On macOS, GPU metrics are system-wide values read from IOKit accelerator
statistics. They are not per-process values, and `--split-subproc` does not make
GPU metrics process-specific on macOS. On Apple Silicon, `gpu_vram_bytes` reports
GPU-related unified/system memory usage when dedicated VRAM counters are not
available. Linux GPU metrics are not implemented because there is no single
common API across NVIDIA, AMD, and Intel drivers. On Linux, GPU metrics render as
`unsupported` instead of failing the run.

Network metrics are ETW-based per-process byte deltas on Windows when kernel
TCP/IP tracing is available. Without that permission, non-split Windows network
metrics fall back to system-wide interface byte deltas, while `--split-subproc`
network rows render as `error` because per-PID attribution is unavailable. Linux
and macOS network metrics are system-wide interface byte deltas. They are not
per-process values. When `--split-subproc` is used on Linux or macOS, network
metrics render as `unsupported` for per-PID rows instead of duplicating
system-wide values.

CPU metrics include `cpu_percent` and `cpu_total_percent` for the target process
tree. On Linux and Windows, `--cpu` also emits best-effort per-core process
metrics named `cpu_1_usage`, `cpu_2_usage`, and so on. Linux attributes thread
CPU time deltas to the CPU reported by `/proc/<pid>/task/<tid>/stat` at sample
time. Windows attributes CPU time from ETW context switch events, which requires
permission to start or update kernel tracing; without that permission, per-core
metrics render as `error` while total CPU metrics remain available. macOS keeps
CPU metrics at the process-tree total level because public APIs do not provide a
simple stable per-core process attribution path.
