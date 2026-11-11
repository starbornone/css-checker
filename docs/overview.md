# Overview

**CSS Checker C++** (`css-checker`) is a dependency-free command-line tool that finds unused CSS classes in a web project and can remove only the safe exact-selector rules. It targets mixed stacks: JavaScript/TypeScript front ends plus ASP.NET Razor (`.cshtml`) and Web Forms (`.aspx`).

## Who it is for

- Teams that ship CSS alongside React/JS/TS and need a fast local check before cleanup.
- .NET web projects where class usage appears in Razor or Web Forms markup (`class`, `CssClass`), not only in JS.
- Anyone who wants a **conservative** cleanup: compound selectors, nested rules, and dynamic class patterns stay in place.

## Core capabilities

| Capability | What it does |
|------------|----------------|
| Single-pass indexing | Each code file is read once; class names are looked up in memory afterward |
| Multi-format scan | Default code extensions: `.tsx`, `.ts`, `.js`, `.jsx`, `.cshtml`, `.aspx`; CSS: `.css` |
| Smart detection | Attributes (`class` / `className` / `CssClass`), `clsx` / `cn` / `classnames`, `classList`, DOM/`setAttribute`, jQuery-style helpers, `@apply` / `@extend` / `composes`, and related patterns described in the [README](../README.md) |
| Dynamic safety | Computed values and template prefixes (for example `icon--${name}`) are reported and not deleted |
| Safe cleanup | First `.backup` is kept; cleaned CSS is written then replaced; `--cleanup` is skipped if no code files were found |
| Reporting | Console summary (colour when stdout is a TTY) and optional markdown report |
| Configuration | `--src`, extensions, repeatable `--ignore` regexes, `--dry-run`, `--verbose`, `--no-report` |

Exit codes: **0** no unused class has an exact removable selector; **1** such unused classes remain; **2** bad arguments, analysis failure, or cleanup failure. See [Getting started](getting-started.md).

## Non-goals

- Not a full CSS parser or linter (specificity, unused properties, a11y).
- Not a JS/TS type checker or bundler plugin.
- Does not claim a measured speedup over other tools; there is **no benchmark suite** in this repository ([README](../README.md#performance)).
- Does not overwrite an existing `.backup` on later cleanups.
- Does not delete rules whose selector is compound or nested (for example `.used .unused`).

Assumption: “drop-in replacement” for a prior JavaScript CSS-checker refers to workflow parity (scan → report → optional cleanup), not identical CLI flags or regex behaviour.

## Where to go next

- [Getting started](getting-started.md) — build, run, first useful scan
- [Architecture](architecture.md) — modules and control flow
- [Development](development.md) — local workflow and tests
- [Roadmap](../roadmap/roadmap.json) — planned work
- [AGENTS.md](../AGENTS.md) — conventions for coding agents
