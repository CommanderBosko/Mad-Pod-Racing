# Project State

## Current Project State

**Working and validated:**
- `main.cpp` — single-pod Wood/Silver-league bot. Compiles clean (`g++ -std=c++17 -O2 -Wall -Wextra`), no warnings. Implements checkpoint discovery with robust cross-lap tracking, distance-tapered next-next-checkpoint blending, predictive (not just proximity-based) SHIELD with a self-tracked 3-turn cooldown, speed-aware braking, boost reserved for the single longest leg of the track, and velocity-drift-compensated steering.
- `tests/harness/` — full physics A/B test harness (elastic pod-pod collisions incl. SHIELD's 3x mass, subprocess-driven bot comparison, shadow no-boost baseline for boost-effectiveness measurement) across 4 approximated tracks (Simple Oval, Hairpin, Long Straight, Mixed). All 9 harness-correctness checks pass (build, self-comparison, known-worse-bot detection, elimination/DNF handling, crash/hang/malformed-output handling, compute-budget flagging, real collision physics, determinism, boost effectiveness). Build command is now centralized in `tests/harness/build.sh` (single source of truth, referenced from CLAUDE.md/README.md/the skill below).
- `tests/harness/fixtures/` — 7 reference stub bots used to validate the harness itself.
- `.claude/skills/validate-bot-change/` — project-local skill that automates the build-baseline/build-candidate/run-harness/report-verdict loop; invoke with "validate this change" or similar. Its mechanical steps live in `scripts/validate.sh`, tested end-to-end (no-diff early exit, and full build+run path).
- Repo is live and public: https://github.com/CommanderBosko/Mad-Pod-Racing (SSH remote, `main` branch).

**Confirmed via CodinGame IDE screenshot (2026-07-04):** the user is in **Silver league**, and this specific "Mad Pod Racing" puzzle uses **one single-pod protocol across every league** (no init block, no 2-pod format) — this is *not* the "Coders Strike Back" 2-pods-per-side game. SHIELD was confirmed newly unlocked entering Silver. Gold/Legend protocol is still unconfirmed.

**Not yet done:**
- The `main.cpp` in this repo has not yet been re-pasted into the CodinGame IDE by the user (they received it as a ready-to-paste block at the end of the last session but confirmation of an actual submission/rank change hasn't come back).
- No Bronze-era or lower-league variants are tracked; this repo targets the bot's current (Silver) league only.

## Current Goals

**Short-term (next 1-3 sessions):**
- Confirm the pasted bot's actual performance/rank change on CodinGame once submitted.
- Investigate why Mixed track showed *negative* boost effectiveness (-19 turns) despite boosting on the computed "longest leg" — likely an interaction between the boost and the track's geometry right after that leg.
- Consider tuning the SHIELD proximity/prediction thresholds (900/800 units) further using the harness.

**Long-term:**
- Climb further leagues (Gold, Legend). When reaching a new league, verify the I/O protocol directly against the CodinGame IDE (screenshot or the "Game input" statement) rather than assuming — this project has already been burned once by an incorrect assumption (Bronze+ was wrongly assumed to require 2 pods).
- If Gold/Legend do introduce a 2nd controllable pod, both `main.cpp` and `tests/harness/` will need a real structural extension (parser, physics for 2v2, potential blocker/runner role split) — this is a large jump, not an incremental tweak.

## Recent Decisions

- **CLAUDE.md's league/protocol section was factually wrong and has been corrected.** It previously claimed "Bronze and above" requires 2 pods + an init block, based on assumptions borrowed from "Coders Strike Back." Direct evidence (an IDE screenshot at Silver league) showed this puzzle keeps the single-pod protocol throughout. Lesson: verify protocol/rule claims against the actual IDE before building against them, especially for game-specific mechanics that vary between similarly-named CodinGame puzzles.
- **Test harness design**: full physics simulator (not a static replay), 4 approximated tracks, in-process stand-in opponent (simple aim-at-next-checkpoint, no boost/shield), subprocess-driven bots under test, side-by-side scorecard with a verdict tally but no single blended score — the user wanted to make the final call, not have it made for them.
- **Boost effectiveness measurement**: implemented via a "shadow" no-boost baseline run (same bot, same track, but the harness intercepts and neutralizes any BOOST command into max thrust) rather than a simple yes/no usage flag — gives a real, comparable turns-saved number.
- **Root-caused and fixed 4 real bugs this session**, 2 in the test harness itself and 2 (plus one deeper one found via validation) in `main.cpp` — see `tests/harness/`'s design and the latest commit message for specifics. The deepest one (fixed-weight next-next-checkpoint blend can aim outside the checkpoint's capture radius at close range) was the single biggest source of DNFs and was not one of the two originally-known weaknesses — it surfaced only because every change was harness-validated rather than taken on faith.
- **Ran an `/improve-system` sweep**: built the `validate-bot-change` skill (see above) to stop hand-running the same build+harness+report loop; extracted `tests/harness/build.sh` after the audit found the harness build command duplicated in 3 places (CLAUDE.md, README.md, and the new skill) with real drift risk. `fewer-permission-prompts` and `claude-rules` passes came back clean (nothing to add). One global-skill gotcha was added to `session-closer` (repo-managed, in the NixOS dotfiles repo) — that change needs a rebuild + new session there before it's live, unrelated to this repo's own state.

## Known Issues / Tech Debt

- Boost effectiveness on the "Mixed" track is negative (-19 turns) — not yet root-caused, noted as a next-step.
- Track layouts in `tests/harness/tracks.cpp` are best-effort approximations of real CodinGame maps, not verified-exact coordinates.
- Gold/Legend protocol assumptions are explicitly unconfirmed (see CLAUDE.md) — do not build against them without verifying first.

## Next Steps

1. Get the current `main.cpp` pasted into CodinGame's IDE and confirm real-world rank movement in Silver league.
2. Investigate the Mixed-track negative boost effectiveness using the harness (`./tests/harness/mpr_harness`).
3. When/if progressing to Gold league, verify the I/O protocol against the IDE before writing any new parser code.
