# AGENTS

## Project

- **Title:** CSS Checker C++
- **Key:** `css-checker`
- **Purpose:** Dependency-free C++17 CLI that finds unused CSS classes in web projects (JavaScript/TypeScript, Razor `.cshtml`, Web Forms `.aspx`) and can remove only exact-selector rules, with backups, dry-run, and markdown reports.

## Layout

| Path | Why it matters |
|------|----------------|
| `README.md` | Human product entry — flags, safety, exit codes |
| `AGENTS.md` | This file — agent agreements |
| `docs/` | Overview, architecture, setup, development, contributing, decisions, roadmap contract |
| `roadmap/` | `roadmap.json` / `completed.json` / `cancelled.json` |
| `src/main.cpp` | Pipeline orchestration |
| `src/cli_parser.cpp` | CLI flags |
| `src/file_scanner.cpp` | File discovery |
| `src/usage_detector.cpp` | Usage index and class disposition |
| `src/css_parser.cpp` | Class extraction, exact selectors, cleanup |
| `src/report_generator.cpp` | Console + markdown output |
| `include/` | Module headers + `common.h` |
| `tests/test_checker.cpp` | Behavioural tests |
| `CMakeLists.txt` | App, tests, ctest |
| `.github/workflows/build.yml` | Linux + Windows CI |

See [docs/architecture.md](docs/architecture.md) for control flow.

## Working agreements

Build and test with CMake (do not invent npm/pip stacks):

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Binary: `build/bin/css_checker` or `build/bin/Release/css_checker.exe` on MSVC Windows.

Prefer small, reviewable diffs. Treat cleanup/backup behaviour as safety-critical — extend `tests/test_checker.cpp` when changing detection or file mutation. Match existing C++17 style; CMake already enables strong warnings.

## Documentation

- Update [README.md](README.md) when user-facing flags, exit codes, or safety semantics change.
- Update the matching page under `docs/` (`overview`, `architecture`, `getting-started`, `development`, `contributing`, `decisions`).
- Keep README and docs consistent; link with relative paths.
- Do not document auth, databases, or other stacks this repo does not use.

## Roadmap

This project's key is `css-checker` and its title is `CSS Checker C++`. Keep the
roadmap in three files in `roadmap/` at the repository root:

- `roadmap/roadmap.json`: project status and items still to do, highest priority first
- `roadmap/completed.json`: finished items
- `roadmap/cancelled.json`: items that will not be done

All three use this shape:

```json
{
  "schema": "name-roadmap/v1",
  "file": "roadmap",
  "updatedAt": "2026-10-14T09:30:00+11:00",
  "project": { "key": "css-checker", "title": "CSS Checker C++", "status": "active", "summary": "Optional, roadmap.json only." },
  "items": [
    { "id": "stable-kebab-id", "title": "Short imperative title", "group": "Optional area", "notes": "Optional detail" }
  ]
}
```

Rules:

1. `schema` is always `"name-roadmap/v1"`. `file` is `"roadmap"`,
   `"completed"` or `"cancelled"` to match the filename.
2. `project.key`, `project.title` and `project.status` are identical in all
   three files. Never change `project.key`. `status` is `active`, `paused` or
   `complete`. `summary` appears only in `roadmap.json`.
3. Every item has an `id`: lowercase kebab-case, unique across all three files,
   never renamed and never reused, even after the item is completed or cancelled.
   Titles and notes may be edited freely; the id stays the same.
4. Allowed item fields are `id`, `title`, `group` and `notes`, plus
   `completedAt` (`completed.json` only, required) and `cancelledAt` with an
   optional `reason` (`cancelled.json` only). Add no other fields.
5. When work is finished, move the item from `roadmap.json` to the end of
   `completed.json` and add `completedAt`. When it is dropped, move it to the
   end of `cancelled.json` and add `cancelledAt` and a short `reason`. Never
   delete an item outright. If a cancelled item is revived, move it back to
   `roadmap.json` with the same id and remove `cancelledAt`/`reason`.
6. Each item is in exactly one file. Split work into new items with new ids
   rather than changing what an existing id means.
7. Set `updatedAt` in every file you change. Timestamps are ISO 8601 with a
   UTC offset, e.g. `2026-10-14T09:30:00+11:00`.
8. Keep `roadmap.json` items in priority order. Titles are at most 500
   characters; notes and reasons at most 20,000; at most 2,000 items per file.
9. Output valid JSON only: UTF-8, two-space indent, no comments, no trailing
   commas. Create a missing file with an empty `items` array.
10. Update the roadmap in the same change as the work it describes: mark an
    item completed only when that work is actually done.

The format contract for other repositories this repo will import is in
[docs/roadmap-format.md](docs/roadmap-format.md).

## Do / don’t

**Do**

- Prefer small diffs and behavioural tests over large speculative refactors.
- Update `roadmap/` in the same change as the work (move items; never rename ids).
- Preserve conservative cleanup: exact selectors only; keep first `.backup`; refuse cleanup when no code files exist.
- Cross-link docs with relative paths.

**Don’t**

- Don’t rename or reuse roadmap item ids.
- Don’t invent APIs, auth, databases, package runtimes, or frameworks the README does not support.
- Don’t overwrite existing `.backup` files or broaden deletion to compound/nested selectors without an explicit decision + docs update.
- Don’t claim performance wins without a benchmark suite (see roadmap item `add-benchmark-suite`).
