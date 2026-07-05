#!/usr/bin/env python3
"""One compass-search iteration over main.cpp's tunable heuristic constants.

Picks the next constant (round-robin), nudges it by its current step size in
its next-due direction, builds both binaries, runs the physics harness, scores
both sides from the harness's own per-bot columns, and accepts the nudge into
main.cpp iff it scores strictly better. Search metadata (step sizes, next
direction, fail streaks, round-robin index, history) persists in
tune-bot-runs/state.json across invocations. main.cpp itself is the
running "current best" -- accepted nudges are left as an uncommitted edit for
the user to review/commit.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

# name -> (type, min, max, initial_step_fraction)
CONSTANTS = {
    "THRUST_ZERO_ANGLE_DEG":         ("int",    30,   179,  0.10),
    "THRUST_ANGLE_FACTOR":           ("double", 0.1,  2.0,  0.10),
    "BRAKING_ZONE_MIN":              ("double", 200,  5000, 0.10),
    "BRAKING_ZONE_SPEED_MULT":       ("double", 0.5,  8.0,  0.10),
    "BRAKING_THRUST_CAP":            ("int",    0,    100,  0.10),
    "BOOST_ANGLE_MAX_DEG":           ("int",    0,    90,   0.10),
    "BOOST_DIST_MIN":                ("int",    500,  12000,0.10),
    "SHIELD_DIST_TRIGGER":           ("double", 200,  3000, 0.10),
    "SHIELD_PREDICTED_DIST_TRIGGER": ("double", 200,  3000, 0.10),
    "TARGET_BLEND_MAX_WEIGHT":       ("double", 0.0,  1.0,  0.10),
    "TARGET_BLEND_DIST_NORM":        ("double", 500,  8000, 0.10),
    "DRIFT_CORRECTION_FACTOR":       ("double", 0.0,  2.0,  0.10),
}
CONST_ORDER = list(CONSTANTS.keys())

DNF_PENALTY = 1000
CHECKPOINT_MISS_WEIGHT = 100
COMPUTE_VIOLATION_WEIGHT = 50
MIN_INT_STEP = 1
MIN_STEP_FLOOR_FRACTION = 0.01  # don't shrink a step below 1% of the constant's initial value


def repo_root() -> Path:
    out = subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True, check=True)
    return Path(out.stdout.strip())


def parse_constants(main_cpp_text: str) -> dict:
    values = {}
    for name in CONST_ORDER:
        m = re.search(rf"static const (?:int|double)\s+{name}\s*=\s*([-\d.eE+]+);", main_cpp_text)
        if not m:
            raise SystemExit(f"Could not find constant {name} in main.cpp -- has the tunable block been edited?")
        values[name] = float(m.group(1))
    return values


def format_value(name: str, value: float) -> str:
    ctype = CONSTANTS[name][0]
    if ctype == "int":
        return str(int(round(value)))
    return repr(round(value, 6))


def apply_constant(main_cpp_text: str, name: str, new_value: float) -> str:
    formatted = format_value(name, new_value)
    pattern = rf"(static const (?:int|double)\s+{name}\s*=\s*)([-\d.eE+]+)(;)"
    new_text, n = re.subn(pattern, lambda m: f"{m.group(1)}{formatted}{m.group(3)}", main_cpp_text)
    if n != 1:
        raise SystemExit(f"Expected exactly one occurrence of {name}, found {n}")
    return new_text


def load_state(state_path: Path, current_values: dict) -> dict:
    if state_path.exists():
        state = json.loads(state_path.read_text())
        # Backfill any constant added to CONSTANTS after state.json was first created.
        for name in CONST_ORDER:
            if name not in state["per_constant"]:
                state["per_constant"][name] = default_constant_state(name, current_values[name])
        return state
    return {
        "index": 0,
        "cycle_no_improve_streak": 0,
        "run_count": 0,
        "per_constant": {name: default_constant_state(name, current_values[name]) for name in CONST_ORDER},
        "history": [],
    }


def default_constant_state(name: str, current_value: float) -> dict:
    ctype, _, _, frac = CONSTANTS[name]
    step = max(abs(current_value) * frac, MIN_INT_STEP if ctype == "int" else 1e-6)
    return {"step": step, "next_dir": "+", "consec_fail": 0, "initial_value": current_value}


def save_state(state_path: Path, state: dict) -> None:
    state_path.write_text(json.dumps(state, indent=2))


def build(binary_path: Path, source_path: Path) -> None:
    r = subprocess.run(
        ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-o", str(binary_path), str(source_path)],
        capture_output=True, text=True,
    )
    if r.returncode != 0:
        raise SystemExit(f"Build failed for {source_path}:\n{r.stderr}")


def run_harness(harness_bin: Path, bot_a: Path, bot_b: Path) -> str:
    r = subprocess.run([str(harness_bin), str(bot_a), str(bot_b)], capture_output=True, text=True, timeout=280)
    if r.returncode != 0:
        raise SystemExit(f"Harness run failed:\n{r.stderr}")
    if "=== Overall Verdict ===" not in r.stdout:
        raise SystemExit(f"Harness output missing expected verdict section:\n{r.stdout}")
    return r.stdout


def score_harness_output(output: str) -> tuple:
    """Returns (cost_a, cost_b); lower is better."""
    cost = {"A": 0.0, "B": 0.0}
    for line in output.splitlines():
        line = line.strip()
        m = re.match(r"Turns to finish:\s+A=(\S+)\s+B=(\S+)", line)
        if m:
            for side, val in zip("AB", m.groups()):
                cost[side] += DNF_PENALTY if val in ("DNF", "CRASH") else float(val)
            continue
        m = re.match(r"Checkpoints missed:\s+A=(\d+)\s+B=(\d+)", line)
        if m:
            for side, val in zip("AB", m.groups()):
                cost[side] += CHECKPOINT_MISS_WEIGHT * int(val)
            continue
        m = re.match(r"Compute budget viol\.:\s+A=(\d+)\s+B=(\d+)", line)
        if m:
            for side, val in zip("AB", m.groups()):
                cost[side] += COMPUTE_VIOLATION_WEIGHT * int(val)
            continue
        m = re.match(r"Boost effectiveness:\s+A=(N/A|-?\d+)(?: turns saved)?\s+B=(N/A|-?\d+)(?: turns saved)?", line)
        if m:
            for side, val in zip("AB", m.groups()):
                if val != "N/A":
                    cost[side] -= int(val)
            continue
    return cost["A"], cost["B"]


def main() -> None:
    root = repo_root()
    main_cpp_path = root / "main.cpp"
    state_path = root / "tune-bot-runs" / "state.json"
    harness_bin = root / "tests" / "harness" / "mpr_harness"

    best_text = main_cpp_path.read_text()
    current_values = parse_constants(best_text)
    state = load_state(state_path, current_values)

    name = CONST_ORDER[state["index"] % len(CONST_ORDER)]
    cstate = state["per_constant"][name]
    ctype, lo, hi, _ = CONSTANTS[name]
    direction = 1 if cstate["next_dir"] == "+" else -1
    raw_new_value = current_values[name] + direction * cstate["step"]
    new_value = max(lo, min(hi, raw_new_value))

    candidate_text = apply_constant(best_text, name, new_value)

    tmp = Path("/tmp")
    best_src, cand_src = tmp / "tune_bot_best.cpp", tmp / "tune_bot_candidate.cpp"
    best_bin, cand_bin = tmp / "tune_bot_best", tmp / "tune_bot_candidate"
    best_src.write_text(best_text)
    cand_src.write_text(candidate_text)
    build(best_bin, best_src)
    build(cand_bin, cand_src)

    subprocess.run([str(root / "tests" / "harness" / "build.sh")], capture_output=True, text=True, check=True)
    output = run_harness(harness_bin, best_bin, cand_bin)
    cost_best, cost_candidate = score_harness_output(output)

    accepted = cost_candidate < cost_best
    if accepted:
        main_cpp_path.write_text(candidate_text)
        cstate["consec_fail"] = 0
        state["cycle_no_improve_streak"] = 0
    else:
        cstate["consec_fail"] += 1
        cstate["next_dir"] = "-" if cstate["next_dir"] == "+" else "+"
        if cstate["consec_fail"] >= 2:
            floor = abs(cstate["initial_value"]) * MIN_STEP_FLOOR_FRACTION
            cstate["step"] = max(cstate["step"] / 2, floor, MIN_INT_STEP if ctype == "int" else 1e-6)
            cstate["consec_fail"] = 0
        state["cycle_no_improve_streak"] += 1

    state["index"] = (state["index"] + 1) % len(CONST_ORDER)
    state["run_count"] += 1
    plateaued = state["cycle_no_improve_streak"] >= len(CONST_ORDER)
    entry = {
        "run": state["run_count"],
        "constant": name,
        "old_value": current_values[name],
        "tried_value": new_value,
        "direction": "+" if direction > 0 else "-",
        "cost_best": cost_best,
        "cost_candidate": cost_candidate,
        "decision": "accepted" if accepted else "rejected",
    }
    state["history"].append(entry)
    state_path.parent.mkdir(parents=True, exist_ok=True)
    save_state(state_path, state)

    summary = {
        **entry,
        "plateaued": plateaued,
        "next_constant": CONST_ORDER[state["index"]],
        "harness_output": output,
    }
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    sys.exit(main())
