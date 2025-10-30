#pragma once
#include <cstddef>
#include <cassert>
#include "types.hpp"
#include "order.hpp"

namespace lob {

struct PriceLevel {
  Price price{};
  Qty   total_qty{0};
  std::size_t count{0};
  OrderNode* head{nullptr};
  OrderNode* tail{nullptr};

  void push_back(OrderNode* n) {
    n->prev = tail; n->next = nullptr;
    if (tail) tail->next = n; else head = n; // both tail and head = n if originally empt
    tail = n; n->level = this;
    ++count; total_qty += n->qty;
  }

  void erase(OrderNode* n) {
    if (n->prev) n->prev->next = n->next; else head = n->next;
    if (n->next) n->next->prev = n->prev; else tail = n->prev;
    n->prev = n->next = nullptr; n->level = nullptr;
    --count; total_qty -= n->qty;
  }

  // subtract dq from node & level; return true if order remains (>0)
  bool reduce(OrderNode* n, Qty dq) {
    assert(dq >= 0 && dq <= n->qty);
    n->qty -= dq; total_qty -= dq;
    return n->qty > 0;
  }

  bool empty() const { return count == 0; }
};

} // namespace lob