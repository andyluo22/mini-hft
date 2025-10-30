#pragma once
#include <cstdint>
#include "lob/types.hpp"

// Re-export scalar types + Side so tests can write Side::Bid/Side::Ask
using Side    = lob::Side;
using OrderId = lob::OrderId;
using Price   = lob::Price;
using Qty     = lob::Qty;

// ---- Events carried on the EventBus ----
struct FillEvent {
  OrderId taker_id;
  OrderId maker_id;
  Side    side;          // taker side (Bid/Ask)
  Price   px;
  Qty     qty;
};

struct CancelEvent {
  OrderId id;
  Side    side;
  Price   px;
  Qty     qty_canceled;  // tests expect this exact field name
};

struct BookChangeEvent {
  Side  side;
  Price px;
  Qty   level_qty;       // total resting at this price after the change
};
