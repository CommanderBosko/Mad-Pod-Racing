#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <algorithm>

using namespace std;

// --- Tunable heuristic constants (edited by the /tune-bot loop; see
// .claude/skills/tune-bot). SHIELD's 3-turn cooldown is NOT here since it's
// a fixed game rule (engine lockout duration), not a heuristic choice.
static const int    THRUST_ZERO_ANGLE_DEG         = 92;     // angle beyond which thrust cuts to 0
static const double THRUST_ANGLE_FACTOR           = 0.7;    // thrust falloff per degree of angle
static const double BRAKING_ZONE_MIN              = 1000.0; // min braking-zone radius
static const double BRAKING_ZONE_SPEED_MULT       = 3.0;    // braking-zone radius per unit speed
static const int    BRAKING_THRUST_CAP            = 40;     // max thrust once inside braking zone
static const int    BOOST_ANGLE_MAX_DEG           = 10;     // max angle to allow a boost
static const int    BOOST_DIST_MIN                = 4000;   // min distance-to-checkpoint to allow a boost
static const double SHIELD_DIST_TRIGGER           = 855.0;  // opponent proximity that arms the shield check
static const double SHIELD_PREDICTED_DIST_TRIGGER = 800.0;  // predicted 1-turn-ahead distance that fires shield
static const double TARGET_BLEND_MAX_WEIGHT       = 0.22;    // max lookahead blend toward next-next checkpoint
static const double TARGET_BLEND_DIST_NORM        = 3000.0; // distance at which lookahead blend saturates
static const double DRIFT_CORRECTION_FACTOR       = 1.0;    // multiplier on velocity subtracted from aim point
// --- end tunable constants ---

// Store checkpoints once discovered. Discovery stops the moment the target
// cycles back to the very first checkpoint seen -- after that, checkpointIndex
// is re-derived every turn by matching the game's current target against this
// list, so lap 2+ never desyncs (the list itself never changes size again,
// unlike blindly appending on every "target changed" turn, which would push a
// duplicate entry -- and shift what index means -- every single lap).
static vector<array<int, 2>> checkpoints;
static bool discoveryComplete = false;
static int lastCheckpointX = -1;
static int lastCheckpointY = -1;

// Smooth thrust scaling
static int calculateThrust(int angle, int distance, double speed) {
    int thrust;
    if (abs(angle) > THRUST_ZERO_ANGLE_DEG) {
        thrust = 0; // too sharp, slow down
    } else {
        thrust = (int)(100 - (abs(angle) * THRUST_ANGLE_FACTOR));
    }

    // Brake near checkpoint -- scale the braking zone with current speed so a
    // fast-moving pod starts slowing earlier instead of overshooting (a fixed
    // 1000-unit zone is fine standing still but too late at high speed).
    double brakingZone = max(BRAKING_ZONE_MIN, speed * BRAKING_ZONE_SPEED_MULT);
    if (distance < brakingZone) {
        thrust = min(thrust, BRAKING_THRUST_CAP);
    }

    return max(thrust, 0);
}

