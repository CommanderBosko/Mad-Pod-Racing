#include "opponent.h"

#include <cmath>
#include <algorithm>

namespace {

// Normalize a degree value into (-180, 180].
double normalizeAngle(double degrees) {
    double a = std::fmod(degrees + 180.0, 360.0);
    if (a <= 0.0) {
        a += 360.0;
    }
    return a - 180.0;
}

} // namespace

TurnCommand decideOpponentMove(const Pod& pod, const Track& track) {
    const Checkpoint& target = track.checkpoints[pod.nextCheckpoint];

    TurnCommand cmd;
    cmd.targetX = static_cast<double>(target.x);
    cmd.targetY = static_cast<double>(target.y);
    cmd.boost = false;
    cmd.shield = false;

    double angleDiff = 0.0;
    if (pod.angleInitialized) {
        double dx = cmd.targetX - pod.x;
        double dy = cmd.targetY - pod.y;
        double angleToTarget = std::atan2(dy, dx) * 180.0 / M_PI;
        angleDiff = normalizeAngle(angleToTarget - pod.angle);
    }

    double absDiff = std::fabs(angleDiff);

    // Simple, deliberately unsophisticated thrust scaling: full thrust when
    // roughly facing the target, tapering to zero as the pod would need to
    // turn sharply (beyond ~90 degrees it just rotates in place this turn).
    constexpr double FULL_THRUST_ANGLE = 15.0;
    constexpr double ZERO_THRUST_ANGLE = 90.0;

    double thrustD;
    if (absDiff <= FULL_THRUST_ANGLE) {
        thrustD = 100.0;
    } else if (absDiff >= ZERO_THRUST_ANGLE) {
        thrustD = 0.0;
    } else {
        thrustD = 100.0 * (ZERO_THRUST_ANGLE - absDiff) /
                  (ZERO_THRUST_ANGLE - FULL_THRUST_ANGLE);
    }

    cmd.thrust = std::clamp(static_cast<int>(std::lround(thrustD)), 0, 100);

    return cmd;
}
