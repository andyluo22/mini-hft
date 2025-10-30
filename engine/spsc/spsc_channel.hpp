// engine/spsc/spsc_channel.hpp
#pragma once
#include <cstdint>
#include <cstddef>   // std::size_t
#include <utility>   // std::forward
#include <thread>
#include <chrono>
#include "../spsc/spsc_ring.hpp"
#include "../common/metrics.hpp"

#if defined(__x86_64__) || defined(_M_X64)
  #include <immintrin.h>
  static inline void cpu_relax() { _mm_pause(); }
#else
  static inline void cpu_relax() {}
#endif

enum class BpMode { Drop, Spin, Sleep };

struct BackpressureCfg {
  std::size_t high_wm;           // start applying backpressure at/above this depth
  std::size_t low_wm;            // hysteresis target (Spin/Sleep). Default = high_wm
  BpMode mode{BpMode::Drop};
  uint64_t sleep_ns{5000};       // Sleep mode only (5 µs)

  // Helpful ctor to set sensible defaults
  BackpressureCfg(std::size_t high,
                  std::size_t low = SIZE_MAX,
                  BpMode m = BpMode::Drop,
                  uint64_t ns = 5000)
    : high_wm(high),
      low_wm(low == SIZE_MAX ? high : low),
      mode(m),
      sleep_ns(ns) {}
};

template<typename T>
class SpscChannel {
public:
  SpscChannel(std::size_t cap_pow2, BackpressureCfg cfg, SpscStats* stats=nullptr)
  : q_(cap_pow2), cfg_(cfg), stats_(stats) {}

  // Producer-facing push with backpressure policy.
  template<typename U>
  bool push(U&& v, const std::atomic<bool>* stop_flag ) {
    for (;;) {
      if (stop_flag && stop_flag->load(std::memory_order_relaxed)) {
        return false;
      }

      const std::size_t depth = q_.size();
      if (stats_) stats_->observe_depth(depth);

      // Early backpressure near watermark to keep latency predictable.
      if (depth >= cfg_.high_wm) {
        if (cfg_.mode == BpMode::Drop) {
          if (stats_) stats_->drops_total.fetch_add(1, std::memory_order_relaxed);
          return false;
        } else if (cfg_.mode == BpMode::Spin) {
          // spin until below low watermark or we manage to push
          if (depth > cfg_.low_wm) { cpu_relax(); continue; }
        } else { // Sleep
          if (depth > cfg_.low_wm) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(cfg_.sleep_ns));
            continue;
          }
        }
      }

      if (q_.try_push(std::forward<U>(v))) {
        if (stats_) stats_->push_ok.fetch_add(1, std::memory_order_relaxed);
        return true;
      }

      // Ring truly full. Apply policy even if below high_wm due to races.
      if (cfg_.mode == BpMode::Drop) {
        if (stats_) stats_->drops_total.fetch_add(1, std::memory_order_relaxed);
        return false;
      } else if (cfg_.mode == BpMode::Spin) {
        cpu_relax();
      } else {
        std::this_thread::sleep_for(std::chrono::nanoseconds(cfg_.sleep_ns));
      }
    }
  }

  // Consumer-facing pop (just delegates, but tracks counters/gauge).
  bool pop(T& out) {
    const bool ok = q_.try_pop(out);
    if (ok && stats_) {
      stats_->pop_ok.fetch_add(1, std::memory_order_relaxed);
      stats_->observe_depth(q_.size());
    }
    return ok;
  }

  std::size_t size() const { return q_.size(); }
  std::size_t capacity() const { return q_.capacity(); }

private:
  SpscRing<T>    q_;
  BackpressureCfg cfg_;
  SpscStats*     stats_; // not owned
};
