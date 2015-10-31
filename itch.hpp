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

#include <cstdint>

template <typename Handler> class Itch50Parser {

public:
  Itch50Parser(Handler &handler) : handler_(handler) {}

  void ParseMessage(uint64_t seqno, const char *buf) {
    switch (buf[0]) {
    case 'A':
      return Add(seqno, buf);
    case 'F':
      return Add(seqno, buf);
    case 'E':
      return Executed(seqno, buf);
    case 'C':
      return ExecutedAtPrice(seqno, buf);
    case 'X':
      return Cancel(seqno, buf);
    case 'D':
      return Delete(seqno, buf);
    case 'U':
      return Replace(seqno, buf);
    }
  }

  size_t ParseMany(const char *buf, size_t len) {
    size_t i = 0;
    while (i < len) {
      int msg_len = read16(&buf[i]);
      if (i + msg_len + 2 > len) {
        break;
      }
      ParseMessage(0, &buf[i + 2]);
      i += msg_len + 2;
    }
    return i;
  }

  using Id = uint64_t;
  using Qty = int32_t;
  using Price = int32_t;
  using Symbol = uint64_t;

private:
  uint32_t read16(const void *buf) {
    return __builtin_bswap16(*static_cast<const uint16_t *>(buf));
  }

  uint32_t read32(const void *buf) {
    return __builtin_bswap32(*static_cast<const uint32_t *>(buf));
  }

  uint64_t read64(const void *buf) {
    return __builtin_bswap64(*static_cast<const uint64_t *>(buf));
  }

  uint64_t readsym8(const void *buf) {
    return __builtin_bswap64(*static_cast<const uint64_t *>(buf));
  }

  void Add(uint64_t seqno, const char *buf) {
    Id ref = read64(buf + 11);
    bool bs = buf[19] == 'B' ? true : false;
    Qty shares = read32(buf + 20);
    Symbol stock = readsym8(buf + 24);
    Price price = read32(buf + 32);
    handler_.Add(seqno, ref, bs, shares, stock, price);
  }

  void Executed(uint64_t seqno, const char *buf) {
    Id ref = read64(buf + 11);
    Qty shares = read32(buf + 19);
    handler_.Executed(seqno, ref, shares);
  }

  void ExecutedAtPrice(uint64_t seqno, const char *buf) {
    Id ref = read64(buf + 11);
    Qty shares = read32(buf + 19);
    Price price = read32(buf + 32);
    handler_.ExecutedAtPrice(seqno, ref, shares, price);
  }

  void Cancel(uint64_t seqno, const char *buf) {
    Id ref = read64(buf + 11);
    Qty shares = read32(buf + 19);
    handler_.Reduce(seqno, ref, shares);
  }

  void Delete(uint64_t seqno, const char *buf) {
    Id ref = read64(buf + 11);
    handler_.Delete(seqno, ref);
  }

  void Replace(uint64_t seqno, const char *buf) {
    Id ref = read64(buf + 11);
    Id ref2 = read64(buf + 19);
    Qty shares = read32(buf + 27);
    Price price = read32(buf + 31);
    handler_.Replace(seqno, ref, ref2, shares, price);
  }

  Handler &handler_;
};
