#include <gtest/gtest.h>
#include "lob/book.hpp"

using namespace lob;

TEST(TIF, IOC_does_not_post_when_not_marketable) {
  Book b;

  // Resting ask: 100 x 5
  auto r0 = b.submit(/*trader*/1, Side::Ask, /*px*/100, /*qty*/5, /*id*/101,
                     Book::OrderType::Limit, Book::TimeInForce::Day);
  ASSERT_EQ(r0.posted_qty, 5);

  // IOC bid at 99 (not marketable) -> no post, no fill, no ghost id
  auto r1 = b.submit(/*trader*/2, Side::Bid, /*px*/99, /*qty*/10, /*id*/202,
                     Book::OrderType::Limit, Book::TimeInForce::IOC);
  EXPECT_TRUE(r1.fills.empty());
  EXPECT_EQ(r1.posted_qty, 0);
  EXPECT_FALSE(b.has(202));

  // Book still has the ask level intact
  auto errs = b.check_invariants();
  EXPECT_TRUE(errs.empty()) << (errs.empty() ? "" : errs.front());
}

TEST(TIF, FOK_all_or_nothing) {
  Book b;

  // Resting asks: 100 x 5
  auto r0 = b.submit(/*trader*/1, Side::Ask, 100, 5, 11,
                     Book::OrderType::Limit, Book::TimeInForce::Day);
  ASSERT_EQ(r0.posted_qty, 5);

  // FOK bid for 6 cannot fully fill -> reject (no changes, no ghost id)
  auto r1 = b.submit(/*trader*/2, Side::Bid, 100, 6, 22,
                     Book::OrderType::Limit, Book::TimeInForce::FOK);
  EXPECT_TRUE(r1.fills.empty());
  EXPECT_EQ(r1.posted_qty, 0);
  EXPECT_FALSE(b.has(22));

  // Add more liquidity at same price: +3 -> total 8 available
  auto r2 = b.submit(/*trader*/3, Side::Ask, 100, 3, 33,
                     Book::OrderType::Limit, Book::TimeInForce::Day);
  ASSERT_EQ(r2.posted_qty, 3);

  // Now FOK 6 should fully execute
  auto r3 = b.submit(/*trader*/2, Side::Bid, 100, 6, 44,
                     Book::OrderType::Limit, Book::TimeInForce::FOK);
  // We don't expose filled_qty, so check via fills count/qty sum
  Qty sum = 0; for (auto& f : r3.fills) sum += f.qty;
  EXPECT_GT(sum, 0);
  EXPECT_EQ(r3.posted_qty, 0);

  auto errs = b.check_invariants();
  EXPECT_TRUE(errs.empty()) << (errs.empty() ? "" : errs.front());
}