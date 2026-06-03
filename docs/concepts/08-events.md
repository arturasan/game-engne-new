# 08 — Events

Events are **typed message channels**: one writer says "this happened", any number of readers see it. Per type, double-buffered, dropped after two frames.

Events are how decoupled systems communicate. Input → game logic. Game logic → audio. Anything → UI.

## Declaring an event type

```cpp
struct EnemyKilled {
    engine::Entity killer;
    engine::Entity victim;
    int            xp_reward;
};
```

Then register it with the app (once, in a plugin):

```cpp
app.add_event<EnemyKilled>();
```

`add_event<E>` inserts an `Events<E>` resource and registers a per-frame swap system in the `First` schedule.

## Sending

```cpp
void score_on_kill(EventReader<DamageEvent> dmg, EventWriter<EnemyKilled> killed,
                   Query<const Health> q)
{
    for (const auto& d : dmg.read()) {
        if (auto* h = q.get(d.target); h && h->hp <= 0) {
            killed.send(EnemyKilled{
                .killer  = d.attacker,
                .victim  = d.target,
                .xp_reward = 10,
            });
        }
    }
}
```

`send` is a typed push into the current frame's buffer. No allocation per call after the first.

## Receiving

```cpp
void award_xp(EventReader<EnemyKilled> killed, ResMut<Score> score) {
    for (const auto& e : killed.read()) {
        score->value += e.xp_reward;
    }
}

void play_kill_sound(EventReader<EnemyKilled> killed, ResMut<AudioMixer> mixer) {
    for (const auto& _ : killed.read()) {
        mixer->play_one_shot(sounds.death, {});
    }
}
```

Both readers see every event exactly once. They are independent: removing one doesn't affect the other.

## How "exactly once per reader" works

Each `EventReader<E>` tracks a **per-system cursor** into the `Events<E>` buffer. When the system runs, `read()` returns the span from the cursor to the buffer's end and advances the cursor.

So:

- Two systems reading the same event each see it once.
- A system that runs twice in a frame (rare) sees each event once total.
- A system that skips a frame still sees the events from the missed frame, until they expire.

## Lifetime: two-frame buffer

`Events<E>` holds two buffers, swapped each frame. An event sent in frame N is readable in frame N and frame N+1, then dropped.

That window exists to handle ordering quirks: if writer system A runs in `PostUpdate` of frame N and reader B runs in `Update` of frame N+1, B still sees it.

Consequence: **you must read events within one frame** of when they fire, or you miss them. If you need long-lived state, copy event data into a component or resource as you read.

## Events vs Commands

| Use Events when… | Use Commands when… |
|---|---|
| Many systems may want to react | One specific structural change |
| The producer doesn't know who consumes | The producer knows exactly what to do |
| The reaction is per-frame, not retained | The change is to entity/component structure |
| Example: `KeyPressed`, `EnemyKilled` | Example: `cmd.despawn(e)` |

If you find yourself sending an event whose only consumer is your own next system, just call a function or use `Commands` directly.

## A common pattern: input → event → world change

```cpp
// Plugin wires it up.
app.add_event<JumpRequested>();
app.add_system<PreUpdate>(emit_jump_on_space);
app.add_system<Update>(apply_jump_to_player);

// Producer.
void emit_jump_on_space(Res<Input> input, EventWriter<JumpRequested> ev) {
    if (input->key_just_pressed(Key::Space)) ev.send({});
}

// Consumer.
void apply_jump_to_player(EventReader<JumpRequested> ev, Query<Velocity, With<Player>> q) {
    for (const auto& _ : ev.read()) {
        for (auto [_, v] : q) v.linear.y = 8.0f;
    }
}
```

Two small systems, easy to test, easy to swap. The producer doesn't know about players; the consumer doesn't know about input keys.

---

**Bevy mapping:** `EventReader<E>` / `EventWriter<E>` and double-buffered `Events<E>` ↔ Bevy's identically named types. Same per-reader cursor, same two-frame retention.
