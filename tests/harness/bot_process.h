#pragma once

#include "types.h"

#include <string>
#include <sys/types.h>

// Runs an arbitrary compiled bot binary as a black-box subprocess and speaks
// the Wood-league CodinGame protocol to it over pipes: one line of
// "x y nextCheckpointX nextCheckpointY nextCheckpointDist nextCheckpointAngle"
// plus a line of "opponentX opponentY" per turn in, one line of
// "targetX targetY thrust" (thrust/BOOST/SHIELD) out.
class BotProcess {
public:
    explicit BotProcess(const std::string& executablePath);
    ~BotProcess();

    BotProcess(const BotProcess&) = delete;
    BotProcess& operator=(const BotProcess&) = delete;

    // Forks and execs the binary. Returns false if the pipes/fork setup
    // itself failed (not for a bad executable path -- that surfaces as a
    // Crashed status on the first takeTurn call, since exec failure happens
    // in the child after fork returns successfully in the parent).
    bool start();

    // Sends one turn of input and waits (up to a generous multiple of the
    // real per-turn compute budget) for one response line. Always returns a
    // usable command -- on Crashed/TimedOut/MalformedOutput the command is a
    // safe fallback (hold position, zero thrust) so callers don't need to
    // special-case every failure path before continuing to simulate.
    // Once a process has crashed or timed out, it is considered dead and
    // every subsequent call returns Crashed without touching the pipes.
    ControllerTurnResult takeTurn(int turnNumber,
                                  double x, double y,
                                  int nextCheckpointX, int nextCheckpointY,
                                  int nextCheckpointDist, int nextCheckpointAngle,
                                  double opponentX, double opponentY);

    void kill();

private:
    std::string executablePath_;
    pid_t pid_ = -1;
    int stdinFd_ = -1;
    int stdoutFd_ = -1;
    bool alive_ = false;
    bool deadOrHung_ = false;
    std::string lineBuffer_;
};
