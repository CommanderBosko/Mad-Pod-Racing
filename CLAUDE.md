# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Scope First (Interview)

Before you do any work, use the `/interview` skill to pin down the real goal with the user — don't start building from a fuzzy or assumed understanding of the request. Surface the unknowns, confirm scope and constraints, and only proceed once the target is clear. Do this in tandem with the Verification Plan below: the interview establishes *what* we're building and how we'll know it's done, and the verification plan establishes *how we'll prove* it works. Lay out both together, up front, before starting the work.

## Verification Plan

Before you do any work, state how you'll verify it with the `/verify` skill — say up front how you'll confirm each part actually works before calling it done. Pick the checks that fit this project (build, test suite, linter, type-check, running the app, hitting the endpoint, reading the logs) and name the specific commands. Lay out the plan with the work, not after it.

## Parallelize with Sub-Agents

**This rule is your standing authorization to spawn sub-agents — you do not need to ask first.** Once scope and the verification plan are set, before starting any task with more than one independent part, stop and run a parallelization check. This is a required step, not an aspiration: ask "Can I split this into pieces that don't depend on each other's output?" If yes, spawn one sub-agent per piece in a single message and let them run concurrently.

Trigger parallelization whenever you hit any of these:
- About to research, search, or read across 2+ areas of the tree that don't depend on each other.
- About to scaffold or draft 2+ files whose *contents* don't reference each other.
- You catch yourself planning "do A, then B, then C" where B doesn't need A's result.

Reserve serial work for genuine dependencies — e.g. a new file and the line elsewhere that imports it are coupled, so keep them together. When the pieces are independent, default to fanning out breadth-first; state in your plan which pieces run in parallel and why.

## Use Existing Skills First

Before doing a task by hand, check whether an existing skill already covers it and invoke it instead of improvising. Skills encode the agreed, repeatable way to do a thing — prefer them over ad-hoc steps. If you find yourself doing the same multi-step task a second time and no skill exists, offer to create one.

## Ask via AskUserQuestion

When you need a decision, choice, or clarification from the user — not just information you can look up yourself — use the **AskUserQuestion** tool rather than asking in plain text. Phrase it as 2-4 concrete options (with a recommended one first); when the answer could be open-ended, the tool's built-in "Other" choice covers free text. This keeps answers structured, makes trade-offs explicit, and avoids an answer getting buried in prose. Reserve plain-text questions for genuinely open, generative prompts where no sensible options exist yet (e.g. "describe the project in your own words").

## Project

A bot for the CodinGame **Mad Pod Racing** challenge (originally "Coders Strike Back"), written in **C++**. Goal: climb the leagues (Wood → Bronze → Silver → Gold → Legend) and rank as high as possible. The bot is a single program that plays a turn-based pod-racing game by reading the game state from **stdin** and writing one command per pod to **stdout** each turn.

The repository is currently empty — code starts from scratch.

## The submission model (this constrains everything)

The bot runs inside CodinGame's online IDE: you paste **one source file** into their editor and it compiles/runs there. There is no package manager, no external libraries beyond the C++ standard library, and the whole program must live in a single translation unit.

- Keep the submittable bot in **one file** (e.g. `main.cpp`) so it can be copy-pasted as-is. Do not split into multiple `.cpp`/`.h` files that require a build step CodinGame can't run. If splitting helps locally, add a concat/build step that produces a single paste-ready file, and treat that combined file as the source of truth.
- Standard library only. No Boost, no third-party headers.
- The program must be a loop: read state, decide, print, repeat — it never exits on its own.

## Build & run locally

```bash
# Compile (match CodinGame's flags closely: C++17/20, optimized)
g++ -std=c++17 -O2 -Wall -Wextra -o pod main.cpp

# Run against a captured turn-by-turn input dump
./pod < tests/sample_input.txt
```

There is no test framework on CodinGame. Local "tests" mean feeding a recorded `stdin` dump and eye-checking the `stdout` commands, or running a self-written simulator.

### A/B test harness (`tests/harness/`)

A full physics simulator that A/B compares two compiled bot binaries by racing each through the same set of approximated tracks and reporting a scorecard — the way to check whether a change to `main.cpp` is an actual improvement rather than a sidegrade or regression.

```bash
# Build the harness (one-time, or after editing anything in tests/harness/)
./tests/harness/build.sh

# Build the two bot versions you want to compare, then run the harness against them
g++ -std=c++17 -O2 -o /tmp/bot_a main.cpp        # e.g. current main.cpp
g++ -std=c++17 -O2 -o /tmp/bot_b main_variant.cpp # e.g. a candidate change
./tests/harness/mpr_harness /tmp/bot_a /tmp/bot_b
```

It races each binary (as a real subprocess speaking the Wood-league protocol) against an in-process stand-in opponent across 4 approximated tracks (oval, hairpin, long straight, mixed), 3 laps each, with full pod-pod collision physics. Per track it reports: turns-to-finish (or DNF on the 100-turn elimination timeout), checkpoints missed, compute-budget violations, and boost effectiveness (measured via a shadow no-boost baseline run). It ends with a verdict tally ("B improved on N/M comparable metrics") — the harness reports, you judge.

`tests/harness/fixtures/` holds small reference bots (`good_bot`, `bad_bot`, `crash_bot`, `garbage_bot`, `slow_bot`, `hang_bot`, `boost_demo_bot`) used to verify the harness itself behaves correctly (crash/hang/malformed-output handling, elimination detection, compute-budget flagging, boost measurement) — rebuild any of them with `g++ -std=c++17 -O2 -pthread -o /tmp/<name> tests/harness/fixtures/<name>.cpp` if you need to re-verify a harness change.

Known limitation: tracks are best-effort approximations of real CodinGame layouts, not verified-exact coordinates.

## I/O protocol

**Output, one line per controlled pod, every turn:** `targetX targetY thrust`
- `thrust` is an integer `0`–`100`, or the keyword `BOOST` (one use per race), or `SHIELD` (skips thrust this turn, triples mass for collisions, then cools down ~3 turns).
- The pod does not teleport to the target; it rotates toward `(targetX, targetY)` (max 18°/turn) and accelerates along its facing.

**Confirmed via the CodinGame IDE itself (screenshot, Silver league, 2026-07-04): this specific "Mad Pod Racing" puzzle uses ONE protocol across every league.** Earlier drafts of this doc assumed a Bronze+ jump to 2 pods/init-block/velocity-angle state — that assumption came from "Coders Strike Back" (the full 2v2 contest) and is **wrong for this puzzle**. Every league so far (confirmed through Silver) reads:

- Your pod: `x y nextCheckpointX nextCheckpointY nextCheckpointDist nextCheckpointAngle`
- Opponent: `x y` (position only — no velocity/angle given for the opponent, ever)
- No init block. No checkpoint count. The track is discovered by watching `nextCheckpointX/Y` change turn to turn, and it repeats every lap.
- Output: a single command line (one pod only — this puzzle has no second pod to control).

New mechanics unlock progressively at higher leagues without changing this I/O shape — e.g. SHIELD was confirmed newly available entering Silver (per the league's "Summary of new rules" banner). **Unconfirmed:** whether Gold/Legend introduce a real protocol change (2 pods, etc.) — verify directly against the IDE (or a screenshot of it) before assuming either way when that point is reached, rather than trusting prior training-data assumptions about "Coders Strike Back."

## Game physics (the simulation spec)

The engine resolves each turn in this order — replicate it exactly in any local simulator:
1. **Rotate** toward target, clamped to **±18°** per turn (first turn faces the initial target freely).
2. **Accelerate**: add `thrust` along the (new) facing direction to velocity.
3. **Move**: `position += velocity` for the turn.
4. **Friction**: `velocity *= 0.85`.
5. **Truncate**: position rounded, velocity truncated toward zero.

Constants:
- Map: `16000 x 9000` units.
- Checkpoint radius: `600` (you "hit" it when within 600). Pod radius: `400`.
- Max thrust per turn: `100`. **BOOST**: one-shot ~`650` acceleration; spend it on the longest straightaway.
- **SHIELD**: mass ×3 for collision response that turn, no thrust applied. Per the official rules, **engines stay inactive for the next 3 turns** after use (not just a re-activation cooldown — thrust is fully unavailable during that window) — use to win/deny collisions, not casually.
- A pod is **eliminated** if it doesn't reach its next checkpoint within ~100 turns.
- Per-turn compute budget is tight (first turn longer, ~1s; later turns are short, on the order of tens of ms) — keep any search/simulation within budget.

## Strategy progression (where the difficulty lives)

- **Wood/Bronze:** aim straight at the next checkpoint; cut thrust when the angle to target is wide; trigger BOOST on long straights. This alone clears the early leagues.
- **Silver (current league):** anticipate the *checkpoint after next* and steer for racing-line corners; manage velocity so you don't overshoot; counter-steer against drift; use the newly-unlocked SHIELD tactically (win/deny collisions) rather than as a panic button.
- **Gold/Legend:** unconfirmed whether these introduce a second controllable pod (a runner/blocker split would require it) — this puzzle has stayed single-pod-per-side through Silver, contrary to earlier assumptions borrowed from "Coders Strike Back." Verify against the IDE when reaching that point rather than assuming. Regardless of pod count, top bots run a **forward simulation / genetic-algorithm search** over future thrust+angle sequences each turn — this is why C++ and a faithful physics simulator matter.

When adding meaningful new mechanics or structure (a simulator, a search, a blocker role), update the relevant section above so it stays the source of truth.