int main() {
    int boostCount = 1;

    // Self-tracked SHIELD cooldown: the protocol never tells us whether our
    // last SHIELD is still locking out our engines, so we have to count it
    // ourselves (mirrors the engine's real 3-turn lockout).
    int shieldCooldownRemaining = 0;

    // Previous-turn positions, used to estimate our own and the opponent's
    // velocity (the protocol gives neither directly) so SHIELD can react to an
    // actually-imminent collision instead of mere proximity.
    bool havePrevPositions = false;
    int lastX = 0, lastY = 0, lastOppX = 0, lastOppY = 0;

    // game loop
    while (true) {
        // Input parsing
        int x, y, nextCheckpointX, nextCheckpointY, nextDist, nextAngle;
        int opponentX, opponentY;
        cin >> x >> y >> nextCheckpointX >> nextCheckpointY >> nextDist >> nextAngle;
        cin >> opponentX >> opponentY;

        // Remember checkpoints as they appear, stopping once we've cycled
        // back to the first one ever seen (a completed discovery lap).
        if (nextCheckpointX != lastCheckpointX || nextCheckpointY != lastCheckpointY) {
            if (!discoveryComplete) {
                bool isRepeatOfFirst = !checkpoints.empty() &&
                    checkpoints[0][0] == nextCheckpointX && checkpoints[0][1] == nextCheckpointY;
                if (isRepeatOfFirst && checkpoints.size() > 1) {
                    discoveryComplete = true;
                } else {
                    checkpoints.push_back({nextCheckpointX, nextCheckpointY});
                }
            }
            lastCheckpointX = nextCheckpointX;
            lastCheckpointY = nextCheckpointY;
        }

        // Re-derive which known checkpoint we're currently targeting by
        // coordinate match every turn -- robust across any number of laps,
        // unlike incrementing a counter that can drift out of sync.
        int checkpointIndex = 0;
        for (size_t i = 0; i < checkpoints.size(); i++) {
            if (checkpoints[i][0] == nextCheckpointX && checkpoints[i][1] == nextCheckpointY) {
                checkpointIndex = (int)i;
                break;
            }
        }

        // Figure out "next-next checkpoint" if available. Only trust the
        // cyclic lookahead once discovery has completed -- during the first
        // lap the list is still partial, so wrapping via modulo would point
        // backward at an already-discovered (already-passed) checkpoint
        // instead of the genuinely upcoming one.
        array<int, 2> nextCheckpoint = {nextCheckpointX, nextCheckpointY};
        array<int, 2> nextNextCheckpoint = nextCheckpoint; // fallback

        if (discoveryComplete && checkpoints.size() > 1) {
            int nextIndex = (checkpointIndex + 1) % (int)checkpoints.size();
            nextNextCheckpoint = checkpoints[nextIndex];
        }

        // Own velocity estimate (the protocol gives neither pod's velocity
        // directly), reused for speed-aware braking, SHIELD collision
        // prediction, and drift-compensated steering below.
        double myVx = havePrevPositions ? (x - lastX) : 0.0;
        double myVy = havePrevPositions ? (y - lastY) : 0.0;
        double mySpeed = sqrt(myVx * myVx + myVy * myVy);

        // Compute opponent distance
        double dx = opponentX - x;
        double dy = opponentY - y;
        double opponentDistance = sqrt(dx * dx + dy * dy);

        // Thrust calculation
        int thrust = calculateThrust(nextAngle, nextDist, mySpeed);
        string thrustOutput = to_string(thrust);

        // Boost logic: only on the single longest leg of the whole track
        // (known once discovery completes), not just the first straight that
        // happens to qualify -- there's only one boost per race, so spend it
        // where it buys the most, and a 3-lap race guarantees we pass the
        // longest leg again even if we skip it during the discovery lap.
        bool onLongestLeg = false;
        if (discoveryComplete && checkpoints.size() > 1) {
            int n = (int)checkpoints.size();
            double bestLen = -1;
            int bestLeg = 0;
            for (int k = 0; k < n; k++) {
                double lx = checkpoints[(k + 1) % n][0] - checkpoints[k][0];
                double ly = checkpoints[(k + 1) % n][1] - checkpoints[k][1];
                double len = sqrt(lx * lx + ly * ly);
                if (len > bestLen) {
                    bestLen = len;
                    bestLeg = k;
                }
            }
            int currentLeg = (checkpointIndex - 1 + n) % n;
            onLongestLeg = (currentLeg == bestLeg);
        }

        // onLongestLeg already identifies the single best straight on the
        // whole course, so we don't also need shouldBoost's "small turn
        // after" veto -- that check exists to avoid boosting into a sharp
        // corner on an arbitrary straight, but every real track bends
        // somewhere after its longest leg, and that veto alone was enough to
        // disqualify boosting here entirely. Alignment right now is still
        // required (small angle, real distance left to cover).
        if (boostCount > 0 && onLongestLeg && abs(nextAngle) <= BOOST_ANGLE_MAX_DEG && nextDist > BOOST_DIST_MIN) {
            thrustOutput = "BOOST";
            boostCount--;
        }

        // Shield logic: react to an actually-imminent collision, not mere
        // proximity. Estimating both pods' velocity by finite difference and
        // extrapolating one turn ahead avoids the old trap where staying near
        // a slow-moving opponent (not on a collision course) triggered SHIELD
        // every turn forever, permanently locking out our own engines.
        if (shieldCooldownRemaining > 0) {
            shieldCooldownRemaining--;
        } else if (havePrevPositions) {
            double oppVx = opponentX - lastOppX, oppVy = opponentY - lastOppY;
            double predictedDx = (opponentX + oppVx) - (x + myVx);
            double predictedDy = (opponentY + oppVy) - (y + myVy);
            double predictedDist = sqrt(predictedDx * predictedDx + predictedDy * predictedDy);

            if (opponentDistance < SHIELD_DIST_TRIGGER && predictedDist < SHIELD_PREDICTED_DIST_TRIGGER) {
                thrustOutput = "SHIELD";
                shieldCooldownRemaining = 3;
            }
        }

        lastX = x;
        lastY = y;
        lastOppX = opponentX;
        lastOppY = opponentY;
        havePrevPositions = true;

        // Targeting: aim between next and next-next, but taper the blend to
        // zero as we close in. A fixed blend keeps aiming at a point between
        // the two checkpoints even at close range, which can sit outside the
        // current checkpoint's 600-unit capture radius entirely -- the pod
        // then orbits just past it forever without ever registering a hit.
        // Full lookahead is only safe (and useful, for smoother cornering)
        // while there's still real distance to close.
        double weight = TARGET_BLEND_MAX_WEIGHT * min(1.0, nextDist / TARGET_BLEND_DIST_NORM);
        double targetX = (1 - weight) * nextCheckpoint[0] + weight * nextNextCheckpoint[0];
        double targetY = (1 - weight) * nextCheckpoint[1] + weight * nextNextCheckpoint[1];

        // Counter-steer against drift: subtract our own estimated velocity
        // from the aim point so the pod corrects toward the checkpoint
        // instead of just chasing it nose-first, which cuts down on the
        // wide, momentum-driven arcs a naive "aim straight at target" bot
        // carves through corners.
        targetX -= DRIFT_CORRECTION_FACTOR * myVx;
        targetY -= DRIFT_CORRECTION_FACTOR * myVy;

        // Debug info
        cerr << "Boosts left: " << boostCount << endl;
        cerr << "Angle: " << nextAngle << endl;
        cerr << "Distance: " << nextDist << endl;
        cerr << "Next CP: [" << nextCheckpoint[0] << ", " << nextCheckpoint[1] << "]" << endl;
        cerr << "Next-Next CP: [" << nextNextCheckpoint[0] << ", " << nextNextCheckpoint[1] << "]" << endl;
        cerr << "Thrust: " << thrustOutput << endl;

        // Output move
        cout << (int)targetX << " " << (int)targetY << " " << thrustOutput << endl;
    }

    return 0;
}
