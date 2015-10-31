/*
Copyright (c) 2015 Erik Rigtorp <erik@rigtorp.se>

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

#include <cstdint>
#include <cstring>

template <typename Handler> class PitchParser {

public:
  PitchParser(Handler &handler) : handler_(handler) {}

  void ParseMessage(uint64_t seqno, const char *buf) {
    switch (buf[1]) {
    case 0x21:
      return AddLong(seqno, buf);
    case 0x22:
      return AddShort(seqno, buf);
    case 0x2F:
      return AddExpanded(seqno, buf);
    case 0x23:
      return Executed(seqno, buf);
    case 0x24:
      return ExecutedAtPriceSize(seqno, buf);
    case 0x25:
      return ReduceLong(seqno, buf);
    case 0x26:
      return ReduceShort(seqno, buf);
    case 0x27:
      return ModifyLong(seqno, buf);
    case 0x28:
      return ModifyShort(seqno, buf);
    case 0x29:
      return Delete(seqno, buf);
    case 0x2A:
      return TradeLong(seqno, buf);
    case 0x2B:
      return TradeShort(seqno, buf);
    case 0x30:
      return TradeExpanded(seqno, buf);
    }
  }

  size_t ParseStream(const char *buf, size_t len) {
    size_t i = 0;
    while (i < len) {
      int msg_len = read8(buf + i);
      if (i + msg_len > len) {
        break;
      }
      ParseMessage(0, buf + i);
      i += msg_len;
    }
    return i;
  }

  void ParsePacket(const char *buf, size_t len) {
    // uint16_t hdr_len = read16(buf);
    int count = read8(buf + 2);
    uint32_t seqno = read32(buf + 4);
    buf += 8;
    for (int i = 0; i < count; ++i) {
      uint8_t msg_len = read8(buf);
      ParseMessage(seqno + i, buf);
      buf += msg_len;
    }
  }

  using Id = uint64_t;
  using Qty = int32_t;
  using Price = int64_t;
  using Symbol = uint64_t;

private:
  uint8_t read8(const void *buf) { return *static_cast<const uint8_t *>(buf); }

  uint32_t read16(const void *buf) {
    return *static_cast<const uint16_t *>(buf);
  }

  uint32_t read32(const void *buf) {
    return *static_cast<const uint32_t *>(buf);
  }

  uint64_t read64(const void *buf) {
    return *static_cast<const uint64_t *>(buf);
  }

  Symbol readsym6(const void *buf) {
    uint64_t v = 0;
    std::memset(&v, ' ', 8);
    std::memcpy(&v, buf, 6);
    return __builtin_bswap64(v);
  }

  uint64_t readsym8(const void *buf) {
    return __builtin_bswap64(*static_cast<const uint64_t *>(buf));
  }

  void AddLong(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    bool bs = buf[14] == 'B' ? true : false;
    Qty qty = read32(buf + 15);
    Symbol symbol = readsym6(buf + 19);
    Price price = read64(buf + 25);
    handler_.Add(seqno, id, bs, qty, symbol, price);
  }

  void AddShort(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    bool bs = buf[14] == 'B' ? true : false;
    Qty qty = read16(buf + 15);
    Symbol symbol = readsym6(buf + 17);
    Price price = read16(buf + 23) * 100;
    handler_.Add(seqno, id, bs, qty, symbol, price);
  }

  void AddExpanded(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    bool bs = buf[14] == 'B' ? true : false;
    Qty qty = read32(buf + 15);
    Symbol symbol = readsym8(buf + 19);
    Price price = read64(buf + 27);
    handler_.Add(seqno, id, bs, qty, symbol, price);
  }

  void Executed(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    Qty qty = read32(buf + 14);
    handler_.Executed(seqno, id, qty);
  }

  void ExecutedAtPriceSize(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    Qty qty = read32(buf + 14);
    Qty leaves_qty = read32(buf + 18);
    Price price = read64(buf + 30);
    handler_.ExecutedAtPriceSize(seqno, id, qty, leaves_qty, price);
  }

  void ReduceLong(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    Qty qty = read32(buf + 14);
    handler_.Reduce(seqno, id, qty);
  }

  void ReduceShort(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    Qty qty = read16(buf + 14);
    handler_.Reduce(seqno, id, qty);
  }

  void ModifyLong(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    Qty qty = read32(buf + 14);
    Price price = read64(buf + 18);
    handler_.Modify(seqno, id, qty, price);
  }

  void ModifyShort(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    Qty qty = read16(buf + 14);
    Price price = read16(buf + 16) * 100;
    handler_.Modify(seqno, id, qty, price);
  }

  void Delete(uint64_t seqno, const char *buf) {
    Id id = read64(buf + 6);
    handler_.Delete(seqno, id);
  }

  void TradeLong(uint64_t seqno, const char *buf) {
    Qty qty = read32(buf + 15);
    Symbol symbol = readsym6(buf + 19);
    Price price = read64(buf + 25);
    handler_.Trade(seqno, qty, symbol, price);
  }

  void TradeShort(uint64_t seqno, const char *buf) {
    Qty qty = read16(buf + 15);
    Symbol symbol = readsym6(buf + 17);
    Price price = read16(buf + 23) * 100;
    handler_.Trade(seqno, qty, symbol, price);
  }

  void TradeExpanded(uint64_t seqno, const char *buf) {
    Qty qty = read32(buf + 15);
    Symbol symbol = readsym8(buf + 19);
    Price price = read64(buf + 27);
    handler_.Trade(seqno, qty, symbol, price);
  }

  Handler &handler_;
};
