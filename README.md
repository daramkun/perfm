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
| --cpu | measure CPU usage percentage (average, per physical/logical core) |
| --mem | measure memory usage (total, physical/virtual) |
| --gpu | measure GPU usage percentage |
| --vmem | measure GPU VRAM usage |
| --file | measure Filesystem I/O usage |
| --network | measure Network I/O usage |
| --time | measure (end - start) time |
| --freq=<time> | frequency for measuring (5s, 1m, ...) |

### support operating systems
- Windows
- Linux
- macOS

### build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build --output-on-failure
```

### examples
```bash
perfm --time -- <application filename> [application arguments]
perfm --as-stdout --cpu --mem --time -- <application filename>
perfm --as-csv=sample.csv --cpu --mem --time -- <application filename>
perfm --as-md=sample.md --file --network --gpu --vmem -- <application filename>
```

When `--as-csv` is used without a path, output is written to `perfm.csv`. When
`--as-md` is used without a path, output is written to `perfm.md`.

### current metric support notes
GPU usage and GPU VRAM metrics are represented in the CLI and output schema, but
portable providers are not implemented yet. They render as `unsupported` instead
of failing the run.

Per-process network I/O is not reliably available across supported operating
systems without heavier tracing or elevated permissions. Network metrics are
therefore emitted as `unsupported` in the initial implementation.
