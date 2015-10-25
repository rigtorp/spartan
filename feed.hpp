#pragma once

#include <iostream>
#include <map>
#include <unordered_map>
#include <functional>
#include <boost/container/flat_map.hpp>

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

struct Level {
  int64_t price;
  int64_t qty;
  uint64_t seqno;

  friend std::ostream &operator<<(std::ostream &out, const Level &level) {
    out << "Level(" << level.price << ", " << level.qty << ", " << level.seqno
        << ")";
    return out;
  }
};

class OrderBook {
public:
  OrderBook(uint64_t symbol, std::string instrument, void *data = NULL)
      : symbol_(symbol), instrument_(instrument), data_(NULL) {}

  const std::string &GetInstrument() const { return instrument_; }

  uint64_t GetSymbol() const { return symbol_; }

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
    if (qty == 0) {
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
    return -buy_.begin()->first >= sell_.begin()->first;
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
    for (auto it = book.buy_.begin(); it != book.buy_.end(); it++) {
      out << it->second << std::endl;
    }
    out << "Sell:" << std::endl;
    for (auto it = book.sell_.begin(); it != book.sell_.end(); it++) {
      out << it->second << std::endl;
    }
    return out;
  }

private:
  // std::map<int64_t, Level> buy_, sell_;
  // boost::container::flat_map<int64_t, Level> buy_, sell_;
  boost::container::flat_map<int64_t, Level, std::greater<int64_t>> buy_, sell_;
  uint64_t symbol_;
  std::string instrument_;
  void *data_;
};

struct Order {
  uint64_t seqno;
  bool buy_sell;
  int64_t qty;
  uint64_t symbol;
  int64_t price;
  OrderBook *book;

  Order(uint64_t seqno, bool buy_sell, int64_t qty, uint64_t symbol,
        int64_t price, OrderBook *book)
      : seqno(seqno), buy_sell(buy_sell), qty(qty), symbol(symbol),
        price(price), book(book) {}
};

template <typename Handler> class Feed {

public:
  Feed(Handler &handler, bool all_orders = true, bool all_books = false)
      : handler_(handler), all_orders_(all_orders), all_books_(all_books) {}

  OrderBook &Subscribe(std::string instrument, void *data = NULL) {
    if (instrument.size() < 8) {
      instrument.insert(instrument.size(), 8 - instrument.size(), ' ');
    }
    uint64_t symbol = __builtin_bswap64(
        *reinterpret_cast<const uint64_t *>(instrument.data()));

    auto res =
        books_.insert(std::make_pair(symbol, OrderBook(symbol, instrument)));
    OrderBook &book = res.first->second;
    if (res.second == false) {
      return book;
    }
    book.SetUserData(data);
    for (auto it = orders_.begin(); it != orders_.end(); it++) {
      Order &order = it->second;
      if (order.symbol == symbol) {
        book.Add(order.seqno, order.buy_sell, order.price, order.qty);
        order.book = &book;
      }
    }
    return book;
  }

  void Add(uint64_t seqno, uint64_t ref, bool buy_sell, int64_t qty,
           uint64_t symbol, int64_t price) {
    typename decltype(books_)::iterator bit;
    if (all_books_) {
      bit = books_.emplace(symbol, OrderBook(symbol, "")).first;
    } else {
      bit = books_.find(symbol);
    }
    if (bit == books_.end()) {
      if (all_orders_) {
        orders_.emplace(ref, Order(seqno, buy_sell, qty, symbol, price, NULL));
      }
      return;
    }

    OrderBook *book = &bit->second;
    if (orders_.emplace(ref, Order(seqno, buy_sell, qty, symbol, price, book))
            .second) {
      bool top = book->Add(seqno, buy_sell, price, qty);
      handler_.OnQuote(book, top);
    }
  }

  void Executed(uint64_t seqno, uint64_t ref, int64_t qty) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }
    Order &order = oit->second;
    if (order.book != NULL) {
      bool top = order.book->Reduce(seqno, order.buy_sell, order.price, qty);
      handler_.OnTrade(order.book, qty, order.price, top);
    }
    order.qty -= qty;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void ExecutedAtPrice(uint64_t seqno, uint64_t ref, int64_t qty,
                       int64_t price) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    if (order.book != NULL) {
      bool top = order.book->Reduce(seqno, order.buy_sell, order.price, qty);
      handler_.OnTrade(order.book, qty, price, top);
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
    if (order.book != NULL) {
      bool top;
      if (delta > 0) {
        top = order.book->Reduce(seqno, order.buy_sell, order.price, delta);
      } else {
        top = order.book->Add(seqno, order.buy_sell, order.price, -delta);
      }
      handler_.OnTrade(order.book, qty, price, top);
    }

    order.qty = leaves_qty;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void Reduce(uint64_t seqno, uint64_t ref, int64_t qty) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }
    Order &order = oit->second;
    if (order.book != NULL) {
      bool top = order.book->Reduce(seqno, order.buy_sell, order.price, qty);
      handler_.OnQuote(order.book, top);
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
    if (order.book != NULL) {
      bool top =
          order.book->Reduce(seqno, order.buy_sell, order.price, order.qty);
      handler_.OnQuote(order.book, top);
    }
    orders_.erase(oit);
  }

  void Replace(uint64_t seqno, uint64_t ref, uint64_t ref2, int64_t qty,
               int64_t price) {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }
    Order &order = oit->second;
    orders_.emplace(ref2, Order(seqno, order.buy_sell, qty, order.symbol, price,
                                order.book));
    if (order.book != NULL) {
      bool top =
          order.book->Reduce(seqno, order.buy_sell, order.price, order.qty);
      bool top2 = order.book->Add(seqno, order.buy_sell, price, qty);
      handler_.OnQuote(order.book, top || top2);
    }
    orders_.erase(ref);
  }

  void Modify(uint64_t seqno, uint64_t id, int32_t qty, int64_t price) {
    auto oit = orders_.find(id);
    if (oit == orders_.end()) {
      return;
    }
    Order &order = oit->second;
    if (order.book != NULL) {
      bool top =
          order.book->Reduce(seqno, order.buy_sell, order.price, order.qty);
      bool top2 = order.book->Add(seqno, order.buy_sell, price, qty);
      handler_.OnQuote(order.book, top || top2);
    }
    order.qty = qty;
    order.price = price;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void Trade(uint64_t seqno, int64_t shares, uint64_t symbol, int64_t price) {
    auto bit = books_.find(symbol);
    if (bit == books_.end()) {
      return;
    }
    OrderBook *book = &bit->second;
    handler_.OnTrade(book, shares, price, false);
  }

private:
  // Non-copyable
  Feed(const Feed &) = delete;
  Feed &operator=(const Feed &) = delete;

  Handler &handler_;
  std::unordered_map<uint64_t, OrderBook> books_;
  std::unordered_map<uint64_t, Order> orders_;
  bool all_orders_;
  bool all_books_;
};
