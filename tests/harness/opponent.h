#pragma once

#include "types.h"

// Deliberately dumb stand-in opponent: aims at the next checkpoint and
// throttles thrust down as the angle-to-target widens. No BOOST, no SHIELD.
TurnCommand decideOpponentMove(const Pod& pod, const Track& track);
