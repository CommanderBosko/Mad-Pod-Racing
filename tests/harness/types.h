#pragma once

#include <string>
#include <vector>

// ---- Map / track constants (mirrors main.cpp's game spec) ----
constexpr int MAP_WIDTH = 16000;
constexpr int MAP_HEIGHT = 9000;
constexpr int CHECKPOINT_RADIUS = 600;
constexpr int POD_RADIUS = 400;
constexpr int MAX_THRUST = 100;
constexpr double BOOST_THRUST = 650.0;
constexpr int ELIMINATION_TIMEOUT_TURNS = 100;
constexpr double FRICTION = 0.85;
constexpr double MAX_TURN_DEGREES = 18.0;
constexpr int SHIELD_COOLDOWN_TURNS = 3;

// Compute budget (approximate, per CLAUDE.md): first turn ~1000ms, later turns tens of ms.
constexpr long long FIRST_TURN_BUDGET_MS = 1000;
constexpr long long TURN_BUDGET_MS = 75;

struct Checkpoint {
    int x;
    int y;
};

struct Track {
    std::string name;
    std::vector<Checkpoint> checkpoints; // cyclic: after last, wraps to [0]
    int laps = 3;
};

// A pod's full physics state, tracked internally by the simulator regardless
// of what subset of it any given league's I/O protocol reveals to a bot.
struct Pod {
    double x = 0, y = 0;
    double vx = 0, vy = 0;
    double angle = 0;          // degrees, [-180, 180); true facing direction
    bool angleInitialized = false; // first turn rotates freely to face the target

    int nextCheckpoint = 1;    // index into Track::checkpoints, cyclic
    int lapsCompleted = 0;
    int turnsSinceLastCheckpoint = 0;
    bool eliminated = false;   // true on 100-turn checkpoint timeout
    bool finished = false;     // true once laps == track.laps

    bool boostAvailable = true;
    int boostUsedOnTurn = -1;  // -1 if never used

    int shieldCooldownRemaining = 0; // >0 means shield cannot be reactivated
    double mass = 1.0;         // 1.0 normal, 3.0 for the turn shield is active

    int finishedOnTurn = -1;   // turn number the pod completed the race, -1 if not yet
};

// What a controller (subprocess bot or stand-in opponent) outputs for a turn.
struct TurnCommand {
    double targetX = 0, targetY = 0;
    int thrust = 0;      // 0-100, ignored if boost or shield set
    bool boost = false;
    bool shield = false;
};

// Diagnostic outcome of asking a subprocess bot for its move.
enum class ControllerStatus {
    Ok,
    Crashed,
    MalformedOutput,
    TimedOut,
};

struct ControllerTurnResult {
    TurnCommand command;
    ControllerStatus status = ControllerStatus::Ok;
    long long decisionTimeMs = 0;
    bool overBudget = false;
};

// Outcome of one full race (one version, one track, boost-enabled or shadow).
struct RaceResult {
    bool dnf = false;             // eliminated or timed out overall
    int turnsToFinish = -1;       // -1 if dnf
    int checkpointsMissed = 0;    // count of elimination-timeouts hit before dnf (normally 0 or 1)
    bool boostUsed = false;
    int boostUsedOnTurn = -1;
    int computeBudgetViolations = 0;
    bool crashedOrMalformed = false;
};
