// Fixture bot for harness self-verification: exits immediately without
// reading any input, used to confirm the harness detects a crashed
// subprocess (and survives the SIGPIPE from writing to its closed stdin).
int main() {
    return 1;
}
