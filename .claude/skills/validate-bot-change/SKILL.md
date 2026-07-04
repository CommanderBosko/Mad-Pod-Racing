---
name: validate-bot-change
description: A/B validates a modified main.cpp against the currently committed version using the tests/harness A/B test harness, and reports a plain-English verdict. Use when the user says "validate this change", "A/B test the bot", "compare against baseline", "run the harness on this", or "is this actually better".
---

# Validate Bot Change

Proves whether an uncommitted change to `main.cpp` is a real improvement over the currently committed version, using this repo's existing physics A/B test harness. (Bucket: Verification)

## Steps

1. **Run the build+compare script** from the repo root (works from any subdirectory — it resolves the repo root itself), with a generous timeout (280s+, the harness covers 4 tracks and can take several minutes):
   ```bash
   bash .claude/skills/validate-bot-change/scripts/validate.sh
   ```
   This handles everything mechanical: checking there's an uncommitted `main.cpp` change to validate (exits 1 with a clear message and no harness run if the working tree matches `HEAD`), building the baseline (`HEAD:main.cpp`) and candidate (working-tree `main.cpp`) binaries, rebuilding `tests/harness/mpr_harness` via `tests/harness/build.sh`, and running the comparison. If a build step fails, the script stops with the compiler error — don't attempt to reinterpret a failed run as a scorecard.

2. **Report a plain-English summary** of the script's harness output, not a raw dump of it:
   - Which tracks improved, regressed, or tied (by turns-to-finish)
   - The total turns-to-finish summed across all tracks, baseline vs candidate, and the net delta/percentage
   - Any change in DNF/crash status, checkpoints missed, or compute-budget violations
   - Any track where boost effectiveness went from N/A to a real number (or vice versa) — call this out explicitly since it signals a change in whether/when boost is being used
   - The harness's own overall verdict tally ("B improved on N/M metrics") as a closing line, but lead with the plain-English summary above it, not the raw scorecard
