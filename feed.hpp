/*
Copyright (c) 2012-2015 Erik Rigtorp <erik@rigtorp.se>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#pragma once

#include "HashMap.h"
#include <boost/container/flat_map.hpp>
#include <iostream>

struct BestPrice {
  int64_t bidqty;
  int64_t bid;
  int64_t ask;
  int64_t askqty;

  BestPrice(int64_t bidqty, int64_t bid, int64_t ask, int64_t askqty)
      : bidqty(bidqty), bid(bid), ask(ask), askqty(askqty) {}

  BestPrice() : bidqty(0), bid(0), ask(0), askqty(0) {}

  friend std::ostream &operator<<(std::ostream &out, const BestPrice &best) {
    out << "BestPrice(" << best.bidqty << ", " << best.bid << ", " << best.ask
        << ", " << best.askqty << ")";
    return out;
  }
};

class OrderBook {
public:
  struct Level {
    int64_t price = 0;
    int64_t qty = 0;
    uint64_t seqno = 0;

    friend std::ostream &operator<<(std::ostream &out, const Level &level) {
      out << "Level(" << level.price << ", " << level.qty << ", " << level.seqno
          << ")";
      return out;
    }
  };

public:
  OrderBook(void *data = NULL) : data_(NULL) {}

  BestPrice GetBestPrice() const {
    auto buy = buy_.rbegin();
    auto sell = sell_.rbegin();
    BestPrice bp;
    if (buy != buy_.rend()) {
      bp.bidqty = buy->second.qty;
      bp.bid = buy->second.price;
    }
    if (sell != sell_.rend()) {
      bp.askqty = sell->second.qty;
      bp.ask = sell->second.price;
    }
    return bp;
  }

  void *GetUserData() const { return data_; }

  void SetUserData(void *data) { data_ = data; }

  bool Add(uint64_t seqno, bool buy_sell, int64_t price, int64_t qty) {
    if (qty <= 0) {
      return false;
    }
    auto &side = buy_sell ? buy_ : sell_;
    int64_t prio = buy_sell ? -price : price;
    auto it = side.insert(std::make_pair(prio, Level())).first;
    it->second.price = price;
    it->second.qty += qty;
    it->second.seqno = seqno;
    return it == side.begin();
  }

  bool Reduce(uint64_t seqno, bool buy_sell, int64_t price, int64_t qty) {
    auto &side = buy_sell ? buy_ : sell_;
    int64_t prio = buy_sell ? -price : price;
    auto it = side.find(prio);
    if (it == side.end()) {
      return false;
    }
    it->second.qty -= qty;
    it->second.seqno = seqno;
    if (it->second.qty <= 0) {
      side.erase(it);
    }
    return it == side.begin();
  }

  bool IsCrossed() {
    if (buy_.empty() || sell_.empty()) {
      return false;
    }
    return -buy_.rbegin()->first >= sell_.rbegin()->first;
  }

  void UnCross() {
    // Invalidates iterators
    auto bit = buy_.begin();
    auto sit = sell_.begin();
    while (bit != buy_.end() && sit != sell_.end() &&
           bit->second.price >= sit->second.price) {
      if (bit->second.seqno > sit->second.seqno) {
        sell_.erase(sit++);
      } else {
        buy_.erase(bit++);
      }
    }
  }

  friend std::ostream &operator<<(std::ostream &out, const OrderBook &book) {
    out << "Buy:" << std::endl;
    for (auto it = book.buy_.rbegin(); it != book.buy_.rend(); it++) {
      out << it->second << std::endl;
    }
    out << "Sell:" << std::endl;
    for (auto it = book.sell_.rbegin(); it != book.sell_.rend(); it++) {
      out << it->second << std::endl;
    }
    return out;
  }

private:
  boost::container::flat_map<int64_t, Level, std::greater<int64_t>> buy_, sell_;
  void *data_ = nullptr;
};

template <typename Handler> class Feed {

  static constexpr int16_t NOBOOK = std::numeric_limits<int16_t>::max();
  static constexpr int16_t MAXBOOK = std::numeric_limits<int16_t>::max();

  struct Order {
    int64_t price = 0;
    int32_t qty = 0;
    bool buy_sell = 0;
    int16_t bookid = NOBOOK;

    Order(int64_t price, int32_t qty, int16_t buy_sell, int16_t bookid)
        : price(price), qty(qty), buy_sell(buy_sell), bookid(bookid) {}
    Order() {}
  };

  static_assert(sizeof(Order) == 16, "");

public:
  Feed(Handler &handler, size_t size_hint, bool all_orders = false,
       bool all_books = false)
      : handler_(handler), all_orders_(all_orders), all_books_(all_books),
        symbols_(16384, 0),
        orders_(size_hint, std::numeric_limits<uint64_t>::max()) {
    size_hint_ = orders_.bucket_count();
  }

  ~Feed() {
    if (orders_.bucket_count() > size_hint_) {
      std::cerr << "WARNING bucket count " << orders_.bucket_count()
                << " greater than size hint " << size_hint_
                << ", recommend increasing size_hint" << std::endl;
    }
  }

  OrderBook &Subscribe(std::string instrument, void *data = NULL) {
    if (instrument.size() < 8) {
      instrument.insert(instrument.size(), 8 - instrument.size(), ' ');
    }
    uint64_t symbol = __builtin_bswap64(
        *reinterpret_cast<const uint64_t *>(instrument.data()));

    auto it = symbols_.find(symbol);
    if (it != symbols_.end()) {
      return books_[it->second];
    }

    if (books_.size() == MAXBOOK) {
      throw std::runtime_error("too many subscriptions");
    }

    books_.push_back(OrderBook());
    symbols_.emplace(symbol, books_.size() - 1);

    OrderBook &book = books_.back();
    book.SetUserData(data);
    return book;
  }

  void Add(uint64_t seqno, uint64_t ref, bool buy_sell, int32_t qty,
           uint64_t symbol, int64_t price) {
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) {
      if (!all_books_) {
        if (all_orders_) {
          orders_.emplace(ref, Order(price, qty, buy_sell, NOBOOK));
        }
        return;
      }
      if (books_.size() == MAXBOOK) {
        // too many books
        return;
      }
      books_.push_back(OrderBook());
      it = symbols_.emplace(symbol, books_.size() - 1).first;
    }
    int16_t bookid = it->second;
    OrderBook &book = books_[bookid];
    if (orders_.emplace(ref, Order(price, qty, buy_sell, bookid)).second) {
      bool top = book.Add(seqno, buy_sell, price, qty);
      handler_.OnQuote(&book, top);
    }
  }

  void Executed(uint64_t seqno, uint64_t ref, int32_t qty) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    if (order.bookid != NOBOOK) {
      OrderBook &book = books_[order.bookid];
      bool top = book.Reduce(seqno, order.buy_sell, order.price, qty);
      handler_.OnTrade(&book, qty, order.price, top);
    }

    order.qty -= qty;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void ExecutedAtPrice(uint64_t seqno, uint64_t ref, int32_t qty,
                       int64_t price) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    if (order.bookid != NOBOOK) {
      OrderBook &book = books_[order.bookid];
      bool top = book.Reduce(seqno, order.buy_sell, order.price, qty);
      handler_.OnTrade(&book, qty, price, top);
    }

    order.qty -= qty;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void ExecutedAtPriceSize(uint64_t seqno, uint64_t id, int32_t qty,
                           int32_t leaves_qty, int64_t price) {
    auto oit = orders_.find(id);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    int32_t delta = order.qty - leaves_qty;
    if (order.bookid != NOBOOK) {
      OrderBook &book = books_[order.bookid];
      bool top;
      if (delta > 0) {
        top = book.Reduce(seqno, order.buy_sell, order.price, delta);
      } else {
        top = book.Add(seqno, order.buy_sell, order.price, -delta);
      }
      handler_.OnTrade(&book, qty, price, top);
    }

    order.qty = leaves_qty;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void Reduce(uint64_t seqno, uint64_t ref, int32_t qty) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    if (order.bookid != NOBOOK) {
      OrderBook &book = books_[order.bookid];
      bool top = book.Reduce(seqno, order.buy_sell, order.price, qty);
      handler_.OnQuote(&book, top);
    }

    order.qty -= qty;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void Delete(uint64_t seqno, uint64_t ref) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    if (order.bookid != NOBOOK) {
      OrderBook &book = books_[order.bookid];
      bool top = book.Reduce(seqno, order.buy_sell, order.price, order.qty);
      handler_.OnQuote(&book, top);
    }

    orders_.erase(oit);
  }

  void Replace(uint64_t seqno, uint64_t ref, uint64_t ref2, int32_t qty,
               int64_t price) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    if (order.bookid != NOBOOK) {
      OrderBook &book = books_[order.bookid];
      bool top = book.Reduce(seqno, order.buy_sell, order.price, order.qty);
      bool top2 = book.Add(seqno, order.buy_sell, price, qty);
      handler_.OnQuote(&book, top || top2);
    }

    orders_.erase(oit);
    orders_.emplace(ref2, Order(price, qty, order.buy_sell, order.bookid));
  }

  void Modify(uint64_t seqno, uint64_t id, int32_t qty, int64_t price) {
    auto oit = orders_.find(id);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    if (order.bookid != NOBOOK) {
      OrderBook &book = books_[order.bookid];
      bool top = book.Reduce(seqno, order.buy_sell, order.price, order.qty);
      bool top2 = book.Add(seqno, order.buy_sell, price, qty);
      handler_.OnQuote(&book, top || top2);
    }

    order.qty = qty;
    order.price = price;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void Trade(uint64_t seqno, int64_t shares, uint64_t symbol, int64_t price) {
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) {
      return;
    }

    OrderBook *book = &books_[it->second];
    handler_.OnTrade(book, shares, price, false);
  }

  size_t Size() const { return orders_.size(); }

private:
  // Non-copyable
  Feed(const Feed &) = delete;
  Feed &operator=(const Feed &) = delete;

  struct Hash {
    size_t operator()(uint64_t h) const noexcept {
      h ^= h >> 33;
      h *= 0xff51afd7ed558ccd;
      h ^= h >> 33;
      h *= 0xc4ceb9fe1a85ec53;
      h ^= h >> 33;
      return h;
    }
  };

  Handler &handler_;
  size_t size_hint_ = 0;
  bool all_orders_ = false;
  bool all_books_ = false;

  std::vector<OrderBook> books_;
  // std::unordered_map<uint64_t, uint16_t, Hash> symbols_;
  HashMap<uint64_t, uint16_t, Hash> symbols_;
  // std::unordered_map<uint64_t, Order, Hash> orders_;
  HashMap<uint64_t, Order, Hash> orders_;
};
