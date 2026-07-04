#include "bot_process.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using Clock = std::chrono::steady_clock;

long long elapsedMs(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

ControllerTurnResult fallback(double holdX, double holdY, ControllerStatus status) {
    ControllerTurnResult r;
    r.command.targetX = holdX;
    r.command.targetY = holdY;
    r.command.thrust = 0;
    r.status = status;
    return r;
}
} // namespace

BotProcess::BotProcess(const std::string& executablePath) : executablePath_(executablePath) {}

BotProcess::~BotProcess() { kill(); }

bool BotProcess::start() {
    int inPipe[2];
    int outPipe[2];
    if (pipe(inPipe) != 0) return false;
    if (pipe(outPipe) != 0) {
        close(inPipe[0]);
        close(inPipe[1]);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        return false;
    }

    if (pid == 0) {
        // Child: wire pipes to stdin/stdout, silence stderr, exec the bot.
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);

        int devNull = open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            dup2(devNull, STDERR_FILENO);
            close(devNull);
        }

        close(inPipe[0]);
        close(inPipe[1]);
        close(outPipe[0]);
        close(outPipe[1]);

        execl(executablePath_.c_str(), executablePath_.c_str(), (char*)nullptr);
        _exit(127); // exec failed
    }

    // Parent
    close(inPipe[0]);
    close(outPipe[1]);
    stdinFd_ = inPipe[1];
    stdoutFd_ = outPipe[0];
    pid_ = pid;
    alive_ = true;
    deadOrHung_ = false;
    return true;
}

void BotProcess::kill() {
    if (pid_ > 0) {
        ::kill(pid_, SIGKILL);
        int status;
        waitpid(pid_, &status, 0);
        pid_ = -1;
    }
    if (stdinFd_ >= 0) { close(stdinFd_); stdinFd_ = -1; }
    if (stdoutFd_ >= 0) { close(stdoutFd_); stdoutFd_ = -1; }
    alive_ = false;
}

ControllerTurnResult BotProcess::takeTurn(int turnNumber,
                                          double x, double y,
                                          int nextCheckpointX, int nextCheckpointY,
                                          int nextCheckpointDist, int nextCheckpointAngle,
                                          double opponentX, double opponentY) {
    if (!alive_ || deadOrHung_) {
        return fallback(x, y, ControllerStatus::Crashed);
    }

    int status;
    pid_t reaped = waitpid(pid_, &status, WNOHANG);
    if (reaped == pid_) {
        alive_ = false;
        deadOrHung_ = true;
        return fallback(x, y, ControllerStatus::Crashed);
    }

    std::ostringstream oss;
    oss << static_cast<int>(std::llround(x)) << ' ' << static_cast<int>(std::llround(y)) << ' '
        << nextCheckpointX << ' ' << nextCheckpointY << ' '
        << nextCheckpointDist << ' ' << nextCheckpointAngle << '\n'
        << static_cast<int>(std::llround(opponentX)) << ' ' << static_cast<int>(std::llround(opponentY)) << '\n';
    std::string input = oss.str();

    ssize_t written = write(stdinFd_, input.data(), input.size());
    if (written < 0 || static_cast<size_t>(written) != input.size()) {
        deadOrHung_ = true;
        return fallback(x, y, ControllerStatus::Crashed);
    }

    long long budget = (turnNumber == 1) ? FIRST_TURN_BUDGET_MS : TURN_BUDGET_MS;
    long long hardCeilingMs = budget * 4 + 200; // slack before we give up on an alive-but-slow process

    auto startTime = Clock::now();
    std::string line;
    bool gotLine = false;
    bool sawEof = false;

    while (true) {
        size_t nl = lineBuffer_.find('\n');
        if (nl != std::string::npos) {
            line = lineBuffer_.substr(0, nl);
            lineBuffer_.erase(0, nl + 1);
            gotLine = true;
            break;
        }

        long long remaining = hardCeilingMs - elapsedMs(startTime);
        if (remaining <= 0) break;

        struct pollfd pfd;
        pfd.fd = stdoutFd_;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, static_cast<int>(remaining));
        if (pr <= 0) continue; // loop re-checks the deadline

        if (pfd.revents & POLLIN) {
            char buf[4096];
            ssize_t n = read(stdoutFd_, buf, sizeof(buf));
            if (n > 0) {
                lineBuffer_.append(buf, static_cast<size_t>(n));
                continue;
            }
            sawEof = true;
        } else if (pfd.revents & (POLLHUP | POLLERR)) {
            sawEof = true;
        }
        if (sawEof) break;
    }

    long long decisionTimeMs = elapsedMs(startTime);

    if (!gotLine) {
        int status2;
        pid_t reaped2 = waitpid(pid_, &status2, WNOHANG);
        ControllerTurnResult r = fallback(x, y,
            reaped2 == pid_ ? ControllerStatus::Crashed : ControllerStatus::TimedOut);
        r.decisionTimeMs = decisionTimeMs;
        r.overBudget = decisionTimeMs > budget;
        alive_ = (reaped2 != pid_);
        deadOrHung_ = true; // either crashed or hung -- treat as dead going forward
        return r;
    }

    std::istringstream iss(line);
    double tx, ty;
    std::string thrustToken;
    if (!(iss >> tx >> ty >> thrustToken)) {
        ControllerTurnResult r = fallback(x, y, ControllerStatus::MalformedOutput);
        r.decisionTimeMs = decisionTimeMs;
        r.overBudget = decisionTimeMs > budget;
        return r;
    }

    ControllerTurnResult r;
    r.command.targetX = tx;
    r.command.targetY = ty;
    r.decisionTimeMs = decisionTimeMs;
    r.overBudget = decisionTimeMs > budget;

    if (thrustToken == "BOOST") {
        r.command.boost = true;
    } else if (thrustToken == "SHIELD") {
        r.command.shield = true;
    } else {
        try {
            size_t consumed = 0;
            int t = std::stoi(thrustToken, &consumed);
            if (consumed != thrustToken.size()) throw std::invalid_argument("trailing junk");
            r.command.thrust = t;
        } catch (...) {
            r.status = ControllerStatus::MalformedOutput;
            r.command = TurnCommand{};
            r.command.targetX = x;
            r.command.targetY = y;
            return r;
        }
    }

    r.status = ControllerStatus::Ok;
    return r;
}
