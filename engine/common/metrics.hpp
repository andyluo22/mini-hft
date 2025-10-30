// engine/common/metrics.hpp
#pragma once
#include <atomic>
#include <cstdint>
#include <algorithm>

struct SpscStats {
  std::atomic<uint64_t> push_ok{0}; // number of pushes
  std::atomic<uint64_t> pop_ok{0}; // number of pops
  std::atomic<uint64_t> drops_total{0}; // number of drops
  std::atomic<uint64_t> depth_gauge{0}; // current queue depth
  std::atomic<uint64_t> max_depth{0}; // max depth

  inline void observe_depth(uint64_t d) {
    depth_gauge.store(d, std::memory_order_relaxed);
    uint64_t old = max_depth.load(std::memory_order_relaxed);
    while (old < d && !max_depth.compare_exchange_weak(old, d,
            std::memory_order_relaxed, std::memory_order_relaxed)) {}
  }
};
