#pragma once
#include "types.hpp"

namespace lob {

struct PriceLevel; // fwd

struct OrderNode {
  OrderId id{};
  Side    side{};
  Price   px{};
  Qty     qty{};     // leaves
  TimeNs  ts_ns{};   // arrival (audits/tests)
  // intrusive links within a level (FIFO)
  OrderNode* prev{nullptr};
  OrderNode* next{nullptr};
  PriceLevel* level{nullptr};
};

} // namespace lob