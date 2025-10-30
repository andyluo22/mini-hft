#pragma once
#include <variant>          // for std::in_place_type
#include "event_bus.hpp"
#include "events.hpp"
#include "lob/book.hpp"     // lob::Book with submit/cancel

class MatchEngine {
public:
  explicit MatchEngine(EventBus& bus) : bus_(bus) {}

  // ----- Limit order -----
  void add(OrderId id, lob::Side side, Price px, Qty qty) {
    auto r = book_.submit(side, px, qty, id, lob::Book::OrderType::Limit);
    for (const auto& f : r.fills) {
      // Event is std::variant<FillEvent, CancelEvent, BookChangeEvent>
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
  void market(OrderId id, lob::Side side, Qty qty) {
    auto r = book_.submit(side, 0, qty, id, lob::Book::OrderType::Market);
    for (const auto& f : r.fills) {
      bus_.try_publish(Event{
        std::in_place_type<FillEvent>,
        f.taker_id, f.maker_id, side, f.px, f.qty
      });
    }
    if (r.book_changed) {
      // price/qty can be 0 to indicate "best level changed"
      bus_.try_publish(Event{
        std::in_place_type<BookChangeEvent>,
        side, Price{0}, Qty{0}
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

  lob::Book book_;
  EventBus& bus_;
};
