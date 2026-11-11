# Decisions

Lightweight records inferred from the [README](../README.md) and the shipped layout. Not a full ADR process — just the choices agents should not casually reverse.

## ADR-001: C++17 CLI, zero runtime dependencies

**Context:** A JavaScript CSS-checker already existed but needed runtime dependencies and lacked Razor/Web Forms support.

**Decision:** Ship a standalone C++17 binary built with CMake, with no runtime package dependencies.

**Consequences:** Fast local runs and easy CI; contributors need a C++ toolchain. Feature work stays in-process (no plugin host). Cross-platform via standard C++ and CI on Linux/Windows.

## ADR-002: Conservative exact-selector cleanup only

**Context:** Deleting any rule that *mentions* an unused class would break compound selectors such as `.used .unused` or `.used.unused`.

**Decision:** `--cleanup` removes a rule (or selector list entry) only when a selector is **exactly** that class, with optional pseudos. Nested/unclosed rules stay. Dynamic classes are never deleted.

**Consequences:** Some unused names remain in stylesheets by design (reported as kept). Users must accept incomplete size wins in exchange for safer edits. Tests should lock this contract.

## ADR-003: First `.backup` is permanent for that path

**Context:** Repeated cleanups need a restore point without silently losing the original first snapshot.

**Decision:** Create `file.css.backup` only if missing; never overwrite an existing backup. Write cleaned content via a temporary file, then replace.

**Consequences:** Later cleanups can drift from the first backup; restore instructions in the README point at that first file. Agents must not “fix” backups by overwriting.

## ADR-004: Refuse cleanup when no code files are found

**Context:** A wrong `--src` or extension list makes every CSS class look unused.

**Decision:** If the code scan is empty, warn and skip `--cleanup` so a bad path cannot delete every class.

**Consequences:** Exit can still surface analysis issues; cleanup requires at least one matching code file. Operators must fix `--code-ext` / `--src` rather than force cleanup.

## ADR-005: Single-pass in-memory usage index

**Context:** Naive “search each class against every file” scales poorly and invites pattern thrash.

**Decision:** Read each code file once, build a class-name index (plus dynamic prefixes / opaque bindings), then classify CSS classes with in-memory lookups.

**Consequences:** Predictable I/O; README does not claim benchmarks (none in repo). Detection logic concentrates in `usage_detector` — changes need thorough behavioural tests.

## ADR-006: Extend formats via CLI extensions, not hard-coded Vue/Svelte defaults

**Context:** Vue, Svelte, HTML, and extra C# files are useful but not every repo wants them scanned by default.

**Decision:** Default code extensions are `.tsx,.ts,.js,.jsx,.cshtml,.aspx`. Users add others with `--code-ext`. Detection treats configured code files uniformly when attributes/helpers match.

**Consequences:** Docs and help must show how to widen the scan. Default behaviour stays focused on JS/TS + ASP.NET markup.
