# Limit Order Book — Invariants (Day 4)

> Treat these as **unit-testable contracts**. Every LOB mutation must leave the book satisfying all invariants below.

---

## 0) Definitions

- **Tick**: the minimal price increment. We store prices in **integer ticks** (no floats in core engine).
- **Side**: `{Bid, Ask}`. Bids want higher prices; Asks want lower.
- **Level**: all resting orders at the same price tick on the same side, kept **FIFO**.
- **Best**: `best_bid = max(bid prices)`, `best_ask = min(ask prices)`.

---

## 1) Topology & Price Invariants

1. **Integer ticks only**: all `level.price` and `order.price` are integers (type-enforced).
2. **Single level per (side, price)**: at most one level per price on a side.
3. **Sorted levels**: bid keys strictly **decreasing**; ask keys strictly **increasing**.
4. **No locked/crossed book**: if both sides exist, `best_bid < best_ask`.
5. **Best pointers consistent**: `best_bid` is first key of bid map; `best_ask` is first key of ask map.

---

## 2) Quantity & Accounting Invariants

6. **Non-negative**: every resting order `qty > 0`.
7. **Level totals**: `level.total_qty == sum(order.qty for order in level.queue)` and `level.count == number of orders`.
8. **Side totals**: `side.total_qty == sum(level.total_qty)` across that side.
9. **Global totals**: `global.total_qty == bids.total_qty + asks.total_qty`.
10. **Empty level pruning**: if `level.count == 0` then that (side, price) **does not exist** in the map.

---

## 3) Identity & Index Invariants

11. **Unique IDs**: every resting order ID is unique.
12. **Index bijection**: `id_index[id]` points to exactly one in-book order node; every in-book node has an entry in `id_index`.
13. **Level membership**: an order referenced by `id_index` appears **in exactly one** level and that level’s `price` and `side` match the order’s fields.

---

## 4) FIFO & Time-Priority Invariants

14. **FIFO within level**: iterating a level yields orders in strict **arrival order**. Reductions preserve relative order; cancels remove a node without reordering others.
15. **Stable head/tail links**: per level, head has `prev=null`, tail has `next=null`, and links are mutually consistent (no cycles).

---

## 5) Best-of-Book & Derived Values

16. **Spread positivity**: if both sides exist, `spread = best_ask - best_bid > 0`.
17. **Mid well-formed**: `mid = (best_bid + best_ask)/2` is defined iff both sides exist.
18. **Price set coverage**: `best_bid`/`best_ask` are present in their side’s key sets when non-empty.

---

## 6) Concurrency Boundary (Day 4 scope)

> The Day-4 LOB is **single-threaded**; cross-thread handoff uses your SPSC channels (Day 2–3). **No atomics inside the LOB**.

---

## 7) Operations that must preserve invariants

- `add(order)`
- `cancel(order_id)`
- `reduce(order_id, dq)`
- (Optional) `replace(order_id, new_price)` as `cancel + add` at engine layer

All leave the book satisfying **1–18**.

---

## 8) Invariant checks — sketch

```text
check_invariants():
  errors = []
  for side in {bids, asks}:
    for (p, lvl) in side.levels:
      if lvl.total_qty != sum(n.qty for n in lvl.queue): errors += [..]
      if lvl.count     != len(lvl.queue): errors += [..]
      if not dll_is_consistent(lvl): errors += [..]
      for node in lvl.queue:
        if id_index[node.id] != &node: errors += [..]
        if node.price != p or node.side != side: errors += [..]
  if bids.total_qty != sum(l.total_qty for l in bids): errors += [..]
  if asks.total_qty != sum(l.total_qty for l in asks): errors += [..]
  if bids.nonempty and asks.nonempty and best_bid >= best_ask: errors += [..]
  return errors
