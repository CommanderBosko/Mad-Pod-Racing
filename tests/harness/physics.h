#pragma once

#include "types.h"

// Executes one full turn of simulation for a pair of pods (the bot under
// test and the stand-in opponent), sharing the same track/checkpoint
// progression rules. Mutates pod state in place: rotation (clamped to
// +-18 deg/turn, free on the first turn), thrust/boost/shield resolution,
// movement with continuous pod-pod collision detection and elastic
// collision response (mass-weighted, SHIELD triples mass for the turn),
// friction, position rounding / velocity truncation, checkpoint hits, lap
// completion, and the 100-turn elimination timeout.
//
// A pod that has already finished or been eliminated stops receiving
// rotation/thrust commands (it just coasts to a stop under friction) but
// still participates in movement/collision so it doesn't teleport off the
// track from the other pod's perspective.
void simulateTurn(Pod& podA, TurnCommand& cmdA,
                  Pod& podB, TurnCommand& cmdB,
                  const Track& track, int turnNumber);
