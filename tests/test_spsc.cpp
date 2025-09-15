#include <cassert>
#include <vector>
#include <cstdint>
#include "../engine/spsc/spsc_ring.hpp"

int main() {
    SpscRing<int> q(1024);
    // Fill half
    for (int i = 0; i < 512; ++i) {
        assert(q.try_push(i));
    }
    // Pop a bit
    for (int i = 0; i < 128; ++i) {
        int x; assert(q.try_pop(x)); assert(x == i);
    }
    // Force wraparound by pushing more than capacity
    for (int i = 512; i < 1800; ++i) {
        bool ok = q.try_push(i);
        if (!ok) { // ring full, drain some then continue
            int x; for (int k = 0; k < 256; ++k) { assert(q.try_pop(x)); }
            assert(q.try_push(i));
        }
    }
    // Drain all and check monotone order
    int last = -1, x;
    while (q.try_pop(x)) {
        assert(x > last);
        last = x;
    }
    return 0;
}
