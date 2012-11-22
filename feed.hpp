#pragma once 

#include <iostream>
#include <map>
#include <unordered_map>
#include <functional>
#include <boost/container/flat_map.hpp>

struct BestPrice
{
  int64_t bidqty;
  int64_t bid;
  int64_t ask;
  int64_t askqty;

  BestPrice(int64_t bidqty, int64_t bid, int64_t ask, int64_t askqty)
    : bidqty(bidqty), bid(bid), ask(ask), askqty(askqty) {}

  BestPrice()
    : bidqty(0), bid(0), ask(0), askqty(0) {}

  friend std::ostream& operator<< (std::ostream &out, const BestPrice &best)
  {
    out << "BestPrice(" << best.bidqty << ", " << best.bid << ", " << best.ask << ", " << best.askqty << ")";
    return out;
  }
};

struct Level
{
  int64_t price;
  int64_t qty;
  uint64_t seqno;

  friend std::ostream& operator<< (std::ostream &out, const Level &level)
  {
    out << "Level(" << level.price << ", " << level.qty << ", " << level.seqno << ")";
    return out;
  }
};

class OrderBook
{
public:

  OrderBook(uint64_t symbol, std::string instrument, void *data = NULL) 
    : symbol_(symbol),
      instrument_(instrument),
      data_(NULL)
  {
  }

  const std::string& GetInstrument() const
  {
    return instrument_;
  }

  uint64_t GetSymbol() const
  {
    return symbol_;
  }

  BestPrice GetBestPrice() const
  {
    auto buy = buy_.begin();
    auto sell = sell_.begin();
    if (buy == buy_.end() || sell == sell_.end())
      return BestPrice(0,0,0,0);
    return BestPrice(buy->second.qty, buy->second.price, sell->second.price, sell->second.qty);
  }

  void* GetUserData() const
  {
    return data_;
  }

  void SetUserData(void *data)
  {
    data_ = data;
  }

  bool Add(uint64_t seqno, bool buy_sell, int64_t price, int64_t qty)
  {
    auto &side = buy_sell ? buy_ : sell_;
    int64_t prio = buy_sell ? -price : price;
    auto res = side.insert(std::make_pair(prio, Level()));
    res.first->second.price = price;
    res.first->second.qty += qty;
    res.first->second.seqno = seqno;
    return res.first == side.begin();
  }

  bool Reduce(uint64_t seqno, bool buy_sell, int64_t price, int64_t qty)
  {
    auto &side = buy_sell ? buy_ : sell_;
    int64_t prio = buy_sell ? -price : price;
    auto it = side.find(prio);
    if (it == side.end())
      return false;
    it->second.qty -= qty;
    it->second.seqno = seqno;
    if (it->second.qty <= 0) 
      side.erase(it);
    return it == side.begin();
  }

  bool IsCrossed()
  {
    if (buy_.empty() || sell_.empty())
      return false;
    return -buy_.begin()->first >= sell_.begin()->first;
  }

  void UnCross()
  {
    // Invalidates iterators
    auto bit = buy_.begin();
    auto sit = sell_.begin();
    while (bit != buy_.end() && sit != sell_.end() && bit->second.price >= sit->second.price) {
      if (bit->second.seqno > sit->second.seqno) {
        sell_.erase(sit++);
      } else {
        buy_.erase(bit++);
      }
    }
  }

  friend std::ostream& operator<< (std::ostream &out, const OrderBook &book)
  {
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
  //std::map<int64_t, Level> buy_, sell_;
  //boost::container::flat_map<int64_t, Level> buy_, sell_;
  boost::container::flat_map<int64_t, Level, std::greater<int64_t>> buy_, sell_;
  uint64_t symbol_;
  std::string instrument_;
  void *data_;
};

struct Order
{    
  uint64_t seqno;
  bool buy_sell;
  int64_t qty;
  uint64_t symbol;
  int64_t price;
  OrderBook *book;

  Order(uint64_t seqno, bool buy_sell, int64_t qty, uint64_t symbol, int64_t price, OrderBook *book)
    : seqno(seqno), buy_sell(buy_sell), qty(qty), symbol(symbol), price(price), book(book) {}
};

template <typename Handler>
class Feed
{

public:

  Feed(Handler &handler, bool all_orders = true, bool all_books = false)
    : handler_(handler),
      all_orders_(all_orders),
      all_books_(all_books)
  {
  }

  OrderBook& Subscribe(std::string instrument, void *data = NULL)
  {
    if (instrument.size() < 8)
      instrument.insert(instrument.size(), 8 - instrument.size(), ' ');
    uint64_t symbol = readfixnum(instrument.data(), 8);

    // cout << __FUNCTION__ << " " << instrument << " " << symbol << std::endl;

    auto res = books_.insert(std::make_pair(symbol, OrderBook(symbol, instrument)));
    OrderBook &book = res.first->second;
    if (res.second == false)
      return book;
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

  void Timestamp(uint64_t seqno, uint64_t ts)
  {
  }

  void Add(uint64_t seqno, uint64_t ts, uint64_t ref, bool buy_sell, int64_t qty, 
           uint64_t symbol, int64_t price)
  {
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
    orders_.emplace(ref, Order(seqno, buy_sell, qty, symbol, price, book));
    bool top = book->Add(seqno, buy_sell, price, qty);    
    handler_.OnQuote(book, top);
  }
  
  void Executed(uint64_t seqno, uint64_t ts, uint64_t ref, int64_t qty)
  {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;

    if (order.book != NULL) {
      bool top = order.book->Reduce(seqno, order.buy_sell, order.price, qty);
      handler_.OnTrade(order.book, order.buy_sell, qty, order.price, top);
    }

    order.qty -= qty;
    if (order.qty <= 0) {
      orders_.erase(oit);
    }
  }

  void Cancel(uint64_t seqno, uint64_t ts, uint64_t ref, int64_t qty)
  {
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

  void Delete(uint64_t seqno, uint64_t ts, uint64_t ref)
  {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    if (order.book != NULL) {
      bool top = order.book->Reduce(seqno, order.buy_sell, order.price, order.qty);
      handler_.OnQuote(order.book, top);
    }

    orders_.erase(oit);
  }

  void Replace(uint64_t seqno, uint64_t ts, uint64_t ref, uint64_t ref2, 
               int64_t qty, int64_t price)
  {
    auto oit = orders_.find(ref);
    if (oit == orders_.end()) {
      return;
    }

    Order &order = oit->second;
    orders_.emplace(ref2, Order(seqno, order.buy_sell, qty, order.symbol, price, order.book));

    if (order.book != NULL) {
      bool top = order.book->Reduce(seqno, order.buy_sell, order.price, order.qty);
      bool top2 = order.book->Add(seqno, order.buy_sell, price, qty);
      handler_.OnQuote(order.book, top || top2);
    }

    orders_.erase(oit);
  }

  void Trade(uint64_t seqno, uint64_t ts, bool bs, int64_t shares, uint64_t symbol, 
             int64_t price, uint64_t matchno)
  {
    auto bit = books_.find(symbol);
    if (bit == books_.end()) {
      return;
    }
    OrderBook *book = &bit->second;
    handler_.OnTrade(book, bs, shares, price, false);
  }

private:
  
  // Non-copyable
  Feed(const Feed &) = delete;
  Feed& operator=(const Feed &) = delete;

  Handler &handler_;
  std::unordered_map<uint64_t, OrderBook> books_;
  std::unordered_map<uint64_t, Order> orders_;
  bool all_orders_;
  bool all_books_;
};

