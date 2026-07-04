// Fixture bot for harness self-verification: never moves (thrust 0 forever),
// used to confirm the harness correctly detects the 100-turn elimination
// timeout and reports a DNF.
#include <iostream>

int main() {
    while (true) {
        int x, y, nextX, nextY, dist, angle, oppX, oppY;
        if (!(std::cin >> x >> y >> nextX >> nextY >> dist >> angle)) break;
        if (!(std::cin >> oppX >> oppY)) break;

        std::cout << x << " " << y << " " << 0 << std::endl;
    }
    return 0;
}
