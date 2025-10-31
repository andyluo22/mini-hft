#include <cstdint>
#include <limits>
#include "../engine/spsc/spsc_ring.hpp"

int main() {
    SpscRing<int> q(1024);

    // Fill half
    for (int i = 0; i < 512; ++i) {
        if (!q.try_push(i)) return 1;
    }

    // Pop a bit (and verify values)
    for (int i = 0; i < 128; ++i) {
        int x;
        if (!q.try_pop(x)) return 2;
        if (x != i) return 3;
    }

    // Force wraparound by pushing more than capacity.
    // When full, drain up to 256 items, ensure at least some progress, then continue.
    for (int i = 512; i < 1800; ++i) {
        if (!q.try_push(i)) {
            int x;
            int popped = 0;
            while (popped < 256 && q.try_pop(x)) ++popped;
            if (popped == 0) return 4;        // no progress? ring misbehaving
            if (!q.try_push(i)) return 5;     // should have space now
        }
    }

    // Drain all and check monotone order (FIFO semantics)
    int last = std::numeric_limits<int>::min();
    int x;
    while (q.try_pop(x)) {
        if (!(x > last)) return 6;
        last = x;
    }
    return 0;
}
