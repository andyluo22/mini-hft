#pragma once
#include <cstdint>
#include <optional>

namespace lob {

using OrderId = std::uint64_t;
using Qty     = std::int64_t;   // >=1 while resting
using Price   = std::int64_t;   // integer ticks
using TimeNs  = std::uint64_t;  // monotonic ns

// Primary sides + back-compat aliases used by tests
enum class Side : std::uint8_t {
  Bid  = 0,
  Ask  = 1,
  Buy  = 0,   // alias of Bid
  Sell = 1    // alias of Ask
};

// Avoid duplicate switch cases (Buy==Bid, Sell==Ask)
inline constexpr const char* side_str(Side s) {
  const auto v = static_cast<std::uint8_t>(s);
  if (v == 0) return "BID";
  if (v == 1) return "ASK";
  return "?";
}

struct BestOfBook {
  std::optional<Price> bid, ask;

  std::optional<double> mid() const {
    if (!bid || !ask) return std::nullopt;
    return 0.5 * (static_cast<double>(*bid) + static_cast<double>(*ask));
  }

  std::optional<Price> spread() const {
    if (!bid || !ask) return std::nullopt;
    return *ask - *bid;
  }
};

} // namespace lob
