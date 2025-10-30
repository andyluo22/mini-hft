// tests/spsc_soak_tsan.cpp
#include <atomic>
#include <thread>
#include <cassert>
#include "../engine/spsc/spsc_channel.hpp"

int main() {
  SpscStats stats;
  BackpressureCfg cfg{ (1u<<16)*3/4, (1u<<16)/2, BpMode::Spin, 0 };
  SpscChannel<int> ch(1u << 16, cfg, &stats);

  std::atomic<bool> stop{false};
  std::thread prod([&]{
    int x=0; while (!stop.load()) { ch.push(x++, &stop); }
  });
  std::thread cons([&]{
    int out; uint64_t seen=0;
    while (!stop.load()) { if (ch.pop(out)) { ++seen; } }
  });

  std::this_thread::sleep_for(std::chrono::seconds(60)); // 60s soak
  stop.store(true);
  prod.join(); cons.join();

  // No assertion needed; TSAN will yell if there are data races
  return 0;
}