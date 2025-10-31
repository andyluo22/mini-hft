#include <gtest/gtest.h>
#include "lob/book.hpp"

using namespace lob;

TEST(Ghosts, IOC_and_FOK_do_not_leave_stranded_ids) {
  Book b;

  // Baseline liquidity
  b.submit(/*trader*/1, Side::Ask, 100, 5, 1001,
           Book::OrderType::Limit, Book::TimeInForce::Day);

  // IOC not marketable -> must NOT post or create id
  auto r1 = b.submit(/*trader*/2, Side::Bid, 99, 5, 2002,
                     Book::OrderType::Limit, Book::TimeInForce::IOC);
  EXPECT_TRUE(r1.fills.empty());
  EXPECT_EQ(r1.posted_qty, 0);
  EXPECT_FALSE(b.has(2002));

  // FOK insufficient -> reject with no side effects
  auto r2 = b.submit(/*trader*/3, Side::Bid, 100, 6, 2003,
                     Book::OrderType::Limit, Book::TimeInForce::FOK);
  EXPECT_TRUE(r2.fills.empty());
  EXPECT_EQ(r2.posted_qty, 0);
  EXPECT_FALSE(b.has(2003));

  auto errs = b.check_invariants();
  EXPECT_TRUE(errs.empty()) << (errs.empty() ? "" : errs.front());
}

TEST(Ghosts, Replace_with_FOK_failure_removes_original_without_ghosts) {
  Book b;

  // Post bid id=10 at 100 x 5 (owned by trader 9)
  auto r0 = b.submit(/*trader*/9, Side::Bid, 100, 5, 10,
                     Book::OrderType::Limit, Book::TimeInForce::Day);
  ASSERT_EQ(r0.posted_qty, 5);
  ASSERT_TRUE(b.has(10));

  // There is only 5 ask liquidity at 100
  b.submit(/*trader*/1, Side::Ask, 100, 5, 11,
           Book::OrderType::Limit, Book::TimeInForce::Day);

  // Replace id=10 to price 100 with qty=12 and FOK:
  // Book::replace cancels original, then submits FOK 12 which cannot fully execute -> failure.
  auto rr = b.replace(/*trader*/9, /*id*/10, /*new_px*/100, /*new_qty*/12,
                      Book::TimeInForce::FOK);
  EXPECT_FALSE(rr.ok);

  // Original should be gone and no stranded id=10
  EXPECT_FALSE(b.has(10));

  auto errs = b.check_invariants();
  EXPECT_TRUE(errs.empty()) << (errs.empty() ? "" : errs.front());
}

