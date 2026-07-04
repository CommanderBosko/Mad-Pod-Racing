// Fixture bot for harness self-verification: a simple, reliable racer with
// no BOOST/SHIELD logic, used to confirm the harness correctly detects
// finishes, lap counting, and checkpoint hits when a bot behaves well.
#include <algorithm>
#include <cmath>
#include <iostream>

int main() {
    while (true) {
        int x, y, nextX, nextY, dist, angle, oppX, oppY;
        if (!(std::cin >> x >> y >> nextX >> nextY >> dist >> angle)) break;
        if (!(std::cin >> oppX >> oppY)) break;

        int thrust = (std::abs(angle) > 90) ? 0 : static_cast<int>(100 - std::abs(angle) * 0.7);
        thrust = std::max(0, std::min(100, thrust));

        std::cout << nextX << " " << nextY << " " << thrust << std::endl;
    }
    return 0;
}
