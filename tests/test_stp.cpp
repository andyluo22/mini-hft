#include <gtest/gtest.h>
#include "lob/book.hpp"

using namespace lob;

TEST(STP, CancelTaker_drops_incoming_overlap) {
  Book b;
  b.cfg_.stp = Book::STPPolicy::CancelTaker;

  // Trader 7 posts ask 100 x 10
  auto r0 = b.submit(/*trader*/7, Side::Ask, 100, 10, 101,
                     Book::OrderType::Limit, Book::TimeInForce::Day);
  ASSERT_EQ(r0.posted_qty, 10);

  // Same trader sends bid MKT 12: overlap should be dropped, no fills
  auto r1 = b.submit(/*trader*/7, Side::Bid, 0, 12, 202,
                     Book::OrderType::Market, Book::TimeInForce::IOC);
  EXPECT_TRUE(r1.fills.empty());
  EXPECT_FALSE(b.has(202)); // market IOC shouldn't post

  // Resting ask should still be there (maybe reduced only by STP policies—here it shouldn't)
  auto errs = b.check_invariants();
  EXPECT_TRUE(errs.empty()) << (errs.empty() ? "" : errs.front());
}

TEST(STP, CancelMaker_reduces_resting_liquidity) {
  Book b;
  b.cfg_.stp = Book::STPPolicy::CancelMaker;

  // Trader 7 posts ask 100 x 5
  b.submit(/*trader*/7, Side::Ask, 100, 5, 101,
           Book::OrderType::Limit, Book::TimeInForce::Day);

  // Same trader sends bid 100 x 3 IOC; should produce no fills (self), but reduce resting by 3
  auto r = b.submit(/*trader*/7, Side::Bid, 100, 3, 202,
                    Book::OrderType::Limit, Book::TimeInForce::IOC);
  EXPECT_TRUE(r.fills.empty());

  // Another trader can now take remaining 2
  auto r2 = b.submit(/*trader*/8, Side::Bid, 100, 2, 303,
                     Book::OrderType::Limit, Book::TimeInForce::IOC);
  // This time we expect fills against the remaining 2
  Qty sum = 0; for (auto& f : r2.fills) sum += f.qty;
  EXPECT_EQ(sum, 2);

  auto errs = b.check_invariants();
  EXPECT_TRUE(errs.empty()) << (errs.empty() ? "" : errs.front());
}

