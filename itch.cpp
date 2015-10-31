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

#include "feed.hpp"
#include "itch.hpp"
#include <algorithm>
#include <boost/iostreams/device/mapped_file.hpp>
#include <iostream>
#include <sys/time.h>
#include <vector>

using namespace std;

class Handler {
public:
  Handler() : count(0) {}

  void OnQuote(OrderBook *book, bool top) {
    count++;
    // if (top) {
    //  cout << book->GetInstrument() << " " << book->GetBestPrice() << endl;
    //}
  }

  void OnTrade(OrderBook *book, int64_t shares, int64_t price, bool top) {
    OnQuote(book, top);
  }

  int count;
};

static inline uint64_t rdtscp() {
  uint64_t lo, hi;
  uint32_t aux;
  asm volatile("rdtscp\n" : "=a"(lo), "=d"(hi), "=c"(aux) : :);
  return (hi << 32) + lo;
}

int main(int argc, char *argv[]) {
  Handler handler;
  Feed<Handler> feed(handler, 16000000, true, true);
  Itch50Parser<Feed<Handler>> parser(feed);

  // feed.Subscribe("SPY");

  boost::iostreams::mapped_file_source file(argv[1]);

  uint64_t overhead = std::numeric_limits<uint64_t>::max();
  for (int i = 0; i < 10; ++i) {
    auto start = rdtscp();
    auto stop = rdtscp();
    auto diff = stop - start;
    overhead = std::min(diff, overhead);
  }

  auto start = rdtscp();
  sleep(1);
  auto stop = rdtscp();
  auto speed = (stop - start - overhead) / 1000000000.0;

  std::cout << "rdtscp() overhead " << overhead << " ticks, speed " << speed
            << " tick/ns" << std::endl;

  size_t i = 0;
  size_t count = 0;
  char buf[2048];
  while (i < file.size() && count < 1000000) {
    int len =
        __builtin_bswap16(*reinterpret_cast<const uint16_t *>(file.data() + i));
    memcpy(buf, file.data() + i + 2, len);
    parser.ParseMessage(0, buf);
    count++;
    i += len + 2;
  }

  std::vector<int> hist(20);
  std::vector<double> samples;
  count = 0;
  size_t size = 0;

  while (i < file.size()) {
    int len =
        __builtin_bswap16(*reinterpret_cast<const uint16_t *>(file.data() + i));
    memcpy(buf, file.data() + i + 2, len);
    auto start = rdtscp();
    parser.ParseMessage(0, buf);
    auto stop = rdtscp();
    auto diff = (stop - start - overhead) / speed;
    if (diff < 10000000000) {
      // not sure how to prevent these huge outliers
      samples.push_back(diff);
    } else {
      // std::cerr << "huge diff" << std::endl;
    }

    hist[std::min((size_t)std::trunc(diff / 100), hist.size() - 1)]++;
    count++;
    size = std::max(size, feed.Size());
    i += len + 2;
    if ((count % 1000000) == 0) {
      std::cout << "*";
      std::cout.flush();
    }
    // if (count > 200000000) break;
  }
  std::cout << count << std::endl;

  std::sort(samples.begin(), samples.end());
  auto mean =
      std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  auto max = samples.empty() ? 0 : samples.back();
  auto min = samples.empty() ? 0 : samples.front();
  auto p99 = samples.empty() ? 0 : samples[0.99 * samples.size()];

  std::cout << "Mean:   " << mean << std::endl;
  std::cout << "99%:    " << p99 << std::endl;
  std::cout << "Max:    " << max << std::endl;
  std::cout << "Min:    " << min << std::endl;
  for (size_t i = 0; i < hist.size(); ++i) {
    std::string s(std::trunc(100 * (double)hist[i] / count), '*');
    std::cout << i << " " << s << std::endl;
  }
  std::cout << "Max orders: " << size << std::endl;

  return 0;
}
