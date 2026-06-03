# Onboarding — your first day on the engine

Read this top to bottom on your first day. Should take 30–45 minutes.

## 1. Build the project (10 min)

Pick the path for your OS:

- **Fedora (the supported path through M4):** `docs/setup/fedora.md`
- **Windows (unmaintained, M5+):** `docs/setup/windows.md` — read the status banner first.

If you're on Windows or macOS and need to start work today, either use Fedora in a VM / WSL2 toolbox, or pick up a doc-only spec until M5. See `docs/adr/0002-fedora-primary-platform.md` for the rationale.

Don't move past this until `cmake --workflow --preset check` passes green. Every subsequent step assumes a working build.

## 2. Read the canonical brief (5 min)

`AGENTS.md` — non-negotiable rules, file layout, build commands, what NOT to touch.

If you're an AI agent: this file is what the IDE/agent loads. Re-read it whenever in doubt. Follow the symlinks: `CLAUDE.md`, `GEMINI.md`, `.github/copilot-instructions.md` all point here.

## 3. Read the architecture overview (10 min)

`docs/architecture/00-overview.md` — system layers, dependency diagram, the **plugin-architecture / no-third-party-types-in-headers** rule that drives everything.

Then skim the per-subsystem docs as relevant to what you're about to touch:

- `01-core.md` — App, Plugin, Schedule (Bevy mapping)
- `02-ecs.md` — World, archetypes, queries
- `03-platform.md` — Window, Input (SDL3 hidden)
- `04-rendering.md` — Renderer (SDL3 GPU hidden, ADR 0003), 2D + 3D unification
- `05-assets.md` — Handle<T>, loaders, hot reload
- `06-scheduler.md` — concurrency model, stdexec
- `07-testing.md` — fast/slow split, screenshot diff, deterministic replay
- `08-error-handling.md` — `std::expected`, no exceptions across module boundaries
- `09-conventions.md` — naming, formatting, header hygiene

## 4. Read the roadmap (5 min)

`docs/roadmap.md` — milestones M0–M6, what's done, what's next, rough sizing.

## 5. Pick a spec (5 min)

`specs/` holds PR-sized tasks. Each follows the same template:

- **Scope** — what to build
- **Acceptance criteria** — checklist that must be green
- **Out of scope** — what you must NOT do
- **Files not to touch** — files reserved for other specs
- **Notes for the implementing agent** — hints, reading list

The first available spec by number is your job unless told otherwise. Phase-0 bring-up has merged, so the M1 starting point is `specs/0001-ecs-archetype-storage.md`.

## 6. Workflow per spec (the loop)

```
1. read the spec end to end
2. read any docs/architecture/ chapter it references
3. read existing related code (grep first — there is more reuse than you think)
4. write the public header(s) and tests FIRST, then the implementation
5. run `cmake --workflow --preset check` after each meaningful change
6. when all acceptance-criteria checkboxes pass, open a PR using the agent-task template
```

The pre-commit hook will reformat and tidy on commit. Don't disable it.

## 7. When you get stuck

In order:

1. Re-read the spec. The "Out of scope" line is usually what trips agents up.
2. Read `build/logs/last_run.jsonl` — every example writes structured logs there.
3. Check `docs/adr/` for any ADR covering the area.
4. Check `git log -- <path>` for recent commits in the area; commit messages explain the *why*.
5. If still stuck, **stop and report**. Do not refactor adjacent code looking for a fix.

## 8. What "done" means for a PR

- All acceptance criteria checked
- `cmake --workflow --preset check` green locally
- All CI jobs green (the matrix is small on purpose; if it's red, fix it before review)
- No public header in `engine/` references a third-party type
- Conventional commit message (`feat:`, `fix:`, etc.)
- Spec file updated if the implementation deviated from the original plan, with a short note explaining why

## 9. What's allowed without a spec

- Typo fixes in docs and comments
- Updating a pinned version in `tools/bootstrap-windows-portable.ps1`
- Adding tests to existing code (still keep them small and tagged)

Anything else needs a spec first. Don't refactor "while you're here". Don't add a feature flag for hypothetical future use. Don't add a helper because three call sites look similar.
