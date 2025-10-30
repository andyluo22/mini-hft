#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include "../engine/event_bus.hpp"
#include "../engine/match_engine.hpp"
#include "../engine/common/cpu.hpp"
#include "../engine/common/timebase.hpp"

int main() {
  pin_thread_to_cpu(0);
  EventBus bus(1<<20);
  MatchEngine eng(bus);

  // preload asks so buys cross immediately
  for (int i=0;i<10000;i++) eng.add(10000+i, Side::Sell, 1000, 1);

  const int N = 200000; // toy run
  std::vector<long long> ns; ns.reserve(N);

  for (int i=0;i<N;i++) {
    auto t0 = now_ns();
    eng.add(1'000'000+i, Side::Buy, 1000, 1);
    // drain a couple events quickly (no heavy work)
    while (auto ev=bus.try_poll()) { (void)ev; break; }
    auto t1 = now_ns();
    ns.push_back(t1 - t0);
  }
  std::nth_element(ns.begin(), ns.begin()+N/2, ns.end());
  auto p50 = ns[N/2];
  std::cout << "p50 match latency: " << p50 << " ns (" << (p50/1000.0) << " us)\n";
}

