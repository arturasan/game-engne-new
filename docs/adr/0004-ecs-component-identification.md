# 0004 — ECS component identification uses process-local integer ids

- Status: accepted
- Date: 2026-06-03
- Related: 0001 ECS archetype storage

## Context

Archetype storage needs a compact component identifier for signatures, column lookup, and add/remove edge caches. Public engine headers must not expose third-party types, and hot ECS paths should avoid `std::type_index` lookups when a dense integer is enough.

## Decision

Each component type receives a lazily allocated `engine::ComponentId`, implemented as a monotonic `std::uint32_t` counter. The id is stable only within the current process and is intended for runtime storage, archetype signatures, and query matching.

The id is not serialized. Future serialization or reflection specs must define a separate stable type name or schema identifier.

## Consequences

- Archetype signatures are small sorted arrays of integers.
- Query and transition matching can compare integer component sets without `std::type_index` in the hot path.
- Component ids depend on first-use order, so they are deterministic only for the same process execution path.
- Save files, replay schemas, and editor reflection need a different stable identifier when those specs land.
