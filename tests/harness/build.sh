#!/usr/bin/env bash
# Single source of truth for building the A/B test harness binary.
# Referenced from CLAUDE.md, README.md, and the validate-bot-change skill --
# update the file list here only, not in three separate places.
set -euo pipefail
cd "$(dirname "$0")"

g++ -std=c++17 -O2 -Wall -Wextra -o mpr_harness \
  harness_main.cpp race.cpp physics.cpp bot_process.cpp opponent.cpp tracks.cpp

echo "Built tests/harness/mpr_harness"
