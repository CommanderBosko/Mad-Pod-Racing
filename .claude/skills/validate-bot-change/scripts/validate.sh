#!/usr/bin/env bash
# Mechanical build+run steps for the validate-bot-change skill. Interpreting
# the harness's scorecard into a plain-English verdict is left to the caller
# (Claude) -- this script's job ends at printing the raw harness output.
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

if git diff --quiet HEAD -- main.cpp; then
  echo "No uncommitted changes to main.cpp -- nothing to validate against HEAD."
  exit 1
fi

echo "Building baseline (HEAD:main.cpp)..."
git show HEAD:main.cpp > /tmp/main_baseline.cpp
g++ -std=c++17 -O2 -o /tmp/bot_baseline /tmp/main_baseline.cpp

echo "Building candidate (working-tree main.cpp)..."
g++ -std=c++17 -O2 -o /tmp/bot_candidate main.cpp

echo "Ensuring the harness is up to date..."
./tests/harness/build.sh

echo "Running the A/B comparison (this can take a few minutes)..."
./tests/harness/mpr_harness /tmp/bot_baseline /tmp/bot_candidate
