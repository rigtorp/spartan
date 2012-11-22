#pragma once

#include <vector>
#include <iostream>
#include <stdexcept>
#include "util.hpp"

template <typename Handler>
class Itch41Parser
{

public:
  
  Itch41Parser(Handler &handler)
    : handler_(handler) 
  {}

  void Parse(uint64_t seqno, const char *s)
  {
    switch (s[0]) {
    case 'T': return;
    case 'S': return;
    case 'R': return;
    case 'H': return;
    case 'Y': return;
    case 'L': return;
    case 'A': return Add(seqno, s);
    case 'F': return Add(seqno, s);
    case 'E': return Executed(seqno, s);
    case 'C': return Executed(seqno, s);
    case 'X': return Cancel(seqno, s);
    case 'D': return Delete(seqno, s);
    case 'U': return Replace(seqno, s);
    case 'P': return;
    case 'Q': return;
    case 'B': return;
    case 'I': return;
    default: throw std::runtime_error("unknown message type");
    }
  }

  size_t ParseMany(const char *buf, size_t n)
  {
    timeval start, stop;
    gettimeofday(&start, NULL);

    int count = 0;
    size_t i = 0;
    while (i < n) {
      int len = readfixnum(&buf[i], 2);
      if (i + len + 2 > n) {
        break;
      }
      Parse(0, &buf[i+2]);
      count++;
      i += len + 2;
    }

    gettimeofday(&stop, NULL);
    std::cout << ((stop.tv_sec - start.tv_sec) * 1e6 + (stop.tv_usec - start.tv_usec))/ (double) count << std::endl;

    return i;
  }

  bool Read(std::istream &stream)
  {
    char buf[4096];
    if (!stream.read(&buf[0], 2).good()) return false;
    int len = readfixnum(&buf[0], 2); 
    if (!stream.read(&buf[2], len).good()) return false;
    Parse(0, &buf[2]);
    return true;
  }

  void ReadMany(std::istream &stream)
  {
    std::vector<char>  buf;
    buf.resize(1<<24);
    size_t n = 0;
    do {
      stream.read(&buf[n], buf.size() - n);
      n += stream.gcount();
      size_t i = ParseMany(&buf[0], n);
      std::memmove(&buf[0], &buf[i], n - i);
      n -= i;
    } while (stream.good());
  }

private:

  void Add(uint64_t seqno, const char *s) 
  {
    uint64_t ts = readfixnum(&s[1], 4);
    uint64_t ref = readfixnum(&s[5], 8);
    bool bs = s[13] == 'B' ? true : false;
    uint32_t shares = readfixnum(&s[14], 4);
    uint64_t stock = readfixnum(&s[18], 8);
    uint32_t price = readfixnum(&s[26], 4);
    //std::cout << "A " << ref << " " << bs << " " << shares << " " << stock << " " << price << std::endl;
    handler_.Add(seqno, ts, ref, bs, shares, stock, price);
  }

  void Executed(uint64_t seqno, const char *s) 
  {
    uint64_t ts = readfixnum(&s[1], 4);
    uint64_t ref = readfixnum(&s[5], 8);
    uint32_t shares = readfixnum(&s[13], 4);
    //std::cout << "E " << ref << " " << shares << std::endl;
    handler_.Executed(seqno, ts, ref, shares);
  }

  void Cancel(uint64_t seqno, const char *s) 
  {
    uint64_t ts = readfixnum(&s[1], 4);
    uint64_t ref = readfixnum(&s[5], 8);
    uint32_t shares = readfixnum(&s[13], 4);
    //std::cout << "X " << ref << " " << shares << std::endl;
    handler_.Cancel(seqno, ts, ref, shares);
  }

  void Delete(uint64_t seqno, const char *s) 
  {
    uint64_t ts = readfixnum(&s[1], 4);
    uint64_t ref = readfixnum(&s[5], 8);
    //std::cout << "D " << ref << std::endl;
    handler_.Delete(seqno, ts, ref);
  }

  void Replace(uint64_t seqno, const char *s) 
  {
    uint64_t ts = readfixnum(&s[1], 4);
    uint64_t ref = readfixnum(&s[5], 8);
    uint64_t ref2 = readfixnum(&s[13], 8);
    uint32_t shares = readfixnum(&s[21], 4);
    uint32_t price = readfixnum(&s[25], 4);
    //std::cout << "U " << ref << " " << ref2 << " " << shares << " " << price << std::endl;
    handler_.Replace(seqno, ts, ref, ref2, shares, price);
  }

  Handler &handler_;
};

