#pragma once
#include <chrono>
#include <cstdint>

namespace tb {

// Monotonic, steady timestamps (nanoseconds)
inline uint64_t now_ns() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               clock::now().time_since_epoch()).count();
}

inline uint64_t now_us() { return now_ns() / 1000ULL; }
inline uint64_t now_ms() { return now_ns() / 1000000ULL; }

struct Stopwatch {
    uint64_t start_ns = now_ns();
    void   reset() { start_ns = now_ns(); }
    double elapsed_sec() const {
        return double(now_ns() - start_ns) / 1e9;
    }
};

} // namespace tb