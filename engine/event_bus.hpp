#pragma once
#include <cstddef>
#include <optional>
#include <variant>
#include <utility>
#include <atomic>

#include "spsc/spsc_ring.hpp"
#include "events.hpp"

// Event payloads are defined in events.hpp
using Event = std::variant<FillEvent, CancelEvent, BookChangeEvent>;

// Single-producer / single-consumer bus built on SpscRing<Event>.
class EventBus {
public:
  explicit EventBus(std::size_t cap = (1u << 20)) : ring_(cap) {}

  // ----- Producer-side -----
  // Push by copy
  bool try_publish(const Event& e) { return ring_.try_push(e); }

  // Push by move
  bool try_publish(Event&& e) { return ring_.try_push(std::move(e)); }

  // Construct the variant in place to avoid a temporary.
  template <class T, class... Args>
  bool publish_in_place(Args&&... args) {
    Event ev{std::in_place_type<T>, std::forward<Args>(args)...};
    return ring_.try_push(std::move(ev));
  }

  // ----- Consumer-side -----
  std::optional<Event> try_poll() {
    Event e;
    if (ring_.try_pop(e)) return e;
    return std::nullopt;
  }

  std::size_t capacity() const { return ring_.capacity(); }

private:
  SpscRing<Event> ring_;
};
