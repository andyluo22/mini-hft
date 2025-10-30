#pragma once
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <cassert>
#include "types.hpp"
#include "order.hpp"
#include "price_level.hpp"

namespace lob {

struct Book {
  // bids: highest first; asks: lowest first
  std::map<Price, PriceLevel, std::greater<Price>> bids_; // sorted dict
  std::map<Price, PriceLevel, std::less<Price>>    asks_;
  std::unordered_map<OrderId, OrderNode*> id_index_; // unsorted dict
  Qty bids_total_{0}, asks_total_{0};

  ~Book(){ clear_all(); }

  void clear_all(){
    auto free_side = [&](auto& m){
      for (auto& [_, lvl] : m) {
        for (auto* n = lvl.head; n; ) { auto* nx=n->next; delete n; n=nx; }
        lvl.head=lvl.tail=nullptr; lvl.count=0; lvl.total_qty=0;
      }
      m.clear();
    };
    free_side(bids_); free_side(asks_);
    id_index_.clear(); bids_total_ = asks_total_ = 0;
  }

  bool has(OrderId id) const { return id_index_.count(id)>0; }

  BestOfBook best() const {
    BestOfBook b;
    if (!bids_.empty()) b.bid = bids_.begin()->first;
    if (!asks_.empty()) b.ask = asks_.begin()->first;
    return b;
  }

  // ---------- mutations (non-matching add) ----------
  bool add(OrderId id, Side side, Price px, Qty qty, TimeNs ts_ns){
    if (qty <= 0 || id_index_.count(id)) return false;

    // Non-matching mode: reject marketable (lock/cross) adds
    if (side == Side::Bid) {
      if (!asks_.empty() && px >= asks_.begin()->first) return false; // would lock/cross
      auto [it, _created] = bids_.try_emplace(px, PriceLevel{.price=px});
      OrderNode* n = new OrderNode{ id, side, px, qty, ts_ns };
      it->second.push_back(n);
      id_index_[id] = n;
      bids_total_ += qty;
    } else { // Side::Ask
      if (!bids_.empty() && px <= bids_.begin()->first) return false; // would lock/cross
      auto [it, _created] = asks_.try_emplace(px, PriceLevel{.price=px});
      OrderNode* n = new OrderNode{ id, side, px, qty, ts_ns };
      it->second.push_back(n);
      id_index_[id] = n;
      asks_total_ += qty;
    }
    return true;
  }

  bool reduce(OrderId id, Qty dq){
    if (dq <= 0) return dq==0; // treat 0 as no-op
    auto it = id_index_.find(id); if (it==id_index_.end()) return false;
    OrderNode* n = it->second;
    if (dq > n->qty) return false;

    if (n->side == Side::Bid) {
      auto lvl_it = bids_.find(n->px); if (lvl_it==bids_.end()) return false;
      auto& lvl = lvl_it->second;
      bool remains = lvl.reduce(n, dq);
      bids_total_ -= dq;
      if (!remains) {
        lvl.erase(n); delete n; id_index_.erase(it);
        if (lvl.empty()) bids_.erase(lvl_it);
      }
    } else {
      auto lvl_it = asks_.find(n->px); if (lvl_it==asks_.end()) return false;
      auto& lvl = lvl_it->second;
      bool remains = lvl.reduce(n, dq);
      asks_total_ -= dq;
      if (!remains) {
        lvl.erase(n); delete n; id_index_.erase(it);
        if (lvl.empty()) asks_.erase(lvl_it);
      }
    }
    return true;
  }

  // ---------- invariants ----------
  std::vector<std::string> check_invariants() const {
    std::vector<std::string> err;
    auto check_side = [&](auto const& side_map, Side side, Qty side_total){
      Qty sum_levels = 0;
      for (auto const& [px, lvl] : side_map) {
        Qty walk_qty = 0; std::size_t cnt=0;
        OrderNode* prev=nullptr;
        for (auto* n=lvl.head; n; n=n->next){
          ++cnt; walk_qty += n->qty;
          auto it = id_index_.find(n->id);
          if (it==id_index_.end() || it->second!=n)
            err.emplace_back("id_index mismatch id="+std::to_string(n->id));
          if (n->px!=px || n->side!=side)
            err.emplace_back("node(level mismatch) id="+std::to_string(n->id));
          if (n->prev!=prev) err.emplace_back("broken prev link @"+std::to_string(px));
          prev = n;
        }
        if (cnt!=lvl.count) err.emplace_back("level.count mismatch @"+std::to_string(px));
        if (walk_qty!=lvl.total_qty) err.emplace_back("level.total_qty mismatch @"+std::to_string(px));
        if ((lvl.count==0) != (lvl.head==nullptr && lvl.tail==nullptr))
          err.emplace_back("empty level head/tail mismatch @"+std::to_string(px));
        sum_levels += lvl.total_qty;
      }
      if (sum_levels != side_total)
        err.emplace_back(std::string("side total mismatch ")+side_str(side));
    };

    check_side(bids_, Side::Bid, bids_total_);
    check_side(asks_, Side::Ask, asks_total_);

    if (!bids_.empty() && !asks_.empty()) {
      if (!(bids_.begin()->first < asks_.begin()->first))
        err.emplace_back("locked/crossed book: best_bid>=best_ask");
    }
    return err;
  }

  struct MatchFill { OrderId taker_id, maker_id; Price px; Qty qty; };
  struct MatchResult {
    std::vector<MatchFill> fills;
    bool book_changed = false;
    Qty  posted_qty   = 0;   // qty left resting (0 for market)
  };

