#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <algorithm>

using namespace std;

// Store checkpoints once discovered
static vector<array<int, 2>> checkpoints;
static int lastCheckpointX = -1;
static int lastCheckpointY = -1;
static int checkpointIndex = 0;

// Smooth thrust scaling
static int calculateThrust(int angle, int distance) {
    int thrust;
    if (abs(angle) > 90) {
        thrust = 0; // too sharp, slow down
    } else {
        thrust = (int)(100 - (abs(angle) * 0.7));
    }

    // Brake near checkpoint
    if (distance < 1000) {
        thrust = min(thrust, 40);
    }

    return max(thrust, 0);
}

// Boost conditions: straight line with both checkpoints aligned
static bool shouldBoost(int x, int y, const array<int, 2>& nextCP,
                        const array<int, 2>& nextNextCP, int angle, int distance) {
    if (abs(angle) > 5) return false;     // must be mostly straight
    if (distance < 4000) return false;    // not enough runway

    // Vector to next checkpoint
    double dx1 = nextCP[0] - x;
    double dy1 = nextCP[1] - y;

    // Vector from next checkpoint to next-next checkpoint
    double dx2 = nextNextCP[0] - nextCP[0];
    double dy2 = nextNextCP[1] - nextCP[1];

    // Compute angle between vectors
    double dot = dx1 * dx2 + dy1 * dy2;
    double mag1 = sqrt(dx1 * dx1 + dy1 * dy1);
    double mag2 = sqrt(dx2 * dx2 + dy2 * dy2);
    if (mag1 == 0 || mag2 == 0) return false;

    double cosTheta = dot / (mag1 * mag2);
    double angleBetween = acos(cosTheta) * 180 / M_PI;

    // Only boost if the turn after is small
    return angleBetween < 10;
}

int main() {
    int boostCount = 1;

    // game loop
    while (true) {
        // Input parsing
        int x, y, nextCheckpointX, nextCheckpointY, nextDist, nextAngle;
        int opponentX, opponentY;
        cin >> x >> y >> nextCheckpointX >> nextCheckpointY >> nextDist >> nextAngle;
        cin >> opponentX >> opponentY;

        // Remember checkpoints as they appear
        if (nextCheckpointX != lastCheckpointX || nextCheckpointY != lastCheckpointY) {
            checkpoints.push_back({nextCheckpointX, nextCheckpointY});
            lastCheckpointX = nextCheckpointX;
            lastCheckpointY = nextCheckpointY;
            checkpointIndex = (checkpointIndex + 1) % checkpoints.size();
        }

        // Figure out "next-next checkpoint" if available
        array<int, 2> nextCheckpoint = {nextCheckpointX, nextCheckpointY};
        array<int, 2> nextNextCheckpoint = nextCheckpoint; // fallback

        if (checkpoints.size() > 1) {
            int nextIndex = (checkpointIndex + 1) % (int)checkpoints.size();
            nextNextCheckpoint = checkpoints[nextIndex];
        }

        // Compute opponent distance
        double dx = opponentX - x;
        double dy = opponentY - y;
        double opponentDistance = sqrt(dx * dx + dy * dy);

        // Thrust calculation
        int thrust = calculateThrust(nextAngle, nextDist);
        string thrustOutput = to_string(thrust);

        // Boost logic (safe version)
        if (boostCount > 0 && shouldBoost(x, y, nextCheckpoint, nextNextCheckpoint, nextAngle, nextDist)) {
            thrustOutput = "BOOST";
            boostCount--;
        }

        // Shield logic
        if (opponentDistance < 800) {
            thrustOutput = "SHIELD";
        }

        // Targeting: aim between next and next-next
        double weight = 0.2; // bias toward next-next checkpoint
        double targetX = (1 - weight) * nextCheckpoint[0] + weight * nextNextCheckpoint[0];
        double targetY = (1 - weight) * nextCheckpoint[1] + weight * nextNextCheckpoint[1];

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
