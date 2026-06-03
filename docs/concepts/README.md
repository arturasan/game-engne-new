# Concepts — mental model for the engine

This series teaches the engine the way Bevy's book teaches Bevy: concept by concept, with the smallest runnable example for each. Read in order on your first day; refer back as needed.

If you are coming from Bevy, the API names will feel familiar. The shapes are deliberately close. C++26 ergonomics differ from Rust in a few places — those differences get called out where they matter.

If you are **not** coming from Bevy, ignore the "Bevy mapping" footnotes; the docs stand on their own.

## Read in this order

1. `00-mental-model.md` — the one-paragraph version of the whole engine.
2. `01-app.md` — the program entry point.
3. `02-world.md` — the data store.
4. `03-entities-components.md` — the unit of data.
5. `04-systems.md` — the unit of behavior.
6. `05-queries.md` — how systems find their data.
7. `06-commands.md` — how systems mutate the world safely.
8. `07-resources.md` — shared singleton state.
9. `08-events.md` — typed message channels.
10. `09-schedules.md` — ordering and parallelism.
11. `10-plugins.md` — modular composition.
12. `11-assets.md` — handles, loaders, hot reload.
13. `12-rendering.md` — main world vs render world vs extract.
14. `13-from-bevy.md` — translation table for Bevy readers.

## What "concept docs" are not

- Not reference. For exact signatures and decisions, see `docs/architecture/`.
- Not specs. For PR-sized tasks, see `specs/`.
- Not tutorials with running examples. Examples live under `examples/`.

A concept doc earns its place by giving a **mental model** — the right picture in your head so the reference and the code make sense. If a concept doc grows past one screen, it is doing too much.
