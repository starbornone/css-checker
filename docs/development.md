# Development

Local workflow for changing detection, cleanup, CLI, or reports. Stack is C++17 + CMake only — no npm, no language server package, no linter config in-repo.

## Repo layout

```
css-checker/
  AGENTS.md           Agent working agreements
  README.md           Human product entry
  CMakeLists.txt      App + tests + ctest
  Makefile            Optional g++ build (no ctest wiring)
  build.sh / build.bat
  include/            Public headers
  src/                Implementation (see architecture.md)
  tests/              test_checker.cpp
  docs/               This documentation set
  roadmap/            Todo / completed / cancelled JSON
  .github/workflows/  Linux + Windows build and test
```

## Configure, build, test

Preferred (matches README and CI):

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CI (`.github/workflows/build.yml`) runs the same on `ubuntu-latest` and `windows-latest` for every push and pull request.

There is **no** project-enforced clang-format, clang-tidy, or sanitizer job. Prefer the warning levels already set in CMake (`/W4` on MSVC, `-Wall -Wextra -pedantic` elsewhere).

### Running the tool while developing

```bash
# After build — path may be build/bin or build/bin/Release
./build/bin/css_checker --src path/to/fixture --verbose --dry-run
```

Use a small fixture tree (CSS + one code file) before touching real apps. Prefer `--cleanup --dry-run` before any destructive `--cleanup`.

## Tests

- Single behavioural binary: `css_checker_tests` from `tests/test_checker.cpp`.
- Covers extraction, ignore flags, usage indexing across TSX/CSHTML/ASPX/Vue samples, cleanup rules, backups, dry-run, dynamic/computed bindings, clsx objects, DOM/jQuery helpers, and related cases.
- Also registered: `cli_help` and `cli_version` against the real `css_checker` binary.

When changing detection or cleanup, add a failing case first, then fix. Keep tests behavioural (inputs → disposition / file contents), not tied to private helper names.

**Assumption:** The Makefile path is for quick compile-only builds; agents should use CMake + ctest unless the user asks otherwise.

## Safe navigation notes

- Prefer editing one concern per change (`usage_detector` vs `css_parser` vs CLI).
- Do not invent runtime dependencies; the product is dependency-free by design ([decisions](decisions.md)).
- Cleanup and backup behaviour is safety-critical — extend tests for any change to `cleanup_file` or exact-selector rules.
- Update [README](../README.md) and docs when flags or exit-code semantics change.
- Move roadmap items in the same change as the work ([roadmap format](roadmap-format.md), [AGENTS.md](../AGENTS.md)).

## Related docs

- [Architecture](architecture.md)
- [Contributing](contributing.md)
- [Getting started](getting-started.md)
