#include "ring.hpp"
int main (int argc, char *argv[]) {
    S_LOGGER_SETUP;
    auto ring = MpscRing<int>::of(10);
    ring->enqueue(1);
    int i = 0xdd;
    ring->dequeue(i);
    if (i == 1) {
        return 0;
    } else {
        S_ERROR("i={}", i);
        return 1;
    }
}
