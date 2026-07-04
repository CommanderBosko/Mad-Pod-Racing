// Fixture bot for harness self-verification: sleeps far past any compute
// budget on every turn, used to confirm the harness treats a hung (but
// still-alive) process as unusable without blocking forever.
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    while (true) {
        int x, y, nextX, nextY, dist, angle, oppX, oppY;
        if (!(std::cin >> x >> y >> nextX >> nextY >> dist >> angle)) break;
        if (!(std::cin >> oppX >> oppY)) break;

        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << nextX << " " << nextY << " " << 100 << std::endl;
    }
    return 0;
}
