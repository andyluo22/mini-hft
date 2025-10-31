#include <gtest/gtest.h>
#include "lob/book.hpp"

using namespace lob;

TEST(Replace, Decrease_same_price_keeps_priority) {
  Book b;

  // Post two bids at 100: id=10 (front), then id=20 (behind)
  auto r1 = b.submit(/*trader*/1, Side::Bid, 100, 10, 10,
                     Book::OrderType::Limit, Book::TimeInForce::Day);
  ASSERT_EQ(r1.posted_qty, 10);
  auto r2 = b.submit(/*trader*/2, Side::Bid, 100, 10, 20,
                     Book::OrderType::Limit, Book::TimeInForce::Day);
  ASSERT_EQ(r2.posted_qty, 10);

  // Decrease id=10 to 6 at same price: should keep head priority
  auto rr = b.replace(/*trader*/1, /*id*/10, /*new_px*/100, /*new_qty*/6,
                      Book::TimeInForce::Day);
  EXPECT_TRUE(rr.ok);

  // Market sell 6 should hit maker id=10 first
  auto t = b.submit(/*trader*/3, Side::Ask, 0, 6, 30,
                    Book::OrderType::Market, Book::TimeInForce::IOC);
  ASSERT_FALSE(t.fills.empty());
  EXPECT_EQ(t.fills.front().maker_id, 10);

  auto errs = b.check_invariants();
  EXPECT_TRUE(errs.empty()) << (errs.empty() ? "" : errs.front());
}

TEST(Replace, Price_change_or_increase_loses_priority) {
  Book b;

  // Two bids at 100: id=10 then id=20
  b.submit(1, Side::Bid, 100, 10, 10, Book::OrderType::Limit, Book::TimeInForce::Day);
  b.submit(2, Side::Bid, 100, 10, 20, Book::OrderType::Limit, Book::TimeInForce::Day);

  // Move id=10 to 101 (price change) -> cancel+repost; now it's alone/best at 101
  auto rr = b.replace(/*trader*/1, /*id*/10, /*new_px*/101, /*new_qty*/10,
                      Book::TimeInForce::Day);
  EXPECT_TRUE(rr.ok);

  // Market sell 10 should trade at 101 with maker id=10
  auto t = b.submit(/*trader*/3, Side::Ask, 0, 10, 30,
                    Book::OrderType::Market, Book::TimeInForce::IOC);
  ASSERT_FALSE(t.fills.empty());
  EXPECT_EQ(t.fills.front().px, 101);
  EXPECT_EQ(t.fills.front().maker_id, 10);

  auto errs = b.check_invariants();
  EXPECT_TRUE(errs.empty()) << (errs.empty() ? "" : errs.front());
}

