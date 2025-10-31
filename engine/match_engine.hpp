#pragma once
#include <variant>          // for std::in_place_type
#include <cstdint>          // for std::uint64_t
#include "event_bus.hpp"
#include "events.hpp"
#include "lob/book.hpp"     // lob::Book with submit/cancel/replace

class MatchEngine {
public:
  // Pass a Book config to choose STP policy, etc.
  explicit MatchEngine(EventBus& bus,
                       lob::Book::BookConfig cfg = {})
    : bus_(bus), book_()
  {
    // If Book constructor doesn’t accept cfg directly, assign it.
    book_.cfg_ = cfg;
  }

  // ===== Day-6 APIs (preferred) =====

  // ----- Limit order -----
  void add(std::uint64_t trader, OrderId id, lob::Side side, Price px, Qty qty,
           lob::Book::TimeInForce tif = lob::Book::TimeInForce::Day) {
    auto r = book_.submit(trader, side, px, qty, id,
                          lob::Book::OrderType::Limit, tif);
    for (const auto& f : r.fills) {
      bus_.try_publish(Event{
        std::in_place_type<FillEvent>,
        f.taker_id, f.maker_id, side, f.px, f.qty
      });
    }
    if (r.posted_qty > 0 || r.book_changed) {
      bus_.try_publish(Event{
        std::in_place_type<BookChangeEvent>,
        side, px, book_level_qty(side, px)
      });
    }
  }

  // ----- Market order -----
  void market(std::uint64_t trader, OrderId id, lob::Side side, Qty qty,
              lob::Book::TimeInForce tif = lob::Book::TimeInForce::IOC) {
    auto r = book_.submit(trader, side, 0, qty, id,
                          lob::Book::OrderType::Market, tif);
    for (const auto& f : r.fills) {
      bus_.try_publish(Event{
        std::in_place_type<FillEvent>,
        f.taker_id, f.maker_id, side, f.px, f.qty
      });
    }
    if (r.book_changed) {
      // 0/0 indicates a best-level change without specifying price
      bus_.try_publish(Event{
        std::in_place_type<BookChangeEvent>,
        side, Price{0}, Qty{0}
      });
    }
  }

  // ----- Replace/Amend -----
  void replace(std::uint64_t trader, OrderId id, Price new_px, Qty new_qty,
               lob::Book::TimeInForce tif = lob::Book::TimeInForce::Day) {
    auto rr = book_.replace(trader, id, new_px, new_qty, tif);
    if (rr.ok) {
      // Emit conservative book-change notifications at the amended price for both sides.
      bus_.try_publish(Event{
        std::in_place_type<BookChangeEvent>,
        lob::Side::Bid, new_px, book_level_qty(lob::Side::Bid, new_px)
      });
      bus_.try_publish(Event{
        std::in_place_type<BookChangeEvent>,
        lob::Side::Ask, new_px, book_level_qty(lob::Side::Ask, new_px)
      });
    }
  }

  // ----- Cancel -----
  void cancel(OrderId id) {
    auto c = book_.cancel(id);
    if (c.ok) {
      bus_.try_publish(Event{
        std::in_place_type<CancelEvent>,
        id, c.side, c.px, c.qty_canceled
      });
      bus_.try_publish(Event{
        std::in_place_type<BookChangeEvent>,
        c.side, c.px, book_level_qty(c.side, c.px)
      });
    }
  }

  // ===== Back-compat wrappers (keep old call sites working) =====
  void add(OrderId id, lob::Side side, Price px, Qty qty) {
    add(/*trader*/0, id, side, px, qty, lob::Book::TimeInForce::Day);
  }
  void market(OrderId id, lob::Side side, Qty qty) {
    market(/*trader*/0, id, side, qty, lob::Book::TimeInForce::IOC);
  }

  // ----- helpers -----
  Qty book_level_qty(lob::Side s, Price px) const {
    if (s == lob::Side::Bid) return book_level_qty_impl(book_.bids_, px);
    return book_level_qty_impl(book_.asks_, px);
  }

private:
  template<typename Map>
  Qty book_level_qty_impl(const Map& m, Price px) const {
    auto it = m.find(px);
    return (it == m.end()) ? 0 : it->second.total_qty;
  }

  // Declare bus_ BEFORE book_ to match constructor init order.
  EventBus& bus_;
  lob::Book book_;
};
