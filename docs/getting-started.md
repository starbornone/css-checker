# Getting started

Build the CLI, point it at a source tree, review unused classes, then optionally dry-run cleanup.

## Prerequisites

- **C++17** compiler: GCC 7+, Clang 5+, or MSVC 2019+
- **CMake** 3.15+

If CMake or a compiler is missing, configuration fails immediately (`cmake ..` / `cmake -S . -B build`). On Windows, install Visual Studio with C++ workload or another generator CMake can find; `build.bat` tries Visual Studio then MinGW Makefiles.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Unix Release configure:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

**Binary location**

| Platform | Typical path |
|----------|----------------|
| Linux / macOS | `build/bin/css_checker` |
| Windows (MSVC) | `build/bin/Release/css_checker.exe` |

Helpers: `build.sh` (Unix), `build.bat` (Windows), or the root `Makefile` (g++ alternative; does not run the CMake test target).

## Verify the build

```bash
ctest --test-dir build -C Release --output-on-failure
```

On single-config generators omit `-C Release`. Success: tests pass (including `checker`, `cli_help`, `cli_version`).

Smoke the binary:

```bash
./build/bin/css_checker --help
./build/bin/css_checker --version
```

## First useful action

Scan a project tree that contains both CSS and code (defaults look under `src`):

```bash
# Preview analysis of ./src
./build/bin/css_checker --verbose

# Or point at your app root / wwwroot / ClientApp
./build/bin/css_checker --src path/to/your/project --verbose
```

Then:

1. Read the console summary (unused, kept compound selectors, dynamic classes).
2. Open `unused-css-report.md` unless you passed `--no-report`.
3. Preview cleanup without writing: `--cleanup --dry-run`.
4. Apply cleanup only after review: `--cleanup` (restores from the first `.backup` if needed).

Mixed React + ASP.NET example:

```bash
./build/bin/css_checker --code-ext .tsx,.ts,.js,.jsx,.cshtml,.cs --src ClientApp
```

Vue/Svelte/HTML: add extensions with `--code-ext`; a `class` / `className` attribute is enough for those files.

## Failure symptoms

| Symptom | Likely cause |
|---------|----------------|
| Exit **2**, “Source directory does not exist” | Wrong path or default `src` missing — pass `--src` or a positional directory |
| Exit **2**, “No CSS files found” | No files matching `--css-ext` under the tree (default `.css`) |
| “No code files found… Cleanup skipped.” | Extensions/path wrong; cleanup refused so every class is not treated as deletable |
| Exit **1** | Analysis OK, but unused classes still have exact selectors — expected until cleanup or code fixes |
| Exit **2** after `--cleanup` | Backup/write/replace failed; stylesheets that failed were not replaced |
| Colour codes in redirected logs | stdout is not a TTY; colour is omitted on real terminals only |
| CMake / compile errors | Toolchain older than C++17 or CMake &lt; 3.15 |

Ignored by the scanner (not an error): `node_modules`, `bin`, `obj`, `.git`.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | No unused class has a selector that is exactly that class |
| 1 | An unused class still has an exact selector |
| 2 | Bad arguments, files could not be analyzed, or cleanup failed |

More detail: [overview](overview.md), [architecture](architecture.md), full flag list in the [README](../README.md#complete-options).
