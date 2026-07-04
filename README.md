# Mad Pod Racing

A C++ bot for [CodinGame](https://www.codingame.com)'s **Mad Pod Racing** challenge (originally "Coders Strike Back"): a turn-based pod-racing game where the goal is to climb the leagues — Wood → Bronze → Silver → Gold → Legend — by reading game state from stdin and writing thrust/steering commands to stdout each turn.

## Overview

The bot is a single, self-contained C++ source file (`main.cpp`) that gets pasted directly into CodinGame's online editor — no build system, no external dependencies, standard library only. It reads the current race state each turn and outputs a target position and thrust level (or `BOOST`/`SHIELD`) for its pod.

Alongside the bot, this repo includes a local **A/B test harness** — a full physics simulator that races two bot binaries against each other across a set of tracks and reports whether a given change is a genuine improvement, a sidegrade, or a regression.

## Requirements

- A C++17-capable compiler (`g++`, matching CodinGame's own toolchain)
- Optionally, [Nix](https://nixos.org/) — a `shell.nix` is provided with a matching dev environment

```bash
nix-shell   # optional: drops you into a shell with g++, gdb, and clangd
```

## Build & run the bot

```bash
g++ -std=c++17 -O2 -Wall -Wextra -o pod main.cpp
./pod < tests/sample_input.txt   # replay against a captured turn-by-turn input dump
```

There's no test framework on CodinGame itself — local verification means replaying recorded input, or using the A/B harness described below.

## I/O protocol

Output, one line per controlled pod, every turn: `targetX targetY thrust`, where `thrust` is `0`-`100`, or `BOOST` (one-shot per race), or `SHIELD` (skips thrust, triples collision mass, then cools down).

Input format differs by league:
- **Wood 2** (1 pod, no init block): each turn reads `x y nextCheckpointX nextCheckpointY nextCheckpointDist nextCheckpointAngle`, then the opponent's `x y`.
- **Bronze and above** (2 pods per side, full track known up front): reads `laps`, `checkpointCount`, then each checkpoint's coordinates once; every turn after reads 2 of your pods and 2 opponent pods as `x y vx vy angle nextCheckpointId`.

See `CLAUDE.md` for the full physics specification (rotation, thrust, friction, collision constants) that any local simulator needs to replicate exactly.

## A/B test harness

`tests/harness/` is a full physics simulator (rotation, thrust, friction, and elastic pod-pod collisions with SHIELD's mass tripling) that races two arbitrary bot binaries — one in-process stand-in opponent, real subprocesses for the bots under test — across four approximated tracks (oval, hairpin, long straight, mixed), and reports a side-by-side scorecard: turns-to-finish, checkpoints missed, compute-budget violations, and boost effectiveness (via a shadow no-boost baseline run).

```bash
# Build the harness once
g++ -std=c++17 -O2 -Wall -Wextra -o tests/harness/mpr_harness \
  tests/harness/harness_main.cpp tests/harness/race.cpp tests/harness/physics.cpp \
  tests/harness/bot_process.cpp tests/harness/opponent.cpp tests/harness/tracks.cpp

# Build the two versions you want to compare, then run the harness
g++ -std=c++17 -O2 -o /tmp/bot_a main.cpp
g++ -std=c++17 -O2 -o /tmp/bot_b main_variant.cpp
./tests/harness/mpr_harness /tmp/bot_a /tmp/bot_b
```

`tests/harness/fixtures/` holds small reference bots (`good_bot`, `bad_bot`, `crash_bot`, `garbage_bot`, `slow_bot`, `hang_bot`, `boost_demo_bot`) used to verify the harness itself behaves correctly.

Track layouts are best-effort approximations of real CodinGame maps, not verified-exact coordinates.

## Project structure

```
main.cpp              # the submittable bot (single translation unit)
shell.nix             # Nix dev shell matching CodinGame's g++ toolchain
CLAUDE.md             # detailed game spec, strategy notes, and dev workflow
tests/
  harness/            # A/B test harness (physics sim, subprocess runner, scorecard)
    fixtures/         # reference bots for self-verifying the harness
```

## Strategy progression

- **Wood/Bronze**: aim at the next checkpoint, cut thrust on wide angles, boost on long straights.
- **Silver/Gold**: anticipate the checkpoint after next for racing-line cornering, manage drift.
- **Gold/Legend**: split roles between a racing pod and a blocker/interceptor pod, with forward-simulated search over thrust/angle sequences.

See `CLAUDE.md` for the full strategy and physics notes.

## License

MIT
