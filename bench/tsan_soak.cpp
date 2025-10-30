#include <random>
#include "../engine/event_bus.hpp"
#include "../engine/match_engine.hpp"

int main() {
  EventBus bus(1<<20);
  MatchEngine eng(bus);
  std::mt19937_64 rng(123);
  std::uniform_int_distribution<int> sd(0,1), qd(1,5), pd(995,1005);

  for (int i=0;i<2'000'000;i++) {
    int op = rng()%10;
    if (op<4) eng.add(10'000+i, sd(rng)?Side::Buy:Side::Sell, pd(rng), qd(rng));
    else if (op<7) eng.market(20'000+i, sd(rng)?Side::Buy:Side::Sell, qd(rng));
    else eng.cancel(10'000 + (i%1000));
    while (auto ev=bus.try_poll()) { (void)ev; }
  }
}

