# Roadmap format contract

This repository keeps product status and work items in three JSON files under `roadmap/`. Other repos (and humans) that import or mirror this convention should follow the same schema.

Canonical files:

| File | Role |
|------|------|
| [`roadmap/roadmap.json`](../roadmap/roadmap.json) | Project status + **todo** items, highest priority first |
| [`roadmap/completed.json`](../roadmap/completed.json) | Finished items |
| [`roadmap/cancelled.json`](../roadmap/cancelled.json) | Explicitly abandoned items |

Agent-oriented rules also live in [AGENTS.md](../AGENTS.md).

## Schema

Every file is UTF-8 JSON, **two-space indent**, no comments, no trailing commas.

```json
{
  "schema": "name-roadmap/v1",
  "file": "roadmap",
  "updatedAt": "2026-11-11T23:06:34+11:00",
  "project": {
    "key": "css-checker",
    "title": "CSS Checker C++",
    "status": "active",
    "summary": "Optional — roadmap.json only."
  },
  "items": []
}
```

### Top-level fields

| Field | Rule |
|-------|------|
| `schema` | Always `"name-roadmap/v1"` |
| `file` | `"roadmap"`, `"completed"`, or `"cancelled"` — must match the filename |
| `updatedAt` | ISO 8601 with UTC offset; bump on every edit to that file |
| `project` | Same `key`, `title`, and `status` in **all three** files |
| `items` | Array of work items (may be empty) |

### `project` fields

| Field | Rule |
|-------|------|
| `key` | Stable forever (kebab-case). **Never** change after creation. This repo: `css-checker` |
| `title` | Display name. This repo: `CSS Checker C++` |
| `status` | `active`, `paused`, or `complete` |
| `summary` | Optional one-line product summary; **only** in `roadmap.json` |

### Item fields

Allowed fields only:

| Field | Where | Rule |
|-------|--------|------|
| `id` | all | Lowercase kebab-case; unique across all three files; never rename or reuse |
| `title` | all | Short imperative title (≤ 500 characters) |
| `group` | all | Optional area label |
| `notes` | all | Optional detail (≤ 20,000 characters) |
| `completedAt` | `completed.json` only | Required ISO 8601 timestamp when the item was finished |
| `cancelledAt` | `cancelled.json` only | Required when cancelled |
| `reason` | `cancelled.json` only | Optional short explanation (≤ 20,000 characters) |

Limits: at most 2,000 items per file.

## Move semantics

- An item exists in **exactly one** of the three files.
- **Complete work:** move from `roadmap.json` to the **end** of `completed.json`; add `completedAt`. Remove from todos.
- **Cancel work:** move to the **end** of `cancelled.json`; add `cancelledAt` and preferably `reason`.
- **Revive:** move from `cancelled.json` back to `roadmap.json` with the **same** `id`; drop `cancelledAt` / `reason`.
- **Never** delete an item outright. Split scope by adding a **new** id rather than redefining an old one.
- Titles and notes may change; `id` must not.
- Keep `roadmap.json` items in **priority order** (highest first).
- Update `updatedAt` on every file you touch. Mark completed only when the work is actually done (same change as the implementation).

## Minimal valid examples

### `roadmap/roadmap.json`

```json
{
  "schema": "name-roadmap/v1",
  "file": "roadmap",
  "updatedAt": "2026-11-11T23:06:34+11:00",
  "project": {
    "key": "css-checker",
    "title": "CSS Checker C++",
    "status": "active",
    "summary": "Fast C++ CLI for finding and safely removing unused CSS classes."
  },
  "items": [
    {
      "id": "example-todo",
      "title": "Add a focused regression for cleanup backups",
      "group": "testing",
      "notes": "Illustrative only."
    }
  ]
}
```

### `roadmap/completed.json`

```json
{
  "schema": "name-roadmap/v1",
  "file": "completed",
  "updatedAt": "2026-11-11T23:06:34+11:00",
  "project": {
    "key": "css-checker",
    "title": "CSS Checker C++",
    "status": "active"
  },
  "items": [
    {
      "id": "example-done",
      "title": "Ship markdown unused-class reports",
      "group": "reporting",
      "notes": "Illustrative only.",
      "completedAt": "2026-11-11T23:06:34+11:00"
    }
  ]
}
```

### `roadmap/cancelled.json`

```json
{
  "schema": "name-roadmap/v1",
  "file": "cancelled",
  "updatedAt": "2026-11-11T23:06:34+11:00",
  "project": {
    "key": "css-checker",
    "title": "CSS Checker C++",
    "status": "active"
  },
  "items": []
}
```

Empty `items` is valid and preferred when nothing is cancelled.
