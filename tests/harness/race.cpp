#include "race.h"
#include "bot_process.h"
#include "opponent.h"
#include "physics.h"

#include <cmath>

namespace {

double normalizeAngleLocal(double a) {
    while (a > 180.0) a -= 360.0;
    while (a <= -180.0) a += 360.0;
    return a;
}

// Spawns a pod near the start checkpoint, offset perpendicular to the
// initial heading (a starting-grid side-by-side layout) so the two pods
// don't begin perfectly coincident, already facing its first target (real
// races start pods pre-aimed, so there's no free-rotation turn to model).
// `perpOffset` must keep the two spawn points comfortably more than 800
// units apart (twice the pod radius, and also the distance main.cpp's own
// SHIELD-if-opponent-near heuristic reacts to) -- a 1v1 race does not start
// with the two racers already touching, and starting them too close
// deadlocks any bot using that kind of proximity heuristic in a way a real
// race never would.
Pod makeStartingPod(const Track& track, double perpOffset) {
    Pod pod;
    const Checkpoint& start = track.checkpoints[0];
    int nextIdx = 1 % static_cast<int>(track.checkpoints.size());
    const Checkpoint& firstTarget = track.checkpoints[nextIdx];

    double headingRad = std::atan2(firstTarget.y - start.y, firstTarget.x - start.x);
    double perpX = -std::sin(headingRad);
    double perpY = std::cos(headingRad);

    pod.x = start.x + perpX * perpOffset;
    pod.y = start.y + perpY * perpOffset;
    pod.nextCheckpoint = nextIdx;

    double dx = firstTarget.x - pod.x;
    double dy = firstTarget.y - pod.y;
    pod.angle = std::atan2(dy, dx) * 180.0 / M_PI;
    pod.angleInitialized = true;
    return pod;
}

void computeCheckpointInfo(const Pod& pod, const Track& track, int& dist, int& angle) {
    const Checkpoint& cp = track.checkpoints[pod.nextCheckpoint];
    double dx = cp.x - pod.x;
    double dy = cp.y - pod.y;
    dist = static_cast<int>(std::llround(std::sqrt(dx * dx + dy * dy)));
    double dirToCp = std::atan2(dy, dx) * 180.0 / M_PI;
    angle = static_cast<int>(std::llround(normalizeAngleLocal(dirToCp - pod.angle)));
}

} // namespace

RaceResult runRace(const std::string& botExecutablePath, const Track& track, bool suppressBoost) {
    RaceResult result;

    BotProcess bot(botExecutablePath);
    if (!bot.start()) {
        result.dnf = true;
        result.crashedOrMalformed = true;
        return result;
    }

    Pod testPod = makeStartingPod(track, -900.0);
    Pod oppPod = makeStartingPod(track, 900.0);

    int numCheckpoints = static_cast<int>(track.checkpoints.size());
    int maxTurns = track.laps * numCheckpoints * (ELIMINATION_TIMEOUT_TURNS + 1);

    int turn = 1;
    while (turn <= maxTurns && !testPod.finished && !testPod.eliminated) {
        int dist, angle;
        computeCheckpointInfo(testPod, track, dist, angle);

        ControllerTurnResult ctrl = bot.takeTurn(
            turn, testPod.x, testPod.y,
            track.checkpoints[testPod.nextCheckpoint].x,
            track.checkpoints[testPod.nextCheckpoint].y,
            dist, angle, oppPod.x, oppPod.y);

        if (ctrl.overBudget) result.computeBudgetViolations++;

        if (ctrl.status == ControllerStatus::Crashed ||
            ctrl.status == ControllerStatus::TimedOut ||
            ctrl.status == ControllerStatus::MalformedOutput) {
            result.crashedOrMalformed = true;
            result.dnf = true;
            break;
        }

        TurnCommand testCmd = ctrl.command;
        if (suppressBoost && testCmd.boost) {
            testCmd.boost = false;
            testCmd.thrust = MAX_THRUST;
        }

        TurnCommand oppCmd = decideOpponentMove(oppPod, track);

        simulateTurn(testPod, testCmd, oppPod, oppCmd, track, turn);

        if (testPod.boostUsedOnTurn >= 0 && !result.boostUsed) {
            result.boostUsed = true;
            result.boostUsedOnTurn = testPod.boostUsedOnTurn;
        }

        turn++;
    }

    if (testPod.finished) {
        result.dnf = false;
        result.turnsToFinish = testPod.finishedOnTurn;
    } else if (!result.crashedOrMalformed) {
        result.dnf = true;
        if (testPod.eliminated) result.checkpointsMissed = 1;
    }

    bot.kill();
    return result;
}
