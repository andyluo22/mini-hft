#include <gtest/gtest.h>
#include "../engine/events.hpp"
#include "../engine/event_bus.hpp"
#include "../engine/match_engine.hpp"

TEST(Match, FifoAndPartial) {
  EventBus bus(1<<16);
  MatchEngine eng(bus);

  // maker side: two sells @100, FIFO 3 then 5
  eng.add(1, Side::Sell, 100, 3);
  eng.add(2, Side::Sell, 100, 5);

  // taker buy hits 6 -> fills 3 (id=1) + 3 (id=2 partial)
  eng.add(42, Side::Buy, 100, 6);

  int fills=0; Qty sum=0; OrderId first=0, second=0;
  while (auto ev=bus.try_poll()) {
    if (std::holds_alternative<FillEvent>(*ev)) {
      auto f = std::get<FillEvent>(*ev);
      fills++; sum+=f.qty;
      if (fills==1) first=f.maker_id; else if (fills==2) second=f.maker_id;
    }
  }
  EXPECT_EQ(fills, 2);
  EXPECT_EQ(sum, 6);
  EXPECT_EQ(first, 1);
  EXPECT_EQ(second, 2);
}

TEST(Match, CancelRemovesQty) {
  EventBus bus(1<<16);
  MatchEngine eng(bus);
  eng.add(10, Side::Buy, 101, 7);
  eng.cancel(10);
  int cancels=0;
  while (auto ev=bus.try_poll()) {
    if (std::holds_alternative<CancelEvent>(*ev)) cancels++;
  }
  EXPECT_EQ(cancels, 1);
}