#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../engine/event_bus.hpp"
#include "../engine/match_engine.hpp"
#include "../engine/common/cpu.hpp"
#include "../engine/common/timebase.hpp"

int main() {
  // Pin to a core for a cleaner toy measurement.
  tb::pin_thread_to_cpu(0);

  EventBus bus(1 << 20);
  MatchEngine eng(bus);

  // Preload asks so incoming bids cross immediately
  for (int i = 0; i < 10000; i++) {
    eng.add(10000 + i, lob::Side::Ask, 1000, 1);
  }

  const int N = 200000; // toy run
  std::vector<std::uint64_t> ns;
  ns.reserve(N);

  for (int i = 0; i < N; i++) {
    auto t0 = tb::now_ns();
    eng.add(1'000'000 + i, lob::Side::Bid, 1000, 1);

    // Drain at most one event quickly (keep the ring from backing up)
    if (auto ev = bus.try_poll()) {
      (void)ev;
    }

    auto t1 = tb::now_ns();
    ns.push_back(t1 - t0);
  }

  // p50 via nth_element
  std::nth_element(ns.begin(), ns.begin() + N / 2, ns.end());
  auto p50 = ns[N / 2];

  std::cout << "p50 match latency: " << p50 << " ns ("
            << (p50 / 1000.0) << " us)\n";
}
