#include "tracks.h"

// Four best-effort approximations of real Mad Pod Racing (Coders Strike Back)
// track layouts, chosen to exercise distinct aspects of a bot's driving logic.
// All checkpoint centers are kept within roughly x:[1000,15000], y:[1000,8000]
// so the 600-radius checkpoint circle stays clear of the 16000x9000 map edge.

std::vector<Track> getAllTracks() {
    std::vector<Track> tracks;

    // 1. Simple Oval - a basic 4-checkpoint loop with gentle, wide turns.
    //    No sharp corners; this is the easy baseline track.
    {
        Track t;
        t.name = "Simple Oval";
        t.checkpoints = {
            {3000, 4500},
            {8000, 2000},
            {13000, 4500},
            {8000, 7500},
        };
        t.laps = 3;
        tracks.push_back(t);
    }

    // 2. Hairpin - includes a checkpoint sequence where the pod must nearly
    //    reverse direction. Going CP0 -> CP1 heads due east; CP1 -> CP2 heads
    //    almost due west (angle change ~174 degrees), forcing hard braking
    //    and a tight cornering maneuver.
    {
        Track t;
        t.name = "Hairpin";
        t.checkpoints = {
            {2000, 4500},
            {10000, 4500}, // hairpin apex
            {9000, 4600},  // near-reversal from the previous leg
            {2000, 7500},  // closes the loop back toward checkpoint 0
        };
        t.laps = 3;
        tracks.push_back(t);
    }

    // 3. Long Straight - one very long diagonal leg (~14300 units) to give
    //    BOOST a meaningful place to be used, plus a few more checkpoints
    //    to close the loop.
    {
        Track t;
        t.name = "Long Straight";
        t.checkpoints = {
            {1500, 1500},
            {14500, 7500}, // long diagonal leg from checkpoint 0 (~14318 units)
            {7500, 7500},
            {1500, 4500},
        };
        t.laps = 3;
        tracks.push_back(t);
    }

    // 4. Mixed - a more complex 6-checkpoint track with varied leg lengths
    //    and turn angles, meant to represent a realistic "average" track.
    {
        Track t;
        t.name = "Mixed";
        t.checkpoints = {
            {1500, 4500},
            {6000, 1500},
            {11000, 2500},
            {14500, 5000},
            {10000, 7500},
            {4000, 7000},
        };
        t.laps = 3;
        tracks.push_back(t);
    }

    return tracks;
}