  enum class OrderType : uint8_t { Limit, Market };

  // ---------- matching submit ----------
  MatchResult submit(Side side, Price px, Qty qty, OrderId id, OrderType type) {
    MatchResult out{};
    if (qty <= 0) return out;

    if (side == Side::Bid) {
      // Cross against asks (lowest first)
      while (qty > 0 && !asks_.empty()) {
        Price best_ask = asks_.begin()->first;
        bool marketable = (type == OrderType::Market) || (px >= best_ask);
        if (!marketable) break;

        auto &lvl = asks_.begin()->second; // FIFO at best ask
        while (qty > 0 && lvl.head) {
          auto* maker = lvl.head;
          Qty traded = (qty < maker->qty) ? qty : maker->qty;

          maker->qty -= traded;
          qty        -= traded;
          lvl.total_qty -= traded;
          asks_total_   -= traded;

          out.fills.push_back(MatchFill{ id, maker->id, best_ask, traded });

          if (maker->qty == 0) {
            auto* dead = maker;
            lvl.head = maker->next;
            if (lvl.head) lvl.head->prev = nullptr; else lvl.tail = nullptr;
            lvl.count--;
            id_index_.erase(dead->id);
            delete dead;
            out.book_changed = true;
          }
        }
        if (!lvl.head) { asks_.erase(asks_.begin()); out.book_changed = true; }
      }

      // Post remainder if limit
      if (qty > 0 && type == OrderType::Limit) {
        auto &lvl = bids_[px];
        auto* n = new OrderNode{ .id=id, .side=side, .px=px, .qty=qty,
                                 .prev=lvl.tail, .next=nullptr };
        if (lvl.tail) lvl.tail->next = n; else lvl.head = n;
        lvl.tail = n; lvl.count++; lvl.total_qty += qty;
        id_index_[id] = n;
        bids_total_ += qty;
        out.posted_qty = qty;
        out.book_changed = true;
      }

    } else { // Side::Ask
      // Cross against bids (highest first)
      while (qty > 0 && !bids_.empty()) {
        Price best_bid = bids_.begin()->first;
        bool marketable = (type == OrderType::Market) || (px <= best_bid);
        if (!marketable) break;

        auto &lvl = bids_.begin()->second; // FIFO at best bid
        while (qty > 0 && lvl.head) {
          auto* maker = lvl.head;
          Qty traded = (qty < maker->qty) ? qty : maker->qty;

          maker->qty -= traded;
          qty        -= traded;
          lvl.total_qty -= traded;
          bids_total_   -= traded;

          out.fills.push_back(MatchFill{ id, maker->id, best_bid, traded });

          if (maker->qty == 0) {
            auto* dead = maker;
            lvl.head = maker->next;
            if (lvl.head) lvl.head->prev = nullptr; else lvl.tail = nullptr;
            lvl.count--;
            id_index_.erase(dead->id);
            delete dead;
            out.book_changed = true;
          }
        }
        if (!lvl.head) { bids_.erase(bids_.begin()); out.book_changed = true; }
      }

      // Post remainder if limit
      if (qty > 0 && type == OrderType::Limit) {
        auto &lvl = asks_[px];
        auto* n = new OrderNode{ .id=id, .side=side, .px=px, .qty=qty,
                                 .prev=lvl.tail, .next=nullptr };
        if (lvl.tail) lvl.tail->next = n; else lvl.head = n;
        lvl.tail = n; lvl.count++; lvl.total_qty += qty;
        id_index_[id] = n;
        asks_total_ += qty;
        out.posted_qty = qty;
        out.book_changed = true;
      }
    }

    return out;
  }

  struct CancelResult { bool ok; Qty qty_canceled; Price px; Side side; };

  // NOTE: explicit branching (no ternary) to avoid mixing map types.
  CancelResult cancel(OrderId id) {
    auto it = id_index_.find(id);
    if (it == id_index_.end()) return {false, 0, 0, Side::Bid};

    auto* n = it->second;
    if (n->side == Side::Bid) {
      auto lvl_it = bids_.find(n->px);
      if (lvl_it == bids_.end()) return {false, 0, 0, Side::Bid};
      auto& lvl = lvl_it->second;

      if (n->prev) n->prev->next = n->next; else lvl.head = n->next;
      if (n->next) n->next->prev = n->prev; else lvl.tail = n->prev;

      lvl.count--;
      lvl.total_qty -= n->qty;
      bids_total_   -= n->qty;

      Qty canceled = n->qty; Price px = n->px; Side s = n->side;
      id_index_.erase(it); delete n;
      if (lvl.count == 0) bids_.erase(lvl_it);
      return {true, canceled, px, s};

    } else { // Side::Ask
      auto lvl_it = asks_.find(n->px);
      if (lvl_it == asks_.end()) return {false, 0, 0, Side::Bid};
      auto& lvl = lvl_it->second;

      if (n->prev) n->prev->next = n->next; else lvl.head = n->next;
      if (n->next) n->next->prev = n->prev; else lvl.tail = n->prev;

      lvl.count--;
      lvl.total_qty -= n->qty;
      asks_total_   -= n->qty;

      Qty canceled = n->qty; Price px = n->px; Side s = n->side;
      id_index_.erase(it); delete n;
      if (lvl.count == 0) asks_.erase(lvl_it);
      return {true, canceled, px, s};
    }
  }
};

} // namespace lob
