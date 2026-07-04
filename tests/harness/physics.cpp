#include "physics.h"

#include <algorithm>
#include <cmath>

namespace {

double normalizeAngle(double a) {
    while (a > 180.0) a -= 360.0;
    while (a <= -180.0) a += 360.0;
    return a;
}

double angleTo(double fromX, double fromY, double toX, double toY) {
    return std::atan2(toY - fromY, toX - fromX) * 180.0 / M_PI;
}

void rotatePod(Pod& pod, double targetX, double targetY) {
    double desired = angleTo(pod.x, pod.y, targetX, targetY);
    if (!pod.angleInitialized) {
        pod.angle = desired;
        pod.angleInitialized = true;
        return;
    }
    double diff = normalizeAngle(desired - pod.angle);
    diff = std::max(-MAX_TURN_DEGREES, std::min(MAX_TURN_DEGREES, diff));
    pod.angle = normalizeAngle(pod.angle + diff);
}

// Resolves boost/shield/thrust for one pod and applies acceleration along
// its (already rotated) facing. Shield zeroes thrust for the turn, triples
// mass for any collision this turn, and starts a 3-turn cooldown. A repeat
// BOOST request after the one-shot has been spent falls back to max thrust,
// matching CodinGame's observed behavior.
void applyThrust(Pod& pod, TurnCommand& cmd, int turnNumber) {
    pod.mass = 1.0;
    bool shieldActivatedThisTurn = false;

    if (cmd.shield && pod.shieldCooldownRemaining <= 0) {
        pod.mass = 3.0;
        shieldActivatedThisTurn = true;
    }

    // Per the official rule, engines stay inactive for the 3 turns following a
    // SHIELD activation -- not just a re-activation cooldown. A pod already in
    // that window gets no thrust at all this turn, regardless of what it asks for.
    bool enginesLockedOut = pod.shieldCooldownRemaining > 0;

    if (pod.shieldCooldownRemaining > 0) {
        pod.shieldCooldownRemaining--;
    }
    if (shieldActivatedThisTurn) {
        pod.shieldCooldownRemaining = SHIELD_COOLDOWN_TURNS;
        return; // no thrust applied on the turn shield is raised
    }
    if (enginesLockedOut) {
        return; // engines inactive: no thrust, no boost, while cooling down
    }

    double thrustMagnitude;
    if (cmd.boost) {
        if (pod.boostAvailable) {
            thrustMagnitude = BOOST_THRUST;
            pod.boostAvailable = false;
            pod.boostUsedOnTurn = turnNumber;
        } else {
            thrustMagnitude = static_cast<double>(MAX_THRUST);
        }
    } else {
        int t = std::max(0, std::min(MAX_THRUST, cmd.thrust));
        thrustMagnitude = static_cast<double>(t);
    }

    double rad = pod.angle * M_PI / 180.0;
    pod.vx += std::cos(rad) * thrustMagnitude;
    pod.vy += std::sin(rad) * thrustMagnitude;
}

struct CollisionEvent {
    bool found;
    double t;
};

// Earliest time in [0, maxT] at which the two pods (moving at their current,
// constant velocities) would be exactly touching (center distance == sum of
// radii). Already-overlapping pods collide immediately at t=0, but only if
// they are still approaching -- pods left touching by a prior bounce this
// same turn are separating (or neutral), and re-triggering a "collision"
// against them every sub-step (at t=0, so position never advances) would
// freeze both pods in place forever instead of letting them coast apart.
CollisionEvent findCollisionTime(const Pod& a, const Pod& b, double maxT) {
    double dpx = b.x - a.x, dpy = b.y - a.y;
    double dvx = b.vx - a.vx, dvy = b.vy - a.vy;
    double sumR = POD_RADIUS * 2.0;

    double distNow2 = dpx * dpx + dpy * dpy;
    double approachRate = dpx * dvx + dpy * dvy; // < 0 means closing
    if (distNow2 <= sumR * sumR) {
        if (approachRate < 0.0) return {true, 0.0};
        return {false, 0.0};
    }

    double A = dvx * dvx + dvy * dvy;
    if (A < 1e-9) return {false, 0.0};

    double B = 2.0 * (dpx * dvx + dpy * dvy);
    double C = distNow2 - sumR * sumR; // > 0 here

    double disc = B * B - 4 * A * C;
    if (disc < 0) return {false, 0.0};

    double sqrtDisc = std::sqrt(disc);
    double t = (-B - sqrtDisc) / (2 * A); // earliest approach root
    if (t < 0 || t > maxT) return {false, 0.0};
    return {true, t};
}

// Perfectly elastic collision along the contact normal (tangential
// component untouched, matching a frictionless bounce). Mass reflects
// SHIELD's x3 multiplier for that turn. CodinGame's real physics enforces a
// minimum impulse magnitude of 120 so glancing touches still produce a
// noticeable bounce; replicated here for fidelity.
void resolveCollision(Pod& a, Pod& b) {
    double nx = b.x - a.x, ny = b.y - a.y;
    double dist = std::sqrt(nx * nx + ny * ny);
    if (dist < 1e-6) {
        nx = 1.0;
        ny = 0.0;
    } else {
        nx /= dist;
        ny /= dist;
    }

    double v1n = a.vx * nx + a.vy * ny;
    double v2n = b.vx * nx + b.vy * ny;

    double m1 = a.mass, m2 = b.mass;
    double v1nAfter = ((m1 - m2) * v1n + 2 * m2 * v2n) / (m1 + m2);
    double v2nAfter = ((m2 - m1) * v2n + 2 * m1 * v1n) / (m1 + m2);

    double impulse1 = m1 * (v1nAfter - v1n);
    double impulse2 = m2 * (v2nAfter - v2n);

    constexpr double MIN_IMPULSE = 120.0;
    double impulseMag = std::fabs(impulse1);
    if (impulseMag > 1e-9 && impulseMag < MIN_IMPULSE) {
        double sign = impulse1 < 0 ? -1.0 : 1.0;
        impulse1 = sign * MIN_IMPULSE;
        impulse2 = -impulse1;
    }

    v1nAfter = v1n + impulse1 / m1;
    v2nAfter = v2n + impulse2 / m2;

    a.vx += (v1nAfter - v1n) * nx;
    a.vy += (v1nAfter - v1n) * ny;
    b.vx += (v2nAfter - v2n) * nx;
    b.vy += (v2nAfter - v2n) * ny;
}

// Advances both pods through the full 1.0-turn timestep, stopping to
// resolve a collision (and continuing with the post-bounce velocities)
// whenever one occurs before the remaining time is exhausted.
void moveWithCollisions(Pod& a, Pod& b) {
    double remaining = 1.0;
    int guard = 0;
    while (remaining > 1e-9 && guard < 6) {
        guard++;
        CollisionEvent ev = findCollisionTime(a, b, remaining);
        double t = ev.found ? ev.t : remaining;

        a.x += a.vx * t;
        a.y += a.vy * t;
        b.x += b.vx * t;
        b.y += b.vy * t;
        remaining -= t;

        if (ev.found) {
            resolveCollision(a, b);
        }
    }
}

void applyFrictionAndTruncate(Pod& pod) {
    pod.vx *= FRICTION;
    pod.vy *= FRICTION;
    pod.x = std::round(pod.x);
    pod.y = std::round(pod.y);
    pod.vx = std::trunc(pod.vx);
    pod.vy = std::trunc(pod.vy);
}

void checkCheckpointAndElimination(Pod& pod, const Track& track, int turnNumber) {
    pod.turnsSinceLastCheckpoint++;

    const auto& cps = track.checkpoints;
    int idx = pod.nextCheckpoint;
    double dx = cps[idx].x - pod.x;
    double dy = cps[idx].y - pod.y;
    double dist = std::sqrt(dx * dx + dy * dy);

    if (dist <= CHECKPOINT_RADIUS) {
        pod.turnsSinceLastCheckpoint = 0;
        if (idx == 0) {
            pod.lapsCompleted++;
            if (pod.lapsCompleted >= track.laps) {
                pod.finished = true;
                pod.finishedOnTurn = turnNumber;
            }
        }
        pod.nextCheckpoint = (idx + 1) % static_cast<int>(cps.size());
    }

    if (!pod.finished && pod.turnsSinceLastCheckpoint > ELIMINATION_TIMEOUT_TURNS) {
        pod.eliminated = true;
    }
}

} // namespace

void simulateTurn(Pod& podA, TurnCommand& cmdA,
                   Pod& podB, TurnCommand& cmdB,
                   const Track& track, int turnNumber) {
    bool activeA = !podA.finished && !podA.eliminated;
    bool activeB = !podB.finished && !podB.eliminated;

    if (activeA) rotatePod(podA, cmdA.targetX, cmdA.targetY);
    if (activeB) rotatePod(podB, cmdB.targetX, cmdB.targetY);

    if (activeA) applyThrust(podA, cmdA, turnNumber);
    if (activeB) applyThrust(podB, cmdB, turnNumber);

    moveWithCollisions(podA, podB);

    applyFrictionAndTruncate(podA);
    applyFrictionAndTruncate(podB);

    if (activeA) checkCheckpointAndElimination(podA, track, turnNumber);
    if (activeB) checkCheckpointAndElimination(podB, track, turnNumber);
}
