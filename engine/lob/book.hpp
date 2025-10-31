#pragma once
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <cassert>
#include <cstdint> // for std::uint64_t
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

  // ---- Day 6: TIF + STP config ----
  enum class TimeInForce : uint8_t { Day, IOC, FOK };
  enum class STPPolicy   : uint8_t { Allow, CancelTaker, CancelMaker, CancelBoth };
  struct BookConfig { STPPolicy stp = STPPolicy::Allow; }; // DEFAULT: Allow (opt-in STP)

  // Order owners (TraderId); 0 means unknown owner
  std::unordered_map<OrderId, std::uint64_t> owners_;
  BookConfig cfg_{};

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
    id_index_.clear(); owners_.clear();
    bids_total_ = asks_total_ = 0;
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
      owners_[id] = 0; // unknown owner in non-matching mode
      bids_total_ += qty;
    } else { // Side::Ask
      if (!bids_.empty() && px <= bids_.begin()->first) return false; // would lock/cross
      auto [it, _created] = asks_.try_emplace(px, PriceLevel{.price=px});
      OrderNode* n = new OrderNode{ id, side, px, qty, ts_ns };
      it->second.push_back(n);
      id_index_[id] = n;
      owners_[id] = 0; // unknown owner
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
        lvl.erase(n); delete n; id_index_.erase(it); owners_.erase(id);
        if (lvl.empty()) bids_.erase(lvl_it);
      }
    } else {
      auto lvl_it = asks_.find(n->px); if (lvl_it==asks_.end()) return false;
      auto& lvl = lvl_it->second;
      bool remains = lvl.reduce(n, dq);
      asks_total_ -= dq;
      if (!remains) {
        lvl.erase(n); delete n; id_index_.erase(it); owners_.erase(id);
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
    Qty  posted_qty   = 0;   // qty left resting (0 for IOC/market/FOK)
  };

  enum class OrderType : uint8_t { Limit, Market };

  // ---------- Day 6: matching submit (owner + TIF) ----------
  MatchResult submit(std::uint64_t trader, Side side, Price px, Qty qty, OrderId id,
                     OrderType type, TimeInForce tif) {
    MatchResult out{};
    if (qty <= 0) return out;
    if (type == OrderType::Limit && px <= 0) return out;

    auto can_trade = [&](Price limit_px, Price best) -> bool {
      return (side == Side::Bid) ? (limit_px >= best) : (limit_px <= best);
    };

    // ---- FOK pre-check (no side effects if insufficient)
    if (tif == TimeInForce::FOK) {
      Qty execable = 0;
      if (side == Side::Bid) {
        for (auto it = asks_.begin(); it != asks_.end(); ++it) {
          Price ak = it->first;
          if (type == OrderType::Limit && !can_trade(px, ak)) break;
          execable += it->second.total_qty;
          if (execable >= qty) break;
        }
      } else {
        for (auto it = bids_.begin(); it != bids_.end(); ++it) {
          Price bk = it->first;
          if (type == OrderType::Limit && !can_trade(px, bk)) break;
          execable += it->second.total_qty;
          if (execable >= qty) break;
        }
      }
      if (execable < qty) {
        // reject: no ghost ids, no changes
        return out;
      }
    }

    auto self_trade_block = [&](OrderNode* maker, Qty& taker_qty,
                                PriceLevel& lvl, Qty& side_total) {
      // maker owner from owners_; if missing, treat 0
      auto itown = owners_.find(maker->id);
      std::uint64_t maker_owner = (itown == owners_.end()) ? 0 : itown->second;
      if (cfg_.stp == STPPolicy::Allow || maker_owner != trader) return false;

      Qty overlap = (taker_qty < maker->qty) ? taker_qty : maker->qty;
      switch (cfg_.stp) {
        case STPPolicy::CancelTaker:
          taker_qty -= overlap;               // drop incoming overlap
          out.book_changed = true;
          break;
        case STPPolicy::CancelMaker:
          maker->qty -= overlap;              // reduce resting
          lvl.total_qty -= overlap; side_total -= overlap;
          if (maker->qty == 0) {
            auto* dead = maker;
            lvl.head = maker->next;
            if (lvl.head) lvl.head->prev = nullptr; else lvl.tail = nullptr;
            lvl.count--; id_index_.erase(dead->id); owners_.erase(dead->id);
            delete dead; out.book_changed = true;
          }
          // Drop the taker so it doesn't keep canceling more
          taker_qty = 0;
          break;
        case STPPolicy::CancelBoth:
          taker_qty   -= overlap;
          maker->qty  -= overlap;
          lvl.total_qty -= overlap; side_total -= overlap;
          if (maker->qty == 0) {
            auto* dead = maker;
            lvl.head = maker->next;
            if (lvl.head) lvl.head->prev = nullptr; else lvl.tail = nullptr;
            lvl.count--; id_index_.erase(dead->id); owners_.erase(dead->id);
            delete dead; out.book_changed = true;
          }
          break;
        default: break;
      }
      return true; // handled STP; do not trade this iteration
    };

    Qty taker_qty = qty;

    if (side == Side::Bid) {
      // Cross against asks (lowest first)
      while (taker_qty > 0 && !asks_.empty()) {
        Price best_ask = asks_.begin()->first;
        bool marketable = (type == OrderType::Market) || can_trade(px, best_ask);
        if (!marketable) break;

        auto &lvl = asks_.begin()->second; // FIFO at best ask
        while (taker_qty > 0 && lvl.head) {
          auto* maker = lvl.head;

          // ---- STP
          if (self_trade_block(maker, taker_qty, lvl, asks_total_)) {
            if (taker_qty == 0) break;      // taker dropped/consumed by STP
            if (!lvl.head) break;           // level emptied by STP
            continue;                       // skip trade, re-evaluate
          }

          // ---- Trade
          Qty traded = (taker_qty < maker->qty) ? taker_qty : maker->qty;
          maker->qty -= traded;
          taker_qty  -= traded;
          lvl.total_qty -= traded;
          asks_total_   -= traded;

          out.fills.push_back(MatchFill{ id, maker->id, best_ask, traded });

          if (maker->qty == 0) {
            auto* dead = maker;
            lvl.head = maker->next;
            if (lvl.head) lvl.head->prev = nullptr; else lvl.tail = nullptr;
            lvl.count--;
            id_index_.erase(dead->id);
            owners_.erase(dead->id);
            delete dead;
            out.book_changed = true;
          }
        }
        if (!lvl.head) { asks_.erase(asks_.begin()); out.book_changed = true; }
      }

      // IOC or Market: drop remainder (no post)
      if (tif == TimeInForce::IOC || type == OrderType::Market) return out;

      // Post remainder if limit + Day
      if (taker_qty > 0 && type == OrderType::Limit) {
        auto &lvl = bids_[px];
        auto* n = new OrderNode{ .id=id, .side=side, .px=px, .qty=taker_qty,
                                 .prev=lvl.tail, .next=nullptr };
        if (lvl.tail) lvl.tail->next = n; else lvl.head = n;
        lvl.tail = n; lvl.count++; lvl.total_qty += taker_qty;
        id_index_[id] = n;
        owners_[id]   = trader;
        bids_total_  += taker_qty;
        out.posted_qty = taker_qty;
        out.book_changed = true;
      }

    } else { // Side::Ask
      // Cross against bids (highest first)
      while (taker_qty > 0 && !bids_.empty()) {
        Price best_bid = bids_.begin()->first;
        bool marketable = (type == OrderType::Market) || can_trade(px, best_bid);
        if (!marketable) break;

        auto &lvl = bids_.begin()->second; // FIFO at best bid
        while (taker_qty > 0 && lvl.head) {
          auto* maker = lvl.head;

          // ---- STP
          if (self_trade_block(maker, taker_qty, lvl, bids_total_)) {
            if (taker_qty == 0) break;
            if (!lvl.head) break;
            continue;                       // skip trade, re-evaluate
          }

          // ---- Trade
          Qty traded = (taker_qty < maker->qty) ? taker_qty : maker->qty;
          maker->qty -= traded;
          taker_qty  -= traded;
          lvl.total_qty -= traded;
          bids_total_   -= traded;

          out.fills.push_back(MatchFill{ id, maker->id, best_bid, traded });

          if (maker->qty == 0) {
            auto* dead = maker;
            lvl.head = maker->next;
            if (lvl.head) lvl.head->prev = nullptr; else lvl.tail = nullptr;
            lvl.count--;
            id_index_.erase(dead->id);
            owners_.erase(dead->id);
            delete dead;
            out.book_changed = true;
          }
        }
        if (!lvl.head) { bids_.erase(bids_.begin()); out.book_changed = true; }
      }

      // IOC or Market: drop remainder (no post)
      if (tif == TimeInForce::IOC || type == OrderType::Market) return out;

      // Post remainder if limit + Day
      if (taker_qty > 0 && type == OrderType::Limit) {
        auto &lvl = asks_[px];
        auto* n = new OrderNode{ .id=id, .side=side, .px=px, .qty=taker_qty,
                                 .prev=lvl.tail, .next=nullptr };
        if (lvl.tail) lvl.tail->next = n; else lvl.head = n;
        lvl.tail = n; lvl.count++; lvl.total_qty += taker_qty;
        id_index_[id] = n;
        owners_[id]   = trader;
        asks_total_  += taker_qty;
        out.posted_qty = taker_qty;
        out.book_changed = true;
      }
    }

    return out;
  }

  // ---------- legacy submit wrapper (kept for compatibility) ----------
  MatchResult submit(Side side, Price px, Qty qty, OrderId id, OrderType type) {
    // Back-compat: unknown owner, Day TIF
    return submit(/*trader*/0, side, px, qty, id, type, TimeInForce::Day);
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
      id_index_.erase(it); owners_.erase(id); delete n;
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
      id_index_.erase(it); owners_.erase(id); delete n;
      if (lvl.count == 0) asks_.erase(lvl_it);
      return {true, canceled, px, s};
    }
  }

  // ---------- Day 6: Replace/Amend ----------
  struct ReplaceResult { bool ok; OrderId id; };

  ReplaceResult replace(std::uint64_t trader, OrderId id, Price new_px, Qty new_qty,
                        TimeInForce tif = TimeInForce::Day) {
    auto it = id_index_.find(id);
    if (it == id_index_.end()) return {false, id};
    OrderNode* n = it->second;

    // simple ownership: if known owner and doesn't match trader, reject
    auto io = owners_.find(id);
    std::uint64_t owner = (io == owners_.end()) ? 0 : io->second;
    if (owner != 0 && owner != trader) return {false, id};

    if (new_qty <= 0) return {false, id};

    // Same price + size decrease => keep priority (in-place)
    if (new_px == n->px && new_qty <= n->qty) {
      Qty delta = n->qty - new_qty;
      if (delta > 0) {
        if (n->side == Side::Bid) {
          auto lvl_it = bids_.find(n->px); if (lvl_it==bids_.end()) return {false, id};
          auto& lvl = lvl_it->second;
          bool remains = lvl.reduce(n, delta); (void)remains; // should remain
          bids_total_ -= delta;
        } else {
          auto lvl_it = asks_.find(n->px); if (lvl_it==asks_.end()) return {false, id};
          auto& lvl = lvl_it->second;
          bool remains = lvl.reduce(n, delta); (void)remains;
          asks_total_ -= delta;
        }
      }
      return {true, id};
    }

    // Otherwise: cancel then submit fresh (new priority; obey IOC/FOK)
    auto c = cancel(id);
    if (!c.ok) return {false, id};
    auto r = submit(trader, c.side, new_px, new_qty, id, OrderType::Limit, tif);

    // If FOK and nothing happened, treat as failure
    bool ok = (tif == TimeInForce::FOK)
              ? (!r.fills.empty() || r.posted_qty > 0)
              : true;
    return {ok, id};
  }
};

} // namespace lob
