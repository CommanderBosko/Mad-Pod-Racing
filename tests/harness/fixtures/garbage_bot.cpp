// Fixture bot for harness self-verification: reads input correctly but
// prints unparseable garbage instead of a valid command, used to confirm
// the harness detects malformed output.
#include <iostream>

int main() {
    while (true) {
        int x, y, nextX, nextY, dist, angle, oppX, oppY;
        if (!(std::cin >> x >> y >> nextX >> nextY >> dist >> angle)) break;
        if (!(std::cin >> oppX >> oppY)) break;

        std::cout << "banana banana banana" << std::endl;
    }
    return 0;
}
