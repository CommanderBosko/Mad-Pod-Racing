#pragma once

#include "types.h"

#include <string>

// Runs one full race on `track` between the bot-under-test binary at
// `botExecutablePath` (driven via the Wood-league subprocess protocol) and
// the in-process stand-in opponent. If `suppressBoost` is true, any BOOST
// command the bot emits is intercepted and replaced with max thrust (100)
// before physics ever sees it -- this produces the shadow no-boost baseline
// used to measure boost effectiveness.
RaceResult runRace(const std::string& botExecutablePath, const Track& track, bool suppressBoost);
