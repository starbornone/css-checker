# Contributing

Contributions follow the short loop in the [README](../README.md#contributing), with documentation and roadmap expectations spelled out here for humans and agents.

## Propose a change

1. Fork the repository (or use a branch on a clone).
2. Create a feature branch.
3. Make focused changes — prefer small diffs that one reviewer can hold in mind.
4. Add or update tests when behaviour changes (`tests/test_checker.cpp`).
5. Open a pull request. CI must build and pass `ctest` on Linux and Windows.

## Documentation expectations

- Keep [README.md](../README.md) accurate for flags, exit codes, safety behaviour, and supported patterns.
- Update the matching page under `docs/` when you change architecture, setup, or contributor process (`overview`, `architecture`, `getting-started`, `development`, etc.).
- Do not document stacks the project does not use (no auth, database, or frontend app layer).
- If behaviour is intentional but subtle (exact-selector cleanup, dynamic prefixes, empty code-scan refusal), prefer a sentence in README **and** a pointer from `docs/`.

## Roadmap expectations

Roadmap items **move with the work they describe**:

- Start work that maps to a todo → keep the same `id` in `roadmap/roadmap.json` (edit title/notes if needed).
- Finish the work → move the item to the end of `roadmap/completed.json` with `completedAt`.
- Drop the work → move to `roadmap/cancelled.json` with `cancelledAt` and a short `reason`.
- Never rename or reuse an `id`. Never delete an item outright.

Full contract: [roadmap-format.md](roadmap-format.md). Agent summary: [AGENTS.md](../AGENTS.md).

## Tests and quality bar

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Cleanup and detection changes without tests are incomplete. Preserve conservative safety: do not broaden deletion to compound/nested selectors without an explicit product decision and docs update.

## License

By contributing you agree the work is provided under the MIT License ([LICENSE](../LICENSE)).
