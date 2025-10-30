#pragma once
#include <optional>
#include <variant>
#include <atomic>
#include "../spsc/spsc_ring.hpp"
#include "events.hpp"

using Event = std::variant<FillEvent, CancelEvent, BookChangeEvent>;

// Single-producer/single-consumer bus built on your SpscRing<Event>.
class EventBus {
public:
  explicit EventBus(std::size_t cap = (1u<<20))
  : ring_(cap) {}

  // producer thread only
  bool try_publish(const Event& e) { return ring_.try_push(e); }

  // consumer thread only
  std::optional<Event> try_poll() {
    Event e;
    if (ring_.try_pop(e)) return e;
    return std::nullopt;
  }

  std::size_t capacity() const { return ring_.capacity(); }

private:
  SpscRing<Event> ring_;
};