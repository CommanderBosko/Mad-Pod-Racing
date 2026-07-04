// Fixture bot for harness self-verification: like good_bot, but spends its
// one BOOST the first time it's pointed nearly straight at a checkpoint
// more than 5000 units away -- used to confirm the shadow no-boost
// baseline and boost-effectiveness measurement work end to end.
#include <algorithm>
#include <cmath>
#include <iostream>

int main() {
    bool boosted = false;
    while (true) {
        int x, y, nextX, nextY, dist, angle, oppX, oppY;
        if (!(std::cin >> x >> y >> nextX >> nextY >> dist >> angle)) break;
        if (!(std::cin >> oppX >> oppY)) break;

        if (!boosted && std::abs(angle) < 5 && dist > 5000) {
            boosted = true;
            std::cout << nextX << " " << nextY << " BOOST" << std::endl;
            continue;
        }

        int thrust = (std::abs(angle) > 90) ? 0 : static_cast<int>(100 - std::abs(angle) * 0.7);
        thrust = std::max(0, std::min(100, thrust));
        std::cout << nextX << " " << nextY << " " << thrust << std::endl;
    }
    return 0;
}
