#include <random>
#include <cassert>
#include <iostream>
#include <algorithm>            // std::find, std::erase (C++20)
#include "../engine/lob/book.hpp"

using namespace lob;

int main() {
  Book book;
  std::mt19937_64 rng(42);
  std::uniform_int_distribution<int> side_d(0,1);
  std::uniform_int_distribution<int> px_d(1000,1100);
  std::uniform_int_distribution<int> qty_d(1,50);
  std::uniform_int_distribution<int> op_d(0,9);

  std::vector<OrderId> live;

  // helper: remove id from `live` if present (choose ONE of the two bodies)
  auto remove_live = [&](OrderId id) {
    // Option A (C++20): erase by value
    std::erase(live, id);

    // Option B (pre-C++20): guard the iterator
    // auto it = std::find(live.begin(), live.end(), id);
    // if (it != live.end()) live.erase(it);
  };

  auto pick = [&](){
    // precondition: live is non-empty (caller ensures this)
    std::uniform_int_distribution<std::size_t> ix(0, live.size()-1);
    return live[ix(rng)];
  };

  const int N = 5000;
  for (int i = 0; i < N; ++i) {
    int op = op_d(rng);

    if (op <= 5 || live.empty()) {
      // add
      OrderId id = static_cast<OrderId>(rng() | 1ULL);     // random odd id
      Side s = side_d(rng) == 0 ? Side::Bid : Side::Ask;
      Price p = px_d(rng);
      Qty q = qty_d(rng);
      if (book.add(id, s, p, q, i)) live.push_back(id);
    } else if (op <= 7) {
      // cancel
      OrderId id = pick();
      if (book.cancel(id)) remove_live(id);
    } else {
      // reduce
      OrderId id = pick();
      Qty dq = qty_d(rng) % 4;          // 0..3, 0 means no-op
      (void)book.reduce(id, dq);
      if (!book.has(id)) remove_live(id);
    }

    auto errs = book.check_invariants();
    if (!errs.empty()) {
      std::cerr << "Invariant failure at step " << i << ":\n";
      for (auto& e : errs) std::cerr << "  - " << e << "\n";
      return 1;
    }
  }

  std::cout << "lob_props: OK\n";
  return 0;
}
