---
name: tune-bot
description: Runs one compass-search hill-climbing iteration over the Mad Pod Racing bot's tunable heuristic constants, scored via the physics A/B harness, and keeps the change only if it scores better. Self-orchestrating loop. Use when the user says "/tune-bot", "run tune-bot", "tune the bot", or "improve the bot's constants".
---

# Tune Bot — Loop

> ## ⚙️ Loop Training Mode: **ON**
> Flip this toggle by changing the line above to `OFF`. It changes how the loop runs:
>
> **When ON (default):**
> - Pause at **every step** and wait for my explicit approval before continuing.
> - **Skip** any step that already passes its done-rule (don't redo finished work).
> - Only **re-run steps that fail** their done-rule.
> - Respect the retry cap below — never loop forever.
>
> **When OFF:**
> - Run **autonomously**, no pauses.
> - Still run **every done-rule check** and still respect the **retry cap**.
> - ON-mode pauses need an interactive turn; use OFF for unattended/`/loop`-scheduled runs.
>
> **Retry cap:** 3 attempts per failing step, then stop and report.

## Goal

Try one nudge to one of `main.cpp`'s named tunable heuristic constants, A/B-score it against the current `main.cpp` via the physics harness, and keep the nudge only if it scores strictly better — one coordinate-ascent step per run.

## Overall done-rule

One candidate nudge was tried against current-best `main.cpp` via `tests/harness/mpr_harness`, a scalarized-cost accept/reject decision was made, `.claude/loops/tune-bot/state.json` reflects it (updated `history`, round-robin index, step size / direction for the tried constant), and `main.cpp` is either unchanged (rejected) or holds the strictly-better candidate (accepted, left uncommitted for review).

## Steps

For each step: read its done-rule FIRST. In Training Mode ON, if the done-rule already
passes, skip the step and tell me. Otherwise run it; if it fails, retry up to the cap,
then stop and report which step blocked.

1. **Read prior notes** — read the most recent `.claude/loops/tune-bot/memory-*.md` (if any) for its "Remember next run" section (e.g. a prior plateau warning). This is advisory only — the actual search state (step sizes, round-robin index, per-constant fail streaks, full history) lives in `.claude/loops/tune-bot/state.json`, owned directly by the script in step 3.
   - Done-rule: latest memory file (if one exists) has been read and its notes considered before continuing.

2. **Confirm `main.cpp` builds clean as the current best** — `g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/tune_bot_sanity main.cpp` from the repo root.
   - Done-rule: exits 0.

3. **Run one compass-search iteration** — `python3 .claude/skills/tune-bot/scripts/tune_step.py` from the repo root (give it a generous timeout, 280s+ — it covers 4 tracks through the harness). It picks the next constant round-robin, nudges it by its current per-constant step size in its next-due direction, builds both the current-best and candidate binaries, runs `tests/harness/mpr_harness`, computes a scalarized cost per side (turns-to-finish summed across tracks, DNF/CRASH as a 1000 penalty, +100/checkpoint missed, +50/compute-budget violation, −boost-turns-saved), and overwrites `main.cpp` with the candidate iff its cost is strictly lower. It always updates `state.json`: advances the round-robin index, flips or shrinks the tried constant's step/direction on rejection, and appends a history entry.
   - Done-rule: script exits 0 and prints a JSON summary containing `constant`, `old_value`, `tried_value`, `cost_best`, `cost_candidate`, `decision` (`accepted`/`rejected`), and `plateaued`.

4. **Report the outcome** — summarize in plain English: which constant was tried and in which direction, old value → tried value, cost_best vs cost_candidate, and the decision. If `main.cpp` changed, explicitly note it's an **uncommitted** edit (this loop never commits on its own — the user reviews/commits accepted nudges in their own batches). If `plateaued` is true (a full round-robin cycle with zero acceptances), say so and suggest either treating the search as converged at current step sizes, or manually widening a step in `state.json` to keep probing.
   - Done-rule: a plain-English summary covering all of the above has been given.

## Verification plan

Before declaring the run done, prove the overall done-rule holds:
- `g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/tune_bot_verify main.cpp` exits 0 (main.cpp still compiles, whichever way the decision went).
- If accepted: `git diff --stat main.cpp` shows a change touching only the tunable-constants block (sanity check the script only changed the one named constant, nothing else).
- `.claude/loops/tune-bot/state.json`'s last `history` entry's `run` matches its top-level `run_count`.
If any check fails, the run is NOT done — record it in the Memory file and stop.

## End-of-run: write two files (ALWAYS)

Resolve `<today>` as the current date (YYYY-MM-DD). Write BOTH files into
`.claude/loops/tune-bot/`:

1. **Output** → `.claude/loops/tune-bot/output-<today>.md`
   The actual narrative this run produced: which constant, direction, old → tried value, the harness scorecard summary (per-track turns/checkpoints-missed/compute-violations/boost deltas for both sides), the computed costs, and the decision.

2. **Memory** → `.claude/loops/tune-bot/memory-<today>.md`, with this shape:
   ```
   # tune-bot run — <today>

   - Mode: ON | OFF
   - Result: done | blocked at step <n>
   - Steps skipped (already passed): <list>
   - Steps re-run: <list, with attempt counts>

   ## What worked
   - …

   ## What failed
   - …

   ## Remember next run
   - … (e.g. plateau warnings, a constant that's been rejected in both directions
     repeatedly, or anything worth a human glance before the next iteration)
   ```

   At the START of every run, read the most recent `memory-*.md` in this dir (if any)
   and apply its "Remember next run" notes before doing anything else.

## Report

Tell me: the mode, the result, which steps were skipped vs re-run, where the two files
were written, and anything from "Remember next run" worth acting on. If `main.cpp` was
changed, remind me it's uncommitted and ask whether I want to commit it now or keep
iterating first.
