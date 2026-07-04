// Fixture bot for harness self-verification: always answers correctly but
// takes 100ms per turn (over the ~75ms later-turn budget, under the
// harness's hard timeout ceiling), used to confirm compute-budget
// violations are flagged without the race being aborted.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

int main() {
    while (true) {
        int x, y, nextX, nextY, dist, angle, oppX, oppY;
        if (!(std::cin >> x >> y >> nextX >> nextY >> dist >> angle)) break;
        if (!(std::cin >> oppX >> oppY)) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int thrust = (std::abs(angle) > 90) ? 0 : static_cast<int>(100 - std::abs(angle) * 0.7);
        thrust = std::max(0, std::min(100, thrust));
        std::cout << nextX << " " << nextY << " " << thrust << std::endl;
    }
    return 0;
}
