# Mad Pod Racing

A C++ bot for [CodinGame](https://www.codingame.com)'s **Mad Pod Racing** puzzle: a turn-based pod-racing game where the goal is to climb the leagues — Wood → Bronze → Silver → Gold → Legend — by reading game state from stdin and writing thrust/steering commands to stdout each turn.

## Current Status

Currently competing in **Silver league**. Confirmed via the CodinGame IDE that this puzzle uses a **single-pod protocol across every league** (no init block, no second pod) — new mechanics like SHIELD unlock progressively without changing that shape. This is a simpler, single-pod ladder, distinct from the 2-pods-per-side "Coders Strike Back" contest.

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

**Confirmed unchanged across every league so far (through Silver):** each turn reads your pod's `x y nextCheckpointX nextCheckpointY nextCheckpointDist nextCheckpointAngle`, then the opponent's `x y` (position only — no velocity/angle for the opponent). No init block; the track is discovered by watching the checkpoint coordinates change turn to turn, and it repeats every lap. Gold/Legend's protocol is unconfirmed — verify against the IDE before assuming it stays this way.

See `CLAUDE.md` for the full physics specification (rotation, thrust, friction, collision constants) that any local simulator needs to replicate exactly.

## A/B test harness

`tests/harness/` is a full physics simulator (rotation, thrust, friction, and elastic pod-pod collisions with SHIELD's mass tripling) that races two arbitrary bot binaries — one in-process stand-in opponent, real subprocesses for the bots under test — across four approximated tracks (oval, hairpin, long straight, mixed), and reports a side-by-side scorecard: turns-to-finish, checkpoints missed, compute-budget violations, and boost effectiveness (via a shadow no-boost baseline run).

```bash
# Build the harness once
./tests/harness/build.sh

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
- **Silver (current)**: anticipate the checkpoint after next for racing-line cornering (tapered to avoid missing the checkpoint's capture radius), speed-aware braking, velocity-drift-compensated steering, boost reserved for the single longest leg of the track, and predictive (not just proximity-based) SHIELD.
- **Gold/Legend**: unconfirmed whether these introduce a second controllable pod — verify against the IDE when reaching that point rather than assuming.

See `CLAUDE.md` for the full strategy and physics notes.

## Recent Changes

- **2026-07-04**: Built the A/B test harness and used it to find and fix real bugs: a SHIELD physics fidelity bug in the harness itself, a SHIELD-stall heuristic and cross-lap checkpoint desync in `main.cpp`, and — found only through harness validation — a deeper bug where the checkpoint-targeting blend could aim outside a checkpoint's capture radius entirely. Added speed-aware braking, longest-leg boost targeting, and drift-compensated steering. Corrected CLAUDE.md's league/protocol description after confirming the real protocol via the CodinGame IDE. Total turns-to-finish across the 4 test tracks dropped ~23% with zero regressions.
- Initial commit: Wood-league bot, A/B test harness, and dev tooling.

## License

MIT
