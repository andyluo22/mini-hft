#pragma once
#include <cstdint>

using OrderId = std::uint64_t;
using Price   = std::int64_t;   // ticks
using Qty     = std::int64_t;

enum class Side : std::uint8_t { Buy=0, Sell=1 };
enum class EventKind : std::uint8_t { Fill, PartialFill, Cancel, BookChange };

struct FillEvent {
  OrderId taker_id;
  OrderId maker_id;
  Side taker_side;
  Price px;
  Qty   qty;
};

struct CancelEvent {
  OrderId id;
  Side    side;
  Price   px;
  Qty     qty_canceled;
};

struct BookChangeEvent {
  Side side;
  Price px;
  Qty   new_level_qty; // 0 if level removed
};