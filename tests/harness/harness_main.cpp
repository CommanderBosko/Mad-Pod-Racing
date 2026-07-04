#include "race.h"
#include "tracks.h"
#include "types.h"

#include <csignal>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string turnsLabel(const RaceResult& r) {
    if (r.crashedOrMalformed) return "CRASH";
    if (r.dnf) return "DNF";
    return std::to_string(r.turnsToFinish);
}

// All comparators share one convention: positive => B is better,
// negative => A is better, 0 => tie.

template <typename T>
int compareSmallerIsBetter(T a, T b) {
    if (a == b) return 0;
    return (b < a) ? 1 : -1;
}

template <typename T>
int compareLargerIsBetter(T a, T b) {
    if (a == b) return 0;
    return (b > a) ? 1 : -1;
}

// A DNF/crash is worse than any finite turn count.
int compareTurnsToFinish(const RaceResult& a, const RaceResult& b) {
    bool aBad = a.dnf || a.crashedOrMalformed;
    bool bBad = b.dnf || b.crashedOrMalformed;
    if (aBad && bBad) return 0;
    if (aBad) return 1;
    if (bBad) return -1;
    return compareSmallerIsBetter(a.turnsToFinish, b.turnsToFinish);
}

struct BoostDelta {
    bool applicable = false;
    int turnsSaved = 0; // shadow (no-boost) turns minus normal turns; positive = boost helped
};

BoostDelta computeBoostDelta(const RaceResult& normalRun, const RaceResult& shadowRun) {
    BoostDelta d;
    if (!normalRun.boostUsed) return d;
    if (normalRun.dnf || normalRun.crashedOrMalformed) return d;
    if (shadowRun.dnf || shadowRun.crashedOrMalformed) return d;
    d.applicable = true;
    d.turnsSaved = shadowRun.turnsToFinish - normalRun.turnsToFinish;
    return d;
}

std::string verdictWord(int cmp) {
    if (cmp > 0) return "B better";
    if (cmp < 0) return "A better";
    return "tie";
}

struct Tally {
    int aBetter = 0;
    int bBetter = 0;
    int ties = 0;
    int total = 0;
};

void accumulate(Tally& t, int cmp) {
    t.total++;
    if (cmp > 0) t.bBetter++;
    else if (cmp < 0) t.aBetter++;
    else t.ties++;
}

} // namespace

int main(int argc, char** argv) {
    // Writing to a crashed subprocess's closed stdin pipe raises SIGPIPE,
    // which by default kills this process too -- ignore it so a dead bot
    // is reported as a Crashed status instead of taking the harness down.
    std::signal(SIGPIPE, SIG_IGN);

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <botA_binary> <botB_binary>\n";
        return 1;
    }

    std::string botA = argv[1];
    std::string botB = argv[2];
    std::vector<Track> tracks = getAllTracks();

    Tally overall;

    std::cout << "=== Mad Pod Racing A/B Test Harness ===\n";
    std::cout << "A = " << botA << "\n";
    std::cout << "B = " << botB << "\n\n";

    for (const Track& track : tracks) {
        std::cout << "--- Track: " << track.name << " (" << track.checkpoints.size()
                   << " checkpoints, " << track.laps << " laps) ---\n";

        RaceResult aNormal = runRace(botA, track, false);
        RaceResult aShadow = runRace(botA, track, true);
        RaceResult bNormal = runRace(botB, track, false);
        RaceResult bShadow = runRace(botB, track, true);

        Tally trackTally;

        int cmpTurns = compareTurnsToFinish(aNormal, bNormal);
        accumulate(trackTally, cmpTurns);
        std::cout << "  Turns to finish:      A=" << turnsLabel(aNormal)
                   << "   B=" << turnsLabel(bNormal)
                   << "   (" << verdictWord(cmpTurns) << ")\n";

        int cmpMissed = compareSmallerIsBetter(aNormal.checkpointsMissed, bNormal.checkpointsMissed);
        accumulate(trackTally, cmpMissed);
        std::cout << "  Checkpoints missed:   A=" << aNormal.checkpointsMissed
                   << "   B=" << bNormal.checkpointsMissed
                   << "   (" << verdictWord(cmpMissed) << ")\n";

        int cmpBudget = compareSmallerIsBetter(aNormal.computeBudgetViolations, bNormal.computeBudgetViolations);
        accumulate(trackTally, cmpBudget);
        std::cout << "  Compute budget viol.: A=" << aNormal.computeBudgetViolations
                   << "   B=" << bNormal.computeBudgetViolations
                   << "   (" << verdictWord(cmpBudget) << ")\n";

        BoostDelta aDelta = computeBoostDelta(aNormal, aShadow);
        BoostDelta bDelta = computeBoostDelta(bNormal, bShadow);
        std::cout << "  Boost usage:          A="
                   << (aNormal.boostUsed ? ("turn " + std::to_string(aNormal.boostUsedOnTurn)) : "unused")
                   << "   B="
                   << (bNormal.boostUsed ? ("turn " + std::to_string(bNormal.boostUsedOnTurn)) : "unused") << "\n";
        std::cout << "  Boost effectiveness:  A="
                   << (aDelta.applicable ? (std::to_string(aDelta.turnsSaved) + " turns saved") : "N/A")
                   << "   B="
                   << (bDelta.applicable ? (std::to_string(bDelta.turnsSaved) + " turns saved") : "N/A");
        if (aDelta.applicable && bDelta.applicable) {
            int cmpBoost = compareLargerIsBetter(aDelta.turnsSaved, bDelta.turnsSaved);
            accumulate(trackTally, cmpBoost);
            std::cout << "   (" << verdictWord(cmpBoost) << ")";
        } else {
            std::cout << "   (not compared -- one or both didn't boost/finish)";
        }
        std::cout << "\n";

        std::cout << "  Track verdict: B improved " << trackTally.bBetter << "/" << trackTally.total
                   << ", A improved " << trackTally.aBetter << "/" << trackTally.total
                   << ", ties " << trackTally.ties << "\n\n";

        overall.aBetter += trackTally.aBetter;
        overall.bBetter += trackTally.bBetter;
        overall.ties += trackTally.ties;
        overall.total += trackTally.total;
    }

    std::cout << "=== Overall Verdict ===\n";
    std::cout << "B improved on " << overall.bBetter << "/" << overall.total << " comparable metrics\n";
    std::cout << "A improved on " << overall.aBetter << "/" << overall.total << " comparable metrics\n";
    std::cout << "Ties: " << overall.ties << "/" << overall.total << "\n";

    return 0;
}
