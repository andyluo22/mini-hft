#pragma once
#include <cstdint>
#include <optional>

namespace lob {

using OrderId = std::uint64_t;
using Qty     = std::int64_t;      // >=1 while resting
using Price   = std::int64_t;      // integer ticks
using TimeNs  = std::uint64_t;     // monotonic ns

enum class Side : std::uint8_t { Bid=0, Ask=1 };
inline const char* side_str(Side s){ return s==Side::Bid?"BID":"ASK"; }

struct BestOfBook {
  std::optional<Price> bid, ask;
  std::optional<double> mid() const {
    if (!bid || !ask) return std::nullopt;
    return 0.5 * (double(*bid) + double(*ask));
  }
  std::optional<Price> spread() const {
    if (!bid || !ask) return std::nullopt;
    return *ask - *bid; // unwrap std::optional<Price>
  }
};

} // namespace lob